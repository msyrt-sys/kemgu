/*
 * KEMGU-OS bare-metal SMP ÇEKİRDEKLER-ARASI ÜRETİCİ-TÜKETİCİ testi (aarch64).
 * ==========================================================================
 *
 * Milestone (D-170/174/180 üstünde): D-174 iki çekirdeğin tek paylaşımlı iş
 * kuyruğundan yarışarak öğe çekmesini (work-stealing, SPINLOCK ile) kanıtladı;
 * D-180 spinlocksuz LDXR/STXR atomik sayaç çekişmesini kanıtladı. Bu test bir
 * kademe DAHA gerçekçi bir eşzamanlılık desenini kanıtlar: ÇEKİRDEKLER-ARASI
 * ÜRETİCİ-TÜKETİCİ akışı — çekirdek 0 ÜRETİR, çekirdek 1 TÜKETİR, aralarında
 * paylaşımlı bir HALKA TAMPON (SPSC ring buffer) akar.
 *
 * SPSC (Single-Producer Single-Consumer) — KİLİTSİZ tasarım:
 *   - Yalnız ÜRETİCİ (çekirdek 0) `bas`'ı (head) yazar.
 *   - Yalnız TÜKETİCİ (çekirdek 1) `son`'u (tail) yazar.
 *   - İkisi de yalnız KARŞI tarafın indeksini OKUR (dolu/boş testi için).
 *   Tek yazar-tek okur invaryantı sayesinde SPINLOCK GEREKMEZ — bellek
 *   bariyerleri (dsb + cache clean/invalidate) yeterli. Bu, kilitli kuyruktan
 *   (D-174) ve atomik-RMW'den (D-180) ayrı, ÜÇÜNCÜ eşzamanlılık disiplini.
 *
 * Ring semantiği — SERBEST-AKAN sayaç (bas/son maskelenmez, sonsuz büyür; yalnız
 * indekslerken & MASKE ile sarmalanır; KAP = güç-of-2):
 *   - Boş  : bas == son
 *   - Dolu : (bas - son) == KAP           (tüm KAP slot dolu — slot israfı yok)
 *   - Yazma: veri[bas & MASKE] = deger; dsb; bas = bas + 1       (üretici)
 *   - Okuma: deger = veri[son & MASKE];   dsb; son = son + 1     (tüketici)
 * NOT: sol tarafı maskeleyip serbest-akan son ile kıyaslamak (karışık kural)
 * son>MASKE olunca dolu-testini bozar → üretici tüketiciyi bir tur aşar. Ya HER
 * İKİ tarafı maskele ya da HİÇBİRİNİ (serbest-akan fark) — burada ikincisi.
 *
 * İş tanımı: N=1000 öğe. Üretici i = 0..999 sırayla ring'e yazar (dolu ise
 * bekler). Tüketici 1000 öğe okur (boş ise bekler); okuduğu her öğenin BEKLENEN
 * SIRADA (0,1,2,...,999) geldiğini doğrular (SPSC FIFO korunmalı) ve toplama
 * ekler.
 *
 * DOĞRULAMA (üç koşul birden):
 *   (1) tuketilen == 1000            (tüm öğeler tüketildi)
 *   (2) sira_bozuldu == 0            (öğeler TAM 0..999 SIRASINDA geldi = SPSC FIFO)
 *   (3) tuketilen_toplam == 499500   (= sum(i, i=0..999) = 999*1000/2; kayıp/tekrar yok)
 *   Üçü de sağlanırsa "SMP PRODCONS OK".
 *
 * KRİTİK — cache coherency (D-170/174/180 dersi): Çekirdek 1 MMU-OFF (non-
 * cacheable, doğrudan RAM); çekirdek 0 MMU-ON (Normal-WB cacheable). Bu
 * cacheability uyuşmazlığı donanım coherency'sini bozar → paylaşılan HER satır
 * (ring verisi, bas, son) için EL ile senkronizasyon:
 *   - Paylaşılan satıra YAZDIKTAN sonra `dc civac` + `dsb sy` → RAM'e boşalt.
 *   - Paylaşılan satırı OKUMADAN önce `dc ivac` + `dsb sy` → tazesini yükle.
 * SPSC doğruluğu tümüyle bu bariyerlere dayanır (kilit yok): üretici veriyi
 * `bas`'ı ilerletmeden ÖNCE RAM'e boşaltmalı, tüketici `bas`'ı okumadan ÖNCE
 * tazelemeli → yayın-tüket sıralaması (release/acquire) korunur.
 *
 * DETERMİNİSTİK: üretici/tüketici sabit N=1000 öğe (bounded). Dolu/boş beklemesi
 * TOPLAM bounded (her iterasyonda TIMEOUT_TIK değil — üretici+tüketici birlikte
 * TOPLAM_TIMEOUT_TIK'i aşarsa "SMP PRODCONS FAIL/TIMEOUT" → deadlock sessizce
 * gizlenmez). Yük/internet bağımsız — toplam her koşuda 499500.
 */
