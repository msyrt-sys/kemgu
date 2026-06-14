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
    /* D-069 Kategori 2: sabit stack dizi [N x T] uzunluğu (derleme-zamanı N).
     * 0 = bilinmiyor (heap/region/skaler). >0 ise arr[i] indekslemesi
     * `icmp uge idx, N` + panic ile sınır-kontrollü (OOB → kdl_panik). */
    int dizi_uzunluk;
    /* v1 bölge-container: *T degisken/parametrenin POINTEE IR tipi
     * (örn. "i8", "i64", "%N"). veri[i] oku/yaz eleman tipini/
     * genisligini BURADAN alir — beklenen/RHS'ten almak tam8/tam64/
     * struct icin yanlis genislik miscompile'iydi (ADIM-0 raporu). */
    const char *pointee_llvm_tip;
    /* Liste<T> BUG-2 fix: tek-tip-arg'li generic kullanici tipi
     * (Liste<tam64> -> "i64") — &Liste<T> parametresinden T inference
     * icin yan-kanal (yapi IR'i type-erased %Liste, T tasimaz). */
    const char *generic_arg_ir;
    /* Matris-A fix / D-005: dtamN (isaretsiz) degisken/parametre.
     * IR tipi ayni (iN) ama udiv/urem/lshr/u-pred + zext gerektirir. */
    int isaretsiz;
    /* D-029 fix: degisken/parametre bir YAPI ya da &Yapi veya *Yapi ise yapinin
     * IR adi ("%T"). Field erisiminde (erisim_uret / erisim_lvalue) DOGRU
     * yapiyi cozmek icin — onceki "ptr ise global alan-adi ara" fallback'i
     * iki yapi ayni alan adini paylasinca YANLIS yapiya cozuyordu. */
    const char *ref_yapi_ir;
    /* D-071 (Sınıf B lambda V2): bu isim bir closure değeri (lambda lokali VEYA
     * işlev(...)→R parametresi) ise 1. Çağrı yerinde indirect-call'ı closure-deref
     * (gep0=fn, gep1=env → call fn(env,args)) yapar; 0 = düz fn-ptr (geriye uyum). */
    int closure_mu;
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
    int donus_isaretsiz;   /* D-005: donus tipi dtamN ise 1 */
    /* Generic islev: AST'yi sakla, instantiation icin gerekli */
    const Dugum *ast;
    int generic_mi;  /* tip_param_sayi > 0 */
    struct IslevKayit *sonraki;
} IslevKayit;

/* Ust duzey sabit kaydi — referans yerlerinde deger ifadesi inline edilir.
 * Ayni dosyadaki + `kullan` ile yuklenen sabitleri codegen'de cozumler
 * (cross-file sabit "; HATA: tanimsiz tanimlayici" hatasinin kok cozumu). */
typedef struct SabitKayit {
    const char *ad;
    int ad_uz;
    const Dugum *deger;    /* sabit.deger — inline edilecek ifade */
    const Dugum *tip;      /* sabit.tip — beklenen IR tipi icin (NULL olabilir) */
    struct SabitKayit *sonraki;
} SabitKayit;

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

/* D-071 (Sınıf B lambda V2): lambda OLUŞTURMA anında kaydedilir, çevre fonksiyon
 * gövdesi bitince (deferred) lifted `@lambda_N(ptr env, params)` olarak emit edilir.
 * Capture listesi OLUŞTURMA anında (scope canlıyken) hesaplanıp burada saklanır —
 * deferred emit'te enclosing scope artık yok. */
typedef struct BekleyenLambda {
    const Dugum *dugum;          /* DUGUM_LAMBDA */
    const char *mangled;         /* @lambda_N */
    const char **capture_adlar;  /* yakalanan serbest değişken adları (ilk-görülme sıralı) */
    int *capture_uzlar;          /* paralel: ad uzunlukları */
    const char **capture_irler;  /* paralel: IR tipleri (env struct alanı + load) */
    int capture_sayi;
    struct BekleyenLambda *sonraki;
} BekleyenLambda;

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
    SabitKayit *sabitler;   /* ust duzey sabit tanimlari (inline icin) */
    /* C2.5: sonuç/seçimlik value codegen — yapısal beklenen tip kanalı.
     * tamam/hata/değer/hiç yapıcıları tam tipi (T, H) buradan okur. */
    const Dugum *beklenen_tip;       /* yapıcının inşa edeceği sonuç/seçimlik tip düğümü */
    const Dugum *aktif_donus_dugum;  /* aktif islevin donus tipi AST düğümü (ver için) */
    int hata_sayisi;                 /* C5 AS001: olumcul codegen hatasi sayaci */
    /* Kampanya seed (a) / D-001: modul uyesi emit edilirken aktif
     * "modul.altmodul" oneki — kardes islevlere ciplak-ad cagrilar
     * islev_bul fallback'inde bu onekle mangle edilip cozulur. */
    const char *aktif_modul_onek;
    int aktif_modul_onek_uz;
    /* D-069 Kat.2 opt-out: güvensiz blok derinliği. >0 iken stack dizi sınır-
     * kontrolü ATLANIR (Rust modeli: varsayılan güvenli, güvensiz'de kontrolsuz —
     * açık + işaretli + programcı sorumluluğunda). */
    int guvensiz_derinlik;
    /* D-071 (Sınıf B lambda V2): lifted lambda emisyon kuyruğu + benzersiz sayaç. */
    BekleyenLambda *bekleyen_lambdalar;
    int lambda_sayaci;
    /* En son üretilen lambda değeri CAPTURE'lı (closure) mı? DEGISKEN, lokal
     * değişkenin closure_mu'sunu buradan okur. Yakalama yok → bare fn-ptr (0). */
    int son_closure;
} LlvmGen;

typedef struct IfadeSonuc {
    int reg;               /* -1 = error */
    const char *tip;       /* IR tipi */
    /* Matris-A fix / D-005: deger isaretsiz (dtamN) mi? IR tipinde
     * isaret bilgisi tasinamadigi icin yan kanal. Mevcut `{ reg, tip }`
     * baslaticilarinda C11 kurali geregi 0 (isaretli) kalir — guvenli
     * varsayilan. */
    int isaretsiz;
} IfadeSonuc;

/* === Forward === */

static IfadeSonuc ifade_uret(LlvmGen *g, const Dugum *d, const char *beklenen);
static IfadeSonuc yapici_uret(LlvmGen *g, const char *yad, int yuz,
                              const Dugum *arg, const Dugum *beklenen);
static int blok_uret(LlvmGen *g, const Dugum *blok);
static void islev_uret(LlvmGen *g, const Dugum *islev);
static int kosul_i1(LlvmGen *g, const Dugum *d);
static YapiKayit *yapi_bul(LlvmGen *g, const char *ad, int ad_uz);
static int mono_emitlendi(LlvmGen *g, const char *mangled);
static const char *mangle_et(LlvmGen *g, const char *ad, int ad_uz,
                              const char **tipler, int tip_sayi);
static int tip_kesirli_mi(const char *ir);
static int tip_genisligi(const char *ir);
static int int_donustur(LlvmGen *g, int src_reg, const char *src_tip,
                        const char *dst_tip);

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

/* === D-071 (Sınıf B lambda V2): capture (serbest değişken) analizi === */
#define LAMBDA_MAX_CAPTURE 32
typedef struct {
    LlvmGen *g;
    const char **param_adlar; const int *param_uzlar; int param_sayi;
    const char *adlar[LAMBDA_MAX_CAPTURE];
    int uzlar[LAMBDA_MAX_CAPTURE];
    const char *irler[LAMBDA_MAX_CAPTURE];
    int sayi;
} CaptureCtx;

static int capture_param_mi(CaptureCtx *c, const char *ad, int uz) {
    for (int i = 0; i < c->param_sayi; i++)
        if (c->param_uzlar[i] == uz &&
            memcmp(c->param_adlar[i], ad, (size_t)uz) == 0) return 1;
    return 0;
}
static void lambda_serbest_tara(CaptureCtx *c, const Dugum *d);
static void lambda_serbest_liste(CaptureCtx *c, Dugum **l, int n) {
    for (int i = 0; i < n; i++) lambda_serbest_tara(c, l[i]);
}
/* Lambda gövdesindeki serbest değişkenleri topla: lambda paramı DEĞİL +
 * isim_bul ile çevre lokal/param ise capture. İç lambda'ya GİRME (kendi yönetir). */
static void lambda_serbest_tara(CaptureCtx *c, const Dugum *d) {
    if (!d) return;
    switch (d->tip) {
        case DUGUM_TANIMLAYICI: {
            const char *ad = d->veri.tanimlayici.metin;
            int uz = d->veri.tanimlayici.uzunluk;
            if (capture_param_mi(c, ad, uz)) return;
            for (int i = 0; i < c->sayi; i++)         /* dedup */
                if (c->uzlar[i] == uz &&
                    memcmp(c->adlar[i], ad, (size_t)uz) == 0) return;
            LlvmIsim *vi = isim_bul(c->g, ad, uz);    /* çevre lokal/param? */
            if (vi && c->sayi < LAMBDA_MAX_CAPTURE) {
                c->adlar[c->sayi] = ad; c->uzlar[c->sayi] = uz;
                c->irler[c->sayi] = vi->llvm_tip ? vi->llvm_tip : "i32";
                c->sayi++;
            }
            return;
        }
        case DUGUM_LAMBDA: return;   /* iç lambda kendi capture'ını yönetir (v1: yok) */
        case DUGUM_BLOK:
            lambda_serbest_liste(c, d->veri.blok.deyimler, d->veri.blok.sayi); return;
        case DUGUM_DEGISKEN: lambda_serbest_tara(c, d->veri.degisken.deger); return;
        case DUGUM_ATAMA:
            lambda_serbest_tara(c, d->veri.atama.hedef);
            lambda_serbest_tara(c, d->veri.atama.deger); return;
        case DUGUM_VER: lambda_serbest_tara(c, d->veri.ver.deger); return;
        case DUGUM_EGER:
            lambda_serbest_tara(c, d->veri.eger.kosul);
            lambda_serbest_tara(c, d->veri.eger.gozdoldur);
            lambda_serbest_tara(c, d->veri.eger.yan); return;
        case DUGUM_IKEN:
            lambda_serbest_tara(c, d->veri.iken.kosul);
            lambda_serbest_tara(c, d->veri.iken.govde); return;
        case DUGUM_ICIN:
            lambda_serbest_tara(c, d->veri.icin.koleksiyon);
            lambda_serbest_tara(c, d->veri.icin.govde); return;
        case DUGUM_IFADE_DEYIMI:
            lambda_serbest_tara(c, d->veri.ifade_deyimi.ifade); return;
        case DUGUM_IKILI:
            lambda_serbest_tara(c, d->veri.ikili.sol);
            lambda_serbest_tara(c, d->veri.ikili.sag); return;
        case DUGUM_TEKLI: lambda_serbest_tara(c, d->veri.tekli.operand); return;
        case DUGUM_CAGRI:
            lambda_serbest_tara(c, d->veri.cagri.hedef);
            lambda_serbest_liste(c, d->veri.cagri.argumanlar, d->veri.cagri.sayi); return;
        case DUGUM_ERISIM: lambda_serbest_tara(c, d->veri.erisim.nesne); return;
        case DUGUM_INDEKS:
            lambda_serbest_tara(c, d->veri.indeks.nesne);
            lambda_serbest_tara(c, d->veri.indeks.indeks); return;
        case DUGUM_DIZI_OLUSTUR:
            lambda_serbest_liste(c, d->veri.dizi_olustur.elemanlar,
                                 d->veri.dizi_olustur.sayi); return;
        case DUGUM_YAPI_OLUSTUR:
            for (int i = 0; i < d->veri.yapi_olustur.alan_sayi; i++) {
                Dugum *aa = d->veri.yapi_olustur.alanlar[i];
                if (aa && aa->tip == DUGUM_ALAN_ATAMA)
                    lambda_serbest_tara(c, aa->veri.alan_atama.deger);
            }
            return;
        case DUGUM_TIP_DONUSTUR: lambda_serbest_tara(c, d->veri.tip_donustur.kaynak); return;
        case DUGUM_KULLAN_IFADE: lambda_serbest_tara(c, d->veri.kullan_ifade.operand); return;
        case DUGUM_IMHA_IFADE: lambda_serbest_tara(c, d->veri.imha_ifade.operand); return;
        case DUGUM_ESLES:
            lambda_serbest_tara(c, d->veri.esles.deger);
            for (int i = 0; i < d->veri.esles.kol_sayi; i++) {
                Dugum *kol = d->veri.esles.kollar[i];
                if (kol && kol->tip == DUGUM_ESLES_KOLU)
                    lambda_serbest_tara(c, kol->veri.esles_kolu.govde);
            }
            return;
        default: return;
    }
}

/* === Ust duzey sabit tablosu === */

static void sabit_kayit(LlvmGen *g, const Dugum *d) {
    if (!d || d->tip != DUGUM_SABIT) return;
    SabitKayit *s = (SabitKayit *)arena_ayir_sifir(g->arena, sizeof(SabitKayit));
    if (!s) return;
    s->ad = d->veri.sabit.ad;
    s->ad_uz = d->veri.sabit.ad_uzunluk;
    s->deger = d->veri.sabit.deger;
    s->tip = d->veri.sabit.tip;
    s->sonraki = g->sabitler;
    g->sabitler = s;
}

