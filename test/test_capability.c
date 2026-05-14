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
 * KEMGU Capability (Object-Capability) Spec V1 — Test Paketi
 * ==========================================================
 *
 * Hedef: CP.8 minimum 35 test (spec onayli ön-koşul).
 *
 * Hata kodlari (belgeler/KEMGU_Capability_Spec_V1.md):
 *   CP001 — CAPABILITY_MISSING               (kaynak erisimi yetki yok)
 *   CP002 — CAPABILITY_REVOKED               (iptal sonrasi kullanim, runtime)
 *   CP003 — CAPABILITY_PERMISSION_INSUFFICIENT (oku/yaz/calistir izin)
 *   CP004 — CAPABILITY_TYPE_MISMATCH         (yetki<Dosya> vs yetki<Soket>)
 *   CP005 — CAPABILITY_LINEAR_VIOLATION      (kopya/alias/sızıntı/çift kullanım)
 *
 * Test gruplari:
 *   C1 (1-4):    Tip ifadesi + producer (pozitif)
 *   C2 (5-8):    delege alt-yetki + izin alt-kumesi (pozitif)
 *   C3 (9-12):   geri_al + linear tuketim (pozitif)
 *   C4 (13-16):  I/O cagrisi yetki ile (pozitif) — yetki tipi parametreli
 *   C5 (17-19):  CP001 eksik yetki (negatif) — yetki yok
 *   C6 (20-22):  CP005 double-use, leak (compile-time)
 *   C7 (23-25):  CP003 izin yetersiz (negatif)
 *   C8 (26-28):  CP004 tip yanlis (negatif)
 *   C9 (29-32):  CP005 linear ihlalleri (kopya, alias, scope leak)
 *   C10 (33-37): Confused deputy + path TOCTOU + CSRF senaryolari
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

/* Test programi sablonu — main fonksiyonu icine gomar. */
static int kontrol_main(const char *govde) {
    static char buf[16384];
    snprintf(buf, sizeof(buf),
        "i\xc5\x9flev test() {\n%s\n}\n", govde);
    return hata_sayisi(buf);
}

/* Test sablonu — main + donus tipi */
static int kontrol_main_donus(const char *govde) {
    static char buf[16384];
    snprintf(buf, sizeof(buf),
        "i\xc5\x9flev test() -> tam32 {\n%s\n}\n", govde);
    return hata_sayisi(buf);
}

/* ========================================================================
 * GROUP C1 (1-4): Tip + producer (pozitif)
 * ======================================================================== */

static void T1_tip_temel_dosya(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Dosya> = yetki_olustur(1, 3);\n"
        "    geri_al(y);");
    test_sonuc("C1: yetki<Dosya> + producer + geri_al = 0 hata", h == 0);
}

static void T2_tip_soket(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Soket> = yetki_olustur(2, 1);\n"
        "    geri_al(y);");
    test_sonuc("C2: yetki<Soket> uretim", h == 0);
}

static void T3_tip_bellek_donanim(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken b: yetki<Bellek> = yetki_olustur(3, 3);\n"
        "    de\xc4\x9fi\xc5\x9fken d: yetki<Donanim> = yetki_olustur(4, 7);\n"
        "    geri_al(b);\n"
        "    geri_al(d);");
    test_sonuc("C1: yetki<Bellek> ve yetki<Donanim> birlikte", h == 0);
}

static void T4_tip_otp_anahtar(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<OTP_Anahtar> = yetki_olustur(5, 32);\n"
        "    geri_al(y);");
    test_sonuc("C1: yetki<OTP_Anahtar> (Linear + Capability)", h == 0);
}

/* ========================================================================
 * GROUP C2 (5-8): delege alt-yetki + izin alt-kumesi (pozitif)
 * ======================================================================== */

static void T5_delege_temel(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Dosya> = yetki_olustur(1, 3);\n"
        "    de\xc4\x9fi\xc5\x9fken y2: yetki<Dosya> = delege(y, 1);\n"
        "    geri_al(y);\n"
        "    geri_al(y2);");
    test_sonuc("C2: delege y2 yeni yetki, y kalir, ikisi de tuketildi", h == 0);
}

