#ifndef KEMGU_AST_H
#define KEMGU_AST_H

#include "arena.h"

#include <stddef.h>
#include <stdint.h>

/*
 * KEMGU Abstract Syntax Tree
 * ===========================
 *
 * Tagged union tasarimi: tek `Dugum` tipi, icindeki `tip` alani hangi union
 * dali aktif oldugunu belirler. Tum dugumler arena ile tahsis edilir.
 *
 * Konum bilgisi (satir/sutun) her dugumde tasinir — hata mesajlari icin.
 *
 * String alanlari (tanimlayici, metin literali, vs.) arena'ya kopyalanir
 * ve null-terminated saklanir. Lexer kaynak buffer'i bittiginde de AST yasar.
 */

/* === Dugum tipleri (~46 adet) === */

typedef enum {
    /* Ust duzey */
    DUGUM_PROGRAM,
    DUGUM_MODUL,
    DUGUM_KULLAN,
    DUGUM_DISA,

    /* Tanimlar */
    DUGUM_ISLEV,
    DUGUM_YAPI,
    DUGUM_CESIT,           /* çeşit Ad { A, B, C } — C2.7 sum type (payloadsuz v1) */
    DUGUM_OZELLIK,
    DUGUM_UYGULA,
    DUGUM_SABIT,
    DUGUM_PARAMETRE,
    DUGUM_ALAN,            /* yapi alani: ad: tip; */

    /* Deyimler */
    DUGUM_DEGISKEN,        /* degisken x = e; */
    DUGUM_ATAMA,           /* x = e; */
    DUGUM_VER,             /* ver e; */
    DUGUM_EGER,
    DUGUM_IKEN,
    DUGUM_ICIN,
    DUGUM_ESLES,
    DUGUM_GUVENSIZ,
    DUGUM_SATIRICI_ASM,    /* satıriçi_asm { ... } — C5 inline assembly */
    DUGUM_BLOK,
    DUGUM_IFADE_DEYIMI,    /* sadece ifade ; (ornegin f(); ) */

    /* Ifadeler */
    DUGUM_IKILI,
    DUGUM_TEKLI,
    DUGUM_CAGRI,
    DUGUM_ERISIM,          /* x.y */
    DUGUM_INDEKS,          /* x[i] */
    DUGUM_YOL,             /* x::y */
    DUGUM_LAMBDA,
    DUGUM_YAPI_OLUSTUR,
    DUGUM_DIZI_OLUSTUR,
    DUGUM_ALAN_ATAMA,      /* yapi_olustur icinde "ad: ifade" */
    DUGUM_KULLAN_IFADE,    /* kullan(e) — Linear Types Spec V1 (extract) */
    DUGUM_IMHA_IFADE,      /* imha(e)   — Linear Types Spec V1 (dispose) */
    DUGUM_TIP_DONUSTUR,    /* x olarak T — Madde E: explicit cast */

    /* Literaller */
    DUGUM_TAM,
    DUGUM_KESIRLI,
    DUGUM_METIN,
    DUGUM_KARAKTER,
    DUGUM_MANTIKSAL,
    DUGUM_BOS,
    DUGUM_TANIMLAYICI,

    /* Tipler */
    DUGUM_TIP_BASIT,       /* tam32, metin, vs. */
    DUGUM_TIP_REFERANS,    /* &T veya &degisken T */
    DUGUM_TIP_POINTER,     /* *T (guvensiz blokta) */
    DUGUM_TIP_DIZI,        /* Dizi<T> */
    DUGUM_TIP_SECIMLIK,    /* secimlik<T> */
    DUGUM_TIP_SONUC,       /* sonuc<T,H> */
    DUGUM_TIP_ISLEV,       /* islev(...) -> T */
    DUGUM_TIP_KULLANICI,   /* modul::Tip<T1,T2> */
    DUGUM_TIP_TEKKEZ,      /* tekkez<T> — Linear Types Spec V1 */
    DUGUM_TIP_SABITSURE,   /* sabitsüre<T> — Sabitsüre Spec V1 (constant-time) */
    DUGUM_TIP_YETKI,       /* yetki<R> — Capability Spec V1 (object-capability) */
    DUGUM_TIP_VEKTOR,      /* vektör<T, N> — SIMD Spec V1 */
    DUGUM_TIP_GOREV,       /* görev<T> — Concurrency / DRF V1 */
    DUGUM_TIP_KANAL,       /* kanal<T> — Concurrency / DRF V1 */

    /* Desenler (esles icin) */
    DUGUM_DESEN_LITERAL,
    DUGUM_DESEN_TANIMLAYICI,
    DUGUM_DESEN_YAPICI,    /* TipAdi(alt_desen, ...) */
    DUGUM_DESEN_YOL,       /* Cesit::Varyant — C2.7 (payloadsuz varyant deseni) */
    DUGUM_DESEN_JOKER,     /* _ */
    DUGUM_ESLES_KOLU,      /* desen => blok/ifade */

    /* Ozel */
    DUGUM_HATA,            /* error recovery placeholder */
} DugumTipi;

