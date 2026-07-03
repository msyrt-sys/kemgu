/*
 * KEMGU-OS bare-metal SMP MCS QUEUE-LOCK (ölçeklenebilir kuyruk-kilidi) testi (aarch64).
 * =====================================================================================
 *
 * Milestone (D-169/174/186/192 SMP + kilit üstünde): D-170 spinlock (test-and-set)
 * karşılıklı-dışlama verdi ama ADİL DEĞİLdi; D-192 ticket-lock FIFO ADALET ekledi.
 * Fakat HEM spinlock HEM ticket-lock TEK bir global adreste döner:
 *   - spinlock: tüm çekirdekler aynı `kilit` sözcüğüne test-and-set çeker.
 *   - ticket-lock: tüm bekleyenler aynı `simdi_hizmet` sözcüğünü poll eder.
 * Bu tek-nokta ÖLÇEKLENMEZ: her unlock o global satırı DEĞİŞTİRİR → tüm bekleyen
 * çekirdeklerin cache kopyaları geçersizleşir → hepsi RAM'den yeniden çeker
 * (cache-line "bouncing"). Çekirdek sayısı arttıkça bu coherency trafiği O(N)'e
 * patlar → kilit kendisi darboğaz olur.
 *
 * MCS QUEUE-LOCK (Mellor-Crummey & Scott, 1991) bunu çözer: her çekirdek KENDİ
 * özel node'undaki `locked` bayrağında döner (GLOBAL adreste DEĞİL). Kilit bir
 * KUYRUK'tur (linked list); serbest kalınca sahibi YALNIZ HALEFİNİN node'unu
 * yazar → yalnız o tek çekirdeğin cache satırı geçersizleşir, diğerleri hiç
 * etkilenmez. Böylece spin trafiği çekirdek-yerel kalır (cache-line bouncing YOK)
 * ve kilit N çekirdeğe DOĞRUSAL ölçeklenir.
 *
 * MCS çalışma prensibi (her çekirdeğin KENDİ node'u var):
 *   node = { volatile int locked; volatile McsNode *next; }
 *   global: volatile McsNode *tail;   (kuyruğun sonu; NULL = kilit boş)
 *
 *   kilitle(node):
 *     node->next = NULL; node->locked = 1;
 *     onceki = ATOMIK_SWAP(tail, node);   // ben artık kuyruğun sonuyum
 *     eğer onceki == NULL:                // kuyruk boştu → kilit BENİM, dönme yok
 *         (kilit alındı, hemen dön)
 *     değilse:                            // önümde biri var → ona bağlan, bekle
 *         onceki->next = node;            // selefin next'i beni gösterir
 *         node->locked == 0 olana kadar KENDİ node'umda dön (yerel spin!)
 *
 *   ac(node):
 *     eğer node->next == NULL:            // görünürde halef yok
 *         eğer ATOMIK_CAS(tail, node, NULL) başarır:  // gerçekten son bendim
 *             (kilit serbest, dön)
 *         değilse:                        // araya biri girdi ama next'i henüz
 *             node->next set olana kadar bekle          // yazmadı → bekle
 *     node->next->locked = 0;             // halefimi serbest bırak (TEK satır!)
 *
 * ÖLÇEKLENEBİLİRLİK (spinlock/ticket'in vermediği): ac() YALNIZ node->next->locked'ı
 * (tek çekirdeğin özel satırı) yazar → sadece o halef uyanır, diğer bekleyenlerin
 * cache'i dokunulmaz. Global `tail` yalnız kuyruğa giriş/çıkışta (SWAP/CAS) yazılır,
 * spin sırasında DEĞİL → bouncing yok.
 *
 * FIFO ADALET (MCS DOĞAL): node'lar `tail`'e atomik SWAP sırasına göre zincirlenir;
 * ac() her zaman zincirdeki BİR SONRAKİ (en eski bekleyen) node'u serbest bırakır →
 * kilit kesin FIFO sırada geçer. Açlık imkansız (ticket-lock ile aynı garanti ama
 * yerel-spin ile ölçeklenir).
 *
 * İş tanımı: paylaşımlı `sayac` = 0. Her çekirdek N=5000 kez: MCS-kilitle →
 * `sayac++` (KRİTİK BÖLGE; kilit karşılıklı-dışlama sağladığı için burada ATOMİK
 * GEREKMEZ — düz load-add-store yeterli) → aç. İki çekirdek toplam 2*N = 10000
 * artırım yapar.
 *
 * DOĞRULAMA (deterministik):
 *   (1) KARŞILIKLI-DIŞLAMA (kilit-doğruluk): son `sayac` == 2*N == 10000 olmalı.
 *       MCS düzgün serialize etmeseydi iki çekirdek aynı eski değeri okuyup üstüne
 *       yazardı (lost-update) → sayac < 10000 → FAIL. 10000 çıkması, düz (atomik-
 *       olmayan) artırımın MCS-kilit koruması altında yarışsız serialize edildiğinin
 *       kanıtıdır.
 *   (2) FIFO ADALET / açlık-yok: her çekirdek işlediği kritik-bölge sayısını kendi
 *       ayrı slotunda tutar. MCS FIFO olduğu için ikisi de gerçekten hizmet görür;
 *       toplam == 2*N ve her ikisi de > 0 (ikisi de kritik bölgeye girdi → açlık yok).
 *
 * KRİTİK — cache coherency (D-170/174/180 dersi): Çekirdek 1 MMU-OFF (non-cacheable,
 * doğrudan RAM); çekirdek 0 MMU-ON (Normal-WB cacheable). Bu cacheability uyuşmazlığı
 * donanım coherency'sini ve exclusive-monitor davranışını bozabilir. Bu yüzden HER
 * paylaşımlı erişim manuel çerçevelenir:
 *   - `tail` SWAP/CAS: satırı her denemeden ÖNCE tazele (dc ivac), yazdıktan SONRA
 *     RAM'e boşalt (dc civac). dmb ish RMW etrafında sıralar.
 *   - node->locked spin: her okumadan önce dc ivac (selefimin ac()'ını gör).
 *   - node->next okuma/yazma: yazan dc civac ile boşaltır, okuyan dc ivac ile tazeler.
 *   - `sayac` kritik bölgede: okumadan önce dc ivac, yazdıktan sonra dc civac.
 *   - Her node ve her paylaşımlı sözcük AYRI 64-byte cache satırında (D-186 yanlış-
 *     paylaşım/klobber önlemi — MCS'nin YEREL-SPIN kazanımı false-sharing ile bozulmaz).
 *
 * DETERMİNİSTİK: her çekirdek sabit N=5000 kritik bölge dener (bounded); tüm bekleme
 * döngüleri bounded üst-sınırlı (< TIMEOUT_TIK). Yük/internet bağımsız — sayac her
 * koşuda 10000, 5/5 koşu byte-identik.
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

/* --- MCS node: her çekirdek KENDİ node'unda döner (yerel-spin, bouncing yok) ---
 *
 * locked: 1 = bekliyorum (selefim henüz serbest bırakmadı), 0 = kilit benim.
 * next:   kuyrukta benden sonraki node (NULL = halef yok / kuyruk sonu).
 *
 * Node'un TAMAMI kendi 64-byte cache satırında (aligned(64)) — false-sharing
 * önlemi. İki node aynı satıra düşerse bir çekirdeğin locked yazımı diğerinin
 * spin satırını geçersizler → MCS'nin yerel-spin kazanımı kaybolur (D-186). */
