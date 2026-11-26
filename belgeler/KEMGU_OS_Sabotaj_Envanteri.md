# KEMGU OS sabotaj envanteri

“Açılış” sütunu kırmızı açılış sayısını verir. “Host” denetimleri konuk başlatmadan sonuçlanır. Adet yayımlanmayan satırlarda kaynak kaydının sınırı belirtilmiştir.

| Kimlik | Bölüm | Değiştirilen davranış | Kırmızı kapı | Açılış |
|---|---|---|---|---:|
| 9-1 | A | `ğ` glifinin tek biti | `calistir_bm_x86_ekran` | 1 |
| 9-2 | B | Canlı yankıyı kaldırma | `calistir_bm_x86_kabuk` | 1 |
| 9-3 | B | Kaydırmada eski satırı kopyalama | `calistir_bm_x86_kabuk` | 1 |
| 9-4 | C | Panikte hazır bayrağını yok sayma | `calistir_bm_x86_panik` | 1 |
| 9-5 | D | PS/2 tarama baytının bit 7'sini silme | `calistir_bm_x86_ps2` | 1 |
| 10-1 | A | Descriptor adımı +8 | `calistir_bm_x86_uefi` | 1 |
| 10-2 | A | Bilgi yapısı magic denetimi | `uefi_test` | Host |
| 10-3 | B | GOP satır adımı +4 | `calistir_bm_x86_uefi_kabuk` | 1 |
| 10-4 | B | GOP piksel biçimi kabulü | `gop_test` | Host |
| 10-5 | C | Ayrılan fiziksel sayfa türü | `calistir_bm_x86_uefi_kabuk` | 1 |
| 10-6 | C | EFI runtime niteliği kabulü | `uefi_harita_test` | Host |
| 10-7 | C | 800 bölgeyle eski harita düzenleme | `self-host harita testi` | Host |
| 10-8 | D | SDT sağlama denetimi | `calistir_bm_x86_uefi_kabuk` | 1 |
| 10-9 | D | SDT sağlama denetimi | `uefi_kok_test` | Host |
| 10-10 | E | GPT bölüm başlangıcı +1 | `calistir_bm_x86_uefi_pc` | 1 |
| 11-1 | A | #DF kapısında IST=0 | `calistir_bm_x86_istisna` | 10 |
| 11-2 | A | İstisna ekran yolu kapalı | `calistir_bm_x86_istisna` | 1 |
| 11-3 | A | OOB mimari kancası boş | `calistir_bm_x86_istisna` | 1 |
| 11-4 | B | MADT ISO yok sayılıyor | `calistir_bm_x86_apic` | 10 |
| 11-5 | C | IRQ12 maskeli | `calistir_bm_x86_ps2_irq` | 10 |
| 11-6 | C | Fare ilk bayt senkron denetimi kaldırıldı | `calistir_bm_x86_ps2_irq` | 10 |
| 11-7 | D | USB bekleyişi fare tüketicisine yol vermiyor | `calistir_bm_x86_ps2_karma` | 10 |
| 11-8 | E | FADT reset yazısı kaldırıldı | `calistir_bm_x86_guc` | 1 |
| 12-1 | Blok ve dosya | Kirli LRU girdisi yazılmadan atıldı | `calistir_bm_x86_blok` | 1 |
| 12-2 | Blok ve dosya | ESP yazma koruması kaldırıldı, bölüm SHA256 değişti | `calistir_bm_x86_disk` | 1 |
| 12-3 | Blok ve dosya | EFI bölüm imzası değişti, kök eşleşmesi ve varsayılan izin kalktı | `calistir_bm_x86_disk` | 1 |
| 12-4 | Blok ve dosya | İkinci FAT kopyası atlandı, fsck.fat rc=1 | `calistir_bm_x86_disk` | 1 |
| 12-5 | Blok ve dosya | LFN sağlaması bozuldu, bağımsız okuyucu reddetti | `calistir_bm_x86_disk` | 1 |
| 12-6 | Blok ve dosya | Önbellek boşaltma atlandı, kapalı imajda dosya yok | `calistir_bm_x86_disk` | 1 |
| 12-7 | Blok ve dosya | RTC tarih damgası sabitlendi, host okuyucu reddetti | `calistir_bm_x86_disk` | 1 |
| 12-8 | Blok ve dosya | İkinci USB BOT için eski olay konumu kullanıldı | `calistir_bm_x86_disk` | 1 |
| 13-1 | Süreç / ABI | Argc sıfır yazıldı | `calistir_bm_x86_surec` | 1 |
| 13-2 | Süreç / ABI | ELF/BSS bölgesinin tahsis sıfırlaması atlandı | `calistir_bm_x86_surec` | 1 |
| 13-3 | Süreç / ABI | fd sınırında 16 geçirildi | `calistir_bm_x86_surec` | 1 |
| 13-4 | Süreç / ABI | Salt okunur yazma izni atlandı | `calistir_bm_x86_surec` | 1 |
| 13-5 | Süreç / ABI | Yuva iadesi atlandı | `calistir_bm_x86_surec` | 1 |
| 13-6 | Süreç / ABI | Kod sayfasına yazma izni verildi | `calistir_bm_x86_surec` | 1 |
| 13-7 | Süreç / ABI | Konumla sonucuna 1 eklendi | `calistir_bm_x86_surec` | 1 |
| 13-8 | Süreç / ABI | Süreç çıkışı eşitlemesi atlandı | `calistir_bm_x86_surec` | 1 |
| 13-9 | Süreç / ABI | ELF'e `kdl_syscall_isle` sembolü eklendi | `surec_elf.py` | Host |
| 13-10 | Süreç / ABI | Belgede tuş çağrısının 55 numarası 58 yapıldı | `calistir_bm_x86_surec_abi` | Host |
| 14-v1-esitle | v1-kesinti | Süreç çıkışında dosya eşitleme | `calistir_bm_x86_v1` | Kayıtta sayı yok |
| 14-gecmis | kabuk14 | Geçmiş indeksi | `calistir_bm_x86_kabuk14` | Kayıtta sayı yok |
| 14-ortak | kabuk14 | Tab ortak öneki | `calistir_bm_x86_kabuk14` | Kayıtta sayı yok |
| 14-tik-satir | gezgin | Tıklanan satır | `calistir_bm_x86_dosyalar` | Kayıtta sayı yok |
| 14-ust-yol | gezgin | Üst dizin yolu | `calistir_bm_x86_dosyalar` | Kayıtta sayı yok |
| 14-utf8 | duzenle-icerik | UTF-8 karakter sınırı | `calistir_bm_x86_duzenle` | Kayıtta sayı yok |
| 14-birlestir | duzenle-icerik | Satır birleştirme | `calistir_bm_x86_duzenle` | Kayıtta sayı yok |
| 14-kaydet | duzenle | Düzenleyicide eşitleme | `calistir_bm_x86_duzenle` | Kayıtta sayı yok |
| 14-hucre | ekran-ps2 | Satır/sütun takası | `calistir_bm_x86_ekran_fare` | 1 |
| 14-kenar | ekran-usb | Ekran kenarında kenetleme | `calistir_bm_x86_ekran_fare` | 1 |
| 14-kabuk-ciz | ekran-ps2 | Çıkışta kabuğu çizme | `calistir_bm_x86_ekran_fare` | 1 |
| 14-yazmac | hesap | Dilimde yazmaç koruma | `calistir_bm_x86_cok_surec` | 1 |
| 14-adres | cocuk | Çocuk adres alanı | `calistir_bm_x86_cok_surec` | 1 |
| 14-cerceve | hesap | Çerçeve iadesi | `calistir_bm_x86_cok_surec` | 1 |
| 14-ofset | buyukelf | Segment kopyalama ofseti | `calistir_bm_x86_buyuk_elf` | 1 |
| 14-bss | buyukelf | BSS temizleme | `calistir_bm_x86_buyuk_elf` | 1 |
| 14-uyandir | tus-ps2 | Bekleyen süreci uyandırma | `calistir_bm_x86_surec` | 1 |
| 15-1 | 2 | Boş AHCI portunda yoklama süresi | `calistir_bm_x86_saglamlik` | 1 |
| 15-2 | 3 | PCI sınıfı bir bayt kaydırıldı | `calistir_bm_x86_donanim` | 1 |
| 15-3 | 4 | Günlük başı iki bayt ilerledi | `calistir_bm_x86_log` | 1 |
| 15-4 | 5 | MBR bölüm başlangıcı bir sektör kaydı | `calistir_kem_os_pc_matris` | 6 |
| 15-5 | 6 | Geçerli ELF program başlığı reddedildi | `calistir_kem_os_ayristirici` | Host |
| S210 | 6 | ELF program başlığı sınır denetimleri (`elf64.kem` iki satır) kaldırıldı → bulanıklaştırıcı `PANIK: dizi sınır ihlali (i=34627, boyut=512)` ile düştü (Claude, D-765) | `calistir_kem_os_ayristirici` | Host |
| S221 | C16-001 | C nitelikli çağrı dalı dönüş bayraklarını yine `0,0` yazdı → `ana_c16_cagri_bayraklari` C exit 3 (Claude, D-767) | `calistir_llvm_test` + `calistir_modul_codegen` | Host |
| S222 | C16-001 | Self-host `cagri_hedef_coz` ham adı döndürdü (fn_coz yok) → self exit 1 (Claude, D-767) | `calistir_modul_codegen` | Host |
| S223 | 9 | USB tanımlayıcı `n < 2` korunması kaldırıldı → ilk denemede SESSİZ (korpus sınıfa özgü türü üretmiyordu); sınır örneği eklenince `exit 124` asılma (Claude, D-767) | `calistir_kem_os_ayristirici` | Host |
| S224 | 10 | MMIO volatile listesine sıradan bellek döngüsü (`bm_esit`) eklendi → `MMIO RED bm_esit` (Claude, D-767) | `calistir_mmio_volatile` | Host |
| 15-6 | 7 | Tekerlek işareti ters | `calistir_bm_x86_girdi15` | 1 |
| 15-7 | 7 | Tuş tekrar gecikmesi sıfır | `calistir_bm_x86_girdi15` | 1 |
| 15-8 | 8 | İfade sonucu +1 | `calistir_bm_x86_program15` | 1 |
| 15-9 | 8 | İlk satır numarası 2 | `calistir_bm_x86_program15` | 1 |
| 15-10 | 8 | BMP mavi kanalı ters | `calistir_bm_x86_program15` | 1 |
| 15-11 | 8 | RTC saat alanı +1 | `calistir_bm_x86_program15` | 1 |
| 15-12 | 9 | Halkada yer yokken yazma başarılı döndü | `calistir_bm_x86_boru` | 1 |
| 15-13 | 10 | USB sonuna 150 LAPIC tiki eklendi | `calistir_bm_x86_acilis_suresi` | 3 |

