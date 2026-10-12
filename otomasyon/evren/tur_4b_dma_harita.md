# Tur 4b — dma_harita — `deepseek-v4.1-flash`

- Tarih: 2026-10-12 06:30:15 · süre 93.5 sn · finish `length` · reasoning_effort `low`
- usage: `{"prompt_tokens": 23991, "total_tokens": 40375, "completion_tokens": 16384, "prompt_tokens_details": {"cached_tokens": 14208, "created_cache_tokens": 9728}, "completion_tokens_details": {"reasoning_tokens": 11822}}`
- Değişiklik blokları: 9 — yeni:runtime/mimari/arm64/kem_platform.kem, yeni:runtime/mimari/x86_64/kem_platform.kem, duzenle:boot/start_x86_64.S, duzenle:boot/start_x86_64.S, duzenle:boot/start_x86_64.S, duzenle:test/ornekler/kem_os_x86.kem, duzenle:test/ornekler/kem_os_x86.kem, duzenle:otomasyon/evren/baglam/makefile_kem_os_x86_kesit.txt, duzenle:otomasyon/evren/baglam/makefile_kem_os_x86_kesit.txt
- İstem: [tur_4b_dma_harita_istem.md](tur_4b_dma_harita_istem.md)

## Yanıt

Önce iki yeni platform dosyası, sonra üç dosyada düzenlemeler.

```yeni:runtime/mimari/arm64/kem_platform.kem
// kem_platform.kem — aarch64 virt DMA adres haritası (D2b: mimari başına platform).
// ============================================================================
// DMA taban adresleri MİMARİYE ÖZGÜDÜR. Makefile CAT listesi mimariye göre bu
// dosyayı seçer: aarch64 → runtime/mimari/arm64/kem_platform.kem,
// x86_64 → runtime/mimari/x86_64/kem_platform.kem. kem_virtio_blk.kem ve
// kem_virtio_net.kem içindeki `vblk_dma`/`vnet_dma` tanımları BURAYA taşındı.
//
// aarch64 QEMU virt: RAM tabanı 0x40000000.
//   vblk_dma = 0x43000000 (1124073472) — blok DMA (VRING + veri).
//   vnet_dma = 0x46800000 (1182793728) — ağ DMA.
// ⚠ AH-1: vnet_dma ESKİDEN 0x44000000'daydı; orası firmware'in DEVICE-TREE'yi
// (DTB) koyduğu yerdi → ağ sürücüsü donanım-keşif verisini EZİYORDU. DTB'den
// uzağa (0x46800000) taşındı; ayrıntılı gerekçe kem_virtio_net.kem 12-21.
// Kalıcı çözüm (DTB /memreserve okuma) AH-2'nin işi; bu sabit tabanlar
// x86 ikizinin (runtime/mimari/x86_64/kem_platform.kem) davranışını DEĞİŞTİRMEZ.
küresel değişken vblk_dma: tam64 = 1124073472;   // 0x43000000
küresel değişken vnet_dma: tam64 = 1182793728;   // 0x46800000
```

```yeni:runtime/mimari/x86_64/kem_platform.kem
// kem_platform.kem — x86_64 DMA adres haritası (D2b: mimari başına platform).
// ============================================================================
// Makefile CAT listesi x86_64 için bu dosyayı seçer. kem_virtio_blk.kem ve
// kem_virtio_net.kem içindeki `vblk_dma`/`vnet_dma` tanımları BURAYA taşındı.
//
// NEDEN bu adresler: x86 boot (start_x86_64.S) yalnız 0..1 GB'ı 2 MB huge
// page'lerle eşler (PD girdileri 0x83 = P|RW|PS) ve QEMU x86 varsayılan RAM
// 128 MB'tır. Dolayısıyla DMA bölgesi __heap_end (~0x110F000) ÜSTÜNDE ve
// 1 GB eşlemenin ALTINDA kalmalıdır. aarch64 değerlerinden farklı seçildi
// çünkü aarch64 QEMU virt RAM tabanı 0x40000000; x86 kernel 0x100000'den yüklenir.
//   vblk_dma = 0x02000000 (33554432) — blok DMA tabanı; ihtiyaç ~800 B.
//   vnet_dma = 0x02100000 (34603008) — ağ DMA tabanı; ihtiyaç ~19 KB
//     (NO_TXBUF + 2048, ayrıca RX tamponları → 16rı KB birkaç offset).
// Çakışma ölçümü Makefile Kanıt 7 (__heap_end < 0x02000000) ile yapılır.
küresel değişken vblk_dma: tam64 = 33554432;    // 0x02000000
küresel değişken vnet_dma: tam64 = 34603008;    // 0x02100000
```

```duzenle:boot/start_x86_64.S
<<<<<<< ESKI
 *   2. (32-bit) Identity sayfa tabloları (0..1GB, 2MB huge page) + PAE + LME +
 *      paging → long mode; 64-bit kod segmentine far jump
=======
 *   2. (32-bit) Identity sayfa tabloları (0..1GB, 2MB huge page) + PAE + LME +
 *      Ayrıca 3..4 GB UC MMIO penceresi (pd_mmio: 0xC0000000..0xFFFFFFFF,
 *      PWT|PCD ile önbelleksiz) + paging → long mode; 64-bit kod segmentine far jump
>>>>>>> YENI
```

```duzenle:boot/start_x86_64.S
<<<<<<< ESKI
    /* 2d. CR3 = PML4 */
=======
    /* 2c2. 3..4 GB UC MMIO penceresi: pd_mmio 512 × 2MB, taban 0xC0000000,
     * bayrak 0x9B = P|RW|PWT|PCD|PS → önbelleksiz (UC). NEDEN UC: bu aralık
     * MMIO'dur — LAPIC (0xFEE00000), IOAPIC (0xFEC00000), microvm virtio-mmio
     * (0xFEB00000). Yan etkili okuma/yazmalar önbelleğe alınır veya yazma-
     * birleştirilirse cihaz durumu bayatlar / kaybolur. Bu pencere 0..1GB
     * PD'sinden AYRI tutulur (farklı bayrak) → PDPT[3]'e bağlanır. */
    movl    $pd_mmio, %edi
    movl    $0xC000009B, %eax      /* present|write|PWT|PCD|PS, taban 0xC0000000 */
    movl    $512, %ecx
1:
    movl    %eax, (%edi)           /* düşük 32 bit */
    movl    $0, 4(%edi)            /* yüksek 32 bit = 0 */
    addl    $0x200000, %eax        /* sonraki 2MB */
    addl    $8, %edi
    loop    1b

    /* PDPT[3] = pd_mmio | present|write (pdpt+24; yüksek dword pdpt+28). */
    movl    $pd_mmio, %eax
    orl     $0x3, %eax
    movl    %eax, pdpt+24
    movl    $0, pdpt+28

    /* 2d. CR3 = PML4 */
>>>>>>> YENI
```

```duzenle:boot/start_x86_64.S
<<<<<<< ESKI
pd:     .skip 4096
=======
pd:     .skip 4096
pd_mmio: .skip 4096
>>>>>>> YENI
```

