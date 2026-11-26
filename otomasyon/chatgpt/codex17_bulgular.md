# Dalga 17 bulguları

## Başlangıç ve ağ eşi

QEMU sürümü 8.2.2. Makinedeki aygıt listesinde e1000, e1000e, igb, rtl8139, virtio-net-pci ve usb-net var. Bölüm 1’de açılan model e1000. Eş ve QEMU için IPv4 loopback üzerindeki geçici UDP portları çekirdek tarafından seçildi. iPXE başlangıcında DISCOVER yinelenebildi. Test, REQUEST içindeki işlem numarasını, seçilen adresi ve sunucu kimliğini önceki teklifle karşılaştırır. İşletim sistemi sürücüsünün ölçümleri sonraki bölümlere aittir.

İlk taban koşusunda iki bağımsız QMP soketinde klavye girdisi yanıtı beklenirken süre aşıldı. Makine yeniden başladı. İlk günlük `build/d17/taban.log`, tekrar günlüğü `build/d17/taban-ikinci.log` içinde. Zaman aşımının nedeni kesinleşmedi.

## 82540EM halka ve kaynak ölçümü

Sırayla C kök yuva, C köprü, self-host kök yuva, self-host köprü. Her açılışta 18 ayrı 60 baytlık test çerçevesi alınır. Kök yuvada kesme sayısının arttığı QMP bellek okumasıyla doğrulandı.

- NIC OK e1000 TX=1 RX=18 pcap; heap=4066880→4066880 boş=47822→47822
- NIC OK e1000 TX=1 RX=18 pcap; heap=4001296→4001296 boş=47822→47822
- NIC OK e1000 TX=1 RX=18 pcap; heap=4263728→4263728 boş=47793→47793
- NIC OK e1000 TX=1 RX=18 pcap; heap=4198144→4198144 boş=47793→47793

RDT negatifinde ikinci yerine sekizinci çerçevede alım durdu. Sekiz tanımlayıcı ile başlangıç RDT=7 değeri donanıma yedi kullanılabilir tanımlayıcı bırakıyor. Kâhin ilk eksik çerçeveyi bildiriyor.

## PCI ağ matrisi

Her derleyicide önce 256 MiB, sonra 512 MiB (82574L için 4 GiB). Her satır yeni imaj, yeni QEMU süreci ve yeni pcap ile ölçüldü.

- NIC OK e1000 TX=1 RX=18 pcap; heap=4066880→4066880 boş=47831→47831
- NIC OK e1000 TX=1 RX=18 pcap; heap=4263664→4263664 boş=113367→113367
- NIC OK e1000e TX=1 RX=18 pcap; heap=4066880→4066880 boş=47831→47831
- NIC OK e1000e TX=1 RX=18 pcap; heap=5050352→5050352 boş=506587→506587
- NIC OK igb TX=1 RX=18 pcap; heap=4066880→4066880 boş=47831→47831
- NIC OK igb TX=1 RX=18 pcap; heap=4263664→4263664 boş=113367→113367
- NIC OK virtio-net-pci TX=1 RX=18 pcap; heap=4066880→4066880 boş=47831→47831
- NIC OK virtio-net-pci TX=1 RX=18 pcap; heap=4263664→4263664 boş=113367→113367
- NIC OK e1000 TX=1 RX=18 pcap; heap=4263728→4263728 boş=47800→47800
- NIC OK e1000 TX=1 RX=18 pcap; heap=4460512→4460512 boş=113336→113336
- NIC OK e1000e TX=1 RX=18 pcap; heap=4263728→4263728 boş=47800→47800
- NIC OK e1000e TX=1 RX=18 pcap; heap=5247200→5247200 boş=506556→506556
- NIC OK igb TX=1 RX=18 pcap; heap=4263728→4263728 boş=47800→47800
- NIC OK igb TX=1 RX=18 pcap; heap=4460512→4460512 boş=113336→113336
- NIC OK virtio-net-pci TX=1 RX=18 pcap; heap=4263728→4263728 boş=47800→47800
- NIC OK virtio-net-pci TX=1 RX=18 pcap; heap=4460512→4460512 boş=113336→113336

82576 sıfırlamasından sonra QEMU STATUS.LU kendiliğinden yükselmedi. PHY 1 BMCR ile görüşmeyi yeniden başlatınca gerçek bağlantı, kesme ve RX/TX çalıştı. Yüksek DMA negatifinde yanlış adrese DMA yapılmadan donanım geri okuması açık hata üretti.

## ARP ve ICMP

