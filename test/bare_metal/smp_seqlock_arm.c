/*
 * KEMGU-OS bare-metal SMP SEQLOCK (optimistik kilitsiz-okuma) testi (aarch64).
 * ===========================================================================
 *
 * Milestone (D-169/174/180/192/199 SMP + kilit üstünde): D-199 reader-writer
 * lock çoklu-okuyucu / tek-yazıcı ADALET kurdu ama orada OKUYUCU HÂLÂ KİLİT
 * ALIR (okuyucu_sayisi atomik ++/--) ve YAZICI okuyucular çıkana kadar BLOKLANIR.
 * Bu test bir kademe DAHA agresif eşzamanlılık kurar: SEQLOCK (sequence lock) —
 * okuma tarafı KİLİT ALMAZ (sıfır atomik RMW, sıfır yazma) ve yazıcı OKUYUCUYU
 * ASLA BEKLEMEZ. Okuyucu OPTİMİSTİKtir: veriyi kilitsiz okur, bir sıra-sayacı
 * (sequence) ile okumanın tutarlı (torn olmayan) olduğunu DOĞRULAR; araya yazım
 * girdiyse okumayı ATAR ve yeniden dener (retry).
 *
 * SEQLOCK çalışma prensibi:
 *   Durum:
 *     `seq` : global sıra sayacı. ÇİFT = veri stabil (yazım sürmüyor);
 *             TEK = bir yazım ŞU AN sürüyor (araya girme). Yazıcı her yazımı
 *             seq++ (çift→tek) ile açar, seq++ (tek→çift) ile kapatır → seq
 *             her tam yazımda 2 artar.
 *
 *   YAZICI (asla bloklanmaz — okuyucuyu beklemez):
 *     1. seq++            (çift→TEK: "yazım başladı" ilan et)
 *     2. dmb ish          (seq-tek YAZIMDAN ÖNCE görünür — seqlock'un KALBİ)
 *     3. korunan çifti yaz: veri_a++ ; veri_b = veri_a*2  (invaryant b==a*2)
 *     4. dmb ish          (yazımlar seq-çift'ten ÖNCE görünür — seqlock'un KALBİ)
 *     5. seq++            (tek→ÇİFT: "yazım bitti, veri stabil" ilan et)
 *
 *   OKUYUCU (optimistik — kilit ALMAZ, hiçbir şey YAZMAZ):
 *     1. s0 = seq oku. s0 TEK ise → yazım sürüyor, hemen RETRY.
 *     2. dmb ish          (s0 okuması VERİ okumalarından ÖNCE — sıra korunur)
 *     3. a = veri_a ; b = veri_b   (kilitsiz oku)
 *     4. dmb ish          (veri okumaları s1 okumasından ÖNCE)
 *     5. s1 = seq oku. s1 != s0  → okuma sırasında yazım araya girdi → RETRY.
 *                     s1 TEK     → yazım hâlâ sürüyor → RETRY.
 *     Aksi halde (s1 == s0, ikisi de çift): okuma TUTARLI — a,b aynı yazım-
 *     çağının değerleri, torn DEĞİL. Kabul et.
 *
 * TORN-READ GARANTİSİ (seqlock doğruluğu):
 *   Okuyucu (a,b) çiftini okurken yazıcı araya girip yarısını değiştirirse:
 *     - Yazıcı seq'i TEK yapmış olur (adım 1) → s1 != s0 VEYA s1 tek → RETRY.
 *   Böylece okuyucu YALNIZ s0==s1 (çift) olduğunda değeri kabul eder; bu da
 *   (a,b)'nin araya yazım girmemiş TEK bir stabil çağdan geldiğini garanti eder
 *   → okuyucu ASLA yarım-yazılmış (torn: b != a*2) çift KABUL ETMEZ. Kilit yok,
 *   ama tutarlılık var. Yazıcı okuyucuyu beklemediği için yazıcı gecikmesizdir.
 *
 * NEDEN dmb ish KRİTİK (yanlış bariyer = torn): Bariyersiz, işlemci/derleyici
 *   seq yazımını veri yazımından SONRAYA (veya reader'da veri okumasını s1
 *   okumasından SONRAYA) taşıyabilir → okuyucu "seq değişmedi" görürken veri
 *   yarı-yazılmış olabilir → SESSİZ torn. dmb ish tam bu yeniden-sıralamayı
 *   yasaklar: yazıcıda seq-tek HER ZAMAN veri-yazımından önce, seq-çift HER ZAMAN
 *   veri-yazımından sonra görünür; okuyucuda s0 HER ZAMAN veri-okumasından önce,
 *   veri-okuması HER ZAMAN s1'den önce. Bu dört sıralama seqlock'un KALBİDİR.
 *
 * KORUNAN VERİ — "tutarlı çift" invaryantı: paylaşımlı `veri_a` ve `veri_b`,
 * invaryant her zaman `veri_b == veri_a * 2`. Yazıcı önce veri_a'yı artırır SONRA
 * veri_b = veri_a * 2 yazar (seq-tek çerçevesi içinde). Okuyucu seqlock ile
 * çifti okur ve `veri_b == veri_a*2` mi diye DOĞRULAR. Seqlock doğruysa KABUL
 * EDİLEN her okumada invaryant tutar → torn_read == 0.
 *
 * İki çekirdek:
 *   Çekirdek 0 = YAZICI: N=2000 kez seqlock-yazım (seq++ tek → veri_a++ +
 *                veri_b=veri_a*2 → seq++ çift). Bittiğinde veri_a==N, veri_b==2*N,
 *                seq == 2*N (her yazım seq'i 2 artırır, başlangıç 0).
 *   Çekirdek 1 = OKUYUCU: yazıcı bitene kadar (bounded) sürekli seqlock-oku:
 *                optimistik oku + seq ile tutarlılık doğrula + KABUL EDİLEN
 *                okumada torn (b != a*2) say. Her başarısız optimistik denemede
 *                retry_sayisi++ (DÜRÜST — kaç kez yazım araya girdi).
 *
 * DOĞRULAMA (kritik invaryantlar deterministik):
 *   (1) TORN-READ YOK: torn_read == 0 (seqlock hiçbir yarım-yazılmış çift KABUL
 *       ETTİRMEDİ). torn_read > 0 çıkarsa "SMP SEQLOCK FAIL torn=N" — SESSİZ
 *       GİZLEME YOK, gerçek torn sayısı basılır.
 *   (2) YAZICI TAMAMLADI: veri_a == N ve veri_b == 2*N (yazıcı N tur yaptı,
 *       son çift tutarlı) ve seq == 2*N (her yazım 2 artırdı, tek kalmadı).
 *   (3) OKUYUCU GERÇEKTEN OKUDU: kabul_sayisi > 0 (okuyucu en az bir tutarlı
 *       okuma kabul etti → seqlock gerçekten sınandı, boş geçilmedi).
 *   Üçü de tutarsa "SMP SEQLOCK OK" + torn + kabul + retry sayısı basılır.
 *
 * NOT — determinizm: veri_a/veri_b'nin SON değerleri (N, 2*N), seq (2*N) ve
 * torn_read (0) HER koşuda aynıdır (seqlock doğruluğu zamanlamadan bağımsız).
 * `kabul_sayisi` (kaç tutarlı okuma) ve `retry_sayisi` (kaç kez yazım araya
 * girdi) zamanlamaya bağlı DEĞİŞEBİLİR — bu yüzden kabul yalnız > 0 kontrol
 * edilir, retry dürüstçe RAPORLANIR ama PASS koşulu değildir. Kritik invaryantlar
 * (torn=0, veri=N/2N, seq=2N) deterministik.
 *
 * KRİTİK — cache coherency (D-169/174/180/192/199 dersi): Çekirdek 1 MMU-OFF
 * (non-cacheable, doğrudan RAM); çekirdek 0 MMU-ON (Normal-WB cacheable). Bu
 * cacheability uyuşmazlığı donanım coherency'sini bozabilir. Bu yüzden:
 *   - seq ve veri_a/veri_b yazımlarını, satırı yazdıktan SONRA RAM'e boşaltarak
 *     (dc civac) çerçeveleriz; okumadan ÖNCE tazeleyerek (dc ivac). dmb ish
 *     mimari-seviye sıralamayı (seqlock KALBİ) sağlar; dc civac/ivac ise
 *     MMU-off/on cacheability uyuşmazlığında görünürlüğü sağlar.
 *   - ÖNEMLİ: reader'da seqlock sıra-doğrulaması ANLAMLI olsun diye seq ve
 *     veri_a/veri_b satırlarını her denemede TAZE oku (dc ivac) — aksi halde
 *     eski cache'ten stale seq + stale veri görüp YANLIŞ "tutarlı" raporlanabilir.
 *   - Her paylaşımlı sözcük AYRI 64-byte cache satırında (yanlış-paylaşım/klobber
 *     önlemi). veri_a ve veri_b de AYRI satırlarda (ama mantıksal çift).
 *
 * DETERMİNİSTİK: yazıcı sabit N=2000 tur (bounded); okuyucu bounded üst-sınıra
 * (< OKU_TIMEOUT_TIK) kadar yazıcı bitene dek çevirir. Tüm bekleme döngüleri
 * bounded. Yük/internet bağımsız — torn her koşuda 0, veri (N, 2*N), seq 2*N.
 */
