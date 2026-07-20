/*
 * KEMGU SIMD LLVM Backend Entegrasyon Testleri
 *
 * Her test:
 *   1) Bir KEMGU kaynagini gecici dosyaya yazar
 *   2) `kemgu --llvm dosya.kem > dosya.ll` calistirir
 *   3) `clang -x ir dosya.ll -o dosya.exe` ile derler
 *   4) `./dosya.exe` calistirir ve exit code'u dogrular
 *
 * Bagimliliklar:
 *   - ./build/kemgu.exe ve clang PATH'te.
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

#ifdef _WIN32
#include <process.h>
#define DEV_NULL "NUL"
#define KEMGU_BIN ".\\build\\kemgu.exe"
#define YOL_ONEK ".\\build\\"
#define SUREC_NO() ((int)_getpid())
#else
#include <unistd.h>
#define DEV_NULL "/dev/null"
#define KEMGU_BIN "./build/kemgu"
#define YOL_ONEK "./build/"
#define SUREC_NO() ((int)getpid())
#endif

/* D-297: gecici yollar SUREC-BENZERSIZ — bkz. test_llvm.c'deki ayrintili not.
 * Sabit yol, ayni testin iki es zamanli kosumunda SAHTE kirmizi uretiyordu. */
static char KEM_PATH[64];
static char LL_PATH[64];
static char EXE_PATH[64];

static void gecici_yollari_kur(void) {
    int pid = SUREC_NO();
    snprintf(KEM_PATH, sizeof(KEM_PATH), "%stest_simd_%d.kem", YOL_ONEK, pid);
    snprintf(LL_PATH,  sizeof(LL_PATH),  "%stest_simd_%d.ll",  YOL_ONEK, pid);
    snprintf(EXE_PATH, sizeof(EXE_PATH), "%stest_simd_%d.exe", YOL_ONEK, pid);
}

static void gecici_yollari_temizle(void) {
    remove(KEM_PATH);
    remove(LL_PATH);
    remove(EXE_PATH);
}

static int derle_ve_calistir(const char *kemgu_kaynak) {
    FILE *f = fopen(KEM_PATH, "w");   /* D-297: surec-benzersiz */
    if (!f) return -1;
    fputs(kemgu_kaynak, f);
    fclose(f);

    char komut[1024];
    snprintf(komut, sizeof(komut),
             "%s --llvm %s > %s 2>%s",
             KEMGU_BIN, KEM_PATH, LL_PATH, DEV_NULL);
    int rc = system(komut);
    if (rc != 0) return -1;

#ifdef _WIN32
    snprintf(komut, sizeof(komut),
             "clang -x ir %s -x none build\\kdl_runtime.o -o %s 2>%s",
             LL_PATH, EXE_PATH, DEV_NULL);
#else
    snprintf(komut, sizeof(komut),
             "clang -x ir %s -x none build/kdl_runtime.o -o %s 2>%s",
             LL_PATH, EXE_PATH, DEV_NULL);
#endif
    rc = system(komut);
    if (rc != 0) return -1;

    snprintf(komut, sizeof(komut), "%s", EXE_PATH);
    rc = system(komut);
    return rc;
}

/* === Testler === */

static void test_vektor_doldur_topla_42(void) {
    /* 4 lane vektör, hepsi 10+11 = 21, toplam = 84 */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 {\n"
        "    de\xc4\x9fi\xc5\x9fken a: vekt\xc3\xb6r<tam32, 4> = vektor_doldur(10);\n"
        "    de\xc4\x9fi\xc5\x9fken b: vekt\xc3\xb6r<tam32, 4> = vektor_doldur(11);\n"
        "    de\xc4\x9fi\xc5\x9fken c: vekt\xc3\xb6r<tam32, 4> = a + b;\n"
        "    ver vektor_topla(c);\n"
        "}\n");
    test_sonuc("SIMD-LLVM 1: vektor_doldur + vektor + vektor_topla = 84",
               rc == 84);
}

static void test_vektor_carp(void) {
    /* 4 lane * 4 lane: 3 * 4 = 12, sum 4 lanes = 48 */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 {\n"
        "    de\xc4\x9fi\xc5\x9fken a: vekt\xc3\xb6r<tam32, 4> = vektor_doldur(3);\n"
        "    de\xc4\x9fi\xc5\x9fken b: vekt\xc3\xb6r<tam32, 4> = vektor_doldur(4);\n"
        "    de\xc4\x9fi\xc5\x9fken c: vekt\xc3\xb6r<tam32, 4> = a * b;\n"
        "    ver vektor_topla(c);\n"
        "}\n");
    test_sonuc("SIMD-LLVM 2: v * v sonra topla = 48", rc == 48);
}

static void test_vektor_cikar(void) {
    /* 4 lane: 100 - 75 = 25, sum 4 lanes = 100 */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 {\n"
        "    de\xc4\x9fi\xc5\x9fken a: vekt\xc3\xb6r<tam32, 4> = vektor_doldur(100);\n"
        "    de\xc4\x9fi\xc5\x9fken b: vekt\xc3\xb6r<tam32, 4> = vektor_doldur(75);\n"
        "    de\xc4\x9fi\xc5\x9fken c: vekt\xc3\xb6r<tam32, 4> = a - b;\n"
        "    ver vektor_topla(c);\n"
        "}\n");
    test_sonuc("SIMD-LLVM 3: v - v sonra topla = 100", rc == 100);
}

static void test_vektor_8lane(void) {
    /* 8 lane * 5 = 40 her lane, sum = 320 */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 {\n"
        "    de\xc4\x9fi\xc5\x9fken v: vekt\xc3\xb6r<tam32, 8> = vektor_doldur(40);\n"
        "    ver vektor_topla(v);\n"
        "}\n");
    test_sonuc("SIMD-LLVM 4: 8 lane vektor_topla = 320", rc == 320);
}

static void test_vektor_eleman(void) {
    /* vektor_doldur(42), eleman 2 = 42 */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 {\n"
        "    de\xc4\x9fi\xc5\x9fken v: vekt\xc3\xb6r<tam32, 4> = vektor_doldur(42);\n"
        "    ver vektor_eleman(v, 2);\n"
        "}\n");
    test_sonuc("SIMD-LLVM 5: vektor_eleman(v, 2) = 42", rc == 42);
}

/* === Ana === */

int main(void) {
    gecici_yollari_kur();   /* D-297 */
    printf("=== KEMGU SIMD LLVM Backend Test Paketi ===\n\n");

    test_vektor_doldur_topla_42();
    test_vektor_carp();
    test_vektor_cikar();
    test_vektor_8lane();
    test_vektor_eleman();

    printf("\n=============================================\n");
    printf("Toplam: %d | Basarili: %d | Basarisiz: %d\n",
           toplam_test, basarili, basarisiz);
    gecici_yollari_temizle();   /* D-297 */
    return basarisiz == 0 ? 0 : 1;
}
