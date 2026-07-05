/*
 * KEMGU Bare-Metal Boot — ARM64 _start
 * =====================================
 *
 * Linker script (linker/bare-metal-aarch64.ld) ile birlikte kullanılır.
 * Görevler:
 *   1. Yığın işaretçisini (SP) kur — __stack_top sembolü
 *   2. .bss bölümünü sıfırla — __bss_start, __bss_end sembolleri
 *   3. main() çağır
 *   4. main dönerse sonsuz halt döngüsü
 *
 * Tek-çekirdek varsayımı: SMP başlatma yapılmaz; ikinci ve sonraki CPU
 * çekirdekleri (varsa) firmware tarafından henüz uyandırılmamış kabul
 * edilir. (QEMU virt -smp 1 ile çalıştırılır.)
 *
 * Kayıt kullanımı: x0-x3 geçici. x29/x30 ABI gereği prologue/epilogue
 * korur ama _start kalıcı değil; basit JSR + halt yeter.
 */

.section .text._start, "ax"
.global _start
.type _start, %function

_start:
    /* 1. Stack pointer — linker'ın hesapladığı __stack_top */
    adrp    x0, __stack_top
    add     x0, x0, :lo12:__stack_top
    mov     sp, x0

    /* 1b. FP/SIMD trap'ini kapat — clang -O2 C runtime'da 128-bit q-register
     * (örn. 16-baytlık struct move → `ldr q0`) emit eder; trap'liyse senkron
     * exception → vektör tablosu yok → çöküş. Başladığımız EL'e göre doğru
     * register: QEMU virt EL2'de başlatır (CPTR_EL2.TFP); ham EL1 board ise
     * CPACR_EL1.FPEN. İkisini de uygun şekilde ayarla. (Zorunlu bring-up.) */
    mrs     x1, CurrentEL           /* CurrentEL[3:2] = EL; EL2 → 0b1000 = 8 */
    cmp     x1, #8
    b.ne    3f
    mrs     x0, cptr_el2            /* EL2: CPTR_EL2.TFP (bit10) = 0 → trap kapalı */
    bic     x0, x0, #(1 << 10)
    msr     cptr_el2, x0
3:
    mov     x0, #(3 << 20)          /* EL1: CPACR_EL1.FPEN = 0b11 (bit[21:20]) */
    msr     cpacr_el1, x0
    isb

    /* 1c. Exception vektör tablosu kur (VBAR) — C3a. Aksi halde fault → 0x0
     * (vektör yok) → sessiz çöküş; kurulduğunda kdl_istisna_isle "ISTISNA" basar.
     * EL'e göre VBAR_EL2 (QEMU virt) veya VBAR_EL1 (ham board). */
    adrp    x0, kdl_vektor_tablosu
    add     x0, x0, :lo12:kdl_vektor_tablosu
    mrs     x1, CurrentEL
    cmp     x1, #8
    b.ne    4f
    msr     vbar_el2, x0
    b       5f
4:
    msr     vbar_el1, x0
5:
    isb

    /* 2. .bss bölümünü sıfırla */
    adrp    x0, __bss_start
    add     x0, x0, :lo12:__bss_start
    adrp    x1, __bss_end
    add     x1, x1, :lo12:__bss_end
1:
    cmp     x0, x1
    b.hs    2f                  /* x0 >= x1 ise bitir */
    str     xzr, [x0], #8       /* *x0 = 0; x0 += 8 */
    b       1b
2:

    /* 2b. MMU'yu aç (C8a) — identity map, RAM Normal-WB cacheable. SIMD + cache
     * + sanal-bellek/process-izolasyon temeli. main MMU-on koşar. */
    bl      kdl_mmu_kur

    /* 3. main() çağır — ABI gereği X0 dönüşü */
    bl      main

    /* 4. main dönerse halt — interrupt + restart olmadan sonsuz dön */
.global _halt
_halt:
    wfe                         /* event bekle, low-power spin */
    b       _halt

.size _start, . - _start

/* === Exception vektör tablosu (C3a) ===
 * ARMv8: 16 giriş × 0x80 bayt, 2KB-hizalı (VBAR gereği). 4 grup (Cur-SP0,
 * Cur-SPx, Lower-a64, Lower-a32) × 4 tip (sync/IRQ/FIQ/SError). Her giriş tipi
 * x0'a koyup ortak işleyiciye dallanır. IRQ (tip 5/9/...) C3b'de ayrılacak;
 * şimdilik hepsi istisna-işleyiciye (yazdır+halt) gider. */
