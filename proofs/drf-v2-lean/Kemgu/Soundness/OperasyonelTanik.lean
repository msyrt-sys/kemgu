/-
KEMGU DRF Mekanize — OPERASYONEL StepStar tanigi (somut konkuran YURUTME)
Kaynak: Otonom Ispat Oturumu — additive hedef 1 (StepStar; iki gorev arasi
        gercek send+recv). Cekirdek teoremleri DEGISTIRMEZ.
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz, sorry/axiom YOK

AMAC (statik tanigin OTESI):
`eszamanli_soundness` (EszamanliTanik.lean) STATIK bir iddiadir — "baslangic
konfigurasyonundan ULASILAN her S icin DRF+bellek-guvenli+fault-suz". Ama o
teorem programin GERCEKTEN ADIMLADIGINI gostermez; refl (sifir adim) tanigiyla
trivial saglanabilir gibi okunabilir. Bu dosya o boslugu KAPATIR: somut
konkuran programin (iki gorev + bir kanal) operasyonel semantikte GERCEKTEN
4 adim yurudugunu — spawn → seq-atla → SEND → RECV — ve bu yurutmenin sonunda
(ve `eszamanli_soundness` ile her ulasilan ara durumda) DRF + bellek-guvenligi
+ fault-suzlugun korundugunu kanitlar.

NON-VACUITY (dejenere-olmama gerekcesi):
- StepStar 4 GERCEK adim icerir (refl/sifir-adim DEGIL).
- Sonuc izinde HEM `kanalGonderOl 0 0 v` (tid 0 GONDERDI) HEM `kanalAlOl 1 0 v`
  (tid 1 ALDI) bulunur; gonderen ≠ alici (0 ≠ 1) → gercek IKI-GOREV arasi
  haberlesme, tek-thread sirali bir parca DEGIL.
- Sonuc konfigurasyonunda IKI thread context (0 ve 1) yan yana yasar → gercek
  konkuranlik (statik tanik yalniz tipliligi; bu tanik OPERASYONEL ilerlemeyi
  gosterir).

PURELY ADDITIVE: IyiTipliCekirdek, kemgu_soundness_v3, Step/StepStar kurallari,
invariant'lar — HICBIRI degismez. Yalniz somut konfigurasyon def'leri + adim
lemmalari + paketleme teoremi eklenir.

