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

/-- `LineerNotr Γ e` — e LINEER-NOTR: hicbir lineer baglamayi tuketmez.
    D-332'de `eger` dallari icin gerekir.

    KRITIK OZELLIK: yargida Λ YOKTUR (yalniz Γ). Bu sayede ortam
    zayiflatmasi altinda AYNEN tasinir (`lineerTamam_kucuk_transport`'un
    `l_eger` kolu bedava kapanir) — dal-duyarli meet yaklasiminin
    gerektirdigi monotonluk makinesi olmadan.

    DISARIDA BIRAKILANLAR (tuketen veya tuketebilen formlar):
    `tanim` lineer tipte, `gorevBaslat` (yakalama tuketir),
    `kanalGonderIf` (gonderim tuketir), `kullanIf`, `imhaIf`. -/
inductive LineerNotr (Γ : TipOrtam) : Ifade → Prop where
  | n_tanim (x : VarId) (τ : Tip) :
      tipOrtamGet Γ x = some τ → Tip.lineerMi τ = false →
      LineerNotr Γ (Ifade.tanim x)
  | n_sabit (v : Deger) : LineerNotr Γ (Ifade.sabit v)
  | n_atama (x : VarId) (e : Ifade) :
      LineerNotr Γ e → LineerNotr Γ (Ifade.atama x e)
  | n_seq (a b : Ifade) :
      LineerNotr Γ a → LineerNotr Γ b → LineerNotr Γ (Ifade.seq a b)
  | n_gorev_birlestir (g : VarId) : LineerNotr Γ (Ifade.gorevBirlestir g)
  | n_kanal_al (k : KanalId) : LineerNotr Γ (Ifade.kanalAlIf k)
  | n_dondur (b : Bolge) : LineerNotr Γ (Ifade.dondurIf b)
  | n_guvensiz (e : Ifade) :
      LineerNotr Γ e → LineerNotr Γ (Ifade.guvensiz e)
  | n_eger (k d y : Ifade) :
      LineerNotr Γ k → LineerNotr Γ d → LineerNotr Γ y →
      LineerNotr Γ (Ifade.eger k d y)
  | n_topla (a b : Ifade) :
      LineerNotr Γ a → LineerNotr Γ b → LineerNotr Γ (Ifade.topla a b)
  | n_bol (a b : Ifade) :
      LineerNotr Γ a → LineerNotr Γ b → LineerNotr Γ (Ifade.bol a b)
  | n_kalan (a b : Ifade) :
      LineerNotr Γ a → LineerNotr Γ b → LineerNotr Γ (Ifade.kalan a b)
  | n_iken (k g : Ifade) :
      LineerNotr Γ k → LineerNotr Γ g → LineerNotr Γ (Ifade.iken k g)
  | n_esles (s : Ifade) (n : Int) (d y : Ifade) :
      LineerNotr Γ s → LineerNotr Γ d → LineerNotr Γ y →
      LineerNotr Γ (Ifade.esles s n d y)
  | n_indeks (x : VarId) (idx : Ifade) (τ : Tip) :
      tipOrtamGet Γ x = some τ → Tip.lineerMi τ = false →
      LineerNotr Γ idx → LineerNotr Γ (Ifade.indeks x idx)
  | n_indeks_ata (x : VarId) (idx e : Ifade) (τ : Tip) :
      tipOrtamGet Γ x = some τ → Tip.lineerMi τ = false →
      LineerNotr Γ idx → LineerNotr Γ e →
      LineerNotr Γ (Ifade.indeksAta x idx e)


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

  /-- L-GOREV-BASLAT: yakalama listesindeki aktif lineer'lar caller'da
      TUKETILIR — cikis ortami ARTIK BELIRLI (F3: serbest Λ' vakumu kapandi):
        Λ' = lineerTuketListe Λ yd
      (cGorevBaslatTamam'in runtime guncellemesiyle birebir ayni fonksiyon).
      Use-after-move guard'i korunur: yakalanan hicbir v zaten tuketilmis
      olamaz — typing_excludes_cGorevBaslatHataLineerIhlal bunu kullanir. -/
  | l_gorev_baslat (Γ : TipOrtam) (Λ Λkod : LineerOrtam) (yd : List VarId) (kod : Ifade) :
                     (∀ v ∈ yd, lineerOrtamGet Λ v ≠ some Lineerlik.tuketildi) →
                     LineerTamam Γ (yd.map (fun v => (v, Lineerlik.aktif))) kod Λkod →
                     LineerTamam Γ Λ (Ifade.gorevBaslat yd kod)
                                   (lineerTuketListe Λ yd)

  /-- L-GOREV-BIRLESTIR: birlestir(g) — Λ etkilenmez (V1 sinir).
      V2 hedef: g'nin gorev<τ> donus ile yeni lineer baglama. -/
  | l_gorev_birlestir (Γ : TipOrtam) (Λ : LineerOrtam) (g : VarId) :
                        LineerTamam Γ Λ (Ifade.gorevBirlestir g) Λ

  /-- L-KANAL-GONDER: v lineer-aktif ise tuketilir — cikis ortami ARTIK
      BELIRLI (F3): Λ' = lineerTuket Λ v (cKanalGonderTamam ile birebir).
      Cifte-gonderim guard'i korunur (zaten-tuketilmis gonderilemez);
      typing_excludes_cKanalGonderHataLineerTuket bunu kullanir. -/
  | l_kanal_gonder (Γ : TipOrtam) (Λ : LineerOrtam) (k : KanalId) (v : VarId) :
                     lineerOrtamGet Λ v ≠ some Lineerlik.tuketildi →
                     LineerTamam Γ Λ (Ifade.kanalGonderIf k v)
                                   (lineerTuket Λ v)

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

  /-- L-TOPLA (D-334): SIRALI kompozisyon (l_seq ile ayni) — dallanma
      olmadigi icin `LineerNotr` daraltmasi GEREKMEZ. -/
  | l_topla (Γ : TipOrtam) (Λ Λa Λb : LineerOrtam) (a b : Ifade) :
              LineerTamam Γ Λ a Λa →
              LineerTamam Γ Λa b Λb →
              LineerTamam Γ Λ (Ifade.topla a b) Λb

  /-- L-BOL (D-338): sirali kompozisyon (`topla` ile ayni). -/
  | l_bol (Γ : TipOrtam) (Λ Λa Λb : LineerOrtam) (a b : Ifade) :
              LineerTamam Γ Λ a Λa →
              LineerTamam Γ Λa b Λb →
              LineerTamam Γ Λ (Ifade.bol a b) Λb

  /-- L-KALAN (D-339): sirali kompozisyon (`topla` ile ayni). -/
  | l_kalan (Γ : TipOrtam) (Λ Λa Λb : LineerOrtam) (a b : Ifade) :
              LineerTamam Γ Λ a Λa →
              LineerTamam Γ Λa b Λb →
              LineerTamam Γ Λ (Ifade.kalan a b) Λb

  /-- L-EGER (D-332): kosul Λ → Λk; HER IKI DAL LINEER-NOTR olmalidir
      (`LineerNotr` — asagida), yani dis bir lineer baglamayi TUKETEMEZ;
      cikis ortami Λk'dir.

      GEREKCE: hangi dal alinirsa alinsin adim-sonrasi lineer ortam
      AYNIDIR → korunum ispati dal secimine bagimsiz olur.

      ** V1 DARALTMASI (acikca borc) **: KEMGU derleyicisi D-311/D-312'de
      dal-DUYARLI tuketimi destekler (`eger p { kullan(t); } degilse
      { imha(t); }` GECERLIDIR). Bu MODEL onu KAPSAMAZ — burada
      dallar lineer tuketim yapamaz. Daraltma KONSERVATIFTIR (model
      dilin bir ALT KUMESI; kabul edilen her program gercekte de
      gecerli), yani ispatlanan teoremler gecerli kalir; eksik olan
      KAPSAMDIR. Tam L-COND icin ortam-bulusmasi (meet: "iki daldan
      birinde tuketildiyse tuketildi") + ≼-monotonluk lemmasi gerekir —
      V2 isi. Bu daraltma CT koprusunu ETKILEMEZ (CT hesabinda lineerlik
      yoktur). -/
  | l_eger (Γ : TipOrtam) (Λ Λk : LineerOrtam) (k d y : Ifade) :
             LineerTamam Γ Λ k Λk →
             LineerNotr Γ d →
             LineerNotr Γ y →
             LineerTamam Γ Λ (Ifade.eger k d y) Λk

  /-- L-IKEN (D-335) — KEMGU'nun L-LOOP kuralinin (D-312) mekanize hali:
      **dongu govdesi DIS bir lineer baglamayi TUKETEMEZ.** Gerekce ayni:
      0 iterasyon = sizinti, ≥2 iterasyon = cifte tuketim. Kosul da notr
      olmalidir (her turda yeniden degerlendirilir). Cikis = giris. -/
  | l_iken (Γ : TipOrtam) (Λ : LineerOrtam) (k g : Ifade) :
             LineerNotr Γ k → LineerNotr Γ g →
             LineerTamam Γ Λ (Ifade.iken k g) Λ

  /-- L-ESLES (D-335): `l_eger` ile ayni disiplin — kollar lineer-notr
      (V1 daraltmasi; dal-duyarli tuketim V2, bkz. D-332 (b)). -/
  | l_esles (Γ : TipOrtam) (Λ Λs : LineerOrtam) (s : Ifade) (n : Int) (d y : Ifade) :
              LineerTamam Γ Λ s Λs →
              LineerNotr Γ d → LineerNotr Γ y →
              LineerTamam Γ Λ (Ifade.esles s n d y) Λs

  /-- L-INDEKS (D-336): `x[idx]` bir OKUMADIR — diziyi TUKETMEZ, ama
      dizinin kendisi LINEER OLMAMALIDIR (lineer bir degerin hucresini
      okumak onu kismen tasimak olurdu; D-315 kismi-tasima disiplini).
      Lineer gecis yalniz indeks ifadesinden gelir. -/
  | l_indeks (Γ : TipOrtam) (Λ Λ' : LineerOrtam) (x : VarId) (idx : Ifade) (τ : Tip) :
               tipOrtamGet Γ x = some τ →
               Tip.lineerMi τ = false →
               LineerTamam Γ Λ idx Λ' →
               LineerTamam Γ Λ (Ifade.indeks x idx) Λ'

  /-- L-INDEKS-ATA (D-337): SIRALI kompozisyon (indeks sonra deger);
      dizi LINEER OLMAMALI (bir lineer degerin hucresine yazmak kismi
      tasima olurdu — D-315). -/
  | l_indeks_ata (Γ : TipOrtam) (Λ Λi Λe : LineerOrtam) (x : VarId)
                 (idx e : Ifade) (τ : Tip) :
                   tipOrtamGet Γ x = some τ →
                   Tip.lineerMi τ = false →
                   LineerTamam Γ Λ idx Λi →
                   LineerTamam Γ Λi e Λe →
                   LineerTamam Γ Λ (Ifade.indeksAta x idx e) Λe

  /-- L-GUVENSIZ: ic ifade delegate. -/
  | l_guvensiz (Γ : TipOrtam) (Λ Λ' : LineerOrtam) (e : Ifade) :
                 LineerTamam Γ Λ e Λ' →
                 LineerTamam Γ Λ (Ifade.guvensiz e) Λ'


/-- LINEER-NOTR ⟹ KIMLIK GECISI: notr ifade HERHANGI bir Λ'dan
    kendisine gecer. `eger` dallarinin adim sonrasi tiplenmesini
    (korunum) ve transport'u besleyen lemma. -/
theorem lineerNotr_kimlik {Γ : TipOrtam} {e : Ifade}
    (h : LineerNotr Γ e) : ∀ Λ, LineerTamam Γ Λ e Λ := by
  induction h with
  | n_tanim x τ h_g h_lin => exact fun Λ => LineerTamam.l_tanim_nonlin Γ Λ x τ h_g h_lin
  | n_sabit v => exact fun Λ => LineerTamam.l_sabit Γ Λ v
  | n_atama x e _ ih => exact fun Λ => LineerTamam.l_atama Γ Λ Λ x e (ih Λ)
  | n_seq a b _ _ ih_a ih_b => exact fun Λ => LineerTamam.l_seq Γ Λ Λ Λ a b (ih_a Λ) (ih_b Λ)
  | n_gorev_birlestir g => exact fun Λ => LineerTamam.l_gorev_birlestir Γ Λ g
  | n_kanal_al k => exact fun Λ => LineerTamam.l_kanal_al Γ Λ k
  | n_dondur b => exact fun Λ => LineerTamam.l_dondur Γ Λ b
  | n_guvensiz e _ ih => exact fun Λ => LineerTamam.l_guvensiz Γ Λ Λ e (ih Λ)
  | n_eger k d y _ hd hy ih_k _ _ =>
      exact fun Λ => LineerTamam.l_eger Γ Λ Λ k d y (ih_k Λ) hd hy
  | n_topla a b _ _ ih_a ih_b =>
      exact fun Λ => LineerTamam.l_topla Γ Λ Λ Λ a b (ih_a Λ) (ih_b Λ)
  | n_bol a b _ _ ih_a ih_b =>
      exact fun Λ => LineerTamam.l_bol Γ Λ Λ Λ a b (ih_a Λ) (ih_b Λ)
  | n_kalan a b _ _ ih_a ih_b =>
      exact fun Λ => LineerTamam.l_kalan Γ Λ Λ Λ a b (ih_a Λ) (ih_b Λ)
  | n_iken k g hk hg _ _ =>
      exact fun Λ => LineerTamam.l_iken Γ Λ k g hk hg
  | n_esles s n d y _ hd hy ih_s _ _ =>
      exact fun Λ => LineerTamam.l_esles Γ Λ Λ s n d y (ih_s Λ) hd hy
  | n_indeks x idx τ h_g h_lin _ ih =>
      exact fun Λ => LineerTamam.l_indeks Γ Λ Λ x idx τ h_g h_lin (ih Λ)
  | n_indeks_ata x idx e τ h_g h_lin _ _ ih_i ih_e =>
      exact fun Λ => LineerTamam.l_indeks_ata Γ Λ Λ Λ x idx e τ h_g h_lin
        (ih_i Λ) (ih_e Λ)


-- ============================================================
-- §4. Yardimci abbrev
-- ============================================================

/-- "Bos lineer ortam" — program seviyesi (kapatilmis tipli ifadeler). -/
abbrev lineerOrtamBos : LineerOrtam := []

-- Onarim v3 F1 NOT: TypedAdim5 + progress_lineer + preservation_lineer
-- SILINDI/TASINDI — birlesik Typed formu Kemgu/Sem/Tipli.lean'de,
-- meta-teoremler Kemgu/Meta/ProgressKorunum.lean'de (Typed-formda dedup).


-- ============================================================
-- §5. LineerKucuk (≼) — "daha-az-tuketilmis" on-siralamasi
-- (Onarim v3 kapanis — cong odak-yuku icin lineer monotonluk:
--  runtime lineer ortami, statik ciktidan daha az tuketmis olabilir
--  [sVarOku lineer-okumayi runtime'da tuketmez] — tum aktif /
--  ¬tuketildi premise'leri bu yonde monoton tasinir.)
-- ============================================================

/-- Λ' ≼ Λ : Λ' "daha az tuketilmis" — Λ'nin aktifleri Λ''de aktif,
    Λ''nin tuketilmisleri Λ'da tuketilmis, Λ'nin kayitsizlari Λ''de
    kayitsiz. -/
def LineerKucuk (Λ' Λ : LineerOrtam) : Prop :=
  (∀ x, lineerOrtamGet Λ x = some Lineerlik.aktif →
     lineerOrtamGet Λ' x = some Lineerlik.aktif)
  ∧ (∀ x, lineerOrtamGet Λ' x = some Lineerlik.tuketildi →
     lineerOrtamGet Λ x = some Lineerlik.tuketildi)
  ∧ (∀ x, lineerOrtamGet Λ x = none → lineerOrtamGet Λ' x = none)

theorem lineerKucuk_refl (Λ : LineerOrtam) : LineerKucuk Λ Λ :=
  ⟨fun _ h => h, fun _ h => h, fun _ h => h⟩

/-- ≼, update-tuketildi altinda monoton. -/
theorem lineerKucuk_update_tuketildi {Λ' Λ : LineerOrtam}
    (h : LineerKucuk Λ' Λ) (x : VarId) :
    LineerKucuk (lineerOrtamUpdate Λ' x Lineerlik.tuketildi)
                (lineerOrtamUpdate Λ x Lineerlik.tuketildi) := by
  refine ⟨?_, ?_, ?_⟩ <;> intro y h_y
  · show lineerOrtamGet ((x, Lineerlik.tuketildi) :: Λ') y = _
    have h_y' : lineerOrtamGet ((x, Lineerlik.tuketildi) :: Λ) y
        = some Lineerlik.aktif := h_y
    rw [lineerOrtamGet] at h_y' ⊢
    by_cases hx : x = y
    · rw [if_pos hx] at h_y'; cases h_y'
    · rw [if_neg hx] at h_y'
      rw [if_neg hx]
      exact h.1 y h_y'
  · show lineerOrtamGet ((x, Lineerlik.tuketildi) :: Λ) y
        = some Lineerlik.tuketildi
    have h_y' : lineerOrtamGet ((x, Lineerlik.tuketildi) :: Λ') y
        = some Lineerlik.tuketildi := h_y
    rw [lineerOrtamGet] at h_y' ⊢
    by_cases hx : x = y
    · rw [if_pos hx]
    · rw [if_neg hx] at h_y'
      rw [if_neg hx]
      exact h.2.1 y h_y'
  · show lineerOrtamGet ((x, Lineerlik.tuketildi) :: Λ') y = none
    have h_y' : lineerOrtamGet ((x, Lineerlik.tuketildi) :: Λ) y
        = none := h_y
    rw [lineerOrtamGet] at h_y' ⊢
    by_cases hx : x = y
    · rw [if_pos hx] at h_y'; cases h_y'
    · rw [if_neg hx] at h_y'
      rw [if_neg hx]
      exact h.2.2 y h_y'

/-- ≼, lineerTuket altinda monoton (lookup kombinasyon analizi). -/
theorem lineerKucuk_tuket {Λ' Λ : LineerOrtam}
    (h : LineerKucuk Λ' Λ) (v : VarId) :
    LineerKucuk (lineerTuket Λ' v) (lineerTuket Λ v) := by
  unfold lineerTuket
  cases hv : lineerOrtamGet Λ v with
  | none =>
      rw [h.2.2 v hv]
      exact h
  | some lin =>
      cases lin with
      | aktif =>
          rw [h.1 v hv]
          exact lineerKucuk_update_tuketildi h v
      | tuketildi =>
          cases hv' : lineerOrtamGet Λ' v with
          | none => exact h
          | some lin' =>
              cases lin' with
              | tuketildi => exact h
              | aktif =>
                  -- Λ' v aktif, Λ v tuketildi: yalniz Λ'-tarafi tuketir
                  refine ⟨?_, ?_, ?_⟩ <;> intro y h_y
                  · have h_ne : v ≠ y := by
                      intro he; rw [he] at hv; rw [hv] at h_y; cases h_y
                    show lineerOrtamGet ((v, Lineerlik.tuketildi) :: Λ') y
                        = some Lineerlik.aktif
                    rw [lineerOrtamGet, if_neg h_ne]
                    exact h.1 y h_y
                  · have h_y' : lineerOrtamGet
                        ((v, Lineerlik.tuketildi) :: Λ') y
                        = some Lineerlik.tuketildi := h_y
                    rw [lineerOrtamGet] at h_y'
                    by_cases hx : v = y
                    · rw [← hx]; exact hv
                    · rw [if_neg hx] at h_y'
                      exact h.2.1 y h_y'
                  · have h_ne : v ≠ y := by
                      intro he; rw [he] at hv; rw [hv] at h_y; cases h_y
                    show lineerOrtamGet ((v, Lineerlik.tuketildi) :: Λ') y
                        = none
                    rw [lineerOrtamGet, if_neg h_ne]
                    exact h.2.2 y h_y

/-- Λ, kendi update-tuketildi'sinden kucuktur (sVarOku lineer-okuma:
    runtime tuketmez, statik cikti tuketir — Λ ≼ update Λ x tuk). -/
theorem lineerKucuk_update_geri (Λ : LineerOrtam) (x : VarId) :
    LineerKucuk Λ (lineerOrtamUpdate Λ x Lineerlik.tuketildi) := by
  refine ⟨?_, ?_, ?_⟩ <;> intro y h_y
  · have h_y' : lineerOrtamGet ((x, Lineerlik.tuketildi) :: Λ) y
        = some Lineerlik.aktif := h_y
    rw [lineerOrtamGet] at h_y'
    by_cases hx : x = y
    · rw [if_pos hx] at h_y'; cases h_y'
    · rw [if_neg hx] at h_y'; exact h_y'
  · show lineerOrtamGet ((x, Lineerlik.tuketildi) :: Λ) y
        = some Lineerlik.tuketildi
    rw [lineerOrtamGet]
    by_cases hx : x = y
    · rw [if_pos hx]
    · rw [if_neg hx]; exact h_y
  · have h_y' : lineerOrtamGet ((x, Lineerlik.tuketildi) :: Λ) y = none := h_y
    rw [lineerOrtamGet] at h_y'
    by_cases hx : x = y
    · rw [if_pos hx] at h_y'; cases h_y'
    · rw [if_neg hx] at h_y'; exact h_y'

/-- ≼, lineerTuketListe altinda monoton. -/
theorem lineerKucuk_tuketListe {Λ' Λ : LineerOrtam}
    (h : LineerKucuk Λ' Λ) (yd : List VarId) :
    LineerKucuk (lineerTuketListe Λ' yd) (lineerTuketListe Λ yd) := by
  induction yd generalizing Λ' Λ with
  | nil => exact h
  | cons v rest ih => exact ih (lineerKucuk_tuket h v)

/-- ≼-TRANSPORT: daha-az-tuketilmis ortam altinda lineer-tiplenme
    korunur (tum aktif / ¬tuketildi premise'leri monoton); cikti da
    ≼-iliskili. Cong odak-yuku kompozisyonunun lineer ayagi. -/
theorem lineerTamam_kucuk_transport {Γ : TipOrtam}
    {Λ Λout : LineerOrtam} {e : Ifade}
    (h : LineerTamam Γ Λ e Λout) :
    ∀ Λn, LineerKucuk Λn Λ →
    ∃ Λoutn, LineerTamam Γ Λn e Λoutn ∧ LineerKucuk Λoutn Λout := by
  induction h with
  | l_tanim_nonlin _ x τ h_g h_lin =>
      exact fun Λn hk =>
        ⟨Λn, LineerTamam.l_tanim_nonlin _ _ x τ h_g h_lin, hk⟩
  | l_tanim_lin _ x τ h_g h_lin h_a =>
      intro Λn hk
      exact ⟨lineerOrtamUpdate Λn x Lineerlik.tuketildi,
        LineerTamam.l_tanim_lin _ _ x τ h_g h_lin (hk.1 x h_a),
        lineerKucuk_update_tuketildi hk x⟩
  | l_sabit _ v =>
      exact fun Λn hk => ⟨Λn, LineerTamam.l_sabit _ _ v, hk⟩
  | l_atama _ _ x e _ ih =>
      intro Λn hk
      obtain ⟨Λen, h_e, hk'⟩ := ih Λn hk
      exact ⟨Λen, LineerTamam.l_atama _ _ _ x e h_e, hk'⟩
  | l_seq _ _ _ a b _ _ ih_a ih_b =>
      intro Λn hk
      obtain ⟨Λan, h_a, hk_a⟩ := ih_a Λn hk
      obtain ⟨Λbn, h_b, hk_b⟩ := ih_b Λan hk_a
      exact ⟨Λbn, LineerTamam.l_seq _ _ _ _ a b h_a h_b, hk_b⟩
  | l_gorev_baslat _ Λkod yd kod h_ntuket h_kod =>
      intro Λn hk
      refine ⟨lineerTuketListe Λn yd,
        LineerTamam.l_gorev_baslat _ _ Λkod yd kod ?_ h_kod,
        lineerKucuk_tuketListe hk yd⟩
      intro v hv h_tuk
      exact h_ntuket v hv (hk.2.1 v h_tuk)
  | l_gorev_birlestir _ g =>
      exact fun Λn hk => ⟨Λn, LineerTamam.l_gorev_birlestir _ _ g, hk⟩
  | l_kanal_gonder _ k v h_ntuket =>
      intro Λn hk
      refine ⟨lineerTuket Λn v,
        LineerTamam.l_kanal_gonder _ _ k v ?_,
        lineerKucuk_tuket hk v⟩
      intro h_tuk
      exact h_ntuket (hk.2.1 v h_tuk)
  | l_kanal_al _ k =>
      exact fun Λn hk => ⟨Λn, LineerTamam.l_kanal_al _ _ k, hk⟩
  | l_dondur _ b =>
      exact fun Λn hk => ⟨Λn, LineerTamam.l_dondur _ _ b, hk⟩
  | l_kullan _ x τ h_g h_a =>
      intro Λn hk
      exact ⟨lineerOrtamUpdate Λn x Lineerlik.tuketildi,
        LineerTamam.l_kullan _ _ x τ h_g (hk.1 x h_a),
        lineerKucuk_update_tuketildi hk x⟩
  | l_imha _ x τ h_g h_a =>
      intro Λn hk
      exact ⟨lineerOrtamUpdate Λn x Lineerlik.tuketildi,
        LineerTamam.l_imha _ _ x τ h_g (hk.1 x h_a),
        lineerKucuk_update_tuketildi hk x⟩
  | l_guvensiz _ _ e _ ih =>
      intro Λn hk
      obtain ⟨Λen, h_e, hk'⟩ := ih Λn hk
      exact ⟨Λen, LineerTamam.l_guvensiz _ _ _ e h_e, hk'⟩
  -- D-332: dallar LineerNotr — yargi Λ'dan BAGIMSIZ oldugu icin aynen
  -- tasinir; cikis kosulun cikisidir, IH onu verir.
  | l_topla _ _ _ a b _ _ ih_a ih_b =>
      intro Λn hk
      obtain ⟨Λan, h_a, hk_a⟩ := ih_a Λn hk
      obtain ⟨Λbn, h_b, hk_b⟩ := ih_b Λan hk_a
      exact ⟨Λbn, LineerTamam.l_topla _ _ _ _ a b h_a h_b, hk_b⟩
  | l_bol _ _ _ a b _ _ ih_a ih_b =>
      intro Λn hk
      obtain ⟨Λan, h_a, hk_a⟩ := ih_a Λn hk
      obtain ⟨Λbn, h_b, hk_b⟩ := ih_b Λan hk_a
      exact ⟨Λbn, LineerTamam.l_bol _ _ _ _ a b h_a h_b, hk_b⟩
  | l_kalan _ _ _ a b _ _ ih_a ih_b =>
      intro Λn hk
      obtain ⟨Λan, h_a, hk_a⟩ := ih_a Λn hk
      obtain ⟨Λbn, h_b, hk_b⟩ := ih_b Λan hk_a
      exact ⟨Λbn, LineerTamam.l_kalan _ _ _ _ a b h_a h_b, hk_b⟩
  -- D-335: l_iken tamamen Λ-bagimsiz (notr yargilar) → aynen tasinir.
  | l_iken _ k g hk hg =>
      intro Λn hkk
      exact ⟨Λn, LineerTamam.l_iken _ _ k g hk hg, hkk⟩
  | l_indeks _ _ x idx τ h_g h_lin _ ih =>
      intro Λn hk
      obtain ⟨Λen, h_e, hk'⟩ := ih Λn hk
      exact ⟨Λen, LineerTamam.l_indeks _ _ _ x idx τ h_g h_lin h_e, hk'⟩
  | l_indeks_ata _ _ _ x idx e τ h_g h_lin _ _ ih_i ih_e =>
      intro Λn hk
      obtain ⟨Λin, h_i, hk_i⟩ := ih_i Λn hk
      obtain ⟨Λen, h_e, hk_e⟩ := ih_e Λin hk_i
      exact ⟨Λen, LineerTamam.l_indeks_ata _ _ _ _ x idx e τ h_g h_lin h_i h_e, hk_e⟩
  | l_esles _ _ s n d y _ h_nd h_ny ih_s =>
      intro Λn hk
      obtain ⟨Λsn, h_s, hk'⟩ := ih_s Λn hk
      exact ⟨Λsn, LineerTamam.l_esles _ _ _ s n d y h_s h_nd h_ny, hk'⟩
  | l_eger _ _ k d y _ h_nd h_ny ih_k =>
      intro Λn hk
      obtain ⟨Λkn, h_k, hk'⟩ := ih_k Λn hk
      exact ⟨Λkn, LineerTamam.l_eger _ _ _ k d y h_k h_nd h_ny, hk'⟩

end Kemgu.Sem.LineerTamam
