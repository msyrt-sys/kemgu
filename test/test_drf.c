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
 * KEMGU DRF (Data Race Freedom) — Test Paketi V1
 * ===============================================
 *
 * Plan Karar J: minimum 30+ test (~20 negative + ~10 positive).
 * Concurrency lang syntax: gorev<T>, kanal<T>, gorev_baslat, gorev_birlestir,
 *                          kanal_gonder, kanal_al, dondur.
 *
 * Hata kodlari:
 *   DRF001 — gorev_baslat arity / closure tipi
 *   DRF002 — gorev_birlestir arity / tip
 *   DRF003 — kanal_gonder arity / tip
 *   DRF004 — kanal_al arity / tip
 *   DRF005 — dondur arity / tip
 *
 * Linear mirası (Plan Karar C):
 *   gorev<T> ve kanal<T> linear olarak takip edilir (DRF-L2/L5/L6).
 *   L001 (tuketilmedi), L002 (cift tuketim) bu tipler icin de aktif.
 *
 * Test cercevesi test_linear.c'den alindi (idiomatic).
 * stderr nul'a yonlendirilmedi — manuel kontrol icin.
 */

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

static int kontrol_main(const char *govde) {
    static char buf[4096];
    snprintf(buf, sizeof(buf),
        "i\xc5\x9flev test() {\n%s\n}\n", govde);
    return hata_sayisi(buf);
}

/* ========================================================================
 * GROUP D1-D6: Tip ifadesi (gorev<T>, kanal<T>) — pozitif lexer/parser
 * ======================================================================== */

static void T1_gorev_tam32(void) {
    int h = hata_sayisi(
        "i\xc5\x9flev test(g: g\xc3\xb6rev<tam32>) -> tam32 {\n"
        "    ver g\xc3\xb6rev_birle\xc5\x9ftir(g);\n"
        "}\n");
    test_sonuc("D1: gorev<tam32> parametre + birlestir = 0 hata", h == 0);
}

static void T2_gorev_metin(void) {
    int h = hata_sayisi(
        "i\xc5\x9flev test(g: g\xc3\xb6rev<metin>) -> metin {\n"
        "    ver g\xc3\xb6rev_birle\xc5\x9ftir(g);\n"
        "}\n");
    test_sonuc("D2: gorev<metin> parametre + birlestir = 0 hata", h == 0);
}

static void T3_kanal_tam32(void) {
    int h = hata_sayisi(
        "i\xc5\x9flev test(k: kanal<tam32>) -> tam32 {\n"
        "    ver kanal_al(k);\n"
        "}\n");
    test_sonuc("D3: kanal<tam32> parametre + kanal_al = 0 hata", h == 0);
}

static void T4_kanal_metin(void) {
    /* D-295: kanal tamponu int64_t'ye genisletildi (gorev ile simetrik) ->
     * isaretci T ARTIK CALISIR. (D-292'de gecici olarak DRF006 bekleniyordu;
     * o kisit `--check`i kapatiyordu ama `--llvm` tip kontrolu calistirmadigi
     * icin sessiz kirpmaya aciti — kisiti gurultulu yapmak yerine SINIFI YOK
     * ETTIK.) Uctan uca kanit test_llvm.c'de. */
    int h = hata_sayisi(
        "i\xc5\x9flev test(k: kanal<metin>) -> metin {\n"
        "    ver kanal_al(k);\n"
        "}\n");
    test_sonuc("D4: kanal<metin> = 0 hata (i64 tasima, D-295)", h == 0);
}

static void T5_gorev_nested_kanal(void) {
    /* gorev<kanal<tam32>> — generic icinde generic */
    int h = hata_sayisi(
        "i\xc5\x9flev test(g: g\xc3\xb6rev<kanal<tam32>>) -> kanal<tam32> {\n"
        "    ver g\xc3\xb6rev_birle\xc5\x9ftir(g);\n"
        "}\n");
    test_sonuc("D5: gorev<kanal<tam32>> nested = 0 hata", h == 0);
}

