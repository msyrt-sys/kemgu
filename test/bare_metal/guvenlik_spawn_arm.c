/*
 * D-152 testi (aarch64) — SPAWN-ENTRY GÜVENLİK: num=12 spawn giriş adresi doğrulama.
 *
 * Kötü/hatalı bir EL0 launcher, spawn syscall'ına (num=12) GEÇERSİZ bir giriş
 * adresi (kernel adresi / null / unmapped / hizasız) verirse, kernel o adresi
 * yeni EL0 sürecin ELR_EL1'ine koymamalı: aksi halde EL0 komut-fetch'i fault
 * eder → lower-EL sync exception → kdl_istisna_isle → tüm kernel sonsuz halt
 * (tek SVC ile DoS). D-152 guard: entry .user kod sayfası [0x42000000,0x42200000)
 * içinde VE 4-byte hizalı olmalı; değilse RED (-1) — süreç yaratılmaz.
 *
 * Test (EL0 launcher):
 *   (1) sys(12, 0x40080000) [kernel .text adresi]   → -1 dönmeli, kernel SAĞ KALMALI;
 *   (2) sys(12, 0x0)        [null]                   → -1 dönmeli;
 *   (3) sys(12, &worker)    [geçerli .user worker]   → >=0 dönmeli, worker koşmalı.
 * Üçü de beklendiği gibiyse "SPAWN GUARD OK" basılır.
 *
 * Kanıt: Success print'e ULAŞMAK = kernel kötü spawn'lardan sağ çıktı (bad entry
 * fault etseydi kernel halt ederdi, bu satıra hiç gelinmezdi). "WORKER OK" =
 * geçerli entry gerçekten yeni izole EL0 sürecinde koştu.
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

/* Geçerli entry ile dinamik oluşturulan çocuk süreç — kendi izole adres-uzayında
 * EL0'da koşar. Basit bir syscall yapar (dosya_yaz: num=15) → geçerli entry'nin
 * gerçekten yeni sürecte koştuğunu kanıtlar. */
__attribute__((section(".user"), noinline))
static void worker(void) {
    sys2(15, (unsigned long)(uintptr_t)"w", 42);   /* dosya_yaz("w", 42) — worker koştu */
    sys(5, (unsigned long)(uintptr_t)"WORKER OK");
    sys(7, 0);
    for (;;) { }
}

/* Launcher (EL0 süreç): kötü + iyi spawn'ları dener. Kötüler -1 dönmeli VE kernel
 * sağ kalmalı (bu satırlara devam edebilmek = kernel halt etmedi = kanıt). */
__attribute__((section(".user"), noinline))
static void launcher(void) {
    /* (1) KERNEL .text adresi → guard RED etmeli. Guard yoksa: EL0 fetch-fault → halt. */
    long r_kernel = (long)sys(12, 0x40080000UL);

    /* (2) null → guard RED etmeli. */
    long r_null = (long)sys(12, 0x0UL);

    /* (3) Geçerli .user worker → guard KABUL etmeli (>=0), worker dinamik koşar. */
    long r_iyi = (long)sys(12, (unsigned long)(uintptr_t)&worker);

    if (r_kernel == -1 && r_null == -1 && r_iyi >= 0) {
        sys(5, (unsigned long)(uintptr_t)"SPAWN GUARD OK");   /* kötü RED, iyi KABUL, kernel sağ */
    } else {
        sys(5, (unsigned long)(uintptr_t)"SPAWN GUARD HATA");
    }
    sys(7, 0);
    for (;;) { }
}

int main(void) {
    kdl_yazdir_metin("SPAWN GUARD BASLA");
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

    for (;;) { __asm__ volatile("wfe"); }   /* launcher kötü+iyi spawn dener, kernel sağ kalır */
}
