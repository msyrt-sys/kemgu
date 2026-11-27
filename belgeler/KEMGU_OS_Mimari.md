# KEMGU OS x86_64 mimarisi

## Firmware devri

UEFI, ESP içindeki `EFI/BOOT/BOOTX64.EFI` dosyasını çalıştırır. Köprü GOP kipi, ACPI kökü, yükleyicinin adresleri, açılış bölümünün aygıt düğümü ve son bellek haritasını çekirdeğe verir. Çekirdek fiziksel haritada firmware, aygıt, görüntü, framebuffer ve bilgi yapısını korur. Çerçeve ayırıcısı kullanılabilir düşük belleği yönetir. OVMF yükleyiciyi 4 GiB üzerine yerleştirdiğinde bu aralık ayırıcının dışındadır. [EFI köprüsü](../boot/efi/stub.c) · [Harita](../drivers/acilis/uefi_harita.kem) · [Fiziksel tahsis](../runtime/mimari/x86_64/kem_bellek_harita.kem)

ACPI tablolarından MADT kesme yolları ve FADT güç bilgisi okunur. PCI taraması depolama denetleyicilerini ve xHCI'yi tanır. Kabuk USB ve PS/2 girişini seçer, disk bölümlerini tanımlayıp UEFI kimliğiyle açılış kökünü eşler. Seri kayıttaki `ACILIS_TSC` değerleri aynı başlangıca göredir. `donanim` aygıt dökümünü, `log` çekirdek mesajlarını gösterir. [ACPI](../runtime/mimari/x86_64/kem_acpi_uefi.kem) · [Kabuk](../test/ornekler/kem_os_x86_kabuk.kem)

## Sürücü ve dosya katmanları

`drivers/` altındaki durum makineleri gelen baytları veya tamamlanmış isteği yorumlar. Çekirdekteki yürütücü port, MMIO ve DMA işlemlerini uygular. xHCI olay halkasındaki HID ve USB depolama tamamlamaları aynı tüketiciden geçer. Blok katmanı mantıksal sektörlerle çalışır. GPT/MBR kimliği ile FAT32 başlangıç sektörü belirlendikten sonra kabuk ve kullanıcı fd çağrıları aynı FAT32 okuyucu ve yazma planını kullanır. ESP salt okunurdur. Kullanıcının yazılabilir bağladığı veri bölümünde FAT kopyaları ve dizin kayıtları eşitlenir. [USB sürücüleri](../drivers/usb) · [Blok yürütücüsü](../test/ornekler/kem_os_x86_blok_ortak.kem) · [Dosya çağrıları](../test/ornekler/kem_os_x86_vfs_ortak.kem)

### Yeni sürücü ekleme

Aygıtın kayıtlarını ve durum geçişlerini `drivers/` altında tanımlayın. Durum makinesine son G/Ç yanıtını verin. Bir sonraki isteği, tamamlanmayı veya açıklamalı hatayı döndürsün. Gerçek port, MMIO ya da DMA erişimini x86 yürütücüsüne bağlayın. Örneğin AHCI durum makinesini hem sahte aygıt hem de gerçek QEMU modeli aynı girişten çalıştırır. Her beklemeye aygıt durumu için üst sınır koyun. [AHCI durum makinesi](../drivers/ahci/cekirdek.kem) · [G/Ç yürütücüsü](../runtime/mimari/x86_64/kem_gc_yurut.kem)

Saf testleri `tests/drivers/` altına ekleyin. Yeni test dizinini Makefile'daki `CHATGPT_TESTLER` listesine dahil edin. `make calistir_chatgpt_kopru` ile iki derleyici sonucunu karşılaştırın. QEMU aygıtı için gerçek G/Ç senaryosu ekleyin. Disk yazan bir sürücüde konuk kapandıktan sonra host üzerinden yazılan ve korunması gereken baytları denetleyin. [AHCI host testleri](../tests/drivers/ahci) · [QEMU koşumu](../test/bm_x86_ahci_harness.sh)

## Kesme ve süreçler

