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

/* === Helper'lar === */

static Sembol mk_sembol_a(Arena *a, const char *ad,
                           SembolKategorisi kat, TipBilgisi *tip) {
    Sembol s;
    memset(&s, 0, sizeof(s));
    int uz = (int)strlen(ad);
    s.ad = ast_string_kopyala(a, ad, uz);
    s.ad_uzunluk = uz;
    s.kategori = kat;
    s.tip = tip;
    return s;
}

/* Scope'u sik kullanilan sembollerle pre-populate et */
static Scope *scope_hazirla(Arena *a) {
    Scope *g = scope_olustur(a, SCOPE_GLOBAL, NULL);
    TipBilgisi *t32 = tip_olustur_basit(a, TIP_TAM32);
    TipBilgisi *tm  = tip_olustur_basit(a, TIP_METIN);
    TipBilgisi *tmant = tip_olustur_basit(a, TIP_MANTIKSAL);

    /* Degiskenler */
    Sembol s;
    s = mk_sembol_a(a, "x",  SEMBOL_DEGISKEN, t32);   sembol_ekle(g, a, &s);
    s = mk_sembol_a(a, "y",  SEMBOL_DEGISKEN, t32);   sembol_ekle(g, a, &s);
    s = mk_sembol_a(a, "z",  SEMBOL_DEGISKEN, tmant); sembol_ekle(g, a, &s);

    /* xs: Dizi<tam32> */
    s = mk_sembol_a(a, "xs", SEMBOL_DEGISKEN, tip_olustur_dizi(a, t32));
    sembol_ekle(g, a, &s);

    /* p32: *tam32 (pointer) */
    s = mk_sembol_a(a, "p32", SEMBOL_DEGISKEN, tip_olustur_pointer(a, t32));
    sembol_ekle(g, a, &s);

    /* f: islev() -> tam32 */
    s = mk_sembol_a(a, "f", SEMBOL_ISLEV,
        tip_olustur_islev(a, NULL, 0, t32));
    sembol_ekle(g, a, &s);

    /* g: islev(tam32) -> mantiksal */
    TipBilgisi **gp = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
    gp[0] = t32;
    s = mk_sembol_a(a, "g", SEMBOL_ISLEV,
        tip_olustur_islev(a, gp, 1, tmant));
    sembol_ekle(g, a, &s);

    /* h: Hasta yapi degiskeni */
    Scope *hasta_s = scope_olustur(a, SCOPE_YAPI, NULL);
    Sembol alan_ad = mk_sembol_a(a, "ad", SEMBOL_DEGISKEN, tm);
    Sembol alan_yas = mk_sembol_a(a, "yas", SEMBOL_DEGISKEN, t32);
    sembol_ekle(hasta_s, a, &alan_ad);
    sembol_ekle(hasta_s, a, &alan_yas);

    Sembol hasta_yapi = mk_sembol_a(a, "Hasta", SEMBOL_YAPI, NULL);
    hasta_yapi.yapi_scope = hasta_s;
    sembol_ekle(g, a, &hasta_yapi);

    TipBilgisi *hasta_tipi = tip_olustur_yapi(a, "Hasta", 5, NULL, 0);
    s = mk_sembol_a(a, "h", SEMBOL_DEGISKEN, hasta_tipi);
    sembol_ekle(g, a, &s);

    /* matematik modulu */
    Scope *mat_s = scope_olustur(a, SCOPE_MODUL, NULL);
    Sembol pi = mk_sembol_a(a, "PI", SEMBOL_SABIT,
        tip_olustur_basit(a, TIP_KESIRLI64));
    sembol_ekle(mat_s, a, &pi);
    Sembol mat = mk_sembol_a(a, "matematik", SEMBOL_MODUL, NULL);
    mat.modul_scope = mat_s;
    sembol_ekle(g, a, &mat);

    return g;
}

/* Ifadeyi parse et + tip_belirle. */
static TipBilgisi *ifade_tipi(const char *kaynak, Arena *a, Scope *scope,
                               int *hata_out) {
    /* Sabit ile sar */
    static char buf[2048];
    snprintf(buf, sizeof(buf), "sabit _X: tam32 = %s;", kaynak);

    Lexer l;
    lexer_baslat(&l, buf, "test");
    Parser p;
    parser_baslat(&p, &l, a, "test", buf);
    Dugum *prog = parser_calistir(&p);
    if (!prog || prog->veri.program.sayi == 0) {
        if (hata_out) *hata_out = -1;
        return NULL;
    }
    Dugum *sabit = prog->veri.program.uyeler[0];
    if (sabit->tip != DUGUM_SABIT) {
        if (hata_out) *hata_out = -1;
        return NULL;
    }
    Dugum *ifade = sabit->veri.sabit.deger;

    TipKontrol tk;
    tip_kontrol_baslat(&tk, a, scope, "test", buf);
    TipBilgisi *t = tip_belirle(&tk, ifade);
    if (hata_out) *hata_out = tk.hata_sayisi;
    return t;
}

static int tip_kategorisi_esit(const TipBilgisi *t, TipKategorisi k) {
    return t && t->kategori == k;
}

/* === Literaller === */

static void test_lit_tam(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    TipBilgisi *t = ifade_tipi("42", a, s, &hata);
    int ok = hata == 0 && tip_kategorisi_esit(t, TIP_TAM32);
    test_sonuc("literal: 42 -> tam32", ok);
    arena_serbest(a);
}

static void test_lit_kesirli(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    TipBilgisi *t = ifade_tipi("3.14", a, s, &hata);
    int ok = hata == 0 && tip_kategorisi_esit(t, TIP_KESIRLI64);
    test_sonuc("literal: 3.14 -> kesirli64", ok);
    arena_serbest(a);
}

static void test_lit_metin(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    TipBilgisi *t = ifade_tipi("\"hello\"", a, s, &hata);
    int ok = hata == 0 && tip_kategorisi_esit(t, TIP_METIN);
    test_sonuc("literal: \"hello\" -> metin", ok);
    arena_serbest(a);
}

static void test_lit_karakter(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    TipBilgisi *t = ifade_tipi("'a'", a, s, &hata);
    int ok = hata == 0 && tip_kategorisi_esit(t, TIP_KARAKTER);
    test_sonuc("literal: 'a' -> karakter", ok);
    arena_serbest(a);
}

