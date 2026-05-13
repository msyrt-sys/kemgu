#ifndef KEMGU_BOLGE_H
#define KEMGU_BOLGE_H

#include "arena.h"

#include <stdio.h>

/*
 * KEMGU Bolge (Region) Sistemi
 * ============================
 *
 * KEMGU_Bellek_Modeli.md formalizasyonuna gore (Katman 1):
 *
 *   ρ_yerel(f)       — fonksiyon f'nin yerel bolgesi
 *   ρ_cagiran(f)     — f'yi cagiran fonksiyonun bolgesi
 *   ρ_iterasyon(d)   — dongu d'nin iterasyon bolgesi
 *   ρ_global         — program omru
 *   ρ_sahip(t)       — thread t'ye ait bolge (Katman 2)
 *   ρ_kanal(k)       — kanal k'nin transfer tamponu (Katman 2)
 *
 * Bolge omru aksiyomlari (kısa <= uzun):
 *   ρ_iterasyon(d) <= ρ_yerel(f)    (d, f icindeyse)
 *   ρ_yerel(f)     <= ρ_cagiran(f)
 *   ρ_yerel(f)     <= ρ_global
 */

typedef enum {
    BOLGE_LIT,            /* basit literal — bolge yok (stack) */
    BOLGE_YEREL,          /* ρ_yerel(f) */
    BOLGE_CAGIRAN,        /* ρ_cagiran(f) */
    BOLGE_ITERASYON,      /* ρ_iterasyon(d) */
    BOLGE_GLOBAL,         /* ρ_global */
    BOLGE_SAHIP,          /* ρ_sahip(t) — Katman 2 */
    BOLGE_KANAL,          /* ρ_kanal(k) — Katman 2 */
    BOLGE_BILINMIYOR,     /* henuz cozulmemis */
    BOLGE_HATA,           /* cozumleme hatasi */
} BolgeKategorisi;

typedef struct BolgeBilgisi {
    BolgeKategorisi kategori;
    union {
        struct {
            const char *islev_adi;   /* f'nin adi */
            int adi_uzunluk;
        } yerel;
        struct {
            const char *islev_adi;
            int adi_uzunluk;
        } cagiran;
        struct {
            int dongu_id;            /* tekil dongu id (parser'dan veya counter) */
        } iterasyon;
        struct {
            int thread_id;           /* tekil thread id */
        } sahip;
        struct {
            int kanal_id;
        } kanal;
    } veri;
} BolgeBilgisi;

/* === Olusturucular === */

BolgeBilgisi *bolge_olustur_basit(Arena *a, BolgeKategorisi k);
BolgeBilgisi *bolge_olustur_yerel(Arena *a, const char *islev_adi, int uz);
BolgeBilgisi *bolge_olustur_cagiran(Arena *a, const char *islev_adi, int uz);
BolgeBilgisi *bolge_olustur_iterasyon(Arena *a, int dongu_id);
BolgeBilgisi *bolge_olustur_sahip(Arena *a, int thread_id);
BolgeBilgisi *bolge_olustur_kanal(Arena *a, int kanal_id);

/* === Katman 2: Concurrency aksiyomlari ===
 *
 * S1 (Tekil Sahiplik): her bolge yalniz bir thread'e ait
 * S2 (Baslangic Sahipligi): yaratan thread sahiptir
 * S3 (Atomik Transfer): sahiplik atomik bir adimla devreder
 *
 * R-GOREV: closure thread'e geciyor -> yakalananlar sahip degisikligi
 * R-BIRLESTIR: thread sonu -> bolge donus + serbest birakma
 * R-KANAL: mesaj transferi -> sahiplik kanaldan alana gecer
 * R-PAYLAS: dondurulmus salt-okunur bolge -> birden cok okuyucu
 */

/* Sahiplik transferi: bolgenin sahibini t1 -> t2 yapar.
 * Mevcut bolgenin yeni bir kopyasini doner (immutable update tarzi). */
BolgeBilgisi *bolge_sahiplik_transfer(Arena *a, const BolgeBilgisi *b,
                                       int yeni_thread_id);

/* Kanal'a gonderim: bolge kanal_id'ye transfer edilir. */
BolgeBilgisi *bolge_kanal_gonder(Arena *a, const BolgeBilgisi *b,
                                  int kanal_id);

/* Donmus mu? — salt-okunur paylasim. Su an basit flag yok; v1'de daima 0. */
int bolge_donmus_mu(const BolgeBilgisi *b);

/* === Iliskiler === */

/* Esitlik (kategori + veri uyumu) */
int bolge_esit(const BolgeBilgisi *a, const BolgeBilgisi *b);

/* a, b'den daha kisa veya esit omurlu mu?
 * Aksiyomlar:
 *   ITERASYON < YEREL < CAGIRAN < GLOBAL */
int bolge_omru_kisa_mi(const BolgeBilgisi *a, const BolgeBilgisi *b);

/* Iki bolgenin LCA'si (koşullu dallanma icin — R-KOSUL).
 * Karsilastirma: omur sirasi → daha uzun olan dondurulur. */
BolgeBilgisi *bolge_lca(Arena *a, const BolgeBilgisi *b1,
                        const BolgeBilgisi *b2);

/* === Yazdirma === */

void bolge_yazdir(const BolgeBilgisi *b, FILE *out);
const char *bolge_kategorisi_adi(BolgeKategorisi k);

#endif /* KEMGU_BOLGE_H */
