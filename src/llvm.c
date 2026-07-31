#include "llvm.h"
#include "arena.h"
#include "lexer.h"
#include "parser.h"
#include "tip.h"           /* D-309: ρ_sahip confinement — TipBilgisi kategorileri */
#include "sembol.h"        /* D-309: cozum_sembol->tip erişimi */
#include "escape.h"        /* F4.2b: escape analizi (bölge yönlendirme oracle'ı) */
#include "bolge_atama.h"   /* F4.2b: R-* bölge atama (escape-driven) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

/* D-269 (P1): hedef mimari/triple çalışma-zamanı selector'ı tip.c'de TANIMLI
 * (hem checker=tip_kontrol.o hem codegen=llvm.o ile linklenen düşük-bağımlılıklı TU;
 * llvm.o'yu unit-testlere sokmadan paylaşım). Buradan getter ile erişilir. */

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
    /* İç-içe Dizi<Dizi<T>> için eleman AST tip düğümü. eleman_llvm_tip iç diziyi
     * "ptr" (KdlDizi* descriptor) olarak gizlerken, bu alan iç dizinin GERÇEK AST
     * tipini (Dizi<T> ya da skaler T) saklar. `m[i][j]` zincirinde `m[i]`'nin heap
     * KdlDizi* mı olduğunu ve [j] eleman tipini recursive çözmek için
     * (heap_dizi_eleman_ast). arena_ayir_sifir → varsayılan NULL (init-uyarısı yok). */
    const Dugum *eleman_tip_ast;
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
    /* D-347: pointee dtamN mi (isaretsiz)? Deref-read genisletmesinde
     * zext/sext secimi icin. Bkz. pointee_isaretsiz_al. */
    int pointee_isaretsiz;
    /* D-293: `işlev(...) -> T` tipli değişken/parametrenin DÖNÜŞ IR tipi.
     * llvm_tip bu değişkenler için "{ ptr, ptr }" (fat value) — T'yi SİLER.
     * Closure çağrı yeri (fat-value dispatch) eskiden `beklenen ? beklenen :
     * "i32"` tahmin ediyordu; lambda dönüşü sabit i32 olduğu sürece bu (yanlış
     * ama) tutarlıydı. Lambda dönüşü gövdeden çıkarsanınca tahmin KIRILDI:
     * `metin_uzunluk(f())` gibi beklenen'in yayılmadığı bağlamlarda ptr dönen
     * lambda i32 olarak çağrılıp SEGFAULT üretti. Bildirilen tip otoriter
     * bilgidir → burada saklanır ve çağrı yerinde beklenen'e TERCİH edilir.
     * NULL = closure değil / tip bilinmiyor (arena_ayir_sifir varsayılanı). */
    const char *kapanis_donus_ir;
    /* D-294: `görev<T>` tipli değişken/parametrenin T IR tipi. görev<T> IR'de
     * opak `ptr` (handle) — T'yi SİLER. Runtime birleştir'i i64 taşır; sonucu
     * T'ye daraltmak (trunc / inttoptr) için T gerekir. NULL = görev değil /
     * annotasyonsuz (arena_ayir_sifir varsayılanı) → beklenen'e düşülür. */
    const char *gorev_ic_ir;
    /* D-295: `kanal<T>` tipli değişken/parametrenin T IR tipi. kanal<T> de
     * IR'de opak `ptr` — T'yi SİLER. Runtime kanalı i64 taşır; gönderimde
     * T→i64, alımda i64→T dönüşümü için gerekir. NULL = kanal değil. */
    const char *kanal_ic_ir;
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
    /* V2-F1 (fat-value closure ABI): "closure mu?" artık derleme-zamanı tag
     * DEĞİL — fn değeri {ptr fn, ptr env} fat value; çağrı yerinde env==null
     * runtime kontrolü bare/closure'ı ayırır (closure_mu kaldırıldı). */
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

/* D-307: per-instantiation generic yapı/çeşit örneği (Kutu<metin> → %Kutu$ptr).
 * GERÇEK monomorphization: her (yapı, arg-IR'ları) çifti için AYRI tip emit
 * edilir (eski "T→i32 tek-layout" yerine). subst, alan/payload tiplerini
 * base'in T'sinden arg'a çözer; field access + construction + tip emit onu
 * push eder. base_mangled ("Kutu$ptr") % olmadan; ast = base yapı/çeşit. */
typedef struct MonoTip {
    const char *mangled;      /* "Kutu$ptr" (arena, % yok) */
    const Dugum *ast;         /* base yapı/çeşit AST düğümü */
    TipSubst *subst;          /* tip_param adı → arg IR (alan çözümü) */
    struct MonoTip *sonraki;
} MonoTip;

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
    /* D-304: bildirilen dönüş IR'ı (işlev()->T annotasyonundan). Blok-form
     * gövde son-`ver` çıkarsaması yapamadığı için (döngüsel) dönüş tipini
     * BAĞLAMDAN alır. NULL → ifade-form (gövdeden çıkarsanır) ya da bilinmiyor. */
    const char *beklenen_donus_ir;
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
    MonoTip *mono_tipler;   /* D-307: per-instantiation generic yapı/çeşit örnekleri */
    BekleyenSpec *bekleyenler;  /* sonradan emit edilecek */
    YuklenmisDosya *yuklenmis_dosyalar;  /* kullan tarafindan yuklenenler */
    SabitKayit *sabitler;   /* ust duzey sabit tanimlari (inline icin) */
    SabitKayit *kureseller; /* D-252: küresel değişken (mutable global @ad, load/store) */
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
    /* D-304: bir lambda değeri emit edilirken bağlamın beklediği dönüş IR'ı
     * (değişken/atama annotasyonu işlev()->T ise T'nin IR'i). Blok-form lambda
     * dönüş tipi için. Save/restore ile ayarlanır; yoksa NULL. */
    const char *lambda_beklenen_donus;
    /* V2-F1: son_closure (derleme-zamanı closure tag'i) kaldırıldı — fn değeri
     * artık {ptr fn, ptr env} fat value; closure'luk env!=null ile runtime'da. */
    /* V2-F4.2a: aktif fonksiyonun bölge (ρ) referansı (IR string). Normal fn →
     * "%rho" (ilk param); main + lambda → seed reg (global bölge). Kullanıcı-fn
     * çağrıları + DİZİ allokasyon helper'ları bunu ilk arg geçirir. Bu fazda
     * metin/closure/bölge_al ρ ALMAZ (global; F4.1 davranışı korunur). */
    const char *rho_ref;
    /* F4.2b: aktif işlevin escape + bölge analizi (per-fn; ρ_yerel yönlendirme
     * oracle'ı). (a) IR-NÖTR: çalışır + saklanır, (c-d) tahsis sitelerinde okunur. */
    EscapeAnaliz *aktif_escape;
    BolgeAtama *aktif_bolge;
    /* F4.2b (c-d): scope-yerel bölge IR ref'i ("%rho_yerel" reg) — fn girişinde
     * kdl_bolge_olustur ile açılır, TÜM ret'lerden önce kdl_bolge_serbest. Kaçmayan
     * (BOLGE_YEREL) tahsisler buraya; kaçanlar rho_ref'e (ρ_caller). NULL → yok. */
    const char *rho_yerel;
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

/* === D-252: küresel değişken (modül-mutable global) tablosu === */
static void kuresel_kayit(LlvmGen *g, const Dugum *d) {
    if (!d || d->tip != DUGUM_DEGISKEN || !d->veri.degisken.kuresel_mi) return;
    SabitKayit *s = (SabitKayit *)arena_ayir_sifir(g->arena, sizeof(SabitKayit));
    if (!s) return;
    s->ad = d->veri.degisken.ad;
    s->ad_uz = d->veri.degisken.ad_uzunluk;
    s->deger = d->veri.degisken.deger;   /* init sabit-literal */
    s->tip = d->veri.degisken.tip;
    s->sonraki = g->kureseller;
    g->kureseller = s;
}

static SabitKayit *kuresel_bul(LlvmGen *g, const char *ad, int ad_uz) {
    for (SabitKayit *s = g->kureseller; s; s = s->sonraki) {
        if (s->ad_uz == ad_uz && memcmp(s->ad, ad, (size_t)ad_uz) == 0) {
            return s;
        }
    }
    return NULL;
}

