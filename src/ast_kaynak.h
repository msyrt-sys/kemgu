#ifndef KEMGU_AST_KAYNAK_H
#define KEMGU_AST_KAYNAK_H

#include "ast.h"

#include <stdio.h>

/*
 * KEMGU Source-Equivalent Pretty Printer
 * ========================================
 *
 * AST'yi KEMGU sözdizimine geri yazar — debug printer (ast_yazdir.c)
 * tree gösterirken bu source-equivalent çıktı verir.
 *
 * Kullanım:
 *   - Snapshot testlerinde "reformatted" karşılaştırma
 *   - LSP'de "format" komutu için
 *   - AST manipulation'dan sonra kod regenerasyon
 *
 * Garanti: ast_kaynak_yaz(parse(x)) parse edilirse x'le eşdeğer AST'yi
 * üretir (round-trip).
 *
 * Garanti edilmez: whitespace, yorum, formatting tercih (bunlar AST'de
 * saklanmıyor). Yorum koruma istense source map ile yapılır.
 */

/* AST'yi out'a KEMGU sözdiziminde yazdır. NULL güvenli. */
void ast_kaynak_yaz(const Dugum *d, FILE *out);

/* Aynı şey ama indent kontrolüyle (recursive). */
void ast_kaynak_yaz_indent(const Dugum *d, FILE *out, int derinlik);

#endif /* KEMGU_AST_KAYNAK_H */
