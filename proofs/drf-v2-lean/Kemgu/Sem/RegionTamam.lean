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

/-- `RegionNotr e` — e BOLGE-NOTR: bolge ortamini DEVRETMEZ.
    Sozdizimseldir (Ρ ve Γ'dan BAGIMSIZ), bu yuzden ortam degisimi
    altinda aynen tasinir — `r_eger`in transport kollarini kapatan sey
    budur.

    DISARIDA BIRAKILANLAR — Ρ'yu gercekten degistiren UC form:
    `gorevBaslat` (yakalananlar sahip(tYeni)'ye gecer), `kanalGonderIf`
    (bolge kanalRho'ya gecer), `dondurIf` (bolge donmus olur).
    ICERIDE: `atama` DAHIL (r_atama'nin ciktisi = govdenin ciktisi;
    yazilabilirlik yan-kosulu ciktiyi DEGISTIRMEZ) — dolayisiyla
    `eger` dallari ATAMA YAPABILIR. -/
inductive RegionNotr : Ifade → Prop where
  | rn_tanim (x : VarId) : RegionNotr (Ifade.tanim x)
  | rn_sabit (v : Deger) : RegionNotr (Ifade.sabit v)
  | rn_atama (x : VarId) (e : Ifade) :
      RegionNotr e → RegionNotr (Ifade.atama x e)
  | rn_seq (a b : Ifade) :
      RegionNotr a → RegionNotr b → RegionNotr (Ifade.seq a b)
  | rn_gorev_birlestir (g : VarId) : RegionNotr (Ifade.gorevBirlestir g)
  | rn_kanal_al (k : KanalId) : RegionNotr (Ifade.kanalAlIf k)
  | rn_kullan (x : VarId) : RegionNotr (Ifade.kullanIf x)
  | rn_imha (x : VarId) : RegionNotr (Ifade.imhaIf x)
  | rn_guvensiz (e : Ifade) :
      RegionNotr e → RegionNotr (Ifade.guvensiz e)
  | rn_eger (k d y : Ifade) :
      RegionNotr k → RegionNotr d → RegionNotr y →
      RegionNotr (Ifade.eger k d y)
  | rn_topla (a b : Ifade) :
      RegionNotr a → RegionNotr b → RegionNotr (Ifade.topla a b)
  | rn_bol (a b : Ifade) :
      RegionNotr a → RegionNotr b → RegionNotr (Ifade.bol a b)
  | rn_kalan (a b : Ifade) :
      RegionNotr a → RegionNotr b → RegionNotr (Ifade.kalan a b)
  | rn_iken (k g : Ifade) :
      RegionNotr k → RegionNotr g → RegionNotr (Ifade.iken k g)
  | rn_esles (s : Ifade) (n : Int) (d y : Ifade) :
      RegionNotr s → RegionNotr d → RegionNotr y →
      RegionNotr (Ifade.esles s n d y)
  | rn_indeks (x : VarId) (idx : Ifade) :
      RegionNotr idx → RegionNotr (Ifade.indeks x idx)
  -- D-337: indeksli yazma da bolge DEVRETMEZ (r_atama gibi: cikti govde
  -- ciktisi; yazilabilirlik yan-kosulu ciktiyi degistirmez).
  | rn_indeks_ata (x : VarId) (idx e : Ifade) :
      RegionNotr idx → RegionNotr e → RegionNotr (Ifade.indeksAta x idx e)


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
    sabit, tanim, seq, gorev_birlestir, kanal_al, kullan, imha, guvensiz)
    + D-332 r_eger. Bu kurallar Ifade'in tum constructor'larini kapsar. -/
inductive RegionTamam : TipOrtam → BolgeOrtam → Ifade → BolgeOrtam → Prop where

  /-- R-TANIM: degisken referansi Ρ'yu etkilemez (salt okuma). -/
  | r_tanim (Γ : TipOrtam) (Ρ : BolgeOrtam) (x : VarId) :
              RegionTamam Γ Ρ (Ifade.tanim x) Ρ

  /-- R-SABIT: literal Ρ'yu etkilemez. -/
  | r_sabit (Γ : TipOrtam) (Ρ : BolgeOrtam) (v : Deger) :
              RegionTamam Γ Ρ (Ifade.sabit v) Ρ

  /-- R-ATAMA (Plan §3.4): e icin region tamam (Ρ → Ρ'); x'in bolgesi
      E-SONRASI ortamda (Ρ') yazilabilir olmali; sonuc Ρ'.
      Bu kural FROZEN YAZMA YASAGINI tasiyici.

      FIX-G (Onarim v3 kapanis — degerlendirme-sirasi sadakati):
      x-kontrolu GIRIS ortami Ρ'dan CIKIS ortami Ρ''ye alindi. Yazma,
      e degerlendirildikten SONRA gerceklesir — giris-ani kontrolu
      `x := (x'i kanala gonderen e)` gibi use-after-send programlarini
      statik kabul edip runtime fault'a dusurur (counterexample-2,
      DECISIONS_LOG). Cikis-ani kontrolu yazma-aninin gercek bolge
      durumunu denetler (KEMGU R-ATAMA aksiyomunun yazma-ani semantigi). -/
  | r_atama (Γ : TipOrtam) (Ρ Ρ' : BolgeOrtam) (x : VarId) (e : Ifade)
            (b : Bolge) :
              RegionTamam Γ Ρ e Ρ' →
              bolgeOrtamGet Ρ' x = some b →
              kategoriYazilabilir b.kategori = true →
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

  /-- R-TOPLA (D-334): SIRALI kompozisyon (r_seq ile ayni) — dallanma
      yok, `RegionNotr` daraltmasi GEREKMEZ. -/
  | r_topla (Γ : TipOrtam) (Ρ Ρa Ρb : BolgeOrtam) (a b : Ifade) :
              RegionTamam Γ Ρ a Ρa →
              RegionTamam Γ Ρa b Ρb →
              RegionTamam Γ Ρ (Ifade.topla a b) Ρb

  /-- R-BOL (D-338). -/
  | r_bol (Γ : TipOrtam) (Ρ Ρa Ρb : BolgeOrtam) (a b : Ifade) :
              RegionTamam Γ Ρ a Ρa →
              RegionTamam Γ Ρa b Ρb →
              RegionTamam Γ Ρ (Ifade.bol a b) Ρb

  /-- R-KALAN (D-339). -/
  | r_kalan (Γ : TipOrtam) (Ρ Ρa Ρb : BolgeOrtam) (a b : Ifade) :
              RegionTamam Γ Ρ a Ρa →
              RegionTamam Γ Ρa b Ρb →
              RegionTamam Γ Ρ (Ifade.kalan a b) Ρb

  /-- R-EGER (D-332): kosul Ρ → Ρk; her iki dal BOLGE-NOTR'dur ve Ρk'den
      Ρk'ye gecer; sonuc Ρk.

      Iki premise ailesi de GEREKLI ve FARKLI is yapar:
      * `RegionNotr d/y` (sozdizimsel): ciktinin girisle AYNI oldugunu
        Ρ'DAN BAGIMSIZ soyler → transport kollarinda yeni ortamda da
        gecerlidir (dal-birlestirmesi icin ortam-bulusmasi GEREKMEZ).
      * `RegionTamam Γ Ρk d/y Ρk` (Ρ'ya bagli): dallardaki ATAMA'larin
        yazilabilirlik yan-kosullarini (frozen-yazma yasagi) tasir.

      Hangi dal alinirsa alinsin cikis Ρk oldugundan korunum dal
      secimine bagimsizdir. V1 daraltmasi: dallar bolge DEVRI yapamaz
      (dondur / kanal_gonder / gorev_baslat) — bunlar kosulda veya
      `eger` disinda yazilir. -/
  | r_eger (Γ : TipOrtam) (Ρ Ρk : BolgeOrtam) (k d y : Ifade) :
             RegionTamam Γ Ρ k Ρk →
             RegionNotr d → RegionNotr y →
             RegionTamam Γ Ρk d Ρk →
             RegionTamam Γ Ρk y Ρk →
             RegionTamam Γ Ρ (Ifade.eger k d y) Ρk

  /-- R-IKEN (D-335): kosul ve govde BOLGE-NOTR; dongu Ρ'yu devretmez.
      Gerekce `l_iken` ile ayni: tur sayisi statik bilinmedigi icin bir
      devir "kac kez" olacagi belirsiz olurdu. Yazma (atama) SERBEST —
      `RegionNotr` atamayi icerir. -/
  | r_iken (Γ : TipOrtam) (Ρ : BolgeOrtam) (k g : Ifade) :
             RegionNotr k → RegionNotr g →
             RegionTamam Γ Ρ k Ρ →
             RegionTamam Γ Ρ g Ρ →
             RegionTamam Γ Ρ (Ifade.iken k g) Ρ

  /-- R-ESLES (D-335): `r_eger` ile ayni sekil (kollar bolge-notr). -/
  | r_esles (Γ : TipOrtam) (Ρ Ρs : BolgeOrtam) (s : Ifade) (n : Int) (d y : Ifade) :
              RegionTamam Γ Ρ s Ρs →
              RegionNotr d → RegionNotr y →
              RegionTamam Γ Ρs d Ρs →
              RegionTamam Γ Ρs y Ρs →
              RegionTamam Γ Ρ (Ifade.esles s n d y) Ρs

  /-- R-INDEKS (D-336): okuma bolge ortamini etkilemez; gecis yalniz
      indeks ifadesinden gelir (r_atama'nin aksine yazma yok, dolayisiyla
      yazilabilirlik yan-kosulu da YOK). -/
  | r_indeks (Γ : TipOrtam) (Ρ Ρ' : BolgeOrtam) (x : VarId) (idx : Ifade) :
               RegionTamam Γ Ρ idx Ρ' →
               RegionTamam Γ Ρ (Ifade.indeks x idx) Ρ'

  /-- R-INDEKS-ATA (D-337): `r_atama` ile AYNI disiplin — FROZEN YAZMA
      YASAGININ tasiyicisi. Yazilabilirlik CIKIS ortaminda denetlenir
      (FIX-G: yazma, alt-ifadeler degerlendirildikten SONRA olur). -/
  | r_indeks_ata (Γ : TipOrtam) (Ρ Ρi Ρe : BolgeOrtam) (x : VarId)
                 (idx e : Ifade) (b : Bolge) :
                   RegionTamam Γ Ρ idx Ρi →
                   RegionTamam Γ Ρi e Ρe →
                   bolgeOrtamGet Ρe x = some b →
                   kategoriYazilabilir b.kategori = true →
                   RegionTamam Γ Ρ (Ifade.indeksAta x idx e) Ρe

  /-- R-GUVENSIZ: ic ifade delegate. -/
  | r_guvensiz (Γ : TipOrtam) (Ρ Ρ' : BolgeOrtam) (e : Ifade) :
                 RegionTamam Γ Ρ e Ρ' →
                 RegionTamam Γ Ρ (Ifade.guvensiz e) Ρ'


-- ============================================================
-- §3. Ortam-guncelleme lookup lemmalari (Onarim v3 kapanis)
-- sahipAta / update / dondurBolge'nin bolgeOrtamGet analizleri —
-- regionTamam_yaz_geri + regionTamam_transport'un mekanik temeli.
-- ============================================================

/-- update lookup: anahtar eslesirse yeni deger, degilse eski. -/
theorem bolgeOrtamUpdate_get (Ρ : BolgeOrtam) (x : VarId) (b : Bolge)
    (y : VarId) :
    bolgeOrtamGet (bolgeOrtamUpdate Ρ x b) y
      = if x = y then some b else bolgeOrtamGet Ρ y := rfl

/-- sahipAta foldl govdesi: y yd'de degilse acc-lookup korunur. -/
theorem sahipAta_foldl_notin (Ρ : BolgeOrtam) (t : ThreadId) :
    ∀ (yd : List VarId) (acc : BolgeOrtam) (y : VarId), y ∉ yd →
    bolgeOrtamGet (yd.foldl (fun acc v =>
        match bolgeOrtamGet Ρ v with
        | some b => (v, bolgeKategoriDegistir b (BolgeKategorisi.sahip t)) :: acc
        | none   => acc) acc) y = bolgeOrtamGet acc y
  | [], _, _, _ => rfl
  | v :: rest, acc, y, h => by
      have h_ne : v ≠ y := fun he => h (he ▸ List.Mem.head _)
      have h_rest : y ∉ rest := fun hm => h (List.Mem.tail _ hm)
      rw [List.foldl_cons, sahipAta_foldl_notin Ρ t rest _ y h_rest]
      cases h_lk : bolgeOrtamGet Ρ v with
      | some b =>
          show bolgeOrtamGet
              ((v, bolgeKategoriDegistir b (BolgeKategorisi.sahip t)) :: acc) y
            = bolgeOrtamGet acc y
          rw [bolgeOrtamGet, if_neg h_ne]
      | none => rfl

/-- sahipAta foldl govdesi: Ρ-kayitsiz y icin acc-lookup korunur. -/
theorem sahipAta_foldl_none (Ρ : BolgeOrtam) (t : ThreadId) :
    ∀ (yd : List VarId) (acc : BolgeOrtam) (y : VarId),
    bolgeOrtamGet Ρ y = none →
    bolgeOrtamGet (yd.foldl (fun acc v =>
        match bolgeOrtamGet Ρ v with
        | some b => (v, bolgeKategoriDegistir b (BolgeKategorisi.sahip t)) :: acc
        | none   => acc) acc) y = bolgeOrtamGet acc y
  | [], _, _, _ => rfl
  | v :: rest, acc, y, h_none => by
      rw [List.foldl_cons, sahipAta_foldl_none Ρ t rest _ y h_none]
      cases h_lk : bolgeOrtamGet Ρ v with
      | some b =>
          have h_ne : v ≠ y := by
            intro he; rw [he, h_none] at h_lk; cases h_lk
          show bolgeOrtamGet
              ((v, bolgeKategoriDegistir b (BolgeKategorisi.sahip t)) :: acc) y
            = bolgeOrtamGet acc y
          rw [bolgeOrtamGet, if_neg h_ne]
      | none => rfl

/-- sahipAta: y ∉ yd → lookup degismez. -/
theorem sahipAta_get_notin (Ρ : BolgeOrtam) (yd : List VarId) (t : ThreadId)
    (y : VarId) (h : y ∉ yd) :
    bolgeOrtamGet (bolgeOrtamSahipAta Ρ yd t) y = bolgeOrtamGet Ρ y :=
  sahipAta_foldl_notin Ρ t yd Ρ y h

/-- sahipAta: Ρ-kayitsiz y → lookup yine kayitsiz. -/
theorem sahipAta_get_none (Ρ : BolgeOrtam) (yd : List VarId) (t : ThreadId)
    (y : VarId) (h : bolgeOrtamGet Ρ y = none) :
    bolgeOrtamGet (bolgeOrtamSahipAta Ρ yd t) y = none :=
  (sahipAta_foldl_none Ρ t yd Ρ y h).trans h

/-- sahipAta: y ∈ yd ∧ kayitli → lookup = sahip-recat'li deger. -/
theorem sahipAta_get_in (Ρ : BolgeOrtam) (t : ThreadId) :
    ∀ (yd : List VarId) (acc : BolgeOrtam) (y : VarId) (b : Bolge),
    y ∈ yd → bolgeOrtamGet Ρ y = some b →
    bolgeOrtamGet (yd.foldl (fun acc v =>
        match bolgeOrtamGet Ρ v with
        | some b => (v, bolgeKategoriDegistir b (BolgeKategorisi.sahip t)) :: acc
        | none   => acc) acc) y
      = some (bolgeKategoriDegistir b (BolgeKategorisi.sahip t))
  | [], _, _, _, h, _ => absurd h (List.not_mem_nil)
  | v :: rest, acc, y, b, h_mem, h_lk => by
      by_cases h_rest : y ∈ rest
      · rw [List.foldl_cons]
        exact sahipAta_get_in Ρ t rest _ y b h_rest h_lk
      · have h_vy : v = y := by
          cases h_mem with
          | head => rfl
          | tail _ hr => exact absurd hr h_rest
        subst h_vy
        rw [List.foldl_cons, sahipAta_foldl_notin Ρ t rest _ v h_rest, h_lk]
        show bolgeOrtamGet ((v, _) :: acc) v = _
        rw [bolgeOrtamGet, if_pos rfl]

/-- sahipAta lookup tersine-analizi: sonuc ya yd-recat'inden ya eskidir. -/
theorem sahipAta_get_inv (Ρ : BolgeOrtam) (t : ThreadId) :
    ∀ (yd : List VarId) (acc : BolgeOrtam) (y : VarId) (bb : Bolge),
    bolgeOrtamGet (yd.foldl (fun acc v =>
        match bolgeOrtamGet Ρ v with
        | some b => (v, bolgeKategoriDegistir b (BolgeKategorisi.sahip t)) :: acc
        | none   => acc) acc) y = some bb →
    (∃ b0, bolgeOrtamGet Ρ y = some b0
        ∧ bb = bolgeKategoriDegistir b0 (BolgeKategorisi.sahip t) ∧ y ∈ yd)
    ∨ bolgeOrtamGet acc y = some bb
  | [], _, _, _, h => Or.inr h
  | v :: rest, acc, y, bb, h => by
      rw [List.foldl_cons] at h
      rcases sahipAta_get_inv Ρ t rest _ y bb h with
          ⟨b0, h_b0, h_bb, h_in⟩ | h_acc
      · exact Or.inl ⟨b0, h_b0, h_bb, List.Mem.tail _ h_in⟩
      · cases h_lk : bolgeOrtamGet Ρ v with
        | none => rw [h_lk] at h_acc; exact Or.inr h_acc
        | some b =>
            rw [h_lk] at h_acc
            by_cases h_vy : v = y
            · subst h_vy
              rw [bolgeOrtamGet, if_pos rfl] at h_acc
              exact Or.inl ⟨b, h_lk, (Option.some.inj h_acc).symm,
                List.Mem.head _⟩
            · rw [bolgeOrtamGet, if_neg h_vy] at h_acc
              exact Or.inr h_acc

/-- sahipAta_get_in — bolgeOrtamSahipAta formu (rw-dostu). -/
theorem sahipAta_get_in' (Ρ : BolgeOrtam) (yd : List VarId) (t : ThreadId)
    (y : VarId) (b : Bolge) (h_in : y ∈ yd)
    (h_lk : bolgeOrtamGet Ρ y = some b) :
    bolgeOrtamGet (bolgeOrtamSahipAta Ρ yd t) y
      = some (bolgeKategoriDegistir b (BolgeKategorisi.sahip t)) :=
  sahipAta_get_in Ρ t yd Ρ y b h_in h_lk

/-- sahipAta_get_inv — bolgeOrtamSahipAta formu (rcases-dostu). -/
theorem sahipAta_get_inv' (Ρ : BolgeOrtam) (yd : List VarId) (t : ThreadId)
    (y : VarId) (bb : Bolge)
    (h : bolgeOrtamGet (bolgeOrtamSahipAta Ρ yd t) y = some bb) :
    (∃ b0, bolgeOrtamGet Ρ y = some b0
        ∧ bb = bolgeKategoriDegistir b0 (BolgeKategorisi.sahip t) ∧ y ∈ yd)
    ∨ bolgeOrtamGet Ρ y = some bb :=
  sahipAta_get_inv Ρ t yd Ρ y bb h

/-- sahipAta id-koruma: kayitli degiskenin lookup'u id-esit kalir. -/
theorem sahipAta_id_koruma (Ρ : BolgeOrtam) (yd : List VarId) (t : ThreadId)
    (x : VarId) (b : Bolge) (h : bolgeOrtamGet Ρ x = some b) :
    ∃ b', bolgeOrtamGet (bolgeOrtamSahipAta Ρ yd t) x = some b'
      ∧ b'.id = b.id := by
  by_cases h_in : x ∈ yd
  · exact ⟨bolgeKategoriDegistir b (BolgeKategorisi.sahip t),
      sahipAta_get_in Ρ t yd Ρ x b h_in h, rfl⟩
  · exact ⟨b, (sahipAta_get_notin Ρ yd t x h_in).trans h, rfl⟩

/-- sahipAta giriş-mutabakati cikista korunur (transport agreement). -/
theorem sahipAta_agree (Ρ Ρn : BolgeOrtam) (yd : List VarId) (t : ThreadId)
    (y : VarId) (h : bolgeOrtamGet Ρn y = bolgeOrtamGet Ρ y) :
    bolgeOrtamGet (bolgeOrtamSahipAta Ρn yd t) y
      = bolgeOrtamGet (bolgeOrtamSahipAta Ρ yd t) y := by
  by_cases h_in : y ∈ yd
  · cases h_lk : bolgeOrtamGet Ρ y with
    | some b =>
        have h1 : bolgeOrtamGet (bolgeOrtamSahipAta Ρ yd t) y
            = some (bolgeKategoriDegistir b (BolgeKategorisi.sahip t)) :=
          sahipAta_get_in Ρ t yd Ρ y b h_in h_lk
        have h2 : bolgeOrtamGet (bolgeOrtamSahipAta Ρn yd t) y
            = some (bolgeKategoriDegistir b (BolgeKategorisi.sahip t)) :=
          sahipAta_get_in Ρn t yd Ρn y b h_in (h.trans h_lk)
        rw [h1, h2]
    | none =>
        rw [sahipAta_get_none Ρ yd t y h_lk,
            sahipAta_get_none Ρn yd t y (h.trans h_lk)]
  · rw [sahipAta_get_notin Ρ yd t y h_in,
        sahipAta_get_notin Ρn yd t y h_in, h]

/-- dondurBolge lookup: map-formu (id-eslesen girisler donmus-recat). -/
theorem dondur_get (Ρ : BolgeOrtam) (b : Bolge) (y : VarId) :
    bolgeOrtamGet (bolgeOrtamDondurBolge Ρ b) y
      = (bolgeOrtamGet Ρ y).map (fun br =>
          if br.id = b.id
            then bolgeKategoriDegistir br BolgeKategorisi.donmus
            else br) := by
  induction Ρ with
  | nil => rfl
  | cons p rest ih =>
      obtain ⟨x0, br0⟩ := p
      show bolgeOrtamGet
          ((if br0.id = b.id
              then (x0, bolgeKategoriDegistir br0 BolgeKategorisi.donmus)
              else (x0, br0)) :: bolgeOrtamDondurBolge rest b) y = _
      by_cases h0 : br0.id = b.id
      · rw [if_pos h0]
        by_cases hx : x0 = y
        · rw [bolgeOrtamGet, if_pos hx,
              bolgeOrtamGet, if_pos hx, Option.map_some, if_pos h0]
        · rw [bolgeOrtamGet, if_neg hx, bolgeOrtamGet, if_neg hx]
          exact ih
      · rw [if_neg h0]
        by_cases hx : x0 = y
        · rw [bolgeOrtamGet, if_pos hx,
              bolgeOrtamGet, if_pos hx, Option.map_some, if_neg h0]
        · rw [bolgeOrtamGet, if_neg hx, bolgeOrtamGet, if_neg hx]
          exact ih


-- ============================================================
-- §4. Yaz-geri + Transport (Onarim v3 kapanis — DECISIONS_LOG tasarimi)
-- ============================================================

/-- BOLGE-NOTR ⟹ CIKIS = GIRIS. Sozdizimsel notrluk, HANGI ortamda
    tiplenirse tiplensin cikisin girise esit oldugunu verir. `r_eger`in
    transport kollarinin motoru: yeni ortam Ρn'de dal yine Ρn'e doner. -/
theorem regionNotr_cikis_esit {Γ : TipOrtam} {Ρ Ρout : BolgeOrtam} {e : Ifade}
    (h : RegionTamam Γ Ρ e Ρout) : RegionNotr e → Ρout = Ρ := by
  induction h with
  | r_tanim _ _ => exact fun _ => rfl
  | r_sabit _ _ => exact fun _ => rfl
  | r_atama _ _ _ _ _ _ _ _ ih =>
      intro h_n; cases h_n with | rn_atama _ _ h_ne => exact ih h_ne
  | r_indeks _ _ _ _ _ ih =>
      intro h_n; cases h_n with | rn_indeks _ _ h_ni => exact ih h_ni
  | r_indeks_ata _ _ _ _ _ _ _ _ _ _ _ ih_i ih_e =>
      intro h_n
      cases h_n with
      | rn_indeks_ata _ _ _ h_ni h_ne => rw [ih_e h_ne, ih_i h_ni]
  | r_seq _ _ _ _ _ _ _ ih_a ih_b =>
      intro h_n
      cases h_n with
      | rn_seq _ _ h_na h_nb => rw [ih_b h_nb, ih_a h_na]
  | r_topla _ _ _ _ _ _ _ ih_a ih_b =>
      intro h_n
      cases h_n with
      | rn_topla _ _ h_na h_nb => rw [ih_b h_nb, ih_a h_na]
  | r_bol _ _ _ _ _ _ _ ih_a ih_b =>
      intro h_n
      cases h_n with
      | rn_bol _ _ h_na h_nb => rw [ih_b h_nb, ih_a h_na]
  | r_kalan _ _ _ _ _ _ _ ih_a ih_b =>
      intro h_n
      cases h_n with
      | rn_kalan _ _ h_na h_nb => rw [ih_b h_nb, ih_a h_na]
  -- D-335: r_iken/r_esles cikisi zaten girise esit / skrutinden gelir.
  | r_iken _ _ _ _ _ _ _ _ _ => intro _; rfl
  | r_esles _ _ _ _ _ _ _ _ _ _ _ ih_s _ _ =>
      intro h_n
      cases h_n with
      | rn_esles _ _ _ _ h_ns _ _ => exact ih_s h_ns
  | r_gorev_baslat _ _ _ _ _ _ _ _ _ _ _ _ => intro h_n; nomatch h_n
  | r_gorev_birlestir _ _ => exact fun _ => rfl
  | r_kanal_gonder _ _ _ _ _ _ _ _ => intro h_n; nomatch h_n
  | r_kanal_al _ _ => exact fun _ => rfl
  | r_dondur _ _ _ _ _ _ _ => intro h_n; nomatch h_n
  | r_kullan _ _ => exact fun _ => rfl
  | r_imha _ _ => exact fun _ => rfl
  | r_eger _ _ _ _ _ _ _ _ _ _ ih_k _ _ =>
      intro h_n; cases h_n with | rn_eger _ _ _ h_nk _ _ => exact ih_k h_nk
  | r_guvensiz _ _ _ _ ih =>
      intro h_n; cases h_n with | rn_guvensiz _ h_ne => exact ih h_ne


/-- BOLGE-NOTR ifadenin BOLGE-HEDEFI YOKTUR. `HedefBolge`nin tek taban
    kurali `dondur_bas`tir ve `dondurIf` notr degildir; dolayisiyla notr
    bir ifade hicbir bolgeyi hedeflemez. Transport kollarinda `eger`
    dallarinin HedefBolge yukumlulugunu VAKUMA dusuren lemma. -/
theorem regionNotr_hedefBolge_yok {e : Ifade} (h_n : RegionNotr e) :
    ∀ b : Bolge, ¬ HedefBolge e b := by
  induction h_n with
  | rn_tanim x => intro b h; nomatch h
  | rn_sabit v => intro b h; nomatch h
  | rn_atama x e _ ih =>
      intro b h; exact ih b (hedefBolge_atama_inv h)
  | rn_indeks x idx _ ih =>
      intro b h; cases h with | indeks_ic _ _ _ h' => exact ih b h'
  | rn_indeks_ata x idx e _ _ ih_i ih_e =>
      intro b h
      cases h with
      | indeksAta_idx _ _ _ _ h' => exact ih_i b h'
      | indeksAta_deg _ _ _ _ h' => exact ih_e b h'
  | rn_seq a c _ _ ih_a ih_c =>
      intro b h
      rcases hedefBolge_seq_inv h with h' | h'
      · exact ih_a b h'
      · exact ih_c b h'
  | rn_gorev_birlestir g => intro b h; nomatch h
  | rn_kanal_al k => intro b h; nomatch h
  | rn_kullan x => intro b h; nomatch h
  | rn_imha x => intro b h; nomatch h
  | rn_guvensiz e _ ih =>
      intro b h; exact ih b (hedefBolge_guvensiz_inv h)
  | rn_eger k d y _ _ _ ih_k ih_d ih_y =>
      intro b h
      cases h with
      | eger_kosul _ _ _ _ h' => exact ih_k b h'
      | eger_dogru _ _ _ _ h' => exact ih_d b h'
      | eger_yanlis _ _ _ _ h' => exact ih_y b h'
  | rn_topla a c _ _ ih_a ih_c =>
      intro b h
      cases h with
      | topla_sol _ _ _ h' => exact ih_a b h'
      | topla_sag _ _ _ h' => exact ih_c b h'
  | rn_bol a c _ _ ih_a ih_c =>
      intro b h
      cases h with
      | bol_sol _ _ _ h' => exact ih_a b h'
      | bol_sag _ _ _ h' => exact ih_c b h'
  | rn_kalan a c _ _ ih_a ih_c =>
      intro b h
      cases h with
      | kalan_sol _ _ _ h' => exact ih_a b h'
      | kalan_sag _ _ _ h' => exact ih_c b h'
  | rn_iken k g _ _ ih_k ih_g =>
      intro b h
      cases h with
      | iken_kosul _ _ _ h' => exact ih_k b h'
      | iken_govde _ _ _ h' => exact ih_g b h'
  | rn_esles s n d y _ _ _ ih_s ih_d ih_y =>
      intro b h
      cases h with
      | esles_skrut _ _ _ _ _ h' => exact ih_s b h'
      | esles_eslesen _ _ _ _ _ h' => exact ih_d b h'
      | esles_kalan _ _ _ _ _ h' => exact ih_y b h'


/-- YAZ-GERI: cikis ortaminda YAZILABILIR kategorili gorunen kayit,
    giriste de aynidir — tum R-guncellemeleri yazilamaz kategori
    (sahip/kanalRho/donmus) yazar. -/
theorem regionTamam_yaz_geri {Γ : TipOrtam} {Ρ Ρout : BolgeOrtam} {e : Ifade}
    (h : RegionTamam Γ Ρ e Ρout) :
    ∀ (y : VarId) (b : Bolge),
    bolgeOrtamGet Ρout y = some b →
    kategoriYazilabilir b.kategori = true →
    bolgeOrtamGet Ρ y = some b := by
  induction h with
  | r_tanim _ _ => exact fun _ _ h_o _ => h_o
  | r_sabit _ _ => exact fun _ _ h_o _ => h_o
  | r_atama _ _ _ _ _ _ _ _ ih => exact ih
  | r_indeks _ _ _ _ _ ih => exact ih
  -- D-337: iki alt-ifade sirali → r_seq gibi KOMPOZISYON.
  | r_indeks_ata _ _ _ _ _ _ _ _ _ _ _ ih_i ih_e =>
      exact fun y b h_o h_y => ih_i y b (ih_e y b h_o h_y) h_y
  | r_seq _ _ _ _ _ _ _ ih_a ih_b =>
      exact fun y b h_o h_y => ih_a y b (ih_b y b h_o h_y) h_y
  | r_topla _ _ _ _ _ _ _ ih_a ih_b =>
      exact fun y b h_o h_y => ih_a y b (ih_b y b h_o h_y) h_y
  | r_bol _ _ _ _ _ _ _ ih_a ih_b =>
      exact fun y b h_o h_y => ih_a y b (ih_b y b h_o h_y) h_y
  | r_kalan _ _ _ _ _ _ _ ih_a ih_b =>
      exact fun y b h_o h_y => ih_a y b (ih_b y b h_o h_y) h_y
  -- D-335: iken cikisi = giris; esles cikisi skrutinden gelir.
  | r_iken _ _ _ _ _ _ _ _ _ => exact fun _ _ h_o _ => h_o
  | r_esles _ _ _ _ _ _ _ _ _ _ _ ih_s _ _ => exact ih_s
  | r_gorev_baslat _ _ _ yd _ tY _ _ _ _ h_eq _ =>
      intro y b h_o h_y
      subst h_eq
      rcases sahipAta_get_inv _ tY yd _ y b h_o with
          ⟨b0, _, h_bb, _⟩ | h_acc
      · rw [h_bb] at h_y
        simp [bolgeKategoriDegistir, kategoriYazilabilir] at h_y
      · exact h_acc
  -- D-332: cikis = kosulun cikisi (dallar notr) → IH_k aynen.
  | r_eger _ _ _ _ _ _ _ _ _ _ ih_k _ _ => exact ih_k
  | r_gorev_birlestir _ _ => exact fun _ _ h_o _ => h_o
  | r_kanal_gonder _ _ k v b0 _ _ h_eq =>
      intro y b h_o h_y
      subst h_eq
      rw [bolgeOrtamUpdate_get] at h_o
      by_cases hv : v = y
      · rw [if_pos hv] at h_o
        rw [← Option.some.inj h_o] at h_y
        simp [bolgeKategoriDegistir, kategoriYazilabilir] at h_y
      · rw [if_neg hv] at h_o
        exact h_o
  | r_kanal_al _ _ => exact fun _ _ h_o _ => h_o
  | r_dondur _ _ b0 x _ _ h_eq =>
      intro y b h_o h_y
      subst h_eq
      rw [dondur_get] at h_o
      cases h_lk : bolgeOrtamGet _ y with
      | none => rw [h_lk] at h_o; cases h_o
      | some br =>
          rw [h_lk, Option.map_some] at h_o
          by_cases hid : br.id = b0.id
          · rw [if_pos hid] at h_o
            rw [← Option.some.inj h_o] at h_y
            simp [bolgeKategoriDegistir, kategoriYazilabilir] at h_y
          · rw [if_neg hid] at h_o
            exact h_o
  | r_kullan _ _ => exact fun _ _ h_o _ => h_o
  | r_imha _ _ => exact fun _ _ h_o _ => h_o
  | r_guvensiz _ _ _ _ ih => exact ih

/-- TRANSPORT (DECISIONS_LOG tasarimi — DisindaEsit-Y formu): e'nin
    HEDEF degiskenlerinde mutabik (HedefVar) ve yazilabilir HEDEF-bolge
    kayitlarini koruyan (HedefBolge) yeni ortam altinda e yine
    region-tamamdir; giriste mutabik HER anahtar cikista da mutabik
    kalir. r_gorev_baslat cocuk-govdesi Yol-B hedefsiz-premise'leriyle
    kosulsuz tasinir. -/
theorem regionTamam_transport {Γ : TipOrtam} {Ρ Ρout : BolgeOrtam} {e : Ifade}
    (h : RegionTamam Γ Ρ e Ρout) :
    ∀ (Ρn : BolgeOrtam),
    (∀ y, HedefVar e y → bolgeOrtamGet Ρn y = bolgeOrtamGet Ρ y) →
    (∀ x bb, HedefBolge e bb → kategoriYazilabilir bb.kategori = true →
       bolgeOrtamGet Ρ x = some bb → bolgeOrtamGet Ρn x = some bb) →
    ∃ Ρoutn, RegionTamam Γ Ρn e Ρoutn
      ∧ ∀ y, bolgeOrtamGet Ρn y = bolgeOrtamGet Ρ y →
          bolgeOrtamGet Ρoutn y = bolgeOrtamGet Ρout y := by
  induction h with
  | r_tanim _ x =>
      exact fun Ρn _ _ => ⟨Ρn, RegionTamam.r_tanim _ _ x, fun _ h => h⟩
  | r_sabit _ v =>
      exact fun Ρn _ _ => ⟨Ρn, RegionTamam.r_sabit _ _ v, fun _ h => h⟩
  | r_atama _ _ x e b _ h_gx h_yz ih =>
      intro Ρn h_hv h_hb
      obtain ⟨Ρe', h_re', h_agree⟩ := ih Ρn
        (fun y hy => h_hv y (HedefVar.atama_ic x e y hy))
        (fun x' bb hb hyz hlk =>
          h_hb x' bb (HedefBolge.atama_ic x e bb hb) hyz hlk)
      have h_gx' : bolgeOrtamGet Ρe' x = some b := by
        rw [h_agree x (h_hv x (HedefVar.atama_bas x e))]
        exact h_gx
      exact ⟨Ρe', RegionTamam.r_atama _ _ _ x e b h_re' h_gx' h_yz, h_agree⟩
  -- D-336: indeksli okuma — yazma yok, yan-kosul yok; sadece indeks tasinir.
  | r_indeks _ _ x e _ ih =>
      intro Ρn h_hv h_hb
      obtain ⟨Ρe', h_re', h_agree⟩ := ih Ρn
        (fun z hz => h_hv z (HedefVar.indeks_ic x e z hz))
        (fun x' bb hb hyz hlk =>
          h_hb x' bb (HedefBolge.indeks_ic x e bb hb) hyz hlk)
      exact ⟨Ρe', RegionTamam.r_indeks _ _ _ x e h_re', h_agree⟩
  -- D-337: r_seq (sirali) + r_atama (yazilabilirlik) birlesimi.
  | r_indeks_ata _ _ _ x idx e b h_ri _ h_gx h_yz ih_i ih_e =>
      intro Ρn h_hv h_hb
      obtain ⟨Ρi', h_ri', agree_i⟩ := ih_i Ρn
        (fun z hz => h_hv z (HedefVar.indeksAta_idx x idx e z hz))
        (fun x' bb hb hyz hlk =>
          h_hb x' bb (HedefBolge.indeksAta_idx x idx e bb hb) hyz hlk)
      obtain ⟨Ρe', h_re', agree_e⟩ := ih_e Ρi'
        (fun z hz => agree_i z (h_hv z (HedefVar.indeksAta_deg x idx e z hz)))
        (fun x' bb hb hyz hlk => by
          have h_geri := regionTamam_yaz_geri h_ri x' bb hlk hyz
          have h_n := h_hb x' bb (HedefBolge.indeksAta_deg x idx e bb hb) hyz h_geri
          have h_ag := agree_i x' (h_n.trans h_geri.symm)
          rw [h_ag]; exact hlk)
      have h_gx' : bolgeOrtamGet Ρe' x = some b := by
        rw [agree_e x (agree_i x (h_hv x (HedefVar.indeksAta_bas x idx e)))]
        exact h_gx
      exact ⟨Ρe', RegionTamam.r_indeks_ata _ _ _ _ x idx e b h_ri' h_re' h_gx' h_yz,
        fun y hy => agree_e y (agree_i y hy)⟩
  | r_seq _ _ _ a b h_ra _ ih_a ih_b =>
      intro Ρn h_hv h_hb
      obtain ⟨Ρa', h_ra', agree_a⟩ := ih_a Ρn
        (fun y hy => h_hv y (HedefVar.seq_sol a b y hy))
        (fun x bb hb hyz hlk =>
          h_hb x bb (HedefBolge.seq_sol a b bb hb) hyz hlk)
      obtain ⟨Ρb', h_rb', agree_b⟩ := ih_b Ρa'
        (fun y hy => agree_a y (h_hv y (HedefVar.seq_sag a b y hy)))
        (fun x bb hb hyz hlk => by
          have h_geri := regionTamam_yaz_geri h_ra x bb hlk hyz
          have h_n := h_hb x bb (HedefBolge.seq_sag a b bb hb) hyz h_geri
          have h_ag := agree_a x (h_n.trans h_geri.symm)
          rw [h_ag]; exact hlk)
      exact ⟨Ρb', RegionTamam.r_seq _ _ _ _ a b h_ra' h_rb',
        fun y hy => agree_b y (agree_a y hy)⟩
  -- D-334: r_seq ile BIREBIR ayni (sirali kompozisyon).
  | r_topla _ _ _ a b h_ra _ ih_a ih_b =>
      intro Ρn h_hv h_hb
      obtain ⟨Ρa', h_ra', agree_a⟩ := ih_a Ρn
        (fun y hy => h_hv y (HedefVar.topla_sol a b y hy))
        (fun x bb hb hyz hlk =>
          h_hb x bb (HedefBolge.topla_sol a b bb hb) hyz hlk)
      obtain ⟨Ρb', h_rb', agree_b⟩ := ih_b Ρa'
        (fun y hy => agree_a y (h_hv y (HedefVar.topla_sag a b y hy)))
        (fun x bb hb hyz hlk => by
          have h_geri := regionTamam_yaz_geri h_ra x bb hlk hyz
          have h_n := h_hb x bb (HedefBolge.topla_sag a b bb hb) hyz h_geri
          have h_ag := agree_a x (h_n.trans h_geri.symm)
          rw [h_ag]; exact hlk)
      exact ⟨Ρb', RegionTamam.r_topla _ _ _ _ a b h_ra' h_rb',
        fun y hy => agree_b y (agree_a y hy)⟩
  -- D-338: r_topla ile birebir ayni (bolme bolge ortamina dokunmaz).
  | r_bol _ _ _ a b h_ra _ ih_a ih_b =>
      intro Ρn h_hv h_hb
      obtain ⟨Ρa', h_ra', agree_a⟩ := ih_a Ρn
        (fun y hy => h_hv y (HedefVar.bol_sol a b y hy))
        (fun x bb hb hyz hlk =>
          h_hb x bb (HedefBolge.bol_sol a b bb hb) hyz hlk)
      obtain ⟨Ρb', h_rb', agree_b⟩ := ih_b Ρa'
        (fun y hy => agree_a y (h_hv y (HedefVar.bol_sag a b y hy)))
        (fun x bb hb hyz hlk => by
          have h_geri := regionTamam_yaz_geri h_ra x bb hlk hyz
          have h_n := h_hb x bb (HedefBolge.bol_sag a b bb hb) hyz h_geri
          have h_ag := agree_a x (h_n.trans h_geri.symm)
          rw [h_ag]; exact hlk)
      exact ⟨Ρb', RegionTamam.r_bol _ _ _ _ a b h_ra' h_rb',
        fun y hy => agree_b y (agree_a y hy)⟩
  -- D-339: r_topla ile birebir ayni (bolme bolge ortamina dokunmaz).
  | r_kalan _ _ _ a b h_ra _ ih_a ih_b =>
      intro Ρn h_hv h_hb
      obtain ⟨Ρa', h_ra', agree_a⟩ := ih_a Ρn
        (fun y hy => h_hv y (HedefVar.kalan_sol a b y hy))
        (fun x bb hb hyz hlk =>
          h_hb x bb (HedefBolge.kalan_sol a b bb hb) hyz hlk)
      obtain ⟨Ρb', h_rb', agree_b⟩ := ih_b Ρa'
        (fun y hy => agree_a y (h_hv y (HedefVar.kalan_sag a b y hy)))
        (fun x bb hb hyz hlk => by
          have h_geri := regionTamam_yaz_geri h_ra x bb hlk hyz
          have h_n := h_hb x bb (HedefBolge.kalan_sag a b bb hb) hyz h_geri
          have h_ag := agree_a x (h_n.trans h_geri.symm)
          rw [h_ag]; exact hlk)
      exact ⟨Ρb', RegionTamam.r_kalan _ _ _ _ a b h_ra' h_rb',
        fun y hy => agree_b y (agree_a y hy)⟩
  | r_gorev_baslat _ _ Ρkod yd kod tY h_cap h_kodhv h_kodhb _ h_eq ih =>
      intro Ρn h_hv _
      obtain ⟨Ρkod', h_kod', _⟩ := ih Ρn
        (fun y hy => absurd hy (h_kodhv y))
        (fun _ bb hb _ _ => absurd hb (h_kodhb bb))
      refine ⟨bolgeOrtamSahipAta Ρn yd tY,
        RegionTamam.r_gorev_baslat _ _ _ Ρkod' yd kod tY ?_
          h_kodhv h_kodhb h_kod' rfl, ?_⟩
      · intro v hv b hlk
        rw [h_hv v (HedefVar.gorev_yakala yd kod v hv)] at hlk
        exact h_cap v hv b hlk
      · intro y hy
        subst h_eq
        exact sahipAta_agree _ Ρn yd tY y hy
  -- D-332: kosul transport edilir; dallar BOLGE-NOTR oldugu icin yeni
  -- ortamda da GIRIS = CIKIS (regionNotr_cikis_esit) ve HedefBolge
  -- yukumlulukleri VAKUMDUR (regionNotr_hedefBolge_yok). Dal-birlestirme
  -- (ortam bulusmasi) GEREKMEZ — daraltmanin satin aldigi sey budur.
  | r_eger _ _ k d y h_rk h_nd h_ny _ _ ih_k ih_d ih_y =>
      intro Ρn h_hv h_hb
      obtain ⟨Ρkn, h_k', agree_k⟩ := ih_k Ρn
        (fun z hz => h_hv z (HedefVar.eger_kosul k d y z hz))
        (fun x bb hb hyz hlk =>
          h_hb x bb (HedefBolge.eger_kosul k d y bb hb) hyz hlk)
      obtain ⟨Ρdn, h_d', _⟩ := ih_d Ρkn
        (fun z hz => agree_k z (h_hv z (HedefVar.eger_dogru k d y z hz)))
        (fun _ bb hb _ _ => absurd hb (regionNotr_hedefBolge_yok h_nd bb))
      obtain ⟨Ρyn, h_y', _⟩ := ih_y Ρkn
        (fun z hz => agree_k z (h_hv z (HedefVar.eger_yanlis k d y z hz)))
        (fun _ bb hb _ _ => absurd hb (regionNotr_hedefBolge_yok h_ny bb))
      rw [regionNotr_cikis_esit h_d' h_nd] at h_d'
      rw [regionNotr_cikis_esit h_y' h_ny] at h_y'
      exact ⟨Ρkn, RegionTamam.r_eger _ _ _ k d y h_k' h_nd h_ny h_d' h_y', agree_k⟩
  -- D-335: iken — her sey NOTR, HedefBolge yukumlulukleri VAKUM.
  | r_iken _ k g h_nk h_ng _ _ ih_k ih_g =>
      intro Ρn h_hv _
      obtain ⟨Ρkn, h_k', agree_k⟩ := ih_k Ρn
        (fun z hz => h_hv z (HedefVar.iken_kosul k g z hz))
        (fun _ bb hb _ _ => absurd hb (regionNotr_hedefBolge_yok h_nk bb))
      obtain ⟨Ρgn, h_g', _⟩ := ih_g Ρn
        (fun z hz => h_hv z (HedefVar.iken_govde k g z hz))
        (fun _ bb hb _ _ => absurd hb (regionNotr_hedefBolge_yok h_ng bb))
      have e1 : Ρkn = Ρn := regionNotr_cikis_esit h_k' h_nk
      have e2 : Ρgn = Ρn := regionNotr_cikis_esit h_g' h_ng
      subst e1; subst e2
      exact ⟨_, RegionTamam.r_iken _ _ k g h_nk h_ng h_k' h_g', agree_k⟩
  -- D-335: esles — r_eger deseninin aynisi.
  | r_esles _ _ s n d y h_rs h_nd h_ny _ _ ih_s ih_d ih_y =>
      intro Ρn h_hv h_hb
      obtain ⟨Ρsn, h_s', agree_s⟩ := ih_s Ρn
        (fun z hz => h_hv z (HedefVar.esles_skrut s n d y z hz))
        (fun x bb hb hyz hlk =>
          h_hb x bb (HedefBolge.esles_skrut s n d y bb hb) hyz hlk)
      obtain ⟨Ρdn, h_d', _⟩ := ih_d Ρsn
        (fun z hz => agree_s z (h_hv z (HedefVar.esles_eslesen s n d y z hz)))
        (fun _ bb hb _ _ => absurd hb (regionNotr_hedefBolge_yok h_nd bb))
      obtain ⟨Ρyn, h_y', _⟩ := ih_y Ρsn
        (fun z hz => agree_s z (h_hv z (HedefVar.esles_kalan s n d y z hz)))
        (fun _ bb hb _ _ => absurd hb (regionNotr_hedefBolge_yok h_ny bb))
      rw [regionNotr_cikis_esit h_d' h_nd] at h_d'
      rw [regionNotr_cikis_esit h_y' h_ny] at h_y'
      exact ⟨Ρsn, RegionTamam.r_esles _ _ _ s n d y h_s' h_nd h_ny h_d' h_y',
        agree_s⟩
  | r_gorev_birlestir _ g =>
      exact fun Ρn _ _ => ⟨Ρn, RegionTamam.r_gorev_birlestir _ _ g, fun _ h => h⟩
  | r_kanal_gonder _ _ k v b h_lk h_yz h_eq =>
      intro Ρn h_hv _
      have h_lk' : bolgeOrtamGet Ρn v = some b := by
        rw [h_hv v (HedefVar.kanal_gonder k v)]; exact h_lk
      refine ⟨bolgeOrtamUpdate Ρn v
          (bolgeKategoriDegistir b (BolgeKategorisi.kanalRho k)),
        RegionTamam.r_kanal_gonder _ _ _ k v b h_lk' h_yz rfl, ?_⟩
      intro y hy
      subst h_eq
      rw [bolgeOrtamUpdate_get, bolgeOrtamUpdate_get]
      by_cases hv : v = y
      · rw [if_pos hv, if_pos hv]
      · rw [if_neg hv, if_neg hv]; exact hy
  | r_kanal_al _ k =>
      exact fun Ρn _ _ => ⟨Ρn, RegionTamam.r_kanal_al _ _ k, fun _ h => h⟩
  | r_dondur _ _ b x h_lk h_yz h_eq =>
      intro Ρn _ h_hb
      have h_lk' : bolgeOrtamGet Ρn x = some b :=
        h_hb x b (HedefBolge.dondur_bas b) h_yz h_lk
      refine ⟨bolgeOrtamDondurBolge Ρn b,
        RegionTamam.r_dondur _ _ _ b x h_lk' h_yz rfl, ?_⟩
      intro y hy
      subst h_eq
      rw [dondur_get, dondur_get, hy]
  | r_kullan _ x =>
      exact fun Ρn _ _ => ⟨Ρn, RegionTamam.r_kullan _ _ x, fun _ h => h⟩
  | r_imha _ x =>
      exact fun Ρn _ _ => ⟨Ρn, RegionTamam.r_imha _ _ x, fun _ h => h⟩
  | r_guvensiz _ _ e _ ih =>
      intro Ρn h_hv h_hb
      obtain ⟨Ρe', h_re', h_agree⟩ := ih Ρn
        (fun y hy => h_hv y (HedefVar.guvensiz_ic e y hy))
        (fun x bb hb hyz hlk =>
          h_hb x bb (HedefBolge.guvensiz_ic e bb hb) hyz hlk)
      exact ⟨Ρe', RegionTamam.r_guvensiz _ _ _ e h_re', h_agree⟩


-- ============================================================
-- §4.5. BolgeIliski + iliski-transport (Onarim v3 kapanis — cong
-- odak-yuku Region ayagi). "Runtime-Ρn, statik-Ρo" iliskisi:
-- yazilabilir kayitlar AYNEN korunur, kayitsizlik korunur, id'ler
-- korunur — tum R-premise'leri bu iliski altinda tasinir ve iliski
-- ciktilara KOMPOZISYONEL gecer (cong-zincirinin tek ihtiyaci).
-- ============================================================

/-- Ρn (runtime) ile Ρo (statik) iliskisi: (R2) yazilabilir-kayit
    aynen-koruma, (R3) kayitsizlik-koruma, (R4) id-koruma. -/
def BolgeIliski (Ρn Ρo : BolgeOrtam) : Prop :=
  (∀ y bb, bolgeOrtamGet Ρo y = some bb →
     kategoriYazilabilir bb.kategori = true → bolgeOrtamGet Ρn y = some bb)
  ∧ (∀ y, bolgeOrtamGet Ρo y = none → bolgeOrtamGet Ρn y = none)
  ∧ (∀ y bb, bolgeOrtamGet Ρo y = some bb →
     ∃ bb', bolgeOrtamGet Ρn y = some bb' ∧ bb'.id = bb.id)

theorem bolgeIliski_refl (Ρ : BolgeOrtam) : BolgeIliski Ρ Ρ :=
  ⟨fun _ _ h _ => h, fun _ h => h, fun _ bb h => ⟨bb, h, rfl⟩⟩

/-- sahipAta-cikti kayitsizligi giris kayitsizligini gerektirir. -/
theorem sahipAta_get_none_inv (Ρ : BolgeOrtam) (yd : List VarId)
    (t : ThreadId) (y : VarId)
    (h : bolgeOrtamGet (bolgeOrtamSahipAta Ρ yd t) y = none) :
    bolgeOrtamGet Ρ y = none := by
  cases h_lk : bolgeOrtamGet Ρ y with
  | none => rfl
  | some b =>
      by_cases h_in : y ∈ yd
      · rw [sahipAta_get_in' Ρ yd t y b h_in h_lk] at h; cases h
      · rw [sahipAta_get_notin Ρ yd t y h_in, h_lk] at h; cases h

/-- ILISKI-TRANSPORT: BolgeIliski altinda region-tiplenme korunur ve
    iliski CIKTILARA gecer. Tum premise'ler iliskiden ICERIDE turetilir
    (r_atama FIX-G cikti-kontrolu sayesinde (R2) ile dogrudan compose
    eder; yakalama/dondur/gonderim premise'leri yazilabilirlik uzerinden
    aynen tasinir). -/
theorem regionTamam_iliski_transport {Γ : TipOrtam}
    {Ρo Ρout : BolgeOrtam} {e : Ifade}
    (h : RegionTamam Γ Ρo e Ρout) :
    ∀ Ρn, BolgeIliski Ρn Ρo →
    ∃ Ρoutn, RegionTamam Γ Ρn e Ρoutn ∧ BolgeIliski Ρoutn Ρout := by
  induction h with
  | r_tanim _ x =>
      exact fun Ρn hi => ⟨Ρn, RegionTamam.r_tanim _ _ x, hi⟩
  | r_sabit _ v =>
      exact fun Ρn hi => ⟨Ρn, RegionTamam.r_sabit _ _ v, hi⟩
  | r_atama _ _ x e b _ h_gx h_yz ih =>
      intro Ρn hi
      obtain ⟨Ρen, h_re', hi'⟩ := ih Ρn hi
      exact ⟨Ρen, RegionTamam.r_atama _ _ _ x e b h_re'
        (hi'.1 x b h_gx h_yz) h_yz, hi'⟩
  -- D-336
  | r_indeks _ _ x e _ ih =>
      intro Ρn hi
      obtain ⟨Ρen, h_re', hi'⟩ := ih Ρn hi
      exact ⟨Ρen, RegionTamam.r_indeks _ _ _ x e h_re', hi'⟩
  -- D-337
  | r_indeks_ata _ _ _ x idx e b _ _ h_gx h_yz ih_i ih_e =>
      intro Ρn hi
      obtain ⟨Ρin, h_ri', hi_i⟩ := ih_i Ρn hi
      obtain ⟨Ρen, h_re', hi_e⟩ := ih_e Ρin hi_i
      exact ⟨Ρen, RegionTamam.r_indeks_ata _ _ _ _ x idx e b h_ri' h_re'
        (hi_e.1 x b h_gx h_yz) h_yz, hi_e⟩
  | r_seq _ _ _ a b _ _ ih_a ih_b =>
      intro Ρn hi
      obtain ⟨Ρan, h_ra', hi_a⟩ := ih_a Ρn hi
      obtain ⟨Ρbn, h_rb', hi_b⟩ := ih_b Ρan hi_a
      exact ⟨Ρbn, RegionTamam.r_seq _ _ _ _ a b h_ra' h_rb', hi_b⟩
  | r_topla _ _ _ a b _ _ ih_a ih_b =>
      intro Ρn hi
      obtain ⟨Ρan, h_ra', hi_a⟩ := ih_a Ρn hi
      obtain ⟨Ρbn, h_rb', hi_b⟩ := ih_b Ρan hi_a
      exact ⟨Ρbn, RegionTamam.r_topla _ _ _ _ a b h_ra' h_rb', hi_b⟩
  -- D-338
  | r_bol _ _ _ a b _ _ ih_a ih_b =>
      intro Ρn hi
      obtain ⟨Ρan, h_ra', hi_a⟩ := ih_a Ρn hi
      obtain ⟨Ρbn, h_rb', hi_b⟩ := ih_b Ρan hi_a
      exact ⟨Ρbn, RegionTamam.r_bol _ _ _ _ a b h_ra' h_rb', hi_b⟩
  | r_kalan _ _ _ a b _ _ ih_a ih_b =>
      intro Ρn hi
      obtain ⟨Ρan, h_ra', hi_a⟩ := ih_a Ρn hi
      obtain ⟨Ρbn, h_rb', hi_b⟩ := ih_b Ρan hi_a
      exact ⟨Ρbn, RegionTamam.r_kalan _ _ _ _ a b h_ra' h_rb', hi_b⟩
  -- D-332: dallar notr → yeni ortamda da kimlik; iliski kosuldan gelir.
  | r_eger _ _ k d y _ h_nd h_ny _ _ ih_k ih_d ih_y =>
      intro Ρn hi
      obtain ⟨Ρkn, h_k', hi_k⟩ := ih_k Ρn hi
      obtain ⟨Ρdn, h_d', _⟩ := ih_d Ρkn hi_k
      obtain ⟨Ρyn, h_y', _⟩ := ih_y Ρkn hi_k
      rw [regionNotr_cikis_esit h_d' h_nd] at h_d'
      rw [regionNotr_cikis_esit h_y' h_ny] at h_y'
      exact ⟨Ρkn, RegionTamam.r_eger _ _ _ k d y h_k' h_nd h_ny h_d' h_y', hi_k⟩
  -- D-335
  | r_iken _ k g h_nk h_ng _ _ ih_k ih_g =>
      intro Ρn hi
      obtain ⟨Ρkn, h_k', hi_k⟩ := ih_k Ρn hi
      obtain ⟨Ρgn, h_g', _⟩ := ih_g Ρn hi
      have e1 : Ρkn = Ρn := regionNotr_cikis_esit h_k' h_nk
      have e2 : Ρgn = Ρn := regionNotr_cikis_esit h_g' h_ng
      subst e1; subst e2
      exact ⟨_, RegionTamam.r_iken _ _ k g h_nk h_ng h_k' h_g', hi_k⟩
  | r_esles _ _ s n d y _ h_nd h_ny _ _ ih_s ih_d ih_y =>
      intro Ρn hi
      obtain ⟨Ρsn, h_s', hi_s⟩ := ih_s Ρn hi
      obtain ⟨Ρdn, h_d', _⟩ := ih_d Ρsn hi_s
      obtain ⟨Ρyn, h_y', _⟩ := ih_y Ρsn hi_s
      rw [regionNotr_cikis_esit h_d' h_nd] at h_d'
      rw [regionNotr_cikis_esit h_y' h_ny] at h_y'
      exact ⟨Ρsn, RegionTamam.r_esles _ _ _ s n d y h_s' h_nd h_ny h_d' h_y', hi_s⟩
  | r_gorev_baslat Ρo' _ Ρkod yd kod tY h_cap h_khv h_khb _ h_eq ih =>
      intro Ρn hi
      subst h_eq
      obtain ⟨Ρkodn, h_kod', _⟩ := ih Ρn hi
      refine ⟨bolgeOrtamSahipAta Ρn yd tY,
        RegionTamam.r_gorev_baslat _ _ _ Ρkodn yd kod tY ?_
          h_khv h_khb h_kod' rfl, ?_, ?_, ?_⟩
      · -- yakalama yazilabilirligi Ρn'de
        intro v hv b h_lk
        cases h_lko : bolgeOrtamGet Ρo' v with
        | none => rw [hi.2.1 v h_lko] at h_lk; cases h_lk
        | some b0 =>
            have h_yz0 := h_cap v hv b0 h_lko
            have h_n := hi.1 v b0 h_lko h_yz0
            rw [h_n] at h_lk
            rw [← Option.some.inj h_lk]
            exact h_yz0
      · -- (R2)
        intro y bb h_o h_yzb
        by_cases h_in : y ∈ yd
        · cases h_lko : bolgeOrtamGet Ρo' y with
          | none => rw [sahipAta_get_none Ρo' yd tY y h_lko] at h_o; cases h_o
          | some b0 =>
              rw [sahipAta_get_in' Ρo' yd tY y b0 h_in h_lko] at h_o
              rw [← Option.some.inj h_o] at h_yzb
              simp [bolgeKategoriDegistir, kategoriYazilabilir] at h_yzb
        · rw [sahipAta_get_notin Ρo' yd tY y h_in] at h_o
          rw [sahipAta_get_notin Ρn yd tY y h_in]
          exact hi.1 y bb h_o h_yzb
      · -- (R3)
        intro y h_o
        have h_bo := sahipAta_get_none_inv Ρo' yd tY y h_o
        exact sahipAta_get_none Ρn yd tY y (hi.2.1 y h_bo)
      · -- (R4)
        intro y bb h_o
        by_cases h_in : y ∈ yd
        · cases h_lko : bolgeOrtamGet Ρo' y with
          | none => rw [sahipAta_get_none Ρo' yd tY y h_lko] at h_o; cases h_o
          | some b0 =>
              rw [sahipAta_get_in' Ρo' yd tY y b0 h_in h_lko] at h_o
              have h_yz0 := h_cap y h_in b0 h_lko
              have h_n := hi.1 y b0 h_lko h_yz0
              refine ⟨bolgeKategoriDegistir b0 (BolgeKategorisi.sahip tY),
                sahipAta_get_in' Ρn yd tY y b0 h_in h_n, ?_⟩
              rw [← Option.some.inj h_o]
        · rw [sahipAta_get_notin Ρo' yd tY y h_in] at h_o
          obtain ⟨bb', h_n, h_id⟩ := hi.2.2 y bb h_o
          exact ⟨bb', (sahipAta_get_notin Ρn yd tY y h_in).trans h_n, h_id⟩
  | r_gorev_birlestir _ g =>
      exact fun Ρn hi => ⟨Ρn, RegionTamam.r_gorev_birlestir _ _ g, hi⟩
  | r_kanal_gonder Ρo' _ k v b h_lk h_yz h_eq =>
      intro Ρn hi
      subst h_eq
      have h_n := hi.1 v b h_lk h_yz
      refine ⟨bolgeOrtamUpdate Ρn v
          (bolgeKategoriDegistir b (BolgeKategorisi.kanalRho k)),
        RegionTamam.r_kanal_gonder _ _ _ k v b h_n h_yz rfl, ?_, ?_, ?_⟩
      · intro y bb h_o h_yzb
        rw [bolgeOrtamUpdate_get] at h_o
        by_cases hv : v = y
        · rw [if_pos hv] at h_o
          rw [← Option.some.inj h_o] at h_yzb
          simp [bolgeKategoriDegistir, kategoriYazilabilir] at h_yzb
        · rw [if_neg hv] at h_o
          rw [bolgeOrtamUpdate_get, if_neg hv]
          exact hi.1 y bb h_o h_yzb
      · intro y h_o
        rw [bolgeOrtamUpdate_get] at h_o
        by_cases hv : v = y
        · rw [if_pos hv] at h_o; cases h_o
        · rw [if_neg hv] at h_o
          rw [bolgeOrtamUpdate_get, if_neg hv]
          exact hi.2.1 y h_o
      · intro y bb h_o
        rw [bolgeOrtamUpdate_get] at h_o
        rw [bolgeOrtamUpdate_get]
        by_cases hv : v = y
        · rw [if_pos hv] at h_o
          rw [if_pos hv]
          exact ⟨_, rfl, by rw [← Option.some.inj h_o]⟩
        · rw [if_neg hv] at h_o
          rw [if_neg hv]
          exact hi.2.2 y bb h_o
  | r_kanal_al _ k =>
      exact fun Ρn hi => ⟨Ρn, RegionTamam.r_kanal_al _ _ k, hi⟩
  | r_dondur Ρo' _ b x h_lk h_yz h_eq =>
      intro Ρn hi
      subst h_eq
      have h_n := hi.1 x b h_lk h_yz
      refine ⟨bolgeOrtamDondurBolge Ρn b,
        RegionTamam.r_dondur _ _ _ b x h_n h_yz rfl, ?_, ?_, ?_⟩
      · intro y bb h_o h_yzb
        rw [dondur_get] at h_o
        cases h_lko : bolgeOrtamGet Ρo' y with
        | none => rw [h_lko] at h_o; cases h_o
        | some b0 =>
            rw [h_lko, Option.map_some] at h_o
            by_cases hid : b0.id = b.id
            · rw [if_pos hid] at h_o
              rw [← Option.some.inj h_o] at h_yzb
              simp [bolgeKategoriDegistir, kategoriYazilabilir] at h_yzb
            · rw [if_neg hid] at h_o
              have h_b0bb := Option.some.inj h_o
              rw [h_b0bb] at h_lko hid
              have h_nn := hi.1 y bb h_lko h_yzb
              rw [dondur_get, h_nn, Option.map_some, if_neg hid]
      · intro y h_o
        rw [dondur_get] at h_o
        cases h_lko : bolgeOrtamGet Ρo' y with
        | none =>
            rw [dondur_get, hi.2.1 y h_lko]
            rfl
        | some b0 => rw [h_lko, Option.map_some] at h_o; cases h_o
      · intro y bb h_o
        rw [dondur_get] at h_o
        cases h_lko : bolgeOrtamGet Ρo' y with
        | none => rw [h_lko] at h_o; cases h_o
        | some b0 =>
            rw [h_lko, Option.map_some] at h_o
            obtain ⟨b0', h_n0, h_id0⟩ := hi.2.2 y b0 h_lko
            rw [dondur_get, h_n0, Option.map_some]
            by_cases hid : b0.id = b.id
            · rw [if_pos hid] at h_o
              rw [if_pos (h_id0.trans hid)]
              refine ⟨_, rfl, ?_⟩
              rw [← Option.some.inj h_o]
              exact h_id0
            · rw [if_neg hid] at h_o
              rw [if_neg (fun hh => hid (h_id0.symm.trans hh))]
              refine ⟨b0', rfl, ?_⟩
              rw [← Option.some.inj h_o]
              exact h_id0
  | r_kullan _ x =>
      exact fun Ρn hi => ⟨Ρn, RegionTamam.r_kullan _ _ x, hi⟩
  | r_imha _ x =>
      exact fun Ρn hi => ⟨Ρn, RegionTamam.r_imha _ _ x, hi⟩
  | r_guvensiz _ _ e _ ih =>
      intro Ρn hi
      obtain ⟨Ρen, h_re', hi'⟩ := ih Ρn hi
      exact ⟨Ρen, RegionTamam.r_guvensiz _ _ _ e h_re', hi'⟩


-- ============================================================
-- §5. NOT (Onarim v3 F1): birlesim + meta katmanlari TASINDI
-- ============================================================
-- Typed + ThreadTipliFull + KonfTipliFull (+intro/elim, bolgeOrtamBos)
--   → Kemgu/Sem/Tipli.lean
-- progress_region + preservation_region
--   → Kemgu/Meta/ProgressKorunum.lean (Typed-formda dedup:
--     progress_typed / preservation_typed)

end Kemgu.Sem.RegionTamam
