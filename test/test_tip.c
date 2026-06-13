#include "tip.h"
#include "arena.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* === Test cercevesi === */

static int toplam_test = 0;
static int basarili = 0;
static int basarisiz = 0;

static void test_sonuc(const char *ad, int durum) {
    toplam_test++;
    if (durum) {
        basarili++;
        printf("  [%d] %s ... \xe2\x9c\x93\n", toplam_test, ad);
    } else {
        basarisiz++;
        printf("  [%d] %s ... \xe2\x9c\x97\n", toplam_test, ad);
    }
}

/* === Olusturma === */

static void test_basit_olustur(void) {
    Arena *a = arena_olustur(0);
    TipBilgisi *t = tip_olustur_basit(a, TIP_TAM32);
    int ok = t && t->kategori == TIP_TAM32;
    test_sonuc("tip_olustur_basit(TAM32)", ok);
    arena_serbest(a);
}

static void test_referans_olustur(void) {
    Arena *a = arena_olustur(0);
    TipBilgisi *hedef = tip_olustur_basit(a, TIP_TAM32);
    TipBilgisi *t = tip_olustur_referans(a, hedef, 0);
    int ok = t && t->kategori == TIP_REFERANS
          && t->veri.referans.degisken_mi == 0
          && t->veri.referans.hedef == hedef;
    test_sonuc("tip_olustur_referans(&tam32)", ok);
    arena_serbest(a);
}

static void test_referans_degisken(void) {
    Arena *a = arena_olustur(0);
    TipBilgisi *hedef = tip_olustur_basit(a, TIP_TAM64);
    TipBilgisi *t = tip_olustur_referans(a, hedef, 1);
    int ok = t && t->veri.referans.degisken_mi == 1;
    test_sonuc("tip_olustur_referans(&degisken tam64)", ok);
    arena_serbest(a);
}

static void test_pointer_olustur(void) {
    Arena *a = arena_olustur(0);
    TipBilgisi *t = tip_olustur_pointer(a,
        tip_olustur_basit(a, TIP_TAM8));
    int ok = t && t->kategori == TIP_POINTER
          && t->veri.pointer.hedef->kategori == TIP_TAM8;
    test_sonuc("tip_olustur_pointer(*tam8)", ok);
    arena_serbest(a);
}

static void test_dizi_olustur(void) {
    Arena *a = arena_olustur(0);
    TipBilgisi *t = tip_olustur_dizi(a,
        tip_olustur_basit(a, TIP_METIN));
    int ok = t && t->kategori == TIP_DIZI
          && t->veri.dizi.eleman->kategori == TIP_METIN;
    test_sonuc("tip_olustur_dizi(Dizi<metin>)", ok);
    arena_serbest(a);
}

static void test_secimlik_olustur(void) {
    Arena *a = arena_olustur(0);
    TipBilgisi *t = tip_olustur_secimlik(a,
        tip_olustur_basit(a, TIP_TAM32));
    int ok = t && t->kategori == TIP_SECIMLIK
          && t->veri.secimlik.ic->kategori == TIP_TAM32;
    test_sonuc("tip_olustur_secimlik(secimlik<tam32>)", ok);
    arena_serbest(a);
}

static void test_sonuc_olustur(void) {
    Arena *a = arena_olustur(0);
    TipBilgisi *t = tip_olustur_sonuc(a,
        tip_olustur_basit(a, TIP_TAM32),
        tip_olustur_basit(a, TIP_METIN));
    int ok = t && t->kategori == TIP_SONUC
          && t->veri.sonuc.deger->kategori == TIP_TAM32
          && t->veri.sonuc.hata->kategori == TIP_METIN;
    test_sonuc("tip_olustur_sonuc(sonuc<tam32,metin>)", ok);
    arena_serbest(a);
}

