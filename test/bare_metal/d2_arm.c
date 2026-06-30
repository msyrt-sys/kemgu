/*
 * D2 testi (aarch64) — user/kernel privilege ayrımı (EL0 + syscall sınırı).
 *
 * main EL1'de (kernel). kdl_el0_calistir → EL0'a düşer + el0_kod'u 0x42000000'da
 * (kdl_mmu.c AP=01 işaretli 2MB sayfa) çalıştırır. el0_kod EL0'da koşar →
 * kernel/device sayfaları (AP=00) EL0'a KAPALI; el0_kod yalnız SVC (syscall) ile
 * EL1 kernel'e geçebilir. Kernel handler kaynak-EL'i SPSR_EL1'den okur → 0 (EL0).
 *
 * Kanıt: "D2 BASLA (EL1)" + "EL0 SYSCALL kaynak-EL=" + "0x0" (EL0!) + "D2 OK".
 */
extern void kdl_yazdir_metin(const char *);
extern void kdl_yazdir_satir(void);
extern void kdl_el0_calistir(void (*fn)(void), void *sp);
extern unsigned char __user_stack_top[];   /* linker .user (AP=01 sayfa) */

/* EL0'da koşar (0x42000000). SELF-CONTAINED: pure SVC; kernel fonksiyonu/globali
 * ÇAĞIRMAZ (kernel sayfaları AP=00, EL0'a kapalı → erişim permission-fault). */
__attribute__((section(".user"), noinline))
static void el0_kod(void) {
    register unsigned long x8 __asm__("x8");
    x8 = 2; __asm__ volatile("svc #0" : : "r"(x8) : "memory");   /* kernel: kaynak-EL bas */
    x8 = 3; __asm__ volatile("svc #0" : : "r"(x8) : "memory");   /* kernel: D2 OK + dur */
    for (;;) { }                                                 /* ulaşılmaz */
}

int main(void) {
    kdl_yazdir_metin("D2 BASLA (EL1)");
    kdl_yazdir_satir();

    kdl_el0_calistir(el0_kod, __user_stack_top);   /* → EL0 */

    /* el0_kod syscall 3 ile kernel'de durur → buraya dönülmez */
    return 0;
}
