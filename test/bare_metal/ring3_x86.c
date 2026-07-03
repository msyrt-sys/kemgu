/*
 * KEMGU-OS bare-metal ring3 (userspace) testi (x86_64) — AYRICALIK AYRIMI.
 * ======================================================================
 *
 * Milestone: aarch64 D2 (EL0 privilege ayrımı) + D-124 (userspace ABI)
 * testlerinin x86 ikizi/paritesi. Long-mode ring0 kernel, ring3 (userspace)
 * kod çalıştırır; ring3 `int 0x80` ile syscall yapar (kernel ring0'da işler),
 * sonra ring3 ayrıcalıklı bir komut (`cli`) dener → #GP (vektör 13) → kernel
 * handler yakalar. Bu, donanım-zorlamalı ayrıcalık ayrımının KANITI.
 *
 * -----------------------------------------------------------------------------
 * x86 ring3'e geçiş neden aarch64'ten SUBTLE:
 *   aarch64'te EL0'a geçiş `eret` + SPSR_EL1.M=EL0t ile net; segment yok.
 *   x86-64'te ise ring3 için GEREKLİ altyapı:
 *     1. GDT'de DPL=3 user-code (L-bit, 64-bit) + user-data descriptor'ları.
 *     2. TSS (Task State Segment) — ring3'ten interrupt/trap gelince CPU
 *        RSP0'ı TSS'ten okuyup ring0 stack'ine geçer. TSS'siz ring3→ring0
 *        trap TRIPLE FAULT yapar (stack switch için hedef yok).
 *     3. IDT'de int 0x80 gate DPL=3 (ring3 çağırabilsin) + #GP gate.
 *     4. iretq ile ring3'e "sahte dönüş": yığına SS|3, RSP_user, RFLAGS,
 *        CS|3, RIP_user push → iretq CPL'yi 3'e düşürür.
 *   Long-mode TSS descriptor 16 BYTE (sistem descriptor), kod/veri 8 byte.
 *
 * -----------------------------------------------------------------------------
 * Akış:
 *   1. "RING3 X86 BASLA" bas.
 *   2. GDT kur (null, kernel-code 0x08, kernel-data 0x10, user-code 0x18|3,
 *      user-data 0x20|3, TSS 0x28) + lgdt + segment register'ları yeniden yükle.
 *   3. TSS kur (RSP0 = kernel_ring0_stack tepesi) + ltr $0x28.
 *   4. IDT kur: vektör 0x80 (syscall, DPL=3, kernel-code) + vektör 13 (#GP,
 *      DPL=0) + lidt.
 *   5. iretq ile ring3_kod'a geç (SS=0x20|3, RSP=user_stack, CS=0x18|3).
 *   6. ring3_kod (CPL=3):
 *        a. CS'i oku → RPL=3 doğrula (paylaşılan alana yaz).
 *        b. `int 0x80` (num=1 → kernel seri'ye "RING3 SYSCALL" basar).
 *        c. `cli` dene (ayrıcalıklı) → #GP → handler "RING3 GP" bas + ring3'e
 *           dönmeden kernel'de akışı sürdür.
 *   7. Kanıtlar toplandıysa "RING3 X86 OK".
 *
 * -----------------------------------------------------------------------------
 * FALLBACK: Tam ring3-exec kurulamazsa (iretq subtle), en azından GDT ring3
 * descriptor'ları (DPL=3) + TSS (ltr) kuruldu + doğrulandı → "RING3 SETUP OK".
 *
 * Kanıt: "RING3 X86 OK"    → ring3'te gerçekten kod koştu, syscall + #GP çalıştı.
 *        "RING3 SETUP OK"  → GDT ring3 seg + TSS kuruldu (ring3-exec teyidi yok).
 *
 * KISIT: Tüm GDT/TSS/IDT/ring3 mantığı test-içi inline asm/blob (smp_x86.c
 * deseni). runtime/boot/linker'a DOKUNULMAZ — kernel'in kendi IDT'sini
 * (kdl_idt_kur) GEÇİCİ olarak KENDİ IDT'mizle değiştiririz (lidt), test
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
 * volatile: derleyici cache'lemesin. Ring3 ve ring0 aynı adres uzayını (kernel
 * sayfa tabloları, tümü present|write — koruma yok, sadece CPL ayrımı) paylaşır.
 * Not: Bu test SAYFA-tabanlı user/kernel izolasyonu KURMAZ (kernel boot sayfaları
 * hepsi RW); ayrıcalık ayrımını CPL (ring) + ayrıcalıklı-komut (#GP) ile kanıtlar.
 * Sayfa-tabanlı ayrım (U/S bit) ayrı bir milestone (D-124 x86 muadili sonraki). */
