-- KEMGU DRF Mekanize — Lake proje tanımı
-- Bkz. belgeler/KEMGU_DRF_Mekanize_Spec.md §5.3
import Lake
open Lake DSL

package «kemgu-drf-proofs» where
  -- Lean derleyici secenekleri (tum spec dosyalari icin)
  leanOptions := #[
    ⟨`pp.unicode.fun, true⟩,        -- pretty-print: a => b yerine a → b
    ⟨`autoImplicit, false⟩,         -- otomatik implicit yasak (acik yaz)
    ⟨`relaxedAutoImplicit, false⟩
  ]

@[default_target]
lean_lib «Kemgu» where
  -- DRF + MemSafety + SideChannel + BET + Soundness alt-modulleri burada
  -- root: proofs/drf-v2-lean/Kemgu.lean
  -- alt:  proofs/drf-v2-lean/Kemgu/Sem/Core.lean (Kemgu.Sem.Core)
  --       proofs/drf-v2-lean/Kemgu/Drf/*.lean

require mathlib from git
  "https://github.com/leanprover-community/mathlib4.git" @ "v4.29.0"
