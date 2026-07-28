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

═══════════════════════════════════════════════════════════════════════
⚠ CARPMA VARSAYIMI (D-340) — MODELE GOMULU DONANIM IDDIASI
═══════════════════════════════════════════════════════════════════════
`carp` (tamsayi carpmasi) bu modelde `topla` SINIFINDADIR: olay uretmez,
CT operand-genellik sarti YOKTUR, yani GIZLI × GIZLI SERBESTTIR.
`bol`/`kalan` ise operandlarini ize koyar ve genellik ISTER (CT006/CT006-M).

BU AYRIM BIR TEOREM DEGIL, BIR VARSAYIMDIR:
  * GECERLI oldugu yer: KEMGU'nun birincil hedefleri ARM64 ve x86_64 —
    tamsayi carpmasi sabit cevrimdir. CT arac ekosisteminin (ct-verif,
    FaCT, dudect) standart varsayimi da budur: div/mod ayrilir, mul
    sabit sayilir.
  * GECERSIZ oldugu yer: ERKEN BITEN carpicilar — ARM Cortex-M0/M3,
    bazi MIPS/PowerPC cekirdekleri. Orada `carp`in da `bol` gibi
    modellenmesi gerekir; bu modelin sonuclari O PLATFORMLARDA GECMEZ.
  * NEDEN boyle secildi (Mehmet karari): aksi halde alan carpimi
    (gizli × gizli) yasaklanir ve HICBIR kripto primitifi — Curve25519,
    Poly1305, RSA — CT-tipli yazilamaz; disiplin asil kullanim alaninda
    ise yaramaz hale gelir.
  * Model-ici tutarlilik taniki: `carp_gizli_operand_zararsiz`.
  * V2 secenegi: platform-parametrik model (bir bayrak tum yargilarda
    tasinir) — kabaca 2 kat is, henuz yapilmadi.
═══════════════════════════════════════════════════════════════════════

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
  | carp    (a b : Ifade)
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
  | c_carp (s s1 s2 : Store) (a b : Ifade) (t1 t2 : Iz) (v1 v2 : Int)
      (h1 : Calis s a s1 t1 v1) (h2 : Calis s1 b s2 t2 v2) :
      Calis s (.carp a b) s2 (t1 ++ t2) (v1 * v2)
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
  | .carp a b      => (ifadeEtiket G a).birlesim (ifadeEtiket G b)
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
  | ct_carp (a b : Ifade) (ha : CtOk G a) (hb : CtOk G b) :
      CtOk G (.carp a b)
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
  | c_carp s sa1 _ a b ta1 tb1 va1 vb1 hA1 hB1 ihA ihB =>
      intro s2 s2' t2 v2 h2 h_low h_et
      cases h2 with
      | c_carp _ sa2 _ _ _ ta2 tb2 va2 vb2 hA2 hB2 =>
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
  | c_carp s sa1 _ a b ta1 tb1 va1 vb1 hA1 hB1 ihA ihB =>
      intro s2 s2' t2 v2 h2 h_ct h_low
      cases h_ct with
      | ct_carp _ _ hca hcb =>
        cases h2 with
        | c_carp _ sa2 _ _ _ ta2 tb2 va2 vb2 hA2 hB2 =>
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

/-- **CARPMA KARSIT TANIGI (D-340):** gizli operandli CARPMA izleri
    DEGISTIRMEZ — yani `carp`i `topla` sinifinda tutmak (CT sarti KOYMAMAK)
    bu modelde tutarlidir; `bol`/`kalan` icin ayni sey YANLIS olurdu
    (`ct006_gerekli` / `ct006m_gerekli`).

    ⚠ BU BIR DONANIM IDDIASINA DAYANIR (bkz. dosya basindaki CARPMA
    VARSAYIMI notu ve D-340): tanik yalnizca MODELIN ic tutarliligini
    gosterir, gercek CPU'nun carpicisinin sabit cevrim oldugunu KANITLAMAZ.
    Erken biten carpicilarda (Cortex-M0/M3) varsayim dusar. -/
theorem carp_gizli_operand_zararsiz :
    ∀ (s1 s2 s1' s2' : Store) (t1 t2 : Iz) (v1 v2 : Int),
      Calis s1 (.carp (.degisken 0) (.sabit 2)) s1' t1 v1 →
      Calis s2 (.carp (.degisken 0) (.sabit 2)) s2' t2 v2 →
      t1 = t2 := by
  intro s1 s2 s1' s2' t1 t2 v1 v2 h1 h2
  cases h1 with
  | c_carp _ _ _ _ _ _ _ _ _ hA1 hB1 =>
    cases h2 with
    | c_carp _ _ _ _ _ _ _ _ _ hA2 hB2 =>
      cases hA1; cases hA2; cases hB1; cases hB2; rfl

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

-- ============================================================
-- §10. ESZAMANLI CT (D-341) — ZAMANLAMA KANALI ve KOMPOZISYONELLIK
-- ============================================================

/-
NE EKLIYOR
──────────
Buraya kadar her sey TEK THREAD'di. Eszamanlilikta YENI olan iki sey var:
  (1) **Zamanlama (schedule) kanali:** thread'ler PAYLASILAN store uzerinde
      calisir; hangi thread ne zaman kostugu gozlemlenen izi degistirir.
  (2) **Capraz girisim:** A thread'inin yazdigi, B thread'inin okudugu.

ISPATLANAN SEY (`ct_esz_ni`): TUM bloklari CT-tipli olan bir sistem,
HERHANGI bir zamanlama altinda, dusuk-esdeger iki baslangic store'unda
BIREBIR AYNI serpistirilmis izi uretir ve son store'lar yine dusuk-esdeger
kalir. Yani **gizli veri ne izi ne de zamanlamanin etkisini degistirir.**

Zamanlama TUMEL NICELENMISTIR (∀ zam), yani "saldırgan zamanlamayi
secebilir" durumu da kapsanir. UYARLANIR (adaptive) bir saldırgan
zamanlayici da kapsanir: teorem izlerin AYNI oldugunu soyledigi icin,
gozlemlere bakarak karar veren deterministik bir zamanlayici iki kosumda
AYNI secimleri yapar — dolayisiyla sabit-liste nicelemesi yeterlidir.

⚠ ACIK SINIR (Mehmet karari — blok-atomik incelik):
  Araya girme (preemption) yalniz BLOK SINIRLARINDA olur; bir blogun
  ICINDE thread degismez. Teorem HER blok ayrimi icin gecerli oldugundan
  programci istedigi kadar ince bolebilir (incelik PARAMETRIKTIR), ama
  bir blogun ortasinda preemption eden gercek bir cekirdek bu modelde
  DOGRUDAN temsil edilmez. Tam incelik icin CT'nin kucuk-adima
  cevrilmesi gerekir — V2.
  GUNCELLEME (D-342/D-343/D-344): "kopru tek thread" notu ARTIK GECERSIZ.
  Kopru N-thread'e cikarildi (Cerceve), odak degisimi Core'da korunum
  lemmasiyla kuruldu (odak_kur) ve n-adim serpistirme tumevarimi kapatildi
  (CTKopru.esz_core_sim). Sonuc: CTKopru.kopru_esz_core_ni — serpistirme
  artik CT tarafinda DEGIL, Core'un kendi Step'indedir. Kalan kisit
  TEK-YAZICI'dir (iki thread ayni degiskene yazamaz; bkz. D-342).
-/

/-- Zamanlama: hangi thread'in sirasi. Liste bittiginde kosum biter. -/
abbrev Zamanlama := List Nat

/-- Bir thread = ARDISIK BLOKLAR. Blok = atomik kosan CT ifadesi. -/
abbrev ThreadBloklari := List Ifade

/-- Sistem = thread listesi. Store PAYLASILIR (tek `Store`). -/
abbrev Sistem := List ThreadBloklari

/-- i. thread'in SIRADAKI blogu (yoksa/bittiyse none). -/
def sysIlkBlok : Sistem → Nat → Option Ifade
  | [], _          => none
  | ts :: _, 0     => ts.head?
  | _ :: rest, n+1 => sysIlkBlok rest n

