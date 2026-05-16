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
    /* Madde B: Dizi<T> tipli degiskenler icin eleman tipi (i32/i64/ptr).
     * dizi_ekle / dizi_al icin element-aware kdl_ cagrisi route etmek icin. */
    const char *eleman_llvm_tip;
    /* Adim 3 (B v2): heap dizi (KdlDizi*) ise 1; stack [N x T] ise 0.
     * dizi literal değişken annot ile heap olarak allocate edildiyse,
     * arr[i] sintaksi kdl_dizi_al ile route edilir. */
    int dinamik_dizi_mi;
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
    /* Adim 7: islev tipi -> ptr (function pointer) */
    if (tip_d->tip == DUGUM_TIP_ISLEV) return "ptr";
    /* Sabitsüre Spec V1: sabitsüre<T> runtime'da T (zero-overhead) */
    if (tip_d->tip == DUGUM_TIP_SABITSURE) {
        return ast_tip_to_ir(g, tip_d->veri.tip_sabitsure.ic_tip);
    }
    /* Tekkez de aynı şekilde — runtime overhead yok */
    if (tip_d->tip == DUGUM_TIP_TEKKEZ) {
        return ast_tip_to_ir(g, tip_d->veri.tip_tekkez.ic_tip);
    }
    /* Capability Spec V1: yetki<R> -> %kdl_yetki struct (16 byte)
     * Bu, monomorphic by-value temsil. R bilgisi sadece type-check'te;
     * runtime'da kaynak_tipi field (uint16) icinde.
     * struct: { i64, i16, i16, i8, [3 x i8] } */
    if (tip_d->tip == DUGUM_TIP_YETKI) {
        return "%kdl_yetki";
    }
    /* SIMD Spec V1: vektör<T, N> → <N x T> LLVM IR */
    if (tip_d->tip == DUGUM_TIP_VEKTOR) {
        const char *eleman_ir = ast_tip_to_ir(g, tip_d->veri.tip_vektor.eleman_tip);
        int lane = tip_d->veri.tip_vektor.lane_sayi;
        /* "<N x T>" stringini arena'da kur */
        int buf_sz = 32;
        char *buf = (char *)arena_ayir(g->arena, (size_t)buf_sz);
        if (buf) {
            int n = snprintf(buf, (size_t)buf_sz, "<%d x %s>", lane, eleman_ir);
            (void)n;
            return buf;
        }
        return "i32";
    }
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
    /* SIMD Spec V1: vektör tipler arasında otomatik conversion yok (tip
     * kontrolü zaten engelliyor); src değişmeden döner. */
    if (src_tip[0] == '<' || dst_tip[0] == '<') return src_reg;
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
    if (!i) {
        /* Adim 7: islev adi mi? Eger oyle ise function pointer dön. */
        IslevKayit *ik = islev_bul(g,
            d->veri.tanimlayici.metin,
            d->veri.tanimlayici.uzunluk);
        if (ik) {
            int r = yeni_reg(g);
            fprintf(g->out, "  %%%d = bitcast ptr ", r);
            ad_yaz(g->out, "@", 1);
            ad_yaz(g->out, d->veri.tanimlayici.metin,
                   d->veri.tanimlayici.uzunluk);
            fputs(" to ptr\n", g->out);
            IfadeSonuc s = { r, "ptr" };
            return s;
        }
        return hata(g, "tanimsiz tanimlayici");
    }
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
    if (!ir) return 0;
    if (strcmp(ir, "float") == 0 || strcmp(ir, "double") == 0) return 1;
    /* SIMD Spec V1: vektör IR tip "<N x float>" / "<N x double>" de kesirli */
    if (ir[0] == '<') {
        /* Suffix "x float>" veya "x double>" arar */
        const char *fp = strstr(ir, " x float>");
        const char *dp = strstr(ir, " x double>");
        if (fp || dp) return 1;
    }
    return 0;
}

