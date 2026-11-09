# Dalga 8 — bare-metal sürücüler; kabuk heap sınırında durduruldu

**Tamamlanmadı; merge için hazır değil.** Görevin “heap yetmezse DUR ve kaydet”
kuralı uygulandı. Kabukta gerçek DMA okuması sırasında yeni65560 baytlık bölge
isteği NULL döndü. Derleyici/runtime/linker sınırları korunarak çalışma kaydedildi.

## Kapsam

- A0: ortak GcIstek yürütücüsü ve PCI taraması taşındı. İşlevsel kapılar yeşil;
  `baremetal_diff`, mevcut sonuç<mantıksal,metin> C/self ABI farkını yakalıyor.
- A1: C6-001..004 dolanmaları kaldırıldı; dizi elemanı yerele al/geri yaz kuralı korundu.
- A2: NVMe, xHCI ve BOT tamamlanma bitinden sonra girdiyi yeniden okuyor.
  Yırtık görüntü testleri ve gerçek QEMU'dan yenilenen205 istekli model var.
- B: bare-metal AHCI,20/20 açılış; seri+rc33+bağımsız disk imajı kâhini.
- C: bare-metal USB FAT32,20/20 açılış;1300 baytlık dosya, fsck+Python kâhini.
- D **eksik**: GPT+isteğe bağlı FAT32+USB klavyeli kabuk taslağı. NVMe'de ilk üç
  komut doğru; heap engeli nedeniyle kabul edilmedi. Üç konfigürasyonda5'er
  başarılı açılış yok. USB ortak BOT/HID halka geçişi henüz gerçek kabukta doğrulanmadı.
- E: durma kuralı nedeniyle başlanmadı.

VARSAYIM: görev dosyaları ana çalışma ağacında/origin/main'de yoktu;
`otomasyon/codex-gorev8` dalındaki tam metinleri git show ile okundu.
Taban947c51e, D-746 doğrulandı. Yalnız ../kemgu-codex8 kullanıldı.

## Test

- `make test_tumu`: rc2; ardından seri `make -k test_tumu`: rc2, tüm bağımsız
  hedefler denendi. PIPESTATUS[0] kaydedildi.
- Kırmızı: kabuk (heap) ve baremetal_diff (C/self sonuç ABI'si).
- FIXPOINT ✓,98132 satır; checker310/310; codegen220/220; modül59/59.
- Köprü kart+126/126; qtest6/6 (C/self ayrı QEMU); NVMe/AHCI/USB ayrı20/20.
- Check340/353,13 muaf,0 RED; ASan132 PASS/0 FAIL/35 SKIP; matris12/12.
- Yeni4 test dosyası. Korpus67 atlama+QEMU üst bellek1 atlama;56 muafiyet.
  ASan4 bilinen+2 sızıntı istisnası ayrıca. Lean/lake yok.
- A0 sabotajı20/20 kırmızı; A1 gerçek+sahte kırmızı; A2 üç yeniden okuma
  kaldırıldığında üç test kırmızı; AHCI yanlış LBA20/20, USB yanlış içerik20/20
  bağımsız kâhinde kırmızı; sektör okuyucu veri bozması kırmızı.
- Python py_compile, shell bash -n ve git diff --check temiz.

## Dil özellikleri ve sınırlar

Yeni dil özelliği/asm yok. Drivers saf; ham erişimler yalnız izinli yeni
kem_gc_yurut.kem içinde. tekkez/yetki/ISR eklenmedi. Derleyici, mevcut C runtime,
boot, linker ve CI değişmedi. Yeni metin ilkelleri mevcut C sözleşmesini izler.
FAT32 önbelleği256 sektör, dosya/dizin64KiB; tüm disk belleğe alınmaz.

## Dosya değişiklikleri (whitelist kontrol)

```text
Makefile
drivers/fs/fat32_sektor.kem
drivers/nvme/cekirdek.kem
drivers/usb/depo.kem
drivers/usb/xhci_cekirdek.kem
otomasyon/chatgpt/codex8_bulgular.md
otomasyon/chatgpt/codex8_ilerleme.md
runtime/mimari/x86_64/kem_gc_yurut.kem
test/bm_x86_ahci_harness.sh
test/bm_x86_kabuk_harness.sh
test/bm_x86_nvme_harness.sh
test/bm_x86_usb_harness.sh
test/check_kapisi.sh
test/ornekler/kem_os_x86_ahci.kem
test/ornekler/kem_os_x86_kabuk.kem
test/ornekler/kem_os_x86_nvme.kem
test/ornekler/kem_os_x86_usb.kem
tests/drivers/fs/fat32_sektor_test.kem
tests/drivers/fs/sektor_fikstur.kem
tests/drivers/nvme/sahte_nvme.kem
tests/drivers/nvme/yirtik_test.kem
tests/drivers/usb/depo_yirtik_test.kem
tests/drivers/usb/sahte_xhci.kem
tests/drivers/usb/xhci_qemu_fikstur.kem
tests/drivers/usb/xhci_yirtik_test.kem
tests/qemu/bm_kabuk.py
tests/qemu/fat_yazici.py
tools/codex/dalga6_xhci_fikstur.py
tools/codex/dalga8_fikstur.py
```

## Sonraki adım

Heap/bölge ömrü tasarımı ve C8-001 ABI farkı çözülmeden kabuk tamamlanmış sayılmaz.
C8-002 negatif tam64 alanı ve C8-003 sonuç dalındaki bayt işareti için küçük
tekrar üretimleri `otomasyon/chatgpt/codex8_bulgular.md` içinde. Kalan kapılar
yeşile çevrilmeden merge edilmemeli. Auto-merge açılmadı, merge yapılmadı.

Başlangıç kaydı:2026-11-08T19:35:46Z. Son doğrulama:2026-11-08T20:23:54.719225+00:00.
