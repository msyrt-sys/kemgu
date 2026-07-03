/*
 * MİLESTONE-B testi (aarch64) — USERSPACE ÇOK-FİBER KOOPERATİF SCHEDULER
 * (N yeşil-thread, round-robin, TAMAMEN EL0'da).
 *
 * D-203 (userspace_fiber_arm.c) İKİ fiber arası doğrudan ping-pong yaptı
 * (fiber A → fiber B → fiber A ...). Bu test onu GERÇEK bir SCHEDULER'a taşır:
 * N>=3 fiber + merkezi bir round-robin scheduler döngüsü. Fiber'lar BİRBİRİNE
 * doğrudan geçmez; her fiber u_yield() ile SCHEDULER'A döner, scheduler bir
 * sonraki READY fiber'ı round-robin seçip ona context-switch yapar. Bu, M:1
 * kooperatif runtime'ın (KEMGU `görev` ileride) tam userspace çekirdeğidir:
 * TEK çekirdek-thread üzerinde N mantıksal akış, KERNEL YARDIMI OLMADAN
 * (timer/IRQ/preemption yok, syscall-tabanlı switch yok — saf EL0 asm switch).
 *
 * Mimari (D-203 primitifi üstünde):
 *   - fiber_gec(eski, yeni): EL0 kooperatif bağlam-değiştirme (naked .user,
 *     callee-saved x19-x30 + sp kaydet/yükle, ret). D-203 ile AYNI — kritik
 *     ders: naked, always_inline OLAMAZ, son 'ret' yüklenen x30'a dallanır.
 *   - Scheduler'ın KENDİ bağlamı (baglam_sched) = "fiber-0-benzeri": u_yield
 *     çalışan fiber'dan scheduler'a döner; scheduler seç + fiber'a geri döner.
 *   - u_yield(): çalışan fiber'ın bağlamını scheduler bağlamına geçirir. Kontrol
 *     scheduler döngüsüne döner; scheduler sonraki READY fiber'ı seçince buraya
 *     geri dönülür (fiber kaldığı yerden sürer).
 *
 * Fiber bağlamı = callee-saved (x19-x28) + x29(fp) + x30(lr) + sp
 * (kdl_baglam_degis TCB düzeniyle aynı: [0..9]=x19-x28, [10]=x29, [11]=x30,
 * [12]=sp). Her fiber KENDİ EL0 yığınına sahip (.user_data → 0x42xxxxxx,
 * EL0-erişimli AP=01). İlk geçişte fiber bağlamı elle kurulur (sp = yığın tepesi
 * 16-hizalı, x30 = fiber girişi) → scheduler'ın ilk fiber_gec'i o girişe ret.
 *
 * Round-robin senaryo (DETERMİNİSTİK): 3 fiber (A/B/C), her biri 3 tur. Scheduler
 * sırayla A → B → C → A → B → C → ... çalıştırır; her fiber bir tur işini yapıp
 * (sayaç artır + "X<n>" bas) u_yield ile scheduler'a döner. Bir fiber tüm turlarını
 * bitince BİTMİŞ (done) işaretlenir; scheduler onu artık seçmez. Tüm fiber'lar
 * bitince scheduler döngüsü sonlanır.
 *   Beklenen interleave: A1 B1 C1 A2 B2 C2 A3 B3 C3 (round-robin turlar).
 *
 * Kanıt: 3 fiber round-robin interleave BEKLENEN sırada + hepsi FIBER_TUR kez
 * koştu + hepsi done → "USERSCHED OK". main-eşdeğeri "USERSCHED BASLA".
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
 * RET → yeni fiber kaldığı yerden (veya ilk kez girişinden) sürer.
 *
 * naked + .user + noinline: GERÇEK fonksiyon (bl ile çağrılır). naked → derleyici
 * prolog/epilog üretmez; son 'ret' YÜKLENEN x30'a dallanır (D-203 kritik ders:
 * always_inline OLAMAZ — ayrı fonksiyon + gerçek ret gerekir). Parametreler AAPCS
 * ile x0=eski, x1=yeni. .user → EL0-exec (AP=01, UXN=0); ayrı .text (AP=00) olsaydı
 * EL0 fetch-fault. Gövde SADECE asm; C ifadesi yok (naked kısıtı). */