- 100 ping heap 4066880→4066880 boş 47818→47818
- HEAP 'ping 192.0.2.1 20 0 1000' +721552 toplam=4788432
- HEAP 'ping 127.0.0.1 3 0 1000' +0 toplam=4788432
- HEAP 'ping 192.0.2.1 1 0 1000' +0 toplam=4788432
- L3 OK dgram 249
- HEAP 'ping 10.0.2.2 2 0 1000' +721552 toplam=4788432
- L3 OK slirp 7
- 100 ping heap 4263728→4263728 boş 47786→47786
- HEAP 'ping 192.0.2.1 20 0 1000' +721584 toplam=4985312
- HEAP 'ping 127.0.0.1 3 0 1000' +131168 toplam=5116480
- HEAP 'ping 192.0.2.1 1 0 1000' +131168 toplam=5247648
- L3 OK dgram 249
- HEAP 'ping 10.0.2.2 2 0 1000' +721584 toplam=4985312
- L3 OK slirp 7

Kâhin, her kullanıcı programı çalıştırıldıktan sonra seri RTT örneklerini özetle karşılaştırır. İlk kullanıcı ELF yüklemesinde ölçülen heap artışı yukarıda ayrıca görünür.

### C17-001 — ham işaretçiden yapı alanına yazma

Aşağıdaki girdi için iki derleyici de tip denetimini geçirdi. Beklenen çıkış 0, ölçülen çıkış 1. Gerçek konukta GARP doğru adresten çıkarken yapıdaki IPv4 adresi 0 kalıyordu. Yapıyı yerele kopyalayıp değiştirerek bütün olarak geri yazmak sorunu dolandı.

```kem
yapı Konum { ip: tam64; }
işlev main() -> tam32 { güvensiz {
    değişken k: Konum = Konum {ip:1};
    değişken p: *Konum = (&k) olarak *Konum;
    (*p).ip=7;
    eğer k.ip!=7 { ver 1; }
    ver 0;
} }
```

Her `c` için (`kemgu`, `codegen`) komutlar `build/$c --llvm c17_001.kem > ornek.ll`, `clang ornek.ll build/kdl_runtime.o -lm -o ornek`, `./ornek`.

## UDP, DHCP ve DNS ölçümleri

- 1000 UDP heap 4066880→4066880 boş 47808→47808, T1 4.130s T2 7.227s
- UDP OK dgram 2044
- UDP OK slirp 5
- 1000 UDP heap 4263728→4263728 boş 47776→47776, T1 4.022s T2 7.030s
- UDP OK dgram 2044
- UDP OK slirp 5

Otomatik DHCP trafiğinden sonra 82574L 18 çerçeveyi aldı, fakat MSI-X sayacı artmadı. EIAC ilk kesmede neden bitinin yanında IMS maskesini de kaldırıyordu. CTRL_EXT.IAME açıldı, IAM sıfırlandı. Düzeltilen sürücüde kâhin her ayrı RX çerçevesi için kesme sayacının yeniden arttığını doğruladı. Dört NIC, iki bellek düzeni ve iki derleyicide matris yeniden geçti.

## Parçalama ve birleştirme

Gelen üç datagramda sırasıyla yedi, beş ve altı parça kaydedildi. İlkinde bir parça yinelendi. Ortadaki datagramın üçüncü saniyede süresi doldu. İki tam yanıtta altışar parça vardı. Host her IP sağlama toplamını, UDP sözde başlık sağlamasını, ofsetleri, MF bitini ve SHA-256 sonucunu doğruladı. TTL sıfır olan ayrı datagram için konuk sayacı bir arttı.

- PARCA OK 8192 bayt SHA256=3b1ca569e7021f5d25ed0b9b92dabe731b87b225ddcfb522448db79ac02e2474 iki yanıt, süre aşımı 1, heap=4066880 boş=47729
- PARCA OK 8192 bayt SHA256=3b1ca569e7021f5d25ed0b9b92dabe731b87b225ddcfb522448db79ac02e2474 iki yanıt, süre aşımı 1, heap=4263728 boş=47697

## TCP temel aktarımı

- TCP OK 65536 bayt pasif=9a73523567c3b7d85f8f63c1591f546e6e1d0f8fb37be17db28cdf9644f540ec aktif=51fe7c95debcfefba857851144285f2450a5f2355c37dd6d99a5733b73e0ae6a, loopback 65536, pcap=300, heap=4066880 boş=47199
- TCP OK 65536 bayt pasif=9a73523567c3b7d85f8f63c1591f546e6e1d0f8fb37be17db28cdf9644f540ec aktif=51fe7c95debcfefba857851144285f2450a5f2355c37dd6d99a5733b73e0ae6a, loopback 65536, pcap=300, heap=4263728 boş=47168

## 4 MiB kayıplı TCP aktarımı