```duzenle:test/ornekler/kem_os_x86.kem
<<<<<<< ESKI
// main sonunda kem_panik("X86 PANIK YOLU OK") çağrılır: panik geri dönmez,
// mesaj .kem COM1 yolundan basılır, çekirdek orada durur; QEMU zaman aşımı keser.

işlev main() -> tam32 {
=======
// main sonunda kem_panik("X86 PANIK YOLU OK") çağrılır: panik geri dönmez,
// mesaj .kem COM1 yolundan basılır, çekirdek orada durur; QEMU zaman aşımı keser.
//
// İş 3: kem_panik'ten ÖNCE üç DMA/MMIO testi koşar (her biri DEĞER denetler,
// sabit metin basmaz — D-264 dersi):
//   (a) Blok DMA RAM — vblk_data_yaz/oku 0..511 yaz-oku karşılaştırma →
//       "X86 DMA RAM BLK OK" (aksi hâlde "X86 DMA RAM BLK HATA" + ilk indeks).
//   (b) Ağ DMA RAM — vnet_tx_yaz + çıplak x86_net_tx_oku (aşağıda) 0..63
//       karşılaştırma → "X86 DMA RAM NET OK".
//   (c) MMIO UC pencere — kdl_mmio_oku32(0xFEE00030) LAPIC sürüm yazmacı
//       (0 DEĞİL ve -1 DEĞİLSE) → "X86 MMIO UC OK lapic_surum=" + değer.
//
// Bu üç test, DMA tabanlarının (kem_platform.kem x86_64 ikizi) 0..1GB eşleme
// ve UC MMIO penceresinin (start_x86_64.S 2c2) gerçekten kurulduğunu
// DAVRANIŞSAL olarak kanıtlar.

// Çıplak yardımcı: küresel vnet_dma'ya NORMAL işlevden doğrudan erişim E010
// verir; çıplak işlev küresele serbestçe erişir (kem_heap.kem idiomu).
çıplak işlev x86_net_tx_oku(i: tam32) -> dtam8 {
    ver vblk_o8(vnet_dma + NO_TXBUF + 12 + (i olarak tam64));
}

işlev main() -> tam32 {
>>>>>>> YENI
```

```duzenle:test/ornekler/kem_os_x86.kem
<<<<<<< ESKI
    kem_panik("X86 PANIK YOLU OK");
=======
    // İş 3 (a): Blok DMA RAM yaz-oku (0..511; 251 ile döngüsel desen).
    değişken dma_i: tam32 = 0;
    iken dma_i < 512 {
        vblk_data_yaz(dma_i, (((dma_i % 251) + 3) olarak dtam8));
        dma_i = dma_i + 1;
    }
    değişken blk_hata: tam32 = 512;   // 512 = "uyuşmazlık yok" (geçerli indeks 0..511)
    dma_i = 0;
    iken dma_i < 512 {
        değişken beklenen_blk: dtam8 = (((dma_i % 251) + 3) olarak dtam8);
        eğer vblk_data_oku(dma_i) != beklenen_blk {
            eğer blk_hata == 512 { blk_hata = dma_i; }
        }
        dma_i = dma_i + 1;
    }
    eğer blk_hata == 512 {
        yazdir_metin("X86 DMA RAM BLK OK");
    }
    değilse {
        yazdir_metin("X86 DMA RAM BLK HATA");
        yazdir_tam(blk_hata);
        yazdir_satir();
    }

    // İş 3 (b): Ağ TX tamponu yaz-oku (0..63; 241 ile döngüsel desen).
    değişken net_i: tam32 = 0;
    iken net_i < 64 {
        vnet_tx_yaz(net_i, (((net_i % 241) + 7) olarak dtam8));
        net_i = net_i + 1;
    }
    değişken net_hata: tam32 = 64;
    net_i = 0;
    iken net_i < 64 {
        değişken beklenen_net: dtam8 = (((net_i % 241) + 7) olarak dtam8);
        eğer x86_net_tx_oku(net_i) != beklenen_net {
            eğer net_hata == 64 { net_hata = net_i; }
        }
        net_i = net_i + 1;
    }
    eğer net_hata == 64 {
        yazdir_metin("X86 DMA RAM NET OK");
    }
    değilse {
        yazdir_metin("X86 DMA RAM NET HATA");
        yazdir_tam(net_hata);
        yazdir_satir();
    }

    // İş 3 (c): MMIO UC pencere — LAPIC sürüm yazmacı (0xFEE00030 = 4276092976).
    değişken lapic_surum: tam32 = kdl_mmio_oku32(4276092976);
    eğer lapic_surum == 0 {
        yazdir_metin("X86 MMIO UC HATA");
    }
    değilse {
        eğer lapic_surum == (0 - 1) {
            yazdir_metin("X86 MMIO UC HATA");
        }
        değilse {
            yazdir_metin("X86 MMIO UC OK lapic_surum=");
            yazdir_tam(lapic_surum);
            yazdir_satir();
        }
    }

    kem_panik("X86 PANIK YOLU OK");
>>>>>>> YENI
```

```duzenle:otomasyon/evren/baglam/makefile_kem_os_x86_kesit.txt
<<<<<<< ESKI
# Kanıt 6: objdump --disassemble-symbols=kem_uart_bayt içinde outb VE inb
# (yalnız .kem işlevi; tüm ikilide aramak C UART nesnesine de uyar).
calistir_kem_os_x86: $(BUILD)/kemgu$(EXE) \
=======
# Kanıt 6: objdump --disassemble-symbols=kem_uart_bayt içinde outb VE inb
# (yalnız .kem işlevi; tüm ikilide aramak C UART nesnesine de uyar).
# Kanıt 7: llvm-nm'de __heap_end < 0x02000000 (DMA tabanı heap'le çakışmaz).
# Kanıt 8: objdump'da 0xc000009b sabiti (UC MMIO PD sayfa bayrağı gerçekten kurulu).
# Kanıt 9: QEMU seri çıktıda "X86 DMA RAM BLK OK", "X86 DMA RAM NET OK" tam
#          satırları ve "X86 MMIO UC OK lapic_surum=" (LAPIC sürüm — UC pencere
#          okunabiliyor).
calistir_kem_os_x86: $(BUILD)/kemgu$(EXE) \
>>>>>>> YENI
```