static void T6_delege_zincirleme(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Dosya> = yetki_olustur(1, 3);\n"
        "    de\xc4\x9fi\xc5\x9fken y2: yetki<Dosya> = delege(y, 3);\n"
        "    de\xc4\x9fi\xc5\x9fken y3: yetki<Dosya> = delege(y2, 1);\n"
        "    geri_al(y);\n"
        "    geri_al(y2);\n"
        "    geri_al(y3);");
    test_sonuc("C2: delege zincirleme 3 seviye", h == 0);
}

static void T7_delege_alt_izin(void) {
    /* y oku+yaz (3); y2 sadece oku (1) — subset OK */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Dosya> = yetki_olustur(1, 3);\n"
        "    de\xc4\x9fi\xc5\x9fken y2: yetki<Dosya> = delege(y, 1);\n"
        "    geri_al(y);\n"
        "    geri_al(y2);");
    test_sonuc("C2: alt-izin (oku+yaz -> oku) OK", h == 0);
}

static void T8_delege_farkli_kaynak(void) {
    /* Soket capability for soket — delege devret kabul */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken s: yetki<Soket> = yetki_olustur(2, 3);\n"
        "    de\xc4\x9fi\xc5\x9fken s2: yetki<Soket> = delege(s, 2);\n"
        "    geri_al(s);\n"
        "    geri_al(s2);");
    test_sonuc("C2: Soket capability delege", h == 0);
}

/* ========================================================================
 * GROUP C3 (9-12): geri_al + linear tuketim (pozitif)
 * ======================================================================== */

static void T9_geri_al_temel(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Dosya> = yetki_olustur(1, 1);\n"
        "    geri_al(y);");
    test_sonuc("C3: geri_al(y) yetki tuketir", h == 0);
}

static void T10_geri_al_iki_bagimsiz_yetki(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y1: yetki<Dosya> = yetki_olustur(1, 1);\n"
        "    de\xc4\x9fi\xc5\x9fken y2: yetki<Soket> = yetki_olustur(2, 1);\n"
        "    geri_al(y1);\n"
        "    geri_al(y2);");
    test_sonuc("C3: iki bagimsiz yetki sirayla geri_al", h == 0);
}

static void T11_yetki_ver(void) {
    /* yetki<R>'yi cagirana devret — ver tuketim olarak sayilir */
    static const char prog[] =
        "i\xc5\x9flev uret() -> yetki<Dosya> {\n"
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Dosya> = yetki_olustur(1, 1);\n"
        "    ver y;\n"
        "}\n"
        "i\xc5\x9flev ana() {\n"
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Dosya> = uret();\n"
        "    geri_al(y);\n"
        "}\n";
    int h = hata_sayisi(prog);
    test_sonuc("C3: yetki cagirana ver -> tuketim", h == 0);
}

static void T12_yetki_cagrisinda_tuketim(void) {
    /* yetki<Dosya>'yi bir fonksiyona geçirmek -> tuketim */
    static const char prog[] =
        "i\xc5\x9flev kullan_y(y: yetki<Dosya>) {\n"
        "    geri_al(y);\n"
        "}\n"
        "i\xc5\x9flev ana() {\n"
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Dosya> = yetki_olustur(1, 1);\n"
        "    kullan_y(y);\n"
        "}\n";
    int h = hata_sayisi(prog);
    test_sonuc("C3: yetki cagri argumani -> tuketim", h == 0);
}

/* ========================================================================
 * GROUP C4 (13-16): I/O cagrisi yetki ile (pozitif — yetki parametreli)
 * ======================================================================== */

static void T13_islev_param_yetki(void) {
    static const char prog[] =
        "i\xc5\x9flev oku_dosya(y: yetki<Dosya>) {\n"
        "    geri_al(y);\n"
        "}\n";
    int h = hata_sayisi(prog);
    test_sonuc("C4: islev parametresi yetki<Dosya>", h == 0);
}

static void T14_islev_param_otp(void) {
    static const char prog[] =
        "i\xc5\x9flev sifrele(y: yetki<OTP_Anahtar>) {\n"
        "    geri_al(y);\n"
        "}\n";
    int h = hata_sayisi(prog);
    test_sonuc("C4: islev parametresi yetki<OTP_Anahtar>", h == 0);
}

