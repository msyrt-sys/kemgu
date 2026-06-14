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

/* B2 (genisletilmis): Gercek thread bind — Windows + POSIX (Linux/macOS/ARM64) */
#ifdef _WIN32
#include <windows.h>
#define KDL_THREAD_VAR 1
#define KDL_THREAD_WIN 1
#elif defined(__linux__) || defined(__APPLE__) || defined(__unix__)
#include <pthread.h>
#define KDL_THREAD_VAR 1
#define KDL_THREAD_POSIX 1
#endif

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

/* Track B C1: isaretsiz + onaltilik formatlar (host paralel kayit). */
void kdl_yazdir_isaretsiz_tam(uint32_t n) {
    printf("%u\n", (unsigned)n);
}

void kdl_yazdir_isaretsiz_tam64(uint64_t n) {
    printf("%llu\n", (unsigned long long)n);
}

void kdl_yazdir_onaltilik(uint64_t n) {
    printf("0x%llx\n", (unsigned long long)n);
}

void kdl_yaz_onaltilik(uint64_t n) {
    printf("0x%llx", (unsigned long long)n);
}

/* Track B D1: kdl_yaz_karakter zaten asagida tanimli (mevcut host runtime
 * kdl_yaz_karakter satir 175 civari) — yeniden tanim cakismayi onlemek
 * icin burayi atla. */

/* Track B D2: oku_karakter host — getchar wrapper */
int32_t kdl_oku_karakter(void) {
    int c = fgetc(stdin);
    if (c == EOF) return -1;
    return (int32_t)(c & 0xFF);
}

/* Track B D3: oku_metin host — fgets benzeri ama CR/LF normalize */
int32_t kdl_oku_metin(char *buf, int32_t max) {
    if (!buf || max <= 1) {
        if (buf && max > 0) buf[0] = '\0';
        return 0;
    }
    int32_t n = 0;
    while (n < max - 1) {
        int c = fgetc(stdin);
        if (c == EOF) break;
        if (c == '\r') continue;
        if (c == '\n') break;
        buf[n++] = (char)(c & 0xFF);
    }
    buf[n] = '\0';
    return n;
}

/* yazdir_* versiyonlari satir sonu eklemez */
void kdl_yaz_metin(const char *s) {
    if (s) fputs(s, stdout);
}

void kdl_yaz_tam(int32_t n) {
    printf("%d", n);
}

/* yaz_bayt: HAM bayt (düşük 8 bit) — putchar. yaz_karakter UTF-8 codepoint
 * encode eder; bu ise ham byte yazar (self-host parser UTF-8 değer dump'ı). */
void kdl_yaz_bayt(int32_t b) {
    putchar(b & 0xFF);
}

/* ondalik_bicimle: float lexeme'i ('_' ayraçlı) C parser gibi strtod + "%g"
 * ile biçimle (self-host parser KESIRLI dump'ı ast_duz_yaz ile birebir). */
const char *kdl_ondalik_bicimle(const char *lex) {
    if (!lex) return "0";
    char buf[64];
    int j = 0;
    for (int i = 0; lex[i] && j < 63; i++) {
        if (lex[i] != '_') buf[j++] = lex[i];
    }
    buf[j] = '\0';
    double d = strtod(buf, NULL);
    char *out = (char *)malloc(64);
    if (!out) return "0";
    snprintf(out, 64, "%g", d);
    return out;
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

/* metin_bayt: s'in i. HAM BAYT'ı (UTF-8 byte; ASCII'de = karakter).
 * Sınır dışı (i<0 veya i>=uzunluk) veya NULL → 0 — tokenizer döngüsü için
 * güvenli sentinel (sınır taşması imkansız, KEMGU güvenlik hedefi). */
int8_t kdl_metin_bayt(const char *s, int32_t i) {
    if (!s || i < 0) return 0;
    int32_t n = (int32_t)strlen(s);
    if (i >= n) return 0;
    return (int8_t)(unsigned char)s[i];
}

/* metin_esit: iki metin byte-byte aynı mı (strcmp == 0). NULL güvenli. */
_Bool kdl_metin_esit(const char *a, const char *b) {
    if (a == b) return 1;
    if (!a || !b) return 0;
    return strcmp(a, b) == 0;
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

/* === J: Metin islemleri (heap alloc) === */

/* Yeni metin allocate eder, kaynaktan kopyalar. NUL-terminator dahil. */
const char *kdl_metin_kopya(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *yeni = (char *)malloc(n + 1);
    if (!yeni) return NULL;
    memcpy(yeni, s, n + 1);
    return yeni;
}

/* Iki metni birlestirir (heap). Sonuc free edilmeli — su an leak OK. */
const char *kdl_metin_birlestir(const char *a, const char *b) {
    if (!a) a = "";
    if (!b) b = "";
    size_t na = strlen(a);
    size_t nb = strlen(b);
    char *yeni = (char *)malloc(na + nb + 1);
    if (!yeni) return NULL;
    memcpy(yeni, a, na);
    memcpy(yeni + na, b, nb);
    yeni[na + nb] = '\0';
    return yeni;
}

/* Metni tam sayiya cevirir (atoi sarmali) */
int32_t kdl_metin_to_tam(const char *s) {
    if (!s) return 0;
    return (int32_t)atoi(s);
}

/* tam32 -> metin (heap, format "%d") */
const char *kdl_tam_to_metin(int32_t n) {
    char *buf = (char *)malloc(16);
    if (!buf) return NULL;
    snprintf(buf, 16, "%d", n);
    return buf;
}

/* === J2: Genisletilmis metin primitifleri (Madde A) ===
 *
 * UTF-8 farkindali; ASCII subset icin O(n) char-by-char islem.
 * Tum doner metinler heap'te (su an leak OK, region API ileride). */

/* metin_kes: substring [baslangic, baslangic+uzunluk).
 * Byte-indeksli (UTF-8 multi-byte sequence kirilabilir; v1 ASCII guvenli). */
const char *kdl_metin_kes(const char *s, int32_t baslangic, int32_t uzunluk) {
    if (!s) return NULL;
    int32_t n = (int32_t)strlen(s);
    if (baslangic < 0) baslangic = 0;
    if (baslangic > n) baslangic = n;
    int32_t kalan = n - baslangic;
    if (uzunluk < 0 || uzunluk > kalan) uzunluk = kalan;
    char *r = (char *)malloc((size_t)uzunluk + 1);
    if (!r) return NULL;
    memcpy(r, s + baslangic, (size_t)uzunluk);
    r[uzunluk] = '\0';
    return r;
}

/* ASCII + Turkce I/i — KEMGU felsefesi: I -> ı, İ -> i.
 * UTF-8: I (0x49) -> ı (0xC4 0xB1), İ (0xC4 0xB0) -> i (0x69). */
const char *kdl_metin_kucuk(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    /* Worst case her ASCII 'I' -> ı (1->2 byte) */
    char *r = (char *)malloc(n * 2 + 1);
    if (!r) return NULL;
    size_t w = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == 'I') {
            r[w++] = (char)0xC4;
            r[w++] = (char)0xB1;            /* ı */
        } else if (c == 0xC4 && i + 1 < n && (unsigned char)s[i+1] == 0xB0) {
            r[w++] = 'i';                    /* İ -> i */
            i++;
        } else if (c >= 'A' && c <= 'Z') {
            r[w++] = (char)(c + 32);
        } else {
            r[w++] = (char)c;
        }
    }
    r[w] = '\0';
    return r;
}

