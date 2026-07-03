/*
 * KALICI-FS milestone testi (aarch64) — WRITE-AHEAD JOURNALING (WAL) + CRASH KURTARMA.
 * =============================================================================
 *
 * Crash-tutarli yazma: veri blogu degismeden ONCE her yazim niyeti bir journal
 * (write-ahead log) blogua kaydedilir. Yazim yarida kesilse bile, journal'daki
 * "commit" bayragi 1 ise, kurtarma fazi journal'daki veriyi hedef veri-bloguna
 * REPLAY eder → disk her zaman tutarli bir duruma gelir.
 *
 * Disk duzeni (512-baytlik sektorler):
 *   blok 0  = JOURNAL:  [magic "JRNL"(4)][hedef_sektor(4)][veri(4)][...][commit(1)@511]
 *   blok 10 = VERI:     kullaniciya ait gercek veri (4 baytlik isaretci; 0xCAFE...)
 *
 * WAL yazim protokolu (kdl_wal_yaz):
 *   (a) journal blogua yaz: [magic][hedef_sektor][veri][commit=0]
 *   (b) journal'i diske flush et (blk_yaz)
 *   (c) commit=1 yap + flush   → journal artik GECERLI (kurtarilabilir)
 *   (d) VERI blogua gercek veriyi yaz
 *   (e) journal'i temizle (commit=0) + flush → journal artik gereksiz
 *
 * Kurtarma protokolu (kdl_wal_kurtar) — her boot basinda:
 *   journal oku → magic "JRNL" mi? & commit==1 mi? → EVET → journal'daki veriyi
 *   hedef veri-bloguna YAZ (replay) → journal temizle. HAYIR → yapacak is yok.
 *
 * Iki senaryo (tek-boot icinde, deterministik):
 *   Senaryo 1 (TEMIZ-COMMIT): tam WAL yazim (a→e). Kurtarma cagrilir ama journal
 *     temiz oldugu icin no-op. Veri blogu dogru olmali.
 *   Senaryo 2 (CRASH-REPLAY): WAL yaziminin (a→c) adimlari yapilir AMA (d) ATLANIR
 *     (crash simulasyonu: journal commit=1 ama veri blogu YAZILMADI). Sonra kurtarma
 *     cagrilir → journal'dan replay → veri blogu journal verisiyle eslesmeli.
 *
 * Kanit: iki senaryo da gecerse "FS JOURNAL OK". Baslangic: "FS JOURNAL BASLA".
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

#define JOURNAL_SEKTOR   0UL     /* WAL journal blogu */
#define VERI_SEKTOR      10UL    /* gercek veri blogu */
#define COMMIT_OFS       511     /* commit bayragi sektorun son baytinda */

/* Journal magic: "JRNL" (bytes 0-3). Sabit tam sayi ile karsilastirma yerine
 * byte-byte kontrol kullaniyoruz (endianness bagimsiz, sifir varsayim). */
static const uint8_t JRNL_MAGIC[4] = { 'J', 'R', 'N', 'L' };

static uint8_t jtampon[512];   /* journal sektor tamponu */
static uint8_t vtampon[512];   /* veri sektor tamponu */

static _Noreturn void halt(void) { for (;;) { __asm__ volatile("wfe"); } }

