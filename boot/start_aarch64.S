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
.macro VEKTOR tip
    .balign 0x80
    mov     x0, #\tip
    b       kdl_exc_ortak
.endm
    VEKTOR 0        /* 0x000 Cur EL SP0   : sync   */
    VEKTOR 1        /* 0x080              : IRQ    */
    VEKTOR 2        /* 0x100              : FIQ    */
    VEKTOR 3        /* 0x180              : SError */
    VEKTOR 4        /* 0x200 Cur EL SPx   : sync   */
    VEKTOR 5        /* 0x280              : IRQ    */
    VEKTOR 6        /* 0x300              : FIQ    */
    VEKTOR 7        /* 0x380              : SError */
    VEKTOR 8        /* 0x400 Lower EL a64 : sync   */
    VEKTOR 9        /* 0x480              : IRQ    */
    VEKTOR 10       /* 0x500              : FIQ    */
    VEKTOR 11       /* 0x580              : SError */
    VEKTOR 12       /* 0x600 Lower EL a32 : sync   */
    VEKTOR 13       /* 0x680              : IRQ    */
    VEKTOR 14       /* 0x700              : FIQ    */
    VEKTOR 15       /* 0x780              : SError */

/* Ortak istisna işleyici: ESR/ELR oku (current EL'e göre — QEMU virt EL1'de
 * koşar; ham board EL2 olabilir), C işleyiciye geç (yazdır + halt).
 * KRİTİK: yanlış-EL sistem register'ı (esr_el2 @ EL1) UNDEF → işleyici kendini
 * sonsuz fault'lar. Bu yüzden EL-duyarlı. */
kdl_exc_ortak:
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
    bl      kdl_istisna_isle    /* noreturn (yazdır + halt) */
6:
    wfe
    b       6b
