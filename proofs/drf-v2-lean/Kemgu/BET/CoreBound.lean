/-
KEMGU BET KOPRUSU — ana modelde (Sem/Core) kosum uzunlugu SINIRI (D-333)
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz, sorry/axiom YOK

═══════════════════════════════════════════════════════════════════════
BU DOSYA NE YAPAR / NE YAPMAZ
═══════════════════════════════════════════════════════════════════════
D-332 (`BET/Boundedness.lean`) BET'i AYRI bir cekirdek hesapta ispatladi;
oradaki acik borc "Sem/Core'a kopru YOK" idi. Bu dosya o koprunun
BUGUN YAPILABILIR parcasini kurar — **ana modelin KENDISI uzerinde**:

  `hatasiz_kosum_siniri` : HatasizZincir n S S' → n ≤ konfOlcu S

Yani ana modelde HATASIZ (fault uretmeyen) bir kosum, baslangic
konfigurasyonundan STATIK olarak hesaplanan `konfOlcu` degerini asamaz.
Sinir girdiden (store/kanal iceriginden) BAGIMSIZDIR — yalnizca thread
ifadelerinin sozdizimsel olcusune bakar.

NEDEN BU BIR SINIR VERIR: `Sem/Core.Ifade`de **DONGU YOK, OZYINELEME YOK,
CAGRI YOK** — her Tamam kurali odakli ifadeyi sozdizimsel olarak KUCULTUR
(`gorevBaslat` bile: kod alt-ifadeye tasinir, sarmalayici kaybolur → net -1).

NE YAPMAZ (durustce):
- **Hatali adimlar HARIC.** Fault kurallari post-state'te YALNIZ `fault`
  alanini degistirir; ifade AYNEN kalir → olcu DUSMEZ ve ayni kural tekrar
  atesleyebilir. Bu yuzden teorem hatasiz zincirler icindir. (Bu bir kusur
  degil, modelin dogru okunmasi: fault sonrasi kosum anlamli degildir.)
- **D-332'nin `max` icerigini TASIMAZ.** `Core.Ifade`de `eger` YOK (olculdu),
  dolayisiyla dal-max muhakemesi burada IFADE EDILEMEZ. O parca, koprunun
  `eger` gerektiren kismidir (D-331 plani; kosul-congruence karari D-331 eki).

BAKIM NOTU: `sEgerKosulCong` + `sEgerSec` Step'e eklendiginde bu dosyadaki
tumevarim +2 case ister (eger: olcu k + max? HAYIR — burada olcu YAPISALDIR,
`olcu (eger k d y) = 1 + olcu k + olcu d + olcu y` yeterli; dal secimi
olcuyu kucultur). Kapi KIRMIZI verecek → sessiz kalmaz.
═══════════════════════════════════════════════════════════════════════
-/

import Kemgu.Sem.Core
import Kemgu.Sem.SmallStep

namespace Kemgu.BET.CoreBound

open Kemgu.Sem.Core Kemgu.Sem.SmallStep

-- ============================================================
-- §1. Sozdizimsel olcu
-- ============================================================

/-- Ifadenin yapisal olcusu. `sabit` = 0 (deger, ilerlemez);
    her ilerletilebilir kurucu en az 1 katkı verir. -/
def olcu : Ifade → Nat
  | .sabit _            => 0
  | .tanim _            => 1
  | .atama _ e          => 1 + olcu e
  | .seq a b            => 1 + olcu a + olcu b
  | .gorevBaslat _ kod  => 1 + olcu kod
  | .gorevBirlestir _   => 1
  | .kanalGonderIf _ _  => 1
  | .kanalAlIf _        => 1
  | .dondurIf _         => 1
  | .kullanIf _         => 1
  | .imhaIf _           => 1
  | .guvensiz e         => 1 + olcu e

def toplam : List ThreadCtx → Nat
  | []      => 0
  | c :: cs => olcu c.ifade + toplam cs

