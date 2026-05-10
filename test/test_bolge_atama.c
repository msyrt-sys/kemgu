#include "bolge_atama.h"
#include "bolge.h"
#include "ast.h"
#include "parser.h"
#include "lexer.h"
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

/* Yardimci: ifadenin bolgesini belirle (sabit context'te) */
static BolgeBilgisi *ifade_bolgesi(const char *ifade_kaynak, Arena *a,
                                    int ver_baglaminda) {
    char buf[1024];
    snprintf(buf, sizeof(buf), "sabit _X: tam32 = %s;", ifade_kaynak);
    Lexer l;
    lexer_baslat(&l, buf, "test");
    Parser p;
    parser_baslat(&p, &l, a, "test", buf);
    Dugum *prog = parser_calistir(&p);
    if (!prog || prog->veri.program.sayi == 0) return NULL;
    Dugum *sabit = prog->veri.program.uyeler[0];
    Dugum *ifade = sabit->veri.sabit.deger;

    BolgeAtama ba;
    bolge_atama_baslat(&ba, a, "test", 4);
    ba.ver_baglaminda = ver_baglaminda;
    return bolge_belirle(&ba, ifade);
}

/* === R-LIT: basit literaller === */

static void test_r_lit_tam(void) {
    Arena *a = arena_olustur(0);
    BolgeBilgisi *b = ifade_bolgesi("42", a, 0);
    test_sonuc("R-LIT: 42 -> LIT",
               b && b->kategori == BOLGE_LIT);
    arena_serbest(a);
}

static void test_r_lit_dogru(void) {
    Arena *a = arena_olustur(0);
    BolgeBilgisi *b = ifade_bolgesi("do\xc4\x9fru", a, 0);
    test_sonuc("R-LIT: dogru -> LIT",
               b && b->kategori == BOLGE_LIT);
    arena_serbest(a);
}

/* === R-YEREL: bilesik literaller === */

static void test_r_yerel_metin(void) {
    Arena *a = arena_olustur(0);
    BolgeBilgisi *b = ifade_bolgesi("\"hello\"", a, 0);
    test_sonuc("R-YEREL: \"hello\" -> YEREL",
               b && b->kategori == BOLGE_YEREL);
    arena_serbest(a);
}

static void test_r_yerel_dizi(void) {
    Arena *a = arena_olustur(0);
    BolgeBilgisi *b = ifade_bolgesi("[1, 2, 3]", a, 0);
    test_sonuc("R-YEREL: [1,2,3] -> YEREL",
               b && b->kategori == BOLGE_YEREL);
    arena_serbest(a);
}

/* === R-VER: ver baglaminda escape === */

static void test_r_ver_metin(void) {
    Arena *a = arena_olustur(0);
    BolgeBilgisi *b = ifade_bolgesi("\"hello\"", a, 1);
    test_sonuc("R-VER: \"hello\" + ver -> CAGIRAN",
               b && b->kategori == BOLGE_CAGIRAN);
    arena_serbest(a);
}

static void test_r_ver_dizi(void) {
    Arena *a = arena_olustur(0);
    BolgeBilgisi *b = ifade_bolgesi("[1, 2, 3]", a, 1);
    test_sonuc("R-VER: [1,2,3] + ver -> CAGIRAN",
               b && b->kategori == BOLGE_CAGIRAN);
    arena_serbest(a);
}

static void test_r_ver_tam_lit(void) {
    Arena *a = arena_olustur(0);
    /* basit literal ver olsa bile LIT (stack, kopyalama) */
    BolgeBilgisi *b = ifade_bolgesi("42", a, 1);
    test_sonuc("R-VER: 42 + ver hala LIT (basit kopya)",
               b && b->kategori == BOLGE_LIT);
    arena_serbest(a);
}

/* === R-KOSUL: kosullu dallanma === */