#include <stdint.h>

extern void kdl_yazdir_metin(const char *);
extern void kdl_yaz_metin(const char *);
extern void kdl_yazdir_onaltilik(uint64_t);
extern void kdl_yaz_onaltilik(uint64_t);
extern void kdl_yazdir_isaretsiz_tam64(uint64_t);  /* satır-sonlu ondalık */

#define N_YAZ            2000ULL          /* yazıcının yapacağı seqlock-çift yazımı sayısı */
#define BEKLENEN_A       (N_YAZ)          /* yazıcı bitince veri_a == N */
#define BEKLENEN_B       (2ULL * N_YAZ)   /* yazıcı bitince veri_b == 2*N */
#define BEKLENEN_SEQ     (2ULL * N_YAZ)   /* her yazım seq'i 2 artırır → 2*N, çift (stabil) */
#define OKU_TIMEOUT_TIK   400000000ULL    /* okuyucu yazıcıyı beklerken toplam bounded tur sınırı */
#define OKU_ICEL_TIMEOUT  4000000ULL      /* tek okumanın optimistik retry üst-sınırı (bounded) */

/* --- Paylaşımlı seqlock durumu (her biri kendi 64-byte cache satırında,
 *     yanlış-paylaşım/klobber önlemi) --- */

/* Global sıra sayacı. ÇİFT = veri stabil, TEK = yazım sürüyor. Yazıcı her yazımı
 * seq++ (aç) / seq++ (kapat) ile çerçeveler → tam yazımda 2 artar. Okuyucu bu
 * sayacın okuma-öncesi ve okuma-sonrası DEĞİŞMEDİĞİNİ (ve çift olduğunu) görürse
 * okumayı tutarlı kabul eder. */
