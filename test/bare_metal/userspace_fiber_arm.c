/*
 * D-201 testi (aarch64) — USERSPACE KOOPERATİF FİBER'LAR (EL0 yeşil-thread).
 *
 * Kernel D-113/C7a'da bağlam-değiştirme (kdl_baglam_degis: callee-saved x19-x30 +
 * sp kaydet/yükle, ret) yaptı. Bu test o mekanizmanın USERSPACE (EL0-içi) muadilini
 * kurar: TEK bir EL0 süreç İÇİNDE birden çok kooperatif fiber (yeşil-thread), kendi
 * aralarında fiber_yield() ile geçiş yapar — KERNEL YARDIMI OLMADAN (timer/IRQ yok,
 * preemption yok, syscall-tabanlı switch yok). Saf EL0 kullanıcı-uzayı asm ile
 * callee-saved register + SP save/restore.
 *
 * Neden önemli: bir EL0 program, kernel'in context-switch primitifini KENDİ içinde
 * yeniden üretir → tek çekirdek-thread üzerinde N mantıksal akış (M:1 threading).
 * Bu, kooperatif runtime'ların (KEMGU `görev` ileride) userspace temelidir.
 *
 * Fiber bağlamı = callee-saved (x19-x30) + SP (kdl_baglam_degis TCB düzeniyle aynı:
 * [0..9]=x19-x28, [10]=x29, [11]=x30/lr, [12]=sp). Her fiber KENDİ EL0 yığınına
 * sahip (.user_data → 0x42xxxxxx, EL0-erişimli AP=01 sayfası). İlk geçişte "yeni"
 * fiber'ın bağlamı elle kurulur (sp = yığın tepesi 16-hizalı, lr = fiber girişi) →
 * ilk fiber_gec o girişe ret ile atlar. Sonraki geçişler kaldığı yerden sürer.
 *
 * Senaryo (DETERMİNİSTİK): launcher (EL0 main-eşdeğeri) fiber A + B'yi kurar,
 * A'ya geçer. A sayaç artırır + "A1" basar + yield → B'ye. B sayaç artırır + "B1"
 * basar + yield → A. TUR sayısı bounded (sonsuz ping-pong değil). Beklenen çıktı:
 * A1,B1,A2,B2,A3,B3 → kooperatif interleave = userspace context-switch çalıştı.
 *
 * Kanıt: iki fiber'ın çıktısı BEKLENEN sırada interleave + her iki sayaç > 0 →
 * "USERFIBER OK". main-eşdeğeri "USERFIBER BASLA".
 */
#include <stdint.h>
extern void kdl_yazdir_metin(const char *);
extern void kdl_yazdir_satir(void);
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

/* === Fiber bağlamı ===
 * kdl_baglam_degis TCB düzeniyle AYNI (boot/start_aarch64.S):
 *   [0..9]=x19-x28, [10]=x29(fp), [11]=x30(lr/giriş), [12]=sp.
 * Bu yapı .user_data'da (EL0-yazılabilir sayfa) → fiber bağlamları EL0'da tutulur. */
typedef struct {
    uint64_t kayit[13];
} FiberBaglam;

/* === fiber_gec(eski, yeni) — EL0 kooperatif bağlam-değiştirme ===
 * kdl_baglam_degis'in EL0 muadili (boot/start_aarch64.S ile aynı stp/ldp deseni).
 * Mevcut fiber'ın callee-saved (x19-x30) + sp'sini *eski'ye kaydet, *yeni'den yükle,
 * RET → yeni fiber kaldığı yerden (veya ilk kez girişinden) sürer. Caller-saved
 * register'lar C ABI'siyle çağrı-noktasında zaten korunur → yalnız callee-saved+sp.
 *
 * naked + .user + noinline: GERÇEK fonksiyon (bl ile çağrılır). naked → derleyici
 * prolog/epilog üretmez, asm tam kontrol; son 'ret' YÜKLENEN x30'a dallanır (yeni
 * fiber). Parametreler AAPCS ile x0=eski, x1=yeni. .user → EL0-exec (AP=01, UXN=0);
 * ayrı .text (AP=00) olsaydı EL0 fetch-fault. Ret adres = x30, ilk girişte fiber
 * girişi (fiber_baslat kurdu), sürdürmede önceki fiber_gec sonrası dönüş adresi.
 *
 * NOT (naked kısıtı): gövde SADECE asm; C ifadesi yok. eski=x0, yeni=x1 doğrudan
 * kullanılır (compiler register tahsisi yok). */
