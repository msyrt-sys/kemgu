/-
KEMGU DRF Mekanize — DRF-L1 Region-Thread Tekilligi (Faz A3.4)
Kaynak (kagit formel): belgeler/KEMGU_DRF_Lemmalar.md §DRF-L1
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz, sorry/axiom YOK

Onkosul: DRF-L0 + drf_l0_bolge_korunumu_starStep (commit 43d5bb2)
-/

import Kemgu.Sem.Core
import Kemgu.Sem.SmallStep
import Kemgu.Drf.L0BolgeKorunumu

namespace Kemgu.Drf.L1BolgeThreadTekilligi
open Kemgu.Sem.Core Kemgu.Sem.SmallStep Kemgu.Drf.L0BolgeKorunumu

-- ============================================================
-- §1. DRF-L1 (ana form) — StepStar uzerinden tum izlere S1
-- ============================================================

/-- DRF-L1 — Region-Thread Tekilligi (Lemmalar.md §DRF-L1):

    Kagit ifadesi:
      IyiTipli(Π) ∧ Π ⟹* S ⟹
        ∀ ρ ∉ {ρ_donmus, ρ_lit, ρ_global, ρ_kanal(_)}, ∀ z :
          |{t : Σ(ρ, z) = t ∧ t ∈ Threads}| ≤ 1

    Bizim modelimizde DRF-L1 = DRF-L0' (drf_l0_bolge_korunumu_starStep).
    s1_invariant tanimimiz "lookup thread donerse tek" formundadir;
    Sahip.donmus, Sahip.kanalSahip, Sahip.bos durumlari implicit olarak
    invariant disinda kalir (thread degildirler → hipotez vacuous).

    Bu yuzden kagitin exclusion listesi (ρ_donmus, ρ_lit, ρ_global,
    ρ_kanal) bizim tanim seviyesinde otomatik handle edilir.

    Ispat: dogrudan DRF-L0'  uzerinden corollary (L0 zaten StepStar
    induksiyonu yapti). Boylece L1 trivial. -/
theorem drf_l1_bolge_thread_tekilligi
    (Pi : Program) (h_iyi : IyiTipli Pi)
    (S₀ S : Konfigurasyon) (h_run : StepStar S₀ S)
    (h_init : s1_invariant S₀) :
    s1_invariant S :=
  drf_l0_bolge_korunumu_starStep Pi h_iyi S₀ S h_run h_init


-- ============================================================
-- §2. DRF-L1 (kagit fidelity formu, explicit exclusion)
-- ============================================================

/-- DRF-L1 (kagit form): exclusion'lar explicit listelenmis sekilde.
    Okuma rahatligi icin — semantik olarak ana formla esit.

    Hipotez: bolge `b` su 4 kategoride DEGIL:
      - lit (literal kalici bolge — coklu okuyucu)
      - global (programa ait kalici bolge — coklu thread okur)
      - donmus (zaten Sahip.donmus state'inde — coklu reader)
      - kanalRho k (kanal transit'inde — gecici sahipsiz)
    Sonuc: belirli (b, z)'de thread sahip varsa tek. -/
theorem drf_l1_bolge_thread_tekilligi_kagit_form
    (Pi : Program) (h_iyi : IyiTipli Pi)
    (S₀ S : Konfigurasyon) (h_run : StepStar S₀ S)
    (h_init : s1_invariant S₀)
    (b : Bolge)
    (_h_no_lit : b.kategori ≠ BolgeKategorisi.lit)
    (_h_no_global : b.kategori ≠ BolgeKategorisi.global)
    (_h_no_donmus : b.kategori ≠ BolgeKategorisi.donmus)
    (_h_no_kanal : ∀ k, b.kategori ≠ BolgeKategorisi.kanalRho k)
    (z : Zaman) (t1 t2 : ThreadId)
    (h1 : sahiplikGet S.sahiplik (b, z) = some (Sahip.thread t1))
    (h2 : sahiplikGet S.sahiplik (b, z) = some (Sahip.thread t2)) :
    t1 = t2 :=
  drf_l1_bolge_thread_tekilligi Pi h_iyi S₀ S h_run h_init b z t1 t2 h1 h2


end Kemgu.Drf.L1BolgeThreadTekilligi
