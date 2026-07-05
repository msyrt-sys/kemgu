/*
 * MİLESTONE D testi (aarch64) — MİNİ DOSYA SİSTEMİ TAM CRUD + BLOK GERİ-KAZANIM.
 * =============================================================================
 *
 * D-210 (minifs_arm.c — superblock+inode+blok-bitmap) uzerine: TAM CRUD.
 * D-210 sadece Create + Read yapiyordu. Bu test Update + DELETE ekliyor ve
 * KRITIK kaniti gosteriyor: silinen bir dosyanin bloklari bitmap'te SERBEST
 * birakildiktan sonra YENI olusturulan bir dosya tarafindan GERI KULLANILIR
 * (bitmap-reclaim). Silme gercekten blok geri-kazanimi yapmazsa (yani sadece
 * inode'u bosaltip bloklari dolu birakirsa) reclaim kaniti BASARISIZ olur.
 *
 * Disk duzeni (D-210 ile ayni — 512-baytlik bloklar):
 *   blok 0  = SUPERBLOCK: [magic "MFS1"(4)][inode_sayisi(4)][blok_sayisi(4)]
 *                         [veri_baslangic(4)]  (little-endian u32)
 *   blok 1  = BLOK-BİTMAP: her veri blogu icin 1 bayt (0=bos, 1=dolu).
 *   blok 2  = INODE-TABLO: her inode 32 bayt:
 *                          ad[16] + boyut(u32) + ilk_blok(u32) + blok_sayisi(u32).
 *                          512/32 = 16 inode.
 *   blok 3+ = VERİ bloklari (veri_baslangic = 3).
 *
 * NOT: inode_sayisi burada "slot ust siniri"dir (yuksek-water-mark). Silme
 * inode slotunu ad[0]=0 yaparak bosaltir ama inode_sayisi'ni azaltmaz —
 * boylece slot indeksleri kararli kalir, bos slot atlanir (ad[0]==0 kontrolu).
 *
 * İşlemler:
 *   mfs_bicimle()             — superblock + bos bitmap + bos inode-tablo.
 *   mfs_olustur(ad,veri,uzun) — bitmap'ten ardisik blok ayir, veri yaz, inode doldur.
 *   mfs_oku(ad,tampon)        — inode bul, veri bloklarini sirayla oku.
 *   mfs_sil(ad)               — inode bul, veri bloklarini bitmap'te SERBEST birak
 *                               (0 yaz), inode slotunu bosalt (ad[0]=0), flush.
 *   mfs_guncelle(ad,veri,uzun)— var olan dosyayi yeniden yaz. Yeni boyut ayni
 *                               veya daha kucuk blok gerektiriyorsa yerinde yazilir;
 *                               farkli blok-sayisi gerekirse sil+olustur.
 *
 * Senaryo (tek-boot, deterministik):
 *   1) mfs_bicimle().
 *   2) 3 dosya olustur — hepsi tek-bloklu (ardisik ilk-uygun tahsis):
 *        "alfa"  → blok 3
 *        "beta"  → blok 4   (ORTADAKI — silinecek)
 *        "gama"  → blok 5
 *      bitmap: 3,4,5 dolu; 6 bos.
 *   3) Round-trip: 3 dosya da geri okunur, icerik eslesir.
 *   4) mfs_guncelle("alfa", ...) — ayni-blok yerinde guncelleme; geri oku eslesir.
 *   5) mfs_sil("beta") — blok 4 SERBEST birakilir. bitmap: 3,5 dolu; 4 bos.
 *      "beta" artik okunamaz (bulunamadi).
 *   6) mfs_olustur("delta", ...) tek-bloklu — ilk-uygun tahsis SERBEST blok 4'u
 *      GERI KULLANIR (ayni blok numarasi 4). bitmap: 3,4,5 dolu.
 *   7) "delta" geri oku → icerik round-trip eslesir + ilk_blok == 4 (reclaim!).
 *
 * Kanit: reclaim edilen blok numarasi (4) == silinen dosyanin blogu +
 *        icerik round-trip + guncelleme + silinen dosya okunamaz → "MINIFS CRUD OK".
 * Baslangic: "MINIFS CRUD BASLA". Marker: "MINIFS CRUD OK".
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
    for (int i = 0; i < BLOK_BOYUT; i++) sbtampon[i] = 0;
    sbtampon[0] = SB_MAGIC[0]; sbtampon[1] = SB_MAGIC[1];
    sbtampon[2] = SB_MAGIC[2]; sbtampon[3] = SB_MAGIC[3];
    u32_yaz(&sbtampon[4], 0);                 /* inode_sayisi = 0 */
    u32_yaz(&sbtampon[8], TOPLAM_BLOK);       /* blok_sayisi */
    u32_yaz(&sbtampon[12], VERI_BASLANGIC);   /* veri_baslangic */
    if (kdl_virtio_blk_yaz(base, SB_SEKTOR, sbtampon) != 0) return -1;

    for (int i = 0; i < BLOK_BOYUT; i++) bmtampon[i] = 0;
    if (kdl_virtio_blk_yaz(base, BITMAP_SEKTOR, bmtampon) != 0) return -2;

    for (int i = 0; i < BLOK_BOYUT; i++) intampon[i] = 0;
    if (kdl_virtio_blk_yaz(base, INODE_SEKTOR, intampon) != 0) return -3;
    return 0;
}

