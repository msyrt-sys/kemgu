/*
 * KEMGU Bare-Metal Runtime — kdl_panik_dur Implementasyonu
 * =========================================================
 *
 * UART'a "PANIK: <mesaj>" yazar, ardindan CPU'yu dusuk-guc halt'a
 * alir. wfe (ARM64) veya hlt (x86) talimati kullanir.
 *
 * libc YOK, heap YOK. KDL_UART_PUTC macro (kdl_runtime_yazdir_bare.c ile
 * ayni pattern) backend'i belirler.
 */

#include <stdint.h>
#include "kdl_uart.h"
#include "kdl_panik.h"

#if !defined(KEMGU_BARE_METAL) && !defined(KEMGU_UART_MOCK)
#  error "kdl_runtime_panik.c: KEMGU_BARE_METAL veya KEMGU_UART_MOCK gerekli"
#endif

#ifndef KDL_UART_PUTC
#  define KDL_UART_PUTC kdl_uart_pl011_putc
#endif

extern void KDL_UART_PUTC(char c);

#ifdef KEMGU_UART_MOCK
uint32_t kdl_panik_sayisi = 0;

void kdl_panik_mock_temizle(void) {
    kdl_panik_sayisi = 0;
}
#endif

static void kdl_panik_yaz(const char *s) {
    while (*s) KDL_UART_PUTC(*s++);
}

void kdl_panik_dur(const char *mesaj) {
    /* Onceki cikti satirini temizle, mesaji ayri satira yaz */
    KDL_UART_PUTC('\n');
    kdl_panik_yaz("PANIK: ");
    if (mesaj) {
        kdl_panik_yaz(mesaj);
    } else {
        kdl_panik_yaz("(bilinmiyor)");
    }
    KDL_UART_PUTC('\n');

#ifdef KEMGU_UART_MOCK
    /* Test modunda halt'a girmeyiz — testin dogrulama yapmasi icin doneriz.
     * Gerçek bare-metal'da bu satira asla ulasilmaz. */
    kdl_panik_sayisi++;
    return;
#else
    /* Gercek bare-metal halt loop — interrupt + restart olmadan sonsuz dur */
    for (;;) {
#  if defined(__aarch64__)
        __asm__ volatile ("wfe");
#  elif defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
        __asm__ volatile ("hlt");
#  else
        /* Bilinmeyen mimari — basit busy loop. */
        __asm__ volatile ("" ::: "memory");
#  endif
    }
#endif
}