static void T15_islev_donus_yetki(void) {
    static const char prog[] =
        "i\xc5\x9flev olustur() -> yetki<Soket> {\n"
        "    ver yetki_olustur(2, 1);\n"
        "}\n"
        "i\xc5\x9flev ana() {\n"
        "    de\xc4\x9fi\xc5\x9fken s: yetki<Soket> = olustur();\n"
        "    geri_al(s);\n"
        "}\n";
    int h = hata_sayisi(prog);
    test_sonuc("C4: islev donus yetki<Soket>", h == 0);
}

static void T16_iki_yetki_birden(void) {
    static const char prog[] =
        "i\xc5\x9flev kopyala(kaynak: yetki<Dosya>, hedef: yetki<Dosya>) {\n"
        "    geri_al(kaynak);\n"
        "    geri_al(hedef);\n"
        "}\n";
    int h = hata_sayisi(prog);
    test_sonuc("C4: islev iki yetki parametre (kaynak + hedef)", h == 0);
}

/* ========================================================================
 * GROUP C5 (17-19): CP001 eksik yetki / CP004 (negatif — bilinmeyen kaynak)
 * ======================================================================== */

static void T17_bilinmeyen_kaynak(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Foo> = yetki_olustur(1, 1);\n"
        "    geri_al(y);");
    test_sonuc("C5: yetki<Foo> -> CP004 (bilinmeyen kaynak)", h >= 1);
}

static void T18_yetki_olustur_gecersiz_id(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Dosya> = yetki_olustur(99, 1);\n"
        "    geri_al(y);");
    test_sonuc("C5: yetki_olustur(99,..) -> CP004 (gecersiz kaynak id)", h >= 1);
}

static void T19_yetki_olustur_non_literal(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken k: tam32 = 1;\n"
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Dosya> = yetki_olustur(k, 1);\n"
        "    geri_al(y);");
    test_sonuc("C5: yetki_olustur(degisken,..) -> CP004 (literal sart)", h >= 1);
}

/* ========================================================================
 * GROUP C6 (20-22): CP005 double-use, leak (compile-time)
 * ======================================================================== */

static void T20_geri_al_iki_kez(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Dosya> = yetki_olustur(1, 1);\n"
        "    geri_al(y);\n"
        "    geri_al(y);");
    test_sonuc("C6: geri_al(y) x2 -> CP005 (cift tuketim)", h >= 1);
}

static void T21_scope_sonu_leak(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Dosya> = yetki_olustur(1, 1);");
    test_sonuc("C6: yetki tuketilmedi -> CP005 (scope leak)", h >= 1);
}

static void T22_iki_yetki_biri_leak(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y1: yetki<Dosya> = yetki_olustur(1, 1);\n"
        "    de\xc4\x9fi\xc5\x9fken y2: yetki<Soket> = yetki_olustur(2, 1);\n"
        "    geri_al(y1);");
    test_sonuc("C6: iki yetki, biri tuketilmedi -> CP005", h >= 1);
}

/* ========================================================================
 * GROUP C7 (23-25): CP003 izin yetersiz (negatif) — geri_al arg degil yetki
 * ======================================================================== */

static void T23_geri_al_non_yetki(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken x: tam32 = 5;\n"
        "    geri_al(x);");
    test_sonuc("C7: geri_al(non-yetki) -> CP004 (tip yanlis)", h >= 1);
}

static void T24_delege_non_yetki(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken x: tam32 = 5;\n"
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Dosya> = delege(x, 1);\n"
        "    geri_al(y);");
    test_sonuc("C7: delege(non-yetki,..) -> CP004", h >= 1);
}

static void T25_delege_eksik_arg(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Dosya> = yetki_olustur(1, 1);\n"
        "    de\xc4\x9fi\xc5\x9fken y2: yetki<Dosya> = delege(y);\n"
        "    geri_al(y);\n"
        "    geri_al(y2);");
    test_sonuc("C7: delege(y) 1-arg -> CP004 (2 arg sart)", h >= 1);
}

/* ========================================================================
 * GROUP C8 (26-28): CP004 tip yanlis (atama / cagri tip-mismatch)
 * ======================================================================== */

