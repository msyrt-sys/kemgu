/*
 * KEMGU-OS bare-metal TAM SAYFA-İZOLASYON testi (x86_64) — D-124 x86 muadili.
 * ==========================================================================
 *
 * Milestone: aarch64 D-124/D3 (EL0 kod kernel-belleğe erişince permission-fault)
 * testinin x86 ikizi/paritesi. D-190 (ring3_x86.c) yalnız CPL + ayrıcalıklı-komut
 * (#GP) ile ayrıcalık ayrımını kanıtladı; SAYFA-tabanlı user/kernel izolasyonu
 * KURMADI (kernel sayfaları kısmen U/S-eklenmemiş bırakıldı, ama kanıtlanmadı).
 *
 * Bu test o boşluğu kapatır: ring3 (CPL=3) kod bir KERNEL-ONLY sayfayı (U/S=0)
 * OKUMAYA çalışınca CPU #PF (vektör 14, hata kodu = present|user) üretir → kernel
 * handler yakalar → "PAGE ISO GP". Bu, donanım-zorlamalı SAYFA-DÜZEYİ izolasyonun
 * KANITI: ring3 kernel belleğini fiziksel olarak okuyamaz.
 *
 * -----------------------------------------------------------------------------
 * x86-64 sayfa-izolasyon mekaniği:
 *   Her sayfa-tablosu girişinde U/S biti (bit 2) vardır. CPL=3 (ring3) yalnız
 *   sayfa-yürüyüşünün HER seviyesinde (PML4→PDPT→PD[→PT]) U/S=1 olan sayfalara
 *   erişebilir. Herhangi bir seviyede U/S=0 → erişim #PF (v=14). Hata kodunda:
 *     bit 0 (P)   : 1 = koruma ihlali (sayfa mevcut ama izin yok), 0 = yok-sayfa
 *     bit 1 (W/R) : 1 = yazma, 0 = okuma
 *     bit 2 (U/S) : 1 = CPL=3 (kullanıcı) erişimi, 0 = supervisor
 *   Ring3'ün supervisor-only present sayfayı OKUMASI → hata kodu = P|U = 0b101 = 5.
 *
 * Boot sayfa tabloları (boot/start_x86_64.S) düşük 1 GB'ı 512×2MB huge page ile
 * identity-map eder; HEPSİ supervisor-only (PD bayrak=0x83, U/S yok; PML4/PDPT
 * bayrak=0x3). Kernel kod/veri + tüm test verisi bu 1 GB içinde.
 *
 * TAM İZOLASYON stratejisi (2MB granülerlik):
 *   Sayfa-izni yürüyüşünde asıl per-sayfa ayrım PD SEVİYESİNDE (2MB huge page).
 *   Ring3'ün erişmesi gereken sayfalara (ring3 kod, user stack, paylaşılan
 *   volatile'lar) U/S ekleriz; DİĞER TÜM 2MB PD girişleri supervisor-only KALIR.
 *   "kernel-sır" tamponunu 2MB-HİZALI + 2MB-BOYUTLU yaparız → kendi PD girişini
 *   TAM işgal eder, ring3 sayfalarıyla ASLA çakışmaz → ona U/S EKLEMEYİZ →
 *   ring3 okuması #PF. Bu, 2MB-granülerlikte GERÇEK sayfa-düzeyi izolasyon.
 *
 * -----------------------------------------------------------------------------
 * Akış:
 *   1. "PAGE ISO BASLA" bas.
 *   2. GDT (ring3 user seg + TSS descriptor) + TSS (RSP0) + IDT (int 0x80 DPL=3,
 *      #GP DPL=0, #PF DPL=0) kur.
 *   3. Kernel-sır tamponunu 2MB-hizalı ayrı sayfaya yaz (sihirli değer).
 *   4. YALNIZ ring3 sayfalarına U/S ekle (kod + user stack + paylaşılanlar).
 *      Kernel-sır sayfasına U/S EKLEME → supervisor-only kalır.
 *   5. iretq ile ring3'e geç. ring3_kod (CPL=3):
 *        a. CS oku → RPL=3 doğrula.
 *        b. KENDİ user sayfasına eriş (paylaşılan volatile oku) → OK.
 *        c. int 0x80 (izinli syscall) → kernel handler ring0'da.
 *        d. Kernel-sır sayfasını OKU → #PF (v=14, err=P|U=5) → handler yakalar
 *           → r3_pf_yakalandi=1, err kodu + CR2 kaydet → kernel_devam'a dallanır.
 *   6. Kanıtlar toplandıysa "PAGE ISO OK".
 *
 * -----------------------------------------------------------------------------
 * FALLBACK: Tam izolasyon (yalnız ring3 sayfalarına U/S) boot'u bozarsa (ör.
 * ring3 kodu + kernel-sır aynı 2MB sayfada ve ayrıştırılamıyorsa), en azından
 * TEK kernel-sır sayfasını U/S=0 tutup ring3'ün onu okuyunca #PF aldığını
 * kanıtla → yine "PAGE ISO OK" (kısmi izolasyon ama gerçek #PF). Ulaşılan
 * seviye ("tam-izolasyon" / "tek-sayfa") rapora yazılır.
 *
 * Kanıt: "PAGE ISO OK"  → ring3 kernel-only sayfayı okuyamadı (#PF v=14, err=P|U).
 *        "PAGE ISO GP"   → #PF yakalandı ara-kanıtı (handler tetiklendi).
 *
 * KISIT: Tüm GDT/TSS/IDT/sayfa/ring3 mantığı test-içi inline asm (smp_x86.c +
 * ring3_x86.c deseni). runtime/boot/linker'a DOKUNULMAZ — kernel'in kendi
 * IDT'sini (kdl_idt_kur) GEÇİCİ olarak KENDİ IDT'mizle değiştiririz (lidt), test
 * bitince önemsiz (halt).
 */
