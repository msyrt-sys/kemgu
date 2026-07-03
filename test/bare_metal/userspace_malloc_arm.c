/*
 * MİLESTONE B testi (aarch64) — USERSPACE EL0 HEAP ALLOCATOR (malloc/free).
 *
 * Şimdiye kadar dinamik bellek (bölge/dizi) hep KERNEL (EL1) tarafında (kdl_bare_heap,
 * frame allocator) ayrıldı. Bu test, bir EL0 (yetkisiz) sürecin KENDİ dinamik bellek
 * ayırıcısını çalıştırdığını kanıtlar — ÇEKİRDEK YARDIMI YOK. Süreç yalnız kendi
 * EL0-erişimli veri sayfasındaki (0x42000000 bölgesi, AP=01) statik bir havuzu
 * yönetir. Kernel sadece I/O syscall'ları (num=5/6/7 yaz, num=3 cik) verir; bellek
 * ayırma tamamen kullanıcı-uzayında (u_malloc/u_free) döner.
 *
 * Allocator (bump + serbest-liste, first-fit):
 *   u_malloc(boyut) — 16-hizalı. Önce serbest-listeden yeterince büyük ilk blok
 *                     (first-fit); yoksa havuz-sonundan bump. Blok başı gizli
 *                     başlıkta (boyut) tutulur → u_free başlığı okur.
 *   u_free(ptr)     — bloğu serbest-listenin başına ekler (LIFO push).
 *
 * Senaryo (serbest-liste ÇALIŞTI kanıtı):
 *   A = u_malloc(64), B = u_malloc(64), C = u_malloc(64)   → üç ardışık bump
 *   u_free(B)                                              → B serbest-listeye
 *   D = u_malloc(64)                                       → D, B'nin YERİNİ alır
 *   (D == B) → serbest-liste yeniden-kullanımı doğrulandı (bump değil reuse).
 *   Round-trip: A/C/D'ye yaz → oku → değerler korunuyor (bellek gerçekten çalışır).
 *   Tüm pointer'lar EL0 user-VA aralığında [0x42000000, 0x42400000) (güvenli).
 *   Süreç kernel adresine YAZMAZ (yalnız kendi .user_data havuzu).
 *
 * Kanıt: "USERMALLOC OK" (reuse doğrulandı + round-trip). "USERMALLOC BASLA".
 * DETERMİNİSTİK (havuz statik, tohumsuz → 2× byte-identik). marker "USERMALLOC OK".
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

/* === EL0 heap havuzu (.user_data — EL0-yazılabilir AP=01 sayfası) ===
 * Allocator'ın backing belleği. Statik → adresi 0x42000000 bölgesinde (EL0 erişebilir).
 * Kernel bu havuza DOKUNMAZ; ayırma tamamen EL0'da. 16-hizalı (blok hizası tutarlı). */
#define U_HAVUZ_BOYUT 4096
__attribute__((section(".user_data"), aligned(16)))
static unsigned char u_havuz[U_HAVUZ_BOYUT];

/* Havuzda ne kadar bump edildi (byte offset). Serbest-liste başı (blok başlık ptr'ı).
 * Hepsi .user_data → EL0-yazılabilir, süreç durumu. */
__attribute__((section(".user_data"))) static uint64_t u_bump;          /* sonraki bump offset */
__attribute__((section(".user_data"))) static void    *u_serbest_bas;   /* serbest-liste başı */

/* === Blok başlığı (gizli, kullanıcı pointer'ından hemen önce) ===
 * Her ayrılan/serbest blok, kullanıcı verisinden ÖNCE 16-byte başlık taşır:
 *   [0] boyut  — kullanıcı-verisi byte sayısı (16 katı)
 *   [1] sonraki — serbest-listede iken bir sonraki serbest blok başlığı (yoksa 0)
 * 16-byte başlık → kullanıcı pointer'ı da 16-hizalı kalır. */
typedef struct UBlok {
    uint64_t      boyut;      /* kullanıcı-veri boyutu (16-hizalı) */
    struct UBlok *sonraki;    /* serbest-liste zinciri (yalnız serbest iken anlamlı) */
} UBlok;

/* Boyutu 16'nın üstüne yuvarla (hizalama). */
__attribute__((always_inline)) static inline uint64_t u_hizala16(uint64_t n) {
    return (n + 15UL) & ~15UL;
}

/* === u_malloc — EL0 kullanıcı-uzayı ayırıcı (kernel yardımı YOK) ===
 * (1) Serbest-listede yeterince büyük İLK bloğu ara (first-fit) → varsa çıkar + döndür.
 * (2) Yoksa havuz-sonundan bump: [başlık(16) + hizalı-boyut].
 * Başarısızlık (havuz dolu) → 0 (NULL). Dönüş = başlıktan SONRAKİ kullanıcı pointer'ı. */
__attribute__((section(".user"), noinline))
static void *u_malloc(uint64_t boyut) {
    if (boyut == 0) return 0;
    uint64_t istem = u_hizala16(boyut);

    /* (1) first-fit: serbest-liste taraması. */
    UBlok *onceki = 0;
    UBlok *b = (UBlok *)u_serbest_bas;
    while (b != 0) {
        if (b->boyut >= istem) {
            /* Bu bloğu serbest-listeden çıkar. */
            if (onceki == 0) {
                u_serbest_bas = b->sonraki;
            } else {
                onceki->sonraki = b->sonraki;
            }
            b->sonraki = 0;
            return (void *)((unsigned char *)b + sizeof(UBlok));
        }
        onceki = b;
        b = b->sonraki;
    }

    /* (2) bump: başlık + kullanıcı verisi havuza sığıyor mu? */
    uint64_t toplam = sizeof(UBlok) + istem;
    if (u_bump + toplam > U_HAVUZ_BOYUT) return 0;   /* havuz dolu */
    UBlok *yeni = (UBlok *)(u_havuz + u_bump);
    yeni->boyut = istem;
    yeni->sonraki = 0;
    u_bump += toplam;
    return (void *)((unsigned char *)yeni + sizeof(UBlok));
}

