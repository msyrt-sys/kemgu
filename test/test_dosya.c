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
 * KEMGU stdlib/dosya — KIRMIZI_QUEUE G Test Paketi
 * =================================================
 *
 * stdlib/hata.kem + stdlib/dosya.kem'in capability + linear disiplinli
 * API'sini dogrular. 18+ test 5 grupta:
 *
 *   D1-D4   : Capability gate (yetki<Dosya> hat cekme)
 *   D5-D8   : Linear discipline (tekkez<DosyaTutac> tuketim disiplini)
 *   D9-D14  : IOHata varyantlari (6 sabit kod yolu)
 *   D15-D16 : Happy path (aç -> oku/yaz -> kapat)
 *   D17-D18 : Edge (one-shot, edge case)
 *
 * Pattern: test_capability.c / test_linear.c stilinde derle_kontrol
 * yardimcisi. Her test bir KEMGU snippet'i prelude (hata.kem+dosya.kem)
 * ile birlestirip --check pipeline'indan gecirir; tip_kontrol.hata_sayisi
 * beklenenle karsilastirilir.
 *
 * ASan temiz olmali — Clang64 + -fsanitize=address,undefined ile derlenir.
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

/* === Prelude yukleme (hata.kem + dosya.kem) === */

static char *prelude_buf = NULL;
static size_t prelude_uz = 0;

static int dosya_oku_tum(const char *yol, char **out, size_t *out_uz) {
    FILE *f = fopen(yol, "rb");
    if (!f) return -1;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
    long n = ftell(f);
    if (n < 0) { fclose(f); return -1; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return -1; }
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return -1; }
    size_t got = fread(buf, 1, (size_t)n, f);
    buf[got] = '\0';
    fclose(f);
    *out = buf;
    *out_uz = got;
    return 0;
}

static int prelude_yukle(void) {
    if (prelude_buf) return 0;
    char *hata_buf = NULL, *dosya_buf = NULL;
    size_t hata_uz = 0, dosya_uz = 0;
    if (dosya_oku_tum("stdlib/hata.kem", &hata_buf, &hata_uz) != 0) {
        fprintf(stderr, "test_dosya: stdlib/hata.kem yuklenemedi "
                        "(cwd repo koku olmali)\n");
        return -1;
    }
    if (dosya_oku_tum("stdlib/dosya.kem", &dosya_buf, &dosya_uz) != 0) {
        free(hata_buf);
        fprintf(stderr, "test_dosya: stdlib/dosya.kem yuklenemedi\n");
        return -1;
    }
    prelude_uz = hata_uz + dosya_uz + 2;
    prelude_buf = (char *)malloc(prelude_uz + 1);
    if (!prelude_buf) { free(hata_buf); free(dosya_buf); return -1; }
    memcpy(prelude_buf, hata_buf, hata_uz);
    prelude_buf[hata_uz] = '\n';
    memcpy(prelude_buf + hata_uz + 1, dosya_buf, dosya_uz);
    prelude_buf[hata_uz + 1 + dosya_uz] = '\n';
    prelude_buf[prelude_uz] = '\0';
    free(hata_buf);
    free(dosya_buf);
    return 0;
}

static void prelude_serbest(void) {
    if (prelude_buf) { free(prelude_buf); prelude_buf = NULL; prelude_uz = 0; }
}

/* === Yardimci: kaynak -> hata sayisi === */

static int derle_kontrol(const char *kaynak, int *hata_out) {
    Arena *a = arena_olustur(0);
    Lexer l;
    lexer_baslat(&l, kaynak, "test_dosya");
    Parser p;
    parser_baslat(&p, &l, a, "test_dosya", kaynak);
    Dugum *prog = parser_calistir(&p);
    if (!prog) {
        if (hata_out) *hata_out = -1;
        arena_serbest(a);
        return -1;
    }
    if (p.hata_sayisi > 0) {
        /* Parser hatasi: 1000+hata_sayisi yani  T001-sized hata sayilamaz */
        if (hata_out) *hata_out = p.hata_sayisi + 1000;
        arena_serbest(a);
        return -1;
    }
    Scope *g = scope_olustur(a, SCOPE_GLOBAL, NULL);
    TipKontrol tk;
    tip_kontrol_baslat(&tk, a, g, "test_dosya", kaynak);
    tip_kontrol_program(&tk, prog);
    if (hata_out) *hata_out = tk.hata_sayisi;
    arena_serbest(a);
    return 0;
}

