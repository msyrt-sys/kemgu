#ifndef KEMGU_PARSER_H
#define KEMGU_PARSER_H

#include "lexer.h"
#include "ast.h"
#include "arena.h"

/*
 * KEMGU Parser
 * ============
 *
 * Hibrit parser:
 *   - Recursive descent: ust duzey, tanimlar, deyimler (parser.c)
 *   - Pratt: ifadeler (ifade.c)
 *
 * Hata kurtarma: panik mod sync points (`;`, `}`, ust duzey keyword'leri).
 * Maksimum hata: PARSER_MAX_HATA — bunu asinca parser durur.
 *
 * Tum tahsisler arena'dan (parse-zamani linked list -> arena'da array kopya).
 */

#define PARSER_MAX_HATA 100

typedef struct Parser {
    Lexer *lexer;
    Token simdiki;            /* henuz tuketilmemis token */
    Token sonraki;            /* 2-token lookahead (lazy) */
    int sonraki_var;          /* sonraki dolu mu */
    Arena *arena;
    int hata_sayisi;
    const char *dosya_adi;
    const char *kaynak;
} Parser;

/* === Public API === */

void parser_baslat(Parser *p, Lexer *l, Arena *a,
                   const char *dosya_adi, const char *kaynak);

/* DUGUM_PROGRAM doner. Hata olsa bile gecerli AST doner.
 * Hata sayisi icin p->hata_sayisi'na bak. */
Dugum *parser_calistir(Parser *p);

/* === Token akisi yardimcilari (parser.c, ifade.c paylasir) === */

Token parser_simdiki(const Parser *p);
Token parser_onizle(Parser *p);          /* 2-token lookahead */
void  parser_ilerle(Parser *p);
int   parser_eslesir(const Parser *p, TokenTipi t);
int   parser_tuket(Parser *p, TokenTipi t);
Token parser_bekle(Parser *p, TokenTipi t,
                   const char *kod, const char *mesaj);

void parser_hata(Parser *p, Token tok,
                 const char *kod, const char *mesaj, const char *ipucu);
void parser_panik_sync(Parser *p);

/* === Ifade ve tip parser (ifade.c'de implement) === */

Dugum *parse_ifade(Parser *p);    /* Pratt parser - ADIM 8.2'de minimal */
Dugum *parse_tip(Parser *p);      /* tip context — ADIM 8.2'de basit */

#endif /* KEMGU_PARSER_H */
