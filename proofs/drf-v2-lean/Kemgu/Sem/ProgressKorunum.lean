/-
KEMGU DRF Mekanize — Progress + Preservation (Plan v2 Adim 4)
Kaynak (kagit formel): belgeler/KEMGU_Mekanize_Onarim_Plan.md §7.2 Adim 4
Wright-Felleisen: TAPL §8.3.2 (Progress) + §8.3.3 (Preservation)
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz

Adim 4.1 ISKELET (bu commit):
- IsValue predicate (tam, ispat gerekmez)
- Progress + Preservation statement (sorry placeholder, TODO Adim 4.2-4.3)
- 4 ConfigTyped korunum lemma statement (sorry placeholder, TODO Adim 4.4)

Adim 4'un alt-adimlari:
- 4.1 (bu): Iskelet + statement'lar
- 4.2: Progress proof — 12 HasType case analizi
- 4.3: Preservation proof — 15 Step constructor case analizi
- 4.4: ConfigTyped korunum lemmalari + birlesim

Plan v2 §7.2 tahmini: ~300 satir, ~2 hafta — 4 alt-oturuma yayilir.

Onkosul: Adim 3 (HasType), Adim 2 (StateTipli + KonfTipli).
-/

import Kemgu.Sem.Core
import Kemgu.Sem.SmallStep
import Kemgu.Sem.StateTipli
import Kemgu.Sem.HasType

namespace Kemgu.Sem.ProgressKorunum
open Kemgu.Sem.Core Kemgu.Sem.SmallStep Kemgu.Sem.StateTipli Kemgu.Sem.HasType

-- ============================================================
-- §1. IsValue — Bir ifadenin deger olmasi (TAPL §8.3.1 normal form)
-- ============================================================

/-- Bir ifade `e` bir DEGER (irreducible normal form) ise IsValue e.

    KEMGU'da degerli ifadeler:
    - `Ifade.sabit v` — literal deger (her v : Deger icin)

    Diger ifadeler (atama, seq, gorevBaslat, kanalGonderIf, kullanIf, vs.)
    DEGER DEGIL — Step reduksiyonu ile ilerlerler.

    V1 sinir: Closure'lar Ifade'de yok (Plan §1 minimal subset);
    closure deger Adim 5 LinearOK + Ifade'nin closureRef gibi
    bir ekleme ile gelir (V2 hedef). -/
inductive IsValue : Ifade → Prop where
  | iv_sabit (v : Deger) : IsValue (Ifade.sabit v)


-- ============================================================
-- §2. Progress (TAPL §8.3.2) — ISKELET
-- ============================================================

