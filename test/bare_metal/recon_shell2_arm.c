/*
 * AG-RECON KABUGU v2 (pentest shell v2) — aarch64 bare-metal.
 *
 * recon_shell_arm.c (D-189: EL1 interaktif UART kabuk + ping/dns) TEMEL ALINDI,
 * IKI YENI RECON KOMUTU eklendi:
 *   scan <son_oktet>   TCP SYN port-taramasi (port_scan_arm.c / D-164 mantigi):
 *                      10.0.2.<oktet>'in 80/443/22 portlarina SYN yolla, her portu
 *                      ACIK / KAPALI / FILTRELI siniflandirir → "SCAN 80:... 443:... 22:...".
 *   arpscan            Subnet ARP taramasi (arp_scan_arm.c / D-158 mantigi): 10.0.2.1..5'e
 *                      ARP istegi yayinla, canli host'lari topla → "ARPSCAN: N host".
 *
 * Mevcut ping/dns komutlari KORUNUR (recon_shell_arm.c'den aynen).
 *
 * === Mimari (recon_shell_arm.c ile ayni) ===
 * EL1 interaktif kabuk (QEMU virt boot EL1'e duser). Tum recon EL1'den net_gonder(24)/
 * net_al(25) syscall'lariyla yapilir (SVC EL1'den de calisir). UART RX MMIO (PL011,
 * 0x09000000) dogrudan EL1'den okunur — canli komut satiri byte-byte gelir. Metin ciktisi
 * runtime TX helper'lariyla (kdl_yaz_metin / kdl_yazdir_satir) yazilir.
 *
 * === Guvenlik (D-150/D-151) uyumu (recon_shell_arm.c ile ayni) ===
 * net_gonder(24)/net_al(25) frame tamponunu user VA araligi [0x42000000,0x42400000)'e
 * kisitlar (kdl_user_yaz_ptr_gecerli). ARP/ICMP/TCP/DNS frame + RX tamponu bu blokta
 * (0x42210000+). Boot identity map bu blogu EL1-RW yapar.
 *
 * === Giris + poll dersleri ===
 *   (1) FIFO'yu ac (pl011_fifo_ac, FEN=1 → 16-byte RX slack) — HERHANGI TX'ten ONCE
 *       (D-188 PL011 1-byte holding reg → burst overrun savunmasi).
 *   (2) Makefile girisi PACE eder (lider gecikme + KARAKTER-KARAKTER ~30ms).
 *   (3) Net-poll KUCUK tik butcesi: net_al(25) syscall'i icten 2M-tik/cagri timeout
 *       kullanir; kabuk sadece SINIRLI SAYIDA poll iterasyonu yapar (arpscan: sadece
 *       ~40 poll + reply toplaninca erken cikis — c8e7124/cde6d00 dersi: buyuk
 *       busy-wait gate'i yuklu makinede QEMU timeout'unu asar, flake eder).
 *
 * === Deadlock-guard (recon_shell_arm.c ile ayni) ===
 * satir_oku her byte icin RXFE'yi BOUNDED poll eder; giris biterse satir basindaysa
 * EOF (-1) doner → kabuk durur. KDL_MAX_KOMUT ikinci bounded cikis. Sonsuz bekleme YOK.
 * Kabuk bir komut basarisiz olsa da devam eder + "RECON2 SHELL OK" basar (ping+arpscan
 * DETERMINISTIK — SLIRP gateway/DNS host — kanit yeter; scan internete baglidir).
 *
 * Kanit: "RECON2 BASLA" + "RECON2> " promptlari + echo + (ping 2) "PING: CANLI"
 *        (deterministik SLIRP echo) + (arpscan) "ARPSCAN: N host" (>=1 host, SLIRP
 *        gateway deterministik) + "RECON2 SHELL OK".
 */
#include <stdint.h>

