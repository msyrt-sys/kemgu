# VirtIO-blk Config Space

## DOGRULAMA

- VAR: D3 `origin/main` icinde mevcut. `git log --oneline origin/main -5` ciktisinda `48533d4 Merge pull request #9 from msyrt-sys/drivers/virtio-queue-bind` ve `7cdec0e feat(drivers): bind split virtqueue to MMIO registers` goruldu.
- VAR: `drivers/virtio/virtqueue_bind.kem` ve `drivers/virtio/virtio_mmio.kem` `origin/main` uzerinde mevcut; queue address register sabitleri `drivers/virtio/constants.kem` icinde yer aliyor.
- YOK: `mmio_oku64`/`mmio_yaz64` bulunmadi. MMIO foundation yalniz `mmio_oku32`/`mmio_yaz32` sagliyor (`docs/drivers/mmio-foundation.md:21-26`, `src/tip_kontrol.c:2294-2295`). Bu PR capacity alanini iki 32-bit okuma ile `low | (high << 32)` olarak birlestirir.
- YOK: `VIRTIO_BLK_CFG_TABAN` ve virtio-blk config alan offsetleri daha once yoktu; bu PR `0x100` config tabani ile capacity, size_max, seg_max ve blk_size offsetlerini `constants.kem` icine ekledi.
- YOK: `ConfigGeneration` offset sabiti daha once yoktu; bu PR `VIRTIO_MMIO_CONFIG_GENERATION = 0x0fc` sabitini ekledi.

## Kapsam

Bu PR virtio-blk cihazina ozel config-space alanlarini okuyan `blk_yapilandirma_oku` yuzeyini ekler. Okunan veri `BlkYapilandirma` yapisinda tasinir: `kapasite_sektor`, `blok_boyut`, `size_max` ve `seg_max`. Capacity VirtIO-blk sozlesmesine gore 512-byte sektor birimindedir; 1 GiB host-mock disk testinde beklenen deger `2097152` sektordur.

Config okuma VirtIO generation protokolunu izler: `ConfigGeneration` once okunur, config alanlari okunur, generation tekrar okunur. Deger degismisse okuma bastan denenir. Dongu `VIRTIO_BLK_CFG_GENERATION_RETRY_MAX = 8` ile sinirlidir; bu sinir asilirsa `sonuc<BlkYapilandirma, tam32>` hata kodu olarak `VIRTIO_BLK_CFG_HATA_GENERATION_INSTABILITE` doner.

`mmio_oku64` olmadigi icin capacity `mmio_oku32` ile low/high iki parcadan okunur. `blk_size` yalniz `VIRTIO_BLK_F_BLK_SIZE` negotiate edildiyse anlamsal olarak gecerlidir; bu PR feature handshake'i tekrar yapmaz, sadece register degerini okur.

## Kapsam Disi

Full init dizisi (`blk_baslat`), request handling, DMA allocation, used-ring tuketimi, ISR ve custom enum hata tipi bu PR'in disindadir. Hata payload'i C2.7 custom enum destegi gelene kadar `tam32` kod olarak kalir.

## Test

Host-mock testleri config-space registerlarini `mmio_yaz32` ile seed eder ve `blk_yapilandirma_oku` sonucunu `esles` ile dogrular. Kapsanan senaryolar: 1 GiB disk (`capacity=2097152`, `blk_size=512`), high-word capacity birlestirme, generation flip sonrasi tek retry ile basari, 8 retry boyunca kararsiz generation hata yolu ve 64-bit birlestirme helper'i.

## Sonraki Adim

Bu PR D6 full `blk_baslat` akisini unblock eder: status handshake, feature negotiation, queue binding ve config okuma tek init dizisinde birlestirilebilir.
