# Dalga 12 — gerçek diskte yazılabilir FAT32

## Kapsam

UEFI kabuğu NVMe, SATA/AHCI ve USB FAT32 bölümlerini sektör üzerinden bağlar. Açılış bölümünü firmware aygıt yolundaki GPT bölüm GUID'i ya da MBR imzası ve bölüm numarasıyla seçer. MBR veri bölümünden açılışta kök yazılabilir. GPT açılış ESP'si salt okunur ve USB veri bölümüne `bagla --yazilabilir` ile açıkça izin verilir. İç diskler varsayılan salt okunur.

`ls cd pwd cat yaz sil mkdir rmdir mv cp sync diskler bagla` ve `saat` seri ile ekranda çalışır. RTC, FAT zaman damgalarını besler. Bağlama/yazma kuralları ve ölçülen donanım davranışı `otomasyon/chatgpt/codex12_bulgular.md` dosyasında.

Taban `b900e17`, D-758 mevcut. Çalışma alanı `../kemgu-codex12`, tek dal `codex/disk-dalga12`.

## Test

PVH tek bölüm bilinmeyen açılış uyarısını `diskler` ve yazma reddinde gösterir. Eski açılış çıktısı aynı kaldı (`f0692d1`).

Son koşu `make -j1 test_tumu calistir_kem_os_x86` ile seri çalıştı. `PIPESTATUS[0]=0`, **FIXPOINT ✓** (100378 IR satırı).

| Yeni gerçek QEMU kapısı | C + self-host |
| --- | ---: |
| Blok yazma, flush ve 65/64 sektör LRU | 2/2 |
| GPT ve MBR üç disk yazma matrisi | 4/4 |
| Kirli kapanış ve yeniden açılış (2 çift) | 4/4 |
| İki USB BOT, USB klavye ve kabuktan kapatma | 2/2 |
| **Toplam** | **12/12, 0 atlama** |

Host denetimleri **20 temiz fsck.fat -n** sonucu verdi. Beklenen **2 kirli kesinti** rc=1 verdi ve yeniden açılışta seri uyarı görüldü. FAT kopyaları, FSInfo, Türkçe LFN, dosya içeriği ve RTC damgaları bağımsız Python okuyucusuyla denetlendi. Koruma ölçümü ESP ve yazma izni verilmemiş iç disklerin SHA256 değerlerini karşılaştırdı.

`calistir_check_kapisi` 354/375, 21 birleşik/parça sınıflandırması, 0 RED. `calistir_baremetal_diff` 23/23, 0 atlama. Birleşik x86 birimi 513/513 noredzone. Sürücü paritesi 17/17, host-mock 4/4, köprü kart + 141 test. Mevcut güç, PS/2, UEFI, x86 kabuk ve NVMe/AHCI/USB kapıları, qtest ve aarch64 regresyonları tam takım içinde çalıştı.

Takımda **69 atlama kaydı** var (benzersiz dosya sayısı değildir): ASan 43, semantik eşdeğerlik 6, geniş codegen 8, yapısal IR 2, sürücü paritesi 1, bölge operandı 4, host-mock 5. ARM üst bellek alt denetiminde 1 koşullu atlama. ASan PASS=133, FAIL=0, bilinen ALLOW=4, sızıntı muafı=2. Açılış başına yeni kapı atlaması yok.

## Sabotaj

| Bozulan kural | Gerçek QEMU kırmızı / koşum |
| --- | ---: |
| Kirli LRU girdisi yazılmadan atıldı | 1/1 |
| ESP yazma koruması kaldırıldı, bölüm SHA256 değişti | 1/1 |
| EFI bölüm imzası değişti, kök eşleşmesi ve varsayılan izin kalktı | 1/1 |
| İkinci FAT kopyası atlandı, fsck.fat rc=1 | 1/1 |
| LFN sağlaması bozuldu, bağımsız okuyucu reddetti | 1/1 |
| Önbellek boşaltma atlandı, kapalı imajda dosya yok | 1/1 |
| RTC tarih damgası sabitlendi, host okuyucu reddetti | 1/1 |
| İkinci USB BOT için eski olay konumu kullanıldı | 1/1 |
| **Toplam** | **8/8** |

Sabotajlar ayrı çalışma kopyasında tekrarlandı ve geri alındı. Gerçek QEMU logları `build/dalga12_son_sabotaj.log` dosyasında.

## İlerleme

- [x] A · ortak NVMe/AHCI/USB blok katmanı, 64 sektörlük yazma önbelleği · `3453313`.
- [x] B · GPT ve MBR kimliği, çoklu bağlama, salt okunur iç disk ve ESP koruması · `aa4826f`.
- [x] C · sektör tabanlı FAT32 veri/FAT/dizin/FSInfo güncellemesi. Üç kümelik dosya, Türkçe LFN, silme ve dizin büyümesinde eski yazıcıyla tam bayt eşliği · `446958a`.
- [x] D · kabuk komutları ve gerçek disk matrisi. İki USB depo aygıtı aynı xHCI halkasını sırayla kullanır · `6b27055`.
- [x] E · CMOS RTC, FAT zaman damgaları, sabit tohumla 1000 bozuk imaj örneği · `6949377` ve `6b27055`.

