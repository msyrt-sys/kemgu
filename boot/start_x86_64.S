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
 *      Ayrıca 3..4 GB UC MMIO penceresi (pd_mmio: 0xC0000000..0xFFFFFFFF,
 *      PWT|PCD ile önbelleksiz) + paging → long mode; 64-bit kod segmentine far jump
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
    /* PVH ABI: QEMU 32-bit pvh_entry'ye atlarken %ebx = struct hvm_start_info
     * fiziksel adresi (aarch64 _start X0 DTB ikizi). İLK İŞ: BSS temizliğinden
     * ÖNCE sakla — rep stosb %ebx'i bozar. Yüksek dword 0 kalır (adres < 4 GB). */
    movl    %ebx, kdl_pvh_bilgi

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

    /* 2c2. 3..4 GB UC MMIO penceresi: pd_mmio 512 × 2MB, taban 0xC0000000,
     * bayrak 0x9B = P|RW|PWT|PCD|PS → önbelleksiz (UC). NEDEN UC: bu aralık
     * MMIO'dur — LAPIC (0xFEE00000), IOAPIC (0xFEC00000), microvm virtio-mmio
     * (0xFEB00000). Yan etkili okuma/yazmalar önbelleğe alınır veya yazma-
     * birleştirilirse cihaz durumu bayatlar / kaybolur. Bu pencere 0..1GB
     * PD'sinden AYRI tutulur (farklı bayrak) → PDPT[3]'e bağlanır. */
    movl    $pd_mmio, %edi
    movl    $0xC000009B, %eax      /* present|write|PWT|PCD|PS, taban 0xC0000000 */
    movl    $512, %ecx
1:
    movl    %eax, (%edi)           /* düşük 32 bit */
    movl    $0, 4(%edi)            /* yüksek 32 bit = 0 */
    addl    $0x200000, %eax        /* sonraki 2MB */
    addl    $8, %edi
    loop    1b

    /* PDPT[3] = pd_mmio | present|write (pdpt+24; yüksek dword pdpt+28). */
    movl    $pd_mmio, %eax
    orl     $0x3, %eax
    movl    %eax, pdpt+24
    movl    $0, pdpt+28

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

    call    kdl_idt_kur            /* C3a: IDT kur (isr0..31 → istisna işleyici) */

    /* 2c (aarch64 eşi): heap penceresini main'den ÖNCE kur. Saf-.kem allocator
     * (runtime/kem_heap.kem) kem_bump/kem_son'u bu çağrıdan alır; çağrılmazsa
     * kem_son=0 kalır → her @malloc null döner → dizi işlevleri sessizce 0
     * basar, panik YOK. kem_heap.kem ile bağlanMAyan C-malloc'lu x86
     * kernel'lerinde runtime/kdl_bare_heap.c'deki ZAYIF kem_heap_kur no-op'u
     * devreye girer (strong .kem sürümü override eder) → eski kernel'ler
     * bozulmaz. */
    movq    $__heap_start, %rdi    /* 1. argüman: heap tabanı (linker sembolü) */
    movq    $__heap_end, %rsi      /* 2. argüman: heap üst sınırı (dışlayıcı) */
    call    kem_heap_kur

    call    main                   /* KEMGU kernel main() */

.global _halt
_halt:
    hlt
    jmp     _halt

/* === GDT (long mode: null + çekirdek kod/veri + [D-695] kullanıcı kod/veri + TSS) ===
 * .data'da (eskiden .rodata): (1) TSS tanımlayıcısı çalışma anında .kem
 * kxr_tss_kur tarafından doldurulur; (2) CPU `ltr`de TSS tanımlayıcısına MEŞGUL
 * bitini, segment yüklerken erişildi bitini YAZAR — D-691 çekirdek W^X'ten sonra
 * .rodata salt-okunurdur ve bu örtük yazmalar #PF verirdi. */
.section .data
.align 16
.global kdl_x86_gdt
kdl_x86_gdt:
gdt:
    .quad   0x0000000000000000     /* null */
    .quad   0x00209A0000000000     /* 0x08: 64-bit code (L, present, code, exec/read) */
    .quad   0x0000920000000000     /* 0x10: data (present, data, read/write) */
    .quad   0x0020FA0000000000     /* 0x18: [D-695] kullanıcı kod (DPL3, L) → CS=0x1B */
    .quad   0x0000F20000000000     /* 0x20: [D-695] kullanıcı veri (DPL3, r/w) → SS=0x23 */
    .quad   0, 0                   /* 0x28: [D-695] 64-bit TSS (16 B), kxr_tss_kur doldurur */
