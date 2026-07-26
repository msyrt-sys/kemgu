/-
KEMGU DRF Mekanize — Meta Katmani: Progress (Onarim v3 F5)
Wright-Felleisen: TAPL §8.3.2 (Progress) — KEMGU concurrency adaptasyonu
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz

F5: progress_konf — AMBIENT-Γ konfigurasyon-seviyesi progress
(eski kapali-Γ formu yalniz degiskensiz programlari kapsiyordu — ADIM 0).
Uc-disjunct form: IsValue ∨ Engelli (bloklu: bos-kanal alimi / bitmemis
gorev birlestirme) ∨ odakli-adim. Guard tanikleri KonfTipliFull
bilesenlerinden: DegiskenlerBagli → konum/deger, HedefVar/HedefBolge
Sahipligi → h_owner, KanalTransit → transit bolge, tazeTid → h_fresh.
-/

import Kemgu.Sem.Core
import Kemgu.Sem.SmallStep
import Kemgu.Sem.StateTipli
import Kemgu.Sem.HasType
import Kemgu.Sem.LineerTamam
import Kemgu.Sem.RegionTamam
import Kemgu.Sem.Tipli

namespace Kemgu.Meta.ProgressKorunum
open Kemgu.Sem.Core Kemgu.Sem.SmallStep Kemgu.Sem.StateTipli
     Kemgu.Sem.HasType Kemgu.Sem.LineerTamam Kemgu.Sem.RegionTamam
     Kemgu.Sem.Tipli

-- ============================================================
-- §1. IsValue + Engelli (bloklu) — concurrency-progress disjunct'lari
-- ============================================================

/-- Bir ifade `e` bir DEGER ise IsValue e (yalniz literal). -/
inductive IsValue : Ifade → Prop where
  | iv_sabit (v : Deger) : IsValue (Ifade.sabit v)

/-- Engelli: deger degil, adim da atamaz ama STUCK sayilmaz —
    karsi taraf gelince acilir. Iki taban: bos kanaldan alim,
    bitmemis gorev birlestirme. Degerlendirme baglamlarinda kapanir. -/
inductive Engelli (S : Konfigurasyon) : Ifade → Prop where
  | bas (k : KanalId) :
      kanalIlk S.kanal k = none → Engelli S (Ifade.kanalAlIf k)
  | birlestir (g : VarId) :
      (∀ hctx ∈ S.thread, ∀ v : Deger, hctx.ifade ≠ Ifade.sabit v) →
      Engelli S (Ifade.gorevBirlestir g)
  | gonder_dolu (k : KanalId) (v : VarId) :
      kanalIlk S.kanal k ≠ none →
      Engelli S (Ifade.kanalGonderIf k v)
  | seq_sol (a b : Ifade) :
      Engelli S a → Engelli S (Ifade.seq a b)
  | atama_ic (x : VarId) (e : Ifade) :
      Engelli S e → Engelli S (Ifade.atama x e)
  | guvensiz_ic (e : Ifade) :
      Engelli S e → Engelli S (Ifade.guvensiz e)
  -- D-332: kosul engelliyse `eger` de engellidir (dallar degerlendirme
  -- baglami DEGILDIR — yalniz kosul odaga girer).
  | eger_kosul (k d y : Ifade) :
      Engelli S k → Engelli S (Ifade.eger k d y)

/-- Engelli'nin odakli konfigurasyondan ana konfigurasyona transferi:
    kanal ayni; thread listesi yalniz odakli ifadede farkli — odaktaki
    ana-ifade deger olmadigindan birlestir-bloklanmasi da tasinir. -/
