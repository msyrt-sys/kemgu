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
bilgisi ekler. Adim 6 RegionOK + Typed conjunction'i sonu KonfTipli.ThreadTipli
(Adim 2 iskelet `True`) gercek tanim alir.

Onkosul: Adim 1.1-1.3 (Step dual), Adim 2 (StateTipli), Adim 3 (HasType).
-/

import Kemgu.Sem.Core
import Kemgu.Sem.SmallStep
import Kemgu.Sem.StateTipli
import Kemgu.Sem.HasType
import Kemgu.Sem.ProgressKorunum

namespace Kemgu.Sem.LineerTamam
open Kemgu.Sem.Core Kemgu.Sem.SmallStep Kemgu.Sem.StateTipli Kemgu.Sem.HasType Kemgu.Sem.ProgressKorunum

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
-- §2. LineerOrtam helper'lari (Λ lookup + update)
-- ============================================================

/-- LineerOrtam Λ lookup: bir VarId'nin Lineerlik durumu.
    `none` = degisken kayitsiz; `some Lineerlik.aktif` = tuketilmemis;
    `some Lineerlik.tuketildi` = tuketildi. -/
def lineerOrtamGet : LineerOrtam → VarId → Option Lineerlik
  | [], _ => none
  | (k, v) :: rest, key => if k = key then some v else lineerOrtamGet rest key

/-- LineerOrtam Λ update: bir VarId'nin Lineerlik durumunu degistir.
    Implementasyon: prepend (newest-wins) — yeni entry eski entry'i mantiken
    override eder. sahiplikSet ile ayni desen (Core.lean §7.1). -/