/-- Progress (Wright-Felleisen, TAPL §8.3.2 KEMGU adaptasyonu):

    Bos ortamda HasType olan bir e ifadesi ya bir DEGERdir
    (IsValue e) ya da bir reduksiyon adimi alabilir (Step S → S').

    KEMGU adaptasyonu: e bir thread'in ifadesi varsayilir; tek-thread
    konfigurasyon altinda Progress single-step semantiginde calisir.

    V1 sinirlari:
    - S.thread = [ctx] (tek thread)
    - h_no_fault: fault state Progress disinda
    - HasType bos Γ ile (kapatilmis program)

    Adim 4.2'de tam ispat — 12 HasType kurali icin case analizi:
    - t_sabit ⟹ IsValue (sol kol)
    - Diger 11 kural ⟹ Step yardimcilarini insa et (sag kol)
-/
theorem progress
    (e : Ifade) (τ : Tip)
    (h_typed : HasType tipOrtamBos e τ)
    (S : Konfigurasyon) (ctx : ThreadCtx)
    (_h_ctx_in : ctx ∈ S.thread) (_h_ctx_ifade : ctx.ifade = e)
    (_h_no_fault : S.fault = none) :
    IsValue e ∨ ∃ S', Step S S' := by
  -- Adim 4.2 partial proof: 12 HasType case analizi
  -- TAMAM (7): t_tanim, t_sabit, t_atama, t_gorev_birlestir, t_kanal_gonder,
  --            t_kullan, t_imha (6 vacuous boş Γ = [] + 1 value)
  -- KALAN  (5): t_seq, t_gorev_baslat, t_kanal_al, t_dondur, t_guvensiz
  --             (Step constructor insasi/induktif IH — Adim 4.2b)
  cases h_typed with
  -- VACUOUS: boş Γ ile lookup imkansiz (kontradiksyon)
  -- NOT: Lean 4 cases pattern Γ index'i outer tipOrtamBos'tan auto-bind ediyor.
  -- Pattern args = constructor binder count - 1.
  | t_tanim x _ h_get =>
    exfalso; simp [tipOrtamGet] at h_get
  -- VALUE: sabit v dogrudan IsValue
  | t_sabit v _ _ =>
    left; exact IsValue.iv_sabit v
  -- VACUOUS:
  | t_atama x _ _ h_get _ =>
    exfalso; simp [tipOrtamGet] at h_get
  -- TODO: Adim 4.2b — seq induktif Progress (alt-ifadelere IH uygula)
  | t_seq _ _ _ _ _ _ =>
    sorry
  -- TODO: Adim 4.2b — Step.cGorevBaslatTamam insasi (threadFresh + yenContext)
  | t_gorev_baslat _ _ _ _ =>
    sorry
  -- VACUOUS:
  | t_gorev_birlestir _ _ h_get =>
    exfalso; simp [tipOrtamGet] at h_get
  -- VACUOUS:
  | t_kanal_gonder _ _ _ h_get =>
    exfalso; simp [tipOrtamGet] at h_get
  -- TODO: Adim 4.2b — Step.cKanalAlTamam insa (kanal var/yok ayrimi)
  | t_kanal_al _ _ =>
    sorry
  -- TODO: Adim 4.2b — Step.cDondurTamam insa (b zaten frozen/değil ayrimi)
  | t_dondur _ =>
    sorry
  -- VACUOUS:
  | t_kullan _ _ h_get =>
    exfalso; simp [tipOrtamGet] at h_get
  -- VACUOUS:
  | t_imha _ _ h_get =>
    exfalso; simp [tipOrtamGet] at h_get
  -- TODO: Adim 4.2b — guvensiz induktif Progress (alt-ifadeye IH)
  | t_guvensiz _ _ _ =>
    sorry


-- ============================================================
-- §3. Preservation (TAPL §8.3.3) — ISKELET
-- ============================================================

/-- Preservation (Wright-Felleisen, TAPL §8.3.3 KEMGU adaptasyonu):

    Eger e : τ ve Step S → S' (S.thread ctx içinde e ise), o zaman
    S' içinde ayni thread'in yeni ifadesi de τ tipinde tip-uyumlu kalir.

    KEMGU adaptasyonu: Step Configuration semantiginde, thread tid
    aracilgiyla ctx ↔ ctx' eslestirmesi.

    V1 sinirlari:
    - HasType bos Γ ile (kapatilmis)
    - Lineerlik/bolge korunumu YOK (Adim 5/6'da eklenecek)

    Adim 4.3'te tam ispat — Step'in 15 constructor'i (8 Tamam + 7 Hata)
    icin case analizi. Hata constructor'lari typed program'da ulasilmaz
    (Adim 7 Discharge) — buradaki Preservation salt structural.
-/
theorem preservation
    (S S' : Konfigurasyon) (_h_step : Step S S')
    (ctx : ThreadCtx) (τ : Tip)
    (_h_in : ctx ∈ S.thread)
    (_h_typed : HasType tipOrtamBos ctx.ifade τ)
    (_h_no_fault_target : S'.fault = none) :  -- Adim 4.3 imza guclendirmesi
    ∃ ctx' ∈ S'.thread,
      ctx'.tid = ctx.tid ∧
      HasType tipOrtamBos ctx'.ifade τ := by
  -- TODO: Adim 4.3'te tam ispat — 15 Step constructor case analizi
  -- Strateji (V2 hedef):
  --   1. Hata constructors (7): exfalso + h_no_fault_target vs h_fault celiski
  --   2. Tamam constructors (8): thread tracking — Step ctx_step.tid == ctx.tid
  --      ise yeni ifade tipi korundu (small-step preservation lemma'sı per
  --      constructor); ctx_step.tid != ctx.tid ise ctx korunur (başka thread).
  --   3. sAtamaTamam: yeni ctx'.ifade Configuration'da ilerletilmis kismi
  --      (örn. atama sonrasi bos ifade). HasType bos -> Tip.bos için tutar.
  -- V1 sinir: tam proof Plan §7.2 Adim 4.3 (2 hafta) + KEMGU thread tracking
  -- detayları açıklığa kavuşunca.
  sorry


-- ============================================================
-- §4. ConfigTyped korunum lemmalari (Plan §5.3)
-- Her alt-yapi icin ayri Preservation lemma — KonfTipli'nin induktif
-- cekirdegi. Adim 4.4'te detay.
-- ============================================================

/-- SigmaTipli (StoreTyped) korunumu.
    Step S → S' altinda store'un tip-uyumu korunur.

    Adim 4.4 full ispat: Step 15 constructor case'i — hangileri store'a
    yazar (sAtamaTamam, h_store ile (k,v) push), hangileri degistirmez.
    `h_no_fault_target` sayesinde Hata constructor'lari typed exfalso. -/
theorem preservation_sigmaTipli
    (Γ : TipOrtam) (Ρ : BolgeOrtam) (S S' : Konfigurasyon)
    (_h_step : Step S S')
    (_h_sigma : SigmaTipli Γ Ρ S.store)
    (_h_no_fault_target : S'.fault = none) :
    SigmaTipli Γ Ρ S'.store := by
  -- TODO: Adim 4.4'te tam ispat — Step constructor case'leri
  sorry

/-- SahiplikTutarli (SahiplikConsistent) korunumu.
    Sahiplik haritasinin bilinen-bolge + frozen-persistence kosullari
    Step altinda korunur.

    Adim 4.4 full ispat: cGorevBaslat/cGorevBirlestir/cKanal*/cDondur
    sahiplik degisimi yapar — her birinde Tutarli kosullarinin korundugu
    gosterilir. -/
theorem preservation_sahiplikTutarli
    (Ρ : BolgeOrtam) (S S' : Konfigurasyon)
    (_h_step : Step S S')
    (_h_sahip : SahiplikTutarli Ρ S.sahiplik S.zaman) :
    SahiplikTutarli Ρ S'.sahiplik S'.zaman := by
  -- TODO: Adim 4.4'te tam ispat
  sorry

/-- KanalTutarli (KanalConsistent) korunumu.
    Kanal kuyrugundaki her degerin tip-uyumu Step altinda korunur.

    Adim 4.4 full ispat: cKanalGonder ekler (DegerTipli ile), cKanalAl
    cikartir — her ikisi de invariant'i korur. -/
theorem preservation_kanalTutarli
    (Γ : TipOrtam) (Ρ : BolgeOrtam) (S S' : Konfigurasyon)
    (_h_step : Step S S')
    (_h_kanal : KanalTutarli Γ Ρ S.kanal) :
    KanalTutarli Γ Ρ S'.kanal := by
  -- TODO: Adim 4.4'te tam ispat
  sorry


-- ============================================================
-- §5. KonfTipli korunumu — ana Preservation teorem catısı
-- (4 alt-lemma'nin conjunction'i + ThreadTipli iskelet)
-- ============================================================

/-- KonfTipli korunumu — Plan v2 §5.3 "Preservation theorem'in induktif
    cekirdegi".

    Bu teorem KEMGU'nun yapilanmasinin SOUNDNESS catısı: typed konfigurasyon
    Step altinda typed kalir. ThreadTipli su anlik `True` (Adim 2 iskelet),
    Adim 6 sonu Typed (HasType + LinearOK + RegionOK) ile gercek tanim.

    Adim 4.4 full ispat: 4 sub-lemma conjunction + ThreadTipli True =
    KonfTipli S' korunumu. -/
theorem preservation_konfTipli
    (Γ : TipOrtam) (Λ : LineerOrtam) (Ρ : BolgeOrtam) (S S' : Konfigurasyon)
    (_h_step : Step S S')
    (_h_konf : KonfTipli Γ Λ Ρ S)
    (_h_no_fault_target : S'.fault = none) :
    KonfTipli Γ Λ Ρ S' := by
  -- TODO: Adim 4.4'te tam ispat — 4 sub-lemma + ThreadTipli (True placeholder)
  sorry


-- ============================================================
-- §6. Soundness corollary (Wright-Felleisen)
-- ISKELET — Progress + Preservation'in birlesimi.
-- ============================================================

/-- Soundness (Wright-Felleisen birlesimi):
    "Well-typed programs never get stuck" — bos ortamda HasType olan
    bir program StepStar altinda her zaman ya bir degere ulasir ya da
    Step almaya devam edebilir.

    Adim 4 sonu (4.4 sonrasi) bu corollary Progress + Preservation
    induktif birlesimiyle dolar. Su an iskelet. -/
theorem soundness_corollary
    (S S' : Konfigurasyon) (_h_run : StepStar S S')
    (ctx : ThreadCtx) (τ : Tip)
    (_h_in : ctx ∈ S.thread)
    (_h_typed : HasType tipOrtamBos ctx.ifade τ) :
    -- "Stuck olmaz": ya S' icinde bir IsValue thread var ya da Step alabilir
    True := by
  -- TODO: Adim 4 sonu — Progress + Preservation StepStar induksiyon
  trivial


-- ============================================================
-- §7. Adim 4 sub-step durumu — V1 sinir notu
-- ============================================================

/-
Adim 4 sub-step durumu (2026-05-22, Adim 4.2 sonrasi):

✅ Adim 4.1 (iskelet): 6 statement placeholder — TAMAM
✅ Adim 4.2 (partial progress): 7/12 case kanitli — TAMAM
   - Vacuous Γ = [] (6): t_tanim, t_atama, t_gorev_birlestir, t_kanal_gonder, t_kullan, t_imha
   - Value (1): t_sabit (IsValue.iv_sabit)

⏳ Adim 4.2b (kalan 5 case): V1 SINIR
   - t_seq, t_guvensiz: induktif Progress alt-ifadelere — induction tactic + IH gerek
   - t_gorev_baslat: Step.cGorevBaslatTamam insasi — threadFresh witness + S' yenContext
   - t_kanal_al: Step.cKanalAlTamam insa — kanal h_kanal_var: ∃ kd ∈ S.kanal kanal bos
     ise Step alinamaz (Progress tam form'u BOZULUR — V1 sinir)
   - t_dondur: Step.cDondurTamam insa — h_sahip update
   Plan §7.2 ~1 hafta ek calisma.

⏳ Adim 4.3 (preservation full): V1 SINIR
   Signature guclendirildi (_h_no_fault_target eklendi, Adim 4.3 zemin).
   - 15 Step constructor case (8 Tamam + 7 Hata)
   - Hata (7): exfalso + h_no_fault_target vs h_fault celiski
   - Tamam (8): thread tracking + per-constructor preservation lemma
   - sAtamaTamam icin: atama sonrasi ifade (ctx.ifade' = ??) — KEMGU
     Configuration semantik detayi gerek (Step ifadeleri DEGISTIRMIYOR mu,
     yoksa ifadeyi ilerletiyormu? V1 model belirtilmemis)
   Plan §7.2 ~1.5 hafta.

⏳ Adim 4.4 (ConfigTyped korunum lemmalari): V1 SINIR
   - preservation_sigmaTipli: sAtamaTamam (k,v) push icin DegerTipli garantisi gerek
   - preservation_sahiplikTutarli: frozen persistence + sahiplik update case'leri
   - preservation_kanalTutarli: cKanalGonder/cKanalAl kanal degisim case'leri
   - preservation_konfTipli: 4 sub-lemma conjunction
   Tum lemmalarda Hata (7) exfalso (h_no_fault_target var), Tamam (8) per-konunum
   case detayı. Plan §7.2 ~1.5 hafta.

KAPSAM NOTU (Plan §7.2 + KEMGU V1 spesifik):

KEMGU semantik karmaşıklıgi V1'de (Configuration-level Step, multi-thread,
sahiplik haritasi vs.) klasik Wright-Felleisen Soundness'i adapte etmeyi
guc kilar. Plan §3 felsefesi "asamali insa" — bu sub-step'ler Adim 5-6
sonrasi (LinearOK + RegionOK + Typed conjunction) ek bilgi ile daha
tractable olur. Adim 7 Discharge lemmalari + No-Fault çatı bu lemmaları
gercek-anlam'da doldurma sirasi.

Tikanma politikasi (CLAUDE.md): Bu noktada DURMA emri kullanici tarafindan
"Adim 5'e kadar otomatik devam et" olarak verildi — yani Adim 4 sub-step'ler
mevcut iskelet durumunda korunur, Adim 5 baslamadan oturum bitirilir.
-/

end Kemgu.Sem.ProgressKorunum
