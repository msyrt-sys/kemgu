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

/* === Belge state (tek dosya icin MVP) === */

typedef struct Belge {
    char *uri;
    char *icerik;
    int icerik_uz;
} Belge;

static void belge_set(Belge *b, const char *uri, int uri_uz,
                      const char *icerik, int icerik_uz) {
    free(b->uri);
    free(b->icerik);
    b->uri = (char *)malloc((size_t)uri_uz + 1);
    if (b->uri) { memcpy(b->uri, uri, (size_t)uri_uz); b->uri[uri_uz] = '\0'; }
    b->icerik = (char *)malloc((size_t)icerik_uz + 1);
    if (b->icerik) {
        memcpy(b->icerik, icerik, (size_t)icerik_uz);
        b->icerik[icerik_uz] = '\0';
    }
    b->icerik_uz = icerik_uz;
}

static void belge_temizle(Belge *b) {
    free(b->uri);
    free(b->icerik);
    memset(b, 0, sizeof(*b));
}

/* === Diagnostic yayini === */

static void publish_diagnostics(FILE *cikti, const char *uri, DiagCtx *dc) {
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
        /* LSP konum 0-tabanli, KEMGU 1-tabanli. Sutun 0 olamayacagi icin
         * dikkat: 1 -> 0. */
        int s = d->satir > 0 ? d->satir - 1 : 0;
        int k = d->sutun > 0 ? d->sutun - 1 : 0;
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

/* === Belgeyi analiz et === */

static void analiz_et(const char *kaynak, DiagCtx *dc) {
    Arena *a = arena_olustur(0);
    if (!a) return;

    diag_temizle(dc);
    hata_callback_ayarla(diag_callback, dc);

    Lexer l;
    lexer_baslat(&l, kaynak, "lsp");
    Parser p;
    parser_baslat(&p, &l, a, "lsp", kaynak);
    Dugum *prog = parser_calistir(&p);

    if (prog && p.hata_sayisi == 0) {
        Scope *g = scope_olustur(a, SCOPE_GLOBAL, NULL);
        TipKontrol tk;
        tip_kontrol_baslat(&tk, a, g, "lsp", kaynak);
        tip_kontrol_program(&tk, prog);
    }

    hata_callback_ayarla(NULL, NULL);
    arena_serbest(a);
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
    /* Capabilities: textDocumentSync = 1 (full) */
    json_yaz(&y, ",\"result\":{\"capabilities\":{"
                 "\"textDocumentSync\":1,"
                 "\"diagnosticProvider\":{\"interFileDependencies\":false,"
                 "\"workspaceDiagnostics\":false}"
                 "},\"serverInfo\":{\"name\":\"kemgu-lsp\",\"version\":\"0.1\"}}}");
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

    const char *icerik = NULL;
    int icerik_uz = 0;
    if (change_mu) {
        JsonDeger *changes = json_alan(params, "contentChanges");
        int n = json_dizi_sayi(changes);
        if (n > 0) {
            JsonDeger *c0 = json_dizi_eleman(changes, 0);
            icerik = json_metin(json_alan(c0, "text"), &icerik_uz);
        }
    } else {
        icerik = json_metin(json_alan(td, "text"), &icerik_uz);
    }
    if (!icerik) return;

    belge_set(belge, uri, uri_uz, icerik, icerik_uz);
    analiz_et(belge->icerik, dc);
    publish_diagnostics(cikti, belge->uri, dc);
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
    publish_diagnostics(cikti, uri, dc);
    belge_temizle(belge);
}

/* === Server loop === */

int lsp_server_calistir(FILE *girdi, FILE *cikti) {
    Belge belge;
    memset(&belge, 0, sizeof(belge));
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
        } else if (strcmp(yontem, "textDocument/didOpen") == 0) {
            open_or_change_isle(cikti, istek, &belge, &dc, 0);
        } else if (strcmp(yontem, "textDocument/didChange") == 0) {
            open_or_change_isle(cikti, istek, &belge, &dc, 1);
        } else if (strcmp(yontem, "textDocument/didClose") == 0) {
            close_isle(cikti, istek, &belge, &dc);
        }
        /* Bilinmeyen request id'li ise yanitsiz birakiyoruz (LSP kabul edilebilir) */

        arena_serbest(a);
        free(govde);
    }

    belge_temizle(&belge);
    diag_temizle(&dc);
    return exit_kodu;
}
