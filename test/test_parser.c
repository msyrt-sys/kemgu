#include "parser.h"
#include "lexer.h"
#include "ast.h"
#include "ast_yazdir.h"
#include "arena.h"

#include <stdio.h>
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

    printf("\n--- Atama / Ifade ---\n");
    test_atama_deyimi();
    test_ifade_deyimi();

    printf("\n--- Konum ---\n");
    test_konum_korunuyor();

    printf("\n--- Yazdirma ---\n");
    test_yazdirma_cokmuyor();

    printf("\n--- Stres ---\n");
    test_stres_50_islev();

    printf("\n========================\n");
    printf("Toplam: %d | Basarili: %d | Basarisiz: %d\n",
           toplam_test, basarili, basarisiz);
    return basarisiz > 0 ? 1 : 0;
}
