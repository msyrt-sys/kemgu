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

    /* MB-4 (çekirdek W^X): bölüm sınırları 4KB'a HİZALI olmalı. Sayfa izinleri
     * 4KB granülde verilir; .text ile .rodata aynı sayfayı paylaşırsa o sayfa
     * zorunlu olarak hem çalıştırılabilir hem rodata olur ve ayrım anlamını
     * yitirir (ölçüldü: .text 0x4000b228'de bitiyor, .rodata 0x4000b230'da
     * başlıyordu — AYNI sayfa). Hizalama birkaç KB dolgu maliyetinde. */
    __text_start = .;

    .text : ALIGN(16) {
        *(.text._start)      /* boot kod en başta */
        *(.text*)
        *(.text.*)
    }

    . = ALIGN(4096);
    __text_end = .;

    .rodata : ALIGN(4096) {
        __rodata_start = .;      /* D-151: syscall okuma-pointer doğrulama sınırı */
        *(.rodata*)
        *(.rodata.*)
        *(.srodata*)
        __rodata_end = .;
    }

    . = ALIGN(4096);
    __rodata_sayfa_sonu = .;     /* rodata'nın kapsadığı SON sayfanın sonu */

    .data : ALIGN(4096) {
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

    /* === Heap (C1: bölge/dizi tahsisleri) ===
     * Stack'in ÜZERİNDE, yukarı doğru büyür (stack aşağı). NOBITS — dosyada
     * yer kaplamaz, runtime RAM. kdl_bare_heap.c bump+serbest-liste backing'i.
     * QEMU virt 128 MB RAM @ 0x40000000 → 16 MB bol. */
    . = ALIGN(16);
    __heap_start = .;
    . = . + 0x1000000;       /* 16 MB heap */
    __heap_end = .;

    /* === EL0 user-region (D2) ===
     * 0x42000000 = kdl_mmu.c'nin AP=01 (EL0-erişimli) işaretlediği 2MB sayfa.
     * EL0 kodu (.user) + 8KB EL0 yığını burada. Kernel'in geri kalanı (AP=00)
     * EL0'a kapalı → privilege ayrımı. heap_end (~0x41xxxxxx) < 0x42000000. */
    . = 0x42000000;
    .user : ALIGN(16) {
        __user_start = .;
        *(.user)             /* EL0 kod (çalıştırılabilir) */
        *(.user.*)
        . = ALIGN(16);
        *(.user_data)        /* D-125: EL0-yazılabilir veri (aynı AP=01 sayfası) */
        *(.user_data.*)
        . = ALIGN(16);
        . = . + 0x2000;      /* 8 KB EL0 user yığını */
        __user_stack_top = .;
        __user_end = .;
    }

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
