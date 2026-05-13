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
 * KEMGU Sabitsure (Constant-Time) Spec V1 — Test Paketi
 * =====================================================
 *
 * Hedef: CT.7 minimum 30 test (spec onayli ön-koşul).
 *
 * Hata kodlari (belgeler/KEMGU_Sabitsure_Spec_V1.md):
 *   CT001 — SABITSURE_IF_BRANCH        (eger/iken/esles kosulu sabitsure)
 *   CT002 — SABITSURE_INDEX            (arr[sabitsure] - cache timing)
 *   CT003 — SABITSURE_LEAK             (implicit downgrade — atama/ver/arg)
 *   CT004 — SABITSURE_DIVMOD           (sabitsure uzerinde / veya %)
 *   CT005 — SABITSURE_PRODUCER_ARITY   (sabitsure_yarat arg sayisi)
 *   CT006 — SABITSURE_WRAP_INVALID     (kesirli/metin/nesting yasak)
 *   CT007 — SABITSURE_DECLASS_ARITY    (ifsa arg sayisi/operand)
 *   CT008 — SABITSURE_SHIFT_AMOUNT     (kaydirma miktari sabitsure)
 *
 * Test gruplari:
 *   S1 (1-4):   Tip + producer (pozitif)
 *   S2 (5-9):   Aritmetik taint yayılımı (pozitif)
 *   S3 (10-13): ifsa declassification (pozitif)
 *   S4 (14-17): CT001 dallanma (negatif)
 *   S5 (18-20): CT002 indeks (negatif)
 *   S6 (21-23): CT003 implicit downgrade (negatif)
 *   S7 (24-26): CT004 div/mod (negatif)
 *   S8 (27-30): CT006 invalid wrap (negatif)
 *   S9 (31-35): CT005/CT007/CT008 ek (negatif)
 *   S10 (36-39): Timing attack senaryolari (CCS/USENIX Security)
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

/* Test programi sablonu — kaynagi main fonksiyonu icine gomar. */
static int kontrol_main(const char *govde) {
    static char buf[8192];
    snprintf(buf, sizeof(buf),
        "i\xc5\x9flev test() {\n%s\n}\n", govde);
    return hata_sayisi(buf);
}

/* Test programi sablonu — main + donus tipi */
static int kontrol_main_donus(const char *govde) {
    static char buf[8192];
    snprintf(buf, sizeof(buf),
        "i\xc5\x9flev test() -> tam32 {\n%s\n}\n", govde);
    return hata_sayisi(buf);
}

/* ========================================================================
 * GROUP S1 (1-4): Tip + producer (pozitif)
 * ======================================================================== */

static void T1_tip_temel(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k: sabits\xc3\xbc" "re<tam32> = "
        "sabits\xc3\xbc" "re_yarat(5);");
    test_sonuc("S1: sabitsure<tam32> + producer = 0 hata", h == 0);
}

static void T2_tip_tamsayi_cesitleri(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken a: sabits\xc3\xbc" "re<tam8> = "
        "sabits\xc3\xbc" "re_yarat(1);\n"
        "    de\xc4\x9fi\xc5\x9fken b: sabits\xc3\xbc" "re<dtam16> = "
        "sabits\xc3\xbc" "re_yarat(2);\n"
        "    de\xc4\x9fi\xc5\x9fken c: sabits\xc3\xbc" "re<tam64> = "
        "sabits\xc3\xbc" "re_yarat(3);");
    test_sonuc("S1: tam8/dtam16/tam64 sarmalamalari", h == 0);
}

static void T3_tip_mantiksal(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k: sabits\xc3\xbc" "re<mant\xc4\xb1ksal>"
        " = sabits\xc3\xbc" "re_yarat(do\xc4\x9fru);");
    test_sonuc("S1: sabitsure<mantiksal> = 0 hata", h == 0);
}

static void T4_tip_karakter(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k: sabits\xc3\xbc" "re<karakter>"
        " = sabits\xc3\xbc" "re_yarat('a');");
    test_sonuc("S1: sabitsure<karakter> = 0 hata", h == 0);
}

/* ========================================================================
 * GROUP S2 (5-9): Aritmetik taint yayılımı (pozitif)
 * ======================================================================== */

