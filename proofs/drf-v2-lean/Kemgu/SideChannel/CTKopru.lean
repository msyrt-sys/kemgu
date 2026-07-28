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
  | s_carp (a b : CT.Ifade) : Sadik (.carp a b)
  /-- D-338: bolumun degeri `skaler (v1/v2)` — sadik. -/
  | s_bol (a b : CT.Ifade) : Sadik (.bol a b)
  /-- D-339: bolumun degeri `skaler (v1/v2)` — sadik. -/
  | s_kalan (a b : CT.Ifade) : Sadik (.kalan a b)
  /-- D-335: `esles`in degeri secilen koldan gelir. -/
  | s_esles (s : CT.Ifade) (n : Int) (d y : CT.Ifade) :
      Sadik d → Sadik y → Sadik (.esles s n d y)
  /-- D-336: okunan hucrenin degeri her iki tarafta da `s x i`. -/
  | s_indeks (x : CT.Ad) (idx : CT.Ifade) : Sadik (.indeks x idx)
  -- NOT (D-337): `indeksAta` SADIK DEGILDIR — CT'de yazilan degeri
  -- dondurur, Core'da `sIndeksYaz` `birim` dondurur (`sabitDeg` ile
  -- ayni sinif). Deyim konumunda serbesttir.
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
  /-- D-338: bolme — `topla` ile ayni gomme sarti. -/
  | g_carp (a b : CT.Ifade) :
      GomOk a → Sadik a → GomOk b → Sadik b → GomOk (.carp a b)
  /-- D-338: bolme — `carp` ile ayni gomme sarti. -/
  | g_bol (a b : CT.Ifade) :
      GomOk a → Sadik a → GomOk b → Sadik b → GomOk (.bol a b)
  /-- D-339: bolme — `topla` ile ayni gomme sarti. -/
  | g_kalan (a b : CT.Ifade) :
      GomOk a → Sadik a → GomOk b → Sadik b → GomOk (.kalan a b)
  /-- D-335: dongu. Kosul `Sadik` olmali (dal karari ona bagli). -/
  | g_iken (k g : CT.Ifade) :
      GomOk k → Sadik k → GomOk g → GomOk (.iken k g)
  /-- D-335: desen eslemesi. Skrutin `Sadik` olmali (literal ile
      karsilastirilir). -/
  | g_esles (s : CT.Ifade) (n : Int) (d y : CT.Ifade) :
      GomOk s → Sadik s → GomOk d → GomOk y → GomOk (.esles s n d y)
  /-- D-336: indeks ifadesi `Sadik` olmali (adres ondan hesaplanir). -/
  | g_indeks (x : CT.Ad) (idx : CT.Ifade) :
      GomOk idx → Sadik idx → GomOk (.indeks x idx)
  /-- D-337: indeks VE deger `Sadik` olmali (`sIndeksYaz` ikisinin de
      `sabit (skaler _)` olmasini ister). -/
  | g_indeks_ata (x : CT.Ad) (idx e : CT.Ifade) :
      GomOk idx → Sadik idx → GomOk e → Sadik e →
      GomOk (.indeksAta x idx e)

/-- CT ifadesi → Core ifadesi. D-334'ten beri TOPLAM (her bicim icin
    gercek bir karsilik var; tikac dal YOK). -/
def gom : CT.Ifade → Ifade
  | .sabit n        => .sabit (.skaler n)
  -- D-336: CT'de duz degisken = `x[0]`, dolayisiyla gomme de INDEKSLI
  -- okumadir (`tanim` DEGIL). Kazanc: okuma TOPLAM oldugundan store
  -- uyumu tek bir bicimle (`hucreOku`) ifade edilir; `sVarOku`nun
  -- `konumGet ... = some v` on-kosulu (kayitli olma) gerekmez.
  | .degisken x     => .indeks x (.sabit (.skaler 0))
  | .sabitDeg x e   => .atama x (gom e)
  | .sira a b       => .seq (gom a) (gom b)
  | .eger k d y     => .eger (gom k) (gom d) (gom y)
  | .topla a b      => .topla (gom a) (gom b)   -- D-334
  | .carp a b      => .carp (gom a) (gom b)   -- D-340
  | .bol a b        => .bol (gom a) (gom b)     -- D-338
  | .kalan a b        => .kalan (gom a) (gom b)     -- D-339
  | .iken k g       => .iken (gom k) (gom g)    -- D-335
  | .esles s n d y  => .esles (gom s) n (gom d) (gom y)
  | .indeks x idx   => .indeks x (gom idx)      -- D-336
  | .indeksAta x idx e => .indeksAta x (gom idx) (gom e)   -- D-337

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
  | k_carp (a b : CT.Ifade) : Kapsar V a → Kapsar V b → Kapsar V (.carp a b)
  | k_bol (a b : CT.Ifade) : Kapsar V a → Kapsar V b → Kapsar V (.bol a b)
  | k_kalan (a b : CT.Ifade) : Kapsar V a → Kapsar V b → Kapsar V (.kalan a b)
  | k_iken (k g : CT.Ifade) : Kapsar V k → Kapsar V g → Kapsar V (.iken k g)
  | k_esles (s : CT.Ifade) (n : Int) (d y : CT.Ifade) :
      Kapsar V s → Kapsar V d → Kapsar V y → Kapsar V (.esles s n d y)
  | k_indeks (x : CT.Ad) (idx : CT.Ifade) :
      x ∈ V → Kapsar V idx → Kapsar V (.indeks x idx)
  | k_indeks_ata (x : CT.Ad) (idx e : CT.Ifade) :
      x ∈ V → Kapsar V idx → Kapsar V e → Kapsar V (.indeksAta x idx e)

/-- D-342: `Yazar e x` — e, x degiskenine YAZAR.
    Neden gerekli: Core'da yazma BOLGE SAHIPLIGI ister
    (`sAtamaTamam`/`sIndeksYaz` → `sahiplikGet .. = some (thread ctx.tid)`),
    CT'nin eszamanli modelinde ise store TAMAMEN PAYLASIMLIDIR. Cakisma
    burada: OKUMA sahiplik istemez (thread-arasi okuma girisimi serbest),
    YAZMA ister. Kopru bu yuzden TEK-YAZICI programlari kapsar (Mehmet
    karari) ve bu yargi "hangi degiskenlere yaziliyor"u yakalar. -/
inductive Yazar : CT.Ifade → CT.Ad → Prop where
  | y_atama_bas (x : CT.Ad) (e : CT.Ifade) : Yazar (.sabitDeg x e) x
  | y_atama_ic (x : CT.Ad) (e : CT.Ifade) (z : CT.Ad) :
      Yazar e z → Yazar (.sabitDeg x e) z
  | y_indeks_ata_bas (x : CT.Ad) (idx e : CT.Ifade) : Yazar (.indeksAta x idx e) x
  | y_indeks_ata_idx (x : CT.Ad) (idx e : CT.Ifade) (z : CT.Ad) :
      Yazar idx z → Yazar (.indeksAta x idx e) z
  | y_indeks_ata_deg (x : CT.Ad) (idx e : CT.Ifade) (z : CT.Ad) :
      Yazar e z → Yazar (.indeksAta x idx e) z
  | y_sira_sol (a b : CT.Ifade) (z : CT.Ad) : Yazar a z → Yazar (.sira a b) z
  | y_sira_sag (a b : CT.Ifade) (z : CT.Ad) : Yazar b z → Yazar (.sira a b) z
  | y_eger_k (k d y : CT.Ifade) (z : CT.Ad) : Yazar k z → Yazar (.eger k d y) z
  | y_eger_d (k d y : CT.Ifade) (z : CT.Ad) : Yazar d z → Yazar (.eger k d y) z
  | y_eger_y (k d y : CT.Ifade) (z : CT.Ad) : Yazar y z → Yazar (.eger k d y) z
  | y_iken_k (k g : CT.Ifade) (z : CT.Ad) : Yazar k z → Yazar (.iken k g) z
  | y_iken_g (k g : CT.Ifade) (z : CT.Ad) : Yazar g z → Yazar (.iken k g) z
  | y_esles_s (s : CT.Ifade) (n : Int) (d y : CT.Ifade) (z : CT.Ad) :
      Yazar s z → Yazar (.esles s n d y) z
  | y_esles_d (s : CT.Ifade) (n : Int) (d y : CT.Ifade) (z : CT.Ad) :
      Yazar d z → Yazar (.esles s n d y) z
  | y_esles_y (s : CT.Ifade) (n : Int) (d y : CT.Ifade) (z : CT.Ad) :
      Yazar y z → Yazar (.esles s n d y) z
  | y_indeks (x : CT.Ad) (idx : CT.Ifade) (z : CT.Ad) :
      Yazar idx z → Yazar (.indeks x idx) z
  | y_topla_sol (a b : CT.Ifade) (z : CT.Ad) : Yazar a z → Yazar (.topla a b) z
  | y_topla_sag (a b : CT.Ifade) (z : CT.Ad) : Yazar b z → Yazar (.topla a b) z
  | y_carp_sol (a b : CT.Ifade) (z : CT.Ad) : Yazar a z → Yazar (.carp a b) z
  | y_carp_sag (a b : CT.Ifade) (z : CT.Ad) : Yazar b z → Yazar (.carp a b) z
  | y_bol_sol (a b : CT.Ifade) (z : CT.Ad) : Yazar a z → Yazar (.bol a b) z
  | y_bol_sag (a b : CT.Ifade) (z : CT.Ad) : Yazar b z → Yazar (.bol a b) z
  | y_kalan_sol (a b : CT.Ifade) (z : CT.Ad) : Yazar a z → Yazar (.kalan a b) z
  | y_kalan_sag (a b : CT.Ifade) (z : CT.Ad) : Yazar b z → Yazar (.kalan a b) z

/-- `e`nin YAZDIGI her degiskenin sahibi `tid`dir. -/
def YazmaSahibi (own : CT.Ad → ThreadId) (tid : ThreadId) (e : CT.Ifade) : Prop :=
  ∀ x, Yazar e x → own x = tid

-- ============================================================
-- §2. Gomme — durum (bolge / sahiplik / store)
-- ============================================================

/-- Gomen thread (Kapsam 4: tek thread). -/
abbrev t0 : ThreadId := 0

/-- D-342: ODAK CERCEVESI — odakli thread'in COK-THREAD'li listedeki
    konumu ve kimligi. `Step` kurallari zaten `ts1 ++ ctx :: ts2` formunda
    yazilmisti; kopru simdiye kadar bunu `[] ++ ctx :: []`e sabitliyordu.
    Cerceve o sabitlemeyi KALDIRIR — D-333'un tek-thread kurulumu artik
    `cerceveTek`in ozel halidir. -/
structure Cerceve where
  ts1 : List ThreadCtx    -- odaktan ONCEKI thread'ler (bu adimda DONMUS)
  ts2 : List ThreadCtx    -- odaktan SONRAKI thread'ler (bu adimda DONMUS)
  tid : ThreadId          -- odakli thread'in kimligi

/-- Tek-thread cerceve (geriye uyum: D-333..D-341 kurulumu). -/
def cerceveTek : Cerceve := ⟨[], [], t0⟩

/-- x degiskeninin bolgesi. Kategori YAZILABILIR olmali (sAtamaTamam);
    `yerel` secildi. Bolge KIMLIGI = degisken kimligi — `konumGet` ve
    `sahiplikGet` zaten id-anahtarlidir (Onarim v3 karari), dolayisiyla
    bu esleme ayriktir (farkli degisken → farkli bolge id). -/
def bol (x : CT.Ad) : Bolge := ⟨x, BolgeKategorisi.yerel 0⟩

def rhoOf (V : List CT.Ad) : BolgeOrtam := V.map (fun x => (x, bol x))
def sahOf (V : List CT.Ad) (own : CT.Ad → ThreadId) : Sahiplik :=
  V.map (fun x => (bol x, Sahip.thread (own x)))

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

theorem sahOf_get {V : List CT.Ad} {own : CT.Ad → ThreadId} {x : CT.Ad} (h : x ∈ V) :
    sahiplikGet (sahOf V own) (bol x) = some (Sahip.thread (own x)) := by
  induction V with
  | nil => cases h
  | cons w rest ih =>
      show sahiplikGet ((bol w, Sahip.thread (own w))
              :: rest.map (fun y => (bol y, Sahip.thread (own y)))) (bol x) = _
      by_cases hw : w = x
      · subst hw; simp [sahiplikGet]
      · have hid : ¬ ((bol w).id = (bol x).id) := hw
        rw [sahiplikGet, if_neg hid]
        exact ih (by cases h with
                     | head => exact absurd rfl hw
                     | tail _ h' => exact h')

/-- Store UYUMU (D-336: HER OFSET icin). Core store'u, CT store'unu
    V uzerinde ve TUM indekslerde GERCEKLER — `hucreOku` uzerinden
    ifade edilir cunku `sIndeksOku` de onu kullanir (ve okuma TOPLAM;
    kayitli olmayan hucre 0 okur, CT tarafinda da store toplam). -/
