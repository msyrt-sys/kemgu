/-
KEMGU DRF Mekanize — ConfigTyped Iskelet (Plan v2 Adim 2)
Kaynak (kagit formel): belgeler/KEMGU_Mekanize_Onarim_Plan.md §5
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz, sorry/axiom YOK

Onarim v3 F1 NOT: Bu dosya artik yalniz state-tipi YAPI TASLARINI icerir
(TipOrtam/BolgeOrtam + DegerTipli + SigmaTipli + SahiplikTutarli +
KanalTutarli). Eski ThreadTipli/KonfTipli placeholder'lari (True) SILINDI —
gercek formlar Kemgu/Sem/Tipli.lean'de (ThreadTipliFull + KonfTipliFull).

Onkosul: Adim 1.1 (Konfigurasyon.fault), Adim 1.2-1.3 (Step Tamam/Hata dual).
-/

import Kemgu.Sem.Core

namespace Kemgu.Sem.StateTipli
open Kemgu.Sem.Core

-- ============================================================
-- §1. Tip ortami (Gamma) ve Bolge ortami (Pho)
-- Plan v2 §3.2: HasType Γ e τ + §3.4: RegionOK Γ Ρ e Ρ' imzalarinda.
-- ============================================================

/-- Tip ortami Γ : VarId → Tip (assoc list temsili).
    Plan v2 §3.2'de HasType imzasinda kullanilir. -/
abbrev TipOrtam := List (VarId × Tip)

/-- Bolge ortami Ρ : VarId → Bolge (assoc list temsili).
    Plan v2 §3.4'te RegionOK imzasinda kullanilir. -/
abbrev BolgeOrtam := List (VarId × Bolge)

/-- Kanal tipi ortami Δ : KanalId → Tip (F3 — t_kanal_al vakumunun
    kapanisi: alinan degerin tipi kanala baglanir; total fonksiyon). -/
abbrev KanalOrtam := KanalId → Tip

/-- TipOrtam lookup: ilk eslesen entry'i dondurur (newest-wins). -/
def tipOrtamGet : TipOrtam → VarId → Option Tip
  | [], _ => none
  | (k, v) :: rest, key => if k = key then some v else tipOrtamGet rest key

/-- Lookup uyeligi (tip ortami). -/
theorem tipOrtamGet_mem : ∀ (Γ : TipOrtam) (x : VarId) (τ : Tip),
    tipOrtamGet Γ x = some τ → (x, τ) ∈ Γ
  | [], _, _, h => by simp [tipOrtamGet] at h
  | (k, v) :: rest, x, τ, h => by
      by_cases hk : k = x
      · subst hk
        simp [tipOrtamGet] at h
        subst h
        exact List.Mem.head _
      · simp [tipOrtamGet, hk] at h
        exact List.Mem.tail _ (tipOrtamGet_mem rest x τ h)

/-- BolgeOrtam lookup. -/
def bolgeOrtamGet : BolgeOrtam → VarId → Option Bolge
  | [], _ => none
  | (k, v) :: rest, key => if k = key then some v else bolgeOrtamGet rest key

-- F2 NOT: asagidaki dort yardimci RegionTamam'dan buraya tasindi —
-- Step kurallari (SmallStep) da runtime S.bolge guncellemeleri icin kullanir.

/-- BolgeOrtam Ρ update: prepend (newest-wins). -/
def bolgeOrtamUpdate (Ρ : BolgeOrtam) (x : VarId) (b : Bolge) : BolgeOrtam :=
  (x, b) :: Ρ

/-- Bolge kategori degistirme: id korur, kategori degisir. -/
def bolgeKategoriDegistir (b : Bolge) (yeni : BolgeKategorisi) : Bolge :=
  { b with kategori := yeni }

/-- R-GOREV destekleyici: yd'deki her v'nin Bolge atamasi kategori =
    sahip(t) olur (foldl + prepend, newest-wins shadow). -/
def bolgeOrtamSahipAta (Ρ : BolgeOrtam) (yd : List VarId) (t : ThreadId)
    : BolgeOrtam :=
  yd.foldl
    (fun acc v =>
      match bolgeOrtamGet Ρ v with
      | some b => (v, bolgeKategoriDegistir b (BolgeKategorisi.sahip t)) :: acc
      | none   => acc)
    Ρ

/-- R-PAYLAS destekleyici: b'yi (id eslesmesiyle) iceren tum entry'ler
    kategori = donmus olur. -/
def bolgeOrtamDondurBolge (Ρ : BolgeOrtam) (b : Bolge) : BolgeOrtam :=
  Ρ.map (fun
    | (x, br) =>
      if br.id = b.id
        then (x, bolgeKategoriDegistir br BolgeKategorisi.donmus)
        else (x, br))

/-- Yakalama listesinin bolgeleri (cGorevBaslatTamam transfer kumesi). -/
def bolgeleriTopla (Ρ : BolgeOrtam) (yd : List VarId) : List Bolge :=
  yd.filterMap (bolgeOrtamGet Ρ)

