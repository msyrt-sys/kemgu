#include "ast_kaynak.h"

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

/*
 * Source-equivalent pretty printer.
 * Round-trip garantisi: ast_kaynak_yaz(parse(x)) -> parse'i x'le ayni
 * AST'yi uretir. Whitespace/yorum korunmaz.
 */

static void indent_yaz(FILE *o, int d) {
    for (int i = 0; i < d; i++) fputs("    ", o);
}

static void str_yaz(FILE *o, const char *s, int n) {
    if (s && n > 0) fwrite(s, 1, (size_t)n, o);
}

static const char *op_str(Operator op) {
    switch (op) {
        case OP_ARTI:         return "+";
        case OP_EKSI:         return "-";
        case OP_CARPI:        return "*";
        case OP_BOLU:         return "/";
        case OP_MOD:          return "%";
        case OP_ESIT:         return "==";
        case OP_ESIT_DEGIL:   return "!=";
        case OP_KUCUK:        return "<";
        case OP_BUYUK:        return ">";
        case OP_KUCUK_ESIT:   return "<=";
        case OP_BUYUK_ESIT:   return ">=";
        case OP_VE:           return "ve";
        case OP_VEYA:         return "veya";
        case OP_BIT_VE:       return "&";
        case OP_BIT_VEYA:     return "|";
        case OP_BIT_OZVEYA:   return "^";
        case OP_SOLA_KAYDIR:  return "<<";
        case OP_SAGA_KAYDIR:  return ">>";
        case OP_NEG:          return "-";
        case OP_DEGIL:        return "de\xc4\x9f" "il ";
        case OP_BIT_DEGIL:    return "~";
        case OP_REF:          return "&";
        case OP_REF_DEGISKEN: return "&de\xc4\x9f" "i\xc5\x9fken ";
        case OP_DEREFERANS:   return "*";
    }
    return "?";
}

static void tip_yaz(const Dugum *t, FILE *o);

static void ifade_yaz(const Dugum *d, FILE *o);

static void deyim_yaz(const Dugum *d, FILE *o, int derinlik);

static void tip_yaz(const Dugum *t, FILE *o) {
    if (!t) { fputs("?", o); return; }
    switch (t->tip) {
        case DUGUM_TIP_BASIT:
            str_yaz(o, t->veri.tip_basit.ad, t->veri.tip_basit.ad_uzunluk);
            return;
        case DUGUM_TIP_REFERANS:
            if (t->veri.tip_referans.degisken_mi)
                fputs("&de\xc4\x9f" "i\xc5\x9fken ", o);
            else
                fputc('&', o);
            tip_yaz(t->veri.tip_referans.hedef_tip, o);
            return;
        case DUGUM_TIP_POINTER:
            fputc('*', o);
            tip_yaz(t->veri.tip_pointer.hedef_tip, o);
            return;
        case DUGUM_TIP_DIZI:
            fputs("Dizi<", o);
            tip_yaz(t->veri.tip_dizi.eleman_tip, o);
            fputc('>', o);
            return;
        case DUGUM_TIP_SECIMLIK:
            fputs("se\xc3\xa7imlik<", o);
            tip_yaz(t->veri.tip_secimlik.ic_tip, o);
            fputc('>', o);
            return;
        case DUGUM_TIP_TEKKEZ:
            fputs("tekkez<", o);
            tip_yaz(t->veri.tip_tekkez.ic_tip, o);
            fputc('>', o);
            return;
        case DUGUM_TIP_SONUC:
            fputs("sonu\xc3\xa7<", o);
            tip_yaz(t->veri.tip_sonuc.deger_tip, o);
            fputs(", ", o);
            tip_yaz(t->veri.tip_sonuc.hata_tip, o);
            fputc('>', o);
            return;
        case DUGUM_TIP_ISLEV:
            fputs("i\xc5\x9flev(", o);
            for (int i = 0; i < t->veri.tip_islev.param_sayi; i++) {
                if (i > 0) fputs(", ", o);
                tip_yaz(t->veri.tip_islev.parametreler[i], o);
            }
            fputs(") -> ", o);
            tip_yaz(t->veri.tip_islev.donus_tip, o);
            return;
        case DUGUM_TIP_KULLANICI: {
            const Dugum *yol = t->veri.tip_kullanici.yol;
            if (yol && yol->tip == DUGUM_TANIMLAYICI) {
                str_yaz(o, yol->veri.tanimlayici.metin,
                        yol->veri.tanimlayici.uzunluk);
            }
            if (t->veri.tip_kullanici.tip_arg_sayi > 0) {
                fputc('<', o);
                for (int i = 0; i < t->veri.tip_kullanici.tip_arg_sayi; i++) {
                    if (i > 0) fputs(", ", o);
                    tip_yaz(t->veri.tip_kullanici.tip_arg[i], o);
                }
                fputc('>', o);
            }
            return;
        }
        default:
            fputs("?", o);
            return;
    }
}

