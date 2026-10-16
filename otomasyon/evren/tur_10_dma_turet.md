# Tur 10 — dma_turet — `deepseek-v4.1-flash`

- Tarih: 2026-10-12 10:20:49 · süre 473.5 sn · finish `stop` · reasoning_effort `low`
- usage: `{"prompt_tokens": 14796, "total_tokens": 26938, "completion_tokens": 12142, "prompt_tokens_details": {"cached_tokens": 10624, "created_cache_tokens": 4160}, "completion_tokens_details": {"reasoning_tokens": 9058}}`
- Değişiklik blokları: 4 — duzenle:runtime/mimari/x86_64/kem_platform.kem, duzenle:test/ornekler/kem_os_x86.kem, duzenle:Makefile, duzenle:Makefile
- İstem: [tur_10_dma_turet_istem.md](tur_10_dma_turet_istem.md)

## Yanıt

Kanıt 13 ve x86 DMA tabanı türetme için üç dosyada düzenleme:

```duzenle:runtime/mimari/x86_64/kem_platform.kem
<<<<<<< ESKI
sabit VM_TARAMA_TABAN: tam64 = 4272947200;  // 0xFEB00000 (QEMU microvm)
sabit VM_YUVA_SAYISI: tam32 = 24;
=======
sabit VM_TARAMA_TABAN: tam64 = 4272947200;  // 0xFEB00000 (QEMU microvm)
sabit VM_YUVA_SAYISI: tam32 = 24;

// İş 10: x86 DMA tabanını PVH bellek haritasından ÇALIŞMA ZAMANINDA türet.
// ----------------------------------------------------------------------------
// NEDEN RAM TEPESİ: kem_os heap'i ~17 MB'ta (__heap_end) biter; kanal/görev
// tahsisleri de MB mertebesinde kalır → RAM tepesinin 256 KB altı, heap'in ÇOK
// üstünde ve dolayısıyla çakışma yok. "DMA bölgesi __heap_end ÜSTÜNDE olmalı"
// kuralı (bu dosya başlığı, Kanıt 7) böylece hâlâ sağlanır.
// NEDEN 1 GB SINIRI: start_x86_64.S yalnız İLK 1 GB'ı 2 MB huge page PD
// girdileriyle (P|RW|PS) identity eşler → 1073741824 üstü adres page fault.
// Bu yüzden her RAM parçasının `son`u 1 GB ile kırpılır.
// NEDEN 256 KB PAY: DMA tabanı RAM tepesine dayanırsa kernel .bss/heap ile
// çakışabilir → 262144 bayt emniyet payı bırakıp 4 KB aşağı hizala.
// PVH YOKSA: ver 0; kem_platform_kur sABİT yedeğe (dosya başındaki küresel
// başlangıç değerleri) dokunmaz → x86 microvm dışı boot yine çalışır.
// Seçim: tür==1 (RAM) parçaları arasında kırpılmış `son` EN BÜYÜK olanı.
// QEMU 128 MB'ta beklenen: 134217728 - 262144 = 133955584 (0x7FC0000, 130816 KB).
// Aynı yöntem: kem_pvh_bilgi + kdl_mmio_oku32 + kem_pvh_oku64 (aynı mimari
// dizini → Makefile CAT listesi hepsini tek birimde toplar).
çıplak işlev kem_platform_dma_tabani() -> tam64 {
    değişken b: tam64 = kem_pvh_bilgi();
    eğer b == 0 { ver 0; }                               // PVH yok → yedeğe düş
    eğer kdl_mmio_oku32(b) != 862897528 { ver 0; }       // magic (u32 @0)
    eğer kdl_mmio_oku32(b + 4) < 1 { ver 0; }            // version (memmap var)
    değişken mm: tam64 = kem_pvh_oku64(b + 40);          // memmap_paddr
    değişken n: tam32 = kdl_mmio_oku32(b + 48);          // memmap_entries
    değişken sec: tam64 = 0;                             // en büyük kırpılmış `son`
    değişken i: tam32 = 0;
    iken i < n {
        değişken e: tam64 = mm + (i olarak tam64) * 24;  // girdi 24 bayt
        değişken tur: tam32 = kdl_mmio_oku32(e + 16);    // tür @16; RAM=1
        eğer tur == 1 {
            değişken adr: tam64 = kem_pvh_oku64(e);      // addr @0
            değişken boy: tam64 = kem_pvh_oku64(e + 8);  // size @8
            değişken son: tam64 = adr + boy;
            eğer son > 1073741824 { son = 1073741824; }  // identity eşleme sınırı
            eğer son > adr {
                eğer son - adr >= 262144 {               // en az 256 KB RAM
                    eğer son > sec { sec = son; }        // en büyük tepesi
                }
            }
        }
        i = i + 1;
    }
    eğer sec == 0 { ver 0; }                             // uygun parça yok → yedek
    ver ((sec - 262144) / 4096) * 4096;                  // 256 KB aşağı, 4 KB hizalı
}

// İş 10: platform kurulumu. t > 0 ise vblk_dma/vnet_dma'yı TÜRET; aksi hâlde
// küresel başlangıç değerleri (sabit yedek: vblk=32 MB, vnet=32 MB+64 KB) KALIR.
// Blok ihtiyacı ~800 B; ağ ~19 KB (NO_TXBUF+2048 + RX 8×2048 ≈ 19 KB) → arada
// 64 KB boşluk güvenli.
çıplak işlev kem_platform_kur() {
    değişken t: tam64 = kem_platform_dma_tabani();
    eğer t > 0 {
        vblk_dma = t;
        vnet_dma = t + 65536;                            // 64 KB yukarı (net ~19 KB)
    }
}
>>>>>>> YENI
```

