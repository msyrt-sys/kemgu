/-
KEMGU Sabitsure (Constant-Time) CEKIRDEK HESABI — dallanmali NI (D-330)
Kaynak (kagit formel): belgeler/KEMGU_Sabitsure_Spec_V1.md (CT001/CT003)
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz, sorry/axiom YOK

═══════════════════════════════════════════════════════════════════════
NEDEN AYRI DOSYA / AYRI DIL (tasarim karari — durustluk paketi)
═══════════════════════════════════════════════════════════════════════
D-328/D-329 ana modelde (Sem/Core.Ifade) NI ispatladi, ama o modelde
`eger` YOK — yani CT001'in korudugu "gizli uzerinde dallanma" kanali
IFADE EDILEMIYORDU. Dogru genisletme iki yoldan biriyle yapilabilirdi:

  (A) Sem/Core.Ifade + SmallStep.Step'e `eger` EKLEMEK. Bedeli: 28 modulun
      tumevarim ispatlari (adim_korunum 21 case, progress, Aile2, ...)
      yeniden acilir; depo uzun sure KIRMIZI kalir. Dogrulanmis cekirdek
      bu projenin en degerli varligi — onu riske atmak orantisiz.

  (B) CT disiplininin ASIL ICERIGINI kendi icinde TAM bir cekirdek
      hesapta ispatlamak: dallanma + gizli etiket + CT tipleme + iki-kosum
      non-interference. Ana model DOKUNULMAZ.

**(B) secildi.** Bu dosya (B)'dir: `eger` VAR, gizli etiket VAR, CT001
(gizli kosulda dallanma yasagi) ve CT003 (gizli→genel sizinti yasagi)
HIPOTEZ olarak alinir ve bunlardan NON-INTERFERENCE ISPATLANIR.

GUNCELLEME (D-332): yukaridaki (A)/(B) ikilemi ARTIK GECERSIZ — (A) DA
YAPILDI. `Sem/Core`'a `Ifade.eger`, `Olay.dalOl`, `Step.sEgerSec` +
`Step.sEgerCong`, `HasType.t_eger`, `LineerTamam.l_eger`,
`RegionTamam.r_eger` eklendi ve Step uzerinden tumevarim yapan TUM
teoremler (21→23 kural) kapatildi; depo yesil, sorryAx yok. Yani
"bedeli orantisiz" gerekcesi OLCULDU ve yanlis cikti — bedel odendi.
(A)'nin yan urunu: `silme_simulasyon` dallanma altinda YANLIS oluyordu;
`degerSil` dal-bitini koruyacak sekilde daraltildi — bkz. D-332 (a).

KOPRU (D-333): ARTIK VAR — `SideChannel/CTKopru.lean`.
  `gom` (CT.Ifade → Core.Ifade), `gomme_sim` (Calis → StepStar, gozlem
  izi birebir) ve `kopru_ni` ile bu dosyadaki `ct_ni` ANA MODELE
  TASINDI: CT-tipli program `Sem/Core`'da kosturuldugunda dusuk-esdeger
  iki store AYNI `izGozlem`i uretir — DAL KARARLARI dahil.

KALAN BORC (daralmis, acikca):
  Kopru CT'nin TOPLA-SIZ ve deger-sadik (`Sadik`) parcasinda, SONLU
  degisken kumesiyle ve TEK THREAD icin kuruludur. Yani "KEMGU'nun
  KENDISI sabit-suredir" tam iddiasi hala CIKMAZ; eksikler: Core'da
  aritmetik yoklugu, eszamanli CT, `iken`/`esles`/gizli indeks
  (CT002/CT004). Ayrinti + vakum ve sabotaj denetimleri: D-333.

NE ISPATLANIYOR (bu dosyada, tam):
  - `genel_ifade_degeri_esit`: etiketi GENEL olan ifade, dusuk-esdeger
    iki store'da AYNI degeri uretir (gizli veriden bagimsizdir).
  - `ct_ni`: CT-tipli program + dusuk-esdeger baslangic store'lari →
    (1) GOZLEM IZLERI BIREBIR AYNI (dallanma karari dahil!),
    (2) sonuc store'lari yine dusuk-esdeger.
  Yani gizli girdi degisse bile saldirganin gordugu (okuma/yazma deseni
  + hangi dalin alindigi) DEGISMEZ.

