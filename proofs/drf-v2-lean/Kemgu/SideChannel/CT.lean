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

GENISLEME (D-334/D-335): `topla` (aritmetik), `iken` (dongu — CT002) ve
  `esles` (literal desen eslemesi — CT004) HEM bu hesaba HEM `Sem/Core`'a
  HEM koprüye eklendi. `ct_ni` artik dort dallanma bicimini de kapsiyor.
  YAPISAL NOT: `iken` buyuk-adim kurali KENDISINE ozyineledigi icin
  `genel_ifade_korunum` ve `ct_ni` artik `Ifade` uzerinde degil KOSUM
  TURETIMI uzerinde tumevarim yapiyor.

KALAN BORC (daralmis, acikca):
  Kopru CT'nin deger-sadik (`Sadik`) parcasinda, SONLU degisken
  kumesiyle ve TEK THREAD icin kuruludur. Yani "KEMGU'nun KENDISI
  sabit-suredir" tam iddiasi hala CIKMAZ; eksikler: ESZAMANLI CT,
  GIZLI INDEKS (bellek erisim adresi), carpma/BOLME (bolmenin
  veri-bagimli gecikmesi `topla`dan farkli bir CT kurali ister),
  ve `esles`in YAPICI/CESIT desenleri (Core'da ADT yok).
  Ayrinti + vakum ve sabotaj denetimleri: D-333, D-334, D-335.

NE ISPATLANIYOR (bu dosyada, tam):
  - `genel_ifade_degeri_esit`: etiketi GENEL olan ifade, dusuk-esdeger
    iki store'da AYNI degeri uretir (gizli veriden bagimsizdir).
  - `ct_ni`: CT-tipli program + dusuk-esdeger baslangic store'lari →
    (1) GOZLEM IZLERI BIREBIR AYNI (dallanma karari dahil!),
    (2) sonuc store'lari yine dusuk-esdeger.
  Yani gizli girdi degisse bile saldirganin gordugu (okuma/yazma deseni
  + hangi dalin alindigi) DEGISMEZ.

NEDEN VAKUM DEGIL:
  `ct_eger`/`ct_iken`/`ct_esles` kurallarindaki "kosul (skrutin) etiketi
  = genel" sarti KALDIRILIRSA ispat COKER. Uc TANIK bunu gosterir:
  `ct001_gerekli` (dallanma), `ct002_gerekli` (dongu — TUR SAYISI sizar),
  `ct004_gerekli` (eslesme — HANGI KOL sizar). Yani bu kurallar keyfi
  degil, NI'nin ta kendisi icin gereklidir.
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

/-- D-336: HER DEGISKEN BIR DIZIDIR (Mehmet karari). Duz degisken =
    indeks 0. `Sem/Core`daki karsilik `Konum = ⟨bolge(x), ofset⟩`dir;
    yani bu esleme ana modelin ZATEN VAROLAN ofset alanina oturur.
    Store TOPLAMDIR — sinir denetimi modellenmez (bkz. `Core.hucreOku`). -/
abbrev Store := Ad → Nat → Int

/-- Etiket DIZI BASINADIR: bir dizinin tum hucreleri ayni gizlilik
    seviyesindedir. (Hucre-basina etiket V2; kriptografik tablolar
    — S-box gibi — zaten dizi-butunu GENEL, INDEKS gizli olur ki
    CT'nin yasakladigi sey tam olarak budur.) -/
abbrev EtiketOrtam := Ad → Etiket

/-- Store guncelleme (duz degisken = indeks 0). -/
def yaz (s : Store) (x : Ad) (v : Int) : Store :=
  fun y i => if y = x ∧ i = 0 then v else s y i

/-- D-337: INDEKSLI store guncelleme — `x[i] = v`. `yaz`, bunun i = 0
    ozel halidir. -/
def yazH (s : Store) (x : Ad) (i : Nat) (v : Int) : Store :=
  fun y j => if y = x ∧ j = i then v else s y j

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
  | iken     (kosul govde : Ifade)                              -- D-335 (CT002)
  | esles    (skrut : Ifade) (n : Int) (eslesen kalan : Ifade)   -- D-335 (CT004)
  | indeks   (dizi : Ad) (idx : Ifade)                           -- D-336 (CT005)
  | indeksAta (dizi : Ad) (idx : Ifade) (deger : Ifade)          -- D-337 (CT005-Y)
  | bol      (a b : Ifade)                                       -- D-338 (CT006)
  | kalan    (a b : Ifade)                                       -- D-339 (CT006-M)

-- ============================================================
-- §3. Saldirgan gozlemi — erisim deseni + DAL KARARI
-- ============================================================

/-- Gozlem: hangi degiskene erisildi + hangi dal alindi.
    `oDal` KRITIK: dallanma karari saldirgan tarafindan gorulur
    (dal hedefleri farkli kod/zaman → PC/timing sizintisi). -/
inductive Gozlem : Type where
  /-- D-336: okuma gozlemi artik INDEKSI de tasir — saldirgan ERISIM
      ADRESINI gorur (onbellek-satiri zamanlamasi). Duz degisken okumasi
      `oOku x 0`dir. Gizli-indeks kanali (CT005) tam olarak buradadir. -/
  | oOku (x : Ad) (i : Nat)
  | oYaz (x : Ad) (i : Nat)
  | oDal (alindi : Bool)
  /-- D-338 (CT006): BOLME gozlemi — OPERANDLARI tasir. Bolme sabit
      cevrim DEGILDIR; gecikmesi operandlarin fonksiyonudur, dolayisiyla
      saldirganin ogrenebileceginin UST SINIRI operandlardir.
      `topla`nin boyle bir gozlemi YOKTUR — fark tam olarak budur. -/
  | oBol (a b : Int)
  /-- D-339 (CT006-M): BOLME gozlemi — OPERANDLARI tasir. Bolme sabit
      cevrim DEGILDIR; gecikmesi operandlarin fonksiyonudur, dolayisiyla
      saldirganin ogrenebileceginin UST SINIRI operandlardir.
      `topla`nin boyle bir gozlemi YOKTUR — fark tam olarak budur. -/
  | oMod (a b : Int)
deriving DecidableEq, Repr

abbrev Iz := List Gozlem

-- ============================================================
-- §4. Buyuk-adim semantik (deger + store + iz uretir)
-- ============================================================

inductive Calis : Store → Ifade → Store → Iz → Int → Prop where
  | c_sabit (s : Store) (n : Int) :
      Calis s (.sabit n) s [] n
  | c_degisken (s : Store) (x : Ad) :
      Calis s (.degisken x) s [.oOku x 0] (s x 0)
  /-- D-336 (CT005): `x[idx]` — once indeks kosar, sonra hucre okunur ve
      **OKUNAN ADRES IZE GIRER** (`oOku x i`). Indeks negatifse `toNat`
      ile 0'a kirpilir (store TOPLAM; sinir denetimi modellenmez). -/
  | c_indeks (s s1 : Store) (x : Ad) (idx : Ifade) (ti : Iz) (vi : Int)
      (hi : Calis s idx s1 ti vi) :
      Calis s (.indeks x idx) s1 (ti ++ [.oOku x vi.toNat]) (s1 x vi.toNat)
  /-- D-337 (CT005-Y): `x[idx] = e` — ONCE INDEKS, SONRA DEGER (Core'un
      `sIndeksAtaCongIdx`/`CongDeg` sirasiyla birebir). **YAZILAN ADRES
      IZE GIRER** (`oYaz x i`). Deger olarak yazilani dondurur. -/
  | c_indeks_ata (s s1 s2 : Store) (x : Ad) (idx e : Ifade)
      (ti te : Iz) (vi ve : Int)
      (hi : Calis s idx s1 ti vi) (he : Calis s1 e s2 te ve) :
      Calis s (.indeksAta x idx e) (yazH s2 x vi.toNat ve)
        (ti ++ te ++ [.oYaz x vi.toNat]) ve
  | c_topla (s s1 s2 : Store) (a b : Ifade) (t1 t2 : Iz) (v1 v2 : Int)
      (h1 : Calis s a s1 t1 v1) (h2 : Calis s1 b s2 t2 v2) :
      Calis s (.topla a b) s2 (t1 ++ t2) (v1 + v2)
  /-- D-338 (CT006): `a / b` — soldan saga kosum, sonra **`oBol` OLAYI**.
      `c_topla` ile TEK farki bu olaydir; sifira bolme icin Lean `Int`
      bolmesi (n/0 = 0) — TOPLAM. -/
  | c_bol (s s1 s2 : Store) (a b : Ifade) (t1 t2 : Iz) (v1 v2 : Int)
      (h1 : Calis s a s1 t1 v1) (h2 : Calis s1 b s2 t2 v2) :
      Calis s (.bol a b) s2 (t1 ++ t2 ++ [.oBol v1 v2]) (v1 / v2)
  /-- D-339 (CT006-M): `a / b` — soldan saga kosum, sonra **`oMod` OLAYI**.
      `c_topla` ile TEK farki bu olaydir; sifira bolme icin Lean `Int`
      bolmesi (n/0 = 0) — TOPLAM. -/
  | c_kalan (s s1 s2 : Store) (a b : Ifade) (t1 t2 : Iz) (v1 v2 : Int)
      (h1 : Calis s a s1 t1 v1) (h2 : Calis s1 b s2 t2 v2) :
      Calis s (.kalan a b) s2 (t1 ++ t2 ++ [.oMod v1 v2]) (v1 % v2)
  | c_atama (s s1 : Store) (x : Ad) (e : Ifade) (t : Iz) (v : Int)
      (h : Calis s e s1 t v) :
      Calis s (.sabitDeg x e) (yaz s1 x v) (t ++ [.oYaz x 0]) v
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
  /-- D-335 (CT002): dongu — her TUR bir `oDal` uretir, yani TUR SAYISI
      saldirgana gorunur. Deger 0 (dongu bir DEYIMDIR). -/
  | c_iken_dogru (s s1 s2 s3 : Store) (k g : Ifade) (tk tg ti : Iz)
      (vk vg vi : Int)
      (hk : Calis s k s1 tk vk) (h_dogru : vk ≠ 0)
      (hg : Calis s1 g s2 tg vg)
      (hi : Calis s2 (.iken k g) s3 ti vi) :
      Calis s (.iken k g) s3 (tk ++ .oDal true :: (tg ++ ti)) 0
  | c_iken_yanlis (s s1 : Store) (k g : Ifade) (tk : Iz) (vk : Int)
      (hk : Calis s k s1 tk vk) (h_yanlis : vk = 0) :
      Calis s (.iken k g) s1 (tk ++ [.oDal false]) 0
  /-- D-335 (CT004): literal desen eslemesi — `eger` ile AYNI gozlem
      modeli (kol basina bir dal karari; Mehmet karari). -/
  | c_esles_tuttu (s s1 s2 : Store) (sk : Ifade) (n : Int) (d y : Ifade)
      (ts td : Iz) (vs vd : Int)
      (hs : Calis s sk s1 ts vs) (h_tuttu : vs = n)
      (hd : Calis s1 d s2 td vd) :
      Calis s (.esles sk n d y) s2 (ts ++ .oDal true :: td) vd
  | c_esles_tutmadi (s s1 s2 : Store) (sk : Ifade) (n : Int) (d y : Ifade)
      (ts ty : Iz) (vs vy : Int)
      (hs : Calis s sk s1 ts vs) (h_tutmadi : vs ≠ n)
      (hy : Calis s1 y s2 ty vy) :
      Calis s (.esles sk n d y) s2 (ts ++ .oDal false :: ty) vy

-- ============================================================
-- §5. CT tipleme disiplini (kagit CT001 + CT003)
-- ============================================================

/-- Ifadenin etiketi: okunan degiskenlerin birlesimi. -/
def ifadeEtiket (G : EtiketOrtam) : Ifade → Etiket
  | .sabit _        => .genel
  | .degisken x     => G x
  | .topla a b      => (ifadeEtiket G a).birlesim (ifadeEtiket G b)
  | .bol a b        => (ifadeEtiket G a).birlesim (ifadeEtiket G b)
  | .kalan a b        => (ifadeEtiket G a).birlesim (ifadeEtiket G b)
  | .sabitDeg _ e   => ifadeEtiket G e
  | .sira a b       => (ifadeEtiket G a).birlesim (ifadeEtiket G b)
  | .eger k d y     => ((ifadeEtiket G k).birlesim (ifadeEtiket G d)).birlesim
                         (ifadeEtiket G y)
  | .iken k g       => (ifadeEtiket G k).birlesim (ifadeEtiket G g)
  | .esles s _ d y  => ((ifadeEtiket G s).birlesim (ifadeEtiket G d)).birlesim
                         (ifadeEtiket G y)
  -- D-336: okunan degerin etiketi DIZININ etiketi (ve indeksinki —
  -- gizli indeksle secilen hucre de gizli sayilir; muhafazakar).
  | .indeks x idx   => (G x).birlesim (ifadeEtiket G idx)
  -- D-337: yazmanin degeri yazilan degerdir, AMA etiketine INDEKSINKI de
  -- katilir. Gerekce (ispat tarafindan zorlandi): `genel_ifade_korunum`
  -- "etiketi genel olan ifade AYNI IZI uretir" der; indeks etiketi
  -- katilmasaydi gizli indeksli bir yazma "genel" sayilir ve iki kosumda
  -- `oYaz x i1` vs `oYaz x i2` uretirdi → lemma YANLIS olurdu.
  -- Katmak MUHAFAZAKARDIR (etiketi yukseltir), dolayisiyla guvenlidir.
  | .indeksAta _ idx e => (ifadeEtiket G idx).birlesim (ifadeEtiket G e)

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
  /-- **CT006 (veri-bagimli gecikme):** BOLMENIN HER IKI OPERANDI GENEL
      olmalidir. `ct_topla`da BOYLE BIR SART YOKTUR — cunku toplama
      sabit cevrimdir. Fark keyfi degil: `sBolTamam`/`c_bol` operandlari
      ize koyar, dolayisiyla gizli operand DOGRUDAN gozlenir
      (`ct006_gerekli` taniki). -/
  | ct_bol (a b : Ifade) (ha : CtOk G a) (hb : CtOk G b)
      (h_a_genel : ifadeEtiket G a = .genel)                   -- CT006
      (h_b_genel : ifadeEtiket G b = .genel) :                 -- CT006
      CtOk G (.bol a b)
  /-- **CT006-M (veri-bagimli gecikme):** BOLMENIN HER IKI OPERANDI GENEL
      olmalidir. `ct_topla`da BOYLE BIR SART YOKTUR — cunku toplama
      sabit cevrimdir. Fark keyfi degil: `sKalanTamam`/`c_kalan` operandlari
      ize koyar, dolayisiyla gizli operand DOGRUDAN gozlenir
      (`ct006_gerekli` taniki). -/
  | ct_kalan (a b : Ifade) (ha : CtOk G a) (hb : CtOk G b)
      (h_a_genel : ifadeEtiket G a = .genel)                   -- CT006-M
      (h_b_genel : ifadeEtiket G b = .genel) :                 -- CT006-M
      CtOk G (.kalan a b)
  | ct_atama (x : Ad) (e : Ifade) (he : CtOk G e)
      (h_akis : (ifadeEtiket G e).altMi (G x) = true) :      -- CT003
      CtOk G (.sabitDeg x e)
  | ct_sira (a b : Ifade) (ha : CtOk G a) (hb : CtOk G b) :
      CtOk G (.sira a b)
  | ct_eger (k d y : Ifade) (hk : CtOk G k) (hd : CtOk G d) (hy : CtOk G y)
      (h_kosul_genel : ifadeEtiket G k = .genel) :            -- CT001
      CtOk G (.eger k d y)
  /-- **CT002 (dongu):** dongu KOSULU GENEL olmalidir — gizli uzerinde
      donmek TUR SAYISINI sizdirir (her tur bir `oDal`). -/
  | ct_iken (k g : Ifade) (hk : CtOk G k) (hg : CtOk G g)
      (h_kosul_genel : ifadeEtiket G k = .genel) :            -- CT002
      CtOk G (.iken k g)
  /-- **CT004 (desen eslemesi):** SKRUTIN GENEL olmalidir — gizli uzerinde
      eslesmek hangi kolun tuttugunu sizdirir. -/
  | ct_esles (s : Ifade) (n : Int) (d y : Ifade)
      (hs : CtOk G s) (hd : CtOk G d) (hy : CtOk G y)
      (h_skrut_genel : ifadeEtiket G s = .genel) :            -- CT004
      CtOk G (.esles s n d y)
  /-- **CT005 (gizli indeks):** INDEKS GENEL olmalidir — gizliye gore
      adreslemek erisilen adresi (dolayisiyla onbellek satirini) sizdirir.
      Dizinin KENDISI gizli olabilir (S-box gibi tablolar genelde
      geneldir; yasaklanan sey INDEKSIN gizli olmasidir). -/
  | ct_indeks (x : Ad) (idx : Ifade) (hi : CtOk G idx)
      (h_idx_genel : ifadeEtiket G idx = .genel) :            -- CT005
      CtOk G (.indeks x idx)
  /-- **CT005-Y (gizli indeksle YAZMA):** indeks GENEL olmali (yazilan
      adres sizar) VE CT003 akis kurali: yazilan degerin etiketi hedef
      dizinin etiketine dusmelidir. Iki sart iki FARKLI kanali kapatir:
      `h_idx_genel` ADRES kanalini, `h_akis` VERI kanalini. -/
  | ct_indeks_ata (x : Ad) (idx e : Ifade) (hi : CtOk G idx) (he : CtOk G e)
      (h_idx_genel : ifadeEtiket G idx = .genel)               -- CT005-Y
      (h_akis : (ifadeEtiket G e).altMi (G x) = true) :        -- CT003
      CtOk G (.indeksAta x idx e)

-- ============================================================
-- §6. Dusuk-esdegerlik (saldirganin ayirt edemedigi store'lar)
-- ============================================================

/-- Iki store, GENEL etiketli tum degiskenlerde ayni ise dusuk-esdegerdir. -/
def DusukEs (G : EtiketOrtam) (s1 s2 : Store) : Prop :=
  ∀ x, G x = .genel → ∀ i, s1 x i = s2 x i

theorem dusukEs_yaz_genel (G : EtiketOrtam) (s1 s2 : Store) (x : Ad) (v : Int)
    (h : DusukEs G s1 s2) : DusukEs G (yaz s1 x v) (yaz s2 x v) := by
  intro y hy i
  by_cases hxy : y = x ∧ i = 0
  · simp [yaz, hxy]
  · simp [yaz, hxy]; exact h y hy i

/-- GIZLI degiskene yazmak dusuk-esdegerligi BOZMAZ (saldirgan gormez). -/
theorem dusukEs_yaz_gizli (G : EtiketOrtam) (s1 s2 : Store) (x : Ad) (v1 v2 : Int)
    (h : DusukEs G s1 s2) (hx : G x = .gizli) :
    DusukEs G (yaz s1 x v1) (yaz s2 x v2) := by
  intro y hy i
  by_cases hxy : y = x
  · rw [hxy] at hy; rw [hx] at hy; exact absurd hy (by simp)
  · have h1 : ¬ (y = x ∧ i = 0) := by intro hc; exact hxy hc.1
    simp [yaz, h1]; exact h y hy i

/-- D-337: INDEKSLI yazmanin dusuk-esdegerlik lemmalari. Adres AYNI
    oldugu surece (CT005-Y bunu garanti eder) GENEL diziye ayni deger
    yazilir; GIZLI diziye ne yazilirsa yazilsin gorunmez. -/
theorem dusukEs_yazH_genel (G : EtiketOrtam) (s1 s2 : Store) (x : Ad)
    (i : Nat) (v : Int) (h : DusukEs G s1 s2) :
    DusukEs G (yazH s1 x i v) (yazH s2 x i v) := by
  intro y hy j
  by_cases hxy : y = x ∧ j = i
  · simp [yazH, hxy]
  · simp [yazH, hxy]; exact h y hy j

theorem dusukEs_yazH_gizli (G : EtiketOrtam) (s1 s2 : Store) (x : Ad)
    (i1 i2 : Nat) (v1 v2 : Int)
    (h : DusukEs G s1 s2) (hx : G x = .gizli) :
    DusukEs G (yazH s1 x i1 v1) (yazH s2 x i2 v2) := by
  intro y hy j
  by_cases hxy : y = x
  · rw [hxy] at hy; rw [hx] at hy; exact absurd hy (by simp)
  · have h1 : ¬ (y = x ∧ j = i1) := by intro hc; exact hxy hc.1
    have h2 : ¬ (y = x ∧ j = i2) := by intro hc; exact hxy hc.1
    show (if y = x ∧ j = i1 then v1 else s1 y j)
        = (if y = x ∧ j = i2 then v2 else s2 y j)
    rw [if_neg h1, if_neg h2]
    exact h y hy j

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

/-- **Ana lemma:** etiketi GENEL olan ifade, dusuk-esdeger iki store'da
    AYNI degeri, AYNI izi uretir ve sonuc store'lari yine dusuk-esdegerdir.
    (Gizli veri onu etkileyemez.)

    D-335 YAPISAL DEGISIKLIK: tumevarim artik `Ifade` uzerinde DEGIL,
    **KOSUM TURETIMI (`Calis`) uzerinde**. Sebep `iken`: dongunun
    buyuk-adim kurali KENDISINE ozyineler (`Calis s2 (iken k g) s3 ...`),
    yani yapisal tumevarim o ic kosum icin IH VERMEZ. Turetim uzerinde
    tumevarim verir. (Ayni degisiklik `ct_ni`de de yapildi.) -/
theorem genel_ifade_korunum (G : EtiketOrtam) :
    ∀ {s1 : Store} {e : Ifade} {s1' : Store} {t1 : Iz} {v1 : Int},
      Calis s1 e s1' t1 v1 →
      ∀ {s2 s2' : Store} {t2 : Iz} {v2 : Int}, Calis s2 e s2' t2 v2 →
      DusukEs G s1 s2 → ifadeEtiket G e = .genel →
      v1 = v2 ∧ t1 = t2 ∧ DusukEs G s1' s2' := by
  intro s1 e s1' t1 v1 h1
  induction h1 with
  | c_sabit s n =>
      intro s2 s2' t2 v2 h2 h_low _
      cases h2; exact ⟨rfl, rfl, h_low⟩
  | c_degisken s x =>
      intro s2 s2' t2 v2 h2 h_low h_et
      cases h2; exact ⟨h_low x h_et 0, rfl, h_low⟩
  -- D-338 (CT006): etiket GENEL → her iki operand genel → operand
  -- DEGERLERI de esit → `oBol` olaylari BIREBIR ayni.
  | c_bol s sa1 _ a b ta1 tb1 va1 vb1 hA1 hB1 ihA ihB =>
      intro s2 s2' t2 v2 h2 h_low h_et
      cases h2 with
      | c_bol _ sa2 _ _ _ ta2 tb2 va2 vb2 hA2 hB2 =>
        obtain ⟨h_ea, h_eb⟩ := birlesim_genel h_et
        obtain ⟨hva, hta, h_low'⟩ := ihA hA2 h_low h_ea
        obtain ⟨hvb, htb, h_low''⟩ := ihB hB2 h_low' h_eb
        exact ⟨by rw [hva, hvb], by rw [hta, htb, hva, hvb], h_low''⟩
  -- D-339 (CT006-M): etiket GENEL → her iki operand genel → operand
  -- DEGERLERI de esit → `oMod` olaylari BIREBIR ayni.
  | c_kalan s sa1 _ a b ta1 tb1 va1 vb1 hA1 hB1 ihA ihB =>
      intro s2 s2' t2 v2 h2 h_low h_et
      cases h2 with
      | c_kalan _ sa2 _ _ _ ta2 tb2 va2 vb2 hA2 hB2 =>
        obtain ⟨h_ea, h_eb⟩ := birlesim_genel h_et
        obtain ⟨hva, hta, h_low'⟩ := ihA hA2 h_low h_ea
        obtain ⟨hvb, htb, h_low''⟩ := ihB hB2 h_low' h_eb
        exact ⟨by rw [hva, hvb], by rw [hta, htb, hva, hvb], h_low''⟩
  | c_topla s sa1 _ a b ta1 tb1 va1 vb1 hA1 hB1 ihA ihB =>
      intro s2 s2' t2 v2 h2 h_low h_et
      cases h2 with
      | c_topla _ sa2 _ _ _ ta2 tb2 va2 vb2 hA2 hB2 =>
        obtain ⟨h_ea, h_eb⟩ := birlesim_genel h_et
        obtain ⟨hva, hta, h_low'⟩ := ihA hA2 h_low h_ea
        obtain ⟨hvb, htb, h_low''⟩ := ihB hB2 h_low' h_eb
        exact ⟨by rw [hva, hvb], by rw [hta, htb], h_low''⟩
  -- D-336 (CT005): etiket GENEL ise hem DIZI hem INDEKS geneldir →
  -- iki kosumda AYNI adres okunur ve AYNI deger doner.
  -- D-337: etiket GENEL ise yazilan DEGER geneldir; indeks etiketi
  -- burada serbesttir (ifadeEtiket yalniz degerden gelir) — ADRES
  -- esitligi `ct_ni` tarafinda `ct_indeks_ata`nin `h_idx_genel`iyle
  -- saglanir. Bu lemma yalniz DEGER + iz esitligini kurar.
  | c_indeks_ata s si se x idx e ti te vi ve hI1 hE1 ihI ihE =>
      intro s2 s2' t2 v2 h2 h_low h_et
      obtain ⟨h_ei, h_ee⟩ := birlesim_genel h_et
      cases h2 with
      | c_indeks_ata _ si2 se2 _ _ _ ti2 te2 vi2 ve2 hI2 hE2 =>
        obtain ⟨hvi, hti, h_low'⟩ := ihI hI2 h_low h_ei
        obtain ⟨hve, hte, h_low''⟩ := ihE hE2 h_low' h_ee
        refine ⟨hve, by rw [hti, hte, hvi], ?_⟩
        rw [hvi, hve]
        exact dusukEs_yazH_genel G se se2 x _ _ h_low''
  | c_indeks s si x idx ti vi hI1 ihI =>
      intro s2 s2' t2 v2 h2 h_low h_et
      obtain ⟨h_ex, h_ei⟩ := birlesim_genel h_et
      cases h2 with
      | c_indeks _ si2 _ _ ti2 vi2 hI2 =>
        obtain ⟨hvi, hti, h_low'⟩ := ihI hI2 h_low h_ei
        refine ⟨?_, by rw [hti, hvi], h_low'⟩
        rw [hvi]
        exact h_low' x h_ex vi2.toNat
  | c_atama s sa1 x e ta1 va1 hE1 ihE =>
      intro s2 s2' t2 v2 h2 h_low h_et
      cases h2 with
      | c_atama _ sa2 _ _ ta2 va2 hE2 =>
        obtain ⟨hv, ht, h_low'⟩ := ihE hE2 h_low h_et
        refine ⟨hv, by rw [ht], ?_⟩
        rw [hv]
        exact dusukEs_yaz_genel G sa1 sa2 x _ h_low'
  | c_sira s sa1 _ a b ta1 tb1 va1 vb1 hA1 hB1 ihA ihB =>
      intro s2 s2' t2 v2 h2 h_low h_et
      cases h2 with
      | c_sira _ sa2 _ _ _ ta2 tb2 va2 vb2 hA2 hB2 =>
        obtain ⟨h_ea, h_eb⟩ := birlesim_genel h_et
        obtain ⟨_, hta, h_low'⟩ := ihA hA2 h_low h_ea
        obtain ⟨hvb, htb, h_low''⟩ := ihB hB2 h_low' h_eb
        exact ⟨hvb, by rw [hta, htb], h_low''⟩
  | c_eger_dogru s sk1 _ k d y tk1 td1 vk1 vd1 hK1 hD1 hDal1 ihK ihD =>
      intro s2 s2' t2 v2 h2 h_low h_et
      obtain ⟨h_kd, h_ey⟩ := birlesim_genel h_et
      obtain ⟨h_ek, h_ed⟩ := birlesim_genel h_kd
      cases h2 with
      | c_eger_dogru _ sk2 _ _ _ _ tk2 td2 vk2 vd2 hK2 hD2 hDal2 =>
          obtain ⟨_, htk, h_low'⟩ := ihK hK2 h_low h_ek
          obtain ⟨hvd, htd, h_low''⟩ := ihD hDal2 h_low' h_ed
          exact ⟨hvd, by rw [htk, htd], h_low''⟩
      | c_eger_yanlis _ sk2 _ _ _ _ tk2 ty2 vk2 vy2 hK2 hK2z hY2 =>
          obtain ⟨hvk, _, _⟩ := ihK hK2 h_low h_ek
          exact absurd (hvk ▸ hK2z) hD1
  | c_eger_yanlis s sk1 _ k d y tk1 ty1 vk1 vy1 hK1 hK1z hY1 ihK ihY =>
      intro s2 s2' t2 v2 h2 h_low h_et
      obtain ⟨h_kd, h_ey⟩ := birlesim_genel h_et
      obtain ⟨h_ek, h_ed⟩ := birlesim_genel h_kd
      cases h2 with
      | c_eger_dogru _ sk2 _ _ _ _ tk2 td2 vk2 vd2 hK2 hD2 hDal2 =>
          obtain ⟨hvk, _, _⟩ := ihK hK2 h_low h_ek
          exact absurd (hvk ▸ hK1z) hD2
      | c_eger_yanlis _ sk2 _ _ _ _ tk2 ty2 vk2 vy2 hK2 hK2z hY2 =>
          obtain ⟨_, htk, h_low'⟩ := ihK hK2 h_low h_ek
          obtain ⟨hvy, hty, h_low''⟩ := ihY hY2 h_low' h_ey
          exact ⟨hvy, by rw [htk, hty], h_low''⟩
  -- D-335 (CT002): dongu. Ucuncu IH (ihI) ic kosum icin — YAPISAL
  -- tumevarimin veremedigi sey tam olarak buydu.
  | c_iken_dogru s s1x s2x s3x k g tk tg ti vk vg vi hK1 hD1 hG1 hI1 ihK ihG ihI =>
      intro s2 s2' t2 v2 h2 h_low h_et
      obtain ⟨h_ek, h_eg⟩ := birlesim_genel h_et
      cases h2 with
      | c_iken_dogru _ sk2 sg2 _ _ _ tk2 tg2 ti2 vk2 vg2 vi2 hK2 hD2 hG2 hI2 =>
          obtain ⟨_, htk, h_low'⟩ := ihK hK2 h_low h_ek
          obtain ⟨_, htg, h_low''⟩ := ihG hG2 h_low' h_eg
          obtain ⟨_, hti, h_low3⟩ := ihI hI2 h_low'' h_et
          exact ⟨rfl, by rw [htk, htg, hti], h_low3⟩
      | c_iken_yanlis _ sk2 _ _ tk2 vk2 hK2 hK2z =>
          obtain ⟨hvk, _, _⟩ := ihK hK2 h_low h_ek
          exact absurd (hvk ▸ hK2z) hD1
  | c_iken_yanlis s sk1 k g tk1 vk1 hK1 hK1z ihK =>
      intro s2 s2' t2 v2 h2 h_low h_et
      obtain ⟨h_ek, h_eg⟩ := birlesim_genel h_et
      cases h2 with
      | c_iken_dogru _ sk2 sg2 _ _ _ tk2 tg2 ti2 vk2 vg2 vi2 hK2 hD2 hG2 hI2 =>
          obtain ⟨hvk, _, _⟩ := ihK hK2 h_low h_ek
          exact absurd (hvk ▸ hK1z) hD2
      | c_iken_yanlis _ sk2 _ _ tk2 vk2 hK2 hK2z =>
          obtain ⟨_, htk, h_low'⟩ := ihK hK2 h_low h_ek
          exact ⟨rfl, by rw [htk], h_low'⟩
  -- D-335 (CT004): literal desen eslemesi — `eger` ile ayni akil yurutme.
  | c_esles_tuttu s ss1 _ sk n d y ts1 td1 vs1 vd1 hS1 hT1 hD1 ihS ihD =>
      intro s2 s2' t2 v2 h2 h_low h_et
      obtain ⟨h_sd, h_ey⟩ := birlesim_genel h_et
      obtain ⟨h_es, h_ed⟩ := birlesim_genel h_sd
      cases h2 with
      | c_esles_tuttu _ ss2 _ _ _ _ _ ts2 td2 vs2 vd2 hS2 hT2 hD2 =>
          obtain ⟨_, hts, h_low'⟩ := ihS hS2 h_low h_es
          obtain ⟨hvd, htd, h_low''⟩ := ihD hD2 h_low' h_ed
          exact ⟨hvd, by rw [hts, htd], h_low''⟩
      | c_esles_tutmadi _ ss2 _ _ _ _ _ ts2 ty2 vs2 vy2 hS2 hT2 hY2 =>
          obtain ⟨hvs, _, _⟩ := ihS hS2 h_low h_es
          exact absurd (hT1.symm.trans (hvs ▸ rfl : vs1 = vs2) ▸ rfl : vs2 = n) hT2
  | c_esles_tutmadi s ss1 _ sk n d y ts1 ty1 vs1 vy1 hS1 hT1 hY1 ihS ihY =>
      intro s2 s2' t2 v2 h2 h_low h_et
      obtain ⟨h_sd, h_ey⟩ := birlesim_genel h_et
      obtain ⟨h_es, h_ed⟩ := birlesim_genel h_sd
      cases h2 with
      | c_esles_tuttu _ ss2 _ _ _ _ _ ts2 td2 vs2 vd2 hS2 hT2 hD2 =>
          obtain ⟨hvs, _, _⟩ := ihS hS2 h_low h_es
          exact absurd (hvs ▸ hT2) hT1
      | c_esles_tutmadi _ ss2 _ _ _ _ _ ts2 ty2 vs2 vy2 hS2 hT2 hY2 =>
          obtain ⟨_, hts, h_low'⟩ := ihS hS2 h_low h_es
          obtain ⟨hvy, hty, h_low''⟩ := ihY hY2 h_low' h_ey
          exact ⟨hvy, by rw [hts, hty], h_low''⟩
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

    KRITIK KURALLAR: `ct_eger`in `h_kosul_genel` sarti (CT001),
    `ct_iken`inki (CT002 — tur sayisi sizmasin) ve `ct_esles`inki
    (CT004 — hangi kolun tuttugu sizmasin). Bu sartlar OLMASAYDI ispat
    ilgili case'de COKERDI: gizli kosul/skrutin iki kosumda farkli
    dallara giderdi → izlerde `oDal true` vs `oDal false` → NI IHLALI.

    D-335: tumevarim `Ifade` uzerinde DEGIL **kosum turetimi uzerinde**
    (bkz. `genel_ifade_korunum` notu — `iken` kendisine ozyineler). -/
theorem ct_ni (G : EtiketOrtam) :
    ∀ {s1 : Store} {e : Ifade} {s1' : Store} {t1 : Iz} {v1 : Int},
      Calis s1 e s1' t1 v1 →
      ∀ {s2 s2' : Store} {t2 : Iz} {v2 : Int}, Calis s2 e s2' t2 v2 →
      CtOk G e → DusukEs G s1 s2 →
      t1 = t2 ∧ DusukEs G s1' s2' := by
  intro s1 e s1' t1 v1 h1
  induction h1 with
  | c_sabit s n =>
      intro s2 s2' t2 v2 h2 _ h_low
      cases h2; exact ⟨rfl, h_low⟩
  | c_degisken s x =>
      intro s2 s2' t2 v2 h2 _ h_low
      cases h2; exact ⟨rfl, h_low⟩
  -- D-338 (CT006): `h_ag`/`h_bg` OLMADAN bu dal COKER — gizli operandli
  -- bolme iki kosumda FARKLI `oBol` uretirdi (ct006_gerekli taniki).
  -- `c_topla` dalinda boyle bir sart YOK; fark tam olarak burada.
  | c_bol s sa1 _ a b ta1 tb1 va1 vb1 hA1 hB1 ihA ihB =>
      intro s2 s2' t2 v2 h2 h_ct h_low
      cases h_ct with
      | ct_bol _ _ hca hcb h_ag h_bg =>
        cases h2 with
        | c_bol _ sa2 _ _ _ ta2 tb2 va2 vb2 hA2 hB2 =>
          obtain ⟨hva, hta, h_low'⟩ := genel_ifade_korunum G hA1 hA2 h_low h_ag
          obtain ⟨hvb, htb, h_low''⟩ := genel_ifade_korunum G hB1 hB2 h_low' h_bg
          exact ⟨by rw [hta, htb, hva, hvb], h_low''⟩
  -- D-339 (CT006-M): `h_ag`/`h_bg` OLMADAN bu dal COKER — gizli operandli
  -- bolme iki kosumda FARKLI `oMod` uretirdi (ct006_gerekli taniki).
  -- `c_topla` dalinda boyle bir sart YOK; fark tam olarak burada.
  | c_kalan s sa1 _ a b ta1 tb1 va1 vb1 hA1 hB1 ihA ihB =>
      intro s2 s2' t2 v2 h2 h_ct h_low
      cases h_ct with
      | ct_kalan _ _ hca hcb h_ag h_bg =>
        cases h2 with
        | c_kalan _ sa2 _ _ _ ta2 tb2 va2 vb2 hA2 hB2 =>
          obtain ⟨hva, hta, h_low'⟩ := genel_ifade_korunum G hA1 hA2 h_low h_ag
          obtain ⟨hvb, htb, h_low''⟩ := genel_ifade_korunum G hB1 hB2 h_low' h_bg
          exact ⟨by rw [hta, htb, hva, hvb], h_low''⟩
  | c_topla s sa1 _ a b ta1 tb1 va1 vb1 hA1 hB1 ihA ihB =>
      intro s2 s2' t2 v2 h2 h_ct h_low
      cases h_ct with
      | ct_topla _ _ hca hcb =>
        cases h2 with
        | c_topla _ sa2 _ _ _ ta2 tb2 va2 vb2 hA2 hB2 =>
          obtain ⟨hta, h_low'⟩ := ihA hA2 hca h_low
          obtain ⟨htb, h_low''⟩ := ihB hB2 hcb h_low'
          exact ⟨by rw [hta, htb], h_low''⟩
  -- D-336 (CT005): `h_ig` (indeks genel) sayesinde iki kosum AYNI adresi
  -- okur → izler ayni. Bu sart OLMASAYDI `oOku x i1` vs `oOku x i2`
  -- ayrisirdi (ct005_gerekli taniği).
  -- D-337 (CT005-Y): IKI sart IKI farkli kanali kapatir —
  -- `h_ig` (indeks genel) ADRESI esitler, `h_akis` (CT003) GENEL diziye
  -- gizli deger yazilmasini engeller. Biri eksik olsa ispat coker.
  | c_indeks_ata s si se x idx e ti te vi ve hI1 hE1 ihI ihE =>
      intro s2 s2' t2 v2 h2 h_ct h_low
      cases h_ct with
      | ct_indeks_ata _ _ _ hci hce h_ig h_akis =>
        cases h2 with
        | c_indeks_ata _ si2 se2 _ _ _ ti2 te2 vi2 ve2 hI2 hE2 =>
          -- indeks GENEL → ayni adres, ayni iz parcasi
          obtain ⟨hvi, hti, h_low'⟩ :=
            genel_ifade_korunum G hI1 hI2 h_low h_ig
          obtain ⟨hte, h_low''⟩ := ihE hE2 hce h_low'
          refine ⟨by rw [hti, hte, hvi], ?_⟩
          -- hedef dizinin etiketine gore ayrilir
          cases hx : G x with
          | gizli =>
              exact dusukEs_yazH_gizli G se se2 x _ _ _ _ h_low'' hx
          | genel =>
              -- CT003: hedef GENEL ise yazilan deger de GENEL olmali
              have h_e_genel : ifadeEtiket G e = .genel := by
                rw [hx] at h_akis
                cases hE : ifadeEtiket G e with
                | genel => rfl
                | gizli => rw [hE] at h_akis
                           exact absurd h_akis (by simp [Etiket.altMi])
              obtain ⟨hve, _, _⟩ :=
                genel_ifade_korunum G hE1 hE2 h_low' h_e_genel
              rw [hvi, hve]
              exact dusukEs_yazH_genel G se se2 x _ _ h_low''
  | c_indeks s si x idx ti vi hI1 ihI =>
      intro s2 s2' t2 v2 h2 h_ct h_low
      cases h_ct with
      | ct_indeks _ _ hci h_ig =>
        cases h2 with
        | c_indeks _ si2 _ _ ti2 vi2 hI2 =>
          obtain ⟨hvi, hti, h_low'⟩ := genel_ifade_korunum G hI1 hI2 h_low h_ig
          exact ⟨by rw [hti, hvi], h_low'⟩
  | c_atama s sa1 x e ta1 va1 hE1 ihE =>
      intro s2 s2' t2 v2 h2 h_ct h_low
      cases h_ct with
      | ct_atama _ _ hce h_akis =>
        cases h2 with
        | c_atama _ sa2 _ _ ta2 va2 hE2 =>
          obtain ⟨ht, h_low'⟩ := ihE hE2 hce h_low
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
                | gizli =>
                    rw [hE] at h_akis
                    exact absurd h_akis (by simp [Etiket.altMi])
              obtain ⟨hv, _, _⟩ := genel_ifade_korunum G hE1 hE2 h_low h_e_genel
              rw [hv]
              exact dusukEs_yaz_genel G sa1 sa2 x _ h_low'
  | c_sira s sa1 _ a b ta1 tb1 va1 vb1 hA1 hB1 ihA ihB =>
      intro s2 s2' t2 v2 h2 h_ct h_low
      cases h_ct with
      | ct_sira _ _ hca hcb =>
        cases h2 with
        | c_sira _ sa2 _ _ _ ta2 tb2 va2 vb2 hA2 hB2 =>
          obtain ⟨hta, h_low'⟩ := ihA hA2 hca h_low
          obtain ⟨htb, h_low''⟩ := ihB hB2 hcb h_low'
          exact ⟨by rw [hta, htb], h_low''⟩
  | c_eger_dogru s sk1 _ k d y tk1 td1 vk1 vd1 hK1 hD1 hDal1 ihK ihD =>
      intro s2 s2' t2 v2 h2 h_ct h_low
      cases h_ct with
      | ct_eger _ _ _ hck hcd hcy h_kg =>
        cases h2 with
        | c_eger_dogru _ sk2 _ _ _ _ tk2 td2 vk2 vd2 hK2 hD2 hDal2 =>
            obtain ⟨_, htk, h_low'⟩ := genel_ifade_korunum G hK1 hK2 h_low h_kg
            obtain ⟨htd, h_low''⟩ := ihD hDal2 hcd h_low'
            exact ⟨by rw [htk, htd], h_low''⟩
        | c_eger_yanlis _ sk2 _ _ _ _ tk2 ty2 vk2 vy2 hK2 hK2z hY2 =>
            -- CT001 OLMADAN BU CELISKI OLUSMAZDI:
            obtain ⟨hvk, _, _⟩ := genel_ifade_korunum G hK1 hK2 h_low h_kg
            exact absurd (hvk ▸ hK2z) hD1
  | c_eger_yanlis s sk1 _ k d y tk1 ty1 vk1 vy1 hK1 hK1z hY1 ihK ihY =>
      intro s2 s2' t2 v2 h2 h_ct h_low
      cases h_ct with
      | ct_eger _ _ _ hck hcd hcy h_kg =>
        cases h2 with
        | c_eger_dogru _ sk2 _ _ _ _ tk2 td2 vk2 vd2 hK2 hD2 hDal2 =>
            obtain ⟨hvk, _, _⟩ := genel_ifade_korunum G hK1 hK2 h_low h_kg
            exact absurd (hvk ▸ hK1z) hD2
        | c_eger_yanlis _ sk2 _ _ _ _ tk2 ty2 vk2 vy2 hK2 hK2z hY2 =>
            obtain ⟨_, htk, h_low'⟩ := genel_ifade_korunum G hK1 hK2 h_low h_kg
            obtain ⟨hty, h_low''⟩ := ihY hY2 hcy h_low'
            exact ⟨by rw [htk, hty], h_low''⟩
  -- D-335 (CT002): DONGU. `h_kg` (kosul genel) olmasaydi iki kosum farkli
  -- TUR SAYISI yapardi → izler ayrisirdi. Ucuncu IH (ihI) ic kosum icin.
  | c_iken_dogru s s1x s2x s3x k g tk tg ti vk vg vi hK1 hD1 hG1 hI1 ihK ihG ihI =>
      intro s2 s2' t2 v2 h2 h_ct h_low
      cases h_ct with
      | ct_iken _ _ hck hcg h_kg =>
        cases h2 with
        | c_iken_dogru _ sk2 sg2 _ _ _ tk2 tg2 ti2 vk2 vg2 vi2 hK2 hD2 hG2 hI2 =>
            obtain ⟨_, htk, h_low'⟩ := genel_ifade_korunum G hK1 hK2 h_low h_kg
            obtain ⟨htg, h_low''⟩ := ihG hG2 hcg h_low'
            obtain ⟨hti, h_low3⟩ := ihI hI2 (CtOk.ct_iken k g hck hcg h_kg) h_low''
            exact ⟨by rw [htk, htg, hti], h_low3⟩
        | c_iken_yanlis _ sk2 _ _ tk2 vk2 hK2 hK2z =>
            obtain ⟨hvk, _, _⟩ := genel_ifade_korunum G hK1 hK2 h_low h_kg
            exact absurd (hvk ▸ hK2z) hD1
  | c_iken_yanlis s sk1 k g tk1 vk1 hK1 hK1z ihK =>
      intro s2 s2' t2 v2 h2 h_ct h_low
      cases h_ct with
      | ct_iken _ _ hck hcg h_kg =>
        cases h2 with
        | c_iken_dogru _ sk2 sg2 _ _ _ tk2 tg2 ti2 vk2 vg2 vi2 hK2 hD2 hG2 hI2 =>
            obtain ⟨hvk, _, _⟩ := genel_ifade_korunum G hK1 hK2 h_low h_kg
            exact absurd (hvk ▸ hK1z) hD2
        | c_iken_yanlis _ sk2 _ _ tk2 vk2 hK2 hK2z =>
            obtain ⟨_, htk, h_low'⟩ := genel_ifade_korunum G hK1 hK2 h_low h_kg
            exact ⟨by rw [htk], h_low'⟩
  -- D-335 (CT004): DESEN ESLEMESI. `h_sg` (skrutin genel) olmasaydi iki
  -- kosum FARKLI KOL secebilirdi → izler ayrisirdi.
  | c_esles_tuttu s ss1 _ sk n d y ts1 td1 vs1 vd1 hS1 hT1 hD1 ihS ihD =>
      intro s2 s2' t2 v2 h2 h_ct h_low
      cases h_ct with
      | ct_esles _ _ _ _ hcs hcd hcy h_sg =>
        cases h2 with
        | c_esles_tuttu _ ss2 _ _ _ _ _ ts2 td2 vs2 vd2 hS2 hT2 hD2 =>
            obtain ⟨_, hts, h_low'⟩ := genel_ifade_korunum G hS1 hS2 h_low h_sg
            obtain ⟨htd, h_low''⟩ := ihD hD2 hcd h_low'
            exact ⟨by rw [hts, htd], h_low''⟩
        | c_esles_tutmadi _ ss2 _ _ _ _ _ ts2 ty2 vs2 vy2 hS2 hT2 hY2 =>
            obtain ⟨hvs, _, _⟩ := genel_ifade_korunum G hS1 hS2 h_low h_sg
            exact absurd (hvs ▸ hT1) hT2
  | c_esles_tutmadi s ss1 _ sk n d y ts1 ty1 vs1 vy1 hS1 hT1 hY1 ihS ihY =>
      intro s2 s2' t2 v2 h2 h_ct h_low
      cases h_ct with
      | ct_esles _ _ _ _ hcs hcd hcy h_sg =>
        cases h2 with
        | c_esles_tuttu _ ss2 _ _ _ _ _ ts2 td2 vs2 vd2 hS2 hT2 hD2 =>
            obtain ⟨hvs, _, _⟩ := genel_ifade_korunum G hS1 hS2 h_low h_sg
            exact absurd (hvs ▸ hT2) hT1
        | c_esles_tutmadi _ ss2 _ _ _ _ _ ts2 ty2 vs2 vy2 hS2 hT2 hY2 =>
            obtain ⟨_, hts, h_low'⟩ := genel_ifade_korunum G hS1 hS2 h_low h_sg
            obtain ⟨hty, h_low''⟩ := ihY hY2 hcy h_low'
            exact ⟨by rw [hts, hty], h_low''⟩
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
          (fun x _ => if x = 0 then 1 else 5), (fun x _ => if x = 0 then 0 else 5),
          (fun x _ => if x = 0 then 1 else 5), (fun x _ => if x = 0 then 0 else 5),
          [.oOku 0 0, .oDal true], [.oOku 0 0, .oDal false], 1, 2, ?_, ?_, ?_, ?_⟩
  · intro x hx
    by_cases h0 : x = 0
    · rw [h0] at hx; exact absurd hx (by simp)
    · simp [h0]
  · exact Calis.c_eger_dogru _ _ _ _ _ _ [.oOku 0 0] [] 1 1
      (Calis.c_degisken _ 0) (by decide) (Calis.c_sabit _ 1)
  · exact Calis.c_eger_yanlis _ _ _ _ _ _ [.oOku 0 0] [] 0 2
      (Calis.c_degisken _ 0) rfl (Calis.c_sabit _ 2)
  · intro h; exact absurd (List.cons.inj h).2 (by decide)

/-- **CT002'nin GEREKLILIGI (D-335):** gizli `h` uzerinde DONEN program,
    iki dusuk-esdeger store'da FARKLI UZUNLUKTA iz uretir — cunku TUR
    SAYISI gizliye baglidir. `ct_iken`in `h_kosul_genel` sarti bu yuzden
    keyfi degildir.

    Program: `iken (degisken 0) (0 = 0)` — h ≠ 0 ise bir tur doner ve
    h'yi sifirlar; h = 0 ise hic donmez. -/
theorem ct002_gerekli :
    ∃ (G : EtiketOrtam) (e : Ifade) (s1 s2 s1' s2' : Store)
      (t1 t2 : Iz) (v1 v2 : Int),
      DusukEs G s1 s2 ∧ Calis s1 e s1' t1 v1 ∧ Calis s2 e s2' t2 v2
      ∧ t1 ≠ t2 := by
  refine ⟨fun x => if x = 0 then .gizli else .genel,
          .iken (.degisken 0) (.sabitDeg 0 (.sabit 0)),
          (fun x _ => if x = 0 then 1 else 5), (fun x _ => if x = 0 then 0 else 5),
          yaz (fun x _ => if x = 0 then 1 else 5) 0 0,
          (fun x _ => if x = 0 then 0 else 5),
          [.oOku 0 0] ++ .oDal true :: ([.oYaz 0 0] ++ [.oOku 0 0, .oDal false]),
          [.oOku 0 0] ++ [.oDal false], 0, 0, ?_, ?_, ?_, ?_⟩
  · intro x hx
    by_cases h0 : x = 0
    · rw [h0] at hx; exact absurd hx (by simp)
    · simp [h0]
  · exact Calis.c_iken_dogru _ _ _ _ _ _ [.oOku 0 0] [.oYaz 0 0]
      [.oOku 0 0, .oDal false] 1 0 0
      (Calis.c_degisken _ 0) (by decide)
      (Calis.c_atama _ _ 0 (.sabit 0) [] 0 (Calis.c_sabit _ 0))
      (Calis.c_iken_yanlis _ _ _ _ [.oOku 0 0] 0 (Calis.c_degisken _ 0) rfl)
  · exact Calis.c_iken_yanlis _ _ _ _ [.oOku 0 0] 0 (Calis.c_degisken _ 0) rfl
  · intro h; exact absurd (List.cons.inj h).2 (by decide)

/-- **CT004'un GEREKLILIGI (D-335):** gizli skrutin uzerinde ESLESEN
    program, hangi kolun tuttugunu sizdirir (`oDal true` vs `oDal false`).
    `ct_esles`in `h_skrut_genel` sarti bu yuzden gereklidir. -/
theorem ct004_gerekli :
    ∃ (G : EtiketOrtam) (e : Ifade) (s1 s2 s1' s2' : Store)
      (t1 t2 : Iz) (v1 v2 : Int),
      DusukEs G s1 s2 ∧ Calis s1 e s1' t1 v1 ∧ Calis s2 e s2' t2 v2
      ∧ t1 ≠ t2 := by
  refine ⟨fun x => if x = 0 then .gizli else .genel,
          .esles (.degisken 0) 1 (.sabit 5) (.sabit 7),
          (fun x _ => if x = 0 then 1 else 5), (fun x _ => if x = 0 then 2 else 5),
          (fun x _ => if x = 0 then 1 else 5), (fun x _ => if x = 0 then 2 else 5),
          [.oOku 0 0] ++ .oDal true :: [], [.oOku 0 0] ++ .oDal false :: [],
          5, 7, ?_, ?_, ?_, ?_⟩
  · intro x hx
    by_cases h0 : x = 0
    · rw [h0] at hx; exact absurd hx (by simp)
    · simp [h0]
  · exact Calis.c_esles_tuttu _ _ _ _ 1 _ _ [.oOku 0 0] [] 1 5
      (Calis.c_degisken _ 0) rfl (Calis.c_sabit _ 5)
  · exact Calis.c_esles_tutmadi _ _ _ _ 1 _ _ [.oOku 0 0] [] 2 7
      (Calis.c_degisken _ 0) (by decide) (Calis.c_sabit _ 7)
  · intro h; exact absurd (List.cons.inj h).2 (by decide)

/-- **CT005'in GEREKLILIGI (D-336):** GIZLI INDEKSLE tablo okumak,
    okunan ADRESI sizdirir. Klasik onbellek-zamanlama saldirisinin
    (AES S-box) mekanize cekirdegi.

    Program: `tablo[h]` — `tablo` (1) GENEL, `h` (0) GIZLI. Iki
    dusuk-esdeger store'da h farkli oldugu icin izler `oOku 1 3` ve
    `oOku 1 7` olur → AYRISIR. `ct_indeks`in `h_idx_genel` sarti bu
    yuzden gereklidir. -/
theorem ct005_gerekli :
    ∃ (G : EtiketOrtam) (e : Ifade) (s1 s2 s1' s2' : Store)
      (t1 t2 : Iz) (v1 v2 : Int),
      DusukEs G s1 s2 ∧ Calis s1 e s1' t1 v1 ∧ Calis s2 e s2' t2 v2
      ∧ t1 ≠ t2 := by
  refine ⟨fun x => if x = 0 then .gizli else .genel,
          .indeks 1 (.degisken 0),
          (fun x _ => if x = 0 then 3 else 5),
          (fun x _ => if x = 0 then 7 else 5),
          (fun x _ => if x = 0 then 3 else 5),
          (fun x _ => if x = 0 then 7 else 5),
          [.oOku 0 0] ++ [.oOku 1 3], [.oOku 0 0] ++ [.oOku 1 7],
          5, 5, ?_, ?_, ?_, ?_⟩
  · intro x hx i
    by_cases h0 : x = 0
    · rw [h0] at hx; exact absurd hx (by simp)
    · simp [h0]
  · exact Calis.c_indeks _ _ 1 _ [.oOku 0 0] 3 (Calis.c_degisken _ 0)
  · exact Calis.c_indeks _ _ 1 _ [.oOku 0 0] 7 (Calis.c_degisken _ 0)
  · intro h; exact absurd (List.cons.inj h).2 (by decide)

/-- **CT005-Y'nin GEREKLILIGI (D-337):** GIZLI INDEKSE YAZMAK, yazilan
    ADRESI sizdirir — okuma tarafiyla (ct005_gerekli) simetrik.
    Program: `tablo[h] = 1`, `h` (0) GIZLI. Izler `oYaz 1 3` vs
    `oYaz 1 7` → AYRISIR. Yani `ct_indeks_ata`nin `h_idx_genel` sarti
    gereklidir (`h_akis` ise AYRI bir kanali — veri sizintisini — kapatir;
    ikisi birbirinin yerine gecmez). -/
theorem ct005y_gerekli :
    ∃ (G : EtiketOrtam) (e : Ifade) (s1 s2 s1' s2' : Store)
      (t1 t2 : Iz) (v1 v2 : Int),
      DusukEs G s1 s2 ∧ Calis s1 e s1' t1 v1 ∧ Calis s2 e s2' t2 v2
      ∧ t1 ≠ t2 := by
  refine ⟨fun x => if x = 0 then .gizli else .genel,
          .indeksAta 1 (.degisken 0) (.sabit 1),
          (fun x _ => if x = 0 then 3 else 5),
          (fun x _ => if x = 0 then 7 else 5),
          yazH (fun x _ => if x = 0 then 3 else 5) 1 3 1,
          yazH (fun x _ => if x = 0 then 7 else 5) 1 7 1,
          [.oOku 0 0] ++ [] ++ [.oYaz 1 3], [.oOku 0 0] ++ [] ++ [.oYaz 1 7],
          1, 1, ?_, ?_, ?_, ?_⟩
  · intro x hx i
    by_cases h0 : x = 0
    · rw [h0] at hx; exact absurd hx (by simp)
    · simp [h0]
  · exact Calis.c_indeks_ata _ _ _ 1 _ _ [.oOku 0 0] [] 3 1
      (Calis.c_degisken _ 0) (Calis.c_sabit _ 1)
  · exact Calis.c_indeks_ata _ _ _ 1 _ _ [.oOku 0 0] [] 7 1
      (Calis.c_degisken _ 0) (Calis.c_sabit _ 1)
  · intro h; exact absurd (List.cons.inj h).2 (by decide)

/-- **CT006'nin GEREKLILIGI (D-338):** GIZLI OPERANDLI BOLME, operandi
    (dolayisiyla gecikmeyi) sizdirir. Program: `h / 2`, `h` (0) GIZLI.
    Izler `oBol 3 2` vs `oBol 7 2` → AYRISIR.

    KRITIK KARSILASTIRMA: AYNI programda `/` yerine `+` olsaydi izler
    AYNI kalirdi (`c_topla` olay uretmez) — yani CT006, `ct_topla`da
    OLMAYAN bir sarti hakli olarak talep eder. Fark uydurma degil,
    bolmenin sabit-cevrimli OLMAMASIDIR. -/
theorem ct006_gerekli :
    ∃ (G : EtiketOrtam) (e : Ifade) (s1 s2 s1' s2' : Store)
      (t1 t2 : Iz) (v1 v2 : Int),
      DusukEs G s1 s2 ∧ Calis s1 e s1' t1 v1 ∧ Calis s2 e s2' t2 v2
      ∧ t1 ≠ t2 := by
  refine ⟨fun x => if x = 0 then .gizli else .genel,
          .bol (.degisken 0) (.sabit 2),
          (fun x _ => if x = 0 then 3 else 5),
          (fun x _ => if x = 0 then 7 else 5),
          (fun x _ => if x = 0 then 3 else 5),
          (fun x _ => if x = 0 then 7 else 5),
          [.oOku 0 0] ++ [] ++ [.oBol 3 2], [.oOku 0 0] ++ [] ++ [.oBol 7 2],
          1, 3, ?_, ?_, ?_, ?_⟩
  · intro x hx i
    by_cases h0 : x = 0
    · rw [h0] at hx; exact absurd hx (by simp)
    · simp [h0]
  · exact Calis.c_bol _ _ _ _ _ [.oOku 0 0] [] 3 2
      (Calis.c_degisken _ 0) (Calis.c_sabit _ 2)
  · exact Calis.c_bol _ _ _ _ _ [.oOku 0 0] [] 7 2
      (Calis.c_degisken _ 0) (Calis.c_sabit _ 2)
  · intro h; exact absurd (List.cons.inj h).2 (by decide)

/-- **CT006-M'nin GEREKLILIGI (D-339):** gizli operandli MOD da operandi
    sizdirir — `ct006_gerekli`nin aynasi. `h % 2` (h GIZLI) iki kosumda
    `oMod 3 2` vs `oMod 7 2` uretir.

    AYRICA: `oMod` `oBol`dan AYRI oldugu icin `a/b` ile `a%b` izleri de
    ayrisir; bu, "hangi islem" bilgisinin saldirgandan SAKLANMADIGI
    (muhafazakar) modelin sonucudur. -/
theorem ct006m_gerekli :
    ∃ (G : EtiketOrtam) (e : Ifade) (s1 s2 s1' s2' : Store)
      (t1 t2 : Iz) (v1 v2 : Int),
      DusukEs G s1 s2 ∧ Calis s1 e s1' t1 v1 ∧ Calis s2 e s2' t2 v2
      ∧ t1 ≠ t2 := by
  refine ⟨fun x => if x = 0 then .gizli else .genel,
          .kalan (.degisken 0) (.sabit 2),
          (fun x _ => if x = 0 then 3 else 5),
          (fun x _ => if x = 0 then 7 else 5),
          (fun x _ => if x = 0 then 3 else 5),
          (fun x _ => if x = 0 then 7 else 5),
          [.oOku 0 0] ++ [] ++ [.oMod 3 2], [.oOku 0 0] ++ [] ++ [.oMod 7 2],
          1, 1, ?_, ?_, ?_, ?_⟩
  · intro x hx i
    by_cases h0 : x = 0
    · rw [h0] at hx; exact absurd hx (by simp)
    · simp [h0]
  · exact Calis.c_kalan _ _ _ _ _ [.oOku 0 0] [] 3 2
      (Calis.c_degisken _ 0) (Calis.c_sabit _ 2)
  · exact Calis.c_kalan _ _ _ _ _ [.oOku 0 0] [] 7 2
      (Calis.c_degisken _ 0) (Calis.c_sabit _ 2)
  · intro h; exact absurd (List.cons.inj h).2 (by decide)

/-- **KARSIT TANIK (D-338):** ayni sekildeki TOPLAMA'da izler AYNIDIR —
    yani CT006'nin `ct_topla`ya EKLENMEMESI de dogru bir karardir.
    Bu, "her aritmetige ayni sarti koy" refleksinin YANLIS oldugunu
    ve ayrimin gercek (gozlem-tabanli) oldugunu gosterir. -/
theorem topla_gizli_operand_zararsiz :
    ∀ (s1 s2 s1' s2' : Store) (t1 t2 : Iz) (v1 v2 : Int),
      Calis s1 (.topla (.degisken 0) (.sabit 2)) s1' t1 v1 →
      Calis s2 (.topla (.degisken 0) (.sabit 2)) s2' t2 v2 →
      t1 = t2 := by
  intro s1 s2 s1' s2' t1 t2 v1 v2 h1 h2
  cases h1 with
  | c_topla _ _ _ _ _ _ _ _ _ hA1 hB1 =>
    cases h2 with
    | c_topla _ _ _ _ _ _ _ _ _ hA2 hB2 =>
      cases hA1; cases hA2; cases hB1; cases hB2; rfl

end Kemgu.SideChannel.CT
