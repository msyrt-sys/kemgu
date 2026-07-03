/*
 * KEMGU-OS bare-metal ÇOK-ÇEKİRDEK BRING-UP testi (x86_64) — 4 ÇEKİRDEK.
 * ==========================================================================
 *
 * Milestone: D-187 (x86 SMP 2-çekirdek) testinin 4-çekirdeğe ÖLÇEKLENMESİ +
 * D-191 (aarch64 4-çekirdek) testinin x86 ikizi/paritesi. D-187'de BSP (çekirdek
 * 0) TEK AP'yi (APIC ID 1) Local APIC INIT-SIPI ile başlattı. Bu test SMP'yi
 * 4 çekirdeğe ölçekler: BSP çekirdek 1, 2 ve 3'ü (APIC ID 1,2,3) ayrı ayrı
 * INIT-SIPI ile başlatır; üçü de long-mode'da canlı olduğunu kanıtlar. Bu
 * "2→4 ölçekleme" milestone'u — çoklu-AP koordinasyonu (ORTAK trampoline +
 * APIC-ID-indeksli yığın + çekirdek-başına canlı-bayrağı) x86'da ilk kez burada.
 *
 * -----------------------------------------------------------------------------
 * x86 çoklu-AP neden aarch64'ten (D-191) DAHA subtle:
 *   aarch64'te PSCI CPU_ON her AP'ye giriş noktası olarak DOĞRUDAN bir 64-bit
 *   giriş adresi verir; her CPU_ON çağrısı bağımsız (hedef MPIDR affinity ile).
 *   x86'da ise:
 *     - Her AP GERÇEK MOD'da başlar → real→protected→long trampoline gerekir.
 *     - INIT-SIPI vektörü (0x08 → 0x8000) düşük belleğe kodlanmış TEK bir
 *       fiziksel adres. 3 AP AYNI SIPI vektörünü paylaşır → HEPSİ AYNI 0x8000
 *       trampoline'inden geçer (ORTAK kod). Ayrışma long-mode'da: her AP KENDİ
 *       APIC ID'sini (LAPIC 0x20 >> 24) okur → kimliğini bulur.
 *     - ORTAK trampoline TEK bir başlangıç yığını verse HEPSİ o yığında yarışır
 *       (call ap_long_giris sırasında torn stack). ÇÖZÜM: long-mode ortak girişi
 *       NAKED — APIC ID'yi okur, APIC-ID-İNDEKSLİ kendi yığınını RSP'ye kurar,
 *       SONRA C işine dallanır. Böylece 3 AP tek koddan geçer ama her biri kendi
 *       izole yığınına sahip (aarch64 `ortak_ap_giris` MPIDR-indeksli deseninin
 *       x86 muadili).
 *
 * -----------------------------------------------------------------------------
 * ORTAK TRAMPOLINE (kritik tasarım): 2-çekirdek testi (smp_x86.c, D-187) tek AP
 * için trampoline'de TEK `stack64` alanına RSP kurdu. 3 AP için AYRI trampoline
 * yazmak (her biri farklı SIPI vektörü + farklı stack alanı) mümkün ama israf.
 * Bunun yerine TEK trampoline + TEK SIPI vektörü (0x08):
 *   - Trampoline'in 64-bit son adımı RSP'yi kurmaz; doğrudan ortak long-mode
 *     naked girişine (ap_ortak_long_giris) atlar.
 *   - Naked giriş: cpuid ile APIC ID oku → RSP = ap_yiginlar + (id+1)*BOYUT →
 *     jmp ap_isi(id). Her AP kendi kimliğini + kendi yığınını bulur.
 * Böylece 3 AP TEK trampoline + TEK naked girişten geçer, her biri ayrışır.
 *
 * -----------------------------------------------------------------------------
 * cache coherency: AP'ler long-mode'da BSP ile AYNI CR3'ü (WB cacheable identity
 * map) kullanır → x86 donanım cache coherency (MESI) OTOMATİK; aarch64'teki
 * manuel `dc civac`/`dc ivac` GEREKMEZ. volatile + mfence yeterli.
 *
 * FALSE-SHARING önlemi: aarch64 (D-191) her canlı-bayrağını ayrı 64-byte cache
 * satırına koydu (dc-civac granülaritesi torn yazımı önlemek için). x86'da
 * donanım coherency olduğundan zorunlu değil ama TASARIM PARİTESİ + gerçek
 * çok-çekirdek yazım deseni için biz de her çekirdek durumunu 64-byte hizalı
 * ayrı satıra koyuyoruz (AP'ler eşzamanlı yazarken bitişik bayraklar aynı satırda
 * olsa cache-hattı ping-pong olurdu; ayrı satır → temiz).
 *
 * -----------------------------------------------------------------------------
 * DOĞRULAMA (dört koşul birden):
 *   (1) cekirdek_canli[1] set  (çekirdek 1 AP gerçekten long-mode'da koştu)
 *   (2) cekirdek_canli[2] set  (çekirdek 2 AP gerçekten long-mode'da koştu)
 *   (3) cekirdek_canli[3] set  (çekirdek 3 AP gerçekten long-mode'da koştu)
 *   (4) her AP kendi APIC ID'sini RAM'e yazdı — 1,2,3 (farklı çekirdek kanıtı)
 *   Dördü de sağlanırsa "SMP4 X86 OK 4 cekirdek". Eksik varsa hangisi raporlanır.
 *
 * DETERMİNİSTİK: 3 AP başlatma bounded (her INIT+SIPI delivery-status ile onaylı);
 * canlı-bayrağı beklemesi bounded poll. Yük/internet bağımsız — her koşuda aynı.
 *
 * FALLBACK: 3 AP hepsi gelmezse kaç AP geldiği raporlanır ("SMP4 X86 KISMI N/3").
 * Hiç AP gelmezse ama LAPIC MMIO + IPI altyapısı çalışıyorsa "APIC OK" (D-187
 * fallback deseni). x86 çoklu-AP timing subtle → seviye NET raporlanır.
 */
