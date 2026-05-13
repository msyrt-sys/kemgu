#include "parser.h"

#include <string.h>
#include <stdlib.h>
#include <stdint.h>

/*
 * KEMGU Ifade Parser (Pratt)
 * ===========================
 *
 * Oncelik tablosu (CLAUDE.md ile birebir, bit op'lar dahil):
 *    1: veya                (sol)  — logical OR
 *    2: ve                  (sol)  — logical AND
 *    3: |                   (sol)  — bit OR
 *    4: ^                   (sol)  — bit XOR
 *    5: &                   (sol)  — bit AND
 *    6: == !=               (sol)  — equality
 *    7: < > <= >=           (sol)  — comparison
 *    8: << >>               (sol)  — shift
 *    9: + -                 (sol)  — additive
 *   10: * / %               (sol)  — multiplicative
 *   11: degil - ~ & * (onek) (sag) — prefix
 *   12: . [] () ::          (sol — sonek)
 *
 * Birincil ifadeler:
 *   - Literaller (TAM, ONDALIK, METIN, KARAKTER, dogru/yanlis/bos)
 *   - Tanimlayici (sonra '{' gelirse YAPI_OLUSTUR)
 *   - Parantezli ifade
 *   - Dizi olusturma [...]
 *   - Lambda |params| body
 */

/* === Onek (prefix) ileri tanim === */

static Dugum *parse_onek(Parser *p);
static Dugum *parse_oncelik(Parser *p, int min_oncelik);
static Dugum *parse_birincil(Parser *p);

/* === Sayi parse yardimcilari === */

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

/* === Oncelik tablosu === */

typedef enum {
    ONC_YOK         = 0,
    ONC_VEYA        = 1,
    ONC_VE          = 2,
    ONC_BIT_VEYA    = 3,
    ONC_BIT_OZVEYA  = 4,
    ONC_BIT_VE      = 5,
    ONC_ESITLIK     = 6,
    ONC_KARSILASTIR = 7,
    ONC_KAYDIR      = 8,
    ONC_TOPLAMA     = 9,
    ONC_CARPMA      = 10,
    ONC_ONEK        = 11,
    ONC_SONEK       = 12,
} Oncelik;

static int ikili_oncelik(TokenTipi t) {
    switch (t) {
        case TOK_VEYA:                                          return ONC_VEYA;
        case TOK_VE:                                            return ONC_VE;
        case TOK_VEYA_BIT:                                      return ONC_BIT_VEYA;
        case TOK_OZVEYA_BIT:                                    return ONC_BIT_OZVEYA;
        case TOK_VE_BIT:                                        return ONC_BIT_VE;
        case TOK_ESIT_ESIT:    case TOK_ESIT_DEGIL:             return ONC_ESITLIK;
        case TOK_KUCUK:        case TOK_BUYUK:
        case TOK_KUCUK_ESIT:   case TOK_BUYUK_ESIT:             return ONC_KARSILASTIR;
        case TOK_SOLA_KAYDIR:  case TOK_SAGA_KAYDIR:            return ONC_KAYDIR;
        case TOK_ARTI:         case TOK_EKSI:                   return ONC_TOPLAMA;
        case TOK_YILDIZ:       case TOK_BOLU:    case TOK_MOD:  return ONC_CARPMA;
        default:                                                return ONC_YOK;
    }
}

static int sonek_oncelik(TokenTipi t) {
    switch (t) {
        case TOK_NOKTA:
        case TOK_SOL_KOSELI:
        case TOK_SOL_PAREN:
        case TOK_CIFT_IKI_NOKTA:
        case TOK_OLARAK:        /* Kirmizi E: explicit cast */
            return ONC_SONEK;
        default:
            return ONC_YOK;
    }
}