```duzenle:otomasyon/evren/baglam/makefile_kem_os_x86_kesit.txt
<<<<<<< ESKI
	@llvm-objdump -d --no-show-raw-insn --disassemble-symbols=kem_uart_bayt $(BUILD)/kem_os_x86.elf | grep -E 'outb|inb'
=======
	@llvm-objdump -d --no-show-raw-insn --disassemble-symbols=kem_uart_bayt $(BUILD)/kem_os_x86.elf | grep -E 'outb|inb'
	@echo "  Kanit 7: yerlesim (__heap_end < DMA tabani 0x02000000)..."
	@he=$$(llvm-nm $(BUILD)/kem_os_x86.elf | awk '/__heap_end/ {print $$1; exit}'); \
	if [ -z "$$he" ]; then echo "FAIL: __heap_end sembolu yok"; exit 1; fi; \
	hedec=$$((0x$$he)); \
	echo "    __heap_end = 0x$$he ($$hedec)"; \
	if [ "$$hedec" -ge 33554432 ]; then \
		echo "FAIL: __heap_end ($$hedec) >= 0x02000000 (DMA tabani)"; exit 1; fi
	@echo "  Kanit 8: UC MMIO bayragi (objdump'da 0xc000009b)..."
	@if ! llvm-objdump -d --no-show-raw-insn $(BUILD)/kem_os_x86.elf | grep -q '0xc000009b'; then \
		echo "FAIL: 0xc000009b objdump'da yok (UC MMIO sayfa bayragi)"; exit 1; fi
	@llvm-objdump -d --no-show-raw-insn $(BUILD)/kem_os_x86.elf | grep '0xc000009b'
	@echo "  Kanit 9: QEMU davranis (DMA RAM BLK/NET + UC MMIO)..."
	@if ! grep -qxF "X86 DMA RAM BLK OK" $(BUILD)/kem_os_x86.out; then \
		echo "FAIL: 'X86 DMA RAM BLK OK' satiri yok"; exit 1; fi
	@if ! grep -qxF "X86 DMA RAM NET OK" $(BUILD)/kem_os_x86.out; then \
		echo "FAIL: 'X86 DMA RAM NET OK' satiri yok"; exit 1; fi
	@if ! grep -qxF "X86 MMIO UC OK lapic_surum=" $(BUILD)/kem_os_x86.out; then \
		echo "FAIL: 'X86 MMIO UC OK lapic_surum=' satiri yok"; exit 1; fi
>>>>>>> YENI
```

## Yapılanlar

1. **Yeni `runtime/mimari/arm64/kem_platform.kem`** — aarch64 değerleri birebir (1124073472 / 1182793728); başlıkta RAM tabanı 0x40000000 ve AH-1 DTB-taşıma gerekçesi.
2. **Yeni `runtime/mimari/x86_64/kem_platform.kem`** — 33554432 / 34603008; neden bu adresler (1 GB eşleme + 128 MB RAM, `__heap_end` üstü), blk ~800 B / net ~19 KB notu.
3. **`boot/start_x86_64.S`** — başlık 2. adımına 3–4 GB UC MMIO penceresi eklendi; `2c2` adımı `pd_mmio`'yu 512 × 0x9B girdisiyle doldurup `PDPT[3]`e bağlar; `.bss` sayfa tablolarına `pd_mmio: .skip 4096`.
4. **`test/ornekler/k

## Düşünme (reasoning)

Let me carefully work through the requested changes. Budget is now limited so I need to be efficient and produce only the five points.

Let me focus:

1. NEW file: `runtime/mimari/arm64/kem_platform.kem`
2. NEW file: `runtime/mimari/x86_64/kem_platform.kem`
5. EDIT `boot/start_x86_64.S` — several edits
7. EDIT `test/ornekler/kem_os_x86.kem`
8. EDIT `Makefile` — add proofs 7-9 and header comment

Let me write each one carefully.

### 1. arm64 platform file

```kemgu
// kem_platform.kem — aarch64 virt DMA adres haritası (D2b: mimari başına platform).
// ============================================================================
// Bu dosya YALNIZ aarch64 (QEMU virt) için DMA taban adreslerini tanımlar.
// Makefile CAT listesi mimariye göre bu dosyayı seçer: aarch64 →
// runtime/mimari/arm64/kem_platform.kem, x86_64 → runtime/mimari/x86_64/kem_platform.kem.
// kem_virtio_blk.kem/kem_virtio_net.kem içindeki `vblk_dma`/`vnet_dma` küresel
// tanımları KALDIRILDI (Orkestratör yapar); değerleri artık BURADAN gelir.
//
// aarch64 QEMU virt: RAM tabanı 0x40000000.
//   vblk_dma = 0x43000000 (1124073472) — blok DMA (VRING + veri).
//   vnet_dma = 0x46800000 (1182793728) — ağ DMA.
// ⚠ vnet_dma AH-1'de 0x44000000'dan 0x46800000'a TAŞINDI: eski adres firmware'in
// DEVICE-TREE'yi (DTB) koyduğu yerdi; ağ sürücüsü donanım-keşif verisini EZİYORDU
// (ayrıntılı gerekçe kem_virtio_net.kem 12-21. satırlarda korunur).
küresel değişken vblk_dma: tam64 = 1124073472;   // 0x43000000
küresel değişken vnet_dma: tam64 = 1182793728;   // 0x46800000
```

Wait — the task says values must be BİREBİR same. Good.

### 2. x86_64 platform file

```kemgu
// kem_platform.kem — x86_64 DMA adres haritası (D2b: mimari başına platform).
// ============================================================================
// x86_64 için DMA taban adresleri. Makefile CAT listesi mimariye göre seçer.
// kem_virtio_blk.kem/kem_virtio_net.kem'deki `vblk_dma`/`vnet_dma` küreselleri
// artık BURADAN gelir.
//
// Neden bu adresler: start_x86_64.S yalnız 0..1 GB'ı 2 MB huge page'lerle
// eşler (PD 0x83 = P|RW|PS). Ayrıca QEMU x86 varsayılan RAM 128 MB. DMA
// bölgesi __heap_end (~0x110F000) ÜSTÜNDE ve 1 GB eşlemenin ALTINDA olmalı:
//   vblk_dma = 0x02000000 (33554432) — blok DMA; ihtiyaç ~800 B.
//   vnet_dma = 0x02100000 (34603008) — ağ DMA; ihtiyaç ~19 KB (NO_TXBUF+2048).
// Çakışma (__heap_end < vblk_dma) Makefile Kanıt 7 ile ölçülür.
küresel değişken vblk_dma: tam64 = 33554432;    // 0x02000000
küresel değişken vnet_dma: tam64 = 34603008;    // 0x02100000
```

### 5. start_x86_64.S edits

Edit 5a: header comment step 2. Anchor: ` *   2. (32-bit) Identity sayfa tabloları (0..1GB, 2MB huge page) + PAE + LME +`

Original text (line in file):
```
 *   2. (32-bit) Identity sayfa tabloları (0..1GB, 2MB huge page) + PAE + LME +
 *      paging → long mode; 64-bit kod segmentine far jump
```

I'll add about 3-4 GB UC MMIO window.

New:
```
 *   2. (32-bit) Identity sayfa tabloları (0..1GB, 2MB huge page) + PAE + LME +
 *      Ayrıca 3..4 GB (0xC0000000..0xFFFFFFFF) ayrı bir PD (pd_mmio) ile
 *      önbelleksiz/UC (PWT|PCD) identity eşlenir → LAPIC/IOAPIC/microvm
 *      virtio-mmio MMIO erişimi. + paging → long mode; 64-bit kod segmentine far jump
```

Hmm, better to keep clean. Let me do:
```
 *   2. (32-bit) Identity sayfa tabloları (0..1GB, 2MB huge page) + PAE + LME +
 *      3..4 GB UC MMIO penceresi (pd_mmio: 0xC0000000..0xFFFFFFFF, PWT|PCD) +
 *      paging → long mode; 64-bit kod segmentine far jump
