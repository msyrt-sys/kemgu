/*
 * MİLESTONE D testi (aarch64) — CRASH-GÜVENLİ FS (WAL journaling + inode-FS SENTEZİ).
 * =============================================================================
 *
 * D-206 (fs_journal_arm.c — WAL commit-flag protokolu) + D-210 (minifs_arm.c —
 * superblock+inode+blok-bitmap) SENTEZİ: ATOMİK dosya-yazimi olan crash-guvenli
 * dosya sistemi. Bir dosya yazarken once JOURNAL'a (inode-guncellemesi + veri-
 * bloklari + commit-flag) yaz, SONRA gercek FS bloklarina UYGULA. Crash yazim
 * ORTASINDA olursa (commit=0) kurtarma journal'i ATLAR → FS eski-tutarli;
 * crash commit SONRASI (flag=1) olursa kurtarma journal'i FS'e REPLAY eder →
 * yeni-tutarli. Yarim-yazilmis (torn) FS durumu ASLA gorunmez.
 *
 * Disk duzeni (512-baytlik bloklar):
 *   blok 0 = SUPERBLOCK:  [magic "MFS1"(4)][inode_sayisi(4)][blok_sayisi(4)]
 *                          [veri_baslangic(4)]  (little-endian u32)
 *   blok 1 = INODE-TABLO: her inode 32 bayt:
 *                          ad[16] + boyut(u32) + ilk_blok(u32) + blok_sayisi(u32).
 *                          512/32 = 16 inode.
 *   blok 2 = JOURNAL-META: [magic "JRNL"(4)][hedef_inode_slot(4)][ad[16]]
 *                          [boyut(u32)][ilk_blok(u32)][blok_sayisi(u32)]
 *                          [commit(1)@511]  — WAL commit-flag protokolu (D-206).
 *   blok 3 = JOURNAL-VERİ: dosya icerigi (tek-blok, <=512 bayt) journal kopyasi.
 *   blok 4+ = FS VERİ bloklari (veri_baslangic = 4).
 *
 * ATOMİK YAZIM PROTOKOLU (crashfs_yaz):
 *   (a) journal-veri blogua dosya icerigini yaz + flush.
 *   (b) journal-meta yaz: [magic][hedef_slot][ad][boyut][ilk_blok][blok_say]
 *       [commit=0] + flush  → journal hazir ama HENUZ gecerli degil.
 *   (c) journal-meta commit=1 + flush  → journal artik GECERLI (replay-edilebilir).
 *   --- BU NOKTADAN SONRA dosya diske "kaydedilmis" sayilir (durability siniri) ---
 *   (d) FS'e UYGULA: journal-veri → FS veri-blogu; inode kaydini inode-tablo'ya
 *       yaz; superblock inode_sayisi guncelle + flush.
 *   (e) journal-meta temizle (commit=0) + flush → journal artik gereksiz.
 *
 *   crash_simule != 0 ise (d) ve (e) ATLANIR → journal commit=1 kalir ama FS'e
 *   hic uygulanmadi (crash: durability sonrasi, uygulama oncesi).
 *
 * KURTARMA PROTOKOLU (crashfs_kurtar) — her boot basinda:
 *   journal-meta oku → magic "JRNL" & commit==1 mi?
 *     EVET → journal-veri'yi FS veri-blogua REPLAY et; inode kaydini inode-
 *            tablo'ya yaz; superblock inode_sayisi guncelle; journal temizle.
 *     HAYIR (commit=0 / magic yok) → yapacak is yok (FS eski-tutarli).
 *
 * Senaryo (tek-boot, deterministik):
 *   (1) TEMİZ yazim: dosya "A" (a→e tam) → FS'te "A" tutarli + journal temiz.
 *       kurtarma cagrilir → no-op (commit=0). "A" okunabilir.
 *   (2) CRASH-replay: dosya "B" journal→commit=1 AMA uygula ATLA (crash sim).
 *       Crash-oncesi FS'te "B" YOK (uygulanmadi). Kurtarma commit=1 gorur →
 *       "B"yi FS'e replay → FS'te "B" tutarli + okunabilir.
 *   (3) CRASH-oncesi (bonus): dosya "C" journal-veri+meta yazilir AMA commit=0
 *       (crash commit ONCESI). Crash-oncesi FS'te "C" YOK. Kurtarma commit=0
 *       gorur → ATLAR → FS DEGISMEDI ("C" hala yok, torn durum gorunmez).
 *
 * Kanit: uc senaryo da gecerse "CRASHFS OK". Baslangic: "CRASHFS BASLA".
 * Marker: "CRASHFS OK".
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
#define INODE_SEKTOR     1UL     /* inode-tablo */
#define JMETA_SEKTOR     2UL     /* journal-meta (commit-flag) */
#define JVERI_SEKTOR     3UL     /* journal-veri (dosya icerigi kopyasi) */
#define VERI_BASLANGIC   4U      /* ilk FS veri blogu numarasi */
#define TOPLAM_BLOK      64U     /* diskin toplam blok sayisi (disk.img boyutu) */

