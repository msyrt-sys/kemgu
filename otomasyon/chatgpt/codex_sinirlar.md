# F5 sınır tabloları

Üst sınır, her satırda belirtilen API alanının kapasitesi veya sabit fikstürün son geçerli konumudur. Sınırı olmayan APIlerde fazladan girdinin kabul edilmesi de açıkça sınanır. Önceden mevcut VirtIO modülleri bu saf-mantık dalgasına dahil değildir.

| Modül | Sınır alanı | Değerler | Test |
|---|---|---|---|
| `acpi/fadt` | FADT fikstürü kullanılabilir kayıt boyu | `[0, 1, 148, 149]` | [4 test](../../tests/drivers/sinir/acpi_fadt_test.kem) |
| `acpi/madt` | tek LAPIC girdisine ayrılmış 8 bayt | `[0, 1, 8, 9]` | [4 test](../../tests/drivers/sinir/acpi_madt_test.kem) |
| `acpi/tablo` | 52 baytlık XSDT içindeki kullanılabilir yük uzunluğu | `[0, 1, 16, 17]` | [4 test](../../tests/drivers/sinir/acpi_tablo_test.kem) |
| `bellek/cerceve` | kullanılabilir sayfa kapasitesi | `[0, 1, 1048576, 1048577]` | [4 test](../../tests/drivers/sinir/bellek_cerceve_test.kem) |
| `bellek/harita` | girdi bölge sayısı; sıfır uzunluklu bölgeler elenir | `[0, 1, 4096, 4097]` | [4 test](../../tests/drivers/sinir/bellek_harita_test.kem) |
| `disk/bolum` | dört MBR bölümü; beşinci kayıt sektör dışında yok sayılır | `[0, 1, 4, 5]` | [4 test](../../tests/drivers/sinir/disk_bolum_test.kem) |
| `disk/crc32` | 3 baytlık CRC aralığının uzunluğu | `[0, 1, 3, 4]` | [4 test](../../tests/drivers/sinir/disk_crc32_test.kem) |
| `disk/guid` | GUID sabit kayıt boyu 16 | `[0, 1, 16, 17]` | [4 test](../../tests/drivers/sinir/disk_guid_test.kem) |
| `ekran/bmp` | BMP genişliği | `[0, 1, 16384, 16385]` | [4 test](../../tests/drivers/sinir/ekran_bmp_test.kem) |
| `ekran/cerceve` | 32bpp çerçeve genişliği | `[0, 1, 16384, 16385]` | [4 test](../../tests/drivers/sinir/ekran_cerceve_test.kem) |
| `ekran/konsol` | konsol sütun sayısı | `[0, 1, 4096, 4097]` | [4 test](../../tests/drivers/sinir/ekran_konsol_test.kem) |
| `ekran/psf` | PSF2 glif genişliği | `[0, 1, 4096, 4097]` | [4 test](../../tests/drivers/sinir/ekran_psf_test.kem) |
| `ekran/vt` | CSI parametre sayısı | `[0, 1, 16, 17]` | [4 test](../../tests/drivers/sinir/ekran_vt_test.kem) |
| `ekran/yazi` | dört glifli fontun glif indeksi | `[0, 1, 3, 4]` | [4 test](../../tests/drivers/sinir/ekran_yazi_test.kem) |
| `elf/elf64` | sıfır dosya boylu LOAD için dosya ofseti | `[0, 1, 512, 513]` | [4 test](../../tests/drivers/sinir/elf_elf64_test.kem) |
| `fs/fat32_bicimle` | FAT32 küme başına sektör | `[0, 1, 128, 129]` | [4 test](../../tests/drivers/sinir/fs_fat32_bicimle_test.kem) |
| `fs/fat32_bpb` | BPB sektör bayt boyu | `[0, 1, 4096, 4097]` | [4 test](../../tests/drivers/sinir/fs_fat32_bpb_test.kem) |
| `fs/fat32_dizin` | LFN parça sayısı; geçersiz uzun ad kısa ada düşer | `[0, 1, 20, 21]` | [4 test](../../tests/drivers/sinir/fs_fat32_dizin_test.kem) |
| `fs/fat32_imaj` | 16MiB bellek önekine sığan son küme numarası | `[0, 1, 31659, 31660]` | [4 test](../../tests/drivers/sinir/fs_fat32_imaj_test.kem) |
| `fs/fat32_kisa_ad` | 8.3 taban adının doğrudan uzunluğu | `[0, 1, 8, 9]` | [4 test](../../tests/drivers/sinir/fs_fat32_kisa_ad_test.kem) |
| `fs/fat32_oku` | tek kümeli dosyanın ilan edilen uzunluğu | `[0, 1, 512, 513]` | [4 test](../../tests/drivers/sinir/fs_fat32_oku_test.kem) |
| `fs/fat32_onar` | FSInfo boş küme sayacının fiziksel küme sayısına göre sınırı | `[0, 1, 68890, 68891]` | [4 test](../../tests/drivers/sinir/fs_fat32_onar_test.kem) |
| `fs/fat32_yaz` | tek küme veri kapasitesi ve sonraki kümeye geçiş | `[0, 1, 512, 513]` | [4 test](../../tests/drivers/sinir/fs_fat32_yaz_test.kem) |
| `fs/ustar` | 2048 baytlık USTAR içinde iki son blokla kalan dosya kapasitesi | `[0, 1, 512, 513]` | [4 test](../../tests/drivers/sinir/fs_ustar_test.kem) |
| `fs/yol` | yol bileşeni UTF-8 bayt uzunluğu | `[0, 1, 255, 256]` | [4 test](../../tests/drivers/sinir/fs_yol_test.kem) |
| `fs/yol_harf` | normalleştirilen yol bileşeni bayt uzunluğu | `[0, 1, 255, 256]` | [4 test](../../tests/drivers/sinir/fs_yol_harf_test.kem) |
| `girdi/halka` | FIFO eleman kapasitesi | `[0, 1, 1048576, 1048577]` | [4 test](../../tests/drivers/sinir/girdi_halka_test.kem) |
| `girdi/imlec` | imleç hız çarpanı | `[0, 1, 1024, 1025]` | [4 test](../../tests/drivers/sinir/girdi_imlec_test.kem) |
| `girdi/klavye_tablo` | karakter eşleme tablosu kapasitesi | `[0, 1, 128, 129]` | [4 test](../../tests/drivers/sinir/girdi_klavye_tablo_test.kem) |
| `girdi/klavye_us` | US giriş baytı | `[0, 1, 255, 256]` | [4 test](../../tests/drivers/sinir/girdi_klavye_us_test.kem) |
| `girdi/satir` | satırdaki Unicode kod noktası sayısı | `[0, 1, 256, 257]` | [4 test](../../tests/drivers/sinir/girdi_satir_test.kem) |
| `ortak/bayt` | 3 baytlık tamponun erişim uzunluğu | `[0, 1, 3, 4]` | [6 test](../../tests/drivers/sinir/ortak_bayt_test.kem) |
| `ortak/harf` | boş/tek harf ve UTF-8 skaler üst sınırı | `[0, 1, 1114111, 1114112]` | [4 test](../../tests/drivers/sinir/ortak_harf_test.kem) |
| `ortak/harf_veri` | üretilmiş üst eşleme dizisi indeksi | `[0, 1, 1524, 1525]` | [4 test](../../tests/drivers/sinir/ortak_harf_veri_test.kem) |
| `ortak/utf8` | Unicode skaler kod noktası | `[0, 1, 1114111, 1114112]` | [4 test](../../tests/drivers/sinir/ortak_utf8_test.kem) |
| `pci/yapilandirma` | sabit PCI kayıt boyu; sonda ek bayt yok sayılır | `[0, 1, 256, 257]` | [4 test](../../tests/drivers/sinir/pci_yapilandirma_test.kem) |
| `ps2/fare` | fare X paket baytı | `[0, 1, 255, 256]` | [4 test](../../tests/drivers/sinir/ps2_fare_test.kem) |
| `ps2/klavye_tr` | PS/2 giriş baytı | `[0, 1, 255, 256]` | [4 test](../../tests/drivers/sinir/ps2_klavye_tr_test.kem) |
| `sim/kabuk` | satır düzenleyici sınırında doğrudan echo; kabuk ayrı sınır koymaz | `[0, 1, 256, 257]` | [4 test](../../tests/drivers/sinir/sim_kabuk_test.kem) |

39 modül, 156 tablo satırı ve ortak bayt denetimi için 2 taşma regresyonu. Beklentiler Python tam sayıları, struct, zlib, tarfile ve Unicode 15 verisinden hesaplanır.
