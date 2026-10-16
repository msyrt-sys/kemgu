/*
 * KEMGU OTP CLI Integration Testleri
 *
 * Onkosul:
 *   - build/kemgu.exe (KEMGU derleyici)
 *   - build/kdl_runtime.o (runtime objesi)
 *   - test/ornekler/otp_cli.kem (KEMGU OTP CLI kaynak kodu)
 *
 * Bu test:
 *   1) otp_cli.kem -> .ll -> .exe derlemesi (bir kerelik)
 *   2) Round-trip: uret -> sifrele -> coz -> orijinal kontrol
 *   3) Negative: bilinmeyen komut, kisa anahtar
 *   4) Linear semantic: bu test C tarafindan; KEMGU linear (compile-time)
 *      kontrolu ayrica test_linear.c'de.
 */

/* [D-481] `WEXITSTATUS` bir MAKRODUR ve <sys/wait.h>DEN gelir. Eksikken
 * derleyici onu ORTUK ISLEV sanip devam etti; hata LINK ZAMANINDA cikti:
 *   undefined reference to `WEXITSTATUS'
 * Windows'ta gorunmez -- o dal `#ifdef` ile disarida.
 * ⚠ D-477'de bu dosyalari "zaten dogru" diye ELEMISTIM: `WEXITSTATUS`
 * GECIYOR diye bakmistim, ama GORUNMEK != DOGRU OLMAK. */
#ifndef _WIN32
#include <sys/wait.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
#define EXE_PATH ".\\build\\otp_cli.exe"
#else
#define DEV_NULL "/dev/null"
#define EXE_PATH "./build/otp_cli.exe"
#endif

#define KEM_SRC "test/ornekler/otp_cli.kem"
#define LL_PATH "build/otp_cli_test.ll"