/* Bitmap'te ardisik `n` bos blok bul (ilk-uygun), dolu isaretle, ilk blok
 * numarasini dondur. bmtampon guncel (diskten okunmus) varsayilir; caller flush.
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

/* Bitmap'te `ilk_blok` (mutlak) baslayan `n` blogu SERBEST birak (0 yaz).
 * bmtampon guncel varsayilir; caller flush. */
static void bitmap_serbest(uint32_t ilk_blok, uint32_t n) {
    for (uint32_t b = 0; b < n; b++) {
        uint32_t blok = ilk_blok + b;
        if (blok < VERI_BASLANGIC) continue;
        uint32_t idx = blok - VERI_BASLANGIC;
        if (idx >= VERI_BLOK_SAYISI) continue;
        bmtampon[idx] = 0;   /* serbest */
    }
}

/* --- DOSYA OLUŞTUR --- ad + veri(uzun bayt) → bitmap ayir, veriyi bloklara yaz,
 * inode kaydi doldur. Donus: 0 = ok, negatif = hata. */
static int mfs_olustur(uint64_t base, const char *ad, const uint8_t *veri, uint32_t uzun) {
    uint32_t blok_say = (uzun + BLOK_BOYUT - 1) / BLOK_BOYUT;
    if (blok_say == 0) blok_say = 1;   /* bos dosya bile 1 blok tutar */

    if (kdl_virtio_blk_oku(base, SB_SEKTOR, sbtampon) != 0) return -1;
    uint32_t inode_say = u32_oku(&sbtampon[4]);   /* yuksek-water-mark */

    /* Once mevcut bos slot ara (silme sonrasi geri-kazanilan slotlar). */
    if (kdl_virtio_blk_oku(base, INODE_SEKTOR, intampon) != 0) return -7;
    uint32_t slot = MAKS_INODE;
    for (uint32_t s = 0; s < inode_say; s++) {
        if (intampon[s * INODE_BOYUT] == 0) { slot = s; break; }
    }
    int yeni_slot = 0;
    if (slot == MAKS_INODE) {
        /* bos slot yok — su-hattini bir artir. */
        if (inode_say >= MAKS_INODE) return -2;   /* inode-tablo dolu */
        slot = inode_say;
        yeni_slot = 1;
    }

    if (kdl_virtio_blk_oku(base, BITMAP_SEKTOR, bmtampon) != 0) return -3;
    uint32_t ilk_blok = bitmap_ayir(blok_say);
    if (ilk_blok == 0) return -4;             /* disk dolu */

    for (uint32_t b = 0; b < blok_say; b++) {
        for (int i = 0; i < BLOK_BOYUT; i++) bloktampon[i] = 0;
        uint32_t ofs = b * BLOK_BOYUT;
        for (uint32_t i = 0; i < BLOK_BOYUT && ofs + i < uzun; i++) {
            bloktampon[i] = veri[ofs + i];
        }
        if (kdl_virtio_blk_yaz(base, (uint64_t)(ilk_blok + b), bloktampon) != 0) return -5;
    }

    if (kdl_virtio_blk_yaz(base, BITMAP_SEKTOR, bmtampon) != 0) return -6;

    uint8_t *ie = &intampon[slot * INODE_BOYUT];
    for (int i = 0; i < INODE_BOYUT; i++) ie[i] = 0;
    uint32_t alen = str_uzun(ad);
    if (alen > AD_UZUN) alen = AD_UZUN;
    for (uint32_t i = 0; i < alen; i++) ie[i] = (uint8_t)ad[i];  /* kalan NUL */
    u32_yaz(&ie[16], uzun);         /* boyut */
    u32_yaz(&ie[20], ilk_blok);     /* ilk_blok */
    u32_yaz(&ie[24], blok_say);     /* blok_sayisi */
    if (kdl_virtio_blk_yaz(base, INODE_SEKTOR, intampon) != 0) return -8;

    if (yeni_slot) {
        u32_yaz(&sbtampon[4], inode_say + 1);   /* su-hattini artir */
        if (kdl_virtio_blk_yaz(base, SB_SEKTOR, sbtampon) != 0) return -9;
    }
    return 0;
}

