Thu Nov  5 18:54:35 UTC 2026

VARSAYIM: Bu oturumda son kullanıcı isteği ve CODEX_GOREV.md kapsamı geçerlidir; önceki AGENTS kapsamı/dal adı yerine bu görevde açıkça izin verilen yollar ve codex/surucu-dalga1 kullanılır.
VARSAYIM: Bayt API'lerinde 0..255 dışı girişler yok sayılır ve yarım paket/önek temizlenir.
- [x] A1 · 62 test · sabotaj: `shift != d.caps` → `shift veya d.caps` → koşum kırmızı; geri alındı, köprü yeşil · 2026-11-05T18:56:07.334704+00:00
- [x] A2 · 19 test · sabotaj: `(d.b0 & 192) != 0` → `(d.b0 & 192) == 1` → koşum kırmızı; geri alındı, köprü yeşil · 2026-11-05T18:56:54.848469+00:00
VARSAYIM: CRC32 API sonuç tipi hata taşımadığından geçersiz ofset/uzunluk için 0 döner. GPT görüntüsü fiziksel diskin yalnız ön bölümü olabilir; bölüm LBA sınırları görüntü boyuyla kıyaslanmaz. C001 için nitelikli çağrı probe ile doğrulandı.
- [x] A3 · 21 test · sabotaj: `crc32::crc32(disk, bas olarak tam32, toplam olarak tam32) != oku_le32(disk, sektor + 88)` → `yanlış` → koşum kırmızı; geri alındı, köprü yeşil · 2026-11-05T19:02:19.882068+00:00
VARSAYIM: BPB sıfır FAT/ayrılmış sektör, veri alanı olmayan toplam ve veri aralığı dışı kök küme için Fat32Degil döndürür. Küme sayısı FAT32 alt eşiği A4 test fikstürleri ve B5 bellek tasarrufu için ayrıca zorlanmaz.
- [x] A4 · 34 test · sabotaj: `oku_le32(fat, (kume * 4) olarak tam32) & 268435455` → `oku_le32(fat, (kume * 4) olarak tam32)` → koşum kırmızı; geri alındı, köprü yeşil · 2026-11-05T19:04:45.188599+00:00
A5 sabotaj ilk turu yeşil kaldı: yalnız LFN parçaları arası sağlama farkı test ediliyordu. İki parçada aynı yanlış sağlama bulunan kalıcı olumsuz test eklendi; kısa ad sağlama kontrolü ayrıca korunuyor.
- [x] A5 · 22 test · sabotaj: `kisa_ad_saglama(b, ofs) == saglama` → `doğru` → koşum kırmızı; geri alındı, köprü yeşil · 2026-11-05T19:06:45.975437+00:00
- [x] A6 · 20 test · sabotaj: `bayt_toplam8(b, ofs, n olarak tam32) != 0` → `yanlış` → koşum kırmızı; geri alındı, köprü yeşil · 2026-11-05T19:08:22.077701+00:00
VARSAYIM: UTF-16 çözümünde geçersiz ofset/negatif birim boş dizi; kalan tek bayt U+FFFD; ilk NUL birim sonlandırıcıdır. Kodlayıcılar skaler olmayan kodları U+FFFD yapar. UTF-8 testleri karttaki metin API ile geçerli metinlerin bayt parçalarından bozuk diziler kurar.
- [x] B1 · 23 test · sabotaj: `kod >= enaz ve skaler_mi(kod)` → `skaler_mi(kod)` → koşum kırmızı; geri alındı, köprü yeşil · 2026-11-05T19:09:58.536859+00:00
- [x] B2 · 4 test · sabotaj: `ver utf8_kodla(utf16le_coz(b, 0, n));` → `ver "bozuk";` → koşum kırmızı; geri alındı, köprü yeşil · 2026-11-05T19:10:46.565487+00:00
- [x] B3 · 16 test · sabotaj: `uzunluk > 255` → `uzunluk > 256` → koşum kırmızı; geri alındı, köprü yeşil · 2026-11-05T19:11:25.895371+00:00
VARSAYIM B4: kisa_ad_uret(ad, mevcut) sonuç<metin,metin> döndürür; sonuç ve çakışma listesi FAT biçiminde boşluk dolgulu 11 ASCII bayttır. Zaten büyük harfli geçerli 8.3 ad dışında ~N üretilir. İç boşluk/nokta ve kontrol karakterleri de _ olur; yalnız . ve boşluklardan oluşan ad hatadır. Çakışma ASCII harf duyarsızdır; en çok ~999999 denenir.
- [x] B4 · 18 test · sabotaj: `değil var_mi(aday, mevcut)` → `doğru` → koşum kırmızı; geri alındı, köprü yeşil · 2026-11-05T19:12:55.553647+00:00
VARSAYIM B5: 512 bayt sektör, 32 ayrılmış sektör, iki FAT; FAT32 için en az 65525 veri kümesi. Seyrek imajın yalnız metadata+kök öneki döner, en çok 16 MiB bellek sınırı; BPB toplamı tam disk boyudur. Etiket en çok 11 ASCII bayt, büyük harfe çevrilir; boşsa NO NAME. Saf API saat almaması nedeniyle sabit KEMG seri no kullanılır.
- [x] B5 · 13 test · sabotaj: `yaz_le32(b, ofs + 8, 268435455)` → `yaz_le32(b, ofs + 8, 0)` → koşum kırmızı; geri alındı, köprü yeşil · 2026-11-05T19:15:17.009433+00:00
VARSAYIM B6: mevcut dosyanın üzerine yazma var hatasıdır; dizin_olustur sonuç<tam64,metin> içinde 0, dosya_yaz yazılan bayt sayısı döndürür. dosya_sil yalnız dosyaları siler. Dizin zinciri gerektiğinde büyür. Disk dolu/bozuk yol hatasında imaj değişmez; FSInfo iki kopyada güncellenir. Host yerleşik çakışması C009 nedeniyle testler ve sonraki modüller imaja_dosya_yaz/imajda_dizin_olustur/imajdan_dosya_sil ek API adlarını kullanır; istenen orijinal API korunmuştur.
- [x] B6 · 16 test · sabotaj: `iken f < b.fat_sayisi` → `iken f < 1` → koşum kırmızı; geri alındı, köprü yeşil · 2026-11-05T19:26:11.127630+00:00
VARSAYIM B7: dizin listeleme ham FAT girdilerinin . ve .. adlarını da içerir; kabuk bunları göstermek istemezse filtreler. İmaj dizisinin dışındaki mantıksal sektörler sıfırdır (seyrek sözleşme). C009 dolanması imajdan_dosya_oku/imaj_dizin_listele adlarıyla uygulanır; orijinal API sarmalayıcıları korunur.
- [x] B7 · 15 test · sabotaj: `dizi_ekle(sonuc, bayt olarak dtam8)` → `dizi_ekle(sonuc, (bayt ^ 1) olarak dtam8)` → koşum kırmızı; geri alındı, köprü yeşil · 2026-11-05T19:28:46.875800+00:00
B8 dış kahin: fsck.fat -n temiz (3 files, 3/68890 clusters). FSInfo boş küme sayısı 123 yapılınca fsck exit=1 ve Free cluster summary wrong; geri alındı. mkfs.fat --invariant ile ters yön boş kök/eksik dosya 3 testi geçti. ATLANDI: mcopy/mdir yok; Linux dosya ekleme, dış dosya içerik karşılaştırması ve mdir listesi. Loop mount denenmedi.
- [x] B8 · 1 test · sabotaj: `dizi_ekle(sonuc, bayt olarak dtam8)` → `dizi_ekle(sonuc, (bayt ^ 1) olarak dtam8)` → koşum kırmızı; geri alındı, köprü yeşil · 2026-11-05T19:31:26.247635+00:00
- [x] B9 · 13 test · sabotaj: `[3, 2, 1, 0, 5, 4, 7, 6` → `[0, 1, 2, 3, 5, 4, 7, 6` → koşum kırmızı; geri alındı, köprü yeşil · 2026-11-05T19:32:41.087300+00:00
- [x] C1 · 10 test · sabotaj: `adres = g.adres` → `adres = 0` → koşum kırmızı; geri alındı, köprü yeşil · 2026-11-05T19:34:33.802721+00:00
- [x] C2 · 9 test · sabotaj: `f.x_dsdt != 0` → `yanlış` → koşum kırmızı; geri alındı, köprü yeşil · 2026-11-05T19:35:50.462500+00:00
VARSAYIM C2: RESET_REG/RESET_VALUE dahil en az 129 bayt FADT kabul edilir; X_DSDT yalnız ≥148 baytta okunur. VARSAYIM C3: pci_coz kısa/bozuk/tip-0 olmayan veya satıcısı FFFF olan başlıkta hiç döner; sıfır BAR yer tutucu olarak korunur. bar_boyu 32 bit ve bar_boyu64 işaretsiz 64 bit maske APIleridir; sıfır maske boyutu 0 kabul edilir.
- [x] C3 · 16 test · sabotaj: `adres = adres | (oku_le32(b, 16 + (i + 1) * 4) << 32)` → `adres = adres` → koşum kırmızı; geri alındı, köprü yeşil · 2026-11-05T19:37:54.699918+00:00
VARSAYIM C4: E820 ham girdileri 20 bayt; mb2_coz tam tip-6 etiketini (16 bayt başlık, ≥24 bayt girdi, sürüm 0) alır. En çok 4096 bölge ve 0..tam64_max adres aralığı; taşma hatadır. Birden çok kullanılamaz türün örtüşmesinde en küçük tür numarası seçilir; her durumda tür 1 kaybeder. Sayfa/bayt toplamı normalleştirilmiş harita bekler. C005 nedeniyle olumsuz tam64 fikstürlerinde açık dönüşüm kullanıldı.
- [x] C4 · 14 test · sabotaj: `b.tur != 1 ve (tur == 1 veya b.tur < tur)` → `b.tur == 1 ve (tur == 1 veya b.tur < tur)` → koşum kırmızı; geri alındı, köprü yeşil · 2026-11-05T19:40:42.399442+00:00
VARSAYIM C5: bit eşlem yalnız kullanılabilir, hizalı ve ≥1MiB sayfalara indeks verir; fiziksel boşluklar bit tüketmez. En çok 1048576 sayfa (128KiB bitmap) desteklenir. ayrilmis_isaretle kısmen örtüşen sayfaları da kalıcı ayırır; serbest bunları reddeder.
- [x] C5 · 14 test · sabotaj: `eğer ilk < 256 { ilk = 256; }` → `eğer ilk < 0 { ilk = 256; }` → koşum kırmızı; geri alındı, köprü yeşil · 2026-11-05T19:43:05.024956+00:00
VARSAYIM C6: ET_EXEC/ET_DYN ve x86_64/AArch64 ELF64 LE kabul edilir; adresler tam64 pozitif aralıkla sınırlı. /bin/true ilk 4KiB fikstürüne orijinal dosya boyu kadar sıfır kuyruk eklendi; böylece başlıklar gerçek dosya sınırlarıyla doğrulanır, yük içeriği çalıştırılmaz. readelf -lhW: AArch64, giriş 6208, iki LOAD; tüm alanlar Python struct ile eşleşti.
- [x] C6 · 19 test · sabotaj: `(bayrak & 3) == 3` → `yanlış` → koşum kırmızı; geri alındı, köprü yeşil · 2026-11-05T19:45:30.207222+00:00
VARSAYIM C7: USTAR normal dosya/dizin tipleri desteklenir; diğer türler hata. Ad alanları UTF-8 olarak çözülür. Tek UTF-8 çözüm çekirdeğini korumak için B1 utf8_bayt_coz API eklendi; mevcut 23 test korundu, yasak başlangıç baytları ve aralık sınırları için 2 test eklendi. Python tarfile USTAR fikstürü Türkçe/emoji içerik, uzun önek ve boş dosya içerir.
- [x] C7 · 15 test · sabotaj: `saglama != beklenen` → `yanlış` → koşum kırmızı; geri alındı, köprü yeşil · 2026-11-05T19:48:06.609555+00:00
VARSAYIM D1: renk 0x00RRGGBB, X baytı sıfır. Çerçeve APIleri yapıyı değer olarak alır, paylaşılan veri dizisine yazar; ölçüler değişmez. Geçersiz çerçevede yazma sessizce yok sayılır, okuma hiç döner. 16MiB/16384 boyut sınırı; aynı veri dizisine örtüşen kopya ara tamponla korunur.
- [x] D1 · 10 test · sabotaj: `x >= c.genislik` → `x > c.genislik` → koşum kırmızı; geri alındı, köprü yeşil · 2026-11-05T19:51:27.484676+00:00
VARSAYIM D2: PSF Unicode dizileri (FFFE/FE sonrası çok kod noktalı ligatürler) doğrulanır fakat glif_indeksi tek kod noktası API olduğundan eşlemeye eklenmez. Unicode tablosu yoksa kod=glif indeksi. PSF1 mod&6 Unicode kabulü; PSF2 sürüm 0. Gerçek kahin Lat38-VGA8.psf.gz: ğ=207 ş=222 ı=214 İ=213 ö=148 ç=135 ü=129.
- [x] D2 · 15 test · sabotaj: `eğer değil dizi_mi { dizi_ekle(kodlar, k);` → `eğer doğru { dizi_ekle(kodlar, k);` → koşum kırmızı; geri alındı, köprü yeşil · 2026-11-05T19:54:55.712747+00:00
- [x] D3 · 7 test · sabotaj: `128 >> (sutun % 8)` → `1 << (sutun % 8)` → koşum kırmızı; geri alındı, köprü yeşil · 2026-11-05T19:56:51.595394+00:00
VARSAYIM D4: sekme sonraki 8 sütun durağına boşluk yazar; geri sil önceki satıra geçebilir. Sarma anlıktır; alt satır taşınca kaydırılır. Konsol satır metni sondaki boşlukları korur. Hücre başına ön/arka renk ayrı tutulur; kirli_satirlar tüketilerek temizlenir.
- [x] D4 · 14 test · sabotaj: `dizi_yaz(k.kirli, i, 0)` → `dizi_yaz(k.kirli, i, 1)` → koşum kırmızı; geri alındı, köprü yeşil · 2026-11-05T19:59:35.064462+00:00
VARSAYIM D5: CSI en çok 16 parametre ve 4096 sayısal değer tutar; bilinmeyen CSI/OSC/DCS yutulur. vt_bitir eksik UTF-8 baytlarını U+FFFD ile sonlandırır. Palet VGA 16 renktir.
- [x] D5 · 10 test · sabotaj: `dizi_boyut(d.utf8) == d.beklenen` → `dizi_boyut(d.utf8) >= 1` → koşum kırmızı; geri alındı, köprü yeşil · 2026-11-05T20:06:26.163048+00:00
VARSAYIM D6: boş Enter boş metin döndürür, geçmişe eklenmez; geçmişten aşağı gezinince taslak geri gelir. Kontrol kodları ve geçersiz Unicode yok sayılır. C010 için sentetik olay sarmalayıcıları aynı satir_olay yolunu çağırır.
- [x] D6 · 11 test · sabotaj: `dizi_boyut(d.kodlar) < 256` → `dizi_boyut(d.kodlar) < 257` → koşum kırmızı; geri alındı, köprü yeşil · 2026-11-05T20:09:48.049409+00:00
VARSAYIM D7: başlangıç (0,0), hız tam sayı 0..1024; düğmeler sol=1, sağ=2, orta=3 sırasıyla raporlanır. Tekerlek tam32 sınırlarında doyurulur. C011 alt-sınır ataması açık dönüşümle dolanıldı.
- [x] D7 · 10 test · sabotaj: `(d.y olarak tam64) - (o.dy olarak tam64)` → `(d.y olarak tam64) + (o.dy olarak tam64)` → koşum kırmızı; geri alındı, köprü yeşil · 2026-11-05T20:12:00.426227+00:00
VARSAYIM D8: sıfır kapasite geçerlidir, tüm yeni girdileri kayıp sayar. Üst sınır 1048576 eleman; kayip tam64 üst sınırda doyurulur. Python deque modeliyle 120 işlem ve 37 kayıp bağımsız doğrulandı.
- [x] D8 · 10 test · sabotaj: `d.kayip = d.kayip + 1` → `d.kayip = d.kayip` → koşum kırmızı; geri alındı, köprü yeşil · 2026-11-05T20:12:55.586117+00:00
VARSAYIM D9: BMP renk maskeleri sıfır olmayan, bitişik ve örtüşmeyen RGB alanlarıdır; bileşenler 255 ölçeğine aşağı yuvarlanır, alfa yok sayılır. 16384 boyut/16MiB piksel sınırı. bmp_ciz ham BMP alır, çizilen görünür piksel sayısını sonuç olarak döndürür.
- [x] D9 · 23 test · sabotaj: `eğer değil b.yukaridan` → `eğer b.yukaridan` → koşum kırmızı; geri alındı, köprü yeşil · 2026-11-05T20:14:45.644423+00:00
VARSAYIM E1: kabuk komut/prompt yankılamaz; ls . ve .. girdilerini gizler, cat çıktı sonuna LF ekler, bulunamadi hatasını kabukta hata: yok olarak normalleştirir. 84 scancode, 31 bayt UTF-8 dosya, 40x6 konsol ve gerçek PSF m glifinin 64 pikseli doğrulandı.
- [x] E1 · 7 test · sabotaj: `s = metin_birlestir(s, satir_sonu(g.ad))` → `s = metin_birlestir(s, satir_sonu("yanlis"))` → koşum kırmızı; geri alındı, köprü yeşil · 2026-11-05T20:17:11.237266+00:00
E2: 13 ayrıştırıcıda 3900 rastgele girdi, 5100 tek-bayt mutasyonu ve 26 sıfır/tek-bayt sınır girdisi iki derleyicide geçti. Geçerli fikstürler mutasyondan önce kabul edilmek zorunda. ACPI/harita/BMP birden fazla biçim için ayrı 300 mutasyon döngüsü içerir. Panik bulunmadı; her tek koşum 30 saniye sınırında.
- [x] E2 · 4 test · sabotaj: `eğer n < 54` → `eğer n < 0` → koşum kırmızı; geri alındı, köprü yeşil · 2026-11-05T20:19:02.708738+00:00
- [x] E3 · 43 modül/API envanteri · yeni KEMGU test yok; E1 tekrar ve 47 dosyalık köprü yeşil · sabotaj: BMP dokümantasyon satırı silinince envanter kontrolü kırmızı, geri alındı · 2026-11-05T20:21:25.305979+00:00
E2 test işlevi toplamı 43 (kapat satırındaki 3, seçilen BMP dosyasına aittir). İşlev sayacı artık fixture yardımcılarını değil mantıksal test işlevlerini sayar.
F1 dış kahin: 110 dosya oluşturuldu, 11 silindi, 12 eklendi; 111 canlı dosya ve 2 iç içe dizin. fsck.fat -n temiz: 114 files, 134/68890 clusters. Bağımsız Python okuyucu tüm LFN/sıra/sağlama, ~100+ kısa adlar, 111 içerik, iki FAT ve FSInfo 68756 boş küme doğruladı. mcopy/mdir hâlâ yok; yalnız bu araçların adımları atlandı.
- [x] F1 · 1 test · sabotaj: `yaz_u8(imaj, o, 229)` → `yaz_u8(imaj, o, 0)` → koşum kırmızı; geri alındı, köprü yeşil · 2026-11-05T20:25:36.558460+00:00
VARSAYIM F2: fsinfo_onar yalnız geçerli FSInfo başlığının sayaçlarını iki kopyada yeniler; sahipsiz_kumeler salt okunur, bozuk/çapraz erişilen zincirde hata verir, bozuk/ayrılmış FAT işaretlerini dosya tahsisi saymaz. 9 birim + 2 dış-kahin testi geçti. fsck.fat -n sayaç onarımını temiz buldu; fsck -a yalnız üretilmiş kopyada iki FSCK*.REC oluşturdu, zincirleri KEMGU ile aynı [100,101,200] üç kümeyi içerdi.
- [x] F2 · 9 test · sabotaj: `tahsis ve (oku_u8(gorulen, o) & m) == 0` → `tahsis ve (oku_u8(gorulen, o) & m) != 0` → koşum kırmızı; geri alındı, köprü yeşil · 2026-11-05T20:28:59.097621+00:00
VARSAYIM F3: A1 dosyasını hiç değiştirmeden klavye_tablo mevcut A1 durum/özel tuş çekirdeğini çağırır, yazdırılabilir karakterleri tabloyla seçer. En çok 128 eşleme, ilk eşleme kazanır. US ANSI + ISO ek 86 tuşu (ters bölü/boru); 49x4 Türkçe tablo eşdeğerliği ve 54 US/parametre testi yeşil.
- [x] F3 · 54 test · sabotaj: `ust = shift != d.caps` → `ust = shift veya d.caps` → koşum kırmızı; geri alındı, köprü yeşil · 2026-11-05T20:30:55.768754+00:00
VARSAYIM F4: buyuk_harf/kucuk_harf metin alıp Unicode 15.0.0 kod noktası eşlemelerini uygular; Türkçe i→İ, I→ı, İ→i istisnaları açık. 1525 üst/1433 alt eşlemenin tümü test edilir; çok karakterli üst eşlemeler desteklenir. Bağlamsal son sigma/Unicode normalizasyonu/tam casefold uygulanmaz. İsteğe bağlı yol_esit ayrı yol_harf modülündedir; mevcut FAT eşlemesi değişmez. 22 test yeşil.
- [x] F4 · 22 test · sabotaj: `kod: 105, metin: "İ"` → `kod: 105, metin: "I"` → koşum kırmızı; geri alındı, köprü yeşil · 2026-11-05T20:34:10.298160+00:00
VARSAYIM F5: üst sınır her modülün API kapasitesi veya sabit fikstürün son kullanılabilir alanı olarak seçildi; 39 saf modülün tümünde 0/1/üst/üst+1 tablo satırları var. Toplam 158 yeni sınır testi iki derleyicide geçti. Önceden mevcut VirtIO kapsam dışı. S002 ortak bayt toplama taşması gerçek panik üretti, iki kalıcı regresyon ve çıkarma temelli sınır denetimiyle düzeltildi. Ayrıntı codex_sinirlar.md.
- [x] F5 · 6 test · sabotaj: `ofs <= dizi_boyut(b) ve n <= dizi_boyut(b) - ofs` → `ofs + n <= dizi_boyut(b)` → koşum kırmızı; geri alındı, köprü yeşil · 2026-11-05T20:43:22.106136+00:00
Son kapı öncesi: A1–F5 toplam 39 iş bitti; 91 dosyalık köprüde 808 test işlevi geçti (4 başlangıç testi, 804 yeni). Ana dal çalışma sırasında abd9aa9/D-736 seviyesine ilerledi. PR farkının yalnız izinli dosyalardan oluşması için kendi dalım bu tabana rebase edilip yerel derleyiciler yeniden kurulacak ve köprü tekrar çalıştırılacak. Ana çalışma klasörüne yazılmadı.