#include <stdint.h>

extern void kdl_yazdir_metin(const char *);    /* metin + newline */
extern void kdl_yaz_metin(const char *);        /* metin, newline'siz */
extern void kdl_yazdir_satir(void);
extern void kdl_yaz_onaltilik(uint64_t);        /* onaltilik, newline'siz */
extern void kdl_yazdir_onaltilik(uint64_t);     /* onaltilik + newline */

static _Noreturn void halt(void) { for (;;) { __asm__ volatile("hlt"); } }

/* === Segment seçicileri === */
#define SEL_KERNEL_KOD  0x08u
#define SEL_KERNEL_VERI 0x10u
#define SEL_USER_KOD    (0x18u | 3u)   /* RPL=3 */
#define SEL_USER_VERI   (0x20u | 3u)   /* RPL=3 */
#define SEL_TSS         0x28u

/* === Paylaşılan durum (ring3 kod ↔ kernel handler) ===
 * volatile: derleyici cache'lemesin. Bu test SAYFA-tabanlı user/kernel
 * izolasyonu KURAR: bu paylaşılan volatile'lar ring3-erişilir sayfalarda (U/S=1),
 * kernel-sır ise supervisor-only sayfada (U/S=0). */
static volatile uint32_t r3_cs_rpl        = 0xFFFFFFFFu;  /* ring3'te okunan CS & 3 */
static volatile uint32_t r3_user_okudu    = 0;            /* kendi user sayfasını okudu (OK) */
static volatile uint32_t r3_syscall_geldi = 0;            /* int 0x80 handler tetiklendi */
static volatile uint32_t r3_syscall_num   = 0;            /* syscall numarası (rax) */
static volatile uint32_t r3_pf_yakalandi  = 0;            /* #PF (kernel-sır okuma) yakalandı */
static volatile uint64_t r3_pf_err        = 0;            /* #PF hata kodu (P|W|U bitleri) */
static volatile uint64_t r3_pf_cr2        = 0;            /* #PF fault adresi (CR2) */

/* === Kernel ring0 stack (ring3→ring0 trap'te TSS.RSP0 buraya işaret eder) === */
static uint8_t kernel_ring0_yigin[16384] __attribute__((aligned(16)));

/* === Ring3 kullanıcı yığını === */
static uint8_t user_yigin[16384] __attribute__((aligned(16)));

