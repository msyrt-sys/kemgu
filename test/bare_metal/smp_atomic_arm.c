/*
 * KEMGU-OS bare-metal SMP ATOMİK SAYAÇ ÇEKİŞMESİ testi (aarch64).
 * ==============================================================
 *
 * Milestone (D-170/174 üstünde): D-174 iki çekirdeğin dinamik iş-kuyruğundan
 * yarışarak öğe çekmesini (work-stealing, spinlock ile) kanıtladı. Bu test bir
 * kademe DAHA ALT seviyeyi kanıtlar: SPINLOCK YOK, doğrudan LDXR/STXR atomik
 * read-modify-write ile TEK bir paylaşımlı sayaç üzerinde İKİ çekirdek çekişir.
 *
 * İş tanımı: paylaşımlı `sayac` = 0. Her çekirdek AYNI sayacı N=10000 kez
 * ATOMİK artırır. Atomik artırım aarch64 exclusive-monitor retry döngüsü:
 *     retry:  ldxr  x, [adr]        ; sayacı exclusive yükle
 *             add   x, x, 1
 *             stxr  w, x, [adr]      ; koşullu yaz; w=0 başarı, w=1 fail
 *             cbnz  w, retry          ; STXR fail (araya başka çekirdek girdi) → yeniden
 * İki çekirdek toplam 2*N = 20000 artırım yapar. Exclusive monitor, iki çekirdek
 * AYNI satıra yazmaya çalışınca birinin STXR'ını fail ettirir → o çekirdek taze
 * değerle retry eder → HİÇBİR artırım kaybolmaz.
 *
 * DOĞRULAMA (deterministik): son sayac == 2*N == 20000 olmalı. ATOMİK OLMASAYDI
 * (düz load-add-store, exclusive-monitor yok) iki çekirdek aynı eski değeri
 * okuyup üstüne yazardı → lost-update → sayac < 20000 çıkardı. 20000 çıkması
 * atomikliğin (yarış-koşulu güvenliğinin) kanıtıdır. Bu değer HER koşuda aynı
 * (deterministik) — atomik garanti sayesinde zamanlamadan bağımsız.
 *
 * KRİTİK — cache coherency (D-170/174 dersi): Çekirdek 1 MMU-OFF (non-cacheable,
 * doğrudan RAM); çekirdek 0 MMU-ON (Normal-WB cacheable). Bu cacheability
 * uyuşmazlığı donanım coherency'sini ve exclusive-monitor davranışını bozabilir.
 * Bu yüzden atomik artırımın RMW-retry döngüsünü, sayaç satırını her denemeden
 * ÖNCE tazeleyerek (dc ivac) ve yazdıktan SONRA RAM'e boşaltarak (dc civac)
 * çerçeveleriz; dsb sy bariyerleri sıralama/görünürlük sağlar. Ayrıca STXR fail
 * durumu (rakip çekirdek araya girdi) zaten retry ile ele alınır.
 *
 * DETERMİNİSTİK: her çekirdek sabit N=10000 artırım yapar (bounded); çekirdek
 * 1'in bitmesi bounded poll (< TIMEOUT_TIK). Yük/internet bağımsız — sayac her
 * koşuda 20000.
 */
#include <stdint.h>

extern void kdl_yazdir_metin(const char *);
extern void kdl_yaz_metin(const char *);
extern void kdl_yazdir_onaltilik(uint64_t);
extern void kdl_yaz_onaltilik(uint64_t);
extern void kdl_yazdir_isaretsiz_tam64(uint64_t);  /* satır-sonlu ondalık */

#define N_ARTIR      10000ULL          /* çekirdek başına artırım sayısı */
#define BEKLENEN     (2ULL * N_ARTIR)  /* = 20000 (iki çekirdek toplamı) */
#define TIMEOUT_TIK  40000000ULL       /* bounded üst-sınır (yük-bağımsız) */

/* --- Paylaşımlı durum (her biri kendi 64-byte cache satırında, yanlış-
 *     paylaşım/klobber önlemi) --- */

/* İki çekirdeğin ATOMİK olarak çekiştiği paylaşımlı sayaç. LDXR/STXR ile
 * korunur (spinlock YOK — atomikliğin kendisi kanıtlanıyor). */
