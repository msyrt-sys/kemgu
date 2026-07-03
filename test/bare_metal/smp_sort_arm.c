/*
 * KEMGU-OS bare-metal SMP 4-ÇEKİRDEK PARALEL BÖL-VE-YÖNET SIRALAMA testi (aarch64).
 * ===============================================================================
 *
 * Milestone (D-191 4-çekirdek + D-179 bariyer + D-170 paralel-hesap üstünde):
 * Önceki SMP testleri ya çekirdeklerin YAŞADIĞINI (smp4_arm.c), ya LOCKSTEP
 * senkronizasyonu (smp_barrier_arm.c), ya da bir dizinin İKİ yarıya bölünüp
 * TOPLANDIĞINI (smp_compute_arm.c) kanıtladı. Bu test İLK KEZ 4 çekirdekli
 * GERÇEK BÖL-VE-YÖNET ALGORİTMASI koşar: paylaşımlı 32-elemanlı KARIŞIK dizi
 * 4 çeyreğe (her biri 8 eleman) bölünür; HER çekirdek KENDİ çeyreğini yerinde
 * SIRALAR (insertion sort); tümü bir BARİYER'de buluşur; sonra çekirdek 0 dört
 * sıralı çeyreği ardışık 2'li MERGE ile birleştirir → TAM sıralı dizi.
 *
 * İŞ BÖLÜMÜ (çeyrekler AYRIK → kilit GEREKMEZ):
 *   - Çekirdek 0 (BSP, MMU-ON):  çeyrek 0 = dizi[0..8)   sıralar.
 *   - Çekirdek 1 (AP, MMU-OFF):  çeyrek 1 = dizi[8..16)  sıralar.
 *   - Çekirdek 2 (AP, MMU-OFF):  çeyrek 2 = dizi[16..24) sıralar.
 *   - Çekirdek 3 (AP, MMU-OFF):  çeyrek 3 = dizi[24..32) sıralar.
 * Çeyrekler örtüşmez → aynı belleğe iki çekirdek yazmaz → VERİ YARIŞI YOK,
 * bu yüzden çeyrek-sıralama fazında SPINLOCK gerekmez (yalnız cache coherency).
 *
 * BARİYER — sense-reversing (nesil/generation ters-çevirme, D-179 dersi):
 *   4 çekirdek çeyrek-sıralamayı bitirdiğinde MERGE-öncesi senkronize OLMALI:
 *   çekirdek 0 birleştirmeye başlamadan ÖNCE 1,2,3 çeyreklerinin SIRALANMIŞ +
 *   RAM'e boşaltılmış olduğundan emin olmalı. SPINLOCK korumasında varan_sayaci
 *   artırılır; SON gelen (sayaç == 4) sayacı sıfırlar + nesli artırır (herkesi
 *   serbest bırakır). Erken gelenler nesil DEĞİŞENE kadar bekler. Sense-reversing:
 *   sayaç yerine NESLİ izlemek ABA problemini önler (D-179).
 *
 * MERGE (çekirdek 0, bariyer sonrası): dört sıralı çeyrek ardışık 2'li merge ile
 * birleşir. Önce çeyrek0+çeyrek1 → [0,16) sıralı yarı; çeyrek2+çeyrek3 → [16,32)
 * sıralı yarı; sonra iki yarı → [0,32) TAM sıralı. Klasik böl-ve-yönet birleşme.
 *
 * DOĞRULAMA (iki koşul birden — permütasyon + sıra):
 *   (1) SIRA:  her i için sonuc[i] <= sonuc[i+1] (tam sıralı)
 *   (2) TOPLAM: sonuc'un toplamı == girdinin toplamı (permütasyon — hiçbir eleman
 *       kaybolmadı/uydurulmadı; merge her elemanı tam bir kez taşıdı)
 *   İkisi de sağlanırsa "SMP SORT OK". Bir eleman kaybı toplamı bozar; bir sıra
 *   ihlali (i)'yi bozar → ikisi birlikte gerçek doğruluk kanıtı.
 *
 * KRİTİK — cache coherency (D-170/180 dersi): AP'ler (1,2,3) MMU-OFF (non-
 * cacheable, doğrudan RAM); BSP (0) MMU-ON (Normal-WB cacheable). Bu cacheability
 * uyuşmazlığı donanım coherency'sini bozar → paylaşılan her satır için EL ile
 * senkronizasyon:
 *   - Bir çekirdek paylaşılan satıra YAZDIKTAN sonra `dc civac` + `dsb sy` → RAM'e
 *     boşalt (diğer çekirdek RAM'den okuyor).
 *   - Bir çekirdek paylaşılan satırı OKUMADAN önce `dc ivac` + `dsb sy` → tazesini
 *     yükle. Her AP kendi çeyreğini sıraladıktan sonra o çeyreğin TÜM elemanlarını
 *     boşaltır; çekirdek 0 merge'den önce 1,2,3 çeyreklerini geçersiz kılar.
 *
 * KRİTİK — naked trampoline SP-önce (D-174 dersi): PSCI CPU_ON, AP'yi MMU-OFF,
 * EL1'de, UNDEFINED SP ile çağırır. C prologue callee-saved register'ları
 * undefined SP'ye spill eder → garbage yazım → sessiz çöküş. ÇÖZÜM: naked
 * trampoline — İLK iş MPIDR oku + SP kur (C-prologue spill'inden ÖNCE), SONRA C.
 *
 * KRİTİK — MPIDR-indeksli per-çekirdek yığın (D-191 dersi): 3 AP TEK ortak girişe
 * dallanır; her biri `mrs mpidr_el1` + `& 0xFF` ile KENDİ çekirdek numarasını
 * bulur, o numarayla İNDEKSLENMİŞ 8 KB yığınına SP kurar (izole yığınlar).
 *
 * FALSE-SHARING YOK (D-186 dersi): çekirdek-durum bayrakları (çeyrek-bitti +
 * bariyer sözcükleri) AYRI 64-byte cache satırlarında; `dc civac`/`dc ivac` bir
 * SATIRIN TAMAMINA etki eder, komşu bayrağı ezmez.
 *
 * DETERMİNİSTİK: girdi sabit formülle üretilir (dizi[i] = (i*7+13)%97); sıralama +
 * merge bounded; bariyer beklemesi bounded poll. Yük/internet bağımsız — her koşuda
 * aynı sonuç (5/5 byte-identik: TAM sıralı + toplam korunur → "SMP SORT OK").
 */
