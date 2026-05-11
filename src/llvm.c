#include "llvm.h"
#include "arena.h"

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

/*
 * KEMGU LLVM IR Backend (ADIM 17 — genişletilmiş)
 * ================================================
 *
 * Desteklenen ozellikler:
 *   - Tam sayilar (i32) — hepsi i32 olarak tek bir tip varsayilir
 *   - Mantiksal (i1)
 *   - Islev tanimi + parametreler (i32)
 *   - Islev cagrisi
 *   - Lokal degisken (alloca + store/load)
 *   - Atama (lvalue: tanimlayici)
 *   - if/else (BR + iki blok + birlesik blok)
 *   - while (header + body + done)
 *   - Karsilastirma ==, !=, <, >, <=, >=
 *   - Mantiksal ve, veya, degil
 *   - Aritmetik +, -, *, /, %, tekli -
 *   - ver
 *
 * Mevcut sinirlamalar:
 *   - Yapilar, diziler henuz yok
 *   - Yalniz i32 sayisal (tam8/16/64, dtam*, kesirli* yok)
 *   - Metin literali yok
 *   - Referans/pointer yok
 *
 * Mimari: LlvmGen state struct'i + visitor.
 *   isimler: lineer (Linked list) — degisken/parametre adi -> alloca reg veya
 *            dogrudan parametre.
 *   reg: SSA register counter (her ifade icin yeni %N).
 *   label: basic block etiket counter.
 */

typedef struct LlvmIsim {
    const char *ad;
    int ad_uz;
    /* 0 = parametre (dogrudan kullan: %<ad>)
     * 1 = lokal alloca (load gerekir): reg no */
    int kategori;
    int reg_no;
    struct LlvmIsim *sonraki;
} LlvmIsim;

typedef struct LlvmGen {
    FILE *out;
    Arena *arena;
    int reg;       /* sonraki SSA reg */
    int label;     /* sonraki etiket */
    LlvmIsim *isimler;
} LlvmGen;

/* === Forward === */
static int ifade_uret(LlvmGen *g, const Dugum *d);
static int blok_uret(LlvmGen *g, const Dugum *blok);  /* son ver yapildi mi? */

/* === Isim tablosu === */

static void isim_ekle(LlvmGen *g, const char *ad, int ad_uz,
                      int kategori, int reg_no) {
    LlvmIsim *i = (LlvmIsim *)arena_ayir_sifir(g->arena, sizeof(LlvmIsim));
    if (!i) return;
    i->ad = ad;
    i->ad_uz = ad_uz;
    i->kategori = kategori;
    i->reg_no = reg_no;
    i->sonraki = g->isimler;
    g->isimler = i;
}

static LlvmIsim *isim_bul(LlvmGen *g, const char *ad, int ad_uz) {
    for (LlvmIsim *i = g->isimler; i; i = i->sonraki) {
        if (i->ad_uz == ad_uz && memcmp(i->ad, ad, (size_t)ad_uz) == 0) {
            return i;
        }
    }
    return NULL;
}

/* Scope giris/cikis: linked list HEAD ve onceki HEAD'i kaydet. */
typedef struct ScopeMarker { LlvmIsim *eski_bas; } ScopeMarker;

static ScopeMarker scope_gir(LlvmGen *g) {
    ScopeMarker m;
    m.eski_bas = g->isimler;
    return m;
}

static void scope_cik(LlvmGen *g, ScopeMarker m) {
    g->isimler = m.eski_bas;
}

/* === Yardimcilar === */

static int yeni_reg(LlvmGen *g) { return g->reg++; }
static int yeni_label(LlvmGen *g) { return g->label++; }

/* Adi yaz (parametre adi vs.) — null-term degilse fwrite */
static void ad_yaz(FILE *out, const char *ad, int ad_uz) {
    fwrite(ad, 1, (size_t)ad_uz, out);
}

/* === Ifade IR === */