static void T6_kanal_nested_dizi(void) {
    /* D-295: D4 ile ayni gerekce — Dizi<T> runtime'da ptr, i64 tampona sigar. */
    int h = hata_sayisi(
        "i\xc5\x9flev test(k: kanal<Dizi<tam32>>) -> Dizi<tam32> {\n"
        "    ver kanal_al(k);\n"
        "}\n");
    test_sonuc("D6: kanal<Dizi<tam32>> = 0 hata (i64 tasima, D-295)", h == 0);
}

/* ========================================================================
 * GROUP D7-D12: gorev_baslat — pozitif + arity
 * ======================================================================== */

static void T7_gorev_baslat_lambda(void) {
    /* Bir lambda'yi gorev_baslat ile thread'e gecir
     * V1: lambda body ifade-form (block-form V2'ye saklı) */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken g: g\xc3\xb6rev<tam32>"
        " = g\xc3\xb6rev_ba\xc5\x9flat(|| 42);\n"
        "    de\xc4\x9fi\xc5\x9fken r = g\xc3\xb6rev_birle\xc5\x9ftir(g);\n");
    test_sonuc("D7: gorev_baslat(lambda) + birlestir = 0 hata", h == 0);
}

static void T8_gorev_baslat_lambda_metin(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken g: g\xc3\xb6rev<metin>"
        " = g\xc3\xb6rev_ba\xc5\x9flat(|| \"selam\");\n"
        "    de\xc4\x9fi\xc5\x9fken r = g\xc3\xb6rev_birle\xc5\x9ftir(g);\n");
    test_sonuc("D8: gorev_baslat(metin lambda) = 0 hata", h == 0);
}

static void T9_gorev_baslat_sifir_arg(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken g = g\xc3\xb6rev_ba\xc5\x9flat();\n");
    test_sonuc("D9: gorev_baslat() 0 arg -> DRF001", h >= 1);
}

static void T10_gorev_baslat_iki_arg(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken g = g\xc3\xb6rev_ba\xc5\x9flat(1, 2);\n");
    test_sonuc("D10: gorev_baslat(1, 2) 2 arg -> DRF001", h >= 1);
}

static void T11_gorev_baslat_non_islev(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken g = g\xc3\xb6rev_ba\xc5\x9flat(42);\n");
    test_sonuc("D11: gorev_baslat(tam32) -> DRF001 (islev olmali)", h >= 1);
}

static void T12_gorev_baslat_donus_cikarsama(void) {
    /* Annotsuz değişken — gorev<T> çıkarsanmalı */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken g = g\xc3\xb6rev_ba\xc5\x9flat(|| 7);\n"
        "    de\xc4\x9fi\xc5\x9fken r = g\xc3\xb6rev_birle\xc5\x9ftir(g);\n");
    test_sonuc("D12: annotsuz değişken gorev<tam32> çıkarsanır", h == 0);
}

/* ========================================================================
 * GROUP D13-D18: gorev_birlestir — pozitif + arity + linear
 * ======================================================================== */

static void T13_birlestir_basit(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken g = g\xc3\xb6rev_ba\xc5\x9flat(|| 5);\n"
        "    de\xc4\x9fi\xc5\x9fken r: tam32 = g\xc3\xb6rev_birle\xc5\x9ftir(g);\n");
    test_sonuc("D13: birlestir(g) -> tam32 = 0 hata", h == 0);
}

static void T14_birlestir_iki_kez(void) {
    /* g iki kez birlestir -> L002 (linear cift tuketim) */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken g = g\xc3\xb6rev_ba\xc5\x9flat(|| 5);\n"
        "    de\xc4\x9fi\xc5\x9fken r1 = g\xc3\xb6rev_birle\xc5\x9ftir(g);\n"
        "    de\xc4\x9fi\xc5\x9fken r2 = g\xc3\xb6rev_birle\xc5\x9ftir(g);\n");
    test_sonuc("D14: birlestir(g) iki kez -> L002 cift tuketim", h >= 1);
}

static void T15_birlestir_arity_sifir(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken r = g\xc3\xb6rev_birle\xc5\x9ftir();\n");
    test_sonuc("D15: birlestir() 0 arg -> DRF002", h >= 1);
}