static void test_islev_olustur(void) {
    Arena *a = arena_olustur(0);
    TipBilgisi **params = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *) * 2);
    params[0] = tip_olustur_basit(a, TIP_TAM32);
    params[1] = tip_olustur_basit(a, TIP_TAM32);
    TipBilgisi *t = tip_olustur_islev(a, params, 2,
        tip_olustur_basit(a, TIP_TAM32));
    int ok = t && t->kategori == TIP_ISLEV
          && t->veri.islev.param_sayi == 2
          && t->veri.islev.donus->kategori == TIP_TAM32;
    test_sonuc("tip_olustur_islev(islev(tam32,tam32) -> tam32)", ok);
    arena_serbest(a);
}

static void test_yapi_olustur(void) {
    Arena *a = arena_olustur(0);
    TipBilgisi *t = tip_olustur_yapi(a, "Hasta", 5, NULL, 0);
    int ok = t && t->kategori == TIP_YAPI
          && t->veri.yapi.ad_uzunluk == 5
          && memcmp(t->veri.yapi.ad, "Hasta", 5) == 0
          && t->veri.yapi.tip_arg_sayi == 0;
    test_sonuc("tip_olustur_yapi(Hasta)", ok);
    arena_serbest(a);
}

static void test_yapi_generic(void) {
    Arena *a = arena_olustur(0);
    TipBilgisi **args = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
    args[0] = tip_olustur_basit(a, TIP_TAM32);
    TipBilgisi *t = tip_olustur_yapi(a, "Kutu", 4, args, 1);
    int ok = t && t->kategori == TIP_YAPI
          && t->veri.yapi.tip_arg_sayi == 1
          && t->veri.yapi.tip_arg[0]->kategori == TIP_TAM32;
    test_sonuc("tip_olustur_yapi(Kutu<tam32>)", ok);
    arena_serbest(a);
}

/* === Esitlik (nominal) === */

static void test_esit_basit_ayni(void) {
    Arena *a = arena_olustur(0);
    TipBilgisi *t1 = tip_olustur_basit(a, TIP_TAM32);
    TipBilgisi *t2 = tip_olustur_basit(a, TIP_TAM32);
    int ok = tip_esit(t1, t2);
    test_sonuc("esit: tam32 == tam32 (farkli ptr)", ok);
    arena_serbest(a);
}

static void test_esit_basit_farkli(void) {
    Arena *a = arena_olustur(0);
    TipBilgisi *t1 = tip_olustur_basit(a, TIP_TAM32);
    TipBilgisi *t2 = tip_olustur_basit(a, TIP_TAM64);
    int ok = !tip_esit(t1, t2);
    test_sonuc("esit: tam32 != tam64", ok);
    arena_serbest(a);
}

static void test_esit_referans_uyumlu(void) {
    Arena *a = arena_olustur(0);
    TipBilgisi *r1 = tip_olustur_referans(a,
        tip_olustur_basit(a, TIP_TAM32), 0);
    TipBilgisi *r2 = tip_olustur_referans(a,
        tip_olustur_basit(a, TIP_TAM32), 0);
    int ok = tip_esit(r1, r2);
    test_sonuc("esit: &tam32 == &tam32", ok);
    arena_serbest(a);
}

static void test_esit_referans_degisken_farki(void) {
    Arena *a = arena_olustur(0);
    TipBilgisi *r1 = tip_olustur_referans(a,
        tip_olustur_basit(a, TIP_TAM32), 0);
    TipBilgisi *r2 = tip_olustur_referans(a,
        tip_olustur_basit(a, TIP_TAM32), 1);
    int ok = !tip_esit(r1, r2);
    test_sonuc("esit: &tam32 != &degisken tam32", ok);
    arena_serbest(a);
}

static void test_esit_yapi_nominal(void) {
    Arena *a = arena_olustur(0);
    /* Nominal: ayni alanli ama farkli isimli yapilar farkli tip */
    TipBilgisi *hasta = tip_olustur_yapi(a, "Hasta", 5, NULL, 0);
    TipBilgisi *doktor = tip_olustur_yapi(a, "Doktor", 6, NULL, 0);
    int ok = !tip_esit(hasta, doktor);
    test_sonuc("esit: yapi NOMINAL (Hasta != Doktor)", ok);
    arena_serbest(a);
}

