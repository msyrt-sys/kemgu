/-
KEMGU BET Mekanize — Bounded Execution Time (D-332)
Kaynak (kagit formel): belgeler/KEMGU_Realtime_Spec_V1.md §RT.8 + src/wcet.c
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz, sorry/axiom YOK

═══════════════════════════════════════════════════════════════════════
KAPSAM — ONCE BUNU OKU (durustluk paketi)
═══════════════════════════════════════════════════════════════════════
Bu dosya, D-330'daki CT hesabiyla AYNI deseni izler: RT disiplininin
ASIL ICERIGINI kendi icinde TAM bir cekirdek hesapta ispatlar. Ana model
(`Sem/Core`) DOKUNULMAZ; kopru ayri istir (bkz. KOPRU YUKUMLULUGU).

NE ISPATLANIYOR:
  `bet`: gercekzamanli cekirdek dilde HER kosum, STATIK olarak hesaplanan
  `wcet e` sinirini asmaz — **girdiden bagimsiz olarak**:
      Calis s e s' v n  →  n ≤ wcet e
  `n` gercek maliyet (calisan adim sayisi), `wcet e` sozdiziminden
  hesaplanan ust sinir. `bet_rt8` bunu kagit formuna getirir:
  ∃N, ∀giris: maliyet ≤ N  (N := wcet e).

  `bet_dal_max_gerekli`: `eger` icin sinir DALLARIN MAKSIMUMU olmak
  ZORUNDA — "yalniz dogru dali say" tanimi YANLIS olurdu (somut
  karsi-ornek ispatlanir). Yani wcet'teki `max` keyfi degildir.