static Operator token_ikili_op(TokenTipi t) {
    switch (t) {
        case TOK_VEYA:        return OP_VEYA;
        case TOK_VE:          return OP_VE;
        case TOK_VEYA_BIT:    return OP_BIT_VEYA;
        case TOK_OZVEYA_BIT:  return OP_BIT_OZVEYA;
        case TOK_VE_BIT:      return OP_BIT_VE;
        case TOK_ESIT_ESIT:   return OP_ESIT;
        case TOK_ESIT_DEGIL:  return OP_ESIT_DEGIL;
        case TOK_KUCUK:       return OP_KUCUK;
        case TOK_BUYUK:       return OP_BUYUK;
        case TOK_KUCUK_ESIT:  return OP_KUCUK_ESIT;
        case TOK_BUYUK_ESIT:  return OP_BUYUK_ESIT;
        case TOK_SOLA_KAYDIR: return OP_SOLA_KAYDIR;
        case TOK_SAGA_KAYDIR: return OP_SAGA_KAYDIR;
        case TOK_ARTI:        return OP_ARTI;
        case TOK_EKSI:        return OP_EKSI;
        case TOK_YILDIZ:      return OP_CARPI;
        case TOK_BOLU:        return OP_BOLU;
        case TOK_MOD:         return OP_MOD;
        default:              return OP_ARTI;  /* unreachable */
    }
}

/* === Yardimci olusturucular === */

static Dugum *yapi_dugum_olustur(Parser *p, DugumTipi tip, int satir, int sutun) {
    return dugum_olustur(p->arena, tip, satir, sutun);
}

/* === Yapi olusturma: TipAdi { alan: ifade, ... } === */

static Dugum *parse_yapi_olusturma(Parser *p, Dugum *tip_id) {
    /* tip_id parse edildi (DUGUM_TANIMLAYICI). Su an '{' tuketilecek. */
    parser_ilerle(p);  /* '{' tuket */

    Liste alanlar;
    liste_baslat(&alanlar);

    if (!parser_eslesir(p, TOK_SAG_SUSLU)) {
        do {
            /* Trailing comma destegi: ',' sonrasi '}' gorursek dur */
            if (parser_eslesir(p, TOK_SAG_SUSLU)) break;

            Token ad_tok = parser_bekle(p, TOK_TANIMLAYICI, "P130",
                                        "alan adi bekleniyor");
            parser_bekle(p, TOK_IKI_NOKTA, "P131",
                         "alan icin ':' bekleniyor");
            Dugum *deger = parse_ifade(p);

            Dugum *aa = yapi_dugum_olustur(p, DUGUM_ALAN_ATAMA,
                                           ad_tok.satir, ad_tok.sutun);
            if (aa) {
                aa->veri.alan_atama.ad =
                    ast_string_kopyala(p->arena, ad_tok.baslangic, ad_tok.uzunluk);
                aa->veri.alan_atama.ad_uzunluk = ad_tok.uzunluk;
                aa->veri.alan_atama.deger = deger;
                liste_ekle(&alanlar, p->arena, aa);
            }
        } while (parser_tuket(p, TOK_VIRGUL));
    }
    parser_bekle(p, TOK_SAG_SUSLU, "P132",
                 "yapi olusturmasinda '}' bekleniyor");

    Dugum *yo = yapi_dugum_olustur(p, DUGUM_YAPI_OLUSTUR,
                                   tip_id->satir, tip_id->sutun);
    if (yo) {
        yo->veri.yapi_olustur.tip_ad = tip_id->veri.tanimlayici.metin;
        yo->veri.yapi_olustur.tip_ad_uzunluk = tip_id->veri.tanimlayici.uzunluk;
        yo->veri.yapi_olustur.alanlar = liste_array_yap(&alanlar, p->arena);
        yo->veri.yapi_olustur.alan_sayi = alanlar.sayi;
    }
    return yo;
}

/* === Dizi olusturma: [e1, e2, ..., en,?] === */

static Dugum *parse_dizi_olusturma(Parser *p) {
    Token tok = parser_simdiki(p);
    parser_ilerle(p);  /* '[' tuket */

    Liste elemanlar;
    liste_baslat(&elemanlar);

    if (!parser_eslesir(p, TOK_SAG_KOSELI)) {
        do {
            if (parser_eslesir(p, TOK_SAG_KOSELI)) break;  /* trailing comma */
            Dugum *e = parse_ifade(p);
            liste_ekle(&elemanlar, p->arena, e);
        } while (parser_tuket(p, TOK_VIRGUL));
    }
    parser_bekle(p, TOK_SAG_KOSELI, "P150",
                 "dizide ']' bekleniyor");

    Dugum *d = yapi_dugum_olustur(p, DUGUM_DIZI_OLUSTUR, tok.satir, tok.sutun);
    if (d) {
        d->veri.dizi_olustur.elemanlar = liste_array_yap(&elemanlar, p->arena);
        d->veri.dizi_olustur.sayi = elemanlar.sayi;
    }
    return d;
}

