/-
KEMGU DRF Mekanize — Aile 2 Discharge: Fault Impossibility (Onarim v3 F2)
Kaynak: belgeler/KEMGU_Mekanize_Onarim_Plan.md §6.2 Aile 2 + FAZ_BRIFINGLERI.md F2
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz, sorry/axiom YOK

Plan v2 §6.2 Aile 2 Discharge: typed program (Typed + KonfTipliFull) altinda
Step.Hata constructor'lara ulaslamayacagini garanti eden 7 lemma — TAMAMI FULL.

F2 guncellemeleri:
- Hata hipotezleri lookup-formda (lineerOrtamGet ctx.lineer x = some l);
  kopruye gecis lineerOrtamGet_mem ile (lookup → uyelik → kopru → Λ).
- Sahiplik zaman'siz (guncel-durum modeli).
- sAtamaHataSahipDegil AtamaOdak formunda (KonfTipliFull 8. bilesen).
- Konum dolaylamasi kalkti: h_b dogrudan bolge verir (k.bolge yerine b).
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
-- §1. Linear Aile 2 lemma'lari
-- ============================================================

/-- AILE 2 Linear — typing_excludes_sLinKullanHataZatenTuketildi.
    l_kullan: Λ x = aktif; kopru + h_tuket: Λ x = tuketildi → celiski. -/
theorem typing_excludes_sLinKullanHataZatenTuketildi
    (Γ : TipOrtam) (Δ : KanalOrtam) (Λ : LineerOrtam) (Ρ : BolgeOrtam)
    (x : VarId) (τ : Tip) (Λ' : LineerOrtam) (Ρ' : BolgeOrtam)
    (h_typed : Typed Γ Δ Λ Ρ (Ifade.kullanIf x) τ Λ' Ρ')
    (ctx : ThreadCtx)
    (h_bridge : ∀ y : VarId, ∀ lin : Lineerlik,
                  (y, lin) ∈ ctx.lineer ↔ lineerOrtamGet Λ y = some lin)
    (h_tuket : lineerOrtamGet ctx.lineer x = some Lineerlik.tuketildi) :
    False := by
  have h_lineerOK := h_typed.lineerOK
  match h_lineerOK with
  | LineerTamam.l_kullan _ _ _ _ _ h_aktif =>
    have h_mem := lineerOrtamGet_mem ctx.lineer x Lineerlik.tuketildi h_tuket
    have h_tuket_Λ := (h_bridge x Lineerlik.tuketildi).mp h_mem
    rw [h_tuket_Λ] at h_aktif
    nomatch h_aktif

/-- AILE 2 Linear — typing_excludes_sLinImhaHataZatenTuketildi (simetrik). -/
theorem typing_excludes_sLinImhaHataZatenTuketildi
    (Γ : TipOrtam) (Δ : KanalOrtam) (Λ : LineerOrtam) (Ρ : BolgeOrtam)
    (x : VarId) (τ : Tip) (Λ' : LineerOrtam) (Ρ' : BolgeOrtam)
    (h_typed : Typed Γ Δ Λ Ρ (Ifade.imhaIf x) τ Λ' Ρ')
    (ctx : ThreadCtx)
    (h_bridge : ∀ y : VarId, ∀ lin : Lineerlik,
                  (y, lin) ∈ ctx.lineer ↔ lineerOrtamGet Λ y = some lin)
    (h_tuket : lineerOrtamGet ctx.lineer x = some Lineerlik.tuketildi) :
    False := by
  have h_lineerOK := h_typed.lineerOK
  match h_lineerOK with
  | LineerTamam.l_imha _ _ _ _ _ h_aktif =>
    have h_mem := lineerOrtamGet_mem ctx.lineer x Lineerlik.tuketildi h_tuket
    have h_tuket_Λ := (h_bridge x Lineerlik.tuketildi).mp h_mem
    rw [h_tuket_Λ] at h_aktif
    nomatch h_aktif

/-- AILE 2 Linear — typing_excludes_cKanalGonderHataLineerTuket.
    l_kanal_gonder: Λ vId ≠ tuketildi; kopru + h_tuket → celiski. -/
theorem typing_excludes_cKanalGonderHataLineerTuket
    (Γ : TipOrtam) (Δ : KanalOrtam) (Λ : LineerOrtam) (Ρ : BolgeOrtam)
    (k : KanalId) (vId : VarId) (τ : Tip) (Λ' : LineerOrtam) (Ρ' : BolgeOrtam)
    (h_typed : Typed Γ Δ Λ Ρ (Ifade.kanalGonderIf k vId) τ Λ' Ρ')
    (ctx : ThreadCtx)
    (h_bridge : ∀ y : VarId, ∀ lin : Lineerlik,
                  (y, lin) ∈ ctx.lineer ↔ lineerOrtamGet Λ y = some lin)
    (h_tuket : lineerOrtamGet ctx.lineer vId = some Lineerlik.tuketildi) :
    False := by
  have h_lineerOK := h_typed.lineerOK
  match h_lineerOK with
  | LineerTamam.l_kanal_gonder _ _ _ _ h_notconsumed =>
    have h_mem := lineerOrtamGet_mem ctx.lineer vId Lineerlik.tuketildi h_tuket
    have h_tuket_Λ := (h_bridge vId Lineerlik.tuketildi).mp h_mem
    exact h_notconsumed h_tuket_Λ

/-- AILE 2 Linear — typing_excludes_cGorevBaslatHataLineerIhlal
    (use-after-move): l_gorev_baslat: ∀ v∈yd, Λ v ≠ tuketildi;
    kopru + h_tuket (vIhlal ∈ yd) → celiski. -/