#include <stdint.h>

extern void kdl_yazdir_metin(const char *);
extern void kdl_yaz_metin(const char *);
extern void kdl_yazdir_onaltilik(uint64_t);
extern void kdl_yaz_onaltilik(uint64_t);
extern void kdl_yazdir_isaretsiz_tam64(uint64_t);  /* satır-sonlu ondalık */

#define N_ELEMAN     32u                 /* toplam eleman */
#define CEKIRDEK_SAYI 4u                 /* BSP 0 + AP 1,2,3 */
#define CEYREK       (N_ELEMAN / CEKIRDEK_SAYI)  /* 8 eleman / çeyrek */
#define AP_YIGIN_BOYUT 8192u             /* her çekirdek için ayrı 8 KB yığın */
#define TIMEOUT_TIK  40000000ULL         /* bounded üst-sınır (yük-bağımsız) */

/* --- Paylaşımlı böl-ve-yönet durumu (kritik olanlar kendi 64-byte satırında) --- */

/* Paylaşımlı iş dizisi: dizi[i] = (i*7+13)%97 (deterministik KARIŞIK girdi). Her
 * çekirdek KENDİ çeyreğine yazar (çeyrekler ayrık → yarış yok). 64-byte hizalı. */
static volatile uint64_t is_dizisi[N_ELEMAN] __attribute__((aligned(64)));

/* Çekirdek 0'ın merge için kullandığı scratch (yalnız çekirdek 0 dokunur). */
static volatile uint64_t merge_scratch[N_ELEMAN] __attribute__((aligned(64)));

