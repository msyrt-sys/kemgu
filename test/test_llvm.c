/*
 * KEMGU LLVM Backend Entegrasyon Testleri
 *
 * Her test:
 *   1) Bir KEMGU kaynagini gecici dosyaya yazar
 *   2) `kemgu --llvm dosya.kem > dosya.ll` calistirir
 *   3) `clang -x ir dosya.ll -o dosya.exe` ile derler
 *   4) `./dosya.exe` calistirir ve exit code'u dogrular
 *
 * Bagimliliklar (runtime):
 *   - ./build/kemgu.exe PATH'te veya goreceli ./build dizininde
 *   - clang PATH'te
 */

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

/* Platform algila: Windows cmd.exe (backslash path) veya POSIX sh */
#ifdef _WIN32
#define DEV_NULL "NUL"
#define KEM_PATH ".\\build\\test_llvm_temp.kem"
#define LL_PATH ".\\build\\test_llvm_temp.ll"
#define EXE_PATH ".\\build\\test_llvm_temp.exe"
#define KEMGU_BIN ".\\build\\kemgu.exe"
#else
#define DEV_NULL "/dev/null"
#define KEM_PATH "./build/test_llvm_temp.kem"
#define LL_PATH "./build/test_llvm_temp.ll"
#define EXE_PATH "./build/test_llvm_temp.exe"
#define KEMGU_BIN "./build/kemgu"
#endif

/* Bir KEMGU programini derle ve calistir, exit code'u don.
 * Hata olursa -1 doner. */
static int derle_ve_calistir(const char *kemgu_kaynak) {
    /* fopen / / ile sorun yok — bu Windows API'sini kullanir */
    FILE *f = fopen("build/test_llvm_temp.kem", "w");
    if (!f) return -1;
    fputs(kemgu_kaynak, f);
    fclose(f);

    char komut[1024];

    /* kemgu --llvm > .ll
     * NOT: cmd.exe path'lerinde / ile baslayan token flag sayilir.
     * Backslash kullaniyoruz Windows'ta. */
    snprintf(komut, sizeof(komut),
             "%s --llvm %s > %s 2>%s",
             KEMGU_BIN, KEM_PATH, LL_PATH, DEV_NULL);
    int rc = system(komut);
    if (rc != 0) return -1;

    /* clang -x ir .ll -x none .o -o .exe; kdl_runtime.o link edilir
     * (Madde A/B/G icin metin/dizi/dosya primitifleri runtime'a baglanmali).
     * -x ir sadece .ll dosyasina uygulanmali; sonra -x none ile defaulta
     * done ki .o file format'i otomatik tanin. */
#ifdef _WIN32
    snprintf(komut, sizeof(komut),
             "clang -x ir %s -x none build\\kdl_runtime.o "
             "build\\kdl_runtime_mmio.o -o %s 2>%s",
             LL_PATH, EXE_PATH, DEV_NULL);
#else
    snprintf(komut, sizeof(komut),
             "clang -x ir %s -x none build/kdl_runtime.o "
             "build/kdl_runtime_mmio.o -o %s 2>%s",
             LL_PATH, EXE_PATH, DEV_NULL);
#endif
    rc = system(komut);
    if (rc != 0) return -1;

    /* Calistir */
    snprintf(komut, sizeof(komut), "%s", EXE_PATH);
    rc = system(komut);
    return rc;
}

/* Mevcut bir .kem dosyasini --llvm | clang ile derle + calistir, exit code
 * don. derle_ve_calistir gibi ama kaynak string yerine dosya yolu alir —
 * Turkce UTF-8 program metnini C string'e gomme zahmetini onler. */
static int derle_dosya_ve_calistir(const char *kem_yol) {
    char komut[1024];
    snprintf(komut, sizeof(komut),
             "%s --llvm %s > %s 2>%s", KEMGU_BIN, kem_yol, LL_PATH, DEV_NULL);
    if (system(komut) != 0) return -1;
#ifdef _WIN32
    snprintf(komut, sizeof(komut),
             "clang -x ir %s -x none build\\kdl_runtime.o "
             "build\\kdl_runtime_mmio.o -o %s 2>%s",
             LL_PATH, EXE_PATH, DEV_NULL);
#else
    snprintf(komut, sizeof(komut),
             "clang -x ir %s -x none build/kdl_runtime.o "
             "build/kdl_runtime_mmio.o -o %s 2>%s",
             LL_PATH, EXE_PATH, DEV_NULL);
#endif
    if (system(komut) != 0) return -1;
    snprintf(komut, sizeof(komut), "%s", EXE_PATH);
    return system(komut);
}

/* `kemgu --llvm <kem_yol>` ciktisini LLVM 'opt -passes=verify'dan gecir.
 * 1 = IR gecerli (her BB terminator'lu, dominance vs.), 0 = reddedildi.
 * C1/C2 kabul kriteri: emit edilen modul opt verifier'dan gecmeli. */
static int kemgu_llvm_opt_verify(const char *kem_yol) {
    char komut[1024];
    snprintf(komut, sizeof(komut),
             "%s --llvm %s > %s 2>%s", KEMGU_BIN, kem_yol, LL_PATH, DEV_NULL);
    if (system(komut) != 0) return 0;
    snprintf(komut, sizeof(komut),
             "opt -passes=verify -disable-output %s 2>%s", LL_PATH, DEV_NULL);
    return system(komut) == 0;
}


/* === Testler === */

/* --- C1: esles (match) deyimi codegen --- */

static void test_esles_wildcard(void) {
    /* '_' catch-all: eslesmeyen tum girdiler son dala duser. */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev g(x: tam32) -> tam32 { "
        "e\xc5\x9fle\xc5\x9f x { 5 => { ver 50; } _ => { ver 77; } } "
        "ver 0; } "
        "i\xc5\x9flev main() -> tam32 { "
        "e\xc4\x9f" "er g(5) != 50 { ver 1; } "
        "e\xc4\x9f" "er g(99) != 77 { ver 2; } ver 42; }");
    test_sonuc("esles wildcard '_' -> exit 42", rc == 42);
}

static void test_esles_nested(void) {
    /* Ic ice esles — driver status state machine deseni. */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev f(a: tam32, b: tam32) -> tam32 { "
        "e\xc5\x9fle\xc5\x9f a { "
        "0 => { e\xc5\x9fle\xc5\x9f b { 1 => { ver 11; } 2 => { ver 12; } } } "
        "1 => { e\xc5\x9fle\xc5\x9f b { 1 => { ver 21; } 2 => { ver 22; } } } } "
        "ver 0; } "
        "i\xc5\x9flev main() -> tam32 { "
        "e\xc4\x9f" "er f(0,1) != 11 { ver 1; } "
        "e\xc4\x9f" "er f(1,2) != 22 { ver 2; } "
        "e\xc4\x9f" "er f(0,9) != 0 { ver 3; } ver 42; }");
    test_sonuc("esles ic ice (driver deseni) -> exit 42", rc == 42);
}

static void test_esles_match_early_return_verify(void) {
    /* Snapshot fiksturu: her case BB ret/br ile bitmeli -> opt kabul eder. */
    int ok = kemgu_llvm_opt_verify("test/snapshots/match_early_return.kem");
    test_sonuc("esles match_early_return: opt -passes=verify PASS", ok);
}

static void test_esles_match_early_return_calistir(void) {
    /* Erken-ver dallari + tail'e dusen dallar + eslesmeyen -> dogru secim. */
    int rc = derle_dosya_ve_calistir("test/snapshots/match_early_return.kem");
    test_sonuc("esles match_early_return: arm secimi -> exit 42", rc == 42);
}

/* --- C2.5: sonuç/seçimlik value codegen --- */

static void test_sonuc_secimlik_verify(void) {
    /* tamam/hata/değer/hiç + eşleş destructuring -> tagged-union, opt temiz. */
    int ok = kemgu_llvm_opt_verify("test/snapshots/sonuc_secimlik.kem");
    test_sonuc("sonuc/secimlik: opt -passes=verify PASS", ok);
}

static void test_sonuc_secimlik_calistir(void) {
    /* tamam/hata round-trip + değer/hiç + payload binding -> exit 42. */
    int rc = derle_dosya_ve_calistir("test/snapshots/sonuc_secimlik.kem");
    test_sonuc("sonuc/secimlik: tamam/hata/deger/hic -> exit 42", rc == 42);
}

static void test_sonuc_struct_payload_verify(void) {
    int ok = kemgu_llvm_opt_verify("test/snapshots/sonuc_struct_payload.kem");
    test_sonuc("sonuc struct payload: opt -passes=verify PASS", ok);
}

static void test_sonuc_struct_payload_calistir(void) {
    /* struct payload (by-value) + sonuç-as-parameter ABI -> exit 42. */
    int rc = derle_dosya_ve_calistir("test/snapshots/sonuc_struct_payload.kem");
    test_sonuc("sonuc struct payload + param ABI -> exit 42", rc == 42);
}

/* --- C2.6: cross-file fonksiyon cagrisi (transitif kullan) --- */

static void test_crossfile_transitif_verify(void) {
    /* transitif -> lib_islem -> lib_sayi; iki_kat transitif yuklenmeli. */
    int ok = kemgu_llvm_opt_verify("test/crossfile/transitif.kem");
    test_sonuc("crossfile transitif: opt -passes=verify PASS", ok);
}

static void test_crossfile_transitif_calistir(void) {
    int rc = derle_dosya_ve_calistir("test/crossfile/transitif.kem");
    test_sonuc("crossfile transitif (uc_kat(14)) -> exit 42", rc == 42);
}

static void test_crossfile_sonuc_verify(void) {
    /* cross-file sonuç dönüşlü çağrı — C2.5 tagged-union ABI uyumu. */
    int ok = kemgu_llvm_opt_verify("test/crossfile/sonuc_cagri.kem");
    test_sonuc("crossfile sonuc ABI: opt -passes=verify PASS", ok);
}

static void test_crossfile_sonuc_calistir(void) {
    int rc = derle_dosya_ve_calistir("test/crossfile/sonuc_cagri.kem");
    test_sonuc("crossfile sonuc donuslu cagri -> exit 42", rc == 42);
}

/* --- C2.7: çeşit (custom sum type) + exhaustiveness --- */

/* `kemgu --check <kem_yol>` başarılı mı? 1 = OK (exit 0), 0 = hata. */
static int kemgu_check_basarili(const char *kem_yol) {
    char komut[1024];
    snprintf(komut, sizeof(komut),
             "%s --check %s > %s 2>%s", KEMGU_BIN, kem_yol, DEV_NULL, DEV_NULL);
    return system(komut) == 0;
}

static void test_cesit_temel_verify(void) {
    int ok = kemgu_llvm_opt_verify("test/snapshots/cesit_temel.kem");
    test_sonuc("cesit temel: opt -passes=verify PASS", ok);
}

static void test_cesit_temel_calistir(void) {
    /* Ad::Varyant inşa + eşleş destructuring (i8 discriminant). */
    int rc = derle_dosya_ve_calistir("test/snapshots/cesit_temel.kem");
    test_sonuc("cesit temel (Ad::Varyant + eslesme) -> exit 42", rc == 42);
}

static void test_cesit_sonuc_verify(void) {
    int ok = kemgu_llvm_opt_verify("test/snapshots/cesit_sonuc.kem");
    test_sonuc("cesit sonuc (D6 sonuc<bos,cesit>): opt verify PASS", ok);
}

static void test_cesit_sonuc_calistir(void) {
    /* D6: hata(Cesit::V) bir-seviye nesting (tag==hata AND disc==idx). */
    int rc = derle_dosya_ve_calistir("test/snapshots/cesit_sonuc.kem");
    test_sonuc("cesit sonuc D6 (hata(Cesit::V) nesting) -> exit 42", rc == 42);
}

static void test_cesit_exhaustive_negatif(void) {
    /* Eksik varyant -> --check M001 ile BAŞARISIZ olmalı. */
    int ok = kemgu_check_basarili("test/snapshots/cesit_eksik.kem");
    test_sonuc("cesit non-exhaustive -> --check basarisiz (M001)", ok == 0);
}

static void test_lit_42(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { ver 42; }");
    test_sonuc("ver 42 -> exit 42", rc == 42);
}

static void test_aritmetik(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { ver 1 + 2 * 3 + 35; }");
    test_sonuc("1 + 2*3 + 35 -> exit 42", rc == 42);
}

