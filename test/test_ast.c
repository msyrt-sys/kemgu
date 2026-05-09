#include "ast.h"
#include "ast_yazdir.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

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

/* === Enum tutarliligi === */

static void test_enum_dugum_tipi_adi(void) {
    int ok = (strcmp(dugum_tipi_adi(DUGUM_PROGRAM), "PROGRAM") == 0)
          && (strcmp(dugum_tipi_adi(DUGUM_IKILI),   "IKILI") == 0)
          && (strcmp(dugum_tipi_adi(DUGUM_TAM),     "TAM") == 0)
          && (strcmp(dugum_tipi_adi(DUGUM_HATA),    "HATA") == 0);
    test_sonuc("dugum_tipi_adi enum -> string", ok);
}

static void test_enum_operator_adi(void) {
    int ok = (strcmp(operator_adi(OP_ARTI),  "+") == 0)
          && (strcmp(operator_adi(OP_VEYA),  "veya") == 0)
          && (strcmp(operator_adi(OP_NEG),   "neg") == 0)
          && (strcmp(operator_adi(OP_DEGIL), "degil") == 0);
    test_sonuc("operator_adi enum -> string", ok);
}

/* === Generic olusturucu === */

static void test_dugum_olustur_temel(void) {
    Arena *a = arena_olustur(0);
    Dugum *d = dugum_olustur(a, DUGUM_PROGRAM, 1, 1);
    int ok = (d != NULL)
          && (d->tip == DUGUM_PROGRAM)
          && (d->satir == 1)
          && (d->sutun == 1);
    test_sonuc("dugum_olustur tip+konum dogru", ok);
    arena_serbest(a);
}

static void test_dugum_olustur_sifirli(void) {
    /* arena_ayir_sifir ile baslatilmali — union icerigi 0 */
    Arena *a = arena_olustur(0);
    Dugum *d = dugum_olustur(a, DUGUM_TAM, 5, 10);
    /* veri.tam.deger varsayilan 0 olmali */
    int ok = (d->veri.tam.deger == 0);
    test_sonuc("dugum_olustur veri sifirli", ok);
    arena_serbest(a);
}

static void test_dugum_olustur_null_arena(void) {
    Dugum *d = dugum_olustur(NULL, DUGUM_TAM, 1, 1);
    test_sonuc("dugum_olustur(NULL) NULL doner", d == NULL);
}

/* === Literaller === */

static void test_dugum_tam(void) {
    Arena *a = arena_olustur(0);
    Dugum *d = dugum_tam(a, 12345, 2, 3);
    int ok = (d->tip == DUGUM_TAM)
          && (d->veri.tam.deger == 12345)
          && (d->satir == 2) && (d->sutun == 3);
    test_sonuc("dugum_tam(12345)", ok);
    arena_serbest(a);
}

static void test_dugum_tam_int64_max(void) {
    Arena *a = arena_olustur(0);
    Dugum *d = dugum_tam(a, INT64_MAX, 1, 1);
    int ok = (d->veri.tam.deger == INT64_MAX);
    test_sonuc("dugum_tam(INT64_MAX) overflow yok", ok);
    arena_serbest(a);
}

static void test_dugum_tam_negatif(void) {
    Arena *a = arena_olustur(0);
    Dugum *d = dugum_tam(a, -42, 1, 1);
    int ok = (d->veri.tam.deger == -42);
    test_sonuc("dugum_tam(-42) negatif", ok);
    arena_serbest(a);
}

static void test_dugum_kesirli(void) {
    Arena *a = arena_olustur(0);
    Dugum *d = dugum_kesirli(a, 3.14, 1, 1);
    int ok = (d->tip == DUGUM_KESIRLI) && (d->veri.kesirli.deger == 3.14);
    test_sonuc("dugum_kesirli(3.14)", ok);
    arena_serbest(a);
}

static void test_dugum_metin(void) {
    Arena *a = arena_olustur(0);
    const char *kaynak = "test";
    Dugum *d = dugum_metin(a, kaynak, 4, 1, 1);
    int ok = (d->tip == DUGUM_METIN)
          && (d->veri.metin_lit.uzunluk == 4)
          && (d->veri.metin_lit.metin != NULL)
          && (memcmp(d->veri.metin_lit.metin, "test", 4) == 0)
          && (d->veri.metin_lit.metin[4] == '\0');  /* null-terminated */
    test_sonuc("dugum_metin null-terminate", ok);
    arena_serbest(a);
}

