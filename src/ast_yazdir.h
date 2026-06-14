#ifndef KEMGU_AST_YAZDIR_H
#define KEMGU_AST_YAZDIR_H

#include "ast.h"

#include <stdio.h>

/*
 * AST Debug Yazdiricisi
 * =====================
 *
 * Recursive AST -> okunabilir indent metin. Her dugum:
 *   <NODE_TYPE> [ozel bilgi] <satir>:<sutun>
 *     <cocuk_node>
 *       ...
 *
 * NULL dugum guvenlidir (no-op). Cikti FILE* (stdout, stderr, dosya, vs.).
 */

/* Bir dugumu (alt agacla birlikte) yazdir. */
void ast_yazdir(const Dugum *dugum, FILE *cikti);

/* Belirli bir derinlikten basla (ic kullanim ama testler de cagirabilir). */
void ast_yazdir_indent(const Dugum *dugum, FILE *cikti, int derinlik);

/* DÜZ (flat) dump — SELF-HOST parser diff-oracle (D-043).
 * Format (preorder): <derinlik>\t<TIP_ADI>\t<deger>\t<satir>\t<sutun>\n
 * Derinlik-etiketli preorder ağacı birebir belirler. KEMGU-parser aynı çıktıyı
 * üretir → diff = doğruluk. TÜM düğüm tipleri + çocukları gezilir. */
void ast_duz_yaz(const Dugum *dugum, FILE *cikti, int derinlik);

#endif /* KEMGU_AST_YAZDIR_H */
