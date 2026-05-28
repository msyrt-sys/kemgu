/-
KEMGU DRF Mekanize — Operasyonel Semantik Kucuk-Adim Reduksiyon (Faz A2.3)
Kaynak (kagit formel): belgeler/KEMGU_Operasyonel_Semantik.md §4-5
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz
-/

import Kemgu.Sem.Core

namespace Kemgu.Sem.SmallStep
open Kemgu.Sem.Core

-- ============================================================
-- §1. Yardimci tanimlar
-- ============================================================

/-- Bir konfigurasyondaki belirli ID'li thread context'inin var oldugu. -/
def threadVar (S : Konfigurasyon) (t : ThreadId) (ctx : ThreadCtx) : Prop :=
  ctx ∈ S.thread ∧ ctx.tid = t

/-- t kimliğinde başka thread yok (fresh thread test). -/
def threadFresh (S : Konfigurasyon) (t : ThreadId) : Prop :=
  ∀ ctx ∈ S.thread, ctx.tid ≠ t


-- ============================================================
-- §2. Tek-adim reduksiyon — Step
-- Kaynak: Op.Sem §4 (tek thread S-* kurallari) + §5.4 (sistem C-* kurallari)
--
-- En azindan asagidaki 6 zorunlu kural (DRF lemma'lari icin):
--   sAtama, cGorevBaslat, cGorevBirlestir,
--   cKanalGonder, cKanalAl, cDondur
-- ============================================================

/-- Sistem kucuk-adim reduksiyon iliskisi.
    `Step S S'` = S konfigurasyonu bir adimda S'ye reduksiyon yapilabilir.

    Kurallar Op.Sem §4-5'in mekanize karsiliklari. Her constructor
    kagit kuralin: (a) hangi thread aktif, (b) hangi ifade reduksiyonu,
    (c) store/sahiplik/iz/zaman degisikliklerini taşır. -/
inductive Step : Konfigurasyon → Konfigurasyon → Prop where

  /-- S-ATAMA Tamam (Plan v2 Adim 1.2): atama Ok varyanti.
      Onkosul: t S.thread'de var, ifadesi `atama x (sabit v)`,
      hedef bolge frozen DEGIL (A3.0''), ve hedef bolge ctx'in sahipliginde
      (A3.0'''' — DRF Teorem 4' + T1 tam form).
      Etki: store'a `(k, v)` push, iz'e `memYaz`, zaman+1.

      Adim 1.2 NOT: Eski sAtama'nin preconditions'i (h_not_frozen, h_owner)
      KORUNDU — Plan v2 §4.2 "runtime guard reinterpretation". Adim 7
      discharge lemmalari bu guard'larin typed program icin saglandigini
      ispatlayacak.

      Pattern position: 13 (eski sAtama ile bit-bit ayni — cases icin
      rename ile yeterli). Adim 7'de h_fault := none eklenince 14 olur. -/
  | sAtamaTamam
      (S S' : Konfigurasyon)
      (ctx : ThreadCtx) (x : VarId) (v : Deger) (k : Konum)
      (h_in         : ctx ∈ S.thread)
      (h_ifade      : ctx.ifade = .atama x (.sabit v))
      (h_not_frozen : ¬ isFrozen S k.bolge)
      (h_owner      : sahiplikGet S.sahiplik (k.bolge, S.zaman)
                        = some (Sahip.thread ctx.tid))
      (h_store      : S'.store = (k, v) :: S.store)
      (h_iz         : S'.iz = .memYaz ctx.tid k v :: S.iz)
      (h_zaman      : S'.zaman = S.zaman + 1)
      (h_sahip      : S'.sahiplik = S.sahiplik)
      (h_kanal      : S'.kanal = S.kanal) :
      Step S S'

  /-- S-ATAMA Hata Donmus (Plan v2 Adim 1.2): frozen bolgeye yazma.
      Fault state'e gecis: S'.fault = some FaultSebep.donmusYazma.
      Etki: store/sahiplik/kanal/iz DEGISMEZ (fault non-observable),
      sadece S'.fault set.

      Pattern position: 8.

      Adim 7 NOT: typed program bu constructor'a ulasamaz (typing excludes
      frozen write) — discharge lemma `typing_excludes_sAtamaHataDonmus`
      ile exfalso. -/
  | sAtamaHataDonmus
      (S S' : Konfigurasyon)
      (ctx : ThreadCtx) (x : VarId) (v : Deger) (k : Konum)
      (h_in     : ctx ∈ S.thread)
      (h_ifade  : ctx.ifade = .atama x (.sabit v))
      (h_frozen : isFrozen S k.bolge)
      (h_fault  : S'.fault = some (FaultSebep.donmusYazma k.bolge))
      -- Plan v2 Adim 7 strengthen: "fault non-observable" (Plan §4.4) formel:
      -- store/iz/zaman/sahiplik/kanal DEGISMEZ. Bu Hata case'lerinin
      -- DRF lemma'larinda trivial kapanmasini saglar (Discharge gerekmez).
      (h_store  : S'.store = S.store)
      (h_iz     : S'.iz = S.iz)
      (h_zaman  : S'.zaman = S.zaman)
      (h_sahip  : S'.sahiplik = S.sahiplik)
      (h_kanal  : S'.kanal = S.kanal) :
      Step S S'

  /-- S-ATAMA Hata Sahip Degil (Plan v2 Adim 1.2): ctx hedef bolgenin
      sahibi degil. Fault: S'.fault = some FaultSebep.sahipDegil.

      DRF-L0 ihlal olur (iki thread potansiyel race) — Adim 7 typed
      program ispati exfalso (`typing_excludes_sAtamaHataSahipDegil`).

      Pattern position: 8. -/
  | sAtamaHataSahipDegil
      (S S' : Konfigurasyon)
      (ctx : ThreadCtx) (x : VarId) (v : Deger) (k : Konum)
      (h_in        : ctx ∈ S.thread)
      (h_ifade     : ctx.ifade = .atama x (.sabit v))
      (h_not_owner : sahiplikGet S.sahiplik (k.bolge, S.zaman)
                       ≠ some (Sahip.thread ctx.tid))
      (h_fault     : S'.fault = some (FaultSebep.sahipDegil k.bolge ctx.tid))
      -- Plan v2 Adim 7 strengthen: fault non-observable (Plan §4.4)
      (h_store     : S'.store = S.store)
      (h_iz        : S'.iz = S.iz)
      (h_zaman     : S'.zaman = S.zaman)
      (h_sahip     : S'.sahiplik = S.sahiplik)
      (h_kanal     : S'.kanal = S.kanal) :
      Step S S'

  /-- C-GOREV-BASLAT Tamam (Plan v2 Adim 1.3): cGorevBaslat Ok varyanti.
      yeni thread t_yeni spawn edilir; yakalama listesindeki tum
      bolgeler Sigma uzerinde t_yeni'ye transfer (sahiplikSetMany).
      DRF-L2 (Linear Move) bu kurali kullanir.
      `transferredBolgeler` yakalanan v_i'lerin bolgeleri.
      `linearYakalananlar` yakalama listesinin LINEAR alt-kumesi
      (Lineerlik takip eden) — A3.0''' refactor (DRF-L2 onkosulu).

      Pattern position: 16 (eski cGorevBaslat ile ayni — cases icin
      rename ile yeterli). -/
  | cGorevBaslatTamam
      (S S' : Konfigurasyon)
      (ctx : ThreadCtx) (tYeni : ThreadId)
      (yd : List VarId) (kod : Ifade)
      (transferredBolgeler : List Bolge)
      (linearYakalananlar : List VarId)
      (h_in             : ctx ∈ S.thread)
      (h_ifade          : ctx.ifade = .gorevBaslat yd kod)
      (h_fresh          : threadFresh S tYeni)
      (h_yeni_th        : ∃ yctx ∈ S'.thread, yctx.tid = tYeni ∧ yctx.ifade = kod)
      (h_sahip          : S'.sahiplik = sahiplikSetMany S.sahiplik
                            transferredBolgeler S.zaman (Sahip.thread tYeni))
      -- A3.0''' refactor (DRF-L2 onkosulu):
      -- IyiTipli'nin lineer-kontrol komponentinin kismi ifadesi.
      -- "Linear yakalananlar caller'da tuketildi" (kagit Lambda1' = Lambda1 \ {YD ∩ Linear}).
      -- Bu olmadan DRF-L2 "linear move = cross-thread no-alias" karsi-ornek ile bozulur.
      (h_lineer_caller  : ∃ ctx' ∈ S'.thread, ctx'.tid = ctx.tid ∧
                            ∀ v ∈ linearYakalananlar,
                              (v, Lineerlik.tuketildi) ∈ ctx'.lineer)
      (h_iz             : S'.iz = .threadBaslat tYeni :: S.iz)
      (h_zaman          : S'.zaman = S.zaman + 1)
      (h_store          : S'.store = S.store)
      (h_kanal          : S'.kanal = S.kanal) :
      Step S S'

  /-- C-GOREV-BASLAT Hata Lineer Ihlal (Plan v2 Adim 1.3): linear yakalanmis
      bir degisken caller'da tuketilmemis (vIhlal hala aktif). Fault:
      lineerCagiranTukenmedi. Pattern position: 8.

      Adim 7 NOT: typed program (LinearOK katmani) bu constructor'a
      ulasamaz — discharge lemma `typing_excludes_cGorevBaslatHataLineerIhlal`. -/
  | cGorevBaslatHataLineerIhlal
      (S S' : Konfigurasyon)
      (ctx : ThreadCtx) (tYeni : ThreadId)
      (yd : List VarId) (kod : Ifade)
      (vIhlal : VarId)
      (h_in     : ctx ∈ S.thread)
      (h_ifade  : ctx.ifade = .gorevBaslat yd kod)
      (h_vAktif : ∃ ctx' ∈ S.thread, ctx'.tid = ctx.tid ∧
                    (vIhlal, Lineerlik.aktif) ∈ ctx'.lineer)
      (h_fault  : S'.fault = some (FaultSebep.lineerCagiranTukenmedi vIhlal))
      -- Plan v2 Adim 7 strengthen: fault non-observable (Plan §4.4)
      (h_store  : S'.store = S.store)
      (h_iz     : S'.iz = S.iz)
      (h_zaman  : S'.zaman = S.zaman)
      (h_sahip  : S'.sahiplik = S.sahiplik)
      (h_kanal  : S'.kanal = S.kanal) :
      Step S S'

  /-- C-GOREV-BIRLESTIR Tamam (Plan v2 Adim 1.3): cGorevBirlestir Ok varyanti.
      birlestir(g) cagrisi; t_hedef bittiginde Pho_sahip bolgeleri serbest,
      donus degeri Pho_cagiran'a terfi. Iz'e threadBitir.
      `returnedBolgeler` t_hedef'in caller'a terfi eden bolgeleri.

      Plan v2 §4.5 NOT: cGorevBirlestir icin Hata varyanti YOK — Adim 1.3
      yalniz rename. Pattern position: 12. -/
  | cGorevBirlestirTamam
      (S S' : Konfigurasyon)
      (ctx : ThreadCtx) (g : VarId) (tHedef : ThreadId)
      (returnedBolgeler : List Bolge)
      (h_in        : ctx ∈ S.thread)
      (h_ifade     : ctx.ifade = .gorevBirlestir g)
      (h_hedef     : ∃ hctx ∈ S.thread, hctx.tid = tHedef ∧ hctx.ifade = .sabit .birim)
      (h_sahip     : S'.sahiplik = sahiplikSetMany S.sahiplik
                        returnedBolgeler S.zaman (Sahip.thread ctx.tid))
      (h_iz        : S'.iz = .threadBitir tHedef :: S.iz)
      (h_zaman     : S'.zaman = S.zaman + 1)
      (h_store     : S'.store = S.store)
      (h_kanal     : S'.kanal = S.kanal) :
      Step S S'

  /-- C-KANAL-GONDER Tamam (Plan v2 Adim 1.3): cKanalGonder Ok varyanti.
      Sigma' = Sigma[bolge(v) ↦ kanalSahip k] (S3 atomik transfer).
      `transferredBolge` v'nin bolgesi (bolge(v)).
      Pattern position: 12. -/
  | cKanalGonderTamam
      (S S' : Konfigurasyon)
      (ctx : ThreadCtx) (k : KanalId) (vId : VarId) (v : Deger)
      (transferredBolge : Bolge)
      (h_in        : ctx ∈ S.thread)
      (h_ifade     : ctx.ifade = .kanalGonderIf k vId)
      (h_kanal     : ∃ kd ∈ S'.kanal, kd.kid = k ∧ v ∈ kd.gonderKuyrugu)
      (h_sahip     : S'.sahiplik = sahiplikSet S.sahiplik
                        transferredBolge S.zaman (Sahip.kanalSahip k))
      (h_iz        : S'.iz = .kanalGonderOl ctx.tid k v :: S.iz)
      (h_zaman     : S'.zaman = S.zaman + 1)
      (h_store     : S'.store = S.store) :
      Step S S'

  /-- C-KANAL-GONDER Hata Lineer Tuket (Plan v2 Adim 1.3): linear v zaten
      tuketilmis (cifte gonderim). Fault: lineerKanalTuket.
      Pattern position: 7.

      Adim 7 NOT: typed program (LinearOK) cifte tuket eylemine ulasamaz —
      discharge lemma `typing_excludes_cKanalGonderHataLineerTuket`. -/
  | cKanalGonderHataLineerTuket
      (S S' : Konfigurasyon)
      (ctx : ThreadCtx) (k : KanalId) (vId : VarId)
      (h_in     : ctx ∈ S.thread)
      (h_ifade  : ctx.ifade = .kanalGonderIf k vId)
      (h_tuket  : (vId, Lineerlik.tuketildi) ∈ ctx.lineer)
      (h_fault  : S'.fault = some (FaultSebep.lineerKanalTuket vId))
      -- Plan v2 Adim 7 strengthen: fault non-observable (Plan §4.4)
      (h_store  : S'.store = S.store)
      (h_iz     : S'.iz = S.iz)
      (h_zaman  : S'.zaman = S.zaman)
      (h_sahip  : S'.sahiplik = S.sahiplik)
      (h_kanal  : S'.kanal = S.kanal) :
      Step S S'

  /-- C-KANAL-AL Tamam (Plan v2 Adim 1.3): cKanalAl Ok varyanti.
      Sigma' = Sigma[bolge(v) ↦ Sahip.thread t_alan]; v artik t'nin lineer
      ortamina geri eklenir. DRF-L5 atomicity bu kurali kullanir.
      `transferredBolge` v'nin bolgesi.

      Plan v2 §4.5 NOT: cKanalAl icin Hata varyanti YOK — alim icin
      precondition zayif. Pattern position: 11. -/
  | cKanalAlTamam
      (S S' : Konfigurasyon)
      (ctx : ThreadCtx) (k : KanalId) (v : Deger)
      (transferredBolge : Bolge)
      (h_in        : ctx ∈ S.thread)
      (h_ifade     : ctx.ifade = .kanalAlIf k)
      (h_kanal_var : ∃ kd ∈ S.kanal, kd.kid = k ∧ v ∈ kd.gonderKuyrugu)
      (h_sahip     : S'.sahiplik = sahiplikSet S.sahiplik
                        transferredBolge S.zaman (Sahip.thread ctx.tid))
      (h_iz        : S'.iz = .kanalAlOl ctx.tid k v :: S.iz)
      (h_zaman     : S'.zaman = S.zaman + 1)
      (h_store     : S'.store = S.store) :
      Step S S'

  /-- C-DONDUR Tamam (Plan v2 Adim 1.3): cDondur Ok varyanti.
      Sigma' = Sigma[bolge(v) ↦ DONMUS] @ S.zaman.
      DRF-L4 frozen region read-soundness bu kurali kullanir.
      Refactor (A3.0'): once sadece "entry exists in S'.sahiplik" diyordu;
      simdi tam atomic set semantigi (sahiplikSet) — diger entry'ler
      degismez kalir, S1 preservation icin gerekli.
      Pattern position: 9. -/
  | cDondurTamam
      (S S' : Konfigurasyon)
      (ctx : ThreadCtx) (b : Bolge)
      (h_in        : ctx ∈ S.thread)
      (h_ifade     : ctx.ifade = .dondurIf b)
      (h_sahip     : S'.sahiplik = sahiplikSet S.sahiplik b S.zaman Sahip.donmus)
      (h_iz        : S'.iz = .dondurOl ctx.tid b :: S.iz)
      (h_zaman     : S'.zaman = S.zaman + 1)
      (h_store     : S'.store = S.store)
      (h_kanal     : S'.kanal = S.kanal) :
      Step S S'

  /-- C-DONDUR Hata Zaten Donmus (Plan v2 Adim 1.3): bolge zaten frozen,
      cifte dondur cagrisi. Fault: zatenDonmus. Pattern position: 6.

      Adim 7 NOT: typed program once dondurmaz iki kez — discharge
      lemma `typing_excludes_cDondurHataZatenDonmus`. -/
  | cDondurHataZatenDonmus
      (S S' : Konfigurasyon)
      (ctx : ThreadCtx) (b : Bolge)
      (h_in     : ctx ∈ S.thread)
      (h_ifade  : ctx.ifade = .dondurIf b)
      (h_zaten  : isFrozen S b)
      (h_fault  : S'.fault = some (FaultSebep.zatenDonmus b))
      -- Plan v2 Adim 7 strengthen: fault non-observable (Plan §4.4)
      (h_store  : S'.store = S.store)
      (h_iz     : S'.iz = S.iz)
      (h_zaman  : S'.zaman = S.zaman)
      (h_sahip  : S'.sahiplik = S.sahiplik)
      (h_kanal  : S'.kanal = S.kanal) :
      Step S S'

  /-- S-LIN-KULLAN Tamam (Plan v2 Adim 1.3): Linear consume Ok varyanti.
      Lambda(x) = aktif → Lambda' = Lambda[x ↦ tuketildi].
      Pattern position: 9. -/
  | sLinKullanTamam
      (S S' : Konfigurasyon)
      (ctx : ThreadCtx) (x : VarId)
      (h_in        : ctx ∈ S.thread)
      (h_ifade     : ctx.ifade = .kullanIf x)
      (h_aktif     : (x, Lineerlik.aktif) ∈ ctx.lineer)
      (h_iz        : S'.iz = S.iz)            -- mem_op degil, gozlemlenebilir olay yok
      (h_zaman     : S'.zaman = S.zaman + 1)
      (h_store     : S'.store = S.store)
      (h_kanal     : S'.kanal = S.kanal) :
      Step S S'

  /-- S-LIN-KULLAN Hata Zaten Tuketildi (Plan v2 Adim 1.3): linear x zaten
      consume edildi. Fault: lineerZatenTuketildi. Pattern position: 6.

      Adim 7 NOT: typed program (LinearOK) zaten tuketilmis kullanim'a
      ulasamaz — discharge lemma `typing_excludes_sLinKullanHataZatenTuketildi`. -/
  | sLinKullanHataZatenTuketildi
      (S S' : Konfigurasyon)
      (ctx : ThreadCtx) (x : VarId)
      (h_in     : ctx ∈ S.thread)
      (h_ifade  : ctx.ifade = .kullanIf x)
      (h_tuket  : (x, Lineerlik.tuketildi) ∈ ctx.lineer)
      (h_fault  : S'.fault = some (FaultSebep.lineerZatenTuketildi x))
      -- Plan v2 Adim 7 strengthen: fault non-observable (Plan §4.4)
      (h_store  : S'.store = S.store)
      (h_iz     : S'.iz = S.iz)
      (h_zaman  : S'.zaman = S.zaman)
      (h_sahip  : S'.sahiplik = S.sahiplik)
      (h_kanal  : S'.kanal = S.kanal) :
      Step S S'

  /-- S-LIN-IMHA Tamam (Plan v2 Adim 1.3): Linear imha (silinme) Ok varyanti.
      Lambda(x) = aktif → Lambda' = Lambda[x ↦ tuketildi].
      Pattern position: 9. -/
  | sLinImhaTamam
      (S S' : Konfigurasyon)
      (ctx : ThreadCtx) (x : VarId)
      (h_in        : ctx ∈ S.thread)
      (h_ifade     : ctx.ifade = .imhaIf x)
      (h_aktif     : (x, Lineerlik.aktif) ∈ ctx.lineer)
      (h_iz        : S'.iz = S.iz)
      (h_zaman     : S'.zaman = S.zaman + 1)
      (h_store     : S'.store = S.store)
      (h_kanal     : S'.kanal = S.kanal) :
      Step S S'

  /-- S-LIN-IMHA Hata Zaten Tuketildi (Plan v2 Adim 1.3): linear x zaten
      consume edildi (imha sonrasi). Fault: lineerZatenTuketildi (paylasilir).
      Pattern position: 6.

      Adim 7 NOT: typed program zaten tuketilmis imha'ya ulasamaz —
      discharge lemma `typing_excludes_sLinImhaHataZatenTuketildi`. -/
  | sLinImhaHataZatenTuketildi
      (S S' : Konfigurasyon)
      (ctx : ThreadCtx) (x : VarId)
      (h_in     : ctx ∈ S.thread)
      (h_ifade  : ctx.ifade = .imhaIf x)
      (h_tuket  : (x, Lineerlik.tuketildi) ∈ ctx.lineer)
      (h_fault  : S'.fault = some (FaultSebep.lineerZatenTuketildi x))
      -- Plan v2 Adim 7 strengthen: fault non-observable (Plan §4.4)
      (h_store  : S'.store = S.store)
      (h_iz     : S'.iz = S.iz)
      (h_zaman  : S'.zaman = S.zaman)
      (h_sahip  : S'.sahiplik = S.sahiplik)
      (h_kanal  : S'.kanal = S.kanal) :
      Step S S'


-- ============================================================
-- §3. Coklu-adim reduksiyon — StepStar (refleksif gecisli kapanis)
-- Kaynak: Op.Sem Tr(Pi) = ⟹* tum izler kumesi
-- ============================================================

/-- Coklu-adim reduksiyon: refleksif gecisli kapanis (⟹*). -/
inductive StepStar : Konfigurasyon → Konfigurasyon → Prop where
  | refl (S : Konfigurasyon) : StepStar S S
  | step (S S1 S' : Konfigurasyon) (h1 : Step S S1) (h2 : StepStar S1 S') :
      StepStar S S'


-- ============================================================
-- §4. Temel teknik lemmalar
-- ============================================================

/-- Tek adim → coklu adim refleksif kapanis. -/
theorem step_to_starStep (S S' : Konfigurasyon) (h : Step S S') :
    StepStar S S' :=
  StepStar.step S S' S' h (StepStar.refl S')

/-- Coklu adim gecisli (transitivity).
    Ispat: indüksiyon h1 uzerine. -/
theorem stepStar_trans (S S1 S2 : Konfigurasyon)
    (h1 : StepStar S S1) (h2 : StepStar S1 S2) : StepStar S S2 := by
  induction h1 with
  | refl _ => exact h2
  | step S0 Smid Send hStep _ ih =>
      exact StepStar.step S0 Smid S2 hStep (ih h2)


end Kemgu.Sem.SmallStep
