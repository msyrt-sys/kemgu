# Dalga 10 — UEFI açılışı ve aynı ESP üzerinde kabuk

## Kapsam

KEMGU kabuğu artık QEMU/OVMF altında kendi `BOOTX64.EFI` yükleyicisiyle açılır.
Aynı GPT/ESP imajı USB bellek ve AHCI disk olarak kullanılır; çekirdek ve kabuk
verileri bu imajdadır. GOP çizimi mevcut çiziciyle ortaktır; fiziksel tahsisler
firmware haritasından gelir; ACPI tablosu listesi bağımsız RAM dökümüyle doğrulanır.

- A–E tamamlandı. Yeni EFI köprüsü, 24 alanlık ABI, UEFI giriş/linker, GOP,
  fiziksel ayırıcı/E4o, ACPI ve bağımsız FAT32/GPT imaj üreticisi eklendi.
- PVH yolu korundu. Derleyici/self-host/runtime C dosyaları ve eski linker
  değişmedi. `boot/start_x86_64.S` eski öneki 18.067 bayt birebir.
- İzinler `CODEX_GOREV_10.md` §3 kapsamındadır; CI değişikliği yalnız `ovmf` paketi.
- F/E4n ve TSS/IST sonraki çalışma olarak kaldı; BIOS/GRUB, Secure Boot ve fiziksel
  donanım denemesi bu PR'ın dışında. Gerçek PC yönergesi bulgu dosyasındadır.

## Test

- Mimari: yeni UEFI yolu x86_64 QEMU/OVMF; saf testler ARM64 konakta C/self-host.
- Beş yeni saf `.kem` test dosyasında 38 olumlu/olumsuz şekil; C/self-host
  `dogrula.sh` ve 135 dosyalık `calistir_chatgpt_kopru` yeşil.
- Gerçek OVMF: USB 5/5, AHCI 5/5; NVMe kabuk RAM matrisi 256M/512M/2G için
  5'er açılış. Self-host ayrıca iki ESP yolu ve üç RAM boyutunda doğrulandı.
- QMP tuşları, ilk açılışta ≥8 saniye boşta bekleme, yedi komut, Türkçe metin,
  canlı yankı/geri silme, 70 satır kaydırma, piksel birebir karşılaştırma.
- QMP disk yazılan bayt=0; açılış/kapanış SHA256 aynı. `make kem_os_pc_img`,
  bağımsız Python dosya okuyucu ve `fsck.fat -n` temiz.
- Her aşamada mevcut PVH kabuk/ekran/panik/kem_os, check kapısı ve bare-metal
  paritesi seri çalıştı. Bare-metal paritesi 23/23, sıfır atlama.
- Sabotaj: 5 gerçek QEMU ölçümü (her biri 1/1 kırmızı) + 5 saf/self-host kırmızı
  ölçüm; tüm bozma değişiklikleri geri alındı. Ayrıntılar aşağıda ve bulgularda.

## Dil özellikleri

Yeni dil özelliği, `tekkez`, `yetki` veya ISR eklenmedi. `drivers/` saf kalır.
Firmware MS x64 ABI köprüsü freestanding C'dir; yeni runtime `.kem` dosyalarında
izin verilen mevcut ham bellek ilkelleri kullanılır. C10-001/C10-002 için küçük
tekrar üretimleri ve dar dolanmalar `codex10_bulgular.md` dosyasındadır.

## Sonraki adım

Taslak incelemesi ve CI; ardından belgelenen sınırlarda, kullanıcı tarafından
ayrı bir fiziksel USB açılış denemesi. Auto-merge açılmadı; merge yapılmadı.

## Tam takım sonucu ve atlamalar

`make -j1 test_tumu 2>&1 | tee build/codex10/test_tumu.log` sonrasında
`rc=${PIPESTATUS[0]}` ile alınan sonuç **0**. `FIXPOINT ✓` görüldü; CODEGEN
99.048 IR satırında kararlı. Qtest altı dalın tamamında C+self-host ile yeşil;
bare-metal NVMe **20/20** açılış geçti. Yeni UEFI kapılarında **0 atlama**.
`calistir_check_kapisi`: 348/365, 17 sınıflandırılmış muaf, 0 RED;
`calistir_baremetal_diff`: 23/23, 0 atlama.

