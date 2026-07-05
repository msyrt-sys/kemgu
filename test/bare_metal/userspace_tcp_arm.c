/*
 * USERSPACE TCP HANDSHAKE testi (aarch64) — EL0 süreç SYSCALL ile TAM TCP el sıkışması.
 *
 * D-176 (userspace_net_arm.c) bir EL0 sürecin net_gonder(24)/net_al(25) syscall'larıyla
 * ham ethernet çerçevesi gönderip aldığını (ARP round-trip) kanıtladı. D-177
 * (userspace_dns_arm.c) aynı syscall'lar üstünde TAM DNS (L2-L7) çözümlemesini EL0'a
 * taşıdı. Bu test bir adım daha ileri gider: bir EL0 (yetkisiz) süreç, virtio-net'e
 * DOĞRUDAN erişmeden, YALNIZ syscall ile TAM bir TCP ÜÇ-YÖNLÜ EL SIKIŞMASI yapar:
 *
 *   ARP (gateway MAC) → DNS ("example.com" A) → SYN → SYN-ACK → ACK → ESTABLISHED
 *
 * tcp_connect_arm.c (D-159) bu handshake mantığını KERNEL'de (EL1) —
 * kdl_virtio_net_gonder/al doğrudan çağrılarıyla — yapmıştı. Burada aynı mantık EL0'a
 * taşınır: TCP segment inşası (pseudo-header checksum dâhil) + SYN-ACK doğrulaması +
 * ACK üretimi TAMAMEN userspace'te; ağ kernel-aracılıdır (sys2 24/25). Kanıt: bir
 * userspace program, çekirdek-aracılı ham-frame syscall'larıyla gerçek bir TCP soket
 * el sıkışması gerçekleştirdi (çekirdekte TCP durum-makinesi YOK).
 *
 * Akış (EL0 launcher):
 *   1) ARP: gateway (SLIRP 10.0.2.2) MAC'ini çöz — istek inşa+sys2(24), sys2(25) poll
 *      ile reply'den SHA (MAC) al. Çözülemezse SLIRP sabit gateway MAC'i fallback.
 *   2) ARP: DNS sunucusu (10.0.2.3) MAC'ini çöz (gateway MAC'i ezmez; ayrı dns_mac).
 *   3) DNS: "example.com" A-kaydını çöz → hedef IPv4 (dns_resolver mantığı, EL0'da).
 *   4) TCP SYN: hedef IP:80'e SYN (pseudo-header checksum) sys2(24) ile yolla.
 *      seq = 0x4B454D47 ("KEMG") — pcap fallback marker + başlangıç seq.
 *   5) SYN-ACK: sys2(25) poll ile al → flags=0x12 (SYN+ACK), ack=bizim_seq+1, src
 *      port=80, src IP=hedef doğrula → onların seq'ini al.
 *   6) ACK: flags=0x10, seq=bizim_seq+1, ack=onların_seq+1 gönder → ESTABLISHED.
 *      Nazik kapanış: RST+ACK ile bekleyen bağlantıyı hemen bırak.
 *
 * Kanıt (gerçek handshake): "USERTCP OK" (SYN-ACK RX).
 * FALLBACK (host internet yoksa / SLIRP dış-TCP yanıt vermezse): EL0'ın SYN'i sys2(24)
 * ile GÖNDERDİĞİ pcap'te kanıtlanır (TCP seq = "KEMG") → "USERTCP SENT OK". Makefile
 * hangi yolu kanıtladığını raporlar.
 *
 * Güvenlik (D-150/151, D-177 dersi): launcher tüm frame/rx/psbuf tamponlarını EL0 user
 * yığınında (0x42000000 VA sayfası) tutar → sys2(24)/sys2(25) user-VA guard'ından
 * geçer. EL0 kernel .rodata'yı (AP=00) DEREF EDEMEZ → hedef IP basımı hex aritmetikle
 * yapılır (tablo lookup YOK); string literalleri yalnız sys(5) — kernel EL0 adına okur.
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

/* Internet checksum (RFC 1071) — dns/tcp _arm.c ile aynı. always_inline →
 * .user launcher'a gömülür (ayrı .text sembolü olmaz → EL0 çağrı sorunu yok). */
__attribute__((always_inline)) static inline uint16_t ip_checksum(const unsigned char *veri, int uzun) {
    uint32_t t = 0;
    int i;
    for (i = 0; i + 1 < uzun; i += 2) t += ((uint32_t)veri[i] << 8) | veri[i + 1];
    if (uzun & 1) t += (uint32_t)veri[uzun - 1] << 8;
    while (t >> 16) t = (t & 0xffff) + (t >> 16);
    return (uint16_t)(~t);
}

