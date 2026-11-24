# Dalga 17 ilerlemesi

24 Kasım 2026 saat 23.02 TRT’de D-769’u taşıyan `origin/main` sürümü `a1617f1` üzerinde çalışmaya başlandı. C ve self-host derleyicileri ile host çalışma zamanı derlendi.

## Bölüm 1 — Ethernet eşi ve pcap

Host eşi ile bağımsız pcap okuyucusu eklendi. Sabit IPv4 ve ICMP paketi, RFC 1071 sözcükleri, UDP ve TCP sağlamaları, iki pcap bayt sırası, parça ofsetleri, kayıp, çiftleme ve sıralama testleri geçti. Gerçek iPXE istemcisi eşin OFFER paketinde dağıttığı adresi ve sunucuyu REQUEST içinde geri gönderdi. Çerçeveler QEMU filter-dump kaydıyla birebir eşleşti. IPv4 başlığının son baytını sağlama hesabından çıkarmak sabit paket testini kırdı (çıkış 1). H12b, S223 ve D-767 kanıtıyla güncellendi.

Beş taban kapısı yeniden koşulup geçti. Bölümün host/QEMU kapısı ve §7’deki yedi regresyon kapısı kalan son doğrulama dahil 1742 saniyede tamamlandı. İlk toplu koşumda 100 çocuklu test süre sınırına ulaştı. Çok süreç kapısı özgün tekrar sayısı ve süre sınırıyla hızlı CPU kümesinde yeniden çalıştırılıp geçti. Başlangıçtan bölüm kapanışına 75 dakika geçti.

## Bölüm 2 — Intel 82540EM

Kök Q35 yuvasında gerçek INTx ve iki PCI köprüsünün ardında yoklama kullanıldı. Her iki derleyicide her iki yol MAC, donanim raporu, iki yönlü çerçeve, bağlantı kapatıp açma ve pcap karşılaştırmasını geçti. Yeni kapı ile §7’deki yedi x86 ve dört saf sürücü kapısının toplu süresi 1604 saniye. Ayrı imajlarda EOP kaldırılınca eş TX çerçevesini alamadı. RDT ilerletilmeyince sekizinci RX çerçevesi gelmedi. İki negatif koşum da çıkış 1 verdi.

İlk toplu çalışmada hata veren kapılar özgün eşiklerle ayrı koşulup geçti.
- calistir_bm_x86_program15

## Bölüm 3 — PCI ağ matrisi

82540EM, 82574L, 82576 ve modern VirtIO PCI aktarımı iki derleyici ve ikişer açılışta pcap ile doğrulandı. 82574L için ikinci açılış 4 GiB RAM ile yüksek fiziksel adres kullandı. Ayrıca iki RTL8139 açılışı destek durumunu doğruladı. NIC matrisi, PC matrisi ve §7’de istenen regresyonlar geçti. Son regresyon koşusu 1303 saniye sürdü. 82574L'de RDBAH yazımı atlanınca taban geri okuması başarısız oldu. 82576'da alıcı kuyruğunu etkinleştirmeyince ilk çerçeve alınamadı. Her iki değiştirilmiş çekirdekte çıkış 1.
