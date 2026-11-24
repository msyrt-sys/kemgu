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
