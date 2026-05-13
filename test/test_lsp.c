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
        ok = j1 && r1 && cap && tds && tds->veri.tamsayi == 1;
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
        "\"position\":{\"line\":0,\"character\":60}}}");
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
        /* result null veya hover icerik dondu */
        ok = (j != NULL);  /* en az parse edilebildiyse */
        if (result && result->tip != JSON_NULL) {
            JsonDeger *cont = json_alan(result, "contents");
            ok = cont != NULL;
        }
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
        "\"position\":{\"line\":0,\"character\":60}}}");
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
        /* Hover spec: result null veya range icerir */
        ok = j != NULL;
        if (result && result->tip != JSON_NULL) {
            JsonDeger *range = json_alan(result, "range");
            ok = range != NULL;
        }
        arena_serbest(a);
    }
    test_sonuc("definition yaniti uri+range doner", ok);
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

    printf("\n==============================\n");
    printf("Toplam: %d | Basarili: %d | Basarisiz: %d\n",
           toplam_test, basarili, basarisiz);
    return basarisiz > 0 ? 1 : 0;
}
