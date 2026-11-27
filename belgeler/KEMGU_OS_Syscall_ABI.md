# KEMGU-OS kullanıcı ABI'si v2

x86_64'te `int 0x80` girişinde RAX çağrı numarasıdır. İlk üç argüman sırasıyla RDI, RDX ve RSI'dedir. Sonuç işaretli 64 bit olarak RAX'tadır. Diğer genel amaçlı yazmaçlar ve RFLAGS korunur. İkinci argümanın RDX'te kalması PVH ve mevcut kullanıcı koduyla uyumluluğu korur. v2 kullanıcı programları genel amaçlı yazmaçlarla derlenir. Kayan nokta ve SIMD komutları süreç kipinde #NM üretir.

| No | Çağrı | Argüman sayısı | Argümanlar | Dönüş | Mimari |
| --- | --- | --- | --- | --- | --- |
| 5 | eski_yaz | 1 | NUL sonlu kullanıcı metni | 0 veya eski hata | x86_64, arm64 |
| 7 | eski_satir | 1 | Geri döndürülecek değer | Argüman | arm64 |
| 10 | tik | 0 | | Zamanlayıcı tik sayısı | x86_64, arm64 |
| 11 | pid | 0 | | Geçerli görev kimliği | x86_64, arm64 |
| 12 | eski_spawn | 1 | EL0 giriş adresi | Görev kimliği | arm64 |
| 13 | cik | 1 | Çıkış kodu, eski görev kipinde yok sayılır | C1 süreç kipinde dönmez | x86_64, arm64 |
| 14 | durum | 1 | Görev kimliği | 0 canlı, 1 bitmiş veya yok | x86_64, arm64 |
| 17 | eski_dosya_yaz | 2 | NUL sonlu ad ve içerik | Yazılan bayt veya eski hata | x86_64, arm64 |
| 18 | eski_dosya_oku | 2 | NUL sonlu ad ve 512 baytlık tampon | Okunan bayt veya eski hata | x86_64, arm64 |
| 24 | eski_ag_yolla | 2 | Ethernet çerçevesi ve bayt sayısı | Yollanan bayt veya eski hata | arm64 C |
| 25 | eski_ag_oku | 2 | Tampon, bayt kapasitesi | Alınan bayt veya eski hata | arm64 C |
| 26 | eski_satir_oku | 2 | Tampon, bayt kapasitesi | Okunan bayt veya eski hata | x86_64, arm64 |
| 27 | eski_saat | 0 | | RTC Unix saniyesi veya eski hata | arm64 C |
| 40 | abi_surumu | 0 | | 2 | x86_64 |
| 41 | ac | 2 | Yol, kip | Dosya tanımlayıcısı | x86_64 |
| 42 | kapat | 1 | fd | 0 | x86_64 |
| 43 | oku | 3 | fd, tampon, bayt kapasitesi | Okunan bayt, 0 dosya sonu | x86_64 |
| 44 | yaz | 3 | fd, tampon, bayt sayısı | Yazılan bayt | x86_64 |
| 45 | konumla | 3 | fd, işaretli ofset, kaynak 0 baş / 1 konum / 2 son | Yeni konum | x86_64 |
| 46 | bilgi | 2 | Yol, 32 baytlık Bilgi tamponu | 0 | x86_64 |
| 47 | dizin_oku | 3 | Dizin fd'si, tampon, bayt kapasitesi | Yazılan bayt, 0 dizin sonu | x86_64 |
| 48 | olustur_dizin | 1 | Yol | 0 | x86_64 |
| 49 | sil | 1 | Dosya yolu | 0 | x86_64 |
| 50 | sil_dizin | 1 | Boş dizin yolu | 0 | x86_64 |
| 51 | yeniden_adlandir | 2 | Eski ve yeni dosya yolu | 0 | x86_64 |
| 52 | calisma_dizini_al | 2 | Tampon, bayt kapasitesi | NUL hariç bayt sayısı | x86_64 |
| 53 | calisma_dizini_degistir | 1 | Dizin yolu | 0 | x86_64 |
| 54 | esitle | 0 | | 0 | x86_64 |
| 55 | tus_bekle | 1 | 32 baytlık TuşOlayı tamponu | 0 | x86_64 |
| 56 | saat | 1 | 48 baytlık Saat tamponu | 0 | x86_64 |
| 57 | uyu | 1 | Milisaniye (0–60.000) | 0 | x86_64 |
| 58 | baslat | 2 | Program yolu, NUL işaretçiyle biten en çok 31 ek argüman veya 0 | Çocuk PID | x86_64 |
| 59 | bekle | 2 | Çocuk PID, 16 baytlık sonuç | 0 | x86_64 |
| 60 | surec_listesi | 2 | Tampon, kayıt kapasitesi (0–8) | Kayıt sayısı | x86_64 |
| 61 | ekran_bilgi | 1 | p | 0 | x86_64 |
| 62 | ekran_yaz | 3 | sat,sut,p | 0 | x86_64 |
| 63 | ekran_temizle | 1 | renk | 0 | x86_64 |
| 64 | imlec | 3 | sat,sut,gorunur | 0 | x86_64 |
| 65 | fare_bekle | 1 | 48 baytlık fare tamponu | 0 | x86_64 |
| 66 | olay_bekle | 3 | 56 baytlık olay tamponu, maske, ms | 0 | x86_64 |
| 67 | onplan_baslat | 2 | Program yolu ve baslat ile aynı argv | Çocuk PID, -13 ön plan gerekli | x86_64 |
| 68 | piksel_satiri | 3 | Piksel y, u32 RGB dizi adresi, piksel sayısı | 0, -22 sınır, -14 adres, -13 ön plan | x86_64 |
| 69 | boru | 1 | Yazılabilir iki i64 adresi (okuma fd, yazma fd) | 0, -23 boru havuzu dolu, -24 fd yok | x86_64 |
| 70 | baslat_eslemeli | 3 | Program yolu, argv adresi, iki i64 (giriş ve çıkış boru veya soket fd, -1 varsayılan) | Çocuk PID veya negatif hata | x86_64 |
| 71 | kopyala | 2 | Kaynak ve hedef UTF-8 yol adresleri | 0 veya negatif hata | x86_64 |
| 72 | tasi | 2 | Kaynak ve hedef UTF-8 yol adresleri | Hedef yazıldıktan sonra kaynağı siler, 0 veya negatif hata | x86_64 |
| 73 | ping | 3 | IPv4 adresi, sıra, zaman aşımı (ms) | İlk ping ELF sürümünün uyumluluk çağrısı. Başarıda RTT (µs), zaman aşımında -110. | x86_64 |
| 74 | soket | 1 | tur | 1 TCP, 2 UDP, 3 ICMP. Yeni süreç fd değeri | x86_64 |
| 75 | bagla | 3 | fd, ip, port | Uzak IPv4 ve port. Bloksuz bağlantıda -11 | x86_64 |
| 76 | baglan_dinle | 3 | fd, port, kuyruk | TCP için kuyruk 1–8, UDP için yerel porta bağlar | x86_64 |
| 77 | kabul | 1 | fd | Kabul edilen bağlantının fd değeri | x86_64 |
| 78 | gonder | 3 | fd, adres, boy | Gönderim halkasına alınan bayt sayısı | x86_64 |
| 79 | al | 3 | fd, adres, boy | Alınan bayt veya akış sonunda sıfır | x86_64 |
| 80 | udp_gonder_hedef | 3 | fd, io, uc | io iki i64 adres ve boy, uç iki i64 IPv4 ve port | x86_64 |
| 81 | udp_al_kaynak | 3 | fd, io, uc | Bir datagram alınır, yazılabilir uca kaynak döner | x86_64 |
| 82 | dns_coz | 2 | ad, out | NUL ile biten ad, 8 baytlık IPv4 çıktısı | x86_64 |
| 83 | ag_bilgi | 2 | out, boy | 96 baytlık ağ bilgisi kaydının dolu uzunluğu | x86_64 |
| 84 | soket_secenek | 3 | fd, ad, deger | 1 bloksuz, 2 Nagle, 3 keepalive ms, 4 alım zaman aşımı ms | x86_64 |
| 85 | bekle_coklu | 3 | liste, adet, ms | 24 baytlık kayıtlar. Hazır kayıt sayısı veya sürede sıfır | x86_64 |
| 86 | icmp_yanki | 3 | fd, ip, istek | ICMP soketi. İki i64 sıra ve süre (ms). RTT mikrosaniye | x86_64 |
| 87 | yazmayi_kapat | 1 | fd | TCP yazma yarısını sonlandırır, okuma devam eder | x86_64 |
| 88 | ag_kayit | 3 | tür, indeks, 64 bayt çıktı | 1 ARP, 2 TCP (dinleyiciler dahil), 3 arayüz, 4 DHCP süreleri. Dolu 1, boş 0 | x86_64 |
| 89 | dosya_akis | 3 | fd, işlem, üst sınır | 1 dosyanın okumalık kopyasını açar. 2 yazılacak içeriği fiziksel çerçevelerde biriktirir. 3 ve sıfır sınır dosyayı yayımlar. Yayımlamadan kapatma veya süreç çıkışı birikimi bırakır. Üst sınır 16 MiB. | x86_64 |
| 90 | paket_hedef | 2 | Çıktı adresi ve tampon boyu | Yazılabilir veri bölümünü seçer, yolunu sıfır sonlandırmalı yazar. Paket işlem kilidi süreç çıkışında bırakılır. Meşgulken -16, uygun bölüm yoksa -30 döner. | x86_64 |
| 91 | islemci_sayisi | 0 | — | Çevrimiçi mantıksal işlemci sayısı. | x86_64 |
| 92 | islemci_no | 0 | — | Çağrının kullanıcı kipinden geldiği anda işlemcinin sıfır tabanlı sıra numarası. | x86_64 |
| 93 | yakinlik_ayarla | 1 | İşlemci numarası | Çağıran sürecin sonraki dilimini bu işlemciye bağlar. −1 bağı kaldırır. Başarıda 0, aralık dışı numarada −22. | x86_64 |
| 94 | islemci_kayit | 3 | İşlemci, tampon, boy | 64 bayt yazar ve 0 döner. Boy 64'ten küçükse veya işlemci geçersizse −22, aralık yazılamıyorsa −14. | x86_64 |

