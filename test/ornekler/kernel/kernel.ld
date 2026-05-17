/* KEMGU bare-metal x86_64 kernel — linker script */

ENTRY(_start)

SECTIONS {
    /* 1MB sinifina yukle — multiboot'un standardi */
    . = 1M;

    /* Multiboot baslik en basta olmali */
    .multiboot : {
        KEEP(*(.multiboot))
    }

    /* Boot kodu (entry) */
    .text.boot : {
        *(.text.boot)
    }

    /* Esas kod */
    .text : {
        *(.text*)
    }

    /* Salt-okunur veri */
    .rodata : {
        *(.rodata*)
    }

    /* Baslatilmis veri */
    .data : {
        *(.data*)
    }

    /* Baslatilmamis veri + stack */
    .bss : {
        *(.bss*)
        . = ALIGN(16);
        . = . + 16K;          /* 16 KB stack */
        stack_top = .;
    }
}
