/*
 * D-154 testi (aarch64) — "DÜŞMAN USERSPACE" BOMBARDIMAN REGRESYON BEKÇİSİ.
 *
 * D-150 (kernel'in YAZDIĞI user-tampon doğrulama) + D-151 (kernel'in OKUDUĞU
 * EL0-string doğrulama) ile sertleştirilmiş syscall-pointer sınırının KALICI
 * regresyon bekçisi. Kötü niyetli tek bir EL0 süreç, TÜM syscall-pointer güvenlik
 * yüzeyine bir BATARYA kötü giriş ateşler; kernel HEPSİNDEN SAĞ ÇIKMALI (HALT
 * ETMEMELİ) ve batarya sonrasında hâlâ TAM ÇALIŞMALI.
 *
 * Batarya (her biri -1 dönmeli, kernel deref ETMEMELİ):
 *   sys(5, 0x80000000)               — unmapped okuma ptr (yaz)               [D-151]
 *   sys(5, 0x0)                      — null okuma ptr (yaz)                   [D-151]
 *   sys2(16, 0x40100000, 0)          — kernel-RAM ad okuma (dosya_oku)        [D-151]
 *   sys2(15, 0xC0000000, 7)          — unmapped ad (dosya_yaz)               [D-151]
 *   sys2(17, 0x90000000, "x")        — unmapped ad (dosya_yaz_metin)         [D-151]
 *   sys2(18, "x", 0x40000000)        — kernel-adres YAZMA hedefi (dosya_oku_metin) [D-150]
 *   sys2(20, 0, 0x08000000)          — Device MMIO YAZMA hedefi (dosya_ad)   [D-150]
 *   sys(21, 0xFFFF000000000000)      — unmapped ad (dosya_sil)               [D-151]
 *
 * Not: unmapped adresler (0x80000000, 0xC0000000, 0x90000000, 0xFFFF...) L1[2+]
 * geçersiz bölgede; fix ÖNCESİ bunlar kernel'i EL1 data-abort ile SONSUZ HALT
 * ederdi (DoS). 0x08000000 = GICv2 MMIO (Device belleği — user değil).
 * "x" dosyası önceden içerikle kurulur → num=18 write-guard vakası "not-found"
 * DEĞİL, gerçekten write-guard'a ULAŞIR (dosya var, hedef tampon geçersiz).
 *
 * Bataryadan SONRA kernel'in TAM canlı olduğunu kanıtla:
 *   - "gunluk" dosyasını "CANLI" içerikle oluştur (num=17);
 *   - "gunluk"u GEÇERLİ user tampona oku (num=18) → r>0;
 *   - "x"i GEÇERLİ user tampona oku (num=18) → rx>0 (write-guard reddinin
 *     not-found DEĞİL guard olduğunu ayırt eder — "x" var + okunabiliyor).
 *
 * Hepsi reddedildi (8× -1) VE r>0 VE rx>0 → "HOSTILE SURVIVED OK".
 * Success print'e ULAŞMAK = kernel bataryadan SAĞ ÇIKTI (halt etmedi).
 * Kanıt: seri çıktıda "HOSTILE SURVIVED OK".
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
    /* (1) baseline: iki geçerli dosya kur (write-guard vakasını doğru tetiklemek
     *     için "x" gerçekten var olmalı → num=18 red = write-guard, not-found değil). */
    sys2(17, (unsigned long)(uintptr_t)"x", (unsigned long)(uintptr_t)"zz");   /* "x" içerikle var */

    /* (2) BATARYA — her biri -1 dönmeli VE kernel HALT ETMEMELİ. reddedildi
     *     bayrağında topla (herhangi biri -1 değilse güvenlik boşluğu var). */
    int reddedildi = 1;

    reddedildi &= ((long)sys(5, 0x80000000UL) == -1);                      /* unmapped okuma (yaz)       */
    reddedildi &= ((long)sys(5, 0x0UL) == -1);                             /* null okuma (yaz)           */
    reddedildi &= ((long)sys2(16, 0x40100000UL, 0) == -1);                 /* kernel-RAM ad (dosya_oku)  */
    reddedildi &= ((long)sys2(15, 0xC0000000UL, 7) == -1);                 /* unmapped ad (dosya_yaz)    */
    reddedildi &= ((long)sys2(17, 0x90000000UL,
                             (unsigned long)(uintptr_t)"x") == -1);        /* unmapped ad (yaz_metin)    */
    reddedildi &= ((long)sys2(18, (unsigned long)(uintptr_t)"x",
                             0x40000000UL) == -1);                         /* kernel-adres YAZMA (D-150) */
    reddedildi &= ((long)sys2(20, 0, 0x08000000UL) == -1);                 /* MMIO YAZMA (D-150)         */
    reddedildi &= ((long)sys(21, 0xFFFF000000000000UL) == -1);            /* unmapped ad (dosya_sil)    */

    /* (3) Bataryadan SONRA kernel HÂLÂ TAM ÇALIŞIR MI? Geçerli iş akışıyla kanıtla. */
    sys2(17, (unsigned long)(uintptr_t)"gunluk",
             (unsigned long)(uintptr_t)"CANLI");                          /* geçerli oluştur            */
    long r  = (long)sys2(18, (unsigned long)(uintptr_t)"gunluk", 0x42210000UL);  /* user tampona oku  */
    long rx = (long)sys2(18, (unsigned long)(uintptr_t)"x",      0x42210000UL);  /* "x"i oku (>0)     */

    /* (4) Hepsi reddedildi + iki geçerli okuma da veri döndü → kernel SAĞ + doğru. */
    if (reddedildi && r > 0 && rx > 0) {
        sys(5, (unsigned long)(uintptr_t)"HOSTILE SURVIVED OK");
    } else {
        sys(5, (unsigned long)(uintptr_t)"HOSTILE HATA");
    }
    sys(7, 0);
    for (;;) { }
}

int main(void) {
    kdl_yazdir_metin("HOSTILE BASLA");
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
