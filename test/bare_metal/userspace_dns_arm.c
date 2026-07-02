/*
 * USERSPACE DNS testi (aarch64) — EL0 süreç SYSCALL ile TAM DNS çözümleme.
 *
 * D-176 (userspace_net_arm.c) bir EL0 sürecin net_gonder(24)/net_al(25)
 * syscall'larıyla ham ethernet çerçevesi gönderip aldığını kanıtladı — ama
 * yalnızca ARP (tek L2 istek/yanıt). Bu test bir adım öteye gider: EL0 (yetkisiz)
 * süreç, virtio-net'e DOĞRUDAN erişmeden, YALNIZ syscall ile TAM bir L2-L7
 * protokol yığını çalıştırır:
 *
 *   L2 (Ethernet) + L3 (IPv4 + checksum) + L4 (UDP) + L7 (DNS "example.com" A)
 *
 * dns_resolver_arm.c (D-147+) bu protokol mantığını KERNEL'de (EL1) yapmıştı.
 * Burada aynı mantık EL0'a taşınır — ağ kernel-aracılıdır ama protokol inşası +
 * yanıt parse'ı tamamen userspace'te olur. Kanıt: bir userspace program,
 * çekirdek-aracılı ham-frame syscall'larıyla gerçek DNS çözümlemesi yaptı.
 *
 * Akış (EL0 launcher):
 *   1) ARP ile gateway/DNS MAC çöz — ARP isteği inşa+sys2(24), sys2(25) poll ile
 *      reply'den MAC al. Çözülemezse SLIRP sabit gateway MAC'i (52:55:0a:00:02:02)
 *      kullanılır (fallback — DNS forward'ı gateway üzerinden de çalışır).
 *   2) DNS sorgusu inşa et: Eth[dst=MAC] + IPv4[dst=10.0.2.3, UDP] + UDP[dst=53] +
 *      DNS["example.com" A]. ip_checksum ile IP başlığı doğrula. sys2(24) ile yolla.
 *   3) sys2(25) poll ile DNS yanıtını al → header doğrula (ID eşleşir, QR=1,
 *      ANCOUNT>=1) → ANSWER parse (isim-sıkıştırma 0xC0) → IPv4 A-kaydı çıkar.
 *   4) Geçerli (non-zero 4-byte) A-kaydı çıkarsa: IP'yi bas + "USERDNS OK".
 *
 * FALLBACK (internet yoksa / SLIRP dış-DNS yanıt vermezse): DNS yanıtı gelmese
 * bile EL0'ın DNS sorgusunu sys2(24) ile GÖNDERDİĞİ pcap'te kanıtlanır (UDP
 * dst-port 53) → "USERDNS SENT OK". Makefile hangi yolu kanıtladığını raporlar.
 *
 * Güvenlik: launcher tüm frame/rx tamponlarını EL0 user yığınında (0x42000000
 * VA sayfası) tutar → sys2(24)/sys2(25) user-VA guard'ından (D-150) geçer.
 */
#include <stdint.h>
extern void kdl_yazdir_metin(const char *);
extern void kdl_yazdir_satir(void);
extern uint64_t kdl_virtio_net_bul(void);
extern int  kdl_virtio_net_kur(uint64_t base);
extern void kdl_surec_kur_el0_veri(uint64_t *l1, uint64_t *l2, uint64_t kod_pa, uint64_t veri_pa);
extern void kdl_kesme_kur(void);
extern void kdl_timer_baslat(void);
extern void kdl_preempt_baslat(void);
extern int  kdl_preempt_gorev_olustur_el0(void (*giris)(void), void *kernel_yigin_tepe,
                                          void *user_yigin_tepe);
extern void kdl_preempt_gorev_ttbr(int gorev, uint64_t *l1);
extern void kdl_preempt_ac(void);

static uint64_t l1_l[512] __attribute__((aligned(4096)));
static uint64_t l2_l[512] __attribute__((aligned(4096)));
static unsigned char kstack_l[8192] __attribute__((aligned(16)));

__attribute__((always_inline)) static inline unsigned long sys2(unsigned long num, unsigned long a0, unsigned long a1) {
    register unsigned long x8 __asm__("x8") = num;
    register unsigned long x0 __asm__("x0") = a0;
    register unsigned long x1 __asm__("x1") = a1;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8), "r"(x1) : "memory");
    return x0;
}
__attribute__((always_inline)) static inline unsigned long sys(unsigned long num, unsigned long arg) {
    register unsigned long x8 __asm__("x8") = num;
    register unsigned long x0 __asm__("x0") = arg;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8) : "memory");
    return x0;
}

