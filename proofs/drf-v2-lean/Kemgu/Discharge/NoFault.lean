/-
KEMGU DRF Mekanize — No-Fault Catı + Birlesik Korunum (Onarim v3 F4)
Kaynak: belgeler/KEMGU_Mekanize_Onarim_Plan.md §6.3 + FAZ_BRIFINGLERI.md F4
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz

F4 yapisi (dairesellik kirildi — ADIM 0 Sorun 2(c) kapanisi):
- step_fault_preserves_typed: per-thread Λ formunda TAM ISPATLI (21 kural;
  Tamam dogrudan, Hata Aile 2, congruence konfTipliFull_odak + IH).
- adim_korunum: BIRLESIK korunum (tek yonlu guclu induksiyon) — eski
  {typed_no_fault ↔ preservation_konfTipli} hipotez dongusu yerine tek
  ileri lemma. ISKELET (sorry): Tamam case'lerinin bilesen-bilesen
  korunum ispatlari F4-ispat fazinin kalan isi (asagida yol haritasi).
- typed_no_fault: adim_korunum'un StepStar kosesi — TAM BAGLANDI
  (tek sorry kaynagi adim_korunum; dongu YOK, ileri induksiyon).
- iyiTipli_no_fault: F3 koprusuyle kagit-formdaki Program-seviyesi catı.
-/

import Kemgu.Sem.Core
import Kemgu.Sem.SmallStep
import Kemgu.Sem.StateTipli
import Kemgu.Sem.HasType
import Kemgu.Sem.LineerTamam
import Kemgu.Sem.RegionTamam
import Kemgu.Sem.Tipli
import Kemgu.Sem.Kopru
import Kemgu.Discharge.Aile2

namespace Kemgu.Discharge.NoFault
open Kemgu.Sem.Core Kemgu.Sem.SmallStep Kemgu.Sem.StateTipli
     Kemgu.Sem.HasType Kemgu.Sem.LineerTamam Kemgu.Sem.RegionTamam
     Kemgu.Sem.Tipli Kemgu.Sem.Kopru Kemgu.Discharge.Aile2

-- ============================================================
-- §1. Tek-adim fault korunumu (typed varsayımı altında) — TAM
-- ============================================================

/-- Step tek-adim fault korunumu (F4 per-thread form — TAM ISPATLI):
    KonfTipliFull S + S.fault = none + Step S S' → S'.fault = none. -/
theorem step_fault_preserves_typed
    (Γ : TipOrtam) (Δ : KanalOrtam) (Ρ : BolgeOrtam)
    (S S' : Konfigurasyon) (h_step : Step S S')
    (h_typed_S : KonfTipliFull Γ Δ Ρ S)
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
  | cKanalGonderTamam S S' ts1 ts2 ctx k vId b v h_t h_if h_b h_v h_owner h_bos h_S' =>
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
      obtain ⟨τ', Λ'', Ρ'', h_typed⟩ := h_typed_S.2.1 ctx h_ctx_in
      rw [h_if] at h_typed
      exact typing_excludes_sAtamaHataDonmus Γ Δ ctx.lineer Ρ x _ τ' Λ'' Ρ''
        h_typed S b h_typed_S.2.2.2.2.2.1 h_typed_S.2.2.2.2.2.2.1 h_b h_frozen
  | sAtamaHataSahipDegil S S' ts1 ts2 ctx x v b h_t h_if h_b h_not_owner h_S' =>
      intro h_typed_S _
      exfalso
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      obtain ⟨τ', Λ'', Ρ'', h_typed⟩ := h_typed_S.2.1 ctx h_ctx_in
      rw [h_if] at h_typed
      exact typing_excludes_sAtamaHataSahipDegil Γ Δ ctx.lineer Ρ S ctx x _ b
        τ' Λ'' Ρ'' h_typed h_ctx_in h_if h_typed_S.2.2.2.2.2.1
        h_typed_S.2.2.2.2.2.2.2.1 h_b h_not_owner
  | cGorevBaslatHataLineerIhlal S S' ts1 ts2 ctx yd kod vIhlal h_t h_if h_in h_tuket h_S' =>
      intro h_typed_S _
      exfalso
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      obtain ⟨τ', Λ'', Ρ'', h_typed⟩ := h_typed_S.2.1 ctx h_ctx_in
      rw [h_if] at h_typed
      exact typing_excludes_cGorevBaslatHataLineerIhlal Γ Δ ctx.lineer Ρ
        yd kod vIhlal τ' Λ'' Ρ'' h_typed h_in h_tuket
  | cKanalGonderHataLineerTuket S S' ts1 ts2 ctx k vId h_t h_if h_tuket h_S' =>
      intro h_typed_S _
      exfalso
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      obtain ⟨τ', Λ'', Ρ'', h_typed⟩ := h_typed_S.2.1 ctx h_ctx_in
      rw [h_if] at h_typed
      exact typing_excludes_cKanalGonderHataLineerTuket Γ Δ ctx.lineer Ρ
        k vId τ' Λ'' Ρ'' h_typed h_tuket
  | cDondurHataZatenDonmus S S' ts1 ts2 ctx b h_t h_if h_zaten h_S' =>
      intro h_typed_S _
      exfalso
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      obtain ⟨τ', Λ'', Ρ'', h_typed⟩ := h_typed_S.2.1 ctx h_ctx_in
      rw [h_if] at h_typed
      exact typing_excludes_cDondurHataZatenDonmus Γ Δ ctx.lineer Ρ
        b τ' Λ'' Ρ'' h_typed S
        h_typed_S.2.2.2.2.2.1 h_typed_S.2.2.2.2.2.2.1 h_zaten
  | sLinKullanHataZatenTuketildi S S' ts1 ts2 ctx x h_t h_if h_tuket h_S' =>
      intro h_typed_S _
      exfalso
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      obtain ⟨τ', Λ'', Ρ'', h_typed⟩ := h_typed_S.2.1 ctx h_ctx_in
      rw [h_if] at h_typed
      exact typing_excludes_sLinKullanHataZatenTuketildi Γ Δ ctx.lineer Ρ
        x τ' Λ'' Ρ'' h_typed h_tuket
  | sLinImhaHataZatenTuketildi S S' ts1 ts2 ctx x h_t h_if h_tuket h_S' =>
      intro h_typed_S _
      exfalso
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      obtain ⟨τ', Λ'', Ρ'', h_typed⟩ := h_typed_S.2.1 ctx h_ctx_in
      rw [h_if] at h_typed
      exact typing_excludes_sLinImhaHataZatenTuketildi Γ Δ ctx.lineer Ρ
        x τ' Λ'' Ρ'' h_typed h_tuket
  -- ============ Congruence kurallari: odaklama + IH ============
  | sSeqCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' a a' b h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      intro h_typed_S h_no_fault
      subst h_S1 h_S'
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      obtain ⟨τ', Λ'', Ρ'', h_typed⟩ := h_typed_S.2.1 ctx h_ctx_in
      rw [h_if] at h_typed
      have h_typed_a := typed_seq_sol h_typed
      have h_konf1 : KonfTipliFull Γ Δ Ρ (ifadeyleKonf S ts1 ts2 ctx a) := by
        show KonfTipliFull Γ Δ Ρ
          { S with thread := ts1 ++ { ctx with ifade := a } :: ts2 }
        exact konfTipliFull_odak Γ Δ Ρ S ts1 ts2 ctx a h_typed_S h_t h_typed_a
          (fun y h => by rw [h_if]; exact HedefVar.seq_sol a b y h)
          (fun bb h => by rw [h_if]; exact HedefBolge.seq_sol a b bb h)
      have h_nf1 : (ifadeyleKonf S ts1 ts2 ctx a).fault = none := by
        simpa [ifadeyleKonf] using h_no_fault
      simpa using ih h_konf1 h_nf1
  | sAtamaCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' x e e' h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      intro h_typed_S h_no_fault
      subst h_S1 h_S'
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      obtain ⟨τ', Λ'', Ρ'', h_typed⟩ := h_typed_S.2.1 ctx h_ctx_in
      rw [h_if] at h_typed
      have h_typed_e := typed_atama_ic h_typed
      have h_konf1 : KonfTipliFull Γ Δ Ρ (ifadeyleKonf S ts1 ts2 ctx e) := by
        show KonfTipliFull Γ Δ Ρ
          { S with thread := ts1 ++ { ctx with ifade := e } :: ts2 }
        exact konfTipliFull_odak Γ Δ Ρ S ts1 ts2 ctx e h_typed_S h_t h_typed_e
          (fun y h => by rw [h_if]; exact HedefVar.atama_ic x e y h)
          (fun bb h => by rw [h_if]; exact HedefBolge.atama_ic x e bb h)
      have h_nf1 : (ifadeyleKonf S ts1 ts2 ctx e).fault = none := by
        simpa [ifadeyleKonf] using h_no_fault
      simpa using ih h_konf1 h_nf1
  | sGuvensizCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' e e' h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      intro h_typed_S h_no_fault
      subst h_S1 h_S'
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      obtain ⟨τ', Λ'', Ρ'', h_typed⟩ := h_typed_S.2.1 ctx h_ctx_in
      rw [h_if] at h_typed
      have h_typed_e := typed_guvensiz_ic h_typed
      have h_konf1 : KonfTipliFull Γ Δ Ρ (ifadeyleKonf S ts1 ts2 ctx e) := by
        show KonfTipliFull Γ Δ Ρ
          { S with thread := ts1 ++ { ctx with ifade := e } :: ts2 }
        exact konfTipliFull_odak Γ Δ Ρ S ts1 ts2 ctx e h_typed_S h_t h_typed_e
          (fun y h => by rw [h_if]; exact HedefVar.guvensiz_ic e y h)
          (fun bb h => by rw [h_if]; exact HedefBolge.guvensiz_ic e bb h)
      have h_nf1 : (ifadeyleKonf S ts1 ts2 ctx e).fault = none := by
        simpa [ifadeyleKonf] using h_no_fault
      simpa using ih h_konf1 h_nf1


-- ============================================================
-- §1.5. ODAK-ADIM YUKU (Onarim v3 kapanis — cong-kapanisinin motoru)
-- Her adimin odakli cifti OdakYuk tasir: FIX-F yan-kosullari +
-- cerrah_kilit verilen ayrisimi RULE-odagina pinler; boylece yuk
-- yalniz odakli cift icin kanitlanir (odaksiz-pozisyon ayrisimlari
-- hipotez-celiskisiyle bos dusur).
-- ============================================================

