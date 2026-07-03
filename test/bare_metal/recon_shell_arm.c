/*
 * AG-RECON KABUGU (pentest shell) — aarch64 bare-metal — DONANIM + OS + AG DORUGU.
 *
 * D-188 (shell_arm.c: EL1 interaktif UART kabuk) + D-176/177/178 (userspace net:
 * net_gonder=24/net_al=25 syscall'lari, ICMP ping + DNS) BIRLESIMI. Bu, bir "pentest
 * OS" kabugudur: CANLI komut satiri (UART RX) → AG recon islemi (ping/dns) → sonuc.
 *
 * === Mimari: EL1 kabuk (shell_arm.c modeli) ===
 * Kabuk main() ile EL1'de kosar (QEMU virt boot EL1'e duser). EL0 surec / TTBR swap
 * YOK — ping/dns testleri (userspace_ping_arm.c / userspace_dns_arm.c) ag mantigini
 * EL0'a tasimisti, ama burada tum recon EL1'den yapilir. Bu SORUNSUZ calisir cunku:
 *   (1) SVC EL1'den de calisir — shell_arm.c FS syscall'larini (svc #0) EL1'den
 *       cagirir; ayni sekilde net_gonder(24)/net_al(25) de EL1'den cagrilir.
 *       (boot/start_aarch64.S: Cur-EL-SPx sync vektoru EC=0x15 → kdl_syscall_isle.)
 *   (2) UART RX MMIO (0x09000000, Device sayfa) dogrudan EL1'den okunur (shell_arm.c
 *       gibi) — canli komut satiri PL011 RX'ten byte-byte gelir.
 *   (3) Metin ciktisi (prompt/echo/sonuc) EL1'den runtime TX helper'lariyla yazilir
 *       (kdl_yaz_metin / kdl_yazdir_satir / kdl_yaz_onaltilik) — syscall gerekmez.
 *
 * === Guvenlik (D-150/D-151) uyumu ===
 * net_gonder(24)/net_al(25) frame tamponunu DOGRULAR: user VA araligi
 * [0x42000000, 0x42400000) (kdl_user_yaz_ptr_gecerli). Bu yuzden ARP/ICMP/DNS frame
 * + RX tamponu + RX satir tamponu bu adres blogunda (0x42210000+) tutulur. Boot
 * identity map (kdl_mmu.c) bu blogu EL1-RW yapar → EL1 kabuk serbest yazar/okur,
 * net-syscall validator'i da kabul eder. FS komut adi literalleri RX tamponundan
 * (user VA) gecer → okuma-validatoru gecer.
 *
 * === Boot-burst yarisi + PACE'li giris (shell_arm.c dersi — KRITIK) ===
 * `-serial stdio` + pipe: QEMU stdin'i guest RX'ine besler. PL011 reset'te FIFO
 * KAPALI (1-byte holding reg) → burst OVERRUN. Iki savunma (shell_arm.c ile ayni):
 *   (1) FIFO'yu ac (pl011_fifo_ac, FEN=1 → 16-byte RX slack) — HERHANGI TX'ten ONCE.
 *   (2) Makefile girisi PACE eder (lider gecikme + KARAKTER-KARAKTER ~30ms).
 * Kabuk satir-satir CANLI okur (prompt → satir_oku → calistir).
 *
 * === Deadlock-guard ===
 * satir_oku her byte icin RXFE'yi BOUNDED poll eder; giris biterse spin sinirinda
 * duser → satir basindaysa EOF (-1) doner → kabuk durur. KDL_MAX_KOMUT ikinci
 * bounded cikis. Sonsuz bekleme YOK.
 *
 * === Komutlar ===
 *   ping <son_oktet>   ICMP echo → gateway 10.0.2.<oktet> (SLIRP 10.0.2.2 = deterministik
 *                      echo) → "PING: CANLI" veya "PING: yanit yok".
 *   dns [isim]         DNS A cozumleme (sabit "example.com", isim opsiyonel/yok sayilir
 *                      v1) → "DNS: <IP hex>" veya "DNS: cozulemedi".
 *   ls / oku / yaz     D-135 FS komutlari (opsiyonel kapsam).
 *   (bilinmeyen)       "?".
 *
 * Kanit: bir kabuk CANLI UART komutlarini okuyup gercek ag recon (ICMP ping + DNS)
 *        yapti — "RECON KABUK BASLA" + "RECON> " promptlari + echo + "PING: CANLI"
 *        (deterministik SLIRP echo) + "DNS: ..." + "RECON SHELL OK".
 */
