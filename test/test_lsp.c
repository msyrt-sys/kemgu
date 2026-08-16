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

/* === UTF-16 konum donusumu (LSP spec: `character` = UTF-16 kod birimi) ===
 *
 * Kullanilan belge (iki satir):
 *   satir 0: "işlev ölç() -> tam32 { ver 7; }"
 *   satir 1: "işlev main() -> tam32 { ver ölç(); }"
 *
 * Satir 1'de "ölç" cagrisi: UTF-16 sutun 28 (bayt sutunu 29) — fark "işlev"
 * icindeki 'ş' harfinin 2 bayt olmasindan gelir.
 *
 * NOT (mevcut davranis, bu adimda DEGISTIRILMEDI): tanim konumu olarak AST
 * dugumunun konumu kullanilir, yani `işlev` anahtar kelimesinin sutunu (0).
 * Aralik uzunlugu ise adin UTF-16 birim sayisidir ("ölç" = 3). Bu yuzden
 * tanim araligi (0,0)-(0,3) beklenir. Bayt tabanli eski kod (0,0)-(0,5)
 * uretiyordu — "ölç" 5 BAYT oldugu icin.
 */

#define UTF16_BELGE \
    "\"i\\u015flev \\u00f6l\\u00e7() -> tam32 { ver 7; }\\n" \
    "i\\u015flev main() -> tam32 { ver \\u00f6l\\u00e7(); }\""

/* Bir Range nesnesinden start/end character + line degerlerini oku. */
static int range_oku(JsonDeger *range, int *sl, int *sc, int *el, int *ec) {
    if (!range) return 0;
    JsonDeger *st = json_alan(range, "start");
    JsonDeger *en = json_alan(range, "end");
    if (!st || !en) return 0;
    *sl = (int)json_tamsayi(json_alan(st, "line"));
    *sc = (int)json_tamsayi(json_alan(st, "character"));
    *el = (int)json_tamsayi(json_alan(en, "line"));
    *ec = (int)json_tamsayi(json_alan(en, "character"));
    return 1;
}

/* didOpen + tek istek gonderen ortak yardimci; istenen yanit dondurulur. */
static void utf16_belge_ac(FILE *girdi) {
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}");
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///t.kem\",\"languageId\":\"kemgu\","
        "\"version\":1,\"text\":" UTF16_BELGE "}}}");
}

static void test_utf16_hover_turkce(void) {
    FILE *girdi = tmpfile();
    FILE *cikti = tmpfile();
    utf16_belge_ac(girdi);
    /* UTF-16 sutun 28 = "ölç" cagrisinin 'ö' harfi */
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"id\":10,\"method\":\"textDocument/hover\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///t.kem\"},"
        "\"position\":{\"line\":1,\"character\":28}}}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"shutdown\"}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"method\":\"exit\"}");
    rewind(girdi);

    lsp_server_calistir(girdi, cikti);
    LspYanit *y = yanitlari_oku(cikti);
    int ok = yanit_sayisi(y) == 4;
    if (ok) {
        Arena *a = arena_olustur(0);
        LspYanit *yh = yanit_n(y, 2);
        JsonDeger *j = json_ayrist(a, yh->govde, yh->content_length, NULL);
        JsonDeger *result = json_alan(j, "result");
        ok = j && result && result->tip != JSON_NULL
          && json_alan(result, "contents") != NULL;
        arena_serbest(a);
    }
    test_sonuc("hover: T\xc3\xbcrk\xc3\xa7" "e satirda UTF-16 sutunu bulur", ok);
    yanitlari_serbest(y);
    fclose(girdi);
    fclose(cikti);
}

static void test_utf16_definition_turkce(void) {
    FILE *girdi = tmpfile();
    FILE *cikti = tmpfile();
    utf16_belge_ac(girdi);
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"id\":12,\"method\":\"textDocument/definition\","
        "\"params\":{\"textDocument\":{\"uri\":\"file:///t.kem\"},"
        "\"position\":{\"line\":1,\"character\":28}}}");
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
        int sl = -1, sc = -1, el = -1, ec = -1;
        ok = result && result->tip != JSON_NULL
          && range_oku(json_alan(result, "range"), &sl, &sc, &el, &ec)
          && sl == 0 && sc == 0 && el == 0 && ec == 3;
        if (!ok) printf("      (olculen: line=%d char=%d..%d)\n", sl, sc, ec);
        arena_serbest(a);
    }
    test_sonuc("definition: tanim araligi UTF-16 (0..3)", ok);
    yanitlari_serbest(y);
    fclose(girdi);
    fclose(cikti);
}

