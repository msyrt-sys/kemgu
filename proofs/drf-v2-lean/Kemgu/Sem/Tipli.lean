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

/-- D-332: `eger`in KOSULU tiplidir (sEgerCong tipleme tarafi). -/
theorem typed_eger_kosul {Γ : TipOrtam} {Δ : KanalOrtam} {Λ : LineerOrtam}
    {Ρ : BolgeOrtam} {k d y : Ifade} {τ : Tip} {Λ' : LineerOrtam} {Ρ' : BolgeOrtam}
    (h : Typed Γ Δ Λ Ρ (Ifade.eger k d y) τ Λ' Ρ') :
    ∃ τk Λk Ρk, Typed Γ Δ Λ Ρ k τk Λk Ρk := by
  obtain ⟨ht, hl, hr⟩ := h
  match ht, hl, hr with
  | HasType.t_eger _ _ _ _ _ τk _ htk _ _,
    LineerTamam.l_eger _ _ Λk _ _ _ hlk _ _,
    RegionTamam.r_eger _ _ Ρk _ _ _ hrk _ _ _ _ =>
    exact ⟨τk, Λk, Ρk, ⟨htk, hlk, hrk⟩⟩

/-- D-332: `eger`in SECILEN DALI tiplidir — kosul deger oldugunda
    (sEgerSec) adim sonrasi thread ifadesinin tiplenmesini verir.
    Cikis ortamlari dal secimine BAGIMSIZ (Λk / Ρk): l_eger dallari
    lineer-notr, r_eger dallari bolge-notr kilar. -/
theorem typed_eger_dal {Γ : TipOrtam} {Δ : KanalOrtam} {Λ : LineerOrtam}
    {Ρ : BolgeOrtam} {v : Deger} {d y : Ifade} {τ : Tip}
    {Λ' : LineerOrtam} {Ρ' : BolgeOrtam}
    (h : Typed Γ Δ Λ Ρ (Ifade.eger (Ifade.sabit v) d y) τ Λ' Ρ')
    (alindi : Bool) :
    Typed Γ Δ Λ Ρ (if alindi then d else y) τ Λ' Ρ' := by
  obtain ⟨ht, hl, hr⟩ := h
  match ht, hl, hr with
  | HasType.t_eger _ _ _ _ _ _ _ _ htd hty,
    LineerTamam.l_eger _ _ _ _ _ _ hlk h_nd h_ny,
    RegionTamam.r_eger _ _ _ _ _ _ hrk _ _ hrd hry =>
    -- kosul `sabit v`: l_sabit / r_sabit → Λk = Λ, Ρk = Ρ
    cases hlk
    cases hrk
    cases alindi with
    | true  => exact ⟨htd, lineerNotr_kimlik h_nd _, hrd⟩
    | false => exact ⟨hty, lineerNotr_kimlik h_ny _, hry⟩

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
    RegionTamam.r_atama _ _ _ _ _ _ hre _ _ =>
    exact ⟨τx, Λ', Ρ', ⟨hte, hle, hre⟩⟩

