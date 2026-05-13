#include "escape.h"
#include "parser.h"
#include "lexer.h"
#include "ast.h"
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

/* === Yardimcilar === */

static Dugum *kaynaktan_ayrist(Arena *a, const char *kaynak) {
    Lexer l;
    lexer_baslat(&l, kaynak, "test");
    Parser p;
    parser_baslat(&p, &l, a, "test", kaynak);
    return parser_calistir(&p);
}

/* AST icinde verilen DugumTipi'nde ilk N. dugumu bul (DFS). */
typedef struct {
    DugumTipi aranan;
    int hedef_indeks;     /* kacinci eslesme istiyoruz (0-tabanli) */
    int gecerli_indeks;
    const Dugum *bulundu;
} DugumArama;

static void ara(DugumArama *aa, const Dugum *d);

static void ara_list(DugumArama *aa, Dugum **liste, int sayi) {
    for (int i = 0; i < sayi && !aa->bulundu; i++) ara(aa, liste[i]);
}

static void ara(DugumArama *aa, const Dugum *d) {
    if (!d || aa->bulundu) return;
    if (d->tip == aa->aranan) {
        if (aa->gecerli_indeks == aa->hedef_indeks) {
            aa->bulundu = d;
            return;
        }
        aa->gecerli_indeks++;
    }
    /* Recurse — yeterli alanlari kapsa */
    switch (d->tip) {
        case DUGUM_PROGRAM:
            ara_list(aa, d->veri.program.uyeler, d->veri.program.sayi); break;
        case DUGUM_MODUL:
            ara_list(aa, d->veri.modul.uyeler, d->veri.modul.sayi); break;
        case DUGUM_DISA:
            ara(aa, d->veri.disa.tanim); break;
        case DUGUM_ISLEV:
            ara_list(aa, d->veri.islev.parametreler, d->veri.islev.param_sayi);
            ara(aa, d->veri.islev.govde); break;
        case DUGUM_BLOK:
            ara_list(aa, d->veri.blok.deyimler, d->veri.blok.sayi); break;
        case DUGUM_DEGISKEN:
            ara(aa, d->veri.degisken.deger); break;
        case DUGUM_VER:
            ara(aa, d->veri.ver.deger); break;
        case DUGUM_ATAMA:
            ara(aa, d->veri.atama.hedef);
            ara(aa, d->veri.atama.deger); break;
        case DUGUM_EGER:
            ara(aa, d->veri.eger.kosul);
            ara(aa, d->veri.eger.gozdoldur);
            ara(aa, d->veri.eger.yan); break;
        case DUGUM_IKEN:
            ara(aa, d->veri.iken.kosul);
            ara(aa, d->veri.iken.govde); break;
        case DUGUM_ICIN:
            ara(aa, d->veri.icin.koleksiyon);
            ara(aa, d->veri.icin.govde); break;
        case DUGUM_IFADE_DEYIMI:
            ara(aa, d->veri.ifade_deyimi.ifade); break;
        case DUGUM_IKILI:
            ara(aa, d->veri.ikili.sol);
            ara(aa, d->veri.ikili.sag); break;
        case DUGUM_TEKLI:
            ara(aa, d->veri.tekli.operand); break;
        case DUGUM_CAGRI:
            ara(aa, d->veri.cagri.hedef);
            ara_list(aa, d->veri.cagri.argumanlar, d->veri.cagri.sayi); break;
        case DUGUM_ERISIM:
            ara(aa, d->veri.erisim.nesne); break;
        case DUGUM_INDEKS:
            ara(aa, d->veri.indeks.nesne);
            ara(aa, d->veri.indeks.indeks); break;
        case DUGUM_DIZI_OLUSTUR:
            ara_list(aa, d->veri.dizi_olustur.elemanlar, d->veri.dizi_olustur.sayi); break;
        case DUGUM_YAPI_OLUSTUR:
            for (int i = 0; i < d->veri.yapi_olustur.alan_sayi && !aa->bulundu; i++) {
                Dugum *aa2 = d->veri.yapi_olustur.alanlar[i];
                if (aa2 && aa2->tip == DUGUM_ALAN_ATAMA) ara(aa, aa2->veri.alan_atama.deger);
            }
            break;
        case DUGUM_LAMBDA:
            ara(aa, d->veri.lambda.govde); break;
        case DUGUM_GUVENSIZ:
            ara(aa, d->veri.guvensiz.blok); break;
        case DUGUM_ESLES:
            ara(aa, d->veri.esles.deger);
            for (int i = 0; i < d->veri.esles.kol_sayi && !aa->bulundu; i++) {
                Dugum *k = d->veri.esles.kollar[i];
                if (k && k->tip == DUGUM_ESLES_KOLU) ara(aa, k->veri.esles_kolu.govde);
            }
            break;
        case DUGUM_SABIT:
            ara(aa, d->veri.sabit.deger); break;
        default:
            break;
    }
}

