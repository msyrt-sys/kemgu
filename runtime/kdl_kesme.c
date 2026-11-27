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

#if defined(__aarch64__)
/* === PART 1(b) / C8c: KONTROLLÜ page-fault kurtarma (demand-paging temeli) ===
 * kdl_fault_bekleniyor != 0 iken bir data-abort olursa, start_aarch64.S fault-yolu
 * (kdl_exc_ortak) HALT ETMEZ; bunun yerine: FAR'ı kdl_fault_yakalanan'a yazar, bayrağı
 * temizler, faulting instr'ı ATLAR (ELR+=4), frame restore + eret → OS DEVAM eder.
 * Bu, "MMU gerçekten zorluyor (haritasız erişim fault eder) + kernel fault'u yönetip
 * ilerliyor" kanıtını TEK boot'ta verir (izole demo değil). Bayrak yoksa → eski
 * davranış (yazdır + halt). volatile: asm ile paylaşılan durum. */
/* FAZ-A1 (D-276): bu iki global .S kdl_exc_ortak ile eşleşik recovery-scratch.
 * kem_os SAF-.kem fault-gate'i (kem_mmu.kem) bunlara inline-asm (adrp/add + ldr/str)
 * ile erişir — .kem 'küresel' internal-linkage olduğundan .S'ye açılamaz (codegen-gap,
 * bkz. D-276 sınır-notu).
 * ZERO-C B3 (D-285): WEAK → boot/start_aarch64.S artık BU İKİ GLOBAL'İ .data'da STRONG
 * tanımlıyor (indirgenemez-.S substrat, Law-4). kem_os kesme.c'yi HİÇ LİNKLEMEZ (B1/B2
 * tüm fonksiyonları .kem'e taşıdı) → bu weak tanım YALNIZ diğer kernel'ler kesme.c'yi
 * .S ile birlikte linklediğinde çift-sembol çakışmasını önler (.S strong kazanır, aynı
 * ad/tip/başlangıç-değer → davranış AYNI). */
__attribute__((weak)) volatile uint64_t kdl_fault_bekleniyor = 0;
__attribute__((weak)) volatile uint64_t kdl_fault_yakalanan = 0;

/* === PART 3(c): EL0 izolasyon-ihlali → SÜRECİ ÖLDÜR (gerçek OS semantiği) ===
 * Bir EL0 (userspace) görev kernel-only sayfaya (AP=00) erişince permission-fault
 * → start_aarch64.S fault-yolu (SPSR kaynak-EL=0 ise) BUNU çağırır: ihlali raporlar +
 * o an koşan görevi ÖLÜ işaretler (scheduler bir daha seçmez) → OS DEVAM eder (kabuk +
 * diğer görevler koşar). Bu, "process izolasyonu: ihlal eden süreç öldürülür, çekirdek
 * hayatta kalır" kanıtı. kernel-mode fonksiyon her-şeye erişebilirdi → izolasyon-yok
 * demekti; EL0 erişemez = GERÇEK izolasyon. */
void kdl_gorev_bitir(void);   /* o an koşan preemptive görevi bitir (ölü işaretle) */
volatile uint64_t kdl_izolasyon_ihlal_sayisi = 0;
/* OPT-IN: yalnız kdl_el0_kill_aktif != 0 ise EL0 izolasyon-faultu süreç-öldürme yapar.
 * Varsayılan 0 → eski davranış (EL0 fault → kdl_istisna_isle "ISTISNA" + halt) KORUNUR
 * → mevcut EL0 demoları (proc_arm/D3 vb. "ISTISNA" bekleyen) REGRESSION YAŞAMAZ.
 * Entegre çekirdek (kemgu_os_arm.c) bunu 1 yapar → process-izolasyon kill semantiği.
 * ZERO-C B3 (D-285): WEAK (bkz. kdl_fault_bekleniyor notu — .S artık strong tanımlıyor). */
__attribute__((weak)) volatile uint64_t kdl_el0_kill_aktif = 0;
/* ZERO-C B2 (D-284): WEAK → kem_os SAF-.kem kdl_el0_izolasyon_isle (kem_mmu.kem,
 * strong) link'te override eder. Diğer kernel'ler C weak'i kullanmaya devam. */
__attribute__((weak))
void kdl_el0_izolasyon_isle(uint64_t far) {
    kdl_izolasyon_ihlal_sayisi++;
    kdl_yazdir_metin("IZOLASYON OK: EL0 kernel-erisim reddedildi (surec olduruldu) FAR=0x");
    kdl_yazdir_onaltilik(far);
    kdl_yazdir_satir();
    kdl_gorev_bitir();            /* ihlal eden EL0 görevi → ölü; scheduler atlar */
}
#endif

/* CPU istisnası — kurtarma yok (bayrak-kapalı yol). Parametreler mimari-spesifik:
 *   aarch64: (vektör_tipi, ESR_EL2, ELR_EL2)
 *   x86_64 : (vektör_no, hata_kodu, RIP) */
/* ZERO-C B1 (D-283): WEAK → kem_os SAF-.kem kdl_istisna_isle (kem_mmu.kem, strong)
 * link'te override eder (kdl_syscall_isle deseni birebir). Diğer kernel'ler strong
 * .kem sağlamaz → bu C tanımı kullanılır (davranış değişmez). */
__attribute__((weak, noreturn))
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
#define KDL_ICERIK_MAX 64   /* D-132: dosya metin içeriği (byte tampon) */
static struct {
    char ad[KDL_AD_MAX];
    int64_t deger;                    /* tek-değer (D-131) */
    char icerik[KDL_ICERIK_MAX];      /* metin içerik (D-132) */
    int boyut;                        /* içerik uzunluğu */
    int kullanildi;
} kdl_dosyalar[KDL_DOSYA_MAX];

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

