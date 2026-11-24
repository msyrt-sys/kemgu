# Dalga 16 ilerlemesi

23 Kasım 2026 saat 22.06–23.36 TRT arasında D-766 beş dakikada bir denetlendi. Başlangıç sürümü `4f59349f`. C derleyicisi, self-host derleyici ve C çalışma zamanı nesnesi derlendi.

D-766 tabanında `make calistir_bm_x86_v1 calistir_bm_x86_uefi_pc calistir_bm_x86_saglamlik calistir_kem_os_x86` 24 Kasım saat 00.03’te 0 döndürdü. v1 ve v1-kesinti toplam 40 açılış yaptı.

## Bölüm 1 — klavye

8042 öz-testi, port testleri, kilitler, PS/2 ve HID LED komutları, Pause/PrintScreen, Num Lock ile sayısal tuş takımı, TR/US düzeni, Ctrl+Boşluk ve ön plan sürecine Ctrl+C eklendi. Kesme halkası tanısı `girdisay` komutuyla çalışır. Kısayol olarak Ctrl+Boşluk seçildi. Her iki kaynağın 500/50 ms zamanlamasını korumak için yazılım tekrarı kullanıldı.

`calistir_bm_x86_klavye16` iki derleyicide de PS/2, USB ve 8042’siz USB açılışlarını geçti. QMP ile gönderilen 38 AltGr tuşunun UTF-8 çıktısı kabuk satırında karşılaştırıldı. Her iki derleyicide 1.600 basışın 1.022’si okundu, 578’i kayıp sayıldı. Dört sabotaj son kaynakların ayrı kopyalarında yeniden uygulandı. Dördü de kırmızı; ölçümler bulgular dosyasında.

Yeni klavye kapısı ile §7’deki sekiz regresyon ve dört saf sürücü kapısı 0 döndürdü. Tek make çağrısı 1247 saniye sürdü. Taban derlemesinden itibaren bölüm süresi 48 dakika.

## Bölüm 2 — PCI, ECAM ve köprüler

Genel 16/32 bit port erişimi, MCFG pencereleri, segment 0 ECAM ve CF8/CFC yedeği, köprü arkası tarama ve BAR boyutlandırma eklendi. Düz Q35, iki root-port, iç içe köprü ve i440FX için C ve self-host ile sekiz açılış geçti. Konuk envanteri, BAR’lar, iki seviyeli köprü numaraları, ECAM ve gerçek sayfa öznitelikleri QMP ile karşılaştırıldı. Sıfırlanmış alt veri yolu numarası QEMU üzerinde yeniden kuruldu. Her topolojide SATA ve NVMe dosyaları host imajıyla eşleşti.

Üç sabotaj da kırmızı döndü. İşlev kaydırmasında iki PCI işlevi eksildi, alt veri yolu sabotajında arkadaki üç işlev kayboldu, üst BAR yarısı atılınca gerçek MMIO erişimi sayfa hatası verdi. Ayrıntılar bulgular dosyasında.

PCI kapısı, PC matrisi, dört saf sürücü kapısı ve sekiz regresyonun son koşumları yeşil. Birleşik koşumda yalnız girdi15 rc 1 verdi (make rc 2). Tuşu bırakma QEMU sanal saatine bağlandıktan sonra girdi15 iki derleyicide aynı kabul aralığıyla bağımsız olarak rc 0 verdi. Birleşik koşum süresi 1283 saniye, bölüm süresi 44 dakika.

## Bölüm 3 — kesme kaydı ve disk bildirimleri

IRQ1, IRQ12, LAPIC zamanlayıcısı ve xHCI MSI-X kayıt arayüzüne bağlandı. 66–79 aralığında dinamik vektör ayırma ve bırakma eklendi. AHCI komutları MSI, NVMe CQ1 ise MSI-X bildirimi bekliyor. Geciken bildirimin ardından sürücü sınırlı beklemeden yoklamaya dönüyor. Donanım raporunda vektör, işleyici ve kesme sayısı var.