```

Edit 5b: insert step 2c2 before `/* 2d. CR3 = PML4 */`.

Actually the PD fill loop uses %edi/%eax/%ecx. After 2c, are those registers still free for reuse? 2d uses %eax. Let me insert 2c2 before 2d. The code:

```
    /* 2b. PDPT[0] = PD | present|write */
    movl    $pd, %eax
    orl     $0x3, %eax
    movl    %eax, pdpt
    movl    $0, pdpt+4

    /* 2c. PML4[0] = PDPT | present|write */
    movl    $pdpt, %eax
    orl     $0x3, %eax
    movl    %eax, pml4
    movl    $0, pml4+4

    /* 2d. CR3 = PML4 */
```

I'll insert between 2c and 2d:

```
    /* 2c2. 3..4 GB UC MMIO penceresi: pd_mmio 512 × 2MB, 0xC0000000 tabanlı,
     * bayrak 0x9B = P|RW|PWT|PCD|PS → önbelleksiz (UC). Neden UC: MMIO yan
     * etkileri olan adreslerdir (LAPIC 0xFEE00000, IOAPIC 0xFEC00000, microvm
     * virtio-mmio 0xFEB00000); önbellekli/önbellek-yaz-birleştirmeli eşleme
     * okuma/yazmaları eler veya yeniden sıralar. Bu pencere 0..1GB PD'sinden
     * AYRI (farklı bayrak); PDPT[3]'e bağlanır. */
    movl    $pd_mmio, %edi
    movl    $0xC000009B, %eax      /* present|write|PWT|PCD|PS, taban 0xC0000000 */
    movl    $512, %ecx
1:
    movl    %eax, (%edi)           /* düşük 32 bit */
    movl    $0, 4(%edi)            /* yüksek 32 bit = 0 */
    addl    $0x200000, %eax        /* sonraki 2MB */
    addl    $8, %edi
    loop    1b

    /* PDPT[3] = pd_mmio | present|write (pdpt+24; yüksek dword pdpt+28) */
    movl    $pd_mmio, %eax
    orl     $0x3, %eax
    movl    %eax, pdpt+24
    movl    $0, pdpt+28

    /* 2d. CR3 = PML4 */
```

Wait, there's a problem — the label `1:` is already used in 2a. Can I reuse `1:` label? In GNU as, local labels (numeric like `1:`) can be reused. `loop 1b` refers to the nearest previous `1:` backwards. So reusing `1:` is fine since it's a local numeric label and `1b` goes backward. Good.

Actually wait, there's a subtle issue: after 2a's `loop 1b`, execution falls through to 2b. Then in 2c2, I add another `1:` label. That's the same as 2a's pattern. Fine.

Edit 5c: add `pd_mmio: .skip 4096` under `pd:     .skip 4096`.

Anchor: `pd:     .skip 4096`. Original:
```
pml4:   .skip 4096
pdpt:   .skip 4096
pd:     .skip 4096
```

New:
```
pml4:   .skip 4096
pdpt:   .skip 4096
pd:     .skip 4096
pd_mmio: .skip 4096
```

### 7. kem_os_x86.kem

Add three tests before `kem_panik(...)`.

The features used: `vblk_data_yaz`, `vblk_data_oku`, `vnet_tx_yaz`, `kdl_mmio_oku32`. I'll trust these exist in the relevant files. Let me check — `vnet_tx_yaz` is defined in kem_virtio_net.kem shown. `vblk_data_yaz`/`vblk_data_oku` presumably in kem_virtio_blk.kem. `kdl_mmio_oku32` from kem_heap.kem.

Additions:

```kem
    // İş 3: x86 DMA adres haritası + UC MMIO penceresi testleri.

    // (a) Blok DMA RAM: 0..511 yaz, geri oku, karşılaştır (değer denetimi).
    değişken bi: tam32 = 0;
    değişken blk_hata: tam32 = 0 - 1;
    iken bi < 512 {
        vblk_data_yaz(bi, (((bi % 251) + 3) olarak dtam8));
        bi = bi + 1;
    }
    bi = 0;
    iken bi < 512 {
        değişken beklenen_blk: dtam8 = (((bi % 251) + 3) olarak dtam8);
        eğer vblk_data_oku(bi) != beklenen_blk {
            eğer blk_hata == (0 - 1) { blk_hata = bi; }
        }
        bi = bi + 1;
    }
    eğer blk_hata == (0 - 1) {
        yazdir_metin("X86 DMA RAM BLK OK");
    }
    değilse {
        yazdir_metin("X86 DMA RAM BLK HATA");
        yazdir_tam(blk_hata);
        yazdir_satir();
    }
```

Hmm but the requirement says "yoksa `X86 DMA RAM BLK HATA` + ilk uyuşmayan indeks (`yazdir_tam`)". OK.

Now the second test:

```kem
    // (b) Ağ DMA RAM: 0..63 yaz, x86_net_tx_oku ile geri oku (çıplak yardımcı).
    değişken ni: tam32 = 0;
    değişken net_hata: tam32 = 0 - 1;
    iken ni < 64 {
        vnet_tx_yaz(ni, (((ni % 241) + 7) olarak dtam8));
        ni = ni + 1;
    }
    ni = 0;
    iken ni < 64 {
        değişken beklenen_net: dtam8 = (((ni % 241) + 7) olarak dtam8);
        eğer x86_net_tx_oku(ni) != beklenen_net {
            eğer net_hata == (0 - 1) { net_hata = ni; }
        }
        ni = ni + 1;
    }
    eğer net_hata == (0 - 1) {
        yazdir_metin("X86 DMA RAM NET OK");
    }
    değilse {
        yazdir_metin("X86 DMA RAM NET HATA");
        yazdir_tam(net_hata);
        yazdir_satir();
    }
```

Third test:

```kem
    // (c) MMIO UC pencere: LAPIC sürüm yazmacı (0xFEE00030) oku, 0/-1 değilse OK.
    değişken lapic_surum: tam32 = kdl_mmio_oku32(4276092976);
    eğer lapic_surum != 0 {
        eğer lapic_surum != (0 - 1) {
            yazdir_metin("X86 MMIO UC OK lapic_surum=");
            yazdir_tam(lapic_surum);
            yazdir_satir();
        }
        değilse {
            yazdir_metin("X86 MMIO UC HATA");
        }
    }
    değilse {
        yazdir_metin("X86 MMIO UC HATA");
    }
```

Hmm, 0 - 1 in tam32 — is that valid? Looking at kem_virtio_net.kem they use `ver 0 - 1;` so it's valid syntax. And `(0 - 1)` might work with parentheses. Let me simplify to use `!= 0` and `!= (0 - 1)` for lapic which checks not 0 and not -1.

Actually, for cleanliness let me check: the description says "değer 0 değil ve -1 değilse". So:
- if value != 0 and value != -1 → OK
- else → HATA

Let me use a flag:

```kem
    değişken lapic_surum: tam32 = kdl_mmio_oku32(4276092976);
    eğer lapic_surum != 0 {
        eğer lapic_surum != (0 - 1) {
            yazdir_metin("X86 MMIO UC OK lapic_surum=");
            yazdir_tam(lapic_surum);
            yazdir_satir();
        }
        değilse {
            yazdir_metin("X86 MMIO UC HATA");
        }
    }
    değilse {
        yazdir_metin("X86 MMIO UC HATA");
    }
