/-
KEMGU DRF Mekanize — DRF-L5 Channel Atomicity Preservation (Faz A3.8)
Kaynak (kagit formel): belgeler/KEMGU_DRF_Lemmalar.md §DRF-L5
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz, sorry/axiom YOK

Onkosul: A3.0' refactor (cKanalGonder/cKanalAl h_sahip, commit c0bd0fd)
-/

import Kemgu.Sem.Core
import Kemgu.Sem.SmallStep

namespace Kemgu.Drf.L5KanalAtomikTransfer
open Kemgu.Sem.Core Kemgu.Sem.SmallStep

-- ============================================================
-- §1. DRF-L5 (b) — cKanalGonder atomik transfer
-- ============================================================

/-- DRF-L5 (b) — cKanalGonder atomic: gönderim sonrasi
    bolge(v) sahibi atomik olarak ρ_kanal(k)'ye gecer.

    Kagit ifadesi: "z = z_gonder noktasinda: bolge(v) atomik olarak
                    t_a → ρ_kanal(k)"

    Bizim modelimizde sahiplikSet semantigi (Op.Sem §S3 atomic transfer).
    A3.0' refactor cKanalGonder'a h_sahip clause ekledi:
      S'.sahiplik = sahiplikSet S.sahiplik transferredBolge S.zaman
                     (Sahip.kanalSahip k)
    sahiplikSet_eq dogrudan lookup'u verir. -/
theorem drf_l5_b_kanalGonder_atomic
    (S S' : Konfigurasyon)
    (ctx : ThreadCtx) (k : KanalId) (vId : VarId) (v : Deger)
    (transferredBolge : Bolge)
    (_h_in : ctx ∈ S.thread)
    (_h_ifade : ctx.ifade = .kanalGonderIf k vId)
    (_h_kanal : ∃ kd ∈ S'.kanal, kd.kid = k ∧ v ∈ kd.gonderKuyrugu)
    (h_sahip : S'.sahiplik = sahiplikSet S.sahiplik
                  transferredBolge S.zaman (Sahip.kanalSahip k))
    (_h_iz : S'.iz = .kanalGonderOl ctx.tid k v :: S.iz)
    (_h_zaman : S'.zaman = S.zaman + 1)
    (_h_store : S'.store = S.store) :
    sahiplikGet S'.sahiplik (transferredBolge, S.zaman) = some (Sahip.kanalSahip k) := by
  rw [h_sahip]
  exact sahiplikSet_eq _ _ _ _


-- ============================================================
-- §2. DRF-L5 (c) — cKanalAl atomik transfer
-- ============================================================

/-- DRF-L5 (c) — cKanalAl atomic: alim sonrasi
    bolge(v) sahibi atomik olarak ρ_kanal(k) → ρ_sahip(t_alan).

    Kagit ifadesi: "z = z_al noktasinda: bolge(v) atomik olarak
                    ρ_kanal(k) → ρ_sahip(t_b)"

    A3.0' refactor cKanalAl'a h_sahip clause ekledi:
      S'.sahiplik = sahiplikSet S.sahiplik transferredBolge S.zaman
                     (Sahip.thread ctx.tid)
    Yine sahiplikSet_eq ile lookup. -/
theorem drf_l5_c_kanalAl_atomic
    (S S' : Konfigurasyon)
    (ctx : ThreadCtx) (k : KanalId) (v : Deger)
    (transferredBolge : Bolge)
    (_h_in : ctx ∈ S.thread)
    (_h_ifade : ctx.ifade = .kanalAlIf k)
    (_h_kanal_var : ∃ kd ∈ S.kanal, kd.kid = k ∧ v ∈ kd.gonderKuyrugu)
    (h_sahip : S'.sahiplik = sahiplikSet S.sahiplik
                  transferredBolge S.zaman (Sahip.thread ctx.tid))
    (_h_iz : S'.iz = .kanalAlOl ctx.tid k v :: S.iz)
    (_h_zaman : S'.zaman = S.zaman + 1)
    (_h_store : S'.store = S.store) :
    sahiplikGet S'.sahiplik (transferredBolge, S.zaman) = some (Sahip.thread ctx.tid) := by
  rw [h_sahip]
  exact sahiplikSet_eq _ _ _ _


-- ============================================================
-- §3. Sinirlar: DRF-L5 (a) ve (d) — refactor gerek
-- ============================================================

/-
DRF-L5 (a) — ∀ z ∈ [z_gonder, z_al) : Σ(bolge(v), z) = ρ_kanal(k):

Kagit ifadesi: gonderim ile alim arasinda kanaldaki bolge'nin sahibi
kanal'in icinde kalir; baska bir Step bu sahipligi degistirmez.

Bizim modelde: gonderim sonrasi sahiplikGet (transferredBolge, z_gonder)
= Sahip.kanalSahip k. Sonraki Step'ler eger AYNI key'i override etmezse
(yani transferredBolge baska bir Step icin de transfer hedefi degilse
ayni zaman damgasinda), lookup donmus kalir.

Persistence ispati DRF-L4 isFrozen_persistent_simple emsali: tek-adim
preservation case analysis (8 constructor). ~100 satir; bu A3.8'in
kapsami disinda (Teorem 4' ispatinda gerekirse derinlestirilir).

DRF-L5 (d) — t_a ve t_b'nin v'ye eszamanli erisimi yok:

Bizim modelde cKanalGonder'in CAGIRAN'in (t_a) ctx.lineer'inde Linear
v'nin tuketildigine dair clause YOK (cGorevBaslat A3.0''' refactor
emsali — h_lineer_sender). DRF-L2 emsali Linear consumption gerekirse
A3.0'''' (cKanalGonder/cKanalAl h_lineer_sender/receiver) refactor
gerek.

Bu A3.8 (b)+(c) provable form ile durur; (a) ve (d) Teorem 4'
ispatinda gerekirse refactor ile detaylandirilir.
-/

end Kemgu.Drf.L5KanalAtomikTransfer
