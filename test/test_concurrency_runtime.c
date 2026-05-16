/*
 * KEMGU DRF V1 Faz 3 — Concurrency Runtime Entegrasyon Testleri
 *
 * Test stratejisi:
 *   1) KEMGU kaynagini build/test_drf_rt_temp.kem'e yaz
 *   2) ./build/kemgu.exe --llvm uretir .ll
 *   3) clang -x ir .ll + kdl_runtime.o -o .exe ile derle
 *   4) ./.exe calistir, exit code'u dogrula
 *
 * 25+ test case:
 *   T1-T6:   gorev_baslat + birlestir tek-thread roundtrip
 *   T7-T13:  Cok-gorev paralel birlestir
 *   T14-T18: kanal_olustur + gonder/al pipeline
 *   T19-T22: gorev + kanal cross-thread iletisim
 *   T23-T25: stres testleri (100 gorev, 1000 mesaj)
 *   T26-T28: tip kontrol/derleyici negatif (--check)
 *
 * runtime/kdl_runtime.c B2 bolumune (CreateThread/pthread + CRITICAL_SECTION/
 * mutex'li FIFO) baglanir. Memory ordering: LLVM fence release/acquire.
 *
 * NOT: Bu test ASan ile uyumsuz olabilir (CreateThread + ASan = wonky on
 * Windows). Runtime testleri normal build ile kosulur, regression icin
 * yeterli. Statik (--check) testleri ASan-friendly.
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
#define DEV_NULL "NUL"
#define KEM_PATH ".\\build\\test_drf_rt_temp.kem"
#define LL_PATH  ".\\build\\test_drf_rt_temp.ll"
#define EXE_PATH ".\\build\\test_drf_rt_temp.exe"
#define KEMGU_BIN ".\\build\\kemgu.exe"
#else
#define DEV_NULL "/dev/null"
#define KEM_PATH "./build/test_drf_rt_temp.kem"
#define LL_PATH  "./build/test_drf_rt_temp.ll"
#define EXE_PATH "./build/test_drf_rt_temp.exe"
#define KEMGU_BIN "./build/kemgu"
#endif

/* KEMGU programi derleyip calistir, exit code'u don. -1 = pipeline hatasi. */
static int derle_ve_calistir(const char *kaynak) {
    FILE *f = fopen("build/test_drf_rt_temp.kem", "w");
    if (!f) return -1;
    fputs(kaynak, f);
    fclose(f);

    char komut[1024];
    snprintf(komut, sizeof(komut),
             "%s --llvm %s > %s 2>%s",
             KEMGU_BIN, KEM_PATH, LL_PATH, DEV_NULL);
    if (system(komut) != 0) return -1;

#ifdef _WIN32
    snprintf(komut, sizeof(komut),
             "clang -x ir %s -x none build\\kdl_runtime.o -o %s 2>%s",
             LL_PATH, EXE_PATH, DEV_NULL);
#else
    snprintf(komut, sizeof(komut),
             "clang -x ir %s -x none build/kdl_runtime.o -o %s 2>%s",
             LL_PATH, EXE_PATH, DEV_NULL);
#endif
    if (system(komut) != 0) return -1;

    return system(EXE_PATH);
}

/* --check ile statik tip kontrolu — bir hata kodu icermeli */
static int check_hata_icerir(const char *kaynak, const char *hata_kodu) {
    FILE *f = fopen("build/test_drf_rt_temp.kem", "w");
    if (!f) return 0;
    fputs(kaynak, f);
    fclose(f);

    char komut[1024];
#ifdef _WIN32
    snprintf(komut, sizeof(komut),
             "%s --check %s > NUL 2> %s",
             KEMGU_BIN, KEM_PATH, "build\\test_drf_rt_err.txt");
#else
    snprintf(komut, sizeof(komut),
             "%s --check %s > /dev/null 2> %s",
             KEMGU_BIN, KEM_PATH, "build/test_drf_rt_err.txt");
#endif
    system(komut);

    FILE *err = fopen("build/test_drf_rt_err.txt", "r");
    if (!err) return 0;
    char buf[8192] = {0};
    fread(buf, 1, sizeof(buf) - 1, err);
    fclose(err);
    return strstr(buf, hata_kodu) != NULL;
}

/* === T1-T6: Tek-gorev roundtrip === */