#include <stdint.h>

extern void kdl_yazdir_metin(const char *);
extern void kdl_yaz_metin(const char *);
extern void kdl_yazdir_onaltilik(uint64_t);
extern void kdl_yaz_onaltilik(uint64_t);
extern void kdl_yazdir_isaretsiz_tam64(uint64_t);  /* satır-sonlu ondalık */

#define RING_KAP     16u                 /* halka kapasitesi (güç-of-2) */
#define RING_MASKE   (RING_KAP - 1u)     /* sarmalama maskesi (& ile) */
#define N_OGE        1000ULL             /* üretilecek/tüketilecek öğe sayısı */
#define BEKLENEN     499500ULL           /* sum(i, i=0..999) = 999*1000/2 */

/* Deadlock-guard: üretici VE tüketici döngülerinde TOPLAM bekleme (tüm boş/dolu
 * spin iterasyonlarının toplamı) bu sınırı aşarsa "TIMEOUT" → sessiz asılma yok.
 * Yük-bağımsız, cömert üst-sınır (normalde asla ulaşılmaz). */
#define TOPLAM_TIMEOUT_TIK  200000000ULL

/* --- Paylaşımlı SPSC halka tampon durumu (false-sharing önlemek için üretici
 *     ve tüketici yazdığı indeksler AYRI 64-byte cache satırlarında) --- */

/* Halka veri slotu — HER slot KENDİ 64-byte cache satırında (padding ile).
 * KRİTİK: `dc civac`/`dc ivac` bir SATIRIN TAMAMINA etki eder. Slotlar tek
 * dizide bitişik uint64 olsaydı (8 slot/satır), bir slotu temizle/geçersiz-kıl
 * komşu slotları da etkiler → MMU-off tüketici ile MMU-on üretici arasında
 * torn/stale okuma (üretici sarmalama sonrası bir slotu yeniden yazarken,
 * tüketicinin `dc ivac`'ı komşu-henüz-boşalmamış satırı düşürür / üreticinin
 * `dc civac`'ı tüketicinin okuduğu komşu slotu geçersiz kılar). Her slotu ayrı
 * satıra koyunca cache-bakım granülaritesi TAM O SLOTA sınırlanır. */
typedef struct {
    volatile uint64_t deger;
    volatile uint8_t  dolgu[64 - sizeof(uint64_t)];  /* satırı 64 byte'a tamamla */
} __attribute__((aligned(64))) RingSlot;

/* Halka veri dizisi — her slot kendi cache satırında. Üretici yazar, tüketici
 * okur (aynı slotu asla eşzamanlı değil: dolu/boş kontrolü ayırır). */
static RingSlot veri[RING_KAP] __attribute__((aligned(64))) = {{0, {0}}};

/* Yazma indeksi (head). YALNIZ ÜRETİCİ yazar; tüketici yalnız okur. Ayrı satır. */
static volatile uint64_t bas __attribute__((aligned(64))) = 0;