/* Tanimlayici cozumle: parametre veya alloca'dan deger oku. Reg no doner. */
static int tanimlayici_yukle(LlvmGen *g, const Dugum *d) {
    LlvmIsim *i = isim_bul(g, d->veri.tanimlayici.metin,
                            d->veri.tanimlayici.uzunluk);
    if (!i) {
        int r = yeni_reg(g);
        fprintf(g->out, "  ; HATA: tanimsiz '%.*s'\n",
                d->veri.tanimlayici.uzunluk, d->veri.tanimlayici.metin);
        fprintf(g->out, "  %%%d = add i32 0, 0\n", r);
        return r;
    }
    if (i->kategori == 0) {
        /* Parametre — dogrudan deger; yeni reg'e kopyala (uniform interface) */
        int r = yeni_reg(g);
        fprintf(g->out, "  %%%d = add i32 0, %%", r);
        ad_yaz(g->out, i->ad, i->ad_uz);
        fputs("\n", g->out);
        return r;
    }
    /* Alloca'dan yukle */
    int r = yeni_reg(g);
    fprintf(g->out, "  %%%d = load i32, ptr %%%d\n", r, i->reg_no);
    return r;
}

/* Karsilastirma op kodu -> LLVM icmp predicate */
static const char *icmp_pred(Operator op) {
    switch (op) {
        case OP_ESIT:        return "eq";
        case OP_ESIT_DEGIL:  return "ne";
        case OP_KUCUK:       return "slt";
        case OP_BUYUK:       return "sgt";
        case OP_KUCUK_ESIT:  return "sle";
        case OP_BUYUK_ESIT:  return "sge";
        default:             return NULL;
    }
}

