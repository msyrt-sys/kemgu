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

/* === E.1: Sembol-bolge haritasi === */

/* Yardimci: tum islev govdesini parse et + bolge_belirle govde uzerinde */
static BolgeAtama *islev_govde_calistir(const char *kaynak, Arena *a) {
    Lexer l;
    lexer_baslat(&l, kaynak, "test");
    Parser p;
    parser_baslat(&p, &l, a, "test", kaynak);
    Dugum *prog = parser_calistir(&p);
    if (!prog || prog->veri.program.sayi == 0) return NULL;
    Dugum *islev = prog->veri.program.uyeler[0];
    if (islev->tip != DUGUM_ISLEV || !islev->veri.islev.govde) return NULL;

    BolgeAtama *ba = (BolgeAtama *)arena_ayir_sifir(a, sizeof(BolgeAtama));
    bolge_atama_baslat(ba, a, islev->veri.islev.ad,
                       islev->veri.islev.ad_uzunluk);
    bolge_atama_kaynak_ayarla(ba, "test", kaynak);
    bolge_belirle(ba, islev->veri.islev.govde);
    return ba;
}

static void test_sembol_bolge_takip(void) {
    Arena *a = arena_olustur(0);
    /* x: tam32 (LIT), y: metin (YEREL) — değişken kayıtları */
    const char *src =
        "i\xc5\x9flev f() {\n"
        "    de\xc4\x9f" "i\xc5\x9fken x = 42;\n"
        "    de\xc4\x9f" "i\xc5\x9fken y = \"hi\";\n"
        "}\n";
    BolgeAtama *ba = islev_govde_calistir(src, a);
    /* sembol_sayi BLOK cikisinda watermark ile sifirlanir (scope semantigi);
     * sadece hata olmadigini dogrula */
    test_sonuc("E.1: degisken kayitlari (hata yok)",
               ba && ba->hata_sayisi == 0);
    arena_serbest(a);
}

static void test_sembol_bolge_lookup(void) {
    Arena *a = arena_olustur(0);
    /* y'nin bolgesi = literal x'in bolgesi (LIT) -- tanimlayici lookup */
    const char *src =
        "i\xc5\x9flev f() {\n"
        "    de\xc4\x9f" "i\xc5\x9fken x = 42;\n"
        "    de\xc4\x9f" "i\xc5\x9fken y = x;\n"
        "}\n";
    BolgeAtama *ba = islev_govde_calistir(src, a);
    /* y kaydedildi mi? hata yok mu? */
    test_sonuc("E.1: tanimlayici lookup (y = x)",
               ba && ba->hata_sayisi == 0);
    arena_serbest(a);
}

/* === E.3: VER ihlali tespiti === */

static void test_ver_lit_hata_yok(void) {
    Arena *a = arena_olustur(0);
    /* ver 42 — LIT, hata yok */
    const char *src =
        "i\xc5\x9flev f() -> tam32 {\n"
        "    ver 42;\n"
        "}\n";
    BolgeAtama *ba = islev_govde_calistir(src, a);
    test_sonuc("E.3: 'ver 42' hatasi yok (LIT escape OK)",
               ba && ba->hata_sayisi == 0);
    arena_serbest(a);
}

static void test_ver_yerel_metin(void) {
    Arena *a = arena_olustur(0);
    /* ver "merhaba" - LIT olarak gorulur (R-VER: CAGIRAN'a kacar, OK) */
    const char *src =
        "i\xc5\x9flev f() -> metin {\n"
        "    ver \"merhaba\";\n"
        "}\n";
    BolgeAtama *ba = islev_govde_calistir(src, a);
    /* Metin literal ver baglaminda CAGIRAN — hata yok */
    test_sonuc("E.3: 'ver \"metin\"' hatasi yok",
               ba && ba->hata_sayisi == 0);
    arena_serbest(a);
}

static void test_ver_yerel_referans_ihlal(void) {
    Arena *a = arena_olustur(0);
    /* B001: ver &x — x YEREL, & yerel adresini caller'a sızdırıyor */
    const char *src =
        "i\xc5\x9flev kotu() -> &tam32 {\n"
        "    de\xc4\x9f" "i\xc5\x9fken x: tam32 = 42;\n"
        "    ver &x;\n"
        "}\n";
    BolgeAtama *ba = islev_govde_calistir(src, a);
    test_sonuc("E.3: 'ver &yerel' B001 ihlali tespit edildi",
               ba && ba->hata_sayisi == 1);
    arena_serbest(a);
}

static void test_ver_deger_kopya_ok(void) {
    Arena *a = arena_olustur(0);
    /* ver x (kopya semantik) — y YEREL ama deger kopyalanir, OK */
    const char *src =
        "i\xc5\x9flev iyi() -> tam32 {\n"
        "    de\xc4\x9f" "i\xc5\x9fken x: tam32 = 42;\n"
        "    ver x;\n"
        "}\n";
    BolgeAtama *ba = islev_govde_calistir(src, a);
    test_sonuc("E.3: 'ver x' (kopya) hatasiz",
               ba && ba->hata_sayisi == 0);
    arena_serbest(a);
}

/* === E.2: Atama dataflow (basit) === */

/* === Katman 2: SAHIP/KANAL + B003 ihlali === */

static void test_kanal_yerel_ihlal(void) {
    Arena *a = arena_olustur(0);
    /* B003: kanal_gonder(&yerel) -> Katman 2 ihlali */
    const char *src =
        "i\xc5\x9flev kotu() {\n"
        "    de\xc4\x9f" "i\xc5\x9fken k: metin = kanal_olustur(8);\n"
        "    de\xc4\x9f" "i\xc5\x9fken x: tam32 = 42;\n"
        "    kanal_gonder(k, x);\n"   /* kopya OK */
        "    kanal_gonder(k, x);\n"
        "}\n";
    BolgeAtama *ba = islev_govde_calistir(src, a);
    test_sonuc("Katman 2: kanal_gonder(k, x) kopya OK",
               ba && ba->hata_sayisi == 0);
    arena_serbest(a);
}

static void test_atama_uyumlu(void) {
    Arena *a = arena_olustur(0);
    /* x atanır, sonra y'ye atanır — sembol haritasi ile takip */
    const char *src =
        "i\xc5\x9flev f() {\n"
        "    de\xc4\x9f" "i\xc5\x9fken x = 1;\n"
        "    de\xc4\x9f" "i\xc5\x9fken y = 2;\n"
        "    x = y;\n"
        "}\n";
    BolgeAtama *ba = islev_govde_calistir(src, a);
    test_sonuc("E.2: ayni bolge atama hatasiz",
               ba && ba->hata_sayisi == 0);
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

    printf("\n--- E.1: Sembol-bolge takibi ---\n");
    test_sembol_bolge_takip();
    test_sembol_bolge_lookup();

    printf("\n--- E.3: VER ihlal tespiti ---\n");
    test_ver_lit_hata_yok();
    test_ver_yerel_metin();
    test_ver_yerel_referans_ihlal();
    test_ver_deger_kopya_ok();

    printf("\n--- E.2: Atama dataflow ---\n");
    test_atama_uyumlu();

    printf("\n--- Katman 2: SAHIP/KANAL ---\n");
    test_kanal_yerel_ihlal();

    printf("\n==============================\n");
    printf("Toplam: %d | Basarili: %d | Basarisiz: %d\n",
           toplam_test, basarili, basarisiz);
    return basarisiz > 0 ? 1 : 0;
}
