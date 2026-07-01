/*
 * D-137 testi (aarch64) — PROGRAM ÇALIŞTIRMA İŞ AKIŞI: spawn→hesap→dosyaya-yaz→join→oku.
 *
 * Gerçek OS iş akışı: bir program başka bir programı ÇALIŞTIRIR, o program HESAP
 * yapıp sonucu bir DOSYAYA yazar, başlatan program sonucu GERİ OKUR. Tüm süreç +
 * FS + IPC yığınının uçtan-uca entegrasyonu (bir programı çalıştır, çıktısını al).
 *
 *   launcher: spawn(worker) → join → dosya_oku("sonuc") → bas.
 *   worker  : 1..10 topla (=55) → dosya_yaz("sonuc",55) → exit.
 *
 * Kanıt: "RESULT=55" (worker hesabı dosya üzerinden launcher'a ulaştı).
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

/* Çalıştırılan program: hesap yap → sonucu dosyaya yaz → exit. */
__attribute__((section(".user"), noinline))
static void worker(void) {
    int toplam = 0;
    for (int i = 1; i <= 10; i++) toplam += i;                  /* = 55 */
    sys2(15, (unsigned long)(uintptr_t)"sonuc", (unsigned long)toplam);   /* dosya_yaz */
    sys(13, 0);                                                 /* exit */
    for (;;) { }
}

/* Başlatan program: worker'ı çalıştır → bitmesini bekle → sonucu dosyadan oku. */
__attribute__((section(".user"), noinline))
static void launcher(void) {
    unsigned long pid = sys(12, (unsigned long)(uintptr_t)worker);   /* spawn */
    while (!sys(14, pid)) { }                                        /* join */
    unsigned long r = sys(16, (unsigned long)(uintptr_t)"sonuc");    /* dosya_oku */
    sys(5, (unsigned long)(uintptr_t)"RESULT=");
    sys(6, r);
    sys(7, 0);
    for (;;) { }
}

int main(void) {
    kdl_yazdir_metin("CALIS BASLA");
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