__attribute__((naked, section(".user"), noinline))
static void fiber_gec(FiberBaglam *eski, FiberBaglam *yeni) {
    /* naked: parametreler (eski=x0, yeni=x1) doğrudan asm'de kullanılır; gövde
     * yalnız asm olabilir (C ifadesi yasak). Kullanılmayan-param uyarısı naked'de
     * çıkmaz (derleyici prolog üretmez → param "kullanılmış" sayılır). */
    __asm__ volatile(
        "stp     x19, x20, [x0, #0]   \n"
        "stp     x21, x22, [x0, #16]  \n"
        "stp     x23, x24, [x0, #32]  \n"
        "stp     x25, x26, [x0, #48]  \n"
        "stp     x27, x28, [x0, #64]  \n"
        "stp     x29, x30, [x0, #80]  \n"
        "mov     x2, sp               \n"
        "str     x2, [x0, #96]        \n"
        "ldp     x19, x20, [x1, #0]   \n"
        "ldp     x21, x22, [x1, #16]  \n"
        "ldp     x23, x24, [x1, #32]  \n"
        "ldp     x25, x26, [x1, #48]  \n"
        "ldp     x27, x28, [x1, #64]  \n"
        "ldp     x29, x30, [x1, #80]  \n"
        "ldr     x2, [x1, #96]        \n"
        "mov     sp, x2               \n"
        "ret                          \n");
}

/* === EL0 fiber durumu (.user_data — EL0-erişimli sayfa) === */
/* İki fiber bağlamı + launcher bağlamı (fiber 0 = launcher). */
__attribute__((section(".user_data"))) static FiberBaglam baglam_launcher;
__attribute__((section(".user_data"))) static FiberBaglam baglam_a;
__attribute__((section(".user_data"))) static FiberBaglam baglam_b;

/* Her fiber KENDİ EL0 yığını (0x42xxxxxx — .user_data, EL0 AP=01). 16-hizalı. */
__attribute__((section(".user_data"), aligned(16))) static unsigned char yigin_a[4096];
__attribute__((section(".user_data"), aligned(16))) static unsigned char yigin_b[4096];

/* Fiber sayaçları — ilerleme kanıtı. */
__attribute__((section(".user_data"))) static volatile int sayac_a;
__attribute__((section(".user_data"))) static volatile int sayac_b;

/* Ping-pong tur sayısı (bounded — sonsuz döngü değil). */
#define FIBER_TUR 3

/* === Fiber A (EL0) ===
 * Sayaç artır + "A<n>" bas + fiber_yield → B'ye geç. FIBER_TUR kez. Son turda
 * launcher'a döner (kendi bağlamını launcher'a geçirerek). */
__attribute__((section(".user"), noinline))
static void fiber_a_giris(void) {
    for (int t = 0; t < FIBER_TUR; t++) {
        sayac_a++;
        sys(5, (unsigned long)(uintptr_t)"A");
        sys(6, (unsigned long)(unsigned int)sayac_a);
        sys(7, 0);
        /* yield → B'ye (son turda da B'ye geç → B son turunu bitirip launcher'a döner) */
        fiber_gec(&baglam_a, &baglam_b);
    }
    /* A tüm turlarını bitirdi → launcher'a dön (ulaşılırsa). */
    fiber_gec(&baglam_a, &baglam_launcher);
    for (;;) { }   /* ulaşılmaz */
}