static SabitKayit *sabit_bul(LlvmGen *g, const char *ad, int ad_uz) {
    for (SabitKayit *s = g->sabitler; s; s = s->sonraki) {
        if (s->ad_uz == ad_uz && memcmp(s->ad, ad, (size_t)ad_uz) == 0) {
            return s;
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

/* Liste<T> stdlib fix: IR yerel ad (%ad) yazimi — Turkce (non-ASCII)
 * karakterli KEMGU adlari (örn. parametre 'böl') ciplak %böl olarak
 * GECERSIZ IR uretiyordu (LLVM identifier ASCII). Non-ASCII veya IR'da
 * ozel karakter varsa LLVM'in tirnakli formu kullanilir: %"böl". */
static void yerel_ad_yaz(FILE *out, const char *ad, int ad_uz) {
    int ascii_guvenli = 1;
    for (int i = 0; i < ad_uz; i++) {
        unsigned char c = (unsigned char)ad[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '.' ||
              c == '$' || c == '-')) {
            ascii_guvenli = 0;
            break;
        }
    }
    if (ascii_guvenli) {
        fwrite(ad, 1, (size_t)ad_uz, out);
    } else {
        fputc('"', out);
        fwrite(ad, 1, (size_t)ad_uz, out);
        fputc('"', out);
    }
}

/* Türkçe kimlik: yapı/çeşit IR tip adı "%Ad" — non-ASCII (Türkçe) ad LLVM'de
 * quote'lanmalı (`%"Düğüm"`). yerel_ad_yaz ile aynı ASCII-güvenli kuralı;
 * arena'da null-terminated döner (tanım + tüm kullanımlar TUTARLI). */
static const char *yapi_ad_ir(LlvmGen *g, const char *ad, int uz) {
    int ascii = 1;
    for (int i = 0; i < uz; i++) {
        unsigned char c = (unsigned char)ad[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '.' ||
              c == '$' || c == '-')) {
            ascii = 0;
            break;
        }
    }
    int extra = ascii ? 1 : 3;  /* '%'  ya da  '%' '"' '"' */
    char *buf = (char *)arena_ayir(g->arena, (size_t)uz + (size_t)extra + 1);
    if (!buf) return "%yapi";
    int o = 0;
    buf[o++] = '%';
    if (!ascii) buf[o++] = '"';
    memcpy(buf + o, ad, (size_t)uz);
    o += uz;
    if (!ascii) buf[o++] = '"';
    buf[o] = '\0';
    return buf;
}

static YapiKayit *yapi_bul(LlvmGen *g, const char *ad, int ad_uz);

/* "%Ad" veya quote'lu "%\"Ad\"" IR tip stringinden YapiKayit bul. yapi_ad_ir
 * ile simetrik (Türkçe ad quote'lanır; bu okuma tarafı quote'u soyar). */
static YapiKayit *yapi_bul_ir(LlvmGen *g, const char *ir) {
    if (!ir || ir[0] != '%') return NULL;
    const char *nm = ir + 1;
    int nl = (int)strlen(nm);
    if (nl >= 2 && nm[0] == '"' && nm[nl - 1] == '"') {
        nm++;
        nl -= 2;
    }
    return yapi_bul(g, nm, nl);
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

/* v1 bölge-container: AST tip dugumu *T ise pointee'nin IR tipi,
 * degilse NULL. (LlvmIsim.pointee_llvm_tip kaynaklari icin.) */
static const char *ast_tip_to_ir(LlvmGen *g, const Dugum *tip_d);
static const char *pointee_ir_al(LlvmGen *g, const Dugum *tip_d) {
    if (tip_d && tip_d->tip == DUGUM_TIP_POINTER) {
        return ast_tip_to_ir(g, tip_d->veri.tip_pointer.hedef_tip);
    }
    return NULL;
}

/* D-029 fix: tip dugumu bir YAPI ya da &Yapi veya *Yapi ise yapinin IR adi ("%T"),
 * degilse NULL. Referans/pointer soyulur; ic tip yapi-kayitli ise ast_tip_to_ir
 * "%Ad" doner (Dizi/seçimlik/sonuç/jenerik degil — yalniz nominal yapi). Field
 * erisiminde nesnenin DOGRU yapi tipini (global alan-adi tahmini yerine) verir. */
static const char *ref_yapi_ir_al(LlvmGen *g, const Dugum *tip_d) {
    if (!tip_d) return NULL;
    const Dugum *ic = tip_d;
    if (ic->tip == DUGUM_TIP_REFERANS) ic = ic->veri.tip_referans.hedef_tip;
    else if (ic->tip == DUGUM_TIP_POINTER) ic = ic->veri.tip_pointer.hedef_tip;
    const char *ir = ast_tip_to_ir(g, ic);
    if (ir && ir[0] == '%') return ir;   /* yalniz yapi (%Ad) */
    return NULL;
}

/* Liste<T> BUG-2 fix: annot Kullanici<X> (tek tip-arg) ise X'in IR'i.
 * &Kullanici<X> icin referans soyulur. Subst aktifken (specialize
 * govdesi) X=T generic parami da dogru IR'a cozulur — ic cagri zinciri. */
static const char *generic_arg_ir_al(LlvmGen *g, const Dugum *tip_d) {
    if (tip_d && tip_d->tip == DUGUM_TIP_REFERANS) {
        tip_d = tip_d->veri.tip_referans.hedef_tip;
    }
    if (tip_d && tip_d->tip == DUGUM_TIP_KULLANICI &&
        tip_d->veri.tip_kullanici.tip_arg_sayi == 1) {
        return ast_tip_to_ir(g, tip_d->veri.tip_kullanici.tip_arg[0]);
    }
    return NULL;
}

/* === C2.7: çeşit (sum type) codegen yardımcıları === */

/* çeşit discriminant IR tipi: ≤256 varyant → i8, değilse i16. */
static const char *cesit_disc_ir(const Dugum *c) {
    return c->veri.cesit.varyant_sayi > 256 ? "i16" : "i8";
}

/* C3: çeşit'in payload taşıyan varyantı var mı? Varsa tagged-union struct
 * temsili (`%Ad = {iDISC, alanlar}`); yoksa bare iN disc (eski davranış). */
static int cesit_payload_var(const Dugum *c) {
    if (!c->veri.cesit.varyant_payload_sayilari) return 0;
    for (int i = 0; i < c->veri.cesit.varyant_sayi; i++) {
        if (c->veri.cesit.varyant_payload_sayilari[i] > 0) return 1;
    }
    return 0;
}

/* C3: vi. varyantın payload alanlarının struct'taki başlangıç indeksi.
 * Alan 0 = disc; varyantlar bildirim sırasıyla peş peşe (sonuç {tag,T,H}
 * deseni). offset(vi) = 1 + sum(payload_sayilari[0..vi-1]). */
static int cesit_varyant_alan_ofset(const Dugum *c, int vi) {
    int ofs = 1;
    if (!c->veri.cesit.varyant_payload_sayilari) return ofs;
    for (int i = 0; i < vi && i < c->veri.cesit.varyant_sayi; i++) {
        ofs += c->veri.cesit.varyant_payload_sayilari[i];
    }
    return ofs;
}

static int cesit_varyant_payload_n(const Dugum *c, int vi) {
    if (vi < 0 || vi >= c->veri.cesit.varyant_sayi ||
        !c->veri.cesit.varyant_payload_sayilari) return 0;
    return c->veri.cesit.varyant_payload_sayilari[vi];
}

/* C3: payload çeşit'in IR struct adı "%Ad" (arena). */
static const char *cesit_struct_ir(LlvmGen *g, const Dugum *cd) {
    return yapi_ad_ir(g, cd->veri.cesit.ad, cd->veri.cesit.ad_uzunluk);
}

/* C3 çapraz-modül: çeşit YapiKayit'ını sol-yoldan çöz — sol TANIMLAYICI
 * (Renk) ise adından, YOL (m::Renk) ise sag_ad'inden (çeşit adı düz IR-ad
 * uzayında, D-011). DUGUM_CESIT değilse NULL. */
static YapiKayit *cesit_kayit_yoldan(LlvmGen *g, const Dugum *sol) {
    if (!sol) return NULL;
    YapiKayit *yk = NULL;
    if (sol->tip == DUGUM_TANIMLAYICI) {
        yk = yapi_bul(g, sol->veri.tanimlayici.metin,
                      sol->veri.tanimlayici.uzunluk);
    } else if (sol->tip == DUGUM_YOL) {
        yk = yapi_bul(g, sol->veri.yol.sag_ad,
                      sol->veri.yol.sag_ad_uzunluk);
    }
    if (yk && yk->ast && yk->ast->tip == DUGUM_CESIT) return yk;
    return NULL;
}

/* C3: çeşit varyant değeri inşası. Payloadsuz çeşit → bare iN disc sabiti
 * (eski davranış). Payload çeşit → {iDISC, alanlar} struct (alloca + GEP+
 * store + load, yapici_uret deseni). cagri varsa argümanları payload'a yazar
 * (Cesit::V(args)); yoksa yalnız disc (bare Cesit::V — payloadsuz varyant). */
static IfadeSonuc cesit_yapici_uret(LlvmGen *g, const Dugum *cd, int vi,
                                    const Dugum *cagri, int n) {
    const char *disc = cesit_disc_ir(cd);
    if (!cesit_payload_var(cd)) {
        int r = yeni_reg(g);
        fprintf(g->out, "  %%%d = add %s 0, %d\n", r, disc, vi);
        IfadeSonuc s = { r, disc, 0 };
        return s;
    }
    const char *agg = cesit_struct_ir(g, cd);
    int ar = yeni_reg(g);
    fprintf(g->out, "  %%%d = alloca %s\n", ar, agg);
    int gt = yeni_reg(g);
    fprintf(g->out, "  %%%d = getelementptr %s, ptr %%%d, i32 0, i32 0\n",
            gt, agg, ar);
    fprintf(g->out, "  store %s %d, ptr %%%d\n", disc, vi, gt);
    int pn = cesit_varyant_payload_n(cd, vi);
    int ofs = cesit_varyant_alan_ofset(cd, vi);
    for (int j = 0; j < pn && cagri && j < n; j++) {
        const char *pir = ast_tip_to_ir(g,
            cd->veri.cesit.varyant_payload_tipleri[vi][j]);
        if (!pir || strcmp(pir, "void") == 0) pir = "i8";
        IfadeSonuc pv = ifade_uret(g, cagri->veri.cagri.argumanlar[j], pir);
        int pr = pv.reg;
        if (!tip_kesirli_mi(pir) && !tip_kesirli_mi(pv.tip) &&
            tip_genisligi(pir) > 0 && tip_genisligi(pv.tip) > 0) {
            pr = int_donustur(g, pv.reg, pv.tip, pir);
        }
        int gp = yeni_reg(g);
        fprintf(g->out,
                "  %%%d = getelementptr %s, ptr %%%d, i32 0, i32 %d\n",
                gp, agg, ar, ofs + j);
        fprintf(g->out, "  store %s %%%d, ptr %%%d\n", pir, pr, gp);
    }
    int lr = yeni_reg(g);
    fprintf(g->out, "  %%%d = load %s, ptr %%%d\n", lr, agg, ar);
    IfadeSonuc s = { lr, agg, 0 };
    return s;
}

/* Varyant adından tag indeksi (bildirim sırası). -1 = bulunamadı. */
static int cesit_varyant_indeksi(const Dugum *c, const char *ad, int uz) {
    for (int i = 0; i < c->veri.cesit.varyant_sayi; i++) {
        if (c->veri.cesit.varyant_uzunluklar[i] == uz &&
            memcmp(c->veri.cesit.varyantlar[i], ad, (size_t)uz) == 0) {
            return i;
        }
    }
    return -1;
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
        if (ESLES("bo" "\xc5\x9f") || ESLES("bos")) return "void";
        #undef ESLES
        /* Taninmayan basit tip — kullanici yapisi olabilir mi? */
        YapiKayit *yk = yapi_bul(g, a, u);
        if (yk) {
            /* çeşit: payloadsuz → bare iN disc; payload → %Ad struct (aşağı). */
            if (yk->ast && yk->ast->tip == DUGUM_CESIT &&
                !cesit_payload_var(yk->ast)) {
                return cesit_disc_ir(yk->ast);
            }
            return yapi_ad_ir(g, yk->ad, yk->ad_uz);
        }
        return "i32";
    }
    if (tip_d->tip == DUGUM_TIP_KULLANICI && tip_d->veri.tip_kullanici.yol) {
        const Dugum *y = tip_d->veri.tip_kullanici.yol;
        /* Yapi adi: niteliksiz (Liste<T>) tanimlayici'dan, nitelikli
         * (modül::Tip — D dilim-1) YOL'un sag_ad'indan. Modül-içi yapilar
         * düz IR-ad uzayinda (type-erased %Liste, D-011) — sag_ad düz bul. */
        const char *yad = NULL;
        int yuz = 0;
        if (y->tip == DUGUM_TANIMLAYICI) {
            yad = y->veri.tanimlayici.metin;
            yuz = y->veri.tanimlayici.uzunluk;
        } else if (y->tip == DUGUM_YOL) {
            yad = y->veri.yol.sag_ad;
            yuz = y->veri.yol.sag_ad_uzunluk;
        }
        if (yad) {
            /* Yapi tipi mi? Kayitliysa "%Ad" doner (struct-by-value),
             * degilse "ptr" (trait vb. kullanici tipi). */
            YapiKayit *yk = yapi_bul(g, yad, yuz);
            if (yk) {
                if (yk->ast && yk->ast->tip == DUGUM_CESIT &&
                    !cesit_payload_var(yk->ast)) {
                    return cesit_disc_ir(yk->ast);
                }
                return yapi_ad_ir(g, yk->ad, yk->ad_uz);
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
    /* C2.5: sonuç<T,H> → {i8 tag, T, H} (tag: 0=tamam, 1=hata).
     *       seçimlik<T> → {i8 tag, T} (tag: 0=değer, 1=hiç).
     * Ayrık alanlar (union değil): by-value LLVM aggregate, struct ABI'siyle
     * aynı. void/boş payload → i8 dummy (LLVM struct alanı sized olmalı). */
    if (tip_d->tip == DUGUM_TIP_SONUC) {
        const char *t_ir = ast_tip_to_ir(g, tip_d->veri.tip_sonuc.deger_tip);
        const char *h_ir = ast_tip_to_ir(g, tip_d->veri.tip_sonuc.hata_tip);
        if (!t_ir || strcmp(t_ir, "void") == 0) t_ir = "i8";
        if (!h_ir || strcmp(h_ir, "void") == 0) h_ir = "i8";
        int sz = (int)strlen(t_ir) + (int)strlen(h_ir) + 16;
        char *buf = (char *)arena_ayir(g->arena, (size_t)sz);
        if (buf) {
            snprintf(buf, (size_t)sz, "{i8, %s, %s}", t_ir, h_ir);
            return buf;
        }
        return "i32";
    }
    if (tip_d->tip == DUGUM_TIP_SECIMLIK) {
        const char *t_ir = ast_tip_to_ir(g, tip_d->veri.tip_secimlik.ic_tip);
        if (!t_ir || strcmp(t_ir, "void") == 0) t_ir = "i8";
        int sz = (int)strlen(t_ir) + 12;
        char *buf = (char *)arena_ayir(g->arena, (size_t)sz);
        if (buf) {
            snprintf(buf, (size_t)sz, "{i8, %s}", t_ir);
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

/* D-029 fix (2): dizi-builtin arg0 bir struct ALANI (s.ad, DUGUM_ERISIM) ise
 * alanin Dizi<T> eleman IR tipini ("ptr"/"i64"/"i32") doner — yoksa NULL.
 * dizi_eleman_beklenen yalniz TANIMLAYICI arg0 (duz degisken) icin set ediliyordu;
 * struct-alan tutan dizi (s.ad: Dizi<metin>) icin eleman tipi cikarsanmiyor ->
 * dizi_al kdl_dizi_al_tam (i32) route edip metin ptr'ini i32 okuyor -> SEGFAULT. */
static const char *dizi_alan_eleman_ir(LlvmGen *g, const Dugum *erisim) {
    if (!erisim || erisim->tip != DUGUM_ERISIM) return NULL;
    const Dugum *nesne = erisim->veri.erisim.nesne;
    YapiKayit *yk = NULL;
    if (nesne && nesne->tip == DUGUM_TANIMLAYICI) {
        LlvmIsim *vi = isim_bul(g, nesne->veri.tanimlayici.metin,
                                nesne->veri.tanimlayici.uzunluk);
        if (vi) {
            if (vi->llvm_tip && vi->llvm_tip[0] == '%')
                yk = yapi_bul_ir(g, vi->llvm_tip);
            else if (vi->ref_yapi_ir)
                yk = yapi_bul_ir(g, vi->ref_yapi_ir);
        }
    }
    if (!yk) return NULL;
    const Dugum *alan_tip = NULL;
    int idx = yapi_alan_indeksi(yk, erisim->veri.erisim.alan,
                                erisim->veri.erisim.alan_uzunluk, &alan_tip);
    if (idx < 0 || !alan_tip || alan_tip->tip != DUGUM_TIP_DIZI) return NULL;
    return ast_tip_to_ir(g, alan_tip->veri.tip_dizi.eleman_tip);
}

/* [D-082] Dizi<T> literali HEAP KdlDizi olarak üret (kdl_dizi_olustur +
 * dizi_ekle), stack [N x T] DEĞİL. Dönüş: KdlDizi* tutan SSA register no.
 *
 * Ortak yardımcı: hem değişken-init (DUGUM_DEGISKEN) hem ATAMA (DUGUM_ATAMA)
 * bu yolu çağırır. Bir dizi-literali Dizi<T> slotuna (yerel değişken veya
 * yapı alanı) yazılırken AYNI heap-promote uygulanmalı — aksi halde slot'a
 * stack pointer'ı store edilir, sonraki dizi_ekle/dizi_boyut KdlDizi*
 * beklerken stack-array görür → SEGFAULT (accept-but-crash; D-070'in ATAMA
 * analoğu, bkz. D-075 🔴 KEŞİF notu).
 *
 * lit       — DUGUM_DIZI_OLUSTUR düğümü
 * eleman_ir — eleman IR tipi ("i8"/"i16"/"i32"/"i64"/"double"/"ptr") */
static int dizi_literal_heap_uret(LlvmGen *g, const Dugum *lit,
                                  const char *eleman_ir) {
    int eb = 4;
    if (strcmp(eleman_ir, "i8") == 0) eb = 1;
    else if (strcmp(eleman_ir, "i16") == 0) eb = 2;
    else if (strcmp(eleman_ir, "i64") == 0) eb = 8;
    else if (strcmp(eleman_ir, "double") == 0) eb = 8;
    else if (strcmp(eleman_ir, "ptr") == 0) eb = 8;
    int n = lit->veri.dizi_olustur.sayi;
    int kdl_reg = yeni_reg(g);
    fprintf(g->out,
        "  %%%d = call ptr @kdl_dizi_olustur(i32 %d)\n", kdl_reg, eb);
    for (int i = 0; i < n; i++) {
        IfadeSonuc v = ifade_uret(g, lit->veri.dizi_olustur.elemanlar[i],
                                  eleman_ir);
        int vr = int_donustur(g, v.reg, v.tip, eleman_ir);
        const char *fn = "kdl_dizi_ekle_tam";
        if (strcmp(eleman_ir, "i64") == 0) fn = "kdl_dizi_ekle_tam64";
        else if (strcmp(eleman_ir, "ptr") == 0) fn = "kdl_dizi_ekle_ptr";
        fprintf(g->out,
            "  call void @%s(ptr %%%d, %s %%%d)\n",
            fn, kdl_reg, eleman_ir, vr);
    }
    return kdl_reg;
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
static int int_donustur_im(LlvmGen *g, int src_reg, const char *src_tip,
                           const char *dst_tip, int isaretsiz) {
    if (!src_tip || !dst_tip || strcmp(src_tip, dst_tip) == 0) return src_reg;
    /* SIMD Spec V1: vektör tipler arasında otomatik conversion yok (tip
     * kontrolü zaten engelliyor); src değişmeden döner. */
    if (src_tip[0] == '<' || dst_tip[0] == '<') return src_reg;
    int src_w = tip_genisligi(src_tip);
    int dst_w = tip_genisligi(dst_tip);
    if (src_w == 0 || dst_w == 0) return src_reg;
    int r = yeni_reg(g);
    if (src_w < dst_w) {
        /* Matris-A fix / D-005: i1 HER ZAMAN zext (dogru=1; sext -1
         * yapardi — '41 + (b olarak tam32)' 40 donuyordu). dtamN
         * (isaretsiz) genisletme de zext (200 -> 200, -56 degil). */
        if (strcmp(src_tip, "i1") == 0 || isaretsiz) {
            fprintf(g->out, "  %%%d = zext %s %%%d to %s\n",
                    r, src_tip, src_reg, dst_tip);
        } else {
            fprintf(g->out, "  %%%d = sext %s %%%d to %s\n",
                    r, src_tip, src_reg, dst_tip);
        }
    } else {
        /* Daralt */
        fprintf(g->out, "  %%%d = trunc %s %%%d to %s\n",
                r, src_tip, src_reg, dst_tip);
    }
    return r;
}

static int int_donustur(LlvmGen *g, int src_reg, const char *src_tip,
                        const char *dst_tip) {
    return int_donustur_im(g, src_reg, src_tip, dst_tip, 0);
}

/* D-005: AST tip dugumu isaretsiz tamsayi (dtamN) mi? */
static int ast_tip_isaretsiz_mi(const Dugum *tip_d) {
    if (!tip_d || tip_d->tip != DUGUM_TIP_BASIT) return 0;
    return tip_d->veri.tip_basit.ad_uzunluk >= 4 &&
           memcmp(tip_d->veri.tip_basit.ad, "dtam", 4) == 0;
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
        case DUGUM_MODUL:
            /* A: modul uyeleri de taranir — onceden modul icindeki metin
             * literalleri @.str.N olarak toplanmiyordu (gap). */
            ast_taransa_metinleri_liste(g, d->veri.modul.uyeler,
                                         d->veri.modul.sayi); break;
        case DUGUM_ESLES:
            ast_taransa_metinleri(g, d->veri.esles.deger);
            for (int i = 0; i < d->veri.esles.kol_sayi; i++) {
                Dugum *kol = d->veri.esles.kollar[i];
                if (kol && kol->tip == DUGUM_ESLES_KOLU) {
                    ast_taransa_metinleri(g, kol->veri.esles_kolu.govde);
                }
            }
            break;
        /* Madde A genişletme: 'x olarak T' cast'i + diğer ifade sarmalayan
         * düğümler önceden taranmıyordu → altlarındaki metin literali
         * "kayitsiz" düşüp `add i32 0,0`'a derleniyordu (sessiz hatalı
         * codegen). En yaygın: `metin_uzunluk("...") olarak tam32`. */
        case DUGUM_TIP_DONUSTUR:
            ast_taransa_metinleri(g, d->veri.tip_donustur.kaynak); break;
        case DUGUM_LAMBDA:
            ast_taransa_metinleri(g, d->veri.lambda.govde); break;
        case DUGUM_KULLAN_IFADE:
            ast_taransa_metinleri(g, d->veri.kullan_ifade.operand); break;
        case DUGUM_IMHA_IFADE:
            ast_taransa_metinleri(g, d->veri.imha_ifade.operand); break;
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
    /* D-005: dtamN donusu cagri sonucuna tasinir */
    r->donus_isaretsiz = ast_tip_isaretsiz_mi(i->veri.islev.donus_tipi);
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

/* === Kampanya seed (a) / DECISIONS_LOG D-001: modul ad-mangling ===
 * Sema: @<modul>.<ad> — ic ice modul "<m1>.<m2>.<ad>". LLVM @ adlari
 * nokta icerebilir (örn. @llvm.x86.*); KEMGU kullanici adlarinda '.'
 * olamayacagi icin cakisma riski yok. Onceki durum: modul uyeleri HIC
 * emit edilmiyordu + mat::f() cagrisi sessiz 0 donerdi (audit DUR-SOR
 * #2). Tip kontrolu modul scope'unu zaten cozuyor; bu katman yalniz
 * IR ad uzayini duzlestirir. */
static const char *modul_mangle(LlvmGen *g, const char *onek, int onek_uz,
                                const char *ad, int ad_uz, int *out_uz) {
    char *m = (char *)arena_ayir(g->arena,
                                 (size_t)onek_uz + 1 + (size_t)ad_uz + 1);
    if (!m) return NULL;
    memcpy(m, onek, (size_t)onek_uz);
    m[onek_uz] = '.';
    memcpy(m + onek_uz + 1, ad, (size_t)ad_uz);
    m[onek_uz + 1 + ad_uz] = '\0';
    if (out_uz) *out_uz = onek_uz + 1 + ad_uz;
    return m;
}

/* mat::alt::f yol zincirini "mat.alt.f" noktali ada knit eder.
 * Donus: yazilan uzunluk, sigmazsa/desteklenmeyen dugumde -1. */
static int yol_noktali_ad(const Dugum *y, char *buf, int kap) {
    if (!y) return -1;
    if (y->tip == DUGUM_TANIMLAYICI) {
        int u = y->veri.tanimlayici.uzunluk;
        if (u >= kap) return -1;
        memcpy(buf, y->veri.tanimlayici.metin, (size_t)u);
        return u;
    }
    if (y->tip == DUGUM_YOL) {
        int o = yol_noktali_ad(y->veri.yol.sol, buf, kap);
        if (o < 0) return -1;
        int su = y->veri.yol.sag_ad_uzunluk;
        if (o + 1 + su >= kap) return -1;
        buf[o] = '.';
        memcpy(buf + o + 1, y->veri.yol.sag_ad, (size_t)su);
        return o + 1 + su;
    }
    return -1;
}

/* Modul uyelerini (islev + disa islev + ic ice modul) mangled adla
 * islev kayit tablosuna ekle. AST kopyasi arena'da kalici (kayit
 * ast pointer'i saklar; specialize_emit'in sahte-dugum deseni). */
static void modul_uyeleri_kayit(LlvmGen *g, const Dugum *m,
                                const char *onek, int onek_uz) {
    for (int i = 0; i < m->veri.modul.sayi; i++) {
        const Dugum *uye = m->veri.modul.uyeler[i];
        if (uye && uye->tip == DUGUM_DISA && uye->veri.disa.tanim) {
            uye = uye->veri.disa.tanim;
        }
        if (!uye) continue;
        if (uye->tip == DUGUM_ISLEV) {
            int muz = 0;
            const char *mangled = modul_mangle(g, onek, onek_uz,
                uye->veri.islev.ad, uye->veri.islev.ad_uzunluk, &muz);
            if (!mangled) continue;
            Dugum *kopya = (Dugum *)arena_ayir(g->arena, sizeof(Dugum));
            if (!kopya) continue;
            *kopya = *uye;
            kopya->veri.islev.ad = mangled;
            kopya->veri.islev.ad_uzunluk = muz;
            islev_kayit(g, kopya);
        } else if (uye->tip == DUGUM_MODUL) {
            int yeni_uz = 0;
            const char *yeni_onek = modul_mangle(g, onek, onek_uz,
                uye->veri.modul.ad, uye->veri.modul.ad_uzunluk, &yeni_uz);
            if (yeni_onek) {
                modul_uyeleri_kayit(g, uye, yeni_onek, yeni_uz);
            }
        }
    }
}

/* Modul uyelerini emit et (kayit asamasinda olusan mangled-adli AST
 * kopyalari uzerinden). aktif_modul_onek emit suresince set edilir —
 * govde icindeki kardes ciplak-ad cagrilar islev_bul fallback'iyle
 * "<onek>.<ad>"a cozulur. */
static void modul_uyeleri_emit(LlvmGen *g, const Dugum *m,
                               const char *onek, int onek_uz) {
    for (int i = 0; i < m->veri.modul.sayi; i++) {
        const Dugum *uye = m->veri.modul.uyeler[i];
        if (uye && uye->tip == DUGUM_DISA && uye->veri.disa.tanim) {
            uye = uye->veri.disa.tanim;
        }
        if (!uye) continue;
        if (uye->tip == DUGUM_ISLEV && uye->veri.islev.govde) {
            int muz = 0;
            const char *mangled = modul_mangle(g, onek, onek_uz,
                uye->veri.islev.ad, uye->veri.islev.ad_uzunluk, &muz);
            IslevKayit *ik = mangled ? islev_bul(g, mangled, muz) : NULL;
            if (ik && ik->ast) {
                const char *eski_onek = g->aktif_modul_onek;
                int eski_uz = g->aktif_modul_onek_uz;
                g->aktif_modul_onek = onek;
                g->aktif_modul_onek_uz = onek_uz;
                islev_uret(g, ik->ast);
                g->aktif_modul_onek = eski_onek;
                g->aktif_modul_onek_uz = eski_uz;
            }
        } else if (uye->tip == DUGUM_MODUL) {
            int yeni_uz = 0;
            const char *yeni_onek = modul_mangle(g, onek, onek_uz,
                uye->veri.modul.ad, uye->veri.modul.ad_uzunluk, &yeni_uz);
            if (yeni_onek) {
                modul_uyeleri_emit(g, uye, yeni_onek, yeni_uz);
            }
        }
    }
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

/* C2.7: çeşit'i yapilar listesine kaydet (ast = DUGUM_CESIT; ast_tip_to_ir
 * ve DUGUM_YOL bunu çeşit olarak tanır). */
static void cesit_kayit(LlvmGen *g, const Dugum *c) {
    YapiKayit *r = (YapiKayit *)arena_ayir_sifir(g->arena, sizeof(YapiKayit));
    if (!r) return;
    r->ad = c->veri.cesit.ad;
    r->ad_uz = c->veri.cesit.ad_uzunluk;
    r->ast = c;
    r->sonraki = g->yapilar;
    g->yapilar = r;
}

/* A: modul icindeki yapi/cesit tanimlarini kaydet (recursive — ic ice
 * moduller dahil). Onceki gap: yapi pre-pass'i yalniz top-level'a
 * bakiyordu; modul icinde tanimlanan struct '%Ad = type' olarak HIC
 * emit edilmiyordu -> modul fonksiyonunda yapi kullanimi gecersiz IR.
 * Adlar duz (mangling'siz) IR tip uzayina kaydedilir; ayni adli ikinci
 * kayit ATLANIR (ilk kazanir — v1 siniri, capraz-modul ayni-adli
 * struct'lar D'de nitelikli tip ile ayrisacak). */
static void modul_tipleri_kayit(LlvmGen *g, const Dugum *m) {
    for (int i = 0; i < m->veri.modul.sayi; i++) {
        const Dugum *uye = m->veri.modul.uyeler[i];
        if (uye && uye->tip == DUGUM_DISA && uye->veri.disa.tanim) {
            uye = uye->veri.disa.tanim;
        }
        if (!uye) continue;
        if (uye->tip == DUGUM_YAPI) {
            if (!yapi_bul(g, uye->veri.yapi.ad, uye->veri.yapi.ad_uzunluk)) {
                yapi_kayit(g, uye);
            }
        } else if (uye->tip == DUGUM_CESIT) {
            if (!yapi_bul(g, uye->veri.cesit.ad,
                          uye->veri.cesit.ad_uzunluk)) {
                cesit_kayit(g, uye);
            }
        } else if (uye->tip == DUGUM_MODUL) {
            modul_tipleri_kayit(g, uye);
        }
    }
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
        /* C3: çeşit — payload taşıyorsa tagged-union struct, taşımıyorsa
         * bare iN disc (struct tipi gerekmez, atla). */
        if (y->ast && y->ast->tip == DUGUM_CESIT) {
            const Dugum *c = y->ast;
            if (!cesit_payload_var(c)) continue;
            fputs("%", g->out);
            yerel_ad_yaz(g->out, y->ad, y->ad_uz);
            fprintf(g->out, " = type { %s", cesit_disc_ir(c));
            for (int vi = 0; vi < c->veri.cesit.varyant_sayi; vi++) {
                int pn = cesit_varyant_payload_n(c, vi);
                for (int j = 0; j < pn; j++) {
                    const char *ir = ast_tip_to_ir(g,
                        c->veri.cesit.varyant_payload_tipleri[vi][j]);
                    fprintf(g->out, ", %s", ir ? ir : "i32");
                }
            }
            fputs(" }\n", g->out);
            continue;
        }
        fputs("%", g->out);
        yerel_ad_yaz(g->out, y->ad, y->ad_uz);
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
    IfadeSonuc s = { r, "i32", 0 };
    return s;
}

/* Tanimlayici cozumle: alloca'dan load. */
static IfadeSonuc tanimlayici_yukle(LlvmGen *g, const Dugum *d,
                                    const char *beklenen) {
    /* C2.5: hiç → seçimlik<T> {i8 1, undef} (beklenen seçimlik ise). */
    if (d->veri.tanimlayici.uzunluk == 4 &&
        memcmp(d->veri.tanimlayici.metin, "hi\xc3\xa7", 4) == 0 &&
        g->beklenen_tip && g->beklenen_tip->tip == DUGUM_TIP_SECIMLIK) {
        return yapici_uret(g, "hi\xc3\xa7", 4, NULL, g->beklenen_tip);
    }
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
            yerel_ad_yaz(g->out, d->veri.tanimlayici.metin,
                   d->veri.tanimlayici.uzunluk);
            fputs(" to ptr\n", g->out);
            IfadeSonuc s = { r, "ptr", 0 };
            return s;
        }
        /* Ust duzey sabit mi? Deger ifadesini inline et. Boylece ayni
         * dosyadaki ve `kullan` ile yuklenen sabitler codegen'de cozulur
         * (cross-file "; HATA: tanimsiz tanimlayici" sorununun kok cozumu). */
        SabitKayit *sk = sabit_bul(g, d->veri.tanimlayici.metin,
                                   d->veri.tanimlayici.uzunluk);
        if (sk && sk->deger) {
            const char *sb = beklenen;
            if (!sb && sk->tip) sb = ast_tip_to_ir(g, sk->tip);
            return ifade_uret(g, sk->deger, sb);
        }
        return hata(g, "tanimsiz tanimlayici");
    }
    int r = yeni_reg(g);
    fprintf(g->out, "  %%%d = load %s, ptr %%%d\n",
            r, i->llvm_tip, i->reg_no);
    IfadeSonuc s = { r, i->llvm_tip, i->isaretsiz };
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

/* D-005: isaretsiz (dtamN) karsilastirma — u-pred'ler. Onceki durum:
 * dtam8 200 > 100 signed icmp'le (-56 > 100) YANLIS donuyordu. */
static const char *icmp_pred_u(Operator op) {
    switch (op) {
        case OP_ESIT:        return "eq";
        case OP_ESIT_DEGIL:  return "ne";
        case OP_KUCUK:       return "ult";
        case OP_BUYUK:       return "ugt";
        case OP_KUCUK_ESIT:  return "ule";
        case OP_BUYUK_ESIT:  return "uge";
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
    IfadeSonuc res = { r, "ptr", 0 };
    return res;
}

/* DUGUM_YAPI_OLUSTUR -> alloca + GEP/store alanlar + load struct value */
static IfadeSonuc yapi_olustur_uret(LlvmGen *g, const Dugum *d) {
    YapiKayit *y = yapi_bul(g, d->veri.yapi_olustur.tip_ad,
                             d->veri.yapi_olustur.tip_ad_uzunluk);
    if (!y) return hata(g, "yapi tipi bilinmiyor");

    /* Tip stringi: "%Ad" (Türkçe ad ise quote'lu — yapi_ad_ir). */
    const char *yapi_ir = yapi_ad_ir(g, y->ad, y->ad_uz);

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
        /* [D-044] Alan tipini beklenen_tip kanalına koy: Dizi<T> alanına `[]`/
         * `[...]` verilince DIZI_OLUSTUR heap KdlDizi üretsin (stack değil). */
        const Dugum *eski_bt = g->beklenen_tip;
        g->beklenen_tip = alan_tip_d;
        IfadeSonuc deger = ifade_uret(g, aa->veri.alan_atama.deger, alan_ir);
        g->beklenen_tip = eski_bt;
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
    IfadeSonuc s = { load_r, yapi_ir, 0 };
    return s;
}

/* DUGUM_ERISIM -> struct value uzerinde extractvalue, ptr uzerinde GEP+load */
static IfadeSonuc erisim_uret(LlvmGen *g, const Dugum *d) {
    IfadeSonuc nesne = ifade_uret(g, d->veri.erisim.nesne, NULL);

    /* Yapi tipini cikar: nesne.tip "%Ad" ise yapi adi, "ptr" ise once
     * nesnenin KAYITLI yapi tipi (ref_yapi_ir), yoksa alan-adi arama. */
    YapiKayit *yk = NULL;
    if (nesne.tip && nesne.tip[0] == '%') {
        yk = yapi_bul_ir(g, nesne.tip);
    } else {
        /* D-029 fix: nesne TANIMLAYICI ise (&Yapi param/lokal) kayitli yapi
         * tipini kullan — global alan-adi tahmini iki yapi ayni alan adini
         * paylasinca YANLIS yapiya cozuyordu (t.ad -> U.ad alan 0 -> t.kind). */
        const Dugum *nd = d->veri.erisim.nesne;
        if (nd && nd->tip == DUGUM_TANIMLAYICI) {
            LlvmIsim *vi = isim_bul(g, nd->veri.tanimlayici.metin,
                                    nd->veri.tanimlayici.uzunluk);
            if (vi && vi->ref_yapi_ir) yk = yapi_bul_ir(g, vi->ref_yapi_ir);
        }
        /* Son care (yapi tipi bilinmiyorsa): alan-adi arama (geriye uyum). */
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

    int alan_isz = ast_tip_isaretsiz_mi(alan_tip_d);  /* D-005 */

    /* Struct value ise: extractvalue */
    if (nesne.tip && nesne.tip[0] == '%') {
        int r = yeni_reg(g);
        fprintf(g->out, "  %%%d = extractvalue %s %%%d, %d\n",
                r, nesne.tip, nesne.reg, idx);
        IfadeSonuc s = { r, alan_ir, alan_isz };
        return s;
    }
    /* Ptr ise: GEP + load */
    int gep_r = yeni_reg(g);
    fprintf(g->out, "  %%%d = getelementptr %%", gep_r);
    yerel_ad_yaz(g->out, yk->ad, yk->ad_uz);
    fprintf(g->out, ", ptr %%%d, i32 0, i32 %d\n", nesne.reg, idx);
    int load_r = yeni_reg(g);
    fprintf(g->out, "  %%%d = load %s, ptr %%%d\n",
            load_r, alan_ir, gep_r);
    IfadeSonuc s = { load_r, alan_ir, alan_isz };
    return s;
}

/* C-track fix (init-test koku #3) + audit fix #3: `x.alan = v` ve
 * ic ice `a.b.c = v` LVALUE adresi. erisim_uret'in okuma mantigini
 * aynalar ama load YERINE alan ADRESINI (GEP) doner.
 *   - nesne tanimlayici + lokal struct (llvm_tip "%Ad"): alloca
 *     dogrudan struct adresi
 *   - nesne tanimlayici + referans param / ptr ("ptr"): alloca'dan
 *     ptr yukle, yapi tipi alan adina gore cozulur (erisim_uret ile
 *     ayni konservatif arama)
 *   - nesne ERISIM (audit gap #3): ozyineleme — ic alanin ADRESI
 *     taban olur, ic alan tipi ("%Ic") yapi kaydini verir. Onceden
 *     -1 donerdi -> a.b.c = v sessizce dusurulurdu.
 * Donus: alan adresi reg (>=0) + alan_ir_out; cozulemezse -1. */
static int erisim_lvalue(LlvmGen *g, const Dugum *d,
                         const char **alan_ir_out) {
    if (!d || d->tip != DUGUM_ERISIM) return -1;
    const Dugum *nesne_d = d->veri.erisim.nesne;
    if (!nesne_d) return -1;

    YapiKayit *yk = NULL;
    int taban_reg = -1;

    if (nesne_d->tip == DUGUM_TANIMLAYICI) {
        LlvmIsim *vi = isim_bul(g, nesne_d->veri.tanimlayici.metin,
                                nesne_d->veri.tanimlayici.uzunluk);
        if (!vi || !vi->llvm_tip) return -1;
        if (vi->llvm_tip[0] == '%') {
            yk = yapi_bul_ir(g, vi->llvm_tip);
            taban_reg = vi->reg_no;  /* alloca = struct'in kendisi */
        } else if (strcmp(vi->llvm_tip, "ptr") == 0) {
            /* D-029 fix: &Yapi lokal/param -> kayitli yapi tipi; yoksa
             * son care alan-adi arama (iki yapi ayni alan adi paylasinca
             * eski global arama YANLIS yapiya cozuyordu). */
            if (vi->ref_yapi_ir) yk = yapi_bul_ir(g, vi->ref_yapi_ir);
            for (YapiKayit *y = g->yapilar; y && !yk; y = y->sonraki) {
                if (yapi_alan_indeksi(y, d->veri.erisim.alan,
                                       d->veri.erisim.alan_uzunluk,
                                       NULL) >= 0) {
                    yk = y;
                }
            }
            if (!yk) return -1;
            taban_reg = yeni_reg(g);
            fprintf(g->out, "  %%%d = load ptr, ptr %%%d\n",
                    taban_reg, vi->reg_no);
        } else {
            return -1;
        }
    } else if (nesne_d->tip == DUGUM_ERISIM) {
        /* a.b.c: ic erisimin (a.b) ALAN ADRESINI al; tipi struct
         * ("%Ic") olmali ki alan GEP'i kurulabilsin. */
        const char *ic_ir = NULL;
        taban_reg = erisim_lvalue(g, nesne_d, &ic_ir);
        if (taban_reg < 0 || !ic_ir || ic_ir[0] != '%') return -1;
        yk = yapi_bul_ir(g, ic_ir);
    } else {
        return -1;
    }
    if (!yk || yk->ast->tip != DUGUM_YAPI) return -1;

    const Dugum *alan_tip_d = NULL;
    int idx = yapi_alan_indeksi(yk, d->veri.erisim.alan,
                                 d->veri.erisim.alan_uzunluk, &alan_tip_d);
    if (idx < 0) return -1;
    if (alan_ir_out) *alan_ir_out = ast_tip_to_ir(g, alan_tip_d);

    int gep_r = yeni_reg(g);
    fprintf(g->out, "  %%%d = getelementptr %%", gep_r);
    yerel_ad_yaz(g->out, yk->ad, yk->ad_uz);
    fprintf(g->out, ", ptr %%%d, i32 0, i32 %d\n", taban_reg, idx);
    return gep_r;
}

/* === C2.5: sonuç/seçimlik tagged-union yardımcıları === */

/* "{i8, T, H}" aggregate IR stringinden idx'inci alanın tipini cikar.
 * Derinlik farkında ({..}, [..], <..> içeren alanlar için). 1=başarı. */
static int agg_alan_ir(const char *agg, int idx, char *out, size_t out_n) {
    if (!agg || agg[0] != '{' || out_n == 0) return 0;
    const char *p = agg + 1;
    int field = 0, depth = 0;
    while (*p == ' ') p++;
    const char *bas = p;
    for (;;) {
        char c = *p;
        if (c == '\0') return 0;
        if (depth == 0 && (c == ',' || c == '}')) {
            if (field == idx) {
                const char *son = p;
                while (son > bas && son[-1] == ' ') son--;
                size_t k = (size_t)(son - bas);
                if (k >= out_n) k = out_n - 1;
                memcpy(out, bas, k);
                out[k] = '\0';
                return 1;
            }
            if (c == '}') return 0;
            field++;
            p++;
            while (*p == ' ') p++;
            bas = p;
            continue;
        }
        if (c == '{' || c == '[' || c == '<') depth++;
        else if (c == '}' || c == ']' || c == '>') depth--;
        p++;
    }
}

/* Yapıcı adı + beklenen sonuç/seçimlik tip düğümü → tag, payload alan indeksi,
 * payload tip düğümü. 1=tanınan yapıcı. payload_field<0 → payload yok (hiç). */
static int yapici_bilgi(const char *yad, int yuz, const Dugum *beklenen,
                        int *tag, int *payload_field,
                        const Dugum **payload_tip) {
    *payload_tip = NULL;
    if (!beklenen) return 0;
    if (beklenen->tip == DUGUM_TIP_SONUC) {
        if (yuz == 5 && memcmp(yad, "tamam", 5) == 0) {
            *tag = 0; *payload_field = 1;
            *payload_tip = beklenen->veri.tip_sonuc.deger_tip; return 1;
        }
        if (yuz == 4 && memcmp(yad, "hata", 4) == 0) {
            *tag = 1; *payload_field = 2;
            *payload_tip = beklenen->veri.tip_sonuc.hata_tip; return 1;
        }
        return 0;
    }
    if (beklenen->tip == DUGUM_TIP_SECIMLIK) {
        if (yuz == 6 && memcmp(yad, "de\xc4\x9f" "er", 6) == 0) {
            *tag = 0; *payload_field = 1;
            *payload_tip = beklenen->veri.tip_secimlik.ic_tip; return 1;
        }
        if (yuz == 4 && memcmp(yad, "hi\xc3\xa7", 4) == 0) {
            *tag = 1; *payload_field = -1; return 1;
        }
        return 0;
    }
    return 0;
}

/* tamam(x)/hata(e)/değer(x)/hiç → {i8 tag, payload...} inline inşa (by-value).
 * yapi_olustur_uret ile aynı desen: alloca + GEP+store + load. */
static IfadeSonuc yapici_uret(LlvmGen *g, const char *yad, int yuz,
                              const Dugum *arg, const Dugum *beklenen) {
    int tag = 0, pf = -1;
    const Dugum *payload_tip = NULL;
    if (!yapici_bilgi(yad, yuz, beklenen, &tag, &pf, &payload_tip)) {
        return hata(g, "sonuc/secimlik yapicisi cozulemedi");
    }
    const char *agg = ast_tip_to_ir(g, beklenen);
    int ar = yeni_reg(g);
    fprintf(g->out, "  %%%d = alloca %s\n", ar, agg);
    /* tag (alan 0) */
    int gt = yeni_reg(g);
    fprintf(g->out, "  %%%d = getelementptr %s, ptr %%%d, i32 0, i32 0\n",
            gt, agg, ar);
    fprintf(g->out, "  store i8 %d, ptr %%%d\n", tag, gt);
    /* payload (alan pf) */
    if (pf >= 0 && arg) {
        const char *pir = ast_tip_to_ir(g, payload_tip);
        if (!pir || strcmp(pir, "void") == 0) pir = "i8";
        const Dugum *eski = g->beklenen_tip;
        g->beklenen_tip = payload_tip;  /* iç içe yapıcı için */
        IfadeSonuc pv = ifade_uret(g, arg, pir);
        g->beklenen_tip = eski;
        int pr = pv.reg;
        if (!tip_kesirli_mi(pir) && !tip_kesirli_mi(pv.tip) &&
            tip_genisligi(pir) > 0 && tip_genisligi(pv.tip) > 0) {
            pr = int_donustur(g, pv.reg, pv.tip, pir);
        }
        int gp = yeni_reg(g);
        fprintf(g->out, "  %%%d = getelementptr %s, ptr %%%d, i32 0, i32 %d\n",
                gp, agg, ar, pf);
        fprintf(g->out, "  store %s %%%d, ptr %%%d\n", pir, pr, gp);
    }
    int lr = yeni_reg(g);
    fprintf(g->out, "  %%%d = load %s, ptr %%%d\n", lr, agg, ar);
    IfadeSonuc s = { lr, agg, 0 };
    return s;
}

/* Bir eşleş deseni sonuç/seçimlik yapıcı deseni mi? (tamam/hata/değer/hiç) */
static int yapici_desen_mi(const Dugum *desen) {
    const char *ad = NULL; int uz = 0;
    if (!desen) return 0;
    if (desen->tip == DUGUM_DESEN_YAPICI) {
        ad = desen->veri.desen_yapici.ad; uz = desen->veri.desen_yapici.ad_uzunluk;
    } else if (desen->tip == DUGUM_DESEN_TANIMLAYICI) {
        ad = desen->veri.desen_tanimlayici.ad;
        uz = desen->veri.desen_tanimlayici.ad_uzunluk;
    } else {
        return 0;
    }
    if (!ad) return 0;
    return (uz == 5 && memcmp(ad, "tamam", 5) == 0) ||
           (uz == 4 && memcmp(ad, "hata", 4) == 0) ||
           (uz == 6 && memcmp(ad, "de\xc4\x9f" "er", 6) == 0) ||
           (uz == 4 && memcmp(ad, "hi\xc3\xa7", 4) == 0);
}

/* === Capraz-modul jenerik (C): ortak instantiation makinesi ===
 *
 * Asagidaki iki yardimci, generic islev cagrisinin codegen'ini TANIMLAYICI
 * (modul-ici / global) ve YOL (capraz-modul `m::f(...)`) yollari arasinda
 * PAYLASIR. Onceki durum: yalniz TANIMLAYICI yolu specialize ediyordu; YOL
 * yolu (1726+) jenerigi plain @modul.f olarak emit edip clang'da
 * 'use of undefined value' veriyordu (capraz-modul routing GAP'i).
 *
 * Binding-koruma: gislev->veri.islev.ad ZATEN modul-nitelikli ("dizi.ekle")
 * — mangle_et ile "dizi.ekle$i64" uretir; specialize_emit "dizi." onekini
 * aktif_modul_onek olarak kurar (kardes ciplak-ad cagrilari owning-modulun
 * uyelerine cozulur). Use-site baglaminda YENIDEN cozum YOK. */

/* Generic islev'in i. parametresi icin codegen-beklenen IR tipi.
 * Generic-param iceren param (T, *T, &K<T>) -> NULL (T inference arg'in
 * dogal tipinden gelir); somut param -> IR tipi. Non-generic islevde tum
 * paramlar somut sayilir. (TANIMLAYICI yolundaki 2439-2478 ile ozdes
 * mantik; tek dogruluk kaynagi.) */
static const char *generic_param_beklenen(LlvmGen *g, const Dugum *islevd,
                                          int i) {
    if (i >= islevd->veri.islev.param_sayi) return NULL;
    const Dugum *pp = islevd->veri.islev.parametreler[i];
    const Dugum *ppt = pp ? pp->veri.parametre.tip : NULL;
    if (!ppt) return NULL;
    int generic_param_mi = 0;
    if (islevd->veri.islev.tip_param_sayi > 0) {
        for (int gi = 0; gi < islevd->veri.islev.tip_param_sayi; gi++) {
            const char *gp = islevd->veri.islev.tip_paramlar[gi];
            int gp_uz = (int)strlen(gp);
            const Dugum *bak = ppt;
            if (bak->tip == DUGUM_TIP_POINTER) {
                bak = bak->veri.tip_pointer.hedef_tip;
            }
            if (bak && bak->tip == DUGUM_TIP_REFERANS) {
                bak = bak->veri.tip_referans.hedef_tip;
            }
            if (bak && bak->tip == DUGUM_TIP_KULLANICI) {
                generic_param_mi = 1;  /* Liste<T> vb. */
                break;
            }
            if (bak && bak->tip == DUGUM_TIP_BASIT &&
                bak->veri.tip_basit.ad_uzunluk == gp_uz &&
                memcmp(bak->veri.tip_basit.ad, gp, (size_t)gp_uz) == 0) {
                generic_param_mi = 1;
                break;
            }
        }
    }
    if (generic_param_mi) return NULL;
    return ast_tip_to_ir(g, ppt);
}

/* Generic islev cagrisini specialize edip emit et. ALREADY-EVALUATED args
 * alir (her iki yol da kendi beklenen-baglamiyla evaluate eder). gislev =
 * generic islev AST (ad modul-nitelikli olabilir); donus = taban donus IR
 * tipi (ik/mik->donus_tip). Tip args param tiplerinden cikarsanir; mangled
 * ad bekleyenlere eklenir; donus tipi T-substitusyonuyla emit edilir. */
static IfadeSonuc generic_islev_cagri_uret(LlvmGen *g, const Dugum *d,
                                           const Dugum *gislev,
                                           IfadeSonuc *args, int n,
                                           const char *donus) {
    int tps = gislev->veri.islev.tip_param_sayi;
    const char **tip_args = (const char **)arena_ayir(g->arena,
        sizeof(const char *) * (size_t)tps);
    /* Her generic param icin: parametrelerde T'yi bulan ilk arg tipinden
     * çıkar */
    for (int ti = 0; ti < tps; ti++) {
        const char *tp = gislev->veri.islev.tip_paramlar[ti];
        int tp_uz = (int)strlen(tp);
        const char *inferred = NULL;
        for (int pi = 0; pi < gislev->veri.islev.param_sayi &&
                           pi < n && !inferred; pi++) {
            const Dugum *p = gislev->veri.islev.parametreler[pi];
            const Dugum *pt = p->veri.parametre.tip;
            if (!pt) continue;
            if (pt->tip == DUGUM_TIP_BASIT) {
                const char *pad = pt->veri.tip_basit.ad;
                int puz = pt->veri.tip_basit.ad_uzunluk;
                if (puz == tp_uz && memcmp(pad, tp, (size_t)tp_uz) == 0) {
                    inferred = args[pi].tip;
                }
                continue;
            }
            /* Liste<T> BUG-2 fix: compound param inference. Onceki durum:
             * yalniz duz `v: T` paramdan T cikarsanir, `veri: *T` /
             * `l: &Liste<T>` dusup $i32 default'a iner — bolge_al sizeof'u
             * 4'e duser = SESSIZ HEAP OVERFLOW (probe pF). */
            const Dugum *arg_d = d->veri.cagri.argumanlar[pi];
            /* (a) `veri: *T` param: arg pointee'sinden */
            if (pt->tip == DUGUM_TIP_POINTER &&
                pt->veri.tip_pointer.hedef_tip &&
                pt->veri.tip_pointer.hedef_tip->tip == DUGUM_TIP_BASIT) {
                const Dugum *h = pt->veri.tip_pointer.hedef_tip;
                if (h->veri.tip_basit.ad_uzunluk == tp_uz &&
                    memcmp(h->veri.tip_basit.ad, tp, (size_t)tp_uz) == 0 &&
                    arg_d && arg_d->tip == DUGUM_TANIMLAYICI) {
                    LlvmIsim *vi = isim_bul(g,
                        arg_d->veri.tanimlayici.metin,
                        arg_d->veri.tanimlayici.uzunluk);
                    if (vi && vi->pointee_llvm_tip) {
                        inferred = vi->pointee_llvm_tip;
                    }
                }
                continue;
            }
            /* (b) `l: &Kullanici<T>` param: arg `&x` -> x'in generic_arg_ir
             * yan-kanali (yapi IR'i type-erased, T'yi tasimaz). */
            if (pt->tip == DUGUM_TIP_REFERANS) {
                const Dugum *ic = pt->veri.tip_referans.hedef_tip;
                if (ic && ic->tip == DUGUM_TIP_KULLANICI &&
                    ic->veri.tip_kullanici.tip_arg_sayi == 1 &&
                    ic->veri.tip_kullanici.tip_arg[0] &&
                    ic->veri.tip_kullanici.tip_arg[0]->tip ==
                        DUGUM_TIP_BASIT) {
                    const Dugum *ta = ic->veri.tip_kullanici.tip_arg[0];
                    if (ta->veri.tip_basit.ad_uzunluk == tp_uz &&
                        memcmp(ta->veri.tip_basit.ad, tp,
                               (size_t)tp_uz) == 0) {
                        /* Arg `&x` (adres-al) ya da ciplak `l` (zaten
                         * &Liste<T> ref-param — ic generic-cagri zinciri). */
                        const Dugum *id_d = NULL;
                        if (arg_d && arg_d->tip == DUGUM_TEKLI &&
                            (arg_d->veri.tekli.op == OP_REF ||
                             arg_d->veri.tekli.op == OP_REF_DEGISKEN) &&
                            arg_d->veri.tekli.operand &&
                            arg_d->veri.tekli.operand->tip ==
                                DUGUM_TANIMLAYICI) {
                            id_d = arg_d->veri.tekli.operand;
                        } else if (arg_d &&
                                   arg_d->tip == DUGUM_TANIMLAYICI) {
                            id_d = arg_d;
                        }
                        if (id_d) {
                            LlvmIsim *vi = isim_bul(g,
                                id_d->veri.tanimlayici.metin,
                                id_d->veri.tanimlayici.uzunluk);
                            if (vi && vi->generic_arg_ir) {
                                inferred = vi->generic_arg_ir;
                            }
                        }
                    }
                }
                continue;
            }
        }
        tip_args[ti] = inferred ? inferred : "i32";
    }
    /* Mangled name */
    const char *mangled = mangle_et(g,
        gislev->veri.islev.ad, gislev->veri.islev.ad_uzunluk,
        tip_args, tps);

    /* Specialization bekleyenlere ekle (henuz emit edilmediyse). Dedup
     * anahtari = modul-nitelikli mangled ad ("dizi.ekle$i64") — ayni
     * specialization birden cok use-site'tan referanslansa da BIR kez. */
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

    /* Liste<T> BUG-1 fix: donus tipi T substitusyonu CALL emisyonundan ONCE
     * hesaplanmali (imza-uyumlu IR). Struct donus (%Nokta) dahil. */
    const char *donus_t = donus;
    if (gislev->veri.islev.donus_tipi &&
        gislev->veri.islev.donus_tipi->tip == DUGUM_TIP_BASIT) {
        const char *dad = gislev->veri.islev.donus_tipi->veri.tip_basit.ad;
        int duz = gislev->veri.islev.donus_tipi->veri.tip_basit.ad_uzunluk;
        for (int ti = 0; ti < tps; ti++) {
            const char *tp = gislev->veri.islev.tip_paramlar[ti];
            int tp_uz = (int)strlen(tp);
            if (duz == tp_uz && memcmp(dad, tp, (size_t)tp_uz) == 0) {
                donus_t = tip_args[ti];
                break;
            }
        }
    }
    if (strcmp(donus_t, "void") == 0) {
        /* donussuz generic (or. buyu<T>) — void call */
        fputs("  call void @", g->out);
        yerel_ad_yaz(g->out, mangled, (int)strlen(mangled));
        fputs("(", g->out);
        for (int i = 0; i < n; i++) {
            if (i > 0) fputs(", ", g->out);
            fprintf(g->out, "%s %%%d", args[i].tip, args[i].reg);
        }
        fputs(")\n", g->out);
        int r0 = yeni_reg(g);
        fprintf(g->out, "  %%%d = add i32 0, 0\n", r0);
        IfadeSonuc s0 = { r0, "i32", 0 };
        return s0;
    }
    int r = yeni_reg(g);
    fprintf(g->out, "  %%%d = call %s @", r, donus_t);
    yerel_ad_yaz(g->out, mangled, (int)strlen(mangled));
    fputs("(", g->out);
    for (int i = 0; i < n; i++) {
        if (i > 0) fputs(", ", g->out);
        fprintf(g->out, "%s %%%d", args[i].tip, args[i].reg);
    }
    fputs(")\n", g->out);
    IfadeSonuc s = { r, donus_t, 0 };
    return s;
}

static IfadeSonuc ifade_uret(LlvmGen *g, const Dugum *d,
                              const char *beklenen) {
    if (!d) {
        IfadeSonuc s = { yeni_reg(g), "i32", 0 };
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
            IfadeSonuc s = { r, tip, 0 };
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
            IfadeSonuc s = { r, tip, 0 };
            return s;
        }

        case DUGUM_KARAKTER: {
            int r = yeni_reg(g);
            fprintf(g->out, "  %%%d = add i32 0, %u\n",
                    r, d->veri.karakter.kod_noktasi);
            IfadeSonuc s = { r, "i32", 0 };
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
            IfadeSonuc s = { r, tip, 0 };
            return s;
        }

        case DUGUM_METIN:
            return metin_lit_uret(g, d);

        case DUGUM_TANIMLAYICI:
            return tanimlayici_yukle(g, d, beklenen);

        case DUGUM_YAPI_OLUSTUR:
            return yapi_olustur_uret(g, d);

        case DUGUM_ERISIM:
            return erisim_uret(g, d);

        case DUGUM_YOL: {
            /* C2.7/C3: Cesit::Varyant (bare) → disc/struct. sol TANIMLAYICI
             * (Renk) ya da YOL (m::Renk — çapraz-modül) olabilir. */
            const Dugum *sol = d->veri.yol.sol;
            YapiKayit *yk = cesit_kayit_yoldan(g, sol);
            if (yk) {
                int idx = cesit_varyant_indeksi(yk->ast,
                    d->veri.yol.sag_ad, d->veri.yol.sag_ad_uzunluk);
                if (idx < 0) idx = 0;
                /* payloadsuz çeşit → disc; payload çeşit → struct
                 * (bare varyant: yalnız disc, payload alanları undef). */
                return cesit_yapici_uret(g, yk->ast, idx, NULL, 0);
            }
            return hata(g, "yol ifadesi desteklenmiyor (cesit disi)");
        }

        case DUGUM_DIZI_OLUSTUR: {
            /* [D-044] Beklenen tip Dizi<T> ise -> HEAP KdlDizi (kdl_dizi_olustur
             * + dizi_ekle), stack [N x T] DEĞİL. değişken-annot dışı bağlamlar
             * (yapı alanı, çağrı argümanı, ver) için kök-fix: önceden bu yol yoktu
             * → struct-field `[]` 0-byte stack alloca olur, dizi_ekle SEGFAULT. */
            if (g->beklenen_tip && g->beklenen_tip->tip == DUGUM_TIP_DIZI) {
                const Dugum *elem_d = g->beklenen_tip->veri.tip_dizi.eleman_tip;
                const char *elem_ir = ast_tip_to_ir(g, elem_d);
                if (!elem_ir) elem_ir = "i32";
                int eb = 4;
                if (strcmp(elem_ir, "i8") == 0) eb = 1;
                else if (strcmp(elem_ir, "i16") == 0) eb = 2;
                else if (strcmp(elem_ir, "i64") == 0) eb = 8;
                else if (strcmp(elem_ir, "double") == 0) eb = 8;
                else if (strcmp(elem_ir, "ptr") == 0) eb = 8;
                int hn = d->veri.dizi_olustur.sayi;
                int kdl_reg = yeni_reg(g);
                fprintf(g->out,
                    "  %%%d = call ptr @kdl_dizi_olustur(i32 %d)\n", kdl_reg, eb);
                const Dugum *eski_bt = g->beklenen_tip;
                g->beklenen_tip = elem_d;  /* iç içe dizi/yapıcı elemanları için */
                for (int i = 0; i < hn; i++) {
                    IfadeSonuc v = ifade_uret(g, d->veri.dizi_olustur.elemanlar[i], elem_ir);
                    int vr = int_donustur(g, v.reg, v.tip, elem_ir);
                    const char *fn = "kdl_dizi_ekle_tam";
                    if (strcmp(elem_ir, "i64") == 0) fn = "kdl_dizi_ekle_tam64";
                    else if (strcmp(elem_ir, "ptr") == 0) fn = "kdl_dizi_ekle_ptr";
                    fprintf(g->out, "  call void @%s(ptr %%%d, %s %%%d)\n",
                            fn, kdl_reg, elem_ir, vr);
                }
                g->beklenen_tip = eski_bt;
                IfadeSonuc s = { kdl_reg, "ptr", 0 };
                return s;
            }

            /* [e1, e2, ...] -> alloca [N x T] + store + return ptr (stack)
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
                IfadeSonuc s = { alloca_r, "ptr", 0 };
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
            IfadeSonuc s = { alloca_r, "ptr", 0 };
            return s;
        }

        case DUGUM_INDEKS: {
            /* Adim 3 (B v2): heap dizi tanimlayicisi mi? Eger oyle ise
             * kdl_dizi_al route et. Aksi halde mevcut GEP yolu (stack). */
            const char *pointee_elem = NULL;
            int stack_uzunluk = 0;   /* D-069 Kat.2: sabit stack dizi N (>0 → sınır-kontrol) */
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
                    IfadeSonuc s = { rr, et, 0 };
                    return s;
                }
                /* v1 bölge-container: *T tabani — eleman tipi POINTEE'den
                 * (beklenen/i32 fallback yanlis genislik uretirdi). */
                if (vi && vi->pointee_llvm_tip) {
                    pointee_elem = vi->pointee_llvm_tip;
                }
                /* D-069 Kat.2: sabit stack dizi [N x T] → sınır-kontrol için N */
                if (vi) stack_uzunluk = vi->dizi_uzunluk;
            }
            /* arr[i] -> GEP ptr (T*) + load (stack) */
            IfadeSonuc nesne = ifade_uret(g, d->veri.indeks.nesne, NULL);
            IfadeSonuc idx = ifade_uret(g, d->veri.indeks.indeks, "i64");
            int idx_r = int_donustur(g, idx.reg, idx.tip, "i64");
            /* D-069 Kat.2: sabit stack dizi sınır-kontrolü (GEP'ten ÖNCE).
             * `icmp uge` unsigned → negatif (dev unsigned) + i>=N tek seferde.
             * OOB → kdl_panik (temiz durma), aksi GEP+load (bb<ok>).
             * güvensiz blok içinde ATLANIR (opt-out — programcı sorumluluğunda). */
            if (stack_uzunluk > 0 && g->guvensiz_derinlik == 0) {
                int c_r = yeni_reg(g);
                fprintf(g->out, "  %%%d = icmp uge i64 %%%d, %d\n",
                        c_r, idx_r, stack_uzunluk);
                int L_oob = yeni_label(g);
                int L_ok = yeni_label(g);
                fprintf(g->out, "  br i1 %%%d, label %%bb%d, label %%bb%d\n",
                        c_r, L_oob, L_ok);
                fprintf(g->out, "bb%d:\n", L_oob);
                fprintf(g->out,
                    "  call void @kdl_panik(ptr @.str.dizi_sinir_panik)\n");
                fprintf(g->out, "  unreachable\n");
                fprintf(g->out, "bb%d:\n", L_ok);
            }
            const char *elem_ir = pointee_elem ? pointee_elem
                                  : (beklenen ? beklenen : "i32");
            int gep_r = yeni_reg(g);
            /* opaque pointer arithmetic: getelementptr T, ptr, idx */
            fprintf(g->out,
                "  %%%d = getelementptr %s, ptr %%%d, i64 %%%d\n",
                gep_r, elem_ir, nesne.reg, idx_r);
            int load_r = yeni_reg(g);
            fprintf(g->out, "  %%%d = load %s, ptr %%%d\n",
                    load_r, elem_ir, gep_r);
            IfadeSonuc s = { load_r, elem_ir, 0 };
            return s;
        }

        case DUGUM_IKILI: {
            /* Kampanya seed (b) / D-002 [YUKSEK]: ve/veya KISA-DEVRE.
             * Onceki durum: 'and/or i32' — HER IKI taraf da kosulsuz
             * degerlendiriliyordu; yan etkili sag taraf (cagri) icin
             * SESSIZ-YANLIS semantik. Standart kisa-devre: sol
             * yeterliyse sag hic degerlendirilmez. phi yerine mevcut
             * codegen idiomu olan bellek-slot deseni. Sonuc tipi i1
             * (icmp/degil ile ayni yuzey). */
            if (d->veri.ikili.op == OP_VE || d->veri.ikili.op == OP_VEYA) {
                int ve_mi = (d->veri.ikili.op == OP_VE);
                int slot = yeni_reg(g);
                fprintf(g->out, "  %%%d = alloca i1\n", slot);
                int sol_i1 = kosul_i1(g, d->veri.ikili.sol);
                int L_sag = yeni_label(g);
                int L_kisa = yeni_label(g);
                int L_son = yeni_label(g);
                if (ve_mi) {
                    /* sol dogruysa sag belirler; yanlissa sonuc yanlis */
                    fprintf(g->out,
                        "  br i1 %%%d, label %%bb%d, label %%bb%d\n",
                        sol_i1, L_sag, L_kisa);
                } else {
                    /* sol dogruysa sonuc dogru; yanlissa sag belirler */
                    fprintf(g->out,
                        "  br i1 %%%d, label %%bb%d, label %%bb%d\n",
                        sol_i1, L_kisa, L_sag);
                }
                fprintf(g->out, "bb%d:\n", L_sag);
                int sag_i1 = kosul_i1(g, d->veri.ikili.sag);
                fprintf(g->out, "  store i1 %%%d, ptr %%%d\n",
                        sag_i1, slot);
                fprintf(g->out, "  br label %%bb%d\n", L_son);
                fprintf(g->out, "bb%d:\n", L_kisa);
                fprintf(g->out, "  store i1 %s, ptr %%%d\n",
                        ve_mi ? "false" : "true", slot);
                fprintf(g->out, "  br label %%bb%d\n", L_son);
                fprintf(g->out, "bb%d:\n", L_son);
                int r = yeni_reg(g);
                fprintf(g->out, "  %%%d = load i1, ptr %%%d\n", r, slot);
                IfadeSonuc s = { r, "i1", 0 };
                return s;
            }
            const char *cmp_i = icmp_pred(d->veri.ikili.op);
            const char *op_beklenen = cmp_i ? NULL : beklenen;
            IfadeSonuc sol = ifade_uret(g, d->veri.ikili.sol, op_beklenen);
            IfadeSonuc sag = ifade_uret(g, d->veri.ikili.sag, sol.tip);
            /* D-005: operandlardan biri isaretsizse (dtamN) tum islem
             * isaretsiz semantikle yapilir (udiv/urem/lshr/u-pred). */
            int isz = sol.isaretsiz || sag.isaretsiz;
            /* Tipler ayniysa donusum yok; degilse int donusumu (float ile karisma yok) */
            int sag_r = sag.reg;
            if (!tip_kesirli_mi(sol.tip)) {
                sag_r = int_donustur_im(g, sag.reg, sag.tip, sol.tip,
                                        sag.isaretsiz);
            }
            int kesirli = tip_kesirli_mi(sol.tip);
            if (cmp_i) {
                if (isz) cmp_i = icmp_pred_u(d->veri.ikili.op);
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
                IfadeSonuc s = { r_cmp, "i1", 0 };
                return s;
            }
            const char *op = NULL;
            switch (d->veri.ikili.op) {
                case OP_ARTI:  op = kesirli ? "fadd" : "add"; break;
                case OP_EKSI:  op = kesirli ? "fsub" : "sub"; break;
                case OP_CARPI: op = kesirli ? "fmul" : "mul"; break;
                case OP_BOLU:
                    op = kesirli ? "fdiv" : (isz ? "udiv" : "sdiv"); break;
                case OP_MOD:
                    op = kesirli ? "frem" : (isz ? "urem" : "srem"); break;
                /* OP_VE/OP_VEYA buraya ULASMAZ — yukarida kisa-devre
                 * dali (D-002) tum ve/veya'yi intercept eder. */
                /* Bit operatorleri — page table / kripto codegen */
                case OP_BIT_VE:      op = "and"; break;
                case OP_BIT_VEYA:    op = "or"; break;
                case OP_BIT_OZVEYA:  op = "xor"; break;
                case OP_SOLA_KAYDIR: op = "shl"; break;
                case OP_SAGA_KAYDIR:
                    /* D-005: isaretsizde mantiksal kaydirma */
                    op = isz ? "lshr" : "ashr"; break;
                default:
                    fputs("  ; ikili op desteklenmiyor\n", g->out);
                    return sol;
            }
            int r = yeni_reg(g);
            fprintf(g->out, "  %%%d = %s %s %%%d, %%%d\n",
                    r, op, sol.tip, sol.reg, sag_r);
            IfadeSonuc s = { r, sol.tip, isz };
            return s;
        }

        case DUGUM_TEKLI: {
            /* C-track fix (&Struct segfault, D9 bulgusu): OP_REF ve
             * OP_REF_DEGISKEN operandin ADRESINI uretmeli. Onceki durum:
             * asagida "tekli op desteklenmiyor" yorumuna dusup operandin
             * DEGERI donuyordu -> `f(&v)` cagrisinda struct BY-VALUE
             * gecer, callee imzasi `ptr` bekler (opt -verify bunu
             * yakalamaz: imza-uyumsuz direkt cagri gecerli-ama-UB IR),
             * callee ilk 8 byte'i pointer sanip deref eder -> runtime
             * SEGFAULT. Tanimlayici: alloca adresi dogrudan donulur.
             * Diger operandlar (rvalue): degeri temp alloca'ya
             * materyalize edilir — read-only referans semantigi. */
            if (d->veri.tekli.op == OP_REF ||
                d->veri.tekli.op == OP_REF_DEGISKEN) {
                const Dugum *op_d = d->veri.tekli.operand;
                if (op_d && op_d->tip == DUGUM_TANIMLAYICI) {
                    LlvmIsim *vi = isim_bul(g,
                        op_d->veri.tanimlayici.metin,
                        op_d->veri.tanimlayici.uzunluk);
                    if (vi) {
                        IfadeSonuc s = { vi->reg_no, "ptr", 0 };
                        return s;
                    }
                }
                IfadeSonuc v = ifade_uret(g, op_d, NULL);
                int t = yeni_reg(g);
                fprintf(g->out, "  %%%d = alloca %s\n", t, v.tip);
                fprintf(g->out, "  store %s %%%d, ptr %%%d\n",
                        v.tip, v.reg, t);
                IfadeSonuc s = { t, "ptr", 0 };
                return s;
            }
            /* Audit fix #1: OP_DEREFERANS — *p ham pointer YUKU.
             * Onceki durum: "tekli op desteklenmiyor" + ptr DEGERI
             * donerdi -> ret baglaminda gecersiz IR, aritmetikte sessiz
             * yanlis deger. Yuklenecek tip: beklenen (ver/atama/cagri
             * baglamindan forward edilir), yoksa i32 varsayilan. */
            if (d->veri.tekli.op == OP_DEREFERANS) {
                IfadeSonuc p = ifade_uret(g, d->veri.tekli.operand, NULL);
                const char *yuk_tip =
                    (beklenen && strcmp(beklenen, "ptr") != 0)
                        ? beklenen : "i32";
                int r = yeni_reg(g);
                fprintf(g->out, "  %%%d = load %s, ptr %%%d\n",
                        r, yuk_tip, p.reg);
                IfadeSonuc s = { r, yuk_tip, 0 };
                return s;
            }
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
                IfadeSonuc s = { r, op_s.tip, 0 };
                return s;
            }
            if (d->veri.tekli.op == OP_DEGIL) {
                /* op_s i1 ise xor 1; aksi halde icmp eq 0 */
                int r;
                if (strcmp(op_s.tip, "i1") == 0) {
                    r = yeni_reg(g);
                    fprintf(g->out, "  %%%d = xor i1 %%%d, true\n",
                            r, op_s.reg);
                    IfadeSonuc s = { r, "i1", 0 };
                    return s;
                }
                r = yeni_reg(g);
                fprintf(g->out, "  %%%d = icmp eq %s %%%d, 0\n",
                        r, op_s.tip, op_s.reg);
                IfadeSonuc s = { r, "i1", 0 };
                return s;
            }
            if (d->veri.tekli.op == OP_BIT_DEGIL) {
                /* ~x = xor TYPE x, -1 (tum bitleri ters cevir) */
                int r = yeni_reg(g);
                fprintf(g->out, "  %%%d = xor %s %%%d, -1\n",
                        r, op_s.tip, op_s.reg);
                IfadeSonuc s = { r, op_s.tip, 0 };
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
                yerel_ad_yaz(g->out, m_ad, m_ad_uz);
                fputs("(", g->out);
                for (int i = 0; i < n + 1; i++) {
                    if (i > 0) fputs(", ", g->out);
                    fprintf(g->out, "%s %%%d", args[i].tip, args[i].reg);
                }
                fputs(")\n", g->out);
                IfadeSonuc s = { r, donus, 0 };
                return s;
            }
            /* C3: çeşit varyant YAPICISI X::V(args) — modül-fonksiyon YOL
             * yolundan ÖNCE. sol bir çeşit + sag varyant ise tagged-union
             * inşası (cesit_yapici_uret), değilse normal modül çağrısı. */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_YOL) {
                const Dugum *yh = d->veri.cagri.hedef;
                /* sol TANIMLAYICI (Cesit) ya da YOL (m::Cesit — çapraz-modül) */
                YapiKayit *yk = cesit_kayit_yoldan(g, yh->veri.yol.sol);
                if (yk) {
                    int vi = cesit_varyant_indeksi(yk->ast,
                        yh->veri.yol.sag_ad, yh->veri.yol.sag_ad_uzunluk);
                    if (vi < 0) vi = 0;
                    return cesit_yapici_uret(g, yk->ast, vi, d,
                                             d->veri.cagri.sayi);
                }
            }
            /* Kampanya seed (a) / D-001: mat::kare(x) — YOL hedefli cagri.
             * Yol zinciri noktali ada knit edilir (@mat.kare) ve kayitli
             * modul uyesine direkt cagri emit edilir. Onceki durum:
             * '; HATA: cagri hedefi tanimlayici degil' + SESSIZ 0. */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_YOL) {
                const Dugum *yol_h = d->veri.cagri.hedef;
                IslevKayit *mik = NULL;
                /* Tek-gecis ad cozumu: binding'deki TAM onek + sag_ad ile
                 * mangle — goreli yol (m icinden ic::f -> @m.ic.f) dogru
                 * cozulur; eski string-knit yalniz yazildigi segmentleri
                 * biliyordu. Binding yoksa eski yola dus. */
                if (yol_h->cozum_kategori == COZUM_MODUL_UYESI &&
                    yol_h->cozum_modul_onek) {
                    int muz = 0;
                    const char *mangled = modul_mangle(g,
                        yol_h->cozum_modul_onek, yol_h->cozum_modul_onek_uz,
                        yol_h->veri.yol.sag_ad, yol_h->veri.yol.sag_ad_uzunluk,
                        &muz);
                    if (mangled) mik = islev_bul(g, mangled, muz);
                }
                if (!mik) {
                    char tam_ad[256];
                    int tuz = yol_noktali_ad(yol_h,
                                             tam_ad, (int)sizeof(tam_ad));
                    mik = (tuz > 0) ? islev_bul(g, tam_ad, tuz) : NULL;
                }
                if (mik) {
                    int n = d->veri.cagri.sayi;
                    /* Capraz-modul JENERIK routing (C): mik generic ise
                     * arg'lari generic-uyumlu beklenen ile evaluate edip
                     * ortak instantiation makinesine yonlendir. Onceki
                     * durum: YOL yolu jenerigi plain @modul.f olarak emit
                     * edip clang'da 'undefined value' veriyordu. */
                    int mik_generic = (mik->generic_mi && mik->ast);
                    IfadeSonuc *args = NULL;
                    if (n > 0) {
                        args = (IfadeSonuc *)arena_ayir(g->arena,
                            sizeof(IfadeSonuc) * (size_t)n);
                        for (int i = 0; i < n; i++) {
                            const char *bekle = mik_generic
                                ? generic_param_beklenen(g, mik->ast, i)
                                : NULL;
                            args[i] = ifade_uret(g,
                                d->veri.cagri.argumanlar[i], bekle);
                            /* somut param int genislik uyumu */
                            if (bekle && strcmp(args[i].tip, bekle) != 0 &&
                                (strcmp(bekle, "i64") == 0 ||
                                 strcmp(bekle, "i32") == 0 ||
                                 strcmp(bekle, "i16") == 0 ||
                                 strcmp(bekle, "i8") == 0) &&
                                (strcmp(args[i].tip, "i64") == 0 ||
                                 strcmp(args[i].tip, "i32") == 0 ||
                                 strcmp(args[i].tip, "i16") == 0 ||
                                 strcmp(args[i].tip, "i8") == 0)) {
                                int nr = int_donustur(g, args[i].reg,
                                                      args[i].tip, bekle);
                                args[i].reg = nr;
                                args[i].tip = bekle;
                            }
                        }
                    }
                    if (mik_generic) {
                        const char *gd = mik->donus_tip
                            ? mik->donus_tip : "i32";
                        return generic_islev_cagri_uret(g, d, mik->ast,
                                                        args, n, gd);
                    }
                    const char *donus = mik->donus_tip
                        ? mik->donus_tip : "i32";
                    int vd = (strcmp(donus, "void") == 0);
                    int r = -1;
                    if (vd) {
                        fputs("  call void @", g->out);
                    } else {
                        r = yeni_reg(g);
                        fprintf(g->out, "  %%%d = call %s @", r, donus);
                    }
                    yerel_ad_yaz(g->out, mik->ad, mik->ad_uz);
                    fputs("(", g->out);
                    for (int i = 0; i < n; i++) {
                        if (i > 0) fputs(", ", g->out);
                        fprintf(g->out, "%s %%%d",
                                args[i].tip, args[i].reg);
                    }
                    fputs(")\n", g->out);
                    if (vd) {
                        r = yeni_reg(g);
                        fprintf(g->out, "  %%%d = add i32 0, 0\n", r);
                        IfadeSonuc s = { r, "i32", 0 };
                        return s;
                    }
                    IfadeSonuc s = { r, donus, 0 };
                    return s;
                }
                /* cozulemedi — mevcut hata yoluna dus (gorunur yorum) */
            }
            if (!d->veri.cagri.hedef ||
                d->veri.cagri.hedef->tip != DUGUM_TANIMLAYICI) {
                return hata(g, "cagri hedefi tanimlayici degil");
            }
            /* C2.5: sonuç/seçimlik yapıcıları — tamam(x)/hata(e)/değer(x).
             * Beklenen yapısal tip sonuç/seçimlik ise generic call yerine
             * tagged-union inşası → @tamam/@hata tanımsız sembolleri kalkar. */
            {
                const char *ca = d->veri.cagri.hedef->veri.tanimlayici.metin;
                int cuz = d->veri.cagri.hedef->veri.tanimlayici.uzunluk;
                int ctor = (cuz == 5 && memcmp(ca, "tamam", 5) == 0) ||
                           (cuz == 4 && memcmp(ca, "hata", 4) == 0) ||
                           (cuz == 6 && memcmp(ca, "de\xc4\x9f" "er", 6) == 0);
                if (ctor && g->beklenen_tip &&
                    (g->beklenen_tip->tip == DUGUM_TIP_SONUC ||
                     g->beklenen_tip->tip == DUGUM_TIP_SECIMLIK) &&
                    d->veri.cagri.sayi >= 1) {
                    return yapici_uret(g, ca, cuz,
                                       d->veri.cagri.argumanlar[0],
                                       g->beklenen_tip);
                }
            }
            /* Sabitsüre Spec V1 intrinsics: sabitsure_olustur (16 byte) ve
             * ifsa (5 byte). Argümanı pass-through, sonra speculation
             * barrier (x86 lfence) emit ederiz. Zero-overhead — IR seviyesi
             * tipleri T (iç tip) ile aynı. */
            {
                const char *_ca = d->veri.cagri.hedef->veri.tanimlayici.metin;
                int _uz = d->veri.cagri.hedef->veri.tanimlayici.uzunluk;
                int _is_olustur = (_uz == 18 &&
                    memcmp(_ca, "sabits\xc3\xbc" "re_olustur", 18) == 0);
                int _is_ifsa = (_uz == 5 &&
                    memcmp(_ca, "if\xc5\x9f" "a", 5) == 0);
                if ((_is_olustur || _is_ifsa) && d->veri.cagri.sayi == 1) {
                    IfadeSonuc inner = ifade_uret(g,
                        d->veri.cagri.argumanlar[0], beklenen);
                    /* x86 lfence speculation barrier — Spectre v1 mitigation.
                     * Modern LLVM intrinsic; declare gerekmez (built-in). */
                    fputs("  call void @llvm.x86.sse2.lfence()\n", g->out);
                    return inner;
                }
                /* Linear Types V1: tekkez_olustur(e) -> tekkez<T>.
                 * IR'da zero-overhead sarmalayici: tekkez<T> = T
                 * (ast_tip_to_ir ic tipi acar) -> arg pass-through.
                 * Audit gap #4: onceden generic call yoluna dusup
                 * TANIMSIZ @tekkez_olustur sembolu uretiyordu (link
                 * hatasi — lineer kod hic calistirilamiyordu). */
                if (_uz == 14 &&
                    memcmp(_ca, "tekkez_olustur", 14) == 0 &&
                    d->veri.cagri.sayi == 1) {
                    return ifade_uret(g, d->veri.cagri.argumanlar[0],
                                      beklenen);
                }
                /* v1 bölge-container: bölge_al(böl: yetki<R>, n) -> *T.
                 * v1 malloc-VEKALETEN (gerçek arena V2, AYNI imza).
                 * T: g->beklenen_tip kanali (degisken annot *T) —
                 * tip kontrol BL001 ile annot'u zaten garanti eder.
                 * sizeof(T): GEP-null + ptrtoint idiomu (datalayout-
                 * dogru; struct padding/align dahil). Yetki argumani
                 * compile-time ispat — mmio deseni gibi IR'a GECMEZ. */
                if (_uz == 9 &&
                    memcmp(_ca, "b\xc3\xb6lge_al", 9) == 0 &&
                    d->veri.cagri.sayi == 2) {
                    const char *pointee_ir = "i32";
                    if (g->beklenen_tip &&
                        g->beklenen_tip->tip == DUGUM_TIP_POINTER) {
                        const char *pp = ast_tip_to_ir(g,
                            g->beklenen_tip->veri.tip_pointer.hedef_tip);
                        if (pp) pointee_ir = pp;
                    }
                    IfadeSonuc n = ifade_uret(g,
                        d->veri.cagri.argumanlar[1], "i64");
                    int n64 = int_donustur(g, n.reg, n.tip, "i64");
                    int szp = yeni_reg(g);
                    fprintf(g->out,
                        "  %%%d = getelementptr %s, ptr null, i64 1\n",
                        szp, pointee_ir);
                    int szi = yeni_reg(g);
                    fprintf(g->out,
                        "  %%%d = ptrtoint ptr %%%d to i64\n", szi, szp);
                    int tot = yeni_reg(g);
                    fprintf(g->out, "  %%%d = mul i64 %%%d, %%%d\n",
                            tot, n64, szi);
                    int buf = yeni_reg(g);
                    fprintf(g->out,
                        "  %%%d = call ptr @malloc(i64 %%%d)\n", buf, tot);
                    IfadeSonuc s = { buf, "ptr", 0 };
                    return s;
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
                    /* C-track ABI fix: %kdl_yetki (16B) Win64 C ABI'de
                     * sret pointer ile DONER — clang'in C tarafi icin
                     * urettigi imza `void(ptr sret, i16, i16)`. Onceki
                     * first-class-donus formu backend demotion'ina bel
                     * bagliyordu; sret'i acikca emit ediyoruz. */
                    int sret = yeni_reg(g);
                    fprintf(g->out, "  %%%d = alloca %%kdl_yetki\n", sret);
                    fprintf(g->out,
                        "  call void @kdl_yetki_olustur("
                        "ptr sret(%%kdl_yetki) align 8 %%%d, "
                        "i16 %%%d, i16 %%%d)\n", sret, r_kt, r_izin);
                    int r = yeni_reg(g);
                    fprintf(g->out,
                        "  %%%d = load %%kdl_yetki, ptr %%%d\n", r, sret);
                    IfadeSonuc s = { r, "%kdl_yetki", 0 };
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
                    /* C-track ABI fix (init-test segfault koku #2):
                     * Win64 C ABI 16B struct ARGUMANI pointer ile gecer
                     * (`void(ptr sret, ptr, i16)`). Onceki first-class
                     * `%kdl_yetki` arg formu C tarafinin bekledigi
                     * pointer'la uyusmuyordu -> kdl_yetki_delege
                     * prologunda copte deref, runtime SEGFAULT (opt
                     * -verify yakalamaz: imza-uyumsuz cagri gecerli IR).
                     * Deger temp alloca'ya yazilir, adresi gecirilir. */
                    int y_slot = yeni_reg(g);
                    fprintf(g->out, "  %%%d = alloca %%kdl_yetki\n", y_slot);
                    fprintf(g->out,
                        "  store %%kdl_yetki %%%d, ptr %%%d\n",
                        arg_y.reg, y_slot);
                    int sret = yeni_reg(g);
                    fprintf(g->out, "  %%%d = alloca %%kdl_yetki\n", sret);
                    fprintf(g->out,
                        "  call void @kdl_yetki_delege("
                        "ptr sret(%%kdl_yetki) align 8 %%%d, "
                        "ptr %%%d, i16 %%%d)\n",
                        sret, y_slot, r_izin);
                    int r = yeni_reg(g);
                    fprintf(g->out,
                        "  %%%d = load %%kdl_yetki, ptr %%%d\n", r, sret);
                    IfadeSonuc s = { r, "%kdl_yetki", 0 };
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
                    IfadeSonuc s = { 0, "void", 0 };
                    return s;
                }
                /* MMIO Foundation: mmio_oku32(y, adres) -> i32.
                 * y compile-time yetki ispati (runtime'a gecmez — WCET).
                 * adres i64'e genisletilir; kdl_mmio_oku32 cagrilir
                 * (host: mock tampon, bare-metal: volatile load — pl011 idiomu). */
                if (_uz == 10 && memcmp(_ca, "mmio_oku32", 10) == 0 &&
                    d->veri.cagri.sayi == 2) {
                    IfadeSonuc adr = ifade_uret(g,
                        d->veri.cagri.argumanlar[1], "i64");
                    int adr64 = int_donustur(g, adr.reg, adr.tip, "i64");
                    int r = yeni_reg(g);
                    fprintf(g->out,
                        "  %%%d = call i32 @kdl_mmio_oku32(i64 %%%d)\n",
                        r, adr64);
                    IfadeSonuc s = { r, "i32", 0 };
                    return s;
                }
                /* mmio_yaz32(y, adres, deger) -> void (volatile store) */
                if (_uz == 10 && memcmp(_ca, "mmio_yaz32", 10) == 0 &&
                    d->veri.cagri.sayi == 3) {
                    IfadeSonuc adr = ifade_uret(g,
                        d->veri.cagri.argumanlar[1], "i64");
                    int adr64 = int_donustur(g, adr.reg, adr.tip, "i64");
                    IfadeSonuc val = ifade_uret(g,
                        d->veri.cagri.argumanlar[2], "i32");
                    int val32 = int_donustur(g, val.reg, val.tip, "i32");
                    fprintf(g->out,
                        "  call void @kdl_mmio_yaz32(i64 %%%d, i32 %%%d)\n",
                        adr64, val32);
                    IfadeSonuc s = { 0, "void", 0 };
                    return s;
                }
                /* mmio_oku16(y, adres) -> i16 (volatile 16-bit load — le16
                 * ring alanlari: avail/used idx, descriptor flags/next). */
                if (_uz == 10 && memcmp(_ca, "mmio_oku16", 10) == 0 &&
                    d->veri.cagri.sayi == 2) {
                    IfadeSonuc adr = ifade_uret(g,
                        d->veri.cagri.argumanlar[1], "i64");
                    int adr64 = int_donustur(g, adr.reg, adr.tip, "i64");
                    int r = yeni_reg(g);
                    fprintf(g->out,
                        "  %%%d = call i16 @kdl_mmio_oku16(i64 %%%d)\n",
                        r, adr64);
                    IfadeSonuc s = { r, "i16", 0 };
                    return s;
                }
                /* mmio_yaz16(y, adres, deger) -> void (volatile 16-bit store) */
                if (_uz == 10 && memcmp(_ca, "mmio_yaz16", 10) == 0 &&
                    d->veri.cagri.sayi == 3) {
                    IfadeSonuc adr = ifade_uret(g,
                        d->veri.cagri.argumanlar[1], "i64");
                    int adr64 = int_donustur(g, adr.reg, adr.tip, "i64");
                    IfadeSonuc val = ifade_uret(g,
                        d->veri.cagri.argumanlar[2], "i16");
                    int val16 = int_donustur(g, val.reg, val.tip, "i16");
                    fprintf(g->out,
                        "  call void @kdl_mmio_yaz16(i64 %%%d, i16 %%%d)\n",
                        adr64, val16);
                    IfadeSonuc s = { 0, "void", 0 };
                    return s;
                }
                /* mmio_oku64(y, adres) -> i64 (volatile 64-bit load — le64
                 * descriptor addr alani). */
                if (_uz == 10 && memcmp(_ca, "mmio_oku64", 10) == 0 &&
                    d->veri.cagri.sayi == 2) {
                    IfadeSonuc adr = ifade_uret(g,
                        d->veri.cagri.argumanlar[1], "i64");
                    int adr64 = int_donustur(g, adr.reg, adr.tip, "i64");
                    int r = yeni_reg(g);
                    fprintf(g->out,
                        "  %%%d = call i64 @kdl_mmio_oku64(i64 %%%d)\n",
                        r, adr64);
                    IfadeSonuc s = { r, "i64", 0 };
                    return s;
                }
                /* mmio_yaz64(y, adres, deger) -> void (volatile 64-bit store) */
                if (_uz == 10 && memcmp(_ca, "mmio_yaz64", 10) == 0 &&
                    d->veri.cagri.sayi == 3) {
                    IfadeSonuc adr = ifade_uret(g,
                        d->veri.cagri.argumanlar[1], "i64");
                    int adr64 = int_donustur(g, adr.reg, adr.tip, "i64");
                    IfadeSonuc val = ifade_uret(g,
                        d->veri.cagri.argumanlar[2], "i64");
                    int val64 = int_donustur(g, val.reg, val.tip, "i64");
                    fprintf(g->out,
                        "  call void @kdl_mmio_yaz64(i64 %%%d, i64 %%%d)\n",
                        adr64, val64);
                    IfadeSonuc s = { 0, "void", 0 };
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
                        IfadeSonuc res = { r2, beklenen, 0 };
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
                    IfadeSonuc res = { r2, "<4 x i32>", 0 };
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
                    IfadeSonuc res = { r, kalici ? kalici : "i32", 0 };
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
                    IfadeSonuc res = { r, kalici ? kalici : "i32", 0 };
                    return res;
                }
            }
            int n = d->veri.cagri.sayi;
            /* Madde B: dizi_ekle / dizi_al ic context — eleman tipi
             * arg[0]'in (dizi degiskeni) eleman_llvm_tip'inden alinir.
             * Beklenen arg[1] icin set edilir. */
            const char *dizi_eleman_beklenen = NULL;
            /* Eleman-değeri argümanının indeksi: dizi_ekle(d,e)→1, dizi_al(d,i)
             * →1 (indeks), dizi_yaz(d,i,e)→2 (değer). dizi_eleman_beklenen bu
             * argümana forward edilir (literal eleman tip çıkarsaması). */
            int dizi_deger_arg = 1;
            {
                const char *adi = d->veri.cagri.hedef
                    ? d->veri.cagri.hedef->veri.tanimlayici.metin : NULL;
                int adi_uz = d->veri.cagri.hedef
                    ? d->veri.cagri.hedef->veri.tanimlayici.uzunluk : 0;
                int dizi_built_in =
                    (adi_uz == 9 && memcmp(adi, "dizi_ekle", 9) == 0) ||
                    (adi_uz == 7 && memcmp(adi, "dizi_al", 7) == 0) ||
                    (adi_uz == 8 && memcmp(adi, "dizi_yaz", 8) == 0);
                if (adi_uz == 8 && memcmp(adi, "dizi_yaz", 8) == 0) {
                    dizi_deger_arg = 2;  /* dizi_yaz: değer = arg[2] */
                }
                if (dizi_built_in && n >= 1) {
                    const Dugum *arg0 = d->veri.cagri.argumanlar[0];
                    if (arg0 && arg0->tip == DUGUM_TANIMLAYICI) {
                        LlvmIsim *vi = isim_bul(g,
                            arg0->veri.tanimlayici.metin,
                            arg0->veri.tanimlayici.uzunluk);
                        if (vi && vi->eleman_llvm_tip) {
                            dizi_eleman_beklenen = vi->eleman_llvm_tip;
                        }
                    } else if (arg0 && arg0->tip == DUGUM_ERISIM) {
                        /* D-029 fix (2): dizi struct-alani (s.ad) -> alan
                         * tipinden eleman IR cikar (yoksa metin ptr i32
                         * okunup SEGFAULT). */
                        const char *et = dizi_alan_eleman_ir(g, arg0);
                        if (et) dizi_eleman_beklenen = et;
                    }
                }
            }

            /* === Tek-gecis ad cozumu (bkz. ast.h CozumKategorisi) ===
             * Resolver binding'i varsa arama ANAHTARINI o belirler —
             * string'le yeniden cozum YOK; tip kontrol ile codegen insa
             * geregi ayni sembole baglanir. Binding yoksa (COZUM_YOK:
             * built-in'ler, resolver kosmamis AST'ler) eski global-first
             * + aktif-onek-fallback yolu korunur (graceful degradation). */
            const Dugum *cg_hedef = d->veri.cagri.hedef;
            IslevKayit *ik = NULL;
            if (cg_hedef->cozum_kategori == COZUM_MODUL_UYESI &&
                cg_hedef->cozum_modul_onek) {
                /* Modul uyesi: resolver'in yazdigi onekle mangle et —
                 * ayni adli global VARSA BILE ona dusulmez. */
                int muz = 0;
                const char *mangled = modul_mangle(g,
                    cg_hedef->cozum_modul_onek,
                    cg_hedef->cozum_modul_onek_uz,
                    cg_hedef->veri.tanimlayici.metin,
                    cg_hedef->veri.tanimlayici.uzunluk, &muz);
                if (mangled) ik = islev_bul(g, mangled, muz);
            } else if (cg_hedef->cozum_kategori == COZUM_GLOBAL) {
                ik = islev_bul(g,
                    cg_hedef->veri.tanimlayici.metin,
                    cg_hedef->veri.tanimlayici.uzunluk);
            } else if (cg_hedef->cozum_kategori == COZUM_YOK) {
                ik = islev_bul(g,
                    cg_hedef->veri.tanimlayici.metin,
                    cg_hedef->veri.tanimlayici.uzunluk);
                /* D-001: modul govdesi icinden kardes islev ciplak-ad
                 * cagrisi — duz ad bulunamadiysa aktif onekle
                 * ("<modul>.<ad>") dene. Built-in'ler kayitli olmadigi
                 * icin etkilenmez. */
                if (!ik && g->aktif_modul_onek) {
                    int muz = 0;
                    const char *mangled = modul_mangle(g,
                        g->aktif_modul_onek, g->aktif_modul_onek_uz,
                        cg_hedef->veri.tanimlayici.metin,
                        cg_hedef->veri.tanimlayici.uzunluk, &muz);
                    if (mangled) ik = islev_bul(g, mangled, muz);
                }
            }
            /* COZUM_YEREL: ik NULL kalir — asagidaki indirect-call yolu
             * (function pointer lokali/parametresi) devralir. */

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
                            if (i == dizi_deger_arg && dizi_eleman_beklenen) ab = dizi_eleman_beklenen;
                            iargs[i] = ifade_uret(g,
                                d->veri.cagri.argumanlar[i], ab);
                        }
                    }
                    const char *donus_indirect = beklenen ? beklenen : "i32";
                    int rr = yeni_reg(g);
                    if (vi->closure_mu) {
                        /* D-071: closure çağrısı — {ptr fn, ptr env} aç, fn(env,args) */
                        int clo = yeni_reg(g);
                        fprintf(g->out, "  %%%d = load ptr, ptr %%%d\n", clo, vi->reg_no);
                        int fg = yeni_reg(g);
                        fprintf(g->out,
                            "  %%%d = getelementptr { ptr, ptr }, ptr %%%d, i32 0, i32 0\n", fg, clo);
                        int fn_reg = yeni_reg(g);
                        fprintf(g->out, "  %%%d = load ptr, ptr %%%d\n", fn_reg, fg);
                        int eg = yeni_reg(g);
                        fprintf(g->out,
                            "  %%%d = getelementptr { ptr, ptr }, ptr %%%d, i32 0, i32 1\n", eg, clo);
                        int env_reg = yeni_reg(g);
                        fprintf(g->out, "  %%%d = load ptr, ptr %%%d\n", env_reg, eg);
                        fprintf(g->out, "  %%%d = call %s %%%d(ptr %%%d",
                                rr, donus_indirect, fn_reg, env_reg);
                        for (int i = 0; i < n; i++)
                            fprintf(g->out, ", %s %%%d", iargs[i].tip, iargs[i].reg);
                        fputs(")\n", g->out);
                    } else {
                        /* Düz fn-ptr (closure değil) — geriye uyum (env yok) */
                        int fn_reg = yeni_reg(g);
                        fprintf(g->out, "  %%%d = load ptr, ptr %%%d\n", fn_reg, vi->reg_no);
                        fprintf(g->out, "  %%%d = call %s %%%d(",
                                rr, donus_indirect, fn_reg);
                        for (int i = 0; i < n; i++) {
                            if (i > 0) fputs(", ", g->out);
                            fprintf(g->out, "%s %%%d", iargs[i].tip, iargs[i].reg);
                        }
                        fputs(")\n", g->out);
                    }
                    IfadeSonuc s = { rr, donus_indirect, 0 };
                    return s;
                }
            }

            const char *cagri_adi = d->veri.cagri.hedef->veri.tanimlayici.metin;
            int cagri_adi_uz = d->veri.cagri.hedef->veri.tanimlayici.uzunluk;
            /* D-001: ik onek-fallback ile farkli (mangled) ad altinda
             * bulunduysa cagri adini ona cevir (@modul.ad). */
            if (ik && (ik->ad_uz != cagri_adi_uz ||
                       memcmp(ik->ad, cagri_adi, (size_t)cagri_adi_uz) != 0)) {
                cagri_adi = ik->ad;
                cagri_adi_uz = ik->ad_uz;
            }

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
            } else if (cagri_adi_uz == 8 &&
                       memcmp(cagri_adi, "yaz_bayt", 8) == 0) {
                cagri_adi = "kdl_yaz_bayt"; cagri_adi_uz = 12;
                param_beklenen[0] = "i32"; builtin_donus = "void";
            } else if (cagri_adi_uz == 15 &&
                       memcmp(cagri_adi, "ondalik_bicimle", 15) == 0) {
                cagri_adi = "kdl_ondalik_bicimle"; cagri_adi_uz = 19;
                param_beklenen[0] = "ptr"; builtin_donus = "ptr";
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
                    if (!bekle && i == dizi_deger_arg && dizi_eleman_beklenen) {
                        bekle = dizi_eleman_beklenen;
                    }
                    /* Liste<T> BUG-3 fix (probe pF): ciplak literal arg
                     * callee'nin tam64 paramina i32 emit ediliyordu ->
                     * imza-uyumsuz IR -> SEGFAULT. Generic islevde yalniz
                     * SOMUT paramlara beklenen verilir — generic-param
                     * iceren paramlar atlanir (T inference arg'in dogal
                     * tipinden). generic_param_beklenen ile tek-kaynak
                     * (YOL yolu da ayni helper'i kullanir). */
                    if (!bekle && ik && ik->ast) {
                        bekle = generic_param_beklenen(g, ik->ast, i);
                    }
                    /* D-070 (Sınıf A): arg `Dizi<T>` parametresine gidiyorsa
                     * beklenen_tip=Dizi<T> AST düğümü ver → `[..]` literal HEAP
                     * KdlDizi olur (stack [N x T] değil). Aksi: callee xs'i KdlDizi*
                     * sanıp stack-array'i okur → misaligned UB/SEGFAULT. D-044'ün
                     * "tüm Dizi<T> bağlamları heap" amacını çağrı-arg'a tamamlar. */
                    const Dugum *cagri_eski_bt = g->beklenen_tip;
                    if (ik && ik->ast && ik->ast->tip == DUGUM_ISLEV &&
                        i < ik->ast->veri.islev.param_sayi) {
                        const Dugum *pp = ik->ast->veri.islev.parametreler[i];
                        if (pp && pp->veri.parametre.tip &&
                            pp->veri.parametre.tip->tip == DUGUM_TIP_DIZI) {
                            g->beklenen_tip = pp->veri.parametre.tip;
                        }
                    }
                    args[i] = ifade_uret(g, d->veri.cagri.argumanlar[i], bekle);
                    g->beklenen_tip = cagri_eski_bt;
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
                } else if (n == 10 && memcmp(kdl_buf + 4, "metin_bayt", 10) == 0) {
                    kdl_donus = "i8";
                } else if ((n == 12 && memcmp(kdl_buf + 4, "metin_icerir", 12) == 0) ||
                           (n == 12 && memcmp(kdl_buf + 4, "metin_baslar", 12) == 0) ||
                           (n == 11 && memcmp(kdl_buf + 4, "metin_biter", 11) == 0) ||
                           (n == 10 && memcmp(kdl_buf + 4, "metin_esit", 10) == 0)) {
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
                /* dizi_olustur(N) -> ptr (KdlDizi*). D-030 fix: element_byte
                 * ptr/i64 elemanlar icin 8 OLMALI. Sabit 4 degeri,
                 * kapasite_ayarla'da ptr/tam64 dizisini YARI boyutta reserve
                 * edip dizi_ekle_ptr/tam64'te (boyut<kapasite iken) HEAP-BUFFER-
                 * OVERFLOW'a yol aciyordu (ASan: kdl_dizi_ekle_ptr container
                 * overflow; Dizi<metin> 32 reserve -> yalniz 16 ptr). Eleman
                 * tipi degisken annotasyonundan (g->beklenen_tip = Dizi<T>);
                 * bilinmiyorsa (struct-alan inşası vb.) 8 = guvenli max (i32'yi
                 * 2x reserve eder ama tasma imkansiz). */
                int eb = 8;
                {
                    const Dugum *bt = g->beklenen_tip;
                    if (bt && bt->tip == DUGUM_TIP_REFERANS)
                        bt = bt->veri.tip_referans.hedef_tip;
                    if (bt && bt->tip == DUGUM_TIP_DIZI) {
                        const char *eir = ast_tip_to_ir(g,
                            bt->veri.tip_dizi.eleman_tip);
                        if (eir && strcmp(eir, "ptr") != 0 &&
                            strcmp(eir, "i64") != 0) {
                            eb = 4;  /* i8/i16/i32 -> 4 byte */
                        }
                    }
                }
                int rr = yeni_reg(g);
                int kap = args[0].reg;
                int kap_i32 = int_donustur(g, kap, args[0].tip, "i32");
                fprintf(g->out,
                    "  %%%d = call ptr @kdl_dizi_olustur(i32 %d)\n", rr, eb);
                /* Adim 6: kapasiteyi pre-reserve et (kullanici N istiyor) */
                fprintf(g->out,
                    "  call void @kdl_dizi_kapasite_ayarla(ptr %%%d, i32 %%%d)\n",
                    rr, kap_i32);
                IfadeSonuc s = { rr, "ptr", 0 };
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
                IfadeSonuc s = { rr, "i32", 0 };
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
                IfadeSonuc s = { rr, et, 0 };
                return s;
            }
            else if (cagri_adi_uz == 8 &&
                     memcmp(cagri_adi, "dizi_yaz", 8) == 0) {
                /* dizi_yaz(d, i, v) -> void. i. elemanı yerinde yaz.
                 * Eleman tipi: dizi_eleman_beklenen > args[2].tip > i32. */
                const char *et = dizi_eleman_beklenen
                    ? dizi_eleman_beklenen
                    : (n > 2 ? args[2].tip : "i32");
                const char *fn;
                const char *cast_tip = et;
                if (strcmp(et, "i64") == 0) fn = "kdl_dizi_yaz_tam64";
                else if (strcmp(et, "ptr") == 0) fn = "kdl_dizi_yaz_ptr";
                else { fn = "kdl_dizi_yaz_tam"; cast_tip = "i32"; }
                int idx_i32 = (n > 1) ? int_donustur(g, args[1].reg,
                                                      args[1].tip, "i32") : 0;
                int ev = (n > 2) ? int_donustur(g, args[2].reg,
                                                 args[2].tip, cast_tip) : 0;
                fprintf(g->out,
                    "  call void @%s(ptr %%%d, i32 %%%d, %s %%%d)\n",
                    fn, args[0].reg, idx_i32, cast_tip, ev);
                int rr = yeni_reg(g);
                fprintf(g->out, "  %%%d = add i32 0, 0\n", rr);
                IfadeSonuc s = { rr, "i32", 0 };
                return s;
            }
            else if (cagri_adi_uz == 10 &&
                     memcmp(cagri_adi, "dizi_boyut", 10) == 0) {
                int rr = yeni_reg(g);
                fprintf(g->out,
                    "  %%%d = call i32 @kdl_dizi_boyut(ptr %%%d)\n",
                    rr, args[0].reg);
                IfadeSonuc s = { rr, "i32", 0 };
                return s;
            }
            /* Adim 6: dizi_kapasite + dizi_kapasite_ayarla */
            else if (cagri_adi_uz == 13 &&
                     memcmp(cagri_adi, "dizi_kapasite", 13) == 0) {
                int rr = yeni_reg(g);
                fprintf(g->out,
                    "  %%%d = call i32 @kdl_dizi_kapasite(ptr %%%d)\n",
                    rr, args[0].reg);
                IfadeSonuc s = { rr, "i32", 0 };
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
                IfadeSonuc s = { rr, "i32", 0 };
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

            /* Generic islev: ortak instantiation makinesine yonlendir
             * (capraz-modul YOL yolu da ayni helper'i kullanir). */
            if (ik && ik->generic_mi && ik->ast) {
                return generic_islev_cagri_uret(g, d, ik->ast, args, n, donus);
            }

            if (strcmp(donus, "void") == 0) {
                /* void-returning call: register atama yok */
                fputs("  call void @", g->out);
                yerel_ad_yaz(g->out, cagri_adi, cagri_adi_uz);
                fputs("(", g->out);
                for (int i = 0; i < n; i++) {
                    if (i > 0) fputs(", ", g->out);
                    fprintf(g->out, "%s %%%d", args[i].tip, args[i].reg);
                }
                fputs(")\n", g->out);
                /* Caller bir IfadeSonuc bekliyor — placeholder i32 0 */
                int r = yeni_reg(g);
                fprintf(g->out, "  %%%d = add i32 0, 0\n", r);
                IfadeSonuc s = { r, "i32", 0 };
                return s;
            }
            int r = yeni_reg(g);
            fprintf(g->out, "  %%%d = call %s @", r, donus);
            yerel_ad_yaz(g->out, cagri_adi, cagri_adi_uz);
            fputs("(", g->out);
            for (int i = 0; i < n; i++) {
                if (i > 0) fputs(", ", g->out);
                fprintf(g->out, "%s %%%d", args[i].tip, args[i].reg);
            }
            fputs(")\n", g->out);
            IfadeSonuc s = { r, donus, ik ? ik->donus_isaretsiz : 0 };
            return s;
        }

        /* Linear Types V1 — IR'da zero-overhead (tekkez<T> = T):
         * Audit gap #5: kullan(e) lineer unwrap — onceden default'a
         * dusup ('ifade tipi 33 desteklenmiyor') SESSIZ 0 donerdi.
         * Lineer muhasebe tamamen tip kontrolde; IR pass-through. */
        case DUGUM_KULLAN_IFADE:
            return ifade_uret(g, d->veri.kullan_ifade.operand, beklenen);

        /* Audit gap #6: imha(e) — linear dispose. Operand yan etkileri
         * icin degerlendirilir, deger dusurulur (primitif/transparent
         * tipler icin runtime is yok). Onceden sessiz 0 + yorum. */
        case DUGUM_IMHA_IFADE: {
            (void)ifade_uret(g, d->veri.imha_ifade.operand, NULL);
            int r = yeni_reg(g);
            fprintf(g->out, "  %%%d = add i32 0, 0\n", r);
            IfadeSonuc s = { r, "i32", 0 };
            return s;
        }

        case DUGUM_TIP_DONUSTUR: {
            /* Madde E: x olarak T — explicit cast */
            const char *hedef = ast_tip_to_ir(g, d->veri.tip_donustur.hedef_tip);
            if (!hedef) hedef = "i32";
            /* D-005: hedef tip isaretsizligi sonuca tasinir */
            int hedef_isz = ast_tip_isaretsiz_mi(
                d->veri.tip_donustur.hedef_tip);
            IfadeSonuc kaynak = ifade_uret(g, d->veri.tip_donustur.kaynak,
                                            hedef);
            if (strcmp(kaynak.tip, hedef) == 0) {
                kaynak.isaretsiz = hedef_isz;
                return kaynak;
            }
            int k_kesirli = tip_kesirli_mi(kaynak.tip);
            int h_kesirli = tip_kesirli_mi(hedef);
            int r = yeni_reg(g);
            if (!k_kesirli && !h_kesirli) {
                /* int -> int: zext/sext/trunc — kaynak isaretsizse zext */
                int rr = int_donustur_im(g, kaynak.reg, kaynak.tip, hedef,
                                         kaynak.isaretsiz);
                IfadeSonuc s = { rr, hedef, hedef_isz };
                return s;
            }
            if (!k_kesirli && h_kesirli) {
                /* int -> float/double: sitofp */
                fprintf(g->out, "  %%%d = sitofp %s %%%d to %s\n",
                        r, kaynak.tip, kaynak.reg, hedef);
                IfadeSonuc s = { r, hedef, 0 };
                return s;
            }
            if (k_kesirli && !h_kesirli) {
                /* float/double -> int: fptosi */
                fprintf(g->out, "  %%%d = fptosi %s %%%d to %s\n",
                        r, kaynak.tip, kaynak.reg, hedef);
                IfadeSonuc s = { r, hedef, 0 };
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
            IfadeSonuc s = { r, hedef, 0 };
            return s;
        }

        case DUGUM_LAMBDA: {
            /* D-071 (Sınıf B lambda V2): closure değeri = stack { ptr fn, ptr env }
             * → ptr. Capture by-value (non-escaping). Lifted @lambda_N(ptr env,
             * params) DEFERRED emit (bekleyen_lambdalar). */
            int np = d->veri.lambda.param_sayi;
            const char **p_ad = (const char **)arena_ayir(g->arena,
                sizeof(char *) * (size_t)(np > 0 ? np : 1));
            int *p_uz = (int *)arena_ayir(g->arena,
                sizeof(int) * (size_t)(np > 0 ? np : 1));
            for (int i = 0; i < np; i++) {
                p_ad[i] = d->veri.lambda.parametreler[i]->veri.parametre.ad;
                p_uz[i] = d->veri.lambda.parametreler[i]->veri.parametre.ad_uzunluk;
            }
            CaptureCtx cc;
            cc.g = g; cc.param_adlar = p_ad; cc.param_uzlar = p_uz;
            cc.param_sayi = np; cc.sayi = 0;
            lambda_serbest_tara(&cc, d->veri.lambda.govde);
            char *mang = (char *)arena_ayir(g->arena, 24);
            snprintf(mang, 24, "lambda_%d", g->lambda_sayaci++);
            /* env struct tip string: { ir0, ir1, ... } (capture yoksa kullanılmaz) */
            char envtip[512]; int eo = 0;
            eo += snprintf(envtip + eo, sizeof(envtip) - eo, "{ ");
            for (int i = 0; i < cc.sayi; i++)
                eo += snprintf(envtip + eo, sizeof(envtip) - eo, "%s%s",
                               i ? ", " : "", cc.irler[i]);
            snprintf(envtip + eo, sizeof(envtip) - eo, " }");
            int env_reg = -1;
            if (cc.sayi > 0) {
                env_reg = yeni_reg(g);
                fprintf(g->out, "  %%%d = alloca %s\n", env_reg, envtip);
                for (int i = 0; i < cc.sayi; i++) {
                    LlvmIsim *vi = isim_bul(g, cc.adlar[i], cc.uzlar[i]);
                    int lv = yeni_reg(g);
                    fprintf(g->out, "  %%%d = load %s, ptr %%%d\n",
                            lv, cc.irler[i], vi ? vi->reg_no : 0);
                    int gp = yeni_reg(g);
                    fprintf(g->out,
                        "  %%%d = getelementptr %s, ptr %%%d, i32 0, i32 %d\n",
                        gp, envtip, env_reg, i);
                    fprintf(g->out, "  store %s %%%d, ptr %%%d\n",
                            cc.irler[i], lv, gp);
                }
            }
            /* Lifted fn'i her durumda kuyruğa al (deferred emit). */
            BekleyenLambda *bl = (BekleyenLambda *)arena_ayir(g->arena,
                sizeof(BekleyenLambda));
            bl->dugum = d; bl->mangled = mang; bl->capture_sayi = cc.sayi;
            if (cc.sayi > 0) {
                bl->capture_adlar = (const char **)arena_ayir(g->arena,
                    sizeof(char *) * (size_t)cc.sayi);
                bl->capture_uzlar = (int *)arena_ayir(g->arena,
                    sizeof(int) * (size_t)cc.sayi);
                bl->capture_irler = (const char **)arena_ayir(g->arena,
                    sizeof(char *) * (size_t)cc.sayi);
                for (int i = 0; i < cc.sayi; i++) {
                    bl->capture_adlar[i] = cc.adlar[i];
                    bl->capture_uzlar[i] = cc.uzlar[i];
                    bl->capture_irler[i] = cc.irler[i];
                }
            } else {
                bl->capture_adlar = NULL; bl->capture_uzlar = NULL;
                bl->capture_irler = NULL;
            }
            bl->sonraki = g->bekleyen_lambdalar;
            g->bekleyen_lambdalar = bl;
            /* KARMA temsil: yakalama YOK → bare fn-ptr (top-level fn gibi; işlev
             * param'a/yerele bare-call ile uyumlu). Yakalama VAR → closure {fn,env}. */
            if (cc.sayi == 0) {
                g->son_closure = 0;
                int r = yeni_reg(g);
                fprintf(g->out, "  %%%d = bitcast ptr @%s to ptr\n", r, mang);
                IfadeSonuc s = { r, "ptr", 0 };
                return s;
            }
            g->son_closure = 1;
            int clo = yeni_reg(g);
            fprintf(g->out, "  %%%d = alloca { ptr, ptr }\n", clo);
            int fg = yeni_reg(g);
            fprintf(g->out,
                "  %%%d = getelementptr { ptr, ptr }, ptr %%%d, i32 0, i32 0\n", fg, clo);
            fprintf(g->out, "  store ptr @%s, ptr %%%d\n", mang, fg);
            int eg = yeni_reg(g);
            fprintf(g->out,
                "  %%%d = getelementptr { ptr, ptr }, ptr %%%d, i32 0, i32 1\n", eg, clo);
            fprintf(g->out, "  store ptr %%%d, ptr %%%d\n", env_reg, eg);
            IfadeSonuc s = { clo, "ptr", 0 };
            return s;
        }

        default: {
            int r = yeni_reg(g);
            fprintf(g->out, "  ; ifade tipi %d desteklenmiyor\n", d->tip);
            fprintf(g->out, "  %%%d = add i32 0, 0\n", r);
            IfadeSonuc s = { r, "i32", 0 };
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
                /* C2.5: ver tamam(x)/hata(e)/değer(x)/hiç — beklenen yapısal
                 * tip = aktif islevin dönüş tipi. */
                const Dugum *eski_bt = g->beklenen_tip;
                g->beklenen_tip = g->aktif_donus_dugum;
                IfadeSonuc s = ifade_uret(g, d->veri.ver.deger, donus_tip);
                g->beklenen_tip = eski_bt;
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
                /* [D-082] ortak heap-promote yardımcısı (init + ATAMA aynı yol) */
                int kdl_reg = dizi_literal_heap_uret(g,
                    d->veri.degisken.deger, eleman_tip);
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
                    /* C2.5: değişken r: sonuç/seçimlik = tamam(x)/hiç — beklenen
                     * yapısal tip = değişkenin annotasyonu. */
                    const Dugum *eski_bt = g->beklenen_tip;
                    g->beklenen_tip = d->veri.degisken.tip;
                    dv = ifade_uret(g, d->veri.degisken.deger, tip);
                    g->beklenen_tip = eski_bt;
                    int rr = int_donustur_im(g, dv.reg, dv.tip, tip,
                                             dv.isaretsiz);
                    fprintf(g->out, "  store %s %%%d, ptr %%%d\n",
                            tip, rr, alloca_reg);
                    isim_ekle(g, d->veri.degisken.ad,
                              d->veri.degisken.ad_uzunluk,
                              1, alloca_reg, tip);
                    /* D-005: dtamN annot -> isaretsiz isim */
                    g->isimler->isaretsiz =
                        ast_tip_isaretsiz_mi(d->veri.degisken.tip);
                    /* v1 bölge-container: *T annot -> pointee kaydi */
                    g->isimler->pointee_llvm_tip =
                        pointee_ir_al(g, d->veri.degisken.tip);
                    /* D-029 fix: &Yapi, *Yapi veya Yapi annot -> yapi IR kaydi */
                    g->isimler->ref_yapi_ir =
                        ref_yapi_ir_al(g, d->veri.degisken.tip);
                    /* Liste<T> BUG-2: Kullanici<X> annot -> X IR kaydi */
                    g->isimler->generic_arg_ir =
                        generic_arg_ir_al(g, d->veri.degisken.tip);
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
                    g->isimler->isaretsiz = dv.isaretsiz;  /* D-005 */
                    /* D-069 Kat.2: değer sabit stack dizisi [N x T] ise N kaydet
                     * (arr[i] sınır-kontrolü için). Annot yok → stack yolu. */
                    if (d->veri.degisken.deger->tip == DUGUM_DIZI_OLUSTUR) {
                        g->isimler->dizi_uzunluk =
                            d->veri.degisken.deger->veri.dizi_olustur.sayi;
                    }
                    /* D-071 KARMA: lambda değeri YAKALAMALI ise closure (closure_mu=1
                     * → çağrıda env-unpack); yakalamasız ise bare fn-ptr (closure_mu=0
                     * → bare-call). son_closure DUGUM_LAMBDA case'inde set edildi. */
                    if (d->veri.degisken.deger->tip == DUGUM_LAMBDA) {
                        g->isimler->closure_mu = g->son_closure;
                    }
                }
            } else {
                /* Deger yok, sadece annot ile alloca */
                if (!tip) tip = "i32";
                int alloca_reg = yeni_reg(g);
                fprintf(g->out, "  %%%d = alloca %s\n", alloca_reg, tip);
                isim_ekle(g, d->veri.degisken.ad,
                          d->veri.degisken.ad_uzunluk,
                          1, alloca_reg, tip);
                g->isimler->isaretsiz =
                    ast_tip_isaretsiz_mi(d->veri.degisken.tip);  /* D-005 */
                g->isimler->pointee_llvm_tip =
                    pointee_ir_al(g, d->veri.degisken.tip);
                g->isimler->ref_yapi_ir =
                    ref_yapi_ir_al(g, d->veri.degisken.tip);
                g->isimler->generic_arg_ir =
                    generic_arg_ir_al(g, d->veri.degisken.tip);
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
                    /* [D-082] xs = [..]  (xs: Dizi<T> heap dinamik) — init
                     * yoluyla AYNI heap-promote; KdlDizi* slot'a store.
                     * Aksi halde stack [N x T] pointer'ı slot'a yazılır →
                     * dizi_ekle/dizi_boyut SEGFAULT (accept-but-crash). */
                    if (i->dinamik_dizi_mi &&
                        d->veri.atama.deger->tip == DUGUM_DIZI_OLUSTUR) {
                        const char *et = i->eleman_llvm_tip
                            ? i->eleman_llvm_tip : "i32";
                        int kdl_reg = dizi_literal_heap_uret(g,
                            d->veri.atama.deger, et);
                        fprintf(g->out, "  store ptr %%%d, ptr %%%d\n",
                                kdl_reg, i->reg_no);
                    } else {
                        IfadeSonuc v = ifade_uret(g, d->veri.atama.deger,
                                                   i->llvm_tip);
                        int rr = int_donustur(g, v.reg, v.tip, i->llvm_tip);
                        fprintf(g->out, "  store %s %%%d, ptr %%%d\n",
                                i->llvm_tip, rr, i->reg_no);
                    }
                }
            } else if (d->veri.atama.hedef &&
                       d->veri.atama.hedef->tip == DUGUM_INDEKS) {
                /* Audit fix #2: arr[i] = v — onceden SESSIZCE dusurulurdu
                 * (hedef yalniz tanimlayici/erisim taniyordu).
                 * Stack dizi: INDEKS okuma yolunun GEP aynasi + store.
                 * Heap dizi (KdlDizi): runtime'da eleman-yazma setter'i
                 * YOK (kdl_dizi_yaz_eleman) — runtime/ bu görevin scope
                 * disinda; gorunur yorum + DUR-SOR raporu (sessiz degil). */
                const Dugum *hedef = d->veri.atama.hedef;
                int heap_dizi = 0;
                const char *pointee_elem = NULL;
                if (hedef->veri.indeks.nesne &&
                    hedef->veri.indeks.nesne->tip == DUGUM_TANIMLAYICI) {
                    LlvmIsim *vi = isim_bul(g,
                        hedef->veri.indeks.nesne->veri.tanimlayici.metin,
                        hedef->veri.indeks.nesne->veri.tanimlayici.uzunluk);
                    if (vi && vi->dinamik_dizi_mi) heap_dizi = 1;
                    /* v1 bölge-container: *T tabani — eleman tipi
                     * POINTEE'den (RHS tipi tam8/tam64 hedefte yanlis
                     * genislik uretirdi). */
                    else if (vi && vi->pointee_llvm_tip) {
                        pointee_elem = vi->pointee_llvm_tip;
                    }
                }
                if (heap_dizi) {
                    fprintf(g->out,
                        "  ; atama: heap dizi eleman atamasi runtime "
                        "setter bekliyor (kdl_dizi_yaz_eleman yok)\n");
                } else {
                    IfadeSonuc nesne = ifade_uret(g,
                        hedef->veri.indeks.nesne, NULL);
                    IfadeSonuc idx = ifade_uret(g,
                        hedef->veri.indeks.indeks, "i64");
                    int idx_r = int_donustur(g, idx.reg, idx.tip, "i64");
                    IfadeSonuc v = ifade_uret(g, d->veri.atama.deger,
                                              pointee_elem);
                    const char *elem_ir = pointee_elem ? pointee_elem
                                                       : v.tip;
                    int vr = v.reg;
                    if (pointee_elem) {
                        vr = int_donustur_im(g, v.reg, v.tip, pointee_elem,
                                             v.isaretsiz);
                    }
                    int gep_r = yeni_reg(g);
                    fprintf(g->out,
                        "  %%%d = getelementptr %s, ptr %%%d, i64 %%%d\n",
                        gep_r, elem_ir, nesne.reg, idx_r);
                    fprintf(g->out, "  store %s %%%d, ptr %%%d\n",
                            elem_ir, vr, gep_r);
                }
            } else if (d->veri.atama.hedef &&
                       d->veri.atama.hedef->tip == DUGUM_ERISIM) {
                /* C-track fix (init-test koku #3): `x.alan = v` daha once
                 * SESSIZCE DUSURULUYORDU (yalniz tanimlayici hedef vardi;
                 * if'e girmeden return 0). Sonuc: blk_yapilandirma_oku
                 * cfg alanlari hep 0 kaldi (D4/D5 config testinin exit 1
                 * nedeni). Simdi: alan adresi (GEP) + store — hem lokal
                 * struct hem &Struct referans param hedefi calisir. */
                /* [D-082] k.xs = [..]  (xs alanı Dizi<T> heap) —
                 * TANIMLAYICI hedefiyle aynı delik: dizi-literali stack
                 * ÜRETME, heap KdlDizi yap ve alan adresine KdlDizi* store
                 * et. dizi_alan_eleman_ir alanın Dizi<T> olup olmadığını
                 * (ve eleman IR tipini) verir; NULL ise normal alan. */
                const char *dizi_et = NULL;
                if (d->veri.atama.deger->tip == DUGUM_DIZI_OLUSTUR) {
                    dizi_et = dizi_alan_eleman_ir(g, d->veri.atama.hedef);
                }
                const char *alan_ir = NULL;
                int adr = erisim_lvalue(g, d->veri.atama.hedef, &alan_ir);
                if (adr >= 0 && dizi_et) {
                    int kdl_reg = dizi_literal_heap_uret(g,
                        d->veri.atama.deger, dizi_et);
                    fprintf(g->out, "  store ptr %%%d, ptr %%%d\n",
                            kdl_reg, adr);
                } else if (adr >= 0 && alan_ir) {
                    IfadeSonuc v = ifade_uret(g, d->veri.atama.deger,
                                               alan_ir);
                    int rr = int_donustur(g, v.reg, v.tip, alan_ir);
                    fprintf(g->out, "  store %s %%%d, ptr %%%d\n",
                            alan_ir, rr, adr);
                } else {
                    fprintf(g->out,
                        "  ; atama: erisim hedefi cozulemedi\n");
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

        /* C1: esles deyimi lowering — switch-with-fall-out.
         * Onceki durum: bu case yoktu -> default -> "; desteklenmiyor"
         * yorumu, tum esles govdesi dusurulurdu (driver'lar magic-number
         * eger zincirine inmek zorunda kaldi). Simdi her kol kendi
         * BB'sinde uretilir ve her BB tam bir terminator (ret/br) ile biter.
         *
         * Sema:
         *     <scrut'u bir kez degerlendir>          ; acik blok
         *     %c = icmp eq <sty> %s, <lit0>
         *     br i1 %c, label %body0, label %test1
         *   body0: <govde0>   -> 'ver' ise 'ret', degilse 'br L_end'
         *   test1: %c = icmp eq <sty> %s, <lit1> ; br body1, test2
         *     ...
         *   testN: br L_end                         ; hicbiri eslesmedi
         *   L_end: <sonraki deyimler / islev epilogu>
         *
         * Joker '_' veya tanimlayici desen = catch-all: kosulsuz dal;
         * sonraki kollar erisilemez. v1 codegen literal + catch-all
         * destekler (driver ihtiyaci: tamsayi literalleri). */
        case DUGUM_ESLES: {
            IfadeSonuc s = ifade_uret(g, d->veri.esles.deger, NULL);
            const char *sty = s.tip ? s.tip : "i32";
            /* C3: &Cesit referansı (ptr) üzerinde eşleş — struct'ı yükle
             * (recursive AST: ağaç referansla gezilir). Desenlerden payload
             * çeşit'i çöz; scrutinee ptr ise load %Cesit. */
            if (strcmp(sty, "ptr") == 0) {
                const Dugum *cd = NULL;
                for (int ki = 0; ki < d->veri.esles.kol_sayi && !cd; ki++) {
                    const Dugum *kol = d->veri.esles.kollar[ki];
                    if (!kol || kol->tip != DUGUM_ESLES_KOLU) continue;
                    const Dugum *ds = kol->veri.esles_kolu.desen;
                    if (ds && ds->tip == DUGUM_DESEN_YOL) {
                        YapiKayit *yk = yapi_bul(g,
                            ds->veri.desen_yol.cesit_ad,
                            ds->veri.desen_yol.cesit_uz);
                        if (yk && yk->ast && yk->ast->tip == DUGUM_CESIT) {
                            cd = yk->ast;
                        }
                    }
                }
                if (cd && cesit_payload_var(cd)) {
                    const char *agg = cesit_struct_ir(g, cd);
                    int lr = yeni_reg(g);
                    fprintf(g->out, "  %%%d = load %s, ptr %%%d\n",
                            lr, agg, s.reg);
                    s.reg = lr;
                    sty = agg;
                }
            }
            int kesirli = (strcmp(sty, "float") == 0 ||
                           strcmp(sty, "double") == 0);
            const char *cmpop = kesirli ? "fcmp oeq" : "icmp eq";
            int L_end = yeni_label(g);
            int n = d->veri.esles.kol_sayi;
            int acik = 1;  /* testler icin acik bir blok mevcut mu */

            for (int i = 0; i < n && acik; i++) {
                const Dugum *kol = d->veri.esles.kollar[i];
                if (!kol || kol->tip != DUGUM_ESLES_KOLU) continue;
                const Dugum *desen = kol->veri.esles_kolu.desen;
                const Dugum *govde = kol->veri.esles_kolu.govde;
                int is_ctor = yapici_desen_mi(desen);
                int catchall = !is_ctor && desen &&
                    (desen->tip == DUGUM_DESEN_JOKER ||
                     desen->tip == DUGUM_DESEN_TANIMLAYICI);
                int L_body = yeni_label(g);
                int L_next = -1;
                int ctor_pf = -1;               /* payload alan indeksi (>=0) */
                const Dugum *ctor_bind = NULL;  /* bağlanacak alt-desen */
                const Dugum *cesit_match_cd = NULL;  /* C3: payload çeşit cd */
                int cesit_match_vi = 0;              /* C3: eşleşen varyant idx */

                if (catchall) {
                    fprintf(g->out, "  br label %%bb%d\n", L_body);
                } else {
                    L_next = yeni_label(g);
                    if (is_ctor) {
                        /* C2.5/C2.7: sonuç/seçimlik destructuring — tag oku +
                         * dallan. tamam/değer=0, hata/hiç=1. Alt-desen DESEN_YOL
                         * ise (hata(Cesit::V)) payload disc'ini de kontrol et
                         * (D6 bir-seviye nesting: tag==hata AND disc==idx). */
                        const char *yad; int yuz;
                        const Dugum *sub = NULL;
                        if (desen->tip == DUGUM_DESEN_YAPICI) {
                            yad = desen->veri.desen_yapici.ad;
                            yuz = desen->veri.desen_yapici.ad_uzunluk;
                            if (desen->veri.desen_yapici.sayi > 0) {
                                sub = desen->veri.desen_yapici.alt_desenler[0];
                            }
                        } else {
                            yad = desen->veri.desen_tanimlayici.ad;
                            yuz = desen->veri.desen_tanimlayici.ad_uzunluk;
                        }
                        ctor_bind = sub;
                        int tag;
                        if (yuz == 4 && memcmp(yad, "hata", 4) == 0) {
                            tag = 1; ctor_pf = 2;
                        } else if (yuz == 4 &&
                                   memcmp(yad, "hi\xc3\xa7", 4) == 0) {
                            tag = 1; ctor_pf = -1;
                        } else {  /* tamam / değer */
                            tag = 0; ctor_pf = 1;
                        }
                        int tr = yeni_reg(g);
                        fprintf(g->out, "  %%%d = extractvalue %s %%%d, 0\n",
                                tr, sty, s.reg);
                        int c1 = yeni_reg(g);
                        fprintf(g->out, "  %%%d = icmp eq i8 %%%d, %d\n",
                                c1, tr, tag);
                        int cr = c1;
                        if (sub && sub->tip == DUGUM_DESEN_YOL && ctor_pf >= 0) {
                            char ftype[160];
                            if (agg_alan_ir(sty, ctor_pf, ftype, sizeof(ftype))) {
                                int idx = -1;
                                YapiKayit *yk = yapi_bul(g,
                                    sub->veri.desen_yol.cesit_ad,
                                    sub->veri.desen_yol.cesit_uz);
                                if (yk && yk->ast &&
                                    yk->ast->tip == DUGUM_CESIT) {
                                    idx = cesit_varyant_indeksi(yk->ast,
                                        sub->veri.desen_yol.varyant_ad,
                                        sub->veri.desen_yol.varyant_uz);
                                }
                                if (idx < 0) idx = 0;
                                int pr = yeni_reg(g);
                                fprintf(g->out,
                                    "  %%%d = extractvalue %s %%%d, %d\n",
                                    pr, sty, s.reg, ctor_pf);
                                int c2 = yeni_reg(g);
                                fprintf(g->out,
                                    "  %%%d = icmp eq %s %%%d, %d\n",
                                    c2, ftype, pr, idx);
                                int cand = yeni_reg(g);
                                fprintf(g->out, "  %%%d = and i1 %%%d, %%%d\n",
                                        cand, c1, c2);
                                cr = cand;
                            }
                        }
                        fprintf(g->out,
                                "  br i1 %%%d, label %%bb%d, label %%bb%d\n",
                                cr, L_body, L_next);
                    } else if (desen && desen->tip == DUGUM_DESEN_YOL) {
                        /* C2.7/C3: Cesit::Variant[(a,b)] — payloadsuz çeşit:
                         * scrutinee bare iN disc; payload çeşit: struct, alan
                         * 0'dan disc çıkar. Payload bağlama için cd/vi kaydet. */
                        int idx = -1;
                        const Dugum *cd = NULL;
                        YapiKayit *yk = yapi_bul(g,
                            desen->veri.desen_yol.cesit_ad,
                            desen->veri.desen_yol.cesit_uz);
                        if (yk && yk->ast && yk->ast->tip == DUGUM_CESIT) {
                            cd = yk->ast;
                            idx = cesit_varyant_indeksi(cd,
                                desen->veri.desen_yol.varyant_ad,
                                desen->veri.desen_yol.varyant_uz);
                        }
                        if (idx < 0) idx = 0;
                        int disc_reg = s.reg;
                        const char *disc_ty = sty;
                        if (cd && cesit_payload_var(cd)) {
                            disc_ty = cesit_disc_ir(cd);
                            int dr = yeni_reg(g);
                            fprintf(g->out,
                                "  %%%d = extractvalue %s %%%d, 0\n",
                                dr, sty, s.reg);
                            disc_reg = dr;
                            cesit_match_cd = cd;
                            cesit_match_vi = idx;
                        }
                        int cr = yeni_reg(g);
                        fprintf(g->out, "  %%%d = icmp eq %s %%%d, %d\n",
                                cr, disc_ty, disc_reg, idx);
                        fprintf(g->out,
                                "  br i1 %%%d, label %%bb%d, label %%bb%d\n",
                                cr, L_body, L_next);
                    } else if (desen && desen->tip == DUGUM_DESEN_LITERAL) {
                        IfadeSonuc lit = ifade_uret(g,
                            desen->veri.desen_literal.deger, sty);
                        int litr = int_donustur(g, lit.reg, lit.tip, sty);
                        int cr = yeni_reg(g);
                        fprintf(g->out, "  %%%d = %s %s %%%d, %%%d\n",
                                cr, cmpop, sty, s.reg, litr);
                        fprintf(g->out,
                                "  br i1 %%%d, label %%bb%d, label %%bb%d\n",
                                cr, L_body, L_next);
                    } else {
                        /* Desteklenmeyen desen (custom ADT vb.): asla eşleşme,
                         * sonraki teste geç. Govde BB yine de geçerli
                         * (terminator'lu) IR olarak üretilir; erişilmez. */
                        fprintf(g->out, "  br label %%bb%d\n", L_next);
                    }
                }

                /* Govde BB */
                fprintf(g->out, "bb%d:\n", L_body);
                ScopeMarker m = scope_gir(g);
                /* C2.5: ctor payload'ını çıkar + bağlı değişkene ata
                 * (tamam(v)/hata(e)/değer(s)). 'tanimsiz tanimlayici' kalkar. */
                if (is_ctor && ctor_pf >= 0 && ctor_bind &&
                    ctor_bind->tip == DUGUM_DESEN_TANIMLAYICI) {
                    char ftype[160];
                    if (agg_alan_ir(sty, ctor_pf, ftype, sizeof(ftype))) {
                        int pr = yeni_reg(g);
                        fprintf(g->out, "  %%%d = extractvalue %s %%%d, %d\n",
                                pr, sty, s.reg, ctor_pf);
                        int ar = yeni_reg(g);
                        fprintf(g->out, "  %%%d = alloca %s\n", ar, ftype);
                        fprintf(g->out, "  store %s %%%d, ptr %%%d\n",
                                ftype, pr, ar);
                        size_t fl = strlen(ftype);
                        char *ftcopy = (char *)arena_ayir(g->arena, fl + 1);
                        if (ftcopy) {
                            memcpy(ftcopy, ftype, fl + 1);
                            isim_ekle(g,
                                ctor_bind->veri.desen_tanimlayici.ad,
                                ctor_bind->veri.desen_tanimlayici.ad_uzunluk,
                                1, ar, ftcopy);
                        }
                    }
                }
                /* C3: çeşit payload deseni Cesit::V(a, b) — varyant alanlarını
                 * extractvalue ile çıkar + bağlı değişkenlere ata. */
                if (cesit_match_cd && desen &&
                    desen->tip == DUGUM_DESEN_YOL &&
                    desen->veri.desen_yol.alt_sayi > 0) {
                    const Dugum *cd = cesit_match_cd;
                    int vi = cesit_match_vi;
                    int ofs = cesit_varyant_alan_ofset(cd, vi);
                    int pn = cesit_varyant_payload_n(cd, vi);
                    int an = desen->veri.desen_yol.alt_sayi;
                    for (int j = 0; j < an && j < pn; j++) {
                        const Dugum *alt = desen->veri.desen_yol.alt_desenler[j];
                        if (!alt || alt->tip != DUGUM_DESEN_TANIMLAYICI) {
                            continue;  /* joker (_) vb. — bind yok */
                        }
                        const char *pir = ast_tip_to_ir(g,
                            cd->veri.cesit.varyant_payload_tipleri[vi][j]);
                        if (!pir) pir = "i32";
                        int pr = yeni_reg(g);
                        fprintf(g->out,
                            "  %%%d = extractvalue %s %%%d, %d\n",
                            pr, sty, s.reg, ofs + j);
                        int ar = yeni_reg(g);
                        fprintf(g->out, "  %%%d = alloca %s\n", ar, pir);
                        fprintf(g->out, "  store %s %%%d, ptr %%%d\n",
                                pir, pr, ar);
                        size_t pl = strlen(pir);
                        char *pcopy = (char *)arena_ayir(g->arena, pl + 1);
                        if (pcopy) {
                            memcpy(pcopy, pir, pl + 1);
                            isim_ekle(g,
                                alt->veri.desen_tanimlayici.ad,
                                alt->veri.desen_tanimlayici.ad_uzunluk,
                                1, ar, pcopy);
                        }
                    }
                }
                int body_term = 0;
                if (govde && govde->tip == DUGUM_BLOK) {
                    body_term = blok_uret(g, govde);
                } else if (govde) {
                    (void)ifade_uret(g, govde, NULL);
                }
                scope_cik(g, m);
                if (!body_term) {
                    fprintf(g->out, "  br label %%bb%d\n", L_end);
                }

                if (catchall) {
                    acik = 0;  /* sonraki kollar erisilemez */
                } else {
                    fprintf(g->out, "bb%d:\n", L_next);
                }
            }

            /* Hicbir kol eslesmezse acik test blogu L_end'e duser. */
            if (acik) {
                fprintf(g->out, "  br label %%bb%d\n", L_end);
            }
            fprintf(g->out, "bb%d:\n", L_end);
            return 0;
        }

        /* C5 on-kosul #1: guvensiz blogu — codegen acisindan duz blok.
         * Onceki durum: bu case yoktu -> default -> "; desteklenmiyor"
         * yorumu, ic blok TAMAMEN dusurulurdu (latent miscompile).
         * Statik kapi tip kontrolunde; burada ic blok aynen uretilir. */
        case DUGUM_GUVENSIZ: {
            ScopeMarker m = scope_gir(g);
            g->guvensiz_derinlik++;   /* D-069 Kat.2: içeride stack sınır-kontrolü atlanır */
            int term = blok_uret(g, d->veri.guvensiz.blok);
            g->guvensiz_derinlik--;
            scope_cik(g, m);
            return term;
        }

        /* === C5: satirici_asm lowering ===
         * call [ret] asm sideeffect "sablon", "kisitlar"(girdiler)
         * 'sideeffect' HER ZAMAN (DCE asm'i silemesin — MMIO/privileged
         * asm icin zorunlu; saf-hesap istisnasi v2). Kisit sirasi LLVM
         * kurali: ciktilar, girdiler, bozulanlar (clobber).
         * Cikti baglama GCC-tarzi lvalue: 0 cikti → void; 1 cikti →
         * dogrudan tip + store; N cikti → {T0,T1,...} agregat +
         * extractvalue + store'lar. Tuple tipi ICAT EDILMEZ (KEMGU
         * yuzeyinde cikti zaten &degisken'lere yazilir). */
        case DUGUM_SATIRICI_ASM: {
            /* C5 AS001: arch-tag, hedef mimariyle uyusmali. Uyusmazsa
             * asm EMIT EDILMEZ (bozuk IR yasak) + olumcul hata sayilir;
             * cagiran (ana.c) derlemeyi hata koduyla bitirir. */
            {
                const char *hm = KEMGU_HEDEF_MIMARI;
                int hm_uz = (int)(sizeof(KEMGU_HEDEF_MIMARI) - 1);
                if (!d->veri.satirici_asm.mimari ||
                    d->veri.satirici_asm.mimari_uz != hm_uz ||
                    memcmp(d->veri.satirici_asm.mimari, hm,
                           (size_t)hm_uz) != 0) {
                    fprintf(stderr,
                        "hata[AS001]: satirici_asm mimari etiketi '%.*s' "
                        "hedef mimariyle uyusmuyor (hedef: %s) — "
                        "satir %d\n",
                        d->veri.satirici_asm.mimari_uz,
                        d->veri.satirici_asm.mimari
                            ? d->veri.satirici_asm.mimari : "",
                        hm, d->satir);
                    g->hata_sayisi++;
                    fprintf(g->out,
                        "  ; AS001: mimari uyusmazligi — asm emit edilmedi\n");
                    return 0;
                }
            }
            int n_out = d->veri.satirici_asm.cikti_sayi;
            int n_in = d->veri.satirici_asm.girdi_sayi;
            int n_clb = d->veri.satirici_asm.bozulan_sayi;

            /* Cikti hedef alloca'lari (isim tablosundan) */
            LlvmIsim **hedefler = NULL;
            if (n_out > 0) {
                hedefler = (LlvmIsim **)arena_ayir_sifir(g->arena,
                    sizeof(LlvmIsim *) * (size_t)n_out);
                if (!hedefler) return 0;
                for (int i = 0; i < n_out; i++) {
                    hedefler[i] = isim_bul(g,
                        d->veri.satirici_asm.cikti_adlar[i],
                        d->veri.satirici_asm.cikti_ad_uzlar[i]);
                    if (!hedefler[i]) {
                        fprintf(g->out,
                            "  ; satirici_asm: cikti hedefi bulunamadi\n");
                        return 0;
                    }
                }
            }

            /* Girdi ifadelerini degerlendir (asm cagrisindan ONCE) */
            IfadeSonuc *girdiler = NULL;
            if (n_in > 0) {
                girdiler = (IfadeSonuc *)arena_ayir_sifir(g->arena,
                    sizeof(IfadeSonuc) * (size_t)n_in);
                if (!girdiler) return 0;
                for (int i = 0; i < n_in; i++) {
                    girdiler[i] = ifade_uret(g,
                        d->veri.satirici_asm.girdi_ifadeler[i], NULL);
                }
            }

            /* Donus tipi: void / T0 / {T0, T1, ...} */
            char rett[256];
            if (n_out == 0) {
                snprintf(rett, sizeof(rett), "void");
            } else if (n_out == 1) {
                snprintf(rett, sizeof(rett), "%s",
                         hedefler[0]->llvm_tip ? hedefler[0]->llvm_tip
                                               : "i32");
            } else {
                int off = snprintf(rett, sizeof(rett), "{ ");
                for (int i = 0; i < n_out && off < (int)sizeof(rett) - 8;
                     i++) {
                    off += snprintf(rett + off, sizeof(rett) - (size_t)off,
                                    "%s%s", i ? ", " : "",
                                    hedefler[i]->llvm_tip
                                        ? hedefler[i]->llvm_tip : "i32");
                }
                snprintf(rett + off, sizeof(rett) - (size_t)off, " }");
            }

            /* call satiri */
            int r = -1;
            if (n_out == 0) {
                fputs("  call void asm sideeffect \"", g->out);
            } else {
                r = yeni_reg(g);
                fprintf(g->out, "  %%%d = call %s asm sideeffect \"",
                        r, rett);
            }
            /* Sablon — IR string escape (global metin sabitleriyle ayni) */
            {
                const char *m = d->veri.satirici_asm.sablon;
                int uz = d->veri.satirici_asm.sablon_uz;
                for (int i = 0; i < uz; i++) {
                    unsigned char ch = (unsigned char)m[i];
                    if (ch == '\\' || ch == '"' || ch < 0x20 || ch >= 0x7F) {
                        fprintf(g->out, "\\%02X", ch);
                    } else {
                        fputc(ch, g->out);
                    }
                }
            }
            fputs("\", \"", g->out);
            /* Kisit listesi: ciktilar, girdiler, bozulanlar */
            {
                int ilk = 1;
                for (int i = 0; i < n_out; i++) {
                    if (!ilk) fputc(',', g->out);
                    ilk = 0;
                    fwrite(d->veri.satirici_asm.cikti_kisitlar[i], 1,
                           (size_t)d->veri.satirici_asm.cikti_kisit_uzlar[i],
                           g->out);
                }
                for (int i = 0; i < n_in; i++) {
                    if (!ilk) fputc(',', g->out);
                    ilk = 0;
                    fwrite(d->veri.satirici_asm.girdi_kisitlar[i], 1,
                           (size_t)d->veri.satirici_asm.girdi_kisit_uzlar[i],
                           g->out);
                }
                for (int i = 0; i < n_clb; i++) {
                    if (!ilk) fputc(',', g->out);
                    ilk = 0;
                    fwrite(d->veri.satirici_asm.bozulanlar[i], 1,
                           (size_t)d->veri.satirici_asm.bozulan_uzlar[i],
                           g->out);
                }
            }
            fputs("\"(", g->out);
            for (int i = 0; i < n_in; i++) {
                if (i > 0) fputs(", ", g->out);
                fprintf(g->out, "%s %%%d", girdiler[i].tip, girdiler[i].reg);
            }
            fputs(")\n", g->out);

            /* Cikti(lar)i hedef alloca'lara yaz */
            if (n_out == 1) {
                fprintf(g->out, "  store %s %%%d, ptr %%%d\n",
                        rett, r, hedefler[0]->reg_no);
            } else if (n_out > 1) {
                for (int i = 0; i < n_out; i++) {
                    const char *ti = hedefler[i]->llvm_tip
                        ? hedefler[i]->llvm_tip : "i32";
                    int er = yeni_reg(g);
                    fprintf(g->out, "  %%%d = extractvalue %s %%%d, %d\n",
                            er, rett, r, i);
                    fprintf(g->out, "  store %s %%%d, ptr %%%d\n",
                            ti, er, hedefler[i]->reg_no);
                }
            }
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

    /* Liste<T> stdlib fix: generic MODUL uyesi bekleyenler kuyrugundan
     * (modul emisyon baglami DISINDA) specialize edilir — govdedeki
     * kardes ciplak-ad cagrilarinin (örn. buyu) cozulmesi icin islev
     * adindaki "modul." onekini aktif_modul_onek olarak kur.
     * "dizi.ekle" -> onek "dizi"; ic ice "a.b.f" -> "a.b". */
    const char *eski_onek = g->aktif_modul_onek;
    int eski_onek_uz = g->aktif_modul_onek_uz;
    {
        const char *iad = islev->veri.islev.ad;
        int iuz = islev->veri.islev.ad_uzunluk;
        int son_nokta = -1;
        for (int i = 0; i < iuz; i++) {
            if (iad[i] == '.') son_nokta = i;
        }
        if (son_nokta > 0) {
            g->aktif_modul_onek = iad;
            g->aktif_modul_onek_uz = son_nokta;
        }
    }
    islev_uret(g, &sahte);
    g->aktif_modul_onek = eski_onek;
    g->aktif_modul_onek_uz = eski_onek_uz;

    /* Subst pop */
    g->substler = eski_substler;
}

/* Bir IR satiri "  %<rakam> = alloca " kalibinda mi? (entry blok'a tasinacak) */
static int alloca_satiri_mi(const char *s, int len) {
    if (len < 4 || s[0] != ' ' || s[1] != ' ' || s[2] != '%') return 0;
    int i = 3;
    if (i >= len || s[i] < '0' || s[i] > '9') return 0;
    while (i < len && s[i] >= '0' && s[i] <= '9') i++;
    return (i + 10 <= len) && strncmp(s + i, " = alloca ", 10) == 0;
}

/* [D-041 YÜKSEK] Fonksiyon govdesini src'den oku; TÜM alloca'lari entry blok
 * basina tasi (LLVM yalniz entry-blok alloca'sini bir kez tahsis eder; diger
 * blok'taki alloca her CALISMADA stack ayirir -> uzun dongude stack overflow).
 * Tasima SSA numara sirasini bozdugu icin tum numarali degerleri (%<rakam>,
 * %bb<ad> ve %<ad> haric) ardisik yeniden numarala. dst'ye yaz. phi YOK
 * (codegen alloca/load-store kullanir) -> ileri-referans yok -> tek-gecis guvenli. */
static void hoist_renumber(FILE *src, FILE *dst) {
    fseek(src, 0, SEEK_END);
    long n = ftell(src);
    fseek(src, 0, SEEK_SET);
    if (n <= 0) return;
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) return;
    size_t got = fread(buf, 1, (size_t)n, src);
    buf[got] = '\0';

    /* Satirlara ayir ('\n' her satira dahil) */
    size_t kapasite = 64, sc = 0;
    char **satir = (char **)malloc(sizeof(char *) * kapasite);
    int *suz = (int *)malloc(sizeof(int) * kapasite);
    size_t bas = 0;
    for (size_t i = 0; i <= got; i++) {
        if (i == got || buf[i] == '\n') {
            if (i == got && bas == i) break;
            if (sc + 1 >= kapasite) {
                kapasite *= 2;
                satir = (char **)realloc(satir, sizeof(char *) * kapasite);
                suz = (int *)realloc(suz, sizeof(int) * kapasite);
            }
            satir[sc] = buf + bas;
            suz[sc] = (int)(i - bas) + (i < got ? 1 : 0);  /* '\n' dahil */
            sc++;
            bas = i + 1;
        }
    }

    /* entry: satirini bul */
    int entry_idx = -1;
    for (size_t i = 0; i < sc; i++)
        if (strncmp(satir[i], "entry:", 6) == 0) { entry_idx = (int)i; break; }
    if (entry_idx < 0) { fwrite(buf, 1, got, dst);  /* beklenmedik: oldugu gibi */
        free(suz); free(satir); free(buf); return; }

    /* Yeni sira: [0..entry] + alloca'lar + digerleri */
    int *sira = (int *)malloc(sizeof(int) * sc);
    size_t so = 0;
    for (int i = 0; i <= entry_idx; i++) sira[so++] = i;
    for (size_t i = (size_t)(entry_idx + 1); i < sc; i++)
        if (alloca_satiri_mi(satir[i], suz[i])) sira[so++] = (int)i;
    for (size_t i = (size_t)(entry_idx + 1); i < sc; i++)
        if (!alloca_satiri_mi(satir[i], suz[i])) sira[so++] = (int)i;

    /* En buyuk reg numarasi -> map boyutu */
    long maxreg = -1;
    for (size_t i = 0; i + 1 < got; ) {
        if (buf[i] == '%' && buf[i+1] >= '0' && buf[i+1] <= '9') {
            long v = 0; size_t j = i + 1;
            while (j < got && buf[j] >= '0' && buf[j] <= '9') { v = v*10 + (buf[j]-'0'); j++; }
            if (v > maxreg) maxreg = v;
            i = j;
        } else i++;
    }
    long *map = (long *)malloc(sizeof(long) * (size_t)(maxreg + 2));
    for (long i = 0; i <= maxreg + 1; i++) map[i] = -1;
    long sonraki = 0;

    /* Yeni sirada gez; %<rakam> -> %<map>; DEF (sonrasi " = ") -> map ata */
    for (size_t k = 0; k < so; k++) {
        const char *s = satir[sira[k]];
        int len = suz[sira[k]];
        for (int i = 0; i < len; ) {
            if (s[i] == '%' && i + 1 < len && s[i+1] >= '0' && s[i+1] <= '9') {
                long v = 0; int j = i + 1;
                while (j < len && s[j] >= '0' && s[j] <= '9') { v = v*10 + (s[j]-'0'); j++; }
                int def = (j + 2 < len && s[j] == ' ' && s[j+1] == '=' && s[j+2] == ' ');
                if (def && v <= maxreg && map[v] < 0) map[v] = sonraki++;
                long nv = (v <= maxreg && map[v] >= 0) ? map[v] : v;
                fprintf(dst, "%%%ld", nv);
                i = j;
            } else {
                fputc(s[i], dst);
                i++;
            }
        }
    }

    free(map); free(sira); free(suz); free(satir); free(buf);
}

static void islev_uret(LlvmGen *g, const Dugum *islev) {
    /* Generic islev: tek basina emit etme — instantiation'lar cagri sirasinda */
    if (islev->veri.islev.tip_param_sayi > 0) return;

    const char *donus = islev->veri.islev.donus_tipi
        ? ast_tip_to_ir(g, islev->veri.islev.donus_tipi)
        : "void";
    if (!donus) donus = "void";
    g_donus_tip = donus;
    g->aktif_donus_dugum = islev->veri.islev.donus_tipi;  /* C2.5: ver yapıcısı için */

    /* [D-041 YÜKSEK] Govdeyi gecici buffer'a yaz; sonra alloca hoist + renumber. */
    FILE *gercek_out = g->out;
    FILE *govde_tmp = tmpfile();
    if (govde_tmp) g->out = govde_tmp;

    /* Realtime Spec V1: gercekzamanli isleve metadata yorumu (V1 minimal;
     * V2'de gercek LLVM metadata: !realtime !N). */
    if (islev->veri.islev.gercekzamanli_mi) {
        fputs("; @kemgu.realtime\n", g->out);
    }

    fprintf(g->out, "define %s @", donus);
    yerel_ad_yaz(g->out, islev->veri.islev.ad, islev->veri.islev.ad_uzunluk);
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
        yerel_ad_yaz(g->out, p->veri.parametre.ad,
                     p->veri.parametre.ad_uzunluk);
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
        yerel_ad_yaz(g->out, p->veri.parametre.ad,
                     p->veri.parametre.ad_uzunluk);
        fprintf(g->out, ", ptr %%%d\n", alloca_reg);
        isim_ekle(g, p->veri.parametre.ad, p->veri.parametre.ad_uzunluk,
                  0, alloca_reg, tip);
        /* D-005: dtamN parametre -> isaretsiz isim */
        g->isimler->isaretsiz =
            ast_tip_isaretsiz_mi(p->veri.parametre.tip);
        /* v1 bölge-container: *T parametre -> pointee kaydi */
        g->isimler->pointee_llvm_tip =
            pointee_ir_al(g, p->veri.parametre.tip);
        /* D-029 fix: &Yapi, *Yapi veya Yapi parametre -> yapi IR kaydi (field
         * erisiminde dogru yapi cozumu) */
        g->isimler->ref_yapi_ir =
            ref_yapi_ir_al(g, p->veri.parametre.tip);
        /* Liste<T> BUG-2: &Kullanici<X> param -> X IR kaydi (subst
         * aktifken X=T dogru cozulur -> ic generic-cagri zinciri) */
        g->isimler->generic_arg_ir =
            generic_arg_ir_al(g, p->veri.parametre.tip);
        /* Adim 7: Eger parametre Dizi<T> ise heap dizi olarak isaretle
         * (caller'dan gelen ptr KdlDizi*). Eleman tipi de yakala. */
        if (p->veri.parametre.tip &&
            p->veri.parametre.tip->tip == DUGUM_TIP_DIZI) {
            const char *et = ast_tip_to_ir(g,
                p->veri.parametre.tip->veri.tip_dizi.eleman_tip);
            if (et) g->isimler->eleman_llvm_tip = et;
            g->isimler->dinamik_dizi_mi = 1;
        }
        /* D-071 KARMA temsil: işlev(...)→R parametresi BARE fn-ptr (top-level fn
         * VEYA yakalamasız lambda) → bare-call (closure_mu=0). Yakalamalı lambda'yı
         * param'a geçmek V2 (D-072). */
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

    /* [D-041] Govdeyi gercek cikti'ya alloca-hoist + renumber ile aktar. */
    if (govde_tmp) {
        g->out = gercek_out;
        hoist_renumber(govde_tmp, gercek_out);
        fclose(govde_tmp);
    }
}

/* D-071 (Sınıf B lambda V2): lifted lambda — `define <ret> @lambda_N(ptr %env,
 * <params>)`. islev_uret deseni + (a) env ilk param, (b) capture'lar %env'den
 * load → lokal. DEFERRED çağrılır (çevre fn gövdesi emit edildikten sonra).
 * v1: ifade-form gövde, dönüş i32 (4 örnek). */
static void lambda_emit(LlvmGen *g, BekleyenLambda *bl) {
    const Dugum *d = bl->dugum;
    int np = d->veri.lambda.param_sayi;
    char envtip[512]; int eo = 0;
    eo += snprintf(envtip + eo, sizeof(envtip) - eo, "{ ");
    for (int i = 0; i < bl->capture_sayi; i++)
        eo += snprintf(envtip + eo, sizeof(envtip) - eo, "%s%s",
                       i ? ", " : "", bl->capture_irler[i]);
    snprintf(envtip + eo, sizeof(envtip) - eo, " }");

    const char *donus = "i32";   /* v1: tek-ifade gövde i32 (V2: gövde/checker'dan) */
    g_donus_tip = donus;
    g->aktif_donus_dugum = NULL;

    FILE *gercek_out = g->out;
    FILE *govde_tmp = tmpfile();
    if (govde_tmp) g->out = govde_tmp;

    /* KARMA temsil: yakalama varsa env ilk param; yoksa env YOK (bare fn). */
    int has_env = bl->capture_sayi > 0;
    fprintf(g->out, "define %s @%s(", donus, bl->mangled);
    if (has_env) fputs("ptr %env", g->out);
    const char **ptip = NULL;
    if (np > 0) ptip = (const char **)arena_ayir(g->arena,
        sizeof(const char *) * (size_t)np);
    for (int i = 0; i < np; i++) {
        const Dugum *p = d->veri.lambda.parametreler[i];
        const char *t = ast_tip_to_ir(g, p->veri.parametre.tip);
        if (!t) t = "i32";
        ptip[i] = t;
        if (i > 0 || has_env) fputs(", ", g->out);
        fprintf(g->out, "%s %%", t);
        yerel_ad_yaz(g->out, p->veri.parametre.ad, p->veri.parametre.ad_uzunluk);
    }
    fputs(") {\nentry:\n", g->out);

    g->reg = 0; g->label = 0; g->isimler = NULL;

    for (int i = 0; i < np; i++) {
        const Dugum *p = d->veri.lambda.parametreler[i];
        const char *t = ptip[i];
        int ar = yeni_reg(g);
        fprintf(g->out, "  %%%d = alloca %s\n", ar, t);
        fprintf(g->out, "  store %s %%", t);
        yerel_ad_yaz(g->out, p->veri.parametre.ad, p->veri.parametre.ad_uzunluk);
        fprintf(g->out, ", ptr %%%d\n", ar);
        isim_ekle(g, p->veri.parametre.ad, p->veri.parametre.ad_uzunluk, 0, ar, t);
        g->isimler->isaretsiz = ast_tip_isaretsiz_mi(p->veri.parametre.tip);
        /* D-071 KARMA: işlev param BARE fn-ptr → bare-call (closure_mu=0). */
    }
    for (int i = 0; i < bl->capture_sayi; i++) {
        const char *t = bl->capture_irler[i];
        int gp = yeni_reg(g);
        fprintf(g->out,
            "  %%%d = getelementptr %s, ptr %%env, i32 0, i32 %d\n", gp, envtip, i);
        int lv = yeni_reg(g);
        fprintf(g->out, "  %%%d = load %s, ptr %%%d\n", lv, t, gp);
        int ar = yeni_reg(g);
        fprintf(g->out, "  %%%d = alloca %s\n", ar, t);
        fprintf(g->out, "  store %s %%%d, ptr %%%d\n", t, lv, ar);
        isim_ekle(g, bl->capture_adlar[i], bl->capture_uzlar[i], 1, ar, t);
    }

    const Dugum *govde = d->veri.lambda.govde;
    int term = 0;
    if (govde && govde->tip == DUGUM_BLOK) {
        /* v1: blok-form gövde son-ver çıkarsama yok (V2/D-072) — güvenli fallback */
        term = blok_uret(g, govde);
    } else if (govde) {
        IfadeSonuc r = ifade_uret(g, govde, donus);
        int rr = int_donustur(g, r.reg, r.tip, donus);
        fprintf(g->out, "  ret %s %%%d\n", donus, rr);
        term = 1;
    }
    if (!term) fprintf(g->out, "  ret %s 0\n", donus);
    fputs("}\n\n", g->out);

    if (govde_tmp) {
        g->out = gercek_out;
        hoist_renumber(govde_tmp, gercek_out);
        fclose(govde_tmp);
    }
}

/* === Public API === */

int llvm_ir_uret(const Dugum *program, FILE *out) {
    if (!out) return 0;
    fputs("; KEMGU LLVM IR (text uretici, ADIM 18 v2 — yapi/metin/multi-int)\n",
          out);
    fputs("; `clang -x ir - -o cikti.exe` ile derlenebilir.\n", out);
    /* C5/C8: triple tek kaynaktan (llvm.h) — hedefe-duyarli secim C8'de */
    fputs("target triple = \"" KEMGU_HEDEF_TRIPLE "\"\n\n", out);
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
    fputs("declare i8 @kdl_metin_bayt(ptr, i32)\n", out);
    fputs("declare i1 @kdl_metin_esit(ptr, ptr)\n", out);
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
    fputs("declare void @kdl_dizi_yaz_tam(ptr, i32, i32)\n", out);
    fputs("declare void @kdl_dizi_yaz_tam64(ptr, i32, i64)\n", out);
    fputs("declare void @kdl_dizi_yaz_ptr(ptr, i32, ptr)\n", out);
    fputs("declare i32 @kdl_dizi_boyut(ptr)\n", out);
    /* Adim 6: capacity API */
    fputs("declare i32 @kdl_dizi_kapasite(ptr)\n", out);
    fputs("declare void @kdl_dizi_kapasite_ayarla(ptr, i32)\n", out);
    /* D-069 Kategori 2: sabit stack dizi sınır-ihlali panic (OOB → temiz durma) */
    fputs("declare void @kdl_panik(ptr)\n", out);
    fputs("@.str.dizi_sinir_panik = private constant "
          "[26 x i8] c\"dizi sinir ihlali (stack)\\00\"\n", out);

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
    fputs("declare void @kdl_yaz_bayt(i32)\n", out);
    fputs("declare ptr @kdl_ondalik_bicimle(ptr)\n", out);
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
    /* C-track ABI fix: %kdl_yetki (16B) Win64 C ABI'de sret/pointer ile
     * tasinir (clang -emit-llvm dogrulamasi: `void(ptr sret, ptr, i16)`).
     * First-class %kdl_yetki arg/donus declare'lari C tarafiyla
     * UYUSMUYORDU (runtime segfault). Tum sinir imzalari C-uyumlu. */
    fputs("declare void @kdl_yetki_olustur(ptr sret(%kdl_yetki) align 8,"
          " i16, i16)\n", out);
    fputs("declare void @kdl_yetki_delege(ptr sret(%kdl_yetki) align 8,"
          " ptr, i16)\n", out);
    fputs("declare void @kdl_yetki_geri_al(ptr)\n", out);
    fputs("declare i32 @kdl_yetki_kontrol(ptr, i16)\n", out);
    fputs("declare i32 @kdl_yetki_kontrol_tipi(ptr, i16, i16)\n", out);
    fputs("declare i64 @kdl_yetki_id(ptr)\n", out);
    fputs("declare i16 @kdl_yetki_tipi(ptr)\n", out);
    fputs("declare i16 @kdl_yetki_izin(ptr)\n", out);
    fputs("declare i8 @kdl_yetki_iptal_mi(ptr)\n\n", out);

    /* MMIO Foundation — 32-bit register erisimi (kdl_mmio_*).
     * Host: mock tampon; bare-metal (-DKEMGU_BARE_METAL): volatile load/store. */
    fputs("declare i16 @kdl_mmio_oku16(i64)\n", out);
    fputs("declare void @kdl_mmio_yaz16(i64, i16)\n", out);
    fputs("declare i32 @kdl_mmio_oku32(i64)\n", out);
    fputs("declare void @kdl_mmio_yaz32(i64, i32)\n", out);
    fputs("declare i64 @kdl_mmio_oku64(i64)\n", out);
    fputs("declare void @kdl_mmio_yaz64(i64, i64)\n\n", out);

    if (!program || program->tip != DUGUM_PROGRAM) {
        fputs("; (program AST'si yok)\n", out);
        return 0;
    }

    Arena *a = arena_olustur(0);
    if (!a) return 0;

    LlvmGen g;
    memset(&g, 0, sizeof(g));
    g.out = out;
    g.arena = a;

    /* Pre-pass: kullan dosyalarini yukle (program->uyeler listesini AST
     * uzerinde mutate ederek genislet) */
    {
        /* Fixed-point kullan genisletme (transitif import destegi, C2.6):
         * worklist'i orijinal uyelerle baslat; DUGUM_KULLAN gordukce dosyayi
         * yukle ve uyelerini worklist'in SONUNA ekle — boylece import edilen
         * dosyanin kendi kullan'lari da islenir (A<-B<-C). Non-kullan uyeler
         * cikti listesine gider. yuklenmis_dosyalar dedup'u cycle/diamond ve
         * cift yuklemeyi engeller (terminasyon garantisi). Cikti sirasi
         * degisebilir ama LLVM cagrilari ada gore cozer + imzalar ayri
         * pre-pass'te kaydedilir, bu yuzden onemsiz. */
        Dugum **is_l = NULL; int is_sayi = 0; int is_kap = 0;        /* worklist */
        Dugum **yeni_uyeler = NULL; int yeni_sayi = 0; int kap = 0;  /* cikti */

        #define IS_EKLE(u) do { \
            if (is_sayi == is_kap) { \
                is_kap = is_kap == 0 ? 16 : is_kap * 2; \
                Dugum **r = (Dugum **)realloc(is_l, sizeof(Dugum *) * (size_t)is_kap); \
                if (!r) break; \
                is_l = r; \
            } \
            is_l[is_sayi++] = (u); \
        } while (0)
        #define EKLE_UYE(u) do { \
            if (yeni_sayi == kap) { \
                kap = kap == 0 ? 16 : kap * 2; \
                Dugum **r = (Dugum **)realloc(yeni_uyeler, sizeof(Dugum *) * (size_t)kap); \
                if (!r) break; \
                yeni_uyeler = r; \
            } \
            yeni_uyeler[yeni_sayi++] = (u); \
        } while (0)

        for (int i = 0; i < program->veri.program.sayi; i++) {
            IS_EKLE(program->veri.program.uyeler[i]);
        }

        for (int wi = 0; wi < is_sayi; wi++) {
            Dugum *uye = is_l[wi];
            /* A: yeni-bicim kullan (tek-segment / secili / alias) —
             * loader (ana.c) sentetik DUGUM_MODUL olarak ekledi;
             * burada duzlestirme YAPILMAZ (meta dugum, atla). */
            if (uye->tip == DUGUM_KULLAN &&
                (uye->veri.kullan.segment_sayi <= 1 ||
                 uye->veri.kullan.secili_sayi > 0 ||
                 uye->veri.kullan.alias_ad != NULL)) {
                continue;
            }
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
                /* Parse + worklist'e ekle (transitif kullan'lar da islensin) */
                Lexer ml; lexer_baslat(&ml, src, dy);
                Parser mp; parser_baslat(&mp, &ml, a, dy, src);
                Dugum *mprog = parser_calistir(&mp);
                if (mprog && mp.hata_sayisi == 0) {
                    for (int k = 0; k < mprog->veri.program.sayi; k++) {
                        IS_EKLE(mprog->veri.program.uyeler[k]);
                    }
                }
            } else {
                EKLE_UYE(uye);
            }
        }
        #undef IS_EKLE
        #undef EKLE_UYE
        free(is_l);
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
        else if (uye->tip == DUGUM_CESIT) cesit_kayit(&g, uye);  /* C2.7 */
        else if (uye->tip == DUGUM_DISA && uye->veri.disa.tanim &&
                 uye->veri.disa.tanim->tip == DUGUM_YAPI) {
            yapi_kayit(&g, uye->veri.disa.tanim);
        }
        else if (uye->tip == DUGUM_DISA && uye->veri.disa.tanim &&
                 uye->veri.disa.tanim->tip == DUGUM_CESIT) {
            cesit_kayit(&g, uye->veri.disa.tanim);
        }
        /* A: modul icindeki yapi/cesit tanimlari (gap fix) */
        else if (uye->tip == DUGUM_MODUL) {
            modul_tipleri_kayit(&g, uye);
        }
        else if (uye->tip == DUGUM_DISA && uye->veri.disa.tanim &&
                 uye->veri.disa.tanim->tip == DUGUM_MODUL) {
            modul_tipleri_kayit(&g, uye->veri.disa.tanim);
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
        /* D-001: modul uyeleri mangled adla (@modul.ad) kayit edilir */
        else if (uye->tip == DUGUM_MODUL) {
            modul_uyeleri_kayit(&g, uye,
                uye->veri.modul.ad, uye->veri.modul.ad_uzunluk);
        }
        else if (uye->tip == DUGUM_DISA && uye->veri.disa.tanim &&
                 uye->veri.disa.tanim->tip == DUGUM_MODUL) {
            const Dugum *m = uye->veri.disa.tanim;
            modul_uyeleri_kayit(&g, m,
                m->veri.modul.ad, m->veri.modul.ad_uzunluk);
        }
    }

    /* Pre-pass: ust duzey sabitleri kayit et (referans yerlerinde inline).
     * `kullan` pre-pass'i sabitleri program uyelerine zaten eklemis olur;
     * boylece hem ayni dosya hem cross-file sabit codegen'de cozulur. */
    for (int i = 0; i < program->veri.program.sayi; i++) {
        const Dugum *uye = program->veri.program.uyeler[i];
        if (uye->tip == DUGUM_SABIT) sabit_kayit(&g, uye);
        else if (uye->tip == DUGUM_DISA && uye->veri.disa.tanim &&
                 uye->veri.disa.tanim->tip == DUGUM_SABIT) {
            sabit_kayit(&g, uye->veri.disa.tanim);
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
        } else if (uye->tip == DUGUM_MODUL) {
            /* D-001: modul uyeleri @modul.ad olarak emit edilir */
            modul_uyeleri_emit(&g, uye,
                uye->veri.modul.ad, uye->veri.modul.ad_uzunluk);
        } else if (uye->tip == DUGUM_DISA && uye->veri.disa.tanim &&
                   uye->veri.disa.tanim->tip == DUGUM_MODUL) {
            const Dugum *m = uye->veri.disa.tanim;
            modul_uyeleri_emit(&g, m,
                m->veri.modul.ad, m->veri.modul.ad_uzunluk);
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

    /* D-071 (Sınıf B lambda V2): bekleyen lifted lambda'ları emit et (deferred —
     * çevre fn gövdeleri bittikten sonra; iç-içe lambda kuyruğu büyütebilir). */
    int lambda_iter = 256;
    while (g.bekleyen_lambdalar && lambda_iter-- > 0) {
        BekleyenLambda *bl = g.bekleyen_lambdalar;
        g.bekleyen_lambdalar = bl->sonraki;
        lambda_emit(&g, bl);
    }

    int hatalar = g.hata_sayisi;
    arena_serbest(a);
    return hatalar;
}
