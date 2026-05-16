/*
 * KEMGU Bare-Metal Runtime — kdl_yazdir_* Konsol Cikti Implementasyonu
 * ====================================================================
 *
 * libc YOK, malloc YOK, snprintf YOK. Stack-yalniz 32 byte tampon ile
 * positional base-10 formatlama. UART surucusu cagrisi makro ile
 * subsitute edilir:
 *
 *   ARM64 (varsayilan):   -DKDL_UART_PUTC=kdl_uart_pl011_putc
 *   x86_64:               -DKDL_UART_PUTC=kdl_uart_16550_putc
 *
 * Bayrak verilmezse PL011 varsayilani secilir.
 *
 * Mod bayraklari (kdl_uart.h ile ayni):
 *   -DKEMGU_BARE_METAL  gercek MMIO/port surucusu
 *   -DKEMGU_UART_MOCK   host test (UART mock buf)
 *
 * Sembol cakismasi: host build (runtime/kdl_runtime.c) bu dosyayi
 * derlemez. Bare-metal build host runtime'i derlemez. Tek-yon yasak
 * Makefile tarafinda yonetilir.
 */

#include <stdint.h>
#include "kdl_uart.h"
#include "kdl_yazdir_bare.h"

#if !defined(KEMGU_BARE_METAL) && !defined(KEMGU_UART_MOCK)
#  error "kdl_runtime_yazdir_bare.c: KEMGU_BARE_METAL veya KEMGU_UART_MOCK tanimlanmali"
#endif

/* Backend secimi — Makefile -D ile override edilebilir. */
#ifndef KDL_UART_PUTC
#  define KDL_UART_PUTC kdl_uart_pl011_putc
#endif

extern void KDL_UART_PUTC(char c);

/* === Yardimci: int64 → base-10 string (stack tampon) ===
 *
 * Donus: yazilan byte (NUL haric). Kapasite az ise -1.
 * INT64_MIN guvenli ele alinir: -(-INT64_MIN) overflow olur, bu yuzden
 * uint64 mutlak deger ayri hesaplanir.
 */
int32_t kdl_format_tam64(int64_t n, char *cikti, int32_t kapasite) {
    /* En kotu durum: "-9223372036854775808" + NUL = 21 byte. */
    if (!cikti || kapasite < 21) return -1;

    int negatif = 0;
    uint64_t mutlak;
    if (n < 0) {
        negatif = 1;
        /* INT64_MIN guvenli: ((u64)(-(n+1))) + 1 */
        mutlak = (uint64_t)(-(n + 1)) + 1U;
    } else {
        mutlak = (uint64_t)n;
    }

    /* Rakamlari ters yaz, sonra ters cevir. */
    char tmp[21];
    int t = 0;
    if (mutlak == 0) {
        tmp[t++] = '0';
    } else {
        while (mutlak > 0) {
            tmp[t++] = (char)('0' + (mutlak % 10U));
            mutlak /= 10U;
        }
    }

    int pos = 0;
    if (negatif) cikti[pos++] = '-';
    while (t > 0) cikti[pos++] = tmp[--t];
    cikti[pos] = '\0';
    return (int32_t)pos;
}

/* === Yardimci: ham string yaz (newline yok) === */
static void kdl_ham_yaz(const char *s) {
    if (!s) return;
    while (*s) {
        KDL_UART_PUTC(*s++);
    }
}

/* === D.1 IO API — Bare-metal portu === */

void kdl_yazdir_metin(const char *s) {
    if (s) {
        kdl_ham_yaz(s);
    } else {
        kdl_ham_yaz("(bos)");
    }
    KDL_UART_PUTC('\n');
}

void kdl_yazdir_satir(void) {
    KDL_UART_PUTC('\n');
}

void kdl_yaz_metin(const char *s) {
    if (s) kdl_ham_yaz(s);
}

void kdl_yazdir_tam(int32_t n) {
    char buf[24];
    if (kdl_format_tam64((int64_t)n, buf, sizeof(buf)) > 0) {
        kdl_ham_yaz(buf);
    }
    KDL_UART_PUTC('\n');
}

void kdl_yazdir_tam64(int64_t n) {
    char buf[24];
    if (kdl_format_tam64(n, buf, sizeof(buf)) > 0) {
        kdl_ham_yaz(buf);
    }
    KDL_UART_PUTC('\n');
}

void kdl_yaz_tam(int32_t n) {
    char buf[24];
    if (kdl_format_tam64((int64_t)n, buf, sizeof(buf)) > 0) {
        kdl_ham_yaz(buf);
    }
}

void kdl_yazdir_mantiksal(int b) {
    /* Host runtime ile birebir: "do\xc4\x9fru" / "yanl\xc4\xb1\xc5\x9f"
     * — UTF-8 Turkce literal. (Hex escape sonrasi 'r'/'f' guvenli devam
     * karakterleri — concatenation gerekmez.) */
    if (b) {
        kdl_ham_yaz("do\xc4\x9f" "ru");
    } else {
        kdl_ham_yaz("yanl\xc4\xb1" "\xc5\x9f");
    }
    KDL_UART_PUTC('\n');
}
