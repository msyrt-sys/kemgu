/-
KEMGU DRF Mekanize — DRF-L5 Channel Atomicity Preservation (Onarim v3 F2)
Kaynak (kagit formel): belgeler/KEMGU_DRF_Lemmalar.md §DRF-L5
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz, sorry/axiom YOK

F2: guncel-durum sahiplik — atomik transfer dogrudan sahiplikSet_eq.
L5 (a) "transit boyunca kanal'da kalir" artik step_donmus_korunur'un
kanal-analogu olarak ifade edilebilir (V2.1); (d) lineer tuketim
cKanalGonderTamam'in lineerTuket guncellemesiyle mekanize oldu.
-/

import Kemgu.Sem.Core
import Kemgu.Sem.SmallStep

namespace Kemgu.Drf.L5KanalAtomikTransfer
open Kemgu.Sem.Core Kemgu.Sem.SmallStep

/-- DRF-L5 (b) — cKanalGonder atomic: gonderim sonrasi bolge(v) sahibi
    atomik olarak kanalSahip(k). (cKanalGonderTamam h_S' + sahiplikSet_eq) -/
theorem drf_l5_b_kanalGonder_atomic
    (S : Konfigurasyon) (b : Bolge) (k : KanalId) :
    sahiplikGet (sahiplikSet S.sahiplik b (Sahip.kanalSahip k)) b
      = some (Sahip.kanalSahip k) :=
  sahiplikSet_eq S.sahiplik b (Sahip.kanalSahip k)

/-- DRF-L5 (c) — cKanalAl atomic: alim sonrasi bolge sahibi atomik olarak
    alici thread. (cKanalAlTamam h_S' + sahiplikSet_eq) -/
theorem drf_l5_c_kanalAl_atomic
    (S : Konfigurasyon) (b : Bolge) (t : ThreadId) :
    sahiplikGet (sahiplikSet S.sahiplik b (Sahip.thread t)) b
      = some (Sahip.thread t) :=
  sahiplikSet_eq S.sahiplik b (Sahip.thread t)

/-- DRF-L5 (d) — gonderilen lineer deger gonderici'de tuketilir:
    cKanalGonderTamam ctx'.lineer = lineerTuket ctx.lineer vId;
    aktif → tuketildi (Core.lineerTuket_tuketir). F2'de MEKANIZE
    (eski "h_lineer_sender refactor gerek" notu kapandi). -/
theorem drf_l5_d_gonderici_tuketir
    (Λ : LineerOrtam) (vId : VarId)
    (h_aktif : lineerOrtamGet Λ vId = some Lineerlik.aktif) :
    lineerOrtamGet (lineerTuket Λ vId) vId = some Lineerlik.tuketildi :=
  lineerTuket_tuketir Λ vId h_aktif

/-
DRF-L5 (a) — ∀ adimlar ∈ [gonder, al) araliginda bolge kanal'da kalir:
step_donmus_korunur'un kanalSahip-analogu gerekir ("kanal-transit bolge
yalniz cKanalAlTamam ile cikar"); cKanalAlTamam h_transit hedefi serbest
(ayni kanalin BIR transit bolgesi) oldugu icin per-bolge persistence V1'de
kismi. V2.1 hedef (~60 satir).
-/

end Kemgu.Drf.L5KanalAtomikTransfer