static void T5_aritmetik_xor(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k: sabits\xc3\xbc" "re<dtam32> = "
        "sabits\xc3\xbc" "re_yarat(0);\n"
        "    de\xc4\x9fi\xc5\x9fken m: sabits\xc3\xbc" "re<dtam32> = "
        "sabits\xc3\xbc" "re_yarat(1);\n"
        "    de\xc4\x9fi\xc5\x9fken r: sabits\xc3\xbc" "re<dtam32> = k ^ m;");
    test_sonuc("S2: sabitsure XOR sabitsure -> sabitsure", h == 0);
}

static void T6_aritmetik_toplama(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k: sabits\xc3\xbc" "re<tam32> = "
        "sabits\xc3\xbc" "re_yarat(10);\n"
        "    de\xc4\x9fi\xc5\x9fken r: sabits\xc3\xbc" "re<tam32> = k + 5;");
    test_sonuc("S2: sabitsure + public -> sabitsure (taint yayilir)", h == 0);
}

static void T7_aritmetik_bit_op(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k: sabits\xc3\xbc" "re<dtam8> = "
        "sabits\xc3\xbc" "re_yarat(0);\n"
        "    de\xc4\x9fi\xc5\x9fken r1: sabits\xc3\xbc" "re<dtam8> = k & 15;\n"
        "    de\xc4\x9fi\xc5\x9fken r2: sabits\xc3\xbc" "re<dtam8> = k | 16;");
    test_sonuc("S2: sabitsure & | bit op -> sabitsure", h == 0);
}

static void T8_kaydirma_sabit_miktar(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k: sabits\xc3\xbc" "re<dtam32> = "
        "sabits\xc3\xbc" "re_yarat(1);\n"
        "    de\xc4\x9fi\xc5\x9fken r: sabits\xc3\xbc" "re<dtam32> = k << 3;");
    test_sonuc("S2: sabitsure << sabit_miktar = OK", h == 0);
}

static void T9_karsilastirma_taint(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken a: sabits\xc3\xbc" "re<tam32> = "
        "sabits\xc3\xbc" "re_yarat(1);\n"
        "    de\xc4\x9fi\xc5\x9fken b: sabits\xc3\xbc" "re<tam32> = "
        "sabits\xc3\xbc" "re_yarat(2);\n"
        "    de\xc4\x9fi\xc5\x9fken e\xc5\x9fit: sabits\xc3\xbc" "re<mant\xc4\xb1ksal>"
        " = a == b;");
    test_sonuc("S2: sabitsure == sabitsure -> sabitsure<mantiksal>", h == 0);
}

/* ========================================================================
 * GROUP S3 (10-13): ifsa declassification (pozitif)
 * ======================================================================== */

static void T10_ifsa_temel(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k: sabits\xc3\xbc" "re<tam32> = "
        "sabits\xc3\xbc" "re_yarat(42);\n"
        "    de\xc4\x9fi\xc5\x9fken n: tam32 = if\xc5\x9f" "a(k);");
    test_sonuc("S3: ifsa(sabitsure<T>) -> T = 0 hata", h == 0);
}

static void T11_ifsa_aritmetik_sonra(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k: sabits\xc3\xbc" "re<tam32> = "
        "sabits\xc3\xbc" "re_yarat(10);\n"
        "    de\xc4\x9fi\xc5\x9fken n: tam32 = if\xc5\x9f" "a(k + 5);");
    test_sonuc("S3: ifsa(sabitsure + public) -> T", h == 0);
}

static void T12_ifsa_branching_acik(void) {
    /* ifsa sonrasi normal mantiksal -> dallanma izinli (programci sorumlu) */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k: sabits\xc3\xbc" "re<mant\xc4\xb1ksal>"
        " = sabits\xc3\xbc" "re_yarat(do\xc4\x9fru);\n"
        "    e\xc4\x9f" "er if\xc5\x9f" "a(k) { }");
    test_sonuc("S3: ifsa sonrasi eger izinli (explicit leak)", h == 0);
}

static void T13_ifsa_non_sabitsure_hata(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken x: tam32 = 5;\n"
        "    de\xc4\x9fi\xc5\x9fken n: tam32 = if\xc5\x9f" "a(x);");
    test_sonuc("S3: ifsa(non-sabitsure) -> CT007", h >= 1);
}