/* === Fiber B (EL0) ===
 * Sayaç artır + "B<n>" bas + fiber_yield → A'ya geç. FIBER_TUR kez. Son turda
 * launcher'a döner → ping-pong sonlanır. */
__attribute__((section(".user"), noinline))
static void fiber_b_giris(void) {
    for (int t = 0; t < FIBER_TUR; t++) {
        sayac_b++;
        sys(5, (unsigned long)(uintptr_t)"B");
        sys(6, (unsigned long)(unsigned int)sayac_b);
        sys(7, 0);
        if (t < FIBER_TUR - 1) {
            /* son değilse A'ya geri ver → A sonraki turunu koşar. */
            fiber_gec(&baglam_b, &baglam_a);
        } else {
            /* son tur: ping-pong bitti → launcher'a dön (kontrolü geri ver). */
            fiber_gec(&baglam_b, &baglam_launcher);
        }
    }
    for (;;) { }   /* ulaşılmaz */
}

/* Bir fiber bağlamını elle kur: ilk fiber_gec o girişe ret ile atlar.
 * x30(lr) = giriş, sp = yığın tepesi (16-hizalı, aşağı büyür). */
__attribute__((always_inline)) static inline void fiber_baslat(FiberBaglam *b, void (*giris)(void),
                                                               void *yigin_tepe) {
    for (int k = 0; k < 13; k++) b->kayit[k] = 0;
    b->kayit[11] = (uint64_t)(uintptr_t)giris;                        /* x30 = giriş */
    b->kayit[12] = ((uint64_t)(uintptr_t)yigin_tepe) & ~0xFUL;        /* sp, 16-hizalı */
}

/* === EL0 launcher (main-eşdeğeri, .user) ===
 * Fiber A + B'yi kur, A'ya geç, ping-pong'u başlat. Ping-pong bitince (B son turda
 * launcher'a döner) sayaçları doğrula → "USERFIBER OK" veya hata. */
__attribute__((section(".user"), noinline))
static void launcher(void) {
    sys(5, (unsigned long)(uintptr_t)"USERFIBER BASLA");
    sys(7, 0);

    sayac_a = 0;
    sayac_b = 0;

    /* İki fiber'ı elle kur: giriş + kendi yığın tepesi (aşağı büyür → dizi sonu). */
    fiber_baslat(&baglam_a, fiber_a_giris, yigin_a + sizeof(yigin_a));
    fiber_baslat(&baglam_b, fiber_b_giris, yigin_b + sizeof(yigin_b));

    /* İlk geçiş: launcher (fiber 0) → A. baglam_launcher'a launcher durumu kaydedilir;
     * B son turda buraya (launcher'a) döner → aşağıdaki satır o dönüşte sürer. */
    fiber_gec(&baglam_launcher, &baglam_a);

    /* Ping-pong bitti (kontrol launcher'a döndü). Determinizm doğrula:
     * A ve B tam FIBER_TUR kez koştu → her ikisi > 0 ve sayaçlar eşit (interleave). */
    if (sayac_a == FIBER_TUR && sayac_b == FIBER_TUR) {
        sys(5, (unsigned long)(uintptr_t)"USERFIBER OK");
    } else {
        sys(5, (unsigned long)(uintptr_t)"USERFIBER HATA");
    }
    sys(7, 0);

    sys(3, 0);            /* cik → kernel'de dur */
    for (;;) { }          /* ulaşılmaz */
}

int main(void) {
    kdl_yazdir_metin("USERFIBER KERNEL BASLA");
    kdl_yazdir_satir();

    /* .user sayfası (0x42000000) kdl_mmu_kur'da AP=01 → EL0 kod + fiber yığınları
     * (.user_data) + launcher yığını burada. EL0'a düş + launcher'ı koştur. */
    kdl_el0_calistir(launcher, __user_stack_top);
    return 0;   /* ulaşılmaz (launcher cik ile kernel'de durur) */
}