```duzenle:test/ornekler/kem_os_x86.kem
<<<<<<< ESKI
    // İş 3 (a): Blok DMA RAM yaz-oku (0..511; 251 ile döngüsel desen).
=======
    kem_platform_kur();
    değişken dt: tam64 = kem_platform_dma_tabani();
    eğer dt > 0 {
        yazdir_metin("X86 DMA TABAN OK kb=");
        yazdir_tam((dt / 1024) olarak tam32);
        yazdir_satir();
    } değilse {
        yazdir_metin("X86 DMA TABAN YEDEK");
    }

    // İş 3 (a): Blok DMA RAM yaz-oku (0..511; 251 ile döngüsel desen).
>>>>>>> YENI
```

```duzenle:Makefile
<<<<<<< ESKI
#          ayrıca pvh_entry disasm'ında %ebx saklama movl'i (PVH start_info ABI).
=======
#          ayrıca pvh_entry disasm'ında %ebx saklama movl'i (PVH start_info ABI).
# Kanıt 13: QEMU seri çıktıda "X86 DMA TABAN OK kb=" TAM satırı ve HEMEN ardındaki
#          satır yedek sabitten (32768 KB) BÜYÜK — PVH memmap RAM tepesinden
#          türetilen DMA tabanı (kem_platform.kem). Değer de yazdırılır.
>>>>>>> YENI
```

```duzenle:Makefile
<<<<<<< ESKI
	@llvm-objdump -d --no-show-raw-insn --disassemble-symbols=pvh_entry $(BUILD)/kem_os_x86.elf | grep '%ebx' || { echo "FAIL: pvh_entry disasm'da %ebx movl'i yok"; exit 1; }
=======
	@llvm-objdump -d --no-show-raw-insn --disassemble-symbols=pvh_entry $(BUILD)/kem_os_x86.elf | grep '%ebx' || { echo "FAIL: pvh_entry disasm'da %ebx movl'i yok"; exit 1; }
	@echo "  Kanit 13: x86 DMA tabani PVH memmap'ten turetildi (yedek 32768 KB'den buyuk)..."
	@if ! grep -qxF "X86 DMA TABAN OK kb=" $(BUILD)/kem_os_x86.out; then \
		echo "FAIL: 'X86 DMA TABAN OK kb=' satiri yok"; exit 1; fi
	@v=`grep -A1 -xF "X86 DMA TABAN OK kb=" $(BUILD)/kem_os_x86.out | tail -1`; \
	if [ "$$v" -gt 32768 ]; then \
		echo "  DMA taban KB = $$v"; \
	else \
		echo "FAIL: DMA tabani yedek sabitten (32768 KB) buyuk degil: $$v"; exit 1; fi
>>>>>>> YENI
```

## Açıklama

1. **`kem_platform.kem`** — `kem_platform_dma_tabani()`: PVH `hvm_start_info` magic/version kontrolü ve memmap taraması (`kem_pvh_ram_toplam` ile aynı yöntem: `kem_pvh_bilgi` + `kdl_mmio_oku32` + `kem_pvh_oku64`). Tür==1 girdilerin `son = addr + boy` değeri 1 GB ile kırpılır; ≥256 KB (262144) parçalar arasından en büyük `son` seçilir. Sonuç `((sec-262144)/4096)*4096`. PVH yok/magic uyuşmaz/uygun parça yok → `ver 0`. `kem_platform_kur()` `t>0` ise `vblk_dma=t; vnet_dma=t+65536;`, aksi hâlde sabitler korunur. Kemgu idiomu (`çıplak işlev`, ham-pointer, `küresel değişken` erişimi) ve dosyanın mevcut yorum üslubuyla uyumlu.

2. **`kem_os_x86.kem`** — İş 3 (a) DMA RAM testinden HEMEN ÖNCE `kem_platform_kur()` + `kem_platform_dma_tabani()` çağrılır; `dt>0` ise `X86 DMA TABAN OK kb=` (yazdir_metin zaten satır sonu ekler, araya `yazdir_satir` konmadı) ve ayrı satırda `dt/1024` yazılır + `yazdir_satir()`; aksi hâlde `X86 DMA TABAN YEDEK`. Böylece sonraki DMA/disk/ağ/FS testleri türetilmiş tabanı kullanır.

3. **Makefile Kanıt 13** — `grep -qxF` ile etiket TAM satır, `grep -A1 ... | tail -1` ile alt satır `v` alınır, `[ "$v" -gt 32768 ]` (dash uyumlu, `16#` yok) kontrolü; başarıda değer yazdırılır, değilse FAIL. Hedef yorumuna Kanıt 13 açıklaması eklendi.

