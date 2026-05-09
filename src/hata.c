#include "hata.h"
#include <stdio.h>
#include <string.h>

static const char *satir_baslangici_bul(const char *kaynak, int hedef_satir) {
    const char *p = kaynak;
    int satir = 1;
    while (*p && satir < hedef_satir) {
        if (*p == '\n') satir++;
        p++;
    }
    return p;
}

static int satir_uzunlugu_bul(const char *satir_bas) {
    int uz = 0;
    while (satir_bas[uz] && satir_bas[uz] != '\n' && satir_bas[uz] != '\r') {
        uz++;
    }
    return uz;
}

void hata_raporla(
    const char *dosya_adi,
    const char *kaynak,
    int satir,
    int sutun,
    const char *kod,
    const char *mesaj,
    const char *ipucu
) {
    int satir_genislik = 1;
    int s = satir;
    while (s >= 10) { satir_genislik++; s /= 10; }

    fprintf(stderr, "hata[%s]: %s\n", kod, mesaj);
    fprintf(stderr, "%*s--> %s:%d:%d\n", satir_genislik + 1, "", dosya_adi, satir, sutun);
    fprintf(stderr, "%*s|\n", satir_genislik + 1, "");

    const char *satir_bas = satir_baslangici_bul(kaynak, satir);
    int satir_uz = satir_uzunlugu_bul(satir_bas);

    fprintf(stderr, "%*d | %.*s\n", satir_genislik, satir, satir_uz, satir_bas);
    fprintf(stderr, "%*s|", satir_genislik + 1, "");

    for (int i = 0; i < sutun; i++) {
        fputc(' ', stderr);
    }
    fprintf(stderr, "^\n");

    if (ipucu) {
        fprintf(stderr, "%*s|\n", satir_genislik + 1, "");
        fprintf(stderr, "%*s= ipucu: %s\n", satir_genislik + 1, "", ipucu);
    }

    fprintf(stderr, "\n");
}
