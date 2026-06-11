/-
KEMGU DRF Mekanize — No-Fault Catı Teoremi (Onarim v3 F2)
Kaynak: belgeler/KEMGU_Mekanize_Onarim_Plan.md §6.3 + FAZ_BRIFINGLERI.md F2
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz

F2 guncellemesi: step_fault_preserves_typed yeni 21-kural Step formuna
yeniden yazildi:
- Tamam kurallari: h_S' esitliginden S'.fault = none dogrudan (rfl).
- Hata kurallari: Aile 2 dispatch (F2 lookup-form + AtamaOdak).
- Congruence kurallari (YENI): konfTipliFull_odak (Tipli §7) +
  tersine-cevirme lemmalari + IH — SORRY'SUZ.

typed_no_fault step case: F4 hedefi (adim_korunum zinciri) — sorry kalir.
-/

import Kemgu.Sem.Core
import Kemgu.Sem.SmallStep
import Kemgu.Sem.StateTipli
import Kemgu.Sem.HasType
import Kemgu.Sem.LineerTamam
import Kemgu.Sem.RegionTamam
import Kemgu.Sem.Tipli
import Kemgu.Discharge.Aile2

namespace Kemgu.Discharge.NoFault
open Kemgu.Sem.Core Kemgu.Sem.SmallStep Kemgu.Sem.StateTipli
     Kemgu.Sem.HasType Kemgu.Sem.LineerTamam Kemgu.Sem.RegionTamam
     Kemgu.Sem.Tipli Kemgu.Discharge.Aile2

-- ============================================================
-- §1. Tek-adim fault korunumu (typed varsayımı altında)
-- ============================================================

