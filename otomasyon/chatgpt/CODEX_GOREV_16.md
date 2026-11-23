# CODEX GÖREVİ 16 — kısmen yapılmış maddeleri bitir: klavye, PCI/ECAM, kesme çerçevesi + MSI, disk hata kurtarma, USB hub, ramdisk, ACPI kapatma, PAT, bulanıklaştırma, UART, ölçüm, araçlar, belgeler

Dalga 1'in kuralları geçerlidir (`otomasyon/chatgpt/CODEX_GOREV.md` §2, §3, §5 — yeniden
oku), aşağıdaki **izin genişletmesi** dışında. Dalga 6'nın gerçek-QEMU kâhin kuralı da
geçerlidir (`CODEX_GOREV_6.md` §0). Soru sorma, durma; takılırsan (60 dk) kaydet ve geç.

## ⏱ Bu dalga ÇOK UZUNDUR — çalışma biçimi
- Dalga 15 on bir bölümü bitirdi ve iyi sonuç verdi. Bu dalga **on üç bölümdür, HEPSİ zorunlu ve sırayla
  yapılır.** Beklenen süre dalga 15'ten uzundur; 15+ saat normaldir. Bir bölümü bitirince **durma, rapor
  yazıp bekleme — sonraki bölüme geç.**
- Her bölümün sonunda: (1) o bölümün kapıları + sabotajları, (2) ayrı commit
  (`SURUCU(codex): Dalga 16 bölüm N — ...`), (3) `codex16_ilerleme.md`'ye kısa not (yapılan, ölçümler,
  sabotaj sonuçları, süre). Dalga yarıda kesilse bile biten bölümler kullanılabilir olmalı.
- Gerçekten takılırsan (60 dk ilerleme yok): `codex16_bulgular.md`'ye yaz, bölümün yapılabilen kısmını
  kapılı bırak ve **sonraki bölüme geç**.
- **Her sabotajı gerçekten uygula ve sonucunu yaz.** Bir sabotajı yapmadıysan bunu açıkça yaz; "benzer bir
  sabotaj" ile değiştirme. (Dalga 15'te ELF sınır denetimi sabotajı yapılmadı; Claude sonradan yaptı, S210.)
- Draft PR'ı yalnız on üç bölüm bittikten (ya da her biri için takılma notu yazıldıktan) sonra aç.

---

## 0. Hedef

v1 QEMU'da tamam (D-763), dalga 15 (D-765) sağlamlık, teşhis, açılış matrisi ve bulanıklaştırmayı ekledi.
Yol haritasında artık çok sayıda **"Kısmen"** işaretli madde var: çoğunun eksik parçası küçük ama gerçek
donanımda önemli (Pause tuşu, Num Lock LED'i, hub arkasındaki USB aygıtları, ECAM, MSI, disk hata
kurtarma, ACPI kapatma). Bu dalga o kısmi maddeleri **tamamlar** ve her birini kanıt kapısıyla kapatır.
Yanında birkaç kullanılabilirlik eklemesi (`bilgi`, düzenleyicide arama/geri al, dosya gezgininde
kopyala/taşı/sil) ve belgelerin güncellenmesi.

Yol haritası: H5a, H5c, H5d, H5e, H5f, H5h, H5i, H5j, H1f, H7a, H7b, H7c, H1g, H7d, H3h, H7e, H10e,
H7h (UEFI karşılığı), H7j, H8a (`/ram`), H3g, H4b, H12b, H10g (kalan), H1h, H1a, H12c.

---

## 1. Bugünkü durum (Claude ölçtü — varsayma, yine de kendin doğrula)

- Dalga 15 (D-765) çıktıları: bekleme envanteri + zaman aşımları (`calistir_bm_x86_saglamlik`),
  `donanim` + donanim.txt, 64 KiB çekirdek günlük halkası (`log`), PC matrisi 12 yapılandırma
  (`calistir_kem_os_pc_matris`, 72/72), bulanıklaştırıcılar (`calistir_kem_os_ayristirici`), tekerlek +
  hızlanma + yazılım tuş tekrarı (`calistir_bm_x86_girdi15`), `goster` `bul` `hesapla` `saat`
  (`calistir_bm_x86_program15`), boru hatları + yönlendirme (`calistir_bm_x86_boru`), açılış süresi
  (`calistir_bm_x86_acilis_suresi`, eşik 650 ms), belgeler (`calistir_bm_x86_belge15`).
  Ölçümler `otomasyon/chatgpt/codex15_bulgular.md`'de; sabotaj tablosu `belgeler/KEMGU_OS_Sabotaj_Envanteri.md`.
- ABI (`belgeler/KEMGU_OS_Syscall_ABI.md`) şu an **70'e kadar** dolu (69 `boru`, 70 `baslat_eslemeli`).
  Bu dalganın yeni çağrıları **71-89**.
