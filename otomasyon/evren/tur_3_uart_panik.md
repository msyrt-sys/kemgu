# Tur 3 — uart_panik — `deepseek-v4.1-flash`

- Tarih: 2026-10-12 06:16:17 · süre 97.1 sn · finish `stop` · reasoning_effort `low`
- usage: `{"prompt_tokens": 17231, "total_tokens": 31428, "completion_tokens": 14197, "prompt_tokens_details": {"cached_tokens": 10624, "created_cache_tokens": 6592}, "completion_tokens_details": {"reasoning_tokens": 10332}}`
- Değişiklik blokları: 14 — yeni:runtime/mimari/arm64/kem_uart.kem, yeni:runtime/mimari/x86_64/kem_uart.kem, duzenle:runtime/kem_heap.kem, duzenle:runtime/kem_heap.kem, duzenle:test/ornekler/kem_os_x86.kem, duzenle:test/ornekler/kem_os_x86.kem, duzenle:Makefile, duzenle:Makefile, duzenle:Makefile, duzenle:Makefile, duzenle:test/baremetal_diff_harness.sh, duzenle:test/check_kapisi.sh, duzenle:test/check_kapisi.sh, duzenle:test/check_kapisi.sh
- İstem: [tur_3_uart_panik_istem.md](tur_3_uart_panik_istem.md)

## Yanıt

```yeni:runtime/mimari/arm64/kem_uart.kem
// kem_uart.kem — İş 2 (D-672, D2b): ARM64 (PL011) UART0 bayt-yaz arayüzü.
// kem_panik artık mimariden bağımsız kem_uart_bayt çağırır; bu dosya arm64'te
// PL011 UART0 @ 0x09000000 (QEMU virt) yolunu sağlar. x86 ikizi:
// runtime/mimari/x86_64/kem_uart.kem (COM1 16550 port I/O). Ayrı dosya: AS001
// gereği bir dosyada yalnız TEK mimarinin satıriçi_asm'i olabilir; Makefile CAT
// listesi mimariye göre seçer (D2b kararı). Asm yok — kdl_mmio_yaz32
// (kem_heap.kem; aynı CAT birimi) zaten çıplak volatile store emit eder.
çıplak işlev kem_uart_bayt(b: tam32) {
    kdl_mmio_yaz32(150994944, b);
}
```

```yeni:runtime/mimari/x86_64/kem_uart.kem
// kem_uart.kem — İş 2 (D-672, D2b): x86_64 (COM1 16550) UART bayt-yaz arayüzü.
// kem_panik artık mimariden bağımsız kem_uart_bayt çağırır; bu dosya x86'da
// COM1 16550 port I/O yolunu sağlar: veri 0x3F8 (=1016), LSR 0x3FD (=1021),
// LSR bit 5 (32) = THRE (verici tutma yazmacı boş). arm64 ikizi:
// runtime/mimari/arm64/kem_uart.kem (PL011 MMIO). Ayrı dosya: AS001 gereği bir
// dosyada yalnız TEK mimarinin satıriçi_asm'i olabilir; Makefile CAT listesi
// mimariye göre seçer (D2b kararı). LSR yoklaması SINIRLI: UART yoksa (ya da
// THRE donmuşsa) panik yolu ASILMASIN diye en çok 100000 deneme; sonra veri
// yine yazılır (mesaj kaybolabilir ama çekirdek ilerler).
çıplak işlev kem_port_yaz8(port: dtam16, deger: dtam8) {
    güvensiz {
        satıriçi_asm { mimari: x86_64 şablon: r#"outb $1, $0"# girdi("{dx}", port) girdi("{al}", deger) bozulan("~{memory}") }
    }
}

çıplak işlev kem_port_oku8(port: dtam16) -> dtam8 {
    değişken v: dtam8 = 0 olarak dtam8;
    güvensiz {
        satıriçi_asm { mimari: x86_64 şablon: r#"inb $1, $0"# çıktı("={al}", &v) girdi("{dx}", port) bozulan("~{memory}") }
    }
    ver v;
}

çıplak işlev kem_uart_bayt(b: tam32) {
    değişken deneme: tam32 = 0;
    iken deneme < 100000 {
        değişken lsr: tam32 = kem_port_oku8(1021) olarak tam32;
        eğer (lsr & 32) != 0 { ver; }
        deneme = deneme + 1;
    }
    kem_port_yaz8(1016, b olarak dtam8);
}
```

```duzenle:runtime/kem_heap.kem
<<<<<<< ESKI
// Çekirdek paniği: PL011'e (0x09000000) mesaj bas ve DUR. kdl_panik kem_os'ta YOK
// (D-592); çıplak bağlamda yalnız çıplak çağrılabilir (E013) → doğrudan MMIO.
// Kanal (D-623) ve görev (D-624) ortak kullanır.
=======
// Çekirdek paniği: konsol artık mimari başına kem_uart_bayt
// (runtime/mimari/<arch>/kem_uart.kem). arm64'te yine PL011 MMIO; x86'da COM1
// 16550 port I/O. kdl_panik kem_os'ta YOK (D-592); çıplak bağlamda yalnız
// çıplak çağrılabilir (E013) → doğrudan MMIO. Kanal (D-623) ve görev (D-624)
// ortak kullanır.
>>>>>>> YENI
```

```duzenle:runtime/kem_heap.kem
<<<<<<< ESKI
        kdl_mmio_yaz32(150994944, metin_bayt(m, i) olarak tam32);
        i = i + 1;
    }
    kdl_mmio_yaz32(150994944, 10);
=======
        kem_uart_bayt(metin_bayt(m, i) olarak tam32);
        i = i + 1;
    }
    kem_uart_bayt(10);
>>>>>>> YENI
```

```duzenle:test/ornekler/kem_os_x86.kem
<<<<<<< ESKI
// Ayrıca main başında kem_dma_bariyeri() çağrılır ve "X86 DMA BARIYER OK"
// satırı basılır (gerçekten çalıştığının kanıtı; #UD olsaydı çekirdek düşerdi).
=======
// Ayrıca main başında kem_dma_bariyeri() çağrılır ve "X86 DMA BARIYER OK"
// satırı basılır (gerçekten çalıştığının kanıtı; #UD olsaydı çekirdek düşerdi).
// main sonunda kem_panik("X86 PANIK YOLU OK") çağrılır: panik geri dönmez,
// mesaj .kem COM1 yolundan basılır, çekirdek orada durur; QEMU zaman aşımı keser.
>>>>>>> YENI
```

