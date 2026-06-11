/-
KEMGU DRF Mekanize — DRF-L6 Capability Linear Inheritance (Onarim v3 F2)
Kaynak (kagit formel): belgeler/KEMGU_DRF_Lemmalar.md §DRF-L6
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz, sorry/axiom YOK

Capability (yetki) Linear tracking seviyesinde tekkez'den ayrik degil
(kagit CP.1.1) — DRF-L6 = DRF-L2 (yetki baglaminda), thin wrapper.
-/

import Kemgu.Sem.Core
import Kemgu.Sem.SmallStep
import Kemgu.Drf.L2LinearMoveCrossThread

namespace Kemgu.Drf.L6CapabilityLinear
open Kemgu.Sem.Core Kemgu.Sem.SmallStep Kemgu.Drf.L2LinearMoveCrossThread

/-- DRF-L6 — Capability Linear Inheritance: yetki<R> tipli y yakalama
    listesindeyse, DRF-L2 consumption garantisi y icin de gecerli
    (LineerOrtam tum lineer kategorileri ayni Lineerlik ile izler). -/
theorem drf_l6_capability_linear_consumed
    (Λ : LineerOrtam) (yd : List VarId) (y : VarId)
    (h_y : y ∈ yd)
    (h_aktif : lineerOrtamGet Λ y = some Lineerlik.aktif) :
    lineerOrtamGet (lineerTuketListe Λ yd) y = some Lineerlik.tuketildi :=
  drf_l2_linear_move_consumed Λ yd y h_y h_aktif

end Kemgu.Drf.L6CapabilityLinear
