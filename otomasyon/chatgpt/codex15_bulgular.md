# Dalga 15 ölçümleri

## Donanım beklemeleri, bölüm 2 anındaki satırlar

| Dosya ve satır | Beklenen durum | Sınır |
|---|---|---|
| `runtime/mimari/x86_64/kem_uart.kem:26` | 16550 LSR.THRE | 100.000 okuma, ardından tek çıkış |
| `runtime/mimari/x86_64/kem_ps2_hw.kem:5` | 8042 IBF boş | 10.000 × 50 gecikme adımı |
| `runtime/mimari/x86_64/kem_ps2_hw.kem:22` | 8042 OBF, klavye cevabı ve yoklamalı girdi dilimi | 10.000 × 50, ardından -1 |
| `runtime/mimari/x86_64/kem_ps2_hw.kem:36` | 8042 başlangıç FIFO drenajı | 256 bayt |
| `runtime/mimari/x86_64/kem_ps2_hw.kem:117` | AUX cevabında OBF ve kaynak biti | 10.000 × 50, ardından -1 |
| `runtime/mimari/x86_64/kem_rtc_hw.kem:14` | RTC UIP temiz ve iki örnek aynı | 2.000 × 10 |
| `runtime/mimari/x86_64/kem_apic_hw.kem:116` | PIT sayacı ve LAPIC kalibrasyonu | 2 milyon × 50 |
| `runtime/mimari/x86_64/kem_guc_hw.kem:49` | 8042 yeniden başlatma komutu için IBF | 10.000 × 50, ardından sıradaki reset yöntemi |
| `runtime/mimari/x86_64/kem_pci_hw.kem:77` | Port 0x80 gecikmesi | Çağıranın verdiği adım sayısı |
| `drivers/nvme/cekirdek.kem:79` | CSTS.RDY ve CQ fazı | CAP.TO, başlangıç değeri 500 |
| `drivers/ahci/cekirdek.kem:22` | HBA, port, görev dosyası ve PxCI | Faz başına 1.000 adım |
| `drivers/usb/xhci_cekirdek.kem:37` | xHCI durum biti ve port reset | 4.000 adım |
| `drivers/usb/xhci_cekirdek.kem:219` | xHCI olay halkasının döngü biti | 4.000 adım |
| `drivers/usb/depo.kem:92` | BOT aktarımı, kontrol ve toplu uç | Faz başına 1.000 adım |
| `test/ornekler/kem_os_x86_blok_ortak.kem:224` | AHCI portlarından birinde PxSSTS.DET=3 | 12 tarama × 32 port |
| `test/ornekler/kem_os_x86_blok_ortak.kem:107` | xHCI olay halkası boşaltma | Tek turda halka kapasitesi |
| `test/ornekler/kem_os_x86_blok_ortak.kem:151` | Blok durum makinesi | 2 milyon adım |
| `test/ornekler/kem_os_x86_blok_ortak.kem:218` | USB yapılandırma ve devir | 100.000 adım |
| `test/ornekler/kem_os_x86_kabuk.kem:62` | Başlangıç xHCI kontrol aktarımı | 100.000 adım |
| `runtime/kem_virtio_blk.kem:157` | VirtIO blok used.idx | 100 milyon tur |
| `runtime/kem_virtio_net.kem:132` | VirtIO TX used.idx | 100 milyon tur |
| `runtime/kem_virtio_net.kem:144` | VirtIO RX used.idx | Çağıranın verdiği tik sayısı |
| `test/ornekler/kem_os_x86_nvme.kem:18` | NVMe QEMU sürücü örneği | 2 milyon adım |
| `test/ornekler/kem_os_x86_ahci.kem:18` | AHCI QEMU sürücü örneği | 2 milyon adım |
| `test/ornekler/kem_os_x86_usb.kem:22` | xHCI ve USB depo QEMU sürücü örneği | İki yürütücü de 100.000 adım |
| `runtime/kdl_runtime_uart_16550.c:156` | C 16550 TX/RX | LSR.THRE ve DR beklemelerinde sınır yok. Dalganın izin vermediği C çalışma zamanı |
| `runtime/kdl_runtime_uart_pl011.c:153` | ARM C PL011 TX/RX | FR.TXFF ve RXFE beklemelerinde sınır yok. ARM çalışma zamanı bu dalganın dışında |