- TCP KAYIP OK 4194304x2 SHA256=fd9565e7e4f3b08e85dee2ccabed09e07c3e34756de9d801dd6c44455135090b süre=152.556s konuk_tekrar=141 pcap_tekrar=140 eş_tekrar=505 kayıp/sıra/çift=[(219, 214, 79), (333, 321, 148)] heap=4066880 boş=47196
- TCP KAYIP OK 4194304x2 SHA256=fd9565e7e4f3b08e85dee2ccabed09e07c3e34756de9d801dd6c44455135090b süre=159.619s konuk_tekrar=137 pcap_tekrar=136 eş_tekrar=554 kayıp/sıra/çift=[(210, 209, 79), (368, 289, 150)] heap=4263728 boş=47165

## 64 MiB TCP aktarımı ve seçenekler

- TCP SECENEK OK ACK 0.031s, pencere 131068, keepalive=3 heap=4066880 boş=45658
- TCP SECENEK OK ACK 0.031s, pencere 131068, keepalive=3 heap=4263728 boş=45628
- TCP VERIM OK olcekli 64 MiB süre=65.308s hız=0.980 MiB/s QEMU_CPU=49.55 SHA256=9de7fd40017b1af57ae88b1b75ce0efe74b9b464b7a71009ae1b0dfccdb1b814 heap=4066880 boş=45658
- TCP VERIM OK olceksiz 64 MiB süre=60.051s hız=1.066 MiB/s QEMU_CPU=47.67 SHA256=9de7fd40017b1af57ae88b1b75ce0efe74b9b464b7a71009ae1b0dfccdb1b814 heap=4066880 boş=45658
- TCP VERIM OK olcekli 64 MiB süre=106.508s hız=0.601 MiB/s QEMU_CPU=88.82 SHA256=9de7fd40017b1af57ae88b1b75ce0efe74b9b464b7a71009ae1b0dfccdb1b814 heap=4263728 boş=45628
- TCP VERIM OK olceksiz 64 MiB süre=106.494s hız=0.601 MiB/s QEMU_CPU=89.03 SHA256=9de7fd40017b1af57ae88b1b75ce0efe74b9b464b7a71009ae1b0dfccdb1b814 heap=4263728 boş=45628

## Soket ABI ve süreç ömrü

- soket heap/boş çerçeve (4066880, 45441) (5050768, 45441)
- soket heap/boş çerçeve (4263728, 45411) (5509984, 45411)

Klavye/boru/UDP sınamasında dolu boruyu fd listesinde bekleyen süreç 100 ms içinde döner. Borunun hazır biti kapatılmış ayrı C imajında konuk çıkışı 15 oldu. Aynı test normal imajlarda geçer. Yükleyici ve kullanıcı bağlamları heap kullanır, iki derleyicide de süreçler kapanınca ayrılan fiziksel çerçeveler geri geldi.

### C17-002 — iç blokta yapı okumasının IR tipi

Gerçek çekirdek bağlamında en küçük değişiklik, `ag_tcp_adim` içindeki `sk_tcp_sonlandi(sahip,s.hata_kodu)` çağrısının yerine aşağıdaki gövdeyi koymaktır. Bağımsız iki yapılı küçük program aynı sorunu göstermedi.

```kem
değişken q: *SkKayit = sahip olarak *SkKayit; değişken sok: SkKayit = *q;
sok.durum=4; sok.tcp=0; sok.hata_kodu=s.hata_kodu olarak tam64;
eğer sok.dinleyen!=0 { sok.ref=0; } *q=sok;
```

Her derleyici için `UEFI_DERLEYICI=build/kemgu UEFI_YALNIZ_DERLE=1 UEFI_KAYNAK=test/ornekler/kem_os_x86_kabuk.kem bash test/bm_x86_uefi_harness.sh` uygulanır. İkinci çalıştırmada `build/codegen` seçilir. Beklenen iki çekirdeğin de bağlanması. C geçti, self-host IR doğrulamasında `load volatile %TcpBag` sonucunu `store %SkKayit` satırında kullanarak LLVM tür hatası verdi. Tek yapı türüyle çalışan kapatma yardımcısında iki imaj da bağlandı ve QEMU sınaması geçti.

## Kullanıcı araçlarında tel ve bellek

