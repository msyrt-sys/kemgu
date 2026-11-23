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
| `donanim` | donanim aygıtları listeler. donanim kaydet &lt;yol&gt; raporu yazar. |
| `log` | log [sayfa] günlük halkasını gösterir. log kaydet &lt;yol&gt; dosyaya yazar. |
| `hassasiyet` | hassasiyet &lt;1..4&gt; fare hızını ayarlar. |
| `goster` | baslat goster &lt;BMP&gt; resmi gösterir. Oklarla kaydırın, Q ile çıkın. |
| `bul` | baslat bul [-i] &lt;metin&gt; [dosya...] eşleşen satırları numaralandırır. |
| `hesapla` | baslat hesapla &lt;ifade&gt; parantez ve işlem önceliğiyle tam sayı hesaplar. |
| `duzenle` | baslat duzenle &lt;dosya&gt; metni açar. Ctrl+S kaydeder, Ctrl+Q çıkar. |
| `dosyalar` | baslat dosyalar [dizin] dosya gezginini açar. |
