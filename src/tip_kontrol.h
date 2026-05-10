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
 *
 * Hatalar 'hata_raporla' ile stderr'e yazilir, hata_sayisi artirilir.
 * Ifade tipi belirlenemezse TIP_HATA doner — caller bu tipi gormezden gelmeli.
 */

typedef struct TipKontrol {
    Arena *arena;
    Scope *scope;             /* mevcut scope */
    Scope *global_scope;      /* yapi/islev tanimlari icin (ileri referans) */
    int hata_sayisi;
    const char *dosya_adi;
    const char *kaynak;
} TipKontrol;

void tip_kontrol_baslat(TipKontrol *tk, Arena *a, Scope *global,
                        const char *dosya_adi, const char *kaynak);

/* Ifadenin tipini belirle. Hata varsa raporla, TIP_HATA donebilir. */
TipBilgisi *tip_belirle(TipKontrol *tk, const Dugum *ifade);

/* AST tip dugumunu (DUGUM_TIP_*) TipBilgisi'ye cevir.
 * Built-in tipler (tam32 vs) ve kullanici tipleri (sembol arama) icin. */
TipBilgisi *ast_tip_to_bilgi(TipKontrol *tk, const Dugum *tip_d);

/* Hata raporlama */
void tip_hata(TipKontrol *tk, const Dugum *d,
              const char *kod, const char *mesaj);

#endif /* KEMGU_TIP_KONTROL_H */
