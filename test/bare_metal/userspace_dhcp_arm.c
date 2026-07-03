/*
 * USERSPACE DHCP testi (aarch64) — EL0 süreç SYSCALL ile AĞ OTO-KONFİGÜRASYON.
 *
 * Bir bare-metal OS açıldığında IP adresini bilmez; ilk iş DHCP ile ağ config
 * almaktır. dhcp_arm.c (D-162) bu DISCOVER/OFFER mantığını KERNEL'de (EL1) yaptı.
 * userspace_dns_arm.c (D-177) TAM DNS'i EL0'a taşıdı. Bu test o zinciri kapatır:
 * bir EL0 (yetkisiz) süreç, virtio-net'e DOĞRUDAN erişmeden, YALNIZ syscall ile
 * KENDİ IP'sini DHCP ile öğrenir:
 *
 *   L2 (Ethernet broadcast) + L3 (IPv4 0.0.0.0 -> 255.255.255.255 + checksum) +
 *   L4 (UDP 68 -> 67) + BOOTP/DHCP (op=1 DISCOVER, magic cookie, option 53=1).
 *
 * SLIRP'in DAHİLİ DHCP sunucusu (10.0.2.2:67) deterministik bir OFFER döner
 * (yiaddr=10.0.2.15). Internet GEREKMEZ — tamamen deterministik.
 *
 * Akış (EL0 launcher):
 *   1) DHCP DISCOVER inşa et → sys2(24) ile yolla.
 *   2) sys2(25) poll ile OFFER'ı al → doğrula (BOOTREPLY op=2, xid eşleşir,
 *      yiaddr non-zero, option 53=2 OFFER).
 *   3) Geçerli OFFER'da yiaddr'ı bas (EL0-güvenli aritmetik hex) +
 *      sys(5,"USERDHCP OK") + sys(7,0).
 *
 * Kanıt: bir userspace program, çekirdek-aracılı ham-frame syscall'larıyla ağ
 * oto-konfigürasyonu (DHCP DISCOVER -> OFFER) yaptı; kendi IP'sini öğrendi.
 *
 * Güvenlik (D-150/151/177 dersi): launcher tüm frame/rx tamponlarını + yazdırılan
 * string'leri EL0 user yığınında (0x42000000 VA sayfası) tutar → sys2(24)/sys2(25)/
 * sys(5) user-VA guard'ından geçer. EL0 kernel .rodata'yı (AP=00) DEREF EDEMEZ:
 * MAC/oktet-basımı .rodata tablosu değil aritmetik hesap kullanır; string
 * literalleri yalnız kernel (sys 5) okur, EL0 onları dereference etmez.
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

/* Bilinen xid — OFFER'da eşleşmeli (dhcp_arm.c ile aynı sabit). */
#define DHCP_XID 0x12345678U

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

/* Bir nibble'ı hex karaktere çevir (0-9 -> '0'..'9', 10-15 -> 'a'..'f').
 * ARİTMETİK — .rodata lookup YOK: EL0'da kernel .rodata (AP=00) okunamaz, o
 * yüzden tablo değil hesap kullanılır (D-177 dersi). */
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
/* Bir '.' ayracı bas (tek karakter user yığınında). */
__attribute__((always_inline)) static inline void bas_nokta(void) {
    char n[2];
    n[0] = '.';
    n[1] = 0;
    sys(5, (unsigned long)(uintptr_t)n);
}

/*
 * EL0 launcher: DHCP DISCOVER -> OFFER protokolü syscall üstünde.
 * Tüm tamponlar user yığınında (0x42000000 sayfası) → user-VA guard geçer.
 * Protokol mantığı dhcp_arm.c (KERNEL) versiyonundan EL0'a taşınmıştır.
 */
