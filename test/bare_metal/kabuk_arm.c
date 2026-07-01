/*
 * D-135 testi (aarch64) — BASİT USERSPACE KABUK (shell) — DORUK.
 *
 * Bir userspace program komut SCRIPT'ini ayrıştırıp (tokenize) FS syscall'larına
 * dağıtır — gerçek bir kabuk/komut yorumlayıcısı. Tüm yığını (süreç + EL0 + syscall
 * ABI + RAM dosya sistemi) tanınabilir bir OS artefaktına bağlar (Faz E/F doruğu).
 *
 * Komutlar (.user_data'daki script'ten, EL0-okunur AP=01):
 *   yaz <ad> <metin>  → dosya_yaz_metin
 *   oku <ad>          → dosya_oku_metin + bas
 *   ls                → dosya listele
 *
 * Kanıt: "SHELL> oku gunluk" + "  KEMGU-OS" (kabuk komutu ayrıştırdı+çalıştırdı).
 */
#include <stdint.h>
extern void kdl_yazdir_metin(const char *);
extern void kdl_yazdir_satir(void);
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

/* Komut script'i — .user_data (AP=01, EL0 okur/yazar). In-place tokenize edilir. */
__attribute__((section(".user_data")))
static char script[] =
    "yaz gunluk KEMGU-OS\n"
    "oku gunluk\n"
    "ls\n";

/* Komut adları — .user_data'da OLMALI: str_esit bunları EL0'da OKUR. Normal string
 * literalleri .rodata'da (AP=00, EL0'a kapalı) → EL0 okuyunca permission-fault. */
__attribute__((section(".user_data"))) static char CMD_YAZ[] = "yaz";
__attribute__((section(".user_data"))) static char CMD_OKU[] = "oku";
__attribute__((section(".user_data"))) static char CMD_LS[]  = "ls";

__attribute__((always_inline)) static inline unsigned long sys(unsigned long num, unsigned long arg) {
    register unsigned long x8 __asm__("x8") = num;
    register unsigned long x0 __asm__("x0") = arg;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8) : "memory");
    return x0;
}
__attribute__((always_inline)) static inline unsigned long sys2(unsigned long num, unsigned long a0, unsigned long a1) {
    register unsigned long x8 __asm__("x8") = num;
    register unsigned long x0 __asm__("x0") = a0;
    register unsigned long x1 __asm__("x1") = a1;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8), "r"(x1) : "memory");
    return x0;
}

/* .user (AP=01, EL0-exec) — kabuk yardımcıları (kernel çağırmaz). */
__attribute__((section(".user"), noinline))
static int str_esit(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

/* Satırı boşluklara böl (in-place null-term), en çok 3 token; token sayısı döner. */
__attribute__((section(".user"), noinline))
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

__attribute__((section(".user"), noinline))
static void kabuk(void) {
    char *buf = (char *)(uintptr_t)0x42210000UL;   /* çıktı/ad tamponu (veri sayfası) */
    char *satir = script;
    while (*satir) {
        char *son = satir;
        while (*son && *son != '\n') son++;
        int devam = (*son != 0);
        *son = 0;

        sys(5, (unsigned long)(uintptr_t)"SHELL> ");
        sys(5, (unsigned long)(uintptr_t)satir);
        sys(7, 0);

        char *tok[3];
        int nt = tokenize(satir, tok);
        if (nt >= 3 && str_esit(tok[0], CMD_YAZ)) {
            sys2(17, (unsigned long)(uintptr_t)tok[1], (unsigned long)(uintptr_t)tok[2]);
        } else if (nt >= 2 && str_esit(tok[0], CMD_OKU)) {
            sys2(18, (unsigned long)(uintptr_t)tok[1], (unsigned long)(uintptr_t)buf);
            sys(5, (unsigned long)(uintptr_t)"  ");
            sys(5, (unsigned long)(uintptr_t)buf);
            sys(7, 0);
        } else if (nt >= 1 && str_esit(tok[0], CMD_LS)) {
            unsigned long n = sys(19, 0);
            for (unsigned long i = 0; i < n; i++) {
                sys2(20, i, (unsigned long)(uintptr_t)buf);
                sys(5, (unsigned long)(uintptr_t)"  ");
                sys(5, (unsigned long)(uintptr_t)buf);
                sys(7, 0);
            }
        }
        satir = devam ? son + 1 : son;
    }
    for (;;) { }
}

int main(void) {
    kdl_yazdir_metin("KABUK BASLA");
    kdl_yazdir_satir();

    kdl_surec_kur_el0_veri(l1_l, l2_l, 0x42000000UL, 0x44000000UL);
    kdl_preempt_baslat();
    int tl = kdl_preempt_gorev_olustur_el0(kabuk, kstack_l + sizeof(kstack_l),
                                           (void *)(uintptr_t)0x42380000UL);
    kdl_preempt_gorev_ttbr(tl, l1_l);
    kdl_kesme_kur();
    kdl_timer_baslat();
    kdl_preempt_ac();

    for (;;) { __asm__ volatile("wfe"); }
}