static void test_esit_yapi_ayni(void) {
    Arena *a = arena_olustur(0);
    TipBilgisi *h1 = tip_olustur_yapi(a, "Hasta", 5, NULL, 0);
    TipBilgisi *h2 = tip_olustur_yapi(a, "Hasta", 5, NULL, 0);
    int ok = tip_esit(h1, h2);
    test_sonuc("esit: yapi Hasta == Hasta", ok);
    arena_serbest(a);
}

static void test_esit_generic_arg_farki(void) {
    Arena *a = arena_olustur(0);
    /* Kutu<tam32> != Kutu<tam64> */
    TipBilgisi **a32 = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
    a32[0] = tip_olustur_basit(a, TIP_TAM32);
    TipBilgisi *k32 = tip_olustur_yapi(a, "Kutu", 4, a32, 1);

    TipBilgisi **a64 = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
    a64[0] = tip_olustur_basit(a, TIP_TAM64);
    TipBilgisi *k64 = tip_olustur_yapi(a, "Kutu", 4, a64, 1);

    int ok = !tip_esit(k32, k64);
    test_sonuc("esit: Kutu<tam32> != Kutu<tam64>", ok);
    arena_serbest(a);
}

static void test_esit_islev_uyumlu(void) {
    Arena *a = arena_olustur(0);
    TipBilgisi **p1 = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
    p1[0] = tip_olustur_basit(a, TIP_TAM32);
    TipBilgisi *i1 = tip_olustur_islev(a, p1, 1,
        tip_olustur_basit(a, TIP_TAM32));

    TipBilgisi **p2 = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
    p2[0] = tip_olustur_basit(a, TIP_TAM32);
    TipBilgisi *i2 = tip_olustur_islev(a, p2, 1,
        tip_olustur_basit(a, TIP_TAM32));

    int ok = tip_esit(i1, i2);
    test_sonuc("esit: islev(tam32)->tam32 ucu uyumlu", ok);
    arena_serbest(a);
}

static void test_esit_null(void) {
    int ok = !tip_esit(NULL, NULL);  /* NULL guvenli ama esit DEGIL */
    test_sonuc("esit: NULL,NULL guvenli ve farkli", ok);
}

/* === Yardimcilar === */

static void test_sayisal_mi(void) {
    Arena *a = arena_olustur(0);
    int ok = tip_sayisal_mi(tip_olustur_basit(a, TIP_TAM32))
          && tip_sayisal_mi(tip_olustur_basit(a, TIP_KESIRLI64))
          && !tip_sayisal_mi(tip_olustur_basit(a, TIP_METIN))
          && !tip_sayisal_mi(tip_olustur_basit(a, TIP_MANTIKSAL));
    test_sonuc("tip_sayisal_mi (tam/kesirli evet, metin/mantiksal hayir)", ok);
    arena_serbest(a);
}

static void test_tamsayi_mi(void) {
    Arena *a = arena_olustur(0);
    int ok = tip_tamsayi_mi(tip_olustur_basit(a, TIP_TAM32))
          && !tip_tamsayi_mi(tip_olustur_basit(a, TIP_KESIRLI64));
    test_sonuc("tip_tamsayi_mi (tam evet, kesirli hayir)", ok);
    arena_serbest(a);
}

/* === Yazdirma === */

static void test_yazdir_basit(void) {
    Arena *a = arena_olustur(0);
    TipBilgisi *t = tip_olustur_basit(a, TIP_TAM32);
    FILE *bos = fopen("NUL", "w");
    if (!bos) bos = stdout;
    tip_yazdir(t, bos);
    if (bos != stdout) fclose(bos);
    test_sonuc("tip_yazdir basit cokmuyor", 1);
    arena_serbest(a);
}

