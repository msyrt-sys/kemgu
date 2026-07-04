/*
 * KEMGU Bare-Metal Metin İlkeleri (kdl_metin_bare.c)
 * ==================================================
 *
 * Faz-2 (.kem-native sürücü katmanı, D-244): `metin_uzunluk` + `metin_bayt`
 * KEMGU builtin'lerinin FREESTANDING (libc-yok) karşılıkları. Host tarafı bunları
 * `kdl_runtime.c`'de strlen/strcmp ile sağlar; o dosya bare-metal'de LİNKLENMEZ
 * (libc'ye bağımlı). Bu dosya aynı simgeleri saf-C ile (manuel uzunluk döngüsü)
 * üretir → .kem kaynağı bare-metal'de string'i BAYT-BAYT gezebilir (ör. .kem UART
 * sürücüsünün metin yazması). ASLA host kdl_runtime.c ile birlikte linklenmez
 * (aynı simge — çift-tanım olur); yalnız gereken bare-metal hedeflere explicit
 * eklenir (bm_a64_mmio.o deseni).
 *
 * Simgeler llvm.c'nin declare'leriyle BİREBİR: i32 kdl_metin_uzunluk(ptr),
 * i8 kdl_metin_bayt(ptr, i32).
 */
#include <stdint.h>

/* metin_uzunluk: null-sonlu string'in bayt uzunluğu (strlen'siz). NULL → 0. */
int32_t kdl_metin_uzunluk(const char *s) {
    if (!s) return 0;
    int32_t n = 0;
    while (s[n]) {
        n++;
    }
    return n;
}

/* metin_bayt: s'in i. HAM BAYT'ı (UTF-8 byte; ASCII'de = karakter). Sınır dışı
 * (i<0 / i>=uzunluk) veya NULL → 0 (güvenli sentinel; sınır taşması imkansız —
 * KEMGU güvenlik hedefi). Host kdl_runtime.c ile aynı semantik. */
int8_t kdl_metin_bayt(const char *s, int32_t i) {
    if (!s || i < 0) return 0;
    int32_t n = 0;
    while (s[n]) {
        n++;
    }
    if (i >= n) return 0;
    return (int8_t)(unsigned char)s[i];
}
