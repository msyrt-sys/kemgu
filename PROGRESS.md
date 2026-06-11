# Codegen Kampanya Matrisi — İlerleme

Yöntem: her hücre → inşa et + işlet + **gözlemlenebilir değer assert** (salt opt-verify
yetersiz). Gap → blanket fix + DECISIONS_LOG + kalıcı runtime regresyon (test_llvm).

Durum: ✅ yeşil/gap-yok · 🔧 gap bulundu+fix · ⏭️ kapsam dışı (DECISIONS_LOG) · ⬜ bekliyor

## Seed (önceki)
- ✅ a — modül `@modul.ad` mangling (D-001) · b — ve/veya kısa-devre (D-002)
- ⏭️ c — heap `d[i]=v` (D-003) · d — LAMBDA V2 (D-004)

## A. Tipler × operatörler
- 🔧 **D-005 [YÜKSEK]:** dtamN (işaretsiz) tamamen işaretli lowering ediliyordu —
  `dtam8 200 > 100` signed `icmp`'le YANLIŞ (`-56 > 100`), `/` `sdiv`, `>>` `ashr`.
  Ayrıca **i1 genişletme `sext`'ti** → `doğru olarak tam32` = `-1` (41+(-1)=40, beklenen 42).
  Fix: IfadeSonuc/LlvmIsim/IslevKayit `isaretsiz` yan-kanalı; udiv/urem/lshr/u-pred;
  i1 + dtamN her zaman zext. Probe: a1-a11 hepsi 42/doğru.
- ✅ tam8/16/32/64 işaretli aritmetik+karşılaştırma+bit+taşma+`~`+mod/neg (a5,a6,a10)
- ✅ tam64 geniş (a7), kesirli64 aritmetik+karşılaştırma (a8)

## B. Erişim/atama — ⬜
## C. İşaretçi/referans zincirleri — ⬜
## D. Kontrol akışı — ⬜
## E. Fonksiyon sınırı — ⬜
## F. Bölge/lineer/yetki etkileşimleri — ⬜
## stretch — ⬜