Kaynaklar [`Dalga 9`](../otomasyon/chatgpt/codex9_ilerleme.md), [`Dalga 10`](../otomasyon/chatgpt/codex10_ilerleme.md), [`Dalga 11`](../otomasyon/chatgpt/codex11_ilerleme.md), [`Dalga 12`](../otomasyon/chatgpt/codex12_ilerleme.md), [`Dalga 13`](../otomasyon/chatgpt/codex13_ilerleme.md), [`Dalga 14`](../otomasyon/chatgpt/codex14_ilerleme.md), [`Dalga 15`](../otomasyon/chatgpt/codex15_ilerleme.md).

Dalga 14 için korunmuş tek-açılış QEMU kayıtları da kontrol edildi. Sayısı belirtilmeyen ölçümlerde tablodaki kapı adı `tools/codex/dalga14_sabotaj.py` içindeki senaryoya dayanır.

## Dalga 9 öncesi yol haritası kanıtları

D-751 ağacında H0a–f donanım ve kabul kararlarıydı. Uygulama olarak işaretli H1e, H4e, H4g ve H4j aşağıda ayrı satırlardadır. D-739 ve D-745 ile çalışan, işareti daha sonra güncellenmiş maddeler ve daha eski saf mantık kanıtları da dahildir. Bir saf test, yalnız o satırdaki alt davranışı sınar. Fiziksel PC kabulü Mehmet'in kontrol listesinde açıktır.

