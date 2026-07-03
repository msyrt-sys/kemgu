/*
 * KEMGU-OS bare-metal PREEMPTIVE scheduler testi (x86_64) — PIT timer-IRQ ile
 * ZORUNLU bağlam-değiştirme. C7b (aarch64 preempt_arm.c) testinin x86 İKİZİ.
 * ============================================================================
 *
 * Milestone F: aarch64'te preemption (C7b, kdl_preempt + timer-IRQ SP swap)
 * ÇALIŞIYOR; x86'da yalnız COOPERATIVE (C7a sched_test) vardı. Bu test x86'a
 * gerçek PREEMPTIVE round-robin getirir: iki kernel-görev busy-loop yapar ve
 * ASLA yield ETMEZ — yalnız PIT (8254) timer'ının IRQ0'ı onları preempt eder.
 *
 * -----------------------------------------------------------------------------
 * x86 preemption neden aarch64'ten SUBTLE:
 *   aarch64'te timer-IRQ girişi tek vektör (VBAR IRQ slot) + boot asm full
 *   trap-frame kaydeder → C handler SP swap yapar → eret. Segment/PIC yok.
 *   x86-64'te ise GEREKLİ altyapı (hepsi bu testte KENDİ-KURULUMLU):
 *     1. Kendi IDT (256 gate) + `lidt` → kernel IDT'sini GEÇİCİ değiştir.
 *     2. PIC (8259) remap: master IRQ0..7 → vektör 0x20..0x27 (boot varsayılan
 *        0x08..0x0F CPU exception'larıyla çakışır → remap ZORUNLU). Slave 0x28+.
 *        Yalnız IRQ0 (timer) maskesiz; gerisi maskeli.
 *     3. PIT (8254) kanal 0, mod 3 (kare dalga), bölen ~11932 → ~100 Hz → IRQ0.
 *     4. IRQ0 gate: DPL=0, 64-bit interrupt gate (0x8E). CPU girişte IF temizler.
 *     5. IRQ0 asm stub: TAM GP trap-frame kaydet (push r15..rax) → C scheduler
 *        (round-robin RSP swap) → PIC EOI (port 0x20 ← 0x20) → frame geri yükle
 *        → iretq.
 *   Bağlam-değiştirme x86'da RSP swap ile: her görevin kaydedilmiş-RSP slotu;
 *   IRQ girişte CPU iretq-frame'i (SS,RSP,RFLAGS,CS,RIP) + stub GP register'ları
 *   AKTİF görevin yığınına iter. Sonraki göreve geçiş = RSP'yi onun kayıtlı
 *   RSP'siyle değiştir → onun push'ladığı frame pop edilir → iretq onun
 *   RIP'sine döner. İLK geçişte görev-B yığınına SENTETİK iretq-frame + sıfır
 *   GP register'lar kurulur (henüz hiç preempt edilmediği için).
 *
 * -----------------------------------------------------------------------------
 * Akış:
 *   1. "PREEMPT X86 BASLA" bas.
 *   2. Görev B için sentetik başlangıç yığını kur (iretq-frame → gorev_b + 15
 *      sıfır GP register). Kayıtlı-RSP[B] = o yığının tepesi.
 *   3. Kendi IDT'yi kur (yalnız vektör 0x20 = IRQ0 dolu), lidt.
 *   4. PIC remap (master 0x20+, slave 0x28+) + yalnız IRQ0 maskesiz.
 *   5. PIT ~100 Hz kur (port 0x43 ← 0x36, port 0x40 ← bölen düşük/yüksek).
 *   6. `sti` → kesmeler açık. AKTİF görev = A (main).
 *   7. Görev A (main): busy-loop, sayac_a artır, YIELD ETMEZ. Timer preempt
 *      eder → B'ye geçilir. B sayac_b artırır (preemption KANITI). A yeniden
 *      preempt-geri-gelince sayac_a>=hedef && sayac_b>0 → "PREEMPT X86 OK".
 *
 * Kanıt: "PREEMPT X86 OK" → B görevi YIELD ÇAĞIRMADAN timer-IRQ ile koştu
 *        (sayac_b>0), zorunlu bağlam-değiştirme çalışıyor.
 *
 * KISIT: Tüm IDT/PIC/PIT/context-switch mantığı test-içi inline asm/blob
 * (smp_x86.c + ring3_x86.c deseni). runtime/boot/linker'a DOKUNULMAZ —
 * kernel'in kendi IDT'sini (kdl_idt_kur) GEÇİCİ olarak KENDİ IDT'mizle
 * değiştiririz (lidt), test bitince önemsiz (hlt-loop).
 *
 * DETERMİNİSTİK: bounded döngüler; başarı tespitinde hlt-loop (timeout ile
 * ölür = beklenen). Birden çok QEMU koşusu byte-identik çıktı verir.
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
#define SEL_KERNEL_VERI 0x10u

/* === Round-robin görev durumu ===
 * 2 görev: 0 = A (main), 1 = B. kayitli_rsp[i] = görev i'nin en son kaydedilmiş
 * yığın işaretçisi (RSP). aktif = şu an koşan görev indeksi.
 * volatile: hem C hem asm (IRQ bağlamı) erişir; derleyici cache'lememeli. */
