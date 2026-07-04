/*
 * KEMGU-OS PART 3(d) — INTERAKTİF KABUK, USERSPACE PROCESS olarak (EL0).
 * ====================================================================
 *
 * Bu, kemgu_os_arm.c'nin EL1 kabuk-döngüsünün YERİNE geçer: kabuk artık bir EL0
 * userspace PROCESS'tir (kernel-içi fonksiyon DEĞİL). Çekirdek (EL1) boot + alt-sistem
 * kurar, sonra bu programı EL0 GÖREV olarak yükler → kabuk EL0'da koşar. Kabuk HER ŞEYİ
 * syscall ile yapar (EL0 direkt MMIO/kernel-fn erişemez):
 *   - UART satır oku → sys(26) read_satir (kernel PL011 RX yapar)
 *   - çıktı → sys(5/6/7) yaz/yaz_sayi/satir
 *   - dosya → sys(17..21) FS
 *   - saat → sys(27) RTC
 * Ayrı derlenir (BM_A64_EL0=GPR-only) → NEON sabit-havuzu kernel .rodata'ya düşmez (D-235).
 * Boot sayfa-tablosu altında koşar (0x42000000 .user AP=01; TTBR-swap YOK).
 *
 * PROOF(d): sys(2) → kernel "EL0 SYSCALL kaynak-EL=0x0" → kabuk GERÇEKTEN EL0'da (donanım
 * SPSR). Deterministik gömülü komut dizisi (yaz/ls/oku/saat) EL0'dan syscall'la işlenir +
 * canlı UART girişi de işlenir → "SHELL EL0 OK". Kabuk = userspace process.
 */
#include <stdint.h>

__attribute__((always_inline)) static inline unsigned long u_sys(unsigned long num, unsigned long a0) {
    register unsigned long x8 __asm__("x8") = num;
    register unsigned long x0 __asm__("x0") = a0;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8) : "memory");
    return x0;
}
__attribute__((always_inline)) static inline unsigned long u_sys2(unsigned long num, unsigned long a0, unsigned long a1) {
    register unsigned long x8 __asm__("x8") = num;
    register unsigned long x0 __asm__("x0") = a0;
    register unsigned long x1 __asm__("x1") = a1;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8), "r"(x1) : "memory");
    return x0;
}

/* Komut adları — .user_data (AP=01, EL0 OKUR). Normal .rodata literalleri EL0'a kapalı
 * (D-135/D-235). str_esit bunları EL0'da karşılaştırır. */
__attribute__((section(".user_data"))) static char CMD_YARDIM[] = "yardim";
__attribute__((section(".user_data"))) static char CMD_ECHO[]   = "echo";
__attribute__((section(".user_data"))) static char CMD_LS[]     = "ls";
__attribute__((section(".user_data"))) static char CMD_YAZ[]    = "yaz";
__attribute__((section(".user_data"))) static char CMD_OKU[]    = "oku";
__attribute__((section(".user_data"))) static char CMD_SIL[]    = "sil";
__attribute__((section(".user_data"))) static char CMD_SAAT[]   = "saat";
__attribute__((section(".user_data"))) static char CMD_CIK[]    = "cik";
/* Gömülü deterministik komut dizisi (gate kanıtı — canlı girişe bağlı DEĞİL). */
__attribute__((section(".user_data"))) static char INIT_BETIK[] =
    "yaz nesne KABUK\noku nesne\nls\nsaat\nping 2\narpscan\ncalistir hesap\ncoklu\n";
/* Kabuk tamponları — .user_data (0x42000000 sayfası, EL0-erişimli + FS-validator izinli). */
__attribute__((section(".user_data"))) static char satir_buf[256];
__attribute__((section(".user_data"))) static char cikti_buf[128];
/* Net recon komut adları + TX/RX çerçeve tamponları (.user_data, EL0-erişimli +
 * net-syscall validator [0x42000000,0x42400000) izinli). */
