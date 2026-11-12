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
- Saf test dolgu adımını (1280 piksele karşı 5376 bayt), RGB/BLT biçimlerini, kısa tamponu, adres taşmasını, hizayı, boş/aşırı boyutu ve kısa yapıyı sınar. Dizi<tam64> fikstürünün literal yazmalarında açık tam64 dönüşümü gerekir; karttaki örtük genişletme yasağına uyuldu.

## C — firmware RAM haritası ve E4o

- Kaynak: [UEFI 2.10 §7.4.6 ExitBootServices](https://uefi.org/specs/UEFI/2.10/07_Services_Boot_Services.html). EBS sonrası 3 (BootServicesCode), 4 (BootServicesData), 7 (ConventionalMemory) kullanılabilir; runtime niteliği taşıyan, Loader, ACPI, MMIO ve diğer türler korunur. Çekirdek/heap, bilgi sayfası, harita kapasitesi, framebuffer, RSDP ve stub aralıkları ayrıca korunur.
- İlk gerçek tahsisler (256M): ayna 1048576/65536, panik 1114112/32768, DMA 63127552/33615872, VirtIO 1146880/131072. ELF büyüyünce DMA adresi değişir; sabit beklenti değildir. DMA içinde xHCI 64 KiB hizalıdır; disk tabanı 16 MiB sonradır.
- İlk UEFI kabuk denemesi sayfa hatası verdi: adres `0xc000004000`. OVMF NVMe/xHCI 64-bit BAR'ları 4 GiB üstüne koyuyor; PVH'nin düşük PCI adres varsayımı burada geçersiz. Yeni runtime, aynı çerçeve ayırıcısından sayfa tabloları ayırıp bu aygıtların kullandığı 16 KiB pencereleri UC 2 MiB sayfalarla eşler. Var olan CR3 okuma/yazma ilkelleri kullanılır; yeni asm eklenmez. Python sayfa tablolarını QMP ile bağımsız yürüyerek fiziksel adresi ve UC bitlerini denetler.
- Yeni girişin 2–4 GiB alanı UC'dir: OVMF'nin düşük PCI BAR'ları 2 GiB'den başlayabilir. İlk 2 GiB RAM WB, GOP'un düşük adrese gelmesi durumunda kendi aralığı ayrıca UC. PVH girişinin kaynak öneki birebir korunur.

- C sabotajları: `uefi_koru` ayrılmış türünü 2→1 yapmak fiziksel işareti ezer; gerçek QEMU 1/1 kırmızı (`ayrılmış sayfa ezildi`). Runtime attribute korumasını kaldırmak saf test exit=1. İkisi derlenir; geri alındı.
- C kabuk ölçümü için EFI dosyaları firmware'in sanal FAT aygıtındadır, kabuk verisi ayrı bağımsız GPT/FAT32 NVMe disktedir. Böylece RAM ayırıcısı ve kabuk E aşamasındaki tek USB imajından önce sınanır. E kapısı aynı ESP imajını hem açılış hem kabuk verisi için kullanacaktır.

## C10-001 — self-host, tam64 diziye türsüz literal yazma

Komut: `bash otomasyon/chatgpt/dogrula.sh build/codex10/literal_probe.kem`.
C check/çalıştır geçer (exit=0), self check geçer fakat çalıştır exit=1. Minimal tekrar:

```kem
işlev main() -> tam32 {
 değişken a: Dizi<tam64> = [];
 değişken i: tam32 = 0;
 iken i < 24 { dizi_ekle(a,0); i=i+1; }
 dizi_yaz(a,8,1);
 eğer dizi_al(a,8) != 1 { ver 1; }
 ver 0;
}
```

Yalnız `dizi_ekle` sıfırını tam64 yapmak farkı düzeltmez (self exit=1).
Yalnız `dizi_yaz` değerini `1 olarak tam64` yapmak iki derleyiciyi eşitler.
GOP/harita fikstürleri açık tam64 yazma kullanır. Mevcut ortak DMA okuyucusu dinamik
tam64 değer yazdığından bu bulgudan etkilenmez; değişmedi. Derleyiciye dokunulmadı.

## C10-002 — self-host döngüde yapı okumasının yığın biriktirmesi

OVMF self kabuk 16 KiB yığınla normalleştirme çağrısında durdu. Geçici 1 MiB yığınla
aynı ikili yedi kabuk komutunu ve piksel kâhinini geçti. LLVM çıktısında
`harita_duzenle` iç döngüsünde `alloca %BellekBolgesi` / `kdl_dizi_al_yapi` var;
geçici yapı her iterasyonda yığında kalıyor. C aynı QEMU kapısını geçiyor.

Dolanma: kapsama ve tür okuması küçük yardımcı çağrılara alındı; geçici yapı ömrü
bir karşılaştırmayla sınırlanır. Harita normalleştirme kuralı/beklentileri değişmez.
VARSAYIM: UEFI başlangıç yığını 512 KiB (Python `512*1024=524288`, `0x80000`); çok sayıda
firmware girdisi için ayrılır, ELF memsz ve RAM korumasına dahildir. Heap 28 MiB ve
PVH linker/yığını değişmez. Yeni 800 bölgeli test C ve self ile geçer; eski iç döngü
geri konularak sabotaj ayrıca ölçülür. Derleyici/runtime C değişmedi.

C10-002 minimal tekrar (`dogrula.sh build/codex10/yigin_probe.kem`):

```kem
yapı Hucre { a: tam64; b: tam64; c: tam32; }
işlev main() -> tam32 {
 değişken xs: Dizi<Hucre> = [];
 dizi_ekle(xs,Hucre { a:7, b:11, c:13 });
 değişken i: tam32 = 0; değişken toplam: tam64 = 0;
 iken i < 400000 {
  değişken h: Hucre = dizi_al(xs,0); toplam=toplam+h.a; i=i+1;
 }
 eğer toplam != 2800000 { ver 1; }
 ver 0;
}
```

800 bölgeli testin eski kod sabotajı: C geçer, self exit=139 (stack/segmentation fault); geri alındı.

## D — ACPI bağımsız RAM kâhini

OVMF RAM dökümü, çekirdeğin RSDP adresini kullanmadan ACPI bellek bölgelerinde
`RSD PTR ` imzasını bayt bayt arar; iki sağlama toplamını, kökü ve bütün çocuk
SDT'leri Python ile ayrıştırır. 256M ölçümü: RSDT RSDP `0xf77e000`, XSDT RSDP
`0xf77e014` (ikinci adres 16 bayta hizalı değildir). Çekirdekle eşleşen sıra:
FACP (244), APIC (120), HPET (56), MCFG (60), WAET (40), BGRT (56 bayt).

- Normal seçim XSDT; gerçek firmware'de revizyon 2 girdisini geçici atlayan
  deneme RSDT geri düşmesini de çalıştırdı ve aynı bağımsız listeyle geçti.
  OVMF'den ölçülen eski RSDP GUID'i `eb9d2d30-2d88-11d3-9a16-0090273fc14d`.
  Geçici stub değişikliği geri alındı.
- Dokuz yeni saf şekil: ayırt edici XSDT/RSDT adresleri, yanlış imza/boy/sağlama
  ve hizasız girdi uzunlukları. C/self `dogrula.sh` yeşil.
- `sdt_coz` sağlama denetimi geçici kaldırıldı: saf test exit=1;
  gerçek firmware kökünün sağlama baytı bozulmuş kopyası kabul edildiği için
  QEMU 1/1 kırmızı. Sabotaj geri alındı. Loglar `build/codex10/d-sabotaj*`.
- Self-host UEFI kabuk 256M 1/1 geçti; bağımsız ACPI, seri ve GOP aynı.