/* === D-140: userspace mesaj kanalı (IPC) ===
 * Global çekirdek mesaj kuyruğu (int ring buffer) — userspace süreçler DOĞRUDAN
 * mesajla haberleşir (dosya-IPC'nin ötesinde; KEMGU `kanal` ilkeli userspace düzeyi).
 * Bloklamasız: gonder dolu ise -1, al boş ise -1 (test negatif-olmayan değer yollar). */
#define KDL_MSG_MAX 16
static int kdl_msg[KDL_MSG_MAX];
static int kdl_msg_bas = 0, kdl_msg_son = 0;

/* Kernel-içi dosya yardımcıları (persistence + test için). */
int kdl_dosya_olustur_deger(const char *ad, int64_t deger) {
    int i = kdl_dosya_ac(ad);
    if (i < 0) return -1;
    kdl_dosyalar[i].deger = deger;
    return i;
}
int64_t kdl_dosya_deger(const char *ad) {
    int i = kdl_dosya_bul(ad);
    if (i < 0) return (int64_t)-1;
    return kdl_dosyalar[i].deger;
}

#if defined(__aarch64__)
/* === D-143: KALICI dosya sistemi (disk-backed persistence) ===
 * kdl_dosyalar tablosunu virtio-blk diske serialize eder → dosyalar BOOT'LAR ARASI
 * yaşar. Blok 0-1: [magic "KEMG"(4)][pad(12)][kdl_dosyalar bytes]. */
extern uint64_t kdl_virtio_blk_bul(void);
extern int kdl_virtio_blk_kur(uint64_t base);
extern int kdl_virtio_blk_oku(uint64_t base, uint64_t sektor, uint8_t *hedef);
extern int kdl_virtio_blk_yaz(uint64_t base, uint64_t sektor, const uint8_t *kaynak);

static uint8_t kdl_fs_buf[1024] __attribute__((aligned(16)));

int kdl_dosya_kaydet(uint64_t base) {
    for (int i = 0; i < 1024; i++) kdl_fs_buf[i] = 0;
    kdl_fs_buf[0] = 'K'; kdl_fs_buf[1] = 'E'; kdl_fs_buf[2] = 'M'; kdl_fs_buf[3] = 'G';   /* magic */
    const unsigned char *src = (const unsigned char *)(const void *)kdl_dosyalar;
    unsigned long n = sizeof(kdl_dosyalar);                 /* <= 784-16 sığar (2 blok) */
    for (unsigned long i = 0; i < n && (16 + i) < 1024; i++) kdl_fs_buf[16 + i] = src[i];
    if (kdl_virtio_blk_yaz(base, 0, kdl_fs_buf) != 0) return -1;
    if (kdl_virtio_blk_yaz(base, 1, kdl_fs_buf + 512) != 0) return -1;
    return 0;
}

int kdl_dosya_yukle(uint64_t base) {
    if (kdl_virtio_blk_oku(base, 0, kdl_fs_buf) != 0) return -1;
    if (kdl_virtio_blk_oku(base, 1, kdl_fs_buf + 512) != 0) return -1;
    if (!(kdl_fs_buf[0] == 'K' && kdl_fs_buf[1] == 'E' &&
          kdl_fs_buf[2] == 'M' && kdl_fs_buf[3] == 'G')) return -1;   /* diskte FS yok */
    unsigned char *dst = (unsigned char *)(void *)kdl_dosyalar;
    unsigned long n = sizeof(kdl_dosyalar);
    for (unsigned long i = 0; i < n && (16 + i) < 1024; i++) dst[i] = kdl_fs_buf[16 + i];
    /* D-153: poisoned-disk boyut clamp (savunma katmanı). Diskteki tablo verbatim
     * yüklendi; kötü niyetli bir disk taşkın alanlar içerebilir. Deserialize sonrası
     * her girişi güvenli sınırlara normalize et — böylece num=18 gibi okuyucular
     * asla icerik[] tamponunu aşan boyut GÜVENMEZ (OOB okuma / kernel-sızıntı engeli). */
    for (int i = 0; i < KDL_DOSYA_MAX; i++) {
        kdl_dosyalar[i].kullanildi = kdl_dosyalar[i].kullanildi ? 1 : 0;
        kdl_dosyalar[i].ad[KDL_AD_MAX - 1] = 0;
        kdl_dosyalar[i].icerik[KDL_ICERIK_MAX - 1] = 0;
        if (kdl_dosyalar[i].boyut < 0 || kdl_dosyalar[i].boyut >= KDL_ICERIK_MAX)
            kdl_dosyalar[i].boyut = 0;
    }
    return 0;
}
#endif /* __aarch64__ */

/* === D-150 GÜVENLİK: kullanıcı-pointer doğrulama ===
 * EL0 syscall'ları kernel'e pointer geçirir; kernel (EL1) o adrese YAZARSA ve adres
 * doğrulanmazsa, kötü/hatalı bir EL0 süreç KERNEL BELLEĞİNE yazdırabilir (bellek
 * bozulması / privilege escalation). Bu yüzden kernel'in YAZDIĞI user-tampon'lar
 * EL0 user VA aralığında [0x42000000, 0x42400000) olmalı (kod+veri sayfaları, AP=01).
 * Kernel-okuduğu string'ler (.rodata çıktı) bu kontrolden MUAF (yalnız yazma). */
static int kdl_user_yaz_ptr_gecerli(uint64_t p, uint64_t len) {
    if (p < 0x42000000UL) return 0;              /* kernel/aşağı bölge → RED */
    if (len > 0x400000UL) return 0;              /* aşırı boy */
    if (p + len < p) return 0;                   /* taşma */
    if (p + len > 0x42400000UL) return 0;        /* user sayfaları dışı → RED */
    return 1;
}