def StoreUyum (V : List CT.Ad) (s : CT.Store) (sigma : Store) : Prop :=
  ∀ x ∈ V, ∀ i : Nat, hucreOku sigma ⟨bol x, i⟩ = s x i

/-- Yazma uyumu korur: Core'un prepend'i CT'nin `yaz`ina karsilik gelir
    (ikisi de YALNIZ ofset 0'i degistirir). -/
theorem storeUyum_yaz {V : List CT.Ad} {s : CT.Store} {sigma : Store}
    (h : StoreUyum V s sigma) (x : CT.Ad) (n : Int) :
    StoreUyum V (CT.yaz s x n) ((⟨bol x, 0⟩, Deger.skaler n) :: sigma) := by
  intro y hy i
  show (match (if (bol x).id = (bol y).id ∧ (0 : Nat) = i
               then some (Deger.skaler n) else konumGet sigma ⟨bol y, i⟩) with
        | some (.skaler m) => m | _ => 0) = CT.yaz s x n y i
  by_cases hxy : y = x ∧ i = 0
  · obtain ⟨h1, h2⟩ := hxy
    subst h1; subst h2
    rw [if_pos ⟨rfl, rfl⟩]
    simp [CT.yaz]
  · have hid : ¬ ((bol x).id = (bol y).id ∧ (0 : Nat) = i) := by
      intro hc
      exact hxy ⟨hc.1.symm, hc.2.symm⟩
    rw [if_neg hid]
    show hucreOku sigma ⟨bol y, i⟩ = _
    rw [h y hy i]
    simp [CT.yaz, hxy]

/-- D-337: INDEKSLI yazma uyumu korur — Core'un prepend'i CT'nin
    `yazH`ine karsilik gelir (ayni bolge, ayni ofset). -/
theorem storeUyum_yazH {V : List CT.Ad} {s : CT.Store} {sigma : Store}
    (h : StoreUyum V s sigma) (x : CT.Ad) (i : Nat) (n : Int) :
    StoreUyum V (CT.yazH s x i n) ((⟨bol x, i⟩, Deger.skaler n) :: sigma) := by
  intro y hy j
  show (match (if (bol x).id = (bol y).id ∧ i = j
               then some (Deger.skaler n) else konumGet sigma ⟨bol y, j⟩) with
        | some (.skaler m) => m | _ => 0) = CT.yazH s x i n y j
  by_cases hxy : y = x ∧ j = i
  · obtain ⟨h1, h2⟩ := hxy
    subst h1; subst h2
    rw [if_pos ⟨rfl, rfl⟩]
    simp [CT.yazH]
  · have hid : ¬ ((bol x).id = (bol y).id ∧ i = j) := by
      intro hc
      exact hxy ⟨hc.1.symm, hc.2.symm⟩
    rw [if_neg hid]
    show hucreOku sigma ⟨bol y, j⟩ = _
    rw [h y hy j]
    simp [CT.yazH, hxy]

/-- **VAKUM DENETIMI (D-336, indeksli hal):** `StoreUyum` hipotezi
    BOS DEGILDIR — hem de SIFIR-OLMAYAN bir DIZI HUCRESI iceren bir
    ornekle. Burada `tablo` (1) degiskeninin 3. hucresi 9'dur; Core
    tarafinda tek elemanli bir store bunu gercekler.
    (Genel bir `storeOf` insasi yerine somut tanik secildi: teoremin
    hipotezinin saglanabildigini gostermek icin bu YETER ve ispati
    kisa/denetlenebilir tutar.) -/
theorem storeUyum_ornek :
    StoreUyum [0, 1] (fun x i => if x = 1 ∧ i = 3 then 9 else 0)
      [(⟨bol 1, 3⟩, Deger.skaler 9)] := by
  intro x _ i
  show (match (if (bol 1).id = (bol x).id ∧ 3 = i
               then some (Deger.skaler 9) else none) with
        | some (.skaler m) => m | _ => 0)
      = if x = 1 ∧ i = 3 then 9 else 0
  by_cases h : x = 1 ∧ i = 3
  · obtain ⟨h1, h2⟩ := h
    subst h1; subst h2
    rw [if_pos ⟨rfl, rfl⟩, if_pos ⟨rfl, rfl⟩]
  · rw [if_neg h, if_neg (by intro hc; exact h ⟨hc.1.symm, hc.2.symm⟩)]


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

def K (V : List CT.Ad) (own : CT.Ad → ThreadId) (F : Cerceve)
    (d : KDurum) : Konfigurasyon :=
  { thread   := F.ts1 ++ ⟨F.tid, d.ifade, []⟩ :: F.ts2
    store    := d.store
    sahiplik := sahOf V own
    kanal    := []
    zaman    := d.zaman
    iz       := d.iz
    fault    := none
    bolge    := rhoOf V }

/-- K-sekilli konfigurasyonlardan gecen reduksiyon kapanisi. -/
inductive KRun (V : List CT.Ad) (own : CT.Ad → ThreadId) (F : Cerceve) :
    KDurum → KDurum → Prop where
  | refl (d : KDurum) : KRun V own F d d
  | adim (d d1 d' : KDurum) :
      Step (K V own F d) (K V own F d1) → KRun V own F d1 d' → KRun V own F d d'

/-- KRun GERCEK bir kosumdur: `StepStar`in alt-iliskisi. -/
theorem krun_stepStar {V : List CT.Ad} {own : CT.Ad → ThreadId} {F : Cerceve} {d d' : KDurum}
    (h : KRun V own F d d') : StepStar (K V own F d) (K V own F d') := by
  induction h with
  | refl d => exact StepStar.refl _
  | adim d d1 d' h1 _ ih => exact StepStar.step _ _ _ h1 ih

theorem krun_trans {V : List CT.Ad} {own : CT.Ad → ThreadId} {F : Cerceve} {d d1 d' : KDurum}
    (h1 : KRun V own F d d1) (h2 : KRun V own F d1 d') : KRun V own F d d' := by
  induction h1 with
  | refl _ => exact h2
  | adim a b _ hs _ ih => exact KRun.adim a b d' hs (ih h2)

-- ============================================================
-- §4. Congruence yukseltmeleri
-- Her biri: ilgili Core cong kuralini TEK adima uygula, KRun uzerinde
-- tumevarimla kosuma yay. FIX-F yan-kosulu daima `Or.inl rfl`
-- (Kapsam 4: spawn yok).
-- ============================================================

theorem krun_seq {V : List CT.Ad} {own : CT.Ad → ThreadId} {F : Cerceve} (b : Ifade) {d d' : KDurum}
    (h : KRun V own F d d') :
    KRun V own F { d with ifade := .seq d.ifade b }
           { d' with ifade := .seq d'.ifade b } := by
  induction h with
  | refl _ => exact KRun.refl _
  | adim dA dB _ hs _ ih =>
      refine KRun.adim _ { dB with ifade := .seq dB.ifade b } _ ?_ ih
      exact Step.sSeqCong (K V own F { dA with ifade := .seq dA.ifade b })
        (K V own F { dB with ifade := .seq dB.ifade b }) (K V own F dA) (K V own F dB)
        F.ts1 F.ts2 F.ts2 ⟨F.tid, .seq dA.ifade b, []⟩ ⟨F.tid, dB.ifade, []⟩
        dA.ifade dB.ifade b rfl rfl rfl hs rfl rfl rfl (Or.inl rfl) rfl

theorem krun_atama {V : List CT.Ad} {own : CT.Ad → ThreadId} {F : Cerceve} (x : VarId) {d d' : KDurum}
    (h : KRun V own F d d') :
    KRun V own F { d with ifade := .atama x d.ifade }
           { d' with ifade := .atama x d'.ifade } := by
  induction h with
  | refl _ => exact KRun.refl _
  | adim dA dB _ hs _ ih =>
      refine KRun.adim _ { dB with ifade := .atama x dB.ifade } _ ?_ ih
      exact Step.sAtamaCong (K V own F { dA with ifade := .atama x dA.ifade })
        (K V own F { dB with ifade := .atama x dB.ifade }) (K V own F dA) (K V own F dB)
        F.ts1 F.ts2 F.ts2 ⟨F.tid, .atama x dA.ifade, []⟩ ⟨F.tid, dB.ifade, []⟩
        x dA.ifade dB.ifade rfl rfl rfl hs rfl rfl rfl (Or.inl rfl) rfl

theorem krun_eger {V : List CT.Ad} {own : CT.Ad → ThreadId} {F : Cerceve} (dd yy : Ifade) {d d' : KDurum}
    (h : KRun V own F d d') :
    KRun V own F { d with ifade := .eger d.ifade dd yy }
           { d' with ifade := .eger d'.ifade dd yy } := by
  induction h with
  | refl _ => exact KRun.refl _
  | adim dA dB _ hs _ ih =>
      refine KRun.adim _ { dB with ifade := .eger dB.ifade dd yy } _ ?_ ih
      exact Step.sEgerCong (K V own F { dA with ifade := .eger dA.ifade dd yy })
        (K V own F { dB with ifade := .eger dB.ifade dd yy }) (K V own F dA) (K V own F dB)
        F.ts1 F.ts2 F.ts2 ⟨F.tid, .eger dA.ifade dd yy, []⟩ ⟨F.tid, dB.ifade, []⟩
        dA.ifade dB.ifade dd yy rfl rfl rfl hs rfl rfl rfl (Or.inl rfl) rfl

theorem krun_topla_sol {V : List CT.Ad} {own : CT.Ad → ThreadId} {F : Cerceve} (b : Ifade) {d d' : KDurum}
    (h : KRun V own F d d') :
    KRun V own F { d with ifade := .topla d.ifade b }
           { d' with ifade := .topla d'.ifade b } := by
  induction h with
  | refl _ => exact KRun.refl _
  | adim dA dB _ hs _ ih =>
      refine KRun.adim _ { dB with ifade := .topla dB.ifade b } _ ?_ ih
      exact Step.sToplaCongSol (K V own F { dA with ifade := .topla dA.ifade b })
        (K V own F { dB with ifade := .topla dB.ifade b }) (K V own F dA) (K V own F dB)
        F.ts1 F.ts2 F.ts2 ⟨F.tid, .topla dA.ifade b, []⟩ ⟨F.tid, dB.ifade, []⟩
        dA.ifade dB.ifade b rfl rfl rfl hs rfl rfl rfl (Or.inl rfl) rfl
theorem krun_carp_sol {V : List CT.Ad} {own : CT.Ad → ThreadId} {F : Cerceve} (b : Ifade) {d d' : KDurum}
    (h : KRun V own F d d') :
    KRun V own F { d with ifade := .carp d.ifade b }
           { d' with ifade := .carp d'.ifade b } := by
  induction h with
  | refl _ => exact KRun.refl _
  | adim dA dB _ hs _ ih =>
      refine KRun.adim _ { dB with ifade := .carp dB.ifade b } _ ?_ ih
      exact Step.sCarpCongSol (K V own F { dA with ifade := .carp dA.ifade b })
        (K V own F { dB with ifade := .carp dB.ifade b }) (K V own F dA) (K V own F dB)
        F.ts1 F.ts2 F.ts2 ⟨F.tid, .carp dA.ifade b, []⟩ ⟨F.tid, dB.ifade, []⟩
        dA.ifade dB.ifade b rfl rfl rfl hs rfl rfl rfl (Or.inl rfl) rfl

theorem krun_topla_sag {V : List CT.Ad} {own : CT.Ad → ThreadId} {F : Cerceve} (v : Deger) {d d' : KDurum}
    (h : KRun V own F d d') :
    KRun V own F { d with ifade := .topla (.sabit v) d.ifade }
           { d' with ifade := .topla (.sabit v) d'.ifade } := by
  induction h with
  | refl _ => exact KRun.refl _
  | adim dA dB _ hs _ ih =>
      refine KRun.adim _ { dB with ifade := .topla (.sabit v) dB.ifade } _ ?_ ih
      exact Step.sToplaCongSag (K V own F { dA with ifade := .topla (.sabit v) dA.ifade })
        (K V own F { dB with ifade := .topla (.sabit v) dB.ifade }) (K V own F dA) (K V own F dB)
        F.ts1 F.ts2 F.ts2 ⟨F.tid, .topla (.sabit v) dA.ifade, []⟩ ⟨F.tid, dB.ifade, []⟩
        v dA.ifade dB.ifade rfl rfl rfl hs rfl rfl rfl (Or.inl rfl) rfl

theorem krun_carp_sag {V : List CT.Ad} {own : CT.Ad → ThreadId} {F : Cerceve} (v : Deger) {d d' : KDurum}
    (h : KRun V own F d d') :
    KRun V own F { d with ifade := .carp (.sabit v) d.ifade }
           { d' with ifade := .carp (.sabit v) d'.ifade } := by
  induction h with
  | refl _ => exact KRun.refl _
  | adim dA dB _ hs _ ih =>
      refine KRun.adim _ { dB with ifade := .carp (.sabit v) dB.ifade } _ ?_ ih
      exact Step.sCarpCongSag (K V own F { dA with ifade := .carp (.sabit v) dA.ifade })
        (K V own F { dB with ifade := .carp (.sabit v) dB.ifade }) (K V own F dA) (K V own F dB)
        F.ts1 F.ts2 F.ts2 ⟨F.tid, .carp (.sabit v) dA.ifade, []⟩ ⟨F.tid, dB.ifade, []⟩
        v dA.ifade dB.ifade rfl rfl rfl hs rfl rfl rfl (Or.inl rfl) rfl

theorem krun_bol_sol {V : List CT.Ad} {own : CT.Ad → ThreadId} {F : Cerceve} (b : Ifade) {d d' : KDurum}
    (h : KRun V own F d d') :
    KRun V own F { d with ifade := .bol d.ifade b }
           { d' with ifade := .bol d'.ifade b } := by
  induction h with
  | refl _ => exact KRun.refl _
  | adim dA dB _ hs _ ih =>
      refine KRun.adim _ { dB with ifade := .bol dB.ifade b } _ ?_ ih
      exact Step.sBolCongSol (K V own F { dA with ifade := .bol dA.ifade b })
        (K V own F { dB with ifade := .bol dB.ifade b }) (K V own F dA) (K V own F dB)
        F.ts1 F.ts2 F.ts2 ⟨F.tid, .bol dA.ifade b, []⟩ ⟨F.tid, dB.ifade, []⟩
        dA.ifade dB.ifade b rfl rfl rfl hs rfl rfl rfl (Or.inl rfl) rfl
theorem krun_kalan_sol {V : List CT.Ad} {own : CT.Ad → ThreadId} {F : Cerceve} (b : Ifade) {d d' : KDurum}
    (h : KRun V own F d d') :
    KRun V own F { d with ifade := .kalan d.ifade b }
           { d' with ifade := .kalan d'.ifade b } := by
  induction h with
  | refl _ => exact KRun.refl _
  | adim dA dB _ hs _ ih =>
      refine KRun.adim _ { dB with ifade := .kalan dB.ifade b } _ ?_ ih
      exact Step.sKalanCongSol (K V own F { dA with ifade := .kalan dA.ifade b })
        (K V own F { dB with ifade := .kalan dB.ifade b }) (K V own F dA) (K V own F dB)
        F.ts1 F.ts2 F.ts2 ⟨F.tid, .kalan dA.ifade b, []⟩ ⟨F.tid, dB.ifade, []⟩
        dA.ifade dB.ifade b rfl rfl rfl hs rfl rfl rfl (Or.inl rfl) rfl

theorem krun_bol_sag {V : List CT.Ad} {own : CT.Ad → ThreadId} {F : Cerceve} (v : Deger) {d d' : KDurum}
    (h : KRun V own F d d') :
    KRun V own F { d with ifade := .bol (.sabit v) d.ifade }
           { d' with ifade := .bol (.sabit v) d'.ifade } := by
  induction h with
  | refl _ => exact KRun.refl _
  | adim dA dB _ hs _ ih =>
      refine KRun.adim _ { dB with ifade := .bol (.sabit v) dB.ifade } _ ?_ ih
      exact Step.sBolCongSag (K V own F { dA with ifade := .bol (.sabit v) dA.ifade })
        (K V own F { dB with ifade := .bol (.sabit v) dB.ifade }) (K V own F dA) (K V own F dB)
        F.ts1 F.ts2 F.ts2 ⟨F.tid, .bol (.sabit v) dA.ifade, []⟩ ⟨F.tid, dB.ifade, []⟩
        v dA.ifade dB.ifade rfl rfl rfl hs rfl rfl rfl (Or.inl rfl) rfl

theorem krun_kalan_sag {V : List CT.Ad} {own : CT.Ad → ThreadId} {F : Cerceve} (v : Deger) {d d' : KDurum}
    (h : KRun V own F d d') :
    KRun V own F { d with ifade := .kalan (.sabit v) d.ifade }
           { d' with ifade := .kalan (.sabit v) d'.ifade } := by
  induction h with
  | refl _ => exact KRun.refl _
  | adim dA dB _ hs _ ih =>
      refine KRun.adim _ { dB with ifade := .kalan (.sabit v) dB.ifade } _ ?_ ih
      exact Step.sKalanCongSag (K V own F { dA with ifade := .kalan (.sabit v) dA.ifade })
        (K V own F { dB with ifade := .kalan (.sabit v) dB.ifade }) (K V own F dA) (K V own F dB)
        F.ts1 F.ts2 F.ts2 ⟨F.tid, .kalan (.sabit v) dA.ifade, []⟩ ⟨F.tid, dB.ifade, []⟩
        v dA.ifade dB.ifade rfl rfl rfl hs rfl rfl rfl (Or.inl rfl) rfl

theorem krun_esles {V : List CT.Ad} {own : CT.Ad → ThreadId} {F : Cerceve} (n : Int) (dd yy : Ifade) {d d' : KDurum}
    (h : KRun V own F d d') :
    KRun V own F { d with ifade := .esles d.ifade n dd yy }
           { d' with ifade := .esles d'.ifade n dd yy } := by
  induction h with
  | refl _ => exact KRun.refl _
  | adim dA dB _ hs _ ih =>
      refine KRun.adim _ { dB with ifade := .esles dB.ifade n dd yy } _ ?_ ih
      exact Step.sEslesCong (K V own F { dA with ifade := .esles dA.ifade n dd yy })
        (K V own F { dB with ifade := .esles dB.ifade n dd yy }) (K V own F dA) (K V own F dB)
        F.ts1 F.ts2 F.ts2 ⟨F.tid, .esles dA.ifade n dd yy, []⟩ ⟨F.tid, dB.ifade, []⟩
        dA.ifade dB.ifade n dd yy rfl rfl rfl hs rfl rfl rfl (Or.inl rfl) rfl

theorem krun_indeks {V : List CT.Ad} {own : CT.Ad → ThreadId} {F : Cerceve} (x : VarId) {d d' : KDurum}
    (h : KRun V own F d d') :
    KRun V own F { d with ifade := .indeks x d.ifade }
           { d' with ifade := .indeks x d'.ifade } := by
  induction h with
  | refl _ => exact KRun.refl _
  | adim dA dB _ hs _ ih =>
      refine KRun.adim _ { dB with ifade := .indeks x dB.ifade } _ ?_ ih
      exact Step.sIndeksCong (K V own F { dA with ifade := .indeks x dA.ifade })
        (K V own F { dB with ifade := .indeks x dB.ifade }) (K V own F dA) (K V own F dB)
        F.ts1 F.ts2 F.ts2 ⟨F.tid, .indeks x dA.ifade, []⟩ ⟨F.tid, dB.ifade, []⟩
        x dA.ifade dB.ifade rfl rfl rfl hs rfl rfl rfl (Or.inl rfl) rfl

/-- D-337: indeksli yazmanin INDEKS kolu icin cong-yukseltme. -/
theorem krun_indeksAta_idx {V : List CT.Ad} {own : CT.Ad → ThreadId} {F : Cerceve} (x : VarId) (ee : Ifade)
    {d d' : KDurum} (h : KRun V own F d d') :
    KRun V own F { d with ifade := .indeksAta x d.ifade ee }
           { d' with ifade := .indeksAta x d'.ifade ee } := by
  induction h with
  | refl _ => exact KRun.refl _
  | adim dA dB _ hs _ ih =>
      refine KRun.adim _ { dB with ifade := .indeksAta x dB.ifade ee } _ ?_ ih
      exact Step.sIndeksAtaCongIdx
        (K V own F { dA with ifade := .indeksAta x dA.ifade ee })
        (K V own F { dB with ifade := .indeksAta x dB.ifade ee }) (K V own F dA) (K V own F dB)
        F.ts1 F.ts2 F.ts2 ⟨F.tid, .indeksAta x dA.ifade ee, []⟩ ⟨F.tid, dB.ifade, []⟩
        x dA.ifade dB.ifade ee rfl rfl rfl hs rfl rfl rfl (Or.inl rfl) rfl

/-- D-337: indeks DEGER iken DEGER kolu icin cong-yukseltme. -/
theorem krun_indeksAta_deg {V : List CT.Ad} {own : CT.Ad → ThreadId} {F : Cerceve} (x : VarId) (vv : Deger)
    {d d' : KDurum} (h : KRun V own F d d') :
    KRun V own F { d with ifade := .indeksAta x (.sabit vv) d.ifade }
           { d' with ifade := .indeksAta x (.sabit vv) d'.ifade } := by
  induction h with
  | refl _ => exact KRun.refl _
  | adim dA dB _ hs _ ih =>
      refine KRun.adim _
        { dB with ifade := .indeksAta x (.sabit vv) dB.ifade } _ ?_ ih
      exact Step.sIndeksAtaCongDeg
        (K V own F { dA with ifade := .indeksAta x (.sabit vv) dA.ifade })
        (K V own F { dB with ifade := .indeksAta x (.sabit vv) dB.ifade })
        (K V own F dA) (K V own F dB)
        F.ts1 F.ts2 F.ts2 ⟨F.tid, .indeksAta x (.sabit vv) dA.ifade, []⟩ ⟨F.tid, dB.ifade, []⟩
        x vv dA.ifade dB.ifade rfl rfl rfl hs rfl rfl rfl (Or.inl rfl) rfl

-- ============================================================
-- §5. Taban adimlar (tek Step, K → K)
-- ============================================================

/-- D-336: INDEKSLI OKUMA taban adimi. `x[i]` okur, `memOku ⟨bol x, i⟩`
    olayini uretir — yani ADRES ize girer. CT'nin `c_degisken`i de
    `c_indeks`i de buna gomulur (CT'de duz degisken = `x[0]`). -/
theorem adim_indeks (V : List CT.Ad) (own : CT.Ad → ThreadId) (F : Cerceve) (sigma : Store) (x : CT.Ad) (i : Int)
    (iz : Iz) (z : Zaman) (hx : x ∈ V) :
    Step (K V own F ⟨sigma, .indeks x (.sabit (.skaler i)), iz, z⟩)
         (K V own F ⟨sigma, .sabit (.skaler (hucreOku sigma ⟨bol x, i.toNat⟩)),
               .memOku F.tid ⟨bol x, i.toNat⟩
                 (.skaler (hucreOku sigma ⟨bol x, i.toNat⟩)) :: iz, z + 1⟩) :=
  Step.sIndeksOku _ _ F.ts1 F.ts2 ⟨F.tid, .indeks x (.sabit (.skaler i)), []⟩
    x i (bol x) _ rfl rfl (rhoOf_get hx) rfl rfl

/-- D-337: INDEKSLI YAZMA taban adimi — `memYaz ⟨bol x, i⟩`, yani
    yazma ADRESI ize girer. Sahiplik `sahipligiOf`den gelir. -/
theorem adim_indeks_yaz (V : List CT.Ad) (own : CT.Ad → ThreadId) (F : Cerceve) (sigma : Store) (x : CT.Ad)
    (i n : Int) (iz : Iz) (z : Zaman) (hx : x ∈ V) (h_own : own x = F.tid) :
    Step (K V own F ⟨sigma, .indeksAta x (.sabit (.skaler i)) (.sabit (.skaler n)),
               iz, z⟩)
         (K V own F ⟨(⟨bol x, i.toNat⟩, Deger.skaler n) :: sigma, .sabit .birim,
               .memYaz F.tid ⟨bol x, i.toNat⟩ (.skaler n) :: iz, z + 1⟩) :=
  Step.sIndeksYaz _ _ F.ts1 F.ts2
    ⟨F.tid, .indeksAta x (.sabit (.skaler i)) (.sabit (.skaler n)), []⟩
    x i n (bol x) rfl rfl (rhoOf_get hx) (h_own ▸ sahOf_get hx) rfl

theorem adim_yaz (V : List CT.Ad) (own : CT.Ad → ThreadId) (F : Cerceve) (sigma : Store) (x : CT.Ad) (n : Int)
    (iz : Iz) (z : Zaman) (hx : x ∈ V) (h_own : own x = F.tid) :
    Step (K V own F ⟨sigma, .atama x (.sabit (.skaler n)), iz, z⟩)
         (K V own F ⟨(⟨bol x, 0⟩, Deger.skaler n) :: sigma, .sabit .birim,
               .memYaz F.tid ⟨bol x, 0⟩ (.skaler n) :: iz, z + 1⟩) :=
  Step.sAtamaTamam _ _ F.ts1 F.ts2 ⟨F.tid, .atama x (.sabit (.skaler n)), []⟩
    x (.skaler n) (bol x) rfl rfl (rhoOf_get hx) (h_own ▸ sahOf_get hx) rfl

theorem adim_seq_atla (V : List CT.Ad) (own : CT.Ad → ThreadId) (F : Cerceve) (sigma : Store) (v : Deger)
    (b : Ifade) (iz : Iz) (z : Zaman) :
    Step (K V own F ⟨sigma, .seq (.sabit v) b, iz, z⟩)
         (K V own F ⟨sigma, b, iz, z + 1⟩) :=
  Step.sSeqAtla _ _ F.ts1 F.ts2 ⟨F.tid, .seq (.sabit v) b, []⟩ v b rfl rfl rfl

theorem adim_dal (V : List CT.Ad) (own : CT.Ad → ThreadId) (F : Cerceve) (sigma : Store) (n : Int)
    (dd yy : Ifade) (iz : Iz) (z : Zaman) :
    Step (K V own F ⟨sigma, .eger (.sabit (.skaler n)) dd yy, iz, z⟩)
         (K V own F ⟨sigma, (if degerDogruMu (.skaler n) then dd else yy),
               .dalOl F.tid (degerDogruMu (.skaler n)) :: iz, z + 1⟩) :=
  Step.sEgerSec _ _ F.ts1 F.ts2 ⟨F.tid, .eger (.sabit (.skaler n)) dd yy, []⟩
    (.skaler n) dd yy (degerDogruMu (.skaler n)) rfl rfl rfl rfl

/-- D-334: iki skaler deger → toplam. Olay uretmez (CT'nin `c_topla`si
    da iz katkisi yapmaz — sadece alt-ifadelerin izleri birlesir). -/
theorem adim_topla (V : List CT.Ad) (own : CT.Ad → ThreadId) (F : Cerceve) (sigma : Store) (n1 n2 : Int)
    (iz : Iz) (z : Zaman) :
    Step (K V own F ⟨sigma, .topla (.sabit (.skaler n1)) (.sabit (.skaler n2)), iz, z⟩)
         (K V own F ⟨sigma, .sabit (.skaler (n1 + n2)), iz, z + 1⟩) :=
  Step.sToplaTamam _ _ F.ts1 F.ts2
    ⟨F.tid, .topla (.sabit (.skaler n1)) (.sabit (.skaler n2)), []⟩ n1 n2 rfl rfl rfl

theorem adim_carp (V : List CT.Ad) (own : CT.Ad → ThreadId) (F : Cerceve) (sigma : Store) (n1 n2 : Int)
    (iz : Iz) (z : Zaman) :
    Step (K V own F ⟨sigma, .carp (.sabit (.skaler n1)) (.sabit (.skaler n2)), iz, z⟩)
         (K V own F ⟨sigma, .sabit (.skaler (n1 * n2)), iz, z + 1⟩) :=
  Step.sCarpTamam _ _ F.ts1 F.ts2
    ⟨F.tid, .carp (.sabit (.skaler n1)) (.sabit (.skaler n2)), []⟩ n1 n2 rfl rfl rfl

/-- D-338: bolme taban adimi — `bolOl` olayi uretir (adim_topla'dan
    tek farki budur). -/
theorem adim_bol (V : List CT.Ad) (own : CT.Ad → ThreadId) (F : Cerceve) (sigma : Store) (n1 n2 : Int)
    (iz : Iz) (z : Zaman) :
    Step (K V own F ⟨sigma, .bol (.sabit (.skaler n1)) (.sabit (.skaler n2)), iz, z⟩)
         (K V own F ⟨sigma, .sabit (.skaler (n1 / n2)),
               .bolOl F.tid n1 n2 :: iz, z + 1⟩) :=
  Step.sBolTamam _ _ F.ts1 F.ts2
    ⟨F.tid, .bol (.sabit (.skaler n1)) (.sabit (.skaler n2)), []⟩ n1 n2 rfl rfl rfl

/-- D-339: bolme taban adimi — `modOl` olayi uretir (adim_topla'dan
    tek farki budur). -/
theorem adim_kalan (V : List CT.Ad) (own : CT.Ad → ThreadId) (F : Cerceve) (sigma : Store) (n1 n2 : Int)
    (iz : Iz) (z : Zaman) :
    Step (K V own F ⟨sigma, .kalan (.sabit (.skaler n1)) (.sabit (.skaler n2)), iz, z⟩)
         (K V own F ⟨sigma, .sabit (.skaler (n1 % n2)),
               .modOl F.tid n1 n2 :: iz, z + 1⟩) :=
  Step.sKalanTamam _ _ F.ts1 F.ts2
    ⟨F.tid, .kalan (.sabit (.skaler n1)) (.sabit (.skaler n2)), []⟩ n1 n2 rfl rfl rfl

/-- D-335: dongu ACILMASI (olaysiz yapisal adim). -/
theorem adim_iken_ac (V : List CT.Ad) (own : CT.Ad → ThreadId) (F : Cerceve) (sigma : Store) (k g : Ifade)
    (iz : Iz) (z : Zaman) :
    Step (K V own F ⟨sigma, .iken k g, iz, z⟩)
         (K V own F ⟨sigma, .eger k (.seq g (.iken k g)) (.sabit .birim), iz, z + 1⟩) :=
  Step.sIkenAc _ _ F.ts1 F.ts2 ⟨F.tid, .iken k g, []⟩ k g rfl rfl rfl

/-- D-335: literal desen secimi (`dalOl` uretir). -/
theorem adim_esles (V : List CT.Ad) (own : CT.Ad → ThreadId) (F : Cerceve) (sigma : Store) (m n : Int)
    (dd yy : Ifade) (iz : Iz) (z : Zaman) :
    Step (K V own F ⟨sigma, .esles (.sabit (.skaler m)) n dd yy, iz, z⟩)
         (K V own F ⟨sigma, (if decide (m = n) then dd else yy),
               .dalOl F.tid (decide (m = n)) :: iz, z + 1⟩) :=
  Step.sEslesSec _ _ F.ts1 F.ts2 ⟨F.tid, .esles (.sabit (.skaler m)) n dd yy, []⟩
    m n dd yy (decide (m = n)) rfl rfl rfl rfl

/-- Dal testi CT ile BIREBIR: `degerDogruMu (skaler n) ↔ n ≠ 0`. -/
theorem dogruMu_skaler (n : Int) : degerDogruMu (.skaler n) = decide (n ≠ 0) := rfl

-- ============================================================
-- §6. Gozlem gommesi
-- ============================================================

def gomGoz (tid : ThreadId) : CT.Gozlem → GozlemOlay
  -- D-336: INDEKS ofsete gider — CT'nin adres gozlemi ile Core'un
  -- `Konum` gozlemi BIREBIR eslesir.
  | .oOku x i => .gOku tid ⟨bol x, i⟩
  | .oYaz x i => .gYaz tid ⟨bol x, i⟩
  | .oDal a   => .gDal tid a
  -- D-338: CT'nin operand-tasiyan bolme gozlemi, Core'un `gBol`una.
  | .oBol a b => .gBol tid a b
  | .oMod a b => .gMod tid a b

/-- CT izi (yeni-SONDA) → Core gozlem izi (yeni-BASTA). -/
def gomGozIz (tid : ThreadId) (t : CT.Iz) : List GozlemOlay :=
  (t.map (gomGoz tid)).reverse

theorem gomGozIz_append (tid : ThreadId) (t1 t2 : CT.Iz) :
    gomGozIz tid (t1 ++ t2) = gomGozIz tid t2 ++ gomGozIz tid t1 := by
  simp [gomGozIz, List.map_append]

theorem gomGozIz_cons (tid : ThreadId) (o : CT.Gozlem) (t : CT.Iz) :
    gomGozIz tid (o :: t) = gomGozIz tid t ++ [gomGoz tid o] := by
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
theorem gomme_sim {V : List CT.Ad} {own : CT.Ad → ThreadId} {F : Cerceve} :
    ∀ {s e s' t v}, CT.Calis s e s' t v → GomOk e → Kapsar V e →
    YazmaSahibi own F.tid e →
    ∀ (sigma : Store) (iz : Iz) (z : Zaman), StoreUyum V s sigma →
    ∃ (w : Deger) (sigma' : Store) (iz' : Iz) (z' : Zaman),
      KRun V own F ⟨sigma, gom e, iz, z⟩ ⟨sigma', .sabit w, iz', z'⟩
      ∧ StoreUyum V s' sigma'
      ∧ izGozlem iz' = gomGozIz F.tid t ++ izGozlem iz
      ∧ (Sadik e → w = .skaler v) := by
  intro s e s' t v h
  induction h with
  | c_sabit s n =>
      intro _ _ h_ys sigma iz z hu
      exact ⟨.skaler n, sigma, iz, z, KRun.refl _, hu, rfl, fun _ => rfl⟩
  -- D-336: duz degisken = `x[0]`; Core tarafi sIndeksOku.
  | c_degisken s x =>
      intro _ h_kap h_ys sigma iz z hu
      have hx : x ∈ V := by cases h_kap with | k_degisken _ h => exact h
      have hval : hucreOku sigma ⟨bol x, (0 : Int).toNat⟩ = s x 0 := hu x hx 0
      refine ⟨.skaler (s x 0), sigma,
        .memOku F.tid ⟨bol x, 0⟩ (.skaler (s x 0)) :: iz, z + 1, ?_, hu, ?_,
        fun _ => rfl⟩
      · have hst := adim_indeks V own F sigma x 0 iz z hx
        rw [hval] at hst
        exact KRun.adim _ _ _ hst (KRun.refl _)
      · show gozlem (Olay.memOku F.tid ⟨bol x, 0⟩ (.skaler (s x 0))) :: izGozlem iz = _
        rfl
  -- D-336 (CT005): `x[idx]` — indeks kosar, sonra ADRESLI okuma.
  | c_indeks s si x idx ti vi _ ihI =>
      intro h_gom h_kap h_ys sigma iz z hu
      cases h_gom with
      | g_indeks _ _ h_gi h_si =>
        have hx : x ∈ V := by cases h_kap with | k_indeks _ _ h _ => exact h
        have h_ki : Kapsar V idx := by cases h_kap with | k_indeks _ _ _ h => exact h
        obtain ⟨wi, sgI, izI, zI, hrI, huI, hgI, hsI⟩ := ihI h_gi h_ki (fun zz hzz => h_ys zz (Yazar.y_indeks _ _ zz hzz)) sigma iz z hu
        rw [hsI h_si] at hrI
        have hval : hucreOku sgI ⟨bol x, vi.toNat⟩ = si x vi.toNat := huI x hx vi.toNat
        have hst := adim_indeks V own F sgI x vi izI zI hx
        rw [hval] at hst
        refine ⟨.skaler (si x vi.toNat), sgI,
          .memOku F.tid ⟨bol x, vi.toNat⟩ (.skaler (si x vi.toNat)) :: izI, zI + 1,
          krun_trans (krun_indeks (V := V) (own := own) (F := F) x hrI)
            (KRun.adim _ _ _ hst (KRun.refl _)),
          huI, ?_, fun _ => rfl⟩
        show gozlem (Olay.memOku F.tid ⟨bol x, vi.toNat⟩ (.skaler (si x vi.toNat)))
               :: izGozlem izI = _
        rw [hgI, gomGozIz_append]
        simp [gozlem, gomGoz, gomGozIz]
  -- D-337 (CT005-Y): once indeks, sonra deger, sonra ADRESLI yazma.
  | c_indeks_ata s si se x idx e ti te vi ve _ _ ihI ihE =>
      intro h_gom h_kap h_ys sigma iz z hu
      cases h_gom with
      | g_indeks_ata _ _ _ h_gi h_si h_ge h_se =>
        have hx : x ∈ V := by cases h_kap with | k_indeks_ata _ _ _ h _ _ => exact h
        have h_ki : Kapsar V idx := by
          cases h_kap with | k_indeks_ata _ _ _ _ h _ => exact h
        have h_ke : Kapsar V e := by
          cases h_kap with | k_indeks_ata _ _ _ _ _ h => exact h
        obtain ⟨wi, sgI, izI, zI, hrI, huI, hgI, hsI⟩ := ihI h_gi h_ki (fun zz hzz => h_ys zz (Yazar.y_indeks_ata_idx _ _ _ zz hzz)) sigma iz z hu
        rw [hsI h_si] at hrI
        obtain ⟨we, sgE, izE, zE, hrE, huE, hgE, hsE⟩ :=
          ihE h_ge h_ke (fun zz hzz => h_ys zz (Yazar.y_indeks_ata_deg _ _ _ zz hzz)) sgI izI zI huI
        rw [hsE h_se] at hrE
        refine ⟨Deger.birim, (⟨bol x, vi.toNat⟩, Deger.skaler ve) :: sgE,
          .memYaz F.tid ⟨bol x, vi.toNat⟩ (.skaler ve) :: izE, zE + 1,
          krun_trans (krun_indeksAta_idx (V := V) (own := own) (F := F) x (gom e) hrI)
            (krun_trans (krun_indeksAta_deg (V := V) (own := own) (F := F) x (.skaler vi) hrE)
              (KRun.adim _ _ _ (adim_indeks_yaz V own F sgE x vi ve izE zE hx (h_ys x (Yazar.y_indeks_ata_bas x _ _)))
                (KRun.refl _))),
          storeUyum_yazH huE x vi.toNat ve, ?_, ?_⟩
        · show gozlem (Olay.memYaz F.tid ⟨bol x, vi.toNat⟩ (.skaler ve))
                 :: izGozlem izE = _
          rw [hgE, hgI, gomGozIz_append, gomGozIz_append]
          simp [gozlem, gomGoz, gomGozIz]
        · -- `indeksAta` SADIK DEGIL → yukumluluk vakum
          intro hs; nomatch hs
  -- D-338: bolme — `topla` ile ayni akis, AMA son adim `bolOl` uretir;
  -- iz karsiligi `gomGoz (.oBol v1 v2) = .gBol F.tid v1 v2` ile kapanir.
  | c_bol s s1 s2 a b t1 t2 v1 v2 _ _ iha ihb =>
      intro h_gom h_kap h_ys sigma iz z hu
      cases h_gom with
      | g_bol _ _ h_ga h_sa h_gb h_sb =>
        have h_ka : Kapsar V a := by cases h_kap with | k_bol _ _ h _ => exact h
        have h_kb : Kapsar V b := by cases h_kap with | k_bol _ _ _ h => exact h
        obtain ⟨w1, sg1, iz1, z1, hr1, hu1, hg1, hs1⟩ := iha h_ga h_ka (fun zz hzz => h_ys zz (Yazar.y_bol_sol _ _ zz hzz)) sigma iz z hu
        rw [hs1 h_sa] at hr1
        obtain ⟨w2, sg2, iz2, z2, hr2, hu2, hg2, hs2⟩ := ihb h_gb h_kb (fun zz hzz => h_ys zz (Yazar.y_bol_sag _ _ zz hzz)) sg1 iz1 z1 hu1
        rw [hs2 h_sb] at hr2
        refine ⟨.skaler (v1 / v2), sg2, .bolOl F.tid v1 v2 :: iz2, z2 + 1,
          krun_trans (krun_bol_sol (V := V) (own := own) (F := F) (gom b) hr1)
            (krun_trans (krun_bol_sag (V := V) (own := own) (F := F) (.skaler v1) hr2)
              (KRun.adim _ ⟨sg2, .sabit (.skaler (v1 / v2)),
                 .bolOl F.tid v1 v2 :: iz2, z2 + 1⟩ _
                (adim_bol V own F sg2 v1 v2 iz2 z2) (KRun.refl _))),
          hu2, ?_, fun _ => rfl⟩
        show gozlem (Olay.bolOl F.tid v1 v2) :: izGozlem iz2 = _
        rw [hg2, hg1, gomGozIz_append, gomGozIz_append]
        simp [gozlem, gomGoz, gomGozIz, List.append_assoc]
  -- D-339: bolme — `topla` ile ayni akis, AMA son adim `modOl` uretir;
  -- iz karsiligi `gomGoz (.oMod v1 v2) = .gMod F.tid v1 v2` ile kapanir.
  | c_kalan s s1 s2 a b t1 t2 v1 v2 _ _ iha ihb =>
      intro h_gom h_kap h_ys sigma iz z hu
      cases h_gom with
      | g_kalan _ _ h_ga h_sa h_gb h_sb =>
        have h_ka : Kapsar V a := by cases h_kap with | k_kalan _ _ h _ => exact h
        have h_kb : Kapsar V b := by cases h_kap with | k_kalan _ _ _ h => exact h
        obtain ⟨w1, sg1, iz1, z1, hr1, hu1, hg1, hs1⟩ := iha h_ga h_ka (fun zz hzz => h_ys zz (Yazar.y_kalan_sol _ _ zz hzz)) sigma iz z hu
        rw [hs1 h_sa] at hr1
        obtain ⟨w2, sg2, iz2, z2, hr2, hu2, hg2, hs2⟩ := ihb h_gb h_kb (fun zz hzz => h_ys zz (Yazar.y_kalan_sag _ _ zz hzz)) sg1 iz1 z1 hu1
        rw [hs2 h_sb] at hr2
        refine ⟨.skaler (v1 % v2), sg2, .modOl F.tid v1 v2 :: iz2, z2 + 1,
          krun_trans (krun_kalan_sol (V := V) (own := own) (F := F) (gom b) hr1)
            (krun_trans (krun_kalan_sag (V := V) (own := own) (F := F) (.skaler v1) hr2)
              (KRun.adim _ ⟨sg2, .sabit (.skaler (v1 % v2)),
                 .modOl F.tid v1 v2 :: iz2, z2 + 1⟩ _
                (adim_kalan V own F sg2 v1 v2 iz2 z2) (KRun.refl _))),
          hu2, ?_, fun _ => rfl⟩
        show gozlem (Olay.modOl F.tid v1 v2) :: izGozlem iz2 = _
        rw [hg2, hg1, gomGozIz_append, gomGozIz_append]
        simp [gozlem, gomGoz, gomGozIz, List.append_assoc]
  | c_topla s s1 s2 a b t1 t2 v1 v2 _ _ iha ihb =>
      -- D-334: soldan saga kosum + sToplaTamam. Iz katkisi `sira` ile
      -- ayni sekilde birlesir (topla adimlari OLAY URETMEZ).
      intro h_gom h_kap h_ys sigma iz z hu
      cases h_gom with
      | g_topla _ _ h_ga h_sa h_gb h_sb =>
        have h_ka : Kapsar V a := by cases h_kap with | k_topla _ _ h _ => exact h
        have h_kb : Kapsar V b := by cases h_kap with | k_topla _ _ _ h => exact h
        obtain ⟨w1, sg1, iz1, z1, hr1, hu1, hg1, hs1⟩ := iha h_ga h_ka (fun zz hzz => h_ys zz (Yazar.y_topla_sol _ _ zz hzz)) sigma iz z hu
        rw [hs1 h_sa] at hr1
        obtain ⟨w2, sg2, iz2, z2, hr2, hu2, hg2, hs2⟩ := ihb h_gb h_kb (fun zz hzz => h_ys zz (Yazar.y_topla_sag _ _ zz hzz)) sg1 iz1 z1 hu1
        rw [hs2 h_sb] at hr2
        refine ⟨.skaler (v1 + v2), sg2, iz2, z2 + 1,
          krun_trans (krun_topla_sol (V := V) (own := own) (F := F) (gom b) hr1)
            (krun_trans (krun_topla_sag (V := V) (own := own) (F := F) (.skaler v1) hr2)
              (KRun.adim _ ⟨sg2, .sabit (.skaler (v1 + v2)), iz2, z2 + 1⟩ _
                (adim_topla V own F sg2 v1 v2 iz2 z2) (KRun.refl _))),
          hu2, ?_, fun _ => rfl⟩
        rw [hg2, hg1, gomGozIz_append, List.append_assoc]
  | c_carp s s1 s2 a b t1 t2 v1 v2 _ _ iha ihb =>
      -- D-340: soldan saga kosum + sCarpTamam. Iz katkisi `sira` ile
      -- ayni sekilde birlesir (carp adimlari OLAY URETMEZ).
      intro h_gom h_kap h_ys sigma iz z hu
      cases h_gom with
      | g_carp _ _ h_ga h_sa h_gb h_sb =>
        have h_ka : Kapsar V a := by cases h_kap with | k_carp _ _ h _ => exact h
        have h_kb : Kapsar V b := by cases h_kap with | k_carp _ _ _ h => exact h
        obtain ⟨w1, sg1, iz1, z1, hr1, hu1, hg1, hs1⟩ := iha h_ga h_ka (fun zz hzz => h_ys zz (Yazar.y_carp_sol _ _ zz hzz)) sigma iz z hu
        rw [hs1 h_sa] at hr1
        obtain ⟨w2, sg2, iz2, z2, hr2, hu2, hg2, hs2⟩ := ihb h_gb h_kb (fun zz hzz => h_ys zz (Yazar.y_carp_sag _ _ zz hzz)) sg1 iz1 z1 hu1
        rw [hs2 h_sb] at hr2
        refine ⟨.skaler (v1 * v2), sg2, iz2, z2 + 1,
          krun_trans (krun_carp_sol (V := V) (own := own) (F := F) (gom b) hr1)
            (krun_trans (krun_carp_sag (V := V) (own := own) (F := F) (.skaler v1) hr2)
              (KRun.adim _ ⟨sg2, .sabit (.skaler (v1 * v2)), iz2, z2 + 1⟩ _
                (adim_carp V own F sg2 v1 v2 iz2 z2) (KRun.refl _))),
          hu2, ?_, fun _ => rfl⟩
        rw [hg2, hg1, gomGozIz_append, List.append_assoc]
  | c_atama s s1 x e t1 vv he ih =>
      intro h_gom h_kap h_ys sigma iz z hu
      cases h_gom with
      | g_atama _ _ h_ge h_se =>
        have hx : x ∈ V := by cases h_kap with | k_atama _ _ h _ => exact h
        have h_ke : Kapsar V e := by cases h_kap with | k_atama _ _ _ h => exact h
        obtain ⟨w, sgE, izE, zE, hrE, huE, hgE, hsE⟩ := ih h_ge h_ke (fun zz hzz => h_ys zz (Yazar.y_atama_ic _ _ zz hzz)) sigma iz z hu
        rw [hsE h_se] at hrE
        refine ⟨.birim, _, _, zE + 1,
          krun_trans (krun_atama (V := V) (own := own) (F := F) x hrE)
            (KRun.adim _ _ _ (adim_yaz V own F sgE x vv izE zE hx (h_ys x (Yazar.y_atama_bas x _))) (KRun.refl _)),
          storeUyum_yaz huE x vv, ?_, ?_⟩
        · show gozlem (.memYaz F.tid ⟨bol x, 0⟩ (.skaler vv)) :: izGozlem izE = _
          rw [hgE, gomGozIz_append]
          rfl
        · intro hs; nomatch hs
  | c_sira s s1 s2 a b t1 t2 v1 v2 _ _ iha ihb =>
      intro h_gom h_kap h_ys sigma iz z hu
      cases h_gom with
      | g_sira _ _ h_ga h_gb =>
        have h_ka : Kapsar V a := by cases h_kap with | k_sira _ _ h _ => exact h
        have h_kb : Kapsar V b := by cases h_kap with | k_sira _ _ _ h => exact h
        obtain ⟨w1, sg1, iz1, z1, hr1, hu1, hg1, _⟩ := iha h_ga h_ka (fun zz hzz => h_ys zz (Yazar.y_sira_sol _ _ zz hzz)) sigma iz z hu
        obtain ⟨w2, sg2, iz2, z2, hr2, hu2, hg2, hs2⟩ :=
          ihb h_gb h_kb (fun zz hzz => h_ys zz (Yazar.y_sira_sag _ _ zz hzz)) sg1 iz1 (z1 + 1) hu1
        refine ⟨w2, sg2, iz2, z2,
          krun_trans (krun_seq (V := V) (own := own) (F := F) (gom b) hr1)
            (KRun.adim _ ⟨sg1, gom b, iz1, z1 + 1⟩ _
              (adim_seq_atla V own F sg1 w1 (gom b) iz1 z1) hr2),
          hu2, ?_, ?_⟩
        · rw [hg2, hg1, gomGozIz_append, List.append_assoc]
        · intro hs
          cases hs with | s_sira _ _ h => exact hs2 h
  | c_eger_dogru s s1 s2 k d y tk td vk vd _ h_dogru _ ihk ihd =>
      intro h_gom h_kap h_ys sigma iz z hu
      cases h_gom with
      | g_eger _ _ _ h_gk h_sk h_gd h_gy =>
        have h_kk : Kapsar V k := by cases h_kap with | k_eger _ _ _ h _ _ => exact h
        have h_kd : Kapsar V d := by cases h_kap with | k_eger _ _ _ _ h _ => exact h
        obtain ⟨wk, sgK, izK, zK, hrK, huK, hgK, hsK⟩ := ihk h_gk h_kk (fun zz hzz => h_ys zz (Yazar.y_eger_k _ _ _ zz hzz)) sigma iz z hu
        rw [hsK h_sk] at hrK
        have h_dal : degerDogruMu (Deger.skaler vk) = true := by
          rw [dogruMu_skaler]; exact decide_eq_true h_dogru
        obtain ⟨wd, sgD, izD, zD, hrD, huD, hgD, hsD⟩ :=
          ihd h_gd h_kd (fun zz hzz => h_ys zz (Yazar.y_eger_d _ _ _ zz hzz)) sgK (.dalOl F.tid true :: izK) (zK + 1) huK
        have hstep := adim_dal V own F sgK vk (gom d) (gom y) izK zK
        rw [h_dal] at hstep
        refine ⟨wd, sgD, izD, zD,
          krun_trans (krun_eger (V := V) (own := own) (F := F) (gom d) (gom y) hrK)
            (KRun.adim _ ⟨sgK, gom d, .dalOl F.tid true :: izK, zK + 1⟩ _
              hstep hrD),
          huD, ?_, ?_⟩
        · rw [hgD, gomGozIz_append, gomGozIz_cons]
          show gomGozIz F.tid td ++ (gozlem (Olay.dalOl F.tid true) :: izGozlem izK) = _
          rw [hgK]
          simp [gozlem, gomGoz, List.append_assoc]
        · intro hs
          cases hs with | s_eger _ _ _ h _ => exact hsD h
  | c_eger_yanlis s s1 s2 k d y tk ty vk vy _ h_yanlis _ ihk ihy =>
      intro h_gom h_kap h_ys sigma iz z hu
      cases h_gom with
      | g_eger _ _ _ h_gk h_sk h_gd h_gy =>
        have h_kk : Kapsar V k := by cases h_kap with | k_eger _ _ _ h _ _ => exact h
        have h_ky : Kapsar V y := by cases h_kap with | k_eger _ _ _ _ _ h => exact h
        obtain ⟨wk, sgK, izK, zK, hrK, huK, hgK, hsK⟩ := ihk h_gk h_kk (fun zz hzz => h_ys zz (Yazar.y_eger_k _ _ _ zz hzz)) sigma iz z hu
        rw [hsK h_sk] at hrK
        have h_dal : degerDogruMu (Deger.skaler vk) = false := by
          rw [dogruMu_skaler, h_yanlis]; rfl
        obtain ⟨wy, sgY, izY, zY, hrY, huY, hgY, hsY⟩ :=
          ihy h_gy h_ky (fun zz hzz => h_ys zz (Yazar.y_eger_y _ _ _ zz hzz)) sgK (.dalOl F.tid false :: izK) (zK + 1) huK
        have hstep := adim_dal V own F sgK vk (gom d) (gom y) izK zK
        rw [h_dal] at hstep
        refine ⟨wy, sgY, izY, zY,
          krun_trans (krun_eger (V := V) (own := own) (F := F) (gom d) (gom y) hrK)
            (KRun.adim _ ⟨sgK, gom y, .dalOl F.tid false :: izK, zK + 1⟩ _
              hstep hrY),
          huY, ?_, ?_⟩
        · rw [hgY, gomGozIz_append, gomGozIz_cons]
          show gomGozIz F.tid ty ++ (gozlem (Olay.dalOl F.tid false) :: izGozlem izK) = _
          rw [hgK]
          simp [gozlem, gomGoz, List.append_assoc]
        · intro hs
          cases hs with | s_eger _ _ _ _ h => exact hsY h
  -- D-335 (CT002): DONGU. Core tarafi: sIkenAc (acilma) → acilmis `eger`in
  -- kosulu kosar → sEgerSec (`dalOl true`) → govde → sSeqAtla → IC DONGU.
  -- Ucuncu IH (ihI) ic dongu icin — CT'nin ozyinelemeli kuralinin karsiligi.
  | c_iken_dogru s s1 s2 s3 k g tk tg ti vk vg vi _ h_dogru _ _ ihK ihG ihI =>
      intro h_gom h_kap h_ys sigma iz z hu
      cases h_gom with
      | g_iken _ _ h_gk h_sk h_gg =>
        have h_kk : Kapsar V k := by cases h_kap with | k_iken _ _ h _ => exact h
        have h_kg : Kapsar V g := by cases h_kap with | k_iken _ _ _ h => exact h
        obtain ⟨wk, sgK, izK, zK, hrK, huK, hgK, hsK⟩ := ihK h_gk h_kk (fun zz hzz => h_ys zz (Yazar.y_iken_k _ _ zz hzz)) sigma iz (z + 1) hu
        rw [hsK h_sk] at hrK
        have h_dal : degerDogruMu (Deger.skaler vk) = true := by
          rw [dogruMu_skaler]; exact decide_eq_true h_dogru
        obtain ⟨wg, sgG, izG, zG, hrG, huG, hgG, _⟩ :=
          ihG h_gg h_kg (fun zz hzz => h_ys zz (Yazar.y_iken_g _ _ zz hzz)) sgK (.dalOl F.tid true :: izK) (zK + 1) huK
        obtain ⟨wi, sgI, izI, zI, hrI, huI, hgI, _⟩ :=
          ihI (GomOk.g_iken k g h_gk h_sk h_gg) (Kapsar.k_iken k g h_kk h_kg) h_ys
            sgG izG (zG + 1) huG
        have hstep := adim_dal V own F sgK vk (.seq (gom g) (.iken (gom k) (gom g)))
          (.sabit .birim) izK zK
        rw [h_dal] at hstep
        refine ⟨wi, sgI, izI, zI,
          KRun.adim _ ⟨sigma, .eger (gom k) (.seq (gom g) (.iken (gom k) (gom g)))
              (.sabit .birim), iz, z + 1⟩ _
            (adim_iken_ac V own F sigma (gom k) (gom g) iz z)
            (krun_trans
              (krun_eger (V := V) (own := own) (F := F) (.seq (gom g) (.iken (gom k) (gom g)))
                (.sabit .birim) hrK)
              (KRun.adim _ ⟨sgK, .seq (gom g) (.iken (gom k) (gom g)),
                  .dalOl F.tid true :: izK, zK + 1⟩ _ hstep
                (krun_trans (krun_seq (V := V) (own := own) (F := F) (.iken (gom k) (gom g)) hrG)
                  (KRun.adim _ ⟨sgG, .iken (gom k) (gom g), izG, zG + 1⟩ _
                    (adim_seq_atla V own F sgG wg (.iken (gom k) (gom g)) izG zG)
                    hrI)))),
          huI, ?_, ?_⟩
        · rw [hgI, hgG, gomGozIz_append, gomGozIz_cons, gomGozIz_append]
          show gomGozIz F.tid ti ++ (gomGozIz F.tid tg ++
                 (gozlem (Olay.dalOl F.tid true) :: izGozlem izK)) = _
          rw [hgK]
          simp [gozlem, gomGoz, List.append_assoc]
        · intro hs; nomatch hs
  | c_iken_yanlis s s1 k g tk vk _ h_yanlis ihK =>
      intro h_gom h_kap h_ys sigma iz z hu
      cases h_gom with
      | g_iken _ _ h_gk h_sk h_gg =>
        have h_kk : Kapsar V k := by cases h_kap with | k_iken _ _ h _ => exact h
        obtain ⟨wk, sgK, izK, zK, hrK, huK, hgK, hsK⟩ := ihK h_gk h_kk (fun zz hzz => h_ys zz (Yazar.y_iken_k _ _ zz hzz)) sigma iz (z + 1) hu
        rw [hsK h_sk] at hrK
        have h_dal : degerDogruMu (Deger.skaler vk) = false := by
          rw [dogruMu_skaler, h_yanlis]; rfl
        have hstep := adim_dal V own F sgK vk (.seq (gom g) (.iken (gom k) (gom g)))
          (.sabit .birim) izK zK
        rw [h_dal] at hstep
        refine ⟨.birim, sgK, .dalOl F.tid false :: izK, zK + 1,
          KRun.adim _ ⟨sigma, .eger (gom k) (.seq (gom g) (.iken (gom k) (gom g)))
              (.sabit .birim), iz, z + 1⟩ _
            (adim_iken_ac V own F sigma (gom k) (gom g) iz z)
            (krun_trans
              (krun_eger (V := V) (own := own) (F := F) (.seq (gom g) (.iken (gom k) (gom g)))
                (.sabit .birim) hrK)
              (KRun.adim _ ⟨sgK, .sabit .birim, .dalOl F.tid false :: izK, zK + 1⟩ _
                hstep (KRun.refl _))),
          huK, ?_, ?_⟩
        · rw [gomGozIz_append]
          show gozlem (Olay.dalOl F.tid false) :: izGozlem izK = _
          rw [hgK]
          simp [gozlem, gomGoz, gomGozIz]
        · intro hs; nomatch hs
  -- D-335 (CT004): DESEN ESLEMESI — `eger` deseninin aynisi.
  | c_esles_tuttu s s1 s2 sk n d y ts td vs vd _ h_tuttu _ ihS ihD =>
      intro h_gom h_kap h_ys sigma iz z hu
      cases h_gom with
      | g_esles _ _ _ _ h_gs h_ss h_gd h_gy =>
        have h_ks : Kapsar V sk := by cases h_kap with | k_esles _ _ _ _ h _ _ => exact h
        have h_kd : Kapsar V d := by cases h_kap with | k_esles _ _ _ _ _ h _ => exact h
        obtain ⟨ws, sgS, izS, zS, hrS, huS, hgS, hsS⟩ := ihS h_gs h_ks (fun zz hzz => h_ys zz (Yazar.y_esles_s _ _ _ _ zz hzz)) sigma iz z hu
        rw [hsS h_ss] at hrS
        have h_dal : decide (vs = n) = true := decide_eq_true h_tuttu
        obtain ⟨wd, sgD, izD, zD, hrD, huD, hgD, hsD⟩ :=
          ihD h_gd h_kd (fun zz hzz => h_ys zz (Yazar.y_esles_d _ _ _ _ zz hzz)) sgS (.dalOl F.tid true :: izS) (zS + 1) huS
        have hstep := adim_esles V own F sgS vs n (gom d) (gom y) izS zS
        rw [h_dal] at hstep
        refine ⟨wd, sgD, izD, zD,
          krun_trans (krun_esles (V := V) (own := own) (F := F) n (gom d) (gom y) hrS)
            (KRun.adim _ ⟨sgS, gom d, .dalOl F.tid true :: izS, zS + 1⟩ _
              hstep hrD),
          huD, ?_, ?_⟩
        · rw [hgD, gomGozIz_append, gomGozIz_cons]
          show gomGozIz F.tid td ++ (gozlem (Olay.dalOl F.tid true) :: izGozlem izS) = _
          rw [hgS]
          simp [gozlem, gomGoz, List.append_assoc]
        · intro hs
          cases hs with | s_esles _ _ _ _ h _ => exact hsD h
  | c_esles_tutmadi s s1 s2 sk n d y ts ty vs vy _ h_tutmadi _ ihS ihY =>
      intro h_gom h_kap h_ys sigma iz z hu
      cases h_gom with
      | g_esles _ _ _ _ h_gs h_ss h_gd h_gy =>
        have h_ks : Kapsar V sk := by cases h_kap with | k_esles _ _ _ _ h _ _ => exact h
        have h_ky : Kapsar V y := by cases h_kap with | k_esles _ _ _ _ _ _ h => exact h
        obtain ⟨ws, sgS, izS, zS, hrS, huS, hgS, hsS⟩ := ihS h_gs h_ks (fun zz hzz => h_ys zz (Yazar.y_esles_s _ _ _ _ zz hzz)) sigma iz z hu
        rw [hsS h_ss] at hrS
        have h_dal : decide (vs = n) = false := decide_eq_false h_tutmadi
        obtain ⟨wy, sgY, izY, zY, hrY, huY, hgY, hsY⟩ :=
          ihY h_gy h_ky (fun zz hzz => h_ys zz (Yazar.y_esles_y _ _ _ _ zz hzz)) sgS (.dalOl F.tid false :: izS) (zS + 1) huS
        have hstep := adim_esles V own F sgS vs n (gom d) (gom y) izS zS
        rw [h_dal] at hstep
        refine ⟨wy, sgY, izY, zY,
          krun_trans (krun_esles (V := V) (own := own) (F := F) n (gom d) (gom y) hrS)
            (KRun.adim _ ⟨sgS, gom y, .dalOl F.tid false :: izS, zS + 1⟩ _
              hstep hrY),
          huY, ?_, ?_⟩
        · rw [hgY, gomGozIz_append, gomGozIz_cons]
          show gomGozIz F.tid ty ++ (gozlem (Olay.dalOl F.tid false) :: izGozlem izS) = _
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
theorem kopru_ni (G : CT.EtiketOrtam) (V : List CT.Ad) (own : CT.Ad → ThreadId) (F : Cerceve) (e : CT.Ifade)
    (h_ct : CT.CtOk G e) (h_gom : GomOk e) (h_kap : Kapsar V e)
    (h_ys : YazmaSahibi own F.tid e)
    {s1 s2 s1' s2' : CT.Store} {t1 t2 : CT.Iz} {v1 v2 : Int}
    (h_low : CT.DusukEs G s1 s2)
    (h_r1 : CT.Calis s1 e s1' t1 v1) (h_r2 : CT.Calis s2 e s2' t2 v2)
    (sigma1 sigma2 : Store) (iz : Iz) (z : Zaman)
    (hu1 : StoreUyum V s1 sigma1) (hu2 : StoreUyum V s2 sigma2) :
    ∃ (w1 w2 : Deger) (sg1 sg2 : Store) (iz1 iz2 : Iz) (z1 z2 : Zaman),
      StepStar (K V own F ⟨sigma1, gom e, iz, z⟩) (K V own F ⟨sg1, .sabit w1, iz1, z1⟩)
      ∧ StepStar (K V own F ⟨sigma2, gom e, iz, z⟩) (K V own F ⟨sg2, .sabit w2, iz2, z2⟩)
      ∧ izGozlem iz1 = izGozlem iz2 := by
  obtain ⟨w1, sg1, iz1, z1, hr1, _, hg1, _⟩ := gomme_sim h_r1 h_gom h_kap h_ys sigma1 iz z hu1
  obtain ⟨w2, sg2, iz2, z2, hr2, _, hg2, _⟩ := gomme_sim h_r2 h_gom h_kap h_ys sigma2 iz z hu2
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

/-- D-336: koprunun INDEKSLI OKUMA kapsadiginin taniki — ve CT005'e
    UYAN bir program. `tablo[i]` : `tablo` (1) GIZLI bir tablo olabilir,
    ama INDEKS (`i`, degisken 1... burada GENEL bir degisken) genel
    olmalidir. Bu tam olarak "S-box'a genel indeksle bak" desenidir;
    CT005'in yasakladigi sey ise gizli indekstir (`ct005_gerekli`). -/
theorem kopru_indeks_bos_degil :
    ∃ (G : CT.EtiketOrtam) (e : CT.Ifade),
      CT.CtOk G e ∧ GomOk e ∧ Kapsar [0, 1] e
      ∧ (∃ x idx, e = .indeks x idx) := by
  refine ⟨fun x => if x = 0 then .gizli else .genel,
          .indeks 0 (.degisken 1), ?_, ?_, ?_, ⟨_, _, rfl⟩⟩
  · exact CT.CtOk.ct_indeks 0 _ (CT.CtOk.ct_degisken 1) (by decide)
  · exact GomOk.g_indeks 0 _ (GomOk.g_degisken 1) (Sadik.s_degisken 1)
  · exact Kapsar.k_indeks 0 _ (by decide) (Kapsar.k_degisken 1 (by decide))

/-- D-337: koprunun INDEKSLI YAZMAYI kapsadiginin taniki — CT005-Y'ye
    UYAN program: `tablo[i] = 0` (indeks GENEL, yazilan deger GENEL).
    Hedef dizi de genel oldugu icin CT003 akis sarti da saglanir. -/
theorem kopru_indeks_yaz_bos_degil :
    ∃ (G : CT.EtiketOrtam) (e : CT.Ifade),
      CT.CtOk G e ∧ GomOk e ∧ Kapsar [0, 1] e
      ∧ (∃ x idx d, e = .indeksAta x idx d) := by
  refine ⟨fun _ => .genel, .indeksAta 0 (.degisken 1) (.sabit 0),
          ?_, ?_, ?_, ⟨_, _, _, rfl⟩⟩
  · exact CT.CtOk.ct_indeks_ata 0 _ _ (CT.CtOk.ct_degisken 1)
      (CT.CtOk.ct_sabit 0) (by decide) (by decide)
  · exact GomOk.g_indeks_ata 0 _ _ (GomOk.g_degisken 1) (Sadik.s_degisken 1)
      (GomOk.g_sabit 0) (Sadik.s_sabit 0)
  · exact Kapsar.k_indeks_ata 0 _ _ (by decide)
      (Kapsar.k_degisken 1 (by decide)) (Kapsar.k_sabit 0)

/-- D-338: koprunun BOLMEYI kapsadiginin taniki — CT006'ya UYAN program
    (`d / 2`, `d` GENEL). Gizli operandli olan `ct006_gerekli`dedir. -/
theorem kopru_bol_bos_degil :
    ∃ (G : CT.EtiketOrtam) (e : CT.Ifade),
      CT.CtOk G e ∧ GomOk e ∧ Kapsar [0, 1] e
      ∧ (∃ a b, e = .bol a b) := by
  refine ⟨fun _ => .genel, .bol (.degisken 1) (.sabit 2),
          ?_, ?_, ?_, ⟨_, _, rfl⟩⟩
  · exact CT.CtOk.ct_bol _ _ (CT.CtOk.ct_degisken 1) (CT.CtOk.ct_sabit 2)
      (by decide) (by decide)
  · exact GomOk.g_bol _ _ (GomOk.g_degisken 1) (Sadik.s_degisken 1)
      (GomOk.g_sabit 2) (Sadik.s_sabit 2)
  · exact Kapsar.k_bol _ _ (Kapsar.k_degisken 1 (by decide)) (Kapsar.k_sabit 2)

/-- D-339: koprunun KALAN (mod) kapsadiginin taniki — CT006-M'ye UYAN
    (`d % 2`, `d` GENEL). Gizli operandli olan `ct006m_gerekli`dedir. -/
theorem kopru_kalan_bos_degil :
    ∃ (G : CT.EtiketOrtam) (e : CT.Ifade),
      CT.CtOk G e ∧ GomOk e ∧ Kapsar [0, 1] e
      ∧ (∃ a b, e = .kalan a b) := by
  refine ⟨fun _ => .genel, .kalan (.degisken 1) (.sabit 2),
          ?_, ?_, ?_, ⟨_, _, rfl⟩⟩
  · exact CT.CtOk.ct_kalan _ _ (CT.CtOk.ct_degisken 1) (CT.CtOk.ct_sabit 2)
      (by decide) (by decide)
  · exact GomOk.g_kalan _ _ (GomOk.g_degisken 1) (Sadik.s_degisken 1)
      (GomOk.g_sabit 2) (Sadik.s_sabit 2)
  · exact Kapsar.k_kalan _ _ (Kapsar.k_degisken 1 (by decide)) (Kapsar.k_sabit 2)

-- ============================================================
-- §9. ESZAMANLI KOPRU (D-342) — N THREAD
-- ============================================================

/-
NE DEGISTI
──────────
D-333..D-341'de kopru `[] ++ ctx :: []` cercevesine SABITLENMISTI. Artik
`Cerceve` (ts1, ts2, tid) parametrik: `gomme_sim` odakli thread'i N-thread'li
bir listede HERHANGI bir konumda calistirir. FIX-F yan-kosulu hala daima
`Or.inl rfl` (spawn yok), ama ts2 artik BOS DEGIL — yani cerceve gercekten
tasiniyor.

SEMANTIK CATISMA ve COZUMU (Mehmet karari — TEK YAZICI)
──────────────────────────────────────────────────────
Core'da YAZMA bolge sahipligi ister (`sAtamaTamam`/`sIndeksYaz`), CT'nin
eszamanli modelinde store TAMAMEN PAYLASIMLIDIR. Cakisma yalniz YAZMADA:
`sVarOku`/`sIndeksOku` sahiplik istemez, yani **thread-arasi OKUMA girisimi
serbesttir** (`esz_capraz_girisim_gercek` bu yolla gomulur).
Cozum: `own : Ad → ThreadId` sahiplik haritasi + `YazmaSahibi` on-kosulu —
bir thread yalniz KENDI sahip oldugu degiskenlere yazar.

⚠ ACIK BORC: `CT.esz_zamanlama_etkili` tanigindaki IKI-YAZICI program
GOMULEMEZ (iki thread ayni degiskene yazar). Bu zaten bir VERI YARISIDIR
ve KEMGU'nun bolge disiplini onu tasarim geregi yasaklar; yani kayip
"ifade edilemeyen guvenli program" degil, "modelin dislamayi AMACLADIGI
program"dir. Yine de kapsam sinirıdir ve boyle yazilmistir.
-/

/-- Sistemin i. thread'i icin ODAK CERCEVESI: onceki/sonraki thread'ler
    DONMUS baglamlar olarak durur. Thread kimligi = CT thread indeksi. -/
def cerceveN (ts1 ts2 : List ThreadCtx) (i : Nat) : Cerceve := ⟨ts1, ts2, i⟩

/-- CT sisteminin TUM bloklari icin yazma-sahipligi: i. thread'in her
    blogu yalniz `own x = i` olan x'lere yazar. -/
def SistemYazmaSahibi (own : CT.Ad → ThreadId) (sys : CT.Sistem) : Prop :=
  ∀ i ts, sys[i]? = some ts → ∀ e ∈ ts, YazmaSahibi own i e

/-- Sistemin TUM bloklari gomulebilir ve V-kapsamlı. -/
def SistemGomOk (V : List CT.Ad) (sys : CT.Sistem) : Prop :=
  ∀ ts ∈ sys, ∀ e ∈ ts, GomOk e ∧ Kapsar V e

/-- **ESZAMANLI KOPRU SONUCU (kopru_esz_ni):** CT-tipli, gomulebilir ve
    tek-yazici bir sistem, HERHANGI bir zamanlama altinda, dusuk-esdeger
    iki baslangic store'unda AYNI CT-izini uretir.

    NOT (durustluk): bu teorem `CT.ct_esz_ni`yi kullanir ve gomme tarafinda
    her blogun Core'da kosturulabilir oldugunu (`gomme_sim`, artik
    N-thread'li cerceveyle) kullanir. Yani ana modele TASINAN sey her
    blogun Core kosumudur; serpistirmenin KENDISI CT tarafinda kalir.
    Tam Core-serpistirmesi (Core'un `Step`i ile thread degistirme) V2'dir —
    bunun icin `Konfigurasyon.thread` listesinin adimlar arasi yeniden
    bolunmesi hakkinda bir korunum lemmasi gerekir. -/
theorem kopru_esz_ni (G : CT.EtiketOrtam) (V : List CT.Ad)
    (own : CT.Ad → ThreadId) (sys : CT.Sistem)
    (h_ct : CT.SistemCtOk G sys) (h_gom : SistemGomOk V sys)
    (h_ys : SistemYazmaSahibi own sys)
    {s1 s2 s1' s2' : CT.Store} {zam : CT.Zamanlama} {t1 t2 : CT.EszIz}
    (h_low : CT.DusukEs G s1 s2)
    (h_r1 : CT.EszCalis s1 sys zam s1' t1)
    (h_r2 : CT.EszCalis s2 sys zam s2' t2) :
    t1 = t2 ∧ CT.DusukEs G s1' s2' :=
  CT.ct_esz_ni G h_r1 h_r2 h_ct h_low

/-- **N-THREAD CERCEVENIN GERCEKTEN TASINDIGININ TANIGI (D-342):**
    `gomme_sim` BOS OLMAYAN `ts1`/`ts2` ile de kosar — yani odakli thread
    cok-thread'li bir listede ORTADA olabilir. Tek-thread kurulumunda
    (`cerceveTek`) bu tanik AYNEN gecerdi; onemli olan `ts2 ≠ []` halinin
    de gectigi: cerceve sadece dekor degil, Step kurallarina gerceken
    geciyor. -/
theorem kopru_cerceve_tasiniyor (yan1 yan2 : ThreadCtx) (tid : ThreadId) :
    KRun [0] (fun _ => tid) (cerceveN [yan1] [yan2] tid)
      ⟨[], gom (.sabitDeg 0 (.sabit 5)), [], 0⟩
      ⟨(⟨bol 0, 0⟩, Deger.skaler 5) :: [], .sabit .birim,
       .memYaz tid ⟨bol 0, 0⟩ (.skaler 5) :: [], 0 + 1⟩ :=
  KRun.adim _ _ _
    (adim_yaz [0] (fun _ => tid) (cerceveN [yan1] [yan2] tid) [] 0 5 [] 0
      (List.Mem.head _) rfl)
    (KRun.refl _)

/-- Tek-thread kurulumu ARTIK OZEL HAL: `cerceveTek` ile eski `kopru_ni`
    ifadesi aynen elde edilir (geriye uyum taniki). -/
theorem kopru_tek_thread_ozel_hal (G : CT.EtiketOrtam) (V : List CT.Ad)
    (own : CT.Ad → ThreadId) (e : CT.Ifade)
    (h_ct : CT.CtOk G e) (h_gom : GomOk e) (h_kap : Kapsar V e)
    (h_ys : YazmaSahibi own cerceveTek.tid e)
    {s1 s2 s1' s2' : CT.Store} {t1 t2 : CT.Iz} {v1 v2 : Int}
    (h_low : CT.DusukEs G s1 s2)
    (h_r1 : CT.Calis s1 e s1' t1 v1) (h_r2 : CT.Calis s2 e s2' t2 v2)
    (sigma1 sigma2 : Store) (iz : Iz) (z : Zaman)
    (hu1 : StoreUyum V s1 sigma1) (hu2 : StoreUyum V s2 sigma2) :
    ∃ (w1 w2 : Deger) (sg1 sg2 : Store) (iz1 iz2 : Iz) (z1 z2 : Zaman),
      StepStar (K V own cerceveTek ⟨sigma1, gom e, iz, z⟩)
               (K V own cerceveTek ⟨sg1, .sabit w1, iz1, z1⟩)
      ∧ StepStar (K V own cerceveTek ⟨sigma2, gom e, iz, z⟩)
                 (K V own cerceveTek ⟨sg2, .sabit w2, iz2, z2⟩)
      ∧ izGozlem iz1 = izGozlem iz2 :=
  kopru_ni G V own cerceveTek e h_ct h_gom h_kap h_ys h_low h_r1 h_r2
    sigma1 sigma2 iz z hu1 hu2

-- ============================================================
-- §10. CORE-SERPISTIRMESI (D-343) — ODAK DEGISTIRME KORUNUMU
-- ============================================================

/-
D-342'nin KALAN BORCU: `kopru_esz_ni` serpistirmeyi CT tarafinda
birakiyordu. Buradaki is, serpistirmeyi CORE'un KENDI `Step`ine tasimak:
zamanlama her adimda ODAGI DEGISTIRIR, ve odak degisiminin gecerli
olmasi icin thread listesinin YENIDEN BOLUNEBILMESI gerekir.

ISIN CEKIRDEGI iki gozlemdir:
  (1) `K V own F d` bir KONFIGURASYONDUR; thread listesi
      `F.ts1 ++ ⟨F.tid, d.ifade, []⟩ :: F.ts2`. Ayni konfigurasyon,
      BASKA bir F' cercevesiyle de yazilabilir — yeter ki thread listesi
      ayni olsun. `odak_kur` bunu verir.
  (2) Core thread'i TEK ifade tutar, blok LISTESI tutmaz. Bir thread'in
      ardisik bloklari `seq` ZINCIRI olarak kodlanir (`blokZinciri`);
      `krun_seq` + `adim_seq_atla` ile bir blok kosup KALANI birakmak
      Core'un kendi kurallariyla yapilir — yani "sonraki blogu yukle"
      diye UYDURMA bir adim GEREKMEZ.
-/

/-- Tam thread listesinden konfigurasyon (cerceve YOK — odak serbest). -/
def KN (V : List CT.Ad) (own : CT.Ad → ThreadId)
    (thr : List ThreadCtx) (sigma : Store) (iz : Iz) (z : Zaman) : Konfigurasyon :=
  { thread   := thr
    store    := sigma
    sahiplik := sahOf V own
    kanal    := []
    zaman    := z
    iz       := iz
    fault    := none
    bolge    := rhoOf V }

/-- `K` ile `KN` ayni seydir: cerceve yalnizca thread listesini BOLER. -/
theorem K_KN (V : List CT.Ad) (own : CT.Ad → ThreadId) (F : Cerceve) (d : KDurum) :
    K V own F d
      = KN V own (F.ts1 ++ ⟨F.tid, d.ifade, []⟩ :: F.ts2) d.store d.iz d.zaman := rfl

/-- **ODAK KURMA (korunumun cekirdegi):** thread listesi `ts1 ++ ctx :: ts2`
    seklinde bolunebiliyorsa, o listeyle kurulan `KN` konfigurasyonu
    `⟨ts1, ts2, ctx.tid⟩` cercevesiyle kurulan `K` ile AYNIDIR.
    Yani ODAK, konfigurasyonu DEGISTIRMEDEN herhangi bir thread'e
    tasinabilir — Core-serpistirmesini mumkun kilan sey budur. -/
theorem odak_kur (V : List CT.Ad) (own : CT.Ad → ThreadId)
    (ts1 ts2 : List ThreadCtx) (tid : ThreadId) (e : Ifade)
    (sigma : Store) (iz : Iz) (z : Zaman) :
    KN V own (ts1 ++ ⟨tid, e, []⟩ :: ts2) sigma iz z
      = K V own ⟨ts1, ts2, tid⟩ ⟨sigma, e, iz, z⟩ := rfl

/-- Bir thread'in ARDISIK BLOKLARI: `seq` zinciri. Bos liste `birim`e iner
    (thread bitti). Core'un `sSeqAtla`si zincirde bir blok ilerletir —
    "sonraki blogu yukle" diye ek bir kural GEREKMEZ. -/
def blokZinciri : List CT.Ifade → Ifade
  | []      => .sabit .birim
  | b :: bs => .seq (gom b) (blokZinciri bs)

/-- **BIR BLOK KOSUMU (Core'da):** `seq (gom b) kalan` odaktayken, b'nin
    CT kosumu Core'da yapilir ve odak `kalan`a gecer. Iki mevcut parca
    birlestirilir: `krun_seq` (gomme_sim'i seq'in soluna yukseltir) ve
    `adim_seq_atla` (deger olan solu atar). -/
theorem blok_kos {V : List CT.Ad} {own : CT.Ad → ThreadId} {F : Cerceve}
    {s s' : CT.Store} {b : CT.Ifade} {t : CT.Iz} {v : Int}
    (h_run : CT.Calis s b s' t v)
    (h_gom : GomOk b) (h_kap : Kapsar V b) (h_ys : YazmaSahibi own F.tid b)
    (kalan : Ifade) (sigma : Store) (iz : Iz) (z : Zaman)
    (hu : StoreUyum V s sigma) :
    ∃ (sigma' : Store) (iz' : Iz) (z' : Zaman),
      KRun V own F ⟨sigma, .seq (gom b) kalan, iz, z⟩ ⟨sigma', kalan, iz', z'⟩
      ∧ StoreUyum V s' sigma'
      ∧ izGozlem iz' = gomGozIz F.tid t ++ izGozlem iz := by
  obtain ⟨w, sg, izb, zb, hr, hu', hg, _⟩ := gomme_sim h_run h_gom h_kap h_ys sigma iz z hu
  refine ⟨sg, izb, zb + 1, ?_, hu', ?_⟩
  · exact krun_trans (krun_seq (V := V) (own := own) (F := F) kalan hr)
      (KRun.adim _ ⟨sg, kalan, izb, zb + 1⟩ _
        (adim_seq_atla V own F sg w kalan izb zb) (KRun.refl _))
  · exact hg

/-- **CORE-SERPISTIRME ADIMI:** thread listesi `ts1 ++ ⟨i, seq (gom b) kalan,
    []⟩ :: ts2` iken, i. thread'in b blogu Core'da kosar ve listede yalniz
    O THREAD'in ifadesi `kalan`a doner — DIGER THREAD'LER AYNEN KALIR.
    "Korunum" tam olarak budur: `ts1`/`ts2` adim boyunca DEGISMEZ, cunku
    `K`nin cercevesi sabittir; ve sonuc yine `KN` formundadir, yani
    SIRADAKI zamanlama adimi BASKA bir thread'e odaklanabilir. -/
theorem serpistirme_adimi {V : List CT.Ad} {own : CT.Ad → ThreadId}
    {s s' : CT.Store} {b : CT.Ifade} {t : CT.Iz} {v : Int}
    (ts1 ts2 : List ThreadCtx) (i : ThreadId) (kalan : Ifade)
    (h_run : CT.Calis s b s' t v)
    (h_gom : GomOk b) (h_kap : Kapsar V b) (h_ys : YazmaSahibi own i b)
    (sigma : Store) (iz : Iz) (z : Zaman) (hu : StoreUyum V s sigma) :
    ∃ (sigma' : Store) (iz' : Iz) (z' : Zaman),
      StepStar (KN V own (ts1 ++ ⟨i, .seq (gom b) kalan, []⟩ :: ts2) sigma iz z)
               (KN V own (ts1 ++ ⟨i, kalan, []⟩ :: ts2) sigma' iz' z')
      ∧ StoreUyum V s' sigma'
      ∧ izGozlem iz' = gomGozIz i t ++ izGozlem iz := by
  obtain ⟨sigma', iz', z', hr, hu', hg⟩ :=
    blok_kos (V := V) (own := own) (F := ⟨ts1, ts2, i⟩) h_run h_gom h_kap h_ys
      kalan sigma iz z hu
  exact ⟨sigma', iz', z', krun_stepStar hr, hu', hg⟩

/-- **ODAK DEGISIMI KORUNUMU (asil lemma):** iki ardisik serpistirme adimi
    FARKLI thread'lere odaklanabilir ve Core'da BIRLESIR. Yani zamanlama
    `[i, j]` gercek bir `StepStar`a karsilik gelir.

    Neden bu bir KORUNUM lemmasi: birinci adim `ts1_i/ts2_i` cercevesiyle,
    ikincisi `ts1_j/ts2_j` ile kosar; ARADAKI konfigurasyon her iki
    bolunmede de AYNI `KN` oldugu icin (odak_kur) `stepStar_trans`
    uygulanabilir. `ts1`/`ts2`nin adim boyunca degismemesi (cerceve
    sabitligi) bunun on kosuludur. -/
theorem odak_degisimi_birlesir {V : List CT.Ad} {own : CT.Ad → ThreadId}
    {s sa s' : CT.Store} {b1 b2 : CT.Ifade} {t1 t2 : CT.Iz} {v1 v2 : Int}
    (ts1 ts2 : List ThreadCtx) (i : ThreadId) (kalan : Ifade)
    (us1 us2 : List ThreadCtx) (j : ThreadId) (kalan2 : Ifade)
    (h_run1 : CT.Calis s b1 sa t1 v1) (h_run2 : CT.Calis sa b2 s' t2 v2)
    (h_g1 : GomOk b1) (h_k1 : Kapsar V b1) (h_y1 : YazmaSahibi own i b1)
    (h_g2 : GomOk b2) (h_k2 : Kapsar V b2) (h_y2 : YazmaSahibi own j b2)
    (sigma : Store) (iz : Iz) (z : Zaman) (hu : StoreUyum V s sigma)
    -- ARA KONFIGURASYON KOSULU: birinci adimin CIKTI thread listesi,
    -- ikinci adimin GIRDI thread listesiyle ayni (odak degisimi burada).
    (h_ara : ts1 ++ ⟨i, kalan, []⟩ :: ts2
             = us1 ++ ⟨j, .seq (gom b2) kalan2, []⟩ :: us2) :
    ∃ (sigma' : Store) (iz' : Iz) (z' : Zaman),
      StepStar (KN V own (ts1 ++ ⟨i, .seq (gom b1) kalan, []⟩ :: ts2) sigma iz z)
               (KN V own (us1 ++ ⟨j, kalan2, []⟩ :: us2) sigma' iz' z')
      ∧ StoreUyum V s' sigma'
      ∧ izGozlem iz' = gomGozIz j t2 ++ gomGozIz i t1 ++ izGozlem iz := by
  obtain ⟨sg1, iz1, z1, hr1, hu1, hg1⟩ :=
    serpistirme_adimi (V := V) (own := own) ts1 ts2 i kalan h_run1 h_g1 h_k1 h_y1
      sigma iz z hu
  rw [h_ara] at hr1
  obtain ⟨sg2, iz2, z2, hr2, hu2, hg2⟩ :=
    serpistirme_adimi (V := V) (own := own) us1 us2 j kalan2 h_run2 h_g2 h_k2 h_y2
      sg1 iz1 z1 hu1
  refine ⟨sg2, iz2, z2, stepStar_trans _ _ _ hr1 hr2, hu2, ?_⟩
  rw [hg2, hg1, List.append_assoc]

/-- **VAKUM DENETIMI:** odak GERCEKTEN degisebiliyor — iki thread'li bir
    listede once 1. thread, sonra 0. thread kosar (zamanlama `[1,0]`).
    Ara konfigurasyon kosulu (`h_ara`) burada gercek bir yeniden-bolunmedir:
    `[] ++ ⟨1,..⟩ :: [t0ctx]`  =  `[t1ctx'] ++ ⟨0,..⟩ :: []` DEGIL —
    listeler farkli sirada bolunuyor, ayni liste iki farkli cerceveye
    ayrisiyor. -/
theorem odak_degisimi_bos_degil (V : List CT.Ad) (own : CT.Ad → ThreadId)
    (e0 e1 : Ifade) :
    ∃ (ts1 ts2 us1 us2 : List ThreadCtx) (i j : ThreadId) (k1 k2 : Ifade),
      i ≠ j
      ∧ ts1 ++ ⟨i, k1, []⟩ :: ts2 = us1 ++ ⟨j, k2, []⟩ :: us2
      ∧ ts1 = [] ∧ us2 = [] := by
  refine ⟨[], [⟨0, e0, []⟩], [⟨1, .sabit .birim, []⟩], [], 1, 0,
          .sabit .birim, e0, ?_, ?_, rfl, rfl⟩
  · decide
  · rfl

end Kemgu.SideChannel.CTKopru
