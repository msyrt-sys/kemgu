# Dalga 11 — kesme altyapısı

## Kapsam

UEFI çekirdeği MADT üzerinden IOAPIC/LAPIC kurar, LAPIC zamanlayıcısını PIT ile kalibre eder ve PS/2 klavye/fareyi IRQ1/IRQ12 üzerinden işler. Bozuk yığınla #DF dahil istisnalar ve dizi sınır ihlalleri heap kullanmadan kırmızı PANIK ekranına ve seri hatta yazılır. Kabuk fare hareketlerini ve düğmeleri bildirir, save-under imleç çizer; USB klavye ve USB FAT32 ile birlikte çalışır. FADT tabanlı PS/2 varlık denetimi, yeniden başlatma ve sınırlı kapatma yolu da eklendi.

Çalışma alanı: `../kemgu-codex11`, dal `codex/kesme-dalga11`. Taban `26e9546e2b515039f087b8e1759b19fce12e1fdb`; D-756 ön koşulu doğrulandı. Ana klasörde ve diğer worktree’lerde kod değiştirilmedi.

## Test

Son komut seri çalıştırıldı:

```bash
set -o pipefail
make -j1 test_tumu calistir_kem_os_x86 calistir_kem_os_arm \
  2>&1 | tee build/dalga11_test_tumu.log > build/dalga11_test_tumu_komut.log
rc=${PIPESTATUS[0]}
```

**rc=0; FIXPOINT ✓**, stage1/stage2 IR birebir, **100270 satır**. Son takımda beş yeni QEMU kapısı, iki yeni saf sürücü test dosyası ve iki yeni istisna/güç çekirdeği bulunuyor.

| Yeni kapı | C + self-host gerçek açılış | Sonuç |
| --- | ---: | --- |
| `calistir_bm_x86_istisna` | 7 şekil × 2 × 10 = 140 | 140/140 |
| `calistir_bm_x86_apic` | 2 × 10 = 20 | 20/20 |
| `calistir_bm_x86_ps2_irq` | 2 × 10 = 20 | 20/20 |
| `calistir_bm_x86_ps2_karma` | 2 × 10 = 20 | 20/20 |
| `calistir_bm_x86_guc` | 4 şekil × 2 = 8 | 8/8 |
| **Toplam** | **208** | **208/208; 0 atlama** |

İstisna şekilleri #DE, #UD, #GP, #PF, bozuk RSP ile #DF ve iki dizi sınır ihlalidir. QEMU `-no-reboot`, tek açılış sayacı, tam seri/piksel karşılaştırması, HLT, IST aralığı ve kanaryalar denetlenir. APIC ve PS/2 kapıları 8 saniye boşta bekler. IRQ klavye kapısı 12×48 hızlı tuş, halka sarımı, kayıp sayacı; fare kapıları kayık AUX baytıyla senkron kurtarma ve 1024000 piksel save-under karşılaştırması yapar. Karma kapı aynı xHCI üzerinde USB klavye ve FAT32 okumasını da korur.

Mevcut panik, PS/2, kabuk, UEFI kabuk/PC, NVMe/AHCI/USB ve qtest kapıları yeşil. `calistir_check_kapisi`: **352/371, 19 sınıflandırılmış, 0 RED**; sınıflandırılan yeni çekirdekler birleşik birim olarak ayrıca denetlenir. `calistir_baremetal_diff`: **23/23, 0 atlama**. Köprü: **kart + 136 test**. x86 noredzone: **493/493**, RSP-altı erişim 0; ring3 ve kurtarılabilir #PF yeşil. ARM64 24 faz ve qemu_cekirdek yeşil; son komutta ARM hedefi daha önce tamamlandığı için “up to date” bildirdi.

Atlamalar ayrı sayıldı: ASan 41; üç semantik eşdeğerlik koşumunda 2+2+2 yabancı mimari; geniş codegen 8; yapısal parite 2; sürücü paritesi 1; sürücü host-mock 5; bölge operandı 4. **Toplam 67 atlama kaydı** (benzersiz dosya sayısı değildir). Ayrıca ARM çekirdeğinin RAM≤1 GiB şekli üst bellek alt-denetimini 1 kez atladı. ASan: PASS=133, FAIL=0, bilinen ALLOW=4, sızıntı muafı=2. Mevcut muafiyet listelerine satır eklenmedi.

