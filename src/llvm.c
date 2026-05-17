#include "llvm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

/*
 * KEMGU LLVM IR text uretici (alloca tabanli)
 * ============================================
 *
 * Tasarım: clang'in -O0 ile urettigi alloca-tabanli model.
 *   - Her degisken/parametre fonksiyon basinda `alloca` ile yer alir
 *   - Okuma `load`, yazma `store`
 *   - Kontrol akisi basic block + label + br ile
 *   - PHI dugumu YOK (mem2reg optimize pass'i bunu yapar)
 *
 * Desteklenenler:
 *   - Tipler: tam8-64, dtam8-64, kesirli32/64, mantiksal (i1),
 *     karakter (i32), bos (void), *T (ptr), &T (ptr), yapi X
 *   - Ifadeler: tam/kesirli/mantiksal literal, tanimlayici (load),
 *     ikili (arti eksi carpi bolu mod, karsilastirma, ve/veya),
 *     tekli (neg, degil, ref, deref), boyut<T>, cagri, lambda IIFE
 *   - Deyimler: degisken, atama, ver, eger, iken, blok, ifade_deyimi
 *   - Yapilar: struct tanim, yapi_olustur, erisim (alan), atama erisime
 *   - Lambda lifting: __lambda_N olarak ust duzeye cikar (capture YOK)
 *
 * Sinirlamalar:
 *   - Dizi codegen yok (ADIM 14.b)
 *   - icin/esles yok (ADIM 14.c)
 *   - Inline asm + volatile yok (ADIM 14.e)
 *   - Generic monomorphization (yapı<T>) yapilmamis — yapi adi kullanilir
 *   - String literal henuz tam degil (ptr varsayar)
 */

#define LLVM_SEM_KAP 512
#define LLVM_TIP_BUF 128
#define LLVM_YAPI_KAP 64

typedef struct {
    const char *ad;
    int ad_uz;
    char yer_ssa[64];      /* alloca pointer'i (lvalue): "%x.addr" veya "%5" */
    char llvm_tip[LLVM_TIP_BUF];  /* element tipi: "i32", "%struct.Foo" */
    char eleman_tip[LLVM_TIP_BUF];/* Dizi<T> ise T'nin LLVM tipi, yoksa "" */
    int kategori;          /* 0 = lokal (yer=alloca), 1 = lambda fn ptr,
                              2 = closure (yer=env ptr SSA) */
    int lambda_no;         /* kategori=2 icin lambda numarasi (-1 yoksa) */
} LLVMSem;

/* Lambda free var (capture) — closure desteklemek icin */
typedef struct CaptureItem {
    const char *ad;
    int ad_uz;
    char llvm_tip[LLVM_TIP_BUF];
    /* Yer (alloca pointer) — lift zamaninda kopyalanir */
    char yer_ssa[64];
    struct CaptureItem *sonraki;
} CaptureItem;

typedef struct LambdaListe {
    const Dugum *lambda;
    int no;
    /* Captures (free vars). NULL ise non-capturing lambda */
    CaptureItem *captures;
    int capture_say;
    struct LambdaListe *sonraki;
} LambdaListe;

/* String literal sabiti — IR sonunda @.s<N> ve @.str<N> olarak emit edilir */
typedef struct StringSabit {
    int no;
    const char *metin;
    int uzunluk;
    struct StringSabit *sonraki;
} StringSabit;

/* Harici (declare) islev kaydi — coklu dosya/ayri obje icin */
typedef struct ExtDecl {
    char ad[64];
    char donus_tip[LLVM_TIP_BUF];
    char arg_tipler[8][LLVM_TIP_BUF];  /* en fazla 8 arg */
    int arg_say;
    struct ExtDecl *sonraki;
} ExtDecl;

typedef struct {
    int reg;                  /* sonraki SSA %N */
    int etiket_no;            /* sonraki etiket numarasi */
    int blok_terminated;      /* 1 = bu basic block ret/br ile bitti */
    LLVMSem semboller[LLVM_SEM_KAP];
    int sem_say;
    LambdaListe *lambdalar;
    int lambda_no_son;
    /* Yapi tanimlari (struct types) */
    const Dugum *yapilar[LLVM_YAPI_KAP];
    int yapi_say;
    /* Yerel olarak tanimlanan islev adlari + donus tipleri */
    char yerel_islev_ad[256][64];
    char yerel_islev_donus[256][LLVM_TIP_BUF];
    int yerel_islev_say;
    /* En az bir ciplak (naked) islev varsa attribute group emit edilecek */
    int naked_kullanildi;
    /* String literal sabitleri (DUGUM_METIN -> @.s<N>) */
    StringSabit *stringler;
    int string_no_son;
    /* Harici cagri kayitlari (declare icin) */
    ExtDecl *ext_decls;
    /* Aktif islevin donus tipi (default ret icin) */
    char aktif_donus_tip[LLVM_TIP_BUF];
    /* Aktif beklenen Dizi eleman tipi (degisken anotasyonundan) */
    char beklenen_eleman_tip[LLVM_TIP_BUF];
    FILE *out;
} LLVMCtx;

static void yerel_islev_kaydet(LLVMCtx *ctx, const char *ad, int ad_uz,
                                const char *donus_tip) {
    if (ctx->yerel_islev_say >= 256) return;
    int n = ad_uz < 63 ? ad_uz : 63;
    memcpy(ctx->yerel_islev_ad[ctx->yerel_islev_say], ad, (size_t)n);
    ctx->yerel_islev_ad[ctx->yerel_islev_say][n] = '\0';
    snprintf(ctx->yerel_islev_donus[ctx->yerel_islev_say],
             LLVM_TIP_BUF, "%s", donus_tip ? donus_tip : "i32");
    ctx->yerel_islev_say++;
}

static int yerel_islev_var(LLVMCtx *ctx, const char *ad, int ad_uz) {
    int n = ad_uz < 63 ? ad_uz : 63;
    for (int i = 0; i < ctx->yerel_islev_say; i++) {
        if (strlen(ctx->yerel_islev_ad[i]) == (size_t)n &&
            memcmp(ctx->yerel_islev_ad[i], ad, (size_t)n) == 0) {
            return 1;
        }
    }
    return 0;
}

static const char *yerel_islev_donus_tipi(LLVMCtx *ctx,
                                            const char *ad, int ad_uz) {
    int n = ad_uz < 63 ? ad_uz : 63;
    for (int i = 0; i < ctx->yerel_islev_say; i++) {
        if (strlen(ctx->yerel_islev_ad[i]) == (size_t)n &&
            memcmp(ctx->yerel_islev_ad[i], ad, (size_t)n) == 0) {
            return ctx->yerel_islev_donus[i];
        }
    }
    return NULL;
}

static void ext_decl_kaydet(LLVMCtx *ctx, const char *ad, int ad_uz,
                              const char *donus_tip,
                              char arg_tip[8][LLVM_TIP_BUF], int arg_say) {
    /* Zaten varsa atla */
    for (ExtDecl *e = ctx->ext_decls; e; e = e->sonraki) {
        if (strncmp(e->ad, ad, (size_t)ad_uz) == 0 &&
            (int)strlen(e->ad) == ad_uz) return;
    }
    ExtDecl *e = (ExtDecl *)malloc(sizeof(ExtDecl));
    int n = ad_uz < 63 ? ad_uz : 63;
    memcpy(e->ad, ad, (size_t)n);
    e->ad[n] = '\0';
    snprintf(e->donus_tip, sizeof(e->donus_tip), "%s", donus_tip);
    e->arg_say = arg_say > 8 ? 8 : arg_say;
    for (int i = 0; i < e->arg_say; i++) {
        snprintf(e->arg_tipler[i], LLVM_TIP_BUF, "%s", arg_tip[i]);
    }
    e->sonraki = ctx->ext_decls;
    ctx->ext_decls = e;
}

/* === Yardimci ileri tanimlar === */

static int ifade_uret(LLVMCtx *ctx, const Dugum *d,
                       const char *beklenen_tip, char *out_tip);
static int ifade_yeri_uret(LLVMCtx *ctx, const Dugum *d, char *out_tip);
static void deyim_uret(LLVMCtx *ctx, const Dugum *d);
static int llvm_tip_str(const Dugum *tip_d, char *buf, size_t bsz);

/* === Sembol tablo === */

static void sem_ekle(LLVMCtx *ctx, const char *ad, int ad_uz,
                     const char *yer_ssa, const char *llvm_tip, int kategori) {
    if (ctx->sem_say >= LLVM_SEM_KAP) return;
    LLVMSem *s = &ctx->semboller[ctx->sem_say++];
    s->ad = ad;
    s->ad_uz = ad_uz;
    snprintf(s->yer_ssa, sizeof(s->yer_ssa), "%s", yer_ssa);
    snprintf(s->llvm_tip, sizeof(s->llvm_tip), "%s", llvm_tip);
    s->eleman_tip[0] = '\0';
    s->kategori = kategori;
    s->lambda_no = -1;
}

static void sem_lambda_no_ayarla(LLVMCtx *ctx, const char *ad, int ad_uz,
                                   int no) {
    for (int i = ctx->sem_say - 1; i >= 0; i--) {
        if (ctx->semboller[i].ad_uz == ad_uz &&
            memcmp(ctx->semboller[i].ad, ad, (size_t)ad_uz) == 0) {
            ctx->semboller[i].lambda_no = no;
            return;
        }
    }
}

static void sem_eleman_tip_ayarla(LLVMCtx *ctx, const char *ad, int ad_uz,
                                    const char *eleman_tip) {
    /* En son eklenen sembol icin eleman_tip set */
    for (int i = ctx->sem_say - 1; i >= 0; i--) {
        if (ctx->semboller[i].ad_uz == ad_uz &&
            memcmp(ctx->semboller[i].ad, ad, (size_t)ad_uz) == 0) {
            snprintf(ctx->semboller[i].eleman_tip,
                     sizeof(ctx->semboller[i].eleman_tip),
                     "%s", eleman_tip);
            return;
        }
    }
}

static const LLVMSem *sem_bul(const LLVMCtx *ctx,
                                const char *ad, int ad_uz) {
    for (int i = ctx->sem_say - 1; i >= 0; i--) {
        if (ctx->semboller[i].ad_uz == ad_uz &&
            memcmp(ctx->semboller[i].ad, ad, (size_t)ad_uz) == 0) {
            return &ctx->semboller[i];
        }
    }
    return NULL;
}

/* === Etiket / temel blok yardimcilari === */

static int yeni_etiket(LLVMCtx *ctx) { return ctx->etiket_no++; }

static void emit_etiket(LLVMCtx *ctx, const char *etiket) {
    fprintf(ctx->out, "%s:\n", etiket);
    ctx->blok_terminated = 0;
}

static void emit_br(LLVMCtx *ctx, const char *hedef) {
    if (ctx->blok_terminated) return;
    fprintf(ctx->out, "  br label %%%s\n", hedef);
    ctx->blok_terminated = 1;
}

static void emit_br_cond(LLVMCtx *ctx, int cond_reg,
                          const char *t_etiket, const char *f_etiket) {
    if (ctx->blok_terminated) return;
    fprintf(ctx->out, "  br i1 %%%d, label %%%s, label %%%s\n",
            cond_reg, t_etiket, f_etiket);
    ctx->blok_terminated = 1;
}

/* === Tip dugumunden bayt boyutu (boyut<T> icin) === */

static int64_t basit_tip_boyut(const char *ad, int uz) {
    static const struct { const char *a; int u; int64_t b; } t[] = {
        {"tam8",      4, 1},
        {"tam16",     5, 2},
        {"tam32",     5, 4},
        {"tam64",     5, 8},
        {"dtam8",     5, 1},
        {"dtam16",    6, 2},
        {"dtam32",    6, 4},
        {"dtam64",    6, 8},
        {"kesirli32", 9, 4},
        {"kesirli64", 9, 8},
        {"karakter",  8, 4},
        {"metin",     5, 16},
        {"mant\xc4\xb1ksal", 10, 1},
        {"bo\xc5\x9f", 4, 0},
    };
    int n = (int)(sizeof(t) / sizeof(t[0]));
    for (int i = 0; i < n; i++) {
        if (t[i].u == uz && memcmp(t[i].a, ad, (size_t)uz) == 0) {
            return t[i].b;
        }
    }
    return -1;
}

static int64_t ast_tip_boyut(const Dugum *tip_d) {
    if (!tip_d) return -1;
    switch (tip_d->tip) {
        case DUGUM_TIP_BASIT:
            return basit_tip_boyut(tip_d->veri.tip_basit.ad,
                                   tip_d->veri.tip_basit.ad_uzunluk);
        case DUGUM_TIP_POINTER:
        case DUGUM_TIP_REFERANS:
        case DUGUM_TIP_ISLEV:
            return 8;
        case DUGUM_TIP_DIZI:
        case DUGUM_TIP_SECIMLIK:
        case DUGUM_TIP_SONUC:
            return 16;
        default:
            return -1;
    }
}

/* === Tip mapping: AST tip dugumu -> LLVM tip stringi === */

