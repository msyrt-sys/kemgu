/*
 * KEMGU-OS bare-metal — TAM x86 syscall ABI testi (int 0x80 çok-argüman +
 * dönüş-değeri). Milestone F. ============================================
 *
 * aarch64'ün zengin syscall ABI'si (D-122 argüman geçişi, D-126 dönüş-değeri +
 * çok-argüman + register-şeffaflık) x86'da EKSİKTİ: yalnız D-190 ring3_x86.c
 * `int 0x80` demo'su vardı (tek num, arg/dönüş yok). Bu test x86'a TAM syscall
 * ABI getirir: kendi-kurulumlu IDT[0x80] gate + handler; num=rax, arg0=rdi,
 * arg1=rsi, dönüş=rax (System V benzeri kural).
 *
 * -----------------------------------------------------------------------------
 * ABI (System V benzeri, syscall bağlamı):
 *   num   → rax   (syscall numarası)
 *   arg0  → rdi   (SysV 1. tamsayı argümanı)
 *   arg1  → rsi   (SysV 2. tamsayı argümanı)
 *   dönüş → rax   (syscall sonucu; çağıran rax'te alır)
 *   Diğer TÜM register'lar (rbx, rcx, rdx, rbp, r8..r15) DEĞİŞMEDEN korunur.
 *
 * -----------------------------------------------------------------------------
 * D-126 REGISTER-ŞEFFAFLIK DERSİ (KRİTİK):
 *   aarch64'te (D-121) EXC vektör-stub'ı x0'ı vektör-indeksiyle EZİYORDU →
 *   argüman kayboluyordu. Ders: handler ÖNCE tüm çağıran register'ları (trap
 *   frame) kaydeder, SONRA okur/dispatch eder. x86'da da aynısı: int 0x80 gate
 *   girişte yalnız CPU iretq-frame'i (SS,RSP,RFLAGS,CS,RIP) push eder; GP
 *   register'lar (rax=num, rdi=arg0, rsi=arg1 dâhil) DOKUNULMAMIŞ çağıran
 *   değerleridir. Stub bunları ÖNCE yığına kaydeder (pushaq-benzeri), sonra C
 *   dispatcher yığından okur. Dönüş: C sonucu SAVE-EDİLMİŞ rax slotuna yazar →
 *   pop rax onu geri yükler → iretq → çağıran rax'te sonucu alır. Böylece num
 *   (rax) argümanları EZMEZ ve dönüş çağırana ŞEFFAF ulaşır.
 *
 * -----------------------------------------------------------------------------
 * Gösterilen 3 syscall:
 *   num=1  yaz(ptr)        : rdi = C-string ptr → kernel seri'ye basar. dönüş 0.
 *   num=2  topla(a, b)     : rdi=a, rsi=b → dönüş = a+b (ÇOK-ARG + DÖNÜŞ kanıtı).
 *   num=3  gettick(x)      : dönüş = x + 0x1000 (echo/dönüş kanıtı; sabit ofset
 *                            → deterministik, gerçek tik sayacı yerine).
 *   num=? bilinmeyen       : dönüş = -1 (0xFFFF...) — hata sınırı.
 *
 * -----------------------------------------------------------------------------
 * Akış:
 *   1. "SYSCALL X86 BASLA" bas.
 *   2. Kendi IDT'yi kur (yalnız vektör 0x80 = syscall gate, DPL=0 ring0 yeterli).
 *      lidt → kernel IDT'sini GEÇİCİ değiştir (test bitince halt, önemsiz).
 *   3. Ring0'dan üç syscall yap:
 *        a. syscall(2, 40, 2) → 42 bekle (çok-arg + dönüş).
 *        b. syscall(1, "...")  → kernel çıktısı (yaz).
 *        c. syscall(3, 41)     → 41+0x1000 bekle (dönüş).
 *      Ayrıca register-şeffaflık: rbx'e kanary koy → syscall(2,...) sonrası
 *      rbx DEĞİŞMEMİŞ olmalı (handler tüm register'ları korur).
 *   4. Üçü de doğruysa (+ rbx korundu) → "SYSCALL X86 OK".
 *
 * Kanıt: "SYSCALL X86 OK" → int 0x80 syscall ABI'si num/arg0/arg1/dönüş +
 *        register-şeffaflık ile TAM çalışıyor (aarch64 D-126 paritesi).
 *
 * KISIT: Tüm IDT/gate/handler mantığı test-içi inline asm/blob (preempt_x86.c +
 * ring3_x86.c deseni). runtime/boot/linker'a DOKUNULMAZ — kernel'in kendi
 * IDT'sini (kdl_idt_kur) GEÇİCİ olarak KENDİ IDT'mizle değiştiririz (lidt).
 *
 * DETERMİNİSTİK: sabit argümanlar + sabit ofset → birden çok QEMU koşusu
 * byte-identik çıktı verir. Başarı/başarısızlık tespitinde hlt-loop.
 */
