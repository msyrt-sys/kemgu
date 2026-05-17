/*
 * KEMGU Parser Fuzzer
 * ===================
 *
 * Deterministik PRNG ile rastgele byte dizileri uretip lexer + parser'a
 * verir. Beklenti: hicbir input parser/lexer'i crash ettirmemeli, sadece
 * hata raporlayabilmeli.
 *
 * Fuzz stratejileri:
 *   1. Pure random bytes (0-255, including null)
 *   2. ASCII-only random
 *   3. KEMGU keyword corpus + random fillers
 *   4. Malformed UTF-8 sequences
 *
 * ASan ile derlenir (clang64); UAF/OOB/null-deref burada yakalanir.
 */

#include "lexer.h"
#include "parser.h"
#include "ast.h"
#include "arena.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

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
    fflush(stdout);
}

/* Basit xorshift PRNG — deterministik */
static uint64_t rng_state = 0x123456789ABCDEFULL;

static uint64_t rng_next(void) {
    uint64_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    rng_state = x;
    return x;
}

static void rng_seed(uint64_t s) {
    rng_state = s ? s : 0x1ULL;
}

/* Bir input'u parse et — crash etmemeli, sadece hata. */
static int try_parse(const char *kaynak) {
    Arena *a = arena_olustur(0);
    if (!a) return -1;
    Lexer l;
    lexer_baslat(&l, kaynak, "fuzz");
    Parser p;
    parser_baslat(&p, &l, a, "fuzz", kaynak);
    Dugum *prog = parser_calistir(&p);
    (void)prog;
    int hata = p.hata_sayisi;
    arena_serbest(a);
    return hata;
}

/* === Strateji 1: Saf rastgele baytlar === */

static void fuzz_random_bytes(int n_iter, size_t boyut) {
    int crash_yok = 1;
    for (int i = 0; i < n_iter && crash_yok; i++) {
        char *buf = (char *)malloc(boyut + 1);
        if (!buf) { crash_yok = 0; break; }
        for (size_t j = 0; j < boyut; j++) {
            buf[j] = (char)(rng_next() & 0xFF);
        }
        buf[boyut] = '\0';
        int r = try_parse(buf);
        free(buf);
        if (r < 0) { crash_yok = 0; break; }
        /* try_parse normalde >= 0 donmeli */
    }
    test_sonuc("fuzz: 100x rastgele 256-bayt", crash_yok);
}

/* === Strateji 2: ASCII-only === */

static void fuzz_ascii(int n_iter, size_t boyut) {
    int ok = 1;
    for (int i = 0; i < n_iter && ok; i++) {
        char *buf = (char *)malloc(boyut + 1);
        if (!buf) { ok = 0; break; }
        for (size_t j = 0; j < boyut; j++) {
            buf[j] = (char)(32 + (rng_next() % 95));  /* printable ASCII */
        }
        buf[boyut] = '\0';
        int r = try_parse(buf);
        free(buf);
        if (r < 0) { ok = 0; break; }
    }
    test_sonuc("fuzz: 100x ASCII 256-bayt", ok);
}

/* === Strateji 3: KEMGU keyword'leri + rastgele dolgu === */

static const char *kemgu_token_corpus[] = {
    "i\xc5\x9flev", "yap\xc4\xb1", "de\xc4\x9f" "i\xc5\x9fken", "sabit",
    "e\xc4\x9f" "er", "iken", "i\xc3\xa7in", "e\xc5\x9fle\xc5\x9f", "ver",
    "do\xc4\x9fru", "yanl\xc4\xb1\xc5\x9f", "bo\xc5\x9f", "boyut",
    "g\xc3\xbcvensiz", "kullan", "d\xc4\xb1\xc5\x9f" "a",
    "tam32", "dtam32", "tam64", "metin", "kesirli32", "mant\xc4\xb1ksal",
    "Dizi", "se\xc3\xa7imlik", "sonu\xc3\xa7",
    "ve", "veya", "de\xc4\x9f" "il",
    "{", "}", "(", ")", "[", "]", ":", ";", ",", ".",
    "->", "=>", "::", "&", "*", "|",
    "+", "-", "/", "%", "==", "!=", "<", ">", "<=", ">=",
    "<<", ">>", "^", "~",
    "0", "1", "42", "0xFF", "0b101",
    "\"merhaba\"", "'a'",
    "abc", "x", "y", "z", "_",
    "\n", " ", "\t", "  ",
};