/* === D-151 GÜVENLİK: kullanıcı-OKUMA-pointer doğrulama ===
 * D-150 kernel'in YAZDIĞI pointer'ları doğruladı. Bu, kernel'in OKUDUĞU (deref
 * ettiği) EL0-kontrollü null-sonlu string pointer'larını doğrular. Doğrulanmazsa:
 *   - unmapped adres → EL1 data-abort → kdl_istisna_isle sonsuz halt (DoS: tek SVC)
 *   - kernel adresi  → kernel belleği UART'a/dosyaya sızar (info-leak)
 * İzin verilen okuma bölgeleri: [user VA 0x42000000,0x42400000) ∪ kernel .rodata
 * (çıktı/ad string literalleri — testler .rodata pointer geçirir). .data/.bss (dosya
 * tablosu burada!) / stack / heap / Device MMIO / unmapped → RED (info-leak kesilir).
 * Null-sonlandırıcı İZİNLİ bölge içinde bulunmalı → straddle-over-read imkânsız.
 * Yalnız mapped-izinli bölge byte'ları taranır → tarama fault üretemez. */
extern char __rodata_start[];
extern char __rodata_end[];
static int kdl_user_oku_str_gecerli(uint64_t p) {
    uint64_t son;
    if (p >= 0x42000000UL && p < 0x42400000UL) {
        son = 0x42400000UL;                      /* EL0 user VA sayfaları (mapped) */
    } else {
        uint64_t rs = (uint64_t)(uintptr_t)__rodata_start;
        uint64_t re = (uint64_t)(uintptr_t)__rodata_end;
        if (p >= rs && p < re) son = re;         /* kernel .rodata (mapped, const, sır değil) */
        else return 0;                           /* izinsiz bölge → RED */
    }
    uint64_t tavan = p + 4096UL;                 /* tarama tavanı (scan maliyeti sınırla) */
    if (tavan > p && tavan < son) son = tavan;   /* taşma-korumalı min */
    for (uint64_t a = p; a < son; a++)
        if (*(const char *)(uintptr_t)a == 0) return 1;   /* bölge-içi null → string güvenli */
    return 0;                                    /* null yok → straddle riski → RED */
}

#if defined(__aarch64__)
/* === D-176: userspace ağ syscall'ları (net_gonder/net_al) ===
 * EL0 süreçleri ham ethernet çerçevesi gönderip alır (virtio-net kernel-aracılı) —
 * süreç modeli + ağ yığını birleşimi. GÜVENLİK (D-150/151 disiplini): net_gonder
 * user buffer'ı OKUR → frame user VA'da + len bounded olmalı; net_al user buffer'a
 * YAZAR → user VA'da olmalı. kdl_virtio_net_gonder frame'i kendi TX DMA buffer'ına
 * KOPYALAR → doğrulanmış user pointer'ı geçirmek güvenli. */
extern uint64_t kdl_virtio_net_bul(void);
extern int kdl_virtio_net_gonder(uint64_t base, const uint8_t *cerceve, int uzun);
extern int kdl_virtio_net_al(uint64_t base, uint8_t *hedef, int max, long tikler);
#endif

/* === Sistem çağrısı dispatch (C6) ===
 * Kullanıcı/kernel kodu SVC (aarch64) / int 0x80 (x86) ile çağırır. Boot asm
 * stub'ı bağlamı kaydeder, num + arg (+ D-131: arg2) ile buraya gelir, dönüşte
 * eret/iretq. NOT: arg2 (2. argüman) yalnız aarch64'te dolu (SVC path geçirir);
 * x86 nums 1/2/3 arg2 kullanmaz (zararsız). */
/* FAZ-A5 (D-281): WEAK → kem_os SAF-.kem kdl_syscall_isle (kem_gorev.kem, strong)
 * bunu link'te override eder (guard/variant gerekmez, cascade yok). Diğer kernel'ler
 * strong .kem sağlamaz → bu C weak tanımı kullanılır (davranış değişmez). */
