/-
KEMGU DRF Mekanize — Operasyonel Semantik Cekirdek Tipleri (Faz A2.2)
Kaynak (kagit formel): belgeler/KEMGU_Operasyonel_Semantik.md §1-3
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz (Lean core yeterli)
-/

namespace Kemgu.Sem.Core

-- ============================================================
-- §1. Tanitici (identifier) aliaslari
-- ============================================================

/-- Degisken kimligi -/
abbrev VarId := Nat

/-- Bolge kimligi (her tahsis edilmis bolge benzersiz numara alir) -/
abbrev RegId := Nat

/-- Thread kimligi (tx0, tx1, ...) -/
abbrev ThreadId := Nat

/-- Kanal kimligi -/
abbrev KanalId := Nat

/-- AST dugum kimligi (bolge atama Pho icin) -/
abbrev DugumId := Nat

/-- Zaman damgasi (sistem reduksiyon adim sayaci) -/
abbrev Zaman := Nat


-- ============================================================
-- §2. Tip sozdizimi (DRF icin gerekli minimal subset)
-- Kaynak: Op.Sem §1 grammar
-- ============================================================

/-- Tip sozdizimi (Op.Sem §1). DRF teoreminin kapsami icin
    yeterli minimal subset. Tam grammar belgeler/KEMGU_Grammar_EBNF.md'de. -/
