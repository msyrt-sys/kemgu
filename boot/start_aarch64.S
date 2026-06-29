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
