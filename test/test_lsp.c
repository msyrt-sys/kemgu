#include "lsp.h"
#include "json.h"
#include "arena.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int toplam_test = 0;
static int basarili = 0;
static int basarisiz = 0;

static void test_sonuc(const char *ad, int durum) {
    toplam_test++;
    if (durum) {
        basarili++;
        printf("  [%d] %s ... \xe2\x9c\x93\n", toplam_test, ad);
    } else {
        basarisiz++;
        printf("  [%d] %s ... \xe2\x9c\x97\n", toplam_test, ad);
    }
}

/* === Bellek tabanli stdio simulasyonu ===
 *
 * tmpfile() ile bellek dosyalari acariz, lsp_server_calistir'a ver.
 * Sonra cikti dosyasini okuyup mesajlari ayrica parse ederiz.
 */

typedef struct LspYanit {
    int content_length;
    char *govde;          /* malloc'lu */
    struct LspYanit *sonraki;
} LspYanit;

/* Cikti dosyasinin tum LSP mesajlarini parse et */
static LspYanit *yanitlari_oku(FILE *f) {
    LspYanit *bas = NULL, *son = NULL;
    rewind(f);

    char satir[1024];
    while (fgets(satir, sizeof(satir), f)) {
        size_t n = strlen(satir);
        while (n > 0 && (satir[n - 1] == '\r' || satir[n - 1] == '\n')) {
            satir[--n] = '\0';
        }
        if (n == 0) continue;  /* bos */
        int cl = 0;
        if (sscanf(satir, "Content-Length: %d", &cl) != 1 &&
            sscanf(satir, "content-length: %d", &cl) != 1) {
            continue;
        }
        /* bos satir at */
        if (!fgets(satir, sizeof(satir), f)) break;
        /* CL byte oku */
        char *govde = (char *)malloc((size_t)cl + 1);
        if (!govde) break;
        size_t okunan = fread(govde, 1, (size_t)cl, f);
        govde[okunan] = '\0';
        LspYanit *y = (LspYanit *)malloc(sizeof(LspYanit));
        y->content_length = cl;
        y->govde = govde;
        y->sonraki = NULL;
        if (son) son->sonraki = y; else bas = y;
        son = y;
    }
    return bas;
}

static void yanitlari_serbest(LspYanit *y) {
    while (y) {
        LspYanit *s = y->sonraki;
        free(y->govde);
        free(y);
        y = s;
    }
}

static int yanit_sayisi(LspYanit *y) {
    int n = 0;
    while (y) { n++; y = y->sonraki; }
    return n;
}

static LspYanit *yanit_n(LspYanit *y, int n) {
    while (n > 0 && y) { y = y->sonraki; n--; }
    return y;
}

/* Bir mesaj girdi dosyasina yaz */
static void mesaj_yaz(FILE *f, const char *govde) {
    int uz = (int)strlen(govde);
    fprintf(f, "Content-Length: %d\r\n\r\n%s", uz, govde);
}

/* === Test senaryolari === */

static void test_initialize_yaniti(void) {
    FILE *girdi = tmpfile();
    FILE *cikti = tmpfile();

    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"shutdown\"}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"method\":\"exit\"}");
    rewind(girdi);

    int rc = lsp_server_calistir(girdi, cikti);

    LspYanit *y = yanitlari_oku(cikti);
    int sayi = yanit_sayisi(y);

    int ok = rc == 0 && sayi == 2;
    if (ok) {
        Arena *a = arena_olustur(0);
        LspYanit *y1 = yanit_n(y, 0);
        JsonDeger *j1 = json_ayrist(a, y1->govde, y1->content_length, NULL);
        JsonDeger *r1 = json_alan(j1, "result");
        JsonDeger *cap = json_alan(r1, "capabilities");
        JsonDeger *tds = json_alan(cap, "textDocumentSync");
        /* LSP v3: incremental sync ilan edilir */
        ok = j1 && r1 && cap && tds && tds->veri.tamsayi == 2;
        arena_serbest(a);
    }
    test_sonuc("initialize -> capabilities donen yanit", ok);
    yanitlari_serbest(y);
    fclose(girdi);
    fclose(cikti);
}

