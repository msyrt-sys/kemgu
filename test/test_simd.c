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
 * KEMGU SIMD Spec V1 — Test Paketi
 * ================================
 *
 * Hedef: SD.8 minimum 25 test (spec onayli ön-koşul).
 *
 * Hata kodlari (belgeler/KEMGU_SIMD_Spec_V1.md):
 *   V001 — VEKTOR_ELEMENT_INVALID         (T vektor-yetenekli değil)
 *   V002 — VEKTOR_LANE_INVALID            (N {2,4,8,16,32,64} dışı)
 *   V003 — VEKTOR_ARITMETIK_TIP_UYUMSUZ   (T1!=T2 veya N1!=N2)
 *   V004 — VEKTOR_SKALER_KARMA            (skaler + vektör)
 *   V005 — VEKTOR_FP_MOD                  (kesirli vektörde %)
 *   V006 — VEKTOR_BIT_FP                  (kesirli vektörde &)
 *   V009 — VEKTOR_AZALT_TIP               (vektor_topla operandi)
 *   V010 — VEKTOR_MASK_TIP                (vektor_sec mask)
 *   V020 — VEKTOR_INTRINSIC_ARITY         (intrinsic arg sayisi)
 *
 * Test gruplari:
 *   G1 (1-5):   Tip + lexer (pozitif)
 *   G2 (6-10):  Tip kontrol negatif (V001, V002, V003, V004, V005)
 *   G3 (11-15): Aritmetik (pozitif)
 *   G4 (16-19): Intrinsic (pozitif)
 *   G5 (20-22): Intrinsic negatif (V009, V020)
 *   G6 (23-27): LLVM end-to-end (exec + exit code)
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

/* === Yardimci: kaynak -> hata sayisi === */

static int derle_kontrol(const char *kaynak, int *hata_out) {
    Arena *a = arena_olustur(0);
    Lexer l;
    lexer_baslat(&l, kaynak, "test");
    Parser p;
    parser_baslat(&p, &l, a, "test", kaynak);
    Dugum *prog = parser_calistir(&p);
    if (!prog) {
        if (hata_out) *hata_out = -1;
        arena_serbest(a);
        return -1;
    }
    if (p.hata_sayisi > 0) {
        if (hata_out) *hata_out = p.hata_sayisi + 1000;
        arena_serbest(a);
        return -1;
    }
    Scope *g = scope_olustur(a, SCOPE_GLOBAL, NULL);
    TipKontrol tk;
    tip_kontrol_baslat(&tk, a, g, "test", kaynak);
    tip_kontrol_program(&tk, prog);
    if (hata_out) *hata_out = tk.hata_sayisi;
    arena_serbest(a);
    return 0;
}

static int hata_sayisi(const char *kaynak) {
    int h = -1;
    if (derle_kontrol(kaynak, &h) != 0) return -1;
    return h;
}

/* Test programi sablonu — main + donus tipi */
static int kontrol_main_donus(const char *govde) {
    static char buf[8192];
    snprintf(buf, sizeof(buf),
        "i\xc5\x9flev test() -> tam32 {\n%s\n}\n", govde);
    return hata_sayisi(buf);
}

/* ========================================================================
 * GROUP G1 (1-5): Tip + lexer (pozitif)
 * ======================================================================== */

static void T1_lexer_vektor_keyword(void) {
    /* Lexer "vektör" tokenını tanır mı? */
    Arena *a = arena_olustur(0);
    Lexer l;
    lexer_baslat(&l, "vekt\xc3\xb6r", "test");
    Token t = lexer_sonraki_token(&l);
    test_sonuc("G1.1: lexer 'vektor' anahtar kelime tanir", t.tip == TOK_VEKTOR);
    arena_serbest(a);
}

static void T2_tip_v4i32(void) {
    int h = kontrol_main_donus(
        "    de\xc4\x9fi\xc5\x9fken v: vekt\xc3\xb6r<tam32, 4> = "
        "vektor_doldur(0);\n"
        "    ver 0;");
    test_sonuc("G1.2: vektor<tam32, 4> = 0 hata", h == 0);
}