static int ifade_uret(LlvmGen *g, const Dugum *d) {
    if (!d) {
        int r = yeni_reg(g);
        fprintf(g->out, "  %%%d = add i32 0, 0\n", r);
        return r;
    }

    switch (d->tip) {
        case DUGUM_TAM: {
            int r = yeni_reg(g);
            fprintf(g->out, "  %%%d = add i32 0, %" PRId64 "\n",
                    r, d->veri.tam.deger);
            return r;
        }

        case DUGUM_MANTIKSAL: {
            /* i1 olarak hesapla ama i32'ye genislet (uniform sayisal akis) */
            int r = yeni_reg(g);
            fprintf(g->out, "  %%%d = add i32 0, %d\n",
                    r, d->veri.mantiksal.deger ? 1 : 0);
            return r;
        }

        case DUGUM_TANIMLAYICI:
            return tanimlayici_yukle(g, d);

        case DUGUM_IKILI: {
            const char *cmp = icmp_pred(d->veri.ikili.op);
            int sol_r = ifade_uret(g, d->veri.ikili.sol);
            int sag_r = ifade_uret(g, d->veri.ikili.sag);
            if (cmp) {
                int r_cmp = yeni_reg(g);
                fprintf(g->out, "  %%%d = icmp %s i32 %%%d, %%%d\n",
                        r_cmp, cmp, sol_r, sag_r);
                /* i1 -> i32 genislet (mantiksal uniform) */
                int r = yeni_reg(g);
                fprintf(g->out, "  %%%d = zext i1 %%%d to i32\n", r, r_cmp);
                return r;
            }
            /* Mantiksal ve/veya: i32 0/1 olarak modelliyoruz (short-circuit
             * yok — pure arithmetic). */
            const char *op = NULL;
            switch (d->veri.ikili.op) {
                case OP_ARTI:  op = "add"; break;
                case OP_EKSI:  op = "sub"; break;
                case OP_CARPI: op = "mul"; break;
                case OP_BOLU:  op = "sdiv"; break;
                case OP_MOD:   op = "srem"; break;
                case OP_VE:    op = "and"; break;
                case OP_VEYA:  op = "or"; break;
                default:
                    fputs("  ; ikili op desteklenmiyor\n", g->out);
                    return sol_r;
            }
            int r = yeni_reg(g);
            fprintf(g->out, "  %%%d = %s i32 %%%d, %%%d\n",
                    r, op, sol_r, sag_r);
            return r;
        }

        case DUGUM_TEKLI: {
            int op_r = ifade_uret(g, d->veri.tekli.operand);
            if (d->veri.tekli.op == OP_NEG) {
                int r = yeni_reg(g);
                fprintf(g->out, "  %%%d = sub i32 0, %%%d\n", r, op_r);
                return r;
            }
            if (d->veri.tekli.op == OP_DEGIL) {
                /* x == 0 -> 1, x != 0 -> 0 */
                int r_cmp = yeni_reg(g);
                fprintf(g->out, "  %%%d = icmp eq i32 %%%d, 0\n", r_cmp, op_r);
                int r = yeni_reg(g);
                fprintf(g->out, "  %%%d = zext i1 %%%d to i32\n", r, r_cmp);
                return r;
            }
            fputs("  ; tekli op desteklenmiyor\n", g->out);
            return op_r;
        }

        case DUGUM_CAGRI: {
            /* Hedef: tanimlayici (global islev adi) */
            if (!d->veri.cagri.hedef ||
                d->veri.cagri.hedef->tip != DUGUM_TANIMLAYICI) {
                int r = yeni_reg(g);
                fprintf(g->out, "  ; HATA: cagri hedefi tanimlayici degil\n");
                fprintf(g->out, "  %%%d = add i32 0, 0\n", r);
                return r;
            }
            int n = d->veri.cagri.sayi;
            int *arg_reg = NULL;
            if (n > 0) {
                arg_reg = (int *)arena_ayir(g->arena, sizeof(int) * (size_t)n);
                for (int i = 0; i < n; i++) {
                    arg_reg[i] = ifade_uret(g, d->veri.cagri.argumanlar[i]);
                }
            }
            int r = yeni_reg(g);
            fprintf(g->out, "  %%%d = call i32 @", r);
            ad_yaz(g->out,
                   d->veri.cagri.hedef->veri.tanimlayici.metin,
                   d->veri.cagri.hedef->veri.tanimlayici.uzunluk);
            fputs("(", g->out);
            for (int i = 0; i < n; i++) {
                if (i > 0) fputs(", ", g->out);
                fprintf(g->out, "i32 %%%d", arg_reg[i]);
            }
            fputs(")\n", g->out);
            return r;
        }

        default: {
            int r = yeni_reg(g);
            fprintf(g->out, "  ; ifade tipi %d desteklenmiyor\n", d->tip);
            fprintf(g->out, "  %%%d = add i32 0, 0\n", r);
            return r;
        }
    }
}

/* === Deyim IR === */