/* DNS isim alanını atla (compression pointer 0xC0 veya düz label dizisi). */
__attribute__((always_inline)) static inline int isim_atla(const unsigned char *dns, int dns_uzun, int off) {
    while (off < dns_uzun) {
        unsigned char len = dns[off];
        if ((len & 0xC0) == 0xC0) return (off + 2 <= dns_uzun) ? off + 2 : -1;
        if (len == 0) return off + 1;
        off += 1 + (int)len;
    }
    return -1;
}

/* Nibble → hex karakter (ARİTMETİK — .rodata lookup YOK; D-177 dersi: EL0 kernel
 * .rodata AP=00 okuyamaz, o yüzden tablo değil hesap). */
__attribute__((always_inline)) static inline char nibble_hex(unsigned char n) {
    n &= 0xf;
    return (char)(n < 10 ? ('0' + n) : ('a' + (n - 10)));
}
/* Bir byte'ı EL0'dan sys(5) ile hex bas. `iki` user yığınında → user-VA guard geçer;
 * kernel sys(5)'te okur. */
__attribute__((always_inline)) static inline void bas_bayt_hex(unsigned char b) {
    char iki[3];
    iki[0] = nibble_hex((unsigned char)(b >> 4));
    iki[1] = nibble_hex(b);
    iki[2] = 0;
    sys(5, (unsigned long)(uintptr_t)iki);
}

/*
 * TCP segmenti inşa et (veri yok): eth + IPv4(proto=6) + TCP header (20 byte).
 * frame[] tamponunu doldurur; toplam çerçeve uzunluğunu (54) döner. Tüm parametreler +
 * lokal psbuf launcher'ın user yığınında → user-VA guard geçer.
 *   seq/ack = host byte order; flags = TCP bayrak byte'i (SYN=0x02, ACK=0x10, RST=0x04).
 *   dst_mac = SLIRP gateway MAC (dış-IP TCP host'a proxy'lenir).
 */
__attribute__((always_inline)) static inline int tcp_segment_kur(
        unsigned char *frame, const unsigned char *dst_mac, const unsigned char *bizim_mac,
        const unsigned char *bizim_ip, const unsigned char *dst_ip, int src_port, int dst_port,
        uint32_t seq, uint32_t ack, unsigned char flags) {
    int i;
    for (i = 0; i < 128; i++) frame[i] = 0;
    for (i = 0; i < 6; i++) frame[i] = dst_mac[i];             /* dst = gateway MAC */
    for (i = 0; i < 6; i++) frame[6 + i] = bizim_mac[i];       /* src = bizim mac */
    frame[12] = 0x08; frame[13] = 0x00;                        /* ethertype = IPv4 */

    /* IPv4 header (20 byte) @ offset 14 */
    int tcp_len = 20;                                          /* TCP header, veri yok */
    int ip_total = 20 + tcp_len;
    frame[14] = 0x45;                                          /* ver=4, IHL=5 */
    frame[15] = 0x00;
    frame[16] = (unsigned char)(ip_total >> 8); frame[17] = (unsigned char)ip_total;
    frame[18] = 0x00; frame[19] = 0x00;                       /* identification */
    frame[20] = 0x40; frame[21] = 0x00;                       /* flags: DF, frag=0 */
    frame[22] = 64;                                           /* TTL */
    frame[23] = 6;                                            /* protocol = TCP */
    /* frame[24..25] = header checksum (aşağıda) */
    for (i = 0; i < 4; i++) frame[26 + i] = bizim_ip[i];      /* src IP 10.0.2.15 */
    for (i = 0; i < 4; i++) frame[30 + i] = dst_ip[i];        /* dst IP = hedef */
    uint16_t ipcs = ip_checksum(&frame[14], 20);
    frame[24] = (unsigned char)(ipcs >> 8); frame[25] = (unsigned char)ipcs;

    /* TCP header (20 byte) @ offset 34 */
    frame[34] = (unsigned char)(src_port >> 8); frame[35] = (unsigned char)src_port;
    frame[36] = (unsigned char)(dst_port >> 8); frame[37] = (unsigned char)dst_port;
    frame[38] = (unsigned char)(seq >> 24); frame[39] = (unsigned char)(seq >> 16);
    frame[40] = (unsigned char)(seq >> 8);  frame[41] = (unsigned char)seq;            /* seq */
    frame[42] = (unsigned char)(ack >> 24); frame[43] = (unsigned char)(ack >> 16);
    frame[44] = (unsigned char)(ack >> 8);  frame[45] = (unsigned char)ack;            /* ack */
    frame[46] = 0x50;                                          /* data offset = 5 (20 byte) */
    frame[47] = flags;                                        /* TCP bayrakları */
    frame[48] = 0x20; frame[49] = 0x00;                       /* window = 8192 */
    /* frame[50..51] = TCP checksum (aşağıda) */
    frame[52] = 0x00; frame[53] = 0x00;                       /* urgent pointer */

    /* TCP checksum: pseudo-header(12) + TCP segmenti(20) = 32 byte. */
    unsigned char psbuf[12 + 20];
    for (i = 0; i < 32; i++) psbuf[i] = 0;
    for (i = 0; i < 4; i++) psbuf[i] = bizim_ip[i];           /* src IP */
    for (i = 0; i < 4; i++) psbuf[4 + i] = dst_ip[i];         /* dst IP */
    psbuf[8] = 0;                                             /* zero */
    psbuf[9] = 6;                                             /* protocol = TCP */
    psbuf[10] = (unsigned char)(tcp_len >> 8); psbuf[11] = (unsigned char)tcp_len;
    for (i = 0; i < tcp_len; i++) psbuf[12 + i] = frame[34 + i];   /* TCP header (cs=0) */
    uint16_t tcs = ip_checksum(psbuf, 12 + tcp_len);
    frame[50] = (unsigned char)(tcs >> 8); frame[51] = (unsigned char)tcs;

    return 34 + tcp_len;                                       /* eth+IP+TCP = 54 */
}