static const Dugum *bul(const Dugum *kok, DugumTipi tip, int indeks) {
    DugumArama aa = { tip, indeks, 0, NULL };
    ara(&aa, kok);
    return aa.bulundu;
}

/* === Testler === */

/* T1: ver "hello" -> METIN CAGIRAN */
static void test_ver_metin_cagiran(void) {
    Arena *a = arena_olustur(0);
    Dugum *prog = kaynaktan_ayrist(a,
        "i\xc5\x9f" "lev f() -> metin { ver \"merhaba\"; }");
    const Dugum *metin = bul(prog, DUGUM_METIN, 0);

    EscapeAnaliz ea;
    escape_baslat(&ea, a);
    escape_analiz_program(&ea, prog);
    int ok = metin && escape_kategori(&ea, metin) == ESC_CAGIRAN;
    test_sonuc("ver \"merhaba\" -> METIN CAGIRAN", ok);
    escape_serbest(&ea);
    arena_serbest(a);
}

/* T2: ver 42 (basit literal — METIN/DIZI yok, kayit olusmaz) */
static void test_ver_basit_lit(void) {
    Arena *a = arena_olustur(0);
    Dugum *prog = kaynaktan_ayrist(a,
        "i\xc5\x9f" "lev f() -> tam32 { ver 42; }");
    EscapeAnaliz ea;
    escape_baslat(&ea, a);
    escape_analiz_program(&ea, prog);
    /* Basit literal kayit olusturmaz — toplam kayit 0 olmali */
    int ok = ea.kayit_sayi == 0;
    test_sonuc("ver 42: basit lit kayit olusturmaz", ok);
    escape_serbest(&ea);
    arena_serbest(a);
}

/* T3: TRANSITIVE — degisken x = "..."; ver x; -> "..." CAGIRAN */
static void test_transitif_metin(void) {
    Arena *a = arena_olustur(0);
    Dugum *prog = kaynaktan_ayrist(a,
        "i\xc5\x9f" "lev f() -> metin { "
        "de\xc4\x9f" "i\xc5\x9f" "ken x = \"merhaba\"; "
        "ver x; }");
    const Dugum *metin = bul(prog, DUGUM_METIN, 0);
    EscapeAnaliz ea;
    escape_baslat(&ea, a);
    escape_analiz_program(&ea, prog);
    int ok = metin && escape_kategori(&ea, metin) == ESC_CAGIRAN;
    test_sonuc("transitif: degisken x = \"..\"; ver x -> CAGIRAN", ok);
    escape_serbest(&ea);
    arena_serbest(a);
}

/* T4: degisken x = "hello"; — escape yok (sadece bag, ver yok) */
static void test_yerel_kalir(void) {
    Arena *a = arena_olustur(0);
    Dugum *prog = kaynaktan_ayrist(a,
        "i\xc5\x9f" "lev f() -> tam32 { "
        "de\xc4\x9f" "i\xc5\x9f" "ken x = \"merhaba\"; "
        "ver 0; }");
    const Dugum *metin = bul(prog, DUGUM_METIN, 0);
    EscapeAnaliz ea;
    escape_baslat(&ea, a);
    escape_analiz_program(&ea, prog);
    int ok = metin && escape_kategori(&ea, metin) == ESC_YEREL;
    test_sonuc("escape yok: metin YEREL kalir", ok);
    escape_serbest(&ea);
    arena_serbest(a);
}