gdt_end:
gdt_desc:
    .word   gdt_end - gdt - 1      /* limit */
    .long   gdt                    /* base (32-bit lgdt; gdt < 4GB) */

/* === PVH start_info adresi (.data, boot'ta sıfırlanmaz) ===
 * .data'da çünkü .bss boot'ta SIFIRLANIR (aarch64 kdl_dtb_adres ile aynı gerekçe);
 * yüksek dword 0 kalır (adres < 4 GB). kem_pvh.kem bu sembolü %rip-relatif okur. */
.section .data
.align 8
.global kdl_pvh_bilgi
kdl_pvh_bilgi:
    .quad 0

/* === Sayfa tabloları (.bss, 4KB-hizalı; boot'ta sıfırlanıp kurulur) === */
.section .bss
.align 4096
pml4:   .skip 4096
pdpt:   .skip 4096
pd:     .skip 4096
pd_mmio: .skip 4096

/* [D-695] 64-bit TSS (104 B). Yalnız RSP0 kullanılır: ring3'ten gelen kesme/
 * istisnada CPU ring0 yığınını buradan alır (TSS'siz ring3→ring0 = üçlü hata).
 * RSP0 kdl_x86_ring3_gir'de, IOPB ofseti + tanımlayıcı .kem kxr_tss_kur'da. */
.align 16
.global kdl_x86_tss
kdl_x86_tss: .skip 104

/* === x86 exception ISR stub'ları (C3a) ===
 * Hata-kodlu istisnalar (8,10,11,12,13,14,17,21) CPU error code push eder → yalnız
 * vektör no push (ISR_ERR). Diğerleri dummy 0 + vektör (ISR_NOERR). Long mode'da
 * same-privilege exception bile SS:RSP:RFLAGS:CS:RIP push eder → ortak yığın:
 *   0(rsp)=vektör  8(rsp)=hata_kodu  16(rsp)=RIP  → isr_ortak → kdl_istisna_isle. */
.section .text
.code64
.macro ISR_NOERR n
isr\n:
    pushq   $0          /* dummy hata kodu */
    pushq   $\n
    jmp     isr_ortak
.endm
.macro ISR_ERR n
isr\n:
    pushq   $\n         /* hata kodu CPU'dan zaten yığında */
    jmp     isr_ortak
.endm

    ISR_NOERR 0
    ISR_NOERR 1
    ISR_NOERR 2
    ISR_NOERR 3
    ISR_NOERR 4
    ISR_NOERR 5
    ISR_NOERR 6
    ISR_NOERR 7
    ISR_ERR   8
    ISR_NOERR 9
    ISR_ERR   10
    ISR_ERR   11
    ISR_ERR   12
    ISR_ERR   13
    ISR_ERR   14
    ISR_NOERR 15
    ISR_NOERR 16
    ISR_ERR   17
    ISR_NOERR 18
    ISR_NOERR 19
    ISR_NOERR 20
    ISR_ERR   21
    ISR_NOERR 22
    ISR_NOERR 23
    ISR_NOERR 24
    ISR_NOERR 25
    ISR_NOERR 26
    ISR_NOERR 27
    ISR_NOERR 28
    ISR_NOERR 29
    ISR_NOERR 30
    ISR_NOERR 31

