/* Kullanıcı görüntüsü. Kalan kullanıcı penceresini yükleyici eşler. */
ENTRY(_start)

PHDRS {
    kod PT_LOAD FLAGS(5);
    salt_okunur PT_LOAD FLAGS(4);
    veri PT_LOAD FLAGS(6);
    sifir PT_LOAD FLAGS(6);
}

SECTIONS {
    . = 0x54000000;
    .text : ALIGN(4096) {
        KEEP(*(.text._start))
        *(.text .text.*)
    } :kod
    .rodata : ALIGN(4096) { *(.rodata .rodata.*) } :salt_okunur
    .data : ALIGN(4096) { *(.data .data.*) } :veri
    .bss (NOLOAD) : ALIGN(4096) { *(.bss .bss.* COMMON) } :sifir
    ASSERT(. <= 0x54100000, "Kullanıcı ELF'i görüntü alanına sığmadı")
    __heap_start = 0x54100000;
    __heap_end = 0x541ef000;

    /DISCARD/ : { *(.note* .comment .eh_frame*) }
}
