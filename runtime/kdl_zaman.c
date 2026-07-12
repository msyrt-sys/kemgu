/*
 * KEMGU Bare-Metal Zamanlayıcı + IRQ Dispatch (kdl_zaman.c)
 * ========================================================
 *
 * C3b/C4: donanım kesmesi (IRQ) altyapısı + periyodik timer. Vektör/IDT asm
 * stub'ları (boot/start_*.S) IRQ'da bağlamı kaydedip kdl_kesme_isle() çağırır.
 *
 * Ortak API (her iki mimari):
 *   kdl_kesme_kur()   — kesme denetleyici init (aarch64 GICv2 / x86 PIC 8259)
 *   kdl_timer_baslat()— periyodik timer + IRQ aç (aarch64 CNTV / x86 PIT 8254)
 *   kdl_kesme_isle()  — IRQ dispatch (boot asm'den; timer → tik say)
 *   kdl_idle()        — düşük-güç bekle (wfi / hlt)
 *
 * KISIT: kesme bağlamı bölge/frame allocator KULLANMAZ (tek-thread, IRQ-safe
 * değil) → yalnız UART + register. Kanıt: timer N kez tetiklenince "TIMER OK".
 */
#include <stdint.h>

void kdl_yazdir_metin(const char *);
void kdl_yazdir_satir(void);

static volatile uint64_t kdl_tik_sayisi = 0;

/* "TIMER OK tik=5" TEK-SEFERLİK tanılaması yalnız bağımsız C4 timer testinindir
 * (calistir_timer_test_arm). Varsayılan AÇIK → o test yeşil kalır. Entegre çekirdek
 * (kemgu_os_arm.c) bunu KAPATIR: preemption init_betik sırasında açık olduğundan
 * timer-IRQ bağlamındaki bu konsol yazımı deterministik init çıktısını (ör. "PAGEFAULT
 * OK") ORTASINDAN bölerdi. Üretim timer-tick'i konsola yazmamalı; entegre çekirdek
 * timer-canlılığını "UPTIME: timer canli" + SCHEDULER OK sayaç-kanıtı ile ispatlar. */
volatile int kdl_timer_diag_aktif = 1;

static void kdl_tik(void) {
    kdl_tik_sayisi++;
    if (kdl_tik_sayisi == 5 && kdl_timer_diag_aktif) {
        kdl_yazdir_metin("TIMER OK tik=5");
        kdl_yazdir_satir();
    }
}

/* D-128: mevcut timer tik sayısı (userspace gettick syscall'ı için). */
uint64_t kdl_tik_al(void) {
    return kdl_tik_sayisi;
}

/* ================= aarch64 (GICv2 + sanal generic timer) ================= */
#if defined(__aarch64__)

#define KDL_GICD_BASE  0x08000000UL    /* QEMU virt distributor */
#define KDL_GICC_BASE  0x08010000UL    /* QEMU virt CPU interface */
#define KDL_TIMER_IRQ  27u             /* sanal timer PPI (INTID 27) */

static uint64_t kdl_timer_aralik = 0;

static inline void mmio32_yaz(uint64_t adr, uint32_t v) {
    *(volatile uint32_t *)adr = v;
}
static inline uint64_t cntfrq(void) {
    uint64_t v; __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(v)); return v;
}
static inline void cntv_tval(uint64_t v) {
    __asm__ volatile("msr cntv_tval_el0, %0" : : "r"(v));
}
static inline void cntv_ctl(uint64_t v) {
    __asm__ volatile("msr cntv_ctl_el0, %0" : : "r"(v));
}

/* FAZ-A3 (D-279): kem_os kdl_kesme_kur/kdl_timer_baslat/kdl_irq_isle'i SAF-.kem'de
 * (kem_zaman.kem) sağlar → .S kdl_irq_ortak `bl kdl_irq_isle` .kem'i çözer.
 * -DKEMGU_KEM_MALLOC (kem_os variant) ile C tanımları ÇIKARILIR (çift-sembol yok).
 * kdl_kesme_isle/kdl_tik/kdl_tik_al C KALIR (statik'ler kullanımda; regresyon yok). */