#include <stdint.h>

extern void kdl_yazdir_metin(const char *);   /* metin + newline */
extern void kdl_yaz_metin(const char *);       /* metin, newline'siz */
extern void kdl_yazdir_satir(void);
extern void kdl_yaz_tam(int32_t);              /* newline'siz ondalik sayi */

/* Kernel (EL1) virtio-net surucusunu kurar — kabuk yalniz net_gonder/net_al syscall'lari
 * (ve dogrudan MMIO net) uzerinden ag yapar. (userspace_ping/dns main()'leriyle ayni.) */
extern uint64_t kdl_virtio_net_bul(void);
extern int      kdl_virtio_net_kur(uint64_t base);

/* --- PL011 UART0 RX (shell_arm.c ile ayni harita) --- */
#define KDL_PL011_BASE    0x09000000UL
#define KDL_PL011_DR      0x00u              /* Data Register (RX/TX byte) */
#define KDL_PL011_FR      0x18u              /* Flag Register */
#define KDL_PL011_LCRH    0x2Cu              /* Satir kontrol (FEN bit4) */
#define KDL_PL011_FR_RXFE (1u << 4)          /* RX FIFO bos (1=bos) */
#define KDL_PL011_LCRH_FEN (1u << 4)         /* FIFO etkin (16-byte RX slack) */

/* Bir byte bekleme siniri (deadlock-guard) — shell_arm.c ile ayni (PACE'li satir-arasi
 * gecikmeyi kopruler; giris bitince bounded duser → sonsuz bekleme YOK). */
#define KDL_RX_BAYT_SINIR  8000000UL

/* Islenecek azami komut (giris kesilmese de sinirli calis → deterministik son). */
#define KDL_MAX_KOMUT      8

/* Tamponlar KULLANICI-VA sayfasinda: net_gonder/net_al + FS syscall pointer
 * validator'lari (D-150/D-151) yalniz [0x42000000,0x42400000) kabul eder. Boot
 * identity map (kdl_mmu.c) bu blogu EL1-RW yapar → EL1 kabuk serbest yazar/okur. */
#define KDL_SATIR_BUF   0x42210000UL         /* RX satir tamponu (user VA) */
#define KDL_CIKTI_BUF   0x42214000UL         /* FS oku/ls cikti tamponu (user VA) */
#define KDL_TX_FRAME    0x42218000UL         /* net TX cerceve tamponu (user VA) */
#define KDL_RX_FRAME    0x4221C000UL         /* net RX cerceve tamponu (user VA) */
#define KDL_SATIR_MAX   200                  /* satir azami uzunluk (< blok) */

static _Noreturn void dur(void) { for (;;) { __asm__ volatile("wfe"); } }

/* MMIO 32-bit oku/yaz (shell_arm.c deseni). */
static inline uint32_t pl011_oku(uint32_t ofs) {
    return *(volatile uint32_t *)(uintptr_t)(KDL_PL011_BASE + ofs);
}
static inline void pl011_yaz(uint32_t ofs, uint32_t deger) {
    *(volatile uint32_t *)(uintptr_t)(KDL_PL011_BASE + ofs) = deger;
}

/* RX FIFO'yu ac (RMW: yalniz LCRH.FEN set). main'in ILK isi, TX ONCESI. */
static void pl011_fifo_ac(void) {
    uint32_t lcrh = pl011_oku(KDL_PL011_LCRH);
    pl011_yaz(KDL_PL011_LCRH, lcrh | KDL_PL011_LCRH_FEN);
}

/* --- 2-argumanli syscall (SVC ABI: x8=num, x0=arg0, x1=arg1 → x0=donus) --- */
static inline uint64_t sys2(uint64_t num, uint64_t a0, uint64_t a1) {
    register uint64_t x8 __asm__("x8") = num;
    register uint64_t x0 __asm__("x0") = a0;
    register uint64_t x1 __asm__("x1") = a1;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8), "r"(x1) : "memory");
    return x0;
}
static inline uint64_t sys1(uint64_t num, uint64_t a0) {
    return sys2(num, a0, 0);
}

