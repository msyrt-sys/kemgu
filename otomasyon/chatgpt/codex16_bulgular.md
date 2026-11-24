# Dalga 16 ölçümleri

D-766 tabanında `runtime/mimari/x86_64/kem_uart.kem` içindeki `KGUN_BOY` 8192 bayttır. ABI çağrı listesi 70 ile biter.

## Klavye

QEMU 8.2.2 üzerinde PS/2 klavyenin 8042 öz-testi 85, birinci ve ikinci port testleri 0 döndürdü. ABD düzeninde fiziksel I ve Shift+2 tuşları kabukta `i@` üretti.

PS/2’de Caps, Num, Scroll, Caps, Scroll, Num basışları için QEMU `ps2_set_ledstate` izi sırasıyla 4, 6, 7, 3, 2, 0 verdi. USB klavyenin USBmon PCAP kaydındaki SET_REPORT veri baytları aynı girişte 2, 3, 7, 5, 1, 0 oldu. `q35,i8042=off` ile FADT boot bayrağı 0, kabuk kaydı “8042 yok” ve USB klavyeyle girilen `echo usb` çıktısı “usb” oldu.

Taşma ölçümünde QMP, PS/2 aygıtına 1.600 adet tek baytlık A basışı gönderdi. Her sekiz basışta IRQ1 sayacıyla aygıt FIFO’sunun boşalması beklendi. Konuk 1.022 basış, 0 bırakma ve 578 kayıp okudu. Halkadaki bir başka bayt, ölçümü başlatan Enter bırakmasıydı. Sonuç seri hattaki `PS2_KAYIP 578 0 0` ile eşleşti.

| Gerçekte uygulanan değişiklik | Gözlem | QEMU kâhini |
|---|---|---:|
| Pause dizisinin dördüncü baytını yut | 581 basışı kayboldu. Ayrı 29 ve 69 bırakmaları geldi. Host testi de 1 döndü. | 1 |
| PS/2 Caps ve Scroll maskelerini değiştir | Konuk 1, 3, 7, 6, 2, 0 gönderdi. | 1 |
| Ctrl+C ön plan süreci yerine kabuk işlevinden çıksın | Seri kayıtta `KABUK SONLANDI` görüldü. Yeni istem gelmedi. | 1 |
| Dolu halkada kayıp sayacını artırma | 1.600 basışın 1.022’si okundu, sayaç 0 kaldı. | 1 |

Bölüm kapılarını ayrı `make` süreçleriyle paralel başlatmak, `selfhost/konak.kem` hedefinin geçici dosyasında yarışa neden oldu. Derleyici yeniden üretildi, teslim kaynağındaki regresyonlar tek `make -k -j6 --output-sync=target` çağrısında başlatıldı.

## PCI

QEMU 8.2.2 ve OVMF ile düz Q35, iki PCIe root-port, iç içe PCIe–PCI ve PCI–PCI köprüsü ve i440FX denendi. Her açılışta SATA ve NVMe bölümündeki dosyanın UTF-8 baytları host FAT okuyucusuyla eşleşti. Her iki köprü topolojisinde QMP 11 işlev, 17 BAR ve iki köprü gösterdi. MCFG bulunmayan i440FX’te CF8/CFC ile dokuz işlev ve 11 BAR bulundu.

MCFG kullanan konukta iki erişim yolunun kök aygıttan okuduğu 64 dword, QMP fiziksel bellek okumasıyla karşılaştırıldı. Her etkin bellek BAR’ının yaprağındaki PCD/PWT bitleri 1 idi. İç içe topolojide QEMU monitörü dış köprünün alt veri yolu yazmacını sıfırladı. Yeniden taramada konuk ikincil numarayı koruyup alt sınırı yeniden programladı. Pasif ROM’un yapılandırma yazmacında boy maskesi kalabildiği için raporlanan etkin taban, ROM enable biti kapalıyken sıfırdır.

| Üretim kodunun ayrı kopyasındaki sabotaj | Gerçek QEMU sonucu |
|---|---|
| ECAM işlev bitlerini bir fazla sola kaydır | `00:1f.2` ve `00:1f.3` konuk envanterinden eksildi. QMP karşılaştırması rc 1. |
| Sıfırlanmış köprü alt yol sınırını güncelleme | Konuk ilk taramada `01:00.0`, `02:00.0` ve `02:01.0` işlevlerini bulamadı. QMP karşılaştırması rc 1. |
| 64 bit BAR’ın üst yarısını at | Gerçek sürücü açılışta `0xc000004000` için sayfa hatası verdi. Kâhin kabuk istemine ulaşamadı, rc 1. |

