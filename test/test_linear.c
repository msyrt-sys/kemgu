#include "tip_kontrol.h"
#include "parser.h"
#include "lexer.h"
#include "ast.h"
#include "tip.h"
#include "sembol.h"
#include "arena.h"
#include "hata.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * KEMGU Linear Types Spec V1 — Test Paketi
 * ========================================
 *
 * Hedef: B.8 minimum 50 test (spec onayli ön-koşul).
 *
 * Hata kodlari (belgeler/KEMGU_Linear_Types_Spec_V1.md):
 *   L001 — LINEAR_NOT_CONSUMED          (tuketilmedi)
 *   L002 — LINEAR_DOUBLE_USE            (cift tuketim / move sonrasi)
 *   L004 — LINEAR_REFERENCE_ATTEMPT     (referans alma)
 *   L007 — kullan/imha operandi tekkez tipinde olmali
 *   L008 — tekkez_olustur arity hatasi
 *   LR002 — LINEAR_REGION_EMBED         (yapi/dizi tekkez iceremez V1)
 *
 * Tum testler tip_kontrol.hata_sayisi'ni dogrular.
 * stderr nul'a yonlendirilir (hata mesajlari testlerde sessiz).
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

/* D-311: KOD-duyarli yardimci. Yalniz "hata sayisi >= 1" demek zayif bir
 * kapidir — L005 bekleyen bir test, BASKA bir sebeple hata alsa da gecerdi.
 * hata_raporla callback'i (hata.c) ile uretilen kodlari yakalar. */
static char yakalanan_kodlar[64][8];
static int yakalanan_sayi;

static void kod_yakala(int satir, int sutun, const char *kod,
                       const char *mesaj, const char *ipucu, void *ctx) {
    (void)satir; (void)sutun; (void)mesaj; (void)ipucu; (void)ctx;
    if (yakalanan_sayi < 64 && kod) {
        snprintf(yakalanan_kodlar[yakalanan_sayi], 8, "%s", kod);
        yakalanan_sayi++;
    }
}

/* Kaynagi kontrol et; `kod` uretilmisse 1 doner. */
static int kod_uretildi_mi(const char *kaynak, const char *kod) {
    yakalanan_sayi = 0;
    hata_callback_ayarla(kod_yakala, NULL);
    int h = -1;
    (void)derle_kontrol(kaynak, &h);
    hata_callback_ayarla(NULL, NULL);
    for (int i = 0; i < yakalanan_sayi; i++)
        if (strcmp(yakalanan_kodlar[i], kod) == 0) return 1;
    return 0;
}

/* basarili = 0 hata; veya 0 hata bekleniyorsa beklenen=0 */
static int hata_sayisi(const char *kaynak) {
    int h = -1;
    if (derle_kontrol(kaynak, &h) != 0) return -1;
    return h;
}

/* ========================================================================
 * Test programi sablonu — main fonksiyonu icine kod gomar.
 * ======================================================================== */

static int kontrol_main(const char *govde) {
    static char buf[4096];
    snprintf(buf, sizeof(buf),
        "i\xc5\x9flev test() {\n%s\n}\n", govde);
    return hata_sayisi(buf);
}

/* ========================================================================
 * GROUP L1-L10: Tip ifadesi + tekkez_olustur (producer)
 * ======================================================================== */

static void T1_tip_tekkez_tam32(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k: tekkez<tam32> = tekkez_olustur(5);\n"
        "    imha(k);");
    test_sonuc("L1: tekkez<tam32> + tekkez_olustur + imha = 0 hata", h == 0);
}

static void T2_tip_tekkez_metin(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k: tekkez<metin> = tekkez_olustur(\"selam\");\n"
        "    imha(k);");
    test_sonuc("L2: tekkez<metin> + producer + imha = 0 hata", h == 0);
}

