/*
 * D-128 testi (aarch64) — userspace INTROSPECTION syscall'ları (gettick + getpid).
 *
 * Preemptive EL0 görev (userspace): gettick(t1) → zaman geçir (timer-IRQ tikler,
 * görev preemptive olduğundan IRQ açık) → gettick(t2) → getpid. t2>t1 (zaman
 * ilerledi) + pid = sürecin scheduler id'si. Syscall DÖNÜŞ değeri ABI'si (D-126)
 * üstünde userspace'in çekirdek durumunu (zaman/kimlik) okuması.
 *
 * Kanıt: "TICK OK pid=1" (userspace zamanın ilerlediğini gördü + kendi id'sini aldı).
 */
#include <stdint.h>
extern void kdl_yazdir_metin(const char *);
extern void kdl_yazdir_satir(void);
extern void kdl_kesme_kur(void);
extern void kdl_timer_baslat(void);
extern void kdl_preempt_baslat(void);
extern int  kdl_preempt_gorev_olustur_el0(void (*giris)(void), void *kernel_yigin_tepe,
                                          void *user_yigin_tepe);
extern void kdl_preempt_ac(void);
extern unsigned char __user_stack_top[];

static unsigned char kstack[8192] __attribute__((aligned(16)));

/* Userspace syscall — dönüş x0'da (D-126). always_inline → SVC .user'a gömülü. */
__attribute__((always_inline)) static inline unsigned long sys(unsigned long num, unsigned long arg) {
    register unsigned long x8 __asm__("x8") = num;
    register unsigned long x0 __asm__("x0") = arg;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8) : "memory");
    return x0;
}

__attribute__((section(".user"), noinline))
static void el0_kod(void) {
    unsigned long t1 = sys(10, 0);                    /* gettick */
    for (volatile long i = 0; i < 4000000; i++) { }   /* zaman geçir (timer tikler) */
    unsigned long t2 = sys(10, 0);                    /* gettick */
    unsigned long pid = sys(11, 0);                   /* getpid */

    if (t2 > t1) {
        sys(5, (unsigned long)(uintptr_t)"TICK OK pid=");
        sys(6, pid);                                  /* yaz_sayi(pid) → "...pid=1" */
    } else {
        sys(5, (unsigned long)(uintptr_t)"TICK HATA");
    }
    sys(7, 0);
    for (;;) { }
}

int main(void) {
    kdl_yazdir_metin("TICK BASLA");
    kdl_yazdir_satir();

    kdl_preempt_baslat();                             /* main = görev 0 */
    kdl_preempt_gorev_olustur_el0(el0_kod, kstack + sizeof(kstack), __user_stack_top);  /* görev 1 (EL0) */
    kdl_kesme_kur();
    kdl_timer_baslat();
    kdl_preempt_ac();

    for (;;) { __asm__ volatile("wfe"); }
}