/* Çeyrek-bitti bayrakları — her slot KENDİ 64-byte cache satırında (false-sharing
 * yok; `dc civac`/`dc ivac` satır granülaritesinde çalışır). Indeks = çekirdek no.
 * [0]=BSP kendi çeyreğini sıraladı; [1..3]=AP'ler. Bariyer bunları örtük kullanır
 * ama teşhis için ayrıca tutulur. */
typedef struct {
    volatile uint64_t bitti;
    volatile uint8_t  dolgu[64 - sizeof(uint64_t)];
} __attribute__((aligned(64))) CeyrekDurum;
static CeyrekDurum ceyrek_durum[CEKIRDEK_SAYI] __attribute__((aligned(64)));

/* --- Sense-reversing bariyer durumu (ayrı satırlar) --- */
static volatile uint64_t varan_sayaci __attribute__((aligned(64))) = 0;
static volatile uint64_t nesil        __attribute__((aligned(64))) = 0;
static volatile uint32_t kilit        __attribute__((aligned(64))) = 0;

/* Tüm AP yığınları TEK dizi — MPIDR-indeksli. TABAN + (no+1)*BOYUT = o çekirdeğin
 * TEPESİ (yığın aşağı büyür). Çekirdek 0 girişe girmez ama hizalama için 0. blok
 * da ayrılır. Dış-bağ (non-static) → naked trampoline asm sembol adıyla eriştiği
 * için linker emit etmeli. 16-byte hizalı (AArch64 SP hizalama gereği). */
uint8_t ap_yiginlar[CEKIRDEK_SAYI * AP_YIGIN_BOYUT] __attribute__((aligned(16)));

/* PSCI conduit çağrıları (smp4_arm.c ile aynı desen). */
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

/* --- Spinlock: aarch64 LDAXR/STXR atomik test-and-set + STLR release --- */

/* Kilidi al: kilit==0 iken atomik olarak 1 yap. Başarısızsa tekrar dene. LDAXR
 * acquire + STLR release semantiği kritik bölgeyi çevreler → doğal bellek bariyeri.
 * MMU-off/on uyuşmazlığında exclusive monitor karışabilir; her denemeden önce kilit
 * satırını geçersiz kıl (dc ivac) → diğer çekirdeğin serbest bırakışını gör. */
static inline void kilit_al(void) {
    uint32_t alindi;
    uint32_t onceki;
    do {
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
        if (alindi == 0 && onceki == 0) {
            satir_temizle_gecersiz(&kilit);   /* meşgul olduğumuzu RAM'e boşalt */
            break;
        }
        for (volatile int b = 0; b < 64; b++) { __asm__ volatile("yield"); }
    } while (1);
}

/* Kilidi bırak: STLR ile 0 yaz (release) + RAM'e boşalt (dc civac). */
static inline void kilit_birak(void) {
    __asm__ volatile(
        "stlr  wzr, [%0]\n"
        :
        : "r"(&kilit)
        : "memory");
    satir_temizle_gecersiz(&kilit);
}

/* --- Sense-reversing 4-çekirdek bariyer (D-179 deseni) ---
 *
 * Her çekirdek çağırır. SPINLOCK korumasında varan_sayaci'nı artır; SON gelen
 * (sayaç == CEKIRDEK_SAYI) sayacı sıfırlar + nesli artırır (tüm bekleyenleri
 * serbest bırakır). Erken gelenler nesil DEĞİŞENE kadar bekler (bounded poll +
 * dc ivac). Böylece 4 çekirdek de bariyerden aynı çizgide çıkar.
 *
 * Dönüş: 1 = normal (nesil beklemesi başarılı veya son-gelen), 0 = timeout. */