40–119 v2 x86_64 çağrılarına ayrılmıştır. Bilinmeyen kullanıcı çağrıları `NOSYS` döndürür. PVH ring0 kapısı eski davranışını korur. 40–79 dışında bilinmeyen çağrının sonucu `-1` olur. Eski minifs çağrıları PVH servisidir. UEFI C1 süreçlerine minifs bağlanmadığında 17 ve 18 `NOSYS` döndürür. VFS çağrıları 41–54 FAT32 bağlamalarını kullanır.

| Değer | Ad | Anlamı |
| --- | --- | --- |
| -7 | E2BIG | Argüman sayısı veya toplam uzunluğu sınırı aştı |
| -10 | ECHILD | Beklenebilecek çocuk bulunamadı |
| -11 | EAGAIN | Süreç tablosu dolu |
| -12 | ENOMEM | Süreç için fiziksel çerçeve bulunamadı |
| -13 | EACCES | Girdi ön plana ait |
| -2 | NOENT | Yol bulunamadı |
| -5 | IO | Aygıt, FAT32 veya RTC hatası |
| -9 | BADF | Tanımlayıcı kapalı, aralık dışı veya işlem için geçersiz |
| -14 | FAULT | Tampon aralığı veya sayfa izni geçersiz |
| -17 | EXIST | Hedef dosya veya dizin var |
| -18 | XDEV | Adlandırma kaynakla aynı bağlamada olmalı |
| -20 | NOTDIR | Dizin gerekli |
| -21 | ISDIR | Dosya gerekli |
| -22 | INVAL | Kip, boyut, konum veya süre geçersiz |
| -24 | MFILE | Sürecin fd tablosu dolu |
| -27 | FBIG | Yazılacak dosya FAT32 yazıcı sınırını aşıyor |
| -28 | NOSPC | Diskte uygun küme yok |
| -30 | ROFS | ESP veya salt okunur bağlı bölüm |
| -34 | RANGE | Dizin girdisi veya çalışma dizini tampona sığmadı |
| -36 | NAMETOOLONG | Yol veya bileşen sınırı aşıldı |
| -38 | NOSYS | Çağrı numarası tanımlı değil veya eski servis bağlanmamış |
| -39 | NOTEMPTY | Dizin boş değil |