```duzenle:test/ornekler/kem_os_x86.kem
<<<<<<< ESKI
    yazdir_tam(toplam);
    yazdir_satir();
    ver 0;
}
=======
    yazdir_tam(toplam);
    yazdir_satir();

    kem_panik("X86 PANIK YOLU OK");
    ver 0;
}
>>>>>>> YENI
```

```duzenle:Makefile
<<<<<<< ESKI
	@cat runtime/kem_heap.kem runtime/kem_mmu.kem runtime/kem_gorev.kem runtime/kem_zaman.kem runtime/mimari/arm64/kem_bariyer.kem runtime/kem_virtio_blk.kem runtime/kem_minifs.kem runtime/kem_virtio_net.kem runtime/kem_elf.kem runtime/kem_dtb.kem test/ornekler/kem_os.kem > $(BUILD)/kem_os_comb.kem
=======
	@cat runtime/kem_heap.kem runtime/mimari/arm64/kem_uart.kem runtime/kem_mmu.kem runtime/kem_gorev.kem runtime/kem_zaman.kem runtime/mimari/arm64/kem_bariyer.kem runtime/kem_virtio_blk.kem runtime/kem_minifs.kem runtime/kem_virtio_net.kem runtime/kem_elf.kem runtime/kem_dtb.kem test/ornekler/kem_os.kem > $(BUILD)/kem_os_comb.kem
>>>>>>> YENI
```

```duzenle:Makefile
<<<<<<< ESKI
	cat runtime/kem_heap.kem runtime/mimari/x86_64/kem_bariyer.kem runtime/kem_virtio_blk.kem test/ornekler/kem_os_x86.kem > $(BUILD)/kem_os_x86_comb.kem
=======
	cat runtime/kem_heap.kem runtime/mimari/x86_64/kem_uart.kem runtime/mimari/x86_64/kem_bariyer.kem runtime/kem_virtio_blk.kem test/ornekler/kem_os_x86.kem > $(BUILD)/kem_os_x86_comb.kem
>>>>>>> YENI
```

```duzenle:Makefile
<<<<<<< ESKI
# Kanıt 4: objdump'da mfence (x86_64 DMA bariyeri) satırları; dsb YOK.
=======
# Kanıt 4: objdump'da mfence (x86_64 DMA bariyeri) satırları; dsb YOK.
# Kanıt 5: QEMU seri çıktıda "X86 PANIK YOLU OK" tam satırı (kem_panik .kem
# UART yolundan basar; panik geri dönmez, QEMU zaman aşımı keser).
# Kanıt 6: objdump --disassemble-symbols=kem_uart_bayt içinde outb VE inb
# (yalnız .kem işlevi; tüm ikilide aramak C UART nesnesine de uyar).
>>>>>>> YENI
```

```duzenle:Makefile
<<<<<<< ESKI
		echo "FAIL: dsb x86_64 binary'de bulundu"; exit 1; fi
=======
		echo "FAIL: dsb x86_64 binary'de bulundu"; exit 1; fi
	@echo "  Kanit 5: QEMU davranis (X86 PANIK YOLU OK)..."
	@if ! grep -qxF "X86 PANIK YOLU OK" $(BUILD)/kem_os_x86.out; then \
		echo "FAIL: 'X86 PANIK YOLU OK' satiri yok"; exit 1; fi
	@echo "  Kanit 6: objdump kem_uart_bayt (outb + inb, .kem UART)..."
	@if ! llvm-objdump -d --no-show-raw-insn --disassemble-symbols=kem_uart_bayt $(BUILD)/kem_os_x86.elf | grep -q 'outb'; then \
		echo "FAIL: kem_uart_bayt icinde outb yok"; exit 1; fi
	@if ! llvm-objdump -d --no-show-raw-insn --disassemble-symbols=kem_uart_bayt $(BUILD)/kem_os_x86.elf | grep -q 'inb'; then \
		echo "FAIL: kem_uart_bayt icinde inb yok"; exit 1; fi
	@llvm-objdump -d --no-show-raw-insn --disassemble-symbols=kem_uart_bayt $(BUILD)/kem_os_x86.elf | grep -E 'outb|inb'
>>>>>>> YENI
```

```duzenle:test/baremetal_diff_harness.sh
<<<<<<< ESKI
OS_PARCA="runtime/kem_heap.kem runtime/kem_mmu.kem runtime/kem_gorev.kem \
runtime/kem_zaman.kem runtime/mimari/arm64/kem_bariyer.kem runtime/kem_virtio_blk.kem runtime/kem_minifs.kem \
=======
OS_PARCA="runtime/kem_heap.kem runtime/mimari/arm64/kem_uart.kem runtime/kem_mmu.kem runtime/kem_gorev.kem \
runtime/kem_zaman.kem runtime/mimari/arm64/kem_bariyer.kem runtime/kem_virtio_blk.kem runtime/kem_minifs.kem \
>>>>>>> YENI
```

```duzenle:test/check_kapisi.sh
<<<<<<< ESKI
KEMOS_PARCALAR="runtime/kem_heap.kem runtime/kem_mmu.kem runtime/kem_gorev.kem
                runtime/kem_zaman.kem runtime/mimari/arm64/kem_bariyer.kem runtime/kem_virtio_blk.kem
=======
KEMOS_PARCALAR="runtime/kem_heap.kem runtime/mimari/arm64/kem_uart.kem runtime/kem_mmu.kem runtime/kem_gorev.kem
                runtime/kem_zaman.kem runtime/mimari/arm64/kem_bariyer.kem runtime/kem_virtio_blk.kem
>>>>>>> YENI
```

```duzenle:test/check_kapisi.sh
<<<<<<< ESKI
KEMOS_X86_PARCALAR="runtime/kem_heap.kem runtime/mimari/x86_64/kem_bariyer.kem
                    runtime/kem_virtio_blk.kem test/ornekler/kem_os_x86.kem"
=======
KEMOS_X86_PARCALAR="runtime/kem_heap.kem runtime/mimari/x86_64/kem_uart.kem runtime/mimari/x86_64/kem_bariyer.kem
                    runtime/kem_virtio_blk.kem test/ornekler/kem_os_x86.kem"
>>>>>>> YENI
```

```duzenle:test/check_kapisi.sh
<<<<<<< ESKI
    echo "  ✅ kem_os_x86 (birleşik kaynak, 4 parça, --mimari x86_64) --check geçti"
=======
    echo "  ✅ kem_os_x86 (birleşik kaynak, 5 parça, --mimari x86_64) --check geçti"
>>>>>>> YENI
```

