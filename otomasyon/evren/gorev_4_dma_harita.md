## İş 3 — DMA adres haritası (mimari başına) + x86 MMIO penceresi UC eşleme

### Sorun (ölçüldü)
- `vblk_dma = 0x43000000` (1124073472) ve `vnet_dma = 0x46800000`
  (1182793728) aarch64 QEMU `virt` RAM'ine (taban 0x40000000) göre seçilmiş.
  x86 boot'u yalnız 0..1 GB'ı 2 MB sayfalarla eşler (`start_x86_64.S` 2a-2c,
  PD girdileri 0x83 = P|RW|PS, PCD/PWT YOK → önbellekli). İki adres de 1 GB
  DIŞINDA → x86'da sayfa hatası; ayrıca QEMU x86 varsayılan RAM 128 MB.
- x86 MMIO (LAPIC 0xFEE00000, IOAPIC 0xFEC00000, microvm virtio-mmio
  0xFEB00000) 3-4 GB aralığında ve HİÇ eşlenmemiş.
- Ölçülen x86 yerleşim: kernel 0x100000'den, `__heap_end = 0x110F000`
  (~17 MB).

### Karar (verilmiş) — D2b deseni
DMA tabanları mimari başına `runtime/mimari/<arch>/kem_platform.kem`
dosyasına taşınır; Makefile CAT seçer.
- arm64: değerler BİREBİR aynı (davranış değişmez).
- x86_64: `vblk_dma = 0x02000000` (33554432), `vnet_dma = 0x02100000`
  (34603008) — 1 GB eşleme ve 128 MB RAM içinde, heap'in üstünde.
x86 MMIO: `PDPT[3]` → yeni `pd_mmio`, 512 × 2 MB, bayrak **0x9B**
(P|RW|PWT|PCD|PS = önbelleksiz/UC), fiziksel 0xC0000000..0xFFFFFFFF identity.

### İstenen değişiklikler (yalnız bunlar)

1. **YENİ `runtime/mimari/arm64/kem_platform.kem`**:
   `küresel değişken vblk_dma: tam64 = 1124073472;` ve
   `küresel değişken vnet_dma: tam64 = 1182793728;`. Türkçe başlık yorumu:
   aarch64 virt RAM 0x40000000 tabanlı; vnet_dma'nın AH-1'de DTB'den uzağa
   taşındığı notu (kem_virtio_net.kem 12-21. satırlardaki gerekçe) kısaca;
   x86 ikizi; D2b.
2. **YENİ `runtime/mimari/x86_64/kem_platform.kem`**: aynı iki küresel,
   değerler 33554432 ve 34603008. Yorum: neden bu adresler (1 GB eşleme + 128
   MB RAM içinde, `__heap_end` 0x110F000 üstünde; çakışma Makefile Kanıt 7
   ile ölçülür), blk ihtiyacı ~800 B, net ~19 KB (NO_TXBUF+2048).
3. **`runtime/kem_virtio_blk.kem`**: `küresel değişken vblk_dma: tam64 =
   1124073472;` satırını SİL; yerine tek satır yorum: taban artık
   `runtime/mimari/<arch>/kem_platform.kem`'de (CAT birimi). Hemen üstteki
   2 satırlık "DMA taban: SABİT identity-map RAM (0x43000000 ...)" yorumunu
   "arm64 değeri" olarak işaretleyecek biçimde güncelle. Başlıktaki
   `• DMA tamponları = SABİT identity-map RAM adresi (0x43000000, ...` satırına
   "(arm64; x86 değeri platform dosyasında)" ekle.
4. **`runtime/kem_virtio_net.kem`**: `küresel değişken vnet_dma: tam64 =
   1182793728;   // 0x46800000 (DTB bölgesinden UZAK)` satırını SİL; yerine
   tek satır yorum (taban platform dosyasında). AH-1 açıklama bloğuna DOKUNMA.