/* ASCII + Turkce: i -> İ, ı -> I. */
const char *kdl_metin_buyuk(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *r = (char *)malloc(n * 2 + 1);
    if (!r) return NULL;
    size_t w = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == 'i') {
            r[w++] = (char)0xC4;
            r[w++] = (char)0xB0;            /* İ */
        } else if (c == 0xC4 && i + 1 < n && (unsigned char)s[i+1] == 0xB1) {
            r[w++] = 'I';                    /* ı -> I */
            i++;
        } else if (c >= 'a' && c <= 'z') {
            r[w++] = (char)(c - 32);
        } else {
            r[w++] = (char)c;
        }
    }
    r[w] = '\0';
    return r;
}

/* === Adim 2: Turkce-aware aliaslari + ASCII-only ek versiyon === */

/* metin_kucuk_tr: I/İ Turkce-aware kucuk (kdl_metin_kucuk ile ayni).
 * Explicit isim — kullanici niyetini netlestirmek icin. */
const char *kdl_metin_kucuk_tr(const char *s) {
    return kdl_metin_kucuk(s);
}

/* metin_buyuk_tr: i/ı Turkce-aware buyuk (kdl_metin_buyuk ile ayni). */
const char *kdl_metin_buyuk_tr(const char *s) {
    return kdl_metin_buyuk(s);
}

/* metin_kucuk_ascii: SAF ASCII A-Z -> a-z. Turkce karakterler degismeden
 * kalir (geri uyumluluk ihtiyaci icin). */
const char *kdl_metin_kucuk_ascii(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *r = (char *)malloc(n + 1);
    if (!r) return NULL;
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        r[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
    }
    r[n] = '\0';
    return r;
}

/* metin_buyuk_ascii: SAF ASCII a-z -> A-Z. */
const char *kdl_metin_buyuk_ascii(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *r = (char *)malloc(n + 1);
    if (!r) return NULL;
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        r[i] = (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
    }
    r[n] = '\0';
    return r;
}

/* metin_icerir: haystack icinde needle var mi (substring) */
_Bool kdl_metin_icerir(const char *haystack, const char *needle) {
    if (!haystack || !needle) return 0;
    return strstr(haystack, needle) != NULL;
}

/* metin_baslar: s prefix ile basliyor mu */
_Bool kdl_metin_baslar(const char *s, const char *prefix) {
    if (!s || !prefix) return 0;
    size_t ns = strlen(s), np = strlen(prefix);
    if (np > ns) return 0;
    return memcmp(s, prefix, np) == 0;
}

/* metin_biter: s suffix ile bitiyor mu */
_Bool kdl_metin_biter(const char *s, const char *suffix) {
    if (!s || !suffix) return 0;
    size_t ns = strlen(s), nb = strlen(suffix);
    if (nb > ns) return 0;
    return memcmp(s + (ns - nb), suffix, nb) == 0;
}

/* metin_kirp: bastaki ve sondaki ASCII whitespace'i temizle (\t \n \r ' ') */
const char *kdl_metin_kirp(const char *s) {
    if (!s) return NULL;
    const char *bas = s;
    while (*bas == ' ' || *bas == '\t' || *bas == '\n' || *bas == '\r') bas++;
    const char *son = s + strlen(s);
    while (son > bas && (son[-1] == ' ' || son[-1] == '\t' ||
                          son[-1] == '\n' || son[-1] == '\r')) son--;
    size_t n = (size_t)(son - bas);
    char *r = (char *)malloc(n + 1);
    if (!r) return NULL;
    memcpy(r, bas, n);
    r[n] = '\0';
    return r;
}

/* metin_yer_degistir: s icindeki tum eski_p alt-metinlerini yeni_p ile degistir.
 * eski_p bos string ise s'in kopyasini doner. */
const char *kdl_metin_yer_degistir(const char *s, const char *eski_p,
                                    const char *yeni_p) {
    if (!s) return NULL;
    if (!eski_p) eski_p = "";
    if (!yeni_p) yeni_p = "";
    size_t ns = strlen(s);
    size_t ne = strlen(eski_p);
    size_t ny = strlen(yeni_p);
    if (ne == 0) {
        char *r = (char *)malloc(ns + 1);
        if (!r) return NULL;
        memcpy(r, s, ns + 1);
        return r;
    }
    /* Once eski_p sayisini bul */
    size_t sayi = 0;
    const char *p = s;
    while ((p = strstr(p, eski_p))) { sayi++; p += ne; }
    /* Yeni boyut: ns + sayi*(ny - ne) (sayisiz, isaretli aritmetik) */
    size_t nr = (ny >= ne)
              ? ns + sayi * (ny - ne)
              : ns - sayi * (ne - ny);
    char *r = (char *)malloc(nr + 1);
    if (!r) return NULL;
    char *dst = r;
    const char *src = s;
    while (1) {
        const char *bul = strstr(src, eski_p);
        if (!bul) {
            size_t kalan = strlen(src);
            memcpy(dst, src, kalan + 1);
            break;
        }
        size_t blen = (size_t)(bul - src);
        memcpy(dst, src, blen);
        dst += blen;
        memcpy(dst, yeni_p, ny);
        dst += ny;
        src = bul + ne;
    }
    return r;
}

