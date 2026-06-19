/*
 * KEMGU Bölge Arena Allokatörü Birim Testi (V2 F4 FAZ 0 — D-099)
 * =============================================================
 * runtime/kdl_bolge.c: blok-içi tahsis · blok-sınırı aşımı (büyüme) · oversized
 * · free-all · sızıntı bakiyesi · hizalama. ASan/UBSan ile derlenir (overflow,
 * use-after-free, hizalama-UB yakalanır).
 */
#include "kdl_bolge.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

static int toplam = 0, basarili = 0, basarisiz = 0;

static void onayla(const char *ad, int kosul) {
    toplam++;
    if (kosul) { basarili++; printf("  [%d] %s ... OK\n", toplam, ad); }
    else       { basarisiz++; printf("  [%d] %s ... HATA\n", toplam, ad); }
}

static int hizali16(const void *p) { return ((uintptr_t)p % 16u) == 0; }

/* === T1: blok-içi tahsis — distinct, hizalı, yazılabilir === */
static void test_blok_ici(void) {
    KdlBolge *b = kdl_bolge_olustur();
    onayla("olustur NULL degil", b != NULL);
    void *p1 = kdl_bolge_ayir(b, 32);
    void *p2 = kdl_bolge_ayir(b, 32);
    void *p3 = kdl_bolge_ayir(b, 64);
    onayla("uc tahsis NULL degil", p1 && p2 && p3);
    onayla("tahsisler distinct", p1 != p2 && p2 != p3 && p1 != p3);
    onayla("hepsi 16-hizali", hizali16(p1) && hizali16(p2) && hizali16(p3));
    /* yaz/oku — örtüşme yok */
    memset(p1, 0x11, 32); memset(p2, 0x22, 32); memset(p3, 0x33, 64);
    onayla("p1 yazimi p2'den etkilenmedi", ((unsigned char *)p1)[0] == 0x11
           && ((unsigned char *)p1)[31] == 0x11);
    onayla("p3 son bayti dogru", ((unsigned char *)p3)[63] == 0x33);
    kdl_bolge_serbest(b);
}

/* === T2: blok-sınırı aşımı (büyüme) — 2x40KB (>64KB) ayrı bloklara düşer === */
static void test_buyume(void) {
    KdlBolge *b = kdl_bolge_olustur();
    onayla("buyume: baslangicta 1 blok", kdl_bolge_blok_sayisi(b) == 1);
    unsigned char *p1 = (unsigned char *)kdl_bolge_ayir(b, 40000);
    onayla("buyume: ilk 40KB tahsis", p1 != NULL);
    onayla("buyume: ilk tahsis hala 1 blok", kdl_bolge_blok_sayisi(b) == 1);
    memset(p1, 0xAB, 40000);
    /* 40000 + 40000 = 80000 > 64KB → tek 64KB blok yetmez → yeni blok ZORUNLU */
    unsigned char *p2 = (unsigned char *)kdl_bolge_ayir(b, 40000);
    onayla("buyume: ikinci 40KB tahsis", p2 != NULL);
    onayla("buyume: blok sayisi 2'ye cikti (kesin buyume)",
           kdl_bolge_blok_sayisi(b) == 2);
    memset(p2, 0xCD, 40000);
    /* örtüşme yoksa p1 hâlâ 0xAB (p2 yazımı bozmadı) */
    int p1_saglam = 1;
    for (int i = 0; i < 40000; i++) if (p1[i] != 0xAB) { p1_saglam = 0; break; }
    onayla("buyume: p1 p2 yaziminca bozulmadi (ayri bloklar)", p1_saglam);
    onayla("buyume: p1/p2 hizali", hizali16(p1) && hizali16(p2));
    kdl_bolge_serbest(b);
}

/* === T3: oversized — tek tahsis > varsayılan blok (adanmış blok) === */
static void test_oversized(void) {
    KdlBolge *b = kdl_bolge_olustur();
    uint64_t n = 128u * 1024u;   /* 128 KB > 64 KB varsayılan */
    unsigned char *p = (unsigned char *)kdl_bolge_ayir(b, n);
    onayla("oversized: 128KB tahsis NULL degil", p != NULL);
    onayla("oversized: 16-hizali", hizali16(p));
    /* oversized fresh bölgede adanmış 2. blok yaratır (1. blok 64KB sığmaz). */
    onayla("oversized: adanmis blok (blok sayisi 2)", kdl_bolge_blok_sayisi(b) == 2);
    if (p) {
        memset(p, 0x7E, (size_t)n);
        onayla("oversized: ilk+son bayt yazilabilir",
               p[0] == 0x7E && p[n - 1] == 0x7E);
    } else onayla("oversized: ilk+son bayt yazilabilir", 0);
    /* oversized'dan sonra normal tahsis hâlâ çalışır */
    void *q = kdl_bolge_ayir(b, 16);
    onayla("oversized sonrasi normal tahsis", q != NULL && hizali16(q));
    kdl_bolge_serbest(b);
}

/* === T4: free-all + sızıntı bakiyesi === */
static void test_bakiye(void) {
    int taban = kdl_bolge_bakiye();
    KdlBolge *b1 = kdl_bolge_olustur();
    KdlBolge *b2 = kdl_bolge_olustur();
    onayla("iki bolge -> bakiye +2", kdl_bolge_bakiye() == taban + 2);
    /* her bölgede çoklu blok zorla (büyüme) — free-all hepsini almalı */
    for (int i = 0; i < 5; i++) { kdl_bolge_ayir(b1, 50000); kdl_bolge_ayir(b2, 50000); }
    kdl_bolge_serbest(b1);
    onayla("bir serbest -> bakiye +1", kdl_bolge_bakiye() == taban + 1);
    kdl_bolge_serbest(b2);
    onayla("tum serbest -> bakiye taban (0 delta)", kdl_bolge_bakiye() == taban);
}