static void T16_birlestir_arity_iki(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken r = g\xc3\xb6rev_birle\xc5\x9ftir(1, 2);\n");
    test_sonuc("D16: birlestir(1, 2) -> DRF002", h >= 1);
}

static void T17_birlestir_non_gorev(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken x: tam32 = 5;\n"
        "    de\xc4\x9fi\xc5\x9fken r = g\xc3\xb6rev_birle\xc5\x9ftir(x);\n");
    test_sonuc("D17: birlestir(non-gorev) -> DRF002", h >= 1);
}

static void T18_gorev_tuketilmedi(void) {
    /* L001: gorev<T> linear, scope sonunda tuketilmedi */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken g = g\xc3\xb6rev_ba\xc5\x9flat(|| 1);\n");
    test_sonuc("D18: gorev tuketilmedi -> L001 (linear leak)", h >= 1);
}

/* ========================================================================
 * GROUP D19-D24: kanal_gonder + kanal_al — pozitif + arity
 * ======================================================================== */

static void T19_kanal_gonder_arity(void) {
    int h = hata_sayisi(
        "i\xc5\x9flev test(k: kanal<tam32>) {\n"
        "    kanal_g\xc3\xb6nder(k);\n"
        "}\n");
    test_sonuc("D19: kanal_gonder(k) 1 arg -> DRF003", h >= 1);
}

static void T20_kanal_gonder_non_kanal(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken x: tam32 = 5;\n"
        "    kanal_g\xc3\xb6nder(x, 42);\n");
    test_sonuc("D20: kanal_gonder(non-kanal, v) -> DRF003", h >= 1);
}

static void T21_kanal_al_arity(void) {
    int h = hata_sayisi(
        "i\xc5\x9flev test(k: kanal<tam32>) -> tam32 {\n"
        "    ver kanal_al();\n"
        "}\n");
    test_sonuc("D21: kanal_al() 0 arg -> DRF004", h >= 1);
}

static void T22_kanal_al_non_kanal(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken x: tam32 = 5;\n"
        "    de\xc4\x9fi\xc5\x9fken r = kanal_al(x);\n");
    test_sonuc("D22: kanal_al(non-kanal) -> DRF004", h >= 1);
}

static void T23_kanal_round_trip(void) {
    int h = hata_sayisi(
        "i\xc5\x9flev kanal_kullan(k: kanal<tam32>) -> tam32 {\n"
        "    kanal_g\xc3\xb6nder(k, 42);\n"
        "    ver kanal_al(k);\n"
        "}\n");
    test_sonuc("D23: kanal_gonder + kanal_al round-trip = 0 hata", h == 0);
}

static void T24_kanal_gonder_tip_uyumsuz(void) {
    /* kanal<tam32>'a metin gönderilemez */
    int h = hata_sayisi(
        "i\xc5\x9flev test(k: kanal<tam32>) {\n"
        "    kanal_g\xc3\xb6nder(k, \"yanli\xc5\x9f\");\n"
        "}\n");
    test_sonuc("D24: kanal_gonder(kanal<tam32>, metin) -> DRF003 tip uyumsuz",
               h >= 1);
}

/* ========================================================================
 * GROUP D25-D28: dondur (R-PAYLAŞ frozen region)
 * ======================================================================== */

static void T25_dondur_basit(void) {
    int h = hata_sayisi(
        "i\xc5\x9flev test(v: &de\xc4\x9fi\xc5\x9fken tam32) -> &tam32 {\n"
        "    ver dondur(v);\n"
        "}\n");
    test_sonuc("D25: dondur(&değişken T) -> &T = 0 hata", h == 0);
}

static void T26_dondur_arity_sifir(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken r = dondur();\n");
    test_sonuc("D26: dondur() 0 arg -> DRF005", h >= 1);
}

static void T27_dondur_non_ref(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken x: tam32 = 5;\n"
        "    de\xc4\x9fi\xc5\x9fken r = dondur(x);\n");
    test_sonuc("D27: dondur(non-ref) -> DRF005", h >= 1);
}

