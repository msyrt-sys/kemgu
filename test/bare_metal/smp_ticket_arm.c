/*
 * KEMGU-OS bare-metal SMP TICKET-LOCK (adil FIFO kilit) testi (aarch64).
 * =====================================================================
 *
 * Milestone (D-170/174/180 SMP üstünde): D-170 spinlock (test-and-set) ile
 * karşılıklı-dışlama (mutual exclusion) kanıtladı ama o kilit ADİL DEĞİLdi —
 * kilidi bırakınca herhangi bekleyen çekirdek onu KAPABİLİR; şanssız bir
 * çekirdek teoride sürekli açlık (starvation) çekebilir. Bu test bir kademe
 * DAHA GÜÇLÜ garantiyi kanıtlar: TICKET-LOCK ile FIFO ADALET.
 *
 * Ticket-lock çalışma prensibi (bir fırın/banka sıra-numarası gibi):
 *   - `sonraki_bilet`: verilecek bir sonraki bilet numarası (atomik sayaç).
 *   - `simdi_hizmet`:  şu an hizmet edilen (kritik bölgeye girmesine izin
 *                      verilen) bilet numarası.
 *   bilet_al():  atomik fetch-add ile `sonraki_bilet`'ten BENZERSİZ bir bilet
 *                kap (LDXR/STXR retry — iki çekirdek asla aynı bileti almaz).
 *   kilitle(b):  `simdi_hizmet == b` olana kadar bekle (poll). Biletler artan
 *                sırada verildiği için hizmet de ARTAN sırada olur → hangi
 *                çekirdek önce bilet aldıysa kritik bölgeye önce girer = FIFO.
 *   ac():        `simdi_hizmet`'i 1 artır → sıradaki bilet sahibi girer.
 *
 * FIFO ADALET (spinlock'un vermediği): biletler kesin artan sırada dağıtıldığı
 * ve hizmet de kesin artan sırada ilerlediği için, kilit her zaman EN ESKİ
 * bekleyene verilir. Açlık imkansız — her bilet sonunda hizmet görür.
 *
 * İş tanımı: paylaşımlı `sayac` = 0. Her çekirdek N=5000 kez: bilet al →
 * kilitle → `sayac++` (KRİTİK BÖLGE; kilit karşılıklı-dışlama sağladığı için
 * burada ATOMİK GEREKMEZ — düz load-add-store yeterli) → aç. İki çekirdek
 * toplam 2*N = 10000 artırım yapar.
 *
 * DOĞRULAMA (deterministik):
 *   (1) KİLİT-DOĞRULUK: son `sayac` == 2*N == 10000 olmalı. Kilit düzgün
 *       serialize etmeseydi iki çekirdek aynı eski değeri okuyup üstüne yazardı
 *       (lost-update) → sayac < 10000 → FAIL. 10000 çıkması, düz (atomik-olmayan)
 *       artırımın ticket-lock koruması altında yarışsız serialize edildiğinin
 *       kanıtıdır.
 *   (2) FIFO ADALET: her çekirdek işlediği kritik-bölge sayısını kendi ayrı
 *       slotunda tutar. Ticket-lock biletleri iki çekirdeğe sırayla dağıttığı
 *       için her çekirdek ~N kez işler; toplam == 2*N ve her biri > 0 (ikisi de
 *       gerçekten hizmet gördü → açlık yok). İkisi de tam N olmasa da (bilet
 *       alma çekişmesi zamanlamaya bağlı) toplamları tam 2*N olmalı ve her ikisi
 *       de belirgin bir pay (>0) almalı → FIFO adalet gözlemi.
 *
 * KRİTİK — cache coherency (D-170/174/180 dersi): Çekirdek 1 MMU-OFF (non-
 * cacheable, doğrudan RAM); çekirdek 0 MMU-ON (Normal-WB cacheable). Bu
 * cacheability uyuşmazlığı donanım coherency'sini ve exclusive-monitor
 * davranışını bozabilir. Bu yüzden:
 *   - `sonraki_bilet` atomik fetch-add'i, satırı her denemeden ÖNCE tazeleyerek
 *     (dc ivac) ve yazdıktan SONRA RAM'e boşaltarak (dc civac) çerçeveleriz.
 *   - `simdi_hizmet` poll'ünde her okumadan önce dc ivac (diğer çekirdeğin
 *     ac()'ını gör); ac()'ta yazımdan sonra dc civac (diğer çekirdek görsün).
 *   - `sayac` kritik bölgede kilit koruması altında; yine de MMU-off/on
 *     görünürlüğü için okumadan önce dc ivac, yazdıktan sonra dc civac.
 *   - Her paylaşımlı sözcük AYRI 64-byte cache satırında (yanlış-paylaşım/
 *     klobber önlemi).
 *
 * DETERMİNİSTİK: her çekirdek sabit N=5000 kritik bölge dener (bounded); tüm
 * bekleme döngüleri bounded üst-sınırlı (< TIMEOUT_TIK). Yük/internet bağımsız —
 * sayac her koşuda 10000.
 */
