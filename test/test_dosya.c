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
 * KEMGU Yetki-gated Dosya I/O — Test Paketi (Faz 2 Altyapı Bootstrap)
 * ====================================================================
 *
 * Hedef: Capability Spec V1 + Linear V1 entegrasyonu ile dosya I/O.
 *
 * Built-in çağrılar (src/tip_kontrol.c Faz 2 ekleme):
 *   dosya_ac_yetkili(yol: metin, izin: tam16) -> yetki<Dosya>
 *   dosya_oku_yetkili(y: yetki<Dosya>) -> metin           (y tüketilmez)
 *   dosya_yaz_yetkili(y: yetki<Dosya>, icerik: metin) -> tam32   (y tüketilmez)
 *   dosya_kapat_yetkili(y: yetki<Dosya>) -> ()            (y tüketilir)
 *
 * Hata kodları:
 *   CP004 — capability tip uyumsuz (arity / kaynak tipi / tip)
 *   L001 — linear baglama scope sonunda tuketilmedi
 *   L002 — linear baglama iki kez tuketildi
 *   L004 — lineer e referans alma
 *   CP005 — linear yetki violation (capture/move/leak)
 *
 * Tüm testler tip-kontrol seviyesinde — runtime fopen/fclose
 * test_runtime_link.c'de yapılır.
 */

/* === Test cercevesi (Faz 1 stilinde) === */

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
 * GROUP F1-F6: Capability handle pozitif (üretici + kapama + izin)
 * ======================================================================== */

static void T1_ac_kapat_round_trip(void) {
    /* En basit yaşam döngüsü: aç + kapat */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Dosya>"
        " = dosya_ac_yetkili(\"x.txt\", 1);\n"
        "    dosya_kapat_yetkili(y);\n");
    test_sonuc("F1: dosya_ac_yetkili + dosya_kapat_yetkili round-trip = 0 hata",
               h == 0);
}

static void T2_ac_oku_kapat(void) {
    /* CP-IO: oku_yetkili yetkiyi tüketmez */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Dosya>"
        " = dosya_ac_yetkili(\"x.txt\", 1);\n"
        "    de\xc4\x9fi\xc5\x9fken icerik: metin = dosya_oku_yetkili(y);\n"
        "    dosya_kapat_yetkili(y);\n");
    test_sonuc("F2: ac + oku + kapat — y CP-IO ile tüketilmez", h == 0);
}

static void T3_ac_yaz_kapat(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Dosya>"
        " = dosya_ac_yetkili(\"x.txt\", 2);\n"
        "    de\xc4\x9fi\xc5\x9fken n: tam32"
        " = dosya_yaz_yetkili(y, \"merhaba\");\n"
        "    dosya_kapat_yetkili(y);\n");
    test_sonuc("F3: ac + yaz + kapat — y CP-IO ile tüketilmez", h == 0);
}

static void T4_ac_oku_yaz_kapat(void) {
    /* Aynı yetki ile oku ve yaz sırayla — CP-IO her ikisinde y tüketilmez */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Dosya>"
        " = dosya_ac_yetkili(\"x.txt\", 3);\n"
        "    de\xc4\x9fi\xc5\x9fken icerik: metin = dosya_oku_yetkili(y);\n"
        "    de\xc4\x9fi\xc5\x9fken n: tam32 = dosya_yaz_yetkili(y, icerik);\n"
        "    dosya_kapat_yetkili(y);\n");
    test_sonuc("F4: ac + oku + yaz + kapat — round-trip OK", h == 0);
}

static void T5_izin_oku_only(void) {
    /* IZIN_OKU = 1 sabiti global scope'ta (KEMGU sabit yalniz top-level).
     * Sabit referansi izin parametresinde kullanilir. */
    int h = hata_sayisi(
        "sabit IZIN_OKU: tam16 = 1;\n"
        "i\xc5\x9flev test() {\n"
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Dosya>"
        " = dosya_ac_yetkili(\"x.txt\", IZIN_OKU);\n"
        "    dosya_kapat_yetkili(y);\n"
        "}\n");
    test_sonuc("F5: IZIN_OKU global sabit + ac + kapat = 0 hata", h == 0);
}

static void T6_yetki_donus_tipi_inference(void) {
    /* Annotsuz değişken: yetki<Dosya> çıkarsanmalı */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y = dosya_ac_yetkili(\"x.txt\", 1);\n"
        "    dosya_kapat_yetkili(y);\n");
    test_sonuc("F6: annotsuz değişken yetki<Dosya> çıkarsanır", h == 0);
}

/* ========================================================================
 * GROUP F7-F12: Linear tüketim (L001 / L002 / L004)
 * ======================================================================== */

