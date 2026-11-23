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
| H6f | Kısmen | D-763 hareket, düğme ve gezginde çift tıklama var. Ham ABI alanı var. Fiziksel tekerleğin uçtan uca uygulama sınaması eksik |
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

## Bölüm 2 — aygıt hatalarından sonra kabuk

Beş QEMU senaryosu iki derleyicide geçti (10/10). USB bellek çıkarıldıktan sonra hem USB hem PS/2 klavye ile komut çalıştı. Sekiz gerileme hedefi rc=0 döndürdü. Süreç ailesinde 200 ölçümlü açılış vardı. Boş SATA için yoklama üst sınırını kaldıran ayrı kopyada ELF derlendi, konuk isteme ulaşamadı ve sağlamlık kâhini rc=1 döndürdü. Envanter ve iki eski C UART yolunun kapsam sınırı bulgular dosyasında. Bölümün QEMU gerileme koşumu yaklaşık 17 dakika sürdü.

## Bölüm 3 — gerçek PC teşhis paketi

GPT ve MBR donanım raporu C ve self-host ile 4/4 geçti. İki diskte de fsck temizdi. MBR bağlama kâhini ayrıca geçti. PCI sınıfı bir bayt kaydırılmış geçici çekirdekte PCI kümesi karşılaştırması rc=1 verdi. Sekiz gerileme hedefi rc=0. QEMU ve derleme ölçümleri yaklaşık 17 dakika sürdü.

## Bölüm 4 — çekirdek günlük halkası

8192 baytlık halkadan yalnız tam satırlar çıkarılır. İki derleyicide de FAT32 dosyası, düşen satır sayısı ve iki ekran sayfası bağımsız seri kayıtla eşleşti. Örnek oturumda 268 satır düştü, 8183 bayt dışa aktarıldı. Başı bir yerine iki ilerleten çalışma kopyasında kâhin rc=1 döndü. Sekiz gerileme hedefi geçti. İlk gerileme koşumu yaklaşık 16 dakika, ikinci sayfa kâhini iki derleyicide ayrıca çalıştırıldı.

## Bölüm 5 — PC açılış matrisi

12 yapılandırma iki derleyicide üçer kez açıldı. 72/72 koşumda program çalıştı, FAT32 dosya içeriği, fsck ve salt okunur alanların SHA256 denetimi geçti. İlk C taramasında 4 GiB belleğe sahip dört satırın 12/12 açılışı başarısızdı. OVMF'nin 4 GiB üzerine koyduğu EFI yükleyicisini düşük fiziksel çerçeve ayırıcısının dışında tutarak giderildi. Bir sektör kaydırılmış MBR ile altı MBR satırının altısı kırmızı. Matris ve sürücü koşumlarına ek olarak sekiz gerileme hedefi çalıştırıldı.

Matrisin düzeltme sonrası koşumu 12 dakika sürdü. İlk gerileme koşumunda çok süreç ve kabuk14 aralıklı kırmızı verdi. QMP tuşlarını konuk LAPIC saatine göre gönderen koşumda çok süreç 60/60, kabuk14 20/20 geçti. Son kabuk14 tekrarı 13:03'te tamamlandı.

## Bölüm 6 — ayrıştırıcılar ve bozuk ELF

İki derleyici sabit tohumlarla 17.000'er örnek çalıştırdı. Kısa ELF için QEMU iki derleyicide de açık hata verdi ve aynı oturumda ikinci kabuk komutu çalıştı. Geçerli ELF başlığını yanlış reddeden değişiklik derlendi ve host kâhini rc=1 verdi. İstenen sınır denetimini kaldırıp bir çökme üretme deneyi uygulanmadı. Sekiz gerileme hedefi ile sürücü kapıları geçti. Bölümün ana koşumu 18 dakika sürdü.

## Bölüm 7 — tekerlek, hız ve tuş tekrarı

