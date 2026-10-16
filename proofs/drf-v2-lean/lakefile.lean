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

-- [D-529] `require mathlib` KALDIRILDI — ÖLÇÜLDÜ: 32 `.lean` dosyasının
-- HİÇBİRİ Mathlib'i import etmiyor (tüm importlar iç: `Kemgu.*`). Bağımlılık
-- yalnız bildirimde duruyordu ve `lake build`i KOŞULAMAZ kılıyordu: lake
-- mathlib4'ü klonlamaya çalışıp `git exited with code 128` ile 11 DAKİKA
-- sonra düşüyordu (ağ/DPI). Yani ispatlar derlenemiyor, `lean_sorry` kapısı da
-- kendi çıktısında "⚠ lake build KOŞULMADI" demek zorunda kalıyordu.
-- Kaldırınca proje çevrimdışı derlenir ve ispatlar GERÇEKTEN tip-denetlenir.
-- ⚠ İleride bir dosya Mathlib'e ihtiyaç duyarsa bu satır geri gelir — ama o
--   zaman bağımlılık GERÇEK bir kullanıma dayanmış olur, bildirim artığına
--   değil.
-- ⚠ `lake-manifest.json` HÂLÂ mathlib/plausible girdilerini taşıyor. BİLEREK
--   DOKUNULMADI: `lake update` ağa çıkar ve bu ortamda tam da düşen şey odur.
--   `lake build` manifesti yok saydığı için çevrimdışı derleme çalışıyor
--   (ölçüldü: 33/33 iş, 45 sn). Manifest ağ erişimi olan bir ortamda
--   temizlenebilir; buradaki iddia "derleniyor", "manifest tutarlı" DEĞİL.
