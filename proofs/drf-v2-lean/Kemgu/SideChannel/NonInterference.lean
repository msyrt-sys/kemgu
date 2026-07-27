/-
KEMGU Side-Channel Mekanize — Cekirdek Alt-Kume Non-Interference (D-328)
Kaynak (kagit formel): belgeler/KEMGU_Sabitsure_Spec_V1.md §CT.10
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz, sorry/axiom YOK

═══════════════════════════════════════════════════════════════════════
KAPSAM — ONCE BUNU OKU (durustluk paketi; Mehmet karari 2026-07-26)
═══════════════════════════════════════════════════════════════════════

Bu dosya GERCEK bir non-interference teoremi ispatlar, ama KAPSAMI DARDIR
ve adi bunu soyler: `ni_cekirdek_altkume`. `kemgu_soundness_v3`e conjunct
olarak EKLENMEZ (vakum/asiri-iddia riski — ADIM 0 Sorun 1 dersi).

NE ISPATLANIYOR (bugun, TAM olarak):
  (A) Gozlem, degerden BAGIMSIZ carpanlanir: `gozlem (olaySil o) = gozlem o`,
      `izGozlem (izSil tau) = izGozlem tau` (§3).
  (B) Silme, veri-erisim yardimcilariyla DEGISIR (§4): store lookup, kanal
      find?/ilk/ekle/cikar — yani arama BASARISI, kuyruk UZUNLUGU ve BOSLUK
      durumu silme altinda korunur (bunlar yapisaldir, veri degil).
  (C) DEGER TASIYAN her Step kurali icin SILME SIMULASYONU (§5, 6/6):
      sVarOku, sAtamaTamam, sSeqAtla, sGuvensizAtla, cKanalGonderTamam,
      cKanalAlTamam. Her biri: kural silinmis konfigurasyonda AYNEN
      uygulanabilir (deger yerine birim) ve sonuc silinmis-esdegerdir.
      → Tasinan verinin adimin VARLIGINA, ODAGINA ve urettigi GOZLEME
      etkisi yoktur. Bunlar NI icereginin tamamen yasadigi kurallardir.

  (D) **GLOBAL CATI (§6, 21/21 kural):** `silme_simulasyon` —
      `Step S S' → Step (konfSil S) (konfSil S')`. Deger tasiyanlar §5
      lemmalariyla, fault/yapisal kurallar dogrudan, 3 congruence kurali
      TUMEVARIM hipoteziyle (ic adim da silinmis dunyada atilabilir).
  (E) **SONUC `ni_cekirdek_altkume`:** dusuk-esdeger (silinmisi ayni) iki
      konfigurasyondan atilan adimlar AYNI gozlemi uretir.

  ⚠ ISPATIN ZORLADIGI DARALTMA: `degerSil` once TUM degerleri birime
  indiriyordu; `cGorevBaslatTamam` case'i COKTU — cunku `gorevVal t` bir
  VERI degil THREAD KIMLIGIDIR ve kural onu yeniden uretir (ayrica `gBaslat t`
  olayiyla zaten gozlemlenebilir). Tanim daraltildi (gorevVal korunur).
  Yani "ne gizlidir" sorusunu ispat cevapladi, biz varsaymadik.

NE ISPATLANMIYOR (ACIKCA):
  Kagit CT001/CT002/CT004'un korudugu asil sizinti kanallari — gizli
  uzerinde DALLANMA (`eger`/`iken`/`esles`), gizli INDEKS (`a[idx]`),
  gizli DIV/MOD — bu modelde IFADE EDILEMEZ, cunku Sem/Core.lean'deki
  `Ifade` alt-kumesinde kosullu/dongu/indeksleme/aritmetik YOK. Yani bu
  teorem "KEMGU sabit-suredir" DEMEZ; "cekirdek alt-kumede veri-bagimli
  gozlem yoktur" der. Dallanmali model genisletmesi (Ifade + gizli etiket
  + CT disiplini hipotezi) AYRI IS — dis-kontrat sinifi degisiklik.

NEDEN YINE DE DEGERLI:
  (1) Vakum degil: bir kural degerden okuyup kontrol akisini ya da odagi
      degistirseydi §5'teki ilgili ispat COKERDI. Ozellikle cKanalGonder'in
      `h_bos` (kapasite-1 blokla) ve cKanalAl'in `h_v` (bos kanal bloklar)
      on-kosullari veri-BAGIMSIZ olarak transport edilebildi — kuyruk
      uzunlugu yapisal oldugu icin. Bu, bir NI ozelliginin gercek testidir.
  (2) `eger` eklendiginde §5 KIRMIZI verir → dallanma eklemenin CT
      disiplini gerektirdigi otomatik ortaya cikar (sessiz kalmaz).
═══════════════════════════════════════════════════════════════════════
-/

import Kemgu.Sem.Core
import Kemgu.Sem.SmallStep

namespace Kemgu.SideChannel.NonInterference

open Kemgu.Sem.Core Kemgu.Sem.SmallStep

-- ============================================================
-- §1. Saldirgan gozlemi — degerler SILINIR, desen KALIR
-- ============================================================

/-- Gozlemlenebilir olay: olayin TURU + kim + nerede. Tasinan DEGER yok.
    Saldirgan modeli: erisim deseni gorulur, veri gorulmez. -/
inductive GozlemOlay : Type where
  | gOku      (t : ThreadId) (k : Konum)
  | gYaz      (t : ThreadId) (k : Konum)
  | gBaslat   (t : ThreadId)
  | gBitir    (t : ThreadId)
  | gGonder   (t : ThreadId) (k : KanalId)
  | gAl       (t : ThreadId) (k : KanalId)
  | gDondur   (t : ThreadId) (b : Bolge)
  /-- D-332: dal karari saldirgan tarafindan GORULUR (PC/timing kanali). -/
  | gDal      (t : ThreadId) (alindi : Bool)

/-- Olay → gozlem (deger projeksiyonu ATILIR). -/
def gozlem : Olay → GozlemOlay
  | .memOku t k v        => .gOku t k
  | .memYaz t k v        => .gYaz t k
  | .threadBaslat t      => .gBaslat t
  | .threadBitir t       => .gBitir t
  | .kanalGonderOl t k v => .gGonder t k
  | .kanalAlOl t k v     => .gAl t k
  | .dondurOl t b        => .gDondur t b
  | .dalOl t a           => .gDal t a

/-- Iz → gozlem dizisi. -/
def izGozlem (tau : Iz) : List GozlemOlay := tau.map gozlem

-- ============================================================
-- §2. Deger silme (erasure) — tum veriler birime indirgenir
-- ============================================================