## Adres alanı ve giriş

- Yeni ET_EXEC görüntüleri `0x54200000` adresinde başlar. Kod ve veri için 4 MiB bulunur. Heap `[0x54600000, 0x54e00000)`, eşlenmemiş alan `[0x54e00000, 0x54f00000)`, yığın `[0x54f00000, 0x55000000)` aralığındadır. Yığın sonundaki sayfa argc/argv taşır. Giriş RSP değeri argc adresinin sekiz bayt altındadır.
- Dalga 13 ET_EXEC dosyaları özgün 2 MiB penceresini, 0x54100000 heap başlangıcını, 0x541ef000 koruma sayfasını ve 0x541ff000 argc adresini kullanır. Segment izinleri R-X, R+NX veya RW+NX olabilir.
- Yükleyici 64 baytlık ELF başlığını ve en çok 256 girdilik program tablosunu ayrı okur. Her PT_LOAD segmenti en çok 16 KiB dosya verisi taşıyan parçalarla kendi fiziksel çerçevelerine yazılır. BSS ilk tahsiste sıfırlanır. Dosyada PT_LOAD dışında kalan içerik yüklenmez.
- Argüman sayısını argc adet u64 adres ve bir sıfır işaretçi izler. Aynı sayfanın kalanında NUL sonlu UTF-8 metinler bulunur. Argv[0] program yoludur. En çok 32 argüman desteklenir.
- İşaretçi ve uzunluk önce taşmasız pencere aralığına, ardından aralıktaki her kullanıcı sayfasına göre denetlenir. Çekirdek çıktı tamponlarında RW biti gerekir. Yol taraması en çok 4096 bayt ve bileşen başına 255 bayttır. Böylece eşlenmemiş yığın koruma sayfası da geçersiz bir çağrı tamponudur.
- Yeni düzende 3594, eski düzende 516 ardışık UEFI çerçevesi her süreç başlarken ayrılır. Çıkışta kullanıcı sayfa tabloları kaldırılır, çerçeveler aynı UEFI ayırıcısına döner. Bir sonraki süreç alanını kullanmadan önce sıfırlar.
- Arka plan stdout/stderr yazıları süreç başına tek bir 4096 baytlık satır tamponunu paylaşır. Satır sonu geldiğinde çıktı birlikte yazılır. Sınırı aşacak çağrı RANGE döndürür. Çıkışta son eksik satır da yazılır.
- Kabuk PID 0 kullanır. En çok sekiz kayıt hazır, çalışıyor, bekliyor veya çıkış sonucu bekleniyor durumunda tutulur. Her LAPIC kesmesinde kullanıcı yazmaçları saklanır. Hazır süreçler döngüsel seçilir. Süreçlerin fd tabloları ve çalışma dizinleri ayrıdır.
- `baslat` yolu argv[0] yapar. `bekle` kendi çocuğunun iki u64 alanını döndürür. Tür 0 için ikinci alan çıkış kodu, tür 1 için sonlandırma nedenidir. Ön plan süreci beklerken çocuk girdiyi devralır. Kayıt bekleme tamamlanınca yeniden kullanılabilir.
- `surec_listesi` her süreç için 96 bayt verir. İlk üç u64 alanı PID, durum ve kullanıcı kipinde tamamlanan 10 ms dilim sayısıdır. Kalan 72 baytta NUL sonlu UTF-8 program adı bulunur. Durumlar hazır (1), çalışıyor (2), bitti (3), sonlandırıldı (4), uyuyor (5), çocuk bekliyor (6), girdi bekliyor (7). Kapasite yetmezse RANGE döner.


