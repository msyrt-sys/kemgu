/*
 * KEMGU Bare-Metal Runtime — Panik Handler
 * =========================================
 *
 * Bare-metal ortamda kurtarilamaz hata (bounds violation, page fault,
 * assertion failure, capability ihlali) UART'a mesaj yazip CPU'yu
 * dusuk-guc halt'a alir.
 *
 * Mod bayraklari:
 *   -DKEMGU_BARE_METAL   gercek halt (wfe/hlt loop, NORETURN)
 *   -DKEMGU_UART_MOCK    test (counter increment, return)
 */

#ifndef KDL_PANIK_H
#define KDL_PANIK_H

#include <stdint.h>

#ifdef KEMGU_UART_MOCK
/* Test izleme — panik kac kere cagrildi? */
extern uint32_t kdl_panik_sayisi;
void kdl_panik_dur(const char *mesaj);
void kdl_panik_mock_temizle(void);
#else
/* Gercek bare-metal: NORETURN — derleyici caller'da dead code elimine eder. */
__attribute__((noreturn)) void kdl_panik_dur(const char *mesaj);
#endif

#endif