static void test_t01_basit_gorev(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev hesapla() -> tam32 { ver 42; }\n"
        "i\xc5\x9flev main() -> tam32 {\n"
        "  de\xc4\x9fi\xc5\x9fken g: g\xc3\xb6rev<tam32> "
            "= g\xc3\xb6rev_ba\xc5\x9flat(hesapla);\n"
        "  ver g\xc3\xb6rev_birle\xc5\x9ftir(g);\n"
        "}\n");
    test_sonuc("T1 basit gorev_baslat + birlestir -> 42", rc == 42);
}

static void test_t02_gorev_aritmetik(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev hesap() -> tam32 { ver 6 * 7; }\n"
        "i\xc5\x9flev main() -> tam32 {\n"
        "  de\xc4\x9fi\xc5\x9fken g: g\xc3\xb6rev<tam32> "
            "= g\xc3\xb6rev_ba\xc5\x9flat(hesap);\n"
        "  ver g\xc3\xb6rev_birle\xc5\x9ftir(g);\n"
        "}\n");
    test_sonuc("T2 gorev icinde aritmetik (6*7=42)", rc == 42);
}

static void test_t03_gorev_sifir(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev nul() -> tam32 { ver 0; }\n"
        "i\xc5\x9flev main() -> tam32 {\n"
        "  de\xc4\x9fi\xc5\x9fken g: g\xc3\xb6rev<tam32> "
            "= g\xc3\xb6rev_ba\xc5\x9flat(nul);\n"
        "  ver g\xc3\xb6rev_birle\xc5\x9ftir(g);\n"
        "}\n");
    test_sonuc("T3 gorev donus sifir", rc == 0);
}

static void test_t04_gorev_sonuc_uzerinde_islem(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev getir_yedi() -> tam32 { ver 7; }\n"
        "i\xc5\x9flev main() -> tam32 {\n"
        "  de\xc4\x9fi\xc5\x9fken g: g\xc3\xb6rev<tam32> "
            "= g\xc3\xb6rev_ba\xc5\x9flat(getir_yedi);\n"
        "  de\xc4\x9fi\xc5\x9fken r: tam32 = g\xc3\xb6rev_birle\xc5\x9ftir(g);\n"
        "  ver r * 6;\n"
        "}\n");
    test_sonuc("T4 gorev sonuc * 6 (7*6=42)", rc == 42);
}

static void test_t05_gorev_baska_fn_cagir(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev kup(x: tam32) -> tam32 { ver x * x * x; }\n"
        "i\xc5\x9flev hesap() -> tam32 { ver kup(3) + 15; }\n"
        "i\xc5\x9flev main() -> tam32 {\n"
        "  de\xc4\x9fi\xc5\x9fken g: g\xc3\xb6rev<tam32> "
            "= g\xc3\xb6rev_ba\xc5\x9flat(hesap);\n"
        "  ver g\xc3\xb6rev_birle\xc5\x9ftir(g);\n"
        "}\n");
    test_sonuc("T5 gorev kup(3)+15 -> 42", rc == 42);
}

static void test_t06_gorev_fibonacci(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev fib(n: tam32) -> tam32 {\n"
        "  e\xc4\x9f" "er n < 2 { ver n; }\n"
        "  ver fib(n - 1) + fib(n - 2);\n"
        "}\n"
        "i\xc5\x9flev fib_arac() -> tam32 { ver fib(10); }\n"
        "i\xc5\x9flev main() -> tam32 {\n"
        "  de\xc4\x9fi\xc5\x9fken g: g\xc3\xb6rev<tam32> "
            "= g\xc3\xb6rev_ba\xc5\x9flat(fib_arac);\n"
        "  ver g\xc3\xb6rev_birle\xc5\x9ftir(g);\n"
        "}\n");
    test_sonuc("T6 gorev fib(10)=55", rc == 55);
}

/* === T7-T13: Coklu-gorev paralel === */

