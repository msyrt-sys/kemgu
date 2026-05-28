/-
KEMGU DRF Mekanize — Aile 2 Discharge: Fault Impossibility (Plan v2 Adim 8 P1)
Kaynak: belgeler/KEMGU_Mekanize_Onarim_Plan.md §6.2 Aile 2 + §7.2 Adim 8
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz, sorry/axiom YOK

Plan v2 §6.2 Aile 2 Discharge: typed program (Typed + KonfTipliFull) altinda
Step.Hata constructor'lara ulaslamayacagini garanti eden lemma'lar.

V1 (Adim 8 P1) durum — bu dosyada SADECE TAM ISPATLI lemma'lar:
✓ 2 Linear lemma FULL ispatli (sLinKullan, sLinImha) — Plan §5.2.3
  ThreadTipliFull kopru sayesinde (ctx.lineer ↔ Λ uyumu).

P2 hedef — Adim 8 ileri parcalarinda eklenecek lemma'lar:
- typing_excludes_sAtamaHataDonmus (BolgeOrtam ↔ Sahiplik kopru gerek)
- typing_excludes_sAtamaHataSahipDegil (Sahiplik kopru)
- typing_excludes_cDondurHataZatenDonmus (Region invariant)
- typing_excludes_cGorevBaslatHataLineerIhlal (LineerTamam V2 strengthen)
- typing_excludes_cKanalGonderHataLineerTuket (LineerTamam V2 strengthen)

Bu 5 lemma'nin yokluğunda step_fault_preserves_typed Hata case'leri V1'de
sorry kalir (NoFault.lean'de Adim 8 P2 TODO).

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


-- ============================================================
-- §2. Adim 8 P2 hedef — 5 Aile 2 lemma'si eklenecek (V1 sinirlar)
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
  V1 sinir: V1 minimal l_gorev_baslat form yakalama bilgisi tutmaz (V2
  strengthen gerek).

theorem typing_excludes_cKanalGonderHataLineerTuket
    LineerTamam.l_kanal_gonder → vId Linear ise aktif → tuket çelişki.
  V1 sinir: V1 minimal l_kanal_gonder serbest Λ → Λ' (lineer bilgi yok,
  V2 strengthen gerek).
-/


end Kemgu.Discharge.Aile2
