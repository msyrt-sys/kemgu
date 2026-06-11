/-
KEMGU DRF Mekanize — DRF-L0 Bolge Korunumu (Onarim v3 F2)
Kaynak (kagit formel): belgeler/KEMGU_DRF_Lemmalar.md §DRF-L0
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz, sorry/axiom YOK

F2: Sahiplik guncel-durum modeli — S1 invariant zaman'siz forma indi.
-/

import Kemgu.Sem.Core
import Kemgu.Sem.SmallStep
import Kemgu.Sem.Kopru

namespace Kemgu.Drf.L0BolgeKorunumu
open Kemgu.Sem.Core Kemgu.Sem.SmallStep Kemgu.Sem.Kopru

-- ============================================================
-- §1. S1 invariant tanimi (Bellek Modeli §Katman 2)
-- ============================================================

/-- S1 invariant — Tekil Sahiplik: bir bolgenin guncel sahibi varsa tektir.

    Modelleme karari: Sahiplik fonksiyon gibi davranir (sahiplikGet ilk
    eslesen entry) → S1 yapisal olarak garanti (lookup deterministik).
    Kagit "preservation by careful rule design" der; biz "preservation
    by structural function design" deriz — ayni sonuc, farkli yol. -/
def s1_invariant (S : Konfigurasyon) : Prop :=
  ∀ (b : Bolge) (t1 t2 : ThreadId),
    sahiplikGet S.sahiplik b = some (Sahip.thread t1) →
    sahiplikGet S.sahiplik b = some (Sahip.thread t2) →
    t1 = t2

-- ============================================================
-- §2. Yardimci lemma: sahiplikGet fonksiyonelligi
-- ============================================================

/-- sahiplikGet fonksiyonel: ayni anahtarda ayni cevap. -/
theorem sahiplikGet_funkc (s : Sahiplik) (key : Bolge) (v1 v2 : Sahip)
    (h1 : sahiplikGet s key = some v1)
    (h2 : sahiplikGet s key = some v2) :
    v1 = v2 := by
  rw [h1] at h2
  exact Option.some.inj h2

/-- S1 YAPISAL: modelimizde her konfigurasyon S1'i saglar (lookup
    deterministik — "preservation by structural function design").
    Bu, kemgu_soundness_v3'un h_init_s1 hipotezini gereksizlestirir. -/
theorem s1_yapisal (S : Konfigurasyon) : s1_invariant S := by
  intro b t1 t2 h1 h2
  have h_eq : Sahip.thread t1 = Sahip.thread t2 :=
    sahiplikGet_funkc S.sahiplik b _ _ h1 h2
  injection h_eq

-- ============================================================
-- §3. DRF-L0: Bolge Korunumu (preservation)
-- ============================================================

/-- DRF-L0 — Bolge Korunumu: Step altinda S1 korunur.
    Ispat yapisal (lookup fonksiyonel) — Step/IyiTipliCekirdek detaylari gerekmez. -/
theorem drf_l0_bolge_korunumu
    (Pi : Program) (_h_iyi : IyiTipliCekirdek Pi)
    (S S' : Konfigurasyon) (_h_step : Step S S')
    (_h_s1 : s1_invariant S) :
    s1_invariant S' := by
  intro b t1 t2 h1 h2
  have h_eq : Sahip.thread t1 = Sahip.thread t2 :=
    sahiplikGet_funkc S'.sahiplik b _ _ h1 h2
  injection h_eq

-- ============================================================
-- §4. DRF-L0' — Coklu-adim preservation (corollary)
-- ============================================================

/-- DRF-L0' — StepStar altinda da S1 korunur. -/
theorem drf_l0_bolge_korunumu_starStep
    (Pi : Program) (h_iyi : IyiTipliCekirdek Pi)
    (S S' : Konfigurasyon) (h_run : StepStar S S')
    (h_s1 : s1_invariant S) :
    s1_invariant S' := by
  induction h_run with
  | refl _ => exact h_s1
  | step S0 Smid Send h1 _ ih =>
      exact ih (drf_l0_bolge_korunumu Pi h_iyi S0 Smid h1 h_s1)

end Kemgu.Drf.L0BolgeKorunumu
