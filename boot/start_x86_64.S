/*
 * KEMGU Bare-Metal Boot — x86_64 (PVH → long mode)
 * ================================================
 *
 * QEMU `-kernel <64-bit ELF>` PVH yoluyla boot eder: .note.Xen içindeki
 * XEN_ELFNOTE_PHYS32_ENTRY (tip 18) 32-bit giriş adresini verir. QEMU
 * pvh_entry'ye 32-bit korumalı modda (paging KAPALI, A20 açık, düz segment)
 * atlar. Görevler:
 *   1. (32-bit) BSS sıfırla (sayfa tabloları dahil)
 *   2. (32-bit) Identity sayfa tabloları (0..1GB, 2MB huge page) + PAE + LME +
 *      paging → long mode; 64-bit kod segmentine far jump
 *   3. (64-bit) Segment register'ları + RSP=__stack_top kur, main() çağır
 *   4. main dönerse hlt döngüsü
 *
 * Linker: linker/bare-metal-x86_64.ld (yük 0x100000). aarch64 muadili
 * boot/start_aarch64.S. -mgeneral-regs-only ile C SSE emit etmez (SSE'yi boot'ta
 * etkinleştirmeye gerek yok).
 */

/* === PVH ELF note — QEMU 64-bit ELF'i 32-bit pvh_entry'den boot eder === */
.section .note.Xen, "a"
.align 4
.long 4                 /* namesz: "Xen\0" */
.long 4                 /* descsz: 32-bit giriş adresi (1 long) */
.long 18                /* type = XEN_ELFNOTE_PHYS32_ENTRY */
.asciz "Xen"
.align 4
.long pvh_entry
.align 4

/* === 32-bit giriş: long mode kurulumu === */
.section .text
.code32
.global pvh_entry
.type pvh_entry, @function
pvh_entry:
    cli
    cld

    /* 1. BSS sıfırla (pml4/pdpt/pd .bss'te → temizlenir, sonra kurulur). */
    movl    $__bss_start, %edi
    movl    $__bss_end, %ecx
    subl    %edi, %ecx
    xorl    %eax, %eax
    rep stosb

    /* 2a. PD: 512 × 2MB identity page (present|write|PS huge). */
    movl    $pd, %edi
    movl    $0x83, %eax            /* present|write|PS, fiziksel adres 0 */
    movl    $512, %ecx
1:
    movl    %eax, (%edi)           /* düşük 32 bit */
    movl    $0, 4(%edi)            /* yüksek 32 bit = 0 */
    addl    $0x200000, %eax        /* sonraki 2MB */
    addl    $8, %edi
    loop    1b

    /* 2b. PDPT[0] = PD | present|write */
    movl    $pd, %eax
    orl     $0x3, %eax
    movl    %eax, pdpt
    movl    $0, pdpt+4

    /* 2c. PML4[0] = PDPT | present|write */
    movl    $pdpt, %eax
    orl     $0x3, %eax
    movl    %eax, pml4
    movl    $0, pml4+4

    /* 2d. CR3 = PML4 */
    movl    $pml4, %eax
    movl    %eax, %cr3

    /* 2e. CR4.PAE (bit 5) */
    movl    %cr4, %eax
    orl     $0x20, %eax
    movl    %eax, %cr4

    /* 2f. EFER.LME (MSR 0xC0000080, bit 8) */
    movl    $0xC0000080, %ecx
    rdmsr
    orl     $0x100, %eax
    wrmsr

    /* 2g. CR0: PG(31) | PE(0) → paging + long mode aktif */
    movl    %cr0, %eax
    orl     $0x80000001, %eax
    movl    %eax, %cr0

    /* 2h. GDT yükle, 64-bit kod segmentine far jump */
    lgdt    gdt_desc
    ljmp    $0x08, $long_entry

/* === 64-bit giriş === */
.code64
long_entry:
    movw    $0x10, %ax             /* data segment */
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %ss
    movw    %ax, %fs
    movw    %ax, %gs

    movq    $__stack_top, %rsp     /* yığın (16-hizalı linker'da) */

    call    main                   /* KEMGU kernel main() */

.global _halt
_halt:
    hlt
    jmp     _halt

/* === GDT (long mode: null + 64-bit code + data) === */
.section .rodata
.align 8
gdt:
    .quad   0x0000000000000000     /* null */
    .quad   0x00209A0000000000     /* 0x08: 64-bit code (L, present, code, exec/read) */
    .quad   0x0000920000000000     /* 0x10: data (present, data, read/write) */
gdt_end:
gdt_desc:
    .word   gdt_end - gdt - 1      /* limit */
    .long   gdt                    /* base (32-bit lgdt; gdt < 4GB) */

/* === Sayfa tabloları (.bss, 4KB-hizalı; boot'ta sıfırlanıp kurulur) === */
.section .bss
.align 4096
pml4:   .skip 4096
pdpt:   .skip 4096
pd:     .skip 4096