## VARSAYIMLAR
- `yazdir_metin(metin)` satır sonu ekliyor, `yazdir_tam(tam32)` satır sonu EKLEMİYOR, `yazdir_satir()` yalnız `\n` basıyor (görev metnindeki tariften).
- `kem_pvh_bilgi`, `kem_pvh_oku64`, `kdl_mmio_oku32` x86_64 derlemesinde `kem_platform.kem` ile aynı CAT birimindedir (kem_pvh.kem ve kem_heap.kem aynı mimari/paylaşılan kümede).
- Çapa satırları (`sabit VM_YUVA_SAYISI...`, `// İş 3 (a)...`, objdump satırı, pvh_entry yorum satırı) dosyalarda TEK kez geçiyor.
- `test/ornekler/kem_os_x86.kem` içinde `kem_platform_kur`/`kem_platform_dma_tabani`/`dt` adları çakışmıyor.
- Version alanı (`u32 @4`) kontrolü spec'te zorunlu değildi; `kem_pvh_ram_toplam` örüntüsüne sadık kalmak için eklendi (zararsız, doğru PVH'de ≥1).

## Düşünme (reasoning)

Let me analyze the task carefully.

We need 4 changes:
1. Add two functions to `runtime/mimari/x86_64/kem_platform.kem`
2. Modify `test/ornekler/kem_os_x86.kem` to call them
3. Add Kanıt 13 to Makefile
4. Add comment about Kanıt 13 to Makefile target comment

Let me start with change 1.

Current end of kem_platform.kem:
```
// VirtIO-MMIO cihaz tarama yuvası tabanı + yuva sayısı. Adım 0x200 (512) iki
// makinede de aynı. x86'da virtio-mmio YALNIZ -M microvm'de var (24 yuva);
// pc makinesinde virtio-mmio YOK.
sabit VM_TARAMA_TABAN: tam64 = 4272947200;  // 0xFEB00000 (QEMU microvm)
sabit VM_YUVA_SAYISI: tam32 = 24;
```

We need to add after `sabit VM_YUVA_SAYISI: tam32 = 24;`:

```
çıplak işlev kem_platform_dma_tabani() -> tam64 { ... }
çıplak işlev kem_platform_kur() { ... }
```

The function logic:
- `b = kem_pvh_bilgi()`
- If b == 0 or magic != 862897528 → return 0
- Iterate memmap (same as kem_pvh_ram_toplam)
- For type==1 entries: `son = addr + size`, if `son > 1073741824` then `son = 1073741824`
- Among those with `son - addr >= 262144`, select LARGEST `son`
- If not found → return 0
- return `((son - 262144) / 4096) * 4096`

Let me write:

```
// İş 10 (D-6xx): x86 DMA tabanını çalışma zamanında PVH memmap'ten türet.
// RAM tepesinin 256 KB (=262144) ALTINA, 4 KB hizalı yerleştir. Neden RAM tepesi:
// heap ~17 MB'ta biter, __heap_end ≈ 0x110F000 (~17.8 MB), DMA ihtiyacı
// (vblk ~800 B + vnet ~19 KB) çok küçük → tepedeki boşluk çakışmaz; ayrıca
// boot yalnız İLK 1 GB'ı (1073741824) identity eşler (start_x86_64.S 2 MB huge
// page PD girdileri) → son 1 GB ile kırpılır, sınır-üstü adres erişilemez.
// PVH yoksa ya da magic uyuşmazsa → ver 0; çağıran sabit yedek (32768 KB) tutar.
// kem_pvh_bilgi/kem_pvh_oku64/kdl_mmio_oku32 aynı mimari dizininde → CAT edilir.
çıplak işlev kem_platform_dma_tabani() -> tam64 {
    değişken b: tam64 = kem_pvh_bilgi();
    eğer b == 0 { ver 0; }
    eğer kdl_mmio_oku32(b) != 862897528 { ver 0; }
    eğer kdl_mmio_oku32(b + 4) < 1 { ver 0; }
    değişken mm: tam64 = kem_pvh_oku64(b + 40);
    değişken n: tam32 = kdl_mmio_oku32(b + 48);
    değişken enbuyuk: tam64 = 0;                     // en büyük uygun son (0 = bulunamadı)
    değişken i: tam32 = 0;
    iken i < n {
        değişken e: tam64 = mm + (i olarak tam64) * 24;
        değişken tur: tam32 = kdl_mmio_oku32(e + 16);
        eğer tur == 1 {
            değişken adr: tam64 = kem_pvh_oku64(e);
            değişken boy: tam64 = kem_pvh_oku64(e + 8);
            değişken son: tam64 = adr + boy;
            eğer son > 1073741824 { son = 1073741824; }   // identity eşleme sınırı
            eğer son > adr {
                eğer son - adr >= 262144 {               // ≥256 KB RAM parçası
                    eğer son > enbuyuk { enbuyuk = son; }
                }
            }
        }
        i = i + 1;
    }
    eğer enbuyuk == 0 { ver 0; }
    ver ((enbuyuk - 262144) / 4096) * 4096;
}

// İş 10: platform kurulumu — vblk_dma/vnet_dma'yı türet; türetilemezse SABİT
// yedekler (33554432/34603008 = 32768 KB) KALIR (küresel başlangıç değerleri).
çıplak işlev kem_platform_kur() {
    değişken t: tam64 = kem_platform_dma_tabani();
    eğer t > 0 {
        vblk_dma = t;
        vnet_dma = t + 65536;                        // net ~19 KB ihtiyaç, blk ~800 B
    }
}
```

Wait, but the constant `65536` is 64 KB. Task says "net ~19 KB ihtiyaç, blk ~800 B". So `vnet_dma = t + 65536;` — 64 KB apart. OK.

Also, the task mentions "Sabit yedek taban KB olarak 32768" — so backup is 32768 KB = 33554432 bytes. That matches the existing vblk_dma value. Good.

