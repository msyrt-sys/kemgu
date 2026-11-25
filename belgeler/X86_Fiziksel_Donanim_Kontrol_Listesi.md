# x86_64 UEFI fiziksel donanım kontrol listesi

Bu deneme x86_64 UEFI açılışını, GOP ekranını, klavye ve fareyi, USB belleğin FAT32 veri bölümünü ve varsa NVMe/AHCI disklerin okunmasını kapsar. QEMU kâhini aynı akışı OVMF ile yürütür. Gerçek makinede gözlenen her sonucu aşağıdaki tabloya kaydedin.

## İmaj ve USB bellek

1. Depo kökünde `make kem_os_usb.img` çalıştırın. Üretici iki GPT başlığını, iki FAT32 bölümünü ve veri bölümündeki ELF dosyalarını doğrular. `sha256sum build/kem_os_usb.img` çıktısını kaydedin.
2. USB belleği takmadan önce ve taktıktan sonra `lsblk -dp -o NAME,MODEL,SERIAL,TRAN,SIZE` çalıştırın. Yeni aygıtı model, seri ve kapasite ile eşleştirin. Bellekteki verilerin yedeğini alın. Aşağıdaki yazma işlemi seçilen diskin bölüm tablosunu ve içeriğini değiştirir.
3. Bir terminalde hedef **bütün diskin** yolunu etkileşimli seçin ve bağlı bölümleri görün. Çıktıdaki model ve seri numarasını fiziksel bellekle doğrulayın. Bağlı bölümler varsa dosya yöneticisinden ayırın.

```sh
read -r -p 'Doğrulanan USB diskinin tam yolu: ' aygit
lsblk -o NAME,MODEL,SERIAL,TRAN,SIZE,TYPE,MOUNTPOINTS "$aygit"
```

4. Aynı terminalde, doğruladığınız USB belleğe yazın. Koruma koşulları bütün disk ve USB taşımasını denetler. Ardından imaj uzunluğu kadar baytı yeniden okuyup karşılaştırın.

```sh
if [ "$(lsblk -dnro TYPE "$aygit")" = disk ] && [ "$(lsblk -dnro TRAN "$aygit")" = usb ]; then
    sudo dd if=build/kem_os_usb.img of="$aygit" bs=4M conv=fsync status=progress &&
        sudo cmp -n "$(stat -c %s build/kem_os_usb.img)" build/kem_os_usb.img "$aygit"
fi
```

## Firmware ve açılış

UEFI x86_64 kipini seçin, Secure Boot'u kapatın ve firmware açılış menüsünden USB belleğin UEFI girdisini seçin. İlk denemede tek USB bellek kullanın. Varsa seri uç kaydını açın. GOP ekranında kabuk açılışı, açılış aygıtı ve istem sırasıyla görünür. Örnek USB açılışı aşağıdadır.

```text
KEMGU-OS kabuk v0
DISK USB
KABUK_HAZIR
>
```

## Deneme sırası

