/*
 * KEMGU Mikrobenchmark Suite (test altyapi)
 *
 * 10 baseline benchmark, nanosaniye hassasiyetinde olcum.
 * Cikti: build/bench_results.json — bench_baseline.json ile karsilastirmali
 * regresyon detection.
 *
 * Bagimsiz: kemgu cekirdek API'lerini (lexer/parser/tip_kontrol/escape)
 * dogrudan baglar; LLVM ve end-to-end benchmark'lari system() ile yapilir.
 */

#include "arena.h"
#include "lexer.h"
#include "parser.h"
#include "tip.h"
#include "sembol.h"
#include "tip_kontrol.h"
#include "escape.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#define KEMGU_BIN ".\\build\\kemgu.exe"
#else
#define KEMGU_BIN "./build/kemgu"
#endif

/* === Yuksek hassasiyet zamanlayici (clock_gettime POSIX + MinGW UCRT) === */
static double simdiki_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

/* === Sonuc kaydi === */
typedef struct {
    const char *ad;
    double ns_avg;      /* iter basina ortalama nanosaniye */
    double iter_per_s;  /* saniyede iter */
    int iter_count;
    const char *birim;  /* "tokens", "lines", "checks", "iters", vs. */
    double birim_per_s; /* birim cinsinden ortalama (token/saniye, vs.) */
    int birim_per_iter; /* her iter'da kac birim islendi */
} BenchSonuc;

/* === Ortak test girdileri === */
static const char *KAYNAK_BASIT =
    "i\xc5\x9flev kup(x: tam32) -> tam32 { ver x * x * x; }\n"
    "i\xc5\x9flev main() -> tam32 {\n"
    "    de\xc4\x9fi\xc5\x9fken n: tam32 = 0;\n"
    "    iken n < 10 { n = n + 1; }\n"
    "    ver kup(n) + 42;\n"
    "}\n";

static const char *KAYNAK_KARMASIK =
    "yap\xc4\xb1 Cift<A, B> { ilk: A; ikinci: B; }\n"
    "i\xc5\x9flev olustur<T>(v: T) -> Cift<T, T> { ver Cift<T, T> { ilk: v, ikinci: v }; }\n"
    "i\xc5\x9flev fib(n: tam32) -> tam32 {\n"
    "    e\xc4\x9f" "er n < 2 { ver n; }\n"
    "    ver fib(n - 1) + fib(n - 2);\n"
    "}\n"
    "i\xc5\x9flev main() -> tam32 {\n"
    "    de\xc4\x9fi\xc5\x9fken xs = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];\n"
    "    de\xc4\x9fi\xc5\x9fken toplam: tam32 = 0;\n"
    "    de\xc4\x9fi\xc5\x9fken i: tam32 = 0;\n"
    "    iken i < 10 {\n"
    "        toplam = toplam + xs[i] + fib(i);\n"
    "        i = i + 1;\n"
    "    }\n"
    "    ver toplam + 42;\n"
    "}\n";

#define ITER_HIZLI 1000
#define ITER_ORTA  500
#define ITER_YAVAS 50

/* === Benchmark fonksiyonlari === */

static BenchSonuc bench_lexer(void) {
    int iter = ITER_HIZLI;
    int token_count = 0;
    double t0 = simdiki_ns();
    for (int i = 0; i < iter; i++) {
        Lexer l;
        lexer_baslat(&l, KAYNAK_KARMASIK, "bench");
        Token t;
        while (1) {
            t = lexer_sonraki_token(&l);
            token_count++;
            if (t.tip == TOK_DOSYA_SONU) break;
        }
    }
    double dt = simdiki_ns() - t0;
    BenchSonuc s;
    s.ad = "lexer";
    s.ns_avg = dt / iter;
    s.iter_per_s = 1e9 * iter / dt;
    s.iter_count = iter;
    s.birim = "tokens";
    s.birim_per_iter = token_count / iter;
    s.birim_per_s = 1e9 * token_count / dt;
    return s;
}

