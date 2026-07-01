/*
 * KEMGU Bare-Metal Görev Zamanlama — Cooperative (kdl_gorev.c)
 * ===========================================================
 *
 * C7a: işbirlikçi (cooperative) zamanlama. Görevler kdl_gorev_ver() (yield) ile
 * gönüllü CPU bırakır → round-robin sonraki göreve bağlam-değiştir. MMU/preemption
 * GEREKTİRMEZ (C7b preemptive = timer-IRQ + quantum, sonra).
 *
 * TCB = callee-saved register'lar + SP. Bağlam-değiştirme (kdl_baglam_degis,
 * boot/start_*.S asm): mevcut görevin callee-saved'ını TCB'ye kaydet, sonrakinin
 * kinden yükle, ret → sonraki görev kaldığı yerden sürer. Caller-saved register'lar
 * yield çağrı-noktasında C ABI'siyle zaten korunur → kaydetmeye gerek yok.
 *
 * Görev 0 = main bağlamı (init gerektirmez; ilk yield'de TCB'ye kaydedilir).
 * KEMGU bağı: ileride region-ownership + görev (D-008) gerçek thread'le buluşur.
 */
#include <stdint.h>

#define KDL_MAX_GOREV 8

/* TCB: arch-spesifik kaydedilen register seti.
 *   aarch64: [0..9]=x19-x28, [10]=x29(fp), [11]=x30(lr/giriş), [12]=sp
 *   x86_64 : [0]=rbx,[1]=rbp,[2..5]=r12-r15,[6]=rsp */
typedef struct {
    uint64_t kayit[16];
} KdlTCB;

extern void kdl_baglam_degis(KdlTCB *eski, KdlTCB *yeni);

static KdlTCB kdl_tcb[KDL_MAX_GOREV];
static int kdl_gorev_sayisi = 0;
static int kdl_aktif = 0;

/* Zamanlamayı başlat: çağıran (main) = görev 0. */
void kdl_gorev_baslat(void) {
    kdl_gorev_sayisi = 1;
    kdl_aktif = 0;
}

/* Yeni görev ekle: ilk geçişte `giris`'e `yigin_tepe` SP'siyle atlar.
 * Görev indeksini döner; tablo doluysa -1. */
int kdl_gorev_olustur(void (*giris)(void), void *yigin_tepe) {
    if (kdl_gorev_sayisi >= KDL_MAX_GOREV) return -1;
    int i = kdl_gorev_sayisi++;
    KdlTCB *t = &kdl_tcb[i];
    for (int k = 0; k < 16; k++) t->kayit[k] = 0;

#if defined(__aarch64__)
    t->kayit[11] = (uint64_t)(uintptr_t)giris;        /* x30 = giriş (ret buraya) */
    t->kayit[12] = (uint64_t)(uintptr_t)yigin_tepe;   /* sp */
#elif defined(__x86_64__)
    /* x86: ret giriş adresini yığından pop eder → giriş'i yığın tepesine koy. */
    uint64_t *sp = (uint64_t *)yigin_tepe;
    *(--sp) = (uint64_t)(uintptr_t)giris;             /* dönüş adresi = giriş */
    t->kayit[6] = (uint64_t)(uintptr_t)sp;            /* rsp */
#endif
    return i;
}

/* CPU'yu gönüllü bırak (yield) → round-robin sonraki göreve geç. */
void kdl_gorev_ver(void) {
    if (kdl_gorev_sayisi < 2) return;
    int eski = kdl_aktif;
    kdl_aktif = (kdl_aktif + 1) % kdl_gorev_sayisi;
    if (kdl_aktif != eski)
        kdl_baglam_degis(&kdl_tcb[eski], &kdl_tcb[kdl_aktif]);
}

