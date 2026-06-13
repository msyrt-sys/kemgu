#ifndef KEMGU_TIP_H
#define KEMGU_TIP_H

#include "arena.h"

#include <stddef.h>
#include <stdio.h>

/*
 * KEMGU Tip Sistemi
 * =================
 *
 * Tagged union tip temsili. Built-in basit tipler + bilesik tipler
 * (referans, pointer, dizi, secimlik, sonuc, islev, yapi).
 *
 * Esitlik: NOMINAL (Rust/Java tarzi). Aynı isimli tipler esit, farkli
 * isimli ama ayni alanli tipler farkli.
 *
 * Built-in tipler her seferinde yeniden tahsis edilir (singleton degil).
 * Equality 'tip_esit' ile recursive karsilastirma. Pointer karsilastirma
 * yetersiz (ayni tip iki kere tahsis edilebilir).
 *
 * Tum tahsisler arena'dan.
 */

typedef enum {
    /* === Tamsayilar === */
    TIP_TAM8,
    TIP_TAM16,
    TIP_TAM32,
    TIP_TAM64,
    /* Isaretsiz */
    TIP_DTAM8,
    TIP_DTAM16,
    TIP_DTAM32,
    TIP_DTAM64,

    /* === Kesirli === */
    TIP_KESIRLI32,
    TIP_KESIRLI64,

    /* === Diger basit === */
    TIP_MANTIKSAL,
    TIP_KARAKTER,
    TIP_METIN,
    TIP_BOS,

    /* === Bilesik === */
    TIP_REFERANS,    /* &T veya &degisken T */
    TIP_POINTER,     /* *T */
    TIP_DIZI,        /* Dizi<T> */
    TIP_SECIMLIK,    /* secimlik<T> */
    TIP_SONUC,       /* sonuc<T,H> */
    TIP_ISLEV,       /* islev(T1, T2) -> T */
    TIP_YAPI,        /* yapi X veya X<T1, T2> */
    TIP_TEKKEZ,      /* tekkez<T> — Linear Types Spec V1 */
    TIP_SABITSURE,   /* sabitsüre<T> — Sabitsüre Spec V1 (constant-time) */
    TIP_YETKI,       /* yetki<R> — Capability Spec V1 (object-capability) */
    TIP_VEKTOR,      /* vektör<T, N> — SIMD Spec V1 */
    TIP_GOREV,       /* görev<T> — Concurrency / DRF V1 */
    TIP_KANAL,       /* kanal<T> — Concurrency / DRF V1 */

    /* === Generic === */
    TIP_GENERIC_PARAM,  /* T (yapı/islev içinde tip parametresi) */

    /* === Cikarsamada === */
    TIP_BILINMIYOR,  /* henuz cikarsanmamis */

    /* === Hata === */
    TIP_HATA,        /* tip kontrol hatasi yer tutucu */
} TipKategorisi;

typedef struct TipBilgisi TipBilgisi;

struct TipBilgisi {
    TipKategorisi kategori;
    union {
        struct {
            int degisken_mi;        /* &degisken T ise 1 */
            TipBilgisi *hedef;
        } referans;

        struct {
            TipBilgisi *hedef;
        } pointer;

        struct {
            TipBilgisi *eleman;
        } dizi;

        struct {
            TipBilgisi *ic;
        } secimlik;

        struct {
            TipBilgisi *deger;
            TipBilgisi *hata;
        } sonuc;

        struct {
            TipBilgisi **parametreler;
            int param_sayi;
            TipBilgisi *donus;
            int gercekzamanli_mi;     /* Realtime Spec V1 — hard real-time qualifier */
        } islev;

        struct {
            const char *ad;
            int ad_uzunluk;
            /* Generic instantiation argumanlari (yoksa 0) */
            TipBilgisi **tip_arg;
            int tip_arg_sayi;
        } yapi;

        struct {
            const char *ad;
            int ad_uzunluk;
        } generic_param;

        struct {
            TipBilgisi *ic;
        } tekkez;                 /* Linear Types Spec V1 */

        struct {
            TipBilgisi *ic;
        } sabitsure;              /* Sabitsüre Spec V1 — constant-time qualifier */

        struct {
            TipBilgisi *kaynak;   /* R: kaynak tipi (Dosya, Soket, vs.) */
        } yetki;                  /* Capability Spec V1 — object-capability */

        struct {
            TipBilgisi *eleman;
            int lane_sayi;        /* N: 2,4,8,16,32,64 */
        } vektor;                 /* SIMD Spec V1 — vektör<T, N> */

        struct {
            TipBilgisi *ic;       /* T — thread'in dönüş tipi */
        } gorev;                  /* Concurrency / DRF V1 — görev<T> */

        struct {
            TipBilgisi *ic;       /* T — kanaldan geçen mesaj tipi */
        } kanal;                  /* Concurrency / DRF V1 — kanal<T> */
    } veri;
};

/* === Linear Types Spec V1: lineer mi? ===
 * tekkez<T> ve tekkez<...> sarilan herhangi bir tip lineer sayilir. */
int tip_lineer_mi(const TipBilgisi *t);

