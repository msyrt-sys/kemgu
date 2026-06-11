/-
KEMGU DRF Mekanize — DRF-L3 Linear Closure Soundness (Onarim v3 F2)
Kaynak (kagit formel): belgeler/KEMGU_DRF_Lemmalar.md §DRF-L3
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz, sorry/axiom YOK

F2: cGorevBaslatTamam'in fonksiyonel post-state'inden 3 yapisal iddia
DOGRUDAN turetilir (eski form hipotez-paketlemeydi; simdi h_S' tek
esitliginden cikarim var). Kagit (a) "body only accesses certain regions"
iddiasi F3 (gercek TipKontrolOk) + F6 degerlendirmesine kaldi.
-/

import Kemgu.Sem.Core
import Kemgu.Sem.StateTipli
import Kemgu.Sem.SmallStep
import Kemgu.Drf.L2LinearMoveCrossThread

namespace Kemgu.Drf.L3LinearClosureSoundness
open Kemgu.Sem.Core Kemgu.Sem.StateTipli Kemgu.Sem.SmallStep
     Kemgu.Drf.L2LinearMoveCrossThread

/-- DRF-L3 — Linear Closure Soundness (F2 bundled form):
    cGorevBaslatTamam sonrasi
    (1) yeni thread tYeni, kod govdesiyle S'.thread'de;
    (2) yakalanan bolgeler tYeni'nin sahipliginde;
    (3) yakalanan aktif lineer'lar caller'da tuketildi. -/
theorem drf_l3_linear_closure_soundness
    (S S' : Konfigurasyon) (ts1 ts2 : List ThreadCtx) (ctx : ThreadCtx)
    (tYeni : ThreadId) (yd : List VarId) (kod : Ifade)
    (h_t : S.thread = ts1 ++ ctx :: ts2)
    (h_S' : S' = { S with
        thread := ts1 ++ { ctx with
                    ifade  := .sabit (.gorevVal tYeni),
                    lineer := lineerTuketListe ctx.lineer yd } :: ts2
                  ++ [⟨tYeni, kod, yd.map (fun v => (v, Lineerlik.aktif))⟩],
        sahiplik := sahiplikSetMany S.sahiplik
                      (bolgeleriTopla S.bolge yd) (Sahip.thread tYeni),
        bolge  := bolgeOrtamSahipAta S.bolge yd tYeni,
        iz     := .threadBaslat tYeni :: S.iz,
        zaman  := S.zaman + 1,
        fault  := none }) :
    -- (1) Yeni thread varligi (govde = kod)
    (∃ yctx ∈ S'.thread, yctx.tid = tYeni ∧ yctx.ifade = kod)
    ∧
    -- (2) Yakalanan bolgeler tYeni'ye transfer (sahiplik lookup)
    (∀ b ∈ bolgeleriTopla S.bolge yd,
       sahiplikGet S'.sahiplik b = some (Sahip.thread tYeni))
    ∧
    -- (3) Yakalanan aktif lineer'lar caller'da tuketildi
    (∀ v ∈ yd, lineerOrtamGet ctx.lineer v = some Lineerlik.aktif →
       ∃ ctx' ∈ S'.thread, ctx'.tid = ctx.tid ∧
         lineerOrtamGet ctx'.lineer v = some Lineerlik.tuketildi) := by
  refine ⟨?_, ?_, ?_⟩
  · -- (1)
    subst h_S'
    refine ⟨⟨tYeni, kod, yd.map (fun v => (v, Lineerlik.aktif))⟩, ?_, rfl, rfl⟩
    exact List.mem_append.mpr (Or.inr (List.Mem.head _))
  · -- (2): setMany sonrasi her hedef bolge yeni sahibi gosterir
    subst h_S'
    intro b h_b
    show sahiplikGet (sahiplikSetMany S.sahiplik
           (bolgeleriTopla S.bolge yd) (Sahip.thread tYeni)) b
         = some (Sahip.thread tYeni)
    -- setMany-uyelik lemmasi: b listede → lookup yeni deger
    exact sahiplikSetMany_mem _ _ _ h_b
  · -- (3): L2 konfigurasyon formu
    intro v h_v h_aktif
    exact drf_l2_step_cGorevBaslat S S' ts1 ts2 ctx tYeni yd kod h_t h_S'
      v h_v h_aktif

end Kemgu.Drf.L3LinearClosureSoundness