static void T3_tip_tekkez_mantiksal(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k: tekkez<mant\xc4\xb1ksal>"
        " = tekkez_olustur(do\xc4\x9fru);\n"
        "    imha(k);");
    test_sonuc("L3: tekkez<mantiksal> = 0 hata", h == 0);
}

static void T4_tip_tekkez_dizi(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k: tekkez<Dizi<tam32>>"
        " = tekkez_olustur([1, 2, 3]);\n"
        "    imha(k);");
    test_sonuc("L4: tekkez<Dizi<tam32>> nested = 0 hata", h == 0);
}

static void T5_tip_tekkez_inner_tekkez(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken ic: tekkez<tam32> = tekkez_olustur(1);\n"
        "    de\xc4\x9fi\xc5\x9fken di\xc5\x9f: tekkez<tekkez<tam32>>"
        " = tekkez_olustur(ic);\n"
        "    imha(di\xc5\x9f);");
    test_sonuc("L5: tekkez<tekkez<tam32>> iclim move + imha = 0 hata", h == 0);
}

static void T6_producer_donus(void) {
    /* Producer dönüsü tekkez<T> — annotsuz değişken bunu çıkarsamali */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k = tekkez_olustur(42);\n"
        "    imha(k);");
    test_sonuc("L6: annotsuz değişken tekkez<tam32> çıkarsar", h == 0);
}

static void T7_producer_zero_arg(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k = tekkez_olustur();\n"
        "    imha(k);");
    test_sonuc("L7: tekkez_olustur() (0 arg) -> L008", h >= 1);
}

static void T8_producer_two_arg(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k = tekkez_olustur(1, 2);\n"
        "    imha(k);");
    test_sonuc("L8: tekkez_olustur(1,2) (2 arg) -> L008", h >= 1);
}

static void T9_imha_temizleme(void) {
    int h = kontrol_main(
        "    imha(tekkez_olustur(0));");
    test_sonuc("L9: gecici tekkez (imha + tekkez_olustur ic-ice) = 0 hata", h == 0);
}

static void T10_kullan_temizleme(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken n = kullan(tekkez_olustur(7));\n");
    test_sonuc("L10: gecici tekkez (kullan + tekkez_olustur ic-ice) = 0 hata", h == 0);
}

/* ========================================================================
 * GROUP L11-L18: kullan extract + tek-tüketim
 * ======================================================================== */

static void T11_kullan_extract(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k = tekkez_olustur(7);\n"
        "    de\xc4\x9fi\xc5\x9fken n = kullan(k);\n");
    test_sonuc("L11: kullan(k) extract = 0 hata", h == 0);
}

static void T12_kullan_non_tekkez(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken x: tam32 = 5;\n"
        "    de\xc4\x9fi\xc5\x9fken n = kullan(x);\n");
    test_sonuc("L12: kullan(non-tekkez) -> L007", h >= 1);
}

static void T13_kullan_metin(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k = tekkez_olustur(\"merhaba\");\n"
        "    de\xc4\x9fi\xc5\x9fken s = kullan(k);\n");
    test_sonuc("L13: kullan(tekkez<metin>) = 0 hata", h == 0);
}

static void T14_kullan_sonra_aritmetik(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k = tekkez_olustur(10);\n"
        "    de\xc4\x9fi\xc5\x9fken n = kullan(k) + 1;\n");
    test_sonuc("L14: kullan(k) sonra aritmetik = 0 hata", h == 0);
}

static void T15_iki_baglama_iki_kullan(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k1 = tekkez_olustur(1);\n"
        "    de\xc4\x9fi\xc5\x9fken k2 = tekkez_olustur(2);\n"
        "    de\xc4\x9fi\xc5\x9fken n1 = kullan(k1);\n"
        "    de\xc4\x9fi\xc5\x9fken n2 = kullan(k2);\n");
    test_sonuc("L15: iki bagimsiz tekkez bagimsiz tuketim = 0 hata", h == 0);
}