IDT ve TSS denetimi çekirdek girişinde kurulur. IOAPIC, PS/2 IRQ1 ve IRQ12'yi hedef vektöre yönlendirir. Gerçek zamanlı kesme işleyicisi gelen baytı halkaya ekler. Tuş ve fare paketleri tüketici bağlamında çözülür. xHCI MSI-X vektörü 65 kullanır. PIT ile kalibre edilen LAPIC dilimi 10 ms'dir. Kullanıcı `int 0x80` ile hizmet ister. Her süreç kendi CR3 kökü, kullanıcı yığını ve ELF sayfalarıyla çalışır. Uyku, girdi, çocuk veya boru bekleyen süreç hazır kuyruğuna koşulu gerçekleştiğinde döner. Son çalışan ön plan süreci ekranı ve klavyeyi kullanır. Çıkışta fd uçları ve fiziksel çerçeveler bırakılır. Ebeveyn sonuç kaydını toplar. [Giriş koçanları](../boot/start_x86_64.S) · [APIC](../runtime/mimari/x86_64/kem_apic_hw.kem) · [Süreç adres alanı](../runtime/mimari/x86_64/kem_surec_hw.kem)

BSP, MADT listesindeki etkin işlemciler için düşük bellekte bir trampolin kurar. Her AP kendi GS kaydı, GDT, TSS, IST yığınları ve LAPIC zamanlayıcısıyla çalışır. IDT ortaktır. Hazır süreç yerel kuyruktan seçilir. Yakınlığı uygun bir süreç başka bir kuyruğa geçebilir. Kullanıcı kodu işlemcilerde eşzamanlı yürür. Aygıt işlemlerini ve sistem çağrısı hizmetlerini BSP sıraya alır. Kabukta `islemci` ile her işlemcinin APIC kimliğini, yerel tıkını, PID sayısını ve boşta yüzdesini okuyabilirsiniz. [SMP mimarisi ve ölçümleri](KEMGU_OS_SMP.md)

## PCI, bellek ve güç

MCFG bir kesim 0 penceresi verirse çekirdek ECAM kullanır. MCFG bulunmayan makinede CF8/CFC ilk 256 baytı sağlar. Köprülerin ikincil ve alt veri yolları izlenir. BAR boyutu ölçülürken I/O ve bellek çözme geçici olarak kapatılır, 64 bitlik BAR iki yarısıyla geri yüklenir. Aygıt MMIO sayfaları UC olarak eşlenir. GOP için IA32_PAT girdisi 7 WC yapılır. Yalnız framebuffer içindeki 4 KiB yapraklarda PAT, PCD ve PWT birlikte kurulur.

`kesme_kaydet` vektörü bir işleyici kimliğine bağlar. `kesme_ayir` boş vektörü ayırır, `kesme_birak` kaydı kaldırır. IRQ1, IRQ12, LAPIC ve xHCI sabit vektörlerle aynı sayaç tablosundadır. AHCI MSI ve NVMe tamamlanma kuyruğu 1 MSI-X kullanır. Beklenen kesme gecikirse sürücü bunu bildirir ve donanım durumunu yoklar. AHCI, NVMe ve USB BOT hatalarında ilk istekten sonra en fazla üç yeniden deneme vardır. USB BOT aygıtından hata nedeni REQUEST SENSE ile okunur.

xHCI hub sürücüsü port gücünü, bağlantı değişimini, sıfırlamayı ve alt aygıt hızını yönetir. Rota kök porttan başlar, her hub için dört bitlik port numarası ekler. Düşük veya tam hızlı aygıtta yüksek hızlı hub'ın işlem çeviricisi bilgisi yuva bağlamına yazılır. Çıkarılan aygıtın eski tanıtıcısıyla yapılan erişim hata verir.