#include <stdint.h>

extern void kdl_yazdir_metin(const char *);
extern void kdl_yaz_metin(const char *);
extern void kdl_yazdir_onaltilik(uint64_t);
extern void kdl_yaz_onaltilik(uint64_t);
extern void kdl_yazdir_isaretsiz_tam64(uint64_t);  /* satır-sonlu ondalık */

#define N_TUR        5000ULL           /* çekirdek başına kritik-bölge sayısı */
#define BEKLENEN     (2ULL * N_TUR)    /* = 10000 (iki çekirdek toplamı) */
#define TIMEOUT_TIK  80000000ULL       /* bounded üst-sınır (yük-bağımsız) */

/* --- Paylaşımlı ticket-lock durumu (her biri kendi 64-byte cache satırında,
 *     yanlış-paylaşım/klobber önlemi) --- */

/* Verilecek bir sonraki bilet numarası. bilet_al() atomik fetch-add ile buradan
 * benzersiz bilet kapar (LDXR/STXR — iki çekirdek asla aynı bileti almaz). */
static volatile uint64_t sonraki_bilet __attribute__((aligned(64))) = 0;

/* Şu an hizmet edilen bilet numarası. kilitle(b) `simdi_hizmet == b` olana
 * kadar bekler; ac() bunu 1 artırır → sıradaki bilet sahibi girer. */
static volatile uint64_t simdi_hizmet __attribute__((aligned(64))) = 0;

/* Ticket-lock ile korunan paylaşımlı sayaç. Kritik bölgede düz (atomik-olmayan)
 * artırılır — kilit karşılıklı-dışlama sağladığı için yarış yok. */
static volatile uint64_t sayac __attribute__((aligned(64))) = 0;

/* (FIFO adalet izleme) Her çekirdeğin işlediği kritik-bölge sayısı (ayrı slot,
 * yalnız kendi çekirdeği yazar → yarış yok, ama coherency için boşalt). */
static volatile uint64_t islenen_cekirdek0 __attribute__((aligned(64))) = 0;
static volatile uint64_t islenen_cekirdek1 __attribute__((aligned(64))) = 0;

/* Çekirdek 1'in "başladım / hazırım" bayrağı (rendezvous — ayrı satır). */
static volatile uint64_t cekirdek1_basladi __attribute__((aligned(64))) = 0;

/* Çekirdek 1'in "işim bitti" bayrağı (ayrı satır). */
static volatile uint64_t cekirdek1_bitti __attribute__((aligned(64))) = 0;

/* Çekirdek 1'in kendi yığını (çekirdek 0'ınkinden ayrı) — 8 KB.
 * NOT: naked trampoline yalnız asm'den (sembol adıyla) eriştiği için dış-bağ
 * (non-static) → linker sembolü emit eder (aksi halde "unused" atılır). */
uint8_t cekirdek1_yigin[8192] __attribute__((aligned(16)));

/* PSCI conduit çağrıları (smp_atomic_arm.c ile aynı desen). */
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

