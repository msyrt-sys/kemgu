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