```

Hmm, that's a bit awkward with two HATA branches. Alternatively use a boolean:

Actually the pattern in the file uses `eğer ... değilse`. Let me keep it simple:

```kem
    değişken lapic_surum: tam32 = kdl_mmio_oku32(4276092976);
    eğer lapic_surum == 0 {
        yazdir_metin("X86 MMIO UC HATA");
    }
    değilse {
        eğer lapic_surum == (0 - 1) {
            yazdir_metin("X86 MMIO UC HATA");
        }
        değilse {
            yazdir_metin("X86 MMIO UC OK lapic_surum=");
            yazdir_tam(lapic_surum);
            yazdir_satir();
        }
    }
```

That works. Let me be consistent.

Actually I'll simplify: define a single boolean `lapic_ok`. But the language might not have boolean... it seems to use integer conditions. Let me use the nested approach.

Now file header comment needs to be updated. Anchor for header? The task says "Başlık yorumuna üç testi ekle." I'll add to the existing comment block. Anchor needs to be something in the header. Let me use:

```
// main sonunda kem_panik("X86 PANIK YOLU OK") çağrılır: panik geri dönmez,
// mesaj .kem COM1 yolundan basılır, çekirdek orada durur; QEMU zaman aşımı keser.
```

I'll add after this.

Now let me also add the `x86_net_tx_oku` helper function. Where? Probably before `işlev main()`. Or after main. Let me put it right before main to be clear.

Also need to update the header comment. Let me draft the addition:

```
// İş 3: kem_panik'ten ÖNCE üç DMA/MMIO testi eklenir:
// (a) Blok DMA RAM — vblk_data_yaz/oku 0..511 yaz-oku karşılaştırma → "X86 DMA RAM BLK OK"
// (b) Ağ DMA RAM — vnet_tx_yaz + çıplak x86_net_tx_oku 0..63 karşılaştırma → "X86 DMA RAM NET OK"
// (c) MMIO UC pencere — kdl_mmio_oku32(0xFEE00030) LAPIC sürüm yazmacı → "X86 MMIO UC OK lapic_surum="
```

OK.

### 8. Makefile edits

Two edits: header comment (after anchor `# (yalnız .kem işlevi; tüm ikilide aramak C UART nesnesine de uyar).`) and the block after the last `llvm-objdump ... grep -E 'outb|inb'` line.

Header edit:
Original:
```
# Kanıt 6: objdump --disassemble-symbols=kem_uart_bayt içinde outb VE inb
# (yalnız .kem işlevi; tüm ikilide aramak C UART nesnesine de uyar).
```

Add after (before the actual rule):
```
# Kanıt 7: llvm-nm'de __heap_end < 0x02000000 (DMA tabanı heap'le çakışmaz).
# Kanıt 8: objdump'da 0xc000009b sabiti (UC MMIO sayfa bayrağı gerçekten kurulu).
# Kanıt 9: QEMU seri çıktıda "X86 DMA RAM BLK OK", "X86 DMA RAM NET OK" tam satırları
#          ve "X86 MMIO UC OK lapic_surum=" (LAPIC sürüm — UC pencere okunabiliyor).
```

Then the actual rule additions after the last grep in Kanıt 6.

Original block:
```
	@if ! llvm-objdump -d --no-show-raw-insn --disassemble-symbols=kem_uart_bayt $(BUILD)/kem_os_x86.elf | grep -q 'inb'; then \
		echo "FAIL: kem_uart_bayt icinde inb yok"; exit 1; fi
	@llvm-objdump -d --no-show-raw-insn --disassemble-symbols=kem_uart_bayt $(BUILD)/kem_os_x86.elf | grep -E 'outb|inb'


calistir_kem_os_x86: ...
```

The anchor is:
```
	@llvm-objdump -d --no-show-raw-insn --disassemble-symbols=kem_uart_bayt $(BUILD)/kem_os_x86.elf | grep -E 'outb|inb'
```

Hmm wait, this line appears as the last line of the rule. Let me look at the file text carefully:

```
	@if ! llvm-objdump -d --no-show-raw-insn --disassemble-symbols=kem_uart_bayt $(BUILD)/kem_os_x86.elf | grep -q 'inb'; then \
		echo "FAIL: kem_uart_bayt icinde inb yok"; exit 1; fi
	@llvm-objdump -d --no-show-raw-insn --disassemble-symbols=kem_uart_bayt $(BUILD)/kem_os_x86.elf | grep -E 'outb|inb'


```

Two blank lines after. Anchor is the last line with the grep. I'll insert after.

Kanıt 7 shell:

```make
	@echo "  Kanit 7: yerlesim (__heap_end < DMA tabani 0x02000000)..."
	@he=$$(llvm-nm $(BUILD)/kem_os_x86.elf | awk '/__heap_end/ {print $$1; exit}'); \
	if [ -z "$$he" ]; then echo "FAIL: __heap_end sembolu yok"; exit 1; fi; \
	hedec=$$((0x$$he)); \
	echo "    __heap_end = 0x$$he ($$hedec)"; \
	if [ "$$hedec" -ge 33554432 ]; then \
		echo "FAIL: __heap_end ($$hedec) >= 0x02000000 (DMA tabani)"; exit 1; fi
```

Wait — `llvm-nm` output format. For `__heap_end` (a linker symbol), llvm-nm might output with spaces or it might not show linker-defined symbols. Hmm. Actually `__heap_end` is a linker symbol, defined in linker script. llvm-nm on ELF object/executable should show it if it's a symbol in the symbol table. Let me assume yes.

The awk prints the first column which is the address (hex without 0x prefix). `$$1` in makefile is `$1` in shell.

`$$((0x$$he))` — is this valid in dash? Yes, `$((0x...))` works in POSIX sh. The task explicitly notes to use `$$((0x$$he))` for hex, and warns that `16#` only works in bash.

Actually wait, the /bin/sh dash. `$(())` arithmetic supports base prefixes 0x, 0... yes POSIX mandates it. Good.

Kanıt 8:

```make
	@echo "  Kanit 8: UC MMIO bayragi (objdump'da 0xc000009b)..."
	@if ! llvm-objdump -d --no-show-raw-insn $(BUILD)/kem_os_x86.elf | grep -q '0xc000009b'; then \
		echo "FAIL: 0xc000009b objdump'da yok (UC MMIO sayfa bayragi)"; exit 1; fi
	@llvm-objdump -d --no-show-raw-insn $(BUILD)/kem_os_x86.elf | grep '0xc000009b'
```

Wait — the value `0xC000009B` will be moved by `movl $0xC000009B, %eax`. Then loops modify it. objdump might show `0xc000009b` in the comments. Actually the immediate appears in the first iteration: `movl $0xC000009B, %eax` → objdump shows something. Let me trust it.

Hmm, but objdump disassembly of `movl $0xc000009b, %eax` shows `movl $0xc000009b, %eax` (with size suffix). Grep should find it.

Kanıt 9:

```make
	@echo "  Kanit 9: QEMU davranis (DMA RAM + UC MMIO)..."
	@if ! grep -qxF "X86 DMA RAM BLK OK" $(BUILD)/kem_os_x86.out; then \
		echo "FAIL: 'X86 DMA RAM BLK OK' satiri yok"; exit 1; fi
	@if ! grep -qxF "X86 DMA RAM NET OK" $(BUILD)/kem_os_x86.out; then \
		echo "FAIL: 'X86 DMA RAM NET OK' satiri yok"; exit 1; fi
	@if ! grep -qxF "X86 MMIO UC OK lapic_surum=" $(BUILD)/kem_os_x86.out; then \
		echo "FAIL: 'X86 MMIO UC OK lapic_surum=' satiri yok"; exit 1; fi
```

Now: the task says "Başlık yorumuna Kanıt 7-9'u ekle (çapa: `# (yalnız .kem işlevi; tüm ikilide aramak C UART nesnesine de uyar).`)." So the anchor is that line which already exists. I insert after it.

Let me now check whether `yazdir_tam` outputs an integer. It's already used in the file with `yazdir_tam(toplam)`. OK.

Hmm, one problem: `vblk_data_yaz/oku` — I need to check they exist. Not shown in context. Task says to use them, so trust.

Also `dtam8` type — used in kem_virtio_net.kem. OK.

Let me check that `((i % 251) + 3) olarak dtam8` — is `tam32 olarak dtam8` valid? In kem_virtio_net.kem we see `vblk_y16(..., 0 olarak dtam16)` and `2 olarak dtam16`. So `olarak` casts. And in kem_heap.kem: `değişken cb: tam8 = c olarak tam8;`. So casting tam32 → dtam8 should be fine.

Now `vblk_data_yaz(i, ...)` — first arg is index. I'll assume signature (i: tam32, b: dtam8). Same for oku returning dtam8.

Let me also check the anchor for adding code: `    kem_panik("X86 PANIK YOLU OK");`. There's exactly one occurrence. I insert before it.

Wait — there's also mention in header comment: `// main sonunda kem_panik("X86 PANIK YOLU OK") ...`. But anchor is `    kem_panik("X86 PANIK YOLU OK");` with leading 4 spaces — that's the code line, unique.

Now I need to worry: is `0 - 1` a valid expression that assigns -1 to tam32? In kem_virtio_net.kem they use `ver 0 - 1;` returning tam32. And `değişken xyz: tam32 = 0 - 1;` should also work.

Actually let me be safer and use a different approach for the "not found" sentinel. Use `-1` directly? The task says "Sayılar ondalık yazılır". Negative numbers might not parse. Let me just use `0 - 1`.

Alternatively use `değişken blk_hata: tam32 = 512;` (index out of range) as the sentinel. Since valid indices are 0..511, 512 signals none found. That's cleaner and avoids `0 - 1`.

Let me use that pattern.

Actually for lapic I still need `!= -1` check. Use `lapic_surum != (0 - 1)`. Or check `lapic_surum > 0`? Since -1 is 0xFFFFFFFF, and sürüm register is small positive. But "0 değil ve -1 değilse" is the spec. Let me use `lapic_surum > 0` — but a valid sürüm could be 0? No, spec says not 0. Hmm let me stick to spec.

Let me use:
```
eğer lapic_surum != 0 {
    eğer lapic_surum != (0 - 1) {
        ... OK
    }
}
```

Actually looking again, the simplest recursive check: just sum the conditions.

Let me write it as:

```kem
    değişken lapic_surum: tam32 = kdl_mmio_oku32(4276092976);
    eğer lapic_surum == 0 {
        yazdir_metin("X86 MMIO UC HATA");
        yazdir_satir();
    }
    değilse {
        eğer lapic_surum == (0 - 1) {
            yazdir_metin("X86 MMIO UC HATA");
            yazdir_satir();
        }
        değilse {
            yazdir_metin("X86 MMIO UC OK lapic_surum=");
            yazdir_tam(lapic_surum);
            yazdir_satir();
        }
    }
```

OK.

Now, the header comment edit for kem_os_x86.kem. Anchor: the last line of header:
```
// mesaj .kem COM1 yolundan basılır, çekirdek orada durur; QEMU zaman aşımı keser.
```

I'll add:
```
//
// İş 3: kem_panik'ten ÖNCE üç DMA/MMIO testi eklenir (her biri DEĞER denetler):
//   (a) Blok DMA RAM — vblk_data_yaz/oku 0..511 yaz-oku karşılaştırma →
//       "X86 DMA RAM BLK OK" (değilse "X86 DMA RAM BLK HATA" + ilk indeks).
//   (b) Ağ DMA RAM — vnet_tx_yaz + çıplak x86_net_tx_oku 0..63 karşılaştırma →
//       "X86 DMA RAM NET OK".
//   (c) MMIO UC pencere — kdl_mmio_oku32(0xFEE00030) LAPIC sürüm yazmacı
//       (0 ve -1 DEĞİLSE) → "X86 MMIO UC OK lapic_surum=" + değer.
```

Now let me finalize everything. Also need `x86_net_tx_oku` helper before main. Anchor: I'll use the line right before `işlev main()`.

Original:
```
// main sonunda kem_panik("X86 PANIK YOLU OK") çağrılır: ...
```

Wait, I'm adding a block to that comment section too. Let me be careful with overlapping edits.

Let me structure two edits to kem_os_x86.kem:
1. Header comment: extend the block ending in "QEMU zaman aşımı keser." — but this line ends the header. Then there's empty line, then `işlev main()`.

Actually I need to insert the helper function AND the header extension. Simplest: one edit extends the header (adds the three-line description), and adds the helper. Let me use a single anchor: the last comment line + the blank lines + `işlev main()`.

Original:
```
// main sonunda kem_panik("X86 PANIK YOLU OK") çağrılır: panik geri dönmez,
// mesaj .kem COM1 yolundan basılır, çekirdek orada durur; QEMU zaman aşımı keser.

işlev main() -> tam32 {
```

New:
```
// main sonunda kem_panik("X86 PANIK YOLU OK") çağrılır: panik geri dönmez,
// mesaj .kem COM1 yolundan basılır, çekirdek orada durur; QEMU zaman aşımı keser.
//
// İş 3: kem_panik'ten ÖNCE üç DMA/MMIO testi eklenir (her biri DEĞER denetler):
//   (a) Blok DMA RAM — vblk_data_yaz/oku 0..511 yaz-oku karşılaştırma →
//       "X86 DMA RAM BLK OK" (değilse "X86 DMA RAM BLK HATA" + ilk indeks).
//   (b) Ağ DMA RAM — vnet_tx_yaz + çıplak x86_net_tx_oku 0..63 karşılaştırma →
//       "X86 DMA RAM NET OK".
//   (c) MMIO UC pencere — kdl_mmio_oku32(0xFEE00030) LAPIC sürüm yazmacı
//       (0 ve -1 DEĞİLSE) → "X86 MMIO UC OK lapic_surum=" + değer.

// Çıplak yardımcı: küresel vnet_dma'ya normal işlevden doğrudan erişim E010 verir.
çıplak işlev x86_net_tx_oku(i: tam32) -> dtam8 {
    ver vblk_o8(vnet_dma + NO_TXBUF + 12 + (i olarak tam64));
}

işlev main() -> tam32 {
```