theorem typing_excludes_cGorevBaslatHataLineerIhlal
    (Γ : TipOrtam) (Δ : KanalOrtam) (Λ : LineerOrtam) (Ρ : BolgeOrtam)
    (yd : List VarId) (kod : Ifade) (vIhlal : VarId)
    (τ : Tip) (Λ' : LineerOrtam) (Ρ' : BolgeOrtam)
    (h_typed : Typed Γ Δ Λ Ρ (Ifade.gorevBaslat yd kod) τ Λ' Ρ')
    (ctx' : ThreadCtx)
    (h_bridge' : ∀ y : VarId, ∀ lin : Lineerlik,
                   (y, lin) ∈ ctx'.lineer ↔ lineerOrtamGet Λ y = some lin)
    (h_vIhlal_in : vIhlal ∈ yd)
    (h_tuket : lineerOrtamGet ctx'.lineer vIhlal = some Lineerlik.tuketildi) :
    False := by
  have h_lineerOK := h_typed.lineerOK
  match h_lineerOK with
  | LineerTamam.l_gorev_baslat _ _ _ _ h_captures =>
    have h_mem := lineerOrtamGet_mem ctx'.lineer vIhlal Lineerlik.tuketildi h_tuket
    have h_tuket_Λ := (h_bridge' vIhlal Lineerlik.tuketildi).mp h_mem
    exact h_captures vIhlal h_vIhlal_in h_tuket_Λ


-- ============================================================
-- §2. Region Aile 2 lemma'lari
-- ============================================================

/-- AILE 2 Region — typing_excludes_sAtamaHataDonmus.
    r_atama: Ρ x = some b' ∧ b'.kategori ≠ donmus; S.bolge = Ρ + h_b →
    b = b'; FrozenKategori koprusu + h_frozen → kategori = donmus → celiski. -/
theorem typing_excludes_sAtamaHataDonmus
    (Γ : TipOrtam) (Δ : KanalOrtam) (Λ : LineerOrtam) (Ρ : BolgeOrtam)
    (x : VarId) (e : Ifade) (τ : Tip) (Λ' : LineerOrtam) (Ρ' : BolgeOrtam)
    (h_typed : Typed Γ Δ Λ Ρ (Ifade.atama x e) τ Λ' Ρ')
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
  | RegionTamam.r_atama _ _ _ _ _ bIc h_get h_notdonmus _ =>
    have h_get_S : bolgeOrtamGet S.bolge x = some bIc := by
      rw [h_bolge_eq]; exact h_get
    have h_bb : b = bIc := Option.some.inj (h_b.symm.trans h_get_S)
    have h_iff := h_frozen_kat x bIc h_get_S
    rw [h_bb] at h_frozen
    exact h_notdonmus (h_iff.mp h_frozen)

/-- AILE 2 Region — typing_excludes_cDondurHataZatenDonmus.
    r_dondur: Ρ kayitli + kategori ≠ donmus; kopru + h_zaten → celiski. -/
theorem typing_excludes_cDondurHataZatenDonmus
    (Γ : TipOrtam) (Δ : KanalOrtam) (Λ : LineerOrtam) (Ρ : BolgeOrtam)
    (b : Bolge) (τ : Tip) (Λ' : LineerOrtam) (Ρ' : BolgeOrtam)
    (h_typed : Typed Γ Δ Λ Ρ (Ifade.dondurIf b) τ Λ' Ρ')
    (S : Konfigurasyon)
    (h_bolge_eq : S.bolge = Ρ)
    (h_frozen_kat : ∀ (y : VarId) (b' : Bolge),
                      bolgeOrtamGet S.bolge y = some b' →
                      (isFrozen S b' ↔ b'.kategori = BolgeKategorisi.donmus))
    (h_zaten : isFrozen S b) :
    False := by
  have h_regionOK := h_typed.regionOK
  match h_regionOK with
  | RegionTamam.r_dondur _ _ _ _ xIc h_get h_notdonmus _ =>
    have h_get_S : bolgeOrtamGet S.bolge xIc = some b := by
      rw [h_bolge_eq]; exact h_get
    have h_iff := h_frozen_kat xIc b h_get_S
    exact h_notdonmus (h_iff.mp h_zaten)

/-- AILE 2 Ownership — typing_excludes_sAtamaHataSahipDegil (AtamaOdak formu).
    AtamaSahipligi invariant'i (KonfTipliFull 8. bilesen) + AtamaOdak.bas:
    atama yapan ctx hedef bolgenin GUNCEL sahibi → h_not_owner celiski.
    (Typed GEREKMEZ — invariant'in kendisi yeterli; invariant'in KURULUMU
    F3 koprusu + F4 korunumu ile gercek iceriklenir.) -/
theorem typing_excludes_sAtamaHataSahipDegil
    (S : Konfigurasyon) (ctx : ThreadCtx) (x : VarId) (e : Ifade) (b : Bolge)
    (h_in : ctx ∈ S.thread)
    (h_ifade : ctx.ifade = Ifade.atama x e)
    (h_atama_sahip : ∀ ctx' ∈ S.thread, ∀ y : VarId, AtamaOdak ctx'.ifade y →
                       ∀ b' : Bolge, bolgeOrtamGet S.bolge y = some b' →
                         sahiplikGet S.sahiplik b' = some (Sahip.thread ctx'.tid))
    (h_b : bolgeOrtamGet S.bolge x = some b)
    (h_not_owner : sahiplikGet S.sahiplik b ≠ some (Sahip.thread ctx.tid)) :
    False :=
  h_not_owner
    (h_atama_sahip ctx h_in x (h_ifade ▸ AtamaOdak.bas x e) b h_b)

end Kemgu.Discharge.Aile2
