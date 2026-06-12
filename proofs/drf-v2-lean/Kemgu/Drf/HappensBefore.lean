/-
KEMGU DRF Mekanize — Happens-Before iliskisi (Faz V2.1)
Kaynak (kagit formel): belgeler/KEMGU_Operasyonel_Semantik.md §6.3-6.4
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz, sorry/axiom YOK

V2.1 hedefi: Cross-Step DRF Teorem 4' tam form icin altyapi.
-/

import Kemgu.Sem.Core
import Kemgu.Sem.SmallStep
import Kemgu.Drf.Drf

namespace Kemgu.Drf.HappensBefore
open Kemgu.Sem.Core Kemgu.Sem.SmallStep
open Kemgu.Drf.Drf

-- ============================================================
-- §1. Iz pozisyonu yardimcilari
-- ============================================================

/-- Bir olay iz'de pozisyonunda bulunur. iz en yenisi BASTA (cons), bu
    yuzden pozisyon 0 = en yeni, pozisyon n = en eski. -/
def IzPos (iz : Iz) (e : Olay) (i : Nat) : Prop :=
  iz[i]? = some e

/-- Iz pozisyon karsilastirmasi: pozisyon i < pozisyon j → e_j daha eski
    (cons-ordered iz'de). -/
def daha_yeni (iz : Iz) (e1 e2 : Olay) : Prop :=
  ∃ i j, IzPos iz e1 i ∧ IzPos iz e2 j ∧ i < j


-- ============================================================
-- §2. Sequenced-before (≺_pl): ayni thread, iz pozisyonu
-- ============================================================

/-- Sequenced-before: ayni thread'in iki olayi, biri daha yeni iz
    pozisyonunda. (Cons-ordered iz: pozisyon 0 = en yeni.)
    Op.Sem §6.2: "e1 ≺_pl e2 ⟺ e1 ve e2 ayni thread'de ve e1 τ'da
                  daha once" — bizim model'de daha onceki = daha buyuk
                  pozisyon (cons'lu). -/
def sequenced_before (iz : Iz) (e1 e2 : Olay) : Prop :=
  olay_thread e1 = olay_thread e2 ∧ daha_yeni iz e2 e1


-- ============================================================
-- §3. Synchronizes-with (≺_sw): senkronizasyon kurali olaylar
-- ============================================================

/-- Synchronizes-with iliskisi. Op.Sem §6.3 kurallari:
    - kanal_gonder ≺_sw kanal_al (ayni kanal, ayni deger)
    - thread_baslat(t_b) ≺_sw t_b'nin ilk olayi
    - t_b'nin son olayi ≺_sw birlestir(t_a, t_b)
    - dondur(t_a, ρ) ≺_sw mem_oku(t_b ≠ t_a, ρ, _, _) -/
inductive synchronizes_with (iz : Iz) : Olay → Olay → Prop where
  | kanal :
      ∀ (t1 t2 : ThreadId) (k : KanalId) (v : Deger),
      Olay.kanalGonderOl t1 k v ∈ iz →
      Olay.kanalAlOl t2 k v ∈ iz →
      synchronizes_with iz (Olay.kanalGonderOl t1 k v) (Olay.kanalAlOl t2 k v)
  | thread_baslat :
      ∀ (t_new : ThreadId) (e_first : Olay),
      Olay.threadBaslat t_new ∈ iz →
      e_first ∈ iz →
      olay_thread e_first = some t_new →
      synchronizes_with iz (Olay.threadBaslat t_new) e_first
  | thread_birlestir :
      ∀ (t_b : ThreadId) (e_last : Olay),
      e_last ∈ iz →
      olay_thread e_last = some t_b →
      Olay.threadBitir t_b ∈ iz →
      synchronizes_with iz e_last (Olay.threadBitir t_b)
  | dondur_oku :
      ∀ (t_a t_b : ThreadId) (b : Bolge) (k : Konum) (v : Deger),
      t_a ≠ t_b →
      k.bolge = b →
      Olay.dondurOl t_a b ∈ iz →
      Olay.memOku t_b k v ∈ iz →
      synchronizes_with iz (Olay.dondurOl t_a b) (Olay.memOku t_b k v)


-- ============================================================
-- §4. Happens-before (≺_hb): transitive closure of ≺_pl ∪ ≺_sw
-- ============================================================

/-- Happens-before iliskisi: sequenced-before ∪ synchronizes-with'in
    gecisli kapanmasi (Op.Sem §6.4). -/
inductive happens_before (iz : Iz) : Olay → Olay → Prop where
  | base_seq :
      ∀ (e1 e2 : Olay), sequenced_before iz e1 e2 →
      happens_before iz e1 e2
  | base_sync :
      ∀ (e1 e2 : Olay), synchronizes_with iz e1 e2 →
      happens_before iz e1 e2
  | trans :
      ∀ (e1 e2 e3 : Olay),
      happens_before iz e1 e2 →
      happens_before iz e2 e3 →
      happens_before iz e1 e3


-- ============================================================
-- §5. data_race tam form (HB ordering ile)
-- ============================================================

/-- data_race tam form (Op.Sem §6.5):
    iki olay ayni konum, farkli thread, en az biri yazma,
    HB ile siralanmamis. -/
def data_race_tam (S : Konfigurasyon) : Prop :=
  ∃ e1 ∈ S.iz, ∃ e2 ∈ S.iz,
    is_data_race_candidate e1 e2 ∧
    ¬ happens_before S.iz e1 e2 ∧
    ¬ happens_before S.iz e2 e1


end Kemgu.Drf.HappensBefore
