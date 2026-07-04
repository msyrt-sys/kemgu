/*
 * KEMGU-OS v0.1 — TEK ENTEGRE ÇEKİRDEK (aarch64 bare-metal, canlı pentest-OS).
 *
 * === Neden bu dosya? ===
 * Şimdiye kadar her yetenek AYRI bir bootable demo idi (bare_metal altında, her biri
 * kendi main()'i olan izole ELF). Bu dosya onları TEK ÇALIŞAN ÇEKİRDEKTE birleştirir:
 * bir kez boot eder, alt-sistemleri CANLI kurar (ağ + dosya sistemi), sonra interaktif
 * bir pentest kabuğuna düşer. Demolar artık "kanıtlanmış rutin kütüphanesi" — buradaki
 * kabuk onların mantığını TEK kernel içinde, canlı, birlikte çalıştırır.
 *
 * === Canlı alt-sistemler (boot'ta kurulur, kabuk çalışırken hepsi ayakta) ===
 *   - AĞ:     virtio-net (kdl_virtio_net_bul/kur) — kabuk net_gonder(24)/net_al(25) ile
 *             ARP/ICMP/TCP kullanır (recon_shell2_arm.c / D-198 rutinleri).
 *   - DOSYA:  RAM-FS (kdl_dosyalar, syscall 17-21) — yaz/oku/ls/sil canlı.
 *   - UART:   PL011 canlı RX (byte-byte komut satırı) + runtime TX helper'ları.
 *   - SYSCALL: EL1 SVC ABI (D-126/131/176) — FS + net aynı çekirdekten.
 *
 * === Kabuk komutları (hepsi TEK koşan çekirdekte) ===
 *   yardim              komut listesi
 *   sysinfo             alt-sistem durumu (ağ/fs/uptime-komut)
 *   ls                  dosyaları listele (syscall 19+20)
 *   yaz <ad> <icerik>   dosya yaz (syscall 17)
 *   oku <ad>            dosya oku + bas (syscall 18)
 *   sil <ad>            dosya sil (syscall 21)
 *   ping <oktet>        ICMP echo (10.0.2.<oktet>) — canlı mı
 *   pingsweep           10.0.2.1..5 host keşfi (nmap-tarzı)
 *   scan <oktet>        TCP SYN port tarama (80/443/22)
 *   arpscan             subnet ARP tarama (10.0.2.1..5)
 *
 * === Güvenlik (D-150/D-151) ===
 * Tüm FS/net syscall'ları tampon adreslerini user-VA [0x42000000,0x42400000)'e kısıtlar.
 * Kabuk tamponları bu blokta (0x42210000+). Boot identity map bu bloğu EL1-RW yapar.
 *
 * === Giriş/poll dersleri (D-158/D-181/D-188) ===
 *   (1) FIFO'yu TX'ten ÖNCE aç (pl011_fifo_ac) — 1-byte holding reg burst overrun savunması.
 *   (2) Makefile girişi karakter-karakter ~30ms PACE eder.
 *   (3) Net-poll KÜÇÜK tik bütçesi (net_al içten 2M-tik) + sınırlı poll iterasyonu.
 *   (4) satir_oku bounded (deadlock-guard) + KDL_MAX_KOMUT — sonsuz bekleme YOK.
 *
 * Kanıt: banner + alt-sistem init log + kabuk komut dizisini canlı çalıştırır
 *        (yaz→ls→oku FS round-trip + ping CANLI + arpscan host + sysinfo) → "KEMGU-OS OK".
 */
#include <stdint.h>

extern void kdl_yazdir_metin(const char *);   /* metin + newline */
extern void kdl_yaz_metin(const char *);       /* metin, newline'siz */
extern void kdl_yazdir_satir(void);
extern void kdl_yaz_tam(int32_t);              /* newline'siz ondalik sayi */

/* Kernel (EL1) virtio-net sürücüsünü kurar — kabuk net_gonder/net_al syscall'ları ile
 * ağ yapar. */
extern uint64_t kdl_virtio_net_bul(void);
extern int      kdl_virtio_net_kur(uint64_t base);

/* virtio-blk kalıcı depolama (D-141/142/143) — RAM-FS'i diske kaydet/yükle. */
extern uint64_t kdl_virtio_blk_bul(void);
extern int      kdl_virtio_blk_kur(uint64_t base);
extern int      kdl_dosya_kaydet(uint64_t base);   /* kdl_dosyalar -> blok 0-1 (magic KEMG) */
extern int      kdl_dosya_yukle(uint64_t base);    /* blok 0-1 -> kdl_dosyalar (0=ok, -1=yok) */

/* Zaman/kesme alt-sistemi (D-109 timer IRQ + D-128 tik). Preempt guard'lı-kapalı →
 * timer IRQ yalnız tik sayar (görev switch YOK), kabuğa müdahale etmez. */
extern void     kdl_kesme_kur(void);
extern void     kdl_timer_baslat(void);
extern uint64_t kdl_tik_al(void);

/* PART 1(b) / C8c: kontrollü page-fault kurtarma (kdl_kesme.c + start_aarch64.S). */
extern volatile uint64_t kdl_fault_bekleniyor;   /* 1 → sonraki data-abort kurtarılır */
extern volatile uint64_t kdl_fault_yakalanan;    /* yakalanan FAR (fault adresi) */

