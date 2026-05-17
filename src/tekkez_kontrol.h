#ifndef KEMGU_TEKKEZ_KONTROL_H
#define KEMGU_TEKKEZ_KONTROL_H

#include "ast.h"
#include "tip.h"
#include "sembol.h"
#include "arena.h"

#include <stdio.h>

/*
 * KEMGU Tekkez (Linear / Affine) Tip Kontrolu
 * ============================================
 *
 * Direktif Ek v1 — Bolum B (Linear Types Meta-Spec v1).
 *
 * Affine semantik:
 *   - En fazla bir kez tuketilir (kullan(x) veya imha(x))
 *   - Asla sessizce scope'tan dusmez
 *   - Tuketim olmadan scope sonu = hata
 *
 * Hata kodlari:
 *   T040 — linear deger iki kez tuketildi
 *   T041 — linear deger hic tuketilmedi (scope sonunda)
 *   T042 — linear deger '_' ile baglandi (sessiz drop)
 *   T043 — linear deger LIT region'a atandi (yasak)
 *
 * Calistirma: tip_kontrol_program sonrasi tekkez_kontrol_program.
 * Feature flag: --experimental-linear (yoksa atlanir).
 */

typedef enum {
    LIN_TANIMSIZ,         /* henuz analiz edilmemis */
    LIN_AKTIF,            /* tanimli, henuz tuketilmemis */
    LIN_TUKETILDI,        /* kullan() veya imha() ile bitti */
} LinDurumu;

/* Linear deger izleme — sym table'a paralel bir state */
typedef struct LinKayit {
    const char *ad;
    int ad_uz;
    LinDurumu durum;
    int satir, sutun;     /* tanim konumu */
    struct LinKayit *sonraki;
} LinKayit;

typedef struct TekKezKontrol {
    Arena *arena;
    Scope *global_scope;
    int hata_sayisi;
    const char *dosya_adi;
    const char *kaynak;
    /* Scope-yapili linear izleme */
    LinKayit *aktif;          /* aktif kayitlarin linked list head */
    /* Feature flag */
    int aktif_mi;             /* 1 = --experimental-linear */
} TekKezKontrol;

/* Public API */

void tekkez_kontrol_baslat(TekKezKontrol *tk, Arena *a, Scope *g,
                            const char *dosya_adi, const char *kaynak);

/* Tum programi tara, linearity hatalarini raporla */
void tekkez_kontrol_program(TekKezKontrol *tk, const Dugum *prog);

#endif /* KEMGU_TEKKEZ_KONTROL_H */