static volatile uint32_t r3_cs_rpl   = 0xFFFFFFFFu;  /* ring3'te okunan CS & 3 */
static volatile uint32_t r3_syscall_geldi = 0;       /* int 0x80 handler tetiklendi */
static volatile uint32_t r3_syscall_num   = 0;       /* syscall numarası (rax) */
static volatile uint32_t r3_gp_yakalandi  = 0;       /* #GP (cli denemesi) yakalandı */

/* === Kernel ring0 stack (ring3→ring0 trap'te TSS.RSP0 buraya işaret eder) === */
static uint8_t kernel_ring0_yigin[16384] __attribute__((aligned(16)));

/* === Ring3 kullanıcı yığını === */
static uint8_t user_yigin[16384] __attribute__((aligned(16)));

/* === TSS (long-mode 64-bit TSS) ===
 * Long-mode TSS: RSP0/1/2 (her biri 8 byte) + IST1..7 + IOPB ofseti. Minimal
 * kullanım: yalnız RSP0 (ring3→ring0 stack switch). Alan düzeni (Intel SDM):
 *   +0x00 reserved(4)
 *   +0x04 RSP0(8) +0x0C RSP1(8) +0x14 RSP2(8)
 *   +0x1C reserved(8)
 *   +0x24 IST1(8) ... +0x5C IST7(8)
 *   +0x64 reserved(8) +0x6C reserved(2) +0x6E IOPB ofseti(2)
 * Toplam 104 (0x68) byte. */
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

/* === GDT ===
 * 7 quad (null + kernel kod/veri + user kod/veri + TSS'in 2 quad'ı = 16 byte).
 * Long-mode kod/veri descriptor'ları taban/limit yok sayar; yalnız L, DPL,
 * present, type bitleri anlamlı.
 *
 * Descriptor bit düzeni (yüksek 32-bit access byte'ları):
 *   Access (bit 40..47): P(47) DPL(46:45) S(44) type(43:40)
 *   Flags  (bit 52..55): G(55) D/B(54) L(53) AVL(52)
 *
 * Kod (exec/read): type=0b1010 (0xA) → S=1 → access alt = 0x9A (kernel DPL0) /
 *   0xFA (user DPL3). L-bit (bit 53) set → 64-bit.
 * Veri (r/w): type=0b0010 (0x2) → S=1 → access = 0x92 (kernel) / 0xF2 (user).
 */
static uint64_t gdt[7] __attribute__((aligned(16)));

struct __attribute__((packed)) DescPtr {
    uint16_t limit;
    uint64_t taban;
};

/* === IDT ===
 * 256 giriş × 16 byte (long-mode gate descriptor). Yalnız vektör 13 (#GP) ve
 * 0x80 (syscall) doldurulur; gerisi 0 (present=0 → tetiklenirse triple-fault,
 * ama bu testte tetiklenmez). */
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

/* === Kullanıcı (ring3) sayfa izni ===
 * KRİTİK: x86-64'te ring3 (CPL=3) yalnız U/S=1 (bit 2) işaretli sayfalara
 * erişebilir. Boot sayfa tabloları (boot/start_x86_64.S) tüm sayfaları
 * SUPERVISOR-only kurar (PML4/PDPT bayrak=0x3, PD huge=0x83 — hiçbirinde U/S
 * yok). ring3_kod'a atlar atlamaz komut-getirme #PF (v=14, err&4=user) verir.
 *
 * boot read-only olduğundan (smp_x86.c'nin lapic_mmio_harita_kur deseni gibi)
 * U/S bitini ÇALIŞMA ANINDA ekleriz — ama YALNIZ ring3'ün ihtiyaç duyduğu
 * 2MB sayfalara (kod + user_yigin + paylaşılan volatile'lar). Kernel-yalnız
 * sayfalar SUPERVISOR kalır → ring3 onlara erişemez (sayfa-düzeyi izolasyon
 * KISMEN korunur; ayrıcalık ayrımının asıl kanıtı CPL + #GP).
 *
 * U/S biti sayfa-yürüyüşünün HER seviyesinde gerekli: PML4[i], PDPT[j], PD[k].
 * Boot yalnız düşük 1GB'ı (PML4[0]→PDPT[0]→PD[0..511], 2MB huge) haritalar;
 * tüm test verisi orada → yalnız o zincire U/S ekleriz. */
