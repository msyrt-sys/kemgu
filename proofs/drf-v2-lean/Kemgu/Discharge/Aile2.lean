/-
KEMGU DRF Mekanize — Aile 2 Discharge: Fault Impossibility (Onarim v3 F4)
Kaynak: belgeler/KEMGU_Mekanize_Onarim_Plan.md §6.2 Aile 2 + FAZ_BRIFINGLERI.md
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz, sorry/axiom YOK

F4 sadeleştirmesi (per-thread Λ): paylasimli-Λ iff koprusu SILINDI —
lemma'lar dogrudan thread'in kendi lineer ortami (Λin = ctx.lineer)
uzerinden calisir; lineerOrtamGet_mem gecisi gereksizlesti.
7 lemma TAMAMI FULL ispatli.
-/

import Kemgu.Sem.Core
import Kemgu.Sem.StateTipli
import Kemgu.Sem.HasType
import Kemgu.Sem.LineerTamam
import Kemgu.Sem.RegionTamam
import Kemgu.Sem.Tipli

namespace Kemgu.Discharge.Aile2
open Kemgu.Sem.Core Kemgu.Sem.StateTipli
     Kemgu.Sem.HasType Kemgu.Sem.LineerTamam Kemgu.Sem.RegionTamam
     Kemgu.Sem.Tipli

-- ============================================================
-- §1. Linear Aile 2 lemma'lari (per-thread Λin formu)
-- ============================================================

/-- AILE 2 Linear — l_kullan: Λin x = aktif; h_tuket: = tuketildi → celiski. -/
theorem typing_excludes_sLinKullanHataZatenTuketildi
    (Γ : TipOrtam) (Δ : KanalOrtam) (Λin : LineerOrtam) (Ρ : BolgeOrtam)
    (x : VarId) (τ : Tip) (Λ' : LineerOrtam) (Ρ' : BolgeOrtam)
    (h_typed : Typed Γ Δ Λin Ρ (Ifade.kullanIf x) τ Λ' Ρ')
    (h_tuket : lineerOrtamGet Λin x = some Lineerlik.tuketildi) :
    False := by
  have h_lineerOK := h_typed.lineerOK
  match h_lineerOK with
  | LineerTamam.l_kullan _ _ _ _ _ h_aktif =>
    rw [h_tuket] at h_aktif
    nomatch h_aktif

/-- AILE 2 Linear — l_imha (simetrik). -/
theorem typing_excludes_sLinImhaHataZatenTuketildi
    (Γ : TipOrtam) (Δ : KanalOrtam) (Λin : LineerOrtam) (Ρ : BolgeOrtam)
    (x : VarId) (τ : Tip) (Λ' : LineerOrtam) (Ρ' : BolgeOrtam)
    (h_typed : Typed Γ Δ Λin Ρ (Ifade.imhaIf x) τ Λ' Ρ')
    (h_tuket : lineerOrtamGet Λin x = some Lineerlik.tuketildi) :
    False := by
  have h_lineerOK := h_typed.lineerOK
  match h_lineerOK with
  | LineerTamam.l_imha _ _ _ _ _ h_aktif =>
    rw [h_tuket] at h_aktif
    nomatch h_aktif

/-- AILE 2 Linear — l_kanal_gonder: Λin vId ≠ tuketildi → celiski. -/
theorem typing_excludes_cKanalGonderHataLineerTuket
    (Γ : TipOrtam) (Δ : KanalOrtam) (Λin : LineerOrtam) (Ρ : BolgeOrtam)
    (k : KanalId) (vId : VarId) (τ : Tip) (Λ' : LineerOrtam) (Ρ' : BolgeOrtam)
    (h_typed : Typed Γ Δ Λin Ρ (Ifade.kanalGonderIf k vId) τ Λ' Ρ')
    (h_tuket : lineerOrtamGet Λin vId = some Lineerlik.tuketildi) :
    False := by
  have h_lineerOK := h_typed.lineerOK
  match h_lineerOK with
  | LineerTamam.l_kanal_gonder _ _ _ _ h_notconsumed =>
    exact h_notconsumed h_tuket

/-- AILE 2 Linear — l_gorev_baslat (use-after-move): ∀ v∈yd, Λin v ≠
    tuketildi; h_tuket (vIhlal ∈ yd) → celiski. -/
theorem typing_excludes_cGorevBaslatHataLineerIhlal
    (Γ : TipOrtam) (Δ : KanalOrtam) (Λin : LineerOrtam) (Ρ : BolgeOrtam)
    (yd : List VarId) (kod : Ifade) (vIhlal : VarId)
    (τ : Tip) (Λ' : LineerOrtam) (Ρ' : BolgeOrtam)
    (h_typed : Typed Γ Δ Λin Ρ (Ifade.gorevBaslat yd kod) τ Λ' Ρ')
    (h_vIhlal_in : vIhlal ∈ yd)
    (h_tuket : lineerOrtamGet Λin vIhlal = some Lineerlik.tuketildi) :
    False := by
  have h_lineerOK := h_typed.lineerOK
  match h_lineerOK with
  | LineerTamam.l_gorev_baslat _ _ _ _ _ h_captures _ =>
    exact h_captures vIhlal h_vIhlal_in h_tuket


-- ============================================================
-- §2. Region Aile 2 lemma'lari
-- ============================================================