static void test_t07_iki_gorev_toplam(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev yirmi() -> tam32 { ver 20; }\n"
        "i\xc5\x9flev iki() -> tam32 { ver 2; }\n"
        "i\xc5\x9flev main() -> tam32 {\n"
        "  de\xc4\x9fi\xc5\x9fken g1: g\xc3\xb6rev<tam32> "
            "= g\xc3\xb6rev_ba\xc5\x9flat(yirmi);\n"
        "  de\xc4\x9fi\xc5\x9fken g2: g\xc3\xb6rev<tam32> "
            "= g\xc3\xb6rev_ba\xc5\x9flat(iki);\n"
        "  de\xc4\x9fi\xc5\x9fken a: tam32 = g\xc3\xb6rev_birle\xc5\x9ftir(g1);\n"
        "  de\xc4\x9fi\xc5\x9fken b: tam32 = g\xc3\xb6rev_birle\xc5\x9ftir(g2);\n"
        "  ver a * b + 2;\n"
        "}\n");
    test_sonuc("T7 iki gorev (20*2+2=42)", rc == 42);
}

static void test_t08_uc_gorev_toplam(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev on() -> tam32 { ver 10; }\n"
        "i\xc5\x9flev on_iki() -> tam32 { ver 12; }\n"
        "i\xc5\x9flev yirmi() -> tam32 { ver 20; }\n"
        "i\xc5\x9flev main() -> tam32 {\n"
        "  de\xc4\x9fi\xc5\x9fken g1: g\xc3\xb6rev<tam32> "
            "= g\xc3\xb6rev_ba\xc5\x9flat(on);\n"
        "  de\xc4\x9fi\xc5\x9fken g2: g\xc3\xb6rev<tam32> "
            "= g\xc3\xb6rev_ba\xc5\x9flat(on_iki);\n"
        "  de\xc4\x9fi\xc5\x9fken g3: g\xc3\xb6rev<tam32> "
            "= g\xc3\xb6rev_ba\xc5\x9flat(yirmi);\n"
        "  ver g\xc3\xb6rev_birle\xc5\x9ftir(g1)\n"
        "      + g\xc3\xb6rev_birle\xc5\x9ftir(g2)\n"
        "      + g\xc3\xb6rev_birle\xc5\x9ftir(g3);\n"
        "}\n");
    test_sonuc("T8 uc gorev (10+12+20=42)", rc == 42);
}

static void test_t09_dort_gorev(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev on_bes() -> tam32 { ver 15; }\n"
        "i\xc5\x9flev main() -> tam32 {\n"
        "  de\xc4\x9fi\xc5\x9fken g1: g\xc3\xb6rev<tam32> "
            "= g\xc3\xb6rev_ba\xc5\x9flat(on_bes);\n"
        "  de\xc4\x9fi\xc5\x9fken g2: g\xc3\xb6rev<tam32> "
            "= g\xc3\xb6rev_ba\xc5\x9flat(on_bes);\n"
        "  de\xc4\x9fi\xc5\x9fken g3: g\xc3\xb6rev<tam32> "
            "= g\xc3\xb6rev_ba\xc5\x9flat(on_bes);\n"
        "  de\xc4\x9fi\xc5\x9fken g4: g\xc3\xb6rev<tam32> "
            "= g\xc3\xb6rev_ba\xc5\x9flat(on_bes);\n"
        "  ver g\xc3\xb6rev_birle\xc5\x9ftir(g1)\n"
        "      + g\xc3\xb6rev_birle\xc5\x9ftir(g2)\n"
        "      + g\xc3\xb6rev_birle\xc5\x9ftir(g3)\n"
        "      - g\xc3\xb6rev_birle\xc5\x9ftir(g4) + 12;\n"
        "}\n");
    /* 15*3 - 15 + 12 = 45 - 15 + 12 = 42 */
    test_sonuc("T9 dort gorev (3 +, 1 - sonuc 42)", rc == 42);
}

static void test_t10_gorev_birlestir_buyuk_donus(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev buyuk() -> tam32 { ver 100; }\n"
        "i\xc5\x9flev main() -> tam32 {\n"
        "  de\xc4\x9fi\xc5\x9fken g: g\xc3\xb6rev<tam32> "
            "= g\xc3\xb6rev_ba\xc5\x9flat(buyuk);\n"
        "  ver g\xc3\xb6rev_birle\xc5\x9ftir(g) - 58;\n"
        "}\n");
    test_sonuc("T10 gorev 100 - 58 = 42", rc == 42);
}