/* T5: Dizi olusturma — ver [1,2,3] -> DIZI_OLUSTUR CAGIRAN */
static void test_dizi_cagiran(void) {
    Arena *a = arena_olustur(0);
    Dugum *prog = kaynaktan_ayrist(a,
        "i\xc5\x9f" "lev f() -> Dizi<tam32> { ver [1, 2, 3]; }");
    const Dugum *dizi = bul(prog, DUGUM_DIZI_OLUSTUR, 0);
    EscapeAnaliz ea;
    escape_baslat(&ea, a);
    escape_analiz_program(&ea, prog);
    int ok = dizi && escape_kategori(&ea, dizi) == ESC_CAGIRAN;
    test_sonuc("ver [1,2,3] -> DIZI CAGIRAN", ok);
    escape_serbest(&ea);
    arena_serbest(a);
}

/* T6: Cift transitif — y = x; ver y; -> x'in tahsisi CAGIRAN */
static void test_cift_transitif(void) {
    Arena *a = arena_olustur(0);
    Dugum *prog = kaynaktan_ayrist(a,
        "i\xc5\x9f" "lev f() -> metin { "
        "de\xc4\x9f" "i\xc5\x9f" "ken x = \"merhaba\"; "
        "de\xc4\x9f" "i\xc5\x9f" "ken y = x; "
        "ver y; }");
    const Dugum *metin = bul(prog, DUGUM_METIN, 0);
    EscapeAnaliz ea;
    escape_baslat(&ea, a);
    escape_analiz_program(&ea, prog);
    int ok = metin && escape_kategori(&ea, metin) == ESC_CAGIRAN;
    test_sonuc("cift transitif: y = x; ver y -> CAGIRAN", ok);
    escape_serbest(&ea);
    arena_serbest(a);
}

/* T7: Kosullu dallanma — eger cond { ver "a" } degilse { ver "b" }
 *     -> Iki METIN de CAGIRAN */
static void test_kosullu_iki_dal(void) {
    Arena *a = arena_olustur(0);
    Dugum *prog = kaynaktan_ayrist(a,
        "i\xc5\x9f" "lev f(k: mant\xc4\xb1" "ksal) -> metin { "
        "e\xc4\x9f" "er k { ver \"evet\"; } "
        "de\xc4\x9f" "ilse { ver \"hayir\"; } "
        "ver \"varsayilan\"; }");
    const Dugum *m1 = bul(prog, DUGUM_METIN, 0);
    const Dugum *m2 = bul(prog, DUGUM_METIN, 1);
    const Dugum *m3 = bul(prog, DUGUM_METIN, 2);
    EscapeAnaliz ea;
    escape_baslat(&ea, a);
    escape_analiz_program(&ea, prog);
    int ok = m1 && m2 && m3
          && escape_kategori(&ea, m1) == ESC_CAGIRAN
          && escape_kategori(&ea, m2) == ESC_CAGIRAN
          && escape_kategori(&ea, m3) == ESC_CAGIRAN;
    test_sonuc("kosullu iki dal: tum METIN CAGIRAN", ok);
    escape_serbest(&ea);
    arena_serbest(a);
}