static int bariyer_bekle(void) {
    kilit_al();
    satir_gecersiz(&varan_sayaci);
    satir_gecersiz(&nesil);
    uint64_t bu_nesil = nesil;
    uint64_t sayac = varan_sayaci + 1;
    varan_sayaci = sayac;
    satir_temizle_gecersiz(&varan_sayaci);

    if (sayac == CEKIRDEK_SAYI) {
        /* SON gelen: sayacı sıfırla + nesli artır → herkesi serbest bırak. */
        varan_sayaci = 0;
        satir_temizle_gecersiz(&varan_sayaci);
        nesil = bu_nesil + 1;
        satir_temizle_gecersiz(&nesil);
        kilit_birak();
        return 1;
    }

    /* Erken gelen: kilidi bırak, sonra nesil DEĞİŞENE kadar bekle (bounded). */
    kilit_birak();
    for (uint64_t bekle = 0; bekle < TIMEOUT_TIK; bekle++) {
        satir_gecersiz(&nesil);
        if (nesil != bu_nesil) {
            return 1;   /* Nesil arttı → bariyer tamamlandı, geç. */
        }
        for (volatile int b = 0; b < 32; b++) { __asm__ volatile("yield"); }
    }
    return 0;   /* Timeout — nesil değişmedi (coherency/deadlock şüphesi). */
}

/* --- Çeyrek sıralama (insertion sort — yerinde, KENDİ çeyreği ayrık) ---
 *
 * dizi[bas..son) aralığını yerinde artan sıraya diz. Her çekirdek KENDİ ayrık
 * çeyreğini çağırır → başka çekirdeğin belleğine dokunmaz → kilit gerekmez.
 * Cache-coherent: okumadan önce dc ivac, yazdıktan sonra dc civac (MMU-off AP'ler
 * ile MMU-on BSP arasında görünürlük). */
static void ceyregi_sirala(uint32_t bas, uint32_t son) {
    /* Önce çeyreğin tümünü taze oku (MMU-off çekirdek main'in doldurduğunu görsün). */
    for (uint32_t i = bas; i < son; i++) {
        satir_gecersiz(&is_dizisi[i]);
    }
    /* Insertion sort: yerel değerler register'da, dizi yerinde güncellenir. */
    for (uint32_t i = bas + 1; i < son; i++) {
        uint64_t anahtar = is_dizisi[i];
        uint32_t j = i;
        while (j > bas && is_dizisi[j - 1] > anahtar) {
            is_dizisi[j] = is_dizisi[j - 1];
            j--;
        }
        is_dizisi[j] = anahtar;
    }
    /* Sıralanmış çeyreği RAM'e boşalt (çekirdek 0 merge'de okuyacak). */
    for (uint32_t i = bas; i < son; i++) {
        satir_temizle_gecersiz(&is_dizisi[i]);
    }
}

/*
 * Bir AP'nin asıl işi (C) — SP zaten kurulu çağrılır (ortak trampoline'den).
 * `cekirdek_no` = trampoline'in MPIDR Aff0'dan okuduğu çekirdek numarası (1,2,3).
 * UART'a DOKUNMA (BSP ile paylaşımlı) — yalnız paylaşılan RAM'e yaz.
 *
 * Her AP: (1) KENDİ çeyreğini sırala, (2) çeyrek-bitti bayrağını set et,
 * (3) BARİYER'de diğer 3 çekirdekle buluş (çekirdek 0 merge'e başlamadan önce
 * hepsi bariyerde), (4) sonsuz spin.
 */
__attribute__((noreturn))
void ap_isi(uint64_t cekirdek_no) {
    if (cekirdek_no < CEKIRDEK_SAYI) {
        uint32_t bas = (uint32_t)cekirdek_no * CEYREK;
        uint32_t son = bas + CEYREK;

        /* 1. KENDİ çeyreğini sırala (ayrık → kilit gerekmez). */
        ceyregi_sirala(bas, son);

        /* 2. Çeyrek-bitti bayrağını set et (teşhis; bariyer asıl senkron). */
        __asm__ volatile("dsb sy" ::: "memory");
        ceyrek_durum[cekirdek_no].bitti = 1;
        satir_temizle_gecersiz(&ceyrek_durum[cekirdek_no].bitti);

        /* 3. BARİYER: 4 çekirdek burada buluşur (merge-öncesi senkron). */
        (void)bariyer_bekle();
    }

    /* 4. Sonsuz düşük-güç spin. */
    __asm__ volatile("sev" ::: "memory");
    for (;;) {
        __asm__ volatile("wfe" ::: "memory");
    }
}