static int basit_llvm_tip(const char *ad, int uz, char *buf, size_t bsz) {
    struct { const char *a; int u; const char *llvm; } t[] = {
        {"tam8",      4, "i8"},
        {"tam16",     5, "i16"},
        {"tam32",     5, "i32"},
        {"tam64",     5, "i64"},
        {"dtam8",     5, "i8"},
        {"dtam16",    6, "i16"},
        {"dtam32",    6, "i32"},
        {"dtam64",    6, "i64"},
        {"kesirli32", 9, "float"},
        {"kesirli64", 9, "double"},
        {"karakter",  8, "i32"},
        {"metin",     5, "{ ptr, i64 }"},  /* slice: ptr + uzunluk */
        {"mant\xc4\xb1ksal", 10, "i1"},
        {"bo\xc5\x9f", 4, "void"},
    };
    int n = (int)(sizeof(t) / sizeof(t[0]));
    for (int i = 0; i < n; i++) {
        if (t[i].u == uz && memcmp(t[i].a, ad, (size_t)uz) == 0) {
            snprintf(buf, bsz, "%s", t[i].llvm);
            return 1;
        }
    }
    return 0;
}

static int llvm_tip_str(const Dugum *tip_d, char *buf, size_t bsz) {
    if (!tip_d) {
        snprintf(buf, bsz, "i32");
        return 0;
    }
    switch (tip_d->tip) {
        case DUGUM_TIP_BASIT:
            if (basit_llvm_tip(tip_d->veri.tip_basit.ad,
                                tip_d->veri.tip_basit.ad_uzunluk,
                                buf, bsz)) return 1;
            /* Kullanici tipi olarak yorumla (yapi) */
            snprintf(buf, bsz, "%%struct.%.*s",
                     tip_d->veri.tip_basit.ad_uzunluk,
                     tip_d->veri.tip_basit.ad);
            return 1;

        case DUGUM_TIP_POINTER:
        case DUGUM_TIP_REFERANS:
            snprintf(buf, bsz, "ptr");
            return 1;

        case DUGUM_TIP_DIZI:
            /* {ptr, i64} slice */
            snprintf(buf, bsz, "{ ptr, i64 }");
            return 1;

        case DUGUM_TIP_KULLANICI:
            if (tip_d->veri.tip_kullanici.yol &&
                tip_d->veri.tip_kullanici.yol->tip == DUGUM_TANIMLAYICI) {
                snprintf(buf, bsz, "%%struct.%.*s",
                    tip_d->veri.tip_kullanici.yol->veri.tanimlayici.uzunluk,
                    tip_d->veri.tip_kullanici.yol->veri.tanimlayici.metin);
                return 1;
            }
            snprintf(buf, bsz, "i32");
            return 0;

        case DUGUM_TIP_ISLEV:
            snprintf(buf, bsz, "ptr");
            return 1;

        case DUGUM_TIP_SECIMLIK:
        case DUGUM_TIP_SONUC:
            snprintf(buf, bsz, "{ i32, i64 }");
            return 1;

        case DUGUM_TIP_TEKKEZ:
            /* tekkez<T> runtime T ile aynidir; linearity sadece tip-level */
            return llvm_tip_str(tip_d->veri.tip_tekkez.ic_tip, buf, bsz);

        default:
            snprintf(buf, bsz, "i32");
            return 0;
    }
}

/* === Yapi tanim toplama (struct types) === */

static void yapi_kaydet(LLVMCtx *ctx, const Dugum *yapi) {
    if (ctx->yapi_say >= LLVM_YAPI_KAP) return;
    ctx->yapilar[ctx->yapi_say++] = yapi;
}

static const Dugum *yapi_bul(LLVMCtx *ctx, const char *ad, int ad_uz) {
    for (int i = 0; i < ctx->yapi_say; i++) {
        const Dugum *y = ctx->yapilar[i];
        if (y->veri.yapi.ad_uzunluk == ad_uz &&
            memcmp(y->veri.yapi.ad, ad, (size_t)ad_uz) == 0) {
            return y;
        }
    }
    return NULL;
}

static void yapi_tanim_uret(LLVMCtx *ctx, const Dugum *yapi) {
    fprintf(ctx->out, "%%struct.%.*s = type { ",
            yapi->veri.yapi.ad_uzunluk, yapi->veri.yapi.ad);
    for (int i = 0; i < yapi->veri.yapi.alan_sayi; i++) {
        const Dugum *alan = yapi->veri.yapi.alanlar[i];
        if (i > 0) fputs(", ", ctx->out);
        char tipbuf[LLVM_TIP_BUF];
        llvm_tip_str(alan->veri.alan.tip, tipbuf, sizeof(tipbuf));
        fputs(tipbuf, ctx->out);
    }
    fputs(" }\n", ctx->out);
}

/* === Lambda lifting === */

/* Lambda params arasinda ad arar */
static int lambda_param_mi(const Dugum *lambda, const char *ad, int ad_uz) {
    for (int i = 0; i < lambda->veri.lambda.param_sayi; i++) {
        const Dugum *p = lambda->veri.lambda.parametreler[i];
        if (p->veri.parametre.ad_uzunluk == ad_uz &&
            memcmp(p->veri.parametre.ad, ad, (size_t)ad_uz) == 0) {
            return 1;
        }
    }
    return 0;
}

/* Capture listesinde var mi */
static int capture_var(CaptureItem *list, const char *ad, int ad_uz) {
    for (CaptureItem *c = list; c; c = c->sonraki) {
        if (c->ad_uz == ad_uz &&
            memcmp(c->ad, ad, (size_t)ad_uz) == 0) return 1;
    }
    return 0;
}

/* Lambda govdesinde TANIMLAYICI'lari recursive tara, sym table'da
 * bulunup param olmayanlari capture olarak listeye ekle. */
static void freevars_topla(LLVMCtx *ctx, const Dugum *d,
                            const Dugum *lambda,
                            CaptureItem **out_captures, int *out_say) {
    if (!d) return;
    switch (d->tip) {
        case DUGUM_TANIMLAYICI: {
            const char *ad = d->veri.tanimlayici.metin;
            int ad_uz = d->veri.tanimlayici.uzunluk;
            if (lambda_param_mi(lambda, ad, ad_uz)) return;
            if (capture_var(*out_captures, ad, ad_uz)) return;
            const LLVMSem *s = sem_bul(ctx, ad, ad_uz);
            if (!s) return;
            /* Yerel olmayan global islev de capture olamaz */
            if (s->kategori == 1) return;
            CaptureItem *c = (CaptureItem *)malloc(sizeof(CaptureItem));
            c->ad = ad;
            c->ad_uz = ad_uz;
            snprintf(c->llvm_tip, sizeof(c->llvm_tip), "%s", s->llvm_tip);
            snprintf(c->yer_ssa, sizeof(c->yer_ssa), "%s", s->yer_ssa);
            c->sonraki = *out_captures;
            *out_captures = c;
            (*out_say)++;
            return;
        }
        case DUGUM_IKILI:
            freevars_topla(ctx, d->veri.ikili.sol, lambda, out_captures, out_say);
            freevars_topla(ctx, d->veri.ikili.sag, lambda, out_captures, out_say);
            return;
        case DUGUM_TEKLI:
            freevars_topla(ctx, d->veri.tekli.operand, lambda, out_captures, out_say);
            return;
        case DUGUM_CAGRI:
            freevars_topla(ctx, d->veri.cagri.hedef, lambda, out_captures, out_say);
            for (int i = 0; i < d->veri.cagri.sayi; i++) {
                freevars_topla(ctx, d->veri.cagri.argumanlar[i],
                               lambda, out_captures, out_say);
            }
            return;
        case DUGUM_ERISIM:
            freevars_topla(ctx, d->veri.erisim.nesne, lambda, out_captures, out_say);
            return;
        case DUGUM_INDEKS:
            freevars_topla(ctx, d->veri.indeks.nesne, lambda, out_captures, out_say);
            freevars_topla(ctx, d->veri.indeks.indeks, lambda, out_captures, out_say);
            return;
        case DUGUM_YAPI_OLUSTUR:
            for (int i = 0; i < d->veri.yapi_olustur.alan_sayi; i++) {
                freevars_topla(ctx,
                    d->veri.yapi_olustur.alanlar[i]->veri.alan_atama.deger,
                    lambda, out_captures, out_say);
            }
            return;
        case DUGUM_DIZI_OLUSTUR:
            for (int i = 0; i < d->veri.dizi_olustur.sayi; i++) {
                freevars_topla(ctx, d->veri.dizi_olustur.elemanlar[i],
                               lambda, out_captures, out_say);
            }
            return;
        case DUGUM_DEGISKEN:
            freevars_topla(ctx, d->veri.degisken.deger, lambda, out_captures, out_say);
            return;
        case DUGUM_ATAMA:
            freevars_topla(ctx, d->veri.atama.hedef, lambda, out_captures, out_say);
            freevars_topla(ctx, d->veri.atama.deger, lambda, out_captures, out_say);
            return;
        case DUGUM_VER:
            freevars_topla(ctx, d->veri.ver.deger, lambda, out_captures, out_say);
            return;
        case DUGUM_EGER:
            freevars_topla(ctx, d->veri.eger.kosul, lambda, out_captures, out_say);
            freevars_topla(ctx, d->veri.eger.gozdoldur, lambda, out_captures, out_say);
            freevars_topla(ctx, d->veri.eger.yan, lambda, out_captures, out_say);
            return;
        case DUGUM_IKEN:
            freevars_topla(ctx, d->veri.iken.kosul, lambda, out_captures, out_say);
            freevars_topla(ctx, d->veri.iken.govde, lambda, out_captures, out_say);
            return;
        case DUGUM_ICIN:
            freevars_topla(ctx, d->veri.icin.koleksiyon, lambda, out_captures, out_say);
            freevars_topla(ctx, d->veri.icin.govde, lambda, out_captures, out_say);
            return;
        case DUGUM_ESLES:
            freevars_topla(ctx, d->veri.esles.deger, lambda, out_captures, out_say);
            for (int i = 0; i < d->veri.esles.kol_sayi; i++) {
                freevars_topla(ctx,
                    d->veri.esles.kollar[i]->veri.esles_kolu.govde,
                    lambda, out_captures, out_say);
            }
            return;
        case DUGUM_BLOK:
            for (int i = 0; i < d->veri.blok.sayi; i++) {
                freevars_topla(ctx, d->veri.blok.deyimler[i],
                               lambda, out_captures, out_say);
            }
            return;
        case DUGUM_IFADE_DEYIMI:
            freevars_topla(ctx, d->veri.ifade_deyimi.ifade,
                           lambda, out_captures, out_say);
            return;
        case DUGUM_GUVENSIZ:
            freevars_topla(ctx, d->veri.guvensiz.blok,
                           lambda, out_captures, out_say);
            return;
        default:
            return;
    }
}

static int lambda_lift(LLVMCtx *ctx, const Dugum *lambda) {
    int no = ctx->lambda_no_son++;
    LambdaListe *l = (LambdaListe *)malloc(sizeof(LambdaListe));
    if (!l) return -1;
    l->lambda = lambda;
    l->no = no;
    /* Captures topla — şu anki sym table'a bakarak */
    l->captures = NULL;
    l->capture_say = 0;
    if (lambda->veri.lambda.govde) {
        freevars_topla(ctx, lambda->veri.lambda.govde, lambda,
                        &l->captures, &l->capture_say);
    }
    l->sonraki = ctx->lambdalar;
    ctx->lambdalar = l;
    return no;
}

static LambdaListe *lambda_no_bul(LLVMCtx *ctx, int no) {
    for (LambdaListe *l = ctx->lambdalar; l; l = l->sonraki) {
        if (l->no == no) return l;
    }
    return NULL;
}

/* === Ifade: degerin tipi cikarsama (tip kontrol gibi degil, basit) === */
/* Onek: ya beklenen_tip kullan, ya literal/sembol tipinden cikar */

static const char *tip_int_uygun_mu(const char *t) {
    if (!t) return NULL;
    if (strcmp(t, "i1") == 0 ||
        strcmp(t, "i8") == 0 ||
        strcmp(t, "i16") == 0 ||
        strcmp(t, "i32") == 0 ||
        strcmp(t, "i64") == 0) return t;
    return NULL;
}

/* === Ifade emit (rvalue / degerin) === */

