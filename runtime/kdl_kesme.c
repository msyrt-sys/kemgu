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
void kdl_yaz_metin(const char *);   /* newline'sız — userspace 'yaz' syscall için */
void kdl_yaz_tam(int32_t);          /* newline'sız sayı — userspace 'yaz_sayi' için */
uint64_t kdl_tik_al(void);          /* timer tik sayısı — userspace gettick için (D-128) */
int      kdl_aktif_gorev(void);     /* aktif preemptive görev id — userspace getpid için */
void     kdl_gorev_bitir(void);     /* o an koşan görevi bitir — userspace exit (D-130) */
int      kdl_gorev_durum(int pid);  /* görev pid bitti mi? — userspace join/wait (D-130) */
#if defined(__aarch64__)
int      kdl_surec_spawn(uint64_t entry);   /* dinamik süreç oluştur — userspace spawn (D-129) */
#endif

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

/* === D-131: minimal RAM dosya deposu ===
 * İsimli, tek-değerli dosyalar — çekirdek-aracılı depolama, SÜREÇLER ARASI paylaşılır
 * (bir süreç yazar, diğeri okur). virtio-blk GEREKTİRMEZ (RAM-backed). Faz E dosya
 * sisteminin ilk adımı. Freestanding (libc yok). */
#define KDL_DOSYA_MAX 8
#define KDL_AD_MAX    16
static struct { char ad[KDL_AD_MAX]; int64_t deger; int kullanildi; } kdl_dosyalar[KDL_DOSYA_MAX];

static int kdl_ad_esit(const char *a, const char *b) {
    for (int i = 0; i < KDL_AD_MAX; i++) {
        if (a[i] != b[i]) return 0;
        if (a[i] == 0) return 1;
    }
    return 1;
}

static int kdl_dosya_bul(const char *ad) {
    for (int i = 0; i < KDL_DOSYA_MAX; i++)
        if (kdl_dosyalar[i].kullanildi && kdl_ad_esit(kdl_dosyalar[i].ad, ad))
            return i;
    return -1;
}

static int kdl_dosya_ac(const char *ad) {   /* bul veya oluştur; -1 = depo dolu */
    int i = kdl_dosya_bul(ad);
    if (i >= 0) return i;
    for (i = 0; i < KDL_DOSYA_MAX; i++) {
        if (!kdl_dosyalar[i].kullanildi) {
            int j = 0;
            for (; j < KDL_AD_MAX - 1 && ad[j]; j++) kdl_dosyalar[i].ad[j] = ad[j];
            kdl_dosyalar[i].ad[j] = 0;
            kdl_dosyalar[i].deger = 0;
            kdl_dosyalar[i].kullanildi = 1;
            return i;
        }
    }
    return -1;
}

/* === Sistem çağrısı dispatch (C6) ===
 * Kullanıcı/kernel kodu SVC (aarch64) / int 0x80 (x86) ile çağırır. Boot asm
 * stub'ı bağlamı kaydeder, num + arg (+ D-131: arg2) ile buraya gelir, dönüşte
 * eret/iretq. NOT: arg2 (2. argüman) yalnız aarch64'te dolu (SVC path geçirir);
 * x86 nums 1/2/3 arg2 kullanmaz (zararsız). */