Yeni MSI kapısı iki derleyiciyle normal ve AHCI MSI yeteneği atlanan dört açılışta geçti. İki gerçek sabotaj kırmızı verdi. Yanlış MSI vektöründe zaman aşımından sonra SATA dosyası yoklama ile yine okundu. İkinci işleyicinin aynı vektöre bağlanmasına izin verilince çakışma testi hata verdi. PC matrisinin 72 açılışı ve §7 regresyonları yeşil. Birleşik koşum 1281 saniye, bölüm 21 dakika.

## Bölüm 4 — disk kurtarma

AHCI hatasında port durduruluyor, hata durumu temizleniyor, BSY/DRQ sürüyorsa COMRESET uygulanıyor ve motor yeniden başlatılıyor. NVMe başarısız tamamlanmayı kuyruktan tüketiyor. USB BOT, yeniden denemeden önce REQUEST SENSE gönderiyor. İlk komuta ek olarak en fazla üç deneme yapılır. Kalıcı sektör hatasının ardından her diskte bağımsız dosya aynı açılışta okundu.

C ve self-host için altışar blkdebug senaryosu geçti. Üç disk türünde tek seferlik hatada bir, kalıcı hatada dört başarısız fiziksel okuma ölçüldü. Yeniden denemeyi sıfırlama ve AHCI portunu yeniden başlatmama sabotajlarının ikisi de kırmızı. Disk kâhini, dört saf sürücü kapısı ve sekiz regresyon yeşil. İlk toplu koşumda eski USB model testi bir REQUEST SENSE beklediğinden kırmızı verdi. Model kalıcı hata için dört REQUEST SENSE ve sonraki geçerli okumayı denetleyecek şekilde güncellendi. Üç ilgili saf test hedefi yeniden yeşil tamamlandı. Toplam test süresi 1391 saniye, bölüm 23 dakika.

## Bölüm 5 — USB 2 hub

Dört portlu bir ve iki katlı hub arkasında klavye, fare ve USB disk çalıştı. İki derleyicide de dosya gezgininin seçim satırı QMP tekerlek olayıyla değişti. Veri diskine yazılan dosyanın baytları kapanışta host imajından okundu. İç içe hub senaryosunda çıkarılan diskin eski tanıtıcısı reddedildi ve yeniden takılan diskten dosya tekrar okundu. Her açılışta boş porta önce tablet, sonra ikinci bir hub takılıp çıkarıldı. Her değişimden sonra HMP ağacı, konuk envanteri ve denetleyicinin fiziksel yuva bağlamları karşılaştırıldı.

İki sabotaj da uygulandı. Port numarası kaydırıldığında xHCI adresleme kod 5 ile durdu. Çıkarmada bağlantı değişim biti onaylanmayınca ilk tablet kaldırıldı fakat ikinci takılan hub konuğun listesine girmedi. USB hub kapısı, dört saf sürücü kapısı, sekiz regresyon ve 72 PC matrisi açılışı geçti. Birleşik koşum 1295 saniye, bölüm 22 dakika.

## Bölüm 6 — Ramdisk ve geçici dosyalar

İki derleyicide USB açılışındaki MBR ve NVMe açılışındaki GPT imajı fiziksel çerçevelere yüklendi. Konuktaki bütün imajın SHA256 değeri host dosyasıyla eşleşti. FAT32'ye yazılan dosya QMP ile okunan RAM'de bulundu. Kapanış sonrası ESP içindeki imaj ve kaynak diskin tamamı ilk özetini korudu. H7j depolama hedefine ramdisk satırları eklendi.

