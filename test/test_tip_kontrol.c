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
    /* C5 on-kosul #2: ciplak ifade baglami guvensiz DISI sayilir ->
     * *p32 artik G001 ile reddedilir (eskiden sessizce gecerdi). */
    TipBilgisi *t = ifade_tipi("*p32", a, s, &hata);
    int ok = hata == 1 && tip_kategorisi_esit(t, TIP_HATA);
    test_sonuc("tekli: *p32 guvensiz disinda -> G001 reddi", ok);
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
     * islev olustur() -> Hasta { ver Hasta { yas: 30 }; } */
    int h = program_kontrol(
        "yap\xc4\xb1 Hasta { yas: tam32; } "
        "i\xc5\x9flev olustur() -> Hasta { ver Hasta { yas: 30 }; }", a);
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

/* === Constraint satisfaction (ADIM 15.5) === */

/* CS-1: Bound karsilanmiyor -> T030 */
static void test_cs_bound_violation(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "\xc3\xb6zellik Say {} "
        "yap\xc4\xb1 Tam { x: tam32; } "
        "yap\xc4\xb1 Vektor<T: Say> { ic: T; } "
        "i\xc5\x9flev f() -> Vektor<Tam> "
        "{ ver Vektor { ic: Tam { x: 0 } }; }", a);
    test_sonuc("cs: bound karsilanmiyor (uygula yok) -> T030 hata", h > 0);
    arena_serbest(a);
}

/* CS-2: Bound karsilaniyor (uygula var) -> OK */
static void test_cs_bound_satisfied(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "\xc3\xb6zellik Say {} "
        "yap\xc4\xb1 Tam { x: tam32; } "
        "uygula Say i\xc3\xa7in Tam {} "
        "yap\xc4\xb1 Vektor<T: Say> { ic: T; } "
        "i\xc5\x9flev f() -> Vektor<Tam> "
        "{ ver Vektor { ic: Tam { x: 0 } }; }", a);
    test_sonuc("cs: bound karsilaniyor (uygula var) -> 0 hata", h == 0);
    arena_serbest(a);
}

/* CS-3: Bound olmadan eski davranis korunur */
static void test_cs_bound_yok(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "yap\xc4\xb1 Tam { x: tam32; } "
        "yap\xc4\xb1 Vektor<T> { ic: T; } "
        "i\xc5\x9flev f() -> Vektor<Tam> "
        "{ ver Vektor { ic: Tam { x: 0 } }; }", a);
    test_sonuc("cs: bound yok -> 0 hata (eski davranis)", h == 0);
    arena_serbest(a);
}

/* CS-4: Coklu bound, hepsi karsilaniyor */
static void test_cs_coklu_bound_ok(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "\xc3\xb6zellik A {} \xc3\xb6zellik B {} "
        "yap\xc4\xb1 X { v: tam32; } "
        "uygula A i\xc3\xa7in X {} "
        "uygula B i\xc3\xa7in X {} "
        "yap\xc4\xb1 K<T: A + B> { ic: T; } "
        "i\xc5\x9flev f() -> K<X> { ver K { ic: X { v: 0 } }; }", a);
    test_sonuc("cs: coklu bound hepsi karsilaniyor -> 0 hata", h == 0);
    arena_serbest(a);
}

/* CS-5: Coklu bound, bir tanesi karsilanmiyor */
static void test_cs_coklu_bound_eksik(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "\xc3\xb6zellik A {} \xc3\xb6zellik B {} "
        "yap\xc4\xb1 X { v: tam32; } "
        "uygula A i\xc3\xa7in X {} "
        /* B icin uygula yok */
        "yap\xc4\xb1 K<T: A + B> { ic: T; } "
        "i\xc5\x9flev f() -> K<X> { ver K { ic: X { v: 0 } }; }", a);
    test_sonuc("cs: B icin uygula yok -> hata", h > 0);
    arena_serbest(a);
}

/* CS-6: Bilinmeyen ozellik bound olarak -> T031 */
static void test_cs_bound_bilinmeyen(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "yap\xc4\xb1 X { v: tam32; } "
        "yap\xc4\xb1 K<T: BilinmeyenOzellik> { ic: T; } "
        "i\xc5\x9flev f() -> K<X> { ver K { ic: X { v: 0 } }; }", a);
    test_sonuc("cs: bilinmeyen ozellik bound -> T031 hata", h > 0);
    arena_serbest(a);
}

/* CS-8: uygula gövdesinde tip hatasi yakalanir */
static void test_cs_uygula_govde_hata(void) {
    Arena *a = arena_olustur(0);
    /* uygula icinde tam32 dondurmesi gerek ama metin veriyor */
    int h = program_kontrol(
        "yap\xc4\xb1 K { v: tam32; } "
        "uygula K { "
        "i\xc5\x9flev m() -> tam32 { ver \"hata\"; } }",
        a);
    test_sonuc("cs: uygula govde tip hatasi -> hata", h > 0);
    arena_serbest(a);
}

/* CS-9: uygula gövdesi temiz kod -> hata yok */
static void test_cs_uygula_govde_temiz(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "yap\xc4\xb1 K { v: tam32; } "
        "uygula K { "
        "i\xc5\x9flev m() -> tam32 { ver 42; } }",
        a);
    test_sonuc("cs: uygula govde temiz -> 0 hata", h == 0);
    arena_serbest(a);
}

/* CS-10: method dispatch (x.method()) calisir */
static void test_cs_method_dispatch(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "yap\xc4\xb1 K { v: tam32; } "
        "uygula K { i\xc5\x9flev say() -> tam32 { ver 42; } } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken k = K { v: 0 }; "
        "ver k.say(); }",
        a);
    test_sonuc("cs: method dispatch (k.say()) -> 0 hata", h == 0);
    arena_serbest(a);
}

/* CS-11: method arg sayisi uyumsuzlugu */
static void test_cs_method_arg_sayi(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "yap\xc4\xb1 K { v: tam32; } "
        "uygula K { i\xc5\x9flev al(x: tam32) -> tam32 { ver x; } } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken k = K { v: 0 }; "
        "ver k.al(); }",
        a);
    /* arg yok, 1 bekleniyor */
    test_sonuc("cs: method arg sayisi yanlis -> hata", h > 0);
    arena_serbest(a);
}