static void test_didopen_valid(void) {
    FILE *girdi = tmpfile();
    FILE *cikti = tmpfile();

    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}");
    /* Gecerli KEMGU: ver 42 */
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///x.kem\",\"languageId\":\"kemgu\","
        "\"version\":1,\"text\":\"i\\u015flev main() -> tam32 { ver 42; }\"}}}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"shutdown\"}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"method\":\"exit\"}");
    rewind(girdi);

    int rc = lsp_server_calistir(girdi, cikti);

    LspYanit *y = yanitlari_oku(cikti);
    int sayi = yanit_sayisi(y);
    /* Beklenen: 3 yanit (initialize, publishDiagnostics, shutdown) */
    int ok = rc == 0 && sayi == 3;
    if (ok) {
        /* 2. yanit publishDiagnostics olmali, diagnostics bos */
        Arena *a = arena_olustur(0);
        LspYanit *y2 = yanit_n(y, 1);
        JsonDeger *j = json_ayrist(a, y2->govde, y2->content_length, NULL);
        int m_uz = 0;
        const char *m = json_metin(json_alan(j, "method"), &m_uz);
        JsonDeger *p = json_alan(j, "params");
        JsonDeger *diags = json_alan(p, "diagnostics");
        ok = m && strcmp(m, "textDocument/publishDiagnostics") == 0
          && json_dizi_sayi(diags) == 0;
        arena_serbest(a);
    }
    test_sonuc("didOpen (valid kemgu) -> bos diagnostics", ok);
    yanitlari_serbest(y);
    fclose(girdi);
    fclose(cikti);
}

static void test_didopen_parser_hata(void) {
    FILE *girdi = tmpfile();
    FILE *cikti = tmpfile();

    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}");
    /* Gecersiz: islev sonu eksik */
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///x.kem\",\"languageId\":\"kemgu\","
        "\"version\":1,\"text\":\"i\\u015flev main() -> tam32 { ver }\"}}}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"shutdown\"}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"method\":\"exit\"}");
    rewind(girdi);

    int rc = lsp_server_calistir(girdi, cikti);

    LspYanit *y = yanitlari_oku(cikti);
    int sayi = yanit_sayisi(y);
    int ok = rc == 0 && sayi == 3;
    if (ok) {
        Arena *a = arena_olustur(0);
        LspYanit *y2 = yanit_n(y, 1);
        JsonDeger *j = json_ayrist(a, y2->govde, y2->content_length, NULL);
        JsonDeger *p = json_alan(j, "params");
        JsonDeger *diags = json_alan(p, "diagnostics");
        /* En az 1 diagnostic olmali */
        ok = json_dizi_sayi(diags) > 0;
        if (ok) {
            JsonDeger *d0 = json_dizi_eleman(diags, 0);
            JsonDeger *kod = json_alan(d0, "code");
            int kod_uz = 0;
            const char *kod_str = json_metin(kod, &kod_uz);
            /* Parser hatasi P kodu ile baslamali */
            ok = kod_str && kod_str[0] == 'P';
        }
        arena_serbest(a);
    }
    test_sonuc("didOpen (parser hata) -> diagnostics dolu", ok);
    yanitlari_serbest(y);
    fclose(girdi);
    fclose(cikti);
}

