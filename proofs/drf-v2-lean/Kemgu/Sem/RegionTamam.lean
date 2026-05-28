/-
KEMGU DRF Mekanize — RegionTamam Katmani (Plan v2 Adim 6)
Kaynak: belgeler/KEMGU_Mekanize_Onarim_Plan.md §3.4 RegionOK + §7.2 Adim 6
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz

Adim 6: Plan §3 katmanli typing judgment'in UCUNCU KATMANI:
  RegionTamam : TipOrtam → BolgeOrtam → Ifade → BolgeOrtam → Prop

Bu katman bolge gecislerini izler:
- Atama (R-ATAMA): hedef bolge donmus olmamali (frozen yazma yasak)
- gorev_baslat (R-GOREV): yakalama listesi bolgeleri sahip(t)'ye gecer
- kanal_gonder (R-KANAL): v'nin bolgesi kanalRho(k)'ya gecer (transit)
- dondur (R-PAYLAS): hedef bolge donmus'a gecer

Katman ayrimi (Plan §3.1 felsefesi):
- HasType (Adim 3): salt tip uyumu (lineerlik/bolge YOK)
- LineerTamam (Adim 5): lineer durum gecisi (Λ → Λ')
- RegionTamam (Adim 6): bolge durum gecisi (Ρ → Ρ')

Uc katmanin birlesimi `Typed` structure ile yapilir. Bu yapi ileri seviye
ThreadTipliFull + KonfTipliFull predicate'lerinde kullanilir.

ONEMLI TASARIM NOTU (import cycle):
  Brifing "StateTipli.ThreadTipli'yi gercek Typed ile doldur" diyordu,
  ancak StateTipli (DegerTipli/SigmaTipli icin temel) → HasType → LineerTamam
  → RegionTamam zincirinde Typed bu dosyada tanimli. StateTipli'nin Typed'i
  import etmesi cyclic dependency yaratir.
  COZUM: ThreadTipliFull + KonfTipliFull burada (yeni isimler) tanimli.
  StateTipli'deki `ThreadTipli` placeholder True KALIR (geriye uyumlu —
  mevcut Adim 4 preservation_konfTipli imzalari etkilenmez). Adim 7
  Discharge bu yeni ThreadTipliFull/KonfTipliFull isimlerini kullanir.

Onkosul: Adim 1.1-1.3 (Step dual), Adim 2 (StateTipli), Adim 3 (HasType),
         Adim 5 (LineerTamam).
-/

import Kemgu.Sem.Core
import Kemgu.Sem.SmallStep
import Kemgu.Sem.StateTipli
import Kemgu.Sem.HasType
import Kemgu.Sem.ProgressKorunum
import Kemgu.Sem.LineerTamam

namespace Kemgu.Sem.RegionTamam
open Kemgu.Sem.Core Kemgu.Sem.SmallStep Kemgu.Sem.StateTipli
     Kemgu.Sem.HasType Kemgu.Sem.ProgressKorunum Kemgu.Sem.LineerTamam

-- ============================================================
-- §1. BolgeOrtam helper'lari (Ρ lookup zaten StateTipli'de;
--     update + sahip atama + dondur burada)
-- ============================================================

/-- BolgeOrtam Ρ update: bir VarId'nin Bolge atamasini degistir.
    Implementasyon: prepend (newest-wins) — sahiplikSet/lineerOrtamUpdate
    ile ayni desen (Core.lean §7.1, LineerTamam §2). -/
def bolgeOrtamUpdate (Ρ : BolgeOrtam) (x : VarId) (b : Bolge) : BolgeOrtam :=
  (x, b) :: Ρ

/-- Bolge kategori degistirme yardimi: id korur, kategori degisir.
    R-GOREV (sahip), R-KANAL (kanalRho), R-PAYLAS (donmus) gecislerinde
    kullanilir. -/
def bolgeKategoriDegistir (b : Bolge) (yeni : BolgeKategorisi) : Bolge :=
  { b with kategori := yeni }