#include <stdint.h>

extern void kdl_yazdir_metin(const char *);   /* metin + newline */
extern void kdl_yaz_metin(const char *);       /* metin, newline'siz (inline etiket) */
extern void kdl_yazdir_satir(void);
extern void kdl_yaz_onaltilik(uint64_t);       /* onaltilik, newline'siz */
extern void kdl_yazdir_onaltilik(uint64_t);    /* onaltilik + newline */

static _Noreturn void halt(void) { for (;;) { __asm__ volatile("hlt"); } }

/* === Local APIC MMIO (xAPIC modu) === */
#define LAPIC_TABAN      0xFEE00000ULL
#define LAPIC_ID         0x020u   /* [31:24] = APIC ID */
#define LAPIC_VERSION    0x030u   /* [7:0]=sürüm, [23:16]=max LVT */
#define LAPIC_SIVR       0x0F0u   /* Spurious Interrupt Vector; bit 8 = APIC etkin */
#define LAPIC_ICR_DUSUK  0x300u   /* Interrupt Command Register, düşük 32 bit */
#define LAPIC_ICR_YUKSEK 0x310u   /* Interrupt Command Register, yüksek 32 bit */

/* ICR düşük alan bitleri. */
#define ICR_VEKTOR(v)         ((uint32_t)((v) & 0xFF))
#define ICR_TESLIM_INIT       (0x5u << 8)   /* delivery mode = INIT */
#define ICR_TESLIM_STARTUP    (0x6u << 8)   /* delivery mode = STARTUP (SIPI) */
#define ICR_LEVEL_ASSERT      (1u << 14)    /* level = assert */
#define ICR_TESLIM_DURUM      (1u << 12)    /* delivery status (RO): 1=beklemede */

/* MMIO 32-bit oku/yaz — LAPIC register'ları yalnız 32-bit hizalı erişir. */
static inline uint32_t lapic_oku(uint32_t reg) {
    return *(volatile uint32_t *)(LAPIC_TABAN + reg);
}
static inline void lapic_yaz(uint32_t reg, uint32_t deger) {
    *(volatile uint32_t *)(LAPIC_TABAN + reg) = deger;
}

/*
 * === LAPIC MMIO sayfasını haritala ===
 * Boot page-table'ları (boot/start_x86_64.S) yalnız 0..1 GB'ı identity-map eder.
 * LAPIC tabanı 0xFEE00000 (~3.98 GB) HARİTASIZ → erişince #PF. boot read-only
 * olduğundan haritayı BURADA, çalışma anında kuruyoruz: 0xFEE00000'ı içeren
 * 2 MB'lık bölgeyi identity 2MB-huge page olarak ekle. PDPT[3] (3..4 GB) boot'ta
 * boş → kendi PD'mizi bağlarız. (D-187 smp_x86.c ile aynı.)
 *
 * Sayfa girişi bayrakları: present|write|PS(huge)|PCD(cache-disable)|PWT
 * (LAPIC MMIO uncacheable olmalı → PCD+PWT).
 */
static uint64_t lapic_pd[512] __attribute__((aligned(4096)));