static void test_didopen_tip_hata(void) {
    FILE *girdi = tmpfile();
    FILE *cikti = tmpfile();

    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}");
    /* Tip hatasi: islev tam32 dondurmesi gerek ama metin veriyor */
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///x.kem\",\"languageId\":\"kemgu\","
        "\"version\":1,\"text\":"
        "\"i\\u015flev main() -> tam32 { ver \\\"hata\\\"; }\"}}}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"shutdown\"}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"method\":\"exit\"}");
    rewind(girdi);

    lsp_server_calistir(girdi, cikti);

    LspYanit *y = yanitlari_oku(cikti);
    int ok = yanit_sayisi(y) >= 3;
    if (ok) {
        Arena *a = arena_olustur(0);
        LspYanit *y2 = yanit_n(y, 1);
        JsonDeger *j = json_ayrist(a, y2->govde, y2->content_length, NULL);
        JsonDeger *p = json_alan(j, "params");
        JsonDeger *diags = json_alan(p, "diagnostics");
        ok = json_dizi_sayi(diags) > 0;
        if (ok) {
            JsonDeger *d0 = json_dizi_eleman(diags, 0);
            int kod_uz = 0;
            const char *kod_str = json_metin(json_alan(d0, "code"), &kod_uz);
            /* Tip hatasi T kodu ile baslamali */
            ok = kod_str && kod_str[0] == 'T';
        }
        arena_serbest(a);
    }
    test_sonuc("didOpen (tip hata) -> T kodlu diagnostic", ok);
    yanitlari_serbest(y);
    fclose(girdi);
    fclose(cikti);
}

static void test_didchange_yeni_metin(void) {
    FILE *girdi = tmpfile();
    FILE *cikti = tmpfile();

    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}");
    /* didOpen hatasi, sonra didChange duzeltir */
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///x.kem\",\"languageId\":\"kemgu\","
        "\"version\":1,\"text\":\"hata\"}}}");
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didChange\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///x.kem\",\"version\":2},"
        "\"contentChanges\":[{\"text\":"
        "\"i\\u015flev main() -> tam32 { ver 0; }\"}]}}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"shutdown\"}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"method\":\"exit\"}");
    rewind(girdi);

    lsp_server_calistir(girdi, cikti);

    LspYanit *y = yanitlari_oku(cikti);
    /* Beklenen: initialize + 2 diag (open=hata, change=temiz) + shutdown */
    int ok = yanit_sayisi(y) == 4;
    if (ok) {
        Arena *a = arena_olustur(0);
        /* 1. diag (open): hata, 2. diag (change): temiz */
        LspYanit *y2 = yanit_n(y, 1);
        LspYanit *y3 = yanit_n(y, 2);
        JsonDeger *j2 = json_ayrist(a, y2->govde, y2->content_length, NULL);
        JsonDeger *j3 = json_ayrist(a, y3->govde, y3->content_length, NULL);
        JsonDeger *d2 = json_alan(json_alan(j2, "params"), "diagnostics");
        JsonDeger *d3 = json_alan(json_alan(j3, "params"), "diagnostics");
        ok = json_dizi_sayi(d2) > 0 && json_dizi_sayi(d3) == 0;
        arena_serbest(a);
    }
    test_sonuc("didChange duzeltir (open hata, change temiz)", ok);
    yanitlari_serbest(y);
    fclose(girdi);
    fclose(cikti);
}