static void test_tekli_neg(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { ver 100 + (0 - 58); }");
    test_sonuc("100 + (-58) -> exit 42", rc == 42);
}

static void test_kiyaslama_lt(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "e\xc4\x9f" "er 3 < 5 { ver 1; } ver 0; }");
    test_sonuc("3 < 5 -> exit 1", rc == 1);
}

static void test_kiyaslama_eq(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "e\xc4\x9f" "er 5 == 5 { ver 1; } ver 0; }");
    test_sonuc("5 == 5 -> exit 1", rc == 1);
}

static void test_mantiksal_ve(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "e\xc4\x9f" "er 1 > 0 ve 2 > 0 { ver 1; } ver 0; }");
    test_sonuc("1>0 ve 2>0 -> exit 1", rc == 1);
}

static void test_mantiksal_degil(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "e\xc4\x9f" "er de\xc4\x9fil (1 == 2) { ver 1; } ver 0; }");
    test_sonuc("degil (1==2) -> exit 1", rc == 1);
}

static void test_islev_cagrisi(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev iki_kat(n: tam32) -> tam32 { ver n * 2; } "
        "i\xc5\x9flev main() -> tam32 { ver iki_kat(21); }");
    test_sonuc("iki_kat(21) -> exit 42", rc == 42);
}

static void test_recursive_fib(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev fib(n: tam32) -> tam32 { "
        "e\xc4\x9f" "er n < 2 { ver n; } "
        "ver fib(n - 1) + fib(n - 2); } "
        "i\xc5\x9flev main() -> tam32 { ver fib(10); }");
    test_sonuc("fib(10) -> exit 55", rc == 55);
}

static void test_iken_dongusu(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken i = 0; "
        "iken i < 10 { i = i + 1; } "
        "ver i; }");
    test_sonuc("iken 0..10 -> exit 10", rc == 10);
}

static void test_lokal_degisken(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken x = 10; "
        "de\xc4\x9fi\xc5\x9fken y = 32; "
        "ver x + y; }");
    test_sonuc("lokal x=10, y=32, x+y -> exit 42", rc == 42);
}

static void test_parametre_atamasi(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev test(n: tam32) -> tam32 { "
        "n = n * 2; "
        "ver n; } "
        "i\xc5\x9flev main() -> tam32 { ver test(21); }");
    test_sonuc("parametre atamasi (n=n*2) -> exit 42", rc == 42);
}

static void test_faktoriyel(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev fac(n: tam32) -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken s = 1; "
        "de\xc4\x9fi\xc5\x9fken i = 1; "
        "iken i <= n { s = s * i; i = i + 1; } "
        "ver s; } "
        "i\xc5\x9flev main() -> tam32 { ver fac(5); }");
    test_sonuc("faktoriyel 5! -> exit 120", rc == 120);
}

static void test_gcd(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev gcd(a: tam32, b: tam32) -> tam32 { "
        "iken b != 0 { de\xc4\x9fi\xc5\x9fken t = b; b = a % b; a = t; } "
        "ver a; } "
        "i\xc5\x9flev main() -> tam32 { ver gcd(48, 36); }");
    test_sonuc("gcd(48, 36) -> exit 12", rc == 12);
}

static void test_eger_else(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev mutlak(x: tam32) -> tam32 { "
        "e\xc4\x9f" "er x < 0 { ver 0 - x; } "
        "de\xc4\x9filse { ver x; } } "
        "i\xc5\x9flev main() -> tam32 { ver mutlak(0 - 42); }");
    test_sonuc("mutlak(-42) -> exit 42", rc == 42);
}

static void test_iki_islev(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev kare(n: tam32) -> tam32 { ver n * n; } "
        "i\xc5\x9flev kup(n: tam32) -> tam32 { ver n * kare(n); } "
        "i\xc5\x9flev main() -> tam32 { ver kup(3) + kare(3); }");
    /* 3^3 + 3^2 = 27 + 9 = 36 */
    test_sonuc("kup(3) + kare(3) -> exit 36", rc == 36);
}

/* === Multi-int testleri === */

static void test_tam8(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev hesap(a: tam8, b: tam8) -> tam8 { ver a + b; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken r: tam8 = hesap(20, 22); "
        "de\xc4\x9fi\xc5\x9fken son: tam32 = 0; "
        "son = son + 42; "
        "ver son; }");
    test_sonuc("tam8 + tam8 -> 42 (i8 add)", rc == 42);
}

static void test_tam64(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev hesap(a: tam64) -> tam64 { ver a * 2; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken k: tam64 = hesap(21); "
        "de\xc4\x9fi\xc5\x9fken r: tam32 = 0; "
        "r = r + 42; "
        "ver r; }");
    test_sonuc("tam64 -> 42 (i64 mul)", rc == 42);
}

static void test_mantiksal_param(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev test(b: mant\xc4\xb1ksal) -> tam32 { "
        "e\xc4\x9f" "er b { ver 42; } "
        "ver 0; } "
        "i\xc5\x9flev main() -> tam32 { ver test(do\xc4\x9fru); }");
    test_sonuc("mantiksal param (i1) -> 42", rc == 42);
}

/* === Metin literali testleri === */

static void test_metin_donus(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev selam() -> metin { ver \"Merhaba\"; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken s = selam(); "
        "ver 42; }");
    test_sonuc("metin donus + lokal -> 42", rc == 42);
}

static void test_metin_global(void) {
    /* Sadece IR'nin string global'i icermesi yeterli — runtime'da
     * tasinmasi gerek degil. Run sonucu 0 (basit). */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken s = \"Dunya\"; "
        "ver 0; }");
    test_sonuc("metin global olusumu", rc == 0);
}

/* === Yapi testleri === */

static void test_yapi_temel(void) {
    int rc = derle_ve_calistir(
        "yap\xc4\xb1 Nokta { x: tam32; y: tam32; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken n = Nokta { x: 10, y: 32 }; "
        "ver n.x + n.y; }");
    test_sonuc("yapi olustur + alan erisim (10 + 32 = 42)", rc == 42);
}

static void test_yapi_atama(void) {
    int rc = derle_ve_calistir(
        "yap\xc4\xb1 Kutu { v: tam32; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken k = Kutu { v: 100 }; "
        "ver k.v - 58; }");
    test_sonuc("yapi alan kullanim (100 - 58 = 42)", rc == 42);
}

static void test_yapi_coklu_alan(void) {
    int rc = derle_ve_calistir(
        "yap\xc4\xb1 V { a: tam32; b: tam32; c: tam32; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken v = V { a: 10, b: 20, c: 12 }; "
        "ver v.a + v.b + v.c; }");
    test_sonuc("yapi 3 alan toplam (10+20+12=42)", rc == 42);
}

/* === ADIM 22: LLVM v3 — float/double + dizi + struct-by-value === */

static void test_kesirli64(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev hesap(x: kesirli64) -> kesirli64 { ver x * 2.0; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken r: kesirli64 = hesap(21.0); "
        "de\xc4\x9fi\xc5\x9fken son: tam32 = 0; "
        "son = son + 42; "
        "ver son; }");
    test_sonuc("kesirli64 (double) hesap(21.0) * 2.0 -> 42", rc == 42);
}

static void test_kesirli32(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev hesap(x: kesirli32) -> kesirli32 { ver x + 21.0; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken r: kesirli32 = hesap(21.0); "
        "ver 42; }");
    test_sonuc("kesirli32 (float) hesap(21.0) + 21.0 -> 42 (statik)", rc == 42);
}

static void test_dizi_temel(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken xs = [10, 20, 12]; "
        "ver xs[0] + xs[1] + xs[2]; }");
    test_sonuc("dizi [10,20,12] xs[0]+xs[1]+xs[2] -> 42", rc == 42);
}

static void test_dizi_dongu(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken xs = [1, 2, 3, 4, 5]; "
        "de\xc4\x9fi\xc5\x9fken s: tam32 = 0; "
        "de\xc4\x9fi\xc5\x9fken i: tam32 = 0; "
        "iken i < 5 { s = s + xs[i]; i = i + 1; } "
        "ver s + 27; }");
    /* 1+2+3+4+5 = 15, + 27 = 42 */
    test_sonuc("dizi[i] dongu icinde toplam -> 42", rc == 42);
}

static void test_struct_param_by_value(void) {
    int rc = derle_ve_calistir(
        "yap\xc4\xb1 Cift { a: tam32; b: tam32; } "
        "i\xc5\x9flev topla(c: Cift) -> tam32 { ver c.a + c.b; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken c = Cift { a: 20, b: 22 }; "
        "ver topla(c); }");
    test_sonuc("struct-by-value param (Cift{20,22}) -> 42", rc == 42);
}

/* === ADIM 23: Generic işlev + stdlib === */

static void test_generic_kimlik(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev kimlik<T>(x: T) -> T { ver x; } "
        "i\xc5\x9flev main() -> tam32 { ver kimlik(42); }");
    test_sonuc("generic kimlik<T>(42) -> 42", rc == 42);
}

static void test_generic_iki_kat(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev iki_kat<T>(x: T) -> T { ver x + x; } "
        "i\xc5\x9flev main() -> tam32 { ver iki_kat(21); }");
    test_sonuc("generic iki_kat<T>(21) -> 42", rc == 42);
}

static void test_generic_coklu_instan(void) {
    /* Iki ayri tip ile iki ayri instantiation */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev kimlik<T>(x: T) -> T { ver x; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken a: tam8 = kimlik(20); "
        "de\xc4\x9fi\xc5\x9fken b: tam32 = 0; "
        "b = b + 22; "
        "ver b + 20; }");
    /* a tam8'ten gelirken default i32 kullanildigi icin sext gerek; sonuc 42 */
    test_sonuc("generic kimlik coklu tip instantiation", rc == 42);
}

static void test_bellek_al_serbest(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken p: metin = bellek_al(100); "
        "bellek_serbest(p); "
        "ver 42; }");
    test_sonuc("bellek_al(100) + bellek_serbest -> exit 42", rc == 42);
}

static void test_yazdir_hello(void) {
    /* yazdir("...") -> puts. Exit kodu yalniz "puts" donus degeri (genelde > 0) */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "yazdir(\"hi\"); "
        "ver 42; }");
    test_sonuc("yazdir(\"hi\") + ver 42 -> exit 42", rc == 42);
}

/* === A: Metin runtime primitifleri === */

static void test_metin_uzunluk(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { ver metin_uzunluk(\"merhaba\"); }");
    test_sonuc("metin_uzunluk(\"merhaba\") -> 7", rc == 7);
}

static void test_metin_uzunluk_bos(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { ver metin_uzunluk(\"\"); }");
    test_sonuc("metin_uzunluk(\"\") -> 0", rc == 0);
}

static void test_metin_birlestir(void) {
    /* "ab" + "cdef" -> 6 byte */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken s: metin = metin_birlestir(\"ab\", \"cdef\"); "
        "ver metin_uzunluk(s); }");
    test_sonuc("metin_birlestir(\"ab\",\"cdef\") uzunluk -> 6", rc == 6);
}

static void test_metin_kes(void) {
    /* metin_kes("merhaba", 0, 3) = "mer" -> uzunluk 3 */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken s: metin = metin_kes(\"merhaba\", 0, 3); "
        "ver metin_uzunluk(s); }");
    test_sonuc("metin_kes(\"merhaba\",0,3) uzunluk -> 3", rc == 3);
}

static void test_metin_kucuk_ascii(void) {
    /* metin_kucuk("ABC") uzunluk 3 (ASCII lowercase) */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken s: metin = metin_kucuk(\"ABC\"); "
        "ver metin_uzunluk(s); }");
    test_sonuc("metin_kucuk(\"ABC\") uzunluk -> 3", rc == 3);
}

static void test_metin_buyuk_turkce_i(void) {
    /* 'i' (1 byte) -> "İ" (2 byte U+0130 C4 B0) — uzunluk = 2 */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken s: metin = metin_buyuk(\"i\"); "
        "ver metin_uzunluk(s); }");
    test_sonuc("metin_buyuk(\"i\") -> Turkce İ (2 byte)", rc == 2);
}

/* === Adim 2: Turkce-aware case folding === */

static void test_metin_kucuk_tr_buyuk_I(void) {
    /* I (1 byte) -> ı (2 byte: \xc4\xb1) — uzunluk = 2 */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "ver metin_uzunluk(metin_kucuk_tr(\"I\")); }");
    test_sonuc("metin_kucuk_tr(\"I\") -> ı (2 byte)", rc == 2);
}

static void test_metin_buyuk_tr_kucuk_i(void) {
    /* i -> İ (2 byte) */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "ver metin_uzunluk(metin_buyuk_tr(\"i\")); }");
    test_sonuc("metin_buyuk_tr(\"i\") -> İ (2 byte)", rc == 2);
}