static void test_r_kosul_basit(void) {
    Arena *a = arena_olustur(0);
    /* Manuel: eger dogru { metin1 } yan { metin2 } -> LCA */
    /* parse_ifade'de eger dugum olmadigi icin manuel olusturayim */
    Dugum *kosul = dugum_mantiksal(a, 1, 1, 1);
    Dugum *gd_ifade = dugum_metin(a, "x", 1, 1, 1);

    /* Blok yapma yerine dogrudan ifade verecek sekilde manuel:
     * Aslinda DUGUM_EGER kosul + gozdoldur (BLOK) + yan (BLOK) bekler. */
    Dugum **gd_dey = (Dugum **)arena_ayir(a, sizeof(Dugum *));
    gd_dey[0] = gd_ifade;
    Dugum *gd_blok = dugum_blok(a, gd_dey, 1, 1, 1);

    Dugum *yan_ifade = dugum_metin(a, "y", 1, 1, 1);
    Dugum **yan_dey = (Dugum **)arena_ayir(a, sizeof(Dugum *));
    yan_dey[0] = yan_ifade;
    Dugum *yan_blok = dugum_blok(a, yan_dey, 1, 1, 1);

    Dugum *eger = dugum_eger(a, kosul, gd_blok, yan_blok, 1, 1);

    BolgeAtama ba;
    bolge_atama_baslat(&ba, a, "test", 4);
    BolgeBilgisi *b = bolge_belirle(&ba, eger);
    /* Iki dal da YEREL -> LCA YEREL */
    test_sonuc("R-KOSUL: dallar yerel -> LCA yerel",
               b && b->kategori == BOLGE_YEREL);
    arena_serbest(a);
}

/* === Erisim recursive === */

static void test_erisim_yerel(void) {
    Arena *a = arena_olustur(0);
    /* Manuel: x.y — x default yerel, y de yerel */
    Dugum *x = dugum_tanimlayici(a, "x", 1, 1, 1);
    Dugum *erisim = dugum_olustur(a, DUGUM_ERISIM, 1, 1);
    erisim->veri.erisim.nesne = x;
    erisim->veri.erisim.alan = ast_string_kopyala(a, "y", 1);
    erisim->veri.erisim.alan_uzunluk = 1;

    BolgeAtama ba;
    bolge_atama_baslat(&ba, a, "test", 4);
    BolgeBilgisi *b = bolge_belirle(&ba, erisim);
    test_sonuc("erisim x.y -> nesne bolgesi (yerel)",
               b && b->kategori == BOLGE_YEREL);
    arena_serbest(a);
}

/* === Yol -> global === */

static void test_yol_global(void) {
    Arena *a = arena_olustur(0);
    /* Manuel: x::y */
    Dugum *yol = dugum_olustur(a, DUGUM_YOL, 1, 1);
    yol->veri.yol.sol = dugum_tanimlayici(a, "x", 1, 1, 1);
    yol->veri.yol.sag_ad = ast_string_kopyala(a, "y", 1);
    yol->veri.yol.sag_ad_uzunluk = 1;

    BolgeAtama ba;
    bolge_atama_baslat(&ba, a, "test", 4);
    BolgeBilgisi *b = bolge_belirle(&ba, yol);
    test_sonuc("YOL x::y -> GLOBAL",
               b && b->kategori == BOLGE_GLOBAL);
    arena_serbest(a);
}

/* === Main === */

int main(void) {
    printf("KEMGU Bolge Atama Test Paketi\n");
    printf("==============================\n");

    printf("\n--- R-LIT ---\n");
    test_r_lit_tam();
    test_r_lit_dogru();

    printf("\n--- R-YEREL ---\n");
    test_r_yerel_metin();
    test_r_yerel_dizi();

    printf("\n--- R-VER ---\n");
    test_r_ver_metin();
    test_r_ver_dizi();
    test_r_ver_tam_lit();

    printf("\n--- R-KOSUL (LCA) ---\n");
    test_r_kosul_basit();

    printf("\n--- Sonek ---\n");
    test_erisim_yerel();
    test_yol_global();

    printf("\n==============================\n");
    printf("Toplam: %d | Basarili: %d | Basarisiz: %d\n",
           toplam_test, basarili, basarisiz);
    return basarisiz > 0 ? 1 : 0;
}
