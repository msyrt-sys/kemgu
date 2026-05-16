/*
 * KEMGU Bare-Metal Runtime — NS16550A UART Surucusu Test Paketi
 * ================================================================
 *
 * Mock LSR/THR port'lariyla driver mantigi host'ta dogrulanir.
 * Gercek port I/O bare-metal cross-compile dogrulamasinda incelenir.
 */

#include "../runtime/kdl_uart.h"

#include <stdio.h>
#include <string.h>

#ifndef KEMGU_UART_MOCK
#  error "test_uart_16550.c: -DKEMGU_UART_MOCK gerekli"
#endif

static int toplam_test = 0;
static int basarili = 0;
static int basarisiz = 0;

static void test_sonuc(const char *ad, int durum) {
    toplam_test++;
    if (durum) {
        basarili++;
        printf("  [%d] %s ... \xe2\x9c\x93\n", toplam_test, ad);
    } else {
        basarisiz++;
        printf("  [%d] %s ... \xe2\x9c\x97\n", toplam_test, ad);
    }
}

static void T1_init_no_op(void) {
    kdl_uart_16550_mock_temizle();
    kdl_uart_16550_init();
    test_sonuc("init no-op (mock tampon bos)",
               kdl_uart_16550_mock_pos == 0 &&
               kdl_uart_16550_mock_buf[0] == 0);
}

static void T2_putc_tek_karakter(void) {
    kdl_uart_16550_mock_temizle();
    kdl_uart_16550_putc('K');
    test_sonuc("putc 'K' -> buf[0]='K', pos=1",
               kdl_uart_16550_mock_pos == 1 &&
               kdl_uart_16550_mock_buf[0] == 'K');
}

static void T3_yaz_null(void) {
    kdl_uart_16550_mock_temizle();
    kdl_uart_16550_yaz(NULL);
    test_sonuc("yaz(NULL) no-op",
               kdl_uart_16550_mock_pos == 0);
}

static void T4_yaz_bos(void) {
    kdl_uart_16550_mock_temizle();
    kdl_uart_16550_yaz("");
    test_sonuc("yaz(\"\") no-op",
               kdl_uart_16550_mock_pos == 0);
}

static void T5_yaz_cok_karakter(void) {
    kdl_uart_16550_mock_temizle();
    kdl_uart_16550_yaz("KEMGU");
    test_sonuc("yaz(\"KEMGU\") -> pos=5",
               kdl_uart_16550_mock_pos == 5 &&
               strcmp(kdl_uart_16550_mock_buf, "KEMGU") == 0);
}

static void T6_lsr_thre_bekle(void) {
    kdl_uart_16550_mock_temizle();
    /* 3 okumada LSR.THRE=0 (busy), sonra serbest */
    kdl_uart_16550_mock_thre_bekle = 3;
    kdl_uart_16550_putc('Y');
    test_sonuc("LSR.THRE=0 spin 3x sonrasi putc('Y')",
               kdl_uart_16550_mock_pos == 1 &&
               kdl_uart_16550_mock_buf[0] == 'Y' &&
               kdl_uart_16550_mock_thre_bekle == 0);
}

static void T7_satir_sonu_crlf(void) {
    kdl_uart_16550_mock_temizle();
    kdl_uart_16550_yaz("Hi\r\n");
    test_sonuc("yaz(\"Hi\\r\\n\") -> 4 byte sirayla",
               kdl_uart_16550_mock_pos == 4 &&
               kdl_uart_16550_mock_buf[0] == 'H' &&
               kdl_uart_16550_mock_buf[3] == '\n');
}

static void T8_yuksek_bit_byte(void) {
    kdl_uart_16550_mock_temizle();
    kdl_uart_16550_putc((char)0xFF);
    kdl_uart_16550_putc((char)0x80);
    test_sonuc("yuksek bit byte 0xFF/0x80 ham aktarir",
               kdl_uart_16550_mock_pos == 2 &&
               (unsigned char)kdl_uart_16550_mock_buf[0] == 0xFF &&
               (unsigned char)kdl_uart_16550_mock_buf[1] == 0x80);
}

int main(void) {
    printf("=== KEMGU 16550A UART Surucusu Test Paketi ===\n");

    puts("\n--- Temel API ---");
    T1_init_no_op(); T2_putc_tek_karakter();
    T3_yaz_null(); T4_yaz_bos();
    T5_yaz_cok_karakter();

    puts("\n--- LSR + Sinir Senaryolari ---");
    T6_lsr_thre_bekle(); T7_satir_sonu_crlf();
    T8_yuksek_bit_byte();

    printf("\n========================================\n");
    printf("Toplam: %d | Basarili: %d | Basarisiz: %d\n",
           toplam_test, basarili, basarisiz);

    return basarisiz == 0 ? 0 : 1;
}