/* ================= Preemptive scheduling (C7b) ================= */
/* Cooperative'den farkı: timer-IRQ zorunlu switch yapar (görev yield etmez).
 * kdl_irq_isle (boot full trap-frame'den) her timer IRQ'da kdl_preempt(sp)
 * çağırır → mevcut görevin trap-frame SP'sini kaydet, sonrakine geç (round-robin).
 * Görev trap-frame'i yığında (x0-x30 + ELR + SPSR), SP swap ile geçiş. */

static int      kdl_preempt_aktif = 0;
static uint64_t kdl_psp[KDL_MAX_GOREV];   /* her görevin trap-frame SP'si */
static int      kdl_block[KDL_MAX_GOREV]; /* >0 = bloklu (tick geri-sayım) — C7c */
static int      kdl_pri[KDL_MAX_GOREV];   /* öncelik (büyük=yüksek; varsayılan 0) — C7e */
static int      kdl_psayi = 0;
static int      kdl_paktif = 0;

/* D3-çoklu: sürece-özel L1 sayfa-tablosu (0=swap yok, kernel/main tablosunda kal).
 * Set edilmişse kdl_preempt o göreve geçerken TTBR0'ı bu tabloya çevirir → her
 * userspace süreç KENDİ adres-uzayında koşar. Guard'lı: hiç set edilmezse (mevcut
 * testler) davranış AYNI (swap yok → regresyon yok). */
#if defined(__aarch64__)
extern void kdl_ttbr_degis(uint64_t *l1);
static uint64_t *kdl_task_l1[KDL_MAX_GOREV];
#endif

/* Görev 0 = main (ilk preempt'te trap-frame SP'si kaydedilir). */
void kdl_preempt_baslat(void) {
    kdl_psayi = 1;
    kdl_paktif = 0;
}

/* Yeni preemptive görev: yığın tepesinde sentetik trap-frame (272 bayt) kur →
 * ilk switch eret ile `giris`'e EL1h + IRQ-açık atlar. */
int kdl_preempt_gorev_olustur(void (*giris)(void), void *yigin_tepe) {
    if (kdl_psayi >= KDL_MAX_GOREV) return -1;
    int i = kdl_psayi++;
    uint64_t sp = ((uint64_t)(uintptr_t)yigin_tepe - 272) & ~0xFUL;
    uint64_t *tf = (uint64_t *)(uintptr_t)sp;
    for (int k = 0; k < 34; k++) tf[k] = 0;
    tf[31] = (uint64_t)(uintptr_t)giris;   /* ELR_EL1 @ offset 248 → eret hedefi */
    tf[32] = 0x5;                          /* SPSR: M=EL1h, DAIF=0 (IRQ açık) */
    kdl_psp[i] = sp;
    return i;
}

/* D-125: PREEMPTIVE EL0 (userspace) görev. Preemptive EL1 görev gibi ama sentetik
 * trap-frame SPSR=EL0t (IRQ-açık) + SP_EL0 = user yığını → ilk switch eret ile
 * `giris`'e EL0'da atlar. İKİ yığın gerekir:
 *   kernel_yigin_tepe — trap-frame (preempt sırasında SP_EL1) burada, kernel bellek.
 *   user_yigin_tepe   — EL0 çalışma yığını (.user sayfası, AP=01), SP_EL0.
 * kdl_irq_ortak SP_EL0'ı @264'te kaydeder/geri yükler (Stage 1) → EL0 görev
 * preempt edilip sürdürülebilir. giris .user sayfasında (AP=01, EL0-exec) olmalı. */
int kdl_preempt_gorev_olustur_el0(void (*giris)(void), void *kernel_yigin_tepe,
                                  void *user_yigin_tepe) {
    if (kdl_psayi >= KDL_MAX_GOREV) return -1;
    int i = kdl_psayi++;
    uint64_t sp = ((uint64_t)(uintptr_t)kernel_yigin_tepe - 272) & ~0xFUL;
    uint64_t *tf = (uint64_t *)(uintptr_t)sp;
    for (int k = 0; k < 34; k++) tf[k] = 0;
    tf[31] = (uint64_t)(uintptr_t)giris;               /* ELR_EL1 = EL0 giriş */
    tf[32] = 0x0;                                      /* SPSR: M=EL0t, DAIF=0 (IRQ açık) */
    tf[33] = (uint64_t)(uintptr_t)user_yigin_tepe;     /* SP_EL0 @264 = user yığını */
    kdl_psp[i] = sp;
    return i;
}