static volatile uint64_t seq __attribute__((aligned(64))) = 0;

/* --- Korunan "tutarlı çift": invaryant veri_b == veri_a * 2. Yazıcı ikisini
 *     seq-tek çerçevesi içinde tutarlı günceller; okuyucu seqlock ile invaryantı
 *     doğrular (torn-read tespiti). Ayrı satırlar (false-sharing önlemi) ama
 *     mantıksal olarak tek çift. --- */
static volatile uint64_t veri_a __attribute__((aligned(64))) = 0;
static volatile uint64_t veri_b __attribute__((aligned(64))) = 0;

/* --- Doğrulama sayaçları (yalnız okuyucu = çekirdek 1 yazar) --- */

/* Okuyucunun KABUL EDİLEN bir okumada tespit ettiği torn-read (yarım-yazılmış
 * çift) sayısı. Seqlock doğruysa == 0 (torn okuma zaten retry ile atılır, kabul
 * edilene ulaşmaz). Ayrı satır. */
static volatile uint64_t torn_read __attribute__((aligned(64))) = 0;

/* Okuyucunun KABUL ETTİĞİ (tutarlı) okuma sayısı (> 0 → seqlock sınandı). */
static volatile uint64_t kabul_sayisi __attribute__((aligned(64))) = 0;

/* Okuyucunun optimistik-okuma retry sayısı (yazım araya girip okumayı attırdı).
 * DÜRÜST rapor — kaç kez yazımla çakışıldığını gösterir. PASS koşulu DEĞİL. */
