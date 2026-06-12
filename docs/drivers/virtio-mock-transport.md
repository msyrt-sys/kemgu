# VirtIO Mock Transport Negotiation

Bu PR gercek bir VirtIO transport implementasyonu degildir. Amac, MMIO
register erisimine gecmeden once cihaz baslatma akisini saf KEMGU modeliyle
bir araya getirmektir. Mock transport register okumaz veya yazmaz; bunun yerine
`magic`, `version`, `device_id`, feature maskeleri ve queue boyutu gibi degerleri
bir yapida tasir ve onceki saf modelleri kullanarak dogrular.

Bu model uc onceki parcayi birlestirir. Status state machine, init sirasinin
`RESET -> ACKNOWLEDGE -> DRIVER -> FEATURES_OK -> DRIVER_OK` seklinde
kumulatif ilerledigini denetler ve final degerin `15` oldugunu gosterir.
Virtqueue layout validation, `queue_num_max` ve `queue_size` icin sifir, max
asimi ve power-of-two kurallarini uygular; mock modelde adresler sadece hizali
ornek degerler olarak verilir. Feature negotiation modeli ise accepted maskeyi
`device_features & driver_supported` seklinde hesaplar ve zorunlu feature'larin
accepted maske icinde kaldigini dogrular.

Basarili mock init icin magic degeri `0x74726976`, version `2`, device id `2`,
gecerli feature negotiation, gecerli queue layout ve gecerli status akisi
gereklidir. Magic yanlis, version legacy, device block degil, zorunlu feature
eksik veya queue gecersiz ise model basarisiz olur. `driver_required == 0` ve
accepted maske `0` oldugunda feature acisindan init gecerlidir; device ve queue
kosullari yine ayrica saglanmalidir.

Kapsam disi kalanlar bilerek dar tutuldu: gercek MMIO, QEMU boot, virtio-blk
init, queue register yazimi, DMA allocation, interrupt/ISR ve request handling
bu PR'da yoktur. Sonraki adim icin iki yol var: once Claude foundation tarafinda
KEMGU MMIO primitive'i ve capability resource'u netlesir; ardindan Codex gercek
`virtio_mmio.kem` transportunu bu saf modellerin uzerine baglar.