| H maddesi | Kaydedilen değişiklik | Kapı ve gözlem | Kaynak |
|---|---|---|---|
| H0a, H4j | Bochs DISPI genişliği 800 yapıldı | x86 ekran kâhini 640×480 yerine 800×480 gördü | [D-748, S193b](../LOOP.md) |
| H0b, H2c | Multiboot2 modül adres sırası değiştirildi | Host ayrıştırıcı kâhini kırmızı. BIOS yolu ertelidir. | [Dalga 4 A3](../otomasyon/chatgpt/codex4_ilerleme.md) |
| H0c, H8d | Yalnız ilk FAT kopyası yazıldı | FAT32 yazma testi kırmızı | [Dalga 1 B6](../otomasyon/chatgpt/codex_ilerleme.md) |
| H0d, H6d, H6f, H6g | HID fare Y yönü ters çevrilmedi | QEMU qtest rc 2, sahte aygıt rc 1 | [Dalga 6 G5](../otomasyon/chatgpt/codex6_bulgular.md) |
| H0e, H10d | HID kısa paket tamamlanma kodu değiştirildi | QEMU qtest rc 2, USB çekirdek testi rc 1 | [Dalga 6 G4](../otomasyon/chatgpt/codex6_bulgular.md) |
| H0f, H8f | Küme zincirinden kopyalanan baytlar bozuldu | Haricî FAT dosya karşılaştırması kırmızı. Uçtan uca v1 için ayrıca 14-v1-esitle satırına bakın. | [Dalga 1 B8](../otomasyon/chatgpt/codex_ilerleme.md) |
| H1d | Ödünç yetki denetimi ve kod üretimi değiştirildi | 16 C kuralı, self-host ve iki kod üretimi değişikliğinde kurulum 0, kapı 2 | [Dalga 3 D2b–e](../otomasyon/chatgpt/codex3_ilerleme.md) |
| H1e, H7c | Bayt okuyucuyla çözülen 64 bit BAR'ın üst sözcüğü atıldı | PCI saf ayrıştırıcı testi kırmızı | [Dalga 1 C3](../otomasyon/chatgpt/codex_ilerleme.md) |
| H2d | İlk MiB'nin çerçeve koruması değiştirildi | Çerçeve ayırıcı koşumu kırmızı | [Dalga 1 C5](../otomasyon/chatgpt/codex_ilerleme.md) |
| H3a, H3b | ACPI SDT sağlama kararı değiştirildi | ACPI tablo koşumu kırmızı | [Dalga 1 A6](../otomasyon/chatgpt/codex_ilerleme.md) |
| H3c | MADT'de kayıttan okunan adres sıfırlandı | MADT koşumu kırmızı | [Dalga 1 C1](../otomasyon/chatgpt/codex_ilerleme.md) |
| H3f | FADT X_DSDT seçimi atlandı | FADT koşumu kırmızı | [Dalga 1 C2](../otomasyon/chatgpt/codex_ilerleme.md) |
| H4a | Çerçeve genişliğinin kenar testi değiştirildi | Piksel sınır testi kırmızı | [Dalga 1 D1](../otomasyon/chatgpt/codex_ilerleme.md) |
| H4c, H4g | Türkçe glif ASCII'ye döndürüldü, ayrıca glif bit sırası ters çevrildi | QEMU ekranında sırasıyla 114 ve 1.534 piksel farklı | [D-748, S194 ve S192](../LOOP.md) |
| H4d | Konsol kirli satır işaretleri temizlenmedi | Konsol modeli kırmızı | [Dalga 1 D4](../otomasyon/chatgpt/codex_ilerleme.md) |
| H4e | Seri bayt yazılırken ekran aynasına kopya atlandı | Bu dalgada taze PVH çekirdeğiyle kabuk kapısı rc 1, ilk görünümde 940 piksel farklı | [Dalga 16 bulguları](../otomasyon/chatgpt/codex16_bulgular.md) |
| H5c, H5d, H5e | Shift ve Caps ilişkisi değiştirildi | Türkçe ve ABD düzeni testleri kırmızı | [Dalga 1 A1 ve F3](../otomasyon/chatgpt/codex_ilerleme.md) |
| H5h | Satır düzenleyicisinin giriş sınırı bir arttı | Satır düzenleme koşumu kırmızı | [Dalga 1 D6](../otomasyon/chatgpt/codex_ilerleme.md) |
| H5i | USB Türkçe ğ kodu değiştirildi | Gerçek QMP girdisinde beklenen kod noktası yerine 286, qtest rc 2 | [Dalga 6 G4](../otomasyon/chatgpt/codex6_bulgular.md) |
| H5j | Dolu girdi halkasında kayıp sayacı sabit tutuldu | Halka koşumu kırmızı | [Dalga 1 D8](../otomasyon/chatgpt/codex_ilerleme.md) |
| H6b, H6c | PS/2 fare taşma bitleri ters yorumlandı | Fare paket testi kırmızı | [Dalga 1 A2](../otomasyon/chatgpt/codex_ilerleme.md) |
| H7e | AHCI yanlış LBA'dan okudu | 20 gerçek açılışın tamamında disk kâhini kırmızı | [Dalga 8 B](../otomasyon/chatgpt/codex8_ilerleme.md) |
| H7f, H8e | Önbellek sürüm denetimi değiştirildi | Kirli LRU ve eski yazma onayı testi kırmızı | [Dalga 4 E1](../otomasyon/chatgpt/codex4_ilerleme.md) |
| H7g | GPT bölüm tablosu CRC kararı değiştirildi | Bölüm testi kırmızı | [Dalga 1 A3](../otomasyon/chatgpt/codex_ilerleme.md) |
| H7i | NVMe okunan verinin yüksek biti silindi | Gerçek qtest rc 2, ilk fark bayt 10 | [Dalga 6 G0](../otomasyon/chatgpt/codex6_bulgular.md) |
| H7j, H8j, H8k, H8l, H8m | Çoklu disk bağlama planında ESP denetimi değiştirildi | Bölüm bağlama ve depolama matrisi kırmızı | [Dalga 4 E2–E3](../otomasyon/chatgpt/codex4_ilerleme.md) |
| H8a | Yol bileşeninde izin verilen uzunluk değiştirildi | Yol ayrıştırıcı testi kırmızı | [Dalga 1 B3](../otomasyon/chatgpt/codex_ilerleme.md) |
| H8b | FAT zincirinden dosyaya kopyalanan veri bozuldu | Çok kümeli dosya okuma testi kırmızı | [Dalga 1 B7](../otomasyon/chatgpt/codex_ilerleme.md) |
| H8c | LFN kısa ad sağlama karşılaştırması değiştirildi | Eklenen iki parçalı olumsuz örnek kırmızı | [Dalga 1 A5](../otomasyon/chatgpt/codex_ilerleme.md) |
| H8g | FAT'ten FSInfo onarılırken tahsis hesabı ters çevrildi | Bozuk imaj ve onarım testi kırmızı | [Dalga 1 F2](../otomasyon/chatgpt/codex_ilerleme.md) |
| H9c | ELF segmentinin izin değerlendirmesi değiştirildi | ELF64 koşumu kırmızı | [Dalga 1 C6](../otomasyon/chatgpt/codex_ilerleme.md) |
| H9i | Masaüstü benzetiminde seçilen dosya adı değiştirildi | Uçtan uca saf benzetim kırmızı | [Dalga 1 E1](../otomasyon/chatgpt/codex_ilerleme.md) |
| H10c | xHCI olay halkası dört girdiye indirildi | Gerçek qtest rc 2 ve tam sahte enumerasyon rc 1 | [Dalga 6 G1/G6](../otomasyon/chatgpt/codex6_bulgular.md) |
| H10f | İlk Unit Attention sonrası yeniden deneme atlandı | SCSI sense 6/41, qtest rc 2 | [Dalga 6 G3](../otomasyon/chatgpt/codex6_bulgular.md) |
| H12b | Bozuk USB korpusundaki VID yorumu değiştirildi | Belirlenimci ayrıştırıcı testi kırmızı | [Dalga 4 F1](../otomasyon/chatgpt/codex4_ilerleme.md) |
| H12e | BMP modülü belge envanterinden çıkarıldı | Belge envanteri kırmızı | [Dalga 1 E3](../otomasyon/chatgpt/codex_ilerleme.md) |

