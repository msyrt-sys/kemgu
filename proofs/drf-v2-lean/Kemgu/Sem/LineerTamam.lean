/-
KEMGU DRF Mekanize — LineerTamam Katmani (Plan v2 Adim 5)
Kaynak: belgeler/KEMGU_Mekanize_Onarim_Plan.md §3.3 LinearOK + §7.2 Adim 5
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz

Adim 5: Plan §3 katmanli typing judgment'in IKINCI KATMANI:
  LineerTamam : TipOrtam → LineerOrtam → Ifade → LineerOrtam → Prop

Bu katman lineer durum (Lambda) gecislerini izler:
- Hangi lineer degisken kullanildi (tuketildi)
- Yakalama listesindeki lineer'lar caller'da tuketilmis mi
- Cifte consume yasagi

HasType (Adim 3) sadece tip uyumu; LineerTamam onun uzerine lineerlik
bilgisi ekler.

Onarim v3 F1 NOT: Bu dosya artik YALNIZ judgment katmanidir. TypedAdim5
SILINDI (Tipli.Typed kullanilir); progress_lineer/preservation_lineer
Meta/ProgressKorunum.lean'e tasinip Typed-formda birlestirildi.
SmallStep/ProgressKorunum import'lari kaldirildi (judgment Step'e bagimsiz).

Onkosul: Adim 2 (StateTipli), Adim 3 (HasType).
-/

import Kemgu.Sem.Core
import Kemgu.Sem.StateTipli
import Kemgu.Sem.HasType

namespace Kemgu.Sem.LineerTamam
open Kemgu.Sem.Core Kemgu.Sem.StateTipli Kemgu.Sem.HasType

-- ============================================================
-- §1. Tip.lineerMi — bir tipin lineer kategoride olmasi
-- Plan v2 §3.3: l_var_nonlinear / l_var_move ayrımi.
-- ============================================================

/-- Bir tip lineer kategoride midir?
    V1: tekkez<T> linear (Linear Types Spec V1).
    Diger tipler nonlinear (copyable/shareable).

    V2 hedef: borrowed reference (&T) ve diger Linear genisletmeler
    ile genisletilir. -/
def Tip.lineerMi : Tip → Bool
  | Tip.tekkez _ => true
  | _ => false


