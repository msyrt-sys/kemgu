/*
 * KEMGU Bare-Metal Linker Script — x86_64 (PVH boot)
 * ==================================================
 *
 * Yük adresi 0x100000 (1 MB) — PVH/multiboot geleneği. QEMU `-kernel` 64-bit
 * ELF'i PVH note ile yükler, pvh_entry (32-bit) çalışır → long mode → main.
 *
 * Bölümler: .note.Xen (PVH note, başta) / .text / .rodata / .data / .bss
 * (sayfa tabloları dahil) / 16KB stack / 16MB heap (frame allocator backing).
 * aarch64 muadili linker/bare-metal-aarch64.ld.
 */

ENTRY(pvh_entry)

SECTIONS
{
    . = 0x100000;            /* 1 MB */

    .note.Xen : ALIGN(4) {
        *(.note.Xen)         /* PVH boot note — yüklü imajın başında olmalı */
    }

    .text : ALIGN(16) {
        *(.text._start)
        *(.text*)
        *(.text.*)
    }

    .rodata : ALIGN(16) {
        __rodata_start = .;      /* D-151: syscall okuma-pointer doğrulama sınırı (arch paritesi) */
        *(.rodata*)
        *(.rodata.*)
        __rodata_end = .;
    }

    .data : ALIGN(16) {
        *(.data*)
        *(.data.*)
    }

    .bss : ALIGN(16) {
        __bss_start = .;
        *(.bss*)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
    }

    . = ALIGN(16);
    . = . + 0x4000;          /* 16 KB stack */
    __stack_top = .;

    /* Heap (C1: bölge/dizi tahsisleri) — frame allocator backing. */
    . = ALIGN(4096);
    __heap_start = .;
    . = . + 0x1000000;       /* 16 MB */
    __heap_end = .;

    /DISCARD/ : {
        *(.comment*)
        *(.eh_frame*)
        *(.note.GNU-stack)
    }
}