static void test_utf16_references_turkce(void) {
    FILE *girdi = tmpfile();
    FILE *cikti = tmpfile();
    utf16_belge_ac(girdi);
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"id\":13,\"method\":\"textDocument/references\","
        "\"params\":{\"textDocument\":{\"uri\":\"file:///t.kem\"},"
        "\"position\":{\"line\":1,\"character\":28},"
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
        ok = result && json_dizi_sayi(result) == 2;
        if (ok) {
            /* Kullanim konumu (satir 1) listede olmali: 28..31 */
            int bulundu = 0;
            for (int i = 0; i < 2; i++) {
                int sl, sc, el, ec;
                if (!range_oku(json_alan(json_dizi_eleman(result, i), "range"),
                               &sl, &sc, &el, &ec)) continue;
                if (sl == 1 && sc == 28 && ec == 31) bulundu = 1;
                /* Tanim: dugum konumu = `işlev` sutunu 0, uzunluk 3 birim */
                if (sl == 0 && (sc != 0 || ec != 3)) ok = 0;
            }
            ok = ok && bulundu;
        }
        arena_serbest(a);
    }
    test_sonuc("references: kullanim+tanim araliklari UTF-16", ok);
    yanitlari_serbest(y);
    fclose(girdi);
    fclose(cikti);
}

/* documentSymbol: "yapı Ölçüm { x_ölçü: tam32; }"
 *   yapi adi  : dugum konumu = `yapı` sutunu 0, ad 5 birim -> 0..5
 *   alan adi  : UTF-16 sutun 13, 6 birim -> 13..19   (bayt: 17..26) */
static void test_utf16_documentsymbol_turkce(void) {
    FILE *girdi = tmpfile();
    FILE *cikti = tmpfile();
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}");
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///t.kem\",\"languageId\":\"kemgu\","
        "\"version\":1,\"text\":"
        "\"yap\\u0131 \\u00d6l\\u00e7\\u00fcm { x_\\u00f6l\\u00e7\\u00fc: tam32; }\""
        "}}}");
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"id\":14,\"method\":\"textDocument/documentSymbol\","
        "\"params\":{\"textDocument\":{\"uri\":\"file:///t.kem\"}}}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"shutdown\"}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"method\":\"exit\"}");
    rewind(girdi);

    lsp_server_calistir(girdi, cikti);
    LspYanit *y = yanitlari_oku(cikti);
    int ok = yanit_sayisi(y) == 4;
    if (ok) {
        Arena *a = arena_olustur(0);
        LspYanit *ys = yanit_n(y, 2);
        JsonDeger *j = json_ayrist(a, ys->govde, ys->content_length, NULL);
        JsonDeger *result = json_alan(j, "result");
        ok = result && json_dizi_sayi(result) == 1;
        if (ok) {
            int sl, sc, el, ec;
            JsonDeger *s0 = json_dizi_eleman(result, 0);
            ok = range_oku(json_alan(s0, "selectionRange"), &sl, &sc, &el, &ec)
              && sl == 0 && sc == 0 && ec == 5;
            if (!ok) printf("      (yapi olculen: line=%d char=%d..%d)\n",
                            sl, sc, ec);
            JsonDeger *ch = json_alan(s0, "children");
            if (ok) ok = json_dizi_sayi(ch) == 1;
            if (ok) {
                ok = range_oku(json_alan(json_dizi_eleman(ch, 0),
                                          "selectionRange"),
                               &sl, &sc, &el, &ec)
                  && sl == 0 && sc == 13 && ec == 19;
                if (!ok) printf("      (alan olculen: line=%d char=%d..%d)\n",
                                sl, sc, ec);
            }
        }
        arena_serbest(a);
    }
    test_sonuc("documentSymbol: yap\xc4\xb1+alan araliklari UTF-16", ok);
    yanitlari_serbest(y);
    fclose(girdi);
    fclose(cikti);
}

