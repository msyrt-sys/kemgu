# CODEX GÖREVİ 4 — depolama ve USB sürücülerinin saf mantığı (NVMe · AHCI · xHCI/HID · USB depo · blok · bağlama)

Dalga 1'in kuralları geçerlidir: `otomasyon/chatgpt/CODEX_GOREV.md` §2 (kesin kurallar),
§3 (iş döngüsü), §5 (bitirirken). **Yeniden oku.** Farklar ve iş listesi aşağıda.
Soru sorma, durma; takılırsan (45 dk) kaydet ve geç.

## 0. Ön koşul ve çalışma alanı
```bash
cd /home/msyrt/Desktop/kemgu && git fetch origin
git log origin/main --oneline | grep D-739      # yoksa 5 dk'da bir tekrar, en çok 2 sa
git worktree add ../kemgu-codex4 -b codex/surucu-dalga4 origin/main
cd ../kemgu-codex4
make -j8 build/kemgu build/codegen build/kdl_runtime.o build/kdl_runtime_mmio.o
bash otomasyon/chatgpt/dogrula.sh --kart
date > otomasyon/chatgpt/codex4_ilerleme.md
```
Tek dal, tek draft PR (dalga 1 gibi). Yalnız `../kemgu-codex4`.

## 1. MİMARİ — "sans-IO" çekirdek (ZORUNLU, her sürücüde)
Sürücü çekirdeği donanıma DOKUNMAZ. Bir adım işlevi bir **istek** döndürür, çağıran
isteği yerine getirip **yanıtı** bir sonraki adıma verir. Testte istekleri bir
**sahte aygıt modeli** yanıtlar; gerçek OS'te Claude aynı istekleri gerçek MMIO/DMA
ile yanıtlayacak. Böylece sürücü mantığı DEĞİŞMEDEN iki yerde çalışır.

Ortak sözlük `drivers/ortak/gc.kem` (A1'de yazacaksın):
```
genel çeşit GcIstek {
    Bitti,
    Oku32(tam64), Yaz32(tam64, tam64),           // yazmaç ofseti (BAR'a göreli)
    Oku64(tam64), Yaz64(tam64, tam64),
    BellekOku(tam64, tam32),                     // fiziksel adres, bayt sayısı
    BellekYaz(tam64, Dizi<dtam8>),
    Bekle(tam32),                                // mikrosaniye
    Hata(metin)
}
```
Yanıt: `Oku*` → okunan değer; `BellekOku` → baytlar; diğerleri → yanıt yok. Yanıtı
taşıyan yapıyı sen tasarla (ör. `yapı GcYanit { deger: tam64; baytlar: Dizi<dtam8>; }`).
Bu desen iki derleyicide ölçüldü ve çalışıyor (`çeşit` + `&değişken` durum + `eşleş` döngüsü).

**Kurallar:**
- Her bekleme döngüsünün üst sınırı var (zaman aşımı → `Hata`). Sonsuz bekleme YOK.
- Aygıttan gelen her sayı/uzunluk/indeks GÜVENİLMEZ: sınır denetimi, panik değil `Hata`.
- Sahte aygıt modelleri `tests/drivers/<dizin>/sahte_*.kem` içinde; sürücü modülü
  onları içe aktarmaz.
- Her sürücünün e2e testi: sahte aygıt + bellek içi disk imajı → mevcut
  `drivers/disk/bolum.kem` (GPT) + `drivers/fs/fat32_*` ile **gerçek bir dosyayı oku/yaz**.

## 2. Doğruluk kaynakları (EZBERDEN YAZMA)
Aşağıdaki ofsetler standartlardan alındı, ama **standart kazanır**. Her sabiti
mümkünse yerel bir kaynaktan Python ile doğrula ve test yorumuna komutu yaz:
- `/usr/include/linux/usb/ch9.h` (USB tanımlayıcı/istek sabitleri)
- `/usr/include/scsi/scsi.h` (SCSI işlem kodları)
- `/usr/include/linux/nvme_ioctl.h` (NVMe komut alanı sırası)
- `/usr/include/linux/input-event-codes.h` (KEY_* değerleri)
- `/usr/share/hwdata/pci.ids`, `/usr/share/hwdata/usb.ids`
Yerel kaynağı olmayan bit alanları (xHCI bağlamları, AHCI FIS) için emin olmadığın
her alanı `codex4_bulgular.md`'de "**QEMU'da doğrulanacak**" listesine yaz — Claude
gerçek QEMU aygıtıyla doğrulayacak. Sahte modeli kendi varsayımına göre yazıp
"geçti" demek yetmez; bu liste o boşluğun dürüst kaydıdır.

## 3. İş listesi
### Faz A — ortak
- **A0** `tests/drivers/virtio/virtio_blk_init_test.kem` 98–102: `blk_cfg_adres`
  seçili ithal edilmeden çağrılıyor (C T002×5, gerçek test hatası — LOOP E4f).
  İthalatı ekle; C ve self `--check` temiz olmalı. `surucu_diff` muafiyetini
  kaldırmak harness değişikliğidir (YASAK) → "kaldırılabilir" diye raporla.
- **A1** `drivers/ortak/gc.kem` (yukarıdaki sözlük + yanıt yapısı + bir örnek sahte döngü testi).
- **A2** `drivers/ortak/bayt.kem`'e büyük-endian okuyucu/yazıcılar (`oku_be16/32/64`,
  `yaz_be16/32/64`) — SCSI için. Mevcut API'yi değiştirme.