Then second edit inserts the tests before kem_panik.

OK let me finalize.

Wait, `NO_TXBUF` is a constant in kem_virtio_net.kem. Since it's `sabit`, I can reference it. Good.

`vblk_o8` is defined in kem_virtio_blk.kem (used by vnet_rx_oku). Good.

Now, the tests in main — let me use gas-safe identifiers. All Turkish so far. Let me use unique variable names to avoid conflicts with existing `d`, `i`, `toplam`.

Let me use `dma_i`, `blk_hata`, `net_hata`, `lapic_surum`, `beklenen_blk`, `beklenen_net`.

Actually, "i" is already used in main for the sum loop. Let me use fresh names.

Let me write the actual edit for kem_os_x86.kem's tests:

```
    // İş 3 (a): Blok DMA RAM yaz-oku (0..511).
    değişken dma_i: tam32 = 0;
    iken dma_i < 512 {
        vblk_data_yaz(dma_i, (((dma_i % 251) + 3) olarak dtam8));
        dma_i = dma_i + 1;
    }
    değişken blk_hata: tam32 = 512;   // 512 = "hiç uyuşmazlık yok" (geçerli indeks 0..511)
    dma_i = 0;
    iken dma_i < 512 {
        değişken beklenen_blk: dtam8 = (((dma_i % 251) + 3) olarak dtam8);
        eğer vblk_data_oku(dma_i) != beklenen_blk {
            eğer blk_hata == 512 { blk_hata = dma_i; }
        }
        dma_i = dma_i + 1;
    }
    eğer blk_hata == 512 {
        yazdir_metin("X86 DMA RAM BLK OK");
        yazdir_satir();
    }
    değilse {
        yazdir_metin("X86 DMA RAM BLK HATA");
        yazdir_tam(blk_hata);
        yazdir_satir();
    }

    // İş 3 (b): Ağ TX tamponu yaz-oku (0..63).
    değişken net_i: tam32 = 0;
    iken net_i < 64 {
        vnet_tx_yaz(net_i, (((net_i % 241) + 7) olarak dtam8));
        net_i = net_i + 1;
    }
    değişken net_hata: tam32 = 64;
    net_i = 0;
    iken net_i < 64 {
        değişken beklenen_net: dtam8 = (((net_i % 241) + 7) olarak dtam8);
        eğer x86_net_tx_oku(net_i) != beklenen_net {
            eğer net_hata == 64 { net_hata = net_i; }
        }
        net_i = net_i + 1;
    }
    eğer net_hata == 64 {
        yazdir_metin("X86 DMA RAM NET OK");
        yazdir_satir();
    }
    değilse {
        yazdir_metin("X86 DMA RAM NET HATA");
        yazdir_tam(net_hata);
        yazdir_satir();
    }

    // İş 3 (c): MMIO UC pencere — LAPIC sürüm yazmacı (0xFEE00030 = 4276092976).
    değişken lapic_surum: tam32 = kdl_mmio_oku32(4276092976);
    eğer lapic_surum == 0 {
        yazdir_metin("X86 MMIO UC HATA");
        yazdir_satir();
    }
    değilse {
        eğer lapic_surum == (0 - 1) {
            yazdir_metin("X86 MMIO UC HATA");
            yazdir_satir();
        }
        değilse {
            yazdir_metin("X86 MMIO UC OK lapic_surum=");
            yazdir_tam(lapic_surum);
            yazdir_satir();
        }
    }

    kem_panik("X86 PANIK YOLU OK");
```

Wait — `yazdir_tam` takes a tam32? Looking at existing code, `yazdir_tam(toplam)` where `toplam: tam32`. Yes.

For header comment — I'll add before the new test block.

Also, do I want to name the third print exactly `X86 MMIO UC OK lapic_surum=` followed by a separate line? The Makefile grep uses `-qxF` for that exact line, and then the value is on its own line. But the task says "satırı + değeri `yazdir_tam` ile ayrı satırda bas". Yes, so `yazdir_metin` prints the "X86 MMIO UC OK lapic_surum=" with a trailing newline (yazdir_metin always adds newline per kem_os_x86.kem comment: "yazdir_metin satır sonu ekler"). Then `yazdir_tam(lapic_surum)` prints value, `yazdir_satir()` newline.

Hmm — wait, the header says yazdir_metin adds a newline. So `yazdir_metin("X86 MMIO UC OK lapic_surum=")` already prints with newline. Then `yazdir_tam(...)` and `yazdir_satir()`. Good.

OK. Concerning "Başlık yorumuna üç testi ekle" — I'll add that.

Now let me make sure I don't break the anchor `    kem_panik("X86 PANIK YOLU OK");`. It's in the file once.

Let me also double check: file ends with `}`. It's fine.

Now let me double check my start_x86_64.S edit doesn't break reset of %edi/%eax/%ecx for later steps. Step 2d uses %eax. Step 2e uses %eax/%cr4. Since I initialize them fresh, it's fine.

Now write the final output.

One more check: the header step 2 anchor in start_x86_64.S. Original anchor: ` *   2. (32-bit) Identity sayfa tabloları (0..1GB, 2MB huge page) + PAE + LME +`

The next line: ` *      paging → long mode; 64-bit kod segmentine far jump`

I need to match whitespace exactly. From the file:
```
 *   2. (32-bit) Identity sayfa tabloları (0..1GB, 2MB huge page) + PAE + LME +
 *      paging → long mode; 64-bit kod segmentine far jump
```

OK. My ESKI will be these two lines. YENI will be:
```
 *   2. (32-bit) Identity sayfa tabloları (0..1GB, 2MB huge page) + PAE + LME +
 *      Ayrıca 3..4 GB UC MMIO penceresi (pd_mmio: 0xC0000000..0xFFFFFFFF,
 *      PWT|PCD) + paging → long mode; 64-bit kod segmentine far jump
```

Good.

Now the pd_mmio fill block. Let me double-check label reuse. GNU as: `1:` local numeric label. After 2a has used `1:`, in 2c2 I use `1:` again. As long as `loop 1b` looks backward for the nearest previous `1:`, it works. In 2c2, the `1:` is right after the `movl $512, %ecx`. Good.

Now also, does the label `1` conflict with anything in code64 section? No, this is still in .code32 section. And we don't have any other `1:` in the code32 section. Good.

But wait — the file has `.code64` sections with `.macro ISR_NOERR` that use `\n` but not `1:`. So no conflict.

Alright, finalize.

For the Makefile, let me be careful about the "header" anchor: the line `# (yalnız .kem işlevi; tüm ikilide aramak C UART nesnesine de uyar).`

Original:
```
# Kanıt 6: objdump --disassemble-symbols=kem_uart_bayt içinde outb VE inb
# (yalnız .kem işlevi; tüm ikilide aramak C UART nesnesine de uyar).
calistir_kem_os_x86: $(BUILD)/kemgu$(EXE) \
```