static void T28_dondur_immutable_ref(void) {
    /* &T (immutable) zaten — dondur reddetmeli */
    int h = hata_sayisi(
        "i\xc5\x9flev test(v: &tam32) -> &tam32 {\n"
        "    ver dondur(v);\n"
        "}\n");
    test_sonuc("D28: dondur(&T immutable) -> DRF005 (&değişken bekleniyor)",
               h >= 1);
}

/* ========================================================================
 * GROUP D29-D32: Linear miras (gorev/kanal linear)
 * ======================================================================== */

static void T29_gorev_tip_lineer(void) {
    /* Cift tüketim — TypeKontrol L002 */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken g = g\xc3\xb6rev_ba\xc5\x9flat(|| 1);\n"
        "    de\xc4\x9fi\xc5\x9fken g2 = g;\n"
        "    de\xc4\x9fi\xc5\x9fken r = g\xc3\xb6rev_birle\xc5\x9ftir(g);\n");
    test_sonuc("D29: gorev<T> move sonrasi erisim -> L002", h >= 1);
}

static void T30_gorev_referans_alma(void) {
    /* &g linear referans yasagi -> L004 */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken g = g\xc3\xb6rev_ba\xc5\x9flat(|| 1);\n"
        "    de\xc4\x9fi\xc5\x9fken r = &g;\n"
        "    de\xc4\x9fi\xc5\x9fken s = g\xc3\xb6rev_birle\xc5\x9ftir(g);\n");
    test_sonuc("D30: &gorev -> L004 (linear referans yasagi)", h >= 1);
}

static void T31_kanal_non_linear(void) {
    /* V1 karari: kanal<T> non-linear (transfer tamponu, yeniden kullanilir).
     * Bu DRF-L5 ile uyumlu — kanal endpoint kalıcı, sadece v transfer edilir.
     * Referans alma izinli, iki kez al() izinli. */
    int h = hata_sayisi(
        "i\xc5\x9flev test(k: kanal<tam32>) -> tam32 {\n"
        "    de\xc4\x9fi\xc5\x9fken v1 = kanal_al(k);\n"
        "    de\xc4\x9fi\xc5\x9fken v2 = kanal_al(k);\n"
        "    ver v1 + v2;\n"
        "}\n");
    test_sonuc("D31: kanal<T> non-linear (iki kez kanal_al OK) = 0 hata",
               h == 0);
}

static void T32_gorev_yapi_icinde(void) {
    /* gorev<T> yapı içine konulamaz (LR002) — yapı linear olmali */
    int h = hata_sayisi(
        "yap\xc4\xb1 S { g: g\xc3\xb6rev<tam32>; }\n");
    test_sonuc("D32: gorev<T> normal yapida -> LR002", h >= 1);
}

/* ========================================================================
 * GROUP D33-D36: Integration + DRF teoremine bagli compositional pattern
 * ======================================================================== */

static void T33_gorev_kanal_birlikte(void) {
    int h = hata_sayisi(
        "i\xc5\x9flev hesapla(k: kanal<tam32>) -> tam32 {\n"
        "    ver kanal_al(k) + 1;\n"
        "}\n"
        "i\xc5\x9flev test() -> tam32 {\n"
        "    de\xc4\x9fi\xc5\x9fken g: g\xc3\xb6rev<tam32>"
        " = g\xc3\xb6rev_ba\xc5\x9flat(|| 0);\n"
        "    de\xc4\x9fi\xc5\x9fken r = g\xc3\xb6rev_birle\xc5\x9ftir(g);\n"
        "    ver r;\n"
        "}\n");
    test_sonuc("D33: gorev + kanal kompozisyon (DRF-L1-L5) = 0 hata", h == 0);
}

