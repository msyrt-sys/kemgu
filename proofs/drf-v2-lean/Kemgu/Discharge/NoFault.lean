/-
KEMGU DRF Mekanize — No-Fault Catı Teoremi (Plan v2 Adim 7 + yarım kalan)
Kaynak: belgeler/KEMGU_Mekanize_Onarim_Plan.md §6.3 No-Fault Theorem + §7.2 Adim 7
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz

Adim 7 (tam): Discharge ailesi catı + No-Fault teoremi.

Adim 7 ana basari (önceki commit): 35 Hata case sorry'si SmallStep
strengthened Hata constructor'lar ile trivial kapandi (L4/L7/Drf/MemSafety).

Adim 7 yarım kalan kısım (bu commit):
1. SmallStep.lean: 8 Tamam Step constructor'a h_no_fault_target eklendi
   (Plan §4.4 simetri — Tamam'lar fault'i preserve, Hata'lar set).
2. step_fault_preserves_typed: 8 Tamam case full ispatli (h_no_fault_target);
   7 Hata case sorry (Adim 8 — Plan §6.2 Aile 2 lemma'lari ile dolar).
3. typed_no_fault: refl case full ispatli (S = S₀ → S.fault = S₀.fault);
   step case sorry (Adim 8 — preservation_konfTipli + step_fault zinciri).

Adim 8 hedefi: Aile 2 (Fault Impossibility) lemma'lari + preservation_konfTipli
full proof → step_fault_preserves_typed Hata case'leri kapanir + typed_no_fault
step case kapanir → sorry: 14 (toplam 16 - 2 Adim 7 iskelet kapanir).

Onkosul: Adim 1.1-1.3 (Step dual), Adim 2 (StateTipli), Adim 3 (HasType),
         Adim 5 (LineerTamam), Adim 6 (RegionTamam + Typed + KonfTipliFull).
-/

import Kemgu.Sem.Core
import Kemgu.Sem.SmallStep
import Kemgu.Sem.StateTipli
import Kemgu.Sem.HasType
import Kemgu.Sem.LineerTamam
import Kemgu.Sem.RegionTamam
import Kemgu.Discharge.Aile2

namespace Kemgu.Discharge.NoFault
open Kemgu.Sem.Core Kemgu.Sem.SmallStep Kemgu.Sem.StateTipli
     Kemgu.Sem.HasType Kemgu.Sem.LineerTamam Kemgu.Sem.RegionTamam
     Kemgu.Discharge.Aile2

-- ============================================================
-- §1. Tek-adim fault korunumu (typed varsayımı altında)
-- ============================================================

/-- Step tek-adim fault korunumu (Adim 7 yarım kalan):
    KonfTipliFull S + S.fault = none + Step S S' → S'.fault = none.

    Tamam Step'leri: h_no_fault_target hipoteziyle direkt.
    Hata Step'leri: typed varsayımı ile imkansiz (Plan §6.2 Aile 2 lemma'lari);
    V1 Adim 7'de Hata case'leri sorry — Adim 8 hedef.

    Aile 2 lemma'lari (Adim 8):
    - typing_excludes_sAtamaHataDonmus: Typed → Ρ x = some b, b.kategori ≠ donmus
      + KonfTipliFull.SahiplikTutarli → isFrozen S k.bolge ⟹ kategori donmus
      → çelişki h_frozen ile
    - typing_excludes_sAtamaHataSahipDegil: Typed + SahiplikTutarli → owner consistency
    - typing_excludes_cGorevBaslatHataLineerIhlal: LineerTamam → yakalama tuketildi
    - typing_excludes_cKanalGonderHataLineerTuket: LineerTamam → vId tuketilmemis
    - typing_excludes_cDondurHataZatenDonmus: RegionTamam → b kategori ≠ donmus
    - typing_excludes_sLinKullanHataZatenTuketildi: LineerTamam → x aktif
    - typing_excludes_sLinImhaHataZatenTuketildi: aynı sLinKullan -/
theorem step_fault_preserves_typed
    (Γ : TipOrtam) (Λ : LineerOrtam) (Ρ : BolgeOrtam)
    (S S' : Konfigurasyon) (h_step : Step S S')
    (h_typed_S : KonfTipliFull Γ Λ Ρ S)
    (_h_no_fault : S.fault = none) :
    S'.fault = none := by
  cases h_step with
  -- Tamam case'leri: h_no_fault_target ile direkt (Adim 7 yarım kalan)
  | sAtamaTamam _ _ _ _ _ _ _ _ _ _ _ _ _ h_no_fault_target =>
    exact h_no_fault_target
  | cGorevBaslatTamam _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ h_no_fault_target =>
    exact h_no_fault_target
  | cGorevBirlestirTamam _ _ _ _ _ _ _ _ _ _ _ _ h_no_fault_target =>
    exact h_no_fault_target
  | cKanalGonderTamam _ _ _ _ _ _ _ _ _ _ _ _ h_no_fault_target =>
    exact h_no_fault_target
  | cKanalAlTamam _ _ _ _ _ _ _ _ _ _ _ h_no_fault_target =>
    exact h_no_fault_target
  | cDondurTamam _ _ _ _ _ _ _ _ _ h_no_fault_target =>
    exact h_no_fault_target
  | sLinKullanTamam _ _ _ _ _ _ _ _ _ h_no_fault_target =>
    exact h_no_fault_target
  | sLinImhaTamam _ _ _ _ _ _ _ _ _ h_no_fault_target =>
    exact h_no_fault_target
  -- Hata case'leri: Adim 8 hedef — Plan §6.2 Aile 2 lemma'lari ile exfalso.
  | sAtamaHataDonmus _ _ _ _ _ _ _ _ _ _ _ _ _ =>
    -- TODO Adim 8: typing_excludes_sAtamaHataDonmus uygula
    sorry
  | sAtamaHataSahipDegil _ _ _ _ _ _ _ _ _ _ _ _ _ =>
    -- TODO Adim 8: typing_excludes_sAtamaHataSahipDegil uygula
    sorry
  | cGorevBaslatHataLineerIhlal _ _ _ _ _ _ _ _ _ _ _ _ _ _ =>
    -- TODO Adim 8: typing_excludes_cGorevBaslatHataLineerIhlal uygula
    sorry
  | cKanalGonderHataLineerTuket ctx kId vId h_in h_ifade h_tuket _ _ _ _ _ _ =>
    -- Adim 8 P2: Aile 2 dispatch — typing_excludes_cKanalGonderHataLineerTuket
    have h_thread := h_typed_S.2.1
    obtain ⟨h_typed_exists, h_bridge⟩ := h_thread ctx h_in
    obtain ⟨τ', Λ'', Ρ'', h_typed⟩ := h_typed_exists
    rw [h_ifade] at h_typed
    exact (typing_excludes_cKanalGonderHataLineerTuket
            Γ Λ Ρ kId vId τ' Λ'' Ρ'' h_typed ctx h_bridge h_tuket).elim
  | cDondurHataZatenDonmus _ _ _ _ _ _ _ _ _ _ _ =>
    -- TODO Adim 8: typing_excludes_cDondurHataZatenDonmus uygula
    sorry
  | sLinKullanHataZatenTuketildi ctx x_pat h_in h_ifade h_tuket _ _ _ _ _ _ =>
    -- Adim 8 P1: Aile 2 dispatch — typing_excludes_sLinKullanHataZatenTuketildi
    have h_thread := h_typed_S.2.1
    obtain ⟨h_typed_exists, h_bridge⟩ := h_thread ctx h_in
    obtain ⟨τ', Λ'', Ρ'', h_typed⟩ := h_typed_exists
    rw [h_ifade] at h_typed
    exact (typing_excludes_sLinKullanHataZatenTuketildi
            Γ Λ Ρ x_pat τ' Λ'' Ρ'' h_typed ctx h_bridge h_tuket).elim
  | sLinImhaHataZatenTuketildi ctx x_pat h_in h_ifade h_tuket _ _ _ _ _ _ =>
    -- Adim 8 P1: Aile 2 dispatch — typing_excludes_sLinImhaHataZatenTuketildi
    have h_thread := h_typed_S.2.1
    obtain ⟨h_typed_exists, h_bridge⟩ := h_thread ctx h_in
    obtain ⟨τ', Λ'', Ρ'', h_typed⟩ := h_typed_exists
    rw [h_ifade] at h_typed
    exact (typing_excludes_sLinImhaHataZatenTuketildi
            Γ Λ Ρ x_pat τ' Λ'' Ρ'' h_typed ctx h_bridge h_tuket).elim


-- ============================================================
-- §2. No-Fault catı teoremi (Plan v2 §6.3) — Adim 7 yarım kalan
-- ============================================================

/-- TYPED NO-FAULT CATI (Plan v2 §6.3) — Adim 7 ana hedef teorem (yarım kalan):

    Iyi-tipli baslangic konfigurasyon (KonfTipliFull) ile baslatilan
    herhangi bir yurutme zinciri (StepStar S₀ S) fault state'e ulasmaz.

    Kagit ifadesi (Plan §6.3):
      IyiTipli(Π) ⟹ ∀ S ∈ Reach(S₀(Π)) : S.fault = none

    Bizim V1 form:
      KonfTipliFull Γ Λ Ρ S₀ → StepStar S₀ S → S.fault = none

    KonfTipliFull S₀ tanim'i `S₀.fault = none` icerir (§5 alti-konuum
    KonfTipliFull RegionTamam.lean'de).

    Ispat:
      Induction h_run:
        refl: S = S₀ → S.fault = S₀.fault = none ✓ (KonfTipliFull S₀)
        step S0 S1 Send hStep hStar IH:
          step_fault_preserves_typed hStep → S1.fault = none
          IH (S1 KonfTipli korunum + S1.fault=none): preservation_konfTipli
            (Adim 4.4 sorry — Adim 8 hedef) gerek

    V1 Adim 7 yarım kalan durumu:
    - refl case FULL ispatli ✓
    - step case sorry (Adim 8 — preservation_konfTipli + step_fault zinciri) -/
theorem typed_no_fault
    (Γ : TipOrtam) (Λ : LineerOrtam) (Ρ : BolgeOrtam)
    (S₀ S : Konfigurasyon)
    (h_typed_init : KonfTipliFull Γ Λ Ρ S₀)
    (h_run : StepStar S₀ S) :
    S.fault = none := by
  induction h_run with
  | refl _ =>
    -- S = S₀, KonfTipliFull S₀ → S₀.fault = none (5. alti-konuum;
    -- Adim 8 V2'de S.bolge=Ρ + kopru eklendi → fault projeksiyonu .2.2.2.2.1)
    exact h_typed_init.2.2.2.2.1
  | step S0 S1 Send _hStep _hStar _IH =>
    -- TODO Adim 8: step_fault_preserves_typed hStep (KonfTipliFull S0)
    --              (KonfTipliFull S₀.5 = S₀.fault = none) → S1.fault = none
    -- Sonra preservation_konfTipli ile KonfTipliFull S1 ve IH(S1 typed, fault=none)
    sorry


-- ============================================================
-- §3. Plan v2 §6.2 Discharge ailelerinin V1 durumu
-- ============================================================

/-
PLAN v2 §6.2 4 Discharge ailesinin V1 (Adim 7 + yarım kalan) durumu:

AILE 1 — Normal Guards (Typed + KonfTipliFull → Step.Ok guard'lar):
  Durum V1: ADIM 8 hedef. Progress + Preservation full proof'unun
  Step.Ok constructor insasi argumani olarak gerekir.

AILE 2 — Fault Impossibility (Typed + KonfTipliFull → Step.Fault imkansiz):
  Durum V1: Adim 7'de 35 L4/L7/Drf/MemSafety Hata case'i strengthen sayesinde
  REDUNDANT kapandı. step_fault_preserves_typed Hata case'leri Aile 2 lemma'lari
  ile dolar. Plan §6.2 7 typing_excludes_* lemma (Aile2.lean):
    typing_excludes_sLinKullanHataZatenTuketildi   ✓ P1 (l_kullan kopru)
    typing_excludes_sLinImhaHataZatenTuketildi     ✓ P1 (l_imha kopru)
    typing_excludes_cKanalGonderHataLineerTuket    ✓ P2 (l_kanal_gonder strengthen)
    typing_excludes_sAtamaHataDonmus               ⏳ V2 (k serbest; Ρ runtime gerek)
    typing_excludes_sAtamaHataSahipDegil           ⏳ V2 (Typed ownership gerek)
    typing_excludes_cGorevBaslatHataLineerIhlal    ⏳ V2 (vIhlal serbest; yd baglantisi)
    typing_excludes_cDondurHataZatenDonmus         ⏳ V2 (isFrozen↔kategori kopru, Ρ runtime)
  Kalan 4'un ortak kok nedeni: statik Ρ sabit + runtime degisken-ortami yok.
  Tek V2 refactor (Ρ → Konfigurasyon) dordunu birden acar.

AILE 3 — Linear Discharge (LineerTamam → linear guard'lar):
  Durum V1: ADIM 8 hedef. cGorevBaslatTamam h_lineer_caller temin eder.

AILE 4 — Region Discharge (RegionTamam → bolge guard'lar):
  Durum V1: ADIM 8 hedef. cGorevBaslatTamam h_sahip temin eder.

NO-FAULT CATI (§6.3):
  Adim 7 yarım: refl case FULL, step case sorry.
  Adim 8 full: step_fault_preserves Hata + preservation_konfTipli zinciri.

ADIM 7 GERCEKLEŞEN (iki commit'te):
  - Onceki: 35 sorry KAPANDI (-33 net) + 2 iskelet (NoFault statement-only)
  - Bu: Tamam strengthen + step_fault_preserves Tamam case full + typed_no_fault
    refl case full. Yeni 7 Hata sorry + 1 step case sorry. Eski 2 statement-only
    sorry'leri yapısal proof'a evrildi. Net sorry: +6 (geçici artış, yapısal
    genişleme). Adim 8'de bu 8 yeni sorry düşer + Adim 4/5/6 iskelet sorry'leri
    (14) dolar → C5 hedef: 0 sorry.
-/


end Kemgu.Discharge.NoFault
