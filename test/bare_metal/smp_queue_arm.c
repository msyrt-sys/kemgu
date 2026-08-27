/*
 * KEMGU-OS bare-metal SMP DİNAMİK İŞ-KUYRUĞU (work-stealing) testi (aarch64).
 * ==========================================================================
 *
 * Milestone (D-170 üstünde): D-170 diziyi STATİK yarı-yarıya böldü (çekirdek 0
 * [0,100), çekirdek 1 [100,200)). Bu test DİNAMİK iş dağıtımı kanıtlar: tek bir
 * paylaşımlı iş kuyruğundan İKİ çekirdek de yarışarak öğe çeker (work-stealing).
 * Hangi çekirdeğin hangi öğeyi işlediği ÖNCEDEN belli değil — hız/zamanlama
 * belirler. Ama toplam DETERMİNİSTİK: her öğe TAM BİR KEZ işlenir (spinlock
 * sayesinde), dolayısıyla toplam her zaman aynı çıkar.
 *
 * İş tanımı: N=40 öğe; öğe i'nin işi = i*i hesaplamak. Paylaşımlı `sonraki_is`
 * indeksi 0'dan başlar. Her çekirdek döngüde:
 *   1. SPINLOCK al → i = sonraki_is; eğer i >= N ise (kuyruk boş) → unlock, dur.
 *      Aksi halde sonraki_is++ → unlock. (İş öğesini atomik "kap".)
 *   2. i*i hesapla (kritik bölge DIŞINDA — paralel gerçek iş burada).
 *   3. SPINLOCK al → ortak_toplam += i*i → unlock. (Yarışsız birikim.)
 *   4. Bu çekirdeğin işlenen-sayacını artır.
 * Kuyruk boşalınca her iki çekirdek de durur.
 *
 * DOĞRULAMA (üç koşul birden):
 *   (1) ortak_toplam == 20540  (= sum(i*i, i=0..39); her öğe tam bir kez işlendi)
 *   (2) cekirdek0_islenen > 0   (çekirdek 0 gerçekten iş çekti)
 *   (3) cekirdek1_islenen > 0   (çekirdek 1 gerçekten iş çekti = GERÇEK work-steal)
 *   Ayrıca cekirdek0_islenen + cekirdek1_islenen == N (her öğe sahiplenildi).
 * Üçü de sağlanırsa "SMP QUEUE OK".
 *
 * KRİTİK — cache coherency (D-170 dersi): Çekirdek 1 MMU-OFF (non-cacheable,
 * doğrudan RAM); çekirdek 0 MMU-ON (Normal-WB cacheable). Bu cacheability
 * uyuşmazlığı donanım coherency'sini bozar → paylaşılan her satır (kilit,
 * sonraki_is, ortak_toplam, per-çekirdek sayaçlar) için EL ile senkronizasyon:
 *   - Paylaşılan satıra YAZDIKTAN sonra `dc civac` + `dsb sy` → RAM'e boşalt.
 *   - Paylaşılan satırı OKUMADAN önce `dc ivac` + `dsb sy` → tazesini yükle.
 *
 * ⚠⚠ [D-490] BU DOSYADAKI ONBELLEK BAKIMI VE BARIYERLER QEMU'DA OLCULMUYOR.
 * OLCULDU (tahmin degil): `dc civac` + `dc ivac` + eslik eden `dsb sy`
 * komutlarinin DORDU DE `nop`a cevrildi (kaynak dogrulandi, dc sayisi 0) ve
 * test BIREBIR AYNI sonucla GECTI (toplam=20540). Yani QEMU TCG onbellegi ve
 * zayif bellek modelini MODELLEMIYOR.
 *
 * UC SONUC:
 *  1. Buradaki bariyerler DOGRU ve GEREKLIDIR (gercek ARM64 donaniminda
 *     onlarsiz bu kod BOZULUR) — ama hicbir kapi bunu zorlamiyor. Sessizce
 *     silinseler QEMU kapisi YESIL kalir.
 *  2. QEMU uzerine kurulacak bir "zayif bellek" kapisi HICBIR SEY KANITLAMAZ.
 *     Bu yuzden boyle bir kapi EKLENMEDI (D-425: yanlisin gozlenebilir
 *     oldugu sekli olcemeyen kapi, kapi degildir).
 *  3. Gercek dogrulama YALNIZ FIZIKSEL ARM64 DONANIMINDA yapilabilir
 *     (DGX Spark). Oraya tasindiginda bu dosya ILK kosulacaklardan olmali ve
 *     yukaridaki sabotaj (bariyerleri nop yap) ORADA TEKRARLANMALI: gercek
 *     donanimda KIRMIZI olmasi beklenir. Olmazsa test yeterince zorlamıyordur.
 * Spinlock LDAXR/STLR (acquire/release) bariyerleri kritik-bölge sıralamasını
 * garanti eder; cacheability uyuşmazlığında görünürlük için `dc` de şart.
 *
 * DETERMİNİSTİK: iş kuyruğu bounded (N=40); çekirdek 1'in bitmesi bounded poll
 * (< TIMEOUT_TIK). Yük/internet bağımsız — toplam her koşuda 20540.
 */
