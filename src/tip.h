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

    /* === Linear (affine) — Direktif Ek v1 B grubu === */
    TIP_TEKKEZ,        /* tekkez<T> — en fazla bir kez tüketilir */

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
            TipBilgisi *ic;        /* tekkez<T> -> T */
        } tekkez;
    } veri;
};

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

/* Linear (tekkez) tipi mi? */
int tip_tekkez_mi(const TipBilgisi *t);

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

#endif /* KEMGU_TIP_H */
