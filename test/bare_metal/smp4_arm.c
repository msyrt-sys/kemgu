/*
 * KEMGU-OS bare-metal ÇOK-ÇEKİRDEK BRING-UP testi (aarch64) — 4 ÇEKİRDEK.
 * ==========================================================================
 *
 * Milestone (D-169/174/186 SMP üstünde): Şimdiye kadarki tüm SMP testleri YALNIZ
 * 2 çekirdek kullandı (BSP çekirdek 0 + tek AP çekirdek 1). Bu test SMP'yi 4
 * çekirdeğe ÖLÇEKLER: BSP (çekirdek 0) çekirdek 1, 2 ve 3'ü PSCI CPU_ON ile
 * ayrı ayrı başlatır; üçü de canlı olduğunu kanıtlar. Bu "2→4 ölçekleme"
 * milestone'u — çoklu-AP koordinasyonu (ortak giriş + MPIDR-indeksli yığın +
 * çekirdek-başına canlı-bayrağı) ilk kez burada.
 *
 * ORTAK AP GİRİŞ (kritik tasarım farkı): 2-çekirdek testleri (smp_queue_arm.c,
 * smp_prodcons_arm.c) tek AP için tek giriş kullandı (çekirdek 1 sabit-kodlu).
 * 3 AP için AYRI giriş yazmak yerine TEK ortak giriş fonksiyonu kullanılır:
 *   - Her AP aynı `ortak_ap_giris` naked trampoline'ine dallanır.
 *   - Trampoline `mrs x, mpidr_el1` ile KENDİ MPIDR'ını okur, Aff0 (& 0xFF) =
 *     çekirdek numarası (QEMU virt cortex-a72: çekirdek 0,1,2,3 → Aff0 0,1,2,3).
 *   - Çekirdek numarasıyla İNDEKSLENMİŞ kendi 8 KB yığınının tepesini SP'ye kur
 *     (4 ayrı yığın, MPIDR-indeksli → her AP kendi izole yığınına sahip).
 *   - SONRA ortak C işine (`ap_isi`) dallan.
 * Böylece 3 AP TEK kod yolundan geçer ama her biri kendi kimliğini bulur.
 *
 * KRİTİK — naked trampoline (D-174 dersi): PSCI CPU_ON, AP'yi MMU-OFF, EL1'de,
 * UNDEFINED SP ile çağırır. C prologue callee-saved register'ları undefined SP'ye
 * `stp ... [sp,#-N]!` ile spill eder → garbage adrese yazım → sessiz çöküş. ÇÖZÜM:
 * naked trampoline — SP kurulmadan ÖNCE HİÇBİR C prologue çalışmaz (compiler
 * prologue/epilogue üretmez). İLK iş MPIDR oku + SP kur, SONRA C'ye dallan.
 *
 * KRİTİK — MPIDR-indeksli yığın adresleme (naked içinde): Naked'da yerel değişken
 * / C hesabı YOK. Yığın seçimi tümüyle asm: MPIDR & 0xFF → çekirdek no; yığın
 * dizisi TABANI + (çekirdek_no + 1) * YIGIN_BOYUT = O çekirdeğin yığın TEPESİ
 * (yığın aşağı büyür, dizi[cekirdek_no] bloğunun ÜST sınırı). Çekirdek 0 (BSP)
 * bu girişe hiç girmez (kendi başlangıç yığınını kullanır) — yalnız 1,2,3.
 *
 * KRİTİK — cache coherency (D-170/174 dersi): AP'ler MMU-OFF (non-cacheable,
 * doğrudan RAM); BSP MMU-ON (Normal-WB cacheable). Bu cacheability uyuşmazlığı
 * donanım coherency'sini bozar → paylaşılan canlı-bayrağı dizisi için EL ile
 * senkronizasyon:
 *   - AP kendi bayrağını YAZDIKTAN sonra `dc civac` + `dsb sy` → RAM'e boşalt.
 *   - BSP bir bayrağı OKUMADAN önce `dc ivac` + `dsb sy` → tazesini yükle.
 *
 * FALSE-SHARING YOK: 4 çekirdeğin canlı-bayrağı AYRI 64-byte cache satırlarında
 * (padding'li struct dizisi). `dc civac`/`dc ivac` bir SATIRIN TAMAMINA etki
 * eder; bayraklar bitişik olsaydı bir AP'nin `dc civac`'ı komşu AP'nin henüz-
 * boşalmamış bayrağını RAM'e ezerdi (torn). Her bayrak kendi satırında → cache-
 * bakım granülaritesi tam o bayrağa sınırlanır.
 *
 * DOĞRULAMA (dört koşul birden):
 *   (1) cekirdek_canli[1] set  (çekirdek 1 AP gerçekten koştu)
 *   (2) cekirdek_canli[2] set  (çekirdek 2 AP gerçekten koştu)
 *   (3) cekirdek_canli[3] set  (çekirdek 3 AP gerçekten koştu)
 *   (4) her AP kendi MPIDR'ını RAM'e yazdı — 1,2,3 (farklı çekirdek kanıtı)
 *   Dördü de sağlanırsa "SMP4 OK 4 cekirdek". Eksik varsa "SMP4 EKSIK cekirdek=N".
 *
 * DETERMİNİSTİK: 3 AP başlatma bounded (her CPU_ON dönüş kodu kontrol edilir);
 * canlı-bayrağı beklemesi bounded poll (< TIMEOUT_TIK). Yük/internet bağımsız —
 * her koşuda aynı sonuç (SMP4 OK, MPIDR'lar 1,2,3).
 */