/-- Yazilabilir bolge kategorileri (F4-ispat — onayli invariant tasarimi):
    sahip(t) / kanalRho(k) / donmus kategorileri TRANSFER-DISI — bu
    kategorilerdeki bolgelere yazma/dondurma/devretme statik olarak yasak
    (r_atama / r_dondur / r_kanal_gonder / r_gorev_baslat premise'leri).
    Boylece "thread, yazilabilir-kategorili hedef bolgelerinin guncel
    sahibidir" invarianti adim altinda korunabilir: sahiplik degistiren
    her kural hedef kategoriyi ES-ZAMANLI transfer-disi yapar. -/
def kategoriYazilabilir : BolgeKategorisi → Bool
  | BolgeKategorisi.donmus     => false
  | BolgeKategorisi.sahip _    => false
  | BolgeKategorisi.kanalRho _ => false
  | _                          => true


-- ============================================================
-- §2. DegerTipli (ValueTyped) — Plan v2 §5.2.1
-- Her runtime degerin tip uyumu (kagit ifadesi: v : τ).
-- ============================================================

mutual

/-- Bir runtime degerin tip uyumu.

    Constructor'larin Deger karsiliklari (Core.lean §4):
    - skaler n         → Tip.scalar
    - metinDeg b uz    → Tip.metin
    - yapiVal b alanlar → Tip.yapi name (alanlar recursively typed)
    - diziVal b uz     → Tip.dizi τ (eleman tipi T)
    - closureVal kodId yakalama → Tip.islev args ret
    - yetkiTok id kaynak → Tip.yetki kaynak
    - birim            → Tip.bos

    V1 sinir: skaler tipleri (tam8/dtam8/...) henuz ayrilmis degil
    (Tip.scalar tek case); detay tip cikarsama Adim 3+ HasType'a baglanir.
    Closure tipi args/ret kontrolu Adim 5 LinearOK'ta dolacak. -/
inductive DegerTipli (Γ : TipOrtam) (Ρ : BolgeOrtam) : Deger → Tip → Prop where
  | dt_skaler  (n : Int) :
                DegerTipli Γ Ρ (Deger.skaler n) Tip.scalar
  | dt_metin   (b : Bolge) (uz : Nat) :
                DegerTipli Γ Ρ (Deger.metinDeg b uz) Tip.metin
  | dt_yapi    (b : Bolge) (alanlar : List Deger) (name : String) :
                DegerTipliAlanlar Γ Ρ alanlar →
                DegerTipli Γ Ρ (Deger.yapiVal b alanlar) (Tip.yapi name)
  | dt_dizi    (b : Bolge) (uz : Nat) (τ : Tip) :
                DegerTipli Γ Ρ (Deger.diziVal b uz) (Tip.dizi τ)
  | dt_closure (kodId : DugumId) (yakalama : List VarId)
               (args : List Tip) (ret : Tip) :
                DegerTipli Γ Ρ (Deger.closureVal kodId yakalama)
                              (Tip.islev args ret)
  | dt_yetki   (id : Nat) (kaynak : String) :
                DegerTipli Γ Ρ (Deger.yetkiTok id kaynak) (Tip.yetki kaynak)
  | dt_gorev   (t : ThreadId) (τ : Tip) :
                DegerTipli Γ Ρ (Deger.gorevVal t) (Tip.gorev τ)
  | dt_birim   :
                DegerTipli Γ Ρ Deger.birim Tip.bos

/-- yapiVal alanlarinin recursively typed olmasi (mutual ile DegerTipli).
    Liste uzerinde "her elemanin tip uyumlu bir τ vardir" predicate'i.
    Adim 3'te yapi alan tipleri bilinen olduğunda bu lemma alan tiplerine
    eslenmis (zip List Tip ile) versiyona iyilestirilebilir. -/
inductive DegerTipliAlanlar (Γ : TipOrtam) (Ρ : BolgeOrtam) : List Deger → Prop where
  | dta_nil  : DegerTipliAlanlar Γ Ρ []
  | dta_cons (v : Deger) (vs : List Deger) (τ : Tip) :
              DegerTipli Γ Ρ v τ →
              DegerTipliAlanlar Γ Ρ vs →
              DegerTipliAlanlar Γ Ρ (v :: vs)

end


-- ============================================================
-- §2.5. DegerTipli cevre-bagimsizligi (F4-ispat)
-- DegerTipli'nin hicbir kurali Γ/Ρ parametrelerini TUKETMEZ (fantom) —
-- istenen cevre ciftine tasinabilir. (t_sabit'in [] cevre formu ile
-- SigmaTipli/KanalTutarli'nin Ρ'li formu arasinda gecis taniki.)
-- ============================================================

mutual

theorem degerTipli_ortam {Γ Γ' : TipOrtam} {Ρ Ρ' : BolgeOrtam} :
    ∀ (v : Deger) (τ : Tip), DegerTipli Γ Ρ v τ → DegerTipli Γ' Ρ' v τ
  | .skaler _, _, h => by
      cases h; exact DegerTipli.dt_skaler _
  | .metinDeg _ _, _, h => by
      cases h; exact DegerTipli.dt_metin _ _
  | .yapiVal b alanlar, _, h => by
      cases h with
      | dt_yapi _ _ name h_al =>
          exact DegerTipli.dt_yapi b alanlar name
            (degerTipliAlanlar_ortam alanlar h_al)
  | .diziVal _ _, _, h => by
      cases h; exact DegerTipli.dt_dizi _ _ _
  | .closureVal _ _, _, h => by
      cases h; exact DegerTipli.dt_closure _ _ _ _
  | .yetkiTok _ _, _, h => by
      cases h; exact DegerTipli.dt_yetki _ _
  | .gorevVal _, _, h => by
      cases h; exact DegerTipli.dt_gorev _ _
  | .birim, _, h => by
      cases h; exact DegerTipli.dt_birim

theorem degerTipliAlanlar_ortam {Γ Γ' : TipOrtam} {Ρ Ρ' : BolgeOrtam} :
    ∀ (vs : List Deger), DegerTipliAlanlar Γ Ρ vs → DegerTipliAlanlar Γ' Ρ' vs
  | [], _ => DegerTipliAlanlar.dta_nil
  | v :: vs, h => by
      cases h with
      | dta_cons _ _ τ h_v h_vs =>
          exact DegerTipliAlanlar.dta_cons v vs τ
            (degerTipli_ortam v τ h_v)
            (degerTipliAlanlar_ortam vs h_vs)

end

-- ============================================================
-- §3. SigmaTipli (StoreTyped) — Plan v2 §5.2.2
-- Store'daki her (k, v) icin: deger tip uyumlu + konum bilinen bolgede.
-- ============================================================

/-- Store sigma'nin tip-uyumu predicate'i.

    Iki kosul:
    (1) Her (k, v) entry'si icin bazi τ tipinde DegerTipli v τ.
    (2) k.bolge BolgeOrtam'da kayitli (yani bilinen bolge).

    Ikinci sart, store'da rastgele bolge referanslarinin olmamasini garanti
    eder — sahiplikGet ile uyum saglar (her bolge bir VarId araciligiyla
    BolgeOrtam'da kayitli olmali). -/
def SigmaTipli (Γ : TipOrtam) (Ρ : BolgeOrtam) (store : Store) : Prop :=
  ∀ (k : Konum) (v : Deger), (k, v) ∈ store →
    ∃ τ : Tip,
      DegerTipli Γ Ρ v τ ∧
      ∃ x, bolgeOrtamGet Ρ x = some k.bolge


-- ============================================================
-- §4. SahiplikTutarli (SahiplikConsistent) — Plan v2 §5.2.4
-- Sahiplik haritasinin bilinen bolgelere refer ettigi + frozen persistence.
-- ============================================================

/-- Sahiplik haritasi tutarliligi (F2 guncel-durum modeli):
    thread'in sahiplendigi her bolge BolgeOrtam'da kayitli — un-owned
    bolgelere sahiplik atamasi yasak.

    Eski (2) "frozen persistence" bileseni KALDIRILDI: guncel-durum
    modelinde persistence kural-tasarimindan gelir (transfer kurallari
    guncel sahibi thread/kanal olan bolgelerle sinirli → donmus entry
    override edilmez; L4 isFrozen_persistent teoremi). -/
def SahiplikTutarli (Ρ : BolgeOrtam) (sahiplik : Sahiplik) : Prop :=
  ∀ (b : Bolge) (t : ThreadId),
    sahiplikGet sahiplik b = some (Sahip.thread t) →
    ∃ x, bolgeOrtamGet Ρ x = some b


-- ============================================================
-- §5. KanalTutarli (KanalConsistent) — Plan v2 §5.2.5
-- Kanal kuyrugundaki her degerin tip-uyumlu olmasi.
-- ============================================================

/-- Kanal durumlari tutarliligi (F3 — Δ'li KESIN form):
    her kuyruktaki deger, kanalin eleman tipi Δ(kid)'de DegerTipli.
    (Eski ∃τ formu her deger icin saglanabiliyordu — vakumdu; kesin tip
    cKanalAlTamam preservation'inin temeli.) -/
def KanalTutarli (Γ : TipOrtam) (Δ : KanalOrtam) (Ρ : BolgeOrtam)
                 (kanal : List KanalDurumu) : Prop :=
  ∀ kd ∈ kanal,
    ∀ v ∈ kd.gonderKuyrugu,
      DegerTipli Γ Ρ v (Δ kd.kid)


-- ============================================================
-- §6. NOT (Onarim v3 F1): ThreadTipli/KonfTipli placeholder'lari SILINDI
-- ============================================================
-- Eski §6 `ThreadTipli := True` ve §7 `KonfTipli` (placeholder bileşenli)
-- ADIM 0 raporu Sorun 1 envanterindeydi. Gercek formlar:
--   Kemgu/Sem/Tipli.lean → ThreadTipliFull + KonfTipliFull
-- Import dongusu kok nedeni F1 katmanlamasiyla kalkti (judgment'lar ile
-- birlesim/meta katmanlari ayrildi) — placeholder'a gerek kalmadi.

end Kemgu.Sem.StateTipli
