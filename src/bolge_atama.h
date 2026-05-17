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
 * NOT: Tam escape analizi (DFA) ileride. Su an basit context-tracking:
 *   - ver_baglaminda: deger ver icinde mi (R-VER aktif)
 *   - dongu_derinligi: dongu icinde miyiz (R-ITERASYON aktif)
 */

typedef struct BolgeAtama {
    Arena *arena;
    const char *islev_adi;       /* aktif islev adi (yerel/cagiran icin) */
    int islev_adi_uz;
    int dongu_derinligi;          /* 0 = dongu disi */
    int dongu_id_sayaci;          /* tekil id uretici */
    BolgeBilgisi *aktif_iterasyon; /* aktif dongu bolgesi */
    int ver_baglaminda;           /* 1 = ver icinde (R-VER aktif) */
    /* Katman 2: concurrency */
    int thread_id_sayaci;         /* yeni gorev (task) id'leri */
    int kanal_id_sayaci;          /* yeni kanal id'leri */
    BolgeBilgisi *aktif_gorev;    /* gorev govdesi icindeysek bolgesi */
} BolgeAtama;

void bolge_atama_baslat(BolgeAtama *ba, Arena *a,
                        const char *islev_adi, int uz);

/* Ifadenin bolgesini belirle. NULL parametresi guvenli (NULL doner). */
BolgeBilgisi *bolge_belirle(BolgeAtama *ba, const Dugum *ifade);

#endif /* KEMGU_BOLGE_ATAMA_H */