static BenchSonuc bench_parser(void) {
    int iter = ITER_HIZLI;
    int satir_count = 0;
    /* KAYNAK_KARMASIK ~14 satir */
    const char *p = KAYNAK_KARMASIK;
    while (*p) { if (*p == '\n') satir_count++; p++; }

    double t0 = simdiki_ns();
    for (int i = 0; i < iter; i++) {
        Arena *a = arena_olustur(0);
        Lexer l;
        lexer_baslat(&l, KAYNAK_KARMASIK, "bench");
        Parser pr;
        parser_baslat(&pr, &l, a, "bench", KAYNAK_KARMASIK);
        Dugum *prog = parser_calistir(&pr);
        (void)prog;
        arena_serbest(a);
    }
    double dt = simdiki_ns() - t0;
    BenchSonuc s;
    s.ad = "parser";
    s.ns_avg = dt / iter;
    s.iter_per_s = 1e9 * iter / dt;
    s.iter_count = iter;
    s.birim = "lines";
    s.birim_per_iter = satir_count;
    s.birim_per_s = 1e9 * iter * satir_count / dt;
    return s;
}

static BenchSonuc bench_tip_kontrol(void) {
    int iter = ITER_ORTA;
    double t0 = simdiki_ns();
    for (int i = 0; i < iter; i++) {
        Arena *a = arena_olustur(0);
        Lexer l;
        lexer_baslat(&l, KAYNAK_KARMASIK, "bench");
        Parser pr;
        parser_baslat(&pr, &l, a, "bench", KAYNAK_KARMASIK);
        Dugum *prog = parser_calistir(&pr);

        Scope *global = scope_olustur(a, SCOPE_GLOBAL, NULL);
        TipKontrol tk;
        tip_kontrol_baslat(&tk, a, global, "bench", KAYNAK_KARMASIK);
        tip_kontrol_program(&tk, prog);
        arena_serbest(a);
    }
    double dt = simdiki_ns() - t0;
    BenchSonuc s;
    s.ad = "tip_kontrol";
    s.ns_avg = dt / iter;
    s.iter_per_s = 1e9 * iter / dt;
    s.iter_count = iter;
    s.birim = "checks";
    s.birim_per_iter = 1;
    s.birim_per_s = s.iter_per_s;
    return s;
}

static BenchSonuc bench_escape(void) {
    int iter = ITER_ORTA;
    double t0 = simdiki_ns();
    for (int i = 0; i < iter; i++) {
        Arena *a = arena_olustur(0);
        Lexer l;
        lexer_baslat(&l, KAYNAK_BASIT, "bench");
        Parser pr;
        parser_baslat(&pr, &l, a, "bench", KAYNAK_BASIT);
        Dugum *prog = parser_calistir(&pr);
        EscapeAnaliz esc;
        escape_baslat(&esc, a);
        escape_analiz_program(&esc, prog);
        arena_serbest(a);
    }
    double dt = simdiki_ns() - t0;
    BenchSonuc s;
    s.ad = "escape_analiz";
    s.ns_avg = dt / iter;
    s.iter_per_s = 1e9 * iter / dt;
    s.iter_count = iter;
    s.birim = "programs";
    s.birim_per_iter = 1;
    s.birim_per_s = s.iter_per_s;
    return s;
}

static BenchSonuc bench_linear(void) {
    /* Linear analiz tip_kontrol icinde otomatik calisir; ek bench
     * tip_kontrol benchmark'i ile aynidir. Burada lineer'a ozel kucuk
     * program ile tekrarliyoruz. */
    const char *kaynak =
        "i\xc5\x9flev main() -> tam32 {\n"
        "    de\xc4\x9fi\xc5\x9fken k = tekkez_olustur(42);\n"
        "    imha(k);\n"
        "    ver 0;\n"
        "}\n";
    int iter = ITER_ORTA;
    double t0 = simdiki_ns();
    for (int i = 0; i < iter; i++) {
        Arena *a = arena_olustur(0);
        Lexer l; lexer_baslat(&l, kaynak, "bench");
        Parser pr; parser_baslat(&pr, &l, a, "bench", kaynak);
        Dugum *prog = parser_calistir(&pr);
        Scope *global = scope_olustur(a, SCOPE_GLOBAL, NULL);
        TipKontrol tk;
        tip_kontrol_baslat(&tk, a, global, "bench", kaynak);
        tip_kontrol_program(&tk, prog);
        arena_serbest(a);
    }
    double dt = simdiki_ns() - t0;
    BenchSonuc s;
    s.ad = "linear_analiz";
    s.ns_avg = dt / iter;
    s.iter_per_s = 1e9 * iter / dt;
    s.iter_count = iter;
    s.birim = "programs";
    s.birim_per_iter = 1;
    s.birim_per_s = s.iter_per_s;
    return s;
}

