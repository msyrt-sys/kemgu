/*
 * KEMGU-OS bare-metal SMP testi (aarch64) — ÇOK-ÇEKİRDEK bring-up.
 * ================================================================
 *
 * Milestone: 2. CPU çekirdeğini PSCI (Power State Coordination Interface)
 * CPU_ON ile başlat. QEMU virt makinesi PSCI destekler; ikincil çekirdekler
 * boot'ta PARK halinde (WFI/pending) bekler, PSCI CPU_ON ile uyandırılır.
 *
 * Akış:
 *   1. Çekirdek 0 (boot) "SMP BASLA" basar.
 *   2. PSCI CPU_ON çağrısı (fn=0xC4000003): x1=hedef_cpu (MPIDR aff=0x1),
 *      x2=giriş_noktası (cekirdek1_giris fiziksel adresi), x3=context_id.
 *      Conduit: önce HVC (QEMU virt EL2'de firmware'siz başlar → PSCI
 *      genelde HVC), başarısızsa SMC. Dönüş x0: 0=SUCCESS, negatif=hata.
 *   3. Çekirdek 1 (cekirdek1_giris): KENDİ stack'ini kurar (ayrı statik
 *      dizi), paylaşılan bayrağı 1 yapar, sonra sonsuz wfe spin.
 *   4. Çekirdek 0: CPU_ON sonrası bayrağı poll eder (timeout'lu). Bayrak
 *      set olursa "SMP OK 2 cekirdek" + PSCI dönüş kodunu basar.
 *
 * KRİTİK — cache coherency: Çekirdek 0 MMU-ON (Normal-WB cacheable) koşar;
 * çekirdek 1 MMU-OFF başlar (tüm erişim Device/non-cacheable → doğrudan RAM).
 * Çekirdek 1'in RAM'e yazdığı bayrağı çekirdek 0'ın cache'i görmeyebilir.
 * Çözüm: çekirdek 0 poll döngüsünde her okumadan önce bayrak satırını
 * `dc ivac` (invalidate to Point-of-Coherency) ile geçersiz kılar → RAM'den
 * taze okur. Çekirdek 1 tarafında yazımdan sonra `dsb sy` ile RAM'e boşaltılır.
 *
 * Kanıt: "SMP OK" → 2. çekirdek gerçekten koştu (bayrağı yazan o).
 * Fallback (CPU_ON çalışmazsa): PSCI_VERSION (0x84000000) çağrısı → "PSCI OK".
 */
#include <stdint.h>

extern void kdl_yazdir_metin(const char *);
extern void kdl_yaz_metin(const char *);
extern void kdl_yazdir_satir(void);
extern void kdl_yazdir_onaltilik(uint64_t);
extern void kdl_yaz_onaltilik(uint64_t);

/* Paylaşılan bayrak — iki çekirdek arasında. volatile: derleyici cache'lemesin.
 * Kendi cache-satırında izole olması için hizala (yanlış-paylaşım/komşu klobber
 * önlemi; dc ivac tüm satırı etkiler). */
static volatile uint64_t cekirdek1_canli __attribute__((aligned(64))) = 0;

/* Çekirdek 1'in kendi yığını (çekirdek 0'ınkinden ayrı) — 8 KB. */
static uint8_t cekirdek1_yigin[8192] __attribute__((aligned(16)));

/*
 * PSCI conduit çağrısı — HVC veya SMC. QEMU virt tipik conduit HVC (EL2 var,
 * firmware yok). c==0 → HVC, c!=0 → SMC. Argüman/dönüş ARM SMCCC'ye göre x0-x3.
 */
static inline int64_t psci_hvc(uint64_t fn, uint64_t a1, uint64_t a2, uint64_t a3) {
    register uint64_t x0 __asm__("x0") = fn;
    register uint64_t x1 __asm__("x1") = a1;
    register uint64_t x2 __asm__("x2") = a2;
    register uint64_t x3 __asm__("x3") = a3;
    __asm__ volatile("hvc #0"
                     : "+r"(x0)
                     : "r"(x1), "r"(x2), "r"(x3)
                     : "memory");
    return (int64_t)x0;
}

static inline int64_t psci_smc(uint64_t fn, uint64_t a1, uint64_t a2, uint64_t a3) {
    register uint64_t x0 __asm__("x0") = fn;
    register uint64_t x1 __asm__("x1") = a1;
    register uint64_t x2 __asm__("x2") = a2;
    register uint64_t x3 __asm__("x3") = a3;
    __asm__ volatile("smc #0"
                     : "+r"(x0)
                     : "r"(x1), "r"(x2), "r"(x3)
                     : "memory");
    return (int64_t)x0;
}

/*
 * Çekirdek 1 giriş noktası — PSCI CPU_ON tarafından MMU-OFF, EL1 (QEMU virt)
 * durumunda çağrılır. GEREKLİ: önce KENDİ SP'sini kur (PSCI çekirdek 1'in
 * SP'sini kurmaz), sonra paylaşılan RAM'e yaz. UART'a DOKUNMA (çekirdek 0 ile
 * paylaşımlı → çakışma riski); yalnız bayrağı set et.
 *
 * noreturn + naked'e gerek yok: prologue SP'yi kullanmadan önce inline asm ile
 * SP'yi kurarız; C prologue callee-saved sadece SP-üstüne yazar (artık geçerli).
 */
