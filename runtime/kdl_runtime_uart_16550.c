/*
 * KEMGU Bare-Metal Runtime — NS16550A UART Surucusu (x86_64 COM1)
 * ================================================================
 *
 * NS16550A — IBM PC standardı seri port denetleyicisi. BIOS COM1 port'u
 * varsayılan olarak 0x3F8 (compile-time override: -DKDL_16550_BASE=...).
 *
 * Kayit yerlesimi (DLAB=0 modunda):
 *   0x00 THR    (write) — Transmit Holding Register
 *   0x00 RBR    (read)  — Receive Buffer Register
 *   0x01 IER             — Interrupt Enable
 *   0x02 IIR/FCR         — Interrupt ID / FIFO Control
 *   0x03 LCR             — Line Control (DLAB biti dahil)
 *   0x04 MCR             — Modem Control
 *   0x05 LSR             — Line Status (bit 5 THRE = TX hazir)
 *   0x06 MSR             — Modem Status
 *
 * Heap YOK, libc YOK. Inline asm in/out (x86 specific) veya mock.
 *
 * Derleme modlari:
 *   -DKEMGU_BARE_METAL   gercek port I/O (yalniz x86/x86_64)
 *   -DKEMGU_UART_MOCK    host test (mock buf, asm yok)
 */

#include <stdint.h>
#include "kdl_uart.h"

#if !defined(KEMGU_BARE_METAL) && !defined(KEMGU_UART_MOCK)
#  error "kdl_runtime_uart_16550.c: KEMGU_BARE_METAL veya KEMGU_UART_MOCK gerekli"
#endif

#if defined(KEMGU_BARE_METAL) && !defined(KEMGU_UART_MOCK)
#  if !defined(__x86_64__) && !defined(__i386__) && !defined(_M_X64) && !defined(_M_IX86)
#    error "16550A surucusu yalniz x86/x86_64 hedeflerinde derlenir"
#  endif
#endif

#ifndef KDL_16550_BASE
#  define KDL_16550_BASE 0x3F8U   /* COM1 */
#endif

#define KDL_16550_THR  0x0U
#define KDL_16550_IER  0x1U
#define KDL_16550_FCR  0x2U
#define KDL_16550_LCR  0x3U
#define KDL_16550_MCR  0x4U
#define KDL_16550_LSR  0x5U

#define KDL_16550_LSR_THRE  (1U << 5)   /* THR bos -> yazilabilir */
#define KDL_16550_LSR_TEMT  (1U << 6)   /* THR + shift register bos */

/* === Mock altyapısı === */

#ifdef KEMGU_UART_MOCK

char     kdl_uart_16550_mock_buf[KDL_UART_MOCK_BUF_SZ];
uint32_t kdl_uart_16550_mock_pos = 0;
uint32_t kdl_uart_16550_mock_thre_bekle = 0;

void kdl_uart_16550_mock_temizle(void) {
    for (uint32_t i = 0; i < KDL_UART_MOCK_BUF_SZ; i++) {
        kdl_uart_16550_mock_buf[i] = 0;
    }
    kdl_uart_16550_mock_pos = 0;
    kdl_uart_16550_mock_thre_bekle = 0;
}

static inline uint8_t kdl_16550_inb(uint16_t port) {
    uint16_t ofs = (uint16_t)(port - KDL_16550_BASE);
    if (ofs == KDL_16550_LSR) {
        if (kdl_uart_16550_mock_thre_bekle > 0) {
            kdl_uart_16550_mock_thre_bekle--;
            return 0;  /* THRE temiz -> busy */
        }
        return KDL_16550_LSR_THRE | KDL_16550_LSR_TEMT;
    }
    return 0;
}

static inline void kdl_16550_outb(uint16_t port, uint8_t v) {
    uint16_t ofs = (uint16_t)(port - KDL_16550_BASE);
    if (ofs == KDL_16550_THR) {
        if (kdl_uart_16550_mock_pos < KDL_UART_MOCK_BUF_SZ - 1) {
            kdl_uart_16550_mock_buf[kdl_uart_16550_mock_pos++] = (char)v;
            kdl_uart_16550_mock_buf[kdl_uart_16550_mock_pos] = 0;
        }
    }
    /* Diger registerlar (IER/FCR/LCR/MCR) mock'ta sessizce yutulur */
}

#else  /* KEMGU_BARE_METAL (x86/x86_64) */

static inline uint8_t kdl_16550_inb(uint16_t port) {
    uint8_t v;
    __asm__ volatile ("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static inline void kdl_16550_outb(uint16_t port, uint8_t v) {
    __asm__ volatile ("outb %0, %1" : : "a"(v), "Nd"(port));
}

#endif

/* === Kamu API === */

void kdl_uart_16550_init(void) {
    /* V1: BIOS/firmware init'li COM1 varsayilir (QEMU + gercek PC ortak).
     * Gercek "soğuk başlangıç" icin tipik sekans (115200 baud, 8N1, FIFO):
     *
     *   outb(BASE+IER, 0x00);   IER disable
     *   outb(BASE+LCR, 0x80);   DLAB=1
     *   outb(BASE+0,   0x01);   Divisor low (115200 baud)
     *   outb(BASE+1,   0x00);   Divisor high
     *   outb(BASE+LCR, 0x03);   8N1, DLAB=0
     *   outb(BASE+FCR, 0xC7);   FIFO etkin, temizle, 14-byte threshold
     *   outb(BASE+MCR, 0x0B);   IRQ etkin, RTS/DSR set
     *
     * Bu sekans her board icin guvenli degildir (clock differences), bu
     * yuzden V1 no-op. Kullanici kendi init kodunu cagirir.
     */
}

void kdl_uart_16550_putc(char c) {
    /* LSR.THRE temiz oldugu surece dön */
    while ((kdl_16550_inb((uint16_t)(KDL_16550_BASE + KDL_16550_LSR))
            & KDL_16550_LSR_THRE) == 0) {
        /* spin */
    }
    kdl_16550_outb((uint16_t)(KDL_16550_BASE + KDL_16550_THR),
                   (uint8_t)c);
}

void kdl_uart_16550_yaz(const char *s) {
    if (!s) return;
    while (*s) {
        kdl_uart_16550_putc(*s++);
    }
}