isr_ortak:
    /* [D-695] RING3 DÖNÜŞÜ: kdl_x86_r3_blok+0 (bekle) != 0 VE kesilen CS'in RPL'i 3
     * ise istisna KULLANICI kodundan gelmiştir (yığın: vektör@0 hata@8 RIP@16 CS@24).
     * Kullanıcı rax/rcx + vektör/hata/RIP/CS/CR2 kaydedilir, bayrak SIFIRLANIR (tek
     * atış), çekirdek bağlamına (kdl_x86_ring3_gir'in çağıranına) dönülür. Kesilen
     * çerçeve ATILIR (ring3'e geri dönülmez). Bayrak 0 ya da çekirdek (CS RPL 0)
     * fault'u → aşağıdaki eski yollar AYNEN: çekirdek fault'u asla yutulmaz. */
    cmpq    $0, kdl_x86_r3_blok(%rip)
    je      5f
    testb   $3, 24(%rsp)
    jz      5f
    movq    %rax, kdl_x86_r3_blok+48(%rip)     /* kullanıcı rax */
    movq    %rcx, kdl_x86_r3_blok+56(%rip)     /* kullanıcı rcx */
    movq    0(%rsp), %rax
    movq    %rax, kdl_x86_r3_blok+16(%rip)     /* vektör */
    movq    8(%rsp), %rax
    movq    %rax, kdl_x86_r3_blok+24(%rip)     /* hata kodu */
    movq    16(%rsp), %rax
    movq    %rax, kdl_x86_r3_blok+32(%rip)     /* RIP */
    movq    24(%rsp), %rax
    movq    %rax, kdl_x86_r3_blok+40(%rip)     /* CS */
    movq    %cr2, %rax
    movq    %rax, kdl_x86_r3_blok+64(%rip)     /* CR2 */
    movq    $0, kdl_x86_r3_blok(%rip)
    jmp     kdl_x86_r3_don
5:
    /* [D-687] KURTARILABİLİR #PF (aarch64 kdl_fault_bekleniyor ikizi): vektör 14 VE
     * kdl_x86_pf_bekle != 0 ise fault BEKLENİYORDUR (.kem kmmu_fault_testi). bekle =
     * fault eden komutun BAYT UZUNLUĞU (x86 değişken uzunluk; .kem sabit-yazmaçlı
     * `movq (%rdi), %rax` = 3 bayt). CR2 kaydedilir, bayrak SIFIRLANIR (tek atış),
     * kaydedilmiş RIP += bekle, vektör+hata kodu atılır, iretq. Bayrak 0 ise eski yol
     * (kdl_istisna_isle → halt) AYNEN: beklenmeyen fault asla sessizce yutulmaz. */
    cmpq    $14, 0(%rsp)
    jne     2f
    cmpq    $0, kdl_x86_pf_bekle(%rip)
    je      2f
    pushq   %rax
    movq    %cr2, %rax
    movq    %rax, kdl_x86_pf_cr2(%rip)
    /* [D-689] hata kodunu da kaydet (yığın: rax@0 vektör@8 hata@16 RIP@24 CS@32
     * RFLAGS@40 RSP@48 SS@56). Bit 4 (I/D) = komut getirme hatası (NX). */
    movq    16(%rsp), %rax
    movq    %rax, kdl_x86_pf_hata(%rip)
    movq    kdl_x86_pf_bekle(%rip), %rax
    cmpq    $65535, %rax
    jne     3f
    /* [D-689] bekle == 65535: RET TAKLİDİ. Komut getirme hatasında RIP fault eden
     * hedefin KENDİSİDİR (atlanacak komut yok) — `call` ile girilen hedef için
     * dönüş adresi kesilen yığının tepesindedir: RIP = [RSP], RSP += 8. */
    pushq   %rbx                    /* bundan sonra ofsetler +8 */
    movq    56(%rsp), %rax          /* kesilen RSP */
    movq    (%rax), %rbx            /* dönüş adresi */
    movq    %rbx, 32(%rsp)          /* RIP */
    addq    $8, 56(%rsp)            /* RSP += 8 */
    popq    %rbx
    jmp     4f
3:
    addq    %rax, 24(%rsp)          /* RIP (rax itmesiyle +8 kaydı: 16+8) */
4:
    movq    $0, kdl_x86_pf_bekle(%rip)
    popq    %rax
    addq    $16, %rsp               /* vektör + hata kodu */
    iretq
2:
    movq    0(%rsp), %rdi    /* vektör no */
    movq    8(%rsp), %rsi    /* hata kodu */
    movq    16(%rsp), %rdx   /* RIP */
    call    kdl_istisna_isle /* noreturn (yazdır + halt) */
1:
    hlt
    jmp     1b