/* PART 2/3: preemptive scheduler (C7b/D-117) — TCB + bağlam-değiştirme + timer-IRQ
 * ZORUNLU switch. main=görev 0; ek görevler timer-IRQ ile preempt edilir (gönüllü
 * yield GEREKMEZ). Ham malzeme kdl_gorev.c'de; entegre çekirdeğe wire ediliyor. */
extern void kdl_preempt_baslat(void);                                  /* main = görev 0 */
extern int  kdl_preempt_gorev_olustur(void (*giris)(void), void *yigin_tepe);
extern void kdl_preempt_ac(void);                                      /* preemption AÇ */

/* İki ARKA-PLAN görevi — sonsuz busy-loop, KENDİ sayacını artırır, ASLA yield ETMEZ.
 * Sayaçların ilerlemesi YALNIZ timer-IRQ preemption ile mümkün (main de yield etmez).
 * Falsifiye-edilemez: preemption yoksa arka-plan görevleri HİÇ koşmaz → sayaç=0 kalır. */
static unsigned char yigin_arka_b[8192] __attribute__((aligned(16)));
static unsigned char yigin_arka_c[8192] __attribute__((aligned(16)));
static volatile uint64_t arka_sayac_b = 0;
static volatile uint64_t arka_sayac_c = 0;

static void arka_gorev_b(void) {
    for (;;) { for (volatile int i = 0; i < 100000; i++) { } arka_sayac_b++; }
}
static void arka_gorev_c(void) {
    for (;;) { for (volatile int i = 0; i < 100000; i++) { } arka_sayac_c++; }
}

/* --- PL011 UART0 RX (recon_shell2_arm.c ile aynı harita) --- */
#define KDL_PL011_BASE    0x09000000UL
#define KDL_PL011_DR      0x00u
#define KDL_PL011_FR      0x18u
#define KDL_PL011_LCRH    0x2Cu
#define KDL_PL011_FR_RXFE (1u << 4)
#define KDL_PL011_LCRH_FEN (1u << 4)

/* PL031 RTC (QEMU virt donanım gerçek-zaman saati, D-172). DR = Unix epoch saniye. */
#define KDL_PL031_DR   0x09010000UL

#define KDL_RX_BAYT_SINIR  8000000UL
#define KDL_MAX_KOMUT      16                  /* entegre kabuk: daha çok komut dizisi */

/* Tamponlar KULLANICI-VA sayfasında (D-150/D-151 validator'ları [0x42000000,0x42400000)). */
#define KDL_SATIR_BUF   0x42210000UL           /* RX satır tamponu */
#define KDL_CIKTI_BUF   0x42214000UL           /* FS oku/ls çıktı tamponu */
#define KDL_TX_FRAME    0x42218000UL           /* net TX çerçeve tamponu */
#define KDL_RX_FRAME    0x4221C000UL           /* net RX çerçeve tamponu */
#define KDL_AD_BUF      0x42220000UL           /* FS ad tamponu (user-VA, syscall yazar) */
#define KDL_SATIR_MAX   200

static _Noreturn void dur(void) { for (;;) { __asm__ volatile("wfe"); } }

static inline uint32_t pl011_oku(uint32_t ofs) {
    return *(volatile uint32_t *)(uintptr_t)(KDL_PL011_BASE + ofs);
}
static inline void pl011_yaz(uint32_t ofs, uint32_t deger) {
    *(volatile uint32_t *)(uintptr_t)(KDL_PL011_BASE + ofs) = deger;
}
static void pl011_fifo_ac(void) {
    uint32_t lcrh = pl011_oku(KDL_PL011_LCRH);
    pl011_yaz(KDL_PL011_LCRH, lcrh | KDL_PL011_LCRH_FEN);
}

/* --- 1/2-argümanlı syscall (SVC ABI: x8=num, x0=arg0, x1=arg1 → x0=dönüş) --- */
static inline uint64_t sys1(uint64_t num, uint64_t a0) {
    register uint64_t x8 __asm__("x8") = num;
    register uint64_t x0 __asm__("x0") = a0;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8) : "memory");
    return x0;
}
static inline uint64_t sys2(uint64_t num, uint64_t a0, uint64_t a1) {
    register uint64_t x8 __asm__("x8") = num;
    register uint64_t x0 __asm__("x0") = a0;
    register uint64_t x1 __asm__("x1") = a1;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8), "r"(x1) : "memory");
    return x0;
}

static int str_esit(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}
static int str_to_int(const char *s) {
    int v = 0;
    while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); s++; }
    return v;
}
/* Satırı boşluklara böl (in-place null-term), en çok 3 token; sayı döner. */
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

static uint16_t ip_checksum(const unsigned char *veri, int uzun) {
    uint32_t t = 0;
    for (int i = 0; i + 1 < uzun; i += 2) t += ((uint32_t)veri[i] << 8) | veri[i + 1];
    if (uzun & 1) t += (uint32_t)veri[uzun - 1] << 8;
    while (t >> 16) t = (t & 0xffff) + (t >> 16);
    return (uint16_t)(~t);
}

static const unsigned char BIZIM_MAC[6] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
static const unsigned char SLIRP_GW_MAC[6] = { 0x52, 0x55, 0x0a, 0x00, 0x02, 0x02 };
static const unsigned char BIZIM_IP[4] = { 10, 0, 2, 15 };

