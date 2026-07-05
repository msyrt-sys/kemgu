/*
 * KEMGU-OS bare-metal SMP READER-WRITER LOCK (çoklu-okuyucu / tek-yazıcı) testi
 * (aarch64).
 * ===========================================================================
 *
 * Milestone (D-170/174/180/192 SMP + kilit üstünde): D-192 ticket-lock FIFO
 * ADALET kanıtladı ama o kilit HÂLÂ karşılıklı-dışlamalı — bir anda kritik
 * bölgede TEK çekirdek. Gerçek işletim sistemlerinde okuma çok, yazma az olduğu
 * için bu israftır: bir veriyi yalnız OKUYAN iki çekirdeğin birbirini dışlaması
 * gereksiz. Bu test bir kademe DAHA GÜÇLÜ eşzamanlılık kurar: READER-WRITER LOCK
 * (RW-lock) — birden çok OKUYUCU AYNI ANDA veriyi okuyabilir; ama bir YAZICI
 * EXCLUSIVE (tek başına, hiçbir okuyucu ve başka yazıcı yokken) girer.
 *
 * RW-lock çalışma prensibi (bu testte basit yazıcı-öncelikli değil, düz sayaç+
 * bayrak modeli):
 *   Durum:
 *     `okuyucu_sayisi` : şu an okuma-kilidini tutan okuyucu sayısı (atomik).
 *     `yazici_aktif`   : bir yazıcı kritik bölgede mi (0/1) — CAS ile exclusive.
 *
 *   oku_kilitle():   Yazıcı yokken okuyucu_sayisi'nı ATOMİK 1 artır. Ama artırma
 *                    ile yazıcı-yok kontrolü arasında yarış olmasın diye: önce
 *                    yazici_aktif==0 bekle, sonra atomik okuyucu_sayisi++, SONRA
 *                    tekrar yazici_aktif'e bak — bu arada yazıcı girdiyse geri çek
 *                    (okuyucu_sayisi--) ve baştan dene. Böylece "okuyucu içerideyken
 *                    yazıcı asla exclusive kazanamaz" invaryantı korunur.
 *   oku_ac():        okuyucu_sayisi'nı ATOMİK 1 azalt.
 *   yaz_kilitle():   Önce yazici_aktif'i 0→1 CAS ile EXCLUSIVE kap (iki yazıcı
 *                    olsa biri kazanır; burada tek yazıcı var ama desen doğru).
 *                    Sonra okuyucu_sayisi==0 olana kadar bekle (mevcut okuyucular
 *                    çıksın). Dönüşte hiç okuyucu yok + tek yazıcı biziz → exclusive.
 *   yaz_ac():        yazici_aktif=0 (sıradaki yazıcı/okuyucular girebilir).
 *
 * KARŞILIKLI-DIŞLAMA GARANTİSİ (RW-lock doğruluğu):
 *   - Yazıcı kritik bölgedeyken (yazici_aktif==1) hiçbir okuyucu içeri giremez
 *     (oku_kilitle yazici_aktif==0 bekler + geri-çekme yarış-kapısı).
 *   - Okuyucu(lar) içerideyken (okuyucu_sayisi>0) yazıcı exclusive kazanamaz
 *     (yaz_kilitle okuyucu_sayisi==0 bekler).
 *   → Yazıcının "tutarlı çift" yazımı (a, sonra b=a*2) hiçbir okuyucuyla
 *     çakışmaz → okuyucu ASLA yarım-yazılmış çift (torn-read) görmez.
 *
 * KORUNAN VERİ — "tutarlı çift" invaryantı: paylaşımlı `veri_a` ve `veri_b`,
 * invaryant her zaman `veri_b == veri_a * 2`. Yazıcı (çekirdek 0) yaz-kilit
 * altında önce veri_a'yı artırır SONRA veri_b = veri_a * 2 yazar; iki yazım
 * arasında kilit sayesinde hiçbir okuyucu araya giremez. Okuyucu (çekirdek 1)
 * oku-kilit altında veri_a ve veri_b'yi okur ve `veri_b == veri_a*2` mi diye
 * DOĞRULAR. RW-lock doğruysa bu invaryant HER okumada tutar → torn_read == 0.
 *
 * İki çekirdek:
 *   Çekirdek 0 = YAZICI: N=1000 kez yaz-kilit → veri_a++ + veri_b=veri_a*2 →
 *                yaz-aç. Bittiğinde veri_a == N, veri_b == 2*N.
 *   Çekirdek 1 = OKUYUCU: yazıcı bitene kadar (bounded) tekrar tekrar oku-kilit →
 *                (veri_a, veri_b) oku + invaryant doğrula (torn-read say) → oku-aç.
 *                Her okumada torn-read tespit edilirse torn_read++.
 *
 * DOĞRULAMA (deterministik-ISH — SMP subtle):
 *   (1) TORN-READ YOK: torn_read == 0 olmalı (RW-lock hiçbir yarım-yazılmış çift
 *       sızdırmadı). torn_read > 0 çıkarsa "SMP RWLOCK FAIL torn=N" — SESSİZ
 *       GİZLEME YOK, gerçek torn sayısı basılır.
 *   (2) YAZICI TAMAMLADI: veri_a == N ve veri_b == 2*N (yazıcı N tur yaptı,
 *       son çift tutarlı).
 *   (3) OKUYUCU GERÇEKTEN OKUDU: okuma_sayisi > 0 (okuyucu en az bir kez oku-kilit
 *       aldı → RW-lock gerçekten sınandı, boş geçilmedi).
 *   Üçü de tutarsa "SMP RWLOCK OK" + torn sayısı + okuma sayısı basılır.
 *
 * NOT — determinizm: veri_a/veri_b'nin SON değerleri (N, 2*N) ve torn_read (0)
 * HER koşuda aynıdır (RW-lock doğruluğu zamanlamadan bağımsız). `okuma_sayisi`
 * (okuyucunun kaç tur çevirdiği) zamanlamaya bağlı DEĞİŞEBİLİR (yazıcı ne kadar
 * sürerse okuyucu o kadar tur atar) — bu yüzden yalnız > 0 kontrol edilir, kesin
 * değeri PASS koşulu değildir. Kritik invaryantlar (torn=0, veri=N/2N) determinist.
 *
 * KRİTİK — cache coherency (D-170/174/180/192 dersi): Çekirdek 1 MMU-OFF (non-
 * cacheable, doğrudan RAM); çekirdek 0 MMU-ON (Normal-WB cacheable). Bu
 * cacheability uyuşmazlığı donanım coherency'sini ve exclusive-monitor
 * davranışını bozabilir. Bu yüzden:
 *   - `okuyucu_sayisi` ve `yazici_aktif` atomik RMW'leri, satırı her denemeden
 *     ÖNCE tazeleyerek (dc ivac) ve yazdıktan SONRA RAM'e boşaltarak (dc civac)
 *     çerçeveleriz; dmb ish RMW etrafında sıralar.
 *   - Poll okumalarında (yazici_aktif / okuyucu_sayisi bekleme) her okumadan önce
 *     dc ivac (diğer çekirdeğin yazımını gör).
 *   - Korunan çift (veri_a, veri_b): yazıcı yazdıktan sonra dc civac (RAM'e
 *     boşalt); okuyucu okumadan önce dc ivac (taze oku). ÖNEMLİ: torn-read testinin
 *     ANLAMLI olması için iki alanı da tazele — aksi halde eski b + yeni a görüp
 *     YANLIŞ torn raporlanabilir; coherency çerçevesi bunu engeller (torn yalnız
 *     GERÇEK kilit ihlalinden gelir).
 *   - Her paylaşımlı sözcük AYRI 64-byte cache satırında (yanlış-paylaşım/klobber
 *     önlemi). veri_a ve veri_b de AYRI satırlarda (ama mantıksal çift).
 *
 * DETERMİNİSTİK: yazıcı sabit N=1000 tur (bounded); okuyucu bounded üst-sınıra
 * (< OKU_TIMEOUT_TIK) kadar yazıcı bitene dek çevirir. Tüm bekleme döngüleri
 * bounded (< KILIT_TIMEOUT_TIK). Yük/internet bağımsız — torn her koşuda 0,
 * veri her koşuda (N, 2*N).
 */