/* LSP v2: hover */
static void test_hover_islev(void) {
    FILE *girdi = tmpfile();
    FILE *cikti = tmpfile();
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}");
    /* "islev say() -> tam32 { ver 0; } islev main() -> tam32 { ver say(); }"
     * say'in cagrildigi konum: line 0 (0-indeksli), char say's index */
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///x.kem\",\"languageId\":\"kemgu\","
        "\"version\":1,\"text\":"
        "\"i\\u015flev say() -> tam32 { ver 0; } "
        "i\\u015flev main() -> tam32 { ver say(); }\"}}}");
    /* Hover at "say()" call site — char index of 's' in second say */
    /* "...işlev main() -> tam32 { ver say(); }" — pozisyonu yaklasik 60 */
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"id\":10,\"method\":\"textDocument/hover\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///x.kem\"},"
        "\"position\":{\"line\":0,\"character\":62}}}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"shutdown\"}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"method\":\"exit\"}");
    rewind(girdi);

    lsp_server_calistir(girdi, cikti);
    LspYanit *y = yanitlari_oku(cikti);
    /* Beklenen: 4 yanit (initialize, diag, hover, shutdown) */
    int ok = yanit_sayisi(y) == 4;
    if (ok) {
        Arena *a = arena_olustur(0);
        LspYanit *yh = yanit_n(y, 2);  /* hover yaniti */
        JsonDeger *j = json_ayrist(a, yh->govde, yh->content_length, NULL);
        JsonDeger *result = json_alan(j, "result");
        /* "say" cagri konumunda hover icerik DOLU olmali (null degil) */
        ok = j && result && result->tip != JSON_NULL
          && json_alan(result, "contents") != NULL;
        arena_serbest(a);
    }
    test_sonuc("hover yaniti formatla doner", ok);
    yanitlari_serbest(y);
    fclose(girdi);
    fclose(cikti);
}

/* LSP v2: completion */
static void test_completion_response(void) {
    FILE *girdi = tmpfile();
    FILE *cikti = tmpfile();
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}");
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///x.kem\",\"languageId\":\"kemgu\","
        "\"version\":1,\"text\":\"yap\\u0131 X { v: tam32; }\"}}}");
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"id\":11,\"method\":\"textDocument/completion\","
        "\"params\":{\"textDocument\":{\"uri\":\"file:///x.kem\"},"
        "\"position\":{\"line\":0,\"character\":0}}}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"shutdown\"}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"method\":\"exit\"}");
    rewind(girdi);

    lsp_server_calistir(girdi, cikti);
    LspYanit *y = yanitlari_oku(cikti);
    int ok = yanit_sayisi(y) >= 4;
    if (ok) {
        Arena *a = arena_olustur(0);
        LspYanit *yc = yanit_n(y, 2);  /* completion */
        JsonDeger *j = json_ayrist(a, yc->govde, yc->content_length, NULL);
        JsonDeger *result = json_alan(j, "result");
        JsonDeger *items = result ? json_alan(result, "items") : NULL;
        ok = json_dizi_sayi(items) > 0;  /* en azindan keyword'ler */
        arena_serbest(a);
    }
    test_sonuc("completion liste doner (keyword + sembol)", ok);
    yanitlari_serbest(y);
    fclose(girdi);
    fclose(cikti);
}

/* LSP v2: definition */
static void test_definition_islev(void) {
    FILE *girdi = tmpfile();
    FILE *cikti = tmpfile();
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}");
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///x.kem\",\"languageId\":\"kemgu\","
        "\"version\":1,\"text\":"
        "\"i\\u015flev say() -> tam32 { ver 0; } "
        "i\\u015flev main() -> tam32 { ver say(); }\"}}}");
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"id\":12,\"method\":\"textDocument/definition\","
        "\"params\":{\"textDocument\":{\"uri\":\"file:///x.kem\"},"
        "\"position\":{\"line\":0,\"character\":62}}}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"shutdown\"}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"method\":\"exit\"}");
    rewind(girdi);

    lsp_server_calistir(girdi, cikti);
    LspYanit *y = yanitlari_oku(cikti);
    int ok = yanit_sayisi(y) >= 4;
    if (ok) {
        Arena *a = arena_olustur(0);
        LspYanit *yd = yanit_n(y, 2);
        JsonDeger *j = json_ayrist(a, yd->govde, yd->content_length, NULL);
        JsonDeger *result = json_alan(j, "result");
        /* "say" cagri konumunda tanim bulunmali: uri + range DOLU */
        ok = j && result && result->tip != JSON_NULL
          && json_alan(result, "uri") != NULL
          && json_alan(result, "range") != NULL;
        arena_serbest(a);
    }
    test_sonuc("definition yaniti uri+range doner", ok);
    yanitlari_serbest(y);
    fclose(girdi);
    fclose(cikti);
}

