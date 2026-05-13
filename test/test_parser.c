#include "parser.h"
#include "lexer.h"
#include "ast.h"
#include "ast_yazdir.h"
#include "arena.h"

#include <stdio.h>
#include <stdlib.h>     /* malloc, free (ornek dosya parse testleri) */
#include <string.h>
#include <stdint.h>

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

/* Yardimci: kaynaktan AST uret. hata_sayisi NULL olabilir. */
static Dugum *parse_kaynak(const char *kaynak, Arena *a, int *hata_sayisi) {
    Lexer l;
    lexer_baslat(&l, kaynak, "test");
    Parser p;
    parser_baslat(&p, &l, a, "test", kaynak);
    Dugum *prog = parser_calistir(&p);
    if (hata_sayisi) *hata_sayisi = p.hata_sayisi;
    return prog;
}

/* === Bos / Tek ust oge === */

static void test_bos_program(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    Dugum *prog = parse_kaynak("", a, &hata);
    int ok = prog != NULL
          && prog->tip == DUGUM_PROGRAM
          && prog->veri.program.sayi == 0
          && hata == 0;
    test_sonuc("bos program", ok);
    arena_serbest(a);
}

/* === Islev tanimlari === */

static void test_islev_bos(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* "islev f() {}" — Turkce: i\xc5\x9flev */
    Dugum *prog = parse_kaynak("i\xc5\x9flev f() {}", a, &hata);
    int ok = prog && prog->veri.program.sayi == 1
          && prog->veri.program.uyeler[0]->tip == DUGUM_ISLEV
          && hata == 0;
    if (ok) {
        Dugum *islev = prog->veri.program.uyeler[0];
        ok = (islev->veri.islev.param_sayi == 0)
          && (islev->veri.islev.govde != NULL)
          && (islev->veri.islev.govde->tip == DUGUM_BLOK)
          && (islev->veri.islev.govde->veri.blok.sayi == 0);
    }
    test_sonuc("islev bos govde", ok);
    arena_serbest(a);
}

static void test_islev_parametreli(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* "islev topla(a: tam32, b: tam32) -> tam32 { ver 0; }" */
    const char *kaynak =
        "i\xc5\x9flev topla(a: tam32, b: tam32) -> tam32 { ver 0; }";
    Dugum *prog = parse_kaynak(kaynak, a, &hata);
    int ok = prog && prog->veri.program.sayi == 1 && hata == 0;
    if (ok) {
        Dugum *islev = prog->veri.program.uyeler[0];
        ok = (islev->tip == DUGUM_ISLEV)
          && (islev->veri.islev.param_sayi == 2)
          && (islev->veri.islev.donus_tipi != NULL)
          && (islev->veri.islev.donus_tipi->tip == DUGUM_TIP_BASIT)
          && (islev->veri.islev.govde != NULL)
          && (islev->veri.islev.govde->veri.blok.sayi == 1);
    }
    test_sonuc("islev parametreli + donus tipi", ok);
    arena_serbest(a);
}

static void test_islev_ad_dogru(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    Dugum *prog = parse_kaynak("i\xc5\x9flev fibonacci() {}", a, &hata);
    int ok = prog && prog->veri.program.sayi == 1 && hata == 0;
    if (ok) {
        Dugum *islev = prog->veri.program.uyeler[0];
        ok = strcmp(islev->veri.islev.ad, "fibonacci") == 0;
    }
    test_sonuc("islev adi 'fibonacci'", ok);
    arena_serbest(a);
}

/* === Yapi tanimi === */

static void test_yapi_bos(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* "yapi Hasta {}" */
    Dugum *prog = parse_kaynak("yap\xc4\xb1 Hasta {}", a, &hata);
    int ok = prog && prog->veri.program.sayi == 1 && hata == 0;
    if (ok) {
        Dugum *yapi = prog->veri.program.uyeler[0];
        ok = (yapi->tip == DUGUM_YAPI)
          && (yapi->veri.yapi.alan_sayi == 0)
          && (strcmp(yapi->veri.yapi.ad, "Hasta") == 0);
    }
    test_sonuc("yapi bos", ok);
    arena_serbest(a);
}

static void test_yapi_alanli(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* "yapi Hasta { ad: metin; yas: tam32; }" */
    const char *kaynak = "yap\xc4\xb1 Hasta { ad: metin; yas: tam32; }";
    Dugum *prog = parse_kaynak(kaynak, a, &hata);
    int ok = prog && prog->veri.program.sayi == 1 && hata == 0;
    if (ok) {
        Dugum *yapi = prog->veri.program.uyeler[0];
        ok = (yapi->tip == DUGUM_YAPI)
          && (yapi->veri.yapi.alan_sayi == 2);
        if (ok) {
            Dugum *a1 = yapi->veri.yapi.alanlar[0];
            Dugum *a2 = yapi->veri.yapi.alanlar[1];
            ok = (a1->tip == DUGUM_ALAN)
              && (strcmp(a1->veri.alan.ad, "ad") == 0)
              && (a2->tip == DUGUM_ALAN)
              && (strcmp(a2->veri.alan.ad, "yas") == 0);
        }
    }
    test_sonuc("yapi 2 alan", ok);
    arena_serbest(a);
}

/* === Sabit === */

static void test_sabit(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* "sabit PI: tam32 = 3;" */
    Dugum *prog = parse_kaynak("sabit PI: tam32 = 3;", a, &hata);
    int ok = prog && prog->veri.program.sayi == 1 && hata == 0;
    if (ok) {
        Dugum *s = prog->veri.program.uyeler[0];
        ok = (s->tip == DUGUM_SABIT)
          && (strcmp(s->veri.sabit.ad, "PI") == 0)
          && (s->veri.sabit.tip != NULL)
          && (s->veri.sabit.deger != NULL)
          && (s->veri.sabit.deger->tip == DUGUM_TAM)
          && (s->veri.sabit.deger->veri.tam.deger == 3);
    }
    test_sonuc("sabit PI: tam32 = 3", ok);
    arena_serbest(a);
}

/* === Kullan === */

static void test_kullan_basit(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* "kullan std;" */
    Dugum *prog = parse_kaynak("kullan std;", a, &hata);
    int ok = prog && prog->veri.program.sayi == 1 && hata == 0;
    if (ok) {
        Dugum *k = prog->veri.program.uyeler[0];
        ok = (k->tip == DUGUM_KULLAN)
          && (strcmp(k->veri.kullan.yol, "std") == 0);
    }
    test_sonuc("kullan std;", ok);
    arena_serbest(a);
}

static void test_kullan_yol(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* "kullan std::dosya::oku;" */
    Dugum *prog = parse_kaynak("kullan std::dosya::oku;", a, &hata);
    int ok = prog && prog->veri.program.sayi == 1 && hata == 0;
    if (ok) {
        Dugum *k = prog->veri.program.uyeler[0];
        ok = (k->tip == DUGUM_KULLAN)
          && (strcmp(k->veri.kullan.yol, "std::dosya::oku") == 0);
    }
    test_sonuc("kullan std::dosya::oku;", ok);
    arena_serbest(a);
}

/* === Modul === */

static void test_modul_bos(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* "modul matematik {}" */
    Dugum *prog = parse_kaynak("mod\xc3\xbcl matematik {}", a, &hata);
    int ok = prog && prog->veri.program.sayi == 1 && hata == 0;
    if (ok) {
        Dugum *m = prog->veri.program.uyeler[0];
        ok = (m->tip == DUGUM_MODUL)
          && (m->veri.modul.sayi == 0);
    }
    test_sonuc("modul bos", ok);
    arena_serbest(a);
}

static void test_modul_iceriger(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* "modul m { yapi X {} }" */
    const char *kaynak = "mod\xc3\xbcl m { yap\xc4\xb1 X {} }";
    Dugum *prog = parse_kaynak(kaynak, a, &hata);
    int ok = prog && prog->veri.program.sayi == 1 && hata == 0;
    if (ok) {
        Dugum *m = prog->veri.program.uyeler[0];
        ok = (m->tip == DUGUM_MODUL) && (m->veri.modul.sayi == 1)
          && (m->veri.modul.uyeler[0]->tip == DUGUM_YAPI);
    }
    test_sonuc("modul icinde yapi", ok);
    arena_serbest(a);
}

/* === Disa === */

static void test_disa_islev(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* "disa islev f() {}" */
    Dugum *prog = parse_kaynak("d\xc4\xb1\xc5\x9f" "a i\xc5\x9flev f() {}",
                               a, &hata);
    int ok = prog && prog->veri.program.sayi == 1 && hata == 0;
    if (ok) {
        Dugum *d = prog->veri.program.uyeler[0];
        ok = (d->tip == DUGUM_DISA)
          && (d->veri.disa.tanim != NULL)
          && (d->veri.disa.tanim->tip == DUGUM_ISLEV);
    }
    test_sonuc("disa islev f()", ok);
    arena_serbest(a);
}

/* === Deyimler (islev govdesinde) === */