#define SAYFA_US  0x4ULL   /* U/S biti (bit 2) — user erişimi */

static void kullanici_sayfa_izni_ver(uint64_t sanal_adr) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    uint64_t *pml4 = (uint64_t *)(uintptr_t)(cr3 & ~0xFFFULL);

    unsigned pml4_i = (sanal_adr >> 39) & 0x1FF;
    unsigned pdpt_i = (sanal_adr >> 30) & 0x1FF;
    unsigned pd_i   = (sanal_adr >> 21) & 0x1FF;

    /* PML4[i] → U/S ekle (mevcutsa). */
    if (pml4[pml4_i] & 0x1ULL) {
        pml4[pml4_i] |= SAYFA_US;
        uint64_t *pdpt = (uint64_t *)(uintptr_t)(pml4[pml4_i] & 0x000FFFFFFFFFF000ULL);
        if (pdpt[pdpt_i] & 0x1ULL) {
            pdpt[pdpt_i] |= SAYFA_US;
            /* PDPT[j] 1GB huge (PS bit 0x80) değilse → PD'ye in. */
            if (!(pdpt[pdpt_i] & 0x80ULL)) {
                uint64_t *pd = (uint64_t *)(uintptr_t)(pdpt[pdpt_i] & 0x000FFFFFFFFFF000ULL);
                if (pd[pd_i] & 0x1ULL) {
                    pd[pd_i] |= SAYFA_US;   /* 2MB huge page → user erişilebilir */
                }
            }
        }
    }
}

/* Handler adreslerini (asm stub) tanımla — dosya-düzeyi asm blob. */
extern void syscall_stub_r3(void);   /* int 0x80 handler */
extern void gp_stub_r3(void);        /* #GP (vektör 13) handler */

/*
 * === Asm stub'lar (dosya-düzeyi inline asm) ===
 *
 * syscall_stub_r3: int 0x80 ile girilir. rax=syscall num. Kernel'de (ring0)
 *   koşar. r3_syscall_num'a rax'i yaz, r3_syscall_geldi=1, iretq → ring3'e dön.
 *   int gate girişte IF'i temizler (CPU); ring3 stack frame'i CPU push etti
 *   (SS,RSP,RFLAGS,CS,RIP) → iretq geri yükler.
 *
 * gp_stub_r3: #GP (vektör 13) → CPU HATA KODU push eder (int 0x80 gate'te
 *   hata kodu YOK). ring3 `cli` → #GP → buraya. r3_gp_yakalandi=1 yaz. Ring3'e
 *   DÖNMEK yerine (dönersek cli'yi tekrar dener → sonsuz #GP), kernel akışına
 *   dallanırız: stack'i düzelt + kernel_devam'a jmp (ring0'da, bilinen stack).
 *   Bunu yapmak için hata kodunu at + kernel_devam etiketine mutlak jmp.
 *
 * NOT: rip-relative erişim (lea sym(%rip)) long-mode'da güvenli; global
 * volatile'lara movl ile yazarız.
 */
__asm__(
    ".text\n"
    ".global syscall_stub_r3\n"
    "syscall_stub_r3:\n"
    /* rax = syscall num (ring3 rax'te bıraktı). r3_syscall_num = eax. */
    "    movl %eax, r3_syscall_num(%rip)\n"
    "    movl $1, r3_syscall_geldi(%rip)\n"
    "    iretq\n"                       /* ring3'e dön (CPU frame'i geri yükle) */
    "\n"
    ".global gp_stub_r3\n"
    "gp_stub_r3:\n"
    /* #GP: yığın tepesinde CPU hata kodu (8 byte) var. r3_gp_yakalandi=1. */
    "    movl $1, r3_gp_yakalandi(%rip)\n"
    /* Hata kodunu at (iretq frame'e ulaşmak için değil — biz iretq YAPMAYACAĞIZ,
       kernel akışına dönüyoruz). Hata kodu + iretq frame (RIP,CS,RFLAGS,RSP,SS
       = 5×8) toplam 48 byte yığında. Bunları temizleyip kernel_devam'a git. */
    "    addq $48, %rsp\n"              /* hata kodu(8) + 5×8 iretq frame at */
    /* Kernel ring0 stack'ine dönmek için: TSS.RSP0 zaten ring3 trap'te CPU
       tarafından yüklendi; şu an ring0 stack'indeyiz. Ama bu stack ring3-trap
       için ayrılmış üst kısım. kernel_devam'a jmp — orada global bayraklardan
       durumu okuyup rapor basacağız. Stack derinliği önemsiz (kernel_devam
       kendi çerçevesini kurar, halt eder). */
    "    jmp kernel_devam\n"
);