static void test_dugum_metin_turkce(void) {
    Arena *a = arena_olustur(0);
    /* "değişken" UTF-8 = de\xc4\x9fi\xc5\x9fken (10 byte) */
    const char *kaynak = "de\xc4\x9f" "i\xc5\x9fken";
    int uz = (int)strlen(kaynak);
    Dugum *d = dugum_metin(a, kaynak, uz, 1, 1);
    int ok = (d->veri.metin_lit.uzunluk == uz)
          && (memcmp(d->veri.metin_lit.metin, kaynak, (size_t)uz) == 0)
          && (d->veri.metin_lit.metin[uz] == '\0');
    test_sonuc("dugum_metin Turkce UTF-8", ok);
    arena_serbest(a);
}

static void test_dugum_karakter(void) {
    Arena *a = arena_olustur(0);
    Dugum *d = dugum_karakter(a, 0x1F600, 1, 1);  /* emoji codepoint */
    int ok = (d->veri.karakter.kod_noktasi == 0x1F600);
    test_sonuc("dugum_karakter Unicode codepoint", ok);
    arena_serbest(a);
}

static void test_dugum_mantiksal(void) {
    Arena *a = arena_olustur(0);
    Dugum *dt = dugum_mantiksal(a, 1, 1, 1);
    Dugum *df = dugum_mantiksal(a, 0, 1, 1);
    Dugum *d2 = dugum_mantiksal(a, 99, 1, 1);  /* >0 -> 1 */
    int ok = (dt->veri.mantiksal.deger == 1)
          && (df->veri.mantiksal.deger == 0)
          && (d2->veri.mantiksal.deger == 1);
    test_sonuc("dugum_mantiksal normalize 0/1", ok);
    arena_serbest(a);
}

static void test_dugum_bos(void) {
    Arena *a = arena_olustur(0);
    Dugum *d = dugum_bos(a, 1, 1);
    int ok = (d != NULL) && (d->tip == DUGUM_BOS);
    test_sonuc("dugum_bos", ok);
    arena_serbest(a);
}

static void test_dugum_tanimlayici(void) {
    Arena *a = arena_olustur(0);
    Dugum *d = dugum_tanimlayici(a, "x", 1, 1, 1);
    int ok = (d->tip == DUGUM_TANIMLAYICI)
          && (strcmp(d->veri.tanimlayici.metin, "x") == 0)
          && (d->veri.tanimlayici.uzunluk == 1);
    test_sonuc("dugum_tanimlayici", ok);
    arena_serbest(a);
}

/* === Ifadeler === */

static void test_dugum_ikili(void) {
    Arena *a = arena_olustur(0);
    Dugum *sol = dugum_tam(a, 1, 1, 1);
    Dugum *sag = dugum_tam(a, 2, 1, 5);
    Dugum *d = dugum_ikili(a, OP_ARTI, sol, sag, 1, 3);
    int ok = (d->tip == DUGUM_IKILI)
          && (d->veri.ikili.op == OP_ARTI)
          && (d->veri.ikili.sol == sol)
          && (d->veri.ikili.sag == sag);
    test_sonuc("dugum_ikili OP_ARTI", ok);
    arena_serbest(a);
}

static void test_dugum_tekli(void) {
    Arena *a = arena_olustur(0);
    Dugum *operand = dugum_tam(a, 5, 1, 2);
    Dugum *d = dugum_tekli(a, OP_NEG, operand, 1, 1);
    int ok = (d->tip == DUGUM_TEKLI)
          && (d->veri.tekli.op == OP_NEG)
          && (d->veri.tekli.operand == operand);
    test_sonuc("dugum_tekli OP_NEG", ok);
    arena_serbest(a);
}

/* === Konteyner: blok, program === */

static void test_dugum_blok_bos(void) {
    Arena *a = arena_olustur(0);
    Dugum *d = dugum_blok(a, NULL, 0, 1, 1);
    int ok = (d->tip == DUGUM_BLOK) && (d->veri.blok.sayi == 0);
    test_sonuc("dugum_blok bos", ok);
    arena_serbest(a);
}

static void test_dugum_blok_uc_deyim(void) {
    Arena *a = arena_olustur(0);
    Dugum **deyimler = (Dugum **)arena_ayir(a, sizeof(Dugum *) * 3);
    deyimler[0] = dugum_tam(a, 1, 1, 1);
    deyimler[1] = dugum_tam(a, 2, 2, 1);
    deyimler[2] = dugum_tam(a, 3, 3, 1);
    Dugum *d = dugum_blok(a, deyimler, 3, 1, 1);
    int ok = (d->veri.blok.sayi == 3)
          && (d->veri.blok.deyimler[0]->veri.tam.deger == 1)
          && (d->veri.blok.deyimler[1]->veri.tam.deger == 2)
          && (d->veri.blok.deyimler[2]->veri.tam.deger == 3);
    test_sonuc("dugum_blok 3 deyim sirayla", ok);
    arena_serbest(a);
}

