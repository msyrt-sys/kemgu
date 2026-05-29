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
  | birim                                              -- () bos tip
end


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

/-- Sigma : (Bolge × Zaman) → Sahip -/
abbrev Sahiplik := List ((Bolge × Zaman) × Sahip)


-- ============================================================
-- §7.1. Sahiplik lookup + atomic set + temel lemma'lar
-- DRF-L0 (Bolge Korunumu) ve diger lemmalar bu helper'lara dayanir.
-- Kaynak: Op.Sem §5.4 R-* aksiyomlarinin atomic transfer semantigi
-- ============================================================

/-- Sahiplik lookup: bir (bolge, zaman) anahtarina karsi gelen sahip degerini
    bul. Birinci eslesen entry'i dondurur (newest-wins; sahiplikSet prepend
    kullanir, bu sayede yeni entry eskisini "shadow"lar). -/
def sahiplikGet : Sahiplik → (Bolge × Zaman) → Option Sahip
  | [], _ => none
  | (k, v) :: rest, key => if k = key then some v else sahiplikGet rest key

/-- Sahiplik atomik set (Op.Sem §5.4 R-* aksiyomlari S3 atomic transfer):
    (bolge, zaman) anahtarina yeni sahip degerini ata.
    Implementasyon: prepend; sahiplikGet ilk eslesen entry'i dondurdugu icin
    yeni deger eski entry'i mantiksal olarak override eder (eski entry fiziksel
    olarak listede kalir ama lookup ona ulasmaz). Bu hem bellek hem ispat
    sadeligi acisindan tercih edilen tasarim. -/
def sahiplikSet (s : Sahiplik) (b : Bolge) (t : Zaman) (yeni : Sahip) : Sahiplik :=
  ((b, t), yeni) :: s

/-- Coklu bolge atomic set: foldl ile sirayla prepend.
    cGorevBaslat (R-GOREV) ve cGorevBirlestir (R-BIRLESTIR) icin —
    her iki kural birden cok bolgenin sahipligini ayni zaman damgasinda
    degistirir. -/
def sahiplikSetMany (s : Sahiplik) (bs : List Bolge) (t : Zaman) (yeni : Sahip) : Sahiplik :=
  bs.foldl (fun acc b => sahiplikSet acc b t yeni) s

/-- Set sonra get ayni anahtarda yeni degeri doner. -/
theorem sahiplikSet_eq (s : Sahiplik) (b : Bolge) (t : Zaman) (yeni : Sahip) :
    sahiplikGet (sahiplikSet s b t yeni) (b, t) = some yeni := by
  simp [sahiplikSet, sahiplikGet]

/-- Set sonra get farkli anahtarda eski lookup degismez. -/
theorem sahiplikSet_ne (s : Sahiplik) (b b' : Bolge) (t t' : Zaman) (yeni : Sahip)
    (h : (b', t') ≠ (b, t)) :
    sahiplikGet (sahiplikSet s b t yeni) (b', t') = sahiplikGet s (b', t') := by
  simp [sahiplikSet, sahiplikGet, h.symm]


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


/-- Iz: gozlemlenmis olaylar (en yenisi basta) -/
abbrev Iz := List Olay


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

/-- Bir bolge S anindaki "frozen" durumu: gecmiste z₀ ≤ S.zaman icin
    sahiplikGet (b, z₀) = Sahip.donmus var ise.

    Kullanim: sAtama precondition (`h_not_frozen : ¬ isFrozen S k.bolge`)
    DRF-L4 (a) "no frozen writes" ispati icin gerekli.

    Niye `∃ z₀ ≤ S.zaman`? cDondur(b) cagrildiktan sonra b kalici olarak
    frozen olur — ama bizim time-stamped Sahiplik modelinde freeze entry
    spesifik bir zaman damgasinda (cDondur'un calistigi anda). Sonraki
    zamanlarda lookup (b, S.zaman') eski freeze entry'i bulamaz (farkli
    key). Bu yuzden "frozen" durumu "gecmis bir zamanda freeze entry var"
    olarak tanimlanir. Persistence asagidaki teoremlerle gosterilir. -/
def isFrozen (S : Konfigurasyon) (b : Bolge) : Prop :=
  ∃ z₀, z₀ ≤ S.zaman ∧ sahiplikGet S.sahiplik (b, z₀) = some Sahip.donmus


-- ============================================================
-- §11. Program + IyiTipli predicate (Op.Sem §7)
-- ============================================================

/-- Program: ust duzey tanim listesi (Op.Sem §1) -/
structure Program where
  islevler : List (String × Ifade)

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

/-- Tip kontrolu gecer (Op.Sem §7 kosul 1).
    V1 mekanize: placeholder True. V2'de tam tip sistemi inductive olarak
    eklenir; bu predicate o zaman gerçek tip turetilebilirligine baglanir. -/
def TipKontrolOk (_Pi : Program) : Prop := True

/-- Lineer kontrol gecer (Op.Sem §7 kosul 2). V1 placeholder. -/
def LineerKontrolOk (_Pi : Program) : Prop := True

/-- Capability kontrol gecer (Op.Sem §7 kosul 3). V1 placeholder. -/
def CapabilityKontrolOk (_Pi : Program) : Prop := True

/-- Sabitsure kontrol gecer (Op.Sem §7 kosul 4). V1 placeholder. -/
def SabitsureKontrolOk (_Pi : Program) : Prop := True

/-- Bolge atama gecer (Op.Sem §7 kosul 5). V1 placeholder. -/
def BolgeAtamaOk (_Pi : Program) : Prop := True

/-- Realtime kontrol gecer (Op.Sem §7 kosul 6). V1 placeholder. -/
def RealtimeKontrolOk (_Pi : Program) : Prop := True

/-- IyiTipli(Pi) — Op.Sem §7'nin yedi kosulu birlikte.
    Bu structure DRF lemmalarinin uniform hipotezi olarak kullanilir.
    Sub-predicate'lerin placeholder True olmasi: V1 sinir, V2'de
    tip sistemi mechanize edilince bu alanlarin ispatlari programdan
    turetilebilir; simdi sadece structure tasiyici. -/
structure IyiTipli (Pi : Program) : Prop where
  tipOk           : TipKontrolOk Pi
  lineerOk        : LineerKontrolOk Pi
  capabilityOk    : CapabilityKontrolOk Pi
  sabitsureOk     : SabitsureKontrolOk Pi
  bolgeOk         : BolgeAtamaOk Pi
  realtimeOk      : RealtimeKontrolOk Pi
  noGuvensiz      : NoGuvensiz Pi


end Kemgu.Sem.Core
