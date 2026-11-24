# KEMGU-OS ağ yığını

## Kablo ve adresler

QEMU her Ethernet çerçevesini yerel UDP soketine tek datagram olarak verir. Eş ve QEMU kendi portlarını işletim sistemine ayırtır. İlk konuk çerçevesinin kaynak adresi dönüş yoludur. Böylece eşzamanlı açılışlar birbirinin trafiğini almaz. Host eşinin MAC adresi `02:51:59:17:00:01`, IPv4 adresi `192.0.2.1/24`, test için dağıttığı adres `192.0.2.2` olur.

## Katmanlar

| Katman | Sorumluluk | Kanıt |
|---|---|---|
| QEMU dgram ve filter-dump | Çerçeveyi taşımak ve iki yönde kaydetmek | `make calistir_ag_es` |
| Host eşi | ARP, ICMP echo, UDP echo ve DHCP yanıtı kurmak | Sabit tel örneği, iPXE DHCP istemcisi |
| Pcap okuyucu | Ethernet, IPv4, TCP, UDP ve ICMP başlıklarını ve sağlamalarını bağımsız okumak | Kesik, geçerli ve sağlaması hatalı kayıtlar |

Konuk tarafındaki geliştirme sırası sürücü, Ethernet ve IP, taşıma, soket ABI’si ve kullanıcı programlarıdır. H13 yol haritasındaki işaretler konukta hangi katmanın çalıştığını gösterir.

## Zaman, sınırlar ve hatalar

