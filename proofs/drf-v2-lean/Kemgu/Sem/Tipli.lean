/-
KEMGU DRF Mekanize — Tipli Katmani (Onarim v3 F1)
Kaynak: ADIM0_DENETIM_RAPORU.md Bolum 2.2 + FAZ_BRIFINGLERI.md F1
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz, sorry/axiom YOK

F1 (modul yeniden katmanlama): Typed + ThreadTipliFull + KonfTipliFull
RegionTamam.lean'den buraya TASINDI. Yeni DAG:

  Core → StateTipli → HasType → LineerTamam → RegionTamam → Tipli(BU DOSYA)
       → Meta/ProgressKorunum → Discharge/{Aile2, NoFault}

Bu ayrim, StateTipli'deki `ThreadTipli = True` placeholder ciftlenmesinin
(import dongusu kaynakli) kok nedenini ortadan kaldirir: judgment'lar
(HasType/LineerTamam/RegionTamam) ile onlarin birlesimi (Typed) ve
konfigurasyon-tipliligi (KonfTipliFull) artik ayri katmanlardadir;
meta-teoremler (progress/preservation) hepsinin ustundeki Meta katmaninda
ifade edilebilir.

NOT (F1 kapsami): tanimlar tasima — davranis degisikligi YOK. KonfTipliFull
bilesen listesi ADIM 0 raporundaki haliyle korunur (7/8. bilesenlerin
fault-guard-aynasi yapisi F4'te birlesik korunum ile gercek iceriklenir).
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
-- HasType + LineerTamam + RegionTamam birlesimi.
-- ============================================================

/-- Typed full — uc katmanin tam birlesimi (Plan §3.6).
    Discharge lemmalari Typed'i hipotez alir; her Step constructor
    (Tamam/Hata) icin Typed korunumu birlesik korunum teoreminin (F4
    `adim_korunum`) induktif cekirdegi.

        Typed Γ Λ Ρ e τ Λ' Ρ' = HasType Γ e τ
                              ∧ LineerTamam Γ Λ e Λ'
                              ∧ RegionTamam Γ Ρ e Ρ' -/
structure Typed (Γ : TipOrtam) (Λ : LineerOrtam) (Ρ : BolgeOrtam)
                (e : Ifade) (τ : Tip)
                (Λ' : LineerOrtam) (Ρ' : BolgeOrtam) : Prop where
  hasType    : HasType Γ e τ
  lineerOK   : LineerTamam Γ Λ e Λ'
  regionOK   : RegionTamam Γ Ρ e Ρ'

/-- "Bos bolge ortami" — program seviyesi (kapatilmis tipli ifadeler). -/
abbrev bolgeOrtamBos : BolgeOrtam := []


-- ============================================================
-- §2. ThreadTipliFull — Plan v2 §5.2.3
-- ============================================================

/-- Thread'lerin tip-uyumu (Plan §5.2.3).

    Her thread baglaminin ifadesi Typed (HasType + LineerTamam + RegionTamam)
    olmali; bazi τ tipi + bazi Λ'/Ρ' cikis ortamlari ile.

    V1 form: tek paylasimli Λ (her ctx ayni Λ'yi kullanir) +
    `ctx.lineer ↔ Λ` iff koprusu. F4'te per-thread Λ_ctx formuna gecilir
    (ctx.lineer dogrudan Λ olarak kullanilir, kopru silinir —
    FAZ_BRIFINGLERI.md F4 madde 1). -/
def ThreadTipliFull (Γ : TipOrtam) (Λ : LineerOrtam) (Ρ : BolgeOrtam)
                    (threads : List ThreadCtx) : Prop :=
  ∀ ctx ∈ threads,
    -- §1 Tipli ifade
    (∃ τ : Tip, ∃ Λ' : LineerOrtam, ∃ Ρ' : BolgeOrtam,
      Typed Γ Λ Ρ ctx.ifade τ Λ' Ρ')
    -- §2 Plan §5.2.3 köprü: ctx.lineer ↔ Λ uyumu (V1; F4'te kalkar)
    ∧ (∀ y : VarId, ∀ lin : Lineerlik,
        (y, lin) ∈ ctx.lineer ↔ lineerOrtamGet Λ y = some lin)


-- ============================================================
-- §3. KonfTipliFull — Plan v2 §5.2 ana merkez predikat
-- ============================================================

/-- Konfigurasyon tipli — Plan v2 §5'in merkezi predicate'i.

    Bilesenler:
    1. SigmaTipli (StoreTyped)
    2. ThreadTipliFull (Typed-tabanli)
    3. SahiplikTutarli
    4. KanalTutarli
    5. S.fault = none
    6. S.bolge = Ρ (runtime bolge ortami statik Ρ ile ozdes)
    7. FrozenKategoriTutarli koprusu (isFrozen ↔ kategori donmus)
    8. AtamaSahipligi (atama yapan ctx hedef bolgeyi sahiplenir)

    ADIM 0 raporu §2.1(b) notu: 7/8. bilesenler fault-guard'larin
    aynasi invariant'lardir; korunum ispatlari F4 `adim_korunum`'da
    gercek iceriklenir (su an yalniz hipotez olarak tuketiliyorlar). -/
def KonfTipliFull (Γ : TipOrtam) (Λ : LineerOrtam) (Ρ : BolgeOrtam)
                  (S : Konfigurasyon) : Prop :=
  SigmaTipli Γ Ρ S.store
  ∧ ThreadTipliFull Γ Λ Ρ S.thread
  ∧ SahiplikTutarli Ρ S.sahiplik S.zaman
  ∧ KanalTutarli Γ Ρ S.kanal
  ∧ S.fault = none
  ∧ S.bolge = Ρ
  ∧ (∀ (x : VarId) (b : Bolge),
       bolgeOrtamGet S.bolge x = some b →
       (isFrozen S b ↔ b.kategori = BolgeKategorisi.donmus))
  ∧ (∀ ctx ∈ S.thread, ∀ (y : VarId) (e : Ifade),
       ctx.ifade = Ifade.atama y e →
       ∀ (b : Bolge), bolgeOrtamGet S.bolge y = some b →
         sahiplikGet S.sahiplik (b, S.zaman) = some (Sahip.thread ctx.tid))


-- ============================================================
-- §4. KonfTipliFull yapilandirma yardimlari
-- ============================================================

/-- KonfTipliFull yapilandirma yardimi (8-bilesen introduce). -/
theorem konfTipliFull_intro
    (Γ : TipOrtam) (Λ : LineerOrtam) (Ρ : BolgeOrtam) (S : Konfigurasyon)
    (h_store  : SigmaTipli Γ Ρ S.store)
    (h_thread : ThreadTipliFull Γ Λ Ρ S.thread)
    (h_sahip  : SahiplikTutarli Ρ S.sahiplik S.zaman)
    (h_kanal  : KanalTutarli Γ Ρ S.kanal)
    (h_fault  : S.fault = none)
    (h_bolge_eq : S.bolge = Ρ)
    (h_frozen_kat : ∀ (x : VarId) (b : Bolge),
                      bolgeOrtamGet S.bolge x = some b →
                      (isFrozen S b ↔ b.kategori = BolgeKategorisi.donmus))
    (h_atama_sahip : ∀ ctx ∈ S.thread, ∀ (y : VarId) (e : Ifade),
                       ctx.ifade = Ifade.atama y e →
                       ∀ (b : Bolge), bolgeOrtamGet S.bolge y = some b →
                         sahiplikGet S.sahiplik (b, S.zaman)
                           = some (Sahip.thread ctx.tid)) :
    KonfTipliFull Γ Λ Ρ S :=
  ⟨h_store, h_thread, h_sahip, h_kanal, h_fault, h_bolge_eq, h_frozen_kat,
   h_atama_sahip⟩

/-- KonfTipliFull'den bilesenleri cikarma (8-bilesen projection). -/
theorem konfTipliFull_elim
    (Γ : TipOrtam) (Λ : LineerOrtam) (Ρ : BolgeOrtam) (S : Konfigurasyon)
    (h : KonfTipliFull Γ Λ Ρ S) :
    SigmaTipli Γ Ρ S.store
    ∧ ThreadTipliFull Γ Λ Ρ S.thread
    ∧ SahiplikTutarli Ρ S.sahiplik S.zaman
    ∧ KanalTutarli Γ Ρ S.kanal
    ∧ S.fault = none
    ∧ S.bolge = Ρ
    ∧ (∀ (x : VarId) (b : Bolge),
         bolgeOrtamGet S.bolge x = some b →
         (isFrozen S b ↔ b.kategori = BolgeKategorisi.donmus))
    ∧ (∀ ctx ∈ S.thread, ∀ (y : VarId) (e : Ifade),
         ctx.ifade = Ifade.atama y e →
         ∀ (b : Bolge), bolgeOrtamGet S.bolge y = some b →
           sahiplikGet S.sahiplik (b, S.zaman) = some (Sahip.thread ctx.tid)) :=
  h

end Kemgu.Sem.Tipli