static volatile uint64_t kayitli_rsp[2] = { 0, 0 };
static volatile uint32_t aktif = 0;

static volatile uint32_t sayac_a = 0;   /* görev A tik sayacı */
static volatile uint32_t sayac_b = 0;   /* görev B tik sayacı — PREEMPTION KANITI */
static volatile uint32_t irq_sayaci = 0;/* toplam IRQ0 (timer) sayısı */

/* === Görev B yığını (aşağı büyür) === */
static uint8_t yigin_b[16384] __attribute__((aligned(16)));

/* === Görev B ===
 * Busy-loop + sayac_b artır. YIELD ÇAĞIRMAZ — yalnız timer-IRQ preempt eder.
 * Bu fonksiyona İLK giriş sentetik iretq-frame ile (aşağıda gorev_b_yigin_kur).
 * DÖNMEZ (kernel-görev; preempt edilir ya da halt). */
__attribute__((noreturn))
static void gorev_b(void) {
    for (;;) {
        for (volatile int i = 0; i < 200000; i++) { }
        sayac_b++;
    }
}

/*
 * === IRQ0 (timer) asm stub ===
 *
 * PIT IRQ0 vektör 0x20'den girilir. CPU iretq-frame'i (SS,RSP,RFLAGS,CS,RIP)
 * AKTİF görevin yığınına itti + IF'i temizledi (interrupt gate). Stub:
 *   1. TAM GP register set kaydet (push rax..r15 — 15 register; rsp CPU frame'de).
 *   2. RSP'yi C scheduler'a (rdi) geçir → scheduler round-robin seçer, aktif
 *      görevin kayitli_rsp'sini yazar, sonraki görevin kayitli_rsp'sini DÖNER.
 *   3. Dönen RSP'ye geç (bağlam-değiştirme — sonraki görevin yığını).
 *   4. GP register'ları o yığından pop et (sonraki görevin kaydettiği veya
 *      sentetik sıfırlar).
 *   5. iretq → sonraki görevin RIP'sine dön.
 *
 * PIC EOI scheduler C fonksiyonunda gönderilir (port 0x20 ← 0x20) — RSP swap'ten
 * ÖNCE, henüz eski bağlamdayken; EOI sadece port I/O, bağlamdan bağımsız.
 *
 * Push sırası (yığında yukarıdan aşağı, iretq-frame'in ALTINA):
 *   [CPU: SS RSP RFLAGS CS RIP]  ← CPU itti (frame tepesi)
 *   rax rbx rcx rdx rsi rdi rbp r8 r9 r10 r11 r12 r13 r14 r15  ← stub itti
 *   ← RSP burada (kayitli_rsp'ye yazılan değer)
 *
 * pop sırası TERS olmalı (r15 önce pop). scheduler_sec RSP'yi rax'te döner.
 */
extern void irq0_stub(void);
extern uint64_t scheduler_sec(uint64_t mevcut_rsp);   /* C — RSP swap kararı */

