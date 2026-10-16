#include "lsp.h"
#include "json.h"
#include "arena.h"
#include "lexer.h"
#include "parser.h"
#include "sembol.h"
#include "tip_kontrol.h"
#include "hata.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* === Toplanan diagnostic === */

typedef struct LspDiag {
    int satir;       /* 1-tabanli (KEMGU), LSP'ye 0-tabanli olarak gonderilir */
    int sutun;       /* 1-tabanli */
    char kod[16];
    char mesaj[256];
    struct LspDiag *sonraki;
} LspDiag;

typedef struct DiagCtx {
    LspDiag *bas;
    LspDiag *son;
    int sayi;
} DiagCtx;

static void diag_callback(int satir, int sutun,
                          const char *kod, const char *mesaj,
                          const char *ipucu, void *ctx) {
    (void)ipucu;
    DiagCtx *dc = (DiagCtx *)ctx;
    if (!dc) return;
    LspDiag *d = (LspDiag *)calloc(1, sizeof(LspDiag));
    if (!d) return;
    d->satir = satir;
    d->sutun = sutun;
    if (kod) {
        size_t n = strlen(kod);
        if (n >= sizeof(d->kod)) n = sizeof(d->kod) - 1;
        memcpy(d->kod, kod, n);
        d->kod[n] = '\0';
    }
    if (mesaj) {
        size_t n = strlen(mesaj);
        if (n >= sizeof(d->mesaj)) n = sizeof(d->mesaj) - 1;
        memcpy(d->mesaj, mesaj, n);
        d->mesaj[n] = '\0';
    }
    if (dc->son) dc->son->sonraki = d;
    else dc->bas = d;
    dc->son = d;
    dc->sayi++;
}

static void diag_temizle(DiagCtx *dc) {
    LspDiag *d = dc->bas;
    while (d) {
        LspDiag *s = d->sonraki;
        free(d);
        d = s;
    }
    dc->bas = NULL;
    dc->son = NULL;
    dc->sayi = 0;
}

/* === Mesaj cercevesi === */

/* ASCII case-insensitive prefix esitlik (Windows + Linux uyumlu) */
static int ascii_ieq_prefix(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        char ca = a[i], cb = b[i];
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
        if (ca != cb) return 0;
        if (ca == '\0') return 1;
    }
    return 1;
}

/* stdin'den bir LSP mesajinin govdesini oku. Govde malloc edilir, donus.
 * Hata: NULL doner. *out_uz body uzunlugu. */
static char *mesaj_oku(FILE *girdi, int *out_uz) {
    /* Header'lar: Content-Length: N\r\n... \r\n */
    char satir[512];
    int content_length = -1;

    while (fgets(satir, sizeof(satir), girdi)) {
        /* \r\n veya \n ile biten satir */
        size_t n = strlen(satir);
        while (n > 0 && (satir[n - 1] == '\r' || satir[n - 1] == '\n')) {
            satir[--n] = '\0';
        }
        if (n == 0) break;  /* bos satir = header sonu */
        if (ascii_ieq_prefix(satir, "content-length:", 15)) {
            content_length = atoi(satir + 15);
        }
    }
    if (content_length <= 0) {
        if (feof(girdi)) return NULL;
        *out_uz = 0;
        return NULL;
    }

    char *govde = (char *)malloc((size_t)content_length + 1);
    if (!govde) return NULL;
    size_t okunan = fread(govde, 1, (size_t)content_length, girdi);
    govde[okunan] = '\0';
    *out_uz = (int)okunan;
    return govde;
}

static void mesaj_yaz(FILE *cikti, const char *govde, size_t uzunluk) {
    fprintf(cikti, "Content-Length: %zu\r\n\r\n", uzunluk);
    fwrite(govde, 1, uzunluk, cikti);
    fflush(cikti);
}

/* === Belge state (tek dosya icin MVP) ===
 *
 * v2: Hover/definition/completion icin AST cache'i ekledik.
 * Her didChange'da AST yeniden parse edilir.
 */

typedef struct BelgeSembol {
    const char *ad;
    int ad_uz;
    int satir;       /* 1-tabanli */
    int sutun;
    const char *kategori;  /* "islev", "yapi", "ozellik", "uygula", "sabit" */
    const char *tip;       /* ozet imza/tip — basit metin */
    struct BelgeSembol *sonraki;
} BelgeSembol;

typedef struct Belge {
    char *uri;
    char *icerik;
    int icerik_uz;
    /* Cache: yeniden parse edilen AST (didChange'da yenilenir) */
    Arena *arena;
    Dugum *prog;
    BelgeSembol *semboller;  /* arena'da; ust duzey tanimlar */
} Belge;

static void belge_set(Belge *b, const char *uri, int uri_uz,
                      const char *icerik, int icerik_uz) {
    free(b->uri);
    free(b->icerik);
    if (b->arena) arena_serbest(b->arena);
    b->uri = (char *)malloc((size_t)uri_uz + 1);
    if (b->uri) { memcpy(b->uri, uri, (size_t)uri_uz); b->uri[uri_uz] = '\0'; }
    b->icerik = (char *)malloc((size_t)icerik_uz + 1);
    if (b->icerik) {
        memcpy(b->icerik, icerik, (size_t)icerik_uz);
        b->icerik[icerik_uz] = '\0';
    }
    b->icerik_uz = icerik_uz;
    b->arena = NULL;
    b->prog = NULL;
    b->semboller = NULL;
}

/* === D-433: ÇOK-BELGE TABLOSU ===
 * Sunucu tek bir `Belge` tutuyordu: ikinci bir dosya açılınca birincisi
 * EZİLİYORDU. Gerçek editör kullanımında bu, hover/definition/diagnostic'in
 * yanlış dosyaya bakması demekti (VS Code birden çok dosyayı açık tutar ve
 * her isteği KENDİ uri'siyle gönderir).
 * WARN: `workspace` metotlari bunun USTUNE kurulur — tek-belge bir sunucuda
 * "workspace symbol" adı yanıltıcı olurdu; önce model düzeltildi. */
typedef struct BelgeTablo {
    Belge *belgeler;   /* dinamik dizi; kullanılmayan yuvada uri == NULL */
    int sayi;
    int kapasite;
} BelgeTablo;

static void belge_temizle(Belge *b);

/* URI ile ara. Bulunamazsa NULL. */
static Belge *tablo_bul(BelgeTablo *t, const char *uri, int uri_uz) {
    if (!t || !uri) return NULL;
    for (int i = 0; i < t->sayi; i++) {
        Belge *b = &t->belgeler[i];
        if (!b->uri) continue;
        if ((int)strlen(b->uri) == uri_uz && memcmp(b->uri, uri, (size_t)uri_uz) == 0)
            return b;
    }
    return NULL;
}

/* URI ile ara; yoksa BOŞ bir yuva ayır (uri henüz atanmaz — belge_set atar). */
static Belge *tablo_ac(BelgeTablo *t, const char *uri, int uri_uz) {
    Belge *b = tablo_bul(t, uri, uri_uz);
    if (b) return b;
    for (int i = 0; i < t->sayi; i++) {           /* boşalmış yuvayı geri kullan */
        if (!t->belgeler[i].uri) return &t->belgeler[i];
    }
    if (t->sayi >= t->kapasite) {
        int yeni = t->kapasite ? t->kapasite * 2 : 4;
        Belge *p = (Belge *)realloc(t->belgeler, (size_t)yeni * sizeof(Belge));
        if (!p) return NULL;                       /* bellek yok → istek yok sayılır */
        t->belgeler = p;
        t->kapasite = yeni;
    }
    Belge *nb = &t->belgeler[t->sayi++];
    memset(nb, 0, sizeof(Belge));
    return nb;
}

static void tablo_temizle(BelgeTablo *t) {
    if (!t) return;
    for (int i = 0; i < t->sayi; i++) belge_temizle(&t->belgeler[i]);
    free(t->belgeler);
    t->belgeler = NULL; t->sayi = 0; t->kapasite = 0;
}

/* İstekten `params.textDocument.uri` çıkar (yoksa NULL). */
static const char *istek_uri(JsonDeger *istek, int *uz) {
    JsonDeger *params = json_alan(istek, "params");
    if (!params) return NULL;
    JsonDeger *td = json_alan(params, "textDocument");
    if (!td) return NULL;
    return json_metin(json_alan(td, "uri"), uz);
}