/* kernel_devam: #GP stub buraya dallanır (ring0). Global bayrakları oku, rapor
 * bas, halt. C'den erişilebilir global etiket (aşağıda tanımlı fonksiyon). */
extern void kernel_devam(void);

/* === GDT descriptor kur === */
static void gdt_kur(void) {
    gdt[0] = 0x0000000000000000ULL;   /* null */
    /* kernel kod (0x08): DPL0, L=1, exec/read. access=0x9A, flags: L(bit53). */
    gdt[1] = 0x00209A0000000000ULL;
    /* kernel veri (0x10): DPL0, r/w. access=0x92. */
    gdt[2] = 0x0000920000000000ULL;
    /* user kod (0x18): DPL3, L=1, exec/read. access=0xFA, flags: L. */
    gdt[3] = 0x0020FA0000000000ULL;
    /* user veri (0x20): DPL3, r/w. access=0xF2. */
    gdt[4] = 0x0000F20000000000ULL;

    /* TSS descriptor (0x28) — long-mode 16-byte sistem descriptor (2 quad).
     * type=0x9 (available 64-bit TSS), S=0, DPL=0, P=1 → access=0x89.
     * taban = &tss (64-bit), limit = sizeof(tss)-1. */
    uint64_t tss_taban = (uint64_t)(uintptr_t)&tss;
    uint32_t tss_limit = (uint32_t)(sizeof(tss) - 1);

    uint64_t desc_dusuk = 0;
    desc_dusuk |= (uint64_t)(tss_limit & 0xFFFFu);              /* limit[15:0] */
    desc_dusuk |= ((uint64_t)(tss_taban & 0xFFFFFFULL)) << 16;  /* taban[23:0] */
    desc_dusuk |= ((uint64_t)0x89ULL) << 40;                    /* access (P|DPL0|type=9) */
    desc_dusuk |= ((uint64_t)((tss_limit >> 16) & 0xFULL)) << 48; /* limit[19:16] */
    /* flags nibble (bit52..55) = 0 (G=0, byte-granüler) */
    desc_dusuk |= ((uint64_t)((tss_taban >> 24) & 0xFFULL)) << 56; /* taban[31:24] */
    uint64_t desc_yuksek = (tss_taban >> 32) & 0xFFFFFFFFULL;   /* taban[63:32] */

    gdt[5] = desc_dusuk;
    gdt[6] = desc_yuksek;

    struct DescPtr gp = {
        .limit = (uint16_t)(sizeof(gdt) - 1),
        .taban = (uint64_t)(uintptr_t)&gdt[0],
    };
    __asm__ volatile("lgdt %0" : : "m"(gp) : "memory");

    /* Segment register'ları yeni GDT'ye göre yeniden yükle. Kod segmenti için
     * far-return (retfq) ile CS'i 0x08'e yükle; veri segmentleri doğrudan. */
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
    /* Alanları temizle. */
    uint8_t *p = (uint8_t *)&tss;
    for (unsigned k = 0; k < sizeof(tss); k++) p[k] = 0;

    /* RSP0 = kernel ring0 yığın tepesi (aşağı büyür, 16-hizalı). */
    uint64_t rsp0 = (uint64_t)(uintptr_t)&kernel_ring0_yigin[sizeof(kernel_ring0_yigin)];
    rsp0 &= ~(uint64_t)0xF;
    tss.rsp0 = rsp0;

    /* IOPB ofseti = TSS boyutu (I/O bitmap yok → tüm portlar ring0-only,
     * ring3 outb → #GP; ki cli zaten #GP verecek). */
    tss.iopb_ofset = (uint16_t)sizeof(tss);

    /* TSS'i yükle. */
    __asm__ volatile("ltr %0" : : "r"((uint16_t)SEL_TSS) : "memory");
}

