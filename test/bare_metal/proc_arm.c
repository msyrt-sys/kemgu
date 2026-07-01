/*
 * D3 testi (aarch64) — KORUMALI EL0 user-process (D1 ⊕ D2 ⊕ D-122 birleşik).
 *
 * Gerçek bir işletim sistemi sürecinin tanımlayıcı özellikleri BİR ARADA:
 *   1. KENDİ adres-uzayı: süreç kendi L1/L2 sayfa tablolarına sahip (kernel'in
 *      global tablolarından ayrı), TTBR0 swap ile geçilir (D1 makinesi).
 *   2. Kullanıcı ayrıcalığı: kod EL0'da koşar (D2), kendi TTBR'ı altında.
 *   3. Syscall arayüzü: EL0 kod SVC ile argüman geçer (num=4 arg=42, D-122) →
 *      kernel "SYSCALL ARG OK" → meşru kernel geçişi çalışır.
 *   4. Bellek koruması (HAPİS): süreç kernel-only sayfaya (0x40000000, AP=00)
 *      erişmeye kalkınca EL0 permission-fault → kernel yakalar (ISTISNA). Süreç
 *      KENDİ adres-uzayına hapsedilmiş; kernel belleğine dokunamaz.
 *
 * Kanıt: "PROC BASLA (EL1)" + "SYSCALL ARG OK" (EL0'dan, özel TTBR altında) +
 * "ISTISNA" (kernel-erişim reddi, adr=0x40000000). = korumalı user-process.
 */
#include <stdint.h>
extern void kdl_yazdir_metin(const char *);
extern void kdl_yazdir_satir(void);
extern void kdl_surec_kur_el0(uint64_t *l1, uint64_t *l2, uint64_t user_pa);
extern void kdl_ttbr_degis(uint64_t *l1);
extern void kdl_el0_calistir(void (*fn)(void), void *sp);
extern unsigned char __user_stack_top[];   /* linker .user (AP=01 sayfa) */

static uint64_t l1[512] __attribute__((aligned(4096)));
static uint64_t l2[512] __attribute__((aligned(4096)));

/* EL0'da, sürecin KENDİ TTBR'ı altında koşar (0x42000000, .user sayfası AP=01).
 * SELF-CONTAINED: pure SVC; kernel fonksiyonu/globali ÇAĞIRMAZ. */
__attribute__((section(".user"), noinline))
static void el0_kod(void) {
    /* 1) Meşru syscall: num=4 (x8), arg=42 (x0) → kernel "SYSCALL ARG OK".
     *    EL0'dan, sürece-özel adres-uzayı altında argümanlı sistem çağrısı. */
    __asm__ volatile("mov x8, #4\n mov x0, #42\n svc #0\n" : : : "x0", "x8", "memory");

    /* 2) Bellek koruması: kernel-only sayfaya (0x40000000, AP=00) EL0 erişimi →
     *    permission-fault → kernel yakalar. Süreç kendi adres-uzayına hapis. */
    volatile uint32_t *kernel_adr = (volatile uint32_t *)0x40000000UL;
    uint32_t kacak = *kernel_adr;     /* → EL0 permission-fault (data abort) */
    (void)kacak;

    for (;;) { }                      /* ulaşılmaz (fault kernel'de halt eder) */
}

int main(void) {
    kdl_yazdir_metin("PROC BASLA (EL1)");
    kdl_yazdir_satir();

    /* Sürecin adres-uzayı: kendi tabloları, user sayfası (0x42000000) EL0-erişimli.
     * user_pa = 0x42000000 (identity — .user section'ın fiziksel yeri). */
    kdl_surec_kur_el0(l1, l2, 0x42000000UL);
    kdl_ttbr_degis(l1);                            /* sürecin KENDİ TTBR'ına geç */
    kdl_el0_calistir(el0_kod, __user_stack_top);   /* → EL0, süreç adres-uzayı altında */

    return 0;   /* ulaşılmaz (el0_kod fault ile kernel'de durur) */
}
