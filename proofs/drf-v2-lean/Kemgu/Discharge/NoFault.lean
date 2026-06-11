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
        have h_mem := konumGet_mem S.store ⟨b, 0⟩ v h_v
        obtain ⟨τv, h_dt, _⟩ := h_konf.1 ⟨b, 0⟩ v h_mem
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
      -- TODO F4-ispat: store-push — comp 1 (degerTipli_ortam []→Ρ + h_b
      -- kayitliligi), comp 10 (BolgeAyrik 12. bilesen GEREKLI: yeni
      -- (⟨b,0⟩,v) girisi yalniz x'in konumunu golgeler — bolge-id↔var
      -- enjektifligi; on-onayli mekanik ekleme, catal cozumuyle birlikte).
      sorry
  | cGorevBaslatTamam S S' ts1 ts2 ctx tYeni yd kod h_t h_if h_fresh h_sahipler h_S' =>
      intro h_konf
      -- TODO F4-ispat 🔴 CATAL: Ρ-DEGISTIREN kural (bolge := sahipAta).
      -- Odaksiz thread'lerin + cocugun RegionTamam'i guncellenmis-Ρ'da
      -- yeniden kurulmali → DUR-SOR raporundaki Yol A/B/C karari gerekli.
      sorry
  | cGorevBirlestirTamam S S' ts1 ts2 ctx g tHedef rb h_t h_if h_hedef h_donen h_S' =>
      intro h_konf
      -- TODO F4-ispat: sahiplik := setMany rb (thread ctx.tid) —
      -- comp 3/7/8/9 icin sahiplikSetMany_lookup_inv + TidAyrik bileseni
      -- (bitmis thread tekilligi) — mekanik, catal-bagimsiz.
      sorry
  | cKanalGonderTamam S S' ts1 ts2 ctx k vId b v h_t h_if h_b h_v h_owner h_bos h_S' =>
      intro h_konf
      -- TODO F4-ispat 🔴 CATAL: Ρ-DEGISTIREN kural (bolge := update kanalRho).
      sorry
  | cKanalAlTamam S S' ts1 ts2 ctx k v tb h_t h_if h_v h_transit h_S' =>
      intro h_konf
      -- TODO F4-ispat: sahiplik := set tb (thread ctx.tid); kanal pop —
      -- comp 4 (kanalCikar tail-tipliligi), comp 11 (pop sonrasi transit:
      -- kuyruk hala dolu ise ayni kanalda IKINCI transit bolge gerekir —
      -- KanalTransit'in coklu-mesaj formu KONTROL EDILMELI; comp 2 odakli
      -- sabit v tiplemesi KanalTutarli'dan. Catal-bagimsiz, mekanik agirlikli.
      sorry
  | cDondurTamam S S' ts1 ts2 ctx b h_t h_if h_owner h_S' =>
      intro h_konf
      -- TODO F4-ispat 🔴 CATAL: Ρ-DEGISTIREN kural (bolge := dondurBolge).
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
      -- TODO F4-ispat 🔴 CATAL-bagimli: yeniden-sarma icin cikti-ortam-
      -- kararli IH + b-parcasinin transportu (DUR-SOR raporu).
      sorry
  | sAtamaCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' x e e' h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_S' ih =>
      intro h_konf
      -- TODO F4-ispat 🔴 CATAL-bagimli (sSeqCong ile ayni).
      sorry
  | sGuvensizCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' e e' h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_S' ih =>
      intro h_konf
      -- TODO F4-ispat 🔴 CATAL-bagimli (sSeqCong ile ayni).
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
    IyiTipli(Π) ⟹ S₀(Π)'den ulasilabilir hicbir konfigurasyon fault degil. -/
theorem iyiTipli_no_fault
    (Pi : Program) (h_iyi : IyiTipli Pi)
    (S : Konfigurasyon)
    (h_run : StepStar (baslangicKonf Pi) S) :
    S.fault = none :=
  typed_no_fault (gammaProgram Pi) (deltaProgram Pi) (rhoBaslangic Pi)
    (baslangicKonf Pi) S (iyiTipli_baslangic Pi h_iyi) h_run

end Kemgu.Discharge.NoFault