/* === KERNEL-SIR sayfası (supervisor-only, ring3 OKUYAMAZ) ===
 * KRİTİK: 2MB-HİZALI + 2MB-BOYUTLU → boot'un 2MB huge page düzeninde KENDİ PD
 * girişini TAM işgal eder. Böylece bu sayfaya U/S EKLEMEYİNCE (ve ring3
 * sayfaları FARKLI PD girişlerinde olunca) ring3'ün okuması kesin #PF verir.
 * İçine sihirli değer yazarız; ring3 bunu okumaya çalışır → #PF (okuyamaz →
 * değer sızmaz). Tampon 2MB → tek başına bir 2MB huge page kaplar. */
#define KERNEL_SIR_SIHIR  0xDEADC0DECAFEBABEULL
static volatile uint64_t kernel_sir_sayfa[0x200000 / sizeof(uint64_t)]
    __attribute__((aligned(0x200000)));

/* === TSS (long-mode 64-bit TSS) === */
struct __attribute__((packed)) Tss64 {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_ofset;
};
static struct Tss64 tss __attribute__((aligned(16)));

/* === GDT === */
static uint64_t gdt[7] __attribute__((aligned(16)));

struct __attribute__((packed)) DescPtr {
    uint16_t limit;
    uint64_t taban;
};

/* === IDT (256 giriş × 16 byte long-mode gate) === */
struct __attribute__((packed)) IdtGate {
    uint16_t ofset_dusuk;    /* handler[15:0] */
    uint16_t selector;       /* kod segmenti seçicisi */
    uint8_t  ist;            /* IST indeksi (0 = normal stack) */
    uint8_t  tip_attr;       /* P DPL 0 type(0xE=interrupt gate) */
    uint16_t ofset_orta;     /* handler[31:16] */
    uint32_t ofset_yuksek;   /* handler[63:32] */
    uint32_t reserved;
};
static struct IdtGate idt[256] __attribute__((aligned(16)));

/* === Kullanıcı (ring3) sayfa izni === */
#define SAYFA_US  0x4ULL   /* U/S biti (bit 2) — user erişimi */

/* Verilen sanal adresin 2MB huge page'ine (PD seviyesi) U/S ekle. Üst seviyeler
 * (PML4[i], PDPT[j]) de U/S almalı (yürüyüş her seviyede U/S=1 ister). Kernel-sır
 * PD girişi ayrı olduğundan supervisor-only kalır — asıl per-sayfa ayrım PD'de. */
static void kullanici_sayfa_izni_ver(uint64_t sanal_adr) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    uint64_t *pml4 = (uint64_t *)(uintptr_t)(cr3 & ~0xFFFULL);

    unsigned pml4_i = (sanal_adr >> 39) & 0x1FF;
    unsigned pdpt_i = (sanal_adr >> 30) & 0x1FF;
    unsigned pd_i   = (sanal_adr >> 21) & 0x1FF;

    if (pml4[pml4_i] & 0x1ULL) {
        pml4[pml4_i] |= SAYFA_US;
        uint64_t *pdpt = (uint64_t *)(uintptr_t)(pml4[pml4_i] & 0x000FFFFFFFFFF000ULL);
        if (pdpt[pdpt_i] & 0x1ULL) {
            pdpt[pdpt_i] |= SAYFA_US;
            if (!(pdpt[pdpt_i] & 0x80ULL)) {   /* 1GB huge değilse PD'ye in */
                uint64_t *pd = (uint64_t *)(uintptr_t)(pdpt[pdpt_i] & 0x000FFFFFFFFFF000ULL);
                if (pd[pd_i] & 0x1ULL) {
                    pd[pd_i] |= SAYFA_US;   /* 2MB huge page → user erişilebilir */
                }
            }
        }
    }
}

/* Bir sanal adresin PD (2MB) girişinde U/S biti var mı? (İzolasyon doğrulama —
 * kernel-sır sayfasının HÂLÂ supervisor-only olduğunu teyit için.) Dönüş:
 * 1 = U/S set (user erişilebilir), 0 = supervisor-only, -1 = eşlenmemiş. */
