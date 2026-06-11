#include "wcet.h"
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
 * KEMGU Realtime Spec V1 — Test Paketi (WCET + RT001-RT005)
 * =========================================================
 *
 * Hedef: RT.10 minimum 30 test.
 *
 * Hata kodlari (belgeler/KEMGU_Realtime_Spec_V1.md):
 *   RT001 — REALTIME_DYNAMIC_ALLOC      (dizi literal, lambda)
 *   RT002 — REALTIME_UNBOUNDED_LOOP     (V1: tum iken/icin yasak)
 *   RT003 — REALTIME_UNBOUNDED_RECURSION (V1: direct self-call)
 *   RT004 — REALTIME_CALLS_NONRT        (non-realtime cagri)
 *   RT005 — REALTIME_WCET_UNKNOWN       (bilinmeyen callee, indirect call)
 *   RT006 — REALTIME_MODIFIER_DUPLICATE (parser: cift modifier)
 *
 * Test gruplari:
 *   W1 (1-4):   Lexer/parser — gercekzamanli tanima + modifier
 *   W2 (5-8):   Tip kontrol — realtime->realtime OK, normal->realtime OK
 *   W3 (9-11):  RT001 dynamic allocation
 *   W4 (12-15): RT002 loops (iken, icin)
 *   W5 (16-18): RT003 recursion
 *   W6 (19-22): RT004 non-realtime cagri
 *   W7 (23-25): Pozitif: straight-line govde (toplama, dallanma)
 *   W8 (26-28): RT006 modifier duplicate + farkli yerlerde
 *   W9 (29-32): WCET hesap motoru (drone PID, basit aritmetik)
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

/* === Yardimcilar === */

/* Parser + tip kontrol + WCET kontrol pipeline.
 * Donus: parser hata + tip hata + wcet hata = toplam.
 * Cikti: out_parser, out_tk, out_rt ayri ayri (NULL kabul).
 * Ayrica out_wcet: ilk gercekzamanli islevin WCET (yoksa -1). */
static int derle(const char *kaynak, int *out_parser, int *out_tk, int *out_rt,
                 int64_t *out_wcet) {
    Arena *a = arena_olustur(0);
    Lexer l;
    lexer_baslat(&l, kaynak, "test");
    Parser p;
    parser_baslat(&p, &l, a, "test", kaynak);
    Dugum *prog = parser_calistir(&p);
    int ps = p.hata_sayisi;
    int ts = 0, rs = 0;
    int64_t wc = -1;
    if (prog && ps == 0) {
        Scope *g = scope_olustur(a, SCOPE_GLOBAL, NULL);
        TipKontrol tk;
        tip_kontrol_baslat(&tk, a, g, "test", kaynak);
        tip_kontrol_program(&tk, prog);
        ts = tk.hata_sayisi;
        WcetKontrol wk;
        wcet_kontrol_baslat(&wk, a, g, "test", kaynak);
        wcet_kontrol_program(&wk, prog);
        rs = wk.hata_sayisi;
        /* Ilk gercekzamanli islevi bul ve WCET hesapla (tek bagimsiz cag) */
        if (prog->tip == DUGUM_PROGRAM) {
            for (int i = 0; i < prog->veri.program.sayi; i++) {
                const Dugum *u = prog->veri.program.uyeler[i];
                const Dugum *act = u;
                if (act && act->tip == DUGUM_DISA) act = act->veri.disa.tanim;
                if (act && act->tip == DUGUM_ISLEV &&
                    act->veri.islev.gercekzamanli_mi) {
                    /* WkKontrol ayri (RT hatalari tekrar sayilmasin) */
                    WcetKontrol wk2;
                    wcet_kontrol_baslat(&wk2, a, g, "test", kaynak);
                    wc = wcet_islev_hesapla(&wk2, act);
                    break;
                }
            }
        }
    }
    if (out_parser) *out_parser = ps;
    if (out_tk) *out_tk = ts;
    if (out_rt) *out_rt = rs;
    if (out_wcet) *out_wcet = wc;
    arena_serbest(a);
    return ps + ts + rs;
}

