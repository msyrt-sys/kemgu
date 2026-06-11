/-
KEMGU DRF Mekanize — Tipli Katmani (Onarim v3 F1 + F2 + F3)
Kaynak: ADIM0_DENETIM_RAPORU.md Bolum 2.2 + FAZ_BRIFINGLERI.md
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz, sorry/axiom YOK

F3 guncellemeleri:
- Δ (KanalOrtam) parametresi tum katmanlara islendi.
- KonfTipliFull'a 9. bilesen DegiskenlerBagli eklendi: Γ'daki her degisken
  icin bolge + konum + tip-uyumlu deger mevcut (sVarOku progress +
  preservation temeli).
- KanalTutarli Δ'li kesin formda.
-/

import Kemgu.Sem.Core
import Kemgu.Sem.StateTipli
import Kemgu.Sem.HasType
import Kemgu.Sem.LineerTamam
import Kemgu.Sem.RegionTamam

namespace Kemgu.Sem.Tipli
open Kemgu.Sem.Core Kemgu.Sem.StateTipli Kemgu.Sem.HasType
     Kemgu.Sem.LineerTamam Kemgu.Sem.RegionTamam

-- ============================================================
-- §1. Typed full conjunction (Plan v2 §3.6) — Δ'li form
-- ============================================================

/-- Typed full — uc katmanin tam birlesimi (Plan §3.6). -/
structure Typed (Γ : TipOrtam) (Δ : KanalOrtam) (Λ : LineerOrtam)
                (Ρ : BolgeOrtam) (e : Ifade) (τ : Tip)
                (Λ' : LineerOrtam) (Ρ' : BolgeOrtam) : Prop where
  hasType    : HasType Γ Δ e τ
  lineerOK   : LineerTamam Γ Λ e Λ'
  regionOK   : RegionTamam Γ Ρ e Ρ'

/-- "Bos bolge ortami" — program seviyesi (kapatilmis tipli ifadeler). -/
abbrev bolgeOrtamBos : BolgeOrtam := []


-- ============================================================
-- §2. Tipleme tersine-cevirme lemmalari (F2 — congruence destegi)
-- ============================================================

/-- Seq sol-bileseni tiplidir (sSeqCong tipleme tarafi). -/
theorem typed_seq_sol {Γ : TipOrtam} {Δ : KanalOrtam} {Λ : LineerOrtam}
    {Ρ : BolgeOrtam} {a b : Ifade} {τ : Tip} {Λ' : LineerOrtam} {Ρ' : BolgeOrtam}
    (h : Typed Γ Δ Λ Ρ (Ifade.seq a b) τ Λ' Ρ') :
    ∃ τa Λa Ρa, Typed Γ Δ Λ Ρ a τa Λa Ρa := by
  obtain ⟨ht, hl, hr⟩ := h
  match ht, hl, hr with
  | HasType.t_seq _ _ _ _ τa _ hta _,
    LineerTamam.l_seq _ _ Λa _ _ _ hla _,
    RegionTamam.r_seq _ _ Ρa _ _ _ hra _ =>
    exact ⟨τa, Λa, Ρa, ⟨hta, hla, hra⟩⟩

/-- Atama RHS'i tiplidir (sAtamaCong tipleme tarafi). -/
theorem typed_atama_ic {Γ : TipOrtam} {Δ : KanalOrtam} {Λ : LineerOrtam}
    {Ρ : BolgeOrtam} {x : VarId} {e : Ifade} {τ : Tip}
    {Λ' : LineerOrtam} {Ρ' : BolgeOrtam}
    (h : Typed Γ Δ Λ Ρ (Ifade.atama x e) τ Λ' Ρ') :
    ∃ τe Λe Ρe, Typed Γ Δ Λ Ρ e τe Λe Ρe := by
  obtain ⟨ht, hl, hr⟩ := h
  match ht, hl, hr with
  | HasType.t_atama _ _ _ _ τx _ hte,
    LineerTamam.l_atama _ _ _ _ _ hle,
    RegionTamam.r_atama _ _ _ _ _ _ _ _ hre =>
    exact ⟨τx, Λ', Ρ', ⟨hte, hle, hre⟩⟩

