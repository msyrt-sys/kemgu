# Virtqueue Register Binding

## DOGRULAMA

- VAR: C2.5 sonuc/secimlik codegen `origin/main` icinde `52b0cf3` merge'iyle geldi; gecmis logda `8f7a556 feat(codegen): tagged-union value lowering for sonuc/secimlik` goruldu. Kucuk `sonuc` smoke'u `--check`, `--llvm` ve `opt -passes=verify` ile aggregate `{i8, ...}` donus uretip `@tamam`/`@hata` cagirmadan dogrulandi.
- VAR: VirtIO queue register offsetleri `drivers/virtio/constants.kem` icinde QueueSel `0x030`, QueueNumMax `0x034`, QueueNum `0x038`, QueueReady `0x044` olarak vardi. Bu PR QueueDescLow/High `0x080/0x084`, QueueDriverLow/High `0x090/0x094` ve QueueDeviceLow/High `0x0a0/0x0a4` sabitlerini ekledi.
- VAR: `drivers/virtio/virtqueue_layout.kem` queue max sifir, queue size sifir, max ustu, power-of-two olmayan boyut, ust limit ve desc/avail/used alignment kurallarini saglar.
- VAR: Cross-file `virtqueue_layout_gecerli_mi` cagrisinin `--check`, `--llvm` ve `opt -passes=verify` smoke'u temizdir; inline workaround gerekmedi.
- YOK: KEMGU su an `&yetki<MMIO>` referansini kabul etmiyor ve `bos` tipi `sonuc<bos, ...>` generic parametresi olarak rahat kullanilamiyor. Ayrica `sonuc` icinde donen linear yetki eslesme sonrasi coklu MMIO okumalarda odunc gibi islenmiyor. Yeni dil ozelligi eklenmedi; `virtqueue_bagla` yetkiyi sahiplenip tek noktada iade eder ve basarida secilen `queue_num` degerini `sonuc<tam32, tam32>` ile dondurur.

## Kapsam

Bu PR VirtIO 1.2 bolum 4.2.4 split virtqueue register baglama adimini saf allocation olmadan gercek MMIO primitive'leriyle modeller. `virtqueue_bagla` once `QueueSel` register'ina queue index yazar, sonra `QueueNumMax` okur. Cihaz queue'yu sunmuyorsa hata doner; sunuyorsa `queue_num = min(vq.boyut, QueueNumMax)` secilir ve mevcut `virtqueue_layout.kem` modeliyle boyut/alignment dogrulanir.

Dogrulamadan sonra `QueueNum`, descriptor table, available ring ve used ring fiziksel adresleri low/high 32-bit register ciftlerine yazilir. Son adimda `QueueReady=1` yazilir. KEMGU tarafinda store barrier intrinsic'i henuz yoksa diye kodda `TODO(codex)` notu birakildi; compiler/runtime alanina dokunulmadı.

## Kapsam Disi

Descriptor/avail/used DMA allocation, `tekkez` tabanli gercek buffer sahipligi, request handling, used-ring tuketimi, interrupt/ISR ve tam virtio-blk init bu PR'in disindadir. Bu PR yalnizca hazir fiziksel adresleri register penceresine baglar.

## Test

Host-mock testleri `yetki_olustur(6, OKU|YAZ)` ile foundation MMIO tamponunu hazirlar ve queue index `0..7` icin `QueueSel`, `QueueNum`, adres low/high registerlari ve `QueueReady` readback'ini dogrular. Basari sonucu secilen `queue_num` degerini de tasir. Ayrica `QueueNumMax=0`, power-of-two olmayan queue size, min secimi ve bozuk alignment hata yollari test edilir.

## Sonraki Adim

Bu PR, D4/D5 konfig okumalari ve D6 tam `blk_baslat` akisi icin queue register binding yuzeyini hazirlar. Sonraki PR descriptor allocation ve `tekkez` sahiplik modelini ekleyebilir.