Now, checking kem_platform.kem — does it currently have `kdl_mmio_oku32` and `kem_pvh_oku64` available? These are in kem_pvh.kem (same arch dir). Since Makefile CATs files for x86_64, and both kem_pvh.kem and kem_platform.kem are for x86_64, they should be CAT'd together. Let me check — kdl_mmio_oku32 is defined in kem_heap.kem (which is platform-independent, always CAT'd). kem_pvh_oku64 is in kem_pvh.kem. We assume it's CAT'd.

Actually wait, the task says "Bağlam: `runtime/mimari/x86_64/kem_pvh.kem` (`kem_pvh_bilgi`, `kem_pvh_oku64`, memmap düzeni ve RAM türü=1 orada belgeli)". So yes, these are available.

But there might be a subtle issue: kem_platform.kem currently uses `kdl_mmio_oku32`? No, it just has constants. But now we use kdl_mmio_oku32, kem_pvh_bilgi, kem_pvh_oku64. As long as all files are CAT'd together for x86_64.

Hmm, but is kem_pvh.kem always CAT'd for x86_64? We should verify Makefile CAT list. But the task says the goal is to reference those — we trust the orchestrator.

Now change 2: test/ornekler/kem_os_x86.kem. We need to add code before `    // İş 3 (a): Blok DMA RAM yaz-oku (0..511; 251 ile döngüsel desen).`

Let me find the exact anchor. The task says:
ÇAPA: `    // İş 3 (a): Blok DMA RAM yaz-oku (0..511; 251 ile döngüsel desen).`

We need to insert before this line:
```
    kem_platform_kur();
    değişken dt: tam64 = kem_platform_dma_tabani();
    eğer dt > 0 {
        yazdir_metin("X86 DMA TABAN OK kb=");
        yazdir_tam((dt / 1024) olarak tam32);
        yazdir_satir();
    } değilse {
        yazdir_metin("X86 DMA TABAN YEDEK");
        yazdir_satir();
    }
```

Wait, the task says: `dt > 0 ise `X86 DMA TABAN OK kb=` + ayrı satırda `(dt / 1024) olarak tam32`; değilse `X86 DMA TABAN YEDEK`. (`yazdir_metin` zaten satır sonu ekler — arasına `yazdir_satir()` KOYMA; sayıdan sonra `yazdir_satir()` koy.)`

Hmm the wording is subtle: "dt > 0 ise `X86 DMA TABAN OK kb=` + ayrı satırda `(dt / 1024) olarak tam32`" — so first print label (which adds newline automatically), then print the number on the next line, then after the number add `yazdir_satir()`.

Wait but yazdir_tam might not add newline? Let me think. The task says "yazdir_metin zaten satır sonu ekler" (yazdir_metin already adds newline), so between the label and number there's no need for yazdir_satir. But "sayıdan sonra yazdir_satir() koy" (after the number put yazdir_satir()).

So:
```
    yazdir_metin("X86 DMA TABAN OK kb=");
    yazdir_tam((dt / 1024) olarak tam32);
    yazdir_satir();
```

For the else case:
```
    yazdir_metin("X86 DMA TABAN YEDEK");
```
Just that (yazdir_metin adds newline).

Hmm but wait — the look at context: yazdir_metin/yazdir_tam/yazdir_satir. Let me look at the existing usage pattern to be sure.

We don't have the full file, only lines 1-25. We don't know exact calls. But the task tells us the functions exist: `yazdir_metin`, `yazdir_tam`, `yazdir_satir`.

Let me be careful. The task says `yazdir_metin` already adds newline — hence `X86 DMA TABAN OK kb=` will be on its own line. Then `yazdir_tam` prints the number (presumably without newline). Then `yazdir_satir()` adds the newline.

OK, let me structure:

```
    kem_platform_kur();
    değişken dt: tam64 = kem_platform_dma_tabani();
    eğer dt > 0 {
        yazdir_metin("X86 DMA TABAN OK kb=");
        yazdir_tam((dt / 1024) olarak tam32);
        yazdir_satir();
    } değilse {
        yazdir_metin("X86 DMA TABAN YEDEK");
    }

    // İş 3 (a): Blok DMA RAM yaz-oku (0..511; 251 ile döngüsel desen).
```

Hmm wait, but the anchor only shows the comment line. We need to be careful about exact match. Task says anchor is:
`    // İş 3 (a): Blok DMA RAM yaz-oku (0..511; 251 ile döngüsel desen).`

So my OLD block will be just that one line, and I add before it.

Actually, we need to be sure the line is unique in the file. Let me assume it is (the task identifies it as anchor).

OK.

Now change 3: Makefile Kanıt 13. Add after Kanıt 12's objdump line.

The anchor given:
```
	@llvm-objdump -d --no-show-raw-insn --disassemble-symbols=pvh_entry $(BUILD)/kem_os_x86.elf | grep '%ebx' || { echo "FAIL: pvh_entry disasm'da %ebx movl'i yok"; exit 1; }
```