/-- R-GOREV destekleyici: yakalama listesi yd icin her v VarId'nin
    Bolge atamasini kategori = sahip(t) olarak guncelle.
    Plan v2 §3.4 r_gorev_baslat:
        Ρ' = Ρ ∪ {bolge(v) ↦ ρ_sahip(tYeni) : v ∈ yd}

    V1 implementasyonu: foldl ile her v icin orijinal Ρ'dan lookup,
    sonuca prepend (newest-wins'ten dolayi acc'taki yeni entry eski
    Ρ entry'sini shadow eder). -/
def bolgeOrtamSahipAta (Ρ : BolgeOrtam) (yd : List VarId) (t : ThreadId)
    : BolgeOrtam :=
  yd.foldl
    (fun acc v =>
      match bolgeOrtamGet Ρ v with
      | some b => (v, bolgeKategoriDegistir b (BolgeKategorisi.sahip t)) :: acc
      | none   => acc)
    Ρ

/-- R-PAYLAS destekleyici: Bolge b'yi iceren tum entry'leri kategori =
    donmus olarak guncelle (id eslesmesi uzerinden).
    Plan v2 §3.4 r_dondur: Ρ' = Ρ.update b ρ_donmus

    V1 implementasyonu: List.map ile her entry kontrol; id eslesirse
    kategori donmus'a degisir. -/
def bolgeOrtamDondurBolge (Ρ : BolgeOrtam) (b : Bolge) : BolgeOrtam :=
  Ρ.map (fun
    | (x, br) =>
      if br.id = b.id
        then (x, bolgeKategoriDegistir br BolgeKategorisi.donmus)
        else (x, br))


-- ============================================================
-- §2. RegionTamam (RegionOK) — Plan v2 §3.4 — INDUKTIF JUDGMENT
-- ============================================================

/-- RegionTamam Γ Ρ e Ρ' — Bolge durum gecisi:
    Γ : TipOrtam (tip ortami, baz alindi)
    Ρ : BolgeOrtam (giris Bolge haritasi)
    e : Ifade (incelenen ifade)
    Ρ' : BolgeOrtam (cikis Bolge haritasi, e degerlendirmesinden sonra)

    Bu judgment bolge kontrolu YAPAR — yani:
    - Atama hedef bolgesi DONMUS olmamali (R-ATAMA frozen yazma yasak)
    - gorev_baslat yakalama icindeki bolgelerin yeni sahip thread'e
      gectigi (R-GOREV transit semantigi)
    - kanal_gonder ile v'nin bolgesi kanalRho'ya gectigi (R-KANAL)
    - dondur ile hedef bolge donmus'a gectigi (R-PAYLAS)

    Plan v2 §3.4 4 ana kural + 8 ek kapsayici kural (ifade case coverage:
    sabit, tanim, seq, gorev_birlestir, kanal_al, kullan, imha, guvensiz).
    Bu 12 kural Ifade'in tum constructor'larini kapsar. -/
inductive RegionTamam : TipOrtam → BolgeOrtam → Ifade → BolgeOrtam → Prop where

  /-- R-TANIM: degisken referansi Ρ'yu etkilemez (salt okuma). -/
  | r_tanim (Γ : TipOrtam) (Ρ : BolgeOrtam) (x : VarId) :
              RegionTamam Γ Ρ (Ifade.tanim x) Ρ

  /-- R-SABIT: literal Ρ'yu etkilemez. -/
  | r_sabit (Γ : TipOrtam) (Ρ : BolgeOrtam) (v : Deger) :
              RegionTamam Γ Ρ (Ifade.sabit v) Ρ

  /-- R-ATAMA (Plan §3.4): x'in bolgesi donmus olmamali; e icin region
      tamam (Ρ → Ρ'); sonuc Ρ'.
      Bu kural FROZEN YAZMA YASAGINI tasiyici. -/
  | r_atama (Γ : TipOrtam) (Ρ Ρ' : BolgeOrtam) (x : VarId) (e : Ifade)
            (b : Bolge) :
              bolgeOrtamGet Ρ x = some b →
              b.kategori ≠ BolgeKategorisi.donmus →
              RegionTamam Γ Ρ e Ρ' →
              RegionTamam Γ Ρ (Ifade.atama x e) Ρ'

  /-- R-SEQ: a Ρ → Ρa, b Ρa → Ρb; sonuc Ρb. -/
  | r_seq (Γ : TipOrtam) (Ρ Ρa Ρb : BolgeOrtam) (a b : Ifade) :
            RegionTamam Γ Ρ a Ρa →
            RegionTamam Γ Ρa b Ρb →
            RegionTamam Γ Ρ (Ifade.seq a b) Ρb

  /-- R-GOREV-BASLAT (Plan §3.4): yakalama listesi yd'deki her v VarId'nin
      Bolge atamasi kategori = sahip(tYeni) olur.
          Ρ' = Ρ ∪ {bolge(v) ↦ ρ_sahip(tYeni) : v ∈ yd}
      V1 implementasyon `bolgeOrtamSahipAta` ile. -/
  | r_gorev_baslat (Γ : TipOrtam) (Ρ Ρ' : BolgeOrtam)
                   (yd : List VarId) (kod : Ifade) (tYeni : ThreadId) :
                     Ρ' = bolgeOrtamSahipAta Ρ yd tYeni →
                     RegionTamam Γ Ρ (Ifade.gorevBaslat yd kod) Ρ'

  /-- R-GOREV-BIRLESTIR: birlestir(g) — V1 sinir, Ρ degismez.
      V2 hedef: g'nin gorev<τ> donus bolgesi cagiranlik ile birlesir. -/
  | r_gorev_birlestir (Γ : TipOrtam) (Ρ : BolgeOrtam) (g : VarId) :
                        RegionTamam Γ Ρ (Ifade.gorevBirlestir g) Ρ

  /-- R-KANAL-GONDER (Plan §3.4): v'nin bolgesi b ise, Ρ'da bu bolge
      kategori = kanalRho(k) olur — kanal transit semantigi.
          Ρ' = Ρ.update b (ρ_kanal k) -/
  | r_kanal_gonder (Γ : TipOrtam) (Ρ Ρ' : BolgeOrtam)
                   (k : KanalId) (v : VarId) (b : Bolge) :
                     bolgeOrtamGet Ρ v = some b →
                     Ρ' = bolgeOrtamUpdate Ρ v
                            (bolgeKategoriDegistir b (BolgeKategorisi.kanalRho k)) →
                     RegionTamam Γ Ρ (Ifade.kanalGonderIf k v) Ρ'

  /-- R-KANAL-AL: alim semantigi alici tarafini etkiler — V1 sinir,
      Ρ degismez (alinan deger receiver scope'unda yeni bind). -/
  | r_kanal_al (Γ : TipOrtam) (Ρ : BolgeOrtam) (k : KanalId) :
                 RegionTamam Γ Ρ (Ifade.kanalAlIf k) Ρ

  /-- R-DONDUR (Plan §3.4): hedef bolge b'yi iceren tum entry'ler
      kategori = donmus olur — frozen marker.
          Ρ' = Ρ.update b ρ_donmus -/
  | r_dondur (Γ : TipOrtam) (Ρ Ρ' : BolgeOrtam) (b : Bolge) :
               Ρ' = bolgeOrtamDondurBolge Ρ b →
               RegionTamam Γ Ρ (Ifade.dondurIf b) Ρ'

  /-- R-KULLAN: linear consume Ρ'yu etkilemez (V1 sinir; consumed degerin
      bolgesi yerel kalir, consume sonrasi VarId tuketildi olarak isaretli
      ama Bolge entry'si kalir). -/
  | r_kullan (Γ : TipOrtam) (Ρ : BolgeOrtam) (x : VarId) :
               RegionTamam Γ Ρ (Ifade.kullanIf x) Ρ

  /-- R-IMHA: linear imha Ρ'yu etkilemez (V1 sinir; benzer kullan). -/
  | r_imha (Γ : TipOrtam) (Ρ : BolgeOrtam) (x : VarId) :
             RegionTamam Γ Ρ (Ifade.imhaIf x) Ρ

  /-- R-GUVENSIZ: ic ifade delegate. -/
  | r_guvensiz (Γ : TipOrtam) (Ρ Ρ' : BolgeOrtam) (e : Ifade) :
                 RegionTamam Γ Ρ e Ρ' →
                 RegionTamam Γ Ρ (Ifade.guvensiz e) Ρ'


-- ============================================================
-- §3. Typed full conjunction (Plan v2 §3.6)
-- HasType + LineerTamam + RegionTamam birlesimi.
-- ============================================================

/-- Typed full — uc katmanin tam birlesimi (Plan §3.6).
    Discharge lemmalari (Adim 7) Typed'i hipotez alir; her Step
    constructor (Tamam/Hata) icin Typed korunumu Preservation theorem'in
    induktif cekirdegi.

    Plan v2 §3.6 "Typed structure":
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
-- §4. ThreadTipliFull — Plan v2 §5.2.3 gercek tanim
-- StateTipli.ThreadTipli placeholder True'nun guclendirilmis form'u.
-- Ayri isim altinda (import cycle onlemek icin) — yorum §0'da.
-- ============================================================

/-- Thread'lerin tip-uyumu (Plan §5.2.3 hedef tam tanim).

    Her thread baglaminin ifadesi Typed (HasType + LineerTamam + RegionTamam)
    olmali; bazi τ tipi + bazi Λ'/Ρ' cikis ortamlari ile.

    Plan v2 §5.2.3:
    ```
    ∀ ctx ∈ threads,
      ∃ Λ_ctx Ρ_ctx τ Λ' Ρ',
        Typed Γ Λ_ctx Ρ_ctx ctx.ifade τ Λ' Ρ'
        ∧ ctx.lineer ≈ Λ_ctx
    ```

    V1 sinir: `ctx.lineer ≈ Λ_ctx` (lineer baglama uyumu) Adim 7
    Discharge'de eklenir; burada her ctx icin Typed varligi yeterli. -/
def ThreadTipliFull (Γ : TipOrtam) (Λ : LineerOrtam) (Ρ : BolgeOrtam)
                    (threads : List ThreadCtx) : Prop :=
  ∀ ctx ∈ threads,
    ∃ τ : Tip, ∃ Λ' : LineerOrtam, ∃ Ρ' : BolgeOrtam,
      Typed Γ Λ Ρ ctx.ifade τ Λ' Ρ'


-- ============================================================
-- §5. KonfTipliFull — Plan v2 §5.2 ana merkez predikat (guclendirilmis)
-- StateTipli.KonfTipli'nin ThreadTipli placeholder True yerine
-- ThreadTipliFull (gercek Typed-tabanli) ile yer degistirilmis form.
-- ============================================================

/-- Konfigurasyon tipli — Plan v2 §5'in merkezi predicate'i (Adim 6 full).

    StateTipli.KonfTipli'nin guclendirilmis hali: ThreadTipli yerine
    ThreadTipliFull ile gercek Typed-tabanli kontrol.

    Bilesenler:
    - SigmaTipli (StoreTyped): StateTipli'den (Adim 2) ✓
    - ThreadTipliFull (yukarida): Typed (HasType + LineerTamam + RegionTamam) ✓
    - SahiplikTutarli: StateTipli'den (Adim 2) ✓
    - KanalTutarli: StateTipli'den (Adim 2) ✓
    - S.fault = none: Konfigurasyon.fault default'tan (Adim 1.1) ✓

    Adim 7 Discharge bu predicate'i hipotez alir, Step Hata constructor'larini
    exfalso ile kapatir. Adim 8 L0-L7 + T1 + Drf adapt eden lemma'larda
    bu predicate kullanilir. -/
def KonfTipliFull (Γ : TipOrtam) (Λ : LineerOrtam) (Ρ : BolgeOrtam)
                  (S : Konfigurasyon) : Prop :=
  SigmaTipli Γ Ρ S.store
  ∧ ThreadTipliFull Γ Λ Ρ S.thread
  ∧ SahiplikTutarli Ρ S.sahiplik S.zaman
  ∧ KanalTutarli Γ Ρ S.kanal
  ∧ S.fault = none


-- ============================================================
-- §6. KonfTipliFull yapilandirma yardimi (yapı + cözüm)
-- ============================================================

/-- KonfTipliFull yapilandirma yardimi (5-tuple introduce). -/
theorem konfTipliFull_intro
    (Γ : TipOrtam) (Λ : LineerOrtam) (Ρ : BolgeOrtam) (S : Konfigurasyon)
    (h_store  : SigmaTipli Γ Ρ S.store)
    (h_thread : ThreadTipliFull Γ Λ Ρ S.thread)
    (h_sahip  : SahiplikTutarli Ρ S.sahiplik S.zaman)
    (h_kanal  : KanalTutarli Γ Ρ S.kanal)
    (h_fault  : S.fault = none) :
    KonfTipliFull Γ Λ Ρ S :=
  ⟨h_store, h_thread, h_sahip, h_kanal, h_fault⟩

/-- KonfTipliFull'den bilesenleri cikarma (5-tuple projection). -/
theorem konfTipliFull_elim
    (Γ : TipOrtam) (Λ : LineerOrtam) (Ρ : BolgeOrtam) (S : Konfigurasyon)
    (h : KonfTipliFull Γ Λ Ρ S) :
    SigmaTipli Γ Ρ S.store
    ∧ ThreadTipliFull Γ Λ Ρ S.thread
    ∧ SahiplikTutarli Ρ S.sahiplik S.zaman
    ∧ KanalTutarli Γ Ρ S.kanal
    ∧ S.fault = none :=
  h


-- ============================================================
-- §7. Progress + Preservation sub-lemma iskelet (Plan §7.2 Adim 6)
-- RegionTamam katmani eklenmis yeni statement'lar.
-- ============================================================

/-- Progress (RegionTamam ile) — Plan v2 §3.7 + §7.2 Adim 6.

    Adim 5'teki progress_lineer artik RegionTamam ile zenginlesti:
    "Iyi-tipli + Lineer-uyumlu + Bolge-uyumlu program ya degerdir ya da
     Step alabilir."

    V1 sinir: full proof Adim 7 Discharge sonrasi (Hata case'leri exfalso
    + No-Fault catı + RegionTamam ek bilgisi ile Step constructor insasi).
    Su an statement-only iskelet. -/
theorem progress_region
    (e : Ifade) (τ : Tip)
    (Λ' : LineerOrtam) (Ρ' : BolgeOrtam)
    (_h_typed : Typed tipOrtamBos lineerOrtamBos bolgeOrtamBos e τ Λ' Ρ')
    (S : Konfigurasyon) (ctx : ThreadCtx)
    (_h_ctx_in : ctx ∈ S.thread) (_h_ctx_ifade : ctx.ifade = e)
    (_h_no_fault : S.fault = none) :
    IsValue e ∨ ∃ S', Step S S' := by
  -- TODO: Adim 6.2'de Typed destructure (hasType + lineerOK + regionOK)
  -- ve progress_lineer (Adim 5.2) + RegionTamam ek bilgisi (frozen yazma
  -- yasagi vs.) ile case analizi. Hata case'leri Adim 7 Discharge ile
  -- exfalso (KonfTipliFull S.fault = none zorlamasi). Tamam case'leri
  -- icin Step constructor insasi RegionTamam'dan Ρ' tamamlanir.
  sorry

/-- Preservation (RegionTamam ile) — Plan v2 §3.7 + §7.2 Adim 6.

    Step S → S' altinda Typed (HasType + LineerTamam + RegionTamam)
    korunur. Bolge durumlari Step ifadeleri tarafindan tutulur:
    - sAtamaTamam: Ρ degismez (R-ATAMA store update yapar, bolge degil)
    - cGorevBaslatTamam: Ρ' = bolgeOrtamSahipAta Ρ yd tYeni (R-GOREV)
    - cKanalGonderTamam: Ρ' = bolgeOrtamUpdate Ρ v ... (R-KANAL)
    - cDondurTamam: Ρ' = bolgeOrtamDondurBolge Ρ b (R-PAYLAS)
    - diger Tamam: Ρ degismez

    V1 sinir: full proof Adim 7 Discharge sonrasi tractable. -/
theorem preservation_region
    (S S' : Konfigurasyon) (_h_step : Step S S')
    (ctx : ThreadCtx) (τ : Tip)
    (Λ' : LineerOrtam) (Ρ' : BolgeOrtam)
    (_h_in : ctx ∈ S.thread)
    (_h_typed : Typed tipOrtamBos lineerOrtamBos bolgeOrtamBos
                      ctx.ifade τ Λ' Ρ')
    (_h_no_fault_target : S'.fault = none) :
    ∃ ctx' ∈ S'.thread, ∃ Λ'_new : LineerOrtam, ∃ Ρ'_new : BolgeOrtam,
      ctx'.tid = ctx.tid ∧
      Typed tipOrtamBos lineerOrtamBos bolgeOrtamBos
            ctx'.ifade τ Λ'_new Ρ'_new := by
  -- TODO: Adim 6.3'te Step constructor case analizi:
  --   Hata (7): exfalso + h_no_fault_target vs h_fault
  --   Tamam (8): hasType korunumu (Adim 4.3 plan) + lineerTamam korunumu
  --     (Adim 5.3 plan) + regionTamam korunumu (yukarıdaki Ρ degisim
  --      patternleri). Adim 7 Discharge No-Fault catı ile.
  sorry


-- ============================================================
-- §8. Adim 6 sub-step durumu — V1 sinir + sonraki adim
-- ============================================================

/-
Adim 6 (RegionTamam katmani) — DURUM 2026-05-22:

✅ Adim 6.1 (bu commit):
   - RegionTamam inductive (12 kural) tam tanim
   - Helper'lar: bolgeOrtamUpdate, bolgeKategoriDegistir, bolgeOrtamSahipAta,
     bolgeOrtamDondurBolge
   - Typed full conjunction (HasType + LineerTamam + RegionTamam)
   - ThreadTipliFull (Plan §5.2.3 gercek tanim)
   - KonfTipliFull (Plan §5.2 guclendirilmis)
   - Sub-lemma iskelet sorry: progress_region, preservation_region

⏳ Adim 6.2 (gelecek): progress_region full proof
⏳ Adim 6.3 (gelecek): preservation_region full proof

KEMGU ile uyum (12 kural Ifade tamamladi):
| Kural             | Ifade constructor         | Ρ degisimi               |
|-------------------|---------------------------|--------------------------|
| r_tanim           | tanim x                   | yok                      |
| r_sabit           | sabit v                   | yok                      |
| r_atama (§3.4)    | atama x e                 | frozen yasagi + e'den Ρ' |
| r_seq             | seq a b                   | a → Ρa, b → Ρb           |
| r_gorev_baslat    | gorevBaslat yd kod        | sahip(tYeni) ata (R-GOR) |
| r_gorev_birlestir | gorevBirlestir g          | yok (V1 sinir)           |
| r_kanal_gonder    | kanalGonderIf k v         | kanalRho(k) ata (R-KAN)  |
| r_kanal_al        | kanalAlIf k               | yok (V1 sinir)           |
| r_dondur (§3.4)   | dondurIf b                | donmus ata (R-PAYLAS)    |
| r_kullan          | kullanIf x                | yok (V1 sinir)           |
| r_imha            | imhaIf x                  | yok (V1 sinir)           |
| r_guvensiz        | guvensiz e                | delegate                 |

Adim 7 hedef (sonraki):
- Discharge lemma ailesi (typing_excludes_*) — Hata case'lerini Typed
  hipotezi altinda exfalso ile kapat
- No-Fault catı teoremi — typed Konfigurasyon Step alirsa S'.fault = none
- L0-L7 + T1 + Drf icindeki 35 Hata case sorry'si bu Discharge'larla DUSER
- Bu adim sonu sorry beklenti: ~10 (35 Hata case'i kapanir, Adim 4-6
  iskelet sorry'leri kalır — Adim 4/5/6 progress/preservation full proof)

V1 sinir notu: progress_region + preservation_region V1'de full proof
Adim 7 Discharge + No-Fault catı sonrasi tractable. Su an statement-only
iskelet (paralel-Adim-5 deseni ile uyumlu).

ThreadTipliFull/KonfTipliFull ayri tanim (StateTipli.ThreadTipli/KonfTipli
placeholder True/iskelet KALIR) — Lean import cycle (StateTipli ↔
RegionTamam) onlemek icin ayri isim altinda. Adim 7 Discharge bu yeni
isimleri kullanir. Plan §5.2.3 hedef tam tanim formuyla uyumlu.
-/

end Kemgu.Sem.RegionTamam