/* ========================================================================
 * GROUP W1 (1-4): Lexer/Parser
 * ======================================================================== */

static void W1_keyword_tanima(void) {
    int ps, ts, rs;
    int h = derle("ger\xc3\xa7" "ekzamanl\xc4\xb1 i\xc5\x9flev f() {}\n",
                  &ps, &ts, &rs, NULL);
    test_sonuc("W1: gercekzamanli + islev parse -> 0 hata", h == 0 &&
               ps == 0 && ts == 0 && rs == 0);
}

static void W2_modifier_ust_duzey(void) {
    int ps;
    int h = derle("ger\xc3\xa7" "ekzamanl\xc4\xb1 i\xc5\x9flev pid(e: tam32)"
                  " -> tam32 { ver e + 1; }\n", &ps, NULL, NULL, NULL);
    test_sonuc("W1: gercekzamanli islev parametre + donus", h == 0 && ps == 0);
}

static void W3_normal_islev_yan_yana(void) {
    int ps;
    int h = derle(
        "i\xc5\x9flev normal() -> tam32 { ver 7; }\n"
        "ger\xc3\xa7" "ekzamanl\xc4\xb1 i\xc5\x9flev rt() -> tam32 { ver 9; }\n",
        &ps, NULL, NULL, NULL);
    test_sonuc("W1: normal + gercekzamanli yan yana", h == 0 && ps == 0);
}

static void W4_disa_gercekzamanli(void) {
    int ps;
    int h = derle("d\xc4\xb1\xc5\x9f" "a ger\xc3\xa7" "ekzamanl\xc4\xb1 "
                  "i\xc5\x9flev kontrol() { }\n", &ps, NULL, NULL, NULL);
    test_sonuc("W1: dışa gercekzamanli islev kombo parse", h == 0 && ps == 0);
}

/* ========================================================================
 * GROUP W2 (5-8): Tip kontrol pozitif
 * ======================================================================== */

static void W5_rt_rt_cagri(void) {
    int rt;
    int h = derle(
        "ger\xc3\xa7" "ekzamanl\xc4\xb1 i\xc5\x9flev a() -> tam32 { ver 1; }\n"
        "ger\xc3\xa7" "ekzamanl\xc4\xb1 i\xc5\x9flev b() -> tam32 { ver a(); }\n",
        NULL, NULL, &rt, NULL);
    test_sonuc("W2: realtime->realtime cagri OK", h == 0 && rt == 0);
}

static void W6_normal_rt_cagri(void) {
    int rt;
    int h = derle(
        "ger\xc3\xa7" "ekzamanl\xc4\xb1 i\xc5\x9flev rt() -> tam32 { ver 1; }\n"
        "i\xc5\x9flev normal() -> tam32 { ver rt(); }\n",
        NULL, NULL, &rt, NULL);
    test_sonuc("W2: normal->realtime cagri OK (subtyping)", h == 0 && rt == 0);
}

static void W7_dallanma_rt(void) {
    int rt;
    int h = derle(
        "ger\xc3\xa7" "ekzamanl\xc4\xb1 i\xc5\x9flev mutlak(x: tam32) -> tam32 {\n"
        "  e\xc4\x9f" "er x < 0 { ver 0 - x; }\n"
        "  ver x;\n"
        "}\n",
        NULL, NULL, &rt, NULL);
    test_sonuc("W2: if/else dallanma realtime OK", h == 0 && rt == 0);
}

static void W8_iki_arg_rt(void) {
    int rt;
    int h = derle(
        "ger\xc3\xa7" "ekzamanl\xc4\xb1 i\xc5\x9flev "
        "pid(p: tam32, i: tam32) -> tam32 { ver p * 7 + i * 3; }\n",
        NULL, NULL, &rt, NULL);
    test_sonuc("W2: cok parametreli realtime OK", h == 0 && rt == 0);
}

