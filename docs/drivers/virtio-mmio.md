# VirtIO MMIO Transport

## DOGRULAMA

- VAR: MMIO API imzalari `src/tip_kontrol.c:2294-2295` ve `docs/drivers/mmio-foundation.md:21-26` icinde `mmio_oku32(y: yetki<MMIO>, adres: tam64) -> tam32` ve `mmio_yaz32(y: yetki<MMIO>, adres: tam64, deger: tam32)` olarak dogrulandi. Kullanim ornegi `test/ornekler/mmio_smoke.kem:38-40` ve `test/ornekler/mmio_smoke.kem:49-63`.
- VAR: `yetki<MMIO>` `yetki_olustur(6, 3)` ile elde ediliyor; 6 MMIO kaynak id, 3 OKU|YAZ izni. Kanit: `docs/drivers/mmio-foundation.md:31-33`, `test/ornekler/mmio_smoke.kem:29-30`, `test/ornekler/mmio_smoke.kem:71`.
- VAR: `mmio_oku32` ve `mmio_yaz32` yetkiyi odunc aliyor; tek yetki ile birden cok register erisimi yapiliyor ve scope sonunda `geri_al(y)` zorunlu. Reassignment yerine ayri binding kuralinin aciklamasi `docs/drivers/mmio-foundation.md:55-88`.
- VAR: `sabit` codegen duzeltmesi `docs/drivers/mmio-foundation.md:166-187` icinde belgeli. Register offsetleri bu PR'da `constants.kem` icinde named `sabit` olarak tutuluyor.
- VAR: C1/C2 `origin/main` uzerinde dogrulandi: `6d01649 fix(codegen): rename diagnostic prefix kdl: -> kemgu:` ve `329ca26 fix(codegen): lower esles with terminated case blocks`.
- VAR: Kucuk `esles` ornegi MSYS2 PATH ile `--check`, `--llvm`, `opt -passes=verify` ve native calisma ile dogrulandi; `RUN_EXIT=20` dogru kolun calistigini gosterdi ve LLVM ciktisinda `deyim tipi`, `desteklenmiyor`, `internal-codegen-error` veya `kemgu:` yoktu.
- VAR: Eski workaround `virtio_mmio_status_gecis_runtime_gecerli_mi` icindeydi ve `mevcut == 3` / `mevcut == 11` kollarini kullaniyordu. Bu kopya kaldirildi; transport artik `virtio_status_gecis_gecerli_mi` cagiriyor.
- YOK: KEMGU parser'i custom `secimlik DurumDurumu { ... }` enum soz dizimini desteklemiyor. Ayrica identifier pattern'leri constant pattern degil binding olarak davraniyor. Yeni dil ozelligi eklenmedi; durumlar named state tag sabitlerine map edilip boolean `esles` guard'lariyla ifade edildi.

## Kapsam

Bu PR saf VirtIO modellerinin uzerine gercek VirtIO MMIO v2 transport katmanini ekler. Hedef pencere QEMU `virt` slot 0 tabani olan `0x0A000000` adresidir. Probe akisi `magic`, `version`, `device_id` ve `vendor_id` registerlarini `mmio_oku32` ile okur; magic `0x74726976`, version `2` ve device id `2` ise seri hatta `[virtio-mmio] device detected: magic=0x74726976 version=2 device_id=2` satirini basar.

Status handshake gercek status register'i uzerinden `RESET -> ACKNOWLEDGE -> DRIVER -> FEATURES_OK -> DRIVER_OK` sirasiyla ilerler. Her adimda once `status.kem` icindeki named state tag haritasi ve boolean `esles` guard'lariyla gecis dogrulanir, sonra kumulatif status register'a yazilir. Basarisizlik durumunda panik yoktur; `FAILED` yazilir, capability `geri_al` ile iade edilir ve `sonuc` hata degeri doner.

Feature negotiation, device feature low word'unu gercek register'dan okur ve `features.kem` icindeki `accepted = device & supported` kuralini kullanir. Driver feature register'lari yazildiktan sonra `FEATURES_OK` set edilir ve status register geri okunur. `FEATURES_OK` biti hala set degilse cihaz feature subset'ini reddetmis kabul edilir.

Queue kesfi bu PR'da yalnizca `QUEUE_SEL=0` yazip `QUEUE_NUM_MAX` okumakla sinirlidir. Okunan deger `virtqueue_layout.kem` icindeki power-of-two, max-size ve alignment modeline baglanir. Descriptor allocation, desc/avail/used adres register'larini yazma, interrupt, ISR ve request handling kapsam disidir.

## Test

Host-mock test dosyasi foundation runtime MMIO tamponunu kullanarak register yaz-oku round-trip, probe, status handshake, feature negotiation ve queue discovery hata yollarini dogrular. Testler native calisma yolunda `VirtioMmioInitDurum` cekirdegini kullanir; public `virtio_mmio_transport_init` API'si ise kernel tarafinda kullanilacak `sonuç<VirtioMmioInitSonuc, metin>` wrapper'ini korur. QEMU boot testi en iyi caba olarak kalir; bu PR'in deterministic dogrulama hatti `build/kemgu.exe --parse`, `--check`, `--llvm` ve LLVM ciktisinda `HATA` aramasidir.