static void test_metin_ascii_I_kalir(void) {
    /* ASCII variant: I -> i (1 byte), Turkce I/ı yok */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "ver metin_uzunluk(metin_kucuk_ascii(\"I\")); }");
    test_sonuc("metin_kucuk_ascii(\"I\") -> i (1 byte, ASCII saf)", rc == 1);
}

static void test_metin_ascii_turkce_korunur(void) {
    /* ASCII variant Turkce karakteri olduğu gibi bırakır */
    /* "Aİ" 3 byte (A + İ olarak \xc4\xb0). metin_kucuk_ascii ile A->a kalir,
     * İ degismez -> "aİ" 3 byte */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "ver metin_uzunluk(metin_kucuk_ascii(\"A\xc4\xb0\")); }");
    test_sonuc("metin_kucuk_ascii(\"Aİ\") Turkce kalir (3 byte)", rc == 3);
}

static void test_metin_tr_yuvarlak_yolculuk(void) {
    /* I -> ı -> I roundtrip uzunluk: I (1) -> ı (2) -> I (1) -> 1 byte */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "ver metin_uzunluk(metin_buyuk_tr(metin_kucuk_tr(\"I\"))); }");
    test_sonuc("metin_buyuk_tr(metin_kucuk_tr(\"I\")) round-trip -> 1", rc == 1);
}

static void test_metin_tr_turkce_c(void) {
    /* C (1 byte) -> c (1 byte ASCII). Cc kucuk verir */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "ver metin_uzunluk(metin_kucuk_tr(\"Cc\")); }");
    test_sonuc("metin_kucuk_tr(\"Cc\") -> 2 byte", rc == 2);
}

/* === Adim 3: Dizi literal heap allocation === */

static void test_heap_dizi_literal_uzunluk(void) {
    /* değişken d: Dizi<tam32> = [10,20,12] heap; dizi_boyut -> 3 */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken d: Dizi<tam32> = [10, 20, 12]; "
        "ver dizi_boyut(d); }");
    test_sonuc("heap dizi literal boyut -> 3", rc == 3);
}

static void test_heap_dizi_literal_indeks(void) {
    /* d[0]+d[1]+d[2] heap, kdl_dizi_al ile route */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken d: Dizi<tam32> = [10, 20, 12]; "
        "ver d[0] + d[1] + d[2]; }");
    test_sonuc("heap dizi literal indeks -> 42", rc == 42);
}

static void test_heap_dizi_literal_tam64(void) {
    /* tam64 elemanli heap dizi */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken d: Dizi<tam64> = [100, 200, 300]; "
        "ver dizi_boyut(d); }");
    test_sonuc("heap dizi tam64 literal boyut -> 3", rc == 3);
}

static void test_heap_dizi_buyume(void) {
    /* Heap dizi literal + sonra ekle (auto-grow) */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken d: Dizi<tam32> = [1, 2, 3]; "
        "dizi_ekle(d, 4); dizi_ekle(d, 5); "
        "ver dizi_boyut(d); }");
    test_sonuc("heap dizi literal + dizi_ekle -> 5", rc == 5);
}

static void test_stack_dizi_korunur(void) {
    /* Annot yok -> stack davranisi (alloca [N x T]) */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken d = [10, 20, 12]; "
        "ver d[0] + d[1] + d[2]; }");
    test_sonuc("stack dizi (annot yok) -> 42", rc == 42);
}

static void test_heap_stack_ayrim(void) {
    /* Iki dizi, biri stack (no annot) biri heap (annot Dizi<tam32>).
     * Heap dizi_boyut destekli, stack alloca'lı. */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken h: Dizi<tam32> = [1, 2, 3]; "
        "de\xc4\x9fi\xc5\x9fken s = [10, 20]; "
        "ver dizi_boyut(h) + s[0]; }");
    test_sonuc("heap+stack karma: 3 + 10 -> 13", rc == 13);
}

static void test_heap_dizi_indeks_zincir(void) {
    /* d[0] * d[1] + d[2] */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken d: Dizi<tam32> = [7, 6, 0]; "
        "ver d[0] * d[1] + d[2]; }");
    test_sonuc("heap dizi indeks aritmetik -> 42", rc == 42);
}

static void test_heap_dizi_uzun(void) {
    /* 5 elemanli heap dizi, sondan oku */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken d: Dizi<tam32> = [1, 2, 3, 4, 42]; "
        "ver d[4]; }");
    test_sonuc("heap dizi uzun (5 eleman) d[4] -> 42", rc == 42);
}

/* === Adim 7: Stdlib bağlama end-to-end === */

static void test_stdlib_harita_calistir(void) {
    /* Inline harita: dizi_olustur + dizi_ekle + f(x) */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev iki_kat(x: tam32) -> tam32 { ver x * 2; } "
        "i\xc5\x9flev harita<T, U>(xs: Dizi<T>, f: i\xc5\x9flev(T) -> U) "
        "-> Dizi<U> { "
        "de\xc4\x9fi\xc5\x9fken r: Dizi<U> = dizi_olustur(0); "
        "i\xc3\xa7in x: xs { dizi_ekle(r, f(x)); } "
        "ver r; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken xs: Dizi<tam32> = [10, 11]; "
        "de\xc4\x9fi\xc5\x9fken ys: Dizi<tam32> = harita(xs, iki_kat); "
        "ver dizi_al(ys, 0) + dizi_al(ys, 1); }");
    /* iki_kat(10)+iki_kat(11) = 20+22 = 42 */
    test_sonuc("stdlib harita end-to-end -> 42", rc == 42);
}

static void test_stdlib_filtre_calistir(void) {
    /* Inline filtre: pred(x) -> dizi_ekle */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev cift_mi(x: tam32) -> mant\xc4\xb1ksal { ver x % 2 == 0; } "
        "i\xc5\x9flev filtre<T>(xs: Dizi<T>, p: i\xc5\x9flev(T) -> mant\xc4\xb1ksal) "
        "-> Dizi<T> { "
        "de\xc4\x9fi\xc5\x9fken r: Dizi<T> = dizi_olustur(0); "
        "i\xc3\xa7in x: xs { e\xc4\x9f" "er p(x) { dizi_ekle(r, x); } } "
        "ver r; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken xs: Dizi<tam32> = [1, 2, 3, 40, 5]; "
        "de\xc4\x9fi\xc5\x9fken cs: Dizi<tam32> = filtre(xs, cift_mi); "
        "ver dizi_al(cs, 0) + dizi_al(cs, 1); }");
    /* cift'ler: 2, 40 -> 42 */
    test_sonuc("stdlib filtre end-to-end -> 42", rc == 42);
}

static void test_stdlib_indirgeme_calistir(void) {
    /* Inline indirgeme: birikim */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev topla(a: tam32, b: tam32) -> tam32 { ver a + b; } "
        "i\xc5\x9flev indirgeme<T, U>(xs: Dizi<T>, b: U, "
        "op: i\xc5\x9flev(U, T) -> U) -> U { "
        "de\xc4\x9fi\xc5\x9fken s: U = b; "
        "i\xc3\xa7in x: xs { s = op(s, x); } "
        "ver s; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken xs: Dizi<tam32> = [10, 12, 20]; "
        "ver indirgeme(xs, 0, topla); }");
    /* 0+10+12+20 = 42 */
    test_sonuc("stdlib indirgeme end-to-end -> 42", rc == 42);
}

static void test_stdlib_metin_kucukbuyuk(void) {
    /* metin_buyuk_tr ile kucukharf upper case round-trip */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken s: metin = \"abc\"; "
        "de\xc4\x9fi\xc5\x9fken bs: metin = metin_buyuk(s); "
        "ver metin_uzunluk(bs); }");
    /* ABC uzunlugu 3 */
    test_sonuc("stdlib metin buyuk ASCII end-to-end -> 3", rc == 3);
}

static void test_stdlib_dizi_uzunluk(void) {
    /* dizi_boyut heap dizi ile */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken xs: Dizi<tam32> = [1, 2, 3, 4, 5, 6, 7, 42]; "
        "ver dizi_boyut(xs); }");
    /* 8 eleman */
    test_sonuc("stdlib dizi uzunluk end-to-end -> 8", rc == 8);
}

static void test_metin_icerir_evet(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "e\xc4\x9f""er metin_icerir(\"merhaba\", \"haba\") { ver 1; } "
        "ver 0; }");
    test_sonuc("metin_icerir(\"merhaba\",\"haba\") -> 1", rc == 1);
}

static void test_metin_icerir_hayir(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "e\xc4\x9f""er metin_icerir(\"merhaba\", \"xyz\") { ver 1; } "
        "ver 0; }");
    test_sonuc("metin_icerir(\"merhaba\",\"xyz\") -> 0", rc == 0);
}

static void test_metin_baslar(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "e\xc4\x9f""er metin_baslar(\"merhaba\", \"mer\") { ver 1; } "
        "ver 0; }");
    test_sonuc("metin_baslar(\"merhaba\",\"mer\") -> 1", rc == 1);
}

static void test_metin_biter(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "e\xc4\x9f""er metin_biter(\"merhaba\", \"aba\") { ver 1; } "
        "ver 0; }");
    test_sonuc("metin_biter(\"merhaba\",\"aba\") -> 1", rc == 1);
}

static void test_metin_kirp(void) {
    /* "  abc  " -> "abc" (uzunluk 3) */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken s: metin = metin_kirp(\"  abc  \"); "
        "ver metin_uzunluk(s); }");
    test_sonuc("metin_kirp(\"  abc  \") uzunluk -> 3", rc == 3);
}

static void test_metin_yer_degistir(void) {
    /* "aaa" -> "bbb" (a -> b), uzunluk 3 */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken s: metin = metin_yer_degistir(\"aaa\", \"a\", \"b\"); "
        "ver metin_uzunluk(s); }");
    test_sonuc("metin_yer_degistir(\"aaa\",\"a\",\"b\") uzunluk -> 3", rc == 3);
}

static void test_kendin_method(void) {
    int rc = derle_ve_calistir(
        "yap\xc4\xb1 K { v: tam32; } "
        "uygula K { "
        "i\xc5\x9flev al(kendin) -> tam32 { ver kendin.v; } } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken k = K { v: 42 }; "
        "ver k.al(); }");
    test_sonuc("kendin (self) method dispatch -> 42", rc == 42);
}

static void test_kendin_arg_ile(void) {
    int rc = derle_ve_calistir(
        "yap\xc4\xb1 K { v: tam32; } "
        "uygula K { "
        "i\xc5\x9flev topla(kendin, x: tam32) -> tam32 { ver kendin.v + x; } } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken k = K { v: 20 }; "
        "ver k.topla(22); }");
    test_sonuc("kendin + arg method -> 42", rc == 42);
}

static void test_generic_mutlak_stdlib(void) {
    /* matematik.kem'den mutlak<T> kullanim */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev mutlak<T>(x: T) -> T { "
        "e\xc4\x9f" "er x < 0 { ver 0 - x; } "
        "ver x; } "
        "i\xc5\x9flev main() -> tam32 { ver mutlak(0 - 42); }");
    test_sonuc("stdlib stil mutlak<T>(-42) -> 42", rc == 42);
}

static void test_struct_donus_by_value(void) {
    int rc = derle_ve_calistir(
        "yap\xc4\xb1 N { x: tam32; } "
        "i\xc5\x9flev yap() -> N { ver N { x: 42 }; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken n = yap(); "
        "ver n.x; }");
    test_sonuc("struct-by-value donus (yap() -> N{42}) -> 42", rc == 42);
}

/* === Bit Operatorleri Testleri (ADIM 30) === */

static void test_bit_ve_temel(void) {
    /* 42 & 63 = 42 (42 = 0b101010 ⊂ 0b111111) */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { ver 42 & 63; }");
    test_sonuc("42 & 63 -> 42 (bit AND temel)", rc == 42);
}

