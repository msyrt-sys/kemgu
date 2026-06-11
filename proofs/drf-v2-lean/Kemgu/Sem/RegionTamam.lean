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

Onarim v3 F1 NOT: Bu dosya artik YALNIZ judgment katmanidir (12 kural +
helper'lar). Typed + ThreadTipliFull + KonfTipliFull → Kemgu/Sem/Tipli.lean'e,
progress_region/preservation_region → Kemgu/Meta/ProgressKorunum.lean'e
TASINDI. Eski import-cycle workaround'u (placeholder ThreadTipli'nin
StateTipli'de kalmasi) F1 katmanlamasiyla kokten cozuldu.

Onkosul: Adim 2 (StateTipli), Adim 3 (HasType), Adim 5 (LineerTamam).
-/

import Kemgu.Sem.Core
import Kemgu.Sem.StateTipli
import Kemgu.Sem.HasType
import Kemgu.Sem.LineerTamam

namespace Kemgu.Sem.RegionTamam
open Kemgu.Sem.Core Kemgu.Sem.StateTipli
     Kemgu.Sem.HasType Kemgu.Sem.LineerTamam

-- ============================================================
-- §1. BolgeOrtam helper'lari — F2'de StateTipli'ye TASINDI
-- (bolgeOrtamUpdate / bolgeKategoriDegistir / bolgeOrtamSahipAta /
--  bolgeOrtamDondurBolge / bolgeleriTopla artik StateTipli'de —
--  Step kurallari da runtime S.bolge guncellemeleri icin kullaniyor.)
-- ============================================================


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
              kategoriYazilabilir b.kategori = true →
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
  | r_gorev_baslat (Γ : TipOrtam) (Ρ Ρ' Ρkod : BolgeOrtam)
                   (yd : List VarId) (kod : Ifade) (tYeni : ThreadId) :
                     (∀ v ∈ yd, ∀ b : Bolge, bolgeOrtamGet Ρ v = some b →
                        kategoriYazilabilir b.kategori = true) →
                     -- YOL-B V1 DARALTMASI (Mehmet karari — DECISIONS_LOG
                     -- Catal 1): gorev govdesi YAZMA-HEDEFSIZDIR. Gerekce:
                     -- yakalanan bolgeler sahip(tYeni) kategorisine gecer
                     -- (yazilamaz); hedefli govdenin spawn-sonrasi ortamda
                     -- tiplenmesi kategori disipliniyle celisir —
                     -- id-anahtarlama bunu COZMEZ (erisim degil kategori
                     -- sorunu). Hedefli govde = V2 (per-thread Ρ).
                     (∀ y : VarId, ¬ HedefVar kod y) →
                     (∀ b : Bolge, ¬ HedefBolge kod b) →
                     RegionTamam Γ Ρ kod Ρkod →
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
                     kategoriYazilabilir b.kategori = true →
                     Ρ' = bolgeOrtamUpdate Ρ v
                            (bolgeKategoriDegistir b (BolgeKategorisi.kanalRho k)) →
                     RegionTamam Γ Ρ (Ifade.kanalGonderIf k v) Ρ'

  /-- R-KANAL-AL: alim semantigi alici tarafini etkiler — V1 sinir,
      Ρ degismez (alinan deger receiver scope'unda yeni bind). -/
  | r_kanal_al (Γ : TipOrtam) (Ρ : BolgeOrtam) (k : KanalId) :
                 RegionTamam Γ Ρ (Ifade.kanalAlIf k) Ρ

  /-- R-DONDUR (Plan §3.4): hedef bolge b'yi iceren tum entry'ler
      kategori = donmus olur — frozen marker.
          Ρ' = Ρ.update b ρ_donmus
      Adim 8 V2 strengthen: b kayitli (bolgeOrtamGet Ρ x = some b) ve ZATEN
      donmus DEGIL (b.kategori ≠ donmus). r_atama frozen-yazma yasagi ile
      simetrik (cifte-dondur yasagi). typing_excludes_cDondurHataZatenDonmus
      bu sartlari + KonfTipliFull kopru ile kullanir. -/
  | r_dondur (Γ : TipOrtam) (Ρ Ρ' : BolgeOrtam) (b : Bolge) (x : VarId) :
               bolgeOrtamGet Ρ x = some b →
               kategoriYazilabilir b.kategori = true →
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
-- §3. NOT (Onarim v3 F1): birlesim + meta katmanlari TASINDI
-- ============================================================
-- Typed + ThreadTipliFull + KonfTipliFull (+intro/elim, bolgeOrtamBos)
--   → Kemgu/Sem/Tipli.lean
-- progress_region + preservation_region
--   → Kemgu/Meta/ProgressKorunum.lean (Typed-formda dedup:
--     progress_typed / preservation_typed)

end Kemgu.Sem.RegionTamam