/* === Lambda: |a: tip, b: tip| ifade  veya  |...| { blok } === */

static Dugum *parse_lambda(Parser *p) {
    Token bar_tok = parser_simdiki(p);
    parser_ilerle(p);  /* ilk '|' tuket */

    Liste params;
    liste_baslat(&params);

    if (!parser_eslesir(p, TOK_VEYA_BIT)) {
        do {
            Dugum *param = parse_parametre(p);
            liste_ekle(&params, p->arena, param);
        } while (parser_tuket(p, TOK_VIRGUL));
    }
    parser_bekle(p, TOK_VEYA_BIT, "P140",
                 "lambda kapanis '|' bekleniyor");

    Dugum *govde;
    if (parser_eslesir(p, TOK_SOL_SUSLU)) {
        govde = parse_blok(p);
    } else {
        govde = parse_ifade(p);
    }

    Dugum *lam = yapi_dugum_olustur(p, DUGUM_LAMBDA, bar_tok.satir, bar_tok.sutun);
    if (lam) {
        lam->veri.lambda.parametreler = liste_array_yap(&params, p->arena);
        lam->veri.lambda.param_sayi = params.sayi;
        lam->veri.lambda.govde = govde;
    }
    return lam;
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
            return d;
        }

        case TOK_ONDALIK: {
            char tampon[64];
            sayi_tokeni_temizle(t.baslangic, t.uzunluk,
                                tampon, sizeof(tampon));
            double deger = strtod(tampon, NULL);
            d = dugum_kesirli(p->arena, deger, t.satir, t.sutun);
            parser_ilerle(p);
            return d;
        }

        case TOK_METIN: {
            int ic_uz = t.uzunluk - 2;
            if (ic_uz < 0) ic_uz = 0;
            d = dugum_metin(p->arena, t.baslangic + 1, ic_uz,
                            t.satir, t.sutun);
            parser_ilerle(p);
            return d;
        }

        case TOK_KARAKTER: {
            uint32_t kp = 0;
            if (t.uzunluk >= 3) {
                kp = (unsigned char)t.baslangic[1];
            }
            d = dugum_karakter(p->arena, kp, t.satir, t.sutun);
            parser_ilerle(p);
            return d;
        }

        case TOK_DOGRU:
            d = dugum_mantiksal(p->arena, 1, t.satir, t.sutun);
            parser_ilerle(p);
            return d;

        case TOK_YANLIS:
            d = dugum_mantiksal(p->arena, 0, t.satir, t.sutun);
            parser_ilerle(p);
            return d;

        case TOK_BOS:
            d = dugum_bos(p->arena, t.satir, t.sutun);
            parser_ilerle(p);
            return d;

        case TOK_TANIMLAYICI:
        /* Secimlik<T> ve sonuc<T,H> variant kelimeleri ifade context'inde
         * tanimlayici/yapici olarak kullanilir:
         *   hic           -> None benzeri (ifade)
         *   deger(v)      -> Some(v) yapicisi (cagri)
         *   tamam(v)      -> Ok(v) yapicisi
         *   hata(e)       -> Err(e) yapicisi */
        case TOK_KENDIN:
        case TOK_HIC:
        case TOK_DEGER:
        case TOK_TAMAM:
        case TOK_HATA: {
            d = dugum_tanimlayici(p->arena, t.baslangic, t.uzunluk,
                                  t.satir, t.sutun);
            parser_ilerle(p);
            /* Tanimlayici sonrasi '{' -> yapi olusturma (CLAUDE.md karari).
             * Kondisyonel ifadelerde (eger/iken/icin/esles) izin yok —
             * orada '{' blok basidir. */
            if (p->yapi_olusturma_izni && parser_eslesir(p, TOK_SOL_SUSLU)) {
                return parse_yapi_olusturma(p, d);
            }
            return d;
        }

        case TOK_SOL_PAREN:
            parser_ilerle(p);
            d = parse_ifade(p);
            parser_bekle(p, TOK_SAG_PAREN, "P110", "')' bekleniyor");
            return d;

        case TOK_SOL_KOSELI:
            return parse_dizi_olusturma(p);

        case TOK_VEYA_BIT:
            return parse_lambda(p);

        /* === Linear Types Spec V1: kullan(e) extract === */
        case TOK_KULLAN: {
            int satir = t.satir;
            int sutun = t.sutun;
            parser_ilerle(p);
            parser_bekle(p, TOK_SOL_PAREN, "P130",
                         "kullan(...) icin '(' bekleniyor");
            Dugum *operand = parse_ifade(p);
            parser_bekle(p, TOK_SAG_PAREN, "P131",
                         "kullan(...) icin ')' bekleniyor");
            d = dugum_olustur(p->arena, DUGUM_KULLAN_IFADE, satir, sutun);
            if (d) d->veri.kullan_ifade.operand = operand;
            return d;
        }

        /* === Linear Types Spec V1: imha(e) dispose === */
        case TOK_IMHA: {
            int satir = t.satir;
            int sutun = t.sutun;
            parser_ilerle(p);
            parser_bekle(p, TOK_SOL_PAREN, "P132",
                         "imha(...) icin '(' bekleniyor");
            Dugum *operand = parse_ifade(p);
            parser_bekle(p, TOK_SAG_PAREN, "P133",
                         "imha(...) icin ')' bekleniyor");
            d = dugum_olustur(p->arena, DUGUM_IMHA_IFADE, satir, sutun);
            if (d) d->veri.imha_ifade.operand = operand;
            return d;
        }

        default:
            parser_hata(p, t, "P010", "ifade bekleniyor", NULL);
            d = dugum_hata(p->arena, t.satir, t.sutun);
            return d;
    }
}