/-- AILE 2 Region — typing_excludes_sAtamaHataDonmus. -/
theorem typing_excludes_sAtamaHataDonmus
    (Γ : TipOrtam) (Δ : KanalOrtam) (Λin : LineerOrtam) (Ρ : BolgeOrtam)
    (x : VarId) (e : Ifade) (τ : Tip) (Λ' : LineerOrtam) (Ρ' : BolgeOrtam)
    (h_typed : Typed Γ Δ Λin Ρ (Ifade.atama x e) τ Λ' Ρ')
    (S : Konfigurasyon) (b : Bolge)
    (h_bolge_eq : S.bolge = Ρ)
    (h_frozen_kat : ∀ (y : VarId) (b' : Bolge),
                      bolgeOrtamGet S.bolge y = some b' →
                      (isFrozen S b' ↔ b'.kategori = BolgeKategorisi.donmus))
    (h_b : bolgeOrtamGet S.bolge x = some b)
    (h_frozen : isFrozen S b) :
    False := by
  have h_regionOK := h_typed.regionOK
  match h_regionOK with
  | RegionTamam.r_atama _ _ _ _ _ bIc h_get h_yaz _ =>
    have h_get_S : bolgeOrtamGet S.bolge x = some bIc := by
      rw [h_bolge_eq]; exact h_get
    have h_bb : b = bIc := Option.some.inj (h_b.symm.trans h_get_S)
    have h_iff := h_frozen_kat x bIc h_get_S
    rw [h_bb] at h_frozen
    have h_kat := h_iff.mp h_frozen
    rw [h_kat] at h_yaz
    simp [kategoriYazilabilir] at h_yaz

/-- AILE 2 Region — typing_excludes_cDondurHataZatenDonmus. -/
theorem typing_excludes_cDondurHataZatenDonmus
    (Γ : TipOrtam) (Δ : KanalOrtam) (Λin : LineerOrtam) (Ρ : BolgeOrtam)
    (b : Bolge) (τ : Tip) (Λ' : LineerOrtam) (Ρ' : BolgeOrtam)
    (h_typed : Typed Γ Δ Λin Ρ (Ifade.dondurIf b) τ Λ' Ρ')
    (S : Konfigurasyon)
    (h_bolge_eq : S.bolge = Ρ)
    (h_frozen_kat : ∀ (y : VarId) (b' : Bolge),
                      bolgeOrtamGet S.bolge y = some b' →
                      (isFrozen S b' ↔ b'.kategori = BolgeKategorisi.donmus))
    (h_zaten : isFrozen S b) :
    False := by
  have h_regionOK := h_typed.regionOK
  match h_regionOK with
  | RegionTamam.r_dondur _ _ _ _ xIc h_get h_yaz _ =>
    have h_get_S : bolgeOrtamGet S.bolge xIc = some b := by
      rw [h_bolge_eq]; exact h_get
    have h_iff := h_frozen_kat xIc b h_get_S
    have h_kat := h_iff.mp h_zaten
    rw [h_kat] at h_yaz
    simp [kategoriYazilabilir] at h_yaz

/-- AILE 2 Ownership — typing_excludes_sAtamaHataSahipDegil
    (F4 onayli invariant — HedefVar formu): tipleme r_atama hedefin
    yazilabilir oldugunu verir; HedefVarSahipligi invarianti yazilabilir
    hedefin sahipligini verir → h_not_owner celiski. -/
theorem typing_excludes_sAtamaHataSahipDegil
    (Γ : TipOrtam) (Δ : KanalOrtam) (Λin : LineerOrtam) (Ρ : BolgeOrtam)
    (S : Konfigurasyon) (ctx : ThreadCtx) (x : VarId) (e : Ifade) (b : Bolge)
    (τ : Tip) (Λ' : LineerOrtam) (Ρ' : BolgeOrtam)
    (h_typed : Typed Γ Δ Λin Ρ (Ifade.atama x e) τ Λ' Ρ')
    (h_in : ctx ∈ S.thread)
    (h_ifade : ctx.ifade = Ifade.atama x e)
    (h_bolge_eq : S.bolge = Ρ)
    (h_hedef_sahip : ∀ ctx' ∈ S.thread, ∀ y : VarId, HedefVar ctx'.ifade y →
                       ∀ b' : Bolge, bolgeOrtamGet S.bolge y = some b' →
                         kategoriYazilabilir b'.kategori = true →
                         sahiplikGet S.sahiplik b' = some (Sahip.thread ctx'.tid))
    (h_b : bolgeOrtamGet S.bolge x = some b)
    (h_not_owner : sahiplikGet S.sahiplik b ≠ some (Sahip.thread ctx.tid)) :
    False := by
  have h_regionOK := h_typed.regionOK
  match h_regionOK with
  | RegionTamam.r_atama _ _ _ _ _ bIc h_get h_yaz _ =>
    have h_get_S : bolgeOrtamGet S.bolge x = some bIc := by
      rw [h_bolge_eq]; exact h_get
    have h_bb : b = bIc := Option.some.inj (h_b.symm.trans h_get_S)
    subst h_bb
    exact h_not_owner
      (h_hedef_sahip ctx h_in x (h_ifade ▸ HedefVar.atama_bas x e) b h_b h_yaz)

end Kemgu.Discharge.Aile2