/* CS-12: Trait method (impl Trait icin K) bulunur */
static void test_cs_trait_method(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "\xc3\xb6zellik Say { i\xc5\x9flev cevir() -> tam32; } "
        "yap\xc4\xb1 K { v: tam32; } "
        "uygula Say i\xc3\xa7in K { "
        "i\xc5\x9flev cevir() -> tam32 { ver 42; } } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken k = K { v: 0 }; "
        "ver k.cevir(); }",
        a);
    test_sonuc("cs: trait method (k.cevir()) -> 0 hata", h == 0);
    arena_serbest(a);
}

/* P-1: hiç ifadesi seçimlik<T> olarak tip alir */
static void test_hic_secimlik(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev f() -> se\xc3\xa7" "imlik<tam32> { ver hi\xc3\xa7; }",
        a);
    test_sonuc("hiç -> seçimlik<T> tip alir", h == 0);
    arena_serbest(a);
}

/* P-2: değer(x) seçimlik<T> konstrüktör */
static void test_deger_konstrüktör(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev f() -> se\xc3\xa7" "imlik<tam32> "
        "{ ver de\xc4\x9f" "er(42); }",
        a);
    test_sonuc("değer(42) -> seçimlik<tam32>", h == 0);
    arena_serbest(a);
}

/* P-3: pattern binding eşleş içinde değer(v) ile v bind */
static void test_pattern_binding(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev al(s: se\xc3\xa7" "imlik<tam32>) -> tam32 { "
        "e\xc5\x9fle\xc5\x9f s { "
        "  de\xc4\x9f" "er(v) => { ver v; } "
        "  hi\xc3\xa7 => { ver 0; } "
        "} "
        "ver 0; }",
        a);
    test_sonuc("pattern binding (değer(v) => v) -> 0 hata", h == 0);
    arena_serbest(a);
}

/* P-4b: sonuc pattern matching — tamam(v) bind */
static void test_pattern_sonuc_tamam_bind(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev al(s: sonu\xc3\xa7<tam32, metin>) -> tam32 { "
        "e\xc5\x9fle\xc5\x9f s { "
        "  tamam(v) => { ver v; } "
        "  hata(m) => { ver 0; } "
        "} "
        "ver 0; }",
        a);
    test_sonuc("pattern sonuc: tamam(v)/hata(m) bind", h == 0);
    arena_serbest(a);
}

/* P-4c: sonuc pattern — hata icinden metin v bind */
static void test_pattern_sonuc_hata_bind(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev al(s: sonu\xc3\xa7<tam32, metin>) -> metin { "
        "e\xc5\x9fle\xc5\x9f s { "
        "  hata(m) => { ver m; } "
        "  tamam(v) => { ver \"\"; } "
        "} "
        "ver \"\"; }",
        a);
    test_sonuc("pattern sonuc: hata(m) -> m metin bind", h == 0);
    arena_serbest(a);
}

/* P-4d: tamam yapici alti tanimlayici desen */
static void test_pattern_sonuc_jokerli(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev al(s: sonu\xc3\xa7<tam32, metin>) -> tam32 { "
        "e\xc5\x9fle\xc5\x9f s { "
        "  tamam(v) => { ver v; } "
        "  _ => { ver 0; } "
        "} "
        "ver 0; }",
        a);
    test_sonuc("pattern sonuc: tamam(v) + _", h == 0);
    arena_serbest(a);
}

/* P-4: tamam(v), hata(e) sonuç konstrüktörleri */
static void test_sonuc_konstrüktörler(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev f() -> sonu\xc3\xa7<tam32, metin> "
        "{ ver tamam(42); }",
        a);
    test_sonuc("tamam(42) -> sonuç<tam32, metin>", h == 0);
    arena_serbest(a);
}

/* C-1: sonuc<T,E> uzerinde tamam(v) ve hata(m) pattern matching */
static void test_sonuc_pattern_matching(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev cozumle(r: sonu\xc3\xa7<tam32, metin>) -> tam32 { "
        "e\xc5\x9fle\xc5\x9f r { "
        "  tamam(v) => { ver v; } "
        "  hata(m) => { ver 0; } "
        "} "
        "ver 0; }",
        a);
    test_sonuc("sonuç pattern (tamam(v)+hata(m)) -> 0 hata", h == 0);
    arena_serbest(a);
}

/* C-2: tamam(v) — v gercekten T tipinde mi (binding gecerli) */
static void test_sonuc_tamam_binding(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev iki_kat_basari(r: sonu\xc3\xa7<tam32, metin>) -> tam32 { "
        "e\xc5\x9fle\xc5\x9f r { "
        "  tamam(v) => { ver v * 2; } "
        "  hata(m) => { ver 0; } "
        "} "
        "ver 0; }",
        a);
    test_sonuc("sonuç tamam(v) binding v: tam32", h == 0);
    arena_serbest(a);
}

/* C-3: hata(m) — m gercekten E tipinde mi */
static void test_sonuc_hata_binding(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev mesaj_uzunluk(r: sonu\xc3\xa7<tam32, metin>) -> metin { "
        "e\xc5\x9fle\xc5\x9f r { "
        "  tamam(v) => { ver \"yok\"; } "
        "  hata(m) => { ver m; } "
        "} "
        "ver \"\"; }",
        a);
    test_sonuc("sonuç hata(m) binding m: metin", h == 0);
    arena_serbest(a);
}

/* C-4: joker (_) alt-desen — tamam(_) kabul edilir */
static void test_sonuc_pattern_joker(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev basari_mi(r: sonu\xc3\xa7<tam32, metin>) -> mant\xc4\xb1ksal { "
        "e\xc5\x9fle\xc5\x9f r { "
        "  tamam(_) => { ver do\xc4\x9fru; } "
        "  hata(_) => { ver yanl\xc4\xb1\xc5\x9f; } "
        "} "
        "ver yanl\xc4\xb1\xc5\x9f; }",
        a);
    test_sonuc("sonuç tamam(_) / hata(_) joker alt-desen", h == 0);
    arena_serbest(a);
}

