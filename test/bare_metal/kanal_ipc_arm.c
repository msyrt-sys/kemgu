/*
 * D-140 testi (aarch64) — USERSPACE MESAJ KANALI (IPC) — süreçler-arası mesajlaşma.
 *
 * İki userspace süreç, çekirdek-aracılı mesaj kanalıyla DOĞRUDAN haberleşir (dosya-
 * IPC'nin ötesinde; KEMGU `kanal` ilkelinin userspace düzeyi). Gönderici değerleri
 * kanala yollar, alıcı (bloklamasız yoklamayla) alıp toplar.
 *
 *   launcher (alıcı): spawn(sender) → kanal_al ile 3 değer al (poll) → topla.
 *   sender (gönderici): kanal_gonder(100/200/300) → exit.
 *
 * Kanıt: "KANAL SUM=600" (mesajlar gönderici→kanal→alıcı ulaştı).
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

/* Gönderici: 3 değeri kanala yolla → exit. */
__attribute__((section(".user"), noinline))
static void sender(void) {
    sys(22, 100);
    sys(22, 200);
    sys(22, 300);
    sys(13, 0);          /* exit */
    for (;;) { }
}

/* Alıcı: gönderici'yi başlat → kanaldan 3 değer al (poll) → topla. */
__attribute__((section(".user"), noinline))
static void launcher(void) {
    sys(12, (unsigned long)(uintptr_t)sender);   /* spawn sender */
    int sum = 0, count = 0;
    while (count < 3) {
        long v = (long)sys(23, 0);               /* kanal_al (boşsa -1) */
        if (v >= 0) { sum += (int)v; count++; }
    }
    sys(5, (unsigned long)(uintptr_t)"KANAL SUM=");
    sys(6, (unsigned long)sum);
    sys(7, 0);
    for (;;) { }
}

int main(void) {
    kdl_yazdir_metin("KANALIPC BASLA");
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
