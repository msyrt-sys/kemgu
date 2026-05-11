#ifndef KEMGU_BOLGE_ATAMA_H
#define KEMGU_BOLGE_ATAMA_H

#include "ast.h"
#include "bolge.h"
#include "arena.h"

/*
 * KEMGU Bolge Atama (Region Inference)
 * =====================================
 *
 * AST visitor: her ifadenin bolgesini belirler. R-* aksiyomlarini uygular
 * (KEMGU_Bellek_Modeli.md, Katman 1).
 *
 * R-LIT:       basit literal → BOLGE_LIT (stack, bolge yok)
 * R-YEREL:     escape etmeyen bilesik deger → BOLGE_YEREL(f)
 * R-VER:       'ver' ile donen deger → BOLGE_CAGIRAN(f)
 * R-ITERASYON: donguden escape etmeyen deger → BOLGE_ITERASYON(d)
 * R-KOSUL:     koşullu dallanma → iki dalin LCA'si
 *
 * ADIM E: Tam escape analizi (genisletilmis DFA)
 * ==============================================
 *
 *  - Sembol-bolge haritasi (lokal degisken -> bolge takibi)
 *  - Atama dataflow: deger bolgesi -> hedef sembol bolgesi
 *  - VER ihlal tespiti: yerel adresi (&yerel) ver -> hata
 *  - Hata raporlama (B001 yerel-adres-ver, B002 bolge-ihlali-atama)
 */

#define BOLGE_ATAMA_MAX_SEMBOL  256

typedef struct {
    const char *ad;
    int ad_uzunluk;
    BolgeBilgisi *bolge;
} BolgeSembol;

typedef struct BolgeAtama {
    Arena *arena;
    const char *islev_adi;       /* aktif islev adi (yerel/cagiran icin) */
    int islev_adi_uz;
    int dongu_derinligi;          /* 0 = dongu disi */
    int dongu_id_sayaci;          /* tekil id uretici */
    BolgeBilgisi *aktif_iterasyon; /* aktif dongu bolgesi */
    int ver_baglaminda;           /* 1 = ver icinde (R-VER aktif) */

    /* E.1: Sembol-bolge haritasi (scope watermark ile) */
    BolgeSembol semboller[BOLGE_ATAMA_MAX_SEMBOL];
    int sembol_sayi;

    /* E.2: Hata raporu */
    int hata_sayisi;
    const char *dosya_adi;
    const char *kaynak;
} BolgeAtama;

void bolge_atama_baslat(BolgeAtama *ba, Arena *a,
                        const char *islev_adi, int uz);

/* Hata raporlama icin dosya/kaynak ayarla (opsiyonel) */
void bolge_atama_kaynak_ayarla(BolgeAtama *ba,
                               const char *dosya_adi, const char *kaynak);

/* Ifadenin bolgesini belirle. NULL parametresi guvenli (NULL doner).
 * Yan etki: deyim/atama analizinde sembol haritasi guncellenir,
 * ihlal tespit edilirse ba->hata_sayisi artar. */
BolgeBilgisi *bolge_belirle(BolgeAtama *ba, const Dugum *ifade);

#endif /* KEMGU_BOLGE_ATAMA_H */
