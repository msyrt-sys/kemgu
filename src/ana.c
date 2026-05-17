#include "lexer.h"
#include "parser.h"
#include "ast.h"
#include "ast_yazdir.h"
#include "ast_kaynak.h"
#include "arena.h"
#include "tip.h"
#include "sembol.h"
#include "tip_kontrol.h"
#include "tekkez_kontrol.h"
#include "llvm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * KEMGU CLI:
 *   kemgu [mod] [bayraklar] dosya1.kem dosya2.kem ...
 *
 * Modlar:
 *   --token       Lexer akisini yazdir (sadece tek dosya)
 *   --parse       Parser calistir + AST yazdir
 *   --check       Parser + tip kontrol (varsayilan)
 *   --llvm        LLVM IR text yazdir (stdout)
 *   -c            Object file uret (clang ile)
 *   --build       Tam derleme: IR + clang ile baglama (exe uretir)
 *
 * Bayraklar:
 *   -o cikti      Cikti dosyasi (-c veya --build modlari icin)
 *
 * Coklu dosya: birden cok .kem dosyasi tek programa birlestirilir
 * (ortak global scope, ortak codegen).
 */

typedef enum { MOD_TOKEN, MOD_PARSE, MOD_CHECK, MOD_LLVM,
               MOD_OBJ, MOD_BUILD, MOD_FORMAT } Mod;

typedef struct {
    int freestanding;          /* --freestanding: -nostdlib -ffreestanding */
    const char *target;        /* --target=TRIPLE */
    const char *linker_script; /* --linker-script=path */
    int deneysel_linear;       /* --experimental-linear (Direktif Ek v1 B) */
} ClangSecenek;