PS/2 IntelliMouse ve USB HID ile iki uygulamada aşağı ve yukarı kaydırma GOP piksellerinden ölçüldü. Dört açılışta 20 tik boyunca tek harf, 68 tik boyunca 5–6 harf görüldü. Tekerlek yönünü ters çevirmek ve ilk tekrar gecikmesini kaldırmak ayrı ayrı kâhini kırdı. İlk sekizli koşum 18 dakika sürdü. Aynı derleme dizinini paylaşan PS/2 IRQ, karma ve UEFI PC hedefleri ayrı dizinlerde yeniden çalıştırıldığında üçü de geçti.

Tuş sayısı son kâhinde komut satırının GOP gliflerinden okundu. Gecikmesi kaldırılmış çekirdek, 20 tikte beklenen bir harf yerine altı harf çizdi. Özgün çekirdek dört açılışta da sınırı karşıladı.

## Bölüm 8 — dört kullanıcı programı

`goster` BMP'yi oranını koruyarak sığdırıyor ve oklarla özgün ölçekte kaydırıyor. `bul -i` Türkçe harfleri ve birden çok dosyayı, `hesapla` önceliği, parantezi ve taşmayı, `saat` canlı RTC saniyesini kapsıyor. Saf mantık testleri ve iki derleyicide dört programın sekiz QEMU senaryosu geçti. Sonuç, satır numarası, mavi kanal ve saat alanı için bağımsız dört yanlış uygulama birer açılışta yakalandı.

Bölümün ilk gerilemesi 18 dakika sürdü. Çocuk süreci oturumlarından birinde program bulunamadı. Aynı çok süreç hedefinin 19 dakikalık yeniden koşumu 60/60 geçti. Son görüntüleyici ve hata iletimiyle dört program ayrıca iki derleyicide yeniden doğrulandı. Saf uygulama testleri köprüye dahil edildi.

## Bölüm 9 — borular ve yönlendirme

Sekiz adet 4096 baytlık boru, fd uçları ve hazır olunca uyandırma eklendi. ABI 69 boruyu açıyor. ABI 70 çocuğu fd eşlemeleriyle başlatıyor. Eşlemelerin tek çağrıda yapılması ebeveyn kabuğun kendi standart fd'lerini geçici olarak değiştirmesini önlüyor. Üç aşamalı Türkçe arama, dosyaya ekleme/kesme, dolu boru ve kullanıcı ABI senaryoları iki derleyicide geçti. Kayıplı yazmayı başarı sayan ayrı çekirdekte büyük aktarım karşılaştırması kırmızı.

İlk gerilemede yüz çocuklu senaryo host bekleme süresini bir kez aştı. Çok süreç ve boru kapılarının yaklaşık 19 dakikalık yeniden koşumu 60/60 ve 8/8 geçti. Sekiz gerileme hedefinin tamamı için yeşil sonuç alındı.

## Bölüm 10 — açılış süresi ve sabotaj envanteri

PIT ile kalibre edilen TSC'den yedi aşama ve bağımsız host saati ölçüldü. Beşer açılışta host medyanları C için 247,0 ms ve self-host için 274,1 ms çıktı. Eşik 650 ms. Yapay 150 tik gecikmesi üç açılışta host medyanını 1866,5 ms'ye çıkardı ve kapı rc=1 verdi. Dalga 9–15 için 71 sabotaj kaydı, özgün kapı ve bilinen açılış sayılarıyla tek tabloda toplandı.

Sekiz gerileme hedefi ve süre kapısı birlikte yaklaşık 21 dakika sürdü, çıkış kodu 0.

## Bölüm 11 — çalışan belgeler

