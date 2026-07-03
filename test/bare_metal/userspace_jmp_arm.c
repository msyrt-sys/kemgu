/*
 * MİLESTONE C testi (aarch64) — EL0 setjmp/longjmp (userspace YEREL-OLMAYAN ATLAMA).
 *
 * D-203 (userspace_fiber_arm.c) EL0-içi kooperatif fiber context-switch'i (naked
 * fiber_gec: callee-saved x19-x30 + sp save/restore) kurdu. Bu test o mekanizmanın
 * bir ADIM ötesini — TEK bir EL0 akış İÇİNDE yerel-olmayan kontrol akışı (C
 * exception / longjmp-benzeri "geri sıçrama") — TAMAMEN userspace'te (kernel yardımı
 * yok: timer/IRQ/syscall-switch yok, saf EL0 asm) kurar.
 *
 * setjmp/longjmp semantiği (POSIX):
 *   u_setjmp(buf):      callee-saved x19-x30 + sp'yi buf'a kaydet, 0 DÖNDÜR (ilk çağrı).
 *   u_longjmp(buf, v):  buf'tan geri yükle, u_setjmp'in ÇAĞRILDIĞI yere v ile "geri dön"
 *                       (v!=0). Ara-katman stack frame'leri (derin çağrı zinciri) ATLANIR.
 *
 * Anahtar mekanizma (D-203 naked fiber_gec ile aynı iskelet):
 *   - jmp_buf düzeni fiber TCB'ye benzer: [sp],[x30/lr],x19-x28,x29(fp).
 *   - u_longjmp yüklenen x30'a `ret` → u_setjmp'in DÖNÜŞ-NOKTASINA (çağrı sonrası)
 *     atlar. Yani `bl u_setjmp` çağrı-noktasının hemen sonrasına düşülür — ama bu
 *     sefer x0=v (u_setjmp'in ilk 0 dönüşü yerine). Ara frame'ler örtük atlanır.
 *   - u_setjmp x0=0 ile döner; u_longjmp x0=v ile AYNI dönüş-noktasına döner → caller
 *     iki farklı dönüş değeri görür (0 = ilk kurulum, v = geri sıçrama).
 *   - naked + .user + noinline: GERÇEK fonksiyon (bl ile çağrılır), derleyici
 *     prolog/epilog üretmez (asm tam kontrol); .user → EL0-exec (AP=01, UXN=0).
 *
 * Senaryo (DETERMİNİSTİK, bounded):
 *   launcher (EL0 main-eşdeğeri) u_setjmp(buf) çağırır → 0 döner (ilk kurulum) →
 *   "SETJMP0" basar → derin_zincir() çağırır (3 katman iç içe: kat1→kat2→kat3) →
 *   en derin katman u_longjmp(buf, 42) yapar → kontrol u_setjmp'e GERİ SIÇRAR ama bu
 *   sefer 42 döner → ara katmanlar (kat1/kat2/kat3 dönüşleri) ATLANDI → sayaç ile
 *   doğrula: setjmp iki kez ziyaret edildi (0 sonra 42) ve en-derin katman ulaşıldı →
 *   "USERJMP OK". main-eşdeğeri "USERJMP BASLA".
 *
 * Kanıt: u_setjmp dönüşü 0 (ilk) → derin çağrı → u_longjmp(buf,42) → u_setjmp'e
 * 42 ile dönüş (yerel-olmayan sıçrama) → ara frame'ler atlandı → "USERJMP OK".
 */
#include <stdint.h>
extern void kdl_el0_calistir(void (*fn)(void), void *sp);
extern unsigned char __user_stack_top[];

/* Userspace syscall sarmalayıcı — always_inline: SVC .user section'a gömülür
 * (ayrı fonksiyon .text/AP=00'da kalır → EL0 çalıştıramaz → fault). */
__attribute__((always_inline)) static inline unsigned long sys(unsigned long num, unsigned long arg) {
    register unsigned long x8 __asm__("x8") = num;
    register unsigned long x0 __asm__("x0") = arg;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8) : "memory");
    return x0;
}

/* === jmp_buf (EL0 yerel-olmayan atlama bağlamı) ===
 * D-203 fiber TCB düzeniyle AYNI aile (callee-saved + sp + lr):
 *   [0]=sp, [1]=x30(lr/dönüş-noktası), [2..11]=x19-x28, [12]=x29(fp).
 * .user_data'da (EL0-yazılabilir sayfa) → jmp_buf EL0'da tutulur. 13 slot (fiber
 * FiberBaglam ile aynı boyut — TCB ailesi tutarlılığı). */
