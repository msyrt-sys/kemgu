# KEMGU sürücü modülleri

Bu dalın yeni modülleri bayt dizileri üzerinde saf mantık çalıştırır. C bootstrap ve self-host derleyicileriyle aynı test çıktısı zorunludur. Donanım/QEMU bütünleştirmesi bu saf-mantık dalgasının kabul kapısı değildir.

`make calistir_chatgpt_kopru` dil kartını, birim testlerini, E1 benzetimini ve E2 bozuk girdi koşumlarını çalıştırır. Tek test için `bash otomasyon/chatgpt/dogrula.sh tests/drivers/ekran/bmp_test.kem` kullanılır. Her ikisi repo kökünden çağrılır.

Tabloda **Çift derleyici** yeni dalga testlerinin geçtiğini; **Önceden mevcut** dalga kapsamındaki doğrulamanın bu eski modüller için yapılmadığını belirtir. API sütununda kaynakta `genel` işaretli işlev, yapı, çeşit ve sabit adları listelenir.

| Modül | Amaç | Genel API | Durum | Kahin | Bilinen sınır |
|---|---|---|---|---|---|
| [acpi/fadt](acpi/fadt.kem) | FADT güç/kesme alanları ve GAS | `Gas`, `Fadt`, `fadt_coz`, `dsdt_adresi` | Çift derleyici | Python struct ACPI fikstürü | En az 129 bayt; X_DSDT yalnız en az 148 baytta, sıfır değilse öncelikli |
| [acpi/madt](acpi/madt.kem) | MADT işlemci ve kesme girdileri | `MadtGirdisi`, `Madt`, `madt_coz` | Çift derleyici | Python struct ACPI fikstürü | Tür 0/1/2/4/5; bilinmeyen türler uzunluk denetimiyle atlanır |
| [acpi/tablo](acpi/tablo.kem) | RSDP/RSDT/XSDT başlık ve sağlama çözümü | `Rsdp`, `SdtBaslik`, `AcpiHatasi`, `rsdp_coz`, `rsdp_ara`, `sdt_coz`, `sdt_girdileri` | Çift derleyici | Python struct ve toplam mod 256 | Fiziksel bellek eşleme yok; RSDP arama 16 bayt adımlıdır |
| [bellek/cerceve](bellek/cerceve.kem) | Bitmap fiziksel sayfa ayırıcı modeli | `CerceveAyirici`, `cerceve_yeni`, `ayir_ardisik`, `ayir`, `serbest`, `ayrilmis_isaretle` | Çift derleyici | 4096 bayt hizalama/sıra vektörleri | İlk 1MiB ayrılmış; en çok 1048576 kullanılabilir sayfa; gerçek eşleme yok |
| [bellek/harita](bellek/harita.kem) | E820/Multiboot2 bellek aralıkları | `BellekBolgesi`, `harita_duzenle`, `e820_coz`, `mb2_coz`, `kullanilabilir_bayt`, `sayfa_sayisi` | Çift derleyici | Python bağımsız aralık süpürmesi | En çok 4096 bölge; pozitif tam64 adresler; kullanılamaz tür örtüşmede kazanır |
| [disk/bolum](disk/bolum.kem) | MBR ve CRC denetimli GPT bölüm tablosu | `Bolum`, `BolumHatasi`, `mbr_coz`, `gpt_mi`, `gpt_coz` | Çift derleyici | Python struct/uuid/zlib | Disk baytları bellekte; genişletilmiş MBR zinciri yok |
| [disk/crc32](disk/crc32.kem) | CRC32/IEEE hesabı | `crc32` | Çift derleyici | Python zlib.crc32 | Geçersiz aralık sıfır döner |
| [disk/guid](disk/guid.kem) | GPT GUID dönüşümü ve tür adları | `guid_bicimle`, `guid_coz`, `guid_tur_adi` | Çift derleyici | Python uuid.bytes_le | 16 bayt veya 36 karakter kanonik metin |
| [ekran/bmp](ekran/bmp.kem) | 24/32 bit BMP çözme ve çizme | `Bmp`, `bmp_coz`, `bmp_ciz` | Çift derleyici | Python struct BMP ve bağımsız piksel matrisi | BI_RGB/32 bit BI_BITFIELDS; alfa yok sayılır; bitişik RGB maskesi; 16MiB piksel |
| [ekran/cerceve](ekran/cerceve.kem) | 32bpp BGRX piksel/dolgu/kopya | `Cerceve`, `cerceve_gecerli`, `cerceve_yeni`, `piksel_yaz`, `piksel_oku`, `dikdortgen_doldur`, `kaydir_yukari`, `kopyala` | Çift derleyici | Python renk baytları ve kırpma | 0x00RRGGBB; adım bayt; 16384 boyut ve 16MiB tampon |
| [ekran/konsol](ekran/konsol.kem) | Unicode hücre ızgarası ve kaydırma | `Konsol`, `konsol_yeni`, `konsol_alan_sil`, `konsol_kod`, `konsol_metin`, `konsol_imlec`, `kirli_satirlar`, `konsol_satir` | Çift derleyici | Bağımsız konsol/sarma örnekleri | 1M hücre, boyut en çok 4096; sekme 8 sütun; kirli satırlar tüketilir |
| [ekran/psf](ekran/psf.kem) | PSF1/PSF2 font ve Unicode eşlemesi | `PsfFont`, `psf_coz`, `glif_indeksi` | Çift derleyici | Lat38-VGA8.psf.gz ve Python PSF çözümü | Tek kod noktalı eşleme; ligatür dizileri doğrulanıp atlanır; 65536 glif/4096 boyut |
| [ekran/vt](ekran/vt.kem) | ANSI CSI ve parçalı UTF-8 konsol akışı | `VtDurumu`, `vt_yeni`, `vt_bayt`, `vt_metin`, `vt_bitir` | Çift derleyici | ANSI durum örnekleri ve E2 mutasyonları | 16 parametre, sayılar 4096da doyar; OSC/DCS yutulur; sınırlı SGR |
| [ekran/yazi](ekran/yazi.kem) | Glif, Unicode kod ve metin çizimi | `glif_ciz`, `kod_ciz`, `metin_ciz` | Çift derleyici | Python MSB piksel matrisi, gerçek PSF | U+FFFD sonra ? yedek glif; çizim kırpılır |
| [elf/elf64](elf/elf64.kem) | ELF64 yükleme planı doğrulama | `ElfSegment`, `Elf64`, `elf64_coz` | Çift derleyici | readelf -lhW /bin/true ve Python struct | LE x86_64/AArch64; EXEC/DYN; W^X, örtüşme ve giriş denetimi; çalıştırmaz |
| [fs/fat32_bicimle](fs/fat32_bicimle.kem) | Seyrek bellek FAT32 imajı kurma | `FAT32_IMAJ_SINIRI`, `fat32_bicimle` | Çift derleyici | fsck.fat -n; mkfs.fat ters örneği; F1 111 dosya | 512 bayt sektör, 32 ayrılmış sektör, iki FAT; en az 65525 küme; 16MiB bellek öneki |
| [fs/fat32_bpb](fs/fat32_bpb.kem) | FAT32 BPB ve küme zinciri | `Fat32Bpb`, `BpbHatasi`, `bpb_coz`, `kume_sektoru`, `FatGirdi`, `fat_girdi`, `zincir` | Çift derleyici | Python struct ve FAT sınır vektörleri | BPB çözümü küçük örnekleri kabul eder; zincir azami uzunlukla sınırlı |
| [fs/fat32_dizin](fs/fat32_dizin.kem) | 8.3 ve UTF-16 LFN dizin çözümü | `DizinGirdisi`, `kisa_ad_saglama`, `dizin_coz` | Çift derleyici | Python UTF-16LE ve LFN sağlama | Bozuk LFN kısa ada düşer; kapalı tür tamlığına güvenilmez |
| [fs/fat32_imaj](fs/fat32_imaj.kem) | Ortak FAT/dizin imaj erişimi | `DizinResmi`, `dizin_resmi_bos`, `FatKayit`, `imaj_bpb`, `imaj_fat_oku`, `imaj_fat_yaz`, `imaj_kume_bayt`, `imaj_konum`, `imaj_buyut`, `imaj_zincir`, `imaj_dizin`, `imaj_kayitlar`, `imaj_ad_esit`, `imaj_bul`, `imaj_dizin_yolu` | Çift derleyici | B6-B8 gidiş/dönüş ve fsck.fat | Mantıksal disk sonundaki tutulmayan baytlar sıfırdır; yardımcılar doğrulanmış BPB bekler |
| [fs/fat32_kisa_ad](fs/fat32_kisa_ad.kem) | Çakışmasız FAT kısa adı üretme | `lfn_gerekli_mi`, `kisa_ad_uret` | Çift derleyici | 8.3 ve ~N sayaç vektörleri | 11 bayt ham ad; ~1..~999999; ASCII dışı karakter _ olur |
| [fs/fat32_oku](fs/fat32_oku.kem) | İmajdan dosya ve dizin okuma | `imajdan_dosya_oku`, `imaj_dizin_listele`, `dosya_oku`, `dizin_listele` | Çift derleyici | Gidiş/dönüş; Linux mkfs.fat örneği | 16MiB dosya sınırı; liste . ve .. içerir; C009 için imajdan_/imaj_ APIleri |
| [fs/fat32_onar](fs/fat32_onar.kem) | FSInfo sayacı onarma ve sahipsiz küme tespiti | `Fat32Sayim`, `fsinfo_onar`, `sahipsiz_kumeler` | Çift derleyici | fsck.fat kurtarılan küme listesi; sayaç onarımı sonrası temiz denetim | Yalnız sayaçları yazar; sahipsiz kümeleri silmez; çapraz/bozuk erişilen zincir hatadır |
| [fs/fat32_yaz](fs/fat32_yaz.kem) | Dosya/dizin oluşturma ve dosya silme | `imaja_dosya_yaz`, `imajda_dizin_olustur`, `imajdan_dosya_sil`, `dosya_yaz`, `dizin_olustur`, `dosya_sil` | Çift derleyici | Gidiş/dönüş, dolu disk, FAT aynaları, fsck.fat | Üzerine yazma ve dizin silme yok; C009 nedeniyle imaja_/imajda_/imajdan_ APIlerini kullan |
| [fs/ustar](fs/ustar.kem) | USTAR arşiv dizini ve dosya bulma | `TarGirdisi`, `tar_listele`, `tar_bul` | Çift derleyici | Python tarfile USTAR_FORMAT | Yalnız normal dosya/dizin; iki sıfır son blok gerekir |
| [fs/yol](fs/yol.kem) | Mutlak yol normalleştirme | `yol_bol`, `yol_birlestir` | Çift derleyici | Bağımsız yol örnekleri | Bileşen en çok 255 UTF-8 baytı; kök üstü .. kökte kalır |
| [girdi/halka](girdi/halka.kem) | Sabit kapasite FIFO olay tamponu | `Halka`, `halka_yeni`, `it`, `al`, `bos_mu`, `sayi` | Çift derleyici | Python collections.deque 120 işlem modeli | 0..1048576 eleman; dolunca en yeni atılır; kayıp sayacı tam64te doyar |
| [girdi/imlec](girdi/imlec.kem) | Fare hareketi ve düğme kenarları | `DugmeOlayi`, `Imlec`, `imlec_yeni`, `imlec_olay` | Çift derleyici | Python kırpma/bit kenarı vektörleri | Hız 0..1024; ekran y aşağı; tekerlek tam32 sınırlarında doyar |
| [girdi/satir](girdi/satir.kem) | Unicode satır düzenleme ve geçmiş | `SatirDuzenleyici`, `satir_yeni`, `satir_metni`, `satir_olay`, `satir_karakter`, `satir_tus` | Çift derleyici | Python Unicode kod noktaları ve E1 scancode akışı | 256 kod noktası, 16 geçmiş; ardışık tekrar/boş satır geçmişe eklenmez |
| [ortak/bayt](ortak/bayt.kem) | Little-endian bayt erişimi | `bayt_sinirda`, `oku_u8`, `oku_le16`, `oku_le32`, `oku_le64`, `imza_esit`, `bayt_toplam8`, `bayt_sifir`, `yaz_u8`, `yaz_le16`, `yaz_le32`, `yaz_le64`, `yaz_ascii` | Çift derleyici | Python int.to_bytes | Ham oku/yaz çağrısından önce aralık denetimi çağırana ait |
| [ortak/utf8](ortak/utf8.kem) | UTF-8 ve UTF-16LE dönüşümü | `utf8_bayt_coz`, `utf8_coz`, `utf8_kodla`, `utf16le_coz`, `utf16le_kodla` | Çift derleyici | Python encode/decode, Unicode vektörleri | Geçersiz UTF-8 bir bayt ilerleyerek U+FFFD; UTF-16 sıfırda biter |
| [pci/yapilandirma](pci/yapilandirma.kem) | PCI tip-0 yapılandırma ve BAR çözümü | `PciBar`, `PciAygit`, `pci_coz`, `bar_boyu`, `bar_boyu64`, `pci_sinif_adi` | Çift derleyici | Python struct ve maske aritmetiği | 256 bayt; yalnız tip-0; 64 bit BAR sonraki yuvayı tüketir |
| [ps2/fare](ps2/fare.kem) | PS/2 üç/dört bayt fare paketleri | `FareDurumu`, `FareOlayi`, `fare_yeni`, `fare_bayt` | Çift derleyici | PS/2 bit alanları, işaretli hareket vektörleri | Taşan paket atılır; dy yukarı pozitiftir |
| [ps2/klavye_tr](ps2/klavye_tr.kem) | Set-1 Türkçe Q klavye durumu | `KlavyeDurumu`, `TusOlayi`, `OZEL_ENTER`, `OZEL_GERI`, `OZEL_TAB`, `OZEL_ESC`, `OZEL_YUKARI`, `OZEL_ASAGI`, `OZEL_SOL`, `OZEL_SAG`, `OZEL_SIL`, `klavye_yeni`, `klavye_bayt` | Çift derleyici | A1 bağımsız 49 tuş tablosu | E0 ve Shift/Caps; donanım I/O yok |
| [sim/kabuk](sim/kabuk.kem) | Bellek diskinde ls/cat/echo komutları | `kabuk_calistir` | Çift derleyici | E1 gerçek scancode → FAT32 → konsol → PSF piksel zinciri | Host süreç/dosya erişimi yok; cat sonuna LF; komut yankısı yok |
| [virtio/constants](virtio/constants.kem) | VirtIO durum bitleri ve kayıt ofsetleri | `VIRTIO_STATUS_RESET`, `VIRTIO_STATUS_ACKNOWLEDGE`, `VIRTIO_STATUS_DRIVER`, `VIRTIO_STATUS_DRIVER_OK`, `VIRTIO_STATUS_FEATURES_OK`, `VIRTIO_STATUS_FAILED`, `VIRTIO_MMIO_MAGIC_VALUE`, `VIRTIO_MMIO_VERSION`, `VIRTIO_MMIO_DEVICE_ID`, `VIRTIO_MMIO_VENDOR_ID`, `VIRTIO_MMIO_DEVICE_FEATURES`, `VIRTIO_MMIO_DEVICE_FEATURES_SEL`, `VIRTIO_MMIO_DRIVER_FEATURES`, `VIRTIO_MMIO_DRIVER_FEATURES_SEL`, `VIRTIO_MMIO_QUEUE_SEL`, `VIRTIO_MMIO_QUEUE_NUM_MAX`, `VIRTIO_MMIO_QUEUE_NUM`, `VIRTIO_MMIO_QUEUE_READY`, `VIRTIO_MMIO_STATUS`, `VIRTIO_MMIO_QUEUE_DESC_LOW`, `VIRTIO_MMIO_QUEUE_DESC_HIGH`, `VIRTIO_MMIO_QUEUE_DRIVER_LOW`, `VIRTIO_MMIO_QUEUE_DRIVER_HIGH`, `VIRTIO_MMIO_QUEUE_DEVICE_LOW`, `VIRTIO_MMIO_QUEUE_DEVICE_HIGH`, `VIRTIO_MMIO_CONFIG_GENERATION`, `VIRTIO_BLK_CFG_TABAN`, `VIRTIO_BLK_CFG_CAPACITY`, `VIRTIO_BLK_CFG_CAPACITY_HIGH`, `VIRTIO_BLK_CFG_SIZE_MAX`, `VIRTIO_BLK_CFG_SEG_MAX`, `VIRTIO_BLK_CFG_BLK_SIZE` | Önceden mevcut | Bu dalgada yeniden doğrulanmadı | Mevcut donanım/mock akışı; bu dalganın kapsamı dışında |
| [virtio/features](virtio/features.kem) | VirtIO özellik pazarlığı maskeleri | `VIRTIO_BLK_F_BLK_SIZE`, `virtio_feature_kabul_maskesi`, `virtio_feature_negotiation_gecerli_mi` | Önceden mevcut | Bu dalgada yeniden doğrulanmadı | Mevcut donanım/mock akışı; bu dalganın kapsamı dışında |
| [virtio/mock_transport](virtio/mock_transport.kem) | VirtIO başlangıç için mock taşıma modeli | Genel API yok (modül içi model) | Önceden mevcut | Bu dalgada yeniden doğrulanmadı | Mevcut donanım/mock akışı; bu dalganın kapsamı dışında |
| [virtio/status](virtio/status.kem) | VirtIO durum geçişleri | `virtio_status_ekle`, `virtio_status_surucu_degeri`, `virtio_status_ozellik_tamam_degeri`, `virtio_status_surucu_tamam_degeri`, `virtio_status_gecis_gecerli_mi` | Önceden mevcut | Bu dalgada yeniden doğrulanmadı | Mevcut donanım/mock akışı; bu dalganın kapsamı dışında |
| [virtio/virtio_blk](virtio/virtio_blk.kem) | VirtIO-blk başlangıç sırası | `BaslatHatasi`, `BlkAygit`, `blk_baslat_cekirdek`, `blk_baslat` | Önceden mevcut | Bu dalgada yeniden doğrulanmadı | Mevcut donanım/mock akışı; bu dalganın kapsamı dışında |
| [virtio/virtio_blk_config](virtio/virtio_blk_config.kem) | VirtIO-blk yapılandırma okuma | `YapilandirmaHatasi`, `BlkYapilandirma`, `blk_cfg_adres`, `blk_cfg_u64_birlestir`, `blk_yapilandirma_oku_cekirdek`, `blk_yapilandirma_oku` | Önceden mevcut | Bu dalgada yeniden doğrulanmadı | Mevcut donanım/mock akışı; bu dalganın kapsamı dışında |
| [virtio/virtio_blk_oku](virtio/virtio_blk_oku.kem) | VirtIO-blk tek sektör okuma | `IOHatasi`, `Tampon512`, `blk_oku_cekirdek`, `blk_oku` | Önceden mevcut | Bu dalgada yeniden doğrulanmadı | Mevcut donanım/mock akışı; bu dalganın kapsamı dışında |
| [virtio/virtio_mmio](virtio/virtio_mmio.kem) | VirtIO MMIO v2 taşıma | `VIRTIO_MMIO_TABAN`, `VIRTIO_MMIO_MAGIC_BEKLENEN`, `VIRTIO_MMIO_VERSION_MODERN`, `VIRTIO_MMIO_DEVICE_ID_BLOCK`, `VIRTIO_MMIO_FEATURE_SEL_LOW`, `VIRTIO_MMIO_DESC_TEST_ADRES`, `VIRTIO_MMIO_AVAIL_TEST_ADRES`, `VIRTIO_MMIO_USED_TEST_ADRES`, `VIRTIO_MMIO_HATA_MAGIC`, `VIRTIO_MMIO_HATA_VERSION`, `VIRTIO_MMIO_HATA_DEVICE`, `VIRTIO_MMIO_HATA_FEATURE`, `VIRTIO_MMIO_HATA_QUEUE`, `VirtioMmioProbeSonuc`, `VirtioMmioInitDurum`, `virtio_mmio_adres`, `virtio_mmio_magic_gecerli_mi`, `virtio_mmio_features_ok_set_mi`, `virtio_mmio_probe`, `virtio_mmio_status_oku`, `virtio_mmio_status_yaz`, `virtio_mmio_transport_init_durum` | Önceden mevcut | Bu dalgada yeniden doğrulanmadı | Mevcut donanım/mock akışı; bu dalganın kapsamı dışında |
| [virtio/virtqueue_bind](virtio/virtqueue_bind.kem) | Split virtqueue kayıt bağlama | `BagHatasi`, `Virtqueue`, `virtqueue_bagla` | Önceden mevcut | Bu dalgada yeniden doğrulanmadı | Mevcut donanım/mock akışı; bu dalganın kapsamı dışında |
| [virtio/virtqueue_layout](virtio/virtqueue_layout.kem) | Virtqueue boyut/yerleşim hesabı | `virtqueue_layout_gecerli_mi` | Önceden mevcut | Bu dalgada yeniden doğrulanmadı | Mevcut donanım/mock akışı; bu dalganın kapsamı dışında |