## Heap ve derleme

`kem_bump - __heap_start` üst sınırı yeni QEMU koşularında **11,019,056 bayt** (28 MiB altında). Komut başı en büyük ölçülen artışlar:

| Komut | Bayt |
| --- | ---: |
| `bagla` | 590,256 |
| `cat` | 524,800 |
| `cd` | 196,752 |
| `cp` | 1,049,376 |
| `diskler` | 0 |
| `mkdir` | 787,040 |
| `mv` | 1,180,512 |
| `pwd` | 0 |
| `rmdir` | 459,088 |
| `saat` | 0 |
| `sil` | 655,840 |
| `sync` | 65,584 |
| `yaz` | 983,952 |

Self-host `--llvm --mimari x86_64-none` aynı makinede ölçüldü:

| Birim | Kaynak satırı | IR satırı | Saniye |
| --- | ---: | ---: | ---: |
| kabuk taban | 4821 | 114707 | 19.699 |
| kabuk son | 5358 | 132926 | 24.163 |
| x86 taban | 6184 | 37497 | 8.792 |
| x86 son | 6242 | 41088 | 8.931 |

İlk oturumdaki kabuk tabanı ayrıca 10,343 sn ölçülmüştü. Yukarıdaki dört koşu aynı doğrulama oturumundadır. Windows 240 sn bütçesi bu makinede ölçülmedi.

## Dil özellikleri ve kapsam denetimi

Kapalı çeşit ve eşleşme ile sürücü seçilir. Saf sürücüler yalnız GcIstek üretir. Yeni kesme işleyicisi, `tekkez` veya `yetki` kullanılmadı. C12-001 ve C12-002 iki derleyiciyle küçük örnekte tekrarlandı. Derleyici kaynakları ve mevcut muafiyet listeleri değişmedi.

Dalga 12 izin genişletmesiyle 38 dosya. `git diff --check origin/main` temiz.

```text
Makefile
boot/efi/stub.c
drivers/blok/aygit.kem
drivers/disk/bolum_kimlik.kem
drivers/fs/bagla.kem
drivers/fs/fat32_kayit.kem
drivers/fs/fat32_sektor.kem
drivers/fs/fat32_sektor_yaz.kem
drivers/fs/fat32_yaz.kem
drivers/fs/yol_harf.kem
drivers/usb/depo.kem
drivers/usb/xhci_cekirdek.kem
drivers/zaman/rtc.kem
otomasyon/chatgpt/codex12_bulgular.md
otomasyon/chatgpt/codex12_ilerleme.md
runtime/mimari/x86_64/kem_rtc_hw.kem
runtime/mimari/x86_64/kem_uefi.kem
test/bm_x86_blok_harness.sh
test/bm_x86_disk_harness.sh
test/bm_x86_kabuk_harness.sh
test/bm_x86_uefi_harness.sh
test/check_kapisi.sh
test/ornekler/kem_os_x86_blok.kem
test/ornekler/kem_os_x86_blok_ortak.kem
test/ornekler/kem_os_x86_kabuk.kem
tests/drivers/blok/aygit_test.kem
tests/drivers/disk/bolum_kimlik_test.kem
tests/drivers/fs/fat32_bozuk_test.kem
tests/drivers/fs/fat32_sektor_yaz_test.kem
tests/drivers/fs/rtc_test.kem
tests/qemu/blok_kahin.py
tests/qemu/dalga12_fat.py
tests/qemu/dalga12_imaj.py
tests/qemu/dalga12_kahin.py
tools/codex/dalga12_blok_sabotaj.py
tools/codex/dalga12_blok_uret.py
tools/codex/dalga12_bolum_uret.py
tools/codex/dalga12_disk_sabotaj.py
```

## Sonraki adım

Tek taslakta inceleme. Fiziksel PC denemesi yapılmadı. 512 bayt sektör, birincil MBR ve ana GPT, mevcut NVMe NSID 1 ve bir xHCI denetleyicisi kapsamı geçerli. `mv` normal dosyaları taşır, dizin taşıma ve mevcut hedefi ezme desteklenmez.

Başlangıç 2026-11-15T19:23:17.111380+03:00, 2026-11-19 tarihinde devam edildi. Son tam koşu 2026-11-19T10:49:42+03:00 → 2026-11-19T11:56:01+03:00, süre 1:06:19. Oturumlar arasındaki bekleme çalışma süresine dahil edilmedi.
