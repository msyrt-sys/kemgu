#include "arena.h"

#include <stdlib.h>   /* malloc, free */
#include <string.h>   /* memset */
#include <stddef.h>   /* max_align_t */

#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 201112L
#error "Arena allocator C11 gerektirir (_Alignof, max_align_t)"
#endif

/* Hizalama birimi — C11 standart maksimum hizalama. */
#define ARENA_HIZA _Alignof(max_align_t)

typedef struct ArenaBlok {
    char *veri;                 /* tahsis edilen ham bellek */
    size_t boyut;               /* veri kapasitesi (byte) */
    size_t cursor;              /* su ana kadar kullanilan byte */
    struct ArenaBlok *sonraki;  /* zincirin bir sonraki blogu (NULL = son) */
} ArenaBlok;

struct Arena {
    ArenaBlok *ilk_blok;        /* zincirin basi — sifirlama sonrasi korunur */
    ArenaBlok *simdiki_blok;    /* tahsis yapilan son blok */
    int hata;                   /* allocation basarisiz olduysa 1 */
};

/* === Yardimci fonksiyonlar === */

/* n'i ARENA_HIZA katina yukari yuvarla. */
static size_t hizala_yukari(size_t n) {
    const size_t maske = (size_t)ARENA_HIZA - 1u;
    return (n + maske) & ~maske;
}

/* Yeni bir blok olustur (malloc + ham bellek). Basarisiz -> NULL. */
static ArenaBlok *blok_olustur(size_t istenen_boyut) {
    ArenaBlok *blok = (ArenaBlok *)malloc(sizeof(ArenaBlok));
    if (!blok) return NULL;

    blok->veri = (char *)malloc(istenen_boyut);
    if (!blok->veri) {
        free(blok);
        return NULL;
    }

    blok->boyut = istenen_boyut;
    blok->cursor = 0;
    blok->sonraki = NULL;
    return blok;
}

/* Bir blogun butun bellegini serbest birak. */
static void blok_serbest(ArenaBlok *blok) {
    if (!blok) return;
    free(blok->veri);
    free(blok);
}

/*
 * Sonraki blok boyutunu hesapla:
 *   geometric x2, MAX cap'li.
 *   Eger istenen tahsis cap'ten buyukse o tahsis boyutunu kullan (ozel blok).
 */
static size_t sonraki_blok_boyutu(size_t mevcut_boyut, size_t istenen_tahsis) {
    size_t buyuyen = mevcut_boyut * 2u;

    if (buyuyen > ARENA_MAX_BLOK_BOYUTU) {
        buyuyen = ARENA_MAX_BLOK_BOYUTU;
    }
    if (buyuyen < istenen_tahsis) {
        buyuyen = istenen_tahsis;  /* cap ustu tek tahsis -> ozel blok */
    }
    return buyuyen;
}

/* === Public API === */

Arena *arena_olustur(size_t baslangic_boyutu) {
    if (baslangic_boyutu == 0) {
        baslangic_boyutu = ARENA_VARSAYILAN_BLOK_BOYUTU;
    }

    Arena *arena = (Arena *)malloc(sizeof(Arena));
    if (!arena) return NULL;

    ArenaBlok *blok = blok_olustur(baslangic_boyutu);
    if (!blok) {
        free(arena);
        return NULL;
    }

    arena->ilk_blok = blok;
    arena->simdiki_blok = blok;
    arena->hata = 0;
    return arena;
}

void arena_serbest(Arena *arena) {
    if (!arena) return;

    ArenaBlok *blok = arena->ilk_blok;
    while (blok) {
        ArenaBlok *sonraki = blok->sonraki;
        blok_serbest(blok);
        blok = sonraki;
    }
    free(arena);
}

void arena_sifirla(Arena *arena) {
    if (!arena || !arena->ilk_blok) return;

    /* Ilk blogun sonrasini hepsini serbest birak. */
    ArenaBlok *blok = arena->ilk_blok->sonraki;
    while (blok) {
        ArenaBlok *sonraki = blok->sonraki;
        blok_serbest(blok);
        blok = sonraki;
    }

    arena->ilk_blok->sonraki = NULL;
    arena->ilk_blok->cursor = 0;
    arena->simdiki_blok = arena->ilk_blok;
    arena->hata = 0;
}

void *arena_ayir(Arena *arena, size_t boyut) {
    if (!arena) return NULL;
    if (boyut == 0) return NULL;
    if (arena->hata) return NULL;

    size_t hizali_boyut = hizala_yukari(boyut);

    /* Overflow koruma (ARENA_HIZA-1 ekleme tasarsa) */
    if (hizali_boyut < boyut) {
        arena->hata = 1;
        return NULL;
    }

    ArenaBlok *blok = arena->simdiki_blok;

    /* Mevcut blokta yer var mi? */
    if (blok->cursor + hizali_boyut <= blok->boyut) {
        void *ptr = blok->veri + blok->cursor;
        blok->cursor += hizali_boyut;
        return ptr;
    }

    /* Yer yok — yeni blok olustur. */
    size_t yeni_boyut = sonraki_blok_boyutu(blok->boyut, hizali_boyut);
    ArenaBlok *yeni_blok = blok_olustur(yeni_boyut);
    if (!yeni_blok) {
        arena->hata = 1;
        return NULL;
    }

    blok->sonraki = yeni_blok;
    arena->simdiki_blok = yeni_blok;

    void *ptr = yeni_blok->veri;
    yeni_blok->cursor = hizali_boyut;
    return ptr;
}

void *arena_ayir_sifir(Arena *arena, size_t boyut) {
    void *ptr = arena_ayir(arena, boyut);
    if (ptr) memset(ptr, 0, boyut);
    return ptr;
}

size_t arena_kullanilan_byte(const Arena *arena) {
    if (!arena) return 0;

    size_t toplam = 0;
    for (const ArenaBlok *blok = arena->ilk_blok; blok; blok = blok->sonraki) {
        toplam += blok->cursor;
    }
    return toplam;
}

size_t arena_toplam_byte(const Arena *arena) {
    if (!arena) return 0;

    size_t toplam = 0;
    for (const ArenaBlok *blok = arena->ilk_blok; blok; blok = blok->sonraki) {
        toplam += blok->boyut;
    }
    return toplam;
}

int arena_hata_var_mi(const Arena *arena) {
    if (!arena) return 0;
    return arena->hata;
}