/* === Operator (CLAUDE.md ile birebir) === */

typedef enum {
    /* Ikili */
    OP_ARTI,           /* + */
    OP_EKSI,           /* - */
    OP_CARPI,          /* * */
    OP_BOLU,           /* / */
    OP_MOD,            /* % */
    OP_ESIT,           /* == */
    OP_ESIT_DEGIL,     /* != */
    OP_KUCUK,          /* < */
    OP_BUYUK,          /* > */
    OP_KUCUK_ESIT,     /* <= */
    OP_BUYUK_ESIT,     /* >= */
    OP_VE,             /* ve */
    OP_VEYA,           /* veya */

    /* Bit operatorleri (ikili) — page table / kripto kodu icin */
    OP_BIT_VE,         /* &  bitwise AND */
    OP_BIT_VEYA,       /* |  bitwise OR */
    OP_BIT_OZVEYA,     /* ^  bitwise XOR */
    OP_SOLA_KAYDIR,    /* << logical shift left */
    OP_SAGA_KAYDIR,    /* >> arithmetic shift right (signed) / logical (unsigned) */

    /* Tekli */
    OP_NEG,            /* -x */
    OP_DEGIL,          /* degil x */
    OP_BIT_DEGIL,      /* ~x bitwise NOT */
    OP_REF,            /* &x */
    OP_REF_DEGISKEN,   /* &degisken x */
    OP_DEREFERANS,     /* *x */
} Operator;

/* === Tek-gecis ad cozumu (resolver binding) ===
 *
 * Resolver (tip_kontrol.c) ad-referansi dugumlerine (DUGUM_TANIMLAYICI,
 * DUGUM_YOL) kazanan sembolu ve kategorisini YAZAR; codegen (llvm.c)
 * adlari string'le yeniden COZMEZ, bu kaydi TUKETIR. Boylece tip kontrol
 * ile codegen insa geregi ayni sembole anlasir (onceki sapma: tip kontrol
 * module-first/lexical, codegen global-first cozuyordu).
 *
 * dugum_olustur arena_ayir_sifir kullandigi icin varsayilan deger
 * COZUM_YOK / NULL — resolver kosmamis AST'lerde (or. dogrudan
 * llvm_ir_uret cagrilari) codegen eski string yoluna duser (graceful
 * degradation). Built-in'ler (yazdir, dizi_ekle, tekkez_olustur, ...)
 * sembol tablosunda olmadigi icin COZUM_YOK kalir — codegen'in built-in
 * eslemeleri etkilenmez. */

struct Sembol;  /* sembol.h — circular include yerine forward decl
                   (sembol.h ast.h'yi include eder) */

typedef enum {
    COZUM_YOK = 0,      /* resolver yazmadi (varsayilan) */
    COZUM_YEREL,        /* islev/blok scope — lokal degisken/parametre */
    COZUM_MODUL_UYESI,  /* modul uyesi — mangling oneki cozum_modul_onek */
    COZUM_GLOBAL,       /* global scope tanimi */
} CozumKategorisi;

/* === Dugum yapisi (tagged union) === */

typedef struct Dugum Dugum;

struct Dugum {
    DugumTipi tip;
    int satir;             /* 1'den baslar */
    int sutun;             /* 1'den baslar (UTF-8 byte konumu) */