static void belge_temizle(Belge *b) {
    free(b->uri);
    free(b->icerik);
    if (b->arena) arena_serbest(b->arena);
    memset(b, 0, sizeof(*b));
}

/* === Incremental sync (LSP v3) ===
 *
 * textDocumentSync = 2 (Incremental). didChange girdileri iki bicimde gelir:
 *   { "range": {...}, "text": "..." }  -> araligi degistir
 *   { "text": "..." }                  -> tum belgeyi degistir (eski full yol)
 * Ikisi de desteklenir; degisiklikler geldikleri sirada uygulanir.
 *
 * LSP konumlarinda `character` UTF-16 kod birimi sayar (spec varsayilani).
 * Asagidaki donusturucu UTF-8 tamponunda bunu bayt ofsetine cevirir. */

static int konum_bayt_ofseti(const char *s, int uz, int satir_0, int karakter_0) {
    int i = 0;
    /* Istenen satirin basina git */
    for (int satir = 0; satir < satir_0 && i < uz; ) {
        if (s[i] == '\n') { satir++; i++; }
        else i++;
    }
    /* Satir icinde karakter_0 kadar UTF-16 kod birimi ilerle */
    int birim = 0;
    while (i < uz && birim < karakter_0 && s[i] != '\n') {
        unsigned char c = (unsigned char)s[i];
        int bayt = 1;
        if (c >= 0xF0) bayt = 4;
        else if (c >= 0xE0) bayt = 3;
        else if (c >= 0xC0) bayt = 2;
        if (i + bayt > uz) bayt = uz - i;
        i += bayt;
        birim += (bayt == 4) ? 2 : 1;  /* BMP disi = surrogate cifti */
    }
    return i;
}

/* === Bayt <-> UTF-16 kod birimi donusumu ===
 *
 * Parser/lexer sutunlari 1-tabanli BAYT sutunudur. LSP `character` alani ise
 * UTF-16 kod birimi sayar. Turkce harfler (ş/ğ/ı/ö/ü/ç) 2 baytlik UTF-8
 * dizileridir; donusum yapilmazsa editor yanlis sutunu isaretler.
 *
 * Tek kaynak: `konum_bayt_ofseti` (UTF-16 -> bayt). Asagidakiler ters yonu ve
 * satir basi hesabini ayni kod uzerinden turetir. */

/* [bas, son) bayt araligindaki UTF-16 kod birimi sayisi. */
static int utf16_say(const char *s, int bas, int son) {
    int n = 0;
    int i = bas;
    while (i < son) {
        unsigned char c = (unsigned char)s[i];
        int bayt = 1;
        if (c >= 0xF0) bayt = 4;
        else if (c >= 0xE0) bayt = 3;
        else if (c >= 0xC0) bayt = 2;
        if (i + bayt > son) bayt = son - i;
        i += bayt;
        n += (bayt == 4) ? 2 : 1;  /* BMP disi = surrogate cifti = 2 birim */
    }
    return n;
}

/* Bir metnin UTF-16 kod birimi uzunlugu. */
static int utf16_uzunluk(const char *s, int uz) {
    if (!s || uz <= 0) return 0;
    return utf16_say(s, 0, uz);
}

/* KEMGU 1-tabanli BAYT sutunu -> LSP 0-tabanli UTF-16 karakteri. */
static int lsp_karakter(const Belge *b, int satir_1, int sutun_1) {
    int bayt = sutun_1 > 0 ? sutun_1 - 1 : 0;
    if (!b || !b->icerik) return bayt;
    int satir_0 = satir_1 > 0 ? satir_1 - 1 : 0;
    int satir_bas = konum_bayt_ofseti(b->icerik, b->icerik_uz, satir_0, 0);
    int son = satir_bas + bayt;
    if (son > b->icerik_uz) son = b->icerik_uz;
    if (son < satir_bas) son = satir_bas;
    return utf16_say(b->icerik, satir_bas, son);
}

/* LSP 0-tabanli UTF-16 karakteri -> KEMGU 1-tabanli BAYT sutunu. */
static int kemgu_sutun(const Belge *b, int satir_0, int karakter_0) {
    if (!b || !b->icerik) return karakter_0 + 1;
    int satir_bas = konum_bayt_ofseti(b->icerik, b->icerik_uz, satir_0, 0);
    int ofset = konum_bayt_ofseti(b->icerik, b->icerik_uz, satir_0, karakter_0);
    return ofset - satir_bas + 1;
}

/* Belgenin [bas, son) bayt araligini `metin` ile degistir. */
static void belge_aralik_uygula(Belge *b, const JsonDeger *range,
                                const char *metin, int uz) {
    if (!b->icerik || !range) return;
    const JsonDeger *st = json_alan(range, "start");
    const JsonDeger *en = json_alan(range, "end");
    if (!st || !en) return;
    int bas = konum_bayt_ofseti(b->icerik, b->icerik_uz,
        (int)json_tamsayi(json_alan(st, "line")),
        (int)json_tamsayi(json_alan(st, "character")));
    int son = konum_bayt_ofseti(b->icerik, b->icerik_uz,
        (int)json_tamsayi(json_alan(en, "line")),
        (int)json_tamsayi(json_alan(en, "character")));
    if (son < bas) son = bas;

    int yeni_uz = b->icerik_uz - (son - bas) + uz;
    char *yeni = (char *)malloc((size_t)yeni_uz + 1);
    if (!yeni) return;
    memcpy(yeni, b->icerik, (size_t)bas);
    if (uz > 0) memcpy(yeni + bas, metin, (size_t)uz);
    memcpy(yeni + bas + uz, b->icerik + son, (size_t)(b->icerik_uz - son));
    yeni[yeni_uz] = '\0';
    free(b->icerik);
    b->icerik = yeni;
    b->icerik_uz = yeni_uz;
}

/* Ust duzey sembolleri AST'den cikar ve belgeye kayit et. */
static void sembol_ekle_belge(Belge *b, const char *ad, int ad_uz,
                              int satir, int sutun,
                              const char *kat, const char *tip) {
    BelgeSembol *s = (BelgeSembol *)arena_ayir_sifir(b->arena,
        sizeof(BelgeSembol));
    if (!s) return;
    s->ad = ad;
    s->ad_uz = ad_uz;
    s->satir = satir;
    s->sutun = sutun;
    s->kategori = kat;
    s->tip = tip;
    s->sonraki = b->semboller;
    b->semboller = s;
}

static void belge_sembolleri_topla(Belge *b) {
    if (!b->prog) return;
    for (int i = 0; i < b->prog->veri.program.sayi; i++) {
        const Dugum *uye = b->prog->veri.program.uyeler[i];
        const Dugum *gercek = uye;
        if (gercek->tip == DUGUM_DISA && gercek->veri.disa.tanim) {
            gercek = gercek->veri.disa.tanim;
        }
        switch (gercek->tip) {
            case DUGUM_ISLEV:
                sembol_ekle_belge(b,
                    gercek->veri.islev.ad,
                    gercek->veri.islev.ad_uzunluk,
                    gercek->satir, gercek->sutun,
                    "islev", "islev");
                break;
            case DUGUM_YAPI:
                sembol_ekle_belge(b,
                    gercek->veri.yapi.ad,
                    gercek->veri.yapi.ad_uzunluk,
                    gercek->satir, gercek->sutun,
                    "yapi", "yapi");
                break;
            case DUGUM_OZELLIK:
                sembol_ekle_belge(b,
                    gercek->veri.ozellik.ad,
                    gercek->veri.ozellik.ad_uzunluk,
                    gercek->satir, gercek->sutun,
                    "ozellik", "ozellik (trait)");
                break;
            case DUGUM_SABIT:
                sembol_ekle_belge(b,
                    gercek->veri.sabit.ad,
                    gercek->veri.sabit.ad_uzunluk,
                    gercek->satir, gercek->sutun,
                    "sabit", "sabit");
                break;
            case DUGUM_CESIT:
                sembol_ekle_belge(b,
                    gercek->veri.cesit.ad,
                    gercek->veri.cesit.ad_uzunluk,
                    gercek->satir, gercek->sutun,
                    "cesit", "\xc3\xa7" "e\xc5\x9fit (sum type)");
                break;
            default: break;
        }
    }
}

