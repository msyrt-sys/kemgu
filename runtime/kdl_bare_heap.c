/*
 * KEMGU Bare-Metal Heap — freestanding malloc/free + memcpy/memset + global bölge
 * ===============================================================================
 *
 * C1 (OS region backing): host libc YOK. Bölge runtime'ı (kdl_bolge.c) ve
 * codegen-emit ham @malloc (bölge_al / closure env) bare-metal'de bu dosyadaki
 * sembollere link olur. Backing = linker script'in rezerve ettiği fiziksel RAM
 * bölgesi (__heap_start .. __heap_end) üzerinde bump + serbest-liste.
 *
 * Model: her tahsis 16-hizalı, başında {boyut, sonraki} header. free() bloğu
 * serbest-listeye iter; malloc() önce ilk-uyan serbest bloğu arar — region 64KB
 * blokları aynı boyutta olduğundan kernel-loop'ta F4.3 per-iter region-free
 * frame'leri geri kazandırır (= sınırlı bellek). Uyan blok yoksa bump'lar.
 *
 * Sınırlar (v1, bilinçli): split/coalesce YOK (ilk-uyum bloğu bütün döner →
 * boyut farkı iç-fragmentasyon; aynı-boyut geri kazanım kernel-loop için
 * yeterli). Tek-thread (concurrency Katman 2). bireysel free bölge modelinde
 * zaten gerekmez — kdl_bolge_serbest blokları + ham @malloc'u karşılar.
 *
 * CODEGEN'E DOKUNULMAZ: üretilen IR aynı @kdl_bolge_olustur/@kdl_bolge_serbest/
 * @kdl_global_bolge_al/@malloc sembollerini çağırır; yalnız backing değişir.
 */
#include <stdint.h>
#include <stddef.h>
#include "kdl_bolge.h"

/* Linker script (linker/bare-metal-*.ld) heap bölgesini rezerve eder. */
extern unsigned char __heap_start[];
extern unsigned char __heap_end[];

/* K1 (D-260): KEMGU_KEM_MALLOC tanımlıysa malloc/free/kem_heap_kur SAF-.kem'den
 * gelir (runtime/kem_heap.kem → çıplak C-ABI @malloc/@free/@kem_heap_kur). Bu dosya
 * o zaman yalnız memcpy/memset/kdl_global_bolge_al/kdl_panik/kdl_dizi sağlar. kem_os
 * bu varyantı (-DKEMGU_KEM_MALLOC) + kem_heap.o ile linkler → C bump-allocator = 0.
 * Diğer bare-metal kernel'ler bayrağı SET ETMEZ → C malloc/free + weak kem_heap_kur
 * no-op (boot `bl kem_heap_kur` çağırır; C-malloc lazy kdl_heap_init'le kendini kurar). */
#ifndef KEMGU_KEM_MALLOC

typedef struct KdlHeapBlok {
    size_t boyut;                 /* header dahil toplam blok boyutu (bayt) */
    struct KdlHeapBlok *sonraki;  /* serbest-liste bağlantısı */
} KdlHeapBlok;

#define KDL_HEAP_HIZA 16u

static unsigned char *kdl_heap_bump = 0;   /* bir sonraki bump adresi */
static unsigned char *kdl_heap_son  = 0;   /* heap üst sınırı (dışlayıcı) */
static KdlHeapBlok    *kdl_heap_bos  = 0;   /* serbest-liste başı (LIFO) */

static void kdl_heap_init(void) {
    if (!kdl_heap_bump) {
        kdl_heap_bump = __heap_start;
        kdl_heap_son  = __heap_end;
    }
}

/* Boot _start `bl kem_heap_kur(taban, son)` çağırır. C-malloc kernel'lerinde no-op
 * (lazy kdl_heap_init yeterli); .kem-malloc kem_os'ta kem_heap.o'daki STRONG
 * @kem_heap_kur bu weak'i override eder (linker: strong > weak). */
__attribute__((weak)) void kem_heap_kur(long taban, long son) {
    (void)taban; (void)son;
}

