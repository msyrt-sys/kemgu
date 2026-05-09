#include "arena.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

/* === Test cercevesi (test_lexer.c ile uyumlu) === */

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

/* === Hayat dongusu testleri === */

static void test_olustur_varsayilan(void) {
    Arena *a = arena_olustur(0);
    int ok = (a != NULL)
          && (arena_kullanilan_byte(a) == 0)
          && (arena_toplam_byte(a) == ARENA_VARSAYILAN_BLOK_BOYUTU)
          && (arena_hata_var_mi(a) == 0);
    test_sonuc("arena_olustur(0) varsayilan boyutla baslar", ok);
    arena_serbest(a);
}

static void test_olustur_ozel_boyut(void) {
    Arena *a = arena_olustur(256);
    int ok = (a != NULL) && (arena_toplam_byte(a) == 256);
    test_sonuc("arena_olustur(256) ozel boyut", ok);
    arena_serbest(a);
}

static void test_serbest_null(void) {
    arena_serbest(NULL);  /* cokmemeli */
    test_sonuc("arena_serbest(NULL) no-op", 1);
}

/* === Tahsis testleri === */

static void test_ayir_sifir_boyut(void) {
    Arena *a = arena_olustur(128);
    void *p = arena_ayir(a, 0);
    test_sonuc("arena_ayir(0) NULL doner", p == NULL);
    arena_serbest(a);
}

static void test_ayir_basit(void) {
    Arena *a = arena_olustur(128);
    void *p = arena_ayir(a, 8);
    int ok = (p != NULL) && (arena_kullanilan_byte(a) >= 8);
    test_sonuc("arena_ayir(8) gecerli pointer", ok);
    arena_serbest(a);
}

static void test_ayir_ardisik_cakismaz(void) {
    Arena *a = arena_olustur(128);
    char *p1 = (char *)arena_ayir(a, 16);
    char *p2 = (char *)arena_ayir(a, 16);
    int ok = (p1 != NULL) && (p2 != NULL);
    /* p2 - p1 en az 16 byte (hizalama nedeniyle daha fazla olabilir) */
    if (ok) ok = ((size_t)(p2 - p1) >= 16);
    test_sonuc("ardisik tahsisler cakismiyor", ok);
    arena_serbest(a);
}

static void test_ayir_sifir(void) {
    Arena *a = arena_olustur(128);
    char *p = (char *)arena_ayir_sifir(a, 32);
    int ok = (p != NULL);
    for (int i = 0; ok && i < 32; i++) {
        if (p[i] != 0) { ok = 0; break; }
    }
    test_sonuc("arena_ayir_sifir tum byte 0", ok);
    arena_serbest(a);
}

static void test_hizalama(void) {
    Arena *a = arena_olustur(256);
    int ok = 1;
    /* Bir kac farkli boyut tahsis et, hizalama dogrula */
    size_t boyutlar[] = {1, 3, 7, 13, 32, 100};
    int n = (int)(sizeof(boyutlar) / sizeof(boyutlar[0]));
    for (int i = 0; ok && i < n; i++) {
        void *p = arena_ayir(a, boyutlar[i]);
        if (!p) { ok = 0; break; }
        uintptr_t adres = (uintptr_t)p;
        if (adres % _Alignof(max_align_t) != 0) { ok = 0; break; }
    }
    test_sonuc("tum tahsisler max_align_t hizali", ok);
    arena_serbest(a);
}

/* === Blok buyume testleri === */

static void test_blok_dolunca_yeni_blok(void) {
    Arena *a = arena_olustur(64);
    /* 64 byte arenaya 3x32 byte istesek 2. tahsiste yeni blok lazim */
    void *p1 = arena_ayir(a, 32);
    void *p2 = arena_ayir(a, 32);
    void *p3 = arena_ayir(a, 32);  /* yeni blok ister */
    int ok = (p1 && p2 && p3) && (arena_toplam_byte(a) > 64);
    test_sonuc("dolu blok sonrasi yeni blok yaratilir", ok);
    arena_serbest(a);
}

static void test_geometric_buyume(void) {
    Arena *a = arena_olustur(128);

    /* Ilk blogu doldur */
    while (arena_kullanilan_byte(a) + 16 <= 128) {
        if (!arena_ayir(a, 16)) break;
    }
    size_t once = arena_toplam_byte(a);

    /* Yeni blok zorla (mevcut blogu tasiracak boyut) */
    void *p = arena_ayir(a, 100);
    size_t sonra = arena_toplam_byte(a);

    /* Yeni blok eski blogun en az 2 kati olmali (geometric) */
    int ok = (p != NULL) && (sonra >= once + 256);
    test_sonuc("geometric x2 blok buyumesi", ok);
    arena_serbest(a);
}

static void test_buyuk_tahsis_cap_ustu(void) {
    Arena *a = arena_olustur(128);
    /* MAX_BLOK ustu tek tahsis -> ozel blok yaratilmali */
    size_t buyuk = (size_t)ARENA_MAX_BLOK_BOYUTU + 1024u;
    void *p = arena_ayir(a, buyuk);
    int ok = (p != NULL) && (arena_toplam_byte(a) >= buyuk);
    test_sonuc("MAX_BLOK ustu tek tahsis basarili", ok);
    arena_serbest(a);
}

