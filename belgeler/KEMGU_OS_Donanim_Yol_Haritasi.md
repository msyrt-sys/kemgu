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

- [x] **H0a [K] ✓ ÖNERİ UYGULANDI (2026-11-06).** Birincil donanım hedefi.** Öneri: **x86_64 PC** (eski BIOS'lu
      masaüstü ilk, sonra modern UEFI). ARM64 (Spark/RPi) ikinci hat; D-490 bariyer
      doğrulaması ayrı kalır (x86-TSO onu kanıtlamaz).
- [x] **H0b [K] ✓ ÖNERİ UYGULANDI (2026-11-06).** Açılış yolu sırası.** Öneri: önce **GRUB + multiboot2** (BIOS ve UEFI'de
      aynı başlık), sonra kendi UEFI stub'ı (isteğe bağlı).
      ⚠ SIRA DEĞİŞTİ (2026-11-12, ÖLÇÜM): DGX Spark (ARM64 Ubuntu) üzerinde x86 GRUB platformları
      KURULAMIYOR (`/usr/lib/grub/` yalnız arm64-efi; grub-pc-bin yok, grub-efi-amd64-bin aday yok).
      OVMF + clang x86_64-windows + lld-link zinciri uçtan uca doğrulandı (EFI uygulaması OVMF'de
      açılıp seriye yazdı) → önce KENDİ UEFI STUB'I (Codex dalga 10), BIOS/GRUB sonra (amd64
      çoklu-mimari depo ya da yalnız CI'da x86 koşucusu — Mehmet'in kararı).
      ✓ KARAR (2026-11-13, D-756, Opus önerisi): **BIOS/GRUB v1'DEN ÇIKTI, ERTELENDİ.** v1 yolu
      yalnız UEFI (kendi stub'ımız, D-755). Gerekçe: v1 bitiş tanımı (H0f) "USB'den açılır" der,
      BIOS şart koşmaz; test makinesi sıradan bir UEFI dizüstü/masaüstü (H0g); amd64 araçlarını
      ARM64'te öykünmek kırılgan. Eski bir BIOS makinesi gerçekten gerekirse yalnız CI'daki x86
      koşucusunda açılır. Etkilenen maddeler (H2a/H2b/H2f/H2g, H8k, H10a, H11a) aşağıda işaretli.
- [x] **H0c [K] ✓ ÖNERİ UYGULANDI (2026-11-06).** Dosya sistemi.** Öneri: **FAT32** (her işletim sistemi okur/yazar →
      dosyaları Linux'tan koyup çıkarabilirsin, test oracle'ı bedava). Kendi
      günlüklü FS'i (KEMFS) sonraki aşama.
- [x] **H0d [K] ✓ ÖNERİ UYGULANDI (2026-11-06).** "Fare çalışıyor"un tanımı.** Öneri: ekranda imleç + tıklama olayı +
      fareyle kullanılabilen EN AZ bir program (dosya listesinde tıkla-aç, basit çizim).
      Tam pencere yöneticisi KAPSAM DIŞI (ayrı yol haritası).
- [x] **H0e [K] ✓ ÖNERİ UYGULANDI (2026-11-06).** USB kapsamı.** Öneri: önce PS/2 (QEMU + eski BIOS'un USB→PS/2
      öykünmesi), sonra xHCI + HID. USB disk (mass storage) isteğe bağlı.
- [x] **H0f [K] ✓ KARAR (2026-11-06): v1 BİTİŞ TANIMI.** Gerçek bir x86 PC USB bellekten
      açılır ve: (1) çerçeve tamponunda Türkçe metin konsolu, (2) Türkçe Q klavye,
      (3) fare imleci + tıklama, (4) açıldığı USB bellekte FAT32 okuma/yazma VE iç diskteki
      (SATA/NVMe) FAT32 bölümlerini bağlama — varsayılan salt okunur, H8j-H8m (2026-11-07), (5) `ls cd cat
      mkdir rm cp` kabuğu, (6) W^X + süreç yalıtımlı ELF kullanıcı programları, (7) en az
      iki program: metin düzenleyici + tıklanabilir dosya gezgini. Eski metin: E3a'yı kapat —
      bu belgenin
      bitiş tanımı önerilir.
- [ ] **H0g [K] Test makinesi profili — ERTELENDİ (2026-11-11, Mehmet).** Gerçek test H2 (USB'den açılış)
      + H4a (firmware çerçeve tamponu) QEMU+OVMF'de yeşil olunca. PS/2 GEREKMEZ: USB klavye (xHCI+HID+TR)
      zaten çalışıyor; sıradan bir x86 dizüstü/masaüstü (UEFI, NVMe ya da SATA) yeter, kurulumsuz USB
      bellekten açılır, iç diske yazılmaz. DGX Spark ARM olduğu için bu yolun makinesi değil.
      Eski öneri: legacy BIOS seçeneği + PS/2
      portu olan eski bir x86 masaüstü (ikinci el yeter); gelene kadar QEMU q35 + OVMF. Elde hangi makineler var (BIOS/UEFI, PS/2
      portu, SATA/NVMe, seri başlık)? Bu, H4/H6/H9 sırasını değiştirir.

---

## H1. Dil / derleyici ön koşulları

Çekirdek büyüdükçe bugünkü "her şeyi `cat` ile tek dosyaya birleştir" yolu darboğaz olur.

- [x] **H1a (✓ D-766/dalga 16 (calistir_derleme_olcum)) [S] Ölç:** `kem_os_x86` derleme süresi + IR boyutu; 20k satırlık
      sentetik çekirdekte aynısı. Darboğaz gerçekse H1b zorunlu, değilse ertelenir.
- [ ] **H1b (Kısmen test/bm_x86_uefi_harness.sh birimleri birleştiriyor. Tek sürücülük artımlı çekirdek derlemesi eksik.) [L] Ayrık derleme (LOOP D7a–f).** Sürücü başına derleme birimi,
      imza-only arayüz dosyaları, artımlı derleme. Kanıt: tek sürücü değişince
      yalnız o birim yeniden derlenir.
- [ ] **H1c (Kısmen D-763 süreç çerçevelerini geri veriyor. E4k için 10 bin çevrimde sabit heap kanıtı eksik.) [M] Çekirdek bellek geri dönüşü (LOOP D6/D9).** Sürücü tamponları,
      dosya önbelleği, süreç yığınları serbest bırakılabilmeli. Ölç: `kem_heap`
      bugün `free` destekliyor mu, parçalanma? Kanıt: 10.000 aç-kapat döngüsünden
      sonra heap kullanımı sabit.
- [x] **H1d (✓ D-739, calistir_capability_test ile çağrı süreli yetki ödüncü.) [M] `yetki<R>` ödünç alma (LOOP D2a–e).** Her sürücü kendi
      `yetki<MMIO>`/port yetkisini taşıyacak; yetkiyi tüketmeden çağrılara vermek şart.
- [x] **H1e [S] ✓ ÖLÇÜLDÜ (D-737): `paketli` GEREKMEZ** — Codex ACPI/FAT32/GPT/ELF/PCI/BMP'nin
      hepsini `drivers/ortak/bayt.kem` okuyucularıyla yazdı, dış kâhinlerle doğrulandı. Eski madde: ACPI tabloları,
      AHCI komut başlıkları, FAT BPB bayt-hizasızdır. Yoksa: ham bayt okuma
      yardımcıları (`oku16le/oku32le`) yeter mi, yoksa `yapı` için `paketli` gerekir mi?
- [x] **H1f (✓ D-766/dalga 16 (calistir_bm_x86_pci16)) [S] Port G/Ç genişletme.** `kem_port_oku16/32`, `yaz16/32` (PCI config,
      ATA, PS/2 hepsi ister). Kanıt: QEMU'da PCI vendor ID okunuyor.
- [x] **H1g (✓ D-766/dalga 16 (calistir_bm_x86_msi)) [M] Kesme işleyici çerçevesi.** IDT girişi → `.kem` işleyiciye köprü
      genelleştirilmeli (bugün IRQ0/timer'a özel). Vektör tablosu, EOI, iç içe kesme
      yasağı. Kanıt: 3 farklı IRQ aynı çerçeveden geçiyor.
- [x] **H1h (✓ D-766/dalga 16 (calistir_mmio_volatile)) [S] `volatile` anlamı denetimi.** MMIO okumalarının optimize edilip
      silinmediğini ölç (`-O2`de durum yazmacı döngüsü). Sabotaj: bariyer/asm
      bozulan listesini kaldır → test asılmalı/kırmızı olmalı.

---

## H2. Gerçek PC açılışı (QEMU `-M pc` / `q35` üzerinde)

- [ ] **H2a [M] (UEFI KARŞILIĞI ✓ D-755: kendi EFI stub + 64 bit giriş; multiboot2 bekliyor) Multiboot2 başlığı + 32→64 bit giriş.** Mevcut `pvh_entry` uzun-kip
      kurulumu yeniden kullanılır; multiboot2 sihirli sayı + bilgi yapısı işaretçisi.
- [ ] **H2b [S] ⏸ BIOS YOLU — ERTELENDİ (D-756). GRUB ISO üretici** (`grub-mkrescue`; yoksa açık hata). Makefile hedefi
      `kem_os_pc.iso`.
- [ ] **H2c (Kısmen D-755 UEFI bilgi yapısı var. Multiboot2 etiketleri ertelendi.) [M] (UEFI KARŞILIĞI ✓ D-755: açılış bilgisi ABI v1 — harita, GOP, RSDP) Multiboot2 bilgi etiketleri:** bellek haritası (E820 karşılığı),
      framebuffer etiketi, modül etiketi (ramdisk), komut satırı, ACPI RSDP etiketi.
      Kanıt: seri çıktıda bellek bölgeleri listesi.
- [x] **H2d [M] ✓ D-755 (UEFI haritası; 256M/512M/2G açılış) Fiziksel bellek yöneticisi bellek haritasından** (bugünkü sabit
      yerleşim yerine). Ayrılmış bölgeler, çekirdek imajı, modüller korunur.
      Kanıt: 512 MB / 2 GB / 4 GB RAM ile açılış.
- [x] **H2e (✓ D-757 (IST1 #DF / IST2 NMI / IST3 #MC, 16 KiB + kanarya; kasıtlı #DE/#UD/#GP/#PF/#DF panik ekranında)) [M] GDT + TSS + IDT tam kurulumu** (çift hata için ayrı IST yığını).
      Kanıt: kasıtlı #PF, #GP, #DF → panik ekranı, asılma yok.
- [ ] **H2f [S] ⏸ BIOS YOLU — ERTELENDİ (D-756; UEFI karşılığı D-755 OVMF kapıları). Kapı `calistir_kem_os_pc`:** QEMU `-M pc -cdrom kem_os_pc.iso
      -serial file:...`, mevcut faz dizgileri. Sabotaj: multiboot sihirli sayıyı boz
      → GRUB yüklemiyor → kapı kırmızı.
- [x] **H2g (✓ D-755, calistir_bm_x86_uefi_pc ile q35 ve OVMF.) [S] `q35` ikizi** (modern yonga seti; AHCI yerleşik, PCIe ECAM).

---

## H3. ACPI ve kesme yönlendirme

- [x] **H3a [M] ✓ D-755 (UEFI yapılandırma tablosu; bağımsız RAM kâhini) RSDP bul** (multiboot2 etiketi; yoksa EBDA/0xE0000 taraması) +
      sağlama toplamı doğrulama.
- [x] **H3b [M] ✓ D-755 (XSDT + RSDT geri düşme) RSDT/XSDT gezgini** + tablo imzası listesi. Kanıt: seri/ekranda
      `APIC FACP HPET MCFG` listesi.
- [x] **H3c (✓ D-757 (UEFI yolu; OVMF: LAPIC 0xfee00000, IOAPIC 0xfec00000 GSI0, ISO IRQ0→GSI2)) [M] MADT:** LAPIC adresi, IOAPIC(ler), kesme kaynağı geçersiz kılmaları
      (ISA IRQ0 → GSI2 gibi). Sabotaj: override'ı yok say → timer gelmiyor → kırmızı.
- [x] **H3d (✓ D-757) [M] IOAPIC sürücüsü:** yönlendirme tablosu, maske, kenar/seviye, kutup.
      8259 PIC tamamen maskelenir.
- [x] **H3e (✓ D-757 (PIT mode0 tek atımla kalibrasyon; PVH yolu PIC/PIT kaldı)) [M] LAPIC zamanlayıcı** (PIT ile kalibre). PIT'e bağımlılık biter.
- [x] **H3f (✓ D-757 (8042 bayrağı + reset GAS)) [S] FADT:** `8042` bayrağı (PS/2 var mı?), sıfırlama yazmacı, PM zamanlayıcı.
- [x] **H3g (✓ D-766/dalga 16 (calistir_bm_x86_kapat)) [M] Kapatma ve yeniden başlatma.** Kapatmada S5, yeniden başlatmada FADT reset ve 8042 yolu kullanılır.
      S5 için tanınan AML paketleri okunur. Çözülemeyen tabloda açık hata ve belgelenen yedek kapanış yolu kullanılır.
- [x] **H3h (✓ D-766/dalga 16 (calistir_bm_x86_msi)) [S] Not: AML'den kaçınma stratejisi.** PCI kesmeleri için `_PRT` (AML)
      yerine **MSI/MSI-X** kullanılır (H6d) — tam ACPI yorumlayıcısı yazmamak için
      bilinçli seçim.

---

## H4. Ekran: framebuffer konsolu

- [x] **H4a [M] ✓ D-755 (UEFI GOP; multiboot2 etiketi BIOS yolunda) Framebuffer etiketi** (GRUB `gfxpayload`; UEFI GOP ile AYNI veri biçimi
      → tek kod yolu). Piksel biçimi (RGB/BGR, 32/24 bpp), pitch.
- [x] **H4b (✓ D-766/dalga 16 (calistir_bm_x86_pat)) [S] Framebuffer belleğini yazma-birleştirmeli/önbelleksiz eşle** (PAT).
      Ölç: önbellekli eşlemede ekranda artık görüntü kalıyor mu.
- [x] **H4c [M] ✓ D-752 (gömülü 8×16 Uni2-Fixed16; Türkçe + kutu çizim). (KISMEN D-748: test yazı tipi 8×8 Lat sistem fontu, Türkçe harfler var; 8×16 + kutu çizim + çekirdeğe GÖMÜLÜ font kaldı) Bit eşlem yazı tipi** (8×16, PSF2 biçimi gömülü). **Türkçe
      karakterler zorunlu:** ç Ç ğ Ğ ı İ ö Ö ş Ş ü Ü + ASCII + kutu çizim.
      Kanıt: tüm Türkçe harfler ekranda doğru.
- [x] **H4d [M] ✓ D-752 (canlı yankı, imleç, geri silme, kaydırma; ANSI renk alt kümesi YOK). Metin konsolu:** imleç, satır sonu, kaydırma, sekme, renk (ANSI
      alt kümesi), UTF-8 çözme (yarım bayt dizisi → `?`, çökme yok).
- [x] **H4e [S] ✓ D-749 (kabuk v0: seri + ekran, ekran kâhini 3 disk türünde piksel piksel; ayna tamponu deseni). Çift çıktı:** her `yazdir` hem seriye hem ekrana; seri yoksa yalnız ekran.
- [x] **H4f [S] ✓ D-752 (yalnız kdl_panik yolu — E4n; log satırları/yazmaç dökümü YOK). Panik ekranı:** kırmızı zemin, panik mesajı, son 20 log satırı,
      yazmaç dökümü. (Seri yokken tek teşhis aracı budur.)
- [x] **H4g [M] ✓ D-748 (`calistir_bm_x86_ekran`: 640×480 tüm pikseller bağımsız Python çizimiyle BİREBİR, 5 açılış; sabotaj: glif bit sırası, Türkçe→ASCII, çözünürlük KIRMIZI). Kapı: ekran görüntüsü karşılaştırması.** QEMU monitörü `screendump`
      → PPM → bilinen bölgenin piksel özeti. Sabotaj: yazı tipinde `ğ` glifini boz → kırmızı.
- [x] **H4j [M] ✓ D-748 QEMU std VGA (PCI 1234:1111) + Bochs DISPI çerçeve tamponu** (MMIO BAR2
      0x500+indeks*2; QEMU docs/specs/standard-vga.rst). Gerçek PC yolu H4a (GOP/multiboot2
      etiketi) — orada çekirdek modu AYARLAMAZ, firmware'in verdiğini kullanır; çizim kodu ortak.
      ⚠ ENABLE'ın doğrusal-tampon biti (0x40) QEMU'da ölçülemez (VRAM BAR0'dan bitten bağımsız görünür).
- [x] **H4h [S] ✓ D-752 (tam ekran 78×29 çizim 32,6 ms; çift tampon gerekmiyor). Performans:** tam ekran kaydırma süresi ölç (çift tampon gerekiyor mu?).
- [x] **H4i (✓ Dalga 15, calistir_bm_x86_log. 8192 bayt, kayıp sayısı, sayfalama ve FAT32 kaydı.) [S] Çekirdek log halkası** (son N KB) + ekranda `log` komutu.

---

## H5. Klavye (PS/2)

- [x] **H5a (✓ D-766/dalga 16 (calistir_bm_x86_klavye16)) [M] 8042 denetleyici başlatma:** FADT bayrağına bak, öz-test (`0xAA`),
      port testi, çeviri modu kararı, her bekleyişte zaman aşımı.
- [x] **H5b (✓ D-757 (UEFI yolu IRQ1/GSI1 vektör 33, kesme-güvenli halka)) [S] IRQ1 → IOAPIC → işleyici**, tarama kodu kuyruğu (kesme-güvenli halka).
- [x] **H5c (✓ D-766/dalga 16 (calistir_bm_x86_klavye16)) [M] Tarama kodu kümesi 1 çözücü** (`E0` genişletilmiş, `E1` Pause),
      bas/bırak durumları.
- [x] **H5d (✓ D-766/dalga 16 (calistir_bm_x86_klavye16)) [M] Değiştirici tuş durum makinesi:** Shift, Ctrl, Alt, **AltGr**,
      Caps/Num/Scroll Lock + LED güncelleme komutu (`0xED`).
- [x] **H5e (✓ D-766/dalga 16 (calistir_bm_x86_klavye16)) [M] Türkçe Q klavye düzeni** (asıl iş): `ı İ ğ Ğ ü Ü ş Ş ö Ö ç Ç`,
      AltGr katmanı (`@ € { [ ] } \ | ~ #`), **Caps Lock'ta i→İ, ı→I** (Türkçe büyük
      harf kuralı — `metin_buyuk_tr` ile tutarlı), ölü tuşlar gerekmez (Türkçe Q'da yok).
      Ayrıca ABD düzeni (geçiş: Ctrl+Boşluk). Kanıt: her tuş için beklenen UTF-8 tablosu.
- [x] **H5f (✓ D-766/dalga 16 (calistir_bm_x86_klavye16)) [S] Tuş tekrarı** (yazılım tarafı; donanım tekrar hızını ayarla `0xF3`).
- [x] **H5g (✓ D-763, calistir_bm_x86_surec ile kullanıcıya basma ve bırakma olayları.) [M] Olay modeli:** `TusOlayi { kod, unicode, bas/bırak, değiştiriciler }`;
      çekirdekten kullanıcı alanına kanal (`kanal<TusOlayi>` ya da syscall).
- [x] **H5h (✓ D-766/dalga 16 (calistir_bm_x86_klavye16)) [M] Konsolda satır düzenleme:** geri sil (UTF-8 çok baytlı karakteri TEK
      silmeli), sol/sağ ok, Home/End, geçmiş (yukarı/aşağı), Ctrl+C (süreç sonlandırma).
- [x] **H5i (✓ D-766/dalga 16 (calistir_bm_x86_klavye16)) [M] Kapı:** QEMU monitörü `sendkey` ile dizi gönder (`s h i f t - i` gibi)
      → kabuk yankısı beklenen metin (`İ`!). Sabotaj: AltGr eşlemesini boz → `@` gelmez → kırmızı.
- [x] **H5j (✓ D-766/dalga 16 (calistir_bm_x86_klavye16)) [S] Pozitif + kenar şekiller:** iki tuşa aynı anda basma, çok hızlı yazma
      (kuyruk taşması → kayıp olay SAYILIR ve bildirilir, çökme yok).

---

## H6. Fare (PS/2 aux) + imleç

- [x] **H6a (✓ D-757) [M] Aux port etkinleştirme** (`0xA8`, `0xD4` öneki), varsayılanlar,
      veri raporlama (`0xF4`).
- [x] **H6b (✓ D-757 (IRQ12/GSI12 vektör 44, senkron kurtarma)) [M] IRQ12 + 3 baytlık paket ayrıştırıcı** + **senkron kurtarma**
      (bit 3 her zaman 1 olmalı; kayarsa yeniden hizala).
- [x] **H6c (✓ Dalga 15, calistir_bm_x86_girdi15. PS/2 kimlik 3 ve dört baytlık paket.) [S] IntelliMouse tekerlek** (200-100-80 örnekleme sihirli dizisi → 4 bayt paket).
- [x] **H6d (✓ Dalga 15, calistir_bm_x86_girdi15. QMP hareketiyle hız ve kenetleme.) [S] Hızlanma eğrisi + hassasiyet + ekran sınırına kenetleme.**
- [x] **H6e (✓ D-757, calistir_bm_x86_ps2_irq ile save-under ve ekran karşılaştırması.) [M] İmleç çizimi:** altını kaydet/geri yükle (save-under), titreme yok,
      metin konsoluyla çakışmada doğru yeniden çizim.
- [x] **H6f (✓ D-763 ve Dalga 15, calistir_bm_x86_ekran_fare ve calistir_bm_x86_girdi15. Gezgin ve düzenleyicide tekerlek.) [M] Fare olayları:** hareket, bas/bırak (sol/sağ/orta), tekerlek,
      çift tıklama (zaman eşiği); kullanıcı alanına kanal.
- [x] **H6g (✓ D-757, calistir_bm_x86_ps2_irq ile kayık AUX baytı, gerçek hareket ve piksel karşılaştırması.) [M] Kapı:** QEMU monitörü `mouse_move` / `mouse_button` → olay günlüğü
      + imleç konumu ekran görüntüsünde. Sabotaj: senkron kurtarmayı kaldır, paket
      kaydır → imleç sapar → kırmızı.
- [x] **H6h (✓ D-763, calistir_bm_x86_ekran_fare PS/2 göreli fareyi ölçüyor.) [S] QEMU USB tablet (mutlak konum) NOT:** QEMU'da `usb-tablet` daha rahat
      ama gerçek PC'de yok; kapılar PS/2 fareyi ölçer.

---

## H7. PCI ve depolama

- [x] **H7a (✓ D-766/dalga 16 (calistir_bm_x86_pci16)) [M] PCI yapılandırma alanı:** önce legacy `0xCF8/0xCFC` (H1f'e bağlı),
      sonra ECAM (ACPI MCFG). `pci_enum_x86.c` mantığı `.kem`e taşınır.
- [x] **H7b (✓ D-766/dalga 16 (calistir_bm_x86_pci16)) [M] Aygıt numaralandırma + sınıf kodları + köprüler** (veri yolu tarama).
      Ekranda `lspci` benzeri çıktı (gerçek donanımda teşhis için ŞART).
- [x] **H7c (✓ D-766/dalga 16 (calistir_bm_x86_pci16)) [M] BAR boyutlandırma + MMIO eşleme** (önbelleksiz), 64-bit BAR.
- [x] **H7d (✓ D-766/dalga 16 (calistir_bm_x86_msi)) [M] MSI / MSI-X** (H3h gereği INTx yerine). Kanıt: AHCI kesmesi MSI ile geliyor.
- [x] **H7e (✓ D-766/dalga 16 (calistir_bm_x86_disk_hata)) [L] AHCI sürücüsü:** HBA sıfırlama, port başlatma, IDENTIFY, komut
      listesi + FIS + PRD tabloları, 48-bit LBA okuma/yazma DMA, hata kurtarma
      (port sıfırlama), zaman aşımları. Kanıt: QEMU `-device ahci` + imaj, sektör
      yaz/oku gidiş-dönüşü + SHA256.
- [x] **H7f (✓ D-759 (çeşit+eşleş: NVMe/AHCI/USB; 64 sektör LRU yazma-geri önbellek)) [M] Blok katmanı:** blok aygıtı soyutlaması (AHCI/ramdisk/virtio/NVMe aynı
      arayüz), sektör önbelleği (LRU + kirli bit), istek kuyruğu.
- [x] **H7g (✓ D-759 (birincil MBR + ana GPT, CRC; yedek GPT / genişletilmiş MBR YOK)) [M] Bölüm tabloları:** MBR + GPT (CRC32 doğrulama — `[30] CRC32 OK` zaten var).
      Kanıt: GPT'li imajda doğru bölüm bulunuyor; bozuk GPT başlığı → yedek başlık.
- [x] **H7h (✓ D-766/dalga 16 (calistir_bm_x86_ramdisk)) [S] UEFI ramdisk imajı.** Açılış bölümündeki ramdisk.img dosyası fiziksel çerçevelere yüklenir.
      BIOS/multiboot2 modül devri D-756 kapsamında ertelidir.
- [x] **H7i (✓ D-745, calistir_bm_x86_nvme ve calistir_qtest ile IDENTIFY ve gerçek blok aktarımı.) [L] NVMe sürücüsü — AHCI'DEN ÖNCE (KARAR 2026-11-07: daha basit, modern makinelerin çoğu):** admin + I/O kuyrukları,
      IDENTIFY, okuma/yazma. Kanıt: QEMU `-device nvme`.
- [x] **H7j (✓ D-766/dalga 16 (calistir_bm_x86_ramdisk)) [S] Kapı matrisi:** ahci / nvme / ramdisk × MBR / GPT.
      Sabotaj: PRD bayt sayısını bir eksik yaz → SHA256 tutmaz → kırmızı.

---

## H8. Dosya sistemi

- [x] **H8a (✓ D-766/dalga 16 (calistir_bm_x86_ramdisk)) [M] VFS katmanı:** düğüm, açık dosya, dosya tanımlayıcı tablosu (LOOP E3d),
      yol çözümleme (`/`, `.`, `..`), bağlama noktası (`/` = AÇILIŞ bölümü, `/ram`,
      `/diskler/*` — H8j).
- [x] **H8b (✓ D-759, calistir_bm_x86_v1 ile FAT32 kök, alt dizin ve dosya okuma.) [M] FAT32 okuma:** BPB doğrulama, FAT zinciri, kök/alt dizin, 8.3 adlar.
- [x] **H8c (✓ D-759, calistir_bm_x86_v1 ve drivers/fs/fat32_dizin.kem ile Türkçe LFN.) [M] Uzun dosya adı (LFN):** UTF-16 → UTF-8; **Türkçe ad** (`günlük.txt`).
      Sağlama toplamı denetimi.
- [x] **H8d (✓ D-759 (sektör tabanlı yazıcı; imaj tabanlı yazıcı kâhin)) [L] FAT32 yazma:** küme ayırma (FSInfo ipucu), iki FAT kopyasını da
      güncelleme, dizin girişi oluşturma/silme, LFN üretme + 8.3 kısa ad çakışması,
      dosya büyütme/küçültme, `mkdir`/`rmdir`/`rename`.
- [x] **H8e (✓ D-759 (BPB 0x41 bit0; fsck.fat 4.2 ikisini de okuyor)) [M] Senkron + kirli bayrak:** `sync`, bağlarken kirli-kapanma bayrağı
      kontrolü, basit `fsck` (kayıp küme, çapraz bağlı zincir raporu).
- [x] **H8f (✓ D-759 (fsck.fat -n + bağımsız Python okuyucu, iki yönlü)) [M] Çapraz doğrulama kapısı (en güçlü oracle):** ana makinede
      `mkfs.fat` + `mtools` ile imaj hazırla → KEMGU-OS okur; KEMGU-OS'un yazdığı
      dosyaları ana makinede `mtype`/`fsck.fat -n` doğrular. Sabotaj: ikinci FAT
      kopyasını güncelleme → `fsck.fat` uyarır → kırmızı.
- [x] **H8g (✓ D-759 (sabit tohumlu 1000 bozuk sektör kümesi, C + self)) [M] Bozuk imaj dayanıklılığı (GÜVENLİK):** rastgele bozulmuş 1.000 FAT
      imajında çekirdek **asla** çökmez / asılmaz / sınır dışı okumaz (zincir
      döngüsü tespiti, küme numarası aralık denetimi). KEMGU'nun "çökmezlik" vaadinin
      sürücü katmanındaki kanıtı.
- [ ] **H8h (Açık D-763 ve E4k. 1 GB okuma sırasında sabit heap kanıtı eksik.) [S] Bellek: dosya önbelleği H1c ile geri verilir.** Kanıt: 1 GB dosya
      okununca heap sabit kalıyor.
- [x] **H8j (✓ D-759 (/diskler/{nvme,sata,usb}<n>p<b>)) [M] Çoklu bağlama (KARAR 2026-11-07: USB + iç disk BİRLİKTE):** açılışta tüm
      blok aygıtları taranır, her FAT32 bölümü bağlanır: açılış bölümü `/`, diğerleri
      `/diskler/<aygıt><no>p<bölüm>` (`nvme0p1`, `sata0p2`, `usb1p1`). Sürücü harfi YOK.
      `bagla` / `ayir` / `bagli` komutları (H9).
- [x] **H8k (✓ D-759 (stub HardDrive düğümü → açılış bilgisi uzantısı; OVMF'de ölçüldü)) [S] Açılış bölümünü bulma:** ⚠ D-756: GRUB ertelendi → v1'de kaynak UEFI'dir:
      EFI_LOADED_IMAGE_PROTOCOL.DeviceHandle'ın aygıt yolundaki GPT bölüm GUID'i stub tarafından
      açılış bilgisi ABI'sine yazılır ve çekirdek o bölümü bağlar. Eski metin (BIOS yolu): GRUB komut satırı `kok=<FAT32 seri no>` (Linux
      `root=UUID` deseni; multiboot2 komut satırı etiketi BIOS ve UEFI'de AYNI). multiboot2
      "açılış aygıtı" etiketi yalnız BIOS'ta var → kullanılmaz. Seri eşleşmezse: tek FAT32
      varsa o, yoksa ramdisk + açık uyarı (sessiz tahmin YOK).
- [x] **H8l (✓ D-759 (ESP hiçbir zaman yazılmaz; iç diskler `bagla --yazilabilir`)) [M] İç disk GÜVENLİĞİ:** açılış bölümü dışındaki her bölüm varsayılan SALT
      OKUNUR; GPT türü EFI Sistem Bölümü (ESP, `C12A7328-…`) ASLA yazılmaz (iç diskteki
      Windows/Linux'un açılışını bozardı); yazılabilir bağlama yalnız `bagla --yazilabilir`
      ile ve ESP hariç. Kanıt: QEMU'da ESP'li GPT diskine yazma girişimi → açık hata;
      sabotaj (ESP denetimini kaldır) → kapı kırmızı.
- [x] **H8m (✓ D-759 (USB + NVMe + AHCI aynı anda, GPT/MBR)) [M] Depolama matrisi kapısı:** QEMU'ya aynı anda USB bellek imajı + NVMe +
      AHCI takılı; {SeaBIOS, OVMF} × açılış aygıtı {USB, NVMe, AHCI}; her birinde
      `/` doğru seçiliyor, `/diskler/*` salt okunur, açılış bölümüne yaz → ana makinede
      `fsck.fat -n` temiz.
- [ ] **H8i [L] (İsteğe bağlı, sonraki aşama) KEMFS:** günlüklü kendi FS'imiz
      (eski `fs_journal`/`crashfs` C testleri referans). Çökme sonrası tutarlılık kapısı.

---

## H9. Kullanıcı alanı: kabuk, programlar, kanıt uygulamaları

- [x] **H9a (✓ D-761 (ABI v2: rax num, rdi/rdx/rsi; ortak VFS katmanı — kabuk ve sistem çağrıları aynı kod)) [M] Dosya syscall'ları:** `ac/oku/yaz/kapat/konumla/bilgi/dizin_oku/
      dizin_olustur/sil/yeniden_adlandir`. Kullanıcı işaretçisi doğrulaması
      (`[28] SYSCALL PTR DOGRULAMA OK` deseni) HER birinde.
- [x] **H9b (✓ D-763 (fare_bekle + olay_bekle)) (KISMEN D-761 (klavye + zaman; fare_bekle YOK)) [M] Olay syscall'ları:** klavye olayı bekle, fare olayı bekle,
      zaman aşımlı bekleme (seçici: `bekle(olaylar, ms)`).
- [x] **H9c (✓ D-761 (ELF FAT32'den, sayfa izinleri segment bayraklarından, bss sıfır, argv yığında)) [M] ELF'i dosya sisteminden yükle** (`[18] ELF YUKLE OK` arm64'te var →
      x86 + VFS'e bağla). `/bin/*` programları.
- [x] **H9d (✓ D-763 (zaman paylaşımlı çok süreç; baslat/bekle/ps/sonlandir)) (KISMEN D-761 (C1 senkron tek süreç; zaman paylaşımlı çok süreç YOK)) [M] Süreç modeli (LOOP E3b/E3c):** çok süreç, `baslat(yol, argv)`,
      `bekle`, çıkış kodu, hata olunca süreci öldür (çekirdek yaşar).
- [x] **H9e (✓ D-763 (geçmiş, Tab, /bin araması)) [L] Kabuk (LOOP E3e):** `ls cd pwd cat yaz sil mkdir rmdir mv cp temizle
      saat bellek lspci log yeniden_baslat`, tırnak, yönlendirme (`>`), geçmiş,
      sekme ile tamamlama (dosya adı).
- [x] **H9f (✓ D-763 (`duzenle`)) [M] Metin düzenleyici `duzenle`** (nano benzeri: aç, imleç, yaz,
      Ctrl+S kaydet, Ctrl+Q çık) — **klavye + FS'in birlikte kanıtı.**
- [x] **H9g (✓ D-763 (`dosyalar`)) [M] Fare kanıt programları:** (1) `dosyalar`: fareyle dosya seç,
      çift tıkla `duzenle`de aç; (2) `ciz`: fareyle çiz, kaydet (BMP → FS'e).
      **Klavye + fare + FS'in birlikte kanıtı.**
- [x] **H9h (✓ D-759 (MC146818; dosya zaman damgaları)) [S] Saat:** RTC (`rtc_x86.c` referans) → `saat` komutu + dosya zaman damgaları.
- [x] **H9i (✓ D-763 (tek açılışta v1 senaryosu)) [M] Kapı: uçtan uca senaryo** (QEMU, `sendkey` + `mouse_*` betiği):
      aç → `duzenle not.txt` → Türkçe metin yaz → kaydet → yeniden başlat → `cat` →
      aynı metin; ana makinede `mtype` ile de aynı metin. Sabotaj: `sync`i kaldır → kırmızı.
- [ ] **H9j (Kısmen D-763 aynı sanal adreste farklı fiziksel çerçeveleri kanıtlıyor. E3f bağımsız fd yalıtım kapısı eksik.) [S] Süreç izolasyonu kapısı (LOOP E3f):** kötü niyetli program diğerinin
      belleğini/dosya tanımlayıcısını okuyamaz.

---

## H10. Gerçek donanım uyumu

- [x] **H10a (✓ D-755 ve D-756, calistir_bm_x86_uefi_pc ile kendi EFI stub'ı ve GOP.) [M] (✓ KENDİ STUB'IMIZLA D-755; GRUB EFI gerekmedi — D-756) UEFI açılışı (GRUB EFI) — H2 İLE BİRLİKTE, ilk günden (KARAR 2026-11-07):** multiboot2 aynı; GOP framebuffer; UEFI
      bellek haritası; ExitBootServices sonrası. Kapı: QEMU + OVMF.
- [x] **H10b (✓ D-763, belgeler/X86_Fiziksel_Donanim_Kontrol_Listesi.md ve calistir_bm_x86_uefi_pc.) [S] Secure Boot kapalı gereksinimi belgelenir** (imza zinciri kapsam dışı).
- [x] **H10c (✓ D-745, calistir_qtest ile gerçek xHCI kök port enumerasyonu.) [L] xHCI (USB 3) sürücüsü:** denetleyici sıfırlama, komut/olay/aktarım
      halkaları, port sıfırlama + hız, cihaz adresleme, yapılandırma tanımlayıcıları.
      Neden gerekli: modern makinelerin çoğunda PS/2 YOK ve UEFI'de PS/2 öykünmesi
      çoğunlukla kapalı. Kapı: QEMU `-device qemu-xhci`.
- [x] **H10d (✓ D-763, calistir_bm_x86_ekran_fare ile USB HID klavye ve fare.) [M] USB HID boot protokolü:** klavye (8 baytlık rapor → H5 düzen
      katmanına aynı olay) + fare (göreli rapor → H6 katmanına aynı olay).
      Kapı: QEMU `usb-kbd`, `usb-mouse`.
- [x] **H10e (✓ D-766/dalga 16 (calistir_bm_x86_usb_hub)) [S] USB hub desteği** (çoğu dizüstünde dahili cihazlar hub arkasında).
- [x] **H10f (✓ D-759, calistir_bm_x86_v1 ile BOT/SCSI üzerinden kalıcı FAT32 kayıt ve okuma.) [L] ZORUNLU (KARAR 2026-11-07, v1 USB bellekten açılır) USB yığın depolama (BOT + SCSI)** — dosya sistemi
      açılış USB belleğinde olsun istenirse. Yoksa iç SATA/NVMe bölümü kullanılır.
- [x] **H10g (✓ D-766/dalga 16 (calistir_uart_16550_test, calistir_uart_pl011_test, calistir_bm_x86_saglamlik)) [M] Sağlamlık turu:** her donanım bekleyişinde zaman aşımı + net hata
      mesajı (hangi aygıt, hangi yazmaç); eksik aygıtta zarif düşüş (PS/2 yoksa USB'ye,
      AHCI yoksa NVMe'ye, hiç disk yoksa ramdisk).
- [x] **H10h (✓ Dalga 15, calistir_bm_x86_donanim. Yazılabilir açılış köküne donanim.txt ve host donanım karşılaştırması.) [M] Teşhis paketi:** açılışta `donanim.txt` (PCI listesi, ACPI tabloları,
      bellek haritası, bulunan/bulunamayan aygıtlar) → FS'e yazılır. Seri port yokken
      sorun bildirmenin tek yolu.
- [ ] **H10i [S] SMP (isteğe bağlı):** LAPIC INIT-SIPI (`smp_x86.c` referans). Tek
      çekirdekle de hedef karşılanır; ertelenebilir.

---

## H11. Donanım koşumu (Mehmet + ben)

- [x] **H11a (✓ D-763 (`make kem_os_usb.img`)) [S] USB imaj üretici:** `make kem_os_usb.img` (GPT + EFI bölümü + [BIOS
      GRUB — D-756 ile ertelendi] + FAT32 veri bölümü + örnek dosyalar). `dd` komutu belgede, cihaz adı ASLA
      betikte sabit değil.
- [x] **H11b (✓ D-763 (X86_Fiziksel_Donanim_Kontrol_Listesi.md)) [S] Kontrol listesi** (`belgeler/X86_Fiziksel_Donanim_Kontrol_Listesi.md`,
      ARM64 listesinin deseni): BIOS ayarları (CSM/Legacy, Secure Boot kapalı, SATA AHCI
      kipi), beklenen ekran çıktıları, başarısızlıkta ne fotoğraflanacak.
- [ ] **H11c (Açık H11b kontrol listesi var. Fiziksel eski masaüstü sonucu yok.) [M] Makine 1 — eski BIOS'lu masaüstü** (PS/2 + SATA): tam senaryo H9i.
- [ ] **H11d (Açık H11b kontrol listesi var. Fiziksel UEFI masaüstü sonucu yok.) [M] Makine 2 — modern UEFI masaüstü** (USB klavye/fare + NVMe).
- [ ] **H11e (Açık H11b kontrol listesi var. Dizüstü aygıt sonuçları yok.) [M] Makine 3 — dizüstü** (hub arkasında USB, dokunmatik yüzey PS/2
      öykünmesi ya da I²C-HID → I²C-HID KAPSAM DIŞI, harici fare kullanılır).
- [ ] **H11f (Kısmen D-763 sonuç şablonu var. Doldurulmuş fiziksel kayıt, dosya ve fotoğraf eksik.) [S] Sonuç kaydı:** her makine için çalışan/çalışmayan aygıt tablosu +
      `donanim.txt` + ekran fotoğrafları depoya.

---

## H12. Kalite kapıları ve bakım

- [ ] **H12a (Kısmen Dalga 15, calistir_kem_os_pc_matris ile 12 UEFI yapılandırması ve 72 açılış. BIOS yolu D-756 ile ertelendi.) [M] QEMU kapı matrisi `calistir_kem_os_pc_matris`:** {BIOS, UEFI} ×
      {PS/2, USB} × {AHCI, NVMe}. `test_tumu`ya süreye göre ALT KÜME bağlanır
      (D-453 deseni: temsilciler her koşumda, tam matris haftalık/CI).
- [x] **H12b (D-767, S223. Dokuz ek korpus ve USB sınır örneği, `calistir_kem_os_ayristirici`.) [M] Bulanıklaştırma:** bozuk FAT imajları (H8g), bozuk ACPI tabloları,
      saçma PS/2 paketleri, bozuk USB tanımlayıcıları → çekirdek asla çökmez.
- [x] **H12c (✓ D-766/dalga 16 (KEMGU_OS_Sabotaj_Envanteri.md)) [S] Sabotaj envanteri:** her H maddesinin sabotajı tek tabloda (S-numaralı).
- [x] **H12d (✓ Dalga 15, calistir_bm_x86_acilis_suresi. Etkin UEFI yolunda devirden kabuğa yedi işaret ve 650 ms medyan eşiği.) [S] Açılış süresi ölçümü** (GRUB → kabuk istemi) ve gerilemeyi yakalayan eşik.
- [x] **H12e (✓ D-766/dalga 16 (calistir_bm_x86_belge15)) [S] Belgeler:** sürücü mimarisi, syscall tablosu, klavye düzeni tablosu,
      "yeni sürücü nasıl eklenir".

---

## H13. Ağ

- [x] **H13a** Host Ethernet eşi ve pcap. `calistir_ag_es`
- [x] **H13b** 82540EM sürücüsü. `calistir_bm_x86_e1000`
- [x] **H13c** 82574L, 82576 ve VirtIO PCI. `calistir_bm_x86_nic_matris`
- [x] **H13d** IPv4, ARP, ICMP ve loopback. `calistir_bm_x86_ag_l3`
- [x] **H13e** UDP, DHCP ve DNS. `calistir_bm_x86_ag_udp`
- [x] **H13f** IPv4 parçaları ve ICMP hataları. `calistir_bm_x86_ag_parca`
- [x] **H13g** TCP bağlantı durumları. `calistir_bm_x86_tcp_temel`
- [x] **H13h** TCP kayıp kurtarma. `calistir_bm_x86_tcp_kayip`
- [x] **H13i** TCP seçenekleri ve verim. `calistir_bm_x86_tcp_verim` ve `calistir_bm_x86_tcp_secenek`
- [x] **H13j** Soket ABI ve çoklu bekleme. `calistir_bm_x86_soket`
- [x] **H13k** Ağ komutları ve borular. `calistir_bm_x86_ag_arac`
- [x] **H13l** HTTP istemcisi ve sunucusu. `calistir_bm_x86_http`
- [ ] **H13m** NTP ve RTC. `calistir_bm_x86_ntp`
- [ ] **H13n** TFTP ve uzak teşhis. `calistir_bm_x86_tftp_teshis`
- [ ] **H13o** Paket kurucu. `calistir_bm_x86_paket`
- [ ] **H13p** USB CDC-ECM. `calistir_bm_x86_usb_ag`
- [ ] **H13q** ARM virtio-mmio köprüsü. `calistir_kem_os_arm_ag`
- [ ] **H13r** Ağ ayrıştırıcıları ve kaynak sınırları. `calistir_kem_os_ayristirici`
- [ ] **H13s** İki konuk ve NIC matrisi. `calistir_kem_os_pc_matris`
- [ ] **H13t** ABI ve derlenebilir ağ örnekleri. `calistir_bm_x86_belge15`

IPv6 ve TLS bu dalganın kapsamı dışındadır.

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

Tam ACPI/AML yorumlayıcısı · pencere yöneticisi / GUI araç takımı · ses · Realtek dahili Ethernet · GPU
hızlandırma · I²C-HID dokunmatik yüzey · Wi-Fi/Bluetooth · Secure Boot imzalama ·
ARM64 donanım hattı (ayrı: E1 + D-490).