/* SIMD Spec V1: IR tip vektör mi? ("<N x T>" formatı) */
static int tip_vektor_ir_mi(const char *ir) {
    return ir && ir[0] == '<' && strstr(ir, " x ") != NULL;
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
            /* Adim 3 (B v2): heap dizi tanimlayicisi mi? Eger oyle ise
             * kdl_dizi_al route et. Aksi halde mevcut GEP yolu (stack). */
            if (d->veri.indeks.nesne &&
                d->veri.indeks.nesne->tip == DUGUM_TANIMLAYICI) {
                LlvmIsim *vi = isim_bul(g,
                    d->veri.indeks.nesne->veri.tanimlayici.metin,
                    d->veri.indeks.nesne->veri.tanimlayici.uzunluk);
                if (vi && vi->dinamik_dizi_mi) {
                    /* Heap dizi: kdl_dizi_al cagrisi */
                    /* nesne load: %v_load = load ptr, ptr %v_alloca */
                    int v_load = yeni_reg(g);
                    fprintf(g->out, "  %%%d = load ptr, ptr %%%d\n",
                            v_load, vi->reg_no);
                    IfadeSonuc idx = ifade_uret(g, d->veri.indeks.indeks, "i32");
                    int idx_r = int_donustur(g, idx.reg, idx.tip, "i32");
                    const char *et = vi->eleman_llvm_tip
                        ? vi->eleman_llvm_tip : "i32";
                    const char *fn = "kdl_dizi_al_tam";
                    if (strcmp(et, "i64") == 0) fn = "kdl_dizi_al_tam64";
                    else if (strcmp(et, "ptr") == 0) fn = "kdl_dizi_al_ptr";
                    int rr = yeni_reg(g);
                    fprintf(g->out,
                        "  %%%d = call %s @%s(ptr %%%d, i32 %%%d)\n",
                        rr, et, fn, v_load, idx_r);
                    IfadeSonuc s = { rr, et };
                    return s;
                }
            }
            /* arr[i] -> GEP ptr (T*) + load (stack) */
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
            /* Sabitsüre Spec V1 intrinsics: sabitsure_yarat (16 byte) ve
             * ifsa (5 byte). Argümanı pass-through, sonra speculation
             * barrier (x86 lfence) emit ederiz. Zero-overhead — IR seviyesi
             * tipleri T (iç tip) ile aynı. */
            {
                const char *_ca = d->veri.cagri.hedef->veri.tanimlayici.metin;
                int _uz = d->veri.cagri.hedef->veri.tanimlayici.uzunluk;
                int _is_yarat = (_uz == 16 &&
                    memcmp(_ca, "sabits\xc3\xbc" "re_yarat", 16) == 0);
                int _is_ifsa = (_uz == 5 &&
                    memcmp(_ca, "if\xc5\x9f" "a", 5) == 0);
                if ((_is_yarat || _is_ifsa) && d->veri.cagri.sayi == 1) {
                    IfadeSonuc inner = ifade_uret(g,
                        d->veri.cagri.argumanlar[0], beklenen);
                    /* x86 lfence speculation barrier — Spectre v1 mitigation.
                     * Modern LLVM intrinsic; declare gerekmez (built-in). */
                    fputs("  call void @llvm.x86.sse2.lfence()\n", g->out);
                    return inner;
                }
            }
            /* Capability Spec V1 intrinsics — yetki_olustur, delege, geri_al */
            {
                const char *_ca = d->veri.cagri.hedef->veri.tanimlayici.metin;
                int _uz = d->veri.cagri.hedef->veri.tanimlayici.uzunluk;
                /* yetki_olustur(kt, izin) -> %kdl_yetki */
                if (_uz == 13 && memcmp(_ca, "yetki_olustur", 13) == 0 &&
                    d->veri.cagri.sayi == 2) {
                    IfadeSonuc arg_kt = ifade_uret(g,
                        d->veri.cagri.argumanlar[0], "i16");
                    IfadeSonuc arg_izin = ifade_uret(g,
                        d->veri.cagri.argumanlar[1], "i16");
                    /* int_donustur tasiyici varsayim — trunc to i16 */
                    int r_kt = arg_kt.reg;
                    int r_izin = arg_izin.reg;
                    if (strcmp(arg_kt.tip, "i16") != 0) {
                        int t = yeni_reg(g);
                        fprintf(g->out,
                            "  %%%d = trunc %s %%%d to i16\n",
                            t, arg_kt.tip, r_kt);
                        r_kt = t;
                    }
                    if (strcmp(arg_izin.tip, "i16") != 0) {
                        int t = yeni_reg(g);
                        fprintf(g->out,
                            "  %%%d = trunc %s %%%d to i16\n",
                            t, arg_izin.tip, r_izin);
                        r_izin = t;
                    }
                    int r = yeni_reg(g);
                    fprintf(g->out,
                        "  %%%d = call %%kdl_yetki @kdl_yetki_olustur("
                        "i16 %%%d, i16 %%%d)\n", r, r_kt, r_izin);
                    IfadeSonuc s = { r, "%kdl_yetki" };
                    return s;
                }
                /* delege(y, izin) -> %kdl_yetki */
                if (_uz == 6 && memcmp(_ca, "delege", 6) == 0 &&
                    d->veri.cagri.sayi == 2) {
                    IfadeSonuc arg_y = ifade_uret(g,
                        d->veri.cagri.argumanlar[0], "%kdl_yetki");
                    IfadeSonuc arg_izin = ifade_uret(g,
                        d->veri.cagri.argumanlar[1], "i16");
                    int r_izin = arg_izin.reg;
                    if (strcmp(arg_izin.tip, "i16") != 0) {
                        int t = yeni_reg(g);
                        fprintf(g->out,
                            "  %%%d = trunc %s %%%d to i16\n",
                            t, arg_izin.tip, r_izin);
                        r_izin = t;
                    }
                    int r = yeni_reg(g);
                    fprintf(g->out,
                        "  %%%d = call %%kdl_yetki @kdl_yetki_delege("
                        "%%kdl_yetki %%%d, i16 %%%d)\n",
                        r, arg_y.reg, r_izin);
                    IfadeSonuc s = { r, "%kdl_yetki" };
                    return s;
                }
                /* geri_al(y) -> void — y bir tanimlayici ise alloca'ya pointer ver */
                if (_uz == 7 && memcmp(_ca, "geri_al", 7) == 0 &&
                    d->veri.cagri.sayi == 1) {
                    const Dugum *arg0 = d->veri.cagri.argumanlar[0];
                    int ptr_reg = -1;
                    if (arg0->tip == DUGUM_TANIMLAYICI) {
                        LlvmIsim *vi = isim_bul(g,
                            arg0->veri.tanimlayici.metin,
                            arg0->veri.tanimlayici.uzunluk);
                        if (vi) {
                            ptr_reg = vi->reg_no;
                        }
                    }
                    if (ptr_reg < 0) {
                        /* Geçici alloca + store + ptr — sub-optimal ama doğru */
                        IfadeSonuc y_val = ifade_uret(g, arg0, "%kdl_yetki");
                        int alloc = yeni_reg(g);
                        fprintf(g->out, "  %%%d = alloca %%kdl_yetki\n", alloc);
                        fprintf(g->out,
                            "  store %%kdl_yetki %%%d, ptr %%%d\n",
                            y_val.reg, alloc);
                        ptr_reg = alloc;
                    }
                    fprintf(g->out,
                        "  call void @kdl_yetki_geri_al(ptr %%%d)\n",
                        ptr_reg);
                    /* Donus: void/i32 0 (geri_al donus tipi bos) */
                    IfadeSonuc s = { 0, "void" };
                    return s;
                }
            }

            /* === SIMD Spec V1 intrinsicleri === */
            {
                const char *fn = d->veri.cagri.hedef->veri.tanimlayici.metin;
                int fn_uz = d->veri.cagri.hedef->veri.tanimlayici.uzunluk;

                /* vektor_doldur(s) — beklenen "<N x T>" ise splat üret */
                if (fn_uz == 13 && memcmp(fn, "vektor_doldur", 13) == 0 &&
                    d->veri.cagri.sayi == 1) {
                    /* Beklenen vektör IR tipinden N ve T çıkar */
                    if (beklenen && tip_vektor_ir_mi(beklenen)) {
                        /* "<N x T>" formatı: N ve T parse */
                        int N = 0;
                        const char *p = beklenen + 1;
                        while (*p >= '0' && *p <= '9') {
                            N = N * 10 + (*p - '0');
                            p++;
                        }
                        /* Element tipini ayır */
                        const char *t_start = strstr(beklenen, " x ");
                        char eleman_buf[32] = "i32";
                        if (t_start) {
                            t_start += 3;
                            const char *t_end = strchr(t_start, '>');
                            if (t_end && (t_end - t_start) < 31) {
                                int len = (int)(t_end - t_start);
                                memcpy(eleman_buf, t_start, (size_t)len);
                                eleman_buf[len] = '\0';
                            }
                        }
                        IfadeSonuc s = ifade_uret(g,
                            d->veri.cagri.argumanlar[0], eleman_buf);
                        /* insertelement <N x T> undef, T s, i32 0 */
                        int r1 = yeni_reg(g);
                        fprintf(g->out,
                            "  %%%d = insertelement %s undef, %s %%%d, i32 0\n",
                            r1, beklenen, eleman_buf, s.reg);
                        /* shufflevector <N x T> %r1, <N x T> undef, <N x i32> zeroinitializer */
                        int r2 = yeni_reg(g);
                        fprintf(g->out,
                            "  %%%d = shufflevector %s %%%d, %s undef, <%d x i32> zeroinitializer\n",
                            r2, beklenen, r1, beklenen, N);
                        IfadeSonuc res = { r2, beklenen };
                        /* beklenen mevcut hafıza yapısından kopyalanmalı çünkü
                         * stable ptr şart — alıcı zaten arena'da tutuyor */
                        return res;
                    }
                    /* Beklenen yoksa default <4 x i32> */
                    IfadeSonuc s = ifade_uret(g,
                        d->veri.cagri.argumanlar[0], "i32");
                    int r1 = yeni_reg(g);
                    fprintf(g->out,
                        "  %%%d = insertelement <4 x i32> undef, i32 %%%d, i32 0\n",
                        r1, s.reg);
                    int r2 = yeni_reg(g);
                    fprintf(g->out,
                        "  %%%d = shufflevector <4 x i32> %%%d, <4 x i32> undef, <4 x i32> zeroinitializer\n",
                        r2, r1);
                    IfadeSonuc res = { r2, "<4 x i32>" };
                    return res;
                }

                /* vektor_eleman(v, i) -> T (extractelement) */
                if (fn_uz == 13 && memcmp(fn, "vektor_eleman", 13) == 0 &&
                    d->veri.cagri.sayi == 2) {
                    IfadeSonuc vs = ifade_uret(g,
                        d->veri.cagri.argumanlar[0], NULL);
                    IfadeSonuc is = ifade_uret(g,
                        d->veri.cagri.argumanlar[1], "i32");
                    /* Element tipini IR'dan çıkar */
                    const char *t_start = strstr(vs.tip, " x ");
                    char eleman_buf[32] = "i32";
                    if (t_start) {
                        t_start += 3;
                        const char *t_end = strchr(t_start, '>');
                        if (t_end && (t_end - t_start) < 31) {
                            int len = (int)(t_end - t_start);
                            memcpy(eleman_buf, t_start, (size_t)len);
                            eleman_buf[len] = '\0';
                        }
                    }
                    int r = yeni_reg(g);
                    fprintf(g->out,
                        "  %%%d = extractelement %s %%%d, i32 %%%d\n",
                        r, vs.tip, vs.reg, is.reg);
                    /* Eleman IR string'i arena'da kalıcı kopya */
                    char *kalici = (char *)arena_ayir(g->arena,
                                                      strlen(eleman_buf) + 1);
                    if (kalici) strcpy(kalici, eleman_buf);
                    IfadeSonuc res = { r, kalici ? kalici : "i32" };
                    return res;
                }

                /* vektor_topla(v) -> T (llvm.vector.reduce.add or fadd) */
                if (fn_uz == 12 && memcmp(fn, "vektor_topla", 12) == 0 &&
                    d->veri.cagri.sayi == 1) {
                    IfadeSonuc vs = ifade_uret(g,
                        d->veri.cagri.argumanlar[0], NULL);
                    /* N + element tipini parse */
                    int N = 0;
                    const char *p = vs.tip + 1;
                    while (*p >= '0' && *p <= '9') {
                        N = N * 10 + (*p - '0'); p++;
                    }
                    const char *t_start = strstr(vs.tip, " x ");
                    char eleman_buf[32] = "i32";
                    if (t_start) {
                        t_start += 3;
                        const char *t_end = strchr(t_start, '>');
                        if (t_end && (t_end - t_start) < 31) {
                            int len = (int)(t_end - t_start);
                            memcpy(eleman_buf, t_start, (size_t)len);
                            eleman_buf[len] = '\0';
                        }
                    }
                    int kesirli_elem = (strcmp(eleman_buf, "float") == 0 ||
                                         strcmp(eleman_buf, "double") == 0);
                    /* LLVM intrinsic abbreviation: float→f32, double→f64,
                     * i8/i16/i32/i64 olduğu gibi. */
                    const char *abbr = eleman_buf;
                    if (strcmp(eleman_buf, "float") == 0)  abbr = "f32";
                    if (strcmp(eleman_buf, "double") == 0) abbr = "f64";
                    int r = yeni_reg(g);
                    if (kesirli_elem) {
                        const char *start = "0.0";
                        fprintf(g->out,
                            "  %%%d = call %s @llvm.vector.reduce.fadd.v%d%s(%s %s, %s %%%d)\n",
                            r, eleman_buf, N, abbr,
                            eleman_buf, start,
                            vs.tip, vs.reg);
                    } else {
                        fprintf(g->out,
                            "  %%%d = call %s @llvm.vector.reduce.add.v%d%s(%s %%%d)\n",
                            r, eleman_buf, N, abbr,
                            vs.tip, vs.reg);
                    }
                    char *kalici = (char *)arena_ayir(g->arena,
                                                      strlen(eleman_buf) + 1);
                    if (kalici) strcpy(kalici, eleman_buf);
                    IfadeSonuc res = { r, kalici ? kalici : "i32" };
                    return res;
                }
            }
            int n = d->veri.cagri.sayi;
            /* Madde B: dizi_ekle / dizi_al ic context — eleman tipi
             * arg[0]'in (dizi degiskeni) eleman_llvm_tip'inden alinir.
             * Beklenen arg[1] icin set edilir. */
            const char *dizi_eleman_beklenen = NULL;
            {
                const char *adi = d->veri.cagri.hedef
                    ? d->veri.cagri.hedef->veri.tanimlayici.metin : NULL;
                int adi_uz = d->veri.cagri.hedef
                    ? d->veri.cagri.hedef->veri.tanimlayici.uzunluk : 0;
                int dizi_built_in =
                    (adi_uz == 9 && memcmp(adi, "dizi_ekle", 9) == 0) ||
                    (adi_uz == 7 && memcmp(adi, "dizi_al", 7) == 0);
                if (dizi_built_in && n >= 1) {
                    const Dugum *arg0 = d->veri.cagri.argumanlar[0];
                    if (arg0 && arg0->tip == DUGUM_TANIMLAYICI) {
                        LlvmIsim *vi = isim_bul(g,
                            arg0->veri.tanimlayici.metin,
                            arg0->veri.tanimlayici.uzunluk);
                        if (vi && vi->eleman_llvm_tip) {
                            dizi_eleman_beklenen = vi->eleman_llvm_tip;
                        }
                    }
                }
            }

            IslevKayit *ik = islev_bul(g,
                d->veri.cagri.hedef->veri.tanimlayici.metin,
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk);

            /* Adim 7: Indirect call — hedef bir parametre/lokal degisken
             * (function pointer) ise call ptr ile ara. Args burada erken
             * evaluate edilir (dizi_eleman_beklenen icin arg[1] context). */
            if (!ik) {
                LlvmIsim *vi = isim_bul(g,
                    d->veri.cagri.hedef->veri.tanimlayici.metin,
                    d->veri.cagri.hedef->veri.tanimlayici.uzunluk);
                if (vi) {
                    IfadeSonuc *iargs = NULL;
                    if (n > 0) {
                        iargs = (IfadeSonuc *)arena_ayir(g->arena,
                            sizeof(IfadeSonuc) * (size_t)n);
                        for (int i = 0; i < n; i++) {
                            const char *ab = NULL;
                            if (i == 1 && dizi_eleman_beklenen) ab = dizi_eleman_beklenen;
                            iargs[i] = ifade_uret(g,
                                d->veri.cagri.argumanlar[i], ab);
                        }
                    }
                    int fn_reg = yeni_reg(g);
                    fprintf(g->out, "  %%%d = load ptr, ptr %%%d\n",
                            fn_reg, vi->reg_no);
                    const char *donus_indirect = beklenen ? beklenen : "i32";
                    int rr = yeni_reg(g);
                    fprintf(g->out, "  %%%d = call %s %%%d(",
                            rr, donus_indirect, fn_reg);
                    for (int i = 0; i < n; i++) {
                        if (i > 0) fputs(", ", g->out);
                        fprintf(g->out, "%s %%%d", iargs[i].tip, iargs[i].reg);
                    }
                    fputs(")\n", g->out);
                    IfadeSonuc s = { rr, donus_indirect };
                    return s;
                }
            }

            const char *cagri_adi = d->veri.cagri.hedef->veri.tanimlayici.metin;
            int cagri_adi_uz = d->veri.cagri.hedef->veri.tanimlayici.uzunluk;

            /* Built-in libc / kdl mapping */
            const char *kdl_donus = NULL;  /* override (NULL ise auto) */
            /* src-bugfix'ten: param_beklenen + builtin_donus (genis tasarim) */
            const char *param_beklenen[8] = { NULL, NULL, NULL, NULL,
                                              NULL, NULL, NULL, NULL };
            const char *builtin_donus = NULL;
            (void)param_beklenen; (void)builtin_donus;
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
            } else if (cagri_adi_uz == 10 &&
                       memcmp(cagri_adi, "yazdir_tam", 10) == 0) {
                cagri_adi = "kdl_yazdir_tam"; cagri_adi_uz = 14;
                param_beklenen[0] = "i32"; builtin_donus = "void";
            } else if (cagri_adi_uz == 12 &&
                       memcmp(cagri_adi, "yazdir_tam64", 12) == 0) {
                cagri_adi = "kdl_yazdir_tam64"; cagri_adi_uz = 16;
                param_beklenen[0] = "i64"; builtin_donus = "void";
            } else if (cagri_adi_uz == 12 &&
                       memcmp(cagri_adi, "yazdir_satir", 12) == 0) {
                cagri_adi = "kdl_yazdir_satir"; cagri_adi_uz = 16;
                builtin_donus = "void";
            } else if (cagri_adi_uz == 7 &&
                       memcmp(cagri_adi, "yaz_tam", 7) == 0) {
                cagri_adi = "kdl_yaz_tam"; cagri_adi_uz = 11;
                param_beklenen[0] = "i32"; builtin_donus = "void";
            } else if (cagri_adi_uz == 9 &&
                       memcmp(cagri_adi, "yaz_tam64", 9) == 0) {
                cagri_adi = "kdl_yaz_tam64"; cagri_adi_uz = 13;
                param_beklenen[0] = "i64"; builtin_donus = "void";
            } else if (cagri_adi_uz == 12 &&
                       memcmp(cagri_adi, "yazdir_metin", 12) == 0) {
                /* Bare-metal hedef icin: yazdir_metin -> kdl_yazdir_metin
                 * (UART backend). Host hedefte ayni isim runtime/kdl_runtime.c
                 * fputs yoluna gider. */
                cagri_adi = "kdl_yazdir_metin"; cagri_adi_uz = 16;
                param_beklenen[0] = "ptr"; builtin_donus = "void";
            } else if (cagri_adi_uz == 20 &&
                       memcmp(cagri_adi, "yazdir_isaretsiz_tam", 20) == 0) {
                cagri_adi = "kdl_yazdir_isaretsiz_tam"; cagri_adi_uz = 24;
                param_beklenen[0] = "i32"; builtin_donus = "void";
            } else if (cagri_adi_uz == 22 &&
                       memcmp(cagri_adi, "yazdir_isaretsiz_tam64", 22) == 0) {
                cagri_adi = "kdl_yazdir_isaretsiz_tam64"; cagri_adi_uz = 26;
                param_beklenen[0] = "i64"; builtin_donus = "void";
            } else if (cagri_adi_uz == 16 &&
                       memcmp(cagri_adi, "yazdir_onaltilik", 16) == 0) {
                cagri_adi = "kdl_yazdir_onaltilik"; cagri_adi_uz = 20;
                param_beklenen[0] = "i64"; builtin_donus = "void";
            } else if (cagri_adi_uz == 13 &&
                       memcmp(cagri_adi, "yaz_onaltilik", 13) == 0) {
                cagri_adi = "kdl_yaz_onaltilik"; cagri_adi_uz = 17;
                param_beklenen[0] = "i64"; builtin_donus = "void";
            } else if (cagri_adi_uz == 15 &&
                       memcmp(cagri_adi, "yazdir_karakter", 15) == 0) {
                cagri_adi = "kdl_yazdir_karakter"; cagri_adi_uz = 19;
                param_beklenen[0] = "i32"; builtin_donus = "void";
            } else if (cagri_adi_uz == 12 &&
                       memcmp(cagri_adi, "yaz_karakter", 12) == 0) {
                cagri_adi = "kdl_yaz_karakter"; cagri_adi_uz = 16;
                param_beklenen[0] = "i32"; builtin_donus = "void";
            } else if (cagri_adi_uz == 12 &&
                       memcmp(cagri_adi, "oku_karakter", 12) == 0) {
                cagri_adi = "kdl_oku_karakter"; cagri_adi_uz = 16;
                builtin_donus = "i32";
            }
            /* Not: `yaz_metin` built-in olarak register edilmiyor — bkz.
             * tip_kontrol.c'deki cakisma aciklamasi (stdlib/dosya.kem). */

            IfadeSonuc *args = NULL;
            if (n > 0) {
                args = (IfadeSonuc *)arena_ayir(g->arena,
                    sizeof(IfadeSonuc) * (size_t)n);
                for (int i = 0; i < n; i++) {
                    /* HEAD: dizi_ekle/al icin arg[1] dizi_eleman_beklenen.
                     * src-bugfix: I/O built-in icin param_beklenen[i]. */
                    const char *bekle = (i < 8) ? param_beklenen[i] : NULL;
                    if (!bekle && i == 1 && dizi_eleman_beklenen) {
                        bekle = dizi_eleman_beklenen;
                    }
                    args[i] = ifade_uret(g, d->veri.cagri.argumanlar[i], bekle);
                    if (bekle && strcmp(args[i].tip, bekle) != 0 &&
                        (strcmp(bekle, "i64") == 0 ||
                         strcmp(bekle, "i32") == 0 ||
                         strcmp(bekle, "i16") == 0 ||
                         strcmp(bekle, "i8") == 0) &&
                        (strcmp(args[i].tip, "i64") == 0 ||
                         strcmp(args[i].tip, "i32") == 0 ||
                         strcmp(args[i].tip, "i16") == 0 ||
                         strcmp(args[i].tip, "i8") == 0)) {
                        int nr = int_donustur(g, args[i].reg, args[i].tip, bekle);
                        args[i].reg = nr;
                        args[i].tip = bekle;
                    }
                }
            }
            /* Konsolidasyon: args sonrasi mapping ve intrinsicler.
             * `if/else if` chain artik bağimsiz, args alloc'tan sonra. */
            if (cagri_adi_uz >= 6 && memcmp(cagri_adi, "metin_", 6) == 0) {
                /* Madde A: metin_* built-in -> kdl_metin_* */
                static char kdl_buf[64];
                int n = cagri_adi_uz < 56 ? cagri_adi_uz : 56;
                memcpy(kdl_buf, "kdl_", 4);
                memcpy(kdl_buf + 4, cagri_adi, (size_t)n);
                kdl_buf[4 + n] = '\0';
                cagri_adi = kdl_buf; cagri_adi_uz = 4 + n;
                if (n == 13 && memcmp(kdl_buf + 4, "metin_uzunluk", 13) == 0) {
                    kdl_donus = "i32";
                } else if ((n == 12 && memcmp(kdl_buf + 4, "metin_icerir", 12) == 0) ||
                           (n == 12 && memcmp(kdl_buf + 4, "metin_baslar", 12) == 0) ||
                           (n == 11 && memcmp(kdl_buf + 4, "metin_biter", 11) == 0)) {
                    kdl_donus = "i1";
                } else {
                    kdl_donus = "ptr";
                }
            }
            /* Madde G: dosya_* built-in -> kdl_dosya_* */
            else if (cagri_adi_uz >= 6 && memcmp(cagri_adi, "dosya_", 6) == 0) {
                static char kdl_dosya_buf[64];
                int n = cagri_adi_uz < 56 ? cagri_adi_uz : 56;
                memcpy(kdl_dosya_buf, "kdl_", 4);
                memcpy(kdl_dosya_buf + 4, cagri_adi, (size_t)n);
                kdl_dosya_buf[4 + n] = '\0';
                cagri_adi = kdl_dosya_buf; cagri_adi_uz = 4 + n;
                if (n == 8 && memcmp(kdl_dosya_buf + 4, "dosya_ac", 8) == 0) {
                    kdl_donus = "ptr";
                } else if (n == 9 && memcmp(kdl_dosya_buf + 4, "dosya_oku", 9) == 0) {
                    kdl_donus = "ptr";
                } else if ((n == 9 && memcmp(kdl_dosya_buf + 4, "dosya_yaz", 9) == 0) ||
                           (n == 9 && memcmp(kdl_dosya_buf + 4, "dosya_sil", 9) == 0) ||
                           (n == 22 && memcmp(kdl_dosya_buf + 4,
                                              "dosya_yeniden_adlandir", 22) == 0)) {
                    kdl_donus = "i32";
                } else if (n == 11 && memcmp(kdl_dosya_buf + 4, "dosya_kapat", 11) == 0) {
                    kdl_donus = "void";
                } else if (n == 12 && memcmp(kdl_dosya_buf + 4, "dosya_var_mi", 12) == 0) {
                    kdl_donus = "i1";
                } else if (n == 11 && memcmp(kdl_dosya_buf + 4, "dosya_boyut", 11) == 0) {
                    kdl_donus = "i64";
                } else {
                    kdl_donus = "ptr";
                }
            }
            /* Madde B: dinamik dizi intrinsicleri (dizi_olustur/ekle/al/boyut)
             * — element tipi arg/return inference ile belirlenir. */
            else if (cagri_adi_uz == 12 &&
                     memcmp(cagri_adi, "dizi_olustur", 12) == 0) {
                /* dizi_olustur(N) -> ptr (KdlDizi*). Adim 6: eleman_byte
                 * context'ten (beklenen ptr ise default 4; gercek T size
                 * gelecek surumde annotation'dan). v1 default 4 (tam32). */
                int rr = yeni_reg(g);
                int kap = args[0].reg;
                int kap_i32 = int_donustur(g, kap, args[0].tip, "i32");
                fprintf(g->out,
                    "  %%%d = call ptr @kdl_dizi_olustur(i32 4)\n", rr);
                /* Adim 6: kapasiteyi pre-reserve et (kullanici N istiyor) */
                fprintf(g->out,
                    "  call void @kdl_dizi_kapasite_ayarla(ptr %%%d, i32 %%%d)\n",
                    rr, kap_i32);
                IfadeSonuc s = { rr, "ptr" };
                return s;
            }
            else if (cagri_adi_uz == 9 &&
                     memcmp(cagri_adi, "dizi_ekle", 9) == 0) {
                /* dizi_ekle(d, e) -> void. T = dizi_eleman_beklenen ya da
                 * e'in tipinden tahmin */
                const char *et = dizi_eleman_beklenen
                    ? dizi_eleman_beklenen
                    : (n > 1 ? args[1].tip : "i32");
                const char *fn;
                const char *cast_tip = et;
                if (strcmp(et, "i64") == 0) fn = "kdl_dizi_ekle_tam64";
                else if (strcmp(et, "ptr") == 0) fn = "kdl_dizi_ekle_ptr";
                else { fn = "kdl_dizi_ekle_tam"; cast_tip = "i32"; }
                int ev = (n > 1) ? int_donustur(g, args[1].reg,
                                                 args[1].tip, cast_tip)
                                 : 0;
                fprintf(g->out,
                    "  call void @%s(ptr %%%d, %s %%%d)\n",
                    fn, args[0].reg, cast_tip, ev);
                /* void donus — placeholder i32 0 */
                int rr = yeni_reg(g);
                fprintf(g->out, "  %%%d = add i32 0, 0\n", rr);
                IfadeSonuc s = { rr, "i32" };
                return s;
            }
            else if (cagri_adi_uz == 7 &&
                     memcmp(cagri_adi, "dizi_al", 7) == 0) {
                /* dizi_al(d, i) -> T. T: dizi_eleman_beklenen > beklenen > i32 */
                const char *et = dizi_eleman_beklenen
                    ? dizi_eleman_beklenen
                    : ((beklenen && *beklenen) ? beklenen : "i32");
                const char *fn;
                if (strcmp(et, "i64") == 0) fn = "kdl_dizi_al_tam64";
                else if (strcmp(et, "ptr") == 0) fn = "kdl_dizi_al_ptr";
                else { fn = "kdl_dizi_al_tam"; et = "i32"; }
                int idx_i32 = (n > 1) ? int_donustur(g, args[1].reg,
                                                      args[1].tip, "i32") : 0;
                int rr = yeni_reg(g);
                fprintf(g->out,
                    "  %%%d = call %s @%s(ptr %%%d, i32 %%%d)\n",
                    rr, et, fn, args[0].reg, idx_i32);
                IfadeSonuc s = { rr, et };
                return s;
            }
            else if (cagri_adi_uz == 10 &&
                     memcmp(cagri_adi, "dizi_boyut", 10) == 0) {
                int rr = yeni_reg(g);
                fprintf(g->out,
                    "  %%%d = call i32 @kdl_dizi_boyut(ptr %%%d)\n",
                    rr, args[0].reg);
                IfadeSonuc s = { rr, "i32" };
                return s;
            }
            /* Adim 6: dizi_kapasite + dizi_kapasite_ayarla */
            else if (cagri_adi_uz == 13 &&
                     memcmp(cagri_adi, "dizi_kapasite", 13) == 0) {
                int rr = yeni_reg(g);
                fprintf(g->out,
                    "  %%%d = call i32 @kdl_dizi_kapasite(ptr %%%d)\n",
                    rr, args[0].reg);
                IfadeSonuc s = { rr, "i32" };
                return s;
            }
            else if (cagri_adi_uz == 20 &&
                     memcmp(cagri_adi, "dizi_kapasite_ayarla", 20) == 0) {
                int yk = (n > 1) ? int_donustur(g, args[1].reg,
                                                 args[1].tip, "i32") : 0;
                fprintf(g->out,
                    "  call void @kdl_dizi_kapasite_ayarla(ptr %%%d, i32 %%%d)\n",
                    args[0].reg, yk);
                int rr = yeni_reg(g);
                fprintf(g->out, "  %%%d = add i32 0, 0\n", rr);
                IfadeSonuc s = { rr, "i32" };
                return s;
            }
            /* Adim 1: CLI args + OTP yardimcilari */
            else if (cagri_adi_uz == 8 &&
                     memcmp(cagri_adi, "arg_sayi", 8) == 0) {
                cagri_adi = "kdl_arg_sayi"; cagri_adi_uz = 12;
                kdl_donus = "i32";
            }
            else if (cagri_adi_uz == 6 &&
                     memcmp(cagri_adi, "arg_al", 6) == 0) {
                cagri_adi = "kdl_arg_al"; cagri_adi_uz = 10;
                kdl_donus = "ptr";
            }
            else if (cagri_adi_uz == 16 &&
                     memcmp(cagri_adi, "otp_anahtar_uret", 16) == 0) {
                cagri_adi = "kdl_otp_anahtar_uret"; cagri_adi_uz = 20;
                kdl_donus = "i32";
            }
            else if (cagri_adi_uz == 14 &&
                     memcmp(cagri_adi, "otp_xor_uygula", 14) == 0) {
                cagri_adi = "kdl_otp_xor_uygula"; cagri_adi_uz = 18;
                kdl_donus = "i32";
            }

            const char *donus = kdl_donus ? kdl_donus
                              : builtin_donus ? builtin_donus
                              : (ik ? ik->donus_tip
                                    : (beklenen ? beklenen : "i32"));

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

            if (strcmp(donus, "void") == 0) {
                /* void-returning call: register atama yok */
                fputs("  call void @", g->out);
                ad_yaz(g->out, cagri_adi, cagri_adi_uz);
                fputs("(", g->out);
                for (int i = 0; i < n; i++) {
                    if (i > 0) fputs(", ", g->out);
                    fprintf(g->out, "%s %%%d", args[i].tip, args[i].reg);
                }
                fputs(")\n", g->out);
                /* Caller bir IfadeSonuc bekliyor — placeholder i32 0 */
                int r = yeni_reg(g);
                fprintf(g->out, "  %%%d = add i32 0, 0\n", r);
                IfadeSonuc s = { r, "i32" };
                return s;
            }
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

        case DUGUM_TIP_DONUSTUR: {
            /* Madde E: x olarak T — explicit cast */
            const char *hedef = ast_tip_to_ir(g, d->veri.tip_donustur.hedef_tip);
            if (!hedef) hedef = "i32";
            IfadeSonuc kaynak = ifade_uret(g, d->veri.tip_donustur.kaynak,
                                            hedef);
            if (strcmp(kaynak.tip, hedef) == 0) {
                return kaynak;
            }
            int k_kesirli = tip_kesirli_mi(kaynak.tip);
            int h_kesirli = tip_kesirli_mi(hedef);
            int r = yeni_reg(g);
            if (!k_kesirli && !h_kesirli) {
                /* int -> int: sext/trunc (int_donustur kullanir) */
                int rr = int_donustur(g, kaynak.reg, kaynak.tip, hedef);
                IfadeSonuc s = { rr, hedef };
                return s;
            }
            if (!k_kesirli && h_kesirli) {
                /* int -> float/double: sitofp */
                fprintf(g->out, "  %%%d = sitofp %s %%%d to %s\n",
                        r, kaynak.tip, kaynak.reg, hedef);
                IfadeSonuc s = { r, hedef };
                return s;
            }
            if (k_kesirli && !h_kesirli) {
                /* float/double -> int: fptosi */
                fprintf(g->out, "  %%%d = fptosi %s %%%d to %s\n",
                        r, kaynak.tip, kaynak.reg, hedef);
                IfadeSonuc s = { r, hedef };
                return s;
            }
            /* float <-> double */
            if (strcmp(kaynak.tip, "float") == 0 &&
                strcmp(hedef, "double") == 0) {
                fprintf(g->out, "  %%%d = fpext float %%%d to double\n",
                        r, kaynak.reg);
            } else {
                fprintf(g->out, "  %%%d = fptrunc double %%%d to float\n",
                        r, kaynak.reg);
            }
            IfadeSonuc s = { r, hedef };
            return s;
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
            /* Madde B: Dizi<T> annot ise eleman tipini de yakala */
            const char *eleman_tip = NULL;
            if (d->veri.degisken.tip) {
                annot = ast_tip_to_ir(g, d->veri.degisken.tip);
                if (d->veri.degisken.tip->tip == DUGUM_TIP_DIZI) {
                    eleman_tip = ast_tip_to_ir(g,
                        d->veri.degisken.tip->veri.tip_dizi.eleman_tip);
                }
            }
            const char *tip = annot;

            /* Adim 3 (B v2): değişken d: Dizi<T> = [e1, ...] heap allocate
             * Pattern: annot Dizi<T> + deger DIZI_OLUSTUR literal -> heap.
             * Stack davranisi: annot yok veya &Dizi<T> ise (referans). */
            if (d->veri.degisken.deger &&
                d->veri.degisken.tip &&
                d->veri.degisken.tip->tip == DUGUM_TIP_DIZI &&
                d->veri.degisken.deger->tip == DUGUM_DIZI_OLUSTUR &&
                eleman_tip) {
                const Dugum *lit = d->veri.degisken.deger;
                int n = lit->veri.dizi_olustur.sayi;
                /* kdl_dizi_olustur(eleman_byte) */
                int eb = 4;
                if (strcmp(eleman_tip, "i8") == 0) eb = 1;
                else if (strcmp(eleman_tip, "i16") == 0) eb = 2;
                else if (strcmp(eleman_tip, "i64") == 0) eb = 8;
                else if (strcmp(eleman_tip, "double") == 0) eb = 8;
                else if (strcmp(eleman_tip, "ptr") == 0) eb = 8;
                int kdl_reg = yeni_reg(g);
                fprintf(g->out,
                    "  %%%d = call ptr @kdl_dizi_olustur(i32 %d)\n",
                    kdl_reg, eb);
                (void)n;
                /* Her elemani ekle */
                for (int i = 0; i < n; i++) {
                    IfadeSonuc v = ifade_uret(g, lit->veri.dizi_olustur.elemanlar[i],
                                              eleman_tip);
                    int vr = int_donustur(g, v.reg, v.tip, eleman_tip);
                    const char *fn = "kdl_dizi_ekle_tam";
                    if (strcmp(eleman_tip, "i64") == 0) fn = "kdl_dizi_ekle_tam64";
                    else if (strcmp(eleman_tip, "ptr") == 0) fn = "kdl_dizi_ekle_ptr";
                    fprintf(g->out,
                        "  call void @%s(ptr %%%d, %s %%%d)\n",
                        fn, kdl_reg, eleman_tip, vr);
                }
                /* alloca ptr + store kdl_reg */
                int alloca_reg = yeni_reg(g);
                fprintf(g->out, "  %%%d = alloca ptr\n", alloca_reg);
                fprintf(g->out, "  store ptr %%%d, ptr %%%d\n",
                        kdl_reg, alloca_reg);
                isim_ekle(g, d->veri.degisken.ad,
                          d->veri.degisken.ad_uzunluk,
                          1, alloca_reg, "ptr");
                g->isimler->eleman_llvm_tip = eleman_tip;
                g->isimler->dinamik_dizi_mi = 1;
                return 0;
            }

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
                    if (eleman_tip) {
                        g->isimler->eleman_llvm_tip = eleman_tip;
                    }
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
                if (eleman_tip) {
                    g->isimler->eleman_llvm_tip = eleman_tip;
                }
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

        /* Adim 7: için x: xs { govde } — heap dizi iteration
         * (KdlDizi*'a dizi_boyut + dizi_al). xs DUGUM_TANIMLAYICI
         * ve dinamik_dizi_mi = 1 ise loop emit edilir. Aksi halde
         * not supported (stack dizi iteration v2'de). */
        case DUGUM_ICIN: {
            const Dugum *koleksiyon = d->veri.icin.koleksiyon;
            if (!koleksiyon || koleksiyon->tip != DUGUM_TANIMLAYICI) {
                fprintf(g->out, "  ; icin: koleksiyon tanimlayici degil\n");
                return 0;
            }
            LlvmIsim *kol_isim = isim_bul(g,
                koleksiyon->veri.tanimlayici.metin,
                koleksiyon->veri.tanimlayici.uzunluk);
            if (!kol_isim || !kol_isim->dinamik_dizi_mi) {
                fprintf(g->out, "  ; icin: heap dizi degil (v1 sinir)\n");
                return 0;
            }
            const char *et = kol_isim->eleman_llvm_tip
                ? kol_isim->eleman_llvm_tip : "i32";
            const char *fn_al = "kdl_dizi_al_tam";
            if (strcmp(et, "i64") == 0) fn_al = "kdl_dizi_al_tam64";
            else if (strcmp(et, "ptr") == 0) fn_al = "kdl_dizi_al_ptr";
            /* Load koleksiyon ptr (kdl_dizi*) */
            int kdl_ptr = yeni_reg(g);
            fprintf(g->out, "  %%%d = load ptr, ptr %%%d\n",
                    kdl_ptr, kol_isim->reg_no);
            /* Boyut al */
            int n_reg = yeni_reg(g);
            fprintf(g->out, "  %%%d = call i32 @kdl_dizi_boyut(ptr %%%d)\n",
                    n_reg, kdl_ptr);
            /* Index i alloca (i32) */
            int i_alloca = yeni_reg(g);
            fprintf(g->out, "  %%%d = alloca i32\n", i_alloca);
            fprintf(g->out, "  store i32 0, ptr %%%d\n", i_alloca);
            /* Eleman x alloca */
            int x_alloca = yeni_reg(g);
            fprintf(g->out, "  %%%d = alloca %s\n", x_alloca, et);
            /* Loop labels */
            int L_head = yeni_label(g);
            int L_body = yeni_label(g);
            int L_done = yeni_label(g);
            fprintf(g->out, "  br label %%bb%d\n", L_head);
            fprintf(g->out, "bb%d:\n", L_head);
            int i_load = yeni_reg(g);
            fprintf(g->out, "  %%%d = load i32, ptr %%%d\n", i_load, i_alloca);
            int cmp = yeni_reg(g);
            fprintf(g->out, "  %%%d = icmp slt i32 %%%d, %%%d\n",
                    cmp, i_load, n_reg);
            fprintf(g->out, "  br i1 %%%d, label %%bb%d, label %%bb%d\n",
                    cmp, L_body, L_done);
            fprintf(g->out, "bb%d:\n", L_body);
            /* x = dizi_al(kdl_ptr, i_load) */
            int el_reg = yeni_reg(g);
            fprintf(g->out, "  %%%d = call %s @%s(ptr %%%d, i32 %%%d)\n",
                    el_reg, et, fn_al, kdl_ptr, i_load);
            fprintf(g->out, "  store %s %%%d, ptr %%%d\n",
                    et, el_reg, x_alloca);
            /* Govde scope: x isim olarak ekle */
            ScopeMarker m = scope_gir(g);
            isim_ekle(g, d->veri.icin.degisken_adi,
                      d->veri.icin.degisken_adi_uzunluk,
                      1, x_alloca, et);
            int body_term = blok_uret(g, d->veri.icin.govde);
            scope_cik(g, m);
            if (!body_term) {
                /* i++ + br */
                int yeni_i = yeni_reg(g);
                fprintf(g->out, "  %%%d = add i32 %%%d, 1\n", yeni_i, i_load);
                fprintf(g->out, "  store i32 %%%d, ptr %%%d\n",
                        yeni_i, i_alloca);
                fprintf(g->out, "  br label %%bb%d\n", L_head);
            }
            fprintf(g->out, "bb%d:\n", L_done);
            return 0;
        }

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

    /* Realtime Spec V1: gercekzamanli isleve metadata yorumu (V1 minimal;
     * V2'de gercek LLVM metadata: !realtime !N). */
    if (islev->veri.islev.gercekzamanli_mi) {
        fputs("; @kemgu.realtime\n", g->out);
    }

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
        /* Adim 7: Eger parametre Dizi<T> ise heap dizi olarak isaretle
         * (caller'dan gelen ptr KdlDizi*). Eleman tipi de yakala. */
        if (p->veri.parametre.tip &&
            p->veri.parametre.tip->tip == DUGUM_TIP_DIZI) {
            const char *et = ast_tip_to_ir(g,
                p->veri.parametre.tip->veri.tip_dizi.eleman_tip);
            if (et) g->isimler->eleman_llvm_tip = et;
            g->isimler->dinamik_dizi_mi = 1;
        }
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
    /* Capability Spec V1 — yetki<R> 16-byte struct (CP.6.1)
     * Layout: { i64 id, i16 kaynak_tipi, i16 izin, i8 iptal, [3 x i8] rezerv } */
    fputs("%kdl_yetki = type { i64, i16, i16, i8, [3 x i8] }\n\n", out);
    /* Built-in extern (libc) bildirimleri */
    fputs("declare i32 @puts(ptr)\n", out);
    fputs("declare ptr @malloc(i64)\n", out);
    fputs("declare void @free(ptr)\n", out);
    fputs("declare ptr @memcpy(ptr, ptr, i64)\n", out);

    /* Madde A: Metin runtime primitifleri (kdl_metin_*) */
    fputs("declare i32 @kdl_metin_uzunluk(ptr)\n", out);
    fputs("declare ptr @kdl_metin_birlestir(ptr, ptr)\n", out);
    fputs("declare ptr @kdl_metin_kes(ptr, i32, i32)\n", out);
    fputs("declare ptr @kdl_metin_kucuk(ptr)\n", out);
    fputs("declare ptr @kdl_metin_buyuk(ptr)\n", out);
    fputs("declare i1 @kdl_metin_icerir(ptr, ptr)\n", out);
    fputs("declare i1 @kdl_metin_baslar(ptr, ptr)\n", out);
    fputs("declare i1 @kdl_metin_biter(ptr, ptr)\n", out);
    fputs("declare ptr @kdl_metin_kirp(ptr)\n", out);
    fputs("declare ptr @kdl_metin_yer_degistir(ptr, ptr, ptr)\n", out);
    fputs("declare ptr @kdl_metin_kucuk_tr(ptr)\n", out);
    fputs("declare ptr @kdl_metin_buyuk_tr(ptr)\n", out);
    fputs("declare ptr @kdl_metin_kucuk_ascii(ptr)\n", out);
    fputs("declare ptr @kdl_metin_buyuk_ascii(ptr)\n", out);

    /* Madde G: Dosya syscall primitifleri (kdl_dosya_*) */
    fputs("declare ptr @kdl_dosya_ac(ptr, ptr)\n", out);
    fputs("declare ptr @kdl_dosya_oku(ptr)\n", out);
    fputs("declare i32 @kdl_dosya_yaz(ptr, ptr)\n", out);
    fputs("declare void @kdl_dosya_kapat(ptr)\n", out);
    fputs("declare i1 @kdl_dosya_var_mi(ptr)\n", out);
    fputs("declare i32 @kdl_dosya_sil(ptr)\n", out);
    fputs("declare i32 @kdl_dosya_yeniden_adlandir(ptr, ptr)\n", out);
    fputs("declare i64 @kdl_dosya_boyut(ptr)\n", out);

    /* Madde B: Dinamik dizi (KdlDizi*) */
    fputs("declare ptr @kdl_dizi_olustur(i32)\n", out);
    fputs("declare void @kdl_dizi_ekle_tam(ptr, i32)\n", out);
    fputs("declare void @kdl_dizi_ekle_tam64(ptr, i64)\n", out);
    fputs("declare void @kdl_dizi_ekle_ptr(ptr, ptr)\n", out);
    fputs("declare i32 @kdl_dizi_al_tam(ptr, i32)\n", out);
    fputs("declare i64 @kdl_dizi_al_tam64(ptr, i32)\n", out);
    fputs("declare ptr @kdl_dizi_al_ptr(ptr, i32)\n", out);
    fputs("declare i32 @kdl_dizi_boyut(ptr)\n", out);
    /* Adim 6: capacity API */
    fputs("declare i32 @kdl_dizi_kapasite(ptr)\n", out);
    fputs("declare void @kdl_dizi_kapasite_ayarla(ptr, i32)\n", out);

    /* Adim 1: CLI args + OTP */
    fputs("declare i32 @kdl_arg_sayi()\n", out);
    fputs("declare ptr @kdl_arg_al(i32)\n", out);
    fputs("declare i32 @kdl_otp_anahtar_uret(ptr, i32)\n", out);
    fputs("declare i32 @kdl_otp_xor_uygula(ptr, ptr, ptr)\n", out);

    /* src-bugfix: KDL I/O genisletme (yazdir_tam, yaz_tam vs.) */
    fputs("declare void @kdl_yazdir_tam(i32)\n", out);
    fputs("declare void @kdl_yazdir_tam64(i64)\n", out);
    fputs("declare void @kdl_yazdir_satir()\n", out);
    fputs("declare void @kdl_yaz_tam(i32)\n", out);
    fputs("declare void @kdl_yaz_tam64(i64)\n", out);
    /* Track B: yazdir_metin -> kdl_yazdir_metin (bare-metal/host ortak) */
    fputs("declare void @kdl_yazdir_metin(ptr)\n", out);
    fputs("declare void @kdl_yazdir_isaretsiz_tam(i32)\n", out);
    fputs("declare void @kdl_yazdir_isaretsiz_tam64(i64)\n", out);
    fputs("declare void @kdl_yazdir_onaltilik(i64)\n", out);
    fputs("declare void @kdl_yaz_onaltilik(i64)\n", out);
    fputs("declare void @kdl_yazdir_karakter(i32)\n", out);
    fputs("declare void @kdl_yaz_karakter(i32)\n", out);
    fputs("declare i32 @kdl_oku_karakter()\n", out);

    /* Capability Spec V1 — yetki<R> runtime intrinsics (kdl_yetki_*) */
    fputs("declare %kdl_yetki @kdl_yetki_olustur(i16, i16)\n", out);
    fputs("declare %kdl_yetki @kdl_yetki_delege(%kdl_yetki, i16)\n", out);
    fputs("declare void @kdl_yetki_geri_al(ptr)\n", out);
    fputs("declare i32 @kdl_yetki_kontrol(%kdl_yetki, i16)\n", out);
    fputs("declare i32 @kdl_yetki_kontrol_tipi(%kdl_yetki, i16, i16)\n", out);
    fputs("declare i64 @kdl_yetki_id(%kdl_yetki)\n", out);
    fputs("declare i16 @kdl_yetki_tipi(%kdl_yetki)\n", out);
    fputs("declare i16 @kdl_yetki_izin(%kdl_yetki)\n", out);
    fputs("declare i8 @kdl_yetki_iptal_mi(%kdl_yetki)\n\n", out);

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
