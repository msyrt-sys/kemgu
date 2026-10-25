# KEMGU-OS — Gerçek Donanım Yol Haritası

**Hedef (bitiş tanımı):** Gerçek bir x86_64 PC'de USB bellekten açılan, ekranda
Türkçe metin gösteren, **klavyeden** (Türkçe Q) yazı alan, **fareyle** imleç
oynatıp tıklanabilen ve **diskteki bir dosya sistemine** dosya yazıp okuyabilen
KEMGU-OS. Seri port ve ikinci makine GEREKMEZ.

Bu belge bir iş listesidir. Her madde: **kimlik · büyüklük · bağımlılık · kanıt**.
Büyüklük: `[S]` küçük (tek PR, birkaç saat) · `[M]` orta (1–3 PR) ·
`[L]` büyük (çok PR) · `[K]` karar (Mehmet). Kanıtı olmayan madde bitmiş sayılmaz.

> Disiplin (depo kuralları, her maddeye uygulanır): her yeni yeteneğe bir QEMU
> kapısı + bir **sabotaj** (yeteneği boz → kapı KIRMIZI olmalı) + mümkünse bir
> **pozitif** şekil. Kapı `test_tumu`ya bağlanmadıkça madde bitmiş sayılmaz
> (D-446: çağrılmayan kapı, olmayan kapıdan tehlikelidir). Donanım bekleyişlerinin
> HEPSİ zaman aşımlıdır — asılan çekirdek, sessiz çekirdek kadar kötüdür.

---

## 0. Başlangıç durumu (2026-10-25 ölçüldü)

| Alan | Bugün | Gerçek PC'de |
|---|---|---|
| Açılış | PVH (`pvh_entry`, `linker/bare-metal-x86_64.ld`) | ❌ PVH yalnız QEMU/Xen; firmware bilmez |
| Makine | QEMU `-M microvm` | ❌ gerçek PC "pc"/q35 sınıfı |
| Konsol | COM1 16550 (`0x3F8`, `kem_uart.kem`) | ⚠ masaüstünde başlık olabilir, dizüstünde yok |
| Ekran | YOK | ❌ |
| Zamanlayıcı | PIT (`kem_zaman_hw.kem`) | ✅ PIT çoğu PC'de var (HPET/LAPIC daha sağlam) |
| Kesme | PIC + LAPIC sürüm okuma | ⚠ IOAPIC yönlendirmesi yok |
| Bellek | Sayfalama, çeviri, fault, W^X, izolasyon | ✅ taşınabilir |
| Kullanıcı kipi | ring3, syscall, süreç izolasyonu (x86 + arm64) | ✅ taşınabilir |
| Disk | virtio-blk (virtio-**mmio**) | ❌ virtio-mmio yalnız microvm'de |
| Ağ | virtio-net (virtio-mmio) | ❌ |
| Dosya sistemi | `kem_minifs`: TEK dosya, 4 harfli ad, ≤512 bayt | ❌ oyuncak |
| Klavye / fare | YOK (yalnız seri RX) | ❌ |
| PCI | eski C testi `pci_enum_x86.c` var, `.kem`de yok | ❌ |
| ACPI | YOK | ❌ |
| Port G/Ç | `kem_port_oku8/yaz8` (satıriçi asm) | ✅ temel hazır |

Toplam çekirdek+runtime `.kem`: ~5.100 satır. Hedefe varınca kabaca **6–8 kat**
büyümesi beklenir → derleme altyapısı (H1) önemlidir.

---

## H0. Kararlar (Mehmet) — işe başlamadan

- [ ] **H0a [K] Birincil donanım hedefi.** Öneri: **x86_64 PC** (eski BIOS'lu
      masaüstü ilk, sonra modern UEFI). ARM64 (Spark/RPi) ikinci hat; D-490 bariyer
      doğrulaması ayrı kalır (x86-TSO onu kanıtlamaz).
- [ ] **H0b [K] Açılış yolu sırası.** Öneri: önce **GRUB + multiboot2** (BIOS ve UEFI'de
      aynı başlık), sonra kendi UEFI stub'ı (isteğe bağlı).
