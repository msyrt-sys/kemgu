#ifndef KEMGU_ESCAPE_H
#define KEMGU_ESCAPE_H

#include "arena.h"
#include "ast.h"

/*
 * KEMGU Escape Analizi (DFA Fixed-Point)
 * =======================================
 *
 * KEMGU_Bellek_Modeli.md, Katman 3:
 *
 *   ESC-0 (Kacmaz):     scope'tan cikmaz -> rho_yerel veya rho_iterasyon
 *   ESC-1 (Cagirana):   ver ile doner -> rho_cagiran
 *   ESC-2 (Belirsiz):   gomulme, yakalama, kosullu dal -> baglam-duyarli
 *
 * Algoritma: forward data-flow analizi, fixed-point iterasyonu.
 *
 *   Pass 1: Tum tahsis bolgelerini topla (metin, dizi_olustur, yapi_olustur,
 *           lambda, cagri sonucu). Default: ESC_YEREL.
 *
 *   Pass 2: Degisken bag haritalari ile birlikte AST'yi tara. Her 'ver e' icin
 *           e'nin altinda yatan tahsis bolgelerini bul, ESC_CAGIRAN olarak isaretle.
 *           Atamalar: x = e ise, x'in eski baglamasinin escape kategorisini
 *           e'ye yay (transitif).
 *
 *   Pass 3: Hicbir kategori degismeyene kadar Pass 2'yi tekrarla.
 *
 * Bu kademe, bolge_atama.c'nin yapamayacagi su senaryolari yakalar:
 *
 *   degisken x = "merhaba"; ver x;       // "merhaba" -> CAGIRAN (transitif)
 *   degisken y = [1,2,3]; ver y[0];      // y -> YEREL (ic eleman basit kopya)
 *   eger k { ver x; } degilse { ... }    // x -> CAGIRAN (kosullu yol)
 *
 * Mevcut sinirlamalar (v1):
 *   - Inter-procedural yok (cagri sonuclari konservatif: yerel)
 *   - Closure yakalama henuz islenmiyor
 *   - Yapi/dizi elemanlarinin alt-escape'i takip edilmiyor (alanin escape'i
 *     parent allocation'a yansir, ama tersine yansima yok)
 */

typedef enum {
    ESC_YEREL,        /* lokal (escape etmez) — default */
    ESC_ITERASYON,    /* dongu iterasyonunda olusur, fonksiyondan escape etmez */
    ESC_CAGIRAN,      /* ver ile cagirana escape eder */
} EscapeKategorisi;

typedef struct EscapeKayit {
    const Dugum *dugum;        /* AST dugumu (tahsis bolgesi) */
    EscapeKategorisi kategori;
    int dongu_derinligi;       /* tahsis edildigi dongu derinligi (0 = islev seviyesi) */
} EscapeKayit;

typedef struct EscapeBag {
    const char *ad;            /* degisken adi (kopyalanmaz — AST ile aynı omur) */
    int ad_uz;
    const Dugum *deger;        /* baglandigi ifade (allocation site veya baska ifade) */
    int scope_seviye;          /* hangi scope'ta tanimlandi (pop icin) */
} EscapeBag;

typedef struct EscapeAnaliz {
    Arena *arena;

    /* Tahsis bolgesi -> escape kategorisi haritasi (lineer arama) */
    EscapeKayit *kayitlar;
    int kayit_sayi;
    int kayit_kapasite;

    /* Aktif degisken baglamalari (scope stack — sondan basa arama) */
    EscapeBag *baglamalar;
    int bag_sayi;
    int bag_kapasite;

    /* Pass durumu */
    int scope_seviye;
    int dongu_derinligi;
    int ver_baglaminda;
    int degisti;               /* bu pass'ta kategori degisti mi? */
} EscapeAnaliz;

void escape_baslat(EscapeAnaliz *ea, Arena *a);

/* Dahili malloc/realloc edilmis tablolari serbest birak. */
void escape_serbest(EscapeAnaliz *ea);

/* Tum islev govdesini fixed-point'e kadar analiz et. */
void escape_analiz_islev(EscapeAnaliz *ea, const Dugum *islev);

/* Genel AST'yi (program/modul/islev listesi) gez ve her isleve analiz uygula. */
void escape_analiz_program(EscapeAnaliz *ea, const Dugum *program);

/* Bir AST dugumunun escape kategorisini sorgula.
 * Kayitsiz dugumler icin ESC_YEREL doner. */
EscapeKategorisi escape_kategori(const EscapeAnaliz *ea, const Dugum *d);

/* Tahsis bolgesinin dongu derinligi (0 = islev seviyesi). */
int escape_dongu_derinligi(const EscapeAnaliz *ea, const Dugum *d);

/* Yazdirma — debug */
const char *escape_kategori_adi(EscapeKategorisi k);

#endif /* KEMGU_ESCAPE_H */