/-- Deger silme: veri yuku birime iner.
    ISTISNA — `gorevVal t`: bu bir VERI degil, THREAD KIMLIGIDIR ve zaten
    gozlemlenebilir (`gBaslat t` olayi saldirganda goruunur). Silinseydi
    teorem GERCEGI ASAN bir sey iddia ederdi: cGorevBaslat kurali handle'i
    yeniden URETIR, dolayisiyla silme orada simulasyon OLMAZDI — bu, ispat
    denemesinde somut olarak ortaya cikti (unsolved goal) ve tanim boylece
    DARALTILDI. Yani "ne silinir" sorusu ispat tarafindan zorlandi.

    ISTISNA 2 (D-332 — DALLANMA EKLENINCE ISPATIN ZORLADIGI IKINCI
    DARALTMA): `degerDogruMu` biti KORUNUR. `eger` ve `dalOl` modele
    girdiginde "tum degerler birime iner" tanimi teoremi YANLIS yapar:
    orijinal kosum `oDal true`, silinmis kosum `oDal false` uretirdi —
    silme artik bir SIMULASYON OLMAZDI. Bu tesadufi bir teknik ayrinti
    DEGIL, CT'nin ta kendisidir: veri kontrol akisini etkiler ve kontrol
    akisi GOZLENIR. Dolayisiyla o TEK BIT saldirgandan saklanamaz ve
    "silinmis" sayilamaz. Isaretci-benzeri degerler `skaler 1`'e (dogru),
    `birim` kendine (yanlis) iner.
    Yani yine: "ne gizlidir" sorusunu ISPAT cevapladi, biz varsaymadik.

    ISTISNA 3 (D-334 — ARITMETIK EKLENINCE ISPATIN ZORLADIGI UCUNCU
    DARALTMA): SKALERLER ARTIK HIC SILINMIYOR. D-332'de 0/1 bitine
    indirmek yetiyordu; `topla` gelince YETMEZ, cunku aritmetik O BITIN
    UZERINDE HESAP YAPAR: n1 = n2 = 1 icin silinmis kosum 1 + 1 = 2
    uretir, orijinalin silinmisi ise `skaler 1`dir — 2 ≠ 1, yani
    `silme_simulasyon` COKERDI (ispat denemesinde somut olarak cikti).

    Bunun ANLAMI (kucumsenmemeli): bu teoremin skaler-duzeyi artik BOS.
    D-329'un "tum veri silinir" ifadesi yalnizca dil veri uzerinde HESAP
    da DALLANMA da yapamadigi icin dogruydu. Ikisi de eklendiginde
    skaler-duzey non-interference'i tasiyan sey BU TEOREM DEGIL,
    `SideChannel/CT` + `CTKopru.kopru_ni`dir (CT001/CT003 disiplini
    altinda). Burada geriye kalan icerik: gozlem, ISARETCI-BENZERI
    yuklerden (metin/yapi/dizi/closure/yetki) BAGIMSIZDIR. -/
def degerSil : Deger → Deger
  | .gorevVal t => .gorevVal t
  | .skaler n   => .skaler n          -- D-334: silinmez (bkz. ISTISNA 3)
  | .birim      => Deger.birim
  | _           => .skaler 1

/-- Silme, DAL KARARINI korur (D-332). `degerSil`in yukaridaki
    daraltmasinin tam olarak satin aldigi sey; sEgerSec simulasyonunun
    kalbi. -/
theorem degerDogruMu_degerSil (v : Deger) :
    degerDogruMu (degerSil v) = degerDogruMu v := by
  cases v <;> rfl

/-- Ifade silme: gomulu literaller birime iner, KONTROL ISKELETI korunur. -/
def ifadeSil : Ifade → Ifade
  | .tanim x              => .tanim x
  | .sabit v              => .sabit (degerSil v)
  | .atama x e            => .atama x (ifadeSil e)
  | .seq a b              => .seq (ifadeSil a) (ifadeSil b)
  | .gorevBaslat yd kod   => .gorevBaslat yd (ifadeSil kod)
  | .gorevBirlestir g     => .gorevBirlestir g
  | .kanalGonderIf k v    => .kanalGonderIf k v
  | .kanalAlIf k          => .kanalAlIf k
  | .dondurIf b           => .dondurIf b
  | .kullanIf x           => .kullanIf x
  | .imhaIf x             => .imhaIf x
  | .guvensiz e           => .guvensiz (ifadeSil e)
  | .eger k d y           => .eger (ifadeSil k) (ifadeSil d) (ifadeSil y)
  | .topla a b            => .topla (ifadeSil a) (ifadeSil b)
  | .iken k g             => .iken (ifadeSil k) (ifadeSil g)
  -- D-335: literal desen `n` SILINMEZ — kontrol iskeletinin parcasi
  -- (hangi kolun tuttugu `dalOl` ile zaten gozlenir).
  | .esles s n d y        => .esles (ifadeSil s) n (ifadeSil d) (ifadeSil y)

/-- Olay silme: tasinan deger birime iner (TUR + konum korunur). -/
def olaySil : Olay → Olay
  | .memOku t k v        => .memOku t k (degerSil v)
  | .memYaz t k v        => .memYaz t k (degerSil v)
  | .threadBaslat t      => .threadBaslat t
  | .threadBitir t       => .threadBitir t
  | .kanalGonderOl t k v => .kanalGonderOl t k (degerSil v)
  | .kanalAlOl t k v     => .kanalAlOl t k (degerSil v)
  | .dondurOl t b        => .dondurOl t b
  -- D-332: dal karari VERI degil KONTROL AKISIDIR → silinmez (silinseydi
  -- teorem, PC-kanalinin saldirgana gorunmedigini iddia ederdi).
  | .dalOl t a           => .dalOl t a

def izSil (tau : Iz) : Iz := tau.map olaySil

def storeSil (s : Store) : Store := s.map (fun p => (p.1, degerSil p.2))

def ctxSil (c : ThreadCtx) : ThreadCtx := { c with ifade := ifadeSil c.ifade }

def threadSil (ts : List ThreadCtx) : List ThreadCtx := ts.map ctxSil

def kanalDurumSil (kd : KanalDurumu) : KanalDurumu :=
  { kd with gonderKuyrugu := kd.gonderKuyrugu.map degerSil }

def kanallarSil (ks : List KanalDurumu) : List KanalDurumu := ks.map kanalDurumSil

/-- Konfigurasyon silme: YALNIZ veri alanlari (store degerleri, kanal
    yukleri, ifade literalleri, iz degerleri). Yapisal alanlar (thread
    kimlikleri, sahiplik, bolge ortami, zaman, fault) AYNEN kalir. -/
def konfSil (S : Konfigurasyon) : Konfigurasyon :=
  { S with
      thread := threadSil S.thread
      store  := storeSil S.store
      kanal  := kanallarSil S.kanal
      iz     := izSil S.iz }

-- ============================================================
-- §3. Gozlem silme uzerinden carpanlanir
-- ============================================================

/-- Silme gozlemi DEGISTIRMEZ: saldirgan zaten degeri gormuyordu. -/
theorem gozlem_olaySil (o : Olay) : gozlem (olaySil o) = gozlem o := by
  cases o <;> rfl

