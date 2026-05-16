# KEMGU Bare-Metal Destek Haritası

Bu belge KEMGU'nun bare-metal (libc'siz, OS'siz) ortamda hangi kapasitelere
sahip olduğunu **şu anki commit itibariyle** özetler. Eksik olanlar
açık şekilde işaretlenir; eksiklik yol haritasına eklenir.

İlgili kaynaklar:
- `belgeler/MIMARI.md` §ARM64 cross — cross-compile pipeline
- `NOTES_TRACK_B.md` — UART/Konsol Driver çalışmasının karar defteri
- `linker/bare-metal-aarch64.ld` — ARM64 yerleşim
- `boot/start_aarch64.S` — ARM64 _start

---

## Hedefler

| Hedef | Toolchain | Linker | Test |
|-------|-----------|--------|------|
| ARM64 (aarch64-unknown-none) | clang 22+, llvm-objdump | ld.lld -m aarch64linux | `make calistir_uart_pl011_bare_metal`, `calistir_uart_merhaba_bare_metal` |
| x86_64 (x86_64-unknown-none) | clang 22+, llvm-objdump | ld.lld -m elf_x86_64 | `make calistir_uart_16550_bare_metal` |

QEMU smoke testi opsiyonel — şu an pipeline'a dahil değil (toolchain
kurulu değilse `file` + `llvm-objdump -h` + `llvm-nm` ile statik analiz
yeterli).

---

## Runtime Modülleri

### Çalışan (bare-metal'da derlenir + link olur)

| Modül | Dosya | API | Notlar |
|-------|-------|-----|--------|
| PL011 UART | `runtime/kdl_runtime_uart_pl011.c` | `kdl_uart_pl011_init`, `kdl_uart_pl011_putc`, `kdl_uart_pl011_yaz` | QEMU virt @ 0x09000000, RPi 4 vb. (`-DKDL_PL011_BASE=...` override) |
| 16550A UART | `runtime/kdl_runtime_uart_16550.c` | `kdl_uart_16550_init`, `kdl_uart_16550_putc`, `kdl_uart_16550_yaz` | PC COM1 @ 0x3F8 (inline asm in/out) |
| Bare-metal yazdır | `runtime/kdl_runtime_yazdir_bare.c` | `kdl_yazdir_metin`, `kdl_yazdir_satir`, `kdl_yaz_metin`, `kdl_yazdir_tam`, `kdl_yazdir_tam64`, `kdl_yaz_tam`, `kdl_yazdir_mantiksal`, `kdl_format_tam64` | Stack-yalnız, libc-yok; backend `-DKDL_UART_PUTC=...` ile seçilir |
| ARM64 _start | `boot/start_aarch64.S` | `_start`, `_halt`, `__bss_start`, `__bss_end`, `__stack_top` | SP setup + BSS zero + `bl main` + WFE spin |

### Compile-time bayraklar

| Bayrak | Etki | Kullanım |
|--------|------|----------|
| `KEMGU_BARE_METAL` | Gerçek MMIO/port erişimi etkin | `clang -target ...-unknown-none -DKEMGU_BARE_METAL ...` |
| `KEMGU_UART_MOCK` | Sürücü-özel mock buffer'a yönlendir | Test ortamı (host derleme) |
| `KDL_PL011_BASE` | PL011 taban adresi override | Raspberry Pi 4 = 0xFE201000 |
| `KDL_16550_BASE` | 16550 port taban override | COM2 = 0x2F8 |
| `KDL_UART_PUTC` | yazdır backend sembolü | `kdl_uart_pl011_putc` veya `kdl_uart_16550_putc` |

### KEMGU dili tarafı (LLVM emit)

| KEMGU built-in | LLVM IR çağrısı | Host/bare-metal ortak |
|----------------|-----------------|-----------------------|
| `yazdir(metin) -> tam32` | `@puts(ptr)` | Yalnız host (eski API) |
| `yazdir_metin(metin) -> boş` | `@kdl_yazdir_metin(ptr)` | **Her iki tarafta**: host = libc fputs, bare-metal = UART |
| `yazdir_tam(tam32) -> boş` | `@kdl_yazdir_tam(i32)` | **Her iki tarafta**: aynı sembol, farklı runtime |
| `yazdir_tam64(tam64) -> boş` | `@kdl_yazdir_tam64(i64)` | Her iki tarafta |
| `yazdir_satir() -> boş` | `@kdl_yazdir_satir()` | Her iki tarafta |
| `yaz_tam(tam32) -> boş` | `@kdl_yaz_tam(i32)` | Her iki tarafta |
| `yaz_tam64(tam64) -> boş` | `@kdl_yaz_tam64(i64)` | Her iki tarafta |