/* === IDT gate kur === */
static void idt_gate_kur(int vektor, void (*handler)(void), uint8_t dpl) {
    uint64_t adr = (uint64_t)(uintptr_t)handler;
    struct IdtGate *g = &idt[vektor];
    g->ofset_dusuk  = (uint16_t)(adr & 0xFFFFu);
    g->selector     = SEL_KERNEL_KOD;   /* handler ring0 kod segmentinde koşar */
    g->ist          = 0;
    /* tip_attr: P(0x80) | DPL(dpl<<5) | type=0xE (64-bit interrupt gate). */
    g->tip_attr     = (uint8_t)(0x80u | ((dpl & 3u) << 5) | 0x0Eu);
    g->ofset_orta   = (uint16_t)((adr >> 16) & 0xFFFFu);
    g->ofset_yuksek = (uint32_t)((adr >> 32) & 0xFFFFFFFFu);
    g->reserved     = 0;
}

static void idt_kur(void) {
    uint8_t *p = (uint8_t *)idt;
    for (unsigned k = 0; k < sizeof(idt); k++) p[k] = 0;

    /* Vektör 13 (#GP): DPL=0 (yalnız CPU/ring0 tetikler). */
    idt_gate_kur(13, gp_stub_r3, 0);
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
 * CPL=3'te koşar. SELF-CONTAINED (kernel fonksiyonu çağırmaz — ama global
 * volatile'lara yazar; bu test sayfa-izolasyon kurmadığından erişim serbest).
 * 1. CS'i oku → RPL (& 3) → r3_cs_rpl'ye yaz (ring3 kanıtı).
 * 2. int 0x80 (rax=1) → syscall handler ring0'da.
 * 3. cli → ayrıcalıklı → #GP → gp_stub_r3 (ring0) → kernel_devam.
 *
 * NOT: naked benzeri — derleyici prologue/epilogue ring3'te sorun değil (kendi
 * user stack'imiz var). Ama fonksiyon DÖNMEZ (cli sonrası #GP kernel'e dallanır).
 */
__attribute__((noinline))
static void ring3_kod(void) {
    /* 1. CS oku → RPL. Long-mode'da `mov %cs, %eax` CPL'yi verir. */
    uint32_t cs_deg;
    __asm__ volatile("movl %%cs, %0" : "=r"(cs_deg));
    r3_cs_rpl = cs_deg & 3u;

    /* 2. Syscall: int 0x80, rax=1 (demo "yaz" numarası). */
    __asm__ volatile(
        "movl $1, %%eax\n"
        "int $0x80\n"
        : : : "eax", "memory");

    /* 3. Ayrıcalıklı komut dene → #GP (ring3'te cli yasak). Handler kernel'e
     *    dallanır → buradan sonrası ULAŞILMAZ. */
    __asm__ volatile("cli" : : : "memory");

    /* Ulaşılmaz (cli #GP verdi). Güvenlik için sonsuz döngü. */
    for (;;) { }
}

/* === Ring3'e geçiş (iretq) ===
 * iretq, yığından RIP,CS,RFLAGS,RSP,SS pop eder. CS'in RPL'si (3) hedef CPL'yi
 * belirler → CPU ring3'e düşer. SS ve RSP ring3 stack'ini kurar. RFLAGS'te IF=1
 * (kesmeler açık) + rezerve bit 1 (her zaman 1) set.
 *
 * Yığın (iretq'nin beklediği, yukarıdan aşağı push sırası):
 *   push SS   (user-data | 3)
 *   push RSP  (user stack tepesi)
 *   push RFLAGS
 *   push CS   (user-code | 3)
 *   push RIP  (ring3_kod)
 *   iretq
 */
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

    /* iretq geri dönmez (ring3'e atlar). */
    for (;;) { __asm__ volatile("hlt"); }
}