static void desen_yaz(const Dugum *d, FILE *o) {
    if (!d) return;
    switch (d->tip) {
        case DUGUM_DESEN_LITERAL:
            ifade_yaz(d->veri.desen_literal.deger, o);
            return;
        case DUGUM_DESEN_TANIMLAYICI:
            str_yaz(o, d->veri.desen_tanimlayici.ad,
                    d->veri.desen_tanimlayici.ad_uzunluk);
            return;
        case DUGUM_DESEN_JOKER:
            fputc('_', o);
            return;
        case DUGUM_DESEN_YAPICI:
            str_yaz(o, d->veri.desen_yapici.ad,
                    d->veri.desen_yapici.ad_uzunluk);
            if (d->veri.desen_yapici.sayi > 0) {
                fputc('(', o);
                for (int i = 0; i < d->veri.desen_yapici.sayi; i++) {
                    if (i > 0) fputs(", ", o);
                    desen_yaz(d->veri.desen_yapici.alt_desenler[i], o);
                }
                fputc(')', o);
            }
            return;
        default:
            return;
    }
}

static void ifade_yaz(const Dugum *d, FILE *o) {
    if (!d) { fputs("?", o); return; }
    switch (d->tip) {
        case DUGUM_TAM:
            fprintf(o, "%" PRId64, d->veri.tam.deger);
            return;
        case DUGUM_KESIRLI:
            fprintf(o, "%g", d->veri.kesirli.deger);
            return;
        case DUGUM_METIN:
            fputc('"', o);
            str_yaz(o, d->veri.metin_lit.metin, d->veri.metin_lit.uzunluk);
            fputc('"', o);
            return;
        case DUGUM_KARAKTER:
            /* Basit: ASCII range icin kullan; aksi halde hex */
            if (d->veri.karakter.kod_noktasi < 128 &&
                d->veri.karakter.kod_noktasi >= 32) {
                fprintf(o, "'%c'", (char)d->veri.karakter.kod_noktasi);
            } else {
                fprintf(o, "'\\u%04X'", d->veri.karakter.kod_noktasi);
            }
            return;
        case DUGUM_MANTIKSAL:
            fputs(d->veri.mantiksal.deger ? "do\xc4\x9fru" : "yanl\xc4\xb1\xc5\x9f", o);
            return;
        case DUGUM_BOS:
            fputs("bo\xc5\x9f", o);
            return;
        case DUGUM_TANIMLAYICI:
            str_yaz(o, d->veri.tanimlayici.metin,
                    d->veri.tanimlayici.uzunluk);
            return;
        case DUGUM_BOYUT:
            fputs("boyut<", o);
            tip_yaz(d->veri.boyut.tip, o);
            fputc('>', o);
            return;
        case DUGUM_IKILI:
            fputc('(', o);
            ifade_yaz(d->veri.ikili.sol, o);
            fputc(' ', o);
            fputs(op_str(d->veri.ikili.op), o);
            fputc(' ', o);
            ifade_yaz(d->veri.ikili.sag, o);
            fputc(')', o);
            return;
        case DUGUM_TEKLI:
            fputs(op_str(d->veri.tekli.op), o);
            ifade_yaz(d->veri.tekli.operand, o);
            return;
        case DUGUM_CAGRI:
            ifade_yaz(d->veri.cagri.hedef, o);
            fputc('(', o);
            for (int i = 0; i < d->veri.cagri.sayi; i++) {
                if (i > 0) fputs(", ", o);
                ifade_yaz(d->veri.cagri.argumanlar[i], o);
            }
            fputc(')', o);
            return;
        case DUGUM_ERISIM:
            ifade_yaz(d->veri.erisim.nesne, o);
            fputc('.', o);
            str_yaz(o, d->veri.erisim.alan, d->veri.erisim.alan_uzunluk);
            return;
        case DUGUM_INDEKS:
            ifade_yaz(d->veri.indeks.nesne, o);
            fputc('[', o);
            ifade_yaz(d->veri.indeks.indeks, o);
            fputc(']', o);
            return;
        case DUGUM_YOL:
            ifade_yaz(d->veri.yol.sol, o);
            fputs("::", o);
            str_yaz(o, d->veri.yol.sag_ad, d->veri.yol.sag_ad_uzunluk);
            return;
        case DUGUM_DIZI_OLUSTUR:
            fputc('[', o);
            for (int i = 0; i < d->veri.dizi_olustur.sayi; i++) {
                if (i > 0) fputs(", ", o);
                ifade_yaz(d->veri.dizi_olustur.elemanlar[i], o);
            }
            fputc(']', o);
            return;
        case DUGUM_YAPI_OLUSTUR:
            str_yaz(o, d->veri.yapi_olustur.tip_ad,
                    d->veri.yapi_olustur.tip_ad_uzunluk);
            fputs(" { ", o);
            for (int i = 0; i < d->veri.yapi_olustur.alan_sayi; i++) {
                if (i > 0) fputs(", ", o);
                const Dugum *aa = d->veri.yapi_olustur.alanlar[i];
                str_yaz(o, aa->veri.alan_atama.ad,
                        aa->veri.alan_atama.ad_uzunluk);
                fputs(": ", o);
                ifade_yaz(aa->veri.alan_atama.deger, o);
            }
            fputs(" }", o);
            return;
        case DUGUM_LAMBDA:
            fputc('|', o);
            for (int i = 0; i < d->veri.lambda.param_sayi; i++) {
                if (i > 0) fputs(", ", o);
                const Dugum *p = d->veri.lambda.parametreler[i];
                str_yaz(o, p->veri.parametre.ad,
                        p->veri.parametre.ad_uzunluk);
                if (p->veri.parametre.tip) {
                    fputs(": ", o);
                    tip_yaz(p->veri.parametre.tip, o);
                }
            }
            fputs("| ", o);
            if (d->veri.lambda.govde) {
                if (d->veri.lambda.govde->tip == DUGUM_BLOK) {
                    deyim_yaz(d->veri.lambda.govde, o, 0);
                } else {
                    ifade_yaz(d->veri.lambda.govde, o);
                }
            }
            return;
        default:
            fputs("/*?*/", o);
            return;
    }
}