/* --- DOSYA OKU --- ad ile inode bul, veri bloklarini oku, tampona kopyala.
 * `tampon` en az inode'un boyutu kadar olmali. Donus: boyut (>=0) veya negatif. */
static int64_t mfs_oku(uint64_t base, const char *ad, uint8_t *tampon) {
    if (kdl_virtio_blk_oku(base, SB_SEKTOR, sbtampon) != 0) return -1;
    for (int i = 0; i < 4; i++) if (sbtampon[i] != SB_MAGIC[i]) return -2;
    uint32_t inode_say = u32_oku(&sbtampon[4]);

    if (kdl_virtio_blk_oku(base, INODE_SEKTOR, intampon) != 0) return -3;
    for (uint32_t s = 0; s < inode_say; s++) {
        const uint8_t *ie = &intampon[s * INODE_BOYUT];
        if (ie[0] == 0) continue;   /* bos slot */
        if (!ad_esit(ie, ad)) continue;
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

/* --- DOSYA SİL --- ad ile inode bul → veri bloklarini bitmap'te SERBEST birak,
 * inode slotunu bosalt (ad[0]=0). Donus: silinen dosyanin ilk_blok'u (>0) veya
 * negatif hata. Silme inode_sayisi'ni (su-hatti) azaltmaz — slot geri-kazanilir. */
static int64_t mfs_sil(uint64_t base, const char *ad) {
    if (kdl_virtio_blk_oku(base, SB_SEKTOR, sbtampon) != 0) return -1;
    for (int i = 0; i < 4; i++) if (sbtampon[i] != SB_MAGIC[i]) return -2;
    uint32_t inode_say = u32_oku(&sbtampon[4]);

    if (kdl_virtio_blk_oku(base, INODE_SEKTOR, intampon) != 0) return -3;
    for (uint32_t s = 0; s < inode_say; s++) {
        uint8_t *ie = &intampon[s * INODE_BOYUT];
        if (ie[0] == 0) continue;
        if (!ad_esit(ie, ad)) continue;
        uint32_t ilk_blok = u32_oku(&ie[20]);
        uint32_t blok_say = u32_oku(&ie[24]);

        /* 1) bloklari bitmap'te serbest birak + flush. */
        if (kdl_virtio_blk_oku(base, BITMAP_SEKTOR, bmtampon) != 0) return -4;
        bitmap_serbest(ilk_blok, blok_say);
        if (kdl_virtio_blk_yaz(base, BITMAP_SEKTOR, bmtampon) != 0) return -5;

        /* 2) inode slotunu bosalt (ad[0]=0 → bos) + flush. */
        for (int i = 0; i < INODE_BOYUT; i++) ie[i] = 0;
        if (kdl_virtio_blk_yaz(base, INODE_SEKTOR, intampon) != 0) return -6;

        return (int64_t)ilk_blok;   /* serbest birakilan ilk blok */
    }
    return -7;   /* bulunamadi */
}

/* --- DOSYA GÜNCELLE --- var olan dosyayi yeniden yaz. Yeni boyut ayni sayida
 * blok gerektiriyorsa YERİNDE yazilir (ayni bloklar — blok numaralari degismez).
 * Farkli blok-sayisi gerekiyorsa sil+yeniden-olustur (bloklar degisebilir).
 * Donus: 0 = ok, negatif = hata. */
static int mfs_guncelle(uint64_t base, const char *ad, const uint8_t *veri, uint32_t uzun) {
    if (kdl_virtio_blk_oku(base, SB_SEKTOR, sbtampon) != 0) return -1;
    for (int i = 0; i < 4; i++) if (sbtampon[i] != SB_MAGIC[i]) return -2;
    uint32_t inode_say = u32_oku(&sbtampon[4]);

    if (kdl_virtio_blk_oku(base, INODE_SEKTOR, intampon) != 0) return -3;
    for (uint32_t s = 0; s < inode_say; s++) {
        uint8_t *ie = &intampon[s * INODE_BOYUT];
        if (ie[0] == 0) continue;
        if (!ad_esit(ie, ad)) continue;
        uint32_t ilk_blok = u32_oku(&ie[20]);
        uint32_t eski_blok_say = u32_oku(&ie[24]);
        uint32_t yeni_blok_say = (uzun + BLOK_BOYUT - 1) / BLOK_BOYUT;
        if (yeni_blok_say == 0) yeni_blok_say = 1;

        if (yeni_blok_say == eski_blok_say) {
            /* YERİNDE guncelle — ayni bloklar. */
            for (uint32_t b = 0; b < yeni_blok_say; b++) {
                for (int i = 0; i < BLOK_BOYUT; i++) bloktampon[i] = 0;
                uint32_t ofs = b * BLOK_BOYUT;
                for (uint32_t i = 0; i < BLOK_BOYUT && ofs + i < uzun; i++) {
                    bloktampon[i] = veri[ofs + i];
                }
                if (kdl_virtio_blk_yaz(base, (uint64_t)(ilk_blok + b), bloktampon) != 0) return -4;
            }
            /* inode boyutunu guncelle (blok_say ayni). */
            u32_yaz(&ie[16], uzun);
            if (kdl_virtio_blk_yaz(base, INODE_SEKTOR, intampon) != 0) return -5;
            return 0;
        }
        /* Blok-sayisi degisti → sil + yeniden olustur. */
        if (mfs_sil(base, ad) < 0) return -6;
        return mfs_olustur(base, ad, veri, uzun);
    }
    return -7;   /* bulunamadi */
}

/* Belirli bir mutlak blok numarasinin bitmap'te dolu olup olmadigini dondur.
 * bmtampon'u DİSKTEN yeniden okur (guncel durum). */
static int bitmap_blok_dolu(uint64_t base, uint32_t blok) {
    if (kdl_virtio_blk_oku(base, BITMAP_SEKTOR, bmtampon) != 0) return -1;
    if (blok < VERI_BASLANGIC) return 0;
    uint32_t idx = blok - VERI_BASLANGIC;
    if (idx >= VERI_BLOK_SAYISI) return 0;
    return bmtampon[idx] ? 1 : 0;
}

/* Bitmap'te dolu (=1) blok sayisini say (diskten okur). */
static int bitmap_dolu_say(uint64_t base) {
    if (kdl_virtio_blk_oku(base, BITMAP_SEKTOR, bmtampon) != 0) return -1;
    int say = 0;
    for (uint32_t i = 0; i < VERI_BLOK_SAYISI; i++) if (bmtampon[i]) say++;
    return say;
}

/* İki bayt-dizisi ilk `n` bayti esit mi? */
static int bayt_esit(const uint8_t *a, const uint8_t *b, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) if (a[i] != b[i]) return 0;
    return 1;
}

/* Bir dosyanin inode'undan ilk_blok'u dondur (kanit icin). Bulunamazsa -1. */
static int64_t mfs_ilk_blok(uint64_t base, const char *ad) {
    if (kdl_virtio_blk_oku(base, SB_SEKTOR, sbtampon) != 0) return -1;
    uint32_t inode_say = u32_oku(&sbtampon[4]);
    if (kdl_virtio_blk_oku(base, INODE_SEKTOR, intampon) != 0) return -1;
    for (uint32_t s = 0; s < inode_say; s++) {
        const uint8_t *ie = &intampon[s * INODE_BOYUT];
        if (ie[0] == 0) continue;
        if (!ad_esit(ie, ad)) continue;
        return (int64_t)u32_oku(&ie[20]);
    }
    return -1;
}

/* Okuma tamponlari. */
static uint8_t oku_tampon[BLOK_BOYUT];

int main(void) {
    kdl_yazdir_metin("MINIFS CRUD BASLA");
    kdl_yazdir_satir();

    uint64_t base = kdl_virtio_blk_bul();
    if (!base) { kdl_yazdir_metin("DISK YOK"); kdl_yazdir_satir(); halt(); }
    if (kdl_virtio_blk_kur(base) != 0) { kdl_yazdir_metin("KUR HATA"); kdl_yazdir_satir(); halt(); }

    int tum_ok = 1;

    /* --- 1) BİÇİMLE --- */
    if (mfs_bicimle(base) != 0) { kdl_yazdir_metin("BICIMLE HATA"); kdl_yazdir_satir(); halt(); }

    /* --- 2) 3 tek-bloklu dosya olustur: alfa→3, beta→4, gama→5 --- */
    const char *A_ICERIK = "alfa-icerik-1";   /* 13 bayt */
    const char *B_ICERIK = "beta-icerik-22";  /* 14 bayt */
    const char *G_ICERIK = "gama-icerik-333"; /* 15 bayt */
    const uint32_t A_UZUN = 13, B_UZUN = 14, G_UZUN = 15;
    if (mfs_olustur(base, "alfa", (const uint8_t *)A_ICERIK, A_UZUN) != 0) {
        kdl_yazdir_metin("OLUSTUR alfa HATA"); kdl_yazdir_satir(); halt();
    }
    if (mfs_olustur(base, "beta", (const uint8_t *)B_ICERIK, B_UZUN) != 0) {
        kdl_yazdir_metin("OLUSTUR beta HATA"); kdl_yazdir_satir(); halt();
    }
    if (mfs_olustur(base, "gama", (const uint8_t *)G_ICERIK, G_UZUN) != 0) {
        kdl_yazdir_metin("OLUSTUR gama HATA"); kdl_yazdir_satir(); halt();
    }

    /* Blok yerlesimi kaniti: alfa→3, beta→4, gama→5. */
    int64_t a_blok = mfs_ilk_blok(base, "alfa");
    int64_t b_blok = mfs_ilk_blok(base, "beta");
    int64_t g_blok = mfs_ilk_blok(base, "gama");
    kdl_yaz_metin("yerlesim: alfa=");
    kdl_yaz_onaltilik((uint64_t)(uint32_t)a_blok);
    kdl_yaz_metin(" beta=");
    kdl_yaz_onaltilik((uint64_t)(uint32_t)b_blok);
    kdl_yaz_metin(" gama=");
    kdl_yaz_onaltilik((uint64_t)(uint32_t)g_blok);
    kdl_yazdir_satir();
    if (a_blok != 3 || b_blok != 4 || g_blok != 5) {
        tum_ok = 0; kdl_yazdir_metin("yerlesim FAIL"); kdl_yazdir_satir();
    }

    /* bitmap: 3,4,5 dolu; 6 bos. */
    int dolu1 = bitmap_dolu_say(base);
    kdl_yaz_metin("bitmap-1 dolu=");
    kdl_yaz_onaltilik((uint64_t)(uint32_t)dolu1);
    kdl_yazdir_satir();
    if (dolu1 != 3 || !bitmap_blok_dolu(base, 3) || !bitmap_blok_dolu(base, 4) ||
        !bitmap_blok_dolu(base, 5) || bitmap_blok_dolu(base, 6)) {
        tum_ok = 0; kdl_yazdir_metin("bitmap-1 FAIL"); kdl_yazdir_satir();
    }

    /* --- 3) 3 dosya round-trip --- */
    for (int i = 0; i < BLOK_BOYUT; i++) oku_tampon[i] = 0xEE;
    int64_t rb = mfs_oku(base, "alfa", oku_tampon);
    if (rb != (int64_t)A_UZUN || !bayt_esit(oku_tampon, (const uint8_t *)A_ICERIK, A_UZUN)) {
        tum_ok = 0; kdl_yazdir_metin("alfa read FAIL"); kdl_yazdir_satir();
    }
    for (int i = 0; i < BLOK_BOYUT; i++) oku_tampon[i] = 0xEE;
    rb = mfs_oku(base, "beta", oku_tampon);
    if (rb != (int64_t)B_UZUN || !bayt_esit(oku_tampon, (const uint8_t *)B_ICERIK, B_UZUN)) {
        tum_ok = 0; kdl_yazdir_metin("beta read FAIL"); kdl_yazdir_satir();
    }
    for (int i = 0; i < BLOK_BOYUT; i++) oku_tampon[i] = 0xEE;
    rb = mfs_oku(base, "gama", oku_tampon);
    if (rb != (int64_t)G_UZUN || !bayt_esit(oku_tampon, (const uint8_t *)G_ICERIK, G_UZUN)) {
        tum_ok = 0; kdl_yazdir_metin("gama read FAIL"); kdl_yazdir_satir();
    }

    /* --- 4) UPDATE: "alfa" yerinde guncelle (ayni blok 3, ayni blok-sayisi) --- */
    const char *A2_ICERIK = "ALFA-YENI-99!";  /* 13 bayt (ayni uzunluk = ayni blok) */
    const uint32_t A2_UZUN = 13;
    if (mfs_guncelle(base, "alfa", (const uint8_t *)A2_ICERIK, A2_UZUN) != 0) {
        kdl_yazdir_metin("GUNCELLE alfa HATA"); kdl_yazdir_satir(); halt();
    }
    int64_t a_blok2 = mfs_ilk_blok(base, "alfa");
    for (int i = 0; i < BLOK_BOYUT; i++) oku_tampon[i] = 0xEE;
    rb = mfs_oku(base, "alfa", oku_tampon);
    kdl_yaz_metin("guncelle: alfa-blok=");
    kdl_yaz_onaltilik((uint64_t)(uint32_t)a_blok2);
    kdl_yazdir_satir();
    if (a_blok2 != 3 || rb != (int64_t)A2_UZUN ||
        !bayt_esit(oku_tampon, (const uint8_t *)A2_ICERIK, A2_UZUN)) {
        tum_ok = 0; kdl_yazdir_metin("guncelle FAIL"); kdl_yazdir_satir();
    }

    /* --- 5) DELETE: ORTADAKI "beta" sil → blok 4 SERBEST --- */
    int64_t serbest_blok = mfs_sil(base, "beta");
    kdl_yaz_metin("sil: beta serbest-blok=");
    kdl_yaz_onaltilik((uint64_t)(uint32_t)serbest_blok);
    kdl_yazdir_satir();
    if (serbest_blok != 4) {
        tum_ok = 0; kdl_yazdir_metin("sil-donus FAIL"); kdl_yazdir_satir();
    }
    /* bitmap: 3,5 dolu; 4 bos artik. */
    int dolu2 = bitmap_dolu_say(base);
    kdl_yaz_metin("bitmap-2 dolu=");
    kdl_yaz_onaltilik((uint64_t)(uint32_t)dolu2);
    kdl_yazdir_satir();
    if (dolu2 != 2 || !bitmap_blok_dolu(base, 3) || bitmap_blok_dolu(base, 4) ||
        !bitmap_blok_dolu(base, 5)) {
        tum_ok = 0; kdl_yazdir_metin("bitmap-2 FAIL"); kdl_yazdir_satir();
    }
    /* "beta" artik okunamaz. */
    int64_t beta_yok = mfs_oku(base, "beta", oku_tampon);
    if (beta_yok >= 0) {
        tum_ok = 0; kdl_yazdir_metin("beta-silinmedi FAIL"); kdl_yazdir_satir();
    }

    /* --- 6) RECLAIM: yeni "delta" olustur → SERBEST blok 4'u GERI KULLAN --- */
    const char *D_ICERIK = "delta-reclaim!!"; /* 15 bayt (tek blok) */
    const uint32_t D_UZUN = 15;
    if (mfs_olustur(base, "delta", (const uint8_t *)D_ICERIK, D_UZUN) != 0) {
        kdl_yazdir_metin("OLUSTUR delta HATA"); kdl_yazdir_satir(); halt();
    }
    int64_t d_blok = mfs_ilk_blok(base, "delta");
    kdl_yaz_metin("reclaim: delta-blok=");
    kdl_yaz_onaltilik((uint64_t)(uint32_t)d_blok);
    kdl_yazdir_satir();
    /* KRİTİK KANIT: delta serbest-birakilan blok 4'u geri kullandi mi? */
    if (d_blok != serbest_blok || d_blok != 4) {
        tum_ok = 0; kdl_yazdir_metin("RECLAIM FAIL"); kdl_yazdir_satir();
    }
    /* delta icerik round-trip. */
    for (int i = 0; i < BLOK_BOYUT; i++) oku_tampon[i] = 0xEE;
    rb = mfs_oku(base, "delta", oku_tampon);
    if (rb != (int64_t)D_UZUN || !bayt_esit(oku_tampon, (const uint8_t *)D_ICERIK, D_UZUN)) {
        tum_ok = 0; kdl_yazdir_metin("delta read FAIL"); kdl_yazdir_satir();
    }
    /* bitmap yeniden 3,4,5 dolu. */
    int dolu3 = bitmap_dolu_say(base);
    kdl_yaz_metin("bitmap-3 dolu=");
    kdl_yaz_onaltilik((uint64_t)(uint32_t)dolu3);
    kdl_yazdir_satir();
    if (dolu3 != 3 || !bitmap_blok_dolu(base, 3) || !bitmap_blok_dolu(base, 4) ||
        !bitmap_blok_dolu(base, 5)) {
        tum_ok = 0; kdl_yazdir_metin("bitmap-3 FAIL"); kdl_yazdir_satir();
    }
    /* gama hala saglam (silme/reclaim komsu dosyayi bozmadi). */
    for (int i = 0; i < BLOK_BOYUT; i++) oku_tampon[i] = 0xEE;
    rb = mfs_oku(base, "gama", oku_tampon);
    if (rb != (int64_t)G_UZUN || !bayt_esit(oku_tampon, (const uint8_t *)G_ICERIK, G_UZUN)) {
        tum_ok = 0; kdl_yazdir_metin("gama-bozuldu FAIL"); kdl_yazdir_satir();
    }

    kdl_yazdir_metin(tum_ok ? "MINIFS CRUD OK" : "MINIFS CRUD HATA");
    kdl_yazdir_satir();
    halt();
}
