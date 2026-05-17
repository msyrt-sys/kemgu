# KEMGU Bare-Metal ARM64 Kernel Scaffold

KEMGU dilinde yazilmis bir **ARM64 (aarch64)** kernel iskeleti.
Direktif Hedef 3'un birincil platformu (Apple Silicon, Pi 3/4/5, DGX Spark).

## Dosyalar

- `kernel.kem` — Ana ARM64 kernel: PL011 UART driver, `_start` naked entry, WFI halt
- `kernel.ld` — ARM64 linker script (QEMU virt: RAM @ 0x40000000)
- `README.md` — Bu dosya

## ARM64-spesifik dil ozellikleri

- `_asm("wfi")`, `_asm("msr daifset, #2")` — ARM64 instruction
- `adrp` + `add :lo12:` — PIC-friendly sembol referansi
- `_yaz_volatile_dtam8(adres, val)` — MMIO (PL011 UART)
- `[ciplak]` + `[bolum: ".text.boot"]` — naked entry + section

## Derleme

### KEMGU -> object file
```bash
./build/kemgu.exe -c test/ornekler/kernel_arm64/kernel.kem \
  --target=aarch64-unknown-none --freestanding \
  -o kernel-arm64.o
```

Sonuc:
```
$ file kernel-arm64.o
kernel-arm64.o: ELF 64-bit LSB relocatable, ARM aarch64, version 1 (SYSV)
```

### Link et
```bash
ld.lld -m aarch64elf -T test/ornekler/kernel_arm64/kernel.ld \
  kernel-arm64.o -o kernel-arm64.elf
```

Sonuc:
```
$ file kernel-arm64.elf
kernel-arm64.elf: ELF 64-bit LSB executable, ARM aarch64, version 1 (SYSV),
                  statically linked
```

Section layout (QEMU virt-uyumlu):
```
.text.boot  @ 0x40000000  <- _start
.text       @ 0x4000001c
.rodata     @ 0x400000a0
.bss        @ 0x40000140 (16KB stack -> stack_top @ 0x40004140)
```

## QEMU virt'te calistirma

Linux/macOS host'ta:
```bash
qemu-system-aarch64 -machine virt -cpu cortex-a72 \
  -nographic -kernel kernel-arm64.elf
```

Beklenen: terminale "KEMGU\n" yazar, WFI loop'una girer.

Windows host'ta WSL kullanin:
```bash
wsl
sudo apt install qemu-system-arm
qemu-system-aarch64 ...
```

## Hedef platformlar

| Platform | RAM base | UART base | Status |
|----------|----------|-----------|--------|
| QEMU virt | 0x40000000 | 0x09000000 (PL011) | Bu scaffold |
| Raspberry Pi 3 | 0x80000 | 0x3F201000 (PL011) | Linker script ayarla |
| Raspberry Pi 4 | 0x80000 | 0xFE201000 (PL011) | Linker script ayarla |
| Apple Silicon | EFI yukler | UART donanima bagli | EFI entry farkli |
| DGX Spark | Bilgi yok | UBoot/UEFI yukler | Hedef donanim |

## Sinirlamalar (yapilacaklar)

1. **MMU/Page tables yok** — ARM64 stage-1 translation (TCR_EL1, TTBR0_EL1) henuz ayarlanmiyor; identity-mapped fiziksel adres.
2. **EL2 -> EL1 dusus yok** — QEMU virt EL2'de baslar; gercek kernel EL1'e dusmeli.
3. **Exception vector table yok** — `vbar_el1` set edilmeli.
4. **PSCI / multi-core yok** — Sekonder CPU'lar park edilmiyor.
5. **DTB parse yok** — Boot parametreleri (x0) yoksayiliyor.

Bunlar tip sistemi degil, KEMGU kullanici kodu duzeyinde — Mehmet'le ayri tasarim oturumlarinda eklenecek.
