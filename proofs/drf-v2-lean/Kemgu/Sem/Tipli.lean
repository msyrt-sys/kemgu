/-
KEMGU DRF Mekanize — Tipli Katmani (Onarim v3 F1-F4)
Kaynak: ADIM0_DENETIM_RAPORU.md + FAZ_BRIFINGLERI.md F4 (onayli invariant)
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz, sorry/axiom YOK

F4-ispat guncellemeleri (onayli invariant tasarimi, 2026-06-11):
- AtamaOdak yerine HEDEFVAR / HEDEFBOLGE: sahiplik gerektiren TUM
  operasyon hedefleri (atama / kanal_gonder / gorev yakalama; dondur
  bolge-literali), ifadenin HER YERINDE (seq sag-sol, atama-RHS,
  guvensiz-ic; gorevBaslat govdesi HARIC — cocugun isi). Boylece
  sSeqAtla'da yeni odaga giren hedefler de onceden kapsanir.
- kategoriYazilabilir muafiyeti: sahip/kanalRho/donmus kategorili
  bolgeler icin sahiplik talep edilmez (transfer kurallari kategoriyi
  ES-ZAMANLI transfer-disi yapar — invariant adim altinda korunabilir).
- KanalTransit (11. bilesen): dolu kuyrugu olan kanalin transit'te
  bolgesi var (cKanalAlTamam progress taniki).
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
-- §3. HedefVar / HedefBolge — sahiplik gerektiren hedefler (F4 onayli)
-- ============================================================

/-- `HedefVar e y`: e'nin govdesinde (gorevBaslat ic-govdeleri HARIC —
    onlar cocuk thread'in hedefleri) y degiskenini hedefleyen sahiplik
    gerektiren bir operasyon var: atama hedefi, kanala gonderim,
    gorev yakalamasi. seq'in HER IKI kolu kapsanir (sSeqAtla'da saga
    gecis invariant'i bozmasin). -/
inductive HedefVar : Ifade → VarId → Prop where
  | atama_bas (y : VarId) (e : Ifade) :
      HedefVar (Ifade.atama y e) y
  | kanal_gonder (k : KanalId) (y : VarId) :
      HedefVar (Ifade.kanalGonderIf k y) y
  | gorev_yakala (yd : List VarId) (kod : Ifade) (y : VarId) :
      y ∈ yd → HedefVar (Ifade.gorevBaslat yd kod) y
  | seq_sol (a b : Ifade) (y : VarId) :
      HedefVar a y → HedefVar (Ifade.seq a b) y
  | seq_sag (a b : Ifade) (y : VarId) :
      HedefVar b y → HedefVar (Ifade.seq a b) y
  | atama_ic (x : VarId) (e : Ifade) (y : VarId) :
      HedefVar e y → HedefVar (Ifade.atama x e) y
  | guvensiz_ic (e : Ifade) (y : VarId) :
      HedefVar e y → HedefVar (Ifade.guvensiz e) y

/-- `HedefBolge e b`: e'nin govdesinde b bolge-literalini donduran bir
    dondurIf var (dondur sahiplik gerektirir — h_owner). -/
inductive HedefBolge : Ifade → Bolge → Prop where
  | dondur_bas (b : Bolge) :
      HedefBolge (Ifade.dondurIf b) b
  | seq_sol (a c : Ifade) (b : Bolge) :
      HedefBolge a b → HedefBolge (Ifade.seq a c) b
  | seq_sag (a c : Ifade) (b : Bolge) :
      HedefBolge c b → HedefBolge (Ifade.seq a c) b
  | atama_ic (x : VarId) (e : Ifade) (b : Bolge) :
      HedefBolge e b → HedefBolge (Ifade.atama x e) b
  | guvensiz_ic (e : Ifade) (b : Bolge) :
      HedefBolge e b → HedefBolge (Ifade.guvensiz e) b


-- ============================================================
-- §4. ThreadTipliFull — per-thread lineer (F4)
-- ============================================================

/-- Thread'lerin tip-uyumu (per-thread lineer ortam). -/
def ThreadTipliFull (Γ : TipOrtam) (Δ : KanalOrtam)
                    (Ρ : BolgeOrtam) (threads : List ThreadCtx) : Prop :=
  ∀ ctx ∈ threads,
    ∃ τ : Tip, ∃ Λ' : LineerOrtam, ∃ Ρ' : BolgeOrtam,
      Typed Γ Δ ctx.lineer Ρ ctx.ifade τ Λ' Ρ'


-- ============================================================
-- §5. KonfTipliFull — 11 bilesen (F4 onayli invariant formu)
-- ============================================================