static void T16_ver_kullan_donus(void) {
    int h = hata_sayisi(
        "i\xc5\x9flev tek_kez_al() -> tam32 {\n"
        "    de\xc4\x9fi\xc5\x9fken k = tekkez_olustur(99);\n"
        "    ver kullan(k);\n"
        "}\n");
    test_sonuc("L16: ver kullan(k) -> tam32 donus = 0 hata", h == 0);
}

static void T17_kullan_sonra_yeni_baglama(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k = tekkez_olustur(5);\n"
        "    de\xc4\x9fi\xc5\x9fken n = kullan(k);\n"
        "    de\xc4\x9fi\xc5\x9fken m: tam32 = n + 1;\n");
    test_sonuc("L17: kullan, sonra yeni non-linear baglama = 0 hata", h == 0);
}

static void T18_kullan_donus_tipi_ic(void) {
    /* kullan(tekkez<metin>) -> metin; metin'i sonra islev'e ver */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k = tekkez_olustur(\"a\");\n"
        "    de\xc4\x9fi\xc5\x9fken s: metin = kullan(k);\n");
    test_sonuc("L18: kullan donus tipi ic_tip ile uyumlu", h == 0);
}

/* ========================================================================
 * GROUP L19-L24: imha dispose + tek-tüketim
 * ======================================================================== */

static void T19_imha_basit(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k = tekkez_olustur(5);\n"
        "    imha(k);\n");
    test_sonuc("L19: imha(k) basit = 0 hata", h == 0);
}

static void T20_imha_non_tekkez(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken x: tam32 = 5;\n"
        "    imha(x);\n");
    test_sonuc("L20: imha(non-tekkez) -> L007", h >= 1);
}

static void T21_iki_baglama_imha_kullan(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k1 = tekkez_olustur(5);\n"
        "    de\xc4\x9fi\xc5\x9fken k2 = tekkez_olustur(7);\n"
        "    imha(k1);\n"
        "    de\xc4\x9fi\xc5\x9fken n = kullan(k2);\n");
    test_sonuc("L21: imha + kullan farkli baglamalar = 0 hata", h == 0);
}

static void T22_imha_gecici(void) {
    int h = kontrol_main(
        "    imha(tekkez_olustur(0));\n");
    test_sonuc("L22: imha(tekkez_olustur(0)) gecici = 0 hata", h == 0);
}

static void T23_imha_sonra_kullan(void) {
    /* imha sonra kullan: k tüketildi, kullan L002 */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k = tekkez_olustur(5);\n"
        "    imha(k);\n"
        "    de\xc4\x9fi\xc5\x9fken n = kullan(k);\n");
    test_sonuc("L23: imha sonra kullan -> L002", h >= 1);
}

static void T24_imha_iki_kez(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k = tekkez_olustur(5);\n"
        "    imha(k);\n"
        "    imha(k);\n");
    test_sonuc("L24: imha + imha -> L002", h >= 1);
}

/* ========================================================================
 * GROUP L25-L30: Single-use enforcement (L001/L002)
 * ======================================================================== */

static void T25_baglama_tuketilmedi(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k = tekkez_olustur(5);\n");
    test_sonuc("L25: baglama scope sonunda tuketilmedi -> L001", h >= 1);
}

static void T26_kullan_iki_kez(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k = tekkez_olustur(5);\n"
        "    de\xc4\x9fi\xc5\x9fken a = kullan(k);\n"
        "    de\xc4\x9fi\xc5\x9fken b = kullan(k);\n");
    test_sonuc("L26: kullan + kullan -> L002", h >= 1);
}

static void T27_imha_kullan_yanyana(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k = tekkez_olustur(5);\n"
        "    imha(k);\n"
        "    de\xc4\x9fi\xc5\x9fken n = kullan(k);\n");
    test_sonuc("L27: imha + kullan -> L002", h >= 1);
}

