/*
 * D-127 testi (aarch64) — ÇOKLU EL0 SÜREÇ: izole userspace multitasking (DORUK).
 *
 * İki userspace süreç (A, B) AYNI kodu (paylaşılan .user, 0x42000000) çalıştırır
 * ama HER BİRİ KENDİ adres-uzayında: veri VA'sı 0x42200000 sürece-özel fiziksel
 * sayfaya gider (A→0x44000000, B→0x46000000). Scheduler her preempt'te TTBR0'ı o
 * sürecin tablosuna çevirir (kdl_preempt_gorev_ttbr + kdl_preempt swap).
 *
 * İZOLASYON KANITI: A markörünü (0xAA) bir kez yazar, sonra uzun döngüde SÜREKLİ
 * 0xAA okuduğunu doğrular. Bu sırada timer-IRQ defalarca B'ye geçirir; B kendi
 * sayfasına 0xBB yazar. Sayfalar İZOLE ise A hep 0xAA görür (B'nin 0xBB'si A'nın
 * PA'sına DOKUNMAZ) → "A OK". İzole DEĞİLSE A bir noktada 0xBB okur → "A CORRUPT".
 * Aynı simetrik B için. İkisi de "OK" → gerçek çok-süreçli izolasyon.
 *
 * Kanıt: "A OK" + "B OK" (aynı VA, farklı PA, preemptive, çapraz-bozulma yok).
 */
#include <stdint.h>
extern void kdl_yazdir_metin(const char *);
extern void kdl_yazdir_satir(void);
extern void kdl_surec_kur_el0_veri(uint64_t *l1, uint64_t *l2, uint64_t kod_pa, uint64_t veri_pa);
extern void kdl_kesme_kur(void);
extern void kdl_timer_baslat(void);
extern void kdl_preempt_baslat(void);
extern int  kdl_preempt_gorev_olustur_el0(void (*giris)(void), void *kernel_yigin_tepe,
                                          void *user_yigin_tepe);
extern void kdl_preempt_gorev_ttbr(int gorev, uint64_t *l1);
extern void kdl_preempt_ac(void);

/* Süreç sayfa tabloları + kernel yığınları (kernel bellek, AP=00). */
static uint64_t l1_a[512] __attribute__((aligned(4096)));
static uint64_t l2_a[512] __attribute__((aligned(4096)));
static uint64_t l1_b[512] __attribute__((aligned(4096)));
static uint64_t l2_b[512] __attribute__((aligned(4096)));
static unsigned char kstack_a[8192] __attribute__((aligned(16)));
static unsigned char kstack_b[8192] __attribute__((aligned(16)));

#define VERI    (*(volatile unsigned int *)0x42200000UL)  /* sürece-özel veri (L2[17], AP=01) */
#define USTACK  ((void *)0x42380000UL)                    /* EL0 user yığını (veri sayfasında) */

/* Userspace syscall (always_inline → SVC .user'a gömülü). */
__attribute__((always_inline)) static inline void sys(unsigned long num, unsigned long arg) {
    register unsigned long x8 __asm__("x8") = num;
    register unsigned long x0 __asm__("x0") = arg;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8) : "memory");
}

__attribute__((section(".user"), noinline))
static void el0_proc_a(void) {
    VERI = 0xAA;                                   /* markörü bir kez yaz */
    int ok = 1;
    for (int i = 0; i < 40000; i++) {              /* uzun döngü → çok preemption */
        if (VERI != 0xAA) { ok = 0; break; }       /* B araya girip A'nın PA'sını bozdu mu? */
    }
    sys(5, (unsigned long)(uintptr_t)(ok ? "A OK" : "A CORRUPT"));
    sys(7, 0);
    for (;;) { }                                   /* rapor verdi; preemptible bekle */
}

__attribute__((section(".user"), noinline))
static void el0_proc_b(void) {
    VERI = 0xBB;
    int ok = 1;
    for (int i = 0; i < 40000; i++) {
        if (VERI != 0xBB) { ok = 0; break; }
    }
    sys(5, (unsigned long)(uintptr_t)(ok ? "B OK" : "B CORRUPT"));
    sys(7, 0);
    for (;;) { }
}

int main(void) {
    kdl_yazdir_metin("MULTIPROC BASLA");
    kdl_yazdir_satir();

    /* İki süreç: kod PAYLAŞILAN (0x42000000), veri ÖZEL (A→0x44000000, B→0x46000000). */
    kdl_surec_kur_el0_veri(l1_a, l2_a, 0x42000000UL, 0x44000000UL);
    kdl_surec_kur_el0_veri(l1_b, l2_b, 0x42000000UL, 0x46000000UL);

    kdl_preempt_baslat();                          /* main = görev 0 (EL1) */
    int ta = kdl_preempt_gorev_olustur_el0(el0_proc_a, kstack_a + sizeof(kstack_a), USTACK);
    kdl_preempt_gorev_ttbr(ta, l1_a);              /* A → kendi adres-uzayı */
    int tb = kdl_preempt_gorev_olustur_el0(el0_proc_b, kstack_b + sizeof(kstack_b), USTACK);
    kdl_preempt_gorev_ttbr(tb, l1_b);              /* B → kendi adres-uzayı */

    kdl_kesme_kur();
    kdl_timer_baslat();
    kdl_preempt_ac();

    for (;;) { __asm__ volatile("wfe"); }          /* main döner; A+B preemptively koşup rapor verir */
}