static void deyim_yaz(const Dugum *d, FILE *o, int derinlik) {
    if (!d) return;
    switch (d->tip) {
        case DUGUM_BLOK: {
            fputs("{\n", o);
            for (int i = 0; i < d->veri.blok.sayi; i++) {
                indent_yaz(o, derinlik + 1);
                deyim_yaz(d->veri.blok.deyimler[i], o, derinlik + 1);
                fputc('\n', o);
            }
            indent_yaz(o, derinlik);
            fputc('}', o);
            return;
        }
        case DUGUM_DEGISKEN:
            fputs("de\xc4\x9f" "i\xc5\x9fken ", o);
            str_yaz(o, d->veri.degisken.ad, d->veri.degisken.ad_uzunluk);
            if (d->veri.degisken.tip) {
                fputs(": ", o);
                tip_yaz(d->veri.degisken.tip, o);
            }
            if (d->veri.degisken.deger) {
                fputs(" = ", o);
                ifade_yaz(d->veri.degisken.deger, o);
            }
            fputc(';', o);
            return;
        case DUGUM_ATAMA:
            ifade_yaz(d->veri.atama.hedef, o);
            fputs(" = ", o);
            ifade_yaz(d->veri.atama.deger, o);
            fputc(';', o);
            return;
        case DUGUM_VER:
            fputs("ver", o);
            if (d->veri.ver.deger) {
                fputc(' ', o);
                ifade_yaz(d->veri.ver.deger, o);
            }
            fputc(';', o);
            return;
        case DUGUM_EGER:
            fputs("e\xc4\x9f" "er ", o);
            ifade_yaz(d->veri.eger.kosul, o);
            fputc(' ', o);
            deyim_yaz(d->veri.eger.gozdoldur, o, derinlik);
            if (d->veri.eger.yan) {
                fputs(" de\xc4\x9f" "ilse ", o);
                deyim_yaz(d->veri.eger.yan, o, derinlik);
            }
            return;
        case DUGUM_IKEN:
            fputs("iken ", o);
            ifade_yaz(d->veri.iken.kosul, o);
            fputc(' ', o);
            deyim_yaz(d->veri.iken.govde, o, derinlik);
            return;
        case DUGUM_ICIN:
            fputs("i\xc3\xa7in ", o);
            str_yaz(o, d->veri.icin.degisken_adi,
                    d->veri.icin.degisken_adi_uzunluk);
            fputs(": ", o);
            ifade_yaz(d->veri.icin.koleksiyon, o);
            fputc(' ', o);
            deyim_yaz(d->veri.icin.govde, o, derinlik);
            return;
        case DUGUM_ESLES:
            fputs("e\xc5\x9fle\xc5\x9f ", o);
            ifade_yaz(d->veri.esles.deger, o);
            fputs(" {\n", o);
            for (int i = 0; i < d->veri.esles.kol_sayi; i++) {
                const Dugum *kol = d->veri.esles.kollar[i];
                indent_yaz(o, derinlik + 1);
                desen_yaz(kol->veri.esles_kolu.desen, o);
                fputs(" => ", o);
                if (kol->veri.esles_kolu.govde->tip == DUGUM_BLOK) {
                    deyim_yaz(kol->veri.esles_kolu.govde, o, derinlik + 1);
                } else {
                    ifade_yaz(kol->veri.esles_kolu.govde, o);
                    fputc(';', o);
                }
                fputc('\n', o);
            }
            indent_yaz(o, derinlik);
            fputc('}', o);
            return;
        case DUGUM_GUVENSIZ:
            fputs("g\xc3\xbcvensiz ", o);
            deyim_yaz(d->veri.guvensiz.blok, o, derinlik);
            return;
        case DUGUM_IFADE_DEYIMI:
            ifade_yaz(d->veri.ifade_deyimi.ifade, o);
            fputc(';', o);
            return;
        default:
            ifade_yaz(d, o);
            return;
    }
}