static void test_bit_ve_mask(void) {
    /* 42 & 0xFF (255) = 42 (alt 8 bit mask) */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { ver 42 & 255; }");
    test_sonuc("42 & 255 -> 42 (mask alt 8 bit)", rc == 42);
}

static void test_bit_veya_kombine(void) {
    /* 40 | 2 = 42 (0b101000 | 0b000010 = 0b101010) */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { ver 40 | 2; }");
    test_sonuc("40 | 2 -> 42 (bit OR kombine)", rc == 42);
}

static void test_bit_veya_identity(void) {
    /* 0 | 42 = 42 (OR sifir nokta) */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { ver 0 | 42; }");
    test_sonuc("0 | 42 -> 42 (OR identity)", rc == 42);
}

static void test_bit_ozveya_temel(void) {
    /* 42 ^ 0 = 42 (XOR identity) */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { ver 42 ^ 0; }");
    test_sonuc("42 ^ 0 -> 42 (XOR identity)", rc == 42);
}

static void test_bit_ozveya_self_inverse(void) {
    /* (42 ^ 0xAB) ^ 0xAB = 42 (XOR self-inverse) */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { ver (42 ^ 171) ^ 171; }");
    test_sonuc("(42 ^ 171) ^ 171 -> 42 (XOR self-inverse)", rc == 42);
}

static void test_sola_kaydir_temel(void) {
    /* 21 << 1 = 42 */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { ver 21 << 1; }");
    test_sonuc("21 << 1 -> 42 (shift left)", rc == 42);
}

static void test_sola_kaydir_sifir(void) {
    /* 42 << 0 = 42 (no-op) */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { ver 42 << 0; }");
    test_sonuc("42 << 0 -> 42 (shift 0 no-op)", rc == 42);
}

static void test_saga_kaydir_temel(void) {
    /* 84 >> 1 = 42 */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { ver 84 >> 1; }");
    test_sonuc("84 >> 1 -> 42 (arith shift right)", rc == 42);
}

static void test_saga_kaydir_buyuk(void) {
    /* 168 >> 2 = 42 */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { ver 168 >> 2; }");
    test_sonuc("168 >> 2 -> 42 (shift right 2)", rc == 42);
}

static void test_bit_degil_double(void) {
    /* ~(~42) = 42 (double NOT) */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { ver ~(~42); }");
    test_sonuc("~(~42) -> 42 (double bitwise NOT)", rc == 42);
}

static void test_bit_kompozisyon(void) {
    /* (1 << 5) | (1 << 3) | 2 = 32 | 8 | 2 = 42 (bit composition) */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { ver (1 << 5) | (1 << 3) | 2; }");
    test_sonuc("(1<<5) | (1<<3) | 2 -> 42 (kompozisyon)", rc == 42);
}

static void test_bit_oncelik_shift_or(void) {
    /* 5 << 3 | 2 = (5 << 3) | 2 = 40 | 2 = 42 (shift > bit OR) */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { ver 5 << 3 | 2; }");
    test_sonuc("5 << 3 | 2 -> 42 (oncelik: shift > OR)", rc == 42);
}

/* (Madde A duplicate test functions removed — bu oturumda yukarida zaten
 *  test_metin_uzunluk, _birlestir, _kes, _kucuk_ascii, _buyuk_turkce_i,
 *  _icerir_evet/_hayir, _baslar, _biter, _kirp, _yer_degistir mevcut.) */

/* === Madde B: Dizi dinamik allocator (generic intrinsics) === */

static void test_dizi_olustur_ekle_boyut(void) {
    /* dizi_olustur<T> generic + 3 ekle -> boyut 3 */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken d: Dizi<tam32> = dizi_olustur(4); "
        "dizi_ekle(d, 10); "
        "dizi_ekle(d, 20); "
        "dizi_ekle(d, 30); "
        "ver dizi_boyut(d); }");
    test_sonuc("dizi_olustur<tam32> + 3 ekle -> boyut 3", rc == 3);
}

static void test_dizi_al(void) {
    /* dizi_al index 1 = 20 */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken d: Dizi<tam32> = dizi_olustur(4); "
        "dizi_ekle(d, 10); "
        "dizi_ekle(d, 20); "
        "dizi_ekle(d, 30); "
        "ver dizi_al(d, 1); }");
    test_sonuc("dizi_al<tam32>(d, 1) -> 20", rc == 20);
}

static void test_dizi_kapasite_otomatik_buyume(void) {
    /* Kapasite 2, 5 eleman ekle -> buyume + boyut 5 */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken d: Dizi<tam32> = dizi_olustur(2); "
        "dizi_ekle(d, 1); dizi_ekle(d, 2); "
        "dizi_ekle(d, 3); dizi_ekle(d, 4); dizi_ekle(d, 5); "
        "ver dizi_boyut(d); }");
    test_sonuc("dizi otomatik buyume (kap=2, +5) -> boyut 5", rc == 5);
}

static void test_dizi_bos_olustur(void) {
    /* kapasite 0 -> boyut 0 */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken d: Dizi<tam32> = dizi_olustur(0); "
        "ver dizi_boyut(d); }");
    test_sonuc("dizi_olustur(0) bos -> boyut 0", rc == 0);
}

static void test_dizi_toplam(void) {
    /* 10 + 20 + 12 = 42 */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken d: Dizi<tam32> = dizi_olustur(4); "
        "dizi_ekle(d, 10); dizi_ekle(d, 20); dizi_ekle(d, 12); "
        "ver dizi_al(d, 0) + dizi_al(d, 1) + dizi_al(d, 2); }");
    test_sonuc("dizi_al toplam (10+20+12) -> 42", rc == 42);
}

static void test_dizi_iken_dongu(void) {
    /* iken dongusu ile 5 ekle, sonra topla */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken d: Dizi<tam32> = dizi_olustur(4); "
        "de\xc4\x9fi\xc5\x9fken i: tam32 = 0; "
        "iken i < 5 { dizi_ekle(d, i + 1); i = i + 1; } "
        "de\xc4\x9fi\xc5\x9fken t: tam32 = 0; "
        "i = 0; "
        "iken i < dizi_boyut(d) { t = t + dizi_al(d, i); i = i + 1; } "
        "ver t; }");
    /* 1+2+3+4+5 = 15 */
    test_sonuc("dizi iken ekle + iken al toplam -> 15", rc == 15);
}

static void test_dizi_tam64_generic(void) {
    /* tam64 element generic instantiation */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken d: Dizi<tam64> = dizi_olustur(4); "
        "dizi_ekle(d, 100); dizi_ekle(d, 200); "
        "ver dizi_boyut(d); }");
    test_sonuc("Dizi<tam64> generic instan + 2 ekle -> 2", rc == 2);
}

static void test_dizi_metin_generic(void) {
    /* metin element — generic */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken d: Dizi<metin> = dizi_olustur(4); "
        "dizi_ekle(d, \"a\"); dizi_ekle(d, \"b\"); dizi_ekle(d, \"c\"); "
        "ver dizi_boyut(d); }");
    test_sonuc("Dizi<metin> generic + 3 ekle -> 3", rc == 3);
}

/* === Madde G: Dosya syscall primitifleri === */

static void test_dosya_var_mi_yok(void) {
    /* Var olmayan dosya -> dosya_var_mi yanlis (0) */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "e\xc4\x9f""er dosya_var_mi(\"nonexistent_xyz_42.txt\") { ver 1; } "
        "ver 0; }");
    test_sonuc("dosya_var_mi(\"nonexistent\") -> 0", rc == 0);
}

static void test_dosya_yaz_oku(void) {
    /* yaz + close + oku tum dosya -> uzunluk dogru */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken h: metin = dosya_ac(\"build/test_G_io.tmp\", \"yazma\"); "
        "dosya_yaz(h, \"merhaba\"); "
        "dosya_kapat(h); "
        "de\xc4\x9fi\xc5\x9fken icerik: metin = dosya_oku(\"build/test_G_io.tmp\"); "
        "de\xc4\x9fi\xc5\x9fken n: tam32 = metin_uzunluk(icerik); "
        "dosya_sil(\"build/test_G_io.tmp\"); "
        "ver n; }");
    test_sonuc("dosya yaz/oku/kapat/sil pipeline -> uzunluk 7", rc == 7);
}

static void test_dosya_boyut(void) {
    /* yaz "hi" (2 byte) -> boyut 2 -> int32 dondurelim */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken h: metin = dosya_ac(\"build/test_G_boyut.tmp\", \"yazma\"); "
        "dosya_yaz(h, \"hi\"); "
        "dosya_kapat(h); "
        "de\xc4\x9fi\xc5\x9fken b: tam64 = dosya_boyut(\"build/test_G_boyut.tmp\"); "
        "dosya_sil(\"build/test_G_boyut.tmp\"); "
        /* Cast tam64 -> tam32 (UTF-8 strict, eger E gerek olmadan) — kucuk val */
        "e\xc4\x9f""er b == 2 { ver 2; } "
        "ver 0; }");
    test_sonuc("dosya_boyut(\"hi\") -> 2", rc == 2);
}

static void test_dosya_yeniden_adlandir(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken h: metin = dosya_ac(\"build/test_G_ren.tmp\", \"yazma\"); "
        "dosya_yaz(h, \"ok\"); "
        "dosya_kapat(h); "
        "dosya_yeniden_adlandir(\"build/test_G_ren.tmp\", \"build/test_G_renamed.tmp\"); "
        "de\xc4\x9fi\xc5\x9fken v: mant\xc4\xb1ksal = dosya_var_mi(\"build/test_G_renamed.tmp\"); "
        "dosya_sil(\"build/test_G_renamed.tmp\"); "
        "e\xc4\x9f""er v { ver 1; } ver 0; }");
    test_sonuc("dosya_yeniden_adlandir + var_mi yeni -> 1", rc == 1);
}

static void test_dosya_sil_yoksa(void) {
    /* Var olmayan dosyayi sil -> nonzero exit (remove rc != 0) */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken r: tam32 = dosya_sil(\"build/test_G_yok.tmp\"); "
        /* r != 0 ise basari; -1 negatif olabilir, exit kodu 255 + ... */
        "e\xc4\x9f""er r != 0 { ver 42; } "
        "ver 0; }");
    test_sonuc("dosya_sil(yok) -> r!=0 -> 42", rc == 42);
}

/* (Madde B paralel session generic API tests removed — bu oturumda
 *  concrete _tam suffix versiyonu (dizi_olustur_tam vb.) tip_kontrol'de
 *  registered. Generic dizi_olustur<T> v2'de.) */

/* === Madde E: Tip donusturme (olarak) === */

static void test_olarak_tam32_tam64(void) {
    /* tam32 -> tam64 -> tam32 round trip */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken x: tam32 = 42; "
        "de\xc4\x9fi\xc5\x9fken y: tam64 = x olarak tam64; "
        "ver y olarak tam32; }");
    test_sonuc("tam32 olarak tam64 olarak tam32 -> 42", rc == 42);
}

static void test_olarak_tam64_tam32(void) {
    /* tam64 -> tam32 (trunc) */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken y: tam64 = 42; "
        "ver y olarak tam32; }");
    test_sonuc("tam64 olarak tam32 (trunc) -> 42", rc == 42);
}

static void test_olarak_zincir(void) {
    /* (x olarak tam64) + 1 olarak tam32 */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken x: tam8 = 40; "
        "ver (x olarak tam32) + 2; }");
    test_sonuc("tam8 olarak tam32 + 2 -> 42", rc == 42);
}

static void test_olarak_aritmetik(void) {
    /* dizi_boyut sonucu tam32 — onu tam64'e cevirmek */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken d: Dizi<tam32> = dizi_olustur(4); "
        "dizi_ekle(d, 1); dizi_ekle(d, 2); "
        "de\xc4\x9fi\xc5\x9fken n: tam64 = dizi_boyut(d) olarak tam64; "
        "ver n olarak tam32 * 21; }");
    test_sonuc("dizi_boyut tam32->tam64->tam32 + carp -> 42", rc == 42);
}

static void test_olarak_tam_kesirli(void) {
    /* tam32 -> kesirli64 — sitofp */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken x: tam32 = 21; "
        "de\xc4\x9fi\xc5\x9fken f: kesirli64 = x olarak kesirli64; "
        "ver (f * 2.0) olarak tam32; }");
    test_sonuc("tam32 -> kesirli64 -> tam32 (sitofp/fptosi) -> 42", rc == 42);
}

