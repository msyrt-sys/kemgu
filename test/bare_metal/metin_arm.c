/*
 * D-132 testi (aarch64) — METİN İÇERİKLİ dosya (bulk read/write, gerçek dosya içeriği).
 *
 * D-131 tek-değer dosyasını GERÇEK byte-içeriğe genişletir: bir süreç dosyaya METİN
 * yazar (kernel kullanıcı belleğinden kopyalar), başka süreç metni KENDİ tamponuna
 * okur (kernel kullanıcı tamponuna kopyalar) + basar. Kernel↔userspace çift-yönlü
 * bellek kopyası (read/write syscall ailesinin temeli).
 *
 *   launcher: dosya_yaz_metin("mesaj", "MERHABA DOSYA") → spawn(worker) → join.
 *   worker  : dosya_oku_metin("mesaj", tampon) → tamponu bas.
 *
 * Kanıt: "FILE TEXT: MERHABA DOSYA" (worker, launcher'ın yazdığı metni okudu+bastı).
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

/* Çocuk süreç: dosyanın metnini KENDİ tamponuna oku (kernel kopyalar) + bas. */
__attribute__((section(".user"), noinline))
static void worker(void) {
    char *tampon = (char *)(uintptr_t)0x42210000UL;   /* worker veri sayfası (AP=01) */
    long n = sys2(18, (unsigned long)(uintptr_t)"mesaj", (unsigned long)(uintptr_t)tampon);
    if (n > 0) {
        sys(5, (unsigned long)(uintptr_t)"FILE TEXT: ");
        sys(5, (unsigned long)(uintptr_t)tampon);     /* okunan metni bas */
    } else {
        sys(5, (unsigned long)(uintptr_t)"FILE TEXT HATA");
    }
    sys(7, 0);
    sys(13, 0);
    for (;;) { }
}

__attribute__((section(".user"), noinline))
static void launcher(void) {
    sys2(17, (unsigned long)(uintptr_t)"mesaj", (unsigned long)(uintptr_t)"MERHABA DOSYA");  /* yaz */
    unsigned long pid = sys(12, (unsigned long)(uintptr_t)worker);
    while (!sys(14, pid)) { }
    for (;;) { }
}

int main(void) {
    kdl_yazdir_metin("METIN BASLA");
    kdl_yazdir_satir();

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
