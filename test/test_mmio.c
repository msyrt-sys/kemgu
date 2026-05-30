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
 * KEMGU MMIO Foundation — Tip Kontrol Test Paketi
 * ===============================================
 *
 * mmio_oku32(y: yetki<MMIO>, adres: tam64) -> tam32   (Karar 1, 2, 4)
 * mmio_yaz32(y: yetki<MMIO>, adres: tam64, deger: tam32) -> bos
 *
 * Hata kodlari:
 *   MM001 — arguman sayisi yanlis
 *   MM002 — ilk arguman yetki<MMIO> degil (kaynak izolasyonu — Karar 3)
 *   MM003 — adres/deger argumani tamsayi degil
 *   CP005 — yetki<R> linear ihlali (leak / cift tuketim)
 *
 * Onemli davranis: mmio_oku32/yaz32 yetkiyi ODUNC alir (TUKETMEZ);
 * tuketim yalniz geri_al ile. Bu yuzden tek yetki ile cok register erisimi
 * mumkun; ama scope sonunda geri_al sart (yoksa CP005 leak).
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

/* main() govdesine gom (donus tipsiz). */
static int kontrol_main(const char *govde) {
    static char buf[16384];
    snprintf(buf, sizeof(buf),
        "i\xc5\x9flev test() {\n%s\n}\n", govde);
    return hata_sayisi(buf);
}

/* ========================================================================
 * POZITIF (M1-M7)
 * ======================================================================== */

static void M1_oku_temel(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<MMIO> = yetki_olustur(6, 1);\n"
        "    de\xc4\x9fi\xc5\x9fken v: tam32 = mmio_oku32(y, 4096);\n"
        "    geri_al(y);");
    test_sonuc("M1: mmio_oku32(yetki<MMIO>, adres) + geri_al = 0 hata", h == 0);
}

static void M2_yaz_temel(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<MMIO> = yetki_olustur(6, 2);\n"
        "    mmio_yaz32(y, 4096, 42);\n"
        "    geri_al(y);");
    test_sonuc("M2: mmio_yaz32(yetki<MMIO>, adres, deger) + geri_al", h == 0);
}

static void M3_odunc_coklu_erisim(void) {
    /* KEY: tek yetki ile cok register erisimi — odunc, tuketmez. */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<MMIO> = yetki_olustur(6, 3);\n"
        "    de\xc4\x9fi\xc5\x9fken a: tam32 = mmio_oku32(y, 4096);\n"
        "    mmio_yaz32(y, 4100, a);\n"
        "    de\xc4\x9fi\xc5\x9fken b: tam32 = mmio_oku32(y, 4104);\n"
        "    geri_al(y);");
    test_sonuc("M3: tek yetki 3x erisim (odunc, tuketmez) = 0 hata", h == 0);
}

static void M4_yetki_olustur_mmio(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<MMIO> = yetki_olustur(6, 3);\n"
        "    geri_al(y);");
    test_sonuc("M4: yetki_olustur(6, ..) -> yetki<MMIO> uretir", h == 0);
}

static void M5_param_yetki_mmio(void) {
    static const char prog[] =
        "i\xc5\x9flev erisim(y: yetki<MMIO>) -> tam32 {\n"
        "    de\xc4\x9fi\xc5\x9fken v: tam32 = mmio_oku32(y, 4096);\n"
        "    geri_al(y);\n"
        "    ver v;\n"
        "}\n";
    int h = hata_sayisi(prog);
    test_sonuc("M5: islev parametresi yetki<MMIO>", h == 0);
}

static void M6_donus_tam32_aritmetik(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<MMIO> = yetki_olustur(6, 1);\n"
        "    de\xc4\x9fi\xc5\x9fken t: tam32 = mmio_oku32(y, 0) + mmio_oku32(y, 4);\n"
        "    geri_al(y);");
    test_sonuc("M6: mmio_oku32 donus duz tam32 (aritmetikte kullanilir)",
               h == 0);
}

static void M7_threading_distinct(void) {
    /* Cok-fonksiyonlu surucu: yetki THREAD edilir (yeni baglama her adimda). */
    static const char prog[] =
        "i\xc5\x9flev yaz(y: yetki<MMIO>, d: tam32) -> yetki<MMIO> {\n"
        "    mmio_yaz32(y, 4096, d);\n"
        "    ver y;\n"
        "}\n"
        "i\xc5\x9flev ana() {\n"
        "    de\xc4\x9fi\xc5\x9fken y: yetki<MMIO> = yetki_olustur(6, 2);\n"
        "    de\xc4\x9fi\xc5\x9fken y1: yetki<MMIO> = yaz(y, 1);\n"
        "    geri_al(y1);\n"
        "}\n";
    int h = hata_sayisi(prog);
    test_sonuc("M7: yetki<MMIO> threading (y -> y1 -> geri_al)", h == 0);
}

