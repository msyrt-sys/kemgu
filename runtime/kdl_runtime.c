/*
 * KEMGU Standart Kutuphane Runtime (KDL — KEMGU Dil Kutuphanesi)
 * ================================================================
 *
 * KEMGU programlari icin C-tarafi runtime destek fonksiyonlari.
 * LLVM IR icinde `declare` ile bildirilen built-in isimleri burada
 * implement edilir. clang ile birlikte link edilir:
 *
 *   ./build/kemgu --llvm prog.kem > prog.ll
 *   clang prog.ll runtime/kdl_runtime.c -o prog.exe
 *
 * Tum fonksiyonlar `kdl_` prefiksli — KEMGU adlandirma cakismalari
 * onlenir. KEMGU LLVM backend tarafindan otomatik bagimlilik kurulur.
 *
 * Kapsam:
 *   D.1 IO:        yazdir_*, hata_yazdir, oku_tam
 *   D.3 Metin:     metin_uzunluk
 *   D.4 Sayisal:   mutlak, min, maks (signed tam32)
 *
 * Turkce karakter cikti: UTF-8 olarak yazilir. Windows konsolunda
 * dogru goruntulemek icin chcp 65001 (varsayilan Win11) yeterli.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/* === D.1 IO === */

void kdl_yazdir_metin(const char *s) {
    if (s) {
        fputs(s, stdout);
        fputc('\n', stdout);
    } else {
        fputs("(bos)\n", stdout);
    }
}

void kdl_yazdir_tam(int32_t n) {
    printf("%d\n", n);
}

void kdl_yazdir_tam64(int64_t n) {
    printf("%lld\n", (long long)n);
}

void kdl_yazdir_kesirli(double x) {
    printf("%g\n", x);
}

void kdl_yazdir_mantiksal(_Bool b) {
    /* _Bool: LLVM i1 ABI ile birebir esler (Clang zeroext + 1 byte storage).
     * "do\xc4\x9fru" / "yanl\xc4\xb1\xc5\x9f" — Turkce UTF-8 */
    fputs(b ? "do\xc4\x9f" "ru" : "yanl\xc4\xb1" "\xc5\x9f", stdout);
    fputc('\n', stdout);
}

/* karakter -> UTF-8 byte dizisine cevirip yazdir.
 * KEMGU 'karakter' tipi i32 (Unicode code point). */
void kdl_yazdir_karakter(int32_t cp) {
    unsigned char buf[5];
    int n = 0;
    uint32_t c = (uint32_t)cp;
    if (c < 0x80u) {
        buf[n++] = (unsigned char)c;
    } else if (c < 0x800u) {
        buf[n++] = (unsigned char)(0xC0u | (c >> 6));
        buf[n++] = (unsigned char)(0x80u | (c & 0x3Fu));
    } else if (c < 0x10000u) {
        buf[n++] = (unsigned char)(0xE0u | (c >> 12));
        buf[n++] = (unsigned char)(0x80u | ((c >> 6) & 0x3Fu));
        buf[n++] = (unsigned char)(0x80u | (c & 0x3Fu));
    } else {
        buf[n++] = (unsigned char)(0xF0u | (c >> 18));
        buf[n++] = (unsigned char)(0x80u | ((c >> 12) & 0x3Fu));
        buf[n++] = (unsigned char)(0x80u | ((c >> 6) & 0x3Fu));
        buf[n++] = (unsigned char)(0x80u | (c & 0x3Fu));
    }
    buf[n] = 0;
    fputs((const char *)buf, stdout);
    fputc('\n', stdout);
}

void kdl_yazdir_satir(void) {
    fputc('\n', stdout);
}

/* yazdir_* versiyonlari satir sonu eklemez */
void kdl_yaz_metin(const char *s) {
    if (s) fputs(s, stdout);
}

void kdl_yaz_tam(int32_t n) {
    printf("%d", n);
}

void kdl_yaz_karakter(int32_t cp) {
    unsigned char buf[5];
    int n = 0;
    uint32_t c = (uint32_t)cp;
    if (c < 0x80u) {
        buf[n++] = (unsigned char)c;
    } else if (c < 0x800u) {
        buf[n++] = (unsigned char)(0xC0u | (c >> 6));
        buf[n++] = (unsigned char)(0x80u | (c & 0x3Fu));
    } else if (c < 0x10000u) {
        buf[n++] = (unsigned char)(0xE0u | (c >> 12));
        buf[n++] = (unsigned char)(0x80u | ((c >> 6) & 0x3Fu));
        buf[n++] = (unsigned char)(0x80u | (c & 0x3Fu));
    } else {
        buf[n++] = (unsigned char)(0xF0u | (c >> 18));
        buf[n++] = (unsigned char)(0x80u | ((c >> 12) & 0x3Fu));
        buf[n++] = (unsigned char)(0x80u | ((c >> 6) & 0x3Fu));
        buf[n++] = (unsigned char)(0x80u | (c & 0x3Fu));
    }
    buf[n] = 0;
    fputs((const char *)buf, stdout);
}

void kdl_hata_yazdir(const char *s) {
    if (s) {
        fputs(s, stderr);
        fputc('\n', stderr);
    } else {
        fputs("(bos)\n", stderr);
    }
}

int32_t kdl_oku_tam(void) {
    int32_t n = 0;
    if (scanf("%d", &n) != 1) return 0;
    return n;
}

/* === D.3 Metin === */

int32_t kdl_metin_uzunluk(const char *s) {
    return s ? (int32_t)strlen(s) : 0;
}

/* === D.4 Sayisal === */

int32_t kdl_mutlak(int32_t x) {
    return x < 0 ? -x : x;
}

int32_t kdl_min(int32_t a, int32_t b) {
    return a < b ? a : b;
}

int32_t kdl_maks(int32_t a, int32_t b) {
    return a > b ? a : b;
}

int64_t kdl_mutlak64(int64_t x) {
    return x < 0 ? -x : x;
}

int64_t kdl_min64(int64_t a, int64_t b) {
    return a < b ? a : b;
}

int64_t kdl_maks64(int64_t a, int64_t b) {
    return a > b ? a : b;
}
