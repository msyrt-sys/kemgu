/-
KEMGU DRF Mekanize — Tipli Katmani (Onarim v3 F1 + F2)
Kaynak: ADIM0_DENETIM_RAPORU.md Bolum 2.2 + FAZ_BRIFINGLERI.md F1/F2
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz, sorry/axiom YOK

F1: Typed + ThreadTipliFull + KonfTipliFull RegionTamam'dan buraya tasindi.
F2 guncellemeleri:
- SahiplikTutarli artik zaman'siz (guncel-durum sahiplik modeli).
- AtamaSahipligi bileseni `AtamaOdak` formuna genellendi: yalniz tepe-seviye
  `atama` degil, degerlendirme pozisyonundaki (seq-sol / atama-RHS /
  guvensiz-ic) atamalar da kapsanir — boylece bilesen ODAKLAMA (congruence)
  altinda kapali (konfTipliFull_odak icin sart).
- Tipleme tersine-cevirme lemmalari (typed_seq_sol / typed_atama_ic /
  typed_guvensiz_ic) eklendi — congruence case'lerinin tipleme tarafi.
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
-- §1. Typed full conjunction (Plan v2 §3.6)
-- ============================================================

/-- Typed full — uc katmanin tam birlesimi (Plan §3.6). -/
structure Typed (Γ : TipOrtam) (Λ : LineerOrtam) (Ρ : BolgeOrtam)
                (e : Ifade) (τ : Tip)
                (Λ' : LineerOrtam) (Ρ' : BolgeOrtam) : Prop where
  hasType    : HasType Γ e τ
  lineerOK   : LineerTamam Γ Λ e Λ'
  regionOK   : RegionTamam Γ Ρ e Ρ'

/-- "Bos bolge ortami" — program seviyesi (kapatilmis tipli ifadeler). -/
abbrev bolgeOrtamBos : BolgeOrtam := []


-- ============================================================
-- §2. Tipleme tersine-cevirme lemmalari (F2 — congruence destegi)
-- ============================================================

/-- Seq sol-bileseni tiplidir (sSeqCong tipleme tarafi). -/
theorem typed_seq_sol {Γ : TipOrtam} {Λ : LineerOrtam} {Ρ : BolgeOrtam}
    {a b : Ifade} {τ : Tip} {Λ' : LineerOrtam} {Ρ' : BolgeOrtam}
    (h : Typed Γ Λ Ρ (Ifade.seq a b) τ Λ' Ρ') :
    ∃ τa Λa Ρa, Typed Γ Λ Ρ a τa Λa Ρa := by
  obtain ⟨ht, hl, hr⟩ := h
  match ht, hl, hr with
  | HasType.t_seq _ _ _ τa _ hta _,
    LineerTamam.l_seq _ _ Λa _ _ _ hla _,
    RegionTamam.r_seq _ _ Ρa _ _ _ hra _ =>
    exact ⟨τa, Λa, Ρa, ⟨hta, hla, hra⟩⟩

/-- Atama RHS'i tiplidir (sAtamaCong tipleme tarafi). -/
theorem typed_atama_ic {Γ : TipOrtam} {Λ : LineerOrtam} {Ρ : BolgeOrtam}
    {x : VarId} {e : Ifade} {τ : Tip} {Λ' : LineerOrtam} {Ρ' : BolgeOrtam}
    (h : Typed Γ Λ Ρ (Ifade.atama x e) τ Λ' Ρ') :
    ∃ τe Λe Ρe, Typed Γ Λ Ρ e τe Λe Ρe := by
  obtain ⟨ht, hl, hr⟩ := h
  match ht, hl, hr with
  | HasType.t_atama _ _ _ τx _ hte,
    LineerTamam.l_atama _ _ _ _ _ hle,
    RegionTamam.r_atama _ _ _ _ _ _ _ _ hre =>
    exact ⟨τx, Λ', Ρ', ⟨hte, hle, hre⟩⟩