/-- Guvensiz ic ifadesi tiplidir (sGuvensizCong tipleme tarafi). -/
theorem typed_guvensiz_ic {Γ : TipOrtam} {Δ : KanalOrtam} {Λ : LineerOrtam}
    {Ρ : BolgeOrtam} {e : Ifade} {τ : Tip} {Λ' : LineerOrtam} {Ρ' : BolgeOrtam}
    (h : Typed Γ Δ Λ Ρ (Ifade.guvensiz e) τ Λ' Ρ') :
    ∃ τe Λe Ρe, Typed Γ Δ Λ Ρ e τe Λe Ρe := by
  obtain ⟨ht, hl, hr⟩ := h
  match ht, hl, hr with
  | HasType.t_guvensiz _ _ _ _ hte,
    LineerTamam.l_guvensiz _ _ _ _ hle,
    RegionTamam.r_guvensiz _ _ _ _ hre =>
    exact ⟨τ, Λ', Ρ', ⟨hte, hle, hre⟩⟩


-- ============================================================
-- §3. AtamaOdak — degerlendirme pozisyonundaki atama hedefleri (F2)
-- ============================================================

/-- `AtamaOdak e y`: e'nin degerlendirme pozisyonunda (seq-sol /
    atama-RHS / guvensiz-ic zinciri) hedefi y olan bir atama var.
    Odaklama (congruence) altinda kapali. -/
inductive AtamaOdak : Ifade → VarId → Prop where
  | bas (y : VarId) (e : Ifade) :
      AtamaOdak (Ifade.atama y e) y
  | seq_sol (a b : Ifade) (y : VarId) :
      AtamaOdak a y → AtamaOdak (Ifade.seq a b) y
  | atama_ic (x : VarId) (e : Ifade) (y : VarId) :
      AtamaOdak e y → AtamaOdak (Ifade.atama x e) y
  | guvensiz_ic (e : Ifade) (y : VarId) :
      AtamaOdak e y → AtamaOdak (Ifade.guvensiz e) y


-- ============================================================
-- §4. ThreadTipliFull — Plan v2 §5.2.3 (Δ'li)
-- ============================================================

/-- Thread'lerin tip-uyumu (F4: PER-THREAD lineer ortam — her ctx kendi
    ctx.lineer'i ile Typed; paylasimli-Λ iff koprusu SILINDI; ADIM 0
    acik soru 3 onayi). -/
def ThreadTipliFull (Γ : TipOrtam) (Δ : KanalOrtam)
                    (Ρ : BolgeOrtam) (threads : List ThreadCtx) : Prop :=
  ∀ ctx ∈ threads,
    ∃ τ : Tip, ∃ Λ' : LineerOrtam, ∃ Ρ' : BolgeOrtam,
      Typed Γ Δ ctx.lineer Ρ ctx.ifade τ Λ' Ρ'


-- ============================================================
-- §5. KonfTipliFull — Plan v2 §5.2 ana merkez predikat (F3: 9 bilesen)
-- ============================================================

/-- Konfigurasyon tipli (F3 formu — 9 bilesen):
    1. SigmaTipli  2. ThreadTipliFull  3. SahiplikTutarli  4. KanalTutarli(Δ)
    5. fault=none  6. S.bolge = Ρ  7. FrozenKategoriTutarli
    8. AtamaSahipligi (AtamaOdak formu)
    9. DegiskenlerBagli (F3 YENI): Γ'daki her degisken icin bolge + konum +
       tip-uyumlu deger mevcut — sVarOku progress/preservation temeli. -/
def KonfTipliFull (Γ : TipOrtam) (Δ : KanalOrtam)
                  (Ρ : BolgeOrtam) (S : Konfigurasyon) : Prop :=
  SigmaTipli Γ Ρ S.store
  ∧ ThreadTipliFull Γ Δ Ρ S.thread
  ∧ SahiplikTutarli Ρ S.sahiplik
  ∧ KanalTutarli Γ Δ Ρ S.kanal
  ∧ S.fault = none
  ∧ S.bolge = Ρ
  ∧ (∀ (x : VarId) (b : Bolge),
       bolgeOrtamGet S.bolge x = some b →
       (isFrozen S b ↔ b.kategori = BolgeKategorisi.donmus))
  ∧ (∀ ctx ∈ S.thread, ∀ y : VarId, AtamaOdak ctx.ifade y →
       ∀ b : Bolge, bolgeOrtamGet S.bolge y = some b →
         sahiplikGet S.sahiplik b = some (Sahip.thread ctx.tid))
  ∧ (∀ (x : VarId) (τ : Tip), tipOrtamGet Γ x = some τ →
       ∃ (b : Bolge) (v : Deger),
         bolgeOrtamGet S.bolge x = some b
         ∧ konumGet S.store ⟨b, 0⟩ = some v
         ∧ DegerTipli Γ Ρ v τ)