static int ifade_uret(LLVMCtx *ctx, const Dugum *d,
                       const char *beklenen_tip, char *out_tip) {
    if (!d || ctx->blok_terminated) {
        if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "i32");
        return -1;
    }

    switch (d->tip) {
        case DUGUM_TAM: {
            const char *t = tip_int_uygun_mu(beklenen_tip);
            if (!t) t = "i32";
            int r = ctx->reg++;
            fprintf(ctx->out, "  %%%d = add %s 0, %" PRId64 "\n",
                    r, t, d->veri.tam.deger);
            if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "%s", t);
            return r;
        }

        case DUGUM_MANTIKSAL: {
            int r = ctx->reg++;
            fprintf(ctx->out, "  %%%d = or i1 0, %d\n",
                    r, d->veri.mantiksal.deger ? 1 : 0);
            if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "i1");
            return r;
        }

        case DUGUM_KESIRLI: {
            const char *t = (beklenen_tip && strcmp(beklenen_tip, "float") == 0)
                          ? "float" : "double";
            int r = ctx->reg++;
            fprintf(ctx->out, "  %%%d = fadd %s 0.0, %g\n",
                    r, t, d->veri.kesirli.deger);
            if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "%s", t);
            return r;
        }

        case DUGUM_KARAKTER: {
            int r = ctx->reg++;
            fprintf(ctx->out, "  %%%d = add i32 0, %u\n",
                    r, d->veri.karakter.kod_noktasi);
            if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "i32");
            return r;
        }

        case DUGUM_METIN: {
            /* String literal -> slice { ptr, i64 } */
            /* Onceden kaydedilmis mi? */
            StringSabit *ss = ctx->stringler;
            int found_no = -1;
            while (ss) {
                if (ss->uzunluk == d->veri.metin_lit.uzunluk &&
                    memcmp(ss->metin, d->veri.metin_lit.metin,
                           (size_t)ss->uzunluk) == 0) {
                    found_no = ss->no;
                    break;
                }
                ss = ss->sonraki;
            }
            if (found_no < 0) {
                ss = (StringSabit *)malloc(sizeof(StringSabit));
                ss->no = ctx->string_no_son++;
                ss->metin = d->veri.metin_lit.metin;
                ss->uzunluk = d->veri.metin_lit.uzunluk;
                ss->sonraki = ctx->stringler;
                ctx->stringler = ss;
                found_no = ss->no;
            }
            int r = ctx->reg++;
            fprintf(ctx->out,
                "  %%%d = load { ptr, i64 }, ptr @.s%d\n",
                r, found_no);
            if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "{ ptr, i64 }");
            return r;
        }

        case DUGUM_BOYUT: {
            int64_t b = ast_tip_boyut(d->veri.boyut.tip);
            const char *t = tip_int_uygun_mu(beklenen_tip);
            if (!t) t = "i64";
            int r = ctx->reg++;
            fprintf(ctx->out, "  %%%d = add %s 0, %" PRId64
                    "  ; boyut<T>\n", r, t, b < 0 ? 0 : b);
            if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "%s", t);
            return r;
        }

        case DUGUM_TANIMLAYICI: {
            const LLVMSem *s = sem_bul(ctx,
                d->veri.tanimlayici.metin,
                d->veri.tanimlayici.uzunluk);
            if (!s) {
                fprintf(ctx->out, "  ; tanimsiz: %.*s\n",
                        d->veri.tanimlayici.uzunluk,
                        d->veri.tanimlayici.metin);
                int r = ctx->reg++;
                fprintf(ctx->out, "  %%%d = add i32 0, 0\n", r);
                if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "i32");
                return r;
            }
            int r = ctx->reg++;
            fprintf(ctx->out, "  %%%d = load %s, ptr %s\n",
                    r, s->llvm_tip, s->yer_ssa);
            if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "%s", s->llvm_tip);
            return r;
        }

        case DUGUM_IKILI: {
            char sol_tip[LLVM_TIP_BUF], sag_tip[LLVM_TIP_BUF];
            int sol = ifade_uret(ctx, d->veri.ikili.sol, beklenen_tip, sol_tip);
            int sag = ifade_uret(ctx, d->veri.ikili.sag, sol_tip, sag_tip);
            if (sol < 0 || sag < 0) return -1;
            const char *tip = sol_tip;
            int r = ctx->reg++;
            switch (d->veri.ikili.op) {
                case OP_ARTI:
                    fprintf(ctx->out, "  %%%d = add %s %%%d, %%%d\n",
                            r, tip, sol, sag);
                    if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "%s", tip);
                    return r;
                case OP_EKSI:
                    fprintf(ctx->out, "  %%%d = sub %s %%%d, %%%d\n",
                            r, tip, sol, sag);
                    if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "%s", tip);
                    return r;
                case OP_CARPI:
                    fprintf(ctx->out, "  %%%d = mul %s %%%d, %%%d\n",
                            r, tip, sol, sag);
                    if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "%s", tip);
                    return r;
                case OP_BOLU:
                    fprintf(ctx->out, "  %%%d = sdiv %s %%%d, %%%d\n",
                            r, tip, sol, sag);
                    if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "%s", tip);
                    return r;
                case OP_MOD:
                    fprintf(ctx->out, "  %%%d = srem %s %%%d, %%%d\n",
                            r, tip, sol, sag);
                    if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "%s", tip);
                    return r;
                case OP_ESIT:
                    fprintf(ctx->out, "  %%%d = icmp eq %s %%%d, %%%d\n",
                            r, tip, sol, sag);
                    if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "i1");
                    return r;
                case OP_ESIT_DEGIL:
                    fprintf(ctx->out, "  %%%d = icmp ne %s %%%d, %%%d\n",
                            r, tip, sol, sag);
                    if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "i1");
                    return r;
                case OP_KUCUK:
                    fprintf(ctx->out, "  %%%d = icmp slt %s %%%d, %%%d\n",
                            r, tip, sol, sag);
                    if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "i1");
                    return r;
                case OP_BUYUK:
                    fprintf(ctx->out, "  %%%d = icmp sgt %s %%%d, %%%d\n",
                            r, tip, sol, sag);
                    if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "i1");
                    return r;
                case OP_KUCUK_ESIT:
                    fprintf(ctx->out, "  %%%d = icmp sle %s %%%d, %%%d\n",
                            r, tip, sol, sag);
                    if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "i1");
                    return r;
                case OP_BUYUK_ESIT:
                    fprintf(ctx->out, "  %%%d = icmp sge %s %%%d, %%%d\n",
                            r, tip, sol, sag);
                    if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "i1");
                    return r;
                case OP_VE:
                    fprintf(ctx->out, "  %%%d = and i1 %%%d, %%%d\n",
                            r, sol, sag);
                    if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "i1");
                    return r;
                case OP_VEYA:
                    fprintf(ctx->out, "  %%%d = or i1 %%%d, %%%d\n",
                            r, sol, sag);
                    if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "i1");
                    return r;
                case OP_BIT_VE:
                    fprintf(ctx->out, "  %%%d = and %s %%%d, %%%d\n",
                            r, tip, sol, sag);
                    if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "%s", tip);
                    return r;
                case OP_BIT_VEYA:
                    fprintf(ctx->out, "  %%%d = or %s %%%d, %%%d\n",
                            r, tip, sol, sag);
                    if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "%s", tip);
                    return r;
                case OP_BIT_OZVEYA:
                    fprintf(ctx->out, "  %%%d = xor %s %%%d, %%%d\n",
                            r, tip, sol, sag);
                    if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "%s", tip);
                    return r;
                case OP_SOLA_KAYDIR:
                    fprintf(ctx->out, "  %%%d = shl %s %%%d, %%%d\n",
                            r, tip, sol, sag);
                    if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "%s", tip);
                    return r;
                case OP_SAGA_KAYDIR:
                    /* lshr — logical shift right (zero fill) — KEMGU
                     * dtam tipler unsigned; tam tipler de zaten C-style
                     * `>>` cogu zaman lshr olarak yorumlanir. Arithmetic
                     * shift gerekirse ayri intrinsic eklenebilir. */
                    fprintf(ctx->out, "  %%%d = lshr %s %%%d, %%%d\n",
                            r, tip, sol, sag);
                    if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "%s", tip);
                    return r;
                default:
                    fputs("  ; ikili op desteklenmiyor\n", ctx->out);
                    return -1;
            }
        }

        case DUGUM_TEKLI: {
            switch (d->veri.tekli.op) {
                case OP_NEG: {
                    char ot[LLVM_TIP_BUF];
                    int op = ifade_uret(ctx, d->veri.tekli.operand,
                                        beklenen_tip, ot);
                    if (op < 0) return -1;
                    int r = ctx->reg++;
                    fprintf(ctx->out, "  %%%d = sub %s 0, %%%d\n", r, ot, op);
                    if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "%s", ot);
                    return r;
                }
                case OP_DEGIL: {
                    char ot[LLVM_TIP_BUF];
                    int op = ifade_uret(ctx, d->veri.tekli.operand, "i1", ot);
                    if (op < 0) return -1;
                    int r = ctx->reg++;
                    fprintf(ctx->out, "  %%%d = xor i1 %%%d, 1\n", r, op);
                    if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "i1");
                    return r;
                }
                case OP_BIT_DEGIL: {
                    /* ~x  =>  xor x, -1 (all-bits-set) */
                    char ot[LLVM_TIP_BUF];
                    int op = ifade_uret(ctx, d->veri.tekli.operand,
                                        beklenen_tip, ot);
                    if (op < 0) return -1;
                    int r = ctx->reg++;
                    fprintf(ctx->out, "  %%%d = xor %s %%%d, -1\n",
                            r, ot, op);
                    if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "%s", ot);
                    return r;
                }
                case OP_REF:
                case OP_REF_DEGISKEN: {
                    /* &x: ifade_yeri_uret cagir, ptr don */
                    int yer = ifade_yeri_uret(ctx,
                                d->veri.tekli.operand, NULL);
                    if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "ptr");
                    return yer;
                }
                case OP_DEREFERANS: {
                    /* *p: p degerini al (ptr), bir kez load et */
                    char ot[LLVM_TIP_BUF];
                    int p = ifade_uret(ctx, d->veri.tekli.operand,
                                       "ptr", ot);
                    if (p < 0) return -1;
                    /* Hedef tipi bilemiyoruz (basit lookup);
                     * default i32 */
                    const char *hedef_tip = beklenen_tip ? beklenen_tip : "i32";
                    int r = ctx->reg++;
                    fprintf(ctx->out, "  %%%d = load %s, ptr %%%d\n",
                            r, hedef_tip, p);
                    if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "%s", hedef_tip);
                    return r;
                }
                default:
                    fputs("  ; tekli op desteklenmiyor\n", ctx->out);
                    return -1;
            }
        }

        case DUGUM_LAMBDA: {
            (void)lambda_lift(ctx, d);
            int r = ctx->reg++;
            fprintf(ctx->out,
                "  %%%d = add i32 0, 0  ; lambda ref (fn ptr basit)\n", r);
            if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "ptr");
            return r;
        }

        case DUGUM_CAGRI: {
            const Dugum *h = d->veri.cagri.hedef;
            char fn_ref[64];
            const char *ret_tip = beklenen_tip ? beklenen_tip : "i32";

            /* Eger hedef yerel olarak bilinen islev ise, donus tipini al */
            if (h->tip == DUGUM_TANIMLAYICI) {
                const char *yerel_donus = yerel_islev_donus_tipi(ctx,
                    h->veri.tanimlayici.metin,
                    h->veri.tanimlayici.uzunluk);
                if (yerel_donus) ret_tip = yerel_donus;
            }

            /* === Intrinsic'ler — _asm, _oku_volatile_*, _yaz_volatile_* === */
            if (h->tip == DUGUM_TANIMLAYICI) {
                const char *ad = h->veri.tanimlayici.metin;
                int ad_uz = h->veri.tanimlayici.uzunluk;

                /* === _tekkez_yarat(val) — Spec B.6 producer (Sarı) ===
                 * Runtime: val'i direkt don (tekkez sadece tip-level marker). */
                if (ad_uz == 13 && memcmp(ad, "_tekkez_yarat", 13) == 0 &&
                    d->veri.cagri.sayi == 1) {
                    char ot[LLVM_TIP_BUF];
                    int v = ifade_uret(ctx, d->veri.cagri.argumanlar[0],
                                       beklenen_tip, ot);
                    if (v < 0) return -1;
                    if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "%s", ot);
                    return v;
                }

                /* === imha(x) — Spec B.6 sıfırla_imha codegen === */
                /* x: tekkez<T> alloca pointer; volatile memset 0 emit et.
                 * 'volatile' onemli: optimize-edip-kaldirilamamali (security). */
                if (ad_uz == 4 && memcmp(ad, "imha", 4) == 0 &&
                    d->veri.cagri.sayi == 1) {
                    const Dugum *arg = d->veri.cagri.argumanlar[0];
                    if (arg->tip == DUGUM_TANIMLAYICI) {
                        const LLVMSem *s = sem_bul(ctx,
                            arg->veri.tanimlayici.metin,
                            arg->veri.tanimlayici.uzunluk);
                        if (s) {
                            /* Tip boyutu — basit lookup */
                            int sz = 4;  /* default i32 */
                            if (strcmp(s->llvm_tip, "i8") == 0) sz = 1;
                            else if (strcmp(s->llvm_tip, "i16") == 0) sz = 2;
                            else if (strcmp(s->llvm_tip, "i32") == 0) sz = 4;
                            else if (strcmp(s->llvm_tip, "i64") == 0) sz = 8;
                            else if (strcmp(s->llvm_tip, "float") == 0) sz = 4;
                            else if (strcmp(s->llvm_tip, "double") == 0) sz = 8;
                            else if (strcmp(s->llvm_tip, "ptr") == 0) sz = 8;
                            /* memset.p0.i64 ile volatile zeroize */
                            fprintf(ctx->out,
                                "  call void @llvm.memset.p0.i64(ptr %s, i8 0, i64 %d, i1 true)\n",
                                s->yer_ssa, sz);
                            int r = ctx->reg++;
                            fprintf(ctx->out, "  %%%d = add i32 0, 0\n", r);
                            if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "void");
                            return r;
                        }
                    }
                }

                /* _asm(metin_literal) -> inline assembly */
                if (ad_uz == 4 && memcmp(ad, "_asm", 4) == 0 &&
                    d->veri.cagri.sayi == 1) {
                    const Dugum *arg = d->veri.cagri.argumanlar[0];
                    if (arg->tip == DUGUM_METIN) {
                        /* asm metnini al, IR'e inline yaz */
                        fprintf(ctx->out,
                            "  call void asm sideeffect \"%.*s\", \"\"()\n",
                            arg->veri.metin_lit.uzunluk,
                            arg->veri.metin_lit.metin);
                        int r = ctx->reg++;
                        fprintf(ctx->out, "  %%%d = add i32 0, 0\n", r);
                        if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "void");
                        return r;
                    }
                }

                /* _yaz_volatile_<size>(adres: dtam64, deger: T) */
                if (ad_uz > 15 && memcmp(ad, "_yaz_volatile_", 14) == 0 &&
                    d->veri.cagri.sayi == 2) {
                    const char *sz = ad + 14;
                    const char *llvm_t = NULL;
                    if (memcmp(sz, "dtam8", 5) == 0)  llvm_t = "i8";
                    else if (memcmp(sz, "dtam16", 6) == 0) llvm_t = "i16";
                    else if (memcmp(sz, "dtam32", 6) == 0) llvm_t = "i32";
                    else if (memcmp(sz, "dtam64", 6) == 0) llvm_t = "i64";

                    if (llvm_t) {
                        char ot1[LLVM_TIP_BUF], ot2[LLVM_TIP_BUF];
                        int adres_int = ifade_uret(ctx,
                            d->veri.cagri.argumanlar[0], "i64", ot1);
                        int deger = ifade_uret(ctx,
                            d->veri.cagri.argumanlar[1], llvm_t, ot2);
                        int p = ctx->reg++;
                        fprintf(ctx->out,
                            "  %%%d = inttoptr i64 %%%d to ptr\n",
                            p, adres_int);
                        fprintf(ctx->out,
                            "  store volatile %s %%%d, ptr %%%d\n",
                            llvm_t, deger, p);
                        int r = ctx->reg++;
                        fprintf(ctx->out, "  %%%d = add i32 0, 0\n", r);
                        if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "void");
                        return r;
                    }
                }

                /* _oku_volatile_<size>(adres: dtam64) -> T */
                if (ad_uz > 15 && memcmp(ad, "_oku_volatile_", 14) == 0 &&
                    d->veri.cagri.sayi == 1) {
                    const char *sz = ad + 14;
                    const char *llvm_t = NULL;
                    if (memcmp(sz, "dtam8", 5) == 0)  llvm_t = "i8";
                    else if (memcmp(sz, "dtam16", 6) == 0) llvm_t = "i16";
                    else if (memcmp(sz, "dtam32", 6) == 0) llvm_t = "i32";
                    else if (memcmp(sz, "dtam64", 6) == 0) llvm_t = "i64";

                    if (llvm_t) {
                        char ot[LLVM_TIP_BUF];
                        int adres_int = ifade_uret(ctx,
                            d->veri.cagri.argumanlar[0], "i64", ot);
                        int p = ctx->reg++;
                        fprintf(ctx->out,
                            "  %%%d = inttoptr i64 %%%d to ptr\n",
                            p, adres_int);
                        int r = ctx->reg++;
                        fprintf(ctx->out,
                            "  %%%d = load volatile %s, ptr %%%d\n",
                            r, llvm_t, p);
                        if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "%s", llvm_t);
                        return r;
                    }
                }

                /* === Atomic intrinsics === */
                /* _atomik_oku_<size>(adres) -> T */
                if (ad_uz > 13 && memcmp(ad, "_atomik_oku_", 12) == 0 &&
                    d->veri.cagri.sayi == 1) {
                    const char *sz = ad + 12;
                    const char *llvm_t = NULL; int align = 0;
                    if (memcmp(sz, "dtam8", 5) == 0)  { llvm_t = "i8"; align = 1; }
                    else if (memcmp(sz, "dtam16", 6) == 0) { llvm_t = "i16"; align = 2; }
                    else if (memcmp(sz, "dtam32", 6) == 0) { llvm_t = "i32"; align = 4; }
                    else if (memcmp(sz, "dtam64", 6) == 0) { llvm_t = "i64"; align = 8; }
                    if (llvm_t) {
                        char ot[LLVM_TIP_BUF];
                        int ai = ifade_uret(ctx, d->veri.cagri.argumanlar[0], "i64", ot);
                        int p = ctx->reg++;
                        fprintf(ctx->out, "  %%%d = inttoptr i64 %%%d to ptr\n", p, ai);
                        int r = ctx->reg++;
                        fprintf(ctx->out,
                            "  %%%d = load atomic %s, ptr %%%d seq_cst, align %d\n",
                            r, llvm_t, p, align);
                        if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "%s", llvm_t);
                        return r;
                    }
                }

                /* _atomik_yaz_<size>(adres, deger) -> bos */
                if (ad_uz > 13 && memcmp(ad, "_atomik_yaz_", 12) == 0 &&
                    d->veri.cagri.sayi == 2) {
                    const char *sz = ad + 12;
                    const char *llvm_t = NULL; int align = 0;
                    if (memcmp(sz, "dtam8", 5) == 0)  { llvm_t = "i8"; align = 1; }
                    else if (memcmp(sz, "dtam16", 6) == 0) { llvm_t = "i16"; align = 2; }
                    else if (memcmp(sz, "dtam32", 6) == 0) { llvm_t = "i32"; align = 4; }
                    else if (memcmp(sz, "dtam64", 6) == 0) { llvm_t = "i64"; align = 8; }
                    if (llvm_t) {
                        char ot1[LLVM_TIP_BUF], ot2[LLVM_TIP_BUF];
                        int ai = ifade_uret(ctx, d->veri.cagri.argumanlar[0], "i64", ot1);
                        int dv = ifade_uret(ctx, d->veri.cagri.argumanlar[1], llvm_t, ot2);
                        int p = ctx->reg++;
                        fprintf(ctx->out, "  %%%d = inttoptr i64 %%%d to ptr\n", p, ai);
                        fprintf(ctx->out,
                            "  store atomic %s %%%d, ptr %%%d seq_cst, align %d\n",
                            llvm_t, dv, p, align);
                        int r = ctx->reg++;
                        fprintf(ctx->out, "  %%%d = add i32 0, 0\n", r);
                        if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "void");
                        return r;
                    }
                }

                /* _atomik_topla_<size> + _atomik_takas_<size> -> atomicrmw */
                const char *rmw_op = NULL; int rmw_offset = 0;
                if (ad_uz > 15 && memcmp(ad, "_atomik_topla_", 14) == 0) {
                    rmw_op = "add"; rmw_offset = 14;
                } else if (ad_uz > 15 && memcmp(ad, "_atomik_takas_", 14) == 0) {
                    rmw_op = "xchg"; rmw_offset = 14;
                }
                if (rmw_op && d->veri.cagri.sayi == 2) {
                    const char *sz = ad + rmw_offset;
                    const char *llvm_t = NULL;
                    if (memcmp(sz, "dtam8", 5) == 0)  llvm_t = "i8";
                    else if (memcmp(sz, "dtam16", 6) == 0) llvm_t = "i16";
                    else if (memcmp(sz, "dtam32", 6) == 0) llvm_t = "i32";
                    else if (memcmp(sz, "dtam64", 6) == 0) llvm_t = "i64";
                    if (llvm_t) {
                        char ot1[LLVM_TIP_BUF], ot2[LLVM_TIP_BUF];
                        int ai = ifade_uret(ctx, d->veri.cagri.argumanlar[0], "i64", ot1);
                        int dv = ifade_uret(ctx, d->veri.cagri.argumanlar[1], llvm_t, ot2);
                        int p = ctx->reg++;
                        fprintf(ctx->out, "  %%%d = inttoptr i64 %%%d to ptr\n", p, ai);
                        int r = ctx->reg++;
                        fprintf(ctx->out,
                            "  %%%d = atomicrmw %s ptr %%%d, %s %%%d seq_cst\n",
                            r, rmw_op, p, llvm_t, dv);
                        if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "%s", llvm_t);
                        return r;
                    }
                }

                /* _atomik_cas_<size>(adres, eski, yeni) -> mantiksal */
                if (ad_uz > 13 && memcmp(ad, "_atomik_cas_", 12) == 0 &&
                    d->veri.cagri.sayi == 3) {
                    const char *sz = ad + 12;
                    const char *llvm_t = NULL;
                    if (memcmp(sz, "dtam8", 5) == 0)  llvm_t = "i8";
                    else if (memcmp(sz, "dtam16", 6) == 0) llvm_t = "i16";
                    else if (memcmp(sz, "dtam32", 6) == 0) llvm_t = "i32";
                    else if (memcmp(sz, "dtam64", 6) == 0) llvm_t = "i64";
                    if (llvm_t) {
                        char ot[LLVM_TIP_BUF];
                        int ai  = ifade_uret(ctx, d->veri.cagri.argumanlar[0], "i64", ot);
                        int eski = ifade_uret(ctx, d->veri.cagri.argumanlar[1], llvm_t, ot);
                        int yeni = ifade_uret(ctx, d->veri.cagri.argumanlar[2], llvm_t, ot);
                        int p = ctx->reg++;
                        fprintf(ctx->out, "  %%%d = inttoptr i64 %%%d to ptr\n", p, ai);
                        int pair = ctx->reg++;
                        fprintf(ctx->out,
                            "  %%%d = cmpxchg ptr %%%d, %s %%%d, %s %%%d seq_cst seq_cst\n",
                            pair, p, llvm_t, eski, llvm_t, yeni);
                        int r = ctx->reg++;
                        fprintf(ctx->out,
                            "  %%%d = extractvalue { %s, i1 } %%%d, 1\n",
                            r, llvm_t, pair);
                        if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "i1");
                        return r;
                    }
                }

                /* Memory barriers */
                if ((ad_uz == 14 && memcmp(ad, "_bellek_engeli", 14) == 0 &&
                     d->veri.cagri.sayi == 0) ||
                    (ad_uz == 11 && memcmp(ad, "_oku_engeli", 11) == 0 &&
                     d->veri.cagri.sayi == 0) ||
                    (ad_uz == 11 && memcmp(ad, "_yaz_engeli", 11) == 0 &&
                     d->veri.cagri.sayi == 0)) {
                    const char *kind = "seq_cst";
                    if (ad_uz == 11 && memcmp(ad, "_oku_engeli", 11) == 0)
                        kind = "acquire";
                    else if (ad_uz == 11 && memcmp(ad, "_yaz_engeli", 11) == 0)
                        kind = "release";
                    fprintf(ctx->out, "  fence %s\n", kind);
                    int r = ctx->reg++;
                    fprintf(ctx->out, "  %%%d = add i32 0, 0\n", r);
                    if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "void");
                    return r;
                }
            }

            if (h->tip == DUGUM_LAMBDA) {
                int no = lambda_lift(ctx, h);
                snprintf(fn_ref, sizeof(fn_ref), "@__lambda_%d", no);
            } else if (h->tip == DUGUM_TANIMLAYICI) {
                const LLVMSem *s = sem_bul(ctx,
                    h->veri.tanimlayici.metin,
                    h->veri.tanimlayici.uzunluk);
                if (s && s->kategori == 1) {
                    snprintf(fn_ref, sizeof(fn_ref), "%s", s->yer_ssa);
                } else if (s && s->kategori == 2) {
                    /* Closure: @__lambda_N + env arg pass */
                    snprintf(fn_ref, sizeof(fn_ref),
                             "@__lambda_%d", s->lambda_no);
                } else {
                    snprintf(fn_ref, sizeof(fn_ref), "@%.*s",
                             h->veri.tanimlayici.uzunluk,
                             h->veri.tanimlayici.metin);
                    /* Yerel olmayan islev mi? declare kaydi at */
                    if (!yerel_islev_var(ctx,
                            h->veri.tanimlayici.metin,
                            h->veri.tanimlayici.uzunluk)) {
                        /* Arg tiplerini evaluate edip sonra declare ekleyecegiz */
                        /* Geliyoruz... */
                    }
                }
            } else {
                fputs("  ; cagri hedefi destek disi\n", ctx->out);
                int r = ctx->reg++;
                fprintf(ctx->out, "  %%%d = add i32 0, 0\n", r);
                if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "i32");
                return r;
            }

            int n = d->veri.cagri.sayi;
            int *arg_reg = NULL;
            char (*arg_tip)[LLVM_TIP_BUF] = NULL;
            if (n > 0) {
                arg_reg = (int *)malloc(sizeof(int) * (size_t)n);
                arg_tip = malloc(sizeof(*arg_tip) * (size_t)n);
                for (int i = 0; i < n; i++) {
                    arg_reg[i] = ifade_uret(ctx, d->veri.cagri.argumanlar[i],
                                             NULL, arg_tip[i]);
                }
            }

            /* Yerel olmayan TANIMLAYICI cagrisi -> extern declare kaydi.
             * Closure/lambda variable (kategori 1 veya 2) extern degil. */
            if (h->tip == DUGUM_TANIMLAYICI &&
                !yerel_islev_var(ctx,
                    h->veri.tanimlayici.metin,
                    h->veri.tanimlayici.uzunluk)) {
                const LLVMSem *s = sem_bul(ctx,
                    h->veri.tanimlayici.metin,
                    h->veri.tanimlayici.uzunluk);
                int is_local_closure = s && (s->kategori == 1 || s->kategori == 2);
                if (!is_local_closure) {
                    char ext_arg[8][LLVM_TIP_BUF];
                    int ne = n > 8 ? 8 : n;
                    for (int i = 0; i < ne; i++) {
                        snprintf(ext_arg[i], LLVM_TIP_BUF, "%s", arg_tip[i]);
                    }
                    ext_decl_kaydet(ctx,
                        h->veri.tanimlayici.metin,
                        h->veri.tanimlayici.uzunluk,
                        ret_tip, ext_arg, ne);
                }
            }

            /* Closure cagrisi: hedef closure ise env'i ilk arg olarak ekle */
            const char *closure_env = NULL;
            if (h->tip == DUGUM_TANIMLAYICI) {
                const LLVMSem *s = sem_bul(ctx,
                    h->veri.tanimlayici.metin,
                    h->veri.tanimlayici.uzunluk);
                if (s && s->kategori == 2) {
                    closure_env = s->yer_ssa;
                }
            }

            int is_void = (strcmp(ret_tip, "void") == 0);
            int r = -1;
            if (is_void) {
                fprintf(ctx->out, "  call void %s(", fn_ref);
            } else {
                r = ctx->reg++;
                fprintf(ctx->out, "  %%%d = call %s %s(", r, ret_tip, fn_ref);
            }
            int arg_basla = 0;
            if (closure_env) {
                fprintf(ctx->out, "ptr %s", closure_env);
                arg_basla = 1;
            }
            for (int i = 0; i < n; i++) {
                if (i > 0 || arg_basla) fputs(", ", ctx->out);
                fprintf(ctx->out, "%s %%%d", arg_tip[i], arg_reg[i]);
            }
            fputs(")\n", ctx->out);
            free(arg_reg);
            free(arg_tip);
            if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "%s", ret_tip);
            /* Void cagri icin yer tutucu olarak 0 done — daha sonra
             * kullanilirsa zaten yanlis kullanim sayilir */
            if (is_void) {
                int z = ctx->reg++;
                fprintf(ctx->out, "  %%%d = add i32 0, 0\n", z);
                return z;
            }
            return r;
        }

        case DUGUM_ERISIM: {
            /* x.alan -> GEP + load */
            int yer = ifade_yeri_uret(ctx, d, out_tip);
            if (yer < 0) return -1;
            const char *tip = out_tip ? out_tip : "i32";
            int r = ctx->reg++;
            fprintf(ctx->out, "  %%%d = load %s, ptr %%%d\n",
                    r, tip, yer);
            return r;
        }

        case DUGUM_DIZI_OLUSTUR: {
            int n = d->veri.dizi_olustur.sayi;

            /* Eleman tipi: ya context'ten ya da ilk elemandan */
            char el_tip[LLVM_TIP_BUF];
            if (ctx->beklenen_eleman_tip[0]) {
                snprintf(el_tip, sizeof(el_tip), "%s",
                         ctx->beklenen_eleman_tip);
            } else if (n > 0) {
                /* On evaluation icin ilk elemani kontrol amacli al */
                strcpy(el_tip, "i32");  /* default */
            } else {
                strcpy(el_tip, "i32");
            }

            /* Bos dizi: null ptr, len 0 */
            if (n == 0) {
                int slice_addr = ctx->reg++;
                fprintf(ctx->out, "  %%%d = alloca { ptr, i64 }\n", slice_addr);
                int gp = ctx->reg++;
                fprintf(ctx->out,
                    "  %%%d = getelementptr inbounds { ptr, i64 }, ptr %%%d, i32 0, i32 0\n",
                    gp, slice_addr);
                fprintf(ctx->out, "  store ptr null, ptr %%%d\n", gp);
                int gl = ctx->reg++;
                fprintf(ctx->out,
                    "  %%%d = getelementptr inbounds { ptr, i64 }, ptr %%%d, i32 0, i32 1\n",
                    gl, slice_addr);
                fprintf(ctx->out, "  store i64 0, ptr %%%d\n", gl);
                int v = ctx->reg++;
                fprintf(ctx->out,
                    "  %%%d = load { ptr, i64 }, ptr %%%d\n", v, slice_addr);
                if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "{ ptr, i64 }");
                return v;
            }

            /* Dolu dizi */
            int *elems = (int *)malloc(sizeof(int) * (size_t)n);
            char first_tip[LLVM_TIP_BUF];
            for (int i = 0; i < n; i++) {
                char et[LLVM_TIP_BUF];
                elems[i] = ifade_uret(ctx, d->veri.dizi_olustur.elemanlar[i],
                                       (i == 0 && !ctx->beklenen_eleman_tip[0])
                                       ? NULL : el_tip, et);
                if (i == 0) snprintf(first_tip, sizeof(first_tip), "%s", et);
            }
            /* Eger context yoksa ilk elemandan al */
            if (!ctx->beklenen_eleman_tip[0]) {
                snprintf(el_tip, sizeof(el_tip), "%s", first_tip);
            }

            /* Array alloca + store */
            int arr = ctx->reg++;
            fprintf(ctx->out, "  %%%d = alloca [%d x %s]\n", arr, n, el_tip);
            for (int i = 0; i < n; i++) {
                int gep = ctx->reg++;
                fprintf(ctx->out,
                    "  %%%d = getelementptr inbounds [%d x %s], ptr %%%d, i32 0, i32 %d\n",
                    gep, n, el_tip, arr, i);
                fprintf(ctx->out, "  store %s %%%d, ptr %%%d\n",
                        el_tip, elems[i], gep);
            }
            free(elems);

            /* Slice {ptr, i64} olustur */
            int slice_addr = ctx->reg++;
            fprintf(ctx->out, "  %%%d = alloca { ptr, i64 }\n", slice_addr);
            int gp = ctx->reg++;
            fprintf(ctx->out,
                "  %%%d = getelementptr inbounds { ptr, i64 }, ptr %%%d, i32 0, i32 0\n",
                gp, slice_addr);
            fprintf(ctx->out, "  store ptr %%%d, ptr %%%d\n", arr, gp);
            int gl = ctx->reg++;
            fprintf(ctx->out,
                "  %%%d = getelementptr inbounds { ptr, i64 }, ptr %%%d, i32 0, i32 1\n",
                gl, slice_addr);
            fprintf(ctx->out, "  store i64 %d, ptr %%%d\n", n, gl);
            int v = ctx->reg++;
            fprintf(ctx->out,
                "  %%%d = load { ptr, i64 }, ptr %%%d\n", v, slice_addr);
            if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "{ ptr, i64 }");
            return v;
        }

        case DUGUM_INDEKS: {
            /* xs[i] -> load eleman */
            char eleman_tip[LLVM_TIP_BUF];
            strcpy(eleman_tip, "i32");
            if (d->veri.indeks.nesne->tip == DUGUM_TANIMLAYICI) {
                const LLVMSem *s = sem_bul(ctx,
                    d->veri.indeks.nesne->veri.tanimlayici.metin,
                    d->veri.indeks.nesne->veri.tanimlayici.uzunluk);
                if (s && s->eleman_tip[0]) {
                    snprintf(eleman_tip, sizeof(eleman_tip),
                             "%s", s->eleman_tip);
                }
            }
            char nt[LLVM_TIP_BUF];
            int slice = ifade_uret(ctx, d->veri.indeks.nesne, NULL, nt);
            int idx = ifade_uret(ctx, d->veri.indeks.indeks, "i64", NULL);
            if (slice < 0 || idx < 0) return -1;
            int p = ctx->reg++;
            fprintf(ctx->out,
                "  %%%d = extractvalue { ptr, i64 } %%%d, 0\n", p, slice);
            int gep = ctx->reg++;
            fprintf(ctx->out,
                "  %%%d = getelementptr inbounds %s, ptr %%%d, i64 %%%d\n",
                gep, eleman_tip, p, idx);
            int r = ctx->reg++;
            fprintf(ctx->out, "  %%%d = load %s, ptr %%%d\n",
                    r, eleman_tip, gep);
            if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "%s", eleman_tip);
            return r;
        }

        case DUGUM_YAPI_OLUSTUR: {
            /* alloca yapi -> her alani GEP + store -> load tum */
            const char *yapi_ad = d->veri.yapi_olustur.tip_ad;
            int yapi_ad_uz = d->veri.yapi_olustur.tip_ad_uzunluk;
            const Dugum *y = yapi_bul(ctx, yapi_ad, yapi_ad_uz);
            if (!y) {
                fprintf(ctx->out, "  ; yapi tanimsiz: %.*s\n",
                        yapi_ad_uz, yapi_ad);
                int r = ctx->reg++;
                fprintf(ctx->out, "  %%%d = add i32 0, 0\n", r);
                return r;
            }

            int yer = ctx->reg++;
            fprintf(ctx->out, "  %%%d = alloca %%struct.%.*s\n",
                    yer, yapi_ad_uz, yapi_ad);

            /* Her alan icin GEP + store */
            for (int i = 0; i < d->veri.yapi_olustur.alan_sayi; i++) {
                const Dugum *aa = d->veri.yapi_olustur.alanlar[i];
                /* alani yapi tanimda bul (sira icin) */
                int idx = -1;
                char alan_tip[LLVM_TIP_BUF];
                strcpy(alan_tip, "i32");
                for (int j = 0; j < y->veri.yapi.alan_sayi; j++) {
                    const Dugum *al = y->veri.yapi.alanlar[j];
                    if (al->veri.alan.ad_uzunluk == aa->veri.alan_atama.ad_uzunluk &&
                        memcmp(al->veri.alan.ad, aa->veri.alan_atama.ad,
                               (size_t)al->veri.alan.ad_uzunluk) == 0) {
                        idx = j;
                        llvm_tip_str(al->veri.alan.tip, alan_tip,
                                     sizeof(alan_tip));
                        break;
                    }
                }
                if (idx < 0) continue;
                /* Deger hesapla, sonra store */
                char vt[LLVM_TIP_BUF];
                int vr = ifade_uret(ctx, aa->veri.alan_atama.deger,
                                     alan_tip, vt);
                int gep = ctx->reg++;
                fprintf(ctx->out,
                    "  %%%d = getelementptr inbounds %%struct.%.*s, ptr %%%d, i32 0, i32 %d\n",
                    gep, yapi_ad_uz, yapi_ad, yer, idx);
                fprintf(ctx->out, "  store %s %%%d, ptr %%%d\n",
                        alan_tip, vr, gep);
            }

            /* Yapi degerini load et (pass by value) */
            int r = ctx->reg++;
            fprintf(ctx->out, "  %%%d = load %%struct.%.*s, ptr %%%d\n",
                    r, yapi_ad_uz, yapi_ad, yer);
            if (out_tip) {
                snprintf(out_tip, LLVM_TIP_BUF, "%%struct.%.*s",
                         yapi_ad_uz, yapi_ad);
            }
            return r;
        }

        default: {
            fprintf(ctx->out, "  ; ifade tipi %d desteklenmiyor\n", d->tip);
            int r = ctx->reg++;
            fprintf(ctx->out, "  %%%d = add i32 0, 0\n", r);
            if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "i32");
            return r;
        }
    }
}

