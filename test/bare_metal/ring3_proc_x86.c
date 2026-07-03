/*
 * KEMGU-OS bare-metal — x86 KULLANICI-SÜRECİ keystone (ring3 ⊕ sayfa-izolasyon ⊕
 * syscall). Milestone F. ====================================================
 *
 * aarch64 D3 (proc_arm.c — KORUMALI EL0 user-process: D1 kendi-adres-uzayı ⊕ D2
 * EL0-ayrıcalık ⊕ D-122 syscall birleşik) testinin x86 İKİZİ/PARİTESİ. Bir
 * gerçek işletim-sistemi sürecinin donanım-zorlamalı üç tanımlayıcı özelliği BİR
 * ARADA, tek testte:
 *
 *   1. KULLANICI AYRICALIĞI (ring3, CPL=3 — D-190 ring3_x86.c):
 *      GDT'de DPL=3 user-code/data descriptor + TSS (ring3→ring0 stack switch) +
 *      iretq ile "sahte dönüş" → CPU CPL'yi 3'e düşürür. Süreç kodu ring3'te koşar.
 *
 *   2. SAYFA-DÜZEYİ İZOLASYON (U/S bit — D-195 ring3_page_x86.c):
 *      Ring3 kod KENDİ user-sayfasında koşar (U/S=1); kernel-sırrı ayrı bir
 *      supervisor-only 2MB sayfada (U/S=0). Ring3 kernel-sayfayı OKUMAYA
 *      kalkınca CPU #PF (v=14, hata kodu = P|U) üretir → süreç kendi adres-
 *      uzayına HAPSEDİLMİŞ, kernel belleğine fiziksel olarak dokunamaz.
 *
 *   3. SYSCALL ARAYÜZÜ (int 0x80 — D-218 syscall_x86.c):
 *      Ring3 kod `int 0x80` ile meşru kernel geçişi yapar; num=rax, arg0=rdi,
 *      arg1=rsi, dönüş=rax (SysV benzeri). Handler ring0'da koşar, TÜM çağıran
 *      register'ları korur (D-126 register-şeffaflık dersi). İki syscall:
 *        num=2 topla(a,b) → a+b (HESAP + çok-arg + dönüş kanıtı).
 *        num=1 yaz(ptr)   → kernel seri'ye basar (I/O kanıtı).
 *      Böylece süreç hesap YAPAR + I/O yapar — gerçek syscall iş akışı.
 *
 * Bu ÜÇÜ bir arada = x86 KULLANICI-SÜRECİ keystone. Ring3 süreç kendi
 * sayfasında koşar, syscall ile hesap+I/O yapar, kernel belleğine erişince
 * hapse düşer. aarch64 proc_arm.c'nin (EL0 ⊕ TTBR ⊕ SVC) tam x86 muadili.
 *
 * -----------------------------------------------------------------------------
 * Akış:
 *   1. "RING3 PROC X86 BASLA" bas.
 *   2. GDT (null, kcode 0x08, kdata 0x10, ucode 0x18|3, udata 0x20|3, TSS 0x28)
 *      + lgdt + segment yeniden yükle.
 *   3. TSS (RSP0 = kernel ring0 stack) + ltr.
 *   4. IDT: 0x80 (syscall, DPL=3), 13 (#GP, DPL=0), 14 (#PF, DPL=0) + lidt.
 *   5. Kernel-sır sayfasına (2MB-hizalı, supervisor-only) sihirli değer yaz.
 *   6. YALNIZ ring3 sayfalarına U/S ekle (kod + user stack + paylaşılan
 *      volatile'lar + syscall metin argümanı). Kernel-sır sayfası U/S=0 KALIR.
 *   7. iretq ile ring3'e geç. ring3_kod (CPL=3):
 *        a. CS oku → RPL=3 doğrula (kendi user sayfasına yaz — U/S=1 OK).
 *        b. int 0x80 num=2 topla(40,2) → dönüş 42 (HESAP syscall).
 *        c. int 0x80 num=1 yaz("...") → kernel I/O (I/O syscall).
 *        d. Kernel-sır sayfasını OKU → #PF (v=14, err=P|U, CR2=kernel-sır) →
 *           pf_stub → kernel_devam'a dallan (HAPİS).
 *   8. Üç kanıt (ring3 CPL=3 + syscall hesap+I/O doğru + kernel-erişim #PF)
 *      toplandıysa "RING3 PROC X86 OK".
 *
 * -----------------------------------------------------------------------------
 * FALLBACK: Tam üçlü-birleşim (paging + ring3 + syscall) tek testte kurulamazsa,
 * en az ring3 (CPL=3) + syscall (int 0x80 dönüş) + kernel-erişim #PF kanıtla;
 * eksik parça (ör. tam sayfa-izolasyon sınıflandırması) rapora AÇIKÇA yazılır.
 *
 * Kanıt: "RING3 PROC X86 OK" → ring3 süreç kendi sayfasında koştu (CPL=3) +
 *        syscall ile hesap(40+2=42)+I/O yaptı + kernel-sayfa okuması #PF ile
 *        reddedildi (err=P|U, CR2=kernel-sır) = x86 korumalı user-process.
 *
 * KISIT: Tüm GDT/TSS/IDT/sayfa/ring3 mantığı test-içi inline asm/blob (ring3_x86
 * + ring3_page_x86 + syscall_x86 deseni). runtime/boot/linker'a DOKUNULMAZ —
 * kernel'in kendi IDT'sini (kdl_idt_kur) GEÇİCİ olarak KENDİ IDT'mizle
 * değiştiririz (lidt), test bitince önemsiz (halt).
 *
 * DETERMİNİSTİK: sabit argümanlar (topla 40+2), sabit sihirli değer, sabit
 * 2MB-hizalı sır adresi → birden çok QEMU koşusu byte-identik çıktı verir.
 * Başarı/başarısızlık tespitinde hlt-loop (sonsuz döngü sonrası halt).
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

/* === Paylaşılan durum (ring3 kod ↔ kernel handler/dispatcher) ===
 * volatile: derleyici cache'lemesin. Ring3-erişilir sayfalarda (U/S=1); kernel-
 * sır ise supervisor-only sayfada (U/S=0). */
