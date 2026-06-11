/-
KEMGU DRF Mekanize — DRF-L2 Linear Move Cross-Thread (Onarim v3 F2)
Kaynak (kagit formel): belgeler/KEMGU_DRF_Lemmalar.md §DRF-L2
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz, sorry/axiom YOK

F2: eski form, cGorevBaslat'in (artik var olmayan) h_lineer_caller
hipotezini yeniden paketliyordu (icerik ≈ 0). Yeni form GERCEK ICERIKLI:
cGorevBaslatTamam'in fonksiyonel post-state'i caller'in lineer ortamini
`lineerTuketListe ctx.lineer yd` yapar; Core.lineerTuketListe_tuketir
lemmasi yakalanan aktif lineerlerin TUKETILDIGINI ispatlar.
-/

import Kemgu.Sem.Core
import Kemgu.Sem.StateTipli
import Kemgu.Sem.SmallStep

namespace Kemgu.Drf.L2LinearMoveCrossThread
open Kemgu.Sem.Core Kemgu.Sem.StateTipli Kemgu.Sem.SmallStep

/-- DRF-L2 — Linear Move Consumption (cekirdek, ortam-seviyesi):
    yakalama listesindeki aktif lineer baglama, yakalama tuketimi sonrasi
    TUKETILDI olur. (Core.lineerTuketListe_tuketir'in DRF-baglamli adi.) -/
theorem drf_l2_linear_move_consumed
    (Λ : LineerOrtam) (yd : List VarId) (v : VarId)
    (h_v : v ∈ yd)
    (h_aktif : lineerOrtamGet Λ v = some Lineerlik.aktif) :
    lineerOrtamGet (lineerTuketListe Λ yd) v = some Lineerlik.tuketildi :=
  lineerTuketListe_tuketir yd Λ v h_v h_aktif

/-- DRF-L2 (konfigurasyon formu): cGorevBaslatTamam adimi sonrasi caller
    thread'in lineer ortaminda yakalanan aktif v TUKETILDI. -/
theorem drf_l2_step_cGorevBaslat
    (S S' : Konfigurasyon) (ts1 ts2 : List ThreadCtx) (ctx : ThreadCtx)
    (tYeni : ThreadId) (yd : List VarId) (kod : Ifade)
    (_h_t : S.thread = ts1 ++ ctx :: ts2)
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
        fault  := none })
    (v : VarId) (h_v : v ∈ yd)
    (h_aktif : lineerOrtamGet ctx.lineer v = some Lineerlik.aktif) :
    ∃ ctx' ∈ S'.thread, ctx'.tid = ctx.tid ∧
      lineerOrtamGet ctx'.lineer v = some Lineerlik.tuketildi := by
  subst h_S'
  refine ⟨{ ctx with
            ifade  := .sabit (.gorevVal tYeni),
            lineer := lineerTuketListe ctx.lineer yd }, ?_, rfl, ?_⟩
  · exact List.mem_append.mpr (Or.inl
      (List.mem_append.mpr (Or.inr (List.Mem.head _))))
  · exact lineerTuketListe_tuketir yd ctx.lineer v h_v h_aktif

end Kemgu.Drf.L2LinearMoveCrossThread
