/*
 * C7e öncelikli (priority) scheduling testi (aarch64) — preemptive üstüne.
 *
 * main (görev 0) YÜKSEK öncelik (1), B (görev 1) DÜŞÜK öncelik (0). Strict
 * priority: main READY iken scheduler DAİMA main'i seçer → B aç kalır (starve).
 *   Faz 1: main meşgul-döner (timer tikler ama main tekelinde) → b_sayac=0.
 *   Faz 2: main kdl_uyu(10) ile bloklanır → tek READY=B → B koşar → b_sayac>0.
 * b1==0 && b2>0 → öncelik hem aç-bırakma hem kurtarma → "PRIORITY OK".
 */
extern void kdl_yaz_metin(const char *);
extern void kdl_yazdir_metin(const char *);
extern void kdl_yazdir_tam(int);
extern void kdl_kesme_kur(void);
extern void kdl_timer_baslat(void);
extern void kdl_preempt_baslat(void);
extern int  kdl_preempt_gorev_olustur(void (*giris)(void), void *yigin_tepe);
extern void kdl_preempt_oncelik(int gorev, int oncelik);
extern void kdl_preempt_ac(void);
extern void kdl_uyu(int tikler);

static unsigned char yigin_b[8192] __attribute__((aligned(16)));
static volatile int b_sayac = 0;

/* Düşük öncelikli görev B: meşgul-döner (yalnız main bloklanınca koşabilir). */
static void gorev_b(void) {
    for (;;) {
        for (volatile int i = 0; i < 100000; i++) { }
        b_sayac++;
    }
}

int main(void) {
    kdl_yazdir_metin("PRIORITY BASLA");

    kdl_preempt_baslat();
    int b = kdl_preempt_gorev_olustur(gorev_b, yigin_b + sizeof(yigin_b));
    kdl_preempt_oncelik(0, 1);     /* main (görev 0) = yüksek öncelik */
    kdl_preempt_oncelik(b, 0);     /* B = düşük öncelik */
    kdl_kesme_kur();
    kdl_timer_baslat();
    kdl_preempt_ac();

    /* Faz 1: main yüksek-öncelikli → tekelde koş (timer tikler, B aç kalır). */
    for (volatile int i = 0; i < 4000000; i++) { }
    int b1 = b_sayac;              /* beklenen: 0 (B hiç koşmadı) */

    /* Faz 2: main bloklan → B tek READY → koşar. */
    kdl_uyu(10);
    int b2 = b_sayac;              /* beklenen: > 0 (B uyku sırasında koştu) */

    if (b1 == 0 && b2 > 0) {
        kdl_yaz_metin("PRIORITY OK ac-faz1=");
        kdl_yazdir_tam(b1);        /* → "PRIORITY OK ac-faz1=0" (tek satır) */
    } else {
        kdl_yazdir_metin("PRIORITY HATA");
    }
    for (;;) { }
}