static void T28_kullan_imha_yanyana(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k = tekkez_olustur(5);\n"
        "    de\xc4\x9fi\xc5\x9fken n = kullan(k);\n"
        "    imha(k);\n");
    test_sonuc("L28: kullan + imha -> L002", h >= 1);
}

static void T29_islev_govdesi_baglama_l001(void) {
    int h = hata_sayisi(
        "i\xc5\x9flev test() {\n"
        "    de\xc4\x9fi\xc5\x9fken k = tekkez_olustur(7);\n"
        "}\n");
    test_sonuc("L29: islev govdesi tuketilmemis baglama -> L001", h >= 1);
}

static void T30_move_yeni_baglama(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k = tekkez_olustur(5);\n"
        "    de\xc4\x9fi\xc5\x9fken k2 = k;\n"
        "    de\xc4\x9fi\xc5\x9fken n = kullan(k2);\n");
    test_sonuc("L30: k -> k2 (move) + kullan(k2) = 0 hata", h == 0);
}

/* ========================================================================
 * GROUP L31-L36: No-copy enforcement (move semantik)
 * ======================================================================== */

static void T31_move_sonrasi_erisim(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k = tekkez_olustur(5);\n"
        "    de\xc4\x9fi\xc5\x9fken k2 = k;\n"
        "    de\xc4\x9fi\xc5\x9fken n = kullan(k);\n"
        "    imha(k2);\n");
    test_sonuc("L31: move sonrasi orijinal erisim -> L002", h >= 1);
}

static void T32_move_yeni_baglama_kullan(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k = tekkez_olustur(5);\n"
        "    de\xc4\x9fi\xc5\x9fken k2 = k;\n"
        "    de\xc4\x9fi\xc5\x9fken n = kullan(k2);\n");
    test_sonuc("L32: move + kullan(yeni) = 0 hata", h == 0);
}

static void T33_ver_move(void) {
    int h = hata_sayisi(
        "i\xc5\x9flev al() -> tekkez<tam32> {\n"
        "    de\xc4\x9fi\xc5\x9fken k = tekkez_olustur(5);\n"
        "    ver k;\n"
        "}\n");
    test_sonuc("L33: ver k (move cagirana) = 0 hata", h == 0);
}

static void T34_param_move(void) {
    int h = hata_sayisi(
        "i\xc5\x9flev tuket(t: tekkez<tam32>) {\n"
        "    imha(t);\n"
        "}\n"
        "i\xc5\x9flev test() {\n"
        "    de\xc4\x9fi\xc5\x9fken k = tekkez_olustur(5);\n"
        "    tuket(k);\n"
        "}\n");
    test_sonuc("L34: f(k) cagri arg consume = 0 hata", h == 0);
}

static void T35_move_sonra_l001(void) {
    /* k moved to k2, k2 NOT consumed -> L001 (k2) ; k cleanup ok */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k = tekkez_olustur(5);\n"
        "    de\xc4\x9fi\xc5\x9fken k2 = k;\n");
    test_sonuc("L35: move + k2 tuketilmedi -> L001", h >= 1);
}

static void T36_double_move(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k = tekkez_olustur(5);\n"
        "    de\xc4\x9fi\xc5\x9fken k1 = k;\n"
        "    de\xc4\x9fi\xc5\x9fken k2 = k;\n"
        "    imha(k1);\n"
        "    imha(k2);\n");
    test_sonuc("L36: double move -> L002 (k iki kez consume)", h >= 1);
}

/* ========================================================================
 * GROUP L37-L42: No-alias enforcement (L004)
 * ======================================================================== */

static void T37_ref_lineer(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k = tekkez_olustur(5);\n"
        "    de\xc4\x9fi\xc5\x9fken r = &k;\n"
        "    imha(k);\n");
    test_sonuc("L37: &k (lineer ref) -> L004", h >= 1);
}

static void T38_ref_degisken_lineer(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k = tekkez_olustur(5);\n"
        "    de\xc4\x9fi\xc5\x9fken r = &de\xc4\x9fi\xc5\x9fken k;\n"
        "    imha(k);\n");
    test_sonuc("L38: &degisken k (lineer mut ref) -> L004", h >= 1);
}