Sürücü durum makineleri çağrı başına bir adım yürütür. Tablo hem bu tekrarları hem onları süren dış sınırları gösterir. Klavye girdisi, çocuğun çıkışı ve boşta HLT bir kullanıcı olayını bekler. Panik ve kapanış sonrası HLT son durumlardır. Eski PVH kabuğu girdi gelene kadar sonlu PS/2 yoklama dilimlerini heap ayırmadan yineliyor.

QEMU’nun NVMe denetleyicisi komutlara yanıt verir. Namespace bağlamadan çalıştırılan gerçek aygıt, kullanılamayan NVMe hedefini temsil etti. Kabuk çalışan USB’yi bağlayıp komut kabul etti. Sökme deneyi gerçek `device_del` kullandı. PORTSC bağlantı ve değişim bitleri, önbellekte duran sektörlerin sökülmüş ortama ait olduklarını da saptadı. xHCI port olayını halkadan alan kod USB klavyenin sıradaki tamamlamasını işleyebildi. Boş SATA denetleyicisindeki süre sınırı kaldırıldığında kâhin istemi beklerken süreyi doldurdu.

## Donanım raporu

GPT açılışında ESP salt okunur. Veri bölümüne kullanıcı yazma izni verdikten sonra açık kaydetme komutu çalıştı. MBR'de yazılabilir açılış FAT32 köküne rapor kendiliğinden kaydedildi. Her iki düzen için konuk seri raporu ve FAT32 dosyası birebir karşılaştırıldı. PCI kimlikleri QMP ağacından, ACPI ve MADT fiziksel RAM'den, CPU QOM'dan, USB aygıt sayısı HMP'den denetlendi. MBR eski bağlama kâhini de özgün dosyaların özetlerini ve oturum boyunca diskin değişmediğini doğruladı.

## Günlük

450 numaralı Türkçe satır yazdırıldı. Tek bir satırın kendisi kapasiteyi aştığında satır sonuna kadar tek kayıp sayılır. Günlük sorgusu kendi çıktısını tekrar okumamak için çıktıdan önce anlık kopya alır. Uzun çıktıdan hemen sonra USB HID ile sentetik tuş gönderildiğinde ilk denemede komutun bazı harfleri kayboldu. Günlük bayt karşılaştırması ve sarım testi kesme tamponlu PS/2 klavyeyle yapıldı. USB klavye ayrıca sağlamlık ve kabuk kapılarında sınandı.

## PC açılış matrisi

Her taşıma iki klavye ve iki tabloyla sınandı. RAM seçimi taşıma, klavye ve tablo indislerine göre döndürüldü. Böylece her değer öbür eksenlerin her değeriyle en az bir kez karşılaştı. MBR imajı iki bölüm içeriyor. Okunan ve yazılan veri yolu seri açılış aygıtıyla eşleştirildi. Her satır üç C ve üç self-host açılışıdır.

| Açılış | Klavye | RAM | Tablo | C | Self-host |
|---|---|---|---|---:|---:|
| NVMe | PS/2 | 1G | MBR | 3/3 | 3/3 |
| NVMe | PS/2 | 4G | GPT | 3/3 | 3/3 |
| NVMe | USB | 256M | GPT | 3/3 | 3/3 |
| NVMe | USB | 4G | MBR | 3/3 | 3/3 |
| AHCI | PS/2 | 256M | GPT | 3/3 | 3/3 |
| AHCI | PS/2 | 4G | MBR | 3/3 | 3/3 |
| AHCI | USB | 1G | GPT | 3/3 | 3/3 |
| AHCI | USB | 256M | MBR | 3/3 | 3/3 |
| USB | PS/2 | 1G | GPT | 3/3 | 3/3 |
| USB | PS/2 | 256M | MBR | 3/3 | 3/3 |
| USB | USB | 1G | MBR | 3/3 | 3/3 |
| USB | USB | 4G | GPT | 3/3 | 3/3 |

İlk C koşusunda 24/36 açılış geçti. Kalan 12 açılış, çerçeve ayırıcısının üst sınırına göre yüksek adresli yükleyiciyi reddetti. Düzeltmeden sonraki 72 matris açılışında aralıklı hata sayısı sıfır. MBR başlangıcındaki tek sektörlük sapma altı yapılandırmayı da bozdu.