static int sayfa_us_durumu(uint64_t sanal_adr) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    uint64_t *pml4 = (uint64_t *)(uintptr_t)(cr3 & ~0xFFFULL);

    unsigned pml4_i = (sanal_adr >> 39) & 0x1FF;
    unsigned pdpt_i = (sanal_adr >> 30) & 0x1FF;
    unsigned pd_i   = (sanal_adr >> 21) & 0x1FF;

    if (!(pml4[pml4_i] & 0x1ULL)) return -1;
    uint64_t *pdpt = (uint64_t *)(uintptr_t)(pml4[pml4_i] & 0x000FFFFFFFFFF000ULL);
    if (!(pdpt[pdpt_i] & 0x1ULL)) return -1;
    if (pdpt[pdpt_i] & 0x80ULL) return (pdpt[pdpt_i] & SAYFA_US) ? 1 : 0;
    uint64_t *pd = (uint64_t *)(uintptr_t)(pdpt[pdpt_i] & 0x000FFFFFFFFFF000ULL);
    if (!(pd[pd_i] & 0x1ULL)) return -1;
    return (pd[pd_i] & SAYFA_US) ? 1 : 0;
}

/* Handler adreslerini (asm stub) tanımla — dosya-düzeyi asm blob. */
extern void syscall_stub_r3(void);   /* int 0x80 handler */
extern void gp_stub_r3(void);        /* #GP (vektör 13) handler */
extern void pf_stub_r3(void);        /* #PF (vektör 14) handler — SAYFA İZOLASYON kanıtı */

/*
 * === Asm stub'lar (dosya-düzeyi inline asm) ===
 *
 * syscall_stub_r3: int 0x80 ile girilir. rax=syscall num. r3_syscall_num'a rax'i
 *   yaz, r3_syscall_geldi=1, iretq → ring3'e dön.
 *
 * pf_stub_r3: #PF (vektör 14) → CPU HATA KODU push eder (yığın tepesinde 8 byte).
 *   Bu, ring3'ün kernel-sır sayfasını okumasının donanım kanıtı. Hata kodu +
 *   CR2 (fault adresi) kaydet, r3_pf_yakalandi=1. Ring3'e DÖNMEYİZ (dönersek
 *   aynı okuma tekrar #PF verir → sonsuz döngü); yığını temizleyip kernel_devam'a
 *   dallanırız (ring0, bilinen akış). Yığın: err(8) + RIP,CS,RFLAGS,RSP,SS (5×8)
 *   = 48 byte.
 *
 * gp_stub_r3: #GP (vektör 13) — beklenmedik (test yalnız #PF bekler). Yine de
 *   güvenli yakala (err kodu at, kernel_devam). Diagnostik için r3_pf_err'e
 *   0xFFFF... yazılmaz; ayrı bayrak tutmayız (bu testte #GP oluşmamalı).
 *
 * NOT: rip-relative erişim long-mode'da güvenli; CR2 okuma `mov %cr2, reg`.
 */
__asm__(
    ".text\n"
    ".global syscall_stub_r3\n"
    "syscall_stub_r3:\n"
    "    movl %eax, r3_syscall_num(%rip)\n"
    "    movl $1, r3_syscall_geldi(%rip)\n"
    "    iretq\n"                       /* ring3'e dön (CPU frame'i geri yükle) */
    "\n"
    ".global pf_stub_r3\n"
    "pf_stub_r3:\n"
    /* #PF: yığın tepesinde CPU hata kodu (8 byte). Onu al → r3_pf_err. */
    "    movq (%rsp), %rax\n"           /* hata kodu (P|W|U bitleri) */
    "    movq %rax, r3_pf_err(%rip)\n"
    "    movq %cr2, %rax\n"             /* fault adresi (okunmaya çalışılan sanal) */
    "    movq %rax, r3_pf_cr2(%rip)\n"
    "    movl $1, r3_pf_yakalandi(%rip)\n"
    /* Ring3'e DÖNMEYECEĞİZ. Hata kodu(8) + iretq frame(5×8=40) = 48 byte at,
       kernel_devam'a dallan (ring0). */
    "    addq $48, %rsp\n"
    "    jmp kernel_devam\n"
    "\n"
    ".global gp_stub_r3\n"
    "gp_stub_r3:\n"
    /* #GP: beklenmedik. Yine de err kodunu at + kernel_devam (güvenli). */
    "    addq $48, %rsp\n"
    "    jmp kernel_devam\n"
);

/* kernel_devam: #PF (veya beklenmedik #GP) stub buraya dallanır (ring0). */
extern void kernel_devam(void);