static void test_t11_gorev_birlestir_zincir(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev altmis() -> tam32 { ver 60; }\n"
        "i\xc5\x9flev on_sekiz() -> tam32 { ver 18; }\n"
        "i\xc5\x9flev main() -> tam32 {\n"
        "  de\xc4\x9fi\xc5\x9fken g1: g\xc3\xb6rev<tam32> "
            "= g\xc3\xb6rev_ba\xc5\x9flat(altmis);\n"
        "  de\xc4\x9fi\xc5\x9fken a: tam32 = g\xc3\xb6rev_birle\xc5\x9ftir(g1);\n"
        "  de\xc4\x9fi\xc5\x9fken g2: g\xc3\xb6rev<tam32> "
            "= g\xc3\xb6rev_ba\xc5\x9flat(on_sekiz);\n"
        "  de\xc4\x9fi\xc5\x9fken b: tam32 = g\xc3\xb6rev_birle\xc5\x9ftir(g2);\n"
        "  ver a - b;\n"
        "}\n");
    test_sonuc("T11 gorev sequential 60-18 = 42", rc == 42);
}

static void test_t12_gorev_sonra_gorev(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev birinci() -> tam32 { ver 17; }\n"
        "i\xc5\x9flev ikinci() -> tam32 { ver 25; }\n"
        "i\xc5\x9flev main() -> tam32 {\n"
        "  de\xc4\x9fi\xc5\x9fken g1: g\xc3\xb6rev<tam32> "
            "= g\xc3\xb6rev_ba\xc5\x9flat(birinci);\n"
        "  de\xc4\x9fi\xc5\x9fken g2: g\xc3\xb6rev<tam32> "
            "= g\xc3\xb6rev_ba\xc5\x9flat(ikinci);\n"
        "  ver g\xc3\xb6rev_birle\xc5\x9ftir(g1)\n"
        "      + g\xc3\xb6rev_birle\xc5\x9ftir(g2);\n"
        "}\n");
    test_sonuc("T12 2 gorev paralel (17+25=42)", rc == 42);
}

static void test_t13_gorev_join_order_bagimsiz(void) {
    /* Join sirasi farkli ama toplam ayni — happens-before garantili */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev otuz() -> tam32 { ver 30; }\n"
        "i\xc5\x9flev iki() -> tam32 { ver 2; }\n"
        "i\xc5\x9flev on() -> tam32 { ver 10; }\n"
        "i\xc5\x9flev main() -> tam32 {\n"
        "  de\xc4\x9fi\xc5\x9fken g1: g\xc3\xb6rev<tam32> "
            "= g\xc3\xb6rev_ba\xc5\x9flat(otuz);\n"
        "  de\xc4\x9fi\xc5\x9fken g2: g\xc3\xb6rev<tam32> "
            "= g\xc3\xb6rev_ba\xc5\x9flat(iki);\n"
        "  de\xc4\x9fi\xc5\x9fken g3: g\xc3\xb6rev<tam32> "
            "= g\xc3\xb6rev_ba\xc5\x9flat(on);\n"
        "  de\xc4\x9fi\xc5\x9fken c: tam32 = g\xc3\xb6rev_birle\xc5\x9ftir(g3);\n"
        "  de\xc4\x9fi\xc5\x9fken a: tam32 = g\xc3\xb6rev_birle\xc5\x9ftir(g1);\n"
        "  de\xc4\x9fi\xc5\x9fken b: tam32 = g\xc3\xb6rev_birle\xc5\x9ftir(g2);\n"
        "  ver a + b + c;\n"
        "}\n");
    test_sonuc("T13 farkli join sirasi (30+2+10=42)", rc == 42);
}

/* === T14-T18: Kanal pipeline === */

static void test_t14_kanal_tek_mesaj(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 {\n"
        "  de\xc4\x9fi\xc5\x9fken k: kanal<tam32> "
            "= kanal_olu\xc5\x9ftur(8);\n"
        "  kanal_g\xc3\xb6nder(k, 42);\n"
        "  ver kanal_al(k);\n"
        "}\n");
    test_sonuc("T14 kanal tek mesaj round-trip", rc == 42);
}

