#ifndef KEMGU_LSP_H
#define KEMGU_LSP_H

#include <stdio.h>

/*
 * KEMGU LSP Server MVP
 * =====================
 *
 * Language Server Protocol uygulamasi (stdio uzerinden JSON-RPC 2.0).
 *
 * Mesaj cercevesi:
 *   Content-Length: NN\r\n
 *   \r\n
 *   {jsonrpc:"2.0",method:"...",params:{...}}
 *
 * Su an desteklenen mesajlar:
 *   - initialize (request)    -> capabilities response
 *   - initialized (notify)    -> no-op
 *   - textDocument/didOpen    -> diagnostic publish
 *   - textDocument/didChange  -> re-parse + diagnostic publish
 *   - textDocument/didClose   -> diagnostic temizle
 *   - shutdown (request)      -> bos response
 *   - exit (notify)           -> server cik
 *
 * Diagnostics: parser hatalari + tip kontrol hatalari LSP diagnostic'lerine
 * cevrilir ve textDocument/publishDiagnostics ile gonderilir.
 *
 * Sinirlamalar (v1):
 *   - Tek dosya hafiza (didChange tum metni yenidiyor — incremental yok)
 *   - Hover, completion, definition yok
 *   - workspace mesajlari yok
 */

/* Server loop'u baslat. stdin'den oku, stdout'a yaz. Hatalari stderr.
 * Donus: 0 normal cikis (exit notify), nonzero hata. */
int lsp_server_calistir(FILE *girdi, FILE *cikti);

#endif /* KEMGU_LSP_H */
