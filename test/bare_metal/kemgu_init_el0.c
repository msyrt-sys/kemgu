/*
 * KEMGU-OS PART 3 — AYRI DERLENEN userspace init programı (EL0).
 * ============================================================
 *
 * Bu dosya kemgu_os_arm.c'den AYRI derlenir (BM_A64_EL0 = -mgeneral-regs-only, GPR-only)
 * ve entegre çekirdeğe linklenir. Kod .user section'a (linker → 0x42000000, AP=01 sayfa)
 * konur → EL0'dan çalıştırılabilir. Çekirdek bunu preemptive EL0 GÖREV olarak koşturur
 * (kdl_preempt_gorev_olustur_el0). Bu = "kernel bir programı YÜKLER + EL0'da koşturur"
 * (kernel-içi C fonksiyonu DEĞİL — gerçekten EL0'da, boot sayfa-tablosu altında).
 *
 * NEDEN AYRI DOSYA + GPR-only? kemgu_os_arm.c KERNEL flag'siz (-mgeneral-regs-only YOK)
 * derlenir (MMU/SIMD kanıtı, D-235). Flag'siz EL0 kodu NEON sabit-havuzunu kernel
 * .rodata'ya (AP=00) koyar → EL0'dan okuyunca permission-fault (D-235 EL0 bug). Bu yüzden
 * EL0 programı BM_A64_EL0 ile GPR-only derlenir → sabit-havuz yok, kendi sayfasında kalır.
 *
 * KANITLAR (kernel'in bastığı, EL0'dan tetiklenen):
 *   (a) sys(2): kernel "EL0 SYSCALL kaynak-EL=0x0" basar — SPSR.M[3:2] donanım register'ı
 *       = 0 → GERÇEKTEN EL0'da (kernel-fonksiyonu olsaydı EL1=1 olurdu). Taklit edilemez.
 *   (b) sys(5,str): EL0'dan SVC ile syscall → kernel string'i basar + EL0'a döner
 *       (syscall arayüzü çalışır).
 *   (c) kernel-belleğe (0x40000000, AP=00) KASITLI eriş → EL0 permission-fault → kernel
 *       süreci ÖLDÜRÜR ("IZOLASYON OK") + OS devam eder (gerçek process izolasyonu;
 *       kernel-fonksiyon her yere erişebilirdi = izolasyon-yok demekti).
 */
#include <stdint.h>

/* Argümansız/1-argümanlı syscall (SVC ABI: x8=num, x0=arg → x0=dönüş). always_inline
 * + .user'da: EL0'dan doğrudan SVC (kernel-fonksiyon ÇAĞIRMAZ). */
__attribute__((always_inline)) static inline unsigned long u_sys(unsigned long num, unsigned long a0) {
    register unsigned long x8 __asm__("x8") = num;
    register unsigned long x0 __asm__("x0") = a0;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8) : "memory");
    return x0;
}

/* EL0 init programı — çekirdek boot sayfa-tablosu altında, 0x42000000 (.user, AP=01)
 * sayfasında EL0'da koşar. Yalnız SVC kullanır (kernel-fonksiyon/globali ÇAĞIRMAZ). */
__attribute__((section(".user"), noinline))
void kemgu_el0_init(void) {
    /* (a) Kaynak-EL bildir → kernel "EL0 SYSCALL kaynak-EL=0x0" (donanım SPSR'dan). */
    u_sys(2, 0);
    /* (b) yaz syscall → kernel string'i basar (EL0'dan SVC + döner). String .rodata'da;
     *     EL0 yalnız ADRESİ geçer (adrp — deref etmez), KERNEL okur (kdl_user_oku_str
     *     .rodata'ya izinli). */
    u_sys(5, (unsigned long)(uintptr_t)"USERSPACE INIT EL0 KOSUYOR");
    /* (c) İZOLASYON: kernel-only sayfaya (0x40000000, AP=00) KASITLI EL0 erişimi →
     *     permission-fault → kernel süreci öldürür + "IZOLASYON OK" + OS devam. */
    volatile uint32_t *kernel_adr = (volatile uint32_t *)(uintptr_t)0x40000000UL;
    uint32_t kacak = *kernel_adr;      /* → EL0 permission-fault (data abort, DFSC=0x0e) */
    (void)kacak;
    /* Normalde ulaşılmaz (süreç öldürüldü, scheduler atlar). Güvenlik için spin. */
    for (;;) { }
}