/* === IRQ0 (timer) stub (D-685) — TAM kayıt + SP takası (preemptive görev). ===
 * İstisnadan farkı: KURTARILIR (iretq) → kesilen koda dönülür. Tüm GP yazmaçları
 * (callee + caller) itilir: rax,rbx,rcx,rdx,rsi,rdi,rbp,r8..r15 (15×8 = 120 B).
 * kdl_irq_isle(sp) dönen SP'ye geçilir (başka görevin kesilmiş/sentetik
 * çerçevesi); yazmaçlar geri alınır, iretq. Çerçeve: [0]=r15 … [14]=rax,
 * RIP@120, CS@128, RFLAGS@136, RSP@144, SS@152 (toplam 160 B). CPU kesmede RSP'yi
 * 16'ya hizalar → 5+15 itme = 160 → call öncesi SP 16-hizalı (SysV) ✓.
 * C kernel'leri uyumluluğu: zayıf kdl_irq_isle SP'yi DEĞİŞTİRMEZ → davranış AYNI.
 * PIC EOI = port 0x20'ye 0x20. */
.global kdl_irq0_stub
kdl_irq0_stub:
    pushq   %rax
    pushq   %rbx
    pushq   %rcx
    pushq   %rdx
    pushq   %rsi
    pushq   %rdi
    pushq   %rbp
    pushq   %r8
    pushq   %r9
    pushq   %r10
    pushq   %r11
    pushq   %r12
    pushq   %r13
    pushq   %r14
    pushq   %r15
    movq    $0, %rdi          /* IRQ no = 0 (timer) */
    call    kdl_kesme_isle
    movq    %rsp, %rdi        /* kesilen görevin çerçeve SP'si */
    call    kdl_irq_isle
    movq    %rax, %rsp        /* seçilen görevin çerçeve SP'si */
    movb    $0x20, %al        /* PIC EOI (master) */
    outb    %al, $0x20
    popq    %r15
    popq    %r14
    popq    %r13
    popq    %r12
    popq    %r11
    popq    %r10
    popq    %r9
    popq    %r8
    popq    %rbp
    popq    %rdi
    popq    %rsi
    popq    %rdx
    popq    %rcx
    popq    %rbx
    popq    %rax
    iretq
/* Zayıf varsayılan: .kem x86 güçlü tanımı (kem_preempt) ezer; C kernel'lerinde
 * SP değişmez → davranış AYNI. */
.weak kdl_irq_isle
kdl_irq_isle:
    movq    %rdi, %rax
    ret

/* === Sistem çağrısı stub (C6) — int 0x80; num=rax, arg=rdi. KURTARILIR (iretq). ===
 * kdl_syscall_isle(rdi=num, rsi=arg). rax = dönüş (void demo → kullanılmaz). */
.global kdl_syscall_stub
kdl_syscall_stub:
    pushq   %rcx
    pushq   %rdx
    pushq   %rsi
    pushq   %rdi
    pushq   %r8
    pushq   %r9
    pushq   %r10
    pushq   %r11
    /* [D-695] çağıranın CS'i (8 itme = 64 B; RIP@64, CS@72) → kdl_x86_r3_blok+72.
     * RPL 3 = sistem çağrısı KULLANICI kodundan geldi (ring3 kanıtı). */
    movq    72(%rsp), %r11
    movq    %r11, kdl_x86_r3_blok+72(%rip)
    movq    %rdi, %rsi        /* arg → 2. param */
    movq    %rax, %rdi        /* num → 1. param */
    call    kdl_syscall_isle
    popq    %r11
    popq    %r10
    popq    %r9
    popq    %r8
    popq    %rdi
    popq    %rsi
    popq    %rdx
    popq    %rcx
    iretq

/* === [D-695] RING3 girişi (arm64 kdl_el0_calistir ikizi) ===
 * kdl_x86_ring3_gir(rdi = kullanıcı RIP, rsi = kullanıcı RSP): RFLAGS + callee-
 * saved yazmaçlar kaydedilir, IRQ kapatılır, kaydedilmiş RSP → kdl_x86_r3_blok+8,
 * TSS.RSP0 = bu RSP (16-hizalı; ring3'ten gelen kesme çerçevesi kaydedilen
 * yazmaçların ALTINA itilir), bekle = 1, iretq çerçevesi: SS=0x23, RSP, RFLAGS=0x2
 * (IF=0, IOPL=0 → ring3'te cli/sti/hlt/in/out #GP), CS=0x1B, RIP. iretq CPL'yi 3'e
 * düşürür. Dönüş: ring3'te BİR istisna (ör. ayrıcalıklı komut #GP, gözetmen sayfa
 * #PF) → isr_ortak → kdl_x86_r3_don → buradan normal `ret`. int 0x80 ise ring3'e
 * iretq ile döner (kdl_syscall_stub). */