static void test_lit_dogru(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    /* dogru */
    TipBilgisi *t = ifade_tipi("do\xc4\x9fru", a, s, &hata);
    int ok = hata == 0 && tip_kategorisi_esit(t, TIP_MANTIKSAL);
    test_sonuc("literal: dogru -> mantiksal", ok);
    arena_serbest(a);
}

static void test_lit_bos(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    /* bos */
    TipBilgisi *t = ifade_tipi("bo\xc5\x9f", a, s, &hata);
    int ok = hata == 0 && tip_kategorisi_esit(t, TIP_BOS);
    test_sonuc("literal: bos -> bos", ok);
    arena_serbest(a);
}

/* === Tanimlayici === */

static void test_id_var(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    TipBilgisi *t = ifade_tipi("x", a, s, &hata);
    int ok = hata == 0 && tip_kategorisi_esit(t, TIP_TAM32);
    test_sonuc("id: x -> tam32 (sembol tablosundan)", ok);
    arena_serbest(a);
}

static void test_id_yok(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    TipBilgisi *t = ifade_tipi("bilinmeyen", a, s, &hata);
    int ok = hata > 0 && tip_kategorisi_esit(t, TIP_HATA);
    test_sonuc("id: bilinmeyen -> T002 hata", ok);
    arena_serbest(a);
}

/* === Ikili === */

static void test_ikili_arti(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    TipBilgisi *t = ifade_tipi("1 + 2", a, s, &hata);
    int ok = hata == 0 && tip_kategorisi_esit(t, TIP_TAM32);
    test_sonuc("ikili: 1 + 2 -> tam32", ok);
    arena_serbest(a);
}

static void test_ikili_arti_uyumsuz(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    /* tam32 + metin */
    TipBilgisi *t = ifade_tipi("1 + \"x\"", a, s, &hata);
    int ok = hata > 0 && tip_kategorisi_esit(t, TIP_HATA);
    test_sonuc("ikili: 1 + metin -> hata", ok);
    arena_serbest(a);
}

static void test_ikili_karsilastirma(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    TipBilgisi *t = ifade_tipi("x < 5", a, s, &hata);
    int ok = hata == 0 && tip_kategorisi_esit(t, TIP_MANTIKSAL);
    test_sonuc("ikili: x < 5 -> mantiksal", ok);
    arena_serbest(a);
}

static void test_ikili_karsilastirma_uyumsuz(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    /* mantiksal < tam */
    TipBilgisi *t = ifade_tipi("z < 5", a, s, &hata);
    int ok = hata > 0;
    test_sonuc("ikili: mantiksal < tam -> hata", ok);
    arena_serbest(a);
}

static void test_ikili_esitlik(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    TipBilgisi *t = ifade_tipi("x == y", a, s, &hata);
    int ok = hata == 0 && tip_kategorisi_esit(t, TIP_MANTIKSAL);
    test_sonuc("ikili: x == y -> mantiksal", ok);
    arena_serbest(a);
}

static void test_ikili_esitlik_uyumsuz(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    /* tam32 == metin */
    TipBilgisi *t = ifade_tipi("x == \"a\"", a, s, &hata);
    int ok = hata > 0;
    test_sonuc("ikili: tam32 == metin -> hata", ok);
    arena_serbest(a);
}

static void test_ikili_mantik(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    /* dogru ve yanlis */
    TipBilgisi *t = ifade_tipi("do\xc4\x9fru ve yanl\xc4\xb1\xc5\x9f",
                                a, s, &hata);
    int ok = hata == 0 && tip_kategorisi_esit(t, TIP_MANTIKSAL);
    test_sonuc("ikili: dogru ve yanlis -> mantiksal", ok);
    arena_serbest(a);
}

static void test_ikili_mantik_uyumsuz(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    /* dogru ve 5 */
    TipBilgisi *t = ifade_tipi("do\xc4\x9fru ve 5", a, s, &hata);
    int ok = hata > 0;
    test_sonuc("ikili: dogru ve 5 -> hata", ok);
    arena_serbest(a);
}

/* === Tekli === */

static void test_tekli_neg(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    TipBilgisi *t = ifade_tipi("-5", a, s, &hata);
    int ok = hata == 0 && tip_kategorisi_esit(t, TIP_TAM32);
    test_sonuc("tekli: -5 -> tam32", ok);
    arena_serbest(a);
}

static void test_tekli_neg_uyumsuz(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    /* -metin */
    TipBilgisi *t = ifade_tipi("-\"x\"", a, s, &hata);
    int ok = hata > 0;
    test_sonuc("tekli: -metin -> hata", ok);
    arena_serbest(a);
}

static void test_tekli_degil(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    /* degil dogru */
    TipBilgisi *t = ifade_tipi("de\xc4\x9fil do\xc4\x9fru", a, s, &hata);
    int ok = hata == 0 && tip_kategorisi_esit(t, TIP_MANTIKSAL);
    test_sonuc("tekli: degil dogru -> mantiksal", ok);
    arena_serbest(a);
}

static void test_tekli_degil_uyumsuz(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    /* degil 5 */
    TipBilgisi *t = ifade_tipi("de\xc4\x9fil 5", a, s, &hata);
    int ok = hata > 0;
    test_sonuc("tekli: degil 5 -> hata", ok);
    arena_serbest(a);
}

static void test_tekli_ref(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    TipBilgisi *t = ifade_tipi("&x", a, s, &hata);
    int ok = hata == 0 && tip_kategorisi_esit(t, TIP_REFERANS)
          && t->veri.referans.degisken_mi == 0
          && t->veri.referans.hedef->kategori == TIP_TAM32;
    test_sonuc("tekli: &x -> &tam32", ok);
    arena_serbest(a);
}

static void test_tekli_ref_degisken(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    /* &degisken x */
    TipBilgisi *t = ifade_tipi("&de\xc4\x9f""i\xc5\x9fken x", a, s, &hata);
    int ok = hata == 0 && tip_kategorisi_esit(t, TIP_REFERANS)
          && t->veri.referans.degisken_mi == 1;
    test_sonuc("tekli: &degisken x -> &degisken tam32", ok);
    arena_serbest(a);
}