/-- Konfigurasyon olcusu: tum thread'lerin ifade olculeri toplami. -/
def konfOlcu (S : Konfigurasyon) : Nat := toplam S.thread

theorem toplam_append (a b : List ThreadCtx) :
    toplam (a ++ b) = toplam a + toplam b := by
  induction a with
  | nil => simp [toplam]
  | cons c cs ih =>
      show olcu c.ifade + toplam (cs ++ b) = _
      rw [ih]
      show olcu c.ifade + (toplam cs + toplam b)
             = olcu c.ifade + toplam cs + toplam b
      omega

/-- Odakli bolunme: olcu = sol + odak + sag. -/
theorem konfOlcu_split (S : Konfigurasyon) (ts1 ts2 : List ThreadCtx)
    (ctx : ThreadCtx) (h : S.thread = ts1 ++ ctx :: ts2) :
    konfOlcu S = toplam ts1 + olcu ctx.ifade + toplam ts2 := by
  show toplam S.thread = _
  rw [h, toplam_append]
  show toplam ts1 + (olcu ctx.ifade + toplam ts2) = _
  omega

/-- Odak degistirilmis konfigurasyonun olcusu. -/
theorem konfOlcu_ifadeyle (S : Konfigurasyon) (ts1 ts2 : List ThreadCtx)
    (ctx : ThreadCtx) (e : Ifade) :
    konfOlcu (ifadeyleKonf S ts1 ts2 ctx e)
      = toplam ts1 + olcu e + toplam ts2 := by
  show toplam (ts1 ++ { ctx with ifade := e } :: ts2) = _
  rw [toplam_append]
  show toplam ts1 + (olcu e + toplam ts2) = _
  omega

-- ============================================================
-- §2. HATASIZ adim olcuyu KESIN OLARAK azaltir
-- ============================================================

/-- **Cekirdek lemma:** fault uretmeyen her adim, konfigurasyon olcusunu
    STRICT azaltir. Dongu/ozyineleme olmadigi icin her kural odakli
    ifadeyi sozdizimsel olarak kucultur. -/