/* === Tanimlayici gezgini ===
 *
 * Agactaki her DUGUM_TANIMLAYICI dugumu icin geri-cagirim uretir.
 * Hem "konumdaki tanimlayiciyi bul" (hover/definition) hem de
 * "adin tum kullanimlarini topla" (references) bunun uzerine kurulu. */

typedef void (*IdZiyaret)(const Dugum *d, void *ctx);

static void id_gez(const Dugum *d, IdZiyaret cb, void *ctx);

static void id_gez_liste(Dugum **liste, int sayi, IdZiyaret cb, void *ctx) {
    if (!liste) return;
    for (int i = 0; i < sayi; i++) id_gez(liste[i], cb, ctx);
}

static void id_gez(const Dugum *d, IdZiyaret cb, void *ctx) {
    if (!d) return;
    if (d->tip == DUGUM_TANIMLAYICI) { cb(d, ctx); return; }
    switch (d->tip) {
        case DUGUM_PROGRAM:
            id_gez_liste(d->veri.program.uyeler, d->veri.program.sayi, cb, ctx);
            break;
        case DUGUM_ISLEV:
            id_gez_liste(d->veri.islev.parametreler, d->veri.islev.param_sayi,
                         cb, ctx);
            id_gez(d->veri.islev.govde, cb, ctx);
            break;
        case DUGUM_DISA:
            id_gez(d->veri.disa.tanim, cb, ctx);
            break;
        case DUGUM_MODUL:
            id_gez_liste(d->veri.modul.uyeler, d->veri.modul.sayi, cb, ctx);
            break;
        case DUGUM_UYGULA:
            id_gez_liste(d->veri.uygula.islevler, d->veri.uygula.islev_sayi,
                         cb, ctx);
            break;
        case DUGUM_BLOK:
            id_gez_liste(d->veri.blok.deyimler, d->veri.blok.sayi, cb, ctx);
            break;
        case DUGUM_DEGISKEN:
            id_gez(d->veri.degisken.deger, cb, ctx);
            break;
        case DUGUM_ATAMA:
            id_gez(d->veri.atama.hedef, cb, ctx);
            id_gez(d->veri.atama.deger, cb, ctx);
            break;
        case DUGUM_VER:
            id_gez(d->veri.ver.deger, cb, ctx);
            break;
        case DUGUM_EGER:
            id_gez(d->veri.eger.kosul, cb, ctx);
            id_gez(d->veri.eger.gozdoldur, cb, ctx);
            id_gez(d->veri.eger.yan, cb, ctx);
            break;
        case DUGUM_IKEN:
            id_gez(d->veri.iken.kosul, cb, ctx);
            id_gez(d->veri.iken.govde, cb, ctx);
            break;
        case DUGUM_ICIN:
            id_gez(d->veri.icin.koleksiyon, cb, ctx);
            id_gez(d->veri.icin.govde, cb, ctx);
            break;
        case DUGUM_ESLES:
            id_gez(d->veri.esles.deger, cb, ctx);
            id_gez_liste(d->veri.esles.kollar, d->veri.esles.kol_sayi, cb, ctx);
            break;
        case DUGUM_ESLES_KOLU:
            id_gez(d->veri.esles_kolu.govde, cb, ctx);
            break;
        case DUGUM_GUVENSIZ:
            id_gez(d->veri.guvensiz.blok, cb, ctx);
            break;
        case DUGUM_IFADE_DEYIMI:
            id_gez(d->veri.ifade_deyimi.ifade, cb, ctx);
            break;
        case DUGUM_IKILI:
            id_gez(d->veri.ikili.sol, cb, ctx);
            id_gez(d->veri.ikili.sag, cb, ctx);
            break;
        case DUGUM_TEKLI:
            id_gez(d->veri.tekli.operand, cb, ctx);
            break;
        case DUGUM_CAGRI:
            id_gez(d->veri.cagri.hedef, cb, ctx);
            id_gez_liste(d->veri.cagri.argumanlar, d->veri.cagri.sayi, cb, ctx);
            break;
        case DUGUM_ERISIM:
            id_gez(d->veri.erisim.nesne, cb, ctx);
            break;
        case DUGUM_INDEKS:
            id_gez(d->veri.indeks.nesne, cb, ctx);
            id_gez(d->veri.indeks.indeks, cb, ctx);
            break;
        case DUGUM_LAMBDA:
            id_gez_liste(d->veri.lambda.parametreler, d->veri.lambda.param_sayi,
                         cb, ctx);
            id_gez(d->veri.lambda.govde, cb, ctx);
            break;
        case DUGUM_YAPI_OLUSTUR:
            id_gez_liste(d->veri.yapi_olustur.alanlar,
                         d->veri.yapi_olustur.alan_sayi, cb, ctx);
            break;
        case DUGUM_ALAN_ATAMA:
            id_gez(d->veri.alan_atama.deger, cb, ctx);
            break;
        case DUGUM_DIZI_OLUSTUR:
            id_gez_liste(d->veri.dizi_olustur.elemanlar,
                         d->veri.dizi_olustur.sayi, cb, ctx);
            break;
        case DUGUM_KULLAN_IFADE:
            id_gez(d->veri.kullan_ifade.operand, cb, ctx);
            break;
        case DUGUM_IMHA_IFADE:
            id_gez(d->veri.imha_ifade.operand, cb, ctx);
            break;
        case DUGUM_SABIT:
            id_gez(d->veri.sabit.deger, cb, ctx);
            break;
        default: break;
    }
}

/* Belirli (line, col) konumundaki tanimlayici dugumu bul.
 * Position 1-tabanli (LSP'den donusturulmus). */

typedef struct {
    int line_1;
    int col_1;
    const Dugum *bulunan;
} KonumCtx;

static void konum_ziyaret(const Dugum *d, void *ctx) {
    KonumCtx *kc = (KonumCtx *)ctx;
    if (kc->bulunan) return;
    if (d->satir != kc->line_1) return;
    int s = d->sutun;
    int e = s + d->veri.tanimlayici.uzunluk;
    if (kc->col_1 >= s && kc->col_1 < e) kc->bulunan = d;
}

static const Dugum *bul_tanimlayici_konum(const Dugum *d,
                                            int line_1, int col_1) {
    KonumCtx kc;
    kc.line_1 = line_1;
    kc.col_1 = col_1;
    kc.bulunan = NULL;
    id_gez(d, konum_ziyaret, &kc);
    return kc.bulunan;
}

static BelgeSembol *belge_sembol_bul(Belge *b, const char *ad, int ad_uz) {
    for (BelgeSembol *s = b->semboller; s; s = s->sonraki) {
        if (s->ad_uz == ad_uz && memcmp(s->ad, ad, (size_t)ad_uz) == 0) {
            return s;
        }
    }
    return NULL;
}

/* === Diagnostic yayini === */

static void publish_diagnostics(FILE *cikti, const char *uri, DiagCtx *dc,
                                const Belge *belge) {
    JsonYazici y;
    json_yazici_baslat(&y);
    json_yaz(&y, "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\","
                 "\"params\":{\"uri\":");
    json_yaz_metin_lit(&y, uri);
    json_yaz(&y, ",\"diagnostics\":[");
    int once = 1;
    for (LspDiag *d = dc->bas; d; d = d->sonraki) {
        if (!once) json_yaz(&y, ",");
        once = 0;
        /* LSP konum 0-tabanli, KEMGU 1-tabanli. Sutun ayrica BAYT'tan
         * UTF-16 kod birimine cevrilir. */
        int s = d->satir > 0 ? d->satir - 1 : 0;
        int k = lsp_karakter(belge, d->satir, d->sutun);
        json_yaz(&y, "{\"range\":{\"start\":{\"line\":");
        json_yaz_int(&y, s);
        json_yaz(&y, ",\"character\":");
        json_yaz_int(&y, k);
        json_yaz(&y, "},\"end\":{\"line\":");
        json_yaz_int(&y, s);
        json_yaz(&y, ",\"character\":");
        json_yaz_int(&y, k + 1);
        json_yaz(&y, "}},\"severity\":1,\"code\":");
        json_yaz_metin_lit(&y, d->kod);
        json_yaz(&y, ",\"source\":\"kemgu\",\"message\":");
        json_yaz_metin_lit(&y, d->mesaj);
        json_yaz(&y, "}");
    }
    json_yaz(&y, "]}}");
    mesaj_yaz(cikti, y.tampon, y.kullanilan);
    json_yazici_serbest(&y);
}