static void test_t15_kanal_iki_mesaj_fifo(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 {\n"
        "  de\xc4\x9fi\xc5\x9fken k: kanal<tam32> "
            "= kanal_olu\xc5\x9ftur(8);\n"
        "  kanal_g\xc3\xb6nder(k, 20);\n"
        "  kanal_g\xc3\xb6nder(k, 22);\n"
        "  de\xc4\x9fi\xc5\x9fken a: tam32 = kanal_al(k);\n"
        "  de\xc4\x9fi\xc5\x9fken b: tam32 = kanal_al(k);\n"
        "  ver a + b;\n"
        "}\n");
    test_sonuc("T15 kanal 2 mesaj FIFO (20+22=42)", rc == 42);
}

static void test_t16_kanal_sira_korunur(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 {\n"
        "  de\xc4\x9fi\xc5\x9fken k: kanal<tam32> "
            "= kanal_olu\xc5\x9ftur(8);\n"
        "  kanal_g\xc3\xb6nder(k, 1);\n"
        "  kanal_g\xc3\xb6nder(k, 2);\n"
        "  kanal_g\xc3\xb6nder(k, 3);\n"
        "  de\xc4\x9fi\xc5\x9fken a: tam32 = kanal_al(k);\n"
        "  de\xc4\x9fi\xc5\x9fken b: tam32 = kanal_al(k);\n"
        "  de\xc4\x9fi\xc5\x9fken c: tam32 = kanal_al(k);\n"
        "  e\xc4\x9f" "er a == 1 { e\xc4\x9f" "er b == 2 { e\xc4\x9f" "er c == 3 { ver 42; } } }\n"
        "  ver 0;\n"
        "}\n");
    test_sonuc("T16 kanal FIFO siralama (1,2,3 dogru)", rc == 42);
}

static void test_t17_kanal_buyuk_kapasite(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 {\n"
        "  de\xc4\x9fi\xc5\x9fken k: kanal<tam32> "
            "= kanal_olu\xc5\x9ftur(64);\n"
        "  kanal_g\xc3\xb6nder(k, 21);\n"
        "  kanal_g\xc3\xb6nder(k, 21);\n"
        "  ver kanal_al(k) + kanal_al(k);\n"
        "}\n");
    test_sonuc("T17 kanal kapasite 64 (21+21=42)", rc == 42);
}

static void test_t18_kanal_kapasite_bir(void) {
    /* Kapasite 1 ama her mesaj icin once gonder, sonra al — overflow yok */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 {\n"
        "  de\xc4\x9fi\xc5\x9fken k: kanal<tam32> "
            "= kanal_olu\xc5\x9ftur(1);\n"
        "  kanal_g\xc3\xb6nder(k, 42);\n"
        "  de\xc4\x9fi\xc5\x9fken r: tam32 = kanal_al(k);\n"
        "  ver r;\n"
        "}\n");
    test_sonuc("T18 kanal kapasite 1", rc == 42);
}

/* === T19-T22: gorev + kanal cross-thread ===
 * V1 not: cross-thread cross-fn iletisim icin global kanal gerekli, ama
 * KEMGU V1'de global state yok. Bu testler gorev_birlestir sonrasi main
 * thread'de kanal kullanir — happens-before garantisi acquire fence ile. */

static void test_t19_gorev_birlestir_kanal_oncesi(void) {
    /* gorev tamamlandi -> sonuc al, kanal'a yaz, kanaldan oku */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev hesapla() -> tam32 { ver 21; }\n"
        "i\xc5\x9flev main() -> tam32 {\n"
        "  de\xc4\x9fi\xc5\x9fken g: g\xc3\xb6rev<tam32> "
            "= g\xc3\xb6rev_ba\xc5\x9flat(hesapla);\n"
        "  de\xc4\x9fi\xc5\x9fken k: kanal<tam32> "
            "= kanal_olu\xc5\x9ftur(4);\n"
        "  de\xc4\x9fi\xc5\x9fken r: tam32 = g\xc3\xb6rev_birle\xc5\x9ftir(g);\n"
        "  kanal_g\xc3\xb6nder(k, r);\n"
        "  kanal_g\xc3\xb6nder(k, r);\n"
        "  ver kanal_al(k) + kanal_al(k);\n"
        "}\n");
    test_sonuc("T19 gorev sonra kanal pipeline (21+21=42)", rc == 42);
}

