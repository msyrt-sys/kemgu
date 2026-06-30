/*
 * C7d kanal (SPSC IPC) testi (aarch64) — preemptive scheduler üstünde
 * görevler-arası iletişim.
 *
 * Üretici görev 1..10 değerlerini kanala SIRAYLA gönderir; tüketici (main) 10
 * değer alıp toplar + FIFO sırayı doğrular. Tüketici kanal boşken bloklanır
 * (alım bekleme): timer-IRQ preemption üreticiye geçirir → üretici doldurur →
 * tüketici uyanıp okur. toplam=55 + sıra=1..10 → "KANAL OK toplam=55".
 *
 * Bu, KEMGU `kanal` ilkelinin (DRF V1) çekirdek-düzeyi kanıtı: iki preemptive
 * görev + bloklamalı-alım + FIFO mesaj geçişi birlikte çalışır.
 */
#include "kdl_kanal.h"

extern void kdl_yaz_metin(const char *);
extern void kdl_yazdir_metin(const char *);
extern void kdl_yazdir_tam(int);
extern void kdl_kesme_kur(void);
extern void kdl_timer_baslat(void);
extern void kdl_preempt_baslat(void);
extern int  kdl_preempt_gorev_olustur(void (*giris)(void), void *yigin_tepe);
extern void kdl_preempt_ac(void);

static unsigned char yigin_u[8192] __attribute__((aligned(16)));
static KdlKanal *kanal;

/* Üretici görev: 1..10 değerlerini sırayla kanala gönder. */
static void uretici(void) {
    for (int i = 1; i <= 10; i++) {
        kdl_kanal_gonder(kanal, i);
    }
    for (;;) { }
}

int main(void) {
    kdl_yazdir_metin("KANAL BASLA");
    kanal = kdl_kanal_olustur();

    kdl_preempt_baslat();
    kdl_preempt_gorev_olustur(uretici, yigin_u + sizeof(yigin_u));
    kdl_kesme_kur();
    kdl_timer_baslat();
    kdl_preempt_ac();

    /* Tüketici (main): 10 değer al, FIFO sıra + toplam doğrula. */
    int toplam = 0;
    int sira_ok = 1;
    for (int i = 1; i <= 10; i++) {
        int v = kdl_kanal_al(kanal);
        if (v != i) sira_ok = 0;
        toplam += v;
    }

    if (toplam == 55 && sira_ok) {
        kdl_yaz_metin("KANAL OK toplam=");
        kdl_yazdir_tam(toplam);          /* → "KANAL OK toplam=55" (tek satır) */
    } else {
        kdl_yazdir_metin("KANAL HATA");
    }
    for (;;) { }
}