Geçici dosya sisteminde dizin oluşturma, yeniden adlandırma, listeleme, silme, ELF çalıştırma, boru ve yönlendirme iki derleyicide geçti. 33.001 baytlık kopyada çerçeve sınırı açıkça bildirildi. Yeni açılışta önceki dosya bulunamadı. Blok ofseti bir sektör kaydırılan çekirdek MBR'yi bağlayamadı. Düğüm silme işareti kaldırıldığında aynı ad dizin listesinde kaldı. İki sabotajın da dönüşü 1. Bölüm kapıları 1300 saniye, bölüm 22 dakika. PC matrisi 72 açılışı tamamladı.

## Bölüm 7 — ACPI S5 ile kapatma

Kabuk dosya sistemlerini boşalttı ve FADT/DSDT üzerinden bulunan S5 tipini PM1 denetim portuna yazdı. QEMU her iki derleyicide de QMP guest-shutdown olayıyla kendiliğinden kapandı. Host kapanırken yazılan dosyayı doğruladı, fsck.fat dönüşü 0. FADT, 8042 ve üçlü hata ile yeniden başlatma denemeleri de geçti. SLP_EN bitini çıkarmak gerçek QEMU kapısında beş saniyelik zaman aşımı verdi. SLP_TYP kaydırmasını değiştirmek sıfırdan farklı tip içeren host testinde 8 ile döndü. Q35'in S5 tipi sıfır olduğu için ikinci hata q35 üzerinde gözlemlenemez. Test takımı 1284 saniye, bölüm 22 dakika.

## Bölüm 8 — GOP sayfalarında PAT

C ve self-host çekirdeklerinde QMP ile etkin sayfa tablosu yüründü. 1280×800 GOP alanının 1.000 sayfası WC, normal RAM örneği WB, LAPIC ve GOP dışındaki MMIO UC çıktı. Ekrandaki 1.024.000 piksel beklenen görüntüyle eşleşti. PAT girişini varsayılan UC bırakan tek değişiklik, ilk sayfanın bağımsız tür denetiminde rc 1 verdi. Birleşik kapılar 1286 saniye, bölüm 22 dakika.

## Bölüm 9 — Ayrıştırıcı korpusları

Her derleyici dokuz korpusta toplam 12.000 örneği tamamladı. USB korpusunda aygıt ve yapılandırma zincirine 1.200'er örnek ayrıldı. Diğer sekiz korpusta 1.200'er girdi işlendi. Her korpusta hem kabul hem ret veya eksik olay görüldü. İki derleyicide sayaçlar birebir eşleşti. USB yapılandırma zincirindeki sıfır uzunluk korumasını kaldırıp asılma üretme sabotajı uygulanmadı. H12b'nin bu kanıtı açık. Kapılar 1256 saniye, bölüm 21 dakika.

## Bölüm 10 — UART gönderme sınırı ve MMIO

16550 ve PL011 bir baytta art arda 100.000 meşgul okumasından sonra kayıp sayacını artırır. Host modellerinde sürekli meşgul, sınırın son denemesinde hazır ve sonraki baytta toparlanma geçti. Sınır kaldırıldığında gerçek 16550 mock testi üç saniyede zaman aşımıyla 124 verdi. Dokuz donanım beklemesi iki derleyicide ham IR, optimize IR ve x86 makine kodunda geçti. Sıradan bayt toplamı döngüsü listeye eklenince volatile kapısı 1 ile döndü. ARM çekirdek kapıları ve iki UART bare-metal kapısı geçti. Testler 1257 saniye, bölüm 21 dakika.

## Bölüm 11 — Derleme büyüme eğrisi

Deterministik sürücü birimlerinde 4.987, 9.985, 19.981 satır ve birleşik kabuk için iki derleyici ölçüldü. Her satırın süre, tepe RSS, IR satırı ve en büyük işlev değerleri bulgulara yazıldı. Her eşik aynı makinedeki ilk ölçümün üç katıdır. 5k üreticinin ana işlevindeki çağrı dizisini on kez çoğaltan sabotajda en büyük işlev her iki derleyicide sınırı aştı. Kapılar 1251 saniye, bölüm 21 dakika.

