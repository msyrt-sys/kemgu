/-
KEMGU — CT HESABI ile Sem/Core ARASINDAKI KOPRU (D-333)
Kaynak (kagit formel): belgeler/KEMGU_Sabitsure_Spec_V1.md (CT001/CT003)
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz, sorry/axiom YOK

═══════════════════════════════════════════════════════════════════════
NE YAPIYOR
═══════════════════════════════════════════════════════════════════════
D-330 `SideChannel/CT.lean`'de `ct_ni`yi ISPATLADI ama AYRI bir cekirdek
hesapta. D-332 `Sem/Core`'a `eger` + `dalOl` ekledi, boylece hedef onerme
ANA MODELDE YAZILABILIR hale geldi. Bu dosya EKSIK HALKAYI kapatir:

  CT.Ifade   --gom-->        Core.Ifade
  CT.Calis   --gomme_sim-->  Core.StepStar
  CT.Gozlem  --gomGoz-->     NonInterference.GozlemOlay

ve `ct_ni`yi ANA MODELE TASIR (`kopru_ni`): CT-tipli bir program Core'da
kosturuldugunda, dusuk-esdeger iki baslangic store'u AYNI GOZLEM IZINI
uretir — DAL KARARLARI (`gDal`) DAHIL.

═══════════════════════════════════════════════════════════════════════
KAPSAM ve ACIKCA BORC (durustluk paketi)
═══════════════════════════════════════════════════════════════════════
1. **`topla` ARTIK KAPSAM ICINDE (D-334).** D-333'te Core'da aritmetik
   olmadigi icin disaridaydi; D-334 `Ifade.topla` + `sToplaTamam` +
   `sToplaCongSol/Sag` + `t_topla`/`l_topla`/`r_topla` ekledi. Gomme
   artik CT'nin TUM ifade bicimlerini kapsar (`sabit`, `degisken`,
   `topla`, `sabitDeg`, `sira`, `eger`) — tek kisit asagidaki `Sadik`.
2. **`Sadik` (deger-sadakati).** OLCULEN ayrisma: CT'de `x = e` DEGER
   olarak v dondurur, Core'da `sAtamaTamam` `birim` dondurur. Bu yuzden
   atama, degeri KULLANILAN konumlara giremez: `GomOk` bir atamanin
   SAG TARAFININ ve bir `eger` KOSULUNUN `Sadik` olmasini sart kosar.
   Sart olmasaydi `eger (x = e) ...` programinda CT `v ≠ 0`e, Core
   `degerDogruMu birim = false`e dallanirdi — SIMULASYON COKERDI.
   Atama deyim konumunda (ornegin `x = e; ...`) SERBESTTIR.
3. **Sonlu degisken kumesi.** `CT.Store = Ad → Int` TOPLAM; Core'un
   `store`/`bolge` alanlari sonlu assoc listelerdir → kopru `V : List Ad`
   ile parametriktir, `Kapsar V e` yan-kosulu tasinir.
4. **Tek thread.** Gomulen programda `gorev_baslat` YOK → spawn yok →
   FIX-F cerceve ayrisimi daima sol kola duser. Kopru ESZAMANLILIK
   HAKKINDA BIR SEY SOYLEMEZ (CT hesabi da soylemiyordu).
5. **Iz yonu.** CT izi yeni-SONDA, Core yeni-BASTA biriktirir; karsilik
   `gomGozIz` icindeki `reverse` ile kurulur.

TASARIM NOTU — `KRun` NEDEN VAR:
  Cong-yukseltmeyi (`a` adim atiyorsa `seq a b` de atar) dogrudan
  `StepStar` uzerinde yapmak ARA konfigurasyonlarin sekli hakkinda bir
  korunum lemmasi ister. Bunun yerine "yalniz K-sekilli konfigurasyonlardan
  gecen" bir kapanis (`KRun`) tanimlandi; cong-yukseltme onun uzerinde
  TEK TUMEVARIMDIR ve `krun_stepStar` ile GERCEK `StepStar`a duser.
  Yani kolaylik bir VARSAYIM DEGIL, ispatlanmis bir alt-iliskidir.
═══════════════════════════════════════════════════════════════════════
-/

import Kemgu.Sem.SmallStep
import Kemgu.SideChannel.CT
import Kemgu.SideChannel.NonInterference

namespace Kemgu.SideChannel.CTKopru

open Kemgu.Sem.Core Kemgu.Sem.SmallStep Kemgu.Sem.StateTipli
     Kemgu.SideChannel.NonInterference

-- ============================================================
-- §1. Gomme — sozdizimi
-- ============================================================

/-- `Sadik e`: e'nin Core'daki degeri, CT'deki degerinin gommesidir.
    Atama DISARIDA (Core `birim` doner, CT atanani doner) — Kapsam 2. -/
inductive Sadik : CT.Ifade → Prop where
  | s_sabit (n : Int) : Sadik (.sabit n)
  | s_degisken (x : CT.Ad) : Sadik (.degisken x)
  /-- `a; b`nin degeri b'den gelir → a'nin sadakati GEREKMEZ. -/
  | s_sira (a b : CT.Ifade) : Sadik b → Sadik (.sira a b)
  | s_eger (k d y : CT.Ifade) : Sadik d → Sadik y → Sadik (.eger k d y)
  /-- D-334: toplamin degeri `skaler (v1+v2)` — sadik. -/
  | s_topla (a b : CT.Ifade) : Sadik (.topla a b)
  /-- D-335: `esles`in degeri secilen koldan gelir. -/
  | s_esles (s : CT.Ifade) (n : Int) (d y : CT.Ifade) :
      Sadik d → Sadik y → Sadik (.esles s n d y)
  -- NOT: `iken` SADIK DEGILDIR — CT'de dongu 0 dondurur, Core'da
  -- (acilmis `eger`in yanlis dali) `birim`. Dongu bir DEYIMDIR; degeri
  -- kullanilan bir konuma giremez (`sabitDeg` ile ayni sinif).

/-- Gomulebilir CT ifadeleri. D-334'ten beri CT'nin TUM bicimleri
    gomulebilir; tek kisit `Sadik` (Kapsam 2). -/