static void lapic_mmio_harita_kur(void) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));

    /* CR3 → PML4 fiziksel taban (identity-map: fiziksel == sanal). */
    uint64_t *pml4 = (uint64_t *)(uintptr_t)(cr3 & ~0xFFFULL);

    /* 0xFEE00000 için PML4/PDPT/PD indeksleri. */
    uint64_t adr = LAPIC_TABAN;                 /* 0xFEE00000 */
    unsigned pml4_i = (adr >> 39) & 0x1FF;      /* = 0 */
    unsigned pdpt_i = (adr >> 30) & 0x1FF;      /* = 3 */
    unsigned pd_i   = (adr >> 21) & 0x1FF;      /* 2MB indeks */

    /* PML4[0] → mevcut PDPT (boot kurdu). Fiziksel tabanını çıkar. */
    uint64_t pml4e = pml4[pml4_i];
    uint64_t *pdpt = (uint64_t *)(uintptr_t)(pml4e & 0x000FFFFFFFFFF000ULL);

    /* PDPT[3]'e kendi PD'mizi bağla (present|write). */
    pdpt[pdpt_i] = ((uint64_t)(uintptr_t)lapic_pd & 0x000FFFFFFFFFF000ULL) | 0x3ULL;

    /* PD[pd_i] = 2MB huge page, taban = 0xFEE00000'ın 2MB-hizalı hâli,
       bayraklar present|write|PS|PCD|PWT. */
    uint64_t sayfa_taban = adr & ~0x1FFFFFULL;   /* 2MB-hizala */
    lapic_pd[pd_i] = sayfa_taban | 0x9BULL;       /* P|RW|PWT(0x8)|PCD(0x10)|PS(0x80) = 0x9B */

    /* TLB'yi temizle (yeni harita görünsün) — CR3 yeniden yükle. */
    __asm__ volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
}

/* ICR delivery-status (bit 12) temizlenene kadar bekle (bounded). Dönüş:
 * 0 = temizlendi (IPI kabul edildi), 1 = timeout (hâlâ beklemede). */
static int icr_bekle(void) {
    for (int i = 0; i < 1000000; i++) {
        if ((lapic_oku(LAPIC_ICR_DUSUK) & ICR_TESLIM_DURUM) == 0) {
            return 0;
        }
    }
    return 1;
}

/* ICR yaz: önce yüksek (hedef APIC ID [31:24]), sonra düşük (yazım IPI'yi
 * tetikler). */
static void ipi_gonder(uint8_t hedef_apic_id, uint32_t icr_dusuk) {
    lapic_yaz(LAPIC_ICR_YUKSEK, (uint32_t)hedef_apic_id << 24);
    lapic_yaz(LAPIC_ICR_DUSUK, icr_dusuk);
}

/*
 * Kaba gecikme — INIT ile SIPI arasında (Intel: INIT sonrası ~10 ms, SIPI'ler
 * arası ~200 µs). Bare-metal'de kalibre saat yok → port 0x80 (POST) I/O ile
 * yaklaşık gecikme (her outb ~1 µs QEMU'da; sayı büyük tutuldu, deterministik).
 */
static void kaba_gecikme(int tur) {
    for (int i = 0; i < tur; i++) {
        __asm__ volatile("outb %%al, $0x80" : : "a"((uint8_t)0) : "memory");
    }
}

#define CEKIRDEK_SAYI  4u                  /* toplam çekirdek: BSP 0 + AP 1,2,3 */
#define AP_YIGIN_BOYUT 16384u              /* her çekirdek için ayrı 16 KB yığın */
/* imul immediate için AP_YIGIN_BOYUT'un string hâli (naked asm template'e gömülür). */
#define AP_YIGIN_BOYUT_STR "16384"

/* === Paylaşılan çok-çekirdek durumu ===
 * Her çekirdek durumu KENDİ 64-byte cache satırında (false-sharing/torn önlemi).
 * x86 donanım cache coherency (MESI) otomatik → aarch64'teki manuel dc-civac
 * gerekmez, ama 3 AP eşzamanlı yazarken bitişik bayraklar aynı satırda olsa
 * cache-hattı ping-pong olurdu; ayrı satır → temiz (D-191 tasarım paritesi).
 *
 * canli: o çekirdek long-mode'da koştu mu (0/1). apic_id_gozlemlenen: o
 * çekirdeğin LAPIC 0x20'den okuduğu KENDİ APIC ID'si (1,2,3 beklenir — farklı
 * çekirdek kanıtı). */
typedef struct {
    volatile uint32_t canli;
    volatile uint32_t apic_id_gozlemlenen;
    volatile uint8_t  dolgu[64 - 2u * sizeof(uint32_t)];  /* satırı 64 byte'a tamamla */
} __attribute__((aligned(64))) CekirdekDurum;

/* Çekirdek-başına durum dizisi — her slot kendi cache satırında. Indeks = APIC
 * ID. [0]=BSP (kullanılmaz), [1..3]=AP'ler. */
static CekirdekDurum cekirdek_durum[CEKIRDEK_SAYI] __attribute__((aligned(64)));

/* Tüm AP yığınları TEK dizi — APIC-ID-indeksli. TABAN + (id+1)*BOYUT = o
 * çekirdeğin TEPESİ (yığın aşağı büyür). Çekirdek 0 (BSP) girişe hiç girmez ama
 * hizalama için 0. blok da ayrılır (kullanılmaz). Dış-bağ (non-static) → naked
 * ortak giriş asm'den sembol adıyla eriştiği için linker emit etmeli (aksi halde
 * "unused" atılır). 16-byte hizalı (x86_64 SysV ABI SP hizalama gereği). */