__asm__(
    ".text\n"
    ".global irq0_stub\n"
    "irq0_stub:\n"
    /* --- 1. TAM GP register kaydet (aktif görevin yığınına) --- */
    "    pushq %rax\n"
    "    pushq %rbx\n"
    "    pushq %rcx\n"
    "    pushq %rdx\n"
    "    pushq %rsi\n"
    "    pushq %rdi\n"
    "    pushq %rbp\n"
    "    pushq %r8\n"
    "    pushq %r9\n"
    "    pushq %r10\n"
    "    pushq %r11\n"
    "    pushq %r12\n"
    "    pushq %r13\n"
    "    pushq %r14\n"
    "    pushq %r15\n"
    /* --- 2. scheduler_sec(mevcut_rsp) → yeni_rsp (rax) ---
     * SysV ABI: 1. arg rdi, dönüş rax. mevcut RSP = şu anki yığın tepesi. */
    "    movq %rsp, %rdi\n"
    "    call scheduler_sec\n"
    /* --- 3. Bağlam-değiştir: RSP = dönen (sonraki görevin kayıtlı RSP'si) --- */
    "    movq %rax, %rsp\n"
    /* --- 4. GP register geri yükle (sonraki görevin kaydettiği/sentetik) --- */
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
    "    popq %rax\n"
    /* --- 5. iretq → sonraki görevin RIP'sine (CPU frame'i geri yükler) --- */
    "    iretq\n"
);

/* === PIC EOI + port yardımcıları === */
static inline void outb(uint16_t port, uint8_t deger) {
    __asm__ volatile("outb %0, %1" : : "a"(deger), "Nd"(port) : "memory");
}

/*
 * === C scheduler (asm stub'dan çağrılır) ===
 * mevcut_rsp = preempt edilen görevin yığın tepesi (tam GP frame kaydedildikten
 * sonra). Round-robin: aktif görevin kayitli_rsp'sini yaz, sonraki görevi seç,
 * onun kayitli_rsp'sini DÖN. PIC EOI'yi burada gönder (RSP swap'ten önce; sadece
 * port I/O). aktif güncellenir. Görev sayaçlarını da burada artırabiliriz ama
 * A/B sayaçlarını görevlerin KENDİSİ artırır (preemption kanıtı net kalsın).
 *
 * NOT: Bu fonksiyon IRQ bağlamında (IF=0) koşar; UART'a DOKUNMAZ (yalnız port
 * I/O + bellek). Deterministik round-robin (2 görev arası kesin dönüşüm).
 */
uint64_t scheduler_sec(uint64_t mevcut_rsp);   /* prototip (asm .global çağırır) */
uint64_t scheduler_sec(uint64_t mevcut_rsp) {
    irq_sayaci++;

    /* PIC EOI — master 8259, port 0x20 ← 0x20 (specific/non-specific EOI). */
    outb(0x20, 0x20);

    /* Aktif görevin kayıtlı RSP'sini güncelle. */
    uint32_t eski = aktif;
    kayitli_rsp[eski] = mevcut_rsp;

    /* Round-robin: 0 ↔ 1. */
    uint32_t yeni = (eski == 0) ? 1u : 0u;
    aktif = yeni;

    /* Sonraki görevin kayıtlı RSP'sini dön (asm buraya geçer). */
    return kayitli_rsp[yeni];
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
    g->selector     = SEL_KERNEL_KOD;
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

    /* Yalnız vektör 0x20 (IRQ0 = PIT timer, remap sonrası). DPL=0. */
    idt_gate_kur(0x20, irq0_stub, 0);

    struct DescPtr ip = {
        .limit = (uint16_t)(sizeof(idt) - 1),
        .taban = (uint64_t)(uintptr_t)&idt[0],
    };
    __asm__ volatile("lidt %0" : : "m"(ip) : "memory");
}

/*
 * === PIC (8259) remap ===
 * Boot varsayılanı: master IRQ0..7 → CPU vektör 0x08..0x0F (double-fault, TSS,
 * #GP gibi exception'larla ÇAKIŞIR). Long-mode'da IRQ'yi kullanmak için remap
 * ZORUNLU: master → 0x20..0x27, slave → 0x28..0x2F. Ardından tüm IRQ'leri
 * maskele, yalnız IRQ0 (timer) aç.
 *
 * ICW dizisi (standart 8259 init):
 *   ICW1 (0x11): init + ICW4 var.
 *   ICW2: vektör tabanı (master 0x20, slave 0x28).
 *   ICW3: master bit3 (slave IRQ2'de), slave kaskad kimliği 2.
 *   ICW4 (0x01): 8086/88 modu.
 *   OCW1 (mask): master 0xFE (yalnız IRQ0 açık), slave 0xFF (tümü maskeli).
 */
