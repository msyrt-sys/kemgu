/*
 * D-151 testi (aarch64) — SYSCALL OKUMA-GÜVENLİK: kullanıcı-OKUMA-pointer doğrulama.
 *
 * D-150 kernel'in YAZDIĞI pointer'ları doğruladı. D-151 kernel'in OKUDUĞU (deref
 * ettiği) EL0-kontrollü string pointer'larını doğrular. Doğrulama olmadan kötü/hatalı
 * bir EL0 süreç:
 *   (a) unmapped adres geçirir → kernel EL1'de data-abort → sonsuz halt (DoS, TEK SVC),
 *   (b) kernel adresi geçirir → kernel belleği UART'a/dosyaya sızar (info-leak).
 *
 * Test (EL0 launcher):
 *   (1) geçerli dosya oluştur (baseline, .rodata string'ler);
 *   (2) num=5 yaz'a UNMAPPED adres (0x80000000) ver → guard RED (-1), kernel HALT ETMEZ;
 *   (3) num=16 dosya_oku'ya KERNEL adresi (0x40100000, mapped .bss RAM) ver → guard RED
 *       (-1), info-leak yok;
 *   (4) BURAYA ULAŞMAK = kernel (2)+(3)'te çökmedi. İki de -1 ise "GUVENLIK OKU OK".
 *
 * Fix ÖNCESİ: adım (2) kernel'i sonsuza kadar halt ederdi → "GUVENLIK OKU OK" hiç basılmaz.
 * Kanıt: seri çıktıda "GUVENLIK OKU OK" (kernel kötü okumalardan sağ çıktı + reddetti).
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
    /* (1) baseline: geçerli dosya (.rodata ad+içerik → guard İZİN verir). */
    sys2(17, (unsigned long)(uintptr_t)"f", (unsigned long)(uintptr_t)"DATA");

    /* (2) DoS denemesi: num=5 yaz'a UNMAPPED adres (L1[2] geçersiz) → guard RED etmeli,
     *     kernel deref ETMEMELİ. Fix öncesi burada sonsuz halt olurdu. */
    long r_dos = (long)sys(5, 0x80000000UL);

    /* (3) info-leak denemesi: num=16 dosya_oku'ya KERNEL adresi (mapped kernel RAM) ad
     *     olarak ver → guard RED etmeli (kernel byte'larını okuma-oracle'ı engelle). */
    long r_leak = (long)sys2(16, 0x40100000UL, 0);

    /* (4) Kernel HÂLÂ CANLI (buraya ulaştık). İki guard da reddettiyse başarı. */
    if (r_dos == -1 && r_leak == -1) {
        sys(5, (unsigned long)(uintptr_t)"GUVENLIK OKU OK");
    } else {
        sys(5, (unsigned long)(uintptr_t)"GUVENLIK OKU HATA");
    }
    sys(7, 0);
    for (;;) { }
}

int main(void) {
    kdl_yazdir_metin("GUVENLIK OKU BASLA");
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