/* C-5: yanlis tip binding — tamam(v)'de v'yi yanlis tip operasyonda kullan */
static void test_sonuc_pattern_yanlis_tip(void) {
    Arena *a = arena_olustur(0);
    /* tamam(v) -> v tam32; metin operasyonu T001/T003 verir */
    int h = program_kontrol(
        "i\xc5\x9flev kotu(r: sonu\xc3\xa7<tam32, metin>) -> metin { "
        "e\xc5\x9fle\xc5\x9f r { "
        "  tamam(v) => { ver v; } "    /* v tam32, metin bekleniyor — T020 */
        "  hata(m) => { ver m; } "
        "} "
        "ver \"\"; }",
        a);
    test_sonuc("sonuç tamam(v) yanlis tip (v tam32, dönüş metin) -> hata", h > 0);
    arena_serbest(a);
}

/* Madde B: dizi_olustur / dizi_ekle / dizi_al tip kontrolu */
static void test_dizi_olustur_temel(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken d: Dizi<tam32> = dizi_olustur(8); "
        "ver 0; }",
        a);
    test_sonuc("dizi_olustur(8) -> Dizi<tam32>", h == 0);
    arena_serbest(a);
}

static void test_dizi_ekle_tip_uyumlu(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken d: Dizi<tam32> = dizi_olustur(4); "
        "dizi_ekle(d, 42); "
        "ver 0; }",
        a);
    test_sonuc("dizi_ekle(d, 42) tam32 uyumlu", h == 0);
    arena_serbest(a);
}

static void test_dizi_ekle_tip_uyumsuz(void) {
    Arena *a = arena_olustur(0);
    /* metin literal -> Dizi<tam32>.ekle hata vermeli */
    int h = program_kontrol(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken d: Dizi<tam32> = dizi_olustur(4); "
        "dizi_ekle(d, \"hata\"); "
        "ver 0; }",
        a);
    test_sonuc("dizi_ekle metin -> tam32 dizi: hata", h > 0);
    arena_serbest(a);
}

static void test_dizi_al_donus(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken d: Dizi<tam32> = dizi_olustur(4); "
        "dizi_ekle(d, 42); "
        "ver dizi_al(d, 0); }",
        a);
    test_sonuc("dizi_al donus tam32", h == 0);
    arena_serbest(a);
}

static void test_dizi_olustur_tam64(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken d: Dizi<tam64> = dizi_olustur(4); "
        "dizi_ekle(d, 100); "
        "ver 0; }",
        a);
    test_sonuc("dizi_olustur context Dizi<tam64> instan", h == 0);
    arena_serbest(a);
}

/* === Madde E: Tip donusturme (olarak) === */

static void test_olarak_tam_tam(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken x: tam32 = 42; "
        "de\xc4\x9fi\xc5\x9fken y: tam64 = x olarak tam64; "
        "ver 0; }",
        a);
    test_sonuc("x: tam32 olarak tam64 — OK", h == 0);
    arena_serbest(a);
}

static void test_olarak_tam_kesirli(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken x: tam32 = 42; "
        "de\xc4\x9fi\xc5\x9fken f: kesirli64 = x olarak kesirli64; "
        "ver 0; }",
        a);
    test_sonuc("tam32 olarak kesirli64 — OK", h == 0);
    arena_serbest(a);
}

static void test_olarak_kesirli_tam(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken f: kesirli64 = 3.14; "
        "ver f olarak tam32; }",
        a);
    test_sonuc("kesirli64 olarak tam32 — OK", h == 0);
    arena_serbest(a);
}

static void test_olarak_metin_yasak(void) {
    Arena *a = arena_olustur(0);
    /* metin -> tam32 yasak (kaynak sayisal degil) */
    int h = program_kontrol(
        "i\xc5\x9flev main() -> tam32 { "
        "ver \"hi\" olarak tam32; }",
        a);
    test_sonuc("metin olarak tam32 -> hata (E002)", h > 0);
    arena_serbest(a);
}

static void test_olarak_karakter_tam(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken c: karakter = 'A'; "
        "ver c olarak tam32; }",
        a);
    test_sonuc("karakter olarak tam32 — OK", h == 0);
    arena_serbest(a);
}

/* === Adim 4: Madde E v2 cast garantileri (E001-E004) === */

/* E001: x olarak tekkez<T> derleme hatasi (Linear creation by cast forbidden) */
static void test_E001_tekkez_hedef_yasak(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken x: tam32 = 42; "
        "de\xc4\x9fi\xc5\x9fken t = x olarak tekkez<tam32>; "
        "ver 0; }",
        a);
    test_sonuc("E001: x olarak tekkez<T> -> hata", h > 0);
    arena_serbest(a);
}

static void test_E001_tekkez_hedef_pozitif(void) {
    /* tekkez<T> hedef ile cast'i denerse, hata SAYISI artmali */
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev main() -> tam32 { "
        "ver 1 olarak tekkez<tam32>; }",
        a);
    test_sonuc("E001 (2): 1 olarak tekkez<T> -> hata", h > 0);
    arena_serbest(a);
}

/* E002: metin olarak tam derleme hatasi */
static void test_E002_metin_to_tam(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev main() -> tam32 { "
        "ver \"42\" olarak tam32; }",
        a);
    test_sonuc("E002: \"42\" olarak tam32 -> hata", h > 0);
    arena_serbest(a);
}

static void test_E002_tam_to_metin(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken s: metin = 42 olarak metin; "
        "ver 0; }",
        a);
    test_sonuc("E002 (2): 42 olarak metin -> hata", h > 0);
    arena_serbest(a);
}

/* E003: tekkez<T> olarak T derleme hatasi (linear escape) */
static void test_E003_tekkez_escape(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken t = tekkez_olustur(42); "
        "de\xc4\x9fi\xc5\x9fken x: tam32 = t olarak tam32; "
        "ver 0; }",
        a);
    test_sonuc("E003: tekkez<tam32> olarak tam32 -> hata", h > 0);
    arena_serbest(a);
}

static void test_E003_tekkez_escape_metin(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken t = tekkez_olustur(\"hi\"); "
        "de\xc4\x9fi\xc5\x9fken s: metin = t olarak metin; "
        "ver 0; }",
        a);
    test_sonuc("E003 (2): tekkez<metin> olarak metin -> hata", h > 0);
    arena_serbest(a);
}

