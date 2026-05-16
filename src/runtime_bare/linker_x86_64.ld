/* KEMGU Bare-Metal Linker Script — x86_64
 * ========================================
 *
 * Custom entry point: _baslat (KEMGU programi tanimlar).
 * Yukleme adresi: 1MB (multiboot/legacy BIOS convention).
 *
 * Kullanim:
 *   clang -target x86_64-unknown-none -nostdlib \
 *         -Wl,-T,src/runtime_bare/linker_x86_64.ld \
 *         prog.ll -o prog.elf
 *
 * QEMU ile calistirma (manuel):
 *   qemu-system-x86_64 -kernel prog.elf -nographic
 */

ENTRY(_baslat)

SECTIONS
{
    . = 0x100000;       /* 1MB — geleneksel kernel adresi */

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
