/*
 * MİLESTONE D testi (aarch64) — MİNİ DOSYA SİSTEMİ (superblock + inode + blok-bitmap).
 * =============================================================================
 *
 * D-143 (duz-serialize) ve D-206 (WAL journaling) uzerine: virtio-blk uzerinde
 * GERCEK bir dosya-sistemi yapisi. Duz tek-bloklu serialize'in OTESINDE — cok
 * dosya, cok blok, bitmap-tabanli blok tahsisi.
 *
 * Disk duzeni (512-baytlik sektorler/bloklar):
 *   blok 0  = SUPERBLOCK: [magic "MFS1"(4)][inode_sayisi(4)][blok_sayisi(4)]
 *                         [veri_baslangic(4)]  (hepsi little-endian u32)
 *   blok 1  = BLOK-BİTMAP: her veri blogu icin 1 bayt (0=bos, 1=dolu). Bit yerine
 *                         bayt kullaniyoruz (basitlik + endianness-bagimsiz).
 *   blok 2  = INODE-TABLO: her inode 32 bayt:
 *                         ad[16] + boyut(u32) + ilk_blok(u32) + blok_sayisi(u32)
 *                         (16+4+4+4 = 28 kullanilir, 32'ye hizali) → 512/32 = 16 inode.
 *   blok 3+ = VERİ blolari (veri_baslangic = 3).
 *
 * İşlemler:
 *   mfs_bicimle()            — superblock + bos bitmap + bos inode-tablo yaz.
 *   mfs_olustur(ad,veri,uzun)— bitmap'ten gereken sayida ardisik veri blogu ayir,
 *                              veriyi bloklara yaz, inode kaydini doldur (ad/boyut/
 *                              ilk_blok/blok_sayisi), inode-tablo + bitmap flush et.
 *   mfs_oku(ad,tampon,*uzun) — inode'u ad ile bul, veri bloklarini sirayla oku,
 *                              tampona yaz, boyutu dondur.
 *
 * Senaryo (tek-boot, deterministik):
 *   1) mfs_bicimle().
 *   2) "gunluk" → kisa icerik (tek blok): "kayit-1" (7 bayt).
 *   3) "veri"   → BLOK-SINIRINI ASAN uzun icerik (600 bayt = 2 blok): tekrar eden
 *                 bayt deseni. Cok-blok tahsis kanitlanir.
 *   4) Diskten geri oku (mfs_oku) → icerik + boyut ikisi de eslesir.
 *   5) Bitmap kontrolu: "gunluk" 1 blok + "veri" 2 blok = 3 blok dolu, kalan bos.
 *
 * Kanit: iki dosya round-trip (biri cok-bloklu) + bitmap tutarli → "MINIFS OK".
 * Baslangic: "MINIFS BASLA". Marker: "MINIFS OK".
 */
#include <stdint.h>
extern void kdl_yazdir_metin(const char *);
extern void kdl_yazdir_satir(void);
extern void kdl_yaz_metin(const char *);
extern void kdl_yaz_onaltilik(uint64_t);   /* newline'siz onaltilik ("0x" onekli) */
extern uint64_t kdl_virtio_blk_bul(void);
extern int kdl_virtio_blk_kur(uint64_t base);
extern int kdl_virtio_blk_oku(uint64_t base, uint64_t sektor, uint8_t *hedef);
extern int kdl_virtio_blk_yaz(uint64_t base, uint64_t sektor, const uint8_t *kaynak);

#define BLOK_BOYUT       512
#define SB_SEKTOR        0UL     /* superblock */
#define BITMAP_SEKTOR    1UL     /* blok-bitmap */
#define INODE_SEKTOR     2UL     /* inode-tablo */
#define VERI_BASLANGIC   3U      /* ilk veri blogu numarasi */
#define TOPLAM_BLOK      64U     /* diskin toplam blok sayisi (disk.img boyutu) */
#define VERI_BLOK_SAYISI (TOPLAM_BLOK - VERI_BASLANGIC)  /* bitmap kapsami */

#define INODE_BOYUT      32      /* her inode kaydinin bayt boyutu (hizali) */
#define AD_UZUN          16      /* dosya adi maks bayt (NUL-dolgu) */
#define MAKS_INODE       (BLOK_BOYUT / INODE_BOYUT)   /* 512/32 = 16 */

/* Superblock magic "MFS1" (bytes 0-3) — byte-byte (endianness bagimsiz). */
static const uint8_t SB_MAGIC[4] = { 'M', 'F', 'S', '1' };

static uint8_t sbtampon[BLOK_BOYUT];     /* superblock tamponu */
static uint8_t bmtampon[BLOK_BOYUT];     /* bitmap tamponu */
static uint8_t intampon[BLOK_BOYUT];     /* inode-tablo tamponu */
static uint8_t bloktampon[BLOK_BOYUT];   /* tek-blok veri okuma/yazma tamponu */

