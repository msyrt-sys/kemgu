/-
KEMGU DRF Mekanize — Program ↔ Konfigurasyon Koprusu (Onarim v3 F3)
Kaynak: ADIM0_DENETIM_RAPORU.md Bolum 2.1 + FAZ_BRIFINGLERI.md F3
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz, sorry/axiom YOK

ADIM 0 Sorun 1'in kapanisi: TipKontrolOk ailesi artik GERCEK predikatlar
(HasType / LineerTamam / RegionTamam'a bagli); IyiTipli yeniden tanimlandi
(Realtime alani kaldirildi — Mehmet onayi; Capability/Sabitsure sozdizimsel
scope-guard). MIMARININ KILIT TASI: `iyiTipli_baslangic` koprusu —
iyi-tipli programin baslangic konfigurasyonu KonfTipliFull'dur. Boylece
typed_no_fault / adim_korunum zinciri Program-seviyesi hipoteze baglanir
(eski modelde bu kopru HIC yoktu — IyiTipli ile KonfTipliFull ayri
dunyalardaydi). Satisfiability tanigi da burada (vakum-hipotez sigortasi).
-/

import Kemgu.Sem.Core
import Kemgu.Sem.StateTipli
import Kemgu.Sem.HasType
import Kemgu.Sem.LineerTamam
import Kemgu.Sem.RegionTamam
import Kemgu.Sem.Tipli

namespace Kemgu.Sem.Kopru
open Kemgu.Sem.Core Kemgu.Sem.StateTipli Kemgu.Sem.HasType
     Kemgu.Sem.LineerTamam Kemgu.Sem.RegionTamam Kemgu.Sem.Tipli

-- ============================================================
-- §1. Program-ortam insalari
-- ============================================================

/-- Degiskenin baslangic bolgesi: id := VarId (degisken basina taze bolge,
    enjektif), kategori yerel. Offset-0 konvansiyonuyla konum ⟨varBolge x, 0⟩. -/
def varBolge (x : VarId) : Bolge :=
  ⟨x, BolgeKategorisi.yerel 0⟩

/-- varBolge enjektif (id bileseninden). -/
theorem varBolge_inj {x y : VarId} (h : varBolge x = varBolge y) : x = y := by
  cases h; rfl

/-- Γ₀ : program degisken tipleri. -/
def gammaProgram (Pi : Program) : TipOrtam := Pi.cevre

/-- Δ₀ : program kanal eleman tipleri. -/
def deltaProgram (Pi : Program) : KanalOrtam := Pi.kanalCevre

/-- Λ₀ : tekkez-tipli degiskenler aktif baslar. -/
def lambdaBaslangic (Pi : Program) : LineerOrtam :=
  Pi.cevre.filterMap
    (fun p => if Tip.lineerMi p.2 then some (p.1, Lineerlik.aktif) else none)

/-- Ρ₀ : her degiskene taze yerel bolge. -/
def rhoBaslangic (Pi : Program) : BolgeOrtam :=
  Pi.cevre.map (fun p => (p.1, varBolge p.1))

/-- Varsayilanlanabilir tipler (DegerTipli kurali olan kategoriler).
    mantiksal/karakter/ref/ptr/tekkez/kanal/sabitsure V1'de deger
    temsiline sahip degil → cevre'de yasak (IyiTipli.cevreBasit). -/
def tipVarsayilanlanabilir : Tip → Bool
  | Tip.bos        => true
  | Tip.scalar     => true
  | Tip.metin      => true
  | Tip.yapi _     => true
  | Tip.dizi _     => true
  | Tip.gorev _    => true
  | Tip.yetki _    => true
  | Tip.islev _ _  => true
  | _              => false

/-- Tipin varsayilan baslangic degeri. -/
def varsayilanDeger : Tip → Deger
  | Tip.bos        => Deger.birim
  | Tip.scalar     => Deger.skaler 0
  | Tip.metin      => Deger.metinDeg ⟨0, BolgeKategorisi.lit⟩ 0
  | Tip.yapi _     => Deger.yapiVal ⟨0, BolgeKategorisi.lit⟩ []
  | Tip.dizi _     => Deger.diziVal ⟨0, BolgeKategorisi.lit⟩ 0
  | Tip.gorev _    => Deger.gorevVal 0
  | Tip.yetki kay  => Deger.yetkiTok 0 kay
  | Tip.islev _ _  => Deger.closureVal 0 []
  | _              => Deger.birim

/-- Varsayilan deger tip-uyumlu (varsayilanlanabilir tipler icin). -/
theorem varsayilanDeger_tipli (Γ : TipOrtam) (Ρ : BolgeOrtam) (τ : Tip)
    (h : tipVarsayilanlanabilir τ = true) :
    DegerTipli Γ Ρ (varsayilanDeger τ) τ := by
  cases τ <;> simp [tipVarsayilanlanabilir] at h <;> first
    | exact DegerTipli.dt_birim
    | exact DegerTipli.dt_skaler 0
    | exact DegerTipli.dt_metin _ 0
    | exact DegerTipli.dt_yapi _ [] _ (DegerTipliAlanlar.dta_nil)
    | exact DegerTipli.dt_dizi _ 0 _
    | exact DegerTipli.dt_gorev 0 _
    | exact DegerTipli.dt_yetki 0 _
    | exact DegerTipli.dt_closure 0 [] _ _

/-- Ana islev govdesi (program bos ise no-op). -/
def anaGovde (Pi : Program) : Ifade :=
  match Pi.islevler with
  | []          => Ifade.sabit Deger.birim
  | (_, e) :: _ => e

/-- Baslangic konfigurasyonu S₀(Π) — tek main thread (tid 0), tum
    degisken bolgeleri thread 0'in sahipliginde, bos kanallar, bos iz. -/
def baslangicKonf (Pi : Program) : Konfigurasyon :=
  { thread   := [⟨0, anaGovde Pi, lambdaBaslangic Pi⟩],
    store    := Pi.cevre.map (fun p => (⟨varBolge p.1, 0⟩, varsayilanDeger p.2)),
    sahiplik := Pi.cevre.map (fun p => (varBolge p.1, Sahip.thread 0)),
    kanal    := [],
    zaman    := 0,
    iz       := [],
    fault    := none,
    bolge    := rhoBaslangic Pi }


-- ============================================================
-- §2. Gercek kontrol predikatlari (ADIM 0 Sorun 1 kapanisi)
-- ============================================================

/-- Tip kontrolu — GERCEK tanim: her islev govdesi Γ₀;Δ₀ altinda tipli. -/
def TipKontrolOk (Pi : Program) : Prop :=
  ∀ p ∈ Pi.islevler, ∃ τ, HasType (gammaProgram Pi) (deltaProgram Pi) p.2 τ

/-- Lineer kontrol — GERCEK tanim: her govde Λ₀'dan lineer-uyumlu. -/
def LineerKontrolOk (Pi : Program) : Prop :=
  ∀ p ∈ Pi.islevler, ∃ Λ',
    LineerTamam (gammaProgram Pi) (lambdaBaslangic Pi) p.2 Λ'

/-- Bolge atama — GERCEK tanim: her govde Ρ₀'dan bolge-uyumlu. -/
def BolgeAtamaOk (Pi : Program) : Prop :=
  ∀ p ∈ Pi.islevler, ∃ Ρ',
    RegionTamam (gammaProgram Pi) (rhoBaslangic Pi) p.2 Ρ'

/-- Yetki literali iceren ifade var mi (sozdizimsel tarama). -/
def ifadeYetkiTokIcerir : Ifade → Bool
  | .sabit (Deger.yetkiTok _ _) => true
  | .atama _ e => ifadeYetkiTokIcerir e
  | .seq a b => ifadeYetkiTokIcerir a || ifadeYetkiTokIcerir b
  | .gorevBaslat _ k => ifadeYetkiTokIcerir k
  | .guvensiz e => ifadeYetkiTokIcerir e
  | _ => false

/-- Capability scope-guard (sozdizimsel — yetki tipi/literali yok). -/
def CapabilityKontrolOk (Pi : Program) : Prop :=
  (Pi.cevre.all (fun p => match p.2 with | Tip.yetki _ => false | _ => true)
   && Pi.islevler.all (fun p => !(ifadeYetkiTokIcerir p.2))) = true

/-- Sabitsure scope-guard (sozdizimsel — sabitsure tipi yok). -/
def SabitsureKontrolOk (Pi : Program) : Prop :=
  Pi.cevre.all
    (fun p => match p.2 with | Tip.sabitsure _ => false | _ => true) = true

/-- IyiTipli(Π) — GERCEK form (F3).
    Realtime alani KALDIRILDI (V1 Ifade'de realtime yapisi yok).
    cevreBasit: cevre tipleri deger temsiline sahip olmali (V1 siniri —
    DegiskenlerBagli koprusu icin sart). -/
structure IyiTipli (Pi : Program) : Prop where
  tipOk        : TipKontrolOk Pi
  lineerOk     : LineerKontrolOk Pi
  bolgeOk      : BolgeAtamaOk Pi
  capabilityOk : CapabilityKontrolOk Pi
  sabitsureOk  : SabitsureKontrolOk Pi
  noGuvensiz   : NoGuvensiz Pi
  cevreBasit   : ∀ p ∈ Pi.cevre, tipVarsayilanlanabilir p.2 = true


-- ============================================================
-- §3. Insa lemmalari (uniform-deger assoc-list sorgulari)
-- ============================================================

/-- Ρ₀ lookup: cevre'de kayitli degisken kendi varBolge'sine esler. -/
theorem rhoBaslangic_get (cevre : List (VarId × Tip)) (x : VarId) (τ : Tip)
    (h : (x, τ) ∈ cevre) :
    bolgeOrtamGet (cevre.map (fun p => (p.1, varBolge p.1))) x
      = some (varBolge x) := by
  induction cevre with
  | nil => cases h
  | cons p rest ih =>
      by_cases hp : p.1 = x
      · simp [bolgeOrtamGet, hp]
      · cases h with
        | head => exact absurd rfl hp
        | tail _ hr => simp [bolgeOrtamGet, hp]; exact ih hr

/-- Ρ₀ lookup deger-belirlenimi: lookup bir bolge donduruyorsa o,
    anahtarin varBolge'sidir (tum girisler anahtar-belirlenimli). -/
theorem rhoBaslangic_get_inv (cevre : List (VarId × Tip)) (x : VarId)
    (b : Bolge)
    (h : bolgeOrtamGet (cevre.map (fun p => (p.1, varBolge p.1))) x = some b) :
    b = varBolge x := by
  induction cevre with
  | nil => simp [bolgeOrtamGet] at h
  | cons p rest ih =>
      by_cases hp : p.1 = x
      · simp [bolgeOrtamGet, hp] at h
        exact h.symm
      · simp [bolgeOrtamGet, hp] at h
        exact ih h

/-- Sahiplik₀ lookup deger-belirlenimi: tum girisler thread 0. -/
theorem sahiplikBaslangic_get_inv (cevre : List (VarId × Tip)) (b : Bolge)
    (v : Sahip)
    (h : sahiplikGet (cevre.map (fun p => (varBolge p.1, Sahip.thread 0))) b
           = some v) :
    v = Sahip.thread 0 := by
  induction cevre with
  | nil => simp [sahiplikGet] at h
  | cons p rest ih =>
      by_cases hp : varBolge p.1 = b
      · simp [sahiplikGet, hp] at h
        exact h.symm
      · simp [sahiplikGet, hp] at h
        exact ih h

/-- Sahiplik₀ lookup: cevre'de kayitli degiskenin bolgesi thread 0'da. -/
theorem sahiplikBaslangic_get (cevre : List (VarId × Tip)) (x : VarId) (τ : Tip)
    (h : (x, τ) ∈ cevre) :
    sahiplikGet (cevre.map (fun p => (varBolge p.1, Sahip.thread 0)))
      (varBolge x) = some (Sahip.thread 0) := by
  induction cevre with
  | nil => cases h
  | cons p rest ih =>
      by_cases hp : varBolge p.1 = varBolge x
      · simp [sahiplikGet, hp]
      · cases h with
        | head => exact absurd rfl hp
        | tail _ hr => simp [sahiplikGet, hp]; exact ih hr

/-- Ρ₀ lookup → cevre uyeligi. -/
theorem rhoBaslangic_get_mem (cevre : List (VarId × Tip)) (y : VarId)
    (b : Bolge)
    (h : bolgeOrtamGet (cevre.map (fun p => (p.1, varBolge p.1))) y = some b) :
    ∃ τ, (y, τ) ∈ cevre := by
  induction cevre with
  | nil => simp [bolgeOrtamGet] at h
  | cons p rest ih =>
      by_cases hp : p.1 = y
      · refine ⟨p.2, ?_⟩
        rw [← hp]
        exact List.Mem.head _
      · simp only [List.map_cons, bolgeOrtamGet, hp, if_false] at h
        obtain ⟨τ, h_t⟩ := ih h
        exact ⟨τ, List.Mem.tail _ h_t⟩

/-- Store₀ lookup: tipOrtamGet ile AYNI liste sirasi uzerinden yurudugu
    icin ilk eslesen tip ile store degeri hizali. -/
theorem storeBaslangic_get (cevre : List (VarId × Tip)) (x : VarId) (τ : Tip)
    (h : tipOrtamGet cevre x = some τ) :
    konumGet (cevre.map (fun p => (⟨varBolge p.1, 0⟩, varsayilanDeger p.2)))
      ⟨varBolge x, 0⟩ = some (varsayilanDeger τ) := by
  induction cevre with
  | nil => simp [tipOrtamGet] at h
  | cons p rest ih =>
      by_cases hp : p.1 = x
      · subst hp
        simp [tipOrtamGet] at h
        subst h
        simp [konumGet]
      · have h_ne : (⟨varBolge p.1, 0⟩ : Konum) ≠ ⟨varBolge x, 0⟩ := by
          intro he
          exact hp (varBolge_inj (congrArg Konum.bolge he))
        simp [tipOrtamGet, hp] at h
        simp [konumGet, h_ne]
        exact ih h


-- ============================================================
-- §4. KOPRU TEOREMI — iyiTipli_baslangic (mimarinin kilit tasi)
-- ============================================================

/-- IyiTipli(Π) ⟹ KonfTipliFull(S₀(Π)) — Program-seviyesi tip kontrolu,
    konfigurasyon-seviyesi tipliligi KURAR. typed_no_fault / adim_korunum
    zinciri boylece kagit-formdaki "IyiTipli(Π) ⟹ ..." iddialarina baglanir. -/
theorem iyiTipli_baslangic (Pi : Program) (h_iyi : IyiTipli Pi) :
    KonfTipliFull (gammaProgram Pi) (deltaProgram Pi)
                  (rhoBaslangic Pi) (baslangicKonf Pi) := by
  refine ⟨?_, ?_, ?_, ?_, rfl, rfl, ?_, ?_, ?_, ?_, ?_⟩
  · -- (1) SigmaTipli
    intro k v h_kv
    rcases List.mem_map.mp h_kv with ⟨p, h_p, h_eq⟩
    obtain ⟨h_k, h_v⟩ := Prod.mk.injEq .. ▸ h_eq
    refine ⟨p.2, ?_, ⟨p.1, ?_⟩⟩
    · rw [← h_v]
      exact varsayilanDeger_tipli _ _ p.2 (h_iyi.cevreBasit p h_p)
    · rw [← h_k]
      show bolgeOrtamGet (rhoBaslangic Pi) p.1 = some (varBolge p.1)
      exact rhoBaslangic_get Pi.cevre p.1 p.2 h_p
  · -- (2) ThreadTipliFull (tek main thread; per-thread lineer = Λ₀)
    intro ctx h_ctx
    rcases List.mem_cons.mp h_ctx with h_eq | h_nil
    · subst h_eq
      show ∃ τ Λ' Ρ', Typed _ _ (lambdaBaslangic Pi) _ (anaGovde Pi) τ Λ' Ρ'
      unfold anaGovde
      cases h_is : Pi.islevler with
      | nil =>
          exact ⟨Tip.bos, lambdaBaslangic Pi, rhoBaslangic Pi,
            ⟨HasType.t_sabit _ _ _ _ DegerTipli.dt_birim,
             LineerTamam.l_sabit _ _ _,
             RegionTamam.r_sabit _ _ _⟩⟩
      | cons p rest =>
          have h_mem : p ∈ Pi.islevler := h_is ▸ List.Mem.head _
          obtain ⟨τ, h_t⟩ := h_iyi.tipOk p h_mem
          obtain ⟨Λ', h_l⟩ := h_iyi.lineerOk p h_mem
          obtain ⟨Ρ', h_r⟩ := h_iyi.bolgeOk p h_mem
          exact ⟨τ, Λ', Ρ', ⟨h_t, h_l, h_r⟩⟩
    · cases h_nil
  · -- (3) SahiplikTutarli
    intro b t h_b
    have h_mem := sahiplikGet_mem _ b _ h_b
    rcases List.mem_map.mp h_mem with ⟨p, h_p, h_eq⟩
    obtain ⟨h_bb, _⟩ := Prod.mk.injEq .. ▸ h_eq
    refine ⟨p.1, ?_⟩
    rw [← h_bb]
    exact rhoBaslangic_get Pi.cevre p.1 p.2 h_p
  · -- (4) KanalTutarli (bos kanal listesi)
    intro kd h_kd
    cases h_kd
  · -- (7) FrozenKategoriTutarli
    intro x b h_b
    have h_bvar : b = varBolge x := rhoBaslangic_get_inv Pi.cevre x b h_b
    constructor
    · intro h_fr
      have := sahiplikBaslangic_get_inv Pi.cevre b _ h_fr
      nomatch this
    · intro h_kat
      rw [h_bvar] at h_kat
      nomatch h_kat
  · -- (8) HedefVarSahipligi (tum kayitli bolgeler thread 0'da)
    intro ctx h_ctx y _h_hv b h_b _h_yaz
    rcases List.mem_cons.mp h_ctx with h_eq | h_nil
    · subst h_eq
      have h_bvar : b = varBolge y := rhoBaslangic_get_inv Pi.cevre y b h_b
      obtain ⟨τ, h_y⟩ := rhoBaslangic_get_mem Pi.cevre y b h_b
      rw [h_bvar]
      show sahiplikGet (baslangicKonf Pi).sahiplik (varBolge y)
             = some (Sahip.thread 0)
      exact sahiplikBaslangic_get Pi.cevre y τ h_y
    · cases h_nil
  · -- (9) HedefBolgeSahipligi (kayitli bolge-literalleri de thread 0'da)
    intro ctx h_ctx b _h_hb h_kayitli _h_yaz
    rcases List.mem_cons.mp h_ctx with h_eq | h_nil
    · subst h_eq
      obtain ⟨x, h_x⟩ := h_kayitli
      have h_bvar : b = varBolge x := rhoBaslangic_get_inv Pi.cevre x b h_x
      obtain ⟨τ, h_yx⟩ := rhoBaslangic_get_mem Pi.cevre x b h_x
      rw [h_bvar]
      show sahiplikGet (baslangicKonf Pi).sahiplik (varBolge x)
             = some (Sahip.thread 0)
      exact sahiplikBaslangic_get Pi.cevre x τ h_yx
    · cases h_nil
  · -- (10) DegiskenlerBagli
    intro x τ h_x
    refine ⟨varBolge x, varsayilanDeger τ, ?_, ?_, ?_⟩
    · exact rhoBaslangic_get Pi.cevre x τ (tipOrtamGet_mem Pi.cevre x τ h_x)
    · exact storeBaslangic_get Pi.cevre x τ h_x
    · have h_mem := tipOrtamGet_mem Pi.cevre x τ h_x
      exact varsayilanDeger_tipli _ _ τ (h_iyi.cevreBasit (x, τ) h_mem)
  · -- (11) KanalTransit (bos kanal listesi — vacuous)
    intro kd h_kd
    cases h_kd


-- ============================================================
-- §5. Satisfiability tanigi (vakum-hipotez sigortasi)
-- ============================================================

/-- Bos program iyi-tipli (tum kosullar bos liste uzerinde). -/
theorem bos_program_iyiTipli : IyiTipli { islevler := [] } := by
  refine ⟨?_, ?_, ?_, ?_, ?_, ?_, ?_⟩
  · intro p h; cases h
  · intro p h; cases h
  · intro p h; cases h
  · rfl
  · rfl
  · rfl
  · intro p h; cases h

/-- KonfTipliFull ORNEKLENEBILIR — bos programin baslangic konfigurasyonu
    taniktir (vakum-hipotez elestirisine somut cevap). -/
example : KonfTipliFull (gammaProgram { islevler := [] })
    (deltaProgram { islevler := [] })
    (rhoBaslangic { islevler := [] })
    (baslangicKonf { islevler := [] }) :=
  iyiTipli_baslangic _ bos_program_iyiTipli

end Kemgu.Sem.Kopru
