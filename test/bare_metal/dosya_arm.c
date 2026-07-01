/*
 * D-131 testi (aarch64) — RAM DOSYA SİSTEMİ + 2-argümanlı syscall.
 *
 * Çekirdek-aracılı isimli depolama: bir süreç dosyaya yazar, BAŞKA bir süreç okur
 * (süreçler-arası paylaşılan kalıcı depolama — Faz E dosya sistemi ilk adımı,
 * virtio-blk gerektirmez). 2-argümanlı syscall (dosya_yaz(ad, değer)) D-126
 * x1-koruma + D-131 SVC arg2 geçişiyle mümkün.
 *
 *   launcher: dosya_yaz("sayac", 1234) → spawn(worker) → join.
 *   worker  : dosya_oku("sayac") → 1234 okumalı → "FILE OK deger=1234".
 *
 * Kanıt: "FILE OK deger=1234" (worker, launcher'ın yazdığı dosyayı okudu).
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
/* 2-argümanlı syscall (D-131): arg0=x0, arg1=x1. */
__attribute__((always_inline)) static inline unsigned long sys2(unsigned long num, unsigned long a0, unsigned long a1) {
    register unsigned long x8 __asm__("x8") = num;
    register unsigned long x0 __asm__("x0") = a0;
    register unsigned long x1 __asm__("x1") = a1;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8), "r"(x1) : "memory");
    return x0;
}

/* Çocuk süreç: launcher'ın yazdığı dosyayı okur (süreçler-arası paylaşım). */
__attribute__((section(".user"), noinline))
static void worker(void) {
    unsigned long v = sys(16, (unsigned long)(uintptr_t)"sayac");   /* dosya_oku */
    if (v == 1234) {
        sys(5, (unsigned long)(uintptr_t)"FILE OK deger=");
        sys(6, v);
    } else {
        sys(5, (unsigned long)(uintptr_t)"FILE HATA");
    }
    sys(7, 0);
    sys(13, 0);                                                     /* exit */
    for (;;) { }
}

__attribute__((section(".user"), noinline))
static void launcher(void) {
    sys2(15, (unsigned long)(uintptr_t)"sayac", 1234);             /* dosya_yaz("sayac",1234) */
    unsigned long pid = sys(12, (unsigned long)(uintptr_t)worker); /* spawn */
    while (!sys(14, pid)) { }                                      /* join */
    for (;;) { }
}

int main(void) {
    kdl_yazdir_metin("DOSYA BASLA");
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