## Sözleşmeler ve doğrulama sınırları

- Dizi taşıyan çerçeve yapıları değer olarak geçirilir; içteki dizi paylaşıldığı için çizim çağrısı tamponu günceller. Ölçüler constructor sonucundan alınmalıdır.
- FAT32 kurucusu gerçek FAT32 küme eşiğini sağlar. Dönen dizi tam disk boyu değildir: sıfır kuyruğu atılmış bellek önekidir. Dış kahin aracı tam seyrek disk dosyasını yeniden kurar.
- `bash tools/codex/fat32_kahin.sh` fsck.fat ile imajı denetler, mkfs.fat ile ters yön örneğini oluşturur. `bash tools/codex/fat32_coklu_kahin.sh` iki alt dizinde 111 canlı dosyayı ve ~100+ kısa ad çakışmasını denetler. `bash tools/codex/fat32_onar_kahin.sh` KEMGU sahipsiz küme listesini fsck kurtarma dosyalarının küme zincirleriyle karşılaştırır. Bu oturumda mcopy/mdir bulunmadığından o karşılaştırmalar atlandı; fsck.fat ve mkfs.fat çalıştırıldı.
- E2, 13 ayrıştırıcıda 3900 rastgele örnek ve 5100 tek-bayt mutasyonu ile sıfır/tek-bayt sınırlarını dener. Bu deterministik örnekler kapsamlı ispat değildir; her koşumun 30 saniye sınırı vardır.
- UTF-8 hataları tek bayt ilerleme ve U+FFFD ile ele alınır. Türkçe dosya adları ve emoji için ortak UTF-16/UTF-8 çekirdeği kullanılır.

