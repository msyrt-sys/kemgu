/-
KEMGU DRF Mekanize — Minimal HasType (Onarim v3 F3)
Kaynak (kagit formel): belgeler/KEMGU_Mekanize_Onarim_Plan.md §3.2
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz, sorry/axiom YOK

F3 guncellemeleri:
- KANAL TIPI ORTAMI Δ : KanalOrtam eklendi (t_kanal_al vakumunun kapanisi:
  alinan degerin tipi artik Δ k — "her τ" serbestligi kalkti; t_kanal_gonder
  de gonderilen degiskenin tipini Δ k'ye baglar — kanal disiplini).
- F2 V1 daraltmalari korunur: t_gorev_birlestir/t_kullan : bos.
-/

import Kemgu.Sem.Core
import Kemgu.Sem.StateTipli

namespace Kemgu.Sem.HasType
open Kemgu.Sem.Core Kemgu.Sem.StateTipli

-- ============================================================
-- §1. HasType — Klasik tip sistemi (12 Ifade kurali, Δ'li form)
-- ============================================================

/-- Klasik tip judgment Γ; Δ ⊢ e : τ.
    Γ degisken tipleri, Δ kanal eleman tipleri. Lineerlik (Λ) ve bolge (Ρ)
    bilgisi ayri katmanlarda (LineerTamam / RegionTamam). -/
inductive HasType : TipOrtam → KanalOrtam → Ifade → Tip → Prop where

  /-- T-TANIM: degisken referansi tipini Γ'dan alir. -/
  | t_tanim   (Γ : TipOrtam) (Δ : KanalOrtam) (x : VarId) (τ : Tip) :
                tipOrtamGet Γ x = some τ →
                HasType Γ Δ (Ifade.tanim x) τ

  /-- T-SABIT: literal tipi DegerTipli'den. -/
  | t_sabit   (Γ : TipOrtam) (Δ : KanalOrtam) (v : Deger) (τ : Tip) :
                DegerTipli Γ [] v τ →
                HasType Γ Δ (Ifade.sabit v) τ

  /-- T-ATAMA: x := e tip-uyumlu ise sonuc bos (no subtyping). -/
  | t_atama   (Γ : TipOrtam) (Δ : KanalOrtam) (x : VarId) (e : Ifade) (τ : Tip) :
                tipOrtamGet Γ x = some τ →
                HasType Γ Δ e τ →
                HasType Γ Δ (Ifade.atama x e) Tip.bos

  /-- T-SEQ: a; b dizisinin tipi son ifadenin tipi. -/
  | t_seq     (Γ : TipOrtam) (Δ : KanalOrtam) (a b : Ifade) (τa τb : Tip) :
                HasType Γ Δ a τa →
                HasType Γ Δ b τb →
                HasType Γ Δ (Ifade.seq a b) τb

  /-- T-GOREV-BASLAT: gorev_baslat(yd, kod) : gorev<τd>. -/
  | t_gorev_baslat (Γ : TipOrtam) (Δ : KanalOrtam)
                   (yd : List VarId) (kod : Ifade) (τd : Tip) :
                    HasType Γ Δ kod τd →
                    HasType Γ Δ (Ifade.gorevBaslat yd kod) (Tip.gorev τd)

  /-- T-GOREV-BIRLESTIR: V1 daraltma (F2) — sonuc bos (join sync-only). -/
  | t_gorev_birlestir (Γ : TipOrtam) (Δ : KanalOrtam) (g : VarId) (τ : Tip) :
                       tipOrtamGet Γ g = some (Tip.gorev τ) →
                       HasType Γ Δ (Ifade.gorevBirlestir g) Tip.bos

  /-- T-KANAL-GONDER (F3 disiplin): gonderilen degiskenin tipi kanalin
      eleman tipi Δ k olmali. -/
  | t_kanal_gonder (Γ : TipOrtam) (Δ : KanalOrtam) (k : KanalId) (v : VarId) :
                    tipOrtamGet Γ v = some (Δ k) →
                    HasType Γ Δ (Ifade.kanalGonderIf k v) Tip.bos

  /-- T-KANAL-AL (F3 — vakum kapandi): alinan degerin tipi Δ k. -/
  | t_kanal_al  (Γ : TipOrtam) (Δ : KanalOrtam) (k : KanalId) :
                  HasType Γ Δ (Ifade.kanalAlIf k) (Δ k)

  /-- T-DONDUR: dondur(b) : bos. -/
  | t_dondur  (Γ : TipOrtam) (Δ : KanalOrtam) (b : Bolge) :
                HasType Γ Δ (Ifade.dondurIf b) Tip.bos

  /-- T-KULLAN: V1 daraltma (F2) — consume effect-only, sonuc bos. -/
  | t_kullan  (Γ : TipOrtam) (Δ : KanalOrtam) (x : VarId) (τ : Tip) :
                tipOrtamGet Γ x = some (Tip.tekkez τ) →
                HasType Γ Δ (Ifade.kullanIf x) Tip.bos

  /-- T-IMHA: tekkez<τ> imha, sonuc bos. -/
  | t_imha    (Γ : TipOrtam) (Δ : KanalOrtam) (x : VarId) (τ : Tip) :
                tipOrtamGet Γ x = some (Tip.tekkez τ) →
                HasType Γ Δ (Ifade.imhaIf x) Tip.bos

  /-- T-EGER (D-332): `eger k d y` — kosul HERHANGI bir tipte olabilir
      (KEMGU'da `degerDogruMu` tum degerler icin tanimli), iki dal AYNI
      tipte olmalidir; sonuc o tiptir.
      NOT: gizlilik/CT disiplini BU katmanda DEGIL — o SideChannel/CT'nin
      `CtOk` yargisidir; klasik tipleme onu bilmez (katman ayrimi). -/
  | t_eger    (Γ : TipOrtam) (Δ : KanalOrtam) (k d y : Ifade) (τk τ : Tip) :
                HasType Γ Δ k τk →
                HasType Γ Δ d τ →
                HasType Γ Δ y τ →
                HasType Γ Δ (Ifade.eger k d y) τ

  /-- T-TOPLA (D-334): iki operand da `scalar`; sonuc `scalar`.
      KRITIK: `DegerTipli`de `Tip.scalar` uretebilen TEK kural
      `dt_skaler`dir → iyi-tipli bir DEGER operand zorunlu olarak
      `Deger.skaler`dir. `progress_konf`in `topla` kolu bu tekilligi
      kullanir (aksi halde `topla (sabit "x") ...` STUCK olurdu). -/
  | t_topla   (Γ : TipOrtam) (Δ : KanalOrtam) (a b : Ifade) :
                HasType Γ Δ a Tip.scalar →
                HasType Γ Δ b Tip.scalar →
                HasType Γ Δ (Ifade.topla a b) Tip.scalar

  /-- T-IKEN (D-335): kosul `scalar`, govde herhangi bir tipte; dongunun
      kendisi `bos`. (Acilma sonrasi `eger`in iki dali da `bos` olur:
      `seq g (iken k g)` : bos ve `sabit birim` : bos.) -/
  | t_iken    (Γ : TipOrtam) (Δ : KanalOrtam) (k g : Ifade) (τg : Tip) :
                HasType Γ Δ k Tip.scalar →
                HasType Γ Δ g τg →
                HasType Γ Δ (Ifade.iken k g) Tip.bos

  /-- T-ESLES (D-335): skrutine `scalar` (literal ile karsilastirilir),
      iki kol AYNI tipte; sonuc o tip. `t_eger` ile ayni sekil. -/
  | t_esles   (Γ : TipOrtam) (Δ : KanalOrtam) (s : Ifade) (n : Int)
              (d y : Ifade) (τ : Tip) :
                HasType Γ Δ s Tip.scalar →
                HasType Γ Δ d τ →
                HasType Γ Δ y τ →
                HasType Γ Δ (Ifade.esles s n d y) τ

  /-- T-INDEKS (D-336): `x[idx]` — x kayitli (bolgesi bulunabilsin diye:
      progress `bolgeOrtamGet` icin Γ-kaydina dayanir), indeks `scalar`,
      okunan hucre `scalar` (`hucreOku` daima skaler doner). -/
  | t_indeks  (Γ : TipOrtam) (Δ : KanalOrtam) (x : VarId) (idx : Ifade) (τ : Tip) :
                tipOrtamGet Γ x = some τ →
                HasType Γ Δ idx Tip.scalar →
                HasType Γ Δ (Ifade.indeks x idx) Tip.scalar

  /-- T-INDEKS-ATA (D-337): `x[idx] = deger` — x kayitli, indeks ve deger
      `scalar`; yazma bir DEYIM oldugundan sonuc `bos`. -/
  | t_indeks_ata (Γ : TipOrtam) (Δ : KanalOrtam) (x : VarId)
                 (idx e : Ifade) :
                   -- Dizinin tipi `scalar`: hucreler skalerdir. Bu SART
                   -- (herhangi bir τ degil), cunku ofset-0'a yazmak
                   -- KonfTipliFull'un `bagli` bileseninde x'in degerinin
                   -- tipini belirler — τ serbest olsaydi `skaler n`
                   -- yazmak o bileseni BOZARDI (korunum ispatinda cikti).
                   tipOrtamGet Γ x = some Tip.scalar →
                   HasType Γ Δ idx Tip.scalar →
                   HasType Γ Δ e Tip.scalar →
                   HasType Γ Δ (Ifade.indeksAta x idx e) Tip.bos

  /-- T-GUVENSIZ: ic ifadeyi delegate eder (NoGuvensiz program seviyesinde). -/
  | t_guvensiz (Γ : TipOrtam) (Δ : KanalOrtam) (e : Ifade) (τ : Tip) :
                HasType Γ Δ e τ →
                HasType Γ Δ (Ifade.guvensiz e) τ


-- ============================================================
-- §2. Yardimci helper'lar
-- ============================================================

/-- Bos tip ortami. -/
abbrev tipOrtamBos : TipOrtam := []

/-- Varsayilan kanal ortami (her kanal bos eleman tipli). -/
abbrev kanalOrtamBos : KanalOrtam := fun _ => Tip.bos

/-- "Kapatilmis" tipli — bos Γ ile HasType (program seviyesi). -/
def IyiTipliKapali (e : Ifade) (τ : Tip) : Prop :=
  HasType tipOrtamBos kanalOrtamBos e τ

end Kemgu.Sem.HasType
