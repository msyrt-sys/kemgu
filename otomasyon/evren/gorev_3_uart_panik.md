## İş 2 — Mimari başına çıplak UART ailesi + `kem_panik`'in x86 yolu (D2b)

### Sorun (ölçüldü)
`kem_panik` (`runtime/kem_heap.kem`) mesajı `kdl_mmio_yaz32(150994944, …)` ile
PL011'e (`0x09000000`, QEMU aarch64 `virt`) yazar. x86'da orada UART YOK →
panik mesajı sessizce kaybolur, çekirdek mesajsız donar. x86 konsolu COM1
16550, PORT I/O: veri `0x3F8` (=1016), LSR `0x3FD` (=1021), LSR bit 5 (32) =
verici tutma yazmacı boş (THRE).

### Karar (verilmiş)
Önceki işle (D-671) AYNI desen: mimari başına dosya, Makefile CAT seçer.
Ortak arayüz: `çıplak işlev kem_uart_bayt(b: tam32)` — tek bayt yaz.

### İstenen değişiklikler (yalnız bunlar)

1. **YENİ `runtime/mimari/arm64/kem_uart.kem`** — `kem_uart_bayt(b: tam32)`:
   gövde `kdl_mmio_yaz32(150994944, b);` (bugünkü davranışın BİREBİR aynısı;
   `kdl_mmio_yaz32` kem_heap.kem'de, aynı CAT birimi). asm YOK. Türkçe başlık
   yorumu: PL011 UART0 @ 0x09000000 (QEMU virt), x86 ikizi, D2b.
2. **YENİ `runtime/mimari/x86_64/kem_uart.kem`** — üç çıplak işlev:
   - `kem_port_yaz8(port: dtam16, deger: dtam8)` — `satıriçi_asm`
     `mimari: x86_64`, şablon `outb $1, $0`, `girdi("{dx}", port)`,
     `girdi("{al}", deger)`, `bozulan("~{memory}")`.
   - `kem_port_oku8(port: dtam16) -> dtam8` — `inb $1, $0`,
     `çıktı("={al}", &v)`, `girdi("{dx}", port)`, `bozulan("~{memory}")`.
   - `kem_uart_bayt(b: tam32)`: LSR'yi (1021) THRE (bit 5 = 32) set olana dek
     yokla — AMA SINIRLI: en çok 100000 deneme, sonra yine yaz (UART yoksa
     panik yolu ASILMASIN). Sonra veriyi 1016'ya yaz. Bit testi için
     `kem_port_oku8(...)` sonucunu `olarak tam32` ile genişlet, `& 32`.
     Değeri `b olarak dtam8` ile daralt.
   - Asm'ler `güvensiz { }` içinde (bkz. `runtime/mimari/x86_64/kem_bariyer.kem`).
   - Türkçe başlık yorumu: COM1 16550 port I/O, LSR yoklaması neden sınırlı,
     arm64 ikizi, D2b.
3. **`runtime/kem_heap.kem` `kem_panik`**: iki `kdl_mmio_yaz32(150994944, X)`
   çağrısını `kem_uart_bayt(X)` yap (X aynen: `metin_bayt(m, i) olarak tam32`
   ve `10`). Üstteki yorumu güncelle: konsol artık mimari başına
   `kem_uart_bayt` (runtime/mimari/<arch>/kem_uart.kem); arm64'te yine PL011,
   x86'da COM1 port I/O. Gövdenin geri kalanı (`iken doğru { }` dahil) AYNI.
4. **CAT listeleri** — `runtime/kem_heap.kem`'in HEMEN ARDINA mimarinin
   uart dosyası:
   - Makefile `calistir_kem_os_arm`: `runtime/mimari/arm64/kem_uart.kem`
   - `test/baremetal_diff_harness.sh` `OS_PARCA`: `runtime/mimari/arm64/kem_uart.kem`
   - `test/check_kapisi.sh` `KEMOS_PARCALAR`: `runtime/mimari/arm64/kem_uart.kem`
   - `test/check_kapisi.sh` `KEMOS_X86_PARCALAR`: `runtime/mimari/x86_64/kem_uart.kem`
     ve aynı bloktaki mesajda `4 parça` → `5 parça`.
   - Makefile `calistir_kem_os_x86` CAT: `runtime/mimari/x86_64/kem_uart.kem`
5. **`test/ornekler/kem_os_x86.kem`**: `main`'in SONUNDA, `ver 0;` ÖNCESİNE
   `kem_panik("X86 PANIK YOLU OK");` çağrısı ekle (panik geri dönmez; mesaj
   .kem COM1 yolundan basılır, çekirdek orada durur — QEMU zaman aşımı keser).
   Başlık yorumuna bunu ekle. `ver 0;` satırı kalsın (tip denetimi için).
6. **`Makefile` `calistir_kem_os_x86`** — dsb denetiminin (`echo "FAIL: dsb
   x86_64 binary'de bulundu"; exit 1; fi`) ARDINA:
   - **Kanıt 5 (davranış):** `$(BUILD)/kem_os_x86.out` içinde TAM satır
     `X86 PANIK YOLU OK` olmalı (`grep -qxF`); yoksa FAIL.
   - **Kanıt 6 (gerçek komut, .kem UART'a ait):** `llvm-objdump -d
     --no-show-raw-insn --disassemble-symbols=kem_uart_bayt
     $(BUILD)/kem_os_x86.elf` çıktısında `outb` VE `inb` satırları olmalı;
     ikisini de OLDUKLARI GİBİ yazdır. Yoksa FAIL. (Tüm ikilide aramak YETMEZ:
     C UART nesnesi de outb/inb içerir; kanıt .kem işlevine ait olmalı.)
   - Hedefin başındaki açıklama yorumuna Kanıt 5/6'yı ekle (çapa:
     `# Kanıt 4: objdump'da mfence (x86_64 DMA bariyeri) satırları; dsb YOK.`).

### Çapalar (ESKI metinleri — AYNEN; her biri TEK kez geçer)
- Makefile arm CAT: `@cat runtime/kem_heap.kem runtime/kem_mmu.kem`
- baremetal_diff: `OS_PARCA="runtime/kem_heap.kem runtime/kem_mmu.kem`
- check_kapisi: `KEMOS_PARCALAR="runtime/kem_heap.kem runtime/kem_mmu.kem`
- check_kapisi: `KEMOS_X86_PARCALAR="runtime/kem_heap.kem runtime/mimari/x86_64/kem_bariyer.kem`
- check_kapisi: `kem_os_x86 (birleşik kaynak, 4 parça, --mimari x86_64)`
- Makefile x86 CAT: `	cat runtime/kem_heap.kem runtime/mimari/x86_64/kem_bariyer.kem runtime/kem_virtio_blk.kem test/ornekler/kem_os_x86.kem > $(BUILD)/kem_os_x86_comb.kem`
- kem_heap.kem: `        kdl_mmio_yaz32(150994944, metin_bayt(m, i) olarak tam32);`
- kem_heap.kem: `    kdl_mmio_yaz32(150994944, 10);`
- kem_heap.kem yorum: `// Çekirdek paniği: PL011'e (0x09000000) mesaj bas ve DUR. kdl_panik kem_os'ta YOK`
- kem_os_x86.kem: `    yazdir_tam(toplam);` + `    yazdir_satir();` + `    ver 0;` + `}` (4 satır)
- Makefile: `		echo "FAIL: dsb x86_64 binary'de bulundu"; exit 1; fi` (başta İKİ TAB)

### Yasak
`src/*.c`, `selfhost/*`, `runtime/kem_gorev.kem`, `runtime/kem_mmu.kem`,
`runtime/kem_zaman.kem`, `test/ornekler/kem_os.kem`.