/* === GDT descriptor kur === */
static void gdt_kur(void) {
    gdt[0] = 0x0000000000000000ULL;   /* null */
    gdt[1] = 0x00209A0000000000ULL;   /* kernel kod (0x08): DPL0, L=1, exec/read */
    gdt[2] = 0x0000920000000000ULL;   /* kernel veri (0x10): DPL0, r/w */
    gdt[3] = 0x0020FA0000000000ULL;   /* user kod (0x18): DPL3, L=1, exec/read */
    gdt[4] = 0x0000F20000000000ULL;   /* user veri (0x20): DPL3, r/w */

    /* TSS descriptor (0x28) — long-mode 16-byte sistem descriptor (2 quad). */
    uint64_t tss_taban = (uint64_t)(uintptr_t)&tss;
    uint32_t tss_limit = (uint32_t)(sizeof(tss) - 1);

    uint64_t desc_dusuk = 0;
    desc_dusuk |= (uint64_t)(tss_limit & 0xFFFFu);                 /* limit[15:0] */
    desc_dusuk |= ((uint64_t)(tss_taban & 0xFFFFFFULL)) << 16;     /* taban[23:0] */
    desc_dusuk |= ((uint64_t)0x89ULL) << 40;                       /* access P|DPL0|type=9 */
    desc_dusuk |= ((uint64_t)((tss_limit >> 16) & 0xFULL)) << 48;  /* limit[19:16] */
    desc_dusuk |= ((uint64_t)((tss_taban >> 24) & 0xFFULL)) << 56; /* taban[31:24] */
    uint64_t desc_yuksek = (tss_taban >> 32) & 0xFFFFFFFFULL;      /* taban[63:32] */

    gdt[5] = desc_dusuk;
    gdt[6] = desc_yuksek;

    struct DescPtr gp = {
        .limit = (uint16_t)(sizeof(gdt) - 1),
        .taban = (uint64_t)(uintptr_t)&gdt[0],
    };
    __asm__ volatile("lgdt %0" : : "m"(gp) : "memory");

    /* Segment register'ları yeni GDT'ye göre yeniden yükle. */
    __asm__ volatile(
        "movw %0, %%ax\n"
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%ss\n"
        "movw %%ax, %%fs\n"
        "movw %%ax, %%gs\n"
        : : "i"(SEL_KERNEL_VERI) : "ax", "memory");

    __asm__ volatile(
        "leaq 1f(%%rip), %%rax\n"
        "pushq %0\n"              /* yeni CS (0x08) */
        "pushq %%rax\n"           /* dönüş RIP */
        "lretq\n"                 /* far return → CS=0x08 yükler */
        "1:\n"
        : : "i"((uint64_t)SEL_KERNEL_KOD) : "rax", "memory");
}

/* === TSS kur + ltr === */
static void tss_kur(void) {
    uint8_t *p = (uint8_t *)&tss;
    for (unsigned k = 0; k < sizeof(tss); k++) p[k] = 0;

    uint64_t rsp0 = (uint64_t)(uintptr_t)&kernel_ring0_yigin[sizeof(kernel_ring0_yigin)];
    rsp0 &= ~(uint64_t)0xF;
    tss.rsp0 = rsp0;
    tss.iopb_ofset = (uint16_t)sizeof(tss);

    __asm__ volatile("ltr %0" : : "r"((uint16_t)SEL_TSS) : "memory");
}

/* === IDT gate kur === */
static void idt_gate_kur(int vektor, void (*handler)(void), uint8_t dpl) {
    uint64_t adr = (uint64_t)(uintptr_t)handler;
    struct IdtGate *g = &idt[vektor];
    g->ofset_dusuk  = (uint16_t)(adr & 0xFFFFu);
    g->selector     = SEL_KERNEL_KOD;   /* handler ring0 kod segmentinde koşar */
    g->ist          = 0;
    g->tip_attr     = (uint8_t)(0x80u | ((dpl & 3u) << 5) | 0x0Eu);
    g->ofset_orta   = (uint16_t)((adr >> 16) & 0xFFFFu);
    g->ofset_yuksek = (uint32_t)((adr >> 32) & 0xFFFFFFFFu);
    g->reserved     = 0;
}

