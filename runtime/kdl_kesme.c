/*
 * KEMGU Bare-Metal Kesme/İstisna İşleyicileri (kdl_kesme.c)
 * ========================================================
 *
 * C3 (OS exception/interrupt). Mimari-spesifik vektör/IDT asm stub'ları
 * (boot/start_aarch64.S, boot/start_x86_64.S) bağlamı toplayıp buradaki C
 * işleyicilerini çağırır.
 *
 *   kdl_istisna_isle  — CPU istisnası (fault): kurtarma YOK → yazdır + halt.
 *                       Vektör-tablosu öncesi sessiz çöküş yerine teşhis.
 *   kdl_kesme_isle    — IRQ (donanım kesmesi): C3b dispatch tablosu (timer vb.).
 *
 * KISIT: bu işleyiciler bölge/frame allocator KULLANMAZ. Frame allocator
 * tek-thread (IRQ-safe değil) → kesme bağlamında tahsis reentrancy bozar.
 * Yalnız UART yazımı (yazdir_bare, tahsissiz) + register okuma.
 *
 * Bağımlılık: yazdir_bare (kdl_yazdir_*). Freestanding, libc yok.
 */
#include <stdint.h>

void kdl_yazdir_metin(const char *);
void kdl_yazdir_satir(void);
void kdl_yazdir_onaltilik(uint64_t);

/* CPU istisnası — kurtarma yok. Parametreler mimari-spesifik:
 *   aarch64: (vektör_tipi, ESR_EL2, ELR_EL2)
 *   x86_64 : (vektör_no, hata_kodu, RIP) */
__attribute__((noreturn))
void kdl_istisna_isle(uint64_t tip, uint64_t a, uint64_t b) {
    /* C8c: fault adresi — data/instruction abort hangi adrese erişti.
     * aarch64 FAR_EL1, x86 CR2 (#PF lineer adresi). Abort-dışı için stale ama
     * zararsız teşhis. */
    uint64_t adr = 0;
#if defined(__aarch64__)
    __asm__ volatile("mrs %0, far_el1" : "=r"(adr));
#elif defined(__x86_64__)
    __asm__ volatile("mov %%cr2, %0" : "=r"(adr));
#endif
    kdl_yazdir_metin("ISTISNA tip=0x");
    kdl_yazdir_onaltilik(tip);
    kdl_yazdir_metin(" a=0x");
    kdl_yazdir_onaltilik(a);
    kdl_yazdir_metin(" b=0x");
    kdl_yazdir_onaltilik(b);
    kdl_yazdir_metin(" adr=0x");
    kdl_yazdir_onaltilik(adr);
    kdl_yazdir_satir();
    for (;;) {
#if defined(__aarch64__)
        __asm__ volatile("wfe");
#elif defined(__x86_64__)
        __asm__ volatile("hlt");
#else
        /* boş döngü */
#endif
    }
}

/* === Sistem çağrısı dispatch (C6) ===
 * Kullanıcı/kernel kodu SVC (aarch64) / int 0x80 (x86) ile çağırır. Boot asm
 * stub'ı bağlamı kaydeder, num + arg ile buraya gelir, dönüşte eret/iretq.
 * Minimal demonstrasyon: çağrı #1 = mesaj yazdır. Gerçek kernel'de bu tablo
 * dosya/bellek/görev syscall'larına genişler. */
void kdl_syscall_isle(uint64_t num, uint64_t arg) {
    (void)arg;
    if (num == 1) {
        kdl_yazdir_metin("SYSCALL OK num=1");
        kdl_yazdir_satir();
    }
}

#if defined(__x86_64__)
/* === x86_64 IDT kurulumu (C3a) ===
 * aarch64 vektörleri VBAR ile asm'de kurulur; x86 IDT'si gate-tablosu gerektirir
 * (offset alanları parçalı → C'de kurmak temiz). isr0..isr31 stub'ları
 * boot/start_x86_64.S'te; kdl_isr_tablo onların adreslerini taşır. Boot,
 * long_entry'de main'den ÖNCE kdl_idt_kur() çağırır. */
struct KdlIdtKapi {
    uint16_t offset_dusuk;
    uint16_t segment;        /* 0x08 = 64-bit kod segmenti */
    uint8_t  ist;
    uint8_t  tip_oz;         /* 0x8E = present, DPL0, interrupt gate */
    uint16_t offset_orta;
    uint32_t offset_yuksek;
    uint32_t rezerve;
} __attribute__((packed));

static struct KdlIdtKapi kdl_idt[256];
extern void *kdl_isr_tablo[32];   /* asm: isr0..isr31 adresleri */

extern void kdl_irq0_stub(void);     /* boot/start_x86_64.S — IRQ0 (timer) */
extern void kdl_syscall_stub(void);  /* boot/start_x86_64.S — int 0x80 syscall */

static void kdl_idt_gate(int i, uint64_t adr) {
    kdl_idt[i].offset_dusuk  = (uint16_t)(adr & 0xFFFF);
    kdl_idt[i].segment       = 0x08;       /* 64-bit kod segmenti */
    kdl_idt[i].ist           = 0;
    kdl_idt[i].tip_oz        = 0x8E;       /* present, DPL0, interrupt gate */
    kdl_idt[i].offset_orta   = (uint16_t)((adr >> 16) & 0xFFFF);
    kdl_idt[i].offset_yuksek = (uint32_t)((adr >> 32) & 0xFFFFFFFF);
    kdl_idt[i].rezerve       = 0;
}

void kdl_idt_kur(void) {
    for (int i = 0; i < 32; i++)
        kdl_idt_gate(i, (uint64_t)(uintptr_t)kdl_isr_tablo[i]);   /* istisnalar 0-31 */
    kdl_idt_gate(32, (uint64_t)(uintptr_t)&kdl_irq0_stub);        /* IRQ0 → vektör 32 */
    kdl_idt_gate(0x80, (uint64_t)(uintptr_t)&kdl_syscall_stub);   /* int 0x80 → syscall */

    struct {
        uint16_t limit;
        uint64_t taban;
    } __attribute__((packed)) idtr = {
        (uint16_t)(sizeof(kdl_idt) - 1),
        (uint64_t)(uintptr_t)kdl_idt
    };
    __asm__ volatile("lidt %0" : : "m"(idtr));
}
#endif