theorem engelli_konf_transfer
    (S : Konfigurasyon) (ts1 ts2 : List ThreadCtx) (ctx : ThreadCtx)
    (eFok e : Ifade)
    (h : Engelli (ifadeyleKonf S ts1 ts2 ctx eFok) e)
    (h_t : S.thread = ts1 ++ ctx :: ts2)
    (h_nv : ∀ v : Deger, ctx.ifade ≠ Ifade.sabit v) :
    Engelli S e := by
  induction h with
  | bas k h_q =>
      exact Engelli.bas k (by simpa [ifadeyleKonf] using h_q)
  | birlestir g h_yok =>
      refine Engelli.birlestir g ?_
      intro hctx h_in v h_eq
      rw [h_t] at h_in
      rcases List.mem_append.mp h_in with h1 | h2
      · exact h_yok hctx (by simp [ifadeyleKonf]; exact Or.inl h1) v h_eq
      · rcases List.mem_cons.mp h2 with h_eq2 | h3
        · subst h_eq2
          exact h_nv v h_eq
        · exact h_yok hctx
            (by simp [ifadeyleKonf]; exact Or.inr (Or.inr h3)) v h_eq
  | gonder_dolu k v h_dolu =>
      exact Engelli.gonder_dolu k v (by simpa [ifadeyleKonf] using h_dolu)
  | seq_sol a b _ ih => exact Engelli.seq_sol a b ih
  | atama_ic x e _ ih => exact Engelli.atama_ic x e ih
  | guvensiz_ic e _ ih => exact Engelli.guvensiz_ic e ih
  | eger_kosul k d y _ ih => exact Engelli.eger_kosul k d y ih


-- ============================================================
-- §2. PROGRESS_KONF (F5) — ambient-Γ, uc-disjunct, odakli-adim formu
-- ============================================================

/-- PROGRESS (KEMGU konfigurasyon formu — F5):
    Tipli konfigurasyonda odaklanan her thread'in ifadesi ya DEGERDIR,
    ya ENGELLIDIR (bos-kanal alimi / bitmemis birlestirme), ya da
    odakli bir adim atabilir (sonuc thread'i ayni konumda, ayni tid —
    congruence sarmalama icin gerekli sekil). -/