/* === Ifade yeri (lvalue): yer pointer'i don === */

static int ifade_yeri_uret(LLVMCtx *ctx, const Dugum *d, char *out_tip) {
    if (!d) return -1;
    switch (d->tip) {
        case DUGUM_TANIMLAYICI: {
            const LLVMSem *s = sem_bul(ctx,
                d->veri.tanimlayici.metin,
                d->veri.tanimlayici.uzunluk);
            if (!s) return -1;
            /* Yer = alloca pointer. Bunu reg olarak don. Stringi alip yeni
             * registera kopya yapariz (LLVM IR allows refer by name). */
            int r = ctx->reg++;
            fprintf(ctx->out,
                "  %%%d = getelementptr %s, ptr %s, i32 0\n",
                r, s->llvm_tip, s->yer_ssa);
            if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "%s", s->llvm_tip);
            return r;
        }

        case DUGUM_ERISIM: {
            /* x.alan: x'in yerini al, GEP ile alan offset */
            char nesne_tip[LLVM_TIP_BUF];
            int nesne_yer = ifade_yeri_uret(ctx,
                d->veri.erisim.nesne, nesne_tip);
            if (nesne_yer < 0) {
                /* Yer cikarsanamiyorsa, ifade gibi al + alloca yap */
                int v = ifade_uret(ctx, d->veri.erisim.nesne,
                                    NULL, nesne_tip);
                if (v < 0) return -1;
                int tmp = ctx->reg++;
                fprintf(ctx->out, "  %%%d = alloca %s\n", tmp, nesne_tip);
                fprintf(ctx->out, "  store %s %%%d, ptr %%%d\n",
                        nesne_tip, v, tmp);
                nesne_yer = tmp;
            }
            /* nesne_tip "%struct.X" formunda olmali */
            const char *p = nesne_tip;
            if (p[0] != '%') {
                fprintf(ctx->out, "  ; erisim: yapi tipi degil: %s\n", p);
                return -1;
            }
            /* "%struct.X" -> "X" cikar */
            const char *yapi_ad = p + strlen("%struct.");
            int yapi_ad_uz = (int)strlen(yapi_ad);
            const Dugum *y = yapi_bul(ctx, yapi_ad, yapi_ad_uz);
            if (!y) {
                fprintf(ctx->out, "  ; erisim: yapi bulunamadi: %s\n", yapi_ad);
                return -1;
            }
            int idx = -1;
            char alan_tip[LLVM_TIP_BUF];
            strcpy(alan_tip, "i32");
            for (int j = 0; j < y->veri.yapi.alan_sayi; j++) {
                const Dugum *al = y->veri.yapi.alanlar[j];
                if (al->veri.alan.ad_uzunluk == d->veri.erisim.alan_uzunluk &&
                    memcmp(al->veri.alan.ad, d->veri.erisim.alan,
                           (size_t)al->veri.alan.ad_uzunluk) == 0) {
                    idx = j;
                    llvm_tip_str(al->veri.alan.tip, alan_tip,
                                 sizeof(alan_tip));
                    break;
                }
            }
            if (idx < 0) {
                fprintf(ctx->out, "  ; erisim: alan bulunamadi\n");
                return -1;
            }
            int r = ctx->reg++;
            fprintf(ctx->out,
                "  %%%d = getelementptr inbounds %s, ptr %%%d, i32 0, i32 %d\n",
                r, p, nesne_yer, idx);
            if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "%s", alan_tip);
            return r;
        }

        case DUGUM_TEKLI: {
            if (d->veri.tekli.op == OP_DEREFERANS) {
                /* *p — p degeri zaten pointer */
                char pt[LLVM_TIP_BUF];
                int pv = ifade_uret(ctx, d->veri.tekli.operand,
                                     "ptr", pt);
                if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "i32");
                return pv;
            }
            return -1;
        }

        case DUGUM_INDEKS: {
            /* xs[i] yeri -> GEP pointer don */
            char eleman_tip[LLVM_TIP_BUF];
            strcpy(eleman_tip, "i32");
            if (d->veri.indeks.nesne->tip == DUGUM_TANIMLAYICI) {
                const LLVMSem *s = sem_bul(ctx,
                    d->veri.indeks.nesne->veri.tanimlayici.metin,
                    d->veri.indeks.nesne->veri.tanimlayici.uzunluk);
                if (s && s->eleman_tip[0]) {
                    snprintf(eleman_tip, sizeof(eleman_tip),
                             "%s", s->eleman_tip);
                }
            }
            char nt[LLVM_TIP_BUF];
            int slice = ifade_uret(ctx, d->veri.indeks.nesne, NULL, nt);
            int idx = ifade_uret(ctx, d->veri.indeks.indeks, "i64", NULL);
            if (slice < 0 || idx < 0) return -1;
            int p = ctx->reg++;
            fprintf(ctx->out,
                "  %%%d = extractvalue { ptr, i64 } %%%d, 0\n", p, slice);
            int gep = ctx->reg++;
            fprintf(ctx->out,
                "  %%%d = getelementptr inbounds %s, ptr %%%d, i64 %%%d\n",
                gep, eleman_tip, p, idx);
            if (out_tip) snprintf(out_tip, LLVM_TIP_BUF, "%s", eleman_tip);
            return gep;
        }

        default:
            return -1;
    }
}

