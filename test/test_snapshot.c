/*
 * KEMGU Snapshot Test
 * ===================
 *
 * test/ornekler/ ve test/snapshot/inputs/ icindeki .kem dosyalarini
 * parse eder, AST text outputunu test/snapshot/expected/<dosya>.ast
 * dosyasi ile karsilastirir.
 *
 * Eksik beklenti dosyasi varsa olusturur (ilk run baseline). Mevcut
 * varsa byte-for-byte karsilastirir — fark varsa fail.
 *
 * Calistirma: make calistir_snapshot_test
 *
 * Yeni baseline yaratmak: ilgili .ast dosyasini sil, tekrar calistir.
 */

#include "lexer.h"
#include "parser.h"
#include "ast.h"
#include "ast_yazdir.h"
#include "arena.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <process.h>
#include <direct.h>
#else
#include <unistd.h>
#endif

static int toplam_test = 0;
static int basarili = 0;
static int basarisiz = 0;
static int update_modu = 0;

/* Forward decls */
static int snapshot_test_dosya_update(const char *kem_yol,
                                       const char *expected_yol);

static void test_sonuc(const char *ad, int durum, const char *not_) {
    toplam_test++;
    if (durum) {
        basarili++;
        printf("  [%d] %s ... \xe2\x9c\x93\n", toplam_test, ad);
    } else {
        basarisiz++;
        printf("  [%d] %s ... \xe2\x9c\x97 %s\n",
               toplam_test, ad, not_ ? not_ : "");
    }
}