Koşum fırsatı bazında toplam **67 atlama** (aynı dosyanın ayrı kapıdaki koşumu
ayrı sayılır); ayrıca **4 ASan ALLOW** nedeniyle çalıştırılmayan örnek vardır:

| Kapı | Atlama | Gerekçe |
|---|---:|---|
| Codegen eşdeğerlik | 4 | 2 yabancı mimari + 2 tek başına bağlanamayan modül parçası |
| Self-driver, iki LLVM koşumu | 8 | Her biri aynı 227 dosyadan 223'ünü çalıştırır: 2 mimari + 2 modül |
| Geniş codegen | 8 | Host'ta bağlanamayan bare-metal MMIO örnekleri |
| Yapısal IR paritesi | 2 | Oracle IR/define karşılaştırması yok |
| Sürücü paritesi | 1 | Oracle IR/define karşılaştırması yok |
| Sürücü host-mock koşumu | 5 | C oracle --check reddediyor |
| ASan E2E | 39 | main yok, host --check/IR/link uygun değil; harness SKIP sayacı |

ASan sonucu PASS=133, FAIL=0, SKIP=39, ALLOW=4, SIZINTI-MUAF=2.
ALLOW adları: `35_binary_search`, `40_dizi_islemler`, `kem_mmio_ham`, `kem_pointer`.
Sızıntı muafiyetleri (`kanal_mesaj`, `gorev_temel`) yalnız sızıntı denetimini
süzüyor; diğer bellek denetimleri çalışıyor. Yeni UEFI örneği birleşik bare-metal
birim olarak denetlenir; tek başına host ASan örneği değildir.

Diğer kapı muafiyet/sınıflandırma sayaçları: stdlib_ithal=2, yapi_diff=24,
surucu_diff=1, check_genis=18, check_kapisi=17 (toplam 62 karşılaştırma girdisi).
Bunlar 67 atlama sayısına karıştırılmadı. Görevde dondurulmuş muafiyet listeleri
değişmedi. Adında “skip (v1)” geçen bound-metin testi gerçekten assertion çalıştırır,
bu yüzden atlama sayılmadı. Lean/lake kurulu değil; tam takımda 32 dosyanın
sorry/admit taraması geçti, gerçek Lean tip denetimi çalıştırılmadı.

Loglar `build/codex10/` altında: `test_tumu.log`, `test_tumu.rc`, `atlamalar.json`,
aşama ve sabotaj logları. `sgdisk -v build/kem_os_pc.img` ayrıca “No problems found”
dedi; denetim öncesi/sonrası imaj hash'i aynı.

## Dosya değişiklikleri (whitelist kontrol)

`git diff --name-only origin/main` + yeni dosyalar; tamamı görev §3 kapsamında:

```text
.github/workflows/ci.yml
Makefile
boot/efi/stub.c
boot/efi/uefi.h
boot/start_x86_64.S
drivers/acilis/uefi.kem
drivers/acilis/uefi_harita.kem
drivers/acpi/kok.kem
drivers/bellek/harita.kem
linker/bare-metal-uefi-x86_64.ld
otomasyon/chatgpt/codex10_bulgular.md
otomasyon/chatgpt/codex10_ilerleme.md
runtime/mimari/x86_64/kem_acpi_uefi.kem
runtime/mimari/x86_64/kem_bellek_harita.kem
runtime/mimari/x86_64/kem_ekran_bga.kem
runtime/mimari/x86_64/kem_ekran_gop.kem
runtime/mimari/x86_64/kem_mmu_hw.kem
runtime/mimari/x86_64/kem_platform.kem
runtime/mimari/x86_64/kem_uefi.kem
test/bm_x86_kabuk_harness.sh
test/bm_x86_uefi_harness.sh
test/check_kapisi.sh
test/ornekler/kem_os_x86_kabuk.kem
test/ornekler/kem_os_x86_uefi.kem
tests/drivers/acilis/gop_test.kem
tests/drivers/acilis/uefi_harita_test.kem
tests/drivers/acilis/uefi_test.kem
tests/drivers/acpi/uefi_kok_test.kem
tests/drivers/bellek/harita_yigin_test.kem
tests/qemu/acpi_uefi_kahin.py
tests/qemu/bm_uefi.py
tests/qemu/psf_kahin.py
tests/qemu/uefi_imaj.py
tests/qemu/uefi_kabuk_kahin.py
tools/codex/dalga10_sabit.py
tools/codex/efi_duzen.py
```