/* === Onek operatorler (sag birlesme) === */

static Dugum *parse_onek(Parser *p) {
    Token t = parser_simdiki(p);
    int satir = t.satir;
    int sutun = t.sutun;

    switch (t.tip) {
        case TOK_EKSI: {
            parser_ilerle(p);
            Dugum *operand = parse_onek(p);
            return dugum_tekli(p->arena, OP_NEG, operand, satir, sutun);
        }
        case TOK_DEGIL: {
            parser_ilerle(p);
            Dugum *operand = parse_onek(p);
            return dugum_tekli(p->arena, OP_DEGIL, operand, satir, sutun);
        }
        case TOK_DEGIL_BIT: {
            /* ~x bitwise NOT */
            parser_ilerle(p);
            Dugum *operand = parse_onek(p);
            return dugum_tekli(p->arena, OP_BIT_DEGIL, operand, satir, sutun);
        }
        case TOK_VE_BIT: {
            /* & x  veya  & degisken x */
            parser_ilerle(p);
            Operator op = OP_REF;
            if (parser_eslesir(p, TOK_DEGISKEN)) {
                parser_ilerle(p);
                op = OP_REF_DEGISKEN;
            }
            Dugum *operand = parse_onek(p);
            return dugum_tekli(p->arena, op, operand, satir, sutun);
        }
        case TOK_YILDIZ: {
            parser_ilerle(p);
            Dugum *operand = parse_onek(p);
            return dugum_tekli(p->arena, OP_DEREFERANS, operand, satir, sutun);
        }
        default:
            return parse_birincil(p);
    }
}

/* === Sonek operatorler (cagri, erisim, indeks, yol) === */