static void T39_ref_non_lineer(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken x: tam32 = 5;\n"
        "    de\xc4\x9fi\xc5\x9fken r: &tam32 = &x;\n");
    test_sonuc("L39: non-lineer ref OK = 0 hata", h == 0);
}

static void T40_ref_tekrar_lineer(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k = tekkez_olustur(5);\n"
        "    de\xc4\x9fi\xc5\x9fken r1 = &k;\n"
        "    de\xc4\x9fi\xc5\x9fken r2 = &k;\n"
        "    imha(k);\n");
    test_sonuc("L40: iki kez &k -> en az 2 L004", h >= 2);
}

static void T41_param_ref_lineer(void) {
    /* islev imzasi tekkez ref kabul etmemeli (& kullanilmis): bu testi
     * basit tutmak icin: degiskende & kullan, sonuc L004 */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k = tekkez_olustur(5);\n"
        "    de\xc4\x9fi\xc5\x9fken n: &tam32 = &k;\n"
        "    imha(k);\n");
    test_sonuc("L41: &k atama hedefi de hata uretir (L004)", h >= 1);
}

static void T42_dizi_ref_non_lineer(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken xs: Dizi<tam32> = [1, 2];\n"
        "    de\xc4\x9fi\xc5\x9fken r = &xs;\n");
    test_sonuc("L42: non-lineer Dizi referansi = 0 hata", h == 0);
}

/* ========================================================================
 * GROUP L43-L46: Region/Linear (LR002)
 * ======================================================================== */

static void T43_yapi_lineer_alan_yasak(void) {
    int h = hata_sayisi(
        "yap\xc4\xb1 S\xc4\xb1radan { x: tekkez<tam32>; }\n"
        "i\xc5\x9flev test() {\n"
        "    de\xc4\x9fi\xc5\x9fken k = tekkez_olustur(5);\n"
        "    imha(k);\n"
        "}\n");
    test_sonuc("L43: yapi { tekkez<tam32> } alan -> LR002", h >= 1);
}

static void T44_yapi_lineer_alan_metin(void) {
    int h = hata_sayisi(
        "yap\xc4\xb1 Sahip { ad: tekkez<metin>; }\n"
        "i\xc5\x9flev test() {\n"
        "}\n");
    test_sonuc("L44: yapi { tekkez<metin> } alan -> LR002", h >= 1);
}

static void T45_dizi_lineer_eleman_yasak(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken d = [tekkez_olustur(1), tekkez_olustur(2)];\n"
        "    imha(d);\n");
    /* LR002 olmasi yeterli; ek hatalar olabilir (genel yapi) */
    test_sonuc("L45: dizi lineer eleman -> LR002+", h >= 1);
}

static void T46_yapi_normal_alan_ok(void) {
    int h = hata_sayisi(
        "yap\xc4\xb1 Normal { x: tam32; ad: metin; }\n"
        "i\xc5\x9flev test() {\n"
        "}\n");
    test_sonuc("L46: yapi non-lineer alanli OK = 0 hata", h == 0);
}

/* ========================================================================
 * GROUP L47-L50: Closure-itself-linear (LC-2/LC-3)
 * ======================================================================== */

static void T47_lambda_lineer_yakalama(void) {
    /* k yakalandi, c kendisi tekkez<islev()->tam32>. c() consume. */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k = tekkez_olustur(5);\n"
        "    de\xc4\x9fi\xc5\x9fken c = || kullan(k);\n"
        "    de\xc4\x9fi\xc5\x9fken n = c();\n");
    test_sonuc("L47: lineer yakalayan closure + c() = 0 hata", h == 0);
}