static volatile uint64_t retry_sayisi __attribute__((aligned(64))) = 0;

/* Çekirdek 1'in "başladım / hazırım" bayrağı (rendezvous — ayrı satır). */
static volatile uint64_t cekirdek1_basladi __attribute__((aligned(64))) = 0;

/* Çekirdek 1'in (okuyucu) "işim bitti" bayrağı (ayrı satır). */
static volatile uint64_t cekirdek1_bitti __attribute__((aligned(64))) = 0;

/* Çekirdek 1'in kendi yığını (çekirdek 0'ınkinden ayrı) — 8 KB.
 * NOT: naked trampoline yalnız asm'den (sembol adıyla) eriştiği için dış-bağ
 * (non-static) → linker sembolü emit eder (aksi halde "unused" atılır). */
uint8_t cekirdek1_yigin[8192] __attribute__((aligned(16)));

/* PSCI conduit çağrıları (smp_rwlock_arm.c / smp_atomic_arm.c ile aynı desen). */
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

/* --- LDAR/STLR ile seq oku/yaz: seqlock sıralamasını mimari-seviyede pekiştirir.
 *
 * seq okuma = acquire (LDAR): sonraki (veri) okumaları seq okumasından ÖNCEYE
 * kayamaz. seq yazma = release (STLR): önceki (veri) yazımları seq yazımından
 * SONRAYA kayamaz. dmb ish ile birlikte seqlock'un dört-sıralama KALBİNİ garanti
 * eder (LDAR/STLR + dmb ish redundant-güvenli; MMU-off/on'da fazladan güvence). */
static inline uint64_t seq_oku_acquire(void) {
    uint64_t v;
    __asm__ volatile("ldar %0, [%1]\n"
                     : "=r"(v)
                     : "r"(&seq)
                     : "memory");
    return v;
}

static inline void seq_yaz_release(uint64_t v) {
    __asm__ volatile("stlr %0, [%1]\n"
                     :
                     : "r"(v), "r"(&seq)
                     : "memory");
}

/* --- Yazıcı işi (çekirdek 0 koşar): N seqlock-çift yazımı — OKUYUCUYU BEKLEMEZ ---
 *
 * Her tur: seq++ (çift→TEK, yazım başladı) → dmb ish → veri_a++; veri_b=veri_a*2
 * → dmb ish → seq++ (tek→ÇİFT, yazım bitti). İki dmb ish seqlock'un KALBİDİR:
 * seq-tek yazımdan ÖNCE, seq-çift yazımdan SONRA görünür → okuyucu asla yarım
 * güncellenmiş çifti "tutarlı" göremez (görürse seq değişmiştir → retry).
 * Yazıcı hiçbir kilit almaz, hiçbir okuyucuyu beklemez → gecikmesiz. */
static void yazici_dongusu(void) {
    for (uint64_t k = 0; k < N_YAZ; k++) {
        /* --- 1. seq++ (çift→TEK): "yazım başladı" ilan et. --- */
        satir_gecersiz(&seq);
        uint64_t s = seq;
        seq_yaz_release(s + 1);          /* release: sonraki veri yazımları bundan sonra */
        satir_temizle_gecersiz(&seq);
        __asm__ volatile("dmb ish" ::: "memory");  /* seq-tek VERİ-yazımından ÖNCE (KALP) */

        /* --- 2. KORUNAN ÇİFTİ YAZ (seq-tek çerçevesinde). --- */
        satir_gecersiz(&veri_a);
        uint64_t yeni_a = veri_a + 1;
        veri_a = yeni_a;
        satir_temizle_gecersiz(&veri_a);
        veri_b = yeni_a * 2ULL;          /* invaryant: b == a*2 */
        satir_temizle_gecersiz(&veri_b);

        /* --- 3. seq++ (tek→ÇİFT): "yazım bitti, veri stabil" ilan et. --- */
        __asm__ volatile("dmb ish" ::: "memory");  /* VERİ-yazımı seq-çift'ten ÖNCE (KALP) */
        satir_gecersiz(&seq);
        s = seq;
        seq_yaz_release(s + 1);          /* release: veri yazımları bundan önce görünür */
        satir_temizle_gecersiz(&seq);
    }
}