#define INODE_BOYUT      32      /* her inode kaydinin bayt boyutu (hizali) */
#define AD_UZUN          16      /* dosya adi maks bayt (NUL-dolgu) */
#define MAKS_INODE       (BLOK_BOYUT / INODE_BOYUT)   /* 512/32 = 16 */
#define COMMIT_OFS       511     /* journal-meta commit bayragi son baytta */

/* Superblock magic "MFS1", journal magic "JRNL" (byte-byte, endianness-bagimsiz). */
static const uint8_t SB_MAGIC[4]   = { 'M', 'F', 'S', '1' };
static const uint8_t JRNL_MAGIC[4] = { 'J', 'R', 'N', 'L' };

static uint8_t sbtampon[BLOK_BOYUT];     /* superblock tamponu */
static uint8_t intampon[BLOK_BOYUT];     /* inode-tablo tamponu */
static uint8_t jmtampon[BLOK_BOYUT];     /* journal-meta tamponu */
static uint8_t jvtampon[BLOK_BOYUT];     /* journal-veri tamponu */
static uint8_t bloktampon[BLOK_BOYUT];   /* FS veri okuma/yazma tamponu */

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
/* iki bayt-dizisi ilk n bayti esit mi? */
static int bayt_esit(const uint8_t *a, const uint8_t *b, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) if (a[i] != b[i]) return 0;
    return 1;
}

/* --- BİÇİMLEME: superblock + bos inode-tablo + temiz journal --- */
static int crashfs_bicimle(uint64_t base) {
    /* superblock */
    for (int i = 0; i < BLOK_BOYUT; i++) sbtampon[i] = 0;
    sbtampon[0] = SB_MAGIC[0]; sbtampon[1] = SB_MAGIC[1];
    sbtampon[2] = SB_MAGIC[2]; sbtampon[3] = SB_MAGIC[3];
    u32_yaz(&sbtampon[4], 0);                 /* inode_sayisi = 0 */
    u32_yaz(&sbtampon[8], TOPLAM_BLOK);       /* blok_sayisi */
    u32_yaz(&sbtampon[12], VERI_BASLANGIC);   /* veri_baslangic */
    if (kdl_virtio_blk_yaz(base, SB_SEKTOR, sbtampon) != 0) return -1;

    /* bos inode-tablo (tumu 0 = ad[0]==0 → kullanilmayan slot) */
    for (int i = 0; i < BLOK_BOYUT; i++) intampon[i] = 0;
    if (kdl_virtio_blk_yaz(base, INODE_SEKTOR, intampon) != 0) return -2;

    /* temiz journal-meta (magic yok, commit=0) */
    for (int i = 0; i < BLOK_BOYUT; i++) jmtampon[i] = 0;
    if (kdl_virtio_blk_yaz(base, JMETA_SEKTOR, jmtampon) != 0) return -3;
    return 0;
}

/* --- FS'E UYGULA (ortak: hem crashfs_yaz (d) hem crashfs_kurtar replay) ---
 * journal-veri'yi FS veri-blogua yaz, inode kaydini inode-tablo'ya yaz,
 * superblock inode_sayisi guncelle. jmtampon'un guncel (commit=1, journal-meta
 * diskten okunmus) oldugu varsayilir. Donus 0 = ok, negatif = disk hatasi. */