typedef struct {
    uint64_t kayit[13];
} JmpBuf;

/* === u_setjmp(buf) — EL0 setjmp ===
 * Callee-saved x19-x30 + sp'yi buf'a kaydet, x0=0 döndür. u_longjmp SONRADAN aynı
 * buf'tan geri yükleyip bu fonksiyonun DÖNÜŞ-NOKTASINA (buf[1]=kaydedilen x30) atlar.
 * Caller-saved register'lar C ABI ile çağrı-noktasında korunur → yalnız callee-saved.
 *
 * naked + .user + noinline: bl ile çağrılır; derleyici prolog/epilog üretmez, asm
 * tam kontrol. Parametre buf=x0 (AAPCS). Son 'ret' KAYDEDİLEN LR'a değil, ÇAĞIRAN'ın
 * LR'ına (x30, u_setjmp çağrılırken set edilen) döner — normal fonksiyon dönüşü. */
__attribute__((naked, section(".user"), noinline))
static long u_setjmp(JmpBuf *buf) {
    /* naked: buf=x0 doğrudan asm'de kullanılır (compiler register tahsisi yok).
     * sp → [x0,#0]; x30(çağrı sonrası dönüş-noktası) → [x0,#8]; x19-x28 → [16..],
     * x29 → [96]. x0=0 (ilk setjmp dönüşü). */
    __asm__ volatile(
        "mov     x9, sp               \n"   /* sp doğrudan okunamaz → x9 üzerinden */
        "str     x9, [x0, #0]         \n"   /* [0] = sp */
        "str     x30, [x0, #8]        \n"   /* [1] = x30 (dönüş-noktası = çağrı sonrası) */
        "stp     x19, x20, [x0, #16]  \n"   /* [2..3] = x19,x20 */
        "stp     x21, x22, [x0, #32]  \n"   /* [4..5] = x21,x22 */
        "stp     x23, x24, [x0, #48]  \n"   /* [6..7] = x23,x24 */
        "stp     x25, x26, [x0, #64]  \n"   /* [8..9] = x25,x26 */
        "stp     x27, x28, [x0, #80]  \n"   /* [10..11] = x27,x28 */
        "str     x29, [x0, #96]       \n"   /* [12] = x29 (fp) */
        "mov     x0, #0               \n"   /* dönüş değeri = 0 (ilk çağrı) */
        "ret                          \n");
}

/* === u_longjmp(buf, val) — EL0 longjmp (YEREL-OLMAYAN sıçrama) ===
 * buf'tan callee-saved + sp + lr geri yükle, x0=val ayarla, RET → u_setjmp'in
 * DÖNÜŞ-NOKTASINA (buf[1]) atlar. Ara-katman stack frame'leri örtük atlanır: sp
 * u_setjmp anındaki değere geri döner, x30 u_setjmp'in çağrı-sonrası adresine döner.
 * val 0 ise POSIX gereği 1'e yükseltilir (u_setjmp asla iki kez 0 dönmemeli).
 *
 * naked + .user + noinline: parametreler buf=x0, val=x1 (AAPCS). Son 'ret' YÜKLENEN
 * x30'a (u_setjmp dönüş-noktası) dallanır → yerel-olmayan atlama. */
__attribute__((naked, section(".user"), noinline))
static void u_longjmp(JmpBuf *buf, long val) {
    /* naked: buf=x0, val=x1 doğrudan. sp,x30,x19-x28,x29'u buf'tan yükle; x0=val
     * (0 ise 1). Son ret yüklenen x30'a → u_setjmp dönüş-noktası (ama x0=val). */
    __asm__ volatile(
        "ldr     x9, [x0, #0]         \n"   /* x9 = kaydedilen sp */
        "ldr     x30, [x0, #8]        \n"   /* x30 = u_setjmp dönüş-noktası */
        "ldp     x19, x20, [x0, #16]  \n"
        "ldp     x21, x22, [x0, #32]  \n"
        "ldp     x23, x24, [x0, #48]  \n"
        "ldp     x25, x26, [x0, #64]  \n"
        "ldp     x27, x28, [x0, #80]  \n"
        "ldr     x29, [x0, #96]       \n"   /* x29 = kaydedilen fp */
        "mov     sp, x9               \n"   /* sp geri yüklendi → ara frame'ler atlandı */
        "mov     x0, x1               \n"   /* dönüş değeri = val */
        "cmp     x0, #0               \n"   /* val==0 ise POSIX gereği 1'e yükselt */
        "csinc   x0, x0, xzr, ne      \n"   /* x0 = (val!=0) ? val : 0+1 */
        "ret                          \n");   /* → u_setjmp dönüş-noktası (x0=val) */
}

