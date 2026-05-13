#include "llvm.h"
#include "arena.h"
#include "lexer.h"
#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

/*
 * KEMGU LLVM IR Backend v2 (ADIM 18)
 * ===================================
 *
 * Eklenenler:
 *   - Coklu int tipleri (tam8/16/32/64, dtam*, mantiksal i1, karakter i32)
 *   - Metin literali (global string constant + GEP)
 *   - Yapi tanimi + olusturma + alan erisimi (locals)
 *   - Tip annotation'larina dayali context-driven tip secimi
 *
 * Tasarim:
 *   - IfadeSonuc { reg, tip_ir } — her ifade hem reg hem LLVM IR tipi doner
 *   - LlvmIsim: deklare edilen degiskenin tipi de saklanir
 *   - Pre-pass: tum metin literalleri ve yapi tanimlari toplanir, module
 *     basinda emit edilir
 *
 * Sinirlamalar (v2'de hala):
 *   - kesirli32/64 (float/double) yok — sadece tam sayisal
 *   - Yapilar by-pointer (struct-by-value parametreler v3'te)
 *   - Karakter UTF-8 byte sequence yerine i32 code point (KEMGU spec uyumu)
 *   - mantiksal islevin parametre/donus tipi olarak kullanildigi zaman i1
 */

/* === Yapilar === */

typedef struct LlvmIsim {
    const char *ad;
    int ad_uz;
    int kategori;          /* 0 = parametre alloca, 1 = lokal alloca */
    int reg_no;            /* alloca register */
    const char *llvm_tip;  /* "i32", "i8", "ptr", "%Hasta" */
    struct LlvmIsim *sonraki;
} LlvmIsim;

typedef struct StrKayit {
    const Dugum *d;        /* DUGUM_METIN node */
    int id;
    int byte_uz;           /* metin uzunlugu (\00 dahil edilmiyor) */
    struct StrKayit *sonraki;
} StrKayit;

typedef struct YapiKayit {
    const char *ad;
    int ad_uz;
    const Dugum *ast;      /* DUGUM_YAPI node */
    struct YapiKayit *sonraki;
} YapiKayit;

typedef struct IslevKayit {
    const char *ad;
    int ad_uz;
    const char *donus_tip;
    /* Generic islev: AST'yi sakla, instantiation icin gerekli */
    const Dugum *ast;
    int generic_mi;  /* tip_param_sayi > 0 */
    struct IslevKayit *sonraki;
} IslevKayit;

/* Tip substitution context: generic param adi -> ir tipi */
typedef struct TipSubst {
    const char *ad;
    int ad_uz;
    const char *ir;
    struct TipSubst *sonraki;
} TipSubst;

/* Emit edilmis instantiation: ad$T1$T2 -> 1 */
typedef struct MonoKayit {
    const char *mangled;  /* arena */
    struct MonoKayit *sonraki;
} MonoKayit;

/* Yüklenmiş modül (cycle önleme) */
typedef struct YuklenmisDosya {
    const char *yol;
    int yol_uz;
    struct YuklenmisDosya *sonraki;
} YuklenmisDosya;

/* Bekleyen specialization (cagri sirasinda olustu, sonradan emit edilecek) */
typedef struct BekleyenSpec {
    const Dugum *ast;        /* generic islev AST */
    const char *mangled;     /* hedef ad */
    const char **tip_args;   /* substitusyon icin */
    int tip_arg_sayi;
    struct BekleyenSpec *sonraki;
} BekleyenSpec;

typedef struct LlvmGen {
    FILE *out;
    Arena *arena;
    int reg;
    int label;
    LlvmIsim *isimler;
    StrKayit *strler;
    int str_sayaci;
    YapiKayit *yapilar;
    IslevKayit *islevler;
    TipSubst *substler;     /* aktif generic param substitutions */
    MonoKayit *monolar;     /* emit edilmis instantiation'lar */
    BekleyenSpec *bekleyenler;  /* sonradan emit edilecek */
    YuklenmisDosya *yuklenmis_dosyalar;  /* kullan tarafindan yuklenenler */
} LlvmGen;

typedef struct IfadeSonuc {
    int reg;               /* -1 = error */
    const char *tip;       /* IR tipi */
} IfadeSonuc;

/* === Forward === */

static IfadeSonuc ifade_uret(LlvmGen *g, const Dugum *d, const char *beklenen);
static int blok_uret(LlvmGen *g, const Dugum *blok);
static YapiKayit *yapi_bul(LlvmGen *g, const char *ad, int ad_uz);
static int mono_emitlendi(LlvmGen *g, const char *mangled);
static const char *mangle_et(LlvmGen *g, const char *ad, int ad_uz,
                              const char **tipler, int tip_sayi);

/* === Isim tablosu === */