static void T34_tekkez_gorev_lc2(void) {
    /* LC-2: tekkez yakaladigi closure otomatik tekkez<islev>
     * V1 sınır: lineer yakalama lambda body'sinde tuketmek block-form gerek;
     * burada outer-scope tüketim ile L001 kontrolünü test ediyoruz. */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k = tekkez_olustur(42);\n"
        "    de\xc4\x9fi\xc5\x9fken n = kullan(k);\n"
        "    de\xc4\x9fi\xc5\x9fken g = g\xc3\xb6rev_ba\xc5\x9flat(|| n);\n"
        "    de\xc4\x9fi\xc5\x9fken r = g\xc3\xb6rev_birle\xc5\x9ftir(g);\n");
    test_sonuc("D34: gorev_baslat(non-linear) + ayri tekkez tuketim = 0 hata",
               h == 0);
}

static void T35_yetki_gorev_drf_l6(void) {
    /* DRF-L6: yetki<R> linear miras alır.
     * V1 sınır: closure body'sinde linear tüketim block-form gerek.
     * Burada outer-scope tüketim ile DRF-L6 linear-pattern test edilir. */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Dosya> = yetki_olustur(1, 1);\n"
        "    de\xc4\x9fi\xc5\x9fken g = g\xc3\xb6rev_ba\xc5\x9flat(|| 0);\n"
        "    de\xc4\x9fi\xc5\x9fken r = g\xc3\xb6rev_birle\xc5\x9ftir(g);\n"
        "    geri_al(y);\n");
    test_sonuc("D35: yetki<R> + gorev outer-scope tuketim (DRF-L6) = 0 hata",
               h == 0);
}

static void T36_kanal_lineer_v_tuketildi(void) {
    /* DRF-L5: kanal_gonder(k, v) — v lineer ise tuketilir
     * Daha sonra v'ye erisim -> L002 */
    int h = hata_sayisi(
        "i\xc5\x9flev test(k: kanal<tekkez<tam32>>) {\n"
        "    de\xc4\x9fi\xc5\x9fken v = tekkez_olustur(7);\n"
        "    kanal_g\xc3\xb6nder(k, v);\n"
        "    imha(v);\n"
        "}\n");
    test_sonuc("D36: kanal_gonder(k, lineer v) sonra v erisim -> L002",
               h >= 1);
}

/* ========================================================================
 * GROUP D37-D39: Patch P2 — non-linear capture + dondur idempotency
 * ======================================================================== */

static void T37_dizi_yakala_pozitif(void) {
    /* DRF-L3 (Linear Closure Soundness) — non-linear capture variantı:
     * Dizi<tam32> görev_başlat closure'da yakalanır, görev_birleştir ile
     * sonuç alınır, çağıran thread Dizi'ye dokunmaz → 0 hata.
     * R-YAKALAMA-THREAD non-linear capture move semantiğini doğrular. */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken xs: Dizi<tam32> = [1, 2, 3];\n"
        "    de\xc4\x9fi\xc5\x9fken g = g\xc3\xb6rev_ba\xc5\x9flat(|| xs[0]);\n"
        "    de\xc4\x9fi\xc5\x9fken r = g\xc3\xb6rev_birle\xc5\x9ftir(g);\n");
    test_sonuc("D37: Dizi non-linear capture + birlestir = 0 hata",
               h == 0);
}

static void T38_dizi_yakala_sonrasi_erisim_v1_limit(void) {
    /* D38: V1 KNOWN-LIMIT — non-linear capture sonrası çağıran thread'in
     * yakalanan Dizi'ye erişimi V1 tip kontrolünde yakalanmıyor.
     * V2 hedefi (Plan §9 risk tablosu: inter-procedural escape analizi):
     * R-YAKALAMA-THREAD'in compile-time enforcement'i Dizi/yapı için
     * bölge sahipliği transferi sonrası ihlal hata vermeli.
     *
     * V1 davranışı: 0 hata (V1 tip kontrol bu pattern'i yakalamıyor).
     * Bu test V1 sınırını dokümante eder; V2'de h >= 1 olmalıdır. */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken xs: Dizi<tam32> = [1, 2, 3];\n"
        "    de\xc4\x9fi\xc5\x9fken g = g\xc3\xb6rev_ba\xc5\x9flat(|| xs[0]);\n"
        "    de\xc4\x9fi\xc5\x9fken r = g\xc3\xb6rev_birle\xc5\x9ftir(g);\n"
        "    de\xc4\x9fi\xc5\x9fken s: tam32 = xs[1];\n");
    test_sonuc("D38: Dizi capture sonrasi erisim V1 KNOWN-LIMIT "
               "(V1: 0 hata, V2: hata bekleniyor)",
               h == 0);
}