## Dalga 16

| Bölüm / doğrulama | Tek değişiklik | Ölçülen sonuç |
|---|---|---|
| 1 · klavye16 | Pause dördüncü baytı yutuldu | 581 olayı kayboldu |
| 1 · klavye16 | Caps ve Scroll LED bitleri yer değiştirdi | PS/2 maske dizisi değişti |
| 1 · klavye16 | Ctrl+C kabuğu sonlandırdı | Yeni istem gelmedi |
| 1 · klavye16 | Taşmada sayaç artırılmadı | 578 eksik basışa karşı sayaç 0 |
| 2 · pci16 | ECAM işlev adresi bir bit kaydı | QMP karşılaştırmasında iki işlev eksik |
| 2 · pci16 | Köprü alt sınırı yenilenmedi | Arkadaki üç PCI işlevi eksik |
| 2 · pci16 | BAR üst yarısı atlandı | MMIO sayfa hatası ve açılış başarısızlığı |
| 3 · msi | AHCI MSI veri vektörü değiştirildi | Kesme zaman aşımı ve yoklama yedeği |
| 3 · msi | Aynı vektöre ikinci işleyici kabul edildi | Çakışma kâhini reddetti |
| 4 · disk_hata | Yeniden deneme sayısı sıfırlandı | Tek seferlik blkdebug hatası kaldı |
| 4 · disk_hata | AHCI portu yeniden başlatılmadı | Aynı porttaki sonraki geçerli dosya okunamadı |
| 5 · usb_hub | Rota port numarası bir kaydırıldı | Aygıt adresleme kodu 5 |
| 5 · usb_hub | Port değişim biti onaylanmadı | İkinci canlı takma konukta görülmedi |
| 6 · ramdisk | LBA bir sektör kaydırıldı | RAM imajında geçersiz MBR |
| 6 · ramdisk | Silinen düğüm listede kaldı | ls sonucunda eski ad görüldü |
| 7 · kapat | SLP_EN atlandı | QEMU beş saniyede kapanmadı |
| 7 · ACPI host | SLP_TYP kaydırması değiştirildi | Sıfır dışı tipte host testi çıkış 8 |
| 8 · pat | PAT giriş 7 varsayılan UC bırakıldı | İlk GOP sayfası UC çözüldü |
| 9 · ayrıştırıcı | USB sıfır uzunluk denetimini kaldırma | Uygulanmadı. Kırmızı kanıt yok. |
| 10 · UART | 16550 TX sınırı kaldırıldı | Meşgul host aygıtında zaman aşımı, rc 124 |
| 10 · mmio_volatile | Sıradan FAT toplama döngüsü listeye eklendi | Volatile ve döngü denetimi reddetti |
| 11 · derleme_olcum | Ana işlev çağrı dizisi on kat çoğaltıldı | Her iki derleyicide en büyük işlev eşiği aşıldı |
| 12 · arac16 | Geçmişte bir adım atlandı | Geri alma ekranında 329 piksel farklı |
| 12 · arac16 | Çapraz kopyada son kısmi küme atlandı | İmajda 1.264 yerine 1.024 bayt |
| 12 · arac16 | Boş çerçeve sayısı bir artırıldı | QMP hesabı 47.834, konuk 47.835 |
| 13 · klavye16 | AltGr+Q işareti değiştirildi | AltGr dizisinin fiziksel tampondaki UTF-8 karşılaştırması kırmızı |