static void idt_kur(void) {
    uint8_t *p = (uint8_t *)idt;
    for (unsigned k = 0; k < sizeof(idt); k++) p[k] = 0;

    /* Vektör 13 (#GP): DPL=0 (beklenmedik — güvenli yakalama). */
    idt_gate_kur(13, gp_stub_r3, 0);
    /* Vektör 14 (#PF): DPL=0 — SAYFA-İZOLASYON kanıtı (ring3 kernel-sır okuması). */
    idt_gate_kur(14, pf_stub_r3, 0);
    /* Vektör 0x80 (syscall): DPL=3 → ring3 `int 0x80` çağırabilir. */
    idt_gate_kur(0x80, syscall_stub_r3, 3);

    struct DescPtr ip = {
        .limit = (uint16_t)(sizeof(idt) - 1),
        .taban = (uint64_t)(uintptr_t)&idt[0],
    };
    __asm__ volatile("lidt %0" : : "m"(ip) : "memory");
}

/*
 * === Ring3 kullanıcı kodu ===
 * CPL=3'te koşar.
 * 1. CS'i oku → RPL (& 3) → ring3 kanıtı.
 * 2. KENDİ user sayfasına eriş (paylaşılan volatile OKU) → OK (U/S=1).
 * 3. int 0x80 (rax=1) → syscall handler ring0'da (izinli).
 * 4. KERNEL-SIR sayfasını OKU (U/S=0) → #PF → pf_stub_r3 (ring0) → kernel_devam.
 *
 * NOT: Fonksiyon DÖNMEZ (kernel-sır okuma #PF verir, handler kernel'e dallanır).
 * Kernel-sır adresini bir yerel register'a alıp okuruz; okuma anında #PF fırlar,
 * değer register'a ULAŞMAZ (izolasyon: sır sızmaz).
 */
__attribute__((noinline))
static void ring3_kod(void) {
    /* 1. CS oku → RPL. */
    uint32_t cs_deg;
    __asm__ volatile("movl %%cs, %0" : "=r"(cs_deg));
    r3_cs_rpl = cs_deg & 3u;

    /* 2. Kendi user sayfasını oku (izinli) — paylaşılan volatile'lardan biri.
     *    Bu erişim BAŞARILI olmalı (U/S=1). Okuyup geri yaz → "user sayfa OK". */
    uint32_t kendi = r3_cs_rpl;         /* zaten yazdık; oku → user sayfa erişimi */
    r3_user_okudu = (kendi == 3u) ? 1u : 2u;

    /* 3. Syscall: int 0x80, rax=1. */
    __asm__ volatile(
        "movl $1, %%eax\n"
        "int $0x80\n"
        : : : "eax", "memory");

    /* 4. KERNEL-SIR sayfasını OKU → #PF (kernel-only, U/S=0). Handler kernel'e
     *    dallanır → buradan sonrası ULAŞILMAZ. Okuma volatile → derleyici elemez.
     *    Sonucu paylaşılan olmayan bir yere yazmaya ÇALIŞIRIZ; #PF okuma anında
     *    fırladığından yazma hiç gerçekleşmez (sır sızmaz). */
    volatile uint64_t *sir = &kernel_sir_sayfa[0];
    uint64_t calindi = *sir;            /* <-- #PF BURADA (supervisor-only okuma) */

    /* Ulaşılmaz. calindi'yi "kullan" ki derleyici okumayı elemesin. */
    r3_syscall_num = (uint32_t)calindi; /* ULAŞILMAZ (yukarıda #PF fırladı) */
    for (;;) { }
}

/* === Ring3'e geçiş (iretq) === */
static _Noreturn void ring3e_gec(void) {
    uint64_t user_sp = (uint64_t)(uintptr_t)&user_yigin[sizeof(user_yigin)];
    user_sp &= ~(uint64_t)0xF;   /* 16-hizala */

    uint64_t user_rip = (uint64_t)(uintptr_t)&ring3_kod;
    uint64_t user_ss  = SEL_USER_VERI;   /* 0x20 | 3 */
    uint64_t user_cs  = SEL_USER_KOD;    /* 0x18 | 3 */
    uint64_t rflags   = 0x202ULL;        /* IF=1 (bit9) + rezerve bit1 */

    __asm__ volatile(
        "pushq %0\n"     /* SS */
        "pushq %1\n"     /* RSP */
        "pushq %2\n"     /* RFLAGS */
        "pushq %3\n"     /* CS */
        "pushq %4\n"     /* RIP */
        "iretq\n"
        :
        : "r"(user_ss), "r"(user_sp), "r"(rflags), "r"(user_cs), "r"(user_rip)
        : "memory");

    for (;;) { __asm__ volatile("hlt"); }
}