__attribute__((section(".user_data"))) static char CMD_PING[]    = "ping";
__attribute__((section(".user_data"))) static char CMD_ARPSCAN[] = "arpscan";
__attribute__((section(".user_data"))) static char CMD_SCAN[]    = "scan";
__attribute__((section(".user_data"))) static char CMD_CALISTIR[] = "calistir";
__attribute__((section(".user_data"))) static char CMD_COKLU[]   = "coklu";
__attribute__((section(".user_data"))) static char PROG_HESAP[]  = "hesap";
__attribute__((section(".user_data"))) static char PROG_SELAM[]  = "selam";
__attribute__((section(".user_data"))) static char FILE_SONUC[]  = "sonuc";
__attribute__((section(".user_data"))) static unsigned char tx_frame[256];
__attribute__((section(".user_data"))) static unsigned char rx_frame[256];
__attribute__((section(".user_data"))) static unsigned char BIZIM_MAC[6]   = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
__attribute__((section(".user_data"))) static unsigned char SLIRP_GW_MAC[6] = { 0x52, 0x55, 0x0a, 0x00, 0x02, 0x02 };
__attribute__((section(".user_data"))) static unsigned char BIZIM_IP[4]     = { 10, 0, 2, 15 };

__attribute__((section(".user"), noinline))
static int str_esit(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}
__attribute__((section(".user"), noinline))
static int tokenize(char *satir, char **tok) {
    int nt = 0; char *p = satir;
    while (*p && nt < 3) {
        while (*p == ' ') p++;
        if (!*p) break;
        tok[nt++] = p;
        while (*p && *p != ' ') p++;
        if (*p) *p++ = 0;
    }
    return nt;
}

/* --- NET RECON (EL0'DAN, sys2(24)=net_gonder / sys2(25)=net_al ile) ---
 * Tüm çerçeve-kurma EL0'da, .user_data tamponlarında (net-syscall validator izinli).
 * userspace_net_arm.c/D-176 deseni: EL0 süreç virtio-net'e DOKUNMADAN syscall ile ağ yapar. */
