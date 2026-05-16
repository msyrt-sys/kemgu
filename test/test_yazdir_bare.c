/*
 * KEMGU Bare-Metal Runtime — kdl_yazdir_* Test Paketi
 * ====================================================
 *
 * Mock PL011 backend ile yazdirma fonksiyonlarini host'ta dogrular.
 * Cikti mock buffer'a yazilir; karsilastirma strcmp ile yapilir.
 */

#include "../runtime/kdl_uart.h"
#include "../runtime/kdl_yazdir_bare.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#ifndef KEMGU_UART_MOCK
#  error "test_yazdir_bare.c: -DKEMGU_UART_MOCK gerekli"
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
        printf("  [%d] %s ... \xe2\x9c\x97  (buf=\"%s\", pos=%u)\n",
               toplam_test, ad, kdl_uart_pl011_mock_buf,
               (unsigned)kdl_uart_pl011_mock_pos);
    }
}

/* === Format yardimcisi (saf — UART cagirisi yok) === */

static void T1_format_pozitif(void) {
    char buf[24];
    int32_t n = kdl_format_tam64(42, buf, sizeof(buf));
    test_sonuc("format_tam64(42) -> \"42\"",
               n == 2 && strcmp(buf, "42") == 0);
}

static void T2_format_sifir(void) {
    char buf[24];
    int32_t n = kdl_format_tam64(0, buf, sizeof(buf));
    test_sonuc("format_tam64(0) -> \"0\"",
               n == 1 && strcmp(buf, "0") == 0);
}

static void T3_format_negatif(void) {
    char buf[24];
    int32_t n = kdl_format_tam64(-1, buf, sizeof(buf));
    test_sonuc("format_tam64(-1) -> \"-1\"",
               n == 2 && strcmp(buf, "-1") == 0);
}

static void T4_format_int32_min(void) {
    char buf[24];
    int32_t n = kdl_format_tam64((int64_t)INT32_MIN, buf, sizeof(buf));
    test_sonuc("format_tam64(INT32_MIN) -> \"-2147483648\"",
               n == 11 && strcmp(buf, "-2147483648") == 0);
}

static void T5_format_int64_max(void) {
    char buf[24];
    int32_t n = kdl_format_tam64(INT64_MAX, buf, sizeof(buf));
    test_sonuc("format_tam64(INT64_MAX) -> \"9223372036854775807\"",
               n == 19 && strcmp(buf, "9223372036854775807") == 0);
}

static void T6_format_int64_min(void) {
    char buf[24];
    int32_t n = kdl_format_tam64(INT64_MIN, buf, sizeof(buf));
    test_sonuc("format_tam64(INT64_MIN) -> \"-9223372036854775808\"",
               n == 20 && strcmp(buf, "-9223372036854775808") == 0);
}

static void T7_format_kapasite_yetersiz(void) {
    char buf[5];
    int32_t n = kdl_format_tam64(123, buf, 5);
    test_sonuc("format_tam64 kapasite<21 -> -1",
               n == -1);
}

/* === yazdir_metin === */

static void T8_yazdir_metin(void) {
    kdl_uart_pl011_mock_temizle();
    kdl_yazdir_metin("KEMGU");
    test_sonuc("yazdir_metin(\"KEMGU\") -> \"KEMGU\\n\"",
               strcmp(kdl_uart_pl011_mock_buf, "KEMGU\n") == 0);
}

static void T9_yazdir_metin_null(void) {
    kdl_uart_pl011_mock_temizle();
    kdl_yazdir_metin(NULL);
    test_sonuc("yazdir_metin(NULL) -> \"(bos)\\n\"",
               strcmp(kdl_uart_pl011_mock_buf, "(bos)\n") == 0);
}

/* === yaz_metin (newline yok) === */