/* ========================================================================
 * GROUP S4 (14-17): CT001 dallanma (negatif)
 * ======================================================================== */

static void T14_if_sabitsure_kosul(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k: sabits\xc3\xbc" "re<mant\xc4\xb1ksal>"
        " = sabits\xc3\xbc" "re_yarat(do\xc4\x9fru);\n"
        "    e\xc4\x9f" "er k { }");
    test_sonuc("S4: eger sabitsure_kosul -> CT001", h >= 1);
}

static void T15_iken_sabitsure_kosul(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k: sabits\xc3\xbc" "re<mant\xc4\xb1ksal>"
        " = sabits\xc3\xbc" "re_yarat(yanl\xc4\xb1\xc5\x9f);\n"
        "    iken k { }");
    test_sonuc("S4: iken sabitsure_kosul -> CT001", h >= 1);
}

static void T16_if_else_dallanma(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken a: sabits\xc3\xbc" "re<tam32> = "
        "sabits\xc3\xbc" "re_yarat(1);\n"
        "    de\xc4\x9fi\xc5\x9fken b: sabits\xc3\xbc" "re<tam32> = "
        "sabits\xc3\xbc" "re_yarat(2);\n"
        "    de\xc4\x9fi\xc5\x9fken e: sabits\xc3\xbc" "re<mant\xc4\xb1ksal>"
        " = a == b;\n"
        "    e\xc4\x9f" "er e { } de\xc4\x9f" "ilse { }");
    test_sonuc("S4: eger/degilse sabitsure -> CT001", h >= 1);
}

static void T17_karsilastirma_kosul(void) {
    /* Sabitsüre == public da sonuc sabitsure<mantiksal> verir -> CT001 */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k: sabits\xc3\xbc" "re<tam32> = "
        "sabits\xc3\xbc" "re_yarat(42);\n"
        "    e\xc4\x9f" "er k == 42 { }");
    test_sonuc("S4: karsilastirma sonrasi eger -> CT001", h >= 1);
}

/* ========================================================================
 * GROUP S5 (18-20): CT002 indeks (negatif)
 * ======================================================================== */

static void T18_array_index_sabitsure(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken arr: Dizi<tam32> = [10, 20, 30];\n"
        "    de\xc4\x9fi\xc5\x9fken i: sabits\xc3\xbc" "re<tam32> = "
        "sabits\xc3\xbc" "re_yarat(1);\n"
        "    de\xc4\x9fi\xc5\x9fken v: tam32 = arr[i];");
    test_sonuc("S5: arr[sabitsure_idx] -> CT002", h >= 1);
}

static void T19_index_ifsa_ile(void) {
    /* ifsa sonrasi index izinli (programci sorumlu) */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken arr: Dizi<tam32> = [10, 20, 30];\n"
        "    de\xc4\x9fi\xc5\x9fken i: sabits\xc3\xbc" "re<tam32> = "
        "sabits\xc3\xbc" "re_yarat(1);\n"
        "    de\xc4\x9fi\xc5\x9fken v: tam32 = arr[if\xc5\x9f" "a(i)];");
    test_sonuc("S5: arr[ifsa(sabitsure)] = 0 hata (explicit)", h == 0);
}

static void T20_index_aritmetik_kayar(void) {
    /* idx = i + 1 nerede i sabitsure -> idx da sabitsure */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken arr: Dizi<tam32> = [10, 20, 30];\n"
        "    de\xc4\x9fi\xc5\x9fken i: sabits\xc3\xbc" "re<tam32> = "
        "sabits\xc3\xbc" "re_yarat(1);\n"
        "    de\xc4\x9fi\xc5\x9fken v: tam32 = arr[i + 1];");
    test_sonuc("S5: arr[sabitsure + 1] -> CT002 (taint yayilim)", h >= 1);
}

/* ========================================================================
 * GROUP S6 (21-23): CT003 implicit downgrade (negatif)
 * ======================================================================== */

static void T21_atama_leak(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k: sabits\xc3\xbc" "re<tam32> = "
        "sabits\xc3\xbc" "re_yarat(5);\n"
        "    de\xc4\x9fi\xc5\x9fken n: tam32 = k;");
    test_sonuc("S6: sabitsure -> normal atama -> CT003", h >= 1);
}