__attribute__((section(".user"), noinline))
static int u_str_to_int(const char *s) {
    int v = 0;
    while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); s++; }
    return v;
}
__attribute__((section(".user"), noinline))
static unsigned int u_ip_checksum(const unsigned char *veri, int uzun) {
    unsigned int t = 0;
    for (int i = 0; i + 1 < uzun; i += 2) t += ((unsigned int)veri[i] << 8) | veri[i + 1];
    if (uzun & 1) t += (unsigned int)veri[uzun - 1] << 8;
    while (t >> 16) t = (t & 0xffff) + (t >> 16);
    return (~t) & 0xffff;
}
/* Gateway 10.0.2.<oktet> MAC'ini ARP ile çöz (çözülemezse SLIRP sabit MAC). */
__attribute__((section(".user"), noinline))
static void u_arp_coz(unsigned char *mac_out, int oktet) {
    int i;
    for (i = 0; i < 42; i++) tx_frame[i] = 0;
    for (i = 0; i < 6; i++) tx_frame[i] = 0xff;
    for (i = 0; i < 6; i++) tx_frame[6 + i] = BIZIM_MAC[i];
    tx_frame[12] = 0x08; tx_frame[13] = 0x06; tx_frame[14] = 0x00; tx_frame[15] = 0x01;
    tx_frame[16] = 0x08; tx_frame[17] = 0x00; tx_frame[18] = 6; tx_frame[19] = 4;
    tx_frame[20] = 0x00; tx_frame[21] = 0x01;
    for (i = 0; i < 6; i++) tx_frame[22 + i] = BIZIM_MAC[i];
    tx_frame[28] = 10; tx_frame[29] = 0; tx_frame[30] = 2; tx_frame[31] = 15;
    tx_frame[38] = 10; tx_frame[39] = 0; tx_frame[40] = 2; tx_frame[41] = (unsigned char)oktet;
    u_sys2(24, (unsigned long)(uintptr_t)tx_frame, 42);
    for (i = 0; i < 6; i++) mac_out[i] = SLIRP_GW_MAC[i];   /* fallback */
    for (int d = 0; d < 200; d++) {
        long n = (long)u_sys2(25, (unsigned long)(uintptr_t)rx_frame, 128);
        if (n < 42) continue;
        if (rx_frame[12] != 0x08 || rx_frame[13] != 0x06) continue;
        if (rx_frame[20] != 0x00 || rx_frame[21] != 0x02) continue;
        if (rx_frame[28] == 10 && rx_frame[29] == 0 && rx_frame[30] == 2 &&
            rx_frame[31] == (unsigned char)oktet) {
            for (i = 0; i < 6; i++) mac_out[i] = rx_frame[22 + i];
            return;
        }
    }
}
/* ping <oktet>: ICMP echo → 1=canlı, 0=yanıt yok. */
__attribute__((section(".user"), noinline))
static int u_ping(int oktet) {
    unsigned char gw[6]; int i;
    u_arp_coz(gw, oktet);
    unsigned char icmp[13];
    for (i = 0; i < 13; i++) icmp[i] = 0;
    icmp[0] = 8; icmp[4] = 0xBE; icmp[5] = 0xEF; icmp[7] = 0x01;
    icmp[8] = 'K'; icmp[9] = 'E'; icmp[10] = 'M'; icmp[11] = 'G'; icmp[12] = 'U';
    unsigned int ics = u_ip_checksum(icmp, 13);
    icmp[2] = (unsigned char)(ics >> 8); icmp[3] = (unsigned char)ics;
    for (i = 0; i < 128; i++) tx_frame[i] = 0;
    for (i = 0; i < 6; i++) tx_frame[i] = gw[i];
    for (i = 0; i < 6; i++) tx_frame[6 + i] = BIZIM_MAC[i];
    tx_frame[12] = 0x08; tx_frame[13] = 0x00;
    int ip_total = 20 + 13;
    tx_frame[14] = 0x45; tx_frame[16] = (unsigned char)(ip_total >> 8); tx_frame[17] = (unsigned char)ip_total;
    tx_frame[20] = 0x40; tx_frame[22] = 64; tx_frame[23] = 1;
    tx_frame[26] = 10; tx_frame[27] = 0; tx_frame[28] = 2; tx_frame[29] = 15;
    tx_frame[30] = 10; tx_frame[31] = 0; tx_frame[32] = 2; tx_frame[33] = (unsigned char)oktet;
    unsigned int ihs = u_ip_checksum(&tx_frame[14], 20);
    tx_frame[24] = (unsigned char)(ihs >> 8); tx_frame[25] = (unsigned char)ihs;
    for (i = 0; i < 13; i++) tx_frame[34 + i] = icmp[i];
    if ((long)u_sys2(24, (unsigned long)(uintptr_t)tx_frame, 34 + 13) < 0) return 0;
    for (int d = 0; d < 400; d++) {
        long n = (long)u_sys2(25, (unsigned long)(uintptr_t)rx_frame, 128);
        if (n < 42) continue;
        if (rx_frame[12] != 0x08 || rx_frame[13] != 0x00 || rx_frame[23] != 1) continue;
        if (!(rx_frame[26] == 10 && rx_frame[27] == 0 && rx_frame[28] == 2 &&
              rx_frame[29] == (unsigned char)oktet)) continue;
        int io = 14 + (rx_frame[14] & 0x0f) * 4;
        if (n < io + 8) continue;
        if (rx_frame[io] == 0 && rx_frame[io + 1] == 0) return 1;   /* echo reply */
    }
    return 0;
}
/* arpscan: 10.0.2.1..5 ARP tara → canlı host sayısı. */
__attribute__((section(".user"), noinline))
static int u_arpscan(void) {
    unsigned int bulunan[5]; int say = 0, i;
    for (int son = 1; son <= 5; son++) {
        for (i = 0; i < 42; i++) tx_frame[i] = 0;
        for (i = 0; i < 6; i++) tx_frame[i] = 0xff;
        for (i = 0; i < 6; i++) tx_frame[6 + i] = BIZIM_MAC[i];
        tx_frame[12] = 0x08; tx_frame[13] = 0x06; tx_frame[14] = 0x00; tx_frame[15] = 0x01;
        tx_frame[16] = 0x08; tx_frame[17] = 0x00; tx_frame[18] = 6; tx_frame[19] = 4;
        tx_frame[20] = 0x00; tx_frame[21] = 0x01;
        for (i = 0; i < 6; i++) tx_frame[22 + i] = BIZIM_MAC[i];
        tx_frame[28] = 10; tx_frame[29] = 0; tx_frame[30] = 2; tx_frame[31] = 15;
        tx_frame[38] = 10; tx_frame[39] = 0; tx_frame[40] = 2; tx_frame[41] = (unsigned char)son;
        u_sys2(24, (unsigned long)(uintptr_t)tx_frame, 42);
    }
    int bos = 0;
    for (int d = 0; d < 40; d++) {
        long n = (long)u_sys2(25, (unsigned long)(uintptr_t)rx_frame, 128);
        if (n < 42) { bos++; if (say >= 1 && bos > 6) break; continue; }
        bos = 0;
        if (rx_frame[12] != 0x08 || rx_frame[13] != 0x06) continue;
        if (rx_frame[20] != 0x00 || rx_frame[21] != 0x02) continue;
        unsigned int spa = ((unsigned int)rx_frame[28] << 24) | ((unsigned int)rx_frame[29] << 16) |
                           ((unsigned int)rx_frame[30] << 8) | rx_frame[31];
        int var = 0;
        for (i = 0; i < say; i++) if (bulunan[i] == spa) { var = 1; break; }
        if (!var && say < 5) bulunan[say++] = spa;
    }
    return say;
}
/* TCP SYN segmenti (veri yok) inşa → tx_frame; toplam uzunluk döner. */
__attribute__((section(".user"), noinline))
static int u_tcp_syn(const unsigned char *gw, const unsigned char *dip, int sport, int dport,
                     unsigned int seq, unsigned char flags) {
    int i;
    for (i = 0; i < 128; i++) tx_frame[i] = 0;
    for (i = 0; i < 6; i++) tx_frame[i] = gw[i];
    for (i = 0; i < 6; i++) tx_frame[6 + i] = BIZIM_MAC[i];
    tx_frame[12] = 0x08; tx_frame[13] = 0x00;
    tx_frame[14] = 0x45; tx_frame[16] = 0; tx_frame[17] = 40;
    tx_frame[20] = 0x40; tx_frame[22] = 64; tx_frame[23] = 6;
    for (i = 0; i < 4; i++) tx_frame[26 + i] = BIZIM_IP[i];
    for (i = 0; i < 4; i++) tx_frame[30 + i] = dip[i];
    unsigned int ipcs = u_ip_checksum(&tx_frame[14], 20);
    tx_frame[24] = (unsigned char)(ipcs >> 8); tx_frame[25] = (unsigned char)ipcs;
    tx_frame[34] = (unsigned char)(sport >> 8); tx_frame[35] = (unsigned char)sport;
    tx_frame[36] = (unsigned char)(dport >> 8); tx_frame[37] = (unsigned char)dport;
    tx_frame[38] = (unsigned char)(seq >> 24); tx_frame[39] = (unsigned char)(seq >> 16);
    tx_frame[40] = (unsigned char)(seq >> 8); tx_frame[41] = (unsigned char)seq;
    tx_frame[46] = 0x50; tx_frame[47] = flags; tx_frame[48] = 0x20;
    unsigned char ps[32];
    for (i = 0; i < 32; i++) ps[i] = 0;
    for (i = 0; i < 4; i++) ps[i] = BIZIM_IP[i];
    for (i = 0; i < 4; i++) ps[4 + i] = dip[i];
    ps[9] = 6; ps[11] = 20;
    for (i = 0; i < 20; i++) ps[12 + i] = tx_frame[34 + i];
    unsigned int tcs = u_ip_checksum(ps, 32);
    tx_frame[50] = (unsigned char)(tcs >> 8); tx_frame[51] = (unsigned char)tcs;
    return 54;
}
/* scan <oktet>: 10.0.2.<oktet>:80/443/22 TCP SYN → ACIK/KAPALI/FILTRELI. */
__attribute__((section(".user"), noinline))
static void u_scan(int oktet) {
    unsigned char gw[6]; unsigned char dip[4] = { 10, 0, 2, (unsigned char)oktet };
    static const int portlar[3] = { 80, 443, 22 };
    u_arp_coz(gw, oktet);
    u_sys(5, (unsigned long)(uintptr_t)"SCAN "); u_sys(6, (unsigned long)oktet); u_sys(5, (unsigned long)(uintptr_t)":");
    for (int p = 0; p < 3; p++) {
        int durum = 0;   /* 0=filtreli 1=acik 2=kapali */
        u_tcp_syn(gw, dip, 40000 + p, portlar[p], 0x4B454D47u, 0x02);
        u_sys2(24, (unsigned long)(uintptr_t)tx_frame, 54);
        for (int d = 0; d < 30 && durum == 0; d++) {
            long n = (long)u_sys2(25, (unsigned long)(uintptr_t)rx_frame, 128);
            if (n < 54) continue;
            if (rx_frame[12] != 0x08 || rx_frame[13] != 0x00 || rx_frame[23] != 6) continue;
            if (!(rx_frame[26] == dip[0] && rx_frame[27] == dip[1] &&
                  rx_frame[28] == dip[2] && rx_frame[29] == dip[3])) continue;
            unsigned char fl = rx_frame[47];
            if ((fl & 0x12) == 0x12) durum = 1; else if (fl & 0x04) durum = 2;
        }
        u_sys(5, (unsigned long)(uintptr_t)" "); u_sys(6, (unsigned long)portlar[p]); u_sys(5, (unsigned long)(uintptr_t)":");
        u_sys(5, (unsigned long)(uintptr_t)(durum == 1 ? "ACIK" : durum == 2 ? "KAPALI" : "FILTRELI"));
    }
    u_sys(7, 0);
}

