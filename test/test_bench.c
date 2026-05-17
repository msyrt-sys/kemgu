/*
 * KEMGU Performance Benchmark
 * ===========================
 *
 * Compile-time performans olculur:
 *   - Lexer hizi  (MB/s)
 *   - Parser hizi (KLOC/s)
 *   - Tip kontrol + LLVM IR (kombine, end-to-end)
 *
 * Cikti: insan okur + machine-readable (kolayca regression dedektoru
 * yazilabilir).
 *
 * NOT: Bu ASan'siz derleme (gercek perf). ASan testi ayri.
 */

#include "lexer.h"
#include "parser.h"
#include "ast.h"
#include "arena.h"
#include "tip_kontrol.h"
#include "sembol.h"
#include "llvm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* JSON output flag (main'den once kullanmak icin global) */
static int json_modu = 0;
static int json_first = 1;

static void json_basla(void) {
    if (json_modu) { printf("["); }
}
static void json_bitir(void) {
    if (json_modu) { printf("\n]\n"); }
}
static void json_ekle(const char *test_ad, const char *etiket, size_t bayt,
                       double ms, double thr, const char *thr_birim) {
    if (!json_modu) return;
    if (!json_first) printf(",");
    json_first = 0;
    printf("\n  {\"test\":\"%s\",\"input\":\"%s\",\"bayt\":%zu,"
           "\"ms\":%.3f,\"throughput\":%.2f,\"birim\":\"%s\"}",
           test_ad, etiket, bayt, ms, thr, thr_birim);
}