Yapılan değişiklikler özeti:
- `runtime/mimari/arm64/kem_uart.kem` ve `runtime/mimari/x86_64/kem_uart.kem` eklendi; ortak arayüz `çıplak işlev kem_uart_bayt(b: tam32)`.
- `kem_panik` artık `kem_uart_bayt` çağırıyor; konsol mimariye göre seçiliyor.
- Tüm CAT listelerine mimarinin UART dosyası `runtime/kem_heap.kem`'in hemen ardına eklendi.
- `kem_os_x86.kem` sonunda `kem_panik("X86 PANIK YOLU OK")` çağrısı ve başlık yorumu eklendi.
- `Makefile` x86 hedefinde Kanıt 5 (QEMU çıktısında tam satır) ve Kanıt 6 (`--disassemble-symbols=kem_uart_bayt` içinde `outb`+`inb`) eklendi; hedef başlığına Kanıt 4/5/6 notu düşüldü.

VARSAYIMLAR:
- `Makefile`, `test/baremetal_diff_harness.sh` ve `test/check_kapisi.sh` içindeki ilgili satırların bağlamda verilen kesitlerle birebir aynı olduğu varsayıldı.
- `dtam8`/`dtam16` tiplerinin KEMGU'da `i8`/`i16` C-ABI tipleri olduğu ve `{al}`/`{dx}` kısıtlarıyla boyut uyumunun derleyici tarafından denetlendiği varsayıldı.
- `llvm-objdump --disassemble-symbols=kem_uart_bayt` işlevin `-O2` altında satır içine alınmadığını varsayar; alınırsa Kanıt 6 `inb`/`outb` bulamayabilir (KEMGU çıplak işlevlerinde beklenen davranış: ayrı sembol).
- x86 `outb`/`inb` AT&T şablonlarının `$0`/`$1` operand sırasının verilen örneklerle aynı olduğu varsayıldı.

## Düşünme (reasoning)

We need produce edit blocks. Let's carefully understand requirements and produce exact edits.

Files:
1. New runtime/mimari/arm64/kem_uart.kem
2. New runtime/mimari/x86_64/kem_uart.kem
3. Edit runtime/kem_heap.kem kem_panik
4-6 edits to Makefile, baremetal_diff_harness.sh, check_kapisi.sh, test/ornekler/kem_os_x86.kem
7. Makefile additions Kanıt 5/6.

We only have access to context; anchors specified. Let's build.

For new files use `yeni:` blocks. For edits use `duzenle:`.

Careful with KEMGU syntax: `dtam16`, `dtam8`, `tam32`, `tam64`. `satıriçi_asm { mimari: x86_64 şablon: r#"outb $1, $0"# girdi("{dx}", port) girdi("{al}", deger) bozulan("~{memory}") }`.

Note pattern from kem_bariyer: `satıriçi_asm { mimari: x86_64 şablon: r#"mfence"# bozulan("~{memory}") }`. So aqui.

`kem_port_oku8`: `çıktı("={al}", &v) girdi("{dx}", port)`. Need variable v: dtam8.

kem_uart_bayt: loop up to 100000, check THRE bit.

Need `güvensiz { }` wrapper.

Let's write arm64 file:

```
// kem_uart.kem — İş 2 (D-672, D2b): ARM64 (PL011) UART0 bayt-yaz arayüzü.
// kem_panik artık mimariden bağımsız olarak kem_uart_bayt çağırır; bu dosya
// arm64'te PL011 UART0 @ 0x09000000 (QEMU virt) yolunu sağlar. x86 ikizi:
// runtime/mimari/x86_64/kem_uart.kem (COM1 16550 port I/O). Ayrı dosya: AS001
// gereği bir dosyada yalnız TEK mimarinin satıriçi_asm'i olabilir; Makefile CAT
// listesi mimariye göre seçer (D2b kararı). Asm yok — kdl_mmio_yaz32 (kem_heap.kem,
// aynı birim) zaten çıplak volatile store emit eder.
çıplak işlev kem_uart_bayt(b: tam32) {
    kdl_mmio_yaz32(150994944, b);
}
```

Wait 0x09000000 decimal = 150994944. Yes.

x86_64 file:

```
// kem_uart.kem — İş 2 (D-672, D2b): x86_64 (COM1 16550) UART bayt-yaz arayüzü.
// kem_panik artık mimariden bağımsız olarak kem_uart_bayt çağırır; bu dosya
// x86'da COM1 16550 port I/O yolunu sağlar: veri 0x3F8 (=1016), LSR 0x3FD (=1021),
// LSR bit 5 (32) = THRE (verici tutma yazmacı boş). arm64 ikizi:
// runtime/mimari/arm64/kem_uart.kem (PL011 MMIO). Ayrı dosya: AS001 gereği bir
// dosyada yalnız TEK mimarinin satıriçi_asm'i olabilir; Makefile CAT listesi
// mimariye göre seçer (D2b kararı). LSR yoklaması SINIRLI: QEMU'da UART yoksa
// (ya da THRE donmuşsa) panik yolu ARM64_DENEME'de asılı kalmasın diye en çok
// 100000 deneme; sonra yine de yaz (panik mesajı kaybolabilir ama çekirdek döner).
çıplak işlev kem_port_yaz8(port: dtam16, deger: dtam8) {
    güvensiz {
        satıriçi_asm { mimari: x86_64 şablon: r#"outb $1, $0"# girdi("{dx}", port) girdi("{al}", deger) bozulan("~{memory}") }
    }
}

çıplak işlev kem_port_oku8(port: dtam16) -> dtam8 {
    değişken v: dtam8 = 0 olarak dtam8;
    güvensiz {
        satıriçi_asm { mimari: x86_64 şablon: r#"inb $1, $0"# çıktı("={al}", &v) girdi("{dx}", port) bozulan("~{memory}") }
    }
    ver v;
}

çıplak işlev kem_uart_bayt(b: tam32) {
    değişken deneme: tam32 = 0;
    iken deneme < 100000 {
        değişken lsr: tam32 = kem_port_oku8(1021) olarak tam32;
        eğer (lsr & 32) != 0 { ver; }
        deneme = deneme + 1;
    }
    kem_port_yaz8(1016, b olarak dtam8);
}
```

