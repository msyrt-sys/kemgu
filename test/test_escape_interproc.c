#include "tip_kontrol.h"
#include "parser.h"
#include "lexer.h"
#include "ast.h"
#include "tip.h"
#include "sembol.h"
#include "arena.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * KEMGU Inter-procedural Escape Analizi V1 — Test Paketi (Faz 4)
 * ===============================================================
 *
 * R-YAKALAMA-THREAD (DRF Op.Sem section9): closure capture sonrasi
 * çağıran thread'in yakalanan değişkene erişimi DRF007 hatası verir.
 *
 * Faz 4 V1: tip_kontrol.c mark_lambda_captures + Sembol.thread_transferred
 * flag ile lokal kapatma analizi. Tam inter-proc summary (callee escape
 * summary, worklist + fixpoint) V2'ye sakli; bu test paketi V1'in
 * yakaladigi senaryolari kapsar:
 *
 *   E1-E5:   Tek free var capture (Dizi/yapı/parametre/değer/zincir)
 *   E6-E10:  Çoklu erişim / çoklu var / iç içe görev
 *   E11-E15: Pozitif (free var yok, lambda iç değişken, parametre shadow)
 *   E16-E18: V1 sinirlari ve recursive konservatif placeholder
 *
 * Negative testler en az 1 hata (DRF007 dahil) bekler.
 * Positive testler 0 hata bekler.
 */

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

static int hata_sayisi(const char *kaynak) {
    Arena *a = arena_olustur(0);
    Lexer l;
    lexer_baslat(&l, kaynak, "test");
    Parser p;
    parser_baslat(&p, &l, a, "test", kaynak);
    Dugum *prog = parser_calistir(&p);
    if (!prog || p.hata_sayisi > 0) {
        int r = p.hata_sayisi + 1000;
        arena_serbest(a);
        return r;
    }
    Scope *g = scope_olustur(a, SCOPE_GLOBAL, NULL);
    TipKontrol tk;
    tip_kontrol_baslat(&tk, a, g, "test", kaynak);
    tip_kontrol_program(&tk, prog);
    int h = tk.hata_sayisi;
    arena_serbest(a);
    return h;
}

static int kontrol_main(const char *govde) {
    static char buf[8192];
    snprintf(buf, sizeof(buf),
        "i\xc5\x9flev test() {\n%s\n}\n", govde);
    return hata_sayisi(buf);
}

/* ============================================================
 * NEGATIVE: Free var capture sonrasi erisim -> DRF007
 * ============================================================ */

static void E1_dizi_capture_indeks(void) {
    /* D38 varyanti: Dizi capture, sonra dizi indeksleme */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken xs: Dizi<tam32> = [1, 2, 3];\n"
        "    de\xc4\x9fi\xc5\x9fken g = g\xc3\xb6rev_ba\xc5\x9flat(|| xs[0]);\n"
        "    de\xc4\x9fi\xc5\x9fken r = g\xc3\xb6rev_birle\xc5\x9ftir(g);\n"
        "    de\xc4\x9fi\xc5\x9fken s: tam32 = xs[1];\n");
    test_sonuc("E1 Dizi capture + sonra indeks -> DRF007", h >= 1);
}

static void E2_tam32_capture_aritmetik(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken n: tam32 = 10;\n"
        "    de\xc4\x9fi\xc5\x9fken g = g\xc3\xb6rev_ba\xc5\x9flat(|| n);\n"
        "    de\xc4\x9fi\xc5\x9fken s: tam32 = n + 1;\n");
    test_sonuc("E2 tam32 capture + sonra aritmetik -> DRF007", h >= 1);
}

static void E3_capture_sonra_atama(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken x: tam32 = 5;\n"
        "    de\xc4\x9fi\xc5\x9fken g = g\xc3\xb6rev_ba\xc5\x9flat(|| x);\n"
        "    x = 7;\n");
    test_sonuc("E3 capture sonra atama -> DRF007 (lvalue)", h >= 1);
}

static void E4_iki_var_capture(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken a: tam32 = 1;\n"
        "    de\xc4\x9fi\xc5\x9fken b: tam32 = 2;\n"
        "    de\xc4\x9fi\xc5\x9fken g = g\xc3\xb6rev_ba\xc5\x9flat(|| a + b);\n"
        "    de\xc4\x9fi\xc5\x9fken s: tam32 = a + b;\n");
    test_sonuc("E4 iki var capture, ikisini eris -> >=2 DRF007", h >= 2);
}

static void E5_capture_indeks_arg(void) {
    /* indeks ifadesinde free var, hem nesne hem index olabilir */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken xs: Dizi<tam32> = [10, 20, 30];\n"
        "    de\xc4\x9fi\xc5\x9fken i: tam32 = 1;\n"
        "    de\xc4\x9fi\xc5\x9fken g = g\xc3\xb6rev_ba\xc5\x9flat(|| xs[i]);\n"
        "    de\xc4\x9fi\xc5\x9fken s: tam32 = i;\n");
    test_sonuc("E5 capture index arg + sonra erisim -> DRF007", h >= 1);
}