/* Iki null-sonlu string esit mi (shell_arm.c str_esit ile ayni). */
static int str_esit(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

/* Bir nibble'i hex karaktere cevir (ARITMETIK — userspace_dns_arm.c nibble_hex ile
 * ayni; .rodata lookup tablosu YOK). */
static char nibble_hex(unsigned char n) {
    n &= 0xf;
    return (char)(n < 10 ? ('0' + n) : ('a' + (n - 10)));
}
/* Bir byte'i iki-basamakli hex olarak EL1'den bas (kdl_yaz_metin dogrudan okur —
 * syscall yok, tampon EL1 yigininda serbest). */
static void bas_bayt_hex(unsigned char b) {
    char iki[3];
    iki[0] = nibble_hex((unsigned char)(b >> 4));
    iki[1] = nibble_hex(b);
    iki[2] = 0;
    kdl_yaz_metin(iki);
}

/* ASCII ondalik string'i tamsayiya cevir (basit, isaretsiz; ilk sayisal-disi'da durur).
 * "ping <oktet>" argumanini (ornek "2") oktet degerine cevirir. */
static int str_to_int(const char *s) {
    int v = 0;
    while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); s++; }
    return v;
}

/* Satiri bosluklara bol (in-place null-term), en cok 3 token; sayi doner
 * (shell_arm.c tokenize ile ayni). */
static int tokenize(char *satir, char **tok) {
    int nt = 0;
    char *p = satir;
    while (*p && nt < 3) {
        while (*p == ' ') p++;
        if (!*p) break;
        tok[nt++] = p;
        while (*p && *p != ' ') p++;
        if (*p) *p++ = 0;
    }
    return nt;
}

/* RFC1071 internet checksum (IPv4 basligi + ICMP mesaji icin — userspace_ping_arm.c
 * ile ayni). EL1'de self-contained. */
static uint16_t ip_checksum(const unsigned char *veri, int uzun) {
    uint32_t t = 0;
    for (int i = 0; i + 1 < uzun; i += 2) t += ((uint32_t)veri[i] << 8) | veri[i + 1];
    if (uzun & 1) t += (uint32_t)veri[uzun - 1] << 8;
    while (t >> 16) t = (t & 0xffff) + (t >> 16);
    return (uint16_t)(~t);
}

/* DNS isim alanini atla (userspace_dns_arm.c isim_atla ile ayni; sikistirma 0xC0). */
static int isim_atla(const unsigned char *dns, int dns_uzun, int off) {
    while (off < dns_uzun) {
        unsigned char len = dns[off];
        if ((len & 0xC0) == 0xC0) return (off + 2 <= dns_uzun) ? off + 2 : -1;
        if (len == 0) return off + 1;
        off += 1 + (int)len;
    }
    return -1;
}

/* Bizim (guest) MAC + SLIRP gateway sabit MAC (ARP cozulemezse fallback). */
static const unsigned char BIZIM_MAC[6] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
static const unsigned char SLIRP_GW_MAC[6] = { 0x52, 0x55, 0x0a, 0x00, 0x02, 0x02 };

/* Gateway 10.0.2.<hedef_oktet> MAC'ini ARP ile coz (net_gonder/net_al syscall).
 * Cozulemezse SLIRP sabit gateway MAC'ini (deterministik) mac_out'a yazar. */