static void isim_ekle(LlvmGen *g, const char *ad, int ad_uz,
                      int kategori, int reg_no, const char *tip) {
    LlvmIsim *i = (LlvmIsim *)arena_ayir_sifir(g->arena, sizeof(LlvmIsim));
    if (!i) return;
    i->ad = ad;
    i->ad_uz = ad_uz;
    i->kategori = kategori;
    i->reg_no = reg_no;
    i->llvm_tip = tip;
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

static void ad_yaz(FILE *out, const char *ad, int ad_uz) {
    fwrite(ad, 1, (size_t)ad_uz, out);
}

/* AST tip dugumunden (DUGUM_TIP_BASIT, DUGUM_TIP_KULLANICI) LLVM IR tipi.
 * NULL -> NULL doner. Bilinmeyen -> "i32" (varsayilan). */
/* Generic param substitusyon kontrolu */
static const char *subst_bul(LlvmGen *g, const char *ad, int ad_uz) {
    for (TipSubst *s = g->substler; s; s = s->sonraki) {
        if (s->ad_uz == ad_uz && memcmp(s->ad, ad, (size_t)ad_uz) == 0) {
            return s->ir;
        }
    }
    return NULL;
}

static const char *ast_tip_to_ir(LlvmGen *g, const Dugum *tip_d) {
    if (!tip_d) return NULL;
    if (tip_d->tip == DUGUM_TIP_BASIT) {
        const char *a = tip_d->veri.tip_basit.ad;
        int u = tip_d->veri.tip_basit.ad_uzunluk;
        /* Generic param substitusyon */
        const char *sub = subst_bul(g, a, u);
        if (sub) return sub;
        #define ESLES(s) (u == (int)(sizeof(s) - 1) && memcmp(a, s, sizeof(s) - 1) == 0)
        if (ESLES("tam8") || ESLES("dtam8")) return "i8";
        if (ESLES("tam16") || ESLES("dtam16")) return "i16";
        if (ESLES("tam32") || ESLES("dtam32")) return "i32";
        if (ESLES("tam64") || ESLES("dtam64")) return "i64";
        if (ESLES("kesirli32")) return "float";
        if (ESLES("kesirli64")) return "double";
        if (ESLES("mant" "\xc4\xb1" "ksal")) return "i1";
        if (ESLES("karakter")) return "i32";
        if (ESLES("metin")) return "ptr";
        if (ESLES("bo" "\xc5\x9f")) return "void";
        #undef ESLES
        /* Taninmayan basit tip — kullanici yapisi olabilir mi? */
        YapiKayit *yk = yapi_bul(g, a, u);
        if (yk) {
            int sz = yk->ad_uz + 1;
            char *buf = (char *)arena_ayir(g->arena, (size_t)sz + 1);
            if (buf) {
                buf[0] = '%';
                memcpy(buf + 1, yk->ad, (size_t)yk->ad_uz);
                buf[sz] = '\0';
                return buf;
            }
        }
        return "i32";
    }
    if (tip_d->tip == DUGUM_TIP_KULLANICI && tip_d->veri.tip_kullanici.yol) {
        const Dugum *y = tip_d->veri.tip_kullanici.yol;
        if (y->tip == DUGUM_TANIMLAYICI) {
            /* Yapi tipi mi? Kayitliysa "%Ad" doner (struct-by-value),
             * degilse "ptr" (trait vb. kullanici tipi). */
            YapiKayit *yk = yapi_bul(g,
                y->veri.tanimlayici.metin,
                y->veri.tanimlayici.uzunluk);
            if (yk) {
                /* "%Ad" stringini arena'da olustur */
                int uz = yk->ad_uz + 1;
                char *buf = (char *)arena_ayir(g->arena, (size_t)uz + 1);
                if (buf) {
                    buf[0] = '%';
                    memcpy(buf + 1, yk->ad, (size_t)yk->ad_uz);
                    buf[uz] = '\0';
                    return buf;
                }
            }
            return "ptr";
        }
    }
    if (tip_d->tip == DUGUM_TIP_REFERANS || tip_d->tip == DUGUM_TIP_POINTER) {
        return "ptr";
    }
    if (tip_d->tip == DUGUM_TIP_DIZI) return "ptr";
    return "i32";  /* default */
}

/* Yapi adi bul (sembol tablosunda yapi mi diye) */
static YapiKayit *yapi_bul(LlvmGen *g, const char *ad, int ad_uz) {
    for (YapiKayit *y = g->yapilar; y; y = y->sonraki) {
        if (y->ad_uz == ad_uz && memcmp(y->ad, ad, (size_t)ad_uz) == 0) {
            return y;
        }
    }
    return NULL;
}

/* Yapinin alaninin index ve tipini bul */
static int yapi_alan_indeksi(const YapiKayit *y, const char *ad, int ad_uz,
                             const Dugum **out_tip) {
    if (!y || !y->ast) return -1;
    for (int i = 0; i < y->ast->veri.yapi.alan_sayi; i++) {
        const Dugum *a = y->ast->veri.yapi.alanlar[i];
        if (a->veri.alan.ad_uzunluk == ad_uz &&
            memcmp(a->veri.alan.ad, ad, (size_t)ad_uz) == 0) {
            if (out_tip) *out_tip = a->veri.alan.tip;
            return i;
        }
    }
    return -1;
}

/* Tip kategorisi: ayni tipler arasinda dogrudan donusum yok.
 * Sadece basit darlatma/genisletme yardimcisi: src->dst int donusumleri. */
static int tip_genisligi(const char *ir) {
    if (!ir) return 0;
    if (strcmp(ir, "i1") == 0) return 1;
    if (strcmp(ir, "i8") == 0) return 8;
    if (strcmp(ir, "i16") == 0) return 16;
    if (strcmp(ir, "i32") == 0) return 32;
    if (strcmp(ir, "i64") == 0) return 64;
    return 0;
}

/* src tipindeki reg'i dst tipine cevir. Sadece int donusumleri.
 * Ayni tip -> reg ayni doner. */
static int int_donustur(LlvmGen *g, int src_reg, const char *src_tip,
                        const char *dst_tip) {
    if (!src_tip || !dst_tip || strcmp(src_tip, dst_tip) == 0) return src_reg;
    int src_w = tip_genisligi(src_tip);
    int dst_w = tip_genisligi(dst_tip);
    if (src_w == 0 || dst_w == 0) return src_reg;
    int r = yeni_reg(g);
    if (src_w < dst_w) {
        /* Genislet (sext for signed) */
        fprintf(g->out, "  %%%d = sext %s %%%d to %s\n",
                r, src_tip, src_reg, dst_tip);
    } else {
        /* Daralt */
        fprintf(g->out, "  %%%d = trunc %s %%%d to %s\n",
                r, src_tip, src_reg, dst_tip);
    }
    return r;
}

/* === Pre-pass: metin literallerini topla === */

static int dugum_metin_eslesir(const Dugum *a, const Dugum *b) {
    if (a == b) return 1;
    if (a->veri.metin_lit.uzunluk != b->veri.metin_lit.uzunluk) return 0;
    return memcmp(a->veri.metin_lit.metin, b->veri.metin_lit.metin,
                  (size_t)a->veri.metin_lit.uzunluk) == 0;
}

static StrKayit *str_bul(LlvmGen *g, const Dugum *d) {
    for (StrKayit *s = g->strler; s; s = s->sonraki) {
        if (dugum_metin_eslesir(s->d, d)) return s;
    }
    return NULL;
}

static int str_kayit_et(LlvmGen *g, const Dugum *d) {
    StrKayit *e = str_bul(g, d);
    if (e) return e->id;
    StrKayit *yeni = (StrKayit *)arena_ayir_sifir(g->arena, sizeof(StrKayit));
    if (!yeni) return -1;
    yeni->d = d;
    yeni->id = g->str_sayaci++;
    yeni->byte_uz = d->veri.metin_lit.uzunluk;
    yeni->sonraki = g->strler;
    g->strler = yeni;
    return yeni->id;
}

static void ast_taransa_metinleri(LlvmGen *g, const Dugum *d);

static void ast_taransa_metinleri_liste(LlvmGen *g, Dugum **liste, int sayi) {
    for (int i = 0; i < sayi; i++) ast_taransa_metinleri(g, liste[i]);
}

static void ast_taransa_metinleri(LlvmGen *g, const Dugum *d) {
    if (!d) return;
    if (d->tip == DUGUM_METIN) {
        str_kayit_et(g, d);
        return;
    }
    switch (d->tip) {
        case DUGUM_PROGRAM:
            ast_taransa_metinleri_liste(g, d->veri.program.uyeler,
                                         d->veri.program.sayi); break;
        case DUGUM_ISLEV:
            ast_taransa_metinleri(g, d->veri.islev.govde); break;
        case DUGUM_DISA:
            ast_taransa_metinleri(g, d->veri.disa.tanim); break;
        case DUGUM_BLOK:
            ast_taransa_metinleri_liste(g, d->veri.blok.deyimler,
                                         d->veri.blok.sayi); break;
        case DUGUM_DEGISKEN:
            ast_taransa_metinleri(g, d->veri.degisken.deger); break;
        case DUGUM_ATAMA:
            ast_taransa_metinleri(g, d->veri.atama.hedef);
            ast_taransa_metinleri(g, d->veri.atama.deger); break;
        case DUGUM_VER:
            ast_taransa_metinleri(g, d->veri.ver.deger); break;
        case DUGUM_EGER:
            ast_taransa_metinleri(g, d->veri.eger.kosul);
            ast_taransa_metinleri(g, d->veri.eger.gozdoldur);
            ast_taransa_metinleri(g, d->veri.eger.yan); break;
        case DUGUM_IKEN:
            ast_taransa_metinleri(g, d->veri.iken.kosul);
            ast_taransa_metinleri(g, d->veri.iken.govde); break;
        case DUGUM_ICIN:
            ast_taransa_metinleri(g, d->veri.icin.koleksiyon);
            ast_taransa_metinleri(g, d->veri.icin.govde); break;
        case DUGUM_IFADE_DEYIMI:
            ast_taransa_metinleri(g, d->veri.ifade_deyimi.ifade); break;
        case DUGUM_IKILI:
            ast_taransa_metinleri(g, d->veri.ikili.sol);
            ast_taransa_metinleri(g, d->veri.ikili.sag); break;
        case DUGUM_TEKLI:
            ast_taransa_metinleri(g, d->veri.tekli.operand); break;
        case DUGUM_CAGRI:
            ast_taransa_metinleri(g, d->veri.cagri.hedef);
            ast_taransa_metinleri_liste(g, d->veri.cagri.argumanlar,
                                         d->veri.cagri.sayi); break;
        case DUGUM_ERISIM:
            ast_taransa_metinleri(g, d->veri.erisim.nesne); break;
        case DUGUM_INDEKS:
            ast_taransa_metinleri(g, d->veri.indeks.nesne);
            ast_taransa_metinleri(g, d->veri.indeks.indeks); break;
        case DUGUM_DIZI_OLUSTUR:
            ast_taransa_metinleri_liste(g, d->veri.dizi_olustur.elemanlar,
                                         d->veri.dizi_olustur.sayi); break;
        case DUGUM_YAPI_OLUSTUR:
            for (int i = 0; i < d->veri.yapi_olustur.alan_sayi; i++) {
                Dugum *aa = d->veri.yapi_olustur.alanlar[i];
                if (aa && aa->tip == DUGUM_ALAN_ATAMA) {
                    ast_taransa_metinleri(g, aa->veri.alan_atama.deger);
                }
            }
            break;
        case DUGUM_SABIT:
            ast_taransa_metinleri(g, d->veri.sabit.deger); break;
        default: break;
    }
}

/* Yapi pre-pass */

static void islev_kayit(LlvmGen *g, const Dugum *i) {
    IslevKayit *r = (IslevKayit *)arena_ayir_sifir(g->arena, sizeof(IslevKayit));
    if (!r) return;
    r->ad = i->veri.islev.ad;
    r->ad_uz = i->veri.islev.ad_uzunluk;
    r->ast = i;
    r->generic_mi = (i->veri.islev.tip_param_sayi > 0);
    const char *dt = i->veri.islev.donus_tipi
        ? ast_tip_to_ir(g, i->veri.islev.donus_tipi)
        : "void";
    r->donus_tip = dt ? dt : "void";
    r->sonraki = g->islevler;
    g->islevler = r;
}

static IslevKayit *islev_bul(LlvmGen *g, const char *ad, int ad_uz) {
    for (IslevKayit *i = g->islevler; i; i = i->sonraki) {
        if (i->ad_uz == ad_uz && memcmp(i->ad, ad, (size_t)ad_uz) == 0) {
            return i;
        }
    }
    return NULL;
}

static void yapi_kayit(LlvmGen *g, const Dugum *y) {
    YapiKayit *r = (YapiKayit *)arena_ayir_sifir(g->arena, sizeof(YapiKayit));
    if (!r) return;
    r->ad = y->veri.yapi.ad;
    r->ad_uz = y->veri.yapi.ad_uzunluk;
    r->ast = y;
    r->sonraki = g->yapilar;
    g->yapilar = r;
}

/* === Module-basi globaller === */

static void str_globalleri_emit(LlvmGen *g) {
    /* @.str.N = private unnamed_addr constant [K x i8] c"...\00" */
    for (StrKayit *s = g->strler; s; s = s->sonraki) {
        int uz = s->byte_uz;
        fprintf(g->out,
            "@.str.%d = private unnamed_addr constant [%d x i8] c\"",
            s->id, uz + 1);
        const char *m = s->d->veri.metin_lit.metin;
        for (int i = 0; i < uz; i++) {
            unsigned char c = (unsigned char)m[i];
            if (c == '\\' || c == '"' || c < 0x20 || c >= 0x7F) {
                fprintf(g->out, "\\%02X", c);
            } else {
                fputc(c, g->out);
            }
        }
        fputs("\\00\"\n", g->out);
    }
    if (g->strler) fputs("\n", g->out);
}

static void yapi_tip_tanimlari_emit(LlvmGen *g) {
    /* %YapiAdi = type { tip1, tip2, ... } */
    for (YapiKayit *y = g->yapilar; y; y = y->sonraki) {
        fputs("%", g->out);
        ad_yaz(g->out, y->ad, y->ad_uz);
        fputs(" = type { ", g->out);
        for (int i = 0; i < y->ast->veri.yapi.alan_sayi; i++) {
            if (i > 0) fputs(", ", g->out);
            const Dugum *a = y->ast->veri.yapi.alanlar[i];
            const char *ir = ast_tip_to_ir(g, a->veri.alan.tip);
            fputs(ir ? ir : "i32", g->out);
        }
        fputs(" }\n", g->out);
    }
    if (g->yapilar) fputs("\n", g->out);
}

/* === Ifade IR === */

static IfadeSonuc hata(LlvmGen *g, const char *mesaj) {
    int r = yeni_reg(g);
    fprintf(g->out, "  ; HATA: %s\n", mesaj);
    fprintf(g->out, "  %%%d = add i32 0, 0\n", r);
    IfadeSonuc s = { r, "i32" };
    return s;
}

/* Tanimlayici cozumle: alloca'dan load. */
static IfadeSonuc tanimlayici_yukle(LlvmGen *g, const Dugum *d) {
    LlvmIsim *i = isim_bul(g, d->veri.tanimlayici.metin,
                            d->veri.tanimlayici.uzunluk);
    if (!i) return hata(g, "tanimsiz tanimlayici");
    int r = yeni_reg(g);
    fprintf(g->out, "  %%%d = load %s, ptr %%%d\n",
            r, i->llvm_tip, i->reg_no);
    IfadeSonuc s = { r, i->llvm_tip };
    return s;
}

/* Karsilastirma op kodu -> LLVM icmp predicate (signed) */
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

/* fcmp predicate (ordered) */
static const char *fcmp_pred(Operator op) {
    switch (op) {
        case OP_ESIT:        return "oeq";
        case OP_ESIT_DEGIL:  return "one";
        case OP_KUCUK:       return "olt";
        case OP_BUYUK:       return "ogt";
        case OP_KUCUK_ESIT:  return "ole";
        case OP_BUYUK_ESIT:  return "oge";
        default:             return NULL;
    }
}

static int tip_kesirli_mi(const char *ir) {
    return ir && (strcmp(ir, "float") == 0 || strcmp(ir, "double") == 0);
}

/* Beklenen tip 'metin' veya 'ptr' ise string literal pointer'a yukselt */
static IfadeSonuc metin_lit_uret(LlvmGen *g, const Dugum *d) {
    StrKayit *s = str_bul(g, d);
    if (!s) return hata(g, "metin literal kayitsiz");
    int r = yeni_reg(g);
    fprintf(g->out,
        "  %%%d = getelementptr [%d x i8], ptr @.str.%d, i32 0, i32 0\n",
        r, s->byte_uz + 1, s->id);
    IfadeSonuc res = { r, "ptr" };
    return res;
}

/* DUGUM_YAPI_OLUSTUR -> alloca + GEP/store alanlar + load struct value */
static IfadeSonuc yapi_olustur_uret(LlvmGen *g, const Dugum *d) {
    YapiKayit *y = yapi_bul(g, d->veri.yapi_olustur.tip_ad,
                             d->veri.yapi_olustur.tip_ad_uzunluk);
    if (!y) return hata(g, "yapi tipi bilinmiyor");

    /* Tip stringi olustur: "%Ad" */
    int tip_uz = y->ad_uz + 1;
    char *yapi_ir = (char *)arena_ayir(g->arena, (size_t)tip_uz + 1);
    yapi_ir[0] = '%';
    memcpy(yapi_ir + 1, y->ad, (size_t)y->ad_uz);
    yapi_ir[tip_uz] = '\0';

    int alloca_r = yeni_reg(g);
    fprintf(g->out, "  %%%d = alloca %s\n", alloca_r, yapi_ir);

    /* Her alan icin: ad ile yapinin alan index'ini bul, GEP+store */
    for (int i = 0; i < d->veri.yapi_olustur.alan_sayi; i++) {
        Dugum *aa = d->veri.yapi_olustur.alanlar[i];
        if (!aa || aa->tip != DUGUM_ALAN_ATAMA) continue;
        const Dugum *alan_tip_d = NULL;
        int idx = yapi_alan_indeksi(y, aa->veri.alan_atama.ad,
                                     aa->veri.alan_atama.ad_uzunluk,
                                     &alan_tip_d);
        if (idx < 0) {
            fprintf(g->out, "  ; HATA: alan bulunamadi\n");
            continue;
        }
        const char *alan_ir = ast_tip_to_ir(g, alan_tip_d);
        IfadeSonuc deger = ifade_uret(g, aa->veri.alan_atama.deger, alan_ir);
        int dr = deger.reg;
        if (!tip_kesirli_mi(alan_ir) && !tip_kesirli_mi(deger.tip)
            && tip_genisligi(alan_ir) > 0 && tip_genisligi(deger.tip) > 0) {
            dr = int_donustur(g, deger.reg, deger.tip, alan_ir);
        }
        int gep_r = yeni_reg(g);
        fprintf(g->out, "  %%%d = getelementptr %s, ptr %%%d, i32 0, i32 %d\n",
                gep_r, yapi_ir, alloca_r, idx);
        fprintf(g->out, "  store %s %%%d, ptr %%%d\n", alan_ir, dr, gep_r);
    }

    /* Struct degerini yukle (by-value akis icin) */
    int load_r = yeni_reg(g);
    fprintf(g->out, "  %%%d = load %s, ptr %%%d\n", load_r, yapi_ir, alloca_r);
    IfadeSonuc s = { load_r, yapi_ir };
    return s;
}

/* DUGUM_ERISIM -> struct value uzerinde extractvalue, ptr uzerinde GEP+load */
static IfadeSonuc erisim_uret(LlvmGen *g, const Dugum *d) {
    IfadeSonuc nesne = ifade_uret(g, d->veri.erisim.nesne, NULL);

    /* Yapi tipini cikar: nesne.tip "%Ad" ise yapi adi, "ptr" ise ad arama */
    YapiKayit *yk = NULL;
    if (nesne.tip && nesne.tip[0] == '%') {
        yk = yapi_bul(g, nesne.tip + 1, (int)strlen(nesne.tip + 1));
    } else {
        /* Konservatif: alan adina gore en uygun yapiyi bul */
        for (YapiKayit *y = g->yapilar; y && !yk; y = y->sonraki) {
            if (yapi_alan_indeksi(y, d->veri.erisim.alan,
                                   d->veri.erisim.alan_uzunluk, NULL) >= 0) {
                yk = y;
            }
        }
    }
    if (!yk) return hata(g, "erisim: yapi tipi cozulemedi");

    const Dugum *alan_tip_d = NULL;
    int idx = yapi_alan_indeksi(yk, d->veri.erisim.alan,
                                 d->veri.erisim.alan_uzunluk, &alan_tip_d);
    if (idx < 0) return hata(g, "alan bulunamadi");
    const char *alan_ir = ast_tip_to_ir(g, alan_tip_d);

    /* Struct value ise: extractvalue */
    if (nesne.tip && nesne.tip[0] == '%') {
        int r = yeni_reg(g);
        fprintf(g->out, "  %%%d = extractvalue %s %%%d, %d\n",
                r, nesne.tip, nesne.reg, idx);
        IfadeSonuc s = { r, alan_ir };
        return s;
    }
    /* Ptr ise: GEP + load */
    int gep_r = yeni_reg(g);
    fprintf(g->out, "  %%%d = getelementptr %%", gep_r);
    ad_yaz(g->out, yk->ad, yk->ad_uz);
    fprintf(g->out, ", ptr %%%d, i32 0, i32 %d\n", nesne.reg, idx);
    int load_r = yeni_reg(g);
    fprintf(g->out, "  %%%d = load %s, ptr %%%d\n",
            load_r, alan_ir, gep_r);
    IfadeSonuc s = { load_r, alan_ir };
    return s;
}

static IfadeSonuc ifade_uret(LlvmGen *g, const Dugum *d,
                              const char *beklenen) {
    if (!d) {
        IfadeSonuc s = { yeni_reg(g), "i32" };
        fprintf(g->out, "  %%%d = add i32 0, 0\n", s.reg);
        return s;
    }

    switch (d->tip) {
        case DUGUM_TAM: {
            const char *tip = beklenen ? beklenen : "i32";
            int w = tip_genisligi(tip);
            int kf = tip_kesirli_mi(tip);
            if (w == 0 && !kf) tip = "i32";
            int r = yeni_reg(g);
            if (kf) {
                fprintf(g->out, "  %%%d = fadd %s 0.0, %" PRId64 ".0\n",
                        r, tip, d->veri.tam.deger);
            } else {
                fprintf(g->out, "  %%%d = add %s 0, %" PRId64 "\n",
                        r, tip, d->veri.tam.deger);
            }
            IfadeSonuc s = { r, tip };
            return s;
        }

        case DUGUM_MANTIKSAL: {
            const char *tip = "i1";
            if (beklenen && strcmp(beklenen, "i1") != 0) tip = "i32";
            int r = yeni_reg(g);
            if (strcmp(tip, "i1") == 0) {
                fprintf(g->out, "  %%%d = or i1 %d, 0\n",
                        r, d->veri.mantiksal.deger ? 1 : 0);
            } else {
                fprintf(g->out, "  %%%d = add %s 0, %d\n",
                        r, tip, d->veri.mantiksal.deger ? 1 : 0);
            }
            IfadeSonuc s = { r, tip };
            return s;
        }

        case DUGUM_KARAKTER: {
            int r = yeni_reg(g);
            fprintf(g->out, "  %%%d = add i32 0, %u\n",
                    r, d->veri.karakter.kod_noktasi);
            IfadeSonuc s = { r, "i32" };
            return s;
        }

        case DUGUM_KESIRLI: {
            const char *tip = beklenen && tip_kesirli_mi(beklenen)
                ? beklenen : "double";
            int r = yeni_reg(g);
            /* LLVM IR ondalik literali her zaman decimal point gerektirir.
             * %g 2.0'i "2" yapabilir — emin olmak icin formatla, sonra
             * gerekirse ".0" ekle. */
            char buf[64];
            int n = snprintf(buf, sizeof(buf), "%g", d->veri.kesirli.deger);
            int decimal_var = 0;
            for (int i = 0; i < n; i++) {
                if (buf[i] == '.' || buf[i] == 'e' ||
                    buf[i] == 'E' || buf[i] == 'n' /* nan/inf */) {
                    decimal_var = 1; break;
                }
            }
            if (!decimal_var && n + 3 < (int)sizeof(buf)) {
                buf[n] = '.'; buf[n+1] = '0'; buf[n+2] = '\0';
            }
            fprintf(g->out, "  %%%d = fadd %s 0.0, %s\n", r, tip, buf);
            IfadeSonuc s = { r, tip };
            return s;
        }

        case DUGUM_METIN:
            return metin_lit_uret(g, d);

        case DUGUM_TANIMLAYICI:
            return tanimlayici_yukle(g, d);

        case DUGUM_YAPI_OLUSTUR:
            return yapi_olustur_uret(g, d);

        case DUGUM_ERISIM:
            return erisim_uret(g, d);

        case DUGUM_DIZI_OLUSTUR: {
            /* [e1, e2, ...] -> alloca [N x T] + store + return ptr
             *
             * SSA register sirasi onemli: alloca emit etmeden once eleman
             * degerlerini hesaplayamayiz cunku alloca_reg = %N rezerve eder
             * ama eleman ifadeleri %N+1 vs uretir. Cozum: ilk elemani once
             * degerlendir (tipini ogren), sonra alloca'yi ayri reg ile yaz.
             * Aslinda en kolay: alloca'yi ilk eleman'dan SONRA tahsis et,
             * yani register sirasinda dogru cikar. */
            int n = d->veri.dizi_olustur.sayi;
            const char *elem_ir = "i32";

            if (n == 0) {
                int alloca_r = yeni_reg(g);
                fprintf(g->out, "  %%%d = alloca [0 x i8]\n", alloca_r);
                IfadeSonuc s = { alloca_r, "ptr" };
                return s;
            }

            /* Once tum elemanlari hesapla — register'lar peshape consume edilir */
            IfadeSonuc *vals = (IfadeSonuc *)arena_ayir(g->arena,
                sizeof(IfadeSonuc) * (size_t)n);
            for (int i = 0; i < n; i++) {
                vals[i] = ifade_uret(g, d->veri.dizi_olustur.elemanlar[i],
                                     i == 0 ? NULL : elem_ir);
                if (i == 0) elem_ir = vals[0].tip;
            }
            /* Simdi alloca */
            int alloca_r = yeni_reg(g);
            fprintf(g->out, "  %%%d = alloca [%d x %s]\n",
                    alloca_r, n, elem_ir);
            /* Store her elemani */
            for (int i = 0; i < n; i++) {
                int er = int_donustur(g, vals[i].reg, vals[i].tip, elem_ir);
                int gepi = yeni_reg(g);
                fprintf(g->out,
                    "  %%%d = getelementptr [%d x %s], ptr %%%d, i32 0, i32 %d\n",
                    gepi, n, elem_ir, alloca_r, i);
                fprintf(g->out, "  store %s %%%d, ptr %%%d\n",
                        elem_ir, er, gepi);
            }
            IfadeSonuc s = { alloca_r, "ptr" };
            return s;
        }

        case DUGUM_INDEKS: {
            /* arr[i] -> GEP ptr (T*) + load
             * Eleman tipini bilmiyoruz — varsayilan i32. Ileride tip
             * kontrolden veri akisi gerek (v3 sinirlamasi). */
            IfadeSonuc nesne = ifade_uret(g, d->veri.indeks.nesne, NULL);
            IfadeSonuc idx = ifade_uret(g, d->veri.indeks.indeks, "i64");
            int idx_r = int_donustur(g, idx.reg, idx.tip, "i64");
            const char *elem_ir = beklenen ? beklenen : "i32";
            int gep_r = yeni_reg(g);
            /* opaque pointer arithmetic: getelementptr T, ptr, idx */
            fprintf(g->out,
                "  %%%d = getelementptr %s, ptr %%%d, i64 %%%d\n",
                gep_r, elem_ir, nesne.reg, idx_r);
            int load_r = yeni_reg(g);
            fprintf(g->out, "  %%%d = load %s, ptr %%%d\n",
                    load_r, elem_ir, gep_r);
            IfadeSonuc s = { load_r, elem_ir };
            return s;
        }

        case DUGUM_IKILI: {
            const char *cmp_i = icmp_pred(d->veri.ikili.op);
            const char *op_beklenen = cmp_i ? NULL : beklenen;
            IfadeSonuc sol = ifade_uret(g, d->veri.ikili.sol, op_beklenen);
            IfadeSonuc sag = ifade_uret(g, d->veri.ikili.sag, sol.tip);
            /* Tipler ayniysa donusum yok; degilse int donusumu (float ile karisma yok) */
            int sag_r = sag.reg;
            if (!tip_kesirli_mi(sol.tip)) {
                sag_r = int_donustur(g, sag.reg, sag.tip, sol.tip);
            }
            int kesirli = tip_kesirli_mi(sol.tip);
            if (cmp_i) {
                int r_cmp = yeni_reg(g);
                if (kesirli) {
                    const char *cmp_f = fcmp_pred(d->veri.ikili.op);
                    fprintf(g->out, "  %%%d = fcmp %s %s %%%d, %%%d\n",
                            r_cmp, cmp_f ? cmp_f : "oeq",
                            sol.tip, sol.reg, sag_r);
                } else {
                    fprintf(g->out, "  %%%d = icmp %s %s %%%d, %%%d\n",
                            r_cmp, cmp_i, sol.tip, sol.reg, sag_r);
                }
                IfadeSonuc s = { r_cmp, "i1" };
                return s;
            }
            const char *op = NULL;
            switch (d->veri.ikili.op) {
                case OP_ARTI:  op = kesirli ? "fadd" : "add"; break;
                case OP_EKSI:  op = kesirli ? "fsub" : "sub"; break;
                case OP_CARPI: op = kesirli ? "fmul" : "mul"; break;
                case OP_BOLU:  op = kesirli ? "fdiv" : "sdiv"; break;
                case OP_MOD:   op = kesirli ? "frem" : "srem"; break;
                case OP_VE:    op = "and"; break;
                case OP_VEYA:  op = "or"; break;
                /* Bit operatorleri — page table / kripto codegen */
                case OP_BIT_VE:      op = "and"; break;
                case OP_BIT_VEYA:    op = "or"; break;
                case OP_BIT_OZVEYA:  op = "xor"; break;
                case OP_SOLA_KAYDIR: op = "shl"; break;
                case OP_SAGA_KAYDIR: op = "ashr"; break;  /* signed; dtamX
                                                            icin lshr ileride */
                default:
                    fputs("  ; ikili op desteklenmiyor\n", g->out);
                    return sol;
            }
            int r = yeni_reg(g);
            fprintf(g->out, "  %%%d = %s %s %%%d, %%%d\n",
                    r, op, sol.tip, sol.reg, sag_r);
            IfadeSonuc s = { r, sol.tip };
            return s;
        }

        case DUGUM_TEKLI: {
            IfadeSonuc op_s = ifade_uret(g, d->veri.tekli.operand, beklenen);
            if (d->veri.tekli.op == OP_NEG) {
                int r = yeni_reg(g);
                if (tip_kesirli_mi(op_s.tip)) {
                    fprintf(g->out, "  %%%d = fsub %s 0.0, %%%d\n",
                            r, op_s.tip, op_s.reg);
                } else {
                    fprintf(g->out, "  %%%d = sub %s 0, %%%d\n",
                            r, op_s.tip, op_s.reg);
                }
                IfadeSonuc s = { r, op_s.tip };
                return s;
            }
            if (d->veri.tekli.op == OP_DEGIL) {
                /* op_s i1 ise xor 1; aksi halde icmp eq 0 */
                int r;
                if (strcmp(op_s.tip, "i1") == 0) {
                    r = yeni_reg(g);
                    fprintf(g->out, "  %%%d = xor i1 %%%d, true\n",
                            r, op_s.reg);
                    IfadeSonuc s = { r, "i1" };
                    return s;
                }
                r = yeni_reg(g);
                fprintf(g->out, "  %%%d = icmp eq %s %%%d, 0\n",
                        r, op_s.tip, op_s.reg);
                IfadeSonuc s = { r, "i1" };
                return s;
            }
            if (d->veri.tekli.op == OP_BIT_DEGIL) {
                /* ~x = xor TYPE x, -1 (tum bitleri ters cevir) */
                int r = yeni_reg(g);
                fprintf(g->out, "  %%%d = xor %s %%%d, -1\n",
                        r, op_s.tip, op_s.reg);
                IfadeSonuc s = { r, op_s.tip };
                return s;
            }
            fputs("  ; tekli op desteklenmiyor\n", g->out);
            return op_s;
        }

        case DUGUM_CAGRI: {
            /* Method dispatch: hedef DUGUM_ERISIM ise (x.method())
             * receiver'i ilk arg olarak gec. Method adi alan_adi. */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_ERISIM) {
                const Dugum *erisim = d->veri.cagri.hedef;
                IfadeSonuc receiver = ifade_uret(g,
                    erisim->veri.erisim.nesne, NULL);
                int n = d->veri.cagri.sayi;
                IfadeSonuc *args = (IfadeSonuc *)arena_ayir(g->arena,
                    sizeof(IfadeSonuc) * (size_t)(n + 1));
                args[0] = receiver;
                for (int i = 0; i < n; i++) {
                    args[i + 1] = ifade_uret(g, d->veri.cagri.argumanlar[i], NULL);
                }
                /* Method adi: erisim.alan */
                const char *m_ad = erisim->veri.erisim.alan;
                int m_ad_uz = erisim->veri.erisim.alan_uzunluk;
                IslevKayit *mik = islev_bul(g, m_ad, m_ad_uz);
                const char *donus = mik ? mik->donus_tip : "i32";
                if (strcmp(donus, "void") == 0) donus = "i32";
                int r = yeni_reg(g);
                fprintf(g->out, "  %%%d = call %s @", r, donus);
                ad_yaz(g->out, m_ad, m_ad_uz);
                fputs("(", g->out);
                for (int i = 0; i < n + 1; i++) {
                    if (i > 0) fputs(", ", g->out);
                    fprintf(g->out, "%s %%%d", args[i].tip, args[i].reg);
                }
                fputs(")\n", g->out);
                IfadeSonuc s = { r, donus };
                return s;
            }
            if (!d->veri.cagri.hedef ||
                d->veri.cagri.hedef->tip != DUGUM_TANIMLAYICI) {
                return hata(g, "cagri hedefi tanimlayici degil");
            }
            int n = d->veri.cagri.sayi;
            IfadeSonuc *args = NULL;
            if (n > 0) {
                args = (IfadeSonuc *)arena_ayir(g->arena,
                    sizeof(IfadeSonuc) * (size_t)n);
                for (int i = 0; i < n; i++) {
                    args[i] = ifade_uret(g, d->veri.cagri.argumanlar[i], NULL);
                }
            }
            IslevKayit *ik = islev_bul(g,
                d->veri.cagri.hedef->veri.tanimlayici.metin,
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk);

            const char *cagri_adi = d->veri.cagri.hedef->veri.tanimlayici.metin;
            int cagri_adi_uz = d->veri.cagri.hedef->veri.tanimlayici.uzunluk;

            /* === Dizi dinamik allocator (Kirmizi B) ===
             * dizi_olustur(N) -> kdl_dizi_olustur_genel(eleman_byte, N)
             * dizi_ekle(d, e) -> kdl_dizi_ekle_<bitlen>(d, e)
             */
            if (cagri_adi_uz == 12 &&
                memcmp(cagri_adi, "dizi_olustur", 12) == 0 && n == 1) {
                /* Eleman byte: beklenen tipinden cikarilir. ptr (Dizi<T>) yoksa
                 * 4 (tam32) varsayim. Su an: 4 sabit (v1 limitasyon — beklenen
                 * eleman tipini parametre adindan cikarmiyoruz; ileride D ile). */
                int eleman_byte = 4;
                /* arg[0] tam64 olmali — gerekirse sext (ONCE, sonra yeni_reg) */
                int n_reg = int_donustur(g, args[0].reg, args[0].tip, "i64");
                int r = yeni_reg(g);
                fprintf(g->out,
                    "  %%%d = call ptr @kdl_dizi_olustur_genel(i32 %d, i64 %%%d)\n",
                    r, eleman_byte, n_reg);
                IfadeSonuc s = { r, "ptr" };
                return s;
            }
            if (cagri_adi_uz == 9 &&
                memcmp(cagri_adi, "dizi_ekle", 9) == 0 && n == 2) {
                /* args[1] tipinden bitlen cikarilir */
                const char *e_tip = args[1].tip;
                const char *suffix = "32";
                if (strcmp(e_tip, "i8") == 0) suffix = "8";
                else if (strcmp(e_tip, "i16") == 0) suffix = "16";
                else if (strcmp(e_tip, "i32") == 0) suffix = "32";
                else if (strcmp(e_tip, "i64") == 0) suffix = "64";
                fprintf(g->out,
                    "  call void @kdl_dizi_ekle_%s(ptr %%%d, %s %%%d)\n",
                    suffix, args[0].reg, e_tip, args[1].reg);
                /* Bos (void) doner — i32 0 dummy ile sarmala */
                int r = yeni_reg(g);
                fprintf(g->out, "  %%%d = add i32 0, 0\n", r);
                IfadeSonuc s = { r, "i32" };
                return s;
            }
            if (cagri_adi_uz == 7 &&
                memcmp(cagri_adi, "dizi_al", 7) == 0 && n == 2) {
                /* Beklenen donus tipinden (varsa) suffix cikariliyor;
                 * yoksa i32 varsayim. Kullanim noktasi: degisken/atama hedef. */
                const char *suffix = "32";
                const char *donus_tip = "i32";
                if (beklenen) {
                    if (strcmp(beklenen, "i8") == 0) { suffix = "8"; donus_tip = "i8"; }
                    else if (strcmp(beklenen, "i16") == 0) { suffix = "16"; donus_tip = "i16"; }
                    else if (strcmp(beklenen, "i32") == 0) { suffix = "32"; donus_tip = "i32"; }
                    else if (strcmp(beklenen, "i64") == 0) { suffix = "64"; donus_tip = "i64"; }
                }
                int i_reg = int_donustur(g, args[1].reg, args[1].tip, "i32");
                int r = yeni_reg(g);
                fprintf(g->out,
                    "  %%%d = call %s @kdl_dizi_al_%s(ptr %%%d, i32 %%%d)\n",
                    r, donus_tip, suffix, args[0].reg, i_reg);
                IfadeSonuc s = { r, donus_tip };
                return s;
            }
            if (cagri_adi_uz == 10 &&
                memcmp(cagri_adi, "dizi_boyut", 10) == 0 && n == 1) {
                int r = yeni_reg(g);
                fprintf(g->out,
                    "  %%%d = call i32 @kdl_dizi_boyut(ptr %%%d)\n",
                    r, args[0].reg);
                IfadeSonuc s = { r, "i32" };
                return s;
            }

            /* Built-in libc mapping */
            if (cagri_adi_uz == 6 && memcmp(cagri_adi, "yazdir", 6) == 0) {
                cagri_adi = "puts"; cagri_adi_uz = 4;
            } else if (cagri_adi_uz == 9 && memcmp(cagri_adi, "bellek_al", 9) == 0) {
                cagri_adi = "malloc"; cagri_adi_uz = 6;
            } else if (cagri_adi_uz == 14 &&
                       memcmp(cagri_adi, "bellek_serbest", 14) == 0) {
                cagri_adi = "free"; cagri_adi_uz = 4;
            } else if (cagri_adi_uz == 14 &&
                       memcmp(cagri_adi, "bellek_kopyala", 14) == 0) {
                cagri_adi = "memcpy"; cagri_adi_uz = 6;
            }
            /* Metin runtime primitifleri (Kirmizi A) -> kdl_* */
            else if (cagri_adi_uz == 13 &&
                     memcmp(cagri_adi, "metin_uzunluk", 13) == 0) {
                cagri_adi = "kdl_metin_uzunluk"; cagri_adi_uz = 17;
            } else if (cagri_adi_uz == 15 &&
                       memcmp(cagri_adi, "metin_birlestir", 15) == 0) {
                cagri_adi = "kdl_metin_birlestir"; cagri_adi_uz = 19;
            } else if (cagri_adi_uz == 9 &&
                       memcmp(cagri_adi, "metin_kes", 9) == 0) {
                cagri_adi = "kdl_metin_kes"; cagri_adi_uz = 13;
            } else if (cagri_adi_uz == 14 &&
                       memcmp(cagri_adi, "metin_k\xc3\xbc\xc3\xa7\xc3\xbck", 14) == 0) {
                cagri_adi = "kdl_metin_kucuk"; cagri_adi_uz = 15;
            } else if (cagri_adi_uz == 13 &&
                       memcmp(cagri_adi, "metin_b\xc3\xbcy\xc3\xbck", 13) == 0) {
                cagri_adi = "kdl_metin_buyuk"; cagri_adi_uz = 15;
            } else if (cagri_adi_uz == 13 &&
                       memcmp(cagri_adi, "metin_i\xc3\xa7" "erir", 13) == 0) {
                cagri_adi = "kdl_metin_icerir"; cagri_adi_uz = 16;
            } else if (cagri_adi_uz == 13 &&
                       memcmp(cagri_adi, "metin_ba\xc5\x9flar", 13) == 0) {
                cagri_adi = "kdl_metin_baslar"; cagri_adi_uz = 16;
            } else if (cagri_adi_uz == 11 &&
                       memcmp(cagri_adi, "metin_biter", 11) == 0) {
                cagri_adi = "kdl_metin_biter"; cagri_adi_uz = 15;
            } else if (cagri_adi_uz == 11 &&
                       memcmp(cagri_adi, "metin_k\xc4\xb1rp", 11) == 0) {
                cagri_adi = "kdl_metin_kirp"; cagri_adi_uz = 14;
            } else if (cagri_adi_uz == 20 &&
                       memcmp(cagri_adi, "metin_yer_de\xc4\x9fi\xc5\x9ftir", 20) == 0) {
                cagri_adi = "kdl_metin_yer_degistir"; cagri_adi_uz = 22;
            }
            /* Dosya syscall layer (Kirmizi G) -> kdl_dosya_* */
            else if (cagri_adi_uz == 8 &&
                     memcmp(cagri_adi, "dosya_ac", 8) == 0) {
                cagri_adi = "kdl_dosya_ac"; cagri_adi_uz = 12;
            } else if (cagri_adi_uz == 9 &&
                       memcmp(cagri_adi, "dosya_oku", 9) == 0) {
                cagri_adi = "kdl_dosya_tumu_oku"; cagri_adi_uz = 18;
            } else if (cagri_adi_uz == 9 &&
                       memcmp(cagri_adi, "dosya_yaz", 9) == 0) {
                cagri_adi = "kdl_dosya_yaz"; cagri_adi_uz = 13;
            } else if (cagri_adi_uz == 11 &&
                       memcmp(cagri_adi, "dosya_kapat", 11) == 0) {
                cagri_adi = "kdl_dosya_kapat"; cagri_adi_uz = 15;
            } else if (cagri_adi_uz == 12 &&
                       memcmp(cagri_adi, "dosya_var_mi", 12) == 0) {
                cagri_adi = "kdl_dosya_var_mi"; cagri_adi_uz = 16;
            } else if (cagri_adi_uz == 9 &&
                       memcmp(cagri_adi, "dosya_sil", 9) == 0) {
                cagri_adi = "kdl_dosya_sil"; cagri_adi_uz = 13;
            } else if (cagri_adi_uz == 22 &&
                       memcmp(cagri_adi, "dosya_yeniden_adlandir", 22) == 0) {
                cagri_adi = "kdl_dosya_yeniden_adlandir"; cagri_adi_uz = 26;
            } else if (cagri_adi_uz == 11 &&
                       memcmp(cagri_adi, "dosya_boyut", 11) == 0) {
                cagri_adi = "kdl_dosya_boyut"; cagri_adi_uz = 15;
            }

            const char *donus = ik ? ik->donus_tip : (beklenen ? beklenen : "i32");

            /* Generic islev: tip args'i arg tipinden cikar, specialize et */
            if (ik && ik->generic_mi && ik->ast) {
                const Dugum *gislev = ik->ast;
                int tps = gislev->veri.islev.tip_param_sayi;
                const char **tip_args = (const char **)arena_ayir(g->arena,
                    sizeof(const char *) * (size_t)tps);
                /* Her generic param icin: parametrelerde T'yi bulan ilk arg
                 * tipinden çıkar */
                for (int ti = 0; ti < tps; ti++) {
                    const char *tp = gislev->veri.islev.tip_paramlar[ti];
                    int tp_uz = (int)strlen(tp);
                    const char *inferred = NULL;
                    for (int pi = 0; pi < gislev->veri.islev.param_sayi &&
                                       pi < n && !inferred; pi++) {
                        const Dugum *p = gislev->veri.islev.parametreler[pi];
                        if (p->veri.parametre.tip &&
                            p->veri.parametre.tip->tip == DUGUM_TIP_BASIT) {
                            const char *pad = p->veri.parametre.tip->veri.tip_basit.ad;
                            int puz = p->veri.parametre.tip->veri.tip_basit.ad_uzunluk;
                            if (puz == tp_uz && memcmp(pad, tp, (size_t)tp_uz) == 0) {
                                inferred = args[pi].tip;
                            }
                        }
                    }
                    tip_args[ti] = inferred ? inferred : "i32";
                }
                /* Mangled name */
                const char *mangled = mangle_et(g,
                    gislev->veri.islev.ad, gislev->veri.islev.ad_uzunluk,
                    tip_args, tps);

                /* Specialization bekleyenlere ekle (henuz emit edilmediyse) */
                if (!mono_emitlendi(g, mangled)) {
                    int z_bekleyen = 0;
                    for (BekleyenSpec *b = g->bekleyenler; b; b = b->sonraki) {
                        if (strcmp(b->mangled, mangled) == 0) {
                            z_bekleyen = 1; break;
                        }
                    }
                    if (!z_bekleyen) {
                        BekleyenSpec *bs = (BekleyenSpec *)arena_ayir_sifir(
                            g->arena, sizeof(BekleyenSpec));
                        if (bs) {
                            bs->ast = gislev;
                            bs->mangled = mangled;
                            bs->tip_arg_sayi = tps;
                            const char **kopya = (const char **)arena_ayir(
                                g->arena, sizeof(const char *) * (size_t)tps);
                            for (int j = 0; j < tps; j++) kopya[j] = tip_args[j];
                            bs->tip_args = kopya;
                            bs->sonraki = g->bekleyenler;
                            g->bekleyenler = bs;
                        }
                    }
                }

                int r = yeni_reg(g);
                fprintf(g->out, "  %%%d = call %s @%s(", r, donus, mangled);
                for (int i = 0; i < n; i++) {
                    if (i > 0) fputs(", ", g->out);
                    fprintf(g->out, "%s %%%d", args[i].tip, args[i].reg);
                }
                fputs(")\n", g->out);
                /* Generic islev'in donus tipi T olabilir — inferred tipini al */
                const char *donus_t = donus;
                if (gislev->veri.islev.donus_tipi &&
                    gislev->veri.islev.donus_tipi->tip == DUGUM_TIP_BASIT) {
                    const char *dad = gislev->veri.islev.donus_tipi->veri.tip_basit.ad;
                    int duz = gislev->veri.islev.donus_tipi->veri.tip_basit.ad_uzunluk;
                    for (int ti = 0; ti < tps; ti++) {
                        const char *tp = gislev->veri.islev.tip_paramlar[ti];
                        int tp_uz = (int)strlen(tp);
                        if (duz == tp_uz &&
                            memcmp(dad, tp, (size_t)tp_uz) == 0) {
                            donus_t = tip_args[ti];
                            break;
                        }
                    }
                }
                IfadeSonuc s = { r, donus_t };
                return s;
            }

            if (strcmp(donus, "void") == 0) donus = "i32";
            int r = yeni_reg(g);
            fprintf(g->out, "  %%%d = call %s @", r, donus);
            ad_yaz(g->out, cagri_adi, cagri_adi_uz);
            fputs("(", g->out);
            for (int i = 0; i < n; i++) {
                if (i > 0) fputs(", ", g->out);
                fprintf(g->out, "%s %%%d", args[i].tip, args[i].reg);
            }
            fputs(")\n", g->out);
            IfadeSonuc s = { r, donus };
            return s;
        }

        case DUGUM_OLARAK: {
            /* x olarak T — Kirmizi E explicit cast */
            IfadeSonuc kaynak = ifade_uret(g, d->veri.olarak.kaynak, NULL);
            const char *hedef_ir = ast_tip_to_ir(g, d->veri.olarak.hedef_tip);
            if (!hedef_ir) hedef_ir = "i32";
            /* Eger zaten ayni tip ise no-op */
            if (strcmp(kaynak.tip, hedef_ir) == 0) {
                return kaynak;
            }
            /* int <-> int: int_donustur (sext/trunc/zext) */
            int k_int = (strcmp(kaynak.tip, "i1") == 0 ||
                          strcmp(kaynak.tip, "i8") == 0 ||
                          strcmp(kaynak.tip, "i16") == 0 ||
                          strcmp(kaynak.tip, "i32") == 0 ||
                          strcmp(kaynak.tip, "i64") == 0);
            int h_int = (strcmp(hedef_ir, "i1") == 0 ||
                          strcmp(hedef_ir, "i8") == 0 ||
                          strcmp(hedef_ir, "i16") == 0 ||
                          strcmp(hedef_ir, "i32") == 0 ||
                          strcmp(hedef_ir, "i64") == 0);
            int k_kesirli = (strcmp(kaynak.tip, "float") == 0 ||
                              strcmp(kaynak.tip, "double") == 0);
            int h_kesirli = (strcmp(hedef_ir, "float") == 0 ||
                              strcmp(hedef_ir, "double") == 0);

            if (k_int && h_int) {
                int rr = int_donustur(g, kaynak.reg, kaynak.tip, hedef_ir);
                IfadeSonuc sn = { rr, hedef_ir };
                return sn;
            }
            if (k_kesirli && h_kesirli) {
                int rr = yeni_reg(g);
                /* float <-> double: fpext/fptrunc */
                const char *op = "fpext";
                if (strcmp(kaynak.tip, "double") == 0 &&
                    strcmp(hedef_ir, "float") == 0) op = "fptrunc";
                fprintf(g->out, "  %%%d = %s %s %%%d to %s\n",
                        rr, op, kaynak.tip, kaynak.reg, hedef_ir);
                IfadeSonuc sn = { rr, hedef_ir };
                return sn;
            }
            if (k_int && h_kesirli) {
                /* signed int -> float/double: sitofp */
                int rr = yeni_reg(g);
                fprintf(g->out, "  %%%d = sitofp %s %%%d to %s\n",
                        rr, kaynak.tip, kaynak.reg, hedef_ir);
                IfadeSonuc sn = { rr, hedef_ir };
                return sn;
            }
            if (k_kesirli && h_int) {
                /* float/double -> signed int: fptosi */
                int rr = yeni_reg(g);
                fprintf(g->out, "  %%%d = fptosi %s %%%d to %s\n",
                        rr, kaynak.tip, kaynak.reg, hedef_ir);
                IfadeSonuc sn = { rr, hedef_ir };
                return sn;
            }
            /* Diger durumlar: no-op (tip kontrol gecmis olmali) */
            return kaynak;
        }

        default: {
            int r = yeni_reg(g);
            fprintf(g->out, "  ; ifade tipi %d desteklenmiyor\n", d->tip);
            fprintf(g->out, "  %%%d = add i32 0, 0\n", r);
            IfadeSonuc s = { r, "i32" };
            return s;
        }
    }
}