static void test_tekli_deref(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    /* p32 = *tam32, *p32 -> tam32 */
    TipBilgisi *t = ifade_tipi("*p32", a, s, &hata);
    int ok = hata == 0 && tip_kategorisi_esit(t, TIP_TAM32);
    test_sonuc("tekli: *p32 -> tam32", ok);
    arena_serbest(a);
}

/* === Cagri === */

static void test_cagri_bos(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    /* f() -> tam32 */
    TipBilgisi *t = ifade_tipi("f()", a, s, &hata);
    int ok = hata == 0 && tip_kategorisi_esit(t, TIP_TAM32);
    test_sonuc("cagri: f() -> tam32", ok);
    arena_serbest(a);
}

static void test_cagri_args(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    /* g(5) -> mantiksal */
    TipBilgisi *t = ifade_tipi("g(5)", a, s, &hata);
    int ok = hata == 0 && tip_kategorisi_esit(t, TIP_MANTIKSAL);
    test_sonuc("cagri: g(5) -> mantiksal", ok);
    arena_serbest(a);
}

static void test_cagri_arg_uyumsuz(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    /* g("x") — metin yerine tam32 bekleniyor */
    TipBilgisi *t = ifade_tipi("g(\"a\")", a, s, &hata);
    int ok = hata > 0;
    test_sonuc("cagri: arg tipi uyumsuz -> hata", ok);
    arena_serbest(a);
    (void)t;
}

static void test_cagri_arg_sayi(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    /* f(5) — f()'in argumani yok */
    TipBilgisi *t = ifade_tipi("f(5)", a, s, &hata);
    int ok = hata > 0;
    test_sonuc("cagri: arg sayi uyumsuz -> hata", ok);
    arena_serbest(a);
    (void)t;
}

static void test_cagri_islev_degil(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    /* x() — x tam32, islev degil */
    TipBilgisi *t = ifade_tipi("x()", a, s, &hata);
    int ok = hata > 0;
    test_sonuc("cagri: islev olmayan -> hata", ok);
    arena_serbest(a);
    (void)t;
}

/* === Erisim (x.y) === */

static void test_erisim(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    TipBilgisi *t = ifade_tipi("h.ad", a, s, &hata);
    int ok = hata == 0 && tip_kategorisi_esit(t, TIP_METIN);
    test_sonuc("erisim: h.ad -> metin", ok);
    arena_serbest(a);
}

static void test_erisim_yas(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    TipBilgisi *t = ifade_tipi("h.yas", a, s, &hata);
    int ok = hata == 0 && tip_kategorisi_esit(t, TIP_TAM32);
    test_sonuc("erisim: h.yas -> tam32", ok);
    arena_serbest(a);
}

static void test_erisim_alan_yok(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    TipBilgisi *t = ifade_tipi("h.yok_alan", a, s, &hata);
    int ok = hata > 0;
    test_sonuc("erisim: h.yok_alan -> T009 hata", ok);
    arena_serbest(a);
    (void)t;
}

static void test_erisim_yapi_degil(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    /* x.y — x tam32, yapi degil */
    TipBilgisi *t = ifade_tipi("x.y", a, s, &hata);
    int ok = hata > 0;
    test_sonuc("erisim: tam32.y -> T007 hata", ok);
    arena_serbest(a);
    (void)t;
}

/* === Indeks (x[i]) === */

static void test_indeks(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    /* xs: Dizi<tam32>, xs[0] -> tam32 */
    TipBilgisi *t = ifade_tipi("xs[0]", a, s, &hata);
    int ok = hata == 0 && tip_kategorisi_esit(t, TIP_TAM32);
    test_sonuc("indeks: xs[0] -> tam32", ok);
    arena_serbest(a);
}

static void test_indeks_dizi_degil(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    /* x[0] — x tam32, dizi degil */
    TipBilgisi *t = ifade_tipi("x[0]", a, s, &hata);
    int ok = hata > 0;
    test_sonuc("indeks: tam32[0] -> T008 hata", ok);
    arena_serbest(a);
    (void)t;
}

static void test_indeks_idx_uyumsuz(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    /* xs["x"] — indeks tamsayi olmali */
    TipBilgisi *t = ifade_tipi("xs[\"x\"]", a, s, &hata);
    int ok = hata > 0;
    test_sonuc("indeks: xs[metin] -> T005 hata", ok);
    arena_serbest(a);
    (void)t;
}

/* === Yapi olusturma === */

static void test_yapi_olustur_tam(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    /* Hasta { ad: "X", yas: 30 } */
    TipBilgisi *t = ifade_tipi("Hasta { ad: \"X\", yas: 30 }", a, s, &hata);
    int ok = hata == 0 && tip_kategorisi_esit(t, TIP_YAPI);
    test_sonuc("yapi olustur: Hasta { ad, yas } -> Hasta", ok);
    arena_serbest(a);
}

static void test_yapi_olustur_alan_uyumsuz(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    /* ad: 5 (metin yerine tam) */
    TipBilgisi *t = ifade_tipi("Hasta { ad: 5, yas: 30 }", a, s, &hata);
    int ok = hata > 0;
    test_sonuc("yapi olustur: alan tipi uyumsuz -> hata", ok);
    arena_serbest(a);
    (void)t;
}

static void test_yapi_olustur_alan_eksik(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    /* yas eksik */
    TipBilgisi *t = ifade_tipi("Hasta { ad: \"X\" }", a, s, &hata);
    int ok = hata > 0;
    test_sonuc("yapi olustur: alan eksik -> T012 hata", ok);
    arena_serbest(a);
    (void)t;
}

static void test_yapi_olustur_alan_yok(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    /* yokAlan */
    TipBilgisi *t = ifade_tipi("Hasta { ad: \"X\", yas: 30, fazla: 1 }",
                                a, s, &hata);
    int ok = hata > 0;
    test_sonuc("yapi olustur: bilinmeyen alan -> T017 hata", ok);
    arena_serbest(a);
    (void)t;
}

/* === Dizi olusturma === */

