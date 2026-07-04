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
    "yaz nesne KABUK\noku nesne\nls\nsaat\n";
/* Kabuk tamponları — .user_data (0x42000000 sayfası, EL0-erişimli + FS-validator izinli). */
__attribute__((section(".user_data"))) static char satir_buf[256];
__attribute__((section(".user_data"))) static char cikti_buf[128];

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

/* Bir komut satırını EL0'dan syscall'larla çalıştır. */
__attribute__((section(".user"), noinline))
static void komut_calistir(char *satir) {
    char *tok[3];
    int nt = tokenize(satir, tok);
    if (nt == 0) return;
    if (str_esit(tok[0], CMD_YARDIM)) {
        u_sys(5, (unsigned long)(uintptr_t)"KOMUTLAR: yardim echo ls yaz oku sil saat cik"); u_sys(7, 0);
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