#include <stdint.h>

extern void kdl_yazdir_metin(const char *);
extern void kdl_yaz_metin(const char *);
extern void kdl_yazdir_onaltilik(uint64_t);
extern void kdl_yaz_onaltilik(uint64_t);
extern void kdl_yazdir_isaretsiz_tam64(uint64_t);  /* satır-sonlu ondalık */

#define N_IS         40
#define BEKLENEN     20540ULL          /* sum(i*i, i=0..39) = 39*40*79/6 */
#define TIMEOUT_TIK  40000000ULL       /* bounded üst-sınır (yük-bağımsız) */

/* --- Paylaşımlı iş kuyruğu durumu (her biri kendi 64-byte cache satırında,
 *     yanlış-paylaşım/klobber önlemi) --- */

/* Sıradaki işlenecek öğenin indeksi. 0'dan N_IS'e kadar; SPINLOCK korumalı. */
static volatile uint64_t sonraki_is __attribute__((aligned(64))) = 0;

/* Tüm i*i sonuçlarının SPINLOCK-korumalı ortak toplamı. */
static volatile uint64_t ortak_toplam __attribute__((aligned(64))) = 0;

/* Her çekirdeğin işlediği öğe sayısı (SPINLOCK korumalı — atomik artış). */
static volatile uint64_t cekirdek0_islenen __attribute__((aligned(64))) = 0;
static volatile uint64_t cekirdek1_islenen __attribute__((aligned(64))) = 0;

/* Çekirdek 1'in "başladım / hazırım" bayrağı (rendezvous — ayrı satır).
 * Çekirdek 0 bu bayrağı görene kadar kuyruğa girmez → iki çekirdek AYNI anda
 * yarışmaya başlar (adil work-stealing; tek çekirdek kuyruğu tek başına
 * boşaltamaz). */
static volatile uint64_t cekirdek1_basladi __attribute__((aligned(64))) = 0;

/* Çekirdek 1'in "işim bitti" bayrağı (ayrı satır). */
static volatile uint64_t cekirdek1_bitti __attribute__((aligned(64))) = 0;

/* SPINLOCK kilit sözcüğü (ayrı satır). */
static volatile uint32_t kilit __attribute__((aligned(64))) = 0;

/* Çekirdek 1'in kendi yığını (çekirdek 0'ınkinden ayrı) — 8 KB.
 * NOT: naked trampoline yalnız asm'den (sembol adıyla) eriştiği için dış-bağ
 * (non-static) → linker sembolü emit eder (aksi halde "unused" atılır). */
uint8_t cekirdek1_yigin[8192] __attribute__((aligned(16)));

/* PSCI conduit çağrıları (smp_compute_arm.c ile aynı desen). */
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

/* --- Dinamik iş kuyruğu: sıradaki öğeyi atomik kap (work-stealing) ---
 *
 * SPINLOCK korumasında paylaşımlı `sonraki_is` indeksini oku; kuyruk boşsa
 * (i >= N_IS) N_IS döner (dur sinyali), aksi halde indeksi işaretleyip artırır
 * ve o indeksi döner. Her öğe TAM BİR KEZ döndürülür → toplam deterministik. */