## Ayrıştırıcı örnekleri

| Ayrıştırıcı | Bir derleyicide örnek | Korpus |
|---|---:|---|
| ACPI kökleri | 4.000 | Rastgele girdi ve RSDP, XSDT, RSDT şablonları |
| MADT | 2.000 | Rastgele veri ve geçerli kayıt mutasyonları |
| FADT | 2.000 | Rastgele veri ve geçerli tablo mutasyonları |
| ELF | 2.000 | Rastgele veri ve geçerli yüklenebilir başlık mutasyonları |
| BMP | 3.000 | Rastgele veri ve iki geçerli resim şablonu |
| GPT/MBR | 2.000 | Rastgele veri ve bölüm tablosu mutasyonları |
| MBR sınırları | 1.000 | Bağımsız beklenen başlangıç, boy ve disk sınırı |
| FAT32 | 1.000 | D-759 sabit tohumlu dosya sistemi korpusu |

Üreticinin sınır örnekleri bu sayılara eklenir. Host'ta iki derleyicinin çıktısı karşılaştırıldı. Bozuk kullanıcı ELF'sini yükleme denemesi QEMU'da üç diskin özgün imaj özetlerini korudu.

## Giriş ölçümleri

PS/2 aygıtı tekerlekli kimlik 3 ve dört baytlık paket bildirdi. USB ve PS/2 yollarında üçüncü satıra kaydırma ve başlangıca dönüş aynı GOP görüntüsünü verdi. Hız deneyi hassasiyet 2 ile 12 birim hareketi 36 piksel olarak ölçtü, ters yönde büyük hareket sol kenarda sıfıra kenetlendi. Tekrarın ilk gecikmesi 500 ms, aralığı 50 ms. Sabotajlar birer gerçek açılışta yakalandı.

## Program sınırları

Görüntüleyici 256 KiB'ye kadar BMP okuyor. Çekirdek, kullanıcı penceresinden bir piksel satırı kopyalarken satır genişliği ile bellek aralığını doğruluyor. Metin araması satır başına 4096 bayt destekliyor. Hesaplayıcı işaretli 32 bit sınırını ve sıfıra bölmeyi bildiriyor. Saat QEMU RTC'sinin iki ayrı görüntüsüyle karşılaştırıldı.

## Boru ve dosya doğrulaması

`kedi < iz.txt | bul ağaç | say > sonuc.txt` zincirinde ev sahibi beklenen satır, kelime ve UTF-8 bayt sayılarını kendi girdisinden hesapladı. Tampon kapasitesini aşan aktarım 1401 satır ve 26.619 bayttı. Son okuyucunun kapanması yazıcıya hata verir. Son yazıcı kapandığında okuyucu önce kuyruktaki baytları, sonra EOF alır. Her oturumdan sonra veri FAT32 bölümü fsck ile ve iki salt okunur disk tam SHA256 ile doğrulandı.

## Açılış süreleri

Aşağıdaki milisaniyeler firmware devrinden itibaren birikimlidir. Host zamanı seri dosyadaki devir ve hazır işaretlerinden bağımsız okunur. İlk denemede host medyanı yaklaşık 267 ms ölçüldü. 650 ms sınırı normal dağılımın üzerinde ve eklenen 1,5 saniyelik gecikmenin altındadır.

| Derleyici | Bellek | ACPI | PCI | USB | Diskler | Kabuk | Host |
|---|---:|---:|---:|---:|---:|---:|---:|
| C | 20.6 | 145.3 | 178.1 | 187.3 | 233.6 | 235.1 | 242.4 |
| C | 36.3 | 160.3 | 201.4 | 213.8 | 267.9 | 270.7 | 322.3 |
| C | 19.9 | 144.3 | 189.8 | 204.3 | 260.7 | 263.0 | 269.8 |
| C | 19.0 | 142.4 | 173.4 | 181.4 | 223.8 | 226.1 | 235.0 |
| C | 30.9 | 154.6 | 193.9 | 202.8 | 236.3 | 237.8 | 247.0 |
| Self-host | 24.0 | 148.6 | 189.5 | 204.1 | 271.7 | 274.1 | 292.5 |
| Self-host | 23.8 | 148.2 | 183.5 | 197.9 | 258.9 | 261.7 | 268.0 |
| Self-host | 23.6 | 148.1 | 182.8 | 197.2 | 252.9 | 254.4 | 260.8 |
| Self-host | 24.7 | 149.6 | 184.3 | 199.1 | 274.3 | 275.8 | 283.9 |
| Self-host | 24.2 | 149.1 | 185.5 | 199.9 | 265.3 | 268.0 | 274.1 |

