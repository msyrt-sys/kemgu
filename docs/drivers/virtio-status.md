# VirtIO Status State Machine

Bu not, VirtIO status register akışını donanım erişiminden bağımsız olarak
modelleyen küçük Faz 1 düzeltmesini açıklar. Bu çalışma MMIO, QEMU, kernel boot
veya blok cihaz init kodu içermez; yalnızca status bitlerinin sıralama kuralını
saf KEMGU fonksiyonlarıyla ifade eder.

`drivers/virtio/constants.kem` içinde status değerleri tek-bit olarak tutulur:
`ACKNOWLEDGE=1`, `DRIVER=2`, `DRIVER_OK=4`, `FEATURES_OK=8` ve `FAILED=128`.
`VIRTIO_STATUS_RESET` sıfırdır. Bu ayrım önemlidir, çünkü VirtIO status register
normal akışta kümülatiftir: örneğin ACKNOWLEDGE sonrası DRIVER aşamasına geçmek
register değerini `1 | 2 == 3` yapar; DRIVER bitinin kendisi ise `2` olarak
kalır.

`drivers/virtio/status.kem` iki temel yardımcı sağlar. `virtio_status_ekle`
mevcut register değerine yeni bit ekler. `virtio_status_gecis_gecerli_mi`
mevcut kümülatif status ve eklenmek istenen tek bit üzerinden geçişi doğrular.
Geçerli sıra resetten ACKNOWLEDGE’a, oradan DRIVER’a, sonra FEATURES_OK’a ve
en son DRIVER_OK’a ilerler. Herhangi bir durumdan FAILED bitine geçişe izin
verilir. FAILED biti görüldükten sonra DRIVER_OK dahil başka ilerleme bitleri
reddedilir.

State machine bilinçli olarak `eşleş` ile yazıldı ve wildcard/default kolu
kullanılmadı. Böylece desteklenen status değerleri açıkça görülebilir; bilinmeyen
veya sıralama dışı bir giriş fonksiyon sonunda `yanlış` döndürür. Test dosyası
sekiz kabul senaryosunu kapsar: dört geçerli geçiş, iki reddedilen geçiş,
kümülatif bitlerin korunması ve FAILED bitinin terminal davranışı.