C16-001. Host üzerinde `pci_port_adres(767,254) olarak tam64` değerini `tam_metin` ile yazdıran küçük birim için `build/kemgu --llvm` ve `build/codegen --llvm` çıktıları ayrı ayrı clang ve `build/kdl_runtime.o` ile bağlandı. Beklenen 2147680252; C 2147680252, self-host −2147287044 verdi. İkisinin düşük 32 biti aynı. Donanım sözcüğünü genişletirken alan maskesi uygulanıyor. Derleyiciye değişiklik yapılmadı.

Küçük tekrar üretimi aşağıdadır. Her derleyici için `<derleyici> --llvm port_tani.kem > port.ll` ve `clang -x ir port.ll -x none build/kdl_runtime.o -o port && ./port` kullanılır.

```kemgu
kullan runtime::mimari::x86_64::kem_pci_adres::{ pci_port_adres } olarak pa;
işlev main() -> tam32 {
    yazdir_metin(tam_metin(pci_port_adres(767,254) olarak tam64)); ver 0;
}
```

İlk PCI regresyonunda her iki kaynakta 68 LAPIC tikinde tekrar sayısı kabul aralığındaydı. Son dosya düzeniyle yük altında tekrarlanan koşumda self-host PS/2 konuk 7 karakter verdi (aralık 4–6). Önceki kâhin basmayı yollayıp en az iki tik onay bekledikten sonra 68 tik beklemeye başlıyor ve bırakmayı ayrı bir host çağrısıyla yolluyordu. Basılı kalınan süreyi QEMU’nun sanal saatinde 200 veya 680 ms programlayan `send-key` ile aynı GOP görüntüsü ve aynı eşikler korundu. İki derleyicide, PS/2 ve USB için dört senaryoda da 200 ms’de 1, 680 ms’de 5 karakter ölçüldü.

## Kesme çerçevesi ve MSI

Q35 üzerinde SATA dosyası okunurken AHCI vektörü 67’nin sayacı 2’den 5’e çıktı. NVMe dosyası okunurken CQ1’in MSI-X vektörü 66 için 4’ten 7’ye çıktı. Her iki derleyicide de ilgili PCI okuması sırasında IOAPIC 16–23 girişlerinde artış sıfırdı. Host ayrıca PCI MSI enable, INTx Disable, MSI mesaj verisi ile NVMe MSI-X tablo girişinin adresini, verisini ve maskesini doğrudan QEMU belleğinde sınadı.

QEMU 8.2.2 `ich9-ahci,help` bir MSI kapatma özelliği sunmadı. Yedek yol için ayrı çekirdekte yalnız AHCI yetenek aramasına “yok” sonucu verildi. Dosya içeriği host FAT okuyucusuyla eşleşti. Dinamik aralık tek işlemcide 14 MSI vektörü barındırır. Donanım raporu dosyası ve ekran farklı anlarda üretilir; değişken sayaçlar QMP ile alınan alt ve üst sınırlar arasında doğrulanır.

MSI veri alanına boş 79 yazılan kopyada `KABUK HATA ahci MSI zaman asimi; yoklama` görüldü. Kâhin dosya baytlarını yedek yoldan doğruladıktan sonra “MSI bekleniyordu” ile rc 1 verdi. Kayıt çakışmasını kabul eden kopyada `kesme sina` çıktısı `HATA kesme vektoru cakismasi` oldu, kapı rc 1 verdi.

## Disk hatası enjeksiyonu