inductive Tip : Type where
  | bos        : Tip                       -- bos (Unit, Op.Sem §1; Plan v2 Adim 2'de eklendi)
  | scalar     : Tip                       -- skaler kategori (tam_w/dtam_w/...)
  | mantiksal  : Tip                       -- mantiksal (Bool, Op.Sem §1; Plan v2 Adim 2'de eklendi)
  | metin      : Tip                       -- metin (string, Op.Sem §1; Plan v2 Adim 2'de eklendi)
  | refIm      : Tip → Tip                 -- &T  (immutable reference)
  | refMut     : Tip → Tip                 -- &degisken T
  | ptr        : Tip → Tip                 -- *T  (guvensiz)
  | yapi       : String → Tip              -- yapi T (nominal)
  | dizi       : Tip → Tip                 -- Dizi<T>
  | tekkez     : Tip → Tip                 -- tekkez<T>  (Linear V1)
  | yetki      : String → Tip              -- yetki<R>   (Capability V1)
  | gorev      : Tip → Tip                 -- gorev<T>   (Concurrency)
  | kanal      : Tip → Tip                 -- kanal<T>   (Concurrency)
  | sabitsure  : Tip → Tip                 -- sabitsure<T>
  | islev      : List Tip → Tip → Tip      -- islev(T_) -> T
deriving Repr


-- ============================================================
-- §3. Bolge kategorisi + Bolge
-- Kaynak: Op.Sem §2.3 + KEMGU_Bellek_Modeli.md (Katman 1 + 2)
-- ============================================================

/-- Bolge kategorisi.
    Katman 1: lit / yerel / cagiran / iterasyon / global
    Katman 2: sahip / kanal / donmus -/
inductive BolgeKategorisi : Type where
  | lit                                    -- Pho_lit (kalici literaller)
  | yerel        (id : DugumId)            -- Pho_yerel(f)
  | cagiran      (id : DugumId)            -- Pho_cagiran(f)
  | iterasyon    (id : DugumId)            -- Pho_iterasyon(d)
  | global                                 -- Pho_global
  | sahip        (t : ThreadId)            -- Pho_sahip(t)   — Katman 2
  | kanalRho     (k : KanalId)             -- Pho_kanal(k)   — Katman 2
  | donmus                                 -- Pho_donmus     — Katman 2
deriving Repr, DecidableEq

structure Bolge where
  id        : RegId
  kategori  : BolgeKategorisi
deriving Repr, DecidableEq


-- ============================================================
-- §4. Deger (store icerigi, Op.Sem §3.2)
-- ============================================================

mutual

/-- Runtime degerleri. Bellek hucresinde duracak icerikler. -/
inductive Deger : Type where
  | skaler      (n : Int)                              -- tamsayi / bool / karakter
  | metinDeg    (b : Bolge) (uzunluk : Nat)            -- (ptr, bolge, byte_uz)
  | yapiVal     (b : Bolge) (alanlar : List Deger)
  | diziVal     (b : Bolge) (uzunluk : Nat)
  | closureVal  (kodId : DugumId) (yakalama : List VarId)
  | yetkiTok    (id : Nat) (kaynak : String)
  | gorevVal    (t : ThreadId)                         -- gorev tanitici (F2: spawn sonucu)
  | birim                                              -- () bos tip
end


/-- Dallanma kosulunun DOGRU sayilip sayilmadigi (D-332).
    `Deger` uzerinde DecidableEq TUREMEZ (mutual + List Deger), o yuzden
    kosul testi desen-eslemesiyle yazilir — toplam ve karar verilebilir.
    Yalniz `skaler`in sifir olmasi ve `birim` YANLIS'tir; diger butun
    degerler (isaretci-benzeri: metin/yapi/dizi/closure/yetki/gorev)
    DOGRU sayilir. -/
def degerDogruMu : Deger → Bool
  | .skaler n => n ≠ 0
  | .birim    => false
  | _         => true


-- ============================================================
-- §5. Lineerlik durumu (Lambda — Linear V1)
-- Kaynak: Op.Sem §2.2
-- ============================================================

/-- Bir lineer baglamanin tuketim durumu. -/
inductive Lineerlik : Type where
  | aktif
  | tuketildi
deriving Repr, DecidableEq

/-- Lambda : VarId → Lineerlik (assoc list ile) -/
abbrev LineerOrtam := List (VarId × Lineerlik)

/-- LineerOrtam Λ lookup: bir VarId'nin Lineerlik durumu.
    (F2: LineerTamam'dan Core'a tasindi — Step kurallari da kullanir.) -/
def lineerOrtamGet : LineerOrtam → VarId → Option Lineerlik
  | [], _ => none
  | (k, v) :: rest, key => if k = key then some v else lineerOrtamGet rest key

/-- LineerOrtam Λ update: prepend (newest-wins). -/
def lineerOrtamUpdate (Λ : LineerOrtam) (x : VarId) (lin : Lineerlik) : LineerOrtam :=
  (x, lin) :: Λ

/-- Tek degiskeni tuket (aktifse tuketildi'ye gecir; degilse dokunma).
    cKanalGonderTamam ctx' guncellemesi. -/
def lineerTuket (Λ : LineerOrtam) (v : VarId) : LineerOrtam :=
  match lineerOrtamGet Λ v with
  | some Lineerlik.aktif => lineerOrtamUpdate Λ v Lineerlik.tuketildi
  | _ => Λ

/-- Yakalama listesindeki aktif baglamalari tuket (cGorevBaslatTamam). -/
def lineerTuketListe (Λ : LineerOrtam) (yd : List VarId) : LineerOrtam :=
  yd.foldl lineerTuket Λ

/-- lineerTuket baska degiskenin lookup'unu degistirmez. -/
theorem lineerTuket_baska (Λ : LineerOrtam) (w x : VarId) (h_ne : w ≠ x) :
    lineerOrtamGet (lineerTuket Λ w) x = lineerOrtamGet Λ x := by
  unfold lineerTuket
  cases hw : lineerOrtamGet Λ w with
  | none => rfl
  | some lin =>
    cases lin with
    | aktif => simp [lineerOrtamUpdate, lineerOrtamGet, h_ne]
    | tuketildi => rfl

/-- lineerTuket mevcut 'tuketildi' lookup'larini korur. -/
theorem lineerTuket_korur (Λ : LineerOrtam) (x w : VarId)
    (h : lineerOrtamGet Λ x = some Lineerlik.tuketildi) :
    lineerOrtamGet (lineerTuket Λ w) x = some Lineerlik.tuketildi := by
  by_cases h_ne : w = x
  · subst h_ne
    unfold lineerTuket
    rw [h]
    exact h
  · rw [lineerTuket_baska Λ w x h_ne]; exact h

/-- lineerTuket aktif degiskeni tuketildi yapar. -/
theorem lineerTuket_tuketir (Λ : LineerOrtam) (w : VarId)
    (h : lineerOrtamGet Λ w = some Lineerlik.aktif) :
    lineerOrtamGet (lineerTuket Λ w) w = some Lineerlik.tuketildi := by
  unfold lineerTuket
  rw [h]
  show lineerOrtamGet (lineerOrtamUpdate Λ w Lineerlik.tuketildi) w
      = some Lineerlik.tuketildi
  simp [lineerOrtamUpdate, lineerOrtamGet]

/-- lineerTuketListe mevcut 'tuketildi' lookup'larini korur (fold). -/
theorem lineerTuketListe_korur (yd : List VarId) (Λ : LineerOrtam) (x : VarId)
    (h : lineerOrtamGet Λ x = some Lineerlik.tuketildi) :
    lineerOrtamGet (lineerTuketListe Λ yd) x = some Lineerlik.tuketildi := by
  induction yd generalizing Λ with
  | nil => exact h
  | cons w rest ih =>
      exact ih (lineerTuket Λ w) (lineerTuket_korur Λ x w h)

/-- DRF-L2 cekirdegi: yakalama listesindeki aktif lineer baglama,
    lineerTuketListe sonrasi TUKETILDI olur. -/
theorem lineerTuketListe_tuketir (yd : List VarId) (Λ : LineerOrtam)
    (v : VarId) (h_v : v ∈ yd)
    (h_aktif : lineerOrtamGet Λ v = some Lineerlik.aktif) :
    lineerOrtamGet (lineerTuketListe Λ yd) v = some Lineerlik.tuketildi := by
  induction yd generalizing Λ with
  | nil => cases h_v
  | cons w rest ih =>
      by_cases h_wv : w = v
      · subst h_wv
        have h_w : lineerOrtamGet (lineerTuket Λ w) w
            = some Lineerlik.tuketildi := lineerTuket_tuketir Λ w h_aktif
        exact lineerTuketListe_korur rest (lineerTuket Λ w) w h_w
      · have h_v_rest : v ∈ rest := by
          cases h_v with
          | head => exact absurd rfl h_wv
          | tail _ h => exact h
        have h_aktif' : lineerOrtamGet (lineerTuket Λ w) v
            = some Lineerlik.aktif := by
          rw [lineerTuket_baska Λ w v h_wv]; exact h_aktif
        exact ih (lineerTuket Λ w) h_v_rest h_aktif'

/-- Lookup uyeligi: get bir deger donduruyorsa o cift listededir.
    (Aile 2 kopru gecisleri icin — gölgeleme yonu tek tarafli.) -/
theorem lineerOrtamGet_mem : ∀ (Λ : LineerOrtam) (x : VarId) (l : Lineerlik),
    lineerOrtamGet Λ x = some l → (x, l) ∈ Λ
  | [], _, _, h => by simp [lineerOrtamGet] at h
  | (k, v) :: rest, x, l, h => by
      by_cases hk : k = x
      · subst hk
        simp [lineerOrtamGet] at h
        subst h
        exact List.Mem.head _
      · simp [lineerOrtamGet, hk] at h
        exact List.Mem.tail _ (lineerOrtamGet_mem rest x l h)


-- ============================================================
-- §6. Konum + Store (sigma)
-- Kaynak: Op.Sem §3.1
-- ============================================================

/-- Bellek konumu: bolge × ofset (Op.Sem §3.1) -/
structure Konum where
  bolge : Bolge
  ofset : Nat
deriving Repr, DecidableEq

/-- Store sigma : Konum → Deger (assoc list temsili) -/
abbrev Store := List (Konum × Deger)

/-- Store lookup: ilk eslesen entry (newest-wins; sAtamaTamam prepend eder).
    F2: sVarOku (degisken okuma) ve cKanalGonderTamam (gonderilen deger)
    bu lookup'i kullanir. Offset-0 konvansiyonu: degisken x'in konumu
    ⟨bolge(x), 0⟩ (V1).

    ID-ANAHTARLAMA (Onarim v3 — DECISIONS_LOG kategori-anahtar karari):
    karsilastirma bolge KIMLIGI (.id) + ofset uzerinden — kategori
    DEGISKEN OZNITELIKTIR (recat erisimi koparmaz). BolgeAyrik
    (id↔degisken injektivligi) bu anahtarlamanin tutarlilik temelidir. -/
def konumGet : Store → Konum → Option Deger
  | [], _ => none
  | (k, v) :: rest, key =>
      if k.bolge.id = key.bolge.id ∧ k.ofset = key.ofset then some v
      else konumGet rest key

/-- Lookup uyeligi (store, id-formu): get bir deger donduruyorsa
    id+ofset eslesen bir cift listededir. -/
theorem konumGet_mem : ∀ (s : Store) (k : Konum) (v : Deger),
    konumGet s k = some v →
    ∃ k', (k', v) ∈ s ∧ k'.bolge.id = k.bolge.id ∧ k'.ofset = k.ofset
  | [], _, _, h => by simp [konumGet] at h
  | (k0, v0) :: rest, k, v, h => by
      by_cases hk : k0.bolge.id = k.bolge.id ∧ k0.ofset = k.ofset
      · rw [konumGet, if_pos hk] at h
        exact ⟨k0, (Option.some.inj h) ▸ List.Mem.head _, hk⟩
      · rw [konumGet, if_neg hk] at h
        obtain ⟨k', h_mem, h_id⟩ := konumGet_mem rest k v h
        exact ⟨k', List.Mem.tail _ h_mem, h_id⟩

/-- Lookup id-kongruansi (store): id+ofset esit anahtarlar ayni sonucu verir. -/
theorem konumGet_id_esit : ∀ (s : Store) (k k' : Konum),
    k.bolge.id = k'.bolge.id → k.ofset = k'.ofset →
    konumGet s k = konumGet s k'
  | [], _, _, _, _ => rfl
  | (k0, v0) :: rest, k, k', h_id, h_of => by
      rw [konumGet, konumGet]
      by_cases hk : k0.bolge.id = k.bolge.id ∧ k0.ofset = k.ofset
      · rw [if_pos hk, if_pos ⟨hk.1.trans h_id, hk.2.trans h_of⟩]
      · rw [if_neg hk,
            if_neg (fun hc => hk ⟨hc.1.trans h_id.symm, hc.2.trans h_of.symm⟩)]
        exact konumGet_id_esit rest k k' h_id h_of


-- ============================================================
-- §7. Sahiplik haritasi (Sigma — Katman 2)
-- Kaynak: Op.Sem §2.4
-- ============================================================

/-- Bir bolgenin sahip durumu.
    ⊥ = bos (sahibi yok), donmus = R-PAYLAS (coklu okuyucu, sifir yazici),
    kanalSahip = kanal transit'inde (R-KANAL gonderim sonrasi, alim oncesi). -/
inductive Sahip : Type where
  | bos                              -- ⊥ (henuz sahibi yok)
  | thread (t : ThreadId)            -- belirli thread sahip
  | kanalSahip (k : KanalId)         -- kanal transit'inde — Op.Sem §5.4 C-KANAL-GONDER
  | donmus                           -- DONMUS (R-PAYLAS, coklu okuyucu)
deriving Repr, DecidableEq

/-- Sigma : Bolge → Sahip (GUNCEL-DURUM modeli; assoc list, newest-wins).

    Onarim v3 F2 (Sorun 3 onarimi — ADIM0_DENETIM_RAPORU §2.2):
    Eski model (Bolge × Zaman) anahtarliydi ve tam-anahtar lookup
    "z aninda verilen sahiplik z+1'de gorunmez" kusurunu tasiyordu
    (h_owner guard'i sonlu baslangic listesiyle yeterince adim sonra
    hic saglanamiyordu → progress yapisal imkansiz). Guncel-durum
    modeli lineerOrtam/bolgeOrtam idiyomuyla ayni; sahiplik TARIHI
    artik iz'de (Olay listesi) yasar. Mehmet onayi: 2026-06-11. -/
abbrev Sahiplik := List (Bolge × Sahip)


-- ============================================================
-- §7.1. Sahiplik lookup + atomic set + temel lemma'lar
-- DRF-L0 (Bolge Korunumu) ve diger lemmalar bu helper'lara dayanir.
-- Kaynak: Op.Sem §5.4 R-* aksiyomlarinin atomic transfer semantigi
-- ============================================================

/-- Sahiplik lookup: bolgenin GUNCEL sahibi (ilk eslesen entry —
    newest-wins; sahiplikSet prepend kullanir).

    ID-ANAHTARLAMA (Onarim v3 — DECISIONS_LOG kategori-anahtar karari):
    karsilastirma bolge KIMLIGI (.id) uzerinden — kategori degisken
    ozniteliktir; recat (sahipAta/dondurBolge) sahiplik erisimini
    koparmaz. BolgeAyrik (id↔degisken injektivligi) temel. -/
def sahiplikGet : Sahiplik → Bolge → Option Sahip
  | [], _ => none
  | (k, v) :: rest, key => if k.id = key.id then some v else sahiplikGet rest key

/-- Lookup id-kongruansi (sahiplik): id-esit bolgeler ayni sonucu verir. -/
theorem sahiplikGet_id_esit : ∀ (s : Sahiplik) (b b' : Bolge),
    b.id = b'.id → sahiplikGet s b = sahiplikGet s b'
  | [], _, _, _ => rfl
  | (k0, v0) :: rest, b, b', h_id => by
      rw [sahiplikGet, sahiplikGet]
      by_cases hk : k0.id = b.id
      · rw [if_pos hk, if_pos (hk.trans h_id)]
      · rw [if_neg hk, if_neg (fun hc => hk (hc.trans h_id.symm))]
        exact sahiplikGet_id_esit rest b b' h_id

/-- Sahiplik atomik set (Op.Sem §5.4 S3 atomic transfer): prepend;
    yeni entry eskisini mantiken override eder. -/
def sahiplikSet (s : Sahiplik) (b : Bolge) (yeni : Sahip) : Sahiplik :=
  (b, yeni) :: s

/-- Coklu bolge atomic set (R-GOREV / R-BIRLESTIR). -/
def sahiplikSetMany (s : Sahiplik) (bs : List Bolge) (yeni : Sahip) : Sahiplik :=
  bs.foldl (fun acc b => sahiplikSet acc b yeni) s

/-- Set sonra get ayni anahtarda yeni degeri doner. -/
theorem sahiplikSet_eq (s : Sahiplik) (b : Bolge) (yeni : Sahip) :
    sahiplikGet (sahiplikSet s b yeni) b = some yeni := by
  simp [sahiplikSet, sahiplikGet]

/-- Set sonra get id-farkli anahtarda eski lookup degismez (id-formu). -/
theorem sahiplikSet_ne (s : Sahiplik) (b b' : Bolge) (yeni : Sahip)
    (h : b'.id ≠ b.id) :
    sahiplikGet (sahiplikSet s b yeni) b' = sahiplikGet s b' := by
  rw [sahiplikSet, sahiplikGet, if_neg (fun he => h he.symm)]

/-- Lookup uyeligi (sahiplik, id-formu): get bir deger donduruyorsa
    id-eslesen bir cift listededir. -/
theorem sahiplikGet_mem : ∀ (s : Sahiplik) (b : Bolge) (v : Sahip),
    sahiplikGet s b = some v → ∃ b'', (b'', v) ∈ s ∧ b''.id = b.id
  | [], _, _, h => by simp [sahiplikGet] at h
  | (k, w) :: rest, b, v, h => by
      by_cases hk : k.id = b.id
      · rw [sahiplikGet, if_pos hk] at h
        exact ⟨k, (Option.some.inj h) ▸ List.Mem.head _, hk⟩
      · rw [sahiplikGet, if_neg hk] at h
        obtain ⟨b'', h_mem, h_id⟩ := sahiplikGet_mem rest b v h
        exact ⟨b'', List.Mem.tail _ h_mem, h_id⟩

/-- SetMany ana analiz (id-formu): lookup ya yeni atamadan gelir
    (id-eslesen liste uyesi tanigiyla) ya da liste id-ayriksa eskidir. -/
theorem sahiplikSetMany_analiz (bs : List Bolge) (s : Sahiplik)
    (yeni : Sahip) (b : Bolge) :
    (sahiplikGet (sahiplikSetMany s bs yeni) b = some yeni
       ∧ ∃ b'' ∈ bs, b''.id = b.id)
    ∨ (sahiplikGet (sahiplikSetMany s bs yeni) b = sahiplikGet s b
       ∧ ∀ b'' ∈ bs, b''.id ≠ b.id) := by
  induction bs generalizing s with
  | nil => exact Or.inr ⟨rfl, fun _ hm => absurd hm (List.not_mem_nil)⟩
  | cons b1 rest ih =>
      rcases ih (sahiplikSet s b1 yeni) with ⟨h_get, b'', h_mem, h_id⟩ | ⟨h_get, h_yok⟩
      · exact Or.inl ⟨h_get, b'', List.Mem.tail _ h_mem, h_id⟩
      · by_cases h_eq : b1.id = b.id
        · refine Or.inl ⟨?_, b1, List.Mem.head _, h_eq⟩
          show sahiplikGet (sahiplikSetMany (sahiplikSet s b1 yeni) rest yeni) b
              = some yeni
          rw [h_get, sahiplikSet, sahiplikGet, if_pos h_eq]
        · refine Or.inr ⟨?_, ?_⟩
          · show sahiplikGet (sahiplikSetMany (sahiplikSet s b1 yeni) rest yeni) b
                = sahiplikGet s b
            rw [h_get]
            exact sahiplikSet_ne s b1 b yeni (fun he => h_eq he.symm)
          · intro b'' h_mem
            cases h_mem with
            | head => exact h_eq
            | tail _ hr => exact h_yok b'' hr

/-- SetMany, id-bazinda listeyle ayrik bolgenin lookup'unu degistirmez
    (Frozen-persistence ispatinin cekirdegi — F2; id-formu). -/
theorem sahiplikSetMany_ne (bs : List Bolge) (s : Sahiplik) (b' : Bolge)
    (yeni : Sahip) (h : ∀ b'' ∈ bs, b''.id ≠ b'.id) :
    sahiplikGet (sahiplikSetMany s bs yeni) b' = sahiplikGet s b' := by
  rcases sahiplikSetMany_analiz bs s yeni b' with ⟨_, b'', h_mem, h_id⟩ | ⟨h_get, _⟩
  · exact absurd h_id (h b'' h_mem)
  · exact h_get

/-- SetMany lookup analizi (id-formu): sonuc ya yeni atamadan
    (id-eslesen uye tanigiyla) ya eskidir. -/
theorem sahiplikSetMany_lookup_inv (bs : List Bolge) (s : Sahiplik)
    (yeni : Sahip) (b : Bolge) (v : Sahip)
    (h : sahiplikGet (sahiplikSetMany s bs yeni) b = some v) :
    ((∃ b'' ∈ bs, b''.id = b.id) ∧ v = yeni) ∨ sahiplikGet s b = some v := by
  rcases sahiplikSetMany_analiz bs s yeni b with ⟨h_get, h_tanik⟩ | ⟨h_get, _⟩
  · rw [h_get] at h
    exact Or.inl ⟨h_tanik, (Option.some.inj h).symm⟩
  · rw [h_get] at h
    exact Or.inr h

/-- LISTE CIFT-AYRISIM KILIDI (FIX-F cerceve kosullariyla — Onarim v3
    kapanis): ayni-prefix iki ayrisim + her iki tarafta yan-sekil
    (suffix esit veya tek-append) + odak-degisimi (cR' ≠ cR) →
    ayrisimlar AYNI pozisyondadir ve bilesenler esittir. -/
theorem cerrah_kilit {α : Type} :
    ∀ (ts1 u1 : List α) {c cR c' cR' : α} {ts2 u2 ts2' u2X : List α},
    u1 ++ cR :: u2 = ts1 ++ c :: ts2 →
    u1 ++ cR' :: u2X = ts1 ++ c' :: ts2' →
    (u2X = u2 ∨ ∃ ch, u2X = u2 ++ [ch]) →
    (ts2' = ts2 ∨ ∃ y, ts2' = ts2 ++ [y]) →
    cR' ≠ cR →
    ts1 = u1 ∧ c = cR ∧ c' = cR' ∧ ts2 = u2 ∧ ts2' = u2X := by
  intro ts1
  induction ts1 with
  | nil =>
      intro u1 c cR c' cR' ts2 u2 ts2' u2X h1 h2 h_u2X h_ts2' h_ne
      cases u1 with
      | nil =>
          simp only [List.nil_append] at h1 h2
          injection h1 with h_c h_t
          injection h2 with h_c' h_t'
          exact ⟨rfl, h_c.symm, h_c'.symm, h_t.symm, h_t'.symm⟩
      | cons a u1' =>
          simp only [List.cons_append, List.nil_append] at h1 h2
          injection h1 with h_ca h_t1
          injection h2 with h_ca' h_t1'
          exfalso
          rcases h_ts2' with h_e | ⟨y, h_e⟩
          · rw [h_e, ← h_t1] at h_t1'
            have h_tail := List.append_cancel_left h_t1'
            injection h_tail with h_head _
            exact h_ne h_head
          · rcases h_u2X with h_x | ⟨ch, h_x⟩
            · rw [h_e, ← h_t1, h_x] at h_t1'
              have h_len := congrArg List.length h_t1'
              simp [List.length_append] at h_len
            · rw [h_e, ← h_t1, h_x] at h_t1'
              rw [show (u1' ++ cR :: u2) ++ [y] = u1' ++ cR :: (u2 ++ [y])
                    by simp] at h_t1'
              have h_tail := List.append_cancel_left h_t1'
              injection h_tail with h_head _
              exact h_ne h_head
  | cons bHead ts1' ih =>
      intro u1 c cR c' cR' ts2 u2 ts2' u2X h1 h2 h_u2X h_ts2' h_ne
      cases u1 with
      | nil =>
          simp only [List.nil_append, List.cons_append] at h1 h2
          injection h1 with h_b h_t1
          injection h2 with h_b' h_t1'
          exact absurd (h_b'.trans h_b.symm) h_ne
      | cons a u1' =>
          simp only [List.cons_append] at h1 h2
          injection h1 with h_ab h_t1
          injection h2 with h_ab' h_t1'
          obtain ⟨h_1, h_2, h_3, h_4, h_5⟩ :=
            ih u1' h_t1 h_t1' h_u2X h_ts2' h_ne
          refine ⟨?_, h_2, h_3, h_4, h_5⟩
          rw [h_ab, h_1]

/-- head? bir deger donduruyorsa o listededir. -/
theorem head?_mem {α : Type} {l : List α} {v : α}
    (h : l.head? = some v) : v ∈ l := by
  cases l with
  | nil => cases h
  | cons a rest =>
      have : a = v := Option.some.inj h
      rw [← this]
      exact List.Mem.head _

/-- tail uyeligi tam listeye tasinir. -/
theorem tail_uye {α : Type} {l : List α} {w : α}
    (h : w ∈ l.tail) : w ∈ l := by
  cases l with
  | nil => cases h
  | cons a rest => exact List.Mem.tail _ h

/-- tail uzunlugu kucuk-esittir. -/
theorem tail_uzunluk {α : Type} (l : List α) :
    l.tail.length ≤ l.length := by
  cases l with
  | nil => exact Nat.le_refl _
  | cons a rest => exact Nat.le_succ _

/-- Kapasite-1 dolu kuyrugun tail'i bos. -/
theorem tail_bos_kapasite {α : Type} {q : List α}
    (h_kap : q.length ≤ 1) (h_dolu : q ≠ []) : q.tail = [] := by
  cases q with
  | nil => exact absurd rfl h_dolu
  | cons a rest =>
      cases rest with
      | nil => rfl
      | cons b r2 =>
          exfalso
          simp [List.length] at h_kap

/-- SetMany, listedeki her bolgenin lookup'unu yeni degere goturur
    (tum yeni entry'ler ayni degeri tasir; id-anahtarla bile uniform). -/
theorem sahiplikSetMany_mem (s : Sahiplik) (bs : List Bolge) (yeni : Sahip)
    {b : Bolge} (h : b ∈ bs) :
    sahiplikGet (sahiplikSetMany s bs yeni) b = some yeni := by
  rcases sahiplikSetMany_analiz bs s yeni b with ⟨h_get, _⟩ | ⟨_, h_yok⟩
  · exact h_get
  · exact absurd rfl (h_yok b h)


-- ============================================================
-- §8. Olay + Iz (Op.Sem §6.1)
-- ============================================================

/-- Gozlemlenebilir olay. Reduksiyon olay uretir; iz olaylari biriktirir. -/
inductive Olay : Type where
  | memOku        (t : ThreadId) (k : Konum) (v : Deger)
  | memYaz        (t : ThreadId) (k : Konum) (v : Deger)
  | threadBaslat  (t : ThreadId)
  | threadBitir   (t : ThreadId)
  | kanalGonderOl (t : ThreadId) (k : KanalId) (v : Deger)
  | kanalAlOl     (t : ThreadId) (k : KanalId) (v : Deger)
  | dondurOl      (t : ThreadId) (b : Bolge)
  /-- Dallanma karari (D-332, Mehmet onayi). SideChannel/CT.Gozlem.oDal'in
      Core karsiligi: dal hedefleri farkli kod/zaman → PC/timing sizintisi,
      dolayisiyla saldirganin GORDUGU bir olaydir. Bu olay OLMADAN kopru
      ct_ni'nin asil icerigini (CT001'in kapattigi kanal) Core'a tasiyamaz.
      Senkronizasyon olayi DEGILDIR: `synchronizes_with` onu saymaz ve
      `olay_konum` none dondurur → veri-yarisi adayi da olamaz. -/
  | dalOl         (t : ThreadId) (alindi : Bool)
  /-- D-338: BOLME olayi — OPERANDLARI tasir (Mehmet karari).
      Gerekce: bolme SABIT CEVRIM DEGILDIR; gecikmesi operandlarin bir
      FONKSIYONUDUR. Saldirganin ogrenebileceginin UST SINIRI operandlarin
      kendisidir, dolayisiyla muhafazakar model onlari gozleme koyar.
      `topla`nin BOYLE bir olayi YOKTUR — cunku toplama sabit cevrimdir;
      fark tam olarak buradadir ve CT006'nin varlik sebebidir. -/
  | bolOl         (t : ThreadId) (a b : Int)
  /-- D-339: MOD olayi. `bolOl`dan AYRI tutuldu — `bolOl` yeniden
      kullanilsaydi `a/b` ile `a%b` AYNI izi uretirdi, yani saldirgandan
      "hangi islem" bilgisi SAKLANMIS olurdu. Bu IYIMSER bir varsayim
      olurdu; muhafazakar model (D-338 ilkesi: gozlem = ust sinir) ayri
      olay ister. Donanimda ayni bolucu birimi kullanilsa bile FARKLI
      komuttur ve izde ayirt edilebilir. -/
  | modOl         (t : ThreadId) (a b : Int)


/-- Iz: gozlemlenmis olaylar (en yenisi basta) -/
abbrev Iz := List Olay


/-- D-336: HUCRE OKUMA — TOPLAM (total) skaler okuma.
    Neden `konumGet` degil de bu: (1) indeksli okuma TAKILMAMALI
    (progress), (2) sonucu DAIMA skaler olmali (korunum: `t_indeks`
    `scalar` der). Kayitli olmayan ya da skaler-olmayan hucre 0 okur.

    DURUSTLUK NOTU: bu, bellegin TOPLAM oldugu bir modeldir — SINIR
    DENETIMI (bounds check) MODELLENMEZ. KEMGU'nun gercek dizi erisimi
    sinir-kontrollu olup ihlalde panikler; burada amac yan-kanal
    (adres sizintisi), bellek-guvenligi degil. CT hesabinin store'u
    (`Ad → Nat → Int`) da toplamdir, yani kopru bu noktada SADIKTIR. -/
def hucreOku (sigma : Store) (k : Konum) : Int :=
  match konumGet sigma k with
  | some (.skaler n) => n
  | _                => 0


-- ============================================================
-- §9. Ifade sozdizimi (DRF icin minimal subset)
-- Kaynak: Op.Sem §1 grammar
-- ============================================================

/-- AST ifade. DRF teoreminin tum uretebildigi reduksiyon noktalarini
    kapsayacak minimal subset. -/
inductive Ifade : Type where
  | tanim          (x : VarId)                              -- degisken referansi
  | sabit          (v : Deger)                              -- literal
  | atama          (x : VarId) (e : Ifade)                  -- x = e
  | seq            (a : Ifade) (b : Ifade)                  -- s1; s2
  | gorevBaslat    (yakalama : List VarId) (kod : Ifade)    -- gorev_baslat
  | gorevBirlestir (g : VarId)                              -- birlestir
  | kanalGonderIf  (k : KanalId) (v : VarId)                -- gonder
  | kanalAlIf      (k : KanalId)                            -- al → deger
  | dondurIf       (b : Bolge)                              -- dondur(v)
  | kullanIf       (x : VarId)                              -- kullan(x) — Linear
  | imhaIf         (x : VarId)                              -- imha(x)   — Linear
  | guvensiz       (e : Ifade)                              -- guvensiz blok
  /-- D-332: dallanma. `eger k d y` — kosul k degerlendirilir (sEgerCong),
      deger sifir-disi ise d, sifir ise y dalina gecilir (sEgerSec) ve
      `dalOl` olayi emit edilir. CT001'in korudugu kanalin ana modeldeki
      temsili budur. -/
  | eger           (kosul dogruDal yanlisDal : Ifade)
  /-- D-334: tamsayi toplama. Degerlendirme SOLDAN SAGA (sToplaCongSol,
      sonra sToplaCongSag), sonra sToplaTamam. Olay URETMEZ — aritmetik
      saldirgan tarafindan dogrudan gozlenmez; sizinti kanali onun
      SONUCUNUN bir kosula/adrese girmesidir, ki onlar zaten gozlenir.
      Tip disiplini `Tip.scalar` (t_topla) — `dt_skaler` scalar'in TEK
      tanitici kurali oldugu icin iyi-tipli deger operandlar zorunlu
      olarak `skaler`dir (progress buna dayanir). -/
  | topla          (sol sag : Ifade)
  /-- D-340: TAMSAYI CARPMASI `a * b`. **`bol`/`kalan` DEGIL, `topla`
      SINIFINDA:** olay URETMEZ, CT operand-genellik sarti YOKTUR, yani
      GIZLI × GIZLI SERBESTTIR.
      Bu bir DONANIM IDDIASIDIR (Mehmet karari) — bkz. SideChannel/CT
      icindeki "CARPMA VARSAYIMI" notu ve D-340. Ozeti: KEMGU'nun hedef
      ISA'larinda (ARM64, x86_64) tamsayi carpmasi sabit cevrimdir; erken
      biten carpicilarda (Cortex-M0/M3 gibi) MODEL GECERSIZDIR.
      Gerekce: aksi halde alan carpimi (gizli × gizli) yasaklanir ve
      HICBIR kripto primitifi CT-tipli yazilamaz. -/
  | carp           (sol sag : Ifade)
  /-- D-335: `iken k g` — kosullu dongu. Semantik ACMA ile verilir
      (sIkenAc): `iken k g ⟶ eger k (seq g (iken k g)) (sabit birim)`.
      Boylece TUR SAYISI her turda bir `dalOl` uretir — yani dongu
      sayisi saldirgana GORUNUR. Bu, CT002'nin (gizli kosullu dongu)
      mekanize edilmis hali icin dogru gozlem modelidir. -/
  | iken           (kosul govde : Ifade)
  /-- D-335: `esles skrut n eslesen kalan` — LITERAL desen eslemesi
      (Mehmet karari: IKILI bicim). `skrut` degeri n'e esitse `eslesen`,
      degilse `kalan` secilir ve bir `dalOl` uretilir. Cok kollu eslesme
      ic ice yazilarak ifade edilir.
      ** KAPSAM: bu, KEMGU `esles`inin LITERAL-DESEN alt kumesidir;
      yapici/cesit desenleri (D-318) KAPSAM DISIDIR (Core'da ADT yok). -/
  | esles          (skrut : Ifade) (n : Int) (eslesen kalan : Ifade)
  /-- D-336: INDEKSLI OKUMA `x[idx]`. `Konum`un `ofset` alani nihayet
      kullaniliyor: okunan konum `⟨bolge(x), idx⟩`dir ve uretilen
      `memOku` olayi O KONUMU tasir → **ERISIM ADRESI GOZLENIR**.
      Gizli-indeks kanali (onbellek-zamanlama; ornek: AES S-box aramasi)
      modelde boylece IFADE EDILEBILIR hale gelir.
      V1 KAPSAM: yalniz OKUMA (indeksli yazma V2 — Mehmet karari). -/
  | indeks         (dizi : VarId) (idx : Ifade)
  /-- D-337: INDEKSLI YAZMA `x[idx] = deger`. Degerlendirme sirasi
      INDEKS SONRA DEGER (CT'nin `c_indeksAta`si ile birebir).
      Uretilen `memYaz` olayi `⟨bolge(x), idx⟩` konumunu tasir →
      **YAZMA ADRESI DE GOZLENIR** (okuma tarafi D-336). -/
  | indeksAta      (dizi : VarId) (idx : Ifade) (deger : Ifade)
  /-- D-338: TAMSAYI BOLMESI `a / b`. `topla` ile ayni degerlendirme
      sirasi (soldan saga) ama `bolOl` olayi uretir — veri-bagimli
      gecikme kanali (CT006).
      V1 KAPSAM: `b = 0` icin sonuc 0 (Lean `Int` bolmesi; TOPLAM).
      KEMGU'nun gercek bolmesi `sonuc<T,H>` dondurur ya da paniklerdir;
      burada amac yan-kanal, hata semantigi DEGIL (bkz. `hucreOku`). -/
  | bol            (sol sag : Ifade)
  /-- D-339: KALAN (mod) `a % b`. `bol` ile ayni sinif: veri-bagimli
      gecikme (ayni bolucu birim) → `modOl` olayi uretir ve CT006-M
      operandlarin GENEL olmasini ister.
      V1 KAPSAM: `b = 0` icin Lean `Int` semantigi (`a % 0 = a`); TOPLAM. -/
  | kalan          (sol sag : Ifade)


/-- seq, sag bilesenine esit olamaz (yapisal buyukluk — odak-degisimi). -/
theorem seq_ne_sag (a b : Ifade) : Ifade.seq a b ≠ b := by
  intro h
  have h_size := congrArg sizeOf h
  simp at h_size

/-- D-332: `eger` secilen dalina esit olamaz (odak GERCEKTEN degisir). -/
theorem eger_ne_dogru (k d y : Ifade) : Ifade.eger k d y ≠ d := by
  intro h
  have h_size := congrArg sizeOf h
  simp at h_size
  omega

theorem eger_ne_yanlis (k d y : Ifade) : Ifade.eger k d y ≠ y := by
  intro h
  have h_size := congrArg sizeOf h
  simp at h_size

/-- D-335: `esles` secilen koluna esit olamaz (odak GERCEKTEN degisir). -/
theorem esles_ne_eslesen (s : Ifade) (n : Int) (d y : Ifade) :
    Ifade.esles s n d y ≠ d := by
  intro h
  have h_size := congrArg sizeOf h
  simp at h_size
  omega

theorem esles_ne_kalan (s : Ifade) (n : Int) (d y : Ifade) :
    Ifade.esles s n d y ≠ y := by
  intro h
  have h_size := congrArg sizeOf h
  simp at h_size


-- ============================================================
-- §9.2. HedefVar / HedefBolge — sahiplik gerektiren hedefler
-- (F4 onayli; Onarim v3 kapanis: Tipli.lean'den BURAYA tasindi —
--  r_gorev_baslat'in Yol-B hedefsiz-govde premise'i bu predikatlari
--  RegionTamam katmaninda gorebilmeli.)
-- ============================================================

/-- `HedefVar e y`: e'nin govdesinde (gorevBaslat ic-govdeleri HARIC —
    onlar cocuk thread'in hedefleri) y degiskenini hedefleyen sahiplik
    gerektiren bir operasyon var: atama hedefi, kanala gonderim,
    gorev yakalamasi. seq'in HER IKI kolu kapsanir (sSeqAtla'da saga
    gecis invariant'i bozmasin). -/
inductive HedefVar : Ifade → VarId → Prop where
  | atama_bas (y : VarId) (e : Ifade) :
      HedefVar (Ifade.atama y e) y
  | kanal_gonder (k : KanalId) (y : VarId) :
      HedefVar (Ifade.kanalGonderIf k y) y
  | gorev_yakala (yd : List VarId) (kod : Ifade) (y : VarId) :
      y ∈ yd → HedefVar (Ifade.gorevBaslat yd kod) y
  | seq_sol (a b : Ifade) (y : VarId) :
      HedefVar a y → HedefVar (Ifade.seq a b) y
  | seq_sag (a b : Ifade) (y : VarId) :
      HedefVar b y → HedefVar (Ifade.seq a b) y
  | atama_ic (x : VarId) (e : Ifade) (y : VarId) :
      HedefVar e y → HedefVar (Ifade.atama x e) y
  | guvensiz_ic (e : Ifade) (y : VarId) :
      HedefVar e y → HedefVar (Ifade.guvensiz e) y
  -- D-332: eger'in UC alt-ifadesi de kapsanir. Kosul sEgerCong ile,
  -- dallar sEgerSec ile odaga girer; invariant her ucunde korunmali.
  | eger_kosul (k d y : Ifade) (z : VarId) :
      HedefVar k z → HedefVar (Ifade.eger k d y) z
  | eger_dogru (k d y : Ifade) (z : VarId) :
      HedefVar d z → HedefVar (Ifade.eger k d y) z
  | eger_yanlis (k d y : Ifade) (z : VarId) :
      HedefVar y z → HedefVar (Ifade.eger k d y) z
  -- D-334: topla'nin her iki operandi da odaga girer.
  | topla_sol (a b : Ifade) (z : VarId) :
      HedefVar a z → HedefVar (Ifade.topla a b) z
  | topla_sag (a b : Ifade) (z : VarId) :
      HedefVar b z → HedefVar (Ifade.topla a b) z
  | carp_sol (a b : Ifade) (z : VarId) :
      HedefVar a z → HedefVar (Ifade.carp a b) z
  | carp_sag (a b : Ifade) (z : VarId) :
      HedefVar b z → HedefVar (Ifade.carp a b) z
  -- D-338
  | bol_sol (a b : Ifade) (z : VarId) :
      HedefVar a z → HedefVar (Ifade.bol a b) z
  | bol_sag (a b : Ifade) (z : VarId) :
      HedefVar b z → HedefVar (Ifade.bol a b) z
  -- D-339
  | kalan_sol (a b : Ifade) (z : VarId) :
      HedefVar a z → HedefVar (Ifade.kalan a b) z
  | kalan_sag (a b : Ifade) (z : VarId) :
      HedefVar b z → HedefVar (Ifade.kalan a b) z
  -- D-335
  | iken_kosul (k g : Ifade) (z : VarId) :
      HedefVar k z → HedefVar (Ifade.iken k g) z
  | iken_govde (k g : Ifade) (z : VarId) :
      HedefVar g z → HedefVar (Ifade.iken k g) z
  | esles_skrut (s : Ifade) (n : Int) (d y : Ifade) (z : VarId) :
      HedefVar s z → HedefVar (Ifade.esles s n d y) z
  | esles_eslesen (s : Ifade) (n : Int) (d y : Ifade) (z : VarId) :
      HedefVar d z → HedefVar (Ifade.esles s n d y) z
  | esles_kalan (s : Ifade) (n : Int) (d y : Ifade) (z : VarId) :
      HedefVar y z → HedefVar (Ifade.esles s n d y) z
  -- D-336: indeksli OKUMA sahiplik gerektirmez (sVarOku gibi); yalniz
  -- INDEKS IFADESI hedef tasiyabilir.
  | indeks_ic (x : VarId) (idx : Ifade) (z : VarId) :
      HedefVar idx z → HedefVar (Ifade.indeks x idx) z
  -- D-337: indeksli YAZMA sahiplik GEREKTIRIR → dizi degiskeni HEDEFTIR.
  | indeksAta_bas (x : VarId) (idx e : Ifade) :
      HedefVar (Ifade.indeksAta x idx e) x
  | indeksAta_idx (x : VarId) (idx e : Ifade) (z : VarId) :
      HedefVar idx z → HedefVar (Ifade.indeksAta x idx e) z
  | indeksAta_deg (x : VarId) (idx e : Ifade) (z : VarId) :
      HedefVar e z → HedefVar (Ifade.indeksAta x idx e) z

/-- `HedefBolge e b`: e'nin govdesinde b bolge-literalini donduran bir
    dondurIf var (dondur sahiplik gerektirir — h_owner). -/
inductive HedefBolge : Ifade → Bolge → Prop where
  | dondur_bas (b : Bolge) :
      HedefBolge (Ifade.dondurIf b) b
  | seq_sol (a c : Ifade) (b : Bolge) :
      HedefBolge a b → HedefBolge (Ifade.seq a c) b
  | seq_sag (a c : Ifade) (b : Bolge) :
      HedefBolge c b → HedefBolge (Ifade.seq a c) b
  | atama_ic (x : VarId) (e : Ifade) (b : Bolge) :
      HedefBolge e b → HedefBolge (Ifade.atama x e) b
  | guvensiz_ic (e : Ifade) (b : Bolge) :
      HedefBolge e b → HedefBolge (Ifade.guvensiz e) b
  -- D-332: eger'in uc alt-ifadesi (bkz. HedefVar aciklamasi).
  | eger_kosul (k d y : Ifade) (b : Bolge) :
      HedefBolge k b → HedefBolge (Ifade.eger k d y) b
  | eger_dogru (k d y : Ifade) (b : Bolge) :
      HedefBolge d b → HedefBolge (Ifade.eger k d y) b
  | eger_yanlis (k d y : Ifade) (b : Bolge) :
      HedefBolge y b → HedefBolge (Ifade.eger k d y) b
  -- D-334
  | topla_sol (a c : Ifade) (b : Bolge) :
      HedefBolge a b → HedefBolge (Ifade.topla a c) b
  | topla_sag (a c : Ifade) (b : Bolge) :
      HedefBolge c b → HedefBolge (Ifade.topla a c) b
  | carp_sol (a c : Ifade) (b : Bolge) :
      HedefBolge a b → HedefBolge (Ifade.carp a c) b
  | carp_sag (a c : Ifade) (b : Bolge) :
      HedefBolge c b → HedefBolge (Ifade.carp a c) b
  -- D-338
  | bol_sol (a c : Ifade) (b : Bolge) :
      HedefBolge a b → HedefBolge (Ifade.bol a c) b
  | bol_sag (a c : Ifade) (b : Bolge) :
      HedefBolge c b → HedefBolge (Ifade.bol a c) b
  -- D-339
  | kalan_sol (a c : Ifade) (b : Bolge) :
      HedefBolge a b → HedefBolge (Ifade.kalan a c) b
  | kalan_sag (a c : Ifade) (b : Bolge) :
      HedefBolge c b → HedefBolge (Ifade.kalan a c) b
  -- D-335
  | iken_kosul (k g : Ifade) (b : Bolge) :
      HedefBolge k b → HedefBolge (Ifade.iken k g) b
  | iken_govde (k g : Ifade) (b : Bolge) :
      HedefBolge g b → HedefBolge (Ifade.iken k g) b
  | esles_skrut (s : Ifade) (n : Int) (d y : Ifade) (b : Bolge) :
      HedefBolge s b → HedefBolge (Ifade.esles s n d y) b
  | esles_eslesen (s : Ifade) (n : Int) (d y : Ifade) (b : Bolge) :
      HedefBolge d b → HedefBolge (Ifade.esles s n d y) b
  | esles_kalan (s : Ifade) (n : Int) (d y : Ifade) (b : Bolge) :
      HedefBolge y b → HedefBolge (Ifade.esles s n d y) b
  -- D-336
  | indeks_ic (x : VarId) (idx : Ifade) (b : Bolge) :
      HedefBolge idx b → HedefBolge (Ifade.indeks x idx) b
  -- D-337
  | indeksAta_idx (x : VarId) (idx e : Ifade) (b : Bolge) :
      HedefBolge idx b → HedefBolge (Ifade.indeksAta x idx e) b
  | indeksAta_deg (x : VarId) (idx e : Ifade) (b : Bolge) :
      HedefBolge e b → HedefBolge (Ifade.indeksAta x idx e) b

/-- Hedef tersine-cevirme yardimcilari (cong odak-yuku ayristirmasi). -/
theorem hedefVar_seq_inv {a b : Ifade} {y : VarId}
    (h : HedefVar (Ifade.seq a b) y) : HedefVar a y ∨ HedefVar b y := by
  cases h with
  | seq_sol _ _ _ h => exact Or.inl h
  | seq_sag _ _ _ h => exact Or.inr h

theorem hedefBolge_seq_inv {a b : Ifade} {bb : Bolge}
    (h : HedefBolge (Ifade.seq a b) bb) :
    HedefBolge a bb ∨ HedefBolge b bb := by
  cases h with
  | seq_sol _ _ _ h => exact Or.inl h
  | seq_sag _ _ _ h => exact Or.inr h

theorem hedefVar_atama_inv {x : VarId} {e : Ifade} {y : VarId}
    (h : HedefVar (Ifade.atama x e) y) : y = x ∨ HedefVar e y := by
  cases h with
  | atama_bas _ _ => exact Or.inl rfl
  | atama_ic _ _ _ h => exact Or.inr h

theorem hedefBolge_atama_inv {x : VarId} {e : Ifade} {bb : Bolge}
    (h : HedefBolge (Ifade.atama x e) bb) : HedefBolge e bb := by
  cases h with
  | atama_ic _ _ _ h => exact h

/-- D-335: ACILMIS dongunun hedefleri, ORIJINAL dongunun hedeflerine
    geri esler. `sIkenAc` sonrasi HedefVar/HedefBolge invaryantlarinin
    korunmasini saglayan lemma — OZYINELEME YOK, cunku `seq`in sag kolu
    zaten `iken k g`nin kendisidir. -/
theorem hedefVar_iken_ac {k g : Ifade} {z : VarId}
    (h : HedefVar (Ifade.eger k (Ifade.seq g (Ifade.iken k g))
                     (Ifade.sabit Deger.birim)) z) :
    HedefVar (Ifade.iken k g) z := by
  cases h with
  | eger_kosul _ _ _ _ h' => exact HedefVar.iken_kosul k g z h'
  | eger_dogru _ _ _ _ h' =>
      rcases hedefVar_seq_inv h' with hg | hi
      · exact HedefVar.iken_govde k g z hg
      · exact hi
  | eger_yanlis _ _ _ _ h' => nomatch h'

theorem hedefBolge_iken_ac {k g : Ifade} {b : Bolge}
    (h : HedefBolge (Ifade.eger k (Ifade.seq g (Ifade.iken k g))
                       (Ifade.sabit Deger.birim)) b) :
    HedefBolge (Ifade.iken k g) b := by
  cases h with
  | eger_kosul _ _ _ _ h' => exact HedefBolge.iken_kosul k g b h'
  | eger_dogru _ _ _ _ h' =>
      rcases hedefBolge_seq_inv h' with hg | hi
      · exact HedefBolge.iken_govde k g b hg
      · exact hi
  | eger_yanlis _ _ _ _ h' => nomatch h'

theorem hedefVar_guvensiz_inv {e : Ifade} {y : VarId}
    (h : HedefVar (Ifade.guvensiz e) y) : HedefVar e y := by
  cases h with
  | guvensiz_ic _ _ h => exact h

theorem hedefBolge_guvensiz_inv {e : Ifade} {bb : Bolge}
    (h : HedefBolge (Ifade.guvensiz e) bb) : HedefBolge e bb := by
  cases h with
  | guvensiz_ic _ _ h => exact h


-- ============================================================
-- §9.5. FaultSebep — Plan v2 Adim 1.1 (Onarim Plani §4.2)
-- Konfigurasyon.fault icin sebep kategorileri. Tum sebepler V1 SC
-- altinda gozlenebilir runtime hatalari (frozen yazma, sahip olmadan
-- yazma, lineer cifte tuketim).
--
-- NOT (Adim 1.1, 2026-05-18): Bu enum ve Konfigurasyon.fault alani
-- eklendi. Mevcut Step constructor'lari (sAtama, cGorevBaslat, ...)
-- henuz S'.fault'i KISITLAMIYOR — Adim 1.2 + 1.3'te dual constructor
-- (sAtamaTamam + sAtamaHataDonmus + sAtamaHataSahipDegil, vs.) ile
-- semantik tamamlanacak. Adim 7'de discharge lemmalari fault case'leri
-- typed program icin imkansiz olarak gosterecek.
-- Kaynak: belgeler/KEMGU_Mekanize_Onarim_Plan.md §4.2 + §4.4.
-- ============================================================

inductive FaultSebep : Type where
  | donmusYazma             (b : Bolge)                  -- sAtama: frozen bolgeye yazma
  | sahipDegil              (b : Bolge) (t : ThreadId)   -- sAtama: ctx sahip degil
  | lineerYakalananZatenTuketildi (v : VarId)            -- cGorevBaslat: yakalanan lineer zaten tuketilmis (use-after-move; Adim 8 V2 P6)
  | lineerZatenTuketildi    (v : VarId)                  -- sLinKullan/sLinImha: ikinci consume
  | lineerKanalTuket        (v : VarId)                  -- cKanalGonder: linear v cifte gonderim
  | zatenDonmus             (b : Bolge)                  -- cDondur: zaten frozen bolge
deriving Repr, DecidableEq


-- ============================================================
-- §10. Thread baglami + Kanal durumu + Konfigurasyon
-- Kaynak: Op.Sem §5.1, §5.2
-- ============================================================

/-- Tek thread baglami (Op.Sem §5.1).
    Pho_t (bolge ortami) iceride ileride ekleniyor — V1'de implicit. -/
structure ThreadCtx where
  tid     : ThreadId
  ifade   : Ifade
  lineer  : LineerOrtam

/-- Kanal durumu — bekleyen mesaj kuyruklari -/
structure KanalDurumu where
  kid           : KanalId
  gonderKuyrugu : List Deger

/-- Kanala mesaj ekle (FIFO sona). Kanal kaydi yoksa oluştur — total fonksiyon,
    cKanalGonderTamam varlik guard'ina ihtiyac duymaz (F2). -/
def kanalEkle (ks : List KanalDurumu) (k : KanalId) (v : Deger) : List KanalDurumu :=
  if ks.any (fun kd => kd.kid = k)
    then ks.map (fun kd =>
           if kd.kid = k then { kd with gonderKuyrugu := kd.gonderKuyrugu ++ [v] } else kd)
    else ⟨k, [v]⟩ :: ks

/-- Kanalin ilk bekleyen mesaji (FIFO bas). -/
def kanalIlk (ks : List KanalDurumu) (k : KanalId) : Option Deger :=
  match ks.find? (fun kd => kd.kid = k) with
  | some kd => kd.gonderKuyrugu.head?
  | none => none

/-- Kanalin ilk mesajini cikar (FIFO pop — cKanalAlTamam). -/
def kanalCikar (ks : List KanalDurumu) (k : KanalId) : List KanalDurumu :=
  ks.map (fun kd =>
    if kd.kid = k then { kd with gonderKuyrugu := kd.gonderKuyrugu.tail } else kd)

/-- kanalEkle uyelik analizi: yeni listede gorulen kayit ya append'li
    eski k-kaydi, ya k-disi eski kayit, ya da taze ⟨k,[v]⟩ kaydidir
    (dal taniklari any-bayragini tasir — birbirini dislar). -/
theorem kanalEkle_uye (ks : List KanalDurumu) (k : KanalId) (v : Deger)
    (kd' : KanalDurumu) (h : kd' ∈ kanalEkle ks k v) :
    (∃ kd0 ∈ ks, kd0.kid = k
       ∧ kd' = { kd0 with gonderKuyrugu := kd0.gonderKuyrugu ++ [v] }
       ∧ ks.any (fun kd => kd.kid = k) = true)
    ∨ (kd' ∈ ks ∧ kd'.kid ≠ k)
    ∨ (kd' = ⟨k, [v]⟩ ∧ ks.any (fun kd => kd.kid = k) = false) := by
  unfold kanalEkle at h
  by_cases h_any : ks.any (fun kd => kd.kid = k) = true
  · rw [if_pos h_any] at h
    rcases List.mem_map.mp h with ⟨kd0, h0, h_img⟩
    by_cases hk : kd0.kid = k
    · rw [if_pos hk] at h_img
      exact Or.inl ⟨kd0, h0, hk, h_img.symm, h_any⟩
    · rw [if_neg hk] at h_img
      subst h_img
      exact Or.inr (Or.inl ⟨h0, hk⟩)
  · rw [if_neg h_any] at h
    have h_any' : ks.any (fun kd => kd.kid = k) = false :=
      Bool.eq_false_iff.mpr h_any
    rcases List.mem_cons.mp h with he | h_tail
    · exact Or.inr (Or.inr ⟨he, h_any'⟩)
    · refine Or.inr (Or.inl ⟨h_tail, ?_⟩)
      intro hk
      exact h_any (List.any_eq_true.mpr ⟨kd', h_tail, by simp [hk]⟩)

/-- Tum sistem konfigurasyonu (Op.Sem §5.2).
    S = ⟨T_, sigma, Sigma, K_⟩ + zaman + iz + fault.
    `fault` alani Plan v2 Adim 1.1'de eklendi: `none` = normal yurutme;
    `some sebep` = fault state'e gecilmis. Mevcut Step constructor'lari
    (Adim 1.2 oncesi) S'.fault'i KISITLAMIYOR — default `none` sayesinde
    geriye uyumlu. Adim 1.2'de dual (Tamam + Hata*) constructor'lar ile
    her gecisin fault semantigi netlestirilecek.
    Kaynak: belgeler/KEMGU_Mekanize_Onarim_Plan.md §4.4. -/
structure Konfigurasyon where
  thread      : List ThreadCtx
  store       : Store
  sahiplik    : Sahiplik
  kanal       : List KanalDurumu
  zaman       : Zaman
  iz          : Iz
  fault       : Option FaultSebep := none
  -- Plan v2 Adim 8 V2 (Ρ→Konfigurasyon refactor): runtime bolge ortami.
  -- Tip `List (VarId × Bolge)` = StateTipli.BolgeOrtam (Core'da abbrev import
  -- edilemez — defeq). KonfTipliFull `S.bolge = Ρ` ile statik Ρ'ya baglar.
  -- Step kurallari (sAtama/cDondur) `k.bolge`/`b`'yi `bolgeOrtamGet S.bolge x`
  -- ile tiplenmis ifadeye baglar → Aile 2 region discharge'lari acilir.
  -- Default `[]`: mevcut config'ler (hicbiri pozisyonel insa edilmiyor) etkilenmez.
  bolge       : List (VarId × Bolge) := []


-- ============================================================
-- §10.1. isFrozen predicate (DRF-L4 icin gerekli, A3.0'' refactor)
-- ============================================================

/-- Bir bolge S anindaki "frozen" durumu (F2 guncel-durum modeli):
    bolgenin guncel sahibi DONMUS ise.

    Eski ∃z₀≤S.zaman formu zaman-anahtarli Sahiplik'in kalintisiydi;
    guncel-durum modelinde dogrudan lookup yeterli. Persistence artik
    kural-tasarimindan gelir: sahiplik yazan her Tamam kurali hedef
    bolgenin guncel sahibinin bir thread/kanal olmasini sart kosar
    (donmus bolge transfer edilemez) → donmus entry asla override
    edilmez. Bkz. L4 isFrozen_persistent (F2'de KOSULSUZ ispatli). -/
def isFrozen (S : Konfigurasyon) (b : Bolge) : Prop :=
  sahiplikGet S.sahiplik b = some Sahip.donmus


-- ============================================================
-- §11. Program + IyiTipli predicate (Op.Sem §7)
-- ============================================================

/-- Program: ust duzey tanim listesi (Op.Sem §1).
    F3 zenginlestirme: `cevre` (ust-duzey degisken bildirimleri — eski model
    degisken bildirimi icermiyordu, gercek TipKontrolOk icin sart) ve
    `kanalCevre` (kanal eleman tipleri Δ). Default'lar geriye uyumlu. -/
structure Program where
  islevler   : List (String × Ifade)
  cevre      : List (VarId × Tip) := []
  kanalCevre : KanalId → Tip := fun _ => Tip.bos

/-- Bir ifade gövdesinde guvensiz blok var mi (recursive). -/
def ifadeGuvensizIcerirMi : Ifade → Bool
  | .guvensiz _ => true
  | .seq a b => ifadeGuvensizIcerirMi a || ifadeGuvensizIcerirMi b
  | .atama _ e => ifadeGuvensizIcerirMi e
  | .gorevBaslat _ k => ifadeGuvensizIcerirMi k
  | _ => false

/-- Bir program guvensiz blok iceriyor mu? -/
def programGuvensizIcerir (Pi : Program) : Bool :=
  Pi.islevler.any (fun p => ifadeGuvensizIcerirMi p.snd)

/-- "Program guvensiz blok icermez" — Op.Sem §7 kosul 7.
    Bu V1'de etkin (Ifade sozdizimi guvensiz constructor'i icerir). -/
def NoGuvensiz (Pi : Program) : Prop :=
  programGuvensizIcerir Pi = false

-- F3 NOT (ADIM 0 Sorun 1 onarimi): eski placeholder'lar
-- (TipKontrolOk/LineerKontrolOk/CapabilityKontrolOk/SabitsureKontrolOk/
--  BolgeAtamaOk/RealtimeKontrolOk := True) ve vakum IyiTipli SILINDI.
-- GERCEK tanimlar Kemgu/Sem/Kopru.lean'de: TipKontrolOk HasType'a,
-- LineerKontrolOk LineerTamam'a, BolgeAtamaOk RegionTamam'a baglanir;
-- Capability/Sabitsure sozdizimsel scope-guard; Realtime alani KALDIRILDI
-- (V1 Ifade'de realtime yapisi yok — guard bile vakum olurdu; Mehmet
-- onayi 2026-06-11). NoGuvensiz (sozdizimsel, gercek) burada kalir.


end Kemgu.Sem.Core
