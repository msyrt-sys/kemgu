/-
KEMGU DRF Mekanize — DRF-L7 Bellek Erisim Tip Soundness (Onarim v3 F2)
Kaynak (kagit formel): belgeler/KEMGU_DRF_Lemmalar.md §DRF-L7
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz, sorry/axiom YOK

F2: step_iz_analiz gorunum lemmasindan turetilir.
-/

import Kemgu.Sem.Core
import Kemgu.Sem.SmallStep

namespace Kemgu.Drf.L7BellekErisimTipSoundness
open Kemgu.Sem.Core Kemgu.Sem.SmallStep

/-- DRF-L7 (a): yeni memYaz event'i eklendiyse (k, v) S'.store'a da
    eklenmistir — olay ile store senkron. -/
theorem drf_l7_a_step
    (S S' : Konfigurasyon) (h_step : Step S S')
    (t : ThreadId) (k : Konum) (v : Deger)
    (h_event : Olay.memYaz t k v ∈ S'.iz)
    (h_not_in_S : Olay.memYaz t k v ∉ S.iz) :
    (k, v) ∈ S'.store := by
  rcases step_iz_analiz S S' h_step with
      ⟨h_iz, _, _⟩
    | ⟨t', k', v', h_iz, _, _⟩
    | ⟨t', k', v', h_iz, h_store, _, _⟩
    | ⟨olay, h_iz, h_nyaz, _, _⟩
  · rw [h_iz] at h_event
    exact absurd h_event h_not_in_S
  · rw [h_iz] at h_event
    rcases List.mem_cons.mp h_event with h_head | h_tail
    · nomatch h_head
    · exact absurd h_tail h_not_in_S
  · rw [h_iz] at h_event
    rcases List.mem_cons.mp h_event with h_head | h_tail
    · injection h_head with h_t h_k h_v
      subst h_k h_v
      rw [h_store]
      exact List.Mem.head _
    · exact absurd h_tail h_not_in_S
  · rw [h_iz] at h_event
    rcases List.mem_cons.mp h_event with h_head | h_tail
    · exact absurd h_head.symm (h_nyaz t k v)
    · exact absurd h_tail h_not_in_S

end Kemgu.Drf.L7BellekErisimTipSoundness