    /* Tek-gecis ad cozumu binding'i (yalniz ad-referansi dugumlerinde
     * dolu). cozum_modul_onek COZUM_MODUL_UYESI icin "m1.m2" formatinda
     * noktali mangling onekidir (llvm.c modul_mangle ile ayni sema). */
    const struct Sembol *cozum_sembol;
    CozumKategorisi cozum_kategori;
    const char *cozum_modul_onek;
    int cozum_modul_onek_uz;

    union {
        /* === Ust duzey === */

        struct {
            Dugum **uyeler;
            int sayi;
        } program;

        struct {
            const char *ad;
            int ad_uzunluk;
            Dugum **uyeler;
            int sayi;
            /* Çok-dosya modül A: 1 = bu modül bir .kem dosyasından
             * yüklendi (loader sentetik sarmalayıcısı). Görünürlük
             * (genel) yalnız dosya-modüllerinde uygulanır; dosya-içi
             * modüller geriye-uyumlu (tüm üyeler görünür). */
            int dosya_modulu;
        } modul;

        struct {
            const char *yol;       /* "x::y::z" tek string */
            int yol_uzunluk;
            /* Çok-dosya modül A — yeni içe-aktarma biçimleri:
             *   kullan dizi;                  -> nitelikli erişim bağı
             *   kullan dizi::{Liste, ekle};   -> seçili adlar niteliksiz
             *   kullan dizi olarak d;         -> alias (d::...)
             * segment_sayi: yol'daki ad sayısı (1 = yeni namespaced
             * yükleme; >1 + seçili/alias yok = legacy düzleştirme). */
            int segment_sayi;
            char **secili_adlar;       /* NULL = seçili liste yok */
            int *secili_uzunluklar;
            int secili_sayi;
            const char *alias_ad;      /* NULL = alias yok */
            int alias_ad_uz;
        } kullan;

        struct {
            Dugum *tanim;          /* dısa edilen tanim */
        } disa;

        /* === Tanimlar === */

        struct {
            const char *ad;
            int ad_uzunluk;
            /* Generic tip parametreleri: islev<T, U: Bound>(...) */
            char **tip_paramlar;
            int tip_param_sayi;
            Dugum ***tip_param_boundlari;
            int *tip_param_bound_sayilari;
            Dugum **parametreler;  /* DUGUM_PARAMETRE listesi */
            int param_sayi;
            Dugum *donus_tipi;     /* NULL = donus yok */
            Dugum *govde;          /* DUGUM_BLOK veya NULL (sadece imza) */
            int gercekzamanli_mi;  /* Realtime Spec V1 — hard real-time qualifier */
            int genel_mi;          /* A: 1 = 'genel' (çapraz-modül export) */
            int ciplak_mi;         /* D-254: 'çıplak işlev' — region-prologue YOK
                                    * (no @kdl_bolge_olustur/@kdl_global_bolge_al,
                                    * no ρ param); WALL-2 bootstrap çözümü. */
        } islev;

        struct {
            const char *ad;
            int ad_uzunluk;
            char **tip_paramlar;   /* generic tip parametre adlari (null-term) */
            int tip_param_sayi;
            /* Bound listeleri (paralel):
             *   tip_param_boundlari[i] = Dugum* dizisi (i. parametrenin bound listesi)
             *   tip_param_bound_sayilari[i] = i. parametre icin bound sayisi
             * NULL veya 0 = bound yok. */
            Dugum ***tip_param_boundlari;
            int *tip_param_bound_sayilari;
            Dugum **alanlar;       /* DUGUM_ALAN listesi */
            int alan_sayi;
            int genel_mi;          /* A: 1 = 'genel' (çapraz-modül export) */
        } yapi;