/* === I: Dinamik Dizi (heap) ===
 *
 * KDL DinamikDizi temsili: { ptr veri, i32 boyut, i32 kapasite } yapisi.
 * Alanlar: tam32 indexlenir; eleman tipi runtime-bilinmez (eleman_byte verilir).
 * Bu KEMGU tarafindan kullanilan basit C-tarafi runtime yapilari.
 */

typedef struct {
    void *veri;
    int32_t boyut;
    int32_t kapasite;
    int32_t eleman_byte;
} KdlDizi;

KdlDizi *kdl_dizi_olustur(int32_t eleman_byte) {
    KdlDizi *d = (KdlDizi *)malloc(sizeof(KdlDizi));
    if (!d) return NULL;
    d->veri = NULL;
    d->boyut = 0;
    d->kapasite = 0;
    d->eleman_byte = eleman_byte;
    return d;
}

void kdl_dizi_ekle_tam(KdlDizi *d, int32_t deger) {
    if (!d) return;
    if (d->boyut == d->kapasite) {
        int32_t yk = d->kapasite ? d->kapasite * 2 : 4;
        d->veri = realloc(d->veri, (size_t)yk * sizeof(int32_t));
        d->kapasite = yk;
    }
    ((int32_t *)d->veri)[d->boyut++] = deger;
}

void kdl_dizi_ekle_tam64(KdlDizi *d, int64_t deger) {
    if (!d) return;
    if (d->boyut == d->kapasite) {
        int32_t yk = d->kapasite ? d->kapasite * 2 : 4;
        d->veri = realloc(d->veri, (size_t)yk * sizeof(int64_t));
        d->kapasite = yk;
    }
    ((int64_t *)d->veri)[d->boyut++] = deger;
}

void kdl_dizi_ekle_ptr(KdlDizi *d, void *deger) {
    if (!d) return;
    if (d->boyut == d->kapasite) {
        int32_t yk = d->kapasite ? d->kapasite * 2 : 4;
        d->veri = realloc(d->veri, (size_t)yk * sizeof(void *));
        d->kapasite = yk;
    }
    ((void **)d->veri)[d->boyut++] = deger;
}

/* ===================================================================
 * Dizi sınır-güvenliği (D-069): OOB erişim → temiz PANIC, asla segfault
 * / sessiz-0 / sessiz-noop. Hosted runtime: stderr + abort() (rc=134).
 * Bare-metal panic ayrı (kdl_runtime_panik.c → kdl_panik_dur, halt).
 * =================================================================== */
__attribute__((noreturn)) void kdl_panik(const char *mesaj) {
    fprintf(stderr, "PANIK: %s\n", mesaj ? mesaj : "(bilinmiyor)");
    fflush(stderr);
    abort();
}
/* Dizi sınır ihlali yardımcısı — mesajı (i, boyut) ile biçimler, panic eder. */
static __attribute__((noreturn)) void kdl_dizi_oob(int32_t i, int32_t boyut) {
    char buf[96];
    snprintf(buf, sizeof(buf),
             "dizi s\xc4\xb1n\xc4\xb1r ihlali (i=%d, boyut=%d)", i, boyut);
    kdl_panik(buf);
}

int32_t kdl_dizi_al_tam(KdlDizi *d, int32_t i) {
    if (!d) return 0;
    if (i < 0 || i >= d->boyut) kdl_dizi_oob(i, d->boyut);
    return ((int32_t *)d->veri)[i];
}

int64_t kdl_dizi_al_tam64(KdlDizi *d, int32_t i) {
    if (!d) return 0;
    if (i < 0 || i >= d->boyut) kdl_dizi_oob(i, d->boyut);
    return ((int64_t *)d->veri)[i];
}

void *kdl_dizi_al_ptr(KdlDizi *d, int32_t i) {
    if (!d) return NULL;
    if (i < 0 || i >= d->boyut) kdl_dizi_oob(i, d->boyut);
    return ((void **)d->veri)[i];
}

/* dizi_yaz: i. elemanı YERİNDE günceller (dizi_al'ın yazma eşi). Sınır dışı
 * (i<0 || i>=boyut) → PANIC (D-069; eskiden sessiz-noop). NULL → atla. */
void kdl_dizi_yaz_tam(KdlDizi *d, int32_t i, int32_t deger) {
    if (!d) return;
    if (i < 0 || i >= d->boyut) kdl_dizi_oob(i, d->boyut);
    ((int32_t *)d->veri)[i] = deger;
}

void kdl_dizi_yaz_tam64(KdlDizi *d, int32_t i, int64_t deger) {
    if (!d) return;
    if (i < 0 || i >= d->boyut) kdl_dizi_oob(i, d->boyut);
    ((int64_t *)d->veri)[i] = deger;
}

void kdl_dizi_yaz_ptr(KdlDizi *d, int32_t i, void *deger) {
    if (!d) return;
    if (i < 0 || i >= d->boyut) kdl_dizi_oob(i, d->boyut);
    ((void **)d->veri)[i] = deger;
}

int32_t kdl_dizi_boyut(KdlDizi *d) {
    return d ? d->boyut : 0;
}

/* Adim 6: capacity (allocate edilmis alan) — boyut <= kapasite. */
int32_t kdl_dizi_kapasite(KdlDizi *d) {
    return d ? d->kapasite : 0;
}

/* Adim 6: önceden kapasite ayarla. Yeni kapasite mevcut kapasiteden
 * kucukse hicbir sey yapma (shrink yok v1). Realloc bir kez yapilir,
 * sonraki dizi_ekle cagrilari realloc'tan kacar. */
void kdl_dizi_kapasite_ayarla(KdlDizi *d, int32_t yeni_kapasite) {
    if (!d || yeni_kapasite <= d->kapasite) return;
    /* Eleman byte'i bilinmiyor — d->eleman_byte kullan */
    int32_t eb = d->eleman_byte > 0 ? d->eleman_byte : 4;
    void *yeni = realloc(d->veri, (size_t)yeni_kapasite * (size_t)eb);
    if (!yeni) return;
    d->veri = yeni;
    d->kapasite = yeni_kapasite;
}

void kdl_dizi_serbest(KdlDizi *d) {
    if (!d) return;
    free(d->veri);
    free(d);
}