static void test_degisken_deyimi(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* "islev f() { degisken x = 42; }" */
    const char *kaynak =
        "i\xc5\x9flev f() { de\xc4\x9fi\xc5\x9fken x = 42; }";
    Dugum *prog = parse_kaynak(kaynak, a, &hata);
    int ok = prog && hata == 0;
    if (ok) {
        Dugum *blok = prog->veri.program.uyeler[0]->veri.islev.govde;
        ok = blok && blok->veri.blok.sayi == 1
          && blok->veri.blok.deyimler[0]->tip == DUGUM_DEGISKEN
          && blok->veri.blok.deyimler[0]->veri.degisken.deger->tip == DUGUM_TAM
          && blok->veri.blok.deyimler[0]->veri.degisken.deger->veri.tam.deger == 42;
    }
    test_sonuc("islev icinde degisken x = 42", ok);
    arena_serbest(a);
}

static void test_degisken_tip_belirt(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* "islev f() { degisken x: tam32 = 0; }" */
    const char *kaynak =
        "i\xc5\x9flev f() { de\xc4\x9fi\xc5\x9fken x: tam32 = 0; }";
    Dugum *prog = parse_kaynak(kaynak, a, &hata);
    int ok = prog && hata == 0;
    if (ok) {
        Dugum *deg = prog->veri.program.uyeler[0]
                         ->veri.islev.govde
                         ->veri.blok.deyimler[0];
        ok = (deg->tip == DUGUM_DEGISKEN)
          && (deg->veri.degisken.tip != NULL)
          && (deg->veri.degisken.tip->tip == DUGUM_TIP_BASIT);
    }
    test_sonuc("degisken x: tam32 = 0", ok);
    arena_serbest(a);
}

static void test_ver_deger(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* "islev f() { ver 7; }" */
    Dugum *prog = parse_kaynak("i\xc5\x9flev f() { ver 7; }", a, &hata);
    int ok = prog && hata == 0;
    if (ok) {
        Dugum *blok = prog->veri.program.uyeler[0]->veri.islev.govde;
        Dugum *ver = blok->veri.blok.deyimler[0];
        ok = (ver->tip == DUGUM_VER)
          && (ver->veri.ver.deger != NULL)
          && (ver->veri.ver.deger->tip == DUGUM_TAM)
          && (ver->veri.ver.deger->veri.tam.deger == 7);
    }
    test_sonuc("ver 7;", ok);
    arena_serbest(a);
}

static void test_ver_bos(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* "islev f() { ver; }" */
    Dugum *prog = parse_kaynak("i\xc5\x9flev f() { ver; }", a, &hata);
    int ok = prog && hata == 0;
    if (ok) {
        Dugum *ver = prog->veri.program.uyeler[0]
                         ->veri.islev.govde
                         ->veri.blok.deyimler[0];
        ok = (ver->tip == DUGUM_VER) && (ver->veri.ver.deger == NULL);
    }
    test_sonuc("ver; (degersiz)", ok);
    arena_serbest(a);
}

static void test_ic_ice_blok(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* "islev f() { { ver 1; } }" */
    Dugum *prog = parse_kaynak("i\xc5\x9flev f() { { ver 1; } }", a, &hata);
    int ok = prog && hata == 0;
    if (ok) {
        Dugum *dis = prog->veri.program.uyeler[0]->veri.islev.govde;
        ok = (dis->veri.blok.sayi == 1)
          && (dis->veri.blok.deyimler[0]->tip == DUGUM_BLOK);
    }
    test_sonuc("ic ice blok", ok);
    arena_serbest(a);
}

/* === Sayi literalleri === */

static void test_sayi_hex(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    Dugum *prog = parse_kaynak("sabit X: tam32 = 0xFF;", a, &hata);
    int ok = prog && hata == 0
          && prog->veri.program.uyeler[0]->veri.sabit.deger->veri.tam.deger == 255;
    test_sonuc("0xFF -> 255", ok);
    arena_serbest(a);
}

static void test_sayi_binary(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    Dugum *prog = parse_kaynak("sabit X: tam32 = 0b1010;", a, &hata);
    int ok = prog && hata == 0
          && prog->veri.program.uyeler[0]->veri.sabit.deger->veri.tam.deger == 10;
    test_sonuc("0b1010 -> 10", ok);
    arena_serbest(a);
}

static void test_sayi_underscore(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    Dugum *prog = parse_kaynak("sabit X: tam32 = 1_000_000;", a, &hata);
    int ok = prog && hata == 0
          && prog->veri.program.uyeler[0]->veri.sabit.deger->veri.tam.deger
             == 1000000;
    test_sonuc("1_000_000 -> 1000000", ok);
    arena_serbest(a);
}

/* === Cogul ust oge === */

static void test_cogul_ust_oge(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* "kullan std; yapi X {} islev f() {}" */
    const char *kaynak =
        "kullan std; yap\xc4\xb1 X {} i\xc5\x9flev f() {}";
    Dugum *prog = parse_kaynak(kaynak, a, &hata);
    int ok = prog && hata == 0
          && prog->veri.program.sayi == 3
          && prog->veri.program.uyeler[0]->tip == DUGUM_KULLAN
          && prog->veri.program.uyeler[1]->tip == DUGUM_YAPI
          && prog->veri.program.uyeler[2]->tip == DUGUM_ISLEV;
    test_sonuc("3 ust oge sira", ok);
    arena_serbest(a);
}

/* === Hata kurtarma === */

static void test_hata_eksik_noktali(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* eksik ; "kullan std" */
    Dugum *prog = parse_kaynak("kullan std", a, &hata);
    int ok = prog && hata > 0;
    test_sonuc("hata: eksik ';' (rapor verir)", ok);
    arena_serbest(a);
}

static void test_hata_kurtarma_devam(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* Ilk yapida hata, ikinci ust oge yine de parse edilmeli */
    /* "yapi X { ad: ; } yapi Y { yas: tam32; }" — ilkte tip eksik */
    const char *kaynak =
        "yap\xc4\xb1 X { ad: ; } yap\xc4\xb1 Y { yas: tam32; }";
    Dugum *prog = parse_kaynak(kaynak, a, &hata);
    int ok = prog && hata > 0
          && prog->veri.program.sayi >= 2;
    test_sonuc("hata sonrasi parser devam ediyor", ok);
    arena_serbest(a);
}

static void test_hata_max_dur(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* Cok hatali kaynak — parser PARSER_MAX_HATA'da durmali.
     * Az sayida ama yeterli token — sonsuz hata uretmemeli. */
    char buyuk[2048];
    int n = 0;
    for (int i = 0; i < 30; i++) {
        n += snprintf(buyuk + n, sizeof(buyuk) - (size_t)n, "? ");
    }
    Dugum *prog = parse_kaynak(buyuk, a, &hata);
    int ok = prog && hata <= PARSER_MAX_HATA;
    test_sonuc("PARSER_MAX_HATA korumasi", ok);
    arena_serbest(a);
}

/* === Sonsuz loop koruma (ayni-pozisyon-ayni-kod tekrari) === */

/* parse_yapi_tanimi: govde icinde sync keyword (TOK_KULLAN/TOK_MODUL vb.)
 * varsa parse_alan -> P018 -> parser_panik_sync -> keyword'u tuketmez
 * -> outer loop ayni pozisyonda yine parse_alan -> SONSUZ LOOP eski hali.
 * Yeni: parser_hata ayni-pozisyon esigi asilinca hata_sayisi >= MAX
 * yaparak inner loop'lari kirar; ayrica parse_yapi_tanimi loop guard'i
 * gucu zorla ilerletir. */
static void test_parser_yapi_keyword_sonsuz_loop_yok(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* "yapi P { kullan modul disa }" — govde icinde sync keyword'ler */
    const char *kaynak =
        "yap\xc4\xb1 P { kullan mod\xc3\xbcl d\xc4\xb1\xc5\x9f" "a }";
    Dugum *prog = parse_kaynak(kaynak, a, &hata);
    /* Crash/hang olmamasi yeterli. Hata sayisi MAX altinda kalmali
     * (eski bugda MAX'a yapisik kaliyordu cunku loop bitmiyordu). */
    int ok = prog != NULL && hata >= 1 && hata <= PARSER_MAX_HATA;
    test_sonuc("yapi govdesi sync keyword sonsuz loop yok", ok);
    arena_serbest(a);
}

/* Random token streams: cok kategorili karisik bozuk ifadeler — parser
 * MAX_AYNI_HATA esigine sahip oldugu icin durmali. */
static void test_parser_ayni_pozisyon_tekrar_durur(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* "yapi X { ( + ( + ( + ( + ( + }" — ayni token sirf
     * parser_panik_sync sonrasi atlanmazsa ayni pozisyonda kalir */
    const char *kaynak =
        "yap\xc4\xb1 X { ( + ( + ( + ( + ( + ( + ( + ( + }";
    Dugum *prog = parse_kaynak(kaynak, a, &hata);
    int ok = prog != NULL && hata <= PARSER_MAX_HATA;
    test_sonuc("ayni-pozisyon-ayni-kod tekrari durur", ok);
    arena_serbest(a);
}

