/*
 * KEMGU Snapshot Test Paketi (ADIM 32)
 *
 * Her test: test/snapshots/<isim>.kem dosyasini kemgu --parse ile parse
 * eder ve cikti'yi test/snapshots/<isim>.ast baseline'i ile karsilastirir.
 *
 * Yeni AST cikti veya yeni desteklenen sozdizimi: baseline'lari yenilemek
 * icin tools/regen_snapshots.sh calistir.
 *
 * Test sayisi: 20 baseline (literal/aritmetik/degisken/cagri/eger/iken/yapi/
 *   dizi/esles/lambda/generic/secimlik/sonuc/referans/metin/bit/shift/
 *   tekkez/uygula/ozyineli).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_DOSYA (256 * 1024)

static int toplam_test = 0;
static int basarili = 0;
static int basarisiz = 0;

#ifdef _WIN32
#define KEMGU_BIN ".\\build\\kemgu.exe"
#define DEV_NULL "NUL"
#else
#define KEMGU_BIN "./build/kemgu"
#define DEV_NULL "/dev/null"
#endif

static int dosya_oku(const char *yol, char *buf, int max) {
    FILE *f = fopen(yol, "rb");
    if (!f) return -1;
    int n = (int)fread(buf, 1, (size_t)(max - 1), f);
    fclose(f);
    buf[n] = '\0';
    return n;
}

/* [D-479] Tampondan CR baytlarini SIL, yeni uzunlugu don. Yerinde calisir. */
static int cr_at(char *s, int n) {
    int i, j = 0;
    if (n <= 0) return n;
    for (i = 0; i < n; i++) {
        if (s[i] != '\r') s[j++] = s[i];
    }
    return j;
}

static void test_snapshot(const char *isim) {
    char kem_yol[256], ast_yol[256], tmp_yol[256], cmd[1024];

    snprintf(kem_yol, sizeof(kem_yol), "test/snapshots/%s.kem", isim);
    snprintf(ast_yol, sizeof(ast_yol), "test/snapshots/%s.ast", isim);
    snprintf(tmp_yol, sizeof(tmp_yol), "build/tmp_snapshot_%s.ast", isim);

    snprintf(cmd, sizeof(cmd),
        "%s --parse %s > %s 2>&1", KEMGU_BIN, kem_yol, tmp_yol);
    int rc = system(cmd);

    toplam_test++;
    char *baseline = (char *)malloc(MAX_DOSYA);
    char *actual = (char *)malloc(MAX_DOSYA);
    if (!baseline || !actual) {
        basarisiz++;
        printf("  [%d] %s ... \xe2\x9c\x97 (bellek)\n", toplam_test, isim);
        free(baseline); free(actual);
        return;
    }

    int b = dosya_oku(ast_yol, baseline, MAX_DOSYA);
    int a = dosya_oku(tmp_yol, actual, MAX_DOSYA);

    /* [D-479] SATIR SONLARINI NORMALLESTIR — CR'leri at.
     *
     * OLCULDU: `.ast` anlik goruntuleri depoda CRLF ile duruyor (Windows/git
     * autocrlf). Windows'ta uretilen cikti da CRLF -> ham `memcmp` tutuyordu.
     * Linux'ta uretilen cikti LF -> 50 testin 50'si BASARISIZ.
     * Belirti tam imzasini veriyordu: "bekl=250 gerc=238" = 12 satir x 1 bayt.
     *
     * ⚠ NORMALLESTIRME DOGRU COZUM, dosyalari LF'e cevirmek DEGIL: bu testin
     * sordugu soru "AST DOKUMU ayni mi", "satir sonlari ayni mi" DEGIL.
     * Dosyalari LF yapsaydik Windows'ta uretilen CRLF cikti bu kez ORADA
     * dusecekti -- kusuru bir platformdan digerine tasimis olurduk. */
    b = cr_at(baseline, b);
    a = cr_at(actual, a);

    int esit = (rc == 0) && (b > 0) && (a > 0) && (b == a) &&
               (memcmp(baseline, actual, (size_t)b) == 0);

    if (esit) {
        basarili++;
        printf("  [%d] %s (%d byte) ... \xe2\x9c\x93\n",
               toplam_test, isim, b);
    } else {
        basarisiz++;
        printf("  [%d] %s ... \xe2\x9c\x97 (bekl=%d gerc=%d rc=%d)\n",
               toplam_test, isim, b, a, rc);
    }

    /* Temp dosyayi sil — clean test/snapshots/ */
    remove(tmp_yol);
    free(baseline);
    free(actual);
}

int main(void) {
    const char *snapshots[] = {
        "01_lit_tam",
        "02_aritmetik",
        "03_degisken",
        "04_islev_cagri",
        "05_eger",
        "06_iken",
        "07_yapi",
        "08_dizi",
        "09_eslesme",
        "10_lambda",
        "11_generic",
        "12_secimlik",
        "13_sonuc",
        "14_referans",
        "15_metin",
        "16_bit_op",
        "17_shift",
        "18_tekkez",
        "19_uygula_method",
        "20_ozyineli",
        /* === Genisletme (test altyapi) === */
        "21_modul_kullan",
        "22_ozellik_uygula",
        "23_generic_constraint",
        "24_nested_generic",
        "25_closure_capture",
        "26_referans_aktarim",
        "27_bolge_sahip",
        "28_escape_yerel",
        "29_linear_closure",
        "30_linear_region",
        "31_bit_komb",
        "32_arm64_kernel",
        "33_fib_recursive",
        "34_fizzbuzz",
        "35_binary_search",
        "36_quicksort_stub",
        "37_gcd_iter",
        "38_faktoriyel_iter",
        "39_yapi_iclice",
        "40_dizi_islemler",
        "41_eslesme_zincir",
        "42_lambda_hesap",
        "43_secimlik_zincir",
        "44_sonuc_pipeline",
        "45_kontrol_akis",
        "46_referans_yapi",
        "47_dizi_referans",
        "48_iclice_islev",
        "49_generic_method",
        "50_kompleks_program",
    };
    int n = (int)(sizeof(snapshots) / sizeof(snapshots[0]));

    printf("KEMGU Snapshot Test Paketi (ADIM 32)\n");
    printf("====================================\n\n");

    for (int i = 0; i < n; i++) {
        test_snapshot(snapshots[i]);
    }

    printf("\n====================================\n");
    printf("Toplam: %d | Basarili: %d | Basarisiz: %d\n",
           toplam_test, basarili, basarisiz);

    return basarisiz > 0 ? 1 : 0;
}