-- ============================================================
-- §2. LineerOrtam helper'lari — F2'de Core'a TASINDI
-- (lineerOrtamGet / lineerOrtamUpdate / lineerTuket / lineerTuketListe
--  artik Kemgu.Sem.Core'da — Step kurallari da kullaniyor.)
-- ============================================================


-- ============================================================
-- §3. LineerTamam (LinearOK) — Plan v2 §3.3 — INDUKTIF JUDGMENT
-- ============================================================

/-- LineerTamam Γ Λ e Λ' — Lineer durum gecisi:
    Γ : TipOrtam (tip ortami)
    Λ : LineerOrtam (giriş Lineerlik haritasi)
    e : Ifade (incelenen ifade)
    Λ' : LineerOrtam (cikiş Lineerlik haritasi, e degerlendirmesinden sonra)

    Bu judgment lineerlik kontrolu YAPAR — yani:
    - Lineer degerlerin TEK KEZ tuketildigini garanti eder
    - Yakalama listesi (gorevBaslat yd) icin caller'da consume kontrolu
    - Lineer'larin sub-expression'larda preserve edildiğini izler

    Plan v2 §3.3 tasarim felsefesi: HasType (Adim 3) sadece tip uyumu;
    LineerTamam (Adim 5) lineer durum gecisi. Adim 6 RegionOK + Typed
    conjunction ile birlesik. -/
inductive LineerTamam : TipOrtam → LineerOrtam → Ifade → LineerOrtam → Prop where

  /-- L-TANIM (nonlinear): tip lineer degil → Λ degismez.
      Plan v2 §3.3 "l_var_nonlinear". -/
  | l_tanim_nonlin (Γ : TipOrtam) (Λ : LineerOrtam) (x : VarId) (τ : Tip) :
                    tipOrtamGet Γ x = some τ →
                    Tip.lineerMi τ = false →
                    LineerTamam Γ Λ (Ifade.tanim x) Λ

  /-- L-TANIM (linear move): tip lineer + aktif → consume (Λ x ↦ tuketildi).
      Plan v2 §3.3 "l_var_move". -/
  | l_tanim_lin (Γ : TipOrtam) (Λ : LineerOrtam) (x : VarId) (τ : Tip) :
                  tipOrtamGet Γ x = some τ →
                  Tip.lineerMi τ = true →
                  lineerOrtamGet Λ x = some Lineerlik.aktif →
                  LineerTamam Γ Λ (Ifade.tanim x)
                                (lineerOrtamUpdate Λ x Lineerlik.tuketildi)

  /-- L-SABIT: literal lineer durum'u etkilemez.
      Λ degismez. Plan §3.3'te direkt geçilmis (sabit hem T-LIT'i hem
      L-LIT'i bir arada). -/
  | l_sabit (Γ : TipOrtam) (Λ : LineerOrtam) (v : Deger) :
              LineerTamam Γ Λ (Ifade.sabit v) Λ

  /-- L-ATAMA: x := e — e degerlendirilir, sonuc Λ degisimi e'den gelir.
      V1 sinir: atama hedef x'in lineer kontrolu yok (Adim 7 Discharge'da
      sıkılaştırılır). -/
  | l_atama (Γ : TipOrtam) (Λ Λ' : LineerOrtam) (x : VarId) (e : Ifade) :
              LineerTamam Γ Λ e Λ' →
              LineerTamam Γ Λ (Ifade.atama x e) Λ'

  /-- L-SEQ: a; b — a Λ' verir, b o Λ''den devam eder. -/
  | l_seq (Γ : TipOrtam) (Λ Λa Λb : LineerOrtam) (a b : Ifade) :
            LineerTamam Γ Λ a Λa →
            LineerTamam Γ Λa b Λb →
            LineerTamam Γ Λ (Ifade.seq a b) Λb

  /-- L-GOREV-BASLAT: yakalama listesinde lineer olanlar TUKETILIR (caller'da).
      Plan v2 §3.3 "l_gorev_baslat":
        Λ' = Λ \ (yd ∩ {v | Tip.lineerMi (Γ v)})

      Adim 8 V2 P6 strengthen (use-after-move): yakalanan hicbir v ZATEN
      tuketilmis olamaz — ∀ v ∈ yd, lineerOrtamGet Λ v ≠ some tuketildi.
      l_kanal_gonder ile simetrik; cifte-move yasagi.
      typing_excludes_cGorevBaslatHataLineerIhlal bu sarti kullanir. -/
  | l_gorev_baslat (Γ : TipOrtam) (Λ Λ' : LineerOrtam) (yd : List VarId) (kod : Ifade) :
                     (∀ v ∈ yd, lineerOrtamGet Λ v ≠ some Lineerlik.tuketildi) →
                     LineerTamam Γ Λ (Ifade.gorevBaslat yd kod) Λ'

  /-- L-GOREV-BIRLESTIR: birlestir(g) — Λ etkilenmez (V1 sinir).
      V2 hedef: g'nin gorev<τ> donus ile yeni lineer baglama. -/
  | l_gorev_birlestir (Γ : TipOrtam) (Λ : LineerOrtam) (g : VarId) :
                        LineerTamam Γ Λ (Ifade.gorevBirlestir g) Λ

  /-- L-KANAL-GONDER: v lineer ise tuketilir.
      Adim 8 P2 strengthen (Plan §6.2 Aile 2): gonderilen v ZATEN tuketilmis
      olamaz (`lineerOrtamGet Λ v ≠ some tuketildi`). Bu sart l_kullan/l_imha
      ile simetrik — "aktif veya kayitsiz (none)" gonderilebilir, yalniz
      "tuketildi" (cifte gonderim) reddedilir.
      Soundness: non-lineer v icin Λ v = none → `none ≠ some tuketildi` trivial
      saglanir; gercekten yalniz cifte-gonderim reddedilir.
      typing_excludes_cKanalGonderHataLineerTuket (Aile2.lean) bu sarti kullanir. -/
  | l_kanal_gonder (Γ : TipOrtam) (Λ Λ' : LineerOrtam) (k : KanalId) (v : VarId) :
                     lineerOrtamGet Λ v ≠ some Lineerlik.tuketildi →
                     LineerTamam Γ Λ (Ifade.kanalGonderIf k v) Λ'

  /-- L-KANAL-AL: kanal'dan deger alinir, yeni lineer baglama olabilir.
      V1: Λ degismez (alinan deger receiver'da bind edilmis varsayilir). -/
  | l_kanal_al (Γ : TipOrtam) (Λ : LineerOrtam) (k : KanalId) :
                 LineerTamam Γ Λ (Ifade.kanalAlIf k) Λ

  /-- L-DONDUR: dondur(b) Λ'yi etkilemez (frozen marker yalniz Sigma'da). -/
  | l_dondur (Γ : TipOrtam) (Λ : LineerOrtam) (b : Bolge) :
               LineerTamam Γ Λ (Ifade.dondurIf b) Λ

  /-- L-KULLAN: kullan(x) — tekkez<τ> consume.
      Plan v2 §3.3 "l_kullan". -/
  | l_kullan (Γ : TipOrtam) (Λ : LineerOrtam) (x : VarId) (τ : Tip) :
              tipOrtamGet Γ x = some (Tip.tekkez τ) →
              lineerOrtamGet Λ x = some Lineerlik.aktif →
              LineerTamam Γ Λ (Ifade.kullanIf x)
                            (lineerOrtamUpdate Λ x Lineerlik.tuketildi)

  /-- L-IMHA: imha(x) — tekkez<τ> consume (silinme yoluyla). -/
  | l_imha (Γ : TipOrtam) (Λ : LineerOrtam) (x : VarId) (τ : Tip) :
            tipOrtamGet Γ x = some (Tip.tekkez τ) →
            lineerOrtamGet Λ x = some Lineerlik.aktif →
            LineerTamam Γ Λ (Ifade.imhaIf x)
                          (lineerOrtamUpdate Λ x Lineerlik.tuketildi)

  /-- L-GUVENSIZ: ic ifade delegate. -/
  | l_guvensiz (Γ : TipOrtam) (Λ Λ' : LineerOrtam) (e : Ifade) :
                 LineerTamam Γ Λ e Λ' →
                 LineerTamam Γ Λ (Ifade.guvensiz e) Λ'


-- ============================================================
-- §4. Yardimci abbrev
-- ============================================================

/-- "Bos lineer ortam" — program seviyesi (kapatilmis tipli ifadeler). -/
abbrev lineerOrtamBos : LineerOrtam := []

-- Onarim v3 F1 NOT: TypedAdim5 + progress_lineer + preservation_lineer
-- SILINDI/TASINDI — birlesik Typed formu Kemgu/Sem/Tipli.lean'de,
-- meta-teoremler Kemgu/Meta/ProgressKorunum.lean'de (Typed-formda dedup).

end Kemgu.Sem.LineerTamam