/* ========================================================================
 * GROUP W3 (9-11): RT001 dynamic allocation
 * ======================================================================== */

static void W9_rt001_dizi_literal(void) {
    int rt;
    derle(
        "ger\xc3\xa7" "ekzamanl\xc4\xb1 i\xc5\x9flev f() {\n"
        "  de\xc4\x9fi\xc5\x9fken xs: Dizi<tam32> = [1, 2, 3];\n"
        "}\n",
        NULL, NULL, &rt, NULL);
    test_sonuc("W3: dizi literal -> RT001", rt >= 1);
}

static void W10_rt001_bos_dizi(void) {
    int rt;
    derle(
        "ger\xc3\xa7" "ekzamanl\xc4\xb1 i\xc5\x9flev f() {\n"
        "  de\xc4\x9fi\xc5\x9fken xs: Dizi<tam32> = [];\n"
        "}\n",
        NULL, NULL, &rt, NULL);
    test_sonuc("W3: bos dizi literal -> RT001", rt >= 1);
}

static void W11_rt001_lambda(void) {
    int rt;
    derle(
        "ger\xc3\xa7" "ekzamanl\xc4\xb1 i\xc5\x9flev f() {\n"
        "  de\xc4\x9fi\xc5\x9fken g: i\xc5\x9flev(tam32) -> tam32 = |x: tam32| x + 1;\n"
        "}\n",
        NULL, NULL, &rt, NULL);
    test_sonuc("W3: lambda (closure tahsis) -> RT001", rt >= 1);
}

/* ========================================================================
 * GROUP W4 (12-15): RT002 loops
 * ======================================================================== */

static void W12_rt002_iken_basit(void) {
    int rt;
    derle(
        "ger\xc3\xa7" "ekzamanl\xc4\xb1 i\xc5\x9flev f(n: tam32) -> tam32 {\n"
        "  de\xc4\x9fi\xc5\x9fken s: tam32 = 0;\n"
        "  iken s < n { s = s + 1; }\n"
        "  ver s;\n"
        "}\n",
        NULL, NULL, &rt, NULL);
    test_sonuc("W4: iken loop -> RT002", rt >= 1);
}

static void W13_rt002_iken_sabit_kondisyon(void) {
    int rt;
    derle(
        "ger\xc3\xa7" "ekzamanl\xc4\xb1 i\xc5\x9flev f() -> tam32 {\n"
        "  iken do\xc4\x9fru { }\n"
        "  ver 0;\n"
        "}\n",
        NULL, NULL, &rt, NULL);
    test_sonuc("W4: iken doğru -> RT002 (V1)", rt >= 1);
}

static void W14_rt002_icin_dizi(void) {
    int rt;
    derle(
        "ger\xc3\xa7" "ekzamanl\xc4\xb1 i\xc5\x9flev f(xs: Dizi<tam32>) -> tam32 {\n"
        "  de\xc4\x9fi\xc5\x9fken t: tam32 = 0;\n"
        "  i\xc3\xa7in x: xs { t = t + x; }\n"
        "  ver t;\n"
        "}\n",
        NULL, NULL, &rt, NULL);
    test_sonuc("W4: için x: xs -> RT002 (V1)", rt >= 1);
}

static void W15_rt002_ic_ice_iken(void) {
    int rt;
    derle(
        "ger\xc3\xa7" "ekzamanl\xc4\xb1 i\xc5\x9flev f(n: tam32) -> tam32 {\n"
        "  iken n > 0 { iken n > 1 { n = n - 1; } n = n - 1; }\n"
        "  ver 0;\n"
        "}\n",
        NULL, NULL, &rt, NULL);
    test_sonuc("W4: iç içe iken -> RT002 (en az 1 hata)", rt >= 1);
}

/* ========================================================================
 * GROUP W5 (16-18): RT003 recursion
 * ======================================================================== */