static void T39_dondur_idempotent_degil(void) {
    /* DRF-L4 (Frozen Region) — dondur(v) idempotent DEĞİL.
     *
     * Spec netleştirme (2026-05-15, Patch P2):
     *   dondur(&değişken T) -> &T
     *   İkinci dondur çağrısı &T üzerinde DRF005 verir
     *   (dondur argümanı &değişken T olmalı; &T immutable, kabul edilmez).
     *
     * Bu, dondurmanın "tek yönlü" olduğunu zorlar — bir kez immutable
     * yapıldıktan sonra tekrar dondurma anlamsız ve tip hatası. */
    int h = hata_sayisi(
        "i\xc5\x9flev test(v: &de\xc4\x9fi\xc5\x9fken tam32) -> &tam32 {\n"
        "    de\xc4\x9fi\xc5\x9fken d1: &tam32 = dondur(v);\n"
        "    de\xc4\x9fi\xc5\x9fken d2: &tam32 = dondur(d1);\n"
        "    ver d2;\n"
        "}\n");
    test_sonuc("D39: dondur(dondur(v)) -> DRF005 (idempotent degil)",
               h >= 1);
}

/* ========================================================================
 * Main
 * ======================================================================== */

/* ========================================================================
 * GROUP D40-D44: kanal_oluştur (R-KANAL kurucusu)
 *
 * D-292'ye kadar DRF V1'de kanal KURUCUSU YOKTU: her test kanalı parametre
 * olarak alıyordu, hiçbiri yaratmıyordu → gerçek bir programda kanal<T> elde
 * etmek imkânsızdı. Karar (Mehmet): tek yönsüz kanal<T> + kanal_oluştur.
 * ======================================================================== */

static void T40_kanal_olustur_annotasyonlu(void) {
    /* T DEĞER argümanından çıkarsanamaz (kanal boş başlar) → bağlamdan gelir. */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k: kanal<tam32> = kanal_olu\xc5\x9ftur(4);\n");
    test_sonuc("D40: değişken k: kanal<tam32> = kanal_oluştur(4) = 0 hata",
               h == 0);
}

static void T41_kanal_olustur_baglamsiz(void) {
    /* Bağlam yoksa T bilinemez → sessizce bir T uydurmak yerine DRF006. */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k = kanal_olu\xc5\x9ftur(4);\n");
    test_sonuc("D41: kanal_oluştur bağlamsız -> DRF006 (T çıkarsanamaz)",
               h >= 1);
}

static void T42_kanal_olustur_arity(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k: kanal<tam32> = kanal_olu\xc5\x9ftur();\n");
    test_sonuc("D42: kanal_oluştur() 0 arg -> DRF006", h >= 1);
}

static void T43_kanal_olustur_kapasite_tamsayi_degil(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k: kanal<tam32> = "
        "kanal_olu\xc5\x9ftur(\"dort\");\n");
    test_sonuc("D43: kanal_oluştur(metin) -> DRF006 (kapasite tamsayı olmalı)",
               h >= 1);
}

static void T47_gorev_kesirli_reddedilir_baslat(void) {
    /* D-294 KATMANLI SAVUNMA — 1. katman (tip kontrolü).
     * Runtime görev sonucunu TAMSAYI dönüşlü bir fn-ptr ile alır (x0/rax);
     * kesirli dönüş v0/xmm0'dadır → değer SESSİZCE çöp olurdu. Bitcast'lamak
     * hata modunu loud→silent'a çevirirdi (D-293'te tam bu tuzağa düşüldü).
     * Kapasite kaybı YOK: görev<kesirli*> zaten hiç derlenmiyordu (LLVM tip
     * hatası); kazanç düzgün bir KEMGU tanısı.
     * 2. katman (--llvm tip kontrolünü atlar) test_llvm.c'de. */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken g = g\xc3\xb6rev_ba\xc5\x9f" "lat(|| 3.5);\n");
    test_sonuc("D47: gorev_baslat(|| 3.5) -> DRF001 (kesirli T yok)", h >= 1);
}