/* --- bilet_al: aarch64 LDXR/STXR atomik fetch-add ile BENZERSİZ bilet kap ---
 *
 * `sonraki_bilet`'i atomik olarak 1 artırır ve ARTIRMADAN ÖNCEKİ değeri (benim
 * biletim) döndürür. LDXR sonraki_bilet'i exclusive yükler; STXR yalnız o satır
 * bu okumadan beri başkasınca yazılmadıysa başarır (w=0). Araya rakip çekirdek
 * girip yazarsa STXR fail (w=1) → cbnz ile taze değerle retry. İki çekirdek asla
 * aynı bileti almaz → biletler kesin benzersiz ve artan sırada.
 *
 * MMU-off/on cacheability uyuşmazlığında: her RMW denemesinden ÖNCE satırı
 * tazele (dc ivac → rakibin son yazımını gör), başarılı yazımdan SONRA RAM'e
 * boşalt (dc civac → rakip taze değeri görsün). dmb ish RMW etrafında sıralar. */
static inline uint64_t bilet_al(void) {
    uint64_t benim_bilet;
    uint64_t yeni;
    uint32_t basarisiz;
    do {
        /* Rakip çekirdeğin (RAM'e) son yazımını görebilmek için satırı tazele. */
        satir_gecersiz(&sonraki_bilet);
        __asm__ volatile(
            "dmb    ish\n"                 /* önceki erişimler sıralı */
            "ldxr   %0, [%3]\n"            /* benim_bilet = sonraki_bilet (exclusive) */
            "add    %1, %0, #1\n"          /* yeni = benim_bilet + 1 */
            "stxr   %w2, %1, [%3]\n"       /* sonraki_bilet = yeni dene; basarisiz=0 OK */
            "dmb    ish\n"                 /* sonraki erişimler sıralı */
            : "=&r"(benim_bilet), "=&r"(yeni), "=&r"(basarisiz)
            : "r"(&sonraki_bilet)
            : "memory");
        if (basarisiz == 0) {
            /* Başarılı yazımı RAM'e boşalt ki rakip çekirdek taze görsün. */
            satir_temizle_gecersiz(&sonraki_bilet);
            return benim_bilet;
        }
        /* STXR fail (rakip araya girdi) → taze değerle yeniden dene. */
    } while (1);
}

/* --- kilitle: `simdi_hizmet == bilet` olana kadar bekle (poll) ---
 *
 * Biletler artan sırada verildiği için hizmet de artan sırada ilerler → benim
 * biletim hizmet edilene kadar (yani benden önceki tüm biletler kritik bölgeyi
 * bitirip ac() çağırana kadar) beklerim. Bu FIFO garantisidir.
 *
 * MMU-off/on: her poll okumasından önce satırı tazele (dc ivac) → diğer
 * çekirdeğin ac()'ının RAM'e yazdığı taze `simdi_hizmet`'i gör. Bounded:
 * TIMEOUT_TIK üst sınırı (kilit-doğruluk bozulursa sonsuz spin yerine dönüş →
 * çağıran gerçek sayacı basıp FAIL raporlar). Dönüş: 1 = kilit alındı, 0 =
 * timeout (deadlock/bozulma). */
static inline int kilitle(uint64_t bilet) {
    for (uint64_t bekle = 0; bekle < TIMEOUT_TIK; bekle++) {
        satir_gecersiz(&simdi_hizmet);
        if (simdi_hizmet == bilet) {
            /* Kilit alındı: sonraki kritik-bölge okumaları için acquire bariyeri. */
            __asm__ volatile("dmb ish" ::: "memory");
            return 1;
        }
        /* Kısa backoff (bounded — meşgul bekleme yerine yield). */
        __asm__ volatile("yield" ::: "memory");
    }
    return 0;  /* timeout: kilit alınamadı (bozulma) */
}

/* --- ac: `simdi_hizmet`'i 1 artır → sıradaki bilet sahibi girer ---
 *
 * Kritik bölgeyi bitiren çekirdek çağırır. `simdi_hizmet++` yalnız kilit
 * sahibince çağrılır (bir anda tek çekirdek kritik bölgede) → atomik gerekmez;
 * düz oku-artır-yaz yeterli. dmb ish (release) kritik-bölge yazımlarının bu
 * artıştan ÖNCE görünmesini garanti eder; dc civac + dsb yeni değeri RAM'e
 * boşaltır ki bekleyen diğer çekirdek görsün. */