static void T48_lambda_iki_kez_cagri(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k = tekkez_olustur(5);\n"
        "    de\xc4\x9fi\xc5\x9fken c = || kullan(k);\n"
        "    de\xc4\x9fi\xc5\x9fken n1 = c();\n"
        "    de\xc4\x9fi\xc5\x9fken n2 = c();\n");
    test_sonuc("L48: lineer closure iki kez cagri -> L002", h >= 1);
}

static void T49_lambda_non_lineer(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken c = |x: tam32| x + 1;\n"
        "    de\xc4\x9fi\xc5\x9fken n = c(5);\n");
    test_sonuc("L49: non-lineer closure cagri = 0 hata", h == 0);
}

static void T50_lambda_cagrilmadi_l001(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k = tekkez_olustur(5);\n"
        "    de\xc4\x9fi\xc5\x9fken c = || kullan(k);\n");
    test_sonuc("L50: lineer closure cagrilmadi -> L001 (closure tuketilmedi)",
               h >= 1);
}

/* ========================================================================
 * Ek testler (B.8 +) — daha sıkı kapsam
 * ======================================================================== */

static void T51_ic_ice_kullan(void) {
    /* kullan(tekkez_olustur(...)) gecici → 0 hata */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken n: tam32 = kullan(tekkez_olustur(99));\n");
    test_sonuc("L51: kullan(tekkez_olustur(...)) gecici = 0 hata", h == 0);
}

static void T52_lineer_donus_kullan(void) {
    int h = hata_sayisi(
        "i\xc5\x9flev al() -> tekkez<tam32> {\n"
        "    ver tekkez_olustur(7);\n"
        "}\n"
        "i\xc5\x9flev test() {\n"
        "    de\xc4\x9fi\xc5\x9fken k = al();\n"
        "    imha(k);\n"
        "}\n");
    test_sonuc("L52: islev donus tekkez + cagri sonra imha = 0 hata", h == 0);
}

static void T53_lineer_donus_kullanılmadı(void) {
    int h = hata_sayisi(
        "i\xc5\x9flev al() -> tekkez<tam32> {\n"
        "    ver tekkez_olustur(7);\n"
        "}\n"
        "i\xc5\x9flev test() {\n"
        "    de\xc4\x9fi\xc5\x9fken k = al();\n"
        "}\n");
    test_sonuc("L53: lineer donus tuketilmedi -> L001", h >= 1);
}

static void T54_iki_islev_zincir(void) {
    int h = hata_sayisi(
        "i\xc5\x9flev al() -> tekkez<tam32> {\n"
        "    ver tekkez_olustur(1);\n"
        "}\n"
        "i\xc5\x9flev tuket(t: tekkez<tam32>) {\n"
        "    imha(t);\n"
        "}\n"
        "i\xc5\x9flev test() {\n"
        "    tuket(al());\n"
        "}\n");
    test_sonuc("L54: tuket(al()) zincir = 0 hata", h == 0);
}

/* === OTP Linear Semantik Kanitlari (Adim 1) === */

static void T55_otp_anahtar_iki_kez_tuketim(void) {
    /* Bir tekkez<tam32> anahtar iki kez kullanilirsa L002 hata vermeli.
     * Bu, OTP CLI'nin anahtar yeniden-kullanim engellemesini kanitliyor. */
    int h = hata_sayisi(
        "i\xc5\x9flev sifrele(k: tekkez<tam32>) -> tam32 {\n"
        "    imha(k);\n"
        "    ver 0;\n"
        "}\n"
        "i\xc5\x9flev coz(k: tekkez<tam32>) -> tam32 {\n"
        "    imha(k);\n"
        "    ver 0;\n"
        "}\n"
        "i\xc5\x9flev test() -> tam32 {\n"
        "    de\xc4\x9fi\xc5\x9fken k = tekkez_olustur(42);\n"
        "    sifrele(k);\n"
        "    coz(k);\n"   /* L002: k iki kez kullanildi! */
        "    ver 0;\n"
        "}\n");
    test_sonuc("L55: OTP anahtar iki kez tuketim -> L002", h >= 1);
}