void *malloc(size_t n) {
    kdl_heap_init();
    if (n == 0) n = 1;                       /* 0-bayt → benzersiz geçerli ptr */

    /* header + istek, 16-hizalı toplam blok boyutu. */
    size_t toplam = (n + sizeof(KdlHeapBlok) + (KDL_HEAP_HIZA - 1))
                    & ~(size_t)(KDL_HEAP_HIZA - 1);
    if (toplam < n) return 0;                /* toplama taşması */

    /* 1) Serbest-liste ilk-uyum. Header'ı KORU (boyut bilgisi sonraki free
     *    için geçerli kalmalı) → bloğu bütün ver, böl/küçült yok. */
    KdlHeapBlok **pp = &kdl_heap_bos;
    for (KdlHeapBlok *b = kdl_heap_bos; b; b = b->sonraki) {
        if (b->boyut >= toplam) {
            *pp = b->sonraki;                /* listeden çıkar */
            return (void *)((unsigned char *)b + sizeof(KdlHeapBlok));
        }
        pp = &b->sonraki;
    }

    /* 2) Bump tahsis (16-hizalı, sınır + taşma korumalı). */
    uintptr_t cur  = (uintptr_t)kdl_heap_bump;
    uintptr_t ahiz = (cur + (KDL_HEAP_HIZA - 1)) & ~(uintptr_t)(KDL_HEAP_HIZA - 1);
    if (ahiz < cur) return 0;                                  /* hiza taşması */
    if (ahiz + toplam < ahiz) return 0;                        /* uç taşması */
    if (ahiz + toplam > (uintptr_t)kdl_heap_son) return 0;     /* OOM */
    kdl_heap_bump = (unsigned char *)(ahiz + toplam);

    KdlHeapBlok *h = (KdlHeapBlok *)ahiz;
    h->boyut = toplam;
    h->sonraki = 0;
    return (void *)((unsigned char *)ahiz + sizeof(KdlHeapBlok));
}

void free(void *p) {
    if (!p) return;
    KdlHeapBlok *h = (KdlHeapBlok *)((unsigned char *)p - sizeof(KdlHeapBlok));
    h->sonraki = kdl_heap_bos;               /* LIFO serbest-listeye it */
    kdl_heap_bos = h;
}

/* Freestanding memcpy/memset — libc yok; clang struct-kopya / kdl_dizi_buyut
 * bunları çağırır. K2 (D-262): kem_os'ta .kem çıplak memcpy/memset (kem_heap.o)
 * sağlar → burada guard-içi (diğer kernel'ler C sürümünü kullanır). */
void *memcpy(void *dst, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dst;
}

void *memset(void *dst, int c, size_t n) {
    unsigned char *d = (unsigned char *)dst;
    while (n--) *d++ = (unsigned char)c;
    return dst;
}

#endif  /* !KEMGU_KEM_MALLOC — malloc/free/memcpy/memset/kem_heap_kur .kem'den (kem_os) */

/* === Global bölge — kdl_runtime.c host eşinin bare-metal freestanding kopyası.
 * main() @kdl_global_bolge_al çağırır (her program). Tek lazy global bölge,
 * program ömrü boyu (status-quo leak; F4.4 dışı). K4b (D-263): kem_os'ta .kem
 * çıplak kdl_global_bolge_al (kem_heap.o) → guard-içi (diğer kernel'ler C). */
#ifndef KEMGU_KEM_MALLOC
static KdlBolge *kdl_global_bolge = 0;

KdlBolge *kdl_global_bolge_al(void) {
    if (!kdl_global_bolge) kdl_global_bolge = kdl_bolge_olustur();
    return kdl_global_bolge;
}
#endif  /* !KEMGU_KEM_MALLOC — kdl_global_bolge_al .kem'den (kem_os) */

/* === Evrensel panik (seam) — kdl_dizi_oob (kdl_dizi.inc) + codegen inline-OOB
 * (src/llvm.c) buraya çağırır. Bare-metal: UART "PANIK:" + CPU halt
 * (runtime/kdl_runtime_panik.c → kdl_panik_dur). Host eşi kdl_runtime.c
 * (stderr+abort). === */
__attribute__((noreturn)) void kdl_panik_dur(const char *);

__attribute__((noreturn)) void kdl_panik(const char *mesaj) {
    kdl_panik_dur(mesaj);
}

/* === Dizi runtime (KdlDizi + kdl_dizi_*) — host (kdl_runtime.c) ile TEK KAYNAK.
 * Bağımlılıklar yukarıda hazır: memcpy, kdl_global_bolge_al, kdl_panik;
 * kdl_bolge_ayir kdl_bolge.h'den. K2 (D-262): kem_os'ta .kem çıplak dizi runtime
 * (kem_heap.o) sağlar → guard-içi (diğer kernel'ler C kdl_dizi.inc kullanır). === */
#ifndef KEMGU_KEM_MALLOC
#include "kdl_dizi.inc"
#endif
