#include "lexer.h"
#include "parser.h"
#include "ast.h"
#include "ast_yazdir.h"
#include "arena.h"
#include "tip.h"
#include "sembol.h"
#include "tip_kontrol.h"
#include "llvm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * KEMGU CLI:
 *   kemgu [--token | --parse | --check | --llvm] [dosya]
 *
 * Mod yoksa (default): --check
 *
 * --token: lexer akisini yazdir
 * --parse: parser calistir + AST yazdir
 * --check: parser + tip kontrol (default)
 * --llvm:  parser + LLVM IR text yazdir
 *
 * Dosya argumani yoksa stdin'den okur.
 */

typedef enum { MOD_TOKEN, MOD_PARSE, MOD_CHECK, MOD_LLVM } Mod;

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

static int mode_parse(const char *kaynak, const char *dosya_adi) {
    Arena *a = arena_olustur(0);
    if (!a) {
        fprintf(stderr, "Arena olusturulamadi\n");
        return 1;
    }

    Lexer l;
    lexer_baslat(&l, kaynak, dosya_adi);
    Parser p;
    parser_baslat(&p, &l, a, dosya_adi, kaynak);

    Dugum *prog = parser_calistir(&p);

    printf("=== AST ===\n");
    ast_yazdir(prog, stdout);
    printf("\n=== Toplam parser hata sayisi: %d ===\n", p.hata_sayisi);

    int rc = (p.hata_sayisi > 0) ? 1 : 0;
    arena_serbest(a);
    return rc;
}

static int mode_check(const char *kaynak, const char *dosya_adi) {
    Arena *a = arena_olustur(0);
    if (!a) {
        fprintf(stderr, "Arena olusturulamadi\n");
        return 1;
    }

    Lexer l;
    lexer_baslat(&l, kaynak, dosya_adi);
    Parser p;
    parser_baslat(&p, &l, a, dosya_adi, kaynak);
    Dugum *prog = parser_calistir(&p);

    int parser_hata = p.hata_sayisi;
    int tk_hata = 0;

    if (parser_hata == 0 && prog) {
        Scope *g = scope_olustur(a, SCOPE_GLOBAL, NULL);
        TipKontrol tk;
        tip_kontrol_baslat(&tk, a, g, dosya_adi, kaynak);
        tip_kontrol_program(&tk, prog);
        tk_hata = tk.hata_sayisi;
    }

    int toplam = parser_hata + tk_hata;
    if (toplam == 0) {
        fprintf(stdout, "OK: %s — tip kontrolu basarili.\n", dosya_adi);
    } else {
        fprintf(stdout,
                "HATA: %s — parser %d, tip kontrol %d (toplam %d hata).\n",
                dosya_adi, parser_hata, tk_hata, toplam);
    }

    arena_serbest(a);
    return toplam > 0 ? 1 : 0;
}

/* KEMGU Standart Kutuphane Prelude (D.5):
 *
 * --llvm modunda kullanici kaynagina otomatik prepend edilir. Standart
 * hata yapisi ve yardimci islev tanimlarini saglar.
 *
 * Turkce UTF-8 hex escape: \xb1 sonra hex-rakam (i) varsa concat ile ayir
 * (CLAUDE.md "Turkce UTF-8 Dikkat Noktasi" kurali). */
static const char *PRELUDE_KEMGU =
    "// === KEMGU Prelude (otomatik) ===\n"
    "yap\xc4\xb1 StandartHata {\n"
    "    kod: tam32;\n"
    "    mesaj: metin;\n"
    "}\n"
    "\n"
    "i\xc5\x9flev hata_olustur(kod: tam32, mesaj: metin) -> StandartHata {\n"
    "    ver StandartHata { kod: kod, mesaj: mesaj };\n"
    "}\n"
    "\n"
    "i\xc5\x9flev hata_yazd\xc4\xb1r_detayli(h: StandartHata) {\n"
    "    yaz(\"hata \"); yaz_tam(h.kod); yaz(\": \"); hata_yazd\xc4\xb1r(h.mesaj);\n"
    "}\n"
    "// === Prelude sonu ===\n";

