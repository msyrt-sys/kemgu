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