/* Mod a fuzz benzeri: cok kategorili rastgele karisik token stream
 * — parser crash etmemeli, MAX'a sapmamali. */
static void test_parser_karisik_token_stream(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* Lexer + parser keyword-icin-keyword karisigi — eski bugda P018
     * kilitleniyordu */
    const char *kaynak =
        "i\xc5\x9flev yap\xc4\xb1 ver e\xc4\x9f" "er { ( + ; } [ ] mod\xc3\xbcl"
        " kullan d\xc4\xb1\xc5\x9f" "a sabit -> ::";
    Dugum *prog = parse_kaynak(kaynak, a, &hata);
    int ok = prog != NULL && hata <= PARSER_MAX_HATA;
    test_sonuc("karisik token stream crash etmiyor", ok);
    arena_serbest(a);
}

/* Buyuk hata akisinda parser cokmemeli ve MAX'i asmamali.
 * Onceki testte parser sonsuz loop'a giriyordu; simdi PARSER_MAX_AYNI_HATA
 * esigi + MAX_HATA esigi onu garantiliyor. */
static void test_parser_farkli_pozisyon_devam(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* "yapi P { 1 1 1 ... }" — yapi govdesinde sayisal literal'lar
     * her biri farkli pozisyonda P018 hatasi vermeli */
    char buyuk[4096];
    int n = 0;
    n += snprintf(buyuk + n, sizeof(buyuk) - (size_t)n,
                  "yap\xc4\xb1 P { ");
    for (int i = 0; i < 300; i++) {
        n += snprintf(buyuk + n, sizeof(buyuk) - (size_t)n, "%d ", i);
    }
    n += snprintf(buyuk + n, sizeof(buyuk) - (size_t)n, "}");
    Dugum *prog = parse_kaynak(buyuk, a, &hata);
    int ok = prog != NULL && hata >= 1 && hata <= PARSER_MAX_HATA;
    test_sonuc("farkli-pozisyon hatalari MAX altinda tutulur", ok);
    arena_serbest(a);
}

/* === Atama vs ifade_deyimi === */

static void test_atama_deyimi(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* "islev f() { x = 5; }" */
    Dugum *prog = parse_kaynak("i\xc5\x9flev f() { x = 5; }", a, &hata);
    int ok = prog && hata == 0;
    if (ok) {
        Dugum *deyim = prog->veri.program.uyeler[0]
                           ->veri.islev.govde
                           ->veri.blok.deyimler[0];
        ok = (deyim->tip == DUGUM_ATAMA)
          && (deyim->veri.atama.hedef->tip == DUGUM_TANIMLAYICI)
          && (deyim->veri.atama.deger->tip == DUGUM_TAM);
    }
    test_sonuc("atama: x = 5;", ok);
    arena_serbest(a);
}

static void test_ifade_deyimi(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* "islev f() { x; }" — sadece tanimlayici ifade_deyimi olur */
    Dugum *prog = parse_kaynak("i\xc5\x9flev f() { x; }", a, &hata);
    int ok = prog && hata == 0;
    if (ok) {
        Dugum *deyim = prog->veri.program.uyeler[0]
                           ->veri.islev.govde
                           ->veri.blok.deyimler[0];
        ok = (deyim->tip == DUGUM_IFADE_DEYIMI)
          && (deyim->veri.ifade_deyimi.ifade->tip == DUGUM_TANIMLAYICI);
    }
    test_sonuc("ifade_deyimi: x;", ok);
    arena_serbest(a);
}

/* === Konum bilgisi === */

static void test_konum_korunuyor(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* "\nislev f() {}" — islev 2. satirda */
    Dugum *prog = parse_kaynak("\ni\xc5\x9flev f() {}", a, &hata);
    int ok = prog && hata == 0;
    if (ok) {
        Dugum *islev = prog->veri.program.uyeler[0];
        ok = (islev->satir == 2);
    }
    test_sonuc("konum: islev 2. satirda", ok);
    arena_serbest(a);
}

/* === Yazdirma cokme yok === */

static void test_yazdirma_cokmuyor(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    const char *kaynak =
        "kullan std; yap\xc4\xb1 X { y: tam32; } "
        "i\xc5\x9flev f() { de\xc4\x9fi\xc5\x9fken z = 1; ver z; }";
    Dugum *prog = parse_kaynak(kaynak, a, &hata);
    FILE *bos = fopen("NUL", "w");
    if (!bos) bos = stdout;
    ast_yazdir(prog, bos);
    if (bos != stdout) fclose(bos);
    test_sonuc("ast_yazdir karmasik program cokmuyor", prog != NULL);
    arena_serbest(a);
}

/* === Stres === */

static void test_stres_50_islev(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* 50 ust uste islev tanimi */
    char buyuk[8192];
    int n = 0;
    for (int i = 0; i < 50; i++) {
        n += snprintf(buyuk + n, sizeof(buyuk) - (size_t)n,
                      "i\xc5\x9flev f%d() {} ", i);
    }
    Dugum *prog = parse_kaynak(buyuk, a, &hata);
    int ok = prog && hata == 0
          && prog->veri.program.sayi == 50;
    test_sonuc("50 islev tanimi (ASan sizinti yok)", ok);
    arena_serbest(a);
}

/* === Ifade testleri (ADIM 9 — Pratt parser) === */

/* Yardimci: ifadeyi 'sabit _X: tam32 = IFADE;' icine sar, AST'den ifadeyi cek. */
static Dugum *ifade_parse(const char *ifade_kaynak, Arena *a, int *hata) {
    char buf[1024];
    snprintf(buf, sizeof(buf), "sabit _X: tam32 = %s;", ifade_kaynak);
    Dugum *prog = parse_kaynak(buf, a, hata);
    if (!prog || prog->veri.program.sayi == 0) return NULL;
    Dugum *sabit = prog->veri.program.uyeler[0];
    if (sabit->tip != DUGUM_SABIT) return NULL;
    return sabit->veri.sabit.deger;
}

/* Yardimci: hata yok bekleyen test. */
static int ifade_dogrula(Dugum *d, int hata, DugumTipi beklenen) {
    return d != NULL && hata == 0 && d->tip == beklenen;
}

static void test_ifade_tek_literal(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    Dugum *e = ifade_parse("42", a, &hata);
    int ok = ifade_dogrula(e, hata, DUGUM_TAM) && e->veri.tam.deger == 42;
    test_sonuc("ifade: 42", ok);
    arena_serbest(a);
}

static void test_ifade_toplama(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    Dugum *e = ifade_parse("1 + 2", a, &hata);
    int ok = ifade_dogrula(e, hata, DUGUM_IKILI)
          && e->veri.ikili.op == OP_ARTI
          && e->veri.ikili.sol->veri.tam.deger == 1
          && e->veri.ikili.sag->veri.tam.deger == 2;
    test_sonuc("ifade: 1 + 2", ok);
    arena_serbest(a);
}

static void test_ifade_oncelik(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* 1 + 2 * 3 -> IKILI(+, 1, IKILI(*, 2, 3)) */
    Dugum *e = ifade_parse("1 + 2 * 3", a, &hata);
    int ok = ifade_dogrula(e, hata, DUGUM_IKILI)
          && e->veri.ikili.op == OP_ARTI
          && e->veri.ikili.sol->veri.tam.deger == 1
          && e->veri.ikili.sag->tip == DUGUM_IKILI
          && e->veri.ikili.sag->veri.ikili.op == OP_CARPI;
    test_sonuc("ifade oncelik: 1 + 2 * 3", ok);
    arena_serbest(a);
}

static void test_ifade_sol_birlesme(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* 10 - 3 - 2 -> IKILI(-, IKILI(-, 10, 3), 2) */
    Dugum *e = ifade_parse("10 - 3 - 2", a, &hata);
    int ok = ifade_dogrula(e, hata, DUGUM_IKILI)
          && e->veri.ikili.op == OP_EKSI
          && e->veri.ikili.sol->tip == DUGUM_IKILI
          && e->veri.ikili.sol->veri.ikili.op == OP_EKSI
          && e->veri.ikili.sol->veri.ikili.sol->veri.tam.deger == 10
          && e->veri.ikili.sol->veri.ikili.sag->veri.tam.deger == 3
          && e->veri.ikili.sag->veri.tam.deger == 2;
    test_sonuc("ifade sol birlesme: 10 - 3 - 2", ok);
    arena_serbest(a);
}

static void test_ifade_parantez(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* (1 + 2) * 3 -> IKILI(*, IKILI(+, 1, 2), 3) */
    Dugum *e = ifade_parse("(1 + 2) * 3", a, &hata);
    int ok = ifade_dogrula(e, hata, DUGUM_IKILI)
          && e->veri.ikili.op == OP_CARPI
          && e->veri.ikili.sol->tip == DUGUM_IKILI
          && e->veri.ikili.sol->veri.ikili.op == OP_ARTI
          && e->veri.ikili.sag->veri.tam.deger == 3;
    test_sonuc("ifade parantez: (1 + 2) * 3", ok);
    arena_serbest(a);
}