## Bölüm 12 — Bilgi, düzenleyici ve dosya gezgini

Bilgi komutu UEFI çerçevelerini, çalışma süresini, heap kullanımını, canlı süreçleri, disklerdeki boş baytları, kesmeleri ve giriş kaynaklarını listeler. Düzenleyici Türkçe harf katlamalı ileri/geri arama, onaylı tek/tüm değiştirme, satıra gitme ve 64 değişikliğe kadar geri alma/yineleme sağlar. Gezgin ve kabuk kopyalama, taşıma ve silme için aynı VFS işlevlerini kullanır. 71 ve 72 numaralı çağrılar çapraz bağlamada kullanılabilir. Gezgin F7 ile dizin oluşturur, Delete öncesinde onay ister.

Üç saf model ve iki derleyiciyle altı araç açılışı geçti. Konuğun çerçeve sayıları QMP'den okunan firmware haritası ve gerçek CR3 ağacıyla, disklerdeki boş alan FAT girdileriyle karşılaştırıldı. Kaydedilen UTF-8 metni ve son kısmı 240 bayt olan ikili kopya host imajında doğrulandı. Üç sabotaj kırmızı. Bölüm kapıları 1499 saniye, bölüm 25 dakika.

## Bölüm 13 — Yol haritası ve belgeler

Yol haritasındaki kanıt notları, ABI, mimari, klavye, program yazma ve fiziksel kontrol belgeleri güncellendi. Dalga 9 öncesi H maddeleri tarihsel ölçümlere eşlendi. H4e için ekran aynasını kapatan yeni denemede 940 piksel farklı çıktı. AltGr+Q karakteri değiştirildiğinde QMP ile yazılan UTF-8 satırı uyuşmadı. Kaynak örneklerinin üçü de iki derleyiciyle üretildi ve OVMF altında çalıştı. Kabuğun genel listesi ve 40 yardım yanıtı belgeyle birebir karşılaştırıldı. Eski ve yeni sağlamlık senaryoları, PVH ekranı, bölüm regresyonları, sınıflandırma, bare-metal yapı paritesi ve sıfır uyarı denetimleri geçti. İlk toplu koşum 1349 saniye sürdü. PVH seri başlığında AHCI yoklama bildirimi eksikti. Kâhin düzeltildi, USB ve PS/2 hedefleri geçti. Bütün bölüm kapılarının ikinci koşumu 1339 saniye, bölüm 45 dakika.

### Yol haritası değişiklikleri