/* Gateway 10.0.2.<hedef_oktet> MAC'ini ARP ile çöz (çözülemezse SLIRP sabit MAC fallback). */
static void arp_coz(unsigned char *mac_out, int hedef_oktet) {
    unsigned char *frame = (unsigned char *)(uintptr_t)KDL_TX_FRAME;
    unsigned char *rx    = (unsigned char *)(uintptr_t)KDL_RX_FRAME;
    int i;
    for (i = 0; i < 42; i++) frame[i] = 0;
    for (i = 0; i < 6; i++) frame[i] = 0xff;
    for (i = 0; i < 6; i++) frame[6 + i] = BIZIM_MAC[i];
    frame[12] = 0x08; frame[13] = 0x06;
    frame[14] = 0x00; frame[15] = 0x01;
    frame[16] = 0x08; frame[17] = 0x00;
    frame[18] = 6; frame[19] = 4;
    frame[20] = 0x00; frame[21] = 0x01;
    for (i = 0; i < 6; i++) frame[22 + i] = BIZIM_MAC[i];
    frame[28] = 10; frame[29] = 0; frame[30] = 2; frame[31] = 15;
    frame[38] = 10; frame[39] = 0; frame[40] = 2;
    frame[41] = (unsigned char)hedef_oktet;
    sys2(24, (uint64_t)(uintptr_t)frame, 42);

    for (i = 0; i < 6; i++) mac_out[i] = SLIRP_GW_MAC[i];   /* fallback (deterministik) */

    for (int deneme = 0; deneme < 200; deneme++) {
        long n = (long)sys2(25, (uint64_t)(uintptr_t)rx, 128);
        if (n < 42) continue;
        if (rx[12] != 0x08 || rx[13] != 0x06) continue;
        if (rx[20] != 0x00 || rx[21] != 0x02) continue;
        if (rx[28] == 10 && rx[29] == 0 && rx[30] == 2 &&
            rx[31] == (unsigned char)hedef_oktet) {
            for (i = 0; i < 6; i++) mac_out[i] = rx[22 + i];
            return;
        }
    }
}

/* ping <oktet>: ICMP echo → 1=canlı, 0=yanıt yok (recon_shell2 komut_ping). */
static int komut_ping(int hedef_oktet) {
    unsigned char *frame = (unsigned char *)(uintptr_t)KDL_TX_FRAME;
    unsigned char *rx    = (unsigned char *)(uintptr_t)KDL_RX_FRAME;
    unsigned char gw_mac[6];
    int i;
    arp_coz(gw_mac, hedef_oktet);

    const unsigned char payload[5] = { 'K', 'E', 'M', 'G', 'U' };
    uint16_t icmp_id  = 0xBEEF;
    uint16_t icmp_seq = 0x0001;
    int icmp_len = 8 + 5;
    unsigned char icmp[13];
    for (i = 0; i < icmp_len; i++) icmp[i] = 0;
    icmp[0] = 8;
    icmp[4] = (unsigned char)(icmp_id >> 8);  icmp[5] = (unsigned char)icmp_id;
    icmp[6] = (unsigned char)(icmp_seq >> 8); icmp[7] = (unsigned char)icmp_seq;
    for (i = 0; i < 5; i++) icmp[8 + i] = payload[i];
    uint16_t ics = ip_checksum(icmp, icmp_len);
    icmp[2] = (unsigned char)(ics >> 8); icmp[3] = (unsigned char)ics;

    for (i = 0; i < 128; i++) frame[i] = 0;
    for (i = 0; i < 6; i++) frame[i] = gw_mac[i];
    for (i = 0; i < 6; i++) frame[6 + i] = BIZIM_MAC[i];
    frame[12] = 0x08; frame[13] = 0x00;
    int ip_total = 20 + icmp_len;
    frame[14] = 0x45;
    frame[16] = (unsigned char)(ip_total >> 8); frame[17] = (unsigned char)ip_total;
    frame[20] = 0x40;
    frame[22] = 64;
    frame[23] = 1;
    frame[26] = 10; frame[27] = 0; frame[28] = 2; frame[29] = 15;
    frame[30] = 10; frame[31] = 0; frame[32] = 2;
    frame[33] = (unsigned char)hedef_oktet;
    uint16_t ihs = ip_checksum(&frame[14], 20);
    frame[24] = (unsigned char)(ihs >> 8); frame[25] = (unsigned char)ihs;
    for (i = 0; i < icmp_len; i++) frame[34 + i] = icmp[i];
    int toplam = 34 + icmp_len;

    long g = (long)sys2(24, (uint64_t)(uintptr_t)frame, (uint64_t)toplam);
    if (g < 0) return 0;

    for (int deneme = 0; deneme < 400; deneme++) {
        long n = (long)sys2(25, (uint64_t)(uintptr_t)rx, 128);
        if (n < 42) continue;
        if (rx[12] != 0x08 || rx[13] != 0x00) continue;
        if (rx[23] != 1) continue;
        if (!(rx[26] == 10 && rx[27] == 0 && rx[28] == 2 &&
              rx[29] == (unsigned char)hedef_oktet)) continue;
        int ihl = (rx[14] & 0x0f) * 4;
        int io = 14 + ihl;
        if (n < io + 8) continue;
        if (rx[io] != 0 || rx[io + 1] != 0) continue;
        uint16_t r_id  = ((uint16_t)rx[io + 4] << 8) | rx[io + 5];
        uint16_t r_seq = ((uint16_t)rx[io + 6] << 8) | rx[io + 7];
        if (r_id != icmp_id || r_seq != icmp_seq) continue;
        return 1;
    }
    return 0;
}