static char *prelude_birlestir(const char *kaynak) {
    size_t pre_len = strlen(PRELUDE_KEMGU);
    size_t src_len = strlen(kaynak);
    char *cikti = (char *)malloc(pre_len + src_len + 1);
    if (!cikti) return NULL;
    memcpy(cikti, PRELUDE_KEMGU, pre_len);
    memcpy(cikti + pre_len, kaynak, src_len + 1);
    return cikti;
}

static int mode_llvm(const char *kaynak, const char *dosya_adi) {
    Arena *a = arena_olustur(0);
    if (!a) {
        fprintf(stderr, "Arena olusturulamadi\n");
        return 1;
    }

    /* Prelude'u kullanici kaynagina prepend et (StandartHata + yardimcilar) */
    char *birlesik = prelude_birlestir(kaynak);
    if (!birlesik) {
        arena_serbest(a);
        return 1;
    }

    Lexer l;
    lexer_baslat(&l, birlesik, dosya_adi);
    Parser p;
    parser_baslat(&p, &l, a, dosya_adi, birlesik);
    Dugum *prog = parser_calistir(&p);
    if (p.hata_sayisi > 0) {
        fprintf(stderr, "Parser hatalari: %d (LLVM IR uretilmedi)\n",
                p.hata_sayisi);
        free(birlesik);
        arena_serbest(a);
        return 1;
    }
    llvm_ir_uret(prog, stdout);
    free(birlesik);
    arena_serbest(a);
    return 0;
}

static void kullanim_yazdir(const char *prog_adi) {
    fprintf(stderr,
        "Kullanim: %s [--token | --parse | --check | --llvm] [dosya]\n"
        "  --token   Lexer akisini yazdir\n"
        "  --parse   Parser calistir + AST yazdir\n"
        "  --check   Parser + tip kontrol (varsayilan)\n"
        "  --llvm    LLVM IR text yazdir (clang -x ir - ile derlenebilir)\n"
        "  dosya     Kaynak dosya yolu (yoksa stdin'den okur)\n",
        prog_adi);
}

int main(int argc, char *argv[]) {
    Mod mod = MOD_CHECK;  /* default */
    int arg_idx = 1;

    while (arg_idx < argc && argv[arg_idx][0] == '-') {
        if (strcmp(argv[arg_idx], "--token") == 0) {
            mod = MOD_TOKEN;
            arg_idx++;
        } else if (strcmp(argv[arg_idx], "--parse") == 0) {
            mod = MOD_PARSE;
            arg_idx++;
        } else if (strcmp(argv[arg_idx], "--check") == 0) {
            mod = MOD_CHECK;
            arg_idx++;
        } else if (strcmp(argv[arg_idx], "--llvm") == 0) {
            mod = MOD_LLVM;
            arg_idx++;
        } else if (strcmp(argv[arg_idx], "--help") == 0 ||
                   strcmp(argv[arg_idx], "-h") == 0) {
            kullanim_yazdir(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Bilinmeyen bayrak: %s\n", argv[arg_idx]);
            kullanim_yazdir(argv[0]);
            return 2;
        }
    }

    char *kaynak;
    const char *dosya_adi;

    if (arg_idx < argc) {
        dosya_adi = argv[arg_idx];
        kaynak = dosya_oku(dosya_adi);
    } else {
        dosya_adi = "<stdin>";
        kaynak = stdin_oku();
    }

    if (!kaynak) {
        fprintf(stderr, "Kaynak okunamadi\n");
        return 1;
    }

    int rc;
    switch (mod) {
        case MOD_TOKEN: rc = mode_token(kaynak, dosya_adi); break;
        case MOD_PARSE: rc = mode_parse(kaynak, dosya_adi); break;
        case MOD_CHECK: rc = mode_check(kaynak, dosya_adi); break;
        case MOD_LLVM:  rc = mode_llvm(kaynak, dosya_adi); break;
        default:        rc = 2; break;
    }

    free(kaynak);
    return rc;
}