/* --- SPAWN edilebilir userspace PROGRAMLAR (.user, EL0) ---
 * Kabuk bunları sys(12)=spawn ile AYRI EL0 PROCESS olarak başlatır. Her biri kendi
 * süreç adres-uzayında (kdl_surec_spawn) EL0'da koşar, syscall ile iş yapar, exit eder.
 * Bu = gerçek OS spawn/exec: bir userspace program başka bir userspace programı çalıştırır. */
__attribute__((section(".user"), noinline))
static void prog_hesap(void) {
    int t = 6 * 7;                                                       /* = 42 (hesap) */
    u_sys2(15, (unsigned long)(uintptr_t)FILE_SONUC, (unsigned long)t);  /* dosya_yaz(sonuc=42) — IPC */
    u_sys(5, (unsigned long)(uintptr_t)"[prog hesap] 6*7=42 -> sonuc dosyasi"); u_sys(7, 0);
    u_sys(13, 0);                                                        /* exit (görev-öldür) */
    for (;;) { }
}
__attribute__((section(".user"), noinline))
static void prog_selam(void) {
    u_sys(5, (unsigned long)(uintptr_t)"[prog selam] merhaba, ben ayri bir userspace process"); u_sys(7, 0);
    u_sys(13, 0);
    for (;;) { }
}
/* Üretici program — 'coklu' ile EŞZAMANLI çok-kopya spawn edilir. Her kopya getpid ile
 * kendi kimliğini alır (ayrı process kanıtı) + paylaşımlı kanala SABİT 100 yazar (IPC,
 * sys 22=kanal_gonder) + exit. Kabuk hepsini join edip kanaldan toplar (det: N*100). */
