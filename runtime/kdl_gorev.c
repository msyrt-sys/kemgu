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
static int      kdl_olu[KDL_MAX_GOREV];   /* 1 = bitmiş (exit); scheduler atlar — D-130 */
static int      kdl_psayi = 0;
static int      kdl_paktif = 0;

/* D3-çoklu: sürece-özel L1 sayfa-tablosu (0=swap yok, kernel/main tablosunda kal).
 * Set edilmişse kdl_preempt o göreve geçerken TTBR0'ı bu tabloya çevirir → her
 * userspace süreç KENDİ adres-uzayında koşar. Guard'lı: hiç set edilmezse (mevcut
 * testler) davranış AYNI (swap yok → regresyon yok). */
#if defined(__aarch64__)
extern void kdl_ttbr_degis(uint64_t *l1);
extern void kdl_surec_kur_el0_veri(uint64_t *l1, uint64_t *l2, uint64_t kod_pa, uint64_t veri_pa);
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
    /* D-138: önce ÖLÜ (exit etmiş) görev slotunu yeniden kullan → sınırsız spawn.
     * Yoksa yeni slot (max'a kadar). main (0) asla yeniden kullanılmaz. */
    int i = -1;
    for (int k = 1; k < kdl_psayi; k++) if (kdl_olu[k]) { i = k; break; }
    if (i < 0) {
        if (kdl_psayi >= KDL_MAX_GOREV) return -1;
        i = kdl_psayi++;
    }
    kdl_olu[i] = 0; kdl_block[i] = 0; kdl_pri[i] = 0;   /* slot durumunu sıfırla */
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

/* D-130: o an koşan görevi BİTİR (exit). scheduler bir daha seçmez → süreç durur.
 * (Kaynaklar geri alınmaz — v1; havuz slotu serbest bırakılmaz.) */
void kdl_gorev_bitir(void) {
    if (kdl_paktif >= 0 && kdl_paktif < KDL_MAX_GOREV) kdl_olu[kdl_paktif] = 1;
}

/* D-130: görev `pid` bitti mi? (join/wait için — ebeveyn EL0'da yoklar). */
int kdl_gorev_durum(int pid) {
    if (pid >= 0 && pid < KDL_MAX_GOREV) return kdl_olu[pid];
    return 1;   /* geçersiz pid → "bitmiş" say (sonsuz bekleme olmasın) */
}

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

/* D-129: DİNAMİK SÜREÇ OLUŞTURMA (spawn). Runtime'da (syscall'dan) yeni izole EL0
 * süreç yaratır — statik main kurulumu değil. Havuzdan sayfa-tabloları + kernel
 * yığını + sürece-özel veri sayfası (0x46000000 + i*2MB) alır, preemptive EL0
 * görevi kurar + TTBR bağlar. entry = EL0 giriş adresi (.user, paylaşılan kod).
 * Yeni görev id'sini (pid) döner; havuz doluysa -1. */
#define KDL_SPAWN_MAX 4
static uint64_t kdl_spawn_l1[KDL_SPAWN_MAX][512] __attribute__((aligned(4096)));
static uint64_t kdl_spawn_l2[KDL_SPAWN_MAX][512] __attribute__((aligned(4096)));
static unsigned char kdl_spawn_kstack[KDL_SPAWN_MAX][8192] __attribute__((aligned(16)));
static int kdl_spawn_kullanildi[KDL_SPAWN_MAX];   /* D-138: 1 = slot kullanımda */
static int kdl_spawn_task[KDL_SPAWN_MAX];         /* slot → görev id (geri-alma için) */

int kdl_surec_spawn(uint64_t entry) {
    /* D-138: boş VEYA görevi ölmüş (exit) havuz slotunu yeniden kullan → sınırsız
     * spawn (eski: monoton sayaç, 4 spawn'da tükeniyordu). */
    int i = -1;
    for (int k = 0; k < KDL_SPAWN_MAX; k++) {
        if (!kdl_spawn_kullanildi[k]) { i = k; break; }
        if (kdl_olu[kdl_spawn_task[k]]) { i = k; break; }   /* slotun görevi öldü → geri al */
    }
    if (i < 0) return -1;                                    /* tüm slotlar canlı */
    uint64_t veri_pa = 0x46000000UL + (uint64_t)i * 0x200000UL;   /* sürece-özel veri (RAM içi) */
    kdl_surec_kur_el0_veri(kdl_spawn_l1[i], kdl_spawn_l2[i], 0x42000000UL, veri_pa);
    int t = kdl_preempt_gorev_olustur_el0((void (*)(void))(uintptr_t)entry,
                                          kdl_spawn_kstack[i] + 8192,
                                          (void *)(uintptr_t)0x42380000UL);  /* USTACK (özel veri sayfasında) */
    if (t < 0) return -1;
    kdl_task_l1[t] = kdl_spawn_l1[i];        /* yeni sürecin adres-uzayı */
    kdl_spawn_kullanildi[i] = 1;
    kdl_spawn_task[i] = t;
    return t;
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
        if (kdl_olu[n]) continue;                /* bitmiş (exit) — asla seçme (D-130) */
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