theorem hatasiz_adim_azaltir (S S' : Konfigurasyon)
    (h : Step S S') (h_nf : S'.fault = none) :
    konfOlcu S' < konfOlcu S := by
  induction h with
  | sVarOku S S' ts1 ts2 ctx x b v h_t h_if h_b h_v h_S' =>
      rw [konfOlcu_split S ts1 ts2 ctx h_t, h_if]
      subst h_S'
      show toplam (ts1 ++ _ :: ts2) < _
      rw [toplam_append]
      show toplam ts1 + (olcu (Ifade.sabit v) + toplam ts2) < _
      simp only [olcu, toplam]; omega
  | sAtamaTamam S S' ts1 ts2 ctx x v b h_t h_if h_b h_owner h_S' =>
      rw [konfOlcu_split S ts1 ts2 ctx h_t, h_if]
      subst h_S'
      show toplam (ts1 ++ _ :: ts2) < _
      rw [toplam_append]
      simp only [olcu, toplam]; omega
  | sSeqAtla S S' ts1 ts2 ctx v b h_t h_if h_S' =>
      rw [konfOlcu_split S ts1 ts2 ctx h_t, h_if]
      subst h_S'
      show toplam (ts1 ++ _ :: ts2) < _
      rw [toplam_append]
      simp only [olcu, toplam]; omega
  | sGuvensizAtla S S' ts1 ts2 ctx v h_t h_if h_S' =>
      rw [konfOlcu_split S ts1 ts2 ctx h_t, h_if]
      subst h_S'
      show toplam (ts1 ++ _ :: ts2) < _
      rw [toplam_append]
      simp only [olcu, toplam]; omega
  | cGorevBirlestirTamam S S' ts1 ts2 ctx g tH rb h_t h_if h_h h_d h_S' =>
      rw [konfOlcu_split S ts1 ts2 ctx h_t, h_if]
      subst h_S'
      show toplam (ts1 ++ _ :: ts2) < _
      rw [toplam_append]
      simp only [olcu, toplam]; omega
  | cKanalGonderTamam S S' ts1 ts2 ctx k vId b v h_t h_if h_b h_v h_o h_bos h_S' =>
      rw [konfOlcu_split S ts1 ts2 ctx h_t, h_if]
      subst h_S'
      show toplam (ts1 ++ _ :: ts2) < _
      rw [toplam_append]
      simp only [olcu, toplam]; omega
  | cKanalAlTamam S S' ts1 ts2 ctx k v tb h_t h_if h_v h_tr h_S' =>
      rw [konfOlcu_split S ts1 ts2 ctx h_t, h_if]
      subst h_S'
      show toplam (ts1 ++ _ :: ts2) < _
      rw [toplam_append]
      simp only [olcu, toplam]; omega
  | cDondurTamam S S' ts1 ts2 ctx b h_t h_if h_o h_S' =>
      rw [konfOlcu_split S ts1 ts2 ctx h_t, h_if]
      subst h_S'
      show toplam (ts1 ++ _ :: ts2) < _
      rw [toplam_append]
      simp only [olcu, toplam]; omega
  | sLinKullanTamam S S' ts1 ts2 ctx x h_t h_if h_a h_S' =>
      rw [konfOlcu_split S ts1 ts2 ctx h_t, h_if]
      subst h_S'
      show toplam (ts1 ++ _ :: ts2) < _
      rw [toplam_append]
      simp only [olcu, toplam]; omega
  | sLinImhaTamam S S' ts1 ts2 ctx x h_t h_if h_a h_S' =>
      rw [konfOlcu_split S ts1 ts2 ctx h_t, h_if]
      subst h_S'
      show toplam (ts1 ++ _ :: ts2) < _
      rw [toplam_append]
      simp only [olcu, toplam]; omega
  | cGorevBaslatTamam S S' ts1 ts2 ctx tY yd kod h_t h_if h_f h_s h_S' =>
      -- Spawn: sarmalayici kaybolur (-1-olcu kod), cocuk thread +olcu kod
      rw [konfOlcu_split S ts1 ts2 ctx h_t, h_if]
      subst h_S'
      show toplam (ts1 ++ _ :: ts2 ++ [_]) < _
      rw [toplam_append, toplam_append]
      show toplam ts1 + (olcu (Ifade.sabit (Deger.gorevVal tY)) + toplam ts2)
             + (olcu kod + 0) < toplam ts1 + olcu (Ifade.gorevBaslat yd kod)
             + toplam ts2
      simp only [olcu, toplam]; omega
  -- ---- Fault kurallari: h_nf ile DISLANIR (post-state fault = some) ----
  | sAtamaHataDonmus S S' ts1 ts2 ctx x v b h_t h_if h_b h_frozen h_S' =>
      subst h_S'; exact absurd h_nf (by simp)
  | sAtamaHataSahipDegil S S' ts1 ts2 ctx x v b h_t h_if h_b h_no h_S' =>
      subst h_S'; exact absurd h_nf (by simp)
  | cGorevBaslatHataLineerIhlal S S' ts1 ts2 ctx yd kod vI h_t h_if h_in h_tuk h_S' =>
      subst h_S'; exact absurd h_nf (by simp)
  | cKanalGonderHataLineerTuket S S' ts1 ts2 ctx k vId h_t h_if h_tuk h_S' =>
      subst h_S'; exact absurd h_nf (by simp)
  | cDondurHataZatenDonmus S S' ts1 ts2 ctx b h_t h_if h_zaten h_S' =>
      subst h_S'; exact absurd h_nf (by simp)
  | sLinKullanHataZatenTuketildi S S' ts1 ts2 ctx x h_t h_if h_tuk h_S' =>
      subst h_S'; exact absurd h_nf (by simp)
  | sLinImhaHataZatenTuketildi S S' ts1 ts2 ctx x h_t h_if h_tuk h_S' =>
      subst h_S'; exact absurd h_nf (by simp)
  -- ---- Congruence: IH ic adimdan gelir ----
  | sAtamaCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' x e e'
      h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      have h_ic : konfOlcu S1' < konfOlcu S1 := by
        apply ih; subst h_S'; exact h_nf
      rw [h_S1, konfOlcu_ifadeyle] at h_ic
      rw [konfOlcu_split S1' ts1 ts2' ctx' h_t1', h_if'] at h_ic
      rw [konfOlcu_split S ts1 ts2 ctx h_t, h_if]
      subst h_S'
      show toplam (ts1 ++ _ :: ts2') < _
      rw [toplam_append]
      simp only [olcu, toplam] at *; omega
  | sSeqCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' a a' b
      h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      have h_ic : konfOlcu S1' < konfOlcu S1 := by
        apply ih; subst h_S'; exact h_nf
      rw [h_S1, konfOlcu_ifadeyle] at h_ic
      rw [konfOlcu_split S1' ts1 ts2' ctx' h_t1', h_if'] at h_ic
      rw [konfOlcu_split S ts1 ts2 ctx h_t, h_if]
      subst h_S'
      show toplam (ts1 ++ _ :: ts2') < _
      rw [toplam_append]
      simp only [olcu, toplam] at *; omega
  | sGuvensizCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' e e'
      h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      have h_ic : konfOlcu S1' < konfOlcu S1 := by
        apply ih; subst h_S'; exact h_nf
      rw [h_S1, konfOlcu_ifadeyle] at h_ic
      rw [konfOlcu_split S1' ts1 ts2' ctx' h_t1', h_if'] at h_ic
      rw [konfOlcu_split S ts1 ts2 ctx h_t, h_if]
      subst h_S'
      show toplam (ts1 ++ _ :: ts2') < _
      rw [toplam_append]
      simp only [olcu, toplam] at *; omega

-- ============================================================
-- §3. KOSUM UZUNLUGU SINIRI (BET'in ana modeldeki karsiligi)
-- ============================================================

/-- Uzunlugu SAYILAN hatasiz kosum zinciri. -/
inductive HatasizZincir : Nat → Konfigurasyon → Konfigurasyon → Prop where
  | bos (S : Konfigurasyon) : HatasizZincir 0 S S
  | adim (n : Nat) (S S1 S' : Konfigurasyon)
      (h : Step S S1) (h_nf : S1.fault = none)
      (rest : HatasizZincir n S1 S') : HatasizZincir (n + 1) S S'

/-- **KOPRU TEOREMI:** ana modelde hatasiz kosum uzunlugu, baslangic
    konfigurasyonunun STATIK olcusunu asamaz. Store/kanal icerigi
    (yani GIRDI) sinirda HIC GECMEZ → sinir girdiden bagimsizdir. -/
theorem hatasiz_kosum_siniri (n : Nat) (S S' : Konfigurasyon)
    (h : HatasizZincir n S S') : n ≤ konfOlcu S := by
  induction h with
  | bos _ => exact Nat.zero_le _
  | adim m S0 S1 _ hstep hnf _ ih =>
      have h_dec := hatasiz_adim_azaltir S0 S1 hstep hnf
      omega

/-- Kagit RT.8 formu (ana model): her konfigurasyon icin TUM hatasiz
    kosumlarda gecerli TEK bir N vardir. -/
theorem core_bet_rt8 (S : Konfigurasyon) :
    ∃ N : Nat, ∀ (n : Nat) (S' : Konfigurasyon), HatasizZincir n S S' → n ≤ N :=
  ⟨konfOlcu S, fun n S' h => hatasiz_kosum_siniri n S S' h⟩

end Kemgu.BET.CoreBound