static void E6_capture_sonra_baska_lambda(void) {
    /* Iki ayri gorev_baslat + iki ayri free var */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken a: tam32 = 10;\n"
        "    de\xc4\x9fi\xc5\x9fken b: tam32 = 20;\n"
        "    de\xc4\x9fi\xc5\x9fken g1 = g\xc3\xb6rev_ba\xc5\x9flat(|| a);\n"
        "    de\xc4\x9fi\xc5\x9fken g2 = g\xc3\xb6rev_ba\xc5\x9flat(|| b);\n"
        "    de\xc4\x9fi\xc5\x9fken s: tam32 = a + b;\n");
    test_sonuc("E6 iki gorev_baslat + iki capture -> >=2 DRF007", h >= 2);
}

static void E7_capture_sonra_yeni_atama_okuma(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken c: tam32 = 7;\n"
        "    de\xc4\x9fi\xc5\x9fken g = g\xc3\xb6rev_ba\xc5\x9flat(|| c * 2);\n"
        "    de\xc4\x9fi\xc5\x9fken y: tam32 = c;\n"
        "    de\xc4\x9fi\xc5\x9fken z: tam32 = c;\n");
    test_sonuc("E7 capture + 2x okuma -> >=2 DRF007", h >= 2);
}

static void E8_capture_birlestir_sonra_var_okuma(void) {
    /* Birlestirme transferred state'i degistirmemeli */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken x: tam32 = 42;\n"
        "    de\xc4\x9fi\xc5\x9fken g = g\xc3\xb6rev_ba\xc5\x9flat(|| x);\n"
        "    de\xc4\x9fi\xc5\x9fken r: tam32 = g\xc3\xb6rev_birle\xc5\x9ftir(g);\n"
        "    de\xc4\x9fi\xc5\x9fken s: tam32 = x;\n");
    test_sonuc("E8 capture, join, sonra x okuma -> DRF007", h >= 1);
}

static void E9_capture_param_olarak(void) {
    /* Kapsayan parametre capture'i da DRF007 verir */
    int kaynak_h = hata_sayisi(
        "i\xc5\x9flev test(n: tam32) -> tam32 {\n"
        "  de\xc4\x9fi\xc5\x9fken g = g\xc3\xb6rev_ba\xc5\x9flat(|| n + 1);\n"
        "  ver n;\n"
        "}\n");
    test_sonuc("E9 parametre capture + okuma -> DRF007", kaynak_h >= 1);
}

static void E10_capture_yapi_alan(void) {
    int h = hata_sayisi(
        "yap\xc4\xb1 Nokta { x: tam32; y: tam32; }\n"
        "i\xc5\x9flev test() {\n"
        "  de\xc4\x9fi\xc5\x9fken p = Nokta { x: 1, y: 2 };\n"
        "  de\xc4\x9fi\xc5\x9fken g = g\xc3\xb6rev_ba\xc5\x9flat(|| p.x);\n"
        "  de\xc4\x9fi\xc5\x9fken s: tam32 = p.y;\n"
        "}\n");
    test_sonuc("E10 yapi capture + alan erisim -> DRF007", h >= 1);
}

/* ============================================================
 * POSITIVE: Capture YOK veya skip listesinde
 * ============================================================ */

static void E11_lambda_param_okuma(void) {
    /* Lambda parametresi skip listesinde, erisim DRF007 yok.
     * Dis kapsayan'da hicbir capture yok. */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken f: i\xc5\x9flev(tam32) -> tam32 = |x: tam32| x + 1;\n");
    test_sonuc("E11 lambda param skip + dis capture yok -> 0 hata", h == 0);
}

static void E12_lambda_yok_capture(void) {
    /* Lambda gövdesinde sabit ifade — yakalama yok */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken g = g\xc3\xb6rev_ba\xc5\x9flat(|| 42);\n"
        "    de\xc4\x9fi\xc5\x9fken r: tam32 = g\xc3\xb6rev_birle\xc5\x9ftir(g);\n");
    test_sonuc("E12 gorev_baslat sabit ifade lambda -> 0 hata", h == 0);
}

static void E13_baslatmadan_lambda(void) {
    /* Lambda yaratma var ama gorev_baslat YOK -> transfer yok */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken n: tam32 = 10;\n"
        "    de\xc4\x9fi\xc5\x9fken f: i\xc5\x9flev() -> tam32 = || n;\n"
        "    de\xc4\x9fi\xc5\x9fken s: tam32 = n;\n");
    test_sonuc("E13 lambda yarat ama gorev_baslat yok -> 0 hata", h == 0);
}