/* E004: tam64 olarak tam8 (kayip prezisyon) derleme hatasi */
static void test_E004_tam64_to_tam8(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken y: tam64 = 42; "
        "de\xc4\x9fi\xc5\x9fken x: tam8 = y olarak tam8; "
        "ver 0; }",
        a);
    test_sonuc("E004: tam64 olarak tam8 -> hata (kayip prezisyon)", h > 0);
    arena_serbest(a);
}

static void test_E004_tam64_to_tam16(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken y: tam64 = 42; "
        "de\xc4\x9fi\xc5\x9fken x: tam16 = y olarak tam16; "
        "ver 0; }",
        a);
    test_sonuc("E004 (2): tam64 olarak tam16 -> hata", h > 0);
    arena_serbest(a);
}

/* E004 pozitif: tam64 olarak tam32 izinli (32-bit native word) */
static void test_E004_tam64_to_tam32_izinli(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken y: tam64 = 42; "
        "ver y olarak tam32; }",
        a);
    test_sonuc("E004 (3): tam64 olarak tam32 -> OK (native word)", h == 0);
    arena_serbest(a);
}

/* === Adim 5: Bound-aware monomorphization === */

static void test_bound_islev_pozitif(void) {
    /* T: Karsilastirilabilir; uygula bildirimi var -> OK */
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "\xc3\xb6zellik Karsilastirilabilir {} "
        "yap\xc4\xb1 Nokta { x: tam32; } "
        "uygula Karsilastirilabilir i\xc3\xa7in Nokta {} "
        "i\xc5\x9flev en_buyuk<T: Karsilastirilabilir>(e: T) -> tam32 { ver 0; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken n: Nokta = Nokta { x: 1 }; "
        "ver en_buyuk(n); }",
        a);
    test_sonuc("bound pozitif: Nokta uygula Karsilastirilabilir -> OK", h == 0);
    arena_serbest(a);
}

static void test_bound_islev_negatif(void) {
    /* T: Karsilastirilabilir; Cizgi uygula yok -> T030 */
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "\xc3\xb6zellik Karsilastirilabilir {} "
        "yap\xc4\xb1 Cizgi { x: tam32; } "
        "i\xc5\x9flev en_buyuk<T: Karsilastirilabilir>(e: T) -> tam32 { ver 0; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken c: Cizgi = Cizgi { x: 1 }; "
        "ver en_buyuk(c); }",
        a);
    test_sonuc("bound negatif: Cizgi uygula yok -> T030", h > 0);
    arena_serbest(a);
}

static void test_bound_iki_param_pozitif(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "\xc3\xb6zellik A {} \xc3\xb6zellik B {} "
        "yap\xc4\xb1 X { x: tam32; } "
        "yap\xc4\xb1 Y { y: tam32; } "
        "uygula A i\xc3\xa7in X {} "
        "uygula B i\xc3\xa7in Y {} "
        "i\xc5\x9flev f<T: A, U: B>(t: T, u: U) -> tam32 { ver 0; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken x: X = X { x: 1 }; "
        "de\xc4\x9fi\xc5\x9fken y: Y = Y { y: 2 }; "
        "ver f(x, y); }",
        a);
    test_sonuc("bound iki param pozitif (A, B)", h == 0);
    arena_serbest(a);
}

static void test_bound_iki_param_negatif(void) {
    /* Y bound A icin uygula yok */
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "\xc3\xb6zellik A {} "
        "yap\xc4\xb1 X { x: tam32; } "
        "yap\xc4\xb1 Y { y: tam32; } "
        "uygula A i\xc3\xa7in X {} "
        "i\xc5\x9flev f<T: A>(t: T) -> tam32 { ver 0; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken y: Y = Y { y: 1 }; "
        "ver f(y); }",
        a);
    test_sonuc("bound iki param negatif (Y!A) -> T030", h > 0);
    arena_serbest(a);
}

static void test_bound_bilinmeyen_ozellik(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "yap\xc4\xb1 X { x: tam32; } "
        "i\xc5\x9flev f<T: BilinmeyenOzellik>(t: T) -> tam32 { ver 0; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken x: X = X { x: 1 }; "
        "ver f(x); }",
        a);
    test_sonuc("bound bilinmeyen ozellik -> T031", h > 0);
    arena_serbest(a);
}

static void test_bound_yok_bound_check_yok(void) {
    /* T (bound yok) -> her tip kabul (Adim D inference): hata yok */
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "yap\xc4\xb1 X { x: tam32; } "
        "i\xc5\x9flev kimlik<T>(t: T) -> tam32 { ver 0; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken x: X = X { x: 1 }; "
        "ver kimlik(x); }",
        a);
    test_sonuc("bound yok (T:) -> her tip kabul", h == 0);
    arena_serbest(a);
}

static void test_bound_compound_dizi(void) {
    /* T: Karsilastirilabilir; param Dizi<T> (compound) */
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "\xc3\xb6zellik Karsilastirilabilir {} "
        "yap\xc4\xb1 N { x: tam32; } "
        "uygula Karsilastirilabilir i\xc3\xa7in N {} "
        "i\xc5\x9flev en_buyuk<T: Karsilastirilabilir>(d: Dizi<T>) -> tam32 { ver 0; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken n: Dizi<N> = [N { x: 1 }]; "
        "ver en_buyuk(n); }",
        a);
    test_sonuc("bound compound Dizi<N> -> OK", h == 0);
    arena_serbest(a);
}

static void test_bound_metin_tip_atla(void) {
    /* T:Karsilastirilabilir; metin (built-in) — bound check'ten kacar (v1).
     * metin TIP_YAPI degil, dolayisi ile arg_ad NULL -> bound check skip. */
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "\xc3\xb6zellik Karsilastirilabilir {} "
        "i\xc5\x9flev en_buyuk<T: Karsilastirilabilir>(t: T) -> tam32 { ver 0; } "
        "i\xc5\x9flev main() -> tam32 { "
        "ver en_buyuk(\"hi\"); }",
        a);
    test_sonuc("bound metin (built-in) — skip (v1)", h == 0);
    arena_serbest(a);
}

/* === Madde D: Generic callback / multi-param inference === */