/* T8: Dongu icinde tahsis — for loop'taki dizi ITERASYON */
static void test_dongu_iterasyon(void) {
    Arena *a = arena_olustur(0);
    Dugum *prog = kaynaktan_ayrist(a,
        "i\xc5\x9f" "lev f() -> tam32 { "
        "i\xc3\xa7" "in i : [0, 1, 2] { "
        "de\xc4\x9f" "i\xc5\x9f" "ken x = [10, 20, 30]; "
        "} ver 0; }");
    /* Ilk DIZI_OLUSTUR = [0,1,2] (icin koleksiyonu, dongu DISI),
     * Ikinci = [10,20,30] (dongu icindeki) */
    const Dugum *d_dis = bul(prog, DUGUM_DIZI_OLUSTUR, 0);
    const Dugum *d_ic = bul(prog, DUGUM_DIZI_OLUSTUR, 1);
    EscapeAnaliz ea;
    escape_baslat(&ea, a);
    escape_analiz_program(&ea, prog);
    int ok_dis = d_dis && escape_kategori(&ea, d_dis) == ESC_YEREL;
    int ok_ic = d_ic && escape_kategori(&ea, d_ic) == ESC_ITERASYON;
    test_sonuc("dongu disi DIZI YEREL", ok_dis);
    test_sonuc("dongu ici DIZI ITERASYON", ok_ic);
    escape_serbest(&ea);
    arena_serbest(a);
}

/* T9: ver e[0] (indeks ile) — METIN'i CAGIRAN olarak isaretle (konservatif) */
static void test_indeks_konservatif(void) {
    Arena *a = arena_olustur(0);
    Dugum *prog = kaynaktan_ayrist(a,
        "i\xc5\x9f" "lev f() -> tam32 { "
        "de\xc4\x9f" "i\xc5\x9f" "ken x = [1, 2, 3]; "
        "ver x[0]; }");
    const Dugum *dizi = bul(prog, DUGUM_DIZI_OLUSTUR, 0);
    EscapeAnaliz ea;
    escape_baslat(&ea, a);
    escape_analiz_program(&ea, prog);
    /* Konservatif: indeks alti tahsis escape kabul edilir */
    int ok = dizi && escape_kategori(&ea, dizi) == ESC_CAGIRAN;
    test_sonuc("indeks konservatif: ver x[0] -> dizi CAGIRAN", ok);
    escape_serbest(&ea);
    arena_serbest(a);
}

/* T10: Fixed-point dongu — birden cok pass gerektirir
 *
 *   degisken a = "foo";
 *   degisken b = a;
 *   degisken c = b;
 *   ver c;
 *
 * 'a' (METIN) -> CAGIRAN olmaliyiz transitif olarak.
 */
static void test_fixed_point_zincir(void) {
    Arena *a = arena_olustur(0);
    Dugum *prog = kaynaktan_ayrist(a,
        "i\xc5\x9f" "lev f() -> metin { "
        "de\xc4\x9f" "i\xc5\x9f" "ken a = \"deep\"; "
        "de\xc4\x9f" "i\xc5\x9f" "ken b = a; "
        "de\xc4\x9f" "i\xc5\x9f" "ken c = b; "
        "ver c; }");
    const Dugum *metin = bul(prog, DUGUM_METIN, 0);
    EscapeAnaliz ea;
    escape_baslat(&ea, a);
    escape_analiz_program(&ea, prog);
    int ok = metin && escape_kategori(&ea, metin) == ESC_CAGIRAN;
    test_sonuc("fixed-point zincir: a -> b -> c -> ver -> 'deep' CAGIRAN", ok);
    escape_serbest(&ea);
    arena_serbest(a);
}

/* T11: Yapi olusturma — ver Kutu { x: 5 } -> YAPI CAGIRAN */
static void test_yapi_olustur_cagiran(void) {
    Arena *a = arena_olustur(0);
    Dugum *prog = kaynaktan_ayrist(a,
        "yap\xc4\xb1 Kutu { x: tam32; } "
        "i\xc5\x9f" "lev f() -> Kutu { ver Kutu { x: 5 }; }");
    const Dugum *yapi = bul(prog, DUGUM_YAPI_OLUSTUR, 0);
    EscapeAnaliz ea;
    escape_baslat(&ea, a);
    escape_analiz_program(&ea, prog);
    int ok = yapi && escape_kategori(&ea, yapi) == ESC_CAGIRAN;
    test_sonuc("ver Kutu{x:5} -> YAPI CAGIRAN", ok);
    escape_serbest(&ea);
    arena_serbest(a);
}

