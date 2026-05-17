#include "bolge.h"

#include <string.h>

/* === Olusturucular === */

BolgeBilgisi *bolge_olustur_basit(Arena *a, BolgeKategorisi k) {
    BolgeBilgisi *b = (BolgeBilgisi *)arena_ayir_sifir(a, sizeof(BolgeBilgisi));
    if (!b) return NULL;
    b->kategori = k;
    return b;
}

BolgeBilgisi *bolge_olustur_yerel(Arena *a, const char *islev_adi, int uz) {
    BolgeBilgisi *b = bolge_olustur_basit(a, BOLGE_YEREL);
    if (!b) return NULL;
    b->veri.yerel.islev_adi = islev_adi;
    b->veri.yerel.adi_uzunluk = uz;
    return b;
}

BolgeBilgisi *bolge_olustur_cagiran(Arena *a, const char *islev_adi, int uz) {
    BolgeBilgisi *b = bolge_olustur_basit(a, BOLGE_CAGIRAN);
    if (!b) return NULL;
    b->veri.cagiran.islev_adi = islev_adi;
    b->veri.cagiran.adi_uzunluk = uz;
    return b;
}

BolgeBilgisi *bolge_olustur_iterasyon(Arena *a, int dongu_id) {
    BolgeBilgisi *b = bolge_olustur_basit(a, BOLGE_ITERASYON);
    if (!b) return NULL;
    b->veri.iterasyon.dongu_id = dongu_id;
    return b;
}

BolgeBilgisi *bolge_olustur_sahip(Arena *a, int thread_id) {
    BolgeBilgisi *b = bolge_olustur_basit(a, BOLGE_SAHIP);
    if (!b) return NULL;
    b->veri.sahip.thread_id = thread_id;
    return b;
}

BolgeBilgisi *bolge_olustur_kanal(Arena *a, int kanal_id) {
    BolgeBilgisi *b = bolge_olustur_basit(a, BOLGE_KANAL);
    if (!b) return NULL;
    b->veri.kanal.kanal_id = kanal_id;
    return b;
}

/* === Iliskiler === */

int bolge_esit(const BolgeBilgisi *a, const BolgeBilgisi *b) {
    if (!a || !b) return 0;
    if (a == b) return 1;
    if (a->kategori != b->kategori) return 0;

    switch (a->kategori) {
        case BOLGE_LIT:
        case BOLGE_GLOBAL:
        case BOLGE_BILINMIYOR:
        case BOLGE_HATA:
            return 1;
        case BOLGE_YEREL:
            return a->veri.yerel.adi_uzunluk == b->veri.yerel.adi_uzunluk
                && memcmp(a->veri.yerel.islev_adi,
                          b->veri.yerel.islev_adi,
                          (size_t)a->veri.yerel.adi_uzunluk) == 0;
        case BOLGE_CAGIRAN:
            return a->veri.cagiran.adi_uzunluk == b->veri.cagiran.adi_uzunluk
                && memcmp(a->veri.cagiran.islev_adi,
                          b->veri.cagiran.islev_adi,
                          (size_t)a->veri.cagiran.adi_uzunluk) == 0;
        case BOLGE_ITERASYON:
            return a->veri.iterasyon.dongu_id == b->veri.iterasyon.dongu_id;
        case BOLGE_SAHIP:
            return a->veri.sahip.thread_id == b->veri.sahip.thread_id;
        case BOLGE_KANAL:
            return a->veri.kanal.kanal_id == b->veri.kanal.kanal_id;
    }
    return 0;
}

/* Bolge omur sirasi (sayisal — kucuk = kisa omur):
 *   LIT/BILINMIYOR/HATA = -1 (uygulanmaz)
 *   ITERASYON = 1
 *   YEREL     = 2
 *   CAGIRAN   = 3
 *   GLOBAL    = 4
 *   SAHIP/KANAL: thread bagli, simdilik 2 (yerel benzeri) */
static int omur_sirasi(const BolgeBilgisi *b) {
    if (!b) return -1;
    switch (b->kategori) {
        case BOLGE_ITERASYON: return 1;
        case BOLGE_YEREL:     return 2;
        case BOLGE_CAGIRAN:   return 3;
        case BOLGE_GLOBAL:    return 4;
        case BOLGE_SAHIP:     return 2;
        case BOLGE_KANAL:     return 2;
        default:              return -1;
    }
}

int bolge_omru_kisa_mi(const BolgeBilgisi *a, const BolgeBilgisi *b) {
    int ka = omur_sirasi(a);
    int kb = omur_sirasi(b);
    if (ka < 0 || kb < 0) return 0;
    return ka <= kb;
}

BolgeBilgisi *bolge_lca(Arena *a, const BolgeBilgisi *b1,
                        const BolgeBilgisi *b2) {
    if (!b1) return (BolgeBilgisi *)b2;
    if (!b2) return (BolgeBilgisi *)b1;
    if (bolge_esit(b1, b2)) return (BolgeBilgisi *)b1;

    /* LCA: daha uzun omur — koşullu dallanma icin */
    int o1 = omur_sirasi(b1);
    int o2 = omur_sirasi(b2);
    if (o1 < 0 || o2 < 0) return bolge_olustur_basit(a, BOLGE_HATA);

    /* Daha uzun omurlu bolgeyi don */
    return (o1 >= o2) ? (BolgeBilgisi *)b1 : (BolgeBilgisi *)b2;
}

/* === Yazdirma === */

void bolge_yazdir(const BolgeBilgisi *b, FILE *out) {
    if (!b) { fputs("(NULL)", out); return; }

    switch (b->kategori) {
        case BOLGE_LIT:         fputs("rho_lit", out); return;
        case BOLGE_GLOBAL:      fputs("rho_global", out); return;
        case BOLGE_BILINMIYOR:  fputs("rho_?", out); return;
        case BOLGE_HATA:        fputs("rho_HATA", out); return;
        case BOLGE_YEREL:
            fputs("rho_yerel(", out);
            if (b->veri.yerel.islev_adi) {
                fwrite(b->veri.yerel.islev_adi, 1,
                       (size_t)b->veri.yerel.adi_uzunluk, out);
            }
            fputc(')', out);
            return;
        case BOLGE_CAGIRAN:
            fputs("rho_cagiran(", out);
            if (b->veri.cagiran.islev_adi) {
                fwrite(b->veri.cagiran.islev_adi, 1,
                       (size_t)b->veri.cagiran.adi_uzunluk, out);
            }
            fputc(')', out);
            return;
        case BOLGE_ITERASYON:
            fprintf(out, "rho_iterasyon(d%d)", b->veri.iterasyon.dongu_id);
            return;
        case BOLGE_SAHIP:
            fprintf(out, "rho_sahip(t%d)", b->veri.sahip.thread_id);
            return;
        case BOLGE_KANAL:
            fprintf(out, "rho_kanal(k%d)", b->veri.kanal.kanal_id);
            return;
    }
    fputs("(BILINMEYEN)", out);
}

const char *bolge_kategorisi_adi(BolgeKategorisi k) {
    switch (k) {
        case BOLGE_LIT:        return "LIT";
        case BOLGE_YEREL:      return "YEREL";
        case BOLGE_CAGIRAN:    return "CAGIRAN";
        case BOLGE_ITERASYON:  return "ITERASYON";
        case BOLGE_GLOBAL:     return "GLOBAL";
        case BOLGE_SAHIP:      return "SAHIP";
        case BOLGE_KANAL:      return "KANAL";
        case BOLGE_BILINMIYOR: return "BILINMIYOR";
        case BOLGE_HATA:       return "HATA";
    }
    return "BILINMEYEN";
}
