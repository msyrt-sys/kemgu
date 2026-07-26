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

HENUZ ISPATLANMAYAN (durustce — SIRADAKI ADIM):
  Global cati `∀ S S', Step S S' → Step (konfSil S) (konfSil S')`. Kalan
  kurallar deger TASIMAZ (cong/fault/lineer/gorev/dondur) ama cati bir
  TUMEVARIM ister (cong kurallari `h_inner : Step S1 S1'` ile ozyinelemeli).
  Politika geregi `sorry` KONMADI: catı ifadesi yazilmadi, yalniz ispatlanan
  parcalar duruyor.

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

/-- Olay → gozlem (deger projeksiyonu ATILIR). -/
def gozlem : Olay → GozlemOlay
  | .memOku t k v        => .gOku t k
  | .memYaz t k v        => .gYaz t k
  | .threadBaslat t      => .gBaslat t
  | .threadBitir t       => .gBitir t
  | .kanalGonderOl t k v => .gGonder t k
  | .kanalAlOl t k v     => .gAl t k
  | .dondurOl t b        => .gDondur t b

/-- Iz → gozlem dizisi. -/
def izGozlem (tau : Iz) : List GozlemOlay := tau.map gozlem

-- ============================================================
-- §2. Deger silme (erasure) — tum veriler birime indirgenir
-- ============================================================

/-- Tum degerler tek kanonik degere (birim) iner: "saldirgan veriyi
    GORMEZ" varsayiminin sozdizimsel karsiligi. -/
def degerSil (_ : Deger) : Deger := Deger.birim

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

/-- Olay silme: tasinan deger birime iner (TUR + konum korunur). -/
def olaySil : Olay → Olay
  | .memOku t k v        => .memOku t k (degerSil v)
  | .memYaz t k v        => .memYaz t k (degerSil v)
  | .threadBaslat t      => .threadBaslat t
  | .threadBitir t       => .threadBitir t
  | .kanalGonderOl t k v => .kanalGonderOl t k (degerSil v)
  | .kanalAlOl t k v     => .kanalAlOl t k (degerSil v)
  | .dondurOl t b        => .dondurOl t b

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
    konumGet (storeSil s) k = some Deger.birim := by
  induction s with
  | nil => simp [konumGet] at h
  | cons p rest ih =>
      by_cases hk : p.1.bolge.id = k.bolge.id ∧ p.1.ofset = k.ofset
      · show konumGet ((p.1, Deger.birim) :: rest.map _) k = _
        rw [konumGet, if_pos hk]
      · show konumGet ((p.1, Deger.birim) :: rest.map _) k = _
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
    kanalIlk (kanallarSil ks) k = some Deger.birim := by
  unfold kanalIlk at h ⊢
  rw [find?_kanallarSil]
  cases hf : ks.find? (fun kd => kd.kid = k) with
  | none => rw [hf] at h; exact absurd h (by simp)
  | some kd =>
      rw [hf] at h
      simp only [Option.map_some]
      show (kd.gonderKuyrugu.map degerSil).head? = some Deger.birim
      simp only [] at h
      cases hq : kd.gonderKuyrugu with
      | nil => rw [hq] at h; exact absurd h (by simp)
      | cons a tl => rfl

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
    kanallarSil (kanalEkle ks k v) = kanalEkle (kanallarSil ks) k Deger.birim := by
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
             = ⟨kd.kid, kd.gonderKuyrugu.map degerSil ++ [Deger.birim]⟩
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
  refine Step.sVarOku _ _ (threadSil ts1) (threadSil ts2) (ctxSil ctx) x b Deger.birim
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
  refine Step.sAtamaTamam _ _ (threadSil ts1) (threadSil ts2) (ctxSil ctx) x Deger.birim b
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
    Deger.birim (ifadeSil b) ?_ ?_ ?_
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
    Deger.birim ?_ ?_ ?_
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
    k vId b Deger.birim ?_ ?_ h_b (konumGet_storeSil _ _ _ h_v) h_owner
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
    k Deger.birim transferredBolge ?_ ?_
    (kanalIlk_kanallarSil _ _ _ h_v) h_transit ?_
  · show threadSil S.thread = _
    rw [h_t, threadSil_split]
  · show ifadeSil ctx.ifade = _
    rw [h_if]; rfl
  · subst h_S'
    simp [konfSil, threadSil_split, izSil, olaySil, ctxSil, ifadeSil, degerSil,
          kanalCikar_kanallarSil]

end Kemgu.SideChannel.NonInterference