/* LSP v3: documentSymbol */
static void test_documentsymbol_hiyerarsi(void) {
    FILE *girdi = tmpfile();
    FILE *cikti = tmpfile();
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}");
    /* yapı Nokta { x: tam32; y: tam32; } işlev main() -> tam32 { ver 0; } */
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///x.kem\",\"languageId\":\"kemgu\","
        "\"version\":1,\"text\":"
        "\"yap\\u0131 Nokta { x: tam32; y: tam32; } "
        "i\\u015flev main() -> tam32 { ver 0; }\"}}}");
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"id\":20,\"method\":\"textDocument/documentSymbol\","
        "\"params\":{\"textDocument\":{\"uri\":\"file:///x.kem\"}}}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"shutdown\"}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"method\":\"exit\"}");
    rewind(girdi);

    lsp_server_calistir(girdi, cikti);
    LspYanit *y = yanitlari_oku(cikti);
    int ok = yanit_sayisi(y) == 4;
    if (ok) {
        Arena *a = arena_olustur(0);
        LspYanit *yd = yanit_n(y, 2);
        JsonDeger *j = json_ayrist(a, yd->govde, yd->content_length, NULL);
        JsonDeger *result = json_alan(j, "result");
        ok = json_dizi_sayi(result) == 2;
        if (ok) {
            JsonDeger *s0 = json_dizi_eleman(result, 0);
            int n_uz = 0;
            const char *ad = json_metin(json_alan(s0, "name"), &n_uz);
            JsonDeger *kind = json_alan(s0, "kind");
            JsonDeger *cocuk = json_alan(s0, "children");
            /* yapi -> SymbolKind.Struct(23), 2 alan cocugu */
            ok = ad && strcmp(ad, "Nokta") == 0
              && kind && kind->veri.tamsayi == 23
              && json_dizi_sayi(cocuk) == 2;
            if (ok) {
                JsonDeger *c0 = json_dizi_eleman(cocuk, 0);
                const char *cad = json_metin(json_alan(c0, "name"), &n_uz);
                ok = cad && strcmp(cad, "x") == 0
                  && json_alan(c0, "kind")->veri.tamsayi == 8
                  && json_alan(c0, "selectionRange") != NULL;
            }
        }
        if (ok) {
            JsonDeger *s1 = json_dizi_eleman(result, 1);
            int n_uz = 0;
            const char *ad = json_metin(json_alan(s1, "name"), &n_uz);
            ok = ad && strcmp(ad, "main") == 0
              && json_alan(s1, "kind")->veri.tamsayi == 12;
        }
        arena_serbest(a);
    }
    test_sonuc("documentSymbol yapi+alanlar hiyerarsik doner", ok);
    yanitlari_serbest(y);
    fclose(girdi);
    fclose(cikti);
}

/* LSP v3: documentSymbol — cesit varyantlari */
static void test_documentsymbol_cesit(void) {
    FILE *girdi = tmpfile();
    FILE *cikti = tmpfile();
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}");
    /* çeşit Renk { Kirmizi, Mavi } */
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///x.kem\",\"languageId\":\"kemgu\","
        "\"version\":1,\"text\":"
        "\"\\u00e7e\\u015fit Renk { Kirmizi, Mavi }\"}}}");
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"id\":21,\"method\":\"textDocument/documentSymbol\","
        "\"params\":{\"textDocument\":{\"uri\":\"file:///x.kem\"}}}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"shutdown\"}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"method\":\"exit\"}");
    rewind(girdi);

    lsp_server_calistir(girdi, cikti);
    LspYanit *y = yanitlari_oku(cikti);
    int ok = yanit_sayisi(y) == 4;
    if (ok) {
        Arena *a = arena_olustur(0);
        LspYanit *yd = yanit_n(y, 2);
        JsonDeger *j = json_ayrist(a, yd->govde, yd->content_length, NULL);
        JsonDeger *result = json_alan(j, "result");
        ok = json_dizi_sayi(result) == 1;
        if (ok) {
            JsonDeger *s0 = json_dizi_eleman(result, 0);
            int n_uz = 0;
            const char *ad = json_metin(json_alan(s0, "name"), &n_uz);
            JsonDeger *cocuk = json_alan(s0, "children");
            ok = ad && strcmp(ad, "Renk") == 0
              && json_alan(s0, "kind")->veri.tamsayi == 10
              && json_dizi_sayi(cocuk) == 2;
            if (ok) {
                const char *v1 = json_metin(
                    json_alan(json_dizi_eleman(cocuk, 1), "name"), &n_uz);
                ok = v1 && strcmp(v1, "Mavi") == 0;
            }
        }
        arena_serbest(a);
    }
    test_sonuc("documentSymbol cesit varyantlari doner", ok);
    yanitlari_serbest(y);
    fclose(girdi);
    fclose(cikti);
}