We add after:
```
	@echo "  Kanit 13: x86 DMA tabani PVH memmap'ten turetildi (yedek 32768 KB'den buyuk)..."
	@if ! grep -qxF "X86 DMA TABAN OK kb=" $(BUILD)/kem_os_x86.out; then \
		echo "FAIL: 'X86 DMA TABAN OK kb=' satiri yok"; exit 1; fi
	@v=`grep -A1 -xF "X86 DMA TABAN OK kb=" $(BUILD)/kem_os_x86.out | tail -1`; \
	if [ "$$v" -gt 32768 ]; then \
		echo "  DMA taban KB = $$v"; \
	else \
		echo "FAIL: DMA tabani yedek sabitten buyuk degil ($$v)"; exit 1; fi
```

Wait, the task says: `X86 DMA TABAN OK kb=` TAM satırı (`grep -qxF`); ardından gelen satır `v` olarak alınıp `[ "$$v" -gt 32768 ]` olmalı; değeri yazdır; değilse FAIL. Tarif kabuğu dash: `16#` KULLANMA.

So we use `grep -A1 -xF` to get next line. Or `grep -q` then `tail -1`. Let me follow the earlier pattern from Kanıt 12:

```
	@if ! grep -qxF "X86 PVH OK ram_mb=" $(BUILD)/kem_os_x86.out; then \
		echo "FAIL: 'X86 PVH OK ram_mb=' satiri yok"; exit 1; fi
	@if ! grep -A1 -xF "X86 PVH OK ram_mb=" $(BUILD)/kem_os_x86.out | tail -1 | grep -qE '^[1-9][0-9]*$$'; then \
		echo "FAIL: PVH OK sonrasi pozitif sayi satiri yok"; exit 1; fi
```

I'll create something analogous for Kanıt 13. But the requirement is `[ "$$v" -gt 32768 ]`.

Need to be careful with dash. In a recipe, `$$v` becomes `$v` for the shell. Using `[ "$v" -gt 32768 ]`.

For dash compatibility, using integer comparison in `[` is fine.

Let me write:
```
	@echo "  Kanit 13: x86 DMA tabani PVH memmap'ten turetildi (yedek 32768 KB'den buyuk)..."
	@if ! grep -qxF "X86 DMA TABAN OK kb=" $(BUILD)/kem_os_x86.out; then \
		echo "FAIL: 'X86 DMA TABAN OK kb=' satiri yok"; exit 1; fi
	@v=`grep -A1 -xF "X86 DMA TABAN OK kb=" $(BUILD)/kem_os_x86.out | tail -1`; \
	if [ "$$v" -gt 32768 ]; then \
		echo "  DMA taban KB = $$v"; \
	else \
		echo "FAIL: DMA tabani yedek sabitten (32768 KB) buyuk degil: $$v"; exit 1; fi
```

Hmm, `$$v` in Makefile becomes `$v` in shell. In a recipe, `$$` → `$`. So `["$$v" -gt 32768]` in the makefile source becomes `["$v" -gt 32768]` in shell. Good.

Actually using backticks with `$$` — `v=\`grep ... | tail -1\`` → `v=$(grep ... | tail -1)`. Fine.

But careful: the recipe line uses backticks. Task says "Tarif kabuğu dash: `16#` KULLANMA." — meaning don't use bash-specific base conversion. We're not.

Let me also think about whether `grep -qE '^[1-9][0-9]*$$'` would be better but they want integer comparison of `>32768`. So we use `[ "$v" -gt 32768 ]`.

Actually, in dash, `[ "$v" -gt 32768 ]` will fail if v is empty or non-numeric. If the previous grep failed, v would be empty. But we already checked the label line exists; the next line should be the number. To be safe maybe add `case` or `-n` check. But task says just the comparison.

Let's just do it.

Now change 4: Makefile hedef yorumu — Kanıt 13 açıklaması. Anchor:
`#          ayrıca pvh_entry disasm'ında %ebx saklama movl'i (PVH start_info ABI).`

We add after that line:
```
# Kanıt 13: QEMU seri çıktıda "X86 DMA TABAN OK kb=" TAM satırı ve hemen
#          ardından gelen satır yedek sabit 32768 KB'den BÜYÜK (PVH memmap'ten
#          türetilen DMA tabanı, kem_platform.kem); değer yazdırılır.
```

Let me now finalize the blocks.

Block 1: runtime/mimari/x86_64/kem_platform.kem

OLD:
```
sabit VM_YUVA_SAYISI: tam32 = 24;
```

Hmm, but that string — `sabit VM_YUVA_SAYISI: tam32 = 24;` — could there be ambiguity? No, likely fine. But it might appear once. Let me include more context to be safe:

OLD:
```
sabit VM_TARAMA_TABAN: tam64 = 4272947200;  // 0xFEB00000 (QEMU microvm)
sabit VM_YUVA_SAYISI: tam32 = 24;
```

That's unique.

NEW: same plus new content.

Actually I need to know if the file uses `değişken` keyword and syntax. Yes, from kem_heap.kem. And the pattern shown in kem_pvh.kem uses `değişken`. Good.

Let me double-check `çıplak işlev` — yes.

Also I need to be careful about using `ver` for `kem_platform_kur()` which has no return — it should not have `-> tip`. Like `kdl_bolge_serbest` in kem_heap.kem. OK.

Also the task says for `kem_platform_dma_tabani`: `ver 0` in some cases and `ver ((son - 262144) / 4096) * 4096;` at the end. Note: the return type is `tam64`, and we use `son` as the largest. Let me rename to match: "EN BÜYÜK `son`'u seç" — so `enbuyuk` or just `sec` — I'll call it `sec`.

Let me refine the function:

```
çıplak işlev kem_platform_dma_tabani() -> tam64 {
    değişken b: tam64 = kem_pvh_bilgi();
    eğer b == 0 { ver 0; }
    eğer kdl_mmio_oku32(b) != 862897528 { ver 0; }
    eğer kdl_mmio_oku32(b + 4) < 1 { ver 0; }
    değişken mm: tam64 = kem_pvh_oku64(b + 40);
    değişken n: tam32 = kdl_mmio_oku32(b + 48);
    değişken sec: tam64 = 0;
    değişken i: tam32 = 0;
    iken i < n {
        değişken e: tam64 = mm + (i olarak tam64) * 24;
        değişken tur: tam32 = kdl_mmio_oku32(e + 16);
        eğer tur == 1 {
            değişken adr: tam64 = kem_pvh_oku64(e);
            değişken boy: tam64 = kem_pvh_oku64(e + 8);
            değişken son: tam64 = adr + boy;
            eğer son > 1073741824 { son = 1073741824; }
            eğer son > adr {
                eğer son - adr >= 262144 {
                    eğer son > sec { sec = son; }
                }
            }
        }
        i = i + 1;
    }
    eğer sec == 0 { ver 0; }
    ver ((sec - 262144) / 4096) * 4096;
}
```

QEMU 128 MB'ta beklenen türetilmiş taban 0x7FC0000 = 133955584 (130816 KB).
128 MB = 134217728. So son = 134217728 (RAM top). 134217728 - 262144 = 133955584. Then /4096 *4096 = 133955584 (already 4096-aligned: 133955584 / 4096 = 32704.0). Yes. Correct. 133955584 / 1024 = 130816. Great.

Now, what about the check for magic + version? Task says: "b == 0 ya da magic (u32 @b, `kdl_mmio_oku32`) ≠ 862897528 → `ver 0`." So version check is not required, but including it is harmless. Actually let me keep it minimal to match RAM toplam pattern. Actually task says "memmap'i gez (kem_pvh_ram_toplam ile aynı yöntem)". The ram_toplam function does check version. Let me include it to be safe.

Actually re-reading: "b == 0 ya da magic (u32 @b, `kdl_mmio_oku32`) ≠ 862897528 → `ver 0`." It only mentions magic. But "aynı yöntem" suggests similar. I'll include version check too — it's harmless.

Hmm, but to strictly match the spec: "b == 0 ya da magic ... ≠ 862897528 → ver 0". I'll add version check anyway — it's a refinement and matches kem_pvh_ram_toplam. Actually, since they explicitly say only magic, maybe I should stick to that. But if version <1, memmap_paddr is invalid... Let me stick to what kem_pvh_ram_toplam does (which is the reference implementation) — include version check.

Actually, the spec says "aynı yöntem" which means use the same reading method (kem_pvh_bilgi, kdl_mmio_oku32, kem_pvh_oku64, memmap entries with type==1). Including version check is consistent with the exemplar. I'll include it.

Now, the comment about Kemgu style — Turkish comments. Let me write clean Turkish.

Now let me also check: `1 GB = 1073741824` — yes.

Now, change 2 in test file. Let me think about indentation. Looking at line 22-25 context:
```
// İş 3: kem_panik'ten ÖNCE üç DMA/MMIO testi koşar (her biri DEĞER denetler,
// sabit metin basmaz — D-264 dersi):
```

So the file uses 4-space indent for code inside main (presumably). The anchor `    // İş 3 (a): ...` starts with 4 spaces. So my inserted lines also 4 spaces.

Let me write:
```
    kem_platform_kur();
    değişken dt: tam64 = kem_platform_dma_tabani();
    eğer dt > 0 {
        yazdir_metin("X86 DMA TABAN OK kb=");
        yazdir_tam((dt / 1024) olarak tam32);
        yazdir_satir();
    } değilse {
        yazdir_metin("X86 DMA TABAN YEDEK");
    }
    
```

Hmm, should I worry about `yazdir_metin` signature? Does it take a string literal directly? Since the file uses these helpers on other lines (not shown), let me assume `yazdir_metin("...")`.

The task says: "dt > 0 ise `X86 DMA TABAN OK kb=` + ayrı satırda `(dt / 1024) olarak tam32`; değilse `X86 DMA TABAN YEDEK`. (`yazdir_metin` zaten satır sonu ekler — arasına `yazdir_satir()` KOYMA; sayıdan sonra `yazdir_satir()` koy.)"

So:
- yazdir_metin("X86 DMA TABAN OK kb=") → outputs "X86 DMA TABAN OK kb=\n"
- yazdir_tam(dt/1024) → outputs "130816" (no newline?)
- yazdir_satir() → outputs "\n"

Hmm, so the label is on one line and the number on the next. Yes.

Wait, but the Makefile grep says `grep -qxF "X86 DMA TABAN OK kb="` — exact line match. So label must be alone on its line. yazdir_metin adds newline → label alone. Good.

Then `grep -A1 -xF "X86 DMA TABAN OK kb=" | tail -1` gets the next line = the number "130816". Yes.

Now, yazdir_tam signature: takes tam32? Task says `(dt / 1024) olarak tam32`. So yazdir_tam expects tam32 probably. OK.

But what about `değişken dt: tam64 = kem_platform_dma_tabani();` — writing at top of main's body might conflict with existing declaration of dt if there's one. Unlikely. The task chose `dt` so it should be new.

