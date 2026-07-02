/*
 * KEMGU-OS bare-metal SMP BARİYER SENKRONİZASYONU testi (aarch64).
 * ================================================================
 *
 * Milestone (D-170/174 üstünde): D-170 diziyi statik böldü, D-174 dinamik iş
 * kuyruğu (work-stealing) kanıtladı. Bu test İKİ çekirdeğin LOCKSTEP (kilitli-
 * adım) senkronizasyonunu kanıtlar: iki çekirdek K=5 tur boyunca her turda bir
 * BARİYER'de buluşur — ikisi de o tura varmadan HİÇBİRİ bir sonraki tura
 * geçemez. Bu, klasik toplu-senkronizasyon ilkelidir (BSP/OpenMP-tarzı).
 *
 * BARİYER — sense-reversing (nesil/generation ters-çevirme):
 *   Paylaşımlı `varan_sayaci` + `nesil` (generation) sözcüğü. Her çekirdek
 *   bariyere varınca:
 *     1. SPINLOCK al → varan_sayaci++ → o turun "varış sırası"nı öğren.
 *     2. Eğer SON gelen isem (sayaç == çekirdek sayısı): sayacı sıfırla +
 *        nesli artır (nesil++) → tüm bekleyenleri serbest bırak. unlock.
 *     3. Aksi halde (erken geldim): unlock, sonra nesil DEĞİŞENE kadar bekle
 *        (bounded poll + dc ivac). Nesil değişince bariyerden geç.
 *   Sense-reversing: sayaç yerine NESLİ izlemek ABA problemini önler — ardışık
 *   turlar sayacı 0'a döndürse de nesil monotonik artar, erken/geç gelenler
 *   asla karışmaz.
 *
 * K=5 TUR (lockstep kanıtı): her turda her çekirdek KENDİ paylaşımlı "tur
 * sayacı"nı (per-çekirdek) artırır, SONRA bariyerde buluşur. Bariyer lockstep'i
 * zorladığı için her çekirdek tam K kez tur-sayacını artırır. K tur sonunda:
 *     cekirdek0_tur == K  VE  cekirdek1_tur == K   → lockstep kanıtlandı.
 * Bariyer olmasaydı hızlı çekirdek yavaşı geçer, sayaçlar tur-tur eşleşmezdi;
 * bariyer her turda ikisini de aynı çizgiye getirir.
 *
 * DOĞRULAMA (üç koşul birden):
 *   (1) cekirdek0_tur == K   (çekirdek 0 tam K tur koştu)
 *   (2) cekirdek1_tur == K   (çekirdek 1 tam K tur koştu)
 *   (3) son_nesil    == K    (bariyer tam K kez tetiklendi — her tur bir kez)
 *   Üçü de sağlanırsa "SMP BARRIER OK".
 *
 * KRİTİK — cache coherency (D-174 dersi): Çekirdek 1 MMU-OFF (non-cacheable,
 * doğrudan RAM); çekirdek 0 MMU-ON (Normal-WB cacheable). Bu cacheability
 * uyuşmazlığı donanım coherency'sini bozar → paylaşılan her satır (kilit,
 * varan_sayaci, nesil, per-çekirdek tur sayaçları) için EL ile senkronizasyon:
 *   - Paylaşılan satıra YAZDIKTAN sonra `dc civac` + `dsb sy` → RAM'e boşalt.
 *   - Paylaşılan satırı OKUMADAN önce `dc ivac` + `dsb sy` → tazesini yükle.
 * Spinlock LDAXR/STLR (acquire/release) bariyerleri kritik-bölge sıralamasını
 * garanti eder; cacheability uyuşmazlığında görünürlük için `dc` de şart.
 *
 * KRİTİK — naked trampoline (D-174 dersi): PSCI CPU_ON ikincil çekirdeği MMU-
 * OFF, EL1'de, UNDEFINED SP ile çağırır. C prologue callee-saved register'ları
 * undefined SP'ye spill eder → garbage adrese yazım → sessiz çöküş. ÇÖZÜM:
 * naked trampoline — İLK iş SP'yi kendi yığınımızın tepesine kur, SONRA C işine
 * dallan (naked → compiler prologue/epilogue üretmez → garanti temiz).
 *
 * DETERMİNİSTİK: K=5 tur bounded; her bariyer bekleme bounded (< TIMEOUT_TIK).
 * Yük/internet bağımsız — her koşuda tur sayaçları = 5, nesil = 5.
 */
#include <stdint.h>