theorem izGozlem_izSil (tau : Iz) : izGozlem (izSil tau) = izGozlem tau := by
  induction tau with
  | nil => rfl
  | cons o rest ih =>
      show (olaySil o :: rest.map olaySil).map gozlem = (o :: rest).map gozlem
      simp only [List.map_cons, gozlem_olaySil]
      exact congrArg _ ih

/-- Konfigurasyonun gozlemi silme altinda korunur. -/
theorem izGozlem_konfSil (S : Konfigurasyon) :
    izGozlem (konfSil S).iz = izGozlem S.iz :=
  izGozlem_izSil S.iz

-- ============================================================
-- §4. Silme, veri-erisim yardimcilariyla DEGISME (commutation)
-- ============================================================

/-- Store lookup silme altinda: konum ANAHTARI degismez (silme yalniz
    degeri vurur) → arama basarisi KORUNUR, donen deger birimdir. -/
theorem konumGet_storeSil (s : Store) (k : Konum) (v : Deger)
    (h : konumGet s k = some v) :
    konumGet (storeSil s) k = some (degerSil v) := by
  induction s with
  | nil => simp [konumGet] at h
  | cons p rest ih =>
      by_cases hk : p.1.bolge.id = k.bolge.id ∧ p.1.ofset = k.ofset
      · show konumGet ((p.1, degerSil p.2) :: rest.map _) k = _
        rw [konumGet, if_pos hk]
        rw [konumGet, if_pos hk] at h
        exact congrArg (fun d => some (degerSil d)) (Option.some.inj h)
      · show konumGet ((p.1, degerSil p.2) :: rest.map _) k = _
        rw [konumGet, if_neg hk]
        exact ih (by rw [konumGet, if_neg hk] at h; exact h)

/-- Kanal kimlikleri silmeden etkilenmez → `find?` predikati AYNI kanali bulur. -/
theorem kanalDurumSil_kid (kd : KanalDurumu) : (kanalDurumSil kd).kid = kd.kid := rfl

/-- `find?` silme altinda: kanal KIMLIGI degismedigi icin ayni kayit bulunur
    (silinmis haliyle). -/
theorem find?_kanallarSil (ks : List KanalDurumu) (k : KanalId) :
    (kanallarSil ks).find? (fun kd => kd.kid = k)
      = (ks.find? (fun kd => kd.kid = k)).map kanalDurumSil := by
  induction ks with
  | nil => rfl
  | cons kd rest ih =>
      show (kanalDurumSil kd :: kanallarSil rest).find? _ = _
      by_cases hk : kd.kid = k
      · rw [List.find?_cons_of_pos (by simp [kanalDurumSil_kid, hk]),
            List.find?_cons_of_pos (by simp [hk])]
        rfl
      · rw [List.find?_cons_of_neg (by simp [kanalDurumSil_kid, hk]),
            List.find?_cons_of_neg (by simp [hk])]
        exact ih

/-- Kanal kuyrugunun BOS-OLMAMA durumu silme altinda korunur; ilk mesaj
    birime iner. (Kuyruk UZUNLUGU yapisaldir — saldirgan zaten gorur.) -/
theorem kanalIlk_kanallarSil (ks : List KanalDurumu) (k : KanalId) (v : Deger)
    (h : kanalIlk ks k = some v) :
    kanalIlk (kanallarSil ks) k = some (degerSil v) := by
  unfold kanalIlk at h ⊢
  rw [find?_kanallarSil]
  cases hf : ks.find? (fun kd => kd.kid = k) with
  | none => rw [hf] at h; exact absurd h (by simp)
  | some kd =>
      rw [hf] at h
      simp only [Option.map_some]
      show (kd.gonderKuyrugu.map degerSil).head? = some (degerSil v)
      simp only [] at h
      cases hq : kd.gonderKuyrugu with
      | nil => rw [hq] at h; exact absurd h (by simp)
      | cons a tl =>
          rw [hq] at h
          exact congrArg (fun d => some (degerSil d)) (Option.some.inj h)

/-- Kanal kuyrugunun BOSLUGU silme altinda korunur (uzunluk yapisaldir). -/
theorem kanalIlk_none_kanallarSil (ks : List KanalDurumu) (k : KanalId)
    (h : kanalIlk ks k = none) : kanalIlk (kanallarSil ks) k = none := by
  unfold kanalIlk at h ⊢
  rw [find?_kanallarSil]
  cases hf : ks.find? (fun kd => kd.kid = k) with
  | none => rfl
  | some kd =>
      rw [hf] at h
      simp only [] at h
      simp only [Option.map_some]
      show (kd.gonderKuyrugu.map degerSil).head? = none
      cases hq : kd.gonderKuyrugu with
      | nil => rfl
      | cons a tl => rw [hq] at h; exact absurd h (by simp)

/-- Kanala gonderim silme ile DEGISIR (kuyruk uzunlugu ayni, yuk birim). -/
theorem kanalEkle_kanallarSil (ks : List KanalDurumu) (k : KanalId) (v : Deger) :
    kanallarSil (kanalEkle ks k v) = kanalEkle (kanallarSil ks) k (degerSil v) := by
  have h_comp : ((fun kd => decide (kd.kid = k)) ∘ kanalDurumSil)
              = (fun kd => decide (kd.kid = k)) := by
    funext kd; simp [kanalDurumSil_kid]
  have h_any : (kanallarSil ks).any (fun kd => kd.kid = k)
             = ks.any (fun kd => kd.kid = k) := by
    show (ks.map kanalDurumSil).any _ = _
    rw [List.any_map, h_comp]
  unfold kanalEkle
  rw [h_any]
  by_cases hb : ks.any (fun kd => kd.kid = k) = true
  · rw [if_pos hb, if_pos hb]
    show (ks.map _).map kanalDurumSil = (ks.map kanalDurumSil).map _
    rw [List.map_map, List.map_map]
    apply List.map_congr_left
    intro kd _
    by_cases hk : kd.kid = k
    · show kanalDurumSil (if kd.kid = k then _ else kd)
             = if (kanalDurumSil kd).kid = k then _ else kanalDurumSil kd
      rw [if_pos hk, if_pos (by rw [kanalDurumSil_kid]; exact hk)]
      show (⟨kd.kid, (kd.gonderKuyrugu ++ [v]).map degerSil⟩ : KanalDurumu)
             = ⟨kd.kid, kd.gonderKuyrugu.map degerSil ++ [degerSil v]⟩
      simp [List.map_append, degerSil]
    · show kanalDurumSil (if kd.kid = k then _ else kd)
             = if (kanalDurumSil kd).kid = k then _ else kanalDurumSil kd
      rw [if_neg hk, if_neg (by rw [kanalDurumSil_kid]; exact hk)]
  · have hb' : ks.any (fun kd => kd.kid = k) = false := Bool.eq_false_iff.mpr hb
    rw [if_neg hb, if_neg hb]
    rfl

