/*
 * KEMGU Bare-Metal Linker Script — ARM64 (aarch64)
 * =================================================
 *
 * Hedef: QEMU virt makinesi varsayılan yükleme adresi 0x40000000
 * (RAM tabanı). Raspberry Pi 4 / firmware-init'li board için
 * adres farklıdır — kullanıcı `-Ttext-segment=...` ile geçersiz
 * kılabilir.
 *
 * Bölümler:
 *   .text        çalıştırılabilir kod (._start dahil)
 *   .rodata      sabit veriler (string literalleri vb.)
 *   .data        başlangıçlanmış global değişkenler
 *   .bss         sıfırlanan global'ler
 *   stack        16 KB statik stack (alt → üst yön büyür)
 *
 * Kalem 3 ölçeği için minimum: dynamic linking yok, exception
 * handling yok, BSS clear işi _start'a düşer.
 */

ENTRY(_start)

SECTIONS
{
    . = 0x40000000;          /* QEMU virt RAM */

    .text : ALIGN(16) {
        *(.text._start)      /* boot kod en başta */
        *(.text*)
        *(.text.*)
    }

    .rodata : ALIGN(16) {
        *(.rodata*)
        *(.rodata.*)
        *(.srodata*)
    }

    .data : ALIGN(16) {
        *(.data*)
        *(.data.*)
        *(.sdata*)
    }

    .bss : ALIGN(16) {
        __bss_start = .;
        *(.bss*)
        *(.bss.*)
        *(.sbss*)
        *(COMMON)
        __bss_end = .;
    }

    . = ALIGN(16);
    . = . + 0x4000;          /* 16 KB stack — Kalem 3 için yeterli */
    __stack_top = .;

    /DISCARD/ : {
        *(.note*)
        *(.comment*)
        *(.eh_frame*)
        *(.gnu.hash)
        *(.hash)
        *(.dynsym)
        *(.dynstr)
        *(.dynamic)
        *(.interp)
    }
}