/* 32-bit degeri tampona little-endian yaz. */
static void u32_yaz(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xff);
    p[1] = (uint8_t)((v >> 8) & 0xff);
    p[2] = (uint8_t)((v >> 16) & 0xff);
    p[3] = (uint8_t)((v >> 24) & 0xff);
}
/* Tampondan 32-bit little-endian degeri oku. */
static uint32_t u32_oku(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* --- WAL YAZIM PROTOKOLU ---
 * crash_simule != 0 ise (d) adimi (veri blogua yaz) ATLANIR → crash simulasyonu.
 * 0 = ok, negatif = disk hatasi. */
static int kdl_wal_yaz(uint64_t base, uint32_t hedef_sektor, uint32_t veri, int crash_simule) {
    /* (a) journal kaydini hazirla: [magic][hedef_sektor][veri][commit=0] */
    for (int i = 0; i < 512; i++) jtampon[i] = 0;
    jtampon[0] = JRNL_MAGIC[0]; jtampon[1] = JRNL_MAGIC[1];
    jtampon[2] = JRNL_MAGIC[2]; jtampon[3] = JRNL_MAGIC[3];
    u32_yaz(&jtampon[4], hedef_sektor);
    u32_yaz(&jtampon[8], veri);
    jtampon[COMMIT_OFS] = 0;                 /* henuz commit degil */

    /* (b) journal'i diske flush et (commit=0 haliyle). */
    if (kdl_virtio_blk_yaz(base, JOURNAL_SEKTOR, jtampon) != 0) return -1;

    /* (c) commit=1 yap + flush → journal artik GECERLI (kurtarilabilir). */
    jtampon[COMMIT_OFS] = 1;
    if (kdl_virtio_blk_yaz(base, JOURNAL_SEKTOR, jtampon) != 0) return -2;

    /* (d) VERI blogua gercek veriyi yaz. crash_simule ise BU ADIM ATLANIR. */
    if (!crash_simule) {
        for (int i = 0; i < 512; i++) vtampon[i] = 0;
        u32_yaz(&vtampon[0], veri);
        if (kdl_virtio_blk_yaz(base, hedef_sektor, vtampon) != 0) return -3;

        /* (e) journal'i temizle (commit=0) + flush → journal artik gereksiz. */
        jtampon[COMMIT_OFS] = 0;
        if (kdl_virtio_blk_yaz(base, JOURNAL_SEKTOR, jtampon) != 0) return -4;
    }
    /* crash_simule: journal commit=1 kaldi, veri blogu yazilmadi → kurtarma bunu
     * duzeltecek. */
    return 0;
}

/* --- KURTARMA PROTOKOLU (her boot basinda cagrilir) ---
 * journal oku → magic gecerli & commit==1 → journal'daki veriyi hedef veri-bloguna
 * REPLAY et → journal temizle. Donus: 1 = replay yapildi, 0 = yapacak is yok,
 * negatif = disk hatasi. */
static int kdl_wal_kurtar(uint64_t base) {
    if (kdl_virtio_blk_oku(base, JOURNAL_SEKTOR, jtampon) != 0) return -1;

    /* magic "JRNL" mi? */
    int magic_ok = 1;
    for (int i = 0; i < 4; i++) if (jtampon[i] != JRNL_MAGIC[i]) { magic_ok = 0; break; }
    if (!magic_ok) return 0;                 /* journal yok/gecersiz → is yok */

    if (jtampon[COMMIT_OFS] != 1) return 0;  /* commit edilmemis → is yok */

    /* Journal GECERLI + commit=1 → replay: journal verisini hedef veri-bloguna yaz. */
    uint32_t hedef_sektor = u32_oku(&jtampon[4]);
    uint32_t veri = u32_oku(&jtampon[8]);
    for (int i = 0; i < 512; i++) vtampon[i] = 0;
    u32_yaz(&vtampon[0], veri);
    if (kdl_virtio_blk_yaz(base, (uint64_t)hedef_sektor, vtampon) != 0) return -2;

    /* Replay tamam → journal temizle (commit=0). */
    jtampon[COMMIT_OFS] = 0;
    if (kdl_virtio_blk_yaz(base, JOURNAL_SEKTOR, jtampon) != 0) return -3;
    return 1;                                /* replay yapildi */
}

/* Veri blogunu oku, ilk 4 baytindaki 32-bit degeri dondur. */
static int veri_oku(uint64_t base, uint32_t *out) {
    if (kdl_virtio_blk_oku(base, VERI_SEKTOR, vtampon) != 0) return -1;
    *out = u32_oku(&vtampon[0]);
    return 0;
}

int main(void) {
    kdl_yazdir_metin("FS JOURNAL BASLA");
    kdl_yazdir_satir();

    uint64_t base = kdl_virtio_blk_bul();
    if (!base) { kdl_yazdir_metin("DISK YOK"); kdl_yazdir_satir(); halt(); }
    if (kdl_virtio_blk_kur(base) != 0) { kdl_yazdir_metin("KUR HATA"); kdl_yazdir_satir(); halt(); }

    int tum_ok = 1;

    /* ============ SENARYO 1: TEMIZ-COMMIT ============
     * Tam WAL yazim (a→e). Journal kurtarma sonrasi temiz → veri blogu dogru. */
    const uint32_t DEGER1 = 0xCAFEu;
    if (kdl_wal_yaz(base, (uint32_t)VERI_SEKTOR, DEGER1, /*crash_simule=*/0) != 0) {
        kdl_yazdir_metin("S1 WAL YAZ HATA"); kdl_yazdir_satir(); halt();
    }
    /* Kurtarma cagir: journal temiz → no-op beklenir (donus 0). */
    int k1 = kdl_wal_kurtar(base);
    uint32_t okunan1 = 0;
    if (veri_oku(base, &okunan1) != 0) { kdl_yazdir_metin("S1 OKU HATA"); kdl_yazdir_satir(); halt(); }

    kdl_yaz_metin("S1 temiz-commit: veri=");
    kdl_yaz_onaltilik(okunan1);
    kdl_yaz_metin(" kurtarma=");
    kdl_yaz_onaltilik((uint64_t)(uint32_t)k1);
    kdl_yazdir_satir();
    /* Beklenti: veri == 0xCAFE, kurtarma == 0 (journal temizdi, replay gerekmedi). */
    if (okunan1 != DEGER1 || k1 != 0) { tum_ok = 0; kdl_yazdir_metin("S1 FAIL"); kdl_yazdir_satir(); }

    /* ============ SENARYO 2: CRASH-REPLAY ============
     * WAL (a→c) yapilir, (d) ATLANIR (crash: journal commit=1 ama veri yazilmadi).
     * Kurtarma journal'dan replay etmeli → veri blogu journal verisiyle eslesir. */
    const uint32_t DEGER2 = 0xBEEFu;
    if (kdl_wal_yaz(base, (uint32_t)VERI_SEKTOR, DEGER2, /*crash_simule=*/1) != 0) {
        kdl_yazdir_metin("S2 WAL YAZ HATA"); kdl_yazdir_satir(); halt();
    }
    /* Crash-oncesi durum: veri blogu HALA 0xCAFE (S2 verisi diske hic yazilmadi). */
    uint32_t crash_oncesi = 0;
    if (veri_oku(base, &crash_oncesi) != 0) { kdl_yazdir_metin("S2 OKU HATA"); kdl_yazdir_satir(); halt(); }

    /* Kurtarma cagir: journal commit=1 → replay 0xBEEF → veri blogu guncellenir. */
    int k2 = kdl_wal_kurtar(base);
    uint32_t okunan2 = 0;
    if (veri_oku(base, &okunan2) != 0) { kdl_yazdir_metin("S2 OKU2 HATA"); kdl_yazdir_satir(); halt(); }

    kdl_yaz_metin("S2 crash-oncesi veri=");
    kdl_yaz_onaltilik(crash_oncesi);
    kdl_yaz_metin(" replay-sonrasi=");
    kdl_yaz_onaltilik(okunan2);
    kdl_yaz_metin(" kurtarma=");
    kdl_yaz_onaltilik((uint64_t)(uint32_t)k2);
    kdl_yazdir_satir();
    /* Beklenti: crash_oncesi == 0xCAFE (yazim yarida kesildi), kurtarma == 1
     * (replay yapildi), replay sonrasi veri == 0xBEEF (journal'dan geri geldi). */
    if (crash_oncesi != DEGER1 || k2 != 1 || okunan2 != DEGER2) {
        tum_ok = 0; kdl_yazdir_metin("S2 FAIL"); kdl_yazdir_satir();
    }

    kdl_yazdir_metin(tum_ok ? "FS JOURNAL OK" : "FS JOURNAL HATA");
    kdl_yazdir_satir();
    halt();
}