/* ========================================================================
 * NEGATIF (M8-M15)
 * ======================================================================== */

static void M8_yanlis_kaynak_dosya(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Dosya> = yetki_olustur(1, 1);\n"
        "    de\xc4\x9fi\xc5\x9fken v: tam32 = mmio_oku32(y, 4096);\n"
        "    geri_al(y);");
    test_sonuc("M8: mmio_oku32(yetki<Dosya>) -> MM002 (kaynak yanlis)", h >= 1);
}

static void M9_yanlis_kaynak_donanim(void) {
    /* Karar 3 izolasyonu: MMIO != Donanim (genis donanim yetkisi reddedilir). */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Donanim> = yetki_olustur(4, 7);\n"
        "    de\xc4\x9fi\xc5\x9fken v: tam32 = mmio_oku32(y, 4096);\n"
        "    geri_al(y);");
    test_sonuc("M9: mmio_oku32(yetki<Donanim>) -> MM002 (MMIO != Donanim)",
               h >= 1);
}

static void M10_oku_arg_sayisi(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<MMIO> = yetki_olustur(6, 1);\n"
        "    de\xc4\x9fi\xc5\x9fken v: tam32 = mmio_oku32(y);\n"
        "    geri_al(y);");
    test_sonuc("M10: mmio_oku32(y) tek arg -> MM001", h >= 1);
}

static void M11_yaz_arg_sayisi(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<MMIO> = yetki_olustur(6, 2);\n"
        "    mmio_yaz32(y, 4096);\n"
        "    geri_al(y);");
    test_sonuc("M11: mmio_yaz32(y, adres) 2 arg -> MM001", h >= 1);
}

static void M12_leak_geri_al_yok(void) {
    /* KEY: mmio_oku32 TUKETMEZ; geri_al yoksa yetki scope sonunda leak. */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<MMIO> = yetki_olustur(6, 1);\n"
        "    de\xc4\x9fi\xc5\x9fken v: tam32 = mmio_oku32(y, 4096);");
    test_sonuc("M12: erisim sonrasi geri_al yok -> CP005 (odunc tuketmez)",
               h >= 1);
}

static void M13_cift_geri_al(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<MMIO> = yetki_olustur(6, 1);\n"
        "    mmio_yaz32(y, 4096, 1);\n"
        "    geri_al(y);\n"
        "    geri_al(y);");
    test_sonuc("M13: cift geri_al -> CP005 (cift tuketim)", h >= 1);
}

static void M14_ilk_arg_non_yetki(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken x: tam32 = 5;\n"
        "    de\xc4\x9fi\xc5\x9fken v: tam32 = mmio_oku32(x, 4096);");
    test_sonuc("M14: mmio_oku32(tam32, ..) -> MM002 (yetki degil)", h >= 1);
}

static void M15_adres_non_integer(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<MMIO> = yetki_olustur(6, 2);\n"
        "    mmio_yaz32(y, \"abc\", 5);\n"
        "    geri_al(y);");
    test_sonuc("M15: mmio_yaz32 adres metin -> MM003", h >= 1);
}

/* ========================================================================
 * Ana
 * ======================================================================== */

int main(void) {
    FILE *eski_stderr = freopen("/dev/null", "w", stderr);
    if (!eski_stderr) {
        eski_stderr = freopen("NUL", "w", stderr);
    }
    (void)eski_stderr;

    puts("=== KEMGU MMIO Foundation — Tip Kontrol Test Paketi ===\n");

    puts("--- Pozitif (M1-M7) ---");
    M1_oku_temel(); M2_yaz_temel(); M3_odunc_coklu_erisim();
    M4_yetki_olustur_mmio(); M5_param_yetki_mmio();
    M6_donus_tam32_aritmetik(); M7_threading_distinct();

    puts("\n--- Negatif (M8-M15) ---");
    M8_yanlis_kaynak_dosya(); M9_yanlis_kaynak_donanim();
    M10_oku_arg_sayisi(); M11_yaz_arg_sayisi();
    M12_leak_geri_al_yok(); M13_cift_geri_al();
    M14_ilk_arg_non_yetki(); M15_adres_non_integer();

    printf("\n========================================\n");
    printf("Toplam: %d | Basarili: %d | Basarisiz: %d\n",
           toplam_test, basarili, basarisiz);

    return basarisiz == 0 ? 0 : 1;
}