static void T48_gorev_kesirli_reddedilir_annot(void) {
    /* Aynı kısıt ANNOTASYON/PARAMETRE yolunda da geçerli olmalı —
     * görev_başlat yalnız YARATMA yolunu kapsar. */
    int h = hata_sayisi(
        "i\xc5\x9flev test(g: g\xc3\xb6rev<kesirli64>) -> kesirli64 {\n"
        "    ver g\xc3\xb6rev_birle\xc5\x9ftir(g);\n"
        "}\n");
    test_sonuc("D48: gorev<kesirli64> parametre -> DRF001", h >= 1);
}

static void T49_gorev_metin_kabul(void) {
    /* Kısıt fazla geniş olmamalı: işaretçi T (metin) D-294'te ÇALIŞIR
     * (runtime i64 taşır + inttoptr). Uçtan uca kanıt test_llvm.c'de. */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken g: g\xc3\xb6rev<metin> = "
        "g\xc3\xb6rev_ba\xc5\x9f" "lat(|| \"selam\");\n"
        "    de\xc4\x9fi\xc5\x9fken s: metin = g\xc3\xb6rev_birle\xc5\x9ftir(g);\n");
    test_sonuc("D49: gorev<metin> = 0 hata (isaretci T destekleniyor)", h == 0);
}

static void T45_kanal_tam64_calisir(void) {
    /* D-295 (D-292'nin kısıtını DEĞİŞTİRİR). D-292'de kanal<tam64> DRF006 ile
     * reddediliyordu; o tıkaç yalnız `--check`i kapatıyordu ve `--llvm` tip
     * kontrolü ÇALIŞTIRMADIĞI için o yoldan derlenip SESSİZCE veri kaybediyordu
     * (ölçüldü: 2^33 gönder→al eşit değil). Kısıtı gürültülü yapmak yerine
     * SINIFI YOK ETTİK: kanal tamponu int64_t (görev ile simetrik) → kırpma
     * imkânsız. Uçtan uca kanıt (2^33 turu) test_llvm.c'de. */
    int h = hata_sayisi(
        "i\xc5\x9flev test(k: kanal<tam64>) -> tam64 {\n"
        "    ver kanal_al(k);\n"
        "}\n");
    test_sonuc("D45: kanal<tam64> = 0 hata (i64 tasima, kirpma yok)", h == 0);
}

static void T50_kanal_kesirli_reddedilir(void) {
    /* D-295 KALAN kısıt: kesirli T. Kanal tamsayı taşır; kesirli değer i64'e
     * bit-korumalı girmez (fptosi DEĞERİ bozar) → görev ile AYNI gerekçeyle
     * tip seviyesinde reddedilir. 2. katman (--llvm) test_llvm.c'de: emisyon
     * `sext double -> i64` üretir, bu GEÇERSİZDİR → LLVM gürültülü reddeder. */
    int h = hata_sayisi(
        "i\xc5\x9flev test(k: kanal<kesirli64>) -> kesirli64 {\n"
        "    ver kanal_al(k);\n"
        "}\n");
    test_sonuc("D50: kanal<kesirli64> -> DRF006 (kesirli T yok)", h >= 1);
}

static void T46_kanal_dar_tamsayi_kabul(void) {
    /* D-295: dar tamsayı da kayıpsız (i64 taşıma + alımda trunc). */
    int h = hata_sayisi(
        "i\xc5\x9flev test(k: kanal<tam8>) -> tam8 {\n"
        "    ver kanal_al(k);\n"
        "}\n");
    test_sonuc("D46: kanal<tam8> = 0 hata (dar tamsayı i32'ye kayıpsız sığar)",
               h == 0);
}