uint8_t ap_yiginlar[CEKIRDEK_SAYI * AP_YIGIN_BOYUT] __attribute__((aligned(16)));

/*
 * Bir AP'nin asıl işi (C) — RSP zaten kurulu çağrılır (ortak naked girişten).
 * `apic_id` = ortak girişin LAPIC 0x20'den okuduğu APIC ID (1,2,3).
 * UART'a DOKUNMA (BSP ile paylaşımlı, senkronizasyonsuz → çakışma). Yalnız
 * paylaşılan RAM'e yaz.
 *
 * Bu fonksiyon 3 AP'nin ORTAK kod yoludur: her AP kendi apic_id'siyle girer,
 * kendi durum-slotunu (cekirdek_durum[apic_id]) set eder. Aynı kod, farklı slot
 * → çoklu-AP tek koddan koordine olur.
 */
__attribute__((noreturn))
void ap_isi(uint64_t apic_id) {
    /* Güvenlik: apic_id beklenen aralıkta değilse (asla olmamalı) dokunma. */
    if (apic_id < CEKIRDEK_SAYI) {
        /* 1. Gözlemlenen APIC ID'yi (kimlik kanıtı) kendi slotuna yaz.
         *    Canlı bayrağından ÖNCE → BSP canlı görünce APIC ID de görünür.
         *    x86 MESI coherency → mfence ile sıralama yeterli (dc-civac yok). */
        cekirdek_durum[apic_id].apic_id_gozlemlenen = (uint32_t)apic_id;
        __asm__ volatile("mfence" ::: "memory");

        /* 2. "Canlıyım" bayrağını en son set et (BSP bunu poll eder). APIC ID
         *    yazımı görünür olduktan SONRA → BSP bayrağı görünce APIC ID de. */
        cekirdek_durum[apic_id].canli = 1;
        __asm__ volatile("mfence" ::: "memory");
    }

    /* 3. Sonsuz durak (hlt). */
    halt();
}

/*
 * === ORTAK LONG-MODE GİRİŞ (naked) ===
 * Trampoline'in 64-bit son adımı buraya (mutlak 64-bit adres) atlar. 3 AP'nin de
 * (APIC ID 1,2,3) dallandığı TEK giriş. RSP HENÜZ kurulu DEĞİL (trampoline
 * kurmadı — ortak yığın yarışını önlemek için).
 *
 * KRİTİK: RSP kurulmadan ÖNCE HİÇBİR C prologue çalışmamalı (undefined/paylaşımlı
 * RSP'ye spill → çöküş/torn). Naked → compiler prologue/epilogue üretmez; RSP'yi
 * ELLE kur (aarch64 `ortak_ap_giris` MPIDR-indeksli deseninin x86 muadili).
 *
 * APIC-ID-indeksli yığın seçimi (naked içinde, saf asm — leaf, stack gerekmez):
 *   1. LAPIC ID register (MMIO 0xFEE00020) oku; [31:24] = APIC ID (rcx).
 *      LAPIC MMIO BSP tarafından zaten haritalandı + AP aynı CR3'ü kullanır →
 *      okuma güvenli.
 *   2. yığın TEPESİ = &ap_yiginlar + (apic_id + 1) * AP_YIGIN_BOYUT
 *      (yığın aşağı büyür → dizi[apic_id] bloğunun ÜST sınırı)
 *   3. mov rsp, <tepe>  (16-byte hizalı: BOYUT ve TABAN 16-hizalı)
 *   4. mov rdi, apic_id (ap_isi'ye 1. argüman, SysV ABI) → jmp ap_isi
 *
 * NOT: RIP-relative `lea` yerine mutlak `movabs` kullanılamaz (linker sembol
 * adresini bilir; identity-map'te sanal==fiziksel). `ap_yiginlar` ve `ap_isi`
 * dış-bağ semboller — assembler RIP-relative çözer, identity-map'te doğru.
 */
__attribute__((naked, noreturn))
static void ap_ortak_long_giris(void) {
    __asm__ volatile(
        /* --- 1. LAPIC ID register (0xFEE00020) oku → APIC ID (rcx). --- */
        "mov  $0xFEE00020, %%eax\n"      /* LAPIC_TABAN + LAPIC_ID (düşük 32 bit adres) */
        "mov  (%%rax), %%ecx\n"          /* ecx = LAPIC ID register ham değeri */
        "shr  $24, %%ecx\n"              /* ecx = APIC ID ([31:24] → [7:0]) */
        "and  $0xFF, %%ecx\n"            /* rcx = APIC ID (0..255) */
        /* --- 2. APIC-ID-indeksli yığın TEPESİni hesapla. --- */
        "lea  ap_yiginlar(%%rip), %%rax\n"   /* rax = &ap_yiginlar[0] (TABAN) */
        "lea  1(%%rcx), %%rdx\n"         /* rdx = apic_id + 1 */
        "imul $" AP_YIGIN_BOYUT_STR ", %%rdx, %%rdx\n"  /* rdx = (apic_id+1)*BOYUT */
        "add  %%rdx, %%rax\n"            /* rax = o çekirdeğin yığın TEPESİ */
        "mov  %%rax, %%rsp\n"            /* RSP kuruldu (16-byte hizalı) */
        /* --- 3. Ortak C işine dallan (apic_id 1. argüman = rdi). --- */
        "mov  %%rcx, %%rdi\n"            /* rdi = apic_id → ap_isi(apic_id) */
        "jmp  ap_isi\n"                  /* RSP kurulu → C işine dallan (noreturn) */
        ::: "memory");
}