/* === Belgeyi analiz et ===
 *
 * Eski arena varsa serbest birakir, yenisini olusturur. AST + sembol
 * tablosu belge'de cache'lenir. */

static void analiz_et(Belge *belge, DiagCtx *dc) {
    if (!belge || !belge->icerik) return;
    if (belge->arena) {
        arena_serbest(belge->arena);
        belge->arena = NULL;
        belge->prog = NULL;
        belge->semboller = NULL;
    }
    belge->arena = arena_olustur(0);
    if (!belge->arena) return;

    diag_temizle(dc);
    hata_callback_ayarla(diag_callback, dc);

    Lexer l;
    lexer_baslat(&l, belge->icerik, "lsp");
    Parser p;
    parser_baslat(&p, &l, belge->arena, "lsp", belge->icerik);
    belge->prog = parser_calistir(&p);

    if (belge->prog && p.hata_sayisi == 0) {
        Scope *g = scope_olustur(belge->arena, SCOPE_GLOBAL, NULL);
        TipKontrol tk;
        tip_kontrol_baslat(&tk, belge->arena, g, "lsp", belge->icerik);
        tip_kontrol_program(&tk, belge->prog);
    }

    hata_callback_ayarla(NULL, NULL);
    belge_sembolleri_topla(belge);
}

/* === Request/Notification yonetimi === */

static int kapanis_isteniyor = 0;

static void initialize_yanitla(FILE *cikti, JsonDeger *istek) {
    JsonDeger *id = json_alan(istek, "id");
    JsonYazici y;
    json_yazici_baslat(&y);
    json_yaz(&y, "{\"jsonrpc\":\"2.0\",\"id\":");
    if (id && id->tip == JSON_TAMSAYI) {
        json_yaz_int(&y, id->veri.tamsayi);
    } else if (id && id->tip == JSON_METIN) {
        json_yaz_metin_lit_n(&y, id->veri.str.metin, (size_t)id->veri.str.uzunluk);
    } else {
        json_yaz(&y, "null");
    }
    /* Capabilities: textDocumentSync = 2 (incremental; range'siz degisiklik
     * geriye uyumlu olarak tam-metin yerine gecer) */
    json_yaz(&y, ",\"result\":{\"capabilities\":{"
                 "\"textDocumentSync\":2,"
                 "\"hoverProvider\":true,"
                 "\"definitionProvider\":true,"
                 "\"completionProvider\":{\"triggerCharacters\":[\".\"]},"
                 "\"documentSymbolProvider\":true,"
                 "\"referencesProvider\":true,"
                 /* D-432: semanticTokens (full). Legend SIRASI kritik —
                  * `data` icindeki tokenType INDEKSTIR, ad degil. */
                 "\"workspaceSymbolProvider\":true,"   /* D-434 */
                 "\"semanticTokensProvider\":{\"legend\":{"
                 "\"tokenTypes\":[\"keyword\",\"string\",\"number\","
                 "\"operator\",\"variable\"],"
                 "\"tokenModifiers\":[]},\"full\":true},"
                 "\"diagnosticProvider\":{\"interFileDependencies\":false,"
                 "\"workspaceDiagnostics\":false}"
                 "},\"serverInfo\":{\"name\":\"kemgu-lsp\",\"version\":\"0.2\"}}}");
    mesaj_yaz(cikti, y.tampon, y.kullanilan);
    json_yazici_serbest(&y);
}

/* === Hover === */

static void hover_yanitla(FILE *cikti, JsonDeger *istek, Belge *belge) {
    JsonDeger *id = json_alan(istek, "id");
    JsonDeger *params = json_alan(istek, "params");
    JsonDeger *pos = params ? json_alan(params, "position") : NULL;
    int line_0 = 0, char_0 = 0;
    if (pos) {
        line_0 = (int)json_tamsayi(json_alan(pos, "line"));
        char_0 = (int)json_tamsayi(json_alan(pos, "character"));
    }

    JsonYazici y;
    json_yazici_baslat(&y);
    json_yaz(&y, "{\"jsonrpc\":\"2.0\",\"id\":");
    if (id && id->tip == JSON_TAMSAYI) json_yaz_int(&y, id->veri.tamsayi);
    else json_yaz(&y, "null");

    const char *icerik_str = NULL;
    BelgeSembol *sem = NULL;
    if (belge && belge->prog) {
        const Dugum *d = bul_tanimlayici_konum(belge->prog, line_0 + 1,
            kemgu_sutun(belge, line_0, char_0));
        if (d) {
            sem = belge_sembol_bul(belge,
                d->veri.tanimlayici.metin,
                d->veri.tanimlayici.uzunluk);
        }
        if (sem) icerik_str = sem->tip;
    }

    if (icerik_str && sem) {
        json_yaz(&y, ",\"result\":{\"contents\":{\"kind\":\"markdown\",\"value\":\"**");
        json_yaz_n(&y, sem->ad, (size_t)sem->ad_uz);
        json_yaz(&y, "**: ");
        json_yaz_n(&y, icerik_str, strlen(icerik_str));
        json_yaz(&y, "\"}}");
    } else {
        json_yaz(&y, ",\"result\":null");
    }
    json_yaz(&y, "}");
    mesaj_yaz(cikti, y.tampon, y.kullanilan);
    json_yazici_serbest(&y);
}

/* === Definition === */

static void definition_yanitla(FILE *cikti, JsonDeger *istek, Belge *belge) {
    JsonDeger *id = json_alan(istek, "id");
    JsonDeger *params = json_alan(istek, "params");
    JsonDeger *pos = params ? json_alan(params, "position") : NULL;
    int line_0 = 0, char_0 = 0;
    if (pos) {
        line_0 = (int)json_tamsayi(json_alan(pos, "line"));
        char_0 = (int)json_tamsayi(json_alan(pos, "character"));
    }

    JsonYazici y;
    json_yazici_baslat(&y);
    json_yaz(&y, "{\"jsonrpc\":\"2.0\",\"id\":");
    if (id && id->tip == JSON_TAMSAYI) json_yaz_int(&y, id->veri.tamsayi);
    else json_yaz(&y, "null");

    int found = 0;
    int def_line = 0, def_col = 0, def_uz = 0;
    if (belge && belge->prog) {
        const Dugum *d = bul_tanimlayici_konum(belge->prog, line_0 + 1,
            kemgu_sutun(belge, line_0, char_0));
        if (d) {
            BelgeSembol *sem = belge_sembol_bul(belge,
                d->veri.tanimlayici.metin,
                d->veri.tanimlayici.uzunluk);
            if (sem) {
                def_line = sem->satir > 0 ? sem->satir - 1 : 0;
                /* BAYT sutunu -> UTF-16 karakteri; uzunluk da UTF-16 birimi */
                def_col = lsp_karakter(belge, sem->satir, sem->sutun);
                def_uz = utf16_uzunluk(sem->ad, sem->ad_uz);
                if (def_uz <= 0) def_uz = 1;
                found = 1;
            }
        }
    }

    if (found && belge && belge->uri) {
        json_yaz(&y, ",\"result\":{\"uri\":");
        json_yaz_metin_lit(&y, belge->uri);
        json_yaz(&y, ",\"range\":{\"start\":{\"line\":");
        json_yaz_int(&y, def_line);
        json_yaz(&y, ",\"character\":");
        json_yaz_int(&y, def_col);
        json_yaz(&y, "},\"end\":{\"line\":");
        json_yaz_int(&y, def_line);
        json_yaz(&y, ",\"character\":");
        json_yaz_int(&y, def_col + def_uz);
        json_yaz(&y, "}}}");
    } else {
        json_yaz(&y, ",\"result\":null");
    }
    json_yaz(&y, "}");
    mesaj_yaz(cikti, y.tampon, y.kullanilan);
    json_yazici_serbest(&y);
}

