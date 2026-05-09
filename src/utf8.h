#ifndef KEMGU_UTF8_H
#define KEMGU_UTF8_H

/*
 * KEMGU UTF-8 Karakter Tanıma
 *
 * Türkçe karakter whitelist'i ile çalışır.
 * Sadece: ASCII Latin (a-z, A-Z), ASCII rakamlar (0-9), alt çizgi (_),
 * Türkçe harfler: ç Ç ğ Ğ ı İ ö Ö ş Ş ü Ü
 */

int utf8_karakter_uzunlugu(unsigned char ilk_byte);
int utf8_tanimlayici_baslangic_mi(const char *s, int *byte_uzunlugu);
int utf8_tanimlayici_devam_mi(const char *s, int *byte_uzunlugu);
int utf8_rakam_mi(unsigned char c);
int utf8_hex_rakam_mi(unsigned char c);
int utf8_bosluk_mu(unsigned char c);

#endif /* KEMGU_UTF8_H */