[Dalga 16 koşumları ve ölçümler](../otomasyon/chatgpt/codex16_bulgular.md)

## Dalga 17 — ağ

Her satırdaki sonuç değiştirilen bileşenin gerçekten çalıştırıldığı ortamı belirtir. Host sonuçları C ve self-host durum makinelerinden alınmıştır. Normal korumalarla geçen sınama, uygulanmamış bir değişikliğe kırmızı sonuç olarak sayılmaz.

| Bölüm | Değişiklik | Ölçüm ve kapı |
|---|---|---|
| 1 | Eşin IPv4 sağlama hesabından bir bayt çıkarıldı | Sabit çerçeve testi çıkış 1, `calistir_ag_es` |
| 2 | TX EOP kaldırıldı. Ayrı imajda RX RDT ilerletilmedi. | Gerçek eşe TX ulaşmadı. Sekizinci RX alınmadı. `calistir_bm_x86_e1000` |
| 3 | 82574L RDBAH yazımı atlandı. 82576 alıcı kuyruğu açılmadı. | Yüksek adres geri okuması ve ilk RX ayrı QEMU imajlarında başarısız, `calistir_bm_x86_nic_matris` |
| 4 | IPv4 taşıma katlama adımı atlandı. ARP yanıt MAC'i ve loopback çıkışı ayrı ayrı değiştirildi. | Üç ayrı QEMU imajı ilgili ping, ARP veya fiziksel kablo denetiminde kırmızı, `calistir_bm_x86_ag_l3` |
| 5 | DHCP T1 zamanı kira sonuna taşındı | Telde beklenen yenileme görülmedi. DNS döngü denetimini kaldırma uygulanmadı. |
| 6 | Parça ofsetinin sekiz çarpanı çıkarıldı | 8 KiB UDP yankısı tamamlanmadı. Yuva zaman aşımını kaldırma uygulanmadı. |
| 7 | Sıra karşılaştırması ve FIN sıra tüketimi ayrı ayrı değiştirildi | Host durum testlerinde çıkış 4 ve 7. Değiştirilmiş çekirdekle QEMU asılma deneyleri uygulanmadı. |
| 8 | RTO sıfırlandı, ayrı denemede çok büyütüldü. Yinelenen payload yeniden eklendi. | Host zamanlayıcı testleri çıkış 2 ve alıcı testi çıkış 9. Bu üç değişiklikle QEMU trafik, süre ve SHA deneyleri uygulanmadı. |
| 9 | Pencere kaydırması kaldırıldı. Host durum makinesinde gecikmeli ACK zamanlayıcısı kaldırıldı. | QEMU 131.068 yerine 262.140 bayt bildirdi. Host tek segment testi çıkış 5. Değiştirilmiş çekirdekte kapanmayan oturum deneyi uygulanmadı. |
| 10 | Çoklu beklemede borunun okunabilir biti kaldırıldı | Konuk çıkış 15 ve kâhin çıkış 1. Süreç çıkışında soketi açık bırakma uygulanmadı. |
| 11 | nc kullanıcı ELF'inde stdin EOF sonrası FIN kaldırıldı | Eş 1 MiB aldı, üç saniyede FIN gelmedi, kâhin çıkış 1. |
| 12 | Sıfır boylu HTTP chunk sonlandırıcı olarak tanınmadı | Geçerli trailer işlenirken -74, kâhin çıkış 1. Yüzde kodlu üst dizin korumasını kaldırma ve sınırsız yönlendirme uygulanmadı. |
| 13 | NTP epoch düzeltmesi kaldırıldı. CMOS BCD alanları ikili yazıldı. | Ayrı QEMU RTC okumalarında 2024 yerine 2094, 2031 yerine 2025. İki çıkış da 1. |
| 14 | Çift blok ikinci kez yazıldı. Kısa son blok tanınmadı. | SHA ve boy farkı (16.384 → 16.896 bayt), ayrı denemede -110. İki kâhin de kırmızı. |
| 15 | Hatalı indirmede geçici dosyayı silme atlandı | FAT32 üzerinde yuk.tmp kaldı, çıkış 1. Bozuk ELF'i yayımlamak için SHA256 karşılaştırmasını kaldırma uygulanmadı. |
| 16 | CDC-ECM MAC dizgisinde haneler kaydırıldı | Ham eşte altı DISCOVER, sıfır OFFER. Slirp üzerinde de beklenen MAC ile uyuşmadı. |
| 17 | Virtio-mmio RX tamponu halkaya geri verilmedi | İki derleyicide [10] PING CANLI sonrasında [43] AG TCP HATA. Pozitif aktarım kâhini başarısız. |
| 18 | TCP sıfır seçenek uzunluğu, IPv4 toplam boyu ve HTTP chunk boy taşması korumalarını kaldırma | Üçü de uygulanmadı. Sınır girdilerinin korumalı sürümde reddi C ve self-host korpusunda denetlendi. |
| 19 | İki konuktan birinin ARP yanıtı kapatıldı | Eş telde ARP isteği gördü. Yanıt gelmedi. Konuk PING HATA -110 ve KABUK_HAZIR verdi. Olumlu sağlamlık kâhini geçti. |
| 20 | Belge örnekleri ve yardım metinleri | Bu bölümde sabotaj istenmedi. Beş belge programı ve kabuk yardım tablosu için `calistir_bm_x86_belge15`. |