-- ============================================================
-- §6. KonfTipliFull projeksiyon yardimi
-- ============================================================

/-- KonfTipliFull'den bilesenleri cikarma (9-bilesen projection). -/
theorem konfTipliFull_elim
    (Γ : TipOrtam) (Δ : KanalOrtam) (Ρ : BolgeOrtam)
    (S : Konfigurasyon) (h : KonfTipliFull Γ Δ Ρ S) :
    SigmaTipli Γ Ρ S.store
    ∧ ThreadTipliFull Γ Δ Ρ S.thread
    ∧ SahiplikTutarli Ρ S.sahiplik
    ∧ KanalTutarli Γ Δ Ρ S.kanal
    ∧ S.fault = none
    ∧ S.bolge = Ρ
    ∧ (∀ (x : VarId) (b : Bolge),
         bolgeOrtamGet S.bolge x = some b →
         (isFrozen S b ↔ b.kategori = BolgeKategorisi.donmus))
    ∧ (∀ ctx ∈ S.thread, ∀ y : VarId, AtamaOdak ctx.ifade y →
         ∀ b : Bolge, bolgeOrtamGet S.bolge y = some b →
           sahiplikGet S.sahiplik b = some (Sahip.thread ctx.tid))
    ∧ (∀ (x : VarId) (τ : Tip), tipOrtamGet Γ x = some τ →
         ∃ (b : Bolge) (v : Deger),
           bolgeOrtamGet S.bolge x = some b
           ∧ konumGet S.store ⟨b, 0⟩ = some v
           ∧ DegerTipli Γ Ρ v τ) :=
  h


-- ============================================================
-- §7. Odaklama lemmasi (F2) — congruence case'lerinin anahtari
-- ============================================================

/-- KonfTipliFull odaklama altinda kapali (F3: 9 bilesen — yeni
    DegiskenlerBagli bileseni state-only, odaklamadan etkilenmez). -/
theorem konfTipliFull_odak
    (Γ : TipOrtam) (Δ : KanalOrtam) (Ρ : BolgeOrtam)
    (S : Konfigurasyon) (ts1 ts2 : List ThreadCtx) (ctx : ThreadCtx)
    (e : Ifade)
    (h_konf : KonfTipliFull Γ Δ Ρ S)
    (h_t : S.thread = ts1 ++ ctx :: ts2)
    (h_typed_e : ∃ τ Λ' Ρ', Typed Γ Δ ctx.lineer Ρ e τ Λ' Ρ')
    (h_odak_kapali : ∀ y : VarId, AtamaOdak e y → AtamaOdak ctx.ifade y) :
    KonfTipliFull Γ Δ Ρ
      { S with thread := ts1 ++ { ctx with ifade := e } :: ts2 } := by
  obtain ⟨h_sigma, h_thread, h_sahip, h_kanal, h_fault, h_beq, h_fkat,
          h_asahip, h_bagli⟩ := h_konf
  have h_ctx_in : ctx ∈ S.thread := by
    rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
  refine ⟨h_sigma, ?_, h_sahip, h_kanal, h_fault, h_beq, h_fkat, ?_, h_bagli⟩
  · -- ThreadTipliFull (guncellenmis liste)
    intro ctx'' h_mem
    rcases List.mem_append.mp h_mem with h1 | h2
    · have h_in : ctx'' ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inl h1)
      exact h_thread ctx'' h_in
    · rcases List.mem_cons.mp h2 with h_eq | h3
      · subst h_eq
        exact h_typed_e
      · have h_in : ctx'' ∈ S.thread := by
          rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.tail _ h3))
        exact h_thread ctx'' h_in
  · -- AtamaSahipligi (guncellenmis liste)
    intro ctx'' h_mem y h_ao b h_bb
    rcases List.mem_append.mp h_mem with h1 | h2
    · have h_in : ctx'' ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inl h1)
      exact h_asahip ctx'' h_in y h_ao b h_bb
    · rcases List.mem_cons.mp h2 with h_eq | h3
      · subst h_eq
        exact h_asahip ctx h_ctx_in y (h_odak_kapali y h_ao) b h_bb
      · have h_in : ctx'' ∈ S.thread := by
          rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.tail _ h3))
        exact h_asahip ctx'' h_in y h_ao b h_bb

end Kemgu.Sem.Tipli
