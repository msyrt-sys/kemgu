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

    /* clang -x ir .ll -o .exe */
    snprintf(komut, sizeof(komut),
             "clang -x ir %s -o %s 2>%s", LL_PATH, EXE_PATH, DEV_NULL);
    rc = system(komut);
    if (rc != 0) return -1;

    /* Calistir */
    snprintf(komut, sizeof(komut), "%s", EXE_PATH);
    rc = system(komut);
    return rc;
}


/* === Testler === */

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

    printf("\n=========================================\n");
    printf("Toplam: %d | Basarili: %d | Basarisiz: %d\n",
           toplam_test, basarili, basarisiz);
    return basarisiz > 0 ? 1 : 0;
}