/* === References (LSP v3) ===
 *
 * Imlecteki tanimlayicinin adini alir, ayni dosyadaki TUM kullanimlarini
 * (ad esitligi ile) Location[] olarak dondurur. `includeDeclaration` true
 * (varsayilan) ise ust duzey tanim konumu da listeye girer.
 *
 * V1 siniri: ad-tabanli eslesme — golgeleme (shadowing) ayirt edilmez,
 * dosya disi kullanimlar taranmaz. */

/* documentSymbol bolumunde tanimli — Range yazici. */
static void ds_range_yaz(JsonYazici *y, const Belge *b, const char *alan_ad,
                         int satir_1, int sutun_1, const char *ad, int ad_uz);

typedef struct {
    JsonYazici *y;
    const Belge *belge;
    const char *uri;
    const char *ad;
    int ad_uz;
    int *once;
} RefCtx;

static void ref_konum_yaz(JsonYazici *y, const Belge *b, const char *uri,
                          int satir_1, int sutun_1,
                          const char *ad, int ad_uz, int *once) {
    if (!*once) json_yaz(y, ",");
    *once = 0;
    json_yaz(y, "{\"uri\":");
    json_yaz_metin_lit(y, uri);
    json_yaz(y, ",");
    ds_range_yaz(y, b, "range", satir_1, sutun_1, ad, ad_uz);
    json_yaz(y, "}");
}

static void ref_ziyaret(const Dugum *d, void *ctx) {
    RefCtx *rc = (RefCtx *)ctx;
    if (d->veri.tanimlayici.uzunluk != rc->ad_uz) return;
    if (memcmp(d->veri.tanimlayici.metin, rc->ad, (size_t)rc->ad_uz) != 0) return;
    ref_konum_yaz(rc->y, rc->belge, rc->uri, d->satir, d->sutun,
                  rc->ad, rc->ad_uz, rc->once);
}

static void references_yanitla(FILE *cikti, JsonDeger *istek, Belge *belge) {
    JsonDeger *id = json_alan(istek, "id");
    JsonDeger *params = json_alan(istek, "params");
    JsonDeger *pos = params ? json_alan(params, "position") : NULL;
    JsonDeger *baglam = params ? json_alan(params, "context") : NULL;
    int tanimi_dahil_et = 1;
    if (baglam) {
        JsonDeger *inc = json_alan(baglam, "includeDeclaration");
        if (inc && inc->tip == JSON_BOOL) tanimi_dahil_et = inc->veri.bool_deger;
    }
    int line_0 = 0, char_0 = 0;
    if (pos) {
        line_0 = (int)json_tamsayi(json_alan(pos, "line"));
        char_0 = (int)json_tamsayi(json_alan(pos, "character"));
    }

    JsonYazici y;
    json_yazici_baslat(&y);
    json_yaz(&y, "{\"jsonrpc\":\"2.0\",\"id\":");
    if (id && id->tip == JSON_TAMSAYI) json_yaz_int(&y, id->veri.tamsayi);
    else json_yaz(&y, "null");

    const Dugum *hedef = NULL;
    if (belge && belge->prog) {
        hedef = bul_tanimlayici_konum(belge->prog, line_0 + 1,
            kemgu_sutun(belge, line_0, char_0));
    }

    if (!hedef || !belge->uri) {
        json_yaz(&y, ",\"result\":null}");
        mesaj_yaz(cikti, y.tampon, y.kullanilan);
        json_yazici_serbest(&y);
        return;
    }

    const char *ad = hedef->veri.tanimlayici.metin;
    int ad_uz = hedef->veri.tanimlayici.uzunluk;

    json_yaz(&y, ",\"result\":[");
    int once = 1;

    if (tanimi_dahil_et) {
        BelgeSembol *sem = belge_sembol_bul(belge, ad, ad_uz);
        if (sem) ref_konum_yaz(&y, belge, belge->uri, sem->satir, sem->sutun,
                               sem->ad, sem->ad_uz, &once);
    }

    RefCtx rc;
    rc.y = &y;
    rc.belge = belge;
    rc.uri = belge->uri;
    rc.ad = ad;
    rc.ad_uz = ad_uz;
    rc.once = &once;
    id_gez(belge->prog, ref_ziyaret, &rc);

    json_yaz(&y, "]}");
    mesaj_yaz(cikti, y.tampon, y.kullanilan);
    json_yazici_serbest(&y);
}

/* === documentSymbol (LSP v3) ===
 *
 * Ust duzey tanimlari hiyerarsik DocumentSymbol[] olarak dondurur:
 *   islev(12) / yapi(23)+alanlar(8) / ozellik(11)+uyeler(6) /
 *   sabit(14) / cesit(10)+varyantlar(22) / modul(2)+uyeler(rekursif)
 *
 * NOT: AST dugumleri bitis konumu tasimadigi icin `range` = `selectionRange`
 * (ad araligi). Istemciler bunu kabul eder (range >= selectionRange sarti saglanir).
 */

/* "ad":{"start":{...},"end":{...}} seklinde bir Range alani yazar. */
static void ds_range_yaz(JsonYazici *y, const Belge *b, const char *alan_ad,
                         int satir_1, int sutun_1,
                         const char *ad, int ad_uz) {
    int s = satir_1 > 0 ? satir_1 - 1 : 0;
    /* Konum BAYT sutunundan UTF-16 karakterine cevrilir; uzunluk ise adin
     * kendi UTF-16 birim sayisidir (ad, konumdan bagimsiz olabilir). */
    int k = lsp_karakter(b, satir_1, sutun_1);
    int uzunluk = utf16_uzunluk(ad, ad_uz);
    if (uzunluk <= 0) uzunluk = 1;
    json_yaz(y, "\"");
    json_yaz(y, alan_ad);
    json_yaz(y, "\":{\"start\":{\"line\":");
    json_yaz_int(y, s);
    json_yaz(y, ",\"character\":");
    json_yaz_int(y, k);
    json_yaz(y, "},\"end\":{\"line\":");
    json_yaz_int(y, s);
    json_yaz(y, ",\"character\":");
    json_yaz_int(y, k + uzunluk);
    json_yaz(y, "}}");
}

/* Bir DocumentSymbol nesnesi acar: {"name":..,"detail":..,"kind":..,range,selectionRange
 * Cocuk yazilacaksa cagiran ",\"children\":[...]" ekler, sonra "}" kapatir. */
static void ds_bas_yaz(JsonYazici *y, const Belge *b,
                       const char *ad, int ad_uz, int kind,
                       const char *detay, int satir_1, int sutun_1) {
    json_yaz(y, "{\"name\":");
    if (ad && ad_uz > 0) json_yaz_metin_lit_n(y, ad, (size_t)ad_uz);
    else json_yaz(y, "\"?\"");
    json_yaz(y, ",\"detail\":");
    json_yaz_metin_lit(y, detay ? detay : "");
    json_yaz(y, ",\"kind\":");
    json_yaz_int(y, kind);
    json_yaz(y, ",");
    ds_range_yaz(y, b, "range", satir_1, sutun_1, ad, ad_uz);
    json_yaz(y, ",");
    ds_range_yaz(y, b, "selectionRange", satir_1, sutun_1, ad, ad_uz);
}