extern void kdl_yazdir_metin(const char *);   /* metin + newline */
extern void kdl_yaz_metin(const char *);       /* metin, newline'siz */
extern void kdl_yazdir_satir(void);
extern void kdl_yaz_tam(int32_t);              /* newline'siz ondalik sayi */

/* Kernel (EL1) virtio-net surucusunu kurar — kabuk yalniz net_gonder/net_al syscall'lari
 * uzerinden ag yapar (recon_shell_arm.c ile ayni). */
extern uint64_t kdl_virtio_net_bul(void);
extern int      kdl_virtio_net_kur(uint64_t base);

/* --- PL011 UART0 RX (recon_shell_arm.c ile ayni harita) --- */
#define KDL_PL011_BASE    0x09000000UL
#define KDL_PL011_DR      0x00u              /* Data Register (RX/TX byte) */
#define KDL_PL011_FR      0x18u              /* Flag Register */
#define KDL_PL011_LCRH    0x2Cu              /* Satir kontrol (FEN bit4) */
#define KDL_PL011_FR_RXFE (1u << 4)          /* RX FIFO bos (1=bos) */
#define KDL_PL011_LCRH_FEN (1u << 4)         /* FIFO etkin (16-byte RX slack) */

/* Bir byte bekleme siniri (deadlock-guard) — recon_shell_arm.c ile ayni. */
#define KDL_RX_BAYT_SINIR  8000000UL

/* Islenecek azami komut (giris kesilmese de sinirli calis → deterministik son). */
#define KDL_MAX_KOMUT      8

/* Tamponlar KULLANICI-VA sayfasinda (D-150/D-151 net + FS syscall validator'lari yalniz
 * [0x42000000,0x42400000) kabul eder). Boot identity map bu blogu EL1-RW yapar. */
#define KDL_SATIR_BUF   0x42210000UL         /* RX satir tamponu (user VA) */
#define KDL_CIKTI_BUF   0x42214000UL         /* FS oku/ls cikti tamponu (user VA) */
#define KDL_TX_FRAME    0x42218000UL         /* net TX cerceve tamponu (user VA) */
#define KDL_RX_FRAME    0x4221C000UL         /* net RX cerceve tamponu (user VA) */
#define KDL_SATIR_MAX   200                  /* satir azami uzunluk (< blok) */

static _Noreturn void dur(void) { for (;;) { __asm__ volatile("wfe"); } }

/* MMIO 32-bit oku/yaz (recon_shell_arm.c deseni). */
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

/* Iki null-sonlu string esit mi (recon_shell_arm.c str_esit ile ayni). */
static int str_esit(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

/* ASCII ondalik string'i tamsayiya cevir (basit, isaretsiz). */
static int str_to_int(const char *s) {
    int v = 0;
    while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); s++; }
    return v;
}

/* Satiri bosluklara bol (in-place null-term), en cok 3 token; sayi doner
 * (recon_shell_arm.c tokenize ile ayni). */
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

/* RFC1071 internet checksum (recon_shell_arm.c / port_scan_arm.c ile ayni). */
static uint16_t ip_checksum(const unsigned char *veri, int uzun) {
    uint32_t t = 0;
    for (int i = 0; i + 1 < uzun; i += 2) t += ((uint32_t)veri[i] << 8) | veri[i + 1];
    if (uzun & 1) t += (uint32_t)veri[uzun - 1] << 8;
    while (t >> 16) t = (t & 0xffff) + (t >> 16);
    return (uint16_t)(~t);
}

/* Bizim (guest) MAC + SLIRP gateway sabit MAC (ARP cozulemezse fallback). */
static const unsigned char BIZIM_MAC[6] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
static const unsigned char SLIRP_GW_MAC[6] = { 0x52, 0x55, 0x0a, 0x00, 0x02, 0x02 };

