# KEMGU Bare-Metal Kernel Scaffold

KEMGU dilinde yazilmis bir x86_64 kernel iskeleti. Multiboot2 uyumlu bootloader (GRUB) tarafindan yuklenir, VGA text-mode ekrana `KEMGU` yazar, sonra halt dongusune girer.

## Dosyalar

- `kernel.kem` — Ana kernel kodu: `_start` (naked entry), `kernel_main`, `vga_kemgu_yaz`, `sonsuz_halt`
- `multiboot.kem` — Multiboot2 baslik (raw bayt, inline asm ile)
- `kernel.ld` — Linker script: layout (1MB load, .multiboot once, sonra .text.boot, .text, ...)
- `README.md` — Bu dosya

## KEMGU tarafindan kullanilan ozellikler

| Ozellik | Nerede |
|---------|--------|
| `[bolum: ".text.boot"]` | `_start` icin (linker section attribute) |
| `[ciplak]` (naked) | `_start`, `sonsuz_halt`, `__multiboot_header` |
| `_asm("...")` | Stack setup, halt loop, multiboot baslik bayt |
| `_yaz_volatile_dtam8(adres, deger)` | VGA buffer'a yazim (MMIO) |
| `güvensiz { ... }` | Raw pointer / asm erisimi |
| `sabit X: dtam64 = ...` | VGA adresi vs. constants |
| `--freestanding` | libc/CRT olmadan derleme |
| `--target=x86_64-unknown-none` | Bare-metal hedef |
| `--linker-script=kernel.ld` | Layout kontrolu |

## Derleme

### Adim 1: kernel.elf uret (Linux/macOS host, Windows'ta WSL onerilir)

```bash
kemgu --build \
      --freestanding \
      --target=x86_64-unknown-none \
      --linker-script=test/ornekler/kernel/kernel.ld \
      test/ornekler/kernel/multiboot.kem \
      test/ornekler/kernel/kernel.kem \
      -o kernel.elf
```

### Adim 2: ISO olustur (GRUB ile)

```bash
mkdir -p isodir/boot/grub
cp kernel.elf isodir/boot/
cat > isodir/boot/grub/grub.cfg <<EOF
menuentry "KEMGU" {
    multiboot2 /boot/kernel.elf
}
EOF
grub-mkrescue -o kemgu.iso isodir/
```

### Adim 3: QEMU'da calistir

```bash
qemu-system-x86_64 -cdrom kemgu.iso
```

Ekrana `KEMGU` yazmasini beklersiniz.

## KEMGU dogrulamasi (host derlemeden)

Bare-metal hedef olmadan KEMGU derleyicisinin kodu parse etmesi ve LLVM IR uretmesi test edilebilir:

```bash
./build/kemgu --check test/ornekler/kernel/kernel.kem
./build/kemgu --llvm test/ornekler/kernel/kernel.kem | head -50
```

`multiboot.kem` dosyasi inline asm directives (`.long`, `.short`) icerdiginden, sadece bare-metal toolchain kullanildiginda gerçekten anlam tasir.

## Sınırlamalar (yapılacaklar)

1. **Constant data section yok** — `sabit METIN_BAYTLAR: Dizi<dtam8> = [...]` gibi compile-time array'leri `.rodata` section'ina koyma henuz yok. Multiboot baslik o yuzden inline asm ile.
2. **String literal codegen tam degil** — `metin` runtime henuz null-term/slice formati ile tam yapilmamis. Bu yuzden kernel'de "Merhaba dunya" yerine manuel karakter yazimi yapildi.
3. **Linker script symbol Erisimi** — `stack_top` sembolu Linux ELF'te calisir, Windows COFF'ta farkli. Cross-build yapilmasi gerek.
4. **Kalip interrupt handlers** — IDT/GDT setup henuz yapilmadi. `[kesme]` attribute hazir ama trap frame, error code handling vs eksik.
5. **Per-CPU storage** — Multi-core icin gerekli (`%gs:` tabanli erisim) — eklenmedi.