/* Donus: bu blok 'ver' (terminator) ile bitti mi? */
static int deyim_uret_terminated(LlvmGen *g, const Dugum *d) {
    if (!d) return 0;

    switch (d->tip) {
        case DUGUM_VER: {
            if (d->veri.ver.deger) {
                int r = ifade_uret(g, d->veri.ver.deger);
                fprintf(g->out, "  ret i32 %%%d\n", r);
            } else {
                fputs("  ret i32 0\n", g->out);
            }
            return 1;
        }

        case DUGUM_DEGISKEN: {
            /* degisken x = ifade;
             *
             * alloca + store: ileride load edebilelim diye.
             * Not: LLVM idiomatic'te alloca'lar entry blogunda olur. Bizimki
             * dogru yerde (entry) cunku biz su an islev govdesinde top-level
             * deyim olarak isliyoruz. Ic blok scope'unda da alloca yapacagiz
             * (mem2reg pass'i kaldirir). */
            int alloca_reg = yeni_reg(g);
            fprintf(g->out, "  %%%d = alloca i32\n", alloca_reg);
            if (d->veri.degisken.deger) {
                int v = ifade_uret(g, d->veri.degisken.deger);
                fprintf(g->out, "  store i32 %%%d, ptr %%%d\n", v, alloca_reg);
            }
            isim_ekle(g, d->veri.degisken.ad, d->veri.degisken.ad_uzunluk,
                      1, alloca_reg);
            return 0;
        }

        case DUGUM_ATAMA: {
            /* Hedef: tanimlayici (basit lvalue) */
            if (d->veri.atama.hedef &&
                d->veri.atama.hedef->tip == DUGUM_TANIMLAYICI) {
                LlvmIsim *i = isim_bul(g,
                    d->veri.atama.hedef->veri.tanimlayici.metin,
                    d->veri.atama.hedef->veri.tanimlayici.uzunluk);
                if (i && i->kategori == 1) {
                    int v = ifade_uret(g, d->veri.atama.deger);
                    fprintf(g->out, "  store i32 %%%d, ptr %%%d\n",
                            v, i->reg_no);
                } else {
                    fputs("  ; atama hedefi alloca degil\n", g->out);
                }
            } else {
                fputs("  ; atama hedefi tanimlayici degil\n", g->out);
            }
            return 0;
        }

        case DUGUM_EGER: {
            int kosul = ifade_uret(g, d->veri.eger.kosul);
            /* i32 kosulu i1'e indirge: kosul != 0 */
            int i1r = yeni_reg(g);
            fprintf(g->out, "  %%%d = icmp ne i32 %%%d, 0\n", i1r, kosul);
            int L_then = yeni_label(g);
            int L_else = yeni_label(g);
            int L_end = yeni_label(g);
            fprintf(g->out, "  br i1 %%%d, label %%bb%d, label %%bb%d\n",
                    i1r, L_then, L_else);

            fprintf(g->out, "bb%d:\n", L_then);
            int then_term = blok_uret(g, d->veri.eger.gozdoldur);
            if (!then_term) fprintf(g->out, "  br label %%bb%d\n", L_end);

            fprintf(g->out, "bb%d:\n", L_else);
            int else_term = 0;
            if (d->veri.eger.yan) {
                if (d->veri.eger.yan->tip == DUGUM_BLOK) {
                    else_term = blok_uret(g, d->veri.eger.yan);
                } else if (d->veri.eger.yan->tip == DUGUM_EGER) {
                    else_term = deyim_uret_terminated(g, d->veri.eger.yan);
                }
            }
            if (!else_term) fprintf(g->out, "  br label %%bb%d\n", L_end);

            /* End-block sadece en az bir dal fall-through ediyorsa gerek.
             * Iki dal da terminated ise burayi atla — caller bu eger'in
             * terminated oldugunu gorur ve sonraki deyim islemez. */
            if (then_term && else_term) {
                return 1;
            }
            fprintf(g->out, "bb%d:\n", L_end);
            return 0;
        }

        case DUGUM_IKEN: {
            int L_head = yeni_label(g);
            int L_body = yeni_label(g);
            int L_done = yeni_label(g);
            fprintf(g->out, "  br label %%bb%d\n", L_head);
            fprintf(g->out, "bb%d:\n", L_head);
            int kosul = ifade_uret(g, d->veri.iken.kosul);
            int i1r = yeni_reg(g);
            fprintf(g->out, "  %%%d = icmp ne i32 %%%d, 0\n", i1r, kosul);
            fprintf(g->out, "  br i1 %%%d, label %%bb%d, label %%bb%d\n",
                    i1r, L_body, L_done);
            fprintf(g->out, "bb%d:\n", L_body);
            int body_term = blok_uret(g, d->veri.iken.govde);
            if (!body_term) fprintf(g->out, "  br label %%bb%d\n", L_head);
            fprintf(g->out, "bb%d:\n", L_done);
            return 0;
        }

        case DUGUM_BLOK: {
            ScopeMarker m = scope_gir(g);
            int term = blok_uret(g, d);
            scope_cik(g, m);
            return term;
        }

        case DUGUM_IFADE_DEYIMI:
            (void)ifade_uret(g, d->veri.ifade_deyimi.ifade);
            return 0;

        default:
            fprintf(g->out, "  ; deyim tipi %d desteklenmiyor\n", d->tip);
            return 0;
    }
}