| Madde | Önce | Sonra | Kanıt |
|---|---|---|---|
| H1a | Yalnız birleşik kabuk | 5k/10k/20k ve kabuk için iki derleyici ölçüldü | `calistir_derleme_olcum` |
| H1f | 32 bit CF8/CFC, köprü ve BAR kanıtı eksik | 16/32 bit port, ECAM, CF8 yedeği, köprü ve BAR karşılaştırması | `calistir_bm_x86_pci16` |
| H7a | 32 bit CF8/CFC, köprü ve BAR kanıtı eksik | 16/32 bit port, ECAM, CF8 yedeği, köprü ve BAR karşılaştırması | `calistir_bm_x86_pci16` |
| H7b | 32 bit CF8/CFC, köprü ve BAR kanıtı eksik | 16/32 bit port, ECAM, CF8 yedeği, köprü ve BAR karşılaştırması | `calistir_bm_x86_pci16` |
| H7c | 32 bit CF8/CFC, köprü ve BAR kanıtı eksik | 16/32 bit port, ECAM, CF8 yedeği, köprü ve BAR karşılaştırması | `calistir_bm_x86_pci16` |
| H1g | Sabit kesme yolları ve xHCI | Kaydedilebilir vektörler, AHCI MSI, NVMe CQ1 MSI-X | `calistir_bm_x86_msi` |
| H3h | Sabit kesme yolları ve xHCI | Kaydedilebilir vektörler, AHCI MSI, NVMe CQ1 MSI-X | `calistir_bm_x86_msi` |
| H7d | Sabit kesme yolları ve xHCI | Kaydedilebilir vektörler, AHCI MSI, NVMe CQ1 MSI-X | `calistir_bm_x86_msi` |
| H1h | MMIO bekleme döngüsü için makine kodu denetimi eksik | Dokuz yol ham IR, optimize IR ve makine kodunda ölçüldü | `calistir_mmio_volatile` |
| H3g | FADT reset ve 8042 | S5 ile eşitleme, guest-shutdown ve temiz FAT | `calistir_bm_x86_kapat` |
| H4b | Framebuffer PAT karşılaştırması eksik | GOP yaprakları WC, diğer BAR yaprakları UC | `calistir_bm_x86_pat` |
| H5a | 8042, özel tuş, LED, düzen, tekrar, Ctrl+C ve taşma kanıtları kısmi | 8042 öz-testi, USB/PS2 girdileri ve donanım geri bildirimi | `calistir_bm_x86_klavye16` |
| H5c | 8042, özel tuş, LED, düzen, tekrar, Ctrl+C ve taşma kanıtları kısmi | 8042 öz-testi, USB/PS2 girdileri ve donanım geri bildirimi | `calistir_bm_x86_klavye16` |
| H5d | 8042, özel tuş, LED, düzen, tekrar, Ctrl+C ve taşma kanıtları kısmi | 8042 öz-testi, USB/PS2 girdileri ve donanım geri bildirimi | `calistir_bm_x86_klavye16` |
| H5e | 8042, özel tuş, LED, düzen, tekrar, Ctrl+C ve taşma kanıtları kısmi | 8042 öz-testi, USB/PS2 girdileri ve donanım geri bildirimi | `calistir_bm_x86_klavye16` |
| H5f | 8042, özel tuş, LED, düzen, tekrar, Ctrl+C ve taşma kanıtları kısmi | 8042 öz-testi, USB/PS2 girdileri ve donanım geri bildirimi | `calistir_bm_x86_klavye16` |
| H5h | 8042, özel tuş, LED, düzen, tekrar, Ctrl+C ve taşma kanıtları kısmi | 8042 öz-testi, USB/PS2 girdileri ve donanım geri bildirimi | `calistir_bm_x86_klavye16` |
| H5i | 8042, özel tuş, LED, düzen, tekrar, Ctrl+C ve taşma kanıtları kısmi | 8042 öz-testi, USB/PS2 girdileri ve donanım geri bildirimi | `calistir_bm_x86_klavye16` |
| H5j | 8042, özel tuş, LED, düzen, tekrar, Ctrl+C ve taşma kanıtları kısmi | 8042 öz-testi, USB/PS2 girdileri ve donanım geri bildirimi | `calistir_bm_x86_klavye16` |
| H7e | Port kurtarma kanıtı eksik | Üç aygıtta geçici ve kalıcı sektör hatası | `calistir_bm_x86_disk_hata` |
| H7h | UEFI ramdisk ve geçici bağlama noktası eksik | Fiziksel çerçevelerde FAT32 imaj ve bağımsız /ram | `calistir_bm_x86_ramdisk` |
| H7j | UEFI ramdisk ve geçici bağlama noktası eksik | Fiziksel çerçevelerde FAT32 imaj ve bağımsız /ram | `calistir_bm_x86_ramdisk` |
| H8a | UEFI ramdisk ve geçici bağlama noktası eksik | Fiziksel çerçevelerde FAT32 imaj ve bağımsız /ram | `calistir_bm_x86_ramdisk` |
| H10e | Yalnız kök port | İki hub katmanı, canlı çıkarma ve yeniden takma | `calistir_bm_x86_usb_hub` |
| H10g | TX sınırı ve disksiz geçici dosya sistemi açık | C UART sayaçları ve diski devreden çıkarılmış kabuk | `calistir_uart_16550_test`, `calistir_uart_pl011_test`, `calistir_bm_x86_saglamlik` |
| H12b | 17.000 örnek, PS/2 ve USB korpusu eksik | Kısmen. Dokuz korpus ve 12.000 ek örnek doğrulandı. İstenen sıfır uzunluk sabotajı uygulanmadı | `calistir_kem_os_ayristirici` |
| H12c | Dalga 9 öncesi eşleme eksik | Tarihsel kaynaklar ve yeni ekran aynası sabotajı eklendi, uygulanmayan deney ayrıca kayıtlı | `KEMGU_OS_Sabotaj_Envanteri.md` |
| H12e | ABI 70, iki çalışan örnek | ABI 72, üç çalışan örnek ve kabuk yardımının birebir karşılaştırılması | `calistir_bm_x86_belge15` |