/* Kosul ifadesini i1'e indirge (eger/iken icin) */
static int kosul_i1(LlvmGen *g, const Dugum *d) {
    IfadeSonuc s = ifade_uret(g, d, "i1");
    if (strcmp(s.tip, "i1") == 0) return s.reg;
    /* int -> i1: kosul != 0 */
    int r = yeni_reg(g);
    fprintf(g->out, "  %%%d = icmp ne %s %%%d, 0\n", r, s.tip, s.reg);
    return r;
}

/* === Deyim IR === */

/* Donus: blok 'ver' (terminator) ile bitti mi? */
static int deyim_uret_terminated(LlvmGen *g, const Dugum *d,
                                  const char *donus_tip) {
    if (!d) return 0;

    switch (d->tip) {
        case DUGUM_VER: {
            if (d->veri.ver.deger) {
                IfadeSonuc s = ifade_uret(g, d->veri.ver.deger, donus_tip);
                int rr = int_donustur(g, s.reg, s.tip, donus_tip);
                fprintf(g->out, "  ret %s %%%d\n", donus_tip, rr);
            } else {
                if (donus_tip && strcmp(donus_tip, "void") == 0) {
                    fputs("  ret void\n", g->out);
                } else {
                    fprintf(g->out, "  ret %s 0\n",
                            donus_tip ? donus_tip : "i32");
                }
            }
            return 1;
        }

        case DUGUM_DEGISKEN: {
            const char *annot = NULL;
            if (d->veri.degisken.tip) {
                annot = ast_tip_to_ir(g, d->veri.degisken.tip);
            }
            const char *tip = annot;

            /* Eger annot yoksa, deger ifadesini once degerlendirip
             * tipini cikariyoruz, sonra alloca'yi dogru tipte yapiyoruz.
             * Bu sira hatasi onler (ozellikle yapi_olustur -> ptr donus). */
            if (d->veri.degisken.deger) {
                IfadeSonuc dv;
                if (annot) {
                    /* Annot var: dogrudan alloca, sonra store */
                    int alloca_reg = yeni_reg(g);
                    fprintf(g->out, "  %%%d = alloca %s\n", alloca_reg, tip);
                    dv = ifade_uret(g, d->veri.degisken.deger, tip);
                    int rr = int_donustur(g, dv.reg, dv.tip, tip);
                    fprintf(g->out, "  store %s %%%d, ptr %%%d\n",
                            tip, rr, alloca_reg);
                    isim_ekle(g, d->veri.degisken.ad,
                              d->veri.degisken.ad_uzunluk,
                              1, alloca_reg, tip);
                } else {
                    /* Annot yok: deger once, sonra alloca */
                    dv = ifade_uret(g, d->veri.degisken.deger, NULL);
                    tip = dv.tip;
                    int alloca_reg = yeni_reg(g);
                    fprintf(g->out, "  %%%d = alloca %s\n", alloca_reg, tip);
                    fprintf(g->out, "  store %s %%%d, ptr %%%d\n",
                            tip, dv.reg, alloca_reg);
                    isim_ekle(g, d->veri.degisken.ad,
                              d->veri.degisken.ad_uzunluk,
                              1, alloca_reg, tip);
                }
            } else {
                /* Deger yok, sadece annot ile alloca */
                if (!tip) tip = "i32";
                int alloca_reg = yeni_reg(g);
                fprintf(g->out, "  %%%d = alloca %s\n", alloca_reg, tip);
                isim_ekle(g, d->veri.degisken.ad,
                          d->veri.degisken.ad_uzunluk,
                          1, alloca_reg, tip);
            }
            return 0;
        }

        case DUGUM_ATAMA: {
            if (d->veri.atama.hedef &&
                d->veri.atama.hedef->tip == DUGUM_TANIMLAYICI) {
                LlvmIsim *i = isim_bul(g,
                    d->veri.atama.hedef->veri.tanimlayici.metin,
                    d->veri.atama.hedef->veri.tanimlayici.uzunluk);
                if (i) {
                    IfadeSonuc v = ifade_uret(g, d->veri.atama.deger,
                                               i->llvm_tip);
                    int rr = int_donustur(g, v.reg, v.tip, i->llvm_tip);
                    fprintf(g->out, "  store %s %%%d, ptr %%%d\n",
                            i->llvm_tip, rr, i->reg_no);
                }
            }
            return 0;
        }

        case DUGUM_EGER: {
            int i1r = kosul_i1(g, d->veri.eger.kosul);
            int L_then = yeni_label(g);
            int L_else = yeni_label(g);
            int L_end = yeni_label(g);
            fprintf(g->out, "  br i1 %%%d, label %%bb%d, label %%bb%d\n",
                    i1r, L_then, L_else);
            fprintf(g->out, "bb%d:\n", L_then);
            ScopeMarker m1 = scope_gir(g);
            int then_term = 0;
            if (d->veri.eger.gozdoldur) {
                then_term = blok_uret(g, d->veri.eger.gozdoldur);
            }
            scope_cik(g, m1);
            if (!then_term) fprintf(g->out, "  br label %%bb%d\n", L_end);
            fprintf(g->out, "bb%d:\n", L_else);
            int else_term = 0;
            if (d->veri.eger.yan) {
                if (d->veri.eger.yan->tip == DUGUM_BLOK) {
                    ScopeMarker m2 = scope_gir(g);
                    else_term = blok_uret(g, d->veri.eger.yan);
                    scope_cik(g, m2);
                } else if (d->veri.eger.yan->tip == DUGUM_EGER) {
                    else_term = deyim_uret_terminated(g, d->veri.eger.yan,
                                                       donus_tip);
                }
            }
            if (!else_term) fprintf(g->out, "  br label %%bb%d\n", L_end);
            if (then_term && else_term) return 1;
            fprintf(g->out, "bb%d:\n", L_end);
            return 0;
        }

        case DUGUM_IKEN: {
            int L_head = yeni_label(g);
            int L_body = yeni_label(g);
            int L_done = yeni_label(g);
            fprintf(g->out, "  br label %%bb%d\n", L_head);
            fprintf(g->out, "bb%d:\n", L_head);
            int i1r = kosul_i1(g, d->veri.iken.kosul);
            fprintf(g->out, "  br i1 %%%d, label %%bb%d, label %%bb%d\n",
                    i1r, L_body, L_done);
            fprintf(g->out, "bb%d:\n", L_body);
            ScopeMarker m = scope_gir(g);
            int body_term = blok_uret(g, d->veri.iken.govde);
            scope_cik(g, m);
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
            (void)ifade_uret(g, d->veri.ifade_deyimi.ifade, NULL);
            return 0;

        default:
            fprintf(g->out, "  ; deyim tipi %d desteklenmiyor\n", d->tip);
            return 0;
    }
}

/* Active function's return type — global state for blok_uret */
static const char *g_donus_tip = "i32";

static int blok_uret(LlvmGen *g, const Dugum *blok) {
    if (!blok || blok->tip != DUGUM_BLOK) return 0;
    ScopeMarker m = scope_gir(g);
    for (int i = 0; i < blok->veri.blok.sayi; i++) {
        int term = deyim_uret_terminated(g, blok->veri.blok.deyimler[i],
                                          g_donus_tip);
        if (term) {
            scope_cik(g, m);
            return 1;
        }
    }
    scope_cik(g, m);
    return 0;
}

/* === Islev IR === */

static int mono_emitlendi(LlvmGen *g, const char *mangled) {
    for (MonoKayit *m = g->monolar; m; m = m->sonraki) {
        if (strcmp(m->mangled, mangled) == 0) return 1;
    }
    return 0;
}

static void mono_ekle(LlvmGen *g, const char *mangled) {
    MonoKayit *m = (MonoKayit *)arena_ayir(g->arena, sizeof(MonoKayit));
    if (!m) return;
    int uz = (int)strlen(mangled);
    char *kopya = (char *)arena_ayir(g->arena, (size_t)uz + 1);
    memcpy(kopya, mangled, (size_t)uz + 1);
    m->mangled = kopya;
    m->sonraki = g->monolar;
    g->monolar = m;
}

/* Mangled isim: "ad$T1$T2..." — arena'da */
static const char *mangle_et(LlvmGen *g, const char *ad, int ad_uz,
                              const char **tipler, int tip_sayi) {
    int toplam = ad_uz;
    for (int i = 0; i < tip_sayi; i++) {
        toplam += 1 + (int)strlen(tipler[i]);
    }
    char *buf = (char *)arena_ayir(g->arena, (size_t)toplam + 1);
    if (!buf) return ad;
    int o = 0;
    memcpy(buf, ad, (size_t)ad_uz); o += ad_uz;
    for (int i = 0; i < tip_sayi; i++) {
        buf[o++] = '$';
        int tu = (int)strlen(tipler[i]);
        memcpy(buf + o, tipler[i], (size_t)tu);
        o += tu;
        /* '%' karakteri LLVM IR'da gecersiz olabilir mangling'de — '.'ile degistir */
        for (int k = o - tu; k < o; k++) {
            if (buf[k] == '%') buf[k] = '_';
        }
    }
    buf[o] = '\0';
    return buf;
}

/* Generic islev'i belirli tip arglariyla specialize et + emit */
static void islev_uret(LlvmGen *g, const Dugum *islev);

static void specialize_emit(LlvmGen *g, const Dugum *islev,
                            const char **tip_args, int tip_arg_sayi,
                            const char *mangled) {
    if (mono_emitlendi(g, mangled)) return;
    mono_ekle(g, mangled);

    /* Subst push */
    TipSubst *eski_substler = g->substler;
    for (int i = 0; i < islev->veri.islev.tip_param_sayi && i < tip_arg_sayi; i++) {
        TipSubst *s = (TipSubst *)arena_ayir(g->arena, sizeof(TipSubst));
        if (!s) continue;
        s->ad = islev->veri.islev.tip_paramlar[i];
        s->ad_uz = (int)strlen(s->ad);
        s->ir = tip_args[i];
        s->sonraki = g->substler;
        g->substler = s;
    }

    /* Geçici olarak islev'in adini mangled ile degistirip emit */
    Dugum sahte = *islev;
    sahte.veri.islev.ad = mangled;
    sahte.veri.islev.ad_uzunluk = (int)strlen(mangled);
    sahte.veri.islev.tip_param_sayi = 0;  /* artik generic degil */
    islev_uret(g, &sahte);

    /* Subst pop */
    g->substler = eski_substler;
}

static void islev_uret(LlvmGen *g, const Dugum *islev) {
    /* Generic islev: tek basina emit etme — instantiation'lar cagri sirasinda */
    if (islev->veri.islev.tip_param_sayi > 0) return;

    const char *donus = islev->veri.islev.donus_tipi
        ? ast_tip_to_ir(g, islev->veri.islev.donus_tipi)
        : "void";
    if (!donus) donus = "void";
    g_donus_tip = donus;

    fprintf(g->out, "define %s @", donus);
    ad_yaz(g->out, islev->veri.islev.ad, islev->veri.islev.ad_uzunluk);
    fputs("(", g->out);

    /* Parametre listesi */
    int n = islev->veri.islev.param_sayi;
    const char **param_tipler = NULL;
    if (n > 0) {
        param_tipler = (const char **)arena_ayir(g->arena,
            sizeof(const char *) * (size_t)n);
    }
    for (int i = 0; i < n; i++) {
        const Dugum *p = islev->veri.islev.parametreler[i];
        const char *tip = ast_tip_to_ir(g, p->veri.parametre.tip);
        if (!tip) tip = "i32";
        param_tipler[i] = tip;
        if (i > 0) fputs(", ", g->out);
        fprintf(g->out, "%s %%", tip);
        ad_yaz(g->out, p->veri.parametre.ad, p->veri.parametre.ad_uzunluk);
    }
    fputs(") {\nentry:\n", g->out);

    g->reg = 0;
    g->label = 0;
    g->isimler = NULL;

    /* Parametreleri alloca'ya kopyala */
    for (int i = 0; i < n; i++) {
        const Dugum *p = islev->veri.islev.parametreler[i];
        const char *tip = param_tipler[i];
        int alloca_reg = yeni_reg(g);
        fprintf(g->out, "  %%%d = alloca %s\n", alloca_reg, tip);
        fprintf(g->out, "  store %s %%", tip);
        ad_yaz(g->out, p->veri.parametre.ad, p->veri.parametre.ad_uzunluk);
        fprintf(g->out, ", ptr %%%d\n", alloca_reg);
        isim_ekle(g, p->veri.parametre.ad, p->veri.parametre.ad_uzunluk,
                  0, alloca_reg, tip);
    }

    int term = 0;
    if (islev->veri.islev.govde) {
        term = blok_uret(g, islev->veri.islev.govde);
    }
    if (!term) {
        if (strcmp(donus, "void") == 0) {
            fputs("  ret void\n", g->out);
        } else {
            fprintf(g->out, "  ret %s 0\n", donus);
        }
    }
    fputs("}\n\n", g->out);
}

/* === Public API === */

void llvm_ir_uret(const Dugum *program, FILE *out) {
    if (!out) return;
    fputs("; KEMGU LLVM IR (text uretici, ADIM 18 v2 — yapi/metin/multi-int)\n",
          out);
    fputs("; `clang -x ir - -o cikti.exe` ile derlenebilir.\n", out);
    fputs("target triple = \"x86_64-pc-windows-gnu\"\n\n", out);
    /* Built-in extern (libc) bildirimleri */
    fputs("declare i32 @puts(ptr)\n", out);
    fputs("declare ptr @malloc(i64)\n", out);
    fputs("declare void @free(ptr)\n", out);
    fputs("declare ptr @memcpy(ptr, ptr, i64)\n", out);
    /* Metin runtime primitifleri (Kirmizi A) */
    fputs("declare i32 @kdl_metin_uzunluk(ptr)\n", out);
    fputs("declare ptr @kdl_metin_birlestir(ptr, ptr)\n", out);
    fputs("declare ptr @kdl_metin_kes(ptr, i32, i32)\n", out);
    fputs("declare ptr @kdl_metin_kucuk(ptr)\n", out);
    fputs("declare ptr @kdl_metin_buyuk(ptr)\n", out);
    fputs("declare i32 @kdl_metin_icerir(ptr, ptr)\n", out);
    fputs("declare i32 @kdl_metin_baslar(ptr, ptr)\n", out);
    fputs("declare i32 @kdl_metin_biter(ptr, ptr)\n", out);
    fputs("declare ptr @kdl_metin_kirp(ptr)\n", out);
    fputs("declare ptr @kdl_metin_yer_degistir(ptr, ptr, ptr)\n", out);
    /* Dizi dinamik allocator (Kirmizi B) */
    fputs("declare ptr @kdl_dizi_olustur_genel(i32, i64)\n", out);
    fputs("declare void @kdl_dizi_ekle_8(ptr, i8)\n", out);
    fputs("declare void @kdl_dizi_ekle_16(ptr, i16)\n", out);
    fputs("declare void @kdl_dizi_ekle_32(ptr, i32)\n", out);
    fputs("declare void @kdl_dizi_ekle_64(ptr, i64)\n", out);
    fputs("declare i8 @kdl_dizi_al_8(ptr, i32)\n", out);
    fputs("declare i16 @kdl_dizi_al_16(ptr, i32)\n", out);
    fputs("declare i32 @kdl_dizi_al_32(ptr, i32)\n", out);
    fputs("declare i64 @kdl_dizi_al_64(ptr, i32)\n", out);
    fputs("declare i32 @kdl_dizi_boyut(ptr)\n", out);
    /* Dosya syscall layer (Kirmizi G) */
    fputs("declare ptr @kdl_dosya_ac(ptr, ptr)\n", out);
    fputs("declare ptr @kdl_dosya_tumu_oku(ptr)\n", out);
    fputs("declare i32 @kdl_dosya_yaz(ptr, ptr)\n", out);
    fputs("declare void @kdl_dosya_kapat(ptr)\n", out);
    fputs("declare i32 @kdl_dosya_var_mi(ptr)\n", out);
    fputs("declare i32 @kdl_dosya_sil(ptr)\n", out);
    fputs("declare i32 @kdl_dosya_yeniden_adlandir(ptr, ptr)\n", out);
    fputs("declare i64 @kdl_dosya_boyut(ptr)\n\n", out);

    if (!program || program->tip != DUGUM_PROGRAM) {
        fputs("; (program AST'si yok)\n", out);
        return;
    }

    Arena *a = arena_olustur(0);
    if (!a) return;

    LlvmGen g;
    memset(&g, 0, sizeof(g));
    g.out = out;
    g.arena = a;

    /* Pre-pass: kullan dosyalarini yukle (program->uyeler listesini AST
     * uzerinde mutate ederek genislet) */
    {
        Dugum **eski_uyeler = program->veri.program.uyeler;
        int eski_sayi = program->veri.program.sayi;
        Dugum **yeni_uyeler = NULL;
        int yeni_sayi = 0;
        int kap = 0;

        #define EKLE_UYE(u) do { \
            if (yeni_sayi == kap) { \
                kap = kap == 0 ? 16 : kap * 2; \
                Dugum **r = (Dugum **)realloc(yeni_uyeler, sizeof(Dugum *) * (size_t)kap); \
                if (!r) break; \
                yeni_uyeler = r; \
            } \
            yeni_uyeler[yeni_sayi++] = (u); \
        } while (0)

        for (int i = 0; i < eski_sayi; i++) {
            Dugum *uye = eski_uyeler[i];
            if (uye->tip == DUGUM_KULLAN) {
                /* Dosya yolu uret */
                const char *y = uye->veri.kullan.yol;
                int yu = uye->veri.kullan.yol_uzunluk;
                char dy[512];
                int o = 0;
                for (int k = 0; k < yu && o + 6 < (int)sizeof(dy); k++) {
                    if (k + 1 < yu && y[k] == ':' && y[k + 1] == ':') {
                        dy[o++] = '/'; k++;
                    } else { dy[o++] = y[k]; }
                }
                const char *ext = ".kem";
                for (int k = 0; k < 4 && o + 1 < (int)sizeof(dy); k++) {
                    dy[o++] = ext[k];
                }
                dy[o] = '\0';
                /* Duplicate? */
                int yuklu = 0;
                for (YuklenmisDosya *yd = g.yuklenmis_dosyalar; yd; yd = yd->sonraki) {
                    if (yd->yol_uz == o && memcmp(yd->yol, dy, (size_t)o) == 0) {
                        yuklu = 1; break;
                    }
                }
                if (yuklu) continue;
                FILE *fp = fopen(dy, "rb");
                if (!fp) continue;
                fseek(fp, 0, SEEK_END);
                long sz = ftell(fp);
                fseek(fp, 0, SEEK_SET);
                char *src = (char *)arena_ayir(a, (size_t)sz + 1);
                if (!src) { fclose(fp); continue; }
                fread(src, 1, (size_t)sz, fp);
                src[sz] = '\0';
                fclose(fp);
                /* Yuklenmis listesine ekle */
                YuklenmisDosya *yd = (YuklenmisDosya *)arena_ayir_sifir(a, sizeof(YuklenmisDosya));
                if (yd) {
                    char *yk = (char *)arena_ayir(a, (size_t)o + 1);
                    memcpy(yk, dy, (size_t)o + 1);
                    yd->yol = yk;
                    yd->yol_uz = o;
                    yd->sonraki = g.yuklenmis_dosyalar;
                    g.yuklenmis_dosyalar = yd;
                }
                /* Parse */
                Lexer ml; lexer_baslat(&ml, src, dy);
                Parser mp; parser_baslat(&mp, &ml, a, dy, src);
                Dugum *mprog = parser_calistir(&mp);
                if (mprog && mp.hata_sayisi == 0) {
                    for (int k = 0; k < mprog->veri.program.sayi; k++) {
                        EKLE_UYE(mprog->veri.program.uyeler[k]);
                    }
                }
            } else {
                EKLE_UYE(uye);
            }
        }
        #undef EKLE_UYE
        /* program->veri.program AST'sini mutate et */
        Dugum *mut_p = (Dugum *)program;
        if (yeni_uyeler) {
            Dugum **arena_arr = (Dugum **)arena_ayir(a,
                sizeof(Dugum *) * (size_t)yeni_sayi);
            if (arena_arr) {
                memcpy(arena_arr, yeni_uyeler, sizeof(Dugum *) * (size_t)yeni_sayi);
                mut_p->veri.program.uyeler = arena_arr;
                mut_p->veri.program.sayi = yeni_sayi;
            }
            free(yeni_uyeler);
        }
    }

    /* Pre-pass: yapilari kayit et */
    for (int i = 0; i < program->veri.program.sayi; i++) {
        const Dugum *uye = program->veri.program.uyeler[i];
        if (uye->tip == DUGUM_YAPI) yapi_kayit(&g, uye);
        else if (uye->tip == DUGUM_DISA && uye->veri.disa.tanim &&
                 uye->veri.disa.tanim->tip == DUGUM_YAPI) {
            yapi_kayit(&g, uye->veri.disa.tanim);
        }
    }

    /* Pre-pass: islev imzalarini kayit et (cagri donus tipi icin) */
    for (int i = 0; i < program->veri.program.sayi; i++) {
        const Dugum *uye = program->veri.program.uyeler[i];
        if (uye->tip == DUGUM_ISLEV) islev_kayit(&g, uye);
        else if (uye->tip == DUGUM_DISA && uye->veri.disa.tanim &&
                 uye->veri.disa.tanim->tip == DUGUM_ISLEV) {
            islev_kayit(&g, uye->veri.disa.tanim);
        }
    }

    /* Pre-pass: metinleri topla */
    ast_taransa_metinleri(&g, program);

    /* Emit module-basi: yapi tip tanimlari + string globalleri */
    yapi_tip_tanimlari_emit(&g);
    str_globalleri_emit(&g);

    /* Islevleri emit et (generic olanlar atlanir; instantiation'lar sonra) */
    for (int i = 0; i < program->veri.program.sayi; i++) {
        const Dugum *uye = program->veri.program.uyeler[i];
        if (uye->tip == DUGUM_ISLEV) {
            islev_uret(&g, uye);
        } else if (uye->tip == DUGUM_DISA && uye->veri.disa.tanim &&
                   uye->veri.disa.tanim->tip == DUGUM_ISLEV) {
            islev_uret(&g, uye->veri.disa.tanim);
        } else if (uye->tip == DUGUM_UYGULA) {
            /* uygula gövdesindeki methodlari emit et — kendin parametresi
             * uygula.tip ile degistirilir */
            for (int j = 0; j < uye->veri.uygula.islev_sayi; j++) {
                Dugum *m = uye->veri.uygula.islevler[j];
                if (!m || m->tip != DUGUM_ISLEV || !m->veri.islev.govde) {
                    continue;
                }
                /* kendin parametre var mi? Tipini hedef tipe ayarla */
                for (int k = 0; k < m->veri.islev.param_sayi; k++) {
                    Dugum *p = m->veri.islev.parametreler[k];
                    if (p->veri.parametre.kendin_mi &&
                        p->veri.parametre.tip == NULL) {
                        p->veri.parametre.tip = uye->veri.uygula.tip;
                    }
                }
                /* IslevKayit'ta kayitli mi? Yoksa kayit et */
                if (!islev_bul(&g, m->veri.islev.ad,
                               m->veri.islev.ad_uzunluk)) {
                    islev_kayit(&g, m);
                }
                islev_uret(&g, m);
            }
        }
    }

    /* Bekleyen generic specialization'lari emit et (fixed-point) */
    int max_iter = 32;
    while (g.bekleyenler && max_iter-- > 0) {
        BekleyenSpec *bs = g.bekleyenler;
        g.bekleyenler = bs->sonraki;
        specialize_emit(&g, bs->ast, bs->tip_args, bs->tip_arg_sayi,
                        bs->mangled);
    }

    arena_serbest(a);
}