NEDEN SINIR VAR (kritik durustluk notu):
  Cunku bu cekirdek dilde **DONGU YOK** ve **OZYINELEME YOK** — kagit
  RT002 (gercekzamanli govdede `iken`/`icin` V1'de YASAK) ve RT003
  (oz-ozyineleme yasak) sozdizimine GOMULU. Sinirin varligi bu
  kisitlarin SONUCUDUR; "KEMGU genel olarak sinirlidir" DEMEZ.
  RT001 (dinamik tahsis) de sozdizimsel: dizi/lambda kurucusu YOK.
  Bu kisitlar C tarafinda src/wcet.c'de RT001-RT005 ile ZORLANIR.

KOPRU YUKUMLULUGU (acik borc):
  Bu hesap ile `Sem/Core` arasinda gomme/simulasyon YOKTUR. Yani
  "KEMGU'nun gercekzamanli islevleri sinirlidir" SONUCU BURADAN CIKMAZ;
  cikan sonuc "RT disiplini (dongusuz + tahsissiz), statik WCET sinirini
  GARANTI EDER"dir. Kopru = CT'deki ile ayni sinif is (bkz. D-331 plani).
═══════════════════════════════════════════════════════════════════════
-/

namespace Kemgu.BET.Boundedness

-- ============================================================
-- §1. Gercekzamanli cekirdek sozdizimi
--     DONGU YOK / OZYINELEME YOK / TAHSIS YOK (RT001-RT003 gomulu)
-- ============================================================

abbrev Ad := Nat
abbrev Store := Ad → Int

def yaz (s : Store) (x : Ad) (v : Int) : Store :=
  fun y => if y = x then v else s y

inductive Ifade : Type where
  | sabit    (n : Int)
  | degisken (x : Ad)
  | topla    (a b : Ifade)
  | atama    (x : Ad) (e : Ifade)
  | sira     (a b : Ifade)
  | eger     (k d y : Ifade)

-- ============================================================
-- §2. MALIYET SAYAN semantik (n = harcanan adim/cycle)
-- ============================================================

/-- `Calis s e s' v n`: `e`, `s` store'unda calisir; sonuc store `s'`,
    deger `v`, HARCANAN MALIYET `n`. Her temel islem 1 birim. -/
inductive Calis : Store → Ifade → Store → Int → Nat → Prop where
  | c_sabit (s : Store) (n : Int) :
      Calis s (.sabit n) s n 1
  | c_degisken (s : Store) (x : Ad) :
      Calis s (.degisken x) s (s x) 1
  | c_topla (s s1 s2 : Store) (a b : Ifade) (v1 v2 : Int) (n1 n2 : Nat)
      (h1 : Calis s a s1 v1 n1) (h2 : Calis s1 b s2 v2 n2) :
      Calis s (.topla a b) s2 (v1 + v2) (n1 + n2 + 1)
  | c_atama (s s1 : Store) (x : Ad) (e : Ifade) (v : Int) (n : Nat)
      (h : Calis s e s1 v n) :
      Calis s (.atama x e) (yaz s1 x v) v (n + 1)
  | c_sira (s s1 s2 : Store) (a b : Ifade) (v1 v2 : Int) (n1 n2 : Nat)
      (h1 : Calis s a s1 v1 n1) (h2 : Calis s1 b s2 v2 n2) :
      Calis s (.sira a b) s2 v2 (n1 + n2)
  | c_eger_dogru (s s1 s2 : Store) (k d y : Ifade) (vk vd : Int) (nk nd : Nat)
      (hk : Calis s k s1 vk nk) (h_dogru : vk ≠ 0)
      (hd : Calis s1 d s2 vd nd) :
      Calis s (.eger k d y) s2 vd (nk + nd + 1)
  | c_eger_yanlis (s s1 s2 : Store) (k d y : Ifade) (vk vy : Int) (nk ny : Nat)
      (hk : Calis s k s1 vk nk) (h_yanlis : vk = 0)
      (hy : Calis s1 y s2 vy ny) :
      Calis s (.eger k d y) s2 vy (nk + ny + 1)

-- ============================================================
-- §3. STATIK WCET — sozdiziminden hesaplanan ust sinir
-- ============================================================

/-- Sozdizimsel en-kotu-durum maliyeti. `eger` dalinda **max**:
    hangi dalin kosacagi GIRDIYE baglidir, sinir ikisini de kapsamali. -/
def wcet : Ifade → Nat
  | .sabit _    => 1
  | .degisken _ => 1
  | .topla a b  => wcet a + wcet b + 1
  | .atama _ e  => wcet e + 1
  | .sira a b   => wcet a + wcet b
  | .eger k d y => wcet k + Nat.max (wcet d) (wcet y) + 1

-- ============================================================
-- §4. ANA TEOREM — BET (girdiden BAGIMSIZ ust sinir)
-- ============================================================

/-- **BET:** gercek maliyet, statik WCET sinirini ASMAZ.
    `s` (girdi store'u) SERBEST → sinir TUM girdiler icin gecerlidir. -/
theorem bet : ∀ (e : Ifade) (s s' : Store) (v : Int) (n : Nat),
    Calis s e s' v n → n ≤ wcet e := by
  intro e
  induction e with
  | sabit m =>
      intro s s' v n h; cases h; exact Nat.le_refl 1
  | degisken x =>
      intro s s' v n h; cases h; exact Nat.le_refl 1
  | topla a b iha ihb =>
      intro s s' v n h
      cases h with
      | c_topla _ sa _ _ _ va vb na nb hA hB =>
          exact Nat.add_le_add
            (Nat.add_le_add (iha _ _ _ _ hA) (ihb _ _ _ _ hB)) (Nat.le_refl 1)
  | atama x e ih =>
      intro s s' v n h
      cases h with
      | c_atama _ sa _ _ va na hE =>
          exact Nat.add_le_add (ih _ _ _ _ hE) (Nat.le_refl 1)
  | sira a b iha ihb =>
      intro s s' v n h
      cases h with
      | c_sira _ sa _ _ _ va vb na nb hA hB =>
          exact Nat.add_le_add (iha _ _ _ _ hA) (ihb _ _ _ _ hB)
  | eger k d y ihk ihd ihy =>
      intro s s' v n h
      cases h with
      | c_eger_dogru _ sk _ _ _ _ vk vd nk nd hK hDogru hD =>
          exact Nat.add_le_add
            (Nat.add_le_add (ihk _ _ _ _ hK)
              (Nat.le_trans (ihd _ _ _ _ hD) (Nat.le_max_left _ _)))
            (Nat.le_refl 1)
      | c_eger_yanlis _ sk _ _ _ _ vk vy nk ny hK hYanlis hY =>
          exact Nat.add_le_add
            (Nat.add_le_add (ihk _ _ _ _ hK)
              (Nat.le_trans (ihy _ _ _ _ hY) (Nat.le_max_right _ _)))
            (Nat.le_refl 1)

-- ============================================================
-- §5. `max`in GEREKLILIGI — "yalniz dogru dali say" YANLIS olurdu
-- ============================================================

/-- Hatali WCET tanimi: `eger`de yalniz DOGRU dali sayar. -/
def wcetYanlis : Ifade → Nat
  | .sabit _    => 1
  | .degisken _ => 1
  | .topla a b  => wcetYanlis a + wcetYanlis b + 1
  | .atama _ e  => wcetYanlis e + 1
  | .sira a b   => wcetYanlis a + wcetYanlis b
  | .eger k d _ => wcetYanlis k + wcetYanlis d + 1     -- YANLIS: y yok sayildi

/-- **TANIK:** `wcetYanlis` bir ust sinir DEGILDIR — yanlis dal daha
    pahaliysa gercek maliyet sinirini ASAR. Yani §3'teki `max` keyfi
    degil, BET'in DOGRULUGU icin gereklidir. -/
theorem bet_dal_max_gerekli :
    ∃ (e : Ifade) (s s' : Store) (v : Int) (n : Nat),
      Calis s e s' v n ∧ wcetYanlis e < n := by
  -- e = eger (sabit 0) (sabit 7) (topla (sabit 1) (sabit 2))
  -- kosul 0 → YANLIS dal kosar (maliyet 3); wcetYanlis yalniz dogru dali
  -- (maliyet 1) sayar → 1+1+1 = 3 < gercek 1+3+1 = 5.
  refine ⟨.eger (.sabit 0) (.sabit 7) (.topla (.sabit 1) (.sabit 2)),
          (fun _ => 0), (fun _ => 0), 3, 5, ?_, ?_⟩
  · exact Calis.c_eger_yanlis _ _ _ _ _ _ 0 3 1 3
      (Calis.c_sabit _ 0) rfl
      (Calis.c_topla _ _ _ _ _ 1 2 1 1 (Calis.c_sabit _ 1) (Calis.c_sabit _ 2))
  · decide

-- ============================================================
-- §6. Kagit RT.8 formu — "∃N, ∀giris"
-- ============================================================

/-- Kagit ifadesi: her gercekzamanli `e` icin, TUM girdilerde gecerli
    TEK bir N vardir (N := wcet e — `bet`in dogrudan sonucu). -/
theorem bet_rt8 (e : Ifade) :
    ∃ N : Nat, ∀ (s s' : Store) (v : Int) (n : Nat),
      Calis s e s' v n → n ≤ N :=
  ⟨wcet e, fun s s' v n h => bet e s s' v n h⟩

end Kemgu.BET.Boundedness