static void T22_ver_leak(void) {
    int h = kontrol_main_donus(
        "    de\xc4\x9fi\xc5\x9fken k: sabits\xc3\xbc" "re<tam32> = "
        "sabits\xc3\xbc" "re_yarat(5);\n"
        "    ver k;");
    test_sonuc("S6: ver sabitsure (donus tam32) -> CT003", h >= 1);
}

static void T23_cagri_arg_leak(void) {
    /* yazdir(metin) — sabitsure<metin> zaten yasak (CT006), ama tam32
     * arg geçirme alanında — yapay islev tanimlayalim. */
    int h = hata_sayisi(
        "i\xc5\x9flev al(x: tam32) -> tam32 { ver x; }\n"
        "i\xc5\x9flev test() {\n"
        "    de\xc4\x9fi\xc5\x9fken k: sabits\xc3\xbc" "re<tam32> = "
        "sabits\xc3\xbc" "re_yarat(7);\n"
        "    de\xc4\x9fi\xc5\x9fken n: tam32 = al(k);\n"
        "}");
    test_sonuc("S6: cagri(sabitsure) public param -> CT003", h >= 1);
}

/* ========================================================================
 * GROUP S7 (24-26): CT004 div/mod (negatif)
 * ======================================================================== */

static void T24_bolme_sabitsure(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken a: sabits\xc3\xbc" "re<tam32> = "
        "sabits\xc3\xbc" "re_yarat(10);\n"
        "    de\xc4\x9fi\xc5\x9fken b: sabits\xc3\xbc" "re<tam32> = "
        "sabits\xc3\xbc" "re_yarat(3);\n"
        "    de\xc4\x9fi\xc5\x9fken r: sabits\xc3\xbc" "re<tam32> = a / b;");
    test_sonuc("S7: sabitsure / sabitsure -> CT004", h >= 1);
}

static void T25_mod_sabitsure(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken a: sabits\xc3\xbc" "re<tam32> = "
        "sabits\xc3\xbc" "re_yarat(10);\n"
        "    de\xc4\x9fi\xc5\x9fken r: sabits\xc3\xbc" "re<tam32> = a % 7;");
    test_sonuc("S7: sabitsure % sabit -> CT004", h >= 1);
}

static void T26_bolme_public_sag(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken a: sabits\xc3\xbc" "re<tam32> = "
        "sabits\xc3\xbc" "re_yarat(10);\n"
        "    de\xc4\x9fi\xc5\x9fken r: sabits\xc3\xbc" "re<tam32> = a / 2;");
    test_sonuc("S7: sabitsure / public_sabit -> CT004", h >= 1);
}

/* ========================================================================
 * GROUP S8 (27-30): CT006 invalid wrap (negatif)
 * ======================================================================== */

static void T27_wrap_kesirli_yasak(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k: sabits\xc3\xbc" "re<kesirli32>"
        " = sabits\xc3\xbc" "re_yarat(3.14);");
    test_sonuc("S8: sabitsure<kesirli32> -> CT006 (FP non-CT)", h >= 1);
}

static void T28_wrap_kesirli64_yasak(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k: sabits\xc3\xbc" "re<kesirli64>"
        " = sabits\xc3\xbc" "re_yarat(2.5);");
    test_sonuc("S8: sabitsure<kesirli64> -> CT006", h >= 1);
}

static void T29_wrap_metin_yasak(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k: sabits\xc3\xbc" "re<metin>"
        " = sabits\xc3\xbc" "re_yarat(\"gizli\");");
    test_sonuc("S8: sabitsure<metin> -> CT006 (UTF-8 varies)", h >= 1);
}

static void T30_nesting_yasak(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k: sabits\xc3\xbc" "re<sabits\xc3\xbc"
        "re<tam32>> = sabits\xc3\xbc" "re_yarat(5);");
    test_sonuc("S8: sabitsure<sabitsure<T>> -> CT006 (nesting)", h >= 1);
}

/* ========================================================================
 * GROUP S9 (31-35): CT005/CT007/CT008 ek
 * ======================================================================== */

static void T31_producer_zero_arg(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k = sabits\xc3\xbc" "re_yarat();");
    test_sonuc("S9: sabitsure_yarat() (0 arg) -> CT005", h >= 1);
}