static void test_dizi_olustur_homojen(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    TipBilgisi *t = ifade_tipi("[1, 2, 3]", a, s, &hata);
    int ok = hata == 0 && tip_kategorisi_esit(t, TIP_DIZI)
          && t->veri.dizi.eleman->kategori == TIP_TAM32;
    test_sonuc("dizi: [1, 2, 3] -> Dizi<tam32>", ok);
    arena_serbest(a);
}

static void test_dizi_olustur_uyumsuz(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    TipBilgisi *t = ifade_tipi("[1, \"x\"]", a, s, &hata);
    int ok = hata > 0;
    test_sonuc("dizi: [1, metin] -> T013 hata", ok);
    arena_serbest(a);
    (void)t;
}

static void test_dizi_olustur_bos(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    /* Bos dizi context lazim, ADIM 11.5'te. Su an T014 hata */
    TipBilgisi *t = ifade_tipi("[]", a, s, &hata);
    int ok = hata > 0;
    test_sonuc("dizi: [] -> T014 hata (context lazim)", ok);
    arena_serbest(a);
    (void)t;
}

/* === Yol === */

static void test_yol(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    /* matematik::PI -> kesirli64 */
    TipBilgisi *t = ifade_tipi("matematik::PI", a, s, &hata);
    int ok = hata == 0 && tip_kategorisi_esit(t, TIP_KESIRLI64);
    test_sonuc("yol: matematik::PI -> kesirli64", ok);
    arena_serbest(a);
}

static void test_yol_yok(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    /* matematik::yokuye */
    TipBilgisi *t = ifade_tipi("matematik::yokuye", a, s, &hata);
    int ok = hata > 0;
    test_sonuc("yol: bilinmeyen uye -> hata", ok);
    arena_serbest(a);
    (void)t;
}

/* === Karmasik kombinasyon === */

static void test_karmasik_iki_op(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    /* x + y * 2 */
    TipBilgisi *t = ifade_tipi("x + y * 2", a, s, &hata);
    int ok = hata == 0 && tip_kategorisi_esit(t, TIP_TAM32);
    test_sonuc("karmasik: x + y * 2 -> tam32", ok);
    arena_serbest(a);
}

static void test_karmasik_zincirleme(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    /* g(x + y) -> mantiksal */
    TipBilgisi *t = ifade_tipi("g(x + y)", a, s, &hata);
    int ok = hata == 0 && tip_kategorisi_esit(t, TIP_MANTIKSAL);
    test_sonuc("karmasik: g(x + y) -> mantiksal", ok);
    arena_serbest(a);
}

/* === Program-duzeyi testleri (ADIM 11.4) === */

/* Yardimci: tum programi tip kontrol et, hata sayisini dondur */
static int program_kontrol(const char *kaynak, Arena *a) {
    Lexer l;
    lexer_baslat(&l, kaynak, "test");
    Parser p;
    parser_baslat(&p, &l, a, "test", kaynak);
    Dugum *prog = parser_calistir(&p);
    if (!prog) return -1;

    Scope *g = scope_olustur(a, SCOPE_GLOBAL, NULL);
    TipKontrol tk;
    tip_kontrol_baslat(&tk, a, g, "test", kaynak);
    tip_kontrol_program(&tk, prog);
    return tk.hata_sayisi;
}

static void test_prog_bos(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol("", a);
    test_sonuc("program: bos -> 0 hata", h == 0);
    arena_serbest(a);
}

static void test_prog_islev_bos(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol("i\xc5\x9flev f() {}", a);
    test_sonuc("program: islev f() {} -> 0 hata", h == 0);
    arena_serbest(a);
}

static void test_prog_islev_donus_uyumlu(void) {
    Arena *a = arena_olustur(0);
    /* islev f() -> tam32 { ver 0; } */
    int h = program_kontrol("i\xc5\x9flev f() -> tam32 { ver 0; }", a);
    test_sonuc("program: islev donus uyumlu -> 0 hata", h == 0);
    arena_serbest(a);
}

static void test_prog_islev_donus_uyumsuz(void) {
    Arena *a = arena_olustur(0);
    /* islev f() -> tam32 { ver "x"; } */
    int h = program_kontrol(
        "i\xc5\x9flev f() -> tam32 { ver \"x\"; }", a);
    test_sonuc("program: islev donus uyumsuz -> hata", h > 0);
    arena_serbest(a);
}

static void test_prog_islev_ver_bos(void) {
    Arena *a = arena_olustur(0);
    /* islev f() { ver; } — donus tipi BOS */
    int h = program_kontrol("i\xc5\x9flev f() { ver; }", a);
    test_sonuc("program: ver; (donus BOS) -> 0 hata", h == 0);
    arena_serbest(a);
}

static void test_prog_islev_donus_var_ver_yok(void) {
    Arena *a = arena_olustur(0);
    /* islev f() -> tam32 { ver; } — uyumsuz */
    int h = program_kontrol("i\xc5\x9flev f() -> tam32 { ver; }", a);
    test_sonuc("program: ver; ama donus tipi var -> hata", h > 0);
    arena_serbest(a);
}

static void test_prog_degisken_uyumlu(void) {
    Arena *a = arena_olustur(0);
    /* islev f() { degisken x: tam32 = 5; } */
    int h = program_kontrol(
        "i\xc5\x9flev f() { de\xc4\x9f""i\xc5\x9fken x: tam32 = 5; }", a);
    test_sonuc("program: degisken tip annot uyumlu -> 0 hata", h == 0);
    arena_serbest(a);
}

static void test_prog_degisken_uyumsuz(void) {
    Arena *a = arena_olustur(0);
    /* islev f() { degisken x: tam32 = "X"; } */
    int h = program_kontrol(
        "i\xc5\x9flev f() { de\xc4\x9f""i\xc5\x9fken x: tam32 = \"X\"; }", a);
    test_sonuc("program: degisken tip uyumsuz -> hata", h > 0);
    arena_serbest(a);
}

static void test_prog_atama_uyumlu(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev f() { de\xc4\x9f""i\xc5\x9fken x: tam32 = 5; x = 10; }",
        a);
    test_sonuc("program: atama uyumlu -> 0 hata", h == 0);
    arena_serbest(a);
}

static void test_prog_atama_uyumsuz(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev f() { de\xc4\x9f""i\xc5\x9fken x: tam32 = 5; "
        "x = \"X\"; }", a);
    test_sonuc("program: atama tipi uyumsuz -> hata", h > 0);
    arena_serbest(a);
}

