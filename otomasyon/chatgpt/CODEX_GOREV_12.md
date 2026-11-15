# CODEX GÖREVİ 12 — gerçek diskte yazılabilir dosya sistemi: blok katmanı, çoklu bağlama, sektör tabanlı FAT32 yazma, kabuk dosya komutları

Dalga 1'in kuralları geçerlidir (`otomasyon/chatgpt/CODEX_GOREV.md` §2, §3, §5 — yeniden
oku), aşağıdaki **izin genişletmesi** dışında. Dalga 6'nın gerçek-QEMU kâhin kuralı da
geçerlidir (`CODEX_GOREV_6.md` §0). Soru sorma, durma; takılırsan (60 dk) kaydet ve geç.

## 0. Hedef
v1 bitiş tanımı (yol haritası H0f) madde 4: *"açıldığı USB bellekte FAT32 okuma/yazma VE iç
diskteki (SATA/NVMe) FAT32 bölümlerini bağlama — varsayılan salt okunur"* ve madde 5: *"`ls cd
cat mkdir rm cp` kabuğu"*. Bugün (Claude ölçtü, 2026-11-15):
- Disk sürücüleri yazmayı ve boşaltmayı ZATEN destekliyor: `nvme_yaz`/`nvme_flush`,
  `ahci_yaz`/`ahci_flush`, `usb_depo_yaz`/`usb_depo_flush` (sans-IO, `GcIstek`).
- Ama kabuk **tek bir diske** bağlanıyor (`KabukDisk.tur` birini seçer), yalnız **okuyor**
  (`FatSektor` = sektör tabanlı SALT OKUMA) ve komutları yalnız `ls cat echo kapat yenidenbaslat`.