#ifndef KEMGU_KEM_MALLOC
void kdl_kesme_kur(void) {
    mmio32_yaz(KDL_GICD_BASE + 0x000, 1);            /* GICD_CTLR = enable */
    mmio32_yaz(KDL_GICC_BASE + 0x004, 0xFF);         /* GICC_PMR = tüm öncelikler */
    mmio32_yaz(KDL_GICC_BASE + 0x000, 1);            /* GICC_CTLR = enable */
    mmio32_yaz(KDL_GICD_BASE + 0x100, (1u << KDL_TIMER_IRQ)); /* ISENABLER0 bit27 */
}

void kdl_timer_baslat(void) {
    uint64_t f = cntfrq();
    kdl_timer_aralik = f / 100u;                     /* ~10ms */
    cntv_tval(kdl_timer_aralik);
    cntv_ctl(1);                                     /* enable + unmask */
    __asm__ volatile("msr daifclr, #2");             /* IRQ aç */
}
#endif  /* KEMGU_KEM_MALLOC — kem_os SAF-.kem kdl_kesme_kur/kdl_timer_baslat (kem_zaman.kem) */

/* IRQ dispatch — irq = IAR & 0x3FF. timer → re-arm + tik. */
void kdl_kesme_isle(uint64_t irq) {
    if (irq == KDL_TIMER_IRQ) {
        cntv_tval(kdl_timer_aralik);                 /* re-arm (ISTATUS temizler) */
        kdl_tik();
    }
}

/* Full trap-frame IRQ dispatch (C7b) — boot kdl_irq_ortak'tan; sp = trap-frame
 * ptr. GICC_IAR → tik/re-arm → EOI (switch'ten ÖNCE, GIC'i serbest bırak) →
 * kdl_preempt (scheduler; kapalıysa sp). Devam edilecek SP'yi döner. */
extern uint64_t kdl_preempt(uint64_t sp);

#ifndef KEMGU_KEM_MALLOC
uint64_t kdl_irq_isle(uint64_t sp) {
    uint32_t iar = *(volatile uint32_t *)(KDL_GICC_BASE + 0x0C);
    uint32_t irq = (uint32_t)(iar & 0x3FF);
    kdl_kesme_isle(irq);                                    /* tik + re-arm */
    *(volatile uint32_t *)(KDL_GICC_BASE + 0x10) = iar;     /* EOI — switch ÖNCESİ */
    return kdl_preempt(sp);
}
#endif  /* KEMGU_KEM_MALLOC — kem_os SAF-.kem kdl_irq_isle (kem_zaman.kem) */

void kdl_idle(void) { __asm__ volatile("wfi"); }

/* ================= x86_64 (PIC 8259 + PIT 8254) ================= */
#elif defined(__x86_64__)

static inline void outb(uint16_t port, uint8_t v) {
    __asm__ volatile("outb %0, %1" : : "a"(v), "Nd"(port));
}

void kdl_kesme_kur(void) {
    /* PIC 8259 remap: IRQ0-7 → vektör 32-39, IRQ8-15 → 40-47. */
    outb(0x20, 0x11); outb(0xA0, 0x11);              /* ICW1: init + ICW4 bekle */
    outb(0x21, 0x20); outb(0xA1, 0x28);              /* ICW2: vektör offset 32/40 */
    outb(0x21, 0x04); outb(0xA1, 0x02);              /* ICW3: cascade (slave@IRQ2) */
    outb(0x21, 0x01); outb(0xA1, 0x01);              /* ICW4: 8086 mode */
    outb(0x21, 0xFE); outb(0xA1, 0xFF);              /* mask: yalnız IRQ0 açık */
}

void kdl_timer_baslat(void) {
    /* PIT 8254 ch0: mode 3 (kare dalga), lo/hi, binary = 0x36. */
    outb(0x43, 0x36);
    uint16_t bolen = 11932;                          /* 1193182/11932 ≈ 100 Hz */
    outb(0x40, (uint8_t)(bolen & 0xFF));
    outb(0x40, (uint8_t)((bolen >> 8) & 0xFF));
    __asm__ volatile("sti");                         /* IRQ aç */
}

/* IRQ dispatch — boot kdl_irq0_stub'tan (irq = 0 timer). PIT mode 3 auto-reload. */
void kdl_kesme_isle(uint64_t irq) {
    (void)irq;                                       /* yalnız IRQ0 açık */
    kdl_tik();
}

void kdl_idle(void) { __asm__ volatile("hlt"); }

#endif