static void T3_tip_v8i32(void) {
    int h = kontrol_main_donus(
        "    de\xc4\x9fi\xc5\x9fken v: vekt\xc3\xb6r<tam32, 8> = "
        "vektor_doldur(0);\n"
        "    ver 0;");
    test_sonuc("G1.3: vektor<tam32, 8> = 0 hata", h == 0);
}

static void T4_tip_v4f32(void) {
    int h = kontrol_main_donus(
        "    de\xc4\x9fi\xc5\x9fken v: vekt\xc3\xb6r<kesirli32, 4> = "
        "vektor_doldur(0.0);\n"
        "    ver 0;");
    test_sonuc("G1.4: vektor<kesirli32, 4> = 0 hata", h == 0);
}

static void T5_tip_v16dtam8(void) {
    int h = kontrol_main_donus(
        "    de\xc4\x9fi\xc5\x9fken v: vekt\xc3\xb6r<dtam8, 16> = "
        "vektor_doldur(0);\n"
        "    ver 0;");
    test_sonuc("G1.5: vektor<dtam8, 16> = 0 hata", h == 0);
}

/* ========================================================================
 * GROUP G2 (6-10): Tip kontrol negatif
 * ======================================================================== */

static void T6_neg_v001_metin(void) {
    /* V001: metin vektör-yetenekli değil */
    int h = kontrol_main_donus(
        "    de\xc4\x9fi\xc5\x9fken v: vekt\xc3\xb6r<metin, 4> = "
        "vektor_doldur(\"a\");\n"
        "    ver 0;");
    test_sonuc("G2.1: V001 metin element = hata", h > 0);
}

static void T7_neg_v002_lane3(void) {
    /* V002: N=3 geçersiz */
    int h = kontrol_main_donus(
        "    de\xc4\x9fi\xc5\x9fken v: vekt\xc3\xb6r<tam32, 3> = "
        "vektor_doldur(0);\n"
        "    ver 0;");
    test_sonuc("G2.2: V002 N=3 = hata", h > 0);
}

static void T8_neg_v002_lane128(void) {
    /* V002: N=128 yasak (V1) */
    int h = kontrol_main_donus(
        "    de\xc4\x9fi\xc5\x9fken v: vekt\xc3\xb6r<tam32, 128> = "
        "vektor_doldur(0);\n"
        "    ver 0;");
    test_sonuc("G2.3: V002 N=128 = hata", h > 0);
}

static void T9_neg_v003_lane_mismatch(void) {
    /* V003: lane mismatch (4 vs 8) */
    int h = kontrol_main_donus(
        "    de\xc4\x9fi\xc5\x9fken a: vekt\xc3\xb6r<tam32, 4> = "
        "vektor_doldur(0);\n"
        "    de\xc4\x9fi\xc5\x9fken b: vekt\xc3\xb6r<tam32, 8> = "
        "vektor_doldur(0);\n"
        "    de\xc4\x9fi\xc5\x9fken c: vekt\xc3\xb6r<tam32, 4> = a + b;\n"
        "    ver 0;");
    test_sonuc("G2.4: V003 lane mismatch = hata", h > 0);
}

static void T10_neg_v004_skaler(void) {
    /* V004: vektör + skaler yasak */
    int h = kontrol_main_donus(
        "    de\xc4\x9fi\xc5\x9fken a: vekt\xc3\xb6r<tam32, 4> = "
        "vektor_doldur(0);\n"
        "    de\xc4\x9fi\xc5\x9fken c: vekt\xc3\xb6r<tam32, 4> = a + 5;\n"
        "    ver 0;");
    test_sonuc("G2.5: V004 skaler karma = hata", h > 0);
}

/* ========================================================================
 * GROUP G3 (11-15): Aritmetik (pozitif)
 * ======================================================================== */