## Kırmızıdan yeşile ve sabotaj

Her yeni kâhin önce eski çekirdekte kırmızı oldu: IST’siz #DF’de istisna satırı yok; APIC, IRQ ve FADT kâhinlerinde ilgili kurulum/ölçüm alanları yok. Düzeltmelerden sonra gerçek QEMU tekrarları yeşil oldu.

| Bozulan kural | Gerçek QEMU kırmızı / açılış |
| --- | ---: |
| #DF kapısında IST=0 | 10/10 |
| İstisna ekran yolu kapalı | 1/1 |
| OOB mimari kancası boş | 1/1 |
| MADT ISO yok sayılıyor | 10/10 |
| IRQ12 maskeli | 10/10 |
| Fare ilk bayt senkron denetimi kaldırıldı | 10/10 |
| USB bekleyişi fare tüketicisine yol vermiyor | 10/10 |
| FADT reset yazısı kaldırıldı | 1/1 |
| **Toplam** | **53/53** |

ISO, fare senkronu ve FADT 8042 biti saf testlerde de çalışma sırasında kırmızı oldu. Sabotajlar geri alındı; son tam takım özgün kaynaklarla geçti. Tekrar araçları `tools/codex/dalga11_*sabotaj.py` dosyalarındadır.

## İlerleme ve tasarım seçimleri

- [x] A/B · #DF/NMI/#MC için ayrı 16 KiB IST; heap’siz ekran/seri istisnalar, mimariye özel OOB kancası · 140/140 ve üç sabotaj · commit `c9e978c`, 2026-11-13 20:08.
- [x] C · MADT/ISO, UC IOAPIC/LAPIC eşlemesi, PIT kalibrasyonu, LAPIC zamanlayıcı · 20/20; ISO 10/10 kırmızı · commit `57af204`, 2026-11-13 20:45.
- [x] D · kesme-güvenli halkalar, PS/2 fare, save-under imleç, USB karma yol · 20/20 + 20/20; üç sabotaj 30/30 kırmızı · commit `47125d0`, 2026-11-13 22:45.
- [x] E · FADT güç görünümü, PS/2 varlığı, reset geri dönüşleri, kabuk komutları · C+self 8/8; saf bit ve gerçek reset sabotajları kırmızı; tam takım rc=0.
- VARSAYIM: IST taşması için alt kanarya seçildi; koruma sayfası yok. Kanarya erişimi engellemez, panik girişinde ve QMP ölçümünde bozulmayı bildirir.
- VARSAYIM: PVH PIC yolu korunur. UEFI tek işlemcili xAPIC kullanır; firmware x2APIC açık bırakırsa açık hata döner.
- VARSAYIM: PS/2 klavye IRQ varsayılandır; mevcut USB klavye varsa USB seçimi korunur ve PS/2 fare birlikte çalışır. Eski yoklamalı yol silinmedi.
- VARSAYIM: opak port asm’i için statik gerçekzamanlı bütçe port başına 1024 çevrimdir; fiziksel WCET ölçümü değildir.
- VARSAYIM: eski/bilinmeyen FADT’de PS/2 probu korunur; bilinen yok bayrağında USB klavye gerekir. Reset 8 bit SystemIO/SystemMemory GAS destekler; aksi durumda 8042 ve üçlü hata geri dönüşü kullanılır.
- VARSAYIM: `kapat`, QEMU TCG’de debug-exit; gerçek PC’de güç tuşu mesajı ve HLT kullanır. Tam S5/AML uygulanmadı. Tekerlek, SMP, MSI/MSI-X, USB HID kesmeleri ve BIOS/GRUB bu dalganın dışında.

C11-001: C ve self-host ham `*tam64` hedefe `0` atamasında dar store üretiyor. Derleyici değiştirilmedi; halka başlıklarında açık `0 olarak tam64` dönüşümü kullanıldı. Küçük tekrar üretimi ve iki derleyici sonucu `codex11_bulgular.md` ile `tools/codex/c11_001.py` içinde. MADT girdileri, IRQ0/1/12 GSI’leri, LAPIC kalibrasyonu, PS/2 kimliği, FADT ve gerçek QEMU ölçümleri bulgular dosyasında kayıtlıdır.

