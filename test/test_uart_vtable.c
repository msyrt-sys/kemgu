/*
 * KEMGU Bare-Metal Runtime — UartSurucu vtable Test Paketi
 * =========================================================
 *
 * KdlUartSurucu yapisi uzerinden indirect call ile her iki driverin
 * (PL011 + 16550A) ayni testten gectigi dogrulanir. Bu, ust katman
 * koda "hedef bagimsiz UART tuketici" yazma imkani verir.
 */

#include "../runtime/kdl_uart.h"

#include <stdio.h>
#include <string.h>

#ifndef KEMGU_UART_MOCK
#  error "test_uart_vtable.c: -DKEMGU_UART_MOCK gerekli"
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

/* === Generic test fonksiyonu — herhangi bir surucuyla calisir === */

typedef struct {
    const char *isim;
    const KdlUartSurucu *surucu;
    /* Mock erisim — test tarafindan yonetilir */
    char *(*mock_buf_al)(void);
    uint32_t (*mock_pos_al)(void);
    void (*mock_temizle)(void);
    void (*mock_rx_doldur)(const char *, uint32_t);
} TestBaglam;

/* PL011 mock erisimcileri */
static char *pl011_buf(void) { return kdl_uart_pl011_mock_buf; }
static uint32_t pl011_pos(void) { return kdl_uart_pl011_mock_pos; }

/* 16550 mock erisimcileri */
static char *u16550_buf(void) { return kdl_uart_16550_mock_buf; }
static uint32_t u16550_pos(void) { return kdl_uart_16550_mock_pos; }

static void test_putc_uzerinden(const TestBaglam *t) {
    t->mock_temizle();
    t->surucu->putc('K');
    char buf_tag[80];
    snprintf(buf_tag, sizeof(buf_tag), "[%s] surucu->putc('K') -> buf[0]='K'",
             t->isim);
    test_sonuc(buf_tag,
               t->mock_pos_al() == 1 && t->mock_buf_al()[0] == 'K');
}

static void test_yaz_uzerinden(const TestBaglam *t) {
    t->mock_temizle();
    t->surucu->yaz("KEMGU");
    char buf_tag[80];
    snprintf(buf_tag, sizeof(buf_tag),
             "[%s] surucu->yaz(\"KEMGU\") -> pos=5", t->isim);
    test_sonuc(buf_tag,
               t->mock_pos_al() == 5 &&
               strcmp(t->mock_buf_al(), "KEMGU") == 0);
}

static void test_rx_uzerinden(const TestBaglam *t) {
    t->mock_temizle();
    t->mock_rx_doldur("AB", 2);
    char buf_tag[80];
    int sirayla =
        t->surucu->rx_hazir() == 1 &&
        t->surucu->oku_karakter() == 'A' &&
        t->surucu->oku_karakter() == 'B' &&
        t->surucu->rx_hazir() == 0;
    snprintf(buf_tag, sizeof(buf_tag),
             "[%s] surucu->oku_karakter x2 -> 'A','B'", t->isim);
    test_sonuc(buf_tag, sirayla);
}

static void test_init_uzerinden(const TestBaglam *t) {
    t->mock_temizle();
    t->surucu->init();
    char buf_tag[80];
    snprintf(buf_tag, sizeof(buf_tag),
             "[%s] surucu->init() no-op (panik yok)", t->isim);
    test_sonuc(buf_tag, t->mock_pos_al() == 0);
}

/* === Statik sembol kontrolu — vtable doldurulmus mu? === */

static void T_pl011_vtable_doldurulmus(void) {
    test_sonuc("kdl_uart_pl011_surucu.init non-NULL",
               kdl_uart_pl011_surucu.init != NULL);
    test_sonuc("kdl_uart_pl011_surucu.putc non-NULL",
               kdl_uart_pl011_surucu.putc != NULL);
    test_sonuc("kdl_uart_pl011_surucu.yaz non-NULL",
               kdl_uart_pl011_surucu.yaz != NULL);
    test_sonuc("kdl_uart_pl011_surucu.oku_karakter non-NULL",
               kdl_uart_pl011_surucu.oku_karakter != NULL);
    test_sonuc("kdl_uart_pl011_surucu.rx_hazir non-NULL",
               kdl_uart_pl011_surucu.rx_hazir != NULL);
}

static void T_16550_vtable_doldurulmus(void) {
    test_sonuc("kdl_uart_16550_surucu.init non-NULL",
               kdl_uart_16550_surucu.init != NULL);
    test_sonuc("kdl_uart_16550_surucu.putc non-NULL",
               kdl_uart_16550_surucu.putc != NULL);
    test_sonuc("kdl_uart_16550_surucu.yaz non-NULL",
               kdl_uart_16550_surucu.yaz != NULL);
    test_sonuc("kdl_uart_16550_surucu.oku_karakter non-NULL",
               kdl_uart_16550_surucu.oku_karakter != NULL);
    test_sonuc("kdl_uart_16550_surucu.rx_hazir non-NULL",
               kdl_uart_16550_surucu.rx_hazir != NULL);
}

/* === Sembol esitligi — vtable doğrudan fonksiyonu mu işaretliyor === */

static void T_pl011_vtable_sembol_esit(void) {
    test_sonuc("vtable putc == kdl_uart_pl011_putc",
               kdl_uart_pl011_surucu.putc == kdl_uart_pl011_putc);
    test_sonuc("vtable yaz == kdl_uart_pl011_yaz",
               kdl_uart_pl011_surucu.yaz == kdl_uart_pl011_yaz);
    test_sonuc("vtable oku == kdl_uart_pl011_oku_karakter",
               kdl_uart_pl011_surucu.oku_karakter ==
                   kdl_uart_pl011_oku_karakter);
}

int main(void) {
    printf("=== KEMGU UartSurucu vtable Test Paketi ===\n");

    TestBaglam pl011 = {
        "PL011", &kdl_uart_pl011_surucu,
        pl011_buf, pl011_pos,
        kdl_uart_pl011_mock_temizle,
        kdl_uart_pl011_mock_rx_doldur,
    };
    TestBaglam u16550 = {
        "16550A", &kdl_uart_16550_surucu,
        u16550_buf, u16550_pos,
        kdl_uart_16550_mock_temizle,
        kdl_uart_16550_mock_rx_doldur,
    };

    puts("\n--- vtable doldurulmus kontrolu ---");
    T_pl011_vtable_doldurulmus(); T_16550_vtable_doldurulmus();

    puts("\n--- Sembol esitligi ---");
    T_pl011_vtable_sembol_esit();

    puts("\n--- PL011 vtable uzerinden ---");
    test_init_uzerinden(&pl011);
    test_putc_uzerinden(&pl011);
    test_yaz_uzerinden(&pl011);
    test_rx_uzerinden(&pl011);

    puts("\n--- 16550A vtable uzerinden ---");
    test_init_uzerinden(&u16550);
    test_putc_uzerinden(&u16550);
    test_yaz_uzerinden(&u16550);
    test_rx_uzerinden(&u16550);

    printf("\n========================================\n");
    printf("Toplam: %d | Basarili: %d | Basarisiz: %d\n",
           toplam_test, basarili, basarisiz);

    return basarisiz == 0 ? 0 : 1;
}