static void test_prog_atama_lvalue_yok(void) {
    Arena *a = arena_olustur(0);
    /* 5 = 10; — lvalue olmayan atama */
    int h = program_kontrol("i\xc5\x9flev f() { 5 = 10; }", a);
    test_sonuc("program: atama lvalue olmayan -> T022 hata", h > 0);
    arena_serbest(a);
}

static void test_prog_eger_mantiksal(void) {
    Arena *a = arena_olustur(0);
    /* islev f() { eger dogru {} } */
    int h = program_kontrol(
        "i\xc5\x9flev f() { e\xc4\x9f""er do\xc4\x9fru {} }", a);
    test_sonuc("program: eger dogru {} -> 0 hata", h == 0);
    arena_serbest(a);
}

static void test_prog_eger_uyumsuz(void) {
    Arena *a = arena_olustur(0);
    /* islev f() { eger 5 {} } */
    int h = program_kontrol("i\xc5\x9flev f() { e\xc4\x9f""er 5 {} }", a);
    test_sonuc("program: eger 5 {} -> T021 hata", h > 0);
    arena_serbest(a);
}

static void test_prog_iken_mantiksal(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev f() { iken do\xc4\x9fru {} }", a);
    test_sonuc("program: iken dogru {} -> 0 hata", h == 0);
    arena_serbest(a);
}

static void test_prog_iken_uyumsuz(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol("i\xc5\x9flev f() { iken 5 {} }", a);
    test_sonuc("program: iken 5 {} -> T021 hata", h > 0);
    arena_serbest(a);
}

static void test_prog_icin_dizi(void) {
    Arena *a = arena_olustur(0);
    /* icin i: xs (xs Dizi<tam32>) */
    int h = program_kontrol(
        "i\xc5\x9flev f() { "
        "de\xc4\x9f""i\xc5\x9fken xs: Dizi<tam32> = [1, 2, 3]; "
        "i\xc3\xa7in i: xs {} }", a);
    test_sonuc("program: icin Dizi<tam32> -> 0 hata", h == 0);
    arena_serbest(a);
}

static void test_prog_icin_dizi_degil(void) {
    Arena *a = arena_olustur(0);
    /* icin i: 5 — dizi degil */
    int h = program_kontrol(
        "i\xc5\x9flev f() { i\xc3\xa7in i: 5 {} }", a);
    test_sonuc("program: icin dizi degil -> T027 hata", h > 0);
    arena_serbest(a);
}

static void test_prog_yapi_basit(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "yap\xc4\xb1 X { y: tam32; }", a);
    test_sonuc("program: yapi X { y: tam32; } -> 0 hata", h == 0);
    arena_serbest(a);
}

static void test_prog_yapi_bilinmeyen_tip(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "yap\xc4\xb1 X { y: yokTipBu; }", a);
    test_sonuc("program: yapi alan bilinmeyen tip -> T011 hata", h > 0);
    arena_serbest(a);
}

static void test_prog_yapi_islev_kullanim(void) {
    Arena *a = arena_olustur(0);
    /* yapi Hasta { yas: tam32; }
     * islev yarat() -> Hasta { ver Hasta { yas: 30 }; } */
    int h = program_kontrol(
        "yap\xc4\xb1 Hasta { yas: tam32; } "
        "i\xc5\x9flev yarat() -> Hasta { ver Hasta { yas: 30 }; }", a);
    test_sonuc("program: yapi olusturma + ver -> 0 hata", h == 0);
    arena_serbest(a);
}

static void test_prog_cift_islev(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev f() {} i\xc5\x9flev f() {}", a);
    test_sonuc("program: cift islev tanimi -> T024 hata", h > 0);
    arena_serbest(a);
}

static void test_prog_cift_yapi(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "yap\xc4\xb1 X {} yap\xc4\xb1 X {}", a);
    test_sonuc("program: cift yapi tanimi -> T026 hata", h > 0);
    arena_serbest(a);
}

static void test_prog_sabit_uyumsuz(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol("sabit P: tam32 = \"x\";", a);
    test_sonuc("program: sabit tip uyumsuz -> hata", h > 0);
    arena_serbest(a);
}

static void test_prog_islev_recursive(void) {
    Arena *a = arena_olustur(0);
    /* islev f(n: tam32) -> tam32 { ver f(n); } — forward ref */
    int h = program_kontrol(
        "i\xc5\x9flev f(n: tam32) -> tam32 { ver f(n); }", a);
    test_sonuc("program: islev recursive -> 0 hata (forward ref)", h == 0);
    arena_serbest(a);
}

static void test_prog_blok_scope(void) {
    Arena *a = arena_olustur(0);
    /* Blok scope: degisken icinde gorunur, disarda yok */
    int h = program_kontrol(
        "i\xc5\x9flev f() { "
        "{ de\xc4\x9f""i\xc5\x9fken x: tam32 = 5; } "
        "}", a);
    test_sonuc("program: blok scope -> 0 hata", h == 0);
    arena_serbest(a);
}

static void test_prog_blok_scope_dis(void) {
    Arena *a = arena_olustur(0);
    /* Blok scope disinda x kullanim hata */
    int h = program_kontrol(
        "i\xc5\x9flev f() { "
        "{ de\xc4\x9f""i\xc5\x9fken x: tam32 = 5; } "
        "ver x; }", a);
    /* x bulunamamali (scope disinda) — T002 hata + ver donus tipi BOS uyumsuz */
    test_sonuc("program: blok dısı x erisim -> hata", h > 0);
    arena_serbest(a);
}

static void test_prog_eger_zincir(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev f(n: tam32) -> tam32 { "
        "e\xc4\x9f""er n < 0 { ver 0; } "
        "de\xc4\x9f""ilse e\xc4\x9f""er n == 0 { ver 1; } "
        "de\xc4\x9f""ilse { ver n; } }", a);
    test_sonuc("program: eger zincir + ver -> 0 hata", h == 0);
    arena_serbest(a);
}