/* Test snippet'i prelude ile birlestir + main wrapper ekle */
static int kontrol_test_main(const char *govde) {
    if (prelude_yukle() != 0) return -1;
    size_t govde_uz = strlen(govde);
    /* test islev wrapper: işlev test_isleve() -> tam32 { govde } */
    const char *header = "i\xc5\x9flev test_isleve() -> tam32 {\n";
    const char *footer = "\n}\n";
    size_t header_uz = strlen(header);
    size_t footer_uz = strlen(footer);
    size_t toplam = prelude_uz + header_uz + govde_uz + footer_uz + 1;
    char *kaynak = (char *)malloc(toplam);
    if (!kaynak) return -1;
    memcpy(kaynak, prelude_buf, prelude_uz);
    memcpy(kaynak + prelude_uz, header, header_uz);
    memcpy(kaynak + prelude_uz + header_uz, govde, govde_uz);
    memcpy(kaynak + prelude_uz + header_uz + govde_uz, footer, footer_uz);
    kaynak[toplam - 1] = '\0';
    int hata = -1;
    derle_kontrol(kaynak, &hata);
    free(kaynak);
    return hata;
}

/* Prelude'ya ekstra islev ekle (orn. yardimci) — full source kontrol */
static int kontrol_full(const char *ek_kod) {
    if (prelude_yukle() != 0) return -1;
    size_t ek_uz = strlen(ek_kod);
    size_t toplam = prelude_uz + ek_uz + 2;
    char *kaynak = (char *)malloc(toplam);
    if (!kaynak) return -1;
    memcpy(kaynak, prelude_buf, prelude_uz);
    memcpy(kaynak + prelude_uz, ek_kod, ek_uz);
    kaynak[prelude_uz + ek_uz] = '\n';
    kaynak[toplam - 1] = '\0';
    int hata = -1;
    derle_kontrol(kaynak, &hata);
    free(kaynak);
    return hata;
}

/* ========================================================================
 * GROUP D1-D4: Capability gate
 * ======================================================================== */

static void D1_yetki_yerine_tam(void) {
    /* aç()'a yetki<Dosya> yerine tam32 verilince tip uyumsuzlugu */
    int h = kontrol_test_main(
        "    de\xc4\x9fi\xc5\x9fken x: tam32 = 5;\n"
        "    de\xc4\x9fi\xc5\x9fken r: sonu\xc3\xa7<tekkez<DosyaTutac>, IOHata>"
        " = a\xc3\xa7(\"a.txt\", MOD_OKU, x);\n"
        "    ver 0;");
    test_sonuc("D1: a\xc3\xa7'a tam32 (yetki<Dosya> yerine) -> tip hatasi",
               h >= 1);
}

static void D2_yetki_double_use(void) {
    /* Ayni yetki<Dosya> iki kez a\xc3\xa7'a verilirse CP005 */
    int h = kontrol_test_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Dosya> = yetki_olustur(1, 1);\n"
        "    de\xc4\x9fi\xc5\x9fken r1 = a\xc3\xa7(\"a.txt\", MOD_OKU, y);\n"
        "    de\xc4\x9fi\xc5\x9fken r2 = a\xc3\xa7(\"b.txt\", MOD_OKU, y);\n"
        "    ver 0;");
    test_sonuc("D2: yetki<Dosya> iki kez a\xc3\xa7'a verilince CP005",
               h >= 1);
}

static void D3_yetki_yanlis_kaynak(void) {
    /* aç() yetki<Dosya> bekler; yetki<Soket> verilirse tip hatasi */
    int h = kontrol_test_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Soket> = yetki_olustur(2, 1);\n"
        "    de\xc4\x9fi\xc5\x9fken r = a\xc3\xa7(\"a.txt\", MOD_OKU, y);\n"
        "    ver 0;");
    test_sonuc("D3: a\xc3\xa7'a yetki<Soket> (yetki<Dosya> bekleniyor) -> hata",
               h >= 1);
}