static volatile uint32_t r3_cs_rpl        = 0xFFFFFFFFu;  /* ring3'te okunan CS & 3 */
static volatile uint32_t r3_user_okudu    = 0;            /* kendi user sayfasını okudu (OK) */
static volatile uint32_t r3_syscall_geldi = 0;            /* int 0x80 handler tetiklendi */
static volatile uint64_t r3_topla_sonuc   = 0;            /* topla(40,2) dönüşü (ring3'te alınan) */
static volatile uint32_t r3_yaz_sayaci    = 0;            /* num=1 yaz kaç kez çağrıldı (I/O) */
static volatile uint32_t r3_pf_yakalandi  = 0;            /* #PF (kernel-sır okuma) yakalandı */
static volatile uint64_t r3_pf_err        = 0;            /* #PF hata kodu (P|W|U bitleri) */
static volatile uint64_t r3_pf_cr2        = 0;            /* #PF fault adresi (CR2) */

/* Ring3'ün num=1 (yaz) syscall'ıyla bastıracağı C-string. Ring3-erişilir sayfada
 * olmalı (ring3 rdi'ye adresini koyar; handler ring0'da okur, ama string ring3
 * sayfasında da erişilebilir olsun diye U/S ekleriz). */
static const char syscall_yaz_metni[] = "merhaba ring3 (x86 user-process int 0x80)";

/* === Kernel ring0 stack (ring3→ring0 trap'te TSS.RSP0 buraya işaret eder) === */
static uint8_t kernel_ring0_yigin[16384] __attribute__((aligned(16)));

