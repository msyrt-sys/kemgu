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
  | r_seq _ _ _ _ _ _ _ ih_a ih_b =>
      exact fun y b h_o h_y => ih_a y b (ih_b y b h_o h_y) h_y
  | r_gorev_baslat _ _ _ yd _ tY _ _ _ _ h_eq _ =>
      intro y b h_o h_y
      subst h_eq
      rcases sahipAta_get_inv _ tY yd _ y b h_o with
          ⟨b0, _, h_bb, _⟩ | h_acc
      · rw [h_bb] at h_y
        simp [bolgeKategoriDegistir, kategoriYazilabilir] at h_y
      · exact h_acc
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
  | r_atama _ _ x e b h_gx h_yz _ ih =>
      intro Ρn h_hv h_hb
      obtain ⟨Ρe', h_re', h_agree⟩ := ih Ρn
        (fun y hy => h_hv y (HedefVar.atama_ic x e y hy))
        (fun x' bb hb hyz hlk =>
          h_hb x' bb (HedefBolge.atama_ic x e bb hb) hyz hlk)
      have h_gx' : bolgeOrtamGet Ρn x = some b := by
        rw [h_hv x (HedefVar.atama_bas x e)]; exact h_gx
      exact ⟨Ρe', RegionTamam.r_atama _ _ _ x e b h_gx' h_yz h_re', h_agree⟩
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
-- §5. NOT (Onarim v3 F1): birlesim + meta katmanlari TASINDI
-- ============================================================
-- Typed + ThreadTipliFull + KonfTipliFull (+intro/elim, bolgeOrtamBos)
--   → Kemgu/Sem/Tipli.lean
-- progress_region + preservation_region
--   → Kemgu/Meta/ProgressKorunum.lean (Typed-formda dedup:
--     progress_typed / preservation_typed)

end Kemgu.Sem.RegionTamam