/* === B2: Concurrency minimal API ===
 *
 * Bu surumde sequential stub'lar (KEMGU programları thread spawn API'sini
 * kullanabilir; runtime suanlik aynı thread'te çalıştırır). Gerçek thread
 * (Windows CreateThread / pthread) ileri surumde — region sistemi
 * R-GÖREV / R-BİRLEŞTİR / R-KANAL aksiyomlari hazirdir.
 *
 * Kanal: basit FIFO queue. Thread-safe degil (sequential).
 */

typedef struct {
    int32_t result;
    int done;
    int32_t (*f)(void);     /* gorev islevi */
#ifdef KDL_THREAD_WIN
    HANDLE handle;
#endif
#ifdef KDL_THREAD_POSIX
    pthread_t thr;
    int thr_valid;
#endif
} KdlGorev;

#ifdef KDL_THREAD_WIN
static DWORD WINAPI kdl_gorev_thread_main(LPVOID param) {
    KdlGorev *g = (KdlGorev *)param;
    g->result = g->f ? g->f() : 0;
    g->done = 1;
    return 0;
}
#endif
#ifdef KDL_THREAD_POSIX
static void *kdl_gorev_thread_posix(void *param) {
    KdlGorev *g = (KdlGorev *)param;
    g->result = g->f ? g->f() : 0;
    g->done = 1;
    return NULL;
}
#endif

/* islev pointer alir, ya gercek thread spawn ya sequential calistirir */
KdlGorev *kdl_gorev_basla_i32(int32_t (*f)(void)) {
    KdlGorev *g = (KdlGorev *)malloc(sizeof(KdlGorev));
    if (!g) return NULL;
    g->f = f;
    g->result = 0;
    g->done = 0;
#ifdef KDL_THREAD_WIN
    g->handle = CreateThread(NULL, 0, kdl_gorev_thread_main, g, 0, NULL);
    if (!g->handle) {
        g->result = f ? f() : 0;
        g->done = 1;
    }
#elif defined(KDL_THREAD_POSIX)
    g->thr_valid = (pthread_create(&g->thr, NULL,
                                    kdl_gorev_thread_posix, g) == 0);
    if (!g->thr_valid) {
        g->result = f ? f() : 0;
        g->done = 1;
    }
#else
    /* Thread yok -> sequential */
    g->result = f ? f() : 0;
    g->done = 1;
#endif
    return g;
}

int32_t kdl_gorev_birlestir(KdlGorev *g) {
    if (!g) return 0;
#ifdef KDL_THREAD_WIN
    if (g->handle) {
        WaitForSingleObject(g->handle, INFINITE);
        CloseHandle(g->handle);
    }
#elif defined(KDL_THREAD_POSIX)
    if (g->thr_valid) pthread_join(g->thr, NULL);
#endif
    int32_t r = g->result;
    free(g);
    return r;
}

typedef struct {
    int32_t *veri;
    int32_t boyut;
    int32_t kapasite;
    int32_t bas, son;   /* circular buffer */
#ifdef KDL_THREAD_WIN
    CRITICAL_SECTION kilit;
    int kilit_aktif;
#endif
#ifdef KDL_THREAD_POSIX
    pthread_mutex_t kilit;
    int kilit_aktif;
#endif
} KdlKanal;

/* Portable kilit yardimcilari */
static void kdl_kilit_init(KdlKanal *k) {
#ifdef KDL_THREAD_WIN
    InitializeCriticalSection(&k->kilit);
    k->kilit_aktif = 1;
#elif defined(KDL_THREAD_POSIX)
    pthread_mutex_init(&k->kilit, NULL);
    k->kilit_aktif = 1;
#else
    (void)k;
#endif
}
static void kdl_kilit_gir(KdlKanal *k) {
#ifdef KDL_THREAD_WIN
    if (k->kilit_aktif) EnterCriticalSection(&k->kilit);
#elif defined(KDL_THREAD_POSIX)
    if (k->kilit_aktif) pthread_mutex_lock(&k->kilit);
#else
    (void)k;
#endif
}
static void kdl_kilit_cik(KdlKanal *k) {
#ifdef KDL_THREAD_WIN
    if (k->kilit_aktif) LeaveCriticalSection(&k->kilit);
#elif defined(KDL_THREAD_POSIX)
    if (k->kilit_aktif) pthread_mutex_unlock(&k->kilit);
#else
    (void)k;
#endif
}
static void kdl_kilit_yok_et(KdlKanal *k) {
#ifdef KDL_THREAD_WIN
    if (k->kilit_aktif) DeleteCriticalSection(&k->kilit);
#elif defined(KDL_THREAD_POSIX)
    if (k->kilit_aktif) pthread_mutex_destroy(&k->kilit);
#else
    (void)k;
#endif
}

KdlKanal *kdl_kanal_olustur(int32_t kapasite) {
    KdlKanal *k = (KdlKanal *)malloc(sizeof(KdlKanal));
    if (!k) return NULL;
    int32_t kap = kapasite > 0 ? kapasite : 16;
    k->veri = (int32_t *)malloc((size_t)kap * sizeof(int32_t));
    k->kapasite = kap;
    k->boyut = 0;
    k->bas = 0;
    k->son = 0;
    kdl_kilit_init(k);
    return k;
}

void kdl_kanal_gonder(KdlKanal *k, int32_t deger) {
    if (!k) return;
    kdl_kilit_gir(k);
    if (k->boyut < k->kapasite) {
        k->veri[k->son] = deger;
        k->son = (k->son + 1) % k->kapasite;
        k->boyut++;
    }
    kdl_kilit_cik(k);
}

int32_t kdl_kanal_al(KdlKanal *k) {
    if (!k) return 0;
    int32_t v = 0;
    kdl_kilit_gir(k);
    if (k->boyut > 0) {
        v = k->veri[k->bas];
        k->bas = (k->bas + 1) % k->kapasite;
        k->boyut--;
    }
    kdl_kilit_cik(k);
    return v;
}

int32_t kdl_kanal_bos_mu(KdlKanal *k) {
    return k ? (k->boyut == 0) : 1;
}

void kdl_kanal_serbest(KdlKanal *k) {
    if (!k) return;
    kdl_kilit_yok_et(k);
    free(k->veri);
    free(k);
}

