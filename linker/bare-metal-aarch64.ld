/*
 * KEMGU Bare-Metal Linker Script — ARM64 (aarch64-unknown-none)
 * ===============================================================
 *
 * Faz 5+ self-host bootstrap için minimum freestanding kernel binary
 * yerlesim duzenidir. Boot loader (U-Boot, ATF, kendi mini-loader)
 * çekirdek imajını LOAD_ADDR'e yükler ve _baslat etiketine sıçrar.
 *
 * Mevcut kullanım: test/ornekler/kernel.kem KEMGU programı
 *   ./build/kemgu --llvm --hedef=aarch64-unknown-none kernel.kem > kernel.ll
 *   clang -target aarch64-unknown-none -x ir kernel.ll -c -o kernel.o
 *   ld.lld -T linker/bare-metal-aarch64.ld kernel.o -o kernel.elf
 *
 * Layout (ARMv8-A pratik):
 *   0x80000  — .text         (kod, _baslat entry burada)
 *   0x100000 — .rodata       (sabit veri)
 *   0x140000 — .data         (initialize edilmis degisken)
 *   0x180000 — .bss          (sifirla initialize)
 *   0x200000 — __stack_top   (16 KB stack, asagiya buyuyen)
 *
 * Boyutlar pragmatik; gercek kernel'de page table ve memory map'e gore
 * ayarlanmalidir. Ornek olarak Raspberry Pi 4 + ARM Cortex-A72 senaryosu.
 *
 * Entry point: _baslat — bu isim Türkçe konvansiyon. Mehmet onayına açık:
 *   Alternatifler: _baslangic, _giris, kernel_baslat
 *   POSIX karsiligi _start (klasik), Linux kernel start_kernel.
 *   "_baslat" secildi cunku KEMGU dilinde "baslat" semantik (gorev_baslat,
 *   thread bootstrap). Kernel'in tek entry'si oldugu icin tek-kelime.
 *
 * KEMGU'da main() fonksiyonu varsa, freestanding modunda otomatik olarak
 * _baslat -> main eslesmesi yapilmaz; programci bunu explicit kurmali:
 *   - Bir KEMGU islevini _baslat olarak isimlendir, veya
 *   - C-side glue (kdl_baslat.S veya kdl_baslat.c) ile main()'i cagir.
 *
 * V1 yaklasimi: linker script ENTRY(_baslat) tanimlar; KEMGU programci
 * islev _baslat() yazar (main yerine). Bu Faz 5+'ta detaylanir.
 */

ENTRY(_baslat)

MEMORY
{
    /* Genel-amac RAM bolgesi (RPi4 + Cortex-A72 benzeri) */
    RAM (rwx) : ORIGIN = 0x80000, LENGTH = 128M
}

SECTIONS
{
    /* Kod segmenti: 4 KB hizalanmis, RAM basina yerlesir */
    .text 0x80000 : ALIGN(4K)
    {
        __text_basi = .;
        KEEP(*(.text.baslat))     /* _baslat ozellikle ilk */
        *(.text .text.*)
        __text_sonu = .;
    } > RAM

    /* Read-only veri (string literal, sabit tablolar) */
    .rodata : ALIGN(4K)
    {
        __rodata_basi = .;
        *(.rodata .rodata.*)
        __rodata_sonu = .;
    } > RAM

    /* Initialize edilmis veri */
    .data : ALIGN(4K)
    {
        __data_basi = .;
        *(.data .data.*)
        __data_sonu = .;
    } > RAM

    /* Sifir-init veri (BSS) — _baslat burayi sifirlamali */
    .bss : ALIGN(4K)
    {
        __bss_basi = .;
        *(.bss .bss.* COMMON)
        __bss_sonu = .;
    } > RAM

    /* Stack — 16 KB, asagi buyur. _baslat ilk islem SP'yi __stack_top'a kur. */
    . = ALIGN(16);
    __stack_basi = .;
    . += 16K;
    __stack_top = .;

    /* Heap iceren bump allocator backing buffer'i icin alan
     * (BUMP_ALLOCATOR_SPEC_TASLAK.md ile gelecek):
     *   __heap_basi = .;
     *   . += BUMP_HEAP_BOYUTU;   (default 64K)
     *   __heap_sonu = .; */

    /* Debug ve symbol bolumleri (ELF metadata, runtime'da yok) */
    /DISCARD/ :
    {
        *(.comment)
        *(.note .note.*)
        *(.eh_frame)
        *(.ARM.attributes)
    }
}

/* Linker symbol export: KEMGU programi _baslat'tan
 *   __text_basi/sonu, __bss_basi/sonu, __stack_top
 * sembollerini extern olarak referans alabilir. */