static _Noreturn void halt(void) { for (;;) { __asm__ volatile("wfe"); } }

/* --- little-endian u32 yardimcilari --- */
static void u32_yaz(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xff);
    p[1] = (uint8_t)((v >> 8) & 0xff);
    p[2] = (uint8_t)((v >> 16) & 0xff);
    p[3] = (uint8_t)((v >> 24) & 0xff);
}
static uint32_t u32_oku(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* --- string yardimcilari (libc yok) --- */
static uint32_t str_uzun(const char *s) {
    uint32_t n = 0;
    while (s[n]) n++;
    return n;
}
/* iki isim AD_UZUN icinde esit mi? (NUL-dolgulu karsilastirma) */
static int ad_esit(const uint8_t *a, const char *b) {
    for (int i = 0; i < AD_UZUN; i++) {
        uint8_t bc = (uint8_t)b[i];
        if (a[i] != bc) return 0;
        if (bc == 0) return 1;   /* iki taraf da burada biter */
    }
    return 1;                    /* AD_UZUN'a kadar tam esit */
}

/* --- BİÇİMLEME: superblock + bos bitmap + bos inode-tablo --- */
static int mfs_bicimle(uint64_t base) {
    /* superblock */
    for (int i = 0; i < BLOK_BOYUT; i++) sbtampon[i] = 0;
    sbtampon[0] = SB_MAGIC[0]; sbtampon[1] = SB_MAGIC[1];
    sbtampon[2] = SB_MAGIC[2]; sbtampon[3] = SB_MAGIC[3];
    u32_yaz(&sbtampon[4], 0);                 /* inode_sayisi = 0 */
    u32_yaz(&sbtampon[8], TOPLAM_BLOK);       /* blok_sayisi */
    u32_yaz(&sbtampon[12], VERI_BASLANGIC);   /* veri_baslangic */
    if (kdl_virtio_blk_yaz(base, SB_SEKTOR, sbtampon) != 0) return -1;

    /* bos bitmap (tumu 0 = bos) */
    for (int i = 0; i < BLOK_BOYUT; i++) bmtampon[i] = 0;
    if (kdl_virtio_blk_yaz(base, BITMAP_SEKTOR, bmtampon) != 0) return -2;

    /* bos inode-tablo (tumu 0 = ad[0]==0 → kullanilmayan slot) */
    for (int i = 0; i < BLOK_BOYUT; i++) intampon[i] = 0;
    if (kdl_virtio_blk_yaz(base, INODE_SEKTOR, intampon) != 0) return -3;
    return 0;
}

/* Bitmap'te ardisik `n` bos blok bul, dolu isaretle, ilk blok numarasini dondur.
 * bmtampon'un guncel (diskten okunmus) oldugu varsayilir; caller flush eder.
 * Basari → ilk_blok (>= VERI_BASLANGIC), yer yoksa → 0. */
static uint32_t bitmap_ayir(uint32_t n) {
    if (n == 0) return 0;
    for (uint32_t bas = 0; bas + n <= VERI_BLOK_SAYISI; bas++) {
        int uygun = 1;
        for (uint32_t j = 0; j < n; j++) {
            if (bmtampon[bas + j] != 0) { uygun = 0; break; }
        }
        if (uygun) {
            for (uint32_t j = 0; j < n; j++) bmtampon[bas + j] = 1;
            return VERI_BASLANGIC + bas;   /* mutlak blok numarasi */
        }
    }
    return 0;   /* yer yok */
}

/* --- DOSYA OLUŞTUR --- ad + veri(uzun bayt) → bitmap ayir, veriyi bloklara yaz,
 * inode kaydi doldur. Donus: 0 = ok, negatif = hata. */
static int mfs_olustur(uint64_t base, const char *ad, const uint8_t *veri, uint32_t uzun) {
    /* gereken veri blogu sayisi (yukari yuvarla) */
    uint32_t blok_say = (uzun + BLOK_BOYUT - 1) / BLOK_BOYUT;
    if (blok_say == 0) blok_say = 1;   /* bos dosya bile 1 blok tutar */

    /* superblock oku (inode_sayisi icin) */
    if (kdl_virtio_blk_oku(base, SB_SEKTOR, sbtampon) != 0) return -1;
    uint32_t inode_say = u32_oku(&sbtampon[4]);
    if (inode_say >= MAKS_INODE) return -2;   /* inode-tablo dolu */

    /* bitmap oku + ardisik blok ayir */
    if (kdl_virtio_blk_oku(base, BITMAP_SEKTOR, bmtampon) != 0) return -3;
    uint32_t ilk_blok = bitmap_ayir(blok_say);
    if (ilk_blok == 0) return -4;             /* disk dolu */

    /* veriyi bloklara yaz (son blok sifir-dolgulu) */
    for (uint32_t b = 0; b < blok_say; b++) {
        for (int i = 0; i < BLOK_BOYUT; i++) bloktampon[i] = 0;
        uint32_t ofs = b * BLOK_BOYUT;
        for (uint32_t i = 0; i < BLOK_BOYUT && ofs + i < uzun; i++) {
            bloktampon[i] = veri[ofs + i];
        }
        if (kdl_virtio_blk_yaz(base, (uint64_t)(ilk_blok + b), bloktampon) != 0) return -5;
    }

    /* bitmap'i flush et (ayrilan bloklar isaretli). */
    if (kdl_virtio_blk_yaz(base, BITMAP_SEKTOR, bmtampon) != 0) return -6;

    /* inode-tablo oku, yeni inode'u yaz (slot = inode_say). */
    if (kdl_virtio_blk_oku(base, INODE_SEKTOR, intampon) != 0) return -7;
    uint8_t *ie = &intampon[inode_say * INODE_BOYUT];
    for (int i = 0; i < INODE_BOYUT; i++) ie[i] = 0;
    uint32_t alen = str_uzun(ad);
    if (alen > AD_UZUN) alen = AD_UZUN;
    for (uint32_t i = 0; i < alen; i++) ie[i] = (uint8_t)ad[i];  /* kalan NUL */
    u32_yaz(&ie[16], uzun);         /* boyut */
    u32_yaz(&ie[20], ilk_blok);     /* ilk_blok */
    u32_yaz(&ie[24], blok_say);     /* blok_sayisi */
    if (kdl_virtio_blk_yaz(base, INODE_SEKTOR, intampon) != 0) return -8;

    /* superblock inode_sayisi'ni artir + flush. */
    u32_yaz(&sbtampon[4], inode_say + 1);
    if (kdl_virtio_blk_yaz(base, SB_SEKTOR, sbtampon) != 0) return -9;
    return 0;
}

/* --- DOSYA OKU --- ad ile inode bul, veri bloklarini oku, tampona kopyala.
 * `tampon` en az inode'un boyutu kadar olmali. Donus: boyut (>=0) veya negatif hata. */
static int64_t mfs_oku(uint64_t base, const char *ad, uint8_t *tampon) {
    if (kdl_virtio_blk_oku(base, SB_SEKTOR, sbtampon) != 0) return -1;
    /* magic dogrula */
    for (int i = 0; i < 4; i++) if (sbtampon[i] != SB_MAGIC[i]) return -2;
    uint32_t inode_say = u32_oku(&sbtampon[4]);

    if (kdl_virtio_blk_oku(base, INODE_SEKTOR, intampon) != 0) return -3;
    for (uint32_t s = 0; s < inode_say; s++) {
        const uint8_t *ie = &intampon[s * INODE_BOYUT];
        if (ie[0] == 0) continue;   /* bos slot */
        if (!ad_esit(ie, ad)) continue;
        /* eslesme: veri bloklarini oku. */
        uint32_t boyut = u32_oku(&ie[16]);
        uint32_t ilk_blok = u32_oku(&ie[20]);
        uint32_t blok_say = u32_oku(&ie[24]);
        uint32_t yazildi = 0;
        for (uint32_t b = 0; b < blok_say; b++) {
            if (kdl_virtio_blk_oku(base, (uint64_t)(ilk_blok + b), bloktampon) != 0) return -4;
            for (uint32_t i = 0; i < BLOK_BOYUT && yazildi < boyut; i++) {
                tampon[yazildi++] = bloktampon[i];
            }
        }
        return (int64_t)boyut;
    }
    return -5;   /* bulunamadi */
}

/* Bitmap'te dolu (=1) blok sayisini say (tutarlilik kontrolu). */
static int bitmap_dolu_say(uint64_t base) {
    if (kdl_virtio_blk_oku(base, BITMAP_SEKTOR, bmtampon) != 0) return -1;
    int say = 0;
    for (uint32_t i = 0; i < VERI_BLOK_SAYISI; i++) if (bmtampon[i]) say++;
    return say;
}

/* Belirli bir mutlak blok numarasinin bitmap'te dolu olup olmadigini dondur. */
static int bitmap_blok_dolu(uint32_t blok) {
    if (blok < VERI_BASLANGIC) return 0;
    uint32_t idx = blok - VERI_BASLANGIC;
    if (idx >= VERI_BLOK_SAYISI) return 0;
    return bmtampon[idx] ? 1 : 0;   /* bitmap_dolu_say sonrasi bmtampon guncel */
}

/* İki bayt-dizisi ilk `n` bayti esit mi? */
static int bayt_esit(const uint8_t *a, const uint8_t *b, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) if (a[i] != b[i]) return 0;
    return 1;
}