/* Gateway 10.0.2.<hedef_oktet> MAC'ini ARP ile coz (net_gonder/net_al syscall).
 * Cozulemezse SLIRP sabit gateway MAC'ini (deterministik) mac_out'a yazar.
 * (recon_shell_arm.c arp_coz ile ayni.) */
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

/* ping <son_oktet>: 10.0.2.<oktet>'e ICMP echo yolla + reply bekle (recon_shell_arm.c
 * komut_ping ile aynen). Doner: 1 = canli (echo reply), 0 = yanit yok. */
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

/* Port durum kodlari (port_scan_arm.c ile ayni). */
#define DURUM_FILTRELI 0
#define DURUM_ACIK     1
#define DURUM_KAPALI   2

/* TCP src port bazi (her taranan port icin +indeks — SLIRP baglanti ayirimi). */
#define KDL_SRC_PORT_BAZ 40000
#define KDL_TCP_SEQ      0x4B454D47UL      /* "KEMG" — pcap marker + baslangic seq */

/* Bizim guest IP (SLIRP 10.0.2.15). */
static const unsigned char BIZIM_IP[4] = { 10, 0, 2, 15 };

/*
 * TCP SYN/RST segmenti insa et (veri yok): eth + IPv4(proto=6) + TCP header (20 bayt).
 * port_scan_arm.c tcp_syn_kur ile ayni pseudo-header checksum mantigi; gw_mac
 * parametrik (arp_coz ile cozulmus gateway MAC). frame[] doldurulur; toplam
 * cerceve uzunlugunu (54) doner.
 */
static int tcp_syn_kur(unsigned char *frame, const unsigned char *gw_mac,
                       const unsigned char *dst_ip, int src_port, int dst_port,
                       uint32_t seq, unsigned char flags) {
    int i;
    for (i = 0; i < 128; i++) frame[i] = 0;
    for (i = 0; i < 6; i++) frame[i] = gw_mac[i];              /* dst = gateway MAC */
    for (i = 0; i < 6; i++) frame[6 + i] = BIZIM_MAC[i];       /* src = bizim mac */
    frame[12] = 0x08; frame[13] = 0x00;                        /* ethertype = IPv4 */

    /* IPv4 header (20 bayt) @ offset 14 */
    int tcp_len = 20;                                         /* TCP header, veri yok */
    int ip_total = 20 + tcp_len;
    frame[14] = 0x45;                                         /* ver=4, IHL=5 */
    frame[16] = (unsigned char)(ip_total >> 8); frame[17] = (unsigned char)ip_total;
    frame[20] = 0x40; frame[21] = 0x00;                      /* flags: DF */
    frame[22] = 64;                                          /* TTL */
    frame[23] = 6;                                           /* protocol = TCP */
    for (i = 0; i < 4; i++) frame[26 + i] = BIZIM_IP[i];      /* src IP 10.0.2.15 */
    for (i = 0; i < 4; i++) frame[30 + i] = dst_ip[i];        /* dst IP = hedef */
    uint16_t ipcs = ip_checksum(&frame[14], 20);
    frame[24] = (unsigned char)(ipcs >> 8); frame[25] = (unsigned char)ipcs;

    /* TCP header (20 bayt) @ offset 34 */
    frame[34] = (unsigned char)(src_port >> 8); frame[35] = (unsigned char)src_port;
    frame[36] = (unsigned char)(dst_port >> 8); frame[37] = (unsigned char)dst_port;
    frame[38] = (unsigned char)(seq >> 24); frame[39] = (unsigned char)(seq >> 16);
    frame[40] = (unsigned char)(seq >> 8);  frame[41] = (unsigned char)seq;
    frame[42] = 0; frame[43] = 0; frame[44] = 0; frame[45] = 0;   /* ack = 0 */
    frame[46] = 0x50;                                        /* data offset = 5 (20 bayt) */
    frame[47] = flags;                                       /* TCP bayraklari (SYN=0x02) */
    frame[48] = 0x20; frame[49] = 0x00;                      /* window = 8192 */
    frame[52] = 0x00; frame[53] = 0x00;                      /* urgent pointer */

    /* TCP checksum: pseudo-header(12) + TCP segmenti(20) = 32 bayt. */
    unsigned char psbuf[12 + 20];
    for (i = 0; i < 32; i++) psbuf[i] = 0;
    for (i = 0; i < 4; i++) psbuf[i] = BIZIM_IP[i];          /* src IP */
    for (i = 0; i < 4; i++) psbuf[4 + i] = dst_ip[i];        /* dst IP */
    psbuf[8] = 0;                                            /* zero */
    psbuf[9] = 6;                                            /* protocol = TCP */
    psbuf[10] = (unsigned char)(tcp_len >> 8); psbuf[11] = (unsigned char)tcp_len;
    for (i = 0; i < tcp_len; i++) psbuf[12 + i] = frame[34 + i];   /* TCP header (cs=0) */
    uint16_t tcs = ip_checksum(psbuf, 12 + tcp_len);
    frame[50] = (unsigned char)(tcs >> 8); frame[51] = (unsigned char)tcs;

    return 34 + tcp_len;                                     /* eth+IP+TCP = 54 */
}

