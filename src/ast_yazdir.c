#include "ast_yazdir.h"

#include <stdio.h>
#include <inttypes.h>  /* PRId64 */

/* === Yardimci === */

static void indent_yaz(FILE *c, int derinlik) {
    for (int i = 0; i < derinlik; i++) fputs("  ", c);
}

/* Konum suffix: "  1:5" */
static void konum_yaz(FILE *c, const Dugum *d) {
    fprintf(c, "  %d:%d", d->satir, d->sutun);
}

/* String'i tirnak icinde yazdir (UTF-8 byte'lari oldugu gibi).
 * NULL guvenli. */
static void string_yaz(FILE *c, const char *s, int uzunluk) {
    if (!s) { fputs("\"\"", c); return; }
    fputc('"', c);
    fwrite(s, 1, (size_t)uzunluk, c);
    fputc('"', c);
}

/* Cocuk listesi yazdir (alt indent ile) */
static void cocuk_listesi_yaz(FILE *c, Dugum **liste, int sayi, int derinlik) {
    for (int i = 0; i < sayi; i++) {
        ast_yazdir_indent(liste[i], c, derinlik + 1);
    }
}

/* === Ana yazdirma === */

void ast_yazdir_indent(const Dugum *d, FILE *c, int derinlik) {
    if (!d) {
        indent_yaz(c, derinlik);
        fputs("(NULL)\n", c);
        return;
    }

    indent_yaz(c, derinlik);
    fputs(dugum_tipi_adi(d->tip), c);

    switch (d->tip) {
        /* === Literaller (degerleri inline) === */

        case DUGUM_TAM:
            fprintf(c, " %" PRId64, d->veri.tam.deger);
            konum_yaz(c, d);
            fputc('\n', c);
            break;

        case DUGUM_KESIRLI:
            fprintf(c, " %g", d->veri.kesirli.deger);
            konum_yaz(c, d);
            fputc('\n', c);
            break;

        case DUGUM_METIN:
            fputc(' ', c);
            string_yaz(c, d->veri.metin_lit.metin, d->veri.metin_lit.uzunluk);
            konum_yaz(c, d);
            fputc('\n', c);
            break;

        case DUGUM_KARAKTER:
            fprintf(c, " U+%04X", d->veri.karakter.kod_noktasi);
            konum_yaz(c, d);
            fputc('\n', c);
            break;

        case DUGUM_MANTIKSAL:
            fputs(d->veri.mantiksal.deger ? " dogru" : " yanlis", c);
            konum_yaz(c, d);
            fputc('\n', c);
            break;

        case DUGUM_BOS:
            konum_yaz(c, d);
            fputc('\n', c);
            break;

        case DUGUM_TANIMLAYICI:
            fputc(' ', c);
            string_yaz(c, d->veri.tanimlayici.metin,
                       d->veri.tanimlayici.uzunluk);
            konum_yaz(c, d);
            fputc('\n', c);
            break;

        /* === Ifadeler === */

        case DUGUM_IKILI:
            fprintf(c, " %s", operator_adi(d->veri.ikili.op));
            konum_yaz(c, d);
            fputc('\n', c);
            ast_yazdir_indent(d->veri.ikili.sol, c, derinlik + 1);
            ast_yazdir_indent(d->veri.ikili.sag, c, derinlik + 1);
            break;

        case DUGUM_TEKLI:
            fprintf(c, " %s", operator_adi(d->veri.tekli.op));
            konum_yaz(c, d);
            fputc('\n', c);
            ast_yazdir_indent(d->veri.tekli.operand, c, derinlik + 1);
            break;

        case DUGUM_CAGRI:
            konum_yaz(c, d);
            fputc('\n', c);
            indent_yaz(c, derinlik + 1);
            fputs("hedef:\n", c);
            ast_yazdir_indent(d->veri.cagri.hedef, c, derinlik + 2);
            indent_yaz(c, derinlik + 1);
            fprintf(c, "argumanlar (%d):\n", d->veri.cagri.sayi);
            cocuk_listesi_yaz(c, d->veri.cagri.argumanlar,
                              d->veri.cagri.sayi, derinlik + 1);
            break;

        case DUGUM_ERISIM:
            fputc(' ', c);
            string_yaz(c, d->veri.erisim.alan, d->veri.erisim.alan_uzunluk);
            konum_yaz(c, d);
            fputc('\n', c);
            ast_yazdir_indent(d->veri.erisim.nesne, c, derinlik + 1);
            break;

        case DUGUM_INDEKS:
            konum_yaz(c, d);
            fputc('\n', c);
            ast_yazdir_indent(d->veri.indeks.nesne, c, derinlik + 1);
            ast_yazdir_indent(d->veri.indeks.indeks, c, derinlik + 1);
            break;

        case DUGUM_YOL:
            fputc(' ', c);
            string_yaz(c, d->veri.yol.sag_ad, d->veri.yol.sag_ad_uzunluk);
            konum_yaz(c, d);
            fputc('\n', c);
            ast_yazdir_indent(d->veri.yol.sol, c, derinlik + 1);
            break;

        /* === Ust duzey & gruplama === */

        case DUGUM_PROGRAM:
            fprintf(c, " (%d uye)", d->veri.program.sayi);
            konum_yaz(c, d);
            fputc('\n', c);
            cocuk_listesi_yaz(c, d->veri.program.uyeler,
                              d->veri.program.sayi, derinlik);
            break;

        case DUGUM_BLOK:
            fprintf(c, " (%d deyim)", d->veri.blok.sayi);
            konum_yaz(c, d);
            fputc('\n', c);
            cocuk_listesi_yaz(c, d->veri.blok.deyimler,
                              d->veri.blok.sayi, derinlik);
            break;

        case DUGUM_MODUL:
            fputc(' ', c);
            string_yaz(c, d->veri.modul.ad, d->veri.modul.ad_uzunluk);
            konum_yaz(c, d);
            fputc('\n', c);
            cocuk_listesi_yaz(c, d->veri.modul.uyeler,
                              d->veri.modul.sayi, derinlik);
            break;

        /* === Deyimler === */

        case DUGUM_VER:
            konum_yaz(c, d);
            fputc('\n', c);
            if (d->veri.ver.deger) {
                ast_yazdir_indent(d->veri.ver.deger, c, derinlik + 1);
            }
            break;

        case DUGUM_EGER:
            konum_yaz(c, d);
            fputc('\n', c);
            indent_yaz(c, derinlik + 1);
            fputs("kosul:\n", c);
            ast_yazdir_indent(d->veri.eger.kosul, c, derinlik + 2);
            indent_yaz(c, derinlik + 1);
            fputs("eger:\n", c);
            ast_yazdir_indent(d->veri.eger.gozdoldur, c, derinlik + 2);
            if (d->veri.eger.yan) {
                indent_yaz(c, derinlik + 1);
                fputs("degilse:\n", c);
                ast_yazdir_indent(d->veri.eger.yan, c, derinlik + 2);
            }
            break;

        case DUGUM_DEGISKEN:
            fputc(' ', c);
            string_yaz(c, d->veri.degisken.ad, d->veri.degisken.ad_uzunluk);
            konum_yaz(c, d);
            fputc('\n', c);
            if (d->veri.degisken.tip) {
                indent_yaz(c, derinlik + 1);
                fputs("tip:\n", c);
                ast_yazdir_indent(d->veri.degisken.tip, c, derinlik + 2);
            }
            indent_yaz(c, derinlik + 1);
            fputs("deger:\n", c);
            ast_yazdir_indent(d->veri.degisken.deger, c, derinlik + 2);
            break;

        case DUGUM_ATAMA:
            konum_yaz(c, d);
            fputc('\n', c);
            indent_yaz(c, derinlik + 1);
            fputs("hedef:\n", c);
            ast_yazdir_indent(d->veri.atama.hedef, c, derinlik + 2);
            indent_yaz(c, derinlik + 1);
            fputs("deger:\n", c);
            ast_yazdir_indent(d->veri.atama.deger, c, derinlik + 2);
            break;

        case DUGUM_IFADE_DEYIMI:
            konum_yaz(c, d);
            fputc('\n', c);
            ast_yazdir_indent(d->veri.ifade_deyimi.ifade, c, derinlik + 1);
            break;

        /* === Tanimlar === */

        case DUGUM_ISLEV:
            fputc(' ', c);
            string_yaz(c, d->veri.islev.ad, d->veri.islev.ad_uzunluk);
            if (d->veri.islev.gercekzamanli_mi) {
                fputs(" [realtime]", c);
            }
            konum_yaz(c, d);
            fputc('\n', c);
            indent_yaz(c, derinlik + 1);
            fprintf(c, "parametreler (%d):\n", d->veri.islev.param_sayi);
            cocuk_listesi_yaz(c, d->veri.islev.parametreler,
                              d->veri.islev.param_sayi, derinlik + 1);
            if (d->veri.islev.donus_tipi) {
                indent_yaz(c, derinlik + 1);
                fputs("donus_tipi:\n", c);
                ast_yazdir_indent(d->veri.islev.donus_tipi, c, derinlik + 2);
            }
            if (d->veri.islev.govde) {
                indent_yaz(c, derinlik + 1);
                fputs("govde:\n", c);
                ast_yazdir_indent(d->veri.islev.govde, c, derinlik + 2);
            }
            break;

        case DUGUM_PARAMETRE:
            fputc(' ', c);
            string_yaz(c, d->veri.parametre.ad, d->veri.parametre.ad_uzunluk);
            konum_yaz(c, d);
            fputc('\n', c);
            if (d->veri.parametre.tip) {
                ast_yazdir_indent(d->veri.parametre.tip, c, derinlik + 1);
            }
            break;

        case DUGUM_ALAN:
            fputc(' ', c);
            string_yaz(c, d->veri.alan.ad, d->veri.alan.ad_uzunluk);
            konum_yaz(c, d);
            fputc('\n', c);
            if (d->veri.alan.tip) {
                ast_yazdir_indent(d->veri.alan.tip, c, derinlik + 1);
            }
            break;

        /* === Tipler === */

        case DUGUM_TIP_BASIT:
            fputc(' ', c);
            string_yaz(c, d->veri.tip_basit.ad, d->veri.tip_basit.ad_uzunluk);
            konum_yaz(c, d);
            fputc('\n', c);
            break;

        case DUGUM_TIP_REFERANS:
            fputs(d->veri.tip_referans.degisken_mi ? " (degisken)" : "", c);
            konum_yaz(c, d);
            fputc('\n', c);
            ast_yazdir_indent(d->veri.tip_referans.hedef_tip, c, derinlik + 1);
            break;

        case DUGUM_TIP_POINTER:
            konum_yaz(c, d);
            fputc('\n', c);
            ast_yazdir_indent(d->veri.tip_pointer.hedef_tip, c, derinlik + 1);
            break;

        case DUGUM_TIP_DIZI:
            konum_yaz(c, d);
            fputc('\n', c);
            ast_yazdir_indent(d->veri.tip_dizi.eleman_tip, c, derinlik + 1);
            break;

        case DUGUM_TIP_SECIMLIK:
            konum_yaz(c, d);
            fputc('\n', c);
            ast_yazdir_indent(d->veri.tip_secimlik.ic_tip, c, derinlik + 1);
            break;

        case DUGUM_TIP_TEKKEZ:
            konum_yaz(c, d);
            fputc('\n', c);
            ast_yazdir_indent(d->veri.tip_tekkez.ic_tip, c, derinlik + 1);
            break;

        case DUGUM_TIP_SABITSURE:
            konum_yaz(c, d);
            fputc('\n', c);
            ast_yazdir_indent(d->veri.tip_sabitsure.ic_tip, c, derinlik + 1);
            break;

        case DUGUM_KULLAN_IFADE:
            konum_yaz(c, d);
            fputc('\n', c);
            ast_yazdir_indent(d->veri.kullan_ifade.operand, c, derinlik + 1);
            break;

        case DUGUM_IMHA_IFADE:
            konum_yaz(c, d);
            fputc('\n', c);
            ast_yazdir_indent(d->veri.imha_ifade.operand, c, derinlik + 1);
            break;

        case DUGUM_TIP_DONUSTUR:
            konum_yaz(c, d);
            fputc('\n', c);
            ast_yazdir_indent(d->veri.tip_donustur.kaynak, c, derinlik + 1);
            ast_yazdir_indent(d->veri.tip_donustur.hedef_tip, c, derinlik + 1);
            break;

        /* === Hata === */

        case DUGUM_HATA:
            konum_yaz(c, d);
            fputs(" (error recovery)\n", c);
            break;

        /* === Default: konum + cocuk yok === */

        default:
            konum_yaz(c, d);
            fputc('\n', c);
            break;
    }
}

void ast_yazdir(const Dugum *d, FILE *c) {
    ast_yazdir_indent(d, c, 0);
}