static void T11_arit_topla(void) {
    int h = kontrol_main_donus(
        "    de\xc4\x9fi\xc5\x9fken a: vekt\xc3\xb6r<tam32, 4> = "
        "vektor_doldur(1);\n"
        "    de\xc4\x9fi\xc5\x9fken b: vekt\xc3\xb6r<tam32, 4> = "
        "vektor_doldur(2);\n"
        "    de\xc4\x9fi\xc5\x9fken c: vekt\xc3\xb6r<tam32, 4> = a + b;\n"
        "    ver 0;");
    test_sonuc("G3.1: v + v = v (tam32, 4)", h == 0);
}

static void T12_arit_carp(void) {
    int h = kontrol_main_donus(
        "    de\xc4\x9fi\xc5\x9fken a: vekt\xc3\xb6r<tam32, 4> = "
        "vektor_doldur(3);\n"
        "    de\xc4\x9fi\xc5\x9fken b: vekt\xc3\xb6r<tam32, 4> = "
        "vektor_doldur(4);\n"
        "    de\xc4\x9fi\xc5\x9fken c: vekt\xc3\xb6r<tam32, 4> = a * b;\n"
        "    ver 0;");
    test_sonuc("G3.2: v * v = v (tam32, 4)", h == 0);
}

static void T13_arit_cikar(void) {
    int h = kontrol_main_donus(
        "    de\xc4\x9fi\xc5\x9fken a: vekt\xc3\xb6r<tam32, 8> = "
        "vektor_doldur(10);\n"
        "    de\xc4\x9fi\xc5\x9fken b: vekt\xc3\xb6r<tam32, 8> = "
        "vektor_doldur(3);\n"
        "    de\xc4\x9fi\xc5\x9fken c: vekt\xc3\xb6r<tam32, 8> = a - b;\n"
        "    ver 0;");
    test_sonuc("G3.3: v - v = v (tam32, 8)", h == 0);
}

static void T14_arit_kesirli(void) {
    int h = kontrol_main_donus(
        "    de\xc4\x9fi\xc5\x9fken a: vekt\xc3\xb6r<kesirli32, 4> = "
        "vektor_doldur(1.0);\n"
        "    de\xc4\x9fi\xc5\x9fken b: vekt\xc3\xb6r<kesirli32, 4> = "
        "vektor_doldur(2.0);\n"
        "    de\xc4\x9fi\xc5\x9fken c: vekt\xc3\xb6r<kesirli32, 4> = a + b;\n"
        "    ver 0;");
    test_sonuc("G3.4: kesirli32 vektor + = (FP)", h == 0);
}

static void T15_arit_neg_v005_mod(void) {
    /* V005: kesirli % yasak.
     * NOT: snprintf %s string'i olduğu gibi geçirir; % karakter olarak
     * tek yazılmalı (printf %% kuralı uygulanmaz). */
    int h = kontrol_main_donus(
        "    de\xc4\x9fi\xc5\x9fken a: vekt\xc3\xb6r<kesirli32, 4> = "
        "vektor_doldur(1.0);\n"
        "    de\xc4\x9fi\xc5\x9fken b: vekt\xc3\xb6r<kesirli32, 4> = "
        "vektor_doldur(2.0);\n"
        "    de\xc4\x9fi\xc5\x9fken c: vekt\xc3\xb6r<kesirli32, 4> = a % b;\n"
        "    ver 0;");
    test_sonuc("G3.5: V005 kesirli mod yasak = hata", h > 0);
}

/* ========================================================================
 * GROUP G4 (16-19): Intrinsic (pozitif)
 * ======================================================================== */

static void T16_intrinsic_topla(void) {
    int h = kontrol_main_donus(
        "    de\xc4\x9fi\xc5\x9fken v: vekt\xc3\xb6r<tam32, 4> = "
        "vektor_doldur(5);\n"
        "    de\xc4\x9fi\xc5\x9fken t: tam32 = vektor_topla(v);\n"
        "    ver t;");
    test_sonuc("G4.1: vektor_topla(v: vektor<tam32, 4>) -> tam32", h == 0);
}