- Yol haritasının "Kısmen" notları maddelerin başında; her bölüm hangi parçanın eksik olduğunu oradan okur.
- Mevcut parçalar (yeniden yazma, genişlet): `drivers/ps2/klavye_tr.kem` (E0, Shift/Ctrl/AltGr/Caps),
  `drivers/pci/yapilandirma.kem` + `runtime/mimari/x86_64/kem_pci_hw.kem` (CF8/CFC, 32 bit),
  `drivers/acpi/fadt.kem` (`pm1a_cnt_blok`, `x_dsdt`, reset yazmacı zaten ayrıştırılıyor),
  `drivers/usb/xhci_cekirdek.kem` (yalnız kök portlar), `drivers/ahci/cekirdek.kem`, MSR erişimi
  `runtime/mimari/x86_64/kem_apic_hw.kem` ve `kem_mmu_hw.kem` içinde (`satıriçi_asm`), xHCI MSI-X vektör 65.
- QEMU 8.2.2 bu makinede şu aygıtları sunuyor (Claude doğruladı): `usb-hub`, `pcie-root-port`,
  `pci-bridge`, `pcie-pci-bridge`, `i82801b11-bridge`, `ich9-ahci`, `nvme`, `qemu-xhci`, `usb-kbd`,
  `usb-mouse`, `usb-tablet`. Disk hata enjeksiyonu `blkdebug` ile çalışıyor; doğru sözdizimi:
  `-drive if=none,id=d0,driver=blkdebug,config=<dosya>,image.driver=file,image.filename=<imaj>`
  (`image.driver=raw` hata verir). OVMF: `/usr/share/OVMF/OVMF_CODE_4M.fd`.