/* === Madde B: Dinamik dizi === */

static void test_dizi_olustur_ekle(void) {
    /* dizi_olustur + 3 dizi_ekle + dizi_boyut -> 3 */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken d: Dizi<tam32> = dizi_olustur(8); "
        "dizi_ekle(d, 1); dizi_ekle(d, 2); dizi_ekle(d, 3); "
        "ver dizi_boyut(d); }");
    test_sonuc("dizi_ekle x3 + boyut -> 3", rc == 3);
}

static void test_dizi_al_topla(void) {
    /* 10 + 20 + 12 = 42 */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken d: Dizi<tam32> = dizi_olustur(4); "
        "dizi_ekle(d, 10); dizi_ekle(d, 20); dizi_ekle(d, 12); "
        "ver dizi_al(d, 0) + dizi_al(d, 1) + dizi_al(d, 2); }");
    test_sonuc("dizi_al toplama -> 42", rc == 42);
}

static void test_dizi_boyut(void) {
    /* Sadece bos dizi: boyut 0 */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken d: Dizi<tam32> = dizi_olustur(4); "
        "ver dizi_boyut(d); }");
    test_sonuc("bos dizi boyut -> 0", rc == 0);
}

static void test_dizi_buyume(void) {
    /* Kapasite 2 ile basla, 5 eleman ekle (auto-grow) — boyut 5 */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken d: Dizi<tam32> = dizi_olustur(2); "
        "dizi_ekle(d, 1); dizi_ekle(d, 2); dizi_ekle(d, 3); "
        "dizi_ekle(d, 4); dizi_ekle(d, 5); "
        "ver dizi_boyut(d); }");
    test_sonuc("dizi buyume (5 ekle) -> 5", rc == 5);
}

static void test_dizi_tam64(void) {
    /* tam64 dizi — generic instantiation tam64 ile */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken d: Dizi<tam64> = dizi_olustur(4); "
        "dizi_ekle(d, 100); dizi_ekle(d, 200); "
        "ver dizi_boyut(d); }");
    test_sonuc("dizi<tam64> generic instan -> 2", rc == 2);
}

/* === MMIO Foundation: capability-parametreli register erisimi === */

static void test_mmio_yaz_oku_round_trip(void) {
    /* yetki<MMIO> uret; yaz 42; oku; exit = okunan deger. Host mock tampon
     * round-trip yapar (kdl_runtime_mmio.c). geri_al ile linear tuketim. */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 {\n"
        "  de\xc4\x9fi\xc5\x9fken y: yetki<MMIO> = yetki_olustur(6, 3);\n"
        "  mmio_yaz32(y, 4096, 42);\n"
        "  de\xc4\x9fi\xc5\x9fken v: tam32 = mmio_oku32(y, 4096);\n"
        "  geri_al(y);\n"
        "  ver v;\n"
        "}");
    test_sonuc("mmio yaz 42 -> oku -> exit 42", rc == 42);
}

static void test_mmio_sabit_adres(void) {
    /* Ust duzey sabit adres (cross-file sabit codegen kok cozumu) +
     * iki ayri register: yaz(taban+0)=30, yaz(taban+4)=12, oku ikisini topla. */
    int rc = derle_ve_calistir(
        "sabit TABAN: tam64 = 268435456;\n"
        "sabit OFS_A: tam64 = 0;\n"
        "sabit OFS_B: tam64 = 4;\n"
        "i\xc5\x9flev main() -> tam32 {\n"
        "  de\xc4\x9fi\xc5\x9fken y: yetki<MMIO> = yetki_olustur(6, 3);\n"
        "  mmio_yaz32(y, TABAN + OFS_A, 30);\n"
        "  mmio_yaz32(y, TABAN + OFS_B, 12);\n"
        "  de\xc4\x9fi\xc5\x9fken s: tam32 = mmio_oku32(y, TABAN + OFS_A)"
        " + mmio_oku32(y, TABAN + OFS_B);\n"
        "  geri_al(y);\n"
        "  ver s;\n"
        "}");
    test_sonuc("mmio sabit adres (taban+ofs) iki register topla -> 42",
               rc == 42);
}

/* === C9: typed-width MMIO (16/64-bit) + byte-adreslenebilir mock === */

static void test_mmio16_round_trip(void) {
    /* 16-bit yaz 42 -> oku16 -> exit. ver: tam16 (i16) -> tam32 (sext). */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 {\n"
        "  de\xc4\x9fi\xc5\x9fken y: yetki<MMIO> = yetki_olustur(6, 3);\n"
        "  mmio_yaz16(y, 4096, 42);\n"
        "  de\xc4\x9fi\xc5\x9fken v: tam16 = mmio_oku16(y, 4096);\n"
        "  geri_al(y);\n"
        "  ver v;\n"
        "}");
    test_sonuc("mmio16 yaz 42 -> oku16 -> exit 42", rc == 42);
}

static void test_mmio64_round_trip(void) {
    /* 64-bit yaz 42 -> oku64 -> exit. ver: tam64 (i64) -> tam32 (trunc). */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 {\n"
        "  de\xc4\x9fi\xc5\x9fken y: yetki<MMIO> = yetki_olustur(6, 3);\n"
        "  mmio_yaz64(y, 8192, 42);\n"
        "  de\xc4\x9fi\xc5\x9fken v: tam64 = mmio_oku64(y, 8192);\n"
        "  geri_al(y);\n"
        "  ver v;\n"
        "}");
    test_sonuc("mmio64 yaz 42 -> oku64 -> exit 42", rc == 42);
}

static void test_mmio16_komsu_ayrisir(void) {
    /* KRITIK regresyon kanit: eski (adres>>2) kelime-collapse mock'ta 4096 ve
     * 4098 AYNI kelimeye duser (4096>>2 == 4098>>2 == 1024); ikinci yaz
     * birinciyi ezer -> oku16(4096)=32 -> toplam 64 (eski exit-5 sinifi hata).
     * Byte-adreslenebilir mock'ta ayri slotlar: 10 + 32 = 42. */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 {\n"
        "  de\xc4\x9fi\xc5\x9fken y: yetki<MMIO> = yetki_olustur(6, 3);\n"
        "  mmio_yaz16(y, 4096, 10);\n"
        "  mmio_yaz16(y, 4098, 32);\n"
        "  de\xc4\x9fi\xc5\x9fken a: tam16 = mmio_oku16(y, 4096);\n"
        "  de\xc4\x9fi\xc5\x9fken b: tam16 = mmio_oku16(y, 4098);\n"
        "  geri_al(y);\n"
        "  ver a + b;\n"
        "}");
    test_sonuc("mmio16 komsu alanlar ayrisir (eski exit-5 cakismasi duzeldi) -> 42",
               rc == 42);
}

static void test_mmio_genis_verify(void) {
    /* Fikstur: le16 komsu idx + le64 desc; her BB terminator'lu, opt temiz. */
    int ok = kemgu_llvm_opt_verify("test/snapshots/mmio_genis.kem");
    test_sonuc("mmio typed-width fikstur: opt -passes=verify PASS", ok);
}

static void test_mmio_genis_calistir(void) {
    /* le16 komsu (10+32) + le64 round-trip (4096) dogrulamasi -> exit 42. */
    int rc = derle_dosya_ve_calistir("test/snapshots/mmio_genis.kem");
    test_sonuc("mmio typed-width fikstur (le16 komsu + le64) -> exit 42", rc == 42);
}

/* --- C-track: &Struct referans-param + alan atama + yetki ABI --- */

static void test_ref_struct_param_alan_okuma(void) {
    /* D9 x3 repro: onceki durum — &v cagri argumani struct DEGERINI
     * gecirirdi (OP_REF lowering yoktu, "tekli op desteklenmiyor") ->
     * callee ptr bekler, ilk alani pointer sanir -> runtime SEGFAULT
     * (exit 139). opt -verify bunu YAKALAMAZ (imza-uyumsuz direkt
     * cagri gecerli-ama-UB IR). */
    int rc = derle_ve_calistir(
        "yap\xc4\xb1 Vq { taban: tam64; boyut: tam32; } "
        "i\xc5\x9flev kur(y: tam32, vq: &Vq) -> tam32 { "
        "ver vq.boyut + y; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken v: Vq = Vq { taban: 4096, boyut: 40 }; "
        "ver kur(2, &v); }");
    test_sonuc("&Struct param + alan okuma round-trip -> exit 42",
               rc == 42);
}

static void test_ref_struct_param_sonuc_verify(void) {
    int ok = kemgu_llvm_opt_verify("test/snapshots/ref_struct_sonuc.kem");
    test_sonuc("&Struct param + sonuc donus: opt verify PASS", ok);
}

static void test_ref_struct_param_sonuc_calistir(void) {
    /* D9 w1 repro: &Struct param + sonuc<bos,cesit> donus + esles. */
    int rc = derle_dosya_ve_calistir("test/snapshots/ref_struct_sonuc.kem");
    test_sonuc("&Struct param + sonuc donus (w1) -> exit 42", rc == 42);
}

static void test_struct_alan_atama(void) {
    /* Init-test koku #3: `x.alan = v` onceden SESSIZCE dusurulurdu
     * (DUGUM_ATAMA yalniz tanimlayici hedef taniyordu) -> D4/D5
     * blk_yapilandirma cfg alanlari hep 0 kalirdi. Hem lokal struct
     * hem &degisken referans param hedefi dogrulanir. */
    int rc = derle_ve_calistir(
        "yap\xc4\xb1 K { a: tam32; b: tam64; } "
        "i\xc5\x9flev doldur(k: &de\xc4\x9fi\xc5\x9fken K) -> tam32 { "
        "k.a = 40; k.b = 2; ver 0; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken k: K = K { a: 0, b: 0 }; "
        "doldur(&k); "
        "ver k.a + (k.b olarak tam32); }");
    test_sonuc("alan atama: &degisken ref uzerinden mutasyon -> exit 42",
               rc == 42);
}

static void test_yetki_delege_abi(void) {
    /* Init-test koku #2: %kdl_yetki (16B) IR<->C sinirinda first-class
     * arg gecirilirdi; Win64 C ABI pointer bekler -> kdl_yetki_delege
     * prologunda segfault. Fix: sret/ptr C-uyumlu imzalar. */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken y: yetki<MMIO> = yetki_olustur(6, 3); "
        "de\xc4\x9fi\xc5\x9fken y1: yetki<MMIO> = delege(y, 3); "
        "de\xc4\x9fi\xc5\x9fken y2: yetki<MMIO> = delege(y1, 1); "
        "ver 42; }");
    test_sonuc("yetki_olustur + delege zinciri (Win64 C ABI) -> exit 42",
               rc == 42);
}

/* --- Codegen coverage audit: lowering bosluk regresyonlari ---
 * KRITIK: hepsi RUNTIME round-trip assert eder (yaz -> oku -> dogru
 * deger). opt-verify bu sinifi YAKALAMAZ (dusurulen deyim / yanlis
 * deger gecerli-ama-yanlis IR uretir). */

static void test_audit_deref_okuma(void) {
    /* Gap #1: *p deref load emit etmiyordu — ptr DEGERI donerdi. */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev oku(p: *tam32) -> tam32 { "
        "g\xc3\xbcvensiz { ver *p; } ver 0; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken x: tam32 = 42; ver oku(&x); }");
    test_sonuc("audit: *p deref yukleme round-trip -> exit 42", rc == 42);
}

static void test_audit_stack_dizi_eleman_atama(void) {
    /* Gap #2 (stack): d[i] = v onceden sessizce dusurulurdu -> toplam
     * 6 kalirdi. Yaz -> oku round-trip: 1 + 38 + 3 = 42. */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken d = [1, 2, 3]; "
        "d[1] = 38; "
        "ver d[0] + d[1] + d[2]; }");
    test_sonuc("audit: stack d[i]=v yaz-oku round-trip -> exit 42",
               rc == 42);
}

static void test_audit_nested_alan_atama(void) {
    /* Gap #3: a.b.c = v onceden dusurulurdu (erisim_lvalue tek seviye).
     * 3-seviye yaz -> oku round-trip. */
    int rc = derle_ve_calistir(
        "yap\xc4\xb1 Ic { x: tam32; } "
        "yap\xc4\xb1 Orta { ic: Ic; } "
        "yap\xc4\xb1 Dis { orta: Orta; y: tam32; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken d: Dis = "
        "Dis { orta: Orta { ic: Ic { x: 0 } }, y: 2 }; "
        "d.orta.ic.x = 40; "
        "ver d.orta.ic.x + d.y; }");
    test_sonuc("audit: a.b.c = v ic ice yaz-oku round-trip -> exit 42",
               rc == 42);
}