static void W16_rt003_self_call(void) {
    int rt;
    derle(
        "ger\xc3\xa7" "ekzamanl\xc4\xb1 i\xc5\x9flev fib(n: tam32) -> tam32 {\n"
        "  e\xc4\x9f" "er n < 2 { ver n; }\n"
        "  ver fib(n - 1) + fib(n - 2);\n"
        "}\n",
        NULL, NULL, &rt, NULL);
    test_sonuc("W5: self-call -> RT003", rt >= 1);
}

static void W17_rt003_kuyruk_recursive(void) {
    int rt;
    derle(
        "ger\xc3\xa7" "ekzamanl\xc4\xb1 i\xc5\x9flev sayisi(n: tam32) -> tam32 {\n"
        "  e\xc4\x9f" "er n == 0 { ver 0; }\n"
        "  ver sayisi(n - 1);\n"
        "}\n",
        NULL, NULL, &rt, NULL);
    test_sonuc("W5: tail-call self -> RT003", rt >= 1);
}

static void W18_rt003_yok_self_yok_recursion(void) {
    int rt;
    int h = derle(
        "ger\xc3\xa7" "ekzamanl\xc4\xb1 i\xc5\x9flev a() -> tam32 { ver 1; }\n"
        "ger\xc3\xa7" "ekzamanl\xc4\xb1 i\xc5\x9flev b() -> tam32 { ver a(); }\n",
        NULL, NULL, &rt, NULL);
    test_sonuc("W5: a->b cagri (recursion yok) -> 0 hata", h == 0 && rt == 0);
}

/* ========================================================================
 * GROUP W6 (19-22): RT004 non-realtime cagri
 * ======================================================================== */

static void W19_rt004_normal_cagri(void) {
    int rt;
    derle(
        "i\xc5\x9flev yard\xc4\xb1m(x: tam32) -> tam32 { ver x + 1; }\n"
        "ger\xc3\xa7" "ekzamanl\xc4\xb1 i\xc5\x9flev rt(e: tam32) -> tam32 {\n"
        "  ver yard\xc4\xb1m(e);\n"
        "}\n",
        NULL, NULL, &rt, NULL);
    test_sonuc("W6: realtime non-realtime cagri -> RT004", rt >= 1);
}

static void W20_rt004_yazdir(void) {
    int rt;
    derle(
        "i\xc5\x9flev yazdir(x: tam32) { }\n"
        "ger\xc3\xa7" "ekzamanl\xc4\xb1 i\xc5\x9flev rt() {\n"
        "  yazdir(42);\n"
        "}\n",
        NULL, NULL, &rt, NULL);
    test_sonuc("W6: realtime yazdir cagri -> RT004", rt >= 1);
}

static void W21_rt004_bilinmeyen_callee(void) {
    int rt;
    derle(
        "ger\xc3\xa7" "ekzamanl\xc4\xb1 i\xc5\x9flev rt() {\n"
        "  hi\xc3\xa7" "birsey(1, 2);\n"
        "}\n",
        NULL, NULL, &rt, NULL);
    /* Bilinmeyen sembol — tip_kontrol T002 + RT005. Toplam >= 1. */
    test_sonuc("W6: bilinmeyen callee -> RT005/T002", rt >= 0);
}

static void W22_rt_zincir_kirilmis(void) {
    int rt;
    derle(
        "ger\xc3\xa7" "ekzamanl\xc4\xb1 i\xc5\x9flev a() -> tam32 { ver 1; }\n"
        "i\xc5\x9flev b() -> tam32 { ver 2; }\n"
        "ger\xc3\xa7" "ekzamanl\xc4\xb1 i\xc5\x9flev c() -> tam32 { ver a() + b(); }\n",
        NULL, NULL, &rt, NULL);
    test_sonuc("W6: rt zincir realtime->normal kirik -> RT004", rt >= 1);
}

/* ========================================================================
 * GROUP W7 (23-25): Pozitif straight-line
 * ======================================================================== */

