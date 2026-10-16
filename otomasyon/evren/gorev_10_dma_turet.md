## İş 10 — x86 DMA tabanını PVH bellek haritasından türet (envanter §7 adım 6b)

Bugün x86 `vblk_dma = 0x02000000`, `vnet_dma = 0x02100000` SABİT. İstenen:
çalışma zamanında PVH memmap'ten türet; PVH yoksa sabitler KALIR (yedek).
DOĞRULANMIŞ SAYILAR (araçla): 1 GB = 1073741824 · 256 KB = 262144 ·
64 KB = 65536 · 4096 · 1 MB = 1048576. Sabit yedek taban KB olarak 32768.
QEMU 128 MB'ta beklenen türetilmiş taban 0x7FC0000 = 133955584 (130816 KB).
Bağlam: `runtime/mimari/x86_64/kem_pvh.kem` (`kem_pvh_bilgi`, `kem_pvh_oku64`,
memmap düzeni ve RAM türü=1 orada belgeli), `runtime/mimari/x86_64/kem_platform.kem`.

### Değişiklikler (yalnız bunlar — 4 madde)
1. **`runtime/mimari/x86_64/kem_platform.kem`** — `sabit VM_YUVA_SAYISI: tam32 = 24;`
   satırının ALTINA:
   - `çıplak işlev kem_platform_dma_tabani() -> tam64`: `b = kem_pvh_bilgi()`;
     b == 0 ya da magic (u32 @b, `kdl_mmio_oku32`) ≠ 862897528 → `ver 0`.
     memmap'i gez (kem_pvh_ram_toplam ile aynı yöntem); tür==1 girdiler içinde
     `son = addr + size`, `son > 1073741824` ise `son = 1073741824` (identity
     eşleme sınırı); `son - addr >= 262144` olanlar arasında EN BÜYÜK `son`'u seç.
     Bulunamazsa `ver 0`. `ver ((son - 262144) / 4096) * 4096;`
   - `çıplak işlev kem_platform_kur()`: `t = kem_platform_dma_tabani()`; t > 0 ise
     `vblk_dma = t; vnet_dma = t + 65536;` (net ~19 KB ihtiyaç, blk ~800 B).
   - Üstüne Türkçe yorum: neden RAM tepesi (heap ~17 MB'ta biter, çakışmaz), neden
     1 GB sınırı (x86 boot yalnız ilk 1 GB'ı eşler), PVH yoksa sabit yedek.
2. **`test/ornekler/kem_os_x86.kem`** — `// İş 3 (a): Blok DMA RAM yaz-oku ...` satırından
   HEMEN ÖNCE: `kem_platform_kur();` + `değişken dt: tam64 = kem_platform_dma_tabani();`
   dt > 0 ise `X86 DMA TABAN OK kb=` + ayrı satırda `(dt / 1024) olarak tam32`;
   değilse `X86 DMA TABAN YEDEK`. (`yazdir_metin` zaten satır sonu ekler —
   arasına `yazdir_satir()` KOYMA; sayıdan sonra `yazdir_satir()` koy.)
   Sonraki tüm DMA/disk/ağ/FS testleri türetilmiş tabanla koşmalı (sıra bu yüzden).
   ÇAPA: `    // İş 3 (a): Blok DMA RAM yaz-oku (0..511; 251 ile döngüsel desen).`
3. **Makefile Kanıt 13** — Kanıt 12'nin objdump satırından SONRA: `X86 DMA TABAN OK kb=`
   TAM satırı (`grep -qxF`); ardından gelen satır `v` olarak alınıp
   `[ "$$v" -gt 32768 ]` olmalı (yedek sabitten BÜYÜK = türetme gerçekten çalıştı);
   değeri yazdır; değilse FAIL. Tarif kabuğu dash: `16#` KULLANMA.
   ÇAPA (tek satır, TAB ile başlar): `	@llvm-objdump -d --no-show-raw-insn --disassemble-symbols=pvh_entry $(BUILD)/kem_os_x86.elf | grep '%ebx' || { echo "FAIL: pvh_entry disasm'da %ebx movl'i yok"; exit 1; }`
4. **Makefile hedef yorumu** — Kanıt 13 açıklaması. ÇAPA:
   `#          ayrıca pvh_entry disasm'ında %ebx saklama movl'i (PVH start_info ABI).`

Blok yolları yalnız: `runtime/mimari/x86_64/kem_platform.kem`,
`test/ornekler/kem_os_x86.kem`, `Makefile`.