/* === Arena bellek modeli (Bolge-tabanli, GC-yok) ===
 *
 * KEMGU'nun temel ozelligi: bolge (region) tabanli bellek. Bir bolgenin
 * omru biter -> tum tahsisleri tek seferde serbest. KEMGU compiler arena'si
 * (src/arena.c) compile-time icin; bu runtime arena'si KEMGU programlari
 * icindir.
 *
 * Implementasyon: bump allocator (linked chunks). Chunk dolarsa yeni
 * chunk ayirilir; arena_serbest tum chunklari free eder.
 */

typedef struct KdlArenaChunk {
    char *buf;
    size_t kullanildi;
    size_t kapasite;
    struct KdlArenaChunk *sonraki;
} KdlArenaChunk;

typedef struct {
    KdlArenaChunk *bas;
    KdlArenaChunk *aktif;
    size_t toplam_tahsis;   /* istatistik: ayrilan toplam byte */
} KdlArena;

#define KDL_ARENA_CHUNK_VARSAYILAN 4096

static KdlArenaChunk *kdl_chunk_olustur(size_t kapasite) {
    KdlArenaChunk *ch = (KdlArenaChunk *)malloc(sizeof(KdlArenaChunk));
    if (!ch) return NULL;
    ch->buf = (char *)malloc(kapasite);
    if (!ch->buf) { free(ch); return NULL; }
    ch->kullanildi = 0;
    ch->kapasite = kapasite;
    ch->sonraki = NULL;
    return ch;
}

KdlArena *kdl_bolge_olustur(void) {
    KdlArena *a = (KdlArena *)malloc(sizeof(KdlArena));
    if (!a) return NULL;
    a->bas = kdl_chunk_olustur(KDL_ARENA_CHUNK_VARSAYILAN);
    if (!a->bas) { free(a); return NULL; }
    a->aktif = a->bas;
    a->toplam_tahsis = 0;
    return a;
}

/* boyut byte ayir, hizalama 8-byte */
void *kdl_bolge_ayir(KdlArena *a, int32_t boyut) {
    if (!a || boyut <= 0) return NULL;
    size_t hizalanmis = (size_t)((boyut + 7) & ~7);
    /* Aktif chunkta yer var mi? */
    if (a->aktif->kullanildi + hizalanmis > a->aktif->kapasite) {
        size_t yeni_kap = hizalanmis > KDL_ARENA_CHUNK_VARSAYILAN
                          ? hizalanmis * 2 : KDL_ARENA_CHUNK_VARSAYILAN;
        KdlArenaChunk *yeni = kdl_chunk_olustur(yeni_kap);
        if (!yeni) return NULL;
        a->aktif->sonraki = yeni;
        a->aktif = yeni;
    }
    void *p = a->aktif->buf + a->aktif->kullanildi;
    a->aktif->kullanildi += hizalanmis;
    a->toplam_tahsis += hizalanmis;
    return p;
}

void kdl_bolge_serbest(KdlArena *a) {
    if (!a) return;
    KdlArenaChunk *c = a->bas;
    while (c) {
        KdlArenaChunk *s = c->sonraki;
        free(c->buf);
        free(c);
        c = s;
    }
    free(a);
}

int32_t kdl_bolge_toplam_byte(KdlArena *a) {
    return a ? (int32_t)a->toplam_tahsis : 0;
}

/* === Dosya I/O (libc fopen/fread/fwrite/fclose wraps) === */

/* Dosya ac: mod stringi "okuma"/"yazma"/"ekleme" (UTF-8 KEMGU) -> "rb"/"wb"/"ab" */
void *kdl_dosya_ac(const char *yol, const char *mod) {
    if (!yol || !mod) return NULL;
    const char *c_mod = "rb";
    /* "okuma" 5 byte, "yazma" 5 byte, "ekleme" 6 byte */
    if (strcmp(mod, "yazma") == 0) c_mod = "wb";
    else if (strcmp(mod, "ekleme") == 0) c_mod = "ab";
    return (void *)fopen(yol, c_mod);
}

/* Dosyadan satir oku — heap'te metin doner (null = EOF) */
const char *kdl_dosya_satir_oku(void *f) {
    if (!f) return NULL;
    char *buf = (char *)malloc(4096);
    if (!buf) return NULL;
    if (!fgets(buf, 4096, (FILE *)f)) {
        free(buf);
        return NULL;
    }
    /* Satir sonu '\n' kaldir */
    size_t n = strlen(buf);
    if (n > 0 && buf[n-1] == '\n') buf[n-1] = '\0';
    if (n > 1 && buf[n-2] == '\r') buf[n-2] = '\0';
    return buf;
}

/* Dosyaya metin yaz (newline eklenmez) */
int32_t kdl_dosya_yaz(void *f, const char *s) {
    if (!f || !s) return 0;
    return (int32_t)fwrite(s, 1, strlen(s), (FILE *)f);
}

int32_t kdl_dosya_yazdir(void *f, const char *s) {
    if (!f || !s) return 0;
    size_t n = strlen(s);
    fwrite(s, 1, n, (FILE *)f);
    fputc('\n', (FILE *)f);
    return (int32_t)(n + 1);
}

void kdl_dosya_kapat(void *f) {
    if (f) fclose((FILE *)f);
}

int32_t kdl_dosya_bitti_mi(void *f) {
    return f ? feof((FILE *)f) : 1;
}

/* Tum dosyayi oku (heap) */
const char *kdl_dosya_tumu_oku(const char *yol) {
    FILE *f = fopen(yol, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f);
    buf[r] = '\0';
    fclose(f);
    return buf;
}

/* === G: Dosya yonetim primitifleri === */

/* dosya_oku — kdl_dosya_tumu_oku alias (KEMGU API ad uyumu icin) */
const char *kdl_dosya_oku(const char *yol) {
    return kdl_dosya_tumu_oku(yol);
}