static void test_audit_linear_kullan_round_trip(void) {
    /* Gap #4+#5: tekkez_yarat onceden TANIMSIZ sembol (link hatasi),
     * kullan(e) sessiz 0 donerdi. Zero-overhead pass-through dogrula:
     * tekkez_yarat(42) -> kullan(t) -> 42. Lineer muhasebe tip
     * kontrolde (program --check'ten de gecer). */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken t: tekkez<tam32> = tekkez_yarat(42); "
        "de\xc4\x9fi\xc5\x9fken v: tam32 = kullan(t); "
        "ver v; }");
    test_sonuc("audit: tekkez_yarat -> kullan round-trip -> exit 42",
               rc == 42);
}

static void test_matris_a_dtam_kiyas(void) {
    /* D-005: dtam8 200 > 100 — signed olsa -56 > 100 = yanlis. */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken a: dtam8 = 200; "
        "e\xc4\x9f" "er a > 100 { ver 42; } ver 1; }");
    test_sonuc("matris-A: dtam8 isaretsiz karsilastirma -> exit 42",
               rc == 42);
}

static void test_matris_a_dtam_bolme_kaydir(void) {
    /* D-005: udiv + lshr. dtam8 200/2=100, 200>>1=100; signed olsa
     * -56/2=-28, -56>>1=-28 -> farkli. */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken a: dtam8 = 200; "
        "e\xc4\x9f" "er (a / 2) olarak tam32 == 100 ve "
        "(a >> 1) olarak tam32 == 100 { ver 42; } ver 1; }");
    test_sonuc("matris-A: dtam8 udiv+lshr -> exit 42", rc == 42);
}

static void test_matris_a_i1_zext(void) {
    /* D-005 (en yaygin gap): dogru olarak tam32 == 1 (sext olsa -1). */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken b: mant\xc4\xb1ksal = do\xc4\x9fru; "
        "ver 41 + (b olarak tam32); }");
    test_sonuc("matris-A: i1 zext (dogru olarak tam32 = 1) -> exit 42",
               rc == 42);
}

static void test_matris_a_dtam_param_donus(void) {
    /* D-005: dtamN param + donus boyunca signedness korunur. */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev yari(x: dtam8) -> dtam8 { ver x / 2; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken a: dtam8 = 200; "
        "e\xc4\x9f" "er yari(a) > 50 { "
        "ver (yari(a)) olarak tam32 - 58; } ver 1; }");
    test_sonuc("matris-A: dtam param/donus signedness -> exit 42",
               rc == 42);
}

static void test_matris_c_deref_ref_round_trip(void) {
    /* Matris C: *(&v) skaler round-trip — &v gercek adres, *  yukler. */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken v: tam32 = 42; "
        "g\xc3\xbcvensiz { ver *(&v); } ver 1; }");
    test_sonuc("matris-C: *(&v) round-trip -> exit 42", rc == 42);
}

static void test_matris_b_deref_atama_t022(void) {
    /* Matris B: *p = v spec geregi T022-RED (DOGRULA). Lvalue yalniz
     * tanimlayici/erisim/indeks; ham pointer-deref hedefi degil. */
    int ok = kemgu_check_basarili("test/snapshots/deref_atama_t022.kem");
    test_sonuc("matris-B: *p=v -> T022 reddi (spec-dogru)", ok == 0);
}

static void test_matris_de_karsilikli_ozyineleme(void) {
    /* Matris D/E: karsilikli ozyineleme (cift_mi <-> tek_mi). */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev cift_mi(n: tam32) -> mant\xc4\xb1ksal { "
        "e\xc4\x9f" "er n == 0 { ver do\xc4\x9fru; } ver tek_mi(n - 1); } "
        "i\xc5\x9flev tek_mi(n: tam32) -> mant\xc4\xb1ksal { "
        "e\xc4\x9f" "er n == 0 { ver yanl\xc4\xb1\xc5\x9f; } "
        "ver cift_mi(n - 1); } "
        "i\xc5\x9flev main() -> tam32 { "
        "e\xc4\x9f" "er cift_mi(10) ve de\xc4\x9fil tek_mi(10) { ver 42; } "
        "ver 1; }");
    test_sonuc("matris-D/E: karsilikli ozyineleme -> exit 42", rc == 42);
}

static void test_matris_e_yetki_param_sinir(void) {
    /* Matris E: yetki<R> fonksiyon SINIRINDA pass-through (Win64 sret
     * ABI yolu calismaya devam — &Struct fix sonrasi). */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev kullan_yetki(y: yetki<MMIO>) -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken y2: yetki<MMIO> = delege(y, 1); ver 42; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken y: yetki<MMIO> = yetki_olustur(6, 3); "
        "ver kullan_yetki(y); }");
    test_sonuc("matris-E: yetki<R> param sinir pass-through -> exit 42",
               rc == 42);
}

static void test_matris_e_tekkez_param_sinir(void) {
    /* Matris E: tekkez<T> fonksiyon SINIRINDA pass-through + cagrida
     * tuketim (lineer kara kutu zero-overhead). */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev tuket(t: tekkez<tam32>) -> tam32 { ver kullan(t); } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken t: tekkez<tam32> = tekkez_yarat(42); "
        "ver tuket(t); }");
    test_sonuc("matris-E: tekkez<T> param sinir round-trip -> exit 42",
               rc == 42);
}

static void test_matris_d_esles_cesit_exhaustive(void) {
    /* Matris D: cesit varyantlari tum kollar (i8 discriminant dispatch). */
    int rc = derle_ve_calistir(
        "\xc3\xa7" "e\xc5\x9fit Renk { Kirmizi, Yesil, Mavi } "
        "i\xc5\x9flev kod(r: Renk) -> tam32 { "
        "e\xc5\x9fle\xc5\x9f r { "
        "Renk::Kirmizi => { ver 10; } "
        "Renk::Yesil => { ver 20; } "
        "Renk::Mavi => { ver 12; } } ver 0; } "
        "i\xc5\x9flev main() -> tam32 { "
        "ver kod(Renk::Kirmizi) + kod(Renk::Mavi) + kod(Renk::Yesil); }");
    test_sonuc("matris-D: cesit exhaustive esles (10+12+20) -> exit 42",
               rc == 42);
}

/* --- D-006: &-of-field/element parser onceligi (postfix > prefix) --- */

static void test_d006_ref_alan_okuma(void) {
    /* &p.x = &(p.x): alan adresi -> deref-oku round-trip. Eskiden
     * (&p).x -> kopya-adres -> i32 deger ptr-param'a -> SEGFAULT. */
    int rc = derle_ve_calistir(
        "yap\xc4\xb1 P { x: tam32; y: tam32; } "
        "i\xc5\x9flev artir(p: *tam32) -> tam32 { "
        "g\xc3\xbcvensiz { ver *p + 1; } ver 0; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken p: P = P { x: 41, y: 0 }; "
        "g\xc3\xbcvensiz { ver artir(&p.x); } ver 1; }");
    test_sonuc("D-006: &p.x = &(p.x) deref-oku round-trip -> exit 42",
               rc == 42);
}

static void test_d006_ref_eleman_okuma(void) {
    /* &d[i] = &(d[i]): eleman adresi -> deref-oku. Eskiden (&d)[i]. */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev oku(p: *tam32) -> tam32 { "
        "g\xc3\xbcvensiz { ver *p; } ver 0; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken d = [10, 42, 30]; "
        "g\xc3\xbcvensiz { ver oku(&d[1]); } ver 1; }");
    test_sonuc("D-006: &d[i] = &(d[i]) deref-oku round-trip -> exit 42",
               rc == 42);
}

static void test_d006_ref_nested_alan(void) {
    /* &a.b.c = &((a.b).c): ic ice alan adresi. */
    int rc = derle_ve_calistir(
        "yap\xc4\xb1 Ic { v: tam32; } "
        "yap\xc4\xb1 Dis { ic: Ic; } "
        "i\xc5\x9flev oku(p: *tam32) -> tam32 { "
        "g\xc3\xbcvensiz { ver *p; } ver 0; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken d: Dis = Dis { ic: Ic { v: 42 } }; "
        "g\xc3\xbcvensiz { ver oku(&d.ic.v); } ver 1; }");
    test_sonuc("D-006: &a.b.c ic ice alan adresi -> exit 42", rc == 42);
}

static void test_d006_regresyon_kombinasyonlar(void) {
    /* Oncelik degisimi diger &/postfix/onek kombinasyonlarini bozmasin:
     * &x duz, *(&x) round-trip, &x+y aritmetik (= (&x adresi degil;
     * burada deger baglaminda *(&x)+y), -p.x = -(p.x), ic ice *& . */
    int rc = derle_ve_calistir(
        "yap\xc4\xb1 P { x: tam32; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken x: tam32 = 20; "
        "de\xc4\x9fi\xc5\x9fken p: P = P { x: 0 - 22 }; "
        "g\xc3\xbcvensiz { "
        "de\xc4\x9fi\xc5\x9fken a: tam32 = *(&x); "      /* *& round-trip = 20 */
        "de\xc4\x9fi\xc5\x9fken b: tam32 = -p.x; "        /* -(p.x) = 22 */
        "ver a + b; } ver 1; }");
    test_sonuc("D-006 regresyon: *(&x) + -p.x = 20+22 -> exit 42",
               rc == 42);
}

static void test_matris_f_tekkez_esles_kolu(void) {
    /* Matris F (oncelik): tekkez sonuc<tekkez<T>,H>'ten cikip esles
     * kolunda tuketiliyor — lineer deger cagri+esles+kullan zinciri. */
    int rc = derle_ve_calistir(
        "\xc3\xa7" "e\xc5\x9fit Yok { Bos } "
        "i\xc5\x9flev al() -> sonu\xc3\xa7<tekkez<tam32>, Yok> { "
        "ver tamam(tekkez_yarat(42)); } "
        "i\xc5\x9flev main() -> tam32 { "
        "e\xc5\x9fle\xc5\x9f al() { "
        "tamam(t) => { ver kullan(t); } "
        "hata(e) => { ver 1; } } ver 2; }");
    test_sonuc("matris-F: tekkez esles-kolunda tuketim -> exit 42",
               rc == 42);
}

static void test_matris_f_capability_lineer_capraz(void) {
    /* Matris F CAPRAZ: yetki<MMIO> (capability-gate) + tekkez<tam32>
     * ayni scope'ta, ikisi de tuketiliyor (geri_al + kullan). */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken y: yetki<MMIO> = yetki_olustur(6, 3); "
        "de\xc4\x9fi\xc5\x9fken t: tekkez<tam32> = tekkez_yarat(42); "
        "mmio_yaz32(y, 4096, 1); "
        "geri_al(y); "
        "ver kullan(t); }");
    test_sonuc("matris-F: capability + lineer capraz (ikisi tuketilir) "
               "-> exit 42", rc == 42);
}

static void test_matris_f_yetki_mmio_gate(void) {
    /* Matris F: yetki al + capability-gate'li typed MMIO round-trip. */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken y: yetki<MMIO> = yetki_olustur(6, 3); "
        "mmio_yaz32(y, 4096, 42); "
        "de\xc4\x9fi\xc5\x9fken v: tam32 = mmio_oku32(y, 4096); "
        "geri_al(y); ver v; }");
    test_sonuc("matris-F: yetki MMIO capability-gate round-trip -> exit 42",
               rc == 42);
}

static void test_stretch_tek_varyant_cesit(void) {
    /* stretch: tek-varyant cesit + esles (kenar durum). */
    int rc = derle_ve_calistir(
        "\xc3\xa7" "e\xc5\x9fit Birim { Tek } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken b: Birim = Birim::Tek; "
        "e\xc5\x9fle\xc5\x9f b { Birim::Tek => { ver 42; } } ver 1; }");
    test_sonuc("stretch: tek-varyant cesit + esles -> exit 42", rc == 42);
}

/* --- v1 bölge-container: bölge_al<T> + *T indeksleme --- */