__attribute__((weak))
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
         * D-151: ptr doğrulanır (user VA ∪ .rodata) — unmapped→halt / kernel-sızıntı engeli. */
        if (!kdl_user_oku_str_gecerli(arg)) return (uint64_t)(int64_t)-1;
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
        if (!kdl_user_oku_str_gecerli(arg)) return (uint64_t)(int64_t)-1;   /* D-151: ad okuma */
        int i = kdl_dosya_ac((const char *)(uintptr_t)arg);
        if (i < 0) return (uint64_t)(int64_t)-1;
        kdl_dosyalar[i].deger = (int64_t)arg2;
        return 0;
    } else if (num == 16) {
        /* D-131 dosya_oku(ad=arg): isimli dosyanın değerini döner (yoksa -1). */
        if (!kdl_user_oku_str_gecerli(arg)) return (uint64_t)(int64_t)-1;   /* D-151: ad okuma */
        int i = kdl_dosya_bul((const char *)(uintptr_t)arg);
        if (i < 0) return (uint64_t)(int64_t)-1;
        return (uint64_t)kdl_dosyalar[i].deger;
    } else if (num == 17) {
        /* D-132 dosya_yaz_metin(ad=arg, str=arg2): kullanıcı belleğinden string'i
         * dosyanın içeriğine kopyala (bulk yaz). Dönen = yazılan byte sayısı. */
        if (!kdl_user_oku_str_gecerli(arg)) return (uint64_t)(int64_t)-1;    /* D-151: ad okuma */
        int i = kdl_dosya_ac((const char *)(uintptr_t)arg);
        if (i < 0) return (uint64_t)(int64_t)-1;
        if (!kdl_user_oku_str_gecerli(arg2)) return (uint64_t)(int64_t)-1;   /* D-151: içerik okuma */
        const char *s = (const char *)(uintptr_t)arg2;
        int n = 0;
        while (n < KDL_ICERIK_MAX - 1 && s[n]) { kdl_dosyalar[i].icerik[n] = s[n]; n++; }
        kdl_dosyalar[i].icerik[n] = 0;
        kdl_dosyalar[i].boyut = n;
        return (uint64_t)(int64_t)n;
    } else if (num == 18) {
        /* D-132 dosya_oku_metin(ad=arg, buf=arg2): dosya içeriğini kullanıcı
         * tamponuna (buf) kopyala (bulk oku). Dönen = kopyalanan byte sayısı (-1 yok). */
        if (!kdl_user_oku_str_gecerli(arg)) return (uint64_t)(int64_t)-1;   /* D-151: ad okuma */
        int i = kdl_dosya_bul((const char *)(uintptr_t)arg);
        if (i < 0) return (uint64_t)(int64_t)-1;
        /* D-153: stale/poisoned boyut clamp (savunma katmanı). kdl_dosya_yukle
         * artık deserialize'te clamp'lasa da, okumayı burada da 64-byte icerik[]
         * tamponuyla sınırla → aşırı boyut asla OOB okuma tetikleyemez. */
        int lim = kdl_dosyalar[i].boyut;
        if (lim < 0 || lim >= KDL_ICERIK_MAX) lim = 0;
        /* D-150: kernel user-tampona YAZAR → adres user aralığında olmalı (kernel
         * belleğine yazmayı engelle). +1 = sonlandırıcı null. */
        if (!kdl_user_yaz_ptr_gecerli(arg2, (uint64_t)lim + 1))
            return (uint64_t)(int64_t)-1;
        char *buf = (char *)(uintptr_t)arg2;
        int n = 0;
        while (n < lim) { buf[n] = kdl_dosyalar[i].icerik[n]; n++; }
        buf[n] = 0;
        return (uint64_t)(int64_t)n;
    } else if (num == 19) {
        /* D-133 dosya_sayisi(): kullanımdaki dosya sayısı (ls için). */
        int c = 0;
        for (int i = 0; i < KDL_DOSYA_MAX; i++) if (kdl_dosyalar[i].kullanildi) c++;
        return (uint64_t)(int64_t)c;
    } else if (num == 20) {
        /* D-133 dosya_ad(idx=arg, buf=arg2): idx'inci KULLANILAN dosyanın adını user
         * tamponuna kopyala (ls). D-134: sil sonrası boşluk atla → kullanılan-index
         * (raw index değil) → sil edilmiş slotlar sıralamayı bozmaz. Dönen = uzunluk. */
        int idx = (int)arg, seen = 0;
        /* D-150: kernel user-tampona YAZAR → adres user aralığında olmalı. */
        if (!kdl_user_yaz_ptr_gecerli(arg2, KDL_AD_MAX)) return (uint64_t)(int64_t)-1;
        for (int i = 0; i < KDL_DOSYA_MAX; i++) {
            if (!kdl_dosyalar[i].kullanildi) continue;
            if (seen == idx) {
                char *buf = (char *)(uintptr_t)arg2;
                int n = 0;
                while (n < KDL_AD_MAX - 1 && kdl_dosyalar[i].ad[n]) { buf[n] = kdl_dosyalar[i].ad[n]; n++; }
                buf[n] = 0;
                return (uint64_t)(int64_t)n;
            }
            seen++;
        }
        return (uint64_t)(int64_t)-1;
    } else if (num == 21) {
        /* D-134 dosya_sil(ad=arg): isimli dosyayı sil (slot serbest). 0=ok, -1=yok. */
        if (!kdl_user_oku_str_gecerli(arg)) return (uint64_t)(int64_t)-1;   /* D-151: ad okuma */
        int i = kdl_dosya_bul((const char *)(uintptr_t)arg);
        if (i < 0) return (uint64_t)(int64_t)-1;
        kdl_dosyalar[i].kullanildi = 0;
        kdl_dosyalar[i].boyut = 0;
        return 0;
    } else if (num == 22) {
        /* D-140 kanal_gonder(deger=arg): mesaj kuyruğuna yaz (0=ok, -1=dolu). */
        int yeni = (kdl_msg_son + 1) % KDL_MSG_MAX;
        if (yeni == kdl_msg_bas) return (uint64_t)(int64_t)-1;
        kdl_msg[kdl_msg_son] = (int)arg;
        kdl_msg_son = yeni;
        return 0;
    } else if (num == 23) {
        /* D-140 kanal_al(): mesaj kuyruğundan oku (değer veya boşsa -1). */
        if (kdl_msg_bas == kdl_msg_son) return (uint64_t)(int64_t)-1;
        int v = kdl_msg[kdl_msg_bas];
        kdl_msg_bas = (kdl_msg_bas + 1) % KDL_MSG_MAX;
        return (uint64_t)(int64_t)v;
    }