- **A3** `drivers/acilis/multiboot2.kem`: bilgi bloğu (toplam boy le32, ayrılmış le32,
  etiketler 8 bayt hizalı: tür le32, boy le32). Etiketler: 1 komut satırı, 2 açıcı adı,
  3 modül (başlangıç, bitiş, ad), 4 temel bellek, 6 bellek haritası (mevcut
  `drivers/bellek/harita.kem`'i kullan), 8 framebuffer (adres le64, adım le32, genişlik
  le32, yükseklik le32, bpp u8, tür u8; tür 1 = RGB: kırmızı/yeşil/mavi alan konumu ve
  boyu), 14 ACPI eski RSDP, 15 ACPI yeni RSDP (baytları mevcut `drivers/acpi/tablo.kem`
  `rsdp_coz`'a ver), 0 son. Ayrıca `komut_satiri_coz` (`kok=1A2B-3C4D sessiz hata=ayikla`
  → anahtar/değer listesi; `kok` değeri FAT32 seri no, onaltılık, `-` isteğe bağlı).
  Kâhin: Python ile bilgi bloğu üret.

### Faz B — NVMe (yol haritası H7i; AHCI'den ÖNCE)
- **B1** `drivers/nvme/kayit.kem`: CAP (0x00, 64-bit: MQES 0-15 sıfır tabanlı, CQR 16,
  TO 24-31 (500 ms birim), DSTRD 32-35, CSS 37-44, MPSMIN 48-51, MPSMAX 52-55),
  VS 0x08, CC 0x14 (EN 0, CSS 4-6, MPS 7-10, AMS 11-13, SHN 14-15, IOSQES 16-19,
  IOCQES 20-23), CSTS 0x1C (RDY 0, CFS 1, SHST 2-3), AQA 0x24, ASQ 0x28, ACQ 0x30.
  Kapı zili: SQ y kuyruğu `0x1000 + (2y) × (4 << DSTRD)`, CQ y başı `0x1000 + (2y+1) × (4 << DSTRD)`.
- **B2** `drivers/nvme/komut.kem`: 64 baytlık SQ girdisi (CDW0: işlem 0-7, FUSE 8-9,
  PSDT 14-15, CID 16-31; NSID @4; MPTR @16; PRP1 @24; PRP2 @32; CDW10..15 @40..60).
  Yönetim: Identify 0x06 (CDW10 CNS: 1 denetleyici, 0 ad alanı), I/O CQ oluştur 0x05,
  I/O SQ oluştur 0x01. G/Ç: Okuma 0x02, Yazma 0x01, Flush 0x00 (SLBA CDW10/11, NLB
  sıfır tabanlı CDW12 0-15). **PRP kuralı:** tek sayfa → PRP1; iki sayfa → PRP2 =
  ikinci sayfa; daha fazla → PRP2 = PRP listesinin adresi (sayfa hizalı 8'er baytlık
  girdiler; liste sayfası dolarsa son girdi sonraki liste sayfasını gösterir). 16 baytlık
  CQ girdisi: DW2 SQ başı 0-15 + SQID 16-31; DW3 CID 0-15, **faz biti 16**, durum 17-31
  (SC 17-24, SCT 25-27, DNR 31).
- **B3** `drivers/nvme/kimlik.kem`: denetleyici (4096 B: VID 0, SN 4-23, MN 24-63,
  FR 64-71, MDTS 77, NN 516-519) ve ad alanı (NSZE 0-7, NLBAF 25, FLBAS 26 bit 0-3,
  LBAF[i] @128+4i: MS 0-15, LBADS 16-23 → blok = 2^LBADS, RP 24-25). ASCII alanları
  sondaki boşluklar kırpılarak.