static int blok_uret(LlvmGen *g, const Dugum *blok) {
    if (!blok || blok->tip != DUGUM_BLOK) return 0;
    ScopeMarker m = scope_gir(g);
    for (int i = 0; i < blok->veri.blok.sayi; i++) {
        int term = deyim_uret_terminated(g, blok->veri.blok.deyimler[i]);
        if (term) {
            scope_cik(g, m);
            return 1;
        }
    }
    scope_cik(g, m);
    return 0;
}

/* deyim_uret kullanilmadigi icin acik tutmadik — gerektiginde
 * dogrudan deyim_uret_terminated cagrilir. */

/* === Islev IR === */

static void islev_uret(LlvmGen *g, const Dugum *islev) {
    fputs("define i32 @", g->out);
    ad_yaz(g->out, islev->veri.islev.ad, islev->veri.islev.ad_uzunluk);
    fputs("(", g->out);
    for (int i = 0; i < islev->veri.islev.param_sayi; i++) {
        const Dugum *p = islev->veri.islev.parametreler[i];
        if (i > 0) fputs(", ", g->out);
        fputs("i32 %", g->out);
        ad_yaz(g->out, p->veri.parametre.ad, p->veri.parametre.ad_uzunluk);
    }
    fputs(") {\nentry:\n", g->out);

    g->reg = 0;
    g->label = 0;
    g->isimler = NULL;

    /* Parametreleri alloca + store ile lokal mutable hale getir.
     * Bu sayede 'b = a + 1;' gibi parametre atamalari calisir.
     * mem2reg pass'i optimize hale getirir. */
    for (int i = 0; i < islev->veri.islev.param_sayi; i++) {
        const Dugum *p = islev->veri.islev.parametreler[i];
        int alloca_reg = yeni_reg(g);
        fprintf(g->out, "  %%%d = alloca i32\n", alloca_reg);
        fprintf(g->out, "  store i32 %%");
        ad_yaz(g->out, p->veri.parametre.ad, p->veri.parametre.ad_uzunluk);
        fprintf(g->out, ", ptr %%%d\n", alloca_reg);
        isim_ekle(g, p->veri.parametre.ad, p->veri.parametre.ad_uzunluk,
                  1, alloca_reg);
    }

    /* Govdeyi uret */
    int term = 0;
    if (islev->veri.islev.govde) {
        term = blok_uret(g, islev->veri.islev.govde);
    }
    if (!term) {
        fputs("  ret i32 0\n", g->out);
    }
    fputs("}\n\n", g->out);
}

/* === Public API === */

void llvm_ir_uret(const Dugum *program, FILE *out) {
    if (!out) return;
    fputs("; KEMGU LLVM IR (text uretici, ADIM 17 genisletilmis)\n", out);
    fputs("; `clang -x ir - -o cikti.exe` ile derlenebilir.\n", out);
    fputs("target triple = \"x86_64-pc-windows-gnu\"\n\n", out);

    if (!program || program->tip != DUGUM_PROGRAM) {
        fputs("; (program AST'si yok)\n", out);
        return;
    }

    /* LlvmGen icin lokal arena */
    Arena *a = arena_olustur(0);
    if (!a) return;

    LlvmGen g;
    memset(&g, 0, sizeof(g));
    g.out = out;
    g.arena = a;

    for (int i = 0; i < program->veri.program.sayi; i++) {
        const Dugum *uye = program->veri.program.uyeler[i];
        if (uye->tip == DUGUM_ISLEV) {
            islev_uret(&g, uye);
        } else if (uye->tip == DUGUM_DISA && uye->veri.disa.tanim &&
                   uye->veri.disa.tanim->tip == DUGUM_ISLEV) {
            islev_uret(&g, uye->veri.disa.tanim);
        }
    }

    arena_serbest(a);
}