.section .text
.code64
.global kdl_x86_ring3_gir
kdl_x86_ring3_gir:
    pushfq
    cli
    pushq   %rbx
    pushq   %rbp
    pushq   %r12
    pushq   %r13
    pushq   %r14
    pushq   %r15
    movq    %rsp, kdl_x86_r3_blok+8(%rip)
    movq    %rsp, %rax
    andq    $-16, %rax
    movq    %rax, kdl_x86_tss+4(%rip)          /* TSS.RSP0 (ofset 4) */
    movq    $1, kdl_x86_r3_blok(%rip)
    pushq   $0x23                              /* SS  = kullanıcı veri | 3 */
    pushq   %rsi                               /* RSP = kullanıcı yığını */
    pushq   $0x2                               /* RFLAGS: IF=0, IOPL=0 */
    pushq   $0x1B                              /* CS  = kullanıcı kod | 3 */
    pushq   %rdi                               /* RIP = kullanıcı girişi */
    iretq

/* Ring3 istisnasından çekirdek bağlamına dönüş (isr_ortak'tan jmp). Ring3→ring0
 * geçişi SS'i NULL, iretq ring3'e inerken DS/ES/FS/GS'i NULL yapmıştı → çekirdek
 * veri seçicisi (0x10) yeniden yüklenir. */
kdl_x86_r3_don:
    movq    kdl_x86_r3_blok+8(%rip), %rsp
    movw    $0x10, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %fs
    movw    %ax, %gs
    movw    %ax, %ss
    popq    %r15
    popq    %r14
    popq    %r13
    popq    %r12
    popq    %rbp
    popq    %rbx
    popfq
    ret

.section .data
.align 8
/* [D-695] ring3 durum bloğu (.kem kxr_* tek taban adresle okur):
 * +0 bekle  +8 çekirdek RSP  +16 vektör  +24 hata kodu  +32 RIP  +40 CS
 * +48 kullanıcı rax  +56 kullanıcı rcx  +64 CR2  +72 son sistem çağrısının CS'i */
.global kdl_x86_r3_blok
kdl_x86_r3_blok: .skip 80

.global kdl_x86_pf_bekle
.global kdl_x86_pf_cr2
kdl_x86_pf_bekle: .quad 0     /* [D-687] 0 = fault beklenmiyor; >0 = atlanacak komut uzunluğu */
kdl_x86_pf_cr2:   .quad 0     /* [D-687] son kurtarılan #PF'in CR2'si (fault VA) */
.global kdl_x86_pf_hata
kdl_x86_pf_hata:  .quad 0     /* [D-689] son kurtarılan #PF'in hata kodu */

.section .rodata
.align 8
.global kdl_isr_tablo
kdl_isr_tablo:
    .quad   isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7
    .quad   isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15
    .quad   isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23
    .quad   isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31

/* === Cooperative bağlam-değiştirme (C7a) — kdl_baglam_degis(rdi=eski, rsi=yeni) ===
 * Callee-saved (rbx,rbp,r12-r15) + rsp kaydet/yükle. ret → yeni görev sürer.
 * TCB: [0]=rbx,[1]=rbp,[2..5]=r12-r15,[6]=rsp (×8 bayt). */
.section .text
.code64
.global kdl_baglam_degis
kdl_baglam_degis:
    movq    %rbx, 0(%rdi)
    movq    %rbp, 8(%rdi)
    movq    %r12, 16(%rdi)
    movq    %r13, 24(%rdi)
    movq    %r14, 32(%rdi)
    movq    %r15, 40(%rdi)
    movq    %rsp, 48(%rdi)
    movq    0(%rsi), %rbx
    movq    8(%rsi), %rbp
    movq    16(%rsi), %r12
    movq    24(%rsi), %r13
    movq    32(%rsi), %r14
    movq    40(%rsi), %r15
    movq    48(%rsi), %rsp
    ret