static void test_generic_callback_govde(void) {
    Arena *a = arena_olustur(0);
    /* harita govde icinde f(x) - x: T, f: islev(T)->U, f(x) -> U */
    int h = program_kontrol(
        "i\xc5\x9flev harita<T, U>(xs: Dizi<T>, f: i\xc5\x9flev(T) -> U) -> Dizi<U> { "
        "de\xc4\x9fi\xc5\x9fken s: Dizi<U> = dizi_olustur(8); "
        "i\xc3\xa7in x: xs { dizi_ekle(s, f(x)); } "
        "ver s; }",
        a);
    test_sonuc("harita<T,U> govde tip kontrol — OK", h == 0);
    arena_serbest(a);
}

static void test_generic_callback_concrete_instan(void) {
    Arena *a = arena_olustur(0);
    /* harita(intDizi, ikiKat) — T=tam32, U=tam32 */
    int h = program_kontrol(
        "i\xc5\x9flev iki_kat(x: tam32) -> tam32 { ver x * 2; } "
        "i\xc5\x9flev harita<T, U>(xs: Dizi<T>, f: i\xc5\x9flev(T) -> U) -> Dizi<U> { "
        "de\xc4\x9fi\xc5\x9fken s: Dizi<U> = dizi_olustur(8); "
        "i\xc3\xa7in x: xs { dizi_ekle(s, f(x)); } "
        "ver s; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken xs: Dizi<tam32> = dizi_olustur(4); "
        "de\xc4\x9fi\xc5\x9fken ys: Dizi<tam32> = harita(xs, iki_kat); "
        "ver 0; }",
        a);
    test_sonuc("harita concrete instan — OK", h == 0);
    arena_serbest(a);
}

static void test_generic_callback_tip_uyumsuz(void) {
    Arena *a = arena_olustur(0);
    /* harita(metinDizi, ikiKat:tam32->tam32) — T conflict (metin vs tam32) */
    int h = program_kontrol(
        "i\xc5\x9flev iki_kat(x: tam32) -> tam32 { ver x * 2; } "
        "i\xc5\x9flev harita<T, U>(xs: Dizi<T>, f: i\xc5\x9flev(T) -> U) -> Dizi<U> { "
        "de\xc4\x9fi\xc5\x9fken s: Dizi<U> = dizi_olustur(8); "
        "i\xc3\xa7in x: xs { dizi_ekle(s, f(x)); } "
        "ver s; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken xs: Dizi<metin> = dizi_olustur(4); "
        "de\xc4\x9fi\xc5\x9fken ys: Dizi<tam32> = harita(xs, iki_kat); "
        "ver 0; }",
        a);
    test_sonuc("harita metin vs tam32 — T conflict", h > 0);
    arena_serbest(a);
}

static void test_generic_donus_substitusyon(void) {
    Arena *a = arena_olustur(0);
    /* f: islev(T) -> U; cagri donus tipi inferred U */
    int h = program_kontrol(
        "i\xc5\x9flev tek<T, U>(x: T, f: i\xc5\x9flev(T) -> U) -> U { "
        "ver f(x); } "
        "i\xc5\x9flev ikiyle(x: tam32) -> tam64 { ver 42; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken r: tam64 = tek(5, ikiyle); "
        "ver 0; }",
        a);
    test_sonuc("tek<T,U> donus inferred U=tam64", h == 0);
    arena_serbest(a);
}

static void test_generic_iclice_inference(void) {
    Arena *a = arena_olustur(0);
    /* T derinden iclie: Dizi<Dizi<T>> */
    int h = program_kontrol(
        "i\xc5\x9flev duzlestir<T>(xs: Dizi<Dizi<T>>) -> tam32 { "
        "ver 0; } "
        "i\xc5\x9flev main() -> tam32 { ver 0; }",
        a);
    test_sonuc("Dizi<Dizi<T>> param — govde tip kontrol", h == 0);
    arena_serbest(a);
}

/* L-1: Lambda govde scope — parametre referansi */
static void test_lambda_govde(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken f = |x: tam32| x * 2; "
        "ver 0; }",
        a);
    /* Lambda govdesinde x kullanim — scope dogruysa tip kontrol gecer */
    test_sonuc("lambda govde scope (x: tam32 -> x*2)", h == 0);
    arena_serbest(a);
}

/* === D-071: islev/lambda degiskeni yeniden-atama yasagi (G004) === */

static void test_lambda_yeniden_atama_g004(void) {
    Arena *a = arena_olustur(0);
    /* Lambda lokali yeniden atanirsa -> G004 (KARMA closure temsili deger-bagimli;
     * yakalama-durumu degisirse cagri yerinde bare-ptr<->closure uyumsuzlugu =
     * kabul-ama-cokuyor). En az 1 hata bekleniyor. */
    int h = program_kontrol(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken f = |x: tam32| x * 2; "
        "f = |y: tam32| y + 21; ver f(21); }", a);
    test_sonuc("G004: lambda lokali yeniden atama -> hata", h >= 1);
    arena_serbest(a);
}

static void test_lambda_yakalama_divergens_g004(void) {
    Arena *a = arena_olustur(0);
    /* Yakalamali -> yakalamasiz yeniden atama: codegen'de SEGFAULT olurdu;
     * checker artik reddediyor. */
    int h = program_kontrol(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken a: tam32 = 5; "
        "de\xc4\x9fi\xc5\x9fken f = |x: tam32| x + a; "
        "f = |y: tam32| y * 2; ver f(21); }", a);
    test_sonuc("G004: yakalama-durumu divergent yeniden atama -> hata", h >= 1);
    arena_serbest(a);
}

static void test_lambda_decl_tek_atama_temiz(void) {
    Arena *a = arena_olustur(0);
    /* Yeniden atama YOK: lambda bildirimi + cagri -> 0 hata (yanlis-pozitif yok) */
    int h = program_kontrol(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken f = |x: tam32| x * 2; ver f(21); }", a);
    test_sonuc("G004: tek-atama lambda lokali -> 0 hata", h == 0);
    arena_serbest(a);
}

/* G004 negatif: lambda OLMAYAN yeniden-atama (tam32) hala gecmeli (over-reject yok) */
static void test_nonlambda_yeniden_atama_temiz(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken x: tam32 = 5; "
        "x = 7; ver x; }", a);
    test_sonuc("G004: lambda-olmayan yeniden atama (x=7) -> 0 hata", h == 0);
    arena_serbest(a);
}

/* === Madde D: Generic callback tip cikarsamasi === */