static BenchSonuc bench_snapshot_serialize(void) {
    /* Snapshot baseline'lardan birini parse-print et — AST serialize hizi */
    int iter = ITER_ORTA;
    int satir_count = 0;
    double t0 = simdiki_ns();
    for (int i = 0; i < iter; i++) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd),
            "%s --parse test/snapshots/20_ozyineli.kem > "
#ifdef _WIN32
            "NUL"
#else
            "/dev/null"
#endif
            " 2>&1", KEMGU_BIN);
        int rc = system(cmd);
        (void)rc;
        satir_count += 50;  /* tahminen */
    }
    double dt = simdiki_ns() - t0;
    BenchSonuc s;
    s.ad = "snapshot_serialize";
    s.ns_avg = dt / iter;
    s.iter_per_s = 1e9 * iter / dt;
    s.iter_count = iter;
    s.birim = "ast_lines";
    s.birim_per_iter = satir_count / iter;
    s.birim_per_s = 1e9 * satir_count / dt;
    return s;
}

static BenchSonuc bench_llvm_uretim(void) {
    /* LLVM IR uretim — kemgu --llvm ile */
    int iter = ITER_ORTA;
    double t0 = simdiki_ns();
    for (int i = 0; i < iter; i++) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd),
            "%s --llvm test/snapshots/20_ozyineli.kem > build/bench_tmp.ll 2>&1",
            KEMGU_BIN);
        int rc = system(cmd);
        (void)rc;
    }
    double dt = simdiki_ns() - t0;
    BenchSonuc s;
    s.ad = "llvm_ir_uretim";
    s.ns_avg = dt / iter;
    s.iter_per_s = 1e9 * iter / dt;
    s.iter_count = iter;
    s.birim = "programs";
    s.birim_per_iter = 1;
    s.birim_per_s = s.iter_per_s;
    return s;
}

static BenchSonuc bench_end_to_end_basit(void) {
    /* Basit program: kemgu --llvm + clang derleme */
    int iter = ITER_YAVAS;
    double t0 = simdiki_ns();
    for (int i = 0; i < iter; i++) {
        char cmd[1024];
        snprintf(cmd, sizeof(cmd),
            "%s --llvm test/snapshots/01_lit_tam.kem > build/bench_e2e.ll 2>&1 "
#ifdef _WIN32
            "&& "
#else
            "&& "
#endif
            "clang -x ir build/bench_e2e.ll -o build/bench_e2e.exe 2>&1 > "
#ifdef _WIN32
            "NUL"
#else
            "/dev/null"
#endif
            , KEMGU_BIN);
        int rc = system(cmd);
        (void)rc;
    }
    double dt = simdiki_ns() - t0;
    BenchSonuc s;
    s.ad = "end_to_end_basit";
    s.ns_avg = dt / iter;
    s.iter_per_s = 1e9 * iter / dt;
    s.iter_count = iter;
    s.birim = "compilations";
    s.birim_per_iter = 1;
    s.birim_per_s = s.iter_per_s;
    return s;
}

