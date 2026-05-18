-- KEMGU mekanize ispatlari — root modulu
-- Tum alt-modulleri burada toplar (lake build default_target bunu derler)
-- Bkz. belgeler/KEMGU_DRF_Mekanize_Spec.md §4 dizin yapisi

-- Faz A2: Operasyonel semantik altyapisi
import Kemgu.Sem.Core
import Kemgu.Sem.SmallStep

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

-- Faz B1: Memory Safety (T1 weak via L4; T2/T3 iskelet — bolge lifecycle refactor bekler)
import Kemgu.MemSafety.Theorems
-- Faz B2: BET iskelet (realtime model refactor bekler)
import Kemgu.BET.Boundedness
-- Faz B3: Side-Channel iskelet (sabitsure tracking refactor bekler)
import Kemgu.SideChannel.NonInterference

-- Faz C (yapilacak): V3 metateorem
-- import Kemgu.Soundness.Main