static void T10_yaz_metin(void) {
    kdl_uart_pl011_mock_temizle();
    kdl_yaz_metin("XY");
    test_sonuc("yaz_metin(\"XY\") -> \"XY\" (newline yok)",
               strcmp(kdl_uart_pl011_mock_buf, "XY") == 0 &&
               kdl_uart_pl011_mock_pos == 2);
}

/* === yazdir_satir === */

static void T11_yazdir_satir(void) {
    kdl_uart_pl011_mock_temizle();
    kdl_yazdir_satir();
    test_sonuc("yazdir_satir() -> \"\\n\"",
               kdl_uart_pl011_mock_buf[0] == '\n' &&
               kdl_uart_pl011_mock_pos == 1);
}

/* === yazdir_tam === */

static void T12_yazdir_tam_42(void) {
    kdl_uart_pl011_mock_temizle();
    kdl_yazdir_tam(42);
    test_sonuc("yazdir_tam(42) -> \"42\\n\"",
               strcmp(kdl_uart_pl011_mock_buf, "42\n") == 0);
}

static void T13_yazdir_tam_negatif(void) {
    kdl_uart_pl011_mock_temizle();
    kdl_yazdir_tam(-42);
    test_sonuc("yazdir_tam(-42) -> \"-42\\n\"",
               strcmp(kdl_uart_pl011_mock_buf, "-42\n") == 0);
}

/* === yazdir_tam64 === */

static void T14_yazdir_tam64(void) {
    kdl_uart_pl011_mock_temizle();
    kdl_yazdir_tam64(1234567890123LL);
    test_sonuc("yazdir_tam64(1234567890123) -> \"1234567890123\\n\"",
               strcmp(kdl_uart_pl011_mock_buf, "1234567890123\n") == 0);
}

/* === yazdir_mantiksal === */

static void T15_yazdir_mantik_dogru(void) {
    kdl_uart_pl011_mock_temizle();
    kdl_yazdir_mantiksal(1);
    /* "do\xc4\x9fru\n" — UTF-8: 6 byte + newline = 7 byte */
    test_sonuc("yazdir_mantiksal(1) -> \"do\xc4\x9fru\\n\"",
               strcmp(kdl_uart_pl011_mock_buf, "do\xc4\x9f" "ru\n") == 0);
}

static void T16_yazdir_mantik_yanlis(void) {
    kdl_uart_pl011_mock_temizle();
    kdl_yazdir_mantiksal(0);
    /* "yanl\xc4\xb1\xc5\x9f\n" — UTF-8: 8 byte + newline = 9 byte */
    test_sonuc("yazdir_mantiksal(0) -> \"yanl\xc4\xb1\xc5\x9f\\n\"",
               strcmp(kdl_uart_pl011_mock_buf, "yanl\xc4\xb1" "\xc5\x9f\n") == 0);
}

/* === C1: isaretsiz + onaltilik formatlar === */

static void T18_isaretsiz_tam(void) {
    kdl_uart_pl011_mock_temizle();
    kdl_yazdir_isaretsiz_tam(4000000000U);  /* INT32_MAX'in ustunde */
    test_sonuc("yazdir_isaretsiz_tam(4000000000) -> \"4000000000\\n\"",
               strcmp(kdl_uart_pl011_mock_buf, "4000000000\n") == 0);
}

static void T19_isaretsiz_tam64_max(void) {
    kdl_uart_pl011_mock_temizle();
    kdl_yazdir_isaretsiz_tam64(UINT64_MAX);
    test_sonuc("yazdir_isaretsiz_tam64(UINT64_MAX) -> \"18446744073709551615\\n\"",
               strcmp(kdl_uart_pl011_mock_buf,
                      "18446744073709551615\n") == 0);
}

static void T20_onaltilik_kucuk(void) {
    kdl_uart_pl011_mock_temizle();
    kdl_yazdir_onaltilik(0x42);
    test_sonuc("yazdir_onaltilik(0x42) -> \"0x42\\n\"",
               strcmp(kdl_uart_pl011_mock_buf, "0x42\n") == 0);
}

