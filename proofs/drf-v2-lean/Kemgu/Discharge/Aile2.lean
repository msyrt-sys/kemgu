/-
KEMGU DRF Mekanize — Aile 2 Discharge: Fault Impossibility (Plan v2 Adim 8 P1)
Kaynak: belgeler/KEMGU_Mekanize_Onarim_Plan.md §6.2 Aile 2 + §7.2 Adim 8
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz, sorry/axiom YOK

Plan v2 §6.2 Aile 2 Discharge: typed program (Typed + KonfTipliFull) altinda
Step.Hata constructor'lara ulaslamayacagini garanti eden lemma'lar.

V1 (Adim 8 P1 + P2) durum — bu dosyada SADECE TAM ISPATLI lemma'lar:
✓ 3 Linear lemma FULL ispatli (sLinKullan, sLinImha — P1; cKanalGonder — P2)
  — Plan §5.2.3 ThreadTipliFull kopru sayesinde (ctx.lineer ↔ Λ uyumu) +
  P2'de l_kanal_gonder strengthen (`Λ vId ≠ some tuketildi`).

Kalan hedef — Adim 8 P3+/V2 parcalarinda eklenecek lemma'lar:
- typing_excludes_sAtamaHataDonmus (V2: BolgeOrtam ↔ Sahiplik kopru; Ρ runtime'da yok)
- typing_excludes_sAtamaHataSahipDegil (V2: Typed ownership + Sahiplik kopru)
- typing_excludes_cDondurHataZatenDonmus (V2: r_dondur strengthen + isFrozen↔kategori invariant)
- typing_excludes_cGorevBaslatHataLineerIhlal (V2: vIhlal serbest; yd baglantisi yok)

Bu 4 lemma'nin ortak kok nedeni: statik Ρ sabit + runtime degisken-ortami yok.
Tek V2 refactor (Ρ'yu Konfigurasyona tasi + Step'ten gecir) dordunu birden acar.
Yoklukta step_fault_preserves_typed'in 4 Hata case'i V1'de sorry kalir
(NoFault.lean'de TODO).

Onkosul: Adim 1.1-1.3, Adim 2-3 (StateTipli, HasType), Adim 5-6
         (LineerTamam, RegionTamam), Adim 7 (Tamam strengthen, NoFault).
-/

import Kemgu.Sem.Core
import Kemgu.Sem.SmallStep
import Kemgu.Sem.StateTipli
import Kemgu.Sem.HasType
import Kemgu.Sem.LineerTamam
import Kemgu.Sem.RegionTamam

namespace Kemgu.Discharge.Aile2
open Kemgu.Sem.Core Kemgu.Sem.SmallStep Kemgu.Sem.StateTipli
     Kemgu.Sem.HasType Kemgu.Sem.LineerTamam Kemgu.Sem.RegionTamam

-- ============================================================
-- §1. Linear Aile 2 lemma'lari (FULL ispat — Plan §5.2.3 kopru)
-- ============================================================

/-- AILE 2 Linear — typing_excludes_sLinKullanHataZatenTuketildi.

    Plan §6.2 ifadesi: typed (LinearOK) program sLinKullanHataZatenTuketildi
    constructor'ina ulasilamaz.

    Ispat: Typed.lineerOK → l_kullan kuralı → lineerOrtamGet Λ x = some aktif.
    Köprü (h_bridge, KonfTipliFull.ThreadTipliFull'dan): ctx.lineer ↔ Λ.
    h_tuket: (x, tuketildi) ∈ ctx.lineer → Λ x = some tuketildi.
    Iki Λ x değeri (aktif vs tuketildi) → çelişki via nomatch. -/
theorem typing_excludes_sLinKullanHataZatenTuketildi
    (Γ : TipOrtam) (Λ : LineerOrtam) (Ρ : BolgeOrtam)
    (x : VarId) (τ : Tip) (Λ' : LineerOrtam) (Ρ' : BolgeOrtam)
    (h_typed : Typed Γ Λ Ρ (Ifade.kullanIf x) τ Λ' Ρ')
    (ctx : ThreadCtx)
    (h_bridge : ∀ y : VarId, ∀ lin : Lineerlik,
                  (y, lin) ∈ ctx.lineer ↔ lineerOrtamGet Λ y = some lin)
    (h_tuket : (x, Lineerlik.tuketildi) ∈ ctx.lineer) :
    False := by
  -- Typed.lineerOK extracted
  have h_lineerOK := h_typed.lineerOK
  -- LineerTamam Γ Λ (kullanIf x) Λ' yalnız l_kullan ile saglanir
  cases h_lineerOK with
  | l_kullan _ _ _ _ h_aktif =>
    -- h_aktif : lineerOrtamGet Λ x = some Lineerlik.aktif
    -- Pattern: Λ x τ h_tip h_aktif (Γ outer'dan auto-bind)
    have h_tuket_Λ := (h_bridge x Lineerlik.tuketildi).mp h_tuket
    -- h_aktif = some aktif vs h_tuket_Λ = some tuketildi → çelişki
    rw [h_tuket_Λ] at h_aktif
    nomatch h_aktif

/-- AILE 2 Linear — typing_excludes_sLinImhaHataZatenTuketildi.
    Aynı pattern (l_imha kuralı, sLinKullan ile simetrik). -/
theorem typing_excludes_sLinImhaHataZatenTuketildi
    (Γ : TipOrtam) (Λ : LineerOrtam) (Ρ : BolgeOrtam)
    (x : VarId) (τ : Tip) (Λ' : LineerOrtam) (Ρ' : BolgeOrtam)
    (h_typed : Typed Γ Λ Ρ (Ifade.imhaIf x) τ Λ' Ρ')
    (ctx : ThreadCtx)
    (h_bridge : ∀ y : VarId, ∀ lin : Lineerlik,
                  (y, lin) ∈ ctx.lineer ↔ lineerOrtamGet Λ y = some lin)
    (h_tuket : (x, Lineerlik.tuketildi) ∈ ctx.lineer) :
    False := by
  have h_lineerOK := h_typed.lineerOK
  cases h_lineerOK with
  | l_imha _ _ _ _ h_aktif =>
    have h_tuket_Λ := (h_bridge x Lineerlik.tuketildi).mp h_tuket
    rw [h_tuket_Λ] at h_aktif
    nomatch h_aktif

/-- AILE 2 Linear — typing_excludes_cKanalGonderHataLineerTuket (Adim 8 P2).

    Plan §6.2 ifadesi: typed (LinearOK) program cKanalGonderHataLineerTuket
    constructor'ina ulasilamaz (tuketilmis lineer deger kanala gonderilemez).

    Ispat: Typed.lineerOK → l_kanal_gonder kuralı (Adim 8 P2 strengthen) →
    `lineerOrtamGet Λ vId ≠ some tuketildi`.
    Köprü (h_bridge, ThreadTipliFull'dan): (vId, tuketildi) ∈ ctx.lineer →
    lineerOrtamGet Λ vId = some tuketildi.
    Iki bilgi dogrudan çelişir: h_notconsumed h_tuket_Λ : False.

    NOT: vId burada SERBEST degil — Step kuralı `ctx.ifade = kanalGonderIf k vId`
    ile gonderilen degiskeni ifadeye baglar; bu yuzden sLinKullan/Imha ile ayni
    temiz pattern uygulanir (sAtama/cGorevBaslat'ta bu baglanti yok → V2). -/
theorem typing_excludes_cKanalGonderHataLineerTuket
    (Γ : TipOrtam) (Λ : LineerOrtam) (Ρ : BolgeOrtam)
    (k : KanalId) (vId : VarId) (τ : Tip) (Λ' : LineerOrtam) (Ρ' : BolgeOrtam)
    (h_typed : Typed Γ Λ Ρ (Ifade.kanalGonderIf k vId) τ Λ' Ρ')
    (ctx : ThreadCtx)
    (h_bridge : ∀ y : VarId, ∀ lin : Lineerlik,
                  (y, lin) ∈ ctx.lineer ↔ lineerOrtamGet Λ y = some lin)
    (h_tuket : (vId, Lineerlik.tuketildi) ∈ ctx.lineer) :
    False := by
  have h_lineerOK := h_typed.lineerOK
  cases h_lineerOK with
  | l_kanal_gonder _ _ _ _ h_notconsumed =>
    -- h_notconsumed : lineerOrtamGet Λ vId ≠ some Lineerlik.tuketildi
    have h_tuket_Λ := (h_bridge vId Lineerlik.tuketildi).mp h_tuket
    exact h_notconsumed h_tuket_Λ


-- ============================================================
-- §2. Adim 8 P2/V2 hedef — kalan 4 Aile 2 lemma'si (V1 sinirlar)
-- ============================================================

/-
P2 hedef Aile 2 lemma'lari (Adim 8 ileri parca):

theorem typing_excludes_sAtamaHataDonmus
    (Γ Λ Ρ) (x e τ Λ' Ρ')
    (h_typed : Typed Γ Λ Ρ (Ifade.atama x e) τ Λ' Ρ')
    (S : Konfigurasyon) (h_config : KonfTipliFull Γ Λ Ρ S)
    (k : Konum) (h_frozen : isFrozen S k.bolge) :
    False
  V1 sinir: BolgeOrtam ↔ Sahiplik kopru gerek. SahiplikTutarli'ye
  bolge.kategori invariant eklenmeli (V2.0 Sahiplik refactor).
  Typed.regionOK r_atama → bolgeOrtamGet Ρ x = some b, b.kategori ≠ donmus
  → isFrozen S b iff b.kategori = donmus → çelişki.

theorem typing_excludes_sAtamaHataSahipDegil
    Benzer pattern: Typed + KonfTipliFull → ctx.tid sahip kanit.
  V1 sinir: Typed'a "ctx sahip bolge atamasi" sarti eklenmeli.

theorem typing_excludes_cDondurHataZatenDonmus
    Typed.regionOK r_dondur → b kategori ≠ donmus (yeni dondurma) → çelişki.
  V1 sinir: r_dondur kurali zaten frozen değil sartı içermez (eklenmeli).

theorem typing_excludes_cGorevBaslatHataLineerIhlal
    LineerTamam.l_gorev_baslat → yakalama lineer tuketim sartı → vIhlal
    aktif olamaz.
  V1 sinir: V1 minimal l_gorev_baslat form yakalama bilgisi tutmaz + vIhlal
  Step kuralinda serbest (yd'ye bagli degil) → V2 (Ρ runtime + yd baglantisi).

NOT: typing_excludes_cKanalGonderHataLineerTuket Adim 8 P2'de §1'de FULL
ispatlandi (l_kanal_gonder strengthen `Λ vId ≠ some tuketildi` + kopru).
Burada cKanalGonder temiz cunku gonderilen vId ifadeye bagli; sAtama/cGorevBaslat
serbest k/vIhlal'den dolayi ayni temiz pattern'i alamaz (V2 kok neden ortak).
-/


end Kemgu.Discharge.Aile2
