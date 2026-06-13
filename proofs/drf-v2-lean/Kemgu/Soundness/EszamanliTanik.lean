/-
KEMGU DRF Mekanize — Cekirdek ANLAMLILIK tanigi: SOMUT KONKURAN program
Kaynak: feature/drf-eszamanli-tanik gorevi (Mehmet)
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz, sorry/axiom YOK

AMAC (vakum-elestirisine somut cevap):
`bos_program_iyiTipli` (Kopru.lean §5) total-vakumu eler ama bos program
TRIVIYAL DRF — sifir gorev, sifir kanal. Bu dosya IyiTipliCekirdek'in
GERCEK bir KONKURAN programi (iki gorev + bir kanal haberlesmesi) kabul
ettigini kanitlar; boylece kemgu_soundness_v3 teoreminin yalniz
triviyal-sirali bir parca hakkinda OLMADIGI gosterilir.

PURELY ADDITIVE: IyiTipliCekirdek (Kopru.lean) ve kemgu_soundness_v3
(Soundness/Main.lean) METINLERI DEGISMEZ — yalniz yeni tanik lemma +
kopru/soundness UYGULAMALARI eklenir.

TANIK PROGRAMIN YAPISI:
  ana =  gorev_baslat([], kanal_al(0))      -- gorev B (cocuk): kanaldan ALIR
       ; kanal_gonder(0, v0)                 -- gorev A (main):  kanala GONDERIR
- Iki gorev: main thread (tid 0) + spawn edilen cocuk.
- Bir kanal (kid 0): A→B yonunde tek skaler mesaj (send + recv).
- Kapasite-1 senkron kanal (V1 cekirdek semantigi; baslangicta bos).

KRITIK GOZLEM (Yol-B asimetrisi): cekirdegin r_gorev_baslat kurali gorev
GOVDESININ yazma-hedefsiz olmasini sart kosar (∀y ¬HedefVar / ∀b ¬HedefBolge,
Yol-B V1 daraltmasi). `kanal_gonder` bir HedefVar uretir (Core.HedefVar.
kanal_gonder), `kanal_al` URETMEZ. Dolayisiyla SPAWN EDILEN gorev yalniz
ALICI olabilir; GONDERIM ebeveyne (main) dusmek ZORUNDADIR. Konkuranlik
cekirdekte IFADE EDILEBILIR (vakum degil) ama bu yonsel kisitla — somut
program bunu rahatca tasir.
-/

import Kemgu.Soundness.Main

namespace Kemgu.Soundness.EszamanliTanik
open Kemgu.Sem.Core Kemgu.Sem.StateTipli Kemgu.Sem.HasType
     Kemgu.Sem.LineerTamam Kemgu.Sem.RegionTamam Kemgu.Sem.Tipli
     Kemgu.Sem.SmallStep Kemgu.Sem.Kopru
     Kemgu.Soundness.Main

-- ============================================================
-- §1. Somut konkuran tanik programi
-- ============================================================

/-- Cocuk gorev govdesi: kanal 0'dan ALIR (write-free — Yol-B uyumlu). -/
def cocukGovde : Ifade := Ifade.kanalAlIf 0

/-- Ana islev govdesi:
      gorev_baslat([], kanal_al(0)) ; kanal_gonder(0, v0)
    Once kanaldan alacak cocuk gorevi baslatilir, sonra main kanala
    skaler degisken v0'i (VarId 0) gonderir. -/
def anaGovdeEszamanli : Ifade :=
  Ifade.seq
    (Ifade.gorevBaslat [] cocukGovde)
    (Ifade.kanalGonderIf 0 0)

/-- TANIK PROGRAM: tek giris islevi "ana"; bir skaler degisken (v0);
    kanal 0'in eleman tipi skaler (gonderim disiplinine uyar). -/
def eszamanliProgram : Program where
  islevler   := [("ana", anaGovdeEszamanli)]
  cevre      := [(0, Tip.scalar)]
  kanalCevre := fun _ => Tip.scalar

-- ============================================================
-- §2. Tanik lemma — IyiTipliCekirdek <konkuran program>
-- ============================================================

/-- ANA TANIK: somut konkuran program (iki gorev + kanal send/recv)
    V1 cekirdek iyi-tipliligini saglar. `bos_program_iyiTipli` sablonu
    uzerine kurulu; yedi alan tek tek dischargelanir.

    Cekirdek konkuranligi RAHATCA ifade etti: tipOk/lineerOk/bolgeOk
    govde gezintileri (gorevBaslat + kanalAl + kanalGonder) standart
    kural uygulamalariyla kapanir; Yol-B hedefsiz-govde sartlari cocuk
    govdesi `kanal_al` icin VAKUM (kanalAl ne HedefVar ne HedefBolge
    uretir). -/