/* T12: Birden cok islev — analiz her isleve bagimsiz uygulanir */
static void test_coklu_islev(void) {
    Arena *a = arena_olustur(0);
    Dugum *prog = kaynaktan_ayrist(a,
        "i\xc5\x9f" "lev f() -> metin { ver \"escape\"; } "
        "i\xc5\x9f" "lev g() -> tam32 { "
        "de\xc4\x9f" "i\xc5\x9f" "ken y = \"yerel\"; "
        "ver 0; }");
    const Dugum *m_escape = bul(prog, DUGUM_METIN, 0);
    const Dugum *m_yerel = bul(prog, DUGUM_METIN, 1);
    EscapeAnaliz ea;
    escape_baslat(&ea, a);
    escape_analiz_program(&ea, prog);
    int ok1 = m_escape && escape_kategori(&ea, m_escape) == ESC_CAGIRAN;
    int ok2 = m_yerel && escape_kategori(&ea, m_yerel) == ESC_YEREL;
    test_sonuc("coklu islev: f \"escape\" CAGIRAN", ok1);
    test_sonuc("coklu islev: g \"yerel\" YEREL", ok2);
    escape_serbest(&ea);
    arena_serbest(a);
}

/* T13: API smoke — escape_kategori_adi */
static void test_kategori_adi(void) {
    int ok = strcmp(escape_kategori_adi(ESC_YEREL), "YEREL") == 0
          && strcmp(escape_kategori_adi(ESC_ITERASYON), "ITERASYON") == 0
          && strcmp(escape_kategori_adi(ESC_CAGIRAN), "CAGIRAN") == 0;
    test_sonuc("kategori adlari", ok);
}

/* T14: Bos islev govdesi — kayit olmaz, crash etmez */
static void test_bos_govde(void) {
    Arena *a = arena_olustur(0);
    Dugum *prog = kaynaktan_ayrist(a,
        "i\xc5\x9f" "lev f() -> tam32 { ver 0; }");
    EscapeAnaliz ea;
    escape_baslat(&ea, a);
    escape_analiz_program(&ea, prog);
    int ok = ea.kayit_sayi == 0;
    test_sonuc("basit govde (ver 0): 0 kayit", ok);
    escape_serbest(&ea);
    arena_serbest(a);
}

/* T15: NULL girdiler — crash etmez */
static void test_null_girdiler(void) {
    Arena *a = arena_olustur(0);
    EscapeAnaliz ea;
    escape_baslat(&ea, a);
    escape_analiz_program(&ea, NULL);
    escape_analiz_islev(&ea, NULL);
    int ok = ea.kayit_sayi == 0
          && escape_kategori(&ea, NULL) == ESC_YEREL;
    test_sonuc("NULL girdiler: 0 kayit, default YEREL", ok);
    escape_serbest(&ea);
    arena_serbest(a);
}

/* === Main === */

int main(void) {
    printf("KEMGU Escape Analiz Test Paketi\n");
    printf("=================================\n");

    printf("\n--- Temel ver ---\n");
    test_ver_metin_cagiran();
    test_ver_basit_lit();
    test_yerel_kalir();

    printf("\n--- Transitif (DFA gucu) ---\n");
    test_transitif_metin();
    test_cift_transitif();
    test_fixed_point_zincir();

    printf("\n--- Yapi/Dizi ---\n");
    test_dizi_cagiran();
    test_yapi_olustur_cagiran();

    printf("\n--- Kontrol akisi ---\n");
    test_kosullu_iki_dal();
    test_dongu_iterasyon();

    printf("\n--- Konservatif (alt-tahsis) ---\n");
    test_indeks_konservatif();

    printf("\n--- Coklu islev ---\n");
    test_coklu_islev();

    printf("\n--- API + sinir durumlar ---\n");
    test_kategori_adi();
    test_bos_govde();
    test_null_girdiler();

    printf("\n=================================\n");
    printf("Toplam: %d | Basarili: %d | Basarisiz: %d\n",
           toplam_test, basarili, basarisiz);
    return basarisiz > 0 ? 1 : 0;
}