static void test_dugum_program(void) {
    Arena *a = arena_olustur(0);
    Dugum **uyeler = (Dugum **)arena_ayir(a, sizeof(Dugum *) * 1);
    uyeler[0] = dugum_olustur(a, DUGUM_ISLEV, 1, 1);
    Dugum *p = dugum_program(a, uyeler, 1, 1, 1);
    int ok = (p->tip == DUGUM_PROGRAM) && (p->veri.program.sayi == 1);
    test_sonuc("dugum_program tek uye", ok);
    arena_serbest(a);
}

/* === Eger / Ver === */

static void test_dugum_eger_gozdoldur_yan(void) {
    Arena *a = arena_olustur(0);
    Dugum *kosul = dugum_mantiksal(a, 1, 1, 1);
    Dugum *gd = dugum_blok(a, NULL, 0, 1, 5);
    Dugum *yan = dugum_blok(a, NULL, 0, 3, 5);
    Dugum *d = dugum_eger(a, kosul, gd, yan, 1, 1);
    int ok = (d->tip == DUGUM_EGER)
          && (d->veri.eger.kosul == kosul)
          && (d->veri.eger.gozdoldur == gd)
          && (d->veri.eger.yan == yan);
    test_sonuc("dugum_eger kosul/gozdoldur/yan", ok);
    arena_serbest(a);
}

static void test_dugum_ver_deger(void) {
    Arena *a = arena_olustur(0);
    Dugum *deger = dugum_tam(a, 42, 1, 5);
    Dugum *v = dugum_ver(a, deger, 1, 1);
    int ok = (v->tip == DUGUM_VER) && (v->veri.ver.deger == deger);
    test_sonuc("dugum_ver(deger)", ok);
    arena_serbest(a);
}

static void test_dugum_ver_bos(void) {
    Arena *a = arena_olustur(0);
    Dugum *v = dugum_ver(a, NULL, 1, 1);
    int ok = (v->tip == DUGUM_VER) && (v->veri.ver.deger == NULL);
    test_sonuc("dugum_ver(NULL) ver; ifadesi", ok);
    arena_serbest(a);
}

/* === Hata === */

static void test_dugum_hata(void) {
    Arena *a = arena_olustur(0);
    Dugum *d = dugum_hata(a, 5, 10);
    int ok = (d->tip == DUGUM_HATA) && (d->satir == 5) && (d->sutun == 10);
    test_sonuc("dugum_hata error placeholder", ok);
    arena_serbest(a);
}

/* === ast_string_kopyala === */

static void test_string_kopyala(void) {
    Arena *a = arena_olustur(0);
    char *k = ast_string_kopyala(a, "hello", 5);
    int ok = (k != NULL) && (memcmp(k, "hello", 5) == 0) && (k[5] == '\0');
    test_sonuc("ast_string_kopyala null-term", ok);
    arena_serbest(a);
}

static void test_string_kopyala_null(void) {
    Arena *a = arena_olustur(0);
    char *k = ast_string_kopyala(a, NULL, 5);
    test_sonuc("ast_string_kopyala(NULL) -> NULL", k == NULL);
    arena_serbest(a);
}

/* === Yazdirma (cokmuyor + ASan temiz) === */

static void test_yazdir_basit_literal(void) {
    Arena *a = arena_olustur(0);
    Dugum *d = dugum_tam(a, 42, 1, 1);
    /* tmpfile() ile cikti yakala (cok karmasiklasmasin diye yok say) */
    FILE *bos = fopen("NUL", "w");  /* Windows null device */
    if (!bos) bos = stdout;          /* fallback */
    ast_yazdir(d, bos);
    if (bos != stdout) fclose(bos);
    test_sonuc("ast_yazdir basit literal cokmuyor", 1);
    arena_serbest(a);
}

static void test_yazdir_null(void) {
    FILE *bos = fopen("NUL", "w");
    if (!bos) bos = stdout;
    ast_yazdir(NULL, bos);  /* NULL guvenli olmali */
    if (bos != stdout) fclose(bos);
    test_sonuc("ast_yazdir(NULL) cokmuyor", 1);
}

