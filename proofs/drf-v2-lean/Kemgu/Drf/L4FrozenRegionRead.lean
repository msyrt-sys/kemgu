/-
KEMGU DRF Mekanize — DRF-L4 Frozen Region Read-Soundness (Faz A3.3)
Kaynak (kagit formel): belgeler/KEMGU_DRF_Lemmalar.md §DRF-L4
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz, sorry/axiom YOK

Onkosul: A3.0'' refactor (sAtama h_not_frozen, commit 9089682)
-/

import Kemgu.Sem.Core
import Kemgu.Sem.SmallStep

namespace Kemgu.Drf.L4FrozenRegionRead
open Kemgu.Sem.Core Kemgu.Sem.SmallStep

-- ============================================================
-- §1. DRF-L4 (a) — Tek-adim formulasyon
-- ============================================================

/-- DRF-L4 (a) Single-step formulation:
    Eger S anindaki bolge `b` frozen ise (isFrozen S b), hicbir Step
    S → S' yeni bir memYaz event'i ekleyemez S'.iz'e ki hedefi `b` olsun.

    Bu kagit ifadesinin "tek adim" versiyonu. Coklu-adim
    versiyon (StepStar uzerinden) induksiyonla bu lemma + persistence
    teoreminin (asagida) kombinasyonu olarak gelistirilir.

    Kanit yapilanmasi: Step'in 8 constructor'i icin case analysis:
    - sAtama: tek memYaz emit eden. h_not_frozen precondition'i devreye
      girer; eger yeni event'in hedefi `b` ise k.bolge = b → isFrozen
      S k.bolge = isFrozen S b = h_frozen, ama h_not_frozen ¬ olmasini
      ister → celiski → right (k.bolge ≠ b).
    - sLinKullan, sLinImha: S'.iz = S.iz (event yok). Event S.iz'de
      olmali (eski).
    - cDondur, cGorevBaslat, cGorevBirlestir, cKanalGonder, cKanalAl:
      yeni event memYaz degil (dondurOl/threadBaslat/threadBitir/
      kanalGonder/kanalAl). Eger memYaz S'.iz'de varsa, tail'da
      olmali (= S.iz). -/