static void E14_capture_kullanim_yok(void) {
    /* Capture var ama sonrasinda kapsayan'da kullanilmiyor -> 0 hata */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken n: tam32 = 10;\n"
        "    de\xc4\x9fi\xc5\x9fken g = g\xc3\xb6rev_ba\xc5\x9flat(|| n);\n"
        "    de\xc4\x9fi\xc5\x9fken r: tam32 = g\xc3\xb6rev_birle\xc5\x9ftir(g);\n");
    test_sonuc("E14 capture ama kapsayan'da kullanim yok -> 0 hata", h == 0);
}

static void E15_lambda_ic_var(void) {
    /* Lambda body'inde lokal var; transfer YOK */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken g = g\xc3\xb6rev_ba\xc5\x9flat(|| 5);\n"
        "    de\xc4\x9fi\xc5\x9fken r: tam32 = g\xc3\xb6rev_birle\xc5\x9ftir(g);\n");
    test_sonuc("E15 lambda body sabit literal -> 0 hata", h == 0);
}

/* ============================================================
 * SINIR / RESERVED testler
 * ============================================================ */

static void E16_iki_lambda_zincirleme(void) {
    /* Tek var iki kez yakalanir (iki ayrı gorev_baslat) -> capture
     * idempotent (zaten transferred), tek bir erisim DRF007 verir. */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken n: tam32 = 5;\n"
        "    de\xc4\x9fi\xc5\x9fken g1 = g\xc3\xb6rev_ba\xc5\x9flat(|| n);\n"
        "    de\xc4\x9fi\xc5\x9fken g2 = g\xc3\xb6rev_ba\xc5\x9flat(|| n + 1);\n"
        "    de\xc4\x9fi\xc5\x9fken s: tam32 = n;\n");
    test_sonuc("E16 iki capture aynı var + 1 erisim -> >=1 DRF007", h >= 1);
}

static void E17_capture_ifade_bagiminda(void) {
    /* Lambda gövdesinde if-else: iki dalda da n yakalanir */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken n: tam32 = 3;\n"
        "    de\xc4\x9fi\xc5\x9fken g = g\xc3\xb6rev_ba\xc5\x9flat(|| e\xc4\x9f"
            "er n > 0 { n } de\xc4\x9f" "ilse { 0 - n });\n"
        "    de\xc4\x9fi\xc5\x9fken s: tam32 = n;\n");
    test_sonuc("E17 lambda if-else capture + erisim -> DRF007", h >= 1);
}

static void E18_dizi_capture_iki_indeks(void) {
    /* Dizi capture, sonra hem [0] hem [1] erisim -> >=2 DRF007 */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken xs: Dizi<tam32> = [1, 2, 3];\n"
        "    de\xc4\x9fi\xc5\x9fken g = g\xc3\xb6rev_ba\xc5\x9flat(|| xs[0]);\n"
        "    de\xc4\x9fi\xc5\x9fken a: tam32 = xs[1];\n"
        "    de\xc4\x9fi\xc5\x9fken b: tam32 = xs[2];\n");
    test_sonuc("E18 Dizi capture + 2 erisim -> >=2 DRF007", h >= 2);
}

int main(void) {
    printf("\n=== KEMGU Inter-procedural Escape Analizi V1 Test Paketi ===\n");
    printf("(Faz 4: R-YAKALAMA-THREAD compile-time enforcement)\n\n");

    printf("--- NEGATIVE: Free var capture sonrasi erisim (DRF007) ---\n");
    E1_dizi_capture_indeks();
    E2_tam32_capture_aritmetik();
    E3_capture_sonra_atama();
    E4_iki_var_capture();
    E5_capture_indeks_arg();
    E6_capture_sonra_baska_lambda();
    E7_capture_sonra_yeni_atama_okuma();
    E8_capture_birlestir_sonra_var_okuma();
    E9_capture_param_olarak();
    E10_capture_yapi_alan();

    printf("\n--- POSITIVE: Capture YOK veya skip listesinde ---\n");
    E11_lambda_param_okuma();
    E12_lambda_yok_capture();
    E13_baslatmadan_lambda();
    E14_capture_kullanim_yok();
    E15_lambda_ic_var();

    printf("\n--- SINIR: Zincirleme / Kosullu / Coklu erisim ---\n");
    E16_iki_lambda_zincirleme();
    E17_capture_ifade_bagiminda();
    E18_dizi_capture_iki_indeks();

    printf("\n=========================================\n");
    printf("Toplam: %d | Basarili: %d | Basarisiz: %d\n",
           toplam_test, basarili, basarisiz);
    if (basarisiz == 0) {
        printf("=== %d/%d test gecti (basarili) ===\n",
               basarili, toplam_test);
    }
    return basarisiz > 0 ? 1 : 0;
}