#if defined(__aarch64__)
    else if (num == 24) {
        /* D-176 net_gonder(cerceve=arg, uzun=arg2): EL0 ham ethernet çerçevesi yollar.
         * GÜVENLİK: frame user VA'da + mantıklı ethernet boyu (kernel OKUR); driver
         * frame'i kendi TX buffer'ına kopyalar. Dönen = gönderilen byte veya -1. */
        if (arg2 == 0 || arg2 > 1514) return (uint64_t)(int64_t)-1;
        if (!kdl_user_yaz_ptr_gecerli(arg, arg2)) return (uint64_t)(int64_t)-1;  /* frame user VA */
        uint64_t nb = kdl_virtio_net_bul();
        if (!nb) return (uint64_t)(int64_t)-1;   /* net cihazı yok */
        return (uint64_t)(int64_t)kdl_virtio_net_gonder(nb, (const uint8_t *)(uintptr_t)arg, (int)arg2);
    } else if (num == 25) {
        /* D-176 net_al(buf=arg, maxlen=arg2): EL0 gelen çerçeveyi alır. GÜVENLİK: kernel
         * user-buffer'a YAZAR → user VA'da olmalı (D-150). Dönen = alınan byte veya 0/-1.
         * Kısa per-çağrı timeout (2M tik) → EL0 kendi poll döngüsünde tekrar çağırır
         * (yük-duyarlı uzun busy-wait YOK — D-158 dersi). */
        if (arg2 == 0 || arg2 > 2048) return (uint64_t)(int64_t)-1;
        if (!kdl_user_yaz_ptr_gecerli(arg, arg2)) return (uint64_t)(int64_t)-1;  /* buf user VA */
        uint64_t nb = kdl_virtio_net_bul();
        if (!nb) return (uint64_t)(int64_t)-1;
        return (uint64_t)(int64_t)kdl_virtio_net_al(nb, (uint8_t *)(uintptr_t)arg, (int)arg2, 2000000);
    } else if (num == 26) {
        /* PART 3(d) read_satir(buf=arg, max=arg2): EL0 shell için UART satır oku. Kernel
         * (EL1) PL011 RX yapar (EL0 Device-MMIO erişemez!) + user-buffer'a kopyalar. Dönen
         * = okunan byte (>=0) veya -1 (EOF). Bounded per-byte poll (deadlock-guard). GÜVENLİK:
         * kernel user-buffer'a YAZAR → user-VA olmalı (D-150). */
        if (arg2 == 0 || arg2 > 4096) return (uint64_t)(int64_t)-1;
        if (!kdl_user_yaz_ptr_gecerli(arg, arg2)) return (uint64_t)(int64_t)-1;
        char *ubuf = (char *)(uintptr_t)arg;
        int umax = (int)arg2, un = 0;
        for (;;) {
            unsigned int geldi = 0;
            for (uint64_t i = 0; i < 8000000UL; i++) {
                if (!(*(volatile uint32_t *)(uintptr_t)(0x09000000UL + 0x18u) & (1u << 4))) { geldi = 1; break; }
            }
            if (!geldi) { ubuf[un] = 0; return (uint64_t)(int64_t)(un > 0 ? un : -1); }  /* EOF */
            char c = (char)(*(volatile uint32_t *)(uintptr_t)(0x09000000UL + 0x00u) & 0xFFu);
            if (c == '\n' || c == '\r') { ubuf[un] = 0; return (uint64_t)(int64_t)un; }
            if (un < umax - 1) ubuf[un++] = c;
        }
    } else if (num == 27) {
        /* PART 3(d) saat(): PL031 RTC Unix epoch saniye (EL0 shell 'saat' komutu için —
         * EL0 Device-MMIO erişemez, kernel okur). */
        return (uint64_t)(*(volatile uint32_t *)(uintptr_t)0x09010000UL);
    }
#endif
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

/* xHCI MSI-X: kesilen kodun yazmaçları korunur, olay tüketimi ana döngüdedir. */
extern void kdl_xhci_irq(void) __attribute__((weak));
__attribute__((naked)) static void kdl_xhci_kocani(void) {
    __asm__ volatile("pushq %rax\n pushq %rcx\n pushq %rdx\n pushq %rsi\n pushq %rdi\n"
                     "pushq %r8\n pushq %r9\n pushq %r10\n pushq %r11\n pushq %rbp\n"
                     "movq %rsp,%rbp\n andq $-16,%rsp\n cld\n call kdl_xhci_irq\n"
                     "movq %rbp,%rsp\n popq %rbp\n popq %r11\n popq %r10\n popq %r9\n"
                     "popq %r8\n popq %rdi\n popq %rsi\n popq %rdx\n popq %rcx\n popq %rax\n iretq");
}
extern void kdl_irq1_stub(void), kdl_irq12_stub(void), kdl_lapic_timer_stub(void), kdl_spurious_stub(void);
extern void kdl_irq0_stub(void);     /* boot/start_x86_64.S — IRQ0 (timer) */
extern void kdl_syscall_stub(void);  /* boot/start_x86_64.S — int 0x80 syscall */

static void kdl_idt_gate(int i, uint64_t adr, uint8_t ist) {
    kdl_idt[i].offset_dusuk  = (uint16_t)(adr & 0xFFFF);
    kdl_idt[i].segment       = 0x08;       /* 64-bit kod segmenti */
    kdl_idt[i].ist           = ist & 7;
    kdl_idt[i].tip_oz        = 0x8E;       /* present, DPL0, interrupt gate */
    kdl_idt[i].offset_orta   = (uint16_t)((adr >> 16) & 0xFFFF);
    kdl_idt[i].offset_yuksek = (uint32_t)((adr >> 32) & 0xFFFFFFFF);
    kdl_idt[i].rezerve       = 0;
}

/* TSS düzeni: Linux x86_hw_tss; tools/codex/dalga11_sabit.py doğrular.
 * Packed yazma: IST1 +36, IOPB +102. Yığınlar çekirdeğin .bss alanındadır. */
extern uint8_t kdl_x86_tss[];
extern uint64_t kdl_x86_gdt[];
extern uint64_t kdl_ist1_alt[], kdl_ist1_ust[];
extern uint64_t kdl_ist2_alt[], kdl_ist2_ust[];
extern uint64_t kdl_ist3_alt[], kdl_ist3_ust[];
struct KdlTssIst {
    uint8_t bas[36];
    uint64_t ist[7];
    uint8_t son[10];
    uint16_t iopb;
} __attribute__((packed));
static void kdl_ist_kur(void) {
    struct KdlTssIst *t = (struct KdlTssIst *)kdl_x86_tss;
    t->ist[0] = (uintptr_t)kdl_ist1_ust;
    t->ist[1] = (uintptr_t)kdl_ist2_ust;
    t->ist[2] = (uintptr_t)kdl_ist3_ust;
    t->iopb = 104;
    *kdl_ist1_alt = *kdl_ist2_alt = *kdl_ist3_alt = UINT64_C(0x4b454d4755495354);
    uint64_t a = (uintptr_t)t;
    kdl_x86_gdt[5] = 103 | ((a & 0xffffff) << 16) | (UINT64_C(0x89) << 40) | ((a & 0xff000000) << 32);
    kdl_x86_gdt[6] = a >> 32;
    __asm__ volatile("ltr %w0" : : "r"((uint16_t)40) : "memory");
}