static void T21_onaltilik_sifir(void) {
    kdl_uart_pl011_mock_temizle();
    kdl_yazdir_onaltilik(0);
    test_sonuc("yazdir_onaltilik(0) -> \"0x0\\n\"",
               strcmp(kdl_uart_pl011_mock_buf, "0x0\n") == 0);
}

static void T22_onaltilik_64bit_max(void) {
    kdl_uart_pl011_mock_temizle();
    kdl_yazdir_onaltilik(UINT64_MAX);
    test_sonuc("yazdir_onaltilik(UINT64_MAX) -> \"0xffffffffffffffff\\n\"",
               strcmp(kdl_uart_pl011_mock_buf,
                      "0xffffffffffffffff\n") == 0);
}

static void T23_onaltilik_pl011_base(void) {
    /* QEMU virt PL011 taban adresi ornek — gercek hata ayiklama use case */
    kdl_uart_pl011_mock_temizle();
    kdl_yazdir_onaltilik(0x09000000ULL);
    test_sonuc("yazdir_onaltilik(0x9000000) -> \"0x9000000\\n\"",
               strcmp(kdl_uart_pl011_mock_buf, "0x9000000\n") == 0);
}

static void T24_yaz_onaltilik(void) {
    kdl_uart_pl011_mock_temizle();
    kdl_yaz_onaltilik(0xDEADBEEF);
    kdl_yazdir_satir();
    test_sonuc("yaz_onaltilik(0xDEADBEEF) + satir -> \"0xdeadbeef\\n\"",
               strcmp(kdl_uart_pl011_mock_buf, "0xdeadbeef\n") == 0);
}

/* === Birlesik akis — bir program akisini simule et === */

static void T17_program_akisi(void) {
    kdl_uart_pl011_mock_temizle();
    kdl_yazdir_metin("KEMGU");
    kdl_yazdir_tam(42);
    kdl_yazdir_mantiksal(1);
    /* Beklenen: "KEMGU\n42\ndo\xc4\x9fru\n" */
    test_sonuc("Cok cagrili akis -> birlesik tampon",
               strcmp(kdl_uart_pl011_mock_buf,
                      "KEMGU\n42\ndo\xc4\x9f" "ru\n") == 0);
}

int main(void) {
    printf("=== KEMGU Bare-Metal kdl_yazdir_* Test Paketi ===\n");

    puts("\n--- Format yardimcisi ---");
    T1_format_pozitif(); T2_format_sifir(); T3_format_negatif();
    T4_format_int32_min(); T5_format_int64_max();
    T6_format_int64_min(); T7_format_kapasite_yetersiz();

    puts("\n--- yazdir_metin / yaz_metin / yazdir_satir ---");
    T8_yazdir_metin(); T9_yazdir_metin_null();
    T10_yaz_metin(); T11_yazdir_satir();

    puts("\n--- yazdir_tam / yazdir_tam64 ---");
    T12_yazdir_tam_42(); T13_yazdir_tam_negatif();
    T14_yazdir_tam64();

    puts("\n--- yazdir_mantiksal ---");
    T15_yazdir_mantik_dogru(); T16_yazdir_mantik_yanlis();

    puts("\n--- C1: isaretsiz + onaltilik ---");
    T18_isaretsiz_tam(); T19_isaretsiz_tam64_max();
    T20_onaltilik_kucuk(); T21_onaltilik_sifir();
    T22_onaltilik_64bit_max(); T23_onaltilik_pl011_base();
    T24_yaz_onaltilik();

    puts("\n--- Birlesik akis ---");
    T17_program_akisi();

    printf("\n========================================\n");
    printf("Toplam: %d | Basarili: %d | Basarisiz: %d\n",
           toplam_test, basarili, basarisiz);

    return basarisiz == 0 ? 0 : 1;
}