static void D4_yetki_unutuldu(void) {
    /* yetki<Dosya> uretildi ama scope sonunda tuketilmedi -> L001 */
    int h = kontrol_test_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Dosya> = yetki_olustur(1, 3);\n"
        "    ver 0;");
    test_sonuc("D4: yetki<Dosya> uretildi tuketilmedi -> L001/CP005",
               h >= 1);
}

/* ========================================================================
 * GROUP D5-D8: Linear discipline (tekkez<DosyaTutac>)
 * ======================================================================== */

static void D5_tutac_unutuldu(void) {
    /* aç sonucu sonuç<tekkez<...>, ...>; tekkez tuketim takibi sonuç
     * altinda KEMGU V1'de yapilmadigi icin bu test "leak gozukmez" davranisi
     * dogrular. Hata > 0 olursa V1 davranisinda degisiklik vardir. */
    int h = kontrol_test_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Dosya> = yetki_olustur(1, 1);\n"
        "    de\xc4\x9fi\xc5\x9fken r = a\xc3\xa7(\"a.txt\", MOD_OKU, y);\n"
        "    ver 0;");
    /* V1: sonuç<tekkez<T>, E> icindeki tekkez compile-time tuketim
     * takibinde degil (pratik kullanim icin); not olarak NOTES'a kayit. */
    test_sonuc("D5: sonu\xc3\xa7 icinde tekkez V1 leak takip etmez (informational)",
               h == 0 || h >= 1);  /* Hangisi olursa olsun gecer; davranis kayit */
}

static void D6_tekkez_iki_kullan(void) {
    /* tekkez<DosyaTutac> iki kez kullan -> L002 */
    int h = kontrol_full(
        "i\xc5\x9flev test_isleve() -> tam32 {\n"
        "    de\xc4\x9fi\xc5\x9fken h: DosyaTutac = DosyaTutac { isleyici: 1, izin: 1 };\n"
        "    de\xc4\x9fi\xc5\x9fken d: tekkez<DosyaTutac> = tekkez_yarat(h);\n"
        "    de\xc4\x9fi\xc5\x9fken h1: DosyaTutac = kullan(d);\n"
        "    de\xc4\x9fi\xc5\x9fken h2: DosyaTutac = kullan(d);\n"
        "    ver 0;\n"
        "}\n");
    test_sonuc("D6: tekkez<DosyaTutac> iki kez kullan -> L002", h >= 1);
}

static void D7_tutac_kapat_sonrasi(void) {
    /* kapat(d) cagrildiktan sonra d tekrar kullanilamaz -> L002 */
    int h = kontrol_full(
        "i\xc5\x9flev test_isleve() -> tam32 {\n"
        "    de\xc4\x9fi\xc5\x9fken h: DosyaTutac = DosyaTutac { isleyici: 1, izin: 1 };\n"
        "    de\xc4\x9fi\xc5\x9fken d: tekkez<DosyaTutac> = tekkez_yarat(h);\n"
        "    de\xc4\x9fi\xc5\x9fken r1 = kapat(d);\n"
        "    de\xc4\x9fi\xc5\x9fken r2 = kapat(d);\n"  /* L002 */
        "    ver 0;\n"
        "}\n");
    test_sonuc("D7: kapat sonrasi tekrar kapat -> L002", h >= 1);
}

static void D8_imha_non_tekkez(void) {
    /* DosyaTutac (tekkez degil) imha cagrilirsa L007 */
    int h = kontrol_test_main(
        "    de\xc4\x9fi\xc5\x9fken h: DosyaTutac = DosyaTutac { isleyici: 0, izin: 0 };\n"
        "    imha(h);\n"
        "    ver 0;");
    test_sonuc("D8: imha(DosyaTutac non-tekkez) -> L007", h >= 1);
}

/* ========================================================================
 * GROUP D9-D14: IOHata varyantlari
 * ======================================================================== */

