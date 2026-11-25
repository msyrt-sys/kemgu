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

## Bölüm 4 — ARP, IPv4, ICMP, loopback ve ping

İki derleyicide dgram üzerinden konuğa 100 ping, konuktan eşe 20 ping, ARP yaşlanmasından sonra bir ping, dış trafiği olmayan üç loopback ping ve slirp ağ geçidine iki ping doğrulandı. Kullanıcı ELF'inin min/ortalama/maksimum RTT'si çıktılardan bağımsız hesaplandı. Çıkış IPv4 sağlamasının taşıma katlamasını atlamak, ARP yanıtında yanlış MAC yazmak ve loopback paketini dış arayüze göndermek üç ayrı çekirdekte ilgili kapıları kırdı. Regresyon koşusu 1823 saniye sürdü.

İlk toplu çalışmada hata veren kapılar özgün eşiklerle ayrı koşulup geçti.
- calistir_bm_x86_boru
- calistir_bm_x86_program15

## Bölüm 5 — UDP, DHCP ve DNS

DHCP DORA, T1 yenilemesi, T2 yeniden bağlaması, bağlantı kesikken kira bitişi ve yeniden adres alma, DNS CNAME ve sıkıştırma, ilk DNS sunucusu susunca ikinciye geçme ve TTL önbelleği gerçek dgram trafiğinde sınandı. Slirp 10.0.2.15 adresini verdi. Her derleyicide 1000 UDP datagramının tamamı yankılandı ve pcap içeriği bağımsız okundu. Bölüm ve ara regresyon kapıları 1350 saniyede tamamlandı. T1'i kira sonuna öteleyen çekirdek, telde yenileme görülmeyince testi kırdı. DNS işaretçi döngüsü korumasını kaldırıp konuğu asma sabotajı uygulanmadı.
Ayrı koşumda geçen kapı: calistir_bm_x86_nic_matris

## Bölüm 6 — IPv4 parçaları

8 KiB UDP aktarımı iki derleyici ve gerçek QEMU ile geçti. Sırasız ve çift parçalar birleştirildi. Bir parça eksikken süre doldu, sonraki datagram tam döndü. Bölüm ve §7 kapılarının doğrulama süresi 1268 saniyeydi. İlk toplu koşunun bir imajı sayaç çıktısına yeni satır eklenmeden derlenmişti. Son kaynaklarla iki derleyicide de bağımsız bölüm kapısı geçti. Ofseti sekizle çarpmayan ayrı çekirdek için 8 KiB yankı gelmedi. Yuva zaman aşımını kapatıp ağdan yuvaları tüketme sabotajı uygulanmadı.

## Bölüm 7 — TCP temeli

Etkin ve pasif açılış, iki yönde 64 KiB, sekiz eşzamanlı bağlantı ve dokuzuncu isteğin reddi, kapalı port, RST, FIN, sıfır pencere yoklaması ve loopback doğrulandı. QEMU kâhini iki derleyicide geçti. Geçen süre 1629 saniye. Sarmaya dayanıksız karşılaştırma ve FIN sıra tüketiminin kaldırılması, iki ayrı host imajında C ve self-host durum testiyle sırasıyla çıkış 4 ve 7 verdi. Değiştirilmiş çekirdekle QEMU üzerinde aktarımı ya da kapanışı asma sabotajı uygulanmadı.

## Bölüm 8 — TCP kaybı ve yeniden iletim

İki derleyici ve QEMU ile iki yönde 4 MiB, sabit tohumlu %5 kayıp, %5 sıra değişimi ve %2 çiftlemede SHA-256 eşleşti. Çekirdek ve tel yeniden iletim sayıları ölçüldü. Geçen süre 1618 saniye. Üç ayrı değiştirilmiş saf durum makinesi gerçek C ve self-host çalıştırmalarında kırıldı. Sıfır ve çok uzun RTO zamanlayıcı kapısında çıkış 2, yinelenen veriyi ekleme alıcı sırasında çıkış 9 verdi. Değiştirilmiş çekirdekte paket taşması veya bağlantı asılması oluşturan ağ sabotajları ve bu değişikliklerle QEMU SHA-256 sabotajı uygulanmadı.

## Bölüm 9 — TCP seçenekleri ve verim

Pencere ölçeği iki yönde, TCP zaman damgası ve PAWS, gecikmeli ACK, kapatılabilir Nagle ve keepalive eklendi. 64 MiB tek yön aktarım ölçek açık ve kapalı olarak iki derleyicide ölçüldü. Geçen süre 1707 saniye. Alıcı tarafında ölçek kaydırması kaldırılan ayrı QEMU imajında 131.068 yerine 262.140 baytlık pencere gözlendi ve kapı kırıldı. Gecikmeli ACK zamanlayıcısının çıkarıldığı iki host ikilisinde tek segment denetimi çıkış 5 verdi. Değiştirilmiş çekirdekte ağ üzerinden kapanmayan oturum oluşturma sabotajı uygulanmadı.