/* Tek bir portu tara: hedef_ip:dst_port'a SYN yolla + kisa poll → durum kodu.
 * port_scan_arm.c port-tarama dongusu (EL1'e tasindi, net_gonder/net_al syscall). */
static int scan_port(const unsigned char *gw_mac, const unsigned char *hedef_ip,
                     int idx, int dst_port) {
    unsigned char *frame = (unsigned char *)(uintptr_t)KDL_TX_FRAME;
    unsigned char *rx    = (unsigned char *)(uintptr_t)KDL_RX_FRAME;
    int src_port = KDL_SRC_PORT_BAZ + idx;   /* her port icin farkli src (baglanti ayirimi) */

    /* SYN gonder: hedef_ip:dst_port, seq="KEMG". */
    int toplam = tcp_syn_kur(frame, gw_mac, hedef_ip, src_port, dst_port,
                             (uint32_t)KDL_TCP_SEQ, 0x02);   /* SYN */
    long g = (long)sys2(24, (uint64_t)(uintptr_t)frame, (uint64_t)toplam);
    if (g < 0) return DURUM_FILTRELI;

    /* Yaniti bekle (kisa poll ~30 iter): SYN-ACK / RST / timeout. */
    int durum = DURUM_FILTRELI;
    for (int d = 0; d < 30 && durum == DURUM_FILTRELI; d++) {
        long n = (long)sys2(25, (uint64_t)(uintptr_t)rx, 128);
        if (n < 54) continue;
        if (rx[12] != 0x08 || rx[13] != 0x00) continue;      /* IPv4 */
        if (rx[23] != 6) continue;                           /* proto = TCP */
        /* src IP = hedef (SLIRP proxy'de kaynak IP korunur) */
        if (!(rx[26] == hedef_ip[0] && rx[27] == hedef_ip[1] &&
              rx[28] == hedef_ip[2] && rx[29] == hedef_ip[3])) continue;
        int rsrc = (rx[34] << 8) | rx[35];                   /* gelen src port */
        int rdst = (rx[36] << 8) | rx[37];                   /* gelen dst port */
        if (rsrc != dst_port || rdst != src_port) continue;  /* bu taramaya ait mi */
        unsigned char flags = rx[47];
        if ((flags & 0x12) == 0x12) {                        /* SYN+ACK → ACIK */
            durum = DURUM_ACIK;
            /* Nazik kapanis: RST yolla (yarim-acik baglantiyi birak). */
            int rt = tcp_syn_kur(frame, gw_mac, hedef_ip, src_port, dst_port,
                                 (uint32_t)(KDL_TCP_SEQ + 1), 0x04);   /* RST */
            sys2(24, (uint64_t)(uintptr_t)frame, (uint64_t)rt);
        } else if (flags & 0x04) {                           /* RST → KAPALI */
            durum = DURUM_KAPALI;
        }
    }
    return durum;
}

