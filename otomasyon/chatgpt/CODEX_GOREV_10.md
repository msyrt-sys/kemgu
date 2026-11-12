# CODEX GÖREVİ 10 — UEFI açılışı: kendi EFI stub'ı, GOP çerçeve tamponu, bellek haritası, ACPI, USB imajı

Dalga 1'in kuralları geçerlidir (`otomasyon/chatgpt/CODEX_GOREV.md` §2, §3, §5 — yeniden
oku), aşağıdaki **izin genişletmesi** dışında. Dalga 6'nın gerçek-QEMU kâhin kuralı da
geçerlidir (`CODEX_GOREV_6.md` §0). Soru sorma, durma; takılırsan (60 dk) kaydet ve geç.

## 0. Hedef ve neden UEFI önce
KEMGU-OS bugün yalnız QEMU `-kernel` (PVH) ile açılıyor; gerçek bir PC bunu yapamaz. v1 bitiş
tanımı (yol haritası H0f) "gerçek bir x86 PC USB bellekten açılır" der. Bu dalganın sonunda:
**QEMU + OVMF (UEFI firmware), USB bellek imajından açılan KEMGU-OS kabuğu** — ekran firmware'in
verdiği çerçeve tamponunda (GOP), bellek firmware'in bellek haritasından, ACPI tabloları
okunmuş. Aynı imaj ileride gerçek bir UEFI dizüstüne `dd` ile yazılabilecek (yol haritası H0g).

**H0b kararından sapma — ÖLÇÜLDÜ (Claude, 2026-11-12):** H0b "önce GRUB + multiboot2" diyordu.
Bu makinede (DGX Spark, ARM64 Ubuntu) x86 GRUB platformları YOK ve KURULAMIYOR: `/usr/lib/grub/`
yalnız `arm64-efi`; `grub-pc-bin` listede yok, `grub-efi-amd64-bin` aday sürümü yok. Kurulu ve
doğrulanmış olan: OVMF (`/usr/share/OVMF/OVMF_CODE_4M.fd` + `OVMF_VARS_4M.fd`), `clang
--target=x86_64-unknown-windows`, `lld-link`, `mkfs.fat`, `xorriso`. Claude şu zinciri uçtan uca
çalıştırdı: freestanding C → `lld-link /subsystem:efi_application` → `EFI/BOOT/BOOTX64.EFI` →
QEMU q35 + OVMF (pflash) + `-drive format=raw,file=fat:rw:<dizin>` → OVMF dosyayı başlattı,
`ConOut->OutputString` seriye "KEMGU EFI" yazdı. Yani H0b'nin İKİNCİ adımı (kendi UEFI stub'ı)
bugün yapılabilir, birincisi yapılamaz. BIOS/GRUB yolu bu dalganın DIŞINDA (bkz. §7).
⚠ Dikkat: `SIMPLE_TEXT_OUTPUT_PROTOCOL` başlık İÇERMEZ (ilk alan `Reset`); Claude'un ilk denemesi
buna başlık ekleyip sessizce yanlış işlevi çağırdı. Firmware yapılarını UEFI belirtiminden alan
alan yaz ve her birini çalışan OVMF'de ÖLÇ.

## 1. Ön koşul ve çalışma alanı
```bash
cd /home/msyrt/Desktop/kemgu && git fetch origin
git log origin/main --oneline | grep D-752     # yoksa 5 dk'da bir, en çok 2 sa; sonra DUR, kaydet
git worktree add ../kemgu-codex10 -b codex/uefi-dalga10 origin/main && cd ../kemgu-codex10
make -j8 build/kemgu build/codegen build/kdl_runtime.o
make calistir_bm_x86_ekran calistir_bm_x86_kabuk   # taban sağlam mı
date > otomasyon/chatgpt/codex10_ilerleme.md
```
Tek dal, tek draft PR.

## 2. Okunacaklar
- `boot/start_x86_64.S` (PVH girişi, sayfa tabloları 0..1 GB + 3..4 GB UC, GDT, 64 bit geçiş),
  `linker/bare-metal-x86_64.ld` (yükleme 1 MB, 28 MB heap).
- `runtime/mimari/x86_64/kem_ekran_bga.kem` (DISPI açıcı + ortak çizim + ayna) ve
  `kem_mmu_hw.kem` `kdl_panik` (panik ekranı, sabit kayıt alanı).
- `runtime/mimari/x86_64/kem_platform.kem` (DMA adresleri, Kanıt 7), `kem_pvh.kem` (PVH bellek
  haritası okuma — UEFI ikizi bunun yerini alacak).