static void test_bolge_al_dosya(const char *isim, const char *ad) {
    /* fixture: --check GECMELI + E2E calisip 42 donmeli. */
    char yol[128];
    snprintf(yol, sizeof(yol), "test/snapshots/%s.kem", isim);
    int ok = kemgu_check_basarili(yol);
    int rc = ok ? derle_dosya_ve_calistir(yol) : -1;
    char mesaj[160];
    snprintf(mesaj, sizeof(mesaj),
             "bolge_al: %s --check + E2E -> exit 42", ad);
    test_sonuc(mesaj, ok == 1 && rc == 42);
}

static void test_bolge_al_genislikler(void) {
    /* DoD: T = tam8 / tam64 / struct — GENISLIK dogrulamali round-trip.
     * tam8: 100+100 wrap -56 (i8 stride+load kaniti); tam64: 2^32 farki
     * (i64 kaniti); struct: GEP-null sizeof (padding dahil), v[0]/v[2]
     * alan butunlugu. Onceki durum: *T indeksleme T008 ile bloklu +
     * eleman tipi beklenen/RHS'ten -> yanlis genislik riski. */
    test_bolge_al_dosya("bolge_al_tam8", "tam8 wrap (-56)");
    test_bolge_al_dosya("bolge_al_tam64", "tam64 2^32 fark");
    test_bolge_al_dosya("bolge_al_struct", "struct sizeof+alan");
}

static void test_bolge_al_grow_copy(void) {
    /* DoD: kucuk alloc doldur -> buyuk alloc -> bellek_kopyala
     * (E002 dar gevsetme: *T -> metin) -> icerik korunuyor. */
    test_bolge_al_dosya("bolge_al_grow", "grow bellek_kopyala");
}

static void test_bolge_al_negatifler(void) {
    /* BL001: beklenen *T baglami yok -> ret. Ters cast metin -> *T
     * KAPALI (E002 dar gevsetme yalniz *T -> metin yonu acti). */
    int ok1 = kemgu_check_basarili("test/snapshots/bolge_al_bl001.kem");
    test_sonuc("bolge_al: beklenen *T yok -> BL001 reddi", ok1 == 0);
    int ok2 = kemgu_check_basarili("test/snapshots/bolge_al_ters_cast.kem");
    test_sonuc("bolge_al: metin -> *T ters cast KAPALI (E002)", ok2 == 0);
}

static void test_bolge_al_guvensiz_gate(void) {
    /* G001 tutarliligi: *T indeksleme guvensiz DISINDA reddedilir. */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken y: yetki<Bellek> = yetki_olustur(3, 3); "
        "de\xc4\x9fi\xc5\x9fken v: *tam32 = b\xc3\xb6lge_al(y, 4); "
        "g\xc3\xbcvensiz { v[0] = 42; "
        "de\xc4\x9fi\xc5\x9fken s: tam32 = v[0]; "
        "geri_al(y); ver s; } ver 1; }");
    test_sonuc("bolge_al: tam32 temel round-trip -> exit 42", rc == 42);
}

static void test_modul_typecheck_e2e_check(void) {
    /* T016 fix: modül programı artik --check GECER (eskiden T016 ile
     * blokluyordu — codegen'den once). ic ice + kardes + YOL cagri. */
    int ok = kemgu_check_basarili("test/snapshots/modul_e2e.kem");
    test_sonuc("T016: modul programi --check gecer (ic ice+kardes+YOL)",
               ok == 1);
}

static void test_modul_typecheck_e2e_run(void) {
    /* T016 fix: ayni program E2E derlenip calisir -> 42 (16+26). */
    int rc = derle_dosya_ve_calistir("test/snapshots/modul_e2e.kem");
    test_sonuc("T016: modul programi E2E calisir -> exit 42", rc == 42);
}

static void test_modul_cesit_check(void) {
    /* T016 fix: modul-yerel cesit + g::Renk::Kirmizi modul-nitelikli
     * varyant + modul-yerel cesit ustunde esles -> --check gecer. */
    int ok = kemgu_check_basarili("test/snapshots/modul_cesit.kem");
    test_sonuc("T016: modul cesit varyanti (g::Renk::Kirmizi) --check",
               ok == 1);
}

static void test_modul_cesit_run(void) {
    int rc = derle_dosya_ve_calistir("test/snapshots/modul_cesit.kem");
    test_sonuc("T016: modul cesit varyanti E2E -> exit 42", rc == 42);
}

/* --- Ilk stdlib: kütüphane/dizi.kem Liste<T> (in-file validasyon) --- */

/* Windows'ta system() cmd.exe ANSI codepage kullanir — UTF-8 'kütüphane'
 * yolu bozulur (bash/MSYS'te sorun yok). Dosyayi UTF-16 yolla acip
 * (_wfopen) ASCII gecici yola kopyalariz; testler o yoldan kosar. */
static const char *stdlib_dizi_yolu(void) {
#ifdef _WIN32
    static int hazir = 0;
    if (!hazir) {
        FILE *src = _wfopen(L"kütüphane/dizi.kem", L"rb");
        if (!src) return NULL;
        FILE *dst = fopen("build/_stdlib_dizi.kem", "wb");
        if (!dst) { fclose(src); return NULL; }
        char buf[4096];
        size_t r;
        while ((r = fread(buf, 1, sizeof(buf), src)) > 0) {
            fwrite(buf, 1, r, dst);
        }
        fclose(src);
        fclose(dst);
        hazir = 1;
    }
    return "build/_stdlib_dizi.kem";
#else
    return "k\xc3\xbct\xc3\xbcphane/dizi.kem";
#endif
}

static void test_stdlib_liste_check(void) {
    /* Liste<T>: generic yapi + *T tampon + bolge_al + modul ici generic
     * monomorphization — --check GECMELI. */
    const char *yol = stdlib_dizi_yolu();
    int ok = yol ? kemgu_check_basarili(yol) : 0;
    test_sonuc("stdlib Liste<T>: kutuphane/dizi.kem --check", ok == 1);
}

static void test_stdlib_liste_e2e(void) {
    /* In-file validasyon: tam32 (6 ekle, 0->4->8 buyume), tam64 (2^32
     * genislik kaniti), struct Nokta (alan butunlugu) + sinir-disi
     * varsayilan + boy/kapasite -> tum kontroller 0, main 42. */
    const char *yol = stdlib_dizi_yolu();
    int rc = yol ? derle_dosya_ve_calistir(yol) : -1;
    test_sonuc("stdlib Liste<T>: cok-tipli round-trip E2E -> exit 42",
               rc == 42);
}

/* --- [YUKSEK] Tek-gecis ad cozumu (feature/ad-cozum-tek-gecis) --- */

static void test_ad_cozum_sapma_check(void) {
    /* Sapma programi --check'ten gecmeli (ciplak kardes cagrilari +
     * goreli YOL, module-first/lexical kuralla cozulur). */
    int ok = kemgu_check_basarili("test/snapshots/ad_cozum_sapma.kem");
    test_sonuc("ad-cozum: sapma programi --check gecer (kardes-oncelik)",
               ok == 1);
}

static void test_ad_cozum_sapma_e2e(void) {
    /* Regresyon guard'i: tip kontrol (module-first/lexical) ile codegen
     * ayni sembole baglanmali. Onceki durum: codegen global-first —
     * ciplak f() modul icinde @f'e (global) baglaniyordu, goreli yol
     * (ic::g) hic cozulemiyordu -> exit 42 yerine yanlis deger / -1. */
    int rc = derle_dosya_ve_calistir("test/snapshots/ad_cozum_sapma.kem");
    test_sonuc("ad-cozum: kardes-oncelik tip kontrol == codegen -> exit 42",
               rc == 42);
}

static void test_ad_cozum_modul_govde_denetimi(void) {
    /* On-kosul guard'i: modul islev govdeleri tip kontrolune DAHIL.
     * Onceki durum: islev sembolu yalniz global scope'ta araniyordu ->
     * sessiz erken donus -> 'ver dogru;' (mantiksal != tam32) --check'ten
     * GECIYORDU. */
    int ok = kemgu_check_basarili("test/snapshots/ad_cozum_govde.kem");
    test_sonuc("ad-cozum: modul govdesi denetlenir (T020 --check kirmizi)",
               ok == 0);
}

static void test_kampanya_modul_mangling(void) {
    /* D-001 [YUKSEK]: modul uyeleri @modul.ad olarak emit + mat::f()
     * YOL cagrisi + ic ice modul + kardes ciplak-ad cagri. (T016 fix
     * sonrasi --check de gecer; bkz test_modul_typecheck_*.) */
    int rc = derle_ve_calistir(
        "mod\xc3\xbcl mat { "
        "i\xc5\x9flev kare(x: tam32) -> tam32 { ver x * x; } "
        "i\xc5\x9flev dordun(x: tam32) -> tam32 { ver kare(kare(x)); } "
        "mod\xc3\xbcl ic { "
        "i\xc5\x9flev arti_bir(x: tam32) -> tam32 { ver x + 1; } } } "
        "i\xc5\x9flev main() -> tam32 { "
        "ver mat::dordun(2) + mat::ic::arti_bir(25); }");
    test_sonuc("kampanya: modul mangling (@mat.dordun, ic ice, kardes) "
               "-> exit 42", rc == 42);
}

static void test_kampanya_short_circuit(void) {
    /* D-002 [YUKSEK]: ve/veya kisa-devre. Onceki 'and/or i32' her iki
     * tarafi da degerlendiriyordu — yan etki gozlemiyle dogrulanir:
     * 'yanlis ve f()' ve 'dogru veya f()' f'i CAGIRMAMALI (boyut 0),
     * 'dogru ve f()' CAGIRMALI (boyut 1). Deger semantigi de assert. */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev ekle_ve_dogru(d: Dizi<tam32>) -> mant\xc4\xb1ksal { "
        "dizi_ekle(d, 1); ver do\xc4\x9fru; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken d: Dizi<tam32> = dizi_olustur(0); "
        "e\xc4\x9f" "er yanl\xc4\xb1\xc5\x9f ve ekle_ve_dogru(d) { ver 1; } "
        "e\xc4\x9f" "er do\xc4\x9fru veya ekle_ve_dogru(d) { } "
        "de\xc4\x9filse { ver 2; } "
        "e\xc4\x9f" "er dizi_boyut(d) != 0 { ver 3; } "
        "e\xc4\x9f" "er do\xc4\x9fru ve ekle_ve_dogru(d) { } "
        "de\xc4\x9filse { ver 4; } "
        "e\xc4\x9f" "er dizi_boyut(d) != 1 { ver 5; } "
        "e\xc4\x9f" "er (do\xc4\x9fru ve do\xc4\x9fru) ve "
        "(yanl\xc4\xb1\xc5\x9f veya do\xc4\x9fru) { ver 42; } "
        "ver 6; }");
    test_sonuc("kampanya: ve/veya kisa-devre (yan etki + deger) "
               "-> exit 42", rc == 42);
}

static void test_audit_linear_imha(void) {
    /* Gap #6: imha(e) onceden sessiz 0 + 'desteklenmiyor' yorumu. */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken t: tekkez<tam32> = tekkez_yarat(5); "
        "imha(t); "
        "ver 42; }");
    test_sonuc("audit: imha(t) lineer dispose -> exit 42", rc == 42);
}

/* --- C5: satirici_asm (inline assembly) --- */

static void test_asm_round_trip_verify(void) {
    /* Coklu cikti + girdi + clobber: emit edilen modul opt'tan gecmeli. */
    int ok = kemgu_llvm_opt_verify("test/snapshots/asm_round_trip.kem");
    test_sonuc("satirici_asm round-trip: opt -passes=verify PASS", ok);
}

static void test_asm_round_trip_calistir(void) {
    /* x86: girdi(40) -> mov+add ile cikti(42); ikinci cikti sabit 100. */
    int rc = derle_dosya_ve_calistir("test/snapshots/asm_round_trip.kem");
    test_sonuc("satirici_asm x86 girdi/cikti round-trip -> exit 42",
               rc == 42);
}

static void test_asm_arm64_llvm_reddi(void) {
    /* AS001: arm64-tagli asm x86 triple altinda --llvm BASARISIZ olmali
     * (bozuk IR uretilmez; hedefe-duyarli triple C8'de). */
    char komut[1024];
    snprintf(komut, sizeof(komut),
             "%s --llvm test/snapshots/asm_arm64_ret.kem > %s 2>%s",
             KEMGU_BIN, LL_PATH, DEV_NULL);
    int rc = system(komut);
    test_sonuc("satirici_asm arm64 tag -> --llvm AS001 reddi", rc != 0);
}