extern void kdl_yazdir_metin(const char *);
extern void kdl_yaz_metin(const char *);
extern void kdl_yazdir_onaltilik(uint64_t);
extern void kdl_yaz_onaltilik(uint64_t);
extern void kdl_yazdir_isaretsiz_tam64(uint64_t);  /* satır-sonlu ondalık */

#define K_TUR        5ULL              /* lockstep tur sayısı */
#define CEKIRDEK_SAYISI 2ULL           /* bariyere katılan çekirdek sayısı */
#define TIMEOUT_TIK  40000000ULL       /* bounded üst-sınır (yük-bağımsız) */

/* --- Paylaşımlı bariyer + lockstep durumu (her biri kendi 64-byte cache
 *     satırında, yanlış-paylaşım/klobber önlemi) --- */

/* Bu nesilde bariyere kaç çekirdek vardı. SON gelen (== CEKIRDEK_SAYISI) sayacı
 * sıfırlar + nesli artırır. SPINLOCK korumalı. */
static volatile uint64_t varan_sayaci __attribute__((aligned(64))) = 0;

/* Bariyer NESLİ (generation) — sense-reversing. Her bariyer tamamlanışında +1.
 * Erken gelenler bu değer değişene kadar bekler. SPINLOCK içinde yazılır,
 * dışarıda cache-coherent okunur. */
static volatile uint64_t nesil __attribute__((aligned(64))) = 0;

/* Her çekirdeğin tamamladığı tur sayısı (lockstep kanıtı — ayrı satırlar).
 * Çekirdek KENDİ sayacını yazar; SPINLOCK gerekmez ama görünürlük için dc. */
static volatile uint64_t cekirdek0_tur __attribute__((aligned(64))) = 0;
static volatile uint64_t cekirdek1_tur __attribute__((aligned(64))) = 0;

/* Çekirdek 1'in "işim bitti" bayrağı (ayrı satır). */
static volatile uint64_t cekirdek1_bitti __attribute__((aligned(64))) = 0;

/* SPINLOCK kilit sözcüğü (ayrı satır). */
static volatile uint32_t kilit __attribute__((aligned(64))) = 0;

/* Çekirdek 1'in kendi yığını (çekirdek 0'ınkinden ayrı) — 8 KB.
 * NOT: naked trampoline yalnız asm'den (sembol adıyla) eriştiği için dış-bağ
 * (non-static) → linker sembolü emit eder (aksi halde "unused" atılır). */
uint8_t cekirdek1_yigin[8192] __attribute__((aligned(16)));

/* PSCI conduit çağrıları (smp_queue_arm.c ile aynı desen). */
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
 * diğer çekirdeğin RAM'e yazdığı taze değeri yükle. */
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
 * veya STXR fail) tekrar dene. LDAXR acquire + STLR release semantiği kritik
 * bölgeyi çevreler → doğal bellek bariyeri.
 *
 * NOT: MMU-off/on cacheability uyuşmazlığında exclusive monitor davranışı
 * karışabilir; her denemeden önce kilit satırını geçersiz kıl (dc ivac) →
 * diğer çekirdeğin serbest bırakışını gör. */
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

/* --- Sense-reversing bariyer: iki çekirdek bir noktada buluşur ---
 *
 * Her çekirdek çağırır. SPINLOCK korumasında varan_sayaci'nı artır; SON gelen
 * (sayaç == CEKIRDEK_SAYISI) sayacı sıfırlar + nesli artırır (tüm bekleyenleri
 * serbest bırakır). Erken gelenler nesil DEĞİŞENE kadar bekler (bounded poll +
 * dc ivac). Böylece iki çekirdek de bariyerden AYNI anda çıkar (lockstep).
 *
 * Dönüş: 1 = normal (nesil beklemesi başarılı veya son-gelen idi),
 *        0 = timeout (nesil zamanında değişmedi → coherency/deadlock şüphesi). */
static int bariyer_bekle(void) {
    kilit_al();
    /* Kritik bölge: bu nesildeki varan sayacı ve mevcut nesli oku. */
    satir_gecersiz(&varan_sayaci);
    satir_gecersiz(&nesil);
    uint64_t bu_nesil = nesil;
    uint64_t sayac = varan_sayaci + 1;
    varan_sayaci = sayac;
    satir_temizle_gecersiz(&varan_sayaci);

    if (sayac == CEKIRDEK_SAYISI) {
        /* SON gelen: sayacı sıfırla + nesli artır → herkesi serbest bırak. */
        varan_sayaci = 0;
        satir_temizle_gecersiz(&varan_sayaci);
        nesil = bu_nesil + 1;
        satir_temizle_gecersiz(&nesil);
        kilit_birak();
        return 1;   /* Son-gelen bariyerden hemen geçer. */
    }

    /* Erken gelen: kilidi bırak, sonra nesil DEĞİŞENE kadar bekle (bounded). */
    kilit_birak();
    for (uint64_t bekle = 0; bekle < TIMEOUT_TIK; bekle++) {
        satir_gecersiz(&nesil);
        if (nesil != bu_nesil) {
            return 1;   /* Nesil arttı → bariyer tamamlandı, geç. */
        }
        /* Kısa backoff (bounded). */
        for (volatile int b = 0; b < 32; b++) { __asm__ volatile("yield"); }
    }
    return 0;   /* Timeout — nesil değişmedi (coherency/deadlock şüphesi). */
}

