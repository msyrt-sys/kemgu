/*
 * D-125 testi (aarch64) — PREEMPTIVE EL0 (userspace) görev.
 *
 * main EL1'de (kernel, görev 0). Bir EL0 (userspace) görev (görev 1) preemptive
 * scheduler'a eklenir: kendi user yığınında EL0'da koşar, timer-IRQ ile ZORUNLU
 * preempt edilir (yield ETMEZ). EL0 görev .user sayfasındaki (AP=01) sayacı
 * artırır; main (EL1) o sayacı okur (EL1, EL0 sayfasını görür).
 *
 * Bu, tam bir OS'un son parçası: userspace görevler PREEMPTIVELY multitask edilir
 * (kernel + userspace görevler timer-IRQ ile araya girerek dönüşümlü koşar).
 * kdl_irq_ortak SP_EL0'ı trap-frame'de kaydeder (Stage 1) → EL0 bağlamı korunur.
 *
 * Kanıt: "PREEMPT EL0 BASLA" + "PREEMPT EL0 OK" (el0_sayac>0 = EL0 görev preempt
 * ile koştu, main de koştu).
 */
#include <stdint.h>
extern void kdl_yazdir_metin(const char *);
extern void kdl_yazdir_satir(void);
extern void kdl_kesme_kur(void);
extern void kdl_timer_baslat(void);
extern void kdl_preempt_baslat(void);
extern int  kdl_preempt_gorev_olustur_el0(void (*giris)(void), void *kernel_yigin_tepe,
                                          void *user_yigin_tepe);
extern void kdl_preempt_ac(void);
extern unsigned char __user_stack_top[];   /* .user (AP=01) EL0 yığını */

/* EL0-yazılabilir sayaç .user_data'da (0x42000000 AP=01 sayfası; kod .user'dan
 * ayrı input-section → derleyici section-tip çakışması olmaz). EL0 yazar, main okur. */
__attribute__((section(".user_data"))) static volatile int el0_sayac = 0;
/* EL0 görevin KERNEL yığını (trap-frame preempt sırasında burada) — normal kernel
 * bellek (AP=00, EL1). EL0 çalışma yığını = __user_stack_top (.user). */
static unsigned char kernel_yigin[8192] __attribute__((aligned(16)));

/* EL0'da koşar (.user, AP=01). SELF-CONTAINED: kernel çağırmaz, yalnız kendi
 * sayfasındaki sayacı artırır. Timer-IRQ ile preempt edilir (SPSR=EL0t IRQ-açık). */
__attribute__((section(".user"), noinline))
static void el0_gorev(void) {
    for (;;) {
        el0_sayac++;
        for (volatile int i = 0; i < 2000; i++) { }   /* meşgul kal (preempt için) */
    }
}

int main(void) {
    kdl_yazdir_metin("PREEMPT EL0 BASLA");
    kdl_yazdir_satir();

    kdl_preempt_baslat();                              /* main = görev 0 (EL1) */
    kdl_preempt_gorev_olustur_el0(el0_gorev,
                                  kernel_yigin + sizeof(kernel_yigin),
                                  __user_stack_top);   /* görev 1 (EL0), user yığını .user'da */
    kdl_kesme_kur();
    kdl_timer_baslat();
    kdl_preempt_ac();

    /* main (EL1): kendi işini yap; EL0 görev preempt ile araya girip sayacı artırmalı. */
    for (volatile int i = 0; i < 4000000; i++) { }

    if (el0_sayac > 0) {
        kdl_yazdir_metin("PREEMPT EL0 OK");            /* EL0 görev preempt ile koştu */
    } else {
        kdl_yazdir_metin("PREEMPT EL0 HATA");
    }
    kdl_yazdir_satir();
    for (;;) { __asm__ volatile("wfe"); }
}