/* GS bloğunun ilk alanları KEMGU ve QMP ile ortak, tamamı 64 bit. */
struct CekirdekVeri {
    uint64_t no, apic, tik, surec;
    uint64_t yigin_alt, yigin_ust, tss, gdt, ist[3], cr3;
    uint64_t kullanici_giris, kullanici_gs;
    uint64_t kilit_sirasi, kilit_beklemeleri, alan_surumu;
    uint64_t dilim, donus_rsp, donus_rip, onceki_rsp0, vektor, hata, hata_adresi;
    uint64_t tek_dilim, bekliyor, bosta_tik, gecis_hedef;
    uint64_t farkli_surec, yuva_pid[8];
};
_Static_assert(__builtin_offsetof(struct CekirdekVeri, alan_surumu) == 128, "GS CR3 surumu");
_Static_assert(__builtin_offsetof(struct CekirdekVeri, dilim) == 136, "GS dilim");
_Static_assert(__builtin_offsetof(struct CekirdekVeri, hata_adresi) == 184, "GS CR2");
_Static_assert(__builtin_offsetof(struct CekirdekVeri, bosta_tik) == 208, "GS bosta");
_Static_assert(__builtin_offsetof(struct CekirdekVeri, gecis_hedef) == 216, "GS sonraki CR3");
_Static_assert(__builtin_offsetof(struct CekirdekVeri, farkli_surec) == 224, "GS PID sayaci");
struct ApTablolari {
    struct CekirdekVeri veri;
    uint64_t gdt[7];
    uint8_t tss[104] __attribute__((aligned(16)));
};
_Static_assert(__builtin_offsetof(struct CekirdekVeri, tik) == 16, "GS tik");
_Static_assert(__builtin_offsetof(struct CekirdekVeri, surec) == 24, "GS surec");
_Static_assert(__builtin_offsetof(struct CekirdekVeri, cr3) == 88, "GS CR3");
_Static_assert(__builtin_offsetof(struct CekirdekVeri, kullanici_giris) == 96, "GS ring3");
_Static_assert(__builtin_offsetof(struct CekirdekVeri, kilit_sirasi) == 112, "GS kilit sirasi");
_Static_assert(__builtin_offsetof(struct CekirdekVeri, kilit_beklemeleri) == 120, "GS kilit bekleme");
_Static_assert(sizeof(struct ApTablolari) <= 4096, "AP tablolarinin sayfasi");
struct CekirdekVeri kdl_cekirdek_bsp __attribute__((aligned(64)));

static void kdl_gs_kur(struct CekirdekVeri *c) {
    uintptr_t a = (uintptr_t)c;
    __asm__ volatile("wrmsr" : : "c"(UINT32_C(0xc0000101)),
                     "a"((uint32_t)a), "d"((uint32_t)(a >> 32)) : "memory");
    __asm__ volatile("wrmsr" : : "c"(UINT32_C(0xc0000102)), "a"(0u), "d"(0u) : "memory");
}

struct KdlTabloYazmaci {
    uint16_t limit;
    uintptr_t taban;
} __attribute__((packed));

/* Panik, kesilen işlemcinin gerçek TR/GDTR çiftindeki IST sınırlarını denetler. */
int kdl_smp_ist_denetle(void) {
    struct KdlTabloYazmaci g;
    uint16_t secici;
    __asm__ volatile("sgdt %0" : "=m"(g));
    __asm__ volatile("str %0" : "=r"(secici));
    if (secici != 40 || g.limit < 55) return 0;
    const uint64_t *tanim = (const uint64_t *)(g.taban + secici);
    uint64_t d = tanim[0];
    if ((d & 0xffff) != 103 || ((d >> 40) & 0x8f) != 0x8b) return 0;
    uintptr_t a = ((d >> 16) & 0xffffff) | ((d >> 32) & 0xff000000) | (tanim[1] << 32);
    const struct KdlTssIst *t = (const struct KdlTssIst *)a;
    for (unsigned i = 0; i < 3; ++i) {
        if (t->ist[i] < 16384 ||
            *(const volatile uint64_t *)(t->ist[i] - 16384) != UINT64_C(0x4b454d4755495354)) return 0;
    }
    return 1;
}

/* Bir AP için ilk sayfa veri/GDT/TSS, arkasında üç ayrı 16 KiB IST bulunur. */
int kdl_smp_ap_kur(struct ApTablolari *p, unsigned no, unsigned id,
                   uintptr_t alt, uintptr_t ust) {
    uintptr_t bas = (uintptr_t)p;
    if (!bas || (bas & 4095) || !no || ust - alt != 16384 ||
        (bas < ust && bas + 13 * 4096 > alt)) return 0;
    struct CekirdekVeri *c = &p->veri;
    struct KdlTssIst *t = (struct KdlTssIst *)p->tss;
    uintptr_t a = (uintptr_t)t;
    if (a < bas || a + sizeof(*t) > bas + 4096) return 0;
    c->no = no;
    c->apic = id;
    c->yigin_alt = alt;
    c->yigin_ust = ust;
    c->gdt = (uintptr_t)p->gdt;
    c->tss = a;
    __asm__ volatile("mov %%cr3,%0" : "=r"(c->cr3));
    for (unsigned i = 0; i < 3; ++i) {
        uintptr_t taban = bas + 4096 + i * 16384;
        *(uint64_t *)taban = UINT64_C(0x4b454d4755495354);
        c->ist[i] = t->ist[i] = taban + 16384;
    }
    __builtin_memcpy(p->tss + 4, &ust, sizeof(ust));
    t->iopb = 104;
    for (unsigned i = 0; i < 5; ++i) p->gdt[i] = kdl_x86_gdt[i];
    p->gdt[5] = 103 | ((a & 0xffffff) << 16) | (UINT64_C(0x89) << 40) | ((a & 0xff000000) << 32);
    p->gdt[6] = a >> 32;
    const struct KdlTabloYazmaci gdtr = {sizeof(p->gdt) - 1, c->gdt};
    const struct KdlTabloYazmaci idtr = {sizeof(kdl_idt) - 1, (uintptr_t)kdl_idt};
    /* Eski trampolin CS=0x18 idi. Kalıcı çekirdek kod seçicisini de yükle. */
    __asm__ volatile("lgdt %0\n lidt %1\n subq $128,%%rsp\n pushq $8\n leaq 1f(%%rip),%%rax\n pushq %%rax\n lretq\n"
                     "1: addq $128,%%rsp\n movw $16,%%ax\n movw %%ax,%%ds\n movw %%ax,%%es\n movw %%ax,%%ss\n"
                     "movw $40,%%ax\n ltr %%ax"
                     : : "m"(gdtr), "m"(idtr) : "rax", "memory");
    kdl_gs_kur(c);
    return 1;
}

