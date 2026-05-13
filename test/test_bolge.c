#include "bolge.h"
#include "arena.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int toplam_test = 0;
static int basarili = 0;
static int basarisiz = 0;

static void test_sonuc(const char *ad, int durum) {
    toplam_test++;
    if (durum) {
        basarili++;
        printf("  [%d] %s ... \xe2\x9c\x93\n", toplam_test, ad);
    } else {
        basarisiz++;
        printf("  [%d] %s ... \xe2\x9c\x97\n", toplam_test, ad);
    }
}

/* === Olusturma === */

static void test_olustur_basit(void) {
    Arena *a = arena_olustur(0);
    BolgeBilgisi *b = bolge_olustur_basit(a, BOLGE_GLOBAL);
    int ok = b && b->kategori == BOLGE_GLOBAL;
    test_sonuc("bolge_olustur_basit(GLOBAL)", ok);
    arena_serbest(a);
}

static void test_olustur_yerel(void) {
    Arena *a = arena_olustur(0);
    BolgeBilgisi *b = bolge_olustur_yerel(a, "fibonacci", 9);
    int ok = b && b->kategori == BOLGE_YEREL
          && b->veri.yerel.adi_uzunluk == 9
          && memcmp(b->veri.yerel.islev_adi, "fibonacci", 9) == 0;
    test_sonuc("bolge_olustur_yerel(fibonacci)", ok);
    arena_serbest(a);
}

static void test_olustur_cagiran(void) {
    Arena *a = arena_olustur(0);
    BolgeBilgisi *b = bolge_olustur_cagiran(a, "main", 4);
    int ok = b && b->kategori == BOLGE_CAGIRAN
          && memcmp(b->veri.cagiran.islev_adi, "main", 4) == 0;
    test_sonuc("bolge_olustur_cagiran(main)", ok);
    arena_serbest(a);
}

static void test_olustur_iterasyon(void) {
    Arena *a = arena_olustur(0);
    BolgeBilgisi *b = bolge_olustur_iterasyon(a, 7);
    int ok = b && b->kategori == BOLGE_ITERASYON
          && b->veri.iterasyon.dongu_id == 7;
    test_sonuc("bolge_olustur_iterasyon(d7)", ok);
    arena_serbest(a);
}

/* === Esitlik === */

static void test_esit_global(void) {
    Arena *a = arena_olustur(0);
    BolgeBilgisi *g1 = bolge_olustur_basit(a, BOLGE_GLOBAL);
    BolgeBilgisi *g2 = bolge_olustur_basit(a, BOLGE_GLOBAL);
    test_sonuc("esit: global == global", bolge_esit(g1, g2));
    arena_serbest(a);
}

static void test_esit_yerel_ayni(void) {
    Arena *a = arena_olustur(0);
    BolgeBilgisi *y1 = bolge_olustur_yerel(a, "f", 1);
    BolgeBilgisi *y2 = bolge_olustur_yerel(a, "f", 1);
    test_sonuc("esit: yerel(f) == yerel(f)", bolge_esit(y1, y2));
    arena_serbest(a);
}

static void test_esit_yerel_farkli(void) {
    Arena *a = arena_olustur(0);
    BolgeBilgisi *y1 = bolge_olustur_yerel(a, "f", 1);
    BolgeBilgisi *y2 = bolge_olustur_yerel(a, "g", 1);
    test_sonuc("esit: yerel(f) != yerel(g)", !bolge_esit(y1, y2));
    arena_serbest(a);
}

static void test_esit_iterasyon(void) {
    Arena *a = arena_olustur(0);
    BolgeBilgisi *i1 = bolge_olustur_iterasyon(a, 5);
    BolgeBilgisi *i2 = bolge_olustur_iterasyon(a, 5);
    BolgeBilgisi *i3 = bolge_olustur_iterasyon(a, 6);
    int ok = bolge_esit(i1, i2) && !bolge_esit(i1, i3);
    test_sonuc("esit: iterasyon(d5) == iterasyon(d5), != iterasyon(d6)", ok);
    arena_serbest(a);
}

/* === Omur sirasi === */