/* Okuma indeksi (tail). YALNIZ TÜKETİCİ yazar; üretici yalnız okur. Ayrı satır. */
static volatile uint64_t son __attribute__((aligned(64))) = 0;

/* Tüketicinin biriktirdiği toplam (yalnız tüketici yazar). Ayrı satır. */
static volatile uint64_t tuketilen_toplam __attribute__((aligned(64))) = 0;

/* Tüketilen öğe sayısı (yalnız tüketici yazar). Ayrı satır. */
static volatile uint64_t tuketilen __attribute__((aligned(64))) = 0;

/* SPSC FIFO sıra ihlali bayrağı: tüketici beklenmeyen bir değer görürse 1 yapar
 * (yalnız tüketici yazar). 0 kalmalı → sıra korundu. Ayrı satır. */
static volatile uint64_t sira_bozuldu __attribute__((aligned(64))) = 0;

/* Tüketici deadlock-guard'ı aştıysa (boş-bekleme timeout) 1 yapar. Ayrı satır. */
static volatile uint64_t tuketici_timeout __attribute__((aligned(64))) = 0;

/* TANI: ilk sıra-ihlalinde beklenen/gelen değer (hata teşhisi). Ayrı satır. */
static volatile uint64_t ilk_hata_beklenen __attribute__((aligned(64))) = 0xFFFFFFFFULL;
static volatile uint64_t ilk_hata_gelen __attribute__((aligned(64))) = 0xFFFFFFFFULL;

/* Çekirdek 1'in (tüketici) "başladım / hazırım" bayrağı (rendezvous — ayrı
 * satır). Çekirdek 0 (üretici) bunu görene kadar üretmeye başlamaz → üretici ve
 * tüketici AYNI anda akışa girer (gerçek çekirdekler-arası akış; ring tek
 * çekirdekçe önden doldurulup boşaltılmaz). */
static volatile uint64_t cekirdek1_basladi __attribute__((aligned(64))) = 0;

/* Çekirdek 1'in (tüketici) "işim bitti" bayrağı (ayrı satır). */
static volatile uint64_t cekirdek1_bitti __attribute__((aligned(64))) = 0;

/* Çekirdek 1'in kendi yığını (çekirdek 0'ınkinden ayrı) — 8 KB.
 * NOT: naked trampoline yalnız asm'den (sembol adıyla) eriştiği için dış-bağ
 * (non-static) → linker sembolü emit eder (aksi halde "unused" atılır). */
uint8_t cekirdek1_yigin[8192] __attribute__((aligned(16)));

/* PSCI conduit çağrıları (smp_queue_arm.c / smp_atomic_arm.c ile aynı desen). */
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

/* --- ÜRETİCİ (çekirdek 0): N_OGE öğeyi (0..N_OGE-1) SPSC ring'e sırayla yaz ---
 *
 * SPSC invaryantı: yalnız `bas`'ı YAZAR, yalnız `son`'u OKUR. Ring dolu iken
 * (((bas+1)&MASKE) == son) tüketicinin bir slot açmasını bekler. Bir slot açık
 * olunca veriyi yazar → RAM'e boşaltır (dc civac) → SONRA `bas`'ı ilerletir →
 * onu da RAM'e boşaltır. YAYIN sıralaması kritik: veri `bas` görünürlüğünden
 * ÖNCE RAM'e ulaşmalı (tüketici `bas`'ı görünce veriyi de görsün).
 *
 * Deadlock-guard: dolu-bekleme spin'lerinin TOPLAMI bounded (aşılırsa timeout
 * işaretle ve dur → sessiz asılma yok). Döner: 0 = tamamlandı, 1 = timeout. */