- **Heap (E4k — açık, Claude'da):** çekirdek heap'i geri verilmiyor (dalga 15: boru oturumu +10,5 MB).
  Uzun senaryoları taze açılışlara böl, ölç ve yaz; derleyiciyi düzeltmeye çalışma. Ramdisk tamponunu
  (Bölüm 6) heap'ten DEĞİL, fiziksel çerçeve ayırıcısından al.
- Claude'un tam takım koşumu (D-765) 4 sa 2 dk sürdü, seninki 72 dk; fark ölçülmedi. Uzun sürmesi normal.

---

## 2. Ön koşul ve çalışma alanı
```bash
cd /home/msyrt/Desktop/kemgu && git fetch origin
git log origin/main --oneline | grep D-766     # yoksa 5 dk'da bir, en çok 2 sa; sonra DUR, kaydet
git worktree add ../kemgu-codex16 -b codex/tamamlama-dalga16 origin/main && cd ../kemgu-codex16
make -j8 build/kemgu build/codegen build/kdl_runtime.o
make calistir_bm_x86_v1 calistir_bm_x86_uefi_pc calistir_bm_x86_saglamlik calistir_kem_os_x86   # taban
date > otomasyon/chatgpt/codex16_ilerleme.md
```
Tek dal, tek draft PR. ⚠ Claude bu dalga sırasında derleyicide (E4k) çalışabilir; **dalını dalga ortasında
main ile birleştirme** — Claude entegrasyonda birleştirir.

---

## 3. Okunacaklar
- `belgeler/KEMGU_OS_Donanim_Yol_Haritasi.md` (tamamı; özellikle "Kısmen" notları), `LOOP.md` D-755..D-765,
  `otomasyon/chatgpt/codex15_bulgular.md`, `codex15_ilerleme.md`.
- `belgeler/KEMGU_OS_Syscall_ABI.md`, `KEMGU_OS_Mimari.md`, `KEMGU_OS_Program_Yazma.md`,
  `KEMGU_OS_Klavye_Duzeni.md`, `KEMGU_OS_Sabotaj_Envanteri.md`, `X86_Fiziksel_Donanim_Kontrol_Listesi.md`.
- `test/ornekler/kem_os_x86_kabuk.kem`, `kem_os_x86_vfs_ortak.kem`, `kem_os_x86_blok_ortak.kem`,
  `runtime/mimari/x86_64/*.kem`, `drivers/**`, `test/kullanici/*.kem`, `runtime/kullanici/x86_64/*.kem`.
- `tests/qemu/*.py`, `test/bm_x86_*_harness.sh`, `otomasyon/chatgpt/dogrula.sh`.

---

## 4. İzin genişletmesi (yalnız bu dosyalarda)
- `belgeler/KEMGU_OS_Donanim_Yol_Haritasi.md` (işaretler ve kanıt notları), `belgeler/KEMGU_OS_*.md`
  (mevcutları güncelle, yenilerini ekle), `belgeler/X86_Fiziksel_Donanim_Kontrol_Listesi.md`.
- `boot/start_x86_64.S`: YALNIZ yeni kesme vektörü koçanı; mevcut yollar birebir korunur.
- `runtime/kdl_kesme.c`: YALNIZ `#if defined(__x86_64__)` IDT bloğu (ikili modda düzenle).
- **YENİ:** `runtime/kdl_runtime_uart_16550.c` ve `runtime/kdl_runtime_uart_pl011.c`: YALNIZ gönderme
  (TX) bekleme döngülerine sınır eklemek (Bölüm 10). Alma (RX) davranışı, imzalar ve diğer satırlar
  değişmez. İkili modda düzenle (bu dosyalar geçerli UTF-8 olmayabilir: Python'da `rb`/`wb`).
- `runtime/kem_gorev_cekirdek.kem` (sistem çağrısı tablosu; arm64 davranışı değişmez).
- `runtime/mimari/x86_64/*.kem`, `runtime/kullanici/x86_64/*.kem`, `linker/user-x86_64.ld`.
- `drivers/**` saf (sans-IO); yeni modüller serbest.
- `test/ornekler/kem_os_x86_*.kem`, `test/kullanici/*.kem`, `test/bm_x86_*_harness.sh`, `tests/qemu/*`,
  `tests/drivers/**`, `tools/codex/*`, `test/olcum/*` (yeni, Bölüm 11).
- Makefile (yeni hedefler + `test_tumu` + `.PHONY` + x86 CAT listeleri), `test/check_kapisi.sh`,
  `test/baremetal_diff_harness.sh` sınıflandırma satırları; `belgeler/KEMGU_OS_Syscall_ABI.md`.
- YASAK: `src/`, `selfhost/`, `boot/efi/`, diğer `linker/` dosyaları, yukarıdakiler dışındaki runtime C
  dosyaları. Derleyici hatası → `codex16_bulgular.md`'ye C16-xxx + küçük tekrar üretimi (C ve self-host
  komutu, beklenen/gerçek sonuç), dolan. Muafiyet listelerine satır EKLEME.

---

## 5. Tasarım ilkeleri (önceki dalgalarınkiler geçerli, ek olarak)
1. **Kısmi madde ancak eksik parçası kapıyla kanıtlanınca `[x]` olur.** Notu sil, yerine
   `✓ D-766/dalga 16 (<kapı adı>)` yaz. Yapamadığın parça kalırsa not kalır ve neyin eksik olduğunu söyler.
2. Her bekleme sınırlı, her hata açık mesajlı (dalga 15 biçimi: `KABUK HATA <aygıt> <ne> ...`).
3. ABI geriye uyumlu büyür (5-70 değişmez; yeni çağrılar 71-89); belge ↔ dağıtıcı ↔ sarmalayıcı tutarlılık
   kapısı (`tests/qemu/surec_abi.py`) her yeni çağrıyı kapsar.
4. Kabuk ve programlar aynı sürücü/dosya katmanını kullanır (D-407); yeni bir aygıt yolu kabuğa ayrı
   kopya olarak girmez.
5. Sessiz kısaltma yok; her sınır belgelenmiş hata kodu döner.
6. Bağımsız kâhin: konuğun kendi raporu tek başına kanıt değildir; host tarafı aynı bilgiyi QEMU'dan
   (QMP, HMP, imaj dosyası, `fsck.fat`) bağımsız olarak okur ve karşılaştırır.

---

## 6. İş listesi — on üç bölüm, HEPSİ zorunlu, sırayla

### Bölüm 1 — klavyeyi tamamla (H5a, H5c, H5d, H5e, H5f, H5h, H5i, H5j)
- **H5a:** 8042 başlatmada FADT `IAPC_BOOT_ARCH` 8042 bitine bak (bit yoksa ve aygıt cevap vermiyorsa
  açık "8042 yok" mesajı, USB klavyeye düş); denetleyici öz-testi (`0xAA` → `0x55`) ve port testleri
  (`0xAB`, `0xA9` → `0x00`). Sonuçlar `donanim` raporunda.
- **H5c:** `E1 1D 45 E1 9D C5` Pause dizisi tek bir "Pause" olayı; yarım dizi sonraki tuşları bozmaz.
  PrintScreen (`E0 2A E0 37` / `E0 B7 E0 AA`) tek olay.
- **H5d:** Num Lock, Scroll Lock, Caps Lock durumları + LED komutu (`0xED` + bit maskesi, ACK bekle,
  zaman aşımlı). Num Lock kapalıyken sayısal tuş takımı gezinme tuşu, açıkken rakam. USB yolunda HID
  çıktı raporu ile aynı LED'ler (SET_REPORT). İki kaynakta aynı davranış (tek ortak durum makinesi).
- **H5e:** Türkçe Q ↔ ABD düzeni geçişi: kabuk komutu `duzen tr|us` + kısayol (seçimini yaz, belgeye ekle).
  Geçiş iki kaynakta da anında etkili.
- **H5f:** PS/2 donanım tekrar hızı `0xF3` ile ayarlanır (yazılım tekrarı ile çakışmamalı: donanım
  tekrarını ya kapat ya da yazılım tekrarı yerine kullan — seçimini ve gerekçesini yaz; dalga 15'in
  500/50 ms davranışı kullanıcıya göre değişmemeli).
- **H5h:** Ctrl+C ön plandaki süreci sonlandırır (kabuk kendisi değil); arka plandaki süreçler etkilenmez;
  kabuk `^C` yazıp yeni istem verir. `sonlandir` ile aynı çekirdek yolunu kullan.
- **H5i:** AltGr ile üretilen karakterler (`@ € { } [ ] \ | ~` — Türkçe Q tablosundaki tam liste)
  QEMU'da QMP `input-send-event` ile gönderilir; kabukta yazılan satır host'ta beklenen UTF-8 ile birebir.
- **H5j:** PS/2 kesme halkası taşması: halkayı gerçekten taşıracak kadar hızlı ve uzun dizi gönder
  (gerekirse çekirdeği belirli süre meşgul eden bir kabuk komutu sırasında); kayıp sayacı (`PS2_KAYIP`)
  artar, değeri host'un gönderdiği ve kabuğun aldığı karakter sayısı farkıyla birebir.
- Kâhin: `calistir_bm_x86_klavye16` — PS/2 ve USB için ayrı senaryolar; LED durumları için konuğun
  gönderdiği `0xED` baytları seri iz + QEMU tarafından (HMP/QMP ya da i8042 izi — seçimini yaz).
- Sabotajlar: Pause dizisinin 4. baytını yut → KIRMIZI; LED bit sırasını değiştir → KIRMIZI;
  Ctrl+C kabuğun kendisini sonlandırsın → KIRMIZI; kayıp sayacını artırma → KIRMIZI.

### Bölüm 2 — PCI'yı tamamla (H1f, H7a, H7b, H7c)
- **H1f:** `kem_port_oku16/32`, `kem_port_yaz16/32` ortak çifti (`kem_pci_hw.kem`'deki 32 bit erişimi
  bunun üstüne taşı); host birim testi + QEMU'da bilinen bir 16 bit yazmaç okuması.
- **H7a:** ACPI `MCFG` tablosunu ayrıştır (sans-IO, `drivers/acpi/mcfg.kem`); ECAM penceresini önbelleksiz
  eşle; yapılandırma alanı erişimini ECAM'e geçir, MCFG yoksa CF8/CFC'ye düş. İki yolun aynı aygıt için
  ilk 256 baytı birebir aynı okuduğu kapıyla kanıtlanır; ECAM ile 256 baytın ötesi (genişletilmiş
  yetenekler listesi) okunur ve listelenir.
- **H7b:** köprü arkası numaralandırma: QEMU'da `pcie-root-port` ve `pci-bridge` arkasına AHCI ve NVMe
  koy; ikincil/alt veri yolu numaraları doğru atanır, arkadaki diskler bağlanır ve okunur. Kâhin: QMP
  `query-pci` ağacı ile konuğun listesi birebir.
- **H7c:** BAR boyutlandırma (tümüne 1 yaz-geri oku-geri yükle), 64 bit BAR, prefetchable bit; MMIO
  BAR'ları önbelleksiz (UC) eşlenir. Konuk her BAR için taban/boy/tip/eşleme özniteliğini raporlar; host
  QMP `query-pci` BAR listesiyle karşılaştırır; sayfa tablosu girdilerinin PCD/PWT bitlerini konuk kendi
  tablosundan okuyup raporlar.
- Kâhin: `calistir_bm_x86_pci16` (en az üç QEMU topolojisi: düz, root-port arkası, pci-bridge arkası).
- Sabotajlar: ECAM adres hesabında işlev numarasını 1 bit kaydır → KIRMIZI; köprü alt veri yolunu
  güncelleme → arkadaki disk bulunamaz → KIRMIZI; 64 bit BAR'ın üst yarısını yok say → KIRMIZI.

### Bölüm 3 — kesme çerçevesi ve MSI (H1g, H7d, H3h)
- **H1g:** ortak, kaydedilebilir vektör arayüzü: `kesme_kaydet(vektor, isleyici_kimligi)`,
  `kesme_ayir()` (boş vektör ayırır), `kesme_birak`. IRQ1, IRQ12, LAPIC zamanlayıcı ve xHCI MSI-X bu
  arayüze taşınır (davranış değişmeden; mevcut kapılar yeşil kalır). Kesme başına sayaç `donanim`/`bilgi`
  raporunda.
- **H7d + H3h:** AHCI MSI (MSI yetenek yapısı, adres/veri, maske varsa), komut tamamlanması kesme ile
  bildirilir; yoklama yolu yedek olarak kalır ve hangisinin kullanıldığı raporlanır. NVMe için MSI-X
  (en az tamamlanma kuyruğu 1) önerilir — yapabiliyorsan ekle, yapamadıysan yaz.
- Kâhin: `calistir_bm_x86_msi` — konuk kesme sayaçlarını raporlar; AHCI okuma sırasında AHCI vektörünün
  sayacı artar, INTx/IOAPIC hattında AHCI için kesme gelmez. MSI kapalı (QEMU'da `ich9-ahci` için MSI'yi
  devre dışı bırakan seçenek varsa onu kullan; yoksa konukta yetenek yapısını yok sayan sabotajla) iken
  yoklama yedeği çalışır ve bunu raporlar.
- Sabotajlar: MSI veri alanına yanlış vektör yaz → kesme gelmez, zaman aşımı mesajı ve yoklama yedeği →
  kâhin "MSI bekleniyordu" diye KIRMIZI; kayıt arayüzünde iki isleyiciyi aynı vektöre bağlamaya izin ver
  → çakışma kapısı KIRMIZI.

### Bölüm 4 — disk hata kurtarma (H7e)
- AHCI: görev dosyası hatası (TFD.ERR / PxIS.TFES) sonrası port durdur → hata temizle → COMRESET
  gerekirse → portu yeniden başlat → isteği sınırlı sayıda (ör. 3) yeniden dene; başarısızsa okunabilir
  hata (sektör LBA'sı + durum baytları) ve kabuk devam eder; sonraki geçerli okuma çalışır.
- NVMe: tamamlanma durum kodu (SC/SCT) hata ise aynı biçimde sınırlı yeniden deneme + açık hata.
- USB depo: CSW hata durumu → REQUEST SENSE → sınırlı yeniden deneme + açık hata (dalga 15'te varsa
  yalnız kanıtını ekle).
- QEMU: `blkdebug` ile belirli sektöre/olaya hata enjekte et (bkz. §1 sözdizimi; `once = "on"` ile tek
  seferlik ve kalıcı iki ayrı senaryo). Tek seferlik hatada okuma yeniden denemeyle başarılı olur ve
  dosya içeriği host'taki imajla birebir; kalıcı hatada açık hata mesajı, kabuk devam eder, aynı diskin
  başka bir dosyası okunabilir.
- Kâhin: `calistir_bm_x86_disk_hata` (AHCI + NVMe + USB depo; her biri tek seferlik + kalıcı).
- Sabotajlar: yeniden deneme sayısını 0 yap → tek seferlik senaryo KIRMIZI; hata sonrası port yeniden
  başlatmayı atla → sonraki geçerli okuma da başarısız → KIRMIZI.

### Bölüm 5 — USB hub (H10e)
- Hub sınıfı sürücüsü (sans-IO `drivers/usb/hub.kem`): hub tanımlayıcısı, port gücü, port durumu/değişim
  bitleri, port sıfırlama, hız; xHCI'de route string + kök port + hub yuvası ile aygıt bağlamı (Slot
  Context `Route String`, `Parent Hub Slot ID`/`Parent Port Number` — gerekiyorsa TT alanları).
- QEMU: `-device usb-hub,bus=xhci.0,port=N` arkasına `usb-kbd`, `usb-mouse`/`usb-tablet` ve `usb-storage`;
  iki seviyeli hub (hub arkasında hub) senaryosu. Klavye yazar, fare hareket eder, hub arkasındaki diskten
  açılış ya da en azından bağlama + dosya okuma/yazma.
- Hub arkasına sonradan takma/çıkarma (QMP `device_add`/`device_del`): port değişim olayı işlenir, aygıt
  eklenir/kaldırılır, kabuk asılmaz.
- Kâhin: `calistir_bm_x86_usb_hub` (QMP `query-usb`/HMP `info usb` ile konuğun aygıt listesi birebir).
- Sabotajlar: route string'de port numarasını bir kaydır → arkadaki aygıt adreslenemez → KIRMIZI;
  port değişim bitini temizleme → ikinci takma görülmez → KIRMIZI.

### Bölüm 6 — ramdisk ve `/ram` (H7h UEFI karşılığı, H7j, H8a)
- Çekirdek açılışta ESP'deki (ya da açılış diskindeki) `ramdisk.img` dosyasını (yoksa sessizce değil,
  `log`a "ramdisk yok" yazarak atla) fiziksel çerçeve ayırıcısından alınmış bitişik belleğe okur ve bunu
  bir blok aygıtı olarak sunar (`drivers/blok` arayüzünü kullanan bellek sürücüsü). GPT/MBR ve FAT32
  aynı katmandan geçer, `/diskler/ram0p<n>` olarak bağlanır. `boot/efi/`'ye DOKUNMA — dosyayı çekirdek
  kendi FAT okuyucusuyla okur.
