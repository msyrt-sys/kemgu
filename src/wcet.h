#ifndef KEMGU_WCET_H
#define KEMGU_WCET_H

#include "ast.h"
#include "sembol.h"
#include "arena.h"

#include <stdint.h>

/*
 * KEMGU Realtime Spec V1 — WCET Hesapla + RT001-RT005 Denetimi
 * =============================================================
 *
 * Bu modul 'gercekzamanli isleve' isaretli her islev govdesi icin:
 *   1) RT001-RT005 statik dogrulama (yasak yapilar)
 *   2) Worst-Case Execution Time (cycle cinsi) hesabi
 *
 * AST visitor + cost tablosu (belgeler/KEMGU_Realtime_Spec_V1.md RT.7.1).
 * Hatalar 'hata_raporla' yoluyla raporlanir; ayrica wk->hata_sayisi artar.
 *
 * Hata kodlari:
 *   RT001 — REALTIME_DYNAMIC_ALLOC      (Dizi literal, lambda, alloc)
 *   RT002 — REALTIME_UNBOUNDED_LOOP     (V1: tum iken/icin yasak)
 *   RT003 — REALTIME_UNBOUNDED_RECURSION (V1: tum direct self-call yasak)
 *   RT004 — REALTIME_CALLS_NONRT        (gercekzamanli olmayan cagri)
 *   RT005 — REALTIME_WCET_UNKNOWN       (cagrilan bilinmeyen sembol)
 *   RT006 — parser hatasi (modifier duplicate) — bkz parser.c
 *   RT007 — satirici_asm 'cevrim:' anotasyonu eksik (C5 C.3 — opak
 *           asm maliyeti gerçekzamanlı baglamda acikca bildirilmeli)
 *
 * V1 sinirlamalari:
 *   - Loop yok (RT002 her zaman). V2: bounded loop annotation.
 *   - Direct self-recursion yasak (RT003). V2: max_depth annotation.
 *   - Mutual recursion algilanmiyor (a->b->a kacirilir).
 *   - Cagrilan callee WCET'i otomatik hesaplanmiyor (V1: sabit 50 cycle).
 */

#define WCET_MAX 1000000000LL

/* [D-545] Cagri zinciri kesif derinligi. Asilirsa RT005 (WCET hesaplanamaz)
 * — SESSIZ ATLAMA DEGIL: bilinmeyen sinir bildirilir (default-DENY). */
#define WCET_ZINCIR_AZAMI 32

typedef struct WcetKontrol {
    Arena *arena;
    Scope *global_scope;
    int hata_sayisi;
    const char *dosya_adi;
    const char *kaynak;
    /* Aktif realtime islev — direct self-recursion algilamak icin. */
    const Dugum *aktif_islev;
    /* [D-545] KARSILIKLI/DOLAYLI OZYINELEME: RT003 yalniz DOGRUDAN
     * self-call'i yakaliyordu; a->b->a zinciri --check'ten TEMIZ geciyor ve
     * program HIC DURMUYORDU (olculdu: exit 124). Cagri zinciri bir yiginda
     * tutulur; zaten yigindaki bir isleve donen cagri RT003'tur.
     * YENI TANI KODU YOK — RT003'un mesaji zaten "ozyineleme (V1 yasak)"
     * diyor; kural SORULMADIGI yerde soruluyor (D-503/D-517 deseni). */
    const Dugum *cagri_yigin[WCET_ZINCIR_AZAMI];
    int cagri_yigin_n;
    int sessiz;          /* ic (kesif) yuruyusunde tani BASILMAZ */
    int dongu_bulundu;   /* ic yuruyuste zincir kapandi mi */
} WcetKontrol;

void wcet_kontrol_baslat(WcetKontrol *wk, Arena *a, Scope *global,
                         const char *dosya_adi, const char *kaynak);

/* Programi dolas, her 'gercekzamanli' isarettli islevi denetle.
 * Modul ve uygula gövdelerindeki islevler de dahil. */
void wcet_kontrol_program(WcetKontrol *wk, const Dugum *program);

/* Tek bir islev icin WCET hesabi (gercekzamanli kabul).
 * RT001-RT005 hatasi durumunda -1, aksi takdirde cycle sayisi. */
int64_t wcet_islev_hesapla(WcetKontrol *wk, const Dugum *islev);

#endif /* KEMGU_WCET_H */