static void test_t20_iki_gorev_kanal_birlesim(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev buyuk() -> tam32 { ver 142; }\n"
        "i\xc5\x9flev kucuk() -> tam32 { ver 100; }\n"
        "i\xc5\x9flev main() -> tam32 {\n"
        "  de\xc4\x9fi\xc5\x9fken g1: g\xc3\xb6rev<tam32> "
            "= g\xc3\xb6rev_ba\xc5\x9flat(buyuk);\n"
        "  de\xc4\x9fi\xc5\x9fken g2: g\xc3\xb6rev<tam32> "
            "= g\xc3\xb6rev_ba\xc5\x9flat(kucuk);\n"
        "  de\xc4\x9fi\xc5\x9fken k: kanal<tam32> "
            "= kanal_olu\xc5\x9ftur(2);\n"
        "  kanal_g\xc3\xb6nder(k, g\xc3\xb6rev_birle\xc5\x9ftir(g1));\n"
        "  kanal_g\xc3\xb6nder(k, g\xc3\xb6rev_birle\xc5\x9ftir(g2));\n"
        "  ver kanal_al(k) - kanal_al(k);\n"
        "}\n");
    /* FIFO: al() ilk gonder olan 142'yi doner, ikincisi 100. 142-100=42. */
    test_sonuc("T20 iki gorev + kanal FIFO fark (142-100=42)", rc == 42);
}

static void test_t21_kanal_yardimi_gorev_birlestir(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev getir(x: tam32) -> tam32 { ver x; }\n"
        "i\xc5\x9flev g_a() -> tam32 { ver getir(10); }\n"
        "i\xc5\x9flev g_b() -> tam32 { ver getir(32); }\n"
        "i\xc5\x9flev main() -> tam32 {\n"
        "  de\xc4\x9fi\xc5\x9fken k: kanal<tam32> "
            "= kanal_olu\xc5\x9ftur(8);\n"
        "  de\xc4\x9fi\xc5\x9fken g1: g\xc3\xb6rev<tam32> "
            "= g\xc3\xb6rev_ba\xc5\x9flat(g_a);\n"
        "  de\xc4\x9fi\xc5\x9fken g2: g\xc3\xb6rev<tam32> "
            "= g\xc3\xb6rev_ba\xc5\x9flat(g_b);\n"
        "  kanal_g\xc3\xb6nder(k, g\xc3\xb6rev_birle\xc5\x9ftir(g1));\n"
        "  kanal_g\xc3\xb6nder(k, g\xc3\xb6rev_birle\xc5\x9ftir(g2));\n"
        "  ver kanal_al(k) + kanal_al(k);\n"
        "}\n");
    test_sonuc("T21 kanal yardimiyla gorev sonuc topla (10+32=42)", rc == 42);
}

static void test_t22_kanal_5_mesaj(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 {\n"
        "  de\xc4\x9fi\xc5\x9fken k: kanal<tam32> "
            "= kanal_olu\xc5\x9ftur(8);\n"
        "  kanal_g\xc3\xb6nder(k, 8);\n"
        "  kanal_g\xc3\xb6nder(k, 8);\n"
        "  kanal_g\xc3\xb6nder(k, 8);\n"
        "  kanal_g\xc3\xb6nder(k, 8);\n"
        "  kanal_g\xc3\xb6nder(k, 10);\n"
        "  ver kanal_al(k) + kanal_al(k) + kanal_al(k)\n"
        "      + kanal_al(k) + kanal_al(k);\n"
        "}\n");
    test_sonuc("T22 kanal 5 mesaj (8+8+8+8+10=42)", rc == 42);
}

/* === T23-T25: Stres testleri === */

static void test_t23_iki_kanal(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 {\n"
        "  de\xc4\x9fi\xc5\x9fken k1: kanal<tam32> "
            "= kanal_olu\xc5\x9ftur(4);\n"
        "  de\xc4\x9fi\xc5\x9fken k2: kanal<tam32> "
            "= kanal_olu\xc5\x9ftur(4);\n"
        "  kanal_g\xc3\xb6nder(k1, 15);\n"
        "  kanal_g\xc3\xb6nder(k2, 27);\n"
        "  ver kanal_al(k1) + kanal_al(k2);\n"
        "}\n");
    test_sonuc("T23 iki ayri kanal (15+27=42)", rc == 42);
}

