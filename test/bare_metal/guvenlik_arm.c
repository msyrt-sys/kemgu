/*
 * D-150 testi (aarch64) — SYSCALL GÜVENLİK: kullanıcı-pointer doğrulama.
 *
 * Kötü/hatalı bir EL0 süreç, kernel'in YAZDIĞI bir syscall'a (dosya_oku_metin)
 * KERNEL ADRESİ verirse, kernel o adrese yazmamalı (bellek bozulması / privilege
 * escalation). D-150 guard: yazma-hedefi user VA aralığında [0x42000000,0x42400000)
 * olmalı; değilse RED (-1).
 *
 * Test: (1) dosya oluştur; (2) kernel-adresine (0x40000000) oku → RED (-1);
 * (3) user-tampona oku → OK. İkisi de beklendiği gibiyse "GUVENLIK OK".
 *
 * Kanıt: "GUVENLIK OK" (kernel-adresi reddedildi, user-tampon kabul edildi).
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

static uint64_t l1_l[512] __attribute__((aligned(4096)));
static uint64_t l2_l[512] __attribute__((aligned(4096)));
static unsigned char kstack_l[8192] __attribute__((aligned(16)));

__attribute__((always_inline)) static inline unsigned long sys2(unsigned long num, unsigned long a0, unsigned long a1) {
    register unsigned long x8 __asm__("x8") = num;
    register unsigned long x0 __asm__("x0") = a0;
    register unsigned long x1 __asm__("x1") = a1;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8), "r"(x1) : "memory");
    return x0;
}
__attribute__((always_inline)) static inline unsigned long sys(unsigned long num, unsigned long arg) {
    register unsigned long x8 __asm__("x8") = num;
    register unsigned long x0 __asm__("x0") = arg;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8) : "memory");
    return x0;
}

__attribute__((section(".user"), noinline))
static void launcher(void) {
    sys2(17, (unsigned long)(uintptr_t)"f", (unsigned long)(uintptr_t)"DATA");  /* dosya_yaz_metin */

    /* (2) KERNEL adresine (0x40000000) yazmayı dene → guard RED etmeli (-1). */
    long r_kotu = (long)sys2(18, (unsigned long)(uintptr_t)"f", 0x40000000UL);

    /* (3) Geçerli user tampona (veri sayfası) → OK. */
    char *user_buf = (char *)(uintptr_t)0x42210000UL;
    long r_iyi = (long)sys2(18, (unsigned long)(uintptr_t)"f", (unsigned long)(uintptr_t)user_buf);

    /* EL0'dan çıktı: yaz SYSCALL'ı (kernel fonksiyonunu doğrudan çağıramayız). */
    if (r_kotu == -1 && r_iyi >= 0) {
        sys(5, (unsigned long)(uintptr_t)"GUVENLIK OK");   /* kernel-adres reddedildi, user kabul */
    } else {
        sys(5, (unsigned long)(uintptr_t)"GUVENLIK HATA");
    }
    sys(7, 0);
    for (;;) { }
}

int main(void) {
    kdl_yazdir_metin("GUVENLIK BASLA");
    kdl_yazdir_satir();

    kdl_surec_kur_el0_veri(l1_l, l2_l, 0x42000000UL, 0x44000000UL);
    kdl_preempt_baslat();
    int tl = kdl_preempt_gorev_olustur_el0(launcher, kstack_l + sizeof(kstack_l),
                                           (void *)(uintptr_t)0x42380000UL);
    kdl_preempt_gorev_ttbr(tl, l1_l);
    kdl_kesme_kur();
    kdl_timer_baslat();
    kdl_preempt_ac();

    for (;;) { __asm__ volatile("wfe"); }
}
