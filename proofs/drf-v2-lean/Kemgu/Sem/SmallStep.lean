/-
KEMGU DRF Mekanize — Operasyonel Semantik Kucuk-Adim Reduksiyon (Onarim v3 F2)
Kaynak (kagit formel): belgeler/KEMGU_Operasyonel_Semantik.md §4-5
            + ADIM0_DENETIM_RAPORU.md §2.1(d)/§2.2 + FAZ_BRIFINGLERI.md F2
Politika: ASCII identifier, Turkce yorum, mathlib bagimsiz

F2 TASARIMI (Mehmet onayli, 2026-06-11):
1. TAM-BELIRLENMIS POST-STATE: her kural tek `h_S' : S' = { S with ... }`
   esitligiyle kapanir — S'.thread/S'.bolge serbestligi (preservation'i
   ifade-yanlis yapan kusur) kalkti. `h_no_fault_target` ayri hipotez
   olmaktan cikti (esitligin `fault := none` parcasi — Sorun 2(a) kapandi).
2. IFADE ILERLETME: thread cerrahisi `S.thread = ts1 ++ ctx :: ts2` →
   `S'.thread = ts1 ++ ctx' :: ts2` — redex sonuc DEGERINE ilerler.
3. GUNCEL-DURUM SAHIPLIK (Sorun 3): sahiplikGet artik Bolge-anahtarli;
   sahiplik yazan kurallar hedefin guncel sahibini sart kosar (frozen
   transfer otomatik imkansiz — lookup fonksiyonel: thread ≠ donmus).
4. CONGRUENCE: sSeqCong / sAtamaCong / sGuvensizCong (Step oz-yinelemeli
   premise) + sSeqAtla / sGuvensizAtla yapısal kurallar.
5. sVarOku YENI: degisken okuma artik memOku olayi uretir (L4(b) kapsami).
   Offset-0 konvansiyonu: degisken x'in konumu ⟨bolge(x), 0⟩ (V1).
6. Lineer hipotezler lookup-formda (lineerOrtamGet ctx.lineer x = some l)
   — uyelik formunun golgeleme belirsizligi kalkti.
-/

import Kemgu.Sem.Core
import Kemgu.Sem.StateTipli

namespace Kemgu.Sem.SmallStep
open Kemgu.Sem.Core Kemgu.Sem.StateTipli

-- ============================================================
-- §1. Yardimci tanimlar
-- ============================================================

/-- Bir konfigurasyondaki belirli ID'li thread context'inin var oldugu. -/
def threadVar (S : Konfigurasyon) (t : ThreadId) (ctx : ThreadCtx) : Prop :=
  ctx ∈ S.thread ∧ ctx.tid = t

/-- t kimliğinde başka thread yok (fresh thread test). -/
def threadFresh (S : Konfigurasyon) (t : ThreadId) : Prop :=
  ∀ ctx ∈ S.thread, ctx.tid ≠ t

/-- Odakli thread'in ifadesini degistir (congruence kurallari icin):
    ayni konfigurasyonun, odaktaki thread'in ifadesi `e` olan kopyasi. -/
def ifadeyleKonf (S : Konfigurasyon) (ts1 ts2 : List ThreadCtx)
    (ctx : ThreadCtx) (e : Ifade) : Konfigurasyon :=
  { S with thread := ts1 ++ { ctx with ifade := e } :: ts2 }

/-- Taze thread kimligi: mevcutlarin maksimumundan buyuk (F5 progress
    taniki — cGorevBaslatTamam'in h_fresh'i icin). -/
def tazeTid (S : Konfigurasyon) : ThreadId :=
  (S.thread.foldl (fun acc ctx => max acc ctx.tid) 0) + 1

/-- foldl-max tohumu asla kucultmez. -/
theorem foldl_max_ge_acc (l : List ThreadCtx) (acc : Nat) :
    acc ≤ l.foldl (fun a c => max a c.tid) acc := by
  induction l generalizing acc with
  | nil => exact Nat.le_refl acc
  | cons c rest ih =>
      exact Nat.le_trans (Nat.le_max_left acc c.tid) (ih (max acc c.tid))

/-- Listedeki her tid foldl-max'tan kucuk-esit. -/
theorem foldl_max_ge_mem (l : List ThreadCtx) (acc : Nat) (ctx : ThreadCtx)
    (h : ctx ∈ l) :
    ctx.tid ≤ l.foldl (fun a c => max a c.tid) acc := by
  induction l generalizing acc with
  | nil => cases h
  | cons c rest ih =>
      rcases List.mem_cons.mp h with h_eq | h_tail
      · subst h_eq
        exact Nat.le_trans (Nat.le_max_right acc ctx.tid)
          (foldl_max_ge_acc rest (max acc ctx.tid))
      · exact ih (max acc c.tid) h_tail

/-- tazeTid gercekten taze. -/
theorem tazeTid_fresh (S : Konfigurasyon) : threadFresh S (tazeTid S) := by
  intro ctx h_ctx h_eq
  have h_le := foldl_max_ge_mem S.thread 0 ctx h_ctx
  rw [h_eq] at h_le
  exact Nat.not_succ_le_self _ h_le


-- ============================================================
-- §2. Tek-adim reduksiyon — Step (21 kural)
-- Kaynak: Op.Sem §4 (S-*) + §5.4 (C-*) — F2 tam-belirlenmis form
-- ============================================================

/-- Sistem kucuk-adim reduksiyon iliskisi (F2).
    `Step S S'` = S konfigurasyonu bir adimda S''ye gecebilir.

    Kural gruplari:
    - Okuma/yazma: sVarOku, sAtama{Tamam,HataDonmus,HataSahipDegil,Cong}
    - Yapisal: sSeqAtla, sSeqCong, sGuvensizAtla, sGuvensizCong
    - Concurrency: cGorevBaslat{Tamam,HataLineerIhlal}, cGorevBirlestirTamam,
      cKanalGonder{Tamam,HataLineerTuket}, cKanalAlTamam
    - Bolge: cDondur{Tamam,HataZatenDonmus}
    - Lineer: sLinKullan{Tamam,Hata...}, sLinImha{Tamam,Hata...} -/