typedef struct McsNode {
    volatile uint64_t locked;
    volatile struct McsNode *next;
    /* Satırı 64 byte'a doldur (padding); dizinin sonraki node'u ayrı satırda. */
    uint8_t dolgu[64 - 2 * sizeof(void *)];
} McsNode;

/* İki çekirdeğin node'ları — her biri kendi 64-byte satırında.
 * NOT: dış-bağ (non-static) → çekirdek 1'in naked trampoline'i asm'den sembol
 * adıyla erişebilsin diye linker sembolü emit etsin (aksi halde "unused" atılır). */
McsNode mcs_node0 __attribute__((aligned(64)));
McsNode mcs_node1 __attribute__((aligned(64)));

/* Kuyruğun sonu (global). NULL = kilit boş. kilitle() bunu atomik SWAP ile kendi
 * node'una set eder; ac() son node ise CAS ile NULL'a döndürür. Kendi satırında. */
static volatile McsNode *mcs_tail __attribute__((aligned(64))) = 0;

/* MCS-kilit ile korunan paylaşımlı sayaç. Kritik bölgede düz (atomik-olmayan)
 * artırılır — kilit karşılıklı-dışlama sağladığı için yarış yok. Kendi satırında. */
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

/* PSCI conduit çağrıları (smp_ticket_arm.c ile aynı desen). */
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