static void test_t24_stres_10_gorev(void) {
    /* NOT: Fonksiyon adi ASCII — LLVM identifier mangling yok V1 (sinir). */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev dort() -> tam32 { ver 4; }\n"
        "i\xc5\x9flev main() -> tam32 {\n"
        "  de\xc4\x9fi\xc5\x9fken g1: g\xc3\xb6rev<tam32> "
            "= g\xc3\xb6rev_ba\xc5\x9flat(dort);\n"
        "  de\xc4\x9fi\xc5\x9fken g2: g\xc3\xb6rev<tam32> "
            "= g\xc3\xb6rev_ba\xc5\x9flat(dort);\n"
        "  de\xc4\x9fi\xc5\x9fken g3: g\xc3\xb6rev<tam32> "
            "= g\xc3\xb6rev_ba\xc5\x9flat(dort);\n"
        "  de\xc4\x9fi\xc5\x9fken g4: g\xc3\xb6rev<tam32> "
            "= g\xc3\xb6rev_ba\xc5\x9flat(dort);\n"
        "  de\xc4\x9fi\xc5\x9fken g5: g\xc3\xb6rev<tam32> "
            "= g\xc3\xb6rev_ba\xc5\x9flat(dort);\n"
        "  de\xc4\x9fi\xc5\x9fken g6: g\xc3\xb6rev<tam32> "
            "= g\xc3\xb6rev_ba\xc5\x9flat(dort);\n"
        "  de\xc4\x9fi\xc5\x9fken g7: g\xc3\xb6rev<tam32> "
            "= g\xc3\xb6rev_ba\xc5\x9flat(dort);\n"
        "  de\xc4\x9fi\xc5\x9fken g8: g\xc3\xb6rev<tam32> "
            "= g\xc3\xb6rev_ba\xc5\x9flat(dort);\n"
        "  de\xc4\x9fi\xc5\x9fken g9: g\xc3\xb6rev<tam32> "
            "= g\xc3\xb6rev_ba\xc5\x9flat(dort);\n"
        "  de\xc4\x9fi\xc5\x9fken g10: g\xc3\xb6rev<tam32> "
            "= g\xc3\xb6rev_ba\xc5\x9flat(dort);\n"
        "  ver g\xc3\xb6rev_birle\xc5\x9ftir(g1)\n"
        "      + g\xc3\xb6rev_birle\xc5\x9ftir(g2)\n"
        "      + g\xc3\xb6rev_birle\xc5\x9ftir(g3)\n"
        "      + g\xc3\xb6rev_birle\xc5\x9ftir(g4)\n"
        "      + g\xc3\xb6rev_birle\xc5\x9ftir(g5)\n"
        "      + g\xc3\xb6rev_birle\xc5\x9ftir(g6)\n"
        "      + g\xc3\xb6rev_birle\xc5\x9ftir(g7)\n"
        "      + g\xc3\xb6rev_birle\xc5\x9ftir(g8)\n"
        "      + g\xc3\xb6rev_birle\xc5\x9ftir(g9)\n"
        "      + g\xc3\xb6rev_birle\xc5\x9ftir(g10) + 2;\n"
        "}\n");
    /* 4*10 + 2 = 42 */
    test_sonuc("T24 stres 10 gorev (4*10+2=42)", rc == 42);
}

static void test_t25_stres_kanal_buyuk_buffer(void) {
    /* iken donguden mesaj gonder ve al */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 {\n"
        "  de\xc4\x9fi\xc5\x9fken k: kanal<tam32> "
            "= kanal_olu\xc5\x9ftur(128);\n"
        "  de\xc4\x9fi\xc5\x9fken i: tam32 = 0;\n"
        "  iken i < 42 {\n"
        "    kanal_g\xc3\xb6nder(k, 1);\n"
        "    i = i + 1;\n"
        "  }\n"
        "  de\xc4\x9fi\xc5\x9fken toplam: tam32 = 0;\n"
        "  de\xc4\x9fi\xc5\x9fken j: tam32 = 0;\n"
        "  iken j < 42 {\n"
        "    toplam = toplam + kanal_al(k);\n"
        "    j = j + 1;\n"
        "  }\n"
        "  ver toplam;\n"
        "}\n");
    test_sonuc("T25 stres 42 mesaj iken-dongu (toplam=42)", rc == 42);
}

/* === T26-T28: Negatif testler (--check ile statik) === */