static void test_yazdir_karmasik(void) {
    Arena *a = arena_olustur(0);
    /* Dizi<secimlik<tam32>> */
    TipBilgisi *t = tip_olustur_dizi(a,
        tip_olustur_secimlik(a,
            tip_olustur_basit(a, TIP_TAM32)));
    FILE *bos = fopen("NUL", "w");
    if (!bos) bos = stdout;
    tip_yazdir(t, bos);
    if (bos != stdout) fclose(bos);
    test_sonuc("tip_yazdir karmasik cokmuyor", 1);
    arena_serbest(a);
}

static void test_yazdir_islev(void) {
    Arena *a = arena_olustur(0);
    TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *) * 2);
    p[0] = tip_olustur_basit(a, TIP_TAM32);
    p[1] = tip_olustur_basit(a, TIP_METIN);
    TipBilgisi *t = tip_olustur_islev(a, p, 2,
        tip_olustur_basit(a, TIP_MANTIKSAL));
    FILE *bos = fopen("NUL", "w");
    if (!bos) bos = stdout;
    tip_yazdir(t, bos);
    if (bos != stdout) fclose(bos);
    test_sonuc("tip_yazdir islev cokmuyor", 1);
    arena_serbest(a);
}

/* === Generic param === */

static void test_generic_param(void) {
    Arena *a = arena_olustur(0);
    TipBilgisi *T = tip_olustur_generic_param(a, "T", 1);
    TipBilgisi *T2 = tip_olustur_generic_param(a, "T", 1);
    TipBilgisi *U = tip_olustur_generic_param(a, "U", 1);
    int ok = tip_esit(T, T2) && !tip_esit(T, U)
          && T->kategori == TIP_GENERIC_PARAM;
    test_sonuc("generic param: T==T, T!=U", ok);
    arena_serbest(a);
}

/* === Stres === */

static void test_stres(void) {
    Arena *a = arena_olustur(0);
    int ok = 1;
    for (int i = 0; i < 1000; i++) {
        TipBilgisi *t = tip_olustur_secimlik(a,
            tip_olustur_dizi(a,
                tip_olustur_basit(a, TIP_TAM32)));
        if (!t || t->kategori != TIP_SECIMLIK) { ok = 0; break; }
    }
    test_sonuc("1000 ic ice tip olustur (ASan sizinti yok)", ok);
    arena_serbest(a);
}

/* === Main === */

int main(void) {
    printf("KEMGU Tip Sistemi Test Paketi\n");
    printf("=============================\n");

    printf("\n--- Olusturma ---\n");
    test_basit_olustur();
    test_referans_olustur();
    test_referans_degisken();
    test_pointer_olustur();
    test_dizi_olustur();
    test_secimlik_olustur();
    test_sonuc_olustur();
    test_islev_olustur();
    test_yapi_olustur();
    test_yapi_generic();

    printf("\n--- Esitlik (Nominal) ---\n");
    test_esit_basit_ayni();
    test_esit_basit_farkli();
    test_esit_referans_uyumlu();
    test_esit_referans_degisken_farki();
    test_esit_yapi_nominal();
    test_esit_yapi_ayni();
    test_esit_generic_arg_farki();
    test_esit_islev_uyumlu();
    test_esit_null();

    printf("\n--- Yardimcilar ---\n");
    test_sayisal_mi();
    test_tamsayi_mi();

    printf("\n--- Yazdirma ---\n");
    test_yazdir_basit();
    test_yazdir_karmasik();
    test_yazdir_islev();

    printf("\n--- Generic Param ---\n");
    test_generic_param();

    printf("\n--- Stres ---\n");
    test_stres();

    printf("\n=============================\n");
    printf("Toplam: %d | Basarili: %d | Basarisiz: %d\n",
           toplam_test, basarili, basarisiz);
    return basarisiz > 0 ? 1 : 0;
}