static uint64_t sonraki_isi_kap(void) {
    uint64_t i;
    kilit_al();
    /* Kritik bölge: diğer çekirdeğin son yazımını gör (dc ivac), oku, artır,
     * boşalt (dc civac). */
    satir_gecersiz(&sonraki_is);
    i = sonraki_is;
    if (i < N_IS) {
        sonraki_is = i + 1;
        satir_temizle_gecersiz(&sonraki_is);
    }
    /* Kuyruk boşsa yazma yok (indeks zaten >= N_IS) — sadece i döner. */
    kilit_birak();
    return i;   /* i == N_IS ise "kuyruk boş" sinyali */
}

/* Ortak toplama spinlock korumasında ekle (her iki çekirdek çağırır). */
static inline void toplama_ekle(uint64_t deger) {
    kilit_al();
    satir_gecersiz(&ortak_toplam);
    ortak_toplam += deger;
    satir_temizle_gecersiz(&ortak_toplam);
    kilit_birak();
}

/* Bir çekirdeğin işlenen-sayacını spinlock korumasında artır. */
static inline void islenen_artir(volatile uint64_t *sayac) {
    kilit_al();
    satir_gecersiz(sayac);
    *sayac += 1;
    satir_temizle_gecersiz(sayac);
    kilit_birak();
}

/* --- Ortak work-stealing döngüsü (HER İKİ çekirdek aynısını koşar) ---
 *
 * Kuyruktan öğe çekilebildiği sürece: i*i hesapla (kritik bölge dışında =
 * paralel gerçek iş), ortak toplama ekle, bu çekirdeğin sayacını artır. */
static void kuyrugu_isle(volatile uint64_t *islenen_sayac) {
    for (;;) {
        uint64_t i = sonraki_isi_kap();
        if (i >= N_IS) {
            break;   /* Kuyruk boş → dur. */
        }
        /* Gerçek iş (kritik bölge DIŞINDA, gerçekten paralel). Bounded küçük
         * gecikme: tek çekirdek 40 öğeyi anında yutup ötekini aç bırakmasın →
         * her iki çekirdek de kuyruğa erişip iş çeksin (adil work-stealing).
         * DETERMİNİSTİK: sabit tik, yük-bağımsız. */
        uint64_t is_sonucu = i * i;
        for (volatile uint32_t g = 0; g < 20000; g++) {
            __asm__ volatile("" ::: "memory");
        }
        toplama_ekle(is_sonucu);
        islenen_artir(islenen_sayac);
    }
}

/*
 * Çekirdek 1 asıl işi (C) — SP zaten kurulu çağrılır (trampoline'den).
 * UART'a DOKUNMA (çekirdek 0 ile paylaşımlı) — yalnız paylaşılan RAM'e yaz.
 */
