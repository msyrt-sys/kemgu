## İş 1 — Taşınabilir DMA bariyeri, D2b deseni (mimari başına dosya + CAT seçimi)

### Karar (orkestratör, verilmiş — tartışma)
Derleyiciye (src/*.c, selfhost/*) DOKUNULMAZ. Yerleşik yerine mimari başına
AYRI `.kem` dosyası; Makefile CAT listesi doğru mimarininkini seçer. AS001
yüzünden bir dosyada yalnız TEK mimarinin asm'si olabilir (ölçüldü).
Emsal: D-468 `sabitsüre` bariyeri arm64 → `csdb`, x86_64 → `lfence`; burada
DMA sıralaması için arm64 → `dsb sy`, x86_64 → `mfence`.

### İstenen değişiklikler (yalnız bunlar)

1. **YENİ `runtime/mimari/arm64/kem_bariyer.kem`** — tek işlev:
   `çıplak işlev kem_dma_bariyeri()` gövdesi `güvensiz { satıriçi_asm {
   mimari: arm64 şablon: r#"dsb sy"# bozulan("~{memory}") } }` (bugünkü
   `vblk_dsb` gövdesinin aynısı). Türkçe başlık yorumu: amaç (DMA tanımlayıcı
   yazımı ↔ MMIO notify sıralaması), neden ayrı dosya (AS001 + D2b), x86 ikizi.
2. **YENİ `runtime/mimari/x86_64/kem_bariyer.kem`** — aynı imza,
   `mimari: x86_64 şablon: r#"mfence"# bozulan("~{memory}")`. Yorumda:
   x86-TSO'da mağazalar sırayla görünür; `mfence` notify öncesi tam sıralama
   için güvenli tercih (performans önemsiz).
3. **`runtime/kem_virtio_blk.kem`** — `vblk_dsb` gövdesindeki `güvensiz { ... }`
   bloğunu `kem_dma_bariyeri();` çağrısıyla değiştir; işlevin adını ve 8 çağrı
   yerini DEĞİŞTİRME. Üstteki yorumu (`// dsb sy — ...`) mimari-bağımsız hâle
   getir. Dosya başlığındaki `• dsb sy bariyeri = satıriçi_asm arm64 ...`
   satırını yeni düzene göre güncelle (bariyer `runtime/mimari/<arch>/
   kem_bariyer.kem`'den; CAT birimi gereği).
4. **`Makefile` — `calistir_kem_os_arm` CAT satırı**: `runtime/kem_virtio_blk.kem`
   ÖNCESİNE `runtime/mimari/arm64/kem_bariyer.kem` ekle.
5. **`test/baremetal_diff_harness.sh` `OS_PARCA`** ve
   **`test/check_kapisi.sh` `KEMOS_PARCALAR`**: aynı yere (kem_virtio_blk
   öncesi) `runtime/mimari/arm64/kem_bariyer.kem` ekle. ZORUNLU: eklenmezse
   birleşik OS birimi T002 verir ve `baremetal_diff` bunu KIRMIZI değil
   "atlandı" sayar (sessiz kapsam kaybı).
6. **`Makefile` — `calistir_kem_os_x86`**:
   - CAT satırına, `test/ornekler/kem_os_x86.kem` ÖNCESİNE
     `runtime/mimari/x86_64/kem_bariyer.kem runtime/kem_virtio_blk.kem` ekle.
   - Yeni **Kanıt 4 (gerçek komut)**: `llvm-objdump -d --no-show-raw-insn
     $(BUILD)/kem_os_x86.elf` çıktısında `mfence` satırlarını bul ve
     OLDUKLARI GİBİ yazdır; yoksa `FAIL` + `exit 1`. Ayrıca `dsb` geçmemeli
     (geçerse `FAIL`).
   - Hedefin başındaki açıklama yorumuna Kanıt 4'ü ekle.
7. **`Makefile` — `calistir_kem_os_arm`**: mevcut
   `asm sideeffect "dsb sy"` IR denetiminin (FAIL mesajı "dsb sy bariyeri (P1
   arm64 asm) emit edilmedi") hemen ARDINA: `llvm-objdump -d
   --no-show-raw-insn $(BUILD)/kem_os.elf` çıktısında `dsb\s+sy` satırları
   bulunmalı (OLDUĞU GİBİ ilk 3'ünü yazdır); `mfence` geçmemeli.
8. **`test/ornekler/kem_os_x86.kem`**: `main`'in BAŞINDA `kem_dma_bariyeri();`
   çağır ve ardından `X86 DMA BARIYER OK` satırını bas (komut gerçekten
   ÇALIŞTI — #UD olsaydı çekirdek düşerdi). Normal işlevden çıplak çağrısı
   serbesttir. Başlık yorumuna bunu ekle. Mevcut heap testi aynen kalsın.
   Ayrıca `calistir_kem_os_x86` Kanıt 3'üne `X86 DMA BARIYER OK` satırının
   varlığını ekle (yoksa FAIL).

### Çapalar (ESKI metinleri — AYNEN kullan; her biri dosyada TEK kez geçer)
- `runtime/kem_virtio_blk.kem`:
  `        satıriçi_asm { mimari: arm64 şablon: r#"dsb sy"# bozulan("~{memory}") }`
  (vblk_dsb içindeki satır; çevresindeki `    güvensiz {` / `    }` satırlarıyla
  birlikte değiştir — bağlamdaki dosyadan birebir kopyala).
- Makefile kem_os_arm CAT: ` runtime/kem_zaman.kem runtime/kem_virtio_blk.kem`
  (bu dizgi Makefile'da TEK kez geçer).
- Makefile dsb IR denetimi: `		echo "FAIL: dsb sy bariyeri (P1 arm64 asm) emit edilmedi — DMA siralama yok"; exit 1; \`
  ve hemen sonraki `	fi` satırı.
- Makefile kem_os_x86 CAT: `	cat runtime/kem_heap.kem test/ornekler/kem_os_x86.kem > $(BUILD)/kem_os_x86_comb.kem`
- baremetal_diff: `runtime/kem_zaman.kem runtime/kem_virtio_blk.kem runtime/kem_minifs.kem \`
- check_kapisi: `                runtime/kem_zaman.kem runtime/kem_virtio_blk.kem`

### Yasak
`src/*.c`, `selfhost/*`, `runtime/kem_gorev.kem`, `runtime/kem_mmu.kem`,
`runtime/kem_zaman.kem`, `runtime/kem_virtio_net.kem` (onun 5 çağrı yeri
`vblk_dsb` üzerinden zaten düzelir).
