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

/* Metin esitligi (1=esit, 0=farkli) */
int kdl_metin_esit(const char *a, const char *b) {
    if (!a || !b) return a == b;
    return strcmp(a, b) == 0;
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

int32_t kdl_dizi_al_tam(KdlDizi *d, int32_t i) {
    if (!d || i < 0 || i >= d->boyut) return 0;
    return ((int32_t *)d->veri)[i];
}

int32_t kdl_dizi_boyut(KdlDizi *d) {
    return d ? d->boyut : 0;
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
} KdlGorev;

/* islev pointer alir, sequential calistirir, sonuc kaydeder */
KdlGorev *kdl_gorev_basla_i32(int32_t (*f)(void)) {
    KdlGorev *g = (KdlGorev *)malloc(sizeof(KdlGorev));
    if (!g) return NULL;
    g->result = f ? f() : 0;
    g->done = 1;
    return g;
}

int32_t kdl_gorev_birlestir(KdlGorev *g) {
    if (!g) return 0;
    int32_t r = g->result;
    free(g);
    return r;
}

typedef struct {
    int32_t *veri;
    int32_t boyut;
    int32_t kapasite;
    int32_t bas, son;   /* circular buffer */
} KdlKanal;

KdlKanal *kdl_kanal_olustur(int32_t kapasite) {
    KdlKanal *k = (KdlKanal *)malloc(sizeof(KdlKanal));
    if (!k) return NULL;
    int32_t kap = kapasite > 0 ? kapasite : 16;
    k->veri = (int32_t *)malloc((size_t)kap * sizeof(int32_t));
    k->kapasite = kap;
    k->boyut = 0;
    k->bas = 0;
    k->son = 0;
    return k;
}

void kdl_kanal_gonder(KdlKanal *k, int32_t deger) {
    if (!k || k->boyut >= k->kapasite) return;
    k->veri[k->son] = deger;
    k->son = (k->son + 1) % k->kapasite;
    k->boyut++;
}

int32_t kdl_kanal_al(KdlKanal *k) {
    if (!k || k->boyut == 0) return 0;
    int32_t v = k->veri[k->bas];
    k->bas = (k->bas + 1) % k->kapasite;
    k->boyut--;
    return v;
}

int32_t kdl_kanal_bos_mu(KdlKanal *k) {
    return k ? (k->boyut == 0) : 1;
}

void kdl_kanal_serbest(KdlKanal *k) {
    if (!k) return;
    free(k->veri);
    free(k);
}