## Sonuç ve kapsam

- A1–A6, B1–B9, C1–C7, D1–D9, E1–E3, F1–F5: **39/39 iş tamamlandı**; 45 dakika nedeniyle bırakılan iş yok. Liste 10 saat dolmadan bitti.
- 39 saf mantık modülü (38 yeni modül + ortak bayt yardımcı düzeltmesi), 49 modüllük API/kahin/sınır envanteri. Donanıma bağlama sonraki çalışma hattıdır.
- Güncel taban: `abd9aa9` (D-736). Ana klasöre yazmadan yalnız `/home/msyrt/Desktop/kemgu-codex` worktree'sinde çalışıldı; bu dal güncel tabana rebase edildi. PR farkında derleyici/runtime/boot/test kapısı değişikliği yok.

## Son test kapıları

- `make -j8 build/kemgu build/codegen build/kdl_runtime.o build/kdl_runtime_mmio.o`: güncel tabanda başarılı.
- `make calistir_chatgpt_kopru`: **91 dosya, 808 test işlevi** yeşil; 804 yeni test + başlangıçtaki 4 bayt testi. Dil kartı ayrıca geçti. Her dosya C ve self-host ile derlendi/çalıştı, exit=0 ve stdout eşit.
- `bash otomasyon/chatgpt/dogrula.sh tests/drivers/probe/*.kem`: 11 dolanma örneği güncel tabanda yeşil.
- E2: 3900 rastgele girdi + 5100 tek-bayt mutasyonu + 26 boş/tek bayt girdisi. F5: 39 modülde 158 sınır testi.
- E1: 84 scancode → iki komut → FAT32 Türkçe metin → konsol → gerçek font; ilk glifin 64 pikseli doğrulandı.
- Dış kahinler: fsck.fat, mkfs.fat, Python struct/zlib/uuid/tarfile/Unicode 15, readelf, gerçek PSF fontu. F1: 111 canlı dosya, iki iç içe dizin, ~100+ çakışma; fsck temiz. F2: fsck kurtarma zincirleriyle aynı [100,101,200] sahipsiz kümeler.
- **40 başarılı sabotaj:** 38 işin derlenen KEMGU bozması + E3 belge envanteri bozması + B8 ek dış fsck sayaç bozması. Her biri kırmızı oldu, geri alındı.
- `python3 tools/codex/son_kontrol.py`, `python3 tools/codex/envanter_kontrol.py`, `git diff --check origin/main`: yeşil.
- Host mimarisi: `aarch64`. Bu görev saf bellek mantığıdır; QEMU/donanım kabul kapısı istenmedi.