## Dosya ve girdi biçimleri

- `ac` kipinde 0 salt okuma, bit 0 okuma/yazma, bit 1 oluşturma, bit 2 her yazıyı sona ekleme, bit 3 açarken kesme, bit 4 dizin açmadır. Oluşturma, sona ekleme ve kesme yazma kipini gerektirir. Dizin kipi tek başına kullanılır.
- 0/1/2 konsola ayrılmıştır. 1/2 ile `yaz` seri ve ekran akışına gider. Klavye olayları `tus_bekle` ile alınır. 3–15 süreç fd'leridir. Bir çağrıda en çok 16 KiB okunur, 4 KiB yazılır. FAT32 değiştirme planı dosya başına 64 KiB ile sınırlıdır. Daha büyük dosyalar okunabilir.
- Bilgi yapısı dört little-endian u64 alandır: bayt boyutu, dizin bayrağı (0/1), FAT değiştirme tarihi, FAT değiştirme saati. Dizin okuma, UTF-8 ad ve satır sonundan oluşan bütün girdiler döndürür. Dizinin adını `/` izler. Bir girdi sığmazsa bölünmez. Yol tabanlı yeniden adlandırma aynı bağlamada dosyayı kopyalayıp kaynağı kaldırır. Kaldırma başarısızlığında iki dosya korunabilir.
- Çalışma dizini tamponu NUL sonlandırılır. Süreç kabuğun çalışma dizinini devralır ve sonraki değişiklikler süreçte kalır. Yazılabilirlik ve ESP denetimi kabukla ortaktır.
- TuşOlayı dört little-endian u64 alandır: set 1 tuş kodu (E0 için bit 8), Unicode kod noktası (yoksa 0), basma (1) veya bırakma (0), değiştiriciler. Değiştiriciler shift=1, ctrl=2, alt=4, altgr=8, caps=16. Ön plandaki süreç PS/2 IRQ halkasından veya USB HID'den giriş alır. Onu başlatan Enter tuşunun bırakması sürece iletilmez. Çıkıştan sonra giriş yeniden kabuğa gider.
- Saat altı little-endian u64 alandır: yıl, ay, gün, saat, dakika, saniye. `uyu` çağrısı UEFI LAPIC tiklerine göre 10 ms'ye yukarı yuvarlanır. `tik` sistem açılışından beri tik sayısını verir.