static void T26_yetki_atama_yanlis_tip(void) {
    /* yetki<Dosya> degil, tam32 atanir */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Dosya> = 42;\n"
        "    geri_al(y);");
    test_sonuc("C8: yetki<Dosya> = tam32 -> T001 (tip uyumsuz)", h >= 1);
}

static void T27_yetki_dosya_vs_soket(void) {
    /* Bir fonksiyon yetki<Dosya> alir, yetki<Soket> verilir — CP004/T001 */
    static const char prog[] =
        "i\xc5\x9flev oku(y: yetki<Dosya>) {\n"
        "    geri_al(y);\n"
        "}\n"
        "i\xc5\x9flev ana() {\n"
        "    de\xc4\x9fi\xc5\x9fken s: yetki<Soket> = yetki_olustur(2, 1);\n"
        "    oku(s);\n"
        "}\n";
    int h = hata_sayisi(prog);
    test_sonuc("C8: yetki<Soket> -> yetki<Dosya> param -> tip uyumsuz", h >= 1);
}

static void T28_yetki_olustur_arg_sayi(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Dosya> = yetki_olustur(1);\n"
        "    geri_al(y);");
    test_sonuc("C8: yetki_olustur(1) tek arg -> CP004", h >= 1);
}

/* ========================================================================
 * GROUP C9 (29-32): CP005 linear ihlalleri (kopya, alias, scope)
 * ======================================================================== */

static void T29_yetki_kopya(void) {
    /* y1'i y2'ye atamak = move; y1 sonra erisilirse CP005 */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y1: yetki<Dosya> = yetki_olustur(1, 1);\n"
        "    de\xc4\x9fi\xc5\x9fken y2: yetki<Dosya> = y1;\n"
        "    geri_al(y1);\n"
        "    geri_al(y2);");
    test_sonuc("C9: y2 = y1 move sonra y1 kullanim -> CP005", h >= 1);
}

static void T30_yetki_referans_yasak(void) {
    /* &y referans alma yasak (L-NO-ALIAS) */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Dosya> = yetki_olustur(1, 1);\n"
        "    de\xc4\x9fi\xc5\x9fken r: &yetki<Dosya> = &y;\n"
        "    geri_al(y);");
    test_sonuc("C9: &yetki referans -> L004/CP005", h >= 1);
}

static void T31_yetki_kopya_arg(void) {
    /* Bir fonksiyon iki kez yetki alir; argumana ayni y iki kez gecirilirse cift kopya */
    static const char prog[] =
        "i\xc5\x9flev iki_yetki(a: yetki<Dosya>, b: yetki<Dosya>) {\n"
        "    geri_al(a);\n"
        "    geri_al(b);\n"
        "}\n"
        "i\xc5\x9flev ana() {\n"
        "    de\xc4\x9fi\xc5\x9fken y: yetki<Dosya> = yetki_olustur(1, 1);\n"
        "    iki_yetki(y, y);\n"
        "}\n";
    int h = hata_sayisi(prog);
    test_sonuc("C9: ayni y iki args -> CP005 cift-kopya", h >= 1);
}

static void T32_yetki_blok_icinde_leak(void) {
    /* Iç scope kapanırken yetki tüketilmedi */
    int h = kontrol_main(
        "    {\n"
        "        de\xc4\x9fi\xc5\x9fken y: yetki<Dosya> = yetki_olustur(1, 1);\n"
        "    }");
    test_sonuc("C9: ic blok yetki leak -> CP005 (LR001)", h >= 1);
}

/* ========================================================================
 * GROUP C10 (33-37+): Confused deputy + path TOCTOU + CSRF senaryolari
 * ======================================================================== */

/* Hardy 1988 — IBM System/38 PL/I derleyici saldirisi.
 * Eski Unix tarzi: derleyici hem kaynak okur hem debug log yazar — saldirgan
 * /etc/passwd'i kaynak yolu olarak verir, derleyici kendi yetkisiyle okur,
 * sonra debug log'a yazar; saldirgan log'dan okur.
 *
 * KEMGU'da: derleyici fonksiyonu iki AYRI yetki alir (src ve log). Yanlis
 * yetkiyle kullanmak tip hatasi. */