## İlk tam koşum ve disk regresyonu

İlk tam deneme 64 dakika 5 saniye sürdü. PIPESTATUS=(2 0) idi. Tek başarısız hedef calistir_bm_x86_disk oldu. Yük altında 50 ms'yi geçen NVMe bildirimi yoklama yoluna düşürdü. QMP ile 8 IOPS sınırı uygulanarak aynı hata eski çekirdekte yeniden gözlendi. Sınırlı bekleme 500 ms'ye çıkarıldı. İki derleyici ve dört MSI açılışında dosya içerikleri, host I/O sayacı ve konuk CQ1 sayacı doğrulandı. Disk kapısı ve MMIO kapısı da geçti. Ardından tüm hedefler yeniden çalıştırıldı.


## İkinci tam koşum ve kapanışta QMP

İkinci tam deneme 63 dakika 55 saniye sürdü. PIPESTATUS=(2 0) idi. Dört NVMe hız sınırlama deneyi ve dört GPT/MBR yazma deneyi geçti. Çok süreç testinde hesap doğru bitti, konuk kapanış satırını yazdı. Son Enter basışından sonra QMP ile bir LAPIC tiki daha okunurken soket süre aşımı verdi. Kapanış kâhini bu olasılıkta da QEMU'nun rc 0 ile çıkmasını ve seri çıktının tamamının birebir eşleşmesini arıyor. Gerçek S5 başarılı oldu. Kapanış satırı olmadan rc 0 dönen QEMU reddedildi. Tam takım üçüncü kez başlatıldı.

## Üçüncü tam koşum ve ön plan klavyesi

Üçüncü denemede 50 süreçlik kaynak denetimi 131.168 bayt çekirdek heap artışı bildirdi. Düzeltme ayrı QEMU oturumlarında doğrulandıktan sonra eski sürümdeki koşum 33 dakika 25 saniyede durduruldu. Tamamlanan hedeflerde kaydedilen tek hata buydu. Kesilen make/tee boru hattının çıkışları (143 0). Ardından ek üç kapı geçti. Ölçüm sınırı 65.616 bayt olarak korundu. HID raporu için ayrılan geçici olaylar ile Set 1 çözümündeki bölgeler kaldırıldı. Kabuk ve ön plan süreci tek klavyenin sabit çıkış alanını sırayla kullanıyor. Yeni QEMU kapısı 32 hesap sürecinde Enter bırakmasını etkin PID ile eşzamanlıyor. Eski çekirdekte ilk başlatmadan sonraki 31 çalıştırma toplam 1.049.344 bayt ile reddedildi. Düzeltilmiş C ve self-host çekirdeklerinde aynı ölçüm 0 bayt. İki derleyicide ikişer kez yapılan uzatılmış 50 süreç deneylerinin dördü de 0 bayt verdi. Tam takım dördüncü kez başlatıldı.