/* dosya_var_mi — fopen kontrolu (stat yerine portable) */
int kdl_dosya_var_mi(const char *yol) {
    if (!yol) return 0;
    FILE *f = fopen(yol, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

/* dosya_sil — remove(yol). Basari 0, hata !=0 */
int32_t kdl_dosya_sil(const char *yol) {
    if (!yol) return -1;
    return (int32_t)remove(yol);
}

/* dosya_yeniden_adlandir — rename(eski, yeni) */
int32_t kdl_dosya_yeniden_adlandir(const char *eski, const char *yeni) {
    if (!eski || !yeni) return -1;
    return (int32_t)rename(eski, yeni);
}

/* dosya_boyut — fseek/ftell ile byte sayisi (yoksa -1) */
int64_t kdl_dosya_boyut(const char *yol) {
    if (!yol) return -1;
    FILE *f = fopen(yol, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fclose(f);
    return n < 0 ? -1 : (int64_t)n;
}

/* Arena-aware metin birlestirme (verilen bolgeye yerlestirir) */
const char *kdl_bolge_metin_birlestir(KdlArena *a,
                                       const char *x, const char *y) {
    if (!x) x = "";
    if (!y) y = "";
    size_t nx = strlen(x);
    size_t ny = strlen(y);
    char *r = (char *)kdl_bolge_ayir(a, (int32_t)(nx + ny + 1));
    if (!r) return NULL;
    memcpy(r, x, nx);
    memcpy(r + nx, y, ny);
    r[nx + ny] = '\0';
    return r;
}

/* === CLI args (OTP CLI demo icin) ===
 *
 * Windows: GetCommandLineA() parse edilir (basit, quoted token destekli).
 * POSIX: /proc/self/cmdline okunup NULL-separated arglara bolunur.
 * Sonuc: kdl_args (char **) ve kdl_arg_sayisi global'leri.
 *
 * Lazy init — ilk arg_sayi() veya arg_al() cagrisinda doldurulur. */

static char **kdl_args = NULL;
static int32_t kdl_arg_sayisi = 0;
static int kdl_args_baslatildi = 0;

#ifdef _WIN32
static void kdl_args_init_win(void) {
    LPSTR cmd = GetCommandLineA();
    if (!cmd) return;
    static char buf[4096];
    size_t cmdlen = strlen(cmd);
    if (cmdlen >= sizeof(buf)) cmdlen = sizeof(buf) - 1;
    memcpy(buf, cmd, cmdlen);
    buf[cmdlen] = '\0';

    static char *parts[128];
    int32_t n = 0;
    char *p = buf;
    while (*p && n < 127) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        if (*p == '"') {
            /* Quoted: ilerle, " bul */
            p++;
            parts[n++] = p;
            while (*p && *p != '"') p++;
            if (*p == '"') *p++ = '\0';
        } else {
            parts[n++] = p;
            while (*p && *p != ' ' && *p != '\t') p++;
            if (*p) *p++ = '\0';
        }
    }
    kdl_args = parts;
    kdl_arg_sayisi = n;
}
#endif

static void kdl_args_baslat(void) {
    if (kdl_args_baslatildi) return;
    kdl_args_baslatildi = 1;
#ifdef _WIN32
    kdl_args_init_win();
#else
    /* POSIX: /proc/self/cmdline okuyalim */
    FILE *f = fopen("/proc/self/cmdline", "rb");
    if (!f) return;
    static char buf[4096];
    size_t got = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    if (got == 0) return;
    buf[got] = '\0';

    static char *parts[128];
    int32_t n = 0;
    size_t i = 0;
    while (i < got && n < 127) {
        parts[n++] = buf + i;
        while (i < got && buf[i] != '\0') i++;
        i++;  /* NULL atla */
    }
    kdl_args = parts;
    kdl_arg_sayisi = n;
#endif
}

int32_t kdl_arg_sayi(void) {
    kdl_args_baslat();
    return kdl_arg_sayisi;
}

const char *kdl_arg_al(int32_t i) {
    kdl_args_baslat();
    if (i < 0 || i >= kdl_arg_sayisi || !kdl_args) return "";
    return kdl_args[i];
}

/* === Binary dosya I/O (OTP CLI icin) === */

/* dosya_yaz_byte: ham byte dizisini dosyaya yaz (boyut byte yazilir).
 * Mod "wb" — uzeryine yazar. byteler ham KdlDizi* (kdl_dizi_*). */
int32_t kdl_dosya_yaz_byte(const char *yol, const char *byteler, int32_t boyut) {
    if (!yol || !byteler || boyut < 0) return -1;
    FILE *f = fopen(yol, "wb");
    if (!f) return -1;
    size_t yazildi = fwrite(byteler, 1, (size_t)boyut, f);
    fclose(f);
    return (int32_t)yazildi;
}

/* dosya_oku_byte: tum dosyayi byte dizisi olarak oku (heap-allocated).
 * boyut_out NULL degilse boyutu yazar. NULL -> dosya yok / hata. */
const char *kdl_dosya_oku_byte(const char *yol, int32_t *boyut_out) {
    if (!yol) { if (boyut_out) *boyut_out = -1; return NULL; }
    FILE *f = fopen(yol, "rb");
    if (!f) { if (boyut_out) *boyut_out = -1; return NULL; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); if (boyut_out) *boyut_out = -1; return NULL; }
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); if (boyut_out) *boyut_out = -1; return NULL; }
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[got] = '\0';
    if (boyut_out) *boyut_out = (int32_t)got;
    return buf;
}

/* === OTP yardimcilari (Linear Types Spec V1 ile entegre) === */

/* Basit deterministic PRNG (xorshift64). TRNG sonra. Seed-able. */
static uint64_t kdl_prng_state = 0x123456789abcdef0ULL;

void kdl_prng_seed(uint64_t s) {
    if (s == 0) s = 0x123456789abcdef0ULL;
    kdl_prng_state = s;
}

uint64_t kdl_prng_next64(void) {
    uint64_t x = kdl_prng_state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    kdl_prng_state = x;
    return x;
}

/* kdl_otp_anahtar_uret: PRNG ile N byte random key uretir, dosyaya yazar.
 * Donus: yazılan byte sayısı (hata: -1). */
int32_t kdl_otp_anahtar_uret(const char *yol, int32_t boyut) {
    if (!yol || boyut <= 0) return -1;
    char *buf = (char *)malloc((size_t)boyut);
    if (!buf) return -1;
    for (int32_t i = 0; i < boyut; i++) {
        buf[i] = (char)(kdl_prng_next64() & 0xff);
    }
    FILE *f = fopen(yol, "wb");
    if (!f) { free(buf); return -1; }
    size_t yazildi = fwrite(buf, 1, (size_t)boyut, f);
    fclose(f);
    /* Zeroize buf — anahtar artik dosyada, hafizada kalmasin. */
    memset(buf, 0, (size_t)boyut);
    free(buf);
    return (int32_t)yazildi;
}