/*
 * ORTAK AP GİRİŞ noktası — 3 AP'nin de (çekirdek 1,2,3) dallandığı TEK giriş.
 * PSCI CPU_ON tarafından MMU-OFF, EL1'de, UNDEFINED SP ile çağrılır (D-174).
 * Naked → compiler prologue/epilogue üretmez; İLK iş SP kur (spill'den önce).
 *
 * MPIDR-indeksli yığın seçimi (D-191, saf asm):
 *   1. mrs x1, mpidr_el1; and x1, x1, #0xFF → çekirdek_no
 *   2. yığın TEPESİ = &ap_yiginlar + (çekirdek_no + 1) * AP_YIGIN_BOYUT
 *   3. mov sp, <tepe>; mov x0, çekirdek_no; b ap_isi
 */
__attribute__((naked, noreturn))
static void ortak_ap_giris(void) {
    __asm__ volatile(
        "mrs  x1, mpidr_el1\n"
        "and  x1, x1, #0xFF\n"        /* x1 = çekirdek_no (Aff0) */
        "adrp x0, ap_yiginlar\n"
        "add  x0, x0, :lo12:ap_yiginlar\n"   /* x0 = &ap_yiginlar[0] (TABAN) */
        "add  x2, x1, #1\n"           /* x2 = çekirdek_no + 1 */
        "mov  x3, %0\n"               /* x3 = AP_YIGIN_BOYUT */
        "mul  x2, x2, x3\n"           /* x2 = (çekirdek_no+1) * BOYUT */
        "add  x0, x0, x2\n"           /* x0 = o çekirdeğin yığın TEPESİ */
        "mov  sp, x0\n"               /* SP kuruldu (16-byte hizalı) */
        "mov  x0, x1\n"               /* x0 = çekirdek_no → ap_isi(cekirdek_no) */
        "b    ap_isi\n"               /* SP kurulu → C işine dallan (noreturn) */
        :
        : "i"((uint64_t)AP_YIGIN_BOYUT)
        : "x0", "x1", "x2", "x3", "memory");
}

/* PSCI dönüş/fonksiyon kodları. */
#define PSCI_SUCCESS       0
#define PSCI_NOT_SUPPORTED (-1)
#define PSCI_FN_CPU_ON  0xC4000003ULL
#define PSCI_FN_VERSION 0x84000000ULL

/* Bir AP'yi (target MPIDR = hedef affinity) PSCI CPU_ON ile başlat. Ortak girişe
 * dallanır → AP kendi MPIDR'ından kimliğini bulur. HVC → SMC fallback.
 * Döner: PSCI dönüş kodu (0=başarı). conduit_out NULL değilse conduit adı yazılır. */
static int64_t ap_baslat(uint64_t hedef_mpidr, const char **conduit_out) {
    uint64_t giris = (uint64_t)(uintptr_t)&ortak_ap_giris;
    const char *conduit = "HVC";
    int64_t ret = psci_hvc(PSCI_FN_CPU_ON, hedef_mpidr, giris, 0xC0DE);
    if (ret == PSCI_NOT_SUPPORTED) {
        conduit = "SMC";
        ret = psci_smc(PSCI_FN_CPU_ON, hedef_mpidr, giris, 0xC0DE);
    }
    if (conduit_out != (const char **)0) {
        *conduit_out = conduit;
    }
    return ret;
}

/* İki sıralı bitişik aralığı [bas,orta) ve [orta,son) → merge_scratch'e sıralı
 * birleştir, sonra is_dizisi'ye geri kopyala (yalnız çekirdek 0 çağırır). Girdi
 * aralıklarını okumadan önce cache-coherent tazele. */