Açılış bölümünde `ramdisk.img` varsa çekirdek, dosyayı kendi FAT okuyucusu üzerinden bitişik fiziksel çerçevelere yükler. İçindeki GPT/MBR bölümleri `/diskler/ram0pN` altında görünür. Yazılar bellekteki imaja gider. Ayrı bir bağlama noktası olan `/ram` açılışta boştur. Dosya verisi sekiz çerçeve, toplam 32 KiB kullanabilir. İki alan da kapanışta bırakılır.

`kapat` önce kirli dosya sistemlerini eşitler. FADT'den bulunan DSDT içindeki tanınan AML `_S5_` paketi PM1a ve varsa PM1b uyku türlerini sağlar. Çekirdek türü ve SLP_EN bitini yazar. S5 eksikse veya makine kapanmazsa tanı mesajından sonra kapanış yedeğine geçer. `yenidenbaslat` FADT reset, 8042 ve son işlemci sıfırlama yolunu izler.

## Ağın iki mimaride çalışması

x86_64 üzerinde PCI veya USB CDC-ECM sürücüsü, AArch64 virt makinesinde virtio-mmio RX/TX halkaları Ethernet çerçevelerini taşır. Her iki çekirdek aynı `drivers/ag` paketinden Ethernet, ARP, IPv4, ICMP, DHCP, TCP ve HTTP kodunu derler. x86 zamanlayıcıları kalibre edilmiş sayacı, ARM ise CNTVCT_EL0 ve CNTFRQ_EL0 değerlerini milisaniyeye dönüştürür.

x86 kullanıcı programları süreç fd tablosu üzerinden soketlere erişir. Hazır olan fd'ler bekleyen süreci uyandırır. ARM sınama imajı TCP ve HTTP işlemini çekirdek içinde başlatır. `calistir_bm_x86_ag_iki` bir x86 ve bir ARM konuk arasında iki yönlü TCP içeriğini ve ARM diskine yazılan sonucu karşılaştırır.

## Kabuk komutları

Aşağıdaki açıklamalar `yardim <komut>` ile okunabilir. Yalın program adları boru hattında aranır. Tek program için `baslat` komutu aynı aramayı yapar. Bir hat en fazla üç program içerir. `<` ilk aşamanın girdisini, `>` ve `>>` son aşamanın çıktısını açar. `>` dosyayı keser, `>>` sonuna ekler. Bir borudaki son yazıcı kapanınca okur kalan veriyi ve ardından EOF'u alır.