static char *dosya_oku(const char *dosya_adi) {
    FILE *f = fopen(dosya_adi, "rb");
    if (!f) {
        fprintf(stderr, "Dosya acilamadi: %s\n", dosya_adi);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long boyut = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (boyut < 0) { fclose(f); return NULL; }
    char *tampon = (char *)malloc((size_t)boyut + 1);
    if (!tampon) { fclose(f); return NULL; }
    size_t okunan = fread(tampon, 1, (size_t)boyut, f);
    tampon[okunan] = '\0';
    fclose(f);
    return tampon;
}

static char *stdin_oku(void) {
    size_t kapasite = 4096;
    size_t uzunluk = 0;
    char *tampon = (char *)malloc(kapasite);
    if (!tampon) return NULL;
    int c;
    while ((c = getchar()) != EOF) {
        if (uzunluk + 1 >= kapasite) {
            kapasite *= 2;
            char *yeni = (char *)realloc(tampon, kapasite);
            if (!yeni) { free(tampon); return NULL; }
            tampon = yeni;
        }
        tampon[uzunluk++] = (char)c;
    }
    tampon[uzunluk] = '\0';
    return tampon;
}

/* Coklu programlari tek bir DUGUM_PROGRAM altinda birlestir. */
static Dugum *birlestir_programlar(Arena *a, Dugum **proglar, int n) {
    if (n == 1) return proglar[0];
    int toplam = 0;
    for (int i = 0; i < n; i++) {
        if (proglar[i] && proglar[i]->tip == DUGUM_PROGRAM) {
            toplam += proglar[i]->veri.program.sayi;
        }
    }
    Dugum **uyeler = (Dugum **)arena_ayir(a,
        sizeof(Dugum *) * (size_t)(toplam > 0 ? toplam : 1));
    int idx = 0;
    for (int i = 0; i < n; i++) {
        if (proglar[i] && proglar[i]->tip == DUGUM_PROGRAM) {
            for (int j = 0; j < proglar[i]->veri.program.sayi; j++) {
                uyeler[idx++] = proglar[i]->veri.program.uyeler[j];
            }
        }
    }
    return dugum_program(a, uyeler, toplam, 1, 1);
}

static int mode_token(const char *kaynak, const char *dosya_adi) {
    Lexer l;
    lexer_baslat(&l, kaynak, dosya_adi);
    Token t;
    do {
        t = lexer_sonraki_token(&l);
        printf("%-20s \"%.*s\"\t\t%d:%d\n",
               token_tipi_adi(t.tip), t.uzunluk, t.baslangic,
               t.satir, t.sutun);
    } while (t.tip != TOK_DOSYA_SONU);
    return 0;
}

/* Aktif global secenekler — coklu_parse Parser flag'lerine pas eder */
static int g_deneysel_linear = 0;

/* Coklu dosyayi parse et + birlestir. Hata yoksa AST don. */
static Dugum *coklu_parse(Arena *a, char **kaynaklar, char **dosya_adlari,
                           int n, int *toplam_hata) {
    Dugum **proglar = (Dugum **)malloc(sizeof(Dugum *) * (size_t)n);
    int hata = 0;
    for (int i = 0; i < n; i++) {
        Lexer l;
        lexer_baslat(&l, kaynaklar[i], dosya_adlari[i]);
        Parser p;
        parser_baslat(&p, &l, a, dosya_adlari[i], kaynaklar[i]);
        p.deneysel_linear = g_deneysel_linear;
        proglar[i] = parser_calistir(&p);
        hata += p.hata_sayisi;
    }
    if (toplam_hata) *toplam_hata = hata;
    Dugum *prog = birlestir_programlar(a, proglar, n);
    free(proglar);
    return prog;
}

static int mode_parse(char **kaynaklar, char **dosya_adlari, int n) {
    Arena *a = arena_olustur(0);
    if (!a) {
        fprintf(stderr, "Arena olusturulamadi\n");
        return 1;
    }

    int hata;
    Dugum *prog = coklu_parse(a, kaynaklar, dosya_adlari, n, &hata);

    printf("=== AST ===\n");
    ast_yazdir(prog, stdout);
    printf("\n=== Toplam parser hata sayisi: %d ===\n", hata);

    int rc = (hata > 0) ? 1 : 0;
    arena_serbest(a);
    return rc;
}

static int mode_check(char **kaynaklar, char **dosya_adlari, int n) {
    Arena *a = arena_olustur(0);
    if (!a) {
        fprintf(stderr, "Arena olusturulamadi\n");
        return 1;
    }

    int parser_hata;
    Dugum *prog = coklu_parse(a, kaynaklar, dosya_adlari, n, &parser_hata);

    int tk_hata = 0;
    if (parser_hata == 0 && prog) {
        Scope *g = scope_olustur(a, SCOPE_GLOBAL, NULL);
        TipKontrol tk;
        /* Tip kontrol icin tek dosya context (ilk dosya) */
        tip_kontrol_baslat(&tk, a, g, dosya_adlari[0], kaynaklar[0]);
        tip_kontrol_program(&tk, prog);
        tk_hata = tk.hata_sayisi;
        /* Linear (tekkez) kontrol — sadece --experimental-linear ile */
        if (g_deneysel_linear) {
            TekKezKontrol lk;
            tekkez_kontrol_baslat(&lk, a, g,
                                   dosya_adlari[0], kaynaklar[0]);
            lk.aktif_mi = 1;
            tekkez_kontrol_program(&lk, prog);
            tk_hata += lk.hata_sayisi;
        }
    }

    int toplam = parser_hata + tk_hata;
    const char *ad = (n == 1) ? dosya_adlari[0] : "(coklu dosya)";
    if (toplam == 0) {
        fprintf(stdout, "OK: %s — tip kontrolu basarili.\n", ad);
    } else {
        fprintf(stdout,
                "HATA: %s — parser %d, tip kontrol %d (toplam %d hata).\n",
                ad, parser_hata, tk_hata, toplam);
    }

    arena_serbest(a);
    return toplam > 0 ? 1 : 0;
}

static int mode_format(char **kaynaklar, char **dosya_adlari, int n) {
    Arena *a = arena_olustur(0);
    if (!a) {
        fprintf(stderr, "Arena olusturulamadi\n");
        return 1;
    }
    int hata;
    Dugum *prog = coklu_parse(a, kaynaklar, dosya_adlari, n, &hata);
    if (hata > 0) {
        fprintf(stderr, "Parser hatalari: %d (format uretilmedi)\n", hata);
        arena_serbest(a);
        return 1;
    }
    ast_kaynak_yaz(prog, stdout);
    arena_serbest(a);
    return 0;
}

static int mode_llvm(char **kaynaklar, char **dosya_adlari, int n) {
    Arena *a = arena_olustur(0);
    if (!a) {
        fprintf(stderr, "Arena olusturulamadi\n");
        return 1;
    }
    int hata;
    Dugum *prog = coklu_parse(a, kaynaklar, dosya_adlari, n, &hata);
    if (hata > 0) {
        fprintf(stderr, "Parser hatalari: %d (LLVM IR uretilmedi)\n", hata);
        arena_serbest(a);
        return 1;
    }
    llvm_ir_uret(prog, stdout);
    arena_serbest(a);
    return 0;
}

/* Object file ya da executable uret. Cikti yoksa default "a.o" veya "a.exe". */
static int mode_derle(char **kaynaklar, char **dosya_adlari, int n,
                       const char *cikti, int obj_mu,
                       const ClangSecenek *secenek) {
    Arena *a = arena_olustur(0);
    if (!a) {
        fprintf(stderr, "Arena olusturulamadi\n");
        return 1;
    }

    int hata;
    Dugum *prog = coklu_parse(a, kaynaklar, dosya_adlari, n, &hata);
    if (hata > 0) {
        fprintf(stderr, "Parser hatalari: %d\n", hata);
        arena_serbest(a);
        return 1;
    }

    /* Tip kontrol (kalip kalip — hatalar uyari) */
    Scope *g = scope_olustur(a, SCOPE_GLOBAL, NULL);
    TipKontrol tk;
    tip_kontrol_baslat(&tk, a, g, dosya_adlari[0], kaynaklar[0]);
    tip_kontrol_program(&tk, prog);
    /* Tip hata olursa bile devam et — bazi ornekler eksik (esles vs.). */

    /* Gecici IR dosyasi yaz */
    char tmp_ir[512];
#ifdef _WIN32
    snprintf(tmp_ir, sizeof(tmp_ir), "%s.ll", cikti);
#else
    snprintf(tmp_ir, sizeof(tmp_ir), "/tmp/kemgu_%d.ll", (int)getpid());
#endif

    FILE *f = fopen(tmp_ir, "w");
    if (!f) {
        fprintf(stderr, "IR dosyasi yazilamadi: %s\n", tmp_ir);
        arena_serbest(a);
        return 1;
    }
    llvm_ir_uret_target(prog, f,
        (secenek && secenek->target) ? secenek->target : NULL);
    fclose(f);

    /* clang komutunu olustur */
    char cmd[4096];
    char extra[1024] = "";
    if (secenek) {
        if (secenek->target) {
            char buf[256];
            snprintf(buf, sizeof(buf), " -target %s", secenek->target);
            strncat(extra, buf, sizeof(extra) - strlen(extra) - 1);
        }
        if (secenek->freestanding) {
            strncat(extra,
                " -ffreestanding -nostdlib -fno-builtin -fno-stack-protector",
                sizeof(extra) - strlen(extra) - 1);
        }
        if (secenek->linker_script && !obj_mu) {
            char buf[512];
            snprintf(buf, sizeof(buf), " -Wl,-T,%s",
                     secenek->linker_script);
            strncat(extra, buf, sizeof(extra) - strlen(extra) - 1);
        }
    }

    if (obj_mu) {
        snprintf(cmd, sizeof(cmd),
                 "clang%s -x ir \"%s\" -c -o \"%s\"",
                 extra, tmp_ir, cikti);
    } else {
        snprintf(cmd, sizeof(cmd),
                 "clang%s -x ir \"%s\" -o \"%s\"",
                 extra, tmp_ir, cikti);
    }

    int rc = system(cmd);
    remove(tmp_ir);
    arena_serbest(a);

    if (rc != 0) {
        fprintf(stderr, "clang basarisiz (cikis kodu %d)\n", rc);
        return 1;
    }
    fprintf(stdout, "Uretildi: %s\n", cikti);
    return 0;
}

static void kullanim_yazdir(const char *prog_adi) {
    fprintf(stderr,
        "Kullanim: %s [mod] [bayraklar] dosya1.kem [dosya2.kem ...]\n"
        "\n"
        "Modlar:\n"
        "  --token            Lexer akisini yazdir\n"
        "  --parse            Parser + AST yazdir\n"
        "  --check            Parser + tip kontrol (varsayilan)\n"
        "  --llvm             LLVM IR text stdout'a yazdir\n"
        "  -c                 Object file (.o) uret (-o gerekli)\n"
        "  --build            Yurutulebilir uret (-o gerekli)\n"
        "\n"
        "Bayraklar:\n"
        "  -o cikti           Cikti dosyasi (-c, --build icin)\n"
        "  --target=TRIPLE    Cross-compile hedef (orn. x86_64-unknown-none)\n"
        "  --freestanding     Bare-metal (-nostdlib -ffreestanding)\n"
        "  --linker-script=P  Linker script yolu (-Wl,-T,P)\n"
        "\n"
        "Ornekler:\n"
        "  %s prog.kem                       # tip kontrol\n"
        "  %s --llvm prog.kem                # IR yazdir\n"
        "  %s --build prog.kem -o prog.exe   # exe uret\n"
        "  %s --build m1.kem m2.kem -o app   # coklu dosya\n"
        "  %s --build --freestanding --target=x86_64-unknown-none \\\n"
        "         kernel.kem -o kernel.elf   # bare-metal kernel\n",
        prog_adi, prog_adi, prog_adi, prog_adi, prog_adi, prog_adi);
}

int main(int argc, char *argv[]) {
    Mod mod = MOD_CHECK;
    const char *cikti = NULL;
    ClangSecenek secenek;
    memset(&secenek, 0, sizeof(secenek));

    /* Bayraklari ayir */
    char **dosyalar = (char **)malloc(sizeof(char *) * (size_t)argc);
    int n_dosya = 0;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "--token") == 0) mod = MOD_TOKEN;
        else if (strcmp(a, "--parse") == 0) mod = MOD_PARSE;
        else if (strcmp(a, "--check") == 0) mod = MOD_CHECK;
        else if (strcmp(a, "--llvm") == 0) mod = MOD_LLVM;
        else if (strcmp(a, "--format") == 0) mod = MOD_FORMAT;
        else if (strcmp(a, "-c") == 0) mod = MOD_OBJ;
        else if (strcmp(a, "--build") == 0) mod = MOD_BUILD;
        else if (strcmp(a, "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "-o icin cikti adi gerek\n");
                free(dosyalar);
                return 2;
            }
            cikti = argv[++i];
        }
        else if (strcmp(a, "--freestanding") == 0) {
            secenek.freestanding = 1;
        }
        else if (strncmp(a, "--target=", 9) == 0) {
            secenek.target = a + 9;
        }
        else if (strncmp(a, "--linker-script=", 16) == 0) {
            secenek.linker_script = a + 16;
        }
        else if (strcmp(a, "--experimental-linear") == 0) {
            secenek.deneysel_linear = 1;
            g_deneysel_linear = 1;
        }
        else if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) {
            kullanim_yazdir(argv[0]);
            free(dosyalar);
            return 0;
        }
        else if (a[0] == '-') {
            fprintf(stderr, "Bilinmeyen bayrak: %s\n", a);
            kullanim_yazdir(argv[0]);
            free(dosyalar);
            return 2;
        }
        else {
            dosyalar[n_dosya++] = argv[i];
        }
    }

    /* Kaynaklari oku */
    char **kaynaklar;
    char **dosya_adlari;
    int n;

    if (n_dosya > 0) {
        n = n_dosya;
        kaynaklar = (char **)malloc(sizeof(char *) * (size_t)n);
        dosya_adlari = (char **)malloc(sizeof(char *) * (size_t)n);
        for (int i = 0; i < n; i++) {
            kaynaklar[i] = dosya_oku(dosyalar[i]);
            dosya_adlari[i] = dosyalar[i];
            if (!kaynaklar[i]) {
                for (int j = 0; j < i; j++) free(kaynaklar[j]);
                free(kaynaklar);
                free(dosya_adlari);
                free(dosyalar);
                return 1;
            }
        }
    } else {
        n = 1;
        kaynaklar = (char **)malloc(sizeof(char *));
        dosya_adlari = (char **)malloc(sizeof(char *));
        kaynaklar[0] = stdin_oku();
        dosya_adlari[0] = (char *)"<stdin>";
        if (!kaynaklar[0]) {
            free(kaynaklar);
            free(dosya_adlari);
            free(dosyalar);
            return 1;
        }
    }

    int rc = 0;
    if (mod == MOD_TOKEN) {
        if (n > 1) {
            fprintf(stderr, "--token tek dosya kabul eder\n");
            rc = 2;
        } else {
            rc = mode_token(kaynaklar[0], dosya_adlari[0]);
        }
    } else if (mod == MOD_PARSE) {
        rc = mode_parse(kaynaklar, dosya_adlari, n);
    } else if (mod == MOD_CHECK) {
        rc = mode_check(kaynaklar, dosya_adlari, n);
    } else if (mod == MOD_LLVM) {
        rc = mode_llvm(kaynaklar, dosya_adlari, n);
    } else if (mod == MOD_FORMAT) {
        rc = mode_format(kaynaklar, dosya_adlari, n);
    } else if (mod == MOD_OBJ) {
        if (!cikti) {
            fprintf(stderr, "-c modunda -o cikti gerek\n");
            rc = 2;
        } else {
            rc = mode_derle(kaynaklar, dosya_adlari, n, cikti, 1, &secenek);
        }
    } else if (mod == MOD_BUILD) {
        if (!cikti) {
            fprintf(stderr, "--build modunda -o cikti gerek\n");
            rc = 2;
        } else {
            rc = mode_derle(kaynaklar, dosya_adlari, n, cikti, 0, &secenek);
        }
    } else {
        rc = 2;
    }

    for (int i = 0; i < n; i++) free(kaynaklar[i]);
    free(kaynaklar);
    free(dosya_adlari);
    free(dosyalar);
    return rc;
}