static void T17_intrinsic_esit(void) {
    int h = kontrol_main_donus(
        "    de\xc4\x9fi\xc5\x9fken a: vekt\xc3\xb6r<tam32, 4> = "
        "vektor_doldur(1);\n"
        "    de\xc4\x9fi\xc5\x9fken b: vekt\xc3\xb6r<tam32, 4> = "
        "vektor_doldur(2);\n"
        "    de\xc4\x9fi\xc5\x9fken m: vekt\xc3\xb6r<mant\xc4\xb1ksal, 4> = "
        "vektor_esit(a, b);\n"
        "    ver 0;");
    test_sonuc("G4.2: vektor_esit(a, b) -> vektor<mantiksal, N>", h == 0);
}

static void T18_intrinsic_eleman(void) {
    int h = kontrol_main_donus(
        "    de\xc4\x9fi\xc5\x9fken v: vekt\xc3\xb6r<tam32, 4> = "
        "vektor_doldur(7);\n"
        "    de\xc4\x9fi\xc5\x9fken e: tam32 = vektor_eleman(v, 2);\n"
        "    ver e;");
    test_sonuc("G4.3: vektor_eleman(v, i) -> T", h == 0);
}

static void T19_intrinsic_min_max(void) {
    int h = kontrol_main_donus(
        "    de\xc4\x9fi\xc5\x9fken v: vekt\xc3\xb6r<tam32, 4> = "
        "vektor_doldur(3);\n"
        "    de\xc4\x9fi\xc5\x9fken mn: tam32 = vektor_min(v);\n"
        "    de\xc4\x9fi\xc5\x9fken mx: tam32 = vektor_max(v);\n"
        "    ver mn + mx;");
    test_sonuc("G4.4: vektor_min, vektor_max -> T", h == 0);
}

/* ========================================================================
 * GROUP G5 (20-22): Intrinsic negatif
 * ======================================================================== */

static void T20_neg_v009_topla_skaler(void) {
    /* V009: vektor_topla skaler operand */
    int h = kontrol_main_donus(
        "    de\xc4\x9fi\xc5\x9fken s: tam32 = 5;\n"
        "    de\xc4\x9fi\xc5\x9fken t: tam32 = vektor_topla(s);\n"
        "    ver t;");
    test_sonuc("G5.1: V009 vektor_topla skaler operand = hata", h > 0);
}

static void T21_neg_v020_arity(void) {
    /* V020: vektor_eleman arg sayisi */
    int h = kontrol_main_donus(
        "    de\xc4\x9fi\xc5\x9fken v: vekt\xc3\xb6r<tam32, 4> = "
        "vektor_doldur(1);\n"
        "    de\xc4\x9fi\xc5\x9fken e: tam32 = vektor_eleman(v);\n"
        "    ver e;");
    test_sonuc("G5.2: V020 vektor_eleman 1 arg = hata", h > 0);
}

static void T22_neg_v010_sec_mask(void) {
    /* V010: vektor_sec mask tipi yanlış */
    int h = kontrol_main_donus(
        "    de\xc4\x9fi\xc5\x9fken bad: vekt\xc3\xb6r<tam32, 4> = "
        "vektor_doldur(0);\n"
        "    de\xc4\x9fi\xc5\x9fken a: vekt\xc3\xb6r<tam32, 4> = "
        "vektor_doldur(1);\n"
        "    de\xc4\x9fi\xc5\x9fken b: vekt\xc3\xb6r<tam32, 4> = "
        "vektor_doldur(2);\n"
        "    de\xc4\x9fi\xc5\x9fken c: vekt\xc3\xb6r<tam32, 4> = "
        "vektor_sec(bad, a, b);\n"
        "    ver 0;");
    test_sonuc("G5.3: V010 vektor_sec mask = hata", h > 0);
}

/* ========================================================================
 * GROUP G6 (23-27): LLVM end-to-end (sadece tip-kontrol; LLVM testleri
 * test_simd.sh ile ayrı çalışır)
 * ======================================================================== */

