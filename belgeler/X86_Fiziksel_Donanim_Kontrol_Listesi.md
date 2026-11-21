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
