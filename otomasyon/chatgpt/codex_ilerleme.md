Thu Nov  5 18:54:35 UTC 2026

VARSAYIM: Bu oturumda son kullanıcı isteği ve CODEX_GOREV.md kapsamı geçerlidir; önceki AGENTS kapsamı/dal adı yerine bu görevde açıkça izin verilen yollar ve codex/surucu-dalga1 kullanılır.
VARSAYIM: Bayt API'lerinde 0..255 dışı girişler yok sayılır ve yarım paket/önek temizlenir.
- [x] A1 · 62 test · sabotaj: `shift != d.caps` → `shift veya d.caps` → koşum kırmızı; geri alındı, köprü yeşil · 2026-11-05T18:56:07.334704+00:00
- [x] A2 · 19 test · sabotaj: `(d.b0 & 192) != 0` → `(d.b0 & 192) == 1` → koşum kırmızı; geri alındı, köprü yeşil · 2026-11-05T18:56:54.848469+00:00
VARSAYIM: CRC32 API sonuç tipi hata taşımadığından geçersiz ofset/uzunluk için 0 döner. GPT görüntüsü fiziksel diskin yalnız ön bölümü olabilir; bölüm LBA sınırları görüntü boyuyla kıyaslanmaz. C001 için nitelikli çağrı probe ile doğrulandı.