5. **`boot/start_x86_64.S`**:
   - `/* 2d. CR3 = PML4 */` satırından HEMEN ÖNCE yeni adım `2c2`:
     `pd_mmio`'yu 512 girdiyle doldur — düşük dword `0xC000009B + i*0x200000`
     (0xC0000000 tabanı | 0x9B), yüksek dword 0 (2a döngüsünün aynası, `%edi`
     /`%eax`/`%ecx` + `loop`); sonra `PDPT[3]` (`pdpt+24`) = `pd_mmio | 3`,
     `pdpt+28` = 0. Yorum: neden UC (MMIO, önbellek yan etkisi/yazma birleştirme
     olmamalı), hangi cihazlar (LAPIC/IOAPIC/microvm virtio-mmio).
   - `.bss` sayfa tablolarına `pd:     .skip 4096` satırının ALTINA
     `pd_mmio: .skip 4096` ekle (4 KB hizalı bölümde; BSS sıfırlaması kapsar).
   - Dosya başlığındaki adım 2 açıklamasına 3-4 GB UC MMIO penceresini ekle.
6. **CAT listeleri** (her yerde `runtime/kem_virtio_blk.kem` ÖNCESİNE
   platform dosyası):
   - Makefile `calistir_kem_os_arm`: `runtime/mimari/arm64/kem_platform.kem`
     (çapa: ` runtime/kem_zaman.kem runtime/mimari/arm64/kem_bariyer.kem runtime/kem_virtio_blk.kem`)
   - `test/baremetal_diff_harness.sh` OS_PARCA ve `test/check_kapisi.sh`
     KEMOS_PARCALAR: aynı yere arm64 platform dosyası.
   - Makefile `calistir_kem_os_x86` CAT: `runtime/kem_virtio_blk.kem` öncesine
     `runtime/mimari/x86_64/kem_platform.kem`, SONRASINA
     `runtime/kem_virtio_net.kem` (ağ DMA'sı da test edilecek).
   - `test/check_kapisi.sh` KEMOS_X86_PARCALAR: aynısı (platform + virtio_net),
     mesajda `5 parça` → `7 parça`.
7. **`test/ornekler/kem_os_x86.kem`** — `kem_panik("X86 PANIK YOLU OK");`
   satırından ÖNCE üç test (her biri DEĞER denetler, sabit metin basmaz):
   - **Blok DMA RAM:** `vblk_data_yaz(i, ((i % 251) + 3) olarak dtam8)` ile
     i=0..511 yaz, `vblk_data_oku(i)` ile geri oku ve karşılaştır; hepsi
     eşleşirse `X86 DMA RAM BLK OK` bas, yoksa `X86 DMA RAM BLK HATA` +
     ilk uyuşmayan indeks (`yazdir_tam`).
   - **Ağ DMA RAM:** `vnet_tx_yaz(i, ((i % 241) + 7) olarak dtam8)` ile i=0..63
     yaz; geri okumak için bu dosyaya bir ÇIPLAK yardımcı ekle:
     `çıplak işlev x86_net_tx_oku(i: tam32) -> dtam8 { ver vblk_o8(vnet_dma +
     NO_TXBUF + 12 + (i olarak tam64)); }` (küresele normal işlevden doğrudan
     erişim E010 verir; çıplak yardımcı şart). Eşleşirse `X86 DMA RAM NET OK`.
   - **MMIO UC pencere:** `kdl_mmio_oku32(4276092976)` (LAPIC sürüm yazmacı,
     0xFEE00030) oku; değer 0 değil ve -1 değilse `X86 MMIO UC OK
     lapic_surum=` satırı + değeri `yazdir_tam` ile ayrı satırda bas; yoksa
     `X86 MMIO UC HATA`.
   - Başlık yorumuna üç testi ekle.
8. **`Makefile` `calistir_kem_os_x86`** — Kanıt 6 bloğunun son satırından
   (`@llvm-objdump ... --disassemble-symbols=kem_uart_bayt ... | grep -E
   'outb|inb'`) SONRA:
   - **Kanıt 7 (yerleşim, yapısal):** `llvm-nm $(BUILD)/kem_os_x86.elf`
     içinden `__heap_end` değerini al; `0x02000000`'dan KÜÇÜK olmalı (DMA
     heap'le çakışmaz). Değeri yazdır. ⚠ Tarif kabuğu `/bin/sh` (dash):
     onaltılık için `$$((0x$$he))` kullan; `16#` YALNIZ bash'te çalışır
     (D-485 — bu depoda ölçülmüş kusur).
   - **Kanıt 8 (UC bayrağı, gerçek komut):** `llvm-objdump -d
     --no-show-raw-insn $(BUILD)/kem_os_x86.elf` içinde `0xc000009b`
     sabitini taşıyan komut satırı olmalı; satırı OLDUĞU GİBİ yazdır; yoksa
     FAIL.
   - **Kanıt 9 (davranış):** `kem_os_x86.out` içinde TAM satırlar
     `X86 DMA RAM BLK OK`, `X86 DMA RAM NET OK` ve `X86 MMIO UC OK
     lapic_surum=` olmalı (`grep -qxF`); yoksa FAIL.
   - Hedef başlık yorumuna Kanıt 7-9'u ekle (çapa: `# (yalnız .kem işlevi;
     tüm ikilide aramak C UART nesnesine de uyar).`).

### Çapalar (ESKI — AYNEN; her biri dosyada TEK kez geçer)
- `küresel değişken vblk_dma: tam64 = 1124073472;`
- `küresel değişken vnet_dma: tam64 = 1182793728;   // 0x46800000 (DTB bölgesinden UZAK)`
- `    /* 2d. CR3 = PML4 */`
- `pd:     .skip 4096`
- ` *   2. (32-bit) Identity sayfa tabloları (0..1GB, 2MB huge page) + PAE + LME +`
- Makefile arm CAT: ` runtime/kem_zaman.kem runtime/mimari/arm64/kem_bariyer.kem runtime/kem_virtio_blk.kem`
- Makefile x86 CAT: `	cat runtime/kem_heap.kem runtime/mimari/x86_64/kem_uart.kem runtime/mimari/x86_64/kem_bariyer.kem runtime/kem_virtio_blk.kem test/ornekler/kem_os_x86.kem > $(BUILD)/kem_os_x86_comb.kem`
- baremetal_diff: `runtime/kem_zaman.kem runtime/mimari/arm64/kem_bariyer.kem runtime/kem_virtio_blk.kem runtime/kem_minifs.kem \`
- check_kapisi: `                runtime/kem_zaman.kem runtime/mimari/arm64/kem_bariyer.kem runtime/kem_virtio_blk.kem`
- check_kapisi x86 listesi (2 satır, ikinci satır 20 boşluk girintili):
  `KEMOS_X86_PARCALAR="runtime/kem_heap.kem runtime/mimari/x86_64/kem_uart.kem runtime/mimari/x86_64/kem_bariyer.kem`
  `                    runtime/kem_virtio_blk.kem test/ornekler/kem_os_x86.kem"`
- check_kapisi mesajı: `kem_os_x86 (birleşik kaynak, 5 parça, --mimari x86_64)`
- kem_os_x86.kem: `    kem_panik("X86 PANIK YOLU OK");`
- Makefile Kanıt 6 son satırı: `	@llvm-objdump -d --no-show-raw-insn --disassemble-symbols=kem_uart_bayt $(BUILD)/kem_os_x86.elf | grep -E 'outb|inb'`
- Makefile yorum: `# (yalnız .kem işlevi; tüm ikilide aramak C UART nesnesine de uyar).`

### Yasak
`src/*.c`, `selfhost/*`, `runtime/kem_gorev.kem`, `runtime/kem_mmu.kem`,
`runtime/kem_zaman.kem`, `test/ornekler/kem_os.kem`, `boot/start_aarch64.S`.