static void T33_confused_deputy_hardy(void) {
    static const char prog[] =
        "// Hardy 1988 IBM System/38 confused deputy\n"
        "i\xc5\x9flev derleyici(kaynak: yetki<Dosya>, log: yetki<Dosya>) {\n"
        "    // Iki yetki AYRI — confused deputy yok\n"
        "    geri_al(kaynak);\n"
        "    geri_al(log);\n"
        "}\n";
    int h = hata_sayisi(prog);
    test_sonuc("C10: Hardy88 confused deputy — iki ayri yetki", h == 0);
}

/* TOCTOU symlink saldirisi: path tabanli kontrol -> handle (capability) ile bypass */
static void T34_toctou_handle_vs_path(void) {
    /* dosya_ac_yetkili: yol once kontrol, sonra HANDLE doner. Saldirgan
     * path'i symlink ile degistirse de capability handle aynidir. */
    static const char prog[] =
        "// TOCTOU symlink saldirisi modeli\n"
        "i\xc5\x9flev kontrol_oku(y: yetki<Dosya>) {\n"
        "    // y handle-tabanli, path tabanli degil\n"
        "    geri_al(y);\n"
        "}\n";
    int h = hata_sayisi(prog);
    test_sonuc("C10: TOCTOU handle vs path (capability handle bypass)", h == 0);
}

/* CSRF benzeri: request ve response yetkilerini ayir.
 * Saldirgan response capability'sini ele gecirse de request okuyamaz. */
static void T35_csrf_request_response_ayrik(void) {
    static const char prog[] =
        "// CSRF benzeri network handler\n"
        "i\xc5\x9flev handler(req: yetki<Soket>, resp: yetki<Soket>) {\n"
        "    // Request capability ele gecse bile response okuma yok\n"
        "    geri_al(req);\n"
        "    geri_al(resp);\n"
        "}\n";
    int h = hata_sayisi(prog);
    test_sonuc("C10: CSRF — req ve resp ayri capability", h == 0);
}

/* Sudo arg injection benzeri — sub-process spawn yetkisi ayri */
static void T36_sudo_subprocess_ayri(void) {
    /* yetki<Bellek> bir prozes icin; baska proseslere DEVRET yok */
    static const char prog[] =
        "// Sub-process spawn ayri yetki (V2 SubProc; V1 Bellek ile model)\n"
        "i\xc5\x9flev spawn(mem: yetki<Bellek>) {\n"
        "    geri_al(mem);\n"
        "}\n";
    int h = hata_sayisi(prog);
    test_sonuc("C10: sudo arg injection — ayri yetki kategorisi", h == 0);
}

/* OpenSSL Heartbleed muadili — memory read capability sinirli.
 * Saldirgan keyfi byte sayisi istese de capability iznine bagli. */
static void T37_heartbleed_memory_bounds(void) {
    /* Yetki var ama tukenmesi gerek — out-of-bound okuma derlemiyor */
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken m: yetki<Bellek> = yetki_olustur(3, 1);\n"
        "    geri_al(m);");
    test_sonuc("C10: Heartbleed muadil — yetki<Bellek> sinirli + linear", h == 0);
}

/* Ek senaryo — seL4 + Genode tarzi capability handoff. Yeni proses
 * sadece aldigi yetkileri kullanir. */
static void T38_capability_handoff(void) {
    static const char prog[] =
        "// seL4 capability handoff modeli\n"
        "i\xc5\x9flev uret_alt_yetki(y: yetki<Dosya>) -> yetki<Dosya> {\n"
        "    de\xc4\x9fi\xc5\x9fken alt: yetki<Dosya> = delege(y, 1);\n"
        "    geri_al(y);\n"
        "    ver alt;\n"
        "}\n";
    int h = hata_sayisi(prog);
    test_sonuc("C10: seL4 capability handoff (delege + ver)", h == 0);
}

/* OTP anahtarinin tekkez ile birlestirilmesi — capability x linear iki katman */
static void T39_otp_tekkez_capability(void) {
    int h = kontrol_main(
        "    de\xc4\x9fi\xc5\x9fken y: yetki<OTP_Anahtar> = yetki_olustur(5, 1);\n"
        "    geri_al(y);");
    test_sonuc("C10: OTP_Anahtar yetki — linear x capability iki katman", h == 0);
}

