/*
 * KEMGU Bare-Metal Runtime — ARM PL011 UART Sürücüsü Test Paketi
 * ================================================================
 *
 * Strateji: KEMGU_UART_MOCK ile derler, MMIO yazımları sürücü-özel
 * global tampona yönelir. Bu sayede driver mantığı host (Windows)
 * üzerinde sentetik biçimde doğrulanır. Gerçek bare-metal ELF
 * doğrulaması ayrı Makefile hedefinde (objdump).
 */

/* KEMGU_UART_MOCK Makefile -D bayragi ile gelir; tum derleme birimleri
 * ayni semantikte derlenmeli. */
#include "../runtime/kdl_uart.h"

#ifndef KEMGU_UART_MOCK
#  error "test_uart_pl011.c: -DKEMGU_UART_MOCK gerekli (Makefile saglar)"
#endif

#include <stdio.h>
#include <string.h>

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

/* === T1: init no-op (panik etmemeli, tampon değişmemeli) === */

static void T1_init_no_op(void) {
    kdl_uart_pl011_mock_temizle();
    kdl_uart_pl011_init();
    test_sonuc("init no-op (mock tampon bos kalir)",
               kdl_uart_pl011_mock_pos == 0 &&
               kdl_uart_pl011_mock_buf[0] == 0);
}

/* === T2: Tek karakter putc === */

static void T2_putc_tek_karakter(void) {
    kdl_uart_pl011_mock_temizle();
    kdl_uart_pl011_putc('K');
    test_sonuc("putc 'K' -> mock_buf[0]='K', pos=1",
               kdl_uart_pl011_mock_pos == 1 &&
               kdl_uart_pl011_mock_buf[0] == 'K');
}

/* === T3: NULL string güvenli no-op === */

static void T3_yaz_null_no_op(void) {
    kdl_uart_pl011_mock_temizle();
    kdl_uart_pl011_yaz(NULL);
    test_sonuc("yaz(NULL) no-op (mock_pos=0)",
               kdl_uart_pl011_mock_pos == 0);
}

/* === T4: Boş string no-op === */

static void T4_yaz_bos_no_op(void) {
    kdl_uart_pl011_mock_temizle();
    kdl_uart_pl011_yaz("");
    test_sonuc("yaz(\"\") no-op",
               kdl_uart_pl011_mock_pos == 0);
}

/* === T5: Çok karakterli string sıra korur === */

static void T5_yaz_cok_karakter(void) {
    kdl_uart_pl011_mock_temizle();
    kdl_uart_pl011_yaz("KEMGU");
    test_sonuc("yaz(\"KEMGU\") -> pos=5, buf=\"KEMGU\"",
               kdl_uart_pl011_mock_pos == 5 &&
               strcmp(kdl_uart_pl011_mock_buf, "KEMGU") == 0);
}

/* === T6: FR.TXFF spin — dolu FIFO simülasyonu === */

static void T6_fifo_dolu_spin(void) {
    kdl_uart_pl011_mock_temizle();
    /* 5 okumada FR.TXFF set döndür, sonra serbest */
    kdl_uart_pl011_mock_fr_full_kalan = 5;
    kdl_uart_pl011_putc('X');
    /* putc 5 spin döndü, sonra yazdı. FR_full_kalan sıfırlanmış olmalı. */
    test_sonuc("FR.TXFF 5 spin sonrasi putc('X') yazar",
               kdl_uart_pl011_mock_pos == 1 &&
               kdl_uart_pl011_mock_buf[0] == 'X' &&
               kdl_uart_pl011_mock_fr_full_kalan == 0);
}

/* === T7: Yeni satır + CR (UTF-8 saf) === */

static void T7_satir_sonu(void) {
    kdl_uart_pl011_mock_temizle();
    kdl_uart_pl011_yaz("Hi\r\n");
    test_sonuc("yaz(\"Hi\\r\\n\") -> 4 byte sirayla yazilir",
               kdl_uart_pl011_mock_pos == 4 &&
               kdl_uart_pl011_mock_buf[0] == 'H' &&
               kdl_uart_pl011_mock_buf[1] == 'i' &&
               kdl_uart_pl011_mock_buf[2] == '\r' &&
               kdl_uart_pl011_mock_buf[3] == '\n');
}

/* === T8: UTF-8 Türkçe byte sequence aynen aktarılır === */

static void T8_utf8_turkce(void) {
    kdl_uart_pl011_mock_temizle();
    /* "Çağ" UTF-8: 0xC3 0x87  0xC3 0xA7  0xC4 0x9F  (6 byte) */
    kdl_uart_pl011_yaz("\xc3\x87" "a\xc4\x9f");  /* "Çağ" */
    test_sonuc("yaz(\"Cag\" UTF-8) -> 5 byte aynen aktarir",
               kdl_uart_pl011_mock_pos == 5 &&
               (unsigned char)kdl_uart_pl011_mock_buf[0] == 0xC3 &&
               (unsigned char)kdl_uart_pl011_mock_buf[1] == 0x87 &&
               kdl_uart_pl011_mock_buf[2] == 'a' &&
               (unsigned char)kdl_uart_pl011_mock_buf[3] == 0xC4 &&
               (unsigned char)kdl_uart_pl011_mock_buf[4] == 0x9F);
}

/* === T9: Tampon taşması güvenli (1023 karakter sonrası) === */

static void T9_tampon_tasmasi_guvenli(void) {
    kdl_uart_pl011_mock_temizle();
    /* 2048 karakter yaz — 1023 yazılır, sonrası yutulur (NUL koruma) */
    for (uint32_t i = 0; i < 2048; i++) {
        kdl_uart_pl011_putc('A');
    }
    /* pos en fazla KDL_UART_MOCK_BUF_SZ - 1 olmalı */
    test_sonuc("2048 char putc -> pos<=1023 (overflow guvenli)",
               kdl_uart_pl011_mock_pos <= (KDL_UART_MOCK_BUF_SZ - 1));
}

/* === T10: Yüksek bit byte (0x80-0xFF) ham aktarılır === */

static void T10_yuksek_bit_byte(void) {
    kdl_uart_pl011_mock_temizle();
    char ham[3] = { (char)0x80, (char)0xFF, (char)0x7F };
    kdl_uart_pl011_putc(ham[0]);
    kdl_uart_pl011_putc(ham[1]);
    kdl_uart_pl011_putc(ham[2]);
    test_sonuc("0x80, 0xFF, 0x7F ham byte aktarir",
               kdl_uart_pl011_mock_pos == 3 &&
               (unsigned char)kdl_uart_pl011_mock_buf[0] == 0x80 &&
               (unsigned char)kdl_uart_pl011_mock_buf[1] == 0xFF &&
               (unsigned char)kdl_uart_pl011_mock_buf[2] == 0x7F);
}

int main(void) {
    printf("=== KEMGU PL011 UART Surucusu Test Paketi ===\n");

    puts("\n--- Temel API ---");
    T1_init_no_op(); T2_putc_tek_karakter();
    T3_yaz_null_no_op(); T4_yaz_bos_no_op();
    T5_yaz_cok_karakter();

    puts("\n--- FIFO + Sinir Senaryolari ---");
    T6_fifo_dolu_spin(); T7_satir_sonu();
    T8_utf8_turkce(); T9_tampon_tasmasi_guvenli();
    T10_yuksek_bit_byte();

    printf("\n========================================\n");
    printf("Toplam: %d | Basarili: %d | Basarisiz: %d\n",
           toplam_test, basarili, basarisiz);

    return basarisiz == 0 ? 0 : 1;
}