static void test_ifade_karsilastirma(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    Dugum *e = ifade_parse("x < 5", a, &hata);
    int ok = ifade_dogrula(e, hata, DUGUM_IKILI)
          && e->veri.ikili.op == OP_KUCUK;
    test_sonuc("ifade karsilastirma: x < 5", ok);
    arena_serbest(a);
}

static void test_ifade_mantik(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* x ve y veya z -> IKILI(veya, IKILI(ve, x, y), z) — veya < ve */
    /* "ve" -> "v" "e" — UTF-8 sorunsuz */
    Dugum *e = ifade_parse("x ve y veya z", a, &hata);
    int ok = ifade_dogrula(e, hata, DUGUM_IKILI)
          && e->veri.ikili.op == OP_VEYA
          && e->veri.ikili.sol->tip == DUGUM_IKILI
          && e->veri.ikili.sol->veri.ikili.op == OP_VE;
    test_sonuc("ifade mantik: x ve y veya z", ok);
    arena_serbest(a);
}

static void test_ifade_esitlik(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    Dugum *e = ifade_parse("x == y", a, &hata);
    int ok = ifade_dogrula(e, hata, DUGUM_IKILI)
          && e->veri.ikili.op == OP_ESIT;
    test_sonuc("ifade esitlik: x == y", ok);
    arena_serbest(a);
}

static void test_ifade_onek_neg(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    Dugum *e = ifade_parse("-x", a, &hata);
    int ok = ifade_dogrula(e, hata, DUGUM_TEKLI)
          && e->veri.tekli.op == OP_NEG
          && e->veri.tekli.operand->tip == DUGUM_TANIMLAYICI;
    test_sonuc("ifade onek: -x", ok);
    arena_serbest(a);
}

static void test_ifade_onek_degil(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* "degil x" Turkce */
    Dugum *e = ifade_parse("de\xc4\x9fil x", a, &hata);
    int ok = ifade_dogrula(e, hata, DUGUM_TEKLI)
          && e->veri.tekli.op == OP_DEGIL;
    test_sonuc("ifade onek: degil x", ok);
    arena_serbest(a);
}

static void test_ifade_onek_degil_cagri(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* "degil f(x)" -> degil(f(x)), parantez gerekmez */
    Dugum *e = ifade_parse("de\xc4\x9fil f(x)", a, &hata);
    int ok = ifade_dogrula(e, hata, DUGUM_TEKLI)
          && e->veri.tekli.op == OP_DEGIL
          && e->veri.tekli.operand->tip == DUGUM_CAGRI;
    test_sonuc("ifade onek: degil f(x) -> degil(f(x))", ok);
    arena_serbest(a);
}

static void test_ifade_onek_degil_erisim(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* "degil x.alan" -> degil(x.alan) */
    Dugum *e = ifade_parse("de\xc4\x9fil x.alan", a, &hata);
    int ok = ifade_dogrula(e, hata, DUGUM_TEKLI)
          && e->veri.tekli.op == OP_DEGIL
          && e->veri.tekli.operand->tip == DUGUM_ERISIM;
    test_sonuc("ifade onek: degil x.alan -> degil(x.alan)", ok);
    arena_serbest(a);
}

static void test_ifade_onek_degil_indeks(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* "degil arr[0]" -> degil(arr[0]) */
    Dugum *e = ifade_parse("de\xc4\x9fil arr[0]", a, &hata);
    int ok = ifade_dogrula(e, hata, DUGUM_TEKLI)
          && e->veri.tekli.op == OP_DEGIL
          && e->veri.tekli.operand->tip == DUGUM_INDEKS;
    test_sonuc("ifade onek: degil arr[0] -> degil(arr[0])", ok);
    arena_serbest(a);
}

static void test_ifade_onek_degil_zincir(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* "degil degil f()" -> degil(degil(f())) */
    Dugum *e = ifade_parse("de\xc4\x9fil de\xc4\x9fil f()", a, &hata);
    int ok = ifade_dogrula(e, hata, DUGUM_TEKLI)
          && e->veri.tekli.op == OP_DEGIL
          && e->veri.tekli.operand->tip == DUGUM_TEKLI
          && e->veri.tekli.operand->veri.tekli.op == OP_DEGIL
          && e->veri.tekli.operand->veri.tekli.operand->tip == DUGUM_CAGRI;
    test_sonuc("ifade onek: degil degil f() (zincir)", ok);
    arena_serbest(a);
}

static void test_ifade_onek_degil_ikili_sonra(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* "degil f(x) ve g(y)" -> (degil f(x)) ve (g(y))
     * Bu test ikili op'larin hala ONEK'ten oncelikli baglandigini gosterir. */
    Dugum *e = ifade_parse(
        "de\xc4\x9fil f(x) ve g(y)", a, &hata);
    int ok = e && hata == 0
          && e->tip == DUGUM_IKILI
          && e->veri.ikili.op == OP_VE
          && e->veri.ikili.sol->tip == DUGUM_TEKLI
          && e->veri.ikili.sol->veri.tekli.op == OP_DEGIL
          && e->veri.ikili.sol->veri.tekli.operand->tip == DUGUM_CAGRI;
    test_sonuc("ifade onek: degil f(x) ve g(y)", ok);
    arena_serbest(a);
}

static void test_ifade_onek_ref(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    Dugum *e = ifade_parse("&x", a, &hata);
    int ok = ifade_dogrula(e, hata, DUGUM_TEKLI)
          && e->veri.tekli.op == OP_REF;
    test_sonuc("ifade onek: &x", ok);
    arena_serbest(a);
}

static void test_ifade_onek_ref_degisken(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* &degisken x */
    Dugum *e = ifade_parse("&de\xc4\x9fi\xc5\x9fken x", a, &hata);
    int ok = ifade_dogrula(e, hata, DUGUM_TEKLI)
          && e->veri.tekli.op == OP_REF_DEGISKEN;
    test_sonuc("ifade onek: &degisken x", ok);
    arena_serbest(a);
}

static void test_ifade_onek_deref(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    Dugum *e = ifade_parse("*x", a, &hata);
    int ok = ifade_dogrula(e, hata, DUGUM_TEKLI)
          && e->veri.tekli.op == OP_DEREFERANS;
    test_sonuc("ifade onek: *x", ok);
    arena_serbest(a);
}

static void test_ifade_cagri_bos(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    Dugum *e = ifade_parse("f()", a, &hata);
    int ok = ifade_dogrula(e, hata, DUGUM_CAGRI)
          && e->veri.cagri.sayi == 0
          && e->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI;
    test_sonuc("ifade cagri: f()", ok);
    arena_serbest(a);
}

static void test_ifade_cagri_args(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    Dugum *e = ifade_parse("f(1, 2, 3)", a, &hata);
    int ok = ifade_dogrula(e, hata, DUGUM_CAGRI)
          && e->veri.cagri.sayi == 3
          && e->veri.cagri.argumanlar[0]->veri.tam.deger == 1
          && e->veri.cagri.argumanlar[2]->veri.tam.deger == 3;
    test_sonuc("ifade cagri: f(1, 2, 3)", ok);
    arena_serbest(a);
}

static void test_ifade_erisim(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    Dugum *e = ifade_parse("x.y", a, &hata);
    int ok = ifade_dogrula(e, hata, DUGUM_ERISIM)
          && strcmp(e->veri.erisim.alan, "y") == 0;
    test_sonuc("ifade erisim: x.y", ok);
    arena_serbest(a);
}

static void test_ifade_indeks(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    Dugum *e = ifade_parse("x[5]", a, &hata);
    int ok = ifade_dogrula(e, hata, DUGUM_INDEKS)
          && e->veri.indeks.indeks->veri.tam.deger == 5;
    test_sonuc("ifade indeks: x[5]", ok);
    arena_serbest(a);
}

static void test_ifade_yol(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    Dugum *e = ifade_parse("std::yaz", a, &hata);
    int ok = ifade_dogrula(e, hata, DUGUM_YOL)
          && strcmp(e->veri.yol.sag_ad, "yaz") == 0;
    test_sonuc("ifade yol: std::yaz", ok);
    arena_serbest(a);
}

static void test_ifade_sonek_zincir(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* x.y.z -> ERISIM(ERISIM(x, "y"), "z") */
    Dugum *e = ifade_parse("x.y.z", a, &hata);
    int ok = ifade_dogrula(e, hata, DUGUM_ERISIM)
          && strcmp(e->veri.erisim.alan, "z") == 0
          && e->veri.erisim.nesne->tip == DUGUM_ERISIM
          && strcmp(e->veri.erisim.nesne->veri.erisim.alan, "y") == 0;
    test_sonuc("ifade sonek zincir: x.y.z", ok);
    arena_serbest(a);
}

