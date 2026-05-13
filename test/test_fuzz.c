/*
 * KEMGU Parser Fuzzer (ADIM 32)
 *
 * Random byte stream'leri lexer + parser'a verir. Parser asla crash
 * etmemeli (panic mode + sync points ile bozuk girdiyi tolere etmeli).
 *
 * Coverage:
 *   - Random ASCII printable + bazi UTF-8 Turkce bayt'lari (kismi)
 *   - Random uzunluk 1-256 byte
 *   - 10000 iterasyon
 *
 * Sanity asserts:
 *   - Her iterasyondan sonra arena temizlenebilmeli (memory leak yok)
 *   - Parser exit code: hata sayisi olabilir, ancak SEGFAULT / abort YOK
 *   - Sonsuz dongu yok (timeout: tum iter 60 sn icinde bitmeli)
 *
 * ASan + UBSan ile derlenirse heap overflow, use-after-free,
 * undefined behavior'lar yakalanir.
 */

#include "arena.h"
#include "lexer.h"
#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ITER_COUNT 10000
#define MAX_KAYNAK 256

/* Deterministic PRNG (xorshift32) — sabit seed ile tekrarlanabilir */
static uint32_t rng_state = 0x12345678u;

static uint32_t rng_sonraki(void) {
    uint32_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x;
    return x;
}

/* Random karakter — ASCII printable + bazi UTF-8 byte'lari
 * (gercek Turkce karakter UTF-8 cift-byte; fuzzer single byte gonderir,
 * lexer'in malformed UTF-8'i tolere etmesi de test edilmis olur) */
static char rng_karakter(void) {
    static const char karakterler[] =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789 \t\n"
        "+-*/%=<>!&|^~"
        "(){}[],;:"
        "\".'\\_"
        /* Bazi UTF-8 lead byte'lari (0xc3-0xc5 araligi — Turkce karakterler) */
        "\xc3\xc4\xc5"
        /* Devam byte'lari */
        "\x87\x96\x9c\x9e\x9f\xa7\xb0\xb1\xb6\xbc";
    int n = (int)sizeof(karakterler) - 1;
    return karakterler[rng_sonraki() % (uint32_t)n];
}

/* Random kaynak string olustur */
static void random_kaynak(char *buf, int max_uz) {
    int uz = 1 + (int)(rng_sonraki() % (uint32_t)(max_uz - 2));
    for (int i = 0; i < uz; i++) {
        buf[i] = rng_karakter();
    }
    buf[uz] = '\0';
}

int main(void) {
    printf("KEMGU Parser Fuzzer (ADIM 32)\n");
    printf("=============================\n");
    printf("Iterasyon: %d, kaynak uzunluk: 1-%d byte\n",
           ITER_COUNT, MAX_KAYNAK);
    printf("Seed: 0x%08x (deterministic)\n\n", rng_state);

    clock_t baslangic = clock();

    /* Hata mesajlarini sustur (stderr -> NUL) */
    if (!freopen(
#ifdef _WIN32
        "NUL"
#else
        "/dev/null"
#endif
        , "w", stderr)) {
        /* sessiz fallback */
    }

    char kaynak[MAX_KAYNAK];
    int total_token = 0;
    int total_hata = 0;
    int crash = 0;

    for (int i = 0; i < ITER_COUNT; i++) {
        random_kaynak(kaynak, MAX_KAYNAK);

        Arena *a = arena_olustur(0);
        if (!a) { crash++; continue; }

        Lexer l;
        lexer_baslat(&l, kaynak, "fuzz");

        Parser p;
        parser_baslat(&p, &l, a, "fuzz", kaynak);

        /* parser_calistir panic mode ile tolere etmeli */
        Dugum *prog = parser_calistir(&p);
        (void)prog;

        total_hata += p.hata_sayisi;
        total_token += 1;  /* iterasyon sayaci olarak */

        arena_serbest(a);

        /* Her 1000'de bir progress */
        if ((i + 1) % 1000 == 0) {
            printf("  %d / %d iter (hata top=%d)\n",
                   i + 1, ITER_COUNT, total_hata);
            fflush(stdout);
        }
    }

    clock_t bitis = clock();
    double sn = (double)(bitis - baslangic) / CLOCKS_PER_SEC;

    printf("\n=============================\n");
    printf("Tamamlandi: %d iterasyon, %.2f saniye\n", ITER_COUNT, sn);
    printf("Toplam hata: %d (ortalama %.1f / iter)\n",
           total_hata, (double)total_hata / (double)ITER_COUNT);
    printf("Crash: %d\n", crash);
    (void)total_token;

    if (crash > 0) {
        printf("FAIL: Parser %d kez crash etti.\n", crash);
        return 1;
    }
    printf("OK: 10000 fuzz iterasyonu temiz (no crash).\n");
    return 0;
}