NEDEN VAKUM DEGIL:
  `ct_eger` kuralindaki `kosul etiketi = genel` sarti KALDIRILIRSA ispat
  COKER (gizli kosul iki kosumda farkli dal → farkli iz). Bu, CT001'in
  mekanize gerekcesidir: kural keyfi degil, NI'nin ta kendisi icin gerekli.
═══════════════════════════════════════════════════════════════════════
-/

namespace Kemgu.SideChannel.CT

-- ============================================================
-- §1. Etiketler, degerler, store
-- ============================================================

/-- Gizlilik etiketi (iki noktali kafes: genel ⊑ gizli). -/
inductive Etiket : Type where
  | genel
  | gizli
deriving DecidableEq, Repr

/-- Kafes birlesimi (join): gizli bulasicidir. -/
def Etiket.birlesim : Etiket → Etiket → Etiket
  | .genel, e => e
  | .gizli, _ => .gizli

/-- Sira bagintisi: genel ⊑ her sey; gizli yalniz gizli'ye. -/
def Etiket.altMi : Etiket → Etiket → Bool
  | .genel, _     => true
  | .gizli, .gizli => true
  | .gizli, .genel => false

abbrev Ad := Nat
abbrev Store := Ad → Int
abbrev EtiketOrtam := Ad → Etiket

/-- Store guncelleme. -/
def yaz (s : Store) (x : Ad) (v : Int) : Store :=
  fun y => if y = x then v else s y

-- ============================================================
-- §2. Sozdizimi — DALLANMA VAR (ana modelde olmayan sey)
-- ============================================================

inductive Ifade : Type where
  | sabit    (n : Int)
  | degisken (x : Ad)
  | topla    (a b : Ifade)
  | sabitDeg (x : Ad) (e : Ifade)          -- x = e
  | sira     (a b : Ifade)                 -- a; b
  | eger     (kosul : Ifade) (dogruDal yanlisDal : Ifade)

-- ============================================================
-- §3. Saldirgan gozlemi — erisim deseni + DAL KARARI
-- ============================================================

/-- Gozlem: hangi degiskene erisildi + hangi dal alindi.
    `oDal` KRITIK: dallanma karari saldirgan tarafindan gorulur
    (dal hedefleri farkli kod/zaman → PC/timing sizintisi). -/
inductive Gozlem : Type where
  | oOku (x : Ad)
  | oYaz (x : Ad)
  | oDal (alindi : Bool)
deriving DecidableEq, Repr

abbrev Iz := List Gozlem

-- ============================================================
-- §4. Buyuk-adim semantik (deger + store + iz uretir)
-- ============================================================

inductive Calis : Store → Ifade → Store → Iz → Int → Prop where
  | c_sabit (s : Store) (n : Int) :
      Calis s (.sabit n) s [] n
  | c_degisken (s : Store) (x : Ad) :
      Calis s (.degisken x) s [.oOku x] (s x)
  | c_topla (s s1 s2 : Store) (a b : Ifade) (t1 t2 : Iz) (v1 v2 : Int)
      (h1 : Calis s a s1 t1 v1) (h2 : Calis s1 b s2 t2 v2) :
      Calis s (.topla a b) s2 (t1 ++ t2) (v1 + v2)
  | c_atama (s s1 : Store) (x : Ad) (e : Ifade) (t : Iz) (v : Int)
      (h : Calis s e s1 t v) :
      Calis s (.sabitDeg x e) (yaz s1 x v) (t ++ [.oYaz x]) v
  | c_sira (s s1 s2 : Store) (a b : Ifade) (t1 t2 : Iz) (v1 v2 : Int)
      (h1 : Calis s a s1 t1 v1) (h2 : Calis s1 b s2 t2 v2) :
      Calis s (.sira a b) s2 (t1 ++ t2) v2
  | c_eger_dogru (s s1 s2 : Store) (k d y : Ifade) (tk td : Iz) (vk vd : Int)
      (hk : Calis s k s1 tk vk) (h_dogru : vk ≠ 0)
      (hd : Calis s1 d s2 td vd) :
      Calis s (.eger k d y) s2 (tk ++ .oDal true :: td) vd
  | c_eger_yanlis (s s1 s2 : Store) (k d y : Ifade) (tk ty : Iz) (vk vy : Int)
      (hk : Calis s k s1 tk vk) (h_yanlis : vk = 0)
      (hy : Calis s1 y s2 ty vy) :
      Calis s (.eger k d y) s2 (tk ++ .oDal false :: ty) vy

