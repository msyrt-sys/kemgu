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
 * === ESC_ITERASYON SAGLAMLIK GERI-CEKILMESI (loop-carried; D-101) ===
 *
 * ESC_ITERASYON omru EN KISA bolgedir (rho_iterasyon <= rho_yerel). Gelecekte
 * (F4.3) iterasyon-basina serbest birakilacagi icin, iterasyonu ASAN bir tahsisi
 * ITERASYON saymak = canliyken serbest = UAF. Iterasyon-yerelligi SAGLAM tespit
 * etmek, kaccis rotalarini kapatan kapilara KOSULLUYDU:
 *   - D-007 (diziler skaler-eleman) -> dis agregaya REFERANS saklanamaz,
 *   - R-GOMME (gomme yok) -> kaccan agregada gomulu heap-ref yok.
 * ANCAK bu kapilar su an ENFORCE EDILMIYOR: `Dizi<Dizi<T>>`, `Dizi<metin>` ve Dizi
 * alanli yapi tip-kontrolden gecer ve by-ref `KdlDizi*` olarak lower edilir. Boylece
 * `dis[i] = tahsis` (DUGUM_INDEKS lvalue) ve `nesne.alan = tahsis` (DUGUM_ERISIM
 * lvalue) bir dongu-tahsisini iterasyondan kaccirir; sentaktik tespit bunu kaccirinca
 * under-approximation = gizli UAF olurdu.
 *
 * KARAR (guvenli geri-cekilme): escape analizi HICBIR tahsisi ESC_ITERASYON
 * URETMEZ; dongu icindeki tahsisler dahil HEPSI ESC_YEREL (daha uzun omurlu =
 * guvenli). ESC_ITERASYON enum'u API/gelecek icin KORUNUR ama bu analiz tarafindan
 * atanmaz. Per-iterasyon optimizasyonu, gercek bolge-serbest semantigi (F4.3)
 * geldiginde -- kapilar enforce edilince ya da TUM kaccis rotalari (ver / daha-sig
 * degisken / agrega-lvalue store / agrega-gomme / cagri / closure) kapsaninca --
 * saglamca eklenecek. Bu sayede UAF su an IMKANSIZ ve soundness arguman'i TRIVIAL.
 *
 * Mevcut sinirlamalar (v1):
 *   - ESC_ITERASYON uretilmez (yukaridaki geri-cekilme; F4.3'te saglamca eklenir)
 *   - Inter-procedural yok (cagri sonuclari konservatif: yerel)
 *   - Closure yakalama henuz islenmiyor (G005 kaccan closure'i ayrica reddeder)
 *   - Yapi/dizi elemanlarinin alt-escape'i takip edilmiyor (alanin escape'i
 *     parent allocation'a yansir, ama tersine yansima yok)
 */

typedef enum {
    ESC_YEREL,        /* lokal (escape etmez) — default; dongu tahsisleri DAHIL (D-101) */
    ESC_ITERASYON,    /* iterasyon-yerel; rho_iterasyon (EN KISA omur). API/gelecek icin
                       * KORUNUR; D-101 geri-cekilmesiyle bu analiz tarafindan URETILMEZ. */
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