static void merge_araligi(uint32_t bas, uint32_t orta, uint32_t son) {
    /* Girdi aralıklarını taze oku (AP'lerin RAM'e boşalttıklarını gör). */
    for (uint32_t i = bas; i < son; i++) {
        satir_gecersiz(&is_dizisi[i]);
    }
    uint32_t i = bas, j = orta, k = bas;
    while (i < orta && j < son) {
        if (is_dizisi[i] <= is_dizisi[j]) {
            merge_scratch[k++] = is_dizisi[i++];
        } else {
            merge_scratch[k++] = is_dizisi[j++];
        }
    }
    while (i < orta) { merge_scratch[k++] = is_dizisi[i++]; }
    while (j < son)  { merge_scratch[k++] = is_dizisi[j++]; }
    /* Birleşimi is_dizisi'ye geri yaz. */
    for (uint32_t t = bas; t < son; t++) {
        is_dizisi[t] = merge_scratch[t];
    }
}

int main(void) {
    kdl_yazdir_metin("SMP SORT BASLA");

    /* --- 0. Deterministik KARIŞIK girdiyi üret: dizi[i] = (i*7+13)%97. Girdi
     *        toplamını (permütasyon referansı) hesapla. Sonra RAM'e boşalt ki
     *        MMU-off AP'ler taze okusun. --- */
    uint64_t girdi_toplam = 0;
    for (uint32_t i = 0; i < N_ELEMAN; i++) {
        uint64_t v = (uint64_t)((i * 7u + 13u) % 97u);
        is_dizisi[i] = v;
        girdi_toplam += v;
    }
    for (uint32_t i = 0; i < N_ELEMAN; i++) {
        satir_temizle_gecersiz(&is_dizisi[i]);
    }

    kdl_yaz_metin("girdi-toplam=");
    kdl_yazdir_isaretsiz_tam64(girdi_toplam);

    /* --- 1. Çekirdek 1,2,3'ü sırayla PSCI CPU_ON ile başlat (ortak giriş). Her
     *        AP kendi çeyreğini sıralar + bariyerde buluşur. --- */
    for (uint64_t no = 1; no < CEKIRDEK_SAYI; no++) {
        const char *conduit = "HVC";
        int64_t ret = ap_baslat(no, &conduit);   /* hedef MPIDR affinity = no */

        kdl_yaz_metin("CPU_ON cekirdek=");
        kdl_yaz_onaltilik(no);
        kdl_yaz_metin(" conduit=");
        kdl_yaz_metin(conduit);
        kdl_yaz_metin(" ret=");
        kdl_yazdir_onaltilik((uint64_t)ret);

        if (ret != PSCI_SUCCESS) {
            kdl_yaz_metin("SMP SORT FAIL: CPU_ON basarisiz cekirdek=");
            kdl_yazdir_isaretsiz_tam64(no);
            kdl_yazdir_metin("");
            for (;;) { __asm__ volatile("wfe"); }
        }
    }

    /* --- 2. Çekirdek 0 KENDİ çeyreğini (çeyrek 0 = [0,8)) sıralar. --- */
    ceyregi_sirala(0, CEYREK);
    __asm__ volatile("dsb sy" ::: "memory");
    ceyrek_durum[0].bitti = 1;
    satir_temizle_gecersiz(&ceyrek_durum[0].bitti);

    /* --- 3. BARİYER: çekirdek 0 da bariyere katılır → 4 çekirdek buluşur. Bu
     *        dönünce 1,2,3 çeyrekleri sıralanmış + RAM'e boşalmış GARANTİ (SON
     *        gelen nesli artırınca herkes serbest; boşaltmalar bariyerden önce). --- */
    int bariyer_ok = bariyer_bekle();
    if (!bariyer_ok) {
        /* Teşhis: hangi çeyrekler bitti? */
        for (uint64_t d = 0; d < CEKIRDEK_SAYI; d++) {
            satir_gecersiz(&ceyrek_durum[d].bitti);
            kdl_yaz_metin("ceyrek ");
            kdl_yaz_onaltilik(d);
            kdl_yaz_metin(" bitti=");
            kdl_yazdir_isaretsiz_tam64(ceyrek_durum[d].bitti);
        }
        kdl_yazdir_metin("SMP SORT FAIL: bariyer timeout — ceyrek-siralama senkronu bozuldu");
        for (;;) { __asm__ volatile("wfe"); }
    }

    /* --- 3b. Ek güvence: tüm çeyrek-bitti bayraklarını cache-coherent doğrula
     *         (bariyer zaten senkronladı ama açık teşhis). --- */
    int hepsi_bitti = 1;
    for (uint64_t d = 0; d < CEKIRDEK_SAYI; d++) {
        satir_gecersiz(&ceyrek_durum[d].bitti);
        if (ceyrek_durum[d].bitti == 0) {
            hepsi_bitti = 0;
            kdl_yaz_metin("SMP SORT FAIL: ceyrek bitmedi cekirdek=");
            kdl_yazdir_isaretsiz_tam64(d);
            kdl_yazdir_metin("");
        }
    }
    if (!hepsi_bitti) {
        for (;;) { __asm__ volatile("wfe"); }
    }

    /* --- 4. MERGE (çekirdek 0, böl-ve-yönet birleşme): dört sıralı çeyreği
     *        ardışık 2'li merge ile birleştir. --- */
    /* 4a. çeyrek0 + çeyrek1 → [0,16) sıralı yarı. */
    merge_araligi(0, CEYREK, 2u * CEYREK);
    /* 4b. çeyrek2 + çeyrek3 → [16,32) sıralı yarı. */
    merge_araligi(2u * CEYREK, 3u * CEYREK, N_ELEMAN);
    /* 4c. iki yarı → [0,32) TAM sıralı. */
    merge_araligi(0, 2u * CEYREK, N_ELEMAN);

    /* --- 5. DOĞRULAMA: (1) tam sıralı, (2) toplam korunur (permütasyon). --- */
    int sirali = 1;
    uint64_t sonuc_toplam = is_dizisi[0];
    for (uint32_t i = 1; i < N_ELEMAN; i++) {
        if (is_dizisi[i - 1] > is_dizisi[i]) {
            sirali = 0;
        }
        sonuc_toplam += is_dizisi[i];
    }

    /* Sıralı sonucu bas (deterministik kanıt). */
    kdl_yaz_metin("sonuc:");
    for (uint32_t i = 0; i < N_ELEMAN; i++) {
        kdl_yaz_metin(" ");
        kdl_yaz_onaltilik(is_dizisi[i]);
    }
    kdl_yazdir_metin("");
    kdl_yaz_metin("sonuc-toplam=");
    kdl_yazdir_isaretsiz_tam64(sonuc_toplam);

    /* --- 6. Sonuç. --- */
    if (sirali && sonuc_toplam == girdi_toplam) {
        kdl_yazdir_metin("SMP SORT OK");
    } else if (!sirali && sonuc_toplam == girdi_toplam) {
        kdl_yazdir_metin("SMP SORT FAIL: toplam korundu ama dizi sirali degil (merge bozuk)");
    } else if (sirali) {
        kdl_yaz_metin("SMP SORT FAIL: sirali ama toplam bozuk — girdi=");
        kdl_yazdir_isaretsiz_tam64(girdi_toplam);
        kdl_yaz_metin("sonuc=");
        kdl_yazdir_isaretsiz_tam64(sonuc_toplam);
        kdl_yazdir_metin("");
    } else {
        kdl_yaz_metin("SMP SORT FAIL: ne sirali ne toplam — girdi=");
        kdl_yazdir_isaretsiz_tam64(girdi_toplam);
        kdl_yaz_metin("sonuc=");
        kdl_yazdir_isaretsiz_tam64(sonuc_toplam);
        kdl_yazdir_metin("");
    }

    for (;;) { __asm__ volatile("wfe"); }
}