/* === EL0 setjmp/longjmp durumu (.user_data — EL0-erişimli sayfa) === */
__attribute__((section(".user_data"))) static JmpBuf atlama_buf;

/* İlerleme kanıtı sayaçları. */
__attribute__((section(".user_data"))) static volatile int en_derine_ulasildi;
__attribute__((section(".user_data"))) static volatile int ara_kat_donusu;   /* longjmp atlarsa 0 kalmalı */

/* === Derin çağrı zinciri (3 katman iç içe) ===
 * kat1 → kat2 → kat3. En derin (kat3) u_longjmp(buf,42) yapar → kontrol doğrudan
 * u_setjmp'e sıçrar; kat1/kat2/kat3'ün NORMAL dönüşleri (aşağıdaki ara_kat_donusu
 * artırımları) ASLA çalışmaz (yerel-olmayan atlama kanıtı). */
__attribute__((section(".user"), noinline))
static void kat3(void) {
    en_derine_ulasildi = 1;                 /* en derin katmana gerçekten inildi */
    sys(5, (unsigned long)(uintptr_t)"K3");
    sys(7, 0);
    u_longjmp(&atlama_buf, 42);             /* YEREL-OLMAYAN sıçrama → u_setjmp'e */
    ara_kat_donusu = 100;                   /* ULAŞILMAZ (longjmp atladı) */
    for (;;) { }                            /* ulaşılmaz */
}

__attribute__((section(".user"), noinline))
static void kat2(void) {
    sys(5, (unsigned long)(uintptr_t)"K2");
    sys(7, 0);
    kat3();
    ara_kat_donusu += 10;                   /* ULAŞILMAZ (kat3 longjmp ile atladı) */
}

__attribute__((section(".user"), noinline))
static void kat1(void) {
    sys(5, (unsigned long)(uintptr_t)"K1");
    sys(7, 0);
    kat2();
    ara_kat_donusu += 1;                    /* ULAŞILMAZ (kat2 hiç dönmedi) */
}

/* === EL0 launcher (main-eşdeğeri, .user) ===
 * u_setjmp(buf) kur → 0 dönerse derin zinciri çalıştır → derin katman longjmp ile
 * buraya (u_setjmp'e) 42 ile geri sıçrar → ikinci ziyarette doğrula. */
__attribute__((section(".user"), noinline))
static void launcher(void) {
    sys(5, (unsigned long)(uintptr_t)"USERJMP BASLA");
    sys(7, 0);

    en_derine_ulasildi = 0;
    ara_kat_donusu = 0;

    long r = u_setjmp(&atlama_buf);
    if (r == 0) {
        /* İlk çağrı: setjmp kuruldu (0 döndü). Derin zinciri başlat. */
        sys(5, (unsigned long)(uintptr_t)"SETJMP0");
        sys(7, 0);
        kat1();                             /* kat1→kat2→kat3→u_longjmp(buf,42) */
        /* kat1 NORMAL dönerse buraya gelir — longjmp atladığı için ULAŞILMAZ. */
        sys(5, (unsigned long)(uintptr_t)"USERJMP HATA-normal-donus");
        sys(7, 0);
    } else {
        /* Yerel-olmayan geri sıçrama: setjmp ikinci kez, bu sefer r=42. */
        sys(5, (unsigned long)(uintptr_t)"LONGJMP");
        sys(6, (unsigned long)(unsigned int)(int)r);   /* → "LONGJMP42" */
        sys(7, 0);
        /* Determinizm doğrula: r==42 (longjmp değeri) + en derine inildi +
         * hiçbir ara-katman NORMAL dönüşü çalışmadı (ara_kat_donusu==0). */
        if (r == 42 && en_derine_ulasildi == 1 && ara_kat_donusu == 0) {
            sys(5, (unsigned long)(uintptr_t)"USERJMP OK");
        } else {
            sys(5, (unsigned long)(uintptr_t)"USERJMP HATA");
        }
        sys(7, 0);
    }

    sys(3, 0);            /* cik → kernel'de dur */
    for (;;) { }          /* ulaşılmaz */
}

int main(void) {
    /* .user sayfası (0x42000000) kdl_mmu_kur'da AP=01 → EL0 kod + jmp_buf
     * (.user_data) + launcher yığını burada. EL0'a düş + launcher'ı koştur. */
    kdl_el0_calistir(launcher, __user_stack_top);
    return 0;   /* ulaşılmaz (launcher cik ile kernel'de durur) */
}
