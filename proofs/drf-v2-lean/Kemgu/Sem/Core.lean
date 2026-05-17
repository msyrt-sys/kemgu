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
  | scalar     : Tip                       -- skaler kategori (tam_w/dtam_w/...)
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

/-- Bir bolgenin sahip durumu. ⊥ = bos, donmus = R-PAYLAS. -/
inductive Sahip : Type where
  | bos                              -- ⊥ (henuz sahibi yok)
  | thread (t : ThreadId)            -- belirli thread sahip
  | donmus                           -- DONMUS (R-PAYLAS, coklu okuyucu)
deriving Repr, DecidableEq

/-- Sigma : (Bolge × Zaman) → Sahip -/
abbrev Sahiplik := List ((Bolge × Zaman) × Sahip)


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
    S = ⟨T_, sigma, Sigma, K_⟩ + zaman + iz. -/
structure Konfigurasyon where
  thread      : List ThreadCtx
  store       : Store
  sahiplik    : Sahiplik
  kanal       : List KanalDurumu
  zaman       : Zaman
  iz          : Iz


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