inductive Step : Konfigurasyon → Konfigurasyon → Prop where

  /-- S-VAR-OKU (F2 YENI): degisken okuma. x'in konumundaki deger okunur,
      ifade o degere ilerler, memOku olayi emit edilir. -/
  | sVarOku
      (S S' : Konfigurasyon) (ts1 ts2 : List ThreadCtx) (ctx : ThreadCtx)
      (x : VarId) (b : Bolge) (v : Deger)
      (h_t  : S.thread = ts1 ++ ctx :: ts2)
      (h_if : ctx.ifade = .tanim x)
      (h_b  : bolgeOrtamGet S.bolge x = some b)
      (h_v  : konumGet S.store ⟨b, 0⟩ = some v)
      (h_S' : S' = { S with
                thread := ts1 ++ { ctx with ifade := .sabit v } :: ts2,
                iz     := .memOku ctx.tid ⟨b, 0⟩ v :: S.iz,
                zaman  := S.zaman + 1,
                fault  := none }) :
      Step S S'

  /-- S-ATAMA Tamam: hedef bolgenin GUNCEL sahibi ctx — yazma yapilir.
      h_owner, frozen-yazmayi otomatik dislar (thread ≠ donmus).
      h_b: yazilan konum x'in bolgesine baglidir (Aile 2 linkage). -/
  | sAtamaTamam
      (S S' : Konfigurasyon) (ts1 ts2 : List ThreadCtx) (ctx : ThreadCtx)
      (x : VarId) (v : Deger) (b : Bolge)
      (h_t     : S.thread = ts1 ++ ctx :: ts2)
      (h_if    : ctx.ifade = .atama x (.sabit v))
      (h_b     : bolgeOrtamGet S.bolge x = some b)
      (h_owner : sahiplikGet S.sahiplik b = some (Sahip.thread ctx.tid))
      (h_S'    : S' = { S with
                thread := ts1 ++ { ctx with ifade := .sabit .birim } :: ts2,
                store  := (⟨b, 0⟩, v) :: S.store,
                iz     := .memYaz ctx.tid ⟨b, 0⟩ v :: S.iz,
                zaman  := S.zaman + 1,
                fault  := none }) :
      Step S S'

  /-- S-ATAMA Hata Donmus: frozen bolgeye yazma → fault.
      Fault non-observable: S' yalniz fault alaniyla farkli. -/
  | sAtamaHataDonmus
      (S S' : Konfigurasyon) (ts1 ts2 : List ThreadCtx) (ctx : ThreadCtx)
      (x : VarId) (v : Deger) (b : Bolge)
      (h_t      : S.thread = ts1 ++ ctx :: ts2)
      (h_if     : ctx.ifade = .atama x (.sabit v))
      (h_b      : bolgeOrtamGet S.bolge x = some b)
      (h_frozen : isFrozen S b)
      (h_S'     : S' = { S with fault := some (.donmusYazma b) }) :
      Step S S'

  /-- S-ATAMA Hata Sahip Degil: ctx hedef bolgenin guncel sahibi degil. -/
  | sAtamaHataSahipDegil
      (S S' : Konfigurasyon) (ts1 ts2 : List ThreadCtx) (ctx : ThreadCtx)
      (x : VarId) (v : Deger) (b : Bolge)
      (h_t         : S.thread = ts1 ++ ctx :: ts2)
      (h_if        : ctx.ifade = .atama x (.sabit v))
      (h_b         : bolgeOrtamGet S.bolge x = some b)
      (h_not_owner : sahiplikGet S.sahiplik b ≠ some (Sahip.thread ctx.tid))
      (h_S'        : S' = { S with fault := some (.sahipDegil b ctx.tid) }) :
      Step S S'

  /-- S-ATAMA Cong: atama RHS'i deger degilse icerde adim at.
      Ic adim, odakli thread'in ifadesi `e` olan konfigurasyonda kosulur;
      sonuc ctx' (tid ayni, ifade e') disari `atama x e'` olarak sarilir. -/
  | sAtamaCong
      (S S' S1 S1' : Konfigurasyon) (ts1 ts2 ts2' : List ThreadCtx)
      (ctx ctx' : ThreadCtx) (x : VarId) (e e' : Ifade)
      (h_t     : S.thread = ts1 ++ ctx :: ts2)
      (h_if    : ctx.ifade = .atama x e)
      (h_S1    : S1 = ifadeyleKonf S ts1 ts2 ctx e)
      (h_inner : Step S1 S1')
      (h_t1'   : S1'.thread = ts1 ++ ctx' :: ts2')
      (h_tid   : ctx'.tid = ctx.tid)
      (h_if'   : ctx'.ifade = e')
      (h_yan   : ts2' = ts2 ∨ ∃ y, ts2' = ts2 ++ [y])  -- FIX-F
      (h_S'    : S' = { S1' with
                thread := ts1 ++ { ctx' with ifade := .atama x e' } :: ts2' }) :
      Step S S'

  /-- S-SEQ-ATLA: sol taraf deger → atilir, sag tarafa gecilir. -/
  | sSeqAtla
      (S S' : Konfigurasyon) (ts1 ts2 : List ThreadCtx) (ctx : ThreadCtx)
      (v : Deger) (b : Ifade)
      (h_t  : S.thread = ts1 ++ ctx :: ts2)
      (h_if : ctx.ifade = .seq (.sabit v) b)
      (h_S' : S' = { S with
                thread := ts1 ++ { ctx with ifade := b } :: ts2,
                zaman  := S.zaman + 1,
                fault  := none }) :
      Step S S'

  /-- S-SEQ-CONG: sol taraf deger degilse icerde adim at. -/
  | sSeqCong
      (S S' S1 S1' : Konfigurasyon) (ts1 ts2 ts2' : List ThreadCtx)
      (ctx ctx' : ThreadCtx) (a a' b : Ifade)
      (h_t     : S.thread = ts1 ++ ctx :: ts2)
      (h_if    : ctx.ifade = .seq a b)
      (h_S1    : S1 = ifadeyleKonf S ts1 ts2 ctx a)
      (h_inner : Step S1 S1')
      (h_t1'   : S1'.thread = ts1 ++ ctx' :: ts2')
      (h_tid   : ctx'.tid = ctx.tid)
      (h_if'   : ctx'.ifade = a')
      -- FIX-F (Mehmet onayi — DECISIONS_LOG cong-penceresi counterexample):
      -- ic adim odakli pozisyona kilitli: yan thread'ler degismez (spawn
      -- appendi haric). Pencere-artefakti adimlar (kosan thread'i pencerede
      -- bitmis goren join) elenir; mesru adimlar dis-duzeyde dogrudan temsil
      -- edilir — davranis kaybi yok.
      (h_yan   : ts2' = ts2 ∨ ∃ y, ts2' = ts2 ++ [y])
      (h_S'    : S' = { S1' with
                thread := ts1 ++ { ctx' with ifade := .seq a' b } :: ts2' }) :
      Step S S'

  /-- S-EGER-SEC (D-332): kosul deger → dal secilir, `dalOl` olayi emit
      edilir. Olayin emit edilmesi teoremin ASIL ICERIGIDIR: dal karari
      saldirganin gordugu ize girer, dolayisiyla CT001 (gizli kosulda
      dallanma yasagi) olmadan non-interference SAGLANAMAZ. -/
  | sEgerSec
      (S S' : Konfigurasyon) (ts1 ts2 : List ThreadCtx) (ctx : ThreadCtx)
      (v : Deger) (d y : Ifade) (alindi : Bool)
      (h_t   : S.thread = ts1 ++ ctx :: ts2)
      (h_if  : ctx.ifade = .eger (.sabit v) d y)
      (h_dal : alindi = degerDogruMu v)
      (h_S'  : S' = { S with
                thread := ts1 ++ { ctx with ifade := if alindi then d else y } :: ts2,
                iz     := .dalOl ctx.tid alindi :: S.iz,
                zaman  := S.zaman + 1,
                fault  := none }) :
      Step S S'

  /-- S-EGER-CONG (D-332, Mehmet karari): kosul deger degilse kosulun
      icinde adim at. FIX-F cerceve yan-kosulu (h_yan) diger cong
      kurallariyla AYNEN ayni — odakli pozisyona kilit, yan thread'ler
      yalniz spawn appendi ile degisebilir. -/
  | sEgerCong
      (S S' S1 S1' : Konfigurasyon) (ts1 ts2 ts2' : List ThreadCtx)
      (ctx ctx' : ThreadCtx) (k k' d y : Ifade)
      (h_t     : S.thread = ts1 ++ ctx :: ts2)
      (h_if    : ctx.ifade = .eger k d y)
      (h_S1    : S1 = ifadeyleKonf S ts1 ts2 ctx k)
      (h_inner : Step S1 S1')
      (h_t1'   : S1'.thread = ts1 ++ ctx' :: ts2')
      (h_tid   : ctx'.tid = ctx.tid)
      (h_if'   : ctx'.ifade = k')
      (h_yan   : ts2' = ts2 ∨ ∃ z, ts2' = ts2 ++ [z])  -- FIX-F
      (h_S'    : S' = { S1' with
                thread := ts1 ++ { ctx' with ifade := .eger k' d y } :: ts2' }) :
      Step S S'

  /-- S-TOPLA-TAMAM (D-334): iki operand da skaler deger → toplanir.
      OLAY URETMEZ (aritmetik dogrudan gozlenmez); store/sahiplik sabit. -/
  | sToplaTamam
      (S S' : Konfigurasyon) (ts1 ts2 : List ThreadCtx) (ctx : ThreadCtx)
      (n1 n2 : Int)
      (h_t  : S.thread = ts1 ++ ctx :: ts2)
      (h_if : ctx.ifade = .topla (.sabit (.skaler n1)) (.sabit (.skaler n2)))
      (h_S' : S' = { S with
                thread := ts1 ++ { ctx with ifade := .sabit (.skaler (n1 + n2)) } :: ts2,
                zaman  := S.zaman + 1,
                fault  := none }) :
      Step S S'
  /-- S-CARP-TAMAM (D-340): iki operand da skaler deger → carpnir.
      OLAY URETMEZ (aritmetik dogrudan gozlenmez); store/sahiplik sabit. -/
  | sCarpTamam
      (S S' : Konfigurasyon) (ts1 ts2 : List ThreadCtx) (ctx : ThreadCtx)
      (n1 n2 : Int)
      (h_t  : S.thread = ts1 ++ ctx :: ts2)
      (h_if : ctx.ifade = .carp (.sabit (.skaler n1)) (.sabit (.skaler n2)))
      (h_S' : S' = { S with
                thread := ts1 ++ { ctx with ifade := .sabit (.skaler (n1 * n2)) } :: ts2,
                zaman  := S.zaman + 1,
                fault  := none }) :
      Step S S'
  /-- S-KALAN-TAMAM (D-339): `n1 % n2`. `sToplaTamam`dan TEK farki:
      **`modOl` OLAYI EMIT EDER**. Toplama sabit cevrimdir, bolme degil;
      gecikme operandlarin fonksiyonu oldugundan operandlar gozleme
      girer (muhafazakar ust sinir). `n2 = 0` icin sonuc 0 (Lean `Int`
      bolmesi) — TOPLAM, takilma/fault YOK. -/
  | sKalanTamam
      (S S' : Konfigurasyon) (ts1 ts2 : List ThreadCtx) (ctx : ThreadCtx)
      (n1 n2 : Int)
      (h_t  : S.thread = ts1 ++ ctx :: ts2)
      (h_if : ctx.ifade = .kalan (.sabit (.skaler n1)) (.sabit (.skaler n2)))
      (h_S' : S' = { S with
                thread := ts1 ++ { ctx with ifade := .sabit (.skaler (n1 % n2)) } :: ts2,
                iz     := .modOl ctx.tid n1 n2 :: S.iz,
                zaman  := S.zaman + 1,
                fault  := none }) :
      Step S S'

  /-- S-KALAN-CONG-SOL (D-339). -/
  | sKalanCongSol
      (S S' S1 S1' : Konfigurasyon) (ts1 ts2 ts2' : List ThreadCtx)
      (ctx ctx' : ThreadCtx) (a a' b : Ifade)
      (h_t     : S.thread = ts1 ++ ctx :: ts2)
      (h_if    : ctx.ifade = .kalan a b)
      (h_S1    : S1 = ifadeyleKonf S ts1 ts2 ctx a)
      (h_inner : Step S1 S1')
      (h_t1'   : S1'.thread = ts1 ++ ctx' :: ts2')
      (h_tid   : ctx'.tid = ctx.tid)
      (h_if'   : ctx'.ifade = a')
      (h_yan   : ts2' = ts2 ∨ ∃ z, ts2' = ts2 ++ [z])  -- FIX-F
      (h_S'    : S' = { S1' with
                thread := ts1 ++ { ctx' with ifade := .kalan a' b } :: ts2' }) :
      Step S S'

  /-- S-KALAN-CONG-SAG (D-339): sol operand DEGER ise sagda adim at. -/
  | sKalanCongSag
      (S S' S1 S1' : Konfigurasyon) (ts1 ts2 ts2' : List ThreadCtx)
      (ctx ctx' : ThreadCtx) (v : Deger) (b b' : Ifade)
      (h_t     : S.thread = ts1 ++ ctx :: ts2)
      (h_if    : ctx.ifade = .kalan (.sabit v) b)
      (h_S1    : S1 = ifadeyleKonf S ts1 ts2 ctx b)
      (h_inner : Step S1 S1')
      (h_t1'   : S1'.thread = ts1 ++ ctx' :: ts2')
      (h_tid   : ctx'.tid = ctx.tid)
      (h_if'   : ctx'.ifade = b')
      (h_yan   : ts2' = ts2 ∨ ∃ z, ts2' = ts2 ++ [z])  -- FIX-F
      (h_S'    : S' = { S1' with
                thread := ts1 ++ { ctx' with ifade := .kalan (.sabit v) b' } :: ts2' }) :
      Step S S'

  /-- S-BOL-TAMAM (D-338): `n1 / n2`. `sToplaTamam`dan TEK farki:
      **`bolOl` OLAYI EMIT EDER**. Toplama sabit cevrimdir, bolme degil;
      gecikme operandlarin fonksiyonu oldugundan operandlar gozleme
      girer (muhafazakar ust sinir). `n2 = 0` icin sonuc 0 (Lean `Int`
      bolmesi) — TOPLAM, takilma/fault YOK. -/
  | sBolTamam
      (S S' : Konfigurasyon) (ts1 ts2 : List ThreadCtx) (ctx : ThreadCtx)
      (n1 n2 : Int)
      (h_t  : S.thread = ts1 ++ ctx :: ts2)
      (h_if : ctx.ifade = .bol (.sabit (.skaler n1)) (.sabit (.skaler n2)))
      (h_S' : S' = { S with
                thread := ts1 ++ { ctx with ifade := .sabit (.skaler (n1 / n2)) } :: ts2,
                iz     := .bolOl ctx.tid n1 n2 :: S.iz,
                zaman  := S.zaman + 1,
                fault  := none }) :
      Step S S'

  /-- S-BOL-CONG-SOL (D-338). -/
  | sBolCongSol
      (S S' S1 S1' : Konfigurasyon) (ts1 ts2 ts2' : List ThreadCtx)
      (ctx ctx' : ThreadCtx) (a a' b : Ifade)
      (h_t     : S.thread = ts1 ++ ctx :: ts2)
      (h_if    : ctx.ifade = .bol a b)
      (h_S1    : S1 = ifadeyleKonf S ts1 ts2 ctx a)
      (h_inner : Step S1 S1')
      (h_t1'   : S1'.thread = ts1 ++ ctx' :: ts2')
      (h_tid   : ctx'.tid = ctx.tid)
      (h_if'   : ctx'.ifade = a')
      (h_yan   : ts2' = ts2 ∨ ∃ z, ts2' = ts2 ++ [z])  -- FIX-F
      (h_S'    : S' = { S1' with
                thread := ts1 ++ { ctx' with ifade := .bol a' b } :: ts2' }) :
      Step S S'

  /-- S-BOL-CONG-SAG (D-338): sol operand DEGER ise sagda adim at. -/
  | sBolCongSag
      (S S' S1 S1' : Konfigurasyon) (ts1 ts2 ts2' : List ThreadCtx)
      (ctx ctx' : ThreadCtx) (v : Deger) (b b' : Ifade)
      (h_t     : S.thread = ts1 ++ ctx :: ts2)
      (h_if    : ctx.ifade = .bol (.sabit v) b)
      (h_S1    : S1 = ifadeyleKonf S ts1 ts2 ctx b)
      (h_inner : Step S1 S1')
      (h_t1'   : S1'.thread = ts1 ++ ctx' :: ts2')
      (h_tid   : ctx'.tid = ctx.tid)
      (h_if'   : ctx'.ifade = b')
      (h_yan   : ts2' = ts2 ∨ ∃ z, ts2' = ts2 ++ [z])  -- FIX-F
      (h_S'    : S' = { S1' with
                thread := ts1 ++ { ctx' with ifade := .bol (.sabit v) b' } :: ts2' }) :
      Step S S'

  /-- S-TOPLA-CONG-SOL (D-334): SOL operand icinde adim at.
      (Deger olan sol operand adim atamaz → sira belirsizligi YOK.) -/
  | sToplaCongSol
      (S S' S1 S1' : Konfigurasyon) (ts1 ts2 ts2' : List ThreadCtx)
      (ctx ctx' : ThreadCtx) (a a' b : Ifade)
      (h_t     : S.thread = ts1 ++ ctx :: ts2)
      (h_if    : ctx.ifade = .topla a b)
      (h_S1    : S1 = ifadeyleKonf S ts1 ts2 ctx a)
      (h_inner : Step S1 S1')
      (h_t1'   : S1'.thread = ts1 ++ ctx' :: ts2')
      (h_tid   : ctx'.tid = ctx.tid)
      (h_if'   : ctx'.ifade = a')
      (h_yan   : ts2' = ts2 ∨ ∃ z, ts2' = ts2 ++ [z])  -- FIX-F
      (h_S'    : S' = { S1' with
                thread := ts1 ++ { ctx' with ifade := .topla a' b } :: ts2' }) :
      Step S S'

  /-- S-TOPLA-CONG-SAG (D-334): sol operand DEGER ise sag operand icinde
      adim at. Sol-once/sag-sonra sirasi CT'nin `c_topla`si ile birebir. -/
  | sToplaCongSag
      (S S' S1 S1' : Konfigurasyon) (ts1 ts2 ts2' : List ThreadCtx)
      (ctx ctx' : ThreadCtx) (v : Deger) (b b' : Ifade)
      (h_t     : S.thread = ts1 ++ ctx :: ts2)
      (h_if    : ctx.ifade = .topla (.sabit v) b)
      (h_S1    : S1 = ifadeyleKonf S ts1 ts2 ctx b)
      (h_inner : Step S1 S1')
      (h_t1'   : S1'.thread = ts1 ++ ctx' :: ts2')
      (h_tid   : ctx'.tid = ctx.tid)
      (h_if'   : ctx'.ifade = b')
      (h_yan   : ts2' = ts2 ∨ ∃ z, ts2' = ts2 ++ [z])  -- FIX-F
      (h_S'    : S' = { S1' with
                thread := ts1 ++ { ctx' with ifade := .topla (.sabit v) b' } :: ts2' }) :
      Step S S'
  /-- S-CARP-CONG-SOL (D-340): SOL operand icinde adim at.
      (Deger olan sol operand adim atamaz → sira belirsizligi YOK.) -/
  | sCarpCongSol
      (S S' S1 S1' : Konfigurasyon) (ts1 ts2 ts2' : List ThreadCtx)
      (ctx ctx' : ThreadCtx) (a a' b : Ifade)
      (h_t     : S.thread = ts1 ++ ctx :: ts2)
      (h_if    : ctx.ifade = .carp a b)
      (h_S1    : S1 = ifadeyleKonf S ts1 ts2 ctx a)
      (h_inner : Step S1 S1')
      (h_t1'   : S1'.thread = ts1 ++ ctx' :: ts2')
      (h_tid   : ctx'.tid = ctx.tid)
      (h_if'   : ctx'.ifade = a')
      (h_yan   : ts2' = ts2 ∨ ∃ z, ts2' = ts2 ++ [z])  -- FIX-F
      (h_S'    : S' = { S1' with
                thread := ts1 ++ { ctx' with ifade := .carp a' b } :: ts2' }) :
      Step S S'

  /-- S-CARP-CONG-SAG (D-340): sol operand DEGER ise sag operand icinde
      adim at. Sol-once/sag-sonra sirasi CT'nin `c_carp`si ile birebir. -/
  | sCarpCongSag
      (S S' S1 S1' : Konfigurasyon) (ts1 ts2 ts2' : List ThreadCtx)
      (ctx ctx' : ThreadCtx) (v : Deger) (b b' : Ifade)
      (h_t     : S.thread = ts1 ++ ctx :: ts2)
      (h_if    : ctx.ifade = .carp (.sabit v) b)
      (h_S1    : S1 = ifadeyleKonf S ts1 ts2 ctx b)
      (h_inner : Step S1 S1')
      (h_t1'   : S1'.thread = ts1 ++ ctx' :: ts2')
      (h_tid   : ctx'.tid = ctx.tid)
      (h_if'   : ctx'.ifade = b')
      (h_yan   : ts2' = ts2 ∨ ∃ z, ts2' = ts2 ++ [z])  -- FIX-F
      (h_S'    : S' = { S1' with
                thread := ts1 ++ { ctx' with ifade := .carp (.sabit v) b' } :: ts2' }) :
      Step S S'

  /-- S-IKEN-AC (D-335): dongu ACILIR — kendi cong/dal kurallarini
      GEREKTIRMEZ, `eger` makinesini yeniden kullanir:
        `iken k g  ⟶  eger k (seq g (iken k g)) (sabit birim)`
      Sonuc: her TUR bir `dalOl` uretir (aciliten `eger` sayesinde),
      yani DONGU SAYISI saldirgana gorunur — CT002 icin dogru model.
      Kendisi OLAY URETMEZ (acilma bir bookkeeping adimidir). -/
  | sIkenAc
      (S S' : Konfigurasyon) (ts1 ts2 : List ThreadCtx) (ctx : ThreadCtx)
      (k g : Ifade)
      (h_t  : S.thread = ts1 ++ ctx :: ts2)
      (h_if : ctx.ifade = .iken k g)
      (h_S' : S' = { S with
                thread := ts1 ++ { ctx with
                  ifade := .eger k (.seq g (.iken k g)) (.sabit .birim) } :: ts2,
                zaman  := S.zaman + 1,
                fault  := none }) :
      Step S S'

  /-- S-ESLES-SEC (D-335): skrutine deger → literal ile karsilastir,
      kol sec, `dalOl` emit et. Mehmet karari: eslesme KOL BASINA bir
      dal karari olarak gozlenir (dallanma zinciri modeli) — atlama
      tablosu VARSAYILMAZ. -/
  | sEslesSec
      (S S' : Konfigurasyon) (ts1 ts2 : List ThreadCtx) (ctx : ThreadCtx)
      (m n : Int) (d y : Ifade) (tuttu : Bool)
      (h_t   : S.thread = ts1 ++ ctx :: ts2)
      (h_if  : ctx.ifade = .esles (.sabit (.skaler m)) n d y)
      (h_dal : tuttu = decide (m = n))
      (h_S'  : S' = { S with
                thread := ts1 ++ { ctx with ifade := if tuttu then d else y } :: ts2,
                iz     := .dalOl ctx.tid tuttu :: S.iz,
                zaman  := S.zaman + 1,
                fault  := none }) :
      Step S S'

  /-- S-ESLES-CONG (D-335): skrutine deger degilse iceride adim at. -/
  | sEslesCong
      (S S' S1 S1' : Konfigurasyon) (ts1 ts2 ts2' : List ThreadCtx)
      (ctx ctx' : ThreadCtx) (s s' : Ifade) (n : Int) (d y : Ifade)
      (h_t     : S.thread = ts1 ++ ctx :: ts2)
      (h_if    : ctx.ifade = .esles s n d y)
      (h_S1    : S1 = ifadeyleKonf S ts1 ts2 ctx s)
      (h_inner : Step S1 S1')
      (h_t1'   : S1'.thread = ts1 ++ ctx' :: ts2')
      (h_tid   : ctx'.tid = ctx.tid)
      (h_if'   : ctx'.ifade = s')
      (h_yan   : ts2' = ts2 ∨ ∃ z, ts2' = ts2 ++ [z])  -- FIX-F
      (h_S'    : S' = { S1' with
                thread := ts1 ++ { ctx' with ifade := .esles s' n d y } :: ts2' }) :
      Step S S'

  /-- S-INDEKS-OKU (D-336): `x[i]` — okunan konum `⟨bolge(x), i.toNat⟩`
      ve emit edilen olay `memOku ... o konumu ...`. **Adres izde
      gorunur** — gizli-indeks kanalinin modeldeki temsili budur.
      Okuma TOPLAMDIR (`hucreOku`): takilma yok, sonuc daima skaler. -/
  | sIndeksOku
      (S S' : Konfigurasyon) (ts1 ts2 : List ThreadCtx) (ctx : ThreadCtx)
      (x : VarId) (i : Int) (b : Bolge) (n : Int)
      (h_t  : S.thread = ts1 ++ ctx :: ts2)
      (h_if : ctx.ifade = .indeks x (.sabit (.skaler i)))
      (h_b  : bolgeOrtamGet S.bolge x = some b)
      (h_n  : n = hucreOku S.store ⟨b, i.toNat⟩)
      (h_S' : S' = { S with
                thread := ts1 ++ { ctx with ifade := .sabit (.skaler n) } :: ts2,
                iz     := .memOku ctx.tid ⟨b, i.toNat⟩ (.skaler n) :: S.iz,
                zaman  := S.zaman + 1,
                fault  := none }) :
      Step S S'

  /-- S-INDEKS-CONG (D-336): indeks ifadesi deger degilse iceride adim at. -/
  | sIndeksCong
      (S S' S1 S1' : Konfigurasyon) (ts1 ts2 ts2' : List ThreadCtx)
      (ctx ctx' : ThreadCtx) (x : VarId) (e e' : Ifade)
      (h_t     : S.thread = ts1 ++ ctx :: ts2)
      (h_if    : ctx.ifade = .indeks x e)
      (h_S1    : S1 = ifadeyleKonf S ts1 ts2 ctx e)
      (h_inner : Step S1 S1')
      (h_t1'   : S1'.thread = ts1 ++ ctx' :: ts2')
      (h_tid   : ctx'.tid = ctx.tid)
      (h_if'   : ctx'.ifade = e')
      (h_yan   : ts2' = ts2 ∨ ∃ z, ts2' = ts2 ++ [z])  -- FIX-F
      (h_S'    : S' = { S1' with
                thread := ts1 ++ { ctx' with ifade := .indeks x e' } :: ts2' }) :
      Step S S'

  /-- S-INDEKS-YAZ (D-337): `x[i] = n`. `sAtamaTamam` ile ayni sahiplik
      sarti (`h_owner`) — yazma sahiplik ister. Yazilan konum
      `⟨bolge(x), i.toNat⟩` ve emit edilen `memYaz` O KONUMU tasir →
      **yazma adresi de gozlenir**. Sonuc `birim` (yazma bir DEYIMDIR). -/
  | sIndeksYaz
      (S S' : Konfigurasyon) (ts1 ts2 : List ThreadCtx) (ctx : ThreadCtx)
      (x : VarId) (i : Int) (n : Int) (b : Bolge)
      (h_t     : S.thread = ts1 ++ ctx :: ts2)
      (h_if    : ctx.ifade = .indeksAta x (.sabit (.skaler i)) (.sabit (.skaler n)))
      (h_b     : bolgeOrtamGet S.bolge x = some b)
      (h_owner : sahiplikGet S.sahiplik b = some (Sahip.thread ctx.tid))
      (h_S'    : S' = { S with
                thread := ts1 ++ { ctx with ifade := .sabit .birim } :: ts2,
                store  := (⟨b, i.toNat⟩, Deger.skaler n) :: S.store,
                iz     := .memYaz ctx.tid ⟨b, i.toNat⟩ (.skaler n) :: S.iz,
                zaman  := S.zaman + 1,
                fault  := none }) :
      Step S S'

  /-- S-INDEKS-ATA-CONG-IDX (D-337): INDEKS icinde adim at (once indeks). -/
  | sIndeksAtaCongIdx
      (S S' S1 S1' : Konfigurasyon) (ts1 ts2 ts2' : List ThreadCtx)
      (ctx ctx' : ThreadCtx) (x : VarId) (idx idx' e : Ifade)
      (h_t     : S.thread = ts1 ++ ctx :: ts2)
      (h_if    : ctx.ifade = .indeksAta x idx e)
      (h_S1    : S1 = ifadeyleKonf S ts1 ts2 ctx idx)
      (h_inner : Step S1 S1')
      (h_t1'   : S1'.thread = ts1 ++ ctx' :: ts2')
      (h_tid   : ctx'.tid = ctx.tid)
      (h_if'   : ctx'.ifade = idx')
      (h_yan   : ts2' = ts2 ∨ ∃ z, ts2' = ts2 ++ [z])  -- FIX-F
      (h_S'    : S' = { S1' with
                thread := ts1 ++ { ctx' with ifade := .indeksAta x idx' e } :: ts2' }) :
      Step S S'

  /-- S-INDEKS-ATA-CONG-DEG (D-337): indeks DEGER ise deger icinde adim at. -/
  | sIndeksAtaCongDeg
      (S S' S1 S1' : Konfigurasyon) (ts1 ts2 ts2' : List ThreadCtx)
      (ctx ctx' : ThreadCtx) (x : VarId) (v : Deger) (e e' : Ifade)
      (h_t     : S.thread = ts1 ++ ctx :: ts2)
      (h_if    : ctx.ifade = .indeksAta x (.sabit v) e)
      (h_S1    : S1 = ifadeyleKonf S ts1 ts2 ctx e)
      (h_inner : Step S1 S1')
      (h_t1'   : S1'.thread = ts1 ++ ctx' :: ts2')
      (h_tid   : ctx'.tid = ctx.tid)
      (h_if'   : ctx'.ifade = e')
      (h_yan   : ts2' = ts2 ∨ ∃ z, ts2' = ts2 ++ [z])  -- FIX-F
      (h_S'    : S' = { S1' with
                thread := ts1 ++ { ctx' with
                  ifade := .indeksAta x (.sabit v) e' } :: ts2' }) :
      Step S S'

  /-- S-GUVENSIZ-ATLA: ic ifade deger → sarmal acilir. -/
  | sGuvensizAtla
      (S S' : Konfigurasyon) (ts1 ts2 : List ThreadCtx) (ctx : ThreadCtx)
      (v : Deger)
      (h_t  : S.thread = ts1 ++ ctx :: ts2)
      (h_if : ctx.ifade = .guvensiz (.sabit v))
      (h_S' : S' = { S with
                thread := ts1 ++ { ctx with ifade := .sabit v } :: ts2,
                zaman  := S.zaman + 1,
                fault  := none }) :
      Step S S'

  /-- S-GUVENSIZ-CONG: ic ifadede adim at. -/
  | sGuvensizCong
      (S S' S1 S1' : Konfigurasyon) (ts1 ts2 ts2' : List ThreadCtx)
      (ctx ctx' : ThreadCtx) (e e' : Ifade)
      (h_t     : S.thread = ts1 ++ ctx :: ts2)
      (h_if    : ctx.ifade = .guvensiz e)
      (h_S1    : S1 = ifadeyleKonf S ts1 ts2 ctx e)
      (h_inner : Step S1 S1')
      (h_t1'   : S1'.thread = ts1 ++ ctx' :: ts2')
      (h_tid   : ctx'.tid = ctx.tid)
      (h_if'   : ctx'.ifade = e')
      (h_yan   : ts2' = ts2 ∨ ∃ y, ts2' = ts2 ++ [y])  -- FIX-F
      (h_S'    : S' = { S1' with
                thread := ts1 ++ { ctx' with ifade := .guvensiz e' } :: ts2' }) :
      Step S S'

  /-- C-GOREV-BASLAT Tamam: yeni thread spawn; yakalanan bolgeler
      (sahip oldugun!) tYeni'ye transfer; yakalanan aktif lineer'lar
      caller'da tuketilir; cocugun lineer ortami yakalananlar-aktif;
      ifade gorev tanitici degerine ilerler. Runtime S.bolge da
      R-GOREV gecisini yansitir (S.bolge = Ρ korunumu icin). -/
  | cGorevBaslatTamam
      (S S' : Konfigurasyon) (ts1 ts2 : List ThreadCtx) (ctx : ThreadCtx)
      (tYeni : ThreadId) (yd : List VarId) (kod : Ifade)
      (h_t        : S.thread = ts1 ++ ctx :: ts2)
      (h_if       : ctx.ifade = .gorevBaslat yd kod)
      (h_fresh    : threadFresh S tYeni)
      (h_sahipler : ∀ b ∈ bolgeleriTopla S.bolge yd,
                      sahiplikGet S.sahiplik b = some (Sahip.thread ctx.tid))
      (h_S'       : S' = { S with
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
      Step S S'

  /-- C-GOREV-BASLAT Hata: yakalanan lineer ZATEN tuketilmis
      (use-after-move) → fault. -/
  | cGorevBaslatHataLineerIhlal
      (S S' : Konfigurasyon) (ts1 ts2 : List ThreadCtx) (ctx : ThreadCtx)
      (yd : List VarId) (kod : Ifade) (vIhlal : VarId)
      (h_t         : S.thread = ts1 ++ ctx :: ts2)
      (h_if        : ctx.ifade = .gorevBaslat yd kod)
      (h_vIhlal_in : vIhlal ∈ yd)
      (h_tuket     : lineerOrtamGet ctx.lineer vIhlal = some Lineerlik.tuketildi)
      (h_S'        : S' = { S with
                fault := some (.lineerYakalananZatenTuketildi vIhlal) }) :
      Step S S'

  /-- C-GOREV-BIRLESTIR Tamam: hedef thread degerini uretmis (sabit vSon);
      tHedef'in sahip oldugu (devredilen) bolgeler caller'a doner.
      V1 daraltma: donus degeri tasinmaz (HasType t_gorev_birlestir : bos). -/
  | cGorevBirlestirTamam
      (S S' : Konfigurasyon) (ts1 ts2 : List ThreadCtx) (ctx : ThreadCtx)
      (g : VarId) (tHedef : ThreadId) (returnedBolgeler : List Bolge)
      (h_t       : S.thread = ts1 ++ ctx :: ts2)
      (h_if      : ctx.ifade = .gorevBirlestir g)
      (h_hedef   : ∃ hctx ∈ S.thread, hctx.tid = tHedef ∧
                     ∃ vSon : Deger, hctx.ifade = .sabit vSon)
      (h_donen   : ∀ b ∈ returnedBolgeler,
                     sahiplikGet S.sahiplik b = some (Sahip.thread tHedef))
      (h_S'      : S' = { S with
                thread := ts1 ++ { ctx with ifade := .sabit .birim } :: ts2,
                sahiplik := sahiplikSetMany S.sahiplik returnedBolgeler
                              (Sahip.thread ctx.tid),
                iz     := .threadBitir tHedef :: S.iz,
                zaman  := S.zaman + 1,
                fault  := none }) :
      Step S S'

  /-- C-KANAL-GONDER Tamam: vId'nin konumundaki deger kanala eklenir;
      bolgesi kanal transit'ine gecer (sahiplik + runtime bolge);
      lineer ise tuketilir. Gonderen bolgenin guncel sahibi olmali. -/
  | cKanalGonderTamam
      (S S' : Konfigurasyon) (ts1 ts2 : List ThreadCtx) (ctx : ThreadCtx)
      (k : KanalId) (vId : VarId) (b : Bolge) (v : Deger)
      (h_t     : S.thread = ts1 ++ ctx :: ts2)
      (h_if    : ctx.ifade = .kanalGonderIf k vId)
      (h_b     : bolgeOrtamGet S.bolge vId = some b)
      (h_v     : konumGet S.store ⟨b, 0⟩ = some v)
      (h_owner : sahiplikGet S.sahiplik b = some (Sahip.thread ctx.tid))
      -- KAPASITE-1 (Mehmet karari, 2026-06-11): dolu kanala gonderim BLOKLAR
      -- (rendezvous-benzeri); boylece kuyruk uzunlugu ≤ 1 invarianti korunur
      -- ve cKanalAl pop sonrasi transit-tanigi sorunu kalkmaz. Buffer = V2.
      (h_bos   : kanalIlk S.kanal k = none)
      (h_S'    : S' = { S with
                thread := ts1 ++ { ctx with
                            ifade  := .sabit .birim,
                            lineer := lineerTuket ctx.lineer vId } :: ts2,
                kanal  := kanalEkle S.kanal k v,
                sahiplik := sahiplikSet S.sahiplik b (Sahip.kanalSahip k),
                bolge  := bolgeOrtamUpdate S.bolge vId
                            (bolgeKategoriDegistir b (BolgeKategorisi.kanalRho k)),
                iz     := .kanalGonderOl ctx.tid k v :: S.iz,
                zaman  := S.zaman + 1,
                fault  := none }) :
      Step S S'

  /-- C-KANAL-GONDER Hata: lineer vId zaten tuketilmis (cifte gonderim). -/
  | cKanalGonderHataLineerTuket
      (S S' : Konfigurasyon) (ts1 ts2 : List ThreadCtx) (ctx : ThreadCtx)
      (k : KanalId) (vId : VarId)
      (h_t     : S.thread = ts1 ++ ctx :: ts2)
      (h_if    : ctx.ifade = .kanalGonderIf k vId)
      (h_tuket : lineerOrtamGet ctx.lineer vId = some Lineerlik.tuketildi)
      (h_S'    : S' = { S with fault := some (.lineerKanalTuket vId) }) :
      Step S S'

  /-- C-KANAL-AL Tamam: kuyruktaki ilk deger alinir, ifade o degere
      ilerler; kanal transit'indeki bolge alana gecer. -/
  | cKanalAlTamam
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
      Step S S'

  /-- C-DONDUR Tamam: sahibi oldugun bolgeyi dondur (R-PAYLAS).
      h_owner cifte-dondur'u otomatik dislar (thread ≠ donmus).
      Runtime S.bolge da R-DONDUR gecisini yansitir. -/
  | cDondurTamam
      (S S' : Konfigurasyon) (ts1 ts2 : List ThreadCtx) (ctx : ThreadCtx)
      (b : Bolge)
      (h_t     : S.thread = ts1 ++ ctx :: ts2)
      (h_if    : ctx.ifade = .dondurIf b)
      (h_owner : sahiplikGet S.sahiplik b = some (Sahip.thread ctx.tid))
      (h_S'    : S' = { S with
                thread := ts1 ++ { ctx with ifade := .sabit .birim } :: ts2,
                sahiplik := sahiplikSet S.sahiplik b Sahip.donmus,
                bolge  := bolgeOrtamDondurBolge S.bolge b,
                iz     := .dondurOl ctx.tid b :: S.iz,
                zaman  := S.zaman + 1,
                fault  := none }) :
      Step S S'

  /-- C-DONDUR Hata Zaten Donmus: cifte dondur → fault. -/
  | cDondurHataZatenDonmus
      (S S' : Konfigurasyon) (ts1 ts2 : List ThreadCtx) (ctx : ThreadCtx)
      (b : Bolge)
      (h_t     : S.thread = ts1 ++ ctx :: ts2)
      (h_if    : ctx.ifade = .dondurIf b)
      (h_zaten : isFrozen S b)
      (h_S'    : S' = { S with fault := some (.zatenDonmus b) }) :
      Step S S'

  /-- S-LIN-KULLAN Tamam: aktif lineer tuketilir; ifade birim'e ilerler
      (V1 daraltma: deger cikarimi V2 — HasType t_kullan : bos). -/
  | sLinKullanTamam
      (S S' : Konfigurasyon) (ts1 ts2 : List ThreadCtx) (ctx : ThreadCtx)
      (x : VarId)
      (h_t     : S.thread = ts1 ++ ctx :: ts2)
      (h_if    : ctx.ifade = .kullanIf x)
      (h_aktif : lineerOrtamGet ctx.lineer x = some Lineerlik.aktif)
      (h_S'    : S' = { S with
                thread := ts1 ++ { ctx with
                            ifade  := .sabit .birim,
                            lineer := lineerOrtamUpdate ctx.lineer x
                                        Lineerlik.tuketildi } :: ts2,
                zaman  := S.zaman + 1,
                fault  := none }) :
      Step S S'

  /-- S-LIN-KULLAN Hata: zaten tuketilmis → fault. -/
  | sLinKullanHataZatenTuketildi
      (S S' : Konfigurasyon) (ts1 ts2 : List ThreadCtx) (ctx : ThreadCtx)
      (x : VarId)
      (h_t     : S.thread = ts1 ++ ctx :: ts2)
      (h_if    : ctx.ifade = .kullanIf x)
      (h_tuket : lineerOrtamGet ctx.lineer x = some Lineerlik.tuketildi)
      (h_S'    : S' = { S with fault := some (.lineerZatenTuketildi x) }) :
      Step S S'

  /-- S-LIN-IMHA Tamam: aktif lineer imha edilir (tuketilir). -/
  | sLinImhaTamam
      (S S' : Konfigurasyon) (ts1 ts2 : List ThreadCtx) (ctx : ThreadCtx)
      (x : VarId)
      (h_t     : S.thread = ts1 ++ ctx :: ts2)
      (h_if    : ctx.ifade = .imhaIf x)
      (h_aktif : lineerOrtamGet ctx.lineer x = some Lineerlik.aktif)
      (h_S'    : S' = { S with
                thread := ts1 ++ { ctx with
                            ifade  := .sabit .birim,
                            lineer := lineerOrtamUpdate ctx.lineer x
                                        Lineerlik.tuketildi } :: ts2,
                zaman  := S.zaman + 1,
                fault  := none }) :
      Step S S'

  /-- S-LIN-IMHA Hata: zaten tuketilmis → fault. -/
  | sLinImhaHataZatenTuketildi
      (S S' : Konfigurasyon) (ts1 ts2 : List ThreadCtx) (ctx : ThreadCtx)
      (x : VarId)
      (h_t     : S.thread = ts1 ++ ctx :: ts2)
      (h_if    : ctx.ifade = .imhaIf x)
      (h_tuket : lineerOrtamGet ctx.lineer x = some Lineerlik.tuketildi)
      (h_S'    : S' = { S with fault := some (.lineerZatenTuketildi x) }) :
      Step S S'


-- ============================================================
-- §3. Coklu-adim reduksiyon — StepStar (refleksif gecisli kapanis)
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

/-- Coklu adim gecisli (transitivity). -/
theorem stepStar_trans (S S1 S2 : Konfigurasyon)
    (h1 : StepStar S S1) (h2 : StepStar S1 S2) : StepStar S S2 := by
  induction h1 with
  | refl _ => exact h2
  | step S0 Smid Send hStep _ ih =>
      exact StepStar.step S0 Smid S2 hStep (ih h2)

-- ============================================================
-- §5. step_iz_analiz — TRACE GORUNUM LEMMASI (F2)
-- L4 / L7 / Drf Teorem 4' / MemSafety T1 hepsi bu tek lemmadan
-- turetilir (eski 5 dosyada ~15'er case'lik tekrarli analiz yerine
-- 21 constructor uzerinde TEK induction).
-- ============================================================

/-- Bir adimin iz/store/sahiplik evrimi — dort sekil:
    (1) sessiz: iz/store/sahiplik degismez (yapisal, lineer, fault adimlar)
    (2) okuma: iz'e memOku eklenir; store/sahiplik degismez
    (3) yazma: iz'e memYaz eklenir; store'a ayni (k,v) push edilir;
        sahiplik degismez; yazan thread hedef bolgenin GUNCEL sahibi
    (4) diger olay: iz'e memYaz/memOku OLMAYAN tek olay eklenir;
        store degismez.

    Her sekil en fazla BIR yeni olay ekler (Teorem 4' same-step cekirdegi). -/
theorem step_iz_analiz (S S' : Konfigurasyon) (h_step : Step S S') :
    (S'.iz = S.iz ∧ S'.store = S.store ∧ S'.sahiplik = S.sahiplik)
    ∨ (∃ t k v, S'.iz = .memOku t k v :: S.iz
          ∧ S'.store = S.store ∧ S'.sahiplik = S.sahiplik)
    ∨ (∃ t k v, S'.iz = .memYaz t k v :: S.iz
          ∧ S'.store = (k, v) :: S.store
          ∧ S'.sahiplik = S.sahiplik
          ∧ sahiplikGet S.sahiplik k.bolge = some (Sahip.thread t))
    ∨ (∃ olay, S'.iz = olay :: S.iz
          ∧ (∀ t k v, olay ≠ .memYaz t k v)
          ∧ (∀ t k v, olay ≠ .memOku t k v)
          ∧ S'.store = S.store) := by
  induction h_step with
  | sVarOku S S' ts1 ts2 ctx x b v h_t h_if h_b h_v h_S' =>
      subst h_S'
      exact Or.inr (Or.inl ⟨ctx.tid, ⟨b, 0⟩, v, rfl, rfl, rfl⟩)
  | sAtamaTamam S S' ts1 ts2 ctx x v b h_t h_if h_b h_owner h_S' =>
      subst h_S'
      exact Or.inr (Or.inr (Or.inl ⟨ctx.tid, ⟨b, 0⟩, v, rfl, rfl, rfl, h_owner⟩))
  | sAtamaHataDonmus S S' ts1 ts2 ctx x v b h_t h_if h_b h_frozen h_S' =>
      subst h_S'; exact Or.inl ⟨rfl, rfl, rfl⟩
  | sAtamaHataSahipDegil S S' ts1 ts2 ctx x v b h_t h_if h_b h_not_owner h_S' =>
      subst h_S'; exact Or.inl ⟨rfl, rfl, rfl⟩
  | sAtamaCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' x e e' h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      subst h_S1 h_S'
      simpa [ifadeyleKonf] using ih
  | sSeqAtla S S' ts1 ts2 ctx v b h_t h_if h_S' =>
      subst h_S'; exact Or.inl ⟨rfl, rfl, rfl⟩
  | sSeqCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' a a' b h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      subst h_S1 h_S'
      simpa [ifadeyleKonf] using ih
  -- D-332: sEgerSec `dalOl` emit eder — memOku/memYaz DEGIL, store'a
  -- dokunmaz → dorduncu disjunct (diger-olay).
  | sEgerSec S S' ts1 ts2 ctx v d y alindi h_t h_if h_dal h_S' =>
      subst h_S'
      refine Or.inr (Or.inr (Or.inr ⟨.dalOl ctx.tid alindi, rfl, ?_, ?_, rfl⟩))
      · intro t0 k0 v0 hh; nomatch hh
      · intro t0 k0 v0 hh; nomatch hh
  | sEgerCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' k k' d y h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      subst h_S1 h_S'
      simpa [ifadeyleKonf] using ih
  -- D-334: topla OLAY URETMEZ, store/sahiplik sabit → birinci disjunct.
  | sToplaTamam S S' ts1 ts2 ctx n1 n2 h_t h_if h_S' =>
      subst h_S'; exact Or.inl ⟨rfl, rfl, rfl⟩
  -- D-338: sBolTamam `bolOl` emit eder — memOku/memYaz DEGIL, store'a
  -- dokunmaz → dorduncu disjunct (diger-olay).
  | sCarpTamam S S' ts1 ts2 ctx n1 n2 h_t h_if h_S' =>
      subst h_S'; exact Or.inl ⟨rfl, rfl, rfl⟩
  -- D-338: sBolTamam `bolOl` emit eder — memOku/memYaz DEGIL, store'a
  -- dokunmaz → dorduncu disjunct (diger-olay).
  | sBolTamam S S' ts1 ts2 ctx n1 n2 h_t h_if h_S' =>
      subst h_S'
      refine Or.inr (Or.inr (Or.inr ⟨.bolOl ctx.tid n1 n2, rfl, ?_, ?_, rfl⟩))
      · intro t0 k0 v0 hh; nomatch hh
      · intro t0 k0 v0 hh; nomatch hh
  | sBolCongSol S S' S1 S1' ts1 ts2 ts2' ctx ctx' a a' b h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      subst h_S1 h_S'
      simpa [ifadeyleKonf] using ih
  | sBolCongSag S S' S1 S1' ts1 ts2 ts2' ctx ctx' v b b' h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      subst h_S1 h_S'
      simpa [ifadeyleKonf] using ih
  | sKalanTamam S S' ts1 ts2 ctx n1 n2 h_t h_if h_S' =>
      subst h_S'
      refine Or.inr (Or.inr (Or.inr ⟨.modOl ctx.tid n1 n2, rfl, ?_, ?_, rfl⟩))
      · intro t0 k0 v0 hh; nomatch hh
      · intro t0 k0 v0 hh; nomatch hh
  | sKalanCongSol S S' S1 S1' ts1 ts2 ts2' ctx ctx' a a' b h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      subst h_S1 h_S'
      simpa [ifadeyleKonf] using ih
  | sKalanCongSag S S' S1 S1' ts1 ts2 ts2' ctx ctx' v b b' h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      subst h_S1 h_S'
      simpa [ifadeyleKonf] using ih
  | sToplaCongSol S S' S1 S1' ts1 ts2 ts2' ctx ctx' a a' b h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      subst h_S1 h_S'
      simpa [ifadeyleKonf] using ih
  | sCarpCongSol S S' S1 S1' ts1 ts2 ts2' ctx ctx' a a' b h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      subst h_S1 h_S'
      simpa [ifadeyleKonf] using ih
  | sToplaCongSag S S' S1 S1' ts1 ts2 ts2' ctx ctx' v b b' h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      subst h_S1 h_S'
      simpa [ifadeyleKonf] using ih
  -- D-335: sIkenAc olay uretmez; sEslesSec `dalOl` uretir.
  | sCarpCongSag S S' S1 S1' ts1 ts2 ts2' ctx ctx' v b b' h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      subst h_S1 h_S'
      simpa [ifadeyleKonf] using ih
  -- D-335: sIkenAc olay uretmez; sEslesSec `dalOl` uretir.
  | sIkenAc S S' ts1 ts2 ctx k g h_t h_if h_S' =>
      subst h_S'; exact Or.inl ⟨rfl, rfl, rfl⟩
  | sEslesSec S S' ts1 ts2 ctx m n d y tuttu h_t h_if h_dal h_S' =>
      subst h_S'
      refine Or.inr (Or.inr (Or.inr ⟨.dalOl ctx.tid tuttu, rfl, ?_, ?_, rfl⟩))
      · intro t0 k0 v0 hh; nomatch hh
      · intro t0 k0 v0 hh; nomatch hh
  | sEslesCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' s s' n d y h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      subst h_S1 h_S'
      simpa [ifadeyleKonf] using ih
  -- D-336: sIndeksOku bir memOku'dur — IKINCI disjunct (okuma).
  | sIndeksOku S S' ts1 ts2 ctx x i b n h_t h_if h_b h_n h_S' =>
      subst h_S'
      exact Or.inr (Or.inl ⟨ctx.tid, ⟨b, i.toNat⟩, .skaler n, rfl, rfl, rfl⟩)
  | sIndeksCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' x e e' h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      subst h_S1 h_S'
      simpa [ifadeyleKonf] using ih
  -- D-337: sIndeksYaz bir memYaz'dir — UCUNCU disjunct (yazma+sahiplik).
  | sIndeksYaz S S' ts1 ts2 ctx x i n b h_t h_if h_b h_owner h_S' =>
      subst h_S'
      exact Or.inr (Or.inr (Or.inl
        ⟨ctx.tid, ⟨b, i.toNat⟩, .skaler n, rfl, rfl, rfl, h_owner⟩))
  | sIndeksAtaCongIdx S S' S1 S1' ts1 ts2 ts2' ctx ctx' x idx idx' e h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      subst h_S1 h_S'
      simpa [ifadeyleKonf] using ih
  | sIndeksAtaCongDeg S S' S1 S1' ts1 ts2 ts2' ctx ctx' x v e e' h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      subst h_S1 h_S'
      simpa [ifadeyleKonf] using ih
  | sGuvensizAtla S S' ts1 ts2 ctx v h_t h_if h_S' =>
      subst h_S'; exact Or.inl ⟨rfl, rfl, rfl⟩
  | sGuvensizCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' e e' h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      subst h_S1 h_S'
      simpa [ifadeyleKonf] using ih
  | cGorevBaslatTamam S S' ts1 ts2 ctx tYeni yd kod h_t h_if h_fresh h_sahipler h_S' =>
      subst h_S'
      refine Or.inr (Or.inr (Or.inr ⟨.threadBaslat tYeni, rfl, ?_, ?_, rfl⟩))
      · intro t0 k0 v0 hh; nomatch hh
      · intro t0 k0 v0 hh; nomatch hh
  | cGorevBaslatHataLineerIhlal S S' ts1 ts2 ctx yd kod vIhlal h_t h_if h_in h_tuket h_S' =>
      subst h_S'; exact Or.inl ⟨rfl, rfl, rfl⟩
  | cGorevBirlestirTamam S S' ts1 ts2 ctx g tHedef rb h_t h_if h_hedef h_donen h_S' =>
      subst h_S'
      refine Or.inr (Or.inr (Or.inr ⟨.threadBitir tHedef, rfl, ?_, ?_, rfl⟩))
      · intro t0 k0 v0 hh; nomatch hh
      · intro t0 k0 v0 hh; nomatch hh
  | cKanalGonderTamam S S' ts1 ts2 ctx k vId b v h_t h_if h_b h_v h_owner h_bos h_S' =>
      subst h_S'
      refine Or.inr (Or.inr (Or.inr ⟨.kanalGonderOl ctx.tid k v, rfl, ?_, ?_, rfl⟩))
      · intro t0 k0 v0 hh; nomatch hh
      · intro t0 k0 v0 hh; nomatch hh
  | cKanalGonderHataLineerTuket S S' ts1 ts2 ctx k vId h_t h_if h_tuket h_S' =>
      subst h_S'; exact Or.inl ⟨rfl, rfl, rfl⟩
  | cKanalAlTamam S S' ts1 ts2 ctx k v tb h_t h_if h_v h_transit h_S' =>
      subst h_S'
      refine Or.inr (Or.inr (Or.inr ⟨.kanalAlOl ctx.tid k v, rfl, ?_, ?_, rfl⟩))
      · intro t0 k0 v0 hh; nomatch hh
      · intro t0 k0 v0 hh; nomatch hh
  | cDondurTamam S S' ts1 ts2 ctx b h_t h_if h_owner h_S' =>
      subst h_S'
      refine Or.inr (Or.inr (Or.inr ⟨.dondurOl ctx.tid b, rfl, ?_, ?_, rfl⟩))
      · intro t0 k0 v0 hh; nomatch hh
      · intro t0 k0 v0 hh; nomatch hh
  | cDondurHataZatenDonmus S S' ts1 ts2 ctx b h_t h_if h_zaten h_S' =>
      subst h_S'; exact Or.inl ⟨rfl, rfl, rfl⟩
  | sLinKullanTamam S S' ts1 ts2 ctx x h_t h_if h_aktif h_S' =>
      subst h_S'; exact Or.inl ⟨rfl, rfl, rfl⟩
  | sLinKullanHataZatenTuketildi S S' ts1 ts2 ctx x h_t h_if h_tuket h_S' =>
      subst h_S'; exact Or.inl ⟨rfl, rfl, rfl⟩
  | sLinImhaTamam S S' ts1 ts2 ctx x h_t h_if h_aktif h_S' =>
      subst h_S'; exact Or.inl ⟨rfl, rfl, rfl⟩
  | sLinImhaHataZatenTuketildi S S' ts1 ts2 ctx x h_t h_if h_tuket h_S' =>
      subst h_S'; exact Or.inl ⟨rfl, rfl, rfl⟩


-- ============================================================
-- §6. Fault gorunumu — fault-non-observability (F2)
-- ============================================================

/-- Her adim ya fault'suz biter ya da gozlemlenemez fault uretir
    (iz/store/sahiplik/zaman degismez). Eski Hata-constructor
    "h_store/h_iz/h_zaman/h_sahip/h_kanal strengthen" hipotez ailesinin
    tek-lemma karsiligi. -/
theorem step_fault_gorunum (S S' : Konfigurasyon) (h_step : Step S S') :
    S'.fault = none
    ∨ (∃ sebep, S'.fault = some sebep ∧ S'.iz = S.iz ∧ S'.store = S.store
          ∧ S'.sahiplik = S.sahiplik ∧ S'.zaman = S.zaman) := by
  induction h_step with
  | sVarOku S S' ts1 ts2 ctx x b v h_t h_if h_b h_v h_S' =>
      subst h_S'; exact Or.inl rfl
  | sAtamaTamam S S' ts1 ts2 ctx x v b h_t h_if h_b h_owner h_S' =>
      subst h_S'; exact Or.inl rfl
  | sAtamaHataDonmus S S' ts1 ts2 ctx x v b h_t h_if h_b h_frozen h_S' =>
      subst h_S'; exact Or.inr ⟨_, rfl, rfl, rfl, rfl, rfl⟩
  | sAtamaHataSahipDegil S S' ts1 ts2 ctx x v b h_t h_if h_b h_no h_S' =>
      subst h_S'; exact Or.inr ⟨_, rfl, rfl, rfl, rfl, rfl⟩
  | sAtamaCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' x e e' h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      subst h_S1 h_S'
      rcases ih with h_f | ⟨sebep, h_f, h_iz, h_st, h_sa, h_z⟩
      · exact Or.inl h_f
      · exact Or.inr ⟨sebep, h_f, by simpa [ifadeyleKonf] using h_iz,
          by simpa [ifadeyleKonf] using h_st, by simpa [ifadeyleKonf] using h_sa,
          by simpa [ifadeyleKonf] using h_z⟩
  | sSeqAtla S S' ts1 ts2 ctx v b h_t h_if h_S' =>
      subst h_S'; exact Or.inl rfl
  | sSeqCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' a a' b h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      subst h_S1 h_S'
      rcases ih with h_f | ⟨sebep, h_f, h_iz, h_st, h_sa, h_z⟩
      · exact Or.inl h_f
      · exact Or.inr ⟨sebep, h_f, by simpa [ifadeyleKonf] using h_iz,
          by simpa [ifadeyleKonf] using h_st, by simpa [ifadeyleKonf] using h_sa,
          by simpa [ifadeyleKonf] using h_z⟩
  -- D-332
  | sEgerSec S S' ts1 ts2 ctx v d y alindi h_t h_if h_dal h_S' =>
      subst h_S'; exact Or.inl rfl
  | sEgerCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' k k' d y h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      subst h_S1 h_S'
      rcases ih with h_f | ⟨sebep, h_f, h_iz, h_st, h_sa, h_z⟩
      · exact Or.inl h_f
      · exact Or.inr ⟨sebep, h_f, by simpa [ifadeyleKonf] using h_iz,
          by simpa [ifadeyleKonf] using h_st, by simpa [ifadeyleKonf] using h_sa,
          by simpa [ifadeyleKonf] using h_z⟩
  -- D-334
  | sToplaTamam S S' ts1 ts2 ctx n1 n2 h_t h_if h_S' =>
      subst h_S'; exact Or.inl rfl
  -- D-338
  | sCarpTamam S S' ts1 ts2 ctx n1 n2 h_t h_if h_S' =>
      subst h_S'; exact Or.inl rfl
  -- D-338
  | sBolTamam S S' ts1 ts2 ctx n1 n2 h_t h_if h_S' =>
      subst h_S'; exact Or.inl rfl
  | sBolCongSol S S' S1 S1' ts1 ts2 ts2' ctx ctx' a a' b h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      subst h_S1 h_S'
      rcases ih with h_f | ⟨sebep, h_f, h_iz, h_st, h_sa, h_z⟩
      · exact Or.inl h_f
      · exact Or.inr ⟨sebep, h_f, by simpa [ifadeyleKonf] using h_iz,
          by simpa [ifadeyleKonf] using h_st, by simpa [ifadeyleKonf] using h_sa,
          by simpa [ifadeyleKonf] using h_z⟩
  | sBolCongSag S S' S1 S1' ts1 ts2 ts2' ctx ctx' v b b' h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      subst h_S1 h_S'
      rcases ih with h_f | ⟨sebep, h_f, h_iz, h_st, h_sa, h_z⟩
      · exact Or.inl h_f
      · exact Or.inr ⟨sebep, h_f, by simpa [ifadeyleKonf] using h_iz,
          by simpa [ifadeyleKonf] using h_st, by simpa [ifadeyleKonf] using h_sa,
          by simpa [ifadeyleKonf] using h_z⟩
  | sKalanTamam S S' ts1 ts2 ctx n1 n2 h_t h_if h_S' =>
      subst h_S'; exact Or.inl rfl
  | sKalanCongSol S S' S1 S1' ts1 ts2 ts2' ctx ctx' a a' b h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      subst h_S1 h_S'
      rcases ih with h_f | ⟨sebep, h_f, h_iz, h_st, h_sa, h_z⟩
      · exact Or.inl h_f
      · exact Or.inr ⟨sebep, h_f, by simpa [ifadeyleKonf] using h_iz,
          by simpa [ifadeyleKonf] using h_st, by simpa [ifadeyleKonf] using h_sa,
          by simpa [ifadeyleKonf] using h_z⟩
  | sKalanCongSag S S' S1 S1' ts1 ts2 ts2' ctx ctx' v b b' h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      subst h_S1 h_S'
      rcases ih with h_f | ⟨sebep, h_f, h_iz, h_st, h_sa, h_z⟩
      · exact Or.inl h_f
      · exact Or.inr ⟨sebep, h_f, by simpa [ifadeyleKonf] using h_iz,
          by simpa [ifadeyleKonf] using h_st, by simpa [ifadeyleKonf] using h_sa,
          by simpa [ifadeyleKonf] using h_z⟩
  | sToplaCongSol S S' S1 S1' ts1 ts2 ts2' ctx ctx' a a' b h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      subst h_S1 h_S'
      rcases ih with h_f | ⟨sebep, h_f, h_iz, h_st, h_sa, h_z⟩
      · exact Or.inl h_f
      · exact Or.inr ⟨sebep, h_f, by simpa [ifadeyleKonf] using h_iz,
          by simpa [ifadeyleKonf] using h_st, by simpa [ifadeyleKonf] using h_sa,
          by simpa [ifadeyleKonf] using h_z⟩
  | sCarpCongSol S S' S1 S1' ts1 ts2 ts2' ctx ctx' a a' b h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      subst h_S1 h_S'
      rcases ih with h_f | ⟨sebep, h_f, h_iz, h_st, h_sa, h_z⟩
      · exact Or.inl h_f
      · exact Or.inr ⟨sebep, h_f, by simpa [ifadeyleKonf] using h_iz,
          by simpa [ifadeyleKonf] using h_st, by simpa [ifadeyleKonf] using h_sa,
          by simpa [ifadeyleKonf] using h_z⟩
  | sToplaCongSag S S' S1 S1' ts1 ts2 ts2' ctx ctx' v b b' h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      subst h_S1 h_S'
      rcases ih with h_f | ⟨sebep, h_f, h_iz, h_st, h_sa, h_z⟩
      · exact Or.inl h_f
      · exact Or.inr ⟨sebep, h_f, by simpa [ifadeyleKonf] using h_iz,
          by simpa [ifadeyleKonf] using h_st, by simpa [ifadeyleKonf] using h_sa,
          by simpa [ifadeyleKonf] using h_z⟩
  -- D-335
  | sCarpCongSag S S' S1 S1' ts1 ts2 ts2' ctx ctx' v b b' h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      subst h_S1 h_S'
      rcases ih with h_f | ⟨sebep, h_f, h_iz, h_st, h_sa, h_z⟩
      · exact Or.inl h_f
      · exact Or.inr ⟨sebep, h_f, by simpa [ifadeyleKonf] using h_iz,
          by simpa [ifadeyleKonf] using h_st, by simpa [ifadeyleKonf] using h_sa,
          by simpa [ifadeyleKonf] using h_z⟩
  -- D-335
  | sIkenAc S S' ts1 ts2 ctx k g h_t h_if h_S' =>
      subst h_S'; exact Or.inl rfl
  | sEslesSec S S' ts1 ts2 ctx m n d y tuttu h_t h_if h_dal h_S' =>
      subst h_S'; exact Or.inl rfl
  | sEslesCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' s s' n d y h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      subst h_S1 h_S'
      rcases ih with h_f | ⟨sebep, h_f, h_iz, h_st, h_sa, h_z⟩
      · exact Or.inl h_f
      · exact Or.inr ⟨sebep, h_f, by simpa [ifadeyleKonf] using h_iz,
          by simpa [ifadeyleKonf] using h_st, by simpa [ifadeyleKonf] using h_sa,
          by simpa [ifadeyleKonf] using h_z⟩
  -- D-336
  | sIndeksOku S S' ts1 ts2 ctx x i b n h_t h_if h_b h_n h_S' =>
      subst h_S'; exact Or.inl rfl
  | sIndeksCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' x e e' h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      subst h_S1 h_S'
      rcases ih with h_f | ⟨sebep, h_f, h_iz, h_st, h_sa, h_z⟩
      · exact Or.inl h_f
      · exact Or.inr ⟨sebep, h_f, by simpa [ifadeyleKonf] using h_iz,
          by simpa [ifadeyleKonf] using h_st, by simpa [ifadeyleKonf] using h_sa,
          by simpa [ifadeyleKonf] using h_z⟩
  -- D-337
  | sIndeksYaz S S' ts1 ts2 ctx x i n b h_t h_if h_b h_owner h_S' =>
      subst h_S'; exact Or.inl rfl
  | sIndeksAtaCongIdx S S' S1 S1' ts1 ts2 ts2' ctx ctx' x idx idx' e h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      subst h_S1 h_S'
      rcases ih with h_f | ⟨sebep, h_f, h_iz, h_st, h_sa, h_z⟩
      · exact Or.inl h_f
      · exact Or.inr ⟨sebep, h_f, by simpa [ifadeyleKonf] using h_iz,
          by simpa [ifadeyleKonf] using h_st, by simpa [ifadeyleKonf] using h_sa,
          by simpa [ifadeyleKonf] using h_z⟩
  | sIndeksAtaCongDeg S S' S1 S1' ts1 ts2 ts2' ctx ctx' x v e e' h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      subst h_S1 h_S'
      rcases ih with h_f | ⟨sebep, h_f, h_iz, h_st, h_sa, h_z⟩
      · exact Or.inl h_f
      · exact Or.inr ⟨sebep, h_f, by simpa [ifadeyleKonf] using h_iz,
          by simpa [ifadeyleKonf] using h_st, by simpa [ifadeyleKonf] using h_sa,
          by simpa [ifadeyleKonf] using h_z⟩
  | sGuvensizAtla S S' ts1 ts2 ctx v h_t h_if h_S' =>
      subst h_S'; exact Or.inl rfl
  | sGuvensizCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' e e' h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      subst h_S1 h_S'
      rcases ih with h_f | ⟨sebep, h_f, h_iz, h_st, h_sa, h_z⟩
      · exact Or.inl h_f
      · exact Or.inr ⟨sebep, h_f, by simpa [ifadeyleKonf] using h_iz,
          by simpa [ifadeyleKonf] using h_st, by simpa [ifadeyleKonf] using h_sa,
          by simpa [ifadeyleKonf] using h_z⟩
  | cGorevBaslatTamam S S' ts1 ts2 ctx tYeni yd kod h_t h_if h_fresh h_sahipler h_S' =>
      subst h_S'; exact Or.inl rfl
  | cGorevBaslatHataLineerIhlal S S' ts1 ts2 ctx yd kod vIhlal h_t h_if h_in h_tuket h_S' =>
      subst h_S'; exact Or.inr ⟨_, rfl, rfl, rfl, rfl, rfl⟩
  | cGorevBirlestirTamam S S' ts1 ts2 ctx g tHedef rb h_t h_if h_hedef h_donen h_S' =>
      subst h_S'; exact Or.inl rfl
  | cKanalGonderTamam S S' ts1 ts2 ctx k vId b v h_t h_if h_b h_v h_owner h_bos h_S' =>
      subst h_S'; exact Or.inl rfl
  | cKanalGonderHataLineerTuket S S' ts1 ts2 ctx k vId h_t h_if h_tuket h_S' =>
      subst h_S'; exact Or.inr ⟨_, rfl, rfl, rfl, rfl, rfl⟩
  | cKanalAlTamam S S' ts1 ts2 ctx k v tb h_t h_if h_v h_transit h_S' =>
      subst h_S'; exact Or.inl rfl
  | cDondurTamam S S' ts1 ts2 ctx b h_t h_if h_owner h_S' =>
      subst h_S'; exact Or.inl rfl
  | cDondurHataZatenDonmus S S' ts1 ts2 ctx b h_t h_if h_zaten h_S' =>
      subst h_S'; exact Or.inr ⟨_, rfl, rfl, rfl, rfl, rfl⟩
  | sLinKullanTamam S S' ts1 ts2 ctx x h_t h_if h_aktif h_S' =>
      subst h_S'; exact Or.inl rfl
  | sLinKullanHataZatenTuketildi S S' ts1 ts2 ctx x h_t h_if h_tuket h_S' =>
      subst h_S'; exact Or.inr ⟨_, rfl, rfl, rfl, rfl, rfl⟩
  | sLinImhaTamam S S' ts1 ts2 ctx x h_t h_if h_aktif h_S' =>
      subst h_S'; exact Or.inl rfl
  | sLinImhaHataZatenTuketildi S S' ts1 ts2 ctx x h_t h_if h_tuket h_S' =>
      subst h_S'; exact Or.inr ⟨_, rfl, rfl, rfl, rfl, rfl⟩


-- ============================================================
-- §7. Sahiplik yazim disiplini: donmus bolge ASLA override edilmez (F2)
-- isFrozen persistence cekirdegi — KOSULSUZ (eski conditional
-- isFrozen_persistent_simple formunun yerini alir).
-- ============================================================

/-- Donmus bolgenin sahipligi hicbir adimda degismez: sahiplik yazan her
    kural hedefin GUNCEL sahibinin thread/kanal olmasini sart kosar;
    donmus bunlardan farkli (lookup fonksiyonel) → hedef kumesi donmus
    bolgeyi iceremez → lookup korunur. -/
theorem step_donmus_korunur (S S' : Konfigurasyon) (h_step : Step S S')
    (b : Bolge) (h_frozen : sahiplikGet S.sahiplik b = some Sahip.donmus) :
    sahiplikGet S'.sahiplik b = some Sahip.donmus := by
  revert h_frozen
  induction h_step with
  | sVarOku S S' ts1 ts2 ctx x b' v h_t h_if h_b h_v h_S' =>
      intro h_frozen; subst h_S'; exact h_frozen
  | sAtamaTamam S S' ts1 ts2 ctx x v b' h_t h_if h_b h_owner h_S' =>
      intro h_frozen; subst h_S'; exact h_frozen
  | sAtamaHataDonmus S S' ts1 ts2 ctx x v b' h_t h_if h_b h_fr h_S' =>
      intro h_frozen; subst h_S'; exact h_frozen
  | sAtamaHataSahipDegil S S' ts1 ts2 ctx x v b' h_t h_if h_b h_no h_S' =>
      intro h_frozen; subst h_S'; exact h_frozen
  | sAtamaCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' x e e' h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      intro h_frozen
      subst h_S1 h_S'
      have h1 : sahiplikGet (ifadeyleKonf S ts1 ts2 ctx e).sahiplik b
          = some Sahip.donmus := by simpa [ifadeyleKonf] using h_frozen
      simpa using ih h1
  | sSeqAtla S S' ts1 ts2 ctx v bb h_t h_if h_S' =>
      intro h_frozen; subst h_S'; exact h_frozen
  | sSeqCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' a a' bb h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      intro h_frozen
      subst h_S1 h_S'
      have h1 : sahiplikGet (ifadeyleKonf S ts1 ts2 ctx a).sahiplik b
          = some Sahip.donmus := by simpa [ifadeyleKonf] using h_frozen
      simpa using ih h1
  -- D-332
  | sEgerSec S S' ts1 ts2 ctx v d y alindi h_t h_if h_dal h_S' =>
      intro h_frozen; subst h_S'; exact h_frozen
  | sEgerCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' k k' d y h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      intro h_frozen
      subst h_S1 h_S'
      have h1 : sahiplikGet (ifadeyleKonf S ts1 ts2 ctx k).sahiplik b
          = some Sahip.donmus := by simpa [ifadeyleKonf] using h_frozen
      simpa using ih h1
  -- D-334
  | sToplaTamam S S' ts1 ts2 ctx n1 n2 h_t h_if h_S' =>
      intro h_frozen; subst h_S'; exact h_frozen
  -- D-338
  | sCarpTamam S S' ts1 ts2 ctx n1 n2 h_t h_if h_S' =>
      intro h_frozen; subst h_S'; exact h_frozen
  -- D-338
  | sBolTamam S S' ts1 ts2 ctx n1 n2 h_t h_if h_S' =>
      intro h_frozen; subst h_S'; exact h_frozen
  | sBolCongSol S S' S1 S1' ts1 ts2 ts2' ctx ctx' a a' bb h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      intro h_frozen
      subst h_S1 h_S'
      have h1 : sahiplikGet (ifadeyleKonf S ts1 ts2 ctx a).sahiplik b
          = some Sahip.donmus := by simpa [ifadeyleKonf] using h_frozen
      simpa using ih h1
  | sBolCongSag S S' S1 S1' ts1 ts2 ts2' ctx ctx' v bb bb' h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      intro h_frozen
      subst h_S1 h_S'
      have h1 : sahiplikGet (ifadeyleKonf S ts1 ts2 ctx bb).sahiplik b
          = some Sahip.donmus := by simpa [ifadeyleKonf] using h_frozen
      simpa using ih h1
  | sKalanTamam S S' ts1 ts2 ctx n1 n2 h_t h_if h_S' =>
      intro h_frozen; subst h_S'; exact h_frozen
  | sKalanCongSol S S' S1 S1' ts1 ts2 ts2' ctx ctx' a a' bb h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      intro h_frozen
      subst h_S1 h_S'
      have h1 : sahiplikGet (ifadeyleKonf S ts1 ts2 ctx a).sahiplik b
          = some Sahip.donmus := by simpa [ifadeyleKonf] using h_frozen
      simpa using ih h1
  | sKalanCongSag S S' S1 S1' ts1 ts2 ts2' ctx ctx' v bb bb' h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      intro h_frozen
      subst h_S1 h_S'
      have h1 : sahiplikGet (ifadeyleKonf S ts1 ts2 ctx bb).sahiplik b
          = some Sahip.donmus := by simpa [ifadeyleKonf] using h_frozen
      simpa using ih h1
  | sToplaCongSol S S' S1 S1' ts1 ts2 ts2' ctx ctx' a a' bb h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      intro h_frozen
      subst h_S1 h_S'
      have h1 : sahiplikGet (ifadeyleKonf S ts1 ts2 ctx a).sahiplik b
          = some Sahip.donmus := by simpa [ifadeyleKonf] using h_frozen
      simpa using ih h1
  | sCarpCongSol S S' S1 S1' ts1 ts2 ts2' ctx ctx' a a' bb h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      intro h_frozen
      subst h_S1 h_S'
      have h1 : sahiplikGet (ifadeyleKonf S ts1 ts2 ctx a).sahiplik b
          = some Sahip.donmus := by simpa [ifadeyleKonf] using h_frozen
      simpa using ih h1
  | sToplaCongSag S S' S1 S1' ts1 ts2 ts2' ctx ctx' v bb bb' h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      intro h_frozen
      subst h_S1 h_S'
      have h1 : sahiplikGet (ifadeyleKonf S ts1 ts2 ctx bb).sahiplik b
          = some Sahip.donmus := by simpa [ifadeyleKonf] using h_frozen
      simpa using ih h1
  -- D-335
  | sCarpCongSag S S' S1 S1' ts1 ts2 ts2' ctx ctx' v bb bb' h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      intro h_frozen
      subst h_S1 h_S'
      have h1 : sahiplikGet (ifadeyleKonf S ts1 ts2 ctx bb).sahiplik b
          = some Sahip.donmus := by simpa [ifadeyleKonf] using h_frozen
      simpa using ih h1
  -- D-335
  | sIkenAc S S' ts1 ts2 ctx k g h_t h_if h_S' =>
      intro h_frozen; subst h_S'; exact h_frozen
  | sEslesSec S S' ts1 ts2 ctx m n d y tuttu h_t h_if h_dal h_S' =>
      intro h_frozen; subst h_S'; exact h_frozen
  | sEslesCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' s s' n d y h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      intro h_frozen
      subst h_S1 h_S'
      have h1 : sahiplikGet (ifadeyleKonf S ts1 ts2 ctx s).sahiplik b
          = some Sahip.donmus := by simpa [ifadeyleKonf] using h_frozen
      simpa using ih h1
  -- D-336
  | sIndeksOku S S' ts1 ts2 ctx x i b' n h_t h_if h_b h_n h_S' =>
      intro h_frozen; subst h_S'; exact h_frozen
  | sIndeksCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' x e e' h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      intro h_frozen
      subst h_S1 h_S'
      have h1 : sahiplikGet (ifadeyleKonf S ts1 ts2 ctx e).sahiplik b
          = some Sahip.donmus := by simpa [ifadeyleKonf] using h_frozen
      simpa using ih h1
  -- D-337
  | sIndeksYaz S S' ts1 ts2 ctx x i n b' h_t h_if h_b h_owner h_S' =>
      intro h_frozen; subst h_S'; exact h_frozen
  | sIndeksAtaCongIdx S S' S1 S1' ts1 ts2 ts2' ctx ctx' x idx idx' e h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      intro h_frozen
      subst h_S1 h_S'
      have h1 : sahiplikGet (ifadeyleKonf S ts1 ts2 ctx idx).sahiplik b
          = some Sahip.donmus := by simpa [ifadeyleKonf] using h_frozen
      simpa using ih h1
  | sIndeksAtaCongDeg S S' S1 S1' ts1 ts2 ts2' ctx ctx' x v e e' h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      intro h_frozen
      subst h_S1 h_S'
      have h1 : sahiplikGet (ifadeyleKonf S ts1 ts2 ctx e).sahiplik b
          = some Sahip.donmus := by simpa [ifadeyleKonf] using h_frozen
      simpa using ih h1
  | sGuvensizAtla S S' ts1 ts2 ctx v h_t h_if h_S' =>
      intro h_frozen; subst h_S'; exact h_frozen
  | sGuvensizCong S S' S1 S1' ts1 ts2 ts2' ctx ctx' e e' h_t h_if h_S1 h_inner h_t1' h_tid h_if' h_yan h_S' ih =>
      intro h_frozen
      subst h_S1 h_S'
      have h1 : sahiplikGet (ifadeyleKonf S ts1 ts2 ctx e).sahiplik b
          = some Sahip.donmus := by simpa [ifadeyleKonf] using h_frozen
      simpa using ih h1
  | cGorevBaslatTamam S S' ts1 ts2 ctx tYeni yd kod h_t h_if h_fresh h_sahipler h_S' =>
      intro h_frozen
      subst h_S'
      have h_not_in : ∀ b'' ∈ bolgeleriTopla S.bolge yd, b''.id ≠ b.id := by
        intro b'' h_in h_id
        have h_own := h_sahipler b'' h_in
        rw [sahiplikGet_id_esit S.sahiplik b'' b h_id, h_frozen] at h_own
        nomatch h_own
      show sahiplikGet (sahiplikSetMany S.sahiplik (bolgeleriTopla S.bolge yd)
             (Sahip.thread tYeni)) b = some Sahip.donmus
      rw [sahiplikSetMany_ne _ _ _ _ h_not_in]; exact h_frozen
  | cGorevBaslatHataLineerIhlal S S' ts1 ts2 ctx yd kod vIhlal h_t h_if h_in h_tuket h_S' =>
      intro h_frozen; subst h_S'; exact h_frozen
  | cGorevBirlestirTamam S S' ts1 ts2 ctx g tHedef rb h_t h_if h_hedef h_donen h_S' =>
      intro h_frozen
      subst h_S'
      have h_not_in : ∀ b'' ∈ rb, b''.id ≠ b.id := by
        intro b'' h_in h_id
        have h_own := h_donen b'' h_in
        rw [sahiplikGet_id_esit S.sahiplik b'' b h_id, h_frozen] at h_own
        nomatch h_own
      show sahiplikGet (sahiplikSetMany S.sahiplik rb (Sahip.thread ctx.tid)) b
             = some Sahip.donmus
      rw [sahiplikSetMany_ne _ _ _ _ h_not_in]; exact h_frozen
  | cKanalGonderTamam S S' ts1 ts2 ctx k vId b' v h_t h_if h_b h_v h_owner h_bos h_S' =>
      intro h_frozen
      subst h_S'
      have h_ne : b.id ≠ b'.id := by
        intro he
        rw [sahiplikGet_id_esit S.sahiplik b b' he, h_owner] at h_frozen
        nomatch h_frozen
      show sahiplikGet (sahiplikSet S.sahiplik b' (Sahip.kanalSahip k)) b
             = some Sahip.donmus
      rw [sahiplikSet_ne _ _ _ _ h_ne]; exact h_frozen
  | cKanalGonderHataLineerTuket S S' ts1 ts2 ctx k vId h_t h_if h_tuket h_S' =>
      intro h_frozen; subst h_S'; exact h_frozen
  | cKanalAlTamam S S' ts1 ts2 ctx k v tb h_t h_if h_v h_transit h_S' =>
      intro h_frozen
      subst h_S'
      have h_ne : b.id ≠ tb.id := by
        intro he
        rw [sahiplikGet_id_esit S.sahiplik b tb he, h_transit] at h_frozen
        nomatch h_frozen
      show sahiplikGet (sahiplikSet S.sahiplik tb (Sahip.thread ctx.tid)) b
             = some Sahip.donmus
      rw [sahiplikSet_ne _ _ _ _ h_ne]; exact h_frozen
  | cDondurTamam S S' ts1 ts2 ctx b' h_t h_if h_owner h_S' =>
      intro h_frozen
      subst h_S'
      have h_ne : b.id ≠ b'.id := by
        intro he
        rw [sahiplikGet_id_esit S.sahiplik b b' he, h_owner] at h_frozen
        nomatch h_frozen
      show sahiplikGet (sahiplikSet S.sahiplik b' Sahip.donmus) b
             = some Sahip.donmus
      rw [sahiplikSet_ne _ _ _ _ h_ne]; exact h_frozen
  | cDondurHataZatenDonmus S S' ts1 ts2 ctx b' h_t h_if h_zaten h_S' =>
      intro h_frozen; subst h_S'; exact h_frozen
  | sLinKullanTamam S S' ts1 ts2 ctx x h_t h_if h_aktif h_S' =>
      intro h_frozen; subst h_S'; exact h_frozen
  | sLinKullanHataZatenTuketildi S S' ts1 ts2 ctx x h_t h_if h_tuket h_S' =>
      intro h_frozen; subst h_S'; exact h_frozen
  | sLinImhaTamam S S' ts1 ts2 ctx x h_t h_if h_aktif h_S' =>
      intro h_frozen; subst h_S'; exact h_frozen
  | sLinImhaHataZatenTuketildi S S' ts1 ts2 ctx x h_t h_if h_tuket h_S' =>
      intro h_frozen; subst h_S'; exact h_frozen

end Kemgu.Sem.SmallStep