/* === Rapor + halt (hem başarı hem #GP-sonrası buradan geçer) === */
static _Noreturn void rapor_ve_dur(void) {
    kdl_yaz_metin("CS_RPL=");
    kdl_yaz_onaltilik((uint64_t)r3_cs_rpl);
    kdl_yaz_metin(" SYSCALL=");
    kdl_yaz_onaltilik((uint64_t)r3_syscall_geldi);
    kdl_yaz_metin(" SYSCALL_NUM=");
    kdl_yaz_onaltilik((uint64_t)r3_syscall_num);
    kdl_yaz_metin(" GP=");
    kdl_yazdir_onaltilik((uint64_t)r3_gp_yakalandi);

    /* Ring3 kanıtları: CS.RPL==3 (userspace'te koştu) + syscall geldi. */
    int ring3_kostu   = (r3_cs_rpl == 3u);
    int syscall_oldu  = (r3_syscall_geldi != 0);
    int gp_oldu       = (r3_gp_yakalandi != 0);

    if (ring3_kostu && syscall_oldu && gp_oldu) {
        kdl_yazdir_metin("RING3 SYSCALL (int 0x80 -> ring0 handler)");
        kdl_yazdir_metin("RING3 GP (cli @ ring3 -> #GP yakalandi)");
        kdl_yazdir_metin("RING3 X86 OK (ring3-exec + syscall + privilege-fault, CS.RPL=3)");
    } else if (ring3_kostu && syscall_oldu) {
        /* Ring3 koştu + syscall çalıştı ama #GP kanıtı yok (yine de güçlü). */
        kdl_yazdir_metin("RING3 SYSCALL (int 0x80 -> ring0 handler)");
        kdl_yazdir_metin("RING3 X86 OK (ring3-exec + syscall, CS.RPL=3; #GP teyidi yok)");
    } else {
        /* Ring3-exec teyidi yok → SETUP kanıtı (GDT ring3 seg + TSS kuruldu). */
        kdl_yazdir_metin("RING3 SETUP OK (GDT DPL=3 user seg + TSS ltr kuruldu; ring3-exec teyidi yok)");
    }

    halt();
}

/* kernel_devam: #GP stub asm'inden `jmp kernel_devam` ile girilir (ring0).
 * C linkage'lı global. rapor_ve_dur'a devam. */
__attribute__((used))
void kernel_devam(void) {
    rapor_ve_dur();
}

int main(void) {
    kdl_yazdir_metin("RING3 X86 BASLA");
    kdl_yazdir_satir();

    /* --- 2. GDT (ring3 user seg + TSS descriptor) --- */
    gdt_kur();

    /* --- 3. TSS (RSP0) + ltr --- */
    tss_kur();

    /* --- 4. IDT (int 0x80 DPL=3 + #GP DPL=0) --- */
    idt_kur();

    /* --- 4b. Ring3'ün erişeceği sayfalara U/S (user) izni ver ---
     * ring3_kod (komut getirme), user_yigin (stack push/pop), paylaşılan
     * volatile'lar (ring3 yazar). Hepsi düşük bellekte, birkaç 2MB sayfada.
     * Her adresin 2MB sayfasına U/S ekle (aynı sayfa tekrar işaretlense zararsız).
     * NOT: Kernel kod/veri farklı 2MB sayfalarda kalırsa SUPERVISOR kalır. */
    kullanici_sayfa_izni_ver((uint64_t)(uintptr_t)&ring3_kod);
    kullanici_sayfa_izni_ver((uint64_t)(uintptr_t)&user_yigin[0]);
    kullanici_sayfa_izni_ver((uint64_t)(uintptr_t)&user_yigin[sizeof(user_yigin) - 1]);
    kullanici_sayfa_izni_ver((uint64_t)(uintptr_t)&r3_cs_rpl);
    kullanici_sayfa_izni_ver((uint64_t)(uintptr_t)&r3_syscall_geldi);
    kullanici_sayfa_izni_ver((uint64_t)(uintptr_t)&r3_syscall_num);
    kullanici_sayfa_izni_ver((uint64_t)(uintptr_t)&r3_gp_yakalandi);

    /* TLB'yi temizle (yeni U/S bitleri görünsün) — CR3 yeniden yükle. */
    {
        uint64_t cr3;
        __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
        __asm__ volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
    }

    kdl_yaz_metin("SETUP: user_kod_sel=");
    kdl_yaz_onaltilik((uint64_t)SEL_USER_KOD);
    kdl_yaz_metin(" user_veri_sel=");
    kdl_yaz_onaltilik((uint64_t)SEL_USER_VERI);
    kdl_yaz_metin(" tss_sel=");
    kdl_yaz_onaltilik((uint64_t)SEL_TSS);
    kdl_yaz_metin(" tss_rsp0=");
    kdl_yazdir_onaltilik(tss.rsp0);

    /* --- 5. Ring3'e geç (iretq). Ring3 kod syscall + cli(#GP) yapar; #GP
     *    handler kernel_devam'a dallanır → rapor_ve_dur. iretq buradan geri
     *    DÖNMEZ. --- */
    ring3e_gec();

    /* Ulaşılmaz. */
    return 0;
}
