/*
 * [D-777] Host runtime dizi büyütmesinde YERİNDE UZATMA birim testi.
 *
 * kdl_runtime.o'ya doğrudan bağlanır (codegen'den bağımsız). Ölçüt işaretçi
 * KARARLILIĞIDIR: dizi bölgenin en son tahsisi olarak büyürken `veri` değişmemeli
 * (yerinde uzatma); iç içe büyüyen iki dizide içerik bozulmamalı (yalnız EN SON
 * tahsis uzatılır). Çekirdek eşi (kem_heap.kem) calistir_bm_x86_heap_uzat ile ölçülür.
 */
#include <stdint.h>
#include <stdio.h>

typedef struct KdlBolge KdlBolge;
typedef struct {
    void *veri; int32_t boyut; int32_t kapasite; int32_t eleman_byte; KdlBolge *bolge;
} KdlDizi;   /* runtime/kdl_dizi.inc ile birebir */

KdlBolge *kdl_bolge_olustur(void);
void kdl_bolge_serbest(KdlBolge *b);
int kdl_bolge_blok_sayisi(const KdlBolge *b);
KdlDizi *kdl_dizi_olustur(KdlBolge *rho, int32_t eleman_byte);
void kdl_dizi_ekle_tam(KdlBolge *rho, KdlDizi *d, int32_t deger);
void kdl_dizi_ekle_tam64(KdlBolge *rho, KdlDizi *d, int64_t deger);
int32_t kdl_dizi_al_tam(KdlDizi *d, int32_t i);
int64_t kdl_dizi_al_tam64(KdlDizi *d, int32_t i);

static int toplam = 0, basarili = 0;
static void onayla(const char *ad, int ok) {
    toplam++;
    if (ok) basarili++;
    printf("  [%d] %s ... %s\n", toplam, ad, ok ? "OK" : "BASARISIZ");
}

int main(void) {
    printf("KEMGU host dizi yerinde uzatma testi (D-777)\n");
    KdlBolge *b = kdl_bolge_olustur();

    KdlDizi *d = kdl_dizi_olustur(b, 4);
    kdl_dizi_ekle_tam(b, d, 0);
    void *ilk = d->veri;
    for (int32_t i = 1; i < 1000; i++) kdl_dizi_ekle_tam(b, d, i * 7);
    onayla("tek dizi 1000 elemana buyurken veri SABIT (yerinde uzatma)", d->veri == ilk);
    int saglam = d->boyut == 1000;
    for (int32_t i = 0; i < 1000 && saglam; i++) if (kdl_dizi_al_tam(d, i) != i * 7) saglam = 0;
    onayla("tek dizi icerigi dogru", saglam);
    onayla("tek blokta kaldi", kdl_bolge_blok_sayisi(b) == 1);

    KdlDizi *a = kdl_dizi_olustur(b, 4);
    KdlDizi *c = kdl_dizi_olustur(b, 8);
    for (int32_t i = 0; i < 2000; i++) {
        kdl_dizi_ekle_tam(b, a, i * 3);
        kdl_dizi_ekle_tam64(b, c, (int64_t)i * 8589934592LL);
    }
    saglam = a->boyut == 2000 && c->boyut == 2000;
    for (int32_t i = 0; i < 2000 && saglam; i++) {
        if (kdl_dizi_al_tam(a, i) != i * 3) saglam = 0;
        if (kdl_dizi_al_tam64(c, i) != (int64_t)i * 8589934592LL) saglam = 0;
    }
    onayla("ic ice buyuyen iki dizi (biri en son DEGIL) icerigi dogru", saglam);
    saglam = 1;
    for (int32_t i = 0; i < 1000 && saglam; i++) if (kdl_dizi_al_tam(d, i) != i * 7) saglam = 0;
    onayla("onceki dizi sonraki buyumelerden ETKILENMEDI", saglam);

    kdl_bolge_serbest(b);
    printf("Toplam: %d | Basarili: %d | Basarisiz: %d\n", toplam, basarili, toplam - basarili);
    return basarili == toplam ? 0 : 1;
}
