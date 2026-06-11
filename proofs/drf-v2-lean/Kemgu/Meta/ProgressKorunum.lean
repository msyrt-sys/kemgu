/-
KEMGU DRF Mekanize — Meta Katmani: Progress (Onarim v3 F4 sonrasi)
Wright-Felleisen: TAPL §8.3.2 (Progress)
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz

F4 temizligi (ADIM 0 §2.1(d) kapanisi):
- IFADE-YANLIS eski preservation iskeletleri SILINDI (preservation,
  preservation_typed, preservation_sigmaTipli/sahiplikTutarli/kanalTutarli,
  preservation_konfTipliFull) — yerlerini Discharge/NoFault.adim_korunum
  (birlesik korunum, dogru ifade) aldi.
- True-sonuclu soundness_corollary SILINDI (vakum — ADIM 0 elestirisi).
- Kalan: progress iskeletleri (F5 hedefi — ambient-Γ progress_konf formuna
  evrilecek + Engelli (blocked) disjunct'i).
-/

import Kemgu.Sem.Core
import Kemgu.Sem.SmallStep
import Kemgu.Sem.StateTipli
import Kemgu.Sem.HasType
import Kemgu.Sem.LineerTamam
import Kemgu.Sem.RegionTamam
import Kemgu.Sem.Tipli

namespace Kemgu.Meta.ProgressKorunum
open Kemgu.Sem.Core Kemgu.Sem.SmallStep Kemgu.Sem.StateTipli
     Kemgu.Sem.HasType Kemgu.Sem.LineerTamam Kemgu.Sem.RegionTamam
     Kemgu.Sem.Tipli

-- ============================================================
-- §1. IsValue — Bir ifadenin deger olmasi (TAPL §8.3.1 normal form)
-- ============================================================

/-- Bir ifade `e` bir DEGER ise IsValue e (yalniz literal). -/
inductive IsValue : Ifade → Prop where
  | iv_sabit (v : Deger) : IsValue (Ifade.sabit v)

/-- Engelli: bos kanaldan alim (concurrency-progress ucuncu disjunct'i —
    F5'te progress_konf bu formu kullanir; deger degil + adim da atamaz
    ama STUCK sayilmaz: gonderici gelince acilir). -/
def Engelli (S : Konfigurasyon) (e : Ifade) : Prop :=
  ∃ k : KanalId, e = Ifade.kanalAlIf k ∧ kanalIlk S.kanal k = none


-- ============================================================
-- §2. Progress (TAPL §8.3.2) — ISKELET (F5 hedefi)
-- ============================================================

/-- Progress (HasType katmani, kapali-Γ formu).

    NOT (ADIM 0): kapali-Γ formu yalniz degiskensiz programlari kapsar.
    F5'te ambient-Γ konfigurasyon-seviyesi forma gecilecek (progress_konf:
    KonfTipliFull + ctx ∈ S.thread → IsValue ∨ Engelli ∨ ∃ S', Step S S' —
    guard tanikleri KonfTipliFull bilesenlerinden: AtamaSahipligi → h_owner,
    DegiskenlerBagli → konum/deger, FrozenKategori → donmus-degil vb.). -/
theorem progress
    (e : Ifade) (τ : Tip)
    (h_typed : HasType tipOrtamBos kanalOrtamBos e τ)
    (S : Konfigurasyon) (ctx : ThreadCtx)
    (_h_ctx_in : ctx ∈ S.thread) (_h_ctx_ifade : ctx.ifade = e)
    (_h_no_fault : S.fault = none) :
    IsValue e ∨ Engelli S e ∨ ∃ S', Step S S' := by
  cases h_typed with
  | t_tanim x _ h_get =>
    exfalso; simp [tipOrtamGet] at h_get
  | t_sabit v _ _ =>
    left; exact IsValue.iv_sabit v
  | t_atama x _ _ h_get _ =>
    exfalso; simp [tipOrtamGet] at h_get
  -- TODO F5: seq induktif Progress (sSeqAtla / sSeqCong insasi)
  | t_seq _ _ _ _ _ _ =>
    sorry
  -- TODO F5: Step.cGorevBaslatTamam insasi (threadFresh taniki: 1 + max tid)
  | t_gorev_baslat _ _ _ _ =>
    sorry
  | t_gorev_birlestir _ _ h_get =>
    exfalso; simp [tipOrtamGet] at h_get
  | t_kanal_gonder _ _ h_get =>
    exfalso; simp [tipOrtamGet] at h_get
  -- TODO F5: kanalIlk dolu → cKanalAlTamam (transit invariant'i gerekli);
  --          kanalIlk bos → Engelli
  | t_kanal_al _ =>
    sorry
  -- TODO F5: sahiplik durumuna gore cDondurTamam / (donmus → tipleme dislar)
  | t_dondur _ =>
    sorry
  | t_kullan _ _ h_get =>
    exfalso; simp [tipOrtamGet] at h_get
  | t_imha _ _ h_get =>
    exfalso; simp [tipOrtamGet] at h_get
  -- TODO F5: guvensiz induktif Progress (sGuvensizAtla / sGuvensizCong)
  | t_guvensiz _ _ _ =>
    sorry

/-- Progress (Typed full ile) — F5'te progress_konf'a evrilecek. -/
theorem progress_typed
    (e : Ifade) (τ : Tip)
    (Λ' : LineerOrtam) (Ρ' : BolgeOrtam)
    (_h_typed : Typed tipOrtamBos kanalOrtamBos lineerOrtamBos bolgeOrtamBos
                  e τ Λ' Ρ')
    (S : Konfigurasyon) (ctx : ThreadCtx)
    (_h_ctx_in : ctx ∈ S.thread) (_h_ctx_ifade : ctx.ifade = e)
    (_h_no_fault : S.fault = none) :
    IsValue e ∨ Engelli S e ∨ ∃ S', Step S S' := by
  -- TODO F5: progress_konf (ambient-Γ + KonfTipliFull tanikleri)
  sorry

end Kemgu.Meta.ProgressKorunum
