/-
KEMGU DRF Mekanize — Minimal HasType (Plan v2 Adim 3)
Kaynak (kagit formel): belgeler/KEMGU_Mekanize_Onarim_Plan.md §3.2
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz, sorry/axiom YOK

Adim 3: Klasik tip sistemi (TAPL §8 STLC formati).
Plan v2 §3'un katmanli typing judgment tasariminin BIRINCI KATMANI:
  HasType : TipOrtam → Ifade → Tip → Prop

Bu katmanda LINEERLIK ve BOLGE bilgisi YOK — sadece tip uyumu.
Lineer geciş (LinearOK) Adim 5'te, bolge gecişleri (RegionOK) Adim 6'da
eklenecek. Adim 6 sonu birleştirici `Typed = HasType ∧ LinearOK ∧ RegionOK`
StateTipli.ThreadTipli iskelet'inde Typed'i dolduracak.

Onkosul: Adim 1.1-1.3 (Step dual), Adim 2 (StateTipli iskelet).
-/

import Kemgu.Sem.Core
import Kemgu.Sem.StateTipli

namespace Kemgu.Sem.HasType
open Kemgu.Sem.Core Kemgu.Sem.StateTipli

-- ============================================================
-- §1. HasType — Klasik tip sistemi (12 Ifade kuralı)
-- Plan v2 §3.2 + Pierce TAPL §8.3 (STLC minimal)
-- ============================================================