Önemli: KEMGU programı **platform-agnostiktir**. Aynı `.kem` dosyası
hem host (`./build/kemgu --llvm a.kem | clang … build/kdl_runtime.o`)
hem ARM64 bare-metal (`… clang -target aarch64-unknown-none … +
kdl_runtime_uart_pl011.o + kdl_runtime_yazdir_bare.o`) link akışlarında
derlenir. Bkz. `test/ornekler/uart_merhaba.kem`.

---

## Eksikler (Açık Maddeler)

### Kısa vade

- **`yazdir(metin) -> puts` map'i** host'ta libc puts'a gidiyor.
  Bare-metal'da kullanıcı `yazdir_metin` çağırmalı. `--hedef bare-metal`
  CLI bayrağı eklenirse `yazdir` da otomatik `kdl_yazdir_metin`'e
  yönlenebilir.

- **Onaltılık ve işaretsiz tam yazdır** — KAPSAMA EKLEYECEĞIM
  (continuation C1).

- **Bare-metal panic handler** — `kdl_panik_dur(const char *)` UART'a
  mesaj + halt loop. KAPSAMA EKLEYECEĞIM (continuation C3).

- **Karakter yazdır (`kdl_yazdir_karakter` UTF-8 encoding)** — Host
  runtime'da var; bare-metal port henüz yok.

### Orta vade

- **Input yönü** — `kdl_oku_karakter`, `kdl_oku_metin` UART RX yolu.
  Capability Spec ile "okuma yetkisi" formalizasyonu gerekir.

- **Bump allocator** — Mehmet onayında. Onay sonrası heap çağrıları
  (`bellek_al`, dizi runtime, kdl_metin_birlestir vb.) bare-metal'da
  da çalışabilir hale gelir. Şu an kapsam dışı.

- **QEMU smoke test pipeline** — `qemu-system-aarch64 -nographic -M virt
  -kernel kernel.elf` ile semihosting çıktı yakalama. Toolchain kurulumu
  gerek.

- **`--hedef` CLI bayrağı** — kemgu'ya bare-metal hedef seçimi. Şu an
  LLVM IR target-triple ASCII varsayım (`x86_64-w64-windows-gnu`); clang
  `-target` ile override edilir. Native KEMGU çalışmasında otomatik
  yönlendirme yok.

### Uzun vade

- **Self-host bootloader** — ARM64 boot kod KEMGU'da yazılması (kendi
  start.kem).

- **SMP başlatma** — multi-CPU çekirdek desteği (PSCI çağrıları).

- **Sayfa tablosu setup** — MMU etkinleştirme, sanal bellek.

- **Interrupt handling** — IRQ vektör tablosu, exception level geçişleri.

---

## Cross-Compile Hızlı Başlangıç

ARM64 bare-metal hello world:

```bash
export PATH=/c/msys64/clang64/bin:/c/msys64/ucrt64/bin:$PATH
mingw32-make calistir_uart_merhaba_bare_metal
```

x86_64 bare-metal driver doğrulaması:

```bash
mingw32-make calistir_uart_16550_bare_metal
```

Tek tek bileşen testleri:

```bash
mingw32-make calistir_uart_pl011_test          # PL011 mock 10 test
mingw32-make calistir_uart_16550_test          # 16550 mock 8 test
mingw32-make calistir_yazdir_bare_test         # yazdır portu 17 test
mingw32-make calistir_uart_pl011_bare_metal    # PL011 ARM64 obj + symbol
mingw32-make calistir_yazdir_bare_bare_metal   # yazdır ARM64 obj + symbol
```

---

## Strateji: Hedef 3 (Evrensel OS) Yol Haritası

Bu Track B (UART/Konsol) Hedef 3'ün **ilk işe yarayan adımıdır**:
"konsola çıktı veren bare-metal kernel". Sonraki adımlar:

1. **Track A (Capability + Linear)** ile sinerji — bare-metal'da yetki
   tabanlı I/O (`yetki<UART>` ile `putc` çağrısı). Şu an iki track
   bağımsız ilerliyor.

2. **Bump allocator onayı** — Mehmet sürecinde. Onaylanırsa bare-metal'da
   `Dizi<T>`, `metin_birlestir` vb. heap-bağımlı API'ler de çalışır.

3. **`--hedef bare-metal` flag** — kemgu CLI tarafında hedef bilgisi
   `yazdir → kdl_yazdir_metin` gibi otomatik yönlendirmeleri sağlar.

4. **Bootloader + init** — Şu an _start asm. KEMGU'da yazılması (`güvensiz`
   blok + inline asm benzeri primitif gerekir — yeni 🔴 keyword: `asm`).

5. **Multi-core + MMU** — sayfa tablosu setup, SMP başlatma. Uzun vade.