/* --- Tek optimistik seqlock-okuma: (a,b) çiftini kilitsiz oku + tutarlılık doğrula.
 *
 * Dönüş: 1 = tutarlı okuma yapıldı (*out_a, *out_b geçerli); 0 = timeout (bozulma).
 * İç retry (bounded): yazım araya girerse (s0 tek / s1 != s0 / s1 tek) yeniden
 * dener + retry_sayisi++. Kilit ALMAZ, hiçbir şey YAZMAZ (retry sayacı hariç). */
static int seqlock_oku(uint64_t *out_a, uint64_t *out_b) {
    for (uint64_t deneme = 0; deneme < OKU_ICEL_TIMEOUT; deneme++) {
        /* --- 1. s0 = seq oku (acquire). TEK ise yazım sürüyor → retry. --- */
        satir_gecersiz(&seq);
        uint64_t s0 = seq_oku_acquire();
        if (s0 & 1ULL) {
            /* Yazım şu an sürüyor (seq tek) → optimistik okuma anlamsız, atla. */
            uint64_t r = retry_sayisi + 1;
            retry_sayisi = r;
            satir_temizle_gecersiz(&retry_sayisi);
            __asm__ volatile("yield" ::: "memory");
            continue;
        }
        __asm__ volatile("dmb ish" ::: "memory");  /* s0-okuması VERİ-okumasından ÖNCE (KALP) */

        /* --- 2. Çifti kilitsiz oku (araya yazım girebilir — seq ile yakalanır). --- */
        satir_gecersiz(&veri_a);
        satir_gecersiz(&veri_b);
        uint64_t a = veri_a;
        uint64_t b = veri_b;

        /* --- 3. dmb ish → s1 = seq tekrar oku (acquire). --- */
        __asm__ volatile("dmb ish" ::: "memory");  /* VERİ-okuması s1-okumasından ÖNCE (KALP) */
        satir_gecersiz(&seq);
        uint64_t s1 = seq_oku_acquire();

        /* --- 4. Tutarlılık: s1 == s0 (değişmedi) VE çift (yazım sürmüyor)? --- */
        if (s1 == s0) {
            /* seq okuma boyunca DEĞİŞMEDİ + s0 çift → (a,b) tek stabil çağdan,
             * torn DEĞİL. Kabul et. */
            *out_a = a;
            *out_b = b;
            return 1;
        }
        /* Okuma sırasında yazım araya girdi (s1 != s0) → okumayı at, yeniden dene. */
        uint64_t r = retry_sayisi + 1;
        retry_sayisi = r;
        satir_temizle_gecersiz(&retry_sayisi);
        __asm__ volatile("yield" ::: "memory");
    }
    return 0;  /* timeout */
}

/* --- Okuyucu işi (çekirdek 1 koşar): yazıcı bitene dek seqlock ile oku+doğrula ---
 *
 * Yazıcı (çekirdek 0) N tur bitene kadar bounded döngüde: seqlock-oku (optimistik
 * + tutarlılık) → KABUL EDİLEN okumada invaryant veri_b == veri_a*2 mi? Değilse
 * torn-read → torn_read++. Her kabul edilen okumada kabul_sayisi++.
 *
 * Yazıcının bittiğini cekirdek1'in KENDİSİ göremez (ayrı bayrak yok); bunun yerine
 * veri_a == N (yazıcı hedefine ulaştı) görülünce birkaç doğrulama turu daha atıp
 * çıkar. Bounded üst-sınır OKU_TIMEOUT_TIK garanti sonlandırma. */