/* === Rapor + halt === */
static _Noreturn void rapor_ve_dur(void) {
    kdl_yaz_metin("CS_RPL=");
    kdl_yaz_onaltilik((uint64_t)r3_cs_rpl);
    kdl_yaz_metin(" USER_OKUDU=");
    kdl_yaz_onaltilik((uint64_t)r3_user_okudu);
    kdl_yaz_metin(" SYSCALL=");
    kdl_yaz_onaltilik((uint64_t)r3_syscall_geldi);
    kdl_yaz_metin(" PF=");
    kdl_yaz_onaltilik((uint64_t)r3_pf_yakalandi);
    kdl_yaz_metin(" PF_ERR=");
    kdl_yaz_onaltilik(r3_pf_err);
    kdl_yaz_metin(" PF_CR2=");
    kdl_yazdir_onaltilik(r3_pf_cr2);

    /* Kernel-sır sayfasının HÂLÂ supervisor-only olduğunu ve ring3 kod
     * sayfasının user-erişilebilir olduğunu teyit (izolasyon sınıflandırması). */
    int sir_us  = sayfa_us_durumu((uint64_t)(uintptr_t)&kernel_sir_sayfa[0]);
    int kod_us  = sayfa_us_durumu((uint64_t)(uintptr_t)&ring3_kod);
    kdl_yaz_metin("SIR_US=");
    kdl_yaz_onaltilik((uint64_t)(uint32_t)sir_us);
    kdl_yaz_metin(" KOD_US=");
    kdl_yazdir_onaltilik((uint64_t)(uint32_t)kod_us);

    int ring3_kostu = (r3_cs_rpl == 3u);
    int user_okudu  = (r3_user_okudu == 1u);       /* kendi sayfasını okudu (OK) */
    int syscall_oldu = (r3_syscall_geldi != 0);
    int pf_oldu      = (r3_pf_yakalandi != 0);
    /* #PF hata kodu: P (bit0) = 1 (koruma ihlali) + U (bit2) = 1 (CPL=3 erişimi).
     * W (bit1) = 0 (okuma). Yani düşük 3 bit = 0b101 = 5. */
    int pf_err_dogru = ((r3_pf_err & 0x1ULL) != 0) && ((r3_pf_err & 0x4ULL) != 0)
                       && ((r3_pf_err & 0x2ULL) == 0);
    /* Fault adresi kernel-sır sayfasında mı? (2MB hizalı taban ile karşılaştır.) */
    uint64_t sir_taban = (uint64_t)(uintptr_t)&kernel_sir_sayfa[0] & ~0x1FFFFFULL;
    int cr2_dogru = ((r3_pf_cr2 & ~0x1FFFFFULL) == sir_taban);

    if (ring3_kostu && pf_oldu) {
        kdl_yazdir_metin("PAGE ISO GP (ring3 kernel-sir okudu -> #PF v=14 yakalandi)");
    }

    if (ring3_kostu && user_okudu && syscall_oldu && pf_oldu && pf_err_dogru) {
        /* İzolasyon seviyesini sınıflandır. Tam izolasyon: kernel-sır sayfası
         * supervisor-only (sir_us==0) + ring3 kod sayfası user (kod_us==1). */
        if (sir_us == 0 && kod_us == 1 && cr2_dogru) {
            kdl_yazdir_metin("PAGE ISO OK (tam-izolasyon: ring3 kernel-only sayfayi OKUYAMADI, #PF err=P|U, CR2=kernel-sir)");
        } else if (sir_us == 0) {
            kdl_yazdir_metin("PAGE ISO OK (tek-sayfa: kernel-sir sayfasi supervisor-only, ring3 okumasi #PF err=P|U)");
        } else {
            kdl_yazdir_metin("PAGE ISO OK (#PF err=P|U yakalandi; sayfa US durumu belirsiz)");
        }
    } else if (ring3_kostu && pf_oldu) {
        kdl_yazdir_metin("PAGE ISO KISMI (#PF yakalandi ama err/user/syscall kanitlarindan biri eksik)");
    } else {
        kdl_yazdir_metin("PAGE ISO BASARISIZ (ring3-exec veya #PF kaniti yok)");
    }

    halt();
}