static void T7_kapat_iki_kez(void) {
    /* L002: y iki kez kapat → çift tüketim */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Dosya>"
        " = dosya_ac_yetkili(\"x.txt\", 1);\n"
        "    dosya_kapat_yetkili(y);\n"
        "    dosya_kapat_yetkili(y);\n");
    test_sonuc("F7: kapat_yetkili(y) iki kez -> L002 cift tuketim", h >= 1);
}

static void T8_yetki_tuketilmedi(void) {
    /* L001: y scope sonunda tüketilmedi */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Dosya>"
        " = dosya_ac_yetkili(\"x.txt\", 1);\n");
    test_sonuc("F8: yetki<Dosya> tuketilmedi -> L001 (linear leak)", h >= 1);
}

static void T9_yetki_referans_alma(void) {
    /* L004: &yetki linear referans yasagi */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Dosya>"
        " = dosya_ac_yetkili(\"x.txt\", 1);\n"
        "    de\xc4\x9fi\xc5\x9fken r = &y;\n"
        "    dosya_kapat_yetkili(y);\n");
    test_sonuc("F9: &yetki<Dosya> -> L004 (linear referans yasagi)", h >= 1);
}

static void T10_kapat_sonra_oku(void) {
    /* L002: kapat sonra oku — y tüketildi, sonra erişim hatası */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Dosya>"
        " = dosya_ac_yetkili(\"x.txt\", 1);\n"
        "    dosya_kapat_yetkili(y);\n"
        "    de\xc4\x9fi\xc5\x9fken icerik = dosya_oku_yetkili(y);\n");
    test_sonuc("F10: kapat sonra oku -> L002 (move sonrasi erisim)", h >= 1);
}

static void T11_kapat_sonra_yaz(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Dosya>"
        " = dosya_ac_yetkili(\"x.txt\", 2);\n"
        "    dosya_kapat_yetkili(y);\n"
        "    de\xc4\x9fi\xc5\x9fken n = dosya_yaz_yetkili(y, \"new\");\n");
    test_sonuc("F11: kapat sonra yaz -> L002 (move sonrasi erisim)", h >= 1);
}

static void T12_iki_yetki_bagimsiz(void) {
    /* İki ayrı yetki bağımsız olarak yönetilir — birinin tüketimi
     * diğerini etkilemez */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y1: yetki<Dosya>"
        " = dosya_ac_yetkili(\"a.txt\", 1);\n"
        "    de\xc4\x9fi\xc5\x9fken y2: yetki<Dosya>"
        " = dosya_ac_yetkili(\"b.txt\", 1);\n"
        "    dosya_kapat_yetkili(y1);\n"
        "    dosya_kapat_yetkili(y2);\n");
    test_sonuc("F12: iki bagimsiz yetki, bagimsiz tuketim = 0 hata", h == 0);
}

/* ========================================================================
 * GROUP F13-F16: Yetki kontrolü (CP004 — arity / kaynak tipi / tip uyumsuz)
 * ======================================================================== */

static void T13_ac_arity_sifir(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y = dosya_ac_yetkili();\n");
    test_sonuc("F13: dosya_ac_yetkili() 0 arg -> CP004", h >= 1);
}

static void T14_ac_arity_uc(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y = dosya_ac_yetkili(\"x\", 1, 2);\n");
    test_sonuc("F14: dosya_ac_yetkili(.,.,.) 3 arg -> CP004", h >= 1);
}

static void T15_ac_yol_tam(void) {
    /* yol tip metin olmali — tam32 yanlış */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y = dosya_ac_yetkili(42, 1);\n");
    test_sonuc("F15: dosya_ac_yetkili(tam, izin) -> CP004 (yol metin olmali)",
               h >= 1);
}

static void T16_oku_non_yetki(void) {
    /* oku_yetkili yetki<Dosya> bekler — başka tip yanlış */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken x: tam32 = 5;\n"
        "    de\xc4\x9fi\xc5\x9fken icerik = dosya_oku_yetkili(x);\n");
    test_sonuc("F16: dosya_oku_yetkili(tam) -> CP004 (yetki<Dosya> bekleniyor)",
               h >= 1);
}

/* ========================================================================
 * GROUP F17-F20: Capability + Linear entegrasyonu (CP005 + delege)
 * ======================================================================== */

static void T17_yetki_baska_kaynak_oku(void) {
    /* yetki<Soket> ile dosya_oku_yetkili — CP004 (kaynak tipi farklı) */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Soket>"
        " = yetki_olustur(2, 1);\n"
        "    de\xc4\x9fi\xc5\x9fken icerik = dosya_oku_yetkili(y);\n"
        "    geri_al(y);\n");
    test_sonuc("F17: yetki<Soket> ile dosya_oku_yetkili -> CP004", h >= 1);
}

static void T18_yetki_baska_kaynak_kapat(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Bellek>"
        " = yetki_olustur(3, 1);\n"
        "    dosya_kapat_yetkili(y);\n");
    test_sonuc("F18: yetki<Bellek> ile dosya_kapat_yetkili -> CP004", h >= 1);
}