/* Bir ust duzey tanimi yazar. Yazdiysa 1 doner. */
static int ds_tanim_yaz(JsonYazici *y, const Belge *b, const Dugum *d) {
    if (!d) return 0;
    if (d->tip == DUGUM_DISA && d->veri.disa.tanim) d = d->veri.disa.tanim;

    switch (d->tip) {
        case DUGUM_ISLEV:
            ds_bas_yaz(y, b, d->veri.islev.ad, d->veri.islev.ad_uzunluk,
                       12, "i\xc5\x9flev", d->satir, d->sutun);
            json_yaz(y, "}");
            return 1;
        case DUGUM_SABIT:
            ds_bas_yaz(y, b, d->veri.sabit.ad, d->veri.sabit.ad_uzunluk,
                       14, "sabit", d->satir, d->sutun);
            json_yaz(y, "}");
            return 1;
        case DUGUM_YAPI: {
            ds_bas_yaz(y, b, d->veri.yapi.ad, d->veri.yapi.ad_uzunluk,
                       23, "yap\xc4\xb1", d->satir, d->sutun);
            json_yaz(y, ",\"children\":[");
            int once = 1;
            for (int i = 0; i < d->veri.yapi.alan_sayi; i++) {
                const Dugum *a = d->veri.yapi.alanlar[i];
                if (!a || a->tip != DUGUM_ALAN) continue;
                if (!once) json_yaz(y, ",");
                once = 0;
                ds_bas_yaz(y, b, a->veri.alan.ad, a->veri.alan.ad_uzunluk,
                           8, "alan", a->satir, a->sutun);
                json_yaz(y, "}");
            }
            json_yaz(y, "]}");
            return 1;
        }
        case DUGUM_CESIT: {
            ds_bas_yaz(y, b, d->veri.cesit.ad, d->veri.cesit.ad_uzunluk,
                       10, "\xc3\xa7" "e\xc5\x9fit", d->satir, d->sutun);
            json_yaz(y, ",\"children\":[");
            for (int i = 0; i < d->veri.cesit.varyant_sayi; i++) {
                if (i) json_yaz(y, ",");
                ds_bas_yaz(y, b, d->veri.cesit.varyantlar[i],
                           d->veri.cesit.varyant_uzunluklar[i],
                           22, "varyant", d->satir, d->sutun);
                json_yaz(y, "}");
            }
            json_yaz(y, "]}");
            return 1;
        }
        case DUGUM_OZELLIK: {
            ds_bas_yaz(y, b, d->veri.ozellik.ad, d->veri.ozellik.ad_uzunluk,
                       11, "\xc3\xb6zellik", d->satir, d->sutun);
            json_yaz(y, ",\"children\":[");
            int once = 1;
            for (int i = 0; i < d->veri.ozellik.uye_sayi; i++) {
                const Dugum *u = d->veri.ozellik.uyeler[i];
                if (!u || u->tip != DUGUM_ISLEV) continue;
                if (!once) json_yaz(y, ",");
                once = 0;
                ds_bas_yaz(y, b, u->veri.islev.ad, u->veri.islev.ad_uzunluk,
                           6, "metot", u->satir, u->sutun);
                json_yaz(y, "}");
            }
            json_yaz(y, "]}");
            return 1;
        }
        case DUGUM_MODUL: {
            ds_bas_yaz(y, b, d->veri.modul.ad, d->veri.modul.ad_uzunluk,
                       2, "mod\xc3\xbcl", d->satir, d->sutun);
            json_yaz(y, ",\"children\":[");
            int once = 1;
            for (int i = 0; i < d->veri.modul.sayi; i++) {
                size_t once_uz = y->kullanilan;
                if (!once) json_yaz(y, ",");
                if (!ds_tanim_yaz(y, b, d->veri.modul.uyeler[i])) {
                    y->kullanilan = once_uz;  /* yazilmadi: virgulu geri al */
                    continue;
                }
                once = 0;
            }
            json_yaz(y, "]}");
            return 1;
        }
        default: return 0;
    }
}

static void documentsymbol_yanitla(FILE *cikti, JsonDeger *istek, Belge *belge) {
    JsonDeger *id = json_alan(istek, "id");
    JsonYazici y;
    json_yazici_baslat(&y);
    json_yaz(&y, "{\"jsonrpc\":\"2.0\",\"id\":");
    if (id && id->tip == JSON_TAMSAYI) json_yaz_int(&y, id->veri.tamsayi);
    else json_yaz(&y, "null");
    json_yaz(&y, ",\"result\":[");
    int once = 1;
    if (belge && belge->prog && belge->prog->tip == DUGUM_PROGRAM) {
        for (int i = 0; i < belge->prog->veri.program.sayi; i++) {
            size_t once_uz = y.kullanilan;
            if (!once) json_yaz(&y, ",");
            if (!ds_tanim_yaz(&y, belge, belge->prog->veri.program.uyeler[i])) {
                y.kullanilan = once_uz;
                continue;
            }
            once = 0;
        }
    }
    json_yaz(&y, "]}");
    mesaj_yaz(cikti, y.tampon, y.kullanilan);
    json_yazici_serbest(&y);
}

/* === semanticTokens (LSP v3) — D-432 ===
 * Kaynak LEXER'dir: her token zaten satir/sutun/uzunluk tasiyor, ayrica
 * anahtar kelime / literal / operator ayrimi lexer'da YAPILMIS durumda.
 * AST'den turetmek gereksiz olurdu (ve hatali kaynakta AST YOK — lexer ise
 * yine de token uretir, yani sozdizimi renklendirme HATALI dosyada da calisir).
 *
 * ⚠ LSP `data` KODLAMASI: 5'li gruplar ve DELTA'lidir —
 *   [deltaSatir, deltaBaslangic, uzunluk, tokenTipi, tokenDegistirici]
 * deltaBaslangic AYNI SATIRDA onceki token'a gore, satir degisince MUTLAK.
 * Sutun ve uzunluk UTF-16 KOD BIRIMI cinsindendir (bayt DEGIL) — Turkce
 * kaynakta bayt kullanmak konumlari kaydirir.
 *
 * Legend indeksleri (capabilities'teki SIRAYLA ayni olmali):
 *   0 keyword · 1 string · 2 number · 3 operator · 4 variable
 * Eslesmeyen token (noktali virgul, parantez vb.) ATLANIR — LSP istemcisi
 * atlanan araligi renklendirmez, bu gecerlidir. */
static int semtok_tipi(TokenTipi t) {
    switch (t) {
    case TOK_METIN: case TOK_HAM_METIN: case TOK_KARAKTER: return 1;
    case TOK_TAMSAYI: case TOK_ONDALIK: return 2;
    case TOK_TANIMLAYICI: return 4;
    /* Operatorler — atama/aritmetik/karsilastirma/bit. */
    case TOK_ARTI: case TOK_EKSI: case TOK_YILDIZ: case TOK_BOLU:
    case TOK_MOD: case TOK_ESIT: case TOK_ESIT_ESIT: case TOK_ESIT_DEGIL:
    case TOK_KUCUK: case TOK_BUYUK: case TOK_KUCUK_ESIT: case TOK_BUYUK_ESIT:
    case TOK_ARTI_ESIT: case TOK_EKSI_ESIT: case TOK_YILDIZ_ESIT:
    case TOK_BOLU_ESIT: case TOK_MOD_ESIT:
    case TOK_VE_BIT: case TOK_VEYA_BIT: case TOK_OZVEYA_BIT: case TOK_DEGIL_BIT:
    case TOK_SOLA_KAYDIR: case TOK_SAGA_KAYDIR:
    case TOK_OK: case TOK_KALIN_OK:
        return 3;
    default:
        /* anahtar_kelime.c tablosundaki her sey keyword; kalan noktalama
         * (`;` `,` `(` `{` ...) ve TOK_DOSYA_SONU/TOK_HATALI atlanir. */
        return -2;
    }
}

static void semantictokens_yanitla(FILE *cikti, JsonDeger *istek, Belge *belge) {
    JsonDeger *id = json_alan(istek, "id");
    JsonYazici y;
    json_yazici_baslat(&y);
    json_yaz(&y, "{\"jsonrpc\":\"2.0\",\"id\":");
    if (id && id->tip == JSON_TAMSAYI) json_yaz_int(&y, id->veri.tamsayi);
    else json_yaz(&y, "null");
    json_yaz(&y, ",\"result\":{\"data\":[");

    int once = 1, onceki_satir = 0, onceki_kar = 0;
    if (belge && belge->icerik) {
        Lexer lx;
        lexer_baslat(&lx, belge->icerik, "lsp");
        for (;;) {
            Token tk = lexer_sonraki_token(&lx);
            if (tk.tip == TOK_DOSYA_SONU) break;
            int tip = semtok_tipi(tk.tip);
            if (tip == -2) {
                /* Anahtar kelime araligi: lexer.h'deki enum blogu
                 * TOK_EGER (ilk deger) .. TOK_GENEL (son anahtar kelime);
                 * hemen ardindan "Literaller" blogu gelir.
                 * ⚠ Ilk yazimda araligi TOK_ISLEV'den baslatmistim — enumun
                 * ILK 6 anahtar kelimesi (eger/degilse/için/iken/eşleş/ver)
                 * disarida kalirdi. Sinirlar lexer.h'den OLCULDU.
                 * Noktalama (`;` `,` `(` ...) ve TOK_HATALI atlanir. */
                if (tk.tip >= TOK_EGER && tk.tip <= TOK_GENEL) tip = 0;
                else continue;
            }
            int bas_ofset = (int)(tk.baslangic - belge->icerik);
            if (bas_ofset < 0 || bas_ofset > belge->icerik_uz) continue;
            int son_ofset = bas_ofset + tk.uzunluk;
            if (son_ofset > belge->icerik_uz) son_ofset = belge->icerik_uz;

            int satir0 = tk.satir > 0 ? tk.satir - 1 : 0;
            int kar = lsp_karakter(belge, tk.satir, tk.sutun);
            int uz16 = utf16_say(belge->icerik, bas_ofset, son_ofset);
            if (uz16 <= 0) continue;

            int d_satir = satir0 - onceki_satir;
            int d_kar = (d_satir == 0) ? (kar - onceki_kar) : kar;
            if (d_satir < 0 || d_kar < 0) continue;   /* geriye giden token: atla */

            if (!once) json_yaz(&y, ",");
            json_yaz_int(&y, d_satir); json_yaz(&y, ",");
            json_yaz_int(&y, d_kar);   json_yaz(&y, ",");
            json_yaz_int(&y, uz16);    json_yaz(&y, ",");
            json_yaz_int(&y, tip);     json_yaz(&y, ",0");
            once = 0;
            onceki_satir = satir0;
            onceki_kar = kar;
        }
    }
    json_yaz(&y, "]}}");
    mesaj_yaz(cikti, y.tampon, y.kullanilan);
    json_yazici_serbest(&y);
}