/* Init sabit-literal → LLVM module-global constant string. */
static void kuresel_init_yaz(FILE *out, const Dugum *dv, const char *ir) {
    if (dv && dv->tip == DUGUM_BOS)       { fputs("null", out); return; }
    if (dv && dv->tip == DUGUM_MANTIKSAL) { fprintf(out, "%d", dv->veri.mantiksal.deger ? 1 : 0); return; }
    if (dv && dv->tip == DUGUM_KARAKTER)  { fprintf(out, "%d", dv->veri.karakter.kod_noktasi); return; }
    if (dv && dv->tip == DUGUM_TAM)       { fprintf(out, "%lld", (long long)dv->veri.tam.deger); return; }
    fputs((ir && strcmp(ir, "ptr") == 0) ? "null" : "0", out);   /* fallback */
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

/* F4.2b (d): ρ_yerel'i serbest bırak — HER ret'ten ÖNCE çağrılır (R4: dönüş değeri
 * zaten materyalize/ρ_caller'da). NULL ρ_yerel → no-op. */
static void rho_yerel_serbest_emit(LlvmGen *g) {
    if (g->rho_yerel) {
        fprintf(g->out, "  call void @kdl_bolge_serbest(ptr %s)\n", g->rho_yerel);
    }
}

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
static const char *mono_cesit_inline_ir(LlvmGen *g, const Dugum *cd,
    char **params, int param_sayi, Dugum **args, int arg_sayi);
static int agg_alan_ir(const char *agg, int idx, char *out, size_t out_n);  /* D-307 ileri */

/* D-307: bir tip düğümü tip_paramlardan birine (ADIYLA) atıfta bulunuyor mu?
 * Recursive (referans/pointer/dizi/seçimlik/sonuç/generic-arg içine iner).
 * Bir generic yapı/çeşit'in per-instantiation MONO gerektirip gerektirmediğini
 * belirler: T ALANDA geçiyorsa layout T'ye bağlı → mono (%Kutu$ptr). Geçmiyorsa
 * (Liste<T> gibi — T yalnız heap elemanında, alan {ptr,i64,i64}) → type-erased
 * tek %Liste (eski mono-FONKSİYON modeliyle uyumlu; per-inst KIRARDI). */
static int tip_dugum_param_gecer(const Dugum *td, char **params, int psay) {
    if (!td || psay <= 0) return 0;
    if (td->tip == DUGUM_TIP_BASIT) {
        const char *a = td->veri.tip_basit.ad;
        int u = td->veri.tip_basit.ad_uzunluk;
        for (int i = 0; i < psay; i++) {
            if ((int)strlen(params[i]) == u && memcmp(params[i], a, (size_t)u) == 0)
                return 1;
        }
        return 0;
    }
    switch (td->tip) {
        /* D-307: &T / *T / Dizi<T> / görev<T> / kanal<T> HER T için `ptr` →
         * layout T'den BAĞIMSIZ; T burada geçse bile mono GEREKMEZ (Liste<T>
         * veri:*T → type-erased %Liste doğru). Bu yüzden İNME (0 dön). */
        case DUGUM_TIP_REFERANS:
        case DUGUM_TIP_POINTER:
        case DUGUM_TIP_DIZI:
        case DUGUM_TIP_GOREV:
        case DUGUM_TIP_KANAL:
            return 0;
        case DUGUM_TIP_SECIMLIK:
            return tip_dugum_param_gecer(td->veri.tip_secimlik.ic_tip, params, psay);
        case DUGUM_TIP_SONUC:
            return tip_dugum_param_gecer(td->veri.tip_sonuc.deger_tip, params, psay) ||
                   tip_dugum_param_gecer(td->veri.tip_sonuc.hata_tip, params, psay);
        case DUGUM_TIP_KULLANICI:
            for (int i = 0; i < td->veri.tip_kullanici.tip_arg_sayi; i++)
                if (tip_dugum_param_gecer(td->veri.tip_kullanici.tip_arg[i], params, psay))
                    return 1;
            return 0;
        default: return 0;
    }
}

/* D-307: generic yapı/çeşit'in HERHANGİ bir alan/payload tipi tip_param'a atıf
 * yapıyor mu? Yaparsa layout T'ye bağlı → mono gerekli. */
static int generic_layout_param_bagimli(const Dugum *ast, char **params, int psay) {
    if (!ast) return 0;
    if (ast->tip == DUGUM_YAPI) {
        for (int i = 0; i < ast->veri.yapi.alan_sayi; i++)
            if (tip_dugum_param_gecer(ast->veri.yapi.alanlar[i]->veri.alan.tip,
                                      params, psay)) return 1;
        return 0;
    }
    if (ast->tip == DUGUM_CESIT) {
        for (int vi = 0; vi < ast->veri.cesit.varyant_sayi; vi++) {
            int pn = ast->veri.cesit.varyant_payload_sayilari
                ? ast->veri.cesit.varyant_payload_sayilari[vi] : 0;
            for (int j = 0; j < pn; j++)
                if (tip_dugum_param_gecer(
                        ast->veri.cesit.varyant_payload_tipleri[vi][j],
                        params, psay)) return 1;
        }
        return 0;
    }
    return 0;
}

/* D-307: bir IR tipini mangle-güvenli hâle getir (i32→i32, ptr→ptr, %Foo→Foo,
 * "{ i8, ptr }"→"i8_ptr"). Non-alfanumerik atlanır/'_' olur → geçerli IR-ad. */
static void mono_ir_sanitize(const char *ir, char *out, size_t out_n) {
    size_t j = 0;
    for (size_t i = 0; ir[i] && j + 1 < out_n; i++) {
        char c = ir[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9')) out[j++] = c;
        else if (c == ',') { if (j + 1 < out_n) out[j++] = '_'; }
        /* '%', ' ', '{', '}' atlanır */
    }
    if (j == 0 && out_n > 1) out[j++] = 'x';
    out[j] = '\0';
}

/* D-307: generic yapı/çeşit örneğini kaydet (Kutu, [metin]) → "%Kutu$ptr".
 * base = yapı/çeşit AST; params = tip_paramlar; args = DUGUM tip arg düğümleri.
 * Örnek dedup edilir; yeni ise mono_tipler'e eklenir (deferred emit). subst,
 * base'in T'sini arg IR'ına eşler (alan/payload çözümü için). */
static const char *mono_tip_kayit(LlvmGen *g, const Dugum *base,
                                  const char *base_ad, int base_uz,
                                  char **params, int param_sayi,
                                  Dugum **args, int arg_sayi) {
    char mang[256];
    int mo = snprintf(mang, sizeof(mang), "%.*s", base_uz, base_ad);
    TipSubst *subst = NULL;
    int n = param_sayi < arg_sayi ? param_sayi : arg_sayi;
    for (int i = 0; i < n; i++) {
        const char *air = ast_tip_to_ir(g, args[i]);   /* nested mono destekli */
        if (!air) air = "i32";
        char san[96];
        mono_ir_sanitize(air, san, sizeof(san));
        mo += snprintf(mang + mo, sizeof(mang) - (size_t)mo, "$%s", san);
        TipSubst *ts = (TipSubst *)arena_ayir(g->arena, sizeof(TipSubst));
        ts->ad = params[i];
        ts->ad_uz = (int)strlen(params[i]);
        ts->ir = air;
        ts->sonraki = subst;
        subst = ts;
    }
    char *mkalici = (char *)arena_ayir(g->arena, (size_t)mo + 1);
    memcpy(mkalici, mang, (size_t)mo + 1);
    /* dedup: aynı mangled zaten kayıtlıysa yeniden kayıt etme */
    int yeni = 1;
    for (MonoTip *m = g->mono_tipler; m; m = m->sonraki) {
        if (strcmp(m->mangled, mkalici) == 0) { yeni = 0; break; }
    }
    if (yeni) {
        MonoTip *mt = (MonoTip *)arena_ayir(g->arena, sizeof(MonoTip));
        mt->mangled = mkalici;
        mt->ast = base;
        mt->subst = subst;
        mt->sonraki = g->mono_tipler;
        g->mono_tipler = mt;
    }
    char *res = (char *)arena_ayir(g->arena, (size_t)mo + 2);
    res[0] = '%';
    memcpy(res + 1, mkalici, (size_t)mo + 1);
    return res;
}

/* D-307: bir mangled IR tipini ("%Kutu$ptr") kayıtlı MonoTip'e çöz (yoksa NULL). */
static MonoTip *mono_tip_bul(LlvmGen *g, const char *ir_tip) {
    if (!ir_tip || ir_tip[0] != '%') return NULL;
    const char *ad = ir_tip + 1;
    for (MonoTip *m = g->mono_tipler; m; m = m->sonraki) {
        if (strcmp(m->mangled, ad) == 0) return m;
    }
    return NULL;
}
static const char *pointee_ir_al(LlvmGen *g, const Dugum *tip_d) {
    if (tip_d && tip_d->tip == DUGUM_TIP_POINTER) {
        return ast_tip_to_ir(g, tip_d->veri.tip_pointer.hedef_tip);
    }
    return NULL;
}

static int ast_tip_isaretsiz_mi(const Dugum *tip_d);   /* ileri bildirim */

/* D-347: pointee ISARETSIZ mi (*dtamN)? Deref-read'de pointee genisliginde
 * yukleyip beklenen tipe genisletirken zext/sext secimi buna bagli:
 * *dtam8 icinde 200 varsa tam64'e ZEXT ile 200 gelmeli, SEXT ile -56 gelirdi. */
static int pointee_isaretsiz_al(const Dugum *tip_d) {
    if (tip_d && tip_d->tip == DUGUM_TIP_POINTER) {
        return ast_tip_isaretsiz_mi(tip_d->veri.tip_pointer.hedef_tip);
    }
    return 0;
}

/* D-293: tip düğümü `işlev(...) -> T` ise T'nin IR tipini döner, değilse NULL.
 * Fat value ("{ ptr, ptr }") T'yi sildiği için closure çağrı yerinin dönüş
 * tipini bilmesinin TEK güvenilir kaynağı bildirilen tiptir. Bkz. LlvmIsim
 * kapanis_donus_ir. */
static const char *kapanis_donus_ir_al(LlvmGen *g, const Dugum *tip_d) {
    if (!tip_d || tip_d->tip != DUGUM_TIP_ISLEV) return NULL;
    const Dugum *dt = tip_d->veri.tip_islev.donus_tip;
    if (!dt) return NULL;
    return ast_tip_to_ir(g, dt);
}

/* D-334: fat value ({ ptr fn, ptr env }) uzerinden DOLAYLI CAGRI — ORTAK yol.
 *
 * Bu yardimci, closure'in NEREDE durdugundan bagimsizdir: degisken, YAPI ALANI
 * ya da (ileride) dizi elemani. TEK KAYNAK olmasi onemli — env-null dallanmasi
 * iki yere kopyalanirsa biri duzeltilip digeri unutulur (D-322'de env emisyonu
 * icin ayni ders alindi, `lam_env_uret` oradan cikti).
 *
 * `fv_reg`: ZATEN YUKLENMIS fat deger. env==null → bare fn(ρ, args);
 * env!=null → closure fn(ρ, env, args). "Closure mu" runtime'da belirlenir. */
static IfadeSonuc fat_cagri_uret(LlvmGen *g, int fv_reg, const char *donus,
                                 IfadeSonuc *iargs, int n) {
    int fn_reg = yeni_reg(g);
    fprintf(g->out, "  %%%d = extractvalue { ptr, ptr } %%%d, 0\n", fn_reg, fv_reg);
    int env_reg = yeni_reg(g);
    fprintf(g->out, "  %%%d = extractvalue { ptr, ptr } %%%d, 1\n", env_reg, fv_reg);
    int slot = yeni_reg(g);
    fprintf(g->out, "  %%%d = alloca %s\n", slot, donus);
    int isnull = yeni_reg(g);
    fprintf(g->out, "  %%%d = icmp eq ptr %%%d, null\n", isnull, env_reg);
    int L_bare = yeni_label(g);
    int L_clo = yeni_label(g);
    int L_join = yeni_label(g);
    fprintf(g->out, "  br i1 %%%d, label %%bb%d, label %%bb%d\n",
            isnull, L_bare, L_clo);
    fprintf(g->out, "bb%d:\n", L_bare);
    int rb = yeni_reg(g);
    fprintf(g->out, "  %%%d = call %s %%%d(ptr %s", rb, donus, fn_reg, g->rho_ref);
    for (int i = 0; i < n; i++)
        fprintf(g->out, ", %s %%%d", iargs[i].tip, iargs[i].reg);
    fputs(")\n", g->out);
    fprintf(g->out, "  store %s %%%d, ptr %%%d\n", donus, rb, slot);
    fprintf(g->out, "  br label %%bb%d\n", L_join);
    fprintf(g->out, "bb%d:\n", L_clo);
    int rc = yeni_reg(g);
    fprintf(g->out, "  %%%d = call %s %%%d(ptr %s, ptr %%%d",
            rc, donus, fn_reg, g->rho_ref, env_reg);
    for (int i = 0; i < n; i++)
        fprintf(g->out, ", %s %%%d", iargs[i].tip, iargs[i].reg);
    fputs(")\n", g->out);
    fprintf(g->out, "  store %s %%%d, ptr %%%d\n", donus, rc, slot);
    fprintf(g->out, "  br label %%bb%d\n", L_join);
    fprintf(g->out, "bb%d:\n", L_join);
    int rr = yeni_reg(g);
    fprintf(g->out, "  %%%d = load %s, ptr %%%d\n", rr, donus, slot);
    IfadeSonuc s = { rr, donus, 0 };
    return s;
}

/* D-334: yapi ALANININ bildirilen tip dugumu (kapanis alani icin donus IR'i
 * buradan cikar). Bulunamazsa NULL. */
static const Dugum *yapi_alan_tip_dugumu(YapiKayit *yk,
                                         const char *ad, int ad_uz) {
    if (!yk || !yk->ast) return NULL;
    for (int i = 0; i < yk->ast->veri.yapi.alan_sayi; i++) {
        const Dugum *a = yk->ast->veri.yapi.alanlar[i];
        if (a->veri.alan.ad_uzunluk == ad_uz &&
            memcmp(a->veri.alan.ad, ad, (size_t)ad_uz) == 0)
            return a->veri.alan.tip;
    }
    return NULL;
}

/* D-325: ANNOTASYONSUZ closure'ın dönüş IR'ı — gövdeden MUHAFAZAKAR tahmin.
 *
 * NEDEN: lifted lambda ERTELENMIS emit edilir; define'in donusu gövdenin DOGAL
 * IR tipinden cikarsanir (D-293). Ama `değişken f = || 1.5` gibi ANNOTASYONSUZ
 * baglamada cagri yeri `kapanis_donus_ir`i NULL gorup beklenen'e (i32) duser →
 * `define double @lambda_0` ama `call i32 %fn(...)`. LLVM DOLAYLI cagrida imza
 * DENETLEMEZ (olculdu: program derlendi, exit 127 = SESSIZ YANLIS CEVAP).
 * Eski kod yorumu "bu vaka LLVM tarafindan GURULTULU reddediliyor" diyordu —
 * OLCUM BUNU CURUTTU (D-295 dersinin tekrari: LLVM imza uyusmazligini yutar).
 *
 * COZUM: ayni tahmin HEM define'a (bl->beklenen_donus_ir) HEM cagri yerine
 * (kapanis_donus_ir) verilir → ikisi DAIMA ayni. Tahmin edilemeyen sekil NULL
 * doner → bugunku i32 davranisi (yeni sessizlik EKLENMEZ). */
static const char *lambda_donus_tahmin(LlvmGen *g, const Dugum *govde) {
    if (!govde) return NULL;
    switch (govde->tip) {
        case DUGUM_METIN:     return "ptr";
        case DUGUM_KESIRLI:   return "double";
        case DUGUM_TAM:       return "i32";
        case DUGUM_MANTIKSAL: return "i32";
        case DUGUM_TANIMLAYICI: {
            LlvmIsim *vi = isim_bul(g, govde->veri.tanimlayici.metin,
                                    govde->veri.tanimlayici.uzunluk);
            return (vi && vi->llvm_tip) ? vi->llvm_tip : NULL;
        }
        case DUGUM_CAGRI: {
            const Dugum *h = govde->veri.cagri.hedef;
            if (!h || h->tip != DUGUM_TANIMLAYICI) return NULL;
            for (IslevKayit *i = g->islevler; i; i = i->sonraki) {
                if (i->ad_uz == h->veri.tanimlayici.uzunluk &&
                    memcmp(i->ad, h->veri.tanimlayici.metin,
                           (size_t)i->ad_uz) == 0)
                    return i->donus_tip;
            }
            return NULL;
        }
        case DUGUM_IKILI:     /* aritmetik: sol operandin tipi */
            return lambda_donus_tahmin(g, govde->veri.ikili.sol);
        case DUGUM_BLOK: {    /* blok-form: ILK `ver` deyiminin degeri */
            for (int i = 0; i < govde->veri.blok.sayi; i++) {
                const Dugum *st = govde->veri.blok.deyimler[i];
                if (st && st->tip == DUGUM_VER)
                    return lambda_donus_tahmin(g, st->veri.ver.deger);
            }
            return NULL;
        }
        default: return NULL;   /* bilinmeyen → tahmin YOK (eski davranis) */
    }
}

/* D-294: tip düğümü `görev<T>` ise T'nin IR tipini döner, değilse NULL.
 * `görev<T>` IR'de opak `ptr` (handle) — T SİLİNİR. Runtime birleştir'i i64
 * taşır; sonucu T'ye daraltmak (trunc / inttoptr) için T'nin IR'i gerekir ve
 * tek güvenilir kaynağı bildirilen tiptir. Bkz. LlvmIsim.gorev_ic_ir. */
static const char *gorev_ic_ir_al(LlvmGen *g, const Dugum *tip_d) {
    if (!tip_d || tip_d->tip != DUGUM_TIP_GOREV) return NULL;
    const Dugum *it = tip_d->veri.tip_gorev.ic_tip;
    if (!it) return NULL;
    return ast_tip_to_ir(g, it);
}

/* D-295: tip düğümü `kanal<T>` ise T'nin IR tipini döner, değilse NULL.
 * `kanal<T>` de IR'de opak `ptr` (handle) — T SİLİNİR. Runtime kanalı i64
 * taşır; gönderimde T→i64, alımda i64→T dönüşümü için T gerekir. */
static const char *kanal_ic_ir_al(LlvmGen *g, const Dugum *tip_d) {
    if (!tip_d || tip_d->tip != DUGUM_TIP_KANAL) return NULL;
    const Dugum *it = tip_d->veri.tip_kanal.ic_tip;
    if (!it) return NULL;
    return ast_tip_to_ir(g, it);
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

/* D-307: generic çeşit örneği → INLINE aggregate "{ i8, <payloads T→arg> }".
 * Named %Secim$X yerine inline: agg_alan_ir positional parse eder, named-emit +
 * forward-ref gerekmez, yapısal-eşitlik ile aynı örnekler paylaşılır. YAPI ise
 * named (%Kutu$X) kalır (field-adı→index için). Payloadsuz generic çeşit → disc. */
static const char *mono_cesit_inline_ir(LlvmGen *g, const Dugum *cd,
    char **params, int param_sayi, Dugum **args, int arg_sayi) {
    /* subst: params[i] → arg IR (arg'lar mevcut subst'la çözülür — concrete). */
    TipSubst *subst = NULL;
    int n = param_sayi < arg_sayi ? param_sayi : arg_sayi;
    for (int i = 0; i < n; i++) {
        const char *air = ast_tip_to_ir(g, args[i]);
        if (!air) air = "i32";
        TipSubst *ts = (TipSubst *)arena_ayir(g->arena, sizeof(TipSubst));
        ts->ad = params[i]; ts->ad_uz = (int)strlen(params[i]);
        ts->ir = air; ts->sonraki = subst; subst = ts;
    }
    if (!cesit_payload_var(cd)) return cesit_disc_ir(cd);
    char buf[512];
    int o = snprintf(buf, sizeof(buf), "{ %s", cesit_disc_ir(cd));
    TipSubst *eski = g->substler;
    g->substler = subst;   /* payload T→arg */
    for (int vi = 0; vi < cd->veri.cesit.varyant_sayi; vi++) {
        int pn = cesit_varyant_payload_n(cd, vi);
        for (int j = 0; j < pn; j++) {
            const char *pir = ast_tip_to_ir(g,
                cd->veri.cesit.varyant_payload_tipleri[vi][j]);
            o += snprintf(buf + o, sizeof(buf) - (size_t)o, ", %s",
                          pir ? pir : "i32");
        }
    }
    g->substler = eski;
    snprintf(buf + o, sizeof(buf) - (size_t)o, " }");
    char *res = (char *)arena_ayir(g->arena, strlen(buf) + 1);
    strcpy(res, buf);
    return res;
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
    /* D-307: generic çeşit construction — beklenen (Secim<metin>) INLINE
     * aggregate ({i8, ptr}) verir → alloca/GEP per-instantiation; payload
     * tipleri agg_alan_ir ile inline'dan okunur (subst gereksiz). */
    int mono_cesit = 0;
    TipSubst *pl_subst = NULL;   /* D-308: nested-mono payload çözümü (params→args) */
    if (cd->veri.cesit.tip_param_sayi > 0 && g->beklenen_tip) {
        const char *mir = ast_tip_to_ir(g, g->beklenen_tip);
        if (mir && mir[0] == '{') { agg = mir; mono_cesit = 1; }
        /* D-308: payload'u BAŞKA mono yapı olan generic çeşit (Sec<T>{Var(Ic<T>)})
         * — iç construction'ın %Ic$ptr üretmesi için params→args subst kur. Bu
         * olmadan iç yapı_olustur %Ic base'ine düşüp pointer'ı i32'ye kırpardı
         * (sessiz miscompile — ölçüldü: metin_uzunluk çöp döndürüyordu). */
        if (mono_cesit && g->beklenen_tip->tip == DUGUM_TIP_KULLANICI) {
            Dugum **args = g->beklenen_tip->veri.tip_kullanici.tip_arg;
            int as = g->beklenen_tip->veri.tip_kullanici.tip_arg_sayi;
            int np = cd->veri.cesit.tip_param_sayi;
            int nn = np < as ? np : as;
            for (int i = 0; i < nn; i++) {
                const char *air = ast_tip_to_ir(g, args[i]);
                if (!air) air = "i32";
                TipSubst *ts = (TipSubst *)arena_ayir(g->arena, sizeof(TipSubst));
                ts->ad = cd->veri.cesit.tip_paramlar[i];
                ts->ad_uz = (int)strlen(cd->veri.cesit.tip_paramlar[i]);
                ts->ir = air; ts->sonraki = pl_subst; pl_subst = ts;
            }
        }
    }
    int ar = yeni_reg(g);
    fprintf(g->out, "  %%%d = alloca %s\n", ar, agg);
    int gt = yeni_reg(g);
    fprintf(g->out, "  %%%d = getelementptr %s, ptr %%%d, i32 0, i32 0\n",
            gt, agg, ar);
    fprintf(g->out, "  store %s %d, ptr %%%d\n", disc, vi, gt);
    int pn = cesit_varyant_payload_n(cd, vi);
    int ofs = cesit_varyant_alan_ofset(cd, vi);
    for (int j = 0; j < pn && cagri && j < n; j++) {
        const char *pir;
        char pbuf[160];
        /* D-307: generic çeşit → payload tipini INLINE agg'den oku (agg_alan_ir);
         * base T ast_tip_to_ir'da çözülemez. Non-generic → eski yol. */
        if (mono_cesit && agg_alan_ir(agg, ofs + j, pbuf, sizeof(pbuf))) {
            pir = pbuf;
        } else {
            pir = ast_tip_to_ir(g,
                cd->veri.cesit.varyant_payload_tipleri[vi][j]);
        }
        if (!pir || strcmp(pir, "void") == 0) pir = "i8";
        /* D-308: payload mono yapı (%Ic$ptr) ise iç construction'a AST beklenen_tip
         * + subst ver → yapi_olustur mangled tipi üretir (yoksa %Ic base'e düşer). */
        const Dugum *pl_eski_bt = g->beklenen_tip;
        TipSubst *pl_eski_subst = g->substler;
        if (mono_cesit && pir[0] == '%') {
            g->beklenen_tip = cd->veri.cesit.varyant_payload_tipleri[vi][j];
            if (pl_subst) g->substler = pl_subst;
        }
        IfadeSonuc pv = ifade_uret(g, cagri->veri.cagri.argumanlar[j], pir);
        g->beklenen_tip = pl_eski_bt;
        g->substler = pl_eski_subst;
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
                /* D-307: generic yapı/çeşit + tip argümanı → GERÇEK
                 * per-instantiation mono (%Kutu$ptr), "T→i32 tek-layout" yerine.
                 * tip_arg'lardan bir örnek kaydı üretilir + subst kurulur. */
                int psay = 0; char **prm = NULL;
                if (yk->ast && yk->ast->tip == DUGUM_YAPI) {
                    psay = yk->ast->veri.yapi.tip_param_sayi;
                    prm = yk->ast->veri.yapi.tip_paramlar;
                } else if (yk->ast && yk->ast->tip == DUGUM_CESIT) {
                    psay = yk->ast->veri.cesit.tip_param_sayi;
                    prm = yk->ast->veri.cesit.tip_paramlar;
                }
                if (psay > 0 && tip_d->veri.tip_kullanici.tip_arg_sayi > 0 &&
                    generic_layout_param_bagimli(yk->ast, prm, psay)) {
                    /* D-307: mono YALNIZ layout T'ye bağlıysa (Kutu<T> deger:T).
                     * Liste<T> gibi type-erased (T alanda geçmez) → base %Liste
                     * (mono-fonksiyon modeliyle uyumlu; aşağı düşer).
                     * YAPI → named %Kutu$X (field-adı→index gerek);
                     * ÇEŞİT → INLINE {i8, payloads} (positional; agg_alan_ir). */
                    if (yk->ast->tip == DUGUM_CESIT) {
                        return mono_cesit_inline_ir(g, yk->ast, prm, psay,
                            tip_d->veri.tip_kullanici.tip_arg,
                            tip_d->veri.tip_kullanici.tip_arg_sayi);
                    }
                    return mono_tip_kayit(g, yk->ast, yk->ad, yk->ad_uz,
                        prm, psay, tip_d->veri.tip_kullanici.tip_arg,
                        tip_d->veri.tip_kullanici.tip_arg_sayi);
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
    /* V2-F1 (fat-value closure ABI): işlev(...)→R = 2-word fat value
     * { ptr fn, ptr env }. env==null → bare fn; env!=null → closure. Önceki
     * `ptr` (bare fn-ptr) D-071'de closure ile çağrı-sınırında uyumsuzdu;
     * fat value + runtime env-null dispatch o tuzağı yapısal olarak eler. */
    if (tip_d->tip == DUGUM_TIP_ISLEV) return "{ ptr, ptr }";
    /* Katman 2 (Concurrency / DRF V1): görev<T> ve kanal<T> runtime'da OPAK
     * handle (ptr) — görev<T> = KdlGorev*, kanal<T> = KdlKanal*. T yalnız
     * tip-kontrolde yaşar (monomorfik temsil; runtime T bilgisi taşımaz).
     * Bu dal olmadan ikisi de fonksiyon sonundaki `return "i32"` fallback'ine
     * düşüyordu → handle 32 bite kırpılır (64-bit host'ta bozuk pointer). */
    if (tip_d->tip == DUGUM_TIP_GOREV || tip_d->tip == DUGUM_TIP_KANAL) {
        return "ptr";
    }
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
/* Yukarıdakinin AST-temelli kardeşi: alan Dizi<T> ise T'nin AST tip düğümünü
 * döner (iç-içe Dizi<Dizi<T>> alanlarda iç dizi AST'sini recursive çözmek için).
 * dizi_alan_eleman_ir bunu ast_tip_to_ir'den geçirir. */
static const Dugum *dizi_alan_eleman_ast(LlvmGen *g, const Dugum *erisim) {
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
    return alan_tip->veri.tip_dizi.eleman_tip;
}
static const char *dizi_alan_eleman_ir(LlvmGen *g, const Dugum *erisim) {
    const Dugum *elem = dizi_alan_eleman_ast(g, erisim);
    return elem ? ast_tip_to_ir(g, elem) : NULL;
}

/* [D-092] Verilen eleman AST'sini saran sentetik DUGUM_TIP_DIZI üret. ATAMA
 * (`xs = [..]` / `k.xs = [..]`) yolunda beklenen_tip kanalına konur; böylece
 * DUGUM_DIZI_OLUSTUR (DIZI_OLUSTUR heap-path, ~2132) dizi-literalini HEAP
 * KdlDizi* olarak üretir — değişken-init (`değişken xs: Dizi<T> = [..]`) ile
 * AYNI yol. Aksi halde stack [N x T] pointer'ı Dizi<T> (KdlDizi*) slot'una
 * store edilir; sonraki dizi_ekle/dizi_boyut KdlDizi* beklerken stack-array
 * görür → SEGFAULT (accept-but-crash; D-070 ailesinin ATAMA analoğu, D-075
 * 🔴 KEŞİF notunda belgelenmişti).
 *
 * Heap-path Dizi düğümünden YALNIZ `tip` + `veri.tip_dizi.eleman_tip` okur;
 * arena_ayir_sifir gerisini sıfırlar. eleman_ast NULL ise NULL döner. */
static const Dugum *dizi_tip_sar(LlvmGen *g, const Dugum *eleman_ast) {
    if (!eleman_ast) return NULL;
    Dugum *d = (Dugum *)arena_ayir_sifir(g->arena, sizeof(Dugum));
    if (!d) return NULL;
    d->tip = DUGUM_TIP_DIZI;
    d->veri.tip_dizi.eleman_tip = (Dugum *)eleman_ast;
    return d;
}

/* D-085: heap KdlDizi eleman okuma/yazma intrinsic'i — eleman IR tipine göre
 * (i64/ptr ayrı; i8/i16/i32 → tam varyantı, i32 genişlikte taşınır). dizi_al/
 * dizi_yaz built-in'leri ile `[]` lowering'i AYNI seçimi paylaşsın diye ortak. */
static const char *kdl_al_fn(const char *et) {
    if (et && strcmp(et, "i64") == 0) return "kdl_dizi_al_tam64";
    if (et && strcmp(et, "ptr") == 0) return "kdl_dizi_al_ptr";
    return "kdl_dizi_al_tam";
}
static const char *kdl_yaz_fn(const char *et) {
    if (et && strcmp(et, "i64") == 0) return "kdl_dizi_yaz_tam64";
    if (et && strcmp(et, "ptr") == 0) return "kdl_dizi_yaz_ptr";
    return "kdl_dizi_yaz_tam";
}
/* kdl_dizi_al_tam i32 döner; i8/i16 eleman da i32 olarak taşınır (sonra
 * int_donustur daraltır). i64/ptr kendi genişliğinde. */
static const char *kdl_al_donus_ir(const char *et) {
    if (et && (strcmp(et, "i64") == 0 || strcmp(et, "ptr") == 0)) return et;
    return "i32";
}

/* D-087: eleman IR'i by-value YAPI mı (%Yapi)? Skaler/ptr → 0. */
/* D-334: by-value (memcpy'lenen) eleman mi?
 * `%Yapi` (nominal struct) VE `{ ptr, ptr }` (KAPANIS fat value) — ikisi de
 * 1 makine-kelimesine SIGMAZ, runtime'in `kdl_dizi_*_yapi` (eleman_byte +
 * memcpy) yolundan gitmelidir. Fat value'yu skaler sanmak, 16 baytlik
 * agregati `kdl_dizi_ekle_tam(i32)` imzasina gecirirdi (olculdu: LLVM bunu
 * SESSIZCE kabul ediyordu → bozuk dizi). */
static int dizi_eleman_struct_mi(const char *et) {
    if (!et) return 0;
    if (et[0] == '%') return 1;
    return strcmp(et, "{ ptr, ptr }") == 0;
}

/* F4.2b YÖNLENDİRME (SOUND, principle 1+3): dizi-tahsis-düğümü `dizi_d` için ρ seç.
 * ρ_yerel'e (serbest edilecek) SADECE: escape analizinde AÇIKÇA KAYITLI
 * (escape_kayitli_mi) VE BOLGE_YEREL (kaçmaz) VE skaler-eleman dizisi gider.
 * Kayıtsız/belirsiz/CAGIRAN/struct-eleman → g->rho_ref (ρ_caller, serbest EDİLMEZ).
 * principle 1: bolge_belirle'nin default-YEREL'ine GÜVENME — kayıt zorunlu.
 * Hem doğrudan dizi-literali (ifade) hem `değişken xs = [...]` yolu bunu kullanır. */
static const char *bolge_yerel_yonlendir(LlvmGen *g, const Dugum *dizi_d,
                                          const char *elem_ir) {
    /* F4.2b SOUND free-routing — İKİ koşul (her ikisi de POZİTİF + inşa-gereği sound):
     * (1) SKALER-ELEMAN dizisi: elem ne struct (%Y) ne ptr. Skaler eleman → dizi_al
     *     KOPYA döndürür (iç-ptr kaçışı YOK). Dizi<Dizi<T>>/Dizi<metin>/Dizi<yapı>
     *     → ptr/struct eleman → iç heap-ref dizi_al ile kaçabilir → ρ_caller.
     * (2) KESİN-YEREL kanıtı: escape_kesin_yerel (confined değişken — tüm kullanımları
     *     yerinde okuma/yazma + retain-etmeyen dizi-builtin). Escape DFA'nın "kaçış
     *     bulamadım"ına GÜVENMEZ; POZİTİF yerellik kanıtı arar (alias/yeniden-atama/
     *     loop-carried/closure/nested hepsi confined-DENY → ρ_caller; escape hunt 18
     *     UAF bu koşulla kapanır). */
    if (g->aktif_escape && g->rho_yerel
        && !dizi_eleman_struct_mi(elem_ir)
        && strcmp(elem_ir, "ptr") != 0
        && escape_kesin_yerel(g->aktif_escape, dizi_d)) {
        return g->rho_yerel;
    }
    return g->rho_ref;
}
/* kdl_dizi_olustur(eleman_byte) operandını yaz: skaler/ptr → derleme-zamanı
 * sabit; struct → LLVM `sizeof(%Yapi)` const-expr (padding/alignment LLVM
 * layout'uyla birebir; C tarafında elle hesaplama miscompile riski taşırdı). */
static void kdl_eleman_byte_yaz(FILE *out, const char *et) {
    if (dizi_eleman_struct_mi(et)) {
        fprintf(out,
            "ptrtoint (ptr getelementptr (%s, ptr null, i32 1) to i32)", et);
        return;
    }
    int eb = 4;
    if (et && strcmp(et, "i8") == 0) eb = 1;
    else if (et && strcmp(et, "i16") == 0) eb = 2;
    else if (et && (strcmp(et, "i64") == 0 || strcmp(et, "double") == 0 ||
                    strcmp(et, "ptr") == 0)) eb = 8;
    fprintf(out, "%d", eb);
}
/* D-087 struct-eleman dizi emit yardımcıları (ekle/al/yaz) — by-value yapı
 * memcpy ile taşınır (kdl_dizi_*_yapi). */
static void dizi_struct_ekle_emit(LlvmGen *g, int desc_reg, int val_reg,
                                  const char *et) {
    int tmp = yeni_reg(g);
    fprintf(g->out, "  %%%d = alloca %s\n", tmp, et);
    fprintf(g->out, "  store %s %%%d, ptr %%%d\n", et, val_reg, tmp);
    fprintf(g->out,
        "  call void @kdl_dizi_ekle_yapi(ptr %s, ptr %%%d, ptr %%%d)\n",
        g->rho_ref, desc_reg, tmp);   /* V2-F4.2a: ρ */
}
static IfadeSonuc dizi_struct_al_emit(LlvmGen *g, int desc_reg, int idx_i32,
                                      const char *et) {
    int dst = yeni_reg(g);
    fprintf(g->out, "  %%%d = alloca %s\n", dst, et);
    fprintf(g->out,
        "  call void @kdl_dizi_al_yapi(ptr %%%d, i32 %%%d, ptr %%%d)\n",
        desc_reg, idx_i32, dst);
    int rr = yeni_reg(g);
    fprintf(g->out, "  %%%d = load %s, ptr %%%d\n", rr, et, dst);
    IfadeSonuc s = { rr, et, 0 };
    return s;
}
static void dizi_struct_yaz_emit(LlvmGen *g, int desc_reg, int idx_i32,
                                 int val_reg, const char *et) {
    int tmp = yeni_reg(g);
    fprintf(g->out, "  %%%d = alloca %s\n", tmp, et);
    fprintf(g->out, "  store %s %%%d, ptr %%%d\n", et, val_reg, tmp);
    fprintf(g->out,
        "  call void @kdl_dizi_yaz_yapi(ptr %%%d, i32 %%%d, ptr %%%d)\n",
        desc_reg, idx_i32, tmp);
}

/* [F-dizi-arg] DUGUM_DIZI_OLUSTUR literalini HEAP KdlDizi* olarak emit eder
 * (kdl_dizi_olustur + kdl_dizi_ekle_*). DUGUM_DIZI_OLUSTUR'un D-044 heap-yolu
 * ile BİREBİR aynı IR'i üretir; tek fark bağlamın g->beklenen_tip yerine
 * parametreyle gelmesi → çıplak `[..]` literali doğrudan bir dizi_* built-in'e
 * (dizi_al/dizi_ekle/...) argüman olarak geçince de heap yoluna girebilir.
 * Aksi halde literal stack [N x T] alloca olur, runtime onu KdlDizi*
 * descriptor sanıp stack çöpünü `boyut`/`veri` olarak okur → ASan misaligned /
 * access-violation (repro: dizi_al([5,6,7],1), dizi_ekle(dis,[40,50,60])).
 *
 * elem_d  : eleman AST tipi (iç-içe Dizi<Dizi<T>> / yapıcı elemanları için
 *           recursive bağlam; NULL → skaler eleman).
 * elem_ir : eleman IR tipi. NULL ise elem_d'den (ast_tip_to_ir) hesaplanır;
 *           o da yoksa "i32" (literal varsayılan tamsayı genişliği). */
static IfadeSonuc dizi_literal_heap_emit(LlvmGen *g, const Dugum *d,
                                          const char *elem_ir,
                                          const Dugum *elem_d) {
    if (!elem_ir && elem_d) elem_ir = ast_tip_to_ir(g, elem_d);
    if (!elem_ir) elem_ir = "i32";
    int hn = d->veri.dizi_olustur.sayi;
    /* [KONSOLIDASYON F4.2b × doc17] Heap-zorlanan literalin ρ'sunu da yönlendir:
     * kesin-yerel (confined) → ρ_yerel (ret'te serbest), değilse → ρ_caller. Çıplak
     * dizi_* arg literali / temp kayıtsız → escape_kesin_yerel=0 → ρ_caller (serbest
     * EDİLMEZ) = güvenli. değişken-bağlı confined literal ρ_yerel'e gider. */
    const char *dizi_rho = bolge_yerel_yonlendir(g, d, elem_ir);
    int kdl_reg = yeni_reg(g);
    fprintf(g->out, "  %%%d = call ptr @kdl_dizi_olustur(ptr %s, i32 ",
            kdl_reg, dizi_rho);   /* F4.2b: yönlendirilmiş ρ */
    kdl_eleman_byte_yaz(g->out, elem_ir);
    fputs(")\n", g->out);
    const Dugum *eski_bt = g->beklenen_tip;
    g->beklenen_tip = elem_d;  /* iç içe dizi/yapıcı elemanları için */
    for (int i = 0; i < hn; i++) {
        IfadeSonuc v = ifade_uret(g, d->veri.dizi_olustur.elemanlar[i], elem_ir);
        if (dizi_eleman_struct_mi(elem_ir)) {
            dizi_struct_ekle_emit(g, kdl_reg, v.reg, elem_ir);
            continue;
        }
        int vr = int_donustur(g, v.reg, v.tip, elem_ir);
        const char *fn = "kdl_dizi_ekle_tam";
        if (strcmp(elem_ir, "i64") == 0) fn = "kdl_dizi_ekle_tam64";
        else if (strcmp(elem_ir, "ptr") == 0) fn = "kdl_dizi_ekle_ptr";
        fprintf(g->out, "  call void @%s(ptr %s, ptr %%%d, %s %%%d)\n",
                fn, dizi_rho, kdl_reg, elem_ir, vr);   /* F4.2b: aynı ρ */
    }
    g->beklenen_tip = eski_bt;
    IfadeSonuc s = { kdl_reg, "ptr", 0 };
    return s;
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
            /* D-307: param + dönüş tip annotasyonlarını mono-kaydet (param
             * alloca'ları per-instantiation tip ister → önceden emit). */
            for (int pi = 0; pi < d->veri.islev.param_sayi; pi++) {
                const Dugum *p = d->veri.islev.parametreler[pi];
                if (p && p->veri.parametre.tip)
                    (void)ast_tip_to_ir(g, p->veri.parametre.tip);
            }
            if (d->veri.islev.donus_tipi)
                (void)ast_tip_to_ir(g, d->veri.islev.donus_tipi);
            ast_taransa_metinleri(g, d->veri.islev.govde); break;
        case DUGUM_DISA:
            ast_taransa_metinleri(g, d->veri.disa.tanim); break;
        case DUGUM_BLOK:
            ast_taransa_metinleri_liste(g, d->veri.blok.deyimler,
                                         d->veri.blok.sayi); break;
        case DUGUM_DEGISKEN:
            /* D-307: değişken tip annotasyonunu mono-kaydet (alloca %Kutu$ptr
             * önceden tanımlı olmalı — forward-ref alloca'da geçersiz). */
            if (d->veri.degisken.tip)
                (void)ast_tip_to_ir(g, d->veri.degisken.tip);
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

/* D-085 + D-088 [YÜKSEK]: Bir indeks tabanı (`nesne`) HEAP KdlDizi* üretiyorsa
 * eleman AST tip düğümünü döner (skaler T ya da İÇ Dizi<T>); değilse NULL →
 * stack GEP yoluna düşer. Çağıran ast_tip_to_ir ile IR'a ("i32"/"ptr"/"%Yapi")
 * çevirir; iç Dizi<T> → "ptr" (zincir devam eder).
 *
 * Kök-neden (D-085): `[]` lowering'i yalnız `nesne == DUGUM_TANIMLAYICI +
 * dinamik_dizi_mi` için heap-route ediyordu; türetilmiş tabanlarda (yapı-alanı
 * k.xs, işlev dönüşü yap()) KdlDizi* DESKRİPTÖRÜNÜ düz veri sanıp GEP yapıyordu.
 *
 * Genişletme (D-088): NESTED INDEKS (`m[i][j]`) eklendi. `m: Dizi<Dizi<T>>`
 * iç içe literalde iç diziler artık heap (DEGISKEN heap path beklenen_tip'i
 * iç eleman tipine ayarlıyor), dolayısıyla `m[i]` heap KdlDizi* döner →
 * `m[i][j]` MUTLAKA kdl_dizi_al'a route edilmeli (stack-GEP iç descriptor'ı
 * bozardı). Recursive: `base[i]`'nin elemanı = base elemanının (Dizi<T>) eleman
 * tipi T. eleman_tip_ast iç dizi AST'sini taşır (eleman_llvm_tip="ptr" gizler).
 *
 * Kapsam: TANIMLAYICI (heap değişken/param) + nested INDEKS + ERISIM (yapı
 * alanı Dizi<T>) + CAGRI (Dizi<T> dönen işlev). */
static const Dugum *heap_dizi_eleman_ast(LlvmGen *g, const Dugum *nesne) {
    if (!nesne) return NULL;
    switch (nesne->tip) {
    case DUGUM_TANIMLAYICI: {
        LlvmIsim *vi = isim_bul(g, nesne->veri.tanimlayici.metin,
                                nesne->veri.tanimlayici.uzunluk);
        if (vi && vi->dinamik_dizi_mi) return vi->eleman_tip_ast;
        return NULL;
    }
    case DUGUM_INDEKS: {
        /* base[i]: base elemanı iç Dizi<T> ise base[i] de heap Dizi<T>,
         * onun elemanı T. (Dizi<Dizi<...>> derinliği için recursive.) */
        const Dugum *be = heap_dizi_eleman_ast(g, nesne->veri.indeks.nesne);
        if (be && be->tip == DUGUM_TIP_DIZI)
            return be->veri.tip_dizi.eleman_tip;
        return NULL;
    }
    case DUGUM_ERISIM:
        return dizi_alan_eleman_ast(g, nesne);
    case DUGUM_CAGRI: {
        if (nesne->veri.cagri.hedef &&
            nesne->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI) {
            const char *ad = nesne->veri.cagri.hedef->veri.tanimlayici.metin;
            int uz = nesne->veri.cagri.hedef->veri.tanimlayici.uzunluk;
            IslevKayit *ik = islev_bul(g, ad, uz);
            if (!ik && g->aktif_modul_onek) {
                int muz = 0;
                const char *m = modul_mangle(g, g->aktif_modul_onek,
                                             g->aktif_modul_onek_uz, ad, uz, &muz);
                if (m) ik = islev_bul(g, m, muz);
            }
            if (ik && ik->ast) {
                const Dugum *rt = ik->ast->veri.islev.donus_tipi;
                if (rt && rt->tip == DUGUM_TIP_DIZI)
                    return rt->veri.tip_dizi.eleman_tip;
            }
        }
        return NULL;
    }
    default:
        return NULL;
    }
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
            /* D-307: layout-param-bağımlı generic çeşit base tipi EMIT EDİLMEZ
             * (inline per-instantiation). Type-erased ise base emit edilir. */
            if (c->veri.cesit.tip_param_sayi > 0 &&
                generic_layout_param_bagimli(c, c->veri.cesit.tip_paramlar,
                                             c->veri.cesit.tip_param_sayi)) continue;
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
        /* D-307: layout-param-bağımlı generic yapı base tipi ('%Kutu') EMIT
         * EDİLMEZ — per-instantiation (%Kutu$ptr). Type-erased generic (Liste<T>,
         * T alanda geçmez) ise base %Liste emit EDİLİR (mono-fonksiyon modeli). */
        if (y->ast->veri.yapi.tip_param_sayi > 0 &&
            generic_layout_param_bagimli(y->ast, y->ast->veri.yapi.tip_paramlar,
                                         y->ast->veri.yapi.tip_param_sayi)) continue;
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

/* D-307: per-instantiation generic yapı/çeşit tip tanımlarını emit et
 * (%Kutu$ptr = type {...}). Fonksiyonlardan SONRA çağrılır — mono_tipler
 * codegen sırasında keşfedilir; LLVM adlı-tipleri modül-genelinde çözer
 * (forward-ref güvenli). Her örneğin subst'ı push edilir → alan/payload
 * tipleri T'den concrete arg'a çözülür. Yeni instantiation ekleyebileceği
 * için (nested generic) sabit-nokta: liste büyümeyi durdurana kadar tekrar. */
static void mono_tip_tanimlari_emit(LlvmGen *g) {
    int emitlenen = 0;
    /* dedup: emit edilmiş mangled'ları izle (basit — MonoKayit yeniden kullan) */
    for (int iter = 0; iter < 64; iter++) {
        int yeni_emit = 0;
        for (MonoTip *m = g->mono_tipler; m; m = m->sonraki) {
            if (mono_emitlendi(g, m->mangled)) continue;
            /* emit edildi işaretle (mangle kaydı) */
            MonoKayit *mk = (MonoKayit *)arena_ayir(g->arena, sizeof(MonoKayit));
            mk->mangled = m->mangled; mk->sonraki = g->monolar; g->monolar = mk;
            yeni_emit = 1; emitlenen = 1;
            const Dugum *a = m->ast;
            TipSubst *eski = g->substler;
            g->substler = m->subst;   /* param→concrete IR (base'de outer subst yok) */
            fprintf(g->out, "%%%s = type { ", m->mangled);
            if (a->tip == DUGUM_CESIT) {
                fprintf(g->out, "%s", cesit_disc_ir(a));
                for (int vi = 0; vi < a->veri.cesit.varyant_sayi; vi++) {
                    int pn = cesit_varyant_payload_n(a, vi);
                    for (int j = 0; j < pn; j++) {
                        const char *ir = ast_tip_to_ir(g,
                            a->veri.cesit.varyant_payload_tipleri[vi][j]);
                        fprintf(g->out, ", %s", ir ? ir : "i32");
                    }
                }
            } else {
                for (int i = 0; i < a->veri.yapi.alan_sayi; i++) {
                    if (i > 0) fputs(", ", g->out);
                    const char *ir = ast_tip_to_ir(g,
                        a->veri.yapi.alanlar[i]->veri.alan.tip);
                    fputs(ir ? ir : "i32", g->out);
                }
            }
            fputs(" }\n", g->out);
            g->substler = eski;
        }
        if (!yeni_emit) break;
    }
    if (emitlenen) fputs("\n", g->out);
}

/* === Ifade IR === */

/* D-326: codegen'in DESTEKLEMEDIGI durum → OLUMCUL. Eskiden bu yol IR'a bir YORUM
 * yazip `add i32 0, 0` uretiyordu: derleme BASARILI gorunuyor, program calisma
 * zamaninda SESSIZCE 0 donuyordu. Olculdu: `değişken xs: Dizi<işlev()->tam32> = [|| 42];
 * ver xs[0]();` → C exit 0 (dogrusu 42), tek iz IR icinde bir yorum satiri.
 * Artik stderr'e yazilir + hata_sayisi artar → ana.c IR'i YAYINLAMAZ ve exit 1
 * (AS001 ile ayni yol). Yayilma alani OLCULDU: 196 korpus/ornek/stdlib dosyasindan
 * yalniz kem_os.kem TEK BASINA derlenince tetikliyor (gercek OS yolu birlestirilmis
 * kaynak → 0 tetik), yani desteklenen hicbir yol kirilmiyor.
 * Kod atanmadi (kullanici-gorunur tani kodlari Mehmet'in karari) — duz metin. */
static IfadeSonuc hata(LlvmGen *g, const char *mesaj) {
    int r = yeni_reg(g);
    fprintf(g->out, "  ; HATA: %s\n", mesaj);
    fprintf(g->out, "  %%%d = add i32 0, 0\n", r);
    fprintf(stderr, "codegen hatasi (desteklenmeyen sekil): %s\n", mesaj);
    g->hata_sayisi++;
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
            /* V2-F1: top-level fn DEĞERİ → fat value {@f, null} (bare fn; env
             * yok). Çağrı yerinde env==null → doğal imzayla @f(args). */
            int t0 = yeni_reg(g);
            fprintf(g->out, "  %%%d = insertvalue { ptr, ptr } undef, ptr ", t0);
            ad_yaz(g->out, "@", 1);
            yerel_ad_yaz(g->out, d->veri.tanimlayici.metin,
                   d->veri.tanimlayici.uzunluk);
            fputs(", 0\n", g->out);
            int r = yeni_reg(g);
            fprintf(g->out,
                "  %%%d = insertvalue { ptr, ptr } %%%d, ptr null, 1\n", r, t0);
            IfadeSonuc s = { r, "{ ptr, ptr }", 0 };
            return s;
        }
        /* D-252: küresel değişken → @ad'den load (mutable global; sabit gibi
         * inline DEĞİL). Erişim güvensiz-only (checker E010 enforce etti). */
        SabitKayit *ku = kuresel_bul(g, d->veri.tanimlayici.metin,
                                     d->veri.tanimlayici.uzunluk);
        if (ku) {
            const char *ir = ku->tip ? ast_tip_to_ir(g, ku->tip) : "i32";
            if (!ir) ir = "i32";
            int r = yeni_reg(g);
            fprintf(g->out, "  %%%d = load %s, ptr @", r, ir);
            yerel_ad_yaz(g->out, d->veri.tanimlayici.metin,
                         d->veri.tanimlayici.uzunluk);
            fputc('\n', g->out);
            IfadeSonuc s = { r, ir, 0 };
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

    /* D-307: generic yapı construction — beklenen_tip (Kutu<metin>) mangled
     * tipi (%Kutu$ptr) + subst verir. Böylece alloca/GEP/store per-instantiation
     * tipiyle, alan tipleri (T→arg) doğru çözülür. Subst fn sonunda geri alınır. */
    TipSubst *mono_eski_subst = g->substler;
    if (y->ast && y->ast->tip == DUGUM_YAPI &&
        y->ast->veri.yapi.tip_param_sayi > 0 && g->beklenen_tip) {
        const char *mir = ast_tip_to_ir(g, g->beklenen_tip);
        MonoTip *mt = mono_tip_bul(g, mir);
        if (mt) { yapi_ir = mir; g->substler = mt->subst; }
    }

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
    g->substler = mono_eski_subst;   /* D-307: subst geri al */
    IfadeSonuc s = { load_r, yapi_ir, 0 };
    return s;
}

/* DUGUM_ERISIM -> struct value uzerinde extractvalue, ptr uzerinde GEP+load */
/* ===================================================================
 * D-309 — ρ_sahip POZİTİF hapsedilme (confinement) kanıtı
 * -------------------------------------------------------------------
 * ρ_sahip yalnız DİZİ tahsislerini taşır: ρ-ABI'de `kdl_dizi_*` ilk argüman
 * olarak ρ alır; metin/closure/bölge_al ρ ALMAZ (global bölge — F4.1). Bir
 * ρ_sahip işaretçisinin görevi aşabildiği yollar ÖLÇÜLDÜ (adversarial):
 *   (1) görev DÖNÜŞÜ   — join sonucu çağırana taşır,
 *   (2) KANAL           — CANLI kaçış (ölçüldü: dizi gönder → join sonrası oku),
 *   (3) küresele yazma  — KAPALI (E011: küresel yalnız skaler),
 *   (4) yakalanan değişkene yazma — env KOPYASI, dışarı sızmaz.
 *
 * Kanıt POZİTİFTİR: escape DFA'nın "kaçış bulamadım"ına GÜVENMEZ; her koşul
 * inşa-gereği sound. Kanıtlanamayan her şey DENY (ρ_sahip sızdırılır = eski
 * güvenli davranış). Bilinmeyen AST düğüm tipi de DENY (`default:`) — dile
 * yeni düğüm eklenirse kanıt sessizce unsound olmaz, sadece muhafazakârlaşır.
 *   P1  gövdedeki her `ver e` için e KANITLI skaler,
 *   P2  erişilebilir kümede `kanal_gönder(k, e)` varsa e KANITLI skaler,
 *   P3  erişilebilir kümede iç-içe `görev_başlat` YOK (iç görev ρ'yu env'inde
 *       tutup dış join'i aşabilir),
 *   P4  her çağrı hedefi çözülebilir kullanıcı-işlevi ya da built-in; işlev
 *       DEĞERİ üzerinden dolaylı çağrı DENY (gövdesi bilinmez).
 * =================================================================== */

#define RHO_MAX_GORULEN 64   /* call-graph kapanışı: ziyaret edilen fn tavanı */

typedef struct {
    LlvmGen *g;
    const Dugum *gorulen[RHO_MAX_GORULEN];   /* döngü kırıcı (özyineleme) */
    int gorulen_sayi;
    int ihlal;                               /* 1 = kanıt düştü */
} RhoConfineCtx;

/* TipBilgisi işaretçi-benzeri mi? Bilinmeyen/çözülemeyen → 1 (DENY tarafı). */
static int rho_tip_isaretci_benzeri(const TipBilgisi *t) {
    if (!t) return 1;
    switch (t->kategori) {
        /* Skaler: değer olarak kopyalanır, ρ'ya iç-işaretçi taşımaz. */
        case TIP_TAM8:  case TIP_TAM16:  case TIP_TAM32:  case TIP_TAM64:
        case TIP_DTAM8: case TIP_DTAM16: case TIP_DTAM32: case TIP_DTAM64:
        case TIP_KESIRLI32: case TIP_KESIRLI64:
        case TIP_MANTIKSAL: case TIP_KARAKTER: case TIP_BOS:
            return 0;
        default:
            return 1;   /* dizi/metin/yapı/referans/görev/kanal/... + bilinmeyen */
    }
}

/* Çağrı hedefinin adı (TANIMLAYICI/YOL); dolaylı çağrıda NULL. */
static const char *rho_cagri_adi_ileri(const Dugum *hedef, int *uz) {
    if (!hedef) return NULL;
    if (hedef->tip == DUGUM_TANIMLAYICI) {
        *uz = hedef->veri.tanimlayici.uzunluk;
        return hedef->veri.tanimlayici.metin;
    }
    if (hedef->tip == DUGUM_YOL) {
        *uz = hedef->veri.yol.sag_ad_uzunluk;
        return hedef->veri.yol.sag_ad;
    }
    return NULL;
}

/* LLVM IR tipi skaler mi? ptr / %Yapi / {agg} → hayır. */
static int rho_skaler_ir(const char *t) {
    if (!t || !*t) return 0;
    if (strcmp(t, "ptr") == 0) return 0;
    if (t[0] == '%' || t[0] == '{') return 0;
    if (strcmp(t, "float") == 0 || strcmp(t, "double") == 0) return 1;
    return t[0] == 'i';   /* i1/i8/i16/i32/i64 */
}

/* İfade KANITLI skaler mi? (1 = kanıtlandı, 0 = kanıtlanamadı → DENY tarafı) */
static int rho_skaler_ifade(LlvmGen *g, const Dugum *d) {
    if (!d) return 0;
    switch (d->tip) {
        case DUGUM_TAM: case DUGUM_KESIRLI:
        case DUGUM_MANTIKSAL: case DUGUM_KARAKTER:
            return 1;
        case DUGUM_TANIMLAYICI:
            /* Çözülmüş sembolün tipi skalerse kanıtlı; çözülmemişse DENY. */
            return d->cozum_sembol && !rho_tip_isaretci_benzeri(
                       ((const Sembol *)d->cozum_sembol)->tip);
        case DUGUM_IKILI:
            /* KEMGU'da işaretçi aritmetiği YOK; yine de iki operandı da iste. */
            return rho_skaler_ifade(g, d->veri.ikili.sol) &&
                   rho_skaler_ifade(g, d->veri.ikili.sag);
        case DUGUM_TEKLI:
            if (d->veri.tekli.op == OP_REF || d->veri.tekli.op == OP_REF_DEGISKEN ||
                d->veri.tekli.op == OP_DEREFERANS) return 0;   /* adres/deref → DENY */
            return rho_skaler_ifade(g, d->veri.tekli.operand);
        case DUGUM_INDEKS:
            /* xs[i] — eleman tipi skalerse kanıtlı (kapsayıcının sembol tipinden). */
            {
                const Dugum *n = d->veri.indeks.nesne;
                if (!n || n->tip != DUGUM_TANIMLAYICI || !n->cozum_sembol) return 0;
                const TipBilgisi *kt = ((const Sembol *)n->cozum_sembol)->tip;
                if (!kt || kt->kategori != TIP_DIZI) return 0;
                return !rho_tip_isaretci_benzeri(kt->veri.dizi.eleman);
            }
        case DUGUM_TIP_DONUSTUR:
            return rho_skaler_ifade(g, d->veri.tip_donustur.kaynak);
        case DUGUM_CAGRI: {
            /* Kullanıcı işlevi + BİLDİRİLEN dönüşü skaler → sonuç kopya değer,
             * ρ'ya iç-işaretçi taşımaz. (`|| yardimci(x)` ifade-form gövdesi bu
             * dal olmadan kanıtlanamıyordu.) Built-in / dolaylı → kanıtlanmaz.
             * NOT: bu YALNIZ değerin skalerliğini kanıtlar; çağrılan işlevin
             * gövdesindeki kanal/spawn ihlalleri AYRICA rho_confine_tara'nın
             * call-graph kapanışında denetlenir. */
            int uz = 0;
            const char *ad = rho_cagri_adi_ileri(d->veri.cagri.hedef, &uz);
            if (!ad) return 0;
            IslevKayit *ik = islev_bul(g, ad, uz);
            if (!ik || ik->generic_mi) return 0;
            return rho_skaler_ir(ik->donus_tip);
        }
        default:
            return 0;   /* erişim/yapı_oluştur/dizi_oluştur/... → kanıtlanmaz */
    }
}

static void rho_confine_tara(RhoConfineCtx *c, const Dugum *d);

static void rho_confine_liste(RhoConfineCtx *c, Dugum **l, int n) {
    for (int i = 0; i < n; i++) rho_confine_tara(c, l[i]);
}

static void rho_confine_tara(RhoConfineCtx *c, const Dugum *d) {
    if (!d || c->ihlal) return;
    switch (d->tip) {
        /* --- yaprak / zararsız --- */
        case DUGUM_TAM: case DUGUM_KESIRLI: case DUGUM_METIN:
        case DUGUM_KARAKTER: case DUGUM_MANTIKSAL: case DUGUM_BOS:
        case DUGUM_TANIMLAYICI: case DUGUM_YOL:
            return;

        /* --- P1: dönüş kanıtlı skaler olmalı --- */
        case DUGUM_VER:
            if (d->veri.ver.deger && !rho_skaler_ifade(c->g, d->veri.ver.deger)) {
                c->ihlal = 1; return;
            }
            rho_confine_tara(c, d->veri.ver.deger);
            return;

        /* --- P2/P3/P4: çağrılar --- */
        case DUGUM_CAGRI: {
            int uz = 0;
            const char *ad = rho_cagri_adi_ileri(d->veri.cagri.hedef, &uz);
            if (!ad) { c->ihlal = 1; return; }   /* P4: dolaylı/hesaplanmış çağrı */

            /* P3: iç-içe görev_başlat — iç görev ρ'yu env'inde tutabilir. */
            if (uz == 14 && memcmp(ad, "g\xc3\xb6rev_ba\xc5\x9f" "lat", 14) == 0) {
                c->ihlal = 1; return;
            }
            /* P2: kanal_gönder(k, e) — e kanıtlı skaler değilse ρ kaçabilir.
             * UZUNLUK 13: "kanal_gönder" 12 karakter ama ö 2 bayt (UTF-8).
             * (14 yazmak testi sessizce geçirmişti — adversarial korpus yakaladı.) */
            if (uz == 13 && memcmp(ad, "kanal_g\xc3\xb6nder", 13) == 0) {
                if (d->veri.cagri.sayi < 2 ||
                    !rho_skaler_ifade(c->g, d->veri.cagri.argumanlar[1])) {
                    c->ihlal = 1; return;
                }
            }
            /* Kullanıcı işlevi → gövdesine in (call-graph kapanışı). */
            IslevKayit *ik = islev_bul(c->g, ad, uz);
            if (ik && ik->ast) {
                if (ik->generic_mi) { c->ihlal = 1; return; }  /* mono gövde belirsiz */
                int yeni = 1;
                for (int i = 0; i < c->gorulen_sayi; i++)
                    if (c->gorulen[i] == ik->ast) { yeni = 0; break; }
                if (yeni) {
                    if (c->gorulen_sayi >= RHO_MAX_GORULEN) { c->ihlal = 1; return; }
                    c->gorulen[c->gorulen_sayi++] = ik->ast;
                    rho_confine_tara(c, ik->ast->veri.islev.govde);
                    if (c->ihlal) return;
                }
            } else if (d->veri.cagri.hedef->cozum_sembol) {
                /* Sembol tablosunda VAR ama işlev kaydı YOK → işlev-değerli
                 * değişken (dolaylı çağrı). Built-in'ler COZUM_YOK kalır ve
                 * ρ'yu görevi aşan bir yere yazamaz (kanal/spawn hariç, ikisi
                 * de yukarıda) → güvenli. */
                c->ihlal = 1; return;
            }
            rho_confine_liste(c, d->veri.cagri.argumanlar, d->veri.cagri.sayi);
            return;
        }

        /* --- gövde/deyim yapıları --- */
        case DUGUM_BLOK:
            rho_confine_liste(c, d->veri.blok.deyimler, d->veri.blok.sayi); return;
        case DUGUM_DEGISKEN:
            rho_confine_tara(c, d->veri.degisken.deger); return;
        case DUGUM_ATAMA:
            rho_confine_tara(c, d->veri.atama.hedef);
            rho_confine_tara(c, d->veri.atama.deger); return;
        case DUGUM_IFADE_DEYIMI:
            rho_confine_tara(c, d->veri.ifade_deyimi.ifade); return;
        case DUGUM_EGER:
            rho_confine_tara(c, d->veri.eger.kosul);
            rho_confine_tara(c, d->veri.eger.gozdoldur);
            rho_confine_tara(c, d->veri.eger.yan); return;
        case DUGUM_IKEN:
            rho_confine_tara(c, d->veri.iken.kosul);
            rho_confine_tara(c, d->veri.iken.govde); return;
        case DUGUM_ICIN:
            rho_confine_tara(c, d->veri.icin.koleksiyon);
            rho_confine_tara(c, d->veri.icin.govde); return;
        case DUGUM_ESLES:
            rho_confine_tara(c, d->veri.esles.deger);
            for (int i = 0; i < d->veri.esles.kol_sayi; i++) {
                const Dugum *kol = d->veri.esles.kollar[i];
                if (kol && kol->tip == DUGUM_ESLES_KOLU)
                    rho_confine_tara(c, kol->veri.esles_kolu.govde);
            }
            return;

        /* --- ifadeler --- */
        case DUGUM_IKILI:
            rho_confine_tara(c, d->veri.ikili.sol);
            rho_confine_tara(c, d->veri.ikili.sag); return;
        case DUGUM_TEKLI:
            rho_confine_tara(c, d->veri.tekli.operand); return;
        case DUGUM_ERISIM:
            rho_confine_tara(c, d->veri.erisim.nesne); return;
        case DUGUM_INDEKS:
            rho_confine_tara(c, d->veri.indeks.nesne);
            rho_confine_tara(c, d->veri.indeks.indeks); return;
        case DUGUM_DIZI_OLUSTUR:
            rho_confine_liste(c, d->veri.dizi_olustur.elemanlar,
                              d->veri.dizi_olustur.sayi); return;
        case DUGUM_YAPI_OLUSTUR:
            for (int i = 0; i < d->veri.yapi_olustur.alan_sayi; i++) {
                const Dugum *aa = d->veri.yapi_olustur.alanlar[i];
                if (aa && aa->tip == DUGUM_ALAN_ATAMA)
                    rho_confine_tara(c, aa->veri.alan_atama.deger);
            }
            return;
        case DUGUM_TIP_DONUSTUR:
            rho_confine_tara(c, d->veri.tip_donustur.kaynak); return;

        default:
            /* Bilinmeyen/ele alınmayan düğüm (lambda, güvensiz, satırici asm,
             * lineer ifadeler, ...) → KANITLANAMADI. Sessiz atlamak unsound
             * olurdu (lambda_serbest_tara'nın `default: return`'ü capture için
             * doğru ama kanıt için DEĞİL). */
            c->ihlal = 1;
            return;
    }
}

/* Görev gövdesi (lambda) ρ_sahip'i hapsediyor mu? 1 = join'de serbest EDİLEBİLİR. */
static int gorev_rho_confined(LlvmGen *g, const Dugum *lambda_d) {
    if (!lambda_d || lambda_d->tip != DUGUM_LAMBDA) return 0;   /* fn değeri → DENY */
    const Dugum *govde = lambda_d->veri.lambda.govde;
    if (!govde) return 0;
    /* P1 (İFADE-FORM): `|| ifade` gövdesinde `ver` DÜĞÜMÜ YOKTUR — gövdenin
     * KENDİSİ dönüş değeridir. Yalnız `ver`e bakmak burada kanıtı UNSOUND
     * yapardı: `görev_başlat(|| [40,2])` ρ_sahip dizisini join'e sızdırır
     * (ölçüldü: bu delik gerçekti, düzeltildi). Blok-form gövdede dönüşler
     * `ver` üzerinden zaten denetleniyor. */
    if (govde->tip != DUGUM_BLOK && !rho_skaler_ifade(g, govde)) return 0;
    RhoConfineCtx c;
    c.g = g; c.gorulen_sayi = 0; c.ihlal = 0;
    rho_confine_tara(&c, govde);
    return !c.ihlal;
}

static IfadeSonuc erisim_uret(LlvmGen *g, const Dugum *d) {
    IfadeSonuc nesne = ifade_uret(g, d->veri.erisim.nesne, NULL);

    /* Yapi tipini cikar: nesne.tip "%Ad" ise yapi adi, "ptr" ise once
     * nesnenin KAYITLI yapi tipi (ref_yapi_ir), yoksa alan-adi arama. */
    YapiKayit *yk = NULL;
    MonoTip *mono_mt = NULL;   /* D-307: mangled generic örnek (%Kutu$ptr) */
    const char *ptr_gep_ir = NULL;   /* D-308: ptr-path mono GEP tipi (%Kutu$ptr) */
    if (nesne.tip && nesne.tip[0] == '%') {
        yk = yapi_bul_ir(g, nesne.tip);
        if (!yk) {
            mono_mt = mono_tip_bul(g, nesne.tip);
            if (mono_mt && mono_mt->ast->tip == DUGUM_YAPI) {
                yk = yapi_bul(g, mono_mt->ast->veri.yapi.ad,
                              mono_mt->ast->veri.yapi.ad_uzunluk);
            }
        }
    } else {
        /* D-029 fix: nesne TANIMLAYICI ise (&Yapi param/lokal) kayitli yapi
         * tipini kullan — global alan-adi tahmini iki yapi ayni alan adini
         * paylasinca YANLIS yapiya cozuyordu (t.ad -> U.ad alan 0 -> t.kind). */
        const Dugum *nd = d->veri.erisim.nesne;
        if (nd && nd->tip == DUGUM_TANIMLAYICI) {
            LlvmIsim *vi = isim_bul(g, nd->veri.tanimlayici.metin,
                                    nd->veri.tanimlayici.uzunluk);
            if (vi && vi->ref_yapi_ir) {
                yk = yapi_bul_ir(g, vi->ref_yapi_ir);
                /* D-308: &Kutu<metin> pointee'si mangled mono tip (%Kutu$ptr) →
                 * yapi_bul_ir bulamaz (mono registry'de). mono_tip_bul + base yapı.
                 * ptr_gep_ir=mangled → GEP/load DOĞRU layout (aksi halde %Kutu base
                 * {i32} ile GEP → pointer i32'ye kırpılırdı, sessiz miscompile). */
                if (!yk) {
                    mono_mt = mono_tip_bul(g, vi->ref_yapi_ir);
                    if (mono_mt && mono_mt->ast->tip == DUGUM_YAPI) {
                        yk = yapi_bul(g, mono_mt->ast->veri.yapi.ad,
                                      mono_mt->ast->veri.yapi.ad_uzunluk);
                        ptr_gep_ir = vi->ref_yapi_ir;
                    }
                }
            }
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
    /* D-307: mangled generic örnek ise alan tipini örneğin subst'ıyla çöz
     * (deger: T → arg IR). Subst hemen geri alınır (extractvalue/GEP etkilenmez). */
    TipSubst *mono_eski = g->substler;
    if (mono_mt) g->substler = mono_mt->subst;
    const char *alan_ir = ast_tip_to_ir(g, alan_tip_d);
    g->substler = mono_eski;

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
    if (ptr_gep_ir) {   /* D-308: mono pointee → mangled tip ile GEP (DOĞRU layout) */
        fprintf(g->out, "  %%%d = getelementptr %s, ptr %%%d, i32 0, i32 %d\n",
                gep_r, ptr_gep_ir, nesne.reg, idx);
    } else {
        fprintf(g->out, "  %%%d = getelementptr %%", gep_r);
        yerel_ad_yaz(g->out, yk->ad, yk->ad_uz);
        fprintf(g->out, ", ptr %%%d, i32 0, i32 %d\n", nesne.reg, idx);
    }
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

    /* Liste<T> BUG-1 + D1 fix: donus tipi T substitusyonu CALL emisyonundan
     * ONCE hesaplanmali (imza-uyumlu IR). Onceki kod yalniz `-> T` (ciplak
     * DUGUM_TIP_BASIT) durumunu yamiyordu; `-> sonuç<T,E>` / `-> seçimlik<T>`
     * gibi BILESIK generic donusler yamasiz kaliyor ve registered donus_tip'in
     * generic-fallback i32'siyle CALL ediliyor, define ise dogru ptr ile
     * emit edildigi icin clang "result type mismatch" verir. Cozum: donus tipi
     * AST'sini, CALL'in tip_args'i gecici subst baglami olarak push edilmis
     * halde ast_tip_to_ir ile yeniden lower et (DUGUM_TIP_SONUC/SECIMLIK/
     * KULLANICI dallari E=ptr'yi gorur). specialize_emit ile ayni push/pop. */
    const char *donus_t = donus;
    if (gislev->veri.islev.donus_tipi && tps > 0) {
        TipSubst *eski_substler = g->substler;
        for (int ti = 0; ti < tps; ti++) {
            TipSubst *s = (TipSubst *)arena_ayir(g->arena, sizeof(TipSubst));
            if (!s) continue;
            s->ad = gislev->veri.islev.tip_paramlar[ti];
            s->ad_uz = (int)strlen(s->ad);
            s->ir = tip_args[ti];
            s->sonraki = g->substler;
            g->substler = s;
        }
        const char *yeniden = ast_tip_to_ir(g, gislev->veri.islev.donus_tipi);
        g->substler = eski_substler;
        if (yeniden) donus_t = yeniden;
    }
    /* D-257: çıplak callee → C-ABI (ρ param YOK). Çağrıda ρ geçme. */
    int callee_rho = !gislev->veri.islev.ciplak_mi;
    if (strcmp(donus_t, "void") == 0) {
        /* donussuz generic (or. buyu<T>) — void call */
        fputs("  call void @", g->out);
        yerel_ad_yaz(g->out, mangled, (int)strlen(mangled));
        fputs("(", g->out);
        if (callee_rho) fprintf(g->out, "ptr %s", g->rho_ref);   /* V2-F4.2a: ρ (kullanıcı-fn) */
        for (int i = 0; i < n; i++) {
            if (i > 0 || callee_rho) fputs(", ", g->out);
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
    if (callee_rho) fprintf(g->out, "ptr %s", g->rho_ref);   /* V2-F4.2a: ρ (kullanıcı-fn) */
    for (int i = 0; i < n; i++) {
        if (i > 0 || callee_rho) fputs(", ", g->out);
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
                /* [KONSOLIDASYON doc17 × F4.2b] D-044 heap-yolu ortak helper'da
                 * (dizi_literal_heap_emit) — çıplak `[..]` literali dizi_* built-in
                 * arg'ı olarak da aynı heap-yoluna girsin (doc17). F4.2b ρ-yönlendirmesi
                 * helper'ın İÇİNDE (bolge_yerel_yonlendir): heap-zorlama "heap olsun" +
                 * confinement "hangi bölge" = KOMPOZE (anlamca dik). Bu beklenen-tip
                 * DIZI yolunda d kesin-yerel ise ρ_yerel, değilse ρ_caller — eski
                 * inline davranışla IR birebir aynı. */
                return dizi_literal_heap_emit(g, d, NULL,
                    g->beklenen_tip->veri.tip_dizi.eleman_tip);
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
                    /* D-087: struct eleman → by-value kdl_dizi_al_yapi. */
                    if (dizi_eleman_struct_mi(et)) {
                        return dizi_struct_al_emit(g, v_load, idx_r, et);
                    }
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
            /* D-085 [YÜKSEK]: TÜRETİLMİŞ heap dizi tabanı (yapı alanı k.xs /
             * işlev dönüşü yap()) → kdl_dizi_al route et. Descriptor'ı (KdlDizi*)
             * düz veri gibi GEP'leme (eski yol sessiz-yanlış/segfault'tu).
             * Skaler/ptr + struct eleman (D-087 by-value yapı). */
            if (d->veri.indeks.nesne &&
                d->veri.indeks.nesne->tip != DUGUM_TANIMLAYICI) {
                const Dugum *elem_ast = heap_dizi_eleman_ast(g,
                    d->veri.indeks.nesne);
                if (elem_ast) {
                    const char *et = ast_tip_to_ir(g, elem_ast);
                    if (!et) et = "i32";
                    IfadeSonuc base = ifade_uret(g, d->veri.indeks.nesne, NULL);
                    IfadeSonuc idx = ifade_uret(g, d->veri.indeks.indeks, "i32");
                    int idx_r = int_donustur(g, idx.reg, idx.tip, "i32");
                    if (dizi_eleman_struct_mi(et)) {
                        return dizi_struct_al_emit(g, base.reg, idx_r, et);
                    }
                    const char *rt = kdl_al_donus_ir(et);
                    int rr = yeni_reg(g);
                    fprintf(g->out,
                        "  %%%d = call %s @%s(ptr %%%d, i32 %%%d)\n",
                        rr, rt, kdl_al_fn(et), base.reg, idx_r);
                    IfadeSonuc s = { rr, rt, 0 };
                    return s;
                }
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
                /* D-265: yük tipi POINTEE'den (operand *tamN → tamN). beklenen
                 * VERİLMEZSE (ör. `*bp == 0` karşılaştırma bağlamı) eski i32
                 * varsayılanı DAR tiplerde YANLIŞ genişlik yüklüyordu (*tam8 → i32,
                 * 4 bayt oku → null-check bozulur; D-264 metin garbling). indeks +
                 * deref-write handler'ları zaten pointee_llvm_tip kullanıyordu; bu
                 * deref-READ gap'iydi. beklenen VERİLİRSE korunur (çağıran o tipi bekler). */
                const char *deref_pointee = NULL;
                int deref_isz = 0;
                if (d->veri.tekli.operand->tip == DUGUM_TANIMLAYICI) {
                    LlvmIsim *dvi = isim_bul(g,
                        d->veri.tekli.operand->veri.tanimlayici.metin,
                        d->veri.tekli.operand->veri.tanimlayici.uzunluk);
                    if (dvi && dvi->pointee_llvm_tip) {
                        deref_pointee = dvi->pointee_llvm_tip;
                        deref_isz = dvi->pointee_isaretsiz;
                    }
                }
                /* D-347 ONARIM — ONCELIK TERS CEVRILDI. Eskiden `beklenen`
                 * pointee'ye TERCIH ediliyordu ("cagiran o tipi bekler").
                 * Bu iki AYRI seyi karistiriyordu:
                 *   • pointee tipi = BELLEKTEKI nesnenin GERCEK genisligi,
                 *   • beklenen     = cagiranin istedigi DEGER tipi.
                 * Sonuc: `değişken p: *dtam8; ver (*p) olarak tam64;`
                 * `load i64` uretiyordu — 1 baytlik nesneden 8 BAYT okuma
                 * (sinir-disi okuma + cop deger). Ne hata ne uyari; SESSIZ
                 * YANLIS CEVAP. Gercek etkisi olculdu: DTB magic dogrulamasi
                 * 0x4c004500450044 gibi absurd bir deger gorup basarisiz oldu.
                 * Ayni ders `pointee_llvm_tip` yorumunda indeks oku/yaz icin
                 * ZATEN yaziliydi; deref-READ yolunda uygulanmamisti.
                 * Dogrusu: pointee genisliginde YUKLE, sonra beklenen'e
                 * DONUSTUR (dtamN -> zext, tamN -> sext). Pointee bilinmiyorsa
                 * eski davranis korunur (baska bilgi yok). */
                const char *yuk_tip =
                    deref_pointee ? deref_pointee
                        : ((beklenen && strcmp(beklenen, "ptr") != 0) ? beklenen
                                                                     : "i32");
                int r = yeni_reg(g);
                /* D-248 (GAP-3): güvensiz blokta VOLATILE load (MMIO okuması
                 * clang -O2 tarafından elenmez/yeniden-sıralanmaz). */
                const char *vol = (g->guvensiz_derinlik > 0) ? "volatile " : "";
                fprintf(g->out, "  %%%d = load %s%s, ptr %%%d\n",
                        r, vol, yuk_tip, p.reg);
                /* Yuklenen GERCEK genislikten beklenen tipe donustur.
                 * int_donustur_im tipler esitse no-op'tur (reg aynen doner). */
                if (beklenen && strcmp(beklenen, "ptr") != 0 &&
                    strcmp(beklenen, yuk_tip) != 0 &&
                    yuk_tip[0] == 'i' && beklenen[0] == 'i') {
                    int rc = int_donustur_im(g, r, yuk_tip, beklenen, deref_isz);
                    IfadeSonuc sc = { rc, beklenen, deref_isz };
                    return sc;
                }
                IfadeSonuc s = { r, yuk_tip, deref_isz };
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
                /* D-334: KAPANIS ALANI mi, METOD mu? `k.fn()` iki sekli de
                 * gosterebilir. Once alicinin yapisina bakip alanin BILDIRILEN
                 * tipi `işlev(...) -> T` mi diye olc; oyleyse bu bir metod
                 * DEGIL, fat-value tutan ALANDIR → dolayli cagri.
                 * Onceki davranis: her ERISIM metod sayiliyordu → alan adiyla
                 * `call i32 @fn(...)` uretilip TANIMSIZ SEMBOL veriyordu (olculdu). */
                {
                    IfadeSonuc alici_on = ifade_uret(g,
                        erisim->veri.erisim.nesne, NULL);
                    YapiKayit *ryk = yapi_bul_ir(g, alici_on.tip);
                    const Dugum *alan_td = yapi_alan_tip_dugumu(ryk,
                        erisim->veri.erisim.alan, erisim->veri.erisim.alan_uzunluk);
                    const char *kap_donus = kapanis_donus_ir_al(g, alan_td);
                    if (kap_donus) {
                        /* Alan gercekten kapanis: fat degeri OKU (ERISIM'i
                         * yeniden uretmek yerine alici degerinden extractvalue —
                         * alici zaten hesaplandi, yan etki tekrarlanmaz). */
                        int alan_ix = yapi_alan_indeksi(ryk,
                            erisim->veri.erisim.alan,
                            erisim->veri.erisim.alan_uzunluk, NULL);
                        if (alan_ix >= 0) {
                            int fv = yeni_reg(g);
                            fprintf(g->out,
                                "  %%%d = extractvalue %s %%%d, %d\n",
                                fv, alici_on.tip, alici_on.reg, alan_ix);
                            int n2 = d->veri.cagri.sayi;
                            IfadeSonuc *ia = (IfadeSonuc *)arena_ayir(g->arena,
                                sizeof(IfadeSonuc) * (size_t)(n2 > 0 ? n2 : 1));
                            for (int i = 0; i < n2; i++)
                                ia[i] = ifade_uret(g, d->veri.cagri.argumanlar[i], NULL);
                            return fat_cagri_uret(g, fv, kap_donus, ia, n2);
                        }
                    }
                }
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
                /* D-257: çıplak method → C-ABI (ρ YOK). */
                int m_rho = !(mik && mik->ast && mik->ast->veri.islev.ciplak_mi);
                int r = yeni_reg(g);
                fprintf(g->out, "  %%%d = call %s @", r, donus);
                yerel_ad_yaz(g->out, m_ad, m_ad_uz);
                fputs("(", g->out);
                if (m_rho) fprintf(g->out, "ptr %s", g->rho_ref);   /* V2-F4.2a: ρ (metot=kullanıcı-fn) */
                for (int i = 0; i < n + 1; i++) {
                    if (i > 0 || m_rho) fputs(", ", g->out);
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
                    /* D-257: çıplak modül-fn → C-ABI (ρ YOK). */
                    int mf_rho = !(mik->ast && mik->ast->veri.islev.ciplak_mi);
                    yerel_ad_yaz(g->out, mik->ad, mik->ad_uz);
                    fputs("(", g->out);
                    if (mf_rho) fprintf(g->out, "ptr %s", g->rho_ref);   /* V2-F4.2a: ρ (modül-fn) */
                    for (int i = 0; i < n; i++) {
                        if (i > 0 || mf_rho) fputs(", ", g->out);
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
            /* D-334: DIZI ELEMANI kapanis cagrisi — `xs[i]()`.
             * Hedefi uret; IR tipi fat value ise dolayli cagir. Dizi elemani
             * by-value 16 bayt olarak saklanir (dizi_eleman_struct_mi).
             * Donus IR'i: dizinin BILDIRILEN eleman tipinden (cg kaydi) degil,
             * beklenen'den cikar — dizi eleman tipi `{ ptr, ptr }` T'yi siler;
             * beklenen yoksa i32 (mevcut annotasyonsuz-kapanis siniri). */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_INDEKS) {
                IfadeSonuc hedef = ifade_uret(g, d->veri.cagri.hedef, NULL);
                if (hedef.tip && strcmp(hedef.tip, "{ ptr, ptr }") == 0) {
                    int n2 = d->veri.cagri.sayi;
                    IfadeSonuc *ia = (IfadeSonuc *)arena_ayir(g->arena,
                        sizeof(IfadeSonuc) * (size_t)(n2 > 0 ? n2 : 1));
                    for (int i = 0; i < n2; i++)
                        ia[i] = ifade_uret(g, d->veri.cagri.argumanlar[i], NULL);
                    const char *dn = (beklenen && *beklenen) ? beklenen : "i32";
                    return fat_cagri_uret(g, hedef.reg, dn, ia, n2);
                }
                return hata(g, "dizi elemani cagrilabilir degil (kapanis bekleniyor)");
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
                    /* D-268 OUT-PTR ABI: %kdl_yetki dönüşü AÇIK out-pointer ile
                     * (düz `ptr` ilk arg, aarch64 x0 — sret DEĞİL/x8 DEĞİL). Çağıran
                     * slot ayırır, sağlayıcı struct'ı slot'a yazar, çağıran geri yükler.
                     * Böylece .kem sağlayıcı (`çıplak fn(out: *KdlYetki,...)`) call-site
                     * ile BİREBİR eşleşir (Yasa-4: yetki saf-.kem'e göç edebilir).
                     * ESKİ sret formu clang C provider'ın register-return'ü (16B ≤ eşik)
                     * ile bare-metal'de zaten uyuşmuyordu (yetki değeri kullanılmadığı
                     * için maskeliydi). Host Win64: düz ptr = RCX = sret ile aynı reg. */
                    int sret = yeni_reg(g);
                    fprintf(g->out, "  %%%d = alloca %%kdl_yetki\n", sret);
                    fprintf(g->out,
                        "  call void @kdl_yetki_olustur("
                        "ptr %%%d, "
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
                    /* D-268 OUT-PTR ABI: dönüş out-ptr (düz `ptr`, x0) + yetki
                     * argümanı da pointer (temp alloca'ya yaz, adresi geç). Böylece
                     * .kem sağlayıcı `çıplak fn(out: *KdlYetki, y: *KdlYetki, izin)`
                     * ile eşleşir. (Eski sret formu bare-metal register-return C
                     * provider ile uyuşmuyordu — bkz. olustur açıklaması.) */
                    int y_slot = yeni_reg(g);
                    fprintf(g->out, "  %%%d = alloca %%kdl_yetki\n", y_slot);
                    fprintf(g->out,
                        "  store %%kdl_yetki %%%d, ptr %%%d\n",
                        arg_y.reg, y_slot);
                    int sret = yeni_reg(g);
                    fprintf(g->out, "  %%%d = alloca %%kdl_yetki\n", sret);
                    fprintf(g->out,
                        "  call void @kdl_yetki_delege("
                        "ptr %%%d, "
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
            /* [F-dizi-arg] Çıplak `[..]` literali doğrudan bir dizi_* built-in'e
             * argüman olunca HEAP zorlamak için (args döngüsünde kullanılır):
             *   dizi_built_in      — eleman-değeri arg'ı olan (ekle/al/yaz)
             *   dizi_desc_built_in — arg0'ı KdlDizi* descriptor olan tüm dizi_*
             *   dizi_al_mi         — dizi_al (arg0 eleman tipi = dönüş beklenen)
             *   dizi_deger_eleman_ast — iç-içe Dizi<Dizi<T>>'de değer-arg'ın
             *                            tam AST tipi (Dizi<T>); nested literal
             *                            heap çözümü için. */
            int dizi_built_in = 0;
            int dizi_desc_built_in = 0;
            int dizi_al_mi = 0;
            const Dugum *dizi_deger_eleman_ast = NULL;
            {
                const char *adi = d->veri.cagri.hedef
                    ? d->veri.cagri.hedef->veri.tanimlayici.metin : NULL;
                int adi_uz = d->veri.cagri.hedef
                    ? d->veri.cagri.hedef->veri.tanimlayici.uzunluk : 0;
                dizi_built_in =
                    (adi_uz == 9 && memcmp(adi, "dizi_ekle", 9) == 0) ||
                    (adi_uz == 7 && memcmp(adi, "dizi_al", 7) == 0) ||
                    (adi_uz == 8 && memcmp(adi, "dizi_yaz", 8) == 0);
                dizi_al_mi = (adi_uz == 7 && memcmp(adi, "dizi_al", 7) == 0);
                /* arg0 = descriptor olan tüm dizi_* (dizi_olustur HARİÇ — onun
                 * arg0'ı eleman sayısı N, descriptor değil). */
                dizi_desc_built_in = dizi_built_in ||
                    (adi_uz == 10 && memcmp(adi, "dizi_boyut", 10) == 0) ||
                    (adi_uz == 13 && memcmp(adi, "dizi_kapasite", 13) == 0) ||
                    (adi_uz == 20 &&
                     memcmp(adi, "dizi_kapasite_ayarla", 20) == 0);
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
                        if (vi) dizi_deger_eleman_ast = vi->eleman_tip_ast;
                    } else if (arg0 && arg0->tip == DUGUM_ERISIM) {
                        /* D-029 fix (2): dizi struct-alani (s.ad) -> alan
                         * tipinden eleman IR cikar (yoksa metin ptr i32
                         * okunup SEGFAULT). */
                        const char *et = dizi_alan_eleman_ir(g, arg0);
                        if (et) dizi_eleman_beklenen = et;
                        dizi_deger_eleman_ast = dizi_alan_eleman_ast(g, arg0);
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
                    /* D-293: dönüş tipini önce BİLDİRİLEN closure tipinden al
                     * (`işlev(...) -> T`). Fat value ("{ ptr, ptr }") T'yi
                     * sildiği için burası eskiden `beklenen ? beklenen : "i32"`
                     * TAHMİN ediyordu. Lambda dönüşü sabit i32 iken bu (yanlış
                     * ama) tutarlıydı; dönüş gövdeden çıkarsanınca tahmin kırıldı:
                     * beklenen'in yayılmadığı bağlamlarda (örn. `metin_uzunluk(f())`
                     * — built-in argümanı) ptr dönen lambda i32 olarak çağrılıp
                     * SEGFAULT üretiyordu. Bildirilen tip otoriter → önce o.
                     *
                     * D-295 (BLOKER onarımı): son çare "i32" DEĞİL, "i64".
                     * D-293 yalnız BİLDİRİLEN tipi kurtardı; ANNOTASYONSUZ
                     * closure (`değişken f = || "selam";`) hâlâ i32'ye düşüyor
                     * ve ptr dönen lambda i32 olarak çağrılıp SEGFAULT veriyordu
                     * (ölçüldü). i64 son çare, hedeflerimizde (x86_64/aarch64)
                     * işaretçi ve tamsayı dönüşünün AYNI yazmaçta (rax/x0)
                     * gelmesinden yararlanır → kırpma yok.
                     * KALAN AÇIK (dürüstçe): gövdesi kesirli dönen ANNOTASYONSUZ
                     * closure, beklenen de yoksa hâlâ yanlış yazmaçtan okur
                     * (double v0/xmm0'da). Bu vaka bugün de aşağı akışta LLVM
                     * tarafından GÜRÜLTÜLÜ reddediliyor (ölçüldü) — i64 onu
                     * sessizleştirmiyor; tam çözüm lambda dönüşünün çağrı
                     * yerinden önce bilinmesini ister (ayrı iş). */
                    const char *donus_indirect =
                        (vi && vi->kapanis_donus_ir) ? vi->kapanis_donus_ir
                        : (beklenen ? beklenen : "i64");
                    /* === V2-F1 fat-value dispatch ===
                     * vi fat değer { ptr fn, ptr env } tutar. env==null → bare
                     * fn(args); env!=null → closure fn(env,args). "Closure mu"
                     * runtime'da (env-null) belirlenir → derleme-zamanı closure_mu
                     * tag'i KALKTI: kaçışta kaybolmaz, D-071 (bare-ptr'ı closure
                     * sanma) tuzağı yapısal olarak imkânsız. phi yerine slot deseni. */
                    int fv = yeni_reg(g);
                    fprintf(g->out,
                        "  %%%d = load { ptr, ptr }, ptr %%%d\n", fv, vi->reg_no);
                    int fn_reg = yeni_reg(g);
                    fprintf(g->out,
                        "  %%%d = extractvalue { ptr, ptr } %%%d, 0\n", fn_reg, fv);
                    int env_reg = yeni_reg(g);
                    fprintf(g->out,
                        "  %%%d = extractvalue { ptr, ptr } %%%d, 1\n", env_reg, fv);
                    int slot = yeni_reg(g);
                    fprintf(g->out, "  %%%d = alloca %s\n", slot, donus_indirect);
                    int isnull = yeni_reg(g);
                    fprintf(g->out,
                        "  %%%d = icmp eq ptr %%%d, null\n", isnull, env_reg);
                    int L_bare = yeni_label(g);
                    int L_clo = yeni_label(g);
                    int L_join = yeni_label(g);
                    fprintf(g->out,
                        "  br i1 %%%d, label %%bb%d, label %%bb%d\n",
                        isnull, L_bare, L_clo);
                    /* bare: env yok → ρ-ABI imza fn(ρ, args). V2-F4.2a: hedef
                     * üst-düzey-fn (ρ-ABI) ya da yakalamasız-lambda (ρ-ABI) →
                     * ikisi de ρ ilk param. ρ = çağıranın ρ_ref'i. */
                    fprintf(g->out, "bb%d:\n", L_bare);
                    int rb = yeni_reg(g);
                    fprintf(g->out, "  %%%d = call %s %%%d(ptr %s",
                            rb, donus_indirect, fn_reg, g->rho_ref);
                    for (int i = 0; i < n; i++)
                        fprintf(g->out, ", %s %%%d", iargs[i].tip, iargs[i].reg);
                    fputs(")\n", g->out);
                    fprintf(g->out, "  store %s %%%d, ptr %%%d\n",
                            donus_indirect, rb, slot);
                    fprintf(g->out, "  br label %%bb%d\n", L_join);
                    /* closure: ρ + env → fn(ρ, env, args) */
                    fprintf(g->out, "bb%d:\n", L_clo);
                    int rc = yeni_reg(g);
                    fprintf(g->out, "  %%%d = call %s %%%d(ptr %s, ptr %%%d",
                            rc, donus_indirect, fn_reg, g->rho_ref, env_reg);
                    for (int i = 0; i < n; i++)
                        fprintf(g->out, ", %s %%%d", iargs[i].tip, iargs[i].reg);
                    fputs(")\n", g->out);
                    fprintf(g->out, "  store %s %%%d, ptr %%%d\n",
                            donus_indirect, rc, slot);
                    fprintf(g->out, "  br label %%bb%d\n", L_join);
                    /* join */
                    fprintf(g->out, "bb%d:\n", L_join);
                    int rr = yeni_reg(g);
                    fprintf(g->out, "  %%%d = load %s, ptr %%%d\n",
                            rr, donus_indirect, slot);
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

            /* === Katman 2 (Concurrency / DRF V1) intrinsic emisyonu ===
             * Bu üçü aşağıdaki generic built-in şeridine SIĞMAZ: şerit yalnız
             * ad→kdl_ad yeniden-eşlemesi yapar ve KEMGU argümanlarını birebir
             * IR argümanına çevirir. Oysa görev_başlat'ın TEK argümanı bir fat
             * value'dur ve ÜÇ IR argümanına açılır; dondur ise hiç talimat
             * üretmez. Bu yüzden şeritten ÖNCE, kendi emisyonuyla erken döner.
             *
             * `!ik` (kullanıcı işlevi çözülmediyse) koşulu şeritteki D2/bug#3
             * dersinin aynısı: kullanıcı aynı adda bir işlev tanımlarsa çağrı
             * intrinsic'e KAÇIRILMAMALI. */
            if (!ik && n == 1 && cagri_adi_uz == 14 &&
                memcmp(cagri_adi, "g\xc3\xb6rev_ba\xc5\x9f" "lat", 14) == 0) {
                /* görev_başlat(c: işlev() -> T) -> görev<T>
                 * c = { ptr fn, ptr env } (V2-F1 fat value). Runtime ρ-ABI
                 * dispatch'ini env-null'a bakarak yapar; fn AYNI değer olarak
                 * her iki tipli parametreye de geçilir — böylece C tarafında
                 * fn-ptr cast'i (dolayısıyla -Wcast-function-type / -Wpedantic
                 * uyarısı) gerekmez. Bkz. kdl_runtime.c KdlGorevBare notu. */
                IfadeSonuc c = ifade_uret(g, d->veri.cagri.argumanlar[0],
                                          "{ ptr, ptr }");
                int fnr = yeni_reg(g);
                fprintf(g->out,
                    "  %%%d = extractvalue { ptr, ptr } %%%d, 0\n", fnr, c.reg);
                int envr = yeni_reg(g);
                fprintf(g->out,
                    "  %%%d = extractvalue { ptr, ptr } %%%d, 1\n", envr, c.reg);
                /* D-309: ρ_sahip POZİTİF hapsedilme kanıtı — kanıtlanırsa
                 * runtime join'de ρ_sahip'i serbest bırakır; kanıtlanamazsa
                 * (0) eski davranış: sızdır. Kanıt gövde AST'sinden üretilir
                 * (gorev_rho_confined); escape DFA'ya GÜVENMEZ. */
                int rho_ok = gorev_rho_confined(g, d->veri.cagri.argumanlar[0]);
                int r = yeni_reg(g);
                fprintf(g->out,
                    "  %%%d = call ptr @kdl_gorev_basla_kapanis"
                    "(ptr %%%d, ptr %%%d, ptr %%%d, i32 %d)\n",
                    r, fnr, fnr, envr, rho_ok);
                /* Karar 1 (D-30x): görev_başlat → sonuç<görev<T>, metin>.
                 * görev<T> ve metin İKİSİ de IR'de ptr → aggregate T'den BAĞIMSIZ
                 * olarak DAİMA { i8, ptr, ptr }. Sarma DALLANMASIZ:
                 *   tag = (handle == null) ? 1(hata) : 0(tamam)
                 * ve üç alan da koşulsuz doldurulur — okuyucu (eşleş) tag'e göre
                 * alan 1'i (görev handle) ya da alan 2'yi (hata metni) okur,
                 * diğerini yok sayar. Başarısızlıkta handle zaten null'dır ama
                 * tag=1 olduğundan okunmaz; başarıda hata metni yok sayılır.
                 * Runtime spawn başarısızsa NULL döndürür (panik YOK, D-296 çözümü). */
                int r_isnull = yeni_reg(g);
                fprintf(g->out, "  %%%d = icmp eq ptr %%%d, null\n", r_isnull, r);
                int r_tag = yeni_reg(g);
                fprintf(g->out, "  %%%d = zext i1 %%%d to i8\n", r_tag, r_isnull);
                int r_slot = yeni_reg(g);
                fprintf(g->out, "  %%%d = alloca { i8, ptr, ptr }\n", r_slot);
                int r_g0 = yeni_reg(g);
                fprintf(g->out,
                    "  %%%d = getelementptr { i8, ptr, ptr }, ptr %%%d, i32 0, i32 0\n",
                    r_g0, r_slot);
                fprintf(g->out, "  store i8 %%%d, ptr %%%d\n", r_tag, r_g0);
                int r_g1 = yeni_reg(g);
                fprintf(g->out,
                    "  %%%d = getelementptr { i8, ptr, ptr }, ptr %%%d, i32 0, i32 1\n",
                    r_g1, r_slot);
                fprintf(g->out, "  store ptr %%%d, ptr %%%d\n", r, r_g1);
                int r_emsg = yeni_reg(g);
                fprintf(g->out,
                    "  %%%d = getelementptr [20 x i8], ptr @.gorev_hata_str, i32 0, i32 0\n",
                    r_emsg);
                int r_g2 = yeni_reg(g);
                fprintf(g->out,
                    "  %%%d = getelementptr { i8, ptr, ptr }, ptr %%%d, i32 0, i32 2\n",
                    r_g2, r_slot);
                fprintf(g->out, "  store ptr %%%d, ptr %%%d\n", r_emsg, r_g2);
                int r_load = yeni_reg(g);
                fprintf(g->out, "  %%%d = load { i8, ptr, ptr }, ptr %%%d\n",
                    r_load, r_slot);
                IfadeSonuc s = { r_load, "{ i8, ptr, ptr }", 0 };
                return s;
            }
            if (!ik && n == 1 && cagri_adi_uz == 17 &&
                memcmp(cagri_adi, "g\xc3\xb6rev_birle\xc5\x9f" "tir", 17) == 0) {
                /* görev_birleştir(g: görev<T>) -> T  (R-BİRLEŞTİR: join)
                 * D-294: runtime i64 taşır (metin/&T gibi işaretçi T'ler için;
                 * i32 taşıma işaretçiyi kırpardı). Sonuç burada T'ye daraltılır.
                 * T'nin IR'i BİLDİRİLEN `görev<T>`den gelir (gorev_ic_ir) —
                 * görev<T> IR'de opak `ptr` olduğu için T başka türlü bilinemez.
                 * Annotasyonsuz görev → gorev_ic_ir NULL → beklenen'e düşülür.
                 * KESİRLİ T tip kontrolünde reddedilir (wrapper x0/rax okur,
                 * float v0/xmm0'dadır → bitcast SESSİZ çöp olurdu). */
                IfadeSonuc gv = ifade_uret(g, d->veri.cagri.argumanlar[0],
                                           "ptr");
                LlvmIsim *gi = NULL;
                if (d->veri.cagri.argumanlar[0]->tip == DUGUM_TANIMLAYICI) {
                    gi = isim_bul(g,
                        d->veri.cagri.argumanlar[0]->veri.tanimlayici.metin,
                        d->veri.cagri.argumanlar[0]->veri.tanimlayici.uzunluk);
                }
                /* D-295 (BLOKER onarımı): son çare "i32" DEĞİL, "i64".
                 * Eski zincir `... : (beklenen ? beklenen : "i32")` idi ve
                 * annotasyonsuz görev + beklenen-yok durumunda i64 runtime
                 * sonucunu SESSİZCE kırpıyordu:
                 *   `değişken c = görev_başlat(|| buyuk_tam64);`
                 *   `değişken s = görev_birleştir(c);`   -> trunc i64->i32
                 * --check OK, LLVM OK, program YANLIŞ cevap veriyordu (ölçüldü:
                 * 2^32 için exit 99, doğrusu 1); T=metin'de segfault. Yani D-291
                 * öncesindeki GÜRÜLTÜLÜ hatayı (tanımsız sembol) SESSİZ yanlışa
                 * çeviriyordu — projenin loud>silent ilkesinin ihlali.
                 * i64 son çare taşıyıcının tam genişliğidir: kırpma YOK. Gerçek T
                 * dar bir tamsayıysa daraltmayı aşağı akıştaki int_donustur
                 * beklenen tiple yapar; T işaretçi ise i64'ü ptr yerine kullanmak
                 * LLVM'de tip hatasıdır (GÜRÜLTÜLÜ) — sessiz bozulma değil. */
                const char *t_ir = (gi && gi->gorev_ic_ir) ? gi->gorev_ic_ir
                                 : (beklenen ? beklenen : "i64");
                int r64 = yeni_reg(g);
                fprintf(g->out,
                    "  %%%d = call i64 @kdl_gorev_birlestir(ptr %%%d)\n",
                    r64, gv.reg);
                if (strcmp(t_ir, "i64") == 0) {
                    IfadeSonuc s = { r64, "i64", 0 };
                    return s;
                }
                int r = yeni_reg(g);
                if (strcmp(t_ir, "ptr") == 0) {
                    fprintf(g->out, "  %%%d = inttoptr i64 %%%d to ptr\n",
                            r, r64);
                } else if (strcmp(t_ir, "void") == 0) {
                    /* görev<boş>: sonuç yok — çağıran yine de bir IfadeSonuc
                     * bekler (built-in void-call desenindeki placeholder). */
                    fprintf(g->out, "  %%%d = add i32 0, 0\n", r);
                    IfadeSonuc s = { r, "i32", 0 };
                    return s;
                } else {
                    /* i1/i8/i16/i32 → trunc (i32 dönen lambda'nın üst 32 biti
                     * ÇÖP olduğu için bu daraltma ŞART, kozmetik değil). */
                    fprintf(g->out, "  %%%d = trunc i64 %%%d to %s\n",
                            r, r64, t_ir);
                }
                IfadeSonuc s = { r, t_ir, 0 };
                return s;
            }
            if (!ik && n == 1 && cagri_adi_uz == 14 &&
                memcmp(cagri_adi, "kanal_olu\xc5\x9ftur", 14) == 0) {
                /* kanal_oluştur(kapasite: tam32) -> kanal<T>  (IR: ptr)
                 * T yalnız tip-kontrolde yaşar; runtime kanal monomorfik i32
                 * taşır (bkz. görev_birleştir'deki T=tam32 notu — aynı sınır). */
                IfadeSonuc kap = ifade_uret(g, d->veri.cagri.argumanlar[0],
                                            "i32");
                int r = yeni_reg(g);
                fprintf(g->out,
                    "  %%%d = call ptr @kdl_kanal_olustur(i32 %%%d)\n",
                    r, kap.reg);
                IfadeSonuc s = { r, "ptr", 0 };
                return s;
            }
            if (!ik && n == 2 && cagri_adi_uz == 13 &&
                memcmp(cagri_adi, "kanal_g\xc3\xb6nder", 13) == 0) {
                /* kanal_gönder(k: kanal<T>, v: T) -> boş  (R-KANAL gönderim)
                 * Runtime kanal DOLUYSA BLOKLAR (akış denetimi).
                 *
                 * D-295: değer i32'ye ZORLANMIYOR, T'nin doğal tipiyle üretilip
                 * i64'e genişletiliyor. Eskiden `ifade_uret(v, "i32")` idi ve
                 * `kanal<tam64>`de değeri SESSİZCE kırpıyordu (ölçüldü: 2^33
                 * gönderilip alındığında eşit çıkmıyordu). `--check` bunu DRF006
                 * ile reddediyordu ama `--llvm` tip kontrolünü ÇALIŞTIRMAZ →
                 * o yol sessiz veri kaybına açıktı. Artık taşıyıcı i64. */
                IfadeSonuc kv = ifade_uret(g, d->veri.cagri.argumanlar[0],
                                           "ptr");
                LlvmIsim *ki = NULL;
                if (d->veri.cagri.argumanlar[0]->tip == DUGUM_TANIMLAYICI) {
                    ki = isim_bul(g,
                        d->veri.cagri.argumanlar[0]->veri.tanimlayici.metin,
                        d->veri.cagri.argumanlar[0]->veri.tanimlayici.uzunluk);
                }
                const char *kt = (ki && ki->kanal_ic_ir) ? ki->kanal_ic_ir
                                                         : "i64";
                IfadeSonuc vv = ifade_uret(g, d->veri.cagri.argumanlar[1], kt);
                int v64 = vv.reg;
                if (strcmp(vv.tip, "i64") != 0) {
                    v64 = yeni_reg(g);
                    if (strcmp(vv.tip, "ptr") == 0) {
                        fprintf(g->out, "  %%%d = ptrtoint ptr %%%d to i64\n",
                                v64, vv.reg);
                    } else {
                        /* i1/i8/i16/i32 → sext (KEMGU tamsayıları işaretli;
                         * dtamN işaretsiz yolu alım tarafında trunc ile geri
                         * daraltıldığı için bit deseni korunur). */
                        fprintf(g->out, "  %%%d = sext %s %%%d to i64\n",
                                v64, vv.tip, vv.reg);
                    }
                }
                fprintf(g->out,
                    "  call void @kdl_kanal_gonder(ptr %%%d, i64 %%%d)\n",
                    kv.reg, v64);
                /* Çağıran bir IfadeSonuc bekliyor — void yerine placeholder
                 * (built-in şeridindeki void-call deseninin aynısı). */
                int r = yeni_reg(g);
                fprintf(g->out, "  %%%d = add i32 0, 0\n", r);
                IfadeSonuc s = { r, "i32", 0 };
                return s;
            }
            if (!ik && n == 1 && cagri_adi_uz == 8 &&
                memcmp(cagri_adi, "kanal_al", 8) == 0) {
                /* kanal_al(k: kanal<T>) -> T  (R-KANAL alım)
                 * Runtime kanal BOŞSA BLOKLAR — böylece "henüz gönderilmedi"
                 * ile "0 gönderildi" karışmaz (eski non-blocking sürümün
                 * sessiz-yanlış-cevabı). */
                /* D-295: runtime i64 taşır; sonuç T'ye daraltılır
                 * (görev_birleştir ile birebir aynı desen). T bildirilen
                 * `kanal<T>`den gelir; kurtarılamazsa i64 (kırpma YOK). */
                IfadeSonuc kv = ifade_uret(g, d->veri.cagri.argumanlar[0],
                                           "ptr");
                LlvmIsim *ki = NULL;
                if (d->veri.cagri.argumanlar[0]->tip == DUGUM_TANIMLAYICI) {
                    ki = isim_bul(g,
                        d->veri.cagri.argumanlar[0]->veri.tanimlayici.metin,
                        d->veri.cagri.argumanlar[0]->veri.tanimlayici.uzunluk);
                }
                const char *kt = (ki && ki->kanal_ic_ir) ? ki->kanal_ic_ir
                               : (beklenen ? beklenen : "i64");
                int a64 = yeni_reg(g);
                fprintf(g->out,
                    "  %%%d = call i64 @kdl_kanal_al(ptr %%%d)\n", a64, kv.reg);
                if (strcmp(kt, "i64") == 0) {
                    IfadeSonuc s = { a64, "i64", 0 };
                    return s;
                }
                int r = yeni_reg(g);
                if (strcmp(kt, "ptr") == 0) {
                    fprintf(g->out, "  %%%d = inttoptr i64 %%%d to ptr\n",
                            r, a64);
                } else {
                    fprintf(g->out, "  %%%d = trunc i64 %%%d to %s\n",
                            r, a64, kt);
                }
                IfadeSonuc s = { r, kt, 0 };
                return s;
            }
            if (!ik && n == 1 && cagri_adi_uz == 6 &&
                memcmp(cagri_adi, "dondur", 6) == 0) {
                /* dondur(v: &değişken T) -> &T  (R-PAYLAŞ)
                 * V1'de bu YALNIZ bir tip-seviyesi işlemi: mutable referansı
                 * immutable'a daraltır. Runtime temsili aynı ptr → identity,
                 * sıfır talimat. Gerçek frozen-flag zorlaması V2 (tip_kontrol.c
                 * DRF005 notu). Bu dal olmadan `call ptr @dondur(...)` üretilip
                 * TANIMSIZ SEMBOL link hatası veriyordu. */
                return ifade_uret(g, d->veri.cagri.argumanlar[0], "ptr");
            }
            if (!ik && n == 1 &&
                ((cagri_adi_uz == 9 &&
                  memcmp(cagri_adi, "g\xc3\xb6nderen", 9) == 0) ||
                 (cagri_adi_uz == 4 &&
                  memcmp(cagri_adi, "alan", 4) == 0))) {
                /* D-303: gönderen(k)/alan(k) — kanal yön ucu projeksiyonu.
                 * Runtime-free IDENTITY (dondur gibi): gönderen<T>/alan<T>
                 * uçları aynı KdlKanal* ptr'ına type-level görünümdür; codegen
                 * hiç talimat üretmez, argümanı aynen döndürür. Yön güvenliği
                 * tamamen tip kontrolünde (DRF007). `!ik` guard: kullanıcının
                 * `alan` adlı işlevi varsa ONA düşer (isim gaspı yok). */
                return ifade_uret(g, d->veri.cagri.argumanlar[0], "ptr");
            }

            /* Built-in libc / kdl mapping */
            const char *kdl_donus = NULL;  /* override (NULL ise auto) */
            /* src-bugfix'ten: param_beklenen + builtin_donus (genis tasarim) */
            const char *param_beklenen[8] = { NULL, NULL, NULL, NULL,
                                              NULL, NULL, NULL, NULL };
            const char *builtin_donus = NULL;
            (void)param_beklenen; (void)builtin_donus;
            /* D2 (bug #3): builtin ad-eslestirme zinciri yalniz KULLANICI islevi
             * COZULMEDIYSE (ik==NULL) calismali. Aksi halde kullanici-tanimli
             * `yazdir`/`yaz_*`/`metin_*`/`dizi_*` islevleri builtin'e kacirilir
             * (hijack). `!ik` zincirin BASINDAKI `if`e konur — sonraki `else if`
             * dallari otomatik atlanir, akis alttaki generic/normal kullanici-fn
             * emisyonuna duser. */
            if (!ik && cagri_adi_uz == 6 && memcmp(cagri_adi, "yazdir", 6) == 0) {
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
                    /* [F-dizi-arg] Çıplak `[..]` literali doğrudan bir dizi_*
                     * built-in'e descriptor (arg0) ya da iç-içe değer argümanı
                     * olarak geçiyorsa HEAP KdlDizi* zorla — aksi halde stack
                     * [N×T] alloca runtime'da KdlDizi* sanılıp stack çöpü
                     * okunur (ASan misaligned / access-violation; repro:
                     * dizi_al([5,6,7],1) ve dizi_ekle(dis,[40,50,60])). Yalnız
                     * built-in'de (!ik); kullanıcı `Dizi<T>` paramı aşağıdaki
                     * D-070 yolundan geçer. */
                    const Dugum *argi = d->veri.cagri.argumanlar[i];
                    if (!ik && argi && argi->tip == DUGUM_DIZI_OLUSTUR) {
                        if (i == 0 && dizi_desc_built_in) {
                            /* descriptor arg0: dizi_al'da eleman tipi = dönüş
                             * beklenen; diğerlerinde (boyut/kapasite/temp ekle)
                             * eleman tipi işlemi etkilemez → i32 varsayılan. */
                            const char *eb = (dizi_al_mi && beklenen && *beklenen)
                                ? beklenen : "i32";
                            args[i] = dizi_literal_heap_emit(g, argi, eb, NULL);
                            continue;
                        }
                        if (i == dizi_deger_arg && dizi_built_in &&
                            dizi_deger_eleman_ast &&
                            dizi_deger_eleman_ast->tip == DUGUM_TIP_DIZI) {
                            /* iç-içe Dizi<Dizi<T>>: değer-arg literali Dizi<T>;
                             * elemanı (T) recursive bağlam olarak ver. */
                            args[i] = dizi_literal_heap_emit(g, argi, NULL,
                                dizi_deger_eleman_ast->veri.tip_dizi.eleman_tip);
                            continue;
                        }
                    }
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
            if (!ik && cagri_adi_uz >= 6 && memcmp(cagri_adi, "metin_", 6) == 0) {
                /* Madde A: metin_* built-in -> kdl_metin_* (D2: !ik — kullanici
                 * islevi cozulmusse builtin'e kacirma; tum else-if ladder atlanir) */
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
                    "  %%%d = call ptr @kdl_dizi_olustur(ptr %s, i32 %d)\n",
                    rr, g->rho_ref, eb);   /* V2-F4.2a: ρ */
                /* Adim 6: kapasiteyi pre-reserve et (kullanici N istiyor) */
                fprintf(g->out,
                    "  call void @kdl_dizi_kapasite_ayarla(ptr %s, ptr %%%d, i32 %%%d)\n",
                    g->rho_ref, rr, kap_i32);   /* V2-F4.2a: ρ */
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
                /* D-087: struct eleman → by-value kdl_dizi_ekle_yapi. */
                if (dizi_eleman_struct_mi(et) && n > 1) {
                    dizi_struct_ekle_emit(g, args[0].reg, args[1].reg, et);
                    int rr = yeni_reg(g);
                    fprintf(g->out, "  %%%d = add i32 0, 0\n", rr);
                    IfadeSonuc s = { rr, "i32", 0 };
                    return s;
                }
                const char *fn;
                const char *cast_tip = et;
                if (strcmp(et, "i64") == 0) fn = "kdl_dizi_ekle_tam64";
                else if (strcmp(et, "ptr") == 0) fn = "kdl_dizi_ekle_ptr";
                else { fn = "kdl_dizi_ekle_tam"; cast_tip = "i32"; }
                int ev = (n > 1) ? int_donustur(g, args[1].reg,
                                                 args[1].tip, cast_tip)
                                 : 0;
                fprintf(g->out,
                    "  call void @%s(ptr %s, ptr %%%d, %s %%%d)\n",
                    fn, g->rho_ref, args[0].reg, cast_tip, ev);   /* V2-F4.2a: ρ */
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
                int idx_i32 = (n > 1) ? int_donustur(g, args[1].reg,
                                                      args[1].tip, "i32") : 0;
                /* D-087: struct eleman → by-value kdl_dizi_al_yapi + load. */
                if (dizi_eleman_struct_mi(et)) {
                    return dizi_struct_al_emit(g, args[0].reg, idx_i32, et);
                }
                const char *fn;
                if (strcmp(et, "i64") == 0) fn = "kdl_dizi_al_tam64";
                else if (strcmp(et, "ptr") == 0) fn = "kdl_dizi_al_ptr";
                else { fn = "kdl_dizi_al_tam"; et = "i32"; }
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
                int idx_i32 = (n > 1) ? int_donustur(g, args[1].reg,
                                                      args[1].tip, "i32") : 0;
                /* D-087: struct eleman → by-value kdl_dizi_yaz_yapi. */
                if (dizi_eleman_struct_mi(et) && n > 2) {
                    dizi_struct_yaz_emit(g, args[0].reg, idx_i32,
                                         args[2].reg, et);
                    int rr = yeni_reg(g);
                    fprintf(g->out, "  %%%d = add i32 0, 0\n", rr);
                    IfadeSonuc s = { rr, "i32", 0 };
                    return s;
                }
                const char *fn;
                const char *cast_tip = et;
                if (strcmp(et, "i64") == 0) fn = "kdl_dizi_yaz_tam64";
                else if (strcmp(et, "ptr") == 0) fn = "kdl_dizi_yaz_ptr";
                else { fn = "kdl_dizi_yaz_tam"; cast_tip = "i32"; }
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
                    "  call void @kdl_dizi_kapasite_ayarla(ptr %s, ptr %%%d, i32 %%%d)\n",
                    g->rho_ref, args[0].reg, yk);   /* V2-F4.2a: ρ */
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

            /* V2-F4.2a: kullanıcı-fn (ik!=NULL) ρ ilk arg alır; built-in (ik==NULL:
             * yazdir/metin/dosya/...) ρ ALMAZ. D-257: çıplak user-fn → C-ABI, ρ YOK. */
            int u_rho = (ik != NULL) && !(ik->ast && ik->ast->veri.islev.ciplak_mi);
            if (strcmp(donus, "void") == 0) {
                /* void-returning call: register atama yok */
                fputs("  call void @", g->out);
                yerel_ad_yaz(g->out, cagri_adi, cagri_adi_uz);
                fputs("(", g->out);
                if (u_rho) fprintf(g->out, "ptr %s", g->rho_ref);
                for (int i = 0; i < n; i++) {
                    if (i > 0 || u_rho) fputs(", ", g->out);
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
            if (u_rho) fprintf(g->out, "ptr %s", g->rho_ref);   /* V2-F4.2a: ρ (kullanıcı-fn) */
            for (int i = 0; i < n; i++) {
                if (i > 0 || u_rho) fputs(", ", g->out);
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
            /* D-248 (GAP-1): int <-> ptr cast. hedef ptr ise kaynağı i64 (adres
             * genişliği) olarak üret; inttoptr/ptrtoint emit. tip_kontrol
             * güvensiz-scope garanti eder (raw pointer güvensiz-only). */
            int hedef_ptr = (strcmp(hedef, "ptr") == 0);
            const char *kaynak_bekle = hedef_ptr ? "i64" : hedef;
            IfadeSonuc kaynak = ifade_uret(g, d->veri.tip_donustur.kaynak,
                                            kaynak_bekle);
            if (strcmp(kaynak.tip, hedef) == 0) {
                kaynak.isaretsiz = hedef_isz;
                return kaynak;
            }
            int kaynak_ptr = (strcmp(kaynak.tip, "ptr") == 0);
            if (hedef_ptr && !kaynak_ptr) {
                int rp = yeni_reg(g);
                fprintf(g->out, "  %%%d = inttoptr %s %%%d to ptr\n",
                        rp, kaynak.tip, kaynak.reg);
                IfadeSonuc s = { rp, "ptr", 0 };
                return s;
            }
            if (kaynak_ptr && !hedef_ptr) {
                int rp = yeni_reg(g);
                fprintf(g->out, "  %%%d = ptrtoint ptr %%%d to %s\n",
                        rp, kaynak.reg, hedef);
                IfadeSonuc s = { rp, hedef, hedef_isz };
                return s;
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
            /* D-071 + V2-F1/F2: closure değeri = fat value { ptr fn, ptr env }
             * (by-value). Capture by-value. env F2'den itibaren HEAP (@malloc) →
             * closure kaçabilir (env yaşar). Lifted @lambda_N(ptr env, params)
             * DEFERRED emit (bekleyen_lambdalar). */
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
                /* V2-F2: env STACK alloca yerine HEAP (@malloc — dizi/metin ile
                 * AYNI allokatör; F4 region-dealloc buraya bağlanır). Closure
                 * frame'i aşsa bile env yaşar → kaçış UAF'i ortadan kalkar.
                 * SERBEST BIRAKMA YOK (leak — dizi/metin status-quo; F4 sonra).
                 * Boyut = sizeof(envtip): LLVM constexpr GEP-null idiomu (D-087);
                 * padding/alignment LLVM layout'uyla birebir. */
                fprintf(g->out,
                    "  %%%d = call ptr @malloc(i64 ptrtoint "
                    "(ptr getelementptr (%s, ptr null, i32 1) to i64))\n",
                    env_reg, envtip);
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
            bl->beklenen_donus_ir = g->lambda_beklenen_donus;   /* D-304 */
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
            /* V2-F1/F2 fat-value temsil: fn değeri = { ptr fn, ptr env } (by-value
             * SSA aggregate). Yakalama YOK → {@lambda_N, null} (bare; env yok).
             * Yakalama VAR → {@lambda_N, %env} (env F2'den itibaren HEAP malloc).
             * closure_mu/son_closure derleme-zamanı tag'i kalktı: çağrı yerinde
             * env==null runtime dispatch (stack/heap fark etmez). */
            if (cc.sayi == 0) {
                int t0 = yeni_reg(g);
                fprintf(g->out,
                    "  %%%d = insertvalue { ptr, ptr } undef, ptr @%s, 0\n",
                    t0, mang);
                int r = yeni_reg(g);
                fprintf(g->out,
                    "  %%%d = insertvalue { ptr, ptr } %%%d, ptr null, 1\n",
                    r, t0);
                IfadeSonuc s = { r, "{ ptr, ptr }", 0 };
                return s;
            }
            int t0 = yeni_reg(g);
            fprintf(g->out,
                "  %%%d = insertvalue { ptr, ptr } undef, ptr @%s, 0\n", t0, mang);
            int r = yeni_reg(g);
            fprintf(g->out,
                "  %%%d = insertvalue { ptr, ptr } %%%d, ptr %%%d, 1\n",
                r, t0, env_reg);
            IfadeSonuc s = { r, "{ ptr, ptr }", 0 };
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
                rho_yerel_serbest_emit(g);   /* F4.2b (d): dönüş değeri (rr) materyalize sonrası */
                fprintf(g->out, "  ret %s %%%d\n", donus_tip, rr);
            } else {
                rho_yerel_serbest_emit(g);   /* F4.2b (d) */
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
            /* D-088: iç-içe Dizi<Dizi<T>> için eleman AST tipi (iç dizi heap
             * üretimi + m[i][j] recursive çözümü). */
            const Dugum *eleman_tip_ast_d = NULL;
            if (d->veri.degisken.tip) {
                annot = ast_tip_to_ir(g, d->veri.degisken.tip);
                if (d->veri.degisken.tip->tip == DUGUM_TIP_DIZI) {
                    eleman_tip_ast_d =
                        d->veri.degisken.tip->veri.tip_dizi.eleman_tip;
                    eleman_tip = ast_tip_to_ir(g, eleman_tip_ast_d);
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
                /* F4.2b: `değişken xs: Dizi<T> = [...]` — dizi-literali `lit`
                 * escape'te kayıtlı; kaçmıyorsa ρ_yerel'e yönlendir (ret'te serbest). */
                const char *dizi_rho = bolge_yerel_yonlendir(g, lit, eleman_tip);
                int kdl_reg = yeni_reg(g);
                /* kdl_dizi_olustur(eleman_byte) — D-087: struct → sizeof const-expr */
                fprintf(g->out, "  %%%d = call ptr @kdl_dizi_olustur(ptr %s, i32 ",
                        kdl_reg, dizi_rho);   /* F4.2b: yönlendirilmiş ρ */
                kdl_eleman_byte_yaz(g->out, eleman_tip);
                fputs(")\n", g->out);
                (void)n;
                /* D-088: iç-içe Dizi<Dizi<T>> — iç eleman (`[1,2]`) üretilirken
                 * beklenen_tip'i İÇ dizi AST tipine (Dizi<T>) ayarla ki
                 * DUGUM_DIZI_OLUSTUR HEAP yolunu seçsin (aksi halde stack [N x T]
                 * düşer, dış heap dizi uzunluk-metadata'sız stack ptr tutardı). */
                const Dugum *eski_bt = g->beklenen_tip;
                g->beklenen_tip = eleman_tip_ast_d;
                /* Her elemani ekle */
                for (int i = 0; i < n; i++) {
                    IfadeSonuc v = ifade_uret(g, lit->veri.dizi_olustur.elemanlar[i],
                                              eleman_tip);
                    if (dizi_eleman_struct_mi(eleman_tip)) {
                        dizi_struct_ekle_emit(g, kdl_reg, v.reg, eleman_tip);
                        continue;
                    }
                    int vr = int_donustur(g, v.reg, v.tip, eleman_tip);
                    const char *fn = "kdl_dizi_ekle_tam";
                    if (strcmp(eleman_tip, "i64") == 0) fn = "kdl_dizi_ekle_tam64";
                    else if (strcmp(eleman_tip, "ptr") == 0) fn = "kdl_dizi_ekle_ptr";
                    fprintf(g->out,
                        "  call void @%s(ptr %s, ptr %%%d, %s %%%d)\n",
                        fn, dizi_rho, kdl_reg, eleman_tip, vr);   /* F4.2b: aynı ρ */
                }
                g->beklenen_tip = eski_bt;
                /* alloca ptr + store kdl_reg */
                int alloca_reg = yeni_reg(g);
                fprintf(g->out, "  %%%d = alloca ptr\n", alloca_reg);
                fprintf(g->out, "  store ptr %%%d, ptr %%%d\n",
                        kdl_reg, alloca_reg);
                isim_ekle(g, d->veri.degisken.ad,
                          d->veri.degisken.ad_uzunluk,
                          1, alloca_reg, "ptr");
                g->isimler->eleman_llvm_tip = eleman_tip;
                g->isimler->eleman_tip_ast = eleman_tip_ast_d;
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
                    /* D-304: blok-form lambda dönüş tipi — annotasyon işlev()->T
                     * ise T'nin IR'ini lambda queue'suna aktar (gövde ön-taraması
                     * yerine bağlamdan). NULL (işlev değil) → ifade-form davranışı. */
                    const char *eski_lbd = g->lambda_beklenen_donus;
                    g->lambda_beklenen_donus =
                        kapanis_donus_ir_al(g, d->veri.degisken.tip);
                    dv = ifade_uret(g, d->veri.degisken.deger, tip);
                    g->lambda_beklenen_donus = eski_lbd;
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
                    /* D-293: `değişken f: işlev() -> T = || ...` → T'nin IR'i.
                     * Fat value ("{ ptr, ptr }") T'yi siler; closure çağrı yeri
                     * dönüş tipini BURADAN alır (bkz. kapanis_donus_ir). */
                    g->isimler->kapanis_donus_ir =
                        kapanis_donus_ir_al(g, d->veri.degisken.tip);
                    /* D-294: `değişken g: görev<T> = ...` → T'nin IR'i
                     * (görev<T> IR'de opak ptr; birleştir i64→T daraltması). */
                    g->isimler->gorev_ic_ir =
                        gorev_ic_ir_al(g, d->veri.degisken.tip);
                    g->isimler->kanal_ic_ir =
                        kanal_ic_ir_al(g, d->veri.degisken.tip);
                    /* v1 bölge-container: *T annot -> pointee kaydi */
                    g->isimler->pointee_llvm_tip =
                        pointee_ir_al(g, d->veri.degisken.tip);
                    g->isimler->pointee_isaretsiz =
                        pointee_isaretsiz_al(d->veri.degisken.tip);
                    /* D-029 fix: &Yapi, *Yapi veya Yapi annot -> yapi IR kaydi */
                    g->isimler->ref_yapi_ir =
                        ref_yapi_ir_al(g, d->veri.degisken.tip);
                    /* Liste<T> BUG-2: Kullanici<X> annot -> X IR kaydi */
                    g->isimler->generic_arg_ir =
                        generic_arg_ir_al(g, d->veri.degisken.tip);
                    if (eleman_tip) {
                        g->isimler->eleman_llvm_tip = eleman_tip;
                        /* D-088: iç-içe `m[i][j]` recursive çözümü için iç dizi
                         * AST tipi. `inner: Dizi<tam32> = m[0]` gibi türetilmiş
                         * heap dizi de buradan işaretlenir. */
                        g->isimler->eleman_tip_ast = eleman_tip_ast_d;
                        /* D-085: Dizi<T> annotasyonlu ama değeri literal-DEĞİL
                         * (örn. `= yap()` çağrı dönüşü, `= başka_dizi`) değişken
                         * de heap KdlDizi* tutar → `xs[i]` heap-route edilmeli.
                         * (Literal değer ayrı dedicated heap path'te işaretlenir.)
                         * Tip kontrolü Dizi<T> annotasyonunu zaten heap garanti
                         * eder (stack dizi → G003 reddi). */
                        g->isimler->dinamik_dizi_mi = 1;
                    }
                } else {
                    /* Annot yok: deger once, sonra alloca */
                    /* D-325: deger LAMBDA ise donus IR'ini govdeden tahmin et ve
                     * AYNI degeri hem define'a (lambda_beklenen_donus) hem cagri
                     * yerine (kapanis_donus_ir) ver — yoksa define double / call
                     * i32 sessiz uyusmazligi olusur (olculdu). */
                    const char *lam_tahmin = NULL;
                    const char *eski_lbd2 = g->lambda_beklenen_donus;
                    if (d->veri.degisken.deger->tip == DUGUM_LAMBDA) {
                        lam_tahmin = lambda_donus_tahmin(
                            g, d->veri.degisken.deger->veri.lambda.govde);
                        if (lam_tahmin) g->lambda_beklenen_donus = lam_tahmin;
                    }
                    dv = ifade_uret(g, d->veri.degisken.deger, NULL);
                    g->lambda_beklenen_donus = eski_lbd2;
                    tip = dv.tip;
                    int alloca_reg = yeni_reg(g);
                    fprintf(g->out, "  %%%d = alloca %s\n", alloca_reg, tip);
                    fprintf(g->out, "  store %s %%%d, ptr %%%d\n",
                            tip, dv.reg, alloca_reg);
                    isim_ekle(g, d->veri.degisken.ad,
                              d->veri.degisken.ad_uzunluk,
                              1, alloca_reg, tip);
                    g->isimler->isaretsiz = dv.isaretsiz;  /* D-005 */
                    /* D-325: tahmin varsa cagri yeri de AYNI donusu gorsun. */
                    if (lam_tahmin) g->isimler->kapanis_donus_ir = lam_tahmin;
                    /* D-293 NOT: kapanis_donus_ir burada AYARLANMAZ (tahmin yoksa) — bu dal
                     * "annot yok" yolu (d->veri.degisken.tip == NULL), yani
                     * bildirilen closure dönüş tipi zaten YOK. Annotasyonsuz
                     * closure değişkeni (`değişken f = || "selam"`) çağrıldığında
                     * dönüş tipi bilinemez → beklenen'e düşer (mevcut davranış). */
                    /* D-069 Kat.2: değer sabit stack dizisi [N x T] ise N kaydet
                     * (arr[i] sınır-kontrolü için). Annot yok → stack yolu. */
                    if (d->veri.degisken.deger->tip == DUGUM_DIZI_OLUSTUR) {
                        g->isimler->dizi_uzunluk =
                            d->veri.degisken.deger->veri.dizi_olustur.sayi;
                    }
                    /* V2-F1: lambda değeri artık fat value { ptr, ptr } (dv.tip)
                     * → alloca/store/load yukarıda jenerik. closure_mu set'i KALKTI
                     * (closure'luk env!=null ile, değerin parçası). */
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
                g->isimler->kapanis_donus_ir =
                    kapanis_donus_ir_al(g, d->veri.degisken.tip);   /* D-293 */
                g->isimler->gorev_ic_ir =
                    gorev_ic_ir_al(g, d->veri.degisken.tip);        /* D-294 */
                g->isimler->kanal_ic_ir =
                    kanal_ic_ir_al(g, d->veri.degisken.tip);
                g->isimler->pointee_llvm_tip =
                    pointee_ir_al(g, d->veri.degisken.tip);
                g->isimler->pointee_isaretsiz =
                    pointee_isaretsiz_al(d->veri.degisken.tip);
                g->isimler->ref_yapi_ir =
                    ref_yapi_ir_al(g, d->veri.degisken.tip);
                g->isimler->generic_arg_ir =
                    generic_arg_ir_al(g, d->veri.degisken.tip);
                if (eleman_tip) {
                    g->isimler->eleman_llvm_tip = eleman_tip;
                    g->isimler->eleman_tip_ast = eleman_tip_ast_d;  /* D-088 */
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
                    /* [D-092] xs = [..]  (xs: Dizi<T> heap) — init yoluyla AYNI
                     * heap-promote. beklenen_tip'i hedefin Dizi<T> AST'sine
                     * ayarla → DUGUM_DIZI_OLUSTUR heap KdlDizi* üretir; aksi
                     * halde stack [N x T] ptr KdlDizi* slot'a store edilir →
                     * dizi_ekle/dizi_boyut SEGFAULT (accept-but-crash). */
                    if (i->dinamik_dizi_mi && i->eleman_tip_ast &&
                        d->veri.atama.deger->tip == DUGUM_DIZI_OLUSTUR) {
                        const Dugum *eski_bt = g->beklenen_tip;
                        g->beklenen_tip = dizi_tip_sar(g, i->eleman_tip_ast);
                        IfadeSonuc v = ifade_uret(g, d->veri.atama.deger, "ptr");
                        g->beklenen_tip = eski_bt;
                        fprintf(g->out, "  store ptr %%%d, ptr %%%d\n",
                                v.reg, i->reg_no);
                    } else {
                        IfadeSonuc v = ifade_uret(g, d->veri.atama.deger,
                                                   i->llvm_tip);
                        int rr = int_donustur(g, v.reg, v.tip, i->llvm_tip);
                        fprintf(g->out, "  store %s %%%d, ptr %%%d\n",
                                i->llvm_tip, rr, i->reg_no);
                    }
                } else {
                    /* D-252: küresel değişken atama → store @ad (local DEĞİL;
                     * checker güvensiz-only enforce etti). */
                    SabitKayit *ku = kuresel_bul(g,
                        d->veri.atama.hedef->veri.tanimlayici.metin,
                        d->veri.atama.hedef->veri.tanimlayici.uzunluk);
                    if (ku) {
                        const char *ir = ku->tip ? ast_tip_to_ir(g, ku->tip) : "i32";
                        if (!ir) ir = "i32";
                        IfadeSonuc v = ifade_uret(g, d->veri.atama.deger, ir);
                        int rr = int_donustur(g, v.reg, v.tip, ir);
                        fprintf(g->out, "  store %s %%%d, ptr @", ir, rr);
                        yerel_ad_yaz(g->out,
                            d->veri.atama.hedef->veri.tanimlayici.metin,
                            d->veri.atama.hedef->veri.tanimlayici.uzunluk);
                        fputc('\n', g->out);
                    }
                }
            } else if (d->veri.atama.hedef &&
                       d->veri.atama.hedef->tip == DUGUM_INDEKS) {
                /* Audit fix #2: arr[i] = v — onceden SESSIZCE dusurulurdu.
                 * D-085 [YÜKSEK]: heap dizi (TANIMLAYICI heap VEYA türetilmiş
                 * yapı-alanı/işlev-dönüşü) → kdl_dizi_yaz (runtime sınır-kontrollü).
                 * Önceki durum: TANIMLAYICI heap yazma SESSİZCE DÜŞÜRÜLÜYORDU
                 * (yorum, kdl_dizi_yaz_eleman yok varsayımı — artık var); türetilmiş
                 * heap yazma KdlDizi*'ı düz veri sanıp GEP+store → SEGFAULT.
                 * Stack dizi: GEP aynası + store + D-069 sınır-kontrol. */
                const Dugum *hedef = d->veri.atama.hedef;
                int heap_dizi = 0;
                const char *heap_et = NULL;   /* heap ise eleman IR tipi */
                const char *pointee_elem = NULL;
                int stack_uz = 0;   /* D-069 Kat.2: sabit stack dizi N (>0 → sınır-kontrol) */
                if (hedef->veri.indeks.nesne &&
                    hedef->veri.indeks.nesne->tip == DUGUM_TANIMLAYICI) {
                    LlvmIsim *vi = isim_bul(g,
                        hedef->veri.indeks.nesne->veri.tanimlayici.metin,
                        hedef->veri.indeks.nesne->veri.tanimlayici.uzunluk);
                    if (vi && vi->dinamik_dizi_mi) {
                        heap_dizi = 1;
                        heap_et = vi->eleman_llvm_tip
                            ? vi->eleman_llvm_tip : "i32";
                    }
                    /* v1 bölge-container: *T tabani — eleman tipi
                     * POINTEE'den (RHS tipi tam8/tam64 hedefte yanlis
                     * genislik uretirdi). */
                    else if (vi && vi->pointee_llvm_tip) {
                        pointee_elem = vi->pointee_llvm_tip;
                    }
                    /* D-069 Kat.2: sabit stack dizi [N x T] → sınır-kontrol için N */
                    if (vi && !heap_dizi) stack_uz = vi->dizi_uzunluk;
                } else if (hedef->veri.indeks.nesne) {
                    /* D-085 + D-088: türetilmiş/iç-içe heap dizi tabanı (k.xs /
                     * yap() / m[i]). AST resolver eleman tipini verir; nested
                     * `m[i][j] = v` için `m[i]` heap KdlDizi* → kdl_dizi_yaz. */
                    const Dugum *elem_ast = heap_dizi_eleman_ast(g,
                        hedef->veri.indeks.nesne);
                    if (elem_ast) {
                        const char *et = ast_tip_to_ir(g, elem_ast);
                        if (!et) et = "i32";
                        heap_dizi = 1; heap_et = et;
                    }
                }
                if (heap_dizi && heap_et) {
                    /* D-085/D-087: kdl_dizi_yaz(descriptor, i32 idx, T v).
                     * Descriptor = ifade_uret(taban) (TANIMLAYICI heap → load
                     * KdlDizi*; ERISIM/CAGRI → KdlDizi*). Runtime OOB → PANIC. */
                    IfadeSonuc base = ifade_uret(g,
                        hedef->veri.indeks.nesne, NULL);
                    IfadeSonuc idx = ifade_uret(g,
                        hedef->veri.indeks.indeks, "i32");
                    int idx_r = int_donustur(g, idx.reg, idx.tip, "i32");
                    if (dizi_eleman_struct_mi(heap_et)) {
                        /* D-087: by-value yapı eleman — kdl_dizi_yaz_yapi. */
                        IfadeSonuc v = ifade_uret(g, d->veri.atama.deger,
                                                  heap_et);
                        dizi_struct_yaz_emit(g, base.reg, idx_r, v.reg, heap_et);
                    } else {
                        const char *cast =
                            (strcmp(heap_et, "i64") == 0 ||
                             strcmp(heap_et, "ptr") == 0) ? heap_et : "i32";
                        IfadeSonuc v = ifade_uret(g, d->veri.atama.deger, cast);
                        int vr = int_donustur_im(g, v.reg, v.tip, cast,
                                                 v.isaretsiz);
                        fprintf(g->out,
                            "  call void @%s(ptr %%%d, i32 %%%d, %s %%%d)\n",
                            kdl_yaz_fn(heap_et), base.reg, idx_r, cast, vr);
                    }
                } else {
                    IfadeSonuc nesne = ifade_uret(g,
                        hedef->veri.indeks.nesne, NULL);
                    IfadeSonuc idx = ifade_uret(g,
                        hedef->veri.indeks.indeks, "i64");
                    int idx_r = int_donustur(g, idx.reg, idx.tip, "i64");
                    /* D-069 Kat.2: sabit stack dizi sınır-kontrolü (GEP/store'dan
                     * ÖNCE). Okuma yolunun (DUGUM_INDEKS) aynası — yazma yolu
                     * eskiden kontrolsüzdü: `xs[10]=9` SESSİZ buffer-overflow
                     * (exit 0). `icmp uge` unsigned → negatif + i>=N tek seferde.
                     * OOB → kdl_panik. güvensiz blok içinde ATLANIR (opt-out). */
                    if (stack_uz > 0 && g->guvensiz_derinlik == 0) {
                        int c_r = yeni_reg(g);
                        fprintf(g->out, "  %%%d = icmp uge i64 %%%d, %d\n",
                                c_r, idx_r, stack_uz);
                        int L_oob = yeni_label(g);
                        int L_ok = yeni_label(g);
                        fprintf(g->out,
                            "  br i1 %%%d, label %%bb%d, label %%bb%d\n",
                            c_r, L_oob, L_ok);
                        fprintf(g->out, "bb%d:\n", L_oob);
                        fprintf(g->out,
                            "  call void @kdl_panik(ptr @.str.dizi_sinir_panik)\n");
                        fprintf(g->out, "  unreachable\n");
                        fprintf(g->out, "bb%d:\n", L_ok);
                    }
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
                const char *alan_ir = NULL;
                int adr = erisim_lvalue(g, d->veri.atama.hedef, &alan_ir);
                /* [D-092] k.xs = [..]  (alan Dizi<T> heap) — TANIMLAYICI ile
                 * aynı delik: dizi-literali stack ÜRETME, heap KdlDizi yap ve
                 * alan adresine KdlDizi* store et. dizi_alan_eleman_ast alanın
                 * Dizi<T> eleman AST'sini verir (NULL → normal skaler alan). */
                const Dugum *alan_eleman_ast = NULL;
                if (d->veri.atama.deger->tip == DUGUM_DIZI_OLUSTUR) {
                    alan_eleman_ast =
                        dizi_alan_eleman_ast(g, d->veri.atama.hedef);
                }
                if (adr >= 0 && alan_eleman_ast) {
                    const Dugum *eski_bt = g->beklenen_tip;
                    g->beklenen_tip = dizi_tip_sar(g, alan_eleman_ast);
                    IfadeSonuc v = ifade_uret(g, d->veri.atama.deger, "ptr");
                    g->beklenen_tip = eski_bt;
                    fprintf(g->out, "  store ptr %%%d, ptr %%%d\n",
                            v.reg, adr);
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
            } else if (d->veri.atama.hedef &&
                       d->veri.atama.hedef->tip == DUGUM_TEKLI &&
                       d->veri.atama.hedef->veri.tekli.op == OP_DEREFERANS) {
                /* D-248 (GAP-2): *p = v — güvensiz ham pointer üzerinden yazma.
                 * tip_kontrol güvensiz-scope kontrol etti. p'yi ptr üret, değeri
                 * doğal tipinde store et. güvensiz blokta VOLATILE (MMIO
                 * side-effect korunur; clang elemez/sıralamaz). */
                IfadeSonuc p = ifade_uret(g,
                    d->veri.atama.hedef->veri.tekli.operand, "ptr");
                IfadeSonuc v = ifade_uret(g, d->veri.atama.deger, NULL);
                const char *vol = (g->guvensiz_derinlik > 0) ? "volatile " : "";
                fprintf(g->out, "  store %s%s %%%d, ptr %%%d\n",
                        vol, v.tip, v.reg, p.reg);
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
            /* x = dizi_al(kdl_ptr, i_load)
             * D-342: BY-VALUE eleman (%Yapi / kapanis fat value) skaler
             * `kdl_dizi_al_tam` ile OKUNAMAZ. Onceki kod donus tipine `et`
             * yazip `@kdl_dizi_al_tam`i cagiriyordu: declare i32 vs call
             * %Nokta. **LLVM bunu SESSIZCE kabul ediyor** (D-295/D-334
             * dersinin tekrari) → cop okunuyordu (olculdu: `için` ile
             * 2-elemanli Nokta dizisi toplaminda exit 14, dogrusu 42).
             * Diger dizi yollarindaki (INDEKS/dizi_al/ekle/yaz) by-value
             * yonlendirmesi VARDI; yalniz `için` dalinda eksikti. */
            int el_reg;
            if (dizi_eleman_struct_mi(et)) {
                IfadeSonuc es = dizi_struct_al_emit(g, kdl_ptr, i_load, et);
                el_reg = es.reg;
            } else {
                el_reg = yeni_reg(g);
                fprintf(g->out, "  %%%d = call %s @%s(ptr %%%d, i32 %%%d)\n",
                        el_reg, et, fn_al, kdl_ptr, i_load);
            }
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
                /* D-318: `YapiAdi { alan }` — yapinin TEK "varyanti" oldugu icin
                 * desen DAIMA eslesir → catchall (kosul dali YOK). Baglamalar
                 * govde basinda extractvalue ile kurulur (asagida). */
                int catchall = !is_ctor && desen &&
                    (desen->tip == DUGUM_DESEN_JOKER ||
                     desen->tip == DUGUM_DESEN_YAPI ||
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
                /* D-318: yapi destructuring — her bagli alan icin extractvalue
                 * + alloca + store, sonra isim tablosuna kaydet. Scrutinee bir
                 * struct DEGERI (%Yapi) olarak `dr`de; alan indeksi yapi
                 * kaydindan cozulur. */
                if (desen && desen->tip == DUGUM_DESEN_YAPI) {
                    YapiKayit *yk = yapi_bul(g, desen->veri.desen_yapi.yapi_ad,
                                             desen->veri.desen_yapi.yapi_uz);
                    for (int fi = 0; yk && fi < desen->veri.desen_yapi.alan_sayi;
                         fi++) {
                        const char *fad = desen->veri.desen_yapi.alan_adlar[fi];
                        int fuz = desen->veri.desen_yapi.alan_uzlar[fi];
                        const Dugum *ftip_d = NULL;
                        int fidx = yapi_alan_indeksi(yk, fad, fuz, &ftip_d);
                        if (fidx < 0) continue;
                        const char *fir = ast_tip_to_ir(g, ftip_d);
                        if (!fir) fir = "i32";
                        int er = yeni_reg(g);
                        fprintf(g->out, "  %%%d = extractvalue %s %%%d, %d\n",
                                er, sty, s.reg, fidx);
                        int ar = yeni_reg(g);
                        fprintf(g->out, "  %%%d = alloca %s\n", ar, fir);
                        fprintf(g->out, "  store %s %%%d, ptr %%%d\n",
                                fir, er, ar);
                        isim_ekle(g, fad, fuz, 0, ar, fir);
                    }
                }
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
                        /* D-307: generic çeşit → payload tipini scrutinee'nin
                         * INLINE agg'inden ({i8, ptr}) oku; base T ast_tip_to_ir'da
                         * çözülemez (i32'ye düşer → ptr payload ile uyumsuz). */
                        const char *pir;
                        char pbuf[160];
                        if (sty && sty[0] == '{' &&
                            agg_alan_ir(sty, ofs + j, pbuf, sizeof(pbuf))) {
                            pir = pbuf;
                        } else {
                            pir = ast_tip_to_ir(g,
                                cd->veri.cesit.varyant_payload_tipleri[vi][j]);
                        }
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
                const char *hm = llvm_hedef_mimari();   /* D-269: çalışma-zamanı hedef */
                int hm_uz = (int)strlen(hm);
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
/* D-293: src'nin TAMAMINI dst'ye ekle (src başa sarılır, kapatılmaz).
 * lambda_emit'te gövde ayrı tampona yazılıp dönüş tipi öğrenildikten sonra
 * `define` satırının ARDINA eklemek için. */
static void dosya_kopyala(FILE *src, FILE *dst) {
    if (!src || !dst) return;
    fseek(src, 0, SEEK_SET);
    char tampon[4096];
    size_t n;
    while ((n = fread(tampon, 1, sizeof(tampon), src)) > 0) {
        fwrite(tampon, 1, n, dst);
    }
}

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

    /* F4.2b (a): C-tarafı escape + bölge analizi — IR-NÖTR (R2). Per-fn çalışır,
     * g->aktif_*'da saklanır. Bu adımda tahsis siteleri HÂLÂ rho_ref kullanır →
     * IR DEĞİŞMEZ, fixpoint yeşil kalır. (c-d) ρ_yerel'i bolge_belirle ile yönlendirir. */
    g->aktif_escape = NULL;
    g->aktif_bolge = NULL;
    g->rho_yerel = NULL;
    {
        EscapeAnaliz *ea = (EscapeAnaliz *)arena_ayir_sifir(g->arena, sizeof(EscapeAnaliz));
        BolgeAtama *ba = (BolgeAtama *)arena_ayir_sifir(g->arena, sizeof(BolgeAtama));
        if (ea && ba) {
            escape_baslat(ea, g->arena);
            escape_analiz_islev(ea, islev);
            bolge_atama_baslat(ba, g->arena, islev->veri.islev.ad,
                               islev->veri.islev.ad_uzunluk);
            bolge_atama_escape_bagla(ba, ea);
            g->aktif_escape = ea;
            g->aktif_bolge = ba;
        }
    }

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

    /* V2-F4.2a: region-passing ABI. main HARİÇ her fonksiyon ilk param `ptr %rho`.
     * main'in çağıranı (libc) ρ geçirmez → main ρ param ALMAZ; gövde başında
     * global bölgeden seed eder. */
    int main_mi = (islev->veri.islev.ad_uzunluk == 4 &&
                   memcmp(islev->veri.islev.ad, "main", 4) == 0);

    /* D-257 çıplak işlev: TRUE C-ABI bare fonksiyon — ρ param ALMAZ (main gibi).
     * Böylece @malloc(i64) / interrupt / syscall gibi C-ABI sembolleri .kem'de
     * çıplak fn olarak ifade edilebilir. Çağrı yerleri de çıplak-callee'ye ρ
     * geçmez (aşağıda cagrilan_ciplak_mi). Sonuç: çıplak yalnız çıplak/extern
     * çağırabilir (verilecek ρ yok → çıplak-call-rule, checker E013). */
    int ciplak = islev->veri.islev.ciplak_mi;
    int rho_var = (!main_mi && !ciplak);   /* bu fn `ptr %rho` param alır mı */

    fprintf(g->out, "define %s @", donus);
    yerel_ad_yaz(g->out, islev->veri.islev.ad, islev->veri.islev.ad_uzunluk);
    fputs("(", g->out);
    if (rho_var) fputs("ptr %rho", g->out);   /* V2-F4.2a: ρ ilk param (çıplak/main hariç) */

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
        if (i > 0 || rho_var) fputs(", ", g->out);   /* ρ'dan sonra virgül */
        fprintf(g->out, "%s %%", tip);
        yerel_ad_yaz(g->out, p->veri.parametre.ad,
                     p->veri.parametre.ad_uzunluk);
    }
    fputs(") {\nentry:\n", g->out);

    g->reg = 0;
    g->label = 0;
    g->isimler = NULL;

    /* D-254/D-257 çıplak işlev: region-prologue EMIT EDİLMEZ (WALL-2 / bootstrap
     * circularity çözümü). @kdl_global_bolge_al + @kdl_bolge_olustur çağrısı YOK
     * → çıplak fn'in IR'ında sıfır region-symbol referansı (K1 allocator hedefi:
     * malloc→region→malloc döngüsü kırılır). D-257: ρ param HİÇ alınmaz (true C-ABI)
     * → gövdede ρ yok, rho_ref = "null" (çıplak-call-rule gereği zaten kullanılmaz;
     * çıplak yalnız çıplak/extern çağırır). rho_yerel = NULL → ret'lerde serbest no-op. */
    if (ciplak) {
        g->rho_ref = "null";
        g->rho_yerel = NULL;
    } else {
        /* V2-F4.2a: ρ referansı. Normal fn → "%rho" (param). main → global bölge
         * seed (gövdenin İLK komutu; provizyonel reg, hoist_renumber tutarlı yeniler). */
        if (main_mi) {
            int rr = yeni_reg(g);
            fprintf(g->out, "  %%%d = call ptr @kdl_global_bolge_al()\n", rr);
            char *rs = (char *)arena_ayir(g->arena, 16);
            snprintf(rs, 16, "%%%d", rr);
            g->rho_ref = rs;
        } else {
            g->rho_ref = "%rho";
        }

        /* F4.2b (c): ρ_yerel — scope-yerel bölge aç (gövde ilk komutlarından; hoist
         * reg'leri tutarlı yeniler). Kaçmayan (BOLGE_YEREL) tahsisler buraya yönlenir,
         * her ret'ten önce kdl_bolge_serbest. main dahil her fn (main'in GLOBAL %rho'su
         * serbest EDİLMEZ; ρ_yerel ayrı + serbest). Bu commit: makine kuruldu, yönlendirme
         * (c-d routing) sonraki commit'te — şimdilik ρ_yerel BOŞ (sound: serbest no-op). */
        int yr = yeni_reg(g);
        fprintf(g->out, "  %%%d = call ptr @kdl_bolge_olustur()\n", yr);
        char *ys = (char *)arena_ayir(g->arena, 16);
        snprintf(ys, 16, "%%%d", yr);
        g->rho_yerel = ys;
    }

    /* D-254: çıplak gövde örtük güvensiz-bağlam — ham pointer deref-write + küresel
     * yazımı explicit `güvensiz {}` gerektirmez (çıplak = güvensiz-tier primitive). */
    int ciplak_onceki_guvensiz = g->guvensiz_derinlik;
    if (ciplak) g->guvensiz_derinlik++;

    /* Parametreleri alloca'ya kopyala */
    for (int i = 0; i < n; i++) {
        const Dugum *p = islev->veri.islev.parametreler[i];
        const char *tip = param_tipler[i];
        /* D-086: &Dizi<T> / &değişken Dizi<T> param. Çağıran `&a` ile dizi
         * değişkeninin SLOT adresini (KdlDizi**) geçer; bunu girişte BİR KEZ
         * deref edip alloca'ya KdlDizi* yazarız → sonrası normal heap dizi
         * (dizi_al/yaz/[] ek deref gerektirmez). Önceki durum: çift-pointer
         * descriptor gibi indekslenip çöp/PANIK. */
        const Dugum *ptip = p->veri.parametre.tip;
        int dizi_ref = (ptip && ptip->tip == DUGUM_TIP_REFERANS &&
                        ptip->veri.tip_referans.hedef_tip &&
                        ptip->veri.tip_referans.hedef_tip->tip == DUGUM_TIP_DIZI);
        int alloca_reg = yeni_reg(g);
        fprintf(g->out, "  %%%d = alloca %s\n", alloca_reg, tip);
        if (dizi_ref) {
            int deref = yeni_reg(g);
            fprintf(g->out, "  %%%d = load ptr, ptr %%", deref);
            yerel_ad_yaz(g->out, p->veri.parametre.ad,
                         p->veri.parametre.ad_uzunluk);
            fputs("\n", g->out);
            fprintf(g->out, "  store ptr %%%d, ptr %%%d\n", deref, alloca_reg);
        } else {
            fprintf(g->out, "  store %s %%", tip);
            yerel_ad_yaz(g->out, p->veri.parametre.ad,
                         p->veri.parametre.ad_uzunluk);
            fprintf(g->out, ", ptr %%%d\n", alloca_reg);
        }
        isim_ekle(g, p->veri.parametre.ad, p->veri.parametre.ad_uzunluk,
                  0, alloca_reg, tip);
        /* D-293: `işlev(...) -> T` tipli parametre (closure argüman) → T'nin IR'i */
        g->isimler->kapanis_donus_ir =
            kapanis_donus_ir_al(g, p->veri.parametre.tip);
        /* D-294: `görev<T>` tipli parametre → T'nin IR'i (birleştir daraltması) */
        g->isimler->gorev_ic_ir =
            gorev_ic_ir_al(g, p->veri.parametre.tip);
        g->isimler->kanal_ic_ir =
            kanal_ic_ir_al(g, p->veri.parametre.tip);
        /* D-005: dtamN parametre -> isaretsiz isim */
        g->isimler->isaretsiz =
            ast_tip_isaretsiz_mi(p->veri.parametre.tip);
        /* v1 bölge-container: *T parametre -> pointee kaydi */
        g->isimler->pointee_llvm_tip =
            pointee_ir_al(g, p->veri.parametre.tip);
        g->isimler->pointee_isaretsiz =
            pointee_isaretsiz_al(p->veri.parametre.tip);
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
            const Dugum *elem_ast =
                p->veri.parametre.tip->veri.tip_dizi.eleman_tip;
            const char *et = ast_tip_to_ir(g, elem_ast);
            if (et) g->isimler->eleman_llvm_tip = et;
            g->isimler->eleman_tip_ast = elem_ast;  /* D-088: iç-içe m[i][j] */
            g->isimler->dinamik_dizi_mi = 1;
        }
        /* D-086: &Dizi<T> param girişte deref edildi → normal heap dizi gibi
         * işaretle (eleman tipi referans hedefinden). */
        if (dizi_ref) {
            const Dugum *iz = ptip->veri.tip_referans.hedef_tip;
            const Dugum *elem_ast = iz->veri.tip_dizi.eleman_tip;
            const char *et = ast_tip_to_ir(g, elem_ast);
            if (et) g->isimler->eleman_llvm_tip = et;
            g->isimler->eleman_tip_ast = elem_ast;  /* D-088: iç-içe m[i][j] */
            g->isimler->dinamik_dizi_mi = 1;
        }
        /* V2-F1: işlev(...)→R parametresi artık fat value { ptr, ptr } (tip
         * ast_tip_to_ir'den jenerik). Çağrı yerinde env==null runtime dispatch
         * → bare fn(args) / closure fn(env,args). Yakalamalı lambda'yı param'a
         * geçmek de çalışır (D-071 KAPSAM-DIŞI item kapandı). */
    }

    int term = 0;
    if (islev->veri.islev.govde) {
        term = blok_uret(g, islev->veri.islev.govde);
    }
    if (!term) {
        rho_yerel_serbest_emit(g);   /* F4.2b (d): örtük fn-sonu dönüşünden önce */
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

    /* F4.2b (a): escape malloc tablolarını serbest bırak (per-fn temizlik). */
    if (g->aktif_escape) {
        escape_serbest(g->aktif_escape);
        g->aktif_escape = NULL;
        g->aktif_bolge = NULL;
    }
    g->rho_yerel = NULL;   /* F4.2b: sonraki fn'e sızmasın */
    g->guvensiz_derinlik = ciplak_onceki_guvensiz;   /* D-254: çıplak güvensiz-grant geri al */
}

/* D-071 (Sınıf B lambda V2): lifted lambda — `define <ret> @lambda_N(ptr %env,
 * <params>)`. islev_uret deseni + (a) env ilk param, (b) capture'lar %env'den
 * load → lokal. DEFERRED çağrılır (çevre fn gövdesi emit edildikten sonra).
 * v1: ifade-form gövde, dönüş i32 (4 örnek). */
static void lambda_emit(LlvmGen *g, BekleyenLambda *bl) {
    const Dugum *d = bl->dugum;
    /* F4.2b: lambda bu fazda ρ_yerel ALMAZ (yönlendirme yok) → VER ret'leri
     * serbest emit etmesin (NULL). Kuşatan fn'in ρ_yerel'ini serbest etme. */
    g->rho_yerel = NULL;
    int np = d->veri.lambda.param_sayi;
    char envtip[512]; int eo = 0;
    eo += snprintf(envtip + eo, sizeof(envtip) - eo, "{ ");
    for (int i = 0; i < bl->capture_sayi; i++)
        eo += snprintf(envtip + eo, sizeof(envtip) - eo, "%s%s",
                       i ? ", " : "", bl->capture_irler[i]);
    snprintf(envtip + eo, sizeof(envtip) - eo, " }");

    /* D-293: dönüş tipi artık SABİT i32 DEĞİL — ifade-form gövdenin kendi DOĞAL
     * IR tipinden çıkarsanır. Eskiden `donus = "i32"` sabitti; `|| "selam"`
     * (işlev() -> metin) ve `|| 3.5` (işlev() -> kesirli64) tip kontrolünden
     * GEÇİP LLVM'de patlıyordu ("'%0' defined with type 'ptr' but expected
     * 'i32'"). Bu, görev/kanal'dan BAĞIMSIZ, düz lambda kullanımını kıran
     * gerçek bir bug'dı.
     *
     * `define` satırı dönüş tipini BİLMEDEN yazılamaz; gövdenin tipi ise ancak
     * emit edilince belli olur. Bu yüzden İKİ tampon: gövde önce `ic_tmp`'ye,
     * tip öğrenilince `define` satırı `govde_tmp`'ye, sonra gövde eklenir.
     * hoist_renumber zaten TÜM fonksiyon metnini tutarlı yeniden numaralandırır
     * (define satırında numaralı register yok — paramlar adlı: %rho/%env/%x),
     * dolayısıyla bu sıralama değişikliği güvenli. */
    const char *donus = "i32";
    g_donus_tip = donus;
    g->aktif_donus_dugum = NULL;

    FILE *gercek_out = g->out;
    FILE *govde_tmp = tmpfile();
    FILE *ic_tmp = tmpfile();
    /* tmpfile() başarısızsa: eski davranışa (sabit i32, tek tampon) düş —
     * çıkarsama kaybolur ama üretim bozulmaz. */
    if (!ic_tmp || !govde_tmp) {
        if (ic_tmp) fclose(ic_tmp);
        ic_tmp = NULL;
        if (govde_tmp) g->out = govde_tmp;
    } else {
        g->out = ic_tmp;
    }

    int has_env = bl->capture_sayi > 0;
    /* Param IR tipleri define satırında da, prologda da lazım → önce hesapla. */
    const char **ptip = NULL;
    if (np > 0) ptip = (const char **)arena_ayir(g->arena,
        sizeof(const char *) * (size_t)np);
    for (int i = 0; i < np; i++) {
        const Dugum *p = d->veri.lambda.parametreler[i];
        const char *t = ast_tip_to_ir(g, p->veri.parametre.tip);
        if (!t) t = "i32";
        ptip[i] = t;
    }
    /* ic_tmp yoksa (fallback) define satırını ŞİMDİ yaz — tip i32 kalır. */
    if (!ic_tmp) {
        fprintf(g->out, "define %s @%s(ptr %%rho", donus, bl->mangled);
        if (has_env) fputs(", ptr %env", g->out);
        for (int i = 0; i < np; i++) {
            const Dugum *p = d->veri.lambda.parametreler[i];
            fputs(", ", g->out);
            fprintf(g->out, "%s %%", ptip[i]);
            yerel_ad_yaz(g->out, p->veri.parametre.ad,
                         p->veri.parametre.ad_uzunluk);
        }
        fputs(") {\nentry:\n", g->out);
    }

    g->reg = 0; g->label = 0; g->isimler = NULL;
    /* V2-F4.2a: lambda ρ'yu param olarak alır (dispatch geçirir) → gövde
     * tahsis/kullanıcı-fn-çağrısı geçirilen ρ_caller'ı kullanır. */
    g->rho_ref = "%rho";

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
        /* V2-F1: lifted lambda'nın işlev-tipli paramı da fat value { ptr, ptr }
         * (ast_tip_to_ir jenerik) → çağrıda env==null runtime dispatch. */
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
        /* Blok-form gövde: son-`ver` çıkarsaması gövde ön-taramasını gerektirir
         * (döngüsel: `ver` emit'i g_donus_tip ister). D-304: bağlamdan gelen
         * BİLDİRİLEN dönüş IR'ı (işlev()->T annotasyonu, bl->beklenen_donus_ir)
         * varsa onu kullan → blok içindeki `ver` doğru tiple ret eder ve define
         * imzası eşleşir. Yoksa i32 kalır (eski davranış; annotasyonsuz blok-form
         * hâlâ çıkarsayamaz — o gerçek gövde ön-taraması ister). */
        if (bl->beklenen_donus_ir && *bl->beklenen_donus_ir) {
            donus = bl->beklenen_donus_ir;
            g_donus_tip = donus;
        }
        term = blok_uret(g, govde);
    } else if (govde) {
        /* İfade-form: beklenen tip VERME (NULL) → gövdenin DOĞAL IR tipi.
         * Eskiden burada `donus`("i32") beklenen olarak geçilip int_donustur
         * ile i32'ye zorlanıyordu; ptr/double gövdelerde bu, define'ın i32
         * imzasıyla çelişen bir değer üretiyordu (LLVM reddi). */
        IfadeSonuc r = ifade_uret(g, govde, NULL);
        if (ic_tmp && r.tip && *r.tip) donus = r.tip;   /* çıkarsanan dönüş */
        g_donus_tip = donus;
        int rr = int_donustur(g, r.reg, r.tip, donus);  /* eş tipte no-op */
        fprintf(g->out, "  ret %s %%%d\n", donus, rr);
        term = 1;
    }
    if (!term) {
        /* Terminatörsüz (blok-form ver'siz düşer). D-304: donus artık ptr/float
         * de olabilir (bildirilen tip) → tipe uygun sıfır-değer emit et
         * (`ret ptr 0` / `ret double 0` GEÇERSİZ olurdu). */
        const char *sifir = "0";
        if (strcmp(donus, "ptr") == 0) sifir = "null";
        else if (strcmp(donus, "float") == 0 || strcmp(donus, "double") == 0)
            sifir = "0.0";
        fprintf(g->out, "  ret %s %s\n", donus, sifir);
    }

    if (ic_tmp) {
        /* Tip artık BİLİNİYOR → define satırını yaz, sonra gövdeyi ekle. */
        g->out = govde_tmp;
        fprintf(g->out, "define %s @%s(ptr %%rho", donus, bl->mangled);
        if (has_env) fputs(", ptr %env", g->out);
        for (int i = 0; i < np; i++) {
            const Dugum *p = d->veri.lambda.parametreler[i];
            fputs(", ", g->out);
            fprintf(g->out, "%s %%", ptip[i]);
            yerel_ad_yaz(g->out, p->veri.parametre.ad,
                         p->veri.parametre.ad_uzunluk);
        }
        fputs(") {\nentry:\n", g->out);
        dosya_kopyala(ic_tmp, govde_tmp);
        fclose(ic_tmp);
    }
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
    /* C5/D-269: triple çalışma-zamanı hedeften (varsayılan makro; --mimari ile aarch64) */
    fprintf(out, "target triple = \"%s\"\n\n", llvm_hedef_triple());
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

    /* V2-F4.2a: region-passing — main/lambda ρ-seed bu global bölgeyi kullanır. */
    fputs("declare ptr @kdl_global_bolge_al()\n", out);
    fputs("declare ptr @kdl_bolge_olustur()\n", out);       /* F4.2b: ρ_yerel aç */
    fputs("declare void @kdl_bolge_serbest(ptr)\n", out);   /* F4.2b: ρ_yerel serbest */
    /* Katman 2 (Concurrency / DRF V1): görev_başlat / görev_birleştir hedefleri.
     * basla_kapanis(fn, fn, env): fn BİLEREK iki kez — C tarafında iki farklı
     * tipli fn-ptr parametresi (bare/kapanış), cast-siz dispatch için. */
    /* D-309: 4. param = rho_serbest (codegen confinement kanıtı). declare ve
     * call AYNI commit'te değişir — D-295 dersi: LLVM declare/call imza
     * uyuşmazlığını SESSİZCE kabul eder, "LLVM reddeder" bir savunma değildir. */
    fputs("declare ptr @kdl_gorev_basla_kapanis(ptr, ptr, ptr, i32)\n", out);
    /* D-294: i64 taşıma (işaretçi T'ler için); çağrı yerinde T'ye daraltılır. */
    fputs("declare i64 @kdl_gorev_birlestir(ptr)\n", out);
    /* Karar 1 (D-30x): görev_başlat başarısızlığında sonuç'un hata(metin)
     * payload'u. Sabit, preamble'da bir kez emit edilir (metin ön-geçişinden
     * BAĞIMSIZ) — "gorev baslatilamadi" = 19 bayt + \00 = 20. */
    fputs("@.gorev_hata_str = private constant [20 x i8] "
          "c\"gorev baslatilamadi\\00\"\n", out);
    /* R-KANAL hedefleri. Bu imza host (kdl_runtime.c) ve bare-metal
     * (kdl_kanal.c) sürümlerinde AYNI — tek çağrı iki backend'e de bağlanır. */
    fputs("declare ptr @kdl_kanal_olustur(i32)\n", out);
    /* D-295: kanal da i64 taşır (görev ile simetrik) — `kanal<tam64>` /
     * `kanal<metin>` sessizce kırpılmasın. */
    fputs("declare void @kdl_kanal_gonder(ptr, i64)\n", out);
    fputs("declare i64 @kdl_kanal_al(ptr)\n", out);
    /* Madde B: Dinamik dizi (KdlDizi*) — V2-F4.2a: allokasyon helper'ları ρ ilk param */
    fputs("declare ptr @kdl_dizi_olustur(ptr, i32)\n", out);
    fputs("declare void @kdl_dizi_ekle_tam(ptr, ptr, i32)\n", out);
    fputs("declare void @kdl_dizi_ekle_tam64(ptr, ptr, i64)\n", out);
    fputs("declare void @kdl_dizi_ekle_ptr(ptr, ptr, ptr)\n", out);
    fputs("declare i32 @kdl_dizi_al_tam(ptr, i32)\n", out);
    fputs("declare i64 @kdl_dizi_al_tam64(ptr, i32)\n", out);
    fputs("declare ptr @kdl_dizi_al_ptr(ptr, i32)\n", out);
    fputs("declare void @kdl_dizi_yaz_tam(ptr, i32, i32)\n", out);
    fputs("declare void @kdl_dizi_yaz_tam64(ptr, i32, i64)\n", out);
    fputs("declare void @kdl_dizi_yaz_ptr(ptr, i32, ptr)\n", out);
    /* D-087: by-value yapı (struct) elemanlı dizi — memcpy tabanlı */
    fputs("declare void @kdl_dizi_ekle_yapi(ptr, ptr, ptr)\n", out);  /* V2-F4.2a: ρ */
    fputs("declare void @kdl_dizi_al_yapi(ptr, i32, ptr)\n", out);
    fputs("declare void @kdl_dizi_yaz_yapi(ptr, i32, ptr)\n", out);
    fputs("declare i32 @kdl_dizi_boyut(ptr)\n", out);
    /* Adim 6: capacity API */
    fputs("declare i32 @kdl_dizi_kapasite(ptr)\n", out);
    fputs("declare void @kdl_dizi_kapasite_ayarla(ptr, ptr, i32)\n", out);  /* V2-F4.2a: ρ */
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
    /* D-268 OUT-PTR ABI: olustur/delege dönüşü AÇIK out-pointer (düz `ptr` ilk arg,
     * aarch64 x0). sret DEĞİL — çünkü %kdl_yetki 16B (≤ AAPCS64/SysV eşiği) ve clang
     * bare-metal'de register-return ediyor; eski sret çağrısı o provider'la uyuşmuyordu
     * (yetki değeri kullanılmadığı için maskeliydi). Out-ptr, saf-.kem sağlayıcının
     * (`çıplak fn(out: *KdlYetki,...)`) emit ettiği imzayla BİREBİR eşleşir (Yasa-4). */
    fputs("declare void @kdl_yetki_olustur(ptr, i16, i16)\n", out);
    fputs("declare void @kdl_yetki_delege(ptr, ptr, i16)\n", out);
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
        else if (uye->tip == DUGUM_DEGISKEN && uye->veri.degisken.kuresel_mi) {
            kuresel_kayit(&g, uye);   /* D-252 küresel değişken */
        }
    }

    /* Pre-pass: metinleri topla */
    ast_taransa_metinleri(&g, program);

    /* Emit module-basi: yapi tip tanimlari + string globalleri */
    yapi_tip_tanimlari_emit(&g);
    /* D-307: per-instantiation generic tipler — ön-geçişte (ast_taransa_metinleri)
     * keşfedilenler burada emit (fonksiyonlardan ÖNCE → alloca sized). Fonksiyon/
     * lambda gövdelerinde keşfedilen geç örnekler sondaki çağrıda (dedup'lı). */
    mono_tip_tanimlari_emit(&g);
    str_globalleri_emit(&g);
    /* D-252: küresel değişken module-globalleri (@ad = internal global <ir> <init>). */
    for (SabitKayit *ku = g.kureseller; ku; ku = ku->sonraki) {
        const char *ir = ku->tip ? ast_tip_to_ir(&g, ku->tip) : "i32";
        if (!ir) ir = "i32";
        ad_yaz(g.out, "@", 1);
        yerel_ad_yaz(g.out, ku->ad, ku->ad_uz);
        fprintf(g.out, " = internal global %s ", ir);
        kuresel_init_yaz(g.out, ku->deger, ir);
        fputc('\n', g.out);
    }

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

    /* D-307: per-instantiation generic yapı/çeşit tipleri (fonksiyonlarda
     * keşfedilir → burada emit; LLVM adlı-tipleri modül-genelinde çözer,
     * forward-ref güvenli). Lambdalardan SONRA — lambda gövdeleri de generic
     * örneklendirme keşfedebilir. */
    mono_tip_tanimlari_emit(&g);

    int hatalar = g.hata_sayisi;
    arena_serbest(a);
    return hatalar;
}