static void test_ifade_yapi_olusturma(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    Dugum *e = ifade_parse("Nokta { x: 1, y: 2 }", a, &hata);
    int ok = ifade_dogrula(e, hata, DUGUM_YAPI_OLUSTUR)
          && strcmp(e->veri.yapi_olustur.tip_ad, "Nokta") == 0
          && e->veri.yapi_olustur.alan_sayi == 2;
    test_sonuc("ifade yapi olusturma", ok);
    arena_serbest(a);
}

static void test_ifade_yapi_trailing_comma(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    Dugum *e = ifade_parse("Nokta { x: 1, y: 2, }", a, &hata);
    int ok = ifade_dogrula(e, hata, DUGUM_YAPI_OLUSTUR)
          && e->veri.yapi_olustur.alan_sayi == 2;
    test_sonuc("ifade yapi trailing comma", ok);
    arena_serbest(a);
}

static void test_ifade_dizi_olusturma(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    Dugum *e = ifade_parse("[1, 2, 3]", a, &hata);
    int ok = ifade_dogrula(e, hata, DUGUM_DIZI_OLUSTUR)
          && e->veri.dizi_olustur.sayi == 3;
    test_sonuc("ifade dizi: [1, 2, 3]", ok);
    arena_serbest(a);
}

static void test_ifade_dizi_bos(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    Dugum *e = ifade_parse("[]", a, &hata);
    int ok = ifade_dogrula(e, hata, DUGUM_DIZI_OLUSTUR)
          && e->veri.dizi_olustur.sayi == 0;
    test_sonuc("ifade dizi bos: []", ok);
    arena_serbest(a);
}

static void test_ifade_lambda(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* |a: tam32| a + 1 */
    Dugum *e = ifade_parse("|a: tam32| a + 1", a, &hata);
    int ok = ifade_dogrula(e, hata, DUGUM_LAMBDA)
          && e->veri.lambda.param_sayi == 1
          && e->veri.lambda.govde != NULL
          && e->veri.lambda.govde->tip == DUGUM_IKILI;
    test_sonuc("ifade lambda: |a: tam32| a + 1", ok);
    arena_serbest(a);
}

/* === Kalan deyim testleri (ADIM 10.A) === */

/* Yardimci: deyimi 'islev _f() { DEYIM }' icine sar, ilk deyimi cek. */
static Dugum *deyim_parse(const char *deyim_kaynak, Arena *a, int *hata) {
    char buf[2048];
    snprintf(buf, sizeof(buf),
             "i\xc5\x9flev _f() { %s }", deyim_kaynak);
    Dugum *prog = parse_kaynak(buf, a, hata);
    if (!prog || prog->veri.program.sayi == 0) return NULL;
    Dugum *islev = prog->veri.program.uyeler[0];
    if (!islev || islev->tip != DUGUM_ISLEV) return NULL;
    if (!islev->veri.islev.govde) return NULL;
    Dugum *blok = islev->veri.islev.govde;
    if (blok->veri.blok.sayi == 0) return NULL;
    return blok->veri.blok.deyimler[0];
}

static void test_deyim_eger_basit(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* "eger dogru { ver 1; }" */
    Dugum *d = deyim_parse(
        "e\xc4\x9f" "er do\xc4\x9fru { ver 1; }", a, &hata);
    int ok = d && hata == 0
          && d->tip == DUGUM_EGER
          && d->veri.eger.kosul->tip == DUGUM_MANTIKSAL
          && d->veri.eger.gozdoldur->tip == DUGUM_BLOK
          && d->veri.eger.yan == NULL;
    test_sonuc("eger dogru { ver 1; }", ok);
    arena_serbest(a);
}

static void test_deyim_eger_degilse(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* "eger x { ver 1; } degilse { ver 2; }" */
    Dugum *d = deyim_parse(
        "e\xc4\x9f" "er x { ver 1; } de\xc4\x9f" "ilse { ver 2; }",
        a, &hata);
    int ok = d && hata == 0
          && d->tip == DUGUM_EGER
          && d->veri.eger.yan != NULL
          && d->veri.eger.yan->tip == DUGUM_BLOK;
    test_sonuc("eger { } degilse { }", ok);
    arena_serbest(a);
}

static void test_deyim_eger_zincir(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* "eger x { 1; } degilse eger y { 2; } degilse { 3; }" */
    Dugum *d = deyim_parse(
        "e\xc4\x9f" "er x { 1; } "
        "de\xc4\x9f" "ilse e\xc4\x9f" "er y { 2; } "
        "de\xc4\x9f" "ilse { 3; }",
        a, &hata);
    int ok = d && hata == 0
          && d->tip == DUGUM_EGER
          && d->veri.eger.yan != NULL
          && d->veri.eger.yan->tip == DUGUM_EGER  /* else if */
          && d->veri.eger.yan->veri.eger.yan != NULL
          && d->veri.eger.yan->veri.eger.yan->tip == DUGUM_BLOK;
    test_sonuc("eger / degilse eger / degilse zincir", ok);
    arena_serbest(a);
}

static void test_deyim_iken(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* "iken x < 10 { x = x + 1; }" */
    Dugum *d = deyim_parse("iken x < 10 { x = x + 1; }", a, &hata);
    int ok = d && hata == 0
          && d->tip == DUGUM_IKEN
          && d->veri.iken.kosul->tip == DUGUM_IKILI
          && d->veri.iken.kosul->veri.ikili.op == OP_KUCUK
          && d->veri.iken.govde->tip == DUGUM_BLOK
          && d->veri.iken.govde->veri.blok.sayi == 1;
    test_sonuc("iken x < 10 { ... }", ok);
    arena_serbest(a);
}

static void test_deyim_icin(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* "icin i: liste { ver i; }" */
    Dugum *d = deyim_parse("i\xc3\xa7in i: liste { ver i; }", a, &hata);
    int ok = d && hata == 0
          && d->tip == DUGUM_ICIN
          && strcmp(d->veri.icin.degisken_adi, "i") == 0
          && d->veri.icin.koleksiyon->tip == DUGUM_TANIMLAYICI
          && d->veri.icin.govde->tip == DUGUM_BLOK;
    test_sonuc("icin i: liste { ... }", ok);
    arena_serbest(a);
}

static void test_deyim_esles_literal(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* "esles x { 1 => dogru; _ => yanlis; }" */
    Dugum *d = deyim_parse(
        "e\xc5\x9fle\xc5\x9f x { "
        "1 => do\xc4\x9fru; "
        "_ => yanl\xc4\xb1\xc5\x9f; }", a, &hata);
    int ok = d && hata == 0
          && d->tip == DUGUM_ESLES
          && d->veri.esles.kol_sayi == 2
          && d->veri.esles.kollar[0]->tip == DUGUM_ESLES_KOLU
          && d->veri.esles.kollar[0]->veri.esles_kolu.desen->tip == DUGUM_DESEN_LITERAL
          && d->veri.esles.kollar[1]->veri.esles_kolu.desen->tip == DUGUM_DESEN_JOKER;
    test_sonuc("esles x { 1 => ...; _ => ...; }", ok);
    arena_serbest(a);
}

static void test_deyim_esles_yapici(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* "esles x { Deger(v) => v; hic => 0; }" */
    Dugum *d = deyim_parse(
        "e\xc5\x9fle\xc5\x9f x { "
        "De\xc4\x9f" "er(v) => v; "
        "hi\xc3\xa7 => 0; }", a, &hata);
    int ok = d && hata == 0
          && d->tip == DUGUM_ESLES
          && d->veri.esles.kol_sayi == 2
          && d->veri.esles.kollar[0]->veri.esles_kolu.desen->tip == DUGUM_DESEN_YAPICI
          && d->veri.esles.kollar[0]->veri.esles_kolu.desen->veri.desen_yapici.sayi == 1
          && d->veri.esles.kollar[1]->veri.esles_kolu.desen->tip == DUGUM_DESEN_TANIMLAYICI;
    test_sonuc("esles yapici desen + tanimlayici", ok);
    arena_serbest(a);
}

/* === C: sonuc<T,E> deseni — tamam(v), hata(m) === */

/* tamam(v) ve hata(m) desenleri parse edilir mi (ad dogrulamali). */
static void test_deyim_esles_tamam_deseni(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* "esles r { tamam(v) => v; hata(m) => 0; }" */
    Dugum *d = deyim_parse(
        "e\xc5\x9fle\xc5\x9f r { "
        "tamam(v) => v; "
        "hata(m) => 0; }", a, &hata);
    int ok = d && hata == 0
          && d->tip == DUGUM_ESLES
          && d->veri.esles.kol_sayi == 2
          && d->veri.esles.kollar[0]->veri.esles_kolu.desen->tip == DUGUM_DESEN_YAPICI
          && d->veri.esles.kollar[0]->veri.esles_kolu.desen->veri.desen_yapici.ad_uzunluk == 5
          && memcmp(d->veri.esles.kollar[0]->veri.esles_kolu.desen->veri.desen_yapici.ad,
                    "tamam", 5) == 0
          && d->veri.esles.kollar[1]->veri.esles_kolu.desen->tip == DUGUM_DESEN_YAPICI
          && d->veri.esles.kollar[1]->veri.esles_kolu.desen->veri.desen_yapici.ad_uzunluk == 4
          && memcmp(d->veri.esles.kollar[1]->veri.esles_kolu.desen->veri.desen_yapici.ad,
                    "hata", 4) == 0;
    test_sonuc("esles tamam(v) + hata(m) desenleri", ok);
    arena_serbest(a);
}