/* === Ring3 kullanıcı yığını === */
static uint8_t user_yigin[16384] __attribute__((aligned(16)));

/* === KERNEL-SIR sayfası (supervisor-only, ring3 OKUYAMAZ) ===
 * 2MB-HİZALI + 2MB-BOYUTLU → boot'un 2MB huge page düzeninde KENDİ PD girişini
 * TAM işgal eder. U/S EKLEMEYİNCE (ve ring3 sayfaları FARKLI PD girişlerinde
 * olunca) ring3'ün okuması kesin #PF verir. Sihirli değer yazılır; ring3 okuma
 * denemesi #PF fırlatır → değer sızmaz. */
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

/* === GDT (7 quad: null + kernel kod/veri + user kod/veri + TSS 2 quad) === */
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

/* Bir sanal adresin PD (2MB) girişinde U/S biti var mı? (İzolasyon doğrulama.)
 * Dönüş: 1 = U/S set (user), 0 = supervisor-only, -1 = eşlenmemiş. */
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
extern void syscall_stub_r3(void);   /* int 0x80 handler (num/arg0/arg1/dönüş) */
extern void gp_stub_r3(void);        /* #GP (vektör 13) handler — beklenmedik */
extern void pf_stub_r3(void);        /* #PF (vektör 14) handler — SAYFA İZOLASYON kanıtı */
extern uint64_t syscall_dispatch(uint64_t num, uint64_t arg0, uint64_t arg1);

/*
 * === Asm stub'lar (dosya-düzeyi inline asm) ===
 *
 * syscall_stub_r3: int 0x80 ile girilir. D-126 register-şeffaflık dersi: ÖNCE
 *   tüm GP register'ları kaydet, SONRA C dispatcher çağır, dönüşü save-edilmiş
 *   rax slotuna yaz → çağıran (ring3) rax'te alır. num=rax, arg0=rdi, arg1=rsi.
 *   iretq ile ring3'e döner. Yığın düzeni (push sonrası, rsp'den artan ofset):
 *     [rsp+0]   r15 ... [rsp+56] r8 [rsp+64] rbp
 *     [rsp+72]  rdi (arg0) [rsp+80] rsi (arg1) [rsp+88] rdx [rsp+96] rcx
 *     [rsp+104] rbx [rsp+112] rax (num; dönüş buraya yazılır)
 *
 * pf_stub_r3: #PF (vektör 14) → CPU HATA KODU push eder. Ring3'ün kernel-sır
 *   sayfasını okumasının donanım kanıtı. err + CR2 kaydet, r3_pf_yakalandi=1.
 *   Ring3'e DÖNMEYİZ (aynı okuma tekrar #PF → sonsuz); yığını temizle (err(8) +
 *   iretq frame(5×8=40) = 48) + kernel_devam'a dallan (ring0, bilinen akış).
 *
 * gp_stub_r3: #GP (vektör 13) — beklenmedik (test yalnız #PF bekler). Güvenli
 *   yakala: err at + kernel_devam.
 */