/* === Capability Spec V1 — yetki<R> object-capability runtime === *
 *
 * Layout (16 byte):
 *   uint64_t id          ; unforgeable PRNG token (id=0 reserved invalid)
 *   uint16_t kaynak_tipi ; 1=Dosya 2=Soket 3=Bellek 4=Donanim 5=OTP_Anahtar
 *   uint16_t izin        ; bit field (CP.5: 1=OKU 2=YAZ 4=CALISTIR 8=SIL 16=DEVRET)
 *   uint8_t  iptal       ; 0=aktif, 1=revoked (geri_al sonrasi)
 *   uint8_t  rezerv[3]   ; alignment
 *
 * Tasarim disiplin:
 *   - id unforgeable (xorshift PRNG; v2 CSPRNG)
 *   - iptal flag tek-yon (0->1), geri donmez
 *   - izin subset-check delege'de (yeni & ~y.izin == 0)
 *   - kaynak_tipi sabit (V1: kt yalniz olusturmada)
 *
 * Hata kodlari (kdl_yetki_kontrol return):
 *    0 = OK
 *   -2 = CP002 (revoked)
 *   -3 = CP003 (permission insufficient)
 *   -4 = CP004 (type mismatch — yetki_kontrol_tipi variant'inda) */

#define KDL_KAYNAK_DOSYA        1
#define KDL_KAYNAK_SOKET        2
#define KDL_KAYNAK_BELLEK       3
#define KDL_KAYNAK_DONANIM      4
#define KDL_KAYNAK_OTP_ANAHTAR  5

#define KDL_IZIN_OKU       0x0001
#define KDL_IZIN_YAZ       0x0002
#define KDL_IZIN_CALISTIR  0x0004
#define KDL_IZIN_SIL       0x0008
#define KDL_IZIN_DEVRET    0x0010
#define KDL_IZIN_HEPSI     0x8000

typedef struct {
    uint64_t id;
    uint16_t kaynak_tipi;
    uint16_t izin;
    uint8_t  iptal;
    uint8_t  rezerv[3];
} KdlYetki;

/* PRNG global state — kdl_prng_state'i kullaniyoruz ama ayri seed mantigi.
 * id=0 reserved invalid, dolayisiyla 0 doncerse yeniden uret. */
static uint64_t kdl_yetki_id_uret(void) {
    uint64_t id;
    do {
        id = kdl_prng_next64();
    } while (id == 0);
    return id;
}

/* Yetki olustur — kt=kaynak tipi (1-5), izin=bit field */
KdlYetki kdl_yetki_olustur(uint16_t kt, uint16_t izin) {
    KdlYetki y;
    y.id = kdl_yetki_id_uret();
    y.kaynak_tipi = kt;
    y.izin = izin;
    y.iptal = 0;
    y.rezerv[0] = y.rezerv[1] = y.rezerv[2] = 0;
    return y;
}

/* Alt-yetki uret — y *kalir*; y2 yeni id, ayni tip, yeni_izin & y.izin
 * (subset). yeni_izin & ~y.izin != 0 ise tum 0 doner (CP003 placeholder;
 * compile-time delege'de iznin literal olmasi tercih edilir). */
KdlYetki kdl_yetki_delege(KdlYetki y, uint16_t yeni_izin) {
    KdlYetki y2;
    /* Subset check: yeni & ~y.izin must be 0 */
    if ((uint16_t)(yeni_izin & ~y.izin) != 0) {
        /* CP003 — invalid yetki dondur (id=0) */
        y2.id = 0;
        y2.kaynak_tipi = y.kaynak_tipi;
        y2.izin = 0;
        y2.iptal = 1;  /* zaten gecersiz */
        y2.rezerv[0] = y2.rezerv[1] = y2.rezerv[2] = 0;
        return y2;
    }
    y2.id = kdl_yetki_id_uret();
    y2.kaynak_tipi = y.kaynak_tipi;
    y2.izin = yeni_izin & y.izin;
    y2.iptal = 0;
    y2.rezerv[0] = y2.rezerv[1] = y2.rezerv[2] = 0;
    return y2;
}

/* geri_al — mutate: iptal=1. Pointer alir cunku KEMGU 'geri_al(y)' linear
 * tuketim sonrasi y kullanilamaz ama runtime'da gercek nesne flag set. */
void kdl_yetki_geri_al(KdlYetki *y) {
    if (y) {
        y->iptal = 1;
    }
}

/* Yetki kontrol — runtime check (iptal + izin). I/O sarmalayicilarinda
 * cagrilir. Donus:
 *    0 = OK
 *   -2 = CP002 (revoked)
 *   -3 = CP003 (permission insufficient) */
int32_t kdl_yetki_kontrol(KdlYetki y, uint16_t gerekli) {
    if (y.iptal) return -2;
    if (y.id == 0) return -2;  /* invalid yetki */
    if ((uint16_t)(y.izin & gerekli) != gerekli) return -3;
    return 0;
}

/* Tip + izin kontrol — kaynak tipi de check edilir (CP004) */
int32_t kdl_yetki_kontrol_tipi(KdlYetki y, uint16_t beklenen_tip,
                                uint16_t gerekli) {
    if (y.kaynak_tipi != beklenen_tip) return -4;  /* CP004 */
    return kdl_yetki_kontrol(y, gerekli);
}

uint64_t kdl_yetki_id(KdlYetki y) {
    return y.id;
}

uint16_t kdl_yetki_tipi(KdlYetki y) {
    return y.kaynak_tipi;
}

uint16_t kdl_yetki_izin(KdlYetki y) {
    return y.izin;
}

uint8_t kdl_yetki_iptal_mi(KdlYetki y) {
    return y.iptal;
}

/* === Yetki-gated I/O sarmalayicilari (Capability spec CP.7) === */

/* dosya_ac_yetkili: yol+izin ile aç, başarılı ise yetki<Dosya> döner.
 * id=0 -> başarısız. izin bit-field (KDL_IZIN_OKU=1, YAZ=2, ...) */
