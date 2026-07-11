/*
 * KEMGU Bölge (Region) Arena Allokatörü — Uygulama (kdl_bolge.c)
 * =============================================================
 *
 * Bkz. kdl_bolge.h. Bölge = malloc'lu blok tek-yönlü listesi + blok-içi bump
 * pointer. Aktif blok = listenin başı (en yeni). Tahsis aktif bloktan bump'lar;
 * yer kalmazsa yeni blok (boyut = max(varsayılan, istek)) eklenir. Bölge
 * kapanışında bloklar baştan sona free edilir.
 *
 * Hizalama: her tahsis 16-bayt hizalı başlar. Esnek dizi (FAM) ofseti platforma
 * göre hizalı OLMAYABİLİR (örn. 64-bit'te header 24 bayt → veri 8-hizalı), bu
 * yüzden hizalama derleme-zamanı ofsetine değil ÇALIŞMA-ZAMANI ADRESİNE göre
 * yapılır → her platformda/header düzeninde doğru.
 *
 * Bare-metal (TODO): KEMGU_BARE_METAL altında malloc/free yerine kdl
 * page-allocator'a (NOTES_TRACK_B bump _heap_start/_heap_end) bağlanacak.
 * F4.0 host hedefi için stdlib malloc/free yeterli — bare-metal bu dosyayı
 * henüz derlemiyor, dolayısıyla şimdilik koşulsuz stdlib kullanılır.
 */
#include "kdl_bolge.h"

#include <stdint.h>
#ifdef KEMGU_BARE_METAL
/* Bare-metal: libc YOK. <stdlib.h> (malloc/free/size_t) aarch64/x86_64
 * -unknown-none hedefinde bulunmaz. size_t için freestanding <stddef.h>;
 * malloc/free sembollerini bare-metal heap (runtime/kdl_bare_heap.c) sağlar —
 * prototipleri burada bildiririz. SIZE_MAX zaten <stdint.h>'den gelir. */
#include <stddef.h>
void *malloc(size_t);
void  free(void *);
#else
#include <stdlib.h>
#endif

#define KDL_BOLGE_HIZA       16u               /* tahsis hizalaması (>= max_align_t) */
#define KDL_BOLGE_VARSAYILAN (64u * 1024u)     /* varsayılan blok = 64 KB */

typedef struct KdlBolgeBlok {
    struct KdlBolgeBlok *sonraki;  /* listede bir önceki (daha eski) blok */
    uint64_t kapasite;             /* veri[] toplam bayt kapasitesi */
    uint64_t kullanilan;           /* veri[] içinde tüketilen bayt (hiza payı dahil) */
    unsigned char veri[];          /* C99 esnek dizi — bump alanı */
} KdlBolgeBlok;

struct KdlBolge {
    KdlBolgeBlok *bas;             /* aktif blok = en yeni (liste başı) */
    int blok_sayisi;              /* teşhis/test için blok adedi */
};

/* === Sızıntı-tanığı sayaçları === */
uint64_t kdl_bolge_olustur_sayisi = 0;
uint64_t kdl_bolge_serbest_sayisi = 0;

/* K3 (D-261): KEMGU_KEM_MALLOC (kem_os pure-.kem runtime) tanımlıysa region
 * primitifleri (olustur/ayir/serbest + helpers) SAF-.kem'den gelir (kem_heap.kem
 * → çıplak @kdl_bolge_olustur/ayir/serbest, .kem malloc/free ile). Bu dosya o zaman
 * yalnız sızıntı-sayaçları + bakiye/blok_sayisi diagnostiklerini tutar. Diğer
 * kernel'ler bayrağı SET ETMEZ → C region (bu bloğu derler). */
#ifndef KEMGU_KEM_MALLOC

/* x'i 16'ya yukarı yuvarla. Taşarsa 0'dan küçük (wrap) döner → çağıran kontrol. */
static uint64_t hiza_yukari(uint64_t x) {
    return (x + (KDL_BOLGE_HIZA - 1)) & ~(uint64_t)(KDL_BOLGE_HIZA - 1);
}