static void arp_coz(unsigned char *mac_out, int hedef_oktet) {
    unsigned char *frame = (unsigned char *)(uintptr_t)KDL_TX_FRAME;
    unsigned char *rx    = (unsigned char *)(uintptr_t)KDL_RX_FRAME;
    int i;

    for (i = 0; i < 42; i++) frame[i] = 0;
    for (i = 0; i < 6; i++) frame[i] = 0xff;               /* dst broadcast */
    for (i = 0; i < 6; i++) frame[6 + i] = BIZIM_MAC[i];   /* src mac */
    frame[12] = 0x08; frame[13] = 0x06;                    /* ethertype ARP */
    frame[14] = 0x00; frame[15] = 0x01;                    /* htype ethernet */
    frame[16] = 0x08; frame[17] = 0x00;                    /* ptype IPv4 */
    frame[18] = 6; frame[19] = 4;                          /* hlen, plen */
    frame[20] = 0x00; frame[21] = 0x01;                    /* oper = request */
    for (i = 0; i < 6; i++) frame[22 + i] = BIZIM_MAC[i];  /* sha */
    frame[28] = 10; frame[29] = 0; frame[30] = 2; frame[31] = 15;  /* spa 10.0.2.15 */
    frame[38] = 10; frame[39] = 0; frame[40] = 2;
    frame[41] = (unsigned char)hedef_oktet;                /* tpa 10.0.2.<oktet> */
    sys2(24, (uint64_t)(uintptr_t)frame, 42);

    for (i = 0; i < 6; i++) mac_out[i] = SLIRP_GW_MAC[i];  /* fallback (deterministik) */

    for (int deneme = 0; deneme < 200; deneme++) {
        long n = (long)sys2(25, (uint64_t)(uintptr_t)rx, 128);
        if (n < 42) continue;
        if (rx[12] != 0x08 || rx[13] != 0x06) continue;    /* ethertype ARP */
        if (rx[20] != 0x00 || rx[21] != 0x02) continue;    /* oper = reply */
        if (rx[28] == 10 && rx[29] == 0 && rx[30] == 2 &&
            rx[31] == (unsigned char)hedef_oktet) {
            for (i = 0; i < 6; i++) mac_out[i] = rx[22 + i]; /* sha = gateway MAC */
            return;
        }
    }
}

/* ping <son_oktet>: 10.0.2.<oktet>'e ICMP echo yolla + reply bekle (userspace_ping_arm.c
 * launcher mantigi, EL1'e tasindi). Doner: 1 = canli (echo reply), 0 = yanit yok. */
static int komut_ping(int hedef_oktet) {
    unsigned char *frame = (unsigned char *)(uintptr_t)KDL_TX_FRAME;
    unsigned char *rx    = (unsigned char *)(uintptr_t)KDL_RX_FRAME;
    unsigned char gw_mac[6];
    int i;

    arp_coz(gw_mac, hedef_oktet);

    /* --- ICMP Echo Request insa et: header(8) + payload(5) = 13 bayt --- */
    const unsigned char payload[5] = { 'K', 'E', 'M', 'G', 'U' };
    uint16_t icmp_id  = 0xBEEF;
    uint16_t icmp_seq = 0x0001;
    int icmp_len = 8 + 5;
    unsigned char icmp[13];
    for (i = 0; i < icmp_len; i++) icmp[i] = 0;
    icmp[0] = 8;                                           /* type = 8 (echo request) */
    icmp[1] = 0;                                           /* code = 0 */
    icmp[4] = (unsigned char)(icmp_id >> 8);  icmp[5] = (unsigned char)icmp_id;
    icmp[6] = (unsigned char)(icmp_seq >> 8); icmp[7] = (unsigned char)icmp_seq;
    for (i = 0; i < 5; i++) icmp[8 + i] = payload[i];
    uint16_t ics = ip_checksum(icmp, icmp_len);
    icmp[2] = (unsigned char)(ics >> 8); icmp[3] = (unsigned char)ics;

    /* --- Ethernet + IPv4 cercevesini kur --- */
    for (i = 0; i < 128; i++) frame[i] = 0;
    for (i = 0; i < 6; i++) frame[i] = gw_mac[i];          /* dst = gateway MAC */
    for (i = 0; i < 6; i++) frame[6 + i] = BIZIM_MAC[i];   /* src */
    frame[12] = 0x08; frame[13] = 0x00;                    /* ethertype = IPv4 */
    int ip_total = 20 + icmp_len;
    frame[14] = 0x45;                                      /* ver=4, IHL=5 */
    frame[16] = (unsigned char)(ip_total >> 8); frame[17] = (unsigned char)ip_total;
    frame[20] = 0x40;                                      /* flags = DF */
    frame[22] = 64;                                        /* TTL */
    frame[23] = 1;                                         /* proto = 1 (ICMP) */
    frame[26] = 10; frame[27] = 0; frame[28] = 2; frame[29] = 15;  /* src = 10.0.2.15 */
    frame[30] = 10; frame[31] = 0; frame[32] = 2;
    frame[33] = (unsigned char)hedef_oktet;               /* dst = 10.0.2.<oktet> */
    uint16_t ihs = ip_checksum(&frame[14], 20);
    frame[24] = (unsigned char)(ihs >> 8); frame[25] = (unsigned char)ihs;
    for (i = 0; i < icmp_len; i++) frame[34 + i] = icmp[i];
    int toplam = 34 + icmp_len;

    long g = (long)sys2(24, (uint64_t)(uintptr_t)frame, (uint64_t)toplam);
    if (g < 0) return 0;                                   /* gonderim hatasi */

    /* --- Echo Reply'i al + dogrula --- */
    for (int deneme = 0; deneme < 400; deneme++) {
        long n = (long)sys2(25, (uint64_t)(uintptr_t)rx, 128);
        if (n < 42) continue;
        if (rx[12] != 0x08 || rx[13] != 0x00) continue;    /* IPv4 mi */
        if (rx[23] != 1) continue;                         /* proto = ICMP */
        if (!(rx[26] == 10 && rx[27] == 0 && rx[28] == 2 &&
              rx[29] == (unsigned char)hedef_oktet)) continue; /* src = hedef */
        int ihl = (rx[14] & 0x0f) * 4;
        int io = 14 + ihl;
        if (n < io + 8) continue;
        if (rx[io] != 0 || rx[io + 1] != 0) continue;      /* type = 0 reply, code = 0 */
        uint16_t r_id  = ((uint16_t)rx[io + 4] << 8) | rx[io + 5];
        uint16_t r_seq = ((uint16_t)rx[io + 6] << 8) | rx[io + 7];
        if (r_id != icmp_id || r_seq != icmp_seq) continue;
        int pl_ok = 1;
        if (n < io + icmp_len) pl_ok = 0;
        for (i = 0; pl_ok && i < 5; i++)
            if (rx[io + 8 + i] != payload[i]) pl_ok = 0;
        if (!pl_ok) continue;
        return 1;                                          /* echo reply → CANLI */
    }
    return 0;                                              /* yanit yok */
}