#define PIC1_KOMUT  0x20u
#define PIC1_VERI   0x21u
#define PIC2_KOMUT  0xA0u
#define PIC2_VERI   0xA1u

static void pic_remap(void) {
    /* ICW1: başlat (kaskad + ICW4). */
    outb(PIC1_KOMUT, 0x11);
    outb(PIC2_KOMUT, 0x11);
    /* ICW2: vektör tabanları. */
    outb(PIC1_VERI, 0x20);   /* master → 0x20 */
    outb(PIC2_VERI, 0x28);   /* slave  → 0x28 */
    /* ICW3: kaskad topolojisi. */
    outb(PIC1_VERI, 0x04);   /* master: slave IRQ2'de (bit2) */
    outb(PIC2_VERI, 0x02);   /* slave: kaskad kimliği 2 */
    /* ICW4: 8086 modu. */
    outb(PIC1_VERI, 0x01);
    outb(PIC2_VERI, 0x01);
    /* OCW1: maskeler. Master yalnız IRQ0 açık (bit0=0), gerisi maskeli. */
    outb(PIC1_VERI, 0xFE);   /* 1111_1110 → IRQ0 açık */
    outb(PIC2_VERI, 0xFF);   /* slave tümü maskeli */
}

/*
 * === PIT (8254) ~100 Hz ===
 * Kanal 0, mod 3 (kare dalga), 16-bit binary, lobyte+hibyte. Giriş saati
 * 1193182 Hz. Bölen = 1193182 / 100 ≈ 11932 → ~100.00 Hz. Komut byte 0x36:
 *   bit7-6 = 00 (kanal 0), bit5-4 = 11 (lobyte sonra hibyte),
 *   bit3-1 = 011 (mod 3, kare dalga), bit0 = 0 (16-bit binary).
 */
#define PIT_KANAL0  0x40u
#define PIT_KOMUT   0x43u
#define PIT_BOLEN   11932u   /* 1193182 / 100 ≈ 11932 → ~100 Hz */

static void pit_100hz(void) {
    outb(PIT_KOMUT, 0x36);
    outb(PIT_KANAL0, (uint8_t)(PIT_BOLEN & 0xFF));         /* lobyte */
    outb(PIT_KANAL0, (uint8_t)((PIT_BOLEN >> 8) & 0xFF));  /* hibyte */
}

/*
 * === Görev B başlangıç yığını (sentetik iretq-frame) ===
 * Görev B henüz hiç preempt edilmediği için yığınında gerçek bir kaydedilmiş
 * bağlam YOK. İlk kez ona geçildiğinde (scheduler B'yi seçince), asm stub onun
 * kayitli_rsp'sinden GP register'ları pop edip iretq yapar. Bu yüzden yığını
 * ELLE, stub'ın BEKLEDİĞİ düzende kurarız (yukarıdan aşağı):
 *
 *   [iretq-frame: SS, RSP, RFLAGS, CS, RIP]  ← iretq bunları pop eder
 *   [15 GP register: r15..rax (pop sırası)]  ← stub pop eder
 *   ← kayitli_rsp[B] buraya işaret eder (en düşük adres = pop başlangıcı)
 *
 * iretq için RIP = gorev_b, CS = kernel kod, RFLAGS = IF=1 (0x202: bit9 IF +
 * rezerve bit1), RSP = B yığın tepesi (iretq sonrası B'nin çalışacağı stack),
 * SS = kernel veri. GP register'lar sıfır (temiz başlangıç).
 *
 * Yığın byte-hizası: iretq-frame RSP alanı 16-hizalı olmalı (ABI). Sentetik
 * frame'i 16-hizaya oturtacak şekilde kurarız.
 */