static void T32_producer_two_arg(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k = sabits\xc3\xbc" "re_yarat(1, 2);");
    test_sonuc("S9: sabitsure_yarat(1,2) -> CT005", h >= 1);
}

static void T33_ifsa_zero_arg(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken n = if\xc5\x9f" "a();");
    test_sonuc("S9: ifsa() (0 arg) -> CT007", h >= 1);
}

static void T34_shift_sabitsure_miktar(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken x: dtam32 = 1;\n"
        "    de\xc4\x9fi\xc5\x9fken s: sabits\xc3\xbc" "re<dtam32> = "
        "sabits\xc3\xbc" "re_yarat(3);\n"
        "    de\xc4\x9fi\xc5\x9fken r: dtam32 = x << s;");
    test_sonuc("S9: public << sabitsure -> CT008 (variable-shift)", h >= 1);
}

static void T35_aritmetik_tip_uyumsuz(void) {
    /* sabitsure<tam32> + sabitsure<tam64> — iç tipler farkli -> T001 */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken a: sabits\xc3\xbc" "re<tam32> = "
        "sabits\xc3\xbc" "re_yarat(1);\n"
        "    de\xc4\x9fi\xc5\x9fken b: sabits\xc3\xbc" "re<tam64> = "
        "sabits\xc3\xbc" "re_yarat(2);\n"
        "    de\xc4\x9fi\xc5\x9fken c = a + b;");
    test_sonuc("S9: sabitsure<tam32> + sabitsure<tam64> -> T001", h >= 1);
}

/* ========================================================================
 * GROUP S10 (36-39): Timing attack senaryolari (CCS/USENIX Security)
 * ======================================================================== */

/* Kocher 1996: RSA modular üs alma — bit'e bağlı dallanma. */
static void T36_rsa_kocher1996_branch(void) {
    int h = kontrol_main(
        "    /* Kocher 1996: RSA private key bit'e bagli kontrol akisi */\n"
        "    de\xc4\x9fi\xc5\x9fken d_bit: sabits\xc3\xbc" "re<dtam32>"
        " = sabits\xc3\xbc" "re_yarat(1);\n"
        "    de\xc4\x9fi\xc5\x9fken sifir: sabits\xc3\xbc" "re<dtam32>"
        " = sabits\xc3\xbc" "re_yarat(0);\n"
        "    e\xc4\x9f" "er d_bit != sifir { }");
    test_sonuc("S10: Kocher96 RSA bit-branch -> CT001", h >= 1);
}

/* Bernstein 2005: AES T-table cache-timing — s_box[plaintext XOR key]. */
static void T37_aes_bernstein2005_index(void) {
    int h = kontrol_main(
        "    /* Bernstein 2005: AES T-table[ p ^ k ] cache leak */\n"
        "    de\xc4\x9fi\xc5\x9fken sbox: Dizi<dtam8> = [0, 1, 2, 3];\n"
        "    de\xc4\x9fi\xc5\x9fken plaintext: dtam8 = 65;\n"
        "    de\xc4\x9fi\xc5\x9fken key: sabits\xc3\xbc" "re<dtam8>"
        " = sabits\xc3\xbc" "re_yarat(42);\n"
        "    de\xc4\x9fi\xc5\x9fken idx: sabits\xc3\xbc" "re<dtam8>"
        " = key ^ plaintext;\n"
        "    de\xc4\x9fi\xc5\x9fken s: dtam8 = sbox[idx];");
    test_sonuc("S10: Bernstein05 AES T-table -> CT002 (cache-timing)", h >= 1);
}

/* OpenSSL CVE-style OTP key compare — early-exit memcmp. */
static void T38_otp_compare_branch(void) {
    int h = kontrol_main(
        "    /* OTP/HMAC key compare — early-exit timing */\n"
        "    de\xc4\x9fi\xc5\x9fken k1: sabits\xc3\xbc" "re<dtam32>"
        " = sabits\xc3\xbc" "re_yarat(0);\n"
        "    de\xc4\x9fi\xc5\x9fken k2: sabits\xc3\xbc" "re<dtam32>"
        " = sabits\xc3\xbc" "re_yarat(0);\n"
        "    de\xc4\x9fi\xc5\x9fken e\xc5\x9fit: sabits\xc3\xbc"
        "re<mant\xc4\xb1ksal> = k1 == k2;\n"
        "    e\xc4\x9f" "er e\xc5\x9fit { }");
    test_sonuc("S10: OTP key compare -> CT001", h >= 1);
}

