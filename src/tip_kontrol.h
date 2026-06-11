#ifndef KEMGU_TIP_KONTROL_H
#define KEMGU_TIP_KONTROL_H

#include "ast.h"
#include "tip.h"
#include "sembol.h"
#include "arena.h"

/*
 * KEMGU Tip Kontrolu
 * ==================
 *
 * AST visitor pattern. Her ifade dugumu icin tip belirler.
 *
 * Hata kodlari (T001-T020):
 *   T001: tip uyumsuzlugu (genel)
 *   T002: tanimsiz sembol
 *   T003: sayisal tip bekleniyor
 *   T004: mantiksal tip bekleniyor
 *   T005: tamsayi tip bekleniyor
 *   T006: islev tipi bekleniyor (cagri icin)
 *   T007: yapi tipi bekleniyor (alan erisimi icin)
 *   T008: dizi tipi bekleniyor (indeks icin)
 *   T009: alan bulunamadi (yapi.alan)
 *   T010: arguman sayisi uyumsuz
 *   T011: bilinmeyen tip (ast tip dugumu cevirme)
 *   T012: yapi olusturmasi alan eksik/fazla
 *   T013: dizi elemanlari farkli tipte
 *   T014: bos dizi context bekleniyor (ADIM 11.5)
 *   T015: lambda parametre tip annot eksik
 *   T016: modul/yol cozumlemesi basarisiz
 *   T017: yapi olusturmada bilinmeyen alan
 *   T030: generic tip argumani bound'u karsilamiyor (constraint violation)
 *   T031: ozellik bilinmiyor (bound olarak verilen ad cozulemedi)
 *   G001: *T dereferans guvensiz blok disinda (C5 on-kosul #2)
 *   G002: satiriçi_asm guvensiz blok disinda (C5)
 *   AS002: asm operandi uygunsuz tip — yalniz kopyalanabilir primitif
 *          (tamN, dtamN, mantiksal, karakter, ham *T); tekkez/yetki
 *          dogrudan gecemez, cikti lineer olamaz (C5 C.1 kara kutu)
 *
 * Hatalar 'hata_raporla' ile stderr'e yazilir, hata_sayisi artirilir.
 * Ifade tipi belirlenemezse TIP_HATA doner — caller bu tipi gormezden gelmeli.
 */

/* Yüklenmiş modül izleme (cycle + duplicate detection) */
typedef struct YuklenmisModul {
    char *yol;             /* "stdlib/temel/matematik.kem" gibi */
    int yol_uz;
    struct YuklenmisModul *sonraki;
} YuklenmisModul;

typedef struct TipKontrol {
    Arena *arena;
    Scope *scope;                  /* mevcut scope */
    Scope *global_scope;           /* yapi/islev tanimlari icin (ileri referans) */
    TipBilgisi *aktif_donus_tipi;  /* aktif islev gövdesi içinde 'ver' icin */
    UygulaTablosu uygulamalar;     /* (Tip, Ozellik) -> impl registry */
    YuklenmisModul *yuklenmisler;  /* duplicate-load engelleme */
    int hata_sayisi;
    const char *dosya_adi;
    const char *kaynak;
    /* === Linear Types Spec V1 takibi === */
    int scope_seviyesi;            /* mevcut scope derinligi (lineer omur kontrolu) */
    int lambda_govdesi_icinde;     /* >0 = lambda govdesi visit ediyoruz */
    int lambda_lineer_yakalama;    /* >0 = lambda lineer baglama yakaladi
                                      (closure-itself-linear icin) */
    Scope *lambda_baslangic_scope; /* lambda govdesi disinda kalan scope sınırı */
    /* === C5: unsafe-context bayragi === */
    int guvensiz_baglam;           /* >0 = guvensiz blok icindeyiz (derinlik).
                                      *T dereferans (G001) ve satiriçi_asm (G002)
                                      yalniz guvensiz baglamda gecerli. */
} TipKontrol;

void tip_kontrol_baslat(TipKontrol *tk, Arena *a, Scope *global,
                        const char *dosya_adi, const char *kaynak);

/* Ifadenin tipini belirle. Hata varsa raporla, TIP_HATA donebilir. */
TipBilgisi *tip_belirle(TipKontrol *tk, const Dugum *ifade);

/* Bidirectional tip cikarsamasi: 'beklenen' tip parametre olarak verilirse
 * literal'lar (TAM, KESIRLI, bos dizi) o tipe gore cikarsanir.
 *   degisken x: tam8 = 1;       // 1 -> tam8 (default tam32 yerine)
 *   degisken xs: Dizi<T> = [];  // bos dizi -> Dizi<T>
 *   f(arg)  // arg, parametre tipi context'inde cikarsanir
 * beklenen NULL ise tip_belirle ile ayni davranir. */
TipBilgisi *tip_belirle_beklenen(TipKontrol *tk, const Dugum *ifade,
                                  const TipBilgisi *beklenen);

/* AST tip dugumunu (DUGUM_TIP_*) TipBilgisi'ye cevir.
 * Built-in tipler (tam32 vs) ve kullanici tipleri (sembol arama) icin. */
TipBilgisi *ast_tip_to_bilgi(TipKontrol *tk, const Dugum *tip_d);

/* Tum programi tip kontrol et (iki gecis: pre-populate + govde kontrol). */
void tip_kontrol_program(TipKontrol *tk, const Dugum *program);

/* Hata raporlama */
void tip_hata(TipKontrol *tk, const Dugum *d,
              const char *kod, const char *mesaj);

#endif /* KEMGU_TIP_KONTROL_H */