/* === Deyim emit === */

static void deyim_uret(LLVMCtx *ctx, const Dugum *d) {
    if (!d || ctx->blok_terminated) return;

    switch (d->tip) {
        case DUGUM_VER: {
            const char *rt = ctx->aktif_donus_tip[0]
                             ? ctx->aktif_donus_tip : "i32";
            if (strcmp(rt, "void") == 0) {
                fputs("  ret void\n", ctx->out);
            } else if (d->veri.ver.deger) {
                char ot[LLVM_TIP_BUF];
                int r = ifade_uret(ctx, d->veri.ver.deger, rt, ot);
                if (r >= 0) {
                    fprintf(ctx->out, "  ret %s %%%d\n", rt, r);
                } else {
                    fprintf(ctx->out, "  ret %s 0\n", rt);
                }
            } else {
                fputs("  ret void\n", ctx->out);
            }
            ctx->blok_terminated = 1;
            return;
        }

        case DUGUM_DEGISKEN: {
            const Dugum *deger = d->veri.degisken.deger;

            /* Lambda atamasi -> lift + capture analizi */
            if (deger && deger->tip == DUGUM_LAMBDA) {
                int no = lambda_lift(ctx, deger);
                LambdaListe *ll = lambda_no_bul(ctx, no);

                if (ll && ll->capture_say > 0) {
                    /* Captures var -> closure: env struct'i caller'da olustur */
                    int env = ctx->reg++;
                    /* Struct tipi: { tip1, tip2, ... } */
                    fprintf(ctx->out, "  %%%d = alloca { ", env);
                    int idx = 0;
                    for (CaptureItem *c = ll->captures; c; c = c->sonraki) {
                        if (idx > 0) fputs(", ", ctx->out);
                        fputs(c->llvm_tip, ctx->out);
                        idx++;
                    }
                    fputs(" }\n", ctx->out);

                    /* Her capture icin: oncekini load + env GEP + store */
                    idx = 0;
                    for (CaptureItem *c = ll->captures; c; c = c->sonraki) {
                        int v = ctx->reg++;
                        fprintf(ctx->out,
                            "  %%%d = load %s, ptr %s\n",
                            v, c->llvm_tip, c->yer_ssa);
                        int gep = ctx->reg++;
                        fprintf(ctx->out,
                            "  %%%d = getelementptr inbounds { ", gep);
                        int idx2 = 0;
                        for (CaptureItem *c2 = ll->captures;
                             c2; c2 = c2->sonraki) {
                            if (idx2 > 0) fputs(", ", ctx->out);
                            fputs(c2->llvm_tip, ctx->out);
                            idx2++;
                        }
                        fprintf(ctx->out,
                            " }, ptr %%%d, i32 0, i32 %d\n", env, idx);
                        fprintf(ctx->out,
                            "  store %s %%%d, ptr %%%d\n",
                            c->llvm_tip, v, gep);
                        idx++;
                    }

                    char ssa[64];
                    snprintf(ssa, sizeof(ssa), "%%%d", env);
                    sem_ekle(ctx, d->veri.degisken.ad,
                             d->veri.degisken.ad_uzunluk, ssa, "ptr", 2);
                    sem_lambda_no_ayarla(ctx, d->veri.degisken.ad,
                                          d->veri.degisken.ad_uzunluk, no);
                } else {
                    /* Non-capturing lambda */
                    char ssa[64];
                    snprintf(ssa, sizeof(ssa), "@__lambda_%d", no);
                    sem_ekle(ctx, d->veri.degisken.ad,
                             d->veri.degisken.ad_uzunluk, ssa, "ptr", 1);
                }
                return;
            }

            /* Tip: annot varsa kullan, yoksa default i32 */
            char tip[LLVM_TIP_BUF];
            char el_tip[LLVM_TIP_BUF] = "";
            if (d->veri.degisken.tip) {
                llvm_tip_str(d->veri.degisken.tip, tip, sizeof(tip));
                /* Dizi<T> annotation ise eleman tipini cikar */
                if (d->veri.degisken.tip->tip == DUGUM_TIP_DIZI) {
                    llvm_tip_str(d->veri.degisken.tip->veri.tip_dizi.eleman_tip,
                                 el_tip, sizeof(el_tip));
                }
            } else {
                /* Degerin tipinden cikar — basit */
                snprintf(tip, sizeof(tip), "i32");
            }

            /* Dizi_olustur context icin eleman tipini ctx'e yaz */
            if (el_tip[0]) {
                snprintf(ctx->beklenen_eleman_tip,
                         sizeof(ctx->beklenen_eleman_tip), "%s", el_tip);
            }

            /* Adres registeri: bir SSA reg */
            int addr_reg = ctx->reg++;
            fprintf(ctx->out, "  %%%d = alloca %s\n", addr_reg, tip);

            char ssa[32];
            snprintf(ssa, sizeof(ssa), "%%%d", addr_reg);
            sem_ekle(ctx, d->veri.degisken.ad,
                     d->veri.degisken.ad_uzunluk, ssa, tip, 0);
            if (el_tip[0]) {
                sem_eleman_tip_ayarla(ctx, d->veri.degisken.ad,
                                       d->veri.degisken.ad_uzunluk, el_tip);
            }

            /* Deger varsa store */
            if (deger) {
                char vt[LLVM_TIP_BUF];
                int r = ifade_uret(ctx, deger, tip, vt);
                if (r >= 0) {
                    fprintf(ctx->out, "  store %s %%%d, ptr %%%d\n",
                            tip, r, addr_reg);
                }
            }
            /* Context'i temizle */
            ctx->beklenen_eleman_tip[0] = '\0';
            return;
        }

        case DUGUM_ATAMA: {
            /* Hedefin yerini al, degeri hesapla, store */
            char hedef_tip[LLVM_TIP_BUF];
            int yer = ifade_yeri_uret(ctx, d->veri.atama.hedef, hedef_tip);
            if (yer < 0) {
                fputs("  ; atama hedefi yer alinamadi\n", ctx->out);
                return;
            }
            char dt[LLVM_TIP_BUF];
            int v = ifade_uret(ctx, d->veri.atama.deger, hedef_tip, dt);
            if (v < 0) return;
            fprintf(ctx->out, "  store %s %%%d, ptr %%%d\n",
                    hedef_tip, v, yer);
            return;
        }

        case DUGUM_EGER: {
            /* eger COND { then } degilse { else } */
            int n = yeni_etiket(ctx);
            char then_lbl[32], else_lbl[32], end_lbl[32];
            snprintf(then_lbl, sizeof(then_lbl), "if.then.%d", n);
            snprintf(else_lbl, sizeof(else_lbl), "if.else.%d", n);
            snprintf(end_lbl,  sizeof(end_lbl),  "if.end.%d",  n);
            int has_else = d->veri.eger.yan != NULL;

            char ct[LLVM_TIP_BUF];
            int cond = ifade_uret(ctx, d->veri.eger.kosul, "i1", ct);
            if (cond < 0) return;
            /* Koşul i1 değilse, icmp ne 0 ile çevir */
            if (strcmp(ct, "i1") != 0) {
                int z = ctx->reg++;
                fprintf(ctx->out, "  %%%d = icmp ne %s %%%d, 0\n",
                        z, ct, cond);
                cond = z;
            }
            emit_br_cond(ctx, cond, then_lbl,
                          has_else ? else_lbl : end_lbl);

            /* then block */
            emit_etiket(ctx, then_lbl);
            int eski_sem = ctx->sem_say;
            deyim_uret(ctx, d->veri.eger.gozdoldur);
            ctx->sem_say = eski_sem;
            emit_br(ctx, end_lbl);

            /* else block (varsa) */
            if (has_else) {
                emit_etiket(ctx, else_lbl);
                eski_sem = ctx->sem_say;
                deyim_uret(ctx, d->veri.eger.yan);
                ctx->sem_say = eski_sem;
                emit_br(ctx, end_lbl);
            }

            /* end block */
            emit_etiket(ctx, end_lbl);
            return;
        }

        case DUGUM_IKEN: {
            /* iken COND { body } */
            int n = yeni_etiket(ctx);
            char hdr_lbl[32], body_lbl[32], end_lbl[32];
            snprintf(hdr_lbl,  sizeof(hdr_lbl),  "while.cond.%d", n);
            snprintf(body_lbl, sizeof(body_lbl), "while.body.%d", n);
            snprintf(end_lbl,  sizeof(end_lbl),  "while.end.%d",  n);

            emit_br(ctx, hdr_lbl);
            emit_etiket(ctx, hdr_lbl);

            char ct[LLVM_TIP_BUF];
            int cond = ifade_uret(ctx, d->veri.iken.kosul, "i1", ct);
            if (cond < 0) return;
            if (strcmp(ct, "i1") != 0) {
                int z = ctx->reg++;
                fprintf(ctx->out, "  %%%d = icmp ne %s %%%d, 0\n",
                        z, ct, cond);
                cond = z;
            }
            emit_br_cond(ctx, cond, body_lbl, end_lbl);

            emit_etiket(ctx, body_lbl);
            int eski_sem = ctx->sem_say;
            deyim_uret(ctx, d->veri.iken.govde);
            ctx->sem_say = eski_sem;
            emit_br(ctx, hdr_lbl);

            emit_etiket(ctx, end_lbl);
            return;
        }

        case DUGUM_ICIN: {
            /* icin x: koleksiyon { body } — Dizi<T> iterasyonu */
            char el_tip[LLVM_TIP_BUF];
            strcpy(el_tip, "i32");
            if (d->veri.icin.koleksiyon->tip == DUGUM_TANIMLAYICI) {
                const LLVMSem *s = sem_bul(ctx,
                    d->veri.icin.koleksiyon->veri.tanimlayici.metin,
                    d->veri.icin.koleksiyon->veri.tanimlayici.uzunluk);
                if (s && s->eleman_tip[0]) {
                    snprintf(el_tip, sizeof(el_tip), "%s", s->eleman_tip);
                }
            }

            char ot[LLVM_TIP_BUF];
            int kol = ifade_uret(ctx, d->veri.icin.koleksiyon, NULL, ot);
            if (kol < 0) return;

            int kol_ptr = ctx->reg++;
            fprintf(ctx->out,
                "  %%%d = extractvalue { ptr, i64 } %%%d, 0\n",
                kol_ptr, kol);
            int kol_len = ctx->reg++;
            fprintf(ctx->out,
                "  %%%d = extractvalue { ptr, i64 } %%%d, 1\n",
                kol_len, kol);

            /* i counter */
            int i_addr = ctx->reg++;
            fprintf(ctx->out, "  %%%d = alloca i64\n", i_addr);
            fprintf(ctx->out, "  store i64 0, ptr %%%d\n", i_addr);

            /* x = current eleman */
            int x_addr = ctx->reg++;
            fprintf(ctx->out, "  %%%d = alloca %s\n", x_addr, el_tip);

            int eski_sem = ctx->sem_say;
            char x_ssa[32];
            snprintf(x_ssa, sizeof(x_ssa), "%%%d", x_addr);
            sem_ekle(ctx, d->veri.icin.degisken_adi,
                     d->veri.icin.degisken_adi_uzunluk, x_ssa, el_tip, 0);

            int n = yeni_etiket(ctx);
            char hdr[32], body[32], end[32];
            snprintf(hdr,  sizeof(hdr),  "for.cond.%d", n);
            snprintf(body, sizeof(body), "for.body.%d", n);
            snprintf(end,  sizeof(end),  "for.end.%d",  n);

            emit_br(ctx, hdr);
            emit_etiket(ctx, hdr);

            int i_v = ctx->reg++;
            fprintf(ctx->out, "  %%%d = load i64, ptr %%%d\n", i_v, i_addr);
            int c = ctx->reg++;
            fprintf(ctx->out, "  %%%d = icmp slt i64 %%%d, %%%d\n",
                    c, i_v, kol_len);
            emit_br_cond(ctx, c, body, end);

            emit_etiket(ctx, body);
            int ep = ctx->reg++;
            fprintf(ctx->out,
                "  %%%d = getelementptr inbounds %s, ptr %%%d, i64 %%%d\n",
                ep, el_tip, kol_ptr, i_v);
            int ev = ctx->reg++;
            fprintf(ctx->out, "  %%%d = load %s, ptr %%%d\n",
                    ev, el_tip, ep);
            fprintf(ctx->out, "  store %s %%%d, ptr %%%d\n",
                    el_tip, ev, x_addr);

            deyim_uret(ctx, d->veri.icin.govde);
            if (!ctx->blok_terminated) {
                int i_v2 = ctx->reg++;
                fprintf(ctx->out, "  %%%d = load i64, ptr %%%d\n",
                        i_v2, i_addr);
                int i_n = ctx->reg++;
                fprintf(ctx->out, "  %%%d = add i64 %%%d, 1\n", i_n, i_v2);
                fprintf(ctx->out, "  store i64 %%%d, ptr %%%d\n",
                        i_n, i_addr);
                emit_br(ctx, hdr);
            }

            ctx->sem_say = eski_sem;
            emit_etiket(ctx, end);
            return;
        }

        case DUGUM_ESLES: {
            /* esles deger { desen1 => kol1; desen2 => kol2; ... } —
             * literal desen tabanli if-else zinciri olarak desuger.
             * Joker desen son catch-all olarak. */
            char dt[LLVM_TIP_BUF];
            int deger = ifade_uret(ctx, d->veri.esles.deger, NULL, dt);
            if (deger < 0) return;

            int n_kol = d->veri.esles.kol_sayi;
            int n = yeni_etiket(ctx);
            char end_lbl[32];
            snprintf(end_lbl, sizeof(end_lbl), "match.end.%d", n);

            for (int i = 0; i < n_kol; i++) {
                const Dugum *kol = d->veri.esles.kollar[i];
                const Dugum *desen = kol->veri.esles_kolu.desen;
                char arm_lbl[32], next_lbl[32];
                snprintf(arm_lbl,  sizeof(arm_lbl),  "match.arm.%d.%d", n, i);
                snprintf(next_lbl, sizeof(next_lbl), "match.next.%d.%d", n, i);

                if (desen->tip == DUGUM_DESEN_LITERAL) {
                    /* deger == literal mi? */
                    char lt[LLVM_TIP_BUF];
                    int lit = ifade_uret(ctx, desen->veri.desen_literal.deger,
                                          dt, lt);
                    int cmp = ctx->reg++;
                    fprintf(ctx->out,
                        "  %%%d = icmp eq %s %%%d, %%%d\n",
                        cmp, dt, deger, lit);
                    emit_br_cond(ctx, cmp, arm_lbl, next_lbl);
                } else if (desen->tip == DUGUM_DESEN_JOKER ||
                           desen->tip == DUGUM_DESEN_TANIMLAYICI) {
                    /* Her zaman match — direkt arm'a git */
                    emit_br(ctx, arm_lbl);
                } else {
                    /* Diger desenler (yapici vs.) — simdilik atla */
                    fprintf(ctx->out, "  ; esles deseni desteklenmiyor: %d\n",
                            desen->tip);
                    emit_br(ctx, next_lbl);
                }

                emit_etiket(ctx, arm_lbl);
                /* Tanimlayici desen ise binding: x = deger */
                int eski_sem = ctx->sem_say;
                if (desen->tip == DUGUM_DESEN_TANIMLAYICI) {
                    int b_addr = ctx->reg++;
                    fprintf(ctx->out, "  %%%d = alloca %s\n", b_addr, dt);
                    fprintf(ctx->out, "  store %s %%%d, ptr %%%d\n",
                            dt, deger, b_addr);
                    char b_ssa[32];
                    snprintf(b_ssa, sizeof(b_ssa), "%%%d", b_addr);
                    sem_ekle(ctx,
                             desen->veri.desen_tanimlayici.ad,
                             desen->veri.desen_tanimlayici.ad_uzunluk,
                             b_ssa, dt, 0);
                }
                deyim_uret(ctx, kol->veri.esles_kolu.govde);
                ctx->sem_say = eski_sem;
                emit_br(ctx, end_lbl);

                emit_etiket(ctx, next_lbl);
            }
            /* Tum eslesmeler basarisiz olursa end'e dus */
            emit_br(ctx, end_lbl);
            emit_etiket(ctx, end_lbl);
            return;
        }

        case DUGUM_BLOK: {
            int eski_sem = ctx->sem_say;
            for (int i = 0; i < d->veri.blok.sayi; i++) {
                if (ctx->blok_terminated) break;
                deyim_uret(ctx, d->veri.blok.deyimler[i]);
            }
            ctx->sem_say = eski_sem;
            return;
        }

        case DUGUM_IFADE_DEYIMI:
            ifade_uret(ctx, d->veri.ifade_deyimi.ifade, NULL, NULL);
            return;

        case DUGUM_GUVENSIZ:
            if (d->veri.guvensiz.blok) {
                deyim_uret(ctx, d->veri.guvensiz.blok);
            }
            return;

        default:
            fprintf(ctx->out, "  ; deyim tipi %d atlandi\n", d->tip);
            return;
    }
}