/* === u_free — bloğu serbest-listeye geri koy (LIFO push) ===
 * Kullanıcı pointer'ından başlığı geri hesapla, serbest-listenin başına ekle.
 * Sonraki eşit/küçük u_malloc bu bloğu YENİDEN kullanır (bump yerine). */
__attribute__((section(".user"), noinline))
static void u_free(void *ptr) {
    if (ptr == 0) return;
    UBlok *b = (UBlok *)((unsigned char *)ptr - sizeof(UBlok));
    b->sonraki = (UBlok *)u_serbest_bas;
    u_serbest_bas = b;
}

/* Pointer EL0 user-VA aralığında [0x42000000, 0x42400000) mı? (güvenlik/aralık kanıtı) */
__attribute__((always_inline)) static inline int u_user_va_mi(void *p) {
    uint64_t a = (uint64_t)(uintptr_t)p;
    return (a >= 0x42000000UL && a < 0x42400000UL);
}

/* === EL0 program (.user) — kendi allocator'ını sürer ===
 * A/B/C ayır, B serbest, D ayır → D == B (reuse). Round-trip yaz/oku. */
__attribute__((section(".user"), noinline))
static void el0_program(void) {
    sys(5, (unsigned long)(uintptr_t)"USERMALLOC BASLA");
    sys(7, 0);

    /* Allocator durumunu sıfırla (deterministik — havuz baştan). */
    u_bump = 0;
    u_serbest_bas = 0;

    /* Üç ardışık ayırma (bump). */
    unsigned char *A = (unsigned char *)u_malloc(64);
    unsigned char *B = (unsigned char *)u_malloc(64);
    unsigned char *C = (unsigned char *)u_malloc(64);

    /* Ayırma başarısı + hepsi user-VA aralığında + ayrık (üst üste binmiyor). */
    int ayirma_ok = (A != 0) && (B != 0) && (C != 0)
                  && u_user_va_mi(A) && u_user_va_mi(B) && u_user_va_mi(C)
                  && (A != B) && (B != C) && (A != C);

    /* A ve C'ye yaz (round-trip için). B serbest bırakılacak → yazma. */
    for (int i = 0; i < 64; i++) { A[i] = (unsigned char)(0xA0 + (i & 0x0F)); }
    for (int i = 0; i < 64; i++) { C[i] = (unsigned char)(0xC0 + (i & 0x0F)); }

    /* B'yi serbest bırak → serbest-listeye gider. */
    u_free(B);

    /* Yeni ayırma: aynı boyut → serbest-listeden B'nin YERİ gelmeli (reuse). */
    unsigned char *D = (unsigned char *)u_malloc(64);
    int reuse_ok = (D != 0) && (D == B) && u_user_va_mi(D);

    /* D'ye yaz (B'nin eski yerine). */
    for (int i = 0; i < 64; i++) { D[i] = (unsigned char)(0xD0 + (i & 0x0F)); }

    /* Round-trip doğrulama: A, C, D beklenen değerleri koruyor mu?
     * (D, B'nin yerini aldı ama artık D'nin verisi — reuse'da veri karışmıyor.) */
    int roundtrip_ok = 1;
    for (int i = 0; i < 64; i++) {
        if (A[i] != (unsigned char)(0xA0 + (i & 0x0F))) roundtrip_ok = 0;
        if (C[i] != (unsigned char)(0xC0 + (i & 0x0F))) roundtrip_ok = 0;
        if (D[i] != (unsigned char)(0xD0 + (i & 0x0F))) roundtrip_ok = 0;
    }

    /* A ve C, reuse'dan ETKİLENMEDİ (D yalnız B'nin yerini aldı, A/C dokunulmadı). */

    if (ayirma_ok && reuse_ok && roundtrip_ok) {
        sys(5, (unsigned long)(uintptr_t)"USERMALLOC OK");
    } else if (ayirma_ok && roundtrip_ok) {
        sys(5, (unsigned long)(uintptr_t)"USERMALLOC REUSE-YOK");   /* free-list reuse başarısız */
    } else {
        sys(5, (unsigned long)(uintptr_t)"USERMALLOC HATA");
    }
    sys(7, 0);

    sys(3, 0);            /* cik → kernel'de dur */
    for (;;) { }          /* ulaşılmaz */
}

int main(void) {
    kdl_yazdir_metin("USERMALLOC KERNEL BASLA");
    kdl_yazdir_satir();

    /* .user sayfası (0x42000000) kdl_mmu_kur'da AP=01 → EL0 kod (.user) + heap havuzu
     * (.user_data) + EL0 yığını burada. EL0'a düş + programı koştur. Allocator tamamen
     * EL0'da döner (kernel yalnız I/O syscall'ları). */
    kdl_el0_calistir(el0_program, __user_stack_top);
    return 0;   /* ulaşılmaz (program cik ile kernel'de durur) */
}