- araç heap/boş çerçeve (4066880, 45439) (14954304, 45439)
- ✓ nc iki yön boru 1 MiB SHA256 a4a3ec30d6388244ded06c36c1c7f501ee42fd7940c04350c6e367a644220313 c70fa909b5587a8d363b6dec6a33d6ad3794c5f74fbca496bddd27b25db19b5d UDP, FAT32, loopback, QMP ve pcap
- araç eş heap/boş çerçeve (4066880, 45439) (6952800, 45439)
- araç heap/boş çerçeve (4263728, 45409) (15151280, 45409)
- ✓ nc iki yön boru 1 MiB SHA256 a4a3ec30d6388244ded06c36c1c7f501ee42fd7940c04350c6e367a644220313 c70fa909b5587a8d363b6dec6a33d6ad3794c5f74fbca496bddd27b25db19b5d UDP, FAT32, loopback, QMP ve pcap
- araç eş heap/boş çerçeve (4263728, 45409) (7739936, 45409)

Dinleyicinin son FIN’i tüketmesi ile istemcinin kabuğa dönmesi farklı süreç dilimlerinde gerçekleşebilir. Ölçüm en çok beş saniyede süreç çerçevelerinin başlangıç değerine dönmesini gerektirir. İki yönlü akışın SHA-256 değerleri tel kaydından yeniden kurularak doğrulandı.

## HTTP ve dosya akışı

Önceki dosya yolunda, QEMU'da gerçek kullanıcı ELF'i 65.536 konumuna gidip tek bayt yazdı ve `-27` aldı. Eski yol her yazıda dosyanın tamamını yeniden kuruyordu. ABI 89'un fiziksel çerçevelerde biriktirme ve yayımlama yolu 8.388.608 baytlık indirmeyi USB FAT32'ye yazdı. Host imajının SHA-256 özeti `1c9f2482f5b03efac6b414668dd3263702fa646d8cf9522d6c0b79a9a3f4c82f` çıktı. Eski hedefe gelen eksik HTTP yanıtı dosyayı korudu. İstemci için seçilen üst sınır 16 MiB. Kısa dosya ve `/ram` aynı çağrıdan geçiyor. Dosya sistemi ayrı bir FAT32 günlüğü tutmuyor. Meta veri yazılırken donanım hatası oluşursa bağlama, önceki FAT32 yazma yolundaki gibi kirli kalabiliyor.

Ayrı bir C konuğunda, var olan 155.649 baytlık dosya önce 73.793, ardından 1.807 baytla değiştirildi. Host her yayımdan hemen sonra gerçek imajı okudu. İki içerik birebir eşleşti ve kapanıştan sonra `fsck.fat -n` çıkışı 0 oldu.
- HTTP istemci heap/boş çerçeve (4066880, 45434) (10560176, 45434)
- HTTP sunucu heap/boş çerçeve (4066880, 45434) (13248864, 45434)
- HTTP NVMe/AHCI heap/boş çerçeve (4066880, 45434) (7674352, 45434)
- HTTP istemci heap/boş çerçeve (4263728, 45404) (11281824, 45404)
- HTTP sunucu heap/boş çerçeve (4263728, 45404) (13773696, 45404)
- HTTP NVMe/AHCI heap/boş çerçeve (4263728, 45404) (8199280, 45404)

## SNTP tel ve RTC ölçümü