theorem adim_odak_yuku
    (Γ : TipOrtam) (Δ : KanalOrtam) (Ρ : BolgeOrtam)
    (S S' : Konfigurasyon) (h_step : Step S S')
    (h_konf : KonfTipliFull Γ Δ Ρ S) :
    ∀ ts1o ts2o ctxo ts2o' ctxo',
      S.thread = ts1o ++ ctxo :: ts2o →
      S'.thread = ts1o ++ ctxo' :: ts2o' →
      (ts2o' = ts2o ∨ ∃ y, ts2o' = ts2o ++ [y]) →
      OdakYuk Γ Δ S S' ctxo ctxo' := by
  revert h_konf
  induction h_step with
  | sVarOku S S' ts1 ts2 ctx x b v h_t h_if h_b h_v h_S' =>
      intro h_konf ts1o ts2o ctxo ts2o' ctxo' h1 h2 h_yano
      subst h_S'
      have h_ne : ({ ctx with ifade := Ifade.sabit v } : ThreadCtx) ≠ ctx := by
        intro h_e
        have h_i : Ifade.sabit v = ctx.ifade := congrArg ThreadCtx.ifade h_e
        rw [h_if] at h_i
        nomatch h_i
      have h2' : ts1 ++ ({ ctx with ifade := Ifade.sabit v } : ThreadCtx) :: ts2
          = ts1o ++ ctxo' :: ts2o' := h2
      obtain ⟨h_e1, h_e2, h_e3, h_e4, h_e5⟩ :=
        cerrah_kilit ts1o ts1 (h_t.symm.trans h1) h2' (Or.inl rfl) h_yano h_ne
      rw [h_e2, h_e3]
      refine ⟨h_ne, ?_, ?_⟩
      · intro τo Λo Ρoo h_ty
        rw [h_if] at h_ty
        obtain ⟨ht, hl, hr⟩ := h_ty
        have h_bagli : ∃ b0 v0, bolgeOrtamGet S.bolge x = some b0
            ∧ konumGet S.store ⟨b0, 0⟩ = some v0 ∧ DegerTipli Γ Ρ v0 τo := by
          match ht with
          | HasType.t_tanim _ _ _ _ h_gx =>
            exact h_konf.2.2.2.2.2.2.2.2.2.1 x τo h_gx
        obtain ⟨b0, v0, h_b0, h_k0, h_dt0⟩ := h_bagli
        have h_bb0 : b0 = b := Option.some.inj (h_b0.symm.trans h_b)
        rw [h_bb0] at h_k0
        have h_v0 : v0 = v := Option.some.inj (h_k0.symm.trans h_v)
        rw [h_v0] at h_dt0
        match hr with
        | RegionTamam.r_tanim _ _ _ =>
          match hl with
          | LineerTamam.l_tanim_nonlin _ _ _ _ _ _ =>
              exact ⟨ctx.lineer, S.bolge,
                ⟨HasType.t_sabit _ _ _ _ (degerTipli_ortam v τo h_dt0),
                 LineerTamam.l_sabit _ _ _, RegionTamam.r_sabit _ _ _⟩,
                lineerKucuk_refl _, bolgeIliski_refl _⟩
          | LineerTamam.l_tanim_lin _ _ _ _ _ _ _ =>
              exact ⟨ctx.lineer, S.bolge,
                ⟨HasType.t_sabit _ _ _ _ (degerTipli_ortam v τo h_dt0),
                 LineerTamam.l_sabit _ _ _, RegionTamam.r_sabit _ _ _⟩,
                lineerKucuk_update_geri ctx.lineer x, bolgeIliski_refl _⟩
      · intro y bb h_o h_yz
        exact ⟨h_o, fun h => h⟩
  | sAtamaTamam S S' ts1 ts2 ctx x v b h_t h_if h_b h_owner h_S' =>
      intro h_konf ts1o ts2o ctxo ts2o' ctxo' h1 h2 h_yano
      subst h_S'
      have h_ne : ({ ctx with ifade := Ifade.sabit Deger.birim } : ThreadCtx)
          ≠ ctx := by
        intro h_e
        have h_i : Ifade.sabit Deger.birim = ctx.ifade :=
          congrArg ThreadCtx.ifade h_e
        rw [h_if] at h_i
        nomatch h_i
      have h2' : ts1 ++ ({ ctx with ifade := Ifade.sabit Deger.birim } : ThreadCtx)
          :: ts2 = ts1o ++ ctxo' :: ts2o' := h2
      obtain ⟨h_e1, h_e2, h_e3, h_e4, h_e5⟩ :=
        cerrah_kilit ts1o ts1 (h_t.symm.trans h1) h2' (Or.inl rfl) h_yano h_ne
      rw [h_e2, h_e3]
      refine ⟨h_ne, ?_, ?_⟩
      · intro τo Λo Ρoo h_ty
        rw [h_if] at h_ty
        obtain ⟨ht, hl, hr⟩ := h_ty
        match ht, hl, hr with
        | HasType.t_atama _ _ _ _ τx h_gx hte,
          LineerTamam.l_atama _ _ _ _ _ hle,
          RegionTamam.r_atama _ _ _ _ _ b2 hre h_gx2 h_yz2 =>
          match hte, hle, hre with
          | HasType.t_sabit _ _ _ _ _, LineerTamam.l_sabit _ _ _,
            RegionTamam.r_sabit _ _ _ =>
            exact ⟨ctx.lineer, S.bolge,
              ⟨HasType.t_sabit _ _ _ _ DegerTipli.dt_birim,
               LineerTamam.l_sabit _ _ _, RegionTamam.r_sabit _ _ _⟩,
              lineerKucuk_refl _, bolgeIliski_refl _⟩
      · intro y bb h_o h_yz
        exact ⟨h_o, fun h => h⟩
  | sSeqAtla S S' ts1 ts2 ctx v b h_t h_if h_S' =>
      intro h_konf ts1o ts2o ctxo ts2o' ctxo' h1 h2 h_yano
      subst h_S'
      have h_ne : ({ ctx with ifade := b } : ThreadCtx) ≠ ctx := by
        intro h_e
        have h_i : b = ctx.ifade := congrArg ThreadCtx.ifade h_e
        rw [h_if] at h_i
        exact seq_ne_sag (Ifade.sabit v) b h_i.symm
      have h2' : ts1 ++ ({ ctx with ifade := b } : ThreadCtx) :: ts2
          = ts1o ++ ctxo' :: ts2o' := h2
      obtain ⟨h_e1, h_e2, h_e3, h_e4, h_e5⟩ :=
        cerrah_kilit ts1o ts1 (h_t.symm.trans h1) h2' (Or.inl rfl) h_yano h_ne
      rw [h_e2, h_e3]
      refine ⟨h_ne, ?_, ?_⟩
      · intro τo Λo Ρoo h_ty
        rw [h_if] at h_ty
        exact ⟨Λo, Ρoo, typed_seq_atla h_ty,
          lineerKucuk_refl _, bolgeIliski_refl _⟩
      · intro y bb h_o h_yz
        exact ⟨h_o, fun h => h⟩
  | sGuvensizAtla S S' ts1 ts2 ctx v h_t h_if h_S' =>
      intro h_konf ts1o ts2o ctxo ts2o' ctxo' h1 h2 h_yano
      subst h_S'
      have h_ne : ({ ctx with ifade := Ifade.sabit v } : ThreadCtx) ≠ ctx := by
        intro h_e
        have h_i : Ifade.sabit v = ctx.ifade := congrArg ThreadCtx.ifade h_e
        rw [h_if] at h_i
        nomatch h_i
      have h2' : ts1 ++ ({ ctx with ifade := Ifade.sabit v } : ThreadCtx) :: ts2
          = ts1o ++ ctxo' :: ts2o' := h2
      obtain ⟨h_e1, h_e2, h_e3, h_e4, h_e5⟩ :=
        cerrah_kilit ts1o ts1 (h_t.symm.trans h1) h2' (Or.inl rfl) h_yano h_ne
      rw [h_e2, h_e3]
      refine ⟨h_ne, ?_, ?_⟩
      · intro τo Λo Ρoo h_ty
        rw [h_if] at h_ty
        obtain ⟨ht, hl, hr⟩ := h_ty
        match ht, hl, hr with
        | HasType.t_guvensiz _ _ _ _ hte,
          LineerTamam.l_guvensiz _ _ _ _ hle,
          RegionTamam.r_guvensiz _ _ _ _ hre =>
          exact ⟨Λo, Ρoo, ⟨hte, hle, hre⟩,
            lineerKucuk_refl _, bolgeIliski_refl _⟩
      · intro y bb h_o h_yz
        exact ⟨h_o, fun h => h⟩
  | sLinKullanTamam S S' ts1 ts2 ctx x h_t h_if h_aktif h_S' =>
      intro h_konf ts1o ts2o ctxo ts2o' ctxo' h1 h2 h_yano
      subst h_S'
      have h_ne : ({ ctx with ifade := Ifade.sabit Deger.birim, lineer := lineerOrtamUpdate ctx.lineer x Lineerlik.tuketildi } : ThreadCtx) ≠ ctx := by
        intro h_e
        have h_i : Ifade.sabit Deger.birim = ctx.ifade :=
          congrArg ThreadCtx.ifade h_e
        rw [h_if] at h_i
        nomatch h_i
      have h2' : ts1 ++ ({ ctx with ifade := Ifade.sabit Deger.birim, lineer := lineerOrtamUpdate ctx.lineer x Lineerlik.tuketildi } : ThreadCtx) :: ts2 = ts1o ++ ctxo' :: ts2o' := h2
      obtain ⟨h_e1, h_e2, h_e3, h_e4, h_e5⟩ :=
        cerrah_kilit ts1o ts1 (h_t.symm.trans h1) h2' (Or.inl rfl) h_yano h_ne
      rw [h_e2, h_e3]
      refine ⟨h_ne, ?_, ?_⟩
      · intro τo Λo Ρoo h_ty
        rw [h_if] at h_ty
        obtain ⟨ht, hl, hr⟩ := h_ty
        match ht, hl, hr with
        | HasType.t_kullan _ _ _ _ _,
          LineerTamam.l_kullan _ _ _ _ _ _,
          RegionTamam.r_kullan _ _ _ =>
          exact ⟨lineerOrtamUpdate ctx.lineer x Lineerlik.tuketildi, S.bolge,
            ⟨HasType.t_sabit _ _ _ _ DegerTipli.dt_birim,
             LineerTamam.l_sabit _ _ _, RegionTamam.r_sabit _ _ _⟩,
            lineerKucuk_refl _, bolgeIliski_refl _⟩
      · intro y bb h_o h_yz
        exact ⟨h_o, fun h => h⟩
  | sLinImhaTamam S S' ts1 ts2 ctx x h_t h_if h_aktif h_S' =>
      intro h_konf ts1o ts2o ctxo ts2o' ctxo' h1 h2 h_yano
      subst h_S'
      have h_ne : ({ ctx with ifade := Ifade.sabit Deger.birim, lineer := lineerOrtamUpdate ctx.lineer x Lineerlik.tuketildi } : ThreadCtx) ≠ ctx := by
        intro h_e
        have h_i : Ifade.sabit Deger.birim = ctx.ifade :=
          congrArg ThreadCtx.ifade h_e
        rw [h_if] at h_i
        nomatch h_i
      have h2' : ts1 ++ ({ ctx with ifade := Ifade.sabit Deger.birim, lineer := lineerOrtamUpdate ctx.lineer x Lineerlik.tuketildi } : ThreadCtx) :: ts2 = ts1o ++ ctxo' :: ts2o' := h2
      obtain ⟨h_e1, h_e2, h_e3, h_e4, h_e5⟩ :=
        cerrah_kilit ts1o ts1 (h_t.symm.trans h1) h2' (Or.inl rfl) h_yano h_ne
      rw [h_e2, h_e3]
      refine ⟨h_ne, ?_, ?_⟩
      · intro τo Λo Ρoo h_ty
        rw [h_if] at h_ty
        obtain ⟨ht, hl, hr⟩ := h_ty
        match ht, hl, hr with
        | HasType.t_imha _ _ _ _ _,
          LineerTamam.l_imha _ _ _ _ _ _,
          RegionTamam.r_imha _ _ _ =>
          exact ⟨lineerOrtamUpdate ctx.lineer x Lineerlik.tuketildi, S.bolge,
            ⟨HasType.t_sabit _ _ _ _ DegerTipli.dt_birim,
             LineerTamam.l_sabit _ _ _, RegionTamam.r_sabit _ _ _⟩,
            lineerKucuk_refl _, bolgeIliski_refl _⟩
      · intro y bb h_o h_yz
        exact ⟨h_o, fun h => h⟩
  | cGorevBaslatTamam S S' ts1 ts2 ctx tYeni yd kod h_t h_if h_fresh h_sahipler h_S' =>
      intro h_konf ts1o ts2o ctxo ts2o' ctxo' h1 h2 h_yano
      subst h_S'
      have h_ne : ({ ctx with ifade := Ifade.sabit (Deger.gorevVal tYeni), lineer := lineerTuketListe ctx.lineer yd } : ThreadCtx) ≠ ctx := by
        intro h_e
        have h_i : Ifade.sabit (Deger.gorevVal tYeni) = ctx.ifade :=
          congrArg ThreadCtx.ifade h_e
        rw [h_if] at h_i
        nomatch h_i
      have h2' : ts1 ++ ({ ctx with ifade := Ifade.sabit (Deger.gorevVal tYeni), lineer := lineerTuketListe ctx.lineer yd } : ThreadCtx)
          :: (ts2 ++ [⟨tYeni, kod, yd.map (fun v => (v, Lineerlik.aktif))⟩])
          = ts1o ++ ctxo' :: ts2o' := by
        rw [show ts1 ++ ({ ctx with ifade := Ifade.sabit (Deger.gorevVal tYeni), lineer := lineerTuketListe ctx.lineer yd } : ThreadCtx)
            :: (ts2 ++ [⟨tYeni, kod, yd.map (fun v => (v, Lineerlik.aktif))⟩])
          = (ts1 ++ { ctx with ifade := Ifade.sabit (Deger.gorevVal tYeni), lineer := lineerTuketListe ctx.lineer yd } :: ts2)
            ++ [⟨tYeni, kod, yd.map (fun v => (v, Lineerlik.aktif))⟩]
          by simp]
        exact h2
      obtain ⟨h_e1, h_e2, h_e3, h_e4, h_e5⟩ :=
        cerrah_kilit ts1o ts1 (h_t.symm.trans h1) h2'
          (Or.inr ⟨_, rfl⟩) h_yano h_ne
      rw [h_e2, h_e3]
      refine ⟨h_ne, ?_, ?_⟩
      · intro τo Λo Ρoo h_ty
        rw [h_if] at h_ty
        obtain ⟨ht, hl, hr⟩ := h_ty
        match ht, hl, hr with
        | HasType.t_gorev_baslat _ _ _ _ τd h_tkod,
          LineerTamam.l_gorev_baslat _ _ Λkod _ _ h_ntuk h_lkod,
          RegionTamam.r_gorev_baslat _ _ _ Ρkod _ _ tYs
            h_cap h_khv h_khb h_rkod h_eqR =>
          subst h_eqR
          refine ⟨lineerTuketListe ctx.lineer yd,
            bolgeOrtamSahipAta S.bolge yd tYeni,
            ⟨HasType.t_sabit _ _ _ _ (DegerTipli.dt_gorev tYeni τd),
             LineerTamam.l_sabit _ _ _, RegionTamam.r_sabit _ _ _⟩,
            lineerKucuk_refl _, ?_, ?_, ?_⟩
          · intro y bb h_o h_yz
            by_cases h_in : y ∈ yd
            · cases h_lk : bolgeOrtamGet S.bolge y with
              | none =>
                  rw [sahipAta_get_none _ _ _ _ h_lk] at h_o; cases h_o
              | some b0 =>
                  rw [sahipAta_get_in' _ _ _ _ b0 h_in h_lk] at h_o
                  rw [← Option.some.inj h_o] at h_yz
                  simp [bolgeKategoriDegistir, kategoriYazilabilir] at h_yz
            · rw [sahipAta_get_notin _ _ _ _ h_in] at h_o
              rw [sahipAta_get_notin _ _ _ _ h_in]
              exact h_o
          · intro y h_o
            exact sahipAta_get_none _ _ _ _ (sahipAta_get_none_inv _ _ _ _ h_o)
          · intro y bb h_o
            by_cases h_in : y ∈ yd
            · cases h_lk : bolgeOrtamGet S.bolge y with
              | none =>
                  rw [sahipAta_get_none _ _ _ _ h_lk] at h_o; cases h_o
              | some b0 =>
                  rw [sahipAta_get_in' _ _ _ _ b0 h_in h_lk] at h_o
                  refine ⟨bolgeKategoriDegistir b0 (BolgeKategorisi.sahip tYeni),
                    sahipAta_get_in' _ _ _ _ b0 h_in h_lk, ?_⟩
                  rw [← Option.some.inj h_o]
                  rfl
            · rw [sahipAta_get_notin _ _ _ _ h_in] at h_o
              exact ⟨bb, by rw [sahipAta_get_notin _ _ _ _ h_in]; exact h_o,
                rfl⟩
      · intro y bb h_o h_yz
        have h_o2 : bolgeOrtamGet (bolgeOrtamSahipAta S.bolge yd tYeni) y
            = some bb := h_o
        by_cases h_in : y ∈ yd
        · exfalso
          cases h_lk : bolgeOrtamGet S.bolge y with
          | none => rw [sahipAta_get_none _ _ _ _ h_lk] at h_o2; cases h_o2
          | some b0 =>
              rw [sahipAta_get_in' _ _ _ _ b0 h_in h_lk] at h_o2
              rw [← Option.some.inj h_o2] at h_yz
              simp [bolgeKategoriDegistir, kategoriYazilabilir] at h_yz
        · rw [sahipAta_get_notin _ _ _ _ h_in] at h_o2
          refine ⟨h_o2, ?_⟩
          intro h_own
          have h_yok : ∀ b'' ∈ bolgeleriTopla S.bolge yd, b''.id ≠ bb.id := by
            intro b'' h_bin h_id
            rcases List.mem_filterMap.mp h_bin with ⟨v0, h_v0, h_v0lk⟩
            have h_vy : v0 = y :=
              h_konf.2.2.2.2.2.2.2.2.2.2.2.1 v0 y b'' bb h_v0lk h_o2 h_id
            rw [h_vy] at h_v0
            exact h_in h_v0
          show sahiplikGet (sahiplikSetMany S.sahiplik
              (bolgeleriTopla S.bolge yd) (Sahip.thread tYeni)) bb
              = some (Sahip.thread ctx.tid)
          rw [sahiplikSetMany_ne _ _ _ _ h_yok]
          exact h_own
  | cGorevBirlestirTamam S S' ts1 ts2 ctx g tHedef rb h_t h_if h_hedef h_donen h_S' =>
      intro h_konf ts1o ts2o ctxo ts2o' ctxo' h1 h2 h_yano
      subst h_S'
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      have h_ne : ({ ctx with ifade := Ifade.sabit Deger.birim } : ThreadCtx)
          ≠ ctx := by
        intro h_e
        have h_i : Ifade.sabit Deger.birim = ctx.ifade :=
          congrArg ThreadCtx.ifade h_e
        rw [h_if] at h_i
        nomatch h_i
      have h2' : ts1 ++ ({ ctx with ifade := Ifade.sabit Deger.birim } : ThreadCtx)
          :: ts2 = ts1o ++ ctxo' :: ts2o' := h2
      obtain ⟨h_e1, h_e2, h_e3, h_e4, h_e5⟩ :=
        cerrah_kilit ts1o ts1 (h_t.symm.trans h1) h2' (Or.inl rfl) h_yano h_ne
      rw [h_e2, h_e3]
      refine ⟨h_ne, ?_, ?_⟩
      · intro τo Λo Ρoo h_ty
        rw [h_if] at h_ty
        obtain ⟨ht, hl, hr⟩ := h_ty
        match ht, hl, hr with
        | HasType.t_gorev_birlestir _ _ _ _ _,
          LineerTamam.l_gorev_birlestir _ _ _,
          RegionTamam.r_gorev_birlestir _ _ _ =>
          exact ⟨ctx.lineer, S.bolge,
            ⟨HasType.t_sabit _ _ _ _ DegerTipli.dt_birim,
             LineerTamam.l_sabit _ _ _, RegionTamam.r_sabit _ _ _⟩,
            lineerKucuk_refl _, bolgeIliski_refl _⟩
      · intro y bb h_o h_yz
        refine ⟨h_o, ?_⟩
        intro h_own
        have h_yok : ∀ b'' ∈ rb, b''.id ≠ bb.id := by
          intro b'' h_in h_id
          have h_f := h_donen b'' h_in
          rw [sahiplikGet_id_esit S.sahiplik b'' bb h_id, h_own] at h_f
          have h_t2 := Option.some.inj h_f
          injection h_t2 with h_t3
          obtain ⟨hctx, h_hin, h_htid, vSon, h_hif⟩ := h_hedef
          have h13 := h_konf.2.2.2.2.2.2.2.2.2.2.2.2.1
          have h_ce : ctx = hctx :=
            tidAyrik_tekil h13 h_ctx_in h_hin (h_t3.trans h_htid.symm)
          rw [← h_ce] at h_hif
          rw [h_if] at h_hif
          nomatch h_hif
        show sahiplikGet (sahiplikSetMany S.sahiplik rb
            (Sahip.thread ctx.tid)) bb = some (Sahip.thread ctx.tid)
        rw [sahiplikSetMany_ne _ _ _ _ h_yok]
        exact h_own
  | cKanalGonderTamam S S' ts1 ts2 ctx k vId b v h_t h_if h_b h_v h_owner h_bos h_S' =>
      intro h_konf ts1o ts2o ctxo ts2o' ctxo' h1 h2 h_yano
      subst h_S'
      have h_ne : ({ ctx with ifade := Ifade.sabit Deger.birim, lineer := lineerTuket ctx.lineer vId } : ThreadCtx) ≠ ctx := by
        intro h_e
        have h_i : Ifade.sabit Deger.birim = ctx.ifade :=
          congrArg ThreadCtx.ifade h_e
        rw [h_if] at h_i
        nomatch h_i
      have h2' : ts1 ++ ({ ctx with ifade := Ifade.sabit Deger.birim, lineer := lineerTuket ctx.lineer vId } : ThreadCtx) :: ts2
          = ts1o ++ ctxo' :: ts2o' := h2
      obtain ⟨h_e1, h_e2, h_e3, h_e4, h_e5⟩ :=
        cerrah_kilit ts1o ts1 (h_t.symm.trans h1) h2' (Or.inl rfl) h_yano h_ne
      rw [h_e2, h_e3]
      refine ⟨h_ne, ?_, ?_⟩
      · intro τo Λo Ρoo h_ty
        rw [h_if] at h_ty
        obtain ⟨ht, hl, hr⟩ := h_ty
        match ht, hl, hr with
        | HasType.t_kanal_gonder _ _ _ _ _,
          LineerTamam.l_kanal_gonder _ _ _ _ _,
          RegionTamam.r_kanal_gonder _ _ _ _ _ bR h_bR h_yzR h_eqR =>
          subst h_eqR
          have h_eqb : bR = b := Option.some.inj (h_bR.symm.trans h_b)
          refine ⟨lineerTuket ctx.lineer vId,
            bolgeOrtamUpdate S.bolge vId
              (bolgeKategoriDegistir bR (BolgeKategorisi.kanalRho k)),
            ⟨HasType.t_sabit _ _ _ _ DegerTipli.dt_birim,
             LineerTamam.l_sabit _ _ _, ?_⟩,
            lineerKucuk_refl _, bolgeIliski_refl _⟩
          rw [h_eqb]
          exact RegionTamam.r_sabit _ _ _
      · intro y bb h_o h_yz
        have h_o2 : bolgeOrtamGet (bolgeOrtamUpdate S.bolge vId
            (bolgeKategoriDegistir b (BolgeKategorisi.kanalRho k))) y
            = some bb := h_o
        rw [bolgeOrtamUpdate_get] at h_o2
        by_cases hv : vId = y
        · rw [if_pos hv] at h_o2
          rw [← Option.some.inj h_o2] at h_yz
          simp [bolgeKategoriDegistir, kategoriYazilabilir] at h_yz
        · rw [if_neg hv] at h_o2
          refine ⟨h_o2, ?_⟩
          intro h_own
          have h_ne2 : bb.id ≠ b.id := by
            intro h_id
            exact hv (h_konf.2.2.2.2.2.2.2.2.2.2.2.1 vId y b bb h_b h_o2
              h_id.symm)
          show sahiplikGet (sahiplikSet S.sahiplik b (Sahip.kanalSahip k)) bb
              = some (Sahip.thread ctx.tid)
          rw [sahiplikSet_ne _ _ _ _ h_ne2]
          exact h_own
  | cKanalAlTamam S S' ts1 ts2 ctx k v tb h_t h_if h_v h_transit h_S' =>
      intro h_konf ts1o ts2o ctxo ts2o' ctxo' h1 h2 h_yano
      subst h_S'
      have h_ne : ({ ctx with ifade := Ifade.sabit v } : ThreadCtx) ≠ ctx := by
        intro h_e
        have h_i : Ifade.sabit v = ctx.ifade := congrArg ThreadCtx.ifade h_e
        rw [h_if] at h_i
        nomatch h_i
      have h2' : ts1 ++ ({ ctx with ifade := Ifade.sabit v } : ThreadCtx) :: ts2
          = ts1o ++ ctxo' :: ts2o' := h2
      obtain ⟨h_e1, h_e2, h_e3, h_e4, h_e5⟩ :=
        cerrah_kilit ts1o ts1 (h_t.symm.trans h1) h2' (Or.inl rfl) h_yano h_ne
      rw [h_e2, h_e3]
      have h_kdF : ∃ kdF ∈ S.kanal, kdF.kid = k
          ∧ kdF.gonderKuyrugu.head? = some v := by
        unfold kanalIlk at h_v
        cases h_f : S.kanal.find? (fun kd => kd.kid = k) with
        | none => rw [h_f] at h_v; cases h_v
        | some kdF =>
            rw [h_f] at h_v
            have h_q : kdF.gonderKuyrugu.head? = some v := h_v
            refine ⟨kdF, List.mem_of_find?_eq_some h_f, ?_, h_q⟩
            have h_pred := List.find?_some h_f
            simpa using h_pred
      obtain ⟨kdF, h_kdF_in, h_kdF_kid, h_kdF_head⟩ := h_kdF
      have h_dtv := h_konf.2.2.2.1 kdF h_kdF_in v (head?_mem h_kdF_head)
      refine ⟨h_ne, ?_, ?_⟩
      · intro τo Λo Ρoo h_ty
        rw [h_if] at h_ty
        obtain ⟨ht, hl, hr⟩ := h_ty
        match ht, hl, hr with
        | HasType.t_kanal_al _ _ _,
          LineerTamam.l_kanal_al _ _ _,
          RegionTamam.r_kanal_al _ _ _ =>
          refine ⟨ctx.lineer, S.bolge,
            ⟨HasType.t_sabit _ _ _ _ ?_,
             LineerTamam.l_sabit _ _ _, RegionTamam.r_sabit _ _ _⟩,
            lineerKucuk_refl _, bolgeIliski_refl _⟩
          rw [← h_kdF_kid]
          exact degerTipli_ortam v _ h_dtv
      · intro y bb h_o h_yz
        refine ⟨h_o, ?_⟩
        intro h_own
        by_cases hid : bb.id = tb.id
        · show sahiplikGet (sahiplikSet S.sahiplik tb (Sahip.thread ctx.tid)) bb
              = some (Sahip.thread ctx.tid)
          rw [sahiplikSet, sahiplikGet, if_pos hid.symm]
        · show sahiplikGet (sahiplikSet S.sahiplik tb (Sahip.thread ctx.tid)) bb
              = some (Sahip.thread ctx.tid)
          rw [sahiplikSet_ne _ _ _ _ hid]
          exact h_own
  | cDondurTamam S S' ts1 ts2 ctx b h_t h_if h_owner h_S' =>
      intro h_konf ts1o ts2o ctxo ts2o' ctxo' h1 h2 h_yano
      subst h_S'
      have h_ne : ({ ctx with ifade := Ifade.sabit Deger.birim } : ThreadCtx)
          ≠ ctx := by
        intro h_e
        have h_i : Ifade.sabit Deger.birim = ctx.ifade :=
          congrArg ThreadCtx.ifade h_e
        rw [h_if] at h_i
        nomatch h_i
      have h2' : ts1 ++ ({ ctx with ifade := Ifade.sabit Deger.birim } : ThreadCtx)
          :: ts2 = ts1o ++ ctxo' :: ts2o' := h2
      obtain ⟨h_e1, h_e2, h_e3, h_e4, h_e5⟩ :=
        cerrah_kilit ts1o ts1 (h_t.symm.trans h1) h2' (Or.inl rfl) h_yano h_ne
      rw [h_e2, h_e3]
      refine ⟨h_ne, ?_, ?_⟩
      · intro τo Λo Ρoo h_ty
        rw [h_if] at h_ty
        obtain ⟨ht, hl, hr⟩ := h_ty
        match ht, hl, hr with
        | HasType.t_dondur _ _ _,
          LineerTamam.l_dondur _ _ _,
          RegionTamam.r_dondur _ _ _ _ x0 h_x0 h_yzb h_eqR =>
          subst h_eqR
          exact ⟨ctx.lineer, bolgeOrtamDondurBolge S.bolge b,
            ⟨HasType.t_sabit _ _ _ _ DegerTipli.dt_birim,
             LineerTamam.l_sabit _ _ _, RegionTamam.r_sabit _ _ _⟩,
            lineerKucuk_refl _, bolgeIliski_refl _⟩
      · intro y bb h_o h_yz
        have h_o2 : bolgeOrtamGet (bolgeOrtamDondurBolge S.bolge b) y
            = some bb := h_o
        rw [dondur_get] at h_o2
        cases h_lk : bolgeOrtamGet S.bolge y with
        | none => rw [h_lk] at h_o2; cases h_o2
        | some b0 =>
            rw [h_lk, Option.map_some] at h_o2
            by_cases hid : b0.id = b.id
            · rw [if_pos hid] at h_o2
              rw [← Option.some.inj h_o2] at h_yz
              simp [bolgeKategoriDegistir, kategoriYazilabilir] at h_yz
            · rw [if_neg hid] at h_o2
              have h_b0bb := Option.some.inj h_o2
              rw [h_b0bb] at hid
              refine ⟨congrArg Option.some h_b0bb, ?_⟩
              intro h_own
              show sahiplikGet (sahiplikSet S.sahiplik b Sahip.donmus) bb
                  = some (Sahip.thread ctx.tid)
              rw [sahiplikSet_ne _ _ _ _ hid]
              exact h_own
  -- ============ Hata kurallari: Aile 2 exfalso ============
  | sAtamaHataDonmus S S' ts1 ts2 ctx x v b h_t h_if h_b h_frozen h_S' =>
      intro h_konf
      exfalso
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      obtain ⟨τ', Λ'', Ρ'', h_typed⟩ := h_konf.2.1 ctx h_ctx_in
      rw [h_if] at h_typed
      exact typing_excludes_sAtamaHataDonmus Γ Δ ctx.lineer Ρ x _ τ' Λ'' Ρ''
        h_typed S b h_konf.2.2.2.2.2.1 h_konf.2.2.2.2.2.2.1 h_b h_frozen
  | sAtamaHataSahipDegil S S' ts1 ts2 ctx x v b h_t h_if h_b h_not_owner h_S' =>
      intro h_konf
      exfalso
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      obtain ⟨τ', Λ'', Ρ'', h_typed⟩ := h_konf.2.1 ctx h_ctx_in
      rw [h_if] at h_typed
      exact typing_excludes_sAtamaHataSahipDegil Γ Δ ctx.lineer Ρ S ctx x _ b
        τ' Λ'' Ρ'' h_typed h_ctx_in h_if h_konf.2.2.2.2.2.1
        h_konf.2.2.2.2.2.2.2.1 h_b h_not_owner
  | cGorevBaslatHataLineerIhlal S S' ts1 ts2 ctx yd kod vIhlal h_t h_if h_in h_tuket h_S' =>
      intro h_konf
      exfalso
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      obtain ⟨τ', Λ'', Ρ'', h_typed⟩ := h_konf.2.1 ctx h_ctx_in
      rw [h_if] at h_typed
      exact typing_excludes_cGorevBaslatHataLineerIhlal Γ Δ ctx.lineer Ρ
        yd kod vIhlal τ' Λ'' Ρ'' h_typed h_in h_tuket
  | cKanalGonderHataLineerTuket S S' ts1 ts2 ctx k vId h_t h_if h_tuket h_S' =>
      intro h_konf
      exfalso
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      obtain ⟨τ', Λ'', Ρ'', h_typed⟩ := h_konf.2.1 ctx h_ctx_in
      rw [h_if] at h_typed
      exact typing_excludes_cKanalGonderHataLineerTuket Γ Δ ctx.lineer Ρ
        k vId τ' Λ'' Ρ'' h_typed h_tuket
  | cDondurHataZatenDonmus S S' ts1 ts2 ctx b h_t h_if h_zaten h_S' =>
      intro h_konf
      exfalso
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      obtain ⟨τ', Λ'', Ρ'', h_typed⟩ := h_konf.2.1 ctx h_ctx_in
      rw [h_if] at h_typed
      exact typing_excludes_cDondurHataZatenDonmus Γ Δ ctx.lineer Ρ
        b τ' Λ'' Ρ'' h_typed S
        h_konf.2.2.2.2.2.1 h_konf.2.2.2.2.2.2.1 h_zaten
  | sLinKullanHataZatenTuketildi S S' ts1 ts2 ctx x h_t h_if h_tuket h_S' =>
      intro h_konf
      exfalso
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      obtain ⟨τ', Λ'', Ρ'', h_typed⟩ := h_konf.2.1 ctx h_ctx_in
      rw [h_if] at h_typed
      exact typing_excludes_sLinKullanHataZatenTuketildi Γ Δ ctx.lineer Ρ
        x τ' Λ'' Ρ'' h_typed h_tuket
  | sLinImhaHataZatenTuketildi S S' ts1 ts2 ctx x h_t h_if h_tuket h_S' =>
      intro h_konf
      exfalso
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      obtain ⟨τ', Λ'', Ρ'', h_typed⟩ := h_konf.2.1 ctx h_ctx_in
      rw [h_if] at h_typed
      exact typing_excludes_sLinImhaHataZatenTuketildi Γ Δ ctx.lineer Ρ
        x τ' Λ'' Ρ'' h_typed h_tuket
  -- ============ Congruence: ic-yukten kompozisyon ============
  | sSeqCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' a a' b h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      intro h_konf ts1o ts2o ctxo ts2o' ctxo' h1 h2 h_yano
      subst h_S1
      subst h_S'
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      obtain ⟨τ0, Λ0, Ρ0, h_ty0⟩ := h_konf.2.1 ctx h_ctx_in
      rw [h_if] at h_ty0
      have h_konf1 : KonfTipliFull Γ Δ Ρ (ifadeyleKonf S ts1 ts2 ctx a) :=
        konfTipliFull_odak Γ Δ Ρ S ts1 ts2 ctx a h_konf h_t
          (typed_seq_sol h_ty0)
          (fun y h => by rw [h_if]; exact HedefVar.seq_sol a b y h)
          (fun bb h => by rw [h_if]; exact HedefBolge.seq_sol a b bb h)
      obtain ⟨h_nei, h_A, h_B⟩ :=
        ih h_konf1 ts1 ts2 { ctx with ifade := a } ts2' ctx' rfl h_t1' h_yan
      have h_ne : ({ ctx' with ifade := Ifade.seq a' b } : ThreadCtx) ≠ ctx := by
        intro h_e
        have h_if2x := congrArg ThreadCtx.ifade h_e
        have h_if2 : Ifade.seq a' b = ctx.ifade := h_if2x
        have h_lin2x := congrArg ThreadCtx.lineer h_e
        have h_lin2 : ctx'.lineer = ctx.lineer := h_lin2x
        rw [h_if] at h_if2
        injection h_if2 with h_aa _
        apply h_nei
        show (⟨ctx'.tid, ctx'.ifade, ctx'.lineer⟩ : ThreadCtx)
            = ⟨ctx.tid, a, ctx.lineer⟩
        rw [h_tid, h_if', h_aa, h_lin2]
      have h2' : ts1 ++ ({ ctx' with ifade := Ifade.seq a' b } : ThreadCtx)
          :: ts2' = ts1o ++ ctxo' :: ts2o' := h2
      obtain ⟨h_e1, h_e2, h_e3, h_e4, h_e5⟩ :=
        cerrah_kilit ts1o ts1 (h_t.symm.trans h1) h2' h_yan h_yano h_ne
      rw [h_e2, h_e3]
      refine ⟨h_ne, ?_, ?_⟩
      · intro τo Λo Ρoo h_ty
        rw [h_if] at h_ty
        obtain ⟨ht, hl, hr⟩ := h_ty
        match ht, hl, hr with
        | HasType.t_seq _ _ _ _ τa _ hta htb,
          LineerTamam.l_seq _ _ Λa _ _ _ hla hlb,
          RegionTamam.r_seq _ _ Ρa _ _ _ hra hrb =>
          obtain ⟨Λa', Ρa', h_tya', h_ka, h_ia⟩ := h_A τa Λa Ρa ⟨hta, hla, hra⟩
          rw [h_if'] at h_tya'
          obtain ⟨Λb', h_lb', h_kb⟩ := lineerTamam_kucuk_transport hlb Λa' h_ka
          obtain ⟨Ρb', h_rb', h_ib⟩ := regionTamam_iliski_transport hrb Ρa' h_ia
          exact ⟨Λb', Ρb',
            ⟨HasType.t_seq _ _ _ _ _ _ h_tya'.hasType htb,
             LineerTamam.l_seq _ _ _ _ a' b h_tya'.lineerOK h_lb',
             RegionTamam.r_seq _ _ _ _ a' b h_tya'.regionOK h_rb'⟩,
            h_kb, h_ib⟩
      · exact h_B
  | sAtamaCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' x e e' h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      intro h_konf ts1o ts2o ctxo ts2o' ctxo' h1 h2 h_yano
      subst h_S1
      subst h_S'
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      obtain ⟨τ0, Λ0, Ρ0, h_ty0⟩ := h_konf.2.1 ctx h_ctx_in
      rw [h_if] at h_ty0
      have h_konf1 : KonfTipliFull Γ Δ Ρ (ifadeyleKonf S ts1 ts2 ctx e) :=
        konfTipliFull_odak Γ Δ Ρ S ts1 ts2 ctx e h_konf h_t
          (typed_atama_ic h_ty0)
          (fun y h => by rw [h_if]; exact HedefVar.atama_ic x e y h)
          (fun bb h => by rw [h_if]; exact HedefBolge.atama_ic x e bb h)
      obtain ⟨h_nei, h_A, h_B⟩ :=
        ih h_konf1 ts1 ts2 { ctx with ifade := e } ts2' ctx' rfl h_t1' h_yan
      have h_ne : ({ ctx' with ifade := Ifade.atama x e' } : ThreadCtx)
          ≠ ctx := by
        intro h_e
        have h_if2x := congrArg ThreadCtx.ifade h_e
        have h_if2 : Ifade.atama x e' = ctx.ifade := h_if2x
        have h_lin2x := congrArg ThreadCtx.lineer h_e
        have h_lin2 : ctx'.lineer = ctx.lineer := h_lin2x
        rw [h_if] at h_if2
        injection h_if2 with _ h_ee
        apply h_nei
        show (⟨ctx'.tid, ctx'.ifade, ctx'.lineer⟩ : ThreadCtx)
            = ⟨ctx.tid, e, ctx.lineer⟩
        rw [h_tid, h_if', h_ee, h_lin2]
      have h2' : ts1 ++ ({ ctx' with ifade := Ifade.atama x e' } : ThreadCtx)
          :: ts2' = ts1o ++ ctxo' :: ts2o' := h2
      obtain ⟨h_e1, h_e2, h_e3, h_e4, h_e5⟩ :=
        cerrah_kilit ts1o ts1 (h_t.symm.trans h1) h2' h_yan h_yano h_ne
      rw [h_e2, h_e3]
      refine ⟨h_ne, ?_, ?_⟩
      · intro τo Λo Ρoo h_ty
        rw [h_if] at h_ty
        obtain ⟨ht, hl, hr⟩ := h_ty
        match ht, hl, hr with
        | HasType.t_atama _ _ _ _ τx h_gx hte,
          LineerTamam.l_atama _ _ _ _ _ hle,
          RegionTamam.r_atama _ _ _ _ _ bA hre h_gxA h_yzA =>
          obtain ⟨Λe', Ρe', h_tye', h_ke, h_ie⟩ := h_A τx Λo Ρoo ⟨hte, hle, hre⟩
          rw [h_if'] at h_tye'
          exact ⟨Λe', Ρe',
            ⟨HasType.t_atama _ _ _ _ τx h_gx h_tye'.hasType,
             LineerTamam.l_atama _ _ _ _ _ h_tye'.lineerOK,
             RegionTamam.r_atama _ _ _ _ _ bA h_tye'.regionOK
               (h_ie.1 x bA h_gxA h_yzA) h_yzA⟩,
            h_ke, h_ie⟩
      · exact h_B
  | sGuvensizCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' e e' h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      intro h_konf ts1o ts2o ctxo ts2o' ctxo' h1 h2 h_yano
      subst h_S1
      subst h_S'
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      obtain ⟨τ0, Λ0, Ρ0, h_ty0⟩ := h_konf.2.1 ctx h_ctx_in
      rw [h_if] at h_ty0
      have h_konf1 : KonfTipliFull Γ Δ Ρ (ifadeyleKonf S ts1 ts2 ctx e) :=
        konfTipliFull_odak Γ Δ Ρ S ts1 ts2 ctx e h_konf h_t
          (typed_guvensiz_ic h_ty0)
          (fun y h => by rw [h_if]; exact HedefVar.guvensiz_ic e y h)
          (fun bb h => by rw [h_if]; exact HedefBolge.guvensiz_ic e bb h)
      obtain ⟨h_nei, h_A, h_B⟩ :=
        ih h_konf1 ts1 ts2 { ctx with ifade := e } ts2' ctx' rfl h_t1' h_yan
      have h_ne : ({ ctx' with ifade := Ifade.guvensiz e' } : ThreadCtx)
          ≠ ctx := by
        intro h_e
        have h_if2x := congrArg ThreadCtx.ifade h_e
        have h_if2 : Ifade.guvensiz e' = ctx.ifade := h_if2x
        have h_lin2x := congrArg ThreadCtx.lineer h_e
        have h_lin2 : ctx'.lineer = ctx.lineer := h_lin2x
        rw [h_if] at h_if2
        injection h_if2 with h_ee
        apply h_nei
        show (⟨ctx'.tid, ctx'.ifade, ctx'.lineer⟩ : ThreadCtx)
            = ⟨ctx.tid, e, ctx.lineer⟩
        rw [h_tid, h_if', h_ee, h_lin2]
      have h2' : ts1 ++ ({ ctx' with ifade := Ifade.guvensiz e' } : ThreadCtx)
          :: ts2' = ts1o ++ ctxo' :: ts2o' := h2
      obtain ⟨h_e1, h_e2, h_e3, h_e4, h_e5⟩ :=
        cerrah_kilit ts1o ts1 (h_t.symm.trans h1) h2' h_yan h_yano h_ne
      rw [h_e2, h_e3]
      refine ⟨h_ne, ?_, ?_⟩
      · intro τo Λo Ρoo h_ty
        rw [h_if] at h_ty
        obtain ⟨ht, hl, hr⟩ := h_ty
        match ht, hl, hr with
        | HasType.t_guvensiz _ _ _ _ hte,
          LineerTamam.l_guvensiz _ _ _ _ hle,
          RegionTamam.r_guvensiz _ _ _ _ hre =>
          obtain ⟨Λe', Ρe', h_tye', h_ke, h_ie⟩ := h_A τo Λo Ρoo ⟨hte, hle, hre⟩
          rw [h_if'] at h_tye'
          exact ⟨Λe', Ρe',
            ⟨HasType.t_guvensiz _ _ _ _ h_tye'.hasType,
             LineerTamam.l_guvensiz _ _ _ _ h_tye'.lineerOK,
             RegionTamam.r_guvensiz _ _ _ _ h_tye'.regionOK⟩,
            h_ke, h_ie⟩
      · exact h_B


-- ============================================================
-- §2. ADIM KORUNUM — birlesik korunum (F4 cekirdegi, dairesellik kirici)
-- ============================================================

/-- BIRLESIK ADIM KORUNUMU (FAZ_BRIFINGLERI.md F4 madde 2):
    Tipli konfigurasyon bir adim sonra da tiplidir (bolge ortami evrilir).

    KAPANIS DURUMU (Onarim v3 id-anahtarlama passi — 18/21):
    - Hata (7): TAM — Aile 2 exfalso (step_fault deseninin aynisi).
    - Tamam (11/11) TAM: sVarOku / sSeqAtla / sGuvensizAtla / sLinKullan /
      sLinImha / sAtamaTamam / cGorevBirlestir / cKanalAl (degisim-
      yardimcilari + BolgeAyrik/TidAyrik/kapasite-1) + Ρ-DEGISTIREN
      cGorevBaslat / cKanalGonder / cDondur (id-anahtarlama +
      Yol-B hedefsiz-govde premise + regionTamam_transport/yaz_geri).
    - KALAN (3): yalniz congruence (sSeqCong/sAtamaCong/sGuvensizCong) —
      TEK BLOKER: odak-adim guclendirilmis-IH ("kalan-yukumluluk
      tasima"); tasarim cong-blok yorumunda + DECISIONS_LOG.md.
      Final teorem ifadesini DEGISTIRMEZ (ic-lemma guclendirmesi). -/
theorem adim_korunum
    (Γ : TipOrtam) (Δ : KanalOrtam) (Ρ : BolgeOrtam)
    (S S' : Konfigurasyon) (h_step : Step S S')
    (h_konf : KonfTipliFull Γ Δ Ρ S) :
    ∃ Ρ', KonfTipliFull Γ Δ Ρ' S' := by
  revert h_konf
  induction h_step with
  -- ============ TAM Tamam kurallari (bolge/sahiplik/kanal sabit) ============
  | sVarOku S S' ts1 ts2 ctx x b v h_t h_if h_b h_v h_S' =>
      intro h_konf
      subst h_S'
      refine ⟨Ρ, h_konf.1, ?_, h_konf.2.2.1, h_konf.2.2.2.1, rfl,
              h_konf.2.2.2.2.2.1, h_konf.2.2.2.2.2.2.1, ?_, ?_,
              h_konf.2.2.2.2.2.2.2.2.2.1, h_konf.2.2.2.2.2.2.2.2.2.2.1,
              h_konf.2.2.2.2.2.2.2.2.2.2.2.1, ?_,
              h_konf.2.2.2.2.2.2.2.2.2.2.2.2.2.1,
              h_konf.2.2.2.2.2.2.2.2.2.2.2.2.2.2⟩
      · -- comp 2: odakli thread sabit v'ye ilerledi
        have h_thread := h_konf.2.1
        rw [h_t] at h_thread
        refine threadTipli_degisim h_thread _ ?_
        obtain ⟨k', h_mem', _⟩ := konumGet_mem S.store ⟨b, 0⟩ v h_v
        obtain ⟨τv, h_dt, _⟩ := h_konf.1 k' v h_mem'
        exact ⟨τv, ctx.lineer, Ρ,
          ⟨HasType.t_sabit _ _ _ _ (degerTipli_ortam v τv h_dt),
           LineerTamam.l_sabit _ _ _, RegionTamam.r_sabit _ _ _⟩⟩
      · -- comp 8
        have h8 := h_konf.2.2.2.2.2.2.2.1
        rw [h_t] at h8
        exact hedefVarSahip_degisim h8 _ rfl (fun y h => nomatch h)
      · -- comp 9
        have h9 := h_konf.2.2.2.2.2.2.2.2.1
        rw [h_t] at h9
        exact hedefBolgeSahip_degisim h9 _ rfl (fun b' h => nomatch h)
      · have h13 := h_konf.2.2.2.2.2.2.2.2.2.2.2.2.1
        rw [h_t] at h13
        exact tidAyrik_degisim h13 _ rfl
  | sSeqAtla S S' ts1 ts2 ctx v b h_t h_if h_S' =>
      intro h_konf
      subst h_S'
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      refine ⟨Ρ, h_konf.1, ?_, h_konf.2.2.1, h_konf.2.2.2.1, rfl,
              h_konf.2.2.2.2.2.1, h_konf.2.2.2.2.2.2.1, ?_, ?_,
              h_konf.2.2.2.2.2.2.2.2.2.1, h_konf.2.2.2.2.2.2.2.2.2.2.1,
              h_konf.2.2.2.2.2.2.2.2.2.2.2.1, ?_,
              h_konf.2.2.2.2.2.2.2.2.2.2.2.2.2.1,
              h_konf.2.2.2.2.2.2.2.2.2.2.2.2.2.2⟩
      · -- comp 2: seq (sabit v) b → b (typed_seq_atla)
        have h_thread := h_konf.2.1
        obtain ⟨τ0, Λ0, Ρ0, h_ty⟩ := h_thread ctx h_ctx_in
        rw [h_if] at h_ty
        have h_thread' := h_thread
        rw [h_t] at h_thread'
        exact threadTipli_degisim h_thread' _ ⟨τ0, Λ0, Ρ0, typed_seq_atla h_ty⟩
      · -- comp 8: b'nin hedefleri seq_sag ile zaten kapsaniyordu
        have h8 := h_konf.2.2.2.2.2.2.2.1
        rw [h_t] at h8
        exact hedefVarSahip_degisim h8 _ rfl
          (fun y h => by rw [h_if]; exact HedefVar.seq_sag _ _ y h)
      · -- comp 9
        have h9 := h_konf.2.2.2.2.2.2.2.2.1
        rw [h_t] at h9
        exact hedefBolgeSahip_degisim h9 _ rfl
          (fun b' h => by rw [h_if]; exact HedefBolge.seq_sag _ _ b' h)
      · have h13 := h_konf.2.2.2.2.2.2.2.2.2.2.2.2.1
        rw [h_t] at h13
        exact tidAyrik_degisim h13 _ rfl
  | sGuvensizAtla S S' ts1 ts2 ctx v h_t h_if h_S' =>
      intro h_konf
      subst h_S'
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      refine ⟨Ρ, h_konf.1, ?_, h_konf.2.2.1, h_konf.2.2.2.1, rfl,
              h_konf.2.2.2.2.2.1, h_konf.2.2.2.2.2.2.1, ?_, ?_,
              h_konf.2.2.2.2.2.2.2.2.2.1, h_konf.2.2.2.2.2.2.2.2.2.2.1,
              h_konf.2.2.2.2.2.2.2.2.2.2.2.1, ?_,
              h_konf.2.2.2.2.2.2.2.2.2.2.2.2.2.1,
              h_konf.2.2.2.2.2.2.2.2.2.2.2.2.2.2⟩
      · have h_thread := h_konf.2.1
        obtain ⟨τ0, Λ0, Ρ0, h_ty⟩ := h_thread ctx h_ctx_in
        rw [h_if] at h_ty
        have h_thread' := h_thread
        rw [h_t] at h_thread'
        exact threadTipli_degisim h_thread' _ (typed_guvensiz_ic h_ty)
      · have h8 := h_konf.2.2.2.2.2.2.2.1
        rw [h_t] at h8
        exact hedefVarSahip_degisim h8 _ rfl (fun y h => nomatch h)
      · have h9 := h_konf.2.2.2.2.2.2.2.2.1
        rw [h_t] at h9
        exact hedefBolgeSahip_degisim h9 _ rfl (fun b' h => nomatch h)
      · have h13 := h_konf.2.2.2.2.2.2.2.2.2.2.2.2.1
        rw [h_t] at h13
        exact tidAyrik_degisim h13 _ rfl
  | sLinKullanTamam S S' ts1 ts2 ctx x h_t h_if h_aktif h_S' =>
      intro h_konf
      subst h_S'
      refine ⟨Ρ, h_konf.1, ?_, h_konf.2.2.1, h_konf.2.2.2.1, rfl,
              h_konf.2.2.2.2.2.1, h_konf.2.2.2.2.2.2.1, ?_, ?_,
              h_konf.2.2.2.2.2.2.2.2.2.1, h_konf.2.2.2.2.2.2.2.2.2.2.1,
              h_konf.2.2.2.2.2.2.2.2.2.2.2.1, ?_,
              h_konf.2.2.2.2.2.2.2.2.2.2.2.2.2.1,
              h_konf.2.2.2.2.2.2.2.2.2.2.2.2.2.2⟩
      · have h_thread := h_konf.2.1
        rw [h_t] at h_thread
        exact threadTipli_degisim h_thread _
          ⟨Tip.bos, lineerOrtamUpdate ctx.lineer x Lineerlik.tuketildi, Ρ,
           ⟨HasType.t_sabit _ _ _ _ DegerTipli.dt_birim,
            LineerTamam.l_sabit _ _ _, RegionTamam.r_sabit _ _ _⟩⟩
      · have h8 := h_konf.2.2.2.2.2.2.2.1
        rw [h_t] at h8
        exact hedefVarSahip_degisim h8 _ rfl (fun y h => nomatch h)
      · have h9 := h_konf.2.2.2.2.2.2.2.2.1
        rw [h_t] at h9
        exact hedefBolgeSahip_degisim h9 _ rfl (fun b' h => nomatch h)
      · have h13 := h_konf.2.2.2.2.2.2.2.2.2.2.2.2.1
        rw [h_t] at h13
        exact tidAyrik_degisim h13 _ rfl
  | sLinImhaTamam S S' ts1 ts2 ctx x h_t h_if h_aktif h_S' =>
      intro h_konf
      subst h_S'
      refine ⟨Ρ, h_konf.1, ?_, h_konf.2.2.1, h_konf.2.2.2.1, rfl,
              h_konf.2.2.2.2.2.1, h_konf.2.2.2.2.2.2.1, ?_, ?_,
              h_konf.2.2.2.2.2.2.2.2.2.1, h_konf.2.2.2.2.2.2.2.2.2.2.1,
              h_konf.2.2.2.2.2.2.2.2.2.2.2.1, ?_,
              h_konf.2.2.2.2.2.2.2.2.2.2.2.2.2.1,
              h_konf.2.2.2.2.2.2.2.2.2.2.2.2.2.2⟩
      · have h_thread := h_konf.2.1
        rw [h_t] at h_thread
        exact threadTipli_degisim h_thread _
          ⟨Tip.bos, lineerOrtamUpdate ctx.lineer x Lineerlik.tuketildi, Ρ,
           ⟨HasType.t_sabit _ _ _ _ DegerTipli.dt_birim,
            LineerTamam.l_sabit _ _ _, RegionTamam.r_sabit _ _ _⟩⟩
      · have h8 := h_konf.2.2.2.2.2.2.2.1
        rw [h_t] at h8
        exact hedefVarSahip_degisim h8 _ rfl (fun y h => nomatch h)
      · have h9 := h_konf.2.2.2.2.2.2.2.2.1
        rw [h_t] at h9
        exact hedefBolgeSahip_degisim h9 _ rfl (fun b' h => nomatch h)
      · have h13 := h_konf.2.2.2.2.2.2.2.2.2.2.2.2.1
        rw [h_t] at h13
        exact tidAyrik_degisim h13 _ rfl
  -- ============ Tamam kurallari (devam) — TUMU TAM ============
  | sAtamaTamam S S' ts1 ts2 ctx x v b h_t h_if h_b h_owner h_S' =>
      intro h_konf
      subst h_S'
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      -- yazilan degerin tipi: t_atama + t_sabit tersine-cevirme
      obtain ⟨τ0, Λ0, Ρ0, h_ty⟩ := h_konf.2.1 ctx h_ctx_in
      rw [h_if] at h_ty
      have h_inv : ∃ τx, tipOrtamGet Γ x = some τx
          ∧ DegerTipli Γ ([] : BolgeOrtam) v τx := by
        match h_ty.hasType with
        | HasType.t_atama _ _ _ _ τx h_gx hte =>
          match hte with
          | HasType.t_sabit _ _ _ _ h_dt =>
            exact ⟨τx, h_gx, h_dt⟩
      obtain ⟨τx, h_gx, h_dtv⟩ := h_inv
      have h_beq := h_konf.2.2.2.2.2.1
      refine ⟨Ρ, ?_, ?_, h_konf.2.2.1, h_konf.2.2.2.1, rfl,
              h_konf.2.2.2.2.2.1, h_konf.2.2.2.2.2.2.1, ?_, ?_, ?_,
              h_konf.2.2.2.2.2.2.2.2.2.2.1,
              h_konf.2.2.2.2.2.2.2.2.2.2.2.1, ?_,
              h_konf.2.2.2.2.2.2.2.2.2.2.2.2.2.1,
              h_konf.2.2.2.2.2.2.2.2.2.2.2.2.2.2⟩
      · -- comp 1: yeni store girisi tipli + kayitli
        intro kk ww h_kv
        rcases List.mem_cons.mp h_kv with h_head | h_tail
        · obtain ⟨h_kk, h_ww⟩ := Prod.mk.injEq .. ▸ h_head
          refine ⟨τx, ?_, ⟨x, b, ?_, ?_⟩⟩
          · rw [h_ww]
            exact degerTipli_ortam v τx h_dtv
          · rw [← h_beq]
            exact h_b
          · rw [h_kk]
        · exact h_konf.1 kk ww h_tail
      · -- comp 2: odakli thread sabit birim'e ilerledi
        have h_thread := h_konf.2.1
        rw [h_t] at h_thread
        exact threadTipli_degisim h_thread _
          ⟨Tip.bos, ctx.lineer, Ρ,
           ⟨HasType.t_sabit _ _ _ _ DegerTipli.dt_birim,
            LineerTamam.l_sabit _ _ _, RegionTamam.r_sabit _ _ _⟩⟩
      · -- comp 8
        have h8 := h_konf.2.2.2.2.2.2.2.1
        rw [h_t] at h8
        exact hedefVarSahip_degisim h8 _ rfl (fun y h => nomatch h)
      · -- comp 9
        have h9 := h_konf.2.2.2.2.2.2.2.2.1
        rw [h_t] at h9
        exact hedefBolgeSahip_degisim h9 _ rfl (fun b' h => nomatch h)
      · -- comp 10: store-golgeleme — BolgeAyrik ile
        intro z τz h_gz
        obtain ⟨bz, vz, h_bz, h_kz, h_dtz⟩ :=
          h_konf.2.2.2.2.2.2.2.2.2.1 z τz h_gz
        by_cases h_eqk : b.id = bz.id
        · -- yeni giris golgeledi: BolgeAyrik → z = x → ayni tip
          have h_zx : z = x := by
            have h_ba := h_konf.2.2.2.2.2.2.2.2.2.2.2.1
            exact h_ba z x bz b h_bz h_b h_eqk.symm
          subst h_zx
          have h_tau : τz = τx :=
            Option.some.inj (h_gz.symm.trans h_gx)
          refine ⟨bz, v, h_bz, ?_, ?_⟩
          · show konumGet ((⟨b, 0⟩, v) :: S.store) ⟨bz, 0⟩ = some v
            rw [konumGet, if_pos (⟨h_eqk, rfl⟩ : _ ∧ _)]
          · rw [h_tau]
            exact degerTipli_ortam v τx h_dtv
        · refine ⟨bz, vz, h_bz, ?_, h_dtz⟩
          show konumGet ((⟨b, 0⟩, v) :: S.store) ⟨bz, 0⟩ = some vz
          rw [konumGet, if_neg (fun hc => h_eqk hc.1)]
          exact h_kz
      · -- comp 13
        have h13 := h_konf.2.2.2.2.2.2.2.2.2.2.2.2.1
        rw [h_t] at h13
        exact tidAyrik_degisim h13 _ rfl
  | cGorevBaslatTamam S S' ts1 ts2 ctx tYeni yd kod h_t h_if h_fresh h_sahipler h_S' =>
      intro h_konf
      subst h_S'
      obtain ⟨h_sigma, h_thread, h_sahip, h_kanal, h_fault, h_beq, h_fkat,
              h_hvar, h_hbolge, h_bagli, h_transit, h_bayrik, h_tayrik,
              h_kap, h_kayrik⟩ := h_konf
      subst h_beq
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      -- odakli tip inversion: kod tipli + yakalama yazilabilir + Yol-B
      obtain ⟨τ0, Λ0, Ρ0, h_ty⟩ := h_thread ctx h_ctx_in
      rw [h_if] at h_ty
      obtain ⟨ht0, hl0, hr0⟩ := h_ty
      have h_inv : (∃ τd, HasType Γ Δ kod τd)
          ∧ (∃ Λkod, LineerTamam Γ
               (yd.map (fun v => (v, Lineerlik.aktif))) kod Λkod)
          ∧ (∀ v ∈ yd, ∀ b : Bolge, bolgeOrtamGet S.bolge v = some b →
               kategoriYazilabilir b.kategori = true)
          ∧ (∀ y : VarId, ¬ HedefVar kod y)
          ∧ (∀ b : Bolge, ¬ HedefBolge kod b)
          ∧ (∃ Ρkod, RegionTamam Γ S.bolge kod Ρkod) := by
        match ht0, hl0, hr0 with
        | HasType.t_gorev_baslat _ _ _ _ τd htk,
          LineerTamam.l_gorev_baslat _ _ Λkod _ _ _ hlk,
          RegionTamam.r_gorev_baslat _ _ _ Ρkod _ _ _
            h_cap h_khv h_khb hrk _ =>
          exact ⟨⟨τd, htk⟩, ⟨Λkod, hlk⟩, h_cap, h_khv, h_khb, ⟨Ρkod, hrk⟩⟩
      obtain ⟨⟨τd, htk⟩, ⟨Λkod, hlk⟩, h_cap, h_khv, h_khb, ⟨Ρkod, hrk⟩⟩ := h_inv
      -- yeni bolge ortami + tid-ayriklik on-bilgileri
      have h_tneq : ∀ c, (c ∈ ts1 ∨ c ∈ ts2) → c.tid ≠ ctx.tid := by
        intro c hc
        have h13 := h_tayrik
        rw [h_t] at h13
        exact tidAyrik_odakdisi h13 hc
      have h_cin : ∀ c, (c ∈ ts1 ∨ c ∈ ts2) → c ∈ S.thread := by
        intro c hc
        rw [h_t]
        rcases hc with h1 | h2
        · exact List.mem_append.mpr (Or.inl h1)
        · exact List.mem_append.mpr (Or.inr (List.Mem.tail _ h2))
      -- odaksiz thread'in hedefleri yakalanmis OLAMAZ (sahiplik catismasi)
      have h_hv_kosul : ∀ c, c ∈ S.thread → c.tid ≠ ctx.tid →
          ∀ y, HedefVar c.ifade y →
          bolgeOrtamGet (bolgeOrtamSahipAta S.bolge yd tYeni) y
            = bolgeOrtamGet S.bolge y := by
        intro c hc h_ne y hy
        by_cases h_in : y ∈ yd
        · cases h_lk : bolgeOrtamGet S.bolge y with
          | none => exact sahipAta_get_none S.bolge yd tYeni y h_lk
          | some by0 =>
              exfalso
              have h_yz := h_cap y h_in by0 h_lk
              have h_own := h_hvar c hc y hy by0 h_lk h_yz
              have h_f := h_sahipler by0 (List.mem_filterMap.mpr ⟨y, h_in, h_lk⟩)
              rw [h_own] at h_f
              have h_t2 := Option.some.inj h_f
              injection h_t2 with h_t3
              exact h_ne h_t3
        · exact sahipAta_get_notin S.bolge yd tYeni y h_in
      have h_hb_kosul : ∀ c, c ∈ S.thread → c.tid ≠ ctx.tid →
          ∀ x bb, HedefBolge c.ifade bb →
          kategoriYazilabilir bb.kategori = true →
          bolgeOrtamGet S.bolge x = some bb →
          bolgeOrtamGet (bolgeOrtamSahipAta S.bolge yd tYeni) x = some bb := by
        intro c hc h_ne x bb hb h_yz h_lk
        by_cases h_in : x ∈ yd
        · exfalso
          have h_own := h_hbolge c hc bb hb ⟨x, h_lk⟩ h_yz
          have h_f := h_sahipler bb (List.mem_filterMap.mpr ⟨x, h_in, h_lk⟩)
          rw [h_own] at h_f
          have h_t2 := Option.some.inj h_f
          injection h_t2 with h_t3
          exact h_ne h_t3
        · rw [sahipAta_get_notin S.bolge yd tYeni x h_in]
          exact h_lk
      -- ters-analiz id-formu (comp-1/3/10/12 icin)
      have h_inv_id : ∀ x b,
          bolgeOrtamGet (bolgeOrtamSahipAta S.bolge yd tYeni) x = some b →
          ∃ b0, bolgeOrtamGet S.bolge x = some b0 ∧ b0.id = b.id := by
        intro x b h
        rcases sahipAta_get_inv' S.bolge yd tYeni x b h with
            ⟨b0, h0, hr, _⟩ | he
        · exact ⟨b0, h0, by simp [hr, bolgeKategoriDegistir]⟩
        · exact ⟨b, he, rfl⟩
      -- transfer kumesiyle id-ayriklik (eski-sahipli bb'ler icin)
      have h_yok_gen : ∀ bb : Bolge,
          sahiplikGet S.sahiplik bb ≠ some (Sahip.thread ctx.tid) →
          ∀ b'' ∈ bolgeleriTopla S.bolge yd, b''.id ≠ bb.id := by
        intro bb h_ne b'' h_in h_id
        have h_f := h_sahipler b'' h_in
        rw [sahiplikGet_id_esit S.sahiplik b'' bb h_id] at h_f
        exact h_ne h_f
      refine ⟨bolgeOrtamSahipAta S.bolge yd tYeni,
              ?_, ?_, ?_, ?_, rfl, rfl, ?_, ?_, ?_, ?_, ?_, ?_, ?_,
              h_kap, h_kayrik⟩
      · -- comp 1: SigmaTipli (store sabit; kayit id-tasima)
        intro k v h_kv
        obtain ⟨τv, h_dt, x, b', h_x, h_id⟩ := h_sigma k v h_kv
        obtain ⟨b'', h_x', h_id'⟩ :=
          sahipAta_id_koruma S.bolge yd tYeni x b' h_x
        exact ⟨τv, degerTipli_ortam v τv h_dt, x, b'', h_x', h_id'.trans h_id⟩
      · -- comp 2: ThreadTipliFull (odaksiz: transport; odak: gorevVal;
        --          cocuk: Yol-B hedefsiz transport)
        intro c h_mem
        rcases List.mem_append.mp h_mem with h_main | h_yeni
        · rcases List.mem_append.mp h_main with h1 | h2
          · obtain ⟨τc, Λc, Ρc, h_tyc⟩ := h_thread c (h_cin c (Or.inl h1))
            obtain ⟨Ρc', h_rc', _⟩ := regionTamam_transport h_tyc.regionOK
              (bolgeOrtamSahipAta S.bolge yd tYeni)
              (h_hv_kosul c (h_cin c (Or.inl h1)) (h_tneq c (Or.inl h1)))
              (h_hb_kosul c (h_cin c (Or.inl h1)) (h_tneq c (Or.inl h1)))
            exact ⟨τc, Λc, Ρc', ⟨h_tyc.hasType, h_tyc.lineerOK, h_rc'⟩⟩
          · rcases List.mem_cons.mp h2 with he | h3
            · subst he
              exact ⟨Tip.gorev τd, lineerTuketListe ctx.lineer yd,
                bolgeOrtamSahipAta S.bolge yd tYeni,
                ⟨HasType.t_sabit _ _ _ _ (DegerTipli.dt_gorev tYeni τd),
                 LineerTamam.l_sabit _ _ _, RegionTamam.r_sabit _ _ _⟩⟩
            · obtain ⟨τc, Λc, Ρc, h_tyc⟩ := h_thread c (h_cin c (Or.inr h3))
              obtain ⟨Ρc', h_rc', _⟩ := regionTamam_transport h_tyc.regionOK
                (bolgeOrtamSahipAta S.bolge yd tYeni)
                (h_hv_kosul c (h_cin c (Or.inr h3)) (h_tneq c (Or.inr h3)))
                (h_hb_kosul c (h_cin c (Or.inr h3)) (h_tneq c (Or.inr h3)))
              exact ⟨τc, Λc, Ρc', ⟨h_tyc.hasType, h_tyc.lineerOK, h_rc'⟩⟩
        · rcases List.mem_cons.mp h_yeni with he | h3
          · subst he
            obtain ⟨Ρkod', h_rkod', _⟩ := regionTamam_transport hrk
              (bolgeOrtamSahipAta S.bolge yd tYeni)
              (fun y hy => absurd hy (h_khv y))
              (fun _ bb hb _ _ => absurd hb (h_khb bb))
            exact ⟨τd, Λkod, Ρkod', ⟨htk, hlk, h_rkod'⟩⟩
          · cases h3
      · -- comp 3: SahiplikTutarli (id-genel)
        intro bb sah h_lk
        rcases sahiplikSetMany_lookup_inv _ _ _ bb sah h_lk with
            ⟨⟨b'', h_in'', h_idb⟩, _⟩ | h_eski
        · rcases List.mem_filterMap.mp h_in'' with ⟨v0, h_v0, h_v0lk⟩
          obtain ⟨b''', h_x', h_id'⟩ :=
            sahipAta_id_koruma S.bolge yd tYeni v0 b'' h_v0lk
          exact ⟨v0, b''', h_x', h_id'.trans h_idb⟩
        · obtain ⟨x, b', h_x, h_id⟩ := h_sahip bb sah h_eski
          obtain ⟨b'', h_x', h_id'⟩ :=
            sahipAta_id_koruma S.bolge yd tYeni x b' h_x
          exact ⟨x, b'', h_x', h_id'.trans h_id⟩
      · -- comp 4: KanalTutarli (kanal sabit; ortam-swap)
        intro kd h_kd w h_w
        exact degerTipli_ortam w _ (h_kanal kd h_kd w h_w)
      · -- comp 7: FrozenKategoriTutarli
        intro x bb h_bb
        rcases sahipAta_get_inv' S.bolge yd tYeni x bb h_bb with
            ⟨b0, h_b0, h_recat, h_xin⟩ | h_eski
        · constructor
          · intro h_fr
            exfalso
            have h_lk2 : sahiplikGet (sahiplikSetMany S.sahiplik
                (bolgeleriTopla S.bolge yd) (Sahip.thread tYeni)) bb
                = some Sahip.donmus := h_fr
            rcases sahiplikSetMany_analiz (bolgeleriTopla S.bolge yd)
                S.sahiplik (Sahip.thread tYeni) bb with
                ⟨h_get, _⟩ | ⟨_, h_yok⟩
            · rw [h_get] at h_lk2; cases h_lk2
            · exact h_yok b0
                (List.mem_filterMap.mpr ⟨x, h_xin, h_b0⟩)
                (by simp [h_recat, bolgeKategoriDegistir])
          · intro h_kat
            rw [h_recat] at h_kat
            simp [bolgeKategoriDegistir] at h_kat
        · have h_yok : ∀ b'' ∈ bolgeleriTopla S.bolge yd, b''.id ≠ bb.id := by
            intro b'' h_in h_id
            rcases List.mem_filterMap.mp h_in with ⟨v0, h_v0, h_v0lk⟩
            have h_vx : v0 = x := h_bayrik v0 x b'' bb h_v0lk h_eski h_id
            subst h_vx
            have h_yz := h_cap v0 h_v0 bb h_eski
            have h_rec := sahipAta_get_in' S.bolge yd tYeni v0 bb h_v0 h_eski
            rw [h_bb] at h_rec
            have h_bbk := Option.some.inj h_rec
            rw [h_bbk] at h_yz
            simp [bolgeKategoriDegistir, kategoriYazilabilir] at h_yz
          have h_pres : sahiplikGet (sahiplikSetMany S.sahiplik
              (bolgeleriTopla S.bolge yd) (Sahip.thread tYeni)) bb
              = sahiplikGet S.sahiplik bb :=
            sahiplikSetMany_ne _ _ _ _ h_yok
          have h_iff := h_fkat x bb h_eski
          constructor
          · intro h_fr
            refine h_iff.mp ?_
            show sahiplikGet S.sahiplik bb = some Sahip.donmus
            rw [← h_pres]
            exact h_fr
          · intro h_kat
            show sahiplikGet (sahiplikSetMany S.sahiplik
                (bolgeleriTopla S.bolge yd) (Sahip.thread tYeni)) bb
                = some Sahip.donmus
            rw [h_pres]
            exact h_iff.mpr h_kat
      · -- comp 8: HedefVarSahipligi
        intro c h_mem y hy bb h_bb h_yz
        have h_unf : c ∈ S.thread → c.tid ≠ ctx.tid →
            sahiplikGet (sahiplikSetMany S.sahiplik
              (bolgeleriTopla S.bolge yd) (Sahip.thread tYeni)) bb
              = some (Sahip.thread c.tid) := by
          intro hc h_ne
          rcases sahipAta_get_inv' S.bolge yd tYeni y bb h_bb with
              ⟨b0, _, h_recat, _⟩ | h_eski
          · rw [h_recat] at h_yz
            simp [bolgeKategoriDegistir, kategoriYazilabilir] at h_yz
          · have h_own := h_hvar c hc y hy bb h_eski h_yz
            have h_yok := h_yok_gen bb (by
              rw [h_own]
              intro h_f
              have h_t2 := Option.some.inj h_f
              injection h_t2 with h_t3
              exact h_ne h_t3)
            rw [sahiplikSetMany_ne _ _ _ _ h_yok]
            exact h_own
        rcases List.mem_append.mp h_mem with h_main | h_yeni
        · rcases List.mem_append.mp h_main with h1 | h2
          · exact h_unf (h_cin c (Or.inl h1)) (h_tneq c (Or.inl h1))
          · rcases List.mem_cons.mp h2 with he | h3
            · subst he; exact absurd hy (fun h => nomatch h)
            · exact h_unf (h_cin c (Or.inr h3)) (h_tneq c (Or.inr h3))
        · rcases List.mem_cons.mp h_yeni with he | h3
          · subst he; exact absurd hy (h_khv y)
          · cases h3
      · -- comp 9: HedefBolgeSahipligi
        intro c h_mem bb hb h_kayit h_yz
        obtain ⟨x, h_x⟩ := h_kayit
        have h_unf : c ∈ S.thread → c.tid ≠ ctx.tid →
            sahiplikGet (sahiplikSetMany S.sahiplik
              (bolgeleriTopla S.bolge yd) (Sahip.thread tYeni)) bb
              = some (Sahip.thread c.tid) := by
          intro hc h_ne
          rcases sahipAta_get_inv' S.bolge yd tYeni x bb h_x with
              ⟨b0, _, h_recat, _⟩ | h_eski
          · rw [h_recat] at h_yz
            simp [bolgeKategoriDegistir, kategoriYazilabilir] at h_yz
          · have h_own := h_hbolge c hc bb hb ⟨x, h_eski⟩ h_yz
            have h_yok := h_yok_gen bb (by
              rw [h_own]
              intro h_f
              have h_t2 := Option.some.inj h_f
              injection h_t2 with h_t3
              exact h_ne h_t3)
            rw [sahiplikSetMany_ne _ _ _ _ h_yok]
            exact h_own
        rcases List.mem_append.mp h_mem with h_main | h_yeni
        · rcases List.mem_append.mp h_main with h1 | h2
          · exact h_unf (h_cin c (Or.inl h1)) (h_tneq c (Or.inl h1))
          · rcases List.mem_cons.mp h2 with he | h3
            · subst he; exact absurd hb (fun h => nomatch h)
            · exact h_unf (h_cin c (Or.inr h3)) (h_tneq c (Or.inr h3))
        · rcases List.mem_cons.mp h_yeni with he | h3
          · subst he; exact absurd hb (h_khb bb)
          · cases h3
      · -- comp 10: DegiskenlerBagli (id-koruma + konum id-kongruansi)
        intro z τz h_gz
        obtain ⟨bz, vz, h_bz, h_kz, h_dtz⟩ := h_bagli z τz h_gz
        obtain ⟨bz', h_bz', h_idz⟩ :=
          sahipAta_id_koruma S.bolge yd tYeni z bz h_bz
        refine ⟨bz', vz, h_bz', ?_, degerTipli_ortam vz τz h_dtz⟩
        rw [konumGet_id_esit S.store ⟨bz', 0⟩ ⟨bz, 0⟩ h_idz rfl]
        exact h_kz
      · -- comp 11: KanalTransit (transit bolge thread-sahipli olamaz)
        intro kd h_kd h_ne'
        obtain ⟨bT, h_bT⟩ := h_transit kd h_kd h_ne'
        have h_yok : ∀ b'' ∈ bolgeleriTopla S.bolge yd, b''.id ≠ bT.id := by
          intro b'' h_in h_id
          have h_f := h_sahipler b'' h_in
          rw [sahiplikGet_id_esit S.sahiplik b'' bT h_id, h_bT] at h_f
          cases h_f
        refine ⟨bT, ?_⟩
        show sahiplikGet (sahiplikSetMany S.sahiplik
            (bolgeleriTopla S.bolge yd) (Sahip.thread tYeni)) bT
            = some (Sahip.kanalSahip kd.kid)
        rw [sahiplikSetMany_ne _ _ _ _ h_yok]
        exact h_bT
      · -- comp 12: BolgeAyrik (id-koruma ile eskiye indirgeme)
        intro x1 x2 b1 b2 h_1 h_2 h_id
        obtain ⟨b01, h_01, h_i1⟩ := h_inv_id x1 b1 h_1
        obtain ⟨b02, h_02, h_i2⟩ := h_inv_id x2 b2 h_2
        exact h_bayrik x1 x2 b01 b02 h_01 h_02
          (h_i1.trans (h_id.trans h_i2.symm))
      · -- comp 13: TidAyrik (degisim + taze-tid ekleme)
        have h13S := h_tayrik
        rw [h_t] at h13S
        have h13' := tidAyrik_degisim h13S
          { ctx with ifade := .sabit (.gorevVal tYeni),
                     lineer := lineerTuketListe ctx.lineer yd } rfl
        have h_fresh' : ∀ a ∈ ts1 ++ { ctx with
            ifade := Ifade.sabit (Deger.gorevVal tYeni),
            lineer := lineerTuketListe ctx.lineer yd } :: ts2,
            a.tid ≠ (⟨tYeni, kod,
              yd.map (fun v => (v, Lineerlik.aktif))⟩ : ThreadCtx).tid := by
          intro a ha
          rcases List.mem_append.mp ha with h1 | h2
          · exact h_fresh a (by
              rw [h_t]; exact List.mem_append.mpr (Or.inl h1))
          · rcases List.mem_cons.mp h2 with he | h3
            · subst he
              exact h_fresh ctx h_ctx_in
            · exact h_fresh a (by
                rw [h_t]
                exact List.mem_append.mpr (Or.inr (List.Mem.tail _ h3)))
        exact tidAyrik_ekle h13' h_fresh'
  | cGorevBirlestirTamam S S' ts1 ts2 ctx g tHedef rb h_t h_if h_hedef h_donen h_S' =>
      intro h_konf
      subst h_S'
      obtain ⟨hctx, h_hctx_in, h_hctx_tid, vSon, h_hctx_if⟩ := h_hedef
      have h13S := h_konf.2.2.2.2.2.2.2.2.2.2.2.2.1
      refine ⟨Ρ, h_konf.1, ?_, ?_, h_konf.2.2.2.1, rfl,
              h_konf.2.2.2.2.2.1, ?_, ?_, ?_,
              h_konf.2.2.2.2.2.2.2.2.2.1, ?_,
              h_konf.2.2.2.2.2.2.2.2.2.2.2.1, ?_,
              h_konf.2.2.2.2.2.2.2.2.2.2.2.2.2.1,
              h_konf.2.2.2.2.2.2.2.2.2.2.2.2.2.2⟩
      · -- comp 2: odakli thread sabit birim'e ilerledi
        have h_thread := h_konf.2.1
        rw [h_t] at h_thread
        exact threadTipli_degisim h_thread _
          ⟨Tip.bos, ctx.lineer, Ρ,
           ⟨HasType.t_sabit _ _ _ _ DegerTipli.dt_birim,
            LineerTamam.l_sabit _ _ _, RegionTamam.r_sabit _ _ _⟩⟩
      · -- comp 3 (id-genel): yeni atamalar rb'den — eski tHedef-sahipli
        intro bb sah h_lk
        rcases sahiplikSetMany_lookup_inv rb S.sahiplik _ bb sah h_lk with
            ⟨⟨b'', h_mem'', h_idb⟩, _⟩ | h_eski
        · obtain ⟨x', b', h_x', h_id'⟩ :=
            h_konf.2.2.1 b'' (Sahip.thread tHedef) (h_donen b'' h_mem'')
          exact ⟨x', b', h_x', h_id'.trans h_idb⟩
        · exact h_konf.2.2.1 bb sah h_eski
      · -- comp 7: rb donmus icermez (eski sahipleri tHedef)
        intro xx bb h_bb
        constructor
        · intro h_fr
          have h_fr' : sahiplikGet
              (sahiplikSetMany S.sahiplik rb (Sahip.thread ctx.tid)) bb
              = some Sahip.donmus := h_fr
          rcases sahiplikSetMany_lookup_inv rb S.sahiplik _ bb _ h_fr' with
              ⟨_, h_v⟩ | h_eski
          · exact absurd h_v (by intro h; cases h)
          · exact (h_konf.2.2.2.2.2.2.1 xx bb h_bb).mp h_eski
        · intro h_kat
          have h_eski := (h_konf.2.2.2.2.2.2.1 xx bb h_bb).mpr h_kat
          have h_notin : ∀ b'' ∈ rb, b''.id ≠ bb.id := by
            intro b'' h_in h_idb
            have h_th := h_donen b'' h_in
            rw [sahiplikGet_id_esit S.sahiplik b'' bb h_idb, h_eski] at h_th
            cases h_th
          show sahiplikGet
              (sahiplikSetMany S.sahiplik rb (Sahip.thread ctx.tid)) bb
              = some Sahip.donmus
          rw [sahiplikSetMany_ne rb S.sahiplik bb _ h_notin]
          exact h_eski
      · -- comp 8: rb-hedefli odaksiz thread olamaz (TidAyrik tekillik)
        intro c h_mem y h_hv bb h_bb h_yaz
        have h_c_eski : c ∈ S.thread ∧ sahiplikGet S.sahiplik bb
            = some (Sahip.thread c.tid) := by
          rcases List.mem_append.mp h_mem with h1 | h2
          · have h_in : c ∈ S.thread := by
              rw [h_t]; exact List.mem_append.mpr (Or.inl h1)
            exact ⟨h_in, h_konf.2.2.2.2.2.2.2.1 c h_in y h_hv bb h_bb h_yaz⟩
          · rcases List.mem_cons.mp h2 with h_eq | h3
            · subst h_eq; exact absurd h_hv (fun h => nomatch h)
            · have h_in : c ∈ S.thread := by
                rw [h_t]
                exact List.mem_append.mpr (Or.inr (List.Mem.tail _ h3))
              exact ⟨h_in, h_konf.2.2.2.2.2.2.2.1 c h_in y h_hv bb h_bb h_yaz⟩
        obtain ⟨h_c_in, h_c_own⟩ := h_c_eski
        have h_notin : ∀ b'' ∈ rb, b''.id ≠ bb.id := by
          intro b'' h_in h_idb
          have h_th := h_donen b'' h_in
          rw [sahiplikGet_id_esit S.sahiplik b'' bb h_idb, h_c_own] at h_th
          have h_tid_eq : c.tid = tHedef := by
            injection Option.some.inj h_th
          have h_ce : c = hctx :=
            tidAyrik_tekil h13S h_c_in h_hctx_in
              (h_tid_eq.trans h_hctx_tid.symm)
          rw [h_ce, h_hctx_if] at h_hv
          exact (fun h => nomatch h : ¬ HedefVar (Ifade.sabit vSon) y) h_hv
        show sahiplikGet
            (sahiplikSetMany S.sahiplik rb (Sahip.thread ctx.tid)) bb
            = some (Sahip.thread c.tid)
        rw [sahiplikSetMany_ne rb S.sahiplik bb _ h_notin]
        exact h_c_own
      · -- comp 9: ayni desen (HedefBolge)
        intro c h_mem bb h_hb h_kayit h_yaz
        have h_c_eski : c ∈ S.thread ∧ sahiplikGet S.sahiplik bb
            = some (Sahip.thread c.tid) := by
          rcases List.mem_append.mp h_mem with h1 | h2
          · have h_in : c ∈ S.thread := by
              rw [h_t]; exact List.mem_append.mpr (Or.inl h1)
            exact ⟨h_in, h_konf.2.2.2.2.2.2.2.2.1 c h_in bb h_hb h_kayit h_yaz⟩
          · rcases List.mem_cons.mp h2 with h_eq | h3
            · subst h_eq; exact absurd h_hb (fun h => nomatch h)
            · have h_in : c ∈ S.thread := by
                rw [h_t]
                exact List.mem_append.mpr (Or.inr (List.Mem.tail _ h3))
              exact ⟨h_in, h_konf.2.2.2.2.2.2.2.2.1 c h_in bb h_hb h_kayit h_yaz⟩
        obtain ⟨h_c_in, h_c_own⟩ := h_c_eski
        have h_notin : ∀ b'' ∈ rb, b''.id ≠ bb.id := by
          intro b'' h_in h_idb
          have h_th := h_donen b'' h_in
          rw [sahiplikGet_id_esit S.sahiplik b'' bb h_idb, h_c_own] at h_th
          have h_tid_eq : c.tid = tHedef := by
            injection Option.some.inj h_th
          have h_ce : c = hctx :=
            tidAyrik_tekil h13S h_c_in h_hctx_in
              (h_tid_eq.trans h_hctx_tid.symm)
          rw [h_ce, h_hctx_if] at h_hb
          exact (fun h => nomatch h : ¬ HedefBolge (Ifade.sabit vSon) bb) h_hb
        show sahiplikGet
            (sahiplikSetMany S.sahiplik rb (Sahip.thread ctx.tid)) bb
            = some (Sahip.thread c.tid)
        rw [sahiplikSetMany_ne rb S.sahiplik bb _ h_notin]
        exact h_c_own
      · -- comp 11: transit bolgeler rb'de olamaz (kanalSahip ≠ thread)
        intro kd h_kd h_ne
        obtain ⟨bT, h_bT⟩ := h_konf.2.2.2.2.2.2.2.2.2.2.1 kd h_kd h_ne
        have h_notin : ∀ b'' ∈ rb, b''.id ≠ bT.id := by
          intro b'' h_in h_idb
          have h_th := h_donen b'' h_in
          rw [sahiplikGet_id_esit S.sahiplik b'' bT h_idb, h_bT] at h_th
          cases h_th
        refine ⟨bT, ?_⟩
        show sahiplikGet
            (sahiplikSetMany S.sahiplik rb (Sahip.thread ctx.tid)) bT
            = some (Sahip.kanalSahip kd.kid)
        rw [sahiplikSetMany_ne rb S.sahiplik bT _ h_notin]
        exact h_bT
      · -- comp 13
        have h13 := h13S
        rw [h_t] at h13
        exact tidAyrik_degisim h13 _ rfl
  | cKanalGonderTamam S S' ts1 ts2 ctx k vId b v h_t h_if h_b h_v h_owner h_bos h_S' =>
      intro h_konf
      subst h_S'
      obtain ⟨h_sigma, h_thread, h_sahip, h_kanal, h_fault, h_beq, h_fkat,
              h_hvar, h_hbolge, h_bagli, h_transit, h_bayrik, h_tayrik,
              h_kap, h_kayrik⟩ := h_konf
      subst h_beq
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      -- odakli tip inversion: vId tipi Δ k + bolgesi yazilabilir
      obtain ⟨τ0, Λ0, Ρ0, h_ty⟩ := h_thread ctx h_ctx_in
      rw [h_if] at h_ty
      obtain ⟨ht0, _, hr0⟩ := h_ty
      have h_inv : tipOrtamGet Γ vId = some (Δ k)
          ∧ kategoriYazilabilir b.kategori = true := by
        match ht0, hr0 with
        | HasType.t_kanal_gonder _ _ _ _ h_gv,
          RegionTamam.r_kanal_gonder _ _ _ _ _ bR h_bR h_yzR _ =>
          have h_eq : bR = b := Option.some.inj (h_bR.symm.trans h_b)
          exact ⟨h_gv, h_eq ▸ h_yzR⟩
      obtain ⟨h_gv, h_yzb⟩ := h_inv
      -- gonderilen degerin tipi (DegiskenlerBagli zinciri)
      obtain ⟨bv, vv, h_bv, h_kv2, h_dtv⟩ := h_bagli vId (Δ k) h_gv
      have h_bvb : bv = b := Option.some.inj (h_bv.symm.trans h_b)
      rw [h_bvb] at h_kv2
      have h_vvv : vv = v := Option.some.inj (h_kv2.symm.trans h_v)
      rw [h_vvv] at h_dtv
      -- tid-ayriklik + uyelik on-bilgileri
      have h_tneq : ∀ c, (c ∈ ts1 ∨ c ∈ ts2) → c.tid ≠ ctx.tid := by
        intro c hc
        have h13 := h_tayrik
        rw [h_t] at h13
        exact tidAyrik_odakdisi h13 hc
      have h_cin : ∀ c, (c ∈ ts1 ∨ c ∈ ts2) → c ∈ S.thread := by
        intro c hc
        rw [h_t]
        rcases hc with h1 | h2
        · exact List.mem_append.mpr (Or.inl h1)
        · exact List.mem_append.mpr (Or.inr (List.Mem.tail _ h2))
      -- guncelleme lookup analizleri
      have h_upd_inv_id : ∀ x bb,
          bolgeOrtamGet (bolgeOrtamUpdate S.bolge vId
            (bolgeKategoriDegistir b (BolgeKategorisi.kanalRho k))) x
            = some bb →
          ∃ b0, bolgeOrtamGet S.bolge x = some b0 ∧ b0.id = bb.id := by
        intro x bb h
        rw [bolgeOrtamUpdate_get] at h
        by_cases hv : vId = x
        · rw [if_pos hv] at h
          rw [← hv]
          exact ⟨b, h_b, by rw [← Option.some.inj h]; rfl⟩
        · rw [if_neg hv] at h
          exact ⟨bb, h, rfl⟩
      have h_id_koruma_upd : ∀ x b0, bolgeOrtamGet S.bolge x = some b0 →
          ∃ b', bolgeOrtamGet (bolgeOrtamUpdate S.bolge vId
            (bolgeKategoriDegistir b (BolgeKategorisi.kanalRho k))) x
            = some b' ∧ b'.id = b0.id := by
        intro x b0 h
        rw [bolgeOrtamUpdate_get]
        by_cases hv : vId = x
        · refine ⟨bolgeKategoriDegistir b (BolgeKategorisi.kanalRho k),
            by rw [if_pos hv], ?_⟩
          rw [hv] at h_b
          have h_b0 : b = b0 := Option.some.inj (h_b.symm.trans h)
          have h_rfl : (bolgeKategoriDegistir b
              (BolgeKategorisi.kanalRho k)).id = b.id := rfl
          exact h_rfl.trans (congrArg Bolge.id h_b0)
        · exact ⟨b0, by rw [if_neg hv]; exact h, rfl⟩
      -- bos-kanal analizi (kapasite-1 taniklari)
      have h_k_bos : ∀ kd0 ∈ S.kanal, kd0.kid = k →
          kd0.gonderKuyrugu = [] := by
        intro kd0 h0 hk
        unfold kanalIlk at h_bos
        cases h_f : S.kanal.find? (fun kd => kd.kid = k) with
        | none =>
            have h_yok := List.find?_eq_none.mp h_f kd0 h0
            simp [hk] at h_yok
        | some kdF =>
            rw [h_f] at h_bos
            have h_bos2 : kdF.gonderKuyrugu.head? = none := h_bos
            have h_kdF_in := List.mem_of_find?_eq_some h_f
            have h_kdF_kid : kdF.kid = k := by
              have := List.find?_some h_f
              simpa using this
            have h_eq := h_kayrik kd0 h0 kdF h_kdF_in
              (hk.trans h_kdF_kid.symm)
            rw [h_eq]
            cases hq : kdF.gonderKuyrugu with
            | nil => rfl
            | cons a r => rw [hq] at h_bos2; cases h_bos2
      -- transport kosullari (odaksiz thread'ler)
      have h_hv_kosul : ∀ c, c ∈ S.thread → c.tid ≠ ctx.tid →
          ∀ y, HedefVar c.ifade y →
          bolgeOrtamGet (bolgeOrtamUpdate S.bolge vId
            (bolgeKategoriDegistir b (BolgeKategorisi.kanalRho k))) y
            = bolgeOrtamGet S.bolge y := by
        intro c hc h_ne y hy
        rw [bolgeOrtamUpdate_get]
        by_cases hv : vId = y
        · exfalso
          rw [hv] at h_b
          have h_own := h_hvar c hc y hy b h_b h_yzb
          rw [h_own] at h_owner
          have h_t2 := Option.some.inj h_owner
          injection h_t2 with h_t3
          exact h_ne h_t3
        · rw [if_neg hv]
      have h_hb_kosul : ∀ c, c ∈ S.thread → c.tid ≠ ctx.tid →
          ∀ x bb, HedefBolge c.ifade bb →
          kategoriYazilabilir bb.kategori = true →
          bolgeOrtamGet S.bolge x = some bb →
          bolgeOrtamGet (bolgeOrtamUpdate S.bolge vId
            (bolgeKategoriDegistir b (BolgeKategorisi.kanalRho k))) x
            = some bb := by
        intro c hc h_ne x bb hb h_yz h_lk
        rw [bolgeOrtamUpdate_get]
        by_cases hv : vId = x
        · exfalso
          rw [hv] at h_b
          have h_bbb : bb = b := Option.some.inj (h_lk.symm.trans h_b)
          subst h_bbb
          have h_own := h_hbolge c hc bb hb ⟨x, h_lk⟩ h_yz
          rw [h_own] at h_owner
          have h_t2 := Option.some.inj h_owner
          injection h_t2 with h_t3
          exact h_ne h_t3
        · rw [if_neg hv]
          exact h_lk
      refine ⟨bolgeOrtamUpdate S.bolge vId
              (bolgeKategoriDegistir b (BolgeKategorisi.kanalRho k)),
              ?_, ?_, ?_, ?_, rfl, rfl, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_⟩
      · -- comp 1: SigmaTipli
        intro kk ww h_kw
        obtain ⟨τw, h_dt, x, b', h_x, h_id⟩ := h_sigma kk ww h_kw
        obtain ⟨b'', h_x', h_id'⟩ := h_id_koruma_upd x b' h_x
        exact ⟨τw, degerTipli_ortam ww τw h_dt, x, b'', h_x', h_id'.trans h_id⟩
      · -- comp 2: ThreadTipliFull
        intro c h_mem
        rcases List.mem_append.mp h_mem with h1 | h2
        · obtain ⟨τc, Λc, Ρc, h_tyc⟩ := h_thread c (h_cin c (Or.inl h1))
          obtain ⟨Ρc', h_rc', _⟩ := regionTamam_transport h_tyc.regionOK _
            (h_hv_kosul c (h_cin c (Or.inl h1)) (h_tneq c (Or.inl h1)))
            (h_hb_kosul c (h_cin c (Or.inl h1)) (h_tneq c (Or.inl h1)))
          exact ⟨τc, Λc, Ρc', ⟨h_tyc.hasType, h_tyc.lineerOK, h_rc'⟩⟩
        · rcases List.mem_cons.mp h2 with he | h3
          · subst he
            exact ⟨Tip.bos, lineerTuket ctx.lineer vId,
              bolgeOrtamUpdate S.bolge vId
                (bolgeKategoriDegistir b (BolgeKategorisi.kanalRho k)),
              ⟨HasType.t_sabit _ _ _ _ DegerTipli.dt_birim,
               LineerTamam.l_sabit _ _ _, RegionTamam.r_sabit _ _ _⟩⟩
          · obtain ⟨τc, Λc, Ρc, h_tyc⟩ := h_thread c (h_cin c (Or.inr h3))
            obtain ⟨Ρc', h_rc', _⟩ := regionTamam_transport h_tyc.regionOK _
              (h_hv_kosul c (h_cin c (Or.inr h3)) (h_tneq c (Or.inr h3)))
              (h_hb_kosul c (h_cin c (Or.inr h3)) (h_tneq c (Or.inr h3)))
            exact ⟨τc, Λc, Ρc', ⟨h_tyc.hasType, h_tyc.lineerOK, h_rc'⟩⟩
      · -- comp 3: SahiplikTutarli
        intro bb sah h_lk
        by_cases h_eq : bb.id = b.id
        · refine ⟨vId, bolgeKategoriDegistir b (BolgeKategorisi.kanalRho k),
            ?_, h_eq.symm⟩
          rw [bolgeOrtamUpdate_get, if_pos rfl]
        · rw [sahiplikSet_ne S.sahiplik b bb _ h_eq] at h_lk
          obtain ⟨x, b', h_x, h_id⟩ := h_sahip bb sah h_lk
          obtain ⟨b'', h_x', h_id'⟩ := h_id_koruma_upd x b' h_x
          exact ⟨x, b'', h_x', h_id'.trans h_id⟩
      · -- comp 4: KanalTutarli (kanalEkle analizi + gonderilen deger tipi)
        intro kd' h_kd' w h_w
        rcases kanalEkle_uye S.kanal k v kd' h_kd' with
            ⟨kd0, h0, hk0, h_img, _⟩ | ⟨h_in, _⟩ | ⟨h_img, _⟩
        · subst h_img
          rcases List.mem_append.mp h_w with h_eski | h_yeni
          · have := h_kanal kd0 h0 w h_eski
            rw [hk0] at this ⊢
            exact degerTipli_ortam w _ this
          · rcases List.mem_cons.mp h_yeni with he | h3
            · subst he
              show DegerTipli Γ _ w (Δ kd0.kid)
              rw [hk0]
              exact degerTipli_ortam w _ h_dtv
            · cases h3
        · exact degerTipli_ortam w _ (h_kanal kd' h_in w h_w)
        · subst h_img
          rcases List.mem_cons.mp h_w with he | h3
          · subst he
            exact degerTipli_ortam w _ h_dtv
          · cases h3
      · -- comp 7: FrozenKategoriTutarli
        intro x bb h_bb
        rw [bolgeOrtamUpdate_get] at h_bb
        by_cases hv : vId = x
        · rw [if_pos hv] at h_bb
          have h_bbr := Option.some.inj h_bb
          constructor
          · intro h_fr
            exfalso
            have h_fr2 : sahiplikGet (sahiplikSet S.sahiplik b
                (Sahip.kanalSahip k)) bb = some Sahip.donmus := h_fr
            rw [sahiplikSet, sahiplikGet,
                if_pos (by rw [← h_bbr]; rfl :
                  b.id = bb.id)] at h_fr2
            cases h_fr2
          · intro h_kat
            rw [← h_bbr] at h_kat
            simp [bolgeKategoriDegistir] at h_kat
        · rw [if_neg hv] at h_bb
          have h_ne : bb.id ≠ b.id := by
            intro h_id
            exact hv (h_bayrik vId x b bb h_b h_bb h_id.symm)
          have h_pres : sahiplikGet (sahiplikSet S.sahiplik b
              (Sahip.kanalSahip k)) bb = sahiplikGet S.sahiplik bb :=
            sahiplikSet_ne S.sahiplik b bb _ h_ne
          have h_iff := h_fkat x bb h_bb
          constructor
          · intro h_fr
            refine h_iff.mp ?_
            show sahiplikGet S.sahiplik bb = some Sahip.donmus
            rw [← h_pres]
            exact h_fr
          · intro h_kat
            show sahiplikGet (sahiplikSet S.sahiplik b
                (Sahip.kanalSahip k)) bb = some Sahip.donmus
            rw [h_pres]
            exact h_iff.mpr h_kat
      · -- comp 8: HedefVarSahipligi
        intro c h_mem y hy bb h_bb h_yz
        rw [bolgeOrtamUpdate_get] at h_bb
        have h_unf : c ∈ S.thread → c.tid ≠ ctx.tid →
            sahiplikGet (sahiplikSet S.sahiplik b (Sahip.kanalSahip k)) bb
              = some (Sahip.thread c.tid) := by
          intro hc h_ne
          by_cases hv : vId = y
          · rw [if_pos hv] at h_bb
            rw [← Option.some.inj h_bb] at h_yz
            simp [bolgeKategoriDegistir, kategoriYazilabilir] at h_yz
          · rw [if_neg hv] at h_bb
            have h_own := h_hvar c hc y hy bb h_bb h_yz
            have h_idne : bb.id ≠ b.id := by
              intro h_id
              exact hv (h_bayrik vId y b bb h_b h_bb h_id.symm)
            rw [sahiplikSet_ne S.sahiplik b bb _ h_idne]
            exact h_own
        rcases List.mem_append.mp h_mem with h1 | h2
        · exact h_unf (h_cin c (Or.inl h1)) (h_tneq c (Or.inl h1))
        · rcases List.mem_cons.mp h2 with he | h3
          · subst he; exact absurd hy (fun h => nomatch h)
          · exact h_unf (h_cin c (Or.inr h3)) (h_tneq c (Or.inr h3))
      · -- comp 9: HedefBolgeSahipligi
        intro c h_mem bb hb h_kayit h_yz
        obtain ⟨x, h_x⟩ := h_kayit
        rw [bolgeOrtamUpdate_get] at h_x
        have h_unf : c ∈ S.thread → c.tid ≠ ctx.tid →
            sahiplikGet (sahiplikSet S.sahiplik b (Sahip.kanalSahip k)) bb
              = some (Sahip.thread c.tid) := by
          intro hc h_ne
          by_cases hv : vId = x
          · rw [if_pos hv] at h_x
            rw [← Option.some.inj h_x] at h_yz
            simp [bolgeKategoriDegistir, kategoriYazilabilir] at h_yz
          · rw [if_neg hv] at h_x
            have h_own := h_hbolge c hc bb hb ⟨x, h_x⟩ h_yz
            have h_idne : bb.id ≠ b.id := by
              intro h_id
              exact hv (h_bayrik vId x b bb h_b h_x h_id.symm)
            rw [sahiplikSet_ne S.sahiplik b bb _ h_idne]
            exact h_own
        rcases List.mem_append.mp h_mem with h1 | h2
        · exact h_unf (h_cin c (Or.inl h1)) (h_tneq c (Or.inl h1))
        · rcases List.mem_cons.mp h2 with he | h3
          · subst he; exact absurd hb (fun h => nomatch h)
          · exact h_unf (h_cin c (Or.inr h3)) (h_tneq c (Or.inr h3))
      · -- comp 10: DegiskenlerBagli
        intro z τz h_gz
        obtain ⟨bz, vz, h_bz, h_kz, h_dtz⟩ := h_bagli z τz h_gz
        obtain ⟨bz', h_bz', h_idz⟩ := h_id_koruma_upd z bz h_bz
        refine ⟨bz', vz, h_bz', ?_, degerTipli_ortam vz τz h_dtz⟩
        rw [konumGet_id_esit S.store ⟨bz', 0⟩ ⟨bz, 0⟩ h_idz rfl]
        exact h_kz
      · -- comp 11: KanalTransit (yeni transit tanigi b; eskiler korunur)
        intro kd' h_kd' h_ne'
        rcases kanalEkle_uye S.kanal k v kd' h_kd' with
            ⟨kd0, _, hk0, h_img, _⟩ | ⟨h_in, h_kne⟩ | ⟨h_img, _⟩
        · subst h_img
          refine ⟨b, ?_⟩
          show sahiplikGet (sahiplikSet S.sahiplik b (Sahip.kanalSahip k)) b
              = some (Sahip.kanalSahip kd0.kid)
          rw [sahiplikSet_eq, hk0]
        · obtain ⟨bT, h_bT⟩ := h_transit kd' h_in h_ne'
          have h_idne : bT.id ≠ b.id := by
            intro h_id
            rw [sahiplikGet_id_esit S.sahiplik bT b h_id, h_owner] at h_bT
            cases h_bT
          refine ⟨bT, ?_⟩
          show sahiplikGet (sahiplikSet S.sahiplik b (Sahip.kanalSahip k)) bT
              = some (Sahip.kanalSahip kd'.kid)
          rw [sahiplikSet_ne S.sahiplik b bT _ h_idne]
          exact h_bT
        · subst h_img
          refine ⟨b, ?_⟩
          show sahiplikGet (sahiplikSet S.sahiplik b (Sahip.kanalSahip k)) b
              = some (Sahip.kanalSahip k)
          rw [sahiplikSet_eq]
      · -- comp 12: BolgeAyrik
        intro x1 x2 b1 b2 h_1 h_2 h_id
        obtain ⟨b01, h_01, h_i1⟩ := h_upd_inv_id x1 b1 h_1
        obtain ⟨b02, h_02, h_i2⟩ := h_upd_inv_id x2 b2 h_2
        exact h_bayrik x1 x2 b01 b02 h_01 h_02
          (h_i1.trans (h_id.trans h_i2.symm))
      · -- comp 13: TidAyrik
        have h13 := h_tayrik
        rw [h_t] at h13
        exact tidAyrik_degisim h13 _ rfl
      · -- comp 14: KanalKapasite1 (bos kanala tek ekleme)
        intro kd' h_kd'
        rcases kanalEkle_uye S.kanal k v kd' h_kd' with
            ⟨kd0, h0, hk0, h_img, _⟩ | ⟨h_in, _⟩ | ⟨h_img, _⟩
        · subst h_img
          show (kd0.gonderKuyrugu ++ [v]).length ≤ 1
          rw [h_k_bos kd0 h0 hk0]
          exact Nat.le_refl 1
        · exact h_kap kd' h_in
        · subst h_img
          exact Nat.le_refl 1
      · -- comp 15: KanalAyrik (kanalEkle kid-tekilligi korur)
        intro kd1 h1 kd2 h2 h_kid
        rcases kanalEkle_uye S.kanal k v kd1 h1 with
            ⟨kd01, h01, hk01, h_img1, h_any1⟩ | ⟨h_in1, h_kne1⟩ | ⟨h_img1, h_any1⟩
        all_goals rcases kanalEkle_uye S.kanal k v kd2 h2 with
            ⟨kd02, h02, hk02, h_img2, h_any2⟩ | ⟨h_in2, h_kne2⟩ | ⟨h_img2, h_any2⟩
        · have h_e := h_kayrik kd01 h01 kd02 h02 (hk01.trans hk02.symm)
          rw [h_img1, h_img2, h_e]
        · exfalso
          apply h_kne2
          rw [← h_kid, h_img1]
          exact hk01
        · exfalso
          rw [h_any1] at h_any2
          cases h_any2
        · exfalso
          apply h_kne1
          rw [h_kid, h_img2]
          exact hk02
        · exact h_kayrik kd1 h_in1 kd2 h_in2 h_kid
        · exfalso
          apply h_kne1
          rw [h_kid, h_img2]
        · exfalso
          rw [h_any2] at h_any1
          cases h_any1
        · exfalso
          apply h_kne2
          rw [← h_kid, h_img1]
        · rw [h_img1, h_img2]
  | cKanalAlTamam S S' ts1 ts2 ctx k v tb h_t h_if h_v h_transit h_S' =>
      intro h_konf
      subst h_S'
      -- bulunan kanal kaydi (find? analizi)
      have h_kdF : ∃ kdF ∈ S.kanal, kdF.kid = k
          ∧ kdF.gonderKuyrugu.head? = some v := by
        unfold kanalIlk at h_v
        cases h_f : S.kanal.find? (fun kd => kd.kid = k) with
        | none => rw [h_f] at h_v; cases h_v
        | some kdF =>
            rw [h_f] at h_v
            have h_q : kdF.gonderKuyrugu.head? = some v := h_v
            refine ⟨kdF, List.mem_of_find?_eq_some h_f, ?_, h_q⟩
            have h_pred := List.find?_some h_f
            simpa using h_pred
      obtain ⟨kdF, h_kdF_in, h_kdF_kid, h_kdF_head⟩ := h_kdF
      have h_v_mem : v ∈ kdF.gonderKuyrugu := head?_mem h_kdF_head
      have h_kdF_dolu : kdF.gonderKuyrugu ≠ [] := by
        intro h_nil; rw [h_nil] at h_kdF_head; cases h_kdF_head
      have h14 := h_konf.2.2.2.2.2.2.2.2.2.2.2.2.2.1
      have h15 := h_konf.2.2.2.2.2.2.2.2.2.2.2.2.2.2
      refine ⟨Ρ, h_konf.1, ?_, ?_, ?_, rfl,
              h_konf.2.2.2.2.2.1, ?_, ?_, ?_,
              h_konf.2.2.2.2.2.2.2.2.2.1, ?_,
              h_konf.2.2.2.2.2.2.2.2.2.2.2.1, ?_, ?_, ?_⟩
      · -- comp 2: odakli thread alinan degere ilerledi (KanalTutarli tipi)
        have h_thread := h_konf.2.1
        rw [h_t] at h_thread
        have h_dtv := h_konf.2.2.2.1 kdF h_kdF_in v h_v_mem
        exact threadTipli_degisim h_thread _
          ⟨Δ kdF.kid, ctx.lineer, Ρ,
           ⟨HasType.t_sabit _ _ _ _ (degerTipli_ortam v _ h_dtv),
            LineerTamam.l_sabit _ _ _, RegionTamam.r_sabit _ _ _⟩⟩
      · -- comp 3 (id-genel): tb zaten kayitliydi (eski kanalSahip kaydi)
        intro bb sah h_lk
        by_cases h_eq : bb.id = tb.id
        · obtain ⟨x', b', h_x', h_id'⟩ :=
            h_konf.2.2.1 tb (Sahip.kanalSahip k) h_transit
          exact ⟨x', b', h_x', h_id'.trans h_eq.symm⟩
        · rw [sahiplikSet_ne S.sahiplik tb bb _ h_eq] at h_lk
          exact h_konf.2.2.1 bb sah h_lk
      · -- comp 4: pop sonrasi kuyruk tipliligi
        intro kd' h_kd' w h_w
        rcases List.mem_map.mp h_kd' with ⟨kd0, h_kd0, h_img⟩
        by_cases h_k0 : kd0.kid = k
        · rw [if_pos h_k0] at h_img
          subst h_img
          exact h_konf.2.2.2.1 kd0 h_kd0 w (tail_uye h_w)
        · rw [if_neg h_k0] at h_img
          subst h_img
          exact h_konf.2.2.2.1 kd0 h_kd0 w h_w
      · -- comp 7: tb'nin yeni sahibi thread (donmus degil)
        intro xx bb h_bb
        constructor
        · intro h_fr
          have h_fr' : sahiplikGet
              (sahiplikSet S.sahiplik tb (Sahip.thread ctx.tid)) bb
              = some Sahip.donmus := h_fr
          by_cases h_eq : bb.id = tb.id
          · rw [sahiplikSet, sahiplikGet, if_pos h_eq.symm] at h_fr'
            cases h_fr'
          · rw [sahiplikSet_ne S.sahiplik tb bb _ h_eq] at h_fr'
            exact (h_konf.2.2.2.2.2.2.1 xx bb h_bb).mp h_fr'
        · intro h_kat
          have h_eski := (h_konf.2.2.2.2.2.2.1 xx bb h_bb).mpr h_kat
          have h_eq : bb.id ≠ tb.id := by
            intro he
            have h_e2 := (sahiplikGet_id_esit S.sahiplik bb tb he).symm.trans h_eski
            have := h_transit.symm.trans h_e2
            cases this
          show sahiplikGet
              (sahiplikSet S.sahiplik tb (Sahip.thread ctx.tid)) bb
              = some Sahip.donmus
          rw [sahiplikSet_ne S.sahiplik tb bb _ h_eq]
          exact h_eski
      · -- comp 8: hedef bolge tb olamaz (eski sahibi kanal'di)
        intro c h_mem y h_hv bb h_bb h_yaz
        have h_c_eski : c ∈ S.thread ∧ sahiplikGet S.sahiplik bb
            = some (Sahip.thread c.tid) := by
          rcases List.mem_append.mp h_mem with h1 | h2
          · have h_in : c ∈ S.thread := by
              rw [h_t]; exact List.mem_append.mpr (Or.inl h1)
            exact ⟨h_in, h_konf.2.2.2.2.2.2.2.1 c h_in y h_hv bb h_bb h_yaz⟩
          · rcases List.mem_cons.mp h2 with h_eq | h3
            · subst h_eq; exact absurd h_hv (fun h => nomatch h)
            · have h_in : c ∈ S.thread := by
                rw [h_t]
                exact List.mem_append.mpr (Or.inr (List.Mem.tail _ h3))
              exact ⟨h_in, h_konf.2.2.2.2.2.2.2.1 c h_in y h_hv bb h_bb h_yaz⟩
        obtain ⟨_, h_c_own⟩ := h_c_eski
        have h_ne : bb.id ≠ tb.id := by
          intro he
          have h_e2 := (sahiplikGet_id_esit S.sahiplik bb tb he).symm.trans h_c_own
          have := h_transit.symm.trans h_e2
          cases this
        show sahiplikGet
            (sahiplikSet S.sahiplik tb (Sahip.thread ctx.tid)) bb
            = some (Sahip.thread c.tid)
        rw [sahiplikSet_ne S.sahiplik tb bb _ h_ne]
        exact h_c_own
      · -- comp 9: ayni desen
        intro c h_mem bb h_hb h_kayit h_yaz
        have h_c_eski : c ∈ S.thread ∧ sahiplikGet S.sahiplik bb
            = some (Sahip.thread c.tid) := by
          rcases List.mem_append.mp h_mem with h1 | h2
          · have h_in : c ∈ S.thread := by
              rw [h_t]; exact List.mem_append.mpr (Or.inl h1)
            exact ⟨h_in, h_konf.2.2.2.2.2.2.2.2.1 c h_in bb h_hb h_kayit h_yaz⟩
          · rcases List.mem_cons.mp h2 with h_eq | h3
            · subst h_eq; exact absurd h_hb (fun h => nomatch h)
            · have h_in : c ∈ S.thread := by
                rw [h_t]
                exact List.mem_append.mpr (Or.inr (List.Mem.tail _ h3))
              exact ⟨h_in, h_konf.2.2.2.2.2.2.2.2.1 c h_in bb h_hb h_kayit h_yaz⟩
        obtain ⟨_, h_c_own⟩ := h_c_eski
        have h_ne : bb.id ≠ tb.id := by
          intro he
          have h_e2 := (sahiplikGet_id_esit S.sahiplik bb tb he).symm.trans h_c_own
          have := h_transit.symm.trans h_e2
          cases this
        show sahiplikGet
            (sahiplikSet S.sahiplik tb (Sahip.thread ctx.tid)) bb
            = some (Sahip.thread c.tid)
        rw [sahiplikSet_ne S.sahiplik tb bb _ h_ne]
        exact h_c_own
      · -- comp 11: pop edilen kanal bosalir (kapasite-1); digerleri korunur
        intro kd' h_kd' h_ne'
        rcases List.mem_map.mp h_kd' with ⟨kd0, h_kd0, h_img⟩
        by_cases h_k0 : kd0.kid = k
        · rw [if_pos h_k0] at h_img
          subst h_img
          have h_kd0F : kd0 = kdF :=
            h15 kd0 h_kd0 kdF h_kdF_in (h_k0.trans h_kdF_kid.symm)
          exfalso
          apply h_ne'
          show kd0.gonderKuyrugu.tail = []
          rw [h_kd0F]
          exact tail_bos_kapasite (h14 kdF h_kdF_in) h_kdF_dolu
        · rw [if_neg h_k0] at h_img
          subst h_img
          obtain ⟨bT, h_bT⟩ :=
            h_konf.2.2.2.2.2.2.2.2.2.2.1 kd0 h_kd0 h_ne'
          have h_ne2 : bT.id ≠ tb.id := by
            intro he
            have h_e2 := (sahiplikGet_id_esit S.sahiplik bT tb he).symm.trans h_bT
            have := h_transit.symm.trans h_e2
            have h_kk : k = kd0.kid := by injection Option.some.inj this
            exact h_k0 h_kk.symm
          refine ⟨bT, ?_⟩
          show sahiplikGet
              (sahiplikSet S.sahiplik tb (Sahip.thread ctx.tid)) bT
              = some (Sahip.kanalSahip kd0.kid)
          rw [sahiplikSet_ne S.sahiplik tb bT _ h_ne2]
          exact h_bT
      · -- comp 13
        have h13 := h_konf.2.2.2.2.2.2.2.2.2.2.2.2.1
        rw [h_t] at h13
        exact tidAyrik_degisim h13 _ rfl
      · -- comp 14: pop uzunlugu kucultur
        intro kd' h_kd'
        rcases List.mem_map.mp h_kd' with ⟨kd0, h_kd0, h_img⟩
        by_cases h_k0 : kd0.kid = k
        · rw [if_pos h_k0] at h_img
          subst h_img
          exact Nat.le_trans (tail_uzunluk _) (h14 kd0 h_kd0)
        · rw [if_neg h_k0] at h_img
          subst h_img
          exact h14 kd0 h_kd0
      · -- comp 15: uniform map kid-tekilligi korur
        intro kd1 h1 kd2 h2 h_kid
        rcases List.mem_map.mp h1 with ⟨p1, hp1, e1⟩
        rcases List.mem_map.mp h2 with ⟨p2, hp2, e2⟩
        have hk1 : kd1.kid = p1.kid := by
          rw [← e1]
          by_cases h : p1.kid = k
          · rw [if_pos h]
          · rw [if_neg h]
        have hk2 : kd2.kid = p2.kid := by
          rw [← e2]
          by_cases h : p2.kid = k
          · rw [if_pos h]
          · rw [if_neg h]
        have h_p : p1 = p2 :=
          h15 p1 hp1 p2 hp2 (hk1 ▸ hk2 ▸ h_kid)
        rw [← e1, ← e2, h_p]
  | cDondurTamam S S' ts1 ts2 ctx b h_t h_if h_owner h_S' =>
      intro h_konf
      subst h_S'
      obtain ⟨h_sigma, h_thread, h_sahip, h_kanal, h_fault, h_beq, h_fkat,
              h_hvar, h_hbolge, h_bagli, h_transit, h_bayrik, h_tayrik,
              h_kap, h_kayrik⟩ := h_konf
      subst h_beq
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      -- odakli tip inversion: b kayitli (x0) + yazilabilir
      obtain ⟨τ0, Λ0, Ρ0, h_ty⟩ := h_thread ctx h_ctx_in
      rw [h_if] at h_ty
      obtain ⟨_, _, hr0⟩ := h_ty
      have h_inv : ∃ x0, bolgeOrtamGet S.bolge x0 = some b
          ∧ kategoriYazilabilir b.kategori = true := by
        match hr0 with
        | RegionTamam.r_dondur _ _ _ _ x0 h_x0 h_yzb _ =>
          exact ⟨x0, h_x0, h_yzb⟩
      obtain ⟨x0, h_x0, h_yzb⟩ := h_inv
      -- tid-ayriklik + uyelik on-bilgileri
      have h_tneq : ∀ c, (c ∈ ts1 ∨ c ∈ ts2) → c.tid ≠ ctx.tid := by
        intro c hc
        have h13 := h_tayrik
        rw [h_t] at h13
        exact tidAyrik_odakdisi h13 hc
      have h_cin : ∀ c, (c ∈ ts1 ∨ c ∈ ts2) → c ∈ S.thread := by
        intro c hc
        rw [h_t]
        rcases hc with h1 | h2
        · exact List.mem_append.mpr (Or.inl h1)
        · exact List.mem_append.mpr (Or.inr (List.Mem.tail _ h2))
      -- dondur lookup analizleri
      have h_d_inv_id : ∀ x bb,
          bolgeOrtamGet (bolgeOrtamDondurBolge S.bolge b) x = some bb →
          ∃ b0, bolgeOrtamGet S.bolge x = some b0 ∧ b0.id = bb.id := by
        intro x bb h
        rw [dondur_get] at h
        cases h_lk : bolgeOrtamGet S.bolge x with
        | none => rw [h_lk] at h; cases h
        | some br =>
            rw [h_lk, Option.map_some] at h
            by_cases hid : br.id = b.id
            · rw [if_pos hid] at h
              have h2 := congrArg Bolge.id (Option.some.inj h)
              have h_rfl : br.id
                  = (bolgeKategoriDegistir br BolgeKategorisi.donmus).id := rfl
              exact ⟨br, rfl, h_rfl.trans h2⟩
            · rw [if_neg hid] at h
              exact ⟨br, rfl, congrArg Bolge.id (Option.some.inj h)⟩
      have h_d_koruma : ∀ x b0, bolgeOrtamGet S.bolge x = some b0 →
          ∃ b', bolgeOrtamGet (bolgeOrtamDondurBolge S.bolge b) x = some b'
            ∧ b'.id = b0.id := by
        intro x b0 h
        rw [dondur_get, h]
        by_cases hid : b0.id = b.id
        · exact ⟨bolgeKategoriDegistir b0 BolgeKategorisi.donmus,
            by rw [Option.map_some, if_pos hid], rfl⟩
        · exact ⟨b0, by rw [Option.map_some, if_neg hid], rfl⟩
      -- transport kosullari (odaksiz thread'ler)
      have h_hv_kosul : ∀ c, c ∈ S.thread → c.tid ≠ ctx.tid →
          ∀ y, HedefVar c.ifade y →
          bolgeOrtamGet (bolgeOrtamDondurBolge S.bolge b) y
            = bolgeOrtamGet S.bolge y := by
        intro c hc h_ne y hy
        rw [dondur_get]
        cases h_lk : bolgeOrtamGet S.bolge y with
        | none => rfl
        | some by0 =>
            by_cases hid : by0.id = b.id
            · exfalso
              have h_yx : y = x0 := h_bayrik y x0 by0 b h_lk h_x0 hid
              rw [h_yx] at h_lk
              have h_byb : by0 = b := Option.some.inj (h_lk.symm.trans h_x0)
              rw [h_yx] at hy
              rw [h_byb] at h_lk
              have h_own := h_hvar c hc x0 hy b h_lk h_yzb
              rw [h_own] at h_owner
              have h_t2 := Option.some.inj h_owner
              injection h_t2 with h_t3
              exact h_ne h_t3
            · rw [Option.map_some, if_neg hid]
      have h_hb_kosul : ∀ c, c ∈ S.thread → c.tid ≠ ctx.tid →
          ∀ x bb, HedefBolge c.ifade bb →
          kategoriYazilabilir bb.kategori = true →
          bolgeOrtamGet S.bolge x = some bb →
          bolgeOrtamGet (bolgeOrtamDondurBolge S.bolge b) x = some bb := by
        intro c hc h_ne x bb hb h_yz h_lk
        rw [dondur_get, h_lk, Option.map_some]
        by_cases hid : bb.id = b.id
        · exfalso
          have h_xx : x = x0 := h_bayrik x x0 bb b h_lk h_x0 hid
          rw [h_xx] at h_lk
          have h_bbb : bb = b := Option.some.inj (h_lk.symm.trans h_x0)
          rw [h_bbb] at hb h_yz h_lk
          have h_own := h_hbolge c hc b hb ⟨x0, h_lk⟩ h_yz
          rw [h_own] at h_owner
          have h_t2 := Option.some.inj h_owner
          injection h_t2 with h_t3
          exact h_ne h_t3
        · rw [if_neg hid]
      refine ⟨bolgeOrtamDondurBolge S.bolge b,
              ?_, ?_, ?_, ?_, rfl, rfl, ?_, ?_, ?_, ?_, ?_, ?_, ?_,
              h_kap, h_kayrik⟩
      · -- comp 1: SigmaTipli
        intro kk ww h_kw
        obtain ⟨τw, h_dt, x, b', h_x, h_id⟩ := h_sigma kk ww h_kw
        obtain ⟨b'', h_x', h_id'⟩ := h_d_koruma x b' h_x
        exact ⟨τw, degerTipli_ortam ww τw h_dt, x, b'', h_x', h_id'.trans h_id⟩
      · -- comp 2: ThreadTipliFull
        intro c h_mem
        rcases List.mem_append.mp h_mem with h1 | h2
        · obtain ⟨τc, Λc, Ρc, h_tyc⟩ := h_thread c (h_cin c (Or.inl h1))
          obtain ⟨Ρc', h_rc', _⟩ := regionTamam_transport h_tyc.regionOK _
            (h_hv_kosul c (h_cin c (Or.inl h1)) (h_tneq c (Or.inl h1)))
            (h_hb_kosul c (h_cin c (Or.inl h1)) (h_tneq c (Or.inl h1)))
          exact ⟨τc, Λc, Ρc', ⟨h_tyc.hasType, h_tyc.lineerOK, h_rc'⟩⟩
        · rcases List.mem_cons.mp h2 with he | h3
          · subst he
            exact ⟨Tip.bos, ctx.lineer, bolgeOrtamDondurBolge S.bolge b,
              ⟨HasType.t_sabit _ _ _ _ DegerTipli.dt_birim,
               LineerTamam.l_sabit _ _ _, RegionTamam.r_sabit _ _ _⟩⟩
          · obtain ⟨τc, Λc, Ρc, h_tyc⟩ := h_thread c (h_cin c (Or.inr h3))
            obtain ⟨Ρc', h_rc', _⟩ := regionTamam_transport h_tyc.regionOK _
              (h_hv_kosul c (h_cin c (Or.inr h3)) (h_tneq c (Or.inr h3)))
              (h_hb_kosul c (h_cin c (Or.inr h3)) (h_tneq c (Or.inr h3)))
            exact ⟨τc, Λc, Ρc', ⟨h_tyc.hasType, h_tyc.lineerOK, h_rc'⟩⟩
      · -- comp 3: SahiplikTutarli
        intro bb sah h_lk
        by_cases h_eq : bb.id = b.id
        · obtain ⟨b'', h_x', h_id'⟩ := h_d_koruma x0 b h_x0
          exact ⟨x0, b'', h_x', h_id'.trans h_eq.symm⟩
        · rw [sahiplikSet_ne S.sahiplik b bb _ h_eq] at h_lk
          obtain ⟨x, b', h_x, h_id⟩ := h_sahip bb sah h_lk
          obtain ⟨b'', h_x', h_id'⟩ := h_d_koruma x b' h_x
          exact ⟨x, b'', h_x', h_id'.trans h_id⟩
      · -- comp 4: KanalTutarli
        intro kd h_kd w h_w
        exact degerTipli_ortam w _ (h_kanal kd h_kd w h_w)
      · -- comp 7: FrozenKategoriTutarli (id-anahtarin kilit case'i)
        intro x bb h_bb
        rw [dondur_get] at h_bb
        cases h_lk : bolgeOrtamGet S.bolge x with
        | none => rw [h_lk] at h_bb; cases h_bb
        | some b0 =>
            rw [h_lk, Option.map_some] at h_bb
            have h_b0bb := Option.some.inj h_bb
            by_cases hid : b0.id = b.id
            · rw [if_pos hid] at h_b0bb
              constructor
              · intro _
                rw [← h_b0bb]
                simp [bolgeKategoriDegistir]
              · intro _
                show sahiplikGet (sahiplikSet S.sahiplik b Sahip.donmus) bb
                    = some Sahip.donmus
                rw [sahiplikSet, sahiplikGet,
                    if_pos (show b.id = bb.id by rw [← h_b0bb]; exact hid.symm)]
            · rw [if_neg hid] at h_b0bb
              rw [h_b0bb] at h_lk hid
              have h_pres := sahiplikSet_ne S.sahiplik b bb Sahip.donmus hid
              have h_iff := h_fkat x bb h_lk
              constructor
              · intro h_fr
                refine h_iff.mp ?_
                show sahiplikGet S.sahiplik bb = some Sahip.donmus
                rw [← h_pres]
                exact h_fr
              · intro h_kat
                show sahiplikGet (sahiplikSet S.sahiplik b Sahip.donmus) bb
                    = some Sahip.donmus
                rw [h_pres]
                exact h_iff.mpr h_kat
      · -- comp 8: HedefVarSahipligi
        intro c h_mem y hy bb h_bb h_yz
        rw [dondur_get] at h_bb
        have h_unf : c ∈ S.thread → c.tid ≠ ctx.tid →
            sahiplikGet (sahiplikSet S.sahiplik b Sahip.donmus) bb
              = some (Sahip.thread c.tid) := by
          intro hc h_ne
          cases h_lk : bolgeOrtamGet S.bolge y with
          | none => rw [h_lk] at h_bb; cases h_bb
          | some b0 =>
              rw [h_lk, Option.map_some] at h_bb
              have h_b0bb := Option.some.inj h_bb
              by_cases hid : b0.id = b.id
              · rw [if_pos hid] at h_b0bb
                rw [← h_b0bb] at h_yz
                simp [bolgeKategoriDegistir, kategoriYazilabilir] at h_yz
              · rw [if_neg hid] at h_b0bb
                rw [h_b0bb] at h_lk hid
                have h_own := h_hvar c hc y hy bb h_lk h_yz
                rw [sahiplikSet_ne S.sahiplik b bb _ hid]
                exact h_own
        rcases List.mem_append.mp h_mem with h1 | h2
        · exact h_unf (h_cin c (Or.inl h1)) (h_tneq c (Or.inl h1))
        · rcases List.mem_cons.mp h2 with he | h3
          · subst he; exact absurd hy (fun h => nomatch h)
          · exact h_unf (h_cin c (Or.inr h3)) (h_tneq c (Or.inr h3))
      · -- comp 9: HedefBolgeSahipligi
        intro c h_mem bb hb h_kayit h_yz
        obtain ⟨x, h_x⟩ := h_kayit
        rw [dondur_get] at h_x
        have h_unf : c ∈ S.thread → c.tid ≠ ctx.tid →
            sahiplikGet (sahiplikSet S.sahiplik b Sahip.donmus) bb
              = some (Sahip.thread c.tid) := by
          intro hc h_ne
          cases h_lk : bolgeOrtamGet S.bolge x with
          | none => rw [h_lk] at h_x; cases h_x
          | some b0 =>
              rw [h_lk, Option.map_some] at h_x
              have h_b0bb := Option.some.inj h_x
              by_cases hid : b0.id = b.id
              · rw [if_pos hid] at h_b0bb
                rw [← h_b0bb] at h_yz
                simp [bolgeKategoriDegistir, kategoriYazilabilir] at h_yz
              · rw [if_neg hid] at h_b0bb
                rw [h_b0bb] at h_lk hid
                have h_own := h_hbolge c hc bb hb ⟨x, h_lk⟩ h_yz
                rw [sahiplikSet_ne S.sahiplik b bb _ hid]
                exact h_own
        rcases List.mem_append.mp h_mem with h1 | h2
        · exact h_unf (h_cin c (Or.inl h1)) (h_tneq c (Or.inl h1))
        · rcases List.mem_cons.mp h2 with he | h3
          · subst he; exact absurd hb (fun h => nomatch h)
          · exact h_unf (h_cin c (Or.inr h3)) (h_tneq c (Or.inr h3))
      · -- comp 10: DegiskenlerBagli
        intro z τz h_gz
        obtain ⟨bz, vz, h_bz, h_kz, h_dtz⟩ := h_bagli z τz h_gz
        obtain ⟨bz', h_bz', h_idz⟩ := h_d_koruma z bz h_bz
        refine ⟨bz', vz, h_bz', ?_, degerTipli_ortam vz τz h_dtz⟩
        rw [konumGet_id_esit S.store ⟨bz', 0⟩ ⟨bz, 0⟩ h_idz rfl]
        exact h_kz
      · -- comp 11: KanalTransit
        intro kd h_kd h_ne'
        obtain ⟨bT, h_bT⟩ := h_transit kd h_kd h_ne'
        have h_idne : bT.id ≠ b.id := by
          intro h_id
          rw [sahiplikGet_id_esit S.sahiplik bT b h_id, h_owner] at h_bT
          cases h_bT
        refine ⟨bT, ?_⟩
        show sahiplikGet (sahiplikSet S.sahiplik b Sahip.donmus) bT
            = some (Sahip.kanalSahip kd.kid)
        rw [sahiplikSet_ne S.sahiplik b bT _ h_idne]
        exact h_bT
      · -- comp 12: BolgeAyrik
        intro x1 x2 b1 b2 h_1 h_2 h_id
        obtain ⟨b01, h_01, h_i1⟩ := h_d_inv_id x1 b1 h_1
        obtain ⟨b02, h_02, h_i2⟩ := h_d_inv_id x2 b2 h_2
        exact h_bayrik x1 x2 b01 b02 h_01 h_02
          (h_i1.trans (h_id.trans h_i2.symm))
      · -- comp 13: TidAyrik
        have h13 := h_tayrik
        rw [h_t] at h13
        exact tidAyrik_degisim h13 _ rfl
  -- ============ Hata kurallari: Aile 2 exfalso — TAM ============
  | sAtamaHataDonmus S S' ts1 ts2 ctx x v b h_t h_if h_b h_frozen h_S' =>
      intro h_konf
      exfalso
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      obtain ⟨τ', Λ'', Ρ'', h_typed⟩ := h_konf.2.1 ctx h_ctx_in
      rw [h_if] at h_typed
      exact typing_excludes_sAtamaHataDonmus Γ Δ ctx.lineer Ρ x _ τ' Λ'' Ρ''
        h_typed S b h_konf.2.2.2.2.2.1 h_konf.2.2.2.2.2.2.1 h_b h_frozen
  | sAtamaHataSahipDegil S S' ts1 ts2 ctx x v b h_t h_if h_b h_not_owner h_S' =>
      intro h_konf
      exfalso
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      obtain ⟨τ', Λ'', Ρ'', h_typed⟩ := h_konf.2.1 ctx h_ctx_in
      rw [h_if] at h_typed
      exact typing_excludes_sAtamaHataSahipDegil Γ Δ ctx.lineer Ρ S ctx x _ b
        τ' Λ'' Ρ'' h_typed h_ctx_in h_if h_konf.2.2.2.2.2.1
        h_konf.2.2.2.2.2.2.2.1 h_b h_not_owner
  | cGorevBaslatHataLineerIhlal S S' ts1 ts2 ctx yd kod vIhlal h_t h_if h_in h_tuket h_S' =>
      intro h_konf
      exfalso
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      obtain ⟨τ', Λ'', Ρ'', h_typed⟩ := h_konf.2.1 ctx h_ctx_in
      rw [h_if] at h_typed
      exact typing_excludes_cGorevBaslatHataLineerIhlal Γ Δ ctx.lineer Ρ
        yd kod vIhlal τ' Λ'' Ρ'' h_typed h_in h_tuket
  | cKanalGonderHataLineerTuket S S' ts1 ts2 ctx k vId h_t h_if h_tuket h_S' =>
      intro h_konf
      exfalso
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      obtain ⟨τ', Λ'', Ρ'', h_typed⟩ := h_konf.2.1 ctx h_ctx_in
      rw [h_if] at h_typed
      exact typing_excludes_cKanalGonderHataLineerTuket Γ Δ ctx.lineer Ρ
        k vId τ' Λ'' Ρ'' h_typed h_tuket
  | cDondurHataZatenDonmus S S' ts1 ts2 ctx b h_t h_if h_zaten h_S' =>
      intro h_konf
      exfalso
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      obtain ⟨τ', Λ'', Ρ'', h_typed⟩ := h_konf.2.1 ctx h_ctx_in
      rw [h_if] at h_typed
      exact typing_excludes_cDondurHataZatenDonmus Γ Δ ctx.lineer Ρ
        b τ' Λ'' Ρ'' h_typed S
        h_konf.2.2.2.2.2.1 h_konf.2.2.2.2.2.2.1 h_zaten
  | sLinKullanHataZatenTuketildi S S' ts1 ts2 ctx x h_t h_if h_tuket h_S' =>
      intro h_konf
      exfalso
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      obtain ⟨τ', Λ'', Ρ'', h_typed⟩ := h_konf.2.1 ctx h_ctx_in
      rw [h_if] at h_typed
      exact typing_excludes_sLinKullanHataZatenTuketildi Γ Δ ctx.lineer Ρ
        x τ' Λ'' Ρ'' h_typed h_tuket
  | sLinImhaHataZatenTuketildi S S' ts1 ts2 ctx x h_t h_if h_tuket h_S' =>
      intro h_konf
      exfalso
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      obtain ⟨τ', Λ'', Ρ'', h_typed⟩ := h_konf.2.1 ctx h_ctx_in
      rw [h_if] at h_typed
      exact typing_excludes_sLinImhaHataZatenTuketildi Γ Δ ctx.lineer Ρ
        x τ' Λ'' Ρ'' h_typed h_tuket
  -- ============ Congruence — 🔴 DUR-SOR: Step-duzeltmesi gerekli ====
  -- KRITIK (2026-06-12, DECISIONS_LOG son kayit): OdakUyum-tasariminin
  -- saglamlik-testi, adim_korunum'un MEVCUT Step ile YANLIS oldugunu
  -- gosterdi — cong-penceresi (ifadeyleKonf) kosan thread'i pencerede
  -- "bitmis" gosterir; cGorevBirlestirTamam.h_hedef (ifade-okuyan tek
  -- global premise, tHedef serbest) pencereden kanar: kosan thread'in
  -- bolgeleri calinir, restorasyon sonrasi comp-8 HER Ρ' icin ihlal
  -- (tam counterexample DECISIONS_LOG'da). Fix-F (cong cerceve
  -- yan-kosulu: ts2' = ts2 ∨ spawn-append) onayi olmadan bu 3 case
  -- KAPATILAMAZ (sorry-0 matematiksel imkansiz).
  --
  -- Onay sonrasi plan: Fix-F + asagidaki ODAK-ADIM GUCLENDIRILMIS-IH
  -- ("kalan-yukumluluk tasima"; lineer ayagi LineerKucuk-ailesiyle
  -- HAZIR — LineerTamam.lean §5).
  --
  -- Sorun: IH yalniz KonfTipliFull S1' verir; S1 odakta YALNIZ a
  -- tasidigindan devam-ifadesi b'nin (i) Λmid/Ρmid-altinda yeniden
  -- tiplenmesi (l_seq/r_seq kompozisyonu) ve (ii) hedef-sahiplikleri
  -- (comp-8/9 buyuyen hedef kumesi) IH'den GELMEZ.
  --
  -- Cozum tasarimi (sonraki oturum — adim_korunum'un mutual
  -- guclendirilmesi, final teorem ifadesi DEGISMEZ):
  -- (1) adim_korunum sonucu odak-yuku tasisin:
  --     ∃ Ρ', KonfTipliFull Γ Δ Ρ' S' ∧ OdakUyum S S' Ρ Ρ' — OdakUyum:
  --     odakli thread'in lineer'i icin Λ' ≼ Λ-statik-cikti
  --     (≼ : tuketildi'ler ⊆, aktif'ler ⊇ — sVarOku lineer-okumayi
  --     runtime'da tuketmedigi icin esitlik degil monotonluk) ve
  --     Ρ'-vs-Ρ icin yazilabilir-hedeflerde mutabakat.
  -- (2) lineer-≼ transport: LineerTamam Γ Λa b Λb ∧ Λmid ≼ Λa →
  --     ∃ Λb', LineerTamam Γ Λmid b Λb' (13 kural, aktif/¬tuketildi
  --     premise'leri ≼ altinda monoton).
  -- (3) b'nin Region tarafi regionTamam_transport ile (mevcut),
  --     kosullari OdakUyum'dan.
  -- (4) comp-8/9 buyuyen kume: b'nin hedefleri Ρmid'de yazilabilir
  --     (r_seq ikinci premise) → yaz_geri ile a-dokunmamis →
  --     sahiplik korunmus.
  | sSeqCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' a a' b h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      intro h_konf
      -- TODO 🔴 TEK BLOKER: odak-adim guclendirilmis-IH (yukaridaki blok).
      sorry
  | sAtamaCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' x e e' h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      intro h_konf
      -- TODO 🔴 TEK BLOKER: odak-adim guclendirilmis-IH (sSeqCong blogu).
      sorry
  | sGuvensizCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' e e' h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      intro h_konf
      -- TODO 🔴 TEK BLOKER: odak-adim guclendirilmis-IH (sSeqCong blogu).
      sorry


-- ============================================================
-- §3. No-Fault catı teoremi (Plan v2 §6.3) — TAM BAGLANDI
-- ============================================================

/-- TYPED NO-FAULT CATI (Plan v2 §6.3):
    KonfTipliFull S₀ → StepStar S₀ S → S.fault = none.

    F4: adim_korunum'un StepStar kosesi — TEK ILERI INDUKSIYON
    (eski {typed_no_fault ↔ preservation} hipotez dongusu KIRILDI;
    tek sorry kaynagi adim_korunum iskeleti). -/
theorem typed_no_fault
    (Γ : TipOrtam) (Δ : KanalOrtam) (Ρ : BolgeOrtam)
    (S₀ S : Konfigurasyon)
    (h_typed_init : KonfTipliFull Γ Δ Ρ S₀)
    (h_run : StepStar S₀ S) :
    S.fault = none := by
  induction h_run generalizing Ρ with
  | refl _ =>
    exact h_typed_init.2.2.2.2.1
  | step S0 S1 Send hStep _hStar ih =>
    obtain ⟨Ρ1, h_konf1⟩ := adim_korunum Γ Δ Ρ S0 S1 hStep h_typed_init
    exact ih Ρ1 h_konf1

/-- Program-seviyesi No-Fault (F3 koprusuyle — kagit formu):
    IyiTipliCekirdek(Π) ⟹ S₀(Π)'den ulasilabilir hicbir konfigurasyon fault degil. -/
theorem iyiTipli_no_fault
    (Pi : Program) (h_iyi : IyiTipliCekirdek Pi)
    (S : Konfigurasyon)
    (h_run : StepStar (baslangicKonf Pi) S) :
    S.fault = none :=
  typed_no_fault (gammaProgram Pi) (deltaProgram Pi) (rhoBaslangic Pi)
    (baslangicKonf Pi) S (iyiTipli_baslangic Pi h_iyi) h_run

end Kemgu.Discharge.NoFault