/* IPv4 başlık sağlaması (RFC 1071). always_inline → .user launcher'a gömülür. */
__attribute__((always_inline)) static inline uint16_t ip_checksum(const unsigned char *veri, int uzun) {
    uint32_t t = 0;
    int i;
    for (i = 0; i + 1 < uzun; i += 2) t += ((uint32_t)veri[i] << 8) | veri[i + 1];
    if (uzun & 1) t += (uint32_t)veri[uzun - 1] << 8;
    while (t >> 16) t = (t & 0xffff) + (t >> 16);
    return (uint16_t)(~t);
}

/*
 * DNS isim alanını (QNAME veya answer NAME) atla. off = payload içinde isme
 * giriş offseti. Döner: isimden sonraki offset (-1 = bozuk paket).
 *   - Compression pointer (üst iki bit 0xC0): 2 byte, isim burada biter.
 *   - Düz label dizisi: uzunluk-öneki byte'lar, 0 byte'a kadar.
 */
__attribute__((always_inline)) static inline int isim_atla(const unsigned char *dns, int dns_uzun, int off) {
    while (off < dns_uzun) {
        unsigned char len = dns[off];
        if ((len & 0xC0) == 0xC0) return (off + 2 <= dns_uzun) ? off + 2 : -1;
        if (len == 0) return off + 1;
        off += 1 + (int)len;
    }
    return -1;
}

/* Bir nibble'ı hex karaktere çevir (0-9 → '0'..'9', 10-15 → 'a'..'f').
 * ARİTMETİK — .rodata lookup YOK: EL0'da kernel .rodata (AP=00) okunamaz, o
 * yüzden tablo değil hesap kullanılır. String literalleri kernel (sys 5) okur
 * → onlar sorunsuz; ama EL0 kodu dereference eden her şey user sayfasında olmalı. */
__attribute__((always_inline)) static inline char nibble_hex(unsigned char n) {
    n &= 0xf;
    return (char)(n < 10 ? ('0' + n) : ('a' + (n - 10)));
}
/* Onaltılık bir byte'ı EL0'dan sys(5) ile bas. `iki` user yığınında (0x42000000
 * sayfası) → user-VA guard geçer; kernel sys(5)'te okur. */
__attribute__((always_inline)) static inline void bas_bayt_hex(unsigned char b) {
    char iki[3];
    iki[0] = nibble_hex((unsigned char)(b >> 4));
    iki[1] = nibble_hex(b);
    iki[2] = 0;
    sys(5, (unsigned long)(uintptr_t)iki);
}

/*
 * EL0 launcher: TAM DNS protokol yığını syscall üstünde.
 * Tüm tamponlar user yığınında (0x42000000 sayfası) → user-VA guard geçer.
 */