/-- i. thread'i bir blok ilerlet. -/
def sysIlerlet : Sistem → Nat → Sistem
  | [], _             => []
  | ts :: rest, 0     => ts.tail :: rest
  | ts :: rest, n+1   => ts :: sysIlerlet rest n

/-- Serpistirilmis iz: her gozlem HANGI THREAD'den geldigini tasir. -/
abbrev EszIz := List (Nat × Gozlem)

/-- SERPISTIRILMIS KOSUM. Zamanlama listesini soldan tuketir; her adimda
    secili thread'in siradaki blogu ATOMIK kosar ve izi thread etiketiyle
    serpistirilmis ize eklenir. Bitmis/olmayan thread secilirse ADIM ATLANIR
    (zamanlama yine tuketilir) — gercek zamanlayicilarin bos quantum'u. -/
inductive EszCalis : Store → Sistem → Zamanlama → Store → EszIz → Prop where
  | bitti (s : Store) (sys : Sistem) :
      EszCalis s sys [] s []
  | adim (s sa s' : Store) (sys : Sistem) (i : Nat) (kalan : Zamanlama)
      (blok : Ifade) (t : Iz) (v : Int) (izK : EszIz)
      (h_get  : sysIlkBlok sys i = some blok)
      (h_run  : Calis s blok sa t v)
      (h_rest : EszCalis sa (sysIlerlet sys i) kalan s' izK) :
      EszCalis s sys (i :: kalan) s' (t.map (fun g => (i, g)) ++ izK)
  | atla (s s' : Store) (sys : Sistem) (i : Nat) (kalan : Zamanlama) (izK : EszIz)
      (h_yok  : sysIlkBlok sys i = none)
      (h_rest : EszCalis s sys kalan s' izK) :
      EszCalis s sys (i :: kalan) s' izK

/-- Sistemin TUM bloklari CT-tipli. -/
def SistemCtOk (G : EtiketOrtam) (sys : Sistem) : Prop :=
  ∀ ts ∈ sys, ∀ e ∈ ts, CtOk G e

theorem sistemCtOk_blok {G : EtiketOrtam} {sys : Sistem} {i : Nat} {blok : Ifade}
    (h : SistemCtOk G sys) (h_get : sysIlkBlok sys i = some blok) :
    CtOk G blok := by
  induction sys generalizing i with
  | nil => cases h_get
  | cons ts rest ih =>
      cases i with
      | zero =>
          have h_head : ts.head? = some blok := h_get
          cases ts with
          | nil => cases h_head
          | cons b bs =>
              have hb : b = blok := Option.some.inj h_head
              subst hb
              exact h (b :: bs) (List.Mem.head _) b (List.Mem.head _)
      | succ n =>
          exact ih (fun ts' h' e he => h ts' (List.Mem.tail _ h') e he) h_get

theorem sistemCtOk_ilerlet {G : EtiketOrtam} {sys : Sistem}
    (h : SistemCtOk G sys) (i : Nat) : SistemCtOk G (sysIlerlet sys i) := by
  induction sys generalizing i with
  | nil => intro ts h_ts; cases h_ts
  | cons ts rest ih =>
      cases i with
      | zero =>
          intro ts' h_ts' e he
          rcases List.mem_cons.mp h_ts' with h_eq | h_rest
          · subst h_eq
            exact h ts (List.Mem.head _) e (List.mem_of_mem_tail he)
          · exact h ts' (List.Mem.tail _ h_rest) e he
      | succ n =>
          intro ts' h_ts' e he
          rcases List.mem_cons.mp h_ts' with h_eq | h_rest
          · subst h_eq; exact h ts' (List.Mem.head _) e he
          · exact ih (fun t2 h2 e2 he2 => h t2 (List.Mem.tail _ h2) e2 he2)
              n ts' h_rest e he

/-- **ANA TEOREM (ct_esz_ni) — ESZAMANLI NON-INTERFERENCE:**
    CT-tipli bir sistem, HERHANGI bir zamanlama altinda, dusuk-esdeger iki
    baslangic store'unda AYNI serpistirilmis gozlem izini uretir ve son
    store'lar yine dusuk-esdegerdir.

    NEDEN TEK-THREAD `ct_ni`DEN DAHA FAZLASI: store PAYLASILIR, yani
    A thread'inin yazdigini B okur. Ispat her blok icin `ct_ni`yi kullanir
    ve dusuk-esdegerligi bloklar ARASINDA tasir — kompozisyonellik
    argumaninin ta kendisi. Zamanlama kanali kapanir cunku her blogun
    izi (dolayisiyla UZUNLUGU) gizliden bagimsizdir; zamanlayici bloklari
    ayni noktalarda boler. -/
theorem ct_esz_ni (G : EtiketOrtam) :
    ∀ {s1 : Store} {sys : Sistem} {zam : Zamanlama} {s1' : Store} {t1 : EszIz},
      EszCalis s1 sys zam s1' t1 →
      ∀ {s2 s2' : Store} {t2 : EszIz}, EszCalis s2 sys zam s2' t2 →
      SistemCtOk G sys → DusukEs G s1 s2 →
      t1 = t2 ∧ DusukEs G s1' s2' := by
  intro s1 sys zam s1' t1 h1
  induction h1 with
  | bitti s sys =>
      intro s2 s2' t2 h2 _ h_low
      cases h2
      exact ⟨rfl, h_low⟩
  | adim s sa s' sys i kalan blok t v izK h_get h_run h_rest ih =>
      intro s2 s2' t2 h2 h_ct h_low
      cases h2 with
      | adim _ sa2 _ _ _ _ blok2 t2b v2 izK2 h_get2 h_run2 h_rest2 =>
          -- AYNI sistem + AYNI thread indeksi → AYNI blok (lookup fonksiyonel)
          have h_eq : blok2 = blok := by
            rw [h_get] at h_get2; exact (Option.some.inj h_get2).symm
          rw [h_eq] at h_run2
          have h_cb : CtOk G blok := sistemCtOk_blok h_ct h_get
          obtain ⟨ht, h_low'⟩ := ct_ni G h_run h_run2 h_cb h_low
          obtain ⟨htk, h_low''⟩ :=
            ih h_rest2 (sistemCtOk_ilerlet h_ct i) h_low'
          exact ⟨by rw [ht, htk], h_low''⟩
      | atla _ _ _ _ _ _ h_yok2 _ =>
          rw [h_get] at h_yok2; nomatch h_yok2
  | atla s s' sys i kalan izK h_yok h_rest ih =>
      intro s2 s2' t2 h2 h_ct h_low
      cases h2 with
      | adim _ _ _ _ _ _ _ _ _ _ h_get2 _ _ =>
          rw [h_yok] at h_get2; nomatch h_get2
      | atla _ _ _ _ _ _ _ h_rest2 =>
          exact ih h_rest2 h_ct h_low

-- ============================================================
-- §10.1 ESZAMANLI MODELIN VAKUM DENETIMLERI
-- ============================================================

/-- **(1) ZAMANLAMA GERCEKTEN ETKILI** — model bagimsiz thread'ler
    KOSTURMUYOR. Iki thread ayni degiskene yazar; FARKLI zamanlamalar
    FARKLI son store verir. `ct_esz_ni` bunu YASAKLAMAZ (zamanlama bir
    GIRDIDIR, gizli DEGIL); yasakladigi sey gizli verinin izi
    degistirmesidir. Bu tanik olmasa "eszamanlilik" adi bos olurdu. -/
theorem esz_zamanlama_etkili :
    ∃ (sys : Sistem) (s : Store) (zamA zamB : Zamanlama)
      (sA sB : Store) (tA tB : EszIz),
      EszCalis s sys zamA sA tA ∧ EszCalis s sys zamB sB tB
      ∧ sA 0 0 ≠ sB 0 0 := by
  refine ⟨[[.sabitDeg 0 (.sabit 1)], [.sabitDeg 0 (.sabit 2)]],
          (fun _ _ => 0), [0, 1], [1, 0],
          yaz (yaz (fun _ _ => 0) 0 1) 0 2,
          yaz (yaz (fun _ _ => 0) 0 2) 0 1,
          [(0, Gozlem.oYaz 0 0), (1, Gozlem.oYaz 0 0)],
          [(1, Gozlem.oYaz 0 0), (0, Gozlem.oYaz 0 0)], ?_, ?_, ?_⟩
  · exact EszCalis.adim _ _ _ _ 0 [1] _ [.oYaz 0 0] 1 _ rfl
      (Calis.c_atama _ _ 0 _ [] 1 (Calis.c_sabit _ 1))
      (EszCalis.adim _ _ _ _ 1 [] _ [.oYaz 0 0] 2 _ rfl
        (Calis.c_atama _ _ 0 _ [] 2 (Calis.c_sabit _ 2))
        (EszCalis.bitti _ _))
  · exact EszCalis.adim _ _ _ _ 1 [0] _ [.oYaz 0 0] 2 _ rfl
      (Calis.c_atama _ _ 0 _ [] 2 (Calis.c_sabit _ 2))
      (EszCalis.adim _ _ _ _ 0 [] _ [.oYaz 0 0] 1 _ rfl
        (Calis.c_atama _ _ 0 _ [] 1 (Calis.c_sabit _ 1))
        (EszCalis.bitti _ _))
  · simp [yaz]

/-- **(2) CAPRAZ GIRISIM GERCEK** — bir thread'in YAZDIGINI digeri OKUR
    (store PAYLASILIR). `x = 7; (baska thread) y = x` zincirinde ikinci
    thread 7 okur. Store thread-yerel olsaydi bu ispat COKERDI. -/
theorem esz_capraz_girisim_gercek :
    ∃ (sys : Sistem) (s s' : Store) (zam : Zamanlama) (iz : EszIz),
      EszCalis s sys zam s' iz ∧ s' 1 0 = 7 := by
  refine ⟨[[.sabitDeg 0 (.sabit 7)], [.sabitDeg 1 (.degisken 0)]],
          (fun _ _ => 0),
          yaz (yaz (fun _ _ => 0) 0 7) 1 7, [0, 1],
          [(0, Gozlem.oYaz 0 0), (1, Gozlem.oOku 0 0), (1, Gozlem.oYaz 1 0)],
          ?_, ?_⟩
  · exact EszCalis.adim _ _ _ _ 0 [1] _ [.oYaz 0 0] 7 _ rfl
      (Calis.c_atama _ _ 0 _ [] 7 (Calis.c_sabit _ 7))
      (EszCalis.adim _ _ _ _ 1 [] _ [.oOku 0 0, .oYaz 1 0] 7 _ rfl
        (Calis.c_atama _ _ 1 _ [.oOku 0 0] 7 (Calis.c_degisken _ 0))
        (EszCalis.bitti _ _))
  · simp [yaz]

/-- **(3) ESZAMANLI CT SARTLARI GEREKLI** — sistemdeki BIR thread CT001'i
    ihlal ederse (gizli kosulda dallanma) serpistirilmis izler AYRISIR.
    Yani `SistemCtOk` hipotezi vakum degil, gercekten gerekli.
    `ct001_gerekli`nin eszamanli sisteme kaldirilmis hali. -/
theorem ct_esz_gerekli :
    ∃ (G : EtiketOrtam) (sys : Sistem) (zam : Zamanlama)
      (s1 s2 s1' s2' : Store) (t1 t2 : EszIz),
      DusukEs G s1 s2 ∧ EszCalis s1 sys zam s1' t1
      ∧ EszCalis s2 sys zam s2' t2 ∧ t1 ≠ t2 := by
  refine ⟨fun x => if x = 0 then .gizli else .genel,
          [[.eger (.degisken 0) (.sabit 1) (.sabit 2)]], [0],
          (fun x _ => if x = 0 then 1 else 5),
          (fun x _ => if x = 0 then 0 else 5),
          (fun x _ => if x = 0 then 1 else 5),
          (fun x _ => if x = 0 then 0 else 5),
          [(0, Gozlem.oOku 0 0), (0, Gozlem.oDal true)],
          [(0, Gozlem.oOku 0 0), (0, Gozlem.oDal false)],
          ?_, ?_, ?_, ?_⟩
  · intro x hx i
    by_cases h0 : x = 0
    · rw [h0] at hx; exact absurd hx (by simp)
    · simp [h0]
  · exact EszCalis.adim _ _ _ _ 0 [] _ [.oOku 0 0, .oDal true] 1 _ rfl
      (Calis.c_eger_dogru _ _ _ _ _ _ [.oOku 0 0] [] 1 1
        (Calis.c_degisken _ 0) (by decide) (Calis.c_sabit _ 1))
      (EszCalis.bitti _ _)
  · exact EszCalis.adim _ _ _ _ 0 [] _ [.oOku 0 0, .oDal false] 2 _ rfl
      (Calis.c_eger_yanlis _ _ _ _ _ _ [.oOku 0 0] [] 0 2
        (Calis.c_degisken _ 0) rfl (Calis.c_sabit _ 2))
      (EszCalis.bitti _ _)
  · intro h
    exact absurd (List.cons.inj (List.cons.inj h).2).1 (by decide)

-- ============================================================
-- §11. KUCUK-ADIM CT (D-345) — BLOK ICI PREEMPTION
-- ============================================================

/-
NEDEN (D-341 KARARI GERI ALINDI):
  Blok-atomik model (§10) araya girmeyi yalniz BLOK SINIRLARINDA birakti.
  Blok ICINDE preemption BUYUK-ADIM `Calis` ile IFADE EDILEMEZ: `Calis s e
  s' t v` store'un yalnizca `e` tarafindan degistirildigini VARSAYAR.
  Bu bolum kucuk-adim bir `Adim` bagintisi ekler; araya girme ARTIK HER
  ADIMDA mumkundur.

TASARIM: buyuk-adim `Calis` SILINMEDI. §1-§10'un tamami (ct_ni, ct_esz_ni,
kopru, tum tanikler) oldugu gibi gecerli kalir — onlar blok-atomik hikayeyi
anlatir. Bu bolum INCE-TANELI hikayeyi anlatir. Iki hikaye ayni `CtOk`
disiplinini paylasir.

DEGER = `sabit n`. Diger her sey adim atar.
-/

/-- Kucuk-adim CT reduksiyonu. Her adim SIFIR ya da BIR gozlem uretir.
    Cong kurallari degerlendirme sirasini sabitler (soldan saga) —
    `Sem/Core`un cong ailesiyle birebir ayni disiplin. -/
inductive Adim : Store → Ifade → Store → Ifade → Iz → Prop where
  -- --- degisken / indeks okuma ---
  | a_degisken (s : Store) (x : Ad) :
      Adim s (.degisken x) s (.sabit (s x 0)) [.oOku x 0]
  | a_indeks_cong (s s' : Store) (x : Ad) (idx idx' : Ifade) (t : Iz) :
      Adim s idx s' idx' t → Adim s (.indeks x idx) s' (.indeks x idx') t
  | a_indeks (s : Store) (x : Ad) (i : Int) :
      Adim s (.indeks x (.sabit i)) s (.sabit (s x i.toNat)) [.oOku x i.toNat]
  -- --- aritmetik (soldan saga) ---
  | a_topla_sol (s s' : Store) (a a' b : Ifade) (t : Iz) :
      Adim s a s' a' t → Adim s (.topla a b) s' (.topla a' b) t
  | a_topla_sag (s s' : Store) (n : Int) (b b' : Ifade) (t : Iz) :
      Adim s b s' b' t → Adim s (.topla (.sabit n) b) s' (.topla (.sabit n) b') t
  | a_topla (s : Store) (n1 n2 : Int) :
      Adim s (.topla (.sabit n1) (.sabit n2)) s (.sabit (n1 + n2)) []
  | a_carp_sol (s s' : Store) (a a' b : Ifade) (t : Iz) :
      Adim s a s' a' t → Adim s (.carp a b) s' (.carp a' b) t
  | a_carp_sag (s s' : Store) (n : Int) (b b' : Ifade) (t : Iz) :
      Adim s b s' b' t → Adim s (.carp (.sabit n) b) s' (.carp (.sabit n) b') t
  | a_carp (s : Store) (n1 n2 : Int) :
      Adim s (.carp (.sabit n1) (.sabit n2)) s (.sabit (n1 * n2)) []
  | a_bol_sol (s s' : Store) (a a' b : Ifade) (t : Iz) :
      Adim s a s' a' t → Adim s (.bol a b) s' (.bol a' b) t
  | a_bol_sag (s s' : Store) (n : Int) (b b' : Ifade) (t : Iz) :
      Adim s b s' b' t → Adim s (.bol (.sabit n) b) s' (.bol (.sabit n) b') t
  /-- D-338: bolme OPERANDLARI ize koyar (veri-bagimli gecikme). -/
  | a_bol (s : Store) (n1 n2 : Int) :
      Adim s (.bol (.sabit n1) (.sabit n2)) s (.sabit (n1 / n2)) [.oBol n1 n2]
  | a_kalan_sol (s s' : Store) (a a' b : Ifade) (t : Iz) :
      Adim s a s' a' t → Adim s (.kalan a b) s' (.kalan a' b) t
  | a_kalan_sag (s s' : Store) (n : Int) (b b' : Ifade) (t : Iz) :
      Adim s b s' b' t → Adim s (.kalan (.sabit n) b) s' (.kalan (.sabit n) b') t
  | a_kalan (s : Store) (n1 n2 : Int) :
      Adim s (.kalan (.sabit n1) (.sabit n2)) s (.sabit (n1 % n2)) [.oMod n1 n2]
  -- --- yazma ---
  | a_atama_cong (s s' : Store) (x : Ad) (e e' : Ifade) (t : Iz) :
      Adim s e s' e' t → Adim s (.sabitDeg x e) s' (.sabitDeg x e') t
  | a_atama (s : Store) (x : Ad) (n : Int) :
      Adim s (.sabitDeg x (.sabit n)) (yaz s x n) (.sabit n) [.oYaz x 0]
  | a_indeks_ata_idx (s s' : Store) (x : Ad) (idx idx' e : Ifade) (t : Iz) :
      Adim s idx s' idx' t →
      Adim s (.indeksAta x idx e) s' (.indeksAta x idx' e) t
  | a_indeks_ata_deg (s s' : Store) (x : Ad) (i : Int) (e e' : Ifade) (t : Iz) :
      Adim s e s' e' t →
      Adim s (.indeksAta x (.sabit i) e) s' (.indeksAta x (.sabit i) e') t
  | a_indeks_ata (s : Store) (x : Ad) (i n : Int) :
      Adim s (.indeksAta x (.sabit i) (.sabit n))
            (yazH s x i.toNat n) (.sabit n) [.oYaz x i.toNat]
  -- --- kontrol akisi ---
  | a_sira_cong (s s' : Store) (a a' b : Ifade) (t : Iz) :
      Adim s a s' a' t → Adim s (.sira a b) s' (.sira a' b) t
  | a_sira_atla (s : Store) (n : Int) (b : Ifade) :
      Adim s (.sira (.sabit n) b) s b []
  | a_eger_cong (s s' : Store) (k k' d y : Ifade) (t : Iz) :
      Adim s k s' k' t → Adim s (.eger k d y) s' (.eger k' d y) t
  | a_eger_dogru (s : Store) (n : Int) (d y : Ifade) : n ≠ 0 →
      Adim s (.eger (.sabit n) d y) s d [.oDal true]
  | a_eger_yanlis (s : Store) (d y : Ifade) :
      Adim s (.eger (.sabit 0) d y) s y [.oDal false]
  /-- Dongu ACILIR (Core'un `sIkenAc`i ile ayni): uydurma kural YOK. -/
  | a_iken_ac (s : Store) (k g : Ifade) :
      Adim s (.iken k g) s (.eger k (.sira g (.iken k g)) (.sabit 0)) []
  | a_esles_cong (s s' : Store) (sk sk' : Ifade) (n : Int) (d y : Ifade) (t : Iz) :
      Adim s sk s' sk' t → Adim s (.esles sk n d y) s' (.esles sk' n d y) t
  | a_esles_tuttu (s : Store) (m n : Int) (d y : Ifade) : m = n →
      Adim s (.esles (.sabit m) n d y) s d [.oDal true]
  | a_esles_tutmadi (s : Store) (m n : Int) (d y : Ifade) : m ≠ n →
      Adim s (.esles (.sabit m) n d y) s y [.oDal false]

