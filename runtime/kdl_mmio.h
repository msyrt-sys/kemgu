/*
 * KEMGU MMIO Foundation — Runtime Arabirimi (kdl_mmio.h)
 * ======================================================
 *
 * Bellek-eslemeli G/C (MMIO) 32-bit register erisimi icin runtime primitifleri.
 * KEMGU dilindeki capability-parametreli sozdizimi:
 *
 *     mmio_oku32(y: yetki<MMIO>, adres: tam64) -> tam32
 *     mmio_yaz32(y: yetki<MMIO>, adres: tam64, deger: tam32)
 *
 * LLVM backend bu cagrilari su runtime fonksiyonlarina indirir (yetki<MMIO>
 * derleme-zamani yetki ispatidir, runtime'a gecmez — WCET icin sifir ek yuk):
 *
 *     kdl_mmio_oku32(adres)
 *     kdl_mmio_yaz32(adres, deger)
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

/* 32-bit MMIO register oku. adres = mutlak fiziksel/sanal register adresi. */
uint32_t kdl_mmio_oku32(uint64_t adres);

/* 32-bit MMIO register yaz. */
void kdl_mmio_yaz32(uint64_t adres, uint32_t deger);

#ifndef KEMGU_BARE_METAL
/* === Host/mock yardimcilari (yalniz test ortaminda mevcuttur) === */

/* Mock MMIO tamponunu sifirla (testler arasi izolasyon). */
void kdl_mmio_mock_sifirla(void);

/* Mock tampondaki ham degeri oku (kdl_mmio_oku32 ile ayni eslesme). */
uint32_t kdl_mmio_mock_oku_ham(uint64_t adres);

/* Mock tampon kelime kapasitesi (adres -> indeks eslemesi icin). */
uint32_t kdl_mmio_mock_kelime_sayisi(void);
#endif /* !KEMGU_BARE_METAL */

#endif /* KEMGU_KDL_MMIO_H */