inductive GomOk : CT.Ifade → Prop where
  | g_sabit (n : Int) : GomOk (.sabit n)
  | g_degisken (x : CT.Ad) : GomOk (.degisken x)
  /-- Atamanin SAG TARAFI deger-sadik olmali (`x = (y = 3)` yasak). -/
  | g_atama (x : CT.Ad) (e : CT.Ifade) :
      GomOk e → Sadik e → GomOk (.sabitDeg x e)
  | g_sira (a b : CT.Ifade) : GomOk a → GomOk b → GomOk (.sira a b)
  /-- `eger`in KOSULU deger-sadik olmali — dal karari ona bagli. -/
  | g_eger (k d y : CT.Ifade) :
      GomOk k → Sadik k → GomOk d → GomOk y → GomOk (.eger k d y)
  /-- D-334: `topla` ARTIK GOMULEBILIR (Core'a `Ifade.topla` eklendi).
      Her iki operand `Sadik` olmali — `sToplaTamam` operandlarin
      `sabit (skaler _)` olmasini ister. -/
  | g_topla (a b : CT.Ifade) :
      GomOk a → Sadik a → GomOk b → Sadik b → GomOk (.topla a b)
  /-- D-335: dongu. Kosul `Sadik` olmali (dal karari ona bagli). -/
  | g_iken (k g : CT.Ifade) :
      GomOk k → Sadik k → GomOk g → GomOk (.iken k g)
  /-- D-335: desen eslemesi. Skrutin `Sadik` olmali (literal ile
      karsilastirilir). -/
  | g_esles (s : CT.Ifade) (n : Int) (d y : CT.Ifade) :
      GomOk s → Sadik s → GomOk d → GomOk y → GomOk (.esles s n d y)

/-- CT ifadesi → Core ifadesi. D-334'ten beri TOPLAM (her bicim icin
    gercek bir karsilik var; tikac dal YOK). -/
def gom : CT.Ifade → Ifade
  | .sabit n        => .sabit (.skaler n)
  | .degisken x     => .tanim x
  | .sabitDeg x e   => .atama x (gom e)
  | .sira a b       => .seq (gom a) (gom b)
  | .eger k d y     => .eger (gom k) (gom d) (gom y)
  | .topla a b      => .topla (gom a) (gom b)   -- D-334
  | .iken k g       => .iken (gom k) (gom g)    -- D-335
  | .esles s n d y  => .esles (gom s) n (gom d) (gom y)

/-- Ifadenin degiskenleri V icinde mi (Kapsam 3). -/
inductive Kapsar (V : List CT.Ad) : CT.Ifade → Prop where
  | k_sabit (n : Int) : Kapsar V (.sabit n)
  | k_degisken (x : CT.Ad) : x ∈ V → Kapsar V (.degisken x)
  | k_atama (x : CT.Ad) (e : CT.Ifade) :
      x ∈ V → Kapsar V e → Kapsar V (.sabitDeg x e)
  | k_sira (a b : CT.Ifade) : Kapsar V a → Kapsar V b → Kapsar V (.sira a b)
  | k_eger (k d y : CT.Ifade) :
      Kapsar V k → Kapsar V d → Kapsar V y → Kapsar V (.eger k d y)
  | k_topla (a b : CT.Ifade) : Kapsar V a → Kapsar V b → Kapsar V (.topla a b)
  | k_iken (k g : CT.Ifade) : Kapsar V k → Kapsar V g → Kapsar V (.iken k g)
  | k_esles (s : CT.Ifade) (n : Int) (d y : CT.Ifade) :
      Kapsar V s → Kapsar V d → Kapsar V y → Kapsar V (.esles s n d y)

-- ============================================================
-- §2. Gomme — durum (bolge / sahiplik / store)
-- ============================================================

/-- Gomen thread (Kapsam 4: tek thread). -/
abbrev t0 : ThreadId := 0

/-- x degiskeninin bolgesi. Kategori YAZILABILIR olmali (sAtamaTamam);
    `yerel` secildi. Bolge KIMLIGI = degisken kimligi — `konumGet` ve
    `sahiplikGet` zaten id-anahtarlidir (Onarim v3 karari), dolayisiyla
    bu esleme ayriktir (farkli degisken → farkli bolge id). -/
def bol (x : CT.Ad) : Bolge := ⟨x, BolgeKategorisi.yerel 0⟩

def rhoOf (V : List CT.Ad) : BolgeOrtam := V.map (fun x => (x, bol x))
def sahOf (V : List CT.Ad) : Sahiplik := V.map (fun x => (bol x, Sahip.thread t0))

theorem rhoOf_get {V : List CT.Ad} {x : CT.Ad} (h : x ∈ V) :
    bolgeOrtamGet (rhoOf V) x = some (bol x) := by
  induction V with
  | nil => cases h
  | cons w rest ih =>
      show bolgeOrtamGet ((w, bol w) :: rest.map (fun y => (y, bol y))) x = _
      by_cases hw : w = x
      · subst hw; simp [bolgeOrtamGet]
      · rw [bolgeOrtamGet, if_neg hw]
        exact ih (by cases h with
                     | head => exact absurd rfl hw
                     | tail _ h' => exact h')

theorem sahOf_get {V : List CT.Ad} {x : CT.Ad} (h : x ∈ V) :
    sahiplikGet (sahOf V) (bol x) = some (Sahip.thread t0) := by
  induction V with
  | nil => cases h
  | cons w rest ih =>
      show sahiplikGet ((bol w, Sahip.thread t0)
              :: rest.map (fun y => (bol y, Sahip.thread t0))) (bol x) = _
      by_cases hw : w = x
      · subst hw; simp [sahiplikGet]
      · have hid : ¬ ((bol w).id = (bol x).id) := hw
        rw [sahiplikGet, if_neg hid]
        exact ih (by cases h with
                     | head => exact absurd rfl hw
                     | tail _ h' => exact h')

/-- Store UYUMU: Core store'u, CT store'unu V uzerinde GERCEKLER.
    GENISLETILEBILIR (extensional) tanim SART: `sAtamaTamam` store'a
    ONE EKLER (uzerine yazmaz), yani sozdizimsel esitlik yasamaz. -/
def StoreUyum (V : List CT.Ad) (s : CT.Store) (sigma : Store) : Prop :=
  ∀ x ∈ V, konumGet sigma ⟨bol x, 0⟩ = some (.skaler (s x))

/-- Yazma uyumu korur: Core'un prepend'i CT'nin `yaz`ina karsilik gelir. -/
theorem storeUyum_yaz {V : List CT.Ad} {s : CT.Store} {sigma : Store}
    (h : StoreUyum V s sigma) (x : CT.Ad) (n : Int) :
    StoreUyum V (CT.yaz s x n) ((⟨bol x, 0⟩, Deger.skaler n) :: sigma) := by
  intro y hy
  show (if (bol x).id = (bol y).id ∧ (0 : Nat) = 0 then some (Deger.skaler n)
        else konumGet sigma ⟨bol y, 0⟩) = some (.skaler (CT.yaz s x n y))
  by_cases hxy : y = x
  · subst hxy
    rw [if_pos ⟨rfl, rfl⟩]
    simp [CT.yaz]
  · have hid : ¬ ((bol x).id = (bol y).id ∧ (0 : Nat) = 0) := by
      intro hc; exact hxy hc.1.symm
    rw [if_neg hid, h y hy]
    simp [CT.yaz, hxy]

/-- CT store'unun Core GERCEKLEMESI. VAKUM DENETIMI: `StoreUyum`
    hipotezi HER CT store'u icin saglanabilir — yani `kopru_ni`
    "boyle bir sigma yoksa" diye bos gecmez. -/