__attribute__((section(".user"), noinline))
static void prog_uretici(void) {
    unsigned long pid = u_sys(11, 0);            /* getpid — ayrı process kimliği */
    u_sys(5, (unsigned long)(uintptr_t)"[prog uretici] pid="); u_sys(6, pid);
    u_sys(5, (unsigned long)(uintptr_t)" kanala 100 yaziyor"); u_sys(7, 0);
    u_sys(22, 100);                              /* kanal_gonder(100) — paylaşımlı IPC */
    u_sys(13, 0);                                /* exit */
    for (;;) { }
}

/* Bir komut satırını EL0'dan syscall'larla çalıştır. */
__attribute__((section(".user"), noinline))
static void komut_calistir(char *satir) {
    char *tok[3];
    int nt = tokenize(satir, tok);
    if (nt == 0) return;
    if (str_esit(tok[0], CMD_YARDIM)) {
        u_sys(5, (unsigned long)(uintptr_t)"KOMUTLAR: yardim echo ls yaz oku sil saat ping arpscan scan calistir coklu cik"); u_sys(7, 0);
    } else if (str_esit(tok[0], CMD_ECHO)) {
        if (nt >= 2) { u_sys(5, (unsigned long)(uintptr_t)tok[1]); u_sys(7, 0); }
    } else if (str_esit(tok[0], CMD_LS)) {
        unsigned long n = u_sys(19, 0);
        u_sys(5, (unsigned long)(uintptr_t)"LS:");
        for (unsigned long i = 0; i < n; i++) {
            u_sys2(20, i, (unsigned long)(uintptr_t)cikti_buf);
            u_sys(5, (unsigned long)(uintptr_t)" "); u_sys(5, (unsigned long)(uintptr_t)cikti_buf);
        }
        u_sys(7, 0);
    } else if (str_esit(tok[0], CMD_YAZ) && nt >= 3) {
        u_sys2(17, (unsigned long)(uintptr_t)tok[1], (unsigned long)(uintptr_t)tok[2]);
        u_sys(5, (unsigned long)(uintptr_t)"YAZILDI"); u_sys(7, 0);
    } else if (str_esit(tok[0], CMD_OKU) && nt >= 2) {
        long r = (long)u_sys2(18, (unsigned long)(uintptr_t)tok[1], (unsigned long)(uintptr_t)cikti_buf);
        u_sys(5, (unsigned long)(uintptr_t)"OKU: ");
        u_sys(5, (unsigned long)(uintptr_t)(r < 0 ? "yok" : cikti_buf)); u_sys(7, 0);
    } else if (str_esit(tok[0], CMD_SIL) && nt >= 2) {
        u_sys(21, (unsigned long)(uintptr_t)tok[1]);
        u_sys(5, (unsigned long)(uintptr_t)"SILINDI"); u_sys(7, 0);
    } else if (str_esit(tok[0], CMD_SAAT)) {
        u_sys(5, (unsigned long)(uintptr_t)"SAAT: ");
        u_sys(6, u_sys(27, 0));   /* RTC syscall → Unix saniye */
        u_sys(7, 0);
    } else if (str_esit(tok[0], CMD_PING)) {
        /* ping <oktet> — EL0'dan ICMP echo (net syscall). Yoksa 2 = SLIRP gateway (det). */
        int oktet = (nt >= 2) ? u_str_to_int(tok[1]) : 2;
        if (oktet <= 0 || oktet > 255) oktet = 2;
        u_sys(5, (unsigned long)(uintptr_t)(u_ping(oktet) ? "PING: CANLI" : "PING: yanit yok"));
        u_sys(7, 0);
    } else if (str_esit(tok[0], CMD_ARPSCAN)) {
        /* arpscan — EL0'dan subnet ARP tarama (net syscall). */
        u_sys(5, (unsigned long)(uintptr_t)"ARPSCAN: ");
        u_sys(6, (unsigned long)u_arpscan()); u_sys(5, (unsigned long)(uintptr_t)" host"); u_sys(7, 0);
    } else if (str_esit(tok[0], CMD_SCAN)) {
        /* scan <oktet> — EL0'dan TCP SYN port tarama (net syscall). */
        int oktet = (nt >= 2) ? u_str_to_int(tok[1]) : 2;
        if (oktet <= 0 || oktet > 255) oktet = 2;
        u_scan(oktet);
    } else if (str_esit(tok[0], CMD_CALISTIR) && nt >= 2) {
        /* calistir <program> — SPAWN/EXEC: kabuk (EL0 process) sys(12) ile AYRI userspace
         * program başlatır → sys(14) join → sonucu (IPC dosyası) okur. Gerçek OS spawn/exec. */
        void (*prog)(void) = 0;
        if (str_esit(tok[1], PROG_HESAP)) prog = prog_hesap;
        else if (str_esit(tok[1], PROG_SELAM)) prog = prog_selam;
        if (!prog) { u_sys(5, (unsigned long)(uintptr_t)"CALISTIR: bilinmeyen program"); u_sys(7, 0); }
        else {
            u_sys(5, (unsigned long)(uintptr_t)"CALISTIR spawn: "); u_sys(5, (unsigned long)(uintptr_t)tok[1]); u_sys(7, 0);
            long pid = (long)u_sys(12, (unsigned long)(uintptr_t)prog);        /* spawn → pid */
            if (pid < 0) { u_sys(5, (unsigned long)(uintptr_t)"SPAWN HATA"); u_sys(7, 0); }
            else {
                for (int w = 0; w < 60000000 && !u_sys(14, (unsigned long)pid); w++) { }  /* join (bounded) */
                if (str_esit(tok[1], PROG_HESAP)) {                            /* hesap → sonuç dosyası oku (IPC) */
                    long r = (long)u_sys(16, (unsigned long)(uintptr_t)FILE_SONUC);
                    u_sys(5, (unsigned long)(uintptr_t)"CALISTIR bitti (pid="); u_sys(6, (unsigned long)pid);
                    u_sys(5, (unsigned long)(uintptr_t)") sonuc="); u_sys(6, (unsigned long)r); u_sys(7, 0);
                } else {
                    u_sys(5, (unsigned long)(uintptr_t)"CALISTIR bitti (pid="); u_sys(6, (unsigned long)pid);
                    u_sys(5, (unsigned long)(uintptr_t)")"); u_sys(7, 0);
                }
            }
        }
    } else if (str_esit(tok[0], CMD_COKLU)) {
        /* coklu — 3 userspace worker'ı EŞZAMANLI spawn et (concurrent multi-process).
         * Hepsi kendi pid'iyle koşar + paylaşımlı kanala 100 yazar (IPC). Kabuk hepsini
         * join eder + kanaldan toplar → 3*100=300 (det). = çoklu-process yönetimi. */
        long pids[3]; int spawn_ok = 0;
        for (int i = 0; i < 3; i++) {
            pids[i] = (long)u_sys(12, (unsigned long)(uintptr_t)prog_uretici);   /* spawn */
            if (pids[i] >= 0) {
                spawn_ok++;
                u_sys(5, (unsigned long)(uintptr_t)"COKLU spawn pid="); u_sys(6, (unsigned long)pids[i]); u_sys(7, 0);
            }
        }
        for (int i = 0; i < 3; i++) {                                            /* hepsini join */
            if (pids[i] < 0) continue;
            for (int w = 0; w < 60000000 && !u_sys(14, (unsigned long)pids[i]); w++) { }
        }
        int toplam = 0, alindi = 0;                                              /* kanaldan topla (IPC) */
        for (int i = 0; i < 8; i++) {
            long v = (long)u_sys(23, 0);                                         /* kanal_al (-1=boş) */
            if (v < 0) break;
            toplam += (int)v; alindi++;
        }
        u_sys(5, (unsigned long)(uintptr_t)"COKLU BITTI: ");
        u_sys(6, (unsigned long)spawn_ok); u_sys(5, (unsigned long)(uintptr_t)" process, kanal-toplam=");
        u_sys(6, (unsigned long)toplam); u_sys(5, (unsigned long)(uintptr_t)" ("); u_sys(6, (unsigned long)alindi);
        u_sys(5, (unsigned long)(uintptr_t)" mesaj)"); u_sys(7, 0);
    } else {
        u_sys(5, (unsigned long)(uintptr_t)"? (yardim)"); u_sys(7, 0);
    }
}