Kullanıcı programı yazma, x86_64 mimarisi ve Türkçe Q klavye düzeni için üç kılavuz eklendi. Markdown içinden alınan iki örnek iki derleyicide de derlendi ve QEMU'da çalıştı. 33 komutun açıklaması gerçek `yardim <komut>` çıktısıyla eşleşti. Sınıflandırma kapısında 358/379 ve mevcut 21 muafiyet, bare-metal yapı paritesinde 23/23 ve sıfır atlama ölçüldü.

Sekiz gerileme hedefini içeren bölüm koşumu 21 dakika sürdü ve rc=0 döndü. Tam takımda ortaya çıkan erken G/Ç zaman aşımının ölçümü ve düzeltmesi bulgular belgesinde. Düzeltmeyle USB ve AHCI 40/40, eski disk yazma kapısı her iki derleyicide geçti. Yol haritasındaki bölüm 1 tablosu D-764 durumunu saklıyor. Yol haritası belgesi dalga sonunda yeni kanıtlarla güncellendi.

## Kapsam ve ölçüm ortamı

D-764 tabanı `030b0f8`. x86_64 Q35, TCG ve OVMF. Kod C bootstrap ve self-host derleyicileriyle sınandı. ARM64 ve x86_64 birleşik çekirdek yapı paritesi de denetlendi. E4k ve fiziksel PC denemesi sonraki entegrasyonun kapsamındadır.

`make kem_os_usb.img` 135.283.200 bayt, 19 ELF ve iki temiz FAT32 bölümü üretti. Bu koşumun SHA256 özeti `17d465c89c7d7ce410b3c88e4df7bbb6b9c2bd7f717ee16874387d1c49b0dc08`. ABI v2 tutarlılık kapısı 35 çağrıyı eşleştirdi. Sürücü alt-makefile'ı `drivers_test` hedefinde her iki testi geçti.

## Dil özellikleri

Saf sürücüler mevcut sonuç, dizi ve yapı tiplerini kullanıyor. Var olan kesme işleyicileri giriş baytlarını ve zaman sayaçlarını aktarıyor. Yeni kesme işleyicisi, `tekkez` veya `yetki` bildirimi eklenmedi.

Son G/Ç düzeltmesiyle v1 ve v1 kesinti senaryoları iki derleyicide toplam 40/40 açılışta geçti. Aynı koşumda sınıflandırma, bare-metal parite, belge, açılış süresi, USB, AHCI ve disk kapıları rc=0 verdi.

## Tam takım

`make -j6 -k --output-sync=target test_tumu` 117 önkoşul hedefiyle 72 dakika 18 saniyede tamamlandı. `PIPESTATUS=(0 0)`, son satır `Tum testler gecti!`. Aynı geçici imaj adını kullanan eski UEFI/PS2/güç/blok hedefleri yerel kabuk kilidiyle sıraya alındı. Diğer hedefler normal paralellikte koştu. Teslim dalındaki iki rapor dışındaki 61 değişen dosya doğrulama kopyasıyla bayt bayt eşleşti.

| Dalga 15 kapısı | Başarılı QEMU açılışı, iki derleyici toplamı |
|---|---:|
| Sağlamlık | 10 |
| Donanım | 4 |
| Günlük | 2 |
| PC matrisi | 72 |
| Bozuk ELF | 2 |
| Giriş | 4 |
| Kullanıcı programları | 8 |
| Boru ve yönlendirme | 8 |
| Açılış süresi | 10 |
| Belgeler | 2 |
| **Toplam** | **122** |

### Mevcut atlamalar

| Kapı | Atlanan girdi |
|---|---:|
| ASan uçtan uca | 45 |
| Geniş codegen | 8 |
| Semantik codegen, yabancı mimari ve bağlanamayan iki çapraz modül | 4 |
| Sürücü paritesi | 1 |
| Yapısal IR paritesi | 2 |
| Sürücü host-mock | 5 |
| Bölge operandı paritesi | 4 |
| ARM konuk üst bellek deneyi | 1 |
| **Ana kapı toplamı** | **70** |