__asm__(
    ".text\n"
    ".global syscall_stub_r3\n"
    "syscall_stub_r3:\n"
    /* --- 1. TAM GP register kaydet (çağıranın frame'i — register-şeffaflık) --- */
    "    pushq %rax\n"        /* [rsp+112] num */
    "    pushq %rbx\n"        /* [rsp+104] */
    "    pushq %rcx\n"        /* [rsp+96] */
    "    pushq %rdx\n"        /* [rsp+88] */
    "    pushq %rsi\n"        /* [rsp+80] arg1 */
    "    pushq %rdi\n"        /* [rsp+72] arg0 */
    "    pushq %rbp\n"        /* [rsp+64] */
    "    pushq %r8\n"         /* [rsp+56] */
    "    pushq %r9\n"         /* [rsp+48] */
    "    pushq %r10\n"        /* [rsp+40] */
    "    pushq %r11\n"        /* [rsp+32] */
    "    pushq %r12\n"        /* [rsp+24] */
    "    pushq %r13\n"        /* [rsp+16] */
    "    pushq %r14\n"        /* [rsp+8] */
    "    pushq %r15\n"        /* [rsp+0] */
    /* --- 2. Kaydedilmiş num/arg0/arg1'i C dispatcher arg register'larına oku --- */
    "    movq 112(%rsp), %rdi\n"   /* num  = kaydedilmiş rax */
    "    movq 72(%rsp),  %rsi\n"   /* arg0 = kaydedilmiş rdi */
    "    movq 80(%rsp),  %rdx\n"   /* arg1 = kaydedilmiş rsi */
    /* --- 3. C dispatcher çağır → sonuç rax'te --- */
    "    call syscall_dispatch\n"
    /* --- 4. Sonucu SAVE-EDİLMİŞ rax slotuna yaz (dönüş-şeffaflığı) --- */
    "    movq %rax, 112(%rsp)\n"
    /* --- 5. TAM GP register geri yükle (rax = syscall sonucu, gerisi şeffaf) --- */
    "    popq %r15\n"
    "    popq %r14\n"
    "    popq %r13\n"
    "    popq %r12\n"
    "    popq %r11\n"
    "    popq %r10\n"
    "    popq %r9\n"
    "    popq %r8\n"
    "    popq %rbp\n"
    "    popq %rdi\n"
    "    popq %rsi\n"
    "    popq %rdx\n"
    "    popq %rcx\n"
    "    popq %rbx\n"
    "    popq %rax\n"          /* syscall sonucu (C yazdı) */
    "    iretq\n"              /* ring3'e dön (CPU iretq-frame'i geri yükler) */
    "\n"
    ".global pf_stub_r3\n"
    "pf_stub_r3:\n"
    /* #PF: yığın tepesinde CPU hata kodu (8 byte). Onu al → r3_pf_err. */
    "    movq (%rsp), %rax\n"           /* hata kodu (P|W|U bitleri) */
    "    movq %rax, r3_pf_err(%rip)\n"
    "    movq %cr2, %rax\n"             /* fault adresi (okunmaya çalışılan sanal) */
    "    movq %rax, r3_pf_cr2(%rip)\n"
    "    movl $1, r3_pf_yakalandi(%rip)\n"
    /* Ring3'e DÖNMEYECEĞİZ. err(8) + iretq frame(5×8=40) = 48 byte at, kernel_devam. */
    "    addq $48, %rsp\n"
    "    jmp kernel_devam\n"
    "\n"
    ".global gp_stub_r3\n"
    "gp_stub_r3:\n"
    /* #GP: beklenmedik. err kodunu at + kernel_devam (güvenli). */
    "    addq $48, %rsp\n"
    "    jmp kernel_devam\n"
);

/* kernel_devam: #PF (veya beklenmedik #GP) stub buraya dallanır (ring0). */
extern void kernel_devam(void);

/*
 * === C syscall dispatcher (asm stub'dan ring0'da çağrılır) ===
 * num'a göre dallanır, sonucu DÖNER (rax → ring3'e şeffaf). DETERMİNİSTİK.
 *   num=1 yaz(ptr)     : rdi = C-string → seri'ye bas (I/O). r3_yaz_sayaci++. dönüş 0.
 *   num=2 topla(a, b)  : a + b (HESAP + çok-arg + dönüş). r3_syscall_geldi=1.
 *   bilinmeyen         : -1 (hata sınırı).
 */
