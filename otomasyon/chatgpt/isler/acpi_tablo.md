# Görev: ACPI RSDP + SDT başlığı + RSDT/XSDT yürüyüşü

Bellek okumayı ÇAĞIRAN yapacak; bu modül verilen bayt dizileri üzerinde çalışır.

## Üretilecek dosyalar
1. `drivers/acpi/tablo.kem`
2. `tests/drivers/acpi/tablo_test.kem`

## API (BİREBİR)
```
genel yapı Rsdp { revizyon: tam32; rsdt: tam64; xsdt: tam64; oem: metin; }
genel yapı SdtBaslik { imza: metin; uzunluk: tam64; revizyon: tam32; oem: metin; }
genel çeşit AcpiHatasi { KisaGirdi, ImzaYok, Saglama, GenisSaglama, Uzunluk }
genel işlev rsdp_coz(b: Dizi<dtam8>) -> sonuç<Rsdp, AcpiHatasi>
genel işlev rsdp_ara(b: Dizi<dtam8>) -> seçimlik<tam32>
genel işlev sdt_coz(b: Dizi<dtam8>) -> sonuç<SdtBaslik, AcpiHatasi>
genel işlev sdt_girdileri(b: Dizi<dtam8>, xsdt_mi: mantıksal) -> sonuç<Dizi<tam64>, AcpiHatasi>
```

## RSDP
0 imza `"RSD PTR "` (8 bayt, sonda boşluk) · 8 saglama · 9 OEM kimliği (6) ·
15 revizyon · 16 RSDT adresi le32 · (rev ≥ 2:) 20 uzunluk le32 · 24 XSDT le64 ·
32 genişletilmiş saglama.
- İlk 20 baytın toplamı mod 256 = 0 olmalı → değilse `Saglama`.
- rev ≥ 2 ise ilk `uzunluk` baytın (36) toplamı da 0 olmalı → `GenisSaglama`.
- rev < 2 ise `xsdt = 0`. `oem` ASCII'den `kod_metin` ile, sondaki boşluklar kırpılır.
- `rsdp_ara`: dizide **16 bayt hizalı** konumlarda `"RSD PTR "` arar ve ilk
  saglaması geçerli olanın ofsetini döndürür (BIOS alanı taraması böyle yapılır).

## SDT başlığı (36 bayt)
0 imza (4) · 4 uzunluk le32 · 8 revizyon · 9 saglama · 10 OEM (6) · 16 OEM tablo (8) ·
24 OEM rev · 28 yaratıcı · 32 yaratıcı rev.
- Tüm `uzunluk` baytın toplamı 0 olmalı → `Saglama`; uzunluk < 36 ya da dizi
  boyundan büyük → `Uzunluk`.
- `sdt_girdileri`: başlıktan sonra RSDT için 4'er bayt (le32), XSDT için 8'er
  bayt (le64) adres; `(uzunluk - 36)` girdi boyunun katı değilse → `Uzunluk`.

## Test en az şunları ölçmeli
- Testte saglama baytını kendin hesapla: önce 0 yaz, `bayt_toplam8` ile topla,
  `(256 - toplam) & 255` yaz. Sonra `bayt_toplam8` = 0 doğrula.
- v1 RSDP (rev 0, RSDT 1048576) → `xsdt` 0, oem `"KEMGU"` (6 bayt "KEMGU ").
- v2 RSDP (rev 2, uzunluk 36, XSDT 8589934592) → `xsdt` 8589934592.
- Bozuk ilk saglama → `Saglama`; bozuk genişletilmiş → `GenisSaglama`.
- `rsdp_ara`: 64 baytlık dizide 32. ofsete koy → `değer(32)`; 33'e koy → `hiç`.
- XSDT 36 + 2×8 = 52 bayt, girdiler 4096 ve 8192 → [4096, 8192].
- Uzunluk 50 olan XSDT (14 artık bayt) → `Uzunluk`.

## Genel kurallar (tüm paketler)
- Ortak bayt yardımcıları HAZIR ve doğrulanmış: `drivers/ortak/bayt.kem`
  (`bayt_sinirda`, `oku_u8`, `oku_le16`, `oku_le32`→tam64, `oku_le64`, `imza_esit`,
  `bayt_toplam8`, `bayt_sifir`, `yaz_u8/le16/le32/le64`, `yaz_ascii`). Bunları
  `kullan drivers::ortak::bayt::{...};` ile al; aynı işi yeniden YAZMA.
- Her okumadan önce `bayt_sinirda` ile sınırı denetle: bozuk/kısa girdi PANİK
  değil, `hata(...)` döndürmeli.
- Yalnız bu paketin adını verdiği dosyaları üret (`yeni:` blokları). Başka
  dosyaya dokunma.
- Kart dışı sözdizimi kullanma. Emin olmadığın her şeyi "VARSAYIMLAR" altında yaz.
- Doğrulama: `otomasyon/chatgpt/dogrula.sh <test dosyası>` iki derleyicide
  derleyip çalıştırır; test exit 0 vermeli.
