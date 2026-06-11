/*
 * KEMGU MMIO Foundation — Runtime Implementasyonu (kdl_runtime_mmio.c)
 * ====================================================================
 *
 * Typed-width volatile MMIO load/store (16/32/64-bit). Iki derleme modu
 * (kdl_runtime_uart_pl011.c ile ayni desen):
 *
 *   -DKEMGU_BARE_METAL  -> gercek MMIO: *(volatile uintN_t *)(uintptr_t)adres
 *                          Bu, PL011/16550 surucusunun kullandigi volatile
 *                          load/store idiomudur. Derleyici bu erisimi optimize
 *                          edemez/yeniden siralayamaz; donanim register'ina
 *                          tam olarak yazilan/okunan deger gider/gelir. Native
 *                          erisim target byte sirasinda olur (ARM64/x86_64 LE).
 *
 *   (varsayilan / host) -> global tampon mock. Tampon BYTE-ADRESLENEBILIR bir
 *                          uint8_t dizisidir; adres tampon boyuna gore mod ile
 *                          byte ofsetine eslenir (eski (adres>>2) kelime-collapse
 *                          KALDIRILDI). 16/32/64-bit erisim ACIK little-endian
 *                          byte birlestirmesi yapar — host byte sirasindan
 *                          bagimsiz, bare-metal LE target ile birebir ayni.
 *                          Komsu byte adresleri (orn. taban+64 ve taban+66) artik
 *                          AYRI slotlara duser; le16 ring alanlari faithful test
 *                          edilir. Keyfi cihaz adresi (orn. 0x0A000000) host
 *                          uzerinde guvenle yazilip okunabilir; ASan temiz.
 *
 * Bu dosya BARE-METAL guvenlidir: <stdint.h> disinda bagimliligi yoktur,
 * malloc/printf/syscall kullanmaz. Host modunda yalniz statik global tampon.
 */

#include "kdl_mmio.h"

#include <stdint.h>

#ifdef KEMGU_BARE_METAL

/* === Bare-metal: gercek volatile MMIO erisimi (PL011 idiomu) ===
 * Native erisim, target byte sirasinda (ARM64/x86_64 = little-endian). */

uint16_t kdl_mmio_oku16(uint64_t adres) {
    return *(volatile uint16_t *)(uintptr_t)adres;
}

void kdl_mmio_yaz16(uint64_t adres, uint16_t deger) {
    *(volatile uint16_t *)(uintptr_t)adres = deger;
}

uint32_t kdl_mmio_oku32(uint64_t adres) {
    return *(volatile uint32_t *)(uintptr_t)adres;
}

void kdl_mmio_yaz32(uint64_t adres, uint32_t deger) {
    *(volatile uint32_t *)(uintptr_t)adres = deger;
}

uint64_t kdl_mmio_oku64(uint64_t adres) {
    return *(volatile uint64_t *)(uintptr_t)adres;
}

void kdl_mmio_yaz64(uint64_t adres, uint64_t deger) {
    *(volatile uint64_t *)(uintptr_t)adres = deger;
}

#else /* !KEMGU_BARE_METAL */

/* === Host/mock: byte-adreslenebilir, segfault-siz global tampon === */

#define KDL_MMIO_MOCK_BYTE 16384u   /* 16 KiB byte-adreslenebilir pencere */

/* +8 dolgu: en yuksek ofs (BYTE-1) uzerinde 64-bit (8 byte) erisimin tampon
 * sinirini asmasini onler. Boylece her width icin ofs+0..7 her zaman gecerli
 * (ASan temiz) ve cok-byte erisim pencere sonunda da tutarli kalir. */
static uint8_t kdl_mmio_mock[KDL_MMIO_MOCK_BYTE + 8u];

/* adres -> byte ofseti: tampon pencere boyuna gore mod. Kelime-collapse YOK —
 * komsu byte adresleri ayri slotlara duser (le16/le64 ring alanlari faithful). */
static uint32_t kdl_mmio_ofs(uint64_t adres) {
    return (uint32_t)(adres % (uint64_t)KDL_MMIO_MOCK_BYTE);
}

/* Acik little-endian: en dusuk anlamli byte en dusuk adreste. */
static void kdl_mmio_le_yaz(uint32_t ofs, uint64_t deger, int byte_sayisi) {
    for (int i = 0; i < byte_sayisi; i++) {
        kdl_mmio_mock[ofs + (uint32_t)i] =
            (uint8_t)((deger >> (8 * i)) & 0xFFu);
    }
}

static uint64_t kdl_mmio_le_oku(uint32_t ofs, int byte_sayisi) {
    uint64_t v = 0;
    for (int i = 0; i < byte_sayisi; i++) {
        v |= (uint64_t)kdl_mmio_mock[ofs + (uint32_t)i] << (8 * i);
    }
    return v;
}

uint16_t kdl_mmio_oku16(uint64_t adres) {
    return (uint16_t)kdl_mmio_le_oku(kdl_mmio_ofs(adres), 2);
}

void kdl_mmio_yaz16(uint64_t adres, uint16_t deger) {
    kdl_mmio_le_yaz(kdl_mmio_ofs(adres), (uint64_t)deger, 2);
}

uint32_t kdl_mmio_oku32(uint64_t adres) {
    return (uint32_t)kdl_mmio_le_oku(kdl_mmio_ofs(adres), 4);
}

void kdl_mmio_yaz32(uint64_t adres, uint32_t deger) {
    kdl_mmio_le_yaz(kdl_mmio_ofs(adres), (uint64_t)deger, 4);
}

uint64_t kdl_mmio_oku64(uint64_t adres) {
    return kdl_mmio_le_oku(kdl_mmio_ofs(adres), 8);
}

void kdl_mmio_yaz64(uint64_t adres, uint64_t deger) {
    kdl_mmio_le_yaz(kdl_mmio_ofs(adres), deger, 8);
}

void kdl_mmio_mock_sifirla(void) {
    for (uint32_t i = 0; i < KDL_MMIO_MOCK_BYTE + 8u; i++) {
        kdl_mmio_mock[i] = 0;
    }
}

uint32_t kdl_mmio_mock_oku_ham(uint64_t adres) {
    return kdl_mmio_oku32(adres);
}

uint32_t kdl_mmio_mock_kelime_sayisi(void) {
    return KDL_MMIO_MOCK_BYTE / 4u;
}

uint32_t kdl_mmio_mock_byte_sayisi(void) {
    return KDL_MMIO_MOCK_BYTE;
}

#endif /* KEMGU_BARE_METAL */
