# VirtIO-blk Init Dizisi

Bu not D6 kapsamındaki saf olmayan, fakat host-mock üzerinde test edilen
`blk_baslat` akışını özetler. Amaç request handling yazmak değil; D1
`virtio_mmio`, D3 `virtqueue_bagla` ve D4/D5 `blk_yapilandirma_oku` parçalarını
VirtIO 1.2 bölüm 3.1.1 sırasıyla birleştirmektir.

## Doğrulama

- C2.7 `origin/main` üzerinde görüldü: `ee5f0f7` merge commit ve `2948094`
  `çeşit`/`boş` commit'i mevcut.
- Minimal `çeşit Deneme { A, B }` testinde yalnız `A` kolu yazılan `eşleş`,
  `--check` ile `M001` verdi. Exhaustiveness aktif.
- Minimal `sonuç<boş, Deneme>` + `tamam(boş)` testi `--check` ve `--llvm`
  geçti; LLVM çıktısında `HATA:`, `tanimsiz`, `@tamam`, `@hata` yoktu.
- Cross-file `kullan` ile `virtio_mmio_adres`, `virtqueue_adres_low`,
  `blk_cfg_u64_birlestir` ve `virtio_status_ekle` çağrıları `--check` ve
  `--llvm` geçti.
- Normal fonksiyona `yetki<MMIO>` geçirmek move olduğu için aynı `y` ile
  ardışık D1/D3 çağrısı `CP005` verdi. `delege(y, 3)` alt-yetki deseni ise
  `--check` ve `--llvm` temiz geçti; bu PR mevcut tüketen helper'ları ana
  yetkiyi kaybetmeden bu şekilde çağırır.

## Akış

`blk_baslat(y, taban)` public API'dir ve `sonuç<BlkAygit, BaslatHatasi>`
döndürür. Hata tipi ilk gerçek `çeşit` kullanımıdır: `AygitYok`,
`ModernDegil`, `BlokDegil`, `FeatureRed`, `QueueKurulumBasarisiz` ve
`YapilandirmaHatasi`. Catch-all varyant yoktur; testler sonuçları exhaustive
`eşleş` ile tüketir.

Init sırası şöyledir: D1 probe, status reset, ACKNOWLEDGE, DRIVER, feature
negotiation, FEATURES_OK yazımı ve readback, D3 queue bind, D4/D5 config read,
son olarak DRIVER_OK. Feature eksikliği veya FEATURES_OK readback reddi
`FeatureRed` olarak döner. Queue bağlama hataları artık `BagHatasi` üzerinden
exhaustive eşleşip dışarı `QueueKurulumBasarisiz` olarak yansır; config
generation stabil olmaması `YapilandirmaHatasi::GenerationInstabilite`
üzerinden dışarı `BaslatHatasi::YapilandirmaHatasi` olarak ayrılır. Her hata
yolunda status register'a FAILED yazılır ve `yetki<MMIO>` iade edilir.

## Kapsam Dışı

Bu PR gerçek DMA allocation, request descriptor zinciri, used ring tüketimi,
interrupt/ISR ve QEMU boot entegrasyonu yazmaz. Varsayılan virtqueue adresleri
önceki host-mock fazındaki sabit test adresleridir. Bir sonraki doğal adım,
gerçek DMA sahipliği ve request handling öncesinde queue allocation modelinin
`tekkez` ile bağlanmasıdır.