static volatile uint64_t sayac __attribute__((aligned(64))) = 0;

/* Çekirdek 1'in "başladım / hazırım" bayrağı (rendezvous — ayrı satır).
 * Çekirdek 0 bu bayrağı görene kadar artırıma başlamaz → iki çekirdek AYNI anda
 * sayaca yarışmaya başlar (gerçek çekişme; tek çekirdek sırayla artırıp bitirmez
 * → exclusive-monitor çekişmesi gerçekten sınanır). */
static volatile uint64_t cekirdek1_basladi __attribute__((aligned(64))) = 0;

/* Çekirdek 1'in "işim bitti" bayrağı (ayrı satır). */
static volatile uint64_t cekirdek1_bitti __attribute__((aligned(64))) = 0;

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

/* --- ATOMİK ARTIRIM: aarch64 LDXR/STXR read-modify-write retry döngüsü ---
 *
 * Paylaşımlı sayacı atomik olarak 1 artırır. LDXR sayacı exclusive olarak
 * yükler (bu çekirdeğin exclusive-monitor'ını bu satıra kilitler); STXR yalnız
 * o satır bu okumadan beri BAŞKASINCA yazılmadıysa başarır (w=0). Araya rakip
 * çekirdek girip yazarsa STXR fail (w=1) → cbnz ile taze değerle retry.
 * HİÇBİR artırım kaybolmaz → lost-update imkansız.
 *
 * MMU-off/on cacheability uyuşmazlığında exclusive-monitor görünürlüğü için:
 * her RMW denemesinden ÖNCE satırı tazele (dc ivac → rakibin son yazımını gör),
 * başarılı yazımdan SONRA RAM'e boşalt (dc civac → rakip taze değeri görsün).
 * dmb ish bariyerleri RMW etrafında sıralamayı pekiştirir. */
static inline void atomik_artir(volatile uint64_t *adr) {
    uint64_t deger;
    uint32_t basarisiz;
    do {
        /* Rakip çekirdeğin (RAM'e) son yazımını görebilmek için satırı tazele. */
        satir_gecersiz(adr);
        __asm__ volatile(
            "dmb    ish\n"             /* önceki erişimler sıralı */
            "ldxr   %0, [%2]\n"        /* deger = *adr (exclusive) */
            "add    %0, %0, #1\n"      /* deger++ */
            "stxr   %w1, %0, [%2]\n"   /* *adr = deger dene; basarisiz=0 OK, 1 fail */
            "dmb    ish\n"             /* sonraki erişimler sıralı */
            : "=&r"(deger), "=&r"(basarisiz)
            : "r"(adr)
            : "memory");
        if (basarisiz == 0) {
            /* Başarılı yazımı RAM'e boşalt ki rakip çekirdek taze görsün. */
            satir_temizle_gecersiz(adr);
            break;
        }
        /* STXR fail (rakip araya girdi) → taze değerle yeniden dene. */
    } while (1);
}

/* --- Ortak çekişme döngüsü (HER İKİ çekirdek aynısını koşar) ---
 *
 * Paylaşımlı sayacı N_ARTIR kez atomik artırır. İki çekirdek eşzamanlı koşunca
 * exclusive-monitor çekişmesi gerçekleşir; STXR-fail retry hiçbir artırımı
 * düşürmez → toplam tam 2*N çıkar. */
static void sayaci_artir(void) {
    for (uint64_t k = 0; k < N_ARTIR; k++) {
        atomik_artir(&sayac);
    }
}

/*
 * Çekirdek 1 asıl işi (C) — SP zaten kurulu çağrılır (trampoline'den).
 * UART'a DOKUNMA (çekirdek 0 ile paylaşımlı) — yalnız paylaşılan RAM'e yaz.
 */