        /* C2.7: çeşit Ad { A, B(t1,t2), C } — isimli varyant kümesi (sum type).
         * Varyant indeksi = bildirim sırası (0'dan); discriminant tag.
         * Payload (C3): her varyant tipli alanlar taşıyabilir — V(t1, t2).
         * Paralel diziler (bound deseni gibi): payloadsuz varyant sayı 0. */
        struct {
            const char *ad;
            int ad_uzunluk;
            char **varyantlar;          /* varyant adları (arena) */
            int *varyant_uzunluklar;    /* her varyantın byte uzunluğu */
            int varyant_sayi;
            int genel_mi;          /* A: 1 = 'genel' (çapraz-modül export) */
            /* Generic çeşit (C-only, D-302): tip parametreleri (yapı ile aynı
             * paralel-dizi deseni). NULL/0 = generic olmayan (eski davranış). */
            char **tip_paramlar;
            int tip_param_sayi;
            /* C3 payload: [varyant][alan] tip düğümü; [varyant] alan sayısı.
             * NULL/0 (payloadsuz çeşit) eski davranışla aynı (bare iN disc). */
            Dugum ***varyant_payload_tipleri; /* [i] = i. varyantın tip düğüm dizisi */
            int *varyant_payload_sayilari;    /* [i] = i. varyantın alan sayısı (0=yok) */
        } cesit;

        struct {
            const char *ad;
            int ad_uzunluk;
            char **tip_paramlar;
            int tip_param_sayi;
            Dugum ***tip_param_boundlari;
            int *tip_param_bound_sayilari;
            Dugum **uyeler;        /* islev imzalari/tanimlari */
            int uye_sayi;
        } ozellik;

        struct {
            char **tip_paramlar;
            int tip_param_sayi;
            Dugum ***tip_param_boundlari;
            int *tip_param_bound_sayilari;
            Dugum *tip;            /* uygulanacak tip */
            Dugum **ozellikler;    /* ozellik yollari (DUGUM_TIP_KULLANICI) */
            int ozellik_sayi;
            Dugum **islevler;
            int islev_sayi;
        } uygula;

        struct {
            const char *ad;
            int ad_uzunluk;
            Dugum *tip;
            Dugum *deger;
            int genel_mi;          /* A: 1 = 'genel' (çapraz-modül export) */
        } sabit;

        struct {
            const char *ad;
            int ad_uzunluk;
            Dugum *tip;
            int kendin_mi;       /* 1 = self parametresi (uygula gövdesinde) */
            int referans_mi;     /* 1 = &kendin */
            int degisken_mi;     /* 1 = &değişken kendin */
        } parametre;

        struct {
            const char *ad;
            int ad_uzunluk;
            Dugum *tip;
            int genel_mi;          /* A: 1 = 'genel' (alan export — D'de kullanılır) */
        } alan;

        /* === Deyimler === */

        struct {
            const char *ad;
            int ad_uzunluk;
            Dugum *tip;            /* opsiyonel */
            Dugum *deger;
            int kuresel_mi;        /* D-252: 1 = modül-düzeyi mutable global (küresel değişken) */
        } degisken;

        struct {
            Dugum *hedef;          /* atama_hedefi */
            Dugum *deger;
        } atama;

        struct {
            Dugum *deger;          /* NULL = ver; */
        } ver;

        struct {
            Dugum *kosul;
            Dugum *gozdoldur;      /* eger dali (DUGUM_BLOK) */
            Dugum *yan;            /* degilse: BLOK, baska EGER, veya NULL */
        } eger;

        struct {
            Dugum *kosul;
            Dugum *govde;
        } iken;

        struct {
            const char *degisken_adi;
            int degisken_adi_uzunluk;
            Dugum *koleksiyon;
            Dugum *govde;
        } icin;

        struct {
            Dugum *deger;
            Dugum **kollar;        /* DUGUM_ESLES_KOLU listesi */
            int kol_sayi;
        } esles;

        struct {
            const char *aciklama_ad;     /* opsiyonel: guvensiz [etiket: "..."] */
            int aciklama_ad_uzunluk;
            const char *aciklama_metin;
            int aciklama_metin_uzunluk;
            Dugum *blok;
        } guvensiz;

