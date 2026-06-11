/-
KEMGU DRF Mekanize — DRF-L4 Frozen Region Read-Soundness (Onarim v3 F2)
Kaynak (kagit formel): belgeler/KEMGU_DRF_Lemmalar.md §DRF-L4
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz, sorry/axiom YOK

F2: step_iz_analiz gorunum lemmasindan turetilir (eski 15-case analiz yerine
4-disjunct rcases). isFrozen persistence artik KOSULSUZ (step_donmus_korunur).
-/

import Kemgu.Sem.Core
import Kemgu.Sem.SmallStep

namespace Kemgu.Drf.L4FrozenRegionRead
open Kemgu.Sem.Core Kemgu.Sem.SmallStep

-- ============================================================
-- §1. DRF-L4 (a) — Tek-adim formulasyon
-- ============================================================

/-- DRF-L4 (a): frozen bolgeye hicbir adim yeni memYaz ekleyemez.
    F2 ispati: step_iz_analiz'in yazma disjunct'inda yazan thread hedefin
    GUNCEL sahibi (thread t) — frozen (donmus) ile celisir. -/
theorem drf_l4_a_step
    (S S' : Konfigurasyon) (h_step : Step S S')
    (b : Bolge) (h_frozen : isFrozen S b)
    (t : ThreadId) (k : Konum) (v : Deger)
    (h_in : Olay.memYaz t k v ∈ S'.iz) :
    Olay.memYaz t k v ∈ S.iz ∨ k.bolge ≠ b := by
  rcases step_iz_analiz S S' h_step with
      ⟨h_iz, _, _⟩
    | ⟨t', k', v', h_iz, _, _⟩
    | ⟨t', k', v', h_iz, _, _, h_owner⟩
    | ⟨olay, h_iz, h_nyaz, _, _⟩
  · left; rwa [h_iz] at h_in
  · rw [h_iz] at h_in
    rcases List.mem_cons.mp h_in with h_head | h_tail
    · nomatch h_head
    · left; exact h_tail
  · rw [h_iz] at h_in
    rcases List.mem_cons.mp h_in with h_head | h_tail
    · injection h_head with h_t h_k h_v
      subst h_t h_k
      right
      intro h_eq
      rw [h_eq] at h_owner
      have h_celiski := h_frozen.symm.trans h_owner
      nomatch h_celiski
    · left; exact h_tail
  · rw [h_iz] at h_in
    rcases List.mem_cons.mp h_in with h_head | h_tail
    · exact absurd h_head.symm (h_nyaz t k v)
    · left; exact h_tail

-- ============================================================
-- §2. DRF-L4 (b) — Iki read race olusturmaz
-- ============================================================

/-- DRF-L4 (b) — iki mem_oku event'i data race olusturmaz (tanim geregi:
    data_race en az bir yazma ister). F2 NOT: sVarOku artik memOku emit
    ediyor — read-coverage gercek (eski "hic memOku yok" vacuity kalkti). -/
theorem drf_l4_b_oku_not_yaz
    (t : ThreadId) (k : Konum) (v : Deger) :
    Olay.memOku t k v ≠ Olay.memYaz t k v := by
  intro h
  nomatch h

-- ============================================================
-- §3. Persistence: isFrozen Step altinda KOSULSUZ korunur (F2)
-- ============================================================

/-- isFrozen tek-adim persistence — F2'de KOSULSUZ (eski conditional
    isFrozen_persistent_simple'in yerini alir). Cekirdek:
    SmallStep.step_donmus_korunur (sahiplik yazan kurallar guncel sahibi
    thread/kanal olan bolgelerle sinirli → donmus override edilemez). -/
theorem isFrozen_persistent
    (S S' : Konfigurasyon) (h_step : Step S S')
    (b : Bolge) (h_frozen : isFrozen S b) :
    isFrozen S' b :=
  step_donmus_korunur S S' h_step b h_frozen

/-- isFrozen StepStar persistence (corollary). -/
theorem isFrozen_persistent_star
    (S S' : Konfigurasyon) (h_run : StepStar S S')
    (b : Bolge) (h_frozen : isFrozen S b) :
    isFrozen S' b := by
  induction h_run with
  | refl _ => exact h_frozen
  | step S0 Smid Send h1 _ ih =>
      exact ih (isFrozen_persistent S0 Smid h1 b h_frozen)

end Kemgu.Drf.L4FrozenRegionRead