theorem progress_konf
    (Γ : TipOrtam) (Δ : KanalOrtam) (Ρ : BolgeOrtam)
    (e : Ifade) (τ : Tip) (Λin Λ' : LineerOrtam) (Ρ' : BolgeOrtam)
    (h_ht : HasType Γ Δ e τ)
    (h_lt : LineerTamam Γ Λin e Λ')
    (h_rt : RegionTamam Γ Ρ e Ρ')
    (S : Konfigurasyon)
    (h_konf : KonfTipliFull Γ Δ Ρ S)
    (ts1 ts2 : List ThreadCtx) (ctx : ThreadCtx)
    (h_t : S.thread = ts1 ++ ctx :: ts2)
    (h_if : ctx.ifade = e)
    (h_lin : ctx.lineer = Λin) :
    IsValue e ∨ Engelli S e
    ∨ (∃ S' ctx' ts2', Step S S' ∧ S'.thread = ts1 ++ ctx' :: ts2'
         ∧ ctx'.tid = ctx.tid
         ∧ (ts2' = ts2 ∨ ∃ y, ts2' = ts2 ++ [y])) := by
  induction e generalizing S ts1 ts2 ctx τ Λin Λ' Ρ' with
  | tanim x =>
      obtain ⟨_, _, _, _, _, _, _, _, _, h_bagli, _, _, _, _, _⟩ := h_konf
      match h_ht with
      | HasType.t_tanim _ _ _ _ h_get =>
        obtain ⟨b, v, h_b, h_v, _⟩ := h_bagli x τ h_get
        exact Or.inr (Or.inr ⟨_, { ctx with ifade := .sabit v }, ts2,
          Step.sVarOku S _ ts1 ts2 ctx x b v h_t h_if h_b h_v rfl, rfl, rfl,
          Or.inl rfl⟩)
  | sabit v =>
      exact Or.inl (IsValue.iv_sabit v)
  | atama x e ih_e =>
      match h_ht, h_lt, h_rt with
      | HasType.t_atama _ _ _ _ τx h_gx h_te,
        LineerTamam.l_atama _ _ _ _ _ h_le,
        RegionTamam.r_atama _ _ _ _ _ b h_re h_gb h_yaz =>
        have h_ctx_in : ctx ∈ S.thread := by
          rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
        have h_konf1 := konfTipliFull_odak Γ Δ Ρ S ts1 ts2 ctx e h_konf h_t
          (by rw [h_lin]; exact ⟨τx, Λ', Ρ', ⟨h_te, h_le, h_re⟩⟩)
          (fun y h => by rw [h_if]; exact HedefVar.atama_ic x e y h)
          (fun bb h => by rw [h_if]; exact HedefBolge.atama_ic x e bb h)
        have h_beq := h_konf.2.2.2.2.2.1
        have h_hvar := h_konf.2.2.2.2.2.2.2.1
        rcases ih_e τx Λin Λ' Ρ' h_te h_le h_re (ifadeyleKonf S ts1 ts2 ctx e)
            h_konf1 ts1 ts2 { ctx with ifade := e } rfl rfl h_lin with
            h_val | h_eng | ⟨S1', ctx1', ts2', h_step1, h_t1', h_tid1, h_yan1⟩
        · -- e deger → sAtamaTamam (FIX-G: r_sabit → cikis = giris ortami)
          cases h_val with
          | iv_sabit v =>
            match h_re with
            | RegionTamam.r_sabit _ _ _ =>
            have h_b_S : bolgeOrtamGet S.bolge x = some b := by
              rw [h_beq]; exact h_gb
            have h_owner := h_hvar ctx h_ctx_in x
              (by rw [h_if]; exact HedefVar.atama_bas x (.sabit v)) b h_b_S h_yaz
            exact Or.inr (Or.inr ⟨_, { ctx with ifade := .sabit .birim }, ts2,
              Step.sAtamaTamam S _ ts1 ts2 ctx x v b h_t h_if h_b_S h_owner rfl,
              rfl, rfl, Or.inl rfl⟩)
        · -- e engelli → atama da engelli
          exact Or.inr (Or.inl (Engelli.atama_ic x e
            (engelli_konf_transfer S ts1 ts2 ctx e e h_eng h_t
              (fun v h => by rw [h_if] at h; cases h))))
        · -- e adim atar → sAtamaCong
          exact Or.inr (Or.inr ⟨_, { ctx1' with ifade := .atama x ctx1'.ifade },
            ts2',
            Step.sAtamaCong S _ (ifadeyleKonf S ts1 ts2 ctx e) S1'
              ts1 ts2 ts2' ctx ctx1' x e ctx1'.ifade
              h_t h_if rfl h_step1 h_t1' h_tid1 rfl h_yan1 rfl,
            rfl, h_tid1, h_yan1⟩)
  | seq a b ih_a _ih_b =>
      match h_ht, h_lt, h_rt with
      | HasType.t_seq _ _ _ _ τa _ hta _,
        LineerTamam.l_seq _ _ Λa _ _ _ hla _,
        RegionTamam.r_seq _ _ Ρa _ _ _ hra _ =>
        have h_konf1 := konfTipliFull_odak Γ Δ Ρ S ts1 ts2 ctx a h_konf h_t
          (by rw [h_lin]; exact ⟨τa, Λa, Ρa, ⟨hta, hla, hra⟩⟩)
          (fun y h => by rw [h_if]; exact HedefVar.seq_sol a b y h)
          (fun bb h => by rw [h_if]; exact HedefBolge.seq_sol a b bb h)
        rcases ih_a τa Λin Λa Ρa hta hla hra (ifadeyleKonf S ts1 ts2 ctx a)
            h_konf1 ts1 ts2 { ctx with ifade := a } rfl rfl h_lin with
            h_val | h_eng | ⟨S1', ctx1', ts2', h_step1, h_t1', h_tid1, h_yan1⟩
        · -- a deger → sSeqAtla
          cases h_val with
          | iv_sabit v =>
            exact Or.inr (Or.inr ⟨_, { ctx with ifade := b }, ts2,
              Step.sSeqAtla S _ ts1 ts2 ctx v b h_t h_if rfl, rfl, rfl,
              Or.inl rfl⟩)
        · -- a engelli → seq de engelli
          exact Or.inr (Or.inl (Engelli.seq_sol a b
            (engelli_konf_transfer S ts1 ts2 ctx a a h_eng h_t
              (fun v h => by rw [h_if] at h; cases h))))
        · -- a adim atar → sSeqCong
          exact Or.inr (Or.inr ⟨_, { ctx1' with ifade := .seq ctx1'.ifade b },
            ts2',
            Step.sSeqCong S _ (ifadeyleKonf S ts1 ts2 ctx a) S1'
              ts1 ts2 ts2' ctx ctx1' a ctx1'.ifade b
              h_t h_if rfl h_step1 h_t1' h_tid1 rfl h_yan1 rfl,
            rfl, h_tid1, h_yan1⟩)
  | gorevBaslat yd kod _ih_kod =>
      match h_rt with
      | RegionTamam.r_gorev_baslat _ _ _ _ _ _ tY h_yazlar _ _ _ _ =>
        obtain ⟨_, _, _, _, _, h_beq, _, h_hvar, _, _, _, _, _, _, _⟩ := h_konf
        have h_ctx_in : ctx ∈ S.thread := by
          rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
        have h_sahipler : ∀ bb ∈ bolgeleriTopla S.bolge yd,
            sahiplikGet S.sahiplik bb = some (Sahip.thread ctx.tid) := by
          intro bb h_bb
          rcases List.mem_filterMap.mp h_bb with ⟨v, h_v_in, h_lk⟩
          have h_lk_R : bolgeOrtamGet Ρ v = some bb := by
            rw [← h_beq]; exact h_lk
          exact h_hvar ctx h_ctx_in v
            (by rw [h_if]; exact HedefVar.gorev_yakala yd kod v h_v_in)
            bb h_lk (h_yazlar v h_v_in bb h_lk_R)
        exact Or.inr (Or.inr ⟨_,
          { ctx with ifade := .sabit (.gorevVal (tazeTid S)),
                     lineer := lineerTuketListe ctx.lineer yd },
          ts2 ++ [⟨tazeTid S, kod, yd.map (fun v => (v, Lineerlik.aktif))⟩],
          Step.cGorevBaslatTamam S _ ts1 ts2 ctx (tazeTid S) yd kod
            h_t h_if (tazeTid_fresh S) h_sahipler rfl,
          by simp, rfl, Or.inr ⟨_, rfl⟩⟩)
  | gorevBirlestir g =>
      by_cases h_var : ∃ hctx ∈ S.thread, ∃ vSon : Deger,
          hctx.ifade = Ifade.sabit vSon
      · obtain ⟨hctx, h_in, vSon, h_v⟩ := h_var
        exact Or.inr (Or.inr ⟨_, { ctx with ifade := .sabit .birim }, ts2,
          Step.cGorevBirlestirTamam S _ ts1 ts2 ctx g hctx.tid []
            h_t h_if ⟨hctx, h_in, rfl, vSon, h_v⟩
            (fun bb h => nomatch h) rfl,
          rfl, rfl, Or.inl rfl⟩)
      · refine Or.inr (Or.inl (Engelli.birlestir g ?_))
        intro hctx h_in v h_eq
        exact h_var ⟨hctx, h_in, v, h_eq⟩
  | kanalGonderIf k v =>
      match h_ht, h_rt with
      | HasType.t_kanal_gonder _ _ _ _ h_gv,
        RegionTamam.r_kanal_gonder _ _ _ _ _ b h_gb h_yaz _ =>
        obtain ⟨_, _, _, _, _, h_beq, _, h_hvar, _, h_bagli, _, _, _, _, _⟩ := h_konf
        have h_ctx_in : ctx ∈ S.thread := by
          rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
        obtain ⟨bv, val, h_bv, h_val, _⟩ := h_bagli v (Δ k) h_gv
        have h_b_S : bolgeOrtamGet S.bolge v = some b := by
          rw [h_beq]; exact h_gb
        have h_bb : bv = b := Option.some.inj (h_bv.symm.trans h_b_S)
        subst h_bb
        have h_owner := h_hvar ctx h_ctx_in v
          (by rw [h_if]; exact HedefVar.kanal_gonder k v) bv h_b_S h_yaz
        cases h_q : kanalIlk S.kanal k with
        | some w =>
            exact Or.inr (Or.inl (Engelli.gonder_dolu k v (by rw [h_q]; simp)))
        | none =>
            exact Or.inr (Or.inr ⟨_,
              { ctx with ifade := .sabit .birim,
                         lineer := lineerTuket ctx.lineer v }, ts2,
              Step.cKanalGonderTamam S _ ts1 ts2 ctx k v bv val
                h_t h_if h_b_S h_val h_owner h_q rfl,
              rfl, rfl, Or.inl rfl⟩)
  | kanalAlIf k =>
      obtain ⟨_, _, _, _, _, _, _, _, _, _, h_transit, _, _, _, _⟩ := h_konf
      cases h_q : kanalIlk S.kanal k with
      | none =>
          exact Or.inr (Or.inl (Engelli.bas k h_q))
      | some v =>
          -- kanalIlk = some → ilgili kanal kaydi var ve kuyrugu dolu
          have h_kd : ∃ kd ∈ S.kanal, kd.kid = k ∧ kd.gonderKuyrugu ≠ [] := by
            unfold kanalIlk at h_q
            cases h_f : S.kanal.find? (fun kd => kd.kid = k) with
            | none => rw [h_f] at h_q; cases h_q
            | some kd =>
                rw [h_f] at h_q
                have h_q2 : kd.gonderKuyrugu.head? = some v := h_q
                refine ⟨kd, List.mem_of_find?_eq_some h_f, ?_, ?_⟩
                · have h_pred := List.find?_some h_f
                  simpa using h_pred
                · intro h_nil
                  rw [h_nil] at h_q2
                  cases h_q2
          obtain ⟨kd, h_kd_in, h_kid, h_dolu⟩ := h_kd
          obtain ⟨tb, h_tb⟩ := h_transit kd h_kd_in h_dolu
          rw [h_kid] at h_tb
          exact Or.inr (Or.inr ⟨_, { ctx with ifade := .sabit v }, ts2,
            Step.cKanalAlTamam S _ ts1 ts2 ctx k v tb h_t h_if h_q h_tb rfl,
            rfl, rfl, Or.inl rfl⟩)
  | dondurIf b =>
      match h_rt with
      | RegionTamam.r_dondur _ _ _ _ x h_gx h_yaz _ =>
        obtain ⟨_, _, _, _, _, h_beq, _, _, h_hbolge, _, _, _, _, _, _⟩ := h_konf
        have h_ctx_in : ctx ∈ S.thread := by
          rw [h_t]; exact List.mem_append.mpr (Or.inr (List.Mem.head _))
        have h_owner := h_hbolge ctx h_ctx_in b
          (by rw [h_if]; exact HedefBolge.dondur_bas b)
          ⟨x, by rw [h_beq]; exact h_gx⟩ h_yaz
        exact Or.inr (Or.inr ⟨_, { ctx with ifade := .sabit .birim }, ts2,
          Step.cDondurTamam S _ ts1 ts2 ctx b h_t h_if h_owner rfl, rfl, rfl,
          Or.inl rfl⟩)
  | kullanIf x =>
      match h_lt with
      | LineerTamam.l_kullan _ _ _ _ _ h_aktif =>
        have h_aktif_ctx : lineerOrtamGet ctx.lineer x = some Lineerlik.aktif := by
          rw [h_lin]; exact h_aktif
        exact Or.inr (Or.inr ⟨_,
          { ctx with ifade := .sabit .birim,
                     lineer := lineerOrtamUpdate ctx.lineer x
                                 Lineerlik.tuketildi }, ts2,
          Step.sLinKullanTamam S _ ts1 ts2 ctx x h_t h_if h_aktif_ctx rfl,
          rfl, rfl, Or.inl rfl⟩)
  | imhaIf x =>
      match h_lt with
      | LineerTamam.l_imha _ _ _ _ _ h_aktif =>
        have h_aktif_ctx : lineerOrtamGet ctx.lineer x = some Lineerlik.aktif := by
          rw [h_lin]; exact h_aktif
        exact Or.inr (Or.inr ⟨_,
          { ctx with ifade := .sabit .birim,
                     lineer := lineerOrtamUpdate ctx.lineer x
                                 Lineerlik.tuketildi }, ts2,
          Step.sLinImhaTamam S _ ts1 ts2 ctx x h_t h_if h_aktif_ctx rfl,
          rfl, rfl, Or.inl rfl⟩)
  | guvensiz e ih_e =>
      match h_ht, h_lt, h_rt with
      | HasType.t_guvensiz _ _ _ _ hte,
        LineerTamam.l_guvensiz _ _ _ _ hle,
        RegionTamam.r_guvensiz _ _ _ _ hre =>
        have h_konf1 := konfTipliFull_odak Γ Δ Ρ S ts1 ts2 ctx e h_konf h_t
          (by rw [h_lin]; exact ⟨τ, Λ', Ρ', ⟨hte, hle, hre⟩⟩)
          (fun y h => by rw [h_if]; exact HedefVar.guvensiz_ic e y h)
          (fun bb h => by rw [h_if]; exact HedefBolge.guvensiz_ic e bb h)
        rcases ih_e τ Λin Λ' Ρ' hte hle hre (ifadeyleKonf S ts1 ts2 ctx e)
            h_konf1 ts1 ts2 { ctx with ifade := e } rfl rfl h_lin with
            h_val | h_eng | ⟨S1', ctx1', ts2', h_step1, h_t1', h_tid1, h_yan1⟩
        · cases h_val with
          | iv_sabit v =>
            exact Or.inr (Or.inr ⟨_, { ctx with ifade := .sabit v }, ts2,
              Step.sGuvensizAtla S _ ts1 ts2 ctx v h_t h_if rfl, rfl, rfl,
              Or.inl rfl⟩)
        · exact Or.inr (Or.inl (Engelli.guvensiz_ic e
            (engelli_konf_transfer S ts1 ts2 ctx e e h_eng h_t
              (fun v h => by rw [h_if] at h; cases h))))
        · exact Or.inr (Or.inr ⟨_,
            { ctx1' with ifade := .guvensiz ctx1'.ifade }, ts2',
            Step.sGuvensizCong S _ (ifadeyleKonf S ts1 ts2 ctx e) S1'
              ts1 ts2 ts2' ctx ctx1' e ctx1'.ifade
              h_t h_if rfl h_step1 h_t1' h_tid1 rfl h_yan1 rfl,
            rfl, h_tid1, h_yan1⟩)
  -- D-332: `eger` progress. Kosul deger ise sEgerSec; engelliyse eger de
  -- engelli; adim atiyorsa sEgerCong ile sarilir. Dallar burada
  -- degerlendirilmez (odak yalniz kosuldur) — seq deseninin aynisi.
  | eger k d y ih_k _ih_d _ih_y =>
      match h_ht, h_lt, h_rt with
      | HasType.t_eger _ _ _ _ _ τk _ htk _ _,
        LineerTamam.l_eger _ _ Λk _ _ _ hlk _ _,
        RegionTamam.r_eger _ _ Ρk _ _ _ hrk _ _ _ _ =>
        have h_konf1 := konfTipliFull_odak Γ Δ Ρ S ts1 ts2 ctx k h_konf h_t
          (by rw [h_lin]; exact ⟨τk, Λk, Ρk, ⟨htk, hlk, hrk⟩⟩)
          (fun z h => by rw [h_if]; exact HedefVar.eger_kosul k d y z h)
          (fun bb h => by rw [h_if]; exact HedefBolge.eger_kosul k d y bb h)
        rcases ih_k τk Λin Λk Ρk htk hlk hrk (ifadeyleKonf S ts1 ts2 ctx k)
            h_konf1 ts1 ts2 { ctx with ifade := k } rfl rfl h_lin with
            h_val | h_eng | ⟨S1', ctx1', ts2', h_step1, h_t1', h_tid1, h_yan1⟩
        · -- kosul deger → sEgerSec (dal `degerDogruMu v` ile secilir)
          cases h_val with
          | iv_sabit v =>
            exact Or.inr (Or.inr ⟨_,
              { ctx with ifade := if degerDogruMu v then d else y }, ts2,
              Step.sEgerSec S _ ts1 ts2 ctx v d y (degerDogruMu v)
                h_t h_if rfl rfl,
              rfl, rfl, Or.inl rfl⟩)
        · -- kosul engelli → eger de engelli
          exact Or.inr (Or.inl (Engelli.eger_kosul k d y
            (engelli_konf_transfer S ts1 ts2 ctx k k h_eng h_t
              (fun v h => by rw [h_if] at h; cases h))))
        · -- kosul adim atar → sEgerCong
          exact Or.inr (Or.inr ⟨_,
            { ctx1' with ifade := .eger ctx1'.ifade d y }, ts2',
            Step.sEgerCong S _ (ifadeyleKonf S ts1 ts2 ctx k) S1'
              ts1 ts2 ts2' ctx ctx1' k ctx1'.ifade d y
              h_t h_if rfl h_step1 h_t1' h_tid1 rfl h_yan1 rfl,
            rfl, h_tid1, h_yan1⟩)

end Kemgu.Meta.ProgressKorunum