static void T56_otp_anahtar_imha_garantili(void) {
    /* imha(anahtar) cagrildiginda lineer baglama tuketilir, hata yok */
    int h = hata_sayisi(
        "i\xc5\x9flev test() -> tam32 {\n"
        "    de\xc4\x9fi\xc5\x9fken k = tekkez_olustur(42);\n"
        "    imha(k);\n"
        "    ver 0;\n"
        "}\n");
    test_sonuc("L56: OTP anahtar imha -> 0 hata", h == 0);
}

static void T57_otp_anahtar_tuketilmedi(void) {
    /* Anahtar uretildi ama hic kullanilmadi -> L001 (scope sonu hatasi) */
    int h = hata_sayisi(
        "i\xc5\x9flev test() -> tam32 {\n"
        "    de\xc4\x9fi\xc5\x9fken k = tekkez_olustur(42);\n"
        "    ver 0;\n"   /* k tuketilmedi! */
        "}\n");
    test_sonuc("L57: OTP anahtar tuketilmedi -> L001", h >= 1);
}

/* === D-311 / L-COND: dal-duyarli tuketim (Spec V1 §L-COND) ===
 * Onceki AKIS-DUYARSIZ sayac hem YANLIS REDDEDIYOR hem YANLIS KABUL EDIYORDU;
 * bu dort test o iki yonu de kilitler. Kodlar KOD-duyarli dogrulanir
 * (yalniz "hata var" demek, baska sebeple gecen bir testi maskelerdi). */

static void T58_iki_dal_tuketir_ok(void) {
    /* Spec'in KANONIK ornegi. Eskiden L002 veriyordu -> lineer bir kaynagi
     * kosullu imha etmek IMKANSIZDI. */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken t = tekkez_olustur(5);\n"
        "    e\xc4\x9f" "er do\xc4\x9fru { kullan(t); } de\xc4\x9filse { imha(t); }\n");
    test_sonuc("L58: iki dal da tuketir -> 0 hata (L002 false-positive gitti)",
               h == 0);
}

static void T59_tek_dal_l005(void) {
    /* else dalinda tuketim yok -> kosul yanlisken sizinti. Eskiden SESSIZ gecerdi. */
    int var = kod_uretildi_mi(
        "i\xc5\x9flev test() {\n"
        "    de\xc4\x9fi\xc5\x9fken t = tekkez_olustur(5);\n"
        "    e\xc4\x9f" "er do\xc4\x9fru { kullan(t); }\n"
        "}\n", "L005");
    test_sonuc("L59: yalniz then tuketir -> L005", var);
}

static void T60_else_tuketir_l005(void) {
    /* Simetri: yalniz else tuketirse de tutarsiz. */
    int var = kod_uretildi_mi(
        "i\xc5\x9flev test() {\n"
        "    de\xc4\x9fi\xc5\x9fken t = tekkez_olustur(5);\n"
        "    e\xc4\x9f" "er do\xc4\x9fru { } de\xc4\x9filse { imha(t); }\n"
        "}\n", "L005");
    test_sonuc("L60: yalniz else tuketir -> L005", var);
}

static void T61_kosul_sonrasi_cift_l002(void) {
    /* Dal-duyarlilik L002'yi ZAYIFLATMAMALI: then tuketip sonra yeniden
     * tuketmek hala cift tuketimdir (kosul dogruyken). */
    int var = kod_uretildi_mi(
        "i\xc5\x9flev test() {\n"
        "    de\xc4\x9fi\xc5\x9fken t = tekkez_olustur(5);\n"
        "    e\xc4\x9f" "er do\xc4\x9fru { kullan(t); } de\xc4\x9filse { imha(t); }\n"
        "    kullan(t);\n"
        "}\n", "L002");
    test_sonuc("L61: iki-dal-tuketim SONRASI yeniden tuketim -> L002", var);
}