static Dugum *parse_sonek_op(Parser *p, Dugum *sol) {
    Token t = parser_simdiki(p);

    switch (t.tip) {
        case TOK_NOKTA: {
            parser_ilerle(p);
            Token alan_tok = parser_bekle(p, TOK_TANIMLAYICI, "P120",
                                           "alan adi bekleniyor");
            Dugum *yeni = yapi_dugum_olustur(p, DUGUM_ERISIM,
                                             t.satir, t.sutun);
            if (yeni) {
                yeni->veri.erisim.nesne = sol;
                yeni->veri.erisim.alan =
                    ast_string_kopyala(p->arena,
                                       alan_tok.baslangic, alan_tok.uzunluk);
                yeni->veri.erisim.alan_uzunluk = alan_tok.uzunluk;
            }
            return yeni;
        }

        case TOK_SOL_KOSELI: {
            parser_ilerle(p);
            Dugum *idx = parse_ifade(p);
            parser_bekle(p, TOK_SAG_KOSELI, "P121", "']' bekleniyor");
            Dugum *yeni = yapi_dugum_olustur(p, DUGUM_INDEKS,
                                             t.satir, t.sutun);
            if (yeni) {
                yeni->veri.indeks.nesne = sol;
                yeni->veri.indeks.indeks = idx;
            }
            return yeni;
        }

        case TOK_SOL_PAREN: {
            parser_ilerle(p);
            Liste args;
            liste_baslat(&args);
            if (!parser_eslesir(p, TOK_SAG_PAREN)) {
                do {
                    if (parser_eslesir(p, TOK_SAG_PAREN)) break;
                    Dugum *arg = parse_ifade(p);
                    liste_ekle(&args, p->arena, arg);
                } while (parser_tuket(p, TOK_VIRGUL));
            }
            parser_bekle(p, TOK_SAG_PAREN, "P122",
                         "cagri argumanlari icin ')' bekleniyor");
            Dugum *yeni = yapi_dugum_olustur(p, DUGUM_CAGRI,
                                             t.satir, t.sutun);
            if (yeni) {
                yeni->veri.cagri.hedef = sol;
                yeni->veri.cagri.argumanlar = liste_array_yap(&args, p->arena);
                yeni->veri.cagri.sayi = args.sayi;
            }
            return yeni;
        }

        case TOK_CIFT_IKI_NOKTA: {
            parser_ilerle(p);
            Token sag_tok = parser_bekle(p, TOK_TANIMLAYICI, "P123",
                                          "yol devami bekleniyor");
            Dugum *yeni = yapi_dugum_olustur(p, DUGUM_YOL, t.satir, t.sutun);
            if (yeni) {
                yeni->veri.yol.sol = sol;
                yeni->veri.yol.sag_ad =
                    ast_string_kopyala(p->arena,
                                       sag_tok.baslangic, sag_tok.uzunluk);
                yeni->veri.yol.sag_ad_uzunluk = sag_tok.uzunluk;
            }
            return yeni;
        }

        case TOK_OLARAK: {
            /* x olarak Tip — Kirmizi E explicit cast */
            parser_ilerle(p);
            Dugum *hedef_tip = parse_tip(p);
            Dugum *yeni = yapi_dugum_olustur(p, DUGUM_OLARAK,
                                              t.satir, t.sutun);
            if (yeni) {
                yeni->veri.olarak.kaynak = sol;
                yeni->veri.olarak.hedef_tip = hedef_tip;
            }
            return yeni;
        }

        default:
            return sol;  /* sonek olmadi */
    }
}

/* === Pratt ana dongusu === */

static Dugum *parse_oncelik(Parser *p, int min_oncelik) {
    Dugum *sol = parse_onek(p);

    while (1) {
        Token t = parser_simdiki(p);

        /* Sonek operatorler en yuksek oncelikte */
        if (sonek_oncelik(t.tip) > min_oncelik) {
            sol = parse_sonek_op(p, sol);
            continue;
        }

        /* Ikili operator? */
        int op_onc = ikili_oncelik(t.tip);
        if (op_onc <= min_oncelik) break;

        Operator op = token_ikili_op(t.tip);
        int satir = t.satir;
        int sutun = t.sutun;
        parser_ilerle(p);

        /* Sol birlesme: aynı oncelikte op icin sag tarafi durdur */
        Dugum *sag = parse_oncelik(p, op_onc);
        sol = dugum_ikili(p->arena, op, sol, sag, satir, sutun);
    }

    return sol;
}

/* === Public API === */

Dugum *parse_ifade(Parser *p) {
    return parse_oncelik(p, ONC_YOK);
}

/* === Tip parser (ADIM 10.B — tam) === */

