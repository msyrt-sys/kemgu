/-
KEMGU DRF Mekanize — DRF-L2 Linear Move Cross-Thread No-Alias (Faz A3.6)
Kaynak (kagit formel): belgeler/KEMGU_DRF_Lemmalar.md §DRF-L2

═══════════════════════════════════════════════════════════════════════
TIKANMA: cGorevBaslat refactor gerek (A3.0''' onerisi)
═══════════════════════════════════════════════════════════════════════

Kagit DRF-L2 ifadesi:
  IyiTipli(Pi) ∧ Gamma ⊢ v : tekkez<tau> ∨ Gamma ⊢ v : yetki<R>
  ∧ Pi'nin bir izinde C-GOREV-BASLAT(c) reduksiyonu uygulanir,
    v ∈ YD(c) (yakalama listesinde)
  ⟹
  ∀ z > z_gorev_baslat : Lambda1(v) = TUKETILDI (cagiran'da tuketilmis)

Tikanma sebebi:
  Mevcut cGorevBaslat constructor (SmallStep.lean §C-*) yakalama
  listesini parametre olarak alir (yd : List VarId, transferredBolgeler
  : List Bolge), fakat CAGIRAN'in ctx.lineer durumunda Linear var'larin
  Lineerlik.aktif → tuketildi transition'ini ENFORCE ETMEZ. Yani
  cGorevBaslat sonrasi S'.thread'da ctx'in lineer durumu DEGISMEDEN
  kalir; aktif Linear v'ler hala AKTIF gozukur.

Bu DRF-L2'nin temel "consumption-after-spawn" semantigini bizim Step
modelinde dogrudan ifade etmeyi imkansiz kilar.

Cozum onerileri (Mehmet onayi gerekli):

(a''') cGorevBaslat refactor: 'h_lineer_caller' clause ekle
   Onerilen constructor sirasi:
     | cGorevBaslat
         (S S' : Konfigurasyon)
         (ctx : ThreadCtx) (tYeni : ThreadId)
         (yd : List VarId) (kod : Ifade)
         (transferredBolgeler : List Bolge)
         (linearYakalananlar : List VarId)  -- YENI: lineer alt-kume
         ...
         (h_lineer_caller :
           ∃ ctx' ∈ S'.thread, ctx'.tid = ctx.tid ∧
             ∀ v ∈ linearYakalananlar,
               (v, Lineerlik.tuketildi) ∈ ctx'.lineer)
         ...
   Tahmini maliyet: ~10-15 satir SmallStep + ~80-120 satir L2 ispat.

(b''') Linear semantik V2 mekanize: ctx.lineer'in tum transition'larini
   (sLinKullan, sLinImha, cGorevBaslat, cKanalGonder, vb.) explicit
   spec eden detayli refactor. Daha kapsamli (~200-400 satir).

(c''') L2'yi V2'ye defer: kabul edilemez — Teorem 4' L2'ye bagimli
   (DRF kompozit ispati Linear move semantigini gerektirir).

Politika: 'sorry KOYMA, lemma'yi YARIM BIRAKMA, o lemma'da DUR.
Iskelet bırakılır.' Bu dosya iskelet. Ispat dahil edilmedi.

Onceki TIKANMA emsali: A3.3 DRF-L4 (4585d6d) → A3.0'' refactor
(9089682) ile cozuldu. Bu (a''') benzer pattern — Mehmet onayi
ile A3.0''' refactor uygulanir.
═══════════════════════════════════════════════════════════════════════
-/

import Kemgu.Sem.Core
import Kemgu.Sem.SmallStep

namespace Kemgu.Drf.L2LinearMoveCrossThread

-- (yer tutucu — cGorevBaslat refactor sonrasi A3.6 devam eder)

end Kemgu.Drf.L2LinearMoveCrossThread