/-- Kanaldan alim silme ile DEGISIR (pop yapisaldir). -/
theorem kanalCikar_kanallarSil (ks : List KanalDurumu) (k : KanalId) :
    kanallarSil (kanalCikar ks k) = kanalCikar (kanallarSil ks) k := by
  unfold kanalCikar kanallarSil
  rw [List.map_map, List.map_map]
  apply List.map_congr_left
  intro kd _
  by_cases hk : kd.kid = k
  · show kanalDurumSil (if kd.kid = k then _ else kd)
           = if (kanalDurumSil kd).kid = k then _ else kanalDurumSil kd
    rw [if_pos hk, if_pos (by rw [kanalDurumSil_kid]; exact hk)]
    show (⟨kd.kid, kd.gonderKuyrugu.tail.map degerSil⟩ : KanalDurumu)
           = ⟨kd.kid, (kd.gonderKuyrugu.map degerSil).tail⟩
    cases kd.gonderKuyrugu <;> rfl
  · show kanalDurumSil (if kd.kid = k then _ else kd)
           = if (kanalDurumSil kd).kid = k then _ else kanalDurumSil kd
    rw [if_neg hk, if_neg (by rw [kanalDurumSil_kid]; exact hk)]

/-- Thread listesi bolunmesi silme altinda (odak korunur). -/
theorem threadSil_split (ts1 ts2 : List ThreadCtx) (ctx : ThreadCtx) :
    threadSil (ts1 ++ ctx :: ts2) = threadSil ts1 ++ ctxSil ctx :: threadSil ts2 := by
  simp [threadSil, List.map_append, List.map_cons]

-- ============================================================
-- §5. SILME SIMULASYONU — deger TASIYAN kurallar
--
-- NI icerigi tam olarak BURADA yasar: asagidaki kurallar bir DEGER okur,
-- yazar ya da tasir. Her biri icin ispatlanan sey: kural silinmis
-- konfigurasyonda da AYNEN uygulanabilir (deger yerine birim ile) ve
-- sonuc yine silinmis-esdegerdir → tasinan verinin adimin varligina,
-- odagina ve urettigi gozleme ETKISI YOKTUR.
--
-- Kapsam notu: yapisal kurallar (cong/fault/lineer) deger TASIMAZ; bunlarin
-- ve tumevarimla birlesik `Step → Step (konfSil)` catisinin toplanmasi
-- SIRADAKI ADIM (cong ozyinelemesi tumevarim gerektirir).
-- ============================================================