#include <stdint.h>

extern void kdl_yazdir_metin(const char *);
extern void kdl_yaz_metin(const char *);
extern void kdl_yazdir_onaltilik(uint64_t);
extern void kdl_yaz_onaltilik(uint64_t);
extern void kdl_yazdir_isaretsiz_tam64(uint64_t);  /* satır-sonlu ondalık */

#define N_YAZ            1000ULL          /* yazıcının yapacağı tutarlı-çift yazımı sayısı */
#define BEKLENEN_A       (N_YAZ)          /* yazıcı bitince veri_a == N */
#define BEKLENEN_B       (2ULL * N_YAZ)   /* yazıcı bitince veri_b == 2*N */
#define KILIT_TIMEOUT_TIK 80000000ULL     /* kilit-bekleme bounded üst-sınır (yük-bağımsız) */
#define OKU_TIMEOUT_TIK   400000000ULL    /* okuyucu yazıcıyı beklerken toplam bounded tur sınırı */

/* --- Paylaşımlı RW-lock durumu (her biri kendi 64-byte cache satırında,
 *     yanlış-paylaşım/klobber önlemi) --- */

/* Şu an okuma-kilidini tutan okuyucu sayısı. oku_kilitle atomik artırır,
 * oku_ac atomik azaltır. yaz_kilitle bunun 0 olmasını bekler. */