/* govde_ve_param_uret artik kullanilmiyor — islev_uret inline ediyor,
 * lambda_uret_full kendi kopyasini kullaniyor. */

/* === Üst düzey islev === */

static void islev_uret(LLVMCtx *ctx, const Dugum *islev) {
    char rt[LLVM_TIP_BUF];
    if (islev->veri.islev.donus_tipi) {
        llvm_tip_str(islev->veri.islev.donus_tipi, rt, sizeof(rt));
    } else {
        snprintf(rt, sizeof(rt), "void");
    }

    /* Calling convention: kesme -> x86_intrcc */
    fprintf(ctx->out, "define ");
    if (islev->veri.islev.kesme_mi) {
        fputs("x86_intrcc ", ctx->out);
    }
    fprintf(ctx->out, "%s @", rt);
    fwrite(islev->veri.islev.ad, 1,
           (size_t)islev->veri.islev.ad_uzunluk, ctx->out);

    /* Suffix oznitelikler — naked + section function-baslik sonunda */
    int ifade_mi = islev->veri.islev.govde &&
                   islev->veri.islev.govde->tip != DUGUM_BLOK;

    /* govde_ve_param_uret parametre listesi + { yazar, ama section/naked
     * onlardan once gelmeli — manuel buraya yaz. */
    char donus_tip_str[LLVM_TIP_BUF];
    snprintf(donus_tip_str, sizeof(donus_tip_str), "%s", rt);
    snprintf(ctx->aktif_donus_tip, sizeof(ctx->aktif_donus_tip),
             "%s", donus_tip_str);

    fputc('(', ctx->out);
    int eski_sem = ctx->sem_say;
    int eski_reg = ctx->reg;
    int eski_etiket = ctx->etiket_no;
    ctx->reg = 0;
    ctx->etiket_no = 0;

    int n = islev->veri.islev.param_sayi;
    for (int i = 0; i < n; i++) {
        const Dugum *p = islev->veri.islev.parametreler[i];
        if (i > 0) fputs(", ", ctx->out);
        char pt[LLVM_TIP_BUF];
        llvm_tip_str(p->veri.parametre.tip, pt, sizeof(pt));
        fprintf(ctx->out, "%s %%.p%d", pt, i);
    }
    fputc(')', ctx->out);

    /* Sira: attribute group oncesi, section sonrasi (LLVM IR spec) */
    /* naked attribute group reference (function-attrs, section'dan once) */
    if (islev->veri.islev.ciplak_mi) {
        fputs(" #0", ctx->out);
        ctx->naked_kullanildi = 1;
    }

    /* Section ekle */
    if (islev->veri.islev.bolum) {
        fprintf(ctx->out, " section \"%.*s\"",
                islev->veri.islev.bolum_uzunluk,
                islev->veri.islev.bolum);
    }

    fputs(" {\nentry:\n", ctx->out);
    ctx->blok_terminated = 0;

    /* Parametre alloca + store */
    if (!islev->veri.islev.ciplak_mi) {
        /* Naked'da prologue yok */
        for (int i = 0; i < n; i++) {
            const Dugum *p = islev->veri.islev.parametreler[i];
            char pt[LLVM_TIP_BUF];
            llvm_tip_str(p->veri.parametre.tip, pt, sizeof(pt));
            int addr = ctx->reg++;
            fprintf(ctx->out, "  %%%d = alloca %s\n", addr, pt);
            fprintf(ctx->out, "  store %s %%.p%d, ptr %%%d\n",
                    pt, i, addr);
            char ssa[32];
            snprintf(ssa, sizeof(ssa), "%%%d", addr);
            sem_ekle(ctx, p->veri.parametre.ad,
                     p->veri.parametre.ad_uzunluk, ssa, pt, 0);
        }
    }

    /* Govde */
    if (islev->veri.islev.govde) {
        if (ifade_mi) {
            char ot[LLVM_TIP_BUF];
            int r = ifade_uret(ctx, islev->veri.islev.govde,
                                donus_tip_str, ot);
            if (!ctx->blok_terminated) {
                if (r >= 0) {
                    fprintf(ctx->out, "  ret %s %%%d\n", donus_tip_str, r);
                } else if (strcmp(donus_tip_str, "void") == 0) {
                    fputs("  ret void\n", ctx->out);
                } else {
                    fprintf(ctx->out, "  ret %s 0\n", donus_tip_str);
                }
                ctx->blok_terminated = 1;
            }
        } else {
            deyim_uret(ctx, islev->veri.islev.govde);
        }
    }

    if (!ctx->blok_terminated) {
        /* Naked icin terminator ekleme — kullanici asm icinde yapar */
        if (islev->veri.islev.ciplak_mi) {
            fputs("  unreachable\n", ctx->out);
        } else if (strcmp(donus_tip_str, "void") == 0) {
            fputs("  ret void\n", ctx->out);
        } else {
            fprintf(ctx->out, "  ret %s 0\n", donus_tip_str);
        }
    }
    fputs("}\n\n", ctx->out);

    ctx->reg = eski_reg;
    ctx->etiket_no = eski_etiket;
    ctx->sem_say = eski_sem;
    (void)ifade_mi;
}