Ön plan ekranı sol üst hücreden (0,0) numaralandırılır. Bilgi dört u64 döndürür (sütun, satır, piksel genişliği, piksel yüksekliği). Ekran yazısının üçüncü argümanı NUL sonlu UTF-8 adresi, ön renk ve arka renkten oluşan üç u64 alanlı yapıdır. Renkler 0xRRGGBB biçimindedir. Bir satıra sığmayan metin RANGE, ekranı olmayan bir sistem ENODEV (-19) döndürür.

Fare kaydında altı u64 bulunur (işaretli dx, işaretli dy, x, y, düğmeler, işaretli tekerlek). Ekranda sağa ve aşağı doğru artış pozitiftir. Tekerlek aşağı çevrilince +1, yukarı çevrilince −1 gelir. PS/2 kimlik 3 dört bayt, kimlik 0 üç bayt gönderir. HID raporunun işareti aynı yöne çevrilir. Kabukta `hassasiyet 1..4` hareketi ölçekler. Varsayılan 1 doğrusal, 2–4 düzeylerinde mutlak değeri 8 pikselden büyük eksen bir ek katsayı alır. Basılı yazı ve gezinme tuşları 500 ms sonra 50 ms arayla yinelenir. Sol/sağ/orta düğmeler bit 0/1/2 kullanır. Birleşik olayda ilk u64 türdür (0 zaman aşımı, 1 tuş, 2 fare). Kalan 48 baytta ilgili olay yer alır. Maske 1 tuş, 2 fare, 3 ikisi. Süre -1 sınırsız, 0 hazır olayı sorgular, 1–60.000 ms LAPIC saatine yukarı yuvarlanır.

Ön plan süreci bir etkileşimli çocuk açarken 67 ile ekran ve girdi sahipliğini çocuk çalışmaya hazır olmadan devreder. 59 ile çocuğu topladığında sahipliği geri alır.

### Kabuktan program bulma

Bir yol ayıracı içeren ad çalışma dizinine göre çözülür. Yalın adlar için kabuk gerekirse `.elf` uzantısını ekler. Önce `/bin` içinde, ardından açılış bölümüyle aynı fiziksel aygıtın diğer bağlı bölümlerindeki `/bin` dizinlerinde arar. İlk eşleşme çalışır. `sonuc` en son toplanmış sürecin çıkış kodunu verir. Komut geçmişi 32 satırdır. Tab, imlece kadar yazılan yolun ortak önekini tamamlar ve birden çok eşleşme varsa adayları listeler.

QEMU xHCI üzerinde USB HID tamamlamaları MSI-X vektör 65'i kullanır. ISR cihazın kesmesini ve LAPIC EOI'yi onaylar. Bekleyen klavye ve fare aktarımları TRB adresi, yuva ve uç ile eşlenir. Bekleyen kullanıcı süreci fiziksel CPU'yu bırakır. PS/2 için IRQ1 ve IRQ12 halkaları aynı olay biçimini besler.