Host testi ilk DHCP işlemini en çok 20 saniye bekler. Yeniden gönderilen DISCOVER aynı işlem içinde geçerlidir. Pcap okuyucu IPv4 başlığının toplam boyunu ve başlık uzunluğunu denetler. Parçaların taşıma sağlaması ancak birleştirmeden sonra doğrulanabilir. Kısa kayıt, yanlış bağlantı tipi ve bozuk sağlama ayrı tanılar üretir. Ağ bayt sırası büyük endian’dır. [İnternet sağlaması, RFC 1071](https://www.rfc-editor.org/rfc/rfc1071.html)

`Hat` kayıp, çiftleme ve iki iletinin sırasını değiştirme kararlarını her koşuma özel tohumdan üretir. Son bekleyen çerçeve `bosalt()` ile teslim edilir. Giden IPv4 parçalarının sonuncusu hariç veri uzunlukları sekizin katıdır.

## Intel 82540EM

PCI kimliği `8086:100e` olan aygıtın BAR0 alanı kullanılır. Sıfırlama için üst sınır yaklaşık 100 ms’dir. RAL0 ve RAH0 donanımın MAC adresini verir. Alıcı ve vericide sekizer adet 16 baytlık tanımlayıcı ve her biri için 2048 baytlık tampon vardır. Sürücü bu alanları başlangıçta ayrılan fiziksel sayfalarda tutar. `ag` bağlantı durumunu, çerçeve ve bayt sayılarını, düşen çerçeveleri ve kesme vektörünü gösterir. `donanim` aynı satırı aygıt listesine yazar.

Q35’in tanınan kök yuvalarında sürücü IOAPIC üzerinden INTx yönlendirmesini kurar ve bir deneme kesmesiyle doğrular. Desteklenen bir yönlendirme bulunamazsa kabuk beklerken alıcıyı yoklar. `vektor -1` bu yolu gösterir. `ag dene` özel EtherType 0x88b5 ile 60 bayt yollar. `ag son` son çerçevenin FCS hariç baytlarını onaltılık gösterir. Gönderimde bağlantı kopuksa -100, boy geçersizse -90, halka doluysa -11 döner.

## PCI aktarımı ve kuyruklar

| PCI kimliği | Aygıt | Aktarım |
|---|---|---|
| `8086:100e` | Intel 82540EM | Eski tanımlayıcılar, INTx veya yoklama |
| `8086:10d3` | Intel 82574L | Eski tanımlayıcılar, kuyruk 0 ve MSI-X |
| `8086:10c9` | Intel 82576 | Gelişmiş tek tamponlu RX ve gelişmiş TX, kuyruk etkinleştirme, MSI-X |
| `1af4:1000`, `1af4:1041` | VirtIO ağ | Modern PCI yetenekleri, split RX/TX kuyrukları, yoklama |
| `10ec:8139` | Realtek RTL8139 | PCI raporunda desteklenmiyor |
| Realtek RTL8111/8168 | Dahili Ethernet | Sanal donanım doğrulaması bulunmuyor |

Düşük adresli halkalar için UEFI’nin açılışta fiziksel çerçevelerden ayırdığı 64 KiB VirtIO ağ alanı kullanılır. 82574L için yüksek bellek varsa firmware’in ConventionalMemory ve WB olarak bildirdiği hizalı bir alan eşlenir. Sürücü RX tabanının yüksek sözcüğünü aygıttan geri okuduktan sonra kuyruğu açar.

82576 SRRCTL ile tanımlayıcı türünü, RXDCTL/TXDCTL ile kuyrukları seçer. Gelişmiş RX geri yazımı tanımlayıcının özgün tampon adresini örter. Sürücü her tüketimde adresi geri koyar. 82574L tek MSI-X girdisine IVAR, 82576 GPIE ve IVAR0 ile yönlendirir. VirtIO sürücüsü VERSION_1, MAC ve STATUS özelliklerini uzlaştırır. PCI yeteneklerinin BAR sınırlarını ve kuyruk bildirim ofsetini denetler. Her NIC için `AgArayuz` bağlantı ve sayaç görünümü sunar.

RTL8139 bu dalganın desteklediği NIC listesine alınmadı. Emülasyonu olan bu aygıt, desteklenmeyen aygıtların raporda görünmesini doğrular. [QEMU 82574L uygulaması](https://github.com/qemu/qemu/blob/v8.2.2/hw/net/e1000e_core.c) [QEMU 82576 yazmaçları](https://github.com/qemu/qemu/blob/v8.2.2/hw/net/igb_regs.h)

## IPv4 ve komşu çözümü

`ag ayarla 192.0.2.2/24 192.0.2.1` statik adresi kurar ve gratuitous ARP yollar. Aynı IP'yi başka bir MAC kullanırsa seri uyarı ve çakışma sayacı artar. `ag sayac` kısa paket, sağlama, bilinmeyen protokol, dolu ARP tablosu, çakışma, TTL ve ICMP hata sayılarını gösterir. Önek dışındaki paketler yapılandırılmış ağ geçidine gider. IPv4 seçenekleri başlıktaki IHL ile atlanır.

ARP önbelleği 16 komşu tutar. Bir kayıt 30 saniye yaşar. Çözüm bekleyen dört çerçeve için fiziksel tampon ayrılır. Yeniden sorgu aralığı 250 ms, üst süre göndericinin zaman aşımıdır. Bekleme kuyruğu doluysa -105 alınır. Bağlantı kapalıysa -100, adres yoksa -99, yön yoksa -101, zaman dolarsa -110 döner.

`ping <ip> [adet [aralık_ms [zaman_aşımı_ms]]]` kullanıcı ELF'idir. Varsayılan dört istek, 1 saniye aralık ve istek başına 1 saniye sınır vardır. Çıktıda RTT mikrosaniyedir. `ag lo` yerel 127.0.0.0/8 arayüzünün sayaçlarını gösterir. Yerel echo istek ve yanıtları fiziksel NIC'e verilmeden IP ve ICMP katmanlarından geçer.

## UDP, DHCP ve DNS

UDP soketleri için 16 sabit port kaydı bulunur. Sıfır port geçersizdir (-22), kullanımda olan porta yeniden bağlanma -98, dolu tablo -105 döner. IPv4 UDP sağlaması sözde başlık dahil doğrulanır. Kapalı porta gelen istekler için ICMP port ulaşılamaz cevabı en çok 100 ms’de bir çıkar. `ag yanki` 9000 numaralı portta yankıyı açar.

Arayüz açılınca DHCP başlar. `ag durum` kira evresi, adres, DNS ve NTP sunucusunu gösterir. `ag dhcp` yeni işlem açar, `ag dhcp kapat` dinamik adresi bırakır. Statik adres ayarı da DHCP'yi kapatır. Yanıt XID, istemci MAC’i, seçenek sınırları ve seçili sunucuyla doğrulanır. T1'de unicast, T2'de broadcast REQUEST çıkar. Kira bittiğinde adres bırakılır. Seçenek 42 NTP sunucusunu taşır.

`cozumle <ad>` IPv4 A kaydı arar. En çok sekiz CNAME ve bir ad içinde en çok 32 geriye sıkıştırma işaretçisi izlenir. Soru adı, tür, sınıf, kaynak sunucu, port ve işlem kimliği yanıtla uyuşmalıdır. Önbellekte son başarılı ad, zincirin en kısa TTL'si kadar tutulur. Her DNS denemesi 650 ms ile sınırlıdır. Üç denemede ilk sunucu yanıtlamazsa ikinci sunucuya geçilir.

## IPv4 parçaları

Arayüz, kaynak adresi, hedef adresi, IPv4 kimliği ve protokolü aynı olan parçaları dört sabit yuvada toplar. Bir datagramın IP yükü en çok 12.288 bayttır. Her yuva ilk parçasından itibaren üç saniye boyunca tutulur. Çift veya örtüşen parçalarda her bayt için ilk gelen değer korunur. Birinci parça, son parça ve aradaki bütün baytlar gelince üst katman çerçeveyi alır. Son boyla çelişen parçalar reddedilir. Boş yuva kalmazsa yeni datagramın parçası düşürülür. Adres değişiminde bekleyen birleştirmeler temizlenir.

Yerel UDP çıkışında MTU 1500’dür. Giden son parça dışında yük uzunluğu sekizin katıdır. Aynı datagramın bütün parçalarında kimlik ortaktır ve başlık sağlaması ayrı hesaplanır. DF işaretli datagram için parçalama işlevi hata verir. TTL sıfır olan paket yerel alımda sayılır ve düşürülür. Çekirdek yönlendirme yapmaz. Kapalı bir UDP portuna ICMP 3/3 yanıtı 100 ms’de en çok bir kez gönderilir.

`ag parca`, `ag sayac` ve `donanim` birleştirilen datagramları, düşen parçaları veya zaman aşımına uğrayan yuvaları ve giden parçaları gösterir. x86 arayüzü dört yuva ve büyük gönderim tamponu için açılışta 77 fiziksel sayfa ayırır.

## TCP bağlantıları

TCP sıra ve ACK karşılaştırmaları 32 bit sarmalı hesaba katar. Etkin ve pasif açılışta MSS ilan edilir. Gönderim eşin penceresiyle sınırlıdır. Sıfır pencerede yoklama yeni sıra numarası tüketmez. Sekiz bağlantının her birinde ikişer 32 KiB halka bulunur. Dokuzuncu bağlantıya RST verilir. SYN için on saniye, FIN için on beş saniye, TIME-WAIT için dört saniye kullanılır. Sayılar yerel KEMGU profilinindir.

`ag tcp dinle` 9001 numaralı portta yankıyı açar. `ag tcp bagla <adres>` aynı porta 64 KiB gönderip dönen baytları karşılaştırır. `ag tcp durum` gönderilen ve doğrulanan miktarı, hata ve kapanış sayısını gösterir.

## TCP yeniden iletimi

SRTT ve RTTVAR milisaniye cinsindendir. İlk RTO bir saniye, alt sınır bir saniye ve üst sınır 60 saniyedir. Zaman aşımında RTO iki katına çıkar. Sekizinci zaman aşımında uygulamaya -110 döner. Yeniden gönderilen veriden RTT örneği çıkarılmaz. Tıkanıklık penceresi başlangıçta on MSS’dir. Yavaş başlangıcı toplamsal artış izler. Üç yinelenen ACK hızlı yeniden iletimi başlatır. Kayıpta eşik uçuşta olan veri miktarının yarısı, en az iki MSS olur. Alıcı, halkada bir bitişik sıra dışı aralık saklar. Bundan ayrık ikinci aralık gönderenin sonraki iletiminden alınır. `ag tcp sayac` yeniden iletim, reddedilen bağlantı ve hatayla kapanış sayılarını gösterir.

## TCP seçenekleri ve verim

Bir bağlantının alım ve gönderim halkaları 128 KiB’dir. SYN penceresi ölçeklenmez. Ölçek iki taraf da önerdiğinde kullanılır. Yerel kaydırma 2, kabul edilen en büyük kaydırma 14’tür. Zaman damgası anlaşılmışsa TSecr RTT ölçümüne katılır. PAWS, 24 gün içinde önceki zaman damgasından eski veriyi reddeder. İlk veri segmentinin ACK üst sınırı 25 ms’dir. İkinci segment hemen onaylanır. Nagle küçük yazıları uçuşta veri varken biriktirir. Keepalive boş bağlantıda üç yoklamadan sonra -110 verir.

Paralel QEMU yükünde ölçülen ilk 64 MiB aktarım 1,075 MiB/s sürdü. Kapı 0,25 MiB/s altını yakalar. Her ölçümde Python eşi ve ayrıca pcap okuyucusu bütün veri akışının SHA-256 değerini doğrular.

## Kullanıcı soketleri

16 soket yuvası toplam 208 fiziksel sayfada açılışta ayrılır. Her UDP soketi dört datagram saklar. Bir TCP soketi sekiz bağlantı yuvasından birini kullanır. Bekleyen çağrıda süreç bir sonraki zamanlayıcı tikine kadar uyur. Çoklu beklemenin listesinde soket, boru ve ön plan klavyesi bulunabilir. Süreçler soketi standart giriş ve çıkış olarak çocuğa aktarabilir. Son fd kapanınca TCP FIN gönderir. İmzalar ve tampon alanları sistem çağrısı ABI belgesindedir.