static void lambda_uret_full(LLVMCtx *ctx, LambdaListe *ll) {
    const Dugum *lambda = ll->lambda;
    int no = ll->no;
    int captures = ll->capture_say > 0;

    fprintf(ctx->out, "define i32 @__lambda_%d(", no);

    int eski_sem = ctx->sem_say;
    int eski_reg = ctx->reg;
    int eski_etiket = ctx->etiket_no;
    ctx->reg = 0;
    ctx->etiket_no = 0;
    snprintf(ctx->aktif_donus_tip, sizeof(ctx->aktif_donus_tip), "i32");

    int param_n = lambda->veri.lambda.param_sayi;

    /* Closure: ilk param env ptr */
    if (captures) {
        fputs("ptr %.env", ctx->out);
    }
    for (int i = 0; i < param_n; i++) {
        const Dugum *p = lambda->veri.lambda.parametreler[i];
        if (i > 0 || captures) fputs(", ", ctx->out);
        char pt[LLVM_TIP_BUF];
        llvm_tip_str(p->veri.parametre.tip, pt, sizeof(pt));
        fprintf(ctx->out, "%s %%.p%d", pt, i);
    }
    fputs(") {\nentry:\n", ctx->out);
    ctx->blok_terminated = 0;

    /* Captures: env'den her birini local alloca'ya yukle */
    if (captures) {
        int idx = 0;
        for (CaptureItem *c = ll->captures; c; c = c->sonraki) {
            int gep = ctx->reg++;
            fprintf(ctx->out,
                "  %%%d = getelementptr inbounds { ", gep);
            int idx2 = 0;
            for (CaptureItem *c2 = ll->captures; c2; c2 = c2->sonraki) {
                if (idx2 > 0) fputs(", ", ctx->out);
                fputs(c2->llvm_tip, ctx->out);
                idx2++;
            }
            fprintf(ctx->out,
                " }, ptr %%.env, i32 0, i32 %d\n", idx);
            int val = ctx->reg++;
            fprintf(ctx->out, "  %%%d = load %s, ptr %%%d\n",
                    val, c->llvm_tip, gep);
            /* Yerel alloca + store — boyle ki normal TANIMLAYICI lookup
             * load yapar */
            int addr = ctx->reg++;
            fprintf(ctx->out, "  %%%d = alloca %s\n", addr, c->llvm_tip);
            fprintf(ctx->out, "  store %s %%%d, ptr %%%d\n",
                    c->llvm_tip, val, addr);
            char ssa[32];
            snprintf(ssa, sizeof(ssa), "%%%d", addr);
            sem_ekle(ctx, c->ad, c->ad_uz, ssa, c->llvm_tip, 0);
            idx++;
        }
    }

    /* Parametre alloca + store */
    for (int i = 0; i < param_n; i++) {
        const Dugum *p = lambda->veri.lambda.parametreler[i];
        char pt[LLVM_TIP_BUF];
        llvm_tip_str(p->veri.parametre.tip, pt, sizeof(pt));
        int addr = ctx->reg++;
        fprintf(ctx->out, "  %%%d = alloca %s\n", addr, pt);
        fprintf(ctx->out, "  store %s %%.p%d, ptr %%%d\n", pt, i, addr);
        char ssa[32];
        snprintf(ssa, sizeof(ssa), "%%%d", addr);
        sem_ekle(ctx, p->veri.parametre.ad,
                 p->veri.parametre.ad_uzunluk, ssa, pt, 0);
    }

    /* Govde */
    int ifade_mi = lambda->veri.lambda.govde &&
                   lambda->veri.lambda.govde->tip != DUGUM_BLOK;
    if (lambda->veri.lambda.govde) {
        if (ifade_mi) {
            char ot[LLVM_TIP_BUF];
            int r = ifade_uret(ctx, lambda->veri.lambda.govde, "i32", ot);
            if (!ctx->blok_terminated && r >= 0) {
                fprintf(ctx->out, "  ret i32 %%%d\n", r);
                ctx->blok_terminated = 1;
            }
        } else {
            deyim_uret(ctx, lambda->veri.lambda.govde);
        }
    }
    if (!ctx->blok_terminated) {
        fputs("  ret i32 0\n", ctx->out);
    }
    fputs("}\n\n", ctx->out);

    ctx->reg = eski_reg;
    ctx->etiket_no = eski_etiket;
    ctx->sem_say = eski_sem;
}

