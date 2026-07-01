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
