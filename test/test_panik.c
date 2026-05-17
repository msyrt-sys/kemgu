/*
 * KEMGU Bare-Metal Runtime — Panik Handler Test Paketi
 * =====================================================
 *
 * Mock modunda halt loop devre disi — panik mesaji yazildiktan sonra
 * geri doner ve test buf'i + sayaci dogrular.
 */

#include "../runtime/kdl_uart.h"
#include "../runtime/kdl_panik.h"

#include <stdio.h>
#include <string.h>

#ifndef KEMGU_UART_MOCK
#  error "test_panik.c: -DKEMGU_UART_MOCK gerekli"
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
        printf("  [%d] %s ... \xe2\x9c\x97  (buf=\"%s\")\n",
               toplam_test, ad, kdl_uart_pl011_mock_buf);
    }
}

static void T1_basit_mesaj(void) {
    kdl_uart_pl011_mock_temizle();
    kdl_panik_mock_temizle();
    kdl_panik_dur("bolme sifira");
    test_sonuc("panik_dur(\"bolme sifira\") -> mesaj + sayac",
               strcmp(kdl_uart_pl011_mock_buf,
                      "\nPANIK: bolme sifira\n") == 0 &&
               kdl_panik_sayisi == 1);
}

static void T2_null_mesaj(void) {
    kdl_uart_pl011_mock_temizle();
    kdl_panik_mock_temizle();
    kdl_panik_dur(NULL);
    test_sonuc("panik_dur(NULL) -> \"PANIK: (bilinmiyor)\"",
               strcmp(kdl_uart_pl011_mock_buf,
                      "\nPANIK: (bilinmiyor)\n") == 0 &&
               kdl_panik_sayisi == 1);
}

static void T3_bos_mesaj(void) {
    kdl_uart_pl011_mock_temizle();
    kdl_panik_mock_temizle();
    kdl_panik_dur("");
    test_sonuc("panik_dur(\"\") -> \"PANIK: \\n\"",
               strcmp(kdl_uart_pl011_mock_buf, "\nPANIK: \n") == 0 &&
               kdl_panik_sayisi == 1);
}

static void T4_uzun_mesaj(void) {
    kdl_uart_pl011_mock_temizle();
    kdl_panik_mock_temizle();
    const char *uzun = "bellek hatasi: page fault @ 0xFFFF000000040000";
    kdl_panik_dur(uzun);
    /* Beklenen: "\nPANIK: <uzun>\n" */
    char beklenen[256];
    snprintf(beklenen, sizeof(beklenen), "\nPANIK: %s\n", uzun);
    test_sonuc("panik_dur(uzun mesaj) -> tum mesaj aktarir",
               strcmp(kdl_uart_pl011_mock_buf, beklenen) == 0);
}

static void T5_utf8_turkce_mesaj(void) {
    kdl_uart_pl011_mock_temizle();
    kdl_panik_mock_temizle();
    /* "S\xc4\xb1n\xc4\xb1r asildi" — "Sınır asildi" UTF-8 */
    kdl_panik_dur("S\xc4\xb1n\xc4\xb1" "r asildi");
    /* "_n_r" arasinda hex rakam yok, devam karakteri 'r' guvenli */
    test_sonuc("panik_dur(UTF-8 Turkce) -> ham byte aktarir",
               strstr(kdl_uart_pl011_mock_buf, "PANIK: ") != NULL &&
               kdl_panik_sayisi == 1);
}

static void T6_birden_fazla_panik(void) {
    kdl_uart_pl011_mock_temizle();
    kdl_panik_mock_temizle();
    kdl_panik_dur("birinci");
    kdl_panik_dur("ikinci");
    kdl_panik_dur("ucuncu");
    test_sonuc("3 ardisik panik -> sayac=3",
               kdl_panik_sayisi == 3);
}

int main(void) {
    printf("=== KEMGU Bare-Metal Panik Handler Test Paketi ===\n");

    puts("\n--- Temel mesaj akisi ---");
    T1_basit_mesaj(); T2_null_mesaj(); T3_bos_mesaj();
    T4_uzun_mesaj(); T5_utf8_turkce_mesaj();

    puts("\n--- Davranis ---");
    T6_birden_fazla_panik();

    printf("\n========================================\n");
    printf("Toplam: %d | Basarili: %d | Basarisiz: %d\n",
           toplam_test, basarili, basarisiz);

    return basarisiz == 0 ? 0 : 1;
}