static void okuyucu_dongusu(void) {
    uint64_t bitti_gozlem = 0;  /* veri_a==N görüldükten sonraki ek tur sayacı */
    for (uint64_t tur = 0; tur < OKU_TIMEOUT_TIK; tur++) {
        uint64_t a = 0;
        uint64_t b = 0;
        if (!seqlock_oku(&a, &b)) {
            break;  /* iç timeout — bozulma; dış doğrulama kabul_sayisi ile yakalar */
        }

        /* Torn-read tespiti: KABUL EDİLEN okumada invaryant b == a*2 tutmuyorsa
         * yarım-yazılmış çift kabul ettik → seqlock ihlali (olmamalı). */
        if (b != a * 2ULL) {
            uint64_t t = torn_read + 1;
            torn_read = t;
            satir_temizle_gecersiz(&torn_read);
        }

        uint64_t o = kabul_sayisi + 1;
        kabul_sayisi = o;
        satir_temizle_gecersiz(&kabul_sayisi);

        /* Yazıcı hedefine (veri_a==N) ulaştıysa birkaç ek doğrulama turu at, sonra
         * çık (bounded — yazıcı bitti, daha fazla okuma anlamsız). */
        if (a >= N_YAZ) {
            bitti_gozlem++;
            if (bitti_gozlem >= 64) {
                break;
            }
        }
    }
}

/*
 * Çekirdek 1 asıl işi (C) = OKUYUCU — SP zaten kurulu çağrılır (trampoline'den).
 * UART'a DOKUNMA (çekirdek 0 ile paylaşımlı) — yalnız paylaşılan RAM'e yaz.
 */