static int uret(void) {
    uint64_t toplam_bekleme = 0;
    for (uint64_t i = 0; i < N_OGE; i++) {
        /* 1. Ring dolu iken tüketicinin bir slot açmasını bekle (bounded).
         *    bas ve son SERBEST-AKAN sayaçlardır (maskelenmez, sonsuz büyür;
         *    yalnız indekslerken & RING_MASKE). Bu kurallada:
         *      dolu  : (bas - son) == RING_KAP   (tüm slotlar dolu)
         *      boş   : bas == son
         *    (Karışık maskeli-karşılaştırma HATASINA düşme: sol tarafı maskeleyip
         *    sağ serbest-akan son ile kıyaslarsan son>MASKE olunca dolu asla
         *    tetiklenmez → üretici tüketiciyi tam bir tur aşar = bozulma.) */
        for (;;) {
            uint64_t yerel_bas = bas;                 /* kendi yazdığımız → taze */
            satir_gecersiz(&son);                     /* tüketicinin son'unu tazele */
            uint64_t yerel_son = son;
            if ((yerel_bas - yerel_son) != RING_KAP) {
                break;                                /* slot var → yaz */
            }
            /* Dolu → bekle (deadlock-guard). */
            if (++toplam_bekleme >= TOPLAM_TIMEOUT_TIK) {
                return 1;                             /* timeout */
            }
            __asm__ volatile("yield" ::: "memory");
        }
        /* 2. Veriyi yaz → RAM'e boşalt (bas ilerlemeden ÖNCE görünür olmalı). */
        uint64_t yerel_bas = bas;
        veri[yerel_bas & RING_MASKE].deger = i;
        satir_temizle_gecersiz(&veri[yerel_bas & RING_MASKE].deger);
        /* 3. bas'ı ilerlet → RAM'e boşalt (tüketici bunu görünce yeni öğe hazır). */
        __asm__ volatile("dsb sy" ::: "memory");
        bas = yerel_bas + 1u;
        satir_temizle_gecersiz(&bas);
    }
    return 0;
}

/* --- TÜKETİCİ (çekirdek 1): N_OGE öğeyi SPSC ring'den sırayla oku + doğrula ---
 *
 * SPSC invaryantı: yalnız `son`'u YAZAR, yalnız `bas`'ı OKUR. Ring boş iken
 * (bas == son) üreticinin bir öğe yazmasını bekler. Bir öğe gelince veriyi okur
 * → doğrular (beklenen sıra) → toplama ekler → SONRA `son`'u ilerletir. TÜKET
 * sıralaması kritik: veri okuması `son` ilerlemesinden ÖNCE bitmeli (üretici
 * `son`'u görüp slotu geri kullanabilir).
 *
 * Deadlock-guard: boş-bekleme spin'lerinin TOPLAMI bounded (aşılırsa
 * tuketici_timeout=1 işaretle ve dur → sessiz asılma yok). */
static void tuket(void) {
    uint64_t toplam_bekleme = 0;
    for (uint64_t okunan = 0; okunan < N_OGE; okunan++) {
        /* 1. Ring boş iken üreticinin bir öğe yazmasını bekle (bounded). */
        for (;;) {
            uint64_t yerel_son = son;                 /* kendi yazdığımız → taze */
            satir_gecersiz(&bas);                     /* üreticinin bas'ını tazele */
            uint64_t yerel_bas = bas;
            if (yerel_bas != yerel_son) {
                break;                                /* öğe var → oku */
            }
            /* Boş → bekle (deadlock-guard). */
            if (++toplam_bekleme >= TOPLAM_TIMEOUT_TIK) {
                tuketici_timeout = 1;
                satir_temizle_gecersiz(&tuketici_timeout);
                return;                               /* timeout → yarım tüketildi */
            }
            __asm__ volatile("yield" ::: "memory");
        }
        /* 2. Veriyi oku (üreticinin RAM'e boşalttığı slotu tazele). */
        uint64_t yerel_son = son;
        satir_gecersiz(&veri[yerel_son & RING_MASKE].deger);
        uint64_t deger = veri[yerel_son & RING_MASKE].deger;

        /* 3. SPSC FIFO doğrulama: öğe TAM beklenen sırada (0,1,...) gelmeli. */
        if (deger != okunan) {
            if (sira_bozuldu == 0) {
                ilk_hata_beklenen = okunan;
                ilk_hata_gelen = deger;
                satir_temizle_gecersiz(&ilk_hata_beklenen);
                satir_temizle_gecersiz(&ilk_hata_gelen);
            }
            sira_bozuldu = 1;
            satir_temizle_gecersiz(&sira_bozuldu);
        }
        tuketilen_toplam += deger;
        tuketilen += 1u;

        /* 4. son'u ilerlet → RAM'e boşalt (üretici slotu geri kullanabilir).
         *    Veri okuması son ilerlemesinden ÖNCE bitti (yukarıda). */
        __asm__ volatile("dsb sy" ::: "memory");
        son = yerel_son + 1u;
        satir_temizle_gecersiz(&son);
    }
    /* Sonuç sayaç/toplamları RAM'e boşalt (çekirdek 0 bitti-bayrağından sonra
     * cache-coherent okuyacak — yine de burada garantiye al). */
    satir_temizle_gecersiz(&tuketilen);
    satir_temizle_gecersiz(&tuketilen_toplam);
    satir_temizle_gecersiz(&sira_bozuldu);
}