static Dugum *parse_generic_listesi(Parser *p, const char *kod_ac,
                                     const char *kod_kapa, int min_say,
                                     Liste *out_args) {
    parser_bekle(p, TOK_KUCUK, kod_ac, "'<' bekleniyor (generic)");
    /* Bos generic? */
    parser_buyuk_ayir(p);
    if (parser_eslesir(p, TOK_BUYUK)) {
        parser_hata(p, parser_simdiki(p), kod_ac,
                    "generic argumanlari bos olamaz", NULL);
    } else {
        do {
            parser_buyuk_ayir(p);
            if (parser_eslesir(p, TOK_BUYUK)) break;
            Dugum *t = parse_tip(p);
            liste_ekle(out_args, p->arena, t);
        } while (parser_tuket(p, TOK_VIRGUL));
    }
    /* Kapanisi: '>>' olabilir, ayir */
    parser_buyuk_ayir(p);
    parser_bekle(p, TOK_BUYUK, kod_kapa, "'>' bekleniyor (generic)");
    (void)min_say;
    return NULL;
}

Dugum *parse_tip(Parser *p) {
    Token t = parser_simdiki(p);

    /* === &T veya &degisken T === */
    if (t.tip == TOK_VE_BIT) {
        parser_ilerle(p);
        int degisken_mi = 0;
        if (parser_eslesir(p, TOK_DEGISKEN)) {
            parser_ilerle(p);
            degisken_mi = 1;
        }
        Dugum *hedef = parse_tip(p);
        Dugum *d = dugum_olustur(p->arena, DUGUM_TIP_REFERANS,
                                 t.satir, t.sutun);
        if (d) {
            d->veri.tip_referans.degisken_mi = degisken_mi;
            d->veri.tip_referans.hedef_tip = hedef;
        }
        return d;
    }

    /* === *T === */
    if (t.tip == TOK_YILDIZ) {
        parser_ilerle(p);
        Dugum *hedef = parse_tip(p);
        Dugum *d = dugum_olustur(p->arena, DUGUM_TIP_POINTER,
                                 t.satir, t.sutun);
        if (d) d->veri.tip_pointer.hedef_tip = hedef;
        return d;
    }

    /* === tekkez<T> — Linear Types Spec V1 === */
    if (t.tip == TOK_TEKKEZ) {
        parser_ilerle(p);
        parser_bekle(p, TOK_KUCUK, "P330", "tekkez<...> icin '<' bekleniyor");
        Dugum *ic = parse_tip(p);
        parser_buyuk_ayir(p);
        parser_bekle(p, TOK_BUYUK, "P331", "tekkez<...> icin '>' bekleniyor");
        Dugum *d = dugum_olustur(p->arena, DUGUM_TIP_TEKKEZ,
                                 t.satir, t.sutun);
        if (d) d->veri.tip_tekkez.ic_tip = ic;
        return d;
    }

    /* === secimlik<T> === */
    if (t.tip == TOK_SECIMLIK) {
        parser_ilerle(p);
        parser_bekle(p, TOK_KUCUK, "P310", "secimlik<...> icin '<' bekleniyor");
        Dugum *ic = parse_tip(p);
        parser_buyuk_ayir(p);
        parser_bekle(p, TOK_BUYUK, "P311", "secimlik<...> icin '>' bekleniyor");
        Dugum *d = dugum_olustur(p->arena, DUGUM_TIP_SECIMLIK,
                                 t.satir, t.sutun);
        if (d) d->veri.tip_secimlik.ic_tip = ic;
        return d;
    }

    /* === sonuc<T,H> === */
    if (t.tip == TOK_SONUC) {
        parser_ilerle(p);
        parser_bekle(p, TOK_KUCUK, "P312", "sonuc<...> icin '<' bekleniyor");
        Dugum *deger = parse_tip(p);
        parser_bekle(p, TOK_VIRGUL, "P313", "',' bekleniyor (sonuc<T,H>)");
        Dugum *hata = parse_tip(p);
        parser_buyuk_ayir(p);
        parser_bekle(p, TOK_BUYUK, "P314", "sonuc<...> icin '>' bekleniyor");
        Dugum *d = dugum_olustur(p->arena, DUGUM_TIP_SONUC, t.satir, t.sutun);
        if (d) {
            d->veri.tip_sonuc.deger_tip = deger;
            d->veri.tip_sonuc.hata_tip = hata;
        }
        return d;
    }

    /* === islev(T1, T2, ...) -> T === */
    if (t.tip == TOK_ISLEV) {
        parser_ilerle(p);
        parser_bekle(p, TOK_SOL_PAREN, "P315",
                     "islev tipinde '(' bekleniyor");
        Liste params;
        liste_baslat(&params);
        if (!parser_eslesir(p, TOK_SAG_PAREN)) {
            do {
                if (parser_eslesir(p, TOK_SAG_PAREN)) break;
                Dugum *pt = parse_tip(p);
                liste_ekle(&params, p->arena, pt);
            } while (parser_tuket(p, TOK_VIRGUL));
        }
        parser_bekle(p, TOK_SAG_PAREN, "P316",
                     "islev tipinde ')' bekleniyor");
        parser_bekle(p, TOK_OK, "P317",
                     "islev tipinde '->' bekleniyor");
        Dugum *donus = parse_tip(p);
        Dugum *d = dugum_olustur(p->arena, DUGUM_TIP_ISLEV,
                                 t.satir, t.sutun);
        if (d) {
            d->veri.tip_islev.parametreler = liste_array_yap(&params, p->arena);
            d->veri.tip_islev.param_sayi = params.sayi;
            d->veri.tip_islev.donus_tip = donus;
        }
        return d;
    }

    /* === Tanimlayici: basit veya kullanici tipi ===
     *   ad                   -> DUGUM_TIP_BASIT (ornegin tam32, metin)
     *   ad < T1, T2, ... >   -> DUGUM_TIP_KULLANICI (generic)
     *   "Dizi" + < T >       -> DUGUM_TIP_DIZI (ozel — Dizi standart konteyneri)
     * Yol (::) destegi su an YOK (gelecekte ADIM 10.C'de eklenir). */
    if (t.tip == TOK_TANIMLAYICI) {
        const char *ad = t.baslangic;
        int ad_uz = t.uzunluk;
        int satir = t.satir;
        int sutun = t.sutun;
        parser_ilerle(p);

        /* Generic argumanlari? */
        if (parser_eslesir(p, TOK_KUCUK)) {
            Liste args;
            liste_baslat(&args);
            (void)parse_generic_listesi(p, "P318", "P319", 1, &args);

            /* "Dizi" ozel: DUGUM_TIP_DIZI */
            if (ad_uz == 4 && memcmp(ad, "Dizi", 4) == 0) {
                if (args.sayi != 1) {
                    parser_hata(p, t, "P318b",
                        "Dizi<T> tam olarak bir tip argumani gerektirir",
                        NULL);
                }
                Dugum *eleman = (args.sayi >= 1)
                    ? args.bas->dugum
                    : dugum_hata(p->arena, satir, sutun);
                Dugum *d = dugum_olustur(p->arena, DUGUM_TIP_DIZI,
                                         satir, sutun);
                if (d) d->veri.tip_dizi.eleman_tip = eleman;
                return d;
            }

            /* Genel: kullanici tipi */
            Dugum *yol = dugum_tanimlayici(p->arena, ad, ad_uz, satir, sutun);
            Dugum *d = dugum_olustur(p->arena, DUGUM_TIP_KULLANICI,
                                     satir, sutun);
            if (d) {
                d->veri.tip_kullanici.yol = yol;
                d->veri.tip_kullanici.tip_arg =
                    liste_array_yap(&args, p->arena);
                d->veri.tip_kullanici.tip_arg_sayi = args.sayi;
            }
            return d;
        }

        /* Basit tip (jenerik degil) */
        Dugum *d = dugum_olustur(p->arena, DUGUM_TIP_BASIT, satir, sutun);
        if (d) {
            d->veri.tip_basit.ad = ast_string_kopyala(p->arena, ad, ad_uz);
            d->veri.tip_basit.ad_uzunluk = ad_uz;
        }
        return d;
    }

    parser_hata(p, t, "P011", "tip bekleniyor", NULL);
    return dugum_hata(p->arena, t.satir, t.sutun);
}
