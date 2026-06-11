/*
 * KEMGU MMIO Foundation — Runtime Arabirimi (kdl_mmio.h)
 * ======================================================
 *
 * Bellek-eslemeli G/C (MMIO) typed-width register erisimi icin runtime
 * primitifleri (16/32/64-bit). KEMGU dilindeki capability-parametreli sozdizimi:
 *
 *     mmio_oku16(y: yetki<MMIO>, adres: tam64) -> tam16
 *     mmio_yaz16(y: yetki<MMIO>, adres: tam64, deger: tam16)
 *     mmio_oku32(y: yetki<MMIO>, adres: tam64) -> tam32
 *     mmio_yaz32(y: yetki<MMIO>, adres: tam64, deger: tam32)
 *     mmio_oku64(y: yetki<MMIO>, adres: tam64) -> tam64
 *     mmio_yaz64(y: yetki<MMIO>, adres: tam64, deger: tam64)
 *
 * Genislik varyantlari D9 (virtio-blk) ring-bellek erisimi icindir: descriptor
 * `addr le64`, avail/used `idx le16` gibi alanlar 16/64-bit typed erisim ister.
 *
 * LLVM backend bu cagrilari su runtime fonksiyonlarina indirir (yetki<MMIO>
 * derleme-zamani yetki ispatidir, runtime'a gecmez — WCET icin sifir ek yuk):
 *
 *     kdl_mmio_oku16/32/64(adres)
 *     kdl_mmio_yaz16/32/64(adres, deger)
 *
 * Endianness: VirtIO alanlari little-endian (le16/le32/le64). Bare-metal yol
 * native volatile erisim kullanir (ARM64/x86_64 target zaten LE). Host/mock yol
 * tamponu ACIK little-endian byte birlestirmesiyle okur/yazar — host byte
 * sirasindan bagimsiz, bare-metal LE target ile birebir ayni sonuc.
 *
 * Derleme modlari (runtime/kdl_runtime_uart_pl011.c ile ayni desen):
 *   -DKEMGU_BARE_METAL : gercek MMIO — *(volatile uint32_t *) load/store
 *                        (PL011 idiomu; ARM64 / x86_64 QEMU virt, gercek cihaz)
 *   (varsayilan / host) : global tampon mock — segfault-siz, deterministik
 *                        round-trip; --llvm cikti host'ta calistirilabilir.
 *
 * Karar 4: MMIO load donanim seviyesinde basarisiz olmaz; donus duz tam32
 * (sonuc<> degil). Gecersiz adres bir PROGRAMLAMA hatasidir — capability
 * bolgesi init/derleme-zamani yakalamali, runtime hatasi degil.
 */
#ifndef KEMGU_KDL_MMIO_H
#define KEMGU_KDL_MMIO_H

#include <stdint.h>

/* 16-bit MMIO register oku (le16). adres = mutlak fiziksel/sanal adres. */
uint16_t kdl_mmio_oku16(uint64_t adres);

/* 16-bit MMIO register yaz (le16). */
void kdl_mmio_yaz16(uint64_t adres, uint16_t deger);

/* 32-bit MMIO register oku (le32). adres = mutlak fiziksel/sanal register adresi. */
uint32_t kdl_mmio_oku32(uint64_t adres);

/* 32-bit MMIO register yaz (le32). */
void kdl_mmio_yaz32(uint64_t adres, uint32_t deger);

/* 64-bit MMIO register oku (le64). */
uint64_t kdl_mmio_oku64(uint64_t adres);

/* 64-bit MMIO register yaz (le64). */
void kdl_mmio_yaz64(uint64_t adres, uint64_t deger);

#ifndef KEMGU_BARE_METAL
/* === Host/mock yardimcilari (yalniz test ortaminda mevcuttur) === */

/* Mock MMIO tamponunu sifirla (testler arasi izolasyon). */
void kdl_mmio_mock_sifirla(void);

/* Mock tampondaki ham 32-bit degeri oku (kdl_mmio_oku32 ile ayni eslesme). */
uint32_t kdl_mmio_mock_oku_ham(uint64_t adres);

/* Mock tampon kelime kapasitesi (16 KiB pencere = 4096 x 32-bit kelime). */
uint32_t kdl_mmio_mock_kelime_sayisi(void);

/* Mock tampon byte kapasitesi (byte-adreslenebilir pencere boyu). */
uint32_t kdl_mmio_mock_byte_sayisi(void);
#endif /* !KEMGU_BARE_METAL */

#endif /* KEMGU_KDL_MMIO_H */