## Derleyici bulguları

Ayrıntılı yeniden üretimler, C/self çıktıları ve dolanmalar [codex_bulgular.md](../otomasyon/chatgpt/codex_bulgular.md) içinde; derleyici kodu değiştirilmedi.

| Kimlik | Bulgu | Kullanılan dolanma |
|---|---|---|
| C001 | Modül ve işlev aynı adlı seçili import çakışması | Modülü alıp nitelikli çağırma |
| C002 | dizi_olustur yapı elemanı genişliği | Açık tipli boş dizi literali |
| C003 | Eşleş bağındaki yapı dizisinin eleman tipi | Açık tipli yerel değişken |
| C004 | İç içe bayt kopyasında işaret genişletme | oku_u8 ile tam32 üzerinden yazma |
| C005 | Yapıdan sonraki tam64 sabit argümanın genişliği | Açık olarak tam64 |
| C006 | İçe aktarılan yapıda boş dizi alanı | Önce tipli dizi değişkeni kurma |
| C007 | Geçişli içe aktarımdaki yapı tipleri | Çağıranda gereken tipleri açık import |
| C008 | Sonuç döndüren eşleş sonrası geçersiz LLVM varsayılanı | Açık erişilemez hata dönüşü |
| C009 | dosya_yaz/dosya_sil/dizin_olustur host yerleşiğine bağlanması | imaja_/imajdan_/imajda_ güvenli adları |
| C010 | İçe aktarılan enum kurucusunda self T016 | Kurucuyu modül içi sarmalayıcıdan çağırma |
| C011 | tam32 minimumunun yapı alanına atanması | Açık olarak tam32 |

S001: FAT etiketinin BPB ile kök dizin metadata girdisinde eşleşmesi gerektiği fsck.fat ile yakalandı ve düzeltildi. [İlerleme kaydı](../otomasyon/chatgpt/codex_ilerleme.md) her işin sabotajını ve kabul kapısını içerir.
