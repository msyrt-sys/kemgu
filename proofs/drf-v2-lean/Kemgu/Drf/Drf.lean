/-
KEMGU DRF Mekanize — Teorem 4' Statik DRF V1 (Onarim v3 F2)
Kaynak (kagit formel): belgeler/KEMGU_DRF_Teoremi.md §3 Ana Teorem
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz, sorry/axiom YOK

F2: same-Step teoremi step_iz_analiz'den turetilir (her adim en fazla
bir yeni olay ekler → iki yeni memYaz ayni olaydir → ayni thread).
-/

import Kemgu.Sem.Core
import Kemgu.Sem.SmallStep
import Kemgu.Sem.Kopru
import Kemgu.Drf.L0BolgeKorunumu
import Kemgu.Drf.L1BolgeThreadTekilligi
import Kemgu.Drf.L2LinearMoveCrossThread
import Kemgu.Drf.L3LinearClosureSoundness
import Kemgu.Drf.L4FrozenRegionRead
import Kemgu.Drf.L5KanalAtomikTransfer
import Kemgu.Drf.L6CapabilityLinear
import Kemgu.Drf.L7BellekErisimTipSoundness

namespace Kemgu.Drf.Drf
open Kemgu.Sem.Core Kemgu.Sem.SmallStep Kemgu.Sem.Kopru
open Kemgu.Drf.L0BolgeKorunumu Kemgu.Drf.L4FrozenRegionRead

-- ============================================================
-- §1. Data race tanim altyapisi (Op.Sem §6.5)
-- ============================================================

def is_memYaz : Olay → Prop
  | .memYaz _ _ _ => True
  | _ => False

def olay_thread : Olay → Option ThreadId
  | .memOku t _ _ => some t
  | .memYaz t _ _ => some t
  | .threadBaslat t => some t
  | .threadBitir t => some t
  | .kanalGonderOl t _ _ => some t
  | .kanalAlOl t _ _ => some t
  | .dondurOl t _ => some t

def olay_konum : Olay → Option Konum
  | .memOku _ k _ => some k
  | .memYaz _ k _ => some k
  | _ => none

def is_data_race_candidate (e1 e2 : Olay) : Prop :=
  ∃ t1 t2 k1 k2,
    olay_thread e1 = some t1 ∧
    olay_thread e2 = some t2 ∧
    t1 ≠ t2 ∧
    olay_konum e1 = some k1 ∧
    olay_konum e2 = some k2 ∧
    k1 = k2 ∧
    (is_memYaz e1 ∨ is_memYaz e2)

def has_data_race (S : Konfigurasyon) : Prop :=
  ∃ e1 ∈ S.iz, ∃ e2 ∈ S.iz, is_data_race_candidate e1 e2

-- ============================================================
-- §2. Teorem 4' tam form — same-Step DRF
-- ============================================================

/-- TEOREM 4' V1 (Same-Step DRF, F2 formu):
    Tek Step icinde iki YENI memYaz event'i ayni thread'tendir
    (her adim en fazla bir yeni olay ekler — step_iz_analiz). -/
theorem kemgu_drf_v1_no_concurrent_writes
    (S S' : Konfigurasyon) (h_step : Step S S')
    (t1 t2 : ThreadId) (k1 k2 : Konum) (v1 v2 : Deger)
    (h_event1 : Olay.memYaz t1 k1 v1 ∈ S'.iz)
    (h_event1_new : Olay.memYaz t1 k1 v1 ∉ S.iz)
    (h_event2 : Olay.memYaz t2 k2 v2 ∈ S'.iz)
    (h_event2_new : Olay.memYaz t2 k2 v2 ∉ S.iz) :
    t1 = t2 := by
  rcases step_iz_analiz S S' h_step with
      ⟨h_iz, _, _⟩
    | ⟨t', k', v', h_iz, _, _⟩
    | ⟨t', k', v', h_iz, _, _, _⟩
    | ⟨olay, h_iz, h_nyaz, _, _⟩
  · rw [h_iz] at h_event1
    exact absurd h_event1 h_event1_new
  · rw [h_iz] at h_event1
    rcases List.mem_cons.mp h_event1 with h_head | h_tail
    · nomatch h_head
    · exact absurd h_tail h_event1_new
  · rw [h_iz] at h_event1 h_event2
    rcases List.mem_cons.mp h_event1 with h1_head | h1_tail
    · rcases List.mem_cons.mp h_event2 with h2_head | h2_tail
      · injection h1_head with h_t1 _ _
        injection h2_head with h_t2 _ _
        rw [h_t1, h_t2]
      · exact absurd h2_tail h_event2_new
    · exact absurd h1_tail h_event1_new
  · rw [h_iz] at h_event1
    rcases List.mem_cons.mp h_event1 with h_head | h_tail
    · exact absurd h_head.symm (h_nyaz t1 k1 v1)
    · exact absurd h_tail h_event1_new

/-- TEOREM 4' V1 bundled — S1 invariant StepStar korunumu
    (DRF lemmalarinin birlesik formu; cross-Step HB V2.1 hedefi). -/
theorem kemgu_drf_v1_bundled
    (Pi : Program) (h_iyi : IyiTipli Pi)
    (S₀ S : Konfigurasyon) (h_run : StepStar S₀ S)
    (h_init_s1 : s1_invariant S₀) :
    s1_invariant S :=
  drf_l0_bolge_korunumu_starStep Pi h_iyi S₀ S h_run h_init_s1

-- ============================================================
-- §3. Sinirlar ve V2 hedefleri
-- ============================================================

/-
TEOREM 4' tam "∀ τ ∈ Tr(Π) : ¬ data_race(τ)" formu icin V2 hedefleri:
(1) Cross-Step HB ordering (sequenced-before ∪ synchronizes-with kapanisi);
(2) data_race tam formu (¬ HB-sirali kosulu ile);
(3) F2 ile memOku event'leri MEKANIZE OLDU (sVarOku) — read-race kapsami
    icin altyapi hazir; cross-Step argumani kaldi.
-/

end Kemgu.Drf.Drf