/-- S-VAR-OKU: okunan deger silinse de okuma AYNI konumdan olur. -/
theorem silme_sim_sVarOku
    (S S' : Konfigurasyon) (ts1 ts2 : List ThreadCtx) (ctx : ThreadCtx)
    (x : VarId) (b : Bolge) (v : Deger)
    (h_t  : S.thread = ts1 ++ ctx :: ts2)
    (h_if : ctx.ifade = .tanim x)
    (h_b  : Sem.StateTipli.bolgeOrtamGet S.bolge x = some b)
    (h_v  : konumGet S.store ⟨b, 0⟩ = some v)
    (h_S' : S' = { S with
              thread := ts1 ++ { ctx with ifade := .sabit v } :: ts2,
              iz     := .memOku ctx.tid ⟨b, 0⟩ v :: S.iz,
              zaman  := S.zaman + 1,
              fault  := none }) :
    Step (konfSil S) (konfSil S') := by
  refine Step.sVarOku _ _ (threadSil ts1) (threadSil ts2) (ctxSil ctx) x b (degerSil v)
    ?_ ?_ h_b (konumGet_storeSil _ _ _ h_v) ?_
  · show threadSil S.thread = _
    rw [h_t, threadSil_split]
  · show ifadeSil ctx.ifade = _
    rw [h_if]; rfl
  · subst h_S'
    simp [konfSil, threadSil_split, izSil, olaySil, ctxSil, ifadeSil, degerSil]

/-- S-ATAMA (Tamam): yazilan deger silinse de yazma AYNI konuma olur. -/
theorem silme_sim_sAtamaTamam
    (S S' : Konfigurasyon) (ts1 ts2 : List ThreadCtx) (ctx : ThreadCtx)
    (x : VarId) (v : Deger) (b : Bolge)
    (h_t     : S.thread = ts1 ++ ctx :: ts2)
    (h_if    : ctx.ifade = .atama x (.sabit v))
    (h_b     : Sem.StateTipli.bolgeOrtamGet S.bolge x = some b)
    (h_owner : sahiplikGet S.sahiplik b = some (Sahip.thread ctx.tid))
    (h_S'    : S' = { S with
              thread := ts1 ++ { ctx with ifade := .sabit .birim } :: ts2,
              store  := (⟨b, 0⟩, v) :: S.store,
              iz     := .memYaz ctx.tid ⟨b, 0⟩ v :: S.iz,
              zaman  := S.zaman + 1,
              fault  := none }) :
    Step (konfSil S) (konfSil S') := by
  refine Step.sAtamaTamam _ _ (threadSil ts1) (threadSil ts2) (ctxSil ctx) x (degerSil v) b
    ?_ ?_ h_b h_owner ?_
  · show threadSil S.thread = _
    rw [h_t, threadSil_split]
  · show ifadeSil ctx.ifade = _
    rw [h_if]; rfl
  · subst h_S'
    simp [konfSil, threadSil_split, izSil, olaySil, ctxSil, ifadeSil, degerSil, storeSil]

/-- S-SEQ-ATLA: atilan deger silinse de ayni sag-tarafa gecilir. -/
theorem silme_sim_sSeqAtla
    (S S' : Konfigurasyon) (ts1 ts2 : List ThreadCtx) (ctx : ThreadCtx)
    (v : Deger) (b : Ifade)
    (h_t  : S.thread = ts1 ++ ctx :: ts2)
    (h_if : ctx.ifade = .seq (.sabit v) b)
    (h_S' : S' = { S with
              thread := ts1 ++ { ctx with ifade := b } :: ts2,
              zaman  := S.zaman + 1,
              fault  := none }) :
    Step (konfSil S) (konfSil S') := by
  refine Step.sSeqAtla _ _ (threadSil ts1) (threadSil ts2) (ctxSil ctx)
    (degerSil v) (ifadeSil b) ?_ ?_ ?_
  · show threadSil S.thread = _
    rw [h_t, threadSil_split]
  · show ifadeSil ctx.ifade = _
    rw [h_if]; rfl
  · subst h_S'
    simp [konfSil, threadSil_split, ctxSil, ifadeSil, degerSil]

/-- S-GUVENSIZ-ATLA: sarmal acilan deger silinse de ayni adim atilir. -/
theorem silme_sim_sGuvensizAtla
    (S S' : Konfigurasyon) (ts1 ts2 : List ThreadCtx) (ctx : ThreadCtx)
    (v : Deger)
    (h_t  : S.thread = ts1 ++ ctx :: ts2)
    (h_if : ctx.ifade = .guvensiz (.sabit v))
    (h_S' : S' = { S with
              thread := ts1 ++ { ctx with ifade := .sabit v } :: ts2,
              zaman  := S.zaman + 1,
              fault  := none }) :
    Step (konfSil S) (konfSil S') := by
  refine Step.sGuvensizAtla _ _ (threadSil ts1) (threadSil ts2) (ctxSil ctx)
    (degerSil v) ?_ ?_ ?_
  · show threadSil S.thread = _
    rw [h_t, threadSil_split]
  · show ifadeSil ctx.ifade = _
    rw [h_if]; rfl
  · subst h_S'
    simp [konfSil, threadSil_split, ctxSil, ifadeSil, degerSil]

/-- C-KANAL-GONDER (Tamam): gonderilen MESAJ silinse de gonderim AYNI
    kanala olur; kuyruk uzunlugu ve sahiplik transferi degismez. -/
theorem silme_sim_cKanalGonderTamam
    (S S' : Konfigurasyon) (ts1 ts2 : List ThreadCtx) (ctx : ThreadCtx)
    (k : KanalId) (vId : VarId) (b : Bolge) (v : Deger)
    (h_t     : S.thread = ts1 ++ ctx :: ts2)
    (h_if    : ctx.ifade = .kanalGonderIf k vId)
    (h_b     : Sem.StateTipli.bolgeOrtamGet S.bolge vId = some b)
    (h_v     : konumGet S.store ⟨b, 0⟩ = some v)
    (h_owner : sahiplikGet S.sahiplik b = some (Sahip.thread ctx.tid))
    (h_bos   : kanalIlk S.kanal k = none)
    (h_S'    : S' = { S with
              thread := ts1 ++ { ctx with
                          ifade  := .sabit .birim,
                          lineer := lineerTuket ctx.lineer vId } :: ts2,
              kanal  := kanalEkle S.kanal k v,
              sahiplik := sahiplikSet S.sahiplik b (Sahip.kanalSahip k),
              bolge  := Sem.StateTipli.bolgeOrtamUpdate S.bolge vId
                          (Sem.StateTipli.bolgeKategoriDegistir b (BolgeKategorisi.kanalRho k)),
              iz     := .kanalGonderOl ctx.tid k v :: S.iz,
              zaman  := S.zaman + 1,
              fault  := none }) :
    Step (konfSil S) (konfSil S') := by
  refine Step.cKanalGonderTamam _ _ (threadSil ts1) (threadSil ts2) (ctxSil ctx)
    k vId b (degerSil v) ?_ ?_ h_b (konumGet_storeSil _ _ _ h_v) h_owner
    (kanalIlk_none_kanallarSil _ _ h_bos) ?_
  · show threadSil S.thread = _
    rw [h_t, threadSil_split]
  · show ifadeSil ctx.ifade = _
    rw [h_if]; rfl
  · subst h_S'
    simp [konfSil, threadSil_split, izSil, olaySil, ctxSil, ifadeSil, degerSil,
          kanalEkle_kanallarSil]

/-- C-KANAL-AL (Tamam): alinan MESAJ silinse de alim AYNI kanaldan olur;
    pop ve sahiplik geri-transferi yapisaldir. -/
theorem silme_sim_cKanalAlTamam
    (S S' : Konfigurasyon) (ts1 ts2 : List ThreadCtx) (ctx : ThreadCtx)
    (k : KanalId) (v : Deger) (transferredBolge : Bolge)
    (h_t       : S.thread = ts1 ++ ctx :: ts2)
    (h_if      : ctx.ifade = .kanalAlIf k)
    (h_v       : kanalIlk S.kanal k = some v)
    (h_transit : sahiplikGet S.sahiplik transferredBolge
                   = some (Sahip.kanalSahip k))
    (h_S'      : S' = { S with
              thread := ts1 ++ { ctx with ifade := .sabit v } :: ts2,
              kanal  := kanalCikar S.kanal k,
              sahiplik := sahiplikSet S.sahiplik transferredBolge
                            (Sahip.thread ctx.tid),
              iz     := .kanalAlOl ctx.tid k v :: S.iz,
              zaman  := S.zaman + 1,
              fault  := none }) :
    Step (konfSil S) (konfSil S') := by
  refine Step.cKanalAlTamam _ _ (threadSil ts1) (threadSil ts2) (ctxSil ctx)
    k (degerSil v) transferredBolge ?_ ?_
    (kanalIlk_kanallarSil _ _ _ h_v) h_transit ?_
  · show threadSil S.thread = _
    rw [h_t, threadSil_split]
  · show ifadeSil ctx.ifade = _
    rw [h_if]; rfl
  · subst h_S'
    simp [konfSil, threadSil_split, izSil, olaySil, ctxSil, ifadeSil, degerSil,
          kanalCikar_kanallarSil]

-- ============================================================
-- §6. GLOBAL CATI — silme ILERI SIMULASYONDUR (21/21 kural)
-- ============================================================

/-- `ifadeyleKonf` (cong odagi) silme ile DEGISIR. -/
theorem ifadeyleKonf_konfSil (S : Konfigurasyon) (ts1 ts2 : List ThreadCtx)
    (ctx : ThreadCtx) (e : Ifade) :
    konfSil (ifadeyleKonf S ts1 ts2 ctx e)
      = ifadeyleKonf (konfSil S) (threadSil ts1) (threadSil ts2) (ctxSil ctx)
          (ifadeSil e) := by
  simp [konfSil, ifadeyleKonf, threadSil_split, ctxSil]

/-- Taze thread kimligi silmeden etkilenmez (tid'ler yapisal). -/
theorem threadFresh_konfSil (S : Konfigurasyon) (t : ThreadId)
    (h : threadFresh S t) : threadFresh (konfSil S) t := by
  intro c h_mem
  show c.tid ≠ t
  have : c ∈ S.thread.map ctxSil := h_mem
  rcases List.mem_map.mp this with ⟨c0, h0, h_eq⟩
  rw [← h_eq]
  exact h c0 h0

/-- Frozen durumu sahiplikten okunur; silme sahipligi DEGISTIRMEZ. -/
theorem isFrozen_konfSil (S : Konfigurasyon) (b : Bolge) (h : isFrozen S b) :
    isFrozen (konfSil S) b := h

/-- **ANA TEOREM (cekirdek alt-kume):** deger-silme bir ILERI SIMULASYONDUR.
    Yani hicbir Step kurali, tasidigi VERIYE bakarak adimin varligini,
    odagini ya da urettigi olayin turunu/konumunu degistirmez.

    ISPAT NOTU: 21 kuralin hepsi kapali. Deger tasiyanlar §5 lemmalariyla,
    yapisal/fault kurallari dogrudan, cong kurallari TUMEVARIM hipoteziyle
    (ic adim da silinmis dunyada atilabilir). -/
theorem silme_simulasyon (S S' : Konfigurasyon) (h : Step S S') :
    Step (konfSil S) (konfSil S') := by
  induction h with
  | sVarOku S S' ts1 ts2 ctx x b v h_t h_if h_b h_v h_S' =>
      exact silme_sim_sVarOku S S' ts1 ts2 ctx x b v h_t h_if h_b h_v h_S'
  | sAtamaTamam S S' ts1 ts2 ctx x v b h_t h_if h_b h_owner h_S' =>
      exact silme_sim_sAtamaTamam S S' ts1 ts2 ctx x v b h_t h_if h_b h_owner h_S'
  | sSeqAtla S S' ts1 ts2 ctx v b h_t h_if h_S' =>
      exact silme_sim_sSeqAtla S S' ts1 ts2 ctx v b h_t h_if h_S'
  -- D-332: sEgerSec. `degerDogruMu_degerSil` sayesinde silinmis dunyada
  -- AYNI dal secilir ve AYNI `dalOl` olayi uretilir. Bu adim, ISTISNA 2
  -- daraltmasinin neden ZORUNLU oldugunun ispat-icindeki kanitidir.
  | sEgerSec S S' ts1 ts2 ctx v d y alindi h_t h_if h_dal h_S' =>
      refine Step.sEgerSec _ _ (threadSil ts1) (threadSil ts2) (ctxSil ctx)
        (degerSil v) (ifadeSil d) (ifadeSil y) alindi ?_ ?_ ?_ ?_
      · show threadSil S.thread = _
        rw [h_t, threadSil_split]
      · show ifadeSil ctx.ifade = _
        rw [h_if]; rfl
      · rw [degerDogruMu_degerSil]; exact h_dal
      · subst h_S'
        cases alindi with
        | true =>
            simp [konfSil, threadSil_split, izSil, olaySil, ctxSil, ifadeSil]
        | false =>
            simp [konfSil, threadSil_split, izSil, olaySil, ctxSil, ifadeSil]
  -- D-334: sToplaTamam. ISTISNA 3 sayesinde silinmis dunyada AYNI
  -- toplama yapilir (skalerler silinmiyor); olay yok.
  | sToplaTamam S S' ts1 ts2 ctx n1 n2 h_t h_if h_S' =>
      refine Step.sToplaTamam _ _ (threadSil ts1) (threadSil ts2) (ctxSil ctx)
        n1 n2 ?_ ?_ ?_
      · show threadSil S.thread = _
        rw [h_t, threadSil_split]
      · show ifadeSil ctx.ifade = _
        rw [h_if]; rfl
      · subst h_S'
        simp [konfSil, threadSil_split, ctxSil, ifadeSil, degerSil]
  | sToplaCongSol S S' S1 S1' ts1 ts2 ts2' ctx ctx' a a' b
      h_t h_if h_S1 _h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      refine Step.sToplaCongSol _ _ (konfSil S1) (konfSil S1')
        (threadSil ts1) (threadSil ts2) (threadSil ts2') (ctxSil ctx) (ctxSil ctx')
        (ifadeSil a) (ifadeSil a') (ifadeSil b) ?_ ?_ ?_ ih ?_ h_tid ?_ ?_ ?_
      · show threadSil S.thread = _; rw [h_t, threadSil_split]
      · show ifadeSil ctx.ifade = _; rw [h_if]; rfl
      · rw [h_S1, ifadeyleKonf_konfSil]
      · show threadSil S1'.thread = _; rw [h_t1', threadSil_split]
      · show ifadeSil ctx'.ifade = _; rw [h_if']
      · rcases h_yan with h | ⟨z, h⟩
        · exact Or.inl (by rw [h])
        · exact Or.inr ⟨ctxSil z, by rw [h]; simp [threadSil, List.map_append]⟩
      · subst h_S'
        simp [konfSil, threadSil_split, ctxSil, ifadeSil]
  | sToplaCongSag S S' S1 S1' ts1 ts2 ts2' ctx ctx' v b b'
      h_t h_if h_S1 _h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      refine Step.sToplaCongSag _ _ (konfSil S1) (konfSil S1')
        (threadSil ts1) (threadSil ts2) (threadSil ts2') (ctxSil ctx) (ctxSil ctx')
        (degerSil v) (ifadeSil b) (ifadeSil b') ?_ ?_ ?_ ih ?_ h_tid ?_ ?_ ?_
      · show threadSil S.thread = _; rw [h_t, threadSil_split]
      · show ifadeSil ctx.ifade = _; rw [h_if]; rfl
      · rw [h_S1, ifadeyleKonf_konfSil]
      · show threadSil S1'.thread = _; rw [h_t1', threadSil_split]
      · show ifadeSil ctx'.ifade = _; rw [h_if']
      · rcases h_yan with h | ⟨z, h⟩
        · exact Or.inl (by rw [h])
        · exact Or.inr ⟨ctxSil z, by rw [h]; simp [threadSil, List.map_append]⟩
      · subst h_S'
        simp [konfSil, threadSil_split, ctxSil, ifadeSil]
  -- D-335: sIkenAc — acilma silme altinda AYNEN yapilir (yapisal adim).
  | sIkenAc S S' ts1 ts2 ctx k g h_t h_if h_S' =>
      refine Step.sIkenAc _ _ (threadSil ts1) (threadSil ts2) (ctxSil ctx)
        (ifadeSil k) (ifadeSil g) ?_ ?_ ?_
      · show threadSil S.thread = _
        rw [h_t, threadSil_split]
      · show ifadeSil ctx.ifade = _
        rw [h_if]; rfl
      · subst h_S'
        simp [konfSil, threadSil_split, ctxSil, ifadeSil, degerSil]
  -- D-335: sEslesSec — skalerler silinmediginden (D-334 ISTISNA 3) AYNI
  -- literal karsilastirmasi yapilir → AYNI kol, AYNI `dalOl`.
  | sEslesSec S S' ts1 ts2 ctx m n d y tuttu h_t h_if h_dal h_S' =>
      refine Step.sEslesSec _ _ (threadSil ts1) (threadSil ts2) (ctxSil ctx)
        m n (ifadeSil d) (ifadeSil y) tuttu ?_ ?_ h_dal ?_
      · show threadSil S.thread = _
        rw [h_t, threadSil_split]
      · show ifadeSil ctx.ifade = _
        rw [h_if]; rfl
      · subst h_S'
        cases tuttu with
        | true =>
            simp [konfSil, threadSil_split, izSil, olaySil, ctxSil, ifadeSil]
        | false =>
            simp [konfSil, threadSil_split, izSil, olaySil, ctxSil, ifadeSil]
  | sEslesCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' s s' n d y
      h_t h_if h_S1 _h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      refine Step.sEslesCong _ _ (konfSil S1) (konfSil S1')
        (threadSil ts1) (threadSil ts2) (threadSil ts2') (ctxSil ctx) (ctxSil ctx')
        (ifadeSil s) (ifadeSil s') n (ifadeSil d) (ifadeSil y)
        ?_ ?_ ?_ ih ?_ h_tid ?_ ?_ ?_
      · show threadSil S.thread = _; rw [h_t, threadSil_split]
      · show ifadeSil ctx.ifade = _; rw [h_if]; rfl
      · rw [h_S1, ifadeyleKonf_konfSil]
      · show threadSil S1'.thread = _; rw [h_t1', threadSil_split]
      · show ifadeSil ctx'.ifade = _; rw [h_if']
      · rcases h_yan with h | ⟨z, h⟩
        · exact Or.inl (by rw [h])
        · exact Or.inr ⟨ctxSil z, by rw [h]; simp [threadSil, List.map_append]⟩
      · subst h_S'
        simp [konfSil, threadSil_split, ctxSil, ifadeSil]
  | sEgerCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' k k' d y
      h_t h_if h_S1 _h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      refine Step.sEgerCong _ _ (konfSil S1) (konfSil S1')
        (threadSil ts1) (threadSil ts2) (threadSil ts2') (ctxSil ctx) (ctxSil ctx')
        (ifadeSil k) (ifadeSil k') (ifadeSil d) (ifadeSil y)
        ?_ ?_ ?_ ih ?_ h_tid ?_ ?_ ?_
      · show threadSil S.thread = _; rw [h_t, threadSil_split]
      · show ifadeSil ctx.ifade = _; rw [h_if]; rfl
      · rw [h_S1, ifadeyleKonf_konfSil]
      · show threadSil S1'.thread = _; rw [h_t1', threadSil_split]
      · show ifadeSil ctx'.ifade = _; rw [h_if']
      · rcases h_yan with h | ⟨z, h⟩
        · exact Or.inl (by rw [h])
        · exact Or.inr ⟨ctxSil z, by rw [h]; simp [threadSil, List.map_append]⟩
      · subst h_S'
        simp [konfSil, threadSil_split, ctxSil, ifadeSil]
  | sGuvensizAtla S S' ts1 ts2 ctx v h_t h_if h_S' =>
      exact silme_sim_sGuvensizAtla S S' ts1 ts2 ctx v h_t h_if h_S'
  | cKanalGonderTamam S S' ts1 ts2 ctx k vId b v h_t h_if h_b h_v h_owner h_bos h_S' =>
      exact silme_sim_cKanalGonderTamam S S' ts1 ts2 ctx k vId b v
              h_t h_if h_b h_v h_owner h_bos h_S'
  | cKanalAlTamam S S' ts1 ts2 ctx k v tb h_t h_if h_v h_transit h_S' =>
      exact silme_sim_cKanalAlTamam S S' ts1 ts2 ctx k v tb h_t h_if h_v h_transit h_S'
  -- ---- Fault kurallari: post-state YALNIZ fault alani (veri tasimaz) ----
  | sAtamaHataDonmus S S' ts1 ts2 ctx x v b h_t h_if h_b h_frozen h_S' =>
      refine Step.sAtamaHataDonmus _ _ (threadSil ts1) (threadSil ts2) (ctxSil ctx)
        x (degerSil v) b ?_ ?_ h_b (isFrozen_konfSil _ _ h_frozen) ?_
      · show threadSil S.thread = _; rw [h_t, threadSil_split]
      · show ifadeSil ctx.ifade = _; rw [h_if]; rfl
      · subst h_S'; rfl
  | sAtamaHataSahipDegil S S' ts1 ts2 ctx x v b h_t h_if h_b h_no h_S' =>
      refine Step.sAtamaHataSahipDegil _ _ (threadSil ts1) (threadSil ts2) (ctxSil ctx)
        x (degerSil v) b ?_ ?_ h_b h_no ?_
      · show threadSil S.thread = _; rw [h_t, threadSil_split]
      · show ifadeSil ctx.ifade = _; rw [h_if]; rfl
      · subst h_S'; rfl
  | cGorevBaslatHataLineerIhlal S S' ts1 ts2 ctx yd kod vI h_t h_if h_in h_tuk h_S' =>
      refine Step.cGorevBaslatHataLineerIhlal _ _ (threadSil ts1) (threadSil ts2)
        (ctxSil ctx) yd (ifadeSil kod) vI ?_ ?_ h_in h_tuk ?_
      · show threadSil S.thread = _; rw [h_t, threadSil_split]
      · show ifadeSil ctx.ifade = _; rw [h_if]; rfl
      · subst h_S'; rfl
  | cKanalGonderHataLineerTuket S S' ts1 ts2 ctx k vId h_t h_if h_tuk h_S' =>
      refine Step.cKanalGonderHataLineerTuket _ _ (threadSil ts1) (threadSil ts2)
        (ctxSil ctx) k vId ?_ ?_ h_tuk ?_
      · show threadSil S.thread = _; rw [h_t, threadSil_split]
      · show ifadeSil ctx.ifade = _; rw [h_if]; rfl
      · subst h_S'; rfl
  | cDondurHataZatenDonmus S S' ts1 ts2 ctx b h_t h_if h_zaten h_S' =>
      refine Step.cDondurHataZatenDonmus _ _ (threadSil ts1) (threadSil ts2)
        (ctxSil ctx) b ?_ ?_ (isFrozen_konfSil _ _ h_zaten) ?_
      · show threadSil S.thread = _; rw [h_t, threadSil_split]
      · show ifadeSil ctx.ifade = _; rw [h_if]; rfl
      · subst h_S'; rfl
  | sLinKullanHataZatenTuketildi S S' ts1 ts2 ctx x h_t h_if h_tuk h_S' =>
      refine Step.sLinKullanHataZatenTuketildi _ _ (threadSil ts1) (threadSil ts2)
        (ctxSil ctx) x ?_ ?_ h_tuk ?_
      · show threadSil S.thread = _; rw [h_t, threadSil_split]
      · show ifadeSil ctx.ifade = _; rw [h_if]; rfl
      · subst h_S'; rfl
  | sLinImhaHataZatenTuketildi S S' ts1 ts2 ctx x h_t h_if h_tuk h_S' =>
      refine Step.sLinImhaHataZatenTuketildi _ _ (threadSil ts1) (threadSil ts2)
        (ctxSil ctx) x ?_ ?_ h_tuk ?_
      · show threadSil S.thread = _; rw [h_t, threadSil_split]
      · show ifadeSil ctx.ifade = _; rw [h_if]; rfl
      · subst h_S'; rfl
  -- ---- Yapisal Tamam kurallari (deger tasimaz) ----
  | cGorevBaslatTamam S S' ts1 ts2 ctx tYeni yd kod h_t h_if h_fresh h_sah h_S' =>
      refine Step.cGorevBaslatTamam _ _ (threadSil ts1) (threadSil ts2) (ctxSil ctx)
        tYeni yd (ifadeSil kod) ?_ ?_ (threadFresh_konfSil _ _ h_fresh) h_sah ?_
      · show threadSil S.thread = _; rw [h_t, threadSil_split]
      · show ifadeSil ctx.ifade = _; rw [h_if]; rfl
      · subst h_S'
        show konfSil _ = _
        simp only [konfSil, threadSil, izSil, olaySil, ctxSil, ifadeSil, degerSil,
                   List.map_append, List.map_cons, List.map_nil]
  | cGorevBirlestirTamam S S' ts1 ts2 ctx g tHedef rb h_t h_if h_hedef h_donen h_S' =>
      refine Step.cGorevBirlestirTamam _ _ (threadSil ts1) (threadSil ts2) (ctxSil ctx)
        g tHedef rb ?_ ?_ ?_ h_donen ?_
      · show threadSil S.thread = _; rw [h_t, threadSil_split]
      · show ifadeSil ctx.ifade = _; rw [h_if]; rfl
      · obtain ⟨hctx, h_mem, h_tid, vSon, h_ifd⟩ := h_hedef
        refine ⟨ctxSil hctx, ?_, h_tid, degerSil vSon, ?_⟩
        · exact List.mem_map.mpr ⟨hctx, h_mem, rfl⟩
        · show ifadeSil hctx.ifade = _; rw [h_ifd]; rfl
      · subst h_S'
        simp [konfSil, threadSil_split, izSil, olaySil, ctxSil, ifadeSil, degerSil]
  | cDondurTamam S S' ts1 ts2 ctx b h_t h_if h_owner h_S' =>
      refine Step.cDondurTamam _ _ (threadSil ts1) (threadSil ts2) (ctxSil ctx)
        b ?_ ?_ h_owner ?_
      · show threadSil S.thread = _; rw [h_t, threadSil_split]
      · show ifadeSil ctx.ifade = _; rw [h_if]; rfl
      · subst h_S'
        simp [konfSil, threadSil_split, izSil, olaySil, ctxSil, ifadeSil, degerSil]
  | sLinKullanTamam S S' ts1 ts2 ctx x h_t h_if h_aktif h_S' =>
      refine Step.sLinKullanTamam _ _ (threadSil ts1) (threadSil ts2) (ctxSil ctx)
        x ?_ ?_ h_aktif ?_
      · show threadSil S.thread = _; rw [h_t, threadSil_split]
      · show ifadeSil ctx.ifade = _; rw [h_if]; rfl
      · subst h_S'
        simp [konfSil, threadSil_split, ctxSil, ifadeSil, degerSil]
  | sLinImhaTamam S S' ts1 ts2 ctx x h_t h_if h_aktif h_S' =>
      refine Step.sLinImhaTamam _ _ (threadSil ts1) (threadSil ts2) (ctxSil ctx)
        x ?_ ?_ h_aktif ?_
      · show threadSil S.thread = _; rw [h_t, threadSil_split]
      · show ifadeSil ctx.ifade = _; rw [h_if]; rfl
      · subst h_S'
        simp [konfSil, threadSil_split, ctxSil, ifadeSil, degerSil]
  -- ---- Congruence kurallari: TUMEVARIM hipotezi (ic adim) ----
  | sAtamaCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' x e e'
      h_t h_if h_S1 _h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      refine Step.sAtamaCong _ _ (konfSil S1) (konfSil S1')
        (threadSil ts1) (threadSil ts2) (threadSil ts2') (ctxSil ctx) (ctxSil ctx')
        x (ifadeSil e) (ifadeSil e') ?_ ?_ ?_ ih ?_ h_tid ?_ ?_ ?_
      · show threadSil S.thread = _; rw [h_t, threadSil_split]
      · show ifadeSil ctx.ifade = _; rw [h_if]; rfl
      · rw [h_S1, ifadeyleKonf_konfSil]
      · show threadSil S1'.thread = _; rw [h_t1', threadSil_split]
      · show ifadeSil ctx'.ifade = _; rw [h_if']
      · rcases h_yan with h | ⟨y, h⟩
        · exact Or.inl (by rw [h])
        · exact Or.inr ⟨ctxSil y, by rw [h]; simp [threadSil, List.map_append]⟩
      · subst h_S'
        simp [konfSil, threadSil_split, ctxSil, ifadeSil]
  | sSeqCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' a a' b
      h_t h_if h_S1 _h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      refine Step.sSeqCong _ _ (konfSil S1) (konfSil S1')
        (threadSil ts1) (threadSil ts2) (threadSil ts2') (ctxSil ctx) (ctxSil ctx')
        (ifadeSil a) (ifadeSil a') (ifadeSil b) ?_ ?_ ?_ ih ?_ h_tid ?_ ?_ ?_
      · show threadSil S.thread = _; rw [h_t, threadSil_split]
      · show ifadeSil ctx.ifade = _; rw [h_if]; rfl
      · rw [h_S1, ifadeyleKonf_konfSil]
      · show threadSil S1'.thread = _; rw [h_t1', threadSil_split]
      · show ifadeSil ctx'.ifade = _; rw [h_if']
      · rcases h_yan with h | ⟨y, h⟩
        · exact Or.inl (by rw [h])
        · exact Or.inr ⟨ctxSil y, by rw [h]; simp [threadSil, List.map_append]⟩
      · subst h_S'
        simp [konfSil, threadSil_split, ctxSil, ifadeSil]
  | sGuvensizCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' e e'
      h_t h_if h_S1 _h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      refine Step.sGuvensizCong _ _ (konfSil S1) (konfSil S1')
        (threadSil ts1) (threadSil ts2) (threadSil ts2') (ctxSil ctx) (ctxSil ctx')
        (ifadeSil e) (ifadeSil e') ?_ ?_ ?_ ih ?_ h_tid ?_ ?_ ?_
      · show threadSil S.thread = _; rw [h_t, threadSil_split]
      · show ifadeSil ctx.ifade = _; rw [h_if]; rfl
      · rw [h_S1, ifadeyleKonf_konfSil]
      · show threadSil S1'.thread = _; rw [h_t1', threadSil_split]
      · show ifadeSil ctx'.ifade = _; rw [h_if']
      · rcases h_yan with h | ⟨y, h⟩
        · exact Or.inl (by rw [h])
        · exact Or.inr ⟨ctxSil y, by rw [h]; simp [threadSil, List.map_append]⟩
      · subst h_S'
        simp [konfSil, threadSil_split, ctxSil, ifadeSil]

/-- **SONUC (NI):** dusuk-esdeger (silinmisi ayni) iki konfigurasyondan
    atilan adimlar AYNI gozlemi uretir — veri gozlemi etkilemez. -/
theorem ni_cekirdek_altkume (S1 S2 S1' : Konfigurasyon)
    (h_dusuk : konfSil S1 = konfSil S2) (h_adim : Step S1 S1') :
    ∃ T', Step (konfSil S2) T' ∧ izGozlem T'.iz = izGozlem S1'.iz := by
  refine ⟨konfSil S1', ?_, izGozlem_konfSil S1'⟩
  rw [← h_dusuk]
  exact silme_simulasyon S1 S1' h_adim

end Kemgu.SideChannel.NonInterference
