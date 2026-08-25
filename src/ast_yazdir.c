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

        case DUGUM_KESIRLI: {
            /* [D-457] "%g" ALTI basamaga kirpiyordu — dokum ve IR AYNI
             * yardimciyi kullanmali (TEK KAYNAK, D-407). */
            char kbuf[64];
            kesirli_kisa_bicimle(d->veri.kesirli.deger, kbuf, sizeof(kbuf));
            fprintf(c, " %s", kbuf);
            konum_yaz(c, d);
            fputc('\n', c);
            break;
        }

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

        case DUGUM_TIP_YETKI:
            konum_yaz(c, d);
            fputc('\n', c);
            ast_yazdir_indent(d->veri.tip_yetki.kaynak_tipi, c, derinlik + 1);
            break;

        case DUGUM_TIP_VEKTOR:
            konum_yaz(c, d);
            fprintf(c, " lane=%d\n", d->veri.tip_vektor.lane_sayi);
            ast_yazdir_indent(d->veri.tip_vektor.eleman_tip, c, derinlik + 1);
            break;

        case DUGUM_TIP_GOREV:
            konum_yaz(c, d);
            fputc('\n', c);
            ast_yazdir_indent(d->veri.tip_gorev.ic_tip, c, derinlik + 1);
            break;

        case DUGUM_TIP_KANAL:
            konum_yaz(c, d);
            fputc('\n', c);
            ast_yazdir_indent(d->veri.tip_kanal.ic_tip, c, derinlik + 1);
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

        /* === C5: satıriçi_asm — mimari + şablon + operand sayilari === */

        case DUGUM_SATIRICI_ASM:
            fputc(' ', c);
            string_yaz(c, d->veri.satirici_asm.mimari,
                       d->veri.satirici_asm.mimari_uz);
            fputc(' ', c);
            string_yaz(c, d->veri.satirici_asm.sablon,
                       d->veri.satirici_asm.sablon_uz);
            fprintf(c, " cikti=%d girdi=%d bozulan=%d",
                    d->veri.satirici_asm.cikti_sayi,
                    d->veri.satirici_asm.girdi_sayi,
                    d->veri.satirici_asm.bozulan_sayi);
            if (d->veri.satirici_asm.cevrim >= 0) {
                fprintf(c, " cevrim=%" PRId64, d->veri.satirici_asm.cevrim);
            }
            konum_yaz(c, d);
            fputc('\n', c);
            cocuk_listesi_yaz(c, d->veri.satirici_asm.girdi_ifadeler,
                              d->veri.satirici_asm.girdi_sayi, derinlik);
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

/* ============================================================================
 * DÜZ (flat) AST dump — SELF-HOST parser diff-oracle (D-043).
 * Format (preorder):  <derinlik>\t<TIP_ADI>\t<deger>\t<satır>\t<sütün>\n
 * Derinlik-etiketli preorder AĞACI BİREBİR belirler. <deger> = skaler yük
 * (ad/literal/operatör); \t \n \r \\ kaçışlı (satır/alan ayracı güvenliği).
 * KEMGU-parser (selfhost/parser.kem) AYNI çıktıyı üretir → diff = doğruluk.
 * NOT: indent format'tan farklı olarak TÜM düğüm tipleri + çocukları gezilir.
 * ========================================================================== */

static void duz_kacis_yaz(FILE *c, const char *s, int uz) {
    if (!s) return;
    for (int i = 0; i < uz; i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch == '\t') fputs("\\t", c);
        else if (ch == '\n') fputs("\\n", c);
        else if (ch == '\r') fputs("\\r", c);
        else if (ch == '\\') fputs("\\\\", c);
        else fputc((int)ch, c);
    }
}