static void test_prog_yapi_alan_kullanim(void) {
    Arena *a = arena_olustur(0);
    /* islev al(h: Hasta) -> tam32 { ver h.yas; } */
    int h = program_kontrol(
        "yap\xc4\xb1 Hasta { yas: tam32; } "
        "i\xc5\x9flev al(h: Hasta) -> tam32 { ver h.yas; }", a);
    test_sonuc("program: yapi.alan -> tip dogru -> 0 hata", h == 0);
    arena_serbest(a);
}

/* === Bidirectional cikarsama testleri (ADIM 11.5) === */

static void test_bd_lit_tam8(void) {
    Arena *a = arena_olustur(0);
    /* degisken x: tam8 = 1; — 1 -> tam8 (default tam32 yerine) */
    int h = program_kontrol(
        "i\xc5\x9flev f() { de\xc4\x9f""i\xc5\x9fken x: tam8 = 1; }", a);
    test_sonuc("bd: degisken tam8 = 1 -> 0 hata", h == 0);
    arena_serbest(a);
}

static void test_bd_lit_tam64(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev f() { de\xc4\x9f""i\xc5\x9fken x: tam64 = 1; }", a);
    test_sonuc("bd: degisken tam64 = 1 -> 0 hata", h == 0);
    arena_serbest(a);
}

static void test_bd_lit_kesirli32(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev f() { de\xc4\x9f""i\xc5\x9fken x: kesirli32 = 3.14; }", a);
    test_sonuc("bd: degisken kesirli32 = 3.14 -> 0 hata", h == 0);
    arena_serbest(a);
}

static void test_bd_dizi_bos_context(void) {
    Arena *a = arena_olustur(0);
    /* degisken xs: Dizi<tam32> = []; — bos dizi context'ten */
    int h = program_kontrol(
        "i\xc5\x9flev f() { "
        "de\xc4\x9f""i\xc5\x9fken xs: Dizi<tam32> = []; }", a);
    test_sonuc("bd: bos dizi Dizi<tam32> -> 0 hata", h == 0);
    arena_serbest(a);
}

static void test_bd_dizi_dolu_context(void) {
    Arena *a = arena_olustur(0);
    /* xs: Dizi<tam8> = [1, 2, 3]; — 1,2,3 -> tam8 */
    int h = program_kontrol(
        "i\xc5\x9flev f() { "
        "de\xc4\x9f""i\xc5\x9fken xs: Dizi<tam8> = [1, 2, 3]; }", a);
    test_sonuc("bd: dolu dizi Dizi<tam8> = [1,2,3] -> 0 hata", h == 0);
    arena_serbest(a);
}

static void test_bd_cagri_arg(void) {
    Arena *a = arena_olustur(0);
    /* islev f(n: tam8) {} f(5) — 5 -> tam8 */
    int h = program_kontrol(
        "i\xc5\x9flev f(n: tam8) {} "
        "i\xc5\x9flev g() { f(5); }", a);
    test_sonuc("bd: cagri arg context'ten -> 0 hata", h == 0);
    arena_serbest(a);
}

static void test_bd_atama(void) {
    Arena *a = arena_olustur(0);
    /* x tam8, x = 7 — 7 -> tam8 */
    int h = program_kontrol(
        "i\xc5\x9flev f() { "
        "de\xc4\x9f""i\xc5\x9fken x: tam8 = 0; x = 7; }", a);
    test_sonuc("bd: atama context'ten -> 0 hata", h == 0);
    arena_serbest(a);
}

static void test_bd_ver(void) {
    Arena *a = arena_olustur(0);
    /* ver 5 donus tam8 -> 0 hata */
    int h = program_kontrol(
        "i\xc5\x9flev f() -> tam8 { ver 5; }", a);
    test_sonuc("bd: ver donus tipi context'inden -> 0 hata", h == 0);
    arena_serbest(a);
}

static void test_bd_yapi_olustur(void) {
    Arena *a = arena_olustur(0);
    /* Hasta { yas: 5 } yas tam8 -> 5 tam8 */
    int h = program_kontrol(
        "yap\xc4\xb1 Hasta { yas: tam8; } "
        "i\xc5\x9flev f() -> Hasta { ver Hasta { yas: 5 }; }", a);
    test_sonuc("bd: yapi olustur alan context'inden -> 0 hata", h == 0);
    arena_serbest(a);
}

static void test_bd_zincir(void) {
    Arena *a = arena_olustur(0);
    /* f(g(5)) — g param tam8, 5 -> tam8 (zincir) */
    int h = program_kontrol(
        "i\xc5\x9flev g(n: tam8) -> tam32 { ver 0; } "
        "i\xc5\x9flev f(n: tam32) {} "
        "i\xc5\x9flev h() { f(g(5)); }", a);
    test_sonuc("bd: zincir f(g(5)) -> 0 hata", h == 0);
    arena_serbest(a);
}

/* === Generic instantiation testleri (ADIM 11.6) === */

static void test_gn_kutu_basit(void) {
    Arena *a = arena_olustur(0);
    /* yapi Kutu<T> { eleman: T; }
     * islev f() -> Kutu<tam32> { ver Kutu { eleman: 5 }; } */
    int h = program_kontrol(
        "yap\xc4\xb1 Kutu<T> { eleman: T; } "
        "i\xc5\x9flev f() -> Kutu<tam32> { ver Kutu { eleman: 5 }; }", a);
    test_sonuc("gn: Kutu<tam32> + ver Kutu { eleman: 5 } -> 0 hata", h == 0);
    arena_serbest(a);
}

static void test_gn_kutu_alan_uyumsuz(void) {
    Arena *a = arena_olustur(0);
    /* Kutu<tam32> { eleman: "x" } — eleman tam32 beklenir, metin verildi */
    int h = program_kontrol(
        "yap\xc4\xb1 Kutu<T> { eleman: T; } "
        "i\xc5\x9flev f() -> Kutu<tam32> { ver Kutu { eleman: \"x\" }; }", a);
    test_sonuc("gn: Kutu<tam32> alan metin -> hata", h > 0);
    arena_serbest(a);
}

