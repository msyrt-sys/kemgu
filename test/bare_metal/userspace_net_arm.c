/*
 * D-176 testi (aarch64) — USERSPACE NETWORKING: EL0 süreç syscall ile ağ yapar.
 *
 * Şimdiye kadar ağ (ARP/IP/UDP/TCP/DNS/HTTP) hep KERNEL (EL1) kodundan yapıldı.
 * Bu, iki büyük alt-sistemi BİRLEŞTİRİR: süreç/syscall modeli (D-124..140) + ağ
 * yığını (D-144..167). Bir EL0 (yetkisiz) süreç, virtio-net'e DOĞRUDAN erişmeden,
 * yalnız SYSCALL ile ham ethernet çerçevesi gönderir/alır:
 *   num=24 net_gonder(cerceve, uzun)  — kernel frame'i OKUR (user VA doğrulanır) + yollar
 *   num=25 net_al(buf, maxlen)        — kernel frame'i user buffer'a YAZAR (user VA doğrulanır)
 *
 * Güvenlik (D-150/151): net_gonder frame'i user VA'da olmalı; net_al hedef user VA'da
 * olmalı. Kötü pointer → -1 (kernel belleği korunur).
 *
 * Senaryo: EL0 süreç bir ARP isteği (gateway 10.0.2.2) inşa eder → net_gonder ile
 * yollar → net_al ile SLIRP'in ARP yanıtını alır + doğrular → "USERNET OK".
 * Kanıt: bir userspace program, çekirdek-aracılı ağ syscall'larıyla ağ round-trip yaptı.
 */
#include <stdint.h>
extern void kdl_yazdir_metin(const char *);
extern void kdl_yazdir_satir(void);
extern uint64_t kdl_virtio_net_bul(void);
extern int  kdl_virtio_net_kur(uint64_t base);
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

/* EL0 süreç: ARP isteği syscall ile yolla + yanıtı syscall ile al. */
__attribute__((section(".user"), noinline))
static void launcher(void) {
    unsigned char frame[42];
    const unsigned char bizim_mac[6] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
    int i;
    for (i = 0; i < 42; i++) frame[i] = 0;
    for (i = 0; i < 6; i++) frame[i] = 0xff;               /* dst broadcast */
    for (i = 0; i < 6; i++) frame[6 + i] = bizim_mac[i];   /* src mac */
    frame[12] = 0x08; frame[13] = 0x06;                    /* ethertype ARP */
    frame[14] = 0x00; frame[15] = 0x01;                    /* htype ethernet */
    frame[16] = 0x08; frame[17] = 0x00;                    /* ptype IPv4 */
    frame[18] = 6; frame[19] = 4;                          /* hlen, plen */
    frame[20] = 0x00; frame[21] = 0x01;                    /* oper = request */
    for (i = 0; i < 6; i++) frame[22 + i] = bizim_mac[i];  /* sha */
    frame[28] = 10; frame[29] = 0; frame[30] = 2; frame[31] = 15;  /* spa 10.0.2.15 */
    frame[38] = 10; frame[39] = 0; frame[40] = 2; frame[41] = 2;   /* tpa 10.0.2.2 gateway */

    /* (1) SYSCALL ile gönder. */
    long g = (long)sys2(24, (unsigned long)(uintptr_t)frame, 42);
    if (g < 0) { sys(5, (unsigned long)(uintptr_t)"USERNET GONDER HATA"); sys(7, 0); for (;;) {} }

    /* (2) SYSCALL ile ARP yanıtını al (poll — kısa per-çağrı timeout, EL0 döngüsü). */
    unsigned char rx[128];
    int alindi = 0;
    for (int deneme = 0; deneme < 200 && !alindi; deneme++) {
        long n = (long)sys2(25, (unsigned long)(uintptr_t)rx, 128);
        if (n < 42) continue;
        if (rx[12] != 0x08 || rx[13] != 0x06) continue;   /* ethertype ARP */
        if (rx[20] != 0x00 || rx[21] != 0x02) continue;   /* oper = reply */
        /* spa = gateway 10.0.2.2 mi? */
        if (rx[28] == 10 && rx[29] == 0 && rx[30] == 2 && rx[31] == 2) alindi = 1;
    }

    /* (3) Kötü-pointer güvenlik kontrolü: kernel adresine net_al → RED (-1). */
    long kotu = (long)sys2(25, 0x40000000UL, 128);

    if (alindi && kotu == -1) {
        sys(5, (unsigned long)(uintptr_t)"USERNET OK");
    } else if (alindi) {
        sys(5, (unsigned long)(uintptr_t)"USERNET OK-GUARD-ZAYIF");
    } else {
        sys(5, (unsigned long)(uintptr_t)"USERNET YANIT YOK");
    }
    sys(7, 0);
    for (;;) { }
}

int main(void) {
    kdl_yazdir_metin("USERNET BASLA");
    kdl_yazdir_satir();

    /* Kernel (EL1) net sürücüsünü kurar — EL0 süreç yalnız syscall kullanır. */
    uint64_t nb = kdl_virtio_net_bul();
    if (!nb || kdl_virtio_net_kur(nb) != 0) {
        kdl_yazdir_metin("NET KUR HATA"); kdl_yazdir_satir();
        for (;;) { __asm__ volatile("wfe"); }
    }

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