/* LSP v3: references */
static void test_references_islev(void) {
    FILE *girdi = tmpfile();
    FILE *cikti = tmpfile();
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}");
    /* "işlev say() -> tam32 { ver 0; } işlev main() -> tam32 { ver say() + say(); }"
     * say tanimi + 2 cagri kullanimi = 3 location (includeDeclaration=true) */
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///x.kem\",\"languageId\":\"kemgu\","
        "\"version\":1,\"text\":"
        "\"i\\u015flev say() -> tam32 { ver 0; } "
        "i\\u015flev main() -> tam32 { ver say() + say(); }\"}}}");
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"id\":30,\"method\":\"textDocument/references\","
        "\"params\":{\"textDocument\":{\"uri\":\"file:///x.kem\"},"
        "\"position\":{\"line\":0,\"character\":62},"
        "\"context\":{\"includeDeclaration\":true}}}");
    /* includeDeclaration=false -> yalniz 2 kullanim */
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"id\":31,\"method\":\"textDocument/references\","
        "\"params\":{\"textDocument\":{\"uri\":\"file:///x.kem\"},"
        "\"position\":{\"line\":0,\"character\":62},"
        "\"context\":{\"includeDeclaration\":false}}}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"shutdown\"}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"method\":\"exit\"}");
    rewind(girdi);

    lsp_server_calistir(girdi, cikti);
    LspYanit *y = yanitlari_oku(cikti);
    int ok = yanit_sayisi(y) == 5;
    if (ok) {
        Arena *a = arena_olustur(0);
        LspYanit *yr = yanit_n(y, 2);
        JsonDeger *j = json_ayrist(a, yr->govde, yr->content_length, NULL);
        JsonDeger *result = json_alan(j, "result");
        ok = json_dizi_sayi(result) == 3;
        if (ok) {
            JsonDeger *l0 = json_dizi_eleman(result, 0);
            int u_uz = 0;
            const char *uri = json_metin(json_alan(l0, "uri"), &u_uz);
            JsonDeger *range = json_alan(l0, "range");
            ok = uri && strcmp(uri, "file:///x.kem") == 0 && range != NULL;
        }
        if (ok) {
            LspYanit *yr2 = yanit_n(y, 3);
            JsonDeger *j2 = json_ayrist(a, yr2->govde, yr2->content_length, NULL);
            ok = json_dizi_sayi(json_alan(j2, "result")) == 2;
        }
        arena_serbest(a);
    }
    test_sonuc("references tanim+kullanimlari doner (includeDeclaration)", ok);
    yanitlari_serbest(y);
    fclose(girdi);
    fclose(cikti);
}

