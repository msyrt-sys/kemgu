/*
 * D-138 testi (aarch64) — KAYNAK GERİ-ALMA (slot reuse) — sınırsız spawn.
 *
 * Süreç bitince (exit) hem scheduler görev-slotu hem spawn-havuz-slotu geri alınır
 * → OS programları SINIRSIZ çalıştırabilir. Eski: monoton sayaç, 4 spawn'da havuz
 * tükeniyordu. Şimdi: launcher 6 kez spawn+join yapar (havuz=4'ten fazla) → hepsi
 * başarılı (geri-alma çalışıyor; olmasaydı 5. spawn -1 → SPAWNS=4).
 *
 * Kanıt: "SPAWNS=6" (6 süreç ardışık yaratıldı+bitti, slotlar geri-alındı).
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

__attribute__((always_inline)) static inline unsigned long sys(unsigned long num, unsigned long arg) {
    register unsigned long x8 __asm__("x8") = num;
    register unsigned long x0 __asm__("x0") = arg;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8) : "memory");
    return x0;
}

/* Çocuk: hemen exit (slot geri-alınabilir olsun). */
__attribute__((section(".user"), noinline))
static void worker(void) {
    sys(13, 0);          /* exit */
    for (;;) { }
}

/* 6 kez spawn+join → havuz (4) tükenmemeli (geri-alma). */
__attribute__((section(".user"), noinline))
static void launcher(void) {
    int ok = 0;
    for (int i = 0; i < 6; i++) {
        long pid = (long)sys(12, (unsigned long)(uintptr_t)worker);   /* spawn */
        if (pid >= 0) {
            while (!sys(14, (unsigned long)pid)) { }                  /* join */
            ok++;
        }
    }
    sys(5, (unsigned long)(uintptr_t)"SPAWNS=");
    sys(6, (unsigned long)ok);
    sys(7, 0);
    for (;;) { }
}

int main(void) {
    kdl_yazdir_metin("GERIAL BASLA");
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