/* scan <son_oktet>: 10.0.2.<oktet>'in birkac TCP portuna (80/443/22) SYN yolla,
 * her portu open/closed/filtered siniflandir + tek satirda bas.
 * Hedef basit tutuldu: sabit gateway 10.0.2.<oktet> (DNS-cozum gerekmez). */
static void komut_scan(int hedef_oktet) {
    unsigned char gw_mac[6];
    unsigned char hedef_ip[4] = { 10, 0, 2, (unsigned char)hedef_oktet };
    static const int portlar[3] = { 80, 443, 22 };

    /* Gateway MAC'ini bir kez coz (tum portlar ayni next-hop). */
    arp_coz(gw_mac, hedef_oktet);

    kdl_yaz_metin("SCAN ");
    kdl_yaz_tam((int32_t)hedef_oktet);
    kdl_yaz_metin(":");
    for (int p = 0; p < 3; p++) {
        int durum = scan_port(gw_mac, hedef_ip, p, portlar[p]);
        kdl_yaz_metin(" ");
        kdl_yaz_tam((int32_t)portlar[p]);
        kdl_yaz_metin(":");
        if (durum == DURUM_ACIK)        kdl_yaz_metin("ACIK");
        else if (durum == DURUM_KAPALI) kdl_yaz_metin("KAPALI");
        else                            kdl_yaz_metin("FILTRELI");
    }
    kdl_yazdir_satir();
}

/* arpscan: subnet ARP taramasi (10.0.2.1 .. 10.0.2.5). Her IP'ye ARP istegi yayinla,
 * gelen reply'lerden canli host'lari topla → sayi bas. arp_scan_arm.c mantigi (EL1'e
 * tasindi, net_gonder/net_al syscall). KUCUK poll butcesi (c8e7124/cde6d00 dersi):
 * net_al(25) icten 2M-tik/cagri timeout kullanir; burada sadece ~40 poll iterasyonu +
 * reply toplaninca ardisik-bos erken cikis → yuklu makinede flake etmez. */
#define KDL_ARPSCAN_ILK   1     /* 10.0.2.1 */
#define KDL_ARPSCAN_SON   5     /* 10.0.2.5 (kucuk subnet — SLIRP 10.0.2.2/3 deterministik) */
#define KDL_ARPSCAN_MAX   (KDL_ARPSCAN_SON - KDL_ARPSCAN_ILK + 1)

static int komut_arpscan(void) {
    unsigned char *frame = (unsigned char *)(uintptr_t)KDL_TX_FRAME;
    unsigned char *rx    = (unsigned char *)(uintptr_t)KDL_RX_FRAME;
    uint32_t bulunan_ip[KDL_ARPSCAN_MAX];
    int bulunan_sayi = 0;
    int i;

    /* --- 1) Tum subnet'e ARP istekleri yayinla (10.0.2.1 .. 10.0.2.5). --- */
    for (int son = KDL_ARPSCAN_ILK; son <= KDL_ARPSCAN_SON; son++) {
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
        frame[41] = (unsigned char)son;                        /* tpa 10.0.2.<son> */
        sys2(24, (uint64_t)(uintptr_t)frame, 42);
    }

    /* --- 2) Gelen ARP-reply'leri topla (KUCUK poll butcesi). --- */
    /* net_al(25) icten 2M-tik/cagri timeout → sadece SINIRLI poll iterasyonu +
     * reply toplaninca ardisik-bos erken cikis (arp_scan_arm.c c8e7124 dersi). */
    int bos_ardisik = 0;
    for (int deneme = 0; deneme < 40; deneme++) {
        long n = (long)sys2(25, (uint64_t)(uintptr_t)rx, 128);
        if (n < 42) {
            bos_ardisik++;
            if (bulunan_sayi >= 1 && bos_ardisik > 6) break;   /* yanitlar toplandi */
            continue;
        }
        bos_ardisik = 0;
        if (rx[12] != 0x08 || rx[13] != 0x06) continue;        /* ethertype ARP */
        if (rx[20] != 0x00 || rx[21] != 0x02) continue;        /* oper = reply */
        uint32_t spa = ((uint32_t)rx[28] << 24) | ((uint32_t)rx[29] << 16) |
                       ((uint32_t)rx[30] << 8)  | (uint32_t)rx[31];
        int var = 0;
        for (i = 0; i < bulunan_sayi; i++) if (bulunan_ip[i] == spa) { var = 1; break; }
        if (var) continue;
        if (bulunan_sayi < KDL_ARPSCAN_MAX) bulunan_ip[bulunan_sayi++] = spa;
    }

    return bulunan_sayi;
}