__attribute__((noreturn))
void cekirdek1_isi(void) {
    /* 1. RENDEZVOUS: "hazırım" bayrağını RAM'e set et. Çekirdek 0 bunu görene
     *    kadar yazmaya başlamaz → yazıcı ve okuyucu AYNI anda çalışır (gerçek
     *    seqlock çekişmesi; tek çekirdek sırayla bitirmez → optimistik-okuma ile
     *    yazım gerçekten üst üste biner). */
    __asm__ volatile("dsb sy" ::: "memory");
    cekirdek1_basladi = 1;
    satir_temizle_gecersiz(&cekirdek1_basladi);

    /* 2. GERÇEK İŞ: yazıcı bitene dek seqlock ile tutarlı-çifti oku + doğrula. */
    okuyucu_dongusu();

    /* 3. "Bittim" bayrağını en son set et (çekirdek 0 bunu poll eder). torn_read,
     *    kabul_sayisi, retry_sayisi yazımları RAM'e ULAŞTIKTAN sonra → çekirdek 0
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
    kdl_yazdir_metin("SMP SEQLOCK BASLA");

    /* --- 1. Çekirdek 1'i (OKUYUCU) başlat (PSCI CPU_ON, HVC→SMC fallback). --- */
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
        kdl_yazdir_metin("SMP SEQLOCK FAIL: CPU_ON basarisiz — 2. cekirdek baslamadi");
        for (;;) { __asm__ volatile("wfe"); }
    }

    /* --- 2. RENDEZVOUS: çekirdek 1'in (okuyucu) "hazırım" bayrağını bekle
     *        (bounded). Böylece yazıcı yazmaya GİRMEDEN önce okuyucu da hazır →
     *        yazıcı + okuyucu AYNI anda çalışır (gerçek seqlock çekişmesi; yazım
     *        ile optimistik-okuma üst üste biner). Bounded: bayrak gelmezse yine
     *        de devam et. --- */
    for (uint64_t bekle = 0; bekle < OKU_ICEL_TIMEOUT; bekle++) {
        satir_gecersiz(&cekirdek1_basladi);
        if (cekirdek1_basladi != 0) { break; }
    }

    /* --- 3. Çekirdek 0 = YAZICI: N seqlock-çift yazımı (okuyucuyu beklemez). --- */
    yazici_dongusu();

    /* --- 4. Okuyucunun (çekirdek 1) bitmesini bekle (bounded poll + coherent). --- */
    int bitti = 0;
    for (uint64_t bekle = 0; bekle < OKU_TIMEOUT_TIK; bekle++) {
        satir_gecersiz(&cekirdek1_bitti);
        if (cekirdek1_bitti != 0) { bitti = 1; break; }
    }

    /* --- 5. Sonuçları cache-coherent oku. --- */
    satir_gecersiz(&veri_a);
    uint64_t son_a = veri_a;
    satir_gecersiz(&veri_b);
    uint64_t son_b = veri_b;
    satir_gecersiz(&seq);
    uint64_t son_seq = seq;
    satir_gecersiz(&torn_read);
    uint64_t torn = torn_read;
    satir_gecersiz(&kabul_sayisi);
    uint64_t kabul = kabul_sayisi;
    satir_gecersiz(&retry_sayisi);
    uint64_t retry = retry_sayisi;

    if (!bitti) {
        kdl_yaz_metin("SMP SEQLOCK FAIL: okuyucu (cekirdek1) timeout — veri_a=");
        kdl_yazdir_isaretsiz_tam64(son_a);
        for (;;) { __asm__ volatile("wfe"); }
    }

    kdl_yaz_metin("veri_a=");
    kdl_yazdir_isaretsiz_tam64(son_a);
    kdl_yaz_metin("veri_b=");
    kdl_yazdir_isaretsiz_tam64(son_b);
    kdl_yaz_metin("seq=");
    kdl_yazdir_isaretsiz_tam64(son_seq);
    kdl_yaz_metin("torn_read=");
    kdl_yazdir_isaretsiz_tam64(torn);
    kdl_yaz_metin("kabul_sayisi=");
    kdl_yazdir_isaretsiz_tam64(kabul);
    kdl_yaz_metin("retry_sayisi=");
    kdl_yazdir_isaretsiz_tam64(retry);

    /* --- 6. DOĞRULAMA:
     *   (1) TORN-READ YOK: torn == 0 (seqlock hiç yarım-yazılmış çift kabul
     *       ettirmedi — optimistik-okuma tutarlı).
     *   (2) YAZICI TAMAMLADI: son_a == N ve son_b == 2*N ve son_seq == 2*N
     *       (her yazım seq'i 2 artırdı, çift kaldı — hiçbir yazım yarım kalmadı).
     *   (3) OKUYUCU GERÇEKTEN OKUDU: kabul > 0 (seqlock gerçekten sınandı).
     * torn > 0 çıkarsa gerçek torn sayısını bas + FAIL (sessiz-gizleme yok). --- */
    int torn_yok      = (torn == 0);
    int yazici_tamam  = (son_a == BEKLENEN_A) && (son_b == BEKLENEN_B) && (son_seq == BEKLENEN_SEQ);
    int okuyucu_okudu = (kabul > 0);

    if (torn_yok && yazici_tamam && okuyucu_okudu) {
        kdl_yaz_metin("SMP SEQLOCK OK torn=");
        kdl_yazdir_isaretsiz_tam64(torn);
        kdl_yaz_metin("kabul=");
        kdl_yazdir_isaretsiz_tam64(kabul);
        kdl_yaz_metin("retry=");
        kdl_yazdir_isaretsiz_tam64(retry);
    } else if (!torn_yok) {
        /* En önemli hata: seqlock torn-read kabul ettirdi → optimistik-okuma bozuk
         * (büyük olasılıkla dmb ish sıralaması / bariyer hatası). */
        kdl_yaz_metin("SMP SEQLOCK FAIL torn=");
        kdl_yazdir_isaretsiz_tam64(torn);
        kdl_yaz_metin("(seqlock yarim-yazilmis cift kabul ettirdi — bariyer/siralama hatasi) veri_a=");
        kdl_yazdir_isaretsiz_tam64(son_a);
        kdl_yaz_metin("veri_b=");
        kdl_yazdir_isaretsiz_tam64(son_b);
    } else if (!yazici_tamam) {
        kdl_yaz_metin("SMP SEQLOCK FAIL: yazici tamamlamadi veri_a=");
        kdl_yazdir_isaretsiz_tam64(son_a);
        kdl_yaz_metin("(beklenen ");
        kdl_yazdir_isaretsiz_tam64(BEKLENEN_A);
        kdl_yaz_metin(") veri_b=");
        kdl_yazdir_isaretsiz_tam64(son_b);
        kdl_yaz_metin("(beklenen ");
        kdl_yazdir_isaretsiz_tam64(BEKLENEN_B);
        kdl_yaz_metin(") seq=");
        kdl_yazdir_isaretsiz_tam64(son_seq);
        kdl_yaz_metin("(beklenen ");
        kdl_yazdir_isaretsiz_tam64(BEKLENEN_SEQ);
        kdl_yazdir_metin(")");
    } else {
        kdl_yazdir_metin("SMP SEQLOCK FAIL: okuyucu hic tutarli okuma kabul etmedi (kabul=0)");
    }

    for (;;) { __asm__ volatile("wfe"); }
}
