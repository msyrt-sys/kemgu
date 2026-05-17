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

  /-- S-ATAMA (Op.Sem §4.2): bir thread'in atama ifadesi.
      Onkosul: t S.thread'de var ve ifadesi `atama x (sabit v)`.
      Etki: store'a `(k, v)` push, iz'e `memYaz`, zaman+1. -/
  | sAtama
      (S S' : Konfigurasyon)
      (ctx : ThreadCtx) (x : VarId) (v : Deger) (k : Konum)
      (h_in    : ctx ∈ S.thread)
      (h_ifade : ctx.ifade = .atama x (.sabit v))
      (h_store : S'.store = (k, v) :: S.store)
      (h_iz    : S'.iz = .memYaz ctx.tid k v :: S.iz)
      (h_zaman : S'.zaman = S.zaman + 1)
      (h_sahip : S'.sahiplik = S.sahiplik)
      (h_kanal : S'.kanal = S.kanal) :
      Step S S'

  /-- C-GOREV-BASLAT (Op.Sem §5.4 R-GOREV uygulamasi):
      yeni thread t_yeni spawn edilir; yakalama listesindeki tum
      bolgeler Sigma uzerinde t_yeni'ye transfer (sahiplikSetMany).
      DRF-L2 (Linear Move) bu kurali kullanir.
      `transferredBolgeler` yakalanan v_i'lerin bolgeleri (yd → bolge
      eslesmesi Pho_t'den gelir — V1'de implicit). -/
  | cGorevBaslat
      (S S' : Konfigurasyon)
      (ctx : ThreadCtx) (tYeni : ThreadId)
      (yd : List VarId) (kod : Ifade)
      (transferredBolgeler : List Bolge)
      (h_in        : ctx ∈ S.thread)
      (h_ifade     : ctx.ifade = .gorevBaslat yd kod)
      (h_fresh     : threadFresh S tYeni)
      (h_yeni_th   : ∃ yctx ∈ S'.thread, yctx.tid = tYeni ∧ yctx.ifade = kod)
      (h_sahip     : S'.sahiplik = sahiplikSetMany S.sahiplik
                        transferredBolgeler S.zaman (Sahip.thread tYeni))
      (h_iz        : S'.iz = .threadBaslat tYeni :: S.iz)
      (h_zaman     : S'.zaman = S.zaman + 1)
      (h_store     : S'.store = S.store)
      (h_kanal     : S'.kanal = S.kanal) :
      Step S S'

  /-- C-GOREV-BIRLESTIR (Op.Sem §5.4 R-BIRLESTIR):
      birlestir(g) cagrisi; t_hedef bittiginde Pho_sahip bolgeleri serbest,
      donus degeri Pho_cagiran'a terfi. Iz'e threadBitir.
      `returnedBolgeler` t_hedef'in caller'a terfi eden bolgeleri. -/
  | cGorevBirlestir
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

  /-- C-KANAL-GONDER (Op.Sem §5.4):
      Sigma' = Sigma[bolge(v) ↦ kanalSahip k] (S3 atomik transfer).
      `transferredBolge` v'nin bolgesi (bolge(v)). -/
  | cKanalGonder
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

  /-- C-KANAL-AL (Op.Sem §5.4):
      Sigma' = Sigma[bolge(v) ↦ Sahip.thread t_alan]; v artik t'nin lineer
      ortamina geri eklenir. DRF-L5 atomicity bu kurali kullanir.
      `transferredBolge` v'nin bolgesi. -/
  | cKanalAl
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

  /-- C-DONDUR (Op.Sem §5.4 R-PAYLAS):
      Sigma' = Sigma[bolge(v) ↦ DONMUS] @ S.zaman.
      DRF-L4 frozen region read-soundness bu kurali kullanir.
      Refactor (A3.0'): once sadece "entry exists in S'.sahiplik" diyordu;
      simdi tam atomic set semantigi (sahiplikSet) — diger entry'ler
      degismez kalir, S1 preservation icin gerekli. -/
  | cDondur
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

  /-- S-LIN-KULLAN (Op.Sem §4.2): Linear consume.
      Lambda(x) = aktif → Lambda' = Lambda[x ↦ tuketildi]. -/
  | sLinKullan
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

  /-- S-LIN-IMHA (Op.Sem §4.2): Linear imha (silinme).
      Lambda(x) = aktif → Lambda' = Lambda[x ↦ tuketildi]. -/
  | sLinImha
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