/-- Step tek-adim fault korunumu (F2 — TAM ISPATLI, sorry'suz):
    KonfTipliFull S + S.fault = none + Step S S' → S'.fault = none.

    Tamam: h_S' esitligi fault := none icerir.
    Hata: Aile 2 typing_excludes_* lemmalari ile exfalso.
    Congruence: konfTipliFull_odak ile ic konfigurasyonun tipliligi
    kurulur, IH uygulanir. -/
theorem step_fault_preserves_typed
    (Γ : TipOrtam) (Λ : LineerOrtam) (Ρ : BolgeOrtam)
    (S S' : Konfigurasyon) (h_step : Step S S')
    (h_typed_S : KonfTipliFull Γ Λ Ρ S)
    (h_no_fault : S.fault = none) :
    S'.fault = none := by
  revert h_typed_S h_no_fault
  induction h_step with
  -- ============ Tamam kurallari: dogrudan ============
  | sVarOku S S' ts1 ts2 ctx x b v h_t h_if h_b h_v h_S' =>
      intro _ _; subst h_S'; rfl
  | sAtamaTamam S S' ts1 ts2 ctx x v b h_t h_if h_b h_owner h_S' =>
      intro _ _; subst h_S'; rfl
  | sSeqAtla S S' ts1 ts2 ctx v b h_t h_if h_S' =>
      intro _ _; subst h_S'; rfl
  | sGuvensizAtla S S' ts1 ts2 ctx v h_t h_if h_S' =>
      intro _ _; subst h_S'; rfl
  | cGorevBaslatTamam S S' ts1 ts2 ctx tYeni yd kod h_t h_if h_fresh h_sahipler h_S' =>
      intro _ _; subst h_S'; rfl
  | cGorevBirlestirTamam S S' ts1 ts2 ctx g tHedef rb h_t h_if h_hedef h_donen h_S' =>
      intro _ _; subst h_S'; rfl
  | cKanalGonderTamam S S' ts1 ts2 ctx k vId b v h_t h_if h_b h_v h_owner h_S' =>
      intro _ _; subst h_S'; rfl
  | cKanalAlTamam S S' ts1 ts2 ctx k v tb h_t h_if h_v h_transit h_S' =>
      intro _ _; subst h_S'; rfl
  | cDondurTamam S S' ts1 ts2 ctx b h_t h_if h_owner h_S' =>
      intro _ _; subst h_S'; rfl
  | sLinKullanTamam S S' ts1 ts2 ctx x h_t h_if h_aktif h_S' =>
      intro _ _; subst h_S'; rfl
  | sLinImhaTamam S S' ts1 ts2 ctx x h_t h_if h_aktif h_S' =>
      intro _ _; subst h_S'; rfl
  -- ============ Hata kurallari: Aile 2 exfalso ============
  | sAtamaHataDonmus S S' ts1 ts2 ctx x v b h_t h_if h_b h_frozen h_S' =>
      intro h_typed_S _
      exfalso
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      obtain ⟨⟨τ', Λ'', Ρ'', h_typed⟩, _⟩ := h_typed_S.2.1 ctx h_ctx_in
      rw [h_if] at h_typed
      exact typing_excludes_sAtamaHataDonmus Γ Λ Ρ x _ τ' Λ'' Ρ'' h_typed S b
        h_typed_S.2.2.2.2.2.1 h_typed_S.2.2.2.2.2.2.1 h_b h_frozen
  | sAtamaHataSahipDegil S S' ts1 ts2 ctx x v b h_t h_if h_b h_not_owner h_S' =>
      intro h_typed_S _
      exfalso
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      exact typing_excludes_sAtamaHataSahipDegil S ctx x _ b h_ctx_in h_if
        h_typed_S.2.2.2.2.2.2.2 h_b h_not_owner
  | cGorevBaslatHataLineerIhlal S S' ts1 ts2 ctx yd kod vIhlal h_t h_if h_in h_tuket h_S' =>
      intro h_typed_S _
      exfalso
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      obtain ⟨⟨τ', Λ'', Ρ'', h_typed⟩, h_bridge⟩ := h_typed_S.2.1 ctx h_ctx_in
      rw [h_if] at h_typed
      exact typing_excludes_cGorevBaslatHataLineerIhlal Γ Λ Ρ yd kod vIhlal
        τ' Λ'' Ρ'' h_typed ctx h_bridge h_in h_tuket
  | cKanalGonderHataLineerTuket S S' ts1 ts2 ctx k vId h_t h_if h_tuket h_S' =>
      intro h_typed_S _
      exfalso
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      obtain ⟨⟨τ', Λ'', Ρ'', h_typed⟩, h_bridge⟩ := h_typed_S.2.1 ctx h_ctx_in
      rw [h_if] at h_typed
      exact typing_excludes_cKanalGonderHataLineerTuket Γ Λ Ρ k vId
        τ' Λ'' Ρ'' h_typed ctx h_bridge h_tuket
  | cDondurHataZatenDonmus S S' ts1 ts2 ctx b h_t h_if h_zaten h_S' =>
      intro h_typed_S _
      exfalso
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      obtain ⟨⟨τ', Λ'', Ρ'', h_typed⟩, _⟩ := h_typed_S.2.1 ctx h_ctx_in
      rw [h_if] at h_typed
      exact typing_excludes_cDondurHataZatenDonmus Γ Λ Ρ b τ' Λ'' Ρ'' h_typed S
        h_typed_S.2.2.2.2.2.1 h_typed_S.2.2.2.2.2.2.1 h_zaten
  | sLinKullanHataZatenTuketildi S S' ts1 ts2 ctx x h_t h_if h_tuket h_S' =>
      intro h_typed_S _
      exfalso
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      obtain ⟨⟨τ', Λ'', Ρ'', h_typed⟩, h_bridge⟩ := h_typed_S.2.1 ctx h_ctx_in
      rw [h_if] at h_typed
      exact typing_excludes_sLinKullanHataZatenTuketildi Γ Λ Ρ x
        τ' Λ'' Ρ'' h_typed ctx h_bridge h_tuket
  | sLinImhaHataZatenTuketildi S S' ts1 ts2 ctx x h_t h_if h_tuket h_S' =>
      intro h_typed_S _
      exfalso
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      obtain ⟨⟨τ', Λ'', Ρ'', h_typed⟩, h_bridge⟩ := h_typed_S.2.1 ctx h_ctx_in
      rw [h_if] at h_typed
      exact typing_excludes_sLinImhaHataZatenTuketildi Γ Λ Ρ x
        τ' Λ'' Ρ'' h_typed ctx h_bridge h_tuket
  -- ============ Congruence kurallari: odaklama + IH ============
  | sSeqCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' a a' b h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_S' ih =>
      intro h_typed_S h_no_fault
      subst h_S1 h_S'
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      obtain ⟨⟨τ', Λ'', Ρ'', h_typed⟩, _⟩ := h_typed_S.2.1 ctx h_ctx_in
      rw [h_if] at h_typed
      have h_typed_a := typed_seq_sol h_typed
      have h_konf1 : KonfTipliFull Γ Λ Ρ (ifadeyleKonf S ts1 ts2 ctx a) := by
        show KonfTipliFull Γ Λ Ρ
          { S with thread := ts1 ++ { ctx with ifade := a } :: ts2 }
        exact konfTipliFull_odak Γ Λ Ρ S ts1 ts2 ctx a h_typed_S h_t h_typed_a
          (fun y h => by rw [h_if]; exact AtamaOdak.seq_sol a b y h)
      have h_nf1 : (ifadeyleKonf S ts1 ts2 ctx a).fault = none := by
        simpa [ifadeyleKonf] using h_no_fault
      simpa using ih h_konf1 h_nf1
  | sAtamaCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' x e e' h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_S' ih =>
      intro h_typed_S h_no_fault
      subst h_S1 h_S'
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      obtain ⟨⟨τ', Λ'', Ρ'', h_typed⟩, _⟩ := h_typed_S.2.1 ctx h_ctx_in
      rw [h_if] at h_typed
      have h_typed_e := typed_atama_ic h_typed
      have h_konf1 : KonfTipliFull Γ Λ Ρ (ifadeyleKonf S ts1 ts2 ctx e) := by
        show KonfTipliFull Γ Λ Ρ
          { S with thread := ts1 ++ { ctx with ifade := e } :: ts2 }
        exact konfTipliFull_odak Γ Λ Ρ S ts1 ts2 ctx e h_typed_S h_t h_typed_e
          (fun y h => by rw [h_if]; exact AtamaOdak.atama_ic x e y h)
      have h_nf1 : (ifadeyleKonf S ts1 ts2 ctx e).fault = none := by
        simpa [ifadeyleKonf] using h_no_fault
      simpa using ih h_konf1 h_nf1
  | sGuvensizCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' e e' h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_S' ih =>
      intro h_typed_S h_no_fault
      subst h_S1 h_S'
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      obtain ⟨⟨τ', Λ'', Ρ'', h_typed⟩, _⟩ := h_typed_S.2.1 ctx h_ctx_in
      rw [h_if] at h_typed
      have h_typed_e := typed_guvensiz_ic h_typed
      have h_konf1 : KonfTipliFull Γ Λ Ρ (ifadeyleKonf S ts1 ts2 ctx e) := by
        show KonfTipliFull Γ Λ Ρ
          { S with thread := ts1 ++ { ctx with ifade := e } :: ts2 }
        exact konfTipliFull_odak Γ Λ Ρ S ts1 ts2 ctx e h_typed_S h_t h_typed_e
          (fun y h => by rw [h_if]; exact AtamaOdak.guvensiz_ic e y h)
      have h_nf1 : (ifadeyleKonf S ts1 ts2 ctx e).fault = none := by
        simpa [ifadeyleKonf] using h_no_fault
      simpa using ih h_konf1 h_nf1


-- ============================================================
-- §2. No-Fault catı teoremi (Plan v2 §6.3)
-- ============================================================

/-- TYPED NO-FAULT CATI (Plan v2 §6.3):
    KonfTipliFull S₀ → StepStar S₀ S → S.fault = none.

    refl: KonfTipliFull S₀'in 5. bileseni.
    step: step_fault_preserves_typed + KonfTipliFull KORUNUMU gerekir —
    korunum F4 `adim_korunum` hedefi (sorry). -/
theorem typed_no_fault
    (Γ : TipOrtam) (Λ : LineerOrtam) (Ρ : BolgeOrtam)
    (S₀ S : Konfigurasyon)
    (h_typed_init : KonfTipliFull Γ Λ Ρ S₀)
    (h_run : StepStar S₀ S) :
    S.fault = none := by
  induction h_run with
  | refl _ =>
    exact h_typed_init.2.2.2.2.1
  | step S0 S1 Send _hStep _hStar _IH =>
    -- TODO F4: adim_korunum (KonfTipliFull S0 → Step → ∃ Λ' Ρ',
    -- KonfTipliFull S1) + IH zinciri. Bkz. FAZ_BRIFINGLERI.md F4.
    sorry

end Kemgu.Discharge.NoFault