theorem drf_l4_a_step
    (S S' : Konfigurasyon) (h_step : Step S S')
    (b : Bolge) (h_frozen : isFrozen S b)
    (t : ThreadId) (k : Konum) (v : Deger)
    (h_in : Olay.memYaz t k v ∈ S'.iz) :
    Olay.memYaz t k v ∈ S.iz ∨ k.bolge ≠ b := by
  cases h_step with
  | sAtama _ _ _ _ _ k_x _ _ h_not_frozen _ h_iz _ _ _ =>
    rw [h_iz] at h_in
    rcases List.mem_cons.mp h_in with h_head | h_tail
    · -- New event head: Olay.memYaz t k v = Olay.memYaz ctx.tid k_x v_x
      injection h_head with _ h_k _
      right
      intro h_eq
      rw [h_k] at h_eq
      rw [h_eq] at h_not_frozen
      exact h_not_frozen h_frozen
    · left; exact h_tail
  | sLinKullan _ _ _ _ _ _ _ h_iz _ _ _ =>
    rw [h_iz] at h_in
    left; exact h_in
  | sLinImha _ _ _ _ _ _ _ h_iz _ _ _ =>
    rw [h_iz] at h_in
    left; exact h_in
  | cDondur _ _ _ _ _ _ _ h_iz _ _ _ =>
    rw [h_iz] at h_in
    rcases List.mem_cons.mp h_in with h_head | h_tail
    · -- memYaz = dondurOl: imkansiz (farkli constructor)
      nomatch h_head
    · left; exact h_tail
  | cGorevBaslat _ _ _ _ _ _ _ _ _ _ _ _ h_iz _ _ _ =>
    rw [h_iz] at h_in
    rcases List.mem_cons.mp h_in with h_head | h_tail
    · nomatch h_head
    · left; exact h_tail
  | cGorevBirlestir _ _ _ _ _ _ _ _ _ h_iz _ _ _ =>
    rw [h_iz] at h_in
    rcases List.mem_cons.mp h_in with h_head | h_tail
    · nomatch h_head
    · left; exact h_tail
  | cKanalGonder _ _ _ _ _ _ _ _ _ _ h_iz _ _ =>
    rw [h_iz] at h_in
    rcases List.mem_cons.mp h_in with h_head | h_tail
    · nomatch h_head
    · left; exact h_tail
  | cKanalAl _ _ _ _ _ _ _ _ _ h_iz _ _ =>
    rw [h_iz] at h_in
    rcases List.mem_cons.mp h_in with h_head | h_tail
    · nomatch h_head
    · left; exact h_tail


-- ============================================================
-- §2. DRF-L4 (b) — Iki read race olusturmaz (definition trivially)
-- ============================================================

/-- DRF-L4 (b) — Iki mem_oku event'i data race olusturmaz.
    Op.Sem §6.5 data_race tanimi: "(e1 veya e2 mem_yaz)".
    Iki mem_oku event'i bu kosulu karsilamaz → trivially no race.

    Ek olarak bizim modelde mem_oku event'i hicbir Step constructor
    tarafindan emit edilmiyor (S-VAR henuz mekanize edilmedi). Bu yuzden
    (b) ikinci yonden vacuous. -/
theorem drf_l4_b_oku_not_yaz
    (t : ThreadId) (k : Konum) (v : Deger) :
    Olay.memOku t k v ≠ Olay.memYaz t k v := by
  intro h
  nomatch h


-- ============================================================
-- §3. Persistence: isFrozen tek-adim altinda korunur
-- (DRF-L4 StepStar lift'i icin gerekli — Teorem 4'te kullanilir)
-- ============================================================

/-- isFrozen tek-adim altinda korunur: cDondur'dan sonra frozen
    durumu Step iliskisi altinda kaybolmaz.

    Anahtar fikir: isFrozen S b = ∃ z₀ ≤ S.zaman, sahiplikGet (b, z₀) =
    some donmus. Step S → S' altinda:
    1. S'.zaman ≥ S.zaman + 1 ≥ z₀ → z₀ ≤ S'.zaman ✓
    2. sahiplikGet S'.sahiplik (b, z₀) korunur cunku:
       - Sahiplik-degistirmeyen Step (sAtama, sLinKullan, sLinImha):
         S'.sahiplik = S.sahiplik, lookup ayni.
       - Sahiplik-degisticen Step (cDondur, cGorevBaslat, cGorevBirlestir,
         cKanalGonder, cKanalAl): yeni entry'ler hep S.zaman zaman
         damgasinda. z₀ ≤ S.zaman olduguna gore z₀ < S.zaman veya
         z₀ = S.zaman. z₀ < S.zaman ise lookup yeni entry'leri atlar
         (sahiplikSet_ne). z₀ = S.zaman ise farkli bolge ise yine atlar;
         AYNI bolge + AYNI zaman ise — Sahip.donmus zaten cDondur'la
         set edilen, baska bir Step bu key'i baska bir Sahip degerine
         override ediyor (corner case).

    NOT: Bu corner case bizim modelimizde mumkun cunku iki farkli
    Step ayni (b, z) anahtarini override edebilir. Pratikte bu olmaz
    cunku Step'ler zaman damgasini incrementliyor; ama Lean'de garanti
    edilmedi (one-step-per-zaman invariant explicit degil). Tam
    persistence ispati bu invariant'i da gerektirir; biz "single step
    correctness" odakliyiz. StepStar persistence Teorem 4' ispatinda
    ek bir induksiyonla yapilir. -/
theorem isFrozen_persistent_simple
    (S S' : Konfigurasyon) (h_step : Step S S')
    (b : Bolge) (h_frozen : isFrozen S b)
    -- Ek hipotez: S'.zaman = S.zaman + 1 (her Step zaman'i artirir)
    (h_zaman_inc : S'.zaman = S.zaman + 1)
    -- Ek hipotez: hicbir Sahiplik degisikligi (b, z₀) anahtarini override etmez
    -- (Bu, isFrozen icin secili z₀ icin gecerli — cunku z₀ ≤ S.zaman ve
    --  yeni entry'ler S.zaman'da, eger z₀ < S.zaman ise korunur)
    (h_preserve : ∀ z, z ≤ S.zaman →
                  sahiplikGet S'.sahiplik (b, z) = sahiplikGet S.sahiplik (b, z)) :
    isFrozen S' b := by
  obtain ⟨z₀, h_z, h_get⟩ := h_frozen
  refine ⟨z₀, ?_, ?_⟩
  · -- z₀ ≤ S'.zaman
    omega
  · -- sahiplikGet S'.sahiplik (b, z₀) = some donmus
    rw [h_preserve z₀ h_z]
    exact h_get


end Kemgu.Drf.L4FrozenRegionRead