Boru havuzu sekiz adet 4096 baytlık halka içerir. Yazma en fazla 4096 baytlık çağrıyı bütünüyle kabul eder veya yer açılana kadar süreci uyutur. Eksik okuma geçerlidir. Son yazar kapandığında kalan baytlar okunur ve ardından EOF gelir. Okur kalmadığında yazma -32 döner. Eşlemeli başlatma uçlara çocuğun ayrı referanslarını ekler. Ebeveyn kendi uçlarını kapatır.

71 ve 72 numaralı çağrılar dosya aktarımı için kabukla aynı VFS yolunu kullanır. Her yol NUL sonlandırmalı UTF-8'dir. Kaynak belleği okunabilir olmalıdır. Hedef salt okunursa EROFS, yazma sınırı aşılırsa EFBIG veya ENOSPC döner. 51 numaralı yeniden_adlandir farklı bağlama noktaları arasında EXDEV döndürür. 72 numaralı tasi hedefi yazdıktan sonra kaynağı kaldırır. Hedef yazılmışken kaynağın silinmesi başarısız olursa hata döner ve iki kopya da kalabilir.

## Soketlerin bellek düzeni ve bekleme

Bir süreçte fd 0–15 aralığındadır. Yeni soket en küçük boş fd’yi 3’ten başlayarak kullanır. 74 numaralı çağrı TCP, UDP veya ICMP uç noktası açar. 42 numaralı kapatma çağrısı bütün fd türlerinde ortaktır. Bir soket 70 ile çocuğun standart girdisine, çıktısına veya ikisine birden eşlenebilir. Son sahip kapattığında ya da süreç sonlandığında bağlı TCP akışı FIN gönderir. 87 ile yalnızca yazma yönü kapatılabilir.

IPv4 adresi ağdaki dört sekizlinin sayısal değeridir (örneğin 192.0.2.1 için 3221225985). Adres, port, işaretçi, uzunluk ve süre alanları kullanıcı belleğinde x86_64 yerel düzeninde sekizer bayttır.

| Çağrı | Tampon |
|---|---|
| 80, 81 | `io` iki alandır (bayt tamponu adresi, tampon boyu). `uc` iki alandır (IPv4, port). 81 kaynak adresini ve portunu yazar. Bir okumada bir datagram tüketilir. |
| 83 | Sırasıyla MAC, IPv4, önek, geçit, DNS, NTP, DHCP bitiş anı (monoton ms), RX çerçeve, TX çerçeve, RX bayt, TX bayt, TCP yeniden iletim. Tampon 96 bayttır. |
| 85 | Her kayıt fd, istenen bitler, dönen bitler alanlarından oluşur. Okuma 1, yazma 2, hata 4. Her çağrıda dönen alan yeniden yazılır. Sonuç hazır kayıt sayısıdır. Süre sıfırsa hemen döner. |
| 86 | İstek tamponu iki alandır (ICMP sıra numarası, zaman aşımı ms). Sonuç RTT mikrosaniye veya negatif hatadır. |

Bloksuz okumada veri yoksa `-11`, dolu fd tablosunda `-24`, sınırlı soket veya TCP havuzu doluysa `-105`, kullanımda olan dinleme portunda `-98`, zaman aşımında `-110` döner. TCP’de okumanın sıfır dönmesi veri sonudur. Dinleme kuyruğu 1–8 aralığında seçilir. UDP’de her sokette dört datagramlık kuyruk vardır. Alım süresi ve keepalive milisaniye cinsindendir.


### İşlemci kaydı

64 baytlık kayıtta sekiz küçük uçlu 64 bit alan bulunur. Sırasıyla işlemci numarası,
APIC kimliği, LAPIC tıkı, bu işlemcide çalıştırılan farklı PID sayısı, açılıştan beri boşta
tık yüzdesi, başka kuyruktan çalınan dilim sayısı, çalışan PID ve hazır süreç
sayısıdır. PID sıfırsa o anda kullanıcı süreci yoktur. Bir süreç başka bir işlemcide
devam edebileceğinden iki bağımsız çağrı arasındaki işlemci numarası değişebilir.
95–99 sonraki genişletmelere ayrılmıştır.