New:
```
# Kanıt 6: objdump --disassemble-symbols=kem_uart_bayt içinde outb VE inb
# (yalnız .kem işlevi; tüm ikilide aramak C UART nesnesine de uyar).
# Kanıt 7: llvm-nm'de __heap_end < 0x02000000 (DMA tabanı heap'le çakışmaz).
# Kanıt 8: objdump'da 0xc000009b sabiti (UC MMIO PD sayfa bayrağı kurulu).
# Kanıt 9: QEMU seri çıktıda "X86 DMA RAM BLK OK", "X86 DMA RAM NET OK" tam satırları
#          ve "X86 MMIO UC OK lapic_surum=" (LAPIC sürüm — UC pencere okunabiliyor).
calistir_kem_os_x86: $(BUILD)/kemgu$(EXE) \
```

Wait, hmm — the anchor given is `# (yalnız .kem işlevi; tüm ikilide aramak C UART nesnesine de uyar).` — accurately matches. Let me use the two-line anchor to be safe.

OK, let me now write everything.

Also — the Makefile has `\t` (tab). I need to preserve tabs. The heredoc shows tabs as tabs; I'll use actual tabs. In the output, I'll write tabs (they'll show as tab characters).

Let me write the final output.

Actually, one concern: In the Makefile, `$(BUILD)` uses `$(`. In my new lines I'll keep that. Also need to escape `$` in shell commands. Fine.

For Kanıt 7's shell: in make, `$$` is for a shell `$`. `$$(llvm-nm ...)` would run llvm-nm and capture output.

Let me write it cleanly:

```make
	@echo "  Kanit 7: yerlesim (__heap_end < DMA tabani 0x02000000)..."
	@he=$$(llvm-nm $(BUILD)/kem_os_x86.elf | awk '/__heap_end/ {print $$1; exit}'); \
	if [ -z "$$he" ]; then echo "FAIL: __heap_end sembolu yok"; exit 1; fi; \
	hedec=$$((0x$$he)); \
	echo "    __heap_end = 0x$$he ($$hedec)"; \
	if [ "$$hedec" -ge 33554432 ]; then \
		echo "FAIL: __heap_end ($$hedec) >= 0x02000000 (DMA tabani)"; exit 1; fi
```

Wait, `$$((0x$$he))` — in make this becomes `$( (0x$he))`? No: `$$` becomes `$` in shell. So `$$((0x$$he))` becomes `$((0x$he))`. The `$he` expands to `he`'s value (e.g. `110f000` without 0x prefix). So `$( (0x110f000))` = 17903616. Then `hedec` = 17903616. Good.

Hmm but wait: `$(( ... ))` doesn't do word splitting on the parameter expansion? Actually inside `$(( ))`, `$he` is arithmetic, and standard says only integer constants, variables. A hex constant `0x110f000` is fine. Good.

Actually there's a subtlety in dash — inside `$(( ))`, variable expansion happens, so `0x$he` becomes `0x110f000`. Good, then arithmetic on it = 17903616.

OK.

Now Kanıt 8:

```make
	@echo "  Kanit 8: UC MMIO bayragi (objdump'da 0xc000009b)..."
	@if ! llvm-objdump -d --no-show-raw-insn $(BUILD)/kem_os_x86.elf | grep -q '0xc000009b'; then \
		echo "FAIL: 0xc000009b objdump'da yok (UC MMIO sayfa bayragi)"; exit 1; fi
	@llvm-objdump -d --no-show-raw-insn $(BUILD)/kem_os_x86.elf | grep '0xc000009b'
```

Concern: objdump may output `$0xc000009b` on the immediate operand line: `movl $0xc000009b, %eax`. grep looks for `0xc000009b` substring — matches. Good. But also the `addl $0x200000, %eax` in the loop — that's `0x200000`, not `0xc000009b`. Good.

But there's a subtle issue: after the movl, the loop iterates and eax = 0xc000009b + i*0x200000. Only the initial movl has the literal. So one match. Fine.

Hmm wait, actually could objdump not print immediate in AT&T? It does print `$0xc000009b`. Good.

Kanıt 9:

```make
	@echo "  Kanit 9: QEMU davranis (DMA RAM + UC MMIO)..."
	@if ! grep -qxF "X86 DMA RAM BLK OK" $(BUILD)/kem_os_x86.out; then \
		echo "FAIL: 'X86 DMA RAM BLK OK' satiri yok"; exit 1; fi
	@if ! grep -qxF "X86 DMA RAM NET OK" $(BUILD)/kem_os_x86.out; then \
		echo "FAIL: 'X86 DMA RAM NET OK' satiri yok"; exit 1; fi
	@if ! grep -qxF "X86 MMIO UC OK lapic_surum=" $(BUILD)/kem_os_x86.out; then \
		echo "FAIL: 'X86 MMIO UC OK lapic_surum=' satiri yok"; exit 1; fi
```

Alright. Ready to write the output.

Wait — I should double-check: making `yazdir_satir()` after `yazdir_metin` produces an extra blank line (since yazdir_metin already adds newline). Looking at existing main: `yazdir_metin("X86 HEAP DIZI OK toplam=");` then `yazdir_tam(toplam); yazdir_satir();`. So `yazdir_metin` prints "...toplam=\n", then `yazdir_tam(55)` prints "55", then `yazdir_satir()` prints "\n". And the check is `grep -A1 -xF "X86 HEAP DIZI OK toplam=" | tail -1 | grep -qxF "55"`.

So actually `yazdir_metin` adds newline. My calls for "BLK OK" would produce "X86 DMA RAM BLK OK\n". Then `yazdir_satir()` adds another newline → blank line. Not a problem for grep -qxF.

But wait, actually I should just call `yazdir_metin("X86 DMA RAM BLK OK");` without `yazdir_satir()` since the newline is already added. Let me match the existing patterns — the existing pattern calls `yazdir_satir()` only after a `yazdir_tam`. So:

- For pure message: just `yazdir_metin(...)`.
- For message + value: `yazdir_metin(msg)`, `yazdir_tam(val)`, `yazdir_satir()`.

Good, let me clean up.

Final form:

```
    // İş 3 (a): Blok DMA RAM yaz-oku (0..511).
    ... 
    eğer blk_hata == 512 {
        yazdir_metin("X86 DMA RAM BLK OK");
    }
    değilse {
        yazdir_metin("X86 DMA RAM BLK HATA");
        yazdir_tam(blk_hata);
        yazdir_satir();
    }
```

Great. Similarly for net.

For lapic:
```
    eğer lapic_surum == 0 {
        yazdir_metin("X86 MMIO UC HATA");
    }
    değilse {
        eğer lapic_surum == (0 - 1) {
            yazdir_metin("X86 MMIO UC HATA");
        }
        değilse {
            yazdir_metin("X86 MMIO UC OK lapic_surum=");
            yazdir_tam(lapic_surum);
            yazdir_satir();
        }
    }
```

Good.

Now the final ESKI for kem_os_x86.kem test insertion — I need to match whitespace. Anchor:
`    kem_panik("X86 PANIK YOLU OK");`

That's 4 spaces before `kem_panik`.

OK, let me write out everything now.