| Komut | Kabukta gösterilen açıklama |
|---|---|
| `ls` | ls [yol] dizindeki adları gösterir. |
| `cd` | cd [yol] çalışma dizinini değiştirir. |
| `pwd` | pwd çalışma dizinini gösterir. |
| `cat` | cat &lt;dosya&gt; içeriği ekrana yazar. |
| `echo` | echo &lt;metin&gt; satırı yazar. |
| `yaz` | yaz &lt;dosya&gt; &lt;metin&gt; dosya içeriğini değiştirir. |
| `cp` | cp &lt;kaynak&gt; &lt;hedef&gt; dosyayı kopyalar. |
| `mv` | mv &lt;kaynak&gt; &lt;hedef&gt; dosyayı taşır. |
| `sil` | sil &lt;dosya&gt; dosyayı siler. |
| `mkdir` | mkdir &lt;yol&gt; dizin oluşturur. |
| `rmdir` | rmdir &lt;yol&gt; boş dizini kaldırır. |
| `diskler` | diskler aygıtları ve bölümleri listeler. |
| `bagla` | bagla --yazilabilir &lt;bölüm&gt; veri bölümünü yazmaya açar. |
| `sync` | sync bekleyen disk yazılarını tamamlar. |
| `saat` | saat tarih ve saati yazar. baslat saat canlı saati açar. |
| `calistir` | calistir &lt;yol&gt; [argüman] ELF dosyasını çalıştırır. |
| `baslat` | baslat &lt;ad&gt; [argüman] program arar. Sona & eklenirse arka planda çalışır. |
| `bekle` | bekle &lt;pid&gt; süreç sonlanınca çıkış kodunu toplar. |
| `ps` | ps süreç durumlarını listeler. |
| `sonlandir` | sonlandir &lt;pid&gt; süreci durdurur. |
| `sonuc` | sonuc en son toplanan çıkış kodunu gösterir. |
| `temizle` | temizle konsol ekranını siler. |
| `yardim` | yardim &lt;komut&gt; komutun kullanımını gösterir. |
| `kapat` | kapat diskleri eşitler ve sistemi kapatır. |
| `yenidenbaslat` | yenidenbaslat diskleri eşitler ve sistemi yeniden başlatır. |
| `islemci` | islemci: no, APIC, tik, ayrı süreç sayısı, boşta yüzde. |
| `donanim` | donanim aygıtları listeler. donanim kaydet &lt;yol&gt; raporu yazar. donanim gonder &lt;ip&gt; &lt;port&gt; kaydı yollar. |
| `log` | log [sayfa] günlük halkasını gösterir. log kaydet &lt;yol&gt; dosyaya yazar. log gonder &lt;ip&gt; &lt;port&gt; bir anlık görüntü yollar. |
| `hassasiyet` | hassasiyet &lt;1..4&gt; fare hızını ayarlar. |
| `goster` | baslat goster &lt;BMP&gt; resmi gösterir. Oklarla kaydırın, Q ile çıkın. |
| `bul` | baslat bul [-i] &lt;metin&gt; [dosya...] eşleşen satırları numaralandırır. |
| `hesapla` | baslat hesapla &lt;ifade&gt; parantez ve işlem önceliğiyle tam sayı hesaplar. |
| `duzenle` | duzenle: Ctrl+F/B ara, Ctrl+I harf duyarlılığı, Ctrl+H değiştir, Ctrl+G satır, Ctrl+Z/Y geri al/yinele, Ctrl+S kaydet, Ctrl+Q çık. |
| `dosyalar` | dosyalar: F5 kopyala, F6 taşı, F7 dizin oluştur, Delete sil, Q çık. |
| `bilgi` | bilgi süreyi, boş belleği, süreçleri, diskleri, kesmeleri ve giriş durumunu gösterir. |
| `kopyala` | kopyala &lt;kaynak&gt; &lt;hedef&gt; dosyayı aynı veya farklı bağlama noktasına kopyalar. |
| `tasi` | tasi &lt;kaynak&gt; &lt;hedef&gt; dosyayı taşır veya adını değiştirir. |
| `kesme` | kesme sina vektör kayıt çakışmasını sınar. |
| `pci` | pci yapılandırma alanını ve BAR eşlemelerini gösterir. |
| `duzen` | duzen tr&#124;us düzeni seçer. Ctrl+Boşluk düzeni değiştirir. |
| `girdisay` | girdisay 1..20 PS/2 baytlarını sayar. Bu sürede yazılan giriş tüketilir. |
| `ag` | ag durum kira ve adresi, ag sayac protokol sayaçlarını, ag ntp &lt;ip&gt; saat eşitlemesini gösterir. |
| `ping` | ping &lt;adres&gt; [adet [aralik_ms [zaman_asimi_ms]]] ICMP yanıtlarını ve RTT değerlerini ölçer. |
| `cozumle` | cozumle &lt;ad&gt; IPv4 adresini DNS ile çözer. |
| `nc` | nc &lt;adres&gt; &lt;port&gt; akışı standart girdiye ve çıktıya bağlar. nc -d &lt;port&gt; dinler. |
| `indir` | indir &lt;http-url&gt; &lt;dosya&gt; HTTP gövdesini diske veya geçici dosya sistemine yazar. |
| `sunucu` | sunucu &lt;port&gt; &lt;dizin&gt; dizini HTTP ile sunar. Ctrl+C ile kapanır. |
| `tftp` | tftp al &lt;ip&gt; &lt;uzak&gt; &lt;yerel&gt; indirir. tftp gonder &lt;ip&gt; &lt;yerel&gt; &lt;uzak&gt; gönderir. |
| `paket` | paket guncelle &lt;url&gt; depoyu okur. liste, bilgi, kur ve kaldir paketleri yönetir. |