static char *dosya_oku(const char *yol, long *out_uzunluk) {
    FILE *f = fopen(yol, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t okunan = fread(buf, 1, (size_t)n, f);
    buf[okunan] = '\0';
    fclose(f);
    if (out_uzunluk) *out_uzunluk = (long)okunan;
    return buf;
}

/* AST'yi memory buffer'a yazdir (FILE * ile fmemopen yerine tmpfile) */
static char *ast_string_uret(const Dugum *prog, long *out_n) {
    char tmp_yol[256];
    /* Windows + POSIX uyumlu gecici dosya */
#ifdef _WIN32
    snprintf(tmp_yol, sizeof(tmp_yol), "%s\\kemgu_snap_%d.txt",
             getenv("TEMP") ? getenv("TEMP") : ".", (int)getpid());
#else
    snprintf(tmp_yol, sizeof(tmp_yol), "/tmp/kemgu_snap_%d.txt", (int)getpid());
#endif
    FILE *t = fopen(tmp_yol, "w+");
    if (!t) return NULL;
    ast_yazdir(prog, t);
    fclose(t);
    char *str = dosya_oku(tmp_yol, out_n);
    remove(tmp_yol);
    return str;
}

static int snapshot_test_dosya(const char *kem_yol, const char *expected_yol) {
    long src_n;
    char *kaynak = dosya_oku(kem_yol, &src_n);
    if (!kaynak) {
        test_sonuc(kem_yol, 0, "(kaynak okunamadi)");
        return 0;
    }

    Arena *a = arena_olustur(0);
    Lexer l;
    lexer_baslat(&l, kaynak, kem_yol);
    Parser p;
    parser_baslat(&p, &l, a, kem_yol, kaynak);
    Dugum *prog = parser_calistir(&p);

    long ast_n;
    char *ast_str = ast_string_uret(prog, &ast_n);
    free(kaynak);
    if (!ast_str) {
        arena_serbest(a);
        test_sonuc(kem_yol, 0, "(AST stringi uretilemedi)");
        return 0;
    }

    /* Beklenen dosya var mi? */
    long expected_n;
    char *expected = dosya_oku(expected_yol, &expected_n);
    if (!expected) {
        /* Baseline yarat */
        FILE *w = fopen(expected_yol, "wb");
        if (w) {
            fwrite(ast_str, 1, (size_t)ast_n, w);
            fclose(w);
            char not_[256];
            snprintf(not_, sizeof(not_), "(yeni baseline: %s)", expected_yol);
            test_sonuc(kem_yol, 1, not_);
        } else {
            test_sonuc(kem_yol, 0, "(expected dosyasi yazilamadi)");
        }
        free(ast_str);
        arena_serbest(a);
        return 1;
    }

    int esit = (expected_n == ast_n) &&
               (memcmp(expected, ast_str, (size_t)ast_n) == 0);
    if (!esit) {
        char not_[256];
        snprintf(not_, sizeof(not_),
                 "(AST farkli: %ld vs %ld bayt)", ast_n, expected_n);
        test_sonuc(kem_yol, 0, not_);
    } else {
        test_sonuc(kem_yol, 1, NULL);
    }

    free(expected);
    free(ast_str);
    arena_serbest(a);
    return esit;
}

/* test/ornekler altinda .kem dosyalarini RECURSIVE tara — alt klasorler
 * dahil. Her .kem icin expected dizini altinda ayni hiyerarsiyi olustur. */
static void mkdir_p(const char *yol) {
    /* Basit ozyinelemeli mkdir — Windows ve POSIX uyumlu */
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s", yol);
    size_t n = strlen(tmp);
    for (size_t i = 1; i < n; i++) {
        if (tmp[i] == '/' || tmp[i] == '\\') {
            char saved = tmp[i];
            tmp[i] = '\0';
#ifdef _WIN32
            (void)mkdir(tmp);
#else
            (void)mkdir(tmp, 0755);
#endif
            tmp[i] = saved;
        }
    }
#ifdef _WIN32
    (void)mkdir(tmp);
#else
    (void)mkdir(tmp, 0755);
#endif
}

static void dizini_tara(const char *dizin, const char *expected_pre) {
    DIR *d = opendir(dizin);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        const char *ad = e->d_name;
        if (strcmp(ad, ".") == 0 || strcmp(ad, "..") == 0) continue;
        size_t adl = strlen(ad);

        char tam_yol[512];
        snprintf(tam_yol, sizeof(tam_yol), "%s/%s", dizin, ad);

        struct stat st;
        if (stat(tam_yol, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            /* Subdirectory'e ozyinele */
            char alt_exp[512];
            snprintf(alt_exp, sizeof(alt_exp), "%s/%s", expected_pre, ad);
            mkdir_p(alt_exp);
            dizini_tara(tam_yol, alt_exp);
            continue;
        }
        if (!S_ISREG(st.st_mode)) continue;
        if (adl < 5) continue;
        if (strcmp(ad + adl - 4, ".kem") != 0) continue;

        char exp_yol[512];
        snprintf(exp_yol, sizeof(exp_yol), "%s/%.*s.ast",
                 expected_pre, (int)(adl - 4), ad);
        if (update_modu) {
            snapshot_test_dosya_update(tam_yol, exp_yol);
        } else {
            snapshot_test_dosya(tam_yol, exp_yol);
        }
    }
    closedir(d);
}

/* --update: tum baseline'lari yeniden uret (fark var/yok bakmadan) */

static int snapshot_test_dosya_update(const char *kem_yol,
                                       const char *expected_yol) {
    long src_n;
    char *kaynak = dosya_oku(kem_yol, &src_n);
    if (!kaynak) {
        test_sonuc(kem_yol, 0, "(kaynak okunamadi)");
        return 0;
    }
    Arena *a = arena_olustur(0);
    Lexer l;
    lexer_baslat(&l, kaynak, kem_yol);
    Parser p;
    parser_baslat(&p, &l, a, kem_yol, kaynak);
    Dugum *prog = parser_calistir(&p);
    long ast_n;
    char *ast_str = ast_string_uret(prog, &ast_n);
    free(kaynak);
    if (!ast_str) {
        arena_serbest(a);
        test_sonuc(kem_yol, 0, "(AST yazilamadi)");
        return 0;
    }
    FILE *w = fopen(expected_yol, "wb");
    if (w) {
        fwrite(ast_str, 1, (size_t)ast_n, w);
        fclose(w);
        char not_[256];
        snprintf(not_, sizeof(not_), "(yeni baseline yazildi)");
        test_sonuc(kem_yol, 1, not_);
    } else {
        test_sonuc(kem_yol, 0, "(expected dosyasi yazilamadi)");
    }
    free(ast_str);
    arena_serbest(a);
    return 1;
}

int main(int argc, char *argv[]) {
    /* Hata mesajlarini sustur (test cikist temiz olsun) */
    if (!freopen("NUL", "w", stderr)) {
        if (!freopen("/dev/null", "w", stderr)) {
            /* fallback sessiz */
        }
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--update") == 0) {
            update_modu = 1;
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("Kullanim: %s [--update]\n", argv[0]);
            printf("  --update: tum snapshot baseline'larini yeniden uret\n");
            return 0;
        }
    }

    printf("KEMGU Snapshot Test Paketi%s\n",
           update_modu ? " (--update)" : "");
    printf("===========================\n");

    printf("\n--- test/ornekler/*.kem ---\n");
    dizini_tara("test/ornekler", "test/snapshot/expected");

    printf("\n===========================\n");
    printf("Toplam: %d | Basarili: %d | Basarisiz: %d\n",
           toplam_test, basarili, basarisiz);
    return basarisiz > 0 ? 1 : 0;
}