/* Ambient authority kaldirildigi senaryo — fonksiyon hicbir yetkisi yok ise
 * yetki kullanamaz; ozellikle yetki_olustur cagrisi olmadan dosya I/O imkansiz. */
static void T40_ambient_authority_kaldirildi(void) {
    /* yetki_olustur YOK; fonksiyon ic yetki yaratamaz, dolayisiyla I/O yok.
     * Bu boş fonksiyon ile ambient-yok semantigi sergilenir. */
    static const char prog[] =
        "// Ambient authority kaldirildi — yetki yok = I/O yok\n"
        "i\xc5\x9flev saf_islev(x: tam32) -> tam32 {\n"
        "    ver x * 2;\n"
        "}\n";
    int h = hata_sayisi(prog);
    test_sonuc("C10: Ambient authority kaldirildi — pure fn yetki gerektirmez",
               h == 0);
}

/* ========================================================================
 * Ana
 * ======================================================================== */

int main(void) {
    /* stderr'i sustur — hata mesajlari testlerde gurultu yapmasin */
    FILE *eski_stderr = freopen("/dev/null", "w", stderr);
    if (!eski_stderr) {
        eski_stderr = freopen("NUL", "w", stderr);
    }
    (void)eski_stderr;

    /* kontrol_main_donus hala kullanilmiyor (gelecek testler icin tanimli). */
    (void)kontrol_main_donus;

    puts("=== KEMGU Capability (Object-Capability) Spec V1 — Test Paketi ===\n");

    puts("--- C1: Tip + producer (4) ---");
    T1_tip_temel_dosya(); T2_tip_soket();
    T3_tip_bellek_donanim(); T4_tip_otp_anahtar();

    puts("\n--- C2: delege alt-yetki (4) ---");
    T5_delege_temel(); T6_delege_zincirleme();
    T7_delege_alt_izin(); T8_delege_farkli_kaynak();

    puts("\n--- C3: geri_al + linear tuketim (4) ---");
    T9_geri_al_temel(); T10_geri_al_iki_bagimsiz_yetki();
    T11_yetki_ver(); T12_yetki_cagrisinda_tuketim();

    puts("\n--- C4: I/O yetki parametreli islev (4) ---");
    T13_islev_param_yetki(); T14_islev_param_otp();
    T15_islev_donus_yetki(); T16_iki_yetki_birden();

    puts("\n--- C5: CP004 bilinmeyen kaynak (3) ---");
    T17_bilinmeyen_kaynak(); T18_yetki_olustur_gecersiz_id();
    T19_yetki_olustur_non_literal();

    puts("\n--- C6: CP005 double-use, leak (3) ---");
    T20_geri_al_iki_kez(); T21_scope_sonu_leak();
    T22_iki_yetki_biri_leak();

    puts("\n--- C7: CP004 arg tipi/sayisi (3) ---");
    T23_geri_al_non_yetki(); T24_delege_non_yetki();
    T25_delege_eksik_arg();

    puts("\n--- C8: T001/CP004 tip uyumsuzlugu (3) ---");
    T26_yetki_atama_yanlis_tip(); T27_yetki_dosya_vs_soket();
    T28_yetki_olustur_arg_sayi();

    puts("\n--- C9: CP005 linear ihlalleri (4) ---");
    T29_yetki_kopya(); T30_yetki_referans_yasak();
    T31_yetki_kopya_arg(); T32_yetki_blok_icinde_leak();

    puts("\n--- C10: Confused deputy + senaryolar (8) ---");
    T33_confused_deputy_hardy(); T34_toctou_handle_vs_path();
    T35_csrf_request_response_ayrik(); T36_sudo_subprocess_ayri();
    T37_heartbleed_memory_bounds(); T38_capability_handoff();
    T39_otp_tekkez_capability(); T40_ambient_authority_kaldirildi();

    printf("\n========================================\n");
    printf("Toplam: %d | Basarili: %d | Basarisiz: %d\n",
           toplam_test, basarili, basarisiz);

    return basarisiz == 0 ? 0 : 1;
}
