/-
KEMGU DRF Mekanize — DRF-L1 Region-Thread Tekilligi (Onarim v3 F2)
Kaynak (kagit formel): belgeler/KEMGU_DRF_Lemmalar.md §DRF-L1
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz, sorry/axiom YOK

F2: zaman'siz guncel-durum sahiplik formu.
-/

import Kemgu.Sem.Core
import Kemgu.Sem.SmallStep
import Kemgu.Sem.Kopru
import Kemgu.Drf.L0BolgeKorunumu

namespace Kemgu.Drf.L1BolgeThreadTekilligi
open Kemgu.Sem.Core Kemgu.Sem.SmallStep Kemgu.Sem.Kopru Kemgu.Drf.L0BolgeKorunumu

/-- DRF-L1 — Region-Thread Tekilligi: bizim modelde DRF-L0' corollary'si
    (s1_invariant "lookup thread donerse tek" formu; donmus/kanalSahip/bos
    thread olmadigi icin implicit olarak dislanir). -/
theorem drf_l1_bolge_thread_tekilligi
    (Pi : Program) (h_iyi : IyiTipliCekirdek Pi)
    (S₀ S : Konfigurasyon) (h_run : StepStar S₀ S)
    (h_init : s1_invariant S₀) :
    s1_invariant S :=
  drf_l0_bolge_korunumu_starStep Pi h_iyi S₀ S h_run h_init

/-- DRF-L1 (kagit fidelity formu, explicit exclusion listesi). -/
theorem drf_l1_bolge_thread_tekilligi_kagit_form
    (Pi : Program) (h_iyi : IyiTipliCekirdek Pi)
    (S₀ S : Konfigurasyon) (h_run : StepStar S₀ S)
    (h_init : s1_invariant S₀)
    (b : Bolge)
    (_h_no_lit : b.kategori ≠ BolgeKategorisi.lit)
    (_h_no_global : b.kategori ≠ BolgeKategorisi.global)
    (_h_no_donmus : b.kategori ≠ BolgeKategorisi.donmus)
    (_h_no_kanal : ∀ k, b.kategori ≠ BolgeKategorisi.kanalRho k)
    (t1 t2 : ThreadId)
    (h1 : sahiplikGet S.sahiplik b = some (Sahip.thread t1))
    (h2 : sahiplikGet S.sahiplik b = some (Sahip.thread t2)) :
    t1 = t2 :=
  drf_l1_bolge_thread_tekilligi Pi h_iyi S₀ S h_run h_init b t1 t2 h1 h2

end Kemgu.Drf.L1BolgeThreadTekilligi