        /* C5: satıriçi_asm { mimari: x86_64  şablon: r#"..."#
         *       çıktı("=r", &v)  girdi("r", e)  bozulan("~{cc}")
         *       çevrim: 3 }
         * Kısıt degerleri ham LLVM/GCC string (Türkçeleştirilmez);
         * yüzey sözcükleri Türkçe. Paralel dizi deseni (bkz. cesit). */
        struct {
            const char *mimari;  int mimari_uz;   /* arch-tag (zorunlu) */
            const char *sablon;  int sablon_uz;   /* asm template (zorunlu) */
            /* çıktı("kısıt", &ad): her çıktı bir &değişken lvalue'ya yazar */
            char **cikti_kisitlar; int *cikti_kisit_uzlar;
            char **cikti_adlar;    int *cikti_ad_uzlar;
            int cikti_sayi;
            /* girdi("kısıt", ifade) */
            char **girdi_kisitlar; int *girdi_kisit_uzlar;
            Dugum **girdi_ifadeler;
            int girdi_sayi;
            /* bozulan("kısıt") — clobber listesi */
            char **bozulanlar; int *bozulan_uzlar;
            int bozulan_sayi;
            int64_t cevrim;    /* -1 = anotasyon yok (gerçekzamanlı'da RT007) */
        } satirici_asm;

        struct {
            Dugum **deyimler;
            int sayi;
        } blok;

        struct {
            Dugum *ifade;
        } ifade_deyimi;

        /* === Ifadeler === */

        struct {
            Operator op;
            Dugum *sol, *sag;
        } ikili;

        struct {
            Operator op;
            Dugum *operand;
        } tekli;

        struct {
            Dugum *hedef;          /* cagrilan ifade */
            Dugum **argumanlar;
            int sayi;
        } cagri;

        struct {
            Dugum *nesne;
            const char *alan;
            int alan_uzunluk;
        } erisim;

        struct {
            Dugum *nesne;
            Dugum *indeks;
        } indeks;

        struct {
            Dugum *sol;            /* TANIMLAYICI veya baska YOL */
            const char *sag_ad;
            int sag_ad_uzunluk;
        } yol;

        struct {
            Dugum **parametreler;
            int param_sayi;
            Dugum *govde;          /* blok veya tek ifade */
        } lambda;

        struct {
            const char *tip_ad;
            int tip_ad_uzunluk;
            Dugum **alanlar;       /* DUGUM_ALAN_ATAMA listesi */
            int alan_sayi;
        } yapi_olustur;

        struct {
            Dugum **elemanlar;
            int sayi;
        } dizi_olustur;

        struct {
            const char *ad;
            int ad_uzunluk;
            Dugum *deger;
        } alan_atama;

        /* === Literaller === */

        struct { int64_t deger; } tam;
        struct { double deger; } kesirli;
        struct { const char *metin; int uzunluk; } metin_lit;
        struct { uint32_t kod_noktasi; } karakter;
        struct { int deger; /* 0 veya 1 */ } mantiksal;
        /* DUGUM_BOS — veri yok */
        struct { const char *metin; int uzunluk; } tanimlayici;

        /* === Tipler === */

        struct {
            const char *ad;
            int ad_uzunluk;
        } tip_basit;               /* tam32, metin, ... */

        struct {
            int degisken_mi;       /* &degisken T ise 1 */
            Dugum *hedef_tip;
        } tip_referans;

        struct {
            Dugum *hedef_tip;
        } tip_pointer;

        struct {
            Dugum *eleman_tip;
        } tip_dizi;

        struct {
            Dugum *ic_tip;
        } tip_secimlik;

        struct {
            Dugum *deger_tip;
            Dugum *hata_tip;
        } tip_sonuc;

        struct {
            Dugum **parametreler;  /* tip listesi */
            int param_sayi;
            Dugum *donus_tip;
        } tip_islev;

        struct {
            Dugum *yol;            /* DUGUM_TANIMLAYICI veya DUGUM_YOL */
            Dugum **tip_arg;       /* generic argumanlar (tip listesi) */
            int tip_arg_sayi;
        } tip_kullanici;

        struct {
            Dugum *ic_tip;
        } tip_tekkez;              /* tekkez<T> — Linear Types Spec V1 */

        struct {
            Dugum *ic_tip;
        } tip_sabitsure;           /* sabitsüre<T> — Sabitsüre Spec V1 */

        struct {
            Dugum *kaynak_tipi;    /* R: DUGUM_TIP_BASIT veya DUGUM_TIP_KULLANICI
                                      (Dosya/Soket/Bellek/Donanim/OTP_Anahtar) */
        } tip_yetki;               /* yetki<R> — Capability Spec V1 */

        struct {
            Dugum *eleman_tip;     /* T (element tipi — basit/dtam/kesirli/mantıksal) */
            int lane_sayi;         /* N: 2,4,8,16,32,64 (compile-time literal) */
        } tip_vektor;              /* vektör<T, N> — SIMD Spec V1 */