__attribute__((noreturn))
static void cekirdek1_giris(void) {
    /* 1. KENDİ yığınını kur — yığın dizisinin TEPESİ (aşağı büyür), 16-hizalı. */
    __asm__ volatile(
        "mov sp, %0\n"
        :
        : "r"(&cekirdek1_yigin[sizeof(cekirdek1_yigin)])
        : "memory");

    /* 2. Paylaşılan bayrağı set et + RAM'e boşalt (dsb sy → coherency noktasına). */
    cekirdek1_canli = 1;
    __asm__ volatile("dsb sy" ::: "memory");

    /* 3. Sonsuz düşük-güç spin. Çekirdek 0 bayrağı okuyup test'i bitirir; QEMU
     *    timeout ile kapanır. sev → çekirdek 0'ın wfe'sini uyandırabilir. */
    __asm__ volatile("sev" ::: "memory");
    for (;;) {
        __asm__ volatile("wfe" ::: "memory");
    }
}

/* Bayrağın cache satırını geçersiz kıl (dc ivac) → çekirdek 0 RAM'den taze okur.
 * Çekirdek 1 MMU-OFF non-cacheable yazdı; çekirdek 0 cache'i eski değeri (0)
 * tutuyor olabilir. ivac + dsb ile Point-of-Coherency'den yeniden yükle. */
static inline void bayragi_taze_oku(void) {
    __asm__ volatile(
        "dc ivac, %0\n"
        "dsb sy\n"
        :
        : "r"(&cekirdek1_canli)
        : "memory");
}

/* PSCI dönüş kodları (SMCCC) — raporlama için isim. */
#define PSCI_SUCCESS            0
#define PSCI_NOT_SUPPORTED      (-1)
#define PSCI_INVALID_PARAMETERS (-2)
#define PSCI_DENIED             (-3)
#define PSCI_ALREADY_ON         (-4)

#define PSCI_FN_CPU_ON   0xC4000003ULL   /* 64-bit CPU_ON */
#define PSCI_FN_VERSION  0x84000000ULL   /* PSCI_VERSION */

int main(void) {
    kdl_yazdir_metin("SMP BASLA");

    uint64_t giris = (uint64_t)(uintptr_t)&cekirdek1_giris;
    uint64_t hedef_cpu = 0x1;   /* QEMU virt: çekirdek 1 MPIDR affinity = 0x1 */
    uint64_t context = 0xC0DE;  /* çekirdek 1'e x0'da geçer (kullanmıyoruz, izleme) */

    /* --- 1. GERÇEK CPU_ON denemesi: önce HVC, başarısızsa SMC. --- */
    const char *conduit = "HVC";
    int64_t ret = psci_hvc(PSCI_FN_CPU_ON, hedef_cpu, giris, context);

    /* NOT_SUPPORTED (-1) → conduit yanlış olabilir; SMC dene. Diğer negatifler
     * (INVALID/DENIED/ALREADY_ON) conduit'in ÇALIŞTIĞINI ama çağrı reddini
     * gösterir → SMC denemeye gerek yok. */
    if (ret == PSCI_NOT_SUPPORTED) {
        conduit = "SMC";
        ret = psci_smc(PSCI_FN_CPU_ON, hedef_cpu, giris, context);
    }

    kdl_yaz_metin("CPU_ON conduit=");
    kdl_yaz_metin(conduit);
    kdl_yaz_metin(" ret=");   /* onaltilik yazici "0x" onekini kendi ekler */
    kdl_yazdir_onaltilik((uint64_t)ret);

    if (ret == PSCI_SUCCESS) {
        /* --- Bayrağı poll et (timeout'lu). Her okumadan önce cache'i geçersiz kıl. --- */
        int canli = 0;
        for (volatile uint64_t bekle = 0; bekle < 200000000ULL; bekle++) {
            bayragi_taze_oku();
            if (cekirdek1_canli != 0) { canli = 1; break; }
        }

        if (canli) {
            kdl_yaz_metin("SMP OK 2 cekirdek (PSCI CPU_ON ");
            kdl_yaz_metin(conduit);
            kdl_yaz_metin(", ret=");
            kdl_yaz_onaltilik((uint64_t)ret);
            kdl_yazdir_metin(")");
        } else {
            kdl_yazdir_metin("SMP CEKIRDEK1 YOK (CPU_ON basarili ama bayrak set olmadi)");
        }
    } else {
        /* --- CPU_ON başarısız → FALLBACK: PSCI_VERSION ile PSCI'yi kanıtla. --- */
        kdl_yazdir_metin("CPU_ON basarisiz — PSCI_VERSION fallback deneniyor");

        const char *fb_conduit = "HVC";
        int64_t ver = psci_hvc(PSCI_FN_VERSION, 0, 0, 0);
        if (ver == PSCI_NOT_SUPPORTED) {
            fb_conduit = "SMC";
            ver = psci_smc(PSCI_FN_VERSION, 0, 0, 0);
        }

        if (ver >= 0) {
            /* PSCI_VERSION dönüşü: [31:16]=major, [15:0]=minor. */
            uint64_t major = ((uint64_t)ver >> 16) & 0xFFFF;
            uint64_t minor = (uint64_t)ver & 0xFFFF;
            kdl_yaz_metin("PSCI OK conduit=");
            kdl_yaz_metin(fb_conduit);
            kdl_yaz_metin(" surum=");
            kdl_yaz_onaltilik(major);
            kdl_yaz_metin(".");
            kdl_yazdir_onaltilik(minor);
        } else {
            kdl_yaz_metin("PSCI YOK — VERSION de basarisiz ret=");
            kdl_yazdir_onaltilik((uint64_t)ver);
        }
    }

    for (;;) { __asm__ volatile("wfe"); }
}