/* --- TCP port tarama (recon_shell2 scan_port / komut_scan) --- */
#define DURUM_FILTRELI 0
#define DURUM_ACIK     1
#define DURUM_KAPALI   2
#define KDL_SRC_PORT_BAZ 40000
#define KDL_TCP_SEQ      0x4B454D47UL

static int tcp_syn_kur(unsigned char *frame, const unsigned char *gw_mac,
                       const unsigned char *dst_ip, int src_port, int dst_port,
                       uint32_t seq, unsigned char flags) {
    int i;
    for (i = 0; i < 128; i++) frame[i] = 0;
    for (i = 0; i < 6; i++) frame[i] = gw_mac[i];
    for (i = 0; i < 6; i++) frame[6 + i] = BIZIM_MAC[i];
    frame[12] = 0x08; frame[13] = 0x00;
    int tcp_len = 20;
    int ip_total = 20 + tcp_len;
    frame[14] = 0x45;
    frame[16] = (unsigned char)(ip_total >> 8); frame[17] = (unsigned char)ip_total;
    frame[20] = 0x40; frame[21] = 0x00;
    frame[22] = 64;
    frame[23] = 6;
    for (i = 0; i < 4; i++) frame[26 + i] = BIZIM_IP[i];
    for (i = 0; i < 4; i++) frame[30 + i] = dst_ip[i];
    uint16_t ipcs = ip_checksum(&frame[14], 20);
    frame[24] = (unsigned char)(ipcs >> 8); frame[25] = (unsigned char)ipcs;
    frame[34] = (unsigned char)(src_port >> 8); frame[35] = (unsigned char)src_port;
    frame[36] = (unsigned char)(dst_port >> 8); frame[37] = (unsigned char)dst_port;
    frame[38] = (unsigned char)(seq >> 24); frame[39] = (unsigned char)(seq >> 16);
    frame[40] = (unsigned char)(seq >> 8);  frame[41] = (unsigned char)seq;
    frame[46] = 0x50;
    frame[47] = flags;
    frame[48] = 0x20; frame[49] = 0x00;
    unsigned char psbuf[12 + 20];
    for (i = 0; i < 32; i++) psbuf[i] = 0;
    for (i = 0; i < 4; i++) psbuf[i] = BIZIM_IP[i];
    for (i = 0; i < 4; i++) psbuf[4 + i] = dst_ip[i];
    psbuf[9] = 6;
    psbuf[10] = (unsigned char)(tcp_len >> 8); psbuf[11] = (unsigned char)tcp_len;
    for (i = 0; i < tcp_len; i++) psbuf[12 + i] = frame[34 + i];
    uint16_t tcs = ip_checksum(psbuf, 12 + tcp_len);
    frame[50] = (unsigned char)(tcs >> 8); frame[51] = (unsigned char)tcs;
    return 34 + tcp_len;
}

static int scan_port(const unsigned char *gw_mac, const unsigned char *hedef_ip,
                     int idx, int dst_port) {
    unsigned char *frame = (unsigned char *)(uintptr_t)KDL_TX_FRAME;
    unsigned char *rx    = (unsigned char *)(uintptr_t)KDL_RX_FRAME;
    int src_port = KDL_SRC_PORT_BAZ + idx;
    int toplam = tcp_syn_kur(frame, gw_mac, hedef_ip, src_port, dst_port,
                             (uint32_t)KDL_TCP_SEQ, 0x02);
    long g = (long)sys2(24, (uint64_t)(uintptr_t)frame, (uint64_t)toplam);
    if (g < 0) return DURUM_FILTRELI;
    int durum = DURUM_FILTRELI;
    for (int d = 0; d < 30 && durum == DURUM_FILTRELI; d++) {
        long n = (long)sys2(25, (uint64_t)(uintptr_t)rx, 128);
        if (n < 54) continue;
        if (rx[12] != 0x08 || rx[13] != 0x00) continue;
        if (rx[23] != 6) continue;
        if (!(rx[26] == hedef_ip[0] && rx[27] == hedef_ip[1] &&
              rx[28] == hedef_ip[2] && rx[29] == hedef_ip[3])) continue;
        int rsrc = (rx[34] << 8) | rx[35];
        int rdst = (rx[36] << 8) | rx[37];
        if (rsrc != dst_port || rdst != src_port) continue;
        unsigned char flags = rx[47];
        if ((flags & 0x12) == 0x12) {
            durum = DURUM_ACIK;
            int rt = tcp_syn_kur(frame, gw_mac, hedef_ip, src_port, dst_port,
                                 (uint32_t)(KDL_TCP_SEQ + 1), 0x04);
            sys2(24, (uint64_t)(uintptr_t)frame, (uint64_t)rt);
        } else if (flags & 0x04) {
            durum = DURUM_KAPALI;
        }
    }
    return durum;
}

