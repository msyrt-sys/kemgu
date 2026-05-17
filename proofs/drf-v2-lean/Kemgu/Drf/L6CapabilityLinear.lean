/-
KEMGU DRF Mekanize — DRF-L6 Capability Linear Inheritance (Faz A3.9)
Kaynak (kagit formel): belgeler/KEMGU_DRF_Lemmalar.md §DRF-L6
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz, sorry/axiom YOK

Onkosul: DRF-L2 (commit 60f571a) — Linear move consumption
-/

import Kemgu.Sem.Core
import Kemgu.Sem.SmallStep
import Kemgu.Drf.L2LinearMoveCrossThread

namespace Kemgu.Drf.L6CapabilityLinear
open Kemgu.Sem.Core Kemgu.Sem.SmallStep Kemgu.Drf.L2LinearMoveCrossThread

-- ============================================================
-- §1. DRF-L6 — Capability Linear miras alir
-- ============================================================

/-- DRF-L6 — Capability Linear Inheritance (Lemmalar.md §DRF-L6):

    Kagit ifadesi:
      Gamma ⊢ y : yetki<R> (Capability V1)
      ⟹ y'nin butun bellek erisimleri DRF-L2 ve DRF-L5'in argumanlarini
         miras alir:
      (a) C-GOREV-BASLAT'ta y yakalandiysa, y t1'den silinir (DRF-L2)
      (b) C-KANAL-GONDER'de y transfer edildiyse, y kanal'da (DRF-L5)
      (c) y ayni anda iki thread'de erisilemez

    Bizim modelimizde Capability tipi (Tip.yetki) Linear tracking
    seviyesinde tekkez'den ayrik degildir; LineerOrtam tum Linear
    degerleri (tekkez, yetki) ayni Lineerlik enum'i ile takip eder
    (kagit CP.1.1 Linear Integration).

    Bu yuzden DRF-L6 = DRF-L2 (yetki context'inde) — thin wrapper.
    Eger y'nin tipi yetki<R> ise ve linearYakalananlar listesinde ise,
    DRF-L2'nin consumption garantisi y icin de gecerli. -/
theorem drf_l6_capability_linear_consumed
    (S S' : Konfigurasyon)
    (ctx : ThreadCtx) (tYeni : ThreadId)
    (yd : List VarId) (kod : Ifade)
    (transferredBolgeler : List Bolge)
    (linearYakalananlar : List VarId)
    -- cGorevBaslat hipotezleri:
    (h_in : ctx ∈ S.thread)
    (h_ifade : ctx.ifade = .gorevBaslat yd kod)
    (h_fresh : threadFresh S tYeni)
    (h_yeni_th : ∃ yctx ∈ S'.thread, yctx.tid = tYeni ∧ yctx.ifade = kod)
    (h_sahip : S'.sahiplik = sahiplikSetMany S.sahiplik
                  transferredBolgeler S.zaman (Sahip.thread tYeni))
    (h_lineer_caller :
      ∃ ctx' ∈ S'.thread, ctx'.tid = ctx.tid ∧
        ∀ v ∈ linearYakalananlar,
          (v, Lineerlik.tuketildi) ∈ ctx'.lineer)
    (h_iz : S'.iz = .threadBaslat tYeni :: S.iz)
    (h_zaman : S'.zaman = S.zaman + 1)
    (h_store : S'.store = S.store)
    (h_kanal : S'.kanal = S.kanal)
    -- y bir yetki<R> tipi degeri, linearYakalananlar'da:
    (y : VarId) (h_y : y ∈ linearYakalananlar)
    -- (Capability semantik bizim modelde Linear ile ayni — CP-NO-COPY = L-NO-COPY)
    : ∃ ctx' ∈ S'.thread, ctx'.tid = ctx.tid ∧
        (y, Lineerlik.tuketildi) ∈ ctx'.lineer :=
  drf_l2_linear_move_consumed S S' ctx tYeni yd kod transferredBolgeler
    linearYakalananlar h_in h_ifade h_fresh h_yeni_th h_sahip
    h_lineer_caller h_iz h_zaman h_store h_kanal y h_y


-- ============================================================
-- §2. Sinirlar: DRF-L6 (b) ve (c) tam form
-- ============================================================

/-
DRF-L6 (b) — C-KANAL-GONDER'de y transfer ile y kanal'a:
DRF-L5 (d) "no eszamanli erisim" ile ayni — cKanalGonder/Al'a
h_lineer_sender/receiver clause refactor gerek (A3.0'''' onerisi).
A3.8 L5 (d) ile birlikte deferred.

DRF-L6 (c) — y ayni anda iki thread'de erisilemez:
(a) ile (b) birlestirilince cikar. DRF-L2 + DRF-L5 corollary.
Tam ifadeyi yazmak icin Linear track persistence (yetki'ye uyarlanmis)
ve cross-Step argument gerek; A3.9 kapsami disinda.
-/

end Kemgu.Drf.L6CapabilityLinear
