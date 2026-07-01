/*
 * D-130 testi (aarch64) — SÜREÇ YAŞAM DÖNGÜSÜ: spawn → çalış → exit → join.
 *
 * Tam süreç yaşam döngüsü (gerçek OS temeli):
 *   1. launcher (EL0 süreç) spawn(worker) ile çocuk süreç yaratır (D-129).
 *   2. worker biraz iş yapar, sonra exit (num=13) → scheduler onu bir daha seçmez.
 *   3. launcher join eder: durum(pid) (num=14) syscall'ını EL0'da yoklayarak
 *      worker'ın bittiğini öğrenir (ebeveyn çocuğun bitişini bekler).
 *
 * Bloklamalı join yerine EL0-yoklama: launcher preemptive olduğundan yoklarken
 * worker koşar → exit eder → durum(pid)=1 → launcher devam (deadlock yok).
 *
 * Kanıt: "WORKER done" + "JOINED worker exited" (ebeveyn çocuğun exit'ini gördü).
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

/* Çocuk süreç: iş yap → rapor → EXIT. exit sonrası scheduler bir daha seçmez. */
__attribute__((section(".user"), noinline))
static void worker(void) {
    for (volatile int i = 0; i < 200000; i++) { }     /* iş */
    sys(5, (unsigned long)(uintptr_t)"WORKER done");
    sys(7, 0);
    sys(13, 0);                                        /* exit */
    for (;;) { }                                       /* exit sonrası kısa spin (atlanır) */
}

/* Ebeveyn: spawn + JOIN (worker bitene dek durum(pid) yokla). */
__attribute__((section(".user"), noinline))
static void launcher(void) {
    unsigned long pid = sys(12, (unsigned long)(uintptr_t)worker);   /* spawn */
    while (!sys(14, pid)) { }                          /* join: worker bitene kadar bekle */
    sys(5, (unsigned long)(uintptr_t)"JOINED worker exited");
    sys(7, 0);
    for (;;) { }
}

int main(void) {
    kdl_yazdir_metin("YASAM BASLA");
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
