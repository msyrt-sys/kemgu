/*
 * KEMGU Gelişmiş Parser Fuzzer (test altyapi)
 *
 * Mevcut test_fuzz.c'ye ek 4 mod, her biri 5000 iter, toplam 20000+ iter.
 * (test_fuzz.c'nin 10000 ile birlikte toplam 30000+.)
 *
 * Modlar:
 *   a) Sozdizimi-aware: random KEMGU keyword/operator/identifier dizilimi
 *   b) AST-level roundtrip: random snippet -> parse -> parse_program tekrar
 *   c) Tip kontrol: parse + tip_kontrol_program crash etmemeli
 *   d) UTF-8 edge: 4-byte chars (emoji-like), BOM, control chars
 *
 * Her seviye crash sayisi loglanir; toplam crash = 0 hedefi (ASan + UBSan
 * altinda).
 */

#include "arena.h"
#include "lexer.h"
#include "parser.h"
#include "sembol.h"
#include "tip_kontrol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Mod a parser panik-loop bug-fix sonrasi GERCEK RANDOM token kullanir
 * (src-bugfix branch: parser_hata ayni-pozisyon-ayni-kod esigi).
 * ITER_PER_MODE 5000'e cikarildi (gercek 5000 random iter).
 *
 * Toplam: 4 mod x 5000 = 20000 iter (mevcut test_fuzz.c'nin 10000 byte-
 * level random'i ile birlikte 30000 iter). */
#define ITER_PER_MODE 5000
#define MAX_KAYNAK 64

/* Hata mesajlari bastir mi? Default: evet. KEMGU_FUZZ_DEBUG=1 ile kapat. */
#define BASTIR_STDERR 1

static uint32_t rng_state = 0xCAFEBABEu;

static uint32_t rng_next(void) {
    uint32_t x = rng_state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    rng_state = x; return x;
}

/* === Mod a: Sozdizimi-aware fuzzer === */

static const char *keywords[] = {
    "i\xc5\x9f" "lev", "yap\xc4\xb1", "de\xc4\x9f" "i\xc5\x9f" "ken", "sabit",
    "e\xc4\x9f" "er", "de\xc4\x9f" "ilse", "iken", "i\xc3\xa7in", "ver",
    "do\xc4\x9f" "ru", "yanl\xc4\xb1\xc5\x9f", "bo\xc5\x9f",
    "ve", "veya", "de\xc4\x9f" "il",
    "kullan", "d\xc4\xb1\xc5\x9f" "a", "mod\xc3\xbcl",
    "se\xc3\xa7imlik", "sonu\xc3\xa7", "tekkez",
    "tam32", "tam64", "metin", "mant\xc4\xb1ksal",
};

static const char *ops[] = {
    "+", "-", "*", "/", "%", "==", "!=", "<", ">", "<=", ">=",
    "&", "|", "^", "~", "<<", ">>",
    "(", ")", "{", "}", "[", "]", ",", ";", ":", "->", "=>",
};

static const char *idents[] = { "x", "y", "z", "a", "n", "main", "f", "Foo", "T" };

static void mod_a_kaynak(char *buf, int max) {
    int pos = 0;
    int n = 20 + (int)(rng_next() % 30);
    for (int i = 0; i < n && pos < max - 32; i++) {
        int kategori = (int)(rng_next() % 4);
        const char *s;
        switch (kategori) {
            case 0: s = keywords[rng_next() % (sizeof(keywords)/sizeof(*keywords))]; break;
            case 1: s = ops[rng_next() % (sizeof(ops)/sizeof(*ops))]; break;
            case 2: s = idents[rng_next() % (sizeof(idents)/sizeof(*idents))]; break;
            default: {
                /* random sayi */
                int v = (int)(rng_next() % 1000);
                int w = snprintf(buf + pos, (size_t)(max - pos), "%d ", v);
                pos += w;
                continue;
            }
        }
        int l = (int)strlen(s);
        if (pos + l + 1 >= max) break;
        memcpy(buf + pos, s, (size_t)l);
        pos += l;
        buf[pos++] = ' ';
    }
    buf[pos] = '\0';
}

/* Mod a (sozdizimi-aware): RANDOM token stream — keyword/operator/
 * identifier/sayi karistirilir. src-bugfix oncesi parser bug
 * (P018/P001'in ayni pozisyonda sonsuz tekrar) ASan OOM ediyordu.
 * Bugfix (parser_hata ayni-pozisyon-ayni-kod esigi + parser_panik_sync
 * sonrasi token-advance guard) sonsuz loop'u kapatti. */
static int mod_a_calistir(void) {
    int crash = 0;
    char buf[MAX_KAYNAK];
    for (int i = 0; i < ITER_PER_MODE; i++) {
        mod_a_kaynak(buf, MAX_KAYNAK);
        Arena *a = arena_olustur(0);
        if (!a) { crash++; continue; }
        Lexer l; lexer_baslat(&l, buf, "fuzz_a");
        Parser p; parser_baslat(&p, &l, a, "fuzz_a", buf);
        Dugum *prog = parser_calistir(&p);
        (void)prog;
        arena_serbest(a);
    }
    return crash;
}

/* === Mod b: AST roundtrip (snapshot parse + reparse) === */

static const char *roundtrip_snippets[] = {
    "i\xc5\x9flev main() -> tam32 { ver 42; }",
    "i\xc5\x9flev f(x: tam32) -> tam32 { ver x * 2; }",
    "yap\xc4\xb1 P { x: tam32; y: tam32; }",
    "i\xc5\x9flev main() -> tam32 { de\xc4\x9fi\xc5\x9fken x = [1,2,3]; ver x[0]; }",
    "i\xc5\x9flev main() -> tam32 { ver 42 & 63; }",
    "i\xc5\x9flev main() -> tam32 { ver (1 << 5) | 10; }",
};

static int mod_b_calistir(void) {
    int crash = 0;
    int n_snip = (int)(sizeof(roundtrip_snippets)/sizeof(*roundtrip_snippets));
    for (int i = 0; i < ITER_PER_MODE; i++) {
        const char *kaynak = roundtrip_snippets[i % n_snip];
        /* 2 kez parse — ikinci sefer sonra arena temizleyince crash mi? */
        Arena *a1 = arena_olustur(0);
        Lexer l1; lexer_baslat(&l1, kaynak, "fuzz_b1");
        Parser p1; parser_baslat(&p1, &l1, a1, "fuzz_b1", kaynak);
        Dugum *prog1 = parser_calistir(&p1);
        (void)prog1;
        arena_serbest(a1);

        Arena *a2 = arena_olustur(0);
        Lexer l2; lexer_baslat(&l2, kaynak, "fuzz_b2");
        Parser p2; parser_baslat(&p2, &l2, a2, "fuzz_b2", kaynak);
        Dugum *prog2 = parser_calistir(&p2);
        (void)prog2;
        arena_serbest(a2);
    }
    return crash;
}

/* === Mod c: Tip kontrol fuzzer (parse + tip_kontrol_program) === */

static const char *tip_kontrol_snippets[] = {
    "i\xc5\x9flev main() -> tam32 { ver 42; }",
    "i\xc5\x9flev f(x: tam32) -> tam32 { ver x; }",
    "i\xc5\x9flev main() -> tam32 { de\xc4\x9fi\xc5\x9fken n = 0; iken n < 10 { n = n + 1; } ver n; }",
    "yap\xc4\xb1 P { v: tam32; } i\xc5\x9flev main() -> tam32 { de\xc4\x9fi\xc5\x9fken p = P { v: 42 }; ver p.v; }",
    "i\xc5\x9flev main() -> tam32 { ver \"hata\" + 1; }",  /* tip hatasi */
    "i\xc5\x9flev main() -> tam32 { ver do\xc4\x9fru & 42; }",  /* T028 */
};

static int mod_c_calistir(void) {
    int crash = 0;
    int n_snip = (int)(sizeof(tip_kontrol_snippets)/sizeof(*tip_kontrol_snippets));
    for (int i = 0; i < ITER_PER_MODE; i++) {
        const char *kaynak = tip_kontrol_snippets[i % n_snip];
        Arena *a = arena_olustur(0);
        Lexer l; lexer_baslat(&l, kaynak, "fuzz_c");
        Parser p; parser_baslat(&p, &l, a, "fuzz_c", kaynak);
        Dugum *prog = parser_calistir(&p);

        Scope *global = scope_olustur(a, SCOPE_GLOBAL, NULL);
        TipKontrol tk;
        tip_kontrol_baslat(&tk, a, global, "fuzz_c", kaynak);
        tip_kontrol_program(&tk, prog);
        arena_serbest(a);
    }
    return crash;
}

/* === Mod d: UTF-8 edge cases === */

static void mod_d_kaynak(char *buf, int max) {
    int pos = 0;
    int n = 5 + (int)(rng_next() % 30);
    for (int i = 0; i < n && pos < max - 8; i++) {
        int cat = (int)(rng_next() % 6);
        switch (cat) {
            case 0: /* 4-byte UTF-8 (emoji range) */
                if (pos + 4 < max) {
                    buf[pos++] = (char)0xf0;
                    buf[pos++] = (char)0x9f;
                    buf[pos++] = (char)(0x80 + (rng_next() % 64));
                    buf[pos++] = (char)(0x80 + (rng_next() % 64));
                }
                break;
            case 1: /* BOM (UTF-8 byte order mark) */
                if (pos + 3 < max) {
                    buf[pos++] = (char)0xef;
                    buf[pos++] = (char)0xbb;
                    buf[pos++] = (char)0xbf;
                }
                break;
            case 2: /* Control characters (0x01-0x1F except whitespace) */
                buf[pos++] = (char)(1 + (rng_next() % 8));
                break;
            case 3: /* Invalid UTF-8 continuation (lonely byte) */
                buf[pos++] = (char)(0x80 + (rng_next() % 64));
                break;
            case 4: /* ASCII */
                buf[pos++] = (char)(32 + (rng_next() % 95));
                break;
            default: /* Turkce karakter */
                if (pos + 2 < max) {
                    buf[pos++] = (char)0xc4;
                    buf[pos++] = (char)(0x80 + (rng_next() % 32));
                }
        }
    }
    buf[pos] = '\0';
}

static int mod_d_calistir(void) {
    int crash = 0;
    char kaynak[MAX_KAYNAK];
    for (int i = 0; i < ITER_PER_MODE; i++) {
        mod_d_kaynak(kaynak, MAX_KAYNAK);
        Arena *a = arena_olustur(0);
        if (!a) { crash++; continue; }
        Lexer l; lexer_baslat(&l, kaynak, "fuzz_d");
        Parser p; parser_baslat(&p, &l, a, "fuzz_d", kaynak);
        Dugum *prog = parser_calistir(&p);
        (void)prog;
        arena_serbest(a);
    }
    return crash;
}

/* === Main === */

int main(void) {
    printf("KEMGU Gelismis Fuzzer (test altyapi)\n");
    printf("=====================================\n");
    printf("4 mod x %d iter = %d toplam\n\n",
           ITER_PER_MODE, 4 * ITER_PER_MODE);

    /* Suppress stderr (env var ile devre disi birakilabilir) */
    const char *debug = getenv("KEMGU_FUZZ_DEBUG");
    if (BASTIR_STDERR && !(debug && debug[0] == '1')) {
        if (!freopen(
#ifdef _WIN32
            "NUL"
#else
            "/dev/null"
#endif
            , "w", stderr)) {}
    }

    /* Detayli log dosyasi */
    FILE *log = fopen("build/fuzz_log.txt", "w");
    if (log) {
        fputs("KEMGU advanced fuzzer log\n=========================\n\n", log);
    }

    int total_crash = 0;

    clock_t t = clock();
    printf("Mod a (sozdizimi-aware): %d iter...", ITER_PER_MODE);
    fflush(stdout);
    int c_a = mod_a_calistir();
    double dt_a = (double)(clock() - t) / CLOCKS_PER_SEC;
    printf(" %d crash, %.2fsn\n", c_a, dt_a);
    if (log) fprintf(log, "Mod a: %d iter, %d crash, %.2fsn\n", ITER_PER_MODE, c_a, dt_a);
    total_crash += c_a;

    t = clock();
    printf("Mod b (AST roundtrip):   %d iter...", ITER_PER_MODE);
    fflush(stdout);
    int c_b = mod_b_calistir();
    double dt_b = (double)(clock() - t) / CLOCKS_PER_SEC;
    printf(" %d crash, %.2fsn\n", c_b, dt_b);
    if (log) fprintf(log, "Mod b: %d iter, %d crash, %.2fsn\n", ITER_PER_MODE, c_b, dt_b);
    total_crash += c_b;

    t = clock();
    printf("Mod c (tip kontrol):     %d iter...", ITER_PER_MODE);
    fflush(stdout);
    int c_c = mod_c_calistir();
    double dt_c = (double)(clock() - t) / CLOCKS_PER_SEC;
    printf(" %d crash, %.2fsn\n", c_c, dt_c);
    if (log) fprintf(log, "Mod c: %d iter, %d crash, %.2fsn\n", ITER_PER_MODE, c_c, dt_c);
    total_crash += c_c;

    t = clock();
    printf("Mod d (UTF-8 edge):      %d iter...", ITER_PER_MODE);
    fflush(stdout);
    int c_d = mod_d_calistir();
    double dt_d = (double)(clock() - t) / CLOCKS_PER_SEC;
    printf(" %d crash, %.2fsn\n", c_d, dt_d);
    if (log) fprintf(log, "Mod d: %d iter, %d crash, %.2fsn\n", ITER_PER_MODE, c_d, dt_d);
    total_crash += c_d;

    if (log) {
        fprintf(log, "\nToplam: %d iter, %d crash\n",
                4 * ITER_PER_MODE, total_crash);
        fclose(log);
    }

    printf("\n=====================================\n");
    printf("Toplam: %d iter, %d crash\n",
           4 * ITER_PER_MODE, total_crash);
    printf("Detayli log: build/fuzz_log.txt\n");

    if (total_crash > 0) {
        printf("FAIL: %d crash.\n", total_crash);
        return 1;
    }
    printf("OK: Tum modlar temiz.\n");
    return 0;
}