#define TRAMBOLIN_ADRES  0x8000u        /* SIPI vektör 0x08 → 0x08<<12 = 0x8000 */
#define SIPI_VEKTOR      0x08u

int main(void) {
    kdl_yazdir_metin("SMP4 X86 BASLA");
    kdl_yazdir_satir();

    /* --- 1. LAPIC MMIO sayfasını haritala (boot yalnız 0..1GB haritalar) --- */
    lapic_mmio_harita_kur();

    /* --- 2. Local APIC MMIO erişimini kanıtla --- */
    uint32_t bsp_apic_id = (lapic_oku(LAPIC_ID) >> 24) & 0xFFu;
    uint32_t apic_ver_ham = lapic_oku(LAPIC_VERSION);
    uint32_t apic_surum   = apic_ver_ham & 0xFFu;
    uint32_t apic_max_lvt = (apic_ver_ham >> 16) & 0xFFu;
    uint32_t sivr         = lapic_oku(LAPIC_SIVR);

    kdl_yaz_metin("BSP APIC_ID=");
    kdl_yaz_onaltilik(bsp_apic_id);
    kdl_yaz_metin(" APIC_VER=");
    kdl_yaz_onaltilik(apic_surum);
    kdl_yaz_metin(" MAX_LVT=");
    kdl_yaz_onaltilik(apic_max_lvt);
    kdl_yaz_metin(" SIVR=");
    kdl_yazdir_onaltilik(sivr);

    /* APIC'i yazılım-etkin yap (SIVR bit 8) — IPI göndermek için gerekli. */
    lapic_yaz(LAPIC_SIVR, sivr | 0x100u | 0xFFu);

    int lapic_erisilebilir = (apic_ver_ham != 0 && apic_ver_ham != 0xFFFFFFFFu);

    /* --- 3. ORTAK AP trampoline'ini düşük belleğe kur (0x8000) --- *
     * D-187 smp_x86.c ile AYNI trampoline gövdesi, TEK FARK: 64-bit son adım
     * RSP'yi kurmaz + ap_long_giris yerine ORTAK naked girişe (ap_ortak_long_giris)
     * atlar. Böylece 3 AP AYNI trampoline'den geçer; ayrışma naked girişte (her
     * AP kendi APIC ID'sini okur → kendi yığını + kendi slotu).
     *
     * Blob düzeni (ofsetler TRAMBOLIN_ADRES'e göre):
     *   0x00  16-bit giriş (real mode)
     *   ...   32-bit protected mode kodu
     *   ...   64-bit long mode kodu (RSP kurmaz → doğrudan ortak girişe jmp)
     *   veri: gdt (yerel), gdt_ptr32, cr3_degeri, giris64 (ortak naked adres)
     */
    volatile uint8_t *T = (volatile uint8_t *)(uintptr_t)TRAMBOLIN_ADRES;

    /* BSP'nin CR3'ünü oku (AP aynı identity-map PML4'ü kullanacak). */
    uint64_t bsp_cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(bsp_cr3));

    int i = 0;
    #define B(x) (T[i++] = (uint8_t)(x))
    #define ADR16(ofs)  ((uint16_t)(TRAMBOLIN_ADRES + (ofs)))   /* 16-bit mutlak */
    #define ADR32(ofs)  ((uint32_t)(TRAMBOLIN_ADRES + (ofs)))   /* 32-bit mutlak */

    /* Veri alanları (kod bölümünden sonra; kod ~0x80 byte → 0x100'de başlat). */
    const int OFS_GDT      = 0x100;  /* yerel GDT (5 descriptor × 8 = 40 byte) → 0x128'e kadar */
    const int OFS_GDTPTR32 = 0x128;  /* 6-byte: limit(2) + taban(4) → 0x12E */
    const int OFS_CR3      = 0x138;  /* 4-byte → 0x13C */
    const int OFS_GIRIS64  = 0x140;  /* 8-byte: ap_ortak_long_giris mutlak adres → 0x148 */

    /* ---- 16-bit real mode (ofset 0x00) ---- */
    /* cli */                         B(0xFA);
    /* cld */                         B(0xFC);
    /* xor ax,ax : 31 C0 */           B(0x31); B(0xC0);
    /* mov ds,ax */                   B(0x8E); B(0xD8);
    /* mov es,ax */                   B(0x8E); B(0xC0);
    /* mov ss,ax */                   B(0x8E); B(0xD0);
    /* lgdtl [gdt_ptr32] — 66 0F 01 16 <disp16> (32-bit lgdt, disp16 mutlak) */
    B(0x66); B(0x0F); B(0x01); B(0x16); {
        uint16_t a = ADR16(OFS_GDTPTR32);
        B(a & 0xFF); B((a >> 8) & 0xFF);
    }
    /* mov eax,cr0 : 0F 20 C0 */      B(0x0F); B(0x20); B(0xC0);
    /* or eax,1 (PE) : 66 83 C8 01 */ B(0x66); B(0x83); B(0xC8); B(0x01);
    /* mov cr0,eax : 0F 22 C0 */      B(0x0F); B(0x22); B(0xC0);
    /* ljmpl 0x08:pm32 — 66 EA <off32> <sel16> */
    B(0x66); B(0xEA);
    int yama_pm32 = i; B(0); B(0); B(0); B(0);   /* off32 (pm32 mutlak, sonra doldur) */
    B(0x08); B(0x00);                             /* selector 0x08 (32-bit kod) */

    /* ---- 32-bit protected mode ---- */
    int ofs_pm32 = i;
    /* mov eax,0x10 : B8 10 00 00 00 */ B(0xB8); B(0x10); B(0x00); B(0x00); B(0x00);
    /* mov ds,ax : 8E D8 */            B(0x8E); B(0xD8);
    /* mov es,ax : 8E C0 */            B(0x8E); B(0xC0);
    /* mov ss,ax : 8E D0 */            B(0x8E); B(0xD0);
    /* mov fs,ax : 8E E0 */            B(0x8E); B(0xE0);
    /* mov gs,ax : 8E E8 */            B(0x8E); B(0xE8);
    /* mov eax,[cr3_degeri] : A1 <disp32> */
    B(0xA1); { uint32_t a = ADR32(OFS_CR3); B(a & 0xFF); B((a>>8)&0xFF); B((a>>16)&0xFF); B((a>>24)&0xFF); }
    /* mov cr3,eax : 0F 22 D8 */       B(0x0F); B(0x22); B(0xD8);
    /* mov eax,cr4 : 0F 20 E0 */       B(0x0F); B(0x20); B(0xE0);
    /* or eax,0x20 (PAE bit5) : 83 C8 20 */ B(0x83); B(0xC8); B(0x20);
    /* mov cr4,eax : 0F 22 E0 */       B(0x0F); B(0x22); B(0xE0);
    /* mov ecx,0xC0000080 (EFER) : B9 80 00 00 C0 */ B(0xB9); B(0x80); B(0x00); B(0x00); B(0xC0);
    /* rdmsr : 0F 32 */                B(0x0F); B(0x32);
    /* or eax,0x100 (LME bit8) : 0D 00 01 00 00 */ B(0x0D); B(0x00); B(0x01); B(0x00); B(0x00);
    /* wrmsr : 0F 30 */                B(0x0F); B(0x30);
    /* mov eax,cr0 : 0F 20 C0 */       B(0x0F); B(0x20); B(0xC0);
    /* or eax,0x80000000 (PG bit31) : 0D 00 00 00 80 */ B(0x0D); B(0x00); B(0x00); B(0x00); B(0x80);
    /* mov cr0,eax : 0F 22 C0 */       B(0x0F); B(0x22); B(0xC0);
    /* ljmpl 0x18:lm64 — EA <off32> <sel16> (selector 0x18 = 64-bit kod) */
    B(0xEA);
    int yama_lm64 = i; B(0); B(0); B(0); B(0);   /* off32 (lm64 mutlak) */
    B(0x18); B(0x00);                             /* selector 0x18 (64-bit kod) */

    /* ---- 64-bit long mode ---- *
     * D-187'den FARK: RSP kurulmaz (ortak yığın yarışını önlemek için). Doğrudan
     * ortak naked girişe (ap_ortak_long_giris) atlar; O giriş APIC-ID-indeksli
     * kendi RSP'sini kurar. Burada yalnız veri segmentlerini temizle + jmp. */
    int ofs_lm64 = i;
    /* mov eax,0x20 : B8 20 00 00 00 */ B(0xB8); B(0x20); B(0x00); B(0x00); B(0x00);
    /* mov ds,ax : 8E D8 */            B(0x8E); B(0xD8);
    /* mov es,ax : 8E C0 */            B(0x8E); B(0xC0);
    /* mov ss,ax : 8E D0 */            B(0x8E); B(0xD0);
    /* mov fs,ax : 8E E0 */            B(0x8E); B(0xE0);
    /* mov gs,ax : 8E E8 */            B(0x8E); B(0xE8);
    /* mov rax,[giris64] : 48 A1 <off64> (ortak naked giriş mutlak 64-bit adres) */
    B(0x48); B(0xA1);
    { uint64_t a = (uint64_t)ADR32(OFS_GIRIS64);
      for (int k = 0; k < 8; k++) B((a >> (8*k)) & 0xFF); }
    /* jmp rax : FF E0 (ortak girişe atla; RSP orada kurulur) */
    B(0xFF); B(0xE0);
    /* hlt (ulaşılmaz ama güvenlik) : F4 */ B(0xF4);
    /* jmp $-1 : EB FE */              B(0xEB); B(0xFE);

    int kod_sonu = i;

    /* --- Yama: pm32 ve lm64 mutlak ofsetleri --- */
    { uint32_t a = ADR32(ofs_pm32);
      T[yama_pm32+0]=a&0xFF; T[yama_pm32+1]=(a>>8)&0xFF; T[yama_pm32+2]=(a>>16)&0xFF; T[yama_pm32+3]=(a>>24)&0xFF; }
    { uint32_t a = ADR32(ofs_lm64);
      T[yama_lm64+0]=a&0xFF; T[yama_lm64+1]=(a>>8)&0xFF; T[yama_lm64+2]=(a>>16)&0xFF; T[yama_lm64+3]=(a>>24)&0xFF; }

    /* Kod bölümü OFS_GDT'yi aşmamalı (aşarsa veri kodu ezer). */
    int kod_sigdi = (kod_sonu <= OFS_GDT);

    /* --- Veri alanları --- */
    /* Yerel GDT: [0]=null, [1]=0x08 32-bit kod, [2]=0x10 32-bit veri,
       [3]=0x18 64-bit kod, [4]=0x20 64-bit veri. 5 descriptor. */
    {
        volatile uint8_t *g = T + OFS_GDT;
        uint64_t gdt[5];
        gdt[0] = 0x0000000000000000ULL;                 /* null */
        gdt[1] = 0x00CF9A000000FFFFULL;                 /* 0x08: 32-bit kod */
        gdt[2] = 0x00CF92000000FFFFULL;                 /* 0x10: 32-bit veri */
        gdt[3] = 0x00209A0000000000ULL;                 /* 0x18: 64-bit kod */
        gdt[4] = 0x0000920000000000ULL;                 /* 0x20: 64-bit veri */
        for (int d = 0; d < 5; d++)
            for (int k = 0; k < 8; k++)
                g[d*8 + k] = (uint8_t)((gdt[d] >> (8*k)) & 0xFF);
    }

    /* gdt_ptr32: limit (2 byte) = 40-1, taban (4 byte) = ADR32(OFS_GDT). */
    {
        volatile uint8_t *p = T + OFS_GDTPTR32;
        uint16_t limit = (uint16_t)(5*8 - 1);
        uint32_t taban = ADR32(OFS_GDT);
        p[0] = limit & 0xFF; p[1] = (limit>>8)&0xFF;
        p[2] = taban & 0xFF; p[3] = (taban>>8)&0xFF; p[4] = (taban>>16)&0xFF; p[5] = (taban>>24)&0xFF;
    }
    /* CR3 (32-bit; PML4 fiziksel < 4 GB). */
    {
        volatile uint8_t *p = T + OFS_CR3;
        uint32_t c = (uint32_t)bsp_cr3;
        p[0]=c&0xFF; p[1]=(c>>8)&0xFF; p[2]=(c>>16)&0xFF; p[3]=(c>>24)&0xFF;
    }
    /* giris64: ap_ortak_long_giris mutlak 64-bit adres (ORTAK naked giriş). */
    {
        volatile uint8_t *p = T + OFS_GIRIS64;
        uint64_t a = (uint64_t)(uintptr_t)&ap_ortak_long_giris;
        for (int k = 0; k < 8; k++) p[k] = (uint8_t)((a >> (8*k)) & 0xFF);
    }

    /* Yazımların RAM'e ulaştığından emin ol (AP'ler okumadan önce). */
    __asm__ volatile("mfence" ::: "memory");

    kdl_yaz_metin("trampolin_kod_bytes=");
    kdl_yaz_onaltilik((uint64_t)kod_sonu);
    kdl_yaz_metin(" kod_sigdi=");
    kdl_yazdir_onaltilik((uint64_t)kod_sigdi);

    /* --- 4. Her AP (APIC ID 1,2,3) için INIT-SIPI-SIPI dizisi --- *
     * Hedef AP APIC ID = 1,2,3 (QEMU -smp 4: BSP=0, AP=1,2,3). ICR yüksek
     * [31:24]=hedef. Her AP AYNI SIPI vektörünü (0x08 → 0x8000 ORTAK trampoline)
     * paylaşır; ayrışma long-mode ortak girişte. */
    int baslatilan_ap = 0;   /* INIT+SIPI başarıyla gönderilen AP sayısı */

    if (lapic_erisilebilir) {
        for (uint8_t hedef = 1; hedef < (uint8_t)CEKIRDEK_SAYI; hedef++) {
            /* INIT IPI (assert). */
            ipi_gonder(hedef, ICR_TESLIM_INIT | ICR_LEVEL_ASSERT);
            int init_ok = (icr_bekle() == 0);
            kaba_gecikme(20000);   /* ~10 ms hedefli kaba bekleme */

            /* STARTUP IPI ×2 (vektör = 0x08 → 0x8000 ORTAK trampoline). */
            ipi_gonder(hedef, ICR_TESLIM_STARTUP | ICR_VEKTOR(SIPI_VEKTOR));
            int s1 = (icr_bekle() == 0);
            kaba_gecikme(400);     /* ~200 µs */
            ipi_gonder(hedef, ICR_TESLIM_STARTUP | ICR_VEKTOR(SIPI_VEKTOR));
            int s2 = (icr_bekle() == 0);

            if (init_ok && (s1 || s2)) {
                baslatilan_ap++;
            }
            kdl_yaz_metin("INIT-SIPI hedef_apic=");
            kdl_yaz_onaltilik((uint64_t)hedef);
            kdl_yaz_metin(" init=");
            kdl_yaz_onaltilik((uint64_t)init_ok);
            kdl_yaz_metin(" sipi=");
            kdl_yazdir_onaltilik((uint64_t)(s1 || s2));
        }
    }

    /* --- 5. Her AP'nin (1,2,3) "canlıyım" bayrağını bekle (bounded poll). --- */
    int canli_sayi = 0;
    for (uint32_t no = 1; no < CEKIRDEK_SAYI; no++) {
        int canli = 0;
        for (volatile uint64_t bekle = 0; bekle < 100000000ULL; bekle++) {
            if (cekirdek_durum[no].canli != 0) { canli = 1; break; }
        }
        if (canli) {
            canli_sayi++;
        }
    }

    /* --- 6. Her AP'nin gözlemlediği APIC ID'yi oku + bas (kanıt: farklı
     *        çekirdekler; her AP kendi APIC ID'sini doğru okumalı). --- */
    __asm__ volatile("mfence" ::: "memory");
    int apic_id_dogru = 1;
    for (uint32_t no = 1; no < CEKIRDEK_SAYI; no++) {
        uint32_t gozlem = cekirdek_durum[no].apic_id_gozlemlenen;
        kdl_yaz_metin("AP APIC_ID gozlem[");
        kdl_yaz_onaltilik((uint64_t)no);
        kdl_yaz_metin("]=");
        kdl_yazdir_onaltilik((uint64_t)gozlem);
        if (cekirdek_durum[no].canli != 0 && gozlem != no) {
            apic_id_dogru = 0;   /* AP yanlış APIC ID okudu → kimlik bozuk */
        }
    }

    /* --- 7. DOĞRULAMA + rapor --- */
    kdl_yaz_metin("baslatilan_ap=");
    kdl_yaz_onaltilik((uint64_t)baslatilan_ap);
    kdl_yaz_metin(" canli_ap=");
    kdl_yazdir_onaltilik((uint64_t)canli_sayi);

    if (canli_sayi == 3 && apic_id_dogru) {
        /* 3 AP de long-mode'a ulaştı + kendi APIC ID'sini doğru okudu → 4
         * çekirdek (BSP + 3 AP) başarıyla çalışıyor. */
        kdl_yazdir_metin("SMP4 X86 OK 4 cekirdek (BSP + 3 AP INIT-SIPI long-mode, APIC ID 1/2/3)");
    } else if (canli_sayi >= 1) {
        /* KISMI: bazı AP'ler geldi ama 3'ü değil. Kaç geldiğini + hangileri
         * eksik raporla (x86 çoklu-AP timing subtle). */
        kdl_yaz_metin("SMP4 X86 KISMI ");
        kdl_yaz_onaltilik((uint64_t)canli_sayi);
        kdl_yazdir_metin("/3 AP long-mode'a ulasti");
        for (uint32_t no = 1; no < CEKIRDEK_SAYI; no++) {
            if (cekirdek_durum[no].canli == 0) {
                kdl_yaz_metin("  EKSIK cekirdek APIC_ID=");
                kdl_yazdir_onaltilik((uint64_t)no);
            }
        }
    } else if (lapic_erisilebilir && baslatilan_ap > 0) {
        /* FALLBACK: hiç AP long-mode'a ulaşmadı ama LAPIC MMIO + IPI altyapısı
         * çalışıyor (D-187 fallback deseni). */
        kdl_yaz_metin("APIC OK (LAPIC MMIO + INIT-SIPI altyapisi calisiyor; AP long-mode teyidi yok) BSP_APIC_ID=");
        kdl_yaz_onaltilik((uint64_t)bsp_apic_id);
        kdl_yaz_metin(" APIC_VER=");
        kdl_yazdir_onaltilik((uint64_t)apic_surum);
    } else {
        kdl_yazdir_metin("SMP4 X86 BASARISIZ (LAPIC MMIO erisilemedi)");
    }

    halt();
    return 0;
}