/* kap baytlık veri kapasiteli yeni blok malloc'la. Taşma/bellek hatasında NULL. */
static KdlBolgeBlok *blok_olustur(uint64_t kap) {
    /* sizeof(header) + kap size_t'te taşmasın (kap çok büyükse). */
    if (kap > (uint64_t)(SIZE_MAX - sizeof(KdlBolgeBlok))) return NULL;
    KdlBolgeBlok *blk = (KdlBolgeBlok *)malloc(sizeof(KdlBolgeBlok) + (size_t)kap);
    if (!blk) return NULL;
    blk->sonraki = NULL;
    blk->kapasite = kap;
    blk->kullanilan = 0;
    return blk;
}

KdlBolge *kdl_bolge_olustur(void) {
    KdlBolge *b = (KdlBolge *)malloc(sizeof(KdlBolge));
    if (!b) return NULL;
    b->bas = blok_olustur(KDL_BOLGE_VARSAYILAN);
    if (!b->bas) { free(b); return NULL; }
    b->blok_sayisi = 1;
    kdl_bolge_olustur_sayisi++;
    return b;
}

void *kdl_bolge_ayir(KdlBolge *b, uint64_t n) {
    if (!b) return NULL;
    if (n == 0) n = 1;                       /* 0-bayt → 1 (benzersiz, geçerli ptr) */
    uint64_t hn = hiza_yukari(n);
    if (hn < n) return NULL;                 /* hizalama taşması */

    /* 1) Aktif blokta yer var mı? Başlangıç adresini 16'ya hizala. */
    KdlBolgeBlok *blok = b->bas;
    if (blok) {
        uintptr_t base = (uintptr_t)blok->veri;
        uintptr_t cur  = base + (uintptr_t)blok->kullanilan;
        uintptr_t ahiz = (cur + (KDL_BOLGE_HIZA - 1)) & ~(uintptr_t)(KDL_BOLGE_HIZA - 1);
        uint64_t pad   = (uint64_t)(ahiz - cur);          /* ahiz >= cur */
        uint64_t kalan = blok->kapasite - blok->kullanilan;  /* kullanilan <= kapasite */
        if (pad <= kalan && hn <= kalan - pad) {
            blok->kullanilan += pad + hn;
            return (void *)ahiz;
        }
    }

    /* 2) Yeni blok: hn + en kötü hizalama payı (16) garantili sığsın. */
    uint64_t kap = hn + KDL_BOLGE_HIZA;
    if (kap < hn) return NULL;                /* taşma */
    if (kap < KDL_BOLGE_VARSAYILAN) kap = KDL_BOLGE_VARSAYILAN;
    KdlBolgeBlok *yeni = blok_olustur(kap);
    if (!yeni) return NULL;
    yeni->sonraki = b->bas;
    b->bas = yeni;
    b->blok_sayisi++;

    uintptr_t base = (uintptr_t)yeni->veri;
    uintptr_t ahiz = (base + (KDL_BOLGE_HIZA - 1)) & ~(uintptr_t)(KDL_BOLGE_HIZA - 1);
    uint64_t pad   = (uint64_t)(ahiz - base);             /* <= 15 < kap - hn */
    yeni->kullanilan = pad + hn;
    return (void *)ahiz;
}

void kdl_bolge_serbest(KdlBolge *b) {
    if (!b) return;
    KdlBolgeBlok *blok = b->bas;
    while (blok) {
        KdlBolgeBlok *eski = blok->sonraki;
        free(blok);
        blok = eski;
    }
    free(b);
    kdl_bolge_serbest_sayisi++;
}

#endif  /* !KEMGU_KEM_MALLOC — region primitifleri .kem'den (kem_os) */

int kdl_bolge_bakiye(void) {
    return (int)(kdl_bolge_olustur_sayisi - kdl_bolge_serbest_sayisi);
}

int kdl_bolge_blok_sayisi(const KdlBolge *b) {
    return b ? b->blok_sayisi : 0;
}
