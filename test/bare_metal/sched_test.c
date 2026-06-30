/*
 * C7a cooperative scheduling testi — bağlam-değiştirme kanıtı (aarch64 + x86 ORTAK).
 *
 * main (görev 0) + gorev1 (görev 1) round-robin yield eder. Çıktı interleave:
 * [main] / [gorev1] / [main] / ... → bağlam-değiştirme çalışıyor. main 3 tur sonra
 * "SCHED OK" basıp döner. "SCHED OK" + hem [main] hem [gorev1] görülürse C7a tamam.
 */
extern void kdl_yazdir_metin(const char *);
extern void kdl_yazdir_satir(void);
extern void kdl_gorev_baslat(void);
extern int  kdl_gorev_olustur(void (*giris)(void), void *yigin_tepe);
extern void kdl_gorev_ver(void);

static unsigned char yigin1[8192] __attribute__((aligned(16)));

static void gorev1(void) {
    for (;;) {
        kdl_yazdir_metin("[gorev1]");
        kdl_yazdir_satir();
        kdl_gorev_ver();            /* CPU'yu main'e bırak */
    }
}

int main(void) {
    kdl_yazdir_metin("SCHED BASLA");
    kdl_yazdir_satir();

    kdl_gorev_baslat();
    kdl_gorev_olustur(gorev1, yigin1 + sizeof(yigin1));

    for (int i = 0; i < 3; i++) {
        kdl_yazdir_metin("[main]");
        kdl_yazdir_satir();
        kdl_gorev_ver();           /* CPU'yu gorev1'e bırak */
    }

    kdl_yazdir_metin("SCHED OK");
    kdl_yazdir_satir();
    return 0;
}