Now, the else case — should I output to else branch? Task says "değilse `X86 DMA TABAN YEDEK`". So yes.

Let me format:
```
    kem_platform_kur();
    değişken dt: tam64 = kem_platform_dma_tabani();
    eğer dt > 0 {
        yazdir_metin("X86 DMA TABAN OK kb=");
        yazdir_tam((dt / 1024) olarak tam32);
        yazdir_satir();
    } değilse {
        yazdir_metin("X86 DMA TABAN YEDEK");
    }

    // İş 3 (a): Blok DMA RAM yaz-oku (0..511; 251 ile döngüsel desen).
```

OK.

Now Makefile changes.

Change 3: insert after Kanıt 12 objdump line. The OLD block should be the anchor line itself. But actually, we could also use a bigger block. Let me just use the anchor line.

Actually the anchor is:
```
	@llvm-objdump -d --no-show-raw-insn --disassemble-symbols=pvh_entry $(BUILD)/kem_os_x86.elf | grep '%ebx' || { echo "FAIL: pvh_entry disasm'da %ebx movl'i yok"; exit 1; }
```

Note TAB at start. My block should preserve the TAB.

Hmm — the existing Kanıt 12 line ends with `exit 1; }` (no newline shown). Adding a Kanıt 13 needs to insert content after this line but before the next line.

Let me write the ESKI as that anchor line, and YENI as anchor line + new content.

Wait, but the anchor must appear exactly once. It's a specific objdump command, likely unique.

Let me write:
```
<<<<<<< ESKI
	@llvm-objdump -d --no-show-raw-insn --disassemble-symbols=pvh_entry $(BUILD)/kem_os_x86.elf | grep '%ebx' || { echo "FAIL: pvh_entry disasm'da %ebx movl'i yok"; exit 1; }
=======
	@llvm-objdump -d --no-show-raw-insn --disassemble-symbols=pvh_entry $(BUILD)/kem_os_x86.elf | grep '%ebx' || { echo "FAIL: pvh_entry disasm'da %ebx movl'i yok"; exit 1; }
	@echo "  Kanit 13: x86 DMA tabani PVH memmap'ten turetildi (yedek 32768 KB'den buyuk)..."
	@if ! grep -qxF "X86 DMA TABAN OK kb=" $(BUILD)/kem_os_x86.out; then \
		echo "FAIL: 'X86 DMA TABAN OK kb=' satiri yok"; exit 1; fi
	@v=`grep -A1 -xF "X86 DMA TABAN OK kb=" $(BUILD)/kem_os_x86.out | tail -1`; \
	if [ "$$v" -gt 32768 ]; then \
		echo "  DMA taban KB = $$v"; \
	else \
		echo "FAIL: DMA tabani yedek sabitten (32768 KB) buyuk degil: $$v"; exit 1; fi
>>>>>>> YENI
```

Now, need to think about the shell in recipe. `@v=\`...\`;` — a bit unusual but valid. Actually mixing backtick and `$$` in make: `$$v` → `$v`, backtick stays as backtick. In shell, `v=$(...)` is cleaner. Let me use `$(...)` instead:

Actually in a Makefile recipe, `$(...)` is Make variable expansion. To get shell `$(...)`, we need `$$(...)`. Using backtick is simpler — no escaping needed. But backticks work fine in dash.

Let me write:
```
	@v=`grep -A1 -xF "X86 DMA TABAN OK kb=" $(BUILD)/kem_os_x86.out | tail -1`; \
	if [ "$$v" -gt 32768 ]; then \
		echo "  DMA taban KB = $$v"; \
	else \
		echo "FAIL: ..."; exit 1; fi
```

Actually `$(BUILD)` on the grep line — Make will expand it. Good.

Also, in a recipe, `$$v` becomes `$v`. And the `\` at line end continues.

There's a subtlety: `[ "$$v" -gt 32768 ]` where `$$v` → `$v` in shell. In dash, `[ "$v" -gt 32768 ]` with empty v → error message but returns false. That's fine since we exit 1 in else. Actually, if v is empty, `[ "" -gt 32768 ]` prints error to stderr and returns 2 (not 1). With `-gt`, dash: "integer expression expected" and returns 2. So the `if` would be false, going to else. Fine. But the error message is noisy — acceptable.

To be safer, we could check first:
```
	@v=`grep -A1 -xF ... | tail -1`; \
	if [ -n "$$v" ] && [ "$$v" -gt 32768 ]; then ...
```

But task says just `[ "$$v" -gt 32768 ]`. Let me keep it simple but add `-n` for safety? Hmm, task says "ardından gelen satır `v` olarak alınıp `[ "$$v" -gt 32768 ]` olmalı". Let me do just that, but with a `[ -z ]` guard. Actually no — follow the spec exactly. Just `[ "$$v" -gt 32768 ]`.

Actually re-reading: "ardından gelen satır `v` olarak alınıp `[ "$$v" -gt 32768 ]` olmalı (yedek sabitten BÜYÜK = türetme gerçekten çalıştı); değeri yazdır; değilse FAIL."

So we take next line as v, check `[ "$$v" -gt 32768 ]`, print value, else FAIL. OK.

Change 4: Makefile hedef yorumu. Anchor:
`#          ayrıca pvh_entry disasm'ında %ebx saklama movl'i (PVH start_info ABI).`

Insert after this the Kanıt 13 description.