QEMU QMP `/machine rtc-time` alanını konuğun istek ve cevap kaydından bağımsız okuduk. 2031-07-15 12:34:56 otomatik, 13:34:56 elle ayarlandı. 150 ms yapay gecikme ve 20 ms sunucu işlem süresinde gecikme ve tel zamanlarından hesaplanan ofset karşılaştırıldı. Dönem mutasyonu 2031 denemesinde tarih aralığına takıldı. 2024 eşinde QMP’nin 2094’e gittiği ayrıca ölçüldü. [QEMU 8.2.2 RTC aygıtı](https://github.com/qemu/qemu/blob/v8.2.2/hw/rtc/mc146818rtc.c)

- SNTP QMP 2031-07-15T13:34:56+00:00 ofset_us 3598289911 gecikme_us 129054 heap/çerçeve (4198048, 45430) (4854016, 45430)
- SNTP QMP 2031-07-15T13:34:56+00:00 ofset_us 3598456254 gecikme_us 128625 heap/çerçeve (4329312, 45400) (5050896, 45400)

## TFTP ve teşhis ölçümleri

Kayıp her yirminci veri bloğunun ilk denemesine uygulanır. Okuma ve yazma için son kısa DATA sıfır uzunluklu olabilir. Standart girdiye bağlanan salt okunur anlık görüntü süreç kapanınca geri verilir. 16 KiB yinelenen yazma deneyinde FAT32 karması 01488e78f48dfd40471b4cc09d82fd42060c7f9bfe4a6758f2f677d69d121253, beklenen c6962ef93671130e5598944c24b8c65254cf4f8046916373a59c2515d59c478b. Son kısa bloğu yok sayan konuk 33. blokta bekleme bütçesini tüketti ve kabuğa döndü.

- TFTP heap/boş çerçeve (4591616, 45430) (7411952, 45430)
- ✓ TFTP FAT32, iki yön pcap ve eş SHA256 cfc8d96831a8bb2b7e72888555a48f09957608057e793e5d9b652eddb733b9c7 bayt 2097152 yön başına kayıp 205 yinelenen 41
- Teşhis heap/boş çerçeve (5313072, 45430) (5837808, 45430)
- ✓ Teşhis alıcı + pcap 1041 92ff22d8db7a919c56c7b6575a700acc8771ed8978d4e36d204931eee476bb50
- ✓ Teşhis alıcı + pcap 1945 86c1574f54cc05a3e23cd7930d5322f8ed193c4d48ae18404d4d548e8fb1d010
- TFTP heap/boş çerçeve (4788496, 45399) (7805680, 45399)
- ✓ TFTP FAT32, iki yön pcap ve eş SHA256 cfc8d96831a8bb2b7e72888555a48f09957608057e793e5d9b652eddb733b9c7 bayt 2097152 yön başına kayıp 205 yinelenen 41
- Teşhis heap/boş çerçeve (5575568, 45399) (6297024, 45399)
- ✓ Teşhis alıcı + pcap 1041 3e6618141ddebb7f8981e3fbb507cd41188fa77a0e2562911c3d49ddfe020a3d
- ✓ Teşhis alıcı + pcap 1945 ba81d309326709c4d783b082d48148cb90e0cb6623f258f7a120a6ff3f4ecf8d

## Paket deposu ve SHA256

`stdlib/kripto/karma.kem` içindeki işlevler modül dışına açık değil (seçili import T041). Kullanıcı ELF derleme adımı aynı kaynak dosyasını paket derleme birimine ekler. Kütüphanenin bağımsız kopyası yoktur. Clang 18.1.3 genel yazmaç hedefine LLVM SSE2 LFENCE içselini seçemedi. Bu hedef için aynı LFENCE makine komutu ve derleyici bellek bariyeri üretildi. llvm-objdump çıktısında 0f ae e8 doğrulandı. Her SHA-256 sıkıştırmasının geçici bölgesi bırakılır. 16 MiB üst sınır manifest ve aktarım boyunca korunur. Büyük geçici dosyayı silmeden önce ABI 89 ile zinciri serbest bırakmak gerekiyor. İlk 524349 baytlık deneyde ELF ve kayıt doğru yayımlandı ama normal dizin silme planı uzun zinciri bırakamadı.

E4k açık. İlk tek açılış denemesinde dizin işlemleri ve ELF çağrıları çekirdek heap değerini 25.907.248 bayta taşıdı, ardından USB FAT32 erişimi hata verdi. Liste okumaları ve gereksiz dizin mutasyonları azaltıldı. Kurulum/tamlık ve bozuk dosya/kaldırma dizileri aynı FAT32 imajını kullanan iki gerçek açılışta ölçüldü. Bu ayrıca kurulu kaydının yeniden açılışta geçerli olduğunu sınar.

- Paket heap/boş çerçeve (4526032, 45429) (20397936, 45429)
- Paket heap/boş çerçeve (4526032, 45429) (17840160, 45429)
- ✓ Paket HTTP + FAT32 SHA256 3d75013fcb941e48dda94ed72cfa785bd19bbe1b85eadb5a52f92e77a419bd03 çalıştırma 42, bozuk paket ve geçici dosya yok, fsck temiz
- ✓ Büyük paket 524349 870202797a69a71dd741d3ec78e77f122bd33a2b2b3924957d6ce2014141438b heap/boş çerçeve (4526032, 45429) (18299120, 45429)
- Paket heap/boş çerçeve (4788496, 45399) (21316400, 45399)
- Paket heap/boş çerçeve (4788496, 45399) (18824208, 45399)
- ✓ Paket HTTP + FAT32 SHA256 3d75013fcb941e48dda94ed72cfa785bd19bbe1b85eadb5a52f92e77a419bd03 çalıştırma 42, bozuk paket ve geçici dosya yok, fsck temiz
- ✓ Büyük paket 524349 870202797a69a71dd741d3ec78e77f122bd33a2b2b3924957d6ce2014141438b heap/boş çerçeve (4788496, 45399) (19086384, 45399)

Geçici dosyayı silme çağrısını kaldırmak gerçek QEMU çalışmasında `bozuk paketin gecici dosyasi kaldi` doğrulamasını kırdı. SHA doğrulamasının atlanmasıyla bozuk kodun kurulmasını sağlayan sabotaj uygulanmadı.

## USB Ethernet ölçümleri

Eski xHCI ilk yapılandırmayı kurarken `xhci ayni yon coklu uc destegi yok` hatasında duruyordu. CDC denetim ucuyla iki toplu uç ayrı halkalara alındı. QEMU'nun bildirimi kontrol arayüzü 0 yerine veri arayüzü 1'i gösteriyor. Konuk MAC adresini dizgiden `40:54:00:17:00:16` olarak okudu. İlk ping denemesinde pcap her iki yanıtı da gösterdiği halde kullanıcı ELF'i iki kez -110 aldı. Senkron çağrı kesmeleri beklerken ISR sayacı ilerlemiyordu. ECM aktarımı beklenirken ortak olay halkasını yoklamak bu durumu giderdi. Hub üzerinde firmware USB önyüklemesi OVMF kabuğuna düştü. Aynı USB hub korunarak NVMe önyükleme seçildi. Kök port senaryosu USB'den açıldı.

MAC dizgisinin hanelerini bir konum döndürme sabotajı gerçek QEMU'da altı DHCP isteği üretti. Pcap kaynak adresi `64:05:40:01:70:01`, eşten OFFER sayısı sıfır ve kira kapısının çıkışı 1. Sabotaj yalnız ayrı derleme kopyasındaydı.

Ek QEMU koşumunda PCI envanterinde `10ec:8139` Realtek ile aynı makinede CDC-ECM seçildi. Aynı MAC, 2 MiB dosya ve paket kurulumu denetimleri geçti. Ham eşin toplam uzunluğu tam 64 bayt olan ICMP yanıtı pcap'te tek Ethernet çerçevesi olarak görüldü.

Aynı MAC sabotajı slirp ile de çalıştırıldı. Slirp değişmiş MAC adresine DHCP yanıtı verdi. Bu topolojide kapıyı kıran denetim konuk arayüzünün beklenen MAC adresiydi (rc=1). Ham eş topolojisinde kapıyı kira zaman aşımı kırdı.

## ARM ağı

Transport ortak AgArayuz kayıtlarını tutuyor. MAC ve isteğe bağlı link durumu virtio özellik uzlaşmasından geliyor. RX kopyası tamamlanınca used kimliği available halkasına tek kez verilir. ARM zaman kaynağı CNTVCT_EL0 ve CNTFRQ_EL0. Önceki ARM örneğinin ARP ve ICMP tel kurucuları kaldırıldı. Protokol kodu iki mimaride aynı modüllerden derlendi. TCP ve HTTP için ARM kullanıcı soket ABI’si eklemek yerine istenen çekirdek fazı seçildi. Kısıtlı HTTP fazı Content-Length içeren 200 yanıtını ortak başlık ayrıştırıcısıyla okur. Sonuç yalnız işaretlenmiş QEMU test diskinin ayrılmış alanına yazılır. Bu ölçüm RAM içeriğinin ötesinde kalıcı baytların doğruluğunu kapsar.

- ✓ ARM virtio-mmio DHCP ARP ping TCP 1 MiB SHA256 a4a3ec30d6388244ded06c36c1c7f501ee42fd7940c04350c6e367a644220313 HTTP 1048459 d2599a3a7d32cca7ba20fc6026fd15db0344be41eb35aed639e405e379ad7da6 saniye 8.8
- ✓ ARM virtio-mmio DHCP ARP ping TCP 1 MiB SHA256 a4a3ec30d6388244ded06c36c1c7f501ee42fd7940c04350c6e367a644220313 HTTP 1048459 d2599a3a7d32cca7ba20fc6026fd15db0344be41eb35aed639e405e379ad7da6 saniye 10.44
- kemgu RX iadesiz: ping yeşil, gerçek TCP kırmızı, tel çerçevesi 54
- codegen RX iadesiz: ping yeşil, gerçek TCP kırmızı, tel çerçevesi 54

## Bölüm 18 — sabit tohum, kaynaklar ve bağlantı kesintisi

Tohum 170018. Her derleyicide 13 × 1024 protokol örneği. Canlı 10.000 çerçeve dört sınıftan oluştu. Uygulama protokollerinin bozuk girdileri host korpusunda çalıştı. Sonraki sağlıklı HTTP, TCP ve ICMP işlemleri aynı çekirdekte gerçekleşti. Tablo testi ağdan trafik üretmeden gerçek çekirdek ayırma ve bırakma işlevlerini çağırdı. ARP ve UDP port tablosu ömür ve doluluk kurallarını iki host derleyicisiyle sınadı. TCP tablosunda kapanmış durumun geri kazanımı ve fragment süresinin dolması sınandı. 200 MiB, 100 MiB gidiş + 100 MiB dönüş olarak ölçüldü.

| Derleyici | İş | Çekirdek heap, bayt | Boş fiziksel çerçeve |
|---|---|---:|---:|
| build/kemgu | 100-tablo-turu | 4198048 → 4198048 | 45422 → 45422 |
| build/kemgu | 10000-karisik | 4198048 → 4198048 | 45422 → 45422 |
| build/kemgu | 10000-ping | 4198048 → 4198048 | 45422 → 45422 |
| build/kemgu | 500-tcp | 4198048 → 4198048 | 45422 → 45422 |
| build/kemgu | 100-http | 5509920 → 27480560 | 41828 → 41828 |
| build/kemgu | 200-MiB-toplam | 4198048 → 4198048 | 45422 → 45422 |
| build/codegen | 100-tablo-turu | 4329312 → 4329312 | 45391 → 45391 |
| build/codegen | 10000-karisik | 4329312 → 4329312 | 45391 → 45391 |
| build/codegen | 10000-ping | 4329312 → 4329312 | 45391 → 45391 |
| build/codegen | 500-tcp | 4329312 → 4329312 | 45391 → 45391 |
| build/codegen | 100-http | 5903552 → 27939776 | 41797 → 41797 |
| build/codegen | 200-MiB-toplam | 4329312 → 4329312 | 45391 → 45391 |

- AG18 saglama/bilinmeyen artisi (5000, 2500)
- AG18 link off/on TCP yeniden iletim 24187226 1400
- AG18 iki yön 100+100 MiB SHA256 0742ba24c68d455b95ad0807a4b5d33c36c235ab2de58626bd87082721a83d1e saniye 284.77
- AG18 DNS ve NTP zaman asimi, DHCP kira bitisi ve yeni ACK
- AG18 saglama/bilinmeyen artisi (5000, 2500)
- AG18 link off/on TCP yeniden iletim 24738354 1400
- AG18 iki yön 100+100 MiB SHA256 0742ba24c68d455b95ad0807a4b5d33c36c235ab2de58626bd87082721a83d1e saniye 462.46
- AG18 DNS ve NTP zaman asimi, DHCP kira bitisi ve yeni ACK

HTTP sırasında çekirdek heap’i büyüyor (E4k). Bu dalga geri kazanım uygulamadı. Üç ayrıştırıcı sabotajı uygulanmadı. Sağlam korumaların reddettiği girdi şekilleri iki derleyiciyle ayrıca doğrulandı.

## İki konuk ve karma PC matrisi

QEMU stream arka ucunun UNIX adresi her koşuma özeldir. Sunucu ucunu x86 açar, ikinci x86 ya da ARM bağlanır. Host trafiği yönlendirmez. x86 HTTP sunucusu depo bildirimini ve ELF baytlarını sundu. NC ve TFTP dosyaları iki ayrı konuk diski üzerinden dışarıdan okundu. TFTP alıcı fikstürü tek WRQ ve sabit çıktı için sınırlıdır. ARM testinde iki yöndeki pcap TCP sıra uzayları ve konuk diski kaynakla birebir eşleşti. Matris satırları C ve self-host sırasındadır. İkinci aşamada fiziksel donanım koşumu yapılmadı.

- AG19 PC e1000e nvme 512 MiB DHCP ping indir paket SHA256 204b6c159df1d960b6a4d660bd013844442bcc0fabdb99b4111e591c5599d3f7 (4329248, 110959) (19282976, 110959)
- AG19 PC e1000 usb 256 MiB DHCP ping indir paket SHA256 204b6c159df1d960b6a4d660bd013844442bcc0fabdb99b4111e591c5599d3f7 (4198048, 45423) (20529040, 45423)
- AG19 PC igb sata 256 MiB DHCP ping indir paket SHA256 204b6c159df1d960b6a4d660bd013844442bcc0fabdb99b4111e591c5599d3f7 (4132464, 45423) (19217360, 45423)
- AG19 PC virtio-net-pci usb 512 MiB DHCP ping indir paket SHA256 204b6c159df1d960b6a4d660bd013844442bcc0fabdb99b4111e591c5599d3f7 (4329248, 110959) (20660240, 110959)
- AG19 PC usb-net sata 512 MiB DHCP ping indir paket SHA256 204b6c159df1d960b6a4d660bd013844442bcc0fabdb99b4111e591c5599d3f7 (4329248, 110959) (19414144, 110959)
- AG19 PC hub nvme 256 MiB DHCP ping indir paket SHA256 204b6c159df1d960b6a4d660bd013844442bcc0fabdb99b4111e591c5599d3f7 (4263632, 45423) (19217360, 45423)
- AG19 PC e1000e sata 1024 MiB DHCP ping indir paket SHA256 204b6c159df1d960b6a4d660bd013844442bcc0fabdb99b4111e591c5599d3f7 (4591520, 242031) (19742000, 242031)
- AG19 PC virtio-net-pci nvme 256 MiB DHCP ping indir paket SHA256 204b6c159df1d960b6a4d660bd013844442bcc0fabdb99b4111e591c5599d3f7 (4132464, 45423) (19086192, 45423)
- AG19 PC e1000e nvme 512 MiB DHCP ping indir paket SHA256 204b6c159df1d960b6a4d660bd013844442bcc0fabdb99b4111e591c5599d3f7 (4460512, 110928) (20135824, 110928)
- AG19 PC e1000 usb 256 MiB DHCP ping indir paket SHA256 204b6c159df1d960b6a4d660bd013844442bcc0fabdb99b4111e591c5599d3f7 (4329312, 45392) (21185136, 45392)
- AG19 PC virtio-net-pci usb 512 MiB DHCP ping indir paket SHA256 204b6c159df1d960b6a4d660bd013844442bcc0fabdb99b4111e591c5599d3f7 (4460512, 110928) (21381920, 110928)
- AG19 PC igb sata 256 MiB DHCP ping indir paket SHA256 204b6c159df1d960b6a4d660bd013844442bcc0fabdb99b4111e591c5599d3f7 (4329344, 45392) (19939072, 45392)
- AG19 PC usb-net sata 512 MiB DHCP ping indir paket SHA256 204b6c159df1d960b6a4d660bd013844442bcc0fabdb99b4111e591c5599d3f7 (4526128, 110928) (20135856, 110928)
- AG19 PC hub nvme 256 MiB DHCP ping indir paket SHA256 204b6c159df1d960b6a4d660bd013844442bcc0fabdb99b4111e591c5599d3f7 (4394896, 45392) (20070208, 45392)
- AG19 PC e1000e sata 1024 MiB DHCP ping indir paket SHA256 204b6c159df1d960b6a4d660bd013844442bcc0fabdb99b4111e591c5599d3f7 (4788400, 242000) (20398128, 242000)
- AG19 PC virtio-net-pci nvme 256 MiB DHCP ping indir paket SHA256 204b6c159df1d960b6a4d660bd013844442bcc0fabdb99b4111e591c5599d3f7 (4263728, 45392) (19939040, 45392)
- AG19 x86 x86 HTTP nc TFTP ve konuk paket deposu SHA256 dca0ee3c03813f420ee602f1ecbd24a174c7913436aff655d797f686af5a9137 [(4198048, 45423), (4198048, 45423)] [(20791152, 45423), (22824544, 45423)]
- AG19 x86 ARM iki yon 1 MiB SHA256 a4a3ec30d6388244ded06c36c1c7f501ee42fd7940c04350c6e367a644220313 heap/cerceve (4198048, 45423)
- AG19 ARP zaman asimi ve kabuk hazir b'PING HATA -110\nPING alinan 0 / 1\n[ping.elf pid=1 cikis=1]\nKABUK_HAZIR\n'
- AG19 x86 x86 HTTP nc TFTP ve konuk paket deposu SHA256 dca0ee3c03813f420ee602f1ecbd24a174c7913436aff655d797f686af5a9137 [(4329312, 45392), (4329312, 45392)] [(20922512, 45392), (23808560, 45392)]
- AG19 x86 ARM iki yon 1 MiB SHA256 a4a3ec30d6388244ded06c36c1c7f501ee42fd7940c04350c6e367a644220313 heap/cerceve (4329312, 45392)
- AG19 ARP zaman asimi ve kabuk hazir b'PING HATA -110\nPING alinan 0 / 1\n[ping.elf pid=1 cikis=1]\nKABUK_HAZIR\n'

## Dalga 17 son ölçümü

Beş belge programı × iki derleyici ve 48 yardım konusu geçti. Örnekler doğrudan Markdown kod bloklarından çıkarıldı. ICMP loopback ve zaman sınırı olan iki UDP tanıtıcısına ayrı ayrı gerçek girdi verildi. `make test_tumu` ve ardından check / bare-metal fark koşumları çıkış 0. Atlanan 70 girdi ve sınıfları ilerleme belgesinde.

Kalanlar fiziksel PC doğrulaması, E4k çekirdek heap geri kazanımı, envanterde açıkça listelenen ağ sabotajları ve C17-001/C17-002 derleyici bulgularıdır. Canlı karışık çerçeve testi dört sınıf yük kullandı. On üç protokol ailesinin sınır girdileri host tarafında çalıştı.