/* tamam'a parametresiz desen (bind yok) — tamam joker olarak alt-desen. */
static void test_deyim_esles_tamam_joker(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* "esles r { tamam(_) => 1; hata(m) => 0; }" */
    Dugum *d = deyim_parse(
        "e\xc5\x9fle\xc5\x9f r { "
        "tamam(_) => 1; "
        "hata(m) => 0; }", a, &hata);
    int ok = d && hata == 0
          && d->tip == DUGUM_ESLES
          && d->veri.esles.kol_sayi == 2
          && d->veri.esles.kollar[0]->veri.esles_kolu.desen->tip == DUGUM_DESEN_YAPICI
          && d->veri.esles.kollar[0]->veri.esles_kolu.desen->veri.desen_yapici.alt_desenler[0]->tip
              == DUGUM_DESEN_JOKER;
    test_sonuc("esles tamam(_) joker alt-desen", ok);
    arena_serbest(a);
}

/* tamam ile joker karma — son kolda underscore */
static void test_deyim_esles_sonuc_karma(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    Dugum *d = deyim_parse(
        "e\xc5\x9fle\xc5\x9f s { "
        "tamam(v) => v; "
        "_ => 0; }", a, &hata);
    int ok = d && hata == 0
          && d->tip == DUGUM_ESLES
          && d->veri.esles.kol_sayi == 2
          && d->veri.esles.kollar[0]->veri.esles_kolu.desen->tip == DUGUM_DESEN_YAPICI
          && d->veri.esles.kollar[1]->veri.esles_kolu.desen->tip == DUGUM_DESEN_JOKER;
    test_sonuc("esles sonuc karma: tamam(v) + _", ok);
    arena_serbest(a);
}

static void test_deyim_esles_blok_govde(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* "esles x { 1 => { ver 1; } }" */
    Dugum *d = deyim_parse(
        "e\xc5\x9fle\xc5\x9f x { 1 => { ver 1; } }", a, &hata);
    int ok = d && hata == 0
          && d->tip == DUGUM_ESLES
          && d->veri.esles.kol_sayi == 1
          && d->veri.esles.kollar[0]->veri.esles_kolu.govde->tip == DUGUM_BLOK;
    test_sonuc("esles kolu blok govde", ok);
    arena_serbest(a);
}

static void test_deyim_guvensiz_basit(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* "guvensiz { ver 0; }" */
    Dugum *d = deyim_parse(
        "g\xc3\xbcvensiz { ver 0; }", a, &hata);
    int ok = d && hata == 0
          && d->tip == DUGUM_GUVENSIZ
          && d->veri.guvensiz.aciklama_ad == NULL
          && d->veri.guvensiz.blok->tip == DUGUM_BLOK;
    test_sonuc("guvensiz { ... } (aciklama yok)", ok);
    arena_serbest(a);
}

static void test_deyim_guvensiz_aciklama(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* "guvensiz [sebep: \"raw pointer\"] { ver 0; }" */
    Dugum *d = deyim_parse(
        "g\xc3\xbcvensiz [sebep: \"ham pointer\"] { ver 0; }", a, &hata);
    int ok = d && hata == 0
          && d->tip == DUGUM_GUVENSIZ
          && d->veri.guvensiz.aciklama_ad != NULL
          && strcmp(d->veri.guvensiz.aciklama_ad, "sebep") == 0
          && d->veri.guvensiz.aciklama_metin != NULL;
    test_sonuc("guvensiz [sebep: \"...\"] { ... }", ok);
    arena_serbest(a);
}

/* === Karmasik tip testleri (ADIM 10.B) === */

/* Yardimci: tipi 'islev _f(x: TIP) {}' parametresinde parse et, tip cek. */
static Dugum *tip_parse(const char *tip_kaynak, Arena *a, int *hata) {
    char buf[2048];
    snprintf(buf, sizeof(buf),
             "i\xc5\x9flev _f(x: %s) {}", tip_kaynak);
    Dugum *prog = parse_kaynak(buf, a, hata);
    if (!prog || prog->veri.program.sayi == 0) return NULL;
    Dugum *islev = prog->veri.program.uyeler[0];
    if (!islev || islev->veri.islev.param_sayi == 0) return NULL;
    return islev->veri.islev.parametreler[0]->veri.parametre.tip;
}

static void test_tip_basit(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    Dugum *t = tip_parse("tam32", a, &hata);
    int ok = t && hata == 0
          && t->tip == DUGUM_TIP_BASIT
          && strcmp(t->veri.tip_basit.ad, "tam32") == 0;
    test_sonuc("tip basit: tam32", ok);
    arena_serbest(a);
}

static void test_tip_referans(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    Dugum *t = tip_parse("&tam32", a, &hata);
    int ok = t && hata == 0
          && t->tip == DUGUM_TIP_REFERANS
          && t->veri.tip_referans.degisken_mi == 0
          && t->veri.tip_referans.hedef_tip->tip == DUGUM_TIP_BASIT;
    test_sonuc("tip referans: &tam32", ok);
    arena_serbest(a);
}

static void test_tip_referans_degisken(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* &degisken tam32 */
    Dugum *t = tip_parse("&de\xc4\x9f""i\xc5\x9fken tam32", a, &hata);
    int ok = t && hata == 0
          && t->tip == DUGUM_TIP_REFERANS
          && t->veri.tip_referans.degisken_mi == 1;
    test_sonuc("tip referans: &degisken tam32", ok);
    arena_serbest(a);
}

static void test_tip_pointer(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    Dugum *t = tip_parse("*tam32", a, &hata);
    int ok = t && hata == 0
          && t->tip == DUGUM_TIP_POINTER
          && t->veri.tip_pointer.hedef_tip->tip == DUGUM_TIP_BASIT;
    test_sonuc("tip pointer: *tam32", ok);
    arena_serbest(a);
}

static void test_tip_secimlik(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* secimlik<tam32> */
    Dugum *t = tip_parse("se\xc3\xa7imlik<tam32>", a, &hata);
    int ok = t && hata == 0
          && t->tip == DUGUM_TIP_SECIMLIK
          && t->veri.tip_secimlik.ic_tip->tip == DUGUM_TIP_BASIT;
    test_sonuc("tip secimlik: secimlik<tam32>", ok);
    arena_serbest(a);
}

static void test_tip_sonuc(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* sonuc<tam32, metin> — 'hata' keyword cakismasi nedeniyle 'metin' kullanildi */
    Dugum *t = tip_parse("sonu\xc3\xa7<tam32, metin>", a, &hata);
    int ok = t && hata == 0
          && t->tip == DUGUM_TIP_SONUC
          && t->veri.tip_sonuc.deger_tip != NULL
          && t->veri.tip_sonuc.hata_tip != NULL;
    test_sonuc("tip sonuc: sonuc<tam32, hata>", ok);
    arena_serbest(a);
}

static void test_tip_dizi(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    Dugum *t = tip_parse("Dizi<tam32>", a, &hata);
    int ok = t && hata == 0
          && t->tip == DUGUM_TIP_DIZI
          && t->veri.tip_dizi.eleman_tip->tip == DUGUM_TIP_BASIT;
    test_sonuc("tip dizi: Dizi<tam32>", ok);
    arena_serbest(a);
}

static void test_tip_islev(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* islev(tam32, tam32) -> tam32 */
    Dugum *t = tip_parse("i\xc5\x9flev(tam32, tam32) -> tam32", a, &hata);
    int ok = t && hata == 0
          && t->tip == DUGUM_TIP_ISLEV
          && t->veri.tip_islev.param_sayi == 2
          && t->veri.tip_islev.donus_tip->tip == DUGUM_TIP_BASIT;
    test_sonuc("tip islev: islev(tam32, tam32) -> tam32", ok);
    arena_serbest(a);
}

static void test_tip_kullanici_generic(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* Hasta<tam32> */
    Dugum *t = tip_parse("Hasta<tam32>", a, &hata);
    int ok = t && hata == 0
          && t->tip == DUGUM_TIP_KULLANICI
          && t->veri.tip_kullanici.tip_arg_sayi == 1;
    test_sonuc("tip kullanici generic: Hasta<tam32>", ok);
    arena_serbest(a);
}

static void test_tip_ic_ice(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* Dizi<secimlik<tam32>> */
    Dugum *t = tip_parse("Dizi<se\xc3\xa7imlik<tam32>>", a, &hata);
    int ok = t && hata == 0
          && t->tip == DUGUM_TIP_DIZI
          && t->veri.tip_dizi.eleman_tip->tip == DUGUM_TIP_SECIMLIK
          && t->veri.tip_dizi.eleman_tip->veri.tip_secimlik.ic_tip->tip == DUGUM_TIP_BASIT;
    test_sonuc("tip ic ice: Dizi<secimlik<tam32>>", ok);
    arena_serbest(a);
}