- `/ram`: bellek içi yazılabilir geçici dosya sistemi (VFS düğümleri; dizin, oluştur/yaz/oku/sil/yeniden
  adlandır; sınır = yapılandırılmış çerçeve sayısı, aşılırsa açık hata). Açılışta boş; kapanışta kaybolur.
  Boru hatları ve programlar `/ram` üzerinde de çalışır.
- **H7j:** kapı matrisine `ramdisk` satırı: {ramdisk} × {MBR, GPT} okuma (+ ramdisk yazma bellekte kalır;
  kapanışta diske yazılmaz — bunu da kanıtla).
- Kâhin: `calistir_bm_x86_ramdisk` (host imajı ile konuk okuması SHA256 birebir; `/ram`'de dosya
  oluştur → `kedi` → `bul` → sil; sınır aşımı mesajı).
- Sabotajlar: ramdisk blok sürücüsünde LBA→bayt ofset hesabını bir sektör kaydır → KIRMIZI; `/ram`
  silmede düğümü listeden çıkarma → `ls` kâhini KIRMIZI.

### Bölüm 7 — ACPI kapatma (H3g)
- DSDT'yi (FADT `x_dsdt`/`dsdt`) bellekte bul; tam AML yorumlayıcısı OLMADAN `_S5_` nesnesini tara
  (yaygın kalıp: `08 5F 53 35 5F 12 ...` — paket, `SLP_TYPa`/`SLP_TYPb` bayt/kelime önekleri `0A`/`0B`/
  sıfır/bir sabitleri; bilinen kalıpları ayrıştır, tanımadığında açık hata). PM1a_CNT (ve varsa PM1b_CNT)
  yazmacına `SLP_TYPx << 10 | SLP_EN (1 << 13)` yaz.