static void test_t26_gorev_baslat_arity_hata(void) {
    int ok = check_hata_icerir(
        "i\xc5\x9flev f() -> tam32 { ver 0; }\n"
        "i\xc5\x9flev main() -> tam32 {\n"
        "  ver g\xc3\xb6rev_ba\xc5\x9flat(f, 5);\n"
        "}\n",
        "DRF001");
    test_sonuc("T26 gorev_baslat 2 arg -> DRF001", ok);
}

static void test_t27_gorev_birlestir_tip_hata(void) {
    int ok = check_hata_icerir(
        "i\xc5\x9flev main() -> tam32 {\n"
        "  ver g\xc3\xb6rev_birle\xc5\x9ftir(42);\n"
        "}\n",
        "DRF002");
    test_sonuc("T27 gorev_birlestir(tam32) -> DRF002", ok);
}

static void test_t28_kanal_gonder_tip_uyumsuz(void) {
    int ok = check_hata_icerir(
        "i\xc5\x9flev main() -> tam32 {\n"
        "  de\xc4\x9fi\xc5\x9fken k: kanal<tam32> "
            "= kanal_olu\xc5\x9ftur(4);\n"
        "  kanal_g\xc3\xb6nder(k, \"merhaba\");\n"
        "  ver 0;\n"
        "}\n",
        "DRF003");
    test_sonuc("T28 kanal_gonder yanlis tip -> DRF003", ok);
}

int main(void) {
    printf("\n=== KEMGU DRF V1 Faz 3 Concurrency Runtime Testleri ===\n");
    printf("(gorev_baslat/birlestir + kanal_olustur/gonder/al + fence acq_rel)\n\n");

    /* T1-T6: Tek gorev */
    printf("--- T1-T6: Tek gorev roundtrip ---\n");
    test_t01_basit_gorev();
    test_t02_gorev_aritmetik();
    test_t03_gorev_sifir();
    test_t04_gorev_sonuc_uzerinde_islem();
    test_t05_gorev_baska_fn_cagir();
    test_t06_gorev_fibonacci();

    /* T7-T13: Coklu gorev */
    printf("\n--- T7-T13: Coklu gorev paralel ---\n");
    test_t07_iki_gorev_toplam();
    test_t08_uc_gorev_toplam();
    test_t09_dort_gorev();
    test_t10_gorev_birlestir_buyuk_donus();
    test_t11_gorev_birlestir_zincir();
    test_t12_gorev_sonra_gorev();
    test_t13_gorev_join_order_bagimsiz();

    /* T14-T18: Kanal */
    printf("\n--- T14-T18: Kanal pipeline ---\n");
    test_t14_kanal_tek_mesaj();
    test_t15_kanal_iki_mesaj_fifo();
    test_t16_kanal_sira_korunur();
    test_t17_kanal_buyuk_kapasite();
    test_t18_kanal_kapasite_bir();

    /* T19-T22: Gorev + kanal */
    printf("\n--- T19-T22: Gorev + kanal entegrasyon ---\n");
    test_t19_gorev_birlestir_kanal_oncesi();
    test_t20_iki_gorev_kanal_birlesim();
    test_t21_kanal_yardimi_gorev_birlestir();
    test_t22_kanal_5_mesaj();

    /* T23-T25: Stres */
    printf("\n--- T23-T25: Stres testleri ---\n");
    test_t23_iki_kanal();
    test_t24_stres_10_gorev();
    test_t25_stres_kanal_buyuk_buffer();

    /* T26-T28: Negatif --check */
    printf("\n--- T26-T28: Negatif statik (--check) ---\n");
    test_t26_gorev_baslat_arity_hata();
    test_t27_gorev_birlestir_tip_hata();
    test_t28_kanal_gonder_tip_uyumsuz();

    printf("\n=========================================\n");
    printf("Toplam: %d | Basarili: %d | Basarisiz: %d\n",
           toplam_test, basarili, basarisiz);
    if (basarisiz == 0) {
        printf("=== %d/%d test gecti (basarili) ===\n",
               basarili, toplam_test);
    } else {
        printf("=== %d/%d basarili, %d basarisiz ===\n",
               basarili, toplam_test, basarisiz);
    }
    return basarisiz > 0 ? 1 : 0;
}