static void T23_e2e_topla(void) {
    int h = kontrol_main_donus(
        "    de\xc4\x9fi\xc5\x9fken a: vekt\xc3\xb6r<tam32, 4> = "
        "vektor_doldur(10);\n"
        "    de\xc4\x9fi\xc5\x9fken b: vekt\xc3\xb6r<tam32, 4> = "
        "vektor_doldur(11);\n"
        "    de\xc4\x9fi\xc5\x9fken c: vekt\xc3\xb6r<tam32, 4> = a + b;\n"
        "    ver vektor_topla(c);");
    test_sonuc("G6.1: end-to-end tip kontrol (4 lane sum)", h == 0);
}

static void T24_e2e_8lane(void) {
    int h = kontrol_main_donus(
        "    de\xc4\x9fi\xc5\x9fken a: vekt\xc3\xb6r<tam32, 8> = "
        "vektor_doldur(5);\n"
        "    de\xc4\x9fi\xc5\x9fken b: vekt\xc3\xb6r<tam32, 8> = "
        "vektor_doldur(3);\n"
        "    de\xc4\x9fi\xc5\x9fken c: vekt\xc3\xb6r<tam32, 8> = a * b;\n"
        "    ver vektor_topla(c);");
    test_sonuc("G6.2: end-to-end (8 lane mul + topla)", h == 0);
}

static void T25_e2e_kesirli(void) {
    int h = kontrol_main_donus(
        "    de\xc4\x9fi\xc5\x9fken a: vekt\xc3\xb6r<kesirli32, 4> = "
        "vektor_doldur(1.0);\n"
        "    de\xc4\x9fi\xc5\x9fken b: vekt\xc3\xb6r<kesirli32, 4> = "
        "vektor_doldur(2.0);\n"
        "    de\xc4\x9fi\xc5\x9fken c: vekt\xc3\xb6r<kesirli32, 4> = a + b;\n"
        "    ver 0;");
    test_sonuc("G6.3: end-to-end kesirli32 vektor", h == 0);
}

static void T26_e2e_mantiksal_vektor(void) {
    int h = kontrol_main_donus(
        "    de\xc4\x9fi\xc5\x9fken a: vekt\xc3\xb6r<tam32, 4> = "
        "vektor_doldur(1);\n"
        "    de\xc4\x9fi\xc5\x9fken b: vekt\xc3\xb6r<tam32, 4> = "
        "vektor_doldur(1);\n"
        "    de\xc4\x9fi\xc5\x9fken m: vekt\xc3\xb6r<mant\xc4\xb1ksal, 4> = "
        "vektor_esit(a, b);\n"
        "    de\xc4\x9fi\xc5\x9fken r: mant\xc4\xb1ksal = vektor_veya_azalt(m);\n"
        "    ver 0;");
    test_sonuc("G6.4: end-to-end mantiksal vektor + reduction", h == 0);
}

static void T27_e2e_v16dtam8_AES(void) {
    /* AES block size = 16 byte */
    int h = kontrol_main_donus(
        "    de\xc4\x9fi\xc5\x9fken blok: vekt\xc3\xb6r<dtam8, 16> = "
        "vektor_doldur(0);\n"
        "    de\xc4\x9fi\xc5\x9fken anahtar: vekt\xc3\xb6r<dtam8, 16> = "
        "vektor_doldur(0);\n"
        "    de\xc4\x9fi\xc5\x9fken xor_sonuc: vekt\xc3\xb6r<dtam8, 16> = "
        "blok ^ anahtar;\n"
        "    ver 0;");
    test_sonuc("G6.5: AES 16-byte block XOR (dtam8 x 16)", h == 0);
}

/* ========================================================================
 * GROUP G7 (28-30): TipBilgisi seviye testleri
 * ======================================================================== */

