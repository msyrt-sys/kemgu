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

/* === J.2: Metin runtime primitifleri (Kirmizi queue A) ===
 *
 * UTF-8 farkindaligi: metin_uzunluk byte sayisi doner (eski API). Asagidaki
 * fonksiyonlar byte-bazli, ASCII guvenli; ileride UTF-8 boundary uyumu icin
 * v2 (kdl_metin_uzunluk_kp, vs.) eklenir.
 *
 * Heap leak notu: Yeni metin dondurenler malloc kullanir. KEMGU tarafinda
 * bolge sistemi henuz metin'e baglanmadi — su an leak kabul (kisa vade).
 */

/* Substring: [baslangic, son) byte aralig.
 * son <= 0 veya baslangic >= boyut => bos metin. */
const char *kdl_metin_kes(const char *s, int32_t baslangic, int32_t son) {
    if (!s) return NULL;
    int32_t n = (int32_t)strlen(s);
    if (baslangic < 0) baslangic = 0;
    if (son > n) son = n;
    if (son < baslangic) son = baslangic;
    int32_t uz = son - baslangic;
    char *r = (char *)malloc((size_t)uz + 1);
    if (!r) return NULL;
    if (uz > 0) memcpy(r, s + baslangic, (size_t)uz);
    r[uz] = '\0';
    return r;
}

/* ASCII lower: a-z'ye cevirir. UTF-8 multi-byte unchanged. */
const char *kdl_metin_kucuk(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *r = (char *)malloc(n + 1);
    if (!r) return NULL;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c >= 'A' && c <= 'Z') r[i] = (char)(c + ('a' - 'A'));
        else r[i] = (char)c;
    }
    r[n] = '\0';
    return r;
}

/* ASCII upper: A-Z'ye cevirir. UTF-8 multi-byte unchanged. */
const char *kdl_metin_buyuk(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *r = (char *)malloc(n + 1);
    if (!r) return NULL;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c >= 'a' && c <= 'z') r[i] = (char)(c - ('a' - 'A'));
        else r[i] = (char)c;
    }
    r[n] = '\0';
    return r;
}

/* s ara'yi icerir mi (1=evet, 0=hayir). Bos ara => 1 (string convention). */
int32_t kdl_metin_icerir(const char *s, const char *ara) {
    if (!s || !ara) return 0;
    return strstr(s, ara) != NULL ? 1 : 0;
}

/* s ile basliyor mu */
int32_t kdl_metin_baslar(const char *s, const char *ile) {
    if (!s || !ile) return 0;
    size_t ns = strlen(s);
    size_t ni = strlen(ile);
    if (ni > ns) return 0;
    return memcmp(s, ile, ni) == 0 ? 1 : 0;
}

/* s ile bitiyor mu */
int32_t kdl_metin_biter(const char *s, const char *ile) {
    if (!s || !ile) return 0;
    size_t ns = strlen(s);
    size_t ni = strlen(ile);
    if (ni > ns) return 0;
    return memcmp(s + ns - ni, ile, ni) == 0 ? 1 : 0;
}

/* Bos byte (space/tab/CR/LF) kirp. Heap'te yeni metin. */
const char *kdl_metin_kirp(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    size_t i = 0;
    while (i < n && (s[i] == ' ' || s[i] == '\t' ||
                     s[i] == '\r' || s[i] == '\n')) i++;
    size_t j = n;
    while (j > i && (s[j-1] == ' ' || s[j-1] == '\t' ||
                     s[j-1] == '\r' || s[j-1] == '\n')) j--;
    size_t uz = j - i;
    char *r = (char *)malloc(uz + 1);
    if (!r) return NULL;
    if (uz > 0) memcpy(r, s + i, uz);
    r[uz] = '\0';
    return r;
}

/* Tum 'eski' alt-stringlerini 'yeni' ile degistir. Heap'te yeni metin. */
const char *kdl_metin_yer_degistir(const char *s,
                                    const char *eski, const char *yeni) {
    if (!s || !eski) return kdl_metin_kopya(s);
    if (!yeni) yeni = "";
    size_t neski = strlen(eski);
    if (neski == 0) return kdl_metin_kopya(s);
    size_t nyeni = strlen(yeni);
    /* Once kac kez gectigini say */
    size_t adet = 0;
    for (const char *p = s; (p = strstr(p, eski)) != NULL; p += neski) adet++;
    size_t ns = strlen(s);
    size_t sonuc_uz = ns + adet * (nyeni > neski ? nyeni - neski : 0)
                        - adet * (neski > nyeni ? neski - nyeni : 0);
    char *r = (char *)malloc(sonuc_uz + 1);
    if (!r) return NULL;
    char *yaz = r;
    const char *oku = s;
    while (1) {
        const char *bul = strstr(oku, eski);
        if (!bul) {
            size_t kalan = strlen(oku);
            memcpy(yaz, oku, kalan);
            yaz += kalan;
            break;
        }
        size_t once = (size_t)(bul - oku);
        memcpy(yaz, oku, once);
        yaz += once;
        memcpy(yaz, yeni, nyeni);
        yaz += nyeni;
        oku = bul + neski;
    }
    *yaz = '\0';
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