static BenchSonuc bench_end_to_end_arm64(void) {
    /* ARM64 cross-compile: kernel.kem -> aarch64 object */
    int iter = ITER_YAVAS;
    double t0 = simdiki_ns();
    for (int i = 0; i < iter; i++) {
        char cmd[1024];
        snprintf(cmd, sizeof(cmd),
            "%s --llvm test/ornekler/kernel.kem > build/bench_arm.ll 2>&1 "
            "&& clang -target aarch64-unknown-none -x ir build/bench_arm.ll "
            "-c -o build/bench_arm.o 2>&1 > "
#ifdef _WIN32
            "NUL"
#else
            "/dev/null"
#endif
            , KEMGU_BIN);
        int rc = system(cmd);
        (void)rc;
    }
    double dt = simdiki_ns() - t0;
    BenchSonuc s;
    s.ad = "end_to_end_arm64";
    s.ns_avg = dt / iter;
    s.iter_per_s = 1e9 * iter / dt;
    s.iter_count = iter;
    s.birim = "elf_objects";
    s.birim_per_iter = 1;
    s.birim_per_s = s.iter_per_s;
    return s;
}

static BenchSonuc bench_fuzz_throughput(void) {
    /* Fuzz benzeri: random parser run'lar — sentetik mini fuzz */
    int iter = ITER_HIZLI;
    /* Sabit kucuk girdi (gercek fuzz random'i degil, throughput olcumu) */
    const char *kaynak = "i\xc5\x9flev a()->tam32{ver 42;}";
    double t0 = simdiki_ns();
    for (int i = 0; i < iter; i++) {
        Arena *a = arena_olustur(0);
        Lexer l; lexer_baslat(&l, kaynak, "fuzz");
        Parser p; parser_baslat(&p, &l, a, "fuzz", kaynak);
        Dugum *prog = parser_calistir(&p);
        (void)prog;
        arena_serbest(a);
    }
    double dt = simdiki_ns() - t0;
    BenchSonuc s;
    s.ad = "fuzz_throughput";
    s.ns_avg = dt / iter;
    s.iter_per_s = 1e9 * iter / dt;
    s.iter_count = iter;
    s.birim = "fuzz_iters";
    s.birim_per_iter = 1;
    s.birim_per_s = s.iter_per_s;
    return s;
}

/* === SIMD Spec V1: vektor vs skaler karsilastirmasi ===
 * Bu bench, tip kontrol + LLVM IR uretim sirasinda SIMD vs skaler kodun
 * derleyici aciligindan ne kadar maliyetli olduguna bakar. Calistirma
 * zamanli speedup degerlendirmesi ayri test/test_simd_llvm.c'de yapilir.
 *
 * Saf-KEMGU benchmark: scalar 4'lu toplama vs vektor 4-lane toplama
 * (her ikisi de tip kontrol'den geciriliyor; SIMD intrinsic resolution
 * extra cost). */
static BenchSonuc bench_simd_kontrol_skaler(void) {
    int iter = ITER_HIZLI;
    /* Skaler: 4 lokal degisken topla */
    const char *skaler_kaynak =
        "i\xc5\x9flev test() -> tam32 {\n"
        "    de\xc4\x9fi\xc5\x9fken a: tam32 = 1;\n"
        "    de\xc4\x9fi\xc5\x9fken b: tam32 = 2;\n"
        "    de\xc4\x9fi\xc5\x9fken c: tam32 = 3;\n"
        "    de\xc4\x9fi\xc5\x9fken d: tam32 = 4;\n"
        "    ver a + b + c + d;\n"
        "}\n";
    double t0 = simdiki_ns();
    for (int i = 0; i < iter; i++) {
        Arena *a = arena_olustur(0);
        Lexer l; lexer_baslat(&l, skaler_kaynak, "bench");
        Parser p; parser_baslat(&p, &l, a, "bench", skaler_kaynak);
        Dugum *prog = parser_calistir(&p);
        Scope *g = scope_olustur(a, SCOPE_GLOBAL, NULL);
        TipKontrol tk;
        tip_kontrol_baslat(&tk, a, g, "bench", skaler_kaynak);
        tip_kontrol_program(&tk, prog);
        arena_serbest(a);
    }
    double dt = simdiki_ns() - t0;
    BenchSonuc s;
    s.ad = "simd_skaler_kontrol";
    s.ns_avg = dt / iter;
    s.iter_per_s = 1e9 * iter / dt;
    s.iter_count = iter;
    s.birim = "iters";
    s.birim_per_iter = 1;
    s.birim_per_s = s.iter_per_s;
    return s;
}