/* dns: "example.com" A kaydini coz (userspace_dns_arm.c launcher mantigi, EL1'e
 * tasindi). Basari halinde ip_out[4]'e IPv4 yazar + 1 doner; cozulemezse 0. */
static int komut_dns(unsigned char *ip_out) {
    unsigned char *frame = (unsigned char *)(uintptr_t)KDL_TX_FRAME;
    unsigned char *rx    = (unsigned char *)(uintptr_t)KDL_RX_FRAME;
    unsigned char dns_mac[6];
    int i;

    arp_coz(dns_mac, 3);                                   /* 10.0.2.3 = SLIRP DNS */

    /* --- DNS sorgusu insa et: eth + IPv4 + UDP + DNS("example.com" A) --- */
    const uint16_t dns_id = 0x4B7E;
    unsigned char dns[29];
    for (i = 0; i < 29; i++) dns[i] = 0;
    dns[0] = (unsigned char)(dns_id >> 8); dns[1] = (unsigned char)dns_id;
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
    for (i = 0; i < 6; i++) frame[i] = dns_mac[i];         /* dst = DNS/gateway MAC */
    for (i = 0; i < 6; i++) frame[6 + i] = BIZIM_MAC[i];
    frame[12] = 0x08; frame[13] = 0x00;                    /* IPv4 */
    int ip_total = 20 + 8 + dl;
    frame[14] = 0x45;
    frame[16] = (unsigned char)(ip_total >> 8); frame[17] = (unsigned char)ip_total;
    frame[20] = 0x40; frame[22] = 64; frame[23] = 17;      /* DF, TTL, UDP */
    frame[26] = 10; frame[27] = 0; frame[28] = 2; frame[29] = 15;   /* src 10.0.2.15 */
    frame[30] = 10; frame[31] = 0; frame[32] = 2; frame[33] = 3;    /* dst 10.0.2.3 DNS */
    uint16_t s = ip_checksum(&frame[14], 20);
    frame[24] = (unsigned char)(s >> 8); frame[25] = (unsigned char)s;
    int udp_len = 8 + dl;
    frame[34] = 0x13; frame[35] = 0x88;                    /* src port 5000 */
    frame[36] = 0x00; frame[37] = 53;                      /* dst port 53 */
    frame[38] = (unsigned char)(udp_len >> 8); frame[39] = (unsigned char)udp_len;
    for (i = 0; i < dl; i++) frame[42 + i] = dns[i];
    int toplam = 42 + dl;

    long g = (long)sys2(24, (uint64_t)(uintptr_t)frame, (uint64_t)toplam);
    if (g < 0) return 0;

    /* --- DNS yanitini al + header dogrula --- */
    int dns_baz = 42;
    int dns_uzun = 0;
    int yanit_ok = 0;
    for (int d = 0; d < 200 && !yanit_ok; d++) {
        long n = (long)sys2(25, (uint64_t)(uintptr_t)rx, 512);
        if (n < 54) continue;
        if (rx[12] != 0x08 || rx[13] != 0x00) continue;    /* IPv4 */
        if (rx[23] != 17) continue;                        /* UDP */
        if (!(rx[26] == 10 && rx[27] == 0 && rx[28] == 2 && rx[29] == 3)) continue;
        if (rx[34] != 0x00 || rx[35] != 53) continue;      /* src port 53 */
        int udp_toplam = ((int)rx[38] << 8) | rx[39];
        dns_uzun = udp_toplam - 8;
        if (dns_uzun < 12 || dns_baz + dns_uzun > (int)n) continue;
        const unsigned char *dp = &rx[dns_baz];
        uint16_t r_id = ((uint16_t)dp[0] << 8) | dp[1];
        int qr = (dp[2] & 0x80) != 0;
        if (r_id == dns_id && qr) yanit_ok = 1;
    }
    if (!yanit_ok) return 0;

    /* --- ANSWER bolumu → A kaydi IPv4 --- */
    const unsigned char *dp = &rx[dns_baz];
    uint16_t qdcount = ((uint16_t)dp[4] << 8) | dp[5];
    uint16_t ancount = ((uint16_t)dp[6] << 8) | dp[7];
    if (ancount == 0) return 0;

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
        off = isim_atla(dp, dns_uzun, off);
        if (off < 0 || off + 10 > dns_uzun) break;
        uint16_t type = ((uint16_t)dp[off] << 8) | dp[off + 1];
        uint16_t rdlength = ((uint16_t)dp[off + 8] << 8) | dp[off + 9];
        int rdata_off = off + 10;
        if (rdata_off + rdlength > dns_uzun) break;
        if (type == 1 && rdlength == 4) {                  /* A kaydi = IPv4 */
            ip[0] = dp[rdata_off + 0]; ip[1] = dp[rdata_off + 1];
            ip[2] = dp[rdata_off + 2]; ip[3] = dp[rdata_off + 3];
            bulundu = 1;
        }
        off = rdata_off + rdlength;
    }

    int hepsi_sifir = (ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0);
    if (!bulundu || hepsi_sifir) return 0;

    for (i = 0; i < 4; i++) ip_out[i] = ip[i];
    return 1;
}