/* Dosya buyukluk olc */
static long dosya_boyut(const char *yol) {
    FILE *f = fopen(yol, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fclose(f);
    return n;
}

/* Iki dosya icerigi ayni mi? */
static int dosyalar_esit(const char *a, const char *b) {
    FILE *fa = fopen(a, "rb");
    if (!fa) return 0;
    FILE *fb = fopen(b, "rb");
    if (!fb) { fclose(fa); return 0; }
    int esit = 1;
    while (1) {
        int ca = fgetc(fa);
        int cb = fgetc(fb);
        if (ca != cb) { esit = 0; break; }
        if (ca == EOF) break;
    }
    fclose(fa);
    fclose(fb);
    return esit;
}

/* OTP CLI'yi derle. Donus: 0 basari, 1 hata */
static int otp_cli_derle(void) {
    char komut[1024];
#ifdef _WIN32
    snprintf(komut, sizeof(komut),
        ".\\build\\kemgu.exe --llvm %s > %s 2>%s", KEM_SRC, LL_PATH, DEV_NULL);
#else
    snprintf(komut, sizeof(komut),
        "./build/kemgu --llvm %s > %s 2>%s", KEM_SRC, LL_PATH, DEV_NULL);
#endif
    if (system(komut) != 0) return 1;
#ifdef _WIN32
    snprintf(komut, sizeof(komut),
        "clang -x ir %s -x none build\\kdl_runtime.o -o %s 2>%s",
        LL_PATH, EXE_PATH, DEV_NULL);
#else
    snprintf(komut, sizeof(komut),
        "clang -x ir %s -x none build/kdl_runtime.o -o %s 2>%s",
        LL_PATH, EXE_PATH, DEV_NULL);
#endif
    if (system(komut) != 0) return 1;
    return 0;
}

/* OTP CLI komutunu calistir. Donus: exit kodu (-1 hata) */
static int otp_cagir(const char *args) {
    char komut[1024];
    snprintf(komut, sizeof(komut), "%s %s > %s 2>&1", EXE_PATH, args, DEV_NULL);
    int rc = system(komut);
#ifdef _WIN32
    return rc;
#else
    return WEXITSTATUS(rc);
#endif
}

/* === Unit testler (3) === */

static void test_kullanim_yazdirma(void) {
    /* Argsiz cagri -> exit 1 + kullanim mesaji */
    int rc = otp_cagir("");
    test_sonuc("argsiz cagri -> exit 1 (kullanim)", rc == 1);
}

static void test_uret_temel(void) {
    /* uret kucuk dosya */
    remove("build/test_otp_k.bin");
    int rc = otp_cagir("uret build/test_otp_k.bin 16");
    long boyut = dosya_boyut("build/test_otp_k.bin");
    test_sonuc("uret 16 -> 16 byte dosya", rc == 0 && boyut == 16);
}

static void test_uret_buyuk(void) {
    /* uret 128 byte */
    remove("build/test_otp_k128.bin");
    int rc = otp_cagir("uret build/test_otp_k128.bin 128");
    long boyut = dosya_boyut("build/test_otp_k128.bin");
    test_sonuc("uret 128 -> 128 byte dosya", rc == 0 && boyut == 128);
}

/* === Integration testler (3) === */

static void test_roundtrip_kucuk(void) {
    /* uret -> sifrele -> coz -> orijinal kontrol */
    FILE *f = fopen("build/test_otp_m.txt", "wb");
    fputs("Selam Dunya", f);
    fclose(f);

    int rc1 = otp_cagir("uret build/test_otp_k.bin 16");
    int rc2 = otp_cagir(
        "sifrele build/test_otp_m.txt build/test_otp_k.bin build/test_otp_c.bin");
    int rc3 = otp_cagir(
        "coz build/test_otp_c.bin build/test_otp_k.bin build/test_otp_d.txt");

    int esit = dosyalar_esit("build/test_otp_m.txt", "build/test_otp_d.txt");
    test_sonuc("roundtrip kucuk (11 byte) — orijinal eslesir",
               rc1 == 0 && rc2 == 0 && rc3 == 0 && esit);
}

static void test_roundtrip_orta(void) {
    /* Daha buyuk mesaj — 64 byte */
    FILE *f = fopen("build/test_otp_m2.txt", "wb");
    for (int i = 0; i < 64; i++) fputc('A' + (i % 26), f);
    fclose(f);

    int rc1 = otp_cagir("uret build/test_otp_k2.bin 64");
    int rc2 = otp_cagir(
        "sifrele build/test_otp_m2.txt build/test_otp_k2.bin build/test_otp_c2.bin");
    int rc3 = otp_cagir(
        "coz build/test_otp_c2.bin build/test_otp_k2.bin build/test_otp_d2.txt");

    int esit = dosyalar_esit("build/test_otp_m2.txt", "build/test_otp_d2.txt");
    test_sonuc("roundtrip orta (64 byte) — orijinal eslesir",
               rc1 == 0 && rc2 == 0 && rc3 == 0 && esit);
}

static void test_sifreli_farkli(void) {
    /* Sifrelenen, orijinalden farkli olmali */
    int esit = dosyalar_esit("build/test_otp_m.txt", "build/test_otp_c.bin");
    test_sonuc("sifreli icerik orijinalden farkli", !esit);
}

/* === Negative testler (3) === */

static void test_bilinmeyen_komut(void) {
    int rc = otp_cagir("xyz");
    test_sonuc("bilinmeyen komut -> exit 2", rc == 2);
}

static void test_eksik_args_uret(void) {
    int rc = otp_cagir("uret");
    test_sonuc("uret args eksik -> exit 1", rc == 1);
}

static void test_kisa_anahtar(void) {
    /* 8 byte anahtar, 64 byte mesaj -> exit 5 */
    int rc1 = otp_cagir("uret build/test_otp_k_kisa.bin 8");
    int rc2 = otp_cagir(
        "sifrele build/test_otp_m2.txt build/test_otp_k_kisa.bin build/test_otp_c_kisa.bin");
    test_sonuc("kisa anahtar (8 vs 64) -> exit 5",
               rc1 == 0 && rc2 == 5);
}

int main(void) {
    printf("KEMGU OTP CLI Integration Testleri\n");
    printf("===================================\n");

    /* Derle */
    printf("\n--- OTP CLI derleniyor ---\n");
    int derle_hata = otp_cli_derle();
    if (derle_hata) {
        printf("HATA: OTP CLI derlenmedi\n");
        return 1;
    }
    printf("Derleme OK\n");

    printf("\n--- Unit testler ---\n");
    test_kullanim_yazdirma();
    test_uret_temel();
    test_uret_buyuk();

    printf("\n--- Integration testler ---\n");
    test_roundtrip_kucuk();
    test_roundtrip_orta();
    test_sifreli_farkli();

    printf("\n--- Negative testler ---\n");
    test_bilinmeyen_komut();
    test_eksik_args_uret();
    test_kisa_anahtar();

    printf("\n===================================\n");
    printf("Toplam: %d | Basarili: %d | Basarisiz: %d\n",
           toplam, basarili, basarisiz);
    return basarisiz > 0 ? 1 : 0;
}