static void fuzz_keyword_corpus(int n_iter) {
    int ok = 1;
    int n_token = (int)(sizeof(kemgu_token_corpus) /
                        sizeof(kemgu_token_corpus[0]));
    for (int i = 0; i < n_iter && ok; i++) {
        /* 5-30 token sec, rastgele birlestir */
        int n = 5 + (int)(rng_next() % 26);
        char *buf = (char *)malloc(2048);
        if (!buf) { ok = 0; break; }
        size_t pos = 0;
        for (int j = 0; j < n && pos < 2000; j++) {
            const char *t = kemgu_token_corpus[rng_next() % n_token];
            size_t tl = strlen(t);
            if (pos + tl + 1 >= 2000) break;
            memcpy(buf + pos, t, tl);
            pos += tl;
            buf[pos++] = ' ';
        }
        buf[pos] = '\0';
        int r = try_parse(buf);
        free(buf);
        if (r < 0) { ok = 0; break; }
    }
    test_sonuc("fuzz: 100x KEMGU token corpus", ok);
}

/* === Strateji 4: Malformed UTF-8 === */

static void fuzz_bad_utf8(int n_iter) {
    int ok = 1;
    for (int i = 0; i < n_iter && ok; i++) {
        char buf[512];
        size_t pos = 0;
        while (pos < 500) {
            uint64_t r = rng_next();
            /* Yari yari: gecerli ASCII ve malformed UTF-8 */
            if (r & 1) {
                buf[pos++] = (char)(32 + (r % 95));
            } else {
                /* Malformed continuation: 0x80-0xBF */
                buf[pos++] = (char)(0x80 | (r & 0x3F));
            }
        }
        buf[pos] = '\0';
        int r = try_parse(buf);
        if (r < 0) { ok = 0; break; }
    }
    test_sonuc("fuzz: 100x malformed UTF-8", ok);
}

/* === Strateji 5: Cok derin yuvalanma (stack overflow korumasi) === */

static void fuzz_derin_yuvalanma(void) {
    int ok = 1;
    char buf[4096];
    /* 1000 acik parantez */
    int n = 1000;
    int pos = 0;
    for (int i = 0; i < n && pos < 4090; i++) {
        buf[pos++] = '(';
    }
    buf[pos++] = '1';
    for (int i = 0; i < n && pos < 4090; i++) {
        buf[pos++] = ')';
    }
    buf[pos] = '\0';
    int r = try_parse(buf);
    if (r < 0) ok = 0;
    test_sonuc("fuzz: 1000-derin parantez", ok);
}

/* === Strateji 6: Cok uzun input (memory) === */

static void fuzz_buyuk_input(void) {
    int ok = 1;
    size_t boyut = 64 * 1024;  /* 64 KB */
    char *buf = (char *)malloc(boyut + 1);
    if (!buf) { ok = 0; }
    else {
        for (size_t i = 0; i < boyut; i++) {
            buf[i] = (char)(32 + (rng_next() % 95));
        }
        buf[boyut] = '\0';
        int r = try_parse(buf);
        free(buf);
        if (r < 0) ok = 0;
    }
    test_sonuc("fuzz: 64KB rastgele input", ok);
}

/* === Mutation-based fuzz: var olan .kem dosyalarini bozarak fuzz === */