static inline void ac(void) {
    __asm__ volatile("dmb ish" ::: "memory");   /* release: kritik-bölge yazımları önce */
    satir_gecersiz(&simdi_hizmet);              /* taze oku (tek yazan biz olsak da coherency) */
    simdi_hizmet = simdi_hizmet + 1;            /* sıradaki bilete geç */
    satir_temizle_gecersiz(&simdi_hizmet);      /* RAM'e boşalt → bekleyen görsün */
}

/* --- Ortak çekişme döngüsü (HER İKİ çekirdek aynısını koşar) ---
 *
 * N_TUR kez: bilet al → kilitle → paylaşımlı sayacı DÜZ (atomik-olmayan) artır
 * (kilit koruduğu için yarış yok) → aç. Ayrıca bu çekirdeğin işlediği kritik-
 * bölge sayısını kendi slotunda tutar (FIFO adalet izleme). Kilit bir turda
 * alınamazsa (timeout) döngüyü keser → çağıran gerçek sayacı basıp FAIL raporlar
 * (sessiz-gizleme yok). Dönüş: bu çekirdeğin işlediği kritik-bölge sayısı. */
static uint64_t ticket_dongusu(volatile uint64_t *islenen_slot) {
    uint64_t islenen = 0;
    for (uint64_t k = 0; k < N_TUR; k++) {
        uint64_t bilet = bilet_al();
        if (!kilitle(bilet)) {
            break;  /* timeout — kilit-doğruluk bozuldu; dış doğrulama yakalar */
        }
        /* --- KRİTİK BÖLGE (kilit koruması altında; atomik GEREKMEZ) --- */
        satir_gecersiz(&sayac);          /* diğer çekirdeğin son yazımını gör */
        sayac = sayac + 1;               /* düz artır — kilit serialize etti */
        satir_temizle_gecersiz(&sayac);  /* RAM'e boşalt → diğer çekirdek görsün */
        /* --- KRİTİK BÖLGE SONU --- */
        ac();
        islenen++;
    }
    /* Bu çekirdeğin işlediği kritik-bölge sayısını slotuna yaz (FIFO izleme). */
    *islenen_slot = islenen;
    satir_temizle_gecersiz(islenen_slot);
    return islenen;
}

/*
 * Çekirdek 1 asıl işi (C) — SP zaten kurulu çağrılır (trampoline'den).
 * UART'a DOKUNMA (çekirdek 0 ile paylaşımlı) — yalnız paylaşılan RAM'e yaz.
 */
