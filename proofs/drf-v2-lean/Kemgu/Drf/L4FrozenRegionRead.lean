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

    Bu kagit ifadesinin "tek adim" versiyonu. Coklu-adim (StepStar
    uzerinden) induksiyonla bu lemma + persistence kombinasyonu olarak
    gelistirilir.

    Kanit yapilanmasi: Step'in 8 constructor'i icin case analysis:
    - sAtama: tek memYaz emit eden. h_not_frozen precondition'i devreye
      girer; eger yeni event'in hedefi `b` ise k.bolge = b → isFrozen
      S k.bolge = isFrozen S b = h_frozen, ama h_not_frozen ¬ ister
      → celiski → right.
    - sLinKullan, sLinImha: S'.iz = S.iz (event yok). Event S.iz'de
      olmali (eski).
    - cDondur, cGorevBaslat, cGorevBirlestir, cKanalGonder, cKanalAl:
      yeni event memYaz degil (dondurOl/threadBaslat/threadBitir/
      kanalGonder/kanalAl). Eger memYaz S'.iz'de varsa, tail'da
      olmali (= S.iz). nomatch ile constructor differentiability. -/
theorem drf_l4_a_step
    (S S' : Konfigurasyon) (h_step : Step S S')
    (b : Bolge) (h_frozen : isFrozen S b)
    (t : ThreadId) (k : Konum) (v : Deger)
    (h_in : Olay.memYaz t k v ∈ S'.iz) :
    Olay.memYaz t k v ∈ S.iz ∨ k.bolge ≠ b := by
  cases h_step with
  -- sAtamaTamam (Plan v2 Adim 1.2 rename): 13 pattern positions
  -- ctx x v k h_in h_ifade h_not_frozen h_owner h_store h_iz h_zaman h_sahip h_kanal
  | sAtamaTamam _ _ _ k_x _ _ h_not_frozen _ _ h_iz _ _ _ =>
    rw [h_iz] at h_in
    rcases List.mem_cons.mp h_in with h_head | h_tail
    · injection h_head with _ h_k _
      right
      intro h_eq
      rw [h_k] at h_eq
      rw [h_eq] at h_not_frozen
      exact h_not_frozen h_frozen
    · left; exact h_tail
  -- Plan v2 Adim 7: Hata constructor'lar strengthen edildi (h_iz : S'.iz = S.iz)
  -- Fault non-observable (Plan §4.4) sayesinde her Hata case trivial:
  -- h_in (memYaz ∈ S'.iz) + rw h_iz → h_in (memYaz ∈ S.iz) → left.
  | sAtamaHataDonmus _ _ _ _ _ _ _ _ _ h_iz _ _ _ _ =>
    rw [h_iz] at h_in
    left; exact h_in
  | sAtamaHataSahipDegil _ _ _ _ _ _ _ _ _ h_iz _ _ _ _ =>
    rw [h_iz] at h_in
    left; exact h_in
  | sLinKullanTamam _ _ _ _ _ h_iz _ _ _ =>
    rw [h_iz] at h_in
    left; exact h_in
  | sLinKullanHataZatenTuketildi _ _ _ _ _ _ _ h_iz _ _ _ =>
    rw [h_iz] at h_in
    left; exact h_in
  | sLinImhaTamam _ _ _ _ _ h_iz _ _ _ =>
    rw [h_iz] at h_in
    left; exact h_in
  | sLinImhaHataZatenTuketildi _ _ _ _ _ _ _ h_iz _ _ _ =>
    rw [h_iz] at h_in
    left; exact h_in
  | cDondurTamam _ _ _ _ _ h_iz _ _ _ =>
    rw [h_iz] at h_in
    rcases List.mem_cons.mp h_in with h_head | h_tail
    · nomatch h_head
    · left; exact h_tail
  | cDondurHataZatenDonmus _ _ _ _ _ _ _ h_iz _ _ _ =>
    rw [h_iz] at h_in
    left; exact h_in
  | cGorevBaslatTamam _ _ _ _ _ _ _ _ _ _ _ _ h_iz _ _ _ =>
    rw [h_iz] at h_in
    rcases List.mem_cons.mp h_in with h_head | h_tail
    · nomatch h_head
    · left; exact h_tail
  | cGorevBaslatHataLineerIhlal _ _ _ _ _ _ _ _ _ _ h_iz _ _ _ _ =>
    rw [h_iz] at h_in
    left; exact h_in
  | cGorevBirlestirTamam _ _ _ _ _ _ _ _ h_iz _ _ _ =>
    rw [h_iz] at h_in
    rcases List.mem_cons.mp h_in with h_head | h_tail
    · nomatch h_head
    · left; exact h_tail
  | cKanalGonderTamam _ _ _ _ _ _ _ _ _ h_iz _ _ =>
    rw [h_iz] at h_in
    rcases List.mem_cons.mp h_in with h_head | h_tail
    · nomatch h_head
    · left; exact h_tail
  | cKanalGonderHataLineerTuket _ _ _ _ _ _ _ _ h_iz _ _ _ =>
    rw [h_iz] at h_in
    left; exact h_in
  | cKanalAlTamam _ _ _ _ _ _ _ _ h_iz _ _ =>
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
-- (DRF-L4 StepStar lift'i icin gerekli — Teorem 4' kullanir)
-- ============================================================

/-- isFrozen tek-adim altinda korunur (gerekli hipotezlerle): cDondur'dan
    sonra frozen durumu Step iliskisi altinda kaybolmaz.

    Anahtar fikir: isFrozen S b = ∃ z₀ ≤ S.zaman, sahiplikGet (b, z₀) =
    some donmus. Step S → S' altinda:
    1. S'.zaman ≥ S.zaman → z₀ ≤ S'.zaman (h_zaman_inc gerek).
    2. sahiplikGet S'.sahiplik (b, z₀) korunur (h_preserve gerek).

    NOT: Tam persistence ispati her constructor icin case analysis +
    sahiplikSet/sahiplikSetMany lookup preservation lemma'lari gerektirir
    (300+ satir tahmini). Tikanma politikasi uyarinca burada conditional
    formda (ek hipotezlerle) duruyor; Teorem 4' ispatinda gerektigi
    yerde insolved.

    Bu form "ihtiyac kadar" — DRF-L4 (a) ile birlikte yeterli ana
    StepStar uplift'i icin. -/
theorem isFrozen_persistent_simple
    (S S' : Konfigurasyon) (_h_step : Step S S')
    (b : Bolge) (h_frozen : isFrozen S b)
    (h_zaman_inc : S'.zaman = S.zaman + 1)
    (h_preserve : ∀ z, z ≤ S.zaman →
                  sahiplikGet S'.sahiplik (b, z) = sahiplikGet S.sahiplik (b, z)) :
    isFrozen S' b := by
  obtain ⟨z₀, h_z, h_get⟩ := h_frozen
  refine ⟨z₀, ?_, ?_⟩
  · -- z₀ ≤ S'.zaman
    rw [h_zaman_inc]
    exact Nat.le_succ_of_le h_z
  · -- sahiplikGet S'.sahiplik (b, z₀) = some donmus
    rw [h_preserve z₀ h_z]
    exact h_get


end Kemgu.Drf.L4FrozenRegionRead
