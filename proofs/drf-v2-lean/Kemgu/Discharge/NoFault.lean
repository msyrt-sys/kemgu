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
  | sSeqCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' a a' b h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_S' ih =>
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
  | sAtamaCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' x e e' h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_S' ih =>
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
  | sGuvensizCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' e e' h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_S' ih =>
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
-- §2. ADIM KORUNUM — birlesik korunum (F4 cekirdegi, dairesellik kirici)
-- ============================================================

/-- BIRLESIK ADIM KORUNUMU (FAZ_BRIFINGLERI.md F4 madde 2):
    Tipli konfigurasyon bir adim sonra da tiplidir (bolge ortami evrilir).

    KAPANIS DURUMU (bu pass):
    - Hata (7): TAM — Aile 2 exfalso (step_fault deseninin aynisi).
    - Tamam (5/11) TAM: sVarOku / sSeqAtla / sGuvensizAtla / sLinKullan /
      sLinImha (bolge+sahiplik+kanal degismeyen kurallar — bilesenler
      degisim-yardimcilariyla tasinir; comp-2 tanigi: degerTipli_ortam /
      typed_seq_atla / typed_guvensiz_ic / dt_birim).
    - KALAN sorry'ler asagida tek tek isaretli:
      sAtamaTamam (BolgeAyrik 12. bilesen gerekli — on-onayli, mekanik);
      cGorevBirlestir/cKanalAl (TidAyrik bileseni + setMany/lookup-inv
      lemmalari — mekanik);
      cGorevBaslat/cKanalGonder/cDondur (Ρ-DEGISTIREN kurallar — odaksiz
      thread'lerin RegionTamam'inin guncellenmis-Ρ'da yeniden kurulmasi
      INVARIANT-MIMARI CATALI gerektirir — DUR-SOR raporunda);
      cong (3) (cikti-ortam-kararli IH — catal cozumune bagimli). -/
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
  -- ============ KALAN Tamam kurallari — isaretli sorry'ler ============
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
      -- TODO 🔴 TEK BLOKER: kategori-anahtar disiplini (DECISIONS_LOG.md
      -- DUR-SOR). id-anahtarlama onayi sonrasi: Yol-B premise + transport
      -- lemmasi ile comp-2; comp-1/7/10 id-anahtarla acilir.
      sorry
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
      -- TODO 🔴 TEK BLOKER: kategori-anahtar (DECISIONS_LOG.md DUR-SOR).
      sorry
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
      -- TODO 🔴 TEK BLOKER: kategori-anahtar (DECISIONS_LOG.md DUR-SOR).
      sorry
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
  -- ============ Congruence — catal cozumune bagimli ============
  | sSeqCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' a a' b h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_S' ih =>
      intro h_konf
      -- TODO 🔴 kategori-anahtar cozumune bagimli: cikti-ortam-kararli IH
      -- + transport (DECISIONS_LOG.md).
      sorry
  | sAtamaCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' x e e' h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_S' ih =>
      intro h_konf
      -- TODO 🔴 kategori-anahtar cozumune bagimli (sSeqCong ile ayni).
      sorry
  | sGuvensizCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' e e' h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_S' ih =>
      intro h_konf
      -- TODO 🔴 kategori-anahtar cozumune bagimli (sSeqCong ile ayni).
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