static void T28_tip_olustur_vektor(void) {
    Arena *a = arena_olustur(0);
    TipBilgisi *e = tip_olustur_basit(a, TIP_TAM32);
    TipBilgisi *v = tip_olustur_vektor(a, e, 4);
    int ok = v && v->kategori == TIP_VEKTOR &&
        v->veri.vektor.lane_sayi == 4 &&
        v->veri.vektor.eleman == e;
    test_sonuc("G7.1: tip_olustur_vektor", ok);
    arena_serbest(a);
}

static void T29_tip_esit_vektor_nominal(void) {
    Arena *a = arena_olustur(0);
    TipBilgisi *e1 = tip_olustur_basit(a, TIP_TAM32);
    TipBilgisi *e2 = tip_olustur_basit(a, TIP_TAM32);
    TipBilgisi *e3 = tip_olustur_basit(a, TIP_TAM64);
    TipBilgisi *v1 = tip_olustur_vektor(a, e1, 4);
    TipBilgisi *v2 = tip_olustur_vektor(a, e2, 4);
    TipBilgisi *v3 = tip_olustur_vektor(a, e3, 4);
    TipBilgisi *v4 = tip_olustur_vektor(a, e1, 8);
    int ok = tip_esit(v1, v2) && !tip_esit(v1, v3) && !tip_esit(v1, v4);
    test_sonuc("G7.2: tip_esit nominal (T, N karşılaştır)", ok);
    arena_serbest(a);
}

static void T30_tip_vektor_lane_gecerli(void) {
    int ok =
        tip_vektor_lane_gecerli_mi(2) &&
        tip_vektor_lane_gecerli_mi(4) &&
        tip_vektor_lane_gecerli_mi(8) &&
        tip_vektor_lane_gecerli_mi(16) &&
        tip_vektor_lane_gecerli_mi(32) &&
        tip_vektor_lane_gecerli_mi(64) &&
        !tip_vektor_lane_gecerli_mi(3) &&
        !tip_vektor_lane_gecerli_mi(7) &&
        !tip_vektor_lane_gecerli_mi(128);
    test_sonuc("G7.3: tip_vektor_lane_gecerli_mi (2-64 izin, diğer reddet)", ok);
}

/* === Ana === */

int main(void) {
    printf("=== KEMGU SIMD Spec V1 Test Paketi ===\n\n");

    printf("--- G1: Tip + lexer (pozitif) ---\n");
    T1_lexer_vektor_keyword();
    T2_tip_v4i32();
    T3_tip_v8i32();
    T4_tip_v4f32();
    T5_tip_v16dtam8();

    printf("\n--- G2: Tip kontrol negatif ---\n");
    T6_neg_v001_metin();
    T7_neg_v002_lane3();
    T8_neg_v002_lane128();
    T9_neg_v003_lane_mismatch();
    T10_neg_v004_skaler();

    printf("\n--- G3: Aritmetik ---\n");
    T11_arit_topla();
    T12_arit_carp();
    T13_arit_cikar();
    T14_arit_kesirli();
    T15_arit_neg_v005_mod();

    printf("\n--- G4: Intrinsic (pozitif) ---\n");
    T16_intrinsic_topla();
    T17_intrinsic_esit();
    T18_intrinsic_eleman();
    T19_intrinsic_min_max();

    printf("\n--- G5: Intrinsic negatif ---\n");
    T20_neg_v009_topla_skaler();
    T21_neg_v020_arity();
    T22_neg_v010_sec_mask();

    printf("\n--- G6: End-to-end tip-kontrol ---\n");
    T23_e2e_topla();
    T24_e2e_8lane();
    T25_e2e_kesirli();
    T26_e2e_mantiksal_vektor();
    T27_e2e_v16dtam8_AES();

    printf("\n--- G7: TipBilgisi seviye ---\n");
    T28_tip_olustur_vektor();
    T29_tip_esit_vektor_nominal();
    T30_tip_vektor_lane_gecerli();

    printf("\n=============================================\n");
    printf("Toplam: %d | Basarili: %d | Basarisiz: %d\n",
           toplam_test, basarili, basarisiz);
    return basarisiz == 0 ? 0 : 1;
}
