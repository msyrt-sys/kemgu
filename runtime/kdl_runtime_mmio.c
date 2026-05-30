/*
 * KEMGU MMIO Foundation — Runtime Implementasyonu (kdl_runtime_mmio.c)
 * ====================================================================
 *
 * Iki derleme modu (kdl_runtime_uart_pl011.c ile ayni desen):
 *
 *   -DKEMGU_BARE_METAL  -> gercek MMIO: *(volatile uint32_t *)(uintptr_t)adres
 *                          Bu, PL011/16550 surucusunun kullandigi volatile
 *                          load/store idiomudur. Derleyici bu erisimi optimize
 *                          edemez/yeniden siralayamaz; donanim register'ina
 *                          tam olarak yazilan/okunan deger gider/gelir.
 *
 *   (varsayilan / host) -> global tampon mock. Adres 4-byte hizali kelime
 *                          indeksine eslenir (mod tampon boyu). Boylece keyfi
 *                          cihaz adresi (orn. 0x0A000000) host uzerinde
 *                          guvenle yazilip okunabilir; yaz-sonra-oku
 *                          deterministik round-trip yapar. ASan temiz.
 *
 * Bu dosya BARE-METAL guvenlidir: <stdint.h> disinda bagimliligi yoktur,
 * malloc/printf/syscall kullanmaz. Host modunda yalniz statik global tampon.
 */

#include "kdl_mmio.h"

#include <stdint.h>

#ifdef KEMGU_BARE_METAL

/* === Bare-metal: gercek volatile MMIO erisimi (PL011 idiomu) === */

uint32_t kdl_mmio_oku32(uint64_t adres) {
    return *(volatile uint32_t *)(uintptr_t)adres;
}

void kdl_mmio_yaz32(uint64_t adres, uint32_t deger) {
    *(volatile uint32_t *)(uintptr_t)adres = deger;
}

#else /* !KEMGU_BARE_METAL */

/* === Host/mock: segfault-siz global tampon === */

#define KDL_MMIO_MOCK_KELIME 4096u   /* 16 KiB pencere (4096 x 4 byte) */

static uint32_t kdl_mmio_mock[KDL_MMIO_MOCK_KELIME];

/* adres -> kelime indeksi: 4-byte hizali, tampon boyuna gore mod. */
static uint32_t kdl_mmio_indeks(uint64_t adres) {
    return (uint32_t)((adres >> 2) % (uint64_t)KDL_MMIO_MOCK_KELIME);
}

uint32_t kdl_mmio_oku32(uint64_t adres) {
    return kdl_mmio_mock[kdl_mmio_indeks(adres)];
}

void kdl_mmio_yaz32(uint64_t adres, uint32_t deger) {
    kdl_mmio_mock[kdl_mmio_indeks(adres)] = deger;
}

void kdl_mmio_mock_sifirla(void) {
    for (uint32_t i = 0; i < KDL_MMIO_MOCK_KELIME; i++) {
        kdl_mmio_mock[i] = 0;
    }
}

uint32_t kdl_mmio_mock_oku_ham(uint64_t adres) {
    return kdl_mmio_mock[kdl_mmio_indeks(adres)];
}

uint32_t kdl_mmio_mock_kelime_sayisi(void) {
    return KDL_MMIO_MOCK_KELIME;
}

#endif /* KEMGU_BARE_METAL */