Wait, if the loop times out we should "sonra yine yaz" — meaning still write. But my code writes after loop either way? Actually if THRE set, we `ver` inside loop. If timeout, falls through to write. Good. But if THRE not set within 100000, we still write once. Wait my code: inside loop if set -> ver; else increments. After loop, writes. Good.

But careful: after loop, if never set, we write. Good.

However spec: "en çok 100000 deneme, sonra yine yaz". Yes.

One issue: `dtam8 = 0 olarak dtam8` — is that valid? In kem_heap patterns: `*pip = 0 olarak dtam8;` yes.

Now edit kem_heap.kem kem_panik. Anchor text for comment and two lines.

Current:
```
// Çekirdek paniği: PL011'e (0x09000000) mesaj bas ve DUR. kdl_panik kem_os'ta YOK
// (D-592); çıplak bağlamda yalnız çıplak çağrılabilir (E013) → doğrudan MMIO.
// Kanal (D-623) ve görev (D-624) ortak kullanır.
çıplak işlev kem_panik(m: metin) {
    değişken n: tam32 = metin_uzunluk(m);
    değişken i: tam32 = 0;
    iken i < n {
        kdl_mmio_yaz32(150994944, metin_bayt(m, i) olarak tam32);
        i = i + 1;
    }
    kdl_mmio_yaz32(150994944, 10);
    iken doğru { }
}
```

We need modify comment and two lines. Let's do full function edit or separate edits. Anchor for comment: `// Çekirdek paniği: PL011'e (0x09000000) mesaj bas ve DUR. kdl_panik kem_os'ta YOK`. We can replace comment part and the two lines separately.

Better use one edit covering comment+function? The instruction says each anchor appears once. Let's produce edits.

Edit 1 for comment lines:
ESKI:
```
// Çekirdek paniği: PL011'e (0x09000000) mesaj bas ve DUR. kdl_panik kem_os'ta YOK
// (D-592); çıplak bağlamda yalnız çıplak çağrılabilir (E013) → doğrudan MMIO.
// Kanal (D-623) ve görev (D-624) ortak kullanır.
```
YENI:
```
// Çekirdek paniği: konsol artık mimari başına kem_uart_bayt
// (runtime/mimari/<arch>/kem_uart.kem). arm64'te yine PL011 MMIO; x86'da COM1
// 16550 port I/O. kem_uart_bayt çıplak (E013 uyumlu). kdl_panik kem_os'ta YOK
// (D-592); çıplak bağlamda yalnız çıplak çağrılabilir (E013) → doğrudan MMIO.
// Kanal (D-623) ve görev (D-624) ortak kullanır.
```

Edit 2 for body lines:
ESKI:
```
        kdl_mmio_yaz32(150994944, metin_bayt(m, i) olarak tam32);
        i = i + 1;
    }
    kdl_mmio_yaz32(150994944, 10);
```
YENI:
```
        kem_uart_bayt(metin_bayt(m, i) olarak tam32);
        i = i + 1;
    }
    kem_uart_bayt(10);
```

That covers both anchors.

Now test/ornekler/kem_os_x86.kem: add kem_panik before ver 0. And update header comment. Anchor for header maybe we edit near top? The instruction says "Başlık yorumuna bunu ekle." We need anchor for the comment. Let's look at top of kem_os_x86.kem comment. We could edit the last paragraph of the header or add a line. Simpler: edit near top by replacing a specific comment line. Anchor: `// Ayrıca main başında kem_dma_bariyeri() çağrılır ve "X86 DMA BARIYER OK"` This line appears once? Yes. We could append after that sentence? Actually that line ends with `"X86 DMA BARIYER OK"` and next line is `// satırı basılır (gerçekten çalıştığının kanıtı; #UD olsaydı çekirdek düşerdi).`

We could replace two lines to insert new info. Let's replace:
ESKI:
```
// Ayrıca main başında kem_dma_bariyeri() çağrılır ve "X86 DMA BARIYER OK"
// satırı basılır (gerçekten çalıştığının kanıtı; #UD olsaydı çekirdek düşerdi).
```
YENI:
```
// Ayrıca main başında kem_dma_bariyeri() çağrılır ve "X86 DMA BARIYER OK"
// satırı basılır (gerçekten çalıştığının kanıtı; #UD olsaydı çekirdek düşerdi).
// main sonunda kem_panik("X86 PANIK YOLU OK") çağrılır: panik geri dönmez,
// mesaj .kem COM1 yolundan basılır, çekirdek orada durur; QEMU zaman aşımı keser.
```

And edit ver 0 part:
ESKI:
```
    yazdir_tam(toplam);
    yazdir_satir();
    ver 0;
}
```
YENI:
```
    yazdir_tam(toplam);
    yazdir_satir();

    kem_panik("X86 PANIK YOLU OK");
    ver 0;
}
```

Wait the anchor includes `}` — need ensure `}` appearing after ver 0 is unique in context. The anchor is the last 4 lines of the file. Should be unique.

Now Makefile edits. We don't have actual Makefile, only snippets given in context files (makefile_kem_os_arm_cat.txt, makefile_kem_os_x86_kesit.txt). But task says edit `Makefile`. The anchors are given. We produce duzenle blocks for Makefile.

Anchor for arm CAT: `@cat runtime/kem_heap.kem runtime/kem_mmu.kem`. But the full line is:
`	@cat runtime/kem_heap.kem runtime/kem_mmu.kem runtime/kem_gorev.kem runtime/kem_zaman.kem runtime/mimari/arm64/kem_bariyer.kem runtime/kem_virtio_blk.kem runtime/kem_minifs.kem runtime/kem_virtio_net.kem runtime/kem_elf.kem runtime/kem_dtb.kem test/ornekler/kem_os.kem > $(BUILD)/kem_os_comb.kem`

We need insert `runtime/mimari/arm64/kem_uart.kem` after kem_heap.kem. So:
ESKI: `@cat runtime/kem_heap.kem runtime/kem_mmu.kem runtime/kem_gorev.kem ...`
YENI: `@cat runtime/kem_heap.kem runtime/mimari/arm64/kem_uart.kem runtime/kem_mmu.kem runtime/kem_gorev.kem ...`

The anchor given is a prefix. For edit we need exact ESKI text that occurs once. We could use the prefix but the block must match a complete chunk. Since anchor includes tab? The context file line begins with a tab. The anchor as given: `@cat runtime/kem_heap.kem runtime/kem_mmu.kem`. Hmm. The block should be exact text. Let's use the whole line including leading tab.