/-- Klasik tip judgment Γ ⊢ e : τ.

    Ifade'in TipOrtam altinda tip uyumu — sadece tip kontrolu yapilir;
    lineerlik (Λ) veya bolge (Ρ) bilgisi YOK (Adim 5/6'da eklenecek).

    Plan v2 §3.2 felsefesi: ayri katmanlar (modulerlik + asamali insa).
    Her katmanin kendi Progress/Preservation lemmalari Adim 4-6'da. -/
inductive HasType : TipOrtam → Ifade → Tip → Prop where

  /-- T-TANIM: degisken referansi tipini Γ'dan alır.
      Kagit: Γ(x) = τ ⟹ Γ ⊢ x : τ. -/
  | t_tanim   (Γ : TipOrtam) (x : VarId) (τ : Tip) :
                tipOrtamGet Γ x = some τ →
                HasType Γ (Ifade.tanim x) τ

  /-- T-SABIT: literal (sabit deger) tipi DegerTipli'den (Adim 2) gelir.
      Kagit: ⊢ v : τ ⟹ Γ ⊢ (sabit v) : τ. Literal'lar bolge bagimsiz
      oldugu icin DegerTipli'yi bos BolgeOrtam ile cagiriyoruz. -/
  | t_sabit   (Γ : TipOrtam) (v : Deger) (τ : Tip) :
                DegerTipli Γ [] v τ →
                HasType Γ (Ifade.sabit v) τ

  /-- T-ATAMA: x := e tip-uyumlu ise sonuc tipi bos.
      Kagit: Γ(x) = τ, Γ ⊢ e : τ ⟹ Γ ⊢ (x := e) : bos.
      V1 sinir: Atama hedef ile deger tipi AYNI olmali (no subtyping). -/
  | t_atama   (Γ : TipOrtam) (x : VarId) (e : Ifade) (τ : Tip) :
                tipOrtamGet Γ x = some τ →
                HasType Γ e τ →
                HasType Γ (Ifade.atama x e) Tip.bos

  /-- T-SEQ: a; b dizisinin tipi son ifadenin (b) tipidir.
      Kagit: Γ ⊢ a : τa, Γ ⊢ b : τb ⟹ Γ ⊢ (a; b) : τb. -/
  | t_seq     (Γ : TipOrtam) (a b : Ifade) (τa τb : Tip) :
                HasType Γ a τa →
                HasType Γ b τb →
                HasType Γ (Ifade.seq a b) τb

  /-- T-GOREV-BASLAT: gorev_baslat(yd, kod) tipi gorev<τd>.
      Kagit: Γ ⊢ kod : τd ⟹ Γ ⊢ gorev_baslat(yd, kod) : gorev<τd>.

      V1 sinir: yd (yakalama listesi) tip kontrolu yok — Adim 5 LinearOK
      bunu detaylandiracak (yakalama Γ_inner ile bagli inner scope). -/
  | t_gorev_baslat (Γ : TipOrtam) (yd : List VarId) (kod : Ifade) (τd : Tip) :
                    HasType Γ kod τd →
                    HasType Γ (Ifade.gorevBaslat yd kod) (Tip.gorev τd)

  /-- T-GOREV-BIRLESTIR: birlestir(g) ile gorev tipinden τ cikar.
      Kagit: Γ(g) = gorev<τ> ⟹ Γ ⊢ birlestir(g) : τ. -/
  | t_gorev_birlestir (Γ : TipOrtam) (g : VarId) (τ : Tip) :
                       tipOrtamGet Γ g = some (Tip.gorev τ) →
                       HasType Γ (Ifade.gorevBirlestir g) τ

  /-- T-KANAL-GONDER: gonder(k, v) tipi bos (etki: kanala v eklenir).
      Kagit: Γ(v) = τ ⟹ Γ ⊢ kanalGonderIf(k, v) : bos.

      V1 sinir: kanal'in TASIDIGI tip ile gonderim tipi uyumu kontrolu
      yok — Adim 4 Progress/Preservation'da KanalTutarli ile sıkılaştırılır. -/
  | t_kanal_gonder (Γ : TipOrtam) (k : KanalId) (v : VarId) (τ : Tip) :
                    tipOrtamGet Γ v = some τ →
                    HasType Γ (Ifade.kanalGonderIf k v) Tip.bos

  /-- T-KANAL-AL: kanalAlIf(k) tipi serbest τ (V1 sinir).
      Kagit: Γ ⊢ kanalAlIf(k) : τ (τ context'ten cikarsanir).

      V1 sinir: kanal'in tipinden cikarim Adim 4'te KanalTutarli ile
      dogru duruma getirilir. -/
  | t_kanal_al  (Γ : TipOrtam) (k : KanalId) (τ : Tip) :
                  HasType Γ (Ifade.kanalAlIf k) τ

  /-- T-DONDUR: dondur(b) sonuc tipi bos (etki: bolge b frozen).
      Kagit: Γ ⊢ dondurIf(b) : bos. -/
  | t_dondur  (Γ : TipOrtam) (b : Bolge) :
                HasType Γ (Ifade.dondurIf b) Tip.bos

  /-- T-KULLAN: linear consume — tekkez<τ>'den τ cikar.
      Kagit: Γ(x) = tekkez<τ> ⟹ Γ ⊢ kullan(x) : τ.

      NOT: Linear consumption garantisi (x sadece bir kez kullanilir)
      Adim 5 LinearOK'ta saglanir; burada salt tip uyumu. -/
  | t_kullan  (Γ : TipOrtam) (x : VarId) (τ : Tip) :
                tipOrtamGet Γ x = some (Tip.tekkez τ) →
                HasType Γ (Ifade.kullanIf x) τ

  /-- T-IMHA: linear imha — tekkez<τ>'yi yok eder, sonuc bos.
      Kagit: Γ(x) = tekkez<τ> ⟹ Γ ⊢ imha(x) : bos.

      NOT: imha consumption garantisi yine Adim 5 LinearOK. -/
  | t_imha    (Γ : TipOrtam) (x : VarId) (τ : Tip) :
                tipOrtamGet Γ x = some (Tip.tekkez τ) →
                HasType Γ (Ifade.imhaIf x) Tip.bos

  /-- T-GUVENSIZ: guvensiz blok ic ifadeyi delegate eder.
      Kagit: Γ ⊢ e : τ ⟹ Γ ⊢ guvensiz(e) : τ.

      NOT: NoGuvensiz (Op.Sem §7 kosul 7) program seviyesinde guvensiz
      blok kullanimini yasaklar — bu kural sadece grammar'da kabul. -/
  | t_guvensiz (Γ : TipOrtam) (e : Ifade) (τ : Tip) :
                HasType Γ e τ →
                HasType Γ (Ifade.guvensiz e) τ


-- ============================================================
-- §2. Yardimci helper'lar
-- ============================================================

/-- Bos tip ortami (TipOrtam.empty). -/
abbrev tipOrtamBos : TipOrtam := []

/-- "Kapatilmis" tipli — bos Γ ile HasType (program seviyesi).
    Wright-Felleisen Soundness'in kapatilmis form'unda kullanilir. -/
def IyiTipliKapali (e : Ifade) (τ : Tip) : Prop :=
  HasType tipOrtamBos e τ


-- ============================================================
-- §3. Temel teknik lemmalar (iskelet)
-- Progress + Preservation lemmalari Adim 4'te ayri olarak gelir.
-- ============================================================

/-- HasType inductive'in temel bir gozlemi: HasType bir Prop oldugu icin
    decidable degil; ek `tipCheck` decision procedure Adim 3 sonrasi
    (opsiyonel, Plan §3.8). -/
theorem hasType_decidable_note : True := trivial


end Kemgu.Sem.HasType
