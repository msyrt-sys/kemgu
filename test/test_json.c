#include "json.h"
#include "arena.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* === Parsing testleri === */

static void test_null(void) {
    Arena *a = arena_olustur(0);
    const char *h = NULL;
    JsonDeger *d = json_ayrist(a, "null", 4, &h);
    int ok = d && d->tip == JSON_NULL && !h;
    test_sonuc("null", ok);
    arena_serbest(a);
}

static void test_bool_dogru(void) {
    Arena *a = arena_olustur(0);
    JsonDeger *d = json_ayrist(a, "true", 4, NULL);
    int ok = d && d->tip == JSON_BOOL && d->veri.bool_deger == 1;
    test_sonuc("true", ok);
    arena_serbest(a);
}

static void test_bool_yanlis(void) {
    Arena *a = arena_olustur(0);
    JsonDeger *d = json_ayrist(a, "false", 5, NULL);
    int ok = d && d->tip == JSON_BOOL && d->veri.bool_deger == 0;
    test_sonuc("false", ok);
    arena_serbest(a);
}

static void test_tamsayi(void) {
    Arena *a = arena_olustur(0);
    JsonDeger *d = json_ayrist(a, "42", 2, NULL);
    int ok = d && d->tip == JSON_TAMSAYI && d->veri.tamsayi == 42;
    test_sonuc("tamsayi 42", ok);
    arena_serbest(a);
}

static void test_negatif(void) {
    Arena *a = arena_olustur(0);
    JsonDeger *d = json_ayrist(a, "-123", 4, NULL);
    int ok = d && d->veri.tamsayi == -123;
    test_sonuc("negatif -123", ok);
    arena_serbest(a);
}

static void test_kesirli(void) {
    Arena *a = arena_olustur(0);
    JsonDeger *d = json_ayrist(a, "3.14", 4, NULL);
    int ok = d && d->tip == JSON_KESIRLI;
    test_sonuc("kesirli 3.14", ok);
    arena_serbest(a);
}

static void test_metin(void) {
    Arena *a = arena_olustur(0);
    JsonDeger *d = json_ayrist(a, "\"merhaba\"", 9, NULL);
    int ok = d && d->tip == JSON_METIN
          && d->veri.str.uzunluk == 7
          && strcmp(d->veri.str.metin, "merhaba") == 0;
    test_sonuc("metin \"merhaba\"", ok);
    arena_serbest(a);
}

static void test_metin_escape(void) {
    Arena *a = arena_olustur(0);
    /* "a\nb\tc\"d\\e" — \n -> newline, \t -> tab, \" -> ", \\ -> \ */
    const char *src = "\"a\\nb\\tc\\\"d\\\\e\"";
    JsonDeger *d = json_ayrist(a, src, (int)strlen(src), NULL);
    int ok = d && d->tip == JSON_METIN
          && d->veri.str.uzunluk == 9
          && memcmp(d->veri.str.metin, "a\nb\tc\"d\\e", 9) == 0;
    test_sonuc("metin escape (\\n \\t \\\" \\\\)", ok);
    arena_serbest(a);
}

static void test_metin_uXXXX(void) {
    Arena *a = arena_olustur(0);
    /* i + ş (2 byte) + lev = 6 byte */
    const char *src = "\"i\\u015flev\"";
    JsonDeger *d = json_ayrist(a, src, (int)strlen(src), NULL);
    int ok = d && d->tip == JSON_METIN
          && d->veri.str.uzunluk == 6
          && memcmp(d->veri.str.metin, "i\xc5\x9flev", 6) == 0;
    test_sonuc("metin \\u015f -> UTF-8 (s)", ok);
    arena_serbest(a);
}

static void test_metin_uXXXX_ascii(void) {
    Arena *a = arena_olustur(0);
    /* A -> 'A' (ascii) */
    const char *src = "\"\\u0041BC\"";
    JsonDeger *d = json_ayrist(a, src, (int)strlen(src), NULL);
    int ok = d && d->veri.str.uzunluk == 3
          && memcmp(d->veri.str.metin, "ABC", 3) == 0;
    test_sonuc("metin \\u0041 -> 'A' (ASCII)", ok);
    arena_serbest(a);
}

static void test_metin_utf8(void) {
    Arena *a = arena_olustur(0);
    /* Turkce karakter aktarimi */
    const char *src = "\"de\xc4\x9f" "er\"";
    JsonDeger *d = json_ayrist(a, src, (int)strlen(src), NULL);
    int ok = d && d->tip == JSON_METIN
          && memcmp(d->veri.str.metin, "de\xc4\x9f" "er", 6) == 0;
    test_sonuc("metin UTF-8 (Turkce karakter)", ok);
    arena_serbest(a);
}

static void test_bos_dizi(void) {
    Arena *a = arena_olustur(0);
    JsonDeger *d = json_ayrist(a, "[]", 2, NULL);
    int ok = d && d->tip == JSON_DIZI && d->veri.dizi.sayi == 0;
    test_sonuc("bos dizi []", ok);
    arena_serbest(a);
}

static void test_dizi_sayisal(void) {
    Arena *a = arena_olustur(0);
    JsonDeger *d = json_ayrist(a, "[1, 2, 3]", 9, NULL);
    int ok = d && d->veri.dizi.sayi == 3
          && d->veri.dizi.elemanlar[0]->veri.tamsayi == 1
          && d->veri.dizi.elemanlar[2]->veri.tamsayi == 3;
    test_sonuc("dizi [1,2,3]", ok);
    arena_serbest(a);
}