/* UART RX'ten BIR SATIR CANLI oku (byte-byte) — shell_arm.c satir_oku ile ayni.
 * Donus: okunan byte sayisi (>=0), veya -1 = EOF (satir basinda giris tukendi). */
static int satir_oku(char *buf) {
    int n = 0;
    for (;;) {
        uint32_t geldi = 0;
        for (uint64_t i = 0; i < KDL_RX_BAYT_SINIR; i++) {
            if (!(pl011_oku(KDL_PL011_FR) & KDL_PL011_FR_RXFE)) { geldi = 1; break; }
        }
        if (!geldi) {
            buf[n] = 0;
            return n > 0 ? n : -1;
        }
        char c = (char)(pl011_oku(KDL_PL011_DR) & 0xFFu);
        if (c == '\n' || c == '\r') { buf[n] = 0; return n; }
        if (n < KDL_SATIR_MAX) buf[n++] = c;
    }
}

/* Bir komut satirini calistir: ping/dns ag recon + FS komutlari (D-135). */
static void komut_calistir(char *satir) {
    char *buf = (char *)(uintptr_t)KDL_CIKTI_BUF;
    unsigned char ip[4];
    char *tok[3];
    int nt = tokenize(satir, tok);
    if (nt == 0) return;                                   /* bos satir — atla */

    if (str_esit(tok[0], "ping")) {
        /* ping <son_oktet> (yoksa 2 = SLIRP gateway 10.0.2.2, deterministik echo). */
        int oktet = (nt >= 2) ? str_to_int(tok[1]) : 2;
        if (oktet <= 0 || oktet > 255) oktet = 2;
        if (komut_ping(oktet)) {
            kdl_yazdir_metin("PING: CANLI");
        } else {
            kdl_yazdir_metin("PING: yanit yok");
        }
    } else if (str_esit(tok[0], "dns")) {
        /* dns [isim] — v1: sabit "example.com" cozulur (isim opsiyonel, yok sayilir). */
        if (komut_dns(ip)) {
            kdl_yaz_metin("DNS: ");
            for (int j = 0; j < 4; j++) {
                if (j) kdl_yaz_metin(".");
                bas_bayt_hex(ip[j]);
            }
            kdl_yazdir_satir();
        } else {
            kdl_yazdir_metin("DNS: cozulemedi");
        }
    } else if (nt >= 3 && str_esit(tok[0], "yaz")) {
        /* D-135 dosya_yaz_metin(ad=tok[1], metin=tok[2]) → num 17. */
        sys2(17, (uint64_t)(uintptr_t)tok[1], (uint64_t)(uintptr_t)tok[2]);
    } else if (nt >= 2 && str_esit(tok[0], "oku")) {
        /* D-135 dosya_oku_metin(ad=tok[1], buf) → num 18 + cikti bas. */
        sys2(18, (uint64_t)(uintptr_t)tok[1], (uint64_t)(uintptr_t)buf);
        kdl_yaz_metin("  ");
        kdl_yaz_metin(buf);
        kdl_yazdir_satir();
    } else if (str_esit(tok[0], "ls")) {
        /* D-135 dosya_sayisi() num 19; her ad dosya_ad(i, buf) num 20 + bas. */
        uint64_t n = sys1(19, 0);
        for (uint64_t i = 0; i < n; i++) {
            sys2(20, i, (uint64_t)(uintptr_t)buf);
            kdl_yaz_metin("  ");
            kdl_yaz_metin(buf);
            kdl_yazdir_satir();
        }
    } else {
        kdl_yazdir_metin("?");                             /* bilinmeyen komut */
    }
}