static void test_omur_iterasyon_yerel(void) {
    Arena *a = arena_olustur(0);
    BolgeBilgisi *i = bolge_olustur_iterasyon(a, 1);
    BolgeBilgisi *y = bolge_olustur_yerel(a, "f", 1);
    int ok = bolge_omru_kisa_mi(i, y) && !bolge_omru_kisa_mi(y, i);
    test_sonuc("omur: iterasyon < yerel", ok);
    arena_serbest(a);
}

static void test_omur_yerel_cagiran(void) {
    Arena *a = arena_olustur(0);
    BolgeBilgisi *y = bolge_olustur_yerel(a, "f", 1);
    BolgeBilgisi *c = bolge_olustur_cagiran(a, "f", 1);
    int ok = bolge_omru_kisa_mi(y, c) && !bolge_omru_kisa_mi(c, y);
    test_sonuc("omur: yerel < cagiran", ok);
    arena_serbest(a);
}

static void test_omur_cagiran_global(void) {
    Arena *a = arena_olustur(0);
    BolgeBilgisi *c = bolge_olustur_cagiran(a, "f", 1);
    BolgeBilgisi *g = bolge_olustur_basit(a, BOLGE_GLOBAL);
    int ok = bolge_omru_kisa_mi(c, g) && !bolge_omru_kisa_mi(g, c);
    test_sonuc("omur: cagiran < global", ok);
    arena_serbest(a);
}

/* === LCA === */

static void test_lca_ayni(void) {
    Arena *a = arena_olustur(0);
    BolgeBilgisi *y1 = bolge_olustur_yerel(a, "f", 1);
    BolgeBilgisi *y2 = bolge_olustur_yerel(a, "f", 1);
    BolgeBilgisi *l = bolge_lca(a, y1, y2);
    test_sonuc("lca: yerel(f), yerel(f) -> yerel(f)",
               l && l->kategori == BOLGE_YEREL);
    arena_serbest(a);
}

static void test_lca_farkli_omur(void) {
    Arena *a = arena_olustur(0);
    BolgeBilgisi *i = bolge_olustur_iterasyon(a, 1);
    BolgeBilgisi *y = bolge_olustur_yerel(a, "f", 1);
    BolgeBilgisi *l = bolge_lca(a, i, y);
    /* Daha uzun olan: yerel */
    test_sonuc("lca: iterasyon, yerel -> yerel",
               l && l->kategori == BOLGE_YEREL);
    arena_serbest(a);
}

static void test_lca_yerel_global(void) {
    Arena *a = arena_olustur(0);
    BolgeBilgisi *y = bolge_olustur_yerel(a, "f", 1);
    BolgeBilgisi *g = bolge_olustur_basit(a, BOLGE_GLOBAL);
    BolgeBilgisi *l = bolge_lca(a, y, g);
    test_sonuc("lca: yerel, global -> global",
               l && l->kategori == BOLGE_GLOBAL);
    arena_serbest(a);
}

/* === Kategori adlari === */

static void test_kategori_adlari(void) {
    int ok = (strcmp(bolge_kategorisi_adi(BOLGE_YEREL), "YEREL") == 0)
          && (strcmp(bolge_kategorisi_adi(BOLGE_CAGIRAN), "CAGIRAN") == 0)
          && (strcmp(bolge_kategorisi_adi(BOLGE_GLOBAL), "GLOBAL") == 0);
    test_sonuc("kategori adlari", ok);
}

/* === Yazdirma cokme yok === */

static void test_yazdir(void) {
    Arena *a = arena_olustur(0);
    BolgeBilgisi *y = bolge_olustur_yerel(a, "fibonacci", 9);
    BolgeBilgisi *i = bolge_olustur_iterasyon(a, 3);
    BolgeBilgisi *g = bolge_olustur_basit(a, BOLGE_GLOBAL);

    FILE *bos = fopen("NUL", "w");
    if (!bos) bos = stdout;
    bolge_yazdir(y, bos);
    bolge_yazdir(i, bos);
    bolge_yazdir(g, bos);
    bolge_yazdir(NULL, bos);  /* NULL guvenli */
    if (bos != stdout) fclose(bos);
    test_sonuc("bolge_yazdir cokmuyor", 1);
    arena_serbest(a);
}