YURUTME IZLEMESI (K0 = baslangicKonf eszamanliProgram):
  K0 --sSeqCong[inner cGorevBaslatTamam]--> K1   (gorev 1 spawn; cocuk = kanal_al 0)
  K1 --sSeqAtla-------------------------->  K2   (spawn degeri atilir, sag'a gec)
  K2 --cKanalGonderTamam---------------->   K3   (gorev 0: kanal 0'a skaler 0 GONDER)
  K3 --cKanalAlTamam-------------------->   K4   (gorev 1: kanal 0'dan skaler 0 AL)
-/

import Kemgu.Soundness.EszamanliTanik

namespace Kemgu.Soundness.OperasyonelTanik
open Kemgu.Sem.Core Kemgu.Sem.StateTipli Kemgu.Sem.SmallStep Kemgu.Sem.Kopru
     Kemgu.Soundness.Main Kemgu.Soundness.EszamanliTanik

-- ============================================================
-- §1. Yurutme zincirindeki somut konfigurasyonlar
-- ============================================================

/-- Baslangic konfigurasyonu (tanik programi icin). -/
def K0 : Konfigurasyon := baslangicKonf eszamanliProgram

/-- Adim-1 ic kaynak: main thread odagi `gorev_baslat([], kanal_al 0)` ifadesine. -/
def S1step1 : Konfigurasyon :=
  ifadeyleKonf K0 [] [] ⟨0, anaGovdeEszamanli, []⟩ (Ifade.gorevBaslat [] cocukGovde)

/-- Adim-1 ic sonuc: cGorevBaslatTamam sonrasi (gorev 1 spawn edildi). -/
def S1Pstep1 : Konfigurasyon :=
  { S1step1 with
    thread   := [⟨0, Ifade.sabit (Deger.gorevVal 1), []⟩, ⟨1, cocukGovde, []⟩],
    sahiplik := sahiplikSetMany S1step1.sahiplik
                  (bolgeleriTopla S1step1.bolge []) (Sahip.thread 1),
    bolge    := bolgeOrtamSahipAta S1step1.bolge [] 1,
    iz       := Olay.threadBaslat 1 :: S1step1.iz,
    zaman    := S1step1.zaman + 1,
    fault    := none }

/-- Adim-1 disari (sSeqCong sarmali): spawn degeri seq'in soluna oturur. -/
def K1 : Konfigurasyon :=
  { S1Pstep1 with
    thread := [⟨0, Ifade.seq (Ifade.sabit (Deger.gorevVal 1)) (Ifade.kanalGonderIf 0 0), []⟩,
               ⟨1, cocukGovde, []⟩] }

/-- Adim-2: sSeqAtla — spawn degeri atilir, main `kanal_gonder 0 0`'a gecer. -/
def K2 : Konfigurasyon :=
  { K1 with
    thread := [⟨0, Ifade.kanalGonderIf 0 0, []⟩, ⟨1, cocukGovde, []⟩],
    zaman  := K1.zaman + 1,
    fault  := none }

/-- Adim-3: cKanalGonderTamam — gorev 0 kanal 0'a skaler 0 GONDERIR. -/
def K3 : Konfigurasyon :=
  { K2 with
    thread   := [⟨0, Ifade.sabit Deger.birim, []⟩, ⟨1, cocukGovde, []⟩],
    kanal    := kanalEkle K2.kanal 0 (Deger.skaler 0),
    sahiplik := sahiplikSet K2.sahiplik (varBolge 0) (Sahip.kanalSahip 0),
    bolge    := bolgeOrtamUpdate K2.bolge 0
                  (bolgeKategoriDegistir (varBolge 0) (BolgeKategorisi.kanalRho 0)),
    iz       := Olay.kanalGonderOl 0 0 (Deger.skaler 0) :: K2.iz,
    zaman    := K2.zaman + 1,
    fault    := none }

/-- Adim-4: cKanalAlTamam — gorev 1 kanal 0'dan skaler 0 ALIR. -/
def K4 : Konfigurasyon :=
  { K3 with
    thread   := [⟨0, Ifade.sabit Deger.birim, []⟩, ⟨1, Ifade.sabit (Deger.skaler 0), []⟩],
    kanal    := kanalCikar K3.kanal 0,
    sahiplik := sahiplikSet K3.sahiplik (varBolge 0) (Sahip.thread 1),
    iz       := Olay.kanalAlOl 1 0 (Deger.skaler 0) :: K3.iz,
    zaman    := K3.zaman + 1,
    fault    := none }

-- ============================================================
-- §2. Tek-tek adim lemmalari (her biri tek Step kurali)
-- ============================================================

/-- ADIM 1: spawn (sSeqCong govdesinde cGorevBaslatTamam). -/
theorem step_K0_K1 : Step K0 K1 := by
  have hinner : Step S1step1 S1Pstep1 :=
    Step.cGorevBaslatTamam S1step1 S1Pstep1 [] []
      ⟨0, Ifade.gorevBaslat [] cocukGovde, []⟩ 1 [] cocukGovde
      rfl rfl (tazeTid_fresh S1step1) (by intro b hb; cases hb) rfl
  exact Step.sSeqCong K0 K1 S1step1 S1Pstep1 [] [] [⟨1, cocukGovde, []⟩]
    ⟨0, anaGovdeEszamanli, []⟩ ⟨0, Ifade.sabit (Deger.gorevVal 1), []⟩
    (Ifade.gorevBaslat [] cocukGovde) (Ifade.sabit (Deger.gorevVal 1))
    (Ifade.kanalGonderIf 0 0)
    rfl rfl rfl hinner rfl rfl rfl (Or.inr ⟨⟨1, cocukGovde, []⟩, rfl⟩) rfl

/-- ADIM 2: sSeqAtla — spawn degeri atilir. -/
theorem step_K1_K2 : Step K1 K2 :=
  Step.sSeqAtla K1 K2 [] [⟨1, cocukGovde, []⟩]
    ⟨0, Ifade.seq (Ifade.sabit (Deger.gorevVal 1)) (Ifade.kanalGonderIf 0 0), []⟩
    (Deger.gorevVal 1) (Ifade.kanalGonderIf 0 0)
    rfl rfl rfl

/-- ADIM 3: cKanalGonderTamam — gorev 0 GONDERIR. -/
theorem step_K2_K3 : Step K2 K3 :=
  Step.cKanalGonderTamam K2 K3 [] [⟨1, cocukGovde, []⟩]
    ⟨0, Ifade.kanalGonderIf 0 0, []⟩ 0 0 (varBolge 0) (Deger.skaler 0)
    rfl rfl rfl rfl rfl rfl rfl

/-- ADIM 4: cKanalAlTamam — gorev 1 ALIR. -/
theorem step_K3_K4 : Step K3 K4 :=
  Step.cKanalAlTamam K3 K4 [⟨0, Ifade.sabit Deger.birim, []⟩] []
    ⟨1, cocukGovde, []⟩ 0 (Deger.skaler 0) (varBolge 0)
    rfl rfl rfl rfl rfl

-- ============================================================
-- §3. Cok-adimli yurutme (StepStar) — 4 gercek adim
-- ============================================================

/-- TANIK YURUTME: somut konkuran program baslangic konfigurasyonundan
    4 GERCEK adimda K4'e ulasir (spawn → seq-atla → SEND → RECV). -/
theorem eszamanli_operasyonel_kosu :
    StepStar (baslangicKonf eszamanliProgram) K4 :=
  StepStar.step K0 K1 K4 step_K0_K1
    (StepStar.step K1 K2 K4 step_K1_K2
      (StepStar.step K2 K3 K4 step_K2_K3
        (step_to_starStep K3 K4 step_K3_K4)))

-- ============================================================
-- §4. Non-vacuity: gercek IKI-GOREV send+recv izde
-- ============================================================

/-- K4 izinde gercek bir kanal haberlesmesi vardir: tid 0 GONDERDI,
    tid 1 ALDI, ayni deger (skaler 0), gonderen ≠ alici. -/
theorem K4_gercek_send_recv :
    Olay.kanalGonderOl 0 0 (Deger.skaler 0) ∈ K4.iz
    ∧ Olay.kanalAlOl 1 0 (Deger.skaler 0) ∈ K4.iz
    ∧ (0 : ThreadId) ≠ 1 := by
  refine ⟨?_, ?_, by decide⟩
  · exact List.Mem.tail _ (List.Mem.head _)
  · exact List.Mem.head _

-- ============================================================
-- §5. PAKETLEME — operasyonel tanik + guvenlik (ANA TEOREM)
-- ============================================================

/-- ANA OPERASYONEL TANIK (statik tanigin guclendirilmesi):
    somut konkuran program operasyonel semantikte gercekten adimlar ve
    ulasilan durumda
      (a) gercek iki-gorev send+recv gerceklesmistir (non-vacuity),
      (b) DRF (s1_invariant), bellek-guvenligi (per-Step ownership) ve
          fault-suzluk korunur.
    (b) dogrudan `eszamanli_soundness`ten gelir — boylece bu operasyonel
    kosu da V3 metateoreminin kapsamindadir; teorem yalniz refl-trivial
    bir parca DEGIL, GERCEK 4-adimli konkuran yurutme hakkindadir. -/
theorem eszamanli_operasyonel_tanik :
    ∃ S : Konfigurasyon,
      StepStar (baslangicKonf eszamanliProgram) S
      ∧ (∃ (tg ta : ThreadId) (v : Deger),
            Olay.kanalGonderOl tg 0 v ∈ S.iz
            ∧ Olay.kanalAlOl ta 0 v ∈ S.iz
            ∧ tg ≠ ta)
      ∧ DrfHolds S ∧ MemSafe_perStep S ∧ S.fault = none := by
  refine ⟨K4, eszamanli_operasyonel_kosu, ⟨0, 1, Deger.skaler 0, ?_, ?_, ?_⟩,
          eszamanli_soundness K4 eszamanli_operasyonel_kosu⟩
  · exact (K4_gercek_send_recv).1
  · exact (K4_gercek_send_recv).2.1
  · decide

end Kemgu.Soundness.OperasyonelTanik
