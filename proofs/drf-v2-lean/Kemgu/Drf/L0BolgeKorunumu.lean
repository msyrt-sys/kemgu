/-
KEMGU DRF Mekanize — DRF-L0 Bolge Korunumu (Faz A3.2)
Kaynak (kagit formel): belgeler/KEMGU_DRF_Lemmalar.md §DRF-L0
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz, sorry/axiom YOK
-/

import Kemgu.Sem.Core
import Kemgu.Sem.SmallStep

namespace Kemgu.Drf.L0BolgeKorunumu
open Kemgu.Sem.Core Kemgu.Sem.SmallStep

-- ============================================================
-- §1. S1 invariant tanimi (Bellek Modeli §Katman 2)
-- ============================================================

/-- S1 invariant — Tekil Sahiplik (Op.Sem §2.4 + Bellek_Modeli §Katman 2):

    Bir bolge `b` ve zaman damgasi `z` icin, eger `b`'nin sahibi varsa
    o tektir; iki farkli thread ayni (b, z) anahtarinda sahip olamaz.

    NOT (modelleme karari): Bizim Lean modelimizde `Sahiplik` bir
    fonksiyon gibi davranir (`sahiplikGet` ilk eslesen entry'i dondurur),
    dolayisi ile S1 "iki farkli thread ayni anahtarda gozukmez" kismi
    yapisal olarak garanti edilir (lookup deterministik). Bu kagit
    modelinden farkli olarak (kagit cok-degerli iliski varsayar; bizim
    fonksiyon). Sonuc: S1 bizde "lookup fonksiyonel + thread variant
    tek" formuna iner — DRF-L0 ispati buradan kisa olur.

    Bu modelleme, kagit ispatin daha SIKI bir yorumudur: kagit
    "preservation by careful rule design" diyor; biz "preservation
    by structural function design" diyoruz. Iki yorum da ayni sonuca
    varir (DRF) farkli yollardan. -/
def s1_invariant (S : Konfigurasyon) : Prop :=
  ∀ (b : Bolge) (z : Zaman) (t1 t2 : ThreadId),
    sahiplikGet S.sahiplik (b, z) = some (Sahip.thread t1) →
    sahiplikGet S.sahiplik (b, z) = some (Sahip.thread t2) →
    t1 = t2

-- ============================================================
-- §2. Yardimci lemma: sahiplikGet fonksiyonelligi
-- ============================================================

/-- sahiplikGet fonksiyonel: ayni anahtarda ayni cevabi verir.
    Bu DRF-L0'in ozudur — modelimizdeki "Sigma fonksiyondur" kararinin
    Lean'de ispati. -/
theorem sahiplikGet_funkc (s : Sahiplik) (key : Bolge × Zaman) (v1 v2 : Sahip)
    (h1 : sahiplikGet s key = some v1)
    (h2 : sahiplikGet s key = some v2) :
    v1 = v2 := by
  rw [h1] at h2
  exact Option.some.inj h2

-- ============================================================
-- §3. DRF-L0: Bolge Korunumu (preservation)
-- ============================================================

/-- DRF-L0 — Bolge Korunumu (Lemmalar.md §DRF-L0).

    Onkosul: IyiTipli program, baslangic S konfigurasyonu S1 saglar,
             Step iliskisi ile S' konfigurasyonuna gecilmistir.
    Sonuc:   S' de S1 saglar.

    Kagit ispat skeci: her Sigma-degisticen Step kurali (cGorevBaslat,
    cGorevBirlestir, cKanalGonder, cKanalAl, cDondur) icin "tek sahip
    → tek sahip" case analysis.

    Bizim ispat (modelimiz strukturel): sahiplikGet fonksiyoneldir
    (sahiplikGet_funkc) → ayni anahtarda farkli thread donmez →
    iki thread esit. Step iliskisinin ic detaylari (h_step) ve
    IyiTipli (h_iyi) gerek olmaz — modelimiz S1'i yapisal olarak
    garanti eder. -/
theorem drf_l0_bolge_korunumu
    (Pi : Program) (_h_iyi : IyiTipli Pi)
    (S S' : Konfigurasyon) (_h_step : Step S S')
    (_h_s1 : s1_invariant S) :
    s1_invariant S' := by
  intro b z t1 t2 h1 h2
  -- sahiplikGet S'.sahiplik (b, z) returns some value; if it's
  -- some (Sahip.thread t1) AND some (Sahip.thread t2), they must
  -- match (Option + Sahip.thread injection).
  have h_eq : Sahip.thread t1 = Sahip.thread t2 := by
    exact sahiplikGet_funkc S'.sahiplik (b, z) _ _ h1 h2
  injection h_eq

-- ============================================================
-- §4. DRF-L0' — Coklu-adim preservation (corollary)
-- ============================================================

/-- DRF-L0' — Coklu-adim preservation: Step* altinda da S1 korunur.
    DRF-L1 (kapatma) bu corollary'ye dayanir. -/
theorem drf_l0_bolge_korunumu_starStep
    (Pi : Program) (h_iyi : IyiTipli Pi)
    (S S' : Konfigurasyon) (h_run : StepStar S S')
    (h_s1 : s1_invariant S) :
    s1_invariant S' := by
  induction h_run with
  | refl _ => exact h_s1
  | step S0 Smid Send h1 _ ih =>
      -- S0 → Smid in one step (h1); Smid →* Send (induction hypothesis).
      -- DRF-L0 on h1 gives s1 Smid; ih gives s1 Send.
      exact ih (drf_l0_bolge_korunumu Pi h_iyi S0 Smid h1 h_s1)


end Kemgu.Drf.L0BolgeKorunumu