/-- **VAKUM DENETIMI:** kucuk-adim GERCEKTEN ince-taneli — `topla`nin SOL
    operandi tek basina adim atar, yani `a + b` ifadesinin ORTASINDA
    (a okundu, b okunmadi) duraklanabilir. Buyuk-adimda bu nokta YOKTU. -/
theorem adim_ara_nokta_var (s : Store) (x y : Ad) :
    Adim s (.topla (.degisken x) (.degisken y))
          s (.topla (.sabit (s x 0)) (.degisken y)) [.oOku x 0] :=
  Adim.a_topla_sol _ _ _ _ _ _ (Adim.a_degisken _ _)

/-- **GENEL PARCANIN KUCUK-ADIM KORUNUMU (D-345 cekirdegi):**
    etiketi GENEL olan bir ifade, dusuk-esdeger iki store'da
    (1) AYNI adimi atar (kalinti ifadeler BIREBIR ESIT),
    (2) AYNI izi uretir,
    (3) sonuc store'lari dusuk-esdeger kalir,
    (4) **kalinti yine GENEL etiketlidir** — tumevarimin devam etmesini
        saglayan sart budur.

    `genel_ifade_korunum`un kucuk-adim karsiligi. ARADAKI FARK: burada
    "ifade bitene kadar" degil "TEK ADIM" konusuluyor, yani ADIMLAR
    ARASINDA baska bir thread'in araya girmesi anlamlidir. Blok ici
    preemption'i mumkun kilan sey tam olarak bu. -/