theorem eszamanli_program_iyiTipli : IyiTipliCekirdek eszamanliProgram := by
  refine ⟨?_, ?_, ?_, ?_, ?_, ?_, ?_⟩
  · -- (1) tipOk : her govde tipli (Γ₀;Δ₀ altinda)
    intro p hp
    rcases List.mem_cons.mp hp with h_eq | h_nil
    · subst h_eq
      refine ⟨Tip.bos, ?_⟩
      apply HasType.t_seq
      · -- HasType Γ Δ (gorevBaslat [] (kanalAl 0)) (gorev (Δ 0))
        apply HasType.t_gorev_baslat
        exact HasType.t_kanal_al _ _ 0
      · -- HasType Γ Δ (kanalGonder 0 0) bos  ;  v0 : Δ 0 = scalar
        apply HasType.t_kanal_gonder
        rfl
    · cases h_nil
  · -- (2) lineerOk : her govde lineer-uyumlu (Λ₀ = [] — skaler nonlinear)
    intro p hp
    rcases List.mem_cons.mp hp with h_eq | h_nil
    · subst h_eq
      apply Exists.intro
      apply LineerTamam.l_seq
      · -- gorevBaslat: yakalama bos → vacuum; cocuk kanalAl Λ degismez
        apply LineerTamam.l_gorev_baslat
        · intro v hv; cases hv
        · exact LineerTamam.l_kanal_al _ _ 0
      · -- kanalGonder: v0 nonlinear (lineerOrtamGet [] 0 = none ≠ tuketildi)
        apply LineerTamam.l_kanal_gonder
        decide
    · cases h_nil
  · -- (3) bolgeOk : her govde bolge-uyumlu (Ρ₀ = [(0, varBolge 0)])
    intro p hp
    rcases List.mem_cons.mp hp with h_eq | h_nil
    · subst h_eq
      apply Exists.intro
      apply RegionTamam.r_seq
      · -- gorevBaslat: Yol-B hedefsiz-govde (kanalAl ne HedefVar ne HedefBolge)
        apply RegionTamam.r_gorev_baslat (tYeni := 1)
        · intro v hv; cases hv          -- yakalama yazilabilirligi: vacuum (yd=[])
        · intro y h; cases h            -- ∀y ¬HedefVar (kanalAl 0) y
        · intro b h; cases h            -- ∀b ¬HedefBolge (kanalAl 0) b
        · exact RegionTamam.r_kanal_al _ _ 0
        · rfl                            -- Ρ' = sahipAta Ρ [] 1 = Ρ
      · -- kanalGonder: v0'in bolgesi yazilabilir (yerel) → kanalRho'ya gecer
        apply RegionTamam.r_kanal_gonder (b := varBolge 0)
        · rfl                            -- bolgeOrtamGet Ρ 0 = some (varBolge 0)
        · rfl                            -- kategoriYazilabilir (yerel 0) = true
        · rfl                            -- Ρ' = update Ρ 0 (kanalRho 0)
    · cases h_nil
  · -- (4) capabilityOk : yetki tipi/literali yok (sozdizimsel)
    rfl
  · -- (5) sabitsureOk : sabitsure tipi yok (sozdizimsel)
    rfl
  · -- (6) noGuvensiz : guvensiz blok yok
    rfl
  · -- (7) cevreBasit : cevre tipleri deger-temsilli (scalar varsayilanlanabilir)
    intro p hp
    rcases List.mem_cons.mp hp with h_eq | h_nil
    · subst h_eq; rfl
    · cases h_nil

-- ============================================================
-- §3. Kopru + Soundness UYGULAMALARI (anlamlilik kanitlari)
-- ============================================================

/-- Konkuran programin BASLANGIC konfigurasyonu KonfTipliFull — `iyiTipli_baslangic`
    koprusu eszamanli tanik uzerinde de calisir (bos-program ornegi ile simetrik,
    fakat artik gorev + kanal iceren program icin). -/
example :
    KonfTipliFull (gammaProgram eszamanliProgram)
                  (deltaProgram eszamanliProgram)
                  (rhoBaslangic eszamanliProgram)
                  (baslangicKonf eszamanliProgram) :=
  iyiTipli_baslangic eszamanliProgram eszamanli_program_iyiTipli

/-- V3 SOUNDNESS teoremi (kemgu_soundness_v3) SOMUT KONKURAN program
    uzerinde uygulanir: iki gorev + kanal haberlesmesi yapan programin
    baslangic konfigurasyonundan ulasilan HER konfigurasyon DRF + bellek-
    guvenli + fault-suzdur. Bu, teoremin triviyal-sirali bir parca degil
    GERCEK konkuran cekirdek hakkinda oldugunun tanigidir. -/
theorem eszamanli_soundness
    (S : Konfigurasyon)
    (h_run : StepStar (baslangicKonf eszamanliProgram) S) :
    DrfHolds S ∧ MemSafe_perStep S ∧ S.fault = none :=
  kemgu_soundness_v3 eszamanliProgram eszamanli_program_iyiTipli S h_run

end Kemgu.Soundness.EszamanliTanik