/* === T7: kenar durumlar — NULL handle + taşma reddi (adversarial inceleme) === */
static void test_kenar(void) {
    /* NULL handle güvenli: ayir(NULL,..) -> NULL; serbest(NULL) -> no-op
     * (sayacı ARTIRMAZ — NULL guard, sayaç artışından önce döner). */
    onayla("ayir(NULL, 16) -> NULL", kdl_bolge_ayir(NULL, 16) == NULL);
    int taban = kdl_bolge_bakiye();
    kdl_bolge_serbest(NULL);
    onayla("serbest(NULL) -> no-op (bakiye degismez)", kdl_bolge_bakiye() == taban);
    /* Taşma reddi: hizalama/boyut sarması malloc'tan ÖNCE NULL döner (under-alloc YOK). */
    KdlBolge *b = kdl_bolge_olustur();
    onayla("ayir(b, UINT64_MAX) -> NULL (tasma reddi)",
           kdl_bolge_ayir(b, UINT64_MAX) == NULL);
    onayla("ayir(b, UINT64_MAX-8) -> NULL (hiza tasmasi)",
           kdl_bolge_ayir(b, UINT64_MAX - 8) == NULL);
    /* Taşma reddinden sonra bölge hâlâ kullanılabilir (durum bozulmadı). */
    void *p = kdl_bolge_ayir(b, 16);
    onayla("tasma reddi sonrasi normal tahsis calisir", p != NULL && hizali16(p));
    kdl_bolge_serbest(b);
}

/* === T5: hizalama — çeşitli boyutlar 16-hizalı; 8/16-bayt türler düzgün === */
static void test_hizalama(void) {
    KdlBolge *b = kdl_bolge_olustur();
    uint64_t boyutlar[] = { 1, 7, 8, 9, 15, 16, 17, 31, 33 };
    int hepsi_hizali = 1;
    for (size_t i = 0; i < sizeof(boyutlar)/sizeof(boyutlar[0]); i++) {
        void *p = kdl_bolge_ayir(b, boyutlar[i]);
        if (!p || !hizali16(p)) { hepsi_hizali = 0; break; }
    }
    onayla("tum boyutlar 16-hizali", hepsi_hizali);
    /* 8-bayt türü hizalı yaz/oku (UB yoksa ASan/UBSan temiz) */
    uint64_t *u = (uint64_t *)kdl_bolge_ayir(b, sizeof(uint64_t));
    onayla("uint64 ptr 8-hizali (>=16)", u && ((uintptr_t)u % 8u) == 0);
    if (u) { *u = 0x0123456789ABCDEFull; onayla("uint64 yaz/oku", *u == 0x0123456789ABCDEFull); }
    else onayla("uint64 yaz/oku", 0);
    /* 16-hizalı tür simülasyonu */
    typedef struct { uint64_t a, b; } Cift16;
    Cift16 *c = (Cift16 *)kdl_bolge_ayir(b, sizeof(Cift16));
    onayla("Cift16 ptr 16-hizali", c && hizali16(c));
    if (c) { c->a = 11; c->b = 22; onayla("Cift16 yaz/oku", c->a == 11 && c->b == 22); }
    else onayla("Cift16 yaz/oku", 0);
    kdl_bolge_serbest(b);
}

/* === T6: çok sayıda küçük tahsis (blok-içi yoğun bump, örtüşmez) === */
static void test_yogun(void) {
    KdlBolge *b = kdl_bolge_olustur();
    enum { N = 1000 };
    int32_t *ptrs[N];
    int ok = 1;
    for (int i = 0; i < N; i++) {
        ptrs[i] = (int32_t *)kdl_bolge_ayir(b, sizeof(int32_t));
        if (!ptrs[i]) { ok = 0; break; }
        *ptrs[i] = i;   /* benzersiz işaret */
    }
    onayla("1000 kucuk tahsis NULL degil", ok);
    int saglam = 1;
    for (int i = 0; i < N; i++) if (!ptrs[i] || *ptrs[i] != i) { saglam = 0; break; }
    onayla("1000 tahsis ortusmez (her biri kendi degeri)", saglam);
    kdl_bolge_serbest(b);
}

int main(void) {
    printf("KEMGU Bolge Arena Allokatoru Birim Testi (F4.0)\n");
    printf("================================================\n");
    test_blok_ici();
    test_buyume();
    test_oversized();
    test_bakiye();
    test_hizalama();
    test_yogun();
    test_kenar();
    printf("------------------------------------------------\n");
    printf("Toplam: %d | Basarili: %d | Basarisiz: %d\n", toplam, basarili, basarisiz);
    printf("Sizinti bakiyesi (test sonu): %d (0 beklenir)\n", kdl_bolge_bakiye());
    if (kdl_bolge_bakiye() != 0) { printf("HATA: sizinti bakiyesi 0 degil!\n"); basarisiz++; }
    if (basarisiz == 0) printf("Tum bolge testleri gecti!\n");
    return basarisiz > 0 ? 1 : 0;
}
