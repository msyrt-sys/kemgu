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
      obtain ⟨τ', Λ'', Ρ'', h_typed⟩ := h_typed_S.2.1 ctx h_ctx_in
      rw [h_if] at h_typed
      exact typing_excludes_sAtamaHataDonmus Γ Δ ctx.lineer Ρ x _ τ' Λ'' Ρ''
        h_typed S b h_typed_S.2.2.2.2.2.1 h_typed_S.2.2.2.2.2.2.1 h_b h_frozen
  | sAtamaHataSahipDegil S S' ts1 ts2 ctx x v b h_t h_if h_b h_not_owner h_S' =>
      intro h_typed_S _
      exfalso
      have h_ctx_in : ctx ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
      exact typing_excludes_sAtamaHataSahipDegil S ctx x _ b h_ctx_in h_if
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
          (fun y h => by rw [h_if]; exact AtamaOdak.seq_sol a b y h)
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
          (fun y h => by rw [h_if]; exact AtamaOdak.atama_ic x e y h)
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
          (fun y h => by rw [h_if]; exact AtamaOdak.guvensiz_ic e y h)
      have h_nf1 : (ifadeyleKonf S ts1 ts2 ctx e).fault = none := by
        simpa [ifadeyleKonf] using h_no_fault
      simpa using ih h_konf1 h_nf1


-- ============================================================
-- §2. ADIM KORUNUM — birlesik korunum (F4 cekirdegi, dairesellik kirici)
-- ============================================================

/-- BIRLESIK ADIM KORUNUMU (FAZ_BRIFINGLERI.md F4 madde 2):
    Tipli konfigurasyon bir adim sonra da tiplidir (bolge ortami evrilir).
    Fault-yoklugu sonucun 5. bileseninde — typed_no_fault asagida bunun
    StepStar kosesi olarak ispatlanir (hipotez dongusu YOK).

    ISKELET (sorry) — F4-ispat fazinin kalan isi. Yol haritasi:
    - Hata (7): Aile 2 exfalso (step_fault_preserves_typed deseninin aynisi).
    - Congruence (3): konfTipliFull_odak + IH + yeniden-sarma; sarma icin
      CIKTI-ORTAM-KARARLI guclendirilmis IH gerekir ("ic adim, ifadenin
      statik cikti ortamini korur" formu — ∃-form yetersiz).
    - Tamam (11): bilesen-bilesen; kritik onkosullar:
      * ThreadTipliFull: redex sonuc-degeri tiplenmesi (∃-form sayesinde
        SigmaTipli/KanalTutarli'dan DegerTipli yeterli); spawn cocugu icin
        l_gorev_baslat + r_gorev_baslat'a cocuk-govde premise'i (strengthen).
      * AtamaSahipligi: sSeqAtla'da yeni odaktaki atamanin sahipligi eski
        invariant'tan GELMEZ — tipleme↔sahiplik iliskilendiren ek invariant
        tasarimi gerekir (per-thread erisim-bolgesi sahipligi; DUR-SOR
        isaretli, F4-ispat brifinginde tasarlanacak).
      * FrozenKategori: cDondurTamam S.bolge + sahiplik senkron guncelliyor
        (F2 tasarimi geregi iki taraf birlikte donuyor). -/
theorem adim_korunum
    (Γ : TipOrtam) (Δ : KanalOrtam) (Ρ : BolgeOrtam)
    (S S' : Konfigurasyon) (_h_step : Step S S')
    (_h_konf : KonfTipliFull Γ Δ Ρ S) :
    ∃ Ρ', KonfTipliFull Γ Δ Ρ' S' := by
  -- TODO F4-ispat: yukaridaki yol haritasi. Bkz. FAZ_BRIFINGLERI.md F4.
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