/* UART RX'ten BIR SATIR CANLI oku (byte-byte) — recon_shell_arm.c satir_oku ile ayni.
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

/* Bir komut satirini calistir: ping/dns (recon_shell_arm.c'den) + YENI scan/arpscan. */
static void komut_calistir(char *satir) {
    char *tok[3];
    int nt = tokenize(satir, tok);
    if (nt == 0) return;                                       /* bos satir — atla */

    if (str_esit(tok[0], "ping")) {
        /* ping <son_oktet> (yoksa 2 = SLIRP gateway 10.0.2.2, deterministik echo). */
        int oktet = (nt >= 2) ? str_to_int(tok[1]) : 2;
        if (oktet <= 0 || oktet > 255) oktet = 2;
        if (komut_ping(oktet)) kdl_yazdir_metin("PING: CANLI");
        else                   kdl_yazdir_metin("PING: yanit yok");
    } else if (str_esit(tok[0], "scan")) {
        /* scan <son_oktet> (yoksa 2 = SLIRP gateway 10.0.2.2). TCP SYN 80/443/22. */
        int oktet = (nt >= 2) ? str_to_int(tok[1]) : 2;
        if (oktet <= 0 || oktet > 255) oktet = 2;
        komut_scan(oktet);
    } else if (str_esit(tok[0], "arpscan")) {
        /* arpscan: subnet ARP taramasi (10.0.2.1..5) → canli host sayisi. */
        int host = komut_arpscan();
        kdl_yaz_metin("ARPSCAN: ");
        kdl_yaz_tam((int32_t)host);
        kdl_yaz_metin(" host");
        kdl_yazdir_satir();
    } else {
        kdl_yazdir_metin("?");                                 /* bilinmeyen komut */
    }
}

int main(void) {
    /* RX FIFO'yu ac (16-byte slack) — HERHANGI TX'ten ONCE (recon_shell_arm.c dersi). */
    pl011_fifo_ac();

    kdl_yazdir_metin("RECON2 BASLA");
    kdl_yazdir_satir();

    /* Kernel (EL1) virtio-net surucusunu kur — kabuk net_gonder/net_al syscall'lari ile
     * ag yapar. Cihaz yoksa recon komutlari "yanit yok"/"0 host" doner ama kabuk yine
     * calisir + graceful son. */
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
        kdl_yaz_metin("RECON2> ");                             /* prompt (TX) */
        int r = satir_oku(satir);                              /* RX'ten bir satir (canli) */
        if (r < 0) { kdl_yazdir_satir(); break; }              /* EOF: giris bitti → dur */
        kdl_yaz_metin(satir);                                  /* komut echo'su */
        kdl_yazdir_satir();
        komut_calistir(satir);                                 /* ping/scan/arpscan dagit */
    }

    kdl_yazdir_metin("RECON2 SHELL OK");                       /* ag-recon kabuk v2 kaniti */
    kdl_yazdir_satir();
    dur();
    return 0;
}