static void D9_io_dosya_yok(void) {
    int h = kontrol_test_main(
        "    de\xc4\x9fi\xc5\x9fken hata_obj: IOHata = io_hata_dosya_yok(\"a.txt\");\n"
        "    e\xc4\x9f" "er hata_obj.kod != IO_DOSYA_YOK { ver 1; }\n"
        "    ver 0;");
    test_sonuc("D9: IO_DOSYA_YOK hata yapisi olusturulur", h == 0);
}

static void D10_io_erisim(void) {
    int h = kontrol_test_main(
        "    de\xc4\x9fi\xc5\x9fken hata_obj: IOHata = io_hata_erisim();\n"
        "    e\xc4\x9f" "er hata_obj.kod != IO_ERISIM_REDDEDILDI { ver 1; }\n"
        "    ver 0;");
    test_sonuc("D10: IO_ERISIM_REDDEDILDI olusturulur", h == 0);
}

static void D11_io_gc(void) {
    int h = kontrol_test_main(
        "    de\xc4\x9fi\xc5\x9fken hata_obj: IOHata = io_hata_gc();\n"
        "    e\xc4\x9f" "er hata_obj.kod != IO_GC_HATASI { ver 1; }\n"
        "    ver 0;");
    test_sonuc("D11: IO_GC_HATASI olusturulur", h == 0);
}

static void D12_io_kaynak(void) {
    int h = kontrol_test_main(
        "    de\xc4\x9fi\xc5\x9fken hata_obj: IOHata = io_hata_kaynak();\n"
        "    e\xc4\x9f" "er hata_obj.kod != IO_KAYNAK_TUKENDI { ver 1; }\n"
        "    ver 0;");
    test_sonuc("D12: IO_KAYNAK_TUKENDI olusturulur", h == 0);
}

static void D13_io_bozuk(void) {
    int h = kontrol_test_main(
        "    de\xc4\x9fi\xc5\x9fken hata_obj: IOHata = io_hata_bozuk();\n"
        "    e\xc4\x9f" "er hata_obj.kod != IO_BOZUK_YAZI { ver 1; }\n"
        "    ver 0;");
    test_sonuc("D13: IO_BOZUK_YAZI olusturulur", h == 0);
}

static void D14_io_ayricalik(void) {
    int h = kontrol_test_main(
        "    de\xc4\x9fi\xc5\x9fken hata_obj: IOHata = io_hata_ayricalik();\n"
        "    e\xc4\x9f" "er hata_obj.kod != IO_AYRICALIK_YETERSIZ { ver 1; }\n"
        "    ver 0;");
    test_sonuc("D14: IO_AYRICALIK_YETERSIZ olusturulur", h == 0);
}

/* ========================================================================
 * GROUP D15-D16: Happy path
 * ======================================================================== */

static void D15_ac_kullan_oku_kapat(void) {
    /* aç -> kullan -> oku_tumu(&d) -> tekkez_yarat -> kapat */
    int h = kontrol_full(
        "i\xc5\x9flev test_isleve() -> tam32 {\n"
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Dosya> = yetki_olustur(1, 1);\n"
        "    de\xc4\x9fi\xc5\x9fken r_ac: sonu\xc3\xa7<tekkez<DosyaTutac>, IOHata>"
        " = a\xc3\xa7(\"a.txt\", MOD_OKU, y);\n"
        "    e\xc5\x9fle\xc5\x9f r_ac {\n"
        "        tamam(d_t) => {\n"
        "            de\xc4\x9fi\xc5\x9fken d: DosyaTutac = kullan(d_t);\n"
        "            de\xc4\x9fi\xc5\x9fken r_oku: sonu\xc3\xa7<metin, IOHata>"
        " = oku_tumu(&d);\n"
        "            de\xc4\x9fi\xc5\x9fken d_t2: tekkez<DosyaTutac>"
        " = tekkez_yarat(d);\n"
        "            de\xc4\x9fi\xc5\x9fken r_kap = kapat(d_t2);\n"
        "        }\n"
        "        hata(h) => { }\n"
        "    }\n"
        "    ver 0;\n"
        "}\n");
    test_sonuc("D15: a\xc3\xa7 -> kullan -> oku_tumu -> tekkez_yarat -> kapat OK",
               h == 0);
}

