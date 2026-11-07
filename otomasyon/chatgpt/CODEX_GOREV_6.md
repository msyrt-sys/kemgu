# CODEX GÖREVİ 6 — xHCI enumerasyonu + USB bellek + HID klavye, GERÇEK QEMU'ya karşı

Dalga 1'in sürücü kuralları geçerlidir (`otomasyon/chatgpt/CODEX_GOREV.md` §2, §3, §5 —
yeniden oku). **Bu dalgadaki fark:** kâhin sahte model DEĞİL, gerçek QEMU aygıtıdır.
Soru sorma, durma; takılırsan (60 dk) `codex6_bulgular.md`'ye yaz ve geç.

## 0. Neden bu dalga farklı (ÖNCE OKU)
`D-742`: dalga 4'ün AHCI sürücüsü sahte modelinin tüm testlerini geçiyordu ama gerçek
QEMU AHCI'de HİÇBİR diski görmüyordu (imza, FIS alımı açılmadan okunuyordu; sahte model
imzayı hemen döndürüyordu). Sahte model, sürücüyle AYNI yanlış anlamayı taşıyabilir.
Bu dalgada her sürücü adımı **önce gerçek QEMU'da** çalıştırılır; sahte model, ölçülen
gerçek davranışa göre yazılır/düzeltilir (CI'da QEMU yok → sahte model orada koruma).

Okunacaklar: `tests/qemu/qtest.kem` (qtest istemcisi + `qt_yurut` sans-IO yürütücüsü +
PCI tarama/BAR atama), `tests/qemu/nvme_qtest.kem` ve `ahci_qtest.kem` (örnek program),
`test/qtest_harness.sh` (örnek harness), `LOOP.md` D-741/D-742 kayıtları,
`otomasyon/chatgpt/codex4_bulgular.md` "QEMU'da doğrulanacak — xHCI / USB BOT".

## 1. Ön koşul ve çalışma alanı
```bash
cd /home/msyrt/Desktop/kemgu && git fetch origin
git log origin/main --oneline | grep D-743     # yoksa 5 dk'da bir, en çok 2 sa; sonra DUR, kaydet
git worktree add ../kemgu-codex6 -b codex/usb-dalga6 origin/main && cd ../kemgu-codex6
make -j8 build/kemgu build/codegen build/kdl_runtime.o build/kdl_runtime_mmio.o
make calistir_qtest                              # nvme + ahci GEÇMELİ (taban sağlam mı)
date > otomasyon/chatgpt/codex6_ilerleme.md
```
Tek dal, tek draft PR.

## 2. Ölçülmüş QEMU tarifi (Claude, Spark, QEMU 8.2.2)
- `-accel qtest` ve `clock_step` bu QEMU'da YOK. NVMe/AHCI için `-accel tcg -S` yetiyordu.
- **xHCI + klavye için VM ÇALIŞMALI:** QMP `input-send-event` duraklatılmış VM'de
  "VM not running" verir (ölçüldü). SeaBIOS çalışırsa xHCI'yi KENDİSİ başlatır ve
  sürücüye karışır. Çözüm (ölçüldü, çalışıyor): yalnız `hlt; jmp` olan 64 KiB sahte BIOS —
  ```python
  b = bytearray(65536); b[0xFFF0:0xFFF3] = bytes([0xF4, 0xEB, 0xFD]); open("dur.bin","wb").write(b)
  ```
  `-bios dur.bin` (`-S` YOK). qtest erişimi VM çalışırken de çalışıyor; sanal saat ilerler.
- Aygıtlar: `-device qemu-xhci,id=x -device usb-storage,bus=x.0,drive=u0
  -drive file=…,if=none,id=u0,format=raw -device usb-kbd,bus=x.0`.
- Tuş basımı: `-qmp tcp:127.0.0.1:PORT,server=on,wait=off`, önce `qmp_capabilities`, sonra
  `{"execute":"input-send-event","arguments":{"events":[{"type":"key","data":{"down":true,"key":{"type":"qcode","data":"a"}}}]}}`
  (bırakma için `down:false`).

## 3. İzinli yollar (dalga 1'e ek)
`tests/qemu/**`, `test/qtest_harness.sh` (YALNIZ yeni `case` dalları + ortak yardımcı;
mevcut `nvme`/`ahci` dalları DAVRANIŞÇA aynı kalır), Makefile'da YALNIZ
`calistir_qtest` hedefinin satırları ve `CHATGPT_TESTLER`. `drivers/**` ve
`tests/drivers/**` (sahte modeller dahil) her zamanki gibi. Derleyici/runtime YASAK.

## 4. İş listesi
- **G0 (T1 dalga 5'ten):** dalga 4 dolanmalarını kaldır (F001–F005 artık main'de): `ahci_`
  önekli özel yardımcılar (F005), açık alias'lar, `oku_u8(...) olarak dtam8` kopyaları (F003),
  ara `değişken` atamaları (F002/F004). Kapanmayanın dolanması kalır, yorumda nedeni.
  `make calistir_chatgpt_kopru` + `make calistir_qtest` yeşil.
- **G1 `drivers/usb/xhci_cekirdek.kem` (sans-IO enumerasyon):** HCRST → CNR=0 bekle →
  CONFIG.MaxSlotsEn → DCBAA (+ HCSPARAMS2'ye göre scratchpad tamponları) → komut halkası
  (CRCR) → olay halkası (ERST tablosu, ERSTSZ, ERSTBA, ERDP) → USBCMD.R/S=1 → bağlı her
  port için PORTSC güç/sıfırlama (PR, PRC bekle, hız) → Enable Slot → Address Device
  (giriş bağlamı: yuva + EP0; EP0 azami paketi hıza göre) → GET_DESCRIPTOR aygıt (önce 8
  bayt, sonra 18) → yapılandırma (toplam boy kadar) → SET_CONFIGURATION → Configure
  Endpoint (bulk IN/OUT, interrupt IN). Olaylar döngü biti ile tüketilir; tamamlanma kodu
  1 = başarı, 13 = kısa paket (IN'de geçerli), diğerleri → `Hata`. Var olan
  `drivers/usb/{tanim,xhci_kayit,xhci_trb}.kem` kullanılır; çelişen bit gerçek QEMU'da
  ölçülerek düzeltilir ve `codex6_bulgular.md`'ye "ölçüldü: X" diye yazılır.
- **G2 `tests/qemu/xhci_qtest.kem` + harness `xhci`:** gerçek QEMU xHCI'de iki aygıtı
  (usb-storage + usb-kbd) enumere et; her birinin VID/PID, sınıf ve uç noktalarını bas.
  Beklenen değerleri UYDURMA: önce ölç, sonra QEMU kaynağının bilinen değerleriyle
  (`/usr/share/hwdata/usb.ids`) tutarlılığını kaydet.
- **G3 USB bellek gerçek QEMU'da (`usb_depo`):** mevcut `drivers/usb/depo.kem` (BOT+SCSI)
  G1'in kurduğu bulk uçları üzerinden. E2E: harness `mkfs.fat -F 32` (Python ile GPT
  bölümü içinde ya da düz imaj) hazırlar → KEMGU INQUIRY + READ CAPACITY + FAT32'yi bağlar
  (`drivers/fs/*`) → bir dosya yazar → harness QEMU kapandıktan sonra `fsck.fat -n` +
  bağımsız Python FAT okuyucusuyla (dalga 1 F1'deki) doğrular.
- **G4 HID klavye gerçek QEMU'da (`usb_kbd`):** interrupt IN uç noktası + `drivers/usb/
  hid_klavye.kem` → `drivers/ps2/klavye_tr.kem`. Program "KLAVYE_HAZIR" basar; harness
  bunu görünce QMP ile "Ağaç İş" + Enter yazar (Türkçe Q konumları: ğ = `bracket_left`,
  ç = `dot`, ş = `semicolon`, İ = Shift + `apostrophe`, ı/I tuşu = `i` — qcode adlarını
  QEMU'nun `qapi/ui.json` sözlüğünden doğrula, uydurma). Program okunan kod noktalarını
  basar; harness karşılaştırır.
- **G5 (zaman kalırsa) HID fare:** `usb-mouse`, QMP `rel` olayları → `FareOlayi`, işaret
  (HID dy pozitif = aşağı) gerçek QEMU'da ölçülerek doğrulanır.
- **G6 sahte modelleri gerçeğe uydur:** G1–G4'te gerçek QEMU'nun sahte modelden farklı
  davrandığı her nokta için sahte modeli düzelt (D-742 deseni) + o farkı yakalayan bir
  sahte test ekle. Liste `codex6_bulgular.md` "SAHTE MODEL DÜZELTİLDİ" başlığında.

## 5. Kapılar
Her işten sonra `make calistir_chatgpt_kopru` ve `make calistir_qtest` (yeni dallar dahil).
Her qtest dalı C ve self-host ikilisini AYRI QEMU örneklerine karşı koşar (nvme deseni)
ve tek dal ≤ 60 sn sürer. Bitirince `make test_tumu` (rc PIPESTATUS ile, atlamalar sayılmış).
Her iş için sabotaj: ilgili sürücü kuralını boz → `calistir_qtest` KIRMIZI; mümkünse
sahte test de KIRMIZI (G6 bunu sağlamalı).

## 6. Bitirince
Draft PR `codex/usb-dalga6`, gövde `codex6_ilerleme.md`; `codex6_bulgular.md`: ölçülen
QEMU davranışları, sahte model düzeltmeleri, derleyici bulguları, kalan doğrulanmamışlar.
Auto-merge AÇMA, merge ETME.