- `drivers/acpi/{tablo,fadt,madt}.kem` (sans-IO ACPI ayrıştırıcıları), `drivers/acilis/multiboot2.kem`.
- `tests/qemu/{bm_kabuk,bm_ekran,psf_kahin,fat_yazici}.py`.
- `LOOP.md` E4n, E4o (sabit fiziksel adresler — bu dalga E4o'yu kapatır), D-752.

## 3. İzin genişletmesi (yalnız bu dosyalarda)
- **Yeni `boot/efi/`**: EFI stub'ı. **Freestanding C** (libc yok) ya da asm. Gerekçe: firmware
  işlev işaretçileri MS x64 çağrı kuralıyla çağrılır; KEMGU çağrı kuralı ifade edemez. Stub
  POLİTİKA İÇERMEZ: GOP bilgisi + bellek haritası + ACPI RSDP adresi + çekirdek ELF'ini yükle +
  `ExitBootServices` + çekirdeğe atla. Geri kalan her şey KEMGU'da.
- `boot/start_x86_64.S`: YALNIZ yeni bir 64 bit giriş noktası EKLE (UEFI uzun kipte devreder);
  `pvh_entry` yolu birebir korunur (bugünkü tüm kapılar onu kullanıyor).
- Yeni linker betiği gerekiyorsa `linker/` altında YENİ dosya; mevcutlara dokunma.
- `runtime/mimari/x86_64/` altında yeni `.kem` dosyaları (ör. `kem_uefi.kem`, `kem_bellek_harita.kem`,
  `kem_ekran_gop.kem`); `kem_ekran_bga.kem`, `kem_platform.kem`, `kem_mmu_hw.kem` (`kdl_panik`
  kayıt adresi) içinde E4o için gereken değişiklikler. `güvensiz`/`çıplak`/ham işaretçi yalnız
  bunlarda ve `test/ornekler/kem_os_x86_*.kem`'de. `drivers/**` saf kalır (yeni sans-IO
  ayrıştırıcı `drivers/acilis/` altına olabilir).
- Harness/Python: `test/bm_x86_*_harness.sh`, `tests/qemu/*`, `tools/codex/*`.
- Makefile (yeni hedefler + `test_tumu` + `.PHONY` + x86 CAT listesi), `test/check_kapisi.sh`
  ve `test/baremetal_diff_harness.sh` sınıflandırma satırları, `.github/workflows/ci.yml`'de
  YALNIZ apt paket listesine `ovmf` ekleme.
- YASAK: `src/`, `selfhost/`, runtime C dosyaları. Derleyici hatası → `codex10_bulgular.md`'ye
  C10-xxx + küçük tekrar üretimi, dolan. Muafiyet listelerine (`bolge_operand ENV_MUAF`,
  `check_genis MUAF`, `yapi_diff`, `stdlib_ithal MUAF`) satır EKLEME.

## 4. İş listesi
### A — EFI stub + 64 bit giriş (H2a'nın UEFI karşılığı)
- `BOOTX64.EFI`: `\KEMGU\CEKIRDEK.ELF`'i ESP'den oku (Simple File System / Loaded Image
  protokolleri), PT_LOAD bölümlerini bağlantı adreslerine yükle (`AllocatePages`
  `AllocateAddress`; başarısızsa AÇIK hata, sessiz devam yok), GOP + bellek haritası + RSDP'yi
  bir **açılış bilgisi yapısına** yaz (düzenini belgeye ve KEMGU tarafına birebir yaz),
  `ExitBootServices` (harita anahtarı değişirse belirtimdeki yeniden deneme), çekirdeğin 64 bit
  girişine `rdi = bilgi` ile atla.
- Çekirdek: yeni giriş kendi sayfa tablolarını/GDT'sini kurar (UEFI'ninkine güvenme), bilgi
  yapısını KEMGU'ya verir. Kâhin: seri "KEMGU-OS UEFI" + bilgi yapısındaki değerler.
- ACPI 2.0 GUID'ini EZBERDEN yazma: yapılandırma tablosundaki her girdinin işaret ettiği yerde
  `RSD PTR ` imzası + geçerli sağlama toplamı ara ve hangi GUID'in bulduğunu ÖLÇÜP yaz.
- Sabotaj: ELF yükleme adresini kaydır / bilgi yapısında bir alanı kaydır → kâhin KIRMIZI.

### B — GOP çerçeve tamponu (H4a)
- `ekran_gop_ac(bilgi)`: taban, satır adımı (piksel değil BAYT — ÖLÇ), genişlik, yükseklik,
  piksel biçimi. Biçim 32 bit BGRX değilse AÇIK hata. Çizim kodu `kem_ekran_bga.kem` ile ORTAK
  kalsın (ikinci bir çizici yazma; D-407: aynı soruyu iki yerde ayrı yanıtlama).
- Kâhin: OVMF altında QMP screendump + `psf_kahin.py` çizimi, piksel piksel. OVMF'nin seçtiği
  çözünürlüğü ölç ve yaz; kâhin onu çekirdeğin seriye bastığı değerden değil bağımsız olarak
  (screendump boyutundan) alsın.

### C — bellek haritasından fiziksel çerçeve ayırıcı (H2d) + E4o
- UEFI bellek haritasından kullanılabilir bölgeler (ExitBootServices sonrası hangi türlerin
  kullanılabilir olduğunu belirtimden al, ölçerek doğrula); çekirdek imajı, açılış bilgisi,
  çerçeve tamponu ve ACPI bölgeleri korunur.