Dalga 15 QEMU kapılarında atlama sıfır. Bir girdinin detay satırı ve aynı kapının özeti tek sayıldı. Self-driver aynı semantik korpusu iki sürücüyle ayrıca çalıştırdı. ASan 4 bilinen sonuç ve 2 sızıntı muafiyeti, yapısal IR 24, sürücü paritesi 1, geniş tip kontrolü 18 ve genel tip kapısı 21 mevcut muafiyet bildirdi. Yeni muafiyet sayısı sıfır. Uyarı kapısı 38 C dosyasında sıfır uyarı verdi. Tam takım sonrasında sınıflandırma ve bare-metal parite tekrar rc=0 döndürdü.

## Dosya değişiklikleri

D-764 ile teslim arasındaki izin listesi denetlendi.

```text
Makefile
belgeler/KEMGU_OS_Donanim_Yol_Haritasi.md
belgeler/KEMGU_OS_Klavye_Duzeni.md
belgeler/KEMGU_OS_Mimari.md
belgeler/KEMGU_OS_Program_Yazma.md
belgeler/KEMGU_OS_Sabotaj_Envanteri.md
belgeler/KEMGU_OS_Syscall_ABI.md
belgeler/X86_Fiziksel_Donanim_Kontrol_Listesi.md
drivers/acilis/uefi_harita.kem
drivers/ekran/bmp.kem
drivers/ps2/klavye_tr.kem
drivers/uygulama/bul.kem
drivers/uygulama/hesapla.kem
otomasyon/chatgpt/codex15_bulgular.md
otomasyon/chatgpt/codex15_ilerleme.md
runtime/kullanici/x86_64/kem_sis.kem
runtime/mimari/x86_64/kem_acpi_uefi.kem
runtime/mimari/x86_64/kem_apic_hw.kem
runtime/mimari/x86_64/kem_gc_yurut.kem
runtime/mimari/x86_64/kem_pci_hw.kem
runtime/mimari/x86_64/kem_ps2_hw.kem
runtime/mimari/x86_64/kem_surec_hw.kem
runtime/mimari/x86_64/kem_uart.kem
test/bm_x86_surec_harness.sh
test/kullanici/boruden.kem
test/kullanici/bul.kem
test/kullanici/dosyalar.kem
test/kullanici/duzenle.kem
test/kullanici/echo.kem
test/kullanici/goster.kem
test/kullanici/hesapla.kem
test/kullanici/kedi.kem
test/kullanici/saat.kem
test/kullanici/say.kem
test/ornekler/kem_os_x86_blok.kem
test/ornekler/kem_os_x86_blok_ortak.kem
test/ornekler/kem_os_x86_guc.kem
test/ornekler/kem_os_x86_kabuk.kem
test/ornekler/kem_os_x86_ps2.kem
test/ornekler/kem_os_x86_vfs_ortak.kem
tests/drivers/bulanik/acpi_test.kem
tests/drivers/bulanik/bmp_test.kem
tests/drivers/bulanik/bolum_test.kem
tests/drivers/bulanik/bolum_uyum_test.kem
tests/drivers/bulanik/elf_test.kem
tests/drivers/bulanik/fadt_test.kem
tests/drivers/bulanik/madt_test.kem
tests/drivers/uygulama/program_test.kem
tests/qemu/acilis_suresi.py
tests/qemu/belge15_kahin.py
tests/qemu/boru_kahin.py
tests/qemu/bozuk_elf_kahin.py
tests/qemu/dalga12_kahin.py
tests/qemu/donanim_kahin.py
tests/qemu/girdi15_kahin.py
tests/qemu/kabuk14_kahin.py
tests/qemu/log_kahin.py
tests/qemu/pc_matris.py
tests/qemu/program15_kahin.py
tests/qemu/ps2_irq_kahin.py
tests/qemu/saglamlik_kahin.py
tests/qemu/surec_kahin.py
tools/codex/e2.py
```