static void test_yapi_generic(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* yapi Kutu<T> { eleman: T; } — 'deger' keyword cakismasi yerine 'eleman' */
    Dugum *prog = parse_kaynak(
        "yap\xc4\xb1 Kutu<T> { eleman: T; }", a, &hata);
    int ok = prog && hata == 0 && prog->veri.program.sayi == 1;
    if (ok) {
        Dugum *yapi = prog->veri.program.uyeler[0];
        ok = yapi->tip == DUGUM_YAPI
          && yapi->veri.yapi.tip_param_sayi == 1
          && yapi->veri.yapi.tip_paramlar != NULL
          && strcmp(yapi->veri.yapi.tip_paramlar[0], "T") == 0
          && yapi->veri.yapi.alan_sayi == 1;
    }
    test_sonuc("yapi generic: Kutu<T> { deger: T; }", ok);
    arena_serbest(a);
}

/* === Ornek .kem dosya parse testleri (ADIM 10.C) === */

static char *dosya_oku_test(const char *yol) {
    FILE *f = fopen(yol, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long boyut = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (boyut < 0) { fclose(f); return NULL; }
    char *t = (char *)malloc((size_t)boyut + 1);
    if (!t) { fclose(f); return NULL; }
    size_t okunan = fread(t, 1, (size_t)boyut, f);
    t[okunan] = '\0';
    fclose(f);
    return t;
}

static void ornek_dosya_test(const char *ad, const char *yol, int min_uye) {
    Arena *a = arena_olustur(0);
    char *kaynak = dosya_oku_test(yol);
    if (!kaynak) {
        test_sonuc(ad, 0);
        arena_serbest(a);
        return;
    }
    int hata = -1;
    Dugum *prog = parse_kaynak(kaynak, a, &hata);
    int ok = prog && hata == 0 && prog->veri.program.sayi >= min_uye;
    test_sonuc(ad, ok);
    free(kaynak);
    arena_serbest(a);
}

static void test_ornek_fibonacci(void) {
    ornek_dosya_test("ornek: fibonacci.kem (2 islev)",
                     "test/ornekler/fibonacci.kem", 2);
}

static void test_ornek_yapilar(void) {
    ornek_dosya_test("ornek: yapilar.kem (2 yapi + 3 islev)",
                     "test/ornekler/yapilar.kem", 5);
}

static void test_ornek_eslesme(void) {
    ornek_dosya_test("ornek: eslesme.kem (4 islev)",
                     "test/ornekler/eslesme.kem", 4);
}

static void test_ornek_hasta(void) {
    ornek_dosya_test("ornek: hasta.kem (1 yapi + 3 islev)",
                     "test/ornekler/hasta.kem", 4);
}

/* === Ozellik (trait) === */

static void test_ozellik_bos(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    Dugum *prog = parse_kaynak("\xc3\xb6zellik Sayilabilir {}", a, &hata);
    int ok = prog && prog->veri.program.sayi == 1 && hata == 0;
    if (ok) {
        Dugum *oz = prog->veri.program.uyeler[0];
        ok = (oz->tip == DUGUM_OZELLIK)
          && (oz->veri.ozellik.uye_sayi == 0)
          && (strcmp(oz->veri.ozellik.ad, "Sayilabilir") == 0);
    }
    test_sonuc("ozellik bos", ok);
    arena_serbest(a);
}

static void test_ozellik_imza(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* özellik Sayilabilir { işlev say() -> tam32; } */
    Dugum *prog = parse_kaynak(
        "\xc3\xb6zellik Sayilabilir { i\xc5\x9flev say() -> tam32; }",
        a, &hata);
    int ok = prog && prog->veri.program.sayi == 1 && hata == 0;
    if (ok) {
        Dugum *oz = prog->veri.program.uyeler[0];
        ok = (oz->tip == DUGUM_OZELLIK)
          && (oz->veri.ozellik.uye_sayi == 1);
        if (ok) {
            Dugum *m = oz->veri.ozellik.uyeler[0];
            /* Imza: govde NULL */
            ok = (m->tip == DUGUM_ISLEV)
              && (m->veri.islev.govde == NULL)
              && (strcmp(m->veri.islev.ad, "say") == 0);
        }
    }
    test_sonuc("ozellik islev imzasi (govdesiz)", ok);
    arena_serbest(a);
}

static void test_ozellik_default_impl(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* özellik X { işlev m() -> tam32 { ver 0; } } */
    Dugum *prog = parse_kaynak(
        "\xc3\xb6zellik X { i\xc5\x9flev m() -> tam32 { ver 0; } }",
        a, &hata);
    int ok = prog && prog->veri.program.sayi == 1 && hata == 0;
    if (ok) {
        Dugum *oz = prog->veri.program.uyeler[0];
        ok = (oz->veri.ozellik.uye_sayi == 1);
        if (ok) {
            Dugum *m = oz->veri.ozellik.uyeler[0];
            ok = (m->veri.islev.govde != NULL);  /* default impl */
        }
    }
    test_sonuc("ozellik default impl (govdeli)", ok);
    arena_serbest(a);
}

static void test_ozellik_generic(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* özellik Foo<T> { işlev m() -> T; } */
    Dugum *prog = parse_kaynak(
        "\xc3\xb6zellik Foo<T> { i\xc5\x9flev m() -> T; }",
        a, &hata);
    int ok = prog && prog->veri.program.sayi == 1 && hata == 0;
    if (ok) {
        Dugum *oz = prog->veri.program.uyeler[0];
        ok = (oz->veri.ozellik.tip_param_sayi == 1)
          && (strcmp(oz->veri.ozellik.tip_paramlar[0], "T") == 0);
    }
    test_sonuc("ozellik generic <T>", ok);
    arena_serbest(a);
}

/* === Uygula === */

static void test_uygula_inherent(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* yapı K { x: tam32; } uygula K { işlev m() -> tam32 { ver 0; } } */
    Dugum *prog = parse_kaynak(
        "yap\xc4\xb1 K { x: tam32; } uygula K { i\xc5\x9flev m() -> tam32 { ver 0; } }",
        a, &hata);
    int ok = prog && prog->veri.program.sayi == 2 && hata == 0;
    if (ok) {
        Dugum *u = prog->veri.program.uyeler[1];
        ok = (u->tip == DUGUM_UYGULA)
          && (u->veri.uygula.ozellik_sayi == 0)  /* inherent */
          && (u->veri.uygula.islev_sayi == 1)
          && (u->veri.uygula.tip != NULL);
    }
    test_sonuc("uygula inherent (ozelliksiz)", ok);
    arena_serbest(a);
}

static void test_uygula_trait(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* özellik Say {} yapı K { x: tam32; } uygula Say için K { } */
    Dugum *prog = parse_kaynak(
        "\xc3\xb6zellik Say {} yap\xc4\xb1 K { x: tam32; } "
        "uygula Say i\xc3\xa7in K { }",
        a, &hata);
    int ok = prog && prog->veri.program.sayi == 3 && hata == 0;
    if (ok) {
        Dugum *u = prog->veri.program.uyeler[2];
        ok = (u->tip == DUGUM_UYGULA)
          && (u->veri.uygula.ozellik_sayi == 1)  /* trait impl */
          && (u->veri.uygula.tip != NULL);
    }
    test_sonuc("uygula Trait icin Tip (trait impl)", ok);
    arena_serbest(a);
}

/* === Bound sozdizimi === */

static void test_bound_tekil(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* yapı V<T: Sayilabilir> { ic: T; } */
    Dugum *prog = parse_kaynak(
        "yap\xc4\xb1 V<T: Sayilabilir> { ic: T; }", a, &hata);
    int ok = prog && prog->veri.program.sayi == 1 && hata == 0;
    if (ok) {
        Dugum *y = prog->veri.program.uyeler[0];
        ok = (y->veri.yapi.tip_param_sayi == 1)
          && (y->veri.yapi.tip_param_bound_sayilari != NULL)
          && (y->veri.yapi.tip_param_bound_sayilari[0] == 1)
          && (y->veri.yapi.tip_param_boundlari[0][0] != NULL);
    }
    test_sonuc("bound tekil: <T: Sayilabilir>", ok);
    arena_serbest(a);
}

static void test_bound_coklu(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* yapı V<T: A + B + C> { ic: T; } */
    Dugum *prog = parse_kaynak(
        "yap\xc4\xb1 V<T: A + B + C> { ic: T; }", a, &hata);
    int ok = prog && prog->veri.program.sayi == 1 && hata == 0;
    if (ok) {
        Dugum *y = prog->veri.program.uyeler[0];
        ok = (y->veri.yapi.tip_param_sayi == 1)
          && (y->veri.yapi.tip_param_bound_sayilari[0] == 3);
    }
    test_sonuc("bound coklu: <T: A + B + C> (3 bound)", ok);
    arena_serbest(a);
}

static void test_bound_karisik(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* yapı W<T: A, U, V: B + C> { x: T; y: U; z: V; } */
    Dugum *prog = parse_kaynak(
        "yap\xc4\xb1 W<T: A, U, V: B + C> { x: T; y: U; z: V; }",
        a, &hata);
    int ok = prog && prog->veri.program.sayi == 1 && hata == 0;
    if (ok) {
        Dugum *y = prog->veri.program.uyeler[0];
        ok = (y->veri.yapi.tip_param_sayi == 3)
          && (y->veri.yapi.tip_param_bound_sayilari[0] == 1)  /* T: A */
          && (y->veri.yapi.tip_param_bound_sayilari[1] == 0)  /* U bound yok */
          && (y->veri.yapi.tip_param_bound_sayilari[2] == 2); /* V: B + C */
    }
    test_sonuc("bound karisik: <T: A, U, V: B + C>", ok);
    arena_serbest(a);
}

static void test_bound_ozellik(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* özellik X<T: A> { işlev f() -> T; } */
    Dugum *prog = parse_kaynak(
        "\xc3\xb6zellik X<T: A> { i\xc5\x9flev f() -> T; }", a, &hata);
    int ok = prog && prog->veri.program.sayi == 1 && hata == 0;
    if (ok) {
        Dugum *o = prog->veri.program.uyeler[0];
        ok = (o->tip == DUGUM_OZELLIK)
          && (o->veri.ozellik.tip_param_bound_sayilari[0] == 1);
    }
    test_sonuc("bound ozellik: ozellik X<T: A>", ok);
    arena_serbest(a);
}

static void test_bound_uygula(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* uygula<T: A + B> K<T> { işlev m() -> tam32 { ver 0; } } */
    Dugum *prog = parse_kaynak(
        "uygula<T: A + B> K<T> { i\xc5\x9flev m() -> tam32 { ver 0; } }",
        a, &hata);
    int ok = prog && prog->veri.program.sayi == 1 && hata == 0;
    if (ok) {
        Dugum *u = prog->veri.program.uyeler[0];
        ok = (u->tip == DUGUM_UYGULA)
          && (u->veri.uygula.tip_param_bound_sayilari[0] == 2);
    }
    test_sonuc("bound uygula: uygula<T: A + B>", ok);
    arena_serbest(a);
}

static void test_uygula_generic(void) {
    Arena *a = arena_olustur(0);
    int hata = -1;
    /* yapı K<T> { x: T; } uygula<T> K<T> { işlev m() -> T { ver ... } }
     * Sadece imza dogrulanir, govde basit. */
    Dugum *prog = parse_kaynak(
        "yap\xc4\xb1 K<T> { x: T; } "
        "uygula<T> K<T> { i\xc5\x9flev yeni() -> tam32 { ver 0; } }",
        a, &hata);
    int ok = prog && prog->veri.program.sayi == 2 && hata == 0;
    if (ok) {
        Dugum *u = prog->veri.program.uyeler[1];
        ok = (u->tip == DUGUM_UYGULA)
          && (u->veri.uygula.tip_param_sayi == 1)
          && (strcmp(u->veri.uygula.tip_paramlar[0], "T") == 0);
    }
    test_sonuc("uygula<T> generic", ok);
    arena_serbest(a);
}

/* === Main === */

int main(void) {
    /* Hata mesajlarini sustur (test ciktisini kirletmesin).
     * Hata sayilari hata_sayisi degiskeninden zaten alinir. */
    freopen("NUL", "w", stderr);

    printf("KEMGU Parser Test Paketi\n");
    printf("========================\n");

    printf("\n--- Bos / Tek ---\n");
    test_bos_program();

    printf("\n--- Islev ---\n");
    test_islev_bos();
    test_islev_parametreli();
    test_islev_ad_dogru();

    printf("\n--- Yapi ---\n");
    test_yapi_bos();
    test_yapi_alanli();

    printf("\n--- Ozellik (trait) ---\n");
    test_ozellik_bos();
    test_ozellik_imza();
    test_ozellik_default_impl();
    test_ozellik_generic();

    printf("\n--- Uygula (impl) ---\n");
    test_uygula_inherent();
    test_uygula_trait();
    test_uygula_generic();

    printf("\n--- Bound (constraint) ---\n");
    test_bound_tekil();
    test_bound_coklu();
    test_bound_karisik();
    test_bound_ozellik();
    test_bound_uygula();

    printf("\n--- Sabit ---\n");
    test_sabit();

    printf("\n--- Kullan ---\n");
    test_kullan_basit();
    test_kullan_yol();

    printf("\n--- Modul ---\n");
    test_modul_bos();
    test_modul_iceriger();

    printf("\n--- Disa ---\n");
    test_disa_islev();

    printf("\n--- Deyimler ---\n");
    test_degisken_deyimi();
    test_degisken_tip_belirt();
    test_ver_deger();
    test_ver_bos();
    test_ic_ice_blok();

    printf("\n--- Sayi Literalleri ---\n");
    test_sayi_hex();
    test_sayi_binary();
    test_sayi_underscore();

    printf("\n--- Cogul Ust Oge ---\n");
    test_cogul_ust_oge();

    printf("\n--- Hata Kurtarma ---\n");
    test_hata_eksik_noktali();
    test_hata_kurtarma_devam();
    test_hata_max_dur();
    test_parser_yapi_keyword_sonsuz_loop_yok();
    test_parser_ayni_pozisyon_tekrar_durur();
    test_parser_karisik_token_stream();
    test_parser_farkli_pozisyon_devam();

    printf("\n--- Atama / Ifade ---\n");
    test_atama_deyimi();
    test_ifade_deyimi();

    printf("\n--- Konum ---\n");
    test_konum_korunuyor();

    printf("\n--- Yazdirma ---\n");
    test_yazdirma_cokmuyor();

    printf("\n--- Stres ---\n");
    test_stres_50_islev();

    printf("\n--- Ifade: Birincil ---\n");
    test_ifade_tek_literal();

    printf("\n--- Ifade: Ikili Operatorler ---\n");
    test_ifade_toplama();
    test_ifade_oncelik();
    test_ifade_sol_birlesme();
    test_ifade_parantez();
    test_ifade_karsilastirma();
    test_ifade_mantik();
    test_ifade_esitlik();

    printf("\n--- Ifade: Onek ---\n");
    test_ifade_onek_neg();
    test_ifade_onek_degil();
    test_ifade_onek_degil_cagri();
    test_ifade_onek_degil_erisim();
    test_ifade_onek_degil_indeks();
    test_ifade_onek_degil_zincir();
    test_ifade_onek_degil_ikili_sonra();
    test_ifade_onek_ref();
    test_ifade_onek_ref_degisken();
    test_ifade_onek_deref();

    printf("\n--- Ifade: Sonek ---\n");
    test_ifade_cagri_bos();
    test_ifade_cagri_args();
    test_ifade_erisim();
    test_ifade_indeks();
    test_ifade_yol();
    test_ifade_sonek_zincir();

    printf("\n--- Ifade: Yapi/Dizi/Lambda ---\n");
    test_ifade_yapi_olusturma();
    test_ifade_yapi_trailing_comma();
    test_ifade_dizi_olusturma();
    test_ifade_dizi_bos();
    test_ifade_lambda();

    printf("\n--- Deyim: Eger ---\n");
    test_deyim_eger_basit();
    test_deyim_eger_degilse();
    test_deyim_eger_zincir();

    printf("\n--- Deyim: Iken / Icin ---\n");
    test_deyim_iken();
    test_deyim_icin();

    printf("\n--- Deyim: Esles ---\n");
    test_deyim_esles_literal();
    test_deyim_esles_yapici();
    test_deyim_esles_tamam_deseni();
    test_deyim_esles_tamam_joker();
    test_deyim_esles_sonuc_karma();
    test_deyim_esles_blok_govde();

    printf("\n--- Deyim: Guvensiz ---\n");
    test_deyim_guvensiz_basit();
    test_deyim_guvensiz_aciklama();

    printf("\n--- Karmasik Tipler ---\n");
    test_tip_basit();
    test_tip_referans();
    test_tip_referans_degisken();
    test_tip_pointer();
    test_tip_secimlik();
    test_tip_sonuc();
    test_tip_dizi();
    test_tip_islev();
    test_tip_kullanici_generic();
    test_tip_ic_ice();

    printf("\n--- Generic Yapi ---\n");
    test_yapi_generic();

    printf("\n--- Ornek .kem Dosyalari ---\n");
    test_ornek_fibonacci();
    test_ornek_yapilar();
    test_ornek_eslesme();
    test_ornek_hasta();

    printf("\n========================\n");
    printf("Toplam: %d | Basarili: %d | Basarisiz: %d\n",
           toplam_test, basarili, basarisiz);
    return basarisiz > 0 ? 1 : 0;
}
