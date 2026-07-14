/*
 * kem_malloc_kompozisyon.c — F3 (D-259) KOMPOZİSYON KANITI harness.
 * ============================================================================
 * SAF-.kem çıplak+küresel allocator'ı (test/ornekler/kem_malloc.kem) sürücü.
 * Çıplak fn'ler artık TRUE C-ABI (ρ param YOK, D-257/258) → C bu .kem
 * sembollerini doğrudan çağırır. Bu, "circularity kırıldı → saf-.kem runtime
 * yolu açık" iddiasının falsifiye-kanıtıdır:
 *
 *   (1) kem_malloc IR'inde @kdl_bolge_olustur/@malloc self-call = 0 (ayrı grep
 *       kanıtı) → tahsis kendini tetiklemez, SONSUZ RECURSION YOK.
 *   (2) 2 çağrı → 2 FARKLI geçerli adres (bump ilerler).
 *   (3) Dönen adresler KULLANILABİLİR (inttoptr + deref-write/read).
 *
 * Çıktı: "ALLOC: p1=<A> p2=<B> A!=B EVET" + exit 42 (spec-formatı).
 * Havuz statik C tamponu (host); bare-metal'de taban __heap_start'tan gelir (K1b).
 */
#include <stdint.h>
#include <stdio.h>

/* Saf-.kem çıplak allocator sembolleri (C-ABI, ρ-suz). */
extern int32_t kem_heap_kur(int64_t taban);
extern int64_t kem_malloc(int64_t n);
extern int32_t kem_yaz32(int64_t adres, int32_t deg);
extern int32_t kem_oku32(int64_t adres);

/* Host backing — bare-metal'de linker-rezerve RAM (__heap_start..__heap_end). */
static unsigned char havuz[65536];

int main(void) {
    kem_heap_kur((int64_t)(intptr_t)havuz);

    int64_t p1 = kem_malloc(8);
    int64_t p2 = kem_malloc(8);

    /* Dönen adresler kullanılabilir mi: yaz + geri oku. */
    kem_yaz32(p1, 111);
    kem_yaz32(p2, 222);
    int32_t v1 = kem_oku32(p1);
    int32_t v2 = kem_oku32(p2);

    int farkli = (p1 != p2);
    int yazilabilir = (v1 == 111) && (v2 == 222);
    /* p2 - p1 == 8 (bump adımı) — üst üste binme yok. */
    int bitisik = ((p2 - p1) == 8);

    printf("ALLOC: p1=%lld p2=%lld A!=B %s\n",
           (long long)p1, (long long)p2, farkli ? "EVET" : "HAYIR");
    printf("YAZ/OKU: v1=%d v2=%d yazilabilir=%s bitisik(8)=%s\n",
           v1, v2, yazilabilir ? "EVET" : "HAYIR", bitisik ? "EVET" : "HAYIR");

    if (farkli && yazilabilir && bitisik) return 42;
    return 1;
}