- `kapat` komutu sırası: dosya sistemlerini boşalt (dalga 14/15'in mevcut kapanış yolu) → ACPI S5 →
  başarısızsa açık mesaj ve mevcut yöntem (D-757 reset zinciri yalnız `yenidenbaslat` için).
- Sans-IO ayrıştırıcı `drivers/acpi/s5.kem` + host birim testleri (QEMU'nun gerçek DSDT'sinin bir dökümü
  fikstür olarak; ayrıca elle kurulmuş `_S5_` varyantları ve bozuk girdiler).
- Kâhin: `calistir_bm_x86_kapat` — QMP `SHUTDOWN` olayı (`guest-shutdown`) gelir, QEMU kendiliğinden
  çıkar (`-no-shutdown` KULLANMA), veri diski `fsck.fat` temiz.
- Sabotajlar: `SLP_EN` bitini atla → QEMU çıkmaz, zaman aşımı → KIRMIZI; `SLP_TYP` kaydırmasını 1 bit
  kaydır → KIRMIZI.

### Bölüm 8 — çerçeve belleği yazma-birleştirme (H4b)
- PAT MSR'ını (0x277) programla (bir girdiyi WC yap; diğerlerini varsayılan sırada bırak, değeri belgele),
  GOP çerçeve belleği sayfalarını o PAT indeksiyle eşle (PTE PAT/PCD/PWT bitleri), TLB boşalt.
- Konuk raporu: PAT MSR değeri, çerçeve belleği aralığının ilk/son sayfasının PTE bitleri, örnek bir
  sıradan sayfa ve bir MMIO sayfası (UC) için aynısı; host bu bitleri bağımsız olarak PAT tablosuna göre
  çözer ve beklenen tiple karşılaştırır.
- Ölçüm: tam ekran doldurma süresi WC öncesi/sonrası (TSC). QEMU TCG'de fark çıkmayabilir — çıkmazsa
  bunu yaz, kapı yalnız bitleri denetler; süre fiziksel PC kontrol listesine ölçüm adımı olarak eklenir.
- Kâhin: `calistir_bm_x86_pat` + mevcut ekran kâhinleri (görüntü değişmemeli).
- Sabotaj: çerçeve belleği PTE'sinde PAT bitini kur ama PAT girdisini WC yapma → kâhin tipi "UC" çözer →
  KIRMIZI.

### Bölüm 9 — bulanıklaştırmayı genişlet (H12b)
- Yeni korpuslar (sans-IO ayrıştırıcılar, C ve self-host ikisiyle, her biri sabit tohumlu ≥1000 örnek):
  PS/2 tarama kodu akışları (rastgele bayt dizileri + geçerli dizilerin mutasyonu: E0/E1 önekleri, yarım
  diziler), PS/2 fare paketleri (3/4 bayt, senkron biti bozuk), USB tanımlayıcıları (aygıt, yapılandırma
  + arayüz + uç zinciri: `bLength`/`wTotalLength` tutarsızlıkları, sıfır uzunluk), HID rapor tanımlayıcıları
  (öğe önekleri, uzun öğe, iç içe koleksiyon, aşırı rapor boyu), hub tanımlayıcısı (Bölüm 5), xHCI olay TRB'leri
  (bilinmeyen tür, yanlış döngü biti), MCFG (Bölüm 2), `_S5_` tarayıcısı (Bölüm 7), FAT uzun ad girdileri
  (UTF-16, sağlama toplamı, sıra numarası). Hiçbiri çökmez/asılmaz; her biri açık hata ya da geçerli sonuç.
- Mevcut `calistir_kem_os_ayristirici` takımına katılır (ayrı hedefe gerek yoksa genişlet).
- Sabotaj: USB yapılandırma tanımlayıcısı zincirinde `bLength == 0` denetimini kaldır → bulanıklaştırıcı
  sonsuz döngüyü (zaman aşımı) ya da çalışma zamanı sınır paniğini yakalar → KIRMIZI. **Bu sabotajı
  gerçekten uygula ve çıktısını yaz.**

### Bölüm 10 — UART gönderme sınırları + MMIO `volatile` denetimi (H10g kalan, H1h)
- **H10g:** `runtime/kdl_runtime_uart_16550.c` (LSR.THRE) ve `runtime/kdl_runtime_uart_pl011.c`
  (FR.TXFF) gönderme beklemelerine sınır: sınır dolunca bayt düşürülür ve düşen bayt sayacı artar (seri
  hat yoksa sistem asılmasın; alma/RX bekleme davranışı DEĞİŞMEZ — o bir kullanıcı olayı beklemesidir).
  Host birim testleri (`calistir_uart_16550_test`, `calistir_uart_pl011_test`) taklit aygıtla "THRE hiç
  gelmiyor" senaryosunu ekler. ARM kapıları: `calistir_kem_os_arm`, `calistir_qemu_cekirdek`,
  `calistir_uart_pl011_bare_metal`, `calistir_uart_16550_bare_metal` yeşil kalır. Dalga 15'teki bekleme
  envanteri tablosunu güncelle (artık sınırsız satır kalmamalı; kalırsa gerekçesi).
- **H1h:** MMIO durum biti bekleyen döngülerin IR'ında okumanın `load volatile` olduğu ve döngü dışına
  taşınmadığını ölçen kapı (`calistir_mmio_volatile`): çekirdek birleşik biriminin iki derleyiciyle
  ürettiği IR'da, bilinen MMIO bekleme işlevleri listesi (envanterden) için her döngü gövdesinde volatile
  yükleme var mı. `-O2` sonrası makine kodunda da döngü içinde bellek okuması kaldığını `objdump` ile
  denetle. Bir derleyicide eksik çıkarsa → C16-xxx bulgusu (derleyiciyi düzeltme, kapıyı kırmızı
  bırakma; o işlevi kapının beklenen-kırmızı listesine DEĞİL, bulguya yaz ve kapıdan çıkarma gerekçesini
  yaz).
- Sabotaj: 16550 TX sınırını kaldır → "THRE hiç gelmiyor" testi zaman aşımıyla KIRMIZI; MMIO kapısının
  işlev listesine volatile olmayan sıradan bir bellek okuması döngüsü ekle → kapı KIRMIZI (kapının ayırt
  ettiğini gösterir).

### Bölüm 11 — derleme ölçümü (H1a)
- `test/olcum/` altında sentetik ama gerçekçi ~20.000 satırlık bir KEMGU birimi üreten betik (sürücü
  benzeri yapı + işlevler + `eşleş` + diziler; deterministik). İki derleyiciyle `--llvm` süresi, IR satır
  sayısı, en büyük işlevin IR boyu, tepe RSS (`/usr/bin/time -v`). Birleşik `kem_os_x86_kabuk` birimi için
  aynı ölçüm.
- Ölçüm kapısı `calistir_derleme_olcum`: değerleri `otomasyon/chatgpt/codex16_bulgular.md`'ye ve
  makinece okunur bir dosyaya yazar; eşik (ölçülen değerin ~3 katı, gerekçesini yaz) aşılırsa KIRMIZI.
  **Derleyiciyi hızlandırmaya çalışma** (E4q Claude'da); yalnız ölç ve büyüme eğrisini (5k/10k/20k)
  raporla.
- Sabotaj: üreticiye yapay olarak katlanan bir yapı ekle (ör. aynı işlevin 10 katı) → eşik KIRMIZI.

### Bölüm 12 — kabuk ve araç kullanılabilirliği
- `bilgi` komutu: çalışma süresi, boş/toplam fiziksel çerçeve, çekirdek heap kullanımı, süreç sayısı,
  bağlı diskler + boş alan, kesme sayaçları (Bölüm 3), klavye/fare kaynağı ve düzen (Bölüm 1).
- `duzenle`: metin arama (ileri/geri, Türkçe büyük/küçük harf duyarsız seçenek), arama-değiştir (tek/tümü,
  onaylı), geri al/yinele (en az 64 adım; sınır belgelenir), satıra git.
- `dosyalar`: kopyala, taşı/yeniden adlandır, sil (onaylı), yeni dizin; işlem çapraz bağlama (`/ram` ↔
  FAT32 bölümü) çalışır; ESP salt okunur yazma denemesinde açık hata.
- Kabuk: `kopyala`/`tasi`/`sil` komutları aynı VFS yolunu kullanır (D-407; `dosyalar` ile ortak modül).
- Her biri için host birim testleri (saf mantık modülü) + QEMU kâhini (seri + ekran + host'ta imaj/fsck).
  Uzun düzenleyici senaryolarını taze açılışlara böl (E4k).
- Sabotajlar: geri alma yığınında bir adımı atla → KIRMIZI; çapraz bağlama kopyasında son kısmi kümeyi
  yazma → host karşılaştırması KIRMIZI; `bilgi`'de boş çerçeve sayısını bir çerçeve kaydır → kâhin KIRMIZI
  (host QEMU'ya verilen RAM ve bilinen ayrılmış bölgelerden bağımsız hesaplar).

### Bölüm 13 — yol haritası, sabotaj envanteri, belgeler (H12c, H12e)
- Yol haritası: bu dalgada tamamlanan her kısmi madde `[x]` + kapı adı; kalan kısmi notlar güncel.
  `codex16_ilerleme.md`'de madde → önceki durum → yeni durum → kanıt tablosu.
- **H12c:** sabotaj envanterine bu dalganın sabotajları + **dalga 9 öncesi** H maddeleri için en az bir
  sabotaj satırı (kaynakta kanıt varsa kaydet; yoksa o maddenin kapısında yeni bir sabotajı gerçekten
  uygula ve yaz — tahmini satır YOK).
- Belgeler: `KEMGU_OS_Klavye_Duzeni.md` (ABD düzeni, Pause/PrintScreen, Num Lock, LED, Ctrl+C),
  `KEMGU_OS_Mimari.md` (kesme arayüzü, MSI, ECAM, hub, ramdisk, `/ram`, ACPI S5, PAT),
  `KEMGU_OS_Syscall_ABI.md` (71+), `KEMGU_OS_Program_Yazma.md` (yeni çağrılar için bir örnek; örnek
  kapıda derlenip çalışır — `calistir_bm_x86_belge15` deseni), fiziksel kontrol listesine yeni adımlar
  (hub, LED, kapatma, PAT süre ölçümü, ramdisk).
- Kabukta `yardim <komut>` yeni komutları belgeyle tutarlı listeler (kapı denetler).

---

## 7. Kapılar (her bölümden sonra)
O bölümün yeni kapıları + `calistir_bm_x86_v1`, `calistir_bm_x86_uefi_pc`, `calistir_bm_x86_saglamlik`,
`calistir_bm_x86_cok_surec`, `calistir_bm_x86_boru`, `calistir_bm_x86_girdi15`, `calistir_bm_x86_program15`,
`calistir_kem_os_x86`; `kem_gorev_cekirdek.kem` ya da UART C dosyaları değişirse `calistir_kem_os_arm` +
`calistir_qemu_cekirdek`; sans-IO değişikliklerinde `calistir_surucu_diff`, `calistir_surucu_kosum`,
`calistir_chatgpt_kopru`, `calistir_kem_os_ayristirici`. Bölüm 2/3/5/6'dan sonra `calistir_kem_os_pc_matris`.
On üçüncü bölümden sonra `make test_tumu` (rc PIPESTATUS ile, atlamalar sayılmış ve sınıflandırılmış),
`calistir_check_kapisi`, `calistir_baremetal_diff`, `calistir_sifir_uyari` (UART C dosyaları için).

**Ölçüm tuzakları:** derle-çalıştır döngüsünde ikiliyi ve kullanıcı ELF'lerini her koşumdan önce `rm -f`;
disk imajı her koşumda yeniden üretilir; `echo "$(cmd) $?"` yazma; `--checkdump` tanı bassa da 0 döner;
QEMU üçlü hatada sessizce yeniden başlar (`-no-reboot` + tek açılış işareti); kâhin anında gönderirse bekleme
kusurları görünmez; Türkçe kaynak okuyan her Python betiği `encoding="utf-8"` kullanır (D-761: Windows CI);
sabotaj geri alındıktan sonra ilgili `.o`/ikiliyi `rm -f` et (bayat artefakt); `make | tail` çıkış kodunu
gizler (`${PIPESTATUS[0]}`); QEMU'da port 0x80 gecikmesi gerçek süreyi vermez (dalga 15: PIT kalibreli
bekleme kullan).

---

## 8. Bitirince
Draft PR `codex/tamamlama-dalga16`, gövde `codex16_ilerleme.md` (on üç bölümün her biri için: yapılanlar,
kapılar, sabotaj sonuçları — uygulanmayan sabotaj varsa AÇIKÇA, süre). `codex16_bulgular.md`: ölçülen
davranışlar ve VARSAYIM'lar, QEMU topolojileri, hata enjeksiyonu sonuçları, MSI/INTx sayaçları, PAT bitleri,
bulanıklaştırma istatistikleri, derleme ölçümleri ve büyüme eğrisi, güncel bekleme envanteri, derleyici
bulguları (C16-xxx), kalanlar. Auto-merge AÇMA, merge ETME.

## 9. Bu dalganın DIŞINDA
- E4k (bölge modeli / heap geri kazanımı, H1c, H8h), E4q (derleme süresi optimizasyonu), H1b (ayrık
  derleme), hata yalıtımı testleri (H9j/E3f): Claude.
- Pencere yöneticisi, ağ, ses, SMP (H10i), KEMFS (H8i), BIOS/GRUB ve multiboot2 (D-756: H2a/H2b/H2c/H2f),
  Secure Boot, fiziksel makine koşumları (H11c-f: Mehmet).