### Dalga 17 — Claude'un tamamladığı sabotajlar (D-771)

Codex'in "uygulanmadı" dediği değişiklikler tek tek uygulandı. Sessiz kalan sabotajda önce korpus düzeltildi, kural silinmedi.

| Sabotaj | Bölüm | Değişiklik → ölçüm | Kapı | QEMU |
|---|---|---|---|---|
| S236 | C17-001 | C `erisim_lvalue` içindeki `(*p).alan = …` dalı kaldırıldı → C exit 1 (self 42) | `calistir_codegen_diff` + `calistir_llvm_test` | Host |
| S237 | C17-001 | Self-host `alan_adres_uret` TEKLI `deref*` dalı kaldırıldı → self exit 1 (C 42) | `calistir_codegen_diff` | Host |
| S238 | 18 | TCP seçenek `boy<2` koruması kaldırıldı → ilk denemede SESSİZ (korpus yalnız bilinen türlerde sıfır boy üretiyordu); bilinmeyen tür 30 + boy 0 sınır örneği eklenince exit 124 asılma | `calistir_chatgpt_kopru` (`ag17_test`) | Host |
| S239 | 18 | IPv4 `t>n-14` toplam boy koruması kaldırıldı → `ag17_test` exit 4, `l3_test` exit 7 | `calistir_chatgpt_kopru` | Host |
| S240 | 18 | HTTP chunk boyu taşma denetimi kaldırıldı → `ag17_test` exit 20, `http_test` exit 5 | `calistir_chatgpt_kopru` | Host |
| S241 | 5 | DNS `adim>=32` sıçrama sınırı kaldırıldı → SESSİZ. Ölçülmüş fazlalık: yalnız-geriye işaretçi kuralı döngüyü tek başına bitirir | — | Host |
| S242 | 5 | DNS "işaretçi yalnız geriye" (`hedef>=k`) kuralı kaldırıldı → SESSİZ. Ölçülmüş fazlalık: 32 sıçrama sınırı döngüyü tek başına bitirir | — | Host |
| S243 | 5 | S241+S242 birlikte → ilk denemede SESSİZ (korpusta etiketsiz iki-işaretçi döngüsü yoktu); 12→14→12 döngüsü eklenince exit 124 asılma. İki koruma birbirini yedekler, birlikte kapılı | `calistir_chatgpt_kopru` (`ag17_test`) | Host |
| S244 | 12 | HTTP yol bileşeni sondaki `.` denetimi kaldırıldı → `http_test` exit 10 | `calistir_chatgpt_kopru` | Host |
| S245 | 15 | `paket.kem` SHA256 karşılaştırması kaldırıldı → bozuk paket `paket kuruldu bozuk`, kâhin AssertionError | `calistir_bm_x86_paket` | 1 |
| S246 | 10 | Süreç çıkışında `ksv_fdleri_birak` içindeki `kb_birak` atlandı → soket açık kaldı, kâhin FIN beklerken zaman aşımı | `calistir_bm_x86_soket` | 1 |
| S247 | 12 | Yönlendirme içi `yon>=5` sınırı gevşetildi → SESSİZ. Ölçülmüş fazlalık: döngü koşulu `iken yon<=5` ve sonraki `ver -40` aynı sınırı yeniden uygular | — | 1 |
| S247b | 12 | Sınır iki yerde birden 5→6 → yedinci istek `/yon6` gitti, kâhin altı istek bekliyordu | `calistir_bm_x86_http` | 1 |
| S248 | 6 | Parça yuvası yaşlanması kaldırıldı → `parca_test` exit 7 | `calistir_chatgpt_kopru` | Host |