- [ ] **H0c [K] Dosya sistemi.** Öneri: **FAT32** (her işletim sistemi okur/yazar →
      dosyaları Linux'tan koyup çıkarabilirsin, test oracle'ı bedava). Kendi
      günlüklü FS'i (KEMFS) sonraki aşama.
- [ ] **H0d [K] "Fare çalışıyor"un tanımı.** Öneri: ekranda imleç + tıklama olayı +
      fareyle kullanılabilen EN AZ bir program (dosya listesinde tıkla-aç, basit çizim).
      Tam pencere yöneticisi KAPSAM DIŞI (ayrı yol haritası).
- [ ] **H0e [K] USB kapsamı.** Öneri: önce PS/2 (QEMU + eski BIOS'un USB→PS/2
      öykünmesi), sonra xHCI + HID. USB disk (mass storage) isteğe bağlı.
- [ ] **H0f [K] E3a'yı kapat: "v1 kullanıcı alanı neye denir?"** — bu belgenin
      bitiş tanımı önerilir.
- [ ] **H0g [K] Test makinesi profili.** Elde hangi makineler var (BIOS/UEFI, PS/2
      portu, SATA/NVMe, seri başlık)? Bu, H4/H6/H9 sırasını değiştirir.

---

## H1. Dil / derleyici ön koşulları

Çekirdek büyüdükçe bugünkü "her şeyi `cat` ile tek dosyaya birleştir" yolu darboğaz olur.

- [ ] **H1a [S] Ölç:** `kem_os_x86` derleme süresi + IR boyutu; 20k satırlık
      sentetik çekirdekte aynısı. Darboğaz gerçekse H1b zorunlu, değilse ertelenir.
- [ ] **H1b [L] Ayrık derleme (LOOP D7a–f).** Sürücü başına derleme birimi,
      imza-only arayüz dosyaları, artımlı derleme. Kanıt: tek sürücü değişince
      yalnız o birim yeniden derlenir.
- [ ] **H1c [M] Çekirdek bellek geri dönüşü (LOOP D6/D9).** Sürücü tamponları,
      dosya önbelleği, süreç yığınları serbest bırakılabilmeli. Ölç: `kem_heap`
      bugün `free` destekliyor mu, parçalanma? Kanıt: 10.000 aç-kapat döngüsünden
      sonra heap kullanımı sabit.
- [ ] **H1d [M] `yetki<R>` ödünç alma (LOOP D2a–e).** Her sürücü kendi
      `yetki<MMIO>`/port yetkisini taşıyacak; yetkiyi tüketmeden çağrılara vermek şart.
- [ ] **H1e [S] Ölç: paketlenmiş (packed) yapı + sabit yerleşim.** ACPI tabloları,
      AHCI komut başlıkları, FAT BPB bayt-hizasızdır. Yoksa: ham bayt okuma
      yardımcıları (`oku16le/oku32le`) yeter mi, yoksa `yapı` için `paketli` gerekir mi?
- [ ] **H1f [S] Port G/Ç genişletme.** `kem_port_oku16/32`, `yaz16/32` (PCI config,
      ATA, PS/2 hepsi ister). Kanıt: QEMU'da PCI vendor ID okunuyor.