- **B4** `drivers/nvme/cekirdek.kem` (sans-IO): başlatma (CC.EN=0 → CSTS.RDY=0 bekle
  (sınır CAP.TO) → AQA/ASQ/ACQ → CC (EN=1, IOSQES=6, IOCQES=4, MPS=0) → RDY=1 bekle →
  Identify denetleyici → Identify ad alanı 1 → I/O CQ1 + SQ1 oluştur) ve
  `oku(lba, sayi)` / `yaz(lba, veri)` / `flush`. Halka matematiği: kuyruk sonu sarma,
  CQ başı ilerleme, **faz biti her turda tersine döner**. CFS → Hata; durum ≠ 0 → Hata;
  MDTS'yi aşan istek bölünür.
- **B5** `tests/drivers/nvme/sahte_nvme.kem` + e2e: kapı zili yazımında SQ'yu işler,
  CQ'ya doğru fazla yazar, Identify'ı fikstürden döndürür, disk imajına okur/yazar.
  E2E: başlat → GPT bul → FAT32'de dosya oku/yaz. Olumsuz: CFS, zaman aşımı, durum
  hatası, yanlış faz (eski girdi yeniden okunmamalı), sahte CID.

### Faz C — AHCI (H7e)
- **C1** `drivers/ahci/kayit.kem`: HBA: CAP 0x00 (NP 0-4, NCS 8-12, S64A 31), GHC 0x04
  (HR 0, IE 1, AE 31), IS 0x08, PI 0x0C, VS 0x10. Port n tabanı `0x100 + 0x80 n`:
  CLB 0x00, CLBU 0x04, FB 0x08, FBU 0x0C, IS 0x10, IE 0x14, CMD 0x18 (ST 0, FRE 4,
  FR 14, CR 15), TFD 0x20 (ERR 0, DRQ 3, BSY 7), SIG 0x24 (SATA = 0x00000101),
  SSTS 0x28 (DET 0-3 = 3, IPM 8-11 = 1), SCTL 0x2C, SERR 0x30, CI 0x38.
- **C2** `drivers/ahci/komut.kem`: 32 baytlık komut başlığı (DW0: CFL 0-4 = FIS dword
  sayısı (5), W bit 6, PRDTL 16-31; DW1 PRDBC; DW2-3 CTBA), komut tablosu (CFIS @0,
  ACMD @0x40, PRDT @0x80, girdi 16 B: DBA, DBAU, ayrılmış, DBC 0-21 = bayt−1 (çift
  sayı), I bit 31). H2D FIS (tür 0x27; bayt1 bit7 C=1; komut bayt2; LBA0-2 bayt 4-6;
  aygıt bayt7 = 64 (LBA kipi); LBA3-5 bayt 8-10; sayı bayt 12-13). Komutlar: IDENTIFY
  0xEC, READ DMA EXT 0x25, WRITE DMA EXT 0x35, FLUSH CACHE EXT 0xEA. PRDT girdisi en
  çok 4 MiB → büyük aktarım bölünür.
- **C3** `drivers/ahci/ata_kimlik.kem`: 512 B IDENTIFY: seri no kelime 10-19, model
  27-46 (**her kelimenin iki baytı YER DEĞİŞTİRİR**), kelime 83 bit 10 = LBA48,
  kelime 100-103 = LBA48 sektör sayısı, mantıksal sektör boyu kelime 106/117-118.
- **C4** `drivers/ahci/cekirdek.kem` (sans-IO): AE=1; PI'deki her port: SSTS DET=3 ve
  IPM=1 ve SIG SATA ise kullan; CMD.ST/FRE kapat, CR/FR temizlenmesini bekle (sınırlı),
  CLB/FB kur, FRE=1, SERR temizle, ST=1; komutu boş bir yuvaya kur, CI biti ver,
  CI temizlenene ya da IS.TFES'e kadar bekle; TFD.ERR → Hata. IDENTIFY → kapasite.
- **C5** `tests/drivers/ahci/sahte_ahci.kem` + e2e (B5 gibi) + olumsuz (aygıt yok,
  ATAPI imzası, TFES, zaman aşımı).