static int crashfs_uygula(uint64_t base) {
    uint32_t hedef_slot = u32_oku(&jmtampon[4]);
    uint32_t boyut      = u32_oku(&jmtampon[24]);
    uint32_t ilk_blok   = u32_oku(&jmtampon[28]);
    uint32_t blok_say   = u32_oku(&jmtampon[32]);
    if (hedef_slot >= MAKS_INODE) return -1;
    if (blok_say != 1) return -2;   /* v1: tek-blok dosya (<=512 bayt) */

    /* journal-veri → FS veri-blogu (ilk_blok). */
    if (kdl_virtio_blk_oku(base, JVERI_SEKTOR, jvtampon) != 0) return -3;
    if (kdl_virtio_blk_yaz(base, (uint64_t)ilk_blok, jvtampon) != 0) return -4;

    /* inode kaydini inode-tablo'ya yaz (ad + boyut + ilk_blok + blok_say). */
    if (kdl_virtio_blk_oku(base, INODE_SEKTOR, intampon) != 0) return -5;
    uint8_t *ie = &intampon[hedef_slot * INODE_BOYUT];
    for (int i = 0; i < INODE_BOYUT; i++) ie[i] = 0;
    for (int i = 0; i < AD_UZUN; i++) ie[i] = jmtampon[8 + i];  /* ad journal'dan */
    u32_yaz(&ie[16], boyut);
    u32_yaz(&ie[20], ilk_blok);
    u32_yaz(&ie[24], blok_say);
    if (kdl_virtio_blk_yaz(base, INODE_SEKTOR, intampon) != 0) return -6;

    /* superblock inode_sayisi guncelle: hedef_slot yeni bir slotsa (== mevcut
     * inode_sayisi) su-hattini bir artir. Idempotent: replay iki kez de calissa
     * ayni sonuc (slot indeksi degismez, inode_sayisi ancak yeni-slotta artar). */
    if (kdl_virtio_blk_oku(base, SB_SEKTOR, sbtampon) != 0) return -7;
    uint32_t inode_say = u32_oku(&sbtampon[4]);
    if (hedef_slot >= inode_say) {
        u32_yaz(&sbtampon[4], hedef_slot + 1);
        if (kdl_virtio_blk_yaz(base, SB_SEKTOR, sbtampon) != 0) return -8;
    }
    return 0;
}

/* --- ATOMİK DOSYA YAZIM (WAL) ---
 * crash_evre: 0 = tam (a→e); 1 = commit SONRASI crash (a→c, uygula/temizle ATLA);
 *             2 = commit ONCESI crash (a→b, commit=1 ATLA — journal-meta commit=0).
 * hedef_slot: yeni inode slotu (bu testte cagirici saglar). ilk_blok: FS veri blogu.
 * Donus 0 = ok, negatif = hata. */
static int crashfs_yaz(uint64_t base, const char *ad, const uint8_t *veri,
                       uint32_t uzun, uint32_t hedef_slot, uint32_t ilk_blok,
                       int crash_evre) {
    if (uzun > BLOK_BOYUT) return -10;   /* v1: tek-blok dosya */

    /* (a) journal-veri blogua dosya icerigini yaz + flush. */
    for (int i = 0; i < BLOK_BOYUT; i++) jvtampon[i] = 0;
    for (uint32_t i = 0; i < uzun; i++) jvtampon[i] = veri[i];
    if (kdl_virtio_blk_yaz(base, JVERI_SEKTOR, jvtampon) != 0) return -1;

    /* (b) journal-meta hazirla: [magic][hedef_slot][ad][boyut][ilk_blok][blok_say]
     *     [commit=0] + flush. */
    for (int i = 0; i < BLOK_BOYUT; i++) jmtampon[i] = 0;
    jmtampon[0] = JRNL_MAGIC[0]; jmtampon[1] = JRNL_MAGIC[1];
    jmtampon[2] = JRNL_MAGIC[2]; jmtampon[3] = JRNL_MAGIC[3];
    u32_yaz(&jmtampon[4], hedef_slot);
    uint32_t alen = str_uzun(ad);
    if (alen > AD_UZUN) alen = AD_UZUN;
    for (uint32_t i = 0; i < alen; i++) jmtampon[8 + i] = (uint8_t)ad[i];  /* kalan NUL */
    u32_yaz(&jmtampon[24], uzun);       /* boyut */
    u32_yaz(&jmtampon[28], ilk_blok);   /* ilk_blok */
    u32_yaz(&jmtampon[32], 1);          /* blok_say (v1: tek blok) */
    jmtampon[COMMIT_OFS] = 0;           /* henuz commit degil */
    if (kdl_virtio_blk_yaz(base, JMETA_SEKTOR, jmtampon) != 0) return -2;

    /* crash_evre==2: commit ONCESI crash → burada dur (journal-meta commit=0). */
    if (crash_evre == 2) return 0;

    /* (c) journal-meta commit=1 + flush → journal GECERLI (replay-edilebilir). */
    jmtampon[COMMIT_OFS] = 1;
    if (kdl_virtio_blk_yaz(base, JMETA_SEKTOR, jmtampon) != 0) return -3;

    /* crash_evre==1: commit SONRASI crash → uygula (d)+temizle (e) ATLA. */
    if (crash_evre == 1) return 0;

    /* (d) FS'e UYGULA. */
    if (crashfs_uygula(base) != 0) return -4;

    /* (e) journal-meta temizle (commit=0) + flush → journal gereksiz. */
    jmtampon[COMMIT_OFS] = 0;
    if (kdl_virtio_blk_yaz(base, JMETA_SEKTOR, jmtampon) != 0) return -5;
    return 0;
}