void kdl_preempt_ac(void) { kdl_preempt_aktif = 1; }

/* D-128: o an koşan preemptive görevin id'si (userspace getpid syscall'ı için). */
int kdl_aktif_gorev(void) { return kdl_paktif; }

/* C7e: göreve öncelik ata (büyük = yüksek; varsayılan 0). Yüksek-öncelikli
 * READY görev her zaman seçilir; eşit öncelikler round-robin döner. */
void kdl_preempt_oncelik(int gorev, int oncelik) {
    if (gorev >= 0 && gorev < KDL_MAX_GOREV) kdl_pri[gorev] = oncelik;
}

#if defined(__aarch64__)
/* D3-çoklu: göreve sürece-özel L1 sayfa-tablosu ata → kdl_preempt o göreve
 * geçerken TTBR0'ı bu tabloya çevirir (userspace süreç izolasyonu). */
void kdl_preempt_gorev_ttbr(int gorev, uint64_t *l1) {
    if (gorev >= 0 && gorev < KDL_MAX_GOREV) kdl_task_l1[gorev] = l1;
}
#endif

/* IRQ trap-frame'inden (sp) çağrılır. Preempt aktifse: bloklu görevlerin tick
 * sayacını azalt, EN YÜKSEK ÖNCELİKLİ READY göreve geç (trap-frame SP swap);
 * değilse sp (switch yok → timer testi nötr). C7c: bloklu görev atlanır.
 * C7e: round-robin sırada tarayıp en yüksek öncelikliyi seç → eşit öncelikte
 * round-robin korunur (kdl_paktif sonrası ilk eşit-en-yüksek kazanır). */
uint64_t kdl_preempt(uint64_t sp) {
    if (!kdl_preempt_aktif || kdl_psayi < 2) return sp;
    kdl_psp[kdl_paktif] = sp;
    for (int i = 0; i < kdl_psayi; i++)          /* C7c: uyku geri-sayımı */
        if (kdl_block[i] > 0) kdl_block[i]--;
    int en_iyi = -1, en_iyi_pri = -1;
    int n = kdl_paktif;
    for (int t = 0; t < kdl_psayi; t++) {        /* round-robin sırada tara */
        n = (n + 1) % kdl_psayi;
        if (kdl_block[n] != 0) continue;         /* READY değil (bloklu) */
        if (kdl_pri[n] > en_iyi_pri) {           /* en yüksek öncelik (eşitte ilk) */
            en_iyi = n;
            en_iyi_pri = kdl_pri[n];
        }
    }
    if (en_iyi < 0) return kdl_psp[kdl_paktif];  /* hepsi bloklu → idle */
    kdl_paktif = en_iyi;
#if defined(__aarch64__)
    /* D3-çoklu: seçilen görevin sürece-özel adres-uzayına geç (varsa). Kernel
     * identity her tabloda → IRQ handler + trap-frame restore güvenli. */
    if (kdl_task_l1[en_iyi]) kdl_ttbr_degis(kdl_task_l1[en_iyi]);
#endif
    return kdl_psp[en_iyi];
}

/* C7c: çağıran görevi N timer-tick blokla (uyut). Scheduler bloklu süresince
 * atlar; sayaç 0'a inince READY → kaldığı yerden sürer. */
void kdl_uyu(int tikler) {
    int self = kdl_paktif;
    kdl_block[self] = tikler;
    while (kdl_block[self] > 0) { __asm__ volatile("" ::: "memory"); }
}