extern void smp_ap_zaman_irq(void) __attribute__((weak));
extern int kem_ipi_isle(int) __attribute__((weak));
extern void kdl_ipi_80(void), kdl_ipi_81(void), kdl_ipi_82(void), kdl_ipi_83(void);
extern uintptr_t kdl_lapic_taban;
__attribute__((naked)) static void kdl_ap_zaman_kocani(void) {
    __asm__ volatile(
        "testb $3,8(%rsp)\n jz 1f\n swapgs\n 1:\n"
        "pushq %rax\n pushq %rcx\n pushq %rdx\n pushq %rsi\n pushq %rdi\n"
        "pushq %r8\n pushq %r9\n pushq %r10\n pushq %r11\n pushq %rbp\n"
        "movq %rsp,%rbp\n andq $-16,%rsp\n cld\n call smp_ap_zaman_irq\n"
        "movq kdl_lapic_taban(%rip),%rax\n movl $0,0xb0(%rax)\n movq %rbp,%rsp\n"
        "popq %rbp\n popq %r11\n popq %r10\n popq %r9\n popq %r8\n popq %rdi\n"
        "popq %rsi\n popq %rdx\n popq %rcx\n popq %rax\n"
        "testb $3,8(%rsp)\n jz 2f\n swapgs\n 2: iretq");
}

/* SMP diliminde donanımın kesme çerçevesini yalnız yerel GS kaydına aktar.
 * Eski çekirdek ve tek çekirdekli kullanıcı girişleri kendi koçanlarını kullanır. */
volatile int kdl_smp_dilim_acik;
extern void kdl_apic_timer_irq(void);
__attribute__((naked, used)) static void kdl_smp_dilim_yakala(void) {
    __asm__ volatile(
        "pushq %rax\n pushq %rbx\n pushq %rcx\n pushq %rdx\n pushq %rsi\n pushq %rdi\n"
        "pushq %rbp\n pushq %r8\n pushq %r9\n pushq %r10\n pushq %r11\n pushq %r12\n"
        "pushq %r13\n pushq %r14\n pushq %r15\n cld\n"
        "movq %rsp,%rsi\n movq %gs:136,%rdi\n movl $15,%ecx\n rep movsq\n"
        "lodsq\n movq %rax,%gs:168\n lodsq\n movq %rax,%gs:176\n"
        "movq %cr2,%rax\n movq %rax,%gs:184\n movl $5,%ecx\n rep movsq\n"
        "andq $-16,%rsp\n cmpq $48,%gs:168\n je 1f\n cmpq $49,%gs:168\n je 2f\n"
        "cmpq $128,%gs:168\n jne 4f\n incq %gs:96\n movl $0xc0000102,%ecx\n"
        "rdmsr\n shlq $32,%rdx\n orq %rdx,%rax\n movq %rax,%gs:104\n jmp 4f\n"
        "1: call kdl_apic_timer_irq\n jmp 3f\n 2: call smp_ap_zaman_irq\n"
        "3: lock incq kdl_dilim_sayisi(%rip)\n movq kdl_lapic_taban(%rip),%rax\n movl $0,176(%rax)\n"
        "4: movq %gs:144,%rsp\n jmp *%gs:152");
}
#define KDL_SMP_KAPI(vektor, cs, hata, eski)                                 \
    __attribute__((naked)) static void kdl_smp_kapi_##vektor(void) {          \
        __asm__ volatile("testb $3," #cs "(%rsp)\n jz 2f\n"                  \
                         "cmpl $0,kdl_smp_dilim_acik(%rip)\n je 2f\n"         \
                         "swapgs\n cmpq $0,%gs:136\n jne 1f\n swapgs\n"         \
                         "2: jmp " eski "\n1: " hata "pushq $" #vektor       \
                         "\n jmp kdl_smp_dilim_yakala");                       \
    }
#define KDL_SMP_KAPILAR(X) \
    X(0,    8, "pushq $0\n", "*kdl_isr_tablo(%rip)") \
    X(1,    8, "pushq $0\n", "*kdl_isr_tablo+8(%rip)") \
    X(3,    8, "pushq $0\n", "*kdl_isr_tablo+24(%rip)") \
    X(4,    8, "pushq $0\n", "*kdl_isr_tablo+32(%rip)") \
    X(5,    8, "pushq $0\n", "*kdl_isr_tablo+40(%rip)") \
    X(6,    8, "pushq $0\n", "*kdl_isr_tablo+48(%rip)") \
    X(7,    8, "pushq $0\n", "*kdl_isr_tablo+56(%rip)") \
    X(10,  16, "", "*kdl_isr_tablo+80(%rip)") \
    X(11,  16, "", "*kdl_isr_tablo+88(%rip)") \
    X(12,  16, "", "*kdl_isr_tablo+96(%rip)") \
    X(13,  16, "", "*kdl_isr_tablo+104(%rip)") \
    X(14,  16, "", "*kdl_isr_tablo+112(%rip)") \
    X(16,   8, "pushq $0\n", "*kdl_isr_tablo+128(%rip)") \
    X(17,  16, "", "*kdl_isr_tablo+136(%rip)") \
    X(19,   8, "pushq $0\n", "*kdl_isr_tablo+152(%rip)") \
    X(48,   8, "pushq $0\n", "kdl_lapic_timer_stub") \
    X(49,   8, "pushq $0\n", "kdl_ap_zaman_kocani") \
    X(128,  8, "pushq $0\n", "kdl_syscall_stub")