static void tanim_yaz(const Dugum *d, FILE *o, int derinlik) {
    if (!d) return;
    switch (d->tip) {
        case DUGUM_ISLEV:
            /* Oznitelikler */
            if (d->veri.islev.ciplak_mi || d->veri.islev.kesme_mi ||
                d->veri.islev.bolum) {
                fputc('[', o);
                int ilk = 1;
                if (d->veri.islev.ciplak_mi) {
                    fputs("ciplak", o); ilk = 0;
                }
                if (d->veri.islev.kesme_mi) {
                    if (!ilk) fputs(", ", o);
                    fputs("kesme", o); ilk = 0;
                }
                if (d->veri.islev.bolum) {
                    if (!ilk) fputs(", ", o);
                    fputs("bolum: \"", o);
                    str_yaz(o, d->veri.islev.bolum,
                            d->veri.islev.bolum_uzunluk);
                    fputc('"', o);
                }
                fputs("]\n", o);
            }
            fputs("i\xc5\x9flev ", o);
            str_yaz(o, d->veri.islev.ad, d->veri.islev.ad_uzunluk);
            fputc('(', o);
            for (int i = 0; i < d->veri.islev.param_sayi; i++) {
                if (i > 0) fputs(", ", o);
                const Dugum *p = d->veri.islev.parametreler[i];
                str_yaz(o, p->veri.parametre.ad,
                        p->veri.parametre.ad_uzunluk);
                fputs(": ", o);
                tip_yaz(p->veri.parametre.tip, o);
            }
            fputc(')', o);
            if (d->veri.islev.donus_tipi) {
                fputs(" -> ", o);
                tip_yaz(d->veri.islev.donus_tipi, o);
            }
            fputc(' ', o);
            if (d->veri.islev.govde) {
                deyim_yaz(d->veri.islev.govde, o, derinlik);
            }
            return;
        case DUGUM_YAPI:
            fputs("yap\xc4\xb1 ", o);
            str_yaz(o, d->veri.yapi.ad, d->veri.yapi.ad_uzunluk);
            if (d->veri.yapi.tip_param_sayi > 0) {
                fputc('<', o);
                for (int i = 0; i < d->veri.yapi.tip_param_sayi; i++) {
                    if (i > 0) fputs(", ", o);
                    fputs(d->veri.yapi.tip_paramlar[i], o);
                }
                fputc('>', o);
            }
            fputs(" {\n", o);
            for (int i = 0; i < d->veri.yapi.alan_sayi; i++) {
                const Dugum *al = d->veri.yapi.alanlar[i];
                indent_yaz(o, derinlik + 1);
                str_yaz(o, al->veri.alan.ad, al->veri.alan.ad_uzunluk);
                fputs(": ", o);
                tip_yaz(al->veri.alan.tip, o);
                fputs(";\n", o);
            }
            indent_yaz(o, derinlik);
            fputc('}', o);
            return;
        case DUGUM_SABIT:
            fputs("sabit ", o);
            str_yaz(o, d->veri.sabit.ad, d->veri.sabit.ad_uzunluk);
            if (d->veri.sabit.tip) {
                fputs(": ", o);
                tip_yaz(d->veri.sabit.tip, o);
            }
            fputs(" = ", o);
            ifade_yaz(d->veri.sabit.deger, o);
            fputc(';', o);
            return;
        case DUGUM_DISA:
            fputs("d\xc4\xb1\xc5\x9f" "a ", o);
            tanim_yaz(d->veri.disa.tanim, o, derinlik);
            return;
        case DUGUM_KULLAN:
            fputs("kullan ", o);
            str_yaz(o, d->veri.kullan.yol, d->veri.kullan.yol_uzunluk);
            fputc(';', o);
            return;
        case DUGUM_MODUL:
            fputs("mod\xc3\xbcl ", o);
            str_yaz(o, d->veri.modul.ad, d->veri.modul.ad_uzunluk);
            fputs(" {\n", o);
            for (int i = 0; i < d->veri.modul.sayi; i++) {
                indent_yaz(o, derinlik + 1);
                tanim_yaz(d->veri.modul.uyeler[i], o, derinlik + 1);
                fputs("\n\n", o);
            }
            indent_yaz(o, derinlik);
            fputc('}', o);
            return;
        default:
            return;
    }
}

void ast_kaynak_yaz_indent(const Dugum *d, FILE *o, int derinlik) {
    if (!d) return;
    if (d->tip == DUGUM_PROGRAM) {
        for (int i = 0; i < d->veri.program.sayi; i++) {
            tanim_yaz(d->veri.program.uyeler[i], o, derinlik);
            fputs("\n\n", o);
        }
        return;
    }
    tanim_yaz(d, o, derinlik);
}

void ast_kaynak_yaz(const Dugum *d, FILE *o) {
    ast_kaynak_yaz_indent(d, o, 0);
}