/-- Konfigurasyon tipli (F4 formu — 11 bilesen):
    1. SigmaTipli  2. ThreadTipliFull  3. SahiplikTutarli  4. KanalTutarli(Δ)
    5. fault=none  6. S.bolge = Ρ  7. FrozenKategoriTutarli
    8. HedefVarSahipligi: thread, yazilabilir-kategorili degisken-hedef
       bolgelerinin GUNCEL sahibi (onayli invariant — atama/kanal/yakalama)
    9. HedefBolgeSahipligi: ayni, dondur bolge-literalleri icin
    10. DegiskenlerBagli  11. KanalTransit (dolu kuyruk → transit bolge). -/
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
  ∧ (∀ ctx ∈ S.thread, ∀ y : VarId, HedefVar ctx.ifade y →
       ∀ b : Bolge, bolgeOrtamGet S.bolge y = some b →
         kategoriYazilabilir b.kategori = true →
         sahiplikGet S.sahiplik b = some (Sahip.thread ctx.tid))
  ∧ (∀ ctx ∈ S.thread, ∀ b : Bolge, HedefBolge ctx.ifade b →
       (∃ x, bolgeOrtamGet S.bolge x = some b) →
       kategoriYazilabilir b.kategori = true →
       sahiplikGet S.sahiplik b = some (Sahip.thread ctx.tid))
  ∧ (∀ (x : VarId) (τ : Tip), tipOrtamGet Γ x = some τ →
       ∃ (b : Bolge) (v : Deger),
         bolgeOrtamGet S.bolge x = some b
         ∧ konumGet S.store ⟨b, 0⟩ = some v
         ∧ DegerTipli Γ Ρ v τ)
  ∧ (∀ kd ∈ S.kanal, kd.gonderKuyrugu ≠ [] →
       ∃ b : Bolge, sahiplikGet S.sahiplik b = some (Sahip.kanalSahip kd.kid))


-- ============================================================
-- §6. Odaklama lemmasi — congruence case'lerinin anahtari
-- ============================================================

/-- KonfTipliFull odaklama altinda kapali (11 bilesen; yeni hedef-
    predikatlari odaklamanin TERS yonunde kapanis constructor'larina
    sahip — cagiran saglar). -/
theorem konfTipliFull_odak
    (Γ : TipOrtam) (Δ : KanalOrtam) (Ρ : BolgeOrtam)
    (S : Konfigurasyon) (ts1 ts2 : List ThreadCtx) (ctx : ThreadCtx)
    (e : Ifade)
    (h_konf : KonfTipliFull Γ Δ Ρ S)
    (h_t : S.thread = ts1 ++ ctx :: ts2)
    (h_typed_e : ∃ τ Λ' Ρ', Typed Γ Δ ctx.lineer Ρ e τ Λ' Ρ')
    (h_hv_kapali : ∀ y : VarId, HedefVar e y → HedefVar ctx.ifade y)
    (h_hb_kapali : ∀ b : Bolge, HedefBolge e b → HedefBolge ctx.ifade b) :
    KonfTipliFull Γ Δ Ρ
      { S with thread := ts1 ++ { ctx with ifade := e } :: ts2 } := by
  obtain ⟨h_sigma, h_thread, h_sahip, h_kanal, h_fault, h_beq, h_fkat,
          h_hvar, h_hbolge, h_bagli, h_transit⟩ := h_konf
  have h_ctx_in : ctx ∈ S.thread := by
    rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
  refine ⟨h_sigma, ?_, h_sahip, h_kanal, h_fault, h_beq, h_fkat,
          ?_, ?_, h_bagli, h_transit⟩
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
  · -- HedefVarSahipligi (guncellenmis liste)
    intro ctx'' h_mem y h_hv b h_bb h_yaz
    rcases List.mem_append.mp h_mem with h1 | h2
    · have h_in : ctx'' ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inl h1)
      exact h_hvar ctx'' h_in y h_hv b h_bb h_yaz
    · rcases List.mem_cons.mp h2 with h_eq | h3
      · subst h_eq
        exact h_hvar ctx h_ctx_in y (h_hv_kapali y h_hv) b h_bb h_yaz
      · have h_in : ctx'' ∈ S.thread := by
          rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.tail _ h3))
        exact h_hvar ctx'' h_in y h_hv b h_bb h_yaz
  · -- HedefBolgeSahipligi (guncellenmis liste)
    intro ctx'' h_mem b h_hb h_kayitli h_yaz
    rcases List.mem_append.mp h_mem with h1 | h2
    · have h_in : ctx'' ∈ S.thread := by
        rw [h_t]; exact List.mem_append.mpr (Or.inl h1)
      exact h_hbolge ctx'' h_in b h_hb h_kayitli h_yaz
    · rcases List.mem_cons.mp h2 with h_eq | h3
      · subst h_eq
        exact h_hbolge ctx h_ctx_in b (h_hb_kapali b h_hb) h_kayitli h_yaz
      · have h_in : ctx'' ∈ S.thread := by
          rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.tail _ h3))
        exact h_hbolge ctx'' h_in b h_hb h_kayitli h_yaz

end Kemgu.Sem.Tipli
