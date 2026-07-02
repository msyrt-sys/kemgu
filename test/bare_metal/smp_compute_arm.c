/*
 * KEMGU-OS bare-metal SMP GERÇEK PARALEL HESAPLAMA testi (aarch64).
 * =================================================================
 *
 * Milestone (D-169 üstünde): D-169 yalnız bir bayrak set etti (2. çekirdek
 * KOŞTU kanıtı). Bu test İKİ çekirdeğin GERÇEK İŞ paylaşmasını kanıtlar:
 * paylaşılan 200-elemanlı diziyi (0..199) İKİYE bölüp her çekirdek kendi
 * yarısını toplar, sonuçları birleştirir. Beklenen toplam = 19900.
 *
 * İşbölümü:
 *   - Çekirdek 0 (boot, MMU-ON): dizinin ilk yarısını [0,100) toplar.
 *   - Çekirdek 1 (PSCI CPU_ON, MMU-OFF): ikinci yarısını [100,200) toplar.
 *
 * İKİ paralel-güvenlik mekanizması birlikte kanıtlanır:
 *   (A) AYRI SLOT: her çekirdek kendi kısmi toplamını AYRI 64-byte-hizalı
 *       slota yazar → veri yarışı YOK. Çekirdek 0 birleştirmeden önce
 *       çekirdek 1'in slotunu cache-coherent (dc ivac) okur.
 *   (B) SPINLOCK: her çekirdek AYRICA aarch64 LDAXR/STXR atomik test-and-set
 *       spinlock'u ile korunan ORTAK akümülatöre kendi kısmi toplamını ekler.
 *       İki çekirdek aynı akümülatöre yarışsız erişir → yarış-koşulu güvenliği.
 *       Ortak akümülatör de 19900 olmalı → spinlock doğru serialize etti.
 *
 * DOĞRULAMA: hem AYRI-SLOT birleşimi hem SPINLOCK akümülatörü 19900 ise
 * "SMP COMPUTE OK". Yalnız iki çekirdek de kendi payını doğru hesaplarsa
 * bu değer çıkar → gerçek paralel hesaplama + yarış güvenliği kanıtı.
 *
 * KRİTİK — cache coherency (D-169 dersi): Çekirdek 1 MMU-OFF (non-cacheable,
 * doğrudan RAM); çekirdek 0 MMU-ON (Normal-WB cacheable). Bu cacheability
 * uyuşmazlığı donanım coherency'sini bozar → paylaşılan her satır için EL ile
 * senkronizasyon gerekir:
 *   - Çekirdek 0 paylaşılan bir satıra YAZDIKTAN sonra `dc civac` (clean+
 *     invalidate to PoC) → yazımı RAM'e boşaltır ki çekirdek 1 (RAM'den okuyan)
 *     görsün, ve satırı invalidate eder ki sonraki okumada tazesini alsın.
 *   - Çekirdek 0 paylaşılan bir satırı OKUMADAN önce `dc ivac` → çekirdek 1'in
 *     RAM'e yazdığı taze değeri yükler.
 *   - Çekirdek 1 tarafında her yazımdan sonra `dsb sy` → RAM'e boşalt.
 * Spinlock'un LDAXR/STLR (acquire/release) bariyerleri sıralamayı garanti eder
 * ama cacheability uyuşmazlığında görünürlük için `dc civac` de şart.
 *
 * DETERMİNİSTİK: tüm bekleme döngüleri sınırlı (bounded) tik sayısıyla; yük/
 * internet bağımsız. Çekirdek 1'in bitmesi < ~2M tik beklenir; timeout üst
 * sınır 40M tik (yük-duyarlı uzun busy-wait YOK).
 */
#include <stdint.h>

extern void kdl_yazdir_metin(const char *);
extern void kdl_yaz_metin(const char *);
extern void kdl_yazdir_onaltilik(uint64_t);
extern void kdl_yaz_onaltilik(uint64_t);
extern void kdl_yazdir_isaretsiz_tam64(uint64_t);  /* satır-sonlu ondalık */

#define N_ELEMAN     200
#define YARI         (N_ELEMAN / 2)     /* 100 */
#define BEKLENEN     19900ULL           /* 0+1+...+199 = 199*200/2 */
#define TIMEOUT_TIK  40000000ULL        /* bounded üst-sınır (yük-bağımsız) */

/* Paylaşılan iş dizisi: dizi[i] = i. İki çekirdek de bunu okur (yalnız-okuma
 * → yarış yok). 64-byte hizalı ki çekirdek 1'in ilk okuması temiz satırdan. */