#include <stdint.h>

extern void kdl_yazdir_metin(const char *);    /* metin + newline */
extern void kdl_yaz_metin(const char *);        /* metin, newline'siz */
extern void kdl_yazdir_satir(void);
extern void kdl_yaz_onaltilik(uint64_t);        /* onaltilik, newline'siz */
extern void kdl_yazdir_onaltilik(uint64_t);     /* onaltilik + newline */

static _Noreturn void halt(void) { for (;;) { __asm__ volatile("hlt"); } }

/* === Segment seçiciler (boot GDT — start_x86_64.S) === */
#define SEL_KERNEL_KOD  0x08u

/*
 * === Paylaşılan durum (handler ↔ ana akış) ===
 * volatile: hem C dispatcher (int 0x80 bağlamı) hem ana akış erişir. num=1
 * (yaz) handler'ın seri'ye bastığını doğrulamak için sayaç tutar.
 */
static volatile uint32_t yaz_sayaci = 0;      /* num=1 kaç kez çağrıldı */
static volatile uint64_t son_topla_a = 0;     /* num=2 son arg0 (teşhis) */
static volatile uint64_t son_topla_b = 0;     /* num=2 son arg1 (teşhis) */

/*
 * === int 0x80 asm stub ===
 *
 * int 0x80 gate girişte CPU iretq-frame'i (SS,RSP,RFLAGS,CS,RIP) yığına iter +
 * IF'i temizler (interrupt gate). GP register'lar (rax=num, rdi=arg0, rsi=arg1)
 * çağıranın DOKUNULMAMIŞ değerleridir.
 *
 * D-126 dersi (register-şeffaflık): ÖNCE tüm GP register'ları kaydet, SONRA C
 * dispatcher'a geç. Dönüş: C sonucu save-edilmiş rax slotuna yazar → pop rax
 * onu geri yükler → çağıran rax'te alır.
 *
 * Push sırası (yığında yukarıdan aşağı, iretq-frame'in ALTINA):
 *   [CPU: SS RSP RFLAGS CS RIP]  ← CPU itti (frame tepesi)
 *   rax rbx rcx rdx rsi rdi rbp r8 r9 r10 r11 r12 r13 r14 r15  ← stub itti
 *   ← RSP burada
 *
 * C dispatcher SysV ABI: 1.arg rdi = num, 2.arg rsi = arg0, 3.arg rdx = arg1;
 * dönüş rax. Stub, kaydedilmiş num/arg0/arg1'i bu register'lara koyar, çağırır,
 * dönen rax'i SAVE-EDİLMİŞ rax slotuna yazar.
 *
 * Yığın düzeni (push sonrası, rsp'den itibaren artan ofset — 8 byte adımlar):
 *   [rsp+0]   r15
 *   [rsp+8]   r14
 *   [rsp+16]  r13
 *   [rsp+24]  r12
 *   [rsp+32]  r11
 *   [rsp+40]  r10
 *   [rsp+48]  r9
 *   [rsp+56]  r8
 *   [rsp+64]  rbp
 *   [rsp+72]  rdi   ← arg0 (kaydedilmiş)
 *   [rsp+80]  rsi   ← arg1 (kaydedilmiş)
 *   [rsp+88]  rdx
 *   [rsp+96]  rcx
 *   [rsp+104] rbx
 *   [rsp+112] rax   ← num (kaydedilmiş); dönüş buraya yazılır
 *
 * pop sırası TERS (r15 ilk pop). rax en son pop → C'nin yazdığı sonuç geri
 * yüklenir. Böylece rax HARİÇ tüm register'lar çağırana ŞEFFAF geri döner,
 * rax ise syscall sonucunu taşır.
 */
extern void syscall_stub_x86(void);
extern uint64_t syscall_dispatch(uint64_t num, uint64_t arg0, uint64_t arg1);