/*
 * K tur lockstep döngüsü (HER İKİ çekirdek aynısını koşar).
 *
 * Her turda: bu çekirdeğin KENDİ tur sayacını artır (cache-coherent yaz),
 * sonra bariyerde diğer çekirdekle buluş. Bariyer lockstep'i zorlar → iki
 * çekirdek de her turda aynı çizgide ilerler. Bariyer timeout olursa 0 döner
 * → çağıran "kısmi/fail" işaretler (sessiz-gizleme yok).
 *
 * Dönüş: 1 = K tur temiz tamamlandı, 0 = bir bariyerde timeout oldu. */
static int lockstep_kos(volatile uint64_t *tur_sayaci) {
    for (uint64_t tur = 0; tur < K_TUR; tur++) {
        /* 1. Bu çekirdeğin tur sayacını artır (kendi satırı, cache-coherent). */
        satir_gecersiz(tur_sayaci);
        *tur_sayaci = *tur_sayaci + 1;
        satir_temizle_gecersiz(tur_sayaci);

        /* 2. Bariyerde diğer çekirdekle buluş (lockstep). */
        if (!bariyer_bekle()) {
            return 0;   /* Bariyer timeout → lockstep bozuldu. */
        }
    }
    return 1;
}

/*
 * Çekirdek 1 asıl işi (C) — SP zaten kurulu çağrılır (trampoline'den).
 * UART'a DOKUNMA (çekirdek 0 ile paylaşımlı) — yalnız paylaşılan RAM'e yaz.
 */
__attribute__((noreturn))
void cekirdek1_isi(void) {
    /* 1. K tur lockstep koş (her turda bariyerde çekirdek 0 ile buluş). */
    (void)lockstep_kos(&cekirdek1_tur);

    /* 2. "Bittim" bayrağını en son set et (çekirdek 0 bunu poll eder). Tur
     *    sayacı yazımları RAM'e ULAŞTIKTAN sonra → çekirdek 0 bayrağı görünce
     *    ötekiler de görünür. */
    __asm__ volatile("dsb sy" ::: "memory");
    cekirdek1_bitti = 1;
    satir_temizle_gecersiz(&cekirdek1_bitti);

    /* 3. Sonsuz düşük-güç spin. */
    __asm__ volatile("sev" ::: "memory");
    for (;;) {
        __asm__ volatile("wfe" ::: "memory");
    }
}

/*
 * Çekirdek 1 GİRİŞ noktası — PSCI CPU_ON tarafından MMU-OFF, EL1'de, UNDEFINED
 * SP ile çağrılır. KRİTİK: SP kurulmadan ÖNCE HİÇBİR C prologue çalışmamalı
 * (C prologue callee-saved register'ları [x29/x30/x19...] undefined SP'ye
 * `stp ... [sp,#-N]!` ile spill eder → garbage adrese yazım → sessiz çöküş).
 *
 * ÇÖZÜM: naked trampoline — İLK iş SP'yi kendi yığınımızın tepesine kur, SONRA
 * C işine dallan. Naked'da compiler prologue/epilogue üretmez → garanti temiz. */
__attribute__((naked, noreturn))
static void cekirdek1_giris(void) {
    /* Naked → compiler prologue/epilogue üretmez; SP'yi elle kur. Sembollere
     * asm içinde adrp/add ile eriş (cekirdek1_yigin + cekirdek1_isi dış-bağlı →
     * linker emit eder). Yığın dizisinin TEPESİ = &dizi + boyut (aşağı büyür). */
    __asm__ volatile(
        "adrp x0, cekirdek1_yigin\n"
        "add  x0, x0, :lo12:cekirdek1_yigin\n"
        "mov  x1, %0\n"
        "add  x0, x0, x1\n"           /* x0 = yığın TEPESİ */
        "mov  sp, x0\n"
        "b    cekirdek1_isi\n"        /* SP kurulu → C işine dallan (noreturn) */
        :
        : "i"((uint64_t)sizeof(cekirdek1_yigin))
        : "x0", "x1", "memory");
}

