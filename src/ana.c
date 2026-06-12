#include "lexer.h"
#include "parser.h"
#include "ast.h"
#include "ast_yazdir.h"
#include "arena.h"
#include "tip.h"
#include "sembol.h"
#include "tip_kontrol.h"
#include "llvm.h"
#include "lsp.h"
#include "wcet.h"
#include "hata.h"

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

typedef enum { MOD_TOKEN, MOD_PARSE, MOD_CHECK, MOD_LLVM, MOD_LSP } Mod;

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
    int rt_hata = 0;

    if (parser_hata == 0 && prog) {
        Scope *g = scope_olustur(a, SCOPE_GLOBAL, NULL);
        TipKontrol tk;
        tip_kontrol_baslat(&tk, a, g, dosya_adi, kaynak);
        tip_kontrol_program(&tk, prog);
        tk_hata = tk.hata_sayisi;

        /* Realtime Spec V1: gerçekzamanlı işlevler icin RT001-RT005
         * denetimi + WCET hesabı (sembol tablosu hazır olunca). */
        WcetKontrol wk;
        wcet_kontrol_baslat(&wk, a, g, dosya_adi, kaynak);
        wcet_kontrol_program(&wk, prog);
        rt_hata = wk.hata_sayisi;
    }

    int toplam = parser_hata + tk_hata + rt_hata;
    if (toplam == 0) {
        fprintf(stdout, "OK: %s — tip kontrolu basarili.\n", dosya_adi);
    } else {
        fprintf(stdout,
                "HATA: %s — parser %d, tip kontrol %d, realtime %d "
                "(toplam %d hata).\n",
                dosya_adi, parser_hata, tk_hata, rt_hata, toplam);
    }

    arena_serbest(a);
    return toplam > 0 ? 1 : 0;
}

/* C2: IR-verifier kapisi varsayilan ACIK; --no-verify ile kapatilir
 * (benchmark kacis yolu). */
static int g_llvm_dogrula = 1;

/* Tek-gecis ad cozumu: resolver gecisinde hatalar susturulur —
 * --llvm tip hatali programlarda da eskisi gibi emit etmeye devam
 * eder (binding'siz dugumler codegen'de string fallback'ine duser). */
static void sessiz_hata_cb(int satir, int sutun, const char *kod,
                           const char *mesaj, const char *ipucu, void *ctx) {
    (void)satir; (void)sutun; (void)kod;
    (void)mesaj; (void)ipucu; (void)ctx;
}

static int mode_llvm(const char *kaynak, const char *dosya_adi) {
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
    if (p.hata_sayisi > 0) {
        fprintf(stderr, "Parser hatalari: %d (LLVM IR uretilmedi)\n",
                p.hata_sayisi);
        arena_serbest(a);
        return 1;
    }

    /* Tek-gecis ad cozumu: resolver (tip_kontrol) binding'leri AST'ye
     * yazar, codegen string'le yeniden cozmek yerine bunlari tuketir
     * (bkz. ast.h CozumKategorisi). Scope/Sembol'ler ayni arena'da —
     * binding pointer'lari llvm_ir_uret boyunca gecerli. */
    if (prog) {
        Scope *g = scope_olustur(a, SCOPE_GLOBAL, NULL);
        if (g) {
            TipKontrol tk;
            tip_kontrol_baslat(&tk, a, g, dosya_adi, kaynak);
            hata_callback_ayarla(sessiz_hata_cb, NULL);
            tip_kontrol_program(&tk, prog);
            hata_callback_ayarla(NULL, NULL);
        }
    }

    /* --no-verify: kapi kapali, dogrudan emit.
     * C5 AS001: olumcul codegen hatasi varsa hata koduyla bit. */
    if (!g_llvm_dogrula) {
        int n = llvm_ir_uret(prog, stdout);
        arena_serbest(a);
        return n > 0 ? 1 : 0;
    }

    /* C2 kapisi: IR'i once tampona uret, opt pass'lerinden ONCE dogrula,
     * sonra stdout'a yaz. Text backend oldugu icin LLVMVerifyModule yerine
     * llvm_ir_dogrula (terminator-tamlik tarayici) kullanilir.
     * tmpfile acilamazsa guvenli taraf: dogrulamayi atla, yine de emit et. */
    FILE *tf = tmpfile();
    if (!tf) {
        int n = llvm_ir_uret(prog, stdout);
        arena_serbest(a);
        return n > 0 ? 1 : 0;
    }
    int codegen_hata = llvm_ir_uret(prog, tf);
    if (codegen_hata > 0) {
        /* C5 AS001: hata mesaji stderr'e zaten yazildi; IR yayinlanmaz. */
        fclose(tf);
        arena_serbest(a);
        return 1;
    }

    long boyut = ftell(tf);
    if (boyut < 0) {
        rewind(tf);
        char tampon[4096];
        size_t r;
        while ((r = fread(tampon, 1, sizeof(tampon), tf)) > 0) {
            fwrite(tampon, 1, r, stdout);
        }
        fclose(tf);
        arena_serbest(a);
        return 0;  /* ftell desteklenmiyor — dogrulamasiz akit */
    }
    rewind(tf);
    char *ir = (char *)malloc((size_t)boyut + 1);
    if (!ir) {
        fclose(tf);
        int n = llvm_ir_uret(prog, stdout);  /* bellek yok — fallback */
        arena_serbest(a);
        return n > 0 ? 1 : 0;
    }
    size_t okunan = fread(ir, 1, (size_t)boyut, tf);
    ir[okunan] = '\0';
    fclose(tf);

    char hata[256];
    if (llvm_ir_dogrula(ir, hata, sizeof(hata)) != 0) {
        fprintf(stderr, "kemgu: internal-codegen-error: %s\n", hata);
        free(ir);
        arena_serbest(a);
        return 1;
    }
    fwrite(ir, 1, okunan, stdout);
    free(ir);
    arena_serbest(a);
    return 0;
}

static void kullanim_yazdir(const char *prog_adi) {
    fprintf(stderr,
        "Kullanim: %s [--token | --parse | --check | --llvm | --lsp] [dosya]\n"
        "  --token   Lexer akisini yazdir\n"
        "  --parse   Parser calistir + AST yazdir\n"
        "  --check   Parser + tip kontrol (varsayilan)\n"
        "  --llvm    LLVM IR text yazdir (clang -x ir - ile derlenebilir)\n"
        "  --lsp     Language Server (stdio JSON-RPC)\n"
        "  --no-verify  LLVM IR dogrulama kapisini kapat (sadece --llvm)\n"
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
        } else if (strcmp(argv[arg_idx], "--lsp") == 0) {
            mod = MOD_LSP;
            arg_idx++;
        } else if (strcmp(argv[arg_idx], "--no-verify") == 0) {
            g_llvm_dogrula = 0;  /* C2 kapisini kapat (benchmark kacisi) */
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

    /* LSP modu dosya/kaynak okumaz — stdio loop'una gecer */
    if (mod == MOD_LSP) {
        return lsp_server_calistir(stdin, stdout);
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
