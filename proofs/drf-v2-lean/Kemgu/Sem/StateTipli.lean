/-
KEMGU DRF Mekanize — ConfigTyped Iskelet (Plan v2 Adim 2)
Kaynak (kagit formel): belgeler/KEMGU_Mekanize_Onarim_Plan.md §5
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz, sorry/axiom YOK

Adim 2 NOT (iskelet): Plan v2 §5'in 5 alt-yapisindan 4'u tam tanimli
burada (DegerTipli + SigmaTipli + SahiplikTutarli + KanalTutarli). 5.
alt-yapi (ThreadTipli) Adim 5+6 sonrasi (LinearOK + RegionOK eklenince)
Typed predicate'ine baglanip gercek tanimla doldurulacak — su an Adim
2'de bilinçli olarak placeholder True (asamali insa zorunlu;
KONFTIPLI uretim usulu degil — Plan §1.1 vacuous elestirisi farkindayim).

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

/-- TipOrtam lookup: ilk eslesen entry'i dondurur (newest-wins). -/
def tipOrtamGet : TipOrtam → VarId → Option Tip
  | [], _ => none
  | (k, v) :: rest, key => if k = key then some v else tipOrtamGet rest key

/-- BolgeOrtam lookup. -/
def bolgeOrtamGet : BolgeOrtam → VarId → Option Bolge
  | [], _ => none
  | (k, v) :: rest, key => if k = key then some v else bolgeOrtamGet rest key


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

