# Dalga 10 bulguları

## A — gerçek OVMF ölçümü

- D-752 tabanı `98f8e13`. Görevde belirtilen kaynaklar ve devralınan kurallar okundu.
- İlk 1 MiB ELF yükleme denemesi kod/sabit segmentlerini ayırdı, 28 MiB heap içeren veri segmenti `AllocateAddress` ile EFI_NOT_FOUND (0x800000000000000e) döndürdü.
- Firmware haritasında 0x800000..0x900000 arası ACPI NVS parçaları, 0x900000..0x1780000 BootServicesData; 0x1780000..0xbb75000 ConventionalMemory ölçüldü (`a-adres.log`).
- VARSAYIM: yalnız yeni UEFI linker betiği 32 MiB yükleme kullanır (Python `32*1024*1024=33554432`). PVH linker ve giriş talimatları değişmez. UEFI bütün PT_LOAD/heap sayfalarını AllocateAddress ile ayırır; hata açıkça durdurur.
- Firmware alan kaynağı: [UEFI 2.10 §4](https://uefi.org/specs/UEFI/2.10/04_EFI_System_Table.html), [§7](https://uefi.org/specs/UEFI/2.10/07_Services_Boot_Services.html), EDK2 MdePkg UefiSpec/GraphicsOutput/SimpleFileSystem/LoadedImage/SimpleTextOut başlıkları. Başlık kopyaları yalnız build/codex10 içindedir.

## Açılış bilgisi ABI v1

Little-endian, her alan u64, toplam 192 bayt; yapı 4 KiB LoaderData sayfasındadır.

| İndeks | Bayt ofseti | Alan |
|---|---|---|
| 0 | 0 | magic KEMUEFI1 |
| 1 | 8 | yapı boyu 192 |
| 2 | 16 | sürüm 1 |
| 3 | 24 | GOP framebuffer tabanı |
| 4 | 32 | GOP framebuffer boyu |
| 5 | 40 | genişlik piksel |
| 6 | 48 | yükseklik piksel |
| 7 | 56 | satır adımı BAYT |
| 8 | 64 | GOP piksel biçimi |
| 9 | 72 | EFI harita adresi |
| 10 | 80 | harita kullanılan bayt |
| 11 | 88 | descriptor adımı |
| 12 | 96 | descriptor sürümü |
| 13 | 104 | RSDP adresi |
| 14 | 112 | çekirdek ilk sayfa |
| 15 | 120 | çekirdek son sayfa (dışlayıcı) |
| 16 | 128 | harita kapasitesi |
| 17 | 136 | ExitBootServices deneme sayısı |
| 18 | 144 | bulunan GUID alt 8 bayt |
| 19 | 152 | bulunan GUID üst 8 bayt |
| 20 | 160 | EFI stub tabanı |
| 21 | 168 | EFI stub sonu (dışlayıcı) |
| 22 | 176 | ayrılmış (0) |
| 23 | 184 | ayrılmış (0) |

OVMF ölçümü: GOP 1280×800, adım 5120, biçim 1 (BGRX), taban 0x80000000. UEFI girişinde GOP sayfaları ayrıca UC yapılır. Descriptor adımı 48, sürümü 1; türler 0,1,2,3,4,5,6,7,9,10,11. RSDP GUID `8868e871-e4f1-11d3-bc22-0080c73c8881`; bu GUID arama koşulu değildir, bulunan yapılandırma girdisinden ölçüldü.

## A doğrulaması

- Bilgi yapısındaki descriptor adımı +8 sabotajı: derleme başarılı, gerçek OVMF 1/1 kırmızı (`UEFI harita`, QEMU rc=3; make rc=2). Magic korumasını kaldırma: saf test rc=1. İkisi geri alındı.
- Eski ExitBootServices anahtarı: ilk çağrı EFI_INVALID_PARAMETER; yeniden alınan haritayla ikinci çağrı başarılı. Fiziksel ABI alanı 17 ve seri çıktı birlikte 2 deneme gösterdi. Deney değişikliği geri alındı.
- Eksik ELF, yanlış sınıf, yürütülebilir segment dışında giriş, geçersiz LOAD boyutu ayrı hata mesajlarıyla reddediliyor; çekirdeğe geçilmiyor.
- Seri kapı zinciri rc=0: UEFI 5/5, PVH kabuk 15/15, ekran 5/5, panik 15/15, kem_os_x86, sürücü köprüsü, check ve baremetal_diff 23/23 (0 atlama).

## B — GOP ve ortak çizici

- OVMF kipine dokunulmaz; `ekran_gop_ac` ölçülen BGRX biçimi, fiziksel aralık, boyut ve bayt adımını doğrular. `Ekran` ve bütün çizim işlevleri BGA yoluyla ortaktır.
- QMP screendump boyutu bağımsız Python çiziminin ölçüsüdür: 1280×800, 1.024.000 piksel birebir. Sabotaj adım+4: 1/1 açılış kırmızı, 2.324 farklı piksel; biçim korumasını kaldırma: saf test exit=2. İkisi geri alındı.
- Saf test dolgu adımını (1280 piksele karşı 5376 bayt), RGB/BLT biçimlerini, kısa tamponu, adres taşmasını, hizayı, boş/aşırı boyutu ve kısa yapıyı sınar. Dizi<tam64> fikstürünün literal eklemelerinde açık tam64 dönüşümü gerekir; karttaki örtük genişletme yasağına uyuldu.