/* Okuma tamponlari (dosya icerigi geri okunur). */
static uint8_t oku_gunluk[BLOK_BOYUT];
static uint8_t oku_veri[BLOK_BOYUT * 4];   /* cok-bloklu dosya icin bol */

int main(void) {
    kdl_yazdir_metin("MINIFS BASLA");
    kdl_yazdir_satir();

    uint64_t base = kdl_virtio_blk_bul();
    if (!base) { kdl_yazdir_metin("DISK YOK"); kdl_yazdir_satir(); halt(); }
    if (kdl_virtio_blk_kur(base) != 0) { kdl_yazdir_metin("KUR HATA"); kdl_yazdir_satir(); halt(); }

    int tum_ok = 1;

    /* --- 1) BİÇİMLE --- */
    if (mfs_bicimle(base) != 0) { kdl_yazdir_metin("BICIMLE HATA"); kdl_yazdir_satir(); halt(); }

    /* --- 2) "gunluk" → kisa (tek blok) icerik --- */
    const char *G_ICERIK = "kayit-1";        /* 7 bayt */
    const uint32_t G_UZUN = 7;
    if (mfs_olustur(base, "gunluk", (const uint8_t *)G_ICERIK, G_UZUN) != 0) {
        kdl_yazdir_metin("OLUSTUR gunluk HATA"); kdl_yazdir_satir(); halt();
    }

    /* --- 3) "veri" → BLOK-SINIRINI ASAN uzun icerik (600 bayt = 2 blok) --- */
    static uint8_t v_icerik[600];
    const uint32_t V_UZUN = 600;
    for (uint32_t i = 0; i < V_UZUN; i++) {
        /* deterministik desen: byte = (i*7 + 3) & 0xff */
        v_icerik[i] = (uint8_t)((i * 7u + 3u) & 0xffu);
    }
    if (mfs_olustur(base, "veri", v_icerik, V_UZUN) != 0) {
        kdl_yazdir_metin("OLUSTUR veri HATA"); kdl_yazdir_satir(); halt();
    }

    /* --- 4) round-trip: "gunluk" geri oku --- */
    for (int i = 0; i < BLOK_BOYUT; i++) oku_gunluk[i] = 0xEE;
    int64_t g_boyut = mfs_oku(base, "gunluk", oku_gunluk);
    kdl_yaz_metin("gunluk: boyut=");
    kdl_yaz_onaltilik((uint64_t)(uint32_t)g_boyut);
    kdl_yazdir_satir();
    if (g_boyut != (int64_t)G_UZUN ||
        !bayt_esit(oku_gunluk, (const uint8_t *)G_ICERIK, G_UZUN)) {
        tum_ok = 0; kdl_yazdir_metin("gunluk FAIL"); kdl_yazdir_satir();
    }

    /* --- round-trip: "veri" geri oku (cok-blok) --- */
    for (uint32_t i = 0; i < sizeof(oku_veri); i++) oku_veri[i] = 0xEE;
    int64_t v_boyut = mfs_oku(base, "veri", oku_veri);
    kdl_yaz_metin("veri: boyut=");
    kdl_yaz_onaltilik((uint64_t)(uint32_t)v_boyut);
    kdl_yazdir_satir();
    if (v_boyut != (int64_t)V_UZUN || !bayt_esit(oku_veri, v_icerik, V_UZUN)) {
        tum_ok = 0; kdl_yazdir_metin("veri FAIL"); kdl_yazdir_satir();
    }

    /* --- 5) bitmap tutarlilik: gunluk(1 blok) + veri(2 blok) = 3 dolu --- */
    int dolu = bitmap_dolu_say(base);   /* bmtampon guncellenir */
    kdl_yaz_metin("bitmap dolu-blok=");
    kdl_yaz_onaltilik((uint64_t)(uint32_t)dolu);
    kdl_yazdir_satir();
    /* Ardisik ilk-uygun tahsis: gunluk → blok 3, veri → blok 4,5. */
    int blok3 = bitmap_blok_dolu(3);
    int blok4 = bitmap_blok_dolu(4);
    int blok5 = bitmap_blok_dolu(5);
    int blok6 = bitmap_blok_dolu(6);   /* bos olmali */
    if (dolu != 3 || !blok3 || !blok4 || !blok5 || blok6) {
        tum_ok = 0; kdl_yazdir_metin("bitmap FAIL"); kdl_yazdir_satir();
    }

    kdl_yazdir_metin(tum_ok ? "MINIFS OK" : "MINIFS HATA");
    kdl_yazdir_satir();
    halt();
}