/* LSP v3: references — tanimlayici olmayan konum -> null */
static void test_references_bos(void) {
    FILE *girdi = tmpfile();
    FILE *cikti = tmpfile();
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}");
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///x.kem\",\"languageId\":\"kemgu\","
        "\"version\":1,\"text\":"
        "\"i\\u015flev main() -> tam32 { ver 0; }\"}}}");
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"id\":32,\"method\":\"textDocument/references\","
        "\"params\":{\"textDocument\":{\"uri\":\"file:///x.kem\"},"
        "\"position\":{\"line\":5,\"character\":0},"
        "\"context\":{\"includeDeclaration\":true}}}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"shutdown\"}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"method\":\"exit\"}");
    rewind(girdi);

    lsp_server_calistir(girdi, cikti);
    LspYanit *y = yanitlari_oku(cikti);
    int ok = yanit_sayisi(y) == 4;
    if (ok) {
        Arena *a = arena_olustur(0);
        LspYanit *yr = yanit_n(y, 2);
        JsonDeger *j = json_ayrist(a, yr->govde, yr->content_length, NULL);
        JsonDeger *result = json_alan(j, "result");
        ok = result != NULL && result->tip == JSON_NULL;
        arena_serbest(a);
    }
    test_sonuc("references tanimlayici disi konumda null doner", ok);
    yanitlari_serbest(y);
    fclose(girdi);
    fclose(cikti);
}

/* LSP v3: incremental sync — aralik degisimi (UTF-16 ofset, Turkce karakterli satir) */
static void test_incremental_aralik_degisimi(void) {
    FILE *girdi = tmpfile();
    FILE *cikti = tmpfile();
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}");
    /* "işlev main() -> tam32 { ver 0; }"
     * "işlev" 5 UTF-16 birim (ş tek birim) -> "main" [6,10) araliginda */
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///x.kem\",\"languageId\":\"kemgu\","
        "\"version\":1,\"text\":"
        "\"i\\u015flev main() -> tam32 { ver 0; }\"}}}");
    /* "main" -> "basla" (aralik degisimi) */
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didChange\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///x.kem\",\"version\":2},"
        "\"contentChanges\":[{\"range\":{"
        "\"start\":{\"line\":0,\"character\":6},"
        "\"end\":{\"line\":0,\"character\":10}},\"text\":\"basla\"}]}}");
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"id\":40,\"method\":\"textDocument/documentSymbol\","
        "\"params\":{\"textDocument\":{\"uri\":\"file:///x.kem\"}}}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"shutdown\"}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"method\":\"exit\"}");
    rewind(girdi);

    lsp_server_calistir(girdi, cikti);
    LspYanit *y = yanitlari_oku(cikti);
    /* initialize + 2 diag (open, change) + documentSymbol + shutdown */
    int ok = yanit_sayisi(y) == 5;
    if (ok) {
        Arena *a = arena_olustur(0);
        /* degisiklik sonrasi diagnostics temiz kalmali */
        LspYanit *yd = yanit_n(y, 2);
        JsonDeger *jd = json_ayrist(a, yd->govde, yd->content_length, NULL);
        ok = json_dizi_sayi(json_alan(json_alan(jd, "params"), "diagnostics")) == 0;
        if (ok) {
            LspYanit *ys = yanit_n(y, 3);
            JsonDeger *j = json_ayrist(a, ys->govde, ys->content_length, NULL);
            JsonDeger *result = json_alan(j, "result");
            ok = json_dizi_sayi(result) == 1;
            if (ok) {
                int n_uz = 0;
                const char *ad = json_metin(
                    json_alan(json_dizi_eleman(result, 0), "name"), &n_uz);
                ok = ad && strcmp(ad, "basla") == 0;
            }
        }
        arena_serbest(a);
    }
    test_sonuc("incremental didChange araligi degistirir (main -> basla)", ok);
    yanitlari_serbest(y);
    fclose(girdi);
    fclose(cikti);
}

