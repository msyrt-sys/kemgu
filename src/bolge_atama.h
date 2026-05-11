#ifndef KEMGU_BOLGE_ATAMA_H
#define KEMGU_BOLGE_ATAMA_H

#include "ast.h"
#include "bolge.h"
#include "arena.h"
#include "escape.h"

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
 * Iki mod:
 *   1) Syntax-tabanli (escape = NULL): ver_baglaminda flag'i ile karar.
 *      Eski API uyumlu; transitif escape'i yakalamaz.
 *
 *   2) Escape-tabanli (escape ayarli): DFA fixed-point analizinden
 *      escape kategorisi bilgisi alir. Transitif akis dogrulanabilir:
 *      degisken x = "..."; ver x; senaryosunda "..." CAGIRAN olur.
 */

typedef struct BolgeAtama {
    Arena *arena;
    const char *islev_adi;       /* aktif islev adi (yerel/cagiran icin) */
    int islev_adi_uz;
    int dongu_derinligi;          /* 0 = dongu disi */
    int dongu_id_sayaci;          /* tekil id uretici */
    BolgeBilgisi *aktif_iterasyon; /* aktif dongu bolgesi */
    int ver_baglaminda;           /* 1 = ver icinde (R-VER aktif) — fallback */
    const EscapeAnaliz *escape;   /* opsiyonel: DFA escape analizi sonuclari */
} BolgeAtama;

void bolge_atama_baslat(BolgeAtama *ba, Arena *a,
                        const char *islev_adi, int uz);

/* Escape analizi sonuclarini bagla (NULL ile syntax moduna geri don). */
void bolge_atama_escape_bagla(BolgeAtama *ba, const EscapeAnaliz *ea);

/* Ifadenin bolgesini belirle. NULL parametresi guvenli (NULL doner). */
BolgeBilgisi *bolge_belirle(BolgeAtama *ba, const Dugum *ifade);

#endif /* KEMGU_BOLGE_ATAMA_H */