static volatile uint64_t is_dizisi[N_ELEMAN] __attribute__((aligned(64)));

/* (A) AYRI SLOT — her çekirdek kendi kısmi toplamını buraya yazar (yarış yok).
 * Her biri kendi 64-byte cache satırında (yanlış-paylaşım/klobber önlemi). */
static volatile uint64_t kismi_cekirdek0 __attribute__((aligned(64))) = 0;
static volatile uint64_t kismi_cekirdek1 __attribute__((aligned(64))) = 0;

/* Çekirdek 1'in "işim bitti" bayrağı (ayrı satır). */
static volatile uint64_t cekirdek1_bitti __attribute__((aligned(64))) = 0;

/* (B) SPINLOCK ile korunan ORTAK akümülatör + kilit sözcüğü (ayrı satırlar). */
static volatile uint64_t ortak_akumulator __attribute__((aligned(64))) = 0;
static volatile uint32_t kilit __attribute__((aligned(64))) = 0;

/* Çekirdek 1'in kendi yığını (çekirdek 0'ınkinden ayrı) — 8 KB. */
static uint8_t cekirdek1_yigin[8192] __attribute__((aligned(16)));

/* PSCI conduit çağrıları (smp_arm.c ile aynı desen). */
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

/* --- Cache coherency yardımcıları (MMU-off/on uyuşmazlığı için) --- */

/* Paylaşılan bir adresin satırını PoC'ye clean+invalidate: kendi yazımını
 * RAM'e boşalt + satırı geçersiz kıl (sonraki okuma tazesini alır). */
static inline void satir_temizle_gecersiz(const volatile void *p) {
    __asm__ volatile(
        "dc civac, %0\n"
        "dsb sy\n"
        :
        : "r"(p)
        : "memory");
}

/* Paylaşılan bir adresin satırını PoC'den geçersiz kıl (okumadan önce) →
 * çekirdek 1'in RAM'e yazdığı taze değeri yükle. */
static inline void satir_gecersiz(const volatile void *p) {
    __asm__ volatile(
        "dc ivac, %0\n"
        "dsb sy\n"
        :
        : "r"(p)
        : "memory");
}

/* --- Spinlock: aarch64 LDAXR/STXR atomik test-and-set + STLR release --- */

/* Kilidi al: kilit==0 iken atomik olarak 1 yap. Başarısızsa (başkası tutuyor
 * veya STXR fail) tekrar dene. LDAXR acquire semantiği + STLR release semantiği
 * kritik bölgeyi çevreler → doğal bellek bariyeri.
 *
 * NOT: MMU-off/on cacheability uyuşmazlığında exclusive monitor davranışı
 * karışabilir; bu yüzden döngüde her denemeden önce kilit satırını geçersiz
 * kıl (dc ivac) → diğer çekirdeğin serbest bırakışını gör. */
static inline void kilit_al(void) {
    uint32_t alindi;
    uint32_t onceki;
    do {
        /* Diğer çekirdeğin STLR'ını (RAM'e) görebilmek için satırı tazele. */
        satir_gecersiz(&kilit);
        __asm__ volatile(
            "ldaxr  %w0, [%2]\n"       /* onceki = kilit (acquire, exclusive) */
            "cbnz   %w0, 1f\n"         /* kilit meşgulse → STXR deneme, fail dön */
            "stxr   %w1, %w3, [%2]\n"  /* kilit = 1 dene; alindi=0 başarı, 1 fail */
            "b      2f\n"
            "1:\n"
            "mov    %w1, #1\n"         /* meşgul → alindi=1 (başarısız) */
            "clrex\n"                  /* exclusive monitor'ı temizle */
            "2:\n"
            : "=&r"(onceki), "=&r"(alindi)
            : "r"(&kilit), "r"((uint32_t)1)
            : "memory");
        /* alindi==0 VE onceki==0 → kilidi biz aldık. Aksi halde tekrar. */
        if (alindi == 0 && onceki == 0) {
            /* Kilit yazımını RAM'e boşalt ki diğer çekirdek meşgul görsün. */
            satir_temizle_gecersiz(&kilit);
            break;
        }
        /* Kısa backoff (bounded). */
        for (volatile int b = 0; b < 64; b++) { __asm__ volatile("yield"); }
    } while (1);
}

