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

## B. Erişim/atama
- ✅ struct alan oku+yaz tek/iç içe (x.a, a.b.c — audit + nested fix); stack `d[i]`
  oku+yaz (audit gap #2)
- ⏭️ **D-007:** struct-değerli diziler — `arr[i].alan` (stack: eleman-tip takibi yok;
  heap: KdlDizi skaler-only), `a.b[i].c`, `d[i][j]` çok-boyut. Feature/runtime, ertelendi.
- ⏭️ heap `d[i]=v` → D-003 · `*p=v` → T022-red (DOĞRULANDI, spec-doğru)

## C. İşaretçi/referans zincirleri
- ✅ `&v` (skaler/struct — &Struct fix), `*(&v)` round-trip, &-param mutasyon (sret yolu)
- ⏭️ **D-006:** `&p.x` → `(&p).x`, `&d[i]` → `(&d)[i]` — parser önceliği (ifade.c,
  SCOPE DIŞI). Codegen doğru AST'ye hazır. Ayrı parser görevi.
## D. Kontrol akışı — ✅ gap yok
- iç içe eğer/değilse (4-yol), iken+döngü-taşıyan birikim, ver erken-dönüş iç içe
  döngüde, ve/veya dal-koşulu kısa-devre, çeşit exhaustive eşleş (i8 dispatch).

## E. Fonksiyon sınırı — ✅ gap yok
- struct param+dönüş by-value, karşılıklı özyineleme, dizi param (`için`),
  aggregate (sonuç) dönüş + extractvalue, @modul.ad çağrı (codegen; T016 type-check
  ayrı), **yetki<R> param sınır pass-through**, **tekkez<T> param sınır round-trip**.
## F. Bölge/lineer/yetki etkileşimleri — ✅ (concurrency hariç)
- ✅ tekkez çağrıdan geçiyor + eşleş-kolunda tüketim (sonuç<tekkez<T>,H>); tekkez
  çağrı-zinciri tüketim; yetki<R> MMIO capability-gate round-trip + geri_al tüketimi;
  ÇAPRAZ capability+lineer birlikte (ikisi de tüketiliyor); LR002 struct-lineer-alan reddi.
- ⏭️ **D-008:** dondur/kanal/görev codegen YOK (concurrency runtime V2). "Lineer değer
  kanaldan geçiyor" buna bağlı. İŞARETLENDİ.

## stretch — ✅ (asm-struct hariç)
- ✅ generic (`$` yolu) instantiation round-trip; tek-varyant çeşit + eşleş; çeşit
  codegen yukarıdaki F/D hücrelerinde (sonuç<tekkez>, eşleş, exhaustive).
- ⏭️ **D-009:** satıriçi_asm çıktısı struct alanına (`&r.deger`) — parser çıktı clause
  düz &var only. Ertelendi.