## İlerleme kaydı

Başlangıç: 2026-11-12T19:13:50.744781+00:00

- D-752 main: 98f8e13; yalnız kemgu-codex10/codex/uefi-dalga10.
- Taban derleme rc=0; ekran 5/5, kabuk 15/15 rc=0.
- VARSAYIM: PVH bilgi adres yuvası UEFI yolunda da kullanılır; magic ile ayrılır. PVH giriş talimatları korunur.

- [x] A · EFI ELF yükleyici, yeni 64-bit giriş, 24 alanlık ABI · C ve self OVMF ayrı 5/5 açılış ve dörder negatif ELF · eski EBS anahtarıyla ikinci deneme başarılı · ABI +8 sabotajı QEMU 1/1 kırmızı, magic sabotajı saf test kırmızı · seri kapılar rc=0 · 2026-11-12T19:28:50.544573+00:00

- [x] B · GOP açıcı + BGA ile ortak çizim · 11 saf olumlu/olumsuz şekil, C/self yeşil · QEMU C 5/5, self 1/1, 1.024.000 piksel birebir · adım+4: 1/1 kırmızı (2324 piksel); biçim koruması: exit=2 · tüm seri kapılar rc=0, baremetal_diff 23/23, 0 atlama · 2026-11-12T19:38:36.717948+00:00

- [x] C · UEFI harita/çerçeve ayırıcısı, E4o ayna/panik/DMA tahsisi, 64-bit PCI MMIO sayfa tabloları · C UEFI 15/15 (256M/512M/2G), self 3/3; QMP fiziksel aralık/UC doğrulaması · ayrılmış sayfa sabotajı 1/1 kırmızı, runtime niteliği exit=1; 800 bölgeli yığın sabotajı self exit=139 · C10-001/C10-002 kaydedildi, yığın dolanması · son seri kapılar rc=0, köprü 134 OK, baremetal_diff 23/23 ve 0 atlama · 2026-11-12T20:25:58.468452+00:00

- [x] D · XSDT/RSDT listeleme + bağımsız RAM ACPI kâhini · 9 saf şekil C/self yeşil; self kabuk 1/1; gerçek RSDT geri düşme yeşil · sağlama sabotajı saf exit=1 ve QEMU 1/1 kırmızı · tüm seri kapılar rc=0; köprü 135 OK, baremetal_diff 23/23, 0 atlama · 2026-11-12T20:42:25.961062+00:00

- [x] E · bağımsız GPT/ESP üreticisi, aynı imaj USB 5/5 + AHCI 5/5; self 2/2 · tam seri/GOP, 8 sn boşta, sıfır disk yazısı/SHA256 · bölüm+1 sabotajı QEMU 1/1 kırmızı · seri kapılar rc=0; nihai CHS imajı tam takımın PC kapısında 10/10 · 2026-11-12T21:05:14.887971+00:00

Bitiş: 2026-11-12T21:31:44.642517+00:00; toplam süre: 2:17:53.
A–E tamamlandı; 5 yeni saf test dosyası/38 şekil, 10 kırmızı sabotaj ölçümü.
F kalemleri kaldı; C10-001/C10-002 kaydedildi. Gerçek donanım çalıştırılmadı.