static void T44_kanal_olustur_gonder_al_kompozisyon(void) {
    /* Kurucu + gönder + al birlikte: uçtan uca tip akışı. kanal<T> LİNEER
     * DEĞİL → k tüketilmez, tekrar kullanılabilir (D31 ile tutarlı). */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k: kanal<tam32> = kanal_olu\xc5\x9ftur(2);\n"
        "    kanal_g\xc3\xb6nder(k, 7);\n"
        "    de\xc4\x9fi\xc5\x9fken v: tam32 = kanal_al(k);\n");
    test_sonuc("D44: kanal_oluştur + gönder + al kompozisyonu = 0 hata",
               h == 0);
}

int main(void) {
    printf("=== KEMGU DRF Test Paketi V1 ===\n");
    printf("Plan: belgeler/KEMGU_DRF_Genisletme_Plan.md\n");
    printf("Teorem: belgeler/KEMGU_DRF_Teoremi.md\n");
    printf("Lemmalar: belgeler/KEMGU_DRF_Lemmalar.md\n\n");

    /* D1-D6: Tip ifadesi */
    T1_gorev_tam32();
    T2_gorev_metin();
    T3_kanal_tam32();
    T4_kanal_metin();
    T5_gorev_nested_kanal();
    T6_kanal_nested_dizi();

    /* D7-D12: gorev_baslat */
    T7_gorev_baslat_lambda();
    T8_gorev_baslat_lambda_metin();
    T9_gorev_baslat_sifir_arg();
    T10_gorev_baslat_iki_arg();
    T11_gorev_baslat_non_islev();
    T12_gorev_baslat_donus_cikarsama();

    /* D13-D18: gorev_birlestir */
    T13_birlestir_basit();
    T14_birlestir_iki_kez();
    T15_birlestir_arity_sifir();
    T16_birlestir_arity_iki();
    T17_birlestir_non_gorev();
    T18_gorev_tuketilmedi();

    /* D19-D24: kanal_gonder / kanal_al */
    T19_kanal_gonder_arity();
    T20_kanal_gonder_non_kanal();
    T21_kanal_al_arity();
    T22_kanal_al_non_kanal();
    T23_kanal_round_trip();
    T24_kanal_gonder_tip_uyumsuz();

    /* D25-D28: dondur */
    T25_dondur_basit();
    T26_dondur_arity_sifir();
    T27_dondur_non_ref();
    T28_dondur_immutable_ref();

    /* D29-D32: Linear miras */
    T29_gorev_tip_lineer();
    T30_gorev_referans_alma();
    T31_kanal_non_linear();
    T32_gorev_yapi_icinde();

    /* D33-D36: Integration */
    T33_gorev_kanal_birlikte();
    T34_tekkez_gorev_lc2();
    T35_yetki_gorev_drf_l6();
    T36_kanal_lineer_v_tuketildi();

    /* D37-D39: Patch P2 — non-linear capture + dondur idempotency */
    T37_dizi_yakala_pozitif();
    T38_dizi_yakala_sonrasi_erisim_v1_limit();
    T39_dondur_idempotent_degil();

    /* D40-D44: kanal_oluştur (R-KANAL kurucusu — D-292) */
    T40_kanal_olustur_annotasyonlu();
    T41_kanal_olustur_baglamsiz();
    T42_kanal_olustur_arity();
    T43_kanal_olustur_kapasite_tamsayi_degil();
    T44_kanal_olustur_gonder_al_kompozisyon();
    T45_kanal_tam64_calisir();
    T50_kanal_kesirli_reddedilir();
    T46_kanal_dar_tamsayi_kabul();

    /* D47-D49: görev<T> genişletme + kesirli T reddi (D-294) */
    T47_gorev_kesirli_reddedilir_baslat();
    T48_gorev_kesirli_reddedilir_annot();
    T49_gorev_metin_kabul();

    printf("\n=== %d/%d test gecti (basarili) ===\n", basarili, toplam_test);
    if (basarisiz > 0) {
        printf("=== %d test BASARISIZ ===\n", basarisiz);
        return 1;
    }
    return 0;
}