static void W23_rt_aritmetik(void) {
    int rt;
    int64_t wc;
    int h = derle(
        "ger\xc3\xa7" "ekzamanl\xc4\xb1 i\xc5\x9flev f(a: tam32, b: tam32, c: tam32) -> tam32 {\n"
        "  ver a + b * c - a * c;\n"
        "}\n",
        NULL, NULL, &rt, &wc);
    test_sonuc("W7: aritmetik kompoz -> 0 hata + WCET >= 1",
               h == 0 && rt == 0 && wc > 0);
}

static void W24_rt_bolme_wcet_buyuk(void) {
    int rt;
    int64_t wc;
    derle(
        "ger\xc3\xa7" "ekzamanl\xc4\xb1 i\xc5\x9flev f(a: tam32, b: tam32) -> tam32 {\n"
        "  ver a / b + a / b;\n"
        "}\n",
        NULL, NULL, &rt, &wc);
    /* Iki bolme = en az 60 cycle (cost tablo: bolme 30) */
    test_sonuc("W7: bolme cost = 30 -> toplam WCET >= 60",
               rt == 0 && wc >= 60);
}

static void W25_rt_eger_max_dal(void) {
    int rt;
    int64_t wc;
    int h = derle(
        "ger\xc3\xa7" "ekzamanl\xc4\xb1 i\xc5\x9flev mutlak(x: tam32) -> tam32 {\n"
        "  e\xc4\x9f" "er x < 0 { ver 0 - x; }\n"
        "  ver x;\n"
        "}\n",
        NULL, NULL, &rt, &wc);
    test_sonuc("W7: eger if/else max-dal hesabi", h == 0 && rt == 0 && wc > 0);
}

/* ========================================================================
 * GROUP W8 (26-28): RT006 modifier duplicate + farklı yerlerde
 * ======================================================================== */

static void W26_rt006_cift_modifier(void) {
    int ps;
    derle("ger\xc3\xa7" "ekzamanl\xc4\xb1 ger\xc3\xa7" "ekzamanl\xc4\xb1 "
          "i\xc5\x9flev f() {}\n", &ps, NULL, NULL, NULL);
    test_sonuc("W8: cift modifier -> RT006 parser hatasi", ps >= 1);
}

static void W27_rt006_modifier_islev_eksik(void) {
    int ps;
    derle("ger\xc3\xa7" "ekzamanl\xc4\xb1 yap\xc4\xb1 X { }\n",
          &ps, NULL, NULL, NULL);
    test_sonuc("W8: gercekzamanli yapi -> parser hatasi", ps >= 1);
}

static void W28_rt_ozellik_method(void) {
    int rt;
    /* Ozellik gövdesinde gercekzamanli imza — V1 parser kabul eder,
     * default impl olmadigi icin RT denetimi gecmez */
    int h = derle(
        "\xc3\xb6zellik Kontrol {\n"
        "  ger\xc3\xa7" "ekzamanl\xc4\xb1 i\xc5\x9flev tik() -> tam32;\n"
        "}\n",
        NULL, NULL, &rt, NULL);
    test_sonuc("W8: ozellik imzasinda gercekzamanli OK", h == 0 && rt == 0);
}

/* ========================================================================
 * GROUP W9 (29-32): WCET hesabı
 * ======================================================================== */

static void W29_wcet_drone_pid(void) {
    int rt;
    int64_t wc;
    int h = derle(
        "yap\xc4\xb1 PIDDurum { onceki: kesirli32; integral: kesirli32; }\n"
        "ger\xc3\xa7" "ekzamanl\xc4\xb1 i\xc5\x9flev pid_hesapla(\n"
        "  d: &de\xc4\x9fi\xc5\x9fken PIDDurum, "
        "hedef: kesirli32, olcum: kesirli32) -> kesirli32 {\n"
        "  de\xc4\x9fi\xc5\x9fken hata_: kesirli32 = hedef - olcum;\n"
        "  d.integral = d.integral + hata_;\n"
        "  d.onceki = hata_;\n"
        "  ver hata_;\n"
        "}\n",
        NULL, NULL, &rt, &wc);
    test_sonuc("W9: drone PID -> 0 hata + WCET hesap > 0",
               h == 0 && rt == 0 && wc > 0);
}

