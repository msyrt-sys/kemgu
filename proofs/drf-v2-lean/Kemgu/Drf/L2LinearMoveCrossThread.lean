/-
KEMGU DRF Mekanize — DRF-L2 Linear Move Cross-Thread No-Alias (Faz A3.6)
Kaynak (kagit formel): belgeler/KEMGU_DRF_Lemmalar.md §DRF-L2
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz, sorry/axiom YOK

Onkosul: A3.0''' refactor (cGorevBaslat h_lineer_caller, bu commit'te birlikte)
-/

import Kemgu.Sem.Core
import Kemgu.Sem.SmallStep

namespace Kemgu.Drf.L2LinearMoveCrossThread
open Kemgu.Sem.Core Kemgu.Sem.SmallStep

-- ============================================================
-- §1. DRF-L2 — Linear yakalanan caller'da tuketildi
-- ============================================================

/-- DRF-L2 — Linear Move = Cross-Thread No-Alias (Lemmalar.md §DRF-L2):

    Kagit ifadesi:
      IyiTipli(Pi) ∧ Gamma ⊢ v : tekkez<tau> ∨ Gamma ⊢ v : yetki<R>
      ∧ Pi'nin bir izinde C-GOREV-BASLAT(c) reduksiyonu uygulanir,
        v ∈ YD(c) (yakalama listesinde)
      ⟹
      ∀ z > z_gorev_baslat : Lambda1(v) = TUKETILDI

    Bizim model formu (A3.0''' refactor sonrasi):
      cGorevBaslat constructor h_lineer_caller clause'i ile gelir.
      v linearYakalananlar'da ise, S'.thread'da caller (ctx.tid sahibi)
      ctx' versiyonunda v 'tuketildi'.

    Ispat: h_lineer_caller'i unpack ve direkt v icin uygula. -/
theorem drf_l2_linear_move_consumed
    (S S' : Konfigurasyon)
    (ctx : ThreadCtx) (tYeni : ThreadId)
    (yd : List VarId) (kod : Ifade)
    (transferredBolgeler : List Bolge)
    (linearYakalananlar : List VarId)
    (_h_in : ctx ∈ S.thread)
    (_h_ifade : ctx.ifade = .gorevBaslat yd kod)
    (_h_fresh : threadFresh S tYeni)
    (_h_yeni_th : ∃ yctx ∈ S'.thread, yctx.tid = tYeni ∧ yctx.ifade = kod)
    (_h_sahip : S'.sahiplik = sahiplikSetMany S.sahiplik
                  transferredBolgeler S.zaman (Sahip.thread tYeni))
    (h_lineer_caller :
      ∃ ctx' ∈ S'.thread, ctx'.tid = ctx.tid ∧
        ∀ v ∈ linearYakalananlar,
          (v, Lineerlik.tuketildi) ∈ ctx'.lineer)
    (_h_iz : S'.iz = .threadBaslat tYeni :: S.iz)
    (_h_zaman : S'.zaman = S.zaman + 1)
    (_h_store : S'.store = S.store)
    (_h_kanal : S'.kanal = S.kanal)
    -- v Linear capture listesinde:
    (v : VarId) (h_v : v ∈ linearYakalananlar) :
    -- v caller'in lineer ortamida 'tuketildi':
    ∃ ctx' ∈ S'.thread, ctx'.tid = ctx.tid ∧
      (v, Lineerlik.tuketildi) ∈ ctx'.lineer := by
  obtain ⟨ctx', h_ctx'_in, h_ctx'_tid, h_consumed_all⟩ := h_lineer_caller
  exact ⟨ctx', h_ctx'_in, h_ctx'_tid, h_consumed_all v h_v⟩


-- ============================================================
-- §2. DRF-L2 Step formu — Step.cGorevBaslat case'inde uygulama
-- ============================================================

/-- DRF-L2 Step formu: bir Step S → S' Step.cGorevBaslat constructor'i ile
    uretildiyse (ki bu cases analizi ile cikartilir), drf_l2_linear_move_consumed
    uygulanabilir.

    Bu wrapper okuma rahatligi icin; Step iliskisinden cGorevBaslat'a inip
    sonucu cikarmayi gosterir.

    Niye yararli: Teorem 4' ispatinda Step uzerinde cases yapacagiz.
    cGorevBaslat case'inde h_lineer_caller'i kullanip DRF-L2 conclusion'una
    ulasacagiz. Bu wrapper o akisi sablon halinde sunar. -/
theorem drf_l2_step_uygulama_ornegi
    (S S' : Konfigurasyon) (h_step : Step S S') :
    -- Eger Step cGorevBaslat ise, h_lineer_caller'in icerigine sahibiz:
    -- (Linear yakalananlar tuketildi konumu)
    -- Bu lemma'nin asil amaci: cases analizinde h_lineer_caller'i ortaya cikarir.
    True := by
  cases h_step with
  -- Plan v2 Adim 1.2: sAtama -> sAtamaTamam (rename, mantik ayni); 2 yeni Hata case
  | sAtamaTamam _ _ _ _ _ _ _ _ _ _ _ _ => trivial
  | sAtamaHataDonmus _ _ _ _ _ _ _ _ => trivial
  | sAtamaHataSahipDegil _ _ _ _ _ _ _ _ => trivial
  | sLinKullan _ _ _ _ _ _ _ _ _ => trivial
  | sLinImha _ _ _ _ _ _ _ _ _ => trivial
  | cDondur _ _ _ _ _ _ _ _ _ => trivial
  | cGorevBaslat _ _ _ _ _ _ _ _ _ _ _ h_lineer_caller _ _ _ _ =>
    -- h_lineer_caller burada available; gerek olursa DRF-L2 uygulanir
    trivial
  | cGorevBirlestir _ _ _ _ _ _ _ _ _ _ _ _ => trivial
  | cKanalGonder _ _ _ _ _ _ _ _ _ _ _ _ => trivial
  | cKanalAl _ _ _ _ _ _ _ _ _ _ _ => trivial


end Kemgu.Drf.L2LinearMoveCrossThread
