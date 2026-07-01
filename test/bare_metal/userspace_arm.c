/*
 * D-124 testi (aarch64) — İLK USERSPACE PROGRAMI (EL0 hesap + syscall ABI ile I/O).
 *
 * el0_program EL0'da (userspace) koşar: 1..10 toplamını HESAPLAR (kullanıcı-uzayı
 * hesap), sonra sonucu KERNEL SYSCALL ABI'siyle yazar. Kernel'i doğrudan çağırmaz
 * (kernel sayfaları EL0'a kapalı); yalnız SVC ile:
 *   yaz(5, ptr)     — string yaz (kernel kullanıcı belleğinden ptr okur)
 *   yaz_sayi(6, n)  — sayı yaz
 *   satir(7)        — satır sonu
 *   cik(3)          — süreç bitir (kernel'de dur)
 *
 * Bu, userspace ABI'nin çekirdek kanıtı: userspace program HESAP yapar + kernel
 * hizmetlerini SYSCALL ile kullanır (pointer/veri geçişi dâhil). Faz F temeli.
 *
 * Kanıt: "MERHABA userspace" + "USERSPACE OK toplam=55" (EL0 hesabı, syscall I/O).
 */
#include <stdint.h>
extern void kdl_el0_calistir(void (*fn)(void), void *sp);
extern unsigned char __user_stack_top[];

/* Userspace syscall sarmalayıcı — always_inline: SVC .user section'a gömülür
 * (ayrı fonksiyon olsa .text/AP=00'da kalır → EL0 çalıştıramaz → fault). */
__attribute__((always_inline)) static inline unsigned long sys(unsigned long num, unsigned long arg) {
    register unsigned long x8 __asm__("x8") = num;
    register unsigned long x0 __asm__("x0") = arg;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8) : "memory");
    return x0;
}

/* EL0 (userspace) programı: hesap + syscall I/O. String literalleri .rodata'da
 * (kernel EL1 okur); EL0 yalnız adreslerini geçer (dereference etmez). */
__attribute__((section(".user"), noinline))
static void el0_program(void) {
    sys(5, (unsigned long)(uintptr_t)"MERHABA userspace");
    sys(7, 0);                                       /* satir */

    /* Userspace hesap: 1..10 topla (EL0'da). */
    int toplam = 0;
    for (int i = 1; i <= 10; i++) toplam += i;       /* = 55 */

    sys(5, (unsigned long)(uintptr_t)"USERSPACE OK toplam=");
    sys(6, (unsigned long)(unsigned int)toplam);     /* yaz_sayi(55) → "...toplam=55" */
    sys(7, 0);                                       /* satir */

    sys(3, 0);                                       /* cik → kernel'de dur */
    for (;;) { }                                     /* ulaşılmaz */
}

int main(void) {
    /* .user sayfası (0x42000000) kdl_mmu_kur'da AP=01 → EL0 kod/stack burada. */
    kdl_el0_calistir(el0_program, __user_stack_top);
    return 0;   /* ulaşılmaz (program cik ile kernel'de durur) */
}