int main(void) {
    /* RX FIFO'yu ac (16-byte slack) — HERHANGI TX'ten ONCE (shell_arm.c dersi). */
    pl011_fifo_ac();

    kdl_yazdir_metin("RECON KABUK BASLA");
    kdl_yazdir_satir();

    /* Kernel (EL1) virtio-net surucusunu kur — kabuk net_gonder/net_al syscall'lari
     * ile ag yapar. Cihaz yoksa recon komutlari "yanit yok"/"cozulemedi" doner ama
     * kabuk yine calisir (FS komutlari + graceful son). */
    uint64_t nb = kdl_virtio_net_bul();
    if (nb) {
        (void)kdl_virtio_net_kur(nb);
    } else {
        kdl_yazdir_metin("NET YOK");
        kdl_yazdir_satir();
    }

    /* Interaktif recon kabugu dongusu: prompt → RX'ten BIR SATIR CANLI oku → echo →
     * calistir. Makefile girisi PACE eder (lider + karakter-karakter gecikme). Sonlanma:
     * satir_oku EOF (-1) VEYA KDL_MAX_KOMUT — her ikisi bounded → sonsuz bekleme YOK. */
    char *satir = (char *)(uintptr_t)KDL_SATIR_BUF;
    int k = 0;
    for (; k < KDL_MAX_KOMUT; k++) {
        kdl_yaz_metin("RECON> ");                          /* prompt (TX) */
        int r = satir_oku(satir);                          /* RX'ten bir satir (canli) */
        if (r < 0) { kdl_yazdir_satir(); break; }          /* EOF: giris bitti → dur */
        kdl_yaz_metin(satir);                              /* komut echo'su */
        kdl_yazdir_satir();
        komut_calistir(satir);                             /* ping/dns/FS dagit */
    }

    kdl_yazdir_metin("RECON SHELL OK");                    /* ag-recon kabuk kaniti */
    kdl_yazdir_satir();
    dur();
    return 0;
}
