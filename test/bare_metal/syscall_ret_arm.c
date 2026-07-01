/*
 * D-126 testi (aarch64) — syscall DÖNÜŞ değeri ABI (kernel → EL0 sonuç döndürür).
 *
 * el0_kod EL0'da: sys(9, 41) çağırır. Kernel 'artir' syscall'ı arg+1=42 hesaplar
 * ve x0'da GERİ DÖNDÜRÜR (kdl_svc_ortak dönüşü saved-x0 slotuna yazar → restore →
 * EL0 çağıran x0'da alır). Sonuç 42 ise "SYSCALL RET OK".
 *
 * Bu, userspace ABI'nin eksik yarısı: şimdiye dek syscall'lar yalnız argüman ALDI
 * (D-122); artık DEĞER DÖNDÜRÜR. read/getpid/gettick/time ailesinin mekanizması.
 *
 * Kanıt: "SYSCALL RET OK" (kernel 41→42 hesabı EL0'a döndü).
 */
#include <stdint.h>
extern void kdl_el0_calistir(void (*fn)(void), void *sp);
extern unsigned char __user_stack_top[];

/* Userspace syscall sarmalayıcı — dönüş değeri x0'da. always_inline → SVC .user'a gömülü. */
__attribute__((always_inline)) static inline unsigned long sys(unsigned long num, unsigned long arg) {
    register unsigned long x8 __asm__("x8") = num;
    register unsigned long x0 __asm__("x0") = arg;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8) : "memory");
    return x0;
}

__attribute__((section(".user"), noinline))
static void el0_kod(void) {
    unsigned long sonuc = sys(9, 41);                /* kernel 'artir': 41 → 42 (dönüş) */
    sys(5, (unsigned long)(uintptr_t)(sonuc == 42 ? "SYSCALL RET OK" : "SYSCALL RET HATA"));
    sys(7, 0);                                       /* satir */
    sys(3, 0);                                       /* cik → kernel'de dur */
    for (;;) { }
}

int main(void) {
    kdl_el0_calistir(el0_kod, __user_stack_top);
    return 0;   /* ulaşılmaz */
}