static void gorev_b_yigin_kur(void) {
    /* B yığınının tepesi (aşağı büyür), 16-hizalı. */
    uint64_t tepe = (uint64_t)(uintptr_t)&yigin_b[sizeof(yigin_b)];
    tepe &= ~(uint64_t)0xF;   /* 16-hizala */

    /* Yığına 64-bit değer it (aşağı büyür → önce azalt, sonra yaz). */
    uint64_t *sp = (uint64_t *)(uintptr_t)tepe;

    /* --- iretq-frame (yüksek adresten düşüğe: SS, RSP, RFLAGS, CS, RIP) --- */
    /* iretq pop sırası: RIP, CS, RFLAGS, RSP, SS (RIP en düşük adreste). */
    *(--sp) = SEL_KERNEL_VERI;                          /* SS */
    *(--sp) = tepe;                                     /* RSP (iretq sonrası B stack) */
    *(--sp) = 0x202ULL;                                 /* RFLAGS: IF=1 + rezerve bit1 */
    *(--sp) = SEL_KERNEL_KOD;                           /* CS */
    *(--sp) = (uint64_t)(uintptr_t)&gorev_b;            /* RIP → gorev_b */

    /* --- 15 GP register (stub pop sırası: r15,r14,...,rax → yani yığında
     *     rax en düşük adreste; pop r15 ilk = en yüksek GP adres). Sıfır kur. --- */
    for (int k = 0; k < 15; k++) {
        *(--sp) = 0ULL;
    }

    /* kayitli_rsp[B] = sp (stub buradan pop'a başlar). */
    kayitli_rsp[1] = (uint64_t)(uintptr_t)sp;
}

int main(void) {
    kdl_yazdir_metin("PREEMPT X86 BASLA");
    kdl_yazdir_satir();

    /* --- 2. Görev B sentetik başlangıç yığını --- */
    gorev_b_yigin_kur();
    aktif = 0;              /* başlangıçta A (main) koşuyor */

    /* --- 3. Kendi IDT (yalnız IRQ0) + lidt --- */
    idt_kur();

    /* --- 4. PIC remap (master 0x20+, yalnız IRQ0 maskesiz) --- */
    pic_remap();

    /* --- 5. PIT ~100 Hz --- */
    pit_100hz();

    kdl_yaz_metin("SETUP: IDT+PIC+PIT kuruldu, PIT_bolen=");
    kdl_yaz_onaltilik((uint64_t)PIT_BOLEN);
    kdl_yaz_metin(" gorev_b_rsp=");
    kdl_yazdir_onaltilik(kayitli_rsp[1]);

    /* --- 6. Kesmeleri aç (sti) → PIT IRQ0 artık A'yı preempt edebilir --- */
    __asm__ volatile("sti" ::: "memory");

    /* --- 7. Görev A (main): busy-loop, YIELD ETMEZ. Timer preempt eder →
     *    B koşar (sayac_b>0). A yeterince tik + B çalıştıysa → PREEMPT X86 OK.
     *    Bounded: sayac_a bir üst sınıra ulaşınca (B koşmadıysa bile) döngüden
     *    çık + rapor (deterministik, sonsuz beklemez). --- */
    for (;;) {
        for (volatile int i = 0; i < 200000; i++) { }
        sayac_a++;

        /* B en az bir kez preemptively koştu + A birkaç tik → KANIT.
         * NOT: sayac_a eşiği (3) ile senkron; sayac_b'nin KESİN değeri timer-vs-CPU
         * jitter'ıyla koşudan koşuya 1-2 değişebilir (deterministik-değil) → onu
         * HAM basmayız; yalnız "sayac_b>0" KALİTATİF gerçeğini basarız (kanıt
         * koşulu >= 1 zaten yukarıda denetlendi). Böylece çıktı byte-identik. */
        if (sayac_a >= 3 && sayac_b >= 1) {
            /* Kesmeleri kapat (rapor sırasında tekrar preempt olmayalım). */
            __asm__ volatile("cli" ::: "memory");

            kdl_yazdir_metin(
                "PREEMPT X86 OK (gorev B YIELD cagirmadan timer-IRQ ile kostu, sayac_b>0)");
            halt();
        }

        /* Emniyet üst sınırı: A çok tik attı ama B hâlâ koşmadıysa (preemption
         * çalışmadı) → deterministik başarısızlık raporu, sonsuz beklemez. */
        if (sayac_a >= 5000) {
            __asm__ volatile("cli" ::: "memory");
            kdl_yaz_metin("sayac_a=");
            kdl_yaz_onaltilik((uint64_t)sayac_a);
            kdl_yaz_metin(" sayac_b=");
            kdl_yaz_onaltilik((uint64_t)sayac_b);
            kdl_yaz_metin(" irq0=");
            kdl_yazdir_onaltilik((uint64_t)irq_sayaci);
            kdl_yazdir_metin(
                "PREEMPT X86 BASARISIZ (gorev B kosmadi, sayac_b=0 — preemption yok)");
            halt();
        }
    }
}