static volatile uint64_t okuyucu_sayisi __attribute__((aligned(64))) = 0;

/* Bir yazıcı kritik bölgede mi (0/1). yaz_kilitle 0→1 CAS ile exclusive kapar;
 * yaz_ac 0'a çeker. oku_kilitle bunun 0 olmasını bekler (yazıcıyla çakışma yok). */
static volatile uint64_t yazici_aktif __attribute__((aligned(64))) = 0;

/* --- Korunan "tutarlı çift": invaryant veri_b == veri_a * 2. Yazıcı ikisini
 *     yaz-kilit altında tutarlı günceller; okuyucu oku-kilit altında invaryantı
 *     doğrular (torn-read tespiti). Ayrı satırlar (false-sharing önlemi) ama
 *     mantıksal olarak tek çift. --- */
static volatile uint64_t veri_a __attribute__((aligned(64))) = 0;
static volatile uint64_t veri_b __attribute__((aligned(64))) = 0;

/* --- Doğrulama sayaçları (yalnız okuyucu = çekirdek 1 yazar) --- */

/* Okuyucunun tespit ettiği torn-read (yarım-yazılmış çift) sayısı. RW-lock
 * doğruysa == 0. Ayrı satır. */
static volatile uint64_t torn_read __attribute__((aligned(64))) = 0;

/* Okuyucunun tamamladığı oku-kilit turu sayısı (> 0 → RW-lock gerçekten sınandı). */
static volatile uint64_t okuma_sayisi __attribute__((aligned(64))) = 0;

/* Çekirdek 1'in "başladım / hazırım" bayrağı (rendezvous — ayrı satır). */
static volatile uint64_t cekirdek1_basladi __attribute__((aligned(64))) = 0;

/* Çekirdek 1'in (okuyucu) "işim bitti" bayrağı (ayrı satır). */
static volatile uint64_t cekirdek1_bitti __attribute__((aligned(64))) = 0;

/* Çekirdek 1'in kendi yığını (çekirdek 0'ınkinden ayrı) — 8 KB.
 * NOT: naked trampoline yalnız asm'den (sembol adıyla) eriştiği için dış-bağ
 * (non-static) → linker sembolü emit eder (aksi halde "unused" atılır). */
uint8_t cekirdek1_yigin[8192] __attribute__((aligned(16)));

/* PSCI conduit çağrıları (smp_ticket_arm.c / smp_atomic_arm.c ile aynı desen). */
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

/* --- ATOMİK fetch-add: aarch64 LDXR/STXR RMW retry döngüsü ---
 *
 * *adr'yi atomik olarak `delta` kadar (delta = +1 veya (uint64_t)-1 = azalt)
 * değiştirir ve ARTIRMADAN ÖNCEKİ değeri döndürür. LDXR exclusive yükler; STXR
 * yalnız satır bu okumadan beri başkasınca yazılmadıysa başarır (w=0). Rakip
 * çekirdek araya girerse STXR fail (w=1) → cbnz ile taze değerle retry.
 *
 * MMU-off/on cacheability uyuşmazlığında: her RMW denemesinden ÖNCE satırı
 * tazele (dc ivac), başarılı yazımdan SONRA RAM'e boşalt (dc civac). dmb ish
 * RMW etrafında sıralar. */