KDL_SMP_KAPILAR(KDL_SMP_KAPI)
#undef KDL_SMP_KAPI

/* BSP ilk AP'yi uyandırmadan önce IDT kurulur, canlı bir IDT girdisi yamanmaz. */
static void kdl_smp_kapilari_kur(void) {
#define KDL_DILIM_KAPISI(v, cs, hata, eski) kdl_idt_gate(v, (uintptr_t)&kdl_smp_kapi_##v, 0);
    KDL_SMP_KAPILAR(KDL_DILIM_KAPISI)
    kdl_idt[128].tip_oz = 0xee;
#undef KDL_DILIM_KAPISI
}
#undef KDL_SMP_KAPILAR

/* Panik sırasında NMI teslimi IF, bilet kilitleri ve normal IPI kutusundan bağımsızdır. */
static unsigned kdl_panik_lider __attribute__((used));
void kdl_smp_panik_gir(uintptr_t lapic) {
    if (!lapic) return;
    volatile uint32_t *tablo = (volatile uint32_t *)lapic;
    unsigned kim = (tablo[0x20 / 4] >> 24) + 1;
    unsigned eski = 0;
    if (__atomic_compare_exchange_n(&kdl_panik_lider, &eski, kim, 0,
                                    __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
        /* Kendi ICR teslimimiz bitince tüm diğer yerel APIC'lere NMI gönder. */
        for (unsigned i = 0; i < 1000000 && (tablo[0x300 / 4] & 0x1000); ++i)
            __asm__ volatile("pause");
        tablo[0x300 / 4] = 0xc4400;
    } else if (eski != kim) {
        for (;;) __asm__ volatile("cli; hlt");
    }
}
__attribute__((naked)) static void kdl_panik_nmi(void) {
    __asm__ volatile("cmpl $0,kdl_panik_lider(%rip)\n"
                     "je 2f\n cli\n 1: hlt\n jmp 1b\n"
                     "2: jmp *kdl_isr_tablo+16(%rip)");
}

void kdl_idt_kur(void) {
    kdl_ist_kur();
    struct CekirdekVeri *c = &kdl_cekirdek_bsp;
    extern char __stack_top[], __bss_end[];
    c->yigin_ust = (uintptr_t)__stack_top;
    c->yigin_alt = ((uintptr_t)__bss_end + 15) & ~(uintptr_t)15;
    c->tss = (uintptr_t)kdl_x86_tss;
    c->gdt = (uintptr_t)kdl_x86_gdt;
    c->ist[0] = (uintptr_t)kdl_ist1_ust;
    c->ist[1] = (uintptr_t)kdl_ist2_ust;
    c->ist[2] = (uintptr_t)kdl_ist3_ust;
    __asm__ volatile("mov %%cr3,%0" : "=r"(c->cr3));
    kdl_gs_kur(c);
    for (int i = 0; i < 32; i++)
        kdl_idt_gate(i, (uint64_t)(uintptr_t)kdl_isr_tablo[i], i == 8 ? 1 : i == 2 ? 2 : i == 18 ? 3 : 0);   /* istisnalar 0-31 */
    kdl_idt_gate(32, (uint64_t)(uintptr_t)&kdl_irq0_stub, 0);        /* IRQ0 → vektör 32 */
    kdl_idt_gate(0x80, (uint64_t)(uintptr_t)&kdl_syscall_stub, 0);   /* int 0x80 → syscall */
    /* [D-699] Sistem çağrısı kapısı DPL3 (0xEE = present, DPL3, interrupt gate):
     * ring3 `int 0x80` yalnız kapı DPL >= CPL ise geçer; DPL0 kapı ring3'ten #GP
     * verir. Çekirdek (ring0) çağrıları etkilenmez. İstisna/IRQ kapıları DPL0
     * KALIR (ring3 `int 13` gibi yazılımsal istisna taklidini reddeder). */
    kdl_idt[0x80].tip_oz = 0xEE;
    kdl_idt_gate(33, (uintptr_t)&kdl_irq1_stub, 0);
    kdl_idt_gate(44, (uintptr_t)&kdl_irq12_stub, 0);
    if (kdl_xhci_irq) kdl_idt_gate(65, (uintptr_t)&kdl_xhci_kocani, 0);
    kdl_idt_gate(48, (uintptr_t)&kdl_lapic_timer_stub, 0);
    if (smp_ap_zaman_irq) kdl_idt_gate(49, (uintptr_t)&kdl_ap_zaman_kocani, 0);
    if (kem_ipi_isle) {
        kdl_idt_gate(2, (uintptr_t)&kdl_panik_nmi, 2);
        kdl_idt_gate(80, (uintptr_t)&kdl_ipi_80, 0);
        kdl_idt_gate(81, (uintptr_t)&kdl_ipi_81, 0);
        kdl_idt_gate(82, (uintptr_t)&kdl_ipi_82, 0);
        kdl_idt_gate(83, (uintptr_t)&kdl_ipi_83, 0);
    }
    extern void kxs_smp_isci(void) __attribute__((weak));
    if (kxs_smp_isci) kdl_smp_kapilari_kur();
    kdl_idt_gate(255, (uintptr_t)&kdl_spurious_stub, 0);
    extern void *kdl_msi_kocanlari[14];
    for (int i = 0; i < 14; ++i) kdl_idt_gate(66 + i, (uintptr_t)kdl_msi_kocanlari[i], 0);

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