/* === workspace/symbol (LSP v3) — D-434 ===
 * ⚠ SADECE D-433'ten SONRA ANLAMLI: sunucu tek belge tutarken "workspace
 * symbol" yalnız açık dosyayı arardı ve ad YANILTICI olurdu. Artık
 * `BelgeTablo`daki TÜM açık belgeler gezilir.
 *
 * ⚠ SymbolKind ≠ CompletionItemKind. Bunlar AYRI LSP enumlarıdır ve
 * `completion_yanitla` bilerek CompletionItemKind (islev=3) kullanır.
 * Burada `documentSymbol`un SymbolKind'ı yeniden kullanılır (islev=12,
 * sabit=14, yapı=23, çeşit=10, özellik=11) — ikisini karıştırmak istemciye
 * yanlış ikon/gruplama gösterir.
 *
 * SINIR (bilinçli, dürüst): yalnız AÇIK belgeler taranır. Diske bakıp tüm
 * projeyi indekslemek ayrı bir iştir (dosya sistemi gezme + izleme);
 * "açık dosyalar" LSP'de geçerli ve yaygın bir workspace tanımıdır. */
static int ws_kind(const char *kat) {
    if (!kat) return 13;                        /* Variable */
    if (strcmp(kat, "islev") == 0) return 12;   /* Function */
    if (strcmp(kat, "sabit") == 0) return 14;   /* Constant */
    if (strcmp(kat, "yapi") == 0) return 23;    /* Struct */
    if (strcmp(kat, "cesit") == 0) return 10;   /* Enum */
    if (strcmp(kat, "ozellik") == 0) return 11; /* Interface */
    return 13;
}

/* ASCII-küçültmeli alt-dizi araması (LSP istemcileri harf duyarsız bekler). */
static int ws_eslesir(const char *ad, int ad_uz, const char *q, int q_uz) {
    if (q_uz <= 0) return 1;                    /* boş sorgu = hepsi */
    if (q_uz > ad_uz) return 0;
    for (int i = 0; i + q_uz <= ad_uz; i++) {
        int j = 0;
        while (j < q_uz) {
            char a = ad[i + j], b = q[j];
            if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
            if (a != b) break;
            j++;
        }
        if (j == q_uz) return 1;
    }
    return 0;
}

static void workspacesymbol_yanitla(FILE *cikti, JsonDeger *istek,
                                    BelgeTablo *tablo) {
    JsonDeger *id = json_alan(istek, "id");
    JsonYazici y;
    json_yazici_baslat(&y);
    json_yaz(&y, "{\"jsonrpc\":\"2.0\",\"id\":");
    if (id && id->tip == JSON_TAMSAYI) json_yaz_int(&y, id->veri.tamsayi);
    else json_yaz(&y, "null");
    json_yaz(&y, ",\"result\":[");

    int q_uz = 0;
    const char *q = NULL;
    JsonDeger *params = json_alan(istek, "params");
    if (params) q = json_metin(json_alan(params, "query"), &q_uz);

    int once = 1;
    for (int i = 0; tablo && i < tablo->sayi; i++) {
        Belge *b = &tablo->belgeler[i];
        if (!b->uri) continue;
        for (BelgeSembol *s = b->semboller; s; s = s->sonraki) {
            if (!s->ad || s->ad_uz <= 0) continue;
            if (!ws_eslesir(s->ad, s->ad_uz, q, q_uz)) continue;
            if (!once) json_yaz(&y, ",");
            json_yaz(&y, "{\"name\":");
            json_yaz_metin_lit_n(&y, s->ad, (size_t)s->ad_uz);
            json_yaz(&y, ",\"kind\":");
            json_yaz_int(&y, ws_kind(s->kategori));
            json_yaz(&y, ",\"location\":{\"uri\":");
            json_yaz_metin_lit(&y, b->uri);
            json_yaz(&y, ",\"range\":");
            /* Tanim adinin araligi; sutunlar UTF-16'ya cevrilir. */
            int sat0 = s->satir > 0 ? s->satir - 1 : 0;
            int kar0 = lsp_karakter(b, s->satir, s->sutun);
            int kar1 = kar0 + utf16_say(s->ad, 0, s->ad_uz);
            json_yaz(&y, "{\"start\":{\"line\":");
            json_yaz_int(&y, sat0);
            json_yaz(&y, ",\"character\":");
            json_yaz_int(&y, kar0);
            json_yaz(&y, "},\"end\":{\"line\":");
            json_yaz_int(&y, sat0);
            json_yaz(&y, ",\"character\":");
            json_yaz_int(&y, kar1);
            json_yaz(&y, "}}}}");
            once = 0;
        }
    }
    json_yaz(&y, "]}");
    mesaj_yaz(cikti, y.tampon, y.kullanilan);
    json_yazici_serbest(&y);
}

/* === Completion === */

static const char *KEYWORDS[] = {
    "e\xc4\x9f" "er", "de\xc4\x9f" "ilse", "i\xc3\xa7" "in", "iken",
    "e\xc5\x9fle\xc5\x9f", "ver", "i\xc5\x9flev", "yap\xc4\xb1",
    "\xc3\xb6zellik", "mod\xc3\xbcl", "de\xc4\x9fi\xc5\x9fken", "sabit",
    "do\xc4\x9fru", "yanl\xc4\xb1\xc5\x9f", "bo\xc5\x9f",
    "ve", "veya", "de\xc4\x9f" "il", "kullan", "d\xc4\xb1\xc5\x9f" "a",
    "tamam", "hata", "uygula", "kendin", "se\xc3\xa7imlik", "sonu\xc3\xa7",
    "de\xc4\x9f" "er", "hi\xc3\xa7", "g\xc3\xbcvensiz",
    "tam32", "tam64", "tam8", "tam16", "metin", "mant\xc4\xb1ksal",
    NULL
};