static void test_yazdir_karmasik(void) {
    /* Mini program: islev(yok) -> ver 1 + 2 */
    Arena *a = arena_olustur(0);
    Dugum *bir = dugum_tam(a, 1, 1, 1);
    Dugum *iki = dugum_tam(a, 2, 1, 5);
    Dugum *toplam = dugum_ikili(a, OP_ARTI, bir, iki, 1, 3);
    Dugum *ver_d = dugum_ver(a, toplam, 1, 1);

    Dugum **deyimler = (Dugum **)arena_ayir(a, sizeof(Dugum *));
    deyimler[0] = ver_d;
    Dugum *blok = dugum_blok(a, deyimler, 1, 1, 1);

    FILE *bos = fopen("NUL", "w");
    if (!bos) bos = stdout;
    ast_yazdir(blok, bos);
    if (bos != stdout) fclose(bos);
    test_sonuc("ast_yazdir karmasik AST cokmuyor", 1);
    arena_serbest(a);
}

/* === Stres === */

static void test_stres_1000_dugum(void) {
    Arena *a = arena_olustur(0);
    int ok = 1;
    for (int i = 0; i < 1000; i++) {
        Dugum *d = dugum_tam(a, i, 1, 1);
        if (!d || d->veri.tam.deger != i) { ok = 0; break; }
    }
    test_sonuc("1000 dugum yarat (ASan sizinti yok)", ok);
    arena_serbest(a);
}

/* === Karmasik AST yapisi === */

static void test_karmasik_program(void) {
    /* fibonacci-misal:
     *   eger n < 2 { ver n; }
     *   degilse    { ver fib(n-1) + fib(n-2); }
     */
    Arena *a = arena_olustur(0);

    Dugum *n_id = dugum_tanimlayici(a, "n", 1, 1, 1);
    Dugum *iki  = dugum_tam(a, 2, 1, 5);
    Dugum *kosul = dugum_ikili(a, OP_KUCUK, n_id, iki, 1, 3);

    Dugum *gd_n = dugum_tanimlayici(a, "n", 1, 2, 8);
    Dugum *gd_ver = dugum_ver(a, gd_n, 2, 4);
    Dugum **gd_deyim = (Dugum **)arena_ayir(a, sizeof(Dugum *));
    gd_deyim[0] = gd_ver;
    Dugum *gd_blok = dugum_blok(a, gd_deyim, 1, 2, 1);

    Dugum *eger = dugum_eger(a, kosul, gd_blok, NULL, 1, 1);

    int ok = (eger->tip == DUGUM_EGER)
          && (eger->veri.eger.kosul->tip == DUGUM_IKILI)
          && (eger->veri.eger.kosul->veri.ikili.op == OP_KUCUK)
          && (eger->veri.eger.gozdoldur->tip == DUGUM_BLOK)
          && (eger->veri.eger.gozdoldur->veri.blok.sayi == 1);
    test_sonuc("karmasik AST: eger + ikili + ver", ok);

    arena_serbest(a);
}

/* === Main === */

int main(void) {
    printf("KEMGU AST Test Paketi\n");
    printf("=====================\n");

    printf("\n--- Enum Tutarliligi ---\n");
    test_enum_dugum_tipi_adi();
    test_enum_operator_adi();

    printf("\n--- Generic Olusturucu ---\n");
    test_dugum_olustur_temel();
    test_dugum_olustur_sifirli();
    test_dugum_olustur_null_arena();

    printf("\n--- Literaller ---\n");
    test_dugum_tam();
    test_dugum_tam_int64_max();
    test_dugum_tam_negatif();
    test_dugum_kesirli();
    test_dugum_metin();
    test_dugum_metin_turkce();
    test_dugum_karakter();
    test_dugum_mantiksal();
    test_dugum_bos();
    test_dugum_tanimlayici();

    printf("\n--- Ifadeler ---\n");
    test_dugum_ikili();
    test_dugum_tekli();

    printf("\n--- Konteyner ---\n");
    test_dugum_blok_bos();
    test_dugum_blok_uc_deyim();
    test_dugum_program();

    printf("\n--- Eger / Ver ---\n");
    test_dugum_eger_gozdoldur_yan();
    test_dugum_ver_deger();
    test_dugum_ver_bos();

    printf("\n--- Hata ---\n");
    test_dugum_hata();

    printf("\n--- String Yardimcisi ---\n");
    test_string_kopyala();
    test_string_kopyala_null();

    printf("\n--- Yazdirma ---\n");
    test_yazdir_basit_literal();
    test_yazdir_null();
    test_yazdir_karmasik();

    printf("\n--- Stres ---\n");
    test_stres_1000_dugum();

    printf("\n--- Karmasik ---\n");
    test_karmasik_program();

    printf("\n=====================\n");
    printf("Toplam: %d | Basarili: %d | Basarisiz: %d\n",
           toplam_test, basarili, basarisiz);
    return basarisiz > 0 ? 1 : 0;
}