static void test_utf16_diagnostic_turkce(void) {
    FILE *girdi = tmpfile();
    FILE *cikti = tmpfile();
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}");
    /* "işlev main() -> tam32 { ver şey; }" — 'şey' tanimsiz.
     * 'ş' harfi UTF-16 sutun 28'de, bayt sutunu 29'da. */
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///t.kem\",\"languageId\":\"kemgu\","
        "\"version\":1,\"text\":"
        "\"i\\u015flev main() -> tam32 { ver \\u015fey; }\"}}}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"shutdown\"}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"method\":\"exit\"}");
    rewind(girdi);

    lsp_server_calistir(girdi, cikti);
    LspYanit *y = yanitlari_oku(cikti);
    int ok = yanit_sayisi(y) == 3;
    if (ok) {
        Arena *a = arena_olustur(0);
        LspYanit *yd = yanit_n(y, 1);
        JsonDeger *j = json_ayrist(a, yd->govde, yd->content_length, NULL);
        JsonDeger *diags = json_alan(json_alan(j, "params"), "diagnostics");
        ok = json_dizi_sayi(diags) > 0;
        if (ok) {
            int sl, sc, el, ec;
            ok = range_oku(json_alan(json_dizi_eleman(diags, 0), "range"),
                           &sl, &sc, &el, &ec)
              && sl == 0 && sc == 28;
            if (!ok) printf("      (olculen: line=%d char=%d..%d)\n", sl, sc, ec);
        }
        arena_serbest(a);
    }
    test_sonuc("diagnostic: hata sutunu UTF-16 (28)", ok);
    yanitlari_serbest(y);
    fclose(girdi);
    fclose(cikti);
}

/* BMP disi karakter (emoji, U+1F600 = surrogate cifti = 2 UTF-16 birim).
 * JSON'a ham UTF-8 bayt olarak gomulur — json.c `\uXXXX` surrogate ciftlerini
 * BIRLESTIRMEZ (olculdu), o yuzden kacis dizisi degil ham bayt kullaniyoruz.
 *
 * Belge: sabit S: metin = "<emoji>"; işlev main() -> tam32 { ver şey; }
 * Emoji oncesi 22 UTF-16 birim; emoji 2 birim; sonra "; işlev main() ..." */
static void test_utf16_bmp_disi_emoji(void) {
    FILE *girdi = tmpfile();
    FILE *cikti = tmpfile();
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}");
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///t.kem\",\"languageId\":\"kemgu\","
        "\"version\":1,\"text\":"
        "\"sabit S: metin = \\\"\xf0\x9f\x98\x80\\\"; "
        "i\\u015flev main() -> tam32 { ver \\u015fey; }\"}}}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"shutdown\"}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"method\":\"exit\"}");
    rewind(girdi);

    lsp_server_calistir(girdi, cikti);
    LspYanit *y = yanitlari_oku(cikti);
    int ok = yanit_sayisi(y) == 3;
    if (ok) {
        Arena *a = arena_olustur(0);
        LspYanit *yd = yanit_n(y, 1);
        JsonDeger *j = json_ayrist(a, yd->govde, yd->content_length, NULL);
        JsonDeger *diags = json_alan(json_alan(j, "params"), "diagnostics");
        ok = json_dizi_sayi(diags) > 0;
        if (ok) {
            int sl, sc, el, ec;
            /* Beklenen: emoji 2 UTF-16 birim sayilir.
             * 'sabit S: metin = "' = 18 birim, emoji 2 -> 20, '"; ' -> 23,
             * 'işlev main() -> tam32 { ver ' 28 birim -> 51. */
            ok = range_oku(json_alan(json_dizi_eleman(diags, 0), "range"),
                           &sl, &sc, &el, &ec)
              && sl == 0 && sc == 51;
            if (!ok) printf("      (olculen: line=%d char=%d..%d)\n", sl, sc, ec);
        }
        arena_serbest(a);
    }
    test_sonuc("diagnostic: BMP disi emoji = 2 UTF-16 birim", ok);
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

/* === D-432: semanticTokens/full ===
 * TURKCE KAYNAK BILINCLI: LSP `data` icindeki sutun ve uzunluk UTF-16 KOD
 * BIRIMI cinsindendir, BAYT degil. `olc` 5 BAYT ama 3 UTF-16 birimidir —
 * bayt kullanan bir uygulama burada kayar ve ASCII testte GORUNMEZ. */