__attribute__((naked, section(".user"), noinline))
static void fiber_gec(FiberBaglam *eski, FiberBaglam *yeni) {
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

/* === Scheduler durumu (.user_data — EL0-erişimli sayfa) === */
#define FIBER_SAYISI 3          /* N>=3 yeşil-thread */
#define FIBER_TUR    3          /* her fiber tur sayısı (bounded — sonsuz değil) */

/* Fiber durumları (round-robin seçim için). */
#define DURUM_HAZIR  0          /* READY — koşmaya hazır */
#define DURUM_BITTI  1          /* DONE  — tüm turlarını bitirdi */

/* Scheduler kendi bağlamı ("fiber-0-benzeri"): fiber'lar buraya u_yield eder. */
__attribute__((section(".user_data"))) static FiberBaglam baglam_sched;

/* N fiber bağlamı + N durum + N sayaç (ilerleme kanıtı). */
__attribute__((section(".user_data"))) static FiberBaglam baglam[FIBER_SAYISI];
__attribute__((section(".user_data"))) static volatile int durum[FIBER_SAYISI];
__attribute__((section(".user_data"))) static volatile int sayac[FIBER_SAYISI];

/* Şu an koşan fiber'ın indeksi (u_yield hangi bağlama kaydedeceğini bilir). */
__attribute__((section(".user_data"))) static volatile int aktif_fiber;

/* Her fiber KENDİ EL0 yığını (0x42xxxxxx — .user_data, EL0 AP=01). 16-hizalı. */
__attribute__((section(".user_data"), aligned(16))) static unsigned char yigin[FIBER_SAYISI][4096];

/* Fiber adları (tek harf — "A"/"B"/"C"). .user_data → EL0 okuyabilir; ayrıca
 * syscall(5) kernel tarafından okunur (EL1 de erişebilir). const DEĞİL: .user_data
 * yazılabilir bir data section; const olsaydı read-only section tipi çakışması olur
 * (diğer .user_data değişkenleri yazılabilir). Değerler runtime'da initialize edilir. */
__attribute__((section(".user_data"))) static char fiber_ad[FIBER_SAYISI][2];

/* === u_yield() — çalışan fiber scheduler'a döner ===
 * Aktif fiber'ın bağlamını KAYDET + scheduler bağlamına GEÇ. Scheduler bu fiber'ı
 * tekrar seçince buraya (u_yield sonrası) geri dönülür → fiber kaldığı yerden sürer. */
__attribute__((section(".user"), noinline))
static void u_yield(void) {
    int i = aktif_fiber;
    fiber_gec(&baglam[i], &baglam_sched);
}

/* === Fiber gövdesi (EL0) — round-robin iş birimi ===
 * FIBER_TUR kez: sayaç artır + "X<n>" bas + u_yield (scheduler'a dön). Tüm turlar
 * bitince kendini DURUM_BITTI işaretle + son bir kez scheduler'a dön (bir daha
 * seçilmez). Her fiber'ın hangisi olduğu: kendi indeksini x19'a bağlamak yerine
 * aktif_fiber'dan okuruz (scheduler her switch'te aktif_fiber'ı set eder). */
__attribute__((section(".user"), noinline))
static void fiber_giris(void) {
    /* Kendi indeksimizi scheduler'ın set ettiği aktif_fiber'dan al (ilk giriş). */
    int ben = aktif_fiber;
    for (int t = 0; t < FIBER_TUR; t++) {
        sayac[ben]++;
        sys(5, (unsigned long)(uintptr_t)fiber_ad[ben]);   /* "A"/"B"/"C" */
        sys(6, (unsigned long)(unsigned int)sayac[ben]);   /* tur numarası */
        sys(7, 0);                                         /* satır sonu */
        u_yield();                                         /* scheduler'a dön */
        /* Geri dönünce: scheduler bizi tekrar seçti; ben aynı (fiber-lokal). */
    }
    /* Tüm turlar bitti → done işaretle + son kez scheduler'a dön. */
    durum[ben] = DURUM_BITTI;
    fiber_gec(&baglam[ben], &baglam_sched);   /* bir daha buraya dönülmez */
    for (;;) { }                              /* ulaşılmaz */
}

/* Bir fiber bağlamını elle kur: ilk fiber_gec o girişe ret ile atlar.
 * x30(lr) = giriş, sp = yığın tepesi (16-hizalı, aşağı büyür). */
__attribute__((always_inline)) static inline void fiber_baslat(FiberBaglam *b, void (*giris)(void),
                                                               void *yigin_tepe) {
    for (int k = 0; k < 13; k++) b->kayit[k] = 0;
    b->kayit[11] = (uint64_t)(uintptr_t)giris;                 /* x30 = giriş */
    b->kayit[12] = ((uint64_t)(uintptr_t)yigin_tepe) & ~0xFUL; /* sp, 16-hizalı */
}

/* === EL0 scheduler (main-eşdeğeri, .user) ===
 * N fiber'ı kur, round-robin döngüsü çalıştır: sırayla READY fiber seç →
 * context-switch → fiber bir tur koşup u_yield ile döner → sonraki READY. Tüm
 * fiber'lar DONE olunca döngü biter. Sonra determinizm doğrula → "USERSCHED OK". */
__attribute__((section(".user"), noinline))
static void scheduler(void) {
    sys(5, (unsigned long)(uintptr_t)"USERSCHED BASLA");
    sys(7, 0);

    /* Fiber adlarını runtime'da kur ("A"/"B"/"C" — .user_data yazılabilir). */
    for (int i = 0; i < FIBER_SAYISI; i++) {
        fiber_ad[i][0] = (char)('A' + i);
        fiber_ad[i][1] = 0;
    }

    /* Fiber'ları kur: her biri kendi girişi + kendi EL0 yığın tepesi (aşağı büyür). */
    for (int i = 0; i < FIBER_SAYISI; i++) {
        durum[i] = DURUM_HAZIR;
        sayac[i] = 0;
        fiber_baslat(&baglam[i], fiber_giris, yigin[i] + sizeof(yigin[i]));
    }

    /* Round-robin döngüsü: bir tam turda hiç READY fiber çalışmazsa → hepsi bitti. */
    int sira = 0;                       /* sonraki denenecek fiber (round-robin) */
    for (;;) {
        int kosan = 0;                  /* bu turda çalışan fiber oldu mu? */
        for (int n = 0; n < FIBER_SAYISI; n++) {
            int i = (sira + n) % FIBER_SAYISI;
            if (durum[i] == DURUM_HAZIR) {
                aktif_fiber = i;                    /* fiber_giris/u_yield bunu okur */
                fiber_gec(&baglam_sched, &baglam[i]);  /* i'ye geç; u_yield ile döner */
                kosan = 1;
                /* Bir sonraki round-robin başlangıcı: bu fiber'dan sonraki. */
                sira = (i + 1) % FIBER_SAYISI;
                break;                              /* round-robin: her seferinde bir fiber */
            }
        }
        if (!kosan) {
            break;                      /* hiç READY yok → tüm fiber'lar DONE */
        }
    }

    /* Determinizm doğrula: her fiber tam FIBER_TUR kez koştu + hepsi DONE. */
    int ok = 1;
    for (int i = 0; i < FIBER_SAYISI; i++) {
        if (sayac[i] != FIBER_TUR || durum[i] != DURUM_BITTI) {
            ok = 0;
        }
    }
    if (ok) {
        sys(5, (unsigned long)(uintptr_t)"USERSCHED OK");
    } else {
        sys(5, (unsigned long)(uintptr_t)"USERSCHED HATA");
    }
    sys(7, 0);

    sys(3, 0);            /* cik → kernel'de dur */
    for (;;) { }          /* ulaşılmaz */
}

int main(void) {
    kdl_yazdir_metin("USERSCHED KERNEL BASLA");
    kdl_yazdir_satir();

    /* .user sayfası (0x42000000) kdl_mmu_kur'da AP=01 → EL0 kod + fiber yığınları
     * (.user_data) + scheduler yığını burada. EL0'a düş + scheduler'ı koştur. */
    kdl_el0_calistir(scheduler, __user_stack_top);
    return 0;   /* ulaşılmaz (scheduler cik ile kernel'de durur) */
}