Looking at context (lines 1950-1956):
```
# Kanıt 12: QEMU seri çıktıda "X86 PVH OK ram_mb=" TAM satırı ve HEMEN ardındaki
#          satır pozitif bir sayı (PVH hvm_start_info RAM toplamı, kem_pvh.kem);
#          ayrıca pvh_entry disasm'ında %ebx saklama movl'i (PVH start_info ABI).
calistir_kem_os_x86: $(BUILD)/kemgu$(EXE) \
```

So after the pvh_entry disasm comment line, we insert Kanıt 13 description, before `calistir_kem_os_x86:`.

```
<<<<<<< ESKI
#          ayrıca pvh_entry disasm'ında %ebx saklama movl'i (PVH start_info ABI).
=======
#          ayrıca pvh_entry disasm'ında %ebx saklama movl'i (PVH start_info ABI).
# Kanıt 13: QEMU seri çıktıda "X86 DMA TABAN OK kb=" TAM satırı ve HEMEN ardındaki
#          satır yedek sabitten (32768 KB) BÜYÜK — PVH memmap RAM tepesinden
#          türetilen DMA tabanı (kem_platform.kem); değer yazdırılır.
>>>>>>> YENI
```

OK, that's it.

Let me double-check that I'm not touching forbidden files. Changes to:
- runtime/mimari/x86_64/kem_platform.kem ✓ (allowed by task)
- test/ornekler/kem_os_x86.kem ✓
- Makefile ✓

Forbidden: src/*.c, selfhost/*.kem, runtime/kem_gorev.kem, runtime/kem_mmu.kem, runtime/kem_zaman.kem. Not touching those. Good.

Now let me finalize the Turkish comments for kem_platform.kem.

Comment for the new functions:

```
// İş 10: x86 DMA tabanını PVH memmap'ten çalışma zamanında türet. Yedek SABİT
// değerler (vblk_dma=33554432/32 MB, vnet_dma=34603008) dosya başında kalır;
// PVH yoksa (microvm dışı ya da -kernel'siz) kem_platform_kur onlara dokunmaz.
//
// NEDEN RAM TEPESİ: kem_os heap'i ~17 MB'ta (__heap_end) biter; kanal/görev
// büyük tahsisleri bile toplamda ~MB mertebesinde → RAM tepesinin 256 KB altı
// heap'in ÇOK üstünde, çakışma yok; boot DMA bölgesini (vblk/vnet) daima
// __heap_end ÜSTÜNDE tutma kuralı böylece hâlâ sağlanır.
//
// NEDEN 1 GB SINIRI: start_x86_64.S yalnız ilk 1 GB'ı 2 MB huge page PD
// girdileriyle identity eşler (P|RW|PS) → 1073741824 üzerindeki adreslere
// erişim page fault. Bu yüzden her RAM parçasının `son`u 1 GB ile kırpılır.
//
// NEDEN 256 KB PAY: DMA tabanı RAM tepesine çok yakın olursa kernel .bss/heap
// ile çakışabilir; 256 KB emniyet payı bırakıp 4 KB aşağı hizala.
//
// Seçim: tür==1 (RAM) parçalar arasında kırpılmış `son` en büyük olanı.
// QEMU 128 MB'ta beklenen: 134217728 - 262144 = 133955584 (0x7FC0000, 130816 KB).
```

Hmm, but I should not be too verbose. Let me keep it substantial but focused.

Actually re-reading task: "Üstüne Türkçe yorum: neden RAM tepesi (heap ~17 MB'ta biter, çakışmaz), neden 1 GB sınırı (x86 boot yalnız ilk 1 GB'ı eşler), PVH yoksa sabit yedek."

Good. Let me make sure comments are consistent with the codebase style (long Turkish comments with D-xxx references). Actually the task doesn't specify a D-xxx code. I'll keep it generic "İş 10".

Let me finalize.

One thing to check: in kem_platform.kem, `vblk_dma` and `vnet_dma` are declared as `küresel değişken`. Are they mutable from a `çıplak işlev`? In kem_heap.kem, `küresel değişken kem_bump: tam64 = 0;` is written from `çıplak işlev kem_heap_kur`. Yes.

OK now let me also verify: does `kdl_mmio_oku32` return `tam32` and can we compare to 862897528? Yes, 862897528 < 2^31? 862897528 ≈ 862 million, 2^31 = 2147483648. So it fits in i32 positive. Good. Comparison `!=` works.

Version check: `kdl_mmio_oku32(b + 4) < 1` → return 0. Good.

Let me finalize the blocks.

Wait, one more consideration: The overlay for kem_platform.kem — the task says "`sabit VM_YUVA_SAYISI: tam32 = 24;` satırının ALTINA". So I should anchor on that line, not on the two-line block. Let me anchor just on `sabit VM_YUVA_SAYISI: tam32 = 24;` to follow the task. But is that line unique? There could be a similar line — but the task explicitly identifies it as unique in the file context. Let me use the two-line context for safety:

Actually to follow the task exactly, anchor on the single line. But we can make the block slightly larger to ensure uniqueness. Let me use:
```
sabit VM_TARAMA_TABAN: tam64 = 4272947200;  // 0xFEB00000 (QEMU microvm)
sabit VM_YUVA_SAYISI: tam32 = 24;
```

That's safe. The task says "satırının ALTINA" (below the line) — using the two-line block still inserts below `sabit VM_YUVA_SAYISI`. Fine.

Final answer.

Let me write the blocks now.