void ast_duz_yaz(const Dugum *d, FILE *c, int derinlik) {
    if (!d) return;  /* NULL çocuk -> satır yok (opsiyonel alanlar atlanır) */

    fprintf(c, "%d\t%s\t", derinlik, dugum_tipi_adi(d->tip));

    /* --- deger (skaler yük) --- */
    switch (d->tip) {
        case DUGUM_TAM: fprintf(c, "%" PRId64, d->veri.tam.deger); break;
        case DUGUM_KESIRLI: {   /* [D-457] TEK KAYNAK: bkz. ast.h */
            char kbuf[64];
            kesirli_kisa_bicimle(d->veri.kesirli.deger, kbuf, sizeof(kbuf));
            fputs(kbuf, c);
            break;
        }
        case DUGUM_METIN:
            duz_kacis_yaz(c, d->veri.metin_lit.metin, d->veri.metin_lit.uzunluk); break;
        case DUGUM_KARAKTER: fprintf(c, "U+%04X", d->veri.karakter.kod_noktasi); break;
        case DUGUM_MANTIKSAL: fputc(d->veri.mantiksal.deger ? '1' : '0', c); break;
        case DUGUM_TANIMLAYICI:
            duz_kacis_yaz(c, d->veri.tanimlayici.metin, d->veri.tanimlayici.uzunluk); break;
        case DUGUM_IKILI: fputs(operator_adi(d->veri.ikili.op), c); break;
        case DUGUM_TEKLI: fputs(operator_adi(d->veri.tekli.op), c); break;
        case DUGUM_ERISIM:
            duz_kacis_yaz(c, d->veri.erisim.alan, d->veri.erisim.alan_uzunluk); break;
        case DUGUM_YOL:
            duz_kacis_yaz(c, d->veri.yol.sag_ad, d->veri.yol.sag_ad_uzunluk); break;
        case DUGUM_MODUL:
            duz_kacis_yaz(c, d->veri.modul.ad, d->veri.modul.ad_uzunluk); break;
        case DUGUM_KULLAN:
            duz_kacis_yaz(c, d->veri.kullan.yol, d->veri.kullan.yol_uzunluk); break;
        case DUGUM_ISLEV:
            duz_kacis_yaz(c, d->veri.islev.ad, d->veri.islev.ad_uzunluk); break;
        case DUGUM_YAPI:
            duz_kacis_yaz(c, d->veri.yapi.ad, d->veri.yapi.ad_uzunluk); break;
        case DUGUM_CESIT:
            duz_kacis_yaz(c, d->veri.cesit.ad, d->veri.cesit.ad_uzunluk); break;
        case DUGUM_OZELLIK:
            duz_kacis_yaz(c, d->veri.ozellik.ad, d->veri.ozellik.ad_uzunluk); break;
        case DUGUM_SABIT:
            duz_kacis_yaz(c, d->veri.sabit.ad, d->veri.sabit.ad_uzunluk); break;
        case DUGUM_PARAMETRE:
            duz_kacis_yaz(c, d->veri.parametre.ad, d->veri.parametre.ad_uzunluk); break;
        case DUGUM_ALAN:
            duz_kacis_yaz(c, d->veri.alan.ad, d->veri.alan.ad_uzunluk); break;
        case DUGUM_DEGISKEN:
            duz_kacis_yaz(c, d->veri.degisken.ad, d->veri.degisken.ad_uzunluk); break;
        case DUGUM_ICIN:
            duz_kacis_yaz(c, d->veri.icin.degisken_adi, d->veri.icin.degisken_adi_uzunluk); break;
        case DUGUM_YAPI_OLUSTUR:
            duz_kacis_yaz(c, d->veri.yapi_olustur.tip_ad, d->veri.yapi_olustur.tip_ad_uzunluk); break;
        case DUGUM_ALAN_ATAMA:
            duz_kacis_yaz(c, d->veri.alan_atama.ad, d->veri.alan_atama.ad_uzunluk); break;
        case DUGUM_TIP_BASIT:
            duz_kacis_yaz(c, d->veri.tip_basit.ad, d->veri.tip_basit.ad_uzunluk); break;
        case DUGUM_TIP_REFERANS:
            if (d->veri.tip_referans.degisken_mi) fputs("degisken", c);
            break;
        case DUGUM_TIP_VEKTOR: fprintf(c, "%d", d->veri.tip_vektor.lane_sayi); break;
        case DUGUM_DESEN_TANIMLAYICI:
            duz_kacis_yaz(c, d->veri.desen_tanimlayici.ad, d->veri.desen_tanimlayici.ad_uzunluk); break;
        case DUGUM_DESEN_YAPICI:
            duz_kacis_yaz(c, d->veri.desen_yapici.ad, d->veri.desen_yapici.ad_uzunluk); break;
        case DUGUM_DESEN_YOL:
            duz_kacis_yaz(c, d->veri.desen_yol.cesit_ad, d->veri.desen_yol.cesit_uz);
            fputs("::", c);
            duz_kacis_yaz(c, d->veri.desen_yol.varyant_ad, d->veri.desen_yol.varyant_uz); break;
        default: break;  /* deger yok */
    }

    fprintf(c, "\t%d\t%d\n", d->satir, d->sutun);

    /* --- çocuklar (KANONİK sıra, derinlik+1) --- */
    int dd = derinlik + 1;
    switch (d->tip) {
        case DUGUM_PROGRAM:
            for (int i = 0; i < d->veri.program.sayi; i++)
                ast_duz_yaz(d->veri.program.uyeler[i], c, dd);
            break;
        case DUGUM_MODUL:
            for (int i = 0; i < d->veri.modul.sayi; i++)
                ast_duz_yaz(d->veri.modul.uyeler[i], c, dd);
            break;
        case DUGUM_DISA:
            ast_duz_yaz(d->veri.disa.tanim, c, dd); break;
        case DUGUM_ISLEV:
            for (int i = 0; i < d->veri.islev.param_sayi; i++)
                ast_duz_yaz(d->veri.islev.parametreler[i], c, dd);
            ast_duz_yaz(d->veri.islev.donus_tipi, c, dd);
            ast_duz_yaz(d->veri.islev.govde, c, dd);
            break;
        case DUGUM_YAPI:
            for (int i = 0; i < d->veri.yapi.alan_sayi; i++)
                ast_duz_yaz(d->veri.yapi.alanlar[i], c, dd);
            break;
        case DUGUM_CESIT:
            for (int i = 0; i < d->veri.cesit.varyant_sayi; i++) {
                if (d->veri.cesit.varyant_payload_tipleri &&
                    d->veri.cesit.varyant_payload_tipleri[i]) {
                    for (int j = 0; j < d->veri.cesit.varyant_payload_sayilari[i]; j++)
                        ast_duz_yaz(d->veri.cesit.varyant_payload_tipleri[i][j], c, dd);
                }
            }
            break;
        case DUGUM_OZELLIK:
            for (int i = 0; i < d->veri.ozellik.uye_sayi; i++)
                ast_duz_yaz(d->veri.ozellik.uyeler[i], c, dd);
            break;
        case DUGUM_UYGULA:
            ast_duz_yaz(d->veri.uygula.tip, c, dd);
            for (int i = 0; i < d->veri.uygula.ozellik_sayi; i++)
                ast_duz_yaz(d->veri.uygula.ozellikler[i], c, dd);
            for (int i = 0; i < d->veri.uygula.islev_sayi; i++)
                ast_duz_yaz(d->veri.uygula.islevler[i], c, dd);
            break;
        case DUGUM_SABIT:
            ast_duz_yaz(d->veri.sabit.tip, c, dd);
            ast_duz_yaz(d->veri.sabit.deger, c, dd);
            break;
        case DUGUM_PARAMETRE:
            ast_duz_yaz(d->veri.parametre.tip, c, dd); break;
        case DUGUM_ALAN:
            ast_duz_yaz(d->veri.alan.tip, c, dd); break;
        case DUGUM_DEGISKEN:
            ast_duz_yaz(d->veri.degisken.tip, c, dd);
            ast_duz_yaz(d->veri.degisken.deger, c, dd);
            break;
        case DUGUM_ATAMA:
            ast_duz_yaz(d->veri.atama.hedef, c, dd);
            ast_duz_yaz(d->veri.atama.deger, c, dd);
            break;
        case DUGUM_VER:
            ast_duz_yaz(d->veri.ver.deger, c, dd); break;
        case DUGUM_EGER:
            ast_duz_yaz(d->veri.eger.kosul, c, dd);
            ast_duz_yaz(d->veri.eger.gozdoldur, c, dd);
            ast_duz_yaz(d->veri.eger.yan, c, dd);
            break;
        case DUGUM_IKEN:
            ast_duz_yaz(d->veri.iken.kosul, c, dd);
            ast_duz_yaz(d->veri.iken.govde, c, dd);
            break;
        case DUGUM_ICIN:
            ast_duz_yaz(d->veri.icin.koleksiyon, c, dd);
            ast_duz_yaz(d->veri.icin.govde, c, dd);
            break;
        case DUGUM_ESLES:
            ast_duz_yaz(d->veri.esles.deger, c, dd);
            for (int i = 0; i < d->veri.esles.kol_sayi; i++)
                ast_duz_yaz(d->veri.esles.kollar[i], c, dd);
            break;
        case DUGUM_GUVENSIZ:
            ast_duz_yaz(d->veri.guvensiz.blok, c, dd); break;
        case DUGUM_SATIRICI_ASM:
            for (int i = 0; i < d->veri.satirici_asm.girdi_sayi; i++)
                ast_duz_yaz(d->veri.satirici_asm.girdi_ifadeler[i], c, dd);
            break;
        case DUGUM_BLOK:
            for (int i = 0; i < d->veri.blok.sayi; i++)
                ast_duz_yaz(d->veri.blok.deyimler[i], c, dd);
            break;
        case DUGUM_IFADE_DEYIMI:
            ast_duz_yaz(d->veri.ifade_deyimi.ifade, c, dd); break;
        case DUGUM_IKILI:
            ast_duz_yaz(d->veri.ikili.sol, c, dd);
            ast_duz_yaz(d->veri.ikili.sag, c, dd);
            break;
        case DUGUM_TEKLI:
            ast_duz_yaz(d->veri.tekli.operand, c, dd); break;
        case DUGUM_CAGRI:
            ast_duz_yaz(d->veri.cagri.hedef, c, dd);
            for (int i = 0; i < d->veri.cagri.sayi; i++)
                ast_duz_yaz(d->veri.cagri.argumanlar[i], c, dd);
            break;
        case DUGUM_ERISIM:
            ast_duz_yaz(d->veri.erisim.nesne, c, dd); break;
        case DUGUM_INDEKS:
            ast_duz_yaz(d->veri.indeks.nesne, c, dd);
            ast_duz_yaz(d->veri.indeks.indeks, c, dd);
            break;
        case DUGUM_YOL:
            ast_duz_yaz(d->veri.yol.sol, c, dd); break;
        case DUGUM_LAMBDA:
            for (int i = 0; i < d->veri.lambda.param_sayi; i++)
                ast_duz_yaz(d->veri.lambda.parametreler[i], c, dd);
            ast_duz_yaz(d->veri.lambda.govde, c, dd);
            break;
        case DUGUM_YAPI_OLUSTUR:
            for (int i = 0; i < d->veri.yapi_olustur.alan_sayi; i++)
                ast_duz_yaz(d->veri.yapi_olustur.alanlar[i], c, dd);
            break;
        case DUGUM_DIZI_OLUSTUR:
            for (int i = 0; i < d->veri.dizi_olustur.sayi; i++)
                ast_duz_yaz(d->veri.dizi_olustur.elemanlar[i], c, dd);
            break;
        case DUGUM_ALAN_ATAMA:
            ast_duz_yaz(d->veri.alan_atama.deger, c, dd); break;
        case DUGUM_KULLAN_IFADE:
            ast_duz_yaz(d->veri.kullan_ifade.operand, c, dd); break;
        case DUGUM_IMHA_IFADE:
            ast_duz_yaz(d->veri.imha_ifade.operand, c, dd); break;
        case DUGUM_TIP_DONUSTUR:
            ast_duz_yaz(d->veri.tip_donustur.kaynak, c, dd);
            ast_duz_yaz(d->veri.tip_donustur.hedef_tip, c, dd);
            break;
        case DUGUM_TIP_REFERANS:
            ast_duz_yaz(d->veri.tip_referans.hedef_tip, c, dd); break;
        case DUGUM_TIP_POINTER:
            ast_duz_yaz(d->veri.tip_pointer.hedef_tip, c, dd); break;
        case DUGUM_TIP_DIZI:
            ast_duz_yaz(d->veri.tip_dizi.eleman_tip, c, dd); break;
        case DUGUM_TIP_SECIMLIK:
            ast_duz_yaz(d->veri.tip_secimlik.ic_tip, c, dd); break;
        case DUGUM_TIP_SONUC:
            ast_duz_yaz(d->veri.tip_sonuc.deger_tip, c, dd);
            ast_duz_yaz(d->veri.tip_sonuc.hata_tip, c, dd);
            break;
        case DUGUM_TIP_ISLEV:
            for (int i = 0; i < d->veri.tip_islev.param_sayi; i++)
                ast_duz_yaz(d->veri.tip_islev.parametreler[i], c, dd);
            ast_duz_yaz(d->veri.tip_islev.donus_tip, c, dd);
            break;
        case DUGUM_TIP_KULLANICI:
            ast_duz_yaz(d->veri.tip_kullanici.yol, c, dd);
            for (int i = 0; i < d->veri.tip_kullanici.tip_arg_sayi; i++)
                ast_duz_yaz(d->veri.tip_kullanici.tip_arg[i], c, dd);
            break;
        case DUGUM_TIP_TEKKEZ:
            ast_duz_yaz(d->veri.tip_tekkez.ic_tip, c, dd); break;
        case DUGUM_TIP_SABITSURE:
            ast_duz_yaz(d->veri.tip_sabitsure.ic_tip, c, dd); break;
        case DUGUM_TIP_YETKI:
            ast_duz_yaz(d->veri.tip_yetki.kaynak_tipi, c, dd); break;
        case DUGUM_TIP_VEKTOR:
            ast_duz_yaz(d->veri.tip_vektor.eleman_tip, c, dd); break;
        case DUGUM_TIP_GOREV:
            ast_duz_yaz(d->veri.tip_gorev.ic_tip, c, dd); break;
        case DUGUM_TIP_KANAL:
            ast_duz_yaz(d->veri.tip_kanal.ic_tip, c, dd); break;
        case DUGUM_DESEN_LITERAL:
            ast_duz_yaz(d->veri.desen_literal.deger, c, dd); break;
        case DUGUM_DESEN_YAPICI:
            for (int i = 0; i < d->veri.desen_yapici.sayi; i++)
                ast_duz_yaz(d->veri.desen_yapici.alt_desenler[i], c, dd);
            break;
        case DUGUM_DESEN_YOL:
            for (int i = 0; i < d->veri.desen_yol.alt_sayi; i++)
                ast_duz_yaz(d->veri.desen_yol.alt_desenler[i], c, dd);
            break;
        case DUGUM_ESLES_KOLU:
            ast_duz_yaz(d->veri.esles_kolu.desen, c, dd);
            ast_duz_yaz(d->veri.esles_kolu.govde, c, dd);
            break;
        default: break;  /* yaprak: çocuk yok */
    }
}