1. `yardim`, `diskler` ve `pwd` çalıştırın. `diskler` çıktısında ESP ve FAT32 veri bölümünü belirleyin. Örneklerdeki `usb0p2` adını o makinedeki gerçek veri bölümüyle değiştirin. İç diskler üzerindeki denemelerde okuma komutları kullanın.
2. `bagla --yazilabilir usb0p2` çalıştırın. `duzenle /diskler/usb0p2/notlar/günlük.txt` ile dosyayı açın. Türkçe karakterler ve yeni bir satır ekleyin. Ctrl+S ile “Kaydedildi” iletisini görün, Ctrl+Q ile kabuğa dönün.
3. `dosyalar /diskler/usb0p2` ile gezgini açın. Fareyle notlar dizinine ve günlük dosyasına çift tıklayın. Bir satır ekleyip kaydedin. Ctrl+Q gezgine, Q kabuğa döner. Oklar ve Enter aynı satırları açar.
4. `say /diskler/usb0p2/notlar/günlük.txt` çalıştırın. `ls /diskler` ile bağlı bir iç diski seçin. Önce `ls`, sonra `kedi` ile o diskteki bilinen bir metin dosyasını okuyun. Salt okunur bölümde düzenleyicinin uyarısını kaydedin.
5. `baslat hesap &` çalıştırın. Verilen PID'yi kaydedin. `ps`, `sonlandir <pid>` ve `bekle <pid>` ile sonucu görün. `kapat` çalıştırın. “Sistem durduruldu; güç tuşuna basın.” satırını gördükten sonra makineyi kapatın.
6. USB belleği başka bir makinede yeniden tanımlayın, veri bölümünü ayırın ve `fsck.fat -n` ile doğrulayın. `günlük.txt` içeriğini beklenen UTF-8 metniyle karşılaştırın. Ataç olarak seri kayıt, ekran fotoğrafı ve imaj SHA256 değerini saklayın.

### Donanım sonucunu getirme

`donanim` komutuyla aygıtları ekranda inceleyin. Yazılabilir açılış kökünde çekirdek açılış sırasında `/donanim.txt` dosyasını kaydeder. Standart GPT USB imajında veri bölümünü `diskler` ile bulun. Örneğin veri bölümü `usb0p2` ise `bagla --yazilabilir usb0p2`, ardından `donanim kaydet /diskler/usb0p2/donanim.txt` ve `kapat` çalıştırın. USB’den aldığınız `donanim.txt` dosyasını makine bilgileri ve ekran fotoğrafıyla birlikte sonuç kaydına ekleyin.

## Sorun giderme

| Gözlem | Kontrol |
|---|---|
| Firmware USB girdisini göstermiyor | Yazma sonrası `cmp` çıktısını, GPT ESP'yi ve firmware'deki UEFI USB açılış seçimini kontrol edin. |
| Firmware imzayı reddediyor | Secure Boot ayarını ve seçilen USB girdisini kaydedin. |
| Ekran boş | Monitör çıkışını ve firmware GOP desteğini kontrol edin. Varsa seri uçtan son açılış satırını kaydedin. |
| İstem var, tuş gelmiyor | USB boot klavyesini doğrudan bir xHCI porta takın. Varsa PS/2 yolu ile karşılaştırın. |
| Fare tepki vermiyor | Gezgin satırında düğmeye basarak seçimi doğrulayın. USB boot fare ve varsa PS/2 sonuçlarını ayrı kaydedin. |
| Kaydetmede salt okunur uyarısı | `diskler` çıktısından seçili bölümün ESP veya iç disk olup olmadığını ve veri bölümünün yazma kipini kontrol edin. |
| İç disk görünmüyor | Firmware depolama kipini ve denetleyici modelini kaydedin. Çalışan sistemin ayarlarını değiştirmeden USB denemesini tamamlayın. |

## Sonuç kaydı

Tarih, makine/anakart, CPU, firmware sürümü, imaj SHA256, Git commit'i ve kullanılan portları kaydedin.

| Aygıt veya adım | Model/bağlantı | Çalıştı / çalışmadı / denenmedi | Ölçüm, son satır veya dosya |
|---|---|---|---|
| UEFI açılışı ve GOP | | | |
| PS/2 klavye ve fare | | | |
| USB klavye ve fare | | | |
| USB depolama ve yeniden okuma | | | |
| NVMe / AHCI okuma | | | |
| Düzenleyici ve gezgin | | | |
| Süreçler ve kapanış | | | |

### GOP yazma süresi

Tanı çekirdeğini aşağıdaki komutlarla bir UEFI imajına yerleştirin. İmajı yukarıdaki USB hazırlama yöntemiyle ayrı bir deneme belleğine yazıp seri kaydı açın.