/* Yuksek-cozunurluklu zaman */
static double simdi_saniye(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static char *dosya_oku(const char *yol, size_t *out_n) {
    FILE *f = fopen(yol, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f);
    buf[r] = '\0';
    fclose(f);
    if (out_n) *out_n = r;
    return buf;
}

/* Buyuk sentetik bench input — N tane islev kopyasi */
static char *sentetik_uret(int n_islev, size_t *out_n) {
    size_t kapasite = (size_t)n_islev * 256 + 64;
    char *buf = (char *)malloc(kapasite);
    if (!buf) return NULL;
    size_t pos = 0;
    for (int i = 0; i < n_islev; i++) {
        int w = snprintf(buf + pos, kapasite - pos,
            "i\xc5\x9flev f%d(x: tam32, y: tam32) -> tam32 {\n"
            "    de\xc4\x9f""i\xc5\x9fken z: tam32 = x * y + %d;\n"
            "    e\xc4\x9f""er z > 100 { ver z; }\n"
            "    ver z + %d;\n"
            "}\n", i, i, i * 2);
        if (w < 0 || (size_t)w >= kapasite - pos) break;
        pos += (size_t)w;
    }
    buf[pos] = '\0';
    if (out_n) *out_n = pos;
    return buf;
}

/* === Bench 1: Lexer ham hizi === */
static void bench_lexer(const char *kaynak, size_t n_bayt,
                         const char *etiket) {
    int n_iter = 10;
    double t0 = simdi_saniye();
    int n_token = 0;
    for (int i = 0; i < n_iter; i++) {
        Lexer l;
        lexer_baslat(&l, kaynak, "bench");
        Token t;
        do {
            t = lexer_sonraki_token(&l);
            n_token++;
        } while (t.tip != TOK_DOSYA_SONU);
    }
    double t1 = simdi_saniye();
    double dt = (t1 - t0) / n_iter;
    double mb_per_sec = (double)n_bayt / (1024.0 * 1024.0) / dt;
    if (!json_modu) {
        printf("  [LEXER] %-30s %7zu bayt  %6.2f ms  %7.2f MB/s  %d token\n",
               etiket, n_bayt, dt * 1000.0, mb_per_sec, n_token / n_iter);
    }
    json_ekle("lexer", etiket, n_bayt, dt * 1000.0, mb_per_sec, "MB/s");
}

/* === Bench 2: Parser hizi === */
static void bench_parser(const char *kaynak, size_t n_bayt,
                          const char *etiket) {
    int n_iter = 5;
    double t0 = simdi_saniye();
    int n_hata = 0;
    int n_uye = 0;
    for (int i = 0; i < n_iter; i++) {
        Arena *a = arena_olustur(0);
        Lexer l;
        lexer_baslat(&l, kaynak, "bench");
        Parser p;
        parser_baslat(&p, &l, a, "bench", kaynak);
        Dugum *prog = parser_calistir(&p);
        n_hata = p.hata_sayisi;
        n_uye = prog ? prog->veri.program.sayi : 0;
        arena_serbest(a);
    }
    double t1 = simdi_saniye();
    double dt = (t1 - t0) / n_iter;
    /* "Satir" sayma kabasaba */
    size_t n_satir = 0;
    for (size_t i = 0; i < n_bayt; i++) if (kaynak[i] == '\n') n_satir++;
    double kloc_per_sec = (double)n_satir / 1000.0 / dt;
    if (!json_modu) {
        printf("  [PARSER] %-29s %7zu bayt  %6.2f ms  %7.2f KLOC/s  uye=%d hata=%d\n",
               etiket, n_bayt, dt * 1000.0, kloc_per_sec, n_uye, n_hata);
    }
    json_ekle("parser", etiket, n_bayt, dt * 1000.0, kloc_per_sec, "KLOC/s");
}

/* === Bench 3: End-to-end (lex + parse + tip + LLVM IR'a yaz) === */
static void bench_e2e(const char *kaynak, const char *etiket) {
    int n_iter = 3;
    double t0 = simdi_saniye();
    for (int i = 0; i < n_iter; i++) {
        Arena *a = arena_olustur(0);
        Lexer l;
        lexer_baslat(&l, kaynak, "bench");
        Parser p;
        parser_baslat(&p, &l, a, "bench", kaynak);
        Dugum *prog = parser_calistir(&p);
        if (p.hata_sayisi == 0 && prog) {
            Scope *g = scope_olustur(a, SCOPE_GLOBAL, NULL);
            TipKontrol tk;
            tip_kontrol_baslat(&tk, a, g, "bench", kaynak);
            tip_kontrol_program(&tk, prog);
            /* LLVM IR'i NULL'a yaz (gerçek emit ama dosyaya yazmadan) */
            FILE *nullf = fopen("NUL", "w");
            if (!nullf) nullf = fopen("/dev/null", "w");
            if (nullf) {
                llvm_ir_uret(prog, nullf);
                fclose(nullf);
            }
        }
        arena_serbest(a);
    }
    double t1 = simdi_saniye();
    double dt = (t1 - t0) / n_iter;
    if (!json_modu) {
        printf("  [E2E   ] %-29s              %6.2f ms\n",
               etiket, dt * 1000.0);
    }
    json_ekle("e2e", etiket, 0, dt * 1000.0, 0, "ms");
}

int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--json") == 0) json_modu = 1;
    }

    if (!json_modu) {
        printf("KEMGU Performans Bench\n");
        printf("======================\n");
    }
    json_basla();

    /* Bench 1: kucuk gerçek kod */
    size_t fib_n;
    char *fib = dosya_oku("test/ornekler/fibonacci.kem", &fib_n);
    if (fib) {
        if (!json_modu) printf("\n---fibonacci.kem (kucuk) ---\n");
        bench_lexer(fib, fib_n, "fibonacci.kem");
        bench_parser(fib, fib_n, "fibonacci.kem");
        bench_e2e(fib, "fibonacci.kem");
        free(fib);
    }

    /* Bench 2: orta boyut */
    size_t faz_n;
    char *faz = dosya_oku("test/ornekler/faz1_kapsamli.kem", &faz_n);
    if (faz) {
        if (!json_modu) printf("\n---faz1_kapsamli.kem (orta) ---\n");
        bench_lexer(faz, faz_n, "faz1_kapsamli.kem");
        bench_parser(faz, faz_n, "faz1_kapsamli.kem");
        bench_e2e(faz, "faz1_kapsamli.kem");
        free(faz);
    }

    /* Bench 3: sentetik buyuk (100 islev) */
    size_t sent_n;
    char *sent = sentetik_uret(100, &sent_n);
    if (sent) {
        if (!json_modu) printf("\n---100x islev (sentetik) ---\n");
        bench_lexer(sent, sent_n, "100x islev");
        bench_parser(sent, sent_n, "100x islev");
        bench_e2e(sent, "100x islev");
        free(sent);
    }

    /* Bench 4: cok buyuk sentetik (1000 islev) */
    char *sent2 = sentetik_uret(1000, &sent_n);
    if (sent2) {
        if (!json_modu) printf("\n---1000x islev (buyuk sentetik) ---\n");
        bench_lexer(sent2, sent_n, "1000x islev");
        bench_parser(sent2, sent_n, "1000x islev");
        bench_e2e(sent2, "1000x islev");
        free(sent2);
    }

    json_bitir();
    if (!json_modu) printf("\n======================\n");
    return 0;
}