static void T19_delege_oku_yetkisi(void) {
    /* Capability delege: tam yetki -> alt-yetki (sadece oku) */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Dosya>"
        " = dosya_ac_yetkili(\"x.txt\", 3);\n"
        "    de\xc4\x9fi\xc5\x9fken y_oku: yetki<Dosya> = delege(y, 1);\n"
        "    de\xc4\x9fi\xc5\x9fken icerik = dosya_oku_yetkili(y_oku);\n"
        "    dosya_kapat_yetkili(y_oku);\n"
        "    dosya_kapat_yetkili(y);\n");
    test_sonuc("F19: delege(y, IZIN_OKU) + oku + kapat (her ikisi) = 0 hata",
               h == 0);
}

static void T20_geri_al_alternatif(void) {
    /* geri_al(y) — kapat_yetkili'nin alternatifi (CP-GERI_AL spec semantik) */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Dosya>"
        " = dosya_ac_yetkili(\"x.txt\", 1);\n"
        "    geri_al(y);\n");
    test_sonuc("F20: geri_al(y) kapat_yetkili alternatifi = 0 hata", h == 0);
}

/* ========================================================================
 * GROUP F21-F22: Edge case (kondisyonel + yapı içinde)
 * ======================================================================== */

static void T21_kondisyon_v1_known_limit(void) {
    /* V1 KNOWN-LIMIT: Mevcut Linear V1 implementasyonu DUGUM_EGER icin
     * dal-bazli tuketim snapshot/merge yapmiyor — flag tek yonlu.
     *
     * Spec Linear V1 B.3 L-COND der ki: iki dal aynı bağlamayı tüketmeli
     * (ya ikisi tüketir ya ikisi de). Ama enforcement henuz yok; her iki
     * dal sırayla flag'i artırır → CP005 (iki kez tüketim).
     *
     * V2 hedefi: L005 LINEAR_COND_INCONSISTENT enforcement + dal-bazli
     * snapshot merge. O zaman bu test h == 0 (iki dal tutarli) olacak.
     *
     * V1 davranisi: h >= 1 (CP005 her iki dal once sirayli flag artirir). */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Dosya>"
        " = dosya_ac_yetkili(\"x.txt\", 1);\n"
        "    e\xc4\x9f" "er do\xc4\x9f" "ru {\n"
        "        dosya_kapat_yetkili(y);\n"
        "    } de\xc4\x9f" "ilse {\n"
        "        geri_al(y);\n"
        "    }\n");
    test_sonuc("F21: kosullu tuketim V1 KNOWN-LIMIT (L-COND henuz yok)",
               h >= 1);
}

static void T22_yapi_icinde_yetki_lr002(void) {
    /* LR-2: yapı linear yetki içermez — yetki<Dosya> alan = LR002 */
    int h = hata_sayisi(
        "yap\xc4\xb1 KayitTutan { y: yetki<Dosya>; }\n");
    test_sonuc("F22: yapida yetki<Dosya> alan -> LR002 (LR-2 yasak)", h >= 1);
}

/* ========================================================================
 * Main
 * ======================================================================== */

int main(void) {
    printf("=== KEMGU Yetki-gated Dosya I/O Test Paketi (Faz 2) ===\n");
    printf("Capability Spec V1 + Linear V1 entegrasyonu\n\n");

    /* F1-F6: Pozitif round-trip */
    T1_ac_kapat_round_trip();
    T2_ac_oku_kapat();
    T3_ac_yaz_kapat();
    T4_ac_oku_yaz_kapat();
    T5_izin_oku_only();
    T6_yetki_donus_tipi_inference();

    /* F7-F12: Linear tüketim */
    T7_kapat_iki_kez();
    T8_yetki_tuketilmedi();
    T9_yetki_referans_alma();
    T10_kapat_sonra_oku();
    T11_kapat_sonra_yaz();
    T12_iki_yetki_bagimsiz();

    /* F13-F16: Arity + tip kontrol */
    T13_ac_arity_sifir();
    T14_ac_arity_uc();
    T15_ac_yol_tam();
    T16_oku_non_yetki();

    /* F17-F20: Capability kaynak tipi + delege */
    T17_yetki_baska_kaynak_oku();
    T18_yetki_baska_kaynak_kapat();
    T19_delege_oku_yetkisi();
    T20_geri_al_alternatif();

    /* F21-F22: Edge case */
    T21_kondisyon_v1_known_limit();
    T22_yapi_icinde_yetki_lr002();

    printf("\n=== %d/%d test gecti (basarili) ===\n", basarili, toplam_test);
    if (basarisiz > 0) {
        printf("=== %d test BASARISIZ ===\n", basarisiz);
        return 1;
    }
    return 0;
}
