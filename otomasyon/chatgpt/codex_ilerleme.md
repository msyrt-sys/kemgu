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