## Bölüm 10 — kullanıcı soketleri ve çoklu bekleme

74–87 çağrıları ve 73 ile uyumlu kullanıcı ping eklendi. C ve self-host konuklarında üç ayrı süreçte üç bağlantı, doğal çıkış ve Ctrl+C ile üç FIN, iki UDP soketi arasında QMP tuşu, boru, kabul ve çocuğa aynı soketin stdin/stdout olarak devri doğrulandı. Süre 2536 saniye. Onuncu bölümün ara regresyonu 22 hedef içerdi. İlk koşumda ring0 çağrı 99 ve self-host IR tipi düzeltme gerektirdi. Boruda okunabilir bit kaldırılınca bekleme üst sınırında konuk 15, kâhin 1 döndürdü. Süreç sonlandırmada soketi açık bırakma sabotajı uygulanmadı.
- calistir_bm_x86_tcp_temel ilk sonuç kırmızı, ayrı koşum çıkışı 0.

## Bölüm 11 — kullanıcı ağ araçları

Ağ kitaplığı, `ag`, `cozumle`, `nc` ve soketli `ping` derlendi. C ve self-host konuklarında iki yönlü 1 MiB boru aktarımı, UDP yankısı, gerçek FAT32 yönlendirmesi ve loopback geçti. DHCP, DNS, ARP, fiziksel MAC, dinleyici ve bağlantı durumu ve 200 ms bağlantı zaman aşımı ile sekiz datagramlık sayaç farkı bağımsız eşle karşılaştırıldı. Geçen süre 2057 saniye. FIN çağrısının çıkarıldığı gerçek C kullanıcı ELF’inde eş bütün 1 MiB veriyi aldıktan sonra üç saniyelik sınırda eksik FIN’i bildirdi, kâhin çıkış 1 verdi.

## Bölüm 12 — HTTP istemcisi ve sunucusu

`indir` ve `sunucu`, sınırlı HTTP ayrıştırıcısı ve dosya akışı ABI 89 eklendi. İki derleyiciyle gerçek QEMU, 8 MiB parçalı yanıt ve yönlendirmeler, aynı TCP oturumunda ikinci dosya, pcap ve FAT32 SHA-256, sekiz eşzamanlı istemci, engellenen yollar, Ctrl+C, üç fiziksel disk sürücüsü, /ram ve fsck geçti. Beş yönlendirme sonrasında altıncı geçiş reddedildi. Geçen süre 1585 saniye. Son sıfır parçayı tanımayan kullanıcı ELF’i, gerçek gövdeden sonra geçerli treyleri okuyunca -74 verdi. Kâhin çıkışı 1 oldu. Yüzde kodlu üst dizin korumasını kaldırma ve sınırsız yönlendirme döngüsü sabotajları uygulanmadı.

## Bölüm 13 — SNTP ile RTC eşitleme

Açılışta DHCP seçenek 42, elle IP, dört zaman damgasıyla ofset/gecikme, BCD/ikili ve 12/24 saat CMOS yazımı eklendi. QMP ve pcap ölçümleri iki derleyicide geçti. Gerçek kullanım programı `baslat saat metin` eşitlenmiş saati gösterdi. Süre 1304 saniye. 1900/1970 dönüşümü çıkarılınca 2024 tarihine karşın QMP 2094 gösterdi. BCD tarih alanları ikili yazılınca 2031 yerine 2025 ölçüldü. İki kâhin de çıkış 1 verdi.

## Bölüm 14 — TFTP ve teşhis aktarımı

Süre 1394 saniye. C ve self-host QEMU’da ayrı ayrı iki yönde 2 MiB, yön başına 205 ilk blok kaybı ve indirmede 41 yinelenen blok ölçüldü. FAT32, eş kaydı ve iki yönün pcap SHA256 değerleri eşleşti. Teşhis alıcısı iki raporu yerel dosyaya yazdı. Donanım raporu FAT32 ile, günlük QMP ile okunan çekirdek halkasıyla aynı. Bağlantı kapalıyken kabuk açık hata verdi. §7 kapıları ve yeni hedef çalıştırıldı. Yinelenen bloğu yeniden yazan değişiklik 16384 yerine 16896 bayt ve farklı SHA256 verdi. Kısa son bloğu tanımayan değişiklik -110 ile başarısız oldu. İki bağımsız koşumun çıkış kodu 1.
