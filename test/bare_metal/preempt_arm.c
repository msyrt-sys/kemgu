/*
 * C7b preemptive scheduling testi (aarch64) — timer-IRQ ZORUNLU switch.
 *
 * İki görev (A=main, B) busy-loop yapar ve ASLA yield ETMEZ. Yalnız timer-IRQ
 * onları preempt edebilir (kdl_irq_ortak full trap-frame → kdl_preempt → SP swap).
 * B'nin çalışması (sayac_b artması) SADECE preemption ile mümkün → "PREEMPT OK"
 * görülürse zorunlu bağlam-değiştirme çalışıyor.
 */
extern void kdl_yazdir_metin(const char *);
extern void kdl_yazdir_satir(void);
extern void kdl_kesme_kur(void);        /* GIC init */
extern void kdl_timer_baslat(void);     /* timer + IRQ aç */
extern void kdl_preempt_baslat(void);
extern int  kdl_preempt_gorev_olustur(void (*giris)(void), void *yigin_tepe);
extern void kdl_preempt_ac(void);

static unsigned char yigin_b[8192] __attribute__((aligned(16)));
static volatile int sayac_a = 0;
static volatile int sayac_b = 0;

/* Görev B: busy-loop + "[B]" bas. YIELD ETMEZ — timer preempt eder. */
static void gorev_b(void) {
    for (;;) {
        for (volatile int i = 0; i < 800000; i++) { }
        kdl_yazdir_metin("[B]");
        kdl_yazdir_satir();
        sayac_b++;
    }
}

int main(void) {
    kdl_yazdir_metin("PREEMPT BASLA");
    kdl_yazdir_satir();

    kdl_preempt_baslat();
    kdl_preempt_gorev_olustur(gorev_b, yigin_b + sizeof(yigin_b));
    kdl_kesme_kur();
    kdl_timer_baslat();      /* timer + IRQ */
    kdl_preempt_ac();        /* preemption aktif */

    /* Görev A (main): busy-loop + "[A]" bas. YIELD ETMEZ. */
    for (;;) {
        for (volatile int i = 0; i < 800000; i++) { }
        kdl_yazdir_metin("[A]");
        kdl_yazdir_satir();
        sayac_a++;
        if (sayac_a >= 3 && sayac_b >= 1) {   /* B çalıştıysa → preemption oldu */
            kdl_yazdir_metin("PREEMPT OK");
            kdl_yazdir_satir();
            for (;;) { }
        }
    }
}