static void test_gn_kutu_erisim(void) {
    Arena *a = arena_olustur(0);
    /* k.eleman -> tam32 (substitusyon) */
    int h = program_kontrol(
        "yap\xc4\xb1 Kutu<T> { eleman: T; } "
        "i\xc5\x9flev al(k: Kutu<tam32>) -> tam32 { ver k.eleman; }", a);
    test_sonuc("gn: k.eleman (Kutu<tam32>) -> tam32", h == 0);
    arena_serbest(a);
}

static void test_gn_iki_param(void) {
    Arena *a = arena_olustur(0);
    /* yapi Cift<A,B> { ilk: A; ikinci: B; } */
    int h = program_kontrol(
        "yap\xc4\xb1 Cift<A, B> { ilk: A; ikinci: B; } "
        "i\xc5\x9flev f() -> Cift<tam32, metin> "
        "{ ver Cift { ilk: 1, ikinci: \"x\" }; }", a);
    test_sonuc("gn: Cift<tam32, metin> ilk + ikinci -> 0 hata", h == 0);
    arena_serbest(a);
}

static void test_gn_iki_param_erisim(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "yap\xc4\xb1 Cift<A, B> { ilk: A; ikinci: B; } "
        "i\xc5\x9flev getMetin(c: Cift<tam32, metin>) -> metin "
        "{ ver c.ikinci; }", a);
    test_sonuc("gn: c.ikinci (Cift<tam32, metin>) -> metin", h == 0);
    arena_serbest(a);
}

static void test_gn_ic_ice(void) {
    Arena *a = arena_olustur(0);
    /* Kutu<Kutu<tam32>> ic ice generic */
    int h = program_kontrol(
        "yap\xc4\xb1 Kutu<T> { eleman: T; } "
        "i\xc5\x9flev al(k: Kutu<Kutu<tam32>>) -> Kutu<tam32> "
        "{ ver k.eleman; }", a);
    test_sonuc("gn: ic ice Kutu<Kutu<tam32>> -> 0 hata", h == 0);
    arena_serbest(a);
}

/* === boyut<T> (sizeof) === */

static void test_boyut_basit(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    /* boyut<tam32> -> dtam64 */
    TipBilgisi *t = ifade_tipi("boyut<tam32>", a, s, &hata);
    int ok = hata == 0 && tip_kategorisi_esit(t, TIP_DTAM64);
    test_sonuc("boyut: boyut<tam32> -> dtam64", ok);
    arena_serbest(a);
}

static void test_boyut_pointer(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    /* boyut<*tam32> -> dtam64 (kontrol kategorisi) */
    TipBilgisi *t = ifade_tipi("boyut<*tam32>", a, s, &hata);
    int ok = hata == 0 && tip_kategorisi_esit(t, TIP_DTAM64);
    test_sonuc("boyut: boyut<*tam32> -> dtam64", ok);
    arena_serbest(a);
}

static void test_boyut_bilinmeyen_tip(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    /* boyut<Bilinmeyen> -> hata */
    TipBilgisi *t = ifade_tipi("boyut<Bilinmeyen>", a, s, &hata);
    int ok = hata > 0 && tip_kategorisi_esit(t, TIP_HATA);
    test_sonuc("boyut: boyut<Bilinmeyen> -> hata", ok);
    arena_serbest(a);
}

static void test_boyut_aritmetik(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    /* boyut<tam32> + boyut<tam8> -> dtam64 (ayni tip oldugu icin OK) */
    TipBilgisi *t = ifade_tipi("boyut<tam32> + boyut<tam8>", a, s, &hata);
    int ok = hata == 0 && tip_kategorisi_esit(t, TIP_DTAM64);
    test_sonuc("boyut: dtam64 + dtam64 -> dtam64", ok);
    arena_serbest(a);
}

/* === Bit operatorleri === */

static void test_bit_ve(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    TipBilgisi *t = ifade_tipi("x & y", a, s, &hata);
    int ok = hata == 0 && tip_kategorisi_esit(t, TIP_TAM32);
    test_sonuc("bit: x & y -> tam32", ok);
    arena_serbest(a);
}

static void test_bit_veya(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    TipBilgisi *t = ifade_tipi("x | y", a, s, &hata);
    int ok = hata == 0 && tip_kategorisi_esit(t, TIP_TAM32);
    test_sonuc("bit: x | y -> tam32", ok);
    arena_serbest(a);
}

static void test_bit_xor(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    TipBilgisi *t = ifade_tipi("x ^ y", a, s, &hata);
    int ok = hata == 0 && tip_kategorisi_esit(t, TIP_TAM32);
    test_sonuc("bit: x ^ y -> tam32", ok);
    arena_serbest(a);
}

static void test_bit_kaydir(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    TipBilgisi *t = ifade_tipi("x << 4", a, s, &hata);
    int ok = hata == 0 && tip_kategorisi_esit(t, TIP_TAM32);
    test_sonuc("bit: x << 4 -> tam32 (sol tip)", ok);
    arena_serbest(a);
}

static void test_bit_not(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    TipBilgisi *t = ifade_tipi("~x", a, s, &hata);
    int ok = hata == 0 && tip_kategorisi_esit(t, TIP_TAM32);
    test_sonuc("bit: ~x -> tam32", ok);
    arena_serbest(a);
}

static void test_bit_uyumsuz(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    /* z mantiksal — bit op'a girmesin */
    TipBilgisi *t = ifade_tipi("z & x", a, s, &hata);
    int ok = hata > 0 && tip_kategorisi_esit(t, TIP_HATA);
    test_sonuc("bit: mantiksal & tamsayi -> hata", ok);
    arena_serbest(a);
}

/* === Pointer aritmetigi === */

static void test_ptr_arti_int(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    /* p32 (*tam32) + tamsayi -> *tam32 */
    TipBilgisi *t = ifade_tipi("p32 + 1", a, s, &hata);
    int ok = hata == 0 && tip_kategorisi_esit(t, TIP_POINTER);
    test_sonuc("ptr: *T + int -> *T", ok);
    arena_serbest(a);
}

static void test_ptr_eksi_int(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    /* p32 (*tam32) - tamsayi -> *tam32 */
    TipBilgisi *t = ifade_tipi("p32 - 4", a, s, &hata);
    int ok = hata == 0 && tip_kategorisi_esit(t, TIP_POINTER);
    test_sonuc("ptr: *T - int -> *T", ok);
    arena_serbest(a);
}

