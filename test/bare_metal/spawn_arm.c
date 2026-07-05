/*
 * D-129 testi (aarch64) — DİNAMİK SÜREÇ OLUŞTURMA (spawn syscall'ı).
 *
 * Şimdiye dek süreçler main'de STATİK kuruluyordu (D-127). Artık bir userspace
 * süreç RUNTIME'da yeni bir izole süreç yaratabilir: launcher (EL0) spawn(worker)
 * syscall'ı çağırır → kernel havuzdan yeni adres-uzayı + preemptive EL0 görev kurar
 * → yeni pid döner. worker kendi adres-uzayında EL0'da koşup rapor verir.
 *
 * Bu, gerçek OS'un temel yeteneği: programların yeni süreç başlatması (fork/spawn).
 *
 * Kanıt: "SPAWN BASLA" + "LAUNCHER spawned pid=2" + "WORKER OK" (dinamik yaratılan
 * süreç izole adres-uzayında koştu).
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

static uint64_t l1_l[512] __attribute__((aligned(4096)));   /* launcher sayfa tabloları */
static uint64_t l2_l[512] __attribute__((aligned(4096)));
static unsigned char kstack_l[8192] __attribute__((aligned(16)));

__attribute__((always_inline)) static inline unsigned long sys(unsigned long num, unsigned long arg) {
    register unsigned long x8 __asm__("x8") = num;
    register unsigned long x0 __asm__("x0") = arg;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8) : "memory");
    return x0;
}

/* Dinamik oluşturulan çocuk süreç — kendi (spawn havuzundan) izole adres-uzayında
 * EL0'da koşar. Rapor verir → dinamik yaratılan görevin gerçekten koştuğunu kanıtlar. */
__attribute__((section(".user"), noinline))
static void worker(void) {
    sys(5, (unsigned long)(uintptr_t)"WORKER OK");
    sys(7, 0);
    for (;;) { }
}

/* Launcher (EL0 süreç): runtime'da spawn(worker) ile yeni izole süreç yaratır. */
__attribute__((section(".user"), noinline))
static void launcher(void) {
    unsigned long pid = sys(12, (unsigned long)(uintptr_t)worker);   /* spawn → yeni pid */
    sys(5, (unsigned long)(uintptr_t)"LAUNCHER spawned pid=");
    sys(6, pid);
    sys(7, 0);
    for (;;) { }
}

int main(void) {
    kdl_yazdir_metin("SPAWN BASLA");
    kdl_yazdir_satir();

    /* launcher'ı EL0 süreç olarak kur (kendi TTBR, veri sayfası 0x44000000). */
    kdl_surec_kur_el0_veri(l1_l, l2_l, 0x42000000UL, 0x44000000UL);
    kdl_preempt_baslat();
    int tl = kdl_preempt_gorev_olustur_el0(launcher, kstack_l + sizeof(kstack_l),
                                           (void *)(uintptr_t)0x42380000UL);
    kdl_preempt_gorev_ttbr(tl, l1_l);
    kdl_kesme_kur();
    kdl_timer_baslat();
    kdl_preempt_ac();

    for (;;) { __asm__ volatile("wfe"); }   /* launcher spawn eder, worker dinamik koşar */
}
