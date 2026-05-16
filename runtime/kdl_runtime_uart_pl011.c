/*
 * KEMGU Bare-Metal Runtime — ARM PL011 UART Sürücüsü
 * ====================================================
 *
 * ARM PrimeCell PL011 UART (TRM DDI 0183G) — QEMU virt makinesi
 * varsayılan UART denetleyicisi.
 *
 * Bağlam: KEMGU'nun Hedef 3 (Evrensel OS) yol haritasında bare-metal
 * konsolun ilk adımıdır. Bump allocator henüz onaylı değil — bu sürücü
 * heap'siz, stack-yalnız, ham MMIO yazımıyla çalışır.
 *
 * Kayıt yerleşimi (TRM Table 3-1):
 *   0x000 DR     veri yazma/okuma
 *   0x018 FR     bayrak: TXFF=bit5 TX FIFO dolu, TXFE=bit7 TX FIFO boş
 *   0x024 IBRD   tam baud rate bölücü
 *   0x028 FBRD   kesirli baud rate bölücü
 *   0x02C LCRH   satır kontrol (8N1 + FIFO)
 *   0x030 CR     kontrol (UARTEN, TXE, RXE)
 *
 * Compile-time override:
 *   -DKDL_PL011_BASE=0x... — taban adresini değiştir (Raspberry Pi 4 vb.)
 *
 * Derleme modları:
 *   -DKEMGU_BARE_METAL  → gerçek MMIO (volatile *(uint32_t *) yazımı)
 *   -DKEMGU_UART_MOCK   → host test (global tampon)
 *
 * Hiçbiri tanımlı değilse, derleyici libc-bağımlı stub'a düşmemesi için
 * derleme zamanında uyarı verir. (Sertifikasyon güvenliği.)
 */

#include <stdint.h>
#include "kdl_uart.h"

#if !defined(KEMGU_BARE_METAL) && !defined(KEMGU_UART_MOCK)
#  error "kdl_runtime_uart_pl011.c: KEMGU_BARE_METAL veya KEMGU_UART_MOCK tanimlanmali (Makefile -D bayragi ile)"
#endif

#ifndef KDL_PL011_BASE
#  define KDL_PL011_BASE ((uintptr_t)0x09000000)   /* QEMU virt UART0 */
#endif

#define KDL_PL011_DR    0x000U
#define KDL_PL011_FR    0x018U
#define KDL_PL011_IBRD  0x024U
#define KDL_PL011_FBRD  0x028U
#define KDL_PL011_LCRH  0x02CU
#define KDL_PL011_CR    0x030U

#define KDL_PL011_FR_TXFE  (1U << 7)   /* TX FIFO bos */
#define KDL_PL011_FR_TXFF  (1U << 5)   /* TX FIFO dolu */
#define KDL_PL011_FR_BUSY  (1U << 3)   /* gonderim devam */

/* === Mock yedek altyapı === */

#ifdef KEMGU_UART_MOCK

char     kdl_uart_pl011_mock_buf[KDL_UART_MOCK_BUF_SZ];
uint32_t kdl_uart_pl011_mock_pos = 0;
uint32_t kdl_uart_pl011_mock_fr_full_kalan = 0;

void kdl_uart_pl011_mock_temizle(void) {
    /* libc memset yok — el ile sıfırla */
    for (uint32_t i = 0; i < KDL_UART_MOCK_BUF_SZ; i++) {
        kdl_uart_pl011_mock_buf[i] = 0;
    }
    kdl_uart_pl011_mock_pos = 0;
    kdl_uart_pl011_mock_fr_full_kalan = 0;
}

static inline uint32_t kdl_pl011_oku32(uint32_t ofs) {
    if (ofs == KDL_PL011_FR) {
        if (kdl_uart_pl011_mock_fr_full_kalan > 0) {
            kdl_uart_pl011_mock_fr_full_kalan--;
            return KDL_PL011_FR_TXFF;
        }
        return 0;
    }
    return 0;
}

static inline void kdl_pl011_yaz32(uint32_t ofs, uint32_t deger) {
    if (ofs == KDL_PL011_DR) {
        if (kdl_uart_pl011_mock_pos < KDL_UART_MOCK_BUF_SZ - 1) {
            kdl_uart_pl011_mock_buf[kdl_uart_pl011_mock_pos++] =
                (char)(deger & 0xFFU);
            /* NUL-sonlandırma garanti — testlerde strcmp güvenli */
            kdl_uart_pl011_mock_buf[kdl_uart_pl011_mock_pos] = 0;
        }
    }
    /* IBRD/FBRD/LCRH/CR yazmaları mock'ta sessizce yutulur */
}

#else  /* KEMGU_BARE_METAL veya tanımsız (warning verildi) */

static inline uint32_t kdl_pl011_oku32(uint32_t ofs) {
    return *(volatile uint32_t *)(KDL_PL011_BASE + (uintptr_t)ofs);
}

static inline void kdl_pl011_yaz32(uint32_t ofs, uint32_t deger) {
    *(volatile uint32_t *)(KDL_PL011_BASE + (uintptr_t)ofs) = deger;
}

#endif

/* === Kamu API === */

void kdl_uart_pl011_init(void) {
    /* V1: QEMU virt + UEFI sonrası firmware-init'li ARM board'lar için
     * no-op yeterli. Gerçek donanımda (Raspberry Pi 4, bare-metal boot)
     * şunlar yazılmalı:
     *   CR=0                       (UART devre dışı)
     *   IBRD/FBRD                  (baud rate, ör. 115200 @ 24 MHz UARTCLK
     *                                 → IBRD=13, FBRD=1)
     *   LCRH = (3<<5) | (1<<4)     (8N1 + FIFO etkin)
     *   CR = (1<<0)|(1<<8)|(1<<9)  (UARTEN | TXE | RXE)
     *
     * Bu kararı sürücüye sabitlemiyoruz — board-spesifik init kodu üst
     * katmanda (kernel boot) çağırır. V1 yalnızca yazım yolunu hazırlar.
     */
}

void kdl_uart_pl011_putc(char c) {
    /* FR.TXFF set olduğu sürece dön. Spin lock — irq/timer yok. */
    while (kdl_pl011_oku32(KDL_PL011_FR) & KDL_PL011_FR_TXFF) {
        /* boşa dön */
    }
    kdl_pl011_yaz32(KDL_PL011_DR, (uint32_t)(unsigned char)c);
}

void kdl_uart_pl011_yaz(const char *s) {
    if (!s) return;
    while (*s) {
        kdl_uart_pl011_putc(*s++);
    }
}