__attribute__((noreturn))
void cekirdek1_isi(void) {
    /* 1. RENDEZVOUS: "hazırım" bayrağını RAM'e set et. Çekirdek 0 bunu görene
     *    kadar artırıma girmez → iki çekirdek AYNI anda sayaca yarışır (gerçek
     *    exclusive-monitor çekişmesi; tek çekirdek sırayla bitirmez). */
    __asm__ volatile("dsb sy" ::: "memory");
    cekirdek1_basladi = 1;
    satir_temizle_gecersiz(&cekirdek1_basladi);

    /* 2. GERÇEK İŞ: paylaşımlı sayacı N_ARTIR kez atomik artır. */
    sayaci_artir();

    /* 3. "Bittim" bayrağını en son set et (çekirdek 0 bunu poll eder). Sayaç
     *    yazımları RAM'e ULAŞTIKTAN sonra → çekirdek 0 bayrağı görünce sayaç da
     *    görünür. */
    __asm__ volatile("dsb sy" ::: "memory");
    cekirdek1_bitti = 1;
    satir_temizle_gecersiz(&cekirdek1_bitti);

    /* 4. Sonsuz düşük-güç spin. */
    __asm__ volatile("sev" ::: "memory");
    for (;;) {
        __asm__ volatile("wfe" ::: "memory");
    }
}

/*
 * Çekirdek 1 GİRİŞ noktası — PSCI CPU_ON tarafından MMU-OFF, EL1'de, UNDEFINED
 * SP ile çağrılır. KRİTİK: SP kurulmadan ÖNCE HİÇBİR C prologue çalışmamalı
 * (C prologue callee-saved register'ları [x29/x30/x19...] undefined SP'ye
 * `stp ... [sp,#-N]!` ile spill eder → garbage adrese yazım → sessiz çöküş;
 * D-174 queue testindeki naked-trampoline dersi).
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
    kdl_yazdir_metin("SMP ATOMIC BASLA");

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
        kdl_yazdir_metin("SMP ATOMIC FAIL: CPU_ON basarisiz — 2. cekirdek baslamadi");
        for (;;) { __asm__ volatile("wfe"); }
    }

    /* --- 2. RENDEZVOUS: çekirdek 1'in "hazırım" bayrağını bekle (bounded).
     *        Böylece çekirdek 0 artırıma GİRMEDEN önce çekirdek 1 de yarışa hazır
     *        → iki çekirdek AYNI anda `sayac`'a yarışır (gerçek çekişme).
     *        Bounded: bayrak gelmezse yine de devam et (test yine geçerli olur
     *        ama çekişme zayıf; aşağıdaki DOĞRULAMA sonucu yakalar). --- */
    for (uint64_t bekle = 0; bekle < TIMEOUT_TIK; bekle++) {
        satir_gecersiz(&cekirdek1_basladi);
        if (cekirdek1_basladi != 0) { break; }
    }

    /* --- 3. Çekirdek 0 de AYNI sayacı atomik artırır. İki çekirdek eşzamanlı
     *        `sayac`'a yarışır → exclusive-monitor + STXR-fail retry her artırımı
     *        korur (lost-update yok). --- */
    sayaci_artir();

    /* --- 4. Çekirdek 1'in bitmesini bekle (bounded poll + cache-coherent). --- */
    int bitti = 0;
    for (uint64_t bekle = 0; bekle < TIMEOUT_TIK; bekle++) {
        satir_gecersiz(&cekirdek1_bitti);
        if (cekirdek1_bitti != 0) { bitti = 1; break; }
    }

    if (!bitti) {
        satir_gecersiz(&sayac);
        kdl_yaz_metin("SMP ATOMIC FAIL: cekirdek1 timeout — sayac=");
        kdl_yazdir_isaretsiz_tam64(sayac);
        for (;;) { __asm__ volatile("wfe"); }
    }

    /* --- 5. Son sayacı cache-coherent oku. --- */
    satir_gecersiz(&sayac);
    uint64_t son = sayac;

    kdl_yaz_metin("sayac=");
    kdl_yazdir_isaretsiz_tam64(son);

    /* --- 6. DOĞRULAMA: son sayac == 2*N == 20000 olmalı. Atomik değilse lost-
     *        update → sayac < 20000 → FAIL (gerçek sayacı bas). --- */
    if (son == BEKLENEN) {
        kdl_yazdir_metin("SMP ATOMIC OK");
    } else {
        kdl_yaz_metin("SMP ATOMIC FAIL sayac=");
        kdl_yazdir_isaretsiz_tam64(son);
        kdl_yaz_metin("(beklenen ");
        kdl_yazdir_isaretsiz_tam64(BEKLENEN);
        kdl_yazdir_metin(") — lost-update/coherency");
    }

    for (;;) { __asm__ volatile("wfe"); }
}