def lineerOrtamUpdate (Λ : LineerOrtam) (x : VarId) (lin : Lineerlik) : LineerOrtam :=
  (x, lin) :: Λ


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

      V1 sinir: Λ' tam set difference notasyonu zor — inductive olarak
      "yeterli Λ_inner ve disardan tuketim" durumu. Detayli iyileştirme
      Adim 7 Discharge'da. -/
  | l_gorev_baslat (Γ : TipOrtam) (Λ Λ' : LineerOrtam) (yd : List VarId) (kod : Ifade) :
                     -- V1: kod inner Lambda ile typed (henuz tam form yok)
                     -- Λ' caller'da consumed yd ∩ Linear kapsar
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
-- §4. Typed conjunction iskelet (Plan v2 §3.6)
-- HasType + LineerTamam birlesimi. Adim 6 RegionOK eklenince tam Typed.
-- ============================================================

/-- Typed iskelet (Adim 5 hali): HasType + LineerTamam conjunction.
    Adim 6'da RegionOK eklenip Typed Γ Λ Ρ e τ Λ' Ρ' formuna tamamlanir.

    Mevcut Adim 5 hali (Plan §3.6 "Typed structure"):
        Typed Γ Λ e τ Λ' = HasType Γ e τ ∧ LineerTamam Γ Λ e Λ'

    Adim 6 hedef:
        Typed Γ Λ Ρ e τ Λ' Ρ' = HasType Γ e τ
                              ∧ LineerTamam Γ Λ e Λ'
                              ∧ RegionTamam Γ Ρ e Ρ' -/
structure TypedAdim5 (Γ : TipOrtam) (Λ : LineerOrtam)
                     (e : Ifade) (τ : Tip)
                     (Λ' : LineerOrtam) : Prop where
  hasType    : HasType Γ e τ
  lineerOK   : LineerTamam Γ Λ e Λ'


-- ============================================================
-- §5. Progress + Preservation update (Plan §7.2 Adim 5)
-- LineerTamam katmani eklenmiş yeni statement'lar.
-- ============================================================

/-- "Bos lineer ortam" — program seviyesi (kapatilmis tipli ifadeler). -/
abbrev lineerOrtamBos : LineerOrtam := []

/-- Progress (LineerTamam ile) — Plan v2 §3.7 + §7.2.

    Adim 4.2'deki progress (yalnız HasType) artik LineerTamam ile zenginlesti:
    "Iyi-tipli + Lineer-uyumlu program ya degerdir ya da Step alabilir."

    V1 sinir: full proof Adim 7 Discharge sonrasi (Hata case'leri exfalso).
    Su an statement-only iskelet. -/
theorem progress_lineer
    (e : Ifade) (τ : Tip) (Λ' : LineerOrtam)
    (_h_typed_lin : TypedAdim5 tipOrtamBos lineerOrtamBos e τ Λ')
    (S : Konfigurasyon) (ctx : ThreadCtx)
    (_h_ctx_in : ctx ∈ S.thread) (_h_ctx_ifade : ctx.ifade = e)
    (_h_no_fault : S.fault = none) :
    IsValue e ∨ ∃ S', Step S S' := by
  -- TODO: Adim 5.2 — TypedAdim5 destructure (hasType + lineerOK) ve
  -- progress (Adim 4.2 kismi) + LineerTamam ek bilgisi ile case analizi.
  -- Hata case'leri Adim 7 Discharge ile.
  sorry

/-- Preservation (LineerTamam ile) — Plan v2 §3.7 + §7.2.

    Step S → S' altinda Typed (HasType + LineerTamam) korunur.
    LineerTamam Λ -> Λ' gecisi Step ifadeleri tarafindan tutulur.

    V1 sinir: full proof Adim 7 Discharge sonrasi. -/
theorem preservation_lineer
    (S S' : Konfigurasyon) (_h_step : Step S S')
    (ctx : ThreadCtx) (τ : Tip) (Λ' : LineerOrtam)
    (_h_in : ctx ∈ S.thread)
    (_h_typed_lin : TypedAdim5 tipOrtamBos lineerOrtamBos ctx.ifade τ Λ')
    (_h_no_fault_target : S'.fault = none) :
    ∃ ctx' ∈ S'.thread, ∃ Λ'_new,
      ctx'.tid = ctx.tid ∧
      TypedAdim5 tipOrtamBos lineerOrtamBos ctx'.ifade τ Λ'_new := by
  -- TODO: Adim 5.3 — Step constructor case analizi:
  --   Hata (7): exfalso + h_no_fault_target vs h_fault
  --   Tamam (8): hasType korunumu (Adim 4.3) + lineerTamam korunumu
  --     - cGorevBaslatTamam: linearYakalananlar tuketim (h_lineer_caller)
  --     - sLinKullanTamam/sLinImhaTamam: Λ' = Λ \ {x ↦ tuketildi}
  --     - diger Tamam'lar: Λ degismez (genel kural)
  sorry


-- ============================================================
-- §6. Adim 5 sub-step durumu — V1 sinir + sonraki adim
-- ============================================================

/-
Adim 5 (LineerTamam katmani) — DURUM 2026-05-22:

✅ Adim 5.1 (bu commit): LineerTamam inductive (13 kural) + helper'lar
   + TypedAdim5 conjunction + sub-lemma statement'lar (sorry)

⏳ Adim 5.2 (gelecek): progress_lineer full proof
   - HasType (Adim 3) + LineerTamam case analizi
   - Hata Step constructor'lari Adim 7 Discharge ile exfalso

⏳ Adim 5.3 (gelecek): preservation_lineer full proof
   - 15 Step constructor case
   - LineerTamam Λ → Λ' korunumu per constructor

KEMGU ile uyum:
- TipOrtam (Γ) zaten Adim 2'de
- LineerOrtam (Λ) Core.lean §5'te var (List (VarId × Lineerlik))
- Lineerlik (aktif | tuketildi) Core.lean §5'te
- Tip.lineerMi yeni eklendi (yalniz tekkez true)

Adim 6 hedef (sonraki):
- RegionTamam (RegionOK) katmani — bolge gecisleri (Plan v2 §3.4)
- Typed full = HasType ∧ LineerTamam ∧ RegionTamam (Plan §3.6)
- KonfTipli.ThreadTipli iskelet (Adim 2 placeholder True) Typed ile dolar
- L0-L7 lemmalari Typed hipotezi alabilir

V1 sinir notu: KEMGU semantik karmaşıklığı (Configuration Step) klasik
Wright-Felleisen lone-form'a uyumsuz. progress_lineer ve
preservation_lineer V1'de full proof Adim 7 Discharge + No-Fault çatı
sonrasi tractable.
-/

end Kemgu.Sem.LineerTamam