static inline uint64_t atomik_fetch_add(volatile uint64_t *adr, uint64_t delta) {
    uint64_t eski;
    uint64_t yeni;
    uint32_t basarisiz;
    do {
        satir_gecersiz(adr);
        __asm__ volatile(
            "dmb    ish\n"
            "ldxr   %0, [%3]\n"        /* eski = *adr (exclusive) */
            "add    %1, %0, %4\n"      /* yeni = eski + delta */
            "stxr   %w2, %1, [%3]\n"   /* *adr = yeni dene; basarisiz=0 OK */
            "dmb    ish\n"
            : "=&r"(eski), "=&r"(yeni), "=&r"(basarisiz)
            : "r"(adr), "r"(delta)
            : "memory");
        if (basarisiz == 0) {
            satir_temizle_gecersiz(adr);
            return eski;
        }
        /* STXR fail (rakip araya girdi) → taze değerle yeniden dene. */
    } while (1);
}

/* --- ATOMİK CAS: *adr == beklenen ise yeni yaz. Dönüş: 1 başarı, 0 fail ---
 *
 * yaz_kilitle'de yazici_aktif'i 0→1 exclusive kapmak için. LDXR exclusive
 * yükler; mevcut değer beklenene EŞİT DEĞİLSE STXR denemeden çık (fail=0-dönüş).
 * Eşitse STXR ile yeni yaz; STXR fail (rakip) → retry. Coherency çerçevesi
 * fetch-add ile aynı. */
static inline int atomik_cas(volatile uint64_t *adr, uint64_t beklenen, uint64_t yeni) {
    uint64_t okunan;
    uint32_t basarisiz;
    do {
        satir_gecersiz(adr);
        __asm__ volatile(
            "dmb    ish\n"
            "ldxr   %0, [%3]\n"        /* okunan = *adr (exclusive) */
            "cmp    %0, %4\n"          /* okunan == beklenen? */
            "b.ne   1f\n"              /* değilse: exclusive'i bırakıp CAS-fail dön */
            "stxr   %w1, %5, [%3]\n"   /* eşit: *adr = yeni dene; basarisiz=0 OK */
            "dmb    ish\n"
            "b      2f\n"
            "1:\n"
            "clrex\n"                  /* exclusive monitor'ı temizle (STXR yapmadık) */
            "mov    %w1, #1\n"         /* basarisiz=1 → beklenen tutmadı (CAS-fail) */
            "2:\n"
            : "=&r"(okunan), "=&r"(basarisiz)
            : "r"(adr), "r"(adr), "r"(beklenen), "r"(yeni)
            : "cc", "memory");
        /* basarisiz==0: STXR başarılı → CAS başardı.
         * basarisiz==1: ya beklenen tutmadı (okunan!=beklenen, kalıcı fail) ya da
         *               STXR rakip yüzünden fail (retry gerek). Ayırt et: okunan
         *               hâlâ beklenene eşitse STXR-fail'di → retry; değilse gerçek
         *               CAS-fail → 0 dön. */
        if (basarisiz == 0) {
            satir_temizle_gecersiz(adr);
            return 1;  /* CAS başarılı */
        }
        satir_gecersiz(adr);
        if (okunan != beklenen) {
            return 0;  /* değer beklenen değildi → gerçek CAS-fail (retry etme) */
        }
        /* okunan==beklenen ama STXR fail → rakip araya girdi, yeniden dene. */
    } while (1);
}

/* --- oku_kilitle: yazıcı yokken okuma-kilidini al (birden çok okuyucu paralel) ---
 *
 * Adım 1: yazici_aktif==0 olana kadar bekle (bounded).
 * Adım 2: okuyucu_sayisi'nı atomik 1 artır (kilidi "iddia et").
 * Adım 3: TEKRAR yazici_aktif'e bak — biz artırırken bir yazıcı 0→1 kapmış
 *         olabilir (yarış). Kapmışsa okuyucu_sayisi'nı geri çek (atomik --) ve
 *         Adım 1'e dön. Bu geri-çekme kapısı "okuyucu içerideyken yazıcı exclusive
 *         kazanamaz" invaryantını kesinleştirir: yazıcı yaz_kilitle'de
 *         okuyucu_sayisi>0 görürse bekler; biz de yazıcıyı görürsek çekiliriz →
 *         ikisi aynı anda içeride olamaz.
 * Dönüş: 1 = kilit alındı, 0 = timeout (bozulma). */