### Faz D — USB (H10c–f)
- **D1** `drivers/usb/tanim.kem`: aygıt (18 B), yapılandırma (9 B + toplam boy),
  arayüz, uç nokta (adres bit7 = IN; öznitelik 0-1 aktarım türü; azami paket boyu;
  aralık), HID (tür 0x21) tanımlayıcıları; yapılandırma bloğunu `bLength`/`bDescriptorType`
  ile gez (bLength 0 ya da taşma → Hata). SETUP paketi kurucuları (8 B): GET_DESCRIPTOR,
  SET_ADDRESS, SET_CONFIGURATION, HID SET_PROTOCOL, SET_IDLE. Sabitleri `ch9.h`'den.
- **D2** `drivers/usb/xhci_kayit.kem`: yetenek yazmaçları (CAPLENGTH 0x00, HCIVERSION
  0x02, HCSPARAMS1 0x04: MaxSlots 0-7, MaxIntrs 8-18, MaxPorts 24-31; HCSPARAMS2 0x08;
  HCCPARAMS1 0x10: AC64 0, CSZ 2, xECP 16-31; DBOFF 0x14; RTSOFF 0x18), işletim
  yazmaçları (USBCMD 0x00: R/S 0, HCRST 1, INTE 2; USBSTS 0x04: HCH 0, CNR 11;
  PAGESIZE 0x08; CRCR 0x18; DCBAAP 0x30; CONFIG 0x38), PORTSC `işletim + 0x400 +
  0x10 (n−1)` (CCS 0, PED 1, PR 4, PLS 5-8, PP 9, hız 10-13, CSC 17, PRC 21).