/* D-1: Dizi<T> param -> T arg'dan cikarsanir */
static void test_gen_callback_dizi_param(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev ilki<T>(xs: Dizi<T>) -> T { "
        "ver dizi_al(xs, 0); } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken d: Dizi<tam32> = dizi_olustur(4); "
        "dizi_ekle(d, 7); "
        "de\xc4\x9fi\xc5\x9fken v: tam32 = ilki(d); "
        "ver v; }",
        a);
    test_sonuc("D: Dizi<T> param -> T inferred (tam32)", h == 0);
    arena_serbest(a);
}

/* D-2: harita<T,U>(Dizi<T>, islev(T)->U) -> Dizi<U> — multi-param compound */
static void test_gen_callback_harita(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev harita<T, U>(xs: Dizi<T>, f: i\xc5\x9flev(T) -> U) -> Dizi<U> { "
        "de\xc4\x9fi\xc5\x9fken r: Dizi<U> = dizi_olustur(4); "
        "de\xc4\x9fi\xc5\x9fken i: tam32 = 0; "
        "iken i < dizi_boyut(xs) { "
        "  dizi_ekle(r, f(dizi_al(xs, i))); "
        "  i = i + 1; "
        "} "
        "ver r; } "
        "i\xc5\x9flev iki_kat(x: tam32) -> tam32 { ver x * 2; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken xs: Dizi<tam32> = dizi_olustur(4); "
        "de\xc4\x9fi\xc5\x9fken ys: Dizi<tam32> = harita(xs, iki_kat); "
        "ver 0; }",
        a);
    test_sonuc("D: harita<T,U>(Dizi<T>, islev(T)->U) -> Dizi<U>", h == 0);
    arena_serbest(a);
}

/* D-3: Callback'in return tipi U farkli, T'den ayri inference */
static void test_gen_callback_farkli_donus(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev ucur<T, U>(x: T, f: i\xc5\x9flev(T) -> U) -> U { "
        "ver f(x); } "
        "i\xc5\x9flev uzunluk_al(s: metin) -> tam32 { ver metin_uzunluk(s); } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken n: tam32 = ucur(\"merhaba\", uzunluk_al); "
        "ver n; }",
        a);
    test_sonuc("D: ucur<T,U>(T, islev(T)->U) -> U", h == 0);
    arena_serbest(a);
}

/* D-4: secimlik<T> param compound */
static void test_gen_callback_secimlik(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev varsayilan<T>(o: se\xc3\xa7imlik<T>, d: T) -> T { "
        "e\xc5\x9fle\xc5\x9f o { "
        "  de\xc4\x9f" "er(v) => { ver v; } "
        "  hi\xc3\xa7 => { ver d; } "
        "} "
        "ver d; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken o: se\xc3\xa7imlik<tam32> = de\xc4\x9f" "er(42); "
        "ver varsayilan(o, 0); }",
        a);
    test_sonuc("D: varsayilan<T>(secimlik<T>, T) -> T", h == 0);
    arena_serbest(a);
}