def storeOf (V : List CT.Ad) (s : CT.Store) : Store :=
  V.map (fun x => (⟨bol x, 0⟩, Deger.skaler (s x)))

theorem storeUyum_storeOf (V : List CT.Ad) (s : CT.Store) :
    StoreUyum V s (storeOf V s) := by
  intro x hx
  induction V with
  | nil => cases hx
  | cons w rest ih =>
      show (if (bol w).id = (bol x).id ∧ (0 : Nat) = 0
            then some (Deger.skaler (s w))
            else konumGet (storeOf rest s) ⟨bol x, 0⟩) = _
      by_cases hw : w = x
      · subst hw; rw [if_pos ⟨rfl, rfl⟩]
      · rw [if_neg (by intro hc; exact hw hc.1)]
        exact ih (by cases hx with
                     | head => exact absurd rfl hw
                     | tail _ h' => exact h')

-- ============================================================
-- §3. K-sekilli konfigurasyon + K-kosumu
-- ============================================================

/-- Kosum boyunca degisen alanlar. `sahiplik`/`bolge`/`kanal` SABITTIR —
    gomulen programda dondur/kanal/gorev yoktur. -/
structure KDurum where
  store : Store
  ifade : Ifade
  iz    : Iz
  zaman : Zaman

def K (V : List CT.Ad) (d : KDurum) : Konfigurasyon :=
  { thread   := [⟨t0, d.ifade, []⟩]
    store    := d.store
    sahiplik := sahOf V
    kanal    := []
    zaman    := d.zaman
    iz       := d.iz
    fault    := none
    bolge    := rhoOf V }