#include <stdint.h>

extern void kdl_yazdir_metin(const char *);
extern void kdl_yaz_metin(const char *);
extern void kdl_yazdir_onaltilik(uint64_t);
extern void kdl_yaz_onaltilik(uint64_t);
extern void kdl_yazdir_isaretsiz_tam64(uint64_t);  /* satır-sonlu ondalık */

#define CEKIRDEK_SAYI  4u                  /* toplam çekirdek: BSP 0 + AP 1,2,3 */
#define AP_YIGIN_BOYUT 8192u               /* her çekirdek için ayrı 8 KB yığın */
#define TIMEOUT_TIK    40000000ULL         /* bounded üst-sınır (yük-bağımsız) */

/* --- Paylaşımlı çok-çekirdek durumu (her biri kendi 64-byte cache satırında,
 *     yanlış-paylaşım/klobber önlemi) --- */

/* Çekirdek-başına canlı bayrağı — HER slot KENDİ 64-byte cache satırında
 * (padding ile). KRİTİK: `dc civac`/`dc ivac` bir SATIRIN TAMAMINA etki eder;
 * bayraklar bitişik olsaydı bir AP'nin boşaltması komşu AP'nin bayrağını ezerdi
 * (MMU-off AP ile MMU-on BSP arasında torn okuma). Her bayrağı ayrı satıra
 * koyunca cache-bakım granülaritesi TAM O BAYRAĞA sınırlanır (false-sharing yok).
 *
 * canli: o çekirdek koştu mu (0/1). mpidr_gozlemlenen: o çekirdeğin okuduğu
 * kendi MPIDR Aff0 değeri (0,1,2,3 beklenir — farklı çekirdek kanıtı). */
typedef struct {
    volatile uint64_t canli;
    volatile uint64_t mpidr_gozlemlenen;
    volatile uint8_t  dolgu[64 - 2u * sizeof(uint64_t)];  /* satırı 64 byte'a tamamla */
} __attribute__((aligned(64))) CekirdekDurum;

/* Çekirdek-başına durum dizisi — her slot kendi cache satırında. Indeks =
 * çekirdek numarası (MPIDR Aff0). [0]=BSP (kullanılmaz), [1..3]=AP'ler. */
static CekirdekDurum cekirdek_durum[CEKIRDEK_SAYI] __attribute__((aligned(64)));

/* MPIDR Aff0 çıkarımı için geriye-uyumlu tek-değer erişim (canlı bayrağı). */
#define cekirdek_canli(n) (cekirdek_durum[(n)].canli)

/* Tüm AP yığınları TEK dizi — MPIDR-indeksli. TABAN + (no+1)*BOYUT = o çekirdeğin
 * TEPESİ (yığın aşağı büyür). Çekirdek 0 (BSP) girişe hiç girmez ama basitlik ve
 * hizalama için 0. blok da ayrılır (kullanılmaz). Dış-bağ (non-static) → naked
 * trampoline asm'den sembol adıyla eriştiği için linker emit etmeli (aksi halde
 * "unused" atılır). 16-byte hizalı (AArch64 SP hizalama gereği). */
uint8_t ap_yiginlar[CEKIRDEK_SAYI * AP_YIGIN_BOYUT] __attribute__((aligned(16)));

/* PSCI conduit çağrıları (smp_queue_arm.c / smp_prodcons_arm.c ile aynı desen). */
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