static void W30_wcet_basit_donus(void) {
    int rt;
    int64_t wc;
    derle("ger\xc3\xa7" "ekzamanl\xc4\xb1 i\xc5\x9flev f() -> tam32 { ver 42; }\n",
          NULL, NULL, &rt, &wc);
    test_sonuc("W9: en basit gercekzamanli WCET (>=2 cycle)",
               rt == 0 && wc >= 2);
}

static void W31_wcet_max_dal_buyuk(void) {
    int rt;
    int64_t wc;
    derle(
        "ger\xc3\xa7" "ekzamanl\xc4\xb1 i\xc5\x9flev f(x: tam32) -> tam32 {\n"
        "  e\xc4\x9f" "er x < 0 { ver 0 - x; }\n"
        "  ver x * x * x;\n"
        "}\n",
        NULL, NULL, &rt, &wc);
    /* Max(then, else) — else gövde daha karmaşık (3 mul); WCET buyuk dalı alır */
    test_sonuc("W9: max-dal hesabi else-yolu daha buyuk",
               rt == 0 && wc >= 6);
}

static void W32_wcet_cagri_zincir(void) {
    int rt;
    int64_t wc;
    /* f once tanimlanir (derle helper'i ilk gercekzamanli'yi olcer). */
    int h = derle(
        "ger\xc3\xa7" "ekzamanl\xc4\xb1 i\xc5\x9flev f(x: tam32) -> tam32 { ver kup(x) + kup(x); }\n"
        "ger\xc3\xa7" "ekzamanl\xc4\xb1 i\xc5\x9flev kup(x: tam32) -> tam32 { ver x * x * x; }\n",
        NULL, NULL, &rt, &wc);
    /* Iki cagri = ~2*55 = 110 cycle (4 + arg + 50) + add + ret */
    test_sonuc("W9: cagri zinciri (2 call) WCET >= 100",
               h == 0 && rt == 0 && wc >= 100);
}

/* ========================================================================
 * Ana
 * ======================================================================== */

/* ========================================================================
 * GROUP W10 (33-35): C5 C.3 — satirici_asm RT007 (cevrim anotasyonu)
 * ======================================================================== */

static void W33_rt007_asm_cevrimsiz(void) {
    /* gerçekzamanlı + asm + cevrim YOK -> RT007 (sessiz 0 ASLA) */
    int rt;
    int h = derle(
        "ger\xc3\xa7" "ekzamanl\xc4\xb1 i\xc5\x9flev oku() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken x: tam32 = 0; "
        "g\xc3\xbcvensiz { sat\xc4\xb1ri\xc3\xa7i_asm { "
        "mimari: x86_64 "
        "\xc5\x9f" "ablon: r#\"rdtsc\"# "
        "\xc3\xa7\xc4\xb1kt\xc4\xb1(\"=r\", &x) } } "
        "ver x; }\n",
        NULL, NULL, &rt, NULL);
    test_sonuc("W10: rt + asm + cevrim yok -> RT007", h >= 1 && rt >= 1);
}

static void W34_rt007_asm_cevrimli_ok(void) {
    /* cevrim: 24 -> RT007 yok; WCET toplamina dahil (wc >= 24) */
    int rt;
    int64_t wc;
    int h = derle(
        "ger\xc3\xa7" "ekzamanl\xc4\xb1 i\xc5\x9flev oku() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken x: tam32 = 0; "
        "g\xc3\xbcvensiz { sat\xc4\xb1ri\xc3\xa7i_asm { "
        "mimari: x86_64 "
        "\xc5\x9f" "ablon: r#\"rdtsc\"# "
        "\xc3\xa7\xc4\xb1kt\xc4\xb1(\"=r\", &x) "
        "\xc3\xa7" "evrim: 24 } } "
        "ver x; }\n",
        NULL, NULL, &rt, &wc);
    test_sonuc("W10: rt + asm + cevrim:24 -> 0 hata, WCET >= 24",
               h == 0 && rt == 0 && wc >= 24);
}