static char *dosya_yukle(const char *yol, size_t *out_n) {
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

/* 5 mutation stratejisi:
 *   1. byte_flip:   rastgele konumda rastgele byte
 *   2. byte_sil:    rastgele konumda byte sil (kisalt)
 *   3. byte_ekle:   rastgele konumda rastgele byte ekle (uzat)
 *   4. blok_kopya:  bir blogu baska konuma kopyala
 *   5. byte_takas:  iki rastgele bytei takasla
 */
static char *mutate(const char *orig, size_t n, size_t *out_n) {
    /* Yeterli buffer ayir — uzayabilir */
    size_t kap = n + 64;
    char *out = (char *)malloc(kap + 1);
    if (!out) return NULL;
    memcpy(out, orig, n);
    size_t cur = n;

    int strateji = (int)(rng_next() % 5);
    switch (strateji) {
        case 0: { /* byte_flip */
            if (cur > 0) {
                size_t p = rng_next() % cur;
                out[p] = (char)(rng_next() & 0xFF);
            }
            break;
        }
        case 1: { /* byte_sil */
            if (cur > 1) {
                size_t p = rng_next() % cur;
                memmove(out + p, out + p + 1, cur - p - 1);
                cur--;
            }
            break;
        }
        case 2: { /* byte_ekle */
            if (cur + 1 < kap) {
                size_t p = rng_next() % (cur + 1);
                memmove(out + p + 1, out + p, cur - p);
                out[p] = (char)(rng_next() & 0xFF);
                cur++;
            }
            break;
        }
        case 3: { /* blok_kopya */
            if (cur > 8) {
                size_t src = rng_next() % cur;
                size_t len = 1 + (rng_next() % (cur - src));
                if (len > 32) len = 32;
                size_t dst = rng_next() % cur;
                if (dst + len <= cur) {
                    memmove(out + dst, out + src, len);
                }
            }
            break;
        }
        case 4: { /* byte_takas */
            if (cur > 1) {
                size_t a = rng_next() % cur;
                size_t b = rng_next() % cur;
                char t = out[a];
                out[a] = out[b];
                out[b] = t;
            }
            break;
        }
    }

    out[cur] = '\0';
    if (out_n) *out_n = cur;
    return out;
}

static void fuzz_mutation(int n_iter) {
    static const char *seedler[] = {
        "test/ornekler/fibonacci.kem",
        "test/ornekler/yapilar.kem",
        "test/ornekler/eslesme.kem",
        "test/ornekler/lambda_boyut.kem",
        "test/ornekler/faz1_kapsamli.kem",
    };
    int n_seed = (int)(sizeof(seedler) / sizeof(seedler[0]));

    /* Yukle (bir kez), sonra mutasyonlari uret */
    char *kaynaklar[5] = {0};
    size_t boyutlar[5] = {0};
    int yukleme_ok = 1;
    for (int i = 0; i < n_seed; i++) {
        kaynaklar[i] = dosya_yukle(seedler[i], &boyutlar[i]);
        if (!kaynaklar[i]) { yukleme_ok = 0; break; }
    }

    int ok = yukleme_ok;
    if (ok) {
        for (int i = 0; i < n_iter && ok; i++) {
            int s = (int)(rng_next() % (uint64_t)n_seed);
            size_t mn;
            char *m = mutate(kaynaklar[s], boyutlar[s], &mn);
            if (!m) { ok = 0; break; }
            int r = try_parse(m);
            free(m);
            if (r < 0) { ok = 0; break; }
        }
    }
    for (int i = 0; i < n_seed; i++) {
        if (kaynaklar[i]) free(kaynaklar[i]);
    }
    test_sonuc("fuzz: mutation-based (.kem seed'lerinden)",
               ok);
}

int main(int argc, char *argv[]) {
    /* Fuzzing sirasinda KEMGU hata raporlari stderr'e gider — Makefile
     * 2>/dev/null ile filtreliyor. ASan raporlari da stderr'e gider —
     * crash olursa ASan exit code'undan anlasilir. */

    uint64_t seed = 0x12345ULL;
    int n_iter = 100;
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--seed=", 7) == 0) {
            seed = strtoull(argv[i] + 7, NULL, 0);
        } else if (strncmp(argv[i], "--iter=", 7) == 0) {
            n_iter = (int)strtol(argv[i] + 7, NULL, 10);
            if (n_iter < 1) n_iter = 1;
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("Kullanim: %s [--seed=N] [--iter=N]\n", argv[0]);
            printf("  --seed=N    PRNG seed (default 0x12345)\n");
            printf("  --iter=N    Strateji basina iter sayisi (default 100)\n");
            return 0;
        }
    }

    printf("KEMGU Parser Fuzzer\n");
    printf("===================\n");
    printf("seed=0x%llx iter=%d\n",
           (unsigned long long)seed, n_iter);
    fflush(stdout);

    rng_seed(seed);

    printf("\n--- Random byte stratejileri ---\n");
    fuzz_random_bytes(n_iter, 256);
    fuzz_ascii(n_iter, 256);

    printf("\n--- Yapilandirilmis ---\n");
    fuzz_keyword_corpus(n_iter);

    printf("\n--- Edge cases ---\n");
    fuzz_bad_utf8(n_iter);
    fuzz_derin_yuvalanma();
    fuzz_buyuk_input();

    printf("\n--- Mutation-based (gercek .kem'leri mutate eder) ---\n");
    fuzz_mutation(n_iter * 5);  /* daha cok iter — coverage'i yuksek */

    printf("\n===================\n");
    printf("Toplam: %d | Basarili: %d | Basarisiz: %d\n",
           toplam_test, basarili, basarisiz);
    return basarisiz > 0 ? 1 : 0;
}