        struct {
            Dugum *ic_tip;         /* T — thread'in dönüş tipi (görev<T>) */
        } tip_gorev;               /* görev<T> — Concurrency / DRF V1 */

        struct {
            Dugum *ic_tip;         /* T — kanaldan geçen mesaj tipi (kanal<T>) */
        } tip_kanal;               /* kanal<T> — Concurrency / DRF V1 */

        struct {
            Dugum *operand;
        } kullan_ifade;            /* kullan(e) — extract */

        struct {
            Dugum *operand;
        } imha_ifade;              /* imha(e) — dispose */

        struct {
            Dugum *kaynak;         /* dönüştürülecek ifade */
            Dugum *hedef_tip;      /* DUGUM_TIP_* */
        } tip_donustur;            /* x olarak T — explicit cast */

        /* === Desenler === */

        struct {
            Dugum *deger;          /* DUGUM_TAM, DUGUM_METIN vb. */
        } desen_literal;

        struct {
            const char *ad;
            int ad_uzunluk;
        } desen_tanimlayici;

        struct {
            const char *ad;        /* yapici adi */
            int ad_uzunluk;
            Dugum **alt_desenler;
            int sayi;
        } desen_yapici;

        /* C2.7: Cesit::Varyant deseni; C3: payload bağlama Cesit::V(a, b). */
        struct {
            const char *cesit_ad; int cesit_uz;
            const char *varyant_ad; int varyant_uz;
            Dugum **alt_desenler;   /* C3: payload alt-desenleri (NULL=payloadsuz) */
            int alt_sayi;           /* alt-desen sayısı (0=payloadsuz) */
        } desen_yol;

        /* DUGUM_DESEN_JOKER — veri yok */

        struct {
            Dugum *desen;
            Dugum *govde;          /* blok veya tek ifade */
        } esles_kolu;
    } veri;
};

/* === API === */

/* Enum -> okunabilir ad (yazdirma/debug icin) */
const char *dugum_tipi_adi(DugumTipi tip);
const char *operator_adi(Operator op);

/* Generic dugum olusturucu (arena_ayir_sifir kullanir).
 * Tip + konum doldurur; veri alanini cagiran kendi doldurur. */
Dugum *dugum_olustur(Arena *a, DugumTipi tip, int satir, int sutun);

/* String'i arena'ya kopyala + null-terminate. NULL veya 0 uzunluk -> NULL. */
char *ast_string_kopyala(Arena *a, const char *kaynak, int uzunluk);

/* === Sik kullanilan yardimci olusturucular === */
/* Diger ~30 dugum tipi icin parser fazinda gerektikce eklenir. */

Dugum *dugum_tam(Arena *a, int64_t deger, int satir, int sutun);
Dugum *dugum_kesirli(Arena *a, double deger, int satir, int sutun);
Dugum *dugum_metin(Arena *a, const char *metin, int uzunluk, int satir, int sutun);
Dugum *dugum_karakter(Arena *a, uint32_t kod_noktasi, int satir, int sutun);
Dugum *dugum_mantiksal(Arena *a, int deger, int satir, int sutun);
Dugum *dugum_bos(Arena *a, int satir, int sutun);
Dugum *dugum_tanimlayici(Arena *a, const char *metin, int uzunluk,
                         int satir, int sutun);

Dugum *dugum_ikili(Arena *a, Operator op, Dugum *sol, Dugum *sag,
                   int satir, int sutun);
Dugum *dugum_tekli(Arena *a, Operator op, Dugum *operand,
                   int satir, int sutun);

Dugum *dugum_blok(Arena *a, Dugum **deyimler, int sayi, int satir, int sutun);
Dugum *dugum_program(Arena *a, Dugum **uyeler, int sayi, int satir, int sutun);
Dugum *dugum_eger(Arena *a, Dugum *kosul, Dugum *gozdoldur, Dugum *yan,
                  int satir, int sutun);
Dugum *dugum_ver(Arena *a, Dugum *deger, int satir, int sutun);

Dugum *dugum_hata(Arena *a, int satir, int sutun);

#endif /* KEMGU_AST_H */
