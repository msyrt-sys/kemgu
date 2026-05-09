#ifndef KEMGU_ARENA_H
#define KEMGU_ARENA_H

#include <stddef.h>  /* size_t */

/*
 * KEMGU Arena Allocator
 * =====================
 *
 * Blok zinciri (linked list) tabanli arena tahsisci. AST ve Parser dugumleri
 * icin tasarlandi: pointer stabil, hizli tahsis, toplu serbest birakma.
 *
 * Kullanim modelleri:
 *   1) Tek seferlik:
 *        Arena *a = arena_olustur(0);
 *        ... arena_ayir(a, ...) ...
 *        arena_serbest(a);
 *
 *   2) Yeniden kullanim (ornegin LSP / her dosyada yeni AST):
 *        Arena *a = arena_olustur(0);
 *        ... arena_ayir(a, ...) ...
 *        arena_sifirla(a);   // ilk blok korunur, hizli reset
 *        ... arena_ayir(a, ...) ...
 *        arena_serbest(a);
 *
 * Hata yonetimi:
 *   arena_ayir basarisizsa NULL doner ve arena->hata bayragi 1 olur.
 *   Cagiranin sorumlulugu kontrol etmek (KEMGU motto: cokmezlik —
 *   kutuphane abort etmez, cagiran karar verir).
 *
 * Hizalama:
 *   _Alignof(max_align_t) — C11 standarti, tum C tipleri icin garantili.
 */

#define ARENA_VARSAYILAN_BLOK_BOYUTU  (64u * 1024u)            /*  64 KB */
#define ARENA_MAX_BLOK_BOYUTU         (16u * 1024u * 1024u)    /*  16 MB cap */

/* Opaque tip — ic yapi arena.c'de */
typedef struct Arena Arena;

/* === Yasam dongusu === */

/*
 * Yeni bir arena olustur.
 *   baslangic_boyutu = 0  -> ARENA_VARSAYILAN_BLOK_BOYUTU kullanilir
 * Donus: NULL (malloc basarisiz) veya gecerli arena pointer'i.
 */
Arena *arena_olustur(size_t baslangic_boyutu);

/*
 * Arena'yi ve tum bloklarini serbest birak.
 * NULL parametresi guvenli (no-op).
 */
void arena_serbest(Arena *arena);

/*
 * Tum bloklari sifirla: ilk blok haric digerlerini free et,
 * cursor'leri 0'a cek. Arena yeniden kullanilabilir hale gelir.
 * Hata bayragi da temizlenir.
 */
void arena_sifirla(Arena *arena);

/* === Tahsis === */

/*
 * Hizali bellek tahsisi (_Alignof(max_align_t)).
 *   boyut = 0  -> NULL (malloc gibi)
 * Mevcut blokta yer yoksa yeni blok eklenir (geometric x2, MAX cap'li).
 * Cap ustu tek tahsisler icin ozel blok yaratilir.
 * Donus: NULL (basarisiz, hata bayragi set) veya gecerli pointer.
 */
void *arena_ayir(Arena *arena, size_t boyut);

/*
 * arena_ayir + memset(0, boyut). Baslatilmamis bellek hatalarini onler.
 * Tercihen kullan (CLAUDE.md kurali).
 */
void *arena_ayir_sifir(Arena *arena, size_t boyut);

/* === Istatistik / Debug === */

/* Su ana kadar tahsis edilen toplam byte (hizalama padding'i dahil). */
size_t arena_kullanilan_byte(const Arena *arena);

/* Tum bloklarin toplam kapasitesi (kullanilan + bos). */
size_t arena_toplam_byte(const Arena *arena);

/* Allocation hatasi olduysa 1, yoksa 0. */
int arena_hata_var_mi(const Arena *arena);

#endif /* KEMGU_ARENA_H */