ESKI:
```
	@cat runtime/kem_heap.kem runtime/kem_mmu.kem runtime/kem_gorev.kem runtime/kem_zaman.kem runtime/mimari/arm64/kem_bariyer.kem runtime/kem_virtio_blk.kem runtime/kem_minifs.kem runtime/kem_virtio_net.kem runtime/kem_elf.kem runtime/kem_dtb.kem test/ornekler/kem_os.kem > $(BUILD)/kem_os_comb.kem
```
YENI:
```
	@cat runtime/kem_heap.kem runtime/mimari/arm64/kem_uart.kem runtime/kem_mmu.kem runtime/kem_gorev.kem runtime/kem_zaman.kem runtime/mimari/arm64/kem_bariyer.kem runtime/kem_virtio_blk.kem runtime/kem_minifs.kem runtime/kem_virtio_net.kem runtime/kem_elf.kem runtime/kem_dtb.kem test/ornekler/kem_os.kem > $(BUILD)/kem_os_comb.kem
```

But wait, is `runtime/kem_heap.kem runtime/kem_mmu.kem` unique? There's only one such line in Makefile per context. Good.

x86 CAT anchor:
`	cat runtime/kem_heap.kem runtime/mimari/x86_64/kem_bariyer.kem runtime/kem_virtio_blk.kem test/ornekler/kem_os_x86.kem > $(BUILD)/kem_os_x86_comb.kem`
We insert uart after kem_heap. Leading tab(s)? Context shows `\tcat`? Actually context file shows:
```
	cat runtime/kem_heap.kem runtime/mimari/x86_64/kem_bariyer.kem runtime/kem_virtio_blk.kem test/ornekler/kem_os_x86.kem > $(BUILD)/kem_os_x86_comb.kem
```
It begins with a tab. Given anchor in task: `	cat runtime/kem_heap.kem runtime/mimari/x86_64/kem_bariyer.kem runtime/kem_virtio_blk.kem test/ornekler/kem_os_x86.kem > $(BUILD)/kem_os_x86_comb.kem` (starts with tab). Good.

ESKI:
```
	cat runtime/kem_heap.kem runtime/mimari/x86_64/kem_bariyer.kem runtime/kem_virtio_blk.kem test/ornekler/kem_os_x86.kem > $(BUILD)/kem_os_x86_comb.kem
```
YENI:
```
	cat runtime/kem_heap.kem runtime/mimari/x86_64/kem_uart.kem runtime/mimari/x86_64/kem_bariyer.kem runtime/kem_virtio_blk.kem test/ornekler/kem_os_x86.kem > $(BUILD)/kem_os_x86_comb.kem
```

Now Makefile header comment anchor:
`# Kanıt 4: objdump'da mfence (x86_64 DMA bariyeri) satırları; dsb YOK.`
We add lines after it. Let's use that line as ESKI and append.

ESKI:
```
# Kanıt 4: objdump'da mfence (x86_64 DMA bariyeri) satırları; dsb YOK.
```
Wait this line is in the header comment block of the target. Let me check the context snippet: The comment block is:
```
# ...
# Kanıt 4: objdump'da mfence (x86_64 DMA bariyeri) satırları; dsb YOK.
calistir_kem_os_x86: ...
```
So line after is blank? Actually context:
```
# Kanıt 4: objdump'da mfence (x86_64 DMA bariyeri) satırları; dsb YOK.
calistir_kem_os_x86: $(BUILD)/kemgu$(EXE) \
```
Yes. So we can add lines before calistir target.

ESKI:
```
# Kanıt 4: objdump'da mfence (x86_64 DMA bariyeri) satırları; dsb YOK.
calistir_kem_os_x86: $(BUILD)/kemgu$(EXE) \
```
YENI:
```
# Kanıt 4: objdump'da mfence (x86_64 DMA bariyeri) satırları; dsb YOK.
# Kanıt 5: QEMU seri çıktıda "X86 PANIK YOLU OK" tam satırı (kem_panik .kem
# UART yolundan basar; panik geri dönmez, QEMU zaman aşımı keser).
# Kanıt 6: objdump --disassemble-symbols=kem_uart_bayt içinde outb VE inb
# (yalnız .kem işlevi; tüm ikilide aramak C UART nesnesine de uyar).
calistir_kem_os_x86: $(BUILD)/kemgu$(EXE) \
```

Now add Kanıt 5/6 shell commands after the dsb check. Anchor given:
`		echo "FAIL: dsb x86_64 binary'de bulundu"; exit 1; fi` (başta İKİ TAB)

Let's locate in context. The end of the x86 target:
```
	@# Komut SÜTUNUNDA ara (sekme + dsb): düz 'dsb' `<vblk_dsb>` sembol adıyla eşleşir.
	@if llvm-objdump -d --no-show-raw-insn $(BUILD)/kem_os_x86.elf | grep -qP '\tdsb\b'; then \
		echo "FAIL: dsb x86_64 binary'de bulundu"; exit 1; fi
```
Then end of snippet. The anchor is the line with two tabs before `echo`. Actually the line is `\t\techo "FAIL: dsb x86_64 binary'de bulundu"; exit 1; fi`. Two tabs? Context shows:
```
	@if llvm-objdump ... ; then \
		echo "FAIL: dsb x86_64 binary'de bulundu"; exit 1; fi
```
The continuation line has two tabs? In Makefile, a `\` continuation line typically indented with tabs. The context snippet shows two tabs before `echo`. Yes anchor says "başta İKİ TAB".

We need to append after the `fi` line. So ESKI:
```
		echo "FAIL: dsb x86_64 binary'de bulundu"; exit 1; fi
```
YENI:
```
		echo "FAIL: dsb x86_64 binary'de bulundu"; exit 1; fi
	@echo "  Kanit 5: QEMU davranis (X86 PANIK YOLU OK)..."
	@if ! grep -qxF "X86 PANIK YOLU OK" $(BUILD)/kem_os_x86.out; then \
		echo "FAIL: 'X86 PANIK YOLU OK' satiri yok"; exit 1; fi
	@echo "  Kanit 6: objdump kem_uart_bayt (outb + inb, .kem UART)..."
	@if ! llvm-objdump -d --no-show-raw-insn --disassemble-symbols=kem_uart_bayt $(BUILD)/kem_os_x86.elf | grep -q 'outb'; then \
		echo "FAIL: kem_uart_bayt icinde outb yok"; exit 1; fi
	@if ! llvm-objdump -d --no-show-raw-insn --disassemble-symbols=kem_uart_bayt $(BUILD)/kem_os_x86.elf | grep -q 'inb'; then \
		echo "FAIL: kem_uart_bayt icinde inb yok"; exit 1; fi
	@llvm-objdump -d --no-show-raw-insn --disassemble-symbols=kem_uart_bayt $(BUILD)/kem_os_x86.elf | grep -E 'outb|inb'