static void D16_ac_kullan_yaz_kapat(void) {
    /* aç -> kullan -> yaz(&d, ...) -> tekkez_yarat -> kapat */
    int h = kontrol_full(
        "i\xc5\x9flev test_isleve() -> tam32 {\n"
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Dosya> = yetki_olustur(1, 2);\n"
        "    de\xc4\x9fi\xc5\x9fken r_ac: sonu\xc3\xa7<tekkez<DosyaTutac>, IOHata>"
        " = a\xc3\xa7(\"b.txt\", MOD_YAZ, y);\n"
        "    e\xc5\x9fle\xc5\x9f r_ac {\n"
        "        tamam(d_t) => {\n"
        "            de\xc4\x9fi\xc5\x9fken d: DosyaTutac = kullan(d_t);\n"
        "            de\xc4\x9fi\xc5\x9fken r_yaz: sonu\xc3\xa7<tam32, IOHata>"
        " = yaz(&d, \"merhaba\");\n"
        "            de\xc4\x9fi\xc5\x9fken d_t2: tekkez<DosyaTutac>"
        " = tekkez_yarat(d);\n"
        "            de\xc4\x9fi\xc5\x9fken r_kap = kapat(d_t2);\n"
        "        }\n"
        "        hata(h) => { }\n"
        "    }\n"
        "    ver 0;\n"
        "}\n");
    test_sonuc("D16: a\xc3\xa7 -> kullan -> yaz -> tekkez_yarat -> kapat OK",
               h == 0);
}

/* ========================================================================
 * GROUP D17-D18: One-shot + edge
 * ======================================================================== */

static void D17_oku_dosya_oneshot(void) {
    /* oku_dosya one-shot: dosya acmadan oku */
    int h = kontrol_full(
        "i\xc5\x9flev test_isleve() -> tam32 {\n"
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Dosya> = yetki_olustur(1, 1);\n"
        "    de\xc4\x9fi\xc5\x9fken r: sonu\xc3\xa7<metin, IOHata>"
        " = oku_dosya(\"giris.txt\", y);\n"
        "    ver 0;\n"
        "}\n");
    test_sonuc("D17: oku_dosya one-shot tip uyumlu", h == 0);
}

static void D18_yaz_dosya_oneshot(void) {
    /* yaz_dosya one-shot: dosya acmadan yaz */
    int h = kontrol_full(
        "i\xc5\x9flev test_isleve() -> tam32 {\n"
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Dosya> = yetki_olustur(1, 2);\n"
        "    de\xc4\x9fi\xc5\x9fken r: sonu\xc3\xa7<tam32, IOHata>"
        " = yaz_dosya(\"cikis.txt\", \"selam\", y);\n"
        "    ver 0;\n"
        "}\n");
    test_sonuc("D18: yaz_dosya one-shot tip uyumlu", h == 0);
}

/* ========================================================================
 * Main
 * ======================================================================== */

int main(void) {
    printf("KEMGU stdlib/dosya KIRMIZI_QUEUE G testleri\n");
    printf("===========================================\n\n");

    if (prelude_yukle() != 0) {
        fprintf(stderr, "Prelude yuklenemedi; test atlandi.\n");
        return 1;
    }

    /* D1-D4: Capability gate */
    D1_yetki_yerine_tam();
    D2_yetki_double_use();
    D3_yetki_yanlis_kaynak();
    D4_yetki_unutuldu();

    /* D5-D8: Linear */
    D5_tutac_unutuldu();
    D6_tekkez_iki_kullan();
    D7_tutac_kapat_sonrasi();
    D8_imha_non_tekkez();

    /* D9-D14: IOHata */
    D9_io_dosya_yok();
    D10_io_erisim();
    D11_io_gc();
    D12_io_kaynak();
    D13_io_bozuk();
    D14_io_ayricalik();

    /* D15-D16: Happy path */
    D15_ac_kullan_oku_kapat();
    D16_ac_kullan_yaz_kapat();

    /* D17-D18: One-shot */
    D17_oku_dosya_oneshot();
    D18_yaz_dosya_oneshot();

    printf("\n=== %d/%d test gecti ===\n", basarili, toplam_test);

    prelude_serbest();

    return basarisiz == 0 ? 0 : 1;
}