uint64_t kdl_syscall_isle(uint64_t num, uint64_t arg, uint64_t arg2) {
    if (num == 1) {
        kdl_yazdir_metin("SYSCALL OK num=1");
        kdl_yazdir_satir();
    } else if (num == 2) {
        /* D2: çağrının kaynak ayrıcalık-seviyesini bildir (privilege ayrımı kanıtı).
         * EL0'dan gelen syscall → kaynak-EL=0; EL1'den → 1. */
        kdl_yazdir_metin("EL0 SYSCALL kaynak-EL=");
#if defined(__aarch64__)
        uint64_t spsr;
        __asm__ volatile("mrs %0, spsr_el1" : "=r"(spsr));
        kdl_yazdir_onaltilik((spsr >> 2) & 0x3);   /* SPSR.M[3:2] = kaynak EL */
#else
        kdl_yazdir_onaltilik(3);                    /* x86: ring3 (D2-x86 ayrı) */
#endif
        kdl_yazdir_satir();
    } else if (num == 3) {
        /* D2 çıkış: EL0'a dönme, kernel'de dur (eret yok). */
        kdl_yazdir_metin("D2 OK");
        kdl_yazdir_satir();
        for (;;) {
#if defined(__aarch64__)
            __asm__ volatile("wfe");
#elif defined(__x86_64__)
            __asm__ volatile("hlt");
#endif
        }
    } else if (num == 4) {
        /* D-122: syscall ARGÜMAN geçişi kanıtı. arg (x0) çağırandan doğru ulaştı mı?
         * Vektör-stub x0-koruma onarımından (D-121) ÖNCE arg = vektör-tip (bozuk)
         * olurdu → "HATA". Onarımla gerçek arg=42 ulaşır → "OK". Userspace
         * syscall'larının (arg geçen) ön-koşulu. */
        kdl_yazdir_metin(arg == 42 ? "SYSCALL ARG OK" : "SYSCALL ARG HATA");
        kdl_yazdir_satir();
    } else if (num == 5) {
        /* D-124 userspace ABI 'yaz': arg = kullanıcı bellek string ptr. Kernel
         * kullanıcı adına yazar (newline yok → parçalı çıktı birleştirilebilir).
         * NOT: gerçek OS'te ptr doğrulanır (user adres-uzayında mı?); burada demo. */
        kdl_yaz_metin((const char *)(uintptr_t)arg);
    } else if (num == 6) {
        /* D-124 'yaz_sayi': arg = yazılacak tamsayı (newline yok). */
        kdl_yaz_tam((int32_t)arg);
    } else if (num == 7) {
        /* D-124 'satir': satır sonu. */
        kdl_yazdir_satir();
    } else if (num == 9) {
        /* D-126 syscall DÖNÜŞ değeri ABI kanıtı: kernel bir sonuç hesaplar +
         * EL0 çağırana x0'da döndürür (kdl_svc_ortak str x0 → saved-x0 → restore).
         * 'artir': arg+1 döner. read/getpid/gettick ailesinin mekanizma temeli. */
        return arg + 1;
    } else if (num == 10) {
        /* D-128 gettick: mevcut timer tik sayısı → userspace zamanı okuyabilir. */
        return kdl_tik_al();
    } else if (num == 11) {
        /* D-128 getpid: o an koşan sürecin (preemptive görev) id'si. */
        return (uint64_t)(int64_t)kdl_aktif_gorev();
#if defined(__aarch64__)
    } else if (num == 12) {
        /* D-129 spawn: arg = EL0 giriş adresi → runtime'da yeni izole süreç oluştur.
         * Yeni sürecin id'sini (pid) döner (-1 havuz dolu). Dinamik process yaratma. */
        return (uint64_t)(int64_t)kdl_surec_spawn(arg);
#endif
    } else if (num == 13) {
        /* D-130 exit: o an koşan süreci bitir (scheduler bir daha seçmez). */
        kdl_gorev_bitir();
    } else if (num == 14) {
        /* D-130 durum: süreç `arg` (pid) bitti mi? → join/wait (ebeveyn yoklar). */
        return (uint64_t)(int64_t)kdl_gorev_durum((int)arg);
    } else if (num == 15) {
        /* D-131 dosya_yaz(ad=arg, deger=arg2): isimli dosyaya değer yaz (oluştur).
         * 2-argümanlı syscall (D-126 x1-koruma + D-131 SVC arg2 geçişi). 0=ok, -1=dolu. */
        int i = kdl_dosya_ac((const char *)(uintptr_t)arg);
        if (i < 0) return (uint64_t)(int64_t)-1;
        kdl_dosyalar[i].deger = (int64_t)arg2;
        return 0;
    } else if (num == 16) {
        /* D-131 dosya_oku(ad=arg): isimli dosyanın değerini döner (yoksa -1). */
        int i = kdl_dosya_bul((const char *)(uintptr_t)arg);
        if (i < 0) return (uint64_t)(int64_t)-1;
        return (uint64_t)kdl_dosyalar[i].deger;
    }
    return 0;   /* dönüş değeri olmayan syscall'lar için 0 */
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
