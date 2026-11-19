# Dalga 12 bulguları

## Tasarım seçimleri

- VARSAYIM: açılış kimliği ESP ile eşleşirse kök okunabilir fakat salt okunur kalır; ESP koruması açılış aygıtı seçimine üstün gelir. Başka veri bölümü sessizce kök/yazılabilir seçilmez.

- A: QEMU q35 varsayılan ATAPI portu SATA adayı görünebilir. İlk gerçek kabuk regresyonu `ahci sata aygiti yok` ile kırmızı oldu. Bu sonuç SATA diski olarak listeye alınmıyor. USB açılışının yedi eski komutu ve piksel kâhini geçti.
- A: NVMe ve AHCI'ye ayrı 128 KiB DMA dilimleri. 64 sektörlük LRU kirli eski girdiyi aygıta yazar, aygıt flush'ından sonra aynı sürümü onaylar. Toplu boşaltma ancak flush tamamlanınca girdileri temizler.
- Başlangıç UEFI kabuk birleşik birimi: 4821 kaynak satırı, self-host `--llvm` 10,343 sn, rc=0.

## UEFI açılış kimliği

- [UEFI 2.10 §10.3.5.1](https://uefi.org/specs/UEFI/2.10/10_Protocols_Device_Path_Protocol.html#hard-drive) düzeni Python ile doğrulandı. `uuid.UUID('09576e91-6d3f-11d2-8e39-00a0c969723b').bytes_le.hex()` → `916e57093f6dd2118e3900a0c969723b`, `struct.calcsize('<BBHIQQ16sBB')` → 42.
- OVMF QMP RAM dökümünde GPT ham kimliği `265bb15cc7ae8e539cd32fcc425b2f9c`, MBR ham imzası `1e90cb72`. Her ikisinde HD tür/alt tür 4/1, LBA 2048, uzunluk 131072, bölüm no 1. Python imaj üreticisinin koyduğu değerlerle birebir.
- 192 baytlık ABI v1 öneki korunuyor. Ayrılmış 22/23 alanları aynı sayfadaki 56 baytlık bölüm uzantısının ofsetini/boyunu taşıyor. Uzantı sürümü 1, ardından 42 bayt ham HD düğümü geliyor. Eski stub/PVH için açık bilinmiyor yolu, okunamayan uzantı için açık hata var.
- VARSAYIM: GPT ana başlık/girdi CRC bozuksa disk atlanır ve uyarı verilir. Yedek başlığa otomatik geçiş yok. Birincil MBR girdileri ve 128'e kadar GPT girdisi desteklenir. Çakışan bölümler veya disk dışına taşan aralıklar reddedilir.
- VARSAYIM: bağlama adları `/diskler/{nvme,sata,usb}<no>p<bölüm>`. Tek FAT32 ve kimlik eşleşmesi yoksa uyarıyla salt okunur kök seçilir. Birden çok adayda `/diskler` üzerinden erişim sürer, otomatik yazma yetkisi oluşmaz. Aynı FAT seri no taşıyan Python diskleri doğru açılış bölümünü ayırt eden fikstürdür.
- GPT matrisinde firmware ESP'den yükler. ESP yazılamaz, USB veri bölümü açık `bagla --yazilabilir usb0p2` ile yazılır. MBR FAT32 açılış bölümü ESP değildir ve otomatik yazılır. İç diskler ancak açık bağlama komutuyla yazılabilir.

## Sektör yazma ve kesinti

- Tam imaj belleğe alınmaz. Sektör yazıcısı eksik sektör ister. Ad çözümü, küme zinciri ve FSInfo doğrulandıktan sonra sektör düzenlemeleri yayımlanır. Dosya/dizin zincirinin veri sınırı 64 KiB, işlem başına okuma 256 sektörle sınırlı. Sektör boyu 512.
- VARSAYIM: oluşturma akışı veri → iki FAT → dizin → FSInfo. Her aşamada önbellek ve aygıt sıralı boşalır. Silmede önce dizin girdisi kaldırılır, sonra FAT boşaltılır ve FSInfo güncellenir. Kesinti tahsis sızıntısı bırakabilir, ancak silinmiş dosya zinciri açık dizin girdisi üzerinden yeniden kullanıma sunulmaz. Başarısız yazma aşamasından sonra temiz bayrağa geçilmez.
- `fsck.fat` 4.2, bağımsız FAT32 imajında BPB 0x41 bit0 ve FAT[1] bit27'nin temiz işaretinin kaldırılmasını ayrı ayrı kirli raporladı (her biri rc=1, temiz rc=0). BPB geleneği seçildi. Ana/yedek BPB'de mount → kirli, sync/kapat/yenidenbaşlat → temiz. Sonraki yazma bayrağı yeniden kurar.
- QMP quit koşusu belirlenen yazma komutunun tamamlandığı yerde, sync'ten önce kesildi. fsck -n rc=1 ve bir sonraki açılışta kirli uyarısı görüldü. Aynı dosya yeniden okundu ve sync sonrası fsck rc=0. Aşamalar arasındaki bütün güç kesintisi kombinasyonları test edilmedi.
- VARSAYIM: `yaz`, `cp`, `mv` mevcut hedefi ezmez. `mv` normal dosyalarda önce hedefi kalıcı kılar, sonra kaynağı siler ve kesintide iki kopya kalabilir. Dizin taşıma bu sürümde desteklenmez. Dizin silme yalnız boş dizin için.
- Türkçe/temel Latin bileşen karşılaştırması sektör yolunda tablonun yeniden kurulmasını önler. İlk QEMU örneğinde genel Unicode eşleme yolu tek komutta 4–6 MB heap artışı üretti. Düzeltmeden sonra üç diskli senaryonun toplamı yaklaşık 11 MB düzeyine indi. Ölçüm `kem_bump - __heap_start` ile her komuttan önce/sonra alınır. Bu bir üst sınır ölçümüdür, anlık canlı nesne sayısı değildir.

## C12-001

Self-host farklı bloklarda aynı yerel ad başka türe bağlandığında ilk alanı sonraki türde arıyor. C kabul ediyor. Aşağıdaki örnek `build/kemgu --check` ile OK, `build/codegen --check` ile `T009 5 26`. Kabuk ve aygıt testi yerelleri farklı adlandırıldı.

```kem
yapı Ilk { sayi: tam32; }
yapı Ikinci { baska: tam32; }
işlev main() -> tam32 {
    değişken xs: Dizi<Ilk> = [];
    için a: xs { eğer a.sayi == 17 { ver 1; } }
    eğer doğru { değişken a: Ikinci = Ikinci { baska:0 }; ver a.baska; }
    ver 2;
}
```

## RTC ve bozuk imajlar

- VARSAYIM: MC146818 iki tutarlı örnekle okunur, Status A/B denetlenir. BCD ve ikili, 12 ve 24 saat desteklenir. Yıl penceresi mevcut RTC örneği gibi 2000–2099. Geçersiz RTC ile `saat` hata verir, dosya zaman damgası 1980-01-01'e düşer.
- Sabit QEMU tabanı 2026-07-14 09:42:00. İlk gerçek `saat` örneği 09:42:08. Host FAT okuyucusu `günlük.txt` 09:42:09 ve `iç.txt` 09:42:14 okudu. Oluşturma/erişim/değiştirme tarihleri ve FAT saniye çözünürlüğü de doğrulanıyor.
- Sabit tohumlu 1000 bozuk sektör kümesi BPB, FAT zinciri, dizin/LFN, GPT başlığı ve girdi ayrıştırıcısından geçirilir. C+self koşumunda çökme ve zaman aşımı olmadı.

## C12-002

İki derleyici `GcIstek` aynı yerleşimdeyken dönüş tipinin IR yazımında ayrılıyor. Aşağıdaki üç satır için C `define %GcIstek @komut`, self `define {i8, i64, i64, i64, i64, i64, i64, i64, i32, i64, ptr, i32, ptr} @komut` üretiyor. C'nin `%GcIstek` tanımı aynı alanlar. Bu davranışsal bir bozulma olarak gözlenmedi, fakat metinsel x86 yapı kapısında 25 imza farklı görünür.

```kem
kullan drivers::ortak::gc::{ GcIstek };
işlev komut() -> GcIstek { ver GcIstek::Bitti; }
işlev main() -> tam32 { ver 0; }
```

Komut: `build/{kemgu,codegen} --llvm --mimari x86_64-none <örnek>` (iki ayrı koşum). Derleyici ve yapı kapısının karşılaştırması korunuyor. Blok yürütücüsü, kullanan iki x86 örneğinin ortak CAT parçasıdır. Bu birimler birleşik `--check` ve C+self gerçek QEMU açılışlarıyla ölçülür.

## xHCI ortak halka

- QEMU q35: aynı xHCI denetleyicide iki USB BOT ve USB klavye. İkinci BOT kendi EP0/IN/OUT tamponlarına sahip, ilk BOT ile ERST/tüketici konumu ortak. Aygıtlar arasında okumalar ve yazmalar sırayla geçildi; iki USB imajı da fsck ve bağımsız okuyucuyla denetlendi.
- İkinci BOT yönetim komutu eski tüketici konumuna döndürülünce 1/1 açılış `xhci olay kimligi` ile kırmızı. Başlangıçta tek PCI xHCI denetleyicisi seçilir. NVMe çekirdeği mevcut tek ad alanı (NSID 1) sözleşmesini kullanır; GPT yedek ve genişletilmiş MBR taraması bu dalgada eklenmedi.

## PVH uyumluluğu

- İlk tam koşu eski PVH kabuk kâhininde açılış çıktısına eklenen uyarıyı yakaladı. Kâhin beklentisine dokunulmadı. Tek FAT32 bölümünün kimliği bilinmeyen PVH yolunda eski açılış çıktısı korunur, `diskler` durumu açıkça listeler ve yazma isteği "acilis bolumu bilinmiyor; kok salt okunur" gerekçesiyle reddedilir. UEFI bilinmeyen kimliği açılışta da bildirir. Birden çok bilinmeyen adayda hiçbir bölüm varsayılan yazılabilir kök olmaz.
- Ek PVH QEMU denemesi: aynı açılışta `diskler` uyarısı, `yaz /deneme.txt veri` reddi, tam seri/piksel ve değişmeyen SHA256 birlikte doğrulandı. `build/dalga12_pvh_uyari.log`, rc=0. Düzeltme commit'i `f0692d1`.

## Son doğrulama

- `make -j1 test_tumu calistir_kem_os_x86` rc=0 (PIPESTATUS[0]), FIXPOINT ✓ 100378 satır, qtest 6/6, köprü kart + 141 test, baremetal_diff 23/23 ve x86 noredzone 513/513. Tam koşu 69 atlama kaydı ve ARM üst bellek alt denetiminde 1 koşullu atlama verdi. ASan 133 geçiş, 0 hata, 43 atlama, 4 bilinen istisna ve 2 sızıntı muafı.
- Son yeni QEMU kapıları C ve self-host toplam 12/12, atlama yok. 20 temiz fsck ve 2 beklenen kirli kesinti. Son kaynakların ayrı kopyasında 8/8 sabotaj beklenen kırmızıyı üretti.
- En yüksek heap 11019056 bayt. Kabuk 4821 → 5358 satır, aynı oturumdaki self-host süresi 19,699 → 24,163 sn. Genel x86 6184 → 6242 satır, 8,792 → 8,931 sn. Komut başı üst sınırlar ilerleme dosyasında.