/-- Seq-atla tiplemesi: sol taraf DEGER ise sag taraf AYNI giris-ortamindan
    ve AYNI cikti-ortamlarina tiplidir (l_sabit/r_sabit identity —
    sSeqAtla'nin adim_korunum comp-2 taniki). -/
theorem typed_seq_atla {Γ : TipOrtam} {Δ : KanalOrtam} {Λ : LineerOrtam}
    {Ρ : BolgeOrtam} {v : Deger} {b : Ifade} {τ : Tip}
    {Λ' : LineerOrtam} {Ρ' : BolgeOrtam}
    (h : Typed Γ Δ Λ Ρ (Ifade.seq (Ifade.sabit v) b) τ Λ' Ρ') :
    Typed Γ Δ Λ Ρ b τ Λ' Ρ' := by
  obtain ⟨ht, hl, hr⟩ := h
  match ht, hl, hr with
  | HasType.t_seq _ _ _ _ _ _ hta htb,
    LineerTamam.l_seq _ _ Λa _ _ _ hla hlb,
    RegionTamam.r_seq _ _ Ρa _ _ _ hra hrb =>
    match hla, hra with
    | LineerTamam.l_sabit _ _ _, RegionTamam.r_sabit _ _ _ =>
      exact ⟨htb, hlb, hrb⟩

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
-- §3. HedefVar / HedefBolge — Onarim v3 kapanisinda Core.lean §9.2'ye
-- TASINDI (r_gorev_baslat'in Yol-B hedefsiz-govde premise'i icin
-- RegionTamam katmaninin gorebildigi yere). Tanimlar degismedi.


-- ============================================================
-- §3.2. OdakYuk — odak-adim yuku (Onarim v3 kapanis, cong-kapanisi)
-- ============================================================

/-- ODAK-ADIM YUKU: bir adimin odakli thread-cifti (ctx → ctx') icin
    tasidigi uc taahhut — cong-sarmalamasinin devam-ifadesi icin
    ihtiyac duydugu HER SEY:
    (E) odak gercekten degisti (cerrah_kilit ile pozisyon-pinleme);
    (A) on-ifadenin HERHANGI tiplenmesi, ayni τ ile, art-duruma tasinir
        — lineer cikti ≼-iliskili (runtime daha az tuketir), bolge
        ciktisi BolgeIliski ile (yazilabilir-aynen + id koruma);
    (B) art-ortamda yazilabilir-kayitli her bolge on-ortamda aynen
        kayitliydi VE odak-thread sahipligi korunur. -/
def OdakYuk (Γ : TipOrtam) (Δ : KanalOrtam) (S S' : Konfigurasyon)
    (ctx ctx' : ThreadCtx) : Prop :=
  ctx' ≠ ctx
  ∧ (∀ τ Λout Ρout, Typed Γ Δ ctx.lineer S.bolge ctx.ifade τ Λout Ρout →
       ∃ Λout' Ρout', Typed Γ Δ ctx'.lineer S'.bolge ctx'.ifade τ Λout' Ρout'
         ∧ LineerKucuk Λout' Λout ∧ BolgeIliski Ρout' Ρout)
  ∧ (∀ y bb, bolgeOrtamGet S'.bolge y = some bb →
       kategoriYazilabilir bb.kategori = true →
       bolgeOrtamGet S.bolge y = some bb
       ∧ (sahiplikGet S.sahiplik bb = some (Sahip.thread ctx.tid) →
          sahiplikGet S'.sahiplik bb = some (Sahip.thread ctx.tid)))


-- ============================================================
-- §3.5. TidAyrik — thread kimlik tekilligi (F4-kapanis, on-onayli analog)
-- ============================================================

/-- Thread kimlikleri konum-bazli ayrik (Pairwise — ayni tid iki konumda
    olamaz; cGorevBirlestir'in bitmis-thread tekillik argumani + spawn/
    dondur sahiplik-catismasi argumanlarinin temeli). -/
def TidAyrik (threads : List ThreadCtx) : Prop :=
  threads.Pairwise (fun a b => a.tid ≠ b.tid)

/-- TidAyrik, odakli thread'in ayni-tid'li degisimi altinda korunur. -/
theorem tidAyrik_degisim {ts1 ts2 : List ThreadCtx} {ctx : ThreadCtx}
    (h : TidAyrik (ts1 ++ ctx :: ts2))
    (ctx' : ThreadCtx) (h_tid : ctx'.tid = ctx.tid) :
    TidAyrik (ts1 ++ ctx' :: ts2) := by
  unfold TidAyrik at h ⊢
  induction ts1 with
  | nil =>
      rw [List.nil_append] at h ⊢
      rw [List.pairwise_cons] at h ⊢
      obtain ⟨h_all, h_rest⟩ := h
      exact ⟨fun b hb => by rw [h_tid]; exact h_all b hb, h_rest⟩
  | cons a ts1' ih =>
      rw [List.cons_append] at h ⊢
      rw [List.pairwise_cons] at h ⊢
      obtain ⟨h_all, h_rest⟩ := h
      refine ⟨?_, ih h_rest⟩
      intro b hb
      rcases List.mem_append.mp hb with h1 | h2
      · exact h_all b (List.mem_append.mpr (Or.inl h1))
      · rcases List.mem_cons.mp h2 with h_eq | h3
        · subst h_eq
          rw [h_tid]
          exact h_all ctx (List.mem_append.mpr (Or.inr (List.Mem.head _)))
        · exact h_all b (List.mem_append.mpr (Or.inr (List.Mem.tail _ h3)))

/-- TidAyrik altinda tid, uyeyi belirler. -/
theorem tidAyrik_tekil {l : List ThreadCtx} (h : TidAyrik l)
    {c1 c2 : ThreadCtx} (h1 : c1 ∈ l) (h2 : c2 ∈ l)
    (h_eq : c1.tid = c2.tid) : c1 = c2 := by
  induction l with
  | nil => cases h1
  | cons a rest ih =>
      unfold TidAyrik at h
      rw [List.pairwise_cons] at h
      obtain ⟨h_all, h_rest⟩ := h
      rcases List.mem_cons.mp h1 with e1 | m1
      · rcases List.mem_cons.mp h2 with e2 | m2
        · subst e1; exact e2.symm
        · subst e1; exact absurd h_eq (h_all c2 m2)
      · rcases List.mem_cons.mp h2 with e2 | m2
        · subst e2; exact absurd h_eq.symm (h_all c1 m1)
        · exact ih h_rest m1 m2

/-- Odak-disi uye, odakli thread'le ayni tid'i tasiyamaz. -/
theorem tidAyrik_odakdisi {ts1 ts2 : List ThreadCtx} {ctx : ThreadCtx}
    (h : TidAyrik (ts1 ++ ctx :: ts2))
    {c : ThreadCtx} (hc : c ∈ ts1 ∨ c ∈ ts2) :
    c.tid ≠ ctx.tid := by
  induction ts1 with
  | nil =>
      rcases hc with h1 | h2
      · cases h1
      · unfold TidAyrik at h
        rw [List.nil_append, List.pairwise_cons] at h
        exact fun he => h.1 c h2 he.symm
  | cons a ts1' ih =>
      unfold TidAyrik at h
      rw [List.cons_append, List.pairwise_cons] at h
      obtain ⟨h_all, h_rest⟩ := h
      rcases hc with h1 | h2
      · rcases List.mem_cons.mp h1 with e | h3
        · subst e
          exact h_all ctx (List.mem_append.mpr (Or.inr (List.Mem.head _)))
        · exact ih h_rest (Or.inl h3)
      · exact ih h_rest (Or.inr h2)

/-- TidAyrik, taze-tid'li tek eleman eklenmesi altinda korunur (spawn). -/
theorem tidAyrik_ekle {l : List ThreadCtx} (h : TidAyrik l)
    {x : ThreadCtx} (h_fresh : ∀ a ∈ l, a.tid ≠ x.tid) :
    TidAyrik (l ++ [x]) := by
  induction l with
  | nil =>
      exact List.Pairwise.cons (fun b hb => nomatch hb) List.Pairwise.nil
  | cons a rest ih =>
      unfold TidAyrik at h ⊢
      rw [List.cons_append, List.pairwise_cons]
      rw [List.pairwise_cons] at h
      obtain ⟨h_all, h_rest⟩ := h
      refine ⟨?_, ih h_rest (fun b hb => h_fresh b (List.Mem.tail _ hb))⟩
      intro b hb
      rcases List.mem_append.mp hb with h1 | h2
      · exact h_all b h1
      · rcases List.mem_cons.mp h2 with e | h3
        · subst e; exact h_fresh a (List.Mem.head _)
        · cases h3


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
  -- 12. BolgeAyrik (on-onayli): bolge-id, kayitli degiskeni belirler —
  -- store-golgelemesi (sAtama comp-10) ve id-takma-ad argumanlarinin temeli.
  ∧ (∀ (x1 x2 : VarId) (b1 b2 : Bolge),
       bolgeOrtamGet S.bolge x1 = some b1 →
       bolgeOrtamGet S.bolge x2 = some b2 →
       b1.id = b2.id → x1 = x2)
  -- 13. TidAyrik (on-onayli analog): thread kimlikleri konum-bazli ayrik.
  ∧ TidAyrik S.thread
  -- 14. KanalKapasite1 (Mehmet karari): kuyruk uzunlugu ≤ 1 —
  -- cKanalAl pop sonrasi transit-tanigi sorununu kapatir; buffer V2.
  ∧ (∀ kd ∈ S.kanal, kd.gonderKuyrugu.length ≤ 1)
  -- 15. KanalAyrik: kanal kaydi kid'le belirlenir (kanalEkle yeni kayit
  -- yalniz kayit-yokken acar; uniform map-guncellemeler korur).
  ∧ (∀ kd1 ∈ S.kanal, ∀ kd2 ∈ S.kanal, kd1.kid = kd2.kid → kd1 = kd2)


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
          h_hvar, h_hbolge, h_bagli, h_transit, h_bayrik, h_tayrik,
          h_kap, h_kayrik⟩ := h_konf
  have h_ctx_in : ctx ∈ S.thread := by
    rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
  have h_tayrik' : TidAyrik (ts1 ++ { ctx with ifade := e } :: ts2) := by
    rw [h_t] at h_tayrik
    exact tidAyrik_degisim h_tayrik _ rfl
  refine ⟨h_sigma, ?_, h_sahip, h_kanal, h_fault, h_beq, h_fkat,
          ?_, ?_, h_bagli, h_transit, h_bayrik, h_tayrik', h_kap, h_kayrik⟩
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

-- ============================================================
-- §7. Odakli-thread DEGISIM yardimcilari (F4-ispat)
-- Bolge/sahiplik DEGISMEYEN adimlar icin: liste cerrahisinde odakli
-- thread degisince bilesenlerin tasinmasi.
-- ============================================================

/-- ThreadTipliFull, odakli thread'in (tipli) bir baskasiyla
    degistirilmesi altinda korunur (ayni Ρ). -/
theorem threadTipli_degisim
    {Γ : TipOrtam} {Δ : KanalOrtam} {Ρ : BolgeOrtam}
    {ts1 ts2 : List ThreadCtx} {ctx : ThreadCtx}
    (h : ThreadTipliFull Γ Δ Ρ (ts1 ++ ctx :: ts2))
    (ctx' : ThreadCtx)
    (h' : ∃ τ Λ' Ρ', Typed Γ Δ ctx'.lineer Ρ ctx'.ifade τ Λ' Ρ') :
    ThreadTipliFull Γ Δ Ρ (ts1 ++ ctx' :: ts2) := by
  intro c h_mem
  rcases List.mem_append.mp h_mem with h1 | h2
  · exact h c (List.mem_append.mpr (Or.inl h1))
  · rcases List.mem_cons.mp h2 with h_eq | h3
    · subst h_eq; exact h'
    · exact h c (List.mem_append.mpr (Or.inr (List.Mem.tail _ h3)))

/-- HedefVar-sahipligi (KonfTipliFull 8. bilesen govdesi), odakli thread'in
    hedef-buyutmeyen + tid-koruyan degisimi altinda korunur
    (ayni bolge-ortami `bOrt` ve sahiplik `sah` uzerinde). -/
theorem hedefVarSahip_degisim
    {bOrt : BolgeOrtam} {sah : Sahiplik}
    {ts1 ts2 : List ThreadCtx} {ctx : ThreadCtx}
    (h : ∀ c ∈ ts1 ++ ctx :: ts2, ∀ y : VarId, HedefVar c.ifade y →
           ∀ b : Bolge, bolgeOrtamGet bOrt y = some b →
             kategoriYazilabilir b.kategori = true →
             sahiplikGet sah b = some (Sahip.thread c.tid))
    (ctx' : ThreadCtx)
    (h_tid : ctx'.tid = ctx.tid)
    (h_hv : ∀ y : VarId, HedefVar ctx'.ifade y → HedefVar ctx.ifade y) :
    ∀ c ∈ ts1 ++ ctx' :: ts2, ∀ y : VarId, HedefVar c.ifade y →
      ∀ b : Bolge, bolgeOrtamGet bOrt y = some b →
        kategoriYazilabilir b.kategori = true →
        sahiplikGet sah b = some (Sahip.thread c.tid) := by
  intro c h_mem y h_y b h_b h_yaz
  rcases List.mem_append.mp h_mem with h1 | h2
  · exact h c (List.mem_append.mpr (Or.inl h1)) y h_y b h_b h_yaz
  · rcases List.mem_cons.mp h2 with h_eq | h3
    · subst h_eq
      rw [h_tid]
      exact h ctx (List.mem_append.mpr (Or.inr (List.Mem.head _)))
        y (h_hv y h_y) b h_b h_yaz
    · exact h c (List.mem_append.mpr (Or.inr (List.Mem.tail _ h3)))
        y h_y b h_b h_yaz

/-- HedefBolge-sahipligi (9. bilesen) — ayni desen. -/
theorem hedefBolgeSahip_degisim
    {bOrt : BolgeOrtam} {sah : Sahiplik}
    {ts1 ts2 : List ThreadCtx} {ctx : ThreadCtx}
    (h : ∀ c ∈ ts1 ++ ctx :: ts2, ∀ b : Bolge, HedefBolge c.ifade b →
           (∃ x, bolgeOrtamGet bOrt x = some b) →
           kategoriYazilabilir b.kategori = true →
           sahiplikGet sah b = some (Sahip.thread c.tid))
    (ctx' : ThreadCtx)
    (h_tid : ctx'.tid = ctx.tid)
    (h_hb : ∀ b : Bolge, HedefBolge ctx'.ifade b → HedefBolge ctx.ifade b) :
    ∀ c ∈ ts1 ++ ctx' :: ts2, ∀ b : Bolge, HedefBolge c.ifade b →
      (∃ x, bolgeOrtamGet bOrt x = some b) →
      kategoriYazilabilir b.kategori = true →
      sahiplikGet sah b = some (Sahip.thread c.tid) := by
  intro c h_mem b h_b h_kayit h_yaz
  rcases List.mem_append.mp h_mem with h1 | h2
  · exact h c (List.mem_append.mpr (Or.inl h1)) b h_b h_kayit h_yaz
  · rcases List.mem_cons.mp h2 with h_eq | h3
    · subst h_eq
      rw [h_tid]
      exact h ctx (List.mem_append.mpr (Or.inr (List.Mem.head _)))
        b (h_hb b h_b) h_kayit h_yaz
    · exact h c (List.mem_append.mpr (Or.inr (List.Mem.tail _ h3)))
        b h_b h_kayit h_yaz

end Kemgu.Sem.Tipli