```

But careful with Makefile `@if ... then \` continuation: line ends with `\`. Good.

However, requirement: "ikisini de OLDUKLARI GİBİ yazdır". So after the checks, print. We use grep -E.

Now, the Kanıt 5 check uses $(BUILD)/kem_os_x86.out; it's created earlier in the QEMU run. Good.

But wait, the QEMU run uses `timeout 8` and `|| true`. kem_panik loops forever, so QEMU will time out after 8 seconds, output captured. Output will contain "X86 PANIK YOLU OK" line. Good.

Now, `.out` might already have been removed and rewrote with `rm -f` earlier. Fine.

Now check_kapisi.sh edits.

KEMOS_PARCALAR:
Context:
```
KEMOS_PARCALAR="runtime/kem_heap.kem runtime/kem_mmu.kem runtime/kem_gorev.kem
                runtime/kem_zaman.kem runtime/mimari/arm64/kem_bariyer.kem runtime/kem_virtio_blk.kem
                runtime/kem_minifs.kem runtime/kem_virtio_net.kem
                runtime/kem_elf.kem runtime/kem_dtb.kem test/ornekler/kem_os.kem"
```
Insert uart after kem_heap.kem. Anchor: `KEMOS_PARCALAR="runtime/kem_heap.kem runtime/kem_mmu.kem`. But edit must match whole chunk. We can replace first two lines chunk.

ESKI:
```
KEMOS_PARCALAR="runtime/kem_heap.kem runtime/kem_mmu.kem runtime/kem_gorev.kem
                runtime/kem_zaman.kem runtime/mimari/arm64/kem_bariyer.kem runtime/kem_virtio_blk.kem
```
YENI:
```
KEMOS_PARCALAR="runtime/kem_heap.kem runtime/mimari/arm64/kem_uart.kem runtime/kem_mmu.kem runtime/kem_gorev.kem
                runtime/kem_zaman.kem runtime/mimari/arm64/kem_bariyer.kem runtime/kem_virtio_blk.kem
```

KEMOS_X86_PARCALAR:
```
KEMOS_X86_PARCALAR="runtime/kem_heap.kem runtime/mimari/x86_64/kem_bariyer.kem
                    runtime/kem_virtio_blk.kem test/ornekler/kem_os_x86.kem"
```
Insert uart after kem_heap.

ESKI:
```
KEMOS_X86_PARCALAR="runtime/kem_heap.kem runtime/mimari/x86_64/kem_bariyer.kem
                    runtime/kem_virtio_blk.kem test/ornekler/kem_os_x86.kem"
```
YENI:
```
KEMOS_X86_PARCALAR="runtime/kem_heap.kem runtime/mimari/x86_64/kem_uart.kem runtime/mimari/x86_64/kem_bariyer.kem
                    runtime/kem_virtio_blk.kem test/ornekler/kem_os_x86.kem"
```

Message: `kem_os_x86 (birleşik kaynak, 4 parça, --mimari x86_64)` → `5 parça`.

ESKI: `  ✅ kem_os_x86 (birleşik kaynak, 4 parça, --mimari x86_64) --check geçti`
YENI: `  ✅ kem_os_x86 (birleşik kaynak, 5 parça, --mimari x86_64) --check geçti`

But anchor only gives `kem_os_x86 (birleşik kaynak, 4 parça, --mimari x86_64)`. Ensure exact.

Now baremetal_diff_harness.sh OS_PARCA:
```
OS_PARCA="runtime/kem_heap.kem runtime/kem_mmu.kem runtime/kem_gorev.kem \
runtime/kem_zaman.kem runtime/mimari/arm64/kem_bariyer.kem runtime/kem_virtio_blk.kem runtime/kem_minifs.kem \
runtime/kem_virtio_net.kem runtime/kem_elf.kem runtime/kem_dtb.kem \
test/ornekler/kem_os.kem"
```
Insert uart after kem_heap. Anchor: `OS_PARCA="runtime/kem_heap.kem runtime/kem_mmu.kem`.

ESKI:
```
OS_PARCA="runtime/kem_heap.kem runtime/kem_mmu.kem runtime/kem_gorev.kem \
runtime/kem_zaman.kem runtime/mimari/arm64/kem_bariyer.kem runtime/kem_virtio_blk.kem runtime/kem_minifs.kem \
```
YENI:
```
OS_PARCA="runtime/kem_heap.kem runtime/mimari/arm64/kem_uart.kem runtime/kem_mmu.kem runtime/kem_gorev.kem \
runtime/kem_zaman.kem runtime/mimari/arm64/kem_bariyer.kem runtime/kem_virtio_blk.kem runtime/kem_minifs.kem \
```

Good.

Now potential issue: in kem_heap.kem, kdl_mmio_yaz32 is in same file (kem_heap.kem), so kem_uart_bayt arm64 calls it. x86 file does not call kdl_mmio_yaz32. Good.

But x86 kem_uart_bayt calls kem_port_oku8 and kem_port_yaz8, both in same file. Good.