static void completion_yanitla(FILE *cikti, JsonDeger *istek, Belge *belge) {
    JsonDeger *id = json_alan(istek, "id");

    JsonYazici y;
    json_yazici_baslat(&y);
    json_yaz(&y, "{\"jsonrpc\":\"2.0\",\"id\":");
    if (id && id->tip == JSON_TAMSAYI) json_yaz_int(&y, id->veri.tamsayi);
    else json_yaz(&y, "null");

    json_yaz(&y, ",\"result\":{\"isIncomplete\":false,\"items\":[");
    int once = 1;
    /* Anahtar kelimeler */
    for (int i = 0; KEYWORDS[i]; i++) {
        if (!once) json_yaz(&y, ",");
        once = 0;
        json_yaz(&y, "{\"label\":");
        json_yaz_metin_lit(&y, KEYWORDS[i]);
        json_yaz(&y, ",\"kind\":14}");  /* CompletionItemKind.Keyword */
    }
    /* Ust duzey semboller */
    if (belge) {
        for (BelgeSembol *s = belge->semboller; s; s = s->sonraki) {
            if (!once) json_yaz(&y, ",");
            once = 0;
            json_yaz(&y, "{\"label\":");
            json_yaz_metin_lit_n(&y, s->ad, (size_t)s->ad_uz);
            json_yaz(&y, ",\"kind\":");
            /* CompletionItemKind: islev=3 (Function), yapi=22 (Struct),
             * ozellik=8 (Interface), sabit=21 (Constant) */
            int kind = 6; /* Variable default */
            if (strcmp(s->kategori, "islev") == 0) kind = 3;
            else if (strcmp(s->kategori, "yapi") == 0) kind = 22;
            else if (strcmp(s->kategori, "ozellik") == 0) kind = 8;
            else if (strcmp(s->kategori, "sabit") == 0) kind = 21;
            else if (strcmp(s->kategori, "cesit") == 0) kind = 13;
            json_yaz_int(&y, kind);
            json_yaz(&y, ",\"detail\":");
            json_yaz_metin_lit(&y, s->kategori);
            json_yaz(&y, "}");
        }
    }
    json_yaz(&y, "]}}");
    mesaj_yaz(cikti, y.tampon, y.kullanilan);
    json_yazici_serbest(&y);
}

static void shutdown_yanitla(FILE *cikti, JsonDeger *istek) {
    JsonDeger *id = json_alan(istek, "id");
    JsonYazici y;
    json_yazici_baslat(&y);
    json_yaz(&y, "{\"jsonrpc\":\"2.0\",\"id\":");
    if (id && id->tip == JSON_TAMSAYI) {
        json_yaz_int(&y, id->veri.tamsayi);
    } else if (id && id->tip == JSON_METIN) {
        json_yaz_metin_lit_n(&y, id->veri.str.metin, (size_t)id->veri.str.uzunluk);
    } else {
        json_yaz(&y, "null");
    }
    json_yaz(&y, ",\"result\":null}");
    mesaj_yaz(cikti, y.tampon, y.kullanilan);
    json_yazici_serbest(&y);
    kapanis_isteniyor = 1;
}

static void open_or_change_isle(FILE *cikti, JsonDeger *istek, Belge *belge,
                                DiagCtx *dc, int change_mu) {
    JsonDeger *params = json_alan(istek, "params");
    if (!params) return;
    JsonDeger *td = json_alan(params, "textDocument");
    if (!td) return;
    int uri_uz = 0;
    const char *uri = json_metin(json_alan(td, "uri"), &uri_uz);
    if (!uri) return;

    if (change_mu) {
        JsonDeger *changes = json_alan(params, "contentChanges");
        int n = json_dizi_sayi(changes);
        int uygulandi = 0;
        for (int i = 0; i < n; i++) {
            JsonDeger *c = json_dizi_eleman(changes, i);
            int t_uz = 0;
            const char *t = json_metin(json_alan(c, "text"), &t_uz);
            if (!t) continue;
            JsonDeger *range = json_alan(c, "range");
            if (range && belge->icerik) {
                belge_aralik_uygula(belge, range, t, t_uz);
            } else {
                /* Geriye uyumlu tam-metin degisimi (full sync) */
                belge_set(belge, uri, uri_uz, t, t_uz);
            }
            uygulandi = 1;
        }
        if (!uygulandi) return;
        if (!belge->uri) return;
    } else {
        int icerik_uz = 0;
        const char *icerik = json_metin(json_alan(td, "text"), &icerik_uz);
        if (!icerik) return;
        belge_set(belge, uri, uri_uz, icerik, icerik_uz);
    }
    analiz_et(belge, dc);
    publish_diagnostics(cikti, belge->uri, dc, belge);
}

static void close_isle(FILE *cikti, JsonDeger *istek, Belge *belge,
                       DiagCtx *dc) {
    JsonDeger *params = json_alan(istek, "params");
    if (!params) return;
    JsonDeger *td = json_alan(params, "textDocument");
    if (!td) return;
    int uri_uz = 0;
    const char *uri = json_metin(json_alan(td, "uri"), &uri_uz);
    if (!uri) return;
    /* Bos diagnostic listesi gonder */
    diag_temizle(dc);
    publish_diagnostics(cikti, uri, dc, NULL);
    belge_temizle(belge);
}

/* === Server loop === */

int lsp_server_calistir(FILE *girdi, FILE *cikti) {
    BelgeTablo tablo;                 /* D-433: URI başına bir belge */
    memset(&tablo, 0, sizeof(tablo));
    DiagCtx dc;
    memset(&dc, 0, sizeof(dc));
    kapanis_isteniyor = 0;

    int exit_kodu = 0;

    while (!kapanis_isteniyor) {
        int uz = 0;
        char *govde = mesaj_oku(girdi, &uz);
        if (!govde) break;

        Arena *a = arena_olustur(0);
        if (!a) { free(govde); break; }

        const char *parse_hata = NULL;
        JsonDeger *istek = json_ayrist(a, govde, uz, &parse_hata);
        if (!istek) {
            fprintf(stderr, "LSP: json parse hatasi: %s\n",
                    parse_hata ? parse_hata : "?");
            arena_serbest(a);
            free(govde);
            continue;
        }

        int m_uz = 0;
        const char *yontem = json_metin(json_alan(istek, "method"), &m_uz);
        if (!yontem) {
            arena_serbest(a);
            free(govde);
            continue;
        }

        if (strcmp(yontem, "initialize") == 0) {
            initialize_yanitla(cikti, istek);
        } else if (strcmp(yontem, "shutdown") == 0) {
            shutdown_yanitla(cikti, istek);
        } else if (strcmp(yontem, "exit") == 0) {
            exit_kodu = kapanis_isteniyor ? 0 : 1;
            break;
        } else if (strcmp(yontem, "initialized") == 0) {
            /* no-op */
        } else if (strcmp(yontem, "workspace/symbol") == 0) {
            workspacesymbol_yanitla(cikti, istek, &tablo);   /* D-434 */
        } else if (strncmp(yontem, "textDocument/", 13) == 0) {
            /* D-433: her `textDocument` istegi KENDI uri'sine çözülür.
             * didOpen yoksa yuva açar; diğerleri yalnız ARAR — bilinmeyen
             * uri'de `belge` NULL kalır ve handler'lar bunu zaten
             * karşılıyor (hepsi `belge && belge->prog` denetler). */
            int uri_uz = 0;
            const char *uri = istek_uri(istek, &uri_uz);
            if (uri) {
                int acilis = (strcmp(yontem, "textDocument/didOpen") == 0);
                Belge *belge = acilis ? tablo_ac(&tablo, uri, uri_uz)
                                      : tablo_bul(&tablo, uri, uri_uz);
                if (belge) {
                    if (acilis) {
                        open_or_change_isle(cikti, istek, belge, &dc, 0);
                    } else if (strcmp(yontem, "textDocument/didChange") == 0) {
                        open_or_change_isle(cikti, istek, belge, &dc, 1);
                    } else if (strcmp(yontem, "textDocument/didClose") == 0) {
                        close_isle(cikti, istek, belge, &dc);
                    } else if (strcmp(yontem, "textDocument/hover") == 0) {
                        hover_yanitla(cikti, istek, belge);
                    } else if (strcmp(yontem, "textDocument/definition") == 0) {
                        definition_yanitla(cikti, istek, belge);
                    } else if (strcmp(yontem, "textDocument/completion") == 0) {
                        completion_yanitla(cikti, istek, belge);
                    } else if (strcmp(yontem, "textDocument/documentSymbol") == 0) {
                        documentsymbol_yanitla(cikti, istek, belge);
                    } else if (strcmp(yontem, "textDocument/references") == 0) {
                        references_yanitla(cikti, istek, belge);
                    } else if (strcmp(yontem, "textDocument/semanticTokens/full") == 0) {
                        semantictokens_yanitla(cikti, istek, belge);   /* D-432 */
                    }
                }
            }
        }
        /* Bilinmeyen request id'li ise yanitsiz birakiyoruz (LSP kabul edilebilir) */

        arena_serbest(a);
        free(govde);
    }

    tablo_temizle(&tablo);
    diag_temizle(&dc);
    return exit_kodu;
}
