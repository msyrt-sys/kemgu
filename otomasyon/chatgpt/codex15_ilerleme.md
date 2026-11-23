2026-11-23T11:35:28+03:00

## Bölüm 1 — D-764 tabanında yol haritası denetimi

| Madde | Durum | Kanıt ve kalan iş |
|---|---|---|
| H0g | Ertelendi | H0g, makine seçimi Mehmet'te |
| H1a | Kısmen | D-763 birleşik kabuk ölçümü var. 20 bin satırlık karşılaştırma eksik |
| H1b | Kısmen | test/bm_x86_uefi_harness.sh birimleri birleştiriyor. Tek sürücülük artımlı çekirdek derlemesi eksik |
| H1c | Kısmen | D-763 süreç çerçevelerini geri veriyor. E4k için 10 bin çevrimde sabit heap kanıtı eksik |
| H1d | Tamam | D-739, calistir_capability_test ile çağrı süreli yetki ödüncü |
| H1f | Kısmen | runtime/mimari/x86_64/kem_pci_hw.kem 32 bit port erişimini sağlıyor. Genel 16 bit çiftinin kapısı eksik |
| H1g | Kısmen | D-757 ve D-763 IRQ1, IRQ12, LAPIC ve xHCI kullanıyor. Ortak kaydedilebilir vektör arayüzü eksik |
| H1h | Kısmen | D-757 kesme halkasındaki volatile yükleme ve saklamaları ölçtü. MMIO döngüsünün bariyer mutasyonu eksik |
| H2a | Ertelendi | D-755 UEFI yolu hazır. D-756 BIOS/GRUB ertelemesi |
| H2b | Ertelendi | D-756 BIOS/GRUB |
| H2c | Kısmen | D-755 UEFI bilgi yapısı var. Multiboot2 etiketleri ertelendi |
| H2f | Ertelendi | D-756 BIOS/GRUB |
| H2g | Tamam | D-755, calistir_bm_x86_uefi_pc ile q35 ve OVMF |
| H3g | Kısmen | D-757 FADT ve 8042 yeniden başlatma var. ACPI S5 kapatması eksik |
| H3h | Kısmen | D-763 xHCI MSI-X kullanıyor. AHCI kesmesi için MSI doğrulanmadı |
| H4b | Açık | runtime/mimari/x86_64/kem_ekran_gop.kem. GOP için PAT karşılaştırması eksik |
| H4i | Açık | D-763 kabuğunda log halkası ve log komutu eksik |
| H5a | Kısmen | D-757 FADT ve IRQ kuruyor. 8042 öz-test ve port testi eksik |
| H5c | Kısmen | drivers/ps2/klavye_tr.kem E0 ile basma ve bırakmayı çözüyor. E1 Pause dizisi eksik |
| H5d | Kısmen | drivers/ps2/klavye_tr.kem Shift, Ctrl, AltGr ve Caps tutuyor. Num/Scroll Lock ve LED komutu eksik |
| H5e | Kısmen | D-752 Türkçe Q ve AltGr var. ABD düzenine geçiş eksik |
| H5f | Açık | drivers/ps2/klavye_tr.kem. Zamanlanmış yazılım tekrarı eksik |
| H5g | Tamam | D-763, calistir_bm_x86_surec ile kullanıcıya basma ve bırakma olayları |
| H5h | Kısmen | D-763 geçmiş ve satır ortası düzenleme var. Ctrl+C ile süreç sonlandırma eksik |
| H5i | Kısmen | D-752 ve D-757 gerçek klavye ve hızlı tuş dizilerini deniyor. AltGr mutasyonu için ayrı QEMU kanıtı eksik |
| H5j | Kısmen | D-757 hızlı girişi deniyor, kabuk PS2_KAYIP bildiriyor. Gerçek halka taşmasıyla artan kayıp sayısının kapısı eksik |
| H6c | Kısmen | drivers/ps2/fare.kem dört baytı çözebiliyor. 200-100-80 ile cihazı bu kipe geçirme eksik |
| H6d | Kısmen | D-763 sınırda kenetlemeyi deniyor. Ayarlanabilir hassasiyet ve hızlanma eğrisi eksik |
| H6e | Tamam | D-757, calistir_bm_x86_ps2_irq ile save-under ve ekran karşılaştırması |
| H6f | Kısmen | D-763 hareket, düğme ve gezginde çift tıklama var. Tekerlek alanı kullanıcı ABI’sinde eksik |
| H6g | Tamam | D-757, calistir_bm_x86_ps2_irq ile kayık AUX baytı, gerçek hareket ve piksel karşılaştırması |
| H6h | Tamam | D-763, calistir_bm_x86_ekran_fare PS/2 göreli fareyi ölçüyor |
| H7a | Kısmen | runtime/mimari/x86_64/kem_pci_hw.kem CF8/CFC kullanıyor. ECAM erişimi eksik |
| H7b | Kısmen | kem_pci_hw.kem ve bd_bul veri yolu 0 üzerinde sınıf tarıyor. Köprü arkası tarama ve konsol listesi eksik |
| H7c | Kısmen | D-745 MMIO penceresi ve BAR erişimi var. Tüm atanmamış BAR'lar için UC eşleme kanıtı eksik |
| H7d | Kısmen | D-763 xHCI MSI-X hazır. AHCI MSI doğrulaması eksik |
| H7e | Kısmen | drivers/ahci/cekirdek.kem ve calistir_qtest IDENTIFY ve sektör G/Ç yolunu içeriyor. Hata sonrası portu sıfırlayıp sürdürme kanıtı eksik |
| H7h | Ertelendi | D-756 multiboot2 modül yolu ertelendi |
| H7i | Tamam | D-745, calistir_bm_x86_nvme ve calistir_qtest ile IDENTIFY ve gerçek blok aktarımı |
| H7j | Kısmen | D-759 AHCI/NVMe/USB ve GPT/MBR kapsıyor. Ramdisk satırı eksik |
| H8a | Kısmen | D-761 süreç fd tablosu ve ortak VFS var. /ram bağlama noktası eksik |
| H8b | Tamam | D-759, calistir_bm_x86_v1 ile FAT32 kök, alt dizin ve dosya okuma |
| H8c | Tamam | D-759, calistir_bm_x86_v1 ve drivers/fs/fat32_dizin.kem ile Türkçe LFN |
| H8h | Açık | D-763 ve E4k. 1 GB okuma sırasında sabit heap kanıtı eksik |
| H8i | Ertelendi | H0c KEMFS sonraki aşama |
| H9j | Kısmen | D-763 aynı sanal adreste farklı fiziksel çerçeveleri kanıtlıyor. E3f bağımsız fd yalıtım kapısı eksik |
| H10a | Tamam | D-755 ve D-756, calistir_bm_x86_uefi_pc ile kendi EFI stub'ı ve GOP |
| H10b | Tamam | D-763, belgeler/X86_Fiziksel_Donanim_Kontrol_Listesi.md ve calistir_bm_x86_uefi_pc |
| H10c | Tamam | D-745, calistir_qtest ile gerçek xHCI kök port enumerasyonu |
| H10d | Tamam | D-763, calistir_bm_x86_ekran_fare ile USB HID klavye ve fare |
| H10e | Açık | drivers/usb/xhci_cekirdek.kem kök portlarla sınırlı. Harici hub enumerasyonu eksik |
| H10f | Tamam | D-759, calistir_bm_x86_v1 ile BOT/SCSI üzerinden kalıcı FAT32 kayıt ve okuma |
| H10g | Kısmen | drivers/usb/xhci_cekirdek.kem sınırlı bekliyor. Eksik aygıt ve sökme sonrası kabuğa dönüşün ortak kapısı eksik |
| H10h | Kısmen | D-755 seri açılış bilgisi var. Diskte donanim.txt eksik |
| H10i | Ertelendi | H10i tek çekirdek kapsamı |
| H11c | Açık | H11b kontrol listesi var. Fiziksel eski masaüstü sonucu yok |
| H11d | Açık | H11b kontrol listesi var. Fiziksel UEFI masaüstü sonucu yok |
| H11e | Açık | H11b kontrol listesi var. Dizüstü aygıt sonuçları yok |
| H11f | Kısmen | D-763 sonuç şablonu var. Doldurulmuş fiziksel kayıt, dosya ve fotoğraf eksik |
| H12a | Kısmen | D-759 ve D-763 ayrı kapılar var. Birleşik açılış, giriş ve RAM matrisi eksik |
| H12b | Kısmen | D-759 FAT korpusu var. ACPI, ELF, BMP ve bölüm tablosu için birleşik korpus eksik |
| H12c | Kısmen | codex9–14 ilerleme belgelerinde bölüm kayıtları var. Tek envanter eksik |
| H12d | Açık | D-763 program başlatma süresi var. Firmware devrinden kabuğa aşamalı süre kapısı eksik |
| H12e | Kısmen | belgeler/KEMGU_OS_Syscall_ABI.md hazır. Mimari, program yazma ve düzen kılavuzları eksik |

64 açık kayıt incelendi. 14 madde için işaret güncellendi. D-764 tabanında v1, UEFI PC, çok süreç ve x86 çekirdek kapıları geçti. Düzenleyici, gezgin, kabuk14, süreç, ekran/fare, PS/2 IRQ, NVMe ve qtest kapıları da geçti. Yetki kapısı 60/60. Belge denetimi ve yeni QEMU koşumları 17 dakika sürdü.