/* PSCI dönüş/fonksiyon kodları. */
#define PSCI_SUCCESS       0
#define PSCI_NOT_SUPPORTED (-1)
#define PSCI_FN_CPU_ON  0xC4000003ULL
#define PSCI_FN_VERSION 0x84000000ULL

int main(void) {
    kdl_yazdir_metin("SMP BARRIER BASLA");

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
        kdl_yazdir_metin("SMP BARRIER FAIL: CPU_ON basarisiz — 2. cekirdek baslamadi");
        for (;;) { __asm__ volatile("wfe"); }
    }

    /* --- 2. Çekirdek 0 de K tur lockstep koşar (her turda bariyerde çekirdek 1
     *        ile buluşur). İki çekirdek bariyerde kilitli-adım ilerler. --- */
    int c0_temiz = lockstep_kos(&cekirdek0_tur);

    /* --- 3. Çekirdek 1'in bitmesini bekle (bounded poll + cache-coherent). --- */
    int bitti = 0;
    for (uint64_t bekle = 0; bekle < TIMEOUT_TIK; bekle++) {
        satir_gecersiz(&cekirdek1_bitti);
        if (cekirdek1_bitti != 0) { bitti = 1; break; }
    }

    if (!bitti) {
        satir_gecersiz(&cekirdek0_tur);
        satir_gecersiz(&cekirdek1_tur);
        satir_gecersiz(&nesil);
        kdl_yaz_metin("SMP BARRIER FAIL: cekirdek1 timeout — c0_tur=");
        kdl_yazdir_isaretsiz_tam64(cekirdek0_tur);
        kdl_yaz_metin("c1_tur=");
        kdl_yazdir_isaretsiz_tam64(cekirdek1_tur);
        kdl_yaz_metin("nesil=");
        kdl_yazdir_isaretsiz_tam64(nesil);
        kdl_yazdir_metin("");
        for (;;) { __asm__ volatile("wfe"); }
    }

    /* --- 4. Sonuçları cache-coherent oku. --- */
    satir_gecersiz(&cekirdek0_tur);
    uint64_t c0 = cekirdek0_tur;
    satir_gecersiz(&cekirdek1_tur);
    uint64_t c1 = cekirdek1_tur;
    satir_gecersiz(&nesil);
    uint64_t son_nesil = nesil;

    kdl_yaz_metin("cekirdek0 tur=");
    kdl_yazdir_isaretsiz_tam64(c0);
    kdl_yaz_metin("cekirdek1 tur=");
    kdl_yazdir_isaretsiz_tam64(c1);
    kdl_yaz_metin("nesil=");
    kdl_yazdir_isaretsiz_tam64(son_nesil);

    /* --- 5. DOĞRULAMA: her iki çekirdek tam K tur koştu VE bariyer tam K kez
     *        tetiklendi (nesil == K). Ayrıca çekirdek 0 lockstep'i timeout'suz
     *        tamamlamış olmalı. --- */
    if (c0 == K_TUR && c1 == K_TUR && son_nesil == K_TUR && c0_temiz) {
        kdl_yazdir_metin("SMP BARRIER OK");
    } else if (c0 == K_TUR && c1 == K_TUR && son_nesil == K_TUR) {
        /* Tur sayaçları doğru ama çekirdek 0 lockstep timeout işaretledi
         * (nadir yarış) — kısmi. */
        kdl_yaz_metin("SMP BARRIER KISMI: turlar=");
        kdl_yazdir_isaretsiz_tam64(K_TUR);
        kdl_yaz_metin("DOGRU ama cekirdek0 bariyer timeout isaretledi (c0_temiz=0)");
        kdl_yazdir_metin("");
    } else {
        /* Tur sayaçları veya nesil beklenenden farklı → lockstep/coherency
         * bozuldu (bir çekirdek diğerini geçti veya bariyer kaçtı). */
        kdl_yaz_metin("SMP BARRIER FAIL: c0_tur=");
        kdl_yazdir_isaretsiz_tam64(c0);
        kdl_yaz_metin("c1_tur=");
        kdl_yazdir_isaretsiz_tam64(c1);
        kdl_yaz_metin("nesil=");
        kdl_yazdir_isaretsiz_tam64(son_nesil);
        kdl_yaz_metin("(beklenen tur=5 nesil=5)");
        kdl_yazdir_metin("");
    }

    for (;;) { __asm__ volatile("wfe"); }
}
