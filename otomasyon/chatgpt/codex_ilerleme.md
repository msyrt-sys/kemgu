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
