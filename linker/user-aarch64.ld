/*
 * KEMGU Kullanıcı Programı Linker Script — ARM64 (MODEL B)
 * ========================================================
 *
 * Çekirdek imajından AYRI bir ELF üretir. Çekirdek bunu diskten okur, ELF64
 * başlıklarını çözümler ve PT_LOAD segmentlerini sürecin adres alanına yükler.
 *
 * 0x54000000 = KMMU_OZEL_VA (runtime/kem_mmu.kem): süreç-özel 2MB sanal sayfa.
 * Her sürecin bu VA'sı BAŞKA bir fiziksel sayfaya bakar → programlar birbirinin
 * kodunu/verisini göremez. Sayfa AP=01 (EL0 RW) ve UXN=0 (EL0-exec).
 *
 * Bu betik bare-metal-aarch64.ld'den AYRI tutulur: çekirdek 0x40000000'a,
 * kullanıcı programı 0x54000000'a bağlanır. Tek betiği paylaşsalardı yükleme
 * adresi çakışırdı.
 */

ENTRY(_start)

SECTIONS
{
    . = 0x54000000;

    .text : ALIGN(16) {
        *(.text._start)
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
        *(COMMON)
        __bss_end = .;
    }

    /* Kullanıcı yığını programın SONUNDA değil, çekirdek tarafından ayrı
     * belirlenir (SP_EL0 trap-frame'den gelir) — burada rezerve edilmez. */

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
