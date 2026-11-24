# Bölüm 3 — EVREN incelemesi

- commit: `c00ff32da6fc785329bad4eae6e18e679bdc1066`
- model: qwen3.8-flash-next · bitiş: stop · süre: 1200 sn · usage: {'prompt_tokens': 19910, 'total_tokens': 38551, 'completion_tokens': 18641, 'prompt_tokens_details': {'cached_tokens': 0, 'created_cache_tokens': 17600}, 'completion_tokens_details': {'reasoning_tokens': 17456}, 'evren': {'credits_held_cr': '0.0000', 'credits_charged_cr': '0.0000', 'credits_remaining_cr': '1050.0000', 'routed_model': 'qwen/qwen3.8-flash-next'}}



## Bulgular

- [KUSUR] `drivers/nvme/cekirdek.kem:60` — NVMe CQ1 oluşturma komutunda IV (Interrupt Vector) alanı yanlış: `yaz_le32(s.komut,44,65539)` değeri CDW11'e yazılıyor; 65539 = 0x10003 → IV=3, IEN=1. Oysa `bd_nvme_kesme` yalnız MSI-X tablo girdisi 1'i programlıyor (tablo+16). IV=1 olmalı → doğru değer `0x10001` (65537). Alıntı: `yaz_le32(s.komut,44,65539)`
- [KUSUR] `test/ornekler/kem_os_x86_blok_ortak.kem:209` — AHCI kapı zili yakalama yolunda `kem_mmio_yaz32(bar+8,bit)` QEMU ich9-ahci'de GHC (Global Host Control) adresine yazıyor (QEMU 8.2: `#define AHCI_GHC 0x08`). `bit=1<<port` port 0 için 1 → GHC.HR (bit 0) set edilerek HBA sıfırlanır. Kesme beklemesinden önce IS'yi (0x0C) W1C ile temizlemek gerekir. Alıntı: `kem_mmio_yaz32(bar+8,bit);`
- [EKSİK] `test/bm_x86_msi_harness.sh` — Görevde istenen iki sabotaj (yanlış MSI vektörü yaz, aynı vektöre iki işleyici izin ver) yalnız bulgular dosyasında elle doğrulanmış olarak tanımlanıyor; harness'da otomatik koşul yok. Harness yalnızca `UEFI_AHCI_MSI_YOK` (yetenek atla) yolunu ve `kesme sina` başarı yolunu koşuyor. Alıntı: yalnız `yoklama in 0 1` döngüsü ve `--yoklama` seçeneği mevcut; sabotaj senaryoları betiğe entegre değil.
- [ŞÜPHE] `test/ornekler/kem_os_x86_blok_ortak.kem:215` — Başarılı kesme sonrası tekrar `kem_mmio_yaz32(bar+8,bit)` yazılıyor; yukarıdaki GHC sorununu ikinci kez tetikliyor. Alıntı: `değilse { kem_mmio_yaz32(port+20,0); kem_mmio_yaz32(bar+8,bit); }`

## Karşılanan gereksinimler

- H1g `kesme_kaydet/kayır/bırak` arayüzü → `runtime/mimari/x86_64/kem_pci_hw.kem`
- IRQ1/IRQ12/LAPIC/xHCI'nin arayüze taşınması → `kem_ps2_hw.kem` (`kesme_geldi(33,1)`, `kesme_geldi(44,2)`), `kem_apic_hw.kem` (`kesme_geldi(48,3)`), `kem_os_x86_kabuk.kem` (`kesme_geldi(65,4)`)
- Dinamik vektör ayırma (66–79) + bırakma → `kem_pci_hw.kem` `kesme_ayir()` / `kesme_birak()`
- Kesme başına sayaç donanım raporunda → `kd_kesme_rapor` + `KESME <v> <kim> <sayı>` satırı
- H7d AHCI MSI: yetenek keşfi, adres/veri, mask, INTx kapalı → `bd_msi_ac`
- Yoklama yedeği + hangi yolun kullanıldığı → `"AHCI YOL MSI"`/`"AHCI YOL yoklama"` + zaman aşımı → `bd_msi_kapat` → polling
- H3h NVMe MSI-X CQ1 (en az) → `bd_nvme_kesme` + `"NVME YOL MSI-X CQ1"`
- Kâhin: konuk sayaçları, IOAPIC 16–23 artış=0, MSI kapalı → polling → dosya doğrulama → `tests/qemu/msi_kahin.py`
- Sabotaj 1 (yanlış vektör → timeout → kâhin KIRMIZI): `msi_kahin.py` içinde `if 'MSI zaman asimi' in ham → AssertionError` mantığı mevcut; bulgular'da sonuç doğrulanmış
- Sabotaj 2 (çift kayıt → çakışma KIRMIZI): `kd_kesme_sina()` + `msi_kahin.py` `c.run('kesme sina',red)`
- `boot/start_x86_64.S` yalnız yeni koçan, mevcut yol korunmuş ✓
- `runtime/kdl_kesme.c` yalnız IDT bloğunda 2 satır ekleme ✓
- `drivers/**` saf (yalnız bellek tamponu yazımı) ✓