- **E4o:** ekran aynası (bugün 200 MiB), panik kaydı (200 MiB + 64 KiB) ve DMA tabanları (128 MiB,
  `kem_platform.kem`) bu ayırıcıdan alınsın. PVH yolu da aynı ayırıcıyı PVH haritasıyla
  beslesin ya da bugünkü değerleri korusun — iki yol da yeşil kalmalı.
- Kanıt: `-m 256M`, `-m 512M`, `-m 2G` ile kabuk açılır. Sabotaj: ayrılmış bir bölgeyi
  kullanılabilir say → ölçülebilir bozulma (kâhinin bunu gördüğünü göster).

### D — ACPI (H3a + H3b)
- RSDP → XSDT (yoksa RSDT) → tablo imzaları listesi seriye. `drivers/acpi/tablo.kem` kullanılır.
- **Bağımsız kâhin:** QMP `pmemsave` ile RAM dökümü; Python dökümde `RSD PTR ` + sağlama
  toplamını kendisi bulur, XSDT'yi kendisi ayrıştırır, imza listesini çekirdeğin seri
  çıktısıyla karşılaştırır (KEMGU ayrıştırıcısını KULLANMA — döngüsel kâhin olur).
- Sabotaj: sağlama toplamı denetimini kapat + bozuk tablo → fark yakalanmalı.

### E — USB bellek imajı (gerçek donanımın yolu)
- `make kem_os_pc_img` → `build/kem_os_pc.img`: GPT + ESP (FAT32) içinde `EFI/BOOT/BOOTX64.EFI`,
  `KEMGU/CEKIRDEK.ELF` ve kabuk dosyaları (`merhaba.txt` vb.). BAĞIMSIZ Python yazıcısıyla
  (`tests/qemu/fat_yazici.py` deseni; KEMGU sürücüsünü kullanma). `fsck.fat -n` temiz.
- Kapı: aynı imaj OVMF ile **iki yolla** açılır: `-device usb-storage` (gerçek USB belleğin
  karşılığı) ve AHCI disk. Kabuk komutları (USB klavye), seri + GOP ekran birebir, disk SHA256
  salt okumada değişmemeli. İlk açılışta ≥ 8 sn tuşsuz bekleme (D-747 dersi).
- Gerçek donanım için ÇALIŞTIRMA YAPMA; yalnız `codex10_bulgular.md`'ye bir kullanıcının imajı
  USB'ye nasıl yazacağını ve iç diske dokunulmadığını nasıl doğrulayacağını yaz.

### F — zaman kalırsa
- **E4n:** x86'da dizi sınır ihlali panik ekranına çıksın (`kdl_dizi_oob`; arm64 kem_os'ta
  `kdl_panik` YOK, D-592 — mimariye özgü kanca gerekir).
- H2e: GDT + TSS + IDT tam kurulumu (çift hata için IST yığını); kasıtlı #PF/#GP/#DF → panik ekranı.

## 5. Kapılar
Her işten sonra: yeni `calistir_*` kapıların + `calistir_bm_x86_kabuk`, `calistir_bm_x86_ekran`,
`calistir_bm_x86_panik`, `calistir_kem_os_x86` (PVH yolu bozulmamalı). OVMF yoksa yeni kapılar
"ATLANDI (OVMF yok)" diyerek temiz çıksın (CI'da `ovmf` kurulana kadar). Bitirince `make
test_tumu` (rc PIPESTATUS ile, atlamalar sayılmış), `calistir_check_kapisi`,
`calistir_baremetal_diff` dahil. Her iş için sabotaj; aralıklı sınıflarda kaç açılışta kaç
kırmızı olduğunu YAZ.

**Ölçüm tuzakları:** derle-çalıştır döngüsünde ikiliyi her koşumdan önce `rm -f`; `echo "$(cmd) $?"`
yazma; `--checkdump` tanı bassa da 0 döner ("OK" satırını ara); kâhin komutu ANINDA yazarsa
boşta bekleme kusurlarını göremez; firmware yapılarında alan sırası ezberden değil belirtimden.

## 6. Bitirince
Draft PR `codex/uefi-dalga10`, gövde `codex10_ilerleme.md`; `codex10_bulgular.md`: ölçülen
firmware davranışları (GOP biçimi/çözünürlüğü, bellek haritası türleri, ACPI GUID'i ve tablo
listesi), açılış bilgisi yapısının düzeni, derleyici bulguları (C10-xxx), tasarım seçimleri,
gerçek donanım yönergesi, kalanlar. Auto-merge AÇMA, merge ETME.

## 7. Bu dalganın DIŞINDA (kaydedildi, yapma)
- BIOS/GRUB + multiboot2 yolu: x86 GRUB bu makinede kurulamıyor. Seçenekler (Mehmet'e): amd64
  çoklu-mimari depo eklemek ya da yalnız CI'da (x86 Ubuntu koşucusu `grub-pc-bin` kurabilir)
  ISO üretip test etmek.
- Secure Boot imzalama, ağdan açılış.