__attribute__((noreturn))
void cekirdek1_isi(void) {
    /* 1. RENDEZVOUS: "hazırım" bayrağını RAM'e set et. Çekirdek 0 bunu görene
     *    kadar ticket döngüsüne girmez → iki çekirdek AYNI anda bilet çekmeye
     *    başlar (gerçek FIFO çekişmesi; tek çekirdek sırayla bitirmez). */
    __asm__ volatile("dsb sy" ::: "memory");
    cekirdek1_basladi = 1;
    satir_temizle_gecersiz(&cekirdek1_basladi);

    /* 2. GERÇEK İŞ: ticket-lock ile N_TUR kritik bölge koştur. */
    ticket_dongusu(&islenen_cekirdek1);

    /* 3. "Bittim" bayrağını en son set et (çekirdek 0 bunu poll eder). Sayaç ve
     *    işlenen-slot yazımları RAM'e ULAŞTIKTAN sonra → çekirdek 0 bayrağı
     *    görünce ötekiler de görünür. */
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
    kdl_yazdir_metin("SMP TICKET BASLA");

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
        kdl_yazdir_metin("SMP TICKET FAIL: CPU_ON basarisiz — 2. cekirdek baslamadi");
        for (;;) { __asm__ volatile("wfe"); }
    }

    /* --- 2. RENDEZVOUS: çekirdek 1'in "hazırım" bayrağını bekle (bounded).
     *        Böylece çekirdek 0 bilet çekmeye GİRMEDEN önce çekirdek 1 de yarışa
     *        hazır → iki çekirdek AYNI anda `sonraki_bilet`'e yarışır (gerçek
     *        FIFO çekişmesi). Bounded: bayrak gelmezse yine de devam et. --- */
    for (uint64_t bekle = 0; bekle < TIMEOUT_TIK; bekle++) {
        satir_gecersiz(&cekirdek1_basladi);
        if (cekirdek1_basladi != 0) { break; }
    }

    /* --- 3. Çekirdek 0 de ticket-lock ile N_TUR kritik bölge koşturur. İki
     *        çekirdek eşzamanlı `sonraki_bilet`'e yarışır → benzersiz biletler +
     *        FIFO hizmet her artırımı serialize eder (lost-update yok). --- */
    ticket_dongusu(&islenen_cekirdek0);

    /* --- 4. Çekirdek 1'in bitmesini bekle (bounded poll + cache-coherent). --- */
    int bitti = 0;
    for (uint64_t bekle = 0; bekle < TIMEOUT_TIK; bekle++) {
        satir_gecersiz(&cekirdek1_bitti);
        if (cekirdek1_bitti != 0) { bitti = 1; break; }
    }

    if (!bitti) {
        satir_gecersiz(&sayac);
        kdl_yaz_metin("SMP TICKET FAIL: cekirdek1 timeout — sayac=");
        kdl_yazdir_isaretsiz_tam64(sayac);
        for (;;) { __asm__ volatile("wfe"); }
    }

    /* --- 5. Son sayacı + her çekirdeğin işlediğini cache-coherent oku. --- */
    satir_gecersiz(&sayac);
    uint64_t son = sayac;
    satir_gecersiz(&islenen_cekirdek0);
    uint64_t c0 = islenen_cekirdek0;
    satir_gecersiz(&islenen_cekirdek1);
    uint64_t c1 = islenen_cekirdek1;

    kdl_yaz_metin("sayac=");
    kdl_yazdir_isaretsiz_tam64(son);
    kdl_yaz_metin("cekirdek0 isledi=");
    kdl_yazdir_isaretsiz_tam64(c0);
    kdl_yaz_metin("cekirdek1 isledi=");
    kdl_yazdir_isaretsiz_tam64(c1);

    /* --- 6. DOĞRULAMA:
     *   (1) KİLİT-DOĞRULUK: son sayac == 2*N == 10000 (kilit serialize etti →
     *       düz artırımda lost-update yok).
     *   (2) FIFO ADALET: c0 + c1 == 2*N ve her ikisi de > 0 (ikisi de gerçekten
     *       hizmet gördü → açlık yok; ticket-lock biletleri sırayla dağıttı).
     * Kilit-doğruluk bozulursa gerçek sayacı bas + FAIL (sessiz-gizleme yok). */
    int kilit_dogru = (son == BEKLENEN);
    int adalet = (c0 + c1 == BEKLENEN) && (c0 > 0) && (c1 > 0);

    if (kilit_dogru && adalet) {
        kdl_yazdir_metin("SMP TICKET OK");
    } else if (kilit_dogru) {
        /* Sayaç doğru ama işlenen paylaşımı beklenmedik (biri 0 veya toplam≠2N). */
        kdl_yaz_metin("SMP TICKET FAIL sayac=");
        kdl_yazdir_isaretsiz_tam64(son);
        kdl_yaz_metin("DOGRU ama adalet bozuk: cekirdek0=");
        kdl_yazdir_isaretsiz_tam64(c0);
        kdl_yaz_metin("cekirdek1=");
        kdl_yazdir_isaretsiz_tam64(c1);
        kdl_yazdir_metin("(FIFO acligi/kayip)");
    } else {
        kdl_yaz_metin("SMP TICKET FAIL sayac=");
        kdl_yazdir_isaretsiz_tam64(son);
        kdl_yaz_metin("(beklenen ");
        kdl_yazdir_isaretsiz_tam64(BEKLENEN);
        kdl_yazdir_metin(") — kilit serialize etmedi/lost-update");
    }

    for (;;) { __asm__ volatile("wfe"); }
}