/* Brumley & Tuveri 2011: ECDSA `k` nonce bit-branching */
static void T39_ecdsa_brumley_tuveri_bit(void) {
    int h = kontrol_main(
        "    /* ECDSA k scalar bit branching — Brumley/Tuveri 2011 */\n"
        "    de\xc4\x9fi\xc5\x9fken k: sabits\xc3\xbc" "re<dtam64>"
        " = sabits\xc3\xbc" "re_yarat(0);\n"
        "    de\xc4\x9fi\xc5\x9fken bit: sabits\xc3\xbc" "re<dtam64>"
        " = k & 1;\n"
        "    de\xc4\x9fi\xc5\x9fken zero: sabits\xc3\xbc" "re<dtam64>"
        " = sabits\xc3\xbc" "re_yarat(0);\n"
        "    iken bit != zero { }");
    test_sonuc("S10: ECDSA k bit-branch -> CT001 (Brumley/Tuveri 2011)", h >= 1);
}

/* ========================================================================
 * Ana
 * ======================================================================== */

int main(void) {
    /* stderr'i sustur — hata mesajlari testlerde gurultu yapmasin */
    FILE *eski_stderr = freopen("/dev/null", "w", stderr);
    if (!eski_stderr) {
        /* Windows: try NUL */
        eski_stderr = freopen("NUL", "w", stderr);
    }

    puts("=== KEMGU Sabitsure (Constant-Time) Spec V1 — Test Paketi ===\n");

    puts("--- S1: Tip + producer (4) ---");
    T1_tip_temel(); T2_tip_tamsayi_cesitleri();
    T3_tip_mantiksal(); T4_tip_karakter();

    puts("\n--- S2: Aritmetik taint yayilim (5) ---");
    T5_aritmetik_xor(); T6_aritmetik_toplama(); T7_aritmetik_bit_op();
    T8_kaydirma_sabit_miktar(); T9_karsilastirma_taint();

    puts("\n--- S3: ifsa declassification (4) ---");
    T10_ifsa_temel(); T11_ifsa_aritmetik_sonra();
    T12_ifsa_branching_acik(); T13_ifsa_non_sabitsure_hata();

    puts("\n--- S4: CT001 dallanma (4) ---");
    T14_if_sabitsure_kosul(); T15_iken_sabitsure_kosul();
    T16_if_else_dallanma(); T17_karsilastirma_kosul();

    puts("\n--- S5: CT002 indeks (3) ---");
    T18_array_index_sabitsure(); T19_index_ifsa_ile();
    T20_index_aritmetik_kayar();

    puts("\n--- S6: CT003 implicit downgrade (3) ---");
    T21_atama_leak(); T22_ver_leak(); T23_cagri_arg_leak();

    puts("\n--- S7: CT004 div/mod (3) ---");
    T24_bolme_sabitsure(); T25_mod_sabitsure(); T26_bolme_public_sag();

    puts("\n--- S8: CT006 invalid wrap (4) ---");
    T27_wrap_kesirli_yasak(); T28_wrap_kesirli64_yasak();
    T29_wrap_metin_yasak(); T30_nesting_yasak();

    puts("\n--- S9: CT005/CT007/CT008 ek (5) ---");
    T31_producer_zero_arg(); T32_producer_two_arg();
    T33_ifsa_zero_arg(); T34_shift_sabitsure_miktar();
    T35_aritmetik_tip_uyumsuz();

    puts("\n--- S10: Timing attack senaryolari (CCS/USENIX) (4) ---");
    T36_rsa_kocher1996_branch();
    T37_aes_bernstein2005_index();
    T38_otp_compare_branch();
    T39_ecdsa_brumley_tuveri_bit();

    printf("\n========================================\n");
    printf("Toplam: %d | Basarili: %d | Basarisiz: %d\n",
           toplam_test, basarili, basarisiz);

    return basarisiz == 0 ? 0 : 1;
}
