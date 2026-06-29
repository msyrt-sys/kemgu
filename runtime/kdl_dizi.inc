/*
 * KEMGU Dinamik Dizi (heap) — PAYLAŞIMLI implementasyon (kdl_dizi.inc)
 * ===================================================================
 *
 * TEK KAYNAK: host (kdl_runtime.c) ve bare-metal (kdl_bare_heap.c) bu dosyayı
 * `#include` eder → her TU kendi kopyasını derler, ASLA birlikte linklenmez
 * (duplicate-symbol yok). Böylece sınır-kontrollü dizi mantığı iki yerde
 * KOPYALANMAZ (D-069 güvenlik invaryantı tek yerden).
 *
 * Bu .inc'i include eden TU ŞUNLARI sağlamalı (include'tan ÖNCE):
 *   - <stdint.h>, <stddef.h>  (int32/64_t, size_t)
 *   - "kdl_bolge.h"           (KdlBolge, kdl_bolge_ayir)
 *   - memcpy                  (host: <string.h>; bare-metal: kdl_bare_heap.c)
 *   - KdlBolge *kdl_global_bolge_al(void);
 *   - __attribute__((noreturn)) void kdl_panik(const char *);  (panik seam)
 *
 * KdlDizi temsili: { ptr veri, i32 boyut, i32 kapasite, i32 eleman_byte }.
 * Bölge-sahipli (kdl_bolge_ayir) — realloc YOK, büyümede yeni bölge tamponu +
 * canlı kopya (eski tampon bölgede sızar; bölge kapanışında topluca serbest).
 */

typedef struct {
    void *veri;
    int32_t boyut;
    int32_t kapasite;
    int32_t eleman_byte;
} KdlDizi;

/* V2-F4.2a: büyüme çağıranın bölgesinden (ρ). ρ NULL ise global bölge. */
static void kdl_dizi_buyut(KdlBolge *rho, KdlDizi *d, int32_t yeni_kap, size_t eb) {
    void *yeni = kdl_bolge_ayir(rho ? rho : kdl_global_bolge_al(),
                               (uint64_t)yeni_kap * (uint64_t)eb);
    if (yeni && d->veri && d->boyut > 0)
        memcpy(yeni, d->veri, (size_t)d->boyut * eb);
    d->veri = yeni;
    d->kapasite = yeni_kap;
}

KdlDizi *kdl_dizi_olustur(KdlBolge *rho, int32_t eleman_byte) {
    KdlDizi *d = (KdlDizi *)kdl_bolge_ayir(rho ? rho : kdl_global_bolge_al(),
                                          sizeof(KdlDizi));
    if (!d) return NULL;
    d->veri = NULL;
    d->boyut = 0;
    d->kapasite = 0;
    d->eleman_byte = eleman_byte;
    return d;
}

void kdl_dizi_ekle_tam(KdlBolge *rho, KdlDizi *d, int32_t deger) {
    if (!d) return;
    if (d->boyut == d->kapasite) {
        int32_t yk = d->kapasite ? d->kapasite * 2 : 4;
        kdl_dizi_buyut(rho, d, yk, sizeof(int32_t));
    }
    ((int32_t *)d->veri)[d->boyut++] = deger;
}

void kdl_dizi_ekle_tam64(KdlBolge *rho, KdlDizi *d, int64_t deger) {
    if (!d) return;
    if (d->boyut == d->kapasite) {
        int32_t yk = d->kapasite ? d->kapasite * 2 : 4;
        kdl_dizi_buyut(rho, d, yk, sizeof(int64_t));
    }
    ((int64_t *)d->veri)[d->boyut++] = deger;
}

void kdl_dizi_ekle_ptr(KdlBolge *rho, KdlDizi *d, void *deger) {
    if (!d) return;
    if (d->boyut == d->kapasite) {
        int32_t yk = d->kapasite ? d->kapasite * 2 : 4;
        kdl_dizi_buyut(rho, d, yk, sizeof(void *));
    }
    ((void **)d->veri)[d->boyut++] = deger;
}

/* ===================================================================
 * Dizi sınır-güvenliği (D-069): OOB erişim → temiz PANIC, asla segfault.
 * kdl_panik seam: host = stderr+abort; bare-metal = UART+halt.
 * Mesaj FREESTANDING biçimlenir (snprintf YOK) → host+bare-metal ortak,
 * "(i, boyut)" detayı korunur.
 * =================================================================== */
static char *kdl_dizi_oob_int(char *p, int32_t v) {
    if (v < 0) *p++ = '-';
    /* -INT32_MIN taşmasını 64-bit ile güvenli al */
    uint32_t u = (v < 0) ? (uint32_t)(-(int64_t)v) : (uint32_t)v;
    char tmp[12];
    int n = 0;
    do { tmp[n++] = (char)('0' + (u % 10u)); u /= 10u; } while (u);
    while (n > 0) *p++ = tmp[--n];
    return p;
}