/* --- KURTARMA (her boot basinda) ---
 * journal-meta oku → magic gecerli & commit==1 → FS'e replay (uygula) + journal
 * temizle. Donus: 1 = replay yapildi, 0 = yapacak is yok, negatif = disk hatasi. */
static int crashfs_kurtar(uint64_t base) {
    if (kdl_virtio_blk_oku(base, JMETA_SEKTOR, jmtampon) != 0) return -1;

    int magic_ok = 1;
    for (int i = 0; i < 4; i++) if (jmtampon[i] != JRNL_MAGIC[i]) { magic_ok = 0; break; }
    if (!magic_ok) return 0;                 /* journal yok/gecersiz → is yok */
    if (jmtampon[COMMIT_OFS] != 1) return 0; /* commit edilmemis → is yok (torn atla) */

    /* Journal GECERLI + commit=1 → FS'e replay. */
    if (crashfs_uygula(base) != 0) return -2;

    /* Replay tamam → journal temizle (commit=0). */
    jmtampon[COMMIT_OFS] = 0;
    if (kdl_virtio_blk_yaz(base, JMETA_SEKTOR, jmtampon) != 0) return -3;
    return 1;                                /* replay yapildi */
}

/* --- DOSYA OKU --- ad ile inode bul (FS'te), veri blogunu oku, tampona kopyala.
 * Donus: boyut (>=0) veya negatif hata (-5 = bulunamadi). */
static int64_t crashfs_oku(uint64_t base, const char *ad, uint8_t *tampon) {
    if (kdl_virtio_blk_oku(base, SB_SEKTOR, sbtampon) != 0) return -1;
    for (int i = 0; i < 4; i++) if (sbtampon[i] != SB_MAGIC[i]) return -2;
    uint32_t inode_say = u32_oku(&sbtampon[4]);

    if (kdl_virtio_blk_oku(base, INODE_SEKTOR, intampon) != 0) return -3;
    for (uint32_t s = 0; s < inode_say; s++) {
        const uint8_t *ie = &intampon[s * INODE_BOYUT];
        if (ie[0] == 0) continue;   /* bos slot */
        if (!ad_esit(ie, ad)) continue;
        uint32_t boyut    = u32_oku(&ie[16]);
        uint32_t ilk_blok = u32_oku(&ie[20]);
        if (kdl_virtio_blk_oku(base, (uint64_t)ilk_blok, bloktampon) != 0) return -4;
        for (uint32_t i = 0; i < BLOK_BOYUT && i < boyut; i++) tampon[i] = bloktampon[i];
        return (int64_t)boyut;
    }
    return -5;   /* bulunamadi */
}

/* Okuma tamponu (dosya icerigi geri okunur). */
static uint8_t oku_tampon[BLOK_BOYUT];