__asm__(
    ".text\n"
    ".global syscall_stub_x86\n"
    "syscall_stub_x86:\n"
    /* --- 1. TAM GP register kaydet (çağıranın frame'i) --- */
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
    /* --- 2. Kaydedilmiş num/arg0/arg1'i C dispatcher arg register'larına oku ---
     * SysV: rdi=1.arg(num), rsi=2.arg(arg0), rdx=3.arg(arg1). Yığından oku. */
    "    movq 112(%rsp), %rdi\n"   /* num  = kaydedilmiş rax */
    "    movq 72(%rsp),  %rsi\n"   /* arg0 = kaydedilmiş rdi */
    "    movq 80(%rsp),  %rdx\n"   /* arg1 = kaydedilmiş rsi */
    /* --- 3. C dispatcher çağır → sonuç rax'te --- */
    "    call syscall_dispatch\n"
    /* --- 4. Sonucu SAVE-EDİLMİŞ rax slotuna yaz (dönüş-değeri şeffaflığı) --- */
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
    /* --- 6. iretq → çağırana dön (CPU iretq-frame'i geri yükler) --- */
    "    iretq\n"
);

/*
 * === C syscall dispatcher (asm stub'dan çağrılır) ===
 * num'a göre dallanır, sonucu DÖNER (rax). IF=0 bağlamında (int gate) koşar
 * ama UART port I/O bağlamdan bağımsız → num=1 (yaz) güvenle basabilir.
 *
 * DETERMİNİSTİK: sabit hesap (a+b, x+0x1000) → koşudan koşuya aynı.
 */
uint64_t syscall_dispatch(uint64_t num, uint64_t arg0, uint64_t arg1);   /* prototip */
uint64_t syscall_dispatch(uint64_t num, uint64_t arg0, uint64_t arg1) {
    switch (num) {
        case 1:   /* yaz(ptr): rdi = C-string → seri'ye bas. dönüş 0. */
            yaz_sayaci++;
            kdl_yaz_metin("  [syscall yaz] ");
            kdl_yazdir_metin((const char *)(uintptr_t)arg0);
            return 0;

        case 2:   /* topla(a, b): a + b (ÇOK-ARG + DÖNÜŞ). */
            son_topla_a = arg0;
            son_topla_b = arg1;
            return arg0 + arg1;

        case 3:   /* gettick(x): x + 0x1000 (echo/dönüş; deterministik sabit ofset). */
            return arg0 + 0x1000ULL;

        default:  /* bilinmeyen → -1 (hata sınırı). */
            return (uint64_t)-1;
    }
}

/* === IDT (256 × 16-byte long-mode gate) === */
struct __attribute__((packed)) IdtGate {
    uint16_t ofset_dusuk;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  tip_attr;
    uint16_t ofset_orta;
    uint32_t ofset_yuksek;
    uint32_t reserved;
};
static struct IdtGate idt[256] __attribute__((aligned(16)));

struct __attribute__((packed)) DescPtr {
    uint16_t limit;
    uint64_t taban;
};

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

    /* Yalnız vektör 0x80 (syscall). DPL=0 (ring0 syscall yeterli — bu test
     * userspace/ring3 kurmuyor; ring0'dan int 0x80 çağırır). ABI mantığı
     * (num/arg0/arg1/dönüş + register-şeffaflık) DPL'den bağımsız aynıdır. */
    idt_gate_kur(0x80, syscall_stub_x86, 0);

    struct DescPtr ip = {
        .limit = (uint16_t)(sizeof(idt) - 1),
        .taban = (uint64_t)(uintptr_t)&idt[0],
    };
    __asm__ volatile("lidt %0" : : "m"(ip) : "memory");
}

/*
 * === Userspace syscall sarmalayıcı (ring0'dan int 0x80) ===
 *
 * num → rax, arg0 → rdi, arg1 → rsi; int 0x80; dönüş rax'te. GCC extended asm:
 *   "+a"(num_dönüş) → rax hem giriş (num) hem çıkış (dönüş).
 *   "D"(arg0) → rdi, "S"(arg1) → rsi (giriş).
 * clobber: "memory" (yan-etkiler görünsün) + "cc". Diğer GP register'ları
 * handler koruduğu için clobber gerekmez — ama derleyiciye int 0x80'in
 * çağrı-clobber register'larını (rcx, rdx, r8..r11) bozabileceğini varsaydırmak
 * güvenli değil çünkü handler HEPSİNİ korur; yine de emniyet için "cc","memory".
 */
static inline uint64_t syscall2(uint64_t num, uint64_t arg0, uint64_t arg1) {
    uint64_t sonuc = num;
    __asm__ volatile(
        "int $0x80\n"
        : "+a"(sonuc)                 /* rax: num giriş → dönüş çıkış */
        : "D"(arg0), "S"(arg1)        /* rdi=arg0, rsi=arg1 */
        : "memory", "cc");
    return sonuc;
}
static inline uint64_t syscall1(uint64_t num, uint64_t arg0) {
    return syscall2(num, arg0, 0);
}