/*
 * Bir AP'nin asıl işi (C) — SP zaten kurulu çağrılır (ortak trampoline'den).
 * `cekirdek_no` = trampoline'in MPIDR Aff0'dan okuduğu çekirdek numarası (1,2,3).
 * UART'a DOKUNMA (BSP ile paylaşımlı) — yalnız paylaşılan RAM'e yaz.
 *
 * Bu fonksiyon 3 AP'nin ORTAK kod yoludur: her AP kendi cekirdek_no'suyla girer,
 * kendi durum-slotunu (cekirdek_durum[cekirdek_no]) set eder. Aynı kod, farklı
 * slot → çoklu-AP tek koddan koordine olur.
 */
__attribute__((noreturn))
void ap_isi(uint64_t cekirdek_no) {
    /* Güvenlik: cekirdek_no beklenen aralıkta değilse (asla olmamalı) dokunma. */
    if (cekirdek_no < CEKIRDEK_SAYI) {
        /* 1. Gözlemlenen MPIDR'ı (kimlik kanıtı) kendi slotuna yaz + boşalt.
         *    Canlı bayrağından ÖNCE → BSP canlı görünce MPIDR de görünür. */
        __asm__ volatile("dsb sy" ::: "memory");
        cekirdek_durum[cekirdek_no].mpidr_gozlemlenen = cekirdek_no;
        satir_temizle_gecersiz(&cekirdek_durum[cekirdek_no].mpidr_gozlemlenen);

        /* 2. "Canlıyım" bayrağını en son set et (BSP bunu poll eder). MPIDR
         *    yazımı RAM'e ULAŞTIKTAN sonra → BSP bayrağı görünce MPIDR de. */
        __asm__ volatile("dsb sy" ::: "memory");
        cekirdek_durum[cekirdek_no].canli = 1;
        satir_temizle_gecersiz(&cekirdek_durum[cekirdek_no].canli);
    }

    /* 3. Sonsuz düşük-güç spin. */
    __asm__ volatile("sev" ::: "memory");
    for (;;) {
        __asm__ volatile("wfe" ::: "memory");
    }
}

/*
 * ORTAK AP GİRİŞ noktası — 3 AP'nin de (çekirdek 1,2,3) dallandığı TEK giriş.
 * PSCI CPU_ON tarafından MMU-OFF, EL1'de, UNDEFINED SP ile çağrılır.
 *
 * KRİTİK: SP kurulmadan ÖNCE HİÇBİR C prologue çalışmamalı (undefined SP'ye spill
 * → garbage yazım → sessiz çöküş; D-174 dersi). Naked → compiler prologue/epilogue
 * üretmez; SP'yi ELLE kur.
 *
 * MPIDR-indeksli yığın seçimi (naked içinde, saf asm):
 *   1. mrs x1, mpidr_el1; and x1, x1, #0xFF → çekirdek_no (Aff0; QEMU virt: 1,2,3)
 *   2. yığın TEPESİ = &ap_yiginlar + (çekirdek_no + 1) * AP_YIGIN_BOYUT
 *      (yığın aşağı büyür → dizi[çekirdek_no] bloğunun ÜST sınırı)
 *   3. mov sp, <tepe>
 *   4. mov x0, çekirdek_no (ap_isi'ye 1. argüman) → b ap_isi
 */