/* === Katman 2: Concurrency === */

static void test_olustur_sahip(void) {
    Arena *a = arena_olustur(0);
    BolgeBilgisi *b = bolge_olustur_sahip(a, 7);
    int ok = b && b->kategori == BOLGE_SAHIP
          && b->veri.sahip.thread_id == 7;
    test_sonuc("bolge_olustur_sahip(7)", ok);
    arena_serbest(a);
}

static void test_olustur_kanal(void) {
    Arena *a = arena_olustur(0);
    BolgeBilgisi *b = bolge_olustur_kanal(a, 3);
    int ok = b && b->kategori == BOLGE_KANAL
          && b->veri.kanal.kanal_id == 3;
    test_sonuc("bolge_olustur_kanal(3)", ok);
    arena_serbest(a);
}

static void test_sahiplik_transfer(void) {
    Arena *a = arena_olustur(0);
    BolgeBilgisi *eski = bolge_olustur_sahip(a, 1);
    BolgeBilgisi *yeni = bolge_sahiplik_transfer(a, eski, 2);
    int ok = yeni && yeni->kategori == BOLGE_SAHIP
          && yeni->veri.sahip.thread_id == 2;
    test_sonuc("R-GOREV: sahiplik transfer t1 -> t2", ok);
    arena_serbest(a);
}

static void test_kanal_gonder(void) {
    Arena *a = arena_olustur(0);
    BolgeBilgisi *yerel = bolge_olustur_yerel(a, "f", 1);
    BolgeBilgisi *kanalda = bolge_kanal_gonder(a, yerel, 5);
    int ok = kanalda && kanalda->kategori == BOLGE_KANAL
          && kanalda->veri.kanal.kanal_id == 5;
    test_sonuc("R-KANAL: yerel -> kanal(5)", ok);
    arena_serbest(a);
}

static void test_donmus_default(void) {
    Arena *a = arena_olustur(0);
    BolgeBilgisi *b = bolge_olustur_yerel(a, "f", 1);
    int ok = bolge_donmus_mu(b) == 0;
    test_sonuc("bolge_donmus_mu default 0 (R-PAYLAS v2'de)", ok);
    arena_serbest(a);
}

/* === Stres === */

static void test_stres(void) {
    Arena *a = arena_olustur(0);
    int ok = 1;
    for (int i = 0; i < 1000; i++) {
        BolgeBilgisi *b = bolge_olustur_iterasyon(a, i);
        if (!b || b->veri.iterasyon.dongu_id != i) { ok = 0; break; }
    }
    test_sonuc("1000 iterasyon bolgesi (ASan temiz)", ok);
    arena_serbest(a);
}

/* === Main === */

int main(void) {
    printf("KEMGU Bolge Sistemi Test Paketi\n");
    printf("================================\n");

    printf("\n--- Olusturma ---\n");
    test_olustur_basit();
    test_olustur_yerel();
    test_olustur_cagiran();
    test_olustur_iterasyon();

    printf("\n--- Esitlik ---\n");
    test_esit_global();
    test_esit_yerel_ayni();
    test_esit_yerel_farkli();
    test_esit_iterasyon();

    printf("\n--- Omur Sirasi ---\n");
    test_omur_iterasyon_yerel();
    test_omur_yerel_cagiran();
    test_omur_cagiran_global();

    printf("\n--- LCA (R-KOSUL icin) ---\n");
    test_lca_ayni();
    test_lca_farkli_omur();
    test_lca_yerel_global();

    printf("\n--- Yardimcilar ---\n");
    test_kategori_adlari();
    test_yazdir();

    printf("\n--- Katman 2: Concurrency ---\n");
    test_olustur_sahip();
    test_olustur_kanal();
    test_sahiplik_transfer();
    test_kanal_gonder();
    test_donmus_default();

    printf("\n--- Stres ---\n");
    test_stres();

    printf("\n================================\n");
    printf("Toplam: %d | Basarili: %d | Basarisiz: %d\n",
           toplam_test, basarili, basarisiz);
    return basarisiz > 0 ? 1 : 0;
}