## Dil özellikleri

Yeni ISR’ler `gerçekzamanlı` işaretlidir; tahsis ve biçimlendirme yapmadan baytı halkaya bırakır. Halka indeksleri ve veri erişimleri volatile; tek üretici/tek tüketici x86 sıralamasını kullanır. Yeni `tekkez` veya `yetki` kullanımı yoktur. `drivers/` saf kalır; ham donanım erişimi görevde izin verilen x86 runtime/boot alanındadır.

## Dosya değişiklikleri (whitelist kontrol)

Dalga 11 §3 izin genişletmesine göre 42 dosya. `runtime/kdl_kesme.c` içinde x86 IDT bloğu dışı bayt bayt aynı; ARM değişikliği yalnız sessiz OOB kancası, heap değişikliği yalnız OOB gövdesidir. Derleyici, self-host, linker, boot/efi, diğer runtime C dosyaları ve dondurulmuş muafiyet listeleri değişmedi. Mevcut FADT API’si ve test beklentileri korundu. `git diff --check origin/main` temiz.

```text
Makefile
boot/start_x86_64.S
drivers/acpi/fadt.kem
drivers/acpi/yonlendirme.kem
otomasyon/chatgpt/codex11_bulgular.md
otomasyon/chatgpt/codex11_ilerleme.md
runtime/kdl_kesme.c
runtime/kem_heap.kem
runtime/mimari/arm64/kem_uart.kem
runtime/mimari/x86_64/kem_acpi_uefi.kem
runtime/mimari/x86_64/kem_apic_hw.kem
runtime/mimari/x86_64/kem_bellek_harita.kem
runtime/mimari/x86_64/kem_ekran_bga.kem
runtime/mimari/x86_64/kem_guc_hw.kem
runtime/mimari/x86_64/kem_mmu_hw.kem
runtime/mimari/x86_64/kem_ps2_hw.kem
test/bm_x86_guc_harness.sh
test/bm_x86_istisna_harness.sh
test/bm_x86_kabuk_harness.sh
test/bm_x86_uefi_harness.sh
test/check_kapisi.sh
test/ornekler/kem_os_x86_guc.kem
test/ornekler/kem_os_x86_istisna.kem
test/ornekler/kem_os_x86_kabuk.kem
tests/drivers/acpi/fadt_guc_test.kem
tests/drivers/acpi/yonlendirme_test.kem
tests/qemu/apic_kahin.py
tests/qemu/bm_istisna.py
tests/qemu/bm_uefi.py
tests/qemu/guc_kahin.py
tests/qemu/ps2_irq_kahin.py
tests/qemu/psf_kahin.py
tests/qemu/uefi_kabuk_kahin.py
tools/codex/c11_001.py
tools/codex/dalga11_apic_sabotaj.py
tools/codex/dalga11_guc_sabit.py
tools/codex/dalga11_guc_sabotaj.py
tools/codex/dalga11_irq_sabotaj.py
tools/codex/dalga11_karma_sabotaj.py
tools/codex/dalga11_ps2_sabit.py
tools/codex/dalga11_sabit.py
tools/codex/dalga11_sabotaj.py
```

## Süre ve kanıt kayıtları

Başlangıç: 2026-11-13T19:27:07.346773+03:00. Tam takım bitişi: 2026-11-14T00:01:55.057418+03:00; aradaki süre 4:34:47.710645. Sonuçlandırma kullanıcı devam mesajından sonra 2026-11-14 tarihinde yapıldı.

Yerel son log: `build/dalga11_test_tumu.log`; SHA256 `26cfe967be410f7ed7cf7fca921ac53323a221becbf17a7ba5166b0d6c923c50`. Dönüş kodu `build/dalga11_test_tumu.rc` dosyasında `0`. A/B, C, D ve E ara kapı/sabotaj kayıtları aynı build dizininde `dalga11_*` adıyla tutulur.

## Sonraki adım

Tek taslak PR ile inceleme. Fiziksel PC doğrulaması ayrıca gerekir; ölçümler QEMU q35/OVMF üzerindedir. C11-001 derleyici sahibine aktarılacak bulgudur. Auto-merge ve merge yapılmadı; yeni D numarası verilmedi.
