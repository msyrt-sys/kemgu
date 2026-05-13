/*
 * KEMGU Dizi Performans Regresyon Bench (Adim 6)
 *
 * Iki KEMGU programi derlenir + calistirilir:
 *   1) "Reserve YOK" — N kez dizi_ekle, capacity grow surekli realloc
 *   2) "Reserve YAR" — dizi_kapasite_ayarla(d, N) once, sonra N ekle
 *
 * Amac: capacity API'sinin yardimi kanitlanmali (her iki sayim ayni
 * sonuc verir, capacity ayarlama ile pre-allocated buffer kullanilir).
 *
 * Bench cikti: her programin toplam suresi + her birim ekle suresi.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int toplam = 0, basarili = 0, basarisiz = 0;

static void test_sonuc(const char *ad, int durum) {
    toplam++;
    if (durum) {
        basarili++;
        printf("  [%d] %s ... \xe2\x9c\x93\n", toplam, ad);
    } else {
        basarisiz++;
        printf("  [%d] %s ... \xe2\x9c\x97\n", toplam, ad);
    }
}

#ifdef _WIN32
#define DEV_NULL "NUL"
#define KEMGU_BIN ".\\build\\kemgu.exe"
#define CC_RUNTIME "build\\kdl_runtime.o"
#else
#define DEV_NULL "/dev/null"
#define KEMGU_BIN "./build/kemgu"
#define CC_RUNTIME "build/kdl_runtime.o"
#endif

/* KEMGU programi derle ve calistir, exit kodu ve süre ölç. */
static int derle_ve_olc(const char *kaynak, const char *etiket,
                         double *sure_sn) {
    char dosya_kem[256], dosya_ll[256], dosya_exe[256];
    snprintf(dosya_kem, sizeof(dosya_kem), "build/perf_%s.kem", etiket);
    snprintf(dosya_ll, sizeof(dosya_ll), "build/perf_%s.ll", etiket);
#ifdef _WIN32
    snprintf(dosya_exe, sizeof(dosya_exe), ".\\build\\perf_%s.exe", etiket);
#else
    snprintf(dosya_exe, sizeof(dosya_exe), "./build/perf_%s.exe", etiket);
#endif

    FILE *f = fopen(dosya_kem, "w");
    if (!f) return -1;
    fputs(kaynak, f);
    fclose(f);

    char komut[1024];
    snprintf(komut, sizeof(komut),
        "%s --llvm %s > %s 2>%s",
        KEMGU_BIN, dosya_kem, dosya_ll, DEV_NULL);
    if (system(komut) != 0) return -1;

    snprintf(komut, sizeof(komut),
        "clang -O2 -x ir %s -x none %s -o %s 2>%s",
        dosya_ll, CC_RUNTIME, dosya_exe, DEV_NULL);
    if (system(komut) != 0) return -1;

    /* Sureyi olc */
    clock_t bas = clock();
    int rc = system(dosya_exe);
    clock_t bit = clock();
    if (sure_sn) *sure_sn = (double)(bit - bas) / CLOCKS_PER_SEC;
#ifdef _WIN32
    return rc;
#else
    return WEXITSTATUS(rc);
#endif
}

static void test_capacity_temel(void) {
    /* Temel: dizi_olustur(50) sonra dizi_kapasite(d) -> 50 */
    double sn;
    int rc = derle_ve_olc(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken d: Dizi<tam32> = dizi_olustur(50); "
        "ver dizi_kapasite(d); }",
        "cap_temel", &sn);
    test_sonuc("dizi_olustur(50) + dizi_kapasite -> 50", rc == 50);
    printf("       sure: %.3f sn\n", sn);
}

static void test_kapasite_ayarla(void) {
    /* dizi_kapasite_ayarla(d, 1000) -> dizi_kapasite -> 1000 */
    double sn;
    int rc = derle_ve_olc(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken d: Dizi<tam32> = dizi_olustur(0); "
        "dizi_kapasite_ayarla(d, 1000); "
        "ver dizi_kapasite(d); }",
        "cap_ayarla", &sn);
    test_sonuc("dizi_kapasite_ayarla(d, 1000) -> 1000", rc == 1000);
    printf("       sure: %.3f sn\n", sn);
}

static void test_kapasite_kucult_yok(void) {
    /* Mevcut kapasiteden kucuk istek -> ignore (shrink yok v1) */
    double sn;
    int rc = derle_ve_olc(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken d: Dizi<tam32> = dizi_olustur(100); "
        "dizi_kapasite_ayarla(d, 10); "
        "ver dizi_kapasite(d); }",
        "cap_kucult", &sn);
    test_sonuc("kucut isteg ignore -> 100", rc == 100);
}

static void test_bench_100k_reserve_yok(void) {
    /* 100K ekle, reserve yok — grow stratejisi (doubling) */
    double sn;
    int rc = derle_ve_olc(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken d: Dizi<tam32> = dizi_olustur(0); "
        "de\xc4\x9fi\xc5\x9fken i: tam32 = 0; "
        "iken i < 100000 { dizi_ekle(d, i); i = i + 1; } "
        "ver dizi_boyut(d) - 99999; }",
        "bench_yok", &sn);
    test_sonuc("100K ekle reserve YOK -> boyut OK", rc == 1);
    printf("       sure: %.3f sn (reserve YOK)\n", sn);
}

static void test_bench_100k_reserve_var(void) {
    /* 100K ekle, reserve var */
    double sn;
    int rc = derle_ve_olc(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken d: Dizi<tam32> = dizi_olustur(0); "
        "dizi_kapasite_ayarla(d, 100000); "
        "de\xc4\x9fi\xc5\x9fken i: tam32 = 0; "
        "iken i < 100000 { dizi_ekle(d, i); i = i + 1; } "
        "ver dizi_boyut(d) - 99999; }",
        "bench_var", &sn);
    test_sonuc("100K ekle reserve VAR -> boyut OK", rc == 1);
    printf("       sure: %.3f sn (reserve VAR)\n", sn);
}

static void test_kapasite_buyume(void) {
    /* Bos dizi, 50 ekle: kapasite >= 50 olmali (doubling: 4,8,16,32,64) */
    double sn;
    int rc = derle_ve_olc(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken d: Dizi<tam32> = dizi_olustur(0); "
        "de\xc4\x9fi\xc5\x9fken i: tam32 = 0; "
        "iken i < 50 { dizi_ekle(d, i); i = i + 1; } "
        "ver dizi_kapasite(d); }",
        "cap_buyume", &sn);
    /* Kapasite >= 50 olmali */
    test_sonuc("kapasite buyume (50 ekle -> kapasite >= 50)", rc >= 50);
    printf("       son kapasite: %d\n", rc);
}

int main(void) {
    printf("KEMGU Dizi Performans Regresyon Bench (Adim 6)\n");
    printf("==============================================\n");

    printf("\n--- Temel API ---\n");
    test_capacity_temel();
    test_kapasite_ayarla();
    test_kapasite_kucult_yok();
    test_kapasite_buyume();

    printf("\n--- Bench (100K ekle) ---\n");
    test_bench_100k_reserve_yok();
    test_bench_100k_reserve_var();

    printf("\n==============================================\n");
    printf("Toplam: %d | Basarili: %d | Basarisiz: %d\n",
           toplam, basarili, basarisiz);
    return basarisiz > 0 ? 1 : 0;
}