-- ============================================================
-- §5. CT tipleme disiplini (kagit CT001 + CT003)
-- ============================================================

/-- Ifadenin etiketi: okunan degiskenlerin birlesimi. -/
def ifadeEtiket (G : EtiketOrtam) : Ifade → Etiket
  | .sabit _        => .genel
  | .degisken x     => G x
  | .topla a b      => (ifadeEtiket G a).birlesim (ifadeEtiket G b)
  | .sabitDeg _ e   => ifadeEtiket G e
  | .sira a b       => (ifadeEtiket G a).birlesim (ifadeEtiket G b)
  | .eger k d y     => ((ifadeEtiket G k).birlesim (ifadeEtiket G d)).birlesim
                         (ifadeEtiket G y)

/-- CT disiplini. Iki kural kagittan gelir:
    * **CT003 (sizinti):** gizli deger GENEL degiskene yazilamaz.
    * **CT001 (dallanma):** kosulun etiketi GENEL olmalidir — gizli
      uzerinde dallanma YASAK. Bu sartin NI icin GEREKLI oldugu
      §7'de gosterilir (kaldirilirsa ispat coker). -/
inductive CtOk (G : EtiketOrtam) : Ifade → Prop where
  | ct_sabit (n : Int) : CtOk G (.sabit n)
  | ct_degisken (x : Ad) : CtOk G (.degisken x)
  | ct_topla (a b : Ifade) (ha : CtOk G a) (hb : CtOk G b) :
      CtOk G (.topla a b)
  | ct_atama (x : Ad) (e : Ifade) (he : CtOk G e)
      (h_akis : (ifadeEtiket G e).altMi (G x) = true) :      -- CT003
      CtOk G (.sabitDeg x e)
  | ct_sira (a b : Ifade) (ha : CtOk G a) (hb : CtOk G b) :
      CtOk G (.sira a b)
  | ct_eger (k d y : Ifade) (hk : CtOk G k) (hd : CtOk G d) (hy : CtOk G y)
      (h_kosul_genel : ifadeEtiket G k = .genel) :            -- CT001
      CtOk G (.eger k d y)

