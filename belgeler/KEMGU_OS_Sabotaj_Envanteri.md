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
