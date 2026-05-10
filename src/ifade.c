#include "parser.h"

#include <string.h>
#include <stdlib.h>
#include <stdint.h>

/*
 * KEMGU Ifade Parser (Pratt — ADIM 9'da tam implementasyon)
 * ===========================================================
 *
 * MINIMAL stub (ADIM 8.2): sadece birincil ifadeler:
 *   - Tum literaller (TAM, ONDALIK, METIN, KARAKTER, dogru/yanlis/bos)
 *   - Tanimlayici
 *   - Parantezli ifade (ic_recursive)
 *
 * Operator parsing (+, -, *, /, ==, vb.) ADIM 9'da Pratt parser ile.
 * Tip parser de minimal: sadece basit tip (tanimlayici).
 */

/* === Sayi parse yardimcilari === */

/* Token metnini buffer'a kopyala, underscore'lari at. Buffer null-term. */
static int sayi_tokeni_temizle(const char *kaynak, int uzunluk,
                                char *buf, int buf_boyut) {
    int j = 0;
    int n = uzunluk < buf_boyut - 1 ? uzunluk : buf_boyut - 1;
    for (int i = 0; i < n; i++) {
        if (kaynak[i] != '_') buf[j++] = kaynak[i];
    }
    buf[j] = '\0';
    return j;
}

/* === Birincil ifade === */

static Dugum *parse_birincil(Parser *p) {
    Token t = parser_simdiki(p);
    Dugum *d = NULL;

    switch (t.tip) {
        case TOK_TAMSAYI: {
            char tampon[64];
            int j = sayi_tokeni_temizle(t.baslangic, t.uzunluk,
                                         tampon, sizeof(tampon));
            int64_t deger = 0;
            if (j >= 2 && tampon[0] == '0' &&
                (tampon[1] == 'x' || tampon[1] == 'X')) {
                deger = (int64_t)strtoll(tampon + 2, NULL, 16);
            } else if (j >= 2 && tampon[0] == '0' &&
                       (tampon[1] == 'b' || tampon[1] == 'B')) {
                deger = (int64_t)strtoll(tampon + 2, NULL, 2);
            } else if (j >= 2 && tampon[0] == '0' &&
                       (tampon[1] == 'o' || tampon[1] == 'O')) {
                deger = (int64_t)strtoll(tampon + 2, NULL, 8);
            } else {
                deger = (int64_t)strtoll(tampon, NULL, 10);
            }
            d = dugum_tam(p->arena, deger, t.satir, t.sutun);
            parser_ilerle(p);
            break;
        }

        case TOK_ONDALIK: {
            char tampon[64];
            sayi_tokeni_temizle(t.baslangic, t.uzunluk,
                                tampon, sizeof(tampon));
            double deger = strtod(tampon, NULL);
            d = dugum_kesirli(p->arena, deger, t.satir, t.sutun);
            parser_ilerle(p);
            break;
        }

        case TOK_METIN: {
            /* Tirnaklar haric, kacis isleme ADIM 9/10'da */
            int ic_uz = t.uzunluk - 2;
            if (ic_uz < 0) ic_uz = 0;
            d = dugum_metin(p->arena, t.baslangic + 1, ic_uz,
                            t.satir, t.sutun);
            parser_ilerle(p);
            break;
        }

        case TOK_KARAKTER: {
            /* Basit: ilk byte kod noktasi, kacis isleme ADIM 9/10'da */
            uint32_t kp = 0;
            if (t.uzunluk >= 3) {
                kp = (unsigned char)t.baslangic[1];
            }
            d = dugum_karakter(p->arena, kp, t.satir, t.sutun);
            parser_ilerle(p);
            break;
        }

        case TOK_DOGRU:
            d = dugum_mantiksal(p->arena, 1, t.satir, t.sutun);
            parser_ilerle(p);
            break;

        case TOK_YANLIS:
            d = dugum_mantiksal(p->arena, 0, t.satir, t.sutun);
            parser_ilerle(p);
            break;

        case TOK_BOS:
            d = dugum_bos(p->arena, t.satir, t.sutun);
            parser_ilerle(p);
            break;

        case TOK_TANIMLAYICI:
            d = dugum_tanimlayici(p->arena, t.baslangic, t.uzunluk,
                                  t.satir, t.sutun);
            parser_ilerle(p);
            break;

        case TOK_SOL_PAREN:
            parser_ilerle(p);
            d = parse_ifade(p);
            parser_bekle(p, TOK_SAG_PAREN, "P110", "')' bekleniyor");
            break;

        default:
            parser_hata(p, t, "P010", "ifade bekleniyor", NULL);
            d = dugum_hata(p->arena, t.satir, t.sutun);
            /* Tuketme — caller karar versin (panik mod gerekirse) */
            break;
    }

    return d;
}

/* === Public API === */

Dugum *parse_ifade(Parser *p) {
    /* ADIM 8.2 MINIMAL: sadece birincil. ADIM 9'da Pratt parser. */
    return parse_birincil(p);
}

Dugum *parse_tip(Parser *p) {
    /* ADIM 8.2 MINIMAL: sadece basit tip (tanimlayici).
     * Karmasik tipler (Dizi<T>, secimlik<T>, sonuc<T,H>, &T, *T) ADIM 10'da. */
    Token t = parser_simdiki(p);
    if (t.tip != TOK_TANIMLAYICI) {
        parser_hata(p, t, "P011", "tip bekleniyor (tanimlayici)", NULL);
        return dugum_hata(p->arena, t.satir, t.sutun);
    }
    Dugum *d = dugum_olustur(p->arena, DUGUM_TIP_BASIT, t.satir, t.sutun);
    if (!d) return NULL;
    d->veri.tip_basit.ad =
        ast_string_kopyala(p->arena, t.baslangic, t.uzunluk);
    d->veri.tip_basit.ad_uzunluk = t.uzunluk;
    parser_ilerle(p);
    return d;
}