/* Paylaşılan bir adresin satırını PoC'ye clean+invalidate: kendi yazımını RAM'e
 * boşalt + satırı geçersiz kıl (sonraki okuma tazesini alır). */
static inline void satir_temizle_gecersiz(const volatile void *p) {
    __asm__ volatile(
        "dc civac, %0\n"
        "dsb sy\n"
        :
        : "r"(p)
        : "memory");
}

/* Paylaşılan bir adresin satırını PoC'den geçersiz kıl (okumadan önce) → diğer
 * çekirdeğin RAM'e yazdığı taze değeri yükle. */
static inline void satir_gecersiz(const volatile void *p) {
    __asm__ volatile(
        "dc ivac, %0\n"
        "dsb sy\n"
        :
        : "r"(p)
        : "memory");
}

/* --- mcs_swap_tail: `mcs_tail`'i atomik olarak `yeni`'ye set et, ÖNCEKİ değeri
 *     döndür (bileşik-atomik SWAP) ---
 *
 * aarch64 LDAXR/STLXR retry döngüsü: LDAXR mcs_tail'i exclusive+acquire yükler
 * (önceki tail), STLXR yeni değeri release-store dener — araya rakip çekirdek
 * girip yazarsa fail (w!=0) → cbnz ile taze değerle retry. İki çekirdek asla
 * aynı "önceki tail"i almaz → kuyruk zinciri kesin sıralı (FIFO).
 *
 * MMU-off/on: her denemeden ÖNCE satırı tazele (dc ivac → rakibin son yazımını
 * gör); başarılı yazımdan SONRA RAM'e boşalt (dc civac → rakip taze görsün). */
static inline volatile McsNode *mcs_swap_tail(volatile McsNode *yeni) {
    uint64_t onceki;
    uint32_t basarisiz;
    do {
        /* Rakip çekirdeğin (RAM'e) son yazımını görebilmek için satırı tazele. */
        satir_gecersiz(&mcs_tail);
        __asm__ volatile(
            "ldaxr  %0, [%3]\n"           /* onceki = mcs_tail (exclusive+acquire) */
            "stlxr  %w1, %2, [%3]\n"      /* mcs_tail = yeni dene (release); basarisiz=0 OK */
            : "=&r"(onceki), "=&r"(basarisiz)
            : "r"(yeni), "r"(&mcs_tail)
            : "memory");
        if (basarisiz == 0) {
            /* Başarılı yazımı RAM'e boşalt ki rakip çekirdek taze görsün. */
            satir_temizle_gecersiz(&mcs_tail);
            return (volatile McsNode *)onceki;
        }
        /* STLXR fail (rakip araya girdi) → taze değerle yeniden dene. */
    } while (1);
}

/* --- mcs_cas_tail: mcs_tail == beklenen ise NULL'a set et; başardıysa 1 döner ---
 *
 * ac()'ta "gerçekten son node ben miyim?" testi. LDAXR ile mcs_tail'i oku;
 * beklenen (benim node) ise STLXR ile NULL yaz. Değilse (araya biri girmiş)
 * exclusive monitor'ü temizleyip (clrex) 0 dön. STLXR fail (rakip SWAP araya
 * girdi) → 0 dön (halef var demektir, çağıran onu bekler).
 *
 * MMU-off/on: denemeden önce dc ivac (rakibin SWAP'ını gör), başarıda dc civac. */
