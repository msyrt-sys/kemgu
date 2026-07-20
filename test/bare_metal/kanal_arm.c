/*
 * C7d kanal (SPSC IPC) testi (aarch64) — preemptive scheduler üstünde
 * görevler-arası iletişim.
 *
 * Üretici görev 1..10 değerlerini kanala SIRAYLA gönderir; tüketici (main) 10
 * değer alıp toplar + FIFO sırayı doğrular. Kanal kapasitesi küçük (4) → üretici
 * dolu-bloklar, tüketici boş-bloklar; timer-IRQ preemption karşı göreve geçirir
 * (çift yönlü ping-pong akış denetimi). toplam=55 + sıra=1..10 → "KANAL OK toplam=55".
 *
 * Bu, KEMGU `kanal` ilkelinin (DRF V1) çekirdek-düzeyi kanıtı: iki preemptive
 * görev + çift-yönlü bloklama + FIFO mesaj geçişi birlikte çalışır. (D-121 IRQ
 * x0-koruma onarımı bu cap=4 ping-pong yolunu sağlamlaştırdı.)
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
    /* 3 = KDL_KANAL_KAP-1 (kdl_kanal.c'deki sabit halkanin tuttugu en cok oge).
     * Bilincli olarak dusuk: uretici sik dolu-bloklar, tuketici sik bos-bloklar
     * -> CIFT YONLU akis denetimi gercekten sinanir (bkz. kdl_kanal.c notu). */
    kanal = kdl_kanal_olustur(3);

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