```sh
UEFI_CALISMA_DIZINI=build/pat-tani UEFI_YALNIZ_DERLE=1 bash test/bm_x86_uefi_harness.sh
python3 tests/qemu/uefi_imaj.py build/pat-tani.img build/pat-tani/esp/EFI/BOOT/BOOTX64.EFI build/pat-tani/k.elf
```

Aynı çözünürlükte üç soğuk açılış yapın. Her açılış için PAT_TSC satırındaki UC ve WC doldurma sürelerini TSC çevrimi olarak, üçüncü değeri piksel sayısı olarak kaydedin. PAT_MSR, PAT_ILK, PAT_SON, PAT_RAM ve PAT_MMIO satırlarını iliştirin. Üç ölçümün medyanını ve ekrandaki Türkçe gliflerin durumunu belirtin.

### Girdi, hub, depolama ve kapatma

- PS/2 ve USB klavyede Num, Caps ve Scroll kilitlerini ayrı ayrı açıp kapatın. LED'leri, sayısal takımın rakam ve gezinme modlarını, Pause ve PrintScreen olaylarını kaydedin.
- Harici USB hub'a klavye, fare ve bir deneme belleği takın. İkinci hub üzerinden dosya okuyup yazın. Belleği güvenli biçimde eşitledikten sonra iki kez çıkarıp takın ve her geçişin seri kaydını alın.
- Açılış bölümüne SHA256'sı kaydedilmiş bir ramdisk.img koyun. İçindeki deneme dosyasını bellekten değiştirin, /ram altında da bir dosya oluşturun. Yeniden açılışta kaynak imaj özetini ve /ram dizininin boş olduğunu denetleyin.
- Veri bölümüne yazıp kapat komutunu verin. ACPI_S5 kaydını ve gücün kesildiği anı kaydedin. Bir sonraki açılıştan önce imajda fsck.fat -n ile dosya sistemi tutarlılığını kontrol edin.

## Ağ ile deneme ve rapor alma

- Yazılabilir veri bölümünü bağlayıp `cd /diskler/<veri>` çalıştırın. `donanim` ile dökümü okuyun ve `donanim kaydet donanim.txt` ile bu dizine yazın. Rapordaki PCI satıcı ve aygıt kimliğini, NIC modelini, MAC adresini ve bağlantı durumunu kaydedin. Kabloyu DHCP hizmeti bulunan test ağına takın.
- `ag dhcp` ve `ag durum` ile kiralanan IPv4 adresini, DNS ve NTP adreslerini okuyun. `ping <ağ-geçidi> 4` ile yanıt sayısını, `ag sayac` ile hata sayaçlarını kaydedin.
- Test ağındaki HTTP sunucusundan `indir http://<sunucu>/deneme.bin /diskler/<veri>/deneme.bin` çalıştırın. Sunucudaki boyu, indirilen bayt sayısını ve aktarılan dosyanın SHA256 değerini karşılaştırın.
- Desteklenmeyen bir Realtek dahili NIC görülürse satıcı ve aygıt kimliğini raporda tutun. USB CDC-ECM sınıfı (iletişim sınıfı 02, ECM alt sınıfı 06, CDC veri sınıfı 0a) sunan adaptörü doğrudan USB porta veya desteklenen huba takın. Yeni MAC ve bağlantı satırını kaydedip aynı DHCP, ping ve indirme adımlarını tekrarlayın.
- `baslat saat metin`, `ag ntp <sunucu-ip>` ve tekrar `baslat saat metin` çalıştırın. İlk ve son UTC zamanı ile bildirilen ofset/gecikmeyi kaydedin.
- Test ağındaki bilgisayarda `python3 tools/codex/teshis_al.py donanim.txt --adres <alıcı-ip> --port 9140` çalıştırın. Konukta aynı dizinden `donanim gonder <alıcı-ip> 9140` ile raporu gönderin. Alıcıdaki bayt sayısını ve rapordaki NIC kimliğini doğrulayın. USB bellek bu işlem boyunca takılı kalır.