Konuk medyanları sırasıyla 237,8 ve 268,0 ms. Sabotajlı konuğun medyanı 1751,1 ms. Tarihsel sabotaj sayısı kaynakta bulunamadığında envanter bunu belirtir.

## Son doğrulamada ölçülen G/Ç beklemesi

İlk tam takımda x86 USB testinde 20 açılıştan biri ve AHCI testinde 20 açılıştan biri zaman aşımı üretti. FAT32 GPT yazma senaryosu da USB zaman aşımı bildirdi. Ayrı USB tekrarında oran yine 1/20 oldu. Üç PVH açılışında PIT kanal 2 zaman tabanına göre port 0x80 ile istenen 1000 µs'nin 52, 53 ve 52 µs sürdüğü ölçüldü. Sürücü isteklerinin sayısal zaman aşımı bu nedenle QEMU'da gerçek süresinden erken doluyordu.

G/Ç yürütücüsü artık UEFI yolunda mevcut PIT kanal 0 kalibrasyonunu paylaşır. PVH yolunda gerektiğinde PIT kanal 2'yi bir kez ölçer ve hoparlör denetim bitlerini geri yükler. Ölçüm denemesinde yeni beklemeler 1048, 1039 ve 1037 µs çıktı. USB ve AHCI aynı koşullarda 20/20'şer açılışı geçti. Disk kapısında GPT ve MBR yazma, güç kesintisi ve iki USB belleği iki derleyicide de geçti.

| Ek bekleme | Koşul | Sınır ve hata |
|---|---|---|
| `runtime/mimari/x86_64/kem_gc_yurut.kem:55` | PIT kanal 2 çıkış biti | 1.000.000 port okuması, açık PIT hatası |
| `runtime/mimari/x86_64/kem_gc_yurut.kem:75` | Kalibre TSC gecikmesi | İstek en çok 1.000.000 µs, döngü en çok 1.000.000 + 1000 × µs tur, açık TSC hatası |

QEMU ölçümleri fiziksel PC sonucunu bekliyor. E4k açık. Yeni bir derleyici kusuru saptanmadı. Eski C UART beklemeleri ve tarihsel sabotajlarda kayıtsız açılış sayıları ilgili tablolarda belirtilmiştir.

## Heap ve fiziksel çerçeveler

Teslim kaynağının C derleyicili ilk düzenleyici oturumunda beş aç–kaydet–kapat çevrimi işlendi. Kabukta başlangıç ve son fiziksel boş çerçeve sayısı 47.885'ti. Aynı iki anda çekirdek heap'i 4.657.280 ve 16.790.576 bayttı. Ayrı üç aşamalı boru oturumunda C heap artışı 10.493.600 bayt, self-host artışı 10.821.616 bayt ölçüldü. Oturumları taze açılışlara bölmek uzun testlerin karşılaştırılmasını sağladı. Kalıcı tahsislerin geri kazanımı E4k çalışmasında açık.

İlk keşif amaçlı tam koşumun son `PIPESTATUS` değerleri `(2 0)` oldu. Üç başarısız hedef `calistir_bm_x86_usb`, `calistir_bm_x86_ahci` ve `calistir_bm_x86_disk` idi. Düzeltmenin ölçümü yukarıdaki G/Ç tablosunda, teslim kaynaklarıyla yapılan ayrı tam koşumun sonucu ilerleme belgesindedir.

## Son tam takım

Tam takımda host/konuk açılış medyanları C için 236.5/229.6 ms, self-host için 266.9/258.0 ms oldu. İki derleyicide de 650 ms sınırı korundu. PC matrisi 72/72. Yeni on QEMU kapısının 122 açılışında atlama yok.