static void test_semantictokens_turkce(void) {
    FILE *girdi = tmpfile();
    FILE *cikti = tmpfile();
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}");
    /* Kaynak: `işlev ölç(x: tam32) -> tam32 { ver x + 42; }` */
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///s.kem\",\"languageId\":\"kemgu\","
        "\"version\":1,\"text\":"
        "\"i\\u015flev \\u00f6l\\u00e7(x: tam32) -> tam32 { ver x + 42; }\"}}}");
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"id\":20,"
        "\"method\":\"textDocument/semanticTokens/full\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///s.kem\"}}}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"shutdown\"}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"method\":\"exit\"}");
    rewind(girdi);

    lsp_server_calistir(girdi, cikti);
    LspYanit *y = yanitlari_oku(cikti);
    int ok = yanit_sayisi(y) == 4;   /* initialize, diag, semtok, shutdown */
    if (ok) {
        Arena *a = arena_olustur(0);
        LspYanit *ys = yanit_n(y, 2);
        JsonDeger *j = json_ayrist(a, ys->govde, ys->content_length, NULL);
        JsonDeger *result = json_alan(j, "result");
        JsonDeger *data = result ? json_alan(result, "data") : NULL;
        ok = data && data->tip == JSON_DIZI && data->veri.dizi.sayi >= 15
          && (data->veri.dizi.sayi % 5) == 0;   /* 5'li gruplar */
        if (ok) {
            JsonDeger **e = data->veri.dizi.elemanlar;
            /* [0] `işlev` : satir 0, sutun 0, uzunluk 5, keyword(0) */
            ok = e[0]->veri.tamsayi == 0 && e[1]->veri.tamsayi == 0
              && e[2]->veri.tamsayi == 5 && e[3]->veri.tamsayi == 0;
            /* [1] `ölç` : ayni satir, +6 sutun, uzunluk 3 (UTF-16!), variable(4)
             * ⚠ BAYT sayilsaydi uzunluk 5 olurdu — kapinin ASIL disi budur. */
            if (ok) {
                ok = e[5]->veri.tamsayi == 0 && e[6]->veri.tamsayi == 6
                  && e[7]->veri.tamsayi == 3 && e[8]->veri.tamsayi == 4;
            }
        }
        arena_serbest(a);
    }
    test_sonuc("semanticTokens UTF-16 sutun/uzunluk dogru", ok);
    yanitlari_serbest(y);
    fclose(girdi);
    fclose(cikti);
}

/* === D-433: COK-BELGE ===
 * Sunucu tek `Belge` tutuyordu: IKINCI dosya acilinca BIRINCISI eziliyordu.
 * Bu test iki dosyayi acar, sonra BIRINCISINE documentSymbol sorar — tek-belge
 * modelde birinci dosyanin sembolleri KAYBOLMUS olurdu (bos dizi donerdi). */
static void test_cok_belge_izolasyon(void) {
    FILE *girdi = tmpfile();
    FILE *cikti = tmpfile();
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}");
    /* a.kem: `işlev alfa() -> tam32 { ver 1; }` */
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///a.kem\",\"languageId\":\"kemgu\","
        "\"version\":1,\"text\":\"i\\u015flev alfa() -> tam32 { ver 1; }\"}}}");
    /* b.kem: `işlev beta() -> tam32 { ver 2; }` — a.kem'i EZMEMELI */
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///b.kem\",\"languageId\":\"kemgu\","
        "\"version\":1,\"text\":\"i\\u015flev beta() -> tam32 { ver 2; }\"}}}");
    /* BIRINCI dosyanin sembolleri hala erisilebilir mi? */
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"id\":30,\"method\":\"textDocument/documentSymbol\","
        "\"params\":{\"textDocument\":{\"uri\":\"file:///a.kem\"}}}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"shutdown\"}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"method\":\"exit\"}");
    rewind(girdi);

    lsp_server_calistir(girdi, cikti);
    LspYanit *y = yanitlari_oku(cikti);
    /* initialize, diag(a), diag(b), documentSymbol, shutdown */
    int ok = yanit_sayisi(y) == 5;
    if (ok) {
        Arena *a = arena_olustur(0);
        LspYanit *yd = yanit_n(y, 3);
        JsonDeger *j = json_ayrist(a, yd->govde, yd->content_length, NULL);
        JsonDeger *result = json_alan(j, "result");
        ok = result && result->tip == JSON_DIZI && result->veri.dizi.sayi == 1;
        if (ok) {
            JsonDeger *s0 = result->veri.dizi.elemanlar[0];
            int nu = 0;
            const char *nm = json_metin(json_alan(s0, "name"), &nu);
            /* a.kem'in sembolu `alfa` olmali — `beta` ise belge EZILMIS demektir */
            ok = nm && nu == 4 && memcmp(nm, "alfa", 4) == 0;
        }
        arena_serbest(a);
    }
    test_sonuc("cok-belge: ikinci didOpen birinciyi EZMEZ", ok);
    yanitlari_serbest(y);
    fclose(girdi);
    fclose(cikti);
}

