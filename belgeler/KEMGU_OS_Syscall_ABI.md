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

40–79 v2 x86_64 çağrılarına ayrılmıştır. Bilinmeyen kullanıcı çağrıları `NOSYS` döndürür. Eski PVH ring0 çağrı kapısı ayrılmış aralığın dışındaki bilinmeyen numaralarda `-1` döndürür. Eski minifs çağrıları PVH servisidir. UEFI C1 süreçlerine minifs bağlanmadığında 17 ve 18 `NOSYS` döndürür. VFS çağrıları 41–54 FAT32 bağlamalarını kullanır.

| Değer | Ad | Anlamı |
| --- | --- | --- |
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

- Kullanıcı sanal penceresi `[0x54000000, 0x54200000)` ve sayfa boyutu 4 KiB. `_start` x86_64 ET_EXEC girişidir. ELF segmentleri `[0x54000000, 0x54100000)` içinde, ayrı sayfalarda R-X, R+NX veya RW+NX eşlenir. Dosya boyunun ötesindeki BSS baytları sıfırdır.
- `[0x54100000, 0x541ef000)` kullanıcı heap'i, `[0x541ef000, 0x541f0000)` eşlenmemiş koruma sayfası, `[0x541f0000, 0x54200000)` RW+NX yığındır.
- `RSP=0x541feff8` ile giriş yapılır. `0x541ff000` konumunda little-endian 64 bit argc, ardından argc kadar 64 bit kullanıcı adresi ve bir sıfır işaretçi bulunur. Buradan sonra aynı sayfada NUL sonlu UTF-8 argümanlar yer alır. Argv[0] komutta verilen program yoludur. 1–32 argüman desteklenir. Kabuk ASCII boşlukla ayırır.
- İşaretçi ve uzunluk önce taşmasız pencere aralığına, ardından aralıktaki her kullanıcı sayfasına göre denetlenir. Çekirdek çıktı tamponlarında RW biti gerekir. Yol taraması en çok 4096 bayt ve bileşen başına 255 bayttır. Böylece eşlenmemiş yığın koruma sayfası da geçersiz bir çağrı tamponudur.
- C1'de kabuk PID 0'dır ve süreçler bitene kadar sırayla çalışır. Süreç çıkışta açık fd'leri bırakır ve yazma eşitlemesi tamamlanır. Kullanıcı istisnası süreç sonucuna taşınır.

## Dosya ve girdi biçimleri

- `ac` kipinde 0 salt okuma, bit 0 okuma/yazma, bit 1 oluşturma, bit 2 her yazıyı sona ekleme, bit 3 açarken kesme, bit 4 dizin açmadır. Oluşturma, sona ekleme ve kesme yazma kipini gerektirir. Dizin kipi tek başına kullanılır.
- 0/1/2 konsola ayrılmıştır. 1/2 ile `yaz` seri ve ekran akışına gider. Klavye olayları `tus_bekle` ile alınır. 3–15 süreç fd'leridir. Bir çağrıda en çok 16 KiB okunur, 4 KiB yazılır. FAT32 değiştirme planı dosya başına 64 KiB ile sınırlıdır. Daha büyük dosyalar okunabilir.
- Bilgi yapısı dört little-endian u64 alandır: bayt boyutu, dizin bayrağı (0/1), FAT değiştirme tarihi, FAT değiştirme saati. Dizin okuma, UTF-8 ad ve satır sonundan oluşan bütün girdiler döndürür. Dizinin adını `/` izler. Bir girdi sığmazsa bölünmez. Yol tabanlı yeniden adlandırma aynı bağlamada dosyayı kopyalayıp kaynağı kaldırır. Kaldırma başarısızlığında iki dosya korunabilir.
- Çalışma dizini tamponu NUL sonlandırılır. Süreç kabuğun çalışma dizinini devralır ve sonraki değişiklikler süreçte kalır. Yazılabilirlik ve ESP denetimi kabukla ortaktır.
- TuşOlayı dört little-endian u64 alandır: set 1 tuş kodu (E0 için bit 8), Unicode kod noktası (yoksa 0), basma (1) veya bırakma (0), değiştiriciler. Değiştiriciler shift=1, ctrl=2, alt=4, altgr=8, caps=16. Ön plandaki süreç PS/2 IRQ halkasından veya USB HID'den giriş alır. Onu başlatan Enter tuşunun bırakması sürece iletilmez. Çıkıştan sonra giriş yeniden kabuğa gider.
- Saat altı little-endian u64 alandır: yıl, ay, gün, saat, dakika, saniye. `uyu` çağrısı UEFI LAPIC tiklerine göre 10 ms'ye yukarı yuvarlanır. `tik` sistem açılışından beri tik sayısını verir.
