#include "utf8.h"

/*
 * Türkçe karakter UTF-8 byte desenleri:
 *   ç = 0xC3 0xA7    Ç = 0xC3 0x87
 *   ğ = 0xC4 0x9F    Ğ = 0xC4 0x9E
 *   ı = 0xC4 0xB1    İ = 0xC4 0xB0
 *   ö = 0xC3 0xB6    Ö = 0xC3 0x96
 *   ş = 0xC5 0x9F    Ş = 0xC5 0x9E
 *   ü = 0xC3 0xBC    Ü = 0xC3 0x9C
 */

int utf8_karakter_uzunlugu(unsigned char ilk_byte) {
    if (ilk_byte < 0x80) return 1;
    if ((ilk_byte & 0xE0) == 0xC0) return 2;
    if ((ilk_byte & 0xF0) == 0xE0) return 3;
    if ((ilk_byte & 0xF8) == 0xF0) return 4;
    return 0;
}

static int turkce_harf_2byte(unsigned char b0, unsigned char b1) {
    if (b0 == 0xC3) {
        return b1 == 0xA7 || b1 == 0x87   /* ç Ç */
            || b1 == 0xB6 || b1 == 0x96   /* ö Ö */
            || b1 == 0xBC || b1 == 0x9C;  /* ü Ü */
    }
    if (b0 == 0xC4) {
        return b1 == 0x9F || b1 == 0x9E   /* ğ Ğ */
            || b1 == 0xB1 || b1 == 0xB0;  /* ı İ */
    }
    if (b0 == 0xC5) {
        return b1 == 0x9F || b1 == 0x9E;  /* ş Ş */
    }
    return 0;
}

int utf8_tanimlayici_baslangic_mi(const char *s, int *byte_uzunlugu) {
    unsigned char c = (unsigned char)s[0];

    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
        if (byte_uzunlugu) *byte_uzunlugu = 1;
        return 1;
    }

    if ((c & 0xE0) == 0xC0) {
        unsigned char c1 = (unsigned char)s[1];
        if ((c1 & 0xC0) != 0x80) return 0;
        if (turkce_harf_2byte(c, c1)) {
            if (byte_uzunlugu) *byte_uzunlugu = 2;
            return 1;
        }
    }

    return 0;
}

int utf8_tanimlayici_devam_mi(const char *s, int *byte_uzunlugu) {
    unsigned char c = (unsigned char)s[0];
    if (c >= '0' && c <= '9') {
        if (byte_uzunlugu) *byte_uzunlugu = 1;
        return 1;
    }
    return utf8_tanimlayici_baslangic_mi(s, byte_uzunlugu);
}

int utf8_rakam_mi(unsigned char c) {
    return c >= '0' && c <= '9';
}

int utf8_hex_rakam_mi(unsigned char c) {
    return (c >= '0' && c <= '9')
        || (c >= 'a' && c <= 'f')
        || (c >= 'A' && c <= 'F');
}

int utf8_bosluk_mu(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}