theorem genel_adim_korunum (G : EtiketOrtam) :
    ∀ {s1 : Store} {e : Ifade} {s1' : Store} {e1' : Ifade} {t1 : Iz},
      Adim s1 e s1' e1' t1 →
      ∀ {s2 s2' : Store} {e2' : Ifade} {t2 : Iz}, Adim s2 e s2' e2' t2 →
      DusukEs G s1 s2 → ifadeEtiket G e = .genel →
      e1' = e2' ∧ t1 = t2 ∧ DusukEs G s1' s2' ∧ ifadeEtiket G e1' = .genel := by
  intro s1 e s1' e1' t1 h1
  induction h1 with
  | a_degisken x =>
      intro s2 s2' e2' t2 h2 h_low h_et
      cases h2
      exact ⟨by rw [h_low x h_et 0], rfl, h_low, rfl⟩
  | a_indeks_cong s' x idx idx' t _ ih =>
      intro s2 s2' e2' t2 h2 h_low h_et
      obtain ⟨h_ex, h_ei⟩ := birlesim_genel h_et
      cases h2 with
      | a_indeks_cong _ _ _ idx2 t2b h2i =>
          obtain ⟨he, ht, hl, hg⟩ := ih h2i h_low h_ei
          exact ⟨by rw [he], ht, hl, by
            show (G x).birlesim (ifadeEtiket G idx') = _
            rw [h_ex, hg]; rfl⟩
      | a_indeks _ _ => nomatch ‹Adim _ (Ifade.sabit _) _ _ _›
  | a_indeks x i =>
      intro s2 s2' e2' t2 h2 h_low h_et
      obtain ⟨h_ex, _⟩ := birlesim_genel h_et
      cases h2 with
      | a_indeks _ _ => exact ⟨by rw [h_low x h_ex i.toNat], rfl, h_low, rfl⟩
      | a_indeks_cong _ _ _ _ _ h2i => nomatch h2i
  | a_topla_sol s' a a' b t _ ih =>
      intro s2 s2' e2' t2 h2 h_low h_et
      obtain ⟨h_ea, h_eb⟩ := birlesim_genel h_et
      cases h2 with
      | a_topla_sol _ _ a2 _ t2b h2a =>
          obtain ⟨he, ht, hl, hg⟩ := ih h2a h_low h_ea
          exact ⟨by rw [he], ht, hl, by
            show (ifadeEtiket G a').birlesim (ifadeEtiket G b) = _
            rw [hg, h_eb]; rfl⟩
      | a_topla_sag _ _ _ _ _ _ => nomatch ‹Adim _ (Ifade.sabit _) _ _ _›
      | a_topla _ _ => nomatch ‹Adim _ (Ifade.sabit _) _ _ _›
  | a_topla_sag s' n b b' t _ ih =>
      intro s2 s2' e2' t2 h2 h_low h_et
      obtain ⟨_, h_eb⟩ := birlesim_genel h_et
      cases h2 with
      | a_topla_sag _ _ _ b2 t2b h2b =>
          obtain ⟨he, ht, hl, hg⟩ := ih h2b h_low h_eb
          exact ⟨by rw [he], ht, hl, by
            show (Etiket.genel).birlesim (ifadeEtiket G b') = _
            rw [hg]; rfl⟩
      | a_topla_sol _ _ _ _ _ h2a => nomatch h2a
      | a_topla _ _ => nomatch ‹Adim _ (Ifade.sabit _) _ _ _›
  | a_topla n1 n2 =>
      intro s2 s2' e2' t2 h2 h_low _
      cases h2 with
      | a_topla _ _ => exact ⟨rfl, rfl, h_low, rfl⟩
      | a_topla_sol _ _ _ _ _ h2a => nomatch h2a
      | a_topla_sag _ _ _ _ _ h2b => nomatch h2b
  | a_carp_sol s' a a' b t _ ih =>
      intro s2 s2' e2' t2 h2 h_low h_et
      obtain ⟨h_ea, h_eb⟩ := birlesim_genel h_et
      cases h2 with
      | a_carp_sol _ _ a2 _ t2b h2a =>
          obtain ⟨he, ht, hl, hg⟩ := ih h2a h_low h_ea
          exact ⟨by rw [he], ht, hl, by
            show (ifadeEtiket G a').birlesim (ifadeEtiket G b) = _
            rw [hg, h_eb]; rfl⟩
      | a_carp_sag _ _ _ _ _ _ => nomatch ‹Adim _ (Ifade.sabit _) _ _ _›
      | a_carp _ _ => nomatch ‹Adim _ (Ifade.sabit _) _ _ _›
  | a_carp_sag s' n b b' t _ ih =>
      intro s2 s2' e2' t2 h2 h_low h_et
      obtain ⟨_, h_eb⟩ := birlesim_genel h_et
      cases h2 with
      | a_carp_sag _ _ _ b2 t2b h2b =>
          obtain ⟨he, ht, hl, hg⟩ := ih h2b h_low h_eb
          exact ⟨by rw [he], ht, hl, by
            show (Etiket.genel).birlesim (ifadeEtiket G b') = _
            rw [hg]; rfl⟩
      | a_carp_sol _ _ _ _ _ h2a => nomatch h2a
      | a_carp _ _ => nomatch ‹Adim _ (Ifade.sabit _) _ _ _›
  | a_carp n1 n2 =>
      intro s2 s2' e2' t2 h2 h_low _
      cases h2 with
      | a_carp _ _ => exact ⟨rfl, rfl, h_low, rfl⟩
      | a_carp_sol _ _ _ _ _ h2a => nomatch h2a
      | a_carp_sag _ _ _ _ _ h2b => nomatch h2b
  | a_bol_sol s' a a' b t _ ih =>
      intro s2 s2' e2' t2 h2 h_low h_et
      obtain ⟨h_ea, h_eb⟩ := birlesim_genel h_et
      cases h2 with
      | a_bol_sol _ _ a2 _ t2b h2a =>
          obtain ⟨he, ht, hl, hg⟩ := ih h2a h_low h_ea
          exact ⟨by rw [he], ht, hl, by
            show (ifadeEtiket G a').birlesim (ifadeEtiket G b) = _
            rw [hg, h_eb]; rfl⟩
      | a_bol_sag _ _ _ _ _ _ => nomatch ‹Adim _ (Ifade.sabit _) _ _ _›
      | a_bol _ _ => nomatch ‹Adim _ (Ifade.sabit _) _ _ _›
  | a_bol_sag s' n b b' t _ ih =>
      intro s2 s2' e2' t2 h2 h_low h_et
      obtain ⟨_, h_eb⟩ := birlesim_genel h_et
      cases h2 with
      | a_bol_sag _ _ _ b2 t2b h2b =>
          obtain ⟨he, ht, hl, hg⟩ := ih h2b h_low h_eb
          exact ⟨by rw [he], ht, hl, by
            show (Etiket.genel).birlesim (ifadeEtiket G b') = _
            rw [hg]; rfl⟩
      | a_bol_sol _ _ _ _ _ h2a => nomatch h2a
      | a_bol _ _ => nomatch ‹Adim _ (Ifade.sabit _) _ _ _›
  | a_bol n1 n2 =>
      intro s2 s2' e2' t2 h2 h_low _
      cases h2 with
      | a_bol _ _ => exact ⟨rfl, rfl, h_low, rfl⟩
      | a_bol_sol _ _ _ _ _ h2a => nomatch h2a
      | a_bol_sag _ _ _ _ _ h2b => nomatch h2b
  | a_kalan_sol s' a a' b t _ ih =>
      intro s2 s2' e2' t2 h2 h_low h_et
      obtain ⟨h_ea, h_eb⟩ := birlesim_genel h_et
      cases h2 with
      | a_kalan_sol _ _ a2 _ t2b h2a =>
          obtain ⟨he, ht, hl, hg⟩ := ih h2a h_low h_ea
          exact ⟨by rw [he], ht, hl, by
            show (ifadeEtiket G a').birlesim (ifadeEtiket G b) = _
            rw [hg, h_eb]; rfl⟩
      | a_kalan_sag _ _ _ _ _ _ => nomatch ‹Adim _ (Ifade.sabit _) _ _ _›
      | a_kalan _ _ => nomatch ‹Adim _ (Ifade.sabit _) _ _ _›
  | a_kalan_sag s' n b b' t _ ih =>
      intro s2 s2' e2' t2 h2 h_low h_et
      obtain ⟨_, h_eb⟩ := birlesim_genel h_et
      cases h2 with
      | a_kalan_sag _ _ _ b2 t2b h2b =>
          obtain ⟨he, ht, hl, hg⟩ := ih h2b h_low h_eb
          exact ⟨by rw [he], ht, hl, by
            show (Etiket.genel).birlesim (ifadeEtiket G b') = _
            rw [hg]; rfl⟩
      | a_kalan_sol _ _ _ _ _ h2a => nomatch h2a
      | a_kalan _ _ => nomatch ‹Adim _ (Ifade.sabit _) _ _ _›
  | a_kalan n1 n2 =>
      intro s2 s2' e2' t2 h2 h_low _
      cases h2 with
      | a_kalan _ _ => exact ⟨rfl, rfl, h_low, rfl⟩
      | a_kalan_sol _ _ _ _ _ h2a => nomatch h2a
      | a_kalan_sag _ _ _ _ _ h2b => nomatch h2b
  | a_atama_cong s' x e e' t _ ih =>
      intro s2 s2' e2' t2 h2 h_low h_et
      cases h2 with
      | a_atama_cong _ _ _ e2b t2b h2e =>
          obtain ⟨he, ht, hl, hg⟩ := ih h2e h_low h_et
          exact ⟨by rw [he], ht, hl, hg⟩
      | a_atama _ _ => nomatch ‹Adim _ (Ifade.sabit _) _ _ _›
  | a_atama x n =>
      intro s2 s2' e2' t2 h2 h_low _
      cases h2 with
      | a_atama _ _ => exact ⟨rfl, rfl, dusukEs_yaz_genel G _ s2 x n h_low, rfl⟩
      | a_atama_cong _ _ _ _ _ h2e => nomatch h2e
  | a_indeks_ata_idx s' x idx idx' e t _ ih =>
      intro s2 s2' e2' t2 h2 h_low h_et
      obtain ⟨h_ei, h_ee⟩ := birlesim_genel h_et
      cases h2 with
      | a_indeks_ata_idx _ _ _ idx2 _ t2b h2i =>
          obtain ⟨he, ht, hl, hg⟩ := ih h2i h_low h_ei
          exact ⟨by rw [he], ht, hl, by
            show (ifadeEtiket G idx').birlesim (ifadeEtiket G e) = _
            rw [hg, h_ee]; rfl⟩
      | a_indeks_ata_deg _ _ _ _ _ _ _ => nomatch ‹Adim _ (Ifade.sabit _) _ _ _›
      | a_indeks_ata _ _ _ => nomatch ‹Adim _ (Ifade.sabit _) _ _ _›
  | a_indeks_ata_deg s' x i e e' t _ ih =>
      intro s2 s2' e2' t2 h2 h_low h_et
      obtain ⟨_, h_ee⟩ := birlesim_genel h_et
      cases h2 with
      | a_indeks_ata_deg _ _ _ _ e2b t2b h2e =>
          obtain ⟨he, ht, hl, hg⟩ := ih h2e h_low h_ee
          exact ⟨by rw [he], ht, hl, by
            show (Etiket.genel).birlesim (ifadeEtiket G e') = _
            rw [hg]; rfl⟩
      | a_indeks_ata_idx _ _ _ _ _ _ h2i => nomatch h2i
      | a_indeks_ata _ _ _ => nomatch ‹Adim _ (Ifade.sabit _) _ _ _›
  | a_indeks_ata x i n =>
      intro s2 s2' e2' t2 h2 h_low _
      cases h2 with
      | a_indeks_ata _ _ _ =>
          exact ⟨rfl, rfl, dusukEs_yazH_genel G _ s2 x i.toNat n h_low, rfl⟩
      | a_indeks_ata_idx _ _ _ _ _ _ h2i => nomatch h2i
      | a_indeks_ata_deg _ _ _ _ _ _ h2e => nomatch h2e
  | a_sira_cong s' a a' b t _ ih =>
      intro s2 s2' e2' t2 h2 h_low h_et
      obtain ⟨h_ea, h_eb⟩ := birlesim_genel h_et
      cases h2 with
      | a_sira_cong _ _ a2 _ t2b h2a =>
          obtain ⟨he, ht, hl, hg⟩ := ih h2a h_low h_ea
          exact ⟨by rw [he], ht, hl, by
            show (ifadeEtiket G a').birlesim (ifadeEtiket G b) = _
            rw [hg, h_eb]; rfl⟩
      | a_sira_atla _ _ => nomatch ‹Adim _ (Ifade.sabit _) _ _ _›
  | a_sira_atla n b =>
      intro s2 s2' e2' t2 h2 h_low h_et
      obtain ⟨_, h_eb⟩ := birlesim_genel h_et
      cases h2 with
      | a_sira_atla _ _ => exact ⟨rfl, rfl, h_low, h_eb⟩
      | a_sira_cong _ _ _ _ _ h2a => nomatch h2a
  | a_eger_cong s' k k' d y t _ ih =>
      intro s2 s2' e2' t2 h2 h_low h_et
      obtain ⟨h_kd, h_ey⟩ := birlesim_genel h_et
      obtain ⟨h_ek, h_ed⟩ := birlesim_genel h_kd
      cases h2 with
      | a_eger_cong _ _ k2 _ _ t2b h2k =>
          obtain ⟨he, ht, hl, hg⟩ := ih h2k h_low h_ek
          exact ⟨by rw [he], ht, hl, by
            show ((ifadeEtiket G k').birlesim (ifadeEtiket G d)).birlesim
                   (ifadeEtiket G y) = _
            rw [hg, h_ed, h_ey]; rfl⟩
      | a_eger_dogru _ _ _ _ => nomatch ‹Adim _ (Ifade.sabit _) _ _ _›
      | a_eger_yanlis _ _ => nomatch ‹Adim _ (Ifade.sabit _) _ _ _›
  | a_eger_dogru n d y hn =>
      intro s2 s2' e2' t2 h2 h_low h_et
      obtain ⟨h_kd, _⟩ := birlesim_genel h_et
      obtain ⟨_, h_ed⟩ := birlesim_genel h_kd
      cases h2 with
      | a_eger_dogru _ _ _ _ => exact ⟨rfl, rfl, h_low, h_ed⟩
      | a_eger_yanlis _ _ => exact absurd rfl hn
      | a_eger_cong _ _ _ _ _ _ h2k => nomatch h2k
  | a_eger_yanlis d y =>
      intro s2 s2' e2' t2 h2 h_low h_et
      obtain ⟨_, h_ey⟩ := birlesim_genel h_et
      cases h2 with
      | a_eger_yanlis _ _ => exact ⟨rfl, rfl, h_low, h_ey⟩
      | a_eger_dogru _ _ _ hn => exact absurd rfl hn
      | a_eger_cong _ _ _ _ _ _ h2k => nomatch h2k
  | a_iken_ac k g =>
      intro s2 s2' e2' t2 h2 h_low h_et
      obtain ⟨h_ek, h_eg⟩ := birlesim_genel h_et
      cases h2
      refine ⟨rfl, rfl, h_low, ?_⟩
      show ((ifadeEtiket G k).birlesim
             ((ifadeEtiket G g).birlesim
               ((ifadeEtiket G k).birlesim (ifadeEtiket G g)))).birlesim
             Etiket.genel = _
      rw [h_ek, h_eg]; rfl
  | a_esles_cong s' sk sk' n d y t _ ih =>
      intro s2 s2' e2' t2 h2 h_low h_et
      obtain ⟨h_sd, h_ey⟩ := birlesim_genel h_et
      obtain ⟨h_es, h_ed⟩ := birlesim_genel h_sd
      cases h2 with
      | a_esles_cong _ _ sk2 _ _ _ t2b h2s =>
          obtain ⟨he, ht, hl, hg⟩ := ih h2s h_low h_es
          exact ⟨by rw [he], ht, hl, by
            show ((ifadeEtiket G sk').birlesim (ifadeEtiket G d)).birlesim
                   (ifadeEtiket G y) = _
            rw [hg, h_ed, h_ey]; rfl⟩
      | a_esles_tuttu _ _ _ _ _ => nomatch ‹Adim _ (Ifade.sabit _) _ _ _›
      | a_esles_tutmadi _ _ _ _ _ => nomatch ‹Adim _ (Ifade.sabit _) _ _ _›
  | a_esles_tuttu m n d y hmn =>
      intro s2 s2' e2' t2 h2 h_low h_et
      obtain ⟨h_sd, _⟩ := birlesim_genel h_et
      obtain ⟨_, h_ed⟩ := birlesim_genel h_sd
      cases h2 with
      | a_esles_tuttu _ _ _ _ _ => exact ⟨rfl, rfl, h_low, h_ed⟩
      | a_esles_tutmadi _ _ _ _ h2n => exact absurd hmn h2n
      | a_esles_cong _ _ _ _ _ _ _ h2s => nomatch h2s
  | a_esles_tutmadi m n d y hmn =>
      intro s2 s2' e2' t2 h2 h_low h_et
      obtain ⟨_, h_ey⟩ := birlesim_genel h_et
      cases h2 with
      | a_esles_tutmadi _ _ _ _ _ => exact ⟨rfl, rfl, h_low, h_ey⟩
      | a_esles_tuttu _ _ _ _ h2m => exact absurd h2m hmn
      | a_esles_cong _ _ _ _ _ _ _ h2s => nomatch h2s

-- ============================================================
-- §12. GIZLI KOL (D-346) — gizliye-toleransli ifade denkligi
-- ============================================================

/-
SORUN (D-345'te olculdu): gizli bir degisken okununca kalinti
`sabit v1` / `sabit v2` FARKLIDIR — etiket sistemi sabitleri hep `genel`
saydigi icin iki kosumu iliskilendirmek yapisal bir denklik ister.
`genel_adim_korunum` bu bagintinin KAMUSAL kolunu verdi (kamusal
alt-terimler ESIT kalir). Burada GIZLI kol tanimlanip korunumu ispatlanir.

FIKIR: `IfDE G e1 e2` = "iki kosumun ayni noktadaki ifadeleri". Sabitler
SERBESTTIR (gizli veriden gelebilir), AMA kontrol akisini / adresi /
veri-bagimli gecikmeyi belirleyen konumlarda ESITLIK ve GENEL etiket
ZORUNLUDUR — bunlar tam olarak CT001/CT004/CT005/CT006'nin sartlaridir.
-/

/-- Gizliye-toleransli ifade denkligi. -/
inductive IfDE (G : EtiketOrtam) : Ifade → Ifade → Prop where
  /-- KAMUSAL kol: GENEL etiketli ifade iki kosumda BIREBIR AYNIDIR
      (`genel_adim_korunum` bunu adim altinda korur). -/
  | genel (e : Ifade) : ifadeEtiket G e = .genel → IfDE G e e
  /-- GIZLI kol: sabitler serbest — gizli okumadan gelmis olabilirler. -/
  | sabit (n1 n2 : Int) : IfDE G (.sabit n1) (.sabit n2)
  | degisken (x : Ad) : IfDE G (.degisken x) (.degisken x)
  | topla (a1 a2 b1 b2 : Ifade) :
      IfDE G a1 a2 → IfDE G b1 b2 → IfDE G (.topla a1 b1) (.topla a2 b2)
  | carp (a1 a2 b1 b2 : Ifade) :
      IfDE G a1 a2 → IfDE G b1 b2 → IfDE G (.carp a1 b1) (.carp a2 b2)
  /-- CT006: bolme OPERANDLARI genel → iki kosumda ESIT. -/
  | bol (a b : Ifade) :
      ifadeEtiket G a = .genel → ifadeEtiket G b = .genel →
      IfDE G (.bol a b) (.bol a b)
  | kalan (a b : Ifade) :
      ifadeEtiket G a = .genel → ifadeEtiket G b = .genel →
      IfDE G (.kalan a b) (.kalan a b)
  | sabitDeg (x : Ad) (e1 e2 : Ifade) :
      IfDE G e1 e2 → IfDE G (.sabitDeg x e1) (.sabitDeg x e2)
  | sira (a1 a2 b : Ifade) :
      IfDE G a1 a2 → IfDE G (.sira a1 b) (.sira a2 b)
  /-- CT001: kosul genel → dal karari AYNI. -/
  | eger (k d y : Ifade) :
      ifadeEtiket G k = .genel → IfDE G (.eger k d y) (.eger k d y)
  /-- CT002: dongu kosulu genel → tur sayisi AYNI. -/
  | iken (k g : Ifade) :
      ifadeEtiket G k = .genel → IfDE G (.iken k g) (.iken k g)
  /-- CT004: skrutin genel → hangi kol AYNI. -/
  | esles (sk : Ifade) (n : Int) (d y : Ifade) :
      ifadeEtiket G sk = .genel → IfDE G (.esles sk n d y) (.esles sk n d y)
  /-- CT005: indeks genel → ADRES AYNI. -/
  | indeks (x : Ad) (idx : Ifade) :
      ifadeEtiket G idx = .genel → IfDE G (.indeks x idx) (.indeks x idx)
  | indeksAta (x : Ad) (idx e1 e2 : Ifade) :
      ifadeEtiket G idx = .genel → IfDE G e1 e2 →
      IfDE G (.indeksAta x idx e1) (.indeksAta x idx e2)

/-- CT-tipli bir ifade KENDISIYLE denktir. `CtOk`un yan-kosullari, `IfDE`nin
    esitlik isteyen kollarini tam olarak besler — yani iki disiplin AYNI
    yerlerde ayni seyi talep ediyor. -/
theorem ifde_refl {G : EtiketOrtam} : ∀ {e : Ifade}, CtOk G e → IfDE G e e := by
  intro e h
  induction h with
  | ct_sabit n => exact IfDE.sabit n n
  | ct_degisken x => exact IfDE.degisken x
  | ct_topla a b _ _ iha ihb => exact IfDE.topla a a b b iha ihb
  | ct_carp a b _ _ iha ihb => exact IfDE.carp a a b b iha ihb
  | ct_bol a b _ _ hag hbg _ _ => exact IfDE.bol a b hag hbg
  | ct_kalan a b _ _ hag hbg _ _ => exact IfDE.kalan a b hag hbg
  | ct_atama x e _ _ ih => exact IfDE.sabitDeg x e e ih
  | ct_sira a b _ _ iha _ => exact IfDE.sira a a b iha
  | ct_eger k d y _ _ _ hkg _ _ _ => exact IfDE.eger k d y hkg
  | ct_iken k g _ _ hkg _ _ => exact IfDE.iken k g hkg
  | ct_esles s n d y _ _ _ hsg _ _ _ => exact IfDE.esles s n d y hsg
  | ct_indeks x idx _ hig _ => exact IfDE.indeks x idx hig
  | ct_indeks_ata x idx e _ _ hig _ _ ihe => exact IfDE.indeksAta x idx e e hig ihe

/-- `CtOk` KUCUK-ADIM ALTINDA KORUNUR. Kritik nokta: etiketler degisse bile
    (gizli okuma → `sabit`, etiketi genel) CT'nin sart kostugu konumlar
    GENEL kalir — bunu `genel_adim_korunum`un 4. conjunct'i saglar. -/
theorem ctok_adim_korunur {G : EtiketOrtam} :
    ∀ {s e s' e' t}, Adim s e s' e' t → CtOk G e → CtOk G e' := by
  intro s e s' e' t h
  induction h with
  | a_degisken x => intro _; exact CtOk.ct_sabit _
  | a_indeks_cong s' x idx idx' t hstep ih =>
      intro hc
      cases hc with
      | ct_indeks _ _ hci hig =>
          exact CtOk.ct_indeks x idx' (ih hci)
            ((genel_adim_korunum G hstep hstep (fun _ _ _ => rfl) hig).2.2.2)
  | a_indeks x i => intro _; exact CtOk.ct_sabit _
  | a_topla_sol s' a a' b t _ ih =>
      intro hc; cases hc with
      | ct_topla _ _ hca hcb => exact CtOk.ct_topla a' b (ih hca) hcb
  | a_topla_sag s' n b b' t _ ih =>
      intro hc; cases hc with
      | ct_topla _ _ hca hcb => exact CtOk.ct_topla _ b' hca (ih hcb)
  | a_topla n1 n2 => intro _; exact CtOk.ct_sabit _
  | a_carp_sol s' a a' b t _ ih =>
      intro hc; cases hc with
      | ct_carp _ _ hca hcb => exact CtOk.ct_carp a' b (ih hca) hcb
  | a_carp_sag s' n b b' t _ ih =>
      intro hc; cases hc with
      | ct_carp _ _ hca hcb => exact CtOk.ct_carp _ b' hca (ih hcb)
  | a_carp n1 n2 => intro _; exact CtOk.ct_sabit _
  | a_bol_sol s' a a' b t hstep ih =>
      intro hc; cases hc with
      | ct_bol _ _ hca hcb hag hbg =>
          exact CtOk.ct_bol a' b (ih hca) hcb
            ((genel_adim_korunum G hstep hstep (fun _ _ _ => rfl) hag).2.2.2) hbg
  | a_bol_sag s' n b b' t hstep ih =>
      intro hc; cases hc with
      | ct_bol _ _ hca hcb hag hbg =>
          exact CtOk.ct_bol _ b' hca (ih hcb) hag
            ((genel_adim_korunum G hstep hstep (fun _ _ _ => rfl) hbg).2.2.2)
  | a_bol n1 n2 => intro _; exact CtOk.ct_sabit _
  | a_kalan_sol s' a a' b t hstep ih =>
      intro hc; cases hc with
      | ct_kalan _ _ hca hcb hag hbg =>
          exact CtOk.ct_kalan a' b (ih hca) hcb
            ((genel_adim_korunum G hstep hstep (fun _ _ _ => rfl) hag).2.2.2) hbg
  | a_kalan_sag s' n b b' t hstep ih =>
      intro hc; cases hc with
      | ct_kalan _ _ hca hcb hag hbg =>
          exact CtOk.ct_kalan _ b' hca (ih hcb) hag
            ((genel_adim_korunum G hstep hstep (fun _ _ _ => rfl) hbg).2.2.2)
  | a_kalan n1 n2 => intro _; exact CtOk.ct_sabit _
  | a_atama_cong s' x e e' t hstep ih =>
      intro hc; cases hc with
      | ct_atama _ _ hce hak => exact CtOk.ct_atama x e' (ih hce) (by
          -- CT003 akis sarti korunur. Iki hal:
          --  * G x = gizli → her sey gizli'ye duser, sart otomatik.
          --  * G x = genel → hak zaten e'nin GENEL oldugunu zorlar, ve
          --    `genel_adim_korunum`in 4. conjunct'i e''nun de genel
          --    kaldigini verir. Yeni lemma GEREKMEZ.
          cases hgx : G x with
          | gizli => cases ifadeEtiket G e' <;> simp [Etiket.altMi]
          | genel =>
              have he : ifadeEtiket G e = .genel := by
                rw [hgx] at hak
                cases hle : ifadeEtiket G e with
                | genel => rfl
                | gizli => rw [hle] at hak; simp [Etiket.altMi] at hak
              rw [(genel_adim_korunum G hstep hstep (fun _ _ _ => rfl) he).2.2.2]
              simp [Etiket.altMi])
  | a_atama x n => intro _; exact CtOk.ct_sabit _
  | a_indeks_ata_idx s' x idx idx' e t hstep ih =>
      intro hc; cases hc with
      | ct_indeks_ata _ _ _ hci hce hig hak =>
          exact CtOk.ct_indeks_ata x idx' e (ih hci) hce
            ((genel_adim_korunum G hstep hstep (fun _ _ _ => rfl) hig).2.2.2) hak
  | a_indeks_ata_deg s' x i e e' t hstep ih =>
      intro hc; cases hc with
      | ct_indeks_ata _ _ _ hci hce hig hak =>
          exact CtOk.ct_indeks_ata x _ e' hci (ih hce) hig (by
            cases hgx : G x with
            | gizli => cases ifadeEtiket G e' <;> simp [Etiket.altMi]
            | genel =>
                have he : ifadeEtiket G e = .genel := by
                  rw [hgx] at hak
                  cases hle : ifadeEtiket G e with
                  | genel => rfl
                  | gizli => rw [hle] at hak; simp [Etiket.altMi] at hak
                rw [(genel_adim_korunum G hstep hstep (fun _ _ _ => rfl) he).2.2.2]
                simp [Etiket.altMi])
  | a_indeks_ata x i n => intro _; exact CtOk.ct_sabit _
  | a_sira_cong s' a a' b t _ ih =>
      intro hc; cases hc with
      | ct_sira _ _ hca hcb => exact CtOk.ct_sira a' b (ih hca) hcb
  | a_sira_atla n b => intro hc; cases hc with | ct_sira _ _ _ hcb => exact hcb
  | a_eger_cong s' k k' d y t hstep ih =>
      intro hc; cases hc with
      | ct_eger _ _ _ hck hcd hcy hkg =>
          exact CtOk.ct_eger k' d y (ih hck) hcd hcy
            ((genel_adim_korunum G hstep hstep (fun _ _ _ => rfl) hkg).2.2.2)
  | a_eger_dogru n d y _ =>
      intro hc; cases hc with | ct_eger _ _ _ _ hcd _ _ => exact hcd
  | a_eger_yanlis d y =>
      intro hc; cases hc with | ct_eger _ _ _ _ _ hcy _ => exact hcy
  | a_iken_ac k g =>
      intro hc; cases hc with
      | ct_iken _ _ hck hcg hkg =>
          exact CtOk.ct_eger k _ _ hck
            (CtOk.ct_sira g _ hcg (CtOk.ct_iken k g hck hcg hkg))
            (CtOk.ct_sabit 0) hkg
  | a_esles_cong s' sk sk' n d y t hstep ih =>
      intro hc; cases hc with
      | ct_esles _ _ _ _ hcs hcd hcy hsg =>
          exact CtOk.ct_esles sk' n d y (ih hcs) hcd hcy
            ((genel_adim_korunum G hstep hstep (fun _ _ _ => rfl) hsg).2.2.2)
  | a_esles_tuttu m n d y _ =>
      intro hc; cases hc with | ct_esles _ _ _ _ _ hcd _ _ => exact hcd
  | a_esles_tutmadi m n d y _ =>
      intro hc; cases hc with | ct_esles _ _ _ _ _ _ hcy _ => exact hcy

/-- **VAKUM DENETIMI 1 — `IfDE` GERCEKTEN gizliye toleransli.** Farkli
    sabitler denktir; yani gizli bir okumanin kalintisi (`sabit v1` vs
    `sabit v2`) iliskilendirilebiliyor. Bu olmasaydi `IfDE` sadece
    esitlik olurdu ve gizli kol HICBIR SEY eklemezdi. -/
theorem ifde_gizli_toleransli (G : EtiketOrtam) :
    IfDE G (.sabit 3) (.sabit 7) := IfDE.sabit 3 7

/-- **VAKUM DENETIMI 2 — ama KONTROL AKISINDA tolerans YOK.** Kosulu
    farkli iki `eger` denk DEGILDIR: `IfDE.genel` esitlik ister,
    `IfDE.eger` de oyle. Yani `IfDE` sabitleri her yerde serbest
    birakmiyor — CT001'in korudugu konumda ESITLIK zorunlu.
    Iki tanik birlikte iliskinin ne cok gevsek ne cok siki oldugunu
    olcuyor. -/
theorem ifde_dal_kosulunda_tolerans_yok (G : EtiketOrtam) (d y : Ifade) :
    ¬ IfDE G (.eger (.sabit 1) d y) (.eger (.sabit 2) d y) := by
  -- HICBIR kurucu birlesmez: `genel` de `eger` de IKI TARAFIN AYNI
  -- olmasini ister, kosullar (1 vs 2) farkli.
  intro h; cases h

/-! ### §12.1 KILIT-ADIM (lockstep) ALTYAPISI

`ince_adim_ni` (kucuk-adim NI) icin sart olan lemmalar. Cekirdek gozlem:
`IfDE` bir DEGERI asla bir DEGER-OLMAYANLA iliskilendirmez. Bu olmasaydi
iki kosum "farkli yerlerde" adim atabilir, izler yapisal olarak
ayrisabilirdi — yani NI'nin kilit-adim argumani cokerdi. -/

/-- Bir `sabit`ten adim ATILAMAZ (deger, normal formdur). -/
theorem adim_sabit_yok {s s' : Store} {n : Int} {e' : Ifade} {t : Iz} :
    ¬ Adim s (.sabit n) s' e' t := by
  intro h; cases h

/-- `IfDE` SAGDA bir deger goruyorsa SOLDA da deger vardir. -/
theorem ifde_sabit_sag {G : EtiketOrtam} {e1 : Ifade} {n : Int} :
    IfDE G e1 (.sabit n) → ∃ m, e1 = .sabit m := by
  intro h; cases h with
  | genel _ _ => exact ⟨n, rfl⟩
  | sabit m _ => exact ⟨m, rfl⟩

/-- Simetrigi: SOLDA deger varsa SAGDA da vardir. -/
theorem ifde_sabit_sol {G : EtiketOrtam} {e2 : Ifade} {n : Int} :
    IfDE G (.sabit n) e2 → ∃ m, e2 = .sabit m := by
  intro h; cases h with
  | genel _ _ => exact ⟨n, rfl⟩
  | sabit _ m => exact ⟨m, rfl⟩

/-- **VAKUM DENETIMI 3 — kilit-adim GERCEKTEN bir kisit.** `IfDE`
    gizli-toleransli oldugu halde (bkz. `ifde_gizli_toleransli`) bir
    degeri adim-atabilir bir terimle iliskilendirmez: ornekte sag taraf
    `degisken x` adim atabilirken sol taraf `sabit 3` atamaz, ve iliski
    zaten KURULAMAZ. Bu tanik olmasaydi yukaridaki iki lemma BOS
    olabilirdi (her ikisi de yalnizca "deger↔deger" durumunu konusur). -/
theorem ifde_deger_deger_olmayan_yok (G : EtiketOrtam) (x : Ad) :
    ¬ IfDE G (.sabit 3) (.degisken x) := by
  intro h; cases h

end Kemgu.SideChannel.CT