static inline int oku_kilitle(void) {
    for (uint64_t dis_bekle = 0; dis_bekle < KILIT_TIMEOUT_TIK; dis_bekle++) {
        /* Adım 1: yazıcı yokken devam et. */
        satir_gecersiz(&yazici_aktif);
        if (yazici_aktif != 0) {
            __asm__ volatile("yield" ::: "memory");
            continue;
        }
        /* Adım 2: okuma-kilidini iddia et (atomik ++). */
        atomik_fetch_add(&okuyucu_sayisi, 1);
        /* Adım 3: biz artırırken yazıcı girdi mi? Tekrar bak. */
        satir_gecersiz(&yazici_aktif);
        if (yazici_aktif == 0) {
            /* Temiz: yazıcı yok, okuma-kilidi bizde. acquire bariyeri. */
            __asm__ volatile("dmb ish" ::: "memory");
            return 1;
        }
        /* Yazıcı araya girdi → geri çekil (atomik --) ve baştan dene. */
        atomik_fetch_add(&okuyucu_sayisi, (uint64_t)-1);
        __asm__ volatile("yield" ::: "memory");
    }
    return 0;  /* timeout */
}

/* --- oku_ac: okuma-kilidini bırak (okuyucu_sayisi atomik --) --- */
static inline void oku_ac(void) {
    __asm__ volatile("dmb ish" ::: "memory");   /* release: kritik-bölge okumaları önce */
    atomik_fetch_add(&okuyucu_sayisi, (uint64_t)-1);
}

/* --- yaz_kilitle: EXCLUSIVE yazma-kilidini al (tek yazıcı, hiç okuyucu yok) ---
 *
 * Adım 1: yazici_aktif'i 0→1 CAS ile kap (iki yazıcı olsa biri kazanır). Başka
 *         yazıcı tutuyorsa CAS-fail → bekle, yeniden dene (bounded).
 * Adım 2: yazici_aktif bizde; şimdi mevcut okuyucular çıksın: okuyucu_sayisi==0
 *         olana kadar bekle (bounded). Bu arada yeni okuyucu giremez çünkü
 *         yazici_aktif==1 (oku_kilitle bunu görür + geri-çekilir).
 * Dönüş: 1 = exclusive kilit alındı, 0 = timeout (bozulma). */
static inline int yaz_kilitle(void) {
    /* Adım 1: yazici_aktif 0→1 exclusive kap. */
    int kapildi = 0;
    for (uint64_t bekle = 0; bekle < KILIT_TIMEOUT_TIK; bekle++) {
        if (atomik_cas(&yazici_aktif, 0, 1)) {
            kapildi = 1;
            break;
        }
        __asm__ volatile("yield" ::: "memory");
    }
    if (!kapildi) {
        return 0;  /* timeout: yazıcı bayrağı kapılamadı */
    }
    /* Adım 2: mevcut okuyucular çıkana kadar bekle. */
    for (uint64_t bekle = 0; bekle < KILIT_TIMEOUT_TIK; bekle++) {
        satir_gecersiz(&okuyucu_sayisi);
        if (okuyucu_sayisi == 0) {
            /* Hiç okuyucu yok + yazıcı biziz → exclusive. acquire bariyeri. */
            __asm__ volatile("dmb ish" ::: "memory");
            return 1;
        }
        __asm__ volatile("yield" ::: "memory");
    }
    /* Timeout: okuyucular çıkmadı → bayrağı geri bırak (deadlock önle) + fail. */
    __asm__ volatile("dmb ish" ::: "memory");
    yazici_aktif = 0;
    satir_temizle_gecersiz(&yazici_aktif);
    return 0;
}

/* --- yaz_ac: exclusive yazma-kilidini bırak (yazici_aktif=0) ---
 *
 * dmb ish (release) kritik-bölge (çift) yazımlarının bu bırakıştan ÖNCE görünmesini
 * garanti eder; dc civac + dsb yazici_aktif=0'ı RAM'e boşaltır ki bekleyen okuyucu/
 * yazıcı görsün. */
static inline void yaz_ac(void) {
    __asm__ volatile("dmb ish" ::: "memory");   /* release: çift yazımları önce görünsün */
    yazici_aktif = 0;
    satir_temizle_gecersiz(&yazici_aktif);
}