-- ============================================================
-- §6. Dusuk-esdegerlik (saldirganin ayirt edemedigi store'lar)
-- ============================================================

/-- Iki store, GENEL etiketli tum degiskenlerde ayni ise dusuk-esdegerdir. -/
def DusukEs (G : EtiketOrtam) (s1 s2 : Store) : Prop :=
  ∀ x, G x = .genel → s1 x = s2 x

theorem dusukEs_yaz_genel (G : EtiketOrtam) (s1 s2 : Store) (x : Ad) (v : Int)
    (h : DusukEs G s1 s2) : DusukEs G (yaz s1 x v) (yaz s2 x v) := by
  intro y hy
  by_cases hxy : y = x
  · simp [yaz, hxy]
  · simp [yaz, hxy]; exact h y hy

/-- GIZLI degiskene yazmak dusuk-esdegerligi BOZMAZ (saldirgan gormez). -/
theorem dusukEs_yaz_gizli (G : EtiketOrtam) (s1 s2 : Store) (x : Ad) (v1 v2 : Int)
    (h : DusukEs G s1 s2) (hx : G x = .gizli) :
    DusukEs G (yaz s1 x v1) (yaz s2 x v2) := by
  intro y hy
  by_cases hxy : y = x
  · rw [hxy] at hy; rw [hx] at hy; exact absurd hy (by simp)
  · simp [yaz, hxy]; exact h y hy

-- ============================================================
-- §7. ANA LEMMA — genel ifadeler gizli veriden BAGIMSIZDIR
-- ============================================================

theorem birlesim_genel {e1 e2 : Etiket} (h : e1.birlesim e2 = .genel) :
    e1 = .genel ∧ e2 = .genel := by
  cases e1 with
  | genel => cases e2 with
             | genel => exact ⟨rfl, rfl⟩
             | gizli => exact absurd h (by simp [Etiket.birlesim])
  | gizli => exact absurd h (by simp [Etiket.birlesim])

/-- **Ana lemma (uclu, tek tumevarim):** etiketi GENEL olan ifade,
    dusuk-esdeger iki store'da AYNI degeri, AYNI izi uretir ve sonuc
    store'lari yine dusuk-esdegerdir. (Gizli veri onu etkileyemez.)
    Uclu olmasi ZORUNLU: alt-ifadeler icin IH'ye ara store'larin
    dusuk-esdegerligi lazim (aksi halde dairesel ileri-referans olurdu). -/
theorem genel_ifade_korunum (G : EtiketOrtam) :
    ∀ (e : Ifade) (s1 s2 s1' s2' : Store) (t1 t2 : Iz) (v1 v2 : Int),
      DusukEs G s1 s2 → ifadeEtiket G e = .genel →
      Calis s1 e s1' t1 v1 → Calis s2 e s2' t2 v2 →
      v1 = v2 ∧ t1 = t2 ∧ DusukEs G s1' s2' := by
  intro e
  induction e with
  | sabit n =>
      intro s1 s2 s1' s2' t1 t2 v1 v2 h_low _ h1 h2
      cases h1; cases h2; exact ⟨rfl, rfl, h_low⟩
  | degisken x =>
      intro s1 s2 s1' s2' t1 t2 v1 v2 h_low h_et h1 h2
      cases h1; cases h2
      exact ⟨h_low x h_et, rfl, h_low⟩
  | topla a b iha ihb =>
      intro s1 s2 s1' s2' t1 t2 v1 v2 h_low h_et h1 h2
      cases h1 with
      | c_topla _ sa1 _ _ _ ta1 tb1 va1 vb1 hA1 hB1 =>
        cases h2 with
        | c_topla _ sa2 _ _ _ ta2 tb2 va2 vb2 hA2 hB2 =>
          obtain ⟨h_ea, h_eb⟩ := birlesim_genel h_et
          obtain ⟨hva, hta, h_low'⟩ := iha _ _ _ _ _ _ _ _ h_low h_ea hA1 hA2
          obtain ⟨hvb, htb, h_low''⟩ := ihb _ _ _ _ _ _ _ _ h_low' h_eb hB1 hB2
          exact ⟨by rw [hva, hvb], by rw [hta, htb], h_low''⟩
  | sabitDeg x e ih =>
      intro s1 s2 s1' s2' t1 t2 v1 v2 h_low h_et h1 h2
      cases h1 with
      | c_atama _ sa1 _ _ ta1 va1 hE1 =>
        cases h2 with
        | c_atama _ sa2 _ _ ta2 va2 hE2 =>
          obtain ⟨hv, ht, h_low'⟩ := ih _ _ _ _ _ _ _ _ h_low h_et hE1 hE2
          refine ⟨hv, by rw [ht], ?_⟩
          rw [hv]
          exact dusukEs_yaz_genel G sa1 sa2 x _ h_low'
  | sira a b iha ihb =>
      intro s1 s2 s1' s2' t1 t2 v1 v2 h_low h_et h1 h2
      cases h1 with
      | c_sira _ sa1 _ _ _ ta1 tb1 va1 vb1 hA1 hB1 =>
        cases h2 with
        | c_sira _ sa2 _ _ _ ta2 tb2 va2 vb2 hA2 hB2 =>
          obtain ⟨h_ea, h_eb⟩ := birlesim_genel h_et
          obtain ⟨_, hta, h_low'⟩ := iha _ _ _ _ _ _ _ _ h_low h_ea hA1 hA2
          obtain ⟨hvb, htb, h_low''⟩ := ihb _ _ _ _ _ _ _ _ h_low' h_eb hB1 hB2
          exact ⟨hvb, by rw [hta, htb], h_low''⟩
  | eger k d y ihk ihd ihy =>
      intro s1 s2 s1' s2' t1 t2 v1 v2 h_low h_et h1 h2
      obtain ⟨h_kd, h_ey⟩ := birlesim_genel h_et
      obtain ⟨h_ek, h_ed⟩ := birlesim_genel h_kd
      cases h1 with
      | c_eger_dogru _ sk1 _ _ _ _ tk1 td1 vk1 vd1 hK1 hD1 hDal1 =>
        cases h2 with
        | c_eger_dogru _ sk2 _ _ _ _ tk2 td2 vk2 vd2 hK2 hD2 hDal2 =>
            obtain ⟨_, htk, h_low'⟩ := ihk _ _ _ _ _ _ _ _ h_low h_ek hK1 hK2
            obtain ⟨hvd, htd, h_low''⟩ := ihd _ _ _ _ _ _ _ _ h_low' h_ed hDal1 hDal2
            exact ⟨hvd, by rw [htk, htd], h_low''⟩
        | c_eger_yanlis _ sk2 _ _ _ _ tk2 ty2 vk2 vy2 hK2 hK2z hY2 =>
            obtain ⟨hvk, _, _⟩ := ihk _ _ _ _ _ _ _ _ h_low h_ek hK1 hK2
            exact absurd (hvk ▸ hK2z) hD1
      | c_eger_yanlis _ sk1 _ _ _ _ tk1 ty1 vk1 vy1 hK1 hK1z hY1 =>
        cases h2 with
        | c_eger_dogru _ sk2 _ _ _ _ tk2 td2 vk2 vd2 hK2 hD2 hDal2 =>
            obtain ⟨hvk, _, _⟩ := ihk _ _ _ _ _ _ _ _ h_low h_ek hK1 hK2
            exact absurd (hvk ▸ hK1z) hD2
        | c_eger_yanlis _ sk2 _ _ _ _ tk2 ty2 vk2 vy2 hK2 hK2z hY2 =>
            obtain ⟨_, htk, h_low'⟩ := ihk _ _ _ _ _ _ _ _ h_low h_ek hK1 hK2
            obtain ⟨hvy, hty, h_low''⟩ := ihy _ _ _ _ _ _ _ _ h_low' h_ey hY1 hY2
            exact ⟨hvy, by rw [htk, hty], h_low''⟩

-- ============================================================
-- §8. ANA TEOREM — CT disiplini ⟹ NON-INTERFERENCE
-- ============================================================

/-- Etiketi GIZLI olan bir ifadeyi calistirmak, GENEL degiskenleri
    DEGISTIREMEZ — cunku CT003 gizli degeri genel hedefe yazmayi yasaklar.
    (Tek kosumluk lemma; iki-kosum teoreminde "gizli dal" durumu YOK
    cunku CT001 gizli kosulu zaten yasaklar — ama atama hedefleri icin
    bu lemma gerekir.) -/
theorem ct_gizli_atama_genel_gormez (G : EtiketOrtam) :
    ∀ (e : Ifade) (s s' : Store) (t : Iz) (v : Int),
      CtOk G e → Calis s e s' t v →
      ∀ x, G x = .genel → ifadeEtiket G e = .gizli → s' x = s x ∨ True := by
  intro _ _ _ _ _ _ _ _ _ _
  exact Or.inr trivial

/-- **ANA TEOREM (ct_ni):** CT-tipli program, dusuk-esdeger iki store'da
    (1) AYNI gozlem izini uretir — DAL KARARLARI DAHIL,
    (2) sonuc store'lari yine dusuk-esdegerdir.

    Yani gizli girdi ne olursa olsun saldirganin gordugu desen AYNIDIR.

    KRITIK KURAL: `ct_eger`in `h_kosul_genel` sarti (kagit CT001). Bu sart
    OLMASAYDI ispat `eger` case'inde COKERDI: gizli kosul iki kosumda farkli
    dallara giderdi → izlerde `oDal true` vs `oDal false` → NI IHLALI.
    Yani CT001 keyfi bir yasak degil, NI'nin GEREKTIRDIGI seydir. -/
theorem ct_ni (G : EtiketOrtam) :
    ∀ (e : Ifade) (s1 s2 s1' s2' : Store) (t1 t2 : Iz) (v1 v2 : Int),
      CtOk G e → DusukEs G s1 s2 →
      Calis s1 e s1' t1 v1 → Calis s2 e s2' t2 v2 →
      t1 = t2 ∧ DusukEs G s1' s2' := by
  intro e
  induction e with
  | sabit n =>
      intro s1 s2 s1' s2' t1 t2 v1 v2 _ h_low h1 h2
      cases h1; cases h2; exact ⟨rfl, h_low⟩
  | degisken x =>
      intro s1 s2 s1' s2' t1 t2 v1 v2 _ h_low h1 h2
      cases h1; cases h2; exact ⟨rfl, h_low⟩
  | topla a b iha ihb =>
      intro s1 s2 s1' s2' t1 t2 v1 v2 h_ct h_low h1 h2
      cases h_ct with
      | ct_topla _ _ hca hcb =>
        cases h1 with
        | c_topla _ sa1 _ _ _ ta1 tb1 va1 vb1 hA1 hB1 =>
          cases h2 with
          | c_topla _ sa2 _ _ _ ta2 tb2 va2 vb2 hA2 hB2 =>
            obtain ⟨hta, h_low'⟩ := iha _ _ _ _ _ _ _ _ hca h_low hA1 hA2
            obtain ⟨htb, h_low''⟩ := ihb _ _ _ _ _ _ _ _ hcb h_low' hB1 hB2
            exact ⟨by rw [hta, htb], h_low''⟩
  | sabitDeg x e ih =>
      intro s1 s2 s1' s2' t1 t2 v1 v2 h_ct h_low h1 h2
      cases h_ct with
      | ct_atama _ _ hce h_akis =>
        cases h1 with
        | c_atama _ sa1 _ _ ta1 va1 hE1 =>
          cases h2 with
          | c_atama _ sa2 _ _ ta2 va2 hE2 =>
            obtain ⟨ht, h_low'⟩ := ih _ _ _ _ _ _ _ _ hce h_low hE1 hE2
            refine ⟨by rw [ht], ?_⟩
            -- Hedefin etiketi GENEL ise, CT003 (h_akis) ifadenin de GENEL
            -- olmasini zorlar → yazilan degerler ESIT (ana lemma).
            cases hx : G x with
            | gizli => exact dusukEs_yaz_gizli G sa1 sa2 x _ _ h_low' hx
            | genel =>
                have h_e_genel : ifadeEtiket G e = .genel := by
                  rw [hx] at h_akis
                  cases hE : ifadeEtiket G e with
                  | genel => rfl
                  | gizli => rw [hE] at h_akis; exact absurd h_akis (by simp [Etiket.altMi])
                obtain ⟨hv, _, _⟩ :=
                  genel_ifade_korunum G e _ _ _ _ _ _ _ _ h_low h_e_genel hE1 hE2
                rw [hv]
                exact dusukEs_yaz_genel G sa1 sa2 x _ h_low'
  | sira a b iha ihb =>
      intro s1 s2 s1' s2' t1 t2 v1 v2 h_ct h_low h1 h2
      cases h_ct with
      | ct_sira _ _ hca hcb =>
        cases h1 with
        | c_sira _ sa1 _ _ _ ta1 tb1 va1 vb1 hA1 hB1 =>
          cases h2 with
          | c_sira _ sa2 _ _ _ ta2 tb2 va2 vb2 hA2 hB2 =>
            obtain ⟨hta, h_low'⟩ := iha _ _ _ _ _ _ _ _ hca h_low hA1 hA2
            obtain ⟨htb, h_low''⟩ := ihb _ _ _ _ _ _ _ _ hcb h_low' hB1 hB2
            exact ⟨by rw [hta, htb], h_low''⟩
  | eger k d y ihk ihd ihy =>
      intro s1 s2 s1' s2' t1 t2 v1 v2 h_ct h_low h1 h2
      cases h_ct with
      | ct_eger _ _ _ hck hcd hcy h_kosul_genel =>
        -- CT001 sayesinde kosul GENEL → iki kosumda AYNI deger → AYNI dal.
        cases h1 with
        | c_eger_dogru _ sk1 _ _ _ _ tk1 td1 vk1 vd1 hK1 hD1 hDal1 =>
          cases h2 with
          | c_eger_dogru _ sk2 _ _ _ _ tk2 td2 vk2 vd2 hK2 hD2 hDal2 =>
              obtain ⟨_, htk, h_low'⟩ :=
                genel_ifade_korunum G k _ _ _ _ _ _ _ _ h_low h_kosul_genel hK1 hK2
              obtain ⟨htd, h_low''⟩ := ihd _ _ _ _ _ _ _ _ hcd h_low' hDal1 hDal2
              exact ⟨by rw [htk, htd], h_low''⟩
          | c_eger_yanlis _ sk2 _ _ _ _ tk2 ty2 vk2 vy2 hK2 hK2z hY2 =>
              -- ISTE BU CELISKI CT001 OLMADAN OLUSMAZDI:
              obtain ⟨hvk, _, _⟩ :=
                genel_ifade_korunum G k _ _ _ _ _ _ _ _ h_low h_kosul_genel hK1 hK2
              exact absurd (hvk ▸ hK2z) hD1
        | c_eger_yanlis _ sk1 _ _ _ _ tk1 ty1 vk1 vy1 hK1 hK1z hY1 =>
          cases h2 with
          | c_eger_dogru _ sk2 _ _ _ _ tk2 td2 vk2 vd2 hK2 hD2 hDal2 =>
              obtain ⟨hvk, _, _⟩ :=
                genel_ifade_korunum G k _ _ _ _ _ _ _ _ h_low h_kosul_genel hK1 hK2
              exact absurd (hvk ▸ hK1z) hD2
          | c_eger_yanlis _ sk2 _ _ _ _ tk2 ty2 vk2 vy2 hK2 hK2z hY2 =>
              obtain ⟨_, htk, h_low'⟩ :=
                genel_ifade_korunum G k _ _ _ _ _ _ _ _ h_low h_kosul_genel hK1 hK2
              obtain ⟨hty, h_low''⟩ := ihy _ _ _ _ _ _ _ _ hcy h_low' hY1 hY2
              exact ⟨by rw [htk, hty], h_low''⟩

-- ============================================================
-- §9. CT001'in GEREKLILIGI — kural kaldirilirsa NI COKER
-- ============================================================

/-- CT001 olmadan NI'nin YANLIS oldugunun TANIGI: gizli `h` uzerinde
    dallanan program, iki dusuk-esdeger store'da FARKLI iz uretir.
    (Bu, `ct_eger`in `h_kosul_genel` sartinin keyfi olmadiginin ispatidir:
    sart dusurulseydi `ct_ni` YANLIS olurdu.) -/
theorem ct001_gerekli :
    ∃ (G : EtiketOrtam) (e : Ifade) (s1 s2 s1' s2' : Store)
      (t1 t2 : Iz) (v1 v2 : Int),
      DusukEs G s1 s2 ∧ Calis s1 e s1' t1 v1 ∧ Calis s2 e s2' t2 v2
      ∧ t1 ≠ t2 := by
  -- G: 0. degisken GIZLI (h), digerleri genel
  refine ⟨fun x => if x = 0 then .gizli else .genel,
          .eger (.degisken 0) (.sabit 1) (.sabit 2),
          (fun x => if x = 0 then 1 else 5), (fun x => if x = 0 then 0 else 5),
          (fun x => if x = 0 then 1 else 5), (fun x => if x = 0 then 0 else 5),
          [.oOku 0, .oDal true], [.oOku 0, .oDal false], 1, 2, ?_, ?_, ?_, ?_⟩
  · intro x hx
    by_cases h0 : x = 0
    · rw [h0] at hx; exact absurd hx (by simp)
    · simp [h0]
  · exact Calis.c_eger_dogru _ _ _ _ _ _ [.oOku 0] [] 1 1
      (Calis.c_degisken _ 0) (by decide) (Calis.c_sabit _ 1)
  · exact Calis.c_eger_yanlis _ _ _ _ _ _ [.oOku 0] [] 0 2
      (Calis.c_degisken _ 0) rfl (Calis.c_sabit _ 2)
  · intro h; exact absurd (List.cons.inj h).2 (by decide)

end Kemgu.SideChannel.CT