__attribute__((section(".user"), noinline))
static void launcher(void) {
    const unsigned char bizim_mac[6] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
    /* SLIRP gateway sabit MAC (fallback — ARP çözülemezse) */
    unsigned char hedef_mac[6] = { 0x52, 0x55, 0x0a, 0x00, 0x02, 0x02 };
    unsigned char frame[128];
    unsigned char rx[512];
    int i;

    /* --- 1) ARP: DNS sunucusu 10.0.2.3 MAC'ini çöz (fallback = gateway MAC) --- */
    for (i = 0; i < 42; i++) frame[i] = 0;
    for (i = 0; i < 6; i++) frame[i] = 0xff;                 /* dst broadcast */
    for (i = 0; i < 6; i++) frame[6 + i] = bizim_mac[i];     /* src mac */
    frame[12] = 0x08; frame[13] = 0x06;                      /* ethertype ARP */
    frame[14] = 0x00; frame[15] = 0x01;                      /* htype ethernet */
    frame[16] = 0x08; frame[17] = 0x00;                      /* ptype IPv4 */
    frame[18] = 6; frame[19] = 4;                            /* hlen, plen */
    frame[20] = 0x00; frame[21] = 0x01;                      /* oper = request */
    for (i = 0; i < 6; i++) frame[22 + i] = bizim_mac[i];    /* sha */
    frame[28] = 10; frame[29] = 0; frame[30] = 2; frame[31] = 15;  /* spa 10.0.2.15 */
    frame[38] = 10; frame[39] = 0; frame[40] = 2; frame[41] = 3;   /* tpa 10.0.2.3 DNS */
    (void)sys2(24, (unsigned long)(uintptr_t)frame, 42);

    int arp_ok = 0;
    for (int d = 0; d < 60 && !arp_ok; d++) {
        long n = (long)sys2(25, (unsigned long)(uintptr_t)rx, 128);
        if (n < 42) continue;
        if (rx[12] != 0x08 || rx[13] != 0x06) continue;      /* ARP */
        if (rx[20] != 0x00 || rx[21] != 0x02) continue;      /* reply */
        if (rx[28] == 10 && rx[29] == 0 && rx[30] == 2 && rx[31] == 3) {
            for (i = 0; i < 6; i++) hedef_mac[i] = rx[22 + i];   /* sha = DNS MAC */
            arp_ok = 1;
        }
    }
    /* arp_ok=0 ise hedef_mac sabit SLIRP gateway MAC'inde kalır (fallback). */

    /* --- 2) DNS sorgusu inşa et: eth + IPv4 + UDP + DNS("example.com" A) --- */
    /* QNAME "\x07example\x03com\x00" = 13 byte. DNS payload = 12+13+2+2 = 29 byte. */
    const uint16_t dns_id = 0x4B7E;
    unsigned char dns[29];
    for (i = 0; i < 29; i++) dns[i] = 0;
    dns[0] = (unsigned char)(dns_id >> 8); dns[1] = (unsigned char)dns_id;  /* id */
    dns[2] = 0x01; dns[3] = 0x00;          /* flags: RD */
    dns[4] = 0x00; dns[5] = 0x01;          /* qdcount = 1 */
    dns[12] = 7;
    dns[13] = 'e'; dns[14] = 'x'; dns[15] = 'a'; dns[16] = 'm';
    dns[17] = 'p'; dns[18] = 'l'; dns[19] = 'e';
    dns[20] = 3; dns[21] = 'c'; dns[22] = 'o'; dns[23] = 'm'; dns[24] = 0;
    dns[25] = 0x00; dns[26] = 0x01;        /* qtype = A */
    dns[27] = 0x00; dns[28] = 0x01;        /* qclass = IN */
    int dl = 29;

    for (i = 0; i < 128; i++) frame[i] = 0;
    for (i = 0; i < 6; i++) frame[i] = hedef_mac[i];         /* dst = DNS/gateway MAC */
    for (i = 0; i < 6; i++) frame[6 + i] = bizim_mac[i];
    frame[12] = 0x08; frame[13] = 0x00;                      /* IPv4 */
    int ip_total = 20 + 8 + dl;
    frame[14] = 0x45; frame[16] = (unsigned char)(ip_total >> 8); frame[17] = (unsigned char)ip_total;
    frame[20] = 0x40; frame[22] = 64; frame[23] = 17;        /* DF, TTL, UDP */
    frame[26] = 10; frame[27] = 0; frame[28] = 2; frame[29] = 15;   /* src 10.0.2.15 */
    frame[30] = 10; frame[31] = 0; frame[32] = 2; frame[33] = 3;    /* dst 10.0.2.3 DNS */
    uint16_t s = ip_checksum(&frame[14], 20);
    frame[24] = (unsigned char)(s >> 8); frame[25] = (unsigned char)s;
    int udp_len = 8 + dl;
    frame[34] = 0x13; frame[35] = 0x88;                      /* src port 5000 */
    frame[36] = 0x00; frame[37] = 53;                        /* dst port 53 */
    frame[38] = (unsigned char)(udp_len >> 8); frame[39] = (unsigned char)udp_len;
    for (i = 0; i < dl; i++) frame[42 + i] = dns[i];
    int toplam = 42 + dl;

    long g = (long)sys2(24, (unsigned long)(uintptr_t)frame, (unsigned long)toplam);
    if (g < 0) {
        sys(5, (unsigned long)(uintptr_t)"USERDNS GONDER HATA");
        sys(7, 0);
        for (;;) { }
    }
    /* Bu noktada DNS sorgusu userspace'ten sys2(24) ile GÖNDERİLDİ (pcap kanıtı). */

    /* --- 3) DNS yanıtını al + header doğrula (poll) --- */
    /* Yanıt: eth(14) + IP(20) + UDP(8) + DNS payload. DNS payload rx[42]'de. */
    int dns_baz = 42;
    int dns_uzun = 0;
    int yanit_ok = 0;
    for (int d = 0; d < 200 && !yanit_ok; d++) {
        long n = (long)sys2(25, (unsigned long)(uintptr_t)rx, 512);
        if (n < 54) continue;
        if (rx[12] != 0x08 || rx[13] != 0x00) continue;      /* IPv4 */
        if (rx[23] != 17) continue;                          /* UDP */
        if (!(rx[26] == 10 && rx[27] == 0 && rx[28] == 2 && rx[29] == 3)) continue;  /* src 10.0.2.3 */
        if (rx[34] != 0x00 || rx[35] != 53) continue;        /* src port 53 */
        int udp_toplam = ((int)rx[38] << 8) | rx[39];
        dns_uzun = udp_toplam - 8;
        if (dns_uzun < 12 || dns_baz + dns_uzun > (int)n) continue;
        const unsigned char *dp = &rx[dns_baz];
        uint16_t r_id = ((uint16_t)dp[0] << 8) | dp[1];
        int qr = (dp[2] & 0x80) != 0;
        if (r_id == dns_id && qr) yanit_ok = 1;
    }

    if (!yanit_ok) {
        /* FALLBACK: yanıt gelmedi ama sorgu sys2(24) ile gönderildi (pcap'te
         * UDP dst-port 53 kanıtı). Userspace protokol-inşa + gönder kanıtlandı. */
        sys(5, (unsigned long)(uintptr_t)"USERDNS SENT OK");
        sys(7, 0);
        for (;;) { }
    }

    /* --- 4) ANSWER bölümünü parse et → A kaydı IPv4 --- */
    const unsigned char *dp = &rx[dns_baz];
    uint16_t qdcount = ((uint16_t)dp[4] << 8) | dp[5];
    uint16_t ancount = ((uint16_t)dp[6] << 8) | dp[7];

    if (ancount == 0) {
        sys(5, (unsigned long)(uintptr_t)"USERDNS SENT OK");  /* sorgu gitti, cevap boş */
        sys(7, 0);
        for (;;) { }
    }

    /* Question bölümünü atla (qdcount × [QNAME + QTYPE(2) + QCLASS(2)]). */
    int off = 12;
    int hata = 0;
    for (int q = 0; q < qdcount && !hata; q++) {
        off = isim_atla(dp, dns_uzun, off);
        if (off < 0 || off + 4 > dns_uzun) { hata = 1; break; }
        off += 4;
    }

    unsigned char ip[4] = { 0, 0, 0, 0 };
    int bulundu = 0;
    for (int a = 0; a < ancount && !bulundu && !hata; a++) {
        off = isim_atla(dp, dns_uzun, off);              /* NAME (genelde 0xC0 pointer) */
        if (off < 0 || off + 10 > dns_uzun) break;        /* TYPE+CLASS+TTL+RDLENGTH */
        uint16_t type = ((uint16_t)dp[off] << 8) | dp[off + 1];
        uint16_t rdlength = ((uint16_t)dp[off + 8] << 8) | dp[off + 9];
        int rdata_off = off + 10;
        if (rdata_off + rdlength > dns_uzun) break;       /* bozuk paket koruması */
        if (type == 1 && rdlength == 4) {                 /* A kaydı = IPv4 */
            ip[0] = dp[rdata_off + 0];
            ip[1] = dp[rdata_off + 1];
            ip[2] = dp[rdata_off + 2];
            ip[3] = dp[rdata_off + 3];
            bulundu = 1;
        }
        off = rdata_off + rdlength;
    }

    int hepsi_sifir = (ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0);
    if (!bulundu || hepsi_sifir) {
        /* A-kaydı çıkarılamadı ama sorgu gönderildi → SENT kanıtı. */
        sys(5, (unsigned long)(uintptr_t)"USERDNS SENT OK");
        sys(7, 0);
        for (;;) { }
    }

    /* Geçerli non-zero A-kaydı: IP'yi bas + tam çözümleme kanıtı. */
    sys(5, (unsigned long)(uintptr_t)"USERDNS IP=");
    bas_bayt_hex(ip[0]); sys(5, (unsigned long)(uintptr_t)".");
    bas_bayt_hex(ip[1]); sys(5, (unsigned long)(uintptr_t)".");
    bas_bayt_hex(ip[2]); sys(5, (unsigned long)(uintptr_t)".");
    bas_bayt_hex(ip[3]);
    sys(7, 0);

    sys(5, (unsigned long)(uintptr_t)"USERDNS OK");
    sys(7, 0);
    for (;;) { }
}

int main(void) {
    kdl_yazdir_metin("USERDNS BASLA");
    kdl_yazdir_satir();

    /* Kernel (EL1) net sürücüsünü kurar — EL0 süreç yalnız syscall kullanır. */
    uint64_t nb = kdl_virtio_net_bul();
    if (!nb || kdl_virtio_net_kur(nb) != 0) {
        kdl_yazdir_metin("NET KUR HATA"); kdl_yazdir_satir();
        for (;;) { __asm__ volatile("wfe"); }
    }

    kdl_surec_kur_el0_veri(l1_l, l2_l, 0x42000000UL, 0x44000000UL);
    kdl_preempt_baslat();
    int tl = kdl_preempt_gorev_olustur_el0(launcher, kstack_l + sizeof(kstack_l),
                                           (void *)(uintptr_t)0x42380000UL);
    kdl_preempt_gorev_ttbr(tl, l1_l);
    kdl_kesme_kur();
    kdl_timer_baslat();
    kdl_preempt_ac();

    for (;;) { __asm__ volatile("wfe"); }
}