static void test_int_arti_ptr(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    /* int + *T -> *T (komutatif) */
    TipBilgisi *t = ifade_tipi("1 + p32", a, s, &hata);
    int ok = hata == 0 && tip_kategorisi_esit(t, TIP_POINTER);
    test_sonuc("ptr: int + *T -> *T", ok);
    arena_serbest(a);
}

static void test_ptr_eksi_ptr(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    /* p32 - p32 -> tam64 (pointer difference) */
    TipBilgisi *t = ifade_tipi("p32 - p32", a, s, &hata);
    int ok = hata == 0 && tip_kategorisi_esit(t, TIP_TAM64);
    test_sonuc("ptr: *T - *T -> tam64", ok);
    arena_serbest(a);
}

/* === Main === */

int main(void) {
    /* Hata mesajlarini sustur (test ciktisi temiz) */
    if (!freopen("NUL", "w", stderr)) {
        /* fallback sessiz */
    }

    printf("KEMGU Tip Kontrolu Test Paketi (ADIM 11.3)\n");
    printf("===========================================\n");

    printf("\n--- Literaller ---\n");
    test_lit_tam();
    test_lit_kesirli();
    test_lit_metin();
    test_lit_karakter();
    test_lit_dogru();
    test_lit_bos();

    printf("\n--- Tanimlayici ---\n");
    test_id_var();
    test_id_yok();

    printf("\n--- Ikili ---\n");
    test_ikili_arti();
    test_ikili_arti_uyumsuz();
    test_ikili_karsilastirma();
    test_ikili_karsilastirma_uyumsuz();
    test_ikili_esitlik();
    test_ikili_esitlik_uyumsuz();
    test_ikili_mantik();
    test_ikili_mantik_uyumsuz();

    printf("\n--- Tekli ---\n");
    test_tekli_neg();
    test_tekli_neg_uyumsuz();
    test_tekli_degil();
    test_tekli_degil_uyumsuz();
    test_tekli_ref();
    test_tekli_ref_degisken();
    test_tekli_deref();

    printf("\n--- Cagri ---\n");
    test_cagri_bos();
    test_cagri_args();
    test_cagri_arg_uyumsuz();
    test_cagri_arg_sayi();
    test_cagri_islev_degil();

    printf("\n--- Erisim ---\n");
    test_erisim();
    test_erisim_yas();
    test_erisim_alan_yok();
    test_erisim_yapi_degil();

    printf("\n--- Indeks ---\n");
    test_indeks();
    test_indeks_dizi_degil();
    test_indeks_idx_uyumsuz();

    printf("\n--- Yapi Olusturma ---\n");
    test_yapi_olustur_tam();
    test_yapi_olustur_alan_uyumsuz();
    test_yapi_olustur_alan_eksik();
    test_yapi_olustur_alan_yok();

    printf("\n--- Dizi Olusturma ---\n");
    test_dizi_olustur_homojen();
    test_dizi_olustur_uyumsuz();
    test_dizi_olustur_bos();

    printf("\n--- Yol ---\n");
    test_yol();
    test_yol_yok();

    printf("\n--- Karmasik ---\n");
    test_karmasik_iki_op();
    test_karmasik_zincirleme();

    printf("\n--- Program: Islev/Donus ---\n");
    test_prog_bos();
    test_prog_islev_bos();
    test_prog_islev_donus_uyumlu();
    test_prog_islev_donus_uyumsuz();
    test_prog_islev_ver_bos();
    test_prog_islev_donus_var_ver_yok();
    test_prog_islev_recursive();

    printf("\n--- Program: Degisken/Atama ---\n");
    test_prog_degisken_uyumlu();
    test_prog_degisken_uyumsuz();
    test_prog_atama_uyumlu();
    test_prog_atama_uyumsuz();
    test_prog_atama_lvalue_yok();

    printf("\n--- Program: Kontrol Akisi ---\n");
    test_prog_eger_mantiksal();
    test_prog_eger_uyumsuz();
    test_prog_iken_mantiksal();
    test_prog_iken_uyumsuz();
    test_prog_icin_dizi();
    test_prog_icin_dizi_degil();
    test_prog_eger_zincir();

    printf("\n--- Program: Yapi/Sabit ---\n");
    test_prog_yapi_basit();
    test_prog_yapi_bilinmeyen_tip();
    test_prog_yapi_islev_kullanim();
    test_prog_yapi_alan_kullanim();
    test_prog_sabit_uyumsuz();

    printf("\n--- Program: Cift Tanim ---\n");
    test_prog_cift_islev();
    test_prog_cift_yapi();

    printf("\n--- Program: Scope ---\n");
    test_prog_blok_scope();
    test_prog_blok_scope_dis();

    printf("\n--- Bidirectional Cikarsama (11.5) ---\n");
    test_bd_lit_tam8();
    test_bd_lit_tam64();
    test_bd_lit_kesirli32();
    test_bd_dizi_bos_context();
    test_bd_dizi_dolu_context();
    test_bd_cagri_arg();
    test_bd_atama();
    test_bd_ver();
    test_bd_yapi_olustur();
    test_bd_zincir();

    printf("\n--- Generic Instantiation (11.6) ---\n");
    test_gn_kutu_basit();
    test_gn_kutu_alan_uyumsuz();
    test_gn_kutu_erisim();
    test_gn_iki_param();
    test_gn_iki_param_erisim();
    test_gn_ic_ice();

    printf("\n--- boyut<T> (sizeof) ---\n");
    test_boyut_basit();
    test_boyut_pointer();
    test_boyut_bilinmeyen_tip();
    test_boyut_aritmetik();

    printf("\n--- Bit Operatorleri ---\n");
    test_bit_ve();
    test_bit_veya();
    test_bit_xor();
    test_bit_kaydir();
    test_bit_not();
    test_bit_uyumsuz();

    printf("\n--- Pointer Aritmetigi ---\n");
    test_ptr_arti_int();
    test_ptr_eksi_int();
    test_int_arti_ptr();
    test_ptr_eksi_ptr();

    printf("\n===========================================\n");
    printf("Toplam: %d | Basarili: %d | Basarisiz: %d\n",
           toplam_test, basarili, basarisiz);
    return basarisiz > 0 ? 1 : 0;
}