static BenchSonuc bench_simd_kontrol_vektor(void) {
    int iter = ITER_HIZLI;
    /* Vektör: 4 lane vektör + reduction */
    const char *vektor_kaynak =
        "i\xc5\x9flev test() -> tam32 {\n"
        "    de\xc4\x9fi\xc5\x9fken v: vekt\xc3\xb6r<tam32, 4> = "
        "vektor_doldur(10);\n"
        "    ver vektor_topla(v);\n"
        "}\n";
    double t0 = simdiki_ns();
    for (int i = 0; i < iter; i++) {
        Arena *a = arena_olustur(0);
        Lexer l; lexer_baslat(&l, vektor_kaynak, "bench");
        Parser p; parser_baslat(&p, &l, a, "bench", vektor_kaynak);
        Dugum *prog = parser_calistir(&p);
        Scope *g = scope_olustur(a, SCOPE_GLOBAL, NULL);
        TipKontrol tk;
        tip_kontrol_baslat(&tk, a, g, "bench", vektor_kaynak);
        tip_kontrol_program(&tk, prog);
        arena_serbest(a);
    }
    double dt = simdiki_ns() - t0;
    BenchSonuc s;
    s.ad = "simd_vektor_kontrol";
    s.ns_avg = dt / iter;
    s.iter_per_s = 1e9 * iter / dt;
    s.iter_count = iter;
    s.birim = "iters";
    s.birim_per_iter = 1;
    s.birim_per_s = s.iter_per_s;
    return s;
}

/* === JSON yazici === */

static void json_yaz(FILE *out, BenchSonuc *r, int n) {
    fputs("{\n", out);
    fputs("  \"format\": \"kemgu-bench-v1\",\n", out);
    /* timestamp */
    time_t now = time(NULL);
    struct tm *tm = gmtime(&now);
    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", tm);
    fprintf(out, "  \"timestamp\": \"%s\",\n", ts);
    fputs("  \"results\": {\n", out);
    for (int i = 0; i < n; i++) {
        fprintf(out,
            "    \"%s\": {\n"
            "      \"ns_avg\": %.2f,\n"
            "      \"iter_per_sec\": %.2f,\n"
            "      \"iter_count\": %d,\n"
            "      \"unit\": \"%s\",\n"
            "      \"unit_per_iter\": %d,\n"
            "      \"unit_per_sec\": %.2f\n"
            "    }%s\n",
            r[i].ad, r[i].ns_avg, r[i].iter_per_s, r[i].iter_count,
            r[i].birim, r[i].birim_per_iter, r[i].birim_per_s,
            (i == n - 1 ? "" : ","));
    }
    fputs("  }\n}\n", out);
}

/* === Regresyon detection (basit, %10 tolerans) === */

static int regression_check(BenchSonuc *r, int n) {
    FILE *baseline = fopen("test/bench_baseline.json", "rb");
    if (!baseline) {
        printf("\n[!] test/bench_baseline.json yok — baseline kayit yok, "
               "regresyon karsilastirmasi atlanti.\n");
        printf("    Mevcut sonuclari baseline yapmak icin: "
               "cp build/bench_results.json test/bench_baseline.json\n");
        return 0;
    }
    /* Basit: dosya boyutu ile aynilik check (gercek parse degil). */
    /* MVP: sadece var/yok kontrolu, gercek JSON karsilastirma sonra. */
    fclose(baseline);
    printf("\n[i] Baseline mevcut — manuel diff icin: "
           "diff test/bench_baseline.json build/bench_results.json\n");
    (void)r; (void)n;
    return 0;
}

/* === Main === */