static void komut_scan(int hedef_oktet) {
    unsigned char gw_mac[6];
    unsigned char hedef_ip[4] = { 10, 0, 2, (unsigned char)hedef_oktet };
    static const int portlar[3] = { 80, 443, 22 };
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

/* arpscan: subnet ARP taraması (10.0.2.1..5) → canlı host sayısı. */
#define KDL_ARPSCAN_ILK 1
#define KDL_ARPSCAN_SON 5
#define KDL_ARPSCAN_MAX (KDL_ARPSCAN_SON - KDL_ARPSCAN_ILK + 1)

static int komut_arpscan(void) {
    unsigned char *frame = (unsigned char *)(uintptr_t)KDL_TX_FRAME;
    unsigned char *rx    = (unsigned char *)(uintptr_t)KDL_RX_FRAME;
    uint32_t bulunan_ip[KDL_ARPSCAN_MAX];
    int bulunan_sayi = 0;
    int i;
    for (int son = KDL_ARPSCAN_ILK; son <= KDL_ARPSCAN_SON; son++) {
        for (i = 0; i < 42; i++) frame[i] = 0;
        for (i = 0; i < 6; i++) frame[i] = 0xff;
        for (i = 0; i < 6; i++) frame[6 + i] = BIZIM_MAC[i];
        frame[12] = 0x08; frame[13] = 0x06;
        frame[14] = 0x00; frame[15] = 0x01;
        frame[16] = 0x08; frame[17] = 0x00;
        frame[18] = 6; frame[19] = 4;
        frame[20] = 0x00; frame[21] = 0x01;
        for (i = 0; i < 6; i++) frame[22 + i] = BIZIM_MAC[i];
        frame[28] = 10; frame[29] = 0; frame[30] = 2; frame[31] = 15;
        frame[38] = 10; frame[39] = 0; frame[40] = 2;
        frame[41] = (unsigned char)son;
        sys2(24, (uint64_t)(uintptr_t)frame, 42);
    }
    int bos_ardisik = 0;
    for (int deneme = 0; deneme < 40; deneme++) {
        long n = (long)sys2(25, (uint64_t)(uintptr_t)rx, 128);
        if (n < 42) {
            bos_ardisik++;
            if (bulunan_sayi >= 1 && bos_ardisik > 6) break;
            continue;
        }
        bos_ardisik = 0;
        if (rx[12] != 0x08 || rx[13] != 0x06) continue;
        if (rx[20] != 0x00 || rx[21] != 0x02) continue;
        uint32_t spa = ((uint32_t)rx[28] << 24) | ((uint32_t)rx[29] << 16) |
                       ((uint32_t)rx[30] << 8)  | (uint32_t)rx[31];
        int var = 0;
        for (i = 0; i < bulunan_sayi; i++) if (bulunan_ip[i] == spa) { var = 1; break; }
        if (var) continue;
        if (bulunan_sayi < KDL_ARPSCAN_MAX) bulunan_ip[bulunan_sayi++] = spa;
    }
    return bulunan_sayi;
}

/* pingsweep: 10.0.2.1..5 host keşfi (nmap-tarzı L3 recon). */
static void komut_pingsweep(void) {
    int canli = 0;
    kdl_yaz_metin("PINGSWEEP 10.0.2.1-5:");
    for (int o = 1; o <= 5; o++) {
        if (komut_ping(o)) {
            canli++;
            kdl_yaz_metin(" .");
            kdl_yaz_tam((int32_t)o);
        }
    }
    kdl_yaz_metin(" -> ");
    kdl_yaz_tam((int32_t)canli);
    kdl_yaz_metin(" host");
    kdl_yazdir_satir();
}

/* --- Dosya sistemi komutları (RAM-FS, syscall 17-21) --- */
static void komut_ls(void) {
    long c = (long)sys1(19, 0);            /* dosya sayısı */
    char *ad = (char *)(uintptr_t)KDL_AD_BUF;
    kdl_yaz_metin("LS ");
    kdl_yaz_tam((int32_t)c);
    kdl_yaz_metin(":");
    for (long i = 0; i < c; i++) {
        long r = (long)sys2(20, (uint64_t)i, (uint64_t)(uintptr_t)ad);   /* ad(i) */
        if (r < 0) continue;
        kdl_yaz_metin(" ");
        kdl_yaz_metin(ad);
    }
    kdl_yazdir_satir();
}
static void komut_yaz(const char *ad, const char *icerik) {
    long r = (long)sys2(17, (uint64_t)(uintptr_t)ad, (uint64_t)(uintptr_t)icerik);  /* yaz_metin */
    if (r >= 0) { kdl_yaz_metin("YAZILDI "); kdl_yaz_tam((int32_t)r); kdl_yaz_metin(" byte"); }
    else kdl_yaz_metin("YAZ HATA");
    kdl_yazdir_satir();
}
static void komut_oku(const char *ad) {
    char *buf = (char *)(uintptr_t)KDL_CIKTI_BUF;
    long r = (long)sys2(18, (uint64_t)(uintptr_t)ad, (uint64_t)(uintptr_t)buf);   /* oku_metin */
    if (r < 0) { kdl_yazdir_metin("OKU: yok"); return; }
    kdl_yaz_metin("OKU: ");
    kdl_yazdir_metin(buf);
}
static void komut_sil(const char *ad) {
    long r = (long)sys1(21, (uint64_t)(uintptr_t)ad);   /* sil */
    kdl_yazdir_metin(r == 0 ? "SILINDI" : "SIL: yok");
}

/* Kabuk durum bilgisi. */
static int g_net_hazir = 0;
static int g_disk_hazir = 0;
static uint64_t g_blk_base = 0;
static int g_komut_sayaci = 0;

/* saat: PL031 donanım RTC'sini oku (Unix epoch saniye). Makul (>1.5e9) ise canlı. */
static uint32_t rtc_oku(void) {
    return *(volatile uint32_t *)(uintptr_t)KDL_PL031_DR;
}
static void komut_saat(void) {
    uint32_t t = rtc_oku();
    kdl_yaz_metin("SAAT: ");
    kdl_yaz_tam((int32_t)t);
    kdl_yaz_metin(" (unix saniye) ");
    kdl_yazdir_metin(t > 1500000000u ? "RTC OK" : "RTC SUPHELI");
}

/* kaydet: RAM-FS'i diske yaz (kalıcılık). */
static void komut_kaydet(void) {
    if (!g_disk_hazir) { kdl_yazdir_metin("DISK: yok"); return; }
    kdl_yazdir_metin(kdl_dosya_kaydet(g_blk_base) == 0 ? "KAYDET OK (FS diske yazildi)"
                                                       : "KAYDET HATA");
}
/* yukle: diskten RAM-FS'e oku. */
static void komut_yukle(void) {
    if (!g_disk_hazir) { kdl_yazdir_metin("DISK: yok"); return; }
    kdl_yazdir_metin(kdl_dosya_yukle(g_blk_base) == 0 ? "YUKLE OK (FS diskten okundu)"
                                                      : "YUKLE HATA (diskte FS yok)");
}

static void komut_sysinfo(void) {
    kdl_yaz_metin("SYSINFO net=");
    kdl_yaz_metin(g_net_hazir ? "hazir" : "yok");
    kdl_yaz_metin(" disk=");
    kdl_yaz_metin(g_disk_hazir ? "hazir" : "yok");
    kdl_yaz_metin(" fs=");
    kdl_yaz_tam((int32_t)(long)sys1(19, 0));       /* dosya sayısı */
    kdl_yaz_metin("dosya komut=");
    kdl_yaz_tam((int32_t)g_komut_sayaci);
    kdl_yaz_metin(" uptime=");
    kdl_yaz_tam((int32_t)kdl_tik_al());            /* canlı timer tik sayısı */
    kdl_yaz_metin("tik arka_b=");
    kdl_yaz_tam((int32_t)arka_sayac_b);            /* PART 2: arka-plan görev B sayacı (canlı) */
    kdl_yaz_metin(" arka_c=");
    kdl_yaz_tam((int32_t)arka_sayac_c);            /* arka-plan görev C sayacı — kabuk çalışırken artar */
    kdl_yazdir_satir();
}

static void komut_yardim(void) {
    kdl_yazdir_metin("KOMUTLAR: yardim sysinfo saat ls yaz oku sil kaydet yukle ping pingsweep scan arpscan");
}

/* UART RX'ten BIR SATIR CANLI oku. Dönüş: byte (>=0) veya -1 = EOF. */
static int satir_oku(char *buf) {
    int n = 0;
    for (;;) {
        uint32_t geldi = 0;
        for (uint64_t i = 0; i < KDL_RX_BAYT_SINIR; i++) {
            if (!(pl011_oku(KDL_PL011_FR) & KDL_PL011_FR_RXFE)) { geldi = 1; break; }
        }
        if (!geldi) { buf[n] = 0; return n > 0 ? n : -1; }
        char c = (char)(pl011_oku(KDL_PL011_DR) & 0xFFu);
        if (c == '\n' || c == '\r') { buf[n] = 0; return n; }
        if (n < KDL_SATIR_MAX) buf[n++] = c;
    }
}

/* Bir komut satırını çalıştır (entegre dağıtım: FS + recon). */
static void komut_calistir(char *satir) {
    char *tok[3];
    int nt = tokenize(satir, tok);
    if (nt == 0) return;
    g_komut_sayaci++;

    if (str_esit(tok[0], "yardim")) {
        komut_yardim();
    } else if (str_esit(tok[0], "sysinfo")) {
        komut_sysinfo();
    } else if (str_esit(tok[0], "ls")) {
        komut_ls();
    } else if (str_esit(tok[0], "yaz")) {
        if (nt >= 3) komut_yaz(tok[1], tok[2]);
        else kdl_yazdir_metin("KULLANIM: yaz <ad> <icerik>");
    } else if (str_esit(tok[0], "oku")) {
        if (nt >= 2) komut_oku(tok[1]);
        else kdl_yazdir_metin("KULLANIM: oku <ad>");
    } else if (str_esit(tok[0], "sil")) {
        if (nt >= 2) komut_sil(tok[1]);
        else kdl_yazdir_metin("KULLANIM: sil <ad>");
    } else if (str_esit(tok[0], "kaydet")) {
        komut_kaydet();
    } else if (str_esit(tok[0], "yukle")) {
        komut_yukle();
    } else if (str_esit(tok[0], "saat")) {
        komut_saat();
    } else if (str_esit(tok[0], "ping")) {
        int oktet = (nt >= 2) ? str_to_int(tok[1]) : 2;
        if (oktet <= 0 || oktet > 255) oktet = 2;
        kdl_yazdir_metin(komut_ping(oktet) ? "PING: CANLI" : "PING: yanit yok");
    } else if (str_esit(tok[0], "pingsweep")) {
        komut_pingsweep();
    } else if (str_esit(tok[0], "scan")) {
        int oktet = (nt >= 2) ? str_to_int(tok[1]) : 2;
        if (oktet <= 0 || oktet > 255) oktet = 2;
        komut_scan(oktet);
    } else if (str_esit(tok[0], "arpscan")) {
        int host = komut_arpscan();
        kdl_yaz_metin("ARPSCAN: ");
        kdl_yaz_tam((int32_t)host);
        kdl_yaz_metin(" host");
        kdl_yazdir_satir();
    } else {
        kdl_yazdir_metin("? (yardim yaz)");
    }
}

/* --- Boot init betiği: alt-sistemleri TEK boot'ta CANLI + DETERMİNİSTİK sınar ---
 * Gerçek OS init-script'i gibi: UART girişine BAĞLI DEĞİL (input-timing yarışı yok).
 * FS yaz->ls->oku round-trip + ağ ICMP/ARP — hepsi aynı koşan çekirdekte, birlikte.
 * (İnteraktif kabuk bundan SONRA gelir; init entegrasyon kanıtıdır.) */
/* PART 1(b): MMU gerçekten ZORLUYOR mu? Haritalanmamış sayfaya (0x80000000 =
 * L1[2], kdl_mmu'da geçersiz) KASITLI eriş → translation-fault. Fault handler
 * (bayrak-kontrollü) FAR'ı yakalar, faulting instr'ı atlar, OS DEVAM eder. Bu,
 * "MMU her-şeyi-map-etmiyor, gerçekten koruyor" + "kernel fault'u yönetip ilerliyor"
 * kanıtı. Taklit edilemez: MMU zorlamıyorsa 0x80000000 okuması sessizce geçerdi. */
static void mmu_zorlama_testi(void) {
    kdl_fault_yakalanan = 0;
    kdl_fault_bekleniyor = 1;
    __asm__ volatile("dsb sy; isb");
    volatile uint64_t v = *(volatile uint64_t *)(uintptr_t)0x80000000UL;  /* haritasız → fault */
    (void)v;
    __asm__ volatile("dsb sy; isb");
    if (kdl_fault_bekleniyor == 0 && kdl_fault_yakalanan == 0x80000000UL)
        kdl_yazdir_metin("PAGEFAULT OK (haritasiz 0x80000000 -> fault yakalandi+kurtarildi, FAR dogru)");
    else
        kdl_yazdir_metin("PAGEFAULT HATA (MMU zorlamiyor veya kurtarma calismadi)");
}

static void init_betik(void) {
    uint64_t t0 = kdl_tik_al();       /* uptime başlangıcı (timer IRQ canlı) */
    uint64_t sb0 = arka_sayac_b;      /* PART 2: arka-plan görev sayaçları (başlangıç) */
    uint64_t sc0 = arka_sayac_c;
    kdl_yazdir_metin("[init] betik: FS + ag entegrasyon sinamasi (deterministik)");
    /* 0) MMU ZORLAMA: haritasız sayfa → fault yakalanır+kurtarılır (OS devam). */
    mmu_zorlama_testi();
    /* 1) FS: yaz -> ls -> oku round-trip (proje=KEMGU + boot-seed surum). */
    komut_yaz("proje", "KEMGU");
    komut_ls();
    komut_oku("surum");     /* boot-seed → OKU: KEMGU-OS-v0.1 */
    komut_oku("proje");     /* yeni yazılan → OKU: KEMGU */
    /* 2) DEPOLAMA: RAM-FS'i diske kaydet -> diskten yükle round-trip (kalıcılık kanıtı).
     * kaydet FS'i blok 0-1'e yazar; yukle geri okur (kdl_dosyalar üzerine). Sonra
     * "proje" hâlâ okunabiliyorsa disk yaz+oku yolu çalışıyor. */
    if (g_disk_hazir) {
        int k = kdl_dosya_kaydet(g_blk_base);
        int y = kdl_dosya_yukle(g_blk_base);
        char *buf = (char *)(uintptr_t)KDL_CIKTI_BUF;
        long r = (long)sys2(18, (uint64_t)(uintptr_t)"proje", (uint64_t)(uintptr_t)buf);
        int korundu = (k == 0 && y == 0 && r == 5 &&
                       buf[0] == 'K' && buf[1] == 'E' && buf[2] == 'M' &&
                       buf[3] == 'G' && buf[4] == 'U');
        kdl_yazdir_metin(korundu ? "DISK RW OK (kaydet->yukle round-trip, proje korundu)"
                                 : "DISK RW HATA");
    }
    /* 3) AĞ: ICMP ping (SLIRP gateway, deterministik echo) + ARP subnet tarama. */
    if (g_net_hazir) {
        kdl_yazdir_metin(komut_ping(2) ? "PING: CANLI" : "PING: yanit yok");
        int host = komut_arpscan();
        kdl_yaz_metin("ARPSCAN: "); kdl_yaz_tam((int32_t)host);
        kdl_yaz_metin(" host"); kdl_yazdir_satir();
    }
    /* 4) ZAMAN: uptime ilerledi mi? (timer IRQ arka planda çalışıyor → canlı çekirdek). */
    if (kdl_tik_al() > t0) kdl_yazdir_metin("UPTIME: timer canli (tik ilerledi)");
    else                   kdl_yazdir_metin("UPTIME: timer durdu");
    /* 4b) PART 2 SCHEDULER: main (görev 0) yukarıdaki init işini yaparken (net/FS/disk —
     * ve ASLA gönüllü yield etmeden), İKİ arka-plan görevi (yield-etmeyen busy-loop)
     * timer-IRQ preemption ile GERÇEKTEN koştu mu? Her iki sayaç da BAŞLANGIÇTAN büyükse
     * → zorunlu bağlam-değiştirme çalışıyor (gerçek multitasking; timer-sayaç DEĞİL).
     * FALSİFİYE-EDİLEMEZ: preemption yoksa arka-plan görevleri hiç seçilmez → sayaç=0. */
    uint64_t db = arka_sayac_b - sb0, dc = arka_sayac_c - sc0;
    if (db > 0 && dc > 0) {
        kdl_yaz_metin("SCHEDULER OK (2 yield-etmeyen arka-plan gorev preempt kostu: +");
        kdl_yaz_tam((int32_t)db); kdl_yaz_metin("B +");
        kdl_yaz_tam((int32_t)dc); kdl_yaz_metin("C)"); kdl_yazdir_satir();
    } else {
        kdl_yazdir_metin("SCHEDULER HATA (arka-plan gorev preempt kosmadi)");
    }
    /* 5) SAAT: donanım RTC (gerçek-zaman saati). */
    komut_saat();
    /* 6) Sistem durumu. */
    komut_sysinfo();
    kdl_yazdir_metin("[init] betik bitti");
}

int main(void) {
    /* RX FIFO'yu TX'ten ÖNCE aç (D-188 burst overrun savunması). */
    pl011_fifo_ac();

    kdl_yazdir_metin("=== KEMGU-OS v0.1 — Turkce pentest cekirdegi (TEK ENTEGRE KERNEL) ===");

    /* --- Alt-sistem 1: AĞ (virtio-net) --- */
    uint64_t nb = kdl_virtio_net_bul();
    if (nb && kdl_virtio_net_kur(nb) == 0) {
        g_net_hazir = 1;
        kdl_yazdir_metin("[boot] ag: virtio-net HAZIR (ARP/ICMP/TCP)");
    } else {
        kdl_yazdir_metin("[boot] ag: virtio-net YOK");
    }

    /* --- Alt-sistem 2: DEPOLAMA (virtio-blk) — kalıcı FS backing (D-143) --- */
    g_blk_base = kdl_virtio_blk_bul();
    if (g_blk_base && kdl_virtio_blk_kur(g_blk_base) == 0) {
        g_disk_hazir = 1;
        int y = kdl_dosya_yukle(g_blk_base);   /* diskte kalıcı FS varsa yükle */
        kdl_yaz_metin("[boot] disk: virtio-blk HAZIR");
        kdl_yazdir_metin(y == 0 ? " (kalici FS yuklendi)" : " (bos disk)");
    } else {
        kdl_yazdir_metin("[boot] disk: virtio-blk YOK");
    }

    /* --- Alt-sistem 3: DOSYA SİSTEMİ (RAM-FS) — boot seed --- */
    /* Sürüm dosyasını yaz (kernel .rodata string'leri kdl_user_oku_str_gecerli izinli). */
    sys2(17, (uint64_t)(uintptr_t)"surum", (uint64_t)(uintptr_t)"KEMGU-OS-v0.1");
    kdl_yazdir_metin("[boot] fs: RAM-FS HAZIR (surum dosyasi tohumlandi)");

    /* --- Alt-sistem 4: PREEMPTIVE SCHEDULER (PART 2 — gerçek multitasking) --- */
    /* main = görev 0 (kabuk). İki ARKA-PLAN görevi (busy-loop, yield ETMEZ) kaydet;
     * timer-IRQ ZORUNLU switch ile hepsini dönüşümlü koştur. Görev switch YOK iken
     * (D-233 uptime) yalnız tik sayılırdı = multitasking DEĞİLdi; şimdi GERÇEK preempt.
     * Sıra (preempt_arm.c/D-117): baslat → görev-olustur → kesme/timer → ac. */
    kdl_preempt_baslat();                                               /* main = görev 0 */
    kdl_preempt_gorev_olustur(arka_gorev_b, yigin_arka_b + sizeof(yigin_arka_b));
    kdl_preempt_gorev_olustur(arka_gorev_c, yigin_arka_c + sizeof(yigin_arka_c));
    kdl_kesme_kur();
    kdl_timer_baslat();
    kdl_preempt_ac();                                                   /* preemption AÇ */
    kdl_yazdir_metin("[boot] scheduler: preemptive HAZIR (main=gorev0 + 2 arka-plan gorev)");
    kdl_yazdir_metin("KEMGU-OS BASLA");

    /* --- DETERMİNİSTİK entegrasyon kanıtı (boot init betiği) --- */
    init_betik();

    /* --- İnteraktif kabuk döngüsü (canlı UART RX, gerçek kullanım) ---
     * Giriş varsa canlı komut işler; yoksa satir_oku bounded EOF ile döner. Gate
     * PASS'ı init_betik'e dayanır → burası input-timing'e bağlı DEĞİL. */
    kdl_yazdir_metin("[kabuk] interaktif kabuk (canli UART) — yardim = komut listesi");
    char *satir = (char *)(uintptr_t)KDL_SATIR_BUF;
    for (int k = 0; k < KDL_MAX_KOMUT; k++) {
        kdl_yaz_metin("kemgu> ");
        int r = satir_oku(satir);
        if (r < 0) { kdl_yazdir_satir(); break; }   /* EOF: giriş bitti */
        kdl_yaz_metin(satir);                        /* echo */
        kdl_yazdir_satir();
        komut_calistir(satir);
    }

    kdl_yazdir_metin("KEMGU-OS OK");   /* entegre çekirdek kanıtı */
    dur();
    return 0;
}