/-- K-sekilli konfigurasyonlardan gecen reduksiyon kapanisi. -/
inductive KRun (V : List CT.Ad) : KDurum → KDurum → Prop where
  | refl (d : KDurum) : KRun V d d
  | adim (d d1 d' : KDurum) :
      Step (K V d) (K V d1) → KRun V d1 d' → KRun V d d'

/-- KRun GERCEK bir kosumdur: `StepStar`in alt-iliskisi. -/
theorem krun_stepStar {V : List CT.Ad} {d d' : KDurum}
    (h : KRun V d d') : StepStar (K V d) (K V d') := by
  induction h with
  | refl d => exact StepStar.refl _
  | adim d d1 d' h1 _ ih => exact StepStar.step _ _ _ h1 ih

theorem krun_trans {V : List CT.Ad} {d d1 d' : KDurum}
    (h1 : KRun V d d1) (h2 : KRun V d1 d') : KRun V d d' := by
  induction h1 with
  | refl _ => exact h2
  | adim a b _ hs _ ih => exact KRun.adim a b d' hs (ih h2)

-- ============================================================
-- §4. Congruence yukseltmeleri
-- Her biri: ilgili Core cong kuralini TEK adima uygula, KRun uzerinde
-- tumevarimla kosuma yay. FIX-F yan-kosulu daima `Or.inl rfl`
-- (Kapsam 4: spawn yok).
-- ============================================================

theorem krun_seq {V : List CT.Ad} (b : Ifade) {d d' : KDurum}
    (h : KRun V d d') :
    KRun V { d with ifade := .seq d.ifade b }
           { d' with ifade := .seq d'.ifade b } := by
  induction h with
  | refl _ => exact KRun.refl _
  | adim dA dB _ hs _ ih =>
      refine KRun.adim _ { dB with ifade := .seq dB.ifade b } _ ?_ ih
      exact Step.sSeqCong (K V { dA with ifade := .seq dA.ifade b })
        (K V { dB with ifade := .seq dB.ifade b }) (K V dA) (K V dB)
        [] [] [] ⟨t0, .seq dA.ifade b, []⟩ ⟨t0, dB.ifade, []⟩
        dA.ifade dB.ifade b rfl rfl rfl hs rfl rfl rfl (Or.inl rfl) rfl

theorem krun_atama {V : List CT.Ad} (x : VarId) {d d' : KDurum}
    (h : KRun V d d') :
    KRun V { d with ifade := .atama x d.ifade }
           { d' with ifade := .atama x d'.ifade } := by
  induction h with
  | refl _ => exact KRun.refl _
  | adim dA dB _ hs _ ih =>
      refine KRun.adim _ { dB with ifade := .atama x dB.ifade } _ ?_ ih
      exact Step.sAtamaCong (K V { dA with ifade := .atama x dA.ifade })
        (K V { dB with ifade := .atama x dB.ifade }) (K V dA) (K V dB)
        [] [] [] ⟨t0, .atama x dA.ifade, []⟩ ⟨t0, dB.ifade, []⟩
        x dA.ifade dB.ifade rfl rfl rfl hs rfl rfl rfl (Or.inl rfl) rfl

theorem krun_eger {V : List CT.Ad} (dd yy : Ifade) {d d' : KDurum}
    (h : KRun V d d') :
    KRun V { d with ifade := .eger d.ifade dd yy }
           { d' with ifade := .eger d'.ifade dd yy } := by
  induction h with
  | refl _ => exact KRun.refl _
  | adim dA dB _ hs _ ih =>
      refine KRun.adim _ { dB with ifade := .eger dB.ifade dd yy } _ ?_ ih
      exact Step.sEgerCong (K V { dA with ifade := .eger dA.ifade dd yy })
        (K V { dB with ifade := .eger dB.ifade dd yy }) (K V dA) (K V dB)
        [] [] [] ⟨t0, .eger dA.ifade dd yy, []⟩ ⟨t0, dB.ifade, []⟩
        dA.ifade dB.ifade dd yy rfl rfl rfl hs rfl rfl rfl (Or.inl rfl) rfl

theorem krun_topla_sol {V : List CT.Ad} (b : Ifade) {d d' : KDurum}
    (h : KRun V d d') :
    KRun V { d with ifade := .topla d.ifade b }
           { d' with ifade := .topla d'.ifade b } := by
  induction h with
  | refl _ => exact KRun.refl _
  | adim dA dB _ hs _ ih =>
      refine KRun.adim _ { dB with ifade := .topla dB.ifade b } _ ?_ ih
      exact Step.sToplaCongSol (K V { dA with ifade := .topla dA.ifade b })
        (K V { dB with ifade := .topla dB.ifade b }) (K V dA) (K V dB)
        [] [] [] ⟨t0, .topla dA.ifade b, []⟩ ⟨t0, dB.ifade, []⟩
        dA.ifade dB.ifade b rfl rfl rfl hs rfl rfl rfl (Or.inl rfl) rfl

theorem krun_topla_sag {V : List CT.Ad} (v : Deger) {d d' : KDurum}
    (h : KRun V d d') :
    KRun V { d with ifade := .topla (.sabit v) d.ifade }
           { d' with ifade := .topla (.sabit v) d'.ifade } := by
  induction h with
  | refl _ => exact KRun.refl _
  | adim dA dB _ hs _ ih =>
      refine KRun.adim _ { dB with ifade := .topla (.sabit v) dB.ifade } _ ?_ ih
      exact Step.sToplaCongSag (K V { dA with ifade := .topla (.sabit v) dA.ifade })
        (K V { dB with ifade := .topla (.sabit v) dB.ifade }) (K V dA) (K V dB)
        [] [] [] ⟨t0, .topla (.sabit v) dA.ifade, []⟩ ⟨t0, dB.ifade, []⟩
        v dA.ifade dB.ifade rfl rfl rfl hs rfl rfl rfl (Or.inl rfl) rfl

theorem krun_esles {V : List CT.Ad} (n : Int) (dd yy : Ifade) {d d' : KDurum}
    (h : KRun V d d') :
    KRun V { d with ifade := .esles d.ifade n dd yy }
           { d' with ifade := .esles d'.ifade n dd yy } := by
  induction h with
  | refl _ => exact KRun.refl _
  | adim dA dB _ hs _ ih =>
      refine KRun.adim _ { dB with ifade := .esles dB.ifade n dd yy } _ ?_ ih
      exact Step.sEslesCong (K V { dA with ifade := .esles dA.ifade n dd yy })
        (K V { dB with ifade := .esles dB.ifade n dd yy }) (K V dA) (K V dB)
        [] [] [] ⟨t0, .esles dA.ifade n dd yy, []⟩ ⟨t0, dB.ifade, []⟩
        dA.ifade dB.ifade n dd yy rfl rfl rfl hs rfl rfl rfl (Or.inl rfl) rfl

-- ============================================================
-- §5. Taban adimlar (tek Step, K → K)
-- ============================================================

theorem adim_oku (V : List CT.Ad) (sigma : Store) (x : CT.Ad) (n : Int)
    (iz : Iz) (z : Zaman) (hx : x ∈ V)
    (hv : konumGet sigma ⟨bol x, 0⟩ = some (.skaler n)) :
    Step (K V ⟨sigma, .tanim x, iz, z⟩)
         (K V ⟨sigma, .sabit (.skaler n),
               .memOku t0 ⟨bol x, 0⟩ (.skaler n) :: iz, z + 1⟩) :=
  Step.sVarOku _ _ [] [] ⟨t0, .tanim x, []⟩ x (bol x) (.skaler n)
    rfl rfl (rhoOf_get hx) hv rfl

theorem adim_yaz (V : List CT.Ad) (sigma : Store) (x : CT.Ad) (n : Int)
    (iz : Iz) (z : Zaman) (hx : x ∈ V) :
    Step (K V ⟨sigma, .atama x (.sabit (.skaler n)), iz, z⟩)
         (K V ⟨(⟨bol x, 0⟩, Deger.skaler n) :: sigma, .sabit .birim,
               .memYaz t0 ⟨bol x, 0⟩ (.skaler n) :: iz, z + 1⟩) :=
  Step.sAtamaTamam _ _ [] [] ⟨t0, .atama x (.sabit (.skaler n)), []⟩
    x (.skaler n) (bol x) rfl rfl (rhoOf_get hx) (sahOf_get hx) rfl

theorem adim_seq_atla (V : List CT.Ad) (sigma : Store) (v : Deger)
    (b : Ifade) (iz : Iz) (z : Zaman) :
    Step (K V ⟨sigma, .seq (.sabit v) b, iz, z⟩)
         (K V ⟨sigma, b, iz, z + 1⟩) :=
  Step.sSeqAtla _ _ [] [] ⟨t0, .seq (.sabit v) b, []⟩ v b rfl rfl rfl

theorem adim_dal (V : List CT.Ad) (sigma : Store) (n : Int)
    (dd yy : Ifade) (iz : Iz) (z : Zaman) :
    Step (K V ⟨sigma, .eger (.sabit (.skaler n)) dd yy, iz, z⟩)
         (K V ⟨sigma, (if degerDogruMu (.skaler n) then dd else yy),
               .dalOl t0 (degerDogruMu (.skaler n)) :: iz, z + 1⟩) :=
  Step.sEgerSec _ _ [] [] ⟨t0, .eger (.sabit (.skaler n)) dd yy, []⟩
    (.skaler n) dd yy (degerDogruMu (.skaler n)) rfl rfl rfl rfl

/-- D-334: iki skaler deger → toplam. Olay uretmez (CT'nin `c_topla`si
    da iz katkisi yapmaz — sadece alt-ifadelerin izleri birlesir). -/
theorem adim_topla (V : List CT.Ad) (sigma : Store) (n1 n2 : Int)
    (iz : Iz) (z : Zaman) :
    Step (K V ⟨sigma, .topla (.sabit (.skaler n1)) (.sabit (.skaler n2)), iz, z⟩)
         (K V ⟨sigma, .sabit (.skaler (n1 + n2)), iz, z + 1⟩) :=
  Step.sToplaTamam _ _ [] []
    ⟨t0, .topla (.sabit (.skaler n1)) (.sabit (.skaler n2)), []⟩ n1 n2 rfl rfl rfl

/-- D-335: dongu ACILMASI (olaysiz yapisal adim). -/
theorem adim_iken_ac (V : List CT.Ad) (sigma : Store) (k g : Ifade)
    (iz : Iz) (z : Zaman) :
    Step (K V ⟨sigma, .iken k g, iz, z⟩)
         (K V ⟨sigma, .eger k (.seq g (.iken k g)) (.sabit .birim), iz, z + 1⟩) :=
  Step.sIkenAc _ _ [] [] ⟨t0, .iken k g, []⟩ k g rfl rfl rfl

/-- D-335: literal desen secimi (`dalOl` uretir). -/
theorem adim_esles (V : List CT.Ad) (sigma : Store) (m n : Int)
    (dd yy : Ifade) (iz : Iz) (z : Zaman) :
    Step (K V ⟨sigma, .esles (.sabit (.skaler m)) n dd yy, iz, z⟩)
         (K V ⟨sigma, (if decide (m = n) then dd else yy),
               .dalOl t0 (decide (m = n)) :: iz, z + 1⟩) :=
  Step.sEslesSec _ _ [] [] ⟨t0, .esles (.sabit (.skaler m)) n dd yy, []⟩
    m n dd yy (decide (m = n)) rfl rfl rfl rfl

/-- Dal testi CT ile BIREBIR: `degerDogruMu (skaler n) ↔ n ≠ 0`. -/
theorem dogruMu_skaler (n : Int) : degerDogruMu (.skaler n) = decide (n ≠ 0) := rfl

-- ============================================================
-- §6. Gozlem gommesi
-- ============================================================

def gomGoz : CT.Gozlem → GozlemOlay
  | .oOku x   => .gOku t0 ⟨bol x, 0⟩
  | .oYaz x   => .gYaz t0 ⟨bol x, 0⟩
  | .oDal a   => .gDal t0 a

/-- CT izi (yeni-SONDA) → Core gozlem izi (yeni-BASTA). -/
def gomGozIz (t : CT.Iz) : List GozlemOlay := (t.map gomGoz).reverse

theorem gomGozIz_append (t1 t2 : CT.Iz) :
    gomGozIz (t1 ++ t2) = gomGozIz t2 ++ gomGozIz t1 := by
  simp [gomGozIz, List.map_append]

theorem gomGozIz_cons (o : CT.Gozlem) (t : CT.Iz) :
    gomGozIz (o :: t) = gomGozIz t ++ [gomGoz o] := by
  simp [gomGozIz]

-- ============================================================
-- §7. ANA SIMULASYON
-- ============================================================

/-- **GOMME SIMULASYONU:** CT'nin buyuk-adim kosumu Core'un kucuk-adim
    kosumuna GOMULUR; uretilen GOZLEM IZI birebir karsilik gelir; ve
    ifade `Sadik` ise son deger CT'nin dondurdugu degerdir.

    CT kurali → Core tarafi:
      c_sabit      → adim yok
      c_degisken   → sVarOku                    (memOku ↔ oOku)
      c_atama      → atamaCong* ; sAtamaTamam   (memYaz ↔ oYaz)
      c_sira       → seqCong*   ; sSeqAtla      (olaysiz)
      c_eger_*     → egerCong*  ; sEgerSec      (dalOl ↔ oDal) -/
theorem gomme_sim {V : List CT.Ad} :
    ∀ {s e s' t v}, CT.Calis s e s' t v → GomOk e → Kapsar V e →
    ∀ (sigma : Store) (iz : Iz) (z : Zaman), StoreUyum V s sigma →
    ∃ (w : Deger) (sigma' : Store) (iz' : Iz) (z' : Zaman),
      KRun V ⟨sigma, gom e, iz, z⟩ ⟨sigma', .sabit w, iz', z'⟩
      ∧ StoreUyum V s' sigma'
      ∧ izGozlem iz' = gomGozIz t ++ izGozlem iz
      ∧ (Sadik e → w = .skaler v) := by
  intro s e s' t v h
  induction h with
  | c_sabit s n =>
      intro _ _ sigma iz z hu
      exact ⟨.skaler n, sigma, iz, z, KRun.refl _, hu, rfl, fun _ => rfl⟩
  | c_degisken s x =>
      intro _ h_kap sigma iz z hu
      have hx : x ∈ V := by cases h_kap with | k_degisken _ h => exact h
      exact ⟨.skaler (s x), sigma, _, z + 1,
        KRun.adim _ _ _ (adim_oku V sigma x (s x) iz z hx (hu x hx))
          (KRun.refl _), hu, rfl, fun _ => rfl⟩
  | c_topla s s1 s2 a b t1 t2 v1 v2 _ _ iha ihb =>
      -- D-334: soldan saga kosum + sToplaTamam. Iz katkisi `sira` ile
      -- ayni sekilde birlesir (topla adimlari OLAY URETMEZ).
      intro h_gom h_kap sigma iz z hu
      cases h_gom with
      | g_topla _ _ h_ga h_sa h_gb h_sb =>
        have h_ka : Kapsar V a := by cases h_kap with | k_topla _ _ h _ => exact h
        have h_kb : Kapsar V b := by cases h_kap with | k_topla _ _ _ h => exact h
        obtain ⟨w1, sg1, iz1, z1, hr1, hu1, hg1, hs1⟩ := iha h_ga h_ka sigma iz z hu
        rw [hs1 h_sa] at hr1
        obtain ⟨w2, sg2, iz2, z2, hr2, hu2, hg2, hs2⟩ := ihb h_gb h_kb sg1 iz1 z1 hu1
        rw [hs2 h_sb] at hr2
        refine ⟨.skaler (v1 + v2), sg2, iz2, z2 + 1,
          krun_trans (krun_topla_sol (V := V) (gom b) hr1)
            (krun_trans (krun_topla_sag (V := V) (.skaler v1) hr2)
              (KRun.adim _ ⟨sg2, .sabit (.skaler (v1 + v2)), iz2, z2 + 1⟩ _
                (adim_topla V sg2 v1 v2 iz2 z2) (KRun.refl _))),
          hu2, ?_, fun _ => rfl⟩
        rw [hg2, hg1, gomGozIz_append, List.append_assoc]
  | c_atama s s1 x e t1 vv he ih =>
      intro h_gom h_kap sigma iz z hu
      cases h_gom with
      | g_atama _ _ h_ge h_se =>
        have hx : x ∈ V := by cases h_kap with | k_atama _ _ h _ => exact h
        have h_ke : Kapsar V e := by cases h_kap with | k_atama _ _ _ h => exact h
        obtain ⟨w, sgE, izE, zE, hrE, huE, hgE, hsE⟩ := ih h_ge h_ke sigma iz z hu
        rw [hsE h_se] at hrE
        refine ⟨.birim, _, _, zE + 1,
          krun_trans (krun_atama (V := V) x hrE)
            (KRun.adim _ _ _ (adim_yaz V sgE x vv izE zE hx) (KRun.refl _)),
          storeUyum_yaz huE x vv, ?_, ?_⟩
        · show gozlem (.memYaz t0 ⟨bol x, 0⟩ (.skaler vv)) :: izGozlem izE = _
          rw [hgE, gomGozIz_append]
          rfl
        · intro hs; nomatch hs
  | c_sira s s1 s2 a b t1 t2 v1 v2 _ _ iha ihb =>
      intro h_gom h_kap sigma iz z hu
      cases h_gom with
      | g_sira _ _ h_ga h_gb =>
        have h_ka : Kapsar V a := by cases h_kap with | k_sira _ _ h _ => exact h
        have h_kb : Kapsar V b := by cases h_kap with | k_sira _ _ _ h => exact h
        obtain ⟨w1, sg1, iz1, z1, hr1, hu1, hg1, _⟩ := iha h_ga h_ka sigma iz z hu
        obtain ⟨w2, sg2, iz2, z2, hr2, hu2, hg2, hs2⟩ :=
          ihb h_gb h_kb sg1 iz1 (z1 + 1) hu1
        refine ⟨w2, sg2, iz2, z2,
          krun_trans (krun_seq (V := V) (gom b) hr1)
            (KRun.adim _ ⟨sg1, gom b, iz1, z1 + 1⟩ _
              (adim_seq_atla V sg1 w1 (gom b) iz1 z1) hr2),
          hu2, ?_, ?_⟩
        · rw [hg2, hg1, gomGozIz_append, List.append_assoc]
        · intro hs
          cases hs with | s_sira _ _ h => exact hs2 h
  | c_eger_dogru s s1 s2 k d y tk td vk vd _ h_dogru _ ihk ihd =>
      intro h_gom h_kap sigma iz z hu
      cases h_gom with
      | g_eger _ _ _ h_gk h_sk h_gd h_gy =>
        have h_kk : Kapsar V k := by cases h_kap with | k_eger _ _ _ h _ _ => exact h
        have h_kd : Kapsar V d := by cases h_kap with | k_eger _ _ _ _ h _ => exact h
        obtain ⟨wk, sgK, izK, zK, hrK, huK, hgK, hsK⟩ := ihk h_gk h_kk sigma iz z hu
        rw [hsK h_sk] at hrK
        have h_dal : degerDogruMu (Deger.skaler vk) = true := by
          rw [dogruMu_skaler]; exact decide_eq_true h_dogru
        obtain ⟨wd, sgD, izD, zD, hrD, huD, hgD, hsD⟩ :=
          ihd h_gd h_kd sgK (.dalOl t0 true :: izK) (zK + 1) huK
        have hstep := adim_dal V sgK vk (gom d) (gom y) izK zK
        rw [h_dal] at hstep
        refine ⟨wd, sgD, izD, zD,
          krun_trans (krun_eger (V := V) (gom d) (gom y) hrK)
            (KRun.adim _ ⟨sgK, gom d, .dalOl t0 true :: izK, zK + 1⟩ _
              hstep hrD),
          huD, ?_, ?_⟩
        · rw [hgD, gomGozIz_append, gomGozIz_cons]
          show gomGozIz td ++ (gozlem (Olay.dalOl t0 true) :: izGozlem izK) = _
          rw [hgK]
          simp [gozlem, gomGoz, List.append_assoc]
        · intro hs
          cases hs with | s_eger _ _ _ h _ => exact hsD h
  | c_eger_yanlis s s1 s2 k d y tk ty vk vy _ h_yanlis _ ihk ihy =>
      intro h_gom h_kap sigma iz z hu
      cases h_gom with
      | g_eger _ _ _ h_gk h_sk h_gd h_gy =>
        have h_kk : Kapsar V k := by cases h_kap with | k_eger _ _ _ h _ _ => exact h
        have h_ky : Kapsar V y := by cases h_kap with | k_eger _ _ _ _ _ h => exact h
        obtain ⟨wk, sgK, izK, zK, hrK, huK, hgK, hsK⟩ := ihk h_gk h_kk sigma iz z hu
        rw [hsK h_sk] at hrK
        have h_dal : degerDogruMu (Deger.skaler vk) = false := by
          rw [dogruMu_skaler, h_yanlis]; rfl
        obtain ⟨wy, sgY, izY, zY, hrY, huY, hgY, hsY⟩ :=
          ihy h_gy h_ky sgK (.dalOl t0 false :: izK) (zK + 1) huK
        have hstep := adim_dal V sgK vk (gom d) (gom y) izK zK
        rw [h_dal] at hstep
        refine ⟨wy, sgY, izY, zY,
          krun_trans (krun_eger (V := V) (gom d) (gom y) hrK)
            (KRun.adim _ ⟨sgK, gom y, .dalOl t0 false :: izK, zK + 1⟩ _
              hstep hrY),
          huY, ?_, ?_⟩
        · rw [hgY, gomGozIz_append, gomGozIz_cons]
          show gomGozIz ty ++ (gozlem (Olay.dalOl t0 false) :: izGozlem izK) = _
          rw [hgK]
          simp [gozlem, gomGoz, List.append_assoc]
        · intro hs
          cases hs with | s_eger _ _ _ _ h => exact hsY h
  -- D-335 (CT002): DONGU. Core tarafi: sIkenAc (acilma) → acilmis `eger`in
  -- kosulu kosar → sEgerSec (`dalOl true`) → govde → sSeqAtla → IC DONGU.
  -- Ucuncu IH (ihI) ic dongu icin — CT'nin ozyinelemeli kuralinin karsiligi.
  | c_iken_dogru s s1 s2 s3 k g tk tg ti vk vg vi _ h_dogru _ _ ihK ihG ihI =>
      intro h_gom h_kap sigma iz z hu
      cases h_gom with
      | g_iken _ _ h_gk h_sk h_gg =>
        have h_kk : Kapsar V k := by cases h_kap with | k_iken _ _ h _ => exact h
        have h_kg : Kapsar V g := by cases h_kap with | k_iken _ _ _ h => exact h
        obtain ⟨wk, sgK, izK, zK, hrK, huK, hgK, hsK⟩ := ihK h_gk h_kk sigma iz (z + 1) hu
        rw [hsK h_sk] at hrK
        have h_dal : degerDogruMu (Deger.skaler vk) = true := by
          rw [dogruMu_skaler]; exact decide_eq_true h_dogru
        obtain ⟨wg, sgG, izG, zG, hrG, huG, hgG, _⟩ :=
          ihG h_gg h_kg sgK (.dalOl t0 true :: izK) (zK + 1) huK
        obtain ⟨wi, sgI, izI, zI, hrI, huI, hgI, _⟩ :=
          ihI (GomOk.g_iken k g h_gk h_sk h_gg) (Kapsar.k_iken k g h_kk h_kg)
            sgG izG (zG + 1) huG
        have hstep := adim_dal V sgK vk (.seq (gom g) (.iken (gom k) (gom g)))
          (.sabit .birim) izK zK
        rw [h_dal] at hstep
        refine ⟨wi, sgI, izI, zI,
          KRun.adim _ ⟨sigma, .eger (gom k) (.seq (gom g) (.iken (gom k) (gom g)))
              (.sabit .birim), iz, z + 1⟩ _
            (adim_iken_ac V sigma (gom k) (gom g) iz z)
            (krun_trans
              (krun_eger (V := V) (.seq (gom g) (.iken (gom k) (gom g)))
                (.sabit .birim) hrK)
              (KRun.adim _ ⟨sgK, .seq (gom g) (.iken (gom k) (gom g)),
                  .dalOl t0 true :: izK, zK + 1⟩ _ hstep
                (krun_trans (krun_seq (V := V) (.iken (gom k) (gom g)) hrG)
                  (KRun.adim _ ⟨sgG, .iken (gom k) (gom g), izG, zG + 1⟩ _
                    (adim_seq_atla V sgG wg (.iken (gom k) (gom g)) izG zG)
                    hrI)))),
          huI, ?_, ?_⟩
        · rw [hgI, hgG, gomGozIz_append, gomGozIz_cons, gomGozIz_append]
          show gomGozIz ti ++ (gomGozIz tg ++
                 (gozlem (Olay.dalOl t0 true) :: izGozlem izK)) = _
          rw [hgK]
          simp [gozlem, gomGoz, List.append_assoc]
        · intro hs; nomatch hs
  | c_iken_yanlis s s1 k g tk vk _ h_yanlis ihK =>
      intro h_gom h_kap sigma iz z hu
      cases h_gom with
      | g_iken _ _ h_gk h_sk h_gg =>
        have h_kk : Kapsar V k := by cases h_kap with | k_iken _ _ h _ => exact h
        obtain ⟨wk, sgK, izK, zK, hrK, huK, hgK, hsK⟩ := ihK h_gk h_kk sigma iz (z + 1) hu
        rw [hsK h_sk] at hrK
        have h_dal : degerDogruMu (Deger.skaler vk) = false := by
          rw [dogruMu_skaler, h_yanlis]; rfl
        have hstep := adim_dal V sgK vk (.seq (gom g) (.iken (gom k) (gom g)))
          (.sabit .birim) izK zK
        rw [h_dal] at hstep
        refine ⟨.birim, sgK, .dalOl t0 false :: izK, zK + 1,
          KRun.adim _ ⟨sigma, .eger (gom k) (.seq (gom g) (.iken (gom k) (gom g)))
              (.sabit .birim), iz, z + 1⟩ _
            (adim_iken_ac V sigma (gom k) (gom g) iz z)
            (krun_trans
              (krun_eger (V := V) (.seq (gom g) (.iken (gom k) (gom g)))
                (.sabit .birim) hrK)
              (KRun.adim _ ⟨sgK, .sabit .birim, .dalOl t0 false :: izK, zK + 1⟩ _
                hstep (KRun.refl _))),
          huK, ?_, ?_⟩
        · rw [gomGozIz_append]
          show gozlem (Olay.dalOl t0 false) :: izGozlem izK = _
          rw [hgK]
          simp [gozlem, gomGoz, gomGozIz]
        · intro hs; nomatch hs
  -- D-335 (CT004): DESEN ESLEMESI — `eger` deseninin aynisi.
  | c_esles_tuttu s s1 s2 sk n d y ts td vs vd _ h_tuttu _ ihS ihD =>
      intro h_gom h_kap sigma iz z hu
      cases h_gom with
      | g_esles _ _ _ _ h_gs h_ss h_gd h_gy =>
        have h_ks : Kapsar V sk := by cases h_kap with | k_esles _ _ _ _ h _ _ => exact h
        have h_kd : Kapsar V d := by cases h_kap with | k_esles _ _ _ _ _ h _ => exact h
        obtain ⟨ws, sgS, izS, zS, hrS, huS, hgS, hsS⟩ := ihS h_gs h_ks sigma iz z hu
        rw [hsS h_ss] at hrS
        have h_dal : decide (vs = n) = true := decide_eq_true h_tuttu
        obtain ⟨wd, sgD, izD, zD, hrD, huD, hgD, hsD⟩ :=
          ihD h_gd h_kd sgS (.dalOl t0 true :: izS) (zS + 1) huS
        have hstep := adim_esles V sgS vs n (gom d) (gom y) izS zS
        rw [h_dal] at hstep
        refine ⟨wd, sgD, izD, zD,
          krun_trans (krun_esles (V := V) n (gom d) (gom y) hrS)
            (KRun.adim _ ⟨sgS, gom d, .dalOl t0 true :: izS, zS + 1⟩ _
              hstep hrD),
          huD, ?_, ?_⟩
        · rw [hgD, gomGozIz_append, gomGozIz_cons]
          show gomGozIz td ++ (gozlem (Olay.dalOl t0 true) :: izGozlem izS) = _
          rw [hgS]
          simp [gozlem, gomGoz, List.append_assoc]
        · intro hs
          cases hs with | s_esles _ _ _ _ h _ => exact hsD h
  | c_esles_tutmadi s s1 s2 sk n d y ts ty vs vy _ h_tutmadi _ ihS ihY =>
      intro h_gom h_kap sigma iz z hu
      cases h_gom with
      | g_esles _ _ _ _ h_gs h_ss h_gd h_gy =>
        have h_ks : Kapsar V sk := by cases h_kap with | k_esles _ _ _ _ h _ _ => exact h
        have h_ky : Kapsar V y := by cases h_kap with | k_esles _ _ _ _ _ _ h => exact h
        obtain ⟨ws, sgS, izS, zS, hrS, huS, hgS, hsS⟩ := ihS h_gs h_ks sigma iz z hu
        rw [hsS h_ss] at hrS
        have h_dal : decide (vs = n) = false := decide_eq_false h_tutmadi
        obtain ⟨wy, sgY, izY, zY, hrY, huY, hgY, hsY⟩ :=
          ihY h_gy h_ky sgS (.dalOl t0 false :: izS) (zS + 1) huS
        have hstep := adim_esles V sgS vs n (gom d) (gom y) izS zS
        rw [h_dal] at hstep
        refine ⟨wy, sgY, izY, zY,
          krun_trans (krun_esles (V := V) n (gom d) (gom y) hrS)
            (KRun.adim _ ⟨sgS, gom y, .dalOl t0 false :: izS, zS + 1⟩ _
              hstep hrY),
          huY, ?_, ?_⟩
        · rw [hgY, gomGozIz_append, gomGozIz_cons]
          show gomGozIz ty ++ (gozlem (Olay.dalOl t0 false) :: izGozlem izS) = _
          rw [hgS]
          simp [gozlem, gomGoz, List.append_assoc]
        · intro hs
          cases hs with | s_esles _ _ _ _ _ h => exact hsY h

-- ============================================================
-- §8. ANA SONUC — ct_ni ANA MODELE TASINDI
-- ============================================================

/-- **KOPRU TEOREMI:** CT-tipli (`CtOk`) ve gomulebilir bir program,
    DUSUK-ESDEGER iki baslangic store'undan `Sem/Core`'da kosturuldugunda
    SALDIRGANIN GORDUGU IZ AYNIDIR.

    "Gordugu iz" = `NonInterference.izGozlem` — okuma/yazma deseni ve
    **DAL KARARLARI** (`gDal`); tasinan degerler zaten projekte edilmis.
    D-330'daki `ct_ni` artik ayri bir oyuncak hesap hakkinda degil, ANA
    MODELDEKI kosumlar hakkinda bir sey soyluyor.

    Bunu mumkun kilan iki D-332 parcasi: `Ifade.eger` (kanal ifade
    edilebilir oldu) ve `Olay.dalOl` (kanal GOZLENEBILIR oldu). -/
theorem kopru_ni (G : CT.EtiketOrtam) (V : List CT.Ad) (e : CT.Ifade)
    (h_ct : CT.CtOk G e) (h_gom : GomOk e) (h_kap : Kapsar V e)
    {s1 s2 s1' s2' : CT.Store} {t1 t2 : CT.Iz} {v1 v2 : Int}
    (h_low : CT.DusukEs G s1 s2)
    (h_r1 : CT.Calis s1 e s1' t1 v1) (h_r2 : CT.Calis s2 e s2' t2 v2)
    (sigma1 sigma2 : Store) (iz : Iz) (z : Zaman)
    (hu1 : StoreUyum V s1 sigma1) (hu2 : StoreUyum V s2 sigma2) :
    ∃ (w1 w2 : Deger) (sg1 sg2 : Store) (iz1 iz2 : Iz) (z1 z2 : Zaman),
      StepStar (K V ⟨sigma1, gom e, iz, z⟩) (K V ⟨sg1, .sabit w1, iz1, z1⟩)
      ∧ StepStar (K V ⟨sigma2, gom e, iz, z⟩) (K V ⟨sg2, .sabit w2, iz2, z2⟩)
      ∧ izGozlem iz1 = izGozlem iz2 := by
  obtain ⟨w1, sg1, iz1, z1, hr1, _, hg1, _⟩ := gomme_sim h_r1 h_gom h_kap sigma1 iz z hu1
  obtain ⟨w2, sg2, iz2, z2, hr2, _, hg2, _⟩ := gomme_sim h_r2 h_gom h_kap sigma2 iz z hu2
  obtain ⟨h_iz_esit, _⟩ := CT.ct_ni G h_r1 h_r2 h_ct h_low
  exact ⟨w1, w2, sg1, sg2, iz1, iz2, z1, z2,
    krun_stepStar hr1, krun_stepStar hr2, by rw [hg1, hg2, h_iz_esit]⟩

-- ============================================================
-- §9. VAKUM DENETIMI — kopru BOS DEGIL
-- ============================================================

/-- Kaprunun ONEMSIZ olmadiginin taniki: hem `CtOk` hem `GomOk` hem
    `Kapsar` saglayan, ustelik GERCEKTEN DALLANAN ve GIZLI degiskene
    YAZAN bir program vardir.

    Program: `eger (1 + 1) (0 = 5) (0 = 7)` — 1 GENEL (kosulda okunur,
    CT001 uyumlu), 0 GIZLI (yazma hedefi, CT003 uyumlu: genel deger
    gizliye yazilabilir). D-334'ten beri kosulda ARITMETIK da var, yani
    tanik yeni kapsamı fiilen kullanir. Hipotezler BIRLIKTE saglanabilir;
    teorem vakum degildir. -/
theorem kopru_bos_degil :
    ∃ (G : CT.EtiketOrtam) (e : CT.Ifade),
      CT.CtOk G e ∧ GomOk e ∧ Kapsar [0, 1] e
      ∧ (∃ a b d y, e = .eger (.topla a b) d y) := by
  refine ⟨fun x => if x = 0 then .gizli else .genel,
          .eger (.topla (.degisken 1) (.sabit 1))
                (.sabitDeg 0 (.sabit 5)) (.sabitDeg 0 (.sabit 7)),
          ?_, ?_, ?_, ⟨_, _, _, _, rfl⟩⟩
  · exact CT.CtOk.ct_eger _ _ _
      (CT.CtOk.ct_topla _ _ (CT.CtOk.ct_degisken 1) (CT.CtOk.ct_sabit 1))
      (CT.CtOk.ct_atama 0 (.sabit 5) (CT.CtOk.ct_sabit 5) (by decide))
      (CT.CtOk.ct_atama 0 (.sabit 7) (CT.CtOk.ct_sabit 7) (by decide))
      (by decide)
  · exact GomOk.g_eger _ _ _
      (GomOk.g_topla _ _ (GomOk.g_degisken 1) (Sadik.s_degisken 1)
        (GomOk.g_sabit 1) (Sadik.s_sabit 1))
      (Sadik.s_topla _ _)
      (GomOk.g_atama 0 _ (GomOk.g_sabit 5) (Sadik.s_sabit 5))
      (GomOk.g_atama 0 _ (GomOk.g_sabit 7) (Sadik.s_sabit 7))
  · exact Kapsar.k_eger _ _ _
      (Kapsar.k_topla _ _ (Kapsar.k_degisken 1 (by decide)) (Kapsar.k_sabit 1))
      (Kapsar.k_atama 0 _ (by decide) (Kapsar.k_sabit 5))
      (Kapsar.k_atama 0 _ (by decide) (Kapsar.k_sabit 7))

/-- D-335: koprunun DONGU ve DESEN ESLEMESI kapsadiginin taniki.
    Program: `iken (1 == 1 kolu) ...` yerine dogrudan
    `esles (degisken 1) 3 (iken (sabit 0) (0 = 5)) (0 = 7)` — hem `esles`
    hem `iken` iceriyor, skrutin/kosul GENEL (CT004/CT002 uyumlu), yazma
    hedefi GIZLI (CT003 uyumlu). Yani D-335'in ekledigi iki bicim de
    `kopru_ni` hipotezlerini saglayabiliyor; genisleme vakum degil. -/
theorem kopru_iken_esles_bos_degil :
    ∃ (G : CT.EtiketOrtam) (e : CT.Ifade),
      CT.CtOk G e ∧ GomOk e ∧ Kapsar [0, 1] e
      ∧ (∃ s n d y, e = .esles s n d y)
      ∧ (∃ s n k g y, e = .esles s n (.iken k g) y) := by
  refine ⟨fun x => if x = 0 then .gizli else .genel,
          .esles (.degisken 1) 3
            (.iken (.sabit 0) (.sabitDeg 0 (.sabit 5)))
            (.sabitDeg 0 (.sabit 7)),
          ?_, ?_, ?_, ⟨_, _, _, _, rfl⟩, ⟨_, _, _, _, _, rfl⟩⟩
  · exact CT.CtOk.ct_esles _ 3 _ _ (CT.CtOk.ct_degisken 1)
      (CT.CtOk.ct_iken _ _ (CT.CtOk.ct_sabit 0)
        (CT.CtOk.ct_atama 0 (.sabit 5) (CT.CtOk.ct_sabit 5) (by decide))
        (by decide))
      (CT.CtOk.ct_atama 0 (.sabit 7) (CT.CtOk.ct_sabit 7) (by decide))
      (by decide)
  · exact GomOk.g_esles _ 3 _ _ (GomOk.g_degisken 1) (Sadik.s_degisken 1)
      (GomOk.g_iken _ _ (GomOk.g_sabit 0) (Sadik.s_sabit 0)
        (GomOk.g_atama 0 _ (GomOk.g_sabit 5) (Sadik.s_sabit 5)))
      (GomOk.g_atama 0 _ (GomOk.g_sabit 7) (Sadik.s_sabit 7))
  · exact Kapsar.k_esles _ 3 _ _ (Kapsar.k_degisken 1 (by decide))
      (Kapsar.k_iken _ _ (Kapsar.k_sabit 0)
        (Kapsar.k_atama 0 _ (by decide) (Kapsar.k_sabit 5)))
      (Kapsar.k_atama 0 _ (by decide) (Kapsar.k_sabit 7))

end Kemgu.SideChannel.CTKopru