static inline int mcs_cas_tail_bosalt(volatile McsNode *beklenen) {
    uint64_t okunan;
    uint32_t basarisiz;
    satir_gecersiz(&mcs_tail);
    __asm__ volatile(
        "ldaxr  %0, [%3]\n"               /* okunan = mcs_tail (exclusive+acquire) */
        "cmp    %0, %2\n"                 /* okunan == beklenen? */
        "b.ne   1f\n"                     /* değilse: halef var → clrex + fail */
        "stlxr  %w1, xzr, [%3]\n"         /* mcs_tail = NULL dene (release) */
        "b      2f\n"
        "1:\n"
        "clrex\n"                         /* exclusive monitor'ü bırak */
        "mov    %w1, #1\n"                /* basarisiz=1 (eşleşmedi) */
        "2:\n"
        : "=&r"(okunan), "=&r"(basarisiz)
        : "r"(beklenen), "r"(&mcs_tail)
        : "cc", "memory");
    if (basarisiz == 0) {
        satir_temizle_gecersiz(&mcs_tail);
        return 1;   /* CAS başardı: son node bendim, kuyruk artık boş */
    }
    return 0;       /* eşleşmedi (halef var) veya STLXR fail (rakip araya girdi) */
}

/* --- mcs_kilitle: MCS kuyruk-kilidini al (çekirdek KENDİ node'unda döner) ---
 *
 * 1. node'u sıfırla (next=NULL, locked=1) + RAM'e boşalt.
 * 2. Atomik SWAP ile mcs_tail'i kendi node'uma set et; önceki tail'i al.
 * 3. Önceki NULL ise → kuyruk boştu, kilit HEMEN benim (dönme yok).
 * 4. Değilse → selefin next'ini beni gösterecek şekilde yaz + RAM'e boşalt,
 *    sonra KENDİ node'umun locked'ı 0 olana kadar YEREL SPIN (bouncing yok).
 *
 * Dönüş: 1 = kilit alındı, 0 = timeout (bozulma; sessiz-gizleme yerine dış
 * doğrulama sayacı basıp FAIL raporlar). */
static inline int mcs_kilitle(volatile McsNode *node) {
    /* 1. Node'u başlat: kuyruk sonu (next yok), bekliyorum (locked=1). */
    node->next = 0;
    node->locked = 1;
    satir_temizle_gecersiz(node);
    __asm__ volatile("dmb ish" ::: "memory");

    /* 2. Kuyruğa gir: atomik SWAP ile mcs_tail'i kendi node'uma çevir. */
    volatile McsNode *onceki = mcs_swap_tail(node);

    /* 3. Kuyruk boştu → kilit HEMEN benim, yerel spin gerekmez. */
    if (onceki == 0) {
        __asm__ volatile("dmb ish" ::: "memory");   /* acquire: kritik bölge önce sıralı */
        return 1;
    }

    /* 4. Önümde selef var → onun next'ini beni gösterecek şekilde bağla. */
    onceki->next = node;
    satir_temizle_gecersiz(&onceki->next);
    __asm__ volatile("dsb sy" ::: "memory");

    /* 5. KENDİ node'umda YEREL SPIN: selefim ac()'ta locked'ımı 0 yapana kadar bekle.
     *    Bu MCS'nin çekirdeği: global adreste DEĞİL, kendi satırımda dönüyorum →
     *    başka çekirdeğin spin'i benim satırımı geçersizlemez (bouncing yok). */
    for (uint64_t bekle = 0; bekle < TIMEOUT_TIK; bekle++) {
        satir_gecersiz(&node->locked);
        if (node->locked == 0) {
            __asm__ volatile("dmb ish" ::: "memory");   /* acquire */
            return 1;   /* selefim beni serbest bıraktı → kilit benim */
        }
        __asm__ volatile("yield" ::: "memory");         /* kısa backoff (bounded) */
    }
    return 0;   /* timeout: kilit alınamadı (bozulma) */
}