/* D-5: Birden cok generic param bagimsiz callsite */
static void test_gen_callback_coklu(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev cift<A, B>(a: A, b: B) -> A { ver a; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken r: tam32 = cift(42, \"hi\"); "
        "ver r; }",
        a);
    test_sonuc("D: cift<A,B>(A, B) -> A — coklu generic", h == 0);
    arena_serbest(a);
}

/* CS-7: Inherent impl kayit edilir, sorgu basarili */
static void test_cs_inherent_kayit(void) {
    Arena *a = arena_olustur(0);
    /* uygula X { ... } -> inherent, ozellik yok */
    int h = program_kontrol(
        "yap\xc4\xb1 X { v: tam32; } "
        "uygula X { i\xc5\x9flev yeni() -> X { ver X { v: 0 }; } }", a);
    /* Inherent impl tek basina hata uretmemeli */
    test_sonuc("cs: inherent impl -> 0 hata", h == 0);
    arena_serbest(a);
}

/* === ADIM 30: Bit operatorleri tip kontrolu === */

static void test_bit_ve_tam32(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    TipBilgisi *t = ifade_tipi("42 & 63", a, s, &hata);
    int ok = hata == 0 && tip_kategorisi_esit(t, TIP_TAM32);
    test_sonuc("bit: 42 & 63 -> tam32", ok);
    arena_serbest(a);
}

static void test_bit_veya_tam32(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    TipBilgisi *t = ifade_tipi("40 | 2", a, s, &hata);
    int ok = hata == 0 && tip_kategorisi_esit(t, TIP_TAM32);
    test_sonuc("bit: 40 | 2 -> tam32", ok);
    arena_serbest(a);
}

static void test_bit_degil_tam32(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    TipBilgisi *t = ifade_tipi("~42", a, s, &hata);
    int ok = hata == 0 && tip_kategorisi_esit(t, TIP_TAM32);
    test_sonuc("bit: ~42 -> tam32", ok);
    arena_serbest(a);
}

static void test_kaydir_tam32(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    TipBilgisi *t = ifade_tipi("21 << 1", a, s, &hata);
    int ok = hata == 0 && tip_kategorisi_esit(t, TIP_TAM32);
    test_sonuc("bit: 21 << 1 -> tam32", ok);
    arena_serbest(a);
}

static void test_bit_ve_mantiksal_hata(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    /* doğru = TOK_DOGRU = mantiksal -> bit op kabul etmez */
    TipBilgisi *t = ifade_tipi("42 & do\xc4\x9f" "ru", a, s, &hata);
    int ok = hata > 0 && tip_kategorisi_esit(t, TIP_HATA);
    test_sonuc("bit: 42 & dogru -> T028 hata", ok);
    arena_serbest(a);
}

static void test_bit_degil_mantiksal_hata(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    TipBilgisi *t = ifade_tipi("~do\xc4\x9f" "ru", a, s, &hata);
    int ok = hata > 0 && tip_kategorisi_esit(t, TIP_HATA);
    test_sonuc("bit: ~dogru -> T028 hata", ok);
    arena_serbest(a);
}

static void test_kaydir_mantiksal_hata(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_hazirla(a);
    int hata;
    TipBilgisi *t = ifade_tipi("1 << do\xc4\x9f" "ru", a, s, &hata);
    int ok = hata > 0 && tip_kategorisi_esit(t, TIP_HATA);
    test_sonuc("bit: 1 << dogru -> T028 hata", ok);
    arena_serbest(a);
}

/* === Built-in IO genisletmesi (ADIM bug-fix) ===
 * tip_kontrol_baslat global scope'a yazdir/bellek_* disinda 5 yeni I/O
 * built-in'i ekledi: yazdir_tam, yazdir_tam64, yazdir_satir, yaz_tam,
 * yaz_tam64. Programi tip_kontrol'den gecirip 0 hata bekliyoruz. */

static void test_builtin_yazdir_tam(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev main() -> tam32 { yazdir_tam(42); ver 0; }", a);
    test_sonuc("built-in: yazdir_tam(tam32) -> 0 hata", h == 0);
    arena_serbest(a);
}

static void test_builtin_yazdir_tam64(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev main() -> tam32 {\n"
        "  de\xc4\x9fi\xc5\x9fken n: tam64 = 1;\n"
        "  yazdir_tam64(n);\n"
        "  ver 0;\n"
        "}", a);
    test_sonuc("built-in: yazdir_tam64(tam64) -> 0 hata", h == 0);
    arena_serbest(a);
}

static void test_builtin_yazdir_satir(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev main() -> tam32 { yazdir_satir(); ver 0; }", a);
    test_sonuc("built-in: yazdir_satir() -> 0 hata", h == 0);
    arena_serbest(a);
}

static void test_builtin_yaz_tam(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev main() -> tam32 { yaz_tam(7); ver 0; }", a);
    test_sonuc("built-in: yaz_tam(tam32) -> 0 hata", h == 0);
    arena_serbest(a);
}

static void test_builtin_yaz_tam64(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev main() -> tam32 {\n"
        "  de\xc4\x9fi\xc5\x9fken n: tam64 = 7;\n"
        "  yaz_tam64(n);\n"
        "  ver 0;\n"
        "}", a);
    test_sonuc("built-in: yaz_tam64(tam64) -> 0 hata", h == 0);
    arena_serbest(a);
}

static void test_builtin_arg_sayi_uyumsuz(void) {
    Arena *a = arena_olustur(0);
    /* yazdir_tam 1 param bekler, 2 verdik -> T010 */
    int h = program_kontrol(
        "i\xc5\x9flev main() -> tam32 { yazdir_tam(1, 2); ver 0; }", a);
    test_sonuc("built-in: yazdir_tam(1,2) -> arg sayi uyumsuz hata", h > 0);
    arena_serbest(a);
}

/* === Main === */

/* === C5 on-kosul #2: unsafe-context bayragi (G001) === */

static void test_guvensiz_deref_icerde_ok(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev f(p: *tam32) -> tam32 { "
        "g\xc3\xbcvensiz { ver *p; } ver 0; }", a);
    test_sonuc("G001: guvensiz ICINDE *p deref -> 0 hata", h == 0);
    arena_serbest(a);
}

static void test_guvensiz_deref_disarda_ret(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev f(p: *tam32) -> tam32 { ver *p; }", a);
    test_sonuc("G001: guvensiz DISINDA *p deref -> 1 hata", h == 1);
    arena_serbest(a);
}

static void test_guvensiz_ic_ice_deref_ok(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev f(p: *tam32) -> tam32 { "
        "g\xc3\xbcvensiz { g\xc3\xbcvensiz { ver *p; } } ver 0; }", a);
    test_sonuc("G001: ic ice guvensiz deref -> 0 hata (derinlik sayaci)", h == 0);
    arena_serbest(a);
}

static void test_guvensiz_kapaninca_deref_ret(void) {
    Arena *a = arena_olustur(0);
    /* guvensiz blok kapandiktan SONRA deref -> bayrak temizlenmis olmali */
    int h = program_kontrol(
        "i\xc5\x9flev f(p: *tam32) -> tam32 { "
        "g\xc3\xbcvensiz { } ver *p; }", a);
    test_sonuc("G001: guvensiz kapandiktan sonra deref -> 1 hata", h == 1);
    arena_serbest(a);
}

/* === C5: satirici_asm tip kurallari (G002 / AS001 / AS002) === */

static void test_asm_temiz(void) {
    Arena *a = arena_olustur(0);
    /* guvensiz icinde, x86_64 tagli, primitif cikti -> 0 hata */
    int h = program_kontrol(
        "i\xc5\x9flev f() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken x: tam32 = 0; "
        "g\xc3\xbcvensiz { sat\xc4\xb1ri\xc3\xa7i_asm { "
        "mimari: x86_64 "
        "\xc5\x9f" "ablon: r#\"nop\"# "
        "\xc3\xa7\xc4\xb1kt\xc4\xb1(\"=r\", &x) } } "
        "ver x; }", a);
    test_sonuc("asm: guvensiz + x86_64 + primitif cikti -> 0 hata", h == 0);
    arena_serbest(a);
}

static void test_asm_guvensiz_disi_g002(void) {
    Arena *a = arena_olustur(0);
    int h = program_kontrol(
        "i\xc5\x9flev f() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken x: tam32 = 0; "
        "sat\xc4\xb1ri\xc3\xa7i_asm { "
        "mimari: x86_64 "
        "\xc5\x9f" "ablon: r#\"nop\"# "
        "\xc3\xa7\xc4\xb1kt\xc4\xb1(\"=r\", &x) } "
        "ver x; }", a);
    test_sonuc("asm: guvensiz DISINDA -> G002 (1 hata)", h == 1);
    arena_serbest(a);
}

static void test_asm_arm64_as001(void) {
    Arena *a = arena_olustur(0);
    /* arm64 tag, x86_64 hedef -> AS001 */
    int h = program_kontrol(
        "i\xc5\x9flev f() -> tam32 { "
        "g\xc3\xbcvensiz { sat\xc4\xb1ri\xc3\xa7i_asm { "
        "mimari: arm64 "
        "\xc5\x9f" "ablon: r#\"wfi\"# } } "
        "ver 0; }", a);
    test_sonuc("asm: arm64 tag x86_64 hedefte -> AS001 (1 hata)", h == 1);
    arena_serbest(a);
}

static void test_asm_tekkez_girdi_as002(void) {
    Arena *a = arena_olustur(0);
    /* C.1 lineer kara kutu: tekkez girdi DOGRUDAN gecemez */
    int h = program_kontrol(
        "i\xc5\x9flev f() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken t: tekkez<tam32> = tekkez_olustur(5); "
        "g\xc3\xbcvensiz { sat\xc4\xb1ri\xc3\xa7i_asm { "
        "mimari: x86_64 "
        "\xc5\x9f" "ablon: r#\"nop\"# "
        "girdi(\"r\", t) } } "
        "imha(t); ver 0; }", a);
    test_sonuc("asm: tekkez girdi -> AS002 (1 hata, lineer-notr)", h == 1);
    arena_serbest(a);
}

static void test_asm_yapi_girdi_as002(void) {
    Arena *a = arena_olustur(0);
    /* C.1: yapi (kompozit) operand olamaz — yalniz primitif */
    int h = program_kontrol(
        "yap\xc4\xb1 N { x: tam32; } "
        "i\xc5\x9flev f() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken n: N = N { x: 1 }; "
        "g\xc3\xbcvensiz { sat\xc4\xb1ri\xc3\xa7i_asm { "
        "mimari: x86_64 "
        "\xc5\x9f" "ablon: r#\"nop\"# "
        "girdi(\"r\", n) } } "
        "ver 0; }", a);
    test_sonuc("asm: yapi girdi -> AS002 (1 hata, primitif degil)", h == 1);
    arena_serbest(a);
}

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

    printf("\n--- Constraint Satisfaction (15.5) ---\n");
    test_cs_bound_violation();
    test_cs_bound_satisfied();
    test_cs_bound_yok();
    test_cs_coklu_bound_ok();
    test_cs_coklu_bound_eksik();
    test_cs_bound_bilinmeyen();
    test_cs_inherent_kayit();

    printf("\n--- Constraint v2 (19) ---\n");
    test_cs_uygula_govde_hata();
    test_cs_uygula_govde_temiz();
    test_cs_method_dispatch();
    test_cs_method_arg_sayi();
    test_cs_trait_method();

    printf("\n--- hiç/değer + Pattern binding (25) ---\n");
    test_hic_secimlik();
    test_deger_konstrüktör();
    test_pattern_binding();
    test_pattern_sonuc_tamam_bind();
    test_pattern_sonuc_hata_bind();
    test_pattern_sonuc_jokerli();
    test_dizi_olustur_temel();
    test_dizi_ekle_tip_uyumlu();
    test_dizi_ekle_tip_uyumsuz();
    test_dizi_al_donus();
    test_dizi_olustur_tam64();
    test_olarak_tam_tam();
    test_olarak_tam_kesirli();
    test_olarak_kesirli_tam();
    test_olarak_metin_yasak();
    test_olarak_karakter_tam();
    /* Adim 4: Cast guarantee testleri */
    test_E001_tekkez_hedef_yasak();
    test_E001_tekkez_hedef_pozitif();
    test_E002_metin_to_tam();
    test_E002_tam_to_metin();
    test_E003_tekkez_escape();
    test_E003_tekkez_escape_metin();
    test_E004_tam64_to_tam8();
    test_E004_tam64_to_tam16();
    test_E004_tam64_to_tam32_izinli();
    /* Adim 5: Bound-aware monomorphization */
    test_bound_islev_pozitif();
    test_bound_islev_negatif();
    test_bound_iki_param_pozitif();
    test_bound_iki_param_negatif();
    test_bound_bilinmeyen_ozellik();
    test_bound_yok_bound_check_yok();
    test_bound_compound_dizi();
    test_bound_metin_tip_atla();
    test_generic_callback_govde();
    test_generic_callback_concrete_instan();
    test_generic_callback_tip_uyumsuz();
    test_generic_donus_substitusyon();
    test_generic_iclice_inference();
    test_sonuc_konstrüktörler();

    printf("\n--- C: sonuç pattern matching (runtime primitif oturumu) ---\n");
    test_sonuc_pattern_matching();
    test_sonuc_tamam_binding();
    test_sonuc_hata_binding();
    test_sonuc_pattern_joker();
    test_sonuc_pattern_yanlis_tip();

    printf("\n--- Lambda govde scope (29) ---\n");
    test_lambda_govde();

    printf("\n--- D: Generic callback tip cikarsama (multi-param compound) ---\n");
    test_gen_callback_dizi_param();
    test_gen_callback_harita();
    test_gen_callback_farkli_donus();
    test_gen_callback_secimlik();
    test_gen_callback_coklu();

    printf("\n--- Bit operatorleri (ADIM 30) ---\n");
    test_bit_ve_tam32();
    test_bit_veya_tam32();
    test_bit_degil_tam32();
    test_kaydir_tam32();
    test_bit_ve_mantiksal_hata();
    test_bit_degil_mantiksal_hata();
    test_kaydir_mantiksal_hata();

    printf("\n--- Built-in I/O genisletmesi (bug-fix) ---\n");
    test_builtin_yazdir_tam();
    test_builtin_yazdir_tam64();
    test_builtin_yazdir_satir();
    test_builtin_yaz_tam();
    test_builtin_yaz_tam64();
    test_builtin_arg_sayi_uyumsuz();

    printf("\n--- C5 on-kosul #2: unsafe-context (G001) ---\n");
    test_guvensiz_deref_icerde_ok();
    test_guvensiz_deref_disarda_ret();
    test_guvensiz_ic_ice_deref_ok();
    test_guvensiz_kapaninca_deref_ret();

    printf("\n--- C5: satirici_asm (G002 / AS001 / AS002) ---\n");
    test_asm_temiz();
    test_asm_guvensiz_disi_g002();
    test_asm_arm64_as001();
    test_asm_tekkez_girdi_as002();
    test_asm_yapi_girdi_as002();

    printf("\n--- D-071: lambda yeniden-atama yasagi (G004) ---\n");
    test_lambda_yeniden_atama_g004();
    test_lambda_yakalama_divergens_g004();
    test_lambda_decl_tek_atama_temiz();
    test_nonlambda_yeniden_atama_temiz();

    printf("\n===========================================\n");
    printf("Toplam: %d | Basarili: %d | Basarisiz: %d\n",
           toplam_test, basarili, basarisiz);
    return basarisiz > 0 ? 1 : 0;
}