## Atlanan araç adımları ve bulgular

- `mcopy`/`mdir` kurulu olmadığından B8/F1'in yalnız bu araç adımları atlandı. Loop mount denenmedi. mkfs.fat ters yön boş kök fikstürü ve fsck/Python dış denetimleri çalıştı.
- C001–C011: 11 derleyici/araç zinciri bulgusu, küçük örnekler ve zararsız dolanmalar kaydedildi; derleyici değiştirilmedi. C009 yüzünden imaj APIlerinde güvenli adlar kullanılmalıdır.
- S001: FAT etiket metadata tutarlılığı düzeltildi. S002: bayt sınır hesabının tam32 taşması düzeltildi ve panik örnekleri kalıcı teste çevrildi.
- F4 dönüşümleri kod noktası temellidir; bağlamsal son sigma, Unicode normalizasyonu ve tam casefold kapsam dışıdır. Diğer API sınırları `drivers/README.md` ve `codex_sinirlar.md` içinde.
- Yeni `tekkez`/`yetki`/ISR kullanımı yok; bu görevde donanım ve ham bellek erişimi yasaktır.

## Dosya değişiklikleri (whitelist kontrolü)

`git diff --name-only origin/main` ile kapanıştaki yeni kontrol aracı birlikte 200 izinli yol. Makefile'ın yalnız `CHATGPT_TESTLER` wildcard listesi değişti; kalan içerik güncel main ile birebir.