Also `kem_panik` in kem_heap.kem now calls `kem_uart_bayt`, which is defined in a different file (CAT'd). Both are çıplak. Good.

Now potential issue: `kem_uart_bayt` is çıplak, so calling it from `kem_panik` (çıplak) is fine.

Check dtam16/dtam8 types: dtam8 = 8-bit unsigned? dtam16 = 16-bit. Yes.

satıriçi_asm constraints: `girdi("{dx}", port)` — port is dtam16, "dx" is 16-bit register. Good. `girdi("{al}", deger)` — deger dtam8, "al" 8-bit. Good.

`çıktı("={al}", &v)` with v dtam8. Good.

Template: `outb $1, $0` — x86 AT&T syntax, outb %al, %dx? Wait AT&T: `outb %al, (%dx)`? Actually x86 out port: `outb %al, %dx`. AT&T syntax: `outb %al, %dx`. The operand order is source, dest. With inline asm, $0 = first operand = output? No, in this asm there are no outputs listed for outb (kem_port_yaz8), only inputs: girdi dx (port), girdi al (deger). So $0 = port (dx), $1 = deger (al). Template `outb $1, $0` → `outb %al, %dx`. That's correct AT&T.

For inb: output v (al), input port (dx). Template `inb $1, $0` → $0 = output (al), $1 = input (dx) → `inb %dx, %al`. Correct.

But the size constraints: "boyut eşleşmeli: tam16↔dx, tam8↔al". They use `dtam16` and `dtam8`. Good.

Now check the `bozulan("~{memory}")` placement. In kem_bariyer: `bozulan("~{memory}")`. Our syntax includes bozulan clause.

Order of clauses in satıriçi_asm: mimari, şablon, girdi..., çıktı..., bozulan. The example `satıriçi_asm { mimari: arm64 şablon: r#"dsb sy"# bozulan("~{memory}") }`. For x86 with output: I placed çıktı before girdi in kem_port_oku8. Order maybe flexible? Task spec says for kem_port_oku8: `inb $1, $0`, `çıktı("={al}", &v)`, `girdi("{dx}", port)`, `bozulan(...)`. So output first then input. I'll match.

For kem_port_yaz8: task says `girdi("{dx}", port)`, `girdi("{al}", deger)`, `bozulan(...)`. OK.

Now careful about KEMGU keywords: `değişken v: dtam8 = 0 olarak dtam8;` — in kem_heap they use `0 olarak dtam8`. Fine.

But wait: is `dtam8` a valid type name? In kem_heap: `değişken pip: *dtam8` and `*pip = 0 olarak dtam8;`. Yes.

Now edge: in kem_uart_bayt x86, `eğer (lsr & 32) != 0 { ver; }` — does KEMGU support `&` bitwise operator? Likely yes. In kem_heap there's `i < n` etc., but no bitwise example. The task explicitly says `& 32`, so yes.

Also `ver;` in a void function. In kem_heap free: `eğer pa == 0 { ver; }`. Good.

Now `kem_panik` is void çıplak; it loops `iken doğru { }`. Fine.

One subtle thing: After adding kem_panik call in kem_os_x86.kem, the function is normal (non-çıplak) main, which has prologue calling kdl_bolge_olustur etc. It calls kem_panik (çıplak) — normal can call çıplak? E013: çıplak işlev yalnız çıplak/extern çağırabilir. But normal işlev can call çıplak? The rule says bare can only call bare. Normal calling bare is allowed presumably. In kem_os.kem they call yazdir_metin which is extern. So yes.

However, kem_panik uses `metin_uzunluk` and `metin_bayt` which are çıplak provided by kem_heap.kem. Good.

Now, x86 file uses `güvensiz { }` block. Is `güvensiz` a keyword? Yes, from kem_bariyer. Good.

The Makefile Kanıt 6 check: `--disassemble-symbols=kem_uart_bayt`. Does llvm-objdump support that flag? Yes, llvm-objdump has `--disassemble-symbols`. Usually with objdump it's `--disassemble=symbol`. llvm-objdump uses `--disassemble-symbols=`. Task explicitly uses it. Good.

Potential issue: kem_uart_bayt might be inlined into kem_panik by clang -O2? The .ll is at -O2 when compiling .ll to .o? Actually BM_X86 -O2. clang might inline kem_uart_bayt into kem_panik. But it's an internal function; with -O2 it could be inlined. Then `--disassemble-symbols=kem_uart_bayt` would fail (no such symbol). Hmm. But task explicitly asks this. Perhaps in KEMGU codegen, çıplak functions are not inlined? Or maybe they mark noinline. The previous Kanıt uses `call.*<kem_heap_kur>` suggesting calls are preserved. For kem_uart_bayt maybe it's not inlined because it's called from kem_panik which loops? Actually if kem_uart_bayt is called twice in kem_panik, -O2 might inline. But we trust the task.

Also, mfence is in kem_dma_bariyeri, likely not inlined either. OK.

Now, about arm64 kem_uart.kem: it calls `kdl_mmio_yaz32`, which is çıplak. Fine.

Now let's assemble output. Multiple blocks. Order: new files, then edits. But instruction says blocks can be multiple. Let's produce.

Need exact whitespace. For the Makefile, tabs are significant. I'll use literal tab characters. In my response, I need to include tabs. I'll be careful.

Let me write each block.

First new file arm64:
```yeni:runtime/mimari/arm64/kem_uart.kem
// kem_uart.kem — İş 2 (D-672, D2b): ARM64 (PL011) UART0 bayt-yaz arayüzü.
// kem_panik artık mimariden bağımsız kem_uart_bayt çağırır; bu dosya arm64'te
// PL011 UART0 @ 0x09000000 (QEMU virt) yolunu sağlar. x86 ikizi:
// runtime/mimari/x86_64/kem_uart.kem (COM1 16550 port I/O). Ayrı dosya: AS001
// gereği bir dosyada yalnız TEK mimarinin satıriçi_asm'i olabilir; Makefile CAT
// listesi mimariye göre seçer (D2b kararı). Asm yok — kdl_mmio_yaz32
// (kem_heap.kem; aynı CAT birimi) zaten çıplak volatile store emit eder.
çıplak işlev kem_uart_bayt(b: tam32) {
    kdl_mmio_yaz32(150994944, b);
}
```

Second:
```yeni:runtime/mimari/x86_64/kem_uart.kem
// kem_uart.kem — İş 2 (D-672, D2b): x86_64 (COM1 16550) UART bayt-yaz arayüzü.
// kem_panik artık mimariden bağımsız kem_uart_bayt çağırır; bu dosya x86'da
// COM1 16550 port I/O yolunu sağlar: veri 0x3F8 (=1016), LSR 0x3FD (=1021),
// LSR bit 5 (32) = THRE (verici tutma yazmacı boş). arm64 ikizi:
// runtime/mimari/arm64/kem_uart.kem (PL011 MMIO). Ayrı dosya: AS001 gereği bir
// dosyada yalnız TEK mimarinin satıriçi_asm'i olabilir; Makefile CAT listesi
// mimariye göre seçer (D2b kararı). LSR yoklaması SINIRLI: UART yoksa (ya da
// THRE donmuşsa) panik yolu ASILMASIN diye en çok 100000 deneme; sonra veri
// yine yazılır (mesaj kaybolabilir ama çekirdek ilerler).
çıplak işlev kem_port_yaz8(port: dtam16, deger: dtam8) {
    güvensiz {
        satıriçi_asm { mimari: x86_64 şablon: r#"outb $1, $0"# girdi("{dx}", port) girdi("{al}", deger) bozulan("~{memory}") }
    }
}

çıplak işlev kem_port_oku8(port: dtam16) -> dtam8 {
    değişken v: dtam8 = 0 olarak dtam8;
    güvensiz {
        satıriçi_asm { mimari: x86_64 şablon: r#"inb $1, $0"# çıktı("={al}", &v) girdi("{dx}", port) bozulan("~{memory}") }
    }
    ver v;
}

çıplak işlev kem_uart_bayt(b: tam32) {
    değişken deneme: tam32 = 0;
    iken deneme < 100000 {
        değişken lsr: tam32 = kem_port_oku8(1021) olarak tam32;
        eğer (lsr & 32) != 0 { ver; }
        deneme = deneme + 1;
    }
    kem_port_yaz8(1016, b olarak dtam8);
}
```

Third edit kem_heap.kem comment:
```
duzenle:runtime/kem_heap.kem
<<<<<<< ESKI
// Çekirdek paniği: PL011'e (0x09000000) mesaj bas ve DUR. kdl_panik kem_os'ta YOK
// (D-592); çıplak bağlamda yalnız çıplak çağrılabilir (E013) → doğrudan MMIO.
// Kanal (D-623) ve görev (D-624) ortak kullanır.
=======
// Çekirdek paniği: konsol artık mimari başına kem_uart_bayt
// (runtime/mimari/<arch>/kem_uart.kem). arm64'te yine PL011 MMIO; x86'da COM1
// 16550 port I/O. kdl_panik kem_os'ta YOK (D-592); çıplak bağlamda yalnız
// çıplak çağrılabilir (E013) → doğrudan MMIO. Kanal (D-623) ve görev (D-624)
// ortak kullanır.
>>>>>>> YENI
```

Wait, the ESKI matches the comment lines exactly. Then body edit separately. But note the comment edit replaced 3 lines with 6 lines; the body edit anchor must still match the original. Fine.

Body edit:
```
duzenle:runtime/kem_heap.kem
<<<<<<< ESKI
        kdl_mmio_yaz32(150994944, metin_bayt(m, i) olarak tam32);
        i = i + 1;
    }
    kdl_mmio_yaz32(150994944, 10);
=======
        kem_uart_bayt(metin_bayt(m, i) olarak tam32);
        i = i + 1;
    }
    kem_uart_bayt(10);
>>>>>>> YENI
```

Now kem_os_x86 comment edit:
ESKI:
```
// Ayrıca main başında kem_dma_bariyeri() çağrılır ve "X86 DMA BARIYER OK"
// satırı basılır (gerçekten çalıştığının kanıtı; #UD olsaydı çekirdek düşerdi).
```
YENI: add two lines.

And body edit.

Makefile edits with tabs. I must ensure tab characters. I'll write them as literal tabs in the response.

Let me produce the final answer with all blocks.

Also, in Makefile Kanıt 6, the `@if ... ; then \` continuation lines: in Makefile, lines after `\` should be indented with tab. I'll use tabs.

Actually within a recipe, a line that's a continuation starts with tab. So:
```
	@if ! llvm-objdump ... ; then \
		echo "FAIL: ..."; exit 1; fi
```
The second line has one tab? The original pattern in context: `\t\techo "FAIL: dsb..."` has TWO tabs. Why two? Because it's inside a recipe and the continuation retains indentation of recipe. Actually the original:
```
	@if llvm-objdump -d --no-show-raw-insn $(BUILD)/kem_os_x86.elf | grep -qP '\tdsb\b'; then \
		echo "FAIL: dsb x86_64 binary'de bulundu"; exit 1; fi
```
First line starts with one tab (`\t@if`). Second line starts with two tabs (`\t\techo`). Hmm, that's odd. Looking at the context snippet:
```
	@# Komut SÜTUNUNDA ara (sekme + dsb): düz 'dsb' `<vblk_dsb>` sembol adıyla eşleşir.
	@if llvm-objdump -d --no-show-raw-insn $(BUILD)/kem_os_x86.elf | grep -qP '\tdsb\b'; then \
		echo "FAIL: dsb x86_64 binary'de bulundu"; exit 1; fi
```
Yes the second line appears with two tabs in the raw context. Actually it could be one tab rendered plus one? The user's task says "başta İKİ TAB". So I'll use two tabs.

For my new lines, I'll follow the same two-tab continuation indentation.

Let's be precise: each continuation line `\t\techo ...`.

Now the anchor ESKI for this edit is exactly `\t\techo "FAIL: dsb x86_64 binary'de bulundu"; exit 1; fi`. Good.

Now output.

Let me double check check_kapisi.sh message anchor. The context:
```
    echo "  ✅ kem_os_x86 (birleşik kaynak, 4 parça, --mimari x86_64) --check geçti"
```
Anchor: `kem_os_x86 (birleşik kaynak, 4 parça, --mimari x86_64)`. I'll replace just that substring? The ESKI must match existing text. I can use the whole line:
ESKI: `    echo "  ✅ kem_os_x86 (birleşik kaynak, 4 parça, --mimari x86_64) --check geçti"`
YENI: `    echo "  ✅ kem_os_x86 (birleşik kaynak, 5 parça, --mimari x86_64) --check geçti"`

But leading spaces? The context shows `    echo` (4 spaces). Let me count in snippet: `    echo "  ✅ kem_os_x86 ...`. Yes 4 spaces. Good.

Now baremetal_diff: the context shows continuation lines with backslash. I'll include.

Now final answer. Ensure no extra blocks not requested.

One more: Makefile comment edit anchor. The context line is `# Kanıt 4: objdump'da mfence (x86_64 DMA bariyeri) satırları; dsb YOK.` followed by `calistir_kem_os_x86: ...`. I'll use both for context? The instruction says anchor is that line. I'll just use that line as ESKI and append.

Actually if I only use that line as ESKI you can't append after unless new text contains it. I'll do that.

Now, Makefile CAT arm anchor: The context file `makefile_kem_os_arm_cat.txt` has exactly:
```
	@cat runtime/kem_heap.kem runtime/kem_mmu.kem runtime/kem_gorev.kem runtime/kem_zaman.kem runtime/mimari/arm64/kem_bariyer.kem runtime/kem_virtio_blk.kem runtime/kem_minifs.kem runtime/kem_virtio_net.kem runtime/kem_elf.kem runtime/kem_dtb.kem test/ornekler/kem_os.kem > $(BUILD)/kem_os_comb.kem
```
One tab. Good.

Now produce final.