__attribute__((noreturn))
void cekirdek1_isi(void) {
    /* 1. RENDEZVOUS: "hazırım" bayrağını RAM'e set et. Çekirdek 0 bunu görene
     *    kadar kuyruğa girmez → iki çekirdek AYNI anda yarışır (adil work-
     *    stealing; çekirdek 0 kuyruğu tek başına boşaltamaz). */
    __asm__ volatile("dsb sy" ::: "memory");
    cekirdek1_basladi = 1;
    satir_temizle_gecersiz(&cekirdek1_basladi);

    /* 2. GERÇEK İŞ: kuyruktan öğe çek, işle (work-stealing). */
    kuyrugu_isle(&cekirdek1_islenen);

    /* 3. "Bittim" bayrağını en son set et (çekirdek 0 bunu poll eder). Sayaç ve
     *    toplam yazımları RAM'e ULAŞTIKTAN sonra → çekirdek 0 bayrağı görünce
     *    ötekiler de görünür. */
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
 * D-170 compute testinde işi basit olduğundan spill YOKTU, burada var).
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
    kdl_yazdir_metin("SMP QUEUE BASLA");

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
        kdl_yazdir_metin("SMP QUEUE FAIL: CPU_ON basarisiz — 2. cekirdek baslamadi");
        for (;;) { __asm__ volatile("wfe"); }
    }

    /* --- 2. RENDEZVOUS: çekirdek 1'in "hazırım" bayrağını bekle (bounded).
     *        Böylece çekirdek 0 kuyruğa GİRMEDEN önce çekirdek 1 de yarışa hazır
     *        → iki çekirdek AYNI anda `sonraki_is`'e yarışır (adil work-steal).
     *        Bounded: bayrak gelmezse yine de devam et (test yine geçerli olur
     *        ama work-stealing zayıf; aşağıdaki DOĞRULAMA yakalar). --- */
    for (uint64_t bekle = 0; bekle < TIMEOUT_TIK; bekle++) {
        satir_gecersiz(&cekirdek1_basladi);
        if (cekirdek1_basladi != 0) { break; }
    }

    /* --- 3. Çekirdek 0 de AYNI kuyruktan iş çeker (work-stealing). İki çekirdek
     *        de eşzamanlı `sonraki_is`'e yarışır → spinlock her öğeyi tam bir
     *        çekirdeğe verir. --- */
    kuyrugu_isle(&cekirdek0_islenen);

    /* --- 4. Çekirdek 1'in bitmesini bekle (bounded poll + cache-coherent). --- */
    int bitti = 0;
    for (uint64_t bekle = 0; bekle < TIMEOUT_TIK; bekle++) {
        satir_gecersiz(&cekirdek1_bitti);
        if (cekirdek1_bitti != 0) { bitti = 1; break; }
    }

    if (!bitti) {
        satir_gecersiz(&cekirdek0_islenen);
        satir_gecersiz(&cekirdek1_islenen);
        satir_gecersiz(&sonraki_is);
        kdl_yaz_metin("SMP QUEUE FAIL: cekirdek1 timeout — c0=");
        kdl_yazdir_isaretsiz_tam64(cekirdek0_islenen);
        kdl_yaz_metin("c1=");
        kdl_yazdir_isaretsiz_tam64(cekirdek1_islenen);
        kdl_yaz_metin("sonraki_is=");
        kdl_yazdir_isaretsiz_tam64(sonraki_is);
        kdl_yazdir_metin("");
        for (;;) { __asm__ volatile("wfe"); }
    }

    /* --- 5. Sonuçları cache-coherent oku. --- */
    satir_gecersiz(&ortak_toplam);
    uint64_t toplam = ortak_toplam;
    satir_gecersiz(&cekirdek0_islenen);
    uint64_t c0 = cekirdek0_islenen;
    satir_gecersiz(&cekirdek1_islenen);
    uint64_t c1 = cekirdek1_islenen;

    kdl_yaz_metin("cekirdek0 islenen=");
    kdl_yazdir_isaretsiz_tam64(c0);
    kdl_yaz_metin("cekirdek1 islenen=");
    kdl_yazdir_isaretsiz_tam64(c1);
    kdl_yaz_metin("toplam=");
    kdl_yazdir_isaretsiz_tam64(toplam);

    /* --- 6. DOĞRULAMA: toplam doğru VE iki çekirdek de iş çekti VE tüm öğeler
     *        sahiplenildi. --- */
    if (toplam == BEKLENEN && c0 > 0 && c1 > 0 && (c0 + c1) == N_IS) {
        kdl_yazdir_metin("SMP QUEUE OK");
    } else if (toplam == BEKLENEN && (c0 == 0 || c1 == 0)) {
        /* Toplam doğru ama tek çekirdek tüm işi yaptı → work-stealing kanıtlanamadı. */
        kdl_yaz_metin("SMP QUEUE KISMI: toplam=");
        kdl_yazdir_isaretsiz_tam64(toplam);
        kdl_yaz_metin("DOGRU ama tek cekirdek isledi (c0=");
        kdl_yazdir_isaretsiz_tam64(c0);
        kdl_yaz_metin("c1=");
        kdl_yazdir_isaretsiz_tam64(c1);
        kdl_yazdir_metin(") — work-stealing kanitlanamadi");
    } else {
        /* Toplam yanlış → coherency/yarış bozulmuş (öğe kaybı veya çift işleme). */
        kdl_yaz_metin("SMP QUEUE FAIL: toplam=");
        kdl_yazdir_isaretsiz_tam64(toplam);
        kdl_yaz_metin("(beklenen 20540) c0=");
        kdl_yazdir_isaretsiz_tam64(c0);
        kdl_yaz_metin("c1=");
        kdl_yazdir_isaretsiz_tam64(c1);
        kdl_yazdir_metin("");
    }

    for (;;) { __asm__ volatile("wfe"); }
}