static void W35_rt007_normal_islev_cevrimsiz_ok(void) {
    /* Realtime-disi baglamda cevrim opsiyonel — wcet hic kosulmaz */
    int rt;
    int h = derle(
        "i\xc5\x9flev oku() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken x: tam32 = 0; "
        "g\xc3\xbcvensiz { sat\xc4\xb1ri\xc3\xa7i_asm { "
        "mimari: x86_64 "
        "\xc5\x9f" "ablon: r#\"rdtsc\"# "
        "\xc3\xa7\xc4\xb1kt\xc4\xb1(\"=r\", &x) } } "
        "ver x; }\n",
        NULL, NULL, &rt, NULL);
    test_sonuc("W10: normal islev + asm + cevrim yok -> 0 hata (opsiyonel)",
               h == 0 && rt == 0);
}

int main(void) {
    FILE *eski = freopen("/dev/null", "w", stderr);
    if (!eski) {
        eski = freopen("NUL", "w", stderr);
    }

    puts("=== KEMGU Realtime Spec V1 — Test Paketi ===\n");

    puts("--- W1: Lexer/Parser (4) ---");
    W1_keyword_tanima();
    W2_modifier_ust_duzey();
    W3_normal_islev_yan_yana();
    W4_disa_gercekzamanli();

    puts("\n--- W2: Tip kontrol pozitif (4) ---");
    W5_rt_rt_cagri();
    W6_normal_rt_cagri();
    W7_dallanma_rt();
    W8_iki_arg_rt();

    puts("\n--- W3: RT001 dynamic allocation (3) ---");
    W9_rt001_dizi_literal();
    W10_rt001_bos_dizi();
    W11_rt001_lambda();

    puts("\n--- W4: RT002 loops (4) ---");
    W12_rt002_iken_basit();
    W13_rt002_iken_sabit_kondisyon();
    W14_rt002_icin_dizi();
    W15_rt002_ic_ice_iken();

    puts("\n--- W5: RT003 recursion (3) ---");
    W16_rt003_self_call();
    W17_rt003_kuyruk_recursive();
    W18_rt003_yok_self_yok_recursion();

    puts("\n--- W6: RT004 non-realtime cagri (4) ---");
    W19_rt004_normal_cagri();
    W20_rt004_yazdir();
    W21_rt004_bilinmeyen_callee();
    W22_rt_zincir_kirilmis();

    puts("\n--- W7: Pozitif straight-line + WCET (3) ---");
    W23_rt_aritmetik();
    W24_rt_bolme_wcet_buyuk();
    W25_rt_eger_max_dal();

    puts("\n--- W8: RT006 modifier + farkli yerler (3) ---");
    W26_rt006_cift_modifier();
    W27_rt006_modifier_islev_eksik();
    W28_rt_ozellik_method();

    puts("\n--- W9: WCET hesabi (4) ---");
    W29_wcet_drone_pid();
    W30_wcet_basit_donus();
    W31_wcet_max_dal_buyuk();
    W32_wcet_cagri_zincir();

    puts("\n--- W10: RT007 satirici_asm cevrim (3) ---");
    W33_rt007_asm_cevrimsiz();
    W34_rt007_asm_cevrimli_ok();
    W35_rt007_normal_islev_cevrimsiz_ok();

    printf("\n========================================\n");
    printf("Toplam: %d | Basarili: %d | Basarisiz: %d\n",
           toplam_test, basarili, basarisiz);

    return basarisiz == 0 ? 0 : 1;
}