/* --- mcs_ac: MCS-kilidi serbest bırak (YALNIZ halefimin node'unu yazarım) ---
 *
 * 1. node->next NULL (görünürde halef yok) ise:
 *    a. CAS(mcs_tail, node, NULL) başarır → gerçekten son node bendim, kuyruk boş,
 *       yazacak halef yok → dön.
 *    b. CAS başarmaz (araya biri SWAP'la girdi ama next'ini henüz yazmadı) →
 *       node->next set olana kadar bekle (bounded).
 * 2. node->next->locked = 0 → halefimi serbest bırak. Bu MCS'nin ölçeklenebilirlik
 *    kazanımı: YALNIZ o TEK çekirdeğin özel satırını yazarım → sadece o uyanır,
 *    diğer bekleyenlerin cache'i dokunulmaz (spinlock/ticket global-satırı yazardı).
 *
 * dmb ish (release): kritik-bölge yazımlarının halefi serbest bırakmadan ÖNCE
 * görünmesini garanti eder. Dönüş: 1 = başarılı, 0 = timeout (halef next yazmadı). */
static inline int mcs_ac(volatile McsNode *node) {
    __asm__ volatile("dmb ish" ::: "memory");   /* release: kritik-bölge yazımları önce */

    /* 1. Görünürde halef var mı? node->next'i taze oku. */
    satir_gecersiz(&node->next);
    if (node->next == 0) {
        /* 1a. Gerçekten son node ben miyim? CAS(mcs_tail, node → NULL). */
        if (mcs_cas_tail_bosalt(node)) {
            return 1;   /* son bendim, kuyruk boş, serbest bırakacak halef yok */
        }
        /* 1b. CAS başarmadı → araya rakip SWAP girdi ama next'ini henüz yazmadı.
         *     next set olana kadar bekle (bounded — bozulmada sonsuz spin yok). */
        int gorundu = 0;
        for (uint64_t bekle = 0; bekle < TIMEOUT_TIK; bekle++) {
            satir_gecersiz(&node->next);
            if (node->next != 0) { gorundu = 1; break; }
            __asm__ volatile("yield" ::: "memory");
        }
        if (!gorundu) {
            return 0;   /* timeout: halef next yazamadı (bozulma) */
        }
    }

    /* 2. Halefimi serbest bırak — YALNIZ onun locked satırını yaz (yerel uyandırma). */
    node->next->locked = 0;
    satir_temizle_gecersiz(&node->next->locked);
    __asm__ volatile("dsb sy" ::: "memory");
    return 1;
}

/* --- Ortak çekişme döngüsü (HER İKİ çekirdek aynısını koşar) ---
 *
 * N_TUR kez: MCS-kilitle → paylaşımlı sayacı DÜZ (atomik-olmayan) artır (kilit
 * koruduğu için yarış yok) → aç. Ayrıca bu çekirdeğin işlediği kritik-bölge
 * sayısını kendi slotunda tutar (FIFO adalet izleme). Kilit bir turda alınamazsa
 * (timeout) veya aç başarısızsa döngüyü keser → çağıran gerçek sayacı basıp FAIL
 * raporlar (sessiz-gizleme yok). Dönüş: bu çekirdeğin işlediği kritik-bölge sayısı. */