static void test_asm_arm64_check_reddi(void) {
    /* AS001 ayni zamanda --check yolunda (kaynak konumuyla). */
    int ok = kemgu_check_basarili("test/snapshots/asm_arm64_ret.kem");
    test_sonuc("satirici_asm arm64 tag -> --check AS001 reddi", ok == 0);
}

static void test_asm_guvensiz_disi_reddi(void) {
    /* G002: guvensiz disinda satirici_asm --check'te reddedilir. */
    int ok = kemgu_check_basarili("test/snapshots/asm_guvensiz_disi.kem");
    test_sonuc("satirici_asm guvensiz disinda -> --check G002 reddi",
               ok == 0);
}

/* --- C5 on-kosul #1: guvensiz blok lowering --- */

static void test_guvensiz_blok_emit(void) {
    /* Onceki durum: DUGUM_GUVENSIZ llvm.c'de default'a duser, ic blok
     * TAMAMEN dusurulurdu -> x = 0 kalir, exit 0 (latent miscompile).
     * Fix sonrasi ic deyimler emit edilir -> exit 42. */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken x: tam32 = 0; "
        "g\xc3\xbcvensiz { x = 41; x = x + 1; } "
        "ver x; }");
    test_sonuc("guvensiz ic deyimler emit edilir -> exit 42", rc == 42);
}

static void test_guvensiz_icinde_ver(void) {
    /* guvensiz icindeki 'ver' terminator olarak fonksiyona yayilmali
     * (blok_uret term=1 doner, cift-ret/epilog karismaz). */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev f() -> tam32 { g\xc3\xbcvensiz { ver 42; } ver 0; } "
        "i\xc5\x9flev main() -> tam32 { ver f(); }");
    test_sonuc("guvensiz icinde 'ver' terminator -> exit 42", rc == 42);
}

int main(void) {
    printf("KEMGU LLVM Backend Entegrasyon Testleri\n");
    printf("=========================================\n");

    printf("\n--- Literaller + aritmetik ---\n");
    test_lit_42();
    test_aritmetik();
    test_tekli_neg();

    printf("\n--- Karsilastirma ---\n");
    test_kiyaslama_lt();
    test_kiyaslama_eq();

    printf("\n--- Mantiksal ---\n");
    test_mantiksal_ve();
    test_mantiksal_degil();

    printf("\n--- Eger/else ---\n");
    test_eger_else();

    printf("\n--- Lokal degisken ---\n");
    test_lokal_degisken();

    printf("\n--- Iken dongusu ---\n");
    test_iken_dongusu();

    printf("\n--- Cagri + recursive ---\n");
    test_islev_cagrisi();
    test_iki_islev();
    test_recursive_fib();

    printf("\n--- Parametre atamasi ---\n");
    test_parametre_atamasi();

    printf("\n--- Karmasik algoritmalar ---\n");
    test_faktoriyel();
    test_gcd();

    printf("\n--- ADIM 18: Multi-int ---\n");
    test_tam8();
    test_tam64();
    test_mantiksal_param();

    printf("\n--- ADIM 18: Metin literali ---\n");
    test_metin_donus();
    test_metin_global();

    printf("\n--- ADIM 18: Yapi ---\n");
    test_yapi_temel();
    test_yapi_atama();
    test_yapi_coklu_alan();

    printf("\n--- ADIM 22: LLVM v3 ---\n");
    test_kesirli64();
    test_kesirli32();
    test_dizi_temel();
    test_dizi_dongu();
    test_struct_param_by_value();
    test_struct_donus_by_value();

    printf("\n--- ADIM 23: Generic islev + stdlib ---\n");
    test_generic_kimlik();
    test_generic_iki_kat();
    test_generic_coklu_instan();
    test_generic_mutlak_stdlib();

    printf("\n--- ADIM 24: kendin (self) ---\n");
    test_kendin_method();
    test_kendin_arg_ile();

    printf("\n--- ADIM 27: Syscall (yazdir) ---\n");
    test_yazdir_hello();

    printf("\n--- A: Metin runtime primitifleri (runtime/kdl_runtime.c) ---\n");
    test_metin_uzunluk();
    test_metin_uzunluk_bos();
    test_metin_birlestir();
    test_metin_kes();
    test_metin_kucuk_ascii();
    test_metin_buyuk_turkce_i();
    test_metin_kucuk_tr_buyuk_I();
    test_metin_buyuk_tr_kucuk_i();
    test_metin_ascii_I_kalir();
    test_metin_ascii_turkce_korunur();
    test_metin_tr_yuvarlak_yolculuk();
    test_metin_tr_turkce_c();
    /* Adim 3: Dizi literal heap allocation */
    test_heap_dizi_literal_uzunluk();
    test_heap_dizi_literal_indeks();
    test_heap_dizi_literal_tam64();
    test_heap_dizi_buyume();
    test_stack_dizi_korunur();
    test_heap_stack_ayrim();
    test_heap_dizi_indeks_zincir();
    test_heap_dizi_uzun();
    /* Adim 7: Stdlib gercek bağlama */
    test_stdlib_harita_calistir();
    test_stdlib_filtre_calistir();
    test_stdlib_indirgeme_calistir();
    test_stdlib_metin_kucukbuyuk();
    test_stdlib_dizi_uzunluk();
    test_metin_icerir_evet();
    test_metin_icerir_hayir();
    test_metin_baslar();
    test_metin_biter();
    test_metin_kirp();
    test_metin_yer_degistir();

    printf("\n--- ADIM 28: Allocator (bellek_al/serbest) ---\n");
    test_bellek_al_serbest();

    printf("\n--- ADIM 30: Bit operatorleri ---\n");
    test_bit_ve_temel();
    test_bit_ve_mask();
    test_bit_veya_kombine();
    test_bit_veya_identity();
    test_bit_ozveya_temel();
    test_bit_ozveya_self_inverse();
    test_sola_kaydir_temel();
    test_sola_kaydir_sifir();
    test_saga_kaydir_temel();
    test_saga_kaydir_buyuk();
    test_bit_degil_double();
    test_bit_kompozisyon();
    test_bit_oncelik_shift_or();

    printf("\n--- B: Dizi dinamik allocator (generic intrinsics) ---\n");
    test_dizi_olustur_ekle_boyut();
    test_dizi_al();
    test_dizi_kapasite_otomatik_buyume();
    test_dizi_bos_olustur();
    test_dizi_toplam();
    test_dizi_iken_dongu();
    test_dizi_tam64_generic();
    test_dizi_metin_generic();

    printf("\n--- G: Dosya syscall primitifleri (runtime/kdl_runtime.c) ---\n");
    test_dosya_var_mi_yok();
    test_dosya_yaz_oku();
    test_dosya_boyut();
    test_dosya_yeniden_adlandir();
    test_dosya_sil_yoksa();


    printf("\n--- Madde E: Tip donusturme (olarak) ---\n");
    test_olarak_tam32_tam64();
    test_olarak_tam64_tam32();
    test_olarak_zincir();
    test_olarak_aritmetik();
    test_olarak_tam_kesirli();

    printf("\n--- Madde B: Dizi dinamik allocator ---\n");
    test_dizi_olustur_ekle();
    test_dizi_al_topla();
    test_dizi_boyut();
    test_dizi_buyume();
    test_dizi_tam64();

    printf("\n--- MMIO Foundation: yetki<MMIO> register erisimi ---\n");
    test_mmio_yaz_oku_round_trip();
    test_mmio_sabit_adres();

    printf("\n--- C9: typed-width MMIO (16/64-bit) + byte-adreslenebilir mock ---\n");
    test_mmio16_round_trip();
    test_mmio64_round_trip();
    test_mmio16_komsu_ayrisir();
    test_mmio_genis_verify();
    test_mmio_genis_calistir();

    printf("\n--- C1: esles (match) deyimi codegen ---\n");
    test_esles_wildcard();
    test_esles_nested();
    test_esles_match_early_return_verify();
    test_esles_match_early_return_calistir();

    printf("\n--- C2.5: sonuc/secimlik value codegen ---\n");
    test_sonuc_secimlik_verify();
    test_sonuc_secimlik_calistir();
    test_sonuc_struct_payload_verify();
    test_sonuc_struct_payload_calistir();

    printf("\n--- C2.6: cross-file fonksiyon cagrisi ---\n");
    test_crossfile_transitif_verify();
    test_crossfile_transitif_calistir();
    test_crossfile_sonuc_verify();
    test_crossfile_sonuc_calistir();

    printf("\n--- C2.7: cesit (custom sum type) + exhaustiveness ---\n");
    test_cesit_temel_verify();
    test_cesit_temel_calistir();
    test_cesit_sonuc_verify();
    test_cesit_sonuc_calistir();
    test_cesit_exhaustive_negatif();

    printf("\n--- C5 on-kosul #1: guvensiz blok lowering ---\n");
    test_guvensiz_blok_emit();
    test_guvensiz_icinde_ver();

    printf("\n--- C5: satirici_asm (inline assembly) ---\n");
    test_asm_round_trip_verify();
    test_asm_round_trip_calistir();
    test_asm_arm64_llvm_reddi();
    test_asm_arm64_check_reddi();
    test_asm_guvensiz_disi_reddi();

    printf("\n--- C-track: &Struct ref-param + alan atama + yetki ABI ---\n");
    test_ref_struct_param_alan_okuma();
    test_ref_struct_param_sonuc_verify();
    test_ref_struct_param_sonuc_calistir();
    test_struct_alan_atama();
    test_yetki_delege_abi();

    printf("\n--- Codegen coverage audit (runtime round-trip) ---\n");
    test_audit_deref_okuma();
    test_audit_stack_dizi_eleman_atama();
    test_audit_nested_alan_atama();
    test_audit_linear_kullan_round_trip();
    test_audit_linear_imha();

    printf("\n--- T016: modul type-check (E2E --check + run) ---\n");
    test_modul_typecheck_e2e_check();
    test_modul_typecheck_e2e_run();
    test_modul_cesit_check();
    test_modul_cesit_run();

    printf("\n--- Kampanya: modul mangling + short-circuit ---\n");
    printf("\n--- v1 bolge-container: bolge_al<T> + *T indeksleme ---\n");
    test_bolge_al_guvensiz_gate();
    test_bolge_al_genislikler();
    test_bolge_al_grow_copy();
    test_bolge_al_negatifler();

    printf("\n--- Ilk stdlib: Liste<T> (kutuphane/dizi.kem) ---\n");
    test_stdlib_liste_check();
    test_stdlib_liste_e2e();

    test_kampanya_modul_mangling();
    test_kampanya_short_circuit();

    printf("\n--- [YUKSEK] Tek-gecis ad cozumu (binding) ---\n");
    test_ad_cozum_sapma_check();
    test_ad_cozum_sapma_e2e();
    test_ad_cozum_modul_govde_denetimi();

    printf("\n--- Matris A: tipler x operatorler (signedness) ---\n");
    test_matris_a_dtam_kiyas();
    test_matris_a_dtam_bolme_kaydir();
    test_matris_a_i1_zext();
    test_matris_a_dtam_param_donus();

    printf("\n--- Matris B+C: erisim/isaretci (in-scope green + DUR-SOR) ---\n");
    test_matris_c_deref_ref_round_trip();
    test_matris_b_deref_atama_t022();

    printf("\n--- Matris D+E: kontrol akisi + fonksiyon siniri ---\n");
    test_matris_de_karsilikli_ozyineleme();
    test_matris_e_yetki_param_sinir();
    test_matris_e_tekkez_param_sinir();
    test_matris_d_esles_cesit_exhaustive();

    printf("\n--- D-006: &-of-field/element parser onceligi ---\n");
    test_d006_ref_alan_okuma();
    test_d006_ref_eleman_okuma();
    test_d006_ref_nested_alan();
    test_d006_regresyon_kombinasyonlar();

    printf("\n--- Matris F + stretch: lineer/bolge/yetki etkilesimleri ---\n");
    test_matris_f_tekkez_esles_kolu();
    test_matris_f_capability_lineer_capraz();
    test_matris_f_yetki_mmio_gate();
    test_stretch_tek_varyant_cesit();

    printf("\n=========================================\n");
    printf("Toplam: %d | Basarili: %d | Basarisiz: %d\n",
           toplam_test, basarili, basarisiz);
    return basarisiz > 0 ? 1 : 0;
}