/-- Guvensiz ic ifadesi tiplidir (sGuvensizCong tipleme tarafi). -/
theorem typed_guvensiz_ic {Γ : TipOrtam} {Λ : LineerOrtam} {Ρ : BolgeOrtam}
    {e : Ifade} {τ : Tip} {Λ' : LineerOrtam} {Ρ' : BolgeOrtam}
    (h : Typed Γ Λ Ρ (Ifade.guvensiz e) τ Λ' Ρ') :
    ∃ τe Λe Ρe, Typed Γ Λ Ρ e τe Λe Ρe := by
  obtain ⟨ht, hl, hr⟩ := h
  match ht, hl, hr with
  | HasType.t_guvensiz _ _ _ hte,
    LineerTamam.l_guvensiz _ _ _ _ hle,
    RegionTamam.r_guvensiz _ _ _ _ hre =>
    exact ⟨τ, Λ', Ρ', ⟨hte, hle, hre⟩⟩


-- ============================================================
-- §3. AtamaOdak — degerlendirme pozisyonundaki atama hedefleri (F2)
-- ============================================================

/-- `AtamaOdak e y`: e'nin degerlendirme pozisyonunda (seq-sol /
    atama-RHS / guvensiz-ic zinciri) hedefi y olan bir atama var.

    AtamaSahipligi invariant'i bu form uzerinden ifade edilir; boylece
    odaklama (congruence kurallarinin ic ifadeye inmesi) invariant'i
    bozmaz: `AtamaOdak a y → AtamaOdak (seq a b) y` vb. kapanis kurallari
    tam da odaklamanin ters yonu. -/
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
-- §4. ThreadTipliFull — Plan v2 §5.2.3
-- ============================================================

/-- Thread'lerin tip-uyumu (Plan §5.2.3).

    V1 form: tek paylasimli Λ + `ctx.lineer ↔ Λ` iff koprusu.
    F4'te per-thread Λ_ctx formuna gecilir (kopru silinir). -/
def ThreadTipliFull (Γ : TipOrtam) (Λ : LineerOrtam) (Ρ : BolgeOrtam)
                    (threads : List ThreadCtx) : Prop :=
  ∀ ctx ∈ threads,
    (∃ τ : Tip, ∃ Λ' : LineerOrtam, ∃ Ρ' : BolgeOrtam,
      Typed Γ Λ Ρ ctx.ifade τ Λ' Ρ')
    ∧ (∀ y : VarId, ∀ lin : Lineerlik,
        (y, lin) ∈ ctx.lineer ↔ lineerOrtamGet Λ y = some lin)


-- ============================================================
-- §5. KonfTipliFull — Plan v2 §5.2 ana merkez predikat
-- ============================================================