/* Kilidi bırak: STLR ile 0 yaz (release) + RAM'e boşalt (dc civac). */
static inline void kilit_birak(void) {
    __asm__ volatile(
        "stlr  wzr, [%0]\n"           /* kilit = 0, release semantiği */
        :
        : "r"(&kilit)
        : "memory");
    satir_temizle_gecersiz(&kilit);
}

/* Ortak akümülatöre spinlock korumasında ekle (her iki çekirdek çağırır). */
static inline void akumulatore_ekle(uint64_t deger) {
    kilit_al();
    /* Kritik bölge: diğer çekirdeğin son yazımını gör (dc ivac), ekle, boşalt. */
    satir_gecersiz(&ortak_akumulator);
    ortak_akumulator += deger;
    satir_temizle_gecersiz(&ortak_akumulator);
    kilit_birak();
}

/* Yalnız-okuma dizisinden [bas, son) aralığını topla (her iki çekirdek). */
static uint64_t araligi_topla(uint32_t bas, uint32_t son) {
    uint64_t toplam = 0;
    for (uint32_t i = bas; i < son; i++) {
        /* Dizi yalnız-okuma; çekirdek 0 doldurdu. Çekirdek 1 (MMU-off) RAM'den
         * okur — çekirdek 0 doldurduktan sonra dc civac ile boşalttı (bkz main).
         * Yine de her elemanı okumadan önce satırı geçersiz kılmak (dc ivac)
         * güvenli tarafta kalır; ama satır 8 eleman taşır, her okumada değil
         * blok-blok geçersiz kılmak yeterli. Basitlik için eleman-bazlı ivac. */
        satir_gecersiz(&is_dizisi[i]);
        toplam += is_dizisi[i];
    }
    return toplam;
}

/*
 * Çekirdek 1 giriş noktası — PSCI CPU_ON tarafından MMU-OFF, EL1'de çağrılır.
 * ÖNCE kendi SP'sini kur, sonra işini yap. UART'a DOKUNMA (çekirdek 0 ile
 * paylaşımlı) — yalnız paylaşılan RAM'e yaz.
 */
__attribute__((noreturn))
static void cekirdek1_giris(void) {
    /* 1. KENDİ yığınını kur (yığın dizisinin TEPESİ, aşağı büyür). */
    __asm__ volatile(
        "mov sp, %0\n"
        :
        : "r"(&cekirdek1_yigin[sizeof(cekirdek1_yigin)])
        : "memory");

    /* 2. GERÇEK İŞ: ikinci yarıyı [100,200) topla. */
    uint64_t benim_toplam = araligi_topla(YARI, N_ELEMAN);

    /* 3a. AYRI SLOT: kendi kısmi toplamını yaz + RAM'e boşalt. */
    kismi_cekirdek1 = benim_toplam;
    satir_temizle_gecersiz(&kismi_cekirdek1);

    /* 3b. SPINLOCK: ortak akümülatöre yarışsız ekle. */
    akumulatore_ekle(benim_toplam);

    /* 4. "Bittim" bayrağını en son set et (çekirdek 0 bunu poll eder). Slot ve
     *    akümülatör yazımları RAM'e ULAŞTIKTAN sonra → çekirdek 0 bayrağı
     *    görünce ötekiler de görünür. */
    __asm__ volatile("dsb sy" ::: "memory");
    cekirdek1_bitti = 1;
    satir_temizle_gecersiz(&cekirdek1_bitti);

    /* 5. Sonsuz düşük-güç spin. */
    __asm__ volatile("sev" ::: "memory");
    for (;;) {
        __asm__ volatile("wfe" ::: "memory");
    }
}

/* PSCI dönüş/fonksiyon kodları. */
#define PSCI_SUCCESS       0
#define PSCI_NOT_SUPPORTED (-1)
#define PSCI_FN_CPU_ON  0xC4000003ULL
#define PSCI_FN_VERSION 0x84000000ULL

