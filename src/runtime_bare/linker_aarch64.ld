/* KEMGU Bare-Metal Linker Script — AArch64
 * =========================================
 *
 * Custom entry point: _baslat.
 * Yukleme adresi: 0x40080000 — QEMU virt machine konvansiyonu
 * (RAM 0x40000000 baslangic + 0x80000 offset ARM mark).
 *
 * Kullanim:
 *   clang -target aarch64-unknown-none -nostdlib \
 *         -Wl,-T,src/runtime_bare/linker_aarch64.ld \
 *         prog.ll -o prog.elf
 *
 * QEMU ile calistirma (manuel):
 *   qemu-system-aarch64 -M virt -cpu cortex-a57 -kernel prog.elf -nographic
 */

ENTRY(_baslat)

SECTIONS
{
    . = 0x40080000;     /* QEMU virt machine RAM + ARM start offset */

    .text ALIGN(4K) : {
        *(.text*)
    }

    .rodata ALIGN(4K) : {
        *(.rodata*)
    }

    .data ALIGN(4K) : {
        *(.data*)
    }

    .bss ALIGN(4K) : {
        *(COMMON)
        *(.bss*)
    }

    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note*)
    }
}