int main(void) {
    kdl_yazdir_metin("SYSCALL X86 BASLA");
    kdl_yazdir_satir();

    /* --- 2. Kendi IDT (yalnız vektör 0x80) + lidt --- */
    idt_kur();

    kdl_yaz_metin("SETUP: IDT[0x80] gate kuruldu, handler=");
    kdl_yazdir_onaltilik((uint64_t)(uintptr_t)&syscall_stub_x86);

    /* --- 3a. num=2 topla(40, 2) → 42 bekle (ÇOK-ARG + DÖNÜŞ) ---
     * Ayrıca register-şeffaflık: rbx'e bilinen kanary koy, syscall sonrası
     * rbx DEĞİŞMEMİŞ olmalı (handler tüm register'ları korur — D-126 dersi). */
    uint64_t rbx_kanary_once = 0xCAFEB0BADEADBEEFULL;
    uint64_t rbx_sonra = 0;
    uint64_t topla_sonuc = 0;
    __asm__ volatile(
        "movq %2, %%rbx\n"       /* rbx = kanary */
        "movq $2, %%rax\n"       /* num = 2 (topla) */
        "movq $40, %%rdi\n"      /* arg0 = 40 */
        "movq $2, %%rsi\n"       /* arg1 = 2 */
        "int $0x80\n"
        "movq %%rax, %0\n"       /* topla_sonuc = dönüş */
        "movq %%rbx, %1\n"       /* rbx_sonra = korunan rbx */
        : "=r"(topla_sonuc), "=r"(rbx_sonra)
        : "r"(rbx_kanary_once)
        : "rax", "rbx", "rdi", "rsi", "memory", "cc");

    kdl_yaz_metin("  topla(40,2)=");
    kdl_yaz_onaltilik(topla_sonuc);
    kdl_yaz_metin(" rbx_korundu=");
    kdl_yazdir_onaltilik((uint64_t)(rbx_sonra == rbx_kanary_once));

    /* --- 3b. num=1 yaz(ptr) → kernel seri çıktısı --- */
    (void)syscall1(1, (uint64_t)(uintptr_t)"merhaba syscall (x86 int 0x80)");

    /* --- 3c. num=3 gettick(41) → 41 + 0x1000 = 0x1029 bekle (DÖNÜŞ) --- */
    uint64_t tick_sonuc = syscall1(3, 41);
    kdl_yaz_metin("  gettick(41)=");
    kdl_yazdir_onaltilik(tick_sonuc);

    /* --- 3d. bilinmeyen num=99 → -1 (hata sınırı, opsiyonel kanıt) --- */
    uint64_t bilinmeyen = syscall1(99, 0);
    kdl_yaz_metin("  syscall(99)=");
    kdl_yazdir_onaltilik(bilinmeyen);

    /* --- 4. Kanıt değerlendirme --- */
    int cok_arg_donus_ok = (topla_sonuc == 42);                 /* 40 + 2 */
    int reg_seffaf_ok    = (rbx_sonra == rbx_kanary_once);      /* rbx korundu */
    int yaz_ok           = (yaz_sayaci == 1);                   /* yaz handler koştu */
    int tick_donus_ok    = (tick_sonuc == (41ULL + 0x1000ULL)); /* 0x1029 */
    int hata_sinir_ok    = (bilinmeyen == (uint64_t)-1);        /* bilinmeyen → -1 */

    kdl_yaz_metin("KANIT: cok_arg+donus=");
    kdl_yaz_onaltilik((uint64_t)cok_arg_donus_ok);
    kdl_yaz_metin(" reg_seffaf=");
    kdl_yaz_onaltilik((uint64_t)reg_seffaf_ok);
    kdl_yaz_metin(" yaz=");
    kdl_yaz_onaltilik((uint64_t)yaz_ok);
    kdl_yaz_metin(" tick_donus=");
    kdl_yaz_onaltilik((uint64_t)tick_donus_ok);
    kdl_yaz_metin(" hata_sinir=");
    kdl_yazdir_onaltilik((uint64_t)hata_sinir_ok);

    if (cok_arg_donus_ok && reg_seffaf_ok && yaz_ok && tick_donus_ok && hata_sinir_ok) {
        kdl_yazdir_metin(
            "SYSCALL X86 OK (int 0x80: num=rax arg0=rdi arg1=rsi donus=rax + register-seffaflik)");
    } else {
        kdl_yazdir_metin(
            "SYSCALL X86 BASARISIZ (ABI kaniti eksik — cok-arg/donus/reg-seffaflik/yaz)");
    }

    halt();
}