static __attribute__((noreturn)) void kdl_dizi_oob(int32_t i, int32_t boyut) {
    char buf[64];
    char *p = buf;
    const char *pre = "dizi s\xc4\xb1n\xc4\xb1r ihlali (i=";   /* "dizi sınır ihlali (i=" */
    while (*pre) *p++ = *pre++;
    p = kdl_dizi_oob_int(p, i);
    const char *mid = ", boyut=";
    while (*mid) *p++ = *mid++;
    p = kdl_dizi_oob_int(p, boyut);
    *p++ = ')';
    *p = 0;
    kdl_panik(buf);
}

int32_t kdl_dizi_al_tam(KdlDizi *d, int32_t i) {
    if (!d) return 0;
    if (i < 0 || i >= d->boyut) kdl_dizi_oob(i, d->boyut);
    return ((int32_t *)d->veri)[i];
}

int64_t kdl_dizi_al_tam64(KdlDizi *d, int32_t i) {
    if (!d) return 0;
    if (i < 0 || i >= d->boyut) kdl_dizi_oob(i, d->boyut);
    return ((int64_t *)d->veri)[i];
}

void *kdl_dizi_al_ptr(KdlDizi *d, int32_t i) {
    if (!d) return NULL;
    if (i < 0 || i >= d->boyut) kdl_dizi_oob(i, d->boyut);
    return ((void **)d->veri)[i];
}

void kdl_dizi_yaz_tam(KdlDizi *d, int32_t i, int32_t deger) {
    if (!d) return;
    if (i < 0 || i >= d->boyut) kdl_dizi_oob(i, d->boyut);
    ((int32_t *)d->veri)[i] = deger;
}

void kdl_dizi_yaz_tam64(KdlDizi *d, int32_t i, int64_t deger) {
    if (!d) return;
    if (i < 0 || i >= d->boyut) kdl_dizi_oob(i, d->boyut);
    ((int64_t *)d->veri)[i] = deger;
}

void kdl_dizi_yaz_ptr(KdlDizi *d, int32_t i, void *deger) {
    if (!d) return;
    if (i < 0 || i >= d->boyut) kdl_dizi_oob(i, d->boyut);
    ((void **)d->veri)[i] = deger;
}

/* === D-087: by-value YAPI (struct) elemanlı dizi === */
void kdl_dizi_ekle_yapi(KdlBolge *rho, KdlDizi *d, const void *kaynak) {
    if (!d || !kaynak) return;
    int32_t eb = d->eleman_byte > 0 ? d->eleman_byte : 1;
    if (d->boyut == d->kapasite) {
        int32_t yk = d->kapasite ? d->kapasite * 2 : 4;
        kdl_dizi_buyut(rho, d, yk, (size_t)eb);
    }
    memcpy((char *)d->veri + (size_t)d->boyut * (size_t)eb, kaynak, (size_t)eb);
    d->boyut++;
}

void kdl_dizi_al_yapi(KdlDizi *d, int32_t i, void *dst) {
    if (!d || !dst) return;
    if (i < 0 || i >= d->boyut) kdl_dizi_oob(i, d->boyut);
    int32_t eb = d->eleman_byte > 0 ? d->eleman_byte : 1;
    memcpy(dst, (const char *)d->veri + (size_t)i * (size_t)eb, (size_t)eb);
}

void kdl_dizi_yaz_yapi(KdlDizi *d, int32_t i, const void *kaynak) {
    if (!d || !kaynak) return;
    if (i < 0 || i >= d->boyut) kdl_dizi_oob(i, d->boyut);
    int32_t eb = d->eleman_byte > 0 ? d->eleman_byte : 1;
    memcpy((char *)d->veri + (size_t)i * (size_t)eb, kaynak, (size_t)eb);
}

int32_t kdl_dizi_boyut(KdlDizi *d) {
    return d ? d->boyut : 0;
}

int32_t kdl_dizi_kapasite(KdlDizi *d) {
    return d ? d->kapasite : 0;
}

void kdl_dizi_kapasite_ayarla(KdlBolge *rho, KdlDizi *d, int32_t yeni_kapasite) {
    if (!d || yeni_kapasite <= d->kapasite) return;
    int32_t eb = d->eleman_byte > 0 ? d->eleman_byte : 4;
    void *yeni = kdl_bolge_ayir(rho ? rho : kdl_global_bolge_al(),
                               (uint64_t)yeni_kapasite * (uint64_t)eb);
    if (!yeni) return;
    if (d->veri && d->boyut > 0)
        memcpy(yeni, d->veri, (size_t)d->boyut * (size_t)eb);
    d->veri = yeni;
    d->kapasite = yeni_kapasite;
}

/* V2-F4.1: NÖTR (no-op) — d->veri bölge-sahipli (free() YANLIŞ). Bölge
 * kapanışında topluca serbest. Codegen zaten emit etmiyor; imza geriye-uyum. */
void kdl_dizi_serbest(KdlDizi *d) {
    (void)d;
}