/*
 * EL0 launcher: TAM TCP üç-yönlü el sıkışması syscall üstünde.
 * Tüm tamponlar user yığınında (0x42000000 sayfası) → user-VA guard geçer.
 */
__attribute__((section(".user"), noinline))
static void launcher(void) {
    const unsigned char bizim_mac[6] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
    const unsigned char bizim_ip[4] = { 10, 0, 2, 15 };
    /* SLIRP gateway sabit MAC (fallback — ARP çözülemezse) */
    unsigned char gw_mac[6] = { 0x52, 0x55, 0x0a, 0x00, 0x02, 0x02 };
    unsigned char dns_mac[6] = { 0x52, 0x55, 0x0a, 0x00, 0x02, 0x03 };
    unsigned char frame[128];
    unsigned char rx[1500];
    int i;

    const int SRC_PORT = 40000;
    const int DST_PORT = 80;                 /* HTTP — internet host SYN-ACK döner */
    const uint32_t TCP_SEQ = 0x4B454D47UL;   /* "KEMG" — pcap fallback marker + başlangıç seq */

    /* --- 1) ARP: gateway 10.0.2.2 MAC'ini çöz (fallback = sabit SLIRP MAC) --- */
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
    frame[38] = 10; frame[39] = 0; frame[40] = 2; frame[41] = 2;   /* tpa 10.0.2.2 gateway */
    (void)sys2(24, (unsigned long)(uintptr_t)frame, 42);

    int gw_ok = 0;
    for (int d = 0; d < 60 && !gw_ok; d++) {
        long n = (long)sys2(25, (unsigned long)(uintptr_t)rx, 128);
        if (n < 42) continue;
        if (rx[12] != 0x08 || rx[13] != 0x06) continue;      /* ARP */
        if (rx[20] != 0x00 || rx[21] != 0x02) continue;      /* reply */
        if (rx[28] == 10 && rx[29] == 0 && rx[30] == 2 && rx[31] == 2) {
            for (i = 0; i < 6; i++) gw_mac[i] = rx[22 + i];  /* sha = gateway MAC */
            gw_ok = 1;
        }
    }
    /* gw_ok=0 ise gw_mac sabit SLIRP gateway MAC'inde kalır (fallback). */

    /* --- 2) ARP: DNS sunucusu 10.0.2.3 MAC'ini çöz (gw_mac'i ezmez) --- */
    for (i = 0; i < 42; i++) frame[i] = 0;
    for (i = 0; i < 6; i++) frame[i] = 0xff;
    for (i = 0; i < 6; i++) frame[6 + i] = bizim_mac[i];
    frame[12] = 0x08; frame[13] = 0x06;
    frame[14] = 0x00; frame[15] = 0x01; frame[16] = 0x08; frame[17] = 0x00;
    frame[18] = 6; frame[19] = 4; frame[20] = 0x00; frame[21] = 0x01;
    for (i = 0; i < 6; i++) frame[22 + i] = bizim_mac[i];
    frame[28] = 10; frame[29] = 0; frame[30] = 2; frame[31] = 15;
    frame[38] = 10; frame[39] = 0; frame[40] = 2; frame[41] = 3;   /* tpa 10.0.2.3 DNS */
    (void)sys2(24, (unsigned long)(uintptr_t)frame, 42);

    for (int d = 0; d < 60; d++) {
        long n = (long)sys2(25, (unsigned long)(uintptr_t)rx, 128);
        if (n < 42) continue;
        if (rx[12] != 0x08 || rx[13] != 0x06) continue;
        if (rx[20] != 0x00 || rx[21] != 0x02) continue;
        if (rx[28] == 10 && rx[29] == 0 && rx[30] == 2 && rx[31] == 3) {
            for (i = 0; i < 6; i++) dns_mac[i] = rx[22 + i];
            break;
        }
    }
    /* dns_mac çözülemezse sabit SLIRP DNS MAC'inde kalır (fallback). */

    /* --- 3) DNS: "example.com" A-kaydını çöz → hedef_ip --- */
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
    for (i = 0; i < 6; i++) frame[i] = dns_mac[i];           /* dst = DNS MAC */
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
    (void)sys2(24, (unsigned long)(uintptr_t)frame, (unsigned long)(42 + dl));

    unsigned char hedef_ip[4] = { 0, 0, 0, 0 };
    int resolve_ok = 0;
    int dns_baz = 42;
    for (int d = 0; d < 200 && !resolve_ok; d++) {
        long n = (long)sys2(25, (unsigned long)(uintptr_t)rx, 512);
        if (n < 54) continue;
        if (rx[12] != 0x08 || rx[13] != 0x00) continue;      /* IPv4 */
        if (rx[23] != 17) continue;                          /* UDP */
        if (!(rx[26] == 10 && rx[27] == 0 && rx[28] == 2 && rx[29] == 3)) continue;  /* src 10.0.2.3 */
        if (rx[34] != 0x00 || rx[35] != 53) continue;        /* src port 53 */
        int udp_toplam = ((int)rx[38] << 8) | rx[39];
        int dns_uzun = udp_toplam - 8;
        if (dns_uzun < 12 || dns_baz + dns_uzun > (int)n) continue;
        const unsigned char *dp = &rx[dns_baz];
        uint16_t r_id = ((uint16_t)dp[0] << 8) | dp[1];
        int qr = (dp[2] & 0x80) != 0;
        if (r_id != dns_id || !qr) continue;
        uint16_t qdcount = ((uint16_t)dp[4] << 8) | dp[5];
        uint16_t ancount = ((uint16_t)dp[6] << 8) | dp[7];
        if (ancount == 0) continue;
        /* Question bölümünü atla. */
        int off = 12;
        int hata = 0;
        for (int q = 0; q < qdcount && !hata; q++) {
            off = isim_atla(dp, dns_uzun, off);
            if (off < 0 || off + 4 > dns_uzun) { hata = 1; break; }
            off += 4;
        }
        if (hata) continue;
        /* Answer record'ları: ilk A kaydı (TYPE=1, RDLENGTH=4). */
        for (int a = 0; a < ancount && !resolve_ok; a++) {
            off = isim_atla(dp, dns_uzun, off);
            if (off < 0 || off + 10 > dns_uzun) break;
            uint16_t type = ((uint16_t)dp[off] << 8) | dp[off + 1];
            uint16_t rdlength = ((uint16_t)dp[off + 8] << 8) | dp[off + 9];
            int rdata_off = off + 10;
            if (rdata_off + rdlength > dns_uzun) break;
            if (type == 1 && rdlength == 4) {
                for (i = 0; i < 4; i++) hedef_ip[i] = dp[rdata_off + i];
                if (!(hedef_ip[0] == 0 && hedef_ip[1] == 0 && hedef_ip[2] == 0 && hedef_ip[3] == 0))
                    resolve_ok = 1;
            }
            off = rdata_off + rdlength;
        }
    }

    if (!resolve_ok) {
        /* DNS çözülemedi (internet yok) — deterministik bir hedef IP kullan
         * (1.1.1.1) ki SYN yine dış-IP'ye gitsin → pcap TX kanıtı üretilsin.
         * SLIRP dış-TCP yanıt verirse gerçek handshake yine tamamlanır. */
        hedef_ip[0] = 1; hedef_ip[1] = 1; hedef_ip[2] = 1; hedef_ip[3] = 1;
    }

    /* Çözülen (veya fallback) hedef IP'yi bas. */
    sys(5, (unsigned long)(uintptr_t)"USERTCP IP=");
    bas_bayt_hex(hedef_ip[0]); sys(5, (unsigned long)(uintptr_t)".");
    bas_bayt_hex(hedef_ip[1]); sys(5, (unsigned long)(uintptr_t)".");
    bas_bayt_hex(hedef_ip[2]); sys(5, (unsigned long)(uintptr_t)".");
    bas_bayt_hex(hedef_ip[3]);
    sys(7, 0);

    /* --- 4) TCP SYN gönder: hedef_ip:80, seq="KEMG" --- */
    int toplam = tcp_segment_kur(frame, gw_mac, bizim_mac, bizim_ip, hedef_ip,
                                 SRC_PORT, DST_PORT, TCP_SEQ, 0, 0x02);   /* SYN */
    long g = (long)sys2(24, (unsigned long)(uintptr_t)frame, (unsigned long)toplam);
    if (g < 0) {
        sys(5, (unsigned long)(uintptr_t)"USERTCP GONDER HATA");
        sys(7, 0);
        for (;;) { }
    }
    /* Bu noktada SYN userspace'ten sys2(24) ile GÖNDERİLDİ (pcap seq "KEMG" kanıtı). */

    /* --- 5) SYN-ACK al (poll): flags=0x12, ack=bizim_seq+1, src port=80, src IP=hedef --- */
    int synack_ok = 0;
    uint32_t onlarin_seq = 0;
    for (int d = 0; d < 200 && !synack_ok; d++) {
        long n = (long)sys2(25, (unsigned long)(uintptr_t)rx, 1500);
        if (n < 54) continue;
        if (rx[12] != 0x08 || rx[13] != 0x00) continue;       /* IPv4 */
        if (rx[23] != 6) continue;                            /* proto = TCP */
        /* src IP = hedef (SLIRP proxy'de kaynak IP korunur) */
        if (!(rx[26] == hedef_ip[0] && rx[27] == hedef_ip[1] &&
              rx[28] == hedef_ip[2] && rx[29] == hedef_ip[3])) continue;
        int rsrc = (rx[34] << 8) | rx[35];                    /* gelen src port */
        int rdst = (rx[36] << 8) | rx[37];                    /* gelen dst port */
        if (rsrc != DST_PORT || rdst != SRC_PORT) continue;
        unsigned char flags = rx[47];
        if ((flags & 0x12) != 0x12) continue;                 /* SYN+ACK bekle */
        /* ack_num = bizim_seq+1 doğrula */
        uint32_t r_ack = ((uint32_t)rx[42] << 24) | ((uint32_t)rx[43] << 16) |
                         ((uint32_t)rx[44] << 8)  | (uint32_t)rx[45];
        if (r_ack != (uint32_t)(TCP_SEQ + 1)) continue;
        onlarin_seq = ((uint32_t)rx[38] << 24) | ((uint32_t)rx[39] << 16) |
                      ((uint32_t)rx[40] << 8)  | (uint32_t)rx[41];
        synack_ok = 1;
    }

    if (!synack_ok) {
        /* FALLBACK: SYN-ACK gelmedi ama SYN sys2(24) ile gönderildi (pcap'te TCP seq
         * "KEMG" = 4b454d47 kanıtı). Userspace TCP-inşa + gönder kanıtlandı. */
        sys(5, (unsigned long)(uintptr_t)"USERTCP SENT OK");
        sys(7, 0);
        for (;;) { }
    }

    /* --- 6) ACK gönder: flags=0x10, seq=bizim_seq+1, ack=onların_seq+1 → ESTABLISHED --- */
    toplam = tcp_segment_kur(frame, gw_mac, bizim_mac, bizim_ip, hedef_ip,
                             SRC_PORT, DST_PORT, (uint32_t)(TCP_SEQ + 1),
                             (uint32_t)(onlarin_seq + 1), 0x10);   /* ACK */
    (void)sys2(24, (unsigned long)(uintptr_t)frame, (unsigned long)toplam);
    /* Nazik kapanış: RST+ACK gönder (bekleyen bağlantıyı hemen bırak). */
    toplam = tcp_segment_kur(frame, gw_mac, bizim_mac, bizim_ip, hedef_ip,
                             SRC_PORT, DST_PORT, (uint32_t)(TCP_SEQ + 1),
                             (uint32_t)(onlarin_seq + 1), 0x14);   /* RST+ACK */
    (void)sys2(24, (unsigned long)(uintptr_t)frame, (unsigned long)toplam);

    /* ESTABLISHED — gerçek üç-yönlü el sıkışması userspace'te tamamlandı. */
    sys(5, (unsigned long)(uintptr_t)"USERTCP OK");
    sys(7, 0);
    for (;;) { }
}

int main(void) {
    kdl_yazdir_metin("USERTCP BASLA");
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
