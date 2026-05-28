-- KEMGU mekanize ispatlari — root modulu
-- Tum alt-modulleri burada toplar (lake build default_target bunu derler)
-- Bkz. belgeler/KEMGU_DRF_Mekanize_Spec.md §4 dizin yapisi

-- Faz A2: Operasyonel semantik altyapisi
import Kemgu.Sem.Core
import Kemgu.Sem.SmallStep
-- Plan v2 Adim 2: ConfigTyped iskelet (DegerTipli + SigmaTipli + SahiplikTutarli + KanalTutarli + KonfTipli)
import Kemgu.Sem.StateTipli
-- Plan v2 Adim 3: Minimal HasType (klasik tip sistemi, 12 Ifade kurali)
import Kemgu.Sem.HasType
-- Plan v2 Adim 4.1: Progress + Preservation iskelet (statement'lar sorry, Adim 4.2-4.4 doldurur)
import Kemgu.Sem.ProgressKorunum
-- Plan v2 Adim 5: LineerTamam katmani (Plan §3.3 LinearOK) + TypedAdim5 conjunction
import Kemgu.Sem.LineerTamam
-- Plan v2 Adim 6: RegionTamam katmani (Plan §3.4 RegionOK) + Typed full + ThreadTipliFull + KonfTipliFull
import Kemgu.Sem.RegionTamam

-- Faz A3: DRF lemmalar
import Kemgu.Drf.L0BolgeKorunumu
import Kemgu.Drf.L4FrozenRegionRead
import Kemgu.Drf.L1BolgeThreadTekilligi
import Kemgu.Drf.L7BellekErisimTipSoundness
import Kemgu.Drf.L2LinearMoveCrossThread
import Kemgu.Drf.L3LinearClosureSoundness
import Kemgu.Drf.L5KanalAtomikTransfer
import Kemgu.Drf.L6CapabilityLinear
-- Faz A3.10: Teorem 4'
import Kemgu.Drf.Drf
-- Faz V2.1: Cross-Step DRF altyapi (HB ordering) + iskelet
import Kemgu.Drf.HappensBefore
import Kemgu.Drf.DrfCrossStep

-- Faz B1: Memory Safety (T1 weak via L4; T2/T3 iskelet — bolge lifecycle refactor bekler)
import Kemgu.MemSafety.Theorems
-- Faz B2: BET iskelet (realtime model refactor bekler)
import Kemgu.BET.Boundedness
-- Faz B3: Side-Channel iskelet (sabitsure tracking refactor bekler)
import Kemgu.SideChannel.NonInterference

-- Faz C: V3 Butunleşik Guvenlik Metateoremi (bundled form)
import Kemgu.Soundness.Main