/* --- Yazıcı işi (çekirdek 0 koşar): N tutarlı-çift yazımı ---
 *
 * Her tur yaz-kilit altında önce veri_a'yı artır SONRA veri_b = veri_a*2 yaz.
 * İki yazım arasında kilit hiçbir okuyucuya yer vermez → okuyucu asla yarım
 * güncellenmiş çift görmez. Kilit alınamazsa (timeout) döngüyü keser → dış
 * doğrulama veri_a<N görüp yakalar (sessiz-gizleme yok). */
static void yazici_dongusu(void) {
    for (uint64_t k = 0; k < N_YAZ; k++) {
        if (!yaz_kilitle()) {
            break;  /* timeout — dış doğrulama veri_a<N ile yakalar */
        }
        /* --- YAZMA KRİTİK BÖLGESİ (exclusive) --- */
        satir_gecersiz(&veri_a);
        uint64_t yeni_a = veri_a + 1;
        veri_a = yeni_a;
        satir_temizle_gecersiz(&veri_a);
        /* Tutarlı ikinci yarı: b = a*2 (kilit araya okuyucu sokmaz). */
        veri_b = yeni_a * 2ULL;
        satir_temizle_gecersiz(&veri_b);
        /* --- YAZMA KRİTİK BÖLGESİ SONU --- */
        yaz_ac();
    }
}

/* --- Okuyucu işi (çekirdek 1 koşar): yazıcı bitene dek tutarlı-çift oku+doğrula ---
 *
 * Yazıcı (çekirdek 0) N tur bitene kadar bounded döngüde: oku-kilit al →
 * (veri_a, veri_b) TAZE oku → invaryant veri_b == veri_a*2 mi? Değilse torn-read
 * (yarım-yazılmış çift) → torn_read++. → oku-aç. Her tur okuma_sayisi++.
 *
 * Yazıcının bittiğini cekirdek1'in KENDİSİ göremez (ayrı bayrak yok); bunun yerine
 * veri_a == N (yazıcı hedefine ulaştı) görülünce birkaç doğrulama turu daha atıp
 * çıkar. Bounded üst-sınır OKU_TIMEOUT_TIK garanti sonlandırma. */
