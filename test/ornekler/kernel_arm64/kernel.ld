/* KEMGU bare-metal ARM64 (aarch64) — linker script
 * Hedef: QEMU virt machine (RAM 0x40000000'dan baslar)
 *        Pi 3/4 icin 0x80000 (ARM_64BIT_LOAD)
 */

ENTRY(_start)

SECTIONS {
    /* QEMU virt RAM baslangici */
    . = 0x40000000;

    /* Boot kodu */
    .text.boot : {
        KEEP(*(.text.boot))
    }

    /* Esas kod */
    .text : {
        *(.text*)
    }

    /* Salt-okunur veri */
    .rodata : ALIGN(8) {
        *(.rodata*)
    }

    /* Baslatilmis veri */
    .data : ALIGN(8) {
        *(.data*)
    }

    /* Baslatilmamis veri + stack */
    .bss : ALIGN(16) {
        *(.bss*)
        . = ALIGN(16);
        . = . + 16K;          /* 16 KB stack */
        stack_top = .;
    }
}
