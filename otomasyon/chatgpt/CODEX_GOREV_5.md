# CODEX GÖREVİ 5 — derleyici kusurları F001–F005 (dalga 4 bulguları)

Dalga 2'nin kuralları AYNEN geçerlidir: `otomasyon/chatgpt/CODEX_GOREV_2.md` §0
(oku), §3 (DOKUNMA kuralı), §4 (her kusur için döngü), §5 (yasaklar), §6 (bitirince).
**Yeniden oku.** Kusur başına ayrı dal + draft PR, en sonda özet PR. Soru sorma, durma;
bir kusurda 60 dk takılırsan `codex5_kararlar.md`'ye yaz, sıradakine geç.

## 0. Ön koşul ve çalışma alanı
```bash
cd /home/msyrt/Desktop/kemgu && git fetch origin
git log origin/main --oneline | grep D-740     # yoksa 5 dk'da bir, en çok 2 sa; sonra DUR ve kaydet
git worktree add ../kemgu-codex5 --detach origin/main && cd ../kemgu-codex5
```
Her kusur: `git fetch origin && git checkout -B codex/d5-<kod> origin/main`.
Kaynak: `otomasyon/chatgpt/codex4_bulgular.md` (senin raporun) ve `LOOP.md` E4g.

## 1. Sıra (sessiz yanlış önce)

### F003 — SESSİZ YANLIŞ (self-host)
Yapı ALANINDAKİ `Dizi<dtam8>`'den `dizi_al` işaretle genişliyor (128 → -128; C doğru).
Kardeşi C004'ü dalga 2'de sen düzelttin (`selfhost/codegen.kem` `ifade_isz` CAGRI dalı,
"C004" ara): yalnız kaynak `TANIMLAYICI` iken eleman işaretsizliğini buluyor; kaynak
`ERISIM` (`s.b`) açık. Alanın eleman tipinden işaretsizliği oku (D-339 `alan_isz`
deseni orada). Fikstür `test/cg_korpus/`: 128, 200, 255 (üç ayrı değer) + iç içe
alan (`a.b.c`) + POZİTİF işaretli `Dizi<tam8>` alanı (-1 kalmalı).

### F005 — TASARIM: aynı adlı modül dosyaları çakışıyor (C + self)
`drivers/nvme/cekirdek.kem` ve `drivers/ahci/cekirdek.kem` birlikte ithal edilince
iki derleyici de `@cekirdek.bekle` üretir → `invalid redefinition`. Kök D-584: dosya
modülünün öneki SON yol parçasına kırpılıyor.

**KARAR (Claude, ölçüme dayalı):** dosya modülü, yolundaki her parça iç içe bir modülmüş
gibi adlandırılır. Satır içi iç içe modüller bunu ZATEN yapıyor; iki derleyicide ölçüldü:
```
modül drivers { modül nvme { modül cekirdek { genel işlev bekle() ... } } ... }
  C    : define i32 @drivers.nvme.cekirdek.bekle
  SELF : define i32 @drivers.nvme.cekirdek.bekle
```
Yani `kullan drivers::nvme::cekirdek::{bekle}` → `@drivers.nvme.cekirdek.bekle`.
Yeni şema YOK; D-398'in iç içe modül kuralı dosya modüllerine uzatılır.
Gereksinimler:
1. Benzersiz: farklı dizinlerdeki aynı adlı dosyalar çakışmaz.
2. Kararlı: bir modülün adı, programda başka neyin ithal edildiğine bağlı DEĞİL
   (ileride ayrık derleme — LOOP D7 — buna dayanacak; "yalnız çakışınca uzat" YASAK).
3. C ve self BİREBİR (`calistir_modul_codegen` önek kümesi ölçümü, D-584).
4. Tek parçalı yol (`kullan mat;`) bugünkü adı korur (`@mat.f`).
5. Ad çözümleme (seçili/alias/nitelikli/T041 gizlilik — D-581, D-590) DEĞİŞMEZ;
   yalnız üretilen IR adı değişir.

Self-host'ta modül kaynağı `modül <ad> { … }` sarmalıyla birleştiriliyor (D-399) ve
`son_segment` 13 yerde geçiyor. Önce hepsini envanterle (hangisi IR adı, hangisi ad
çözümleme). Aynı önekli iki dosya (`drivers/nvme/a.kem`, `drivers/nvme/b.kem`) iç içe
sarmalda yinelenen `modül drivers` üretmemeli (birleştir ya da IR adını doğrudan kur —
seçimi ölçerek yap, kaydet). C tarafı `src/llvm.c` `modul_mangle` (D-584 kırpması).
Fikstür `test/moduller/`: iki dizinde aynı adlı dosya + aynı adlı özel ve genel
işlevler, alias ve seçili ithalat, exit 42; tek parçalı modül pozitif. Kapılar:
`modul_codegen`, `surucu_diff`, `checker_diff`, `baremetal_diff`, tam takım.
**2 saati aşarsa** dur: envanter + tasarım notu + takıldığın yer `codex5_kararlar.md`'ye.

### F002 — iki derleyici LINK-RED
`s.x = değer(7 olarak tam64);` (yapı alanına yapıcı ataması). D-465 yapıcıları
argüman ve yerel atama konumunda çözdü; **alan lvalue** kolu açık. Beklenen tip alanın
tipinden gelmeli. `hiç`, `değer`, `tamam`, `hata` dördü de; iç içe alan; dizi elemanı
(`xs[0] = değer(..)`) da ölç. Fikstür `test/cg_korpus/`.

### F004 — C LINK-RED
`sonuç<Dizi<Cesit>, metin>` → `tamam(xs)` → `eşleş dizi_al(xs, 0)` → C `extractvalue i32`.
Dalga 2'de C003'ü self'te düzelttin (sonuç payload'ının eleman tipi bağlamaya taşınır);
bu, C kolu. Fikstür `test/cg_korpus/`.

### F001 — C sahte T011
`kullan drivers::acpi::tablo::{ Rsdp }; yapı K { r: seçimlik<Rsdp>; }` → C T011, self OK.
Önce ölç: `r: Rsdp` (sarmalsız) da reddediliyor mu? Seçili ithal tipin yapı alanı
tipinde çözülmesi bir FAZ sorunu olabilir (C007'yi sen düzelttin: imzalar tiplerden
sonra; yapı alanları?). Bilinçli değilse düzelt; bilinçliyse DOKUNMA kuralı.

## 2. Bitince (zaman kalırsa)
- **T1** Dalga 4 dolanmalarını kaldır (dalga 3'teki §4 deseni): F001–F005 için
  sürücülerde/testlerde yapılan dolanmaları, kusur kapandıysa asıl biçime çevir
  (`ahci_` önekli özel yardımcılar, açık alias'lar, `oku_u8(...) olarak dtam8`
  kopyaları, ara `değişken` atamaları). `make calistir_chatgpt_kopru` yeşil.
  Kapanmayan kusurun dolanması kalır, yorumda nedeni.

## 3. Kapılar (her PR)
`CODEX_GOREV_2.md` §4.6 listesi + `make test_tumu` (rc PIPESTATUS ile, atlama ve
başarısızlık sayılarak). FIXPOINT ✓ şart. F005 ek olarak: `calistir_baremetal_diff`
ve `calistir_kem_os_x86` (bare-metal adları bozulmamalı).