static void test_bos_nesne(void) {
    Arena *a = arena_olustur(0);
    JsonDeger *d = json_ayrist(a, "{}", 2, NULL);
    int ok = d && d->tip == JSON_NESNE && d->veri.nesne.alan_sayi == 0;
    test_sonuc("bos nesne {}", ok);
    arena_serbest(a);
}

static void test_nesne_alanli(void) {
    Arena *a = arena_olustur(0);
    const char *src = "{\"ad\":\"Ali\",\"yas\":30}";
    JsonDeger *d = json_ayrist(a, src, (int)strlen(src), NULL);
    int ok = d && d->veri.nesne.alan_sayi == 2;
    if (ok) {
        JsonDeger *ad = json_alan(d, "ad");
        JsonDeger *yas = json_alan(d, "yas");
        ok = ad && yas && yas->veri.tamsayi == 30
          && strcmp(ad->veri.str.metin, "Ali") == 0;
    }
    test_sonuc("nesne 2 alan", ok);
    arena_serbest(a);
}

static void test_ic_ice(void) {
    Arena *a = arena_olustur(0);
    const char *src =
        "{\"params\":{\"textDocument\":{\"uri\":\"file:///x\"}}}";
    JsonDeger *d = json_ayrist(a, src, (int)strlen(src), NULL);
    int ok = d != NULL;
    if (ok) {
        JsonDeger *p = json_alan(d, "params");
        JsonDeger *td = json_alan(p, "textDocument");
        JsonDeger *uri = json_alan(td, "uri");
        int uz = 0;
        const char *s = json_metin(uri, &uz);
        ok = s && strcmp(s, "file:///x") == 0;
    }
    test_sonuc("ic ice nesne (LSP-tarzi)", ok);
    arena_serbest(a);
}

static void test_hatali_parse(void) {
    Arena *a = arena_olustur(0);
    const char *h = NULL;
    JsonDeger *d = json_ayrist(a, "{ad: 1}", 7, &h);
    int ok = d == NULL && h != NULL;  /* key tirnaksiz, hata */
    test_sonuc("hatali parse: tirnaksiz key", ok);
    arena_serbest(a);
}

/* === Yazici testleri === */

static void test_yazici_basit(void) {
    JsonYazici y;
    json_yazici_baslat(&y);
    json_yaz(&y, "{\"a\":");
    json_yaz_int(&y, 42);
    json_yaz(&y, "}");
    int ok = strcmp(y.tampon, "{\"a\":42}") == 0;
    test_sonuc("yazici basit", ok);
    json_yazici_serbest(&y);
}

static void test_yazici_metin_escape(void) {
    JsonYazici y;
    json_yazici_baslat(&y);
    json_yaz_metin_lit(&y, "a\"b\\c\nd");
    /* Beklenen: "a\"b\\c\nd" */
    int ok = strcmp(y.tampon, "\"a\\\"b\\\\c\\nd\"") == 0;
    test_sonuc("yazici metin escape", ok);
    json_yazici_serbest(&y);
}

static void test_yazici_utf8(void) {
    JsonYazici y;
    json_yazici_baslat(&y);
    /* UTF-8 byte aktarimi (escape edilmez) */
    json_yaz_metin_lit(&y, "de\xc4\x9f" "er");
    int ok = strcmp(y.tampon, "\"de\xc4\x9f" "er\"") == 0;
    test_sonuc("yazici UTF-8 aktarir", ok);
    json_yazici_serbest(&y);
}

static void test_roundtrip(void) {
    /* Yaz, sonra parse et — esit cikmali */
    Arena *a = arena_olustur(0);
    JsonYazici y;
    json_yazici_baslat(&y);
    json_yaz(&y, "{\"uri\":");
    json_yaz_metin_lit(&y, "file:///c:/test.kem");
    json_yaz(&y, ",\"sayi\":");
    json_yaz_int(&y, -42);
    json_yaz(&y, "}");
    JsonDeger *d = json_ayrist(a, y.tampon, (int)y.kullanilan, NULL);
    int ok = d && d->veri.nesne.alan_sayi == 2;
    if (ok) {
        JsonDeger *uri = json_alan(d, "uri");
        JsonDeger *s = json_alan(d, "sayi");
        ok = uri && s && s->veri.tamsayi == -42
          && strcmp(uri->veri.str.metin, "file:///c:/test.kem") == 0;
    }
    test_sonuc("roundtrip yaz->parse", ok);
    json_yazici_serbest(&y);
    arena_serbest(a);
}

int main(void) {
    printf("KEMGU JSON Test Paketi\n");
    printf("=======================\n");

    printf("\n--- Temel tipler ---\n");
    test_null();
    test_bool_dogru();
    test_bool_yanlis();
    test_tamsayi();
    test_negatif();
    test_kesirli();

    printf("\n--- Metin ---\n");
    test_metin();
    test_metin_escape();
    test_metin_uXXXX();
    test_metin_uXXXX_ascii();
    test_metin_utf8();

    printf("\n--- Yapilar ---\n");
    test_bos_dizi();
    test_dizi_sayisal();
    test_bos_nesne();
    test_nesne_alanli();
    test_ic_ice();

    printf("\n--- Hata ---\n");
    test_hatali_parse();

    printf("\n--- Yazici ---\n");
    test_yazici_basit();
    test_yazici_metin_escape();
    test_yazici_utf8();
    test_roundtrip();

    printf("\n=======================\n");
    printf("Toplam: %d | Basarili: %d | Basarisiz: %d\n",
           toplam_test, basarili, basarisiz);
    return basarisiz > 0 ? 1 : 0;
}