/*
 * Çekirdek 1 asıl işi (C) — SP zaten kurulu çağrılır (trampoline'den).
 * TÜKETİCİ rolü. UART'a DOKUNMA (çekirdek 0 ile paylaşımlı) — yalnız paylaşılan
 * RAM'e yaz.
 */
__attribute__((noreturn))
void cekirdek1_isi(void) {
    /* 1. RENDEZVOUS: "hazırım" bayrağını RAM'e set et. Çekirdek 0 (üretici) bunu
     *    görene kadar üretmeye başlamaz → üretici ve tüketici AYNI anda akışa
     *    girer (gerçek çekirdekler-arası üretici-tüketici; ring önden dolup
     *    boşalmaz). */
    __asm__ volatile("dsb sy" ::: "memory");
    cekirdek1_basladi = 1;
    satir_temizle_gecersiz(&cekirdek1_basladi);

    /* 2. GERÇEK İŞ: ring'den N_OGE öğe tüket (SPSC, kilitsiz). */
    tuket();

    /* 3. "Bittim" bayrağını en son set et (çekirdek 0 bunu poll eder). Sayaç,
     *    toplam ve sıra-bayrağı yazımları RAM'e ULAŞTIKTAN sonra → çekirdek 0
     *    bayrağı görünce ötekiler de görünür. */
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
    kdl_yazdir_metin("SMP PRODCONS BASLA");

    /* --- 1. Çekirdek 1'i (tüketici) başlat (PSCI CPU_ON, HVC→SMC fallback). --- */
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
        kdl_yazdir_metin("SMP PRODCONS FAIL: CPU_ON basarisiz — 2. cekirdek baslamadi");
        for (;;) { __asm__ volatile("wfe"); }
    }

    /* --- 2. RENDEZVOUS: çekirdek 1'in (tüketici) "hazırım" bayrağını bekle
     *        (bounded). Böylece üretici (çekirdek 0) üretmeye GİRMEDEN önce
     *        tüketici de hazır → üretici ve tüketici AYNI anda akışa girer
     *        (gerçek çekirdekler-arası akış). Bayrak gelmezse yine de devam et
     *        (aşağıdaki DOĞRULAMA sonucu yakalar). --- */
    for (uint64_t bekle = 0; bekle < TOPLAM_TIMEOUT_TIK; bekle++) {
        satir_gecersiz(&cekirdek1_basladi);
        if (cekirdek1_basladi != 0) { break; }
    }

    /* --- 3. Çekirdek 0 ÜRETİR: N_OGE öğeyi (0..N_OGE-1) SPSC ring'e sırayla
     *        yazar. Tüketici (çekirdek 1) eşzamanlı boşaltır. --- */
    int uretici_timeout = uret();

    if (uretici_timeout) {
        satir_gecersiz(&bas);
        satir_gecersiz(&son);
        kdl_yaz_metin("SMP PRODCONS FAIL/TIMEOUT: uretici dolu-bekleme asti — bas=");
        kdl_yazdir_isaretsiz_tam64(bas);
        kdl_yaz_metin("son=");
        kdl_yazdir_isaretsiz_tam64(son);
        kdl_yazdir_metin("");
        for (;;) { __asm__ volatile("wfe"); }
    }

    /* --- 4. Çekirdek 1'in (tüketici) bitmesini bekle (bounded poll + cache-
     *        coherent). --- */
    int bitti = 0;
    for (uint64_t bekle = 0; bekle < TOPLAM_TIMEOUT_TIK; bekle++) {
        satir_gecersiz(&cekirdek1_bitti);
        if (cekirdek1_bitti != 0) { bitti = 1; break; }
    }

    if (!bitti) {
        satir_gecersiz(&tuketilen);
        satir_gecersiz(&bas);
        satir_gecersiz(&son);
        kdl_yaz_metin("SMP PRODCONS FAIL/TIMEOUT: cekirdek1 (tuketici) bitmedi — tuketilen=");
        kdl_yazdir_isaretsiz_tam64(tuketilen);
        kdl_yaz_metin("bas=");
        kdl_yazdir_isaretsiz_tam64(bas);
        kdl_yaz_metin("son=");
        kdl_yazdir_isaretsiz_tam64(son);
        kdl_yazdir_metin("");
        for (;;) { __asm__ volatile("wfe"); }
    }

    /* --- 5. Sonuçları cache-coherent oku. --- */
    satir_gecersiz(&tuketilen);
    uint64_t adet = tuketilen;
    satir_gecersiz(&tuketilen_toplam);
    uint64_t toplam = tuketilen_toplam;
    satir_gecersiz(&sira_bozuldu);
    uint64_t bozuk = sira_bozuldu;
    satir_gecersiz(&tuketici_timeout);
    uint64_t t_timeout = tuketici_timeout;

    kdl_yaz_metin("tuketilen=");
    kdl_yazdir_isaretsiz_tam64(adet);
    kdl_yaz_metin("toplam=");
    kdl_yazdir_isaretsiz_tam64(toplam);
    kdl_yaz_metin("sira_bozuldu=");
    kdl_yazdir_isaretsiz_tam64(bozuk);
    if (bozuk != 0) {
        satir_gecersiz(&ilk_hata_beklenen);
        satir_gecersiz(&ilk_hata_gelen);
        kdl_yaz_metin("ilk_hata beklenen=");
        kdl_yazdir_isaretsiz_tam64(ilk_hata_beklenen);
        kdl_yaz_metin("gelen=");
        kdl_yazdir_isaretsiz_tam64(ilk_hata_gelen);
    }

    /* --- 6. DOĞRULAMA: tüm öğeler tüketildi VE SPSC FIFO sırası korundu VE
     *        toplam doğru (kayıp/tekrar yok). --- */
    if (t_timeout != 0) {
        kdl_yaz_metin("SMP PRODCONS FAIL/TIMEOUT: tuketici bos-bekleme asti — tuketilen=");
        kdl_yazdir_isaretsiz_tam64(adet);
        kdl_yazdir_metin("");
    } else if (adet == N_OGE && bozuk == 0 && toplam == BEKLENEN) {
        kdl_yazdir_metin("SMP PRODCONS OK");
    } else {
        /* Toplam yanlış veya sıra bozuk veya eksik tüketim → coherency/SPSC ihlali. */
        kdl_yaz_metin("SMP PRODCONS FAIL: tuketilen=");
        kdl_yazdir_isaretsiz_tam64(adet);
        kdl_yaz_metin("(beklenen 1000) toplam=");
        kdl_yazdir_isaretsiz_tam64(toplam);
        kdl_yaz_metin("(beklenen 499500) sira_bozuldu=");
        kdl_yazdir_isaretsiz_tam64(bozuk);
        kdl_yazdir_metin("");
    }

    for (;;) { __asm__ volatile("wfe"); }
}
