/*
 * C3b/C4 timer testi — IRQ + periyodik timer kanıtı (aarch64 + x86_64 ORTAK).
 *
 * Kesme denetleyici + timer kur, IRQ'leri aç, idle-döngüsü. Timer ~10ms'de bir
 * kesme üretir → boot IRQ stub → kdl_kesme_isle (tik say). 5. tikte "TIMER OK
 * tik=5". "TIMER OK" görülürse IRQ teslimi + timer çalışıyor.
 *
 * AYNI kernel iki mimaride: aarch64 (GICv2+CNTV) ve x86_64 (PIC+PIT) — runtime
 * (kdl_zaman.c) arch-spesifik, kernel arch-bağımsız.
 */
extern void kdl_yazdir_metin(const char *);
extern void kdl_yazdir_satir(void);
extern void kdl_kesme_kur(void);
extern void kdl_timer_baslat(void);
extern void kdl_idle(void);

int main(void) {
    kdl_yazdir_metin("TIMER BASLA");
    kdl_yazdir_satir();

    kdl_kesme_kur();
    kdl_timer_baslat();

    for (;;) {
        kdl_idle();          /* timer IRQ bunu uyandırır */
    }
}