```text
Makefile
drivers/README.md
drivers/acpi/fadt.kem
drivers/acpi/madt.kem
drivers/acpi/tablo.kem
drivers/bellek/cerceve.kem
drivers/bellek/harita.kem
drivers/disk/bolum.kem
drivers/disk/crc32.kem
drivers/disk/guid.kem
drivers/ekran/bmp.kem
drivers/ekran/cerceve.kem
drivers/ekran/konsol.kem
drivers/ekran/psf.kem
drivers/ekran/vt.kem
drivers/ekran/yazi.kem
drivers/elf/elf64.kem
drivers/fs/fat32_bicimle.kem
drivers/fs/fat32_bpb.kem
drivers/fs/fat32_dizin.kem
drivers/fs/fat32_imaj.kem
drivers/fs/fat32_kisa_ad.kem
drivers/fs/fat32_oku.kem
drivers/fs/fat32_onar.kem
drivers/fs/fat32_yaz.kem
drivers/fs/ustar.kem
drivers/fs/yol.kem
drivers/fs/yol_harf.kem
drivers/girdi/halka.kem
drivers/girdi/imlec.kem
drivers/girdi/klavye_tablo.kem
drivers/girdi/klavye_us.kem
drivers/girdi/satir.kem
drivers/ortak/bayt.kem
drivers/ortak/harf.kem
drivers/ortak/harf_veri.kem
drivers/ortak/utf8.kem
drivers/pci/yapilandirma.kem
drivers/ps2/fare.kem
drivers/ps2/klavye_tr.kem
drivers/sim/kabuk.kem
otomasyon/chatgpt/codex_bulgular.md
otomasyon/chatgpt/codex_font_kahini.md
otomasyon/chatgpt/codex_ilerleme.md
otomasyon/chatgpt/codex_sinirlar.md
tests/drivers/acpi/fadt_test.kem
tests/drivers/acpi/madt_test.kem
tests/drivers/acpi/tablo_test.kem
tests/drivers/bellek/cerceve_test.kem
tests/drivers/bellek/harita_test.kem
tests/drivers/bulanik/acpi_test.kem
tests/drivers/bulanik/bmp_test.kem
tests/drivers/bulanik/bolum_test.kem
tests/drivers/bulanik/bpb_test.kem
tests/drivers/bulanik/dizin_test.kem
tests/drivers/bulanik/elf_test.kem
tests/drivers/bulanik/fadt_test.kem
tests/drivers/bulanik/harita_test.kem
tests/drivers/bulanik/madt_test.kem
tests/drivers/bulanik/pci_test.kem
tests/drivers/bulanik/psf_test.kem
tests/drivers/bulanik/ustar_test.kem
tests/drivers/bulanik/veri/fikstur.kem
tests/drivers/bulanik/vt_test.kem
tests/drivers/disk/bolum_test.kem
tests/drivers/disk/guid_test.kem
tests/drivers/ekran/bmp_test.kem
tests/drivers/ekran/cerceve_test.kem
tests/drivers/ekran/konsol_test.kem
tests/drivers/ekran/psf_test.kem
tests/drivers/ekran/veri/bmp_fikstur.kem
tests/drivers/ekran/veri/psf_gercek.kem
tests/drivers/ekran/veri/psf_sentetik.kem
tests/drivers/ekran/vt_test.kem
tests/drivers/ekran/yazi_test.kem
tests/drivers/elf/elf64_test.kem
tests/drivers/elf/veri/gercek_elf.kem
tests/drivers/fs/fat32_bicimle_test.kem
tests/drivers/fs/fat32_bpb_test.kem
tests/drivers/fs/fat32_coklu_test.kem
tests/drivers/fs/fat32_dizin_test.kem
tests/drivers/fs/fat32_kahin_test.kem
tests/drivers/fs/fat32_kisa_ad_test.kem
tests/drivers/fs/fat32_linux_test.kem
tests/drivers/fs/fat32_oku_test.kem
tests/drivers/fs/fat32_onar_kahin_test.kem
tests/drivers/fs/fat32_onar_test.kem
tests/drivers/fs/fat32_yaz_test.kem
tests/drivers/fs/unicode_ad_test.kem
tests/drivers/fs/ustar_test.kem
tests/drivers/fs/veri/linux_fat32.kem
tests/drivers/fs/veri/ustar_veri.kem
tests/drivers/fs/yol_test.kem
tests/drivers/girdi/halka_test.kem
tests/drivers/girdi/imlec_test.kem
tests/drivers/girdi/klavye_us_test.kem
tests/drivers/girdi/satir_test.kem
tests/drivers/ortak/harf_test.kem
tests/drivers/ortak/utf8_test.kem
tests/drivers/pci/yapilandirma_test.kem
tests/drivers/probe/bayt_kopya.kem
tests/drivers/probe/bos_alan.kem
tests/drivers/probe/crc_ad.kem
tests/drivers/probe/enum_aktar.kem
tests/drivers/probe/host_ad.kem
tests/drivers/probe/sonuc_dizi.kem
tests/drivers/probe/sonuc_donus.kem
tests/drivers/probe/tam32_alt.kem
tests/drivers/probe/tasima_tipi.kem
tests/drivers/probe/yapi_arguman.kem
tests/drivers/probe/yapi_dizi.kem
tests/drivers/ps2/fare_test.kem
tests/drivers/ps2/klavye_tr_test.kem
tests/drivers/sim/masaustu_sim_test.kem
tests/drivers/sinir/acpi_fadt_test.kem
tests/drivers/sinir/acpi_madt_test.kem
tests/drivers/sinir/acpi_tablo_test.kem
tests/drivers/sinir/bellek_cerceve_test.kem
tests/drivers/sinir/bellek_harita_test.kem
tests/drivers/sinir/disk_bolum_test.kem
tests/drivers/sinir/disk_crc32_test.kem
tests/drivers/sinir/disk_guid_test.kem
tests/drivers/sinir/ekran_bmp_test.kem
tests/drivers/sinir/ekran_cerceve_test.kem
tests/drivers/sinir/ekran_konsol_test.kem
tests/drivers/sinir/ekran_psf_test.kem
tests/drivers/sinir/ekran_vt_test.kem
tests/drivers/sinir/ekran_yazi_test.kem
tests/drivers/sinir/elf_elf64_test.kem
tests/drivers/sinir/fs_fat32_bicimle_test.kem
tests/drivers/sinir/fs_fat32_bpb_test.kem
tests/drivers/sinir/fs_fat32_dizin_test.kem
tests/drivers/sinir/fs_fat32_imaj_test.kem
tests/drivers/sinir/fs_fat32_kisa_ad_test.kem
tests/drivers/sinir/fs_fat32_oku_test.kem
tests/drivers/sinir/fs_fat32_onar_test.kem
tests/drivers/sinir/fs_fat32_yaz_test.kem
tests/drivers/sinir/fs_ustar_test.kem
tests/drivers/sinir/fs_yol_harf_test.kem
tests/drivers/sinir/fs_yol_test.kem
tests/drivers/sinir/girdi_halka_test.kem
tests/drivers/sinir/girdi_imlec_test.kem
tests/drivers/sinir/girdi_klavye_tablo_test.kem
tests/drivers/sinir/girdi_klavye_us_test.kem
tests/drivers/sinir/girdi_satir_test.kem
tests/drivers/sinir/ortak_bayt_test.kem
tests/drivers/sinir/ortak_harf_test.kem
tests/drivers/sinir/ortak_harf_veri_test.kem
tests/drivers/sinir/ortak_utf8_test.kem
tests/drivers/sinir/pci_yapilandirma_test.kem
tests/drivers/sinir/ps2_fare_test.kem
tests/drivers/sinir/ps2_klavye_tr_test.kem
tests/drivers/sinir/sim_kabuk_test.kem
tools/codex/a1.py
tools/codex/a2.py
tools/codex/a3.py
tools/codex/a4.py
tools/codex/a5.py
tools/codex/a6.py
tools/codex/b1.py
tools/codex/b2.py
tools/codex/b3.py
tools/codex/b4.py
tools/codex/b5.py
tools/codex/b6.py
tools/codex/b7.py
tools/codex/b8.py
tools/codex/b9.py
tools/codex/c1.py
tools/codex/c2.py
tools/codex/c3.py
tools/codex/c4.py
tools/codex/c5.py
tools/codex/c6.py
tools/codex/c7.py
tools/codex/d1.py
tools/codex/d2.py
tools/codex/d3.py
tools/codex/d4.py
tools/codex/d5.py
tools/codex/d6.py
tools/codex/d7.py
tools/codex/d8.py
tools/codex/d9.py
tools/codex/e1.py
tools/codex/e2.py
tools/codex/e3.py
tools/codex/envanter_kontrol.py
tools/codex/f1.py
tools/codex/f2.py
tools/codex/f2_kahin.py
tools/codex/f3.py
tools/codex/f4.py
tools/codex/f5.py
tools/codex/fat32_coklu_kahin.sh
tools/codex/fat32_kahin.sh
tools/codex/fat32_onar_kahin.sh
tools/codex/kapat.py
tools/codex/son_kontrol.py
tools/codex/yardim.py
```

## Sonraki adım

Claude tarafından draft PR incelemesi ve saf APIlerin donanım taşıma/ekran/kesme hattına bağlanması. Auto-merge veya main'e push yapılmaz.

Bitiş ölçümü: 2026-11-05T20:46:42.548115+00:00 (`date` UTC). Başlangıç: 2026-11-05 18:54:35 UTC. Toplam çalışma: 1 saat 52 dakika 7 saniye.