.section .text
.balign 0x800
.global kdl_vektor_tablosu
kdl_vektor_tablosu:
/* D-121/D-122: EXC vektör stub'u da x0'ı bozmamalı. SVC'de x0 = syscall arg0;
 * eski `mov x0,#\tip` onu (vektör-indeksiyle) yok ediyordu → userspace syscall
 * argümanları kaybolurdu. Doğrudan dallan; kdl_exc_ortak fault-yolu için "tip"i
 * ESR.EC'den türetir (kdl_istisna_isle'a x9=EC geçer), SVC-yolu x0'ı korur. */
.macro VEKTOR_EXC tip
    .balign 0x80
    b       kdl_exc_ortak
.endm
/* C7b/D-121 KRİTİK: IRQ vektör stub'u x0'ı KESİNLİKLE bozmamalı. kdl_irq_ortak
 * trap-frame'i kaydetmeden ÖNCE çalışır ve kesilen görevin x0'ı henüz kaydedilmedi.
 * Eski `mov x0,#\tip` (tip = vektör indeksi, ör. Cur-EL-SPx IRQ=5) preempt edilen
 * görevin canlı x0'ını yok ediyordu → görev x0=5 ile sürüyor → ham-pointer'lı kod
 * (ör. kanal `gonder` spin'i) çöküyordu. kdl_irq_ortak tip'i KULLANMAZ (IRQ id'yi
 * GICC_IAR'dan okur, x0'ı `mov x0,sp` ile ezer) → tip gereksiz. Doğrudan dallan. */
.macro VEKTOR_IRQ tip
    .balign 0x80
    b       kdl_irq_ortak
.endm
    VEKTOR_EXC 0    /* 0x000 Cur EL SP0   : sync   */
    VEKTOR_IRQ 1    /* 0x080              : IRQ    */
    VEKTOR_EXC 2    /* 0x100              : FIQ    */
    VEKTOR_EXC 3    /* 0x180              : SError */
    VEKTOR_EXC 4    /* 0x200 Cur EL SPx   : sync   */
    VEKTOR_IRQ 5    /* 0x280              : IRQ  ← timer buraya */
    VEKTOR_EXC 6    /* 0x300              : FIQ    */
    VEKTOR_EXC 7    /* 0x380              : SError */
    VEKTOR_EXC 8    /* 0x400 Lower EL a64 : sync   */
    VEKTOR_IRQ 9    /* 0x480              : IRQ    */
    VEKTOR_EXC 10   /* 0x500              : FIQ    */
    VEKTOR_EXC 11   /* 0x580              : SError */
    VEKTOR_EXC 12   /* 0x600 Lower EL a32 : sync   */
    VEKTOR_IRQ 13   /* 0x680              : IRQ    */
    VEKTOR_EXC 14   /* 0x700              : FIQ    */
    VEKTOR_EXC 15   /* 0x780              : SError */

/* Ortak istisna işleyici: ESR/ELR oku (current EL'e göre — QEMU virt EL1'de
 * koşar; ham board EL2 olabilir), C işleyiciye geç (yazdır + halt).
 * KRİTİK: yanlış-EL sistem register'ı (esr_el2 @ EL1) UNDEF → işleyici kendini
 * sonsuz fault'lar. Bu yüzden EL-duyarlı. */
/* D-126 KRİTİK: FRAME'İ ÖNCE KAYDET, SONRA ESR oku/dispatch et. Eski sıra (önce
 * ESR oku → x1/x2/x3/x9 klobber → sonra kdl_svc_ortak frame kaydet) SVC çağıranın
 * x1/x2/x3/x9'unu YOK EDİYORDU: kdl_exc_ortak EC'yi x9'a yazıyor (`lsr x9,x1,#26`;
 * SVC için EC=0x15), kaydetmeden önce → syscall ABI register-şeffaf DEĞİLDİ (canlı
 * pointer x9'da hayatta kalamaz; çok-argümanlı syscall x1'i kaybederdi). Şimdi frame
 * PRISTINE kaydedilir → tüm çağıran register'ları korunur (yalnız x0=dönüş değişir). */