- FAT32 yazıcısı (`drivers/fs/fat32_yaz.kem`: `dosya_yaz`, `dizin_olustur`, `dosya_sil`,
  LFN dahil) **bellekteki tam imaj** (`Dizi<dtam8>`) üzerinde çalışıyor — gerçek bir diskte
  (GB'larca) kullanılamaz.
- Bağlama mantığı (`drivers/fs/bagla.kem`: `bag_bolum`, `bag_tablosu`, `yazma_izni`), bölüm
  tablosu (`drivers/disk/bolum.kem`: `mbr_coz`, `gpt_coz`) ve yazma-geri önbelleği
  (`drivers/blok/onbellek.kem`) sans-IO olarak VAR ama donanıma bağlı DEĞİL.

Bu dalganın sonunda: QEMU + OVMF'de USB bellek + NVMe + AHCI disklerinin üçü birden takılıyken
açılan kabuk tüm FAT32 bölümlerini bağlar, açıldığı USB bölümünü yazılabilir, iç diskleri salt
okunur bağlar, `ls cd pwd cat yaz sil mkdir rmdir mv cp sync diskler` çalışır ve yazılan disk
**ana makinedeki `fsck.fat` ve bağımsız Python okuyucuyla** doğrulanır.

Yol haritası maddeleri: H7f, H7g, H7j, H8a (kısmen), H8d (gerçek disk yolu), H8e, H8f, H8j,
H8k, H8l, H8m, H9e (dosya komutları kısmı).

## 1. Ön koşul ve çalışma alanı
```bash
cd /home/msyrt/Desktop/kemgu && git fetch origin
git log origin/main --oneline | grep D-758     # yoksa 5 dk'da bir, en çok 2 sa; sonra DUR, kaydet
git worktree add ../kemgu-codex12 -b codex/disk-dalga12 origin/main && cd ../kemgu-codex12
make -j8 build/kemgu build/codegen build/kdl_runtime.o
make calistir_bm_x86_uefi_pc calistir_bm_x86_kabuk calistir_bm_x86_nvme calistir_bm_x86_ahci calistir_bm_x86_usb   # taban
date > otomasyon/chatgpt/codex12_ilerleme.md
```
Tek dal, tek draft PR.

## 2. Okunacaklar
- `test/ornekler/kem_os_x86_kabuk.kem` (`KabukDisk`, `kd_bul`, `kd_oku`, `kd_bagla`, `doldur`,
  komut döngüsü), `drivers/fs/fat32_sektor.kem` (sektör tabanlı okuyucu: `gerekli` sektörü
  isteyip yeniden deneyen desen), `drivers/fs/fat32_yaz.kem` + `fat32_kisa_ad.kem` +
  `fat32_onar.kem` (imaj tabanlı yazıcı, LFN, FSInfo), `drivers/fs/bagla.kem`,
  `drivers/disk/bolum.kem` + `crc32.kem` + `guid.kem`, `drivers/blok/onbellek.kem`.
- `drivers/{nvme,ahci}/cekirdek.kem`, `drivers/usb/depo.kem` (yaz + flush), `runtime/mimari/x86_64/kem_gc_yurut.kem`.
- `boot/efi/stub.c` (LoadedImage `image->aygit` zaten alınıyor; 192 baytlık açılış bilgisi ABI v1),
  `drivers/acilis/uefi.kem` (ABI okuyucu).
- `tests/qemu/fat_yazici.py`, `fat_kahin.py`, `uefi_imaj.py` (GPT + ESP, fsck temiz), `bm_uefi.py`,
  `uefi_kabuk_kahin.py`; `test/bm_x86_uefi_harness.sh`.
- `LOOP.md` D-747 (E4k: kabuk tuş başına ~3 KB sızıntı), D-755..D-757; E4q (self-host derleme süresi).

## 3. İzin genişletmesi (yalnız bu dosyalarda)
- `drivers/**` (sans-IO, saf). Yeni modüller: ör. `drivers/blok/aygit.kem`, `drivers/fs/fat32_sektor_yaz.kem`.
- `boot/efi/stub.c`: YALNIZ açılış bölümünün kimliğini açılış bilgisine eklemek için (bkz. B).
  ABI'yi büyütürsen **sürüm alanı** ekle/arttır ve okuyucu eski boyutu reddetmek yerine açık hata
  versin; PVH yolu bu bilgiyi hiç almaz → "açılış bölümü bilinmiyor" yolu açık olmalı.
- `runtime/mimari/x86_64/*.kem`, `test/ornekler/kem_os_x86_*.kem`, `test/bm_x86_*_harness.sh`,
  `tests/qemu/*`, `tools/codex/*`.
- Makefile (yeni `calistir_bm_x86_*` + `test_tumu` + `.PHONY` + x86 CAT listesi), `test/check_kapisi.sh`
  ve `test/baremetal_diff_harness.sh` sınıflandırma satırları.
- YASAK: `src/`, `selfhost/`, `linker/`, `boot/start_x86_64.S`, runtime C dosyaları. Derleyici
  hatası → `codex12_bulgular.md`'ye C12-xxx + küçük tekrar üretimi, dolan. Muafiyet listelerine
  (`bolge_operand ENV_MUAF`, `check_genis MUAF`, `yapi_diff`, `stdlib_ithal MUAF`) satır EKLEME.

## 4. İş listesi
### A — blok katmanı (H7f)
- Tek bir blok aygıtı soyutlaması: `çeşit` + `eşleş` (karar D8e: kapalı küme, dinamik dispatch YOK)
  — NVMe / AHCI / USB depo (+ isteğe bağlı bellek-içi ramdisk, test için). Ortak işlemler:
  `oku(lba, adet)`, `yaz(lba, veri)`, `bosalt()`, sektör boyu, kapasite. Sans-IO desen korunur.
- `drivers/blok/onbellek.kem` yazma-geri önbelleği bu katmanın üstüne; `bosalt()` sırası: kirli
  sektörleri yaz → aygıt flush'ı. Önbellek kapasitesi sabit (ör. 64 sektör) ve taşmada en eski
  KİRLİ sektör önce yazılır (sessiz düşürme YOK).
- Kabuk `KabukDisk.tur` tek-disk seçimini bırakıp **tüm** bulunan aygıtları bir listede tutar.
- ⚠ Heap (E4k): kabuk tuş başına ~3 KB sızdırıyor; büyük yazmalar daha fazlasını tahsis eder.
  Her yeni komut için **komut başına heap artışını ÖLÇ ve yaz**; test senaryoları 28 MB heap'i
  aşmamalı. Aşıyorsa senaryoyu küçült, DUR ve kaydet — bölge modelini düzeltmeye çalışma
  (Claude'un işi, escape v3).

### B — bölümler + çoklu bağlama + açılış bölümü + iç disk güvenliği (H7g, H8j, H8k, H8l)
- Her aygıtta MBR / GPT (GPT başlık + girdi CRC32 doğrulaması; yedek başlığa düşme davranışını
  seç ve yaz). FAT32 bölümlerini `bagla.kem` ile bağla; adlandırma şemasını seç ve yaz
  (ör. `usb0p1`, `nvme0p1`, `ahci0p2`) — kabukta `diskler` komutu tabloyu basar.
- **Açılış bölümü (H8k, D-756 kararı):** EFI stub `image->aygit` (LoadedImage.DeviceHandle)
  üzerinde aygıt yolu protokolünü açıp **HardDrive medya düğümünü** (tür 0x04, alt tür 0x01) bulsun
  ve bölüm imzasını (GPT bölüm GUID'i ya da MBR imzası + bölüm no) açılış bilgisine yazsın; çekirdek
  o bölümü kök + yazılabilir bağlasın. ⚠ Claude doğrulayamadı: `EFI_DEVICE_PATH_PROTOCOL` GUID'i
  (UEFI belirtimi: `09576e91-6d3f-11d2-8e39-00a0c969723b`) ve HardDrive düğüm düzeni (uzunluk 42;
  bölüm no u32 @4, başlangıç u64 @8, boy u64 @16, imza 16 B @24, MBR türü u8 @40, imza türü u8 @41)
  belirtimden alındı; OVMF ikilisi sıkıştırılmış olduğu için bayt aramasıyla doğrulanamadı (çalışan
  LoadedImage GUID'i de 0 kez bulundu). **Çalışan OVMF'de ÖLÇ:** protokol çağrısının başarısı ve
  okunan GUID'in imajı yazan Python'un koyduğu GUID'le eşleşmesi doğrulamadır.
- Açılış bölümü bulunamazsa (PVH yolu, eşleşme yok): tek FAT32 varsa o, yoksa salt okunur her şey
  + açık uyarı (sessiz tahmin YOK — bugünkü `bag_tablosu` davranışı).
- **İç disk güvenliği (H8l):** açılış bölümü dışındaki her bölüm SALT OKUNUR; GPT türü EFI Sistem
  Bölümü (ESP, `C12A7328-F81F-11D2-BA4B-00A0C93EC93B`) **ASLA** yazılmaz — açılış ESP'si dahil
  (çekirdek ESP'yi okuyabilir, yazamaz). Yazılabilir bağlama yalnız `bagla --yazilabilir <ad>` ile
  ve ESP hariç. Kâhin: ESP'ye ve salt okunur bir bölüme yazma girişimi → açık hata + disk SHA256
  değişmedi.

### C — sektör tabanlı FAT32 yazma + kabuk komutları (H8d gerçek disk yolu, H8e, H9e kısmen)
- `fat32_yaz.kem`'in mantığını (küme ayırma + FSInfo ipucu, iki FAT kopyası, LFN + kısa ad +
  sağlama, dizin büyütme) **sektör tabanlı** yeni bir modüle taşı: blok katmanından/önbellekten
  okuyup yazsın, imajın tamamını HİÇBİR ZAMAN belleğe almasın. Mevcut imaj tabanlı yazıcı ve
  testleri SİLİNMEZ; iki yazıcının aynı işlemler için **aynı baytları** ürettiğini host'ta ölçen bir
  test yaz (imaj tabanlı = kâhin).
- Yazma sırası (çökmeye dayanıklılık): veri kümeleri → FAT kopyaları → dizin girdisi → FSInfo,
  her aşamadan sonra önbellek boşaltma noktası; seçimi ve gerekçeyi yaz.
- **Kirli bayrak (H8e):** bağlarken "temiz değil" işaretle, `sync`/`kapat`'ta temizle. İki gelenek
  var — hangisini `fsck.fat`'ın okuduğunu ÖLÇ, varsayma: (a) Linux `fat_set_state`: önyükleme
  sektöründe FAT32 genişletilmiş BPB `state` baytı, `FAT_STATE_DIRTY 0x01` (Claude
  `/usr/include/linux/msdos_fs.h` `struct fat_boot_sector`'dan hesapladı: **ofset 0x41**); (b)
  Microsoft FAT belirtimi: FAT[1] girdisinin üst bitleri. Kirli kapanmış diski bağlarken uyar.
- Kabuk komutları: `cd pwd ls cat yaz <dosya> <metin> sil mkdir rmdir mv cp sync diskler bagla`.
  Yollar bağlama adıyla başlar ya da geçerli dizine göredir (`drivers/fs/yol.kem`, `yol_harf.kem`
  harf duyarsız eşitlik). **Türkçe ad** (`günlük.txt`, `çalışma/`) LFN ile yazılıp geri okunmalı.
- Seri çıktı + ekran ikisi de (mevcut ayna deseni); mevcut kâhinlerin beklentileri DEĞİŞMEZ.

### D — kâhinler + depolama matrisi (H7j, H8f, H8m)
- **Çapraz doğrulama (en güçlü kâhin):** QEMU kapandıktan sonra ana makinede her disk imajında
  `fsck.fat -n` temiz + bağımsız Python FAT okuyucusu (`fat_kahin.py`; KEMGU'dan bağımsız) yazılan
  dosyaların içeriğini ve Türkçe adlarını birebir okur. Tersi de: Python'un yazdığı dosyaları kabuk okur.
- **Matris (H8m):** USB bellek imajı (GPT: ESP + FAT32 veri) + NVMe (GPT FAT32) + AHCI (MBR FAT32)
  **aynı anda**; açılış aygıtı {USB} × bölüm tablosu {GPT, MBR}; her birinde: yazılabilir bağlama
  yalnız açılış bölümü, iç diskler + ESP SHA256 değişmedi, `--yazilabilir` sonrası iç diske yazma
  çalışır ve fsck temiz.
- **Kesilme dayanıklılığı:** yazma ortasında QEMU'yu öldür (QMP `quit` belirli bir komut
  sonrasında) → yeniden açılışta kirli bayrak uyarısı + `fsck.fat -n` sonucu kaydedilir (onarım
  bu dalganın işi değil; yalnız kirli bayrağın DOĞRU raporlandığını ölç).
- Sabotajlar: (1) ESP korumasını kaldır → ESP SHA256 değişir → KIRMIZI; (2) ikinci FAT kopyasını
  yazma → `fsck.fat` KIRMIZI; (3) LFN sağlamasını boz → Python okuyucu Türkçe adı göremez → KIRMIZI;
  (4) açılış bölümü eşleşmesini boz → yanlış bölüm yazılabilir olur / uyarı → KIRMIZI;
  (5) önbellek boşaltmayı atla → kapanış sonrası veri yok → KIRMIZI.

### E — zaman kalırsa
- H9h RTC (CMOS `0x70/0x71`) → `saat` komutu + yazılan dosyalara zaman damgası (fsck + Python
  okuyucu damgayı doğrular).
- H8g bozuk imaj dayanıklılığı: host'ta sans-IO ayrıştırıcıları (BPB, FAT zinciri, dizin, LFN, GPT)
  rastgele bozulmuş 1000 imajla koş — çökme/sonsuz döngü YOK, her biri açık hata ya da geçerli sonuç.

## 5. Kapılar
Her işten sonra: yeni `calistir_bm_x86_*` kapıların + `calistir_bm_x86_uefi_pc`,
`calistir_bm_x86_uefi_kabuk`, `calistir_bm_x86_kabuk`, `calistir_bm_x86_nvme`, `calistir_bm_x86_ahci`,
`calistir_bm_x86_usb`, `calistir_bm_x86_ps2_karma`, `calistir_kem_os_x86`; sans-IO değişikliklerinde
`calistir_surucu_diff`, `calistir_surucu_kosum`, `calistir_chatgpt_kopru`. Bitirince `make test_tumu`
(rc PIPESTATUS ile, atlamalar sayılmış), `calistir_check_kapisi`, `calistir_baremetal_diff` dahil.
Her iş için sabotaj; aralıklı sınıflarda kaç açılışta kaç kırmızı olduğunu YAZ. Disk kâhinlerinde
imaj her koşumdan önce YENİDEN üretilir (önceki koşumun yazdığı diskle sahte yeşil olmasın).

**Ölçüm tuzakları:** derle-çalıştır döngüsünde ikiliyi her koşumdan önce `rm -f`; `echo "$(cmd) $?"`
yazma; `--checkdump` tanı bassa da 0 döner ("OK" satırını ara); QEMU üçlü hatada sessizce yeniden
başlar (`-no-reboot`); `fsck.fat` çıkış kodu 0 = temiz, 1 = düzeltilebilir hata — `-n` ile koş ve
çıktıyı da sakla. ⚠ E4q: birleşik x86 birimi büyüdükçe self-host derleme süresi orantısız artıyor
(Windows CI `baremetal_diff` bütçesi 240 sn); birleşik birimin satır sayısını ve self-host
`--llvm` süresini başta ve sonda ÖLÇ ve yaz.

## 6. Bitirince
Draft PR `codex/disk-dalga12`, gövde `codex12_ilerleme.md`; `codex12_bulgular.md`: ölçülen
davranışlar (aygıt yolu düğümleri ve okunan bölüm kimliği, kirli bayrak geleneği ve `fsck.fat`'ın
okuduğu, yazma sırası, komut başına heap artışı, matris sonuçları, birleşik birim boyutu ve derleme
süresi), derleyici bulguları (C12-xxx), tasarım seçimleri, kalanlar. Auto-merge AÇMA, merge ETME.

## 7. Bu dalganın DIŞINDA (kaydedildi, yapma)
- E4k (bölge modeli) ve E4q (self-host derleme performansı): Claude.
- VFS'in tam hâli + dosya syscall'ları + süreçler (H8a geri kalanı, H9a-d): sonraki dalga.
- KEMFS (H8i), FAT onarımı (`fsck` benzeri), BIOS/GRUB (D-756), Secure Boot.
