/*
 * MİLESTONE B testi (aarch64) — USERSPACE PAYLAŞIMLI-BELLEK IPC (D-127'nin TERSİ).
 *
 * D-127 (multiproc_arm.c) İZOLASYON kanıtlar: iki EL0 süreç aynı veri-VA'yı FARKLI
 * fiziksel sayfaya map eder → çapraz-bozulma yok. BU test onun TERSİdir: iki EL0
 * süreç KASITLI olarak AYNI fiziksel veri sayfasını PAYLAŞIR. Her ikisinin L2[17]
 * girişi AYNI veri_pa'ya (0x44000000) gider — kdl_surec_kur_el0_veri iki süreç için
 * AYNI veri_pa ile çağrılır (D-127'de FARKLI pa = izolasyon; burada AYNI pa = paylaşım).
 *
 * Paylaşımlı-bellek IPC (kanal/dosya syscall'ı DEĞİL — DOĞRUDAN bellek):
 *   Üretici süreç paylaşımlı sayfaya 1..10 değerlerini yazar, sonra "hazır" bayrağını
 *   set eder. Tüketici süreç bayrağı (bounded spin-poll, deadlock-guard) bekleyip
 *   10 değeri okur + toplar → 1+2+...+10 = 55.
 *
 * PAYLAŞIM KANITI: ayrı-PA olsaydı (D-127 izolasyonu) tüketici üreticinin yazdığını
 * GÖREMEZDİ (bayrak hiç set olmaz / toplam 0) → deadlock-guard "USERSHM PAYLASIM YOK".
 * AYNI-PA (paylaşım) ise tüketici tam 55 okur → "USERSHM OK".
 *
 * İki süreç D-127 scheduler'ı ile preemptively dönüşümlü koşar (timer-IRQ).
 *
 * Paylaşımlı 2MB veri sayfası (0x42200000..0x423FFFFF) yerleşimi:
 *   0x42200000  IPC bölgesi (bayrak + 10 değer)  — paylaşılan
 *   0x42300000  tüketici EL0 user yığını (1MB offset, aşağı büyür)
 *   0x42380000  üretici  EL0 user yığını (1.5MB offset, aşağı büyür)
 * Sayfa PAYLAŞILDIĞI için iki süreç AYRI yığın-VA kullanır (aynı PA'da farklı bölge)
 * → yığın çerçeveleri çakışmaz; IPC bölgesi tabanı yığınların altında güvende.
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

/* Süreç sayfa tabloları + kernel yığınları (kernel bellek, AP=00). */
static uint64_t l1_u[512] __attribute__((aligned(4096)));  /* üretici */
static uint64_t l2_u[512] __attribute__((aligned(4096)));
static uint64_t l1_t[512] __attribute__((aligned(4096)));  /* tüketici */
static uint64_t l2_t[512] __attribute__((aligned(4096)));
static unsigned char kstack_u[8192] __attribute__((aligned(16)));
static unsigned char kstack_t[8192] __attribute__((aligned(16)));

/* Paylaşımlı IPC bölgesi düzeni (veri sayfası tabanı 0x42200000, L2[17], AP=01). */
#define SHM_BAYRAK  (*(volatile unsigned int *)0x42200000UL)          /* "hazır" bayrağı */
#define SHM_DEGER(i) (((volatile unsigned int *)0x42200040UL)[(i)])   /* 10 değer dizisi */

/* İki süreç AYRI user-yığın VA'sı kullanır (paylaşılan PA'da farklı bölge). */
#define USTACK_T  ((void *)0x42300000UL)   /* tüketici yığın tepesi (1.0MB) */
#define USTACK_U  ((void *)0x42380000UL)   /* üretici  yığın tepesi (1.5MB) */

#define BAYRAK_HAZIR  0x600DU              /* üretici tamamlandı işareti */

/* Userspace syscall (always_inline → SVC .user'a gömülü). */
__attribute__((always_inline)) static inline void sys(unsigned long num, unsigned long arg) {
    register unsigned long x8 __asm__("x8") = num;
    register unsigned long x0 __asm__("x0") = arg;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8) : "memory");
}

/* ÜRETİCİ: paylaşımlı sayfaya 1..10 yazar, sonra bayrağı set eder. */
__attribute__((section(".user"), noinline))
static void el0_uretici(void) {
    for (unsigned int i = 0; i < 10; i++) {
        SHM_DEGER(i) = i + 1;          /* 1, 2, ..., 10 */
    }
    /* Değerler bayraktan ÖNCE görünür olmalı (yayın sırası); ardından bayrak. */
    __asm__ volatile("dmb ish" ::: "memory");
    SHM_BAYRAK = BAYRAK_HAZIR;         /* tüketiciye "veri hazır" sinyali */
    for (;;) { }                       /* iş bitti; preemptible bekle */
}

/* TÜKETİCİ: bayrağı bounded bekler, 10 değeri okuyup toplar → 55 mi? */
__attribute__((section(".user"), noinline))
static void el0_tuketici(void) {
    int hazir = 0;
    /* Bounded spin-poll (deadlock-guard): timer-IRQ üreticiye geçiş yaptırır.
     * Paylaşım YOKSA bayrak asla set olmaz → döngü biter, hazir=0. */
    for (long bekle = 0; bekle < 2000000L; bekle++) {
        if (SHM_BAYRAK == BAYRAK_HAZIR) { hazir = 1; break; }
    }
    if (!hazir) {
        sys(5, (unsigned long)(uintptr_t)"USERSHM PAYLASIM YOK");
        sys(7, 0);
        for (;;) { }
    }
    __asm__ volatile("dmb ish" ::: "memory");   /* bayraktan sonra değerleri oku */
    unsigned int toplam = 0;
    for (unsigned int i = 0; i < 10; i++) {
        toplam += SHM_DEGER(i);
    }
    if (toplam == 55) {
        sys(5, (unsigned long)(uintptr_t)"USERSHM OK");
    } else {
        sys(5, (unsigned long)(uintptr_t)"USERSHM TOPLAM HATA");
        sys(6, toplam);
    }
    sys(7, 0);
    for (;;) { }
}

int main(void) {
    kdl_yazdir_metin("USERSHM BASLA");
    kdl_yazdir_satir();

    /* İKİSİ DE AYNI veri_pa (0x44000000) → PAYLAŞIMLI fiziksel veri sayfası.
     * (D-127'de üretici/tüketici FARKLI pa alırdı = izolasyon; burada AYNI = IPC.)
     * Kod sayfası zaten paylaşılan (0x42000000). */
    kdl_surec_kur_el0_veri(l1_u, l2_u, 0x42000000UL, 0x44000000UL);
    kdl_surec_kur_el0_veri(l1_t, l2_t, 0x42000000UL, 0x44000000UL);

    kdl_preempt_baslat();              /* main = görev 0 (EL1) */
    int tu = kdl_preempt_gorev_olustur_el0(el0_uretici, kstack_u + sizeof(kstack_u), USTACK_U);
    kdl_preempt_gorev_ttbr(tu, l1_u);  /* üretici → kendi tablosu (aynı veri PA'ya bakar) */
    int tt = kdl_preempt_gorev_olustur_el0(el0_tuketici, kstack_t + sizeof(kstack_t), USTACK_T);
    kdl_preempt_gorev_ttbr(tt, l1_t);  /* tüketici → kendi tablosu (aynı veri PA'ya bakar) */

    kdl_kesme_kur();
    kdl_timer_baslat();
    kdl_preempt_ac();

    for (;;) { __asm__ volatile("wfe"); }   /* üretici+tüketici preemptively koşup rapor verir */
}