/* === Sabitsüre Spec V1: tip sabitsüre (gizli) mi? ===
 * sabitsüre<T> tipi gizli (secret) sayılır; constant-time disiplin gerekir. */
int tip_sabitsure_mi(const TipBilgisi *t);

/* Tip 'T' sabitsüre<...> sarılabilir mi? (CT-WRAP yetenekli)
 * Yetenekli: tamX, dtamX, karakter, mantıksal, Dizi<yetenekli>.
 * Yasak: kesirli32/64, metin, yapı (V1), seçimlik, sonuç, işlev, tekkez,
 * sabitsüre (nesting redundancy), referans, pointer. */
int tip_sabitsure_yetenekli_mi(const TipBilgisi *t);

/* Bir T → T' ifadesi/atamasında: hedef T' beklenirken kaynak T sabitsüre ise
 * implicit downgrade ihlali. Helper: 'kaynak'tan 'hedef'e otomatik geçirilebilir mi?
 * - Kaynak ve hedef ikisi de sabitsüre: tip_esit kullan.
 * - Kaynak T, hedef sabitsüre<T>: V1'de explicit sabitsüre_olustur zorunlu, 0 doner.
 * - Kaynak sabitsüre<T>, hedef T: HER ZAMAN 0 (CT003 leak).
 * - İkisi de normal: tip_esit.
 * Bu helper return:
 *   1 = uyumlu (no error)
 *   0 = uyumsuz (caller hata raporlar) */
int tip_sabitsure_uyumlu_mu(const TipBilgisi *kaynak, const TipBilgisi *hedef);

/* === Olusturucular === */

TipBilgisi *tip_olustur_basit(Arena *a, TipKategorisi k);
TipBilgisi *tip_olustur_referans(Arena *a, TipBilgisi *hedef, int degisken_mi);
TipBilgisi *tip_olustur_pointer(Arena *a, TipBilgisi *hedef);
TipBilgisi *tip_olustur_dizi(Arena *a, TipBilgisi *eleman);
TipBilgisi *tip_olustur_secimlik(Arena *a, TipBilgisi *ic);
TipBilgisi *tip_olustur_sonuc(Arena *a, TipBilgisi *deger, TipBilgisi *hata);
TipBilgisi *tip_olustur_islev(Arena *a, TipBilgisi **params, int param_sayi,
                               TipBilgisi *donus);
TipBilgisi *tip_olustur_yapi(Arena *a, const char *ad, int ad_uzunluk,
                              TipBilgisi **tip_arg, int tip_arg_sayi);
TipBilgisi *tip_olustur_generic_param(Arena *a, const char *ad, int ad_uzunluk);
TipBilgisi *tip_olustur_tekkez(Arena *a, TipBilgisi *ic);
TipBilgisi *tip_olustur_sabitsure(Arena *a, TipBilgisi *ic);
TipBilgisi *tip_olustur_yetki(Arena *a, TipBilgisi *kaynak);
TipBilgisi *tip_olustur_vektor(Arena *a, TipBilgisi *eleman, int lane_sayi);
TipBilgisi *tip_olustur_gorev(Arena *a, TipBilgisi *ic);
TipBilgisi *tip_olustur_kanal(Arena *a, TipBilgisi *ic);

/* === Concurrency / DRF V1 helper === */
/* görev<T> mü? */
int tip_gorev_mu(const TipBilgisi *t);
/* kanal<T> mü? */
int tip_kanal_mu(const TipBilgisi *t);

/* === Capability Spec V1 helper === */
/* yetki<R> mi? (TIP_YETKI veya iceren tekkez<yetki<R>>) */
int tip_yetki_mi(const TipBilgisi *t);
/* yetki<R> ise R'yi don. Aksi NULL. */
const TipBilgisi *tip_yetki_kaynak(const TipBilgisi *t);

/* === Iliskiler === */

/* Nominal esitlik (recursive). NULL guvenli. */
int tip_esit(const TipBilgisi *a, const TipBilgisi *b);

/* === Yazdirma === */

void tip_yazdir(const TipBilgisi *t, FILE *out);
const char *tip_kategorisi_adi(TipKategorisi k);

/* === Yardimcilar === */

/* Sayisal mi? (toplama, cikarma vs icin uygun) */
int tip_sayisal_mi(const TipBilgisi *t);
/* Tamsayi mi? (bit op, mod icin) */
int tip_tamsayi_mi(const TipBilgisi *t);
/* Mantiksal mi? */
int tip_mantiksal_mi(const TipBilgisi *t);

/* === SIMD Spec V1: vektör helpers === */

/* Bir tipin vektör-yetenekli skaler eleman tipi olup olmadığını döner.
 * Yetenekli: tamX, dtamX, kesirli32/64, mantıksal.
 * Yasak: karakter, metin, yapı, referans, pointer, seçimlik, sonuç, işlev,
 *        tekkez, sabitsüre, dizi, vektör (nested). */
int tip_vektor_eleman_yetenekli_mi(const TipBilgisi *t);

/* N (lane sayısı) izinli {2, 4, 8, 16, 32, 64} setinde mi? */
int tip_vektor_lane_gecerli_mi(int n);

/* Tip vektör mü? */
int tip_vektor_mu(const TipBilgisi *t);

#endif /* KEMGU_TIP_H */