/-- Konfigurasyon tipli — Plan v2 §5'in merkezi predicate'i (F2 formu).

    Bilesenler:
    1. SigmaTipli (StoreTyped)
    2. ThreadTipliFull (Typed-tabanli)
    3. SahiplikTutarli (F2: zaman'siz)
    4. KanalTutarli
    5. S.fault = none
    6. S.bolge = Ρ
    7. FrozenKategoriTutarli (isFrozen ↔ kategori donmus)
    8. AtamaSahipligi (F2: AtamaOdak formu — odaklama altinda kapali) -/
def KonfTipliFull (Γ : TipOrtam) (Λ : LineerOrtam) (Ρ : BolgeOrtam)
                  (S : Konfigurasyon) : Prop :=
  SigmaTipli Γ Ρ S.store
  ∧ ThreadTipliFull Γ Λ Ρ S.thread
  ∧ SahiplikTutarli Ρ S.sahiplik
  ∧ KanalTutarli Γ Ρ S.kanal
  ∧ S.fault = none
  ∧ S.bolge = Ρ
  ∧ (∀ (x : VarId) (b : Bolge),
       bolgeOrtamGet S.bolge x = some b →
       (isFrozen S b ↔ b.kategori = BolgeKategorisi.donmus))
  ∧ (∀ ctx ∈ S.thread, ∀ y : VarId, AtamaOdak ctx.ifade y →
       ∀ b : Bolge, bolgeOrtamGet S.bolge y = some b →
         sahiplikGet S.sahiplik b = some (Sahip.thread ctx.tid))


-- ============================================================
-- §6. KonfTipliFull yapilandirma yardimlari
-- ============================================================

/-- KonfTipliFull yapilandirma yardimi (8-bilesen introduce). -/
theorem konfTipliFull_intro
    (Γ : TipOrtam) (Λ : LineerOrtam) (Ρ : BolgeOrtam) (S : Konfigurasyon)
    (h_store  : SigmaTipli Γ Ρ S.store)
    (h_thread : ThreadTipliFull Γ Λ Ρ S.thread)
    (h_sahip  : SahiplikTutarli Ρ S.sahiplik)
    (h_kanal  : KanalTutarli Γ Ρ S.kanal)
    (h_fault  : S.fault = none)
    (h_bolge_eq : S.bolge = Ρ)
    (h_frozen_kat : ∀ (x : VarId) (b : Bolge),
                      bolgeOrtamGet S.bolge x = some b →
                      (isFrozen S b ↔ b.kategori = BolgeKategorisi.donmus))
    (h_atama_sahip : ∀ ctx ∈ S.thread, ∀ y : VarId, AtamaOdak ctx.ifade y →
                       ∀ b : Bolge, bolgeOrtamGet S.bolge y = some b →
                         sahiplikGet S.sahiplik b = some (Sahip.thread ctx.tid)) :
    KonfTipliFull Γ Λ Ρ S :=
  ⟨h_store, h_thread, h_sahip, h_kanal, h_fault, h_bolge_eq, h_frozen_kat,
   h_atama_sahip⟩

/-- KonfTipliFull'den bilesenleri cikarma (8-bilesen projection). -/
theorem konfTipliFull_elim
    (Γ : TipOrtam) (Λ : LineerOrtam) (Ρ : BolgeOrtam) (S : Konfigurasyon)
    (h : KonfTipliFull Γ Λ Ρ S) :
    SigmaTipli Γ Ρ S.store
    ∧ ThreadTipliFull Γ Λ Ρ S.thread
    ∧ SahiplikTutarli Ρ S.sahiplik
    ∧ KanalTutarli Γ Ρ S.kanal
    ∧ S.fault = none
    ∧ S.bolge = Ρ
    ∧ (∀ (x : VarId) (b : Bolge),
         bolgeOrtamGet S.bolge x = some b →
         (isFrozen S b ↔ b.kategori = BolgeKategorisi.donmus))
    ∧ (∀ ctx ∈ S.thread, ∀ y : VarId, AtamaOdak ctx.ifade y →
         ∀ b : Bolge, bolgeOrtamGet S.bolge y = some b →
           sahiplikGet S.sahiplik b = some (Sahip.thread ctx.tid)) :=
  h

-- ============================================================
-- §7. Odaklama lemmasi (F2) — congruence case'lerinin anahtari
-- ============================================================

/-- KonfTipliFull odaklama altinda kapali: odaktaki thread'in ifadesi,
    tipli bir alt-ifadeye indirildiginde (seq-sol / atama-RHS / guvensiz-ic)
    konfigurasyon tipli kalir.

    h_typed_e: alt-ifadenin tipliligi (cagiran, tersine-cevirme lemmasiyla
    saglar — typed_seq_sol / typed_atama_ic / typed_guvensiz_ic).
    h_odak_kapali: alt-ifadedeki atama-odaklari ana ifadede de odakta
    (cagiran, AtamaOdak kapanis constructor'iyla saglar). -/
theorem konfTipliFull_odak
    (Γ : TipOrtam) (Λ : LineerOrtam) (Ρ : BolgeOrtam)
    (S : Konfigurasyon) (ts1 ts2 : List ThreadCtx) (ctx : ThreadCtx)
    (e : Ifade)
    (h_konf : KonfTipliFull Γ Λ Ρ S)
    (h_t : S.thread = ts1 ++ ctx :: ts2)
    (h_typed_e : ∃ τ Λ' Ρ', Typed Γ Λ Ρ e τ Λ' Ρ')
    (h_odak_kapali : ∀ y : VarId, AtamaOdak e y → AtamaOdak ctx.ifade y) :
    KonfTipliFull Γ Λ Ρ
      { S with thread := ts1 ++ { ctx with ifade := e } :: ts2 } := by
  obtain ⟨h_sigma, h_thread, h_sahip, h_kanal, h_fault, h_beq, h_fkat,
          h_asahip⟩ := h_konf
  have h_ctx_in : ctx ∈ S.thread := by
    rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
  refine ⟨h_sigma, ?_, h_sahip, h_kanal, h_fault, h_beq, h_fkat, ?_⟩
  · -- ThreadTipliFull (guncellenmis liste)
    intro ctx'' h_mem
    rcases List.mem_append.mp h_mem with h1 | h2
    · have h_in : ctx'' ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inl h1)
      exact h_thread ctx'' h_in
    · rcases List.mem_cons.mp h2 with h_eq | h3
      · subst h_eq
        exact ⟨h_typed_e, (h_thread ctx h_ctx_in).2⟩
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