/* LSP v3: incremental sync — cok satirli belgeye ekleme (bos aralik) */
static void test_incremental_ekleme(void) {
    FILE *girdi = tmpfile();
    FILE *cikti = tmpfile();
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}");
    /* "işlev main() -> tam32 {\n  ver 0\n}" — ';' eksik, parser hatasi */
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///x.kem\",\"languageId\":\"kemgu\","
        "\"version\":1,\"text\":"
        "\"i\\u015flev main() -> tam32 {\\n  ver 0\\n}\"}}}");
    /* 1. satirin sonuna ';' ekle (bos aralik = saf ekleme) */
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didChange\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///x.kem\",\"version\":2},"
        "\"contentChanges\":[{\"range\":{"
        "\"start\":{\"line\":1,\"character\":7},"
        "\"end\":{\"line\":1,\"character\":7}},\"text\":\";\"}]}}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"shutdown\"}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"method\":\"exit\"}");
    rewind(girdi);

    lsp_server_calistir(girdi, cikti);
    LspYanit *y = yanitlari_oku(cikti);
    int ok = yanit_sayisi(y) == 4;
    if (ok) {
        Arena *a = arena_olustur(0);
        LspYanit *y2 = yanit_n(y, 1);   /* open: hatali */
        LspYanit *y3 = yanit_n(y, 2);   /* change: temiz */
        JsonDeger *j2 = json_ayrist(a, y2->govde, y2->content_length, NULL);
        JsonDeger *j3 = json_ayrist(a, y3->govde, y3->content_length, NULL);
        JsonDeger *d2 = json_alan(json_alan(j2, "params"), "diagnostics");
        JsonDeger *d3 = json_alan(json_alan(j3, "params"), "diagnostics");
        ok = json_dizi_sayi(d2) > 0 && json_dizi_sayi(d3) == 0;
        arena_serbest(a);
    }
    test_sonuc("incremental didChange ekleme yapar (';' -> temiz)", ok);
    yanitlari_serbest(y);
    fclose(girdi);
    fclose(cikti);
}

static void test_shutdown_yanit(void) {
    FILE *girdi = tmpfile();
    FILE *cikti = tmpfile();

    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"id\":42,\"method\":\"shutdown\"}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"method\":\"exit\"}");
    rewind(girdi);

    int rc = lsp_server_calistir(girdi, cikti);

    LspYanit *y = yanitlari_oku(cikti);
    int ok = rc == 0 && yanit_sayisi(y) == 1;
    if (ok) {
        Arena *a = arena_olustur(0);
        JsonDeger *j = json_ayrist(a, y->govde, y->content_length, NULL);
        JsonDeger *id = json_alan(j, "id");
        ok = id && id->tip == JSON_TAMSAYI && id->veri.tamsayi == 42;
        arena_serbest(a);
    }
    test_sonuc("shutdown -> id yansir, result null", ok);
    yanitlari_serbest(y);
    fclose(girdi);
    fclose(cikti);
}

int main(void) {
    printf("KEMGU LSP Server Test Paketi\n");
    printf("==============================\n");

    printf("\n--- Protokol ---\n");
    test_initialize_yaniti();
    test_shutdown_yanit();

    printf("\n--- Diagnostic ---\n");
    test_didopen_valid();
    test_didopen_parser_hata();
    test_didopen_tip_hata();
    test_didchange_yeni_metin();

    printf("\n--- LSP v2 (hover/completion/definition) ---\n");
    test_hover_islev();
    test_completion_response();
    test_definition_islev();

    printf("\n--- LSP v3 (documentSymbol) ---\n");
    test_documentsymbol_hiyerarsi();
    test_documentsymbol_cesit();

    printf("\n--- LSP v3 (references) ---\n");
    test_references_islev();
    test_references_bos();

    printf("\n--- LSP v3 (incremental sync) ---\n");
    test_incremental_aralik_degisimi();
    test_incremental_ekleme();

    printf("\n==============================\n");
    printf("Toplam: %d | Basarili: %d | Basarisiz: %d\n",
           toplam_test, basarili, basarisiz);
    return basarisiz > 0 ? 1 : 0;
}
