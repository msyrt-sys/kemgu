# Görev: CRC32 + MBR/GPT bölüm tablosu ayrıştırıcı

## Üretilecek dosyalar
1. `drivers/disk/crc32.kem` — `genel işlev crc32(b: Dizi<dtam8>, ofs: tam32, n: tam32) -> tam64`
2. `drivers/disk/bolum.kem` — MBR/GPT
3. `tests/drivers/disk/bolum_test.kem` — İKİ modülü birden test eder

## CRC32
IEEE 802.3, yansıtılmış, polinom 3988292384 (0xEDB88320), başlangıç
4294967295, sonda 4294967295 ile XOR. Tablosuz bit-bit döngü yeterli. Sonuç
`tam64` (0..4294967295). Aritmetiği `tam64`te yap ve her adımda
`& 4294967295` ile 32 bite indir.
**Test vektörleri (zlib ile ölçüldü):** `"123456789"` → 3421780262,
`""` → 0, `"a"` → 3904355907.

## API (bolum.kem)
```
genel yapı Bolum { tur: tam32; baslangic_lba: tam64; sektor_sayisi: tam64; ad: metin; }
genel çeşit BolumHatasi { KisaGirdi, ImzaYok, KorumaciMBRDegil, GptImzaYok,
                          GptBaslikCrc, GptGirdiCrc, GptBoyut }
genel işlev mbr_coz(lba0: Dizi<dtam8>) -> sonuç<Dizi<Bolum>, BolumHatasi>
genel işlev gpt_mi(lba0: Dizi<dtam8>) -> mantıksal
genel işlev gpt_coz(disk: Dizi<dtam8>, sektor: tam32) -> sonuç<Dizi<Bolum>, BolumHatasi>
```
`disk` görüntünün baştan itibaren baytlarıdır; `sektor` sektör boyutu (512).

## MBR (LBA 0, 512 bayt)
- 510 = 85 (0x55), 511 = 170 (0xAA); değilse `ImzaYok`. 512'den kısa → `KisaGirdi`.
- 4 girdi, 446'dan başlar, 16'şar bayt: +4 tür (u8), +8 başlangıç LBA (le32),
  +12 sektör sayısı (le32). Tür 0 olan girdiler atlanır. `ad = ""`.
- `gpt_mi`: imza geçerli ve 1. girdinin türü 238 (0xEE) ise doğru.

## GPT (LBA 1 başlık, girdiler başlıkta yazan LBA'da)
Başlık ofsetleri: 0 imza `"EFI PART"` · 8 revizyon le32 · 12 başlık boyu le32
(92) · 16 başlık CRC le32 · 24 bu LBA le64 · 32 yedek LBA · 40 ilk kullanılabilir
· 48 son kullanılabilir · 56 disk GUID (16) · 72 girdi LBA le64 · 80 girdi sayısı
le32 · 84 girdi boyu le32 · 88 girdi dizisi CRC le32.
- Başlık CRC: başlığın ilk `başlık boyu` baytı üzerinde, **16..19 baytları SIFIR
  sayılarak** hesaplanır (kopyada sıfırla). Uyuşmazsa `GptBaslikCrc`.
- Girdi CRC: `girdi sayısı × girdi boyu` bayt üzerinde. Uyuşmazsa `GptGirdiCrc`.
- `koruyucu MBR değil` → `KorumaciMBRDegil`; imza yok → `GptImzaYok`;
  başlık boyu < 92 ya da girdi boyu < 128 ya da tablo görüntü dışına taşıyorsa → `GptBoyut`.
- Girdi: 0..15 tür GUID, 32 ilk LBA le64, 40 son LBA le64, 56 ad (72 bayt UTF-16LE,
  ilk 0 kod biriminde biter). **Tür GUID'i tamamen sıfır olan girdi boştur, atlanır.**
- `Bolum.tur = 238` (GPT için), `sektor_sayisi = son - ilk + 1`, `ad` UTF-16LE'den
  `kod_metin` ile kurulur (yalnız BMP; vekil çifti V1'de `?`).

## Test diski (değerler zlib ile hesaplandı — testte bunları BAYT BAYT kur)
Toplam 3 × 512 bayt sıfırlanmış dizi:
- LBA0: 446+4 = 238, 446+8 = 1 (le32), 446+12 = 2047 (le32), 510/511 = 85/170.
- LBA2 (bayt 1024'ten, 4 girdi × 128): girdi 0 tür GUID baytları
  `40 115 42 193 31 248 210 17 186 75 0 160 201 62 201 59`, 16..31 hepsi 34,
  ilk LBA 34, son LBA 2014, ad `"EFI"` UTF-16LE (69 0 70 0 73 0). Diğer 3 girdi sıfır.
- LBA1 (bayt 512'den): `"EFI PART"`, revizyon 65536, boy 92, CRC alanı (sonra),
  bu LBA 1, yedek 2047, ilk 34, son 2014, disk GUID 56..71 hepsi 17,
  girdi LBA 2, girdi sayısı 4, girdi boyu 128, **girdi CRC 2282802036**,
  **başlık CRC 3793980875** (16. bayttan le32 yaz).
- Beklenen: `gpt_coz` → 1 bölüm, `baslangic_lba` 34, `sektor_sayisi` 1981, ad `"EFI"`.
- Olumsuz: başlık CRC'sinin bir baytını boz → `GptBaslikCrc`; girdi baytı boz
  (CRC'yi güncellemeden) → `GptGirdiCrc`; 510'u boz → `ImzaYok`.
- MBR testi: tür 12 (FAT32 LBA) girdisi, başlangıç 2048, sayı 100000 → 1 bölüm.

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