- **D3** `drivers/usb/xhci_trb.kem`: 16 B TRB (parametre 0-7, durum 8-11, denetim
  12-15: döngü biti 0, tür 10-15). Türler: Normal 1, Setup 2, Data 3, Status 4,
  Link 6, Enable Slot 9, Address Device 11, Configure Endpoint 12, Transfer olayı 32,
  Komut tamamlanma 33, Port durum değişimi 34. Halka modeli: üretici (Link TRB +
  döngü biti çevirme), olay halkası tüketicisi (döngü biti). Yuva/uç nokta bağlamı
  kurucuları (CSZ'ye göre 32/64 B) — **emin olmadığın her bit "QEMU'da doğrulanacak"**.
- **D4** `drivers/usb/hid_klavye.kem`: 8 baytlık önyükleme raporu (bayt0 değiştiriciler:
  bit0 sol Ctrl, 1 sol Shift, 2 sol Alt, 4 sağ Ctrl, 5 sağ Shift, 6 sağ Alt = AltGr;
  bayt 2-7 tuş kodları). Önceki raporla farkı → basış/bırakma. Tamamı 0x01 olan rapor
  (rollover hatası) yok sayılır. **Tuş kodu → mevcut `drivers/ps2/klavye_tr.kem`
  çekirdeği:** HID kullanım kodu → Linux KEY_* adı → `input-event-codes.h`'deki
  sayısal değer (ana blokta bu değerler PS/2 set 1 kodudur) → `klavye_bayt`.
  Ad eşlemesi: 0x04-0x1D = KEY_A..KEY_Z (alfabe sırası), 0x1E-0x27 = KEY_1..KEY_9,
  KEY_0, 0x28 ENTER, 0x29 ESC, 0x2A BACKSPACE, 0x2B TAB, 0x2C SPACE, 0x2D MINUS,
  0x2E EQUAL, 0x2F LEFTBRACE, 0x30 RIGHTBRACE, 0x31 BACKSLASH, 0x33 SEMICOLON,
  0x34 APOSTROPHE, 0x35 GRAVE, 0x36 COMMA, 0x37 DOT, 0x38 SLASH, 0x39 CAPSLOCK,
  0x64 102ND. Ok tuşları (0x4F sağ, 0x50 sol, 0x51 aşağı, 0x52 yukarı) ve 0x4C Delete
  set 1'de E0 önekli olduğu için doğrudan `Ozel` olayına çevrilir. Test: raporlarla
  "Ağaç İş" yazdır → kod noktaları doğru (Türkçe Q tablosundan).
- **D5** `drivers/usb/hid_fare.kem`: önyükleme raporu (bayt0 düğmeler, bayt1 dx int8,
  bayt2 dy int8, bayt3 tekerlek int8 varsa) → mevcut `FareOlayi`. **İşaret:** HID'de
  dy pozitif = AŞAĞI, `FareOlayi.dy` pozitif = YUKARI → ters çevir; test et.
- **D6** `drivers/usb/depo.kem`: BOT. CBW 31 B (imza 0x43425355 le32, etiket, veri boyu,
  bayrak bit7 = IN, LUN, CB boyu, CB 16 B), CSW 13 B (imza 0x53425355, etiket, artık,
  durum 0 tamam / 1 hata / 2 faz hatası). SCSI (**büyük-endian**): TEST UNIT READY 0x00,
  REQUEST SENSE 0x03 (anahtar bayt2 & 15, ASC 12, ASCQ 13), INQUIRY 0x12, READ CAPACITY(10)
  0x25 (son LBA + blok boyu, ikisi BE32), READ(10) 0x28 / WRITE(10) 0x2A (LBA BE32 bayt
  2-5, uzunluk BE16 bayt 7-8). Sans-IO durum makinesi: CBW → veri → CSW; etiket
  eşleşmeli; durum 1 → REQUEST SENSE; durum 2 → sıfırlama kurtarması isteği. Kodları
  `scsi.h`'den doğrula.
- **D7** `tests/drivers/usb/sahte_depo.kem` (disk imajı üzerinde BOT+SCSI aygıt modeli)
  + e2e: USB bellek benzetiminden GPT → FAT32 oku/yaz. Olumsuz: yanlış imza/etiket,
  faz hatası, kısa veri (artık ≠ 0).

### Faz E — blok katmanı ve çoklu bağlama (H7f, H8j–H8l)
- **E1** `drivers/blok/onbellek.kem`: sektör önbelleği modeli (LRU + kirli bit, geri
  yazma planı), istek bölme (aygıtın azami aktarımına göre). Saf.
- **E2** `drivers/fs/bagla.kem`: bağlama tablosu. Adlar: açılış bölümü `/`, diğerleri
  `/diskler/<nvme|sata|usb><aygıt no>p<bölüm no>`. **Açılış bölümü:** komut satırı
  `kok=` değeri FAT32 seri no (BPB ofset 67 le32) ile eşleşen bölüm; eşleşme yoksa:
  tek FAT32 varsa o + uyarı, yoksa ramdisk + uyarı (sessiz tahmin YOK).
  **Yazma politikası** `yazma_izni(...)`: açılış bölümü yazılabilir; diğerleri
  varsayılan SALT OKUNUR; `--yazilabilir` ile açılabilir; **EFI Sistem Bölümü HER
  ZAMAN salt okunur** (GPT tür `C12A7328-F81F-11D2-BA4B-00A0C93EC93B` — mevcut
  `drivers/disk/guid.kem`; MBR tür 0xEF de ESP sayılır). Sabotaj: ESP denetimini
  kaldır → test KIRMIZI olmalı.
- **E3** `tests/drivers/sim/depolama_matrisi_test.kem`: üç sahte aygıt aynı anda
  (USB: seri X'li FAT32 · NVMe: GPT + ESP + veri bölümü · AHCI: FAT32) + `kok=X`
  → bağlama tablosu doğru; `/`a yaz → tamam; `/diskler/nvme0p1` (ESP) yaz → hata;
  `--yazilabilir` ile veri bölümüne yaz → tamam, ESP'ye yine hata.

### Faz F — sağlamlık (zaman kalırsa)
- F1 Yeni her ayrıştırıcıyı `tests/drivers/bulanik/`e ekle (dalga 1 E2 deseni:
  LCG ile rastgele + tek-bayt mutasyon, panik yok).
- F2 Sınır değer tabloları (dalga 1 F5 deseni).

## 4. Kapılar ve süre
- Yeni test dizinlerini (`nvme ahci usb blok acilis`) Makefile'da YALNIZ
  `CHATGPT_TESTLER` listesine ekle. `make calistir_chatgpt_kopru` her işten sonra yeşil.
- Her test dosyası self-host ile Spark'ta **≤ 5 sn** olmalı (CI x86 2–3 kat yavaş;
  dalga 1'de 16 sn'lik test CI'yı düşürdü). Uzarsa iş yükünü değil algoritmayı düzelt.
- Bitirince: `make test_tumu 2>&1 | tee build/tumu.log; echo "rc=${PIPESTATUS[0]}"`
  — rc=0, FIXPOINT ✓, atlama/başarısızlık sayılmış.

## 5. Bitirince
Draft PR: `codex/surucu-dalga4`, gövde `codex4_ilerleme.md`. `codex4_bulgular.md`:
derleyici bulguları (dalga 1 biçimi) + **"QEMU'da doğrulanacak" listesi**. Auto-merge
AÇMA, merge ETME.