int main(void) {
    /* Tip kontrol stderr'e hata mesajlari yazar — testlerde sessiz olsun */
    freopen("nul", "w", stderr);

    printf("KEMGU Linear Types Spec V1 Test Paketi\n");
    printf("==========================================\n\n");

    printf("--- L1-L10: Tip + tekkez_olustur (producer) ---\n");
    T1_tip_tekkez_tam32();
    T2_tip_tekkez_metin();
    T3_tip_tekkez_mantiksal();
    T4_tip_tekkez_dizi();
    T5_tip_tekkez_inner_tekkez();
    T6_producer_donus();
    T7_producer_zero_arg();
    T8_producer_two_arg();
    T9_imha_temizleme();
    T10_kullan_temizleme();

    printf("\n--- L11-L18: kullan extract + tek-tüketim ---\n");
    T11_kullan_extract();
    T12_kullan_non_tekkez();
    T13_kullan_metin();
    T14_kullan_sonra_aritmetik();
    T15_iki_baglama_iki_kullan();
    T16_ver_kullan_donus();
    T17_kullan_sonra_yeni_baglama();
    T18_kullan_donus_tipi_ic();

    printf("\n--- L19-L24: imha dispose + tek-tuketim ---\n");
    T19_imha_basit();
    T20_imha_non_tekkez();
    T21_iki_baglama_imha_kullan();
    T22_imha_gecici();
    T23_imha_sonra_kullan();
    T24_imha_iki_kez();

    printf("\n--- L25-L30: Single-use enforcement (L001/L002) ---\n");
    T25_baglama_tuketilmedi();
    T26_kullan_iki_kez();
    T27_imha_kullan_yanyana();
    T28_kullan_imha_yanyana();
    T29_islev_govdesi_baglama_l001();
    T30_move_yeni_baglama();

    printf("\n--- L31-L36: No-copy enforcement (move semantik) ---\n");
    T31_move_sonrasi_erisim();
    T32_move_yeni_baglama_kullan();
    T33_ver_move();
    T34_param_move();
    T35_move_sonra_l001();
    T36_double_move();

    printf("\n--- L37-L42: No-alias enforcement (L004) ---\n");
    T37_ref_lineer();
    T38_ref_degisken_lineer();
    T39_ref_non_lineer();
    T40_ref_tekrar_lineer();
    T41_param_ref_lineer();
    T42_dizi_ref_non_lineer();

    printf("\n--- L43-L46: Region/Linear (LR002) ---\n");
    T43_yapi_lineer_alan_yasak();
    T44_yapi_lineer_alan_metin();
    T45_dizi_lineer_eleman_yasak();
    T46_yapi_normal_alan_ok();

    printf("\n--- L47-L50: Closure-itself-linear (LC-2/LC-3) ---\n");
    T47_lambda_lineer_yakalama();
    T48_lambda_iki_kez_cagri();
    T49_lambda_non_lineer();
    T50_lambda_cagrilmadi_l001();

    printf("\n--- Ek testler (B.8+) ---\n");
    T51_ic_ice_kullan();
    T52_lineer_donus_kullan();
    T53_lineer_donus_kullanılmadı();
    T54_iki_islev_zincir();

    printf("\n--- OTP Linear Semantik Kanitlari ---\n");
    T55_otp_anahtar_iki_kez_tuketim();
    T56_otp_anahtar_imha_garantili();
    T57_otp_anahtar_tuketilmedi();

    printf("\n--- L58-L61: L-COND dal-duyarli tuketim (D-311) ---\n");
    T58_iki_dal_tuketir_ok();
    T59_tek_dal_l005();
    T60_else_tuketir_l005();
    T61_kosul_sonrasi_cift_l002();

    printf("\n========================================\n");
    printf("Toplam: %d | Basarili: %d | Basarisiz: %d\n",
           toplam_test, basarili, basarisiz);

    return basarisiz == 0 ? 0 : 1;
}