/* Eski lambda_uret kaldirildi — lambda_uret_full doğrudan kullaniliyor */

/* === Public API === */

void llvm_ir_uret_target(const Dugum *program, FILE *out, const char *triple) {
    if (!out) return;
    if (!triple) triple = "x86_64-pc-windows-gnu";
    fputs("; KEMGU LLVM IR (alloca tabanli)\n", out);
    fputs("; clang -x ir - -o cikti.exe ile derlenebilir.\n", out);
    fprintf(out, "target triple = \"%s\"\n\n", triple);

    if (!program || program->tip != DUGUM_PROGRAM) {
        fputs("; (program AST'si yok)\n", out);
        return;
    }

    LLVMCtx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.out = out;

    /* 1. Gecis: yapi tanimlarini topla + emit et + yerel islev adlarini topla */
    for (int i = 0; i < program->veri.program.sayi; i++) {
        const Dugum *uye = program->veri.program.uyeler[i];
        const Dugum *g = (uye->tip == DUGUM_DISA && uye->veri.disa.tanim)
                         ? uye->veri.disa.tanim : uye;
        if (g->tip == DUGUM_YAPI) {
            yapi_kaydet(&ctx, g);
            yapi_tanim_uret(&ctx, g);
        } else if (g->tip == DUGUM_ISLEV) {
            char dt[LLVM_TIP_BUF];
            if (g->veri.islev.donus_tipi) {
                llvm_tip_str(g->veri.islev.donus_tipi, dt, sizeof(dt));
            } else {
                snprintf(dt, sizeof(dt), "void");
            }
            yerel_islev_kaydet(&ctx,
                g->veri.islev.ad, g->veri.islev.ad_uzunluk, dt);
        }
    }
    if (ctx.yapi_say > 0) fputc('\n', out);

    /* 1.5. Sabit (compile-time constant) tanimlari -> LLVM global constant */
    for (int i = 0; i < program->veri.program.sayi; i++) {
        const Dugum *uye = program->veri.program.uyeler[i];
        const Dugum *g = (uye->tip == DUGUM_DISA && uye->veri.disa.tanim)
                         ? uye->veri.disa.tanim : uye;
        if (g->tip != DUGUM_SABIT) continue;
        char tip[LLVM_TIP_BUF];
        llvm_tip_str(g->veri.sabit.tip, tip, sizeof(tip));
        const Dugum *dv = g->veri.sabit.deger;

        /* === Dizi sabiti -> .rodata raw bayt + slice === */
        if (dv && dv->tip == DUGUM_DIZI_OLUSTUR &&
            g->veri.sabit.tip &&
            g->veri.sabit.tip->tip == DUGUM_TIP_DIZI) {
            char el_tip[LLVM_TIP_BUF];
            llvm_tip_str(g->veri.sabit.tip->veri.tip_dizi.eleman_tip,
                         el_tip, sizeof(el_tip));
            int n = dv->veri.dizi_olustur.sayi;
            /* Alt veri array'i: @<ad>.data */
            fprintf(out, "@%.*s.data = private unnamed_addr constant [%d x %s] [",
                    g->veri.sabit.ad_uzunluk, g->veri.sabit.ad, n, el_tip);
            for (int k = 0; k < n; k++) {
                const Dugum *el = dv->veri.dizi_olustur.elemanlar[k];
                if (k > 0) fputs(", ", out);
                if (el->tip == DUGUM_TAM) {
                    fprintf(out, "%s %lld", el_tip,
                            (long long)el->veri.tam.deger);
                } else if (el->tip == DUGUM_MANTIKSAL) {
                    fprintf(out, "%s %d", el_tip,
                            el->veri.mantiksal.deger ? 1 : 0);
                } else if (el->tip == DUGUM_KESIRLI) {
                    fprintf(out, "%s %g", el_tip,
                            el->veri.kesirli.deger);
                } else {
                    fprintf(out, "%s 0", el_tip);
                }
            }
            fputs("]\n", out);
            /* Slice (ptr + len): @<ad> */
            fprintf(out,
                "@%.*s = constant { ptr, i64 } { ptr @%.*s.data, i64 %d }\n",
                g->veri.sabit.ad_uzunluk, g->veri.sabit.ad,
                g->veri.sabit.ad_uzunluk, g->veri.sabit.ad, n);
            /* Sembol tablosu: yer @<ad>, tip { ptr, i64 }, eleman tipi el_tip */
            char yer[64];
            snprintf(yer, sizeof(yer), "@%.*s",
                     g->veri.sabit.ad_uzunluk, g->veri.sabit.ad);
            sem_ekle(&ctx, g->veri.sabit.ad,
                     g->veri.sabit.ad_uzunluk, yer, "{ ptr, i64 }", 0);
            sem_eleman_tip_ayarla(&ctx,
                g->veri.sabit.ad, g->veri.sabit.ad_uzunluk, el_tip);
            continue;
        }

        /* === Skaler sabit === */
        char deger_str[64];
        deger_str[0] = '\0';
        if (dv && dv->tip == DUGUM_TAM) {
            snprintf(deger_str, sizeof(deger_str), "%lld",
                     (long long)dv->veri.tam.deger);
        } else if (dv && dv->tip == DUGUM_MANTIKSAL) {
            snprintf(deger_str, sizeof(deger_str), "%d",
                     dv->veri.mantiksal.deger ? 1 : 0);
        } else if (dv && dv->tip == DUGUM_KESIRLI) {
            snprintf(deger_str, sizeof(deger_str), "%g",
                     dv->veri.kesirli.deger);
        } else {
            snprintf(deger_str, sizeof(deger_str), "0");
        }
        fprintf(out, "@%.*s = constant %s %s\n",
                g->veri.sabit.ad_uzunluk, g->veri.sabit.ad,
                tip, deger_str);
        char yer[64];
        snprintf(yer, sizeof(yer), "@%.*s",
                 g->veri.sabit.ad_uzunluk, g->veri.sabit.ad);
        sem_ekle(&ctx, g->veri.sabit.ad,
                 g->veri.sabit.ad_uzunluk, yer, tip, 0);
    }
    fputc('\n', out);

    /* 2. Gecis: islev tanimlari */
    for (int i = 0; i < program->veri.program.sayi; i++) {
        const Dugum *uye = program->veri.program.uyeler[i];
        const Dugum *g = (uye->tip == DUGUM_DISA && uye->veri.disa.tanim)
                         ? uye->veri.disa.tanim : uye;
        if (g->tip == DUGUM_ISLEV) {
            islev_uret(&ctx, g);
        }
    }

    /* 3. Lifted lambdalari emit et. lambda_uret_full doğrudan
     * LambdaListe pointer'ini kullanir (captures dahil). */
    while (ctx.lambdalar) {
        LambdaListe *l = ctx.lambdalar;
        ctx.lambdalar = l->sonraki;
        lambda_uret_full(&ctx, l);
        /* captures'i serbest birak */
        CaptureItem *c = l->captures;
        while (c) {
            CaptureItem *n = c->sonraki;
            free(c);
            c = n;
        }
        free(l);
    }

    /* 4. Harici (declare) islevleri emit et — coklu dosya/obje icin */
    if (ctx.ext_decls) {
        fputc('\n', out);
        for (ExtDecl *e = ctx.ext_decls; e; e = e->sonraki) {
            fprintf(out, "declare %s @%s(", e->donus_tip, e->ad);
            for (int i = 0; i < e->arg_say; i++) {
                if (i > 0) fputs(", ", out);
                fputs(e->arg_tipler[i], out);
            }
            fputs(")\n", out);
        }
    }

    /* 4.5. LLVM intrinsic declaration — imha (zeroize) icin gerekirse */
    fputs("\ndeclare void @llvm.memset.p0.i64(ptr nocapture writeonly, "
          "i8, i64, i1 immarg)\n", out);
    /* 5. Attribute group emit et — naked icin */
    if (ctx.naked_kullanildi) {
        fputs("\nattributes #0 = { naked }\n", out);
    }

    /* 6. String literal globallari (.rodata) */
    if (ctx.stringler) fputc('\n', out);
    StringSabit *ss = ctx.stringler;
    while (ss) {
        /* Byte array (null-term yok — slice tabanli) */
        fprintf(out, "@.str%d = private unnamed_addr constant [%d x i8] [",
                ss->no, ss->uzunluk);
        for (int k = 0; k < ss->uzunluk; k++) {
            if (k > 0) fputs(", ", out);
            fprintf(out, "i8 %d", (unsigned char)ss->metin[k]);
        }
        fputs("]\n", out);
        /* Slice: { ptr, i64 } */
        fprintf(out,
            "@.s%d = private unnamed_addr constant { ptr, i64 } { ptr @.str%d, i64 %d }\n",
            ss->no, ss->no, ss->uzunluk);
        StringSabit *nx = ss->sonraki;
        free(ss);
        ss = nx;
    }

    /* Free ext_decls */
    ExtDecl *e = ctx.ext_decls;
    while (e) {
        ExtDecl *n = e->sonraki;
        free(e);
        e = n;
    }
}

void llvm_ir_uret(const Dugum *program, FILE *out) {
    llvm_ir_uret_target(program, out, NULL);
}