Doğrudan blkdebug → file düzeninde `read_aio` geçici hatayı tetiklemedi. QEMU 8.2.2’nin açılış olayına `event="none"` ile bağlanan, `iotype="read"` ve hedef sektörle sınırlanan kural kullanıldı. Ayrı `qemu-io` ölçümünde sırasıyla EIO ve 512/512 bayt okuma görüldü. Konukta her senaryonun gerçek hata sayısı QMP `query-blockstats` ile alındı. [QEMU 8.2.2 blkdebug kaynağı](https://github.com/qemu/qemu/blob/v8.2.2/block/blkdebug.c)

| Taşıyıcı | Disk LBA | Geçici / kalıcı hata sayısı | Kalıcı hatada konuk durumu |
|---|---:|---|---|
| AHCI | 4103 | 1 / 4 | TFD 1089, PxIS 1073741824 |
| NVMe | 4103 | 1 / 4 | SC 129, SCT 2 |
| USB BOT | 135175 | 1 / 4 | SENSE 11, ASC 0, ASCQ 6 |

USB SCSI izinde her başarısız okuma başına bir REQUEST SENSE ölçüldü. İki derleyicide de geçici hata sonrası dosya baytları host FAT imajıyla eşleşti. Kalıcı hatadan sonra aynı bölümün başka dosyası okundu ve fsck temizdi. QEMU’daki AHCI EIO durumunda BSY ve DRQ kapalıydı. COMRESET gerektiren meşgul durumunun yazmaç sırası ve bir milisaniyelik beklemesi ayrı host testinde doğrulandı.

Sıfır yeniden denemede tek seferlik hatadan sonra SATA dosyası yerine LBA/TFD/PxIS hata satırı döndü. Port yeniden başlatma yazımını atlayan kopyada hem hatalı dosya (LBA 4103) hem de sonraki dosya için gereken sektör (4102) okunamadı. İki sabotaj koşumu rc 1 verdi.

## USB hub ağacı ve olayları

QEMU 8.2.2 üzerinde USB 2 kök portu 5, HMP fiziksel portu 1 ile eşleşti. Host bu dönüşümü denetleyicinin Supported Protocol alanından çıkardı. İkinci hub için rota 1, klavye, disk ve fare için sırasıyla 17, 33, 49 bulundu. Ek tabletin rotası 65 idi. QMP ile DCBAA üzerinden okunan çıkış bağlamlarının kök portu, hızıyla uyumlu TT alanı, rota ve yapılandırılmış yuva durumu denetlendi. QEMU'nun hub modeli full-speed çalıştığı için high-speed işlem çeviricisi fiziksel olarak sınanmadı. [QEMU hub](https://github.com/qemu/qemu/blob/v8.2.2/hw/usb/dev-hub.c) ve [xHCI](https://github.com/qemu/qemu/blob/v8.2.2/hw/usb/hcd-xhci.c)

Sürücü hub tanımlayıcısını iki istekte okur. Her portun gücünü açar, gücün kararlı olmasını bekler, portu sıfırlar ve değişim bitlerini onaylar. Yuva, klavye/fare ve BOT yönetim işlemleri aynı olay halkasını paylaşır. Sınırlar beş rota hanesi, hub başına 15 port, denetleyici başına 16 eşzamanlı aygıttır. Eski açık dosyaların yeniden kullanılan USB yuvasından okumasını engellemek için sökülen hub disklerinde ortam nesli kontrol edilir. Bir oturumdaki blok tanıtıcı geçmişi 64 ile sınırlıdır. Hub sürücüsü USB 2 sınıfı içindir.

Rota sabotajında konuk `KABUK HATA xhci tamamlanma kodu 5` verdi. Çıkarma sırasında C_PORT_CONNECTION onayını atlayan kopyada ilk çıkarma işlendi, ikinci takmada HMP 1.1.4 üzerinde hub gösterdi ve konuk aynı yolu listelemedi. İlgili iki süreç de rc 1 döndürdü.

## Fiziksel ramdisk ve geçici VFS

MBR kaynağı 36.700.160 bayt, SHA256 28c8e4fa7e98718e844741a287e097af9d52b0828717ba4516bae343089cd7ad. GPT kaynağı 36.717.056 bayt, SHA256 4c6552d38ad1db0ac02babeba3efb3858059d8aa9309c7ab22510f2924e94b32. Her ikisinde de host FAT32 bölümündeki dosya içeriğiyle konuk okuması eşleşti. Verinin tamamı bitişik fiziksel çerçevelerde tutulur. Konuk, kaynak FAT zincirindeki bitişik alanları en fazla 64 KiB'lık DMA istekleriyle aktarır. Denenen görüntülerde yazma ve geri okuma sonrasında bootstrap çekirdeğinin heap tüketimi NVMe ile 6.427.936, USB ile 7.411.680 bayttı. Bu değerler ramdisk için ayrılan fiziksel çerçeveleri içermez.

İmaj boyu sınırı 128 MiB. Sürücü RAM imajının bölüm tablosunu ve FAT bağını doğruladıktan sonra aygıtı görünür kılar. Geçici dosya sistemi 64 düğüm ve sekiz veri çerçevesiyle yapılandırıldı. Silinen düğümler ve dosya sayfaları tekrar kullanılabilir. İmaj yokluğunda konuk “ramdisk yok” kaydı verdi. Fiziksel PC'de ölçüm yapılmadı.

## ACPI S5

Q35'te QMP fiziksel bellek dökümündeki 8.424 baytlık DSDT'nin adı kök altında _S5_, paket uzunluğu 6 ve ilk iki değeri sıfırdı. PM1a_CNT portu 0x604, PM1b yoktu. QMP olayının reason alanı guest-shutdown, guest alanı true ve süreç dönüşü 0 oldu. Kapatma kapısında debug-exit aygıtı bağlı değildir. tests/drivers/acpi/qemu_q35_dsdt.hex aynı tablonun onaltılık dökümüdür.

Ayrıştırıcı geçerli SDT imzası, uzunluk, sağlama toplamı, NameOp ve PackageOp kalıbını arar. Sıfır, bir, BytePrefix ve WordPrefix sabitlerini destekler. Bu sınırın dışında veya PM1 adres uzayı desteklenmediğinde açık hata kaydı üretilir. I/O portlu PM1b varsa ikinci uyku tipi o porta yazılır. Q35 yalnız PM1a ile ölçüldü. SLP_EN sabotajında konuk geri dönüş yolunu bildirdi ve QEMU açık kaldı. Yalnız kaydırmayı değiştiren ikinci kopyada tip 5 için beklenen 0x3401 sözcüğünü denetleyen test başarısız oldu. [QEMU ACPI PM1 uygulaması](https://github.com/qemu/qemu/blob/v8.2.2/hw/acpi/core.c)

## GOP bellek öznitelikleri

IA32_PAT için ölçülen değişim 0x0007040600070406 → 0x0107040600070406. Giriş 7 WC, giriş 3 UC, giriş 0 WB. Etkin CR3 ve QMP fiziksel sayfa tablolarından çözülen küçük GOP yaprakları PAT/PCD/PWT için 1/1/1 verdi. Host ilk ve son GOP sayfasıyla beraber tüm aralığı ve BAR'ların dışarıda kalan sayfalarını denetledi. Sabotaj kopyasında ilk sayfa 0x80000000, PTE 0x800000fb ve PAT indeksi 7 olduğu halde tip UC çıktı.

Aşağıdaki süreler 1.024.000 pikselin doldurulması için TSC çevrimleridir. QEMU TCG süresi fiziksel performansı göstermez. Fiziksel PC'de üç soğuk açılışla ölçüm adımı kontrol listesine eklendi.
PAT_TSC UC=4575064 WC=4432823 piksel=1024000
PAT_TSC UC=4658138 WC=4438295 piksel=1024000

## Ayrıştırıcı örnekleri

Tohumlar sırasıyla 160000–160008. Korpuslar geçerli taban, rastgele akış, kesilmiş önek, tek bayt mutasyonu ve türe özgü alan değişiklikleri içeriyor. Tüm çalıştırmalar zaman sınırlıdır. USB 2.400 girdide, diğer satırlar 1.200 girdide ölçüldü. PS/2 için olay üreten akış sayısı, diğerlerinde kabul edilen girdi sayısı aşağıdadır. Kalan girdiler eksik olay veya açıklamalı ret verdi. Bootstrap ve self-host tutanakları aynıdır.

ps2-klavye kabul=1118
ps2-fare kabul=1079
usb kabul=974
hid kabul=598
hub kabul=599
xhci kabul=378
mcfg kabul=528
s5 kabul=449
lfn kabul=812

## Güncel bekleme envanteri

Dalga 15'in [donanım bekleme envanterindeki](codex15_bulgular.md) satırlara aşağıdaki sonuçlar uygulanır. C sürücülerindeki kayıp sayacı her düşen TX baytı için bir artar.

| Yol | Beklenen olay | Güncel sınır |
|---|---|---|
| 16550 C TX ve PL011 C TX | THRE ve boş TX FIFO | Bayt başına 100.000 durum okuması |
| 16550 C RX ve PL011 C RX | Kullanıcının karakter göndermesi | Bloklayan alma API'sinin sözleşmesi gereği açık uçlu |
| AHCI ve NVMe MSI | Kesme sayacı değişimi | 50 ms veya beş milyon turdan önce tamamlanma, yoksa yoklamaya dönüş |
| AHCI, NVMe ve USB disk işlemi | Geçici ortam hatası | İlk girişim ve en çok üç yeniden deneme |
| Hub portu | Bağlantı değişimi ve port sıfırlaması | Sayılı durum yoklamaları, güç gecikmesi tanımlayıcıdan |
| ACPI S5 | Sistemin gücü kesmesi | Yazma sonrasında 250 ms, sonra hata ve eski durdurma yolu |
| Bekleyen kabuk, süreç ve son HLT | Kullanıcı olayı, süreç çıkışı veya son durum | Sürücü durum biti beklemesi içermeyen açık uçlu durumlar |

MMIO ölçümü VirtIO blok/TX/RX used.idx, SATA port durumu, kesme sayacı, xHCI olay halkası, USB kontrol ve blok/yapılandırma durum makinelerini kapsar. PIO ile çalışan 8042, UART ve RTC, x86 in talimatını kullanır. Kâhin ardıllardan yeniden erişilebilen döngü bloklarını seçer. Bölünmüş yürütücüde aynı döngü içinden donanım okuyan yordamın çağrıldığı da doğrulanır. Gerçek çalışma zamanının iki UART dosyasında RX gövdeleri korunmuştur.