static void okuyucu_dongusu(void) {
    uint64_t bitti_gozlem = 0;  /* veri_a==N görüldükten sonraki ek tur sayacı */
    for (uint64_t tur = 0; tur < OKU_TIMEOUT_TIK; tur++) {
        if (!oku_kilitle()) {
            break;  /* kilit timeout — bozulma; dış doğrulama okuma_sayisi ile yakalar */
        }
        /* --- OKUMA KRİTİK BÖLGESİ (paylaşımlı; başka okuyucu da olabilir) --- */
        satir_gecersiz(&veri_a);
        satir_gecersiz(&veri_b);
        uint64_t a = veri_a;
        uint64_t b = veri_b;
        /* --- OKUMA KRİTİK BÖLGESİ SONU --- */
        oku_ac();

        /* Torn-read tespiti: invaryant b == a*2 tutmuyorsa yarım-yazılmış çift
         * gördük → RW-lock ihlali. */
        if (b != a * 2ULL) {
            uint64_t t = torn_read + 1;
            torn_read = t;
            satir_temizle_gecersiz(&torn_read);
        }

        uint64_t o = okuma_sayisi + 1;
        okuma_sayisi = o;
        satir_temizle_gecersiz(&okuma_sayisi);

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
     *    kadar yazmaya başlamaz → yazıcı ve okuyucu AYNI anda kilit için yarışır
     *    (gerçek RW-lock çekişmesi; tek çekirdek sırayla bitirmez). */
    __asm__ volatile("dsb sy" ::: "memory");
    cekirdek1_basladi = 1;
    satir_temizle_gecersiz(&cekirdek1_basladi);

    /* 2. GERÇEK İŞ: yazıcı bitene dek tutarlı-çifti oku + torn-read doğrula. */
    okuyucu_dongusu();

    /* 3. "Bittim" bayrağını en son set et (çekirdek 0 bunu poll eder). torn_read
     *    ve okuma_sayisi yazımları RAM'e ULAŞTIKTAN sonra → çekirdek 0 bayrağı
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
    kdl_yazdir_metin("SMP RWLOCK BASLA");

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
        kdl_yazdir_metin("SMP RWLOCK FAIL: CPU_ON basarisiz — 2. cekirdek baslamadi");
        for (;;) { __asm__ volatile("wfe"); }
    }

    /* --- 2. RENDEZVOUS: çekirdek 1'in (okuyucu) "hazırım" bayrağını bekle
     *        (bounded). Böylece yazıcı yazmaya GİRMEDEN önce okuyucu da yarışa
     *        hazır → yazıcı + okuyucu AYNI anda kilit için çekişir (gerçek RW-lock
     *        çekişmesi). Bounded: bayrak gelmezse yine de devam et. --- */
    for (uint64_t bekle = 0; bekle < KILIT_TIMEOUT_TIK; bekle++) {
        satir_gecersiz(&cekirdek1_basladi);
        if (cekirdek1_basladi != 0) { break; }
    }

    /* --- 3. Çekirdek 0 = YAZICI: N tutarlı-çift yazımı yaz-kilit altında. --- */
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
    satir_gecersiz(&torn_read);
    uint64_t torn = torn_read;
    satir_gecersiz(&okuma_sayisi);
    uint64_t okuma = okuma_sayisi;

    if (!bitti) {
        kdl_yaz_metin("SMP RWLOCK FAIL: okuyucu (cekirdek1) timeout — veri_a=");
        kdl_yazdir_isaretsiz_tam64(son_a);
        for (;;) { __asm__ volatile("wfe"); }
    }

    kdl_yaz_metin("veri_a=");
    kdl_yazdir_isaretsiz_tam64(son_a);
    kdl_yaz_metin("veri_b=");
    kdl_yazdir_isaretsiz_tam64(son_b);
    kdl_yaz_metin("torn_read=");
    kdl_yazdir_isaretsiz_tam64(torn);
    kdl_yaz_metin("okuma_sayisi=");
    kdl_yazdir_isaretsiz_tam64(okuma);

    /* --- 6. DOĞRULAMA:
     *   (1) TORN-READ YOK: torn == 0 (RW-lock hiç yarım-yazılmış çift sızdırmadı).
     *   (2) YAZICI TAMAMLADI: son_a == N ve son_b == 2*N.
     *   (3) OKUYUCU GERÇEKTEN OKUDU: okuma > 0 (RW-lock gerçekten sınandı).
     * torn > 0 çıkarsa gerçek torn sayısını bas + FAIL (sessiz-gizleme yok). --- */
    int torn_yok      = (torn == 0);
    int yazici_tamam  = (son_a == BEKLENEN_A) && (son_b == BEKLENEN_B);
    int okuyucu_okudu = (okuma > 0);

    if (torn_yok && yazici_tamam && okuyucu_okudu) {
        kdl_yaz_metin("SMP RWLOCK OK torn=");
        kdl_yazdir_isaretsiz_tam64(torn);
        kdl_yaz_metin("okuma=");
        kdl_yazdir_isaretsiz_tam64(okuma);
    } else if (!torn_yok) {
        /* En önemli hata: RW-lock torn-read sızdırdı → karşılıklı-dışlama ihlali. */
        kdl_yaz_metin("SMP RWLOCK FAIL torn=");
        kdl_yazdir_isaretsiz_tam64(torn);
        kdl_yaz_metin("(RW-lock yarim-yazilmis cift sizdirdi — karsilikli-dislama ihlali) veri_a=");
        kdl_yazdir_isaretsiz_tam64(son_a);
        kdl_yaz_metin("veri_b=");
        kdl_yazdir_isaretsiz_tam64(son_b);
    } else if (!yazici_tamam) {
        kdl_yaz_metin("SMP RWLOCK FAIL: yazici tamamlamadi veri_a=");
        kdl_yazdir_isaretsiz_tam64(son_a);
        kdl_yaz_metin("(beklenen ");
        kdl_yazdir_isaretsiz_tam64(BEKLENEN_A);
        kdl_yaz_metin(") veri_b=");
        kdl_yazdir_isaretsiz_tam64(son_b);
        kdl_yaz_metin("(beklenen ");
        kdl_yazdir_isaretsiz_tam64(BEKLENEN_B);
        kdl_yazdir_metin(")");
    } else {
        kdl_yazdir_metin("SMP RWLOCK FAIL: okuyucu hic oku-kilit almadi (okuma=0)");
    }

    for (;;) { __asm__ volatile("wfe"); }
}
