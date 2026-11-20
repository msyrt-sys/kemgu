# KEMGU-OS kullanıcı ABI'si v2

x86_64'te `int 0x80` girişinde RAX çağrı numarasıdır. İlk üç argüman sırasıyla RDI, RDX ve RSI'dedir. Sonuç işaretli 64 bit olarak RAX'tadır. Diğer genel amaçlı yazmaçlar ve RFLAGS korunur. İkinci argümanın RDX'te kalması PVH ve D-699/D-705 kullanıcı koduyla uyumluluğu korur.

| No | Çağrı | Argüman sayısı | Argümanlar | Dönüş | Mimari |
| --- | --- | --- | --- | --- | --- |
| 5 | eski_yaz | 1 | NUL sonlu metin | 0 veya eski hata | x86_64, arm64 |
| 7 | eski_satir | 0 | | Eski çağrı sonucu | arm64 |
| 10 | tik | 0 | | Zamanlayıcı tik sayısı | x86_64, arm64 |
| 11 | pid | 0 | | Geçerli görev kimliği | x86_64, arm64 |
| 12 | eski_spawn | 0 | | Eski çağrı sonucu | arm64 |
| 13 | cik | 1 | Kod (arm64 eski çağrı) | Süreç çağrısında dönmez | x86_64, arm64 |
| 14 | durum | 1 | Görev kimliği | Görev durumu | x86_64, arm64 |
| 17 | eski_dosya_yaz | 2 | NUL sonlu ad ve içerik | Yazılan bayt veya eski hata | x86_64, arm64 |
| 18 | eski_dosya_oku | 2 | NUL sonlu ad ve 512 baytlık tampon | Okunan bayt veya eski hata | x86_64, arm64 |
| 24 | eski_ag_yolla | 2 | Eski ağ çağrısı | Eski çağrı sonucu | arm64 |
| 25 | eski_ag_oku | 2 | Eski ağ çağrısı | Eski çağrı sonucu | arm64 |
| 26 | eski_satir_oku | 2 | Tampon ve kapasite | Okunan bayt veya eski hata | x86_64, arm64 |
| 27 | eski_saat | 0 | | Eski çağrı sonucu | arm64 |
| 40 | abi_surumu | 0 | | 2 | x86_64 |

40–79 yeni x86_64 çağrılarına ayrılmıştır. Bilinmeyen x86_64 kullanıcı çağrıları `-38` döndürür. Çekirdek kipindeki eski PVH çağrı testlerinin ayrılmış aralık dışındaki bilinmeyen numaralar için `-1` sonucu korunur. arm64 mevcut ABI ile çalışır.

| Değer | Ad | Anlamı |
| --- | --- | --- |
| -38 | NOSYS | Çağrı numarası tanımlı değil |