int main(void) {
    kdl_yazdir_metin("SMP COMPUTE BASLA");

    /* --- 0. İş dizisini doldur: dizi[i] = i. Sonra tüm satırları RAM'e boşalt
     *        ki MMU-off çekirdek 1 taze okusun. --- */
    for (uint32_t i = 0; i < N_ELEMAN; i++) {
        is_dizisi[i] = i;
    }
    for (uint32_t i = 0; i < N_ELEMAN; i++) {
        satir_temizle_gecersiz(&is_dizisi[i]);
    }

    /* --- 1. Çekirdek 1'i başlat (PSCI CPU_ON, HVC→SMC fallback). --- */
    uint64_t giris = (uint64_t)(uintptr_t)&cekirdek1_giris;
    const char *conduit = "HVC";
    int64_t ret = psci_hvc(PSCI_FN_CPU_ON, 0x1, giris, 0xC0DE);
    if (ret == PSCI_NOT_SUPPORTED) {
        conduit = "SMC";
        ret = psci_smc(PSCI_FN_CPU_ON, 0x1, giris, 0xC0DE);
    }

    kdl_yaz_metin("CPU_ON conduit=");
    kdl_yaz_metin(conduit);
    kdl_yaz_metin(" ret=");
    kdl_yazdir_onaltilik((uint64_t)ret);

    if (ret != PSCI_SUCCESS) {
        kdl_yazdir_metin("SMP COMPUTE FAIL: CPU_ON basarisiz — 2. cekirdek baslamadi");
        for (;;) { __asm__ volatile("wfe"); }
    }

    /* --- 2. Çekirdek 0 GERÇEK İŞ: ilk yarıyı [0,100) topla. --- */
    uint64_t benim_toplam = araligi_topla(0, YARI);

    /* 2a. AYRI SLOT: kendi kısmi toplamını yaz. */
    kismi_cekirdek0 = benim_toplam;
    satir_temizle_gecersiz(&kismi_cekirdek0);

    /* 2b. SPINLOCK: ortak akümülatöre yarışsız ekle (çekirdek 1 ile eşzamanlı
     *     olabilir → kilit serialize eder). */
    akumulatore_ekle(benim_toplam);

    kdl_yaz_metin("cekirdek0 kismi=");
    kdl_yazdir_isaretsiz_tam64(benim_toplam);

    /* --- 3. Çekirdek 1'in bitmesini bekle (bounded poll + cache-coherent). --- */
    int bitti = 0;
    for (uint64_t bekle = 0; bekle < TIMEOUT_TIK; bekle++) {
        satir_gecersiz(&cekirdek1_bitti);
        if (cekirdek1_bitti != 0) { bitti = 1; break; }
    }

    if (!bitti) {
        kdl_yazdir_metin("SMP COMPUTE FAIL: cekirdek1 zamaninda bitmedi (timeout)");
        for (;;) { __asm__ volatile("wfe"); }
    }

    /* --- 4. Çekirdek 1'in kısmi toplamını cache-coherent oku. --- */
    satir_gecersiz(&kismi_cekirdek1);
    uint64_t c1_kismi = kismi_cekirdek1;

    kdl_yaz_metin("cekirdek1 kismi=");
    kdl_yazdir_isaretsiz_tam64(c1_kismi);

    /* --- 5a. AYRI-SLOT birleşimi. --- */
    uint64_t slot_toplam = benim_toplam + c1_kismi;

    /* --- 5b. SPINLOCK ortak akümülatörünü cache-coherent oku. --- */
    satir_gecersiz(&ortak_akumulator);
    uint64_t akum_toplam = ortak_akumulator;

    kdl_yaz_metin("ayri-slot toplam=");
    kdl_yazdir_isaretsiz_tam64(slot_toplam);
    kdl_yaz_metin("spinlock akumulator=");
    kdl_yazdir_isaretsiz_tam64(akum_toplam);

    /* --- 6. DOĞRULAMA: her iki yol da 19900 olmalı. --- */
    if (slot_toplam == BEKLENEN && akum_toplam == BEKLENEN) {
        kdl_yaz_metin("toplam=");
        kdl_yazdir_isaretsiz_tam64(slot_toplam);   /* satır-sonlu ondalık */
        kdl_yazdir_metin("SMP COMPUTE OK");
    } else if (slot_toplam == BEKLENEN) {
        /* Ayrı-slot doğru ama spinlock akümülatörü değil → yarış/coherency. */
        kdl_yaz_metin("SMP COMPUTE KISMI: ayri-slot=");
        kdl_yazdir_isaretsiz_tam64(slot_toplam);
        kdl_yaz_metin("DOGRU ama spinlock-akumulator=");
        kdl_yazdir_isaretsiz_tam64(akum_toplam);
        kdl_yazdir_metin("YANLIS (beklenen 19900)");
    } else {
        kdl_yaz_metin("SMP COMPUTE FAIL: ayri-slot=");
        kdl_yazdir_isaretsiz_tam64(slot_toplam);
        kdl_yaz_metin("akumulator=");
        kdl_yazdir_isaretsiz_tam64(akum_toplam);
        kdl_yazdir_metin("(beklenen 19900)");
    }

    for (;;) { __asm__ volatile("wfe"); }
}