__attribute__((naked, noreturn))
static void ortak_ap_giris(void) {
    __asm__ volatile(
        /* --- 1. Kendi MPIDR Aff0'ını (çekirdek numarası) oku. --- */
        "mrs  x1, mpidr_el1\n"
        "and  x1, x1, #0xFF\n"        /* x1 = çekirdek_no (Aff0) */
        /* --- 2. MPIDR-indeksli yığın TEPESİni hesapla. --- */
        "adrp x0, ap_yiginlar\n"
        "add  x0, x0, :lo12:ap_yiginlar\n"   /* x0 = &ap_yiginlar[0] (TABAN) */
        "add  x2, x1, #1\n"           /* x2 = çekirdek_no + 1 */
        "mov  x3, %0\n"               /* x3 = AP_YIGIN_BOYUT */
        "mul  x2, x2, x3\n"           /* x2 = (çekirdek_no+1) * BOYUT */
        "add  x0, x0, x2\n"           /* x0 = o çekirdeğin yığın TEPESİ */
        "mov  sp, x0\n"               /* SP kuruldu (16-byte hizalı: BOYUT ve TABAN 16-hizalı) */
        /* --- 3. Ortak C işine dallan (çekirdek_no 1. argüman = x0). --- */
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

/* Bir AP'yi (target MPIDR = hedef affinity) PSCI CPU_ON ile başlat. Ortak giriş
 * noktasına (ortak_ap_giris) dallanır → AP kendi MPIDR'ından kimliğini bulur.
 * HVC → SMC fallback (smp_queue_arm.c deseni). Döner: PSCI dönüş kodu (0=başarı).
 * `conduit_out` NULL değilse kullanılan conduit adı ("HVC"/"SMC") yazılır. */
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

int main(void) {
    kdl_yazdir_metin("SMP4 BASLA");

    /* --- 1. Çekirdek 1, 2, 3'ü sırayla PSCI CPU_ON ile başlat (ortak giriş).
     *        Her AP target MPIDR affinity'siyle (1,2,3) çağrılır; ortak girişe
     *        dallanır ve kendi MPIDR'ından hangi çekirdek olduğunu bulur. --- */
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
            kdl_yaz_metin("SMP4 EKSIK cekirdek=");
            kdl_yazdir_isaretsiz_tam64(no);
            kdl_yazdir_metin("");
            kdl_yazdir_metin("SMP4 FAIL: CPU_ON basarisiz");
            for (;;) { __asm__ volatile("wfe"); }
        }
    }

    /* --- 2. Her AP'nin (1,2,3) "canlıyım" bayrağını bekle (bounded poll +
     *        cache-coherent). Bir AP zamanında gelmezse hangisi eksik raporla. --- */
    for (uint64_t no = 1; no < CEKIRDEK_SAYI; no++) {
        int canli = 0;
        for (uint64_t bekle = 0; bekle < TIMEOUT_TIK; bekle++) {
            satir_gecersiz(&cekirdek_durum[no].canli);
            if (cekirdek_durum[no].canli != 0) { canli = 1; break; }
        }
        if (!canli) {
            kdl_yaz_metin("SMP4 EKSIK cekirdek=");
            kdl_yazdir_isaretsiz_tam64(no);
            kdl_yazdir_metin("");
            /* Diğerlerinin durumunu da bas (tanı). */
            for (uint64_t d = 1; d < CEKIRDEK_SAYI; d++) {
                satir_gecersiz(&cekirdek_durum[d].canli);
                kdl_yaz_metin("cekirdek ");
                kdl_yaz_onaltilik(d);
                kdl_yaz_metin(" canli=");
                kdl_yazdir_isaretsiz_tam64(cekirdek_durum[d].canli);
            }
            kdl_yazdir_metin("SMP4 FAIL: AP canli-bayragi timeout");
            for (;;) { __asm__ volatile("wfe"); }
        }
    }

    /* --- 3. Her AP'nin gözlemlediği MPIDR'ı cache-coherent oku + bas (kanıt:
     *        farklı çekirdekler; her AP kendi Aff0'ını doğru okumalı). --- */
    int mpidr_dogru = 1;
    for (uint64_t no = 1; no < CEKIRDEK_SAYI; no++) {
        satir_gecersiz(&cekirdek_durum[no].mpidr_gozlemlenen);
        uint64_t mp = cekirdek_durum[no].mpidr_gozlemlenen;
        kdl_yaz_metin("cekirdek ");
        kdl_yaz_onaltilik(no);
        kdl_yaz_metin(" MPIDR-Aff0=");
        kdl_yazdir_isaretsiz_tam64(mp);
        if (mp != no) {
            mpidr_dogru = 0;   /* AP yanlış çekirdek numarası okudu → kimlik bozuk */
        }
    }

    /* --- 4. DOĞRULAMA: 3 AP de canlı VE her biri kendi MPIDR'ını (1,2,3) doğru
     *        okudu → 4 çekirdek (BSP + 3 AP) başarıyla çalışıyor. --- */
    satir_gecersiz(&cekirdek_durum[1].canli);
    satir_gecersiz(&cekirdek_durum[2].canli);
    satir_gecersiz(&cekirdek_durum[3].canli);
    int hepsi_canli = (cekirdek_durum[1].canli != 0) &&
                      (cekirdek_durum[2].canli != 0) &&
                      (cekirdek_durum[3].canli != 0);

    if (hepsi_canli && mpidr_dogru) {
        kdl_yazdir_metin("SMP4 OK 4 cekirdek");
    } else {
        /* Hangi çekirdek eksik/bozuk raporla. */
        for (uint64_t no = 1; no < CEKIRDEK_SAYI; no++) {
            satir_gecersiz(&cekirdek_durum[no].canli);
            if (cekirdek_durum[no].canli == 0) {
                kdl_yaz_metin("SMP4 EKSIK cekirdek=");
                kdl_yazdir_isaretsiz_tam64(no);
                kdl_yazdir_metin("");
            }
        }
        kdl_yazdir_metin("SMP4 FAIL: 4 cekirdek dogrulanamadi");
    }

    for (;;) { __asm__ volatile("wfe"); }
}