/* kernel_devam: #PF (veya beklenmedik #GP) stub asm'inden `jmp kernel_devam` ile
 * girilir (ring0). C linkage'lı global. */
__attribute__((used))
void kernel_devam(void) {
    rapor_ve_dur();
}

int main(void) {
    kdl_yazdir_metin("PAGE ISO BASLA");
    kdl_yazdir_satir();

    /* --- 2. GDT (ring3 user seg + TSS descriptor) --- */
    gdt_kur();

    /* --- 3. TSS (RSP0) + ltr --- */
    tss_kur();

    /* --- 4. IDT (int 0x80 DPL=3 + #GP DPL=0 + #PF DPL=0) --- */
    idt_kur();

    /* --- 5. Kernel-sır sayfasına sihirli değeri yaz (ring0, izinli). Ring3
     *    bunu OKUYAMAYACAK (supervisor-only). Sayfanın tümünü işaretlemek yerine
     *    ilk qword yeterli (okuma orada test edilir). --- */
    kernel_sir_sayfa[0] = KERNEL_SIR_SIHIR;
    __asm__ volatile("mfence" ::: "memory");

    /* --- 6. YALNIZ ring3 sayfalarına U/S ekle (TAM İZOLASYON) ---
     * ring3_kod (komut getirme), user_yigin (stack), paylaşılan volatile'lar
     * (ring3 okur/yazar). Kernel-sır sayfasına U/S EKLEME → supervisor-only kalır.
     * Kernel-sır 2MB-hizalı+2MB-boyutlu → kendi PD girişinde, ring3 sayfalarıyla
     * ASLA çakışmaz. */
    kullanici_sayfa_izni_ver((uint64_t)(uintptr_t)&ring3_kod);
    kullanici_sayfa_izni_ver((uint64_t)(uintptr_t)&user_yigin[0]);
    kullanici_sayfa_izni_ver((uint64_t)(uintptr_t)&user_yigin[sizeof(user_yigin) - 1]);
    kullanici_sayfa_izni_ver((uint64_t)(uintptr_t)&r3_cs_rpl);
    kullanici_sayfa_izni_ver((uint64_t)(uintptr_t)&r3_user_okudu);
    kullanici_sayfa_izni_ver((uint64_t)(uintptr_t)&r3_syscall_geldi);
    kullanici_sayfa_izni_ver((uint64_t)(uintptr_t)&r3_syscall_num);
    kullanici_sayfa_izni_ver((uint64_t)(uintptr_t)&r3_pf_yakalandi);
    kullanici_sayfa_izni_ver((uint64_t)(uintptr_t)&r3_pf_err);
    kullanici_sayfa_izni_ver((uint64_t)(uintptr_t)&r3_pf_cr2);

    /* TLB'yi temizle (yeni U/S bitleri görünsün) — CR3 yeniden yükle. */
    {
        uint64_t cr3;
        __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
        __asm__ volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
    }

    /* İzolasyon ön-teyidi (ring0'dan sayfa tablolarını oku). */
    int sir_us = sayfa_us_durumu((uint64_t)(uintptr_t)&kernel_sir_sayfa[0]);
    int kod_us = sayfa_us_durumu((uint64_t)(uintptr_t)&ring3_kod);
    kdl_yaz_metin("SETUP: sir_us=");
    kdl_yaz_onaltilik((uint64_t)(uint32_t)sir_us);
    kdl_yaz_metin(" kod_us=");
    kdl_yaz_onaltilik((uint64_t)(uint32_t)kod_us);
    kdl_yaz_metin(" sir_adr=");
    kdl_yaz_onaltilik((uint64_t)(uintptr_t)&kernel_sir_sayfa[0]);
    kdl_yaz_metin(" kod_adr=");
    kdl_yazdir_onaltilik((uint64_t)(uintptr_t)&ring3_kod);

    /* --- 7. Ring3'e geç (iretq). Ring3 kod: user-oku (OK) + syscall (OK) +
     *    kernel-sir-oku (#PF). #PF handler kernel_devam'a dallanır → rapor. --- */
    ring3e_gec();

    /* Ulaşılmaz. */
    return 0;
}