/* === D-434: workspace/symbol ===
 * ASIL KANIT: sorgu IKI FARKLI dosyadan sonuc dondurmeli. Tek-belge modelde
 * (D-433 oncesi) bu IMKANSIZDI — o yuzden bu test ayni zamanda cok-belge
 * modelinin ISE YARADIGININ ikinci kanitidir. */
static void test_workspace_symbol_capraz(void) {
    FILE *girdi = tmpfile();
    FILE *cikti = tmpfile();
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}");
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///a.kem\",\"languageId\":\"kemgu\","
        "\"version\":1,\"text\":\"i\\u015flev ortakbir() -> tam32 { ver 1; }\"}}}");
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///b.kem\",\"languageId\":\"kemgu\","
        "\"version\":1,\"text\":\"i\\u015flev ortakiki() -> tam32 { ver 2; }\"}}}");
    /* "ortak" IKI dosyada da gecer -> 2 sonuc, FARKLI uri'lerle */
    mesaj_yaz(girdi,
        "{\"jsonrpc\":\"2.0\",\"id\":40,\"method\":\"workspace/symbol\","
        "\"params\":{\"query\":\"ortak\"}}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"shutdown\"}");
    mesaj_yaz(girdi, "{\"jsonrpc\":\"2.0\",\"method\":\"exit\"}");
    rewind(girdi);

    lsp_server_calistir(girdi, cikti);
    LspYanit *y = yanitlari_oku(cikti);
    int ok = yanit_sayisi(y) == 5;   /* init, diag(a), diag(b), ws, shutdown */
    if (ok) {
        Arena *a = arena_olustur(0);
        LspYanit *yw = yanit_n(y, 3);
        JsonDeger *j = json_ayrist(a, yw->govde, yw->content_length, NULL);
        JsonDeger *r = json_alan(j, "result");
        ok = r && r->tip == JSON_DIZI && r->veri.dizi.sayi == 2;
        if (ok) {
            /* IKI FARKLI uri gelmeli — ayni uri iki kez ise capraz arama YOK */
            int u0 = 0, u1 = 0;
            JsonDeger *l0 = json_alan(r->veri.dizi.elemanlar[0], "location");
            JsonDeger *l1 = json_alan(r->veri.dizi.elemanlar[1], "location");
            const char *s0 = l0 ? json_metin(json_alan(l0, "uri"), &u0) : NULL;
            const char *s1 = l1 ? json_metin(json_alan(l1, "uri"), &u1) : NULL;
            ok = s0 && s1 && !(u0 == u1 && memcmp(s0, s1, (size_t)u0) == 0);
            /* SymbolKind: islev = 12 (CompletionItemKind 3 DEGIL) */
            if (ok) {
                JsonDeger *k = json_alan(r->veri.dizi.elemanlar[0], "kind");
                ok = k && k->tip == JSON_TAMSAYI && k->veri.tamsayi == 12;
            }
        }
        arena_serbest(a);
    }
    test_sonuc("workspace/symbol: IKI dosyadan sonuc + SymbolKind", ok);
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

    printf("\n--- LSP v3 (semanticTokens) ---\n");
    test_semantictokens_turkce();

    printf("\n--- LSP v3 (cok-belge) ---\n");
    test_cok_belge_izolasyon();

    printf("\n--- LSP v3 (workspace/symbol) ---\n");
    test_workspace_symbol_capraz();

    printf("\n--- UTF-16 konum donusumu ---\n");
    test_utf16_hover_turkce();
    test_utf16_definition_turkce();
    test_utf16_references_turkce();
    test_utf16_documentsymbol_turkce();
    test_utf16_diagnostic_turkce();
    test_utf16_bmp_disi_emoji();

    printf("\n==============================\n");
    printf("Toplam: %d | Basarili: %d | Basarisiz: %d\n",
           toplam_test, basarili, basarisiz);
    return basarisiz > 0 ? 1 : 0;
}