/-- Sahiplik haritasi tutarliligi.

    Iki kosul:
    (1) Sahiplenen thread'in oldugu her (b, z) entry'sinde b bilinen
        (BolgeOrtam'da kayit) — un-owned bolgelere sahiplik atamasi yasak.
    (2) Frozen persistence: bir bolge frozen olduktan sonra her sonraki
        zaman damgasinda da frozen olmali (R-PAYLAS aksiyomu Op.Sem §5.4
        + DRF-L4 frozen region read-soundness icin gerekli).

    Adim 7 Discharge: cDondurTamam Step'i alindiginda frozen entry eklenir;
    bu predicate'in (2) kosulu typed program'da Step korunumuyla preserved. -/
def SahiplikTutarli (Ρ : BolgeOrtam) (sahiplik : Sahiplik) (zaman : Zaman) : Prop :=
  -- (1) Thread sahipligi bilinen bolgede
  (∀ (b : Bolge) (z : Zaman) (t : ThreadId), z ≤ zaman →
    sahiplikGet sahiplik (b, z) = some (Sahip.thread t) →
    ∃ x, bolgeOrtamGet Ρ x = some b)
  ∧
  -- (2) Frozen persistence
  (∀ (b : Bolge) (z₀ z : Zaman), z₀ ≤ z → z ≤ zaman →
    sahiplikGet sahiplik (b, z₀) = some Sahip.donmus →
    sahiplikGet sahiplik (b, z) = some Sahip.donmus)


-- ============================================================
-- §5. KanalTutarli (KanalConsistent) — Plan v2 §5.2.5
-- Kanal kuyrugundaki her degerin tip-uyumlu olmasi.
-- ============================================================

/-- Kanal durumlari tutarliligi.
    Her kanal entry'sinin gonderKuyrugu'undaki her deger bazi τ ile
    DegerTipli — yani kanal uzerinden gecen tum mesajlar typed. -/
def KanalTutarli (Γ : TipOrtam) (Ρ : BolgeOrtam)
                 (kanal : List KanalDurumu) : Prop :=
  ∀ kd ∈ kanal,
    ∀ v ∈ kd.gonderKuyrugu,
      ∃ τ, DegerTipli Γ Ρ v τ


-- ============================================================
-- §6. ThreadTipli (ThreadsTyped) — Plan v2 §5.2.3 — ISKELET
-- Adim 5+6 sonrasi (LinearOK + RegionOK + Typed) gercek tanimla
-- doldurulacak. Su an placeholder True.
-- ============================================================

/-- Thread'lerin tip-uyumu (Adim 2 placeholder).

    Plan v2 §5.2.3 hedef tam tanim:
    ```
    ∀ ctx ∈ threads,
      ∃ Λ_ctx Ρ_ctx τ Λ' Ρ',
        Typed Γ Λ_ctx Ρ_ctx ctx.ifade τ Λ' Ρ'
        ∧ ctx.lineer ≈ Λ_ctx
    ```

    Suanki state: `Typed` predicate'i Adim 3-6'da inductive olarak
    eklenecek (HasType + LinearOK + RegionOK birlesimi). Adim 6 sonu
    gercek tanim doldurulur. Burada placeholder `True` — BU BILINCLI
    GECICI iskelet, Plan §1.1 "vacuous predicates" elestirisi farkinda;
    UYGUNSUZ degil cunku asamali insa gerektirir (Plan §7.2). -/
def ThreadTipli (_Γ : TipOrtam) (_Λ : LineerOrtam) (_Ρ : BolgeOrtam)
                (_threads : List ThreadCtx) : Prop :=
  True


-- ============================================================
-- §7. KonfTipli (ConfigTyped) — Plan v2 §5.2 — ANA MERKEZ PREDIKAT
-- Tüm 5 alt-yapi + fault yokluğu birleşimi.
-- ============================================================

/-- Konfigurasyon tipli — Plan v2 §5'in merkezi predicate'i.

    Discharge lemmalari (Adim 7) typed program ile state-typing koprusunu
    bu predicate uzerinden kurar. Her Step constructor (Tamam veya Hata)
    icin KonfTipli korunumu Preservation theorem'in (Adim 4+) induktif
    cekirdegi.

    Mevcut Adim 2 state'i:
    - SigmaTipli (StoreTyped): tam tanim ✓
    - ThreadTipli (ThreadsTyped): iskelet `True` (Adim 6 sonu tam)
    - SahiplikTutarli (SahiplikConsistent): tam tanim ✓
    - KanalTutarli (KanalConsistent): tam tanim ✓
    - S.fault = none: tam (Adim 1.1 Konfigurasyon.fault := none default'tan)
-/
def KonfTipli (Γ : TipOrtam) (Λ : LineerOrtam) (Ρ : BolgeOrtam)
              (S : Konfigurasyon) : Prop :=
  SigmaTipli Γ Ρ S.store
  ∧ ThreadTipli Γ Λ Ρ S.thread
  ∧ SahiplikTutarli Ρ S.sahiplik S.zaman
  ∧ KanalTutarli Γ Ρ S.kanal
  ∧ S.fault = none


-- ============================================================
-- §8. Temel teknik lemmalar (iskelet)
-- Korunum lemmalari (Preservation_*) Adim 4'te Progress + Preservation
-- ile birlikte eklenir.
-- ============================================================

/-- KonfTipli yapilandirma yardimi (5-tuple introduce). -/
theorem konfTipli_intro
    (Γ : TipOrtam) (Λ : LineerOrtam) (Ρ : BolgeOrtam) (S : Konfigurasyon)
    (h_store  : SigmaTipli Γ Ρ S.store)
    (h_thread : ThreadTipli Γ Λ Ρ S.thread)
    (h_sahip  : SahiplikTutarli Ρ S.sahiplik S.zaman)
    (h_kanal  : KanalTutarli Γ Ρ S.kanal)
    (h_fault  : S.fault = none) :
    KonfTipli Γ Λ Ρ S :=
  ⟨h_store, h_thread, h_sahip, h_kanal, h_fault⟩

/-- KonfTipli'den bilesenleri cikarma (5-tuple projection). -/
theorem konfTipli_elim
    (Γ : TipOrtam) (Λ : LineerOrtam) (Ρ : BolgeOrtam) (S : Konfigurasyon)
    (h : KonfTipli Γ Λ Ρ S) :
    SigmaTipli Γ Ρ S.store
    ∧ ThreadTipli Γ Λ Ρ S.thread
    ∧ SahiplikTutarli Ρ S.sahiplik S.zaman
    ∧ KanalTutarli Γ Ρ S.kanal
    ∧ S.fault = none :=
  h


end Kemgu.Sem.StateTipli
