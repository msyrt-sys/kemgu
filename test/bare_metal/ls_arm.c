/*
 * D-133 testi (aarch64) — DOSYA LİSTELEME (ls) — userspace dosya enumerasyonu.
 *
 * Bir userspace program dosya deposunu listeler: dosya_sayisi() ile sayar,
 * dosya_ad(idx, buf) ile her dosyanın adını okur + basar. Gerçek shell/OS
 * primitifi ("ls"). D-131/132 dosya sistemi üstünde.
 *
 * launcher: dosya_yaz("alfa",1) + dosya_yaz("beta",2) → sonra listele.
 *
 * Kanıt: "LS count=2" + "  alfa" + "  beta".
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

/* Userspace "ls": dosyaları say + adlarını listele. */
__attribute__((section(".user"), noinline))
static void launcher(void) {
    sys2(15, (unsigned long)(uintptr_t)"alfa", 1);    /* dosya_yaz */
    sys2(15, (unsigned long)(uintptr_t)"beta", 2);

    unsigned long n = sys(19, 0);                     /* dosya_sayisi */
    sys(5, (unsigned long)(uintptr_t)"LS count=");
    sys(6, n);
    sys(7, 0);

    char *buf = (char *)(uintptr_t)0x42210000UL;      /* ad tamponu (veri sayfası, AP=01) */
    for (unsigned long i = 0; i < n; i++) {
        sys2(20, i, (unsigned long)(uintptr_t)buf);   /* dosya_ad(i, buf) */
        sys(5, (unsigned long)(uintptr_t)"  ");
        sys(5, (unsigned long)(uintptr_t)buf);        /* dosya adını bas */
        sys(7, 0);
    }
    for (;;) { }
}

int main(void) {
    kdl_yazdir_metin("LS BASLA");
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
