# Codegen Kampanya Matrisi — İlerleme

> **[D-638] BU BELGE TARİHSEL BİR KAYITTIR — canlı yapılacaklar listesi DEĞİLDİR.**
> Kampanya kapandı. Aşağıdaki ⏭️ *"ertelendi"* maddelerinin **beşte dördü o
> tarihten sonra kapandı** ama belge güncellenmediği için yıllarca açık iş gibi
> okundu. 2026-09-28'de tek tek **ölçüldü** (tahmin edilmedi) ve satır satır
> düzeltildi. **Canlı kuyruk `LOOP.md` → "Sirada" bölümündedir.**

Yöntem: her hücre → inşa et + işlet + **gözlemlenebilir değer assert** (salt opt-verify
yetersiz). Gap → blanket fix + DECISIONS_LOG + kalıcı runtime regresyon (test_llvm).

Durum: ✅ yeşil/gap-yok · 🔧 gap bulundu+fix · ⏭️ kapsam dışı (DECISIONS_LOG) · ⬜ bekliyor

## Seed (önceki)
- ✅ a — modül `@modul.ad` mangling (D-001) · b — ve/veya kısa-devre (D-002)
- ✅ c — heap `d[i]=v` (D-003) **KAPANDI** · d — LAMBDA V2 (D-004) **KAPANDI**
  — [D-638'de ölçüldü] D-003/D-007 ile aynı kanıt (`cg_yapi_dizi.kem`); kapanış
  uçtan uca koşuyor (`|| k + 2` → exit 42) ve korpusta 10 kapanış/lambda dosyası var
  (`cg_kapanis_*`, `cg_gorev_lambda_blok`).

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
- ✅ **D-007 KAPANDI** [D-638'de ölçüldü]: `cg_yapi_dizi.kem` (D-342) `Dizi<Nokta>`
  üzerinde indeks okuma **ve yazma**, `ps[0].x` alan erişimi ve `için` gezinmeyi
  kapsıyor; `codegen_diff` 179/179 yeşil. Aşağıdaki eski gerekçe artık geçersiz:
  ~~struct-değerli diziler — `arr[i].alan` (stack: eleman-tip takibi yok;
  heap: KdlDizi skaler-only), `a.b[i].c`, `d[i][j]` çok-boyut. Feature/runtime, ertelendi.
- ✅ heap `d[i]=v` → D-003 **KAPANDI** (yukarıdaki kanıt) · `*p=v` → T022-red
  (DOĞRULANDI, spec-doğru — bu kasıtlı, kapanacak bir şey değil)

## C. İşaretçi/referans zincirleri
- ✅ `&v` (skaler/struct — &Struct fix), `*(&v)` round-trip, &-param mutasyon (sret yolu)
- ✅ **D-006 ÇÖZÜLDÜ** (ifade.c, ayrı görev): `&p.x`=`&(p.x)`, `&d[i]`=`&(d[i])`,
  `&a.b.c` — postfix prefix'ten sıkı; prefix operandı `parse_oncelik(ONC_ONEK)`.
  deref-oku round-trip yeşil, segfault yok. (`&p.x` YAZ → `*p=v` T022-red, ayrı;
  `&arr[i].alan` codegen D-007 bloklu.)
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
- ✅ **D-008 KAPANDI** [D-638'de ölçüldü]: `cg_gorev_*` korpusu (`cg_gorev_baslat`,
  `cg_gorev_capture`, `cg_gorev_bagli_kapanis`, …) codegen'i kapıyor; bare-metal
  tarafı da D-623/624 ile kem_os faz [41]/[42]. Eski not: ~~dondur/kanal/görev codegen YOK (concurrency runtime V2). "Lineer değer
  kanaldan geçiyor" buna bağlı. İŞARETLENDİ.

## stretch — ✅ (asm-struct hariç)
- ✅ generic (`$` yolu) instantiation round-trip; tek-varyant çeşit + eşleş; çeşit
  codegen yukarıdaki F/D hücrelerinde (sonuç<tekkez>, eşleş, exhaustive).
- ⏭️ **D-009 — TEK GERÇEKTEN AÇIK MADDE.** 2026-09-28'de doğrulandı: hâlâ
  reddediliyor (P264). Canlı kuyrukta **`LOOP.md` → Sirada → C3** olarak izleniyor;
  bu belge tarihsel olduğu için iş oradan sürülür.
  satıriçi_asm çıktısı struct alanına (`&r.deger`) — parser çıktı clause
  düz &var only. Ertelendi.
