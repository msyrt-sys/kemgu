# VirtIO Feature Negotiation

Bu PR VirtIO feature negotiation akisini saf KEMGU modeli olarak ekler. Model
gercek MMIO register okuma/yazma yapmaz; `FEATURES_OK` status bitinin yazilmasi
ve cihazdan geri okunmasi kapsam disidir. Amac, cihaz tarafindan sunulan feature
maskesi ile driver tarafindan desteklenen ve zorunlu gorulen feature maskelerini
donanimdan bagimsiz olarak dogrulamaktir.

Temel kural dogrudan VirtIO init akisini izler: kabul edilen maske
`accepted = device_features & driver_supported` olarak hesaplanir. Bu maske,
cihazin sundugu ama driver'in bilmedigi bitleri ve driver'in destekledigi ama
cihazin sunmadigi bitleri otomatik olarak disarida birakir. Modelin ana
yardimcilari `virtio_feature_kabul_maskesi`,
`virtio_feature_zorunlu_karsilaniyor_mu` ve
`virtio_feature_negotiation_gecerli_mi` fonksiyonlaridir.

Zorunlu feature kontrolu su invariant uzerinden yapilir: driver'in gerekli
gordugu tum bitler accepted maske icinde bulunmalidir. Baska bir ifadeyle
`driver_required & accepted == driver_required` olmali. Zorunlu bit driver'in
`driver_supported` maskesinde yoksa model `FeatureDestekDisiZorunlu` hatasini,
driver desteklese bile cihaz sunmuyorsa `FeatureZorunluEksik` hatasini dondurur.
`driver_required == 0` oldugunda accepted maske bos bile olsa negotiation
gecerli kabul edilir; opsiyonel feature'lar olmadan calisabilen driver icin bu
beklenen davranistir.

VirtIO-blk feature isimleri bu PR'da sembolik deger fonksiyonlari olarak
tutuldu: `VIRTIO_BLK_F_SIZE_MAX`, `SEG_MAX`, `BLK_SIZE`, `FLUSH`, `RO`, `MQ`,
`DISCARD` ve `WRITE_ZEROES`. Gercek virtio-blk init, queue register yazimi,
DMA allocation, ISR ve request handling sonraki PR'larin konusudur.

Sonraki dogal adim, MMIO transport'a gecmeden once mock transport negotiation
akisini yazmak veya Claude foundation sonrasinda gercek VirtIO MMIO transport
init'ine bu saf modeli baglamaktir.