int main(void) {
    kdl_yazdir_metin("CRASHFS BASLA");
    kdl_yazdir_satir();

    uint64_t base = kdl_virtio_blk_bul();
    if (!base) { kdl_yazdir_metin("DISK YOK"); kdl_yazdir_satir(); halt(); }
    if (kdl_virtio_blk_kur(base) != 0) { kdl_yazdir_metin("KUR HATA"); kdl_yazdir_satir(); halt(); }

    int tum_ok = 1;

    /* --- BİÇİMLE --- */
    if (crashfs_bicimle(base) != 0) { kdl_yazdir_metin("BICIMLE HATA"); kdl_yazdir_satir(); halt(); }

    /* ============ SENARYO 1: TEMİZ YAZIM ============
     * dosya "A" tam WAL (a→e) → FS'te tutarli + journal temiz. Kurtarma no-op. */
    const char *A_ICERIK = "icerik-A-temiz";   /* 14 bayt */
    const uint32_t A_UZUN = 14;
    /* hedef_slot=0 (ilk inode), ilk_blok=VERI_BASLANGIC (blok 4). */
    if (crashfs_yaz(base, "A", (const uint8_t *)A_ICERIK, A_UZUN,
                    0, VERI_BASLANGIC, /*crash_evre=*/0) != 0) {
        kdl_yazdir_metin("S1 YAZ HATA"); kdl_yazdir_satir(); halt();
    }
    /* Kurtarma cagir: journal temiz (commit=0) → no-op (donus 0). */
    int k1 = crashfs_kurtar(base);
    for (int i = 0; i < BLOK_BOYUT; i++) oku_tampon[i] = 0xEE;
    int64_t a_boyut = crashfs_oku(base, "A", oku_tampon);
    kdl_yaz_metin("S1 temiz: A-boyut=");
    kdl_yaz_onaltilik((uint64_t)(uint32_t)a_boyut);
    kdl_yaz_metin(" kurtarma=");
    kdl_yaz_onaltilik((uint64_t)(uint32_t)k1);
    kdl_yazdir_satir();
    /* Beklenti: A okunur (boyut=14, icerik eslesir), kurtarma=0 (journal temizdi). */
    if (k1 != 0 || a_boyut != (int64_t)A_UZUN ||
        !bayt_esit(oku_tampon, (const uint8_t *)A_ICERIK, A_UZUN)) {
        tum_ok = 0; kdl_yazdir_metin("S1 FAIL"); kdl_yazdir_satir();
    }

    /* ============ SENARYO 2: CRASH-REPLAY ============
     * dosya "B" journal→commit=1 AMA uygula ATLA (crash: durability sonrasi,
     * uygulama oncesi). Crash-oncesi FS'te "B" YOK. Kurtarma commit=1 → replay. */
    const char *B_ICERIK = "icerik-B-replay!";  /* 16 bayt */
    const uint32_t B_UZUN = 16;
    /* hedef_slot=1 (ikinci inode), ilk_blok=VERI_BASLANGIC+1 (blok 5). */
    if (crashfs_yaz(base, "B", (const uint8_t *)B_ICERIK, B_UZUN,
                    1, VERI_BASLANGIC + 1, /*crash_evre=*/1) != 0) {
        kdl_yazdir_metin("S2 YAZ HATA"); kdl_yazdir_satir(); halt();
    }
    /* Crash-oncesi: FS'te "B" HENUZ yok (uygulama atlandi). */
    for (int i = 0; i < BLOK_BOYUT; i++) oku_tampon[i] = 0xEE;
    int64_t b_oncesi = crashfs_oku(base, "B", oku_tampon);
    /* Kurtarma cagir: journal-meta commit=1 → "B"yi FS'e replay. */
    int k2 = crashfs_kurtar(base);
    for (int i = 0; i < BLOK_BOYUT; i++) oku_tampon[i] = 0xEE;
    int64_t b_sonrasi = crashfs_oku(base, "B", oku_tampon);
    kdl_yaz_metin("S2 crash-oncesi B-oku=");
    kdl_yaz_onaltilik((uint64_t)(uint32_t)b_oncesi);
    kdl_yaz_metin(" kurtarma=");
    kdl_yaz_onaltilik((uint64_t)(uint32_t)k2);
    kdl_yaz_metin(" replay-sonrasi=");
    kdl_yaz_onaltilik((uint64_t)(uint32_t)b_sonrasi);
    kdl_yazdir_satir();
    /* Beklenti: crash-oncesi "B" bulunamaz (<0), kurtarma=1 (replay yapildi),
     * replay-sonrasi B okunur (boyut=16, icerik eslesir). */
    if (b_oncesi >= 0 || k2 != 1 || b_sonrasi != (int64_t)B_UZUN ||
        !bayt_esit(oku_tampon, (const uint8_t *)B_ICERIK, B_UZUN)) {
        tum_ok = 0; kdl_yazdir_metin("S2 FAIL"); kdl_yazdir_satir();
    }
    /* "A" hala saglam (replay komsu dosyayi bozmadi). */
    for (int i = 0; i < BLOK_BOYUT; i++) oku_tampon[i] = 0xEE;
    int64_t a_hala = crashfs_oku(base, "A", oku_tampon);
    if (a_hala != (int64_t)A_UZUN ||
        !bayt_esit(oku_tampon, (const uint8_t *)A_ICERIK, A_UZUN)) {
        tum_ok = 0; kdl_yazdir_metin("S2 A-bozuldu FAIL"); kdl_yazdir_satir();
    }

    /* ============ SENARYO 3: CRASH-ÖNCESİ (torn atla) ============
     * dosya "C" journal-veri+meta yazilir AMA commit=0 (crash commit ONCESI).
     * Kurtarma commit=0 gorur → ATLAR → FS DEGISMEDI ("C" hala yok). */
    const char *C_ICERIK = "icerik-C-torn";   /* 13 bayt */
    const uint32_t C_UZUN = 13;
    /* hedef_slot=2 (ucuncu inode), ilk_blok=VERI_BASLANGIC+2 (blok 6). */
    if (crashfs_yaz(base, "C", (const uint8_t *)C_ICERIK, C_UZUN,
                    2, VERI_BASLANGIC + 2, /*crash_evre=*/2) != 0) {
        kdl_yazdir_metin("S3 YAZ HATA"); kdl_yazdir_satir(); halt();
    }
    /* Crash-oncesi: FS'te "C" yok (commit=0, uygulanmadi). */
    for (int i = 0; i < BLOK_BOYUT; i++) oku_tampon[i] = 0xEE;
    int64_t c_oncesi = crashfs_oku(base, "C", oku_tampon);
    /* Kurtarma cagir: commit=0 → ATLA (no-op). */
    int k3 = crashfs_kurtar(base);
    for (int i = 0; i < BLOK_BOYUT; i++) oku_tampon[i] = 0xEE;
    int64_t c_sonrasi = crashfs_oku(base, "C", oku_tampon);
    kdl_yaz_metin("S3 crash-oncesi C-oku=");
    kdl_yaz_onaltilik((uint64_t)(uint32_t)c_oncesi);
    kdl_yaz_metin(" kurtarma=");
    kdl_yaz_onaltilik((uint64_t)(uint32_t)k3);
    kdl_yaz_metin(" sonrasi=");
    kdl_yaz_onaltilik((uint64_t)(uint32_t)c_sonrasi);
    kdl_yazdir_satir();
    /* Beklenti: crash-oncesi "C" yok (<0), kurtarma=0 (torn atlandi),
     * kurtarma-sonrasi "C" HALA yok (<0) → torn durum asla gorunmedi. */
    if (c_oncesi >= 0 || k3 != 0 || c_sonrasi >= 0) {
        tum_ok = 0; kdl_yazdir_metin("S3 FAIL"); kdl_yazdir_satir();
    }
    /* "A" ve "B" hala saglam (torn-atla FS'i degistirmedi). */
    for (int i = 0; i < BLOK_BOYUT; i++) oku_tampon[i] = 0xEE;
    int64_t a_son = crashfs_oku(base, "A", oku_tampon);
    int a_son_ok = (a_son == (int64_t)A_UZUN &&
                    bayt_esit(oku_tampon, (const uint8_t *)A_ICERIK, A_UZUN));
    for (int i = 0; i < BLOK_BOYUT; i++) oku_tampon[i] = 0xEE;
    int64_t b_son = crashfs_oku(base, "B", oku_tampon);
    int b_son_ok = (b_son == (int64_t)B_UZUN &&
                    bayt_esit(oku_tampon, (const uint8_t *)B_ICERIK, B_UZUN));
    if (!a_son_ok || !b_son_ok) {
        tum_ok = 0; kdl_yazdir_metin("S3 A/B-bozuldu FAIL"); kdl_yazdir_satir();
    }

    kdl_yazdir_metin(tum_ok ? "CRASHFS OK" : "CRASHFS HATA");
    kdl_yazdir_satir();
    halt();
}
