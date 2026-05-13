#ifndef KEMGU_JSON_H
#define KEMGU_JSON_H

#include "arena.h"

#include <stddef.h>
#include <stdio.h>

/*
 * KEMGU Minimal JSON Parser
 * ==========================
 *
 * LSP server icin gerekli temel JSON parse + olusturma. Kapsam:
 *   - object {}, array [], string "...", number, true/false/null
 *   - Escape: \" \\ \n \r \t (diger \uXXXX henuz yok)
 *   - UTF-8 byte aktarimi (Turkce karakterler korunur)
 *
 * Parse cikti: agac yapisi (arena allocator ile).
 * Yazma: dinamik genisleyen tampon (malloc).
 */

typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_TAMSAYI,
    JSON_KESIRLI,
    JSON_METIN,
    JSON_DIZI,
    JSON_NESNE,
} JsonTipi;

typedef struct JsonDeger JsonDeger;
typedef struct JsonAlan JsonAlan;

struct JsonAlan {
    const char *ad;          /* arena'da null-terminated kopya */
    int ad_uz;
    JsonDeger *deger;
    JsonAlan *sonraki;
};

struct JsonDeger {
    JsonTipi tip;
    union {
        int bool_deger;          /* 0 veya 1 */
        long long tamsayi;
        double kesirli;
        struct {
            const char *metin;   /* arena'da kopya, null-terminated */
            int uzunluk;
        } str;
        struct {
            JsonDeger **elemanlar;
            int sayi;
        } dizi;
        struct {
            JsonAlan *bas;
            int alan_sayi;
        } nesne;
    } veri;
};

/* === Parsing === */

/* Kaynaktan JSON parse et. Hata olursa NULL doner ve *out_hata'ya kisa mesaj
 * yazilir (arena'da). out_hata NULL gecilebilir. */
JsonDeger *json_ayrist(Arena *a, const char *kaynak, int uzunluk,
                        const char **out_hata);

/* === Sorgu === */

/* Nesnenin alan'ini ad ile bul. NULL = bulunamadi veya nesne degil. */
JsonDeger *json_alan(const JsonDeger *nesne, const char *ad);

/* Metin degeri. Tip metin degilse NULL doner. */
const char *json_metin(const JsonDeger *d, int *out_uz);

/* Tamsayi. Tip tamsayi/kesirli degilse 0. */
long long json_tamsayi(const JsonDeger *d);

/* Dizi yardimcilari */
int json_dizi_sayi(const JsonDeger *d);
JsonDeger *json_dizi_eleman(const JsonDeger *d, int i);

/* === Yazma (dinamik tampon) === */

typedef struct JsonYazici {
    char *tampon;
    size_t kullanilan;
    size_t kapasite;
} JsonYazici;

void json_yazici_baslat(JsonYazici *y);
void json_yazici_serbest(JsonYazici *y);

/* Yazici icine yaz. Buyume gerekirse realloc. */
void json_yaz(JsonYazici *y, const char *s);
void json_yaz_n(JsonYazici *y, const char *s, size_t n);
void json_yaz_int(JsonYazici *y, long long v);

/* JSON metin literali olarak yaz (escape edilmis, tirnaklarla cevrili). */
void json_yaz_metin_lit(JsonYazici *y, const char *s);
void json_yaz_metin_lit_n(JsonYazici *y, const char *s, size_t n);

#endif /* KEMGU_JSON_H */