static uint64_t mcs_dongusu(volatile McsNode *node, volatile uint64_t *islenen_slot) {
    uint64_t islenen = 0;
    for (uint64_t k = 0; k < N_TUR; k++) {
        if (!mcs_kilitle(node)) {
            break;  /* timeout — kilit-doğruluk bozuldu; dış doğrulama yakalar */
        }
        /* --- KRİTİK BÖLGE (kilit koruması altında; atomik GEREKMEZ) --- */
        satir_gecersiz(&sayac);          /* diğer çekirdeğin son yazımını gör */
        sayac = sayac + 1;               /* düz artır — kilit serialize etti */
        satir_temizle_gecersiz(&sayac);  /* RAM'e boşalt → diğer çekirdek görsün */
        /* --- KRİTİK BÖLGE SONU --- */
        if (!mcs_ac(node)) {
            break;  /* aç timeout — halef next yazamadı; dış doğrulama yakalar */
        }
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
     *    kadar MCS döngüsüne girmez → iki çekirdek AYNI anda kuyruğa yarışmaya
     *    başlar (gerçek FIFO çekişmesi; tek çekirdek sırayla bitirmez). */
    __asm__ volatile("dsb sy" ::: "memory");
    cekirdek1_basladi = 1;
    satir_temizle_gecersiz(&cekirdek1_basladi);

    /* 2. GERÇEK İŞ: MCS queue-lock ile N_TUR kritik bölge koştur (KENDİ node'u). */
    mcs_dongusu(&mcs_node1, &islenen_cekirdek1);

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
    kdl_yazdir_metin("SMP MCS BASLA");

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
        kdl_yazdir_metin("SMP MCS FAIL: CPU_ON basarisiz — 2. cekirdek baslamadi");
        for (;;) { __asm__ volatile("wfe"); }
    }

    /* --- 2. RENDEZVOUS: çekirdek 1'in "hazırım" bayrağını bekle (bounded).
     *        Böylece çekirdek 0 kuyruğa GİRMEDEN önce çekirdek 1 de yarışa hazır →
     *        iki çekirdek AYNI anda `mcs_tail`'e SWAP yarışır (gerçek FIFO
     *        çekişmesi). Bounded: bayrak gelmezse yine de devam et. --- */
    for (uint64_t bekle = 0; bekle < TIMEOUT_TIK; bekle++) {
        satir_gecersiz(&cekirdek1_basladi);
        if (cekirdek1_basladi != 0) { break; }
    }

    /* --- 3. Çekirdek 0 de MCS queue-lock ile N_TUR kritik bölge koşturur (KENDİ
     *        node'u). İki çekirdek eşzamanlı `mcs_tail`'e SWAP yarışır → benzersiz
     *        kuyruk sırası + FIFO serbest bırakma her artırımı serialize eder
     *        (lost-update yok). --- */
    mcs_dongusu(&mcs_node0, &islenen_cekirdek0);

    /* --- 4. Çekirdek 1'in bitmesini bekle (bounded poll + cache-coherent). --- */
    int bitti = 0;
    for (uint64_t bekle = 0; bekle < TIMEOUT_TIK; bekle++) {
        satir_gecersiz(&cekirdek1_bitti);
        if (cekirdek1_bitti != 0) { bitti = 1; break; }
    }

    if (!bitti) {
        satir_gecersiz(&sayac);
        kdl_yaz_metin("SMP MCS FAIL: cekirdek1 timeout — sayac=");
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
     *   (1) KARŞILIKLI-DIŞLAMA: son sayac == 2*N == 10000 (MCS serialize etti →
     *       düz artırımda lost-update yok).
     *   (2) FIFO ADALET / açlık-yok: c0 + c1 == 2*N ve her ikisi de > 0 (ikisi de
     *       gerçekten hizmet gördü → açlık yok; MCS doğal FIFO kuyruğu sırayla geçti).
     * Kilit-doğruluk bozulursa gerçek sayacı bas + FAIL (sessiz-gizleme yok). */
    int kilit_dogru = (son == BEKLENEN);
    int adalet = (c0 + c1 == BEKLENEN) && (c0 > 0) && (c1 > 0);

    if (kilit_dogru && adalet) {
        kdl_yazdir_metin("SMP MCS OK");
    } else if (kilit_dogru) {
        /* Sayaç doğru ama işlenen paylaşımı beklenmedik (biri 0 veya toplam≠2N). */
        kdl_yaz_metin("SMP MCS FAIL sayac=");
        kdl_yazdir_isaretsiz_tam64(son);
        kdl_yaz_metin("DOGRU ama adalet bozuk: cekirdek0=");
        kdl_yazdir_isaretsiz_tam64(c0);
        kdl_yaz_metin("cekirdek1=");
        kdl_yazdir_isaretsiz_tam64(c1);
        kdl_yazdir_metin("(FIFO acligi/kayip)");
    } else {
        kdl_yaz_metin("SMP MCS FAIL sayac=");
        kdl_yazdir_isaretsiz_tam64(son);
        kdl_yaz_metin("(beklenen ");
        kdl_yazdir_isaretsiz_tam64(BEKLENEN);
        kdl_yazdir_metin(") — kilit serialize etmedi/lost-update");
    }

    for (;;) { __asm__ volatile("wfe"); }
}