KdlYetki kdl_dosya_ac_yetkili(const char *yol, uint16_t izin) {
    KdlYetki y;
    y.id = 0;
    y.kaynak_tipi = KDL_KAYNAK_DOSYA;
    y.izin = 0;
    y.iptal = 1;
    y.rezerv[0] = y.rezerv[1] = y.rezerv[2] = 0;
    if (!yol) return y;
    /* Mode string seç */
    const char *c_mod = NULL;
    if (izin & KDL_IZIN_YAZ) c_mod = (izin & KDL_IZIN_OKU) ? "wb+" : "wb";
    else if (izin & KDL_IZIN_OKU) c_mod = "rb";
    else return y;  /* izin yok = ac yok */
    FILE *f = fopen(yol, c_mod);
    if (!f) return y;
    /* Basari: yetki uret + handle bir global tabloya ekle (basit v1: id->FILE*)
     * V1'de FILE* dogrudan id yerine kullanilir (id=ptr value, unforgeable yok).
     * Bu Phase 1 — kosmaktan onceki minimal calistirma. V2'de proper id+tablo. */
    y.id = (uint64_t)(uintptr_t)f;
    y.izin = izin;
    y.iptal = 0;
    return y;
}

/* dosya_oku_yetkili: yetki kontrol + tum dosyayi metin oku */
const char *kdl_dosya_oku_yetkili(KdlYetki y) {
    if (kdl_yetki_kontrol_tipi(y, KDL_KAYNAK_DOSYA, KDL_IZIN_OKU) != 0) {
        return NULL;
    }
    FILE *f = (FILE *)(uintptr_t)y.id;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) return NULL;
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) return NULL;
    size_t got = fread(buf, 1, (size_t)n, f);
    buf[got] = '\0';
    return buf;
}

/* dosya_yaz_yetkili: yetki kontrol + metni yaz */
int32_t kdl_dosya_yaz_yetkili(KdlYetki y, const char *s) {
    if (kdl_yetki_kontrol_tipi(y, KDL_KAYNAK_DOSYA, KDL_IZIN_YAZ) != 0) {
        return -1;
    }
    FILE *f = (FILE *)(uintptr_t)y.id;
    if (!f || !s) return -1;
    return (int32_t)fwrite(s, 1, strlen(s), f);
}

/* dosya_kapat_yetkili: geri_al + fclose */
void kdl_dosya_kapat_yetkili(KdlYetki *y) {
    if (!y) return;
    if (y->iptal) return;
    FILE *f = (FILE *)(uintptr_t)y->id;
    if (f) fclose(f);
    y->iptal = 1;
}

/* kdl_otp_xor: msg ile key xor edilir, sonuc dosyaya yazilir.
 * Donus: yazilan byte (-1 hata). Anahtar metinden kisa ise -2. */
int32_t kdl_otp_xor_uygula(const char *msg_yol,
                            const char *anahtar_yol,
                            const char *cikti_yol) {
    if (!msg_yol || !anahtar_yol || !cikti_yol) return -1;
    int32_t msg_n = 0, anahtar_n = 0;
    const char *msg_buf = kdl_dosya_oku_byte(msg_yol, &msg_n);
    if (!msg_buf || msg_n < 0) return -1;
    const char *anahtar_buf = kdl_dosya_oku_byte(anahtar_yol, &anahtar_n);
    if (!anahtar_buf || anahtar_n < 0) {
        memset((void *)msg_buf, 0, (size_t)(msg_n < 0 ? 0 : msg_n));
        free((void *)msg_buf);
        return -1;
    }
    /* OTP guvenligi: anahtar >= mesaj olmali */
    if (anahtar_n < msg_n) {
        memset((void *)msg_buf, 0, (size_t)msg_n);
        memset((void *)anahtar_buf, 0, (size_t)anahtar_n);
        free((void *)msg_buf);
        free((void *)anahtar_buf);
        return -2;
    }
    char *cikti = (char *)malloc((size_t)msg_n);
    if (!cikti) {
        memset((void *)msg_buf, 0, (size_t)msg_n);
        memset((void *)anahtar_buf, 0, (size_t)anahtar_n);
        free((void *)msg_buf);
        free((void *)anahtar_buf);
        return -1;
    }
    for (int32_t i = 0; i < msg_n; i++) {
        cikti[i] = (char)(msg_buf[i] ^ anahtar_buf[i]);
    }
    FILE *f = fopen(cikti_yol, "wb");
    if (!f) {
        memset(cikti, 0, (size_t)msg_n);
        memset((void *)msg_buf, 0, (size_t)msg_n);
        memset((void *)anahtar_buf, 0, (size_t)anahtar_n);
        free(cikti);
        free((void *)msg_buf);
        free((void *)anahtar_buf);
        return -1;
    }
    size_t yazildi = fwrite(cikti, 1, (size_t)msg_n, f);
    fclose(f);
    /* Zeroize tum bufferlari — Linear Types prensibi */
    memset(cikti, 0, (size_t)msg_n);
    memset((void *)msg_buf, 0, (size_t)msg_n);
    memset((void *)anahtar_buf, 0, (size_t)anahtar_n);
    free(cikti);
    free((void *)msg_buf);
    free((void *)anahtar_buf);
    return (int32_t)yazildi;
}

/* === D.7 SIMD Spec V1 — Hizali Bellek === */

/* kdl_bellek_hizali_al(boyut, hizalama) -> ptr
 *
 * Hizalama 16/32/64 byte (vektör tipi boyutuna gore). NULL doner basarisizsa.
 * Cross-platform:
 *   - POSIX: posix_memalign
 *   - Windows (MSVC/MinGW): _aligned_malloc
 *
 * Hizalama 2^n olmali; degilse implementation-defined.
 * Free icin kdl_bellek_hizali_serbest cagrılmali (Windows MinGW _aligned_free). */
void *kdl_bellek_hizali_al(int64_t boyut, int64_t hizalama) {
    if (boyut <= 0 || hizalama <= 0) return NULL;
#ifdef _WIN32
    return _aligned_malloc((size_t)boyut, (size_t)hizalama);
#else
    void *ptr = NULL;
    if (posix_memalign(&ptr, (size_t)hizalama, (size_t)boyut) != 0) {
        return NULL;
    }
    return ptr;
#endif
}

void kdl_bellek_hizali_serbest(void *p) {
    if (!p) return;
#ifdef _WIN32
    _aligned_free(p);
#else
    free(p);
#endif
}