uint64_t syscall_dispatch(uint64_t num, uint64_t arg0, uint64_t arg1);   /* prototip */
uint64_t syscall_dispatch(uint64_t num, uint64_t arg0, uint64_t arg1) {
    r3_syscall_geldi = 1;
    switch (num) {
        case 1:   /* yaz(ptr): rdi = C-string → seri'ye bas (I/O). dönüş 0. */
            r3_yaz_sayaci++;
            kdl_yaz_metin("  [ring3 syscall yaz] ");
            kdl_yazdir_metin((const char *)(uintptr_t)arg0);
            return 0;
        case 2:   /* topla(a, b): a + b (HESAP + çok-arg + dönüş). */
            return arg0 + arg1;
        default:  /* bilinmeyen → -1 (hata sınırı). */
            return (uint64_t)-1;
    }
}

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

    /* CS'i yeniden yükle: yığına yeni CS + dönüş adresi push → lretq. */
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
    /* P(0x80) | DPL(dpl<<5) | type=0xE (64-bit interrupt gate → girişte IF=0). */
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
 * === Ring3 kullanıcı-süreç kodu ===
 * CPL=3'te koşar, KENDİ user sayfasında (U/S=1). SELF-CONTAINED (kernel
 * fonksiyonu çağırmaz; yalnız int 0x80 ile geçer + kendi user sayfasına yazar).
 * 1. CS oku → RPL (& 3) → ring3 kanıtı (kendi sayfasına yaz — U/S=1 OK).
 * 2. int 0x80 num=2 topla(40,2) → dönüş 42 (HESAP syscall; dönüş rax'te alınır).
 * 3. int 0x80 num=1 yaz("...") → kernel I/O (I/O syscall).
 * 4. KERNEL-SIR sayfasını OKU (U/S=0) → #PF → pf_stub_r3 (ring0) → kernel_devam.
 *
 * NOT: Fonksiyon DÖNMEZ (kernel-sır okuma #PF verir, handler kernel'e dallanır).
 */
__attribute__((noinline))
static void ring3_kod(void) {
    /* 1. CS oku → RPL. Kendi user sayfasına yaz (U/S=1 → izinli). */
    uint32_t cs_deg;
    __asm__ volatile("movl %%cs, %0" : "=r"(cs_deg));
    r3_cs_rpl = cs_deg & 3u;
    uint32_t kendi = r3_cs_rpl;             /* kendi user sayfasını oku → erişim OK */
    r3_user_okudu = (kendi == 3u) ? 1u : 2u;

    /* 2. HESAP syscall: int 0x80 num=2 topla(40, 2) → dönüş 42. rax=num,
     *    rdi=arg0, rsi=arg1; dönüş rax → r3_topla_sonuc (kendi sayfasına yaz). */
    uint64_t topla;
    __asm__ volatile(
        "movq $2, %%rax\n"       /* num = 2 (topla) */
        "movq $40, %%rdi\n"      /* arg0 = 40 */
        "movq $2, %%rsi\n"       /* arg1 = 2 */
        "int $0x80\n"
        "movq %%rax, %0\n"       /* topla = dönüş */
        : "=r"(topla)
        : : "rax", "rdi", "rsi", "memory", "cc");
    r3_topla_sonuc = topla;

    /* 3. I/O syscall: int 0x80 num=1 yaz(ptr). rdi = C-string ptr. */
    __asm__ volatile(
        "movq $1, %%rax\n"       /* num = 1 (yaz) */
        "movq %0, %%rdi\n"       /* arg0 = metin ptr */
        "int $0x80\n"
        : : "r"((uint64_t)(uintptr_t)syscall_yaz_metni)
        : "rax", "rdi", "rsi", "memory", "cc");

    /* 4. KERNEL-SIR sayfasını OKU → #PF (kernel-only, U/S=0). Handler kernel'e
     *    dallanır → buradan sonrası ULAŞILMAZ. Okuma anında #PF fırlar → değer
     *    register'a ulaşmaz (izolasyon: sır sızmaz). */
    volatile uint64_t *sir = &kernel_sir_sayfa[0];
    uint64_t calindi = *sir;                /* <-- #PF BURADA (supervisor-only okuma) */

    /* Ulaşılmaz. calindi'yi "kullan" ki derleyici okumayı elemesin. */
    r3_topla_sonuc = (uint64_t)calindi;     /* ULAŞILMAZ (yukarıda #PF fırladı) */
    for (;;) { }
}

/* === Ring3'e geçiş (iretq) ===
 * Yığın (iretq'nin beklediği, yukarıdan aşağı push): SS, RSP, RFLAGS, CS, RIP.
 * CS.RPL=3 → hedef CPL=3 (ring3). */
static _Noreturn void ring3e_gec(void) {
    uint64_t user_sp = (uint64_t)(uintptr_t)&user_yigin[sizeof(user_yigin)];
    user_sp &= ~(uint64_t)0xF;   /* 16-hizala */

    uint64_t user_rip = (uint64_t)(uintptr_t)&ring3_kod;
    uint64_t user_ss  = SEL_USER_VERI;   /* 0x20 | 3 */
    uint64_t user_cs  = SEL_USER_KOD;    /* 0x18 | 3 */
    /* RFLAGS: rezerve bit1=1, IF=0 (KRİTİK: ring3 kesme AÇMAZ).
     * DETERMİNİZM: IDT yalnız vektör 13/14/0x80 dolu; diğer tüm vektörler
     * present=0. Ring3'te IF=1 olsaydı (0x202) async bir IRQ/spurious kesme
     * boş IDT girişine vektörleyip #GP → erken kernel_devam (ring3 hiç koşmaz)
     * → CS_RPL=0xffffffff yarışı olurdu. Ring3 kodu YALNIZ senkron int 0x80 +
     * kasıtlı #PF yapar; kesmeye ihtiyacı yok → IF=0 yarışı tümden eler. */
    uint64_t rflags   = 0x002ULL;        /* IF=0 (kesme kapalı) + rezerve bit1 */

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

/* === Rapor + halt (başarı ve #PF-sonrası buradan geçer) === */
static _Noreturn void rapor_ve_dur(void) {
    kdl_yaz_metin("CS_RPL=");
    kdl_yaz_onaltilik((uint64_t)r3_cs_rpl);
    kdl_yaz_metin(" USER_OKUDU=");
    kdl_yaz_onaltilik((uint64_t)r3_user_okudu);
    kdl_yaz_metin(" SYSCALL=");
    kdl_yaz_onaltilik((uint64_t)r3_syscall_geldi);
    kdl_yaz_metin(" TOPLA=");
    kdl_yaz_onaltilik(r3_topla_sonuc);
    kdl_yaz_metin(" YAZ=");
    kdl_yaz_onaltilik((uint64_t)r3_yaz_sayaci);
    kdl_yaz_metin(" PF=");
    kdl_yaz_onaltilik((uint64_t)r3_pf_yakalandi);
    kdl_yaz_metin(" PF_ERR=");
    kdl_yaz_onaltilik(r3_pf_err);
    kdl_yaz_metin(" PF_CR2=");
    kdl_yazdir_onaltilik(r3_pf_cr2);

    /* Kernel-sır sayfasının HÂLÂ supervisor-only, ring3 kod sayfasının user
     * olduğunu teyit (izolasyon sınıflandırması). */
    int sir_us  = sayfa_us_durumu((uint64_t)(uintptr_t)&kernel_sir_sayfa[0]);
    int kod_us  = sayfa_us_durumu((uint64_t)(uintptr_t)&ring3_kod);
    kdl_yaz_metin("SIR_US=");
    kdl_yaz_onaltilik((uint64_t)(uint32_t)sir_us);
    kdl_yaz_metin(" KOD_US=");
    kdl_yazdir_onaltilik((uint64_t)(uint32_t)kod_us);

    /* Üç keystone özelliğinin kanıtları. */
    int ring3_kostu  = (r3_cs_rpl == 3u);              /* 1. ring3 (CPL=3) */
    int user_okudu   = (r3_user_okudu == 1u);          /*    kendi sayfasını okudu (OK) */
    int syscall_oldu = (r3_syscall_geldi != 0);        /* 3. syscall tetiklendi */
    int hesap_ok     = (r3_topla_sonuc == 42u);        /*    topla(40,2)=42 (HESAP) */
    int yaz_ok       = (r3_yaz_sayaci == 1u);          /*    yaz koştu (I/O) */
    int pf_oldu      = (r3_pf_yakalandi != 0);         /* 2. kernel-erişim #PF */
    /* #PF hata kodu: P (bit0)=1 (koruma ihlali) + U (bit2)=1 (CPL=3) + W (bit1)=0 (oku). */
    int pf_err_dogru = ((r3_pf_err & 0x1ULL) != 0) && ((r3_pf_err & 0x4ULL) != 0)
                       && ((r3_pf_err & 0x2ULL) == 0);
    /* Fault adresi kernel-sır sayfasında mı? (2MB hizalı taban ile karşılaştır.) */
    uint64_t sir_taban = (uint64_t)(uintptr_t)&kernel_sir_sayfa[0] & ~0x1FFFFFULL;
    int cr2_dogru = ((r3_pf_cr2 & ~0x1FFFFFULL) == sir_taban);

    /* Ara-kanıt satırları (aarch64 proc_arm.c'nin "SYSCALL ARG OK" + "ISTISNA"
     * satırlarının x86 muadili). */
    if (ring3_kostu && syscall_oldu && hesap_ok && yaz_ok) {
        kdl_yazdir_metin("RING3 SYSCALL ARG OK (int 0x80 num=2 topla(40,2)=42 + num=1 yaz -> hesap+I/O)");
    }
    if (ring3_kostu && pf_oldu) {
        kdl_yazdir_metin("RING3 ISTISNA (kernel-sir okuma -> #PF v=14, HAPIS)");
    }

    /* Tam üçlü-birleşim: ring3 (CPL=3) + syscall (hesap+I/O) + kernel-erişim #PF. */
    int uclu_ok = ring3_kostu && user_okudu && syscall_oldu && hesap_ok && yaz_ok
                  && pf_oldu && pf_err_dogru;

    if (uclu_ok && sir_us == 0 && kod_us == 1 && cr2_dogru) {
        kdl_yazdir_metin(
            "RING3 PROC X86 OK (ring3-exec CPL=3 + syscall hesap+I/O + kernel-sayfa #PF err=P|U, CR2=kernel-sir; tam-izolasyon)");
    } else if (uclu_ok) {
        /* Üçü de kanıtlandı ama sayfa US sınıflandırması belirsiz — yine keystone. */
        kdl_yazdir_metin(
            "RING3 PROC X86 OK (ring3-exec CPL=3 + syscall hesap+I/O + kernel-sayfa #PF err=P|U; sayfa-US sinifi belirsiz)");
    } else if (ring3_kostu && syscall_oldu && hesap_ok && pf_oldu) {
        /* FALLBACK: ring3 + syscall-dönüş + kernel-erişim #PF (min üçlü) — eksik
         * parçayı açıkça belirt (yaz/user-okuma/err-biti kanıtlarından biri zayıf). */
        kdl_yazdir_metin(
            "RING3 PROC X86 OK (FALLBACK: ring3-exec + syscall-donus + kernel #PF; tam err/US teyidi eksik)");
    } else {
        kdl_yazdir_metin(
            "RING3 PROC X86 BASARISIZ (ring3-exec / syscall-hesap / kernel-#PF kanitlarindan biri eksik)");
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
    kdl_yazdir_metin("RING3 PROC X86 BASLA");
    kdl_yazdir_satir();

    /* --- 2. GDT (ring3 user seg + TSS descriptor) --- */
    gdt_kur();

    /* --- 3. TSS (RSP0) + ltr --- */
    tss_kur();

    /* --- 4. IDT (int 0x80 DPL=3 + #GP DPL=0 + #PF DPL=0) --- */
    idt_kur();

    /* --- 5. Kernel-sır sayfasına sihirli değeri yaz (ring0, izinli). Ring3
     *    bunu OKUYAMAYACAK (supervisor-only). --- */
    kernel_sir_sayfa[0] = KERNEL_SIR_SIHIR;
    __asm__ volatile("mfence" ::: "memory");

    /* --- 6. YALNIZ ring3 sayfalarına U/S ekle (TAM İZOLASYON). Kernel-sır
     *    sayfasına U/S EKLEME → supervisor-only kalır. Syscall metin argümanı
     *    da ring3-erişilir olsun (ring3 rdi'ye adresini koyar). --- */
    kullanici_sayfa_izni_ver((uint64_t)(uintptr_t)&ring3_kod);
    kullanici_sayfa_izni_ver((uint64_t)(uintptr_t)&user_yigin[0]);
    kullanici_sayfa_izni_ver((uint64_t)(uintptr_t)&user_yigin[sizeof(user_yigin) - 1]);
    kullanici_sayfa_izni_ver((uint64_t)(uintptr_t)&r3_cs_rpl);
    kullanici_sayfa_izni_ver((uint64_t)(uintptr_t)&r3_user_okudu);
    kullanici_sayfa_izni_ver((uint64_t)(uintptr_t)&r3_syscall_geldi);
    kullanici_sayfa_izni_ver((uint64_t)(uintptr_t)&r3_topla_sonuc);
    kullanici_sayfa_izni_ver((uint64_t)(uintptr_t)&r3_yaz_sayaci);
    kullanici_sayfa_izni_ver((uint64_t)(uintptr_t)&r3_pf_yakalandi);
    kullanici_sayfa_izni_ver((uint64_t)(uintptr_t)&r3_pf_err);
    kullanici_sayfa_izni_ver((uint64_t)(uintptr_t)&r3_pf_cr2);
    kullanici_sayfa_izni_ver((uint64_t)(uintptr_t)&syscall_yaz_metni[0]);
    kullanici_sayfa_izni_ver((uint64_t)(uintptr_t)&syscall_yaz_metni[sizeof(syscall_yaz_metni) - 1]);

    /* TLB'yi temizle (yeni U/S bitleri görünsün) — CR3 yeniden yükle. */
    {
        uint64_t cr3;
        __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
        __asm__ volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
    }

    /* İzolasyon ön-teyidi (ring0'dan sayfa tablolarını oku). */
    int sir_us = sayfa_us_durumu((uint64_t)(uintptr_t)&kernel_sir_sayfa[0]);
    int kod_us = sayfa_us_durumu((uint64_t)(uintptr_t)&ring3_kod);
    kdl_yaz_metin("SETUP: user_kod_sel=");
    kdl_yaz_onaltilik((uint64_t)SEL_USER_KOD);
    kdl_yaz_metin(" tss_sel=");
    kdl_yaz_onaltilik((uint64_t)SEL_TSS);
    kdl_yaz_metin(" sir_us=");
    kdl_yaz_onaltilik((uint64_t)(uint32_t)sir_us);
    kdl_yaz_metin(" kod_us=");
    kdl_yaz_onaltilik((uint64_t)(uint32_t)kod_us);
    kdl_yaz_metin(" sir_adr=");
    kdl_yazdir_onaltilik((uint64_t)(uintptr_t)&kernel_sir_sayfa[0]);

    /* --- 7. Ring3'e geç (iretq). Ring3 süreç: CPL=3 + syscall(hesap+I/O) +
     *    kernel-sir-oku(#PF). #PF handler kernel_devam'a dallanır → rapor. --- */
    ring3e_gec();

    /* Ulaşılmaz. */
    return 0;
}