kdl_exc_ortak:
    sub     sp, sp, #(16*10)
    stp     x0, x1,   [sp, #16*0]
    stp     x2, x3,   [sp, #16*1]
    stp     x4, x5,   [sp, #16*2]
    stp     x6, x7,   [sp, #16*3]
    stp     x8, x9,   [sp, #16*4]
    stp     x10, x11, [sp, #16*5]
    stp     x12, x13, [sp, #16*6]
    stp     x14, x15, [sp, #16*7]
    stp     x16, x17, [sp, #16*8]
    stp     x18, x30, [sp, #16*9]

    /* ESR/ELR oku (EL-duyarlı — QEMU virt EL1; ham board EL2). Frame kaydedildi →
     * x1/x2/x3/x9 klobber artık güvenli (orijinaller yığında). */
    mrs     x3, CurrentEL
    cmp     x3, #8
    b.ne    7f
    mrs     x1, esr_el2         /* EL2 */
    mrs     x2, elr_el2
    b       8f
7:
    mrs     x1, esr_el1         /* EL1 (QEMU virt) */
    mrs     x2, elr_el1
8:
    lsr     x9, x1, #26         /* EC = ESR[31:26] */
    cmp     x9, #0x15           /* SVC (aarch64 same-EL) → sistem çağrısı (C6) */
    b.eq    9f
    /* --- Fault (PART 1b / C8c): KONTROLLÜ KURTARMA. kdl_fault_bekleniyor set ise
     * FAR yakala + faulting instr'ı ATLA (ELR+4) + frame restore + eret → OS DEVAM.
     * Bayrak yoksa → yazdır + halt (eski davranış). x1=ESR, x2=ELR(EL-correct), x9=EC.
     * Saf asm (bl YOK) → EL-correct x1/x2 klobber edilmez. --- */
    adrp    x3, kdl_fault_bekleniyor
    add     x3, x3, :lo12:kdl_fault_bekleniyor
    ldr     x4, [x3]
    cbz     x4, 6f              /* beklenmiyor → normal fault (halt) */
    /* Kurtar: FAR yakala (EL-duyarlı) → kdl_fault_yakalanan. */
    mrs     x5, CurrentEL
    cmp     x5, #8
    b.ne    16f
    mrs     x7, far_el2
    b       17f
16:
    mrs     x7, far_el1
17:
    adrp    x6, kdl_fault_yakalanan
    add     x6, x6, :lo12:kdl_fault_yakalanan
    str     x7, [x6]            /* kdl_fault_yakalanan = FAR */
    str     xzr, [x3]           /* kdl_fault_bekleniyor = 0 */
    add     x2, x2, #4          /* faulting instr'ı atla (ELR += 4) */
    mrs     x5, CurrentEL
    cmp     x5, #8
    b.ne    18f
    msr     elr_el2, x2
    b       19f
18:
    msr     elr_el1, x2
19:
    /* frame restore (SVC yolu ile aynı) + eret → kesilen kod bir-sonraki instr'dan sürer */
    ldp     x0, x1,   [sp, #16*0]
    ldp     x2, x3,   [sp, #16*1]
    ldp     x4, x5,   [sp, #16*2]
    ldp     x6, x7,   [sp, #16*3]
    ldp     x8, x9,   [sp, #16*4]
    ldp     x10, x11, [sp, #16*5]
    ldp     x12, x13, [sp, #16*6]
    ldp     x14, x15, [sp, #16*7]
    ldp     x16, x17, [sp, #16*8]
    ldp     x18, x30, [sp, #16*9]
    add     sp, sp, #(16*10)
    eret
6:
    /* --- PART 3(c): EL0 izolasyon-ihlali mi? Fault EL0'dan geldiyse (SPSR.M[3:2]=0)
     * → ihlal eden SÜRECİ ÖLDÜR + OS DEVAM (gerçek process izolasyonu). EL1 fault ise
     * → teşhis + halt (eski). Kernel EL1'de → spsr/far/elr_el1. --- */
    mrs     x5, spsr_el1
    ubfx    x6, x5, #2, #2      /* SPSR.M[3:2] = kaynak istisna-öncesi EL */
    cbnz    x6, 21f             /* kaynak EL != 0 → EL1 fault → halt (normal) */
    /* EL0 fault. OPT-IN: kdl_el0_kill_aktif set DEĞİLSE eski davranış (halt/ISTISNA) —
     * mevcut EL0 demoları REGRESSION YAŞAMAZ. Set ise → süreç-öldürme (entegre çekirdek). */
    adrp    x5, kdl_el0_kill_aktif
    add     x5, x5, :lo12:kdl_el0_kill_aktif
    ldr     x6, [x5]
    cbz     x6, 21f             /* kill kapalı → halt (eski ISTISNA davranışı) */
    /* EL0 fault + kill AÇIK = izolasyon ihlali: FAR'ı arg0'a al, öldür-işleyiciyi çağır. */
    mrs     x0, far_el1
    bl      kdl_el0_izolasyon_isle   /* rapor + kdl_gorev_bitir (görevi ölü işaretle) */
    /* Faulting instr'ı atla + frame restore + eret → ölü EL0 görev kısa sürer,
     * sonraki timer-IRQ'da scheduler onu ATLAR → OS (kabuk + diğer görevler) devam. */
    mrs     x2, elr_el1
    add     x2, x2, #4
    msr     elr_el1, x2
    ldp     x0, x1,   [sp, #16*0]
    ldp     x2, x3,   [sp, #16*1]
    ldp     x4, x5,   [sp, #16*2]
    ldp     x6, x7,   [sp, #16*3]
    ldp     x8, x9,   [sp, #16*4]
    ldp     x10, x11, [sp, #16*5]
    ldp     x12, x13, [sp, #16*6]
    ldp     x14, x15, [sp, #16*7]
    ldp     x16, x17, [sp, #16*8]
    ldp     x18, x30, [sp, #16*9]
    add     sp, sp, #(16*10)
    eret
21:
    /* --- EL1 fault (kaynak EL != 0): kurtarma yok → teşhis + halt. tip=EC(x9→x0). --- */
    mov     x0, x9
    bl      kdl_istisna_isle    /* noreturn (yazdır + halt) */
    b       21b                 /* güvenlik: noreturn ihlal edilirse geri dön */
9:
    /* --- SVC: frame kaydedildi; dispatch. num/arg SAKLANANDAN oku (canlı reg
     * ESR-okuma sırasında klobber edilmiş olabilir → yığından pristine al). --- */
    ldr     x1, [sp, #16*0]     /* arg0 = saklanan x0 (2. C param) */
    ldr     x2, [sp, #8]        /* arg1 = saklanan x1 (3. C param) — D-131 2-arg syscall */
    ldr     x0, [sp, #16*4]     /* num  = saklanan x8 (offset 16*4 = x8 slotu; 1. C param) */
    bl      kdl_syscall_isle    /* (num, arg0, arg1) → dönüş x0 */
    str     x0, [sp, #16*0]     /* dönüş değeri → saklanan x0 slotu (EL0 x0'da alır) */

    ldp     x0, x1,   [sp, #16*0]
    ldp     x2, x3,   [sp, #16*1]
    ldp     x4, x5,   [sp, #16*2]
    ldp     x6, x7,   [sp, #16*3]
    ldp     x8, x9,   [sp, #16*4]
    ldp     x10, x11, [sp, #16*5]
    ldp     x12, x13, [sp, #16*6]
    ldp     x14, x15, [sp, #16*7]
    ldp     x16, x17, [sp, #16*8]
    ldp     x18, x30, [sp, #16*9]
    add     sp, sp, #(16*10)
    eret

/* === IRQ işleyici (C3b) — bağlamı kaydet, GICC_IAR oku, dispatch, EOIR, eret ===
 * İstisnadan farkı: KURTARILIR (eret) → kesilen koda dönülür, caller-saved
 * register'lar korunur. C kdl_kesme_isle x0-x18,x30 clobber eder; x19-x29
 * callee-saved (C korur), kaydetmeye gerek yok. */
/* C7b: FULL trap frame (x0-x30 + ELR + SPSR = 272 bayt) kaydet → kdl_irq_isle
 * (IAR/tik/EOI + preempt) DEVAM SP'sini döner → SP swap (preempt'te sonraki
 * görevin trap-frame'i) → restore → eret. Preempt kapalıysa SP aynı → eski
 * davranış (timer testi nötr). Görev trap-frame'i: x30@240, ELR@248, SPSR@256. */
kdl_irq_ortak:
    sub     sp, sp, #272
    stp     x0, x1,   [sp, #16*0]
    stp     x2, x3,   [sp, #16*1]
    stp     x4, x5,   [sp, #16*2]
    stp     x6, x7,   [sp, #16*3]
    stp     x8, x9,   [sp, #16*4]
    stp     x10, x11, [sp, #16*5]
    stp     x12, x13, [sp, #16*6]
    stp     x14, x15, [sp, #16*7]
    stp     x16, x17, [sp, #16*8]
    stp     x18, x19, [sp, #16*9]
    stp     x20, x21, [sp, #16*10]
    stp     x22, x23, [sp, #16*11]
    stp     x24, x25, [sp, #16*12]
    stp     x26, x27, [sp, #16*13]
    stp     x28, x29, [sp, #16*14]
    mrs     x9, elr_el1
    mrs     x10, spsr_el1
    stp     x30, x9,  [sp, #16*15]    /* x30 @240, ELR @248 */
    str     x10,      [sp, #256]      /* SPSR @256 */
    mrs     x11, sp_el0              /* D-125: EL0 görev banked user-SP'si */
    str     x11,      [sp, #264]      /* SP_EL0 @264 (EL1 görevde kullanılmaz — zararsız) */

    mov     x0, sp                   /* arg0 = trap-frame ptr */
    bl      kdl_irq_isle             /* devam SP'si → x0 (preempt veya aynı) */
    mov     sp, x0

    ldr     x10,      [sp, #256]      /* SPSR */
    ldp     x30, x9,  [sp, #16*15]    /* x30, ELR */
    msr     spsr_el1, x10
    msr     elr_el1,  x9
    ldr     x9,       [sp, #264]      /* D-125: SP_EL0 (EL0 görev user-SP restore) */
    msr     sp_el0,   x9             /* x9 aşağıda ldp ile yeniden yüklenir — scratch */
    ldp     x0, x1,   [sp, #16*0]
    ldp     x2, x3,   [sp, #16*1]
    ldp     x4, x5,   [sp, #16*2]
    ldp     x6, x7,   [sp, #16*3]
    ldp     x8, x9,   [sp, #16*4]
    ldp     x10, x11, [sp, #16*5]
    ldp     x12, x13, [sp, #16*6]
    ldp     x14, x15, [sp, #16*7]
    ldp     x16, x17, [sp, #16*8]
    ldp     x18, x19, [sp, #16*9]
    ldp     x20, x21, [sp, #16*10]
    ldp     x22, x23, [sp, #16*11]
    ldp     x24, x25, [sp, #16*12]
    ldp     x26, x27, [sp, #16*13]
    ldp     x28, x29, [sp, #16*14]
    add     sp, sp, #272
    eret

/* === Cooperative bağlam-değiştirme (C7a) — kdl_baglam_degis(x0=eski, x1=yeni) ===
 * Callee-saved (x19-x28, x29, x30) + sp kaydet/yükle. ret → yeni görev kaldığı
 * yerden sürer. TCB: [0..9]=x19-x28, [10]=x29, [11]=x30, [12]=sp (×8 bayt). */
.global kdl_baglam_degis
kdl_baglam_degis:
    stp     x19, x20, [x0, #0]
    stp     x21, x22, [x0, #16]
    stp     x23, x24, [x0, #32]
    stp     x25, x26, [x0, #48]
    stp     x27, x28, [x0, #64]
    stp     x29, x30, [x0, #80]
    mov     x2, sp
    str     x2, [x0, #96]

    ldp     x19, x20, [x1, #0]
    ldp     x21, x22, [x1, #16]
    ldp     x23, x24, [x1, #32]
    ldp     x25, x26, [x1, #48]
    ldp     x27, x28, [x1, #64]
    ldp     x29, x30, [x1, #80]
    ldr     x2, [x1, #96]
    mov     sp, x2
    ret

/* === EL0'a düş + kullanıcı kodu çalıştır (D2) — kdl_el0_calistir(x0=fn, x1=sp) ===
 * SP_EL0 = EL0 yığını, ELR_EL1 = fn, SPSR_EL1 = EL0t (M[3:0]=0; DAIF maskeli).
 * eret → EL0. fn yalnız SVC ile EL1'e geçebilir (device + kernel sayfaları
 * EL1-only → doğrudan erişim permission-fault). Privilege sınırı. */
.global kdl_el0_calistir
kdl_el0_calistir:
    msr     sp_el0, x1
    msr     elr_el1, x0
    mov     x2, #0x3C0          /* SPSR: M=EL0t, DAIF=1111 (IRQ maskeli) */
    msr     spsr_el1, x2
    eret