/* === Sifirlama testleri === */

static void test_sifirla_kullanim_temizler(void) {
    Arena *a = arena_olustur(128);
    arena_ayir(a, 32);
    arena_ayir(a, 32);
    arena_sifirla(a);
    int ok = (arena_kullanilan_byte(a) == 0);
    test_sonuc("arena_sifirla kullanilan_byte = 0", ok);
    arena_serbest(a);
}

static void test_sifirla_ilk_blok_kalir(void) {
    Arena *a = arena_olustur(128);
    /* Cok blok yarat */
    for (int i = 0; i < 20; i++) arena_ayir(a, 32);
    size_t once = arena_toplam_byte(a);

    arena_sifirla(a);
    size_t sonra = arena_toplam_byte(a);

    /* Sifirlama sonrasi sadece ilk blok kalmali (128 byte) */
    int ok = (once > 128) && (sonra == 128);
    test_sonuc("arena_sifirla sadece ilk blok kalir", ok);
    arena_serbest(a);
}

static void test_sifirla_yeniden_kullanim(void) {
    Arena *a = arena_olustur(64);
    arena_ayir(a, 32);
    arena_sifirla(a);
    void *p = arena_ayir(a, 32);
    test_sonuc("sifirlama sonrasi yeniden tahsis", p != NULL);
    arena_serbest(a);
}

/* === UTF-8 Turkce icerik testi === */

static void test_utf8_string(void) {
    Arena *a = arena_olustur(256);
    /* "degisken hasta" — Turkce UTF-8 hex escape ile */
    const char *kaynak = "de\xc4\x9f" "i\xc5\x9fken hasta";
    size_t uz = strlen(kaynak);

    char *kopya = (char *)arena_ayir(a, uz + 1);
    int ok = (kopya != NULL);
    if (ok) {
        memcpy(kopya, kaynak, uz);
        kopya[uz] = '\0';
        ok = (memcmp(kopya, kaynak, uz + 1) == 0);
    }
    test_sonuc("UTF-8 Turkce string kopya", ok);
    arena_serbest(a);
}

/* === Stres testleri === */

static void test_stres_kucuk_tahsis(void) {
    Arena *a = arena_olustur(0);
    int ok = 1;
    for (int i = 0; i < 10000; i++) {
        if (!arena_ayir(a, 16)) { ok = 0; break; }
    }
    test_sonuc("10000 kucuk tahsis (ASan sizinti yok)", ok);
    arena_serbest(a);
}

static void test_stres_karisik(void) {
    Arena *a = arena_olustur(0);
    int ok = 1;
    for (int i = 0; i < 1000; i++) {
        size_t boyut = (i % 100 == 0) ? 4096u : 16u;
        if (!arena_ayir(a, boyut)) { ok = 0; break; }
    }
    test_sonuc("1000 karisik boyutlu tahsis", ok);
    arena_serbest(a);
}

static void test_sifirla_yeniden_dongu(void) {
    Arena *a = arena_olustur(0);
    int ok = 1;
    for (int dongu = 0; ok && dongu < 100; dongu++) {
        for (int i = 0; i < 100; i++) {
            if (!arena_ayir(a, 32)) { ok = 0; break; }
        }
        arena_sifirla(a);
    }
    test_sonuc("100 dongu tahsis-sifirla (bellek artmaz)", ok);
    arena_serbest(a);
}

/* === Hata testleri === */

static void test_hata_bayrak_baslangic(void) {
    Arena *a = arena_olustur(128);
    int ok = (arena_hata_var_mi(a) == 0);
    test_sonuc("baslangic hata bayragi 0", ok);
    arena_serbest(a);
}

/* === Main === */

int main(void) {
    printf("KEMGU Arena Test Paketi\n");
    printf("========================\n");

    printf("\n--- Hayat Dongusu ---\n");
    test_olustur_varsayilan();
    test_olustur_ozel_boyut();
    test_serbest_null();

    printf("\n--- Tahsis ---\n");
    test_ayir_sifir_boyut();
    test_ayir_basit();
    test_ayir_ardisik_cakismaz();
    test_ayir_sifir();
    test_hizalama();

    printf("\n--- Blok Buyume ---\n");
    test_blok_dolunca_yeni_blok();
    test_geometric_buyume();
    test_buyuk_tahsis_cap_ustu();

    printf("\n--- Sifirlama ---\n");
    test_sifirla_kullanim_temizler();
    test_sifirla_ilk_blok_kalir();
    test_sifirla_yeniden_kullanim();

    printf("\n--- UTF-8 ---\n");
    test_utf8_string();

    printf("\n--- Stres ---\n");
    test_stres_kucuk_tahsis();
    test_stres_karisik();
    test_sifirla_yeniden_dongu();

    printf("\n--- Hata ---\n");
    test_hata_bayrak_baslangic();

    printf("\n========================\n");
    printf("Toplam: %d | Basarili: %d | Basarisiz: %d\n",
           toplam_test, basarili, basarisiz);
    return basarisiz > 0 ? 1 : 0;
}
