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
