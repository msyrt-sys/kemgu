/*
 * D1 testi (aarch64) — per-process adres-uzayı izolasyonu.
 *
 * 2 süreç (A, B) her biri AYRI sayfa tablosu. Kernel'i aynı (identity) görürler
 * ama user VA 0x42000000 FARKLI fiziksel sayfalara gider: A→0x44000000,
 * B→0x46000000. TTBR0 swap (kdl_ttbr_degis) + TLB flush ile adres-uzayı geçişi.
 *
 * Kanıt: AYNI sanal adrese (0x42000000) A 0xAA, B 0xBB yazar; geçişlerden sonra
 * A hâlâ 0xAA, B hâlâ 0xBB okur → birbirini ETKİLEMEZ = per-process izolasyon.
 */
#include <stdint.h>
extern void kdl_yazdir_metin(const char *);
extern void kdl_yazdir_satir(void);
extern void kdl_yazdir_onaltilik(uint64_t);
extern void kdl_surec_kur(uint64_t *l1, uint64_t *l2, uint64_t user_pa);
extern void kdl_ttbr_degis(uint64_t *l1);

static uint64_t l1_a[512] __attribute__((aligned(4096)));
static uint64_t l2_a[512] __attribute__((aligned(4096)));
static uint64_t l1_b[512] __attribute__((aligned(4096)));
static uint64_t l2_b[512] __attribute__((aligned(4096)));

int main(void) {
    kdl_yazdir_metin("D1 BASLA");
    kdl_yazdir_satir();

    volatile uint32_t *uva = (volatile uint32_t *)0x42000000UL;  /* ortak sanal adres */
    kdl_surec_kur(l1_a, l2_a, 0x44000000UL);   /* süreç A: user VA → PA 0x44000000 */
    kdl_surec_kur(l1_b, l2_b, 0x46000000UL);   /* süreç B: user VA → PA 0x46000000 */

    kdl_ttbr_degis(l1_a); *uva = 0xAA;         /* A'nın özel sayfası = 0xAA */
    kdl_ttbr_degis(l1_b); *uva = 0xBB;         /* B'nin özel sayfası = 0xBB */

    kdl_ttbr_degis(l1_a);                      /* A'ya geç */
    kdl_yazdir_metin("SUREC A uva=");
    kdl_yazdir_onaltilik(*uva);                /* 0xAA bekleniyor (B etkilemedi) */
    kdl_yazdir_satir();

    kdl_ttbr_degis(l1_b);                      /* B'ye geç */
    kdl_yazdir_metin("SUREC B uva=");
    kdl_yazdir_onaltilik(*uva);                /* 0xBB bekleniyor */
    kdl_yazdir_satir();
    return 0;
}