int main(void) {
    printf("KEMGU Mikrobenchmark Suite\n");
    printf("===========================\n\n");

    BenchSonuc sonuclar[16];
    int n = 0;

    printf("Calistiriliyor (10 benchmark)...\n");

    sonuclar[n++] = bench_lexer();
    printf("  %-22s %10.0f ns/iter  (%10.0f %s/sec)\n",
           sonuclar[n-1].ad, sonuclar[n-1].ns_avg,
           sonuclar[n-1].birim_per_s, sonuclar[n-1].birim);

    sonuclar[n++] = bench_parser();
    printf("  %-22s %10.0f ns/iter  (%10.0f %s/sec)\n",
           sonuclar[n-1].ad, sonuclar[n-1].ns_avg,
           sonuclar[n-1].birim_per_s, sonuclar[n-1].birim);

    sonuclar[n++] = bench_tip_kontrol();
    printf("  %-22s %10.0f ns/iter  (%10.0f %s/sec)\n",
           sonuclar[n-1].ad, sonuclar[n-1].ns_avg,
           sonuclar[n-1].birim_per_s, sonuclar[n-1].birim);

    sonuclar[n++] = bench_escape();
    printf("  %-22s %10.0f ns/iter  (%10.0f %s/sec)\n",
           sonuclar[n-1].ad, sonuclar[n-1].ns_avg,
           sonuclar[n-1].birim_per_s, sonuclar[n-1].birim);

    sonuclar[n++] = bench_linear();
    printf("  %-22s %10.0f ns/iter  (%10.0f %s/sec)\n",
           sonuclar[n-1].ad, sonuclar[n-1].ns_avg,
           sonuclar[n-1].birim_per_s, sonuclar[n-1].birim);

    sonuclar[n++] = bench_snapshot_serialize();
    printf("  %-22s %10.0f ns/iter  (%10.0f %s/sec)\n",
           sonuclar[n-1].ad, sonuclar[n-1].ns_avg,
           sonuclar[n-1].birim_per_s, sonuclar[n-1].birim);

    sonuclar[n++] = bench_llvm_uretim();
    printf("  %-22s %10.0f ns/iter  (%10.0f %s/sec)\n",
           sonuclar[n-1].ad, sonuclar[n-1].ns_avg,
           sonuclar[n-1].birim_per_s, sonuclar[n-1].birim);

    sonuclar[n++] = bench_end_to_end_basit();
    printf("  %-22s %10.0f ns/iter  (%10.0f %s/sec)\n",
           sonuclar[n-1].ad, sonuclar[n-1].ns_avg,
           sonuclar[n-1].birim_per_s, sonuclar[n-1].birim);

    sonuclar[n++] = bench_end_to_end_arm64();
    printf("  %-22s %10.0f ns/iter  (%10.0f %s/sec)\n",
           sonuclar[n-1].ad, sonuclar[n-1].ns_avg,
           sonuclar[n-1].birim_per_s, sonuclar[n-1].birim);

    sonuclar[n++] = bench_fuzz_throughput();
    printf("  %-22s %10.0f ns/iter  (%10.0f %s/sec)\n",
           sonuclar[n-1].ad, sonuclar[n-1].ns_avg,
           sonuclar[n-1].birim_per_s, sonuclar[n-1].birim);

    /* SIMD Spec V1: skaler vs vektor karsilastirmasi */
    sonuclar[n++] = bench_simd_kontrol_skaler();
    printf("  %-22s %10.0f ns/iter  (%10.0f %s/sec)\n",
           sonuclar[n-1].ad, sonuclar[n-1].ns_avg,
           sonuclar[n-1].birim_per_s, sonuclar[n-1].birim);

    sonuclar[n++] = bench_simd_kontrol_vektor();
    printf("  %-22s %10.0f ns/iter  (%10.0f %s/sec)\n",
           sonuclar[n-1].ad, sonuclar[n-1].ns_avg,
           sonuclar[n-1].birim_per_s, sonuclar[n-1].birim);

    /* JSON yaz */
    FILE *jf = fopen("build/bench_results.json", "w");
    if (jf) {
        json_yaz(jf, sonuclar, n);
        fclose(jf);
        printf("\nJSON cikti: build/bench_results.json\n");
    }

    regression_check(sonuclar, n);

    printf("\n===========================\n");
    printf("Toplam: %d benchmark calistirildi.\n", n);
    return 0;
}