- [ ] **H1g [M] Kesme işleyici çerçevesi.** IDT girişi → `.kem` işleyiciye köprü
      genelleştirilmeli (bugün IRQ0/timer'a özel). Vektör tablosu, EOI, iç içe kesme
      yasağı. Kanıt: 3 farklı IRQ aynı çerçeveden geçiyor.
- [ ] **H1h [S] `volatile` anlamı denetimi.** MMIO okumalarının optimize edilip
      silinmediğini ölç (`-O2`de durum yazmacı döngüsü). Sabotaj: bariyer/asm
      bozulan listesini kaldır → test asılmalı/kırmızı olmalı.

---

## H2. Gerçek PC açılışı (QEMU `-M pc` / `q35` üzerinde)

- [ ] **H2a [M] Multiboot2 başlığı + 32→64 bit giriş.** Mevcut `pvh_entry` uzun-kip
      kurulumu yeniden kullanılır; multiboot2 sihirli sayı + bilgi yapısı işaretçisi.
- [ ] **H2b [S] GRUB ISO üretici** (`grub-mkrescue`; yoksa açık hata). Makefile hedefi
      `kem_os_pc.iso`.
- [ ] **H2c [M] Multiboot2 bilgi etiketleri:** bellek haritası (E820 karşılığı),
      framebuffer etiketi, modül etiketi (ramdisk), komut satırı, ACPI RSDP etiketi.
      Kanıt: seri çıktıda bellek bölgeleri listesi.
- [ ] **H2d [M] Fiziksel bellek yöneticisi bellek haritasından** (bugünkü sabit
      yerleşim yerine). Ayrılmış bölgeler, çekirdek imajı, modüller korunur.
      Kanıt: 512 MB / 2 GB / 4 GB RAM ile açılış.
- [ ] **H2e [M] GDT + TSS + IDT tam kurulumu** (çift hata için ayrı IST yığını).
      Kanıt: kasıtlı #PF, #GP, #DF → panik ekranı, asılma yok.
- [ ] **H2f [S] Kapı `calistir_kem_os_pc`:** QEMU `-M pc -cdrom kem_os_pc.iso
      -serial file:...`, mevcut faz dizgileri. Sabotaj: multiboot sihirli sayıyı boz
      → GRUB yüklemiyor → kapı kırmızı.
- [ ] **H2g [S] `q35` ikizi** (modern yonga seti; AHCI yerleşik, PCIe ECAM).

---

## H3. ACPI ve kesme yönlendirme

- [ ] **H3a [M] RSDP bul** (multiboot2 etiketi; yoksa EBDA/0xE0000 taraması) +
      sağlama toplamı doğrulama.
- [ ] **H3b [M] RSDT/XSDT gezgini** + tablo imzası listesi. Kanıt: seri/ekranda
      `APIC FACP HPET MCFG` listesi.
- [ ] **H3c [M] MADT:** LAPIC adresi, IOAPIC(ler), kesme kaynağı geçersiz kılmaları
      (ISA IRQ0 → GSI2 gibi). Sabotaj: override'ı yok say → timer gelmiyor → kırmızı.
- [ ] **H3d [M] IOAPIC sürücüsü:** yönlendirme tablosu, maske, kenar/seviye, kutup.
      8259 PIC tamamen maskelenir.
- [ ] **H3e [M] LAPIC zamanlayıcı** (PIT ile kalibre). PIT'e bağımlılık biter.
- [ ] **H3f [S] FADT:** `8042` bayrağı (PS/2 var mı?), sıfırlama yazmacı, PM zamanlayıcı.
- [ ] **H3g [M] Kapatma/yeniden başlatma:** önce FADT reset yazmacı, sonra 8042
      `0xFE`, sonra üçlü hata. Tam ACPI kapatma (S5) **AML yorumlayıcısı** ister →
      **KAPSAM DIŞI**; QEMU için `isa-debug-exit`, gerçek PC'de "güç tuşuna bas" mesajı.
- [ ] **H3h [S] Not: AML'den kaçınma stratejisi.** PCI kesmeleri için `_PRT` (AML)
      yerine **MSI/MSI-X** kullanılır (H6d) — tam ACPI yorumlayıcısı yazmamak için
      bilinçli seçim.

---

## H4. Ekran: framebuffer konsolu

- [ ] **H4a [M] Framebuffer etiketi** (GRUB `gfxpayload`; UEFI GOP ile AYNI veri biçimi
      → tek kod yolu). Piksel biçimi (RGB/BGR, 32/24 bpp), pitch.
- [ ] **H4b [S] Framebuffer belleğini yazma-birleştirmeli/önbelleksiz eşle** (PAT).
      Ölç: önbellekli eşlemede ekranda artık görüntü kalıyor mu.
- [ ] **H4c [M] Bit eşlem yazı tipi** (8×16, PSF2 biçimi gömülü). **Türkçe
      karakterler zorunlu:** ç Ç ğ Ğ ı İ ö Ö ş Ş ü Ü + ASCII + kutu çizim.
      Kanıt: tüm Türkçe harfler ekranda doğru.
- [ ] **H4d [M] Metin konsolu:** imleç, satır sonu, kaydırma, sekme, renk (ANSI
      alt kümesi), UTF-8 çözme (yarım bayt dizisi → `?`, çökme yok).
- [ ] **H4e [S] Çift çıktı:** her `yazdir` hem seriye hem ekrana; seri yoksa yalnız ekran.
- [ ] **H4f [S] Panik ekranı:** kırmızı zemin, panik mesajı, son 20 log satırı,
      yazmaç dökümü. (Seri yokken tek teşhis aracı budur.)
- [ ] **H4g [M] Kapı: ekran görüntüsü karşılaştırması.** QEMU monitörü `screendump`
      → PPM → bilinen bölgenin piksel özeti. Sabotaj: yazı tipinde `ğ` glifini boz → kırmızı.
- [ ] **H4h [S] Performans:** tam ekran kaydırma süresi ölç (çift tampon gerekiyor mu?).
- [ ] **H4i [S] Çekirdek log halkası** (son N KB) + ekranda `log` komutu.

---

## H5. Klavye (PS/2)

- [ ] **H5a [M] 8042 denetleyici başlatma:** FADT bayrağına bak, öz-test (`0xAA`),
      port testi, çeviri modu kararı, her bekleyişte zaman aşımı.
- [ ] **H5b [S] IRQ1 → IOAPIC → işleyici**, tarama kodu kuyruğu (kesme-güvenli halka).
- [ ] **H5c [M] Tarama kodu kümesi 1 çözücü** (`E0` genişletilmiş, `E1` Pause),
      bas/bırak durumları.
- [ ] **H5d [M] Değiştirici tuş durum makinesi:** Shift, Ctrl, Alt, **AltGr**,
      Caps/Num/Scroll Lock + LED güncelleme komutu (`0xED`).
- [ ] **H5e [M] Türkçe Q klavye düzeni** (asıl iş): `ı İ ğ Ğ ü Ü ş Ş ö Ö ç Ç`,
      AltGr katmanı (`@ € { [ ] } \ | ~ #`), **Caps Lock'ta i→İ, ı→I** (Türkçe büyük
      harf kuralı — `metin_buyuk_tr` ile tutarlı), ölü tuşlar gerekmez (Türkçe Q'da yok).
      Ayrıca ABD düzeni (geçiş: Ctrl+Boşluk). Kanıt: her tuş için beklenen UTF-8 tablosu.
- [ ] **H5f [S] Tuş tekrarı** (yazılım tarafı; donanım tekrar hızını ayarla `0xF3`).
- [ ] **H5g [M] Olay modeli:** `TusOlayi { kod, unicode, bas/bırak, değiştiriciler }`;
      çekirdekten kullanıcı alanına kanal (`kanal<TusOlayi>` ya da syscall).
- [ ] **H5h [M] Konsolda satır düzenleme:** geri sil (UTF-8 çok baytlı karakteri TEK
      silmeli), sol/sağ ok, Home/End, geçmiş (yukarı/aşağı), Ctrl+C (süreç sonlandırma).
- [ ] **H5i [M] Kapı:** QEMU monitörü `sendkey` ile dizi gönder (`s h i f t - i` gibi)
      → kabuk yankısı beklenen metin (`İ`!). Sabotaj: AltGr eşlemesini boz → `@` gelmez → kırmızı.
- [ ] **H5j [S] Pozitif + kenar şekiller:** iki tuşa aynı anda basma, çok hızlı yazma
      (kuyruk taşması → kayıp olay SAYILIR ve bildirilir, çökme yok).

---

## H6. Fare (PS/2 aux) + imleç

- [ ] **H6a [M] Aux port etkinleştirme** (`0xA8`, `0xD4` öneki), varsayılanlar,
      veri raporlama (`0xF4`).
- [ ] **H6b [M] IRQ12 + 3 baytlık paket ayrıştırıcı** + **senkron kurtarma**
      (bit 3 her zaman 1 olmalı; kayarsa yeniden hizala).
- [ ] **H6c [S] IntelliMouse tekerlek** (200-100-80 örnekleme sihirli dizisi → 4 bayt paket).
- [ ] **H6d [S] Hızlanma eğrisi + hassasiyet + ekran sınırına kenetleme.**
- [ ] **H6e [M] İmleç çizimi:** altını kaydet/geri yükle (save-under), titreme yok,
      metin konsoluyla çakışmada doğru yeniden çizim.
- [ ] **H6f [M] Fare olayları:** hareket, bas/bırak (sol/sağ/orta), tekerlek,
      çift tıklama (zaman eşiği); kullanıcı alanına kanal.
- [ ] **H6g [M] Kapı:** QEMU monitörü `mouse_move` / `mouse_button` → olay günlüğü
      + imleç konumu ekran görüntüsünde. Sabotaj: senkron kurtarmayı kaldır, paket
      kaydır → imleç sapar → kırmızı.
- [ ] **H6h [S] QEMU USB tablet (mutlak konum) NOT:** QEMU'da `usb-tablet` daha rahat
      ama gerçek PC'de yok; kapılar PS/2 fareyi ölçer.

---

## H7. PCI ve depolama

- [ ] **H7a [M] PCI yapılandırma alanı:** önce legacy `0xCF8/0xCFC` (H1f'e bağlı),
      sonra ECAM (ACPI MCFG). `pci_enum_x86.c` mantığı `.kem`e taşınır.
- [ ] **H7b [M] Aygıt numaralandırma + sınıf kodları + köprüler** (veri yolu tarama).
      Ekranda `lspci` benzeri çıktı (gerçek donanımda teşhis için ŞART).
- [ ] **H7c [M] BAR boyutlandırma + MMIO eşleme** (önbelleksiz), 64-bit BAR.
- [ ] **H7d [M] MSI / MSI-X** (H3h gereği INTx yerine). Kanıt: AHCI kesmesi MSI ile geliyor.
- [ ] **H7e [L] AHCI sürücüsü:** HBA sıfırlama, port başlatma, IDENTIFY, komut
      listesi + FIS + PRD tabloları, 48-bit LBA okuma/yazma DMA, hata kurtarma
      (port sıfırlama), zaman aşımları. Kanıt: QEMU `-device ahci` + imaj, sektör
      yaz/oku gidiş-dönüşü + SHA256.
- [ ] **H7f [M] Blok katmanı:** blok aygıtı soyutlaması (AHCI/ramdisk/virtio/NVMe aynı
      arayüz), sektör önbelleği (LRU + kirli bit), istek kuyruğu.
- [ ] **H7g [M] Bölüm tabloları:** MBR + GPT (CRC32 doğrulama — `[30] CRC32 OK` zaten var).
      Kanıt: GPT'li imajda doğru bölüm bulunuyor; bozuk GPT başlığı → yedek başlık.
- [ ] **H7h [S] Ramdisk (multiboot2 modülü)** — H7e beklemeden dosya sistemi işine
      başlamak için ara yol.
- [ ] **H7i [L] NVMe sürücüsü** (modern makinelerin çoğu): admin + I/O kuyrukları,
      IDENTIFY, okuma/yazma. Kanıt: QEMU `-device nvme`.
- [ ] **H7j [S] Kapı matrisi:** ahci / nvme / ramdisk × MBR / GPT.
      Sabotaj: PRD bayt sayısını bir eksik yaz → SHA256 tutmaz → kırmızı.

---

## H8. Dosya sistemi

- [ ] **H8a [M] VFS katmanı:** düğüm, açık dosya, dosya tanımlayıcı tablosu (LOOP E3d),
      yol çözümleme (`/`, `.`, `..`), bağlama noktası (`/` = FAT bölümü, `/ram`).
- [ ] **H8b [M] FAT32 okuma:** BPB doğrulama, FAT zinciri, kök/alt dizin, 8.3 adlar.
- [ ] **H8c [M] Uzun dosya adı (LFN):** UTF-16 → UTF-8; **Türkçe ad** (`günlük.txt`).
      Sağlama toplamı denetimi.
- [ ] **H8d [L] FAT32 yazma:** küme ayırma (FSInfo ipucu), iki FAT kopyasını da
      güncelleme, dizin girişi oluşturma/silme, LFN üretme + 8.3 kısa ad çakışması,
      dosya büyütme/küçültme, `mkdir`/`rmdir`/`rename`.
- [ ] **H8e [M] Senkron + kirli bayrak:** `sync`, bağlarken kirli-kapanma bayrağı
      kontrolü, basit `fsck` (kayıp küme, çapraz bağlı zincir raporu).
- [ ] **H8f [M] Çapraz doğrulama kapısı (en güçlü oracle):** ana makinede
      `mkfs.fat` + `mtools` ile imaj hazırla → KEMGU-OS okur; KEMGU-OS'un yazdığı
      dosyaları ana makinede `mtype`/`fsck.fat -n` doğrular. Sabotaj: ikinci FAT
      kopyasını güncelleme → `fsck.fat` uyarır → kırmızı.
- [ ] **H8g [M] Bozuk imaj dayanıklılığı (GÜVENLİK):** rastgele bozulmuş 1.000 FAT
      imajında çekirdek **asla** çökmez / asılmaz / sınır dışı okumaz (zincir
      döngüsü tespiti, küme numarası aralık denetimi). KEMGU'nun "çökmezlik" vaadinin
      sürücü katmanındaki kanıtı.
- [ ] **H8h [S] Bellek: dosya önbelleği H1c ile geri verilir.** Kanıt: 1 GB dosya
      okununca heap sabit kalıyor.
- [ ] **H8i [L] (İsteğe bağlı, sonraki aşama) KEMFS:** günlüklü kendi FS'imiz
      (eski `fs_journal`/`crashfs` C testleri referans). Çökme sonrası tutarlılık kapısı.

---

## H9. Kullanıcı alanı: kabuk, programlar, kanıt uygulamaları

- [ ] **H9a [M] Dosya syscall'ları:** `ac/oku/yaz/kapat/konumla/bilgi/dizin_oku/
      dizin_olustur/sil/yeniden_adlandir`. Kullanıcı işaretçisi doğrulaması
      (`[28] SYSCALL PTR DOGRULAMA OK` deseni) HER birinde.
- [ ] **H9b [M] Olay syscall'ları:** klavye olayı bekle, fare olayı bekle,
      zaman aşımlı bekleme (seçici: `bekle(olaylar, ms)`).
- [ ] **H9c [M] ELF'i dosya sisteminden yükle** (`[18] ELF YUKLE OK` arm64'te var →
      x86 + VFS'e bağla). `/bin/*` programları.
- [ ] **H9d [M] Süreç modeli (LOOP E3b/E3c):** çok süreç, `baslat(yol, argv)`,
      `bekle`, çıkış kodu, hata olunca süreci öldür (çekirdek yaşar).
- [ ] **H9e [L] Kabuk (LOOP E3e):** `ls cd pwd cat yaz sil mkdir rmdir mv cp temizle
      saat bellek lspci log yeniden_baslat`, tırnak, yönlendirme (`>`), geçmiş,
      sekme ile tamamlama (dosya adı).
- [ ] **H9f [M] Metin düzenleyici `duzenle`** (nano benzeri: aç, imleç, yaz,
      Ctrl+S kaydet, Ctrl+Q çık) — **klavye + FS'in birlikte kanıtı.**
- [ ] **H9g [M] Fare kanıt programları:** (1) `dosyalar`: fareyle dosya seç,
      çift tıkla `duzenle`de aç; (2) `ciz`: fareyle çiz, kaydet (BMP → FS'e).
      **Klavye + fare + FS'in birlikte kanıtı.**
- [ ] **H9h [S] Saat:** RTC (`rtc_x86.c` referans) → `saat` komutu + dosya zaman damgaları.
- [ ] **H9i [M] Kapı: uçtan uca senaryo** (QEMU, `sendkey` + `mouse_*` betiği):
      aç → `duzenle not.txt` → Türkçe metin yaz → kaydet → yeniden başlat → `cat` →
      aynı metin; ana makinede `mtype` ile de aynı metin. Sabotaj: `sync`i kaldır → kırmızı.
- [ ] **H9j [S] Süreç izolasyonu kapısı (LOOP E3f):** kötü niyetli program diğerinin
      belleğini/dosya tanımlayıcısını okuyamaz.

---

## H10. Gerçek donanım uyumu

- [ ] **H10a [M] UEFI açılışı (GRUB EFI):** multiboot2 aynı; GOP framebuffer; UEFI
      bellek haritası; ExitBootServices sonrası. Kapı: QEMU + OVMF.
- [ ] **H10b [S] Secure Boot kapalı gereksinimi belgelenir** (imza zinciri kapsam dışı).
- [ ] **H10c [L] xHCI (USB 3) sürücüsü:** denetleyici sıfırlama, komut/olay/aktarım
      halkaları, port sıfırlama + hız, cihaz adresleme, yapılandırma tanımlayıcıları.
      Neden gerekli: modern makinelerin çoğunda PS/2 YOK ve UEFI'de PS/2 öykünmesi
      çoğunlukla kapalı. Kapı: QEMU `-device qemu-xhci`.
- [ ] **H10d [M] USB HID boot protokolü:** klavye (8 baytlık rapor → H5 düzen
      katmanına aynı olay) + fare (göreli rapor → H6 katmanına aynı olay).
      Kapı: QEMU `usb-kbd`, `usb-mouse`.
- [ ] **H10e [S] USB hub desteği** (çoğu dizüstünde dahili cihazlar hub arkasında).
- [ ] **H10f [L] (İsteğe bağlı) USB yığın depolama (BOT + SCSI)** — dosya sistemi
      açılış USB belleğinde olsun istenirse. Yoksa iç SATA/NVMe bölümü kullanılır.
- [ ] **H10g [M] Sağlamlık turu:** her donanım bekleyişinde zaman aşımı + net hata
      mesajı (hangi aygıt, hangi yazmaç); eksik aygıtta zarif düşüş (PS/2 yoksa USB'ye,
      AHCI yoksa NVMe'ye, hiç disk yoksa ramdisk).
- [ ] **H10h [M] Teşhis paketi:** açılışta `donanim.txt` (PCI listesi, ACPI tabloları,
      bellek haritası, bulunan/bulunamayan aygıtlar) → FS'e yazılır. Seri port yokken
      sorun bildirmenin tek yolu.
- [ ] **H10i [S] SMP (isteğe bağlı):** LAPIC INIT-SIPI (`smp_x86.c` referans). Tek
      çekirdekle de hedef karşılanır; ertelenebilir.

---

## H11. Donanım koşumu (Mehmet + ben)

- [ ] **H11a [S] USB imaj üretici:** `make kem_os_usb.img` (GPT + EFI bölümü + BIOS
      GRUB + FAT32 veri bölümü + örnek dosyalar). `dd` komutu belgede, cihaz adı ASLA
      betikte sabit değil.
- [ ] **H11b [S] Kontrol listesi** (`belgeler/X86_Fiziksel_Donanim_Kontrol_Listesi.md`,
      ARM64 listesinin deseni): BIOS ayarları (CSM/Legacy, Secure Boot kapalı, SATA AHCI
      kipi), beklenen ekran çıktıları, başarısızlıkta ne fotoğraflanacak.
- [ ] **H11c [M] Makine 1 — eski BIOS'lu masaüstü** (PS/2 + SATA): tam senaryo H9i.
- [ ] **H11d [M] Makine 2 — modern UEFI masaüstü** (USB klavye/fare + NVMe).
- [ ] **H11e [M] Makine 3 — dizüstü** (hub arkasında USB, dokunmatik yüzey PS/2
      öykünmesi ya da I²C-HID → I²C-HID KAPSAM DIŞI, harici fare kullanılır).
- [ ] **H11f [S] Sonuç kaydı:** her makine için çalışan/çalışmayan aygıt tablosu +
      `donanim.txt` + ekran fotoğrafları depoya.

---

## H12. Kalite kapıları ve bakım

- [ ] **H12a [M] QEMU kapı matrisi `calistir_kem_os_pc_matris`:** {BIOS, UEFI} ×
      {PS/2, USB} × {AHCI, NVMe}. `test_tumu`ya süreye göre ALT KÜME bağlanır
      (D-453 deseni: temsilciler her koşumda, tam matris haftalık/CI).
- [ ] **H12b [M] Bulanıklaştırma:** bozuk FAT imajları (H8g), bozuk ACPI tabloları,
      saçma PS/2 paketleri, bozuk USB tanımlayıcıları → çekirdek asla çökmez.
- [ ] **H12c [S] Sabotaj envanteri:** her H maddesinin sabotajı tek tabloda (S-numaralı).
- [ ] **H12d [S] Açılış süresi ölçümü** (GRUB → kabuk istemi) ve gerilemeyi yakalayan eşik.
- [ ] **H12e [S] Belgeler:** sürücü mimarisi, syscall tablosu, klavye düzeni tablosu,
      "yeni sürücü nasıl eklenir".

---

## Bağımlılık özeti (kritik yol)

```
H0 kararlar
 └─ H1f/H1g (port G/Ç, kesme çerçevesi)
     └─ H2 (multiboot2 + GRUB + bellek haritası)
         ├─ H3 (ACPI + IOAPIC + LAPIC timer)
         │   ├─ H5 klavye ─┐
         │   ├─ H6 fare  ──┤
         │   └─ H7 PCI → AHCI/NVMe → H8 FAT32 ──┐
         └─ H4 ekran ──────┴─────────────────────┴─ H9 kabuk + duzenle + dosyalar
                                                       └─ H10 UEFI + xHCI/HID
                                                           └─ H11 GERÇEK DONANIM ✅
H1b/H1c/H1d (ayrık derleme, bellek geri dönüşü, yetki ödünç) paralel yürür;
H8/H9 başlamadan H1c bitmiş olmalı (dosya önbelleği bellek geri ister).
```

## Ara kilometre taşları (her biri tek başına gösterilebilir)

1. **K1 — "Ekranda merhaba":** QEMU `-M pc`, GRUB, framebuffer'da Türkçe metin (H2+H4).
2. **K2 — "Yazabiliyorum":** PS/2 klavyeden Türkçe yazı, kabukta yankı (H3+H5).
3. **K3 — "İmleç oynuyor":** fare imleci + tıklama (H6).
4. **K4 — "Dosya kalıcı":** AHCI + FAT32'ye yazılan dosya yeniden başlatmadan ve
   Linux'tan okunuyor (H7+H8).
5. **K5 — "Kullanılabilir":** `duzenle` + `dosyalar` QEMU'da uçtan uca (H9).
6. **K6 — "Modern PC":** UEFI + USB klavye/fare QEMU'da (H10).
7. **K7 — "Gerçek donanım":** eski PC'de K5, modern PC'de K6 (H11). **BİTİŞ.**

## Bilinçli olarak kapsam dışı

Tam ACPI/AML yorumlayıcısı · pencere yöneticisi / GUI araç takımı · ses · ağ
(virtio dışında gerçek NIC sürücüsü: e1000/rtl8169 sonraki yol haritası) · GPU
hızlandırma · I²C-HID dokunmatik yüzey · Wi-Fi/Bluetooth · Secure Boot imzalama ·
ARM64 donanım hattı (ayrı: E1 + D-490).
