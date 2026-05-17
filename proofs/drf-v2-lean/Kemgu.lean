-- KEMGU mekanize ispatlari — root modulu
-- Tum alt-modulleri burada toplar (lake build default_target bunu derler)
-- Bkz. belgeler/KEMGU_DRF_Mekanize_Spec.md §4 dizin yapisi

-- Faz A2: Operasyonel semantik altyapisi
import Kemgu.Sem.Core
import Kemgu.Sem.SmallStep

-- Faz A3-A5 (yapilacak): DRF lemmalar + Teorem 4'
-- import Kemgu.Drf.L0BolgeKorunumu
-- import Kemgu.Drf.L7BellekErisimTipSoundness
-- import Kemgu.Drf.L1BolgeThreadTekilligi
-- import Kemgu.Drf.L2LinearMoveCrossThread
-- import Kemgu.Drf.L3LinearClosureSoundness
-- import Kemgu.Drf.L4FrozenRegionRead
-- import Kemgu.Drf.L5ChannelAtomicity
-- import Kemgu.Drf.L6CapabilityLinear
-- import Kemgu.Drf.Drf

-- Faz B (yapilacak): Memory Safety + Side-Channel + BET
-- import Kemgu.MemSafety.Theorems
-- import Kemgu.SideChannel.NonInterference
-- import Kemgu.BET.Boundedness

-- Faz C (yapilacak): V3 metateorem
-- import Kemgu.Soundness.Main
