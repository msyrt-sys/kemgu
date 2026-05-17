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
