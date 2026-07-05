/*
 * C7c blocking (sleep/wake) testi (aarch64) — preemptive scheduler üstüne.
 *
 * Görev B kdl_uyu(8) ile 8 timer-tick BLOKLANIR; scheduler onu atlar, A (main)
 * o sırada koşar (a_sayac artar). 8 tick sonra B READY olur → uyanır + "B WOKE".
 * a_sayac>0 (uyku sırasında A koştu) → blocking + uyandırma çalışıyor.
 */
extern void kdl_yazdir_metin(const char *);
extern void kdl_yazdir_satir(void);
extern void kdl_kesme_kur(void);
extern void kdl_timer_baslat(void);
extern void kdl_preempt_baslat(void);
extern int  kdl_preempt_gorev_olustur(void (*giris)(void), void *yigin_tepe);
extern void kdl_preempt_ac(void);
extern void kdl_uyu(int tikler);

static unsigned char yigin_b[8192] __attribute__((aligned(16)));
static volatile int a_sayac = 0;

/* Görev B: 8 tick uyu (bloklu), uyanınca A'nın koşmuş olduğunu doğrula. */
static void gorev_b(void) {
    kdl_uyu(8);                          /* 8 timer-tick blokla */
    kdl_yazdir_metin("B WOKE a_kostu=");
    kdl_yazdir_metin(a_sayac > 0 ? "VAR" : "YOK");
    kdl_yazdir_satir();
    for (;;) { }
}

int main(void) {
    kdl_yazdir_metin("SLEEP BASLA");
    kdl_yazdir_satir();

    kdl_preempt_baslat();
    kdl_preempt_gorev_olustur(gorev_b, yigin_b + sizeof(yigin_b));
    kdl_kesme_kur();
    kdl_timer_baslat();
    kdl_preempt_ac();

    /* Görev A (main): B uyurken koş (a_sayac artar). */
    for (;;) {
        for (volatile int i = 0; i < 400000; i++) { }
        a_sayac++;
    }
}