/* EL0 kabuk PROCESS'i — çekirdek boot tablosu altında, 0x42000000 (.user) EL0'da koşar. */
__attribute__((section(".user"), noinline))
void kemgu_el0_shell(void) {
    /* PROOF(d): kaynak-EL bildir → kernel "EL0 SYSCALL kaynak-EL=0x0" (kabuk GERÇEKTEN EL0). */
    u_sys(2, 0);
    u_sys(5, (unsigned long)(uintptr_t)"SHELL EL0 BASLADI (userspace process)"); u_sys(7, 0);

    /* 1) Gömülü deterministik dizi — EL0 kabuk komutları syscall'la işler (gate kanıtı). */
    char *s = INIT_BETIK;
    while (*s) {
        char *son = s; while (*son && *son != '\n') son++;
        int devam = (*son != 0); *son = 0;
        u_sys(5, (unsigned long)(uintptr_t)"el0$ "); u_sys(5, (unsigned long)(uintptr_t)s); u_sys(7, 0);
        komut_calistir(s);
        s = devam ? son + 1 : son;
    }

    /* 2) Canlı UART girişi — kabuk EL0'dan sys(26) ile satır okur + işler (gerçek interaktif).
     * Best-effort: boot-window FIFO overrun (D-188) canlı girişi kısmen düşürebilir; gate
     * PASS'ı yukarıdaki DETERMİNİSTİK gömülü diziye + "SHELL EL0 OK"e dayanır. Kısa döngü. */
    for (int k = 0; k < 4; k++) {
        u_sys(5, (unsigned long)(uintptr_t)"el0$ ");
        long r = (long)u_sys2(26, (unsigned long)(uintptr_t)satir_buf, 256);
        if (r < 0) { u_sys(7, 0); break; }         /* EOF */
        u_sys(5, (unsigned long)(uintptr_t)satir_buf); u_sys(7, 0);   /* echo */
        if (str_esit(satir_buf, CMD_CIK)) break;
        komut_calistir(satir_buf);
    }

    u_sys(5, (unsigned long)(uintptr_t)"SHELL EL0 OK"); u_sys(7, 0);
    /* exit = num=13 (kdl_gorev_bitir): görevi ÖLÜ işaretle → scheduler bırakır + çekirdek
     * (main) devam eder. NOT num=3 (o kernel'i for(;;) halt eder = D2 non-preempt çıkış). */
    u_sys(13, 0);
    for (;;) { }        /* ölü görev — sonraki preempt'te scheduler atlar */
}