__attribute__((section(".user"), noinline))
static void launcher(void) {
    const unsigned char bizim_mac[6] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
    unsigned char frame[400];
    unsigned char rx[600];
    int i;

    /* --- DHCP DISCOVER inşası (dhcp_arm.c mantığı) --- */
    for (i = 0; i < 400; i++) frame[i] = 0;

    /* Ethernet (14) */
    for (i = 0; i < 6; i++) frame[i] = 0xff;               /* dst = broadcast */
    for (i = 0; i < 6; i++) frame[6 + i] = bizim_mac[i];   /* src = bizim MAC */
    frame[12] = 0x08; frame[13] = 0x00;                    /* ethertype = IPv4 */

    /* DHCP payload uzunluğu: BOOTP 236 + magic cookie 4 + options.
     * options: 53,1,1 (3) + 55,4,1,3,6,15 (6) + 255 (1) = 10 bayt. */
    int dhcp_len = 236 + 4 + 10;                           /* = 250 */

    /* IPv4 (offset 14, 20 bayt) */
    int ip_total = 20 + 8 + dhcp_len;
    frame[14] = 0x45;                                      /* v4, IHL=5 */
    frame[15] = 0x00;                                      /* TOS */
    frame[16] = (unsigned char)(ip_total >> 8); frame[17] = (unsigned char)ip_total;
    frame[18] = 0; frame[19] = 0;                          /* id */
    frame[20] = 0x00; frame[21] = 0;                       /* flags/frag = 0 */
    frame[22] = 64;                                        /* TTL */
    frame[23] = 17;                                        /* protocol = UDP */
    frame[24] = 0; frame[25] = 0;                          /* checksum (önce 0) */
    frame[26] = 0; frame[27] = 0; frame[28] = 0; frame[29] = 0;           /* src = 0.0.0.0 */
    frame[30] = 255; frame[31] = 255; frame[32] = 255; frame[33] = 255;   /* dst = 255.255.255.255 */
    uint16_t ipsum = ip_checksum(&frame[14], 20);
    frame[24] = (unsigned char)(ipsum >> 8); frame[25] = (unsigned char)ipsum;

    /* UDP (offset 34, 8 bayt) */
    int udp_len = 8 + dhcp_len;
    frame[34] = 0x00; frame[35] = 68;                      /* src port = 68 (DHCP client) */
    frame[36] = 0x00; frame[37] = 67;                      /* dst port = 67 (DHCP server) */
    frame[38] = (unsigned char)(udp_len >> 8); frame[39] = (unsigned char)udp_len;
    frame[40] = 0; frame[41] = 0;                          /* UDP checksum = 0 (opsiyonel) */

    /* BOOTP/DHCP payload (offset 42) */
    int dd = 42;
    frame[dd + 0] = 1;                                     /* op = BOOTREQUEST */
    frame[dd + 1] = 1;                                     /* htype = Ethernet */
    frame[dd + 2] = 6;                                     /* hlen = 6 */
    frame[dd + 3] = 0;                                     /* hops = 0 */
    frame[dd + 4] = (unsigned char)(DHCP_XID >> 24);       /* xid (big-endian) */
    frame[dd + 5] = (unsigned char)(DHCP_XID >> 16);
    frame[dd + 6] = (unsigned char)(DHCP_XID >> 8);
    frame[dd + 7] = (unsigned char)(DHCP_XID);
    /* secs/flags/ciaddr/yiaddr/siaddr/giaddr = sıfır (zaten). */
    for (i = 0; i < 6; i++) frame[dd + 28 + i] = bizim_mac[i];   /* chaddr = bizim MAC */

    /* magic cookie @ offset 236 */
    int mc = dd + 236;
    frame[mc + 0] = 0x63; frame[mc + 1] = 0x82; frame[mc + 2] = 0x53; frame[mc + 3] = 0x63;

    /* options @ offset 240 */
    int op = mc + 4;
    frame[op + 0] = 53; frame[op + 1] = 1; frame[op + 2] = 1;   /* option 53: DHCP msg type = DISCOVER */
    frame[op + 3] = 55; frame[op + 4] = 4;                      /* option 55: param request list */
    frame[op + 5] = 1;                                          /*   subnet mask */
    frame[op + 6] = 3;                                          /*   router */
    frame[op + 7] = 6;                                          /*   DNS */
    frame[op + 8] = 15;                                         /*   domain name */
    frame[op + 9] = 255;                                        /* option 255: end */

    int toplam = 42 + dhcp_len;                            /* = 292 */
    if (toplam < 60) toplam = 60;

    /* (1) SYSCALL ile DHCP DISCOVER'ı yolla. */
    long g = (long)sys2(24, (unsigned long)(uintptr_t)frame, (unsigned long)toplam);
    if (g < 0) {
        sys(5, (unsigned long)(uintptr_t)"USERDHCP GONDER HATA");
        sys(7, 0);
        for (;;) { }
    }

    /* (2) SYSCALL ile OFFER'ı al (poll — kısa per-çağrı timeout, EL0 döngüsü). */
    int offer_ok = 0;
    unsigned char yi0 = 0, yi1 = 0, yi2 = 0, yi3 = 0;
    for (int t = 0; t < 200 && !offer_ok; t++) {
        long n = (long)sys2(25, (unsigned long)(uintptr_t)rx, 600);
        if (n < 240 + 42) continue;                        /* eth+ip+udp+bootp+cookie+en az 1 opt */
        /* IPv4 + UDP kontrolü */
        if (!(rx[12] == 0x08 && rx[13] == 0x00 && rx[23] == 17)) continue;
        /* UDP port: src 67 -> dst 68 */
        if (!(rx[34] == 0x00 && rx[35] == 67 && rx[36] == 0x00 && rx[37] == 68)) continue;
        /* BOOTP başlangıcı offset 42 */
        int b = 42;
        if (rx[b + 0] != 2) continue;                      /* op = BOOTREPLY (OFFER) */
        /* xid eşleşmesi (offset 4..7) */
        uint32_t xid = ((uint32_t)rx[b + 4] << 24) | ((uint32_t)rx[b + 5] << 16) |
                       ((uint32_t)rx[b + 6] << 8) | (uint32_t)rx[b + 7];
        if (xid != DHCP_XID) continue;
        /* yiaddr (offset 16..19) — non-zero şart */
        yi0 = rx[b + 16]; yi1 = rx[b + 17]; yi2 = rx[b + 18]; yi3 = rx[b + 19];
        if (yi0 == 0 && yi1 == 0 && yi2 == 0 && yi3 == 0) continue;
        /* magic cookie (offset 236..239) */
        int mco = b + 236;
        if (!(rx[mco + 0] == 0x63 && rx[mco + 1] == 0x82 &&
              rx[mco + 2] == 0x53 && rx[mco + 3] == 0x63)) continue;
        /* option 53 = 2 (OFFER) arama (240'tan itibaren TLV yürü) */
        int o = mco + 4;
        int msg_tipi = 0;
        while (o < n) {
            unsigned char kod = rx[o];
            if (kod == 255) break;                         /* end */
            if (kod == 0) { o++; continue; }               /* pad */
            if (o + 1 >= n) break;
            unsigned char ln = rx[o + 1];
            if (kod == 53 && ln >= 1 && o + 2 < n) msg_tipi = rx[o + 2];
            o += 2 + (int)ln;
        }
        if (msg_tipi != 2) continue;                       /* option 53 = OFFER (2) şart */
        offer_ok = 1;
    }

    if (!offer_ok) {
        sys(5, (unsigned long)(uintptr_t)"USERDHCP OFFER YOK");
        sys(7, 0);
        for (;;) { }
    }

    /* (3) Geçerli OFFER: yiaddr'ı bas (EL0-güvenli aritmetik hex) + kanıt. */
    sys(5, (unsigned long)(uintptr_t)"USERDHCP YIADDR=");
    bas_bayt_hex(yi0); bas_nokta();
    bas_bayt_hex(yi1); bas_nokta();
    bas_bayt_hex(yi2); bas_nokta();
    bas_bayt_hex(yi3);
    sys(7, 0);

    sys(5, (unsigned long)(uintptr_t)"USERDHCP OK");
    sys(7, 0);
    for (;;) { }
}

int main(void) {
    kdl_yazdir_metin("USERDHCP BASLA");
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
