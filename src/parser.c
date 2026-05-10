#include "parser.h"
#include "hata.h"

#include <string.h>
#include <stdio.h>

/* === Linked list yardimcilari (parse-zamani cocuk listeleri) === */

typedef struct DugumLink {
    Dugum *dugum;
    struct DugumLink *sonraki;
} DugumLink;

typedef struct Liste {
    DugumLink *bas;
    DugumLink *son;
    int sayi;
} Liste;

static void liste_baslat(Liste *l) {
    l->bas = NULL;
    l->son = NULL;
    l->sayi = 0;
}

static void liste_ekle(Liste *l, Arena *a, Dugum *d) {
    DugumLink *link = (DugumLink *)arena_ayir(a, sizeof(DugumLink));
    if (!link) return;
    link->dugum = d;
    link->sonraki = NULL;
    if (l->son) {
        l->son->sonraki = link;
    } else {
        l->bas = link;
    }
    l->son = link;
    l->sayi++;
}

static Dugum **liste_array_yap(const Liste *l, Arena *a) {
    if (l->sayi == 0) return NULL;
    Dugum **arr = (Dugum **)arena_ayir(a, sizeof(Dugum *) * (size_t)l->sayi);
    if (!arr) return NULL;
    int i = 0;
    for (DugumLink *link = l->bas; link; link = link->sonraki) {
        arr[i++] = link->dugum;
    }
    return arr;
}

/* === Token akisi === */

void parser_baslat(Parser *p, Lexer *l, Arena *a,
                   const char *dosya_adi, const char *kaynak) {
    p->lexer = l;
    p->simdiki = lexer_sonraki_token(l);
    p->sonraki_var = 0;
    p->arena = a;
    p->hata_sayisi = 0;
    p->dosya_adi = dosya_adi;
    p->kaynak = kaynak;
}

Token parser_simdiki(const Parser *p) {
    return p->simdiki;
}

Token parser_onizle(Parser *p) {
    if (!p->sonraki_var) {
        p->sonraki = lexer_sonraki_token(p->lexer);
        p->sonraki_var = 1;
    }
    return p->sonraki;
}

void parser_ilerle(Parser *p) {
    if (p->sonraki_var) {
        p->simdiki = p->sonraki;
        p->sonraki_var = 0;
    } else {
        p->simdiki = lexer_sonraki_token(p->lexer);
    }
}

int parser_eslesir(const Parser *p, TokenTipi t) {
    return p->simdiki.tip == t;
}

int parser_tuket(Parser *p, TokenTipi t) {
    if (p->simdiki.tip == t) {
        parser_ilerle(p);
        return 1;
    }
    return 0;
}

Token parser_bekle(Parser *p, TokenTipi t, const char *kod, const char *mesaj) {
    Token tok = p->simdiki;
    if (p->simdiki.tip == t) {
        parser_ilerle(p);
        return tok;
    }
    parser_hata(p, tok, kod, mesaj, NULL);
    return tok;
}

void parser_hata(Parser *p, Token tok,
                 const char *kod, const char *mesaj, const char *ipucu) {
    if (p->hata_sayisi >= PARSER_MAX_HATA) return;
    p->hata_sayisi++;
    hata_raporla(p->dosya_adi, p->kaynak,
                 tok.satir, tok.sutun, kod, mesaj, ipucu);
}

/* Sync token mu? Panik mod bunlardan birinde durur. */
static int sync_token_mu(TokenTipi t) {
    switch (t) {
        case TOK_NOKTALI_VIRGUL:
        case TOK_SAG_SUSLU:
        case TOK_ISLEV:
        case TOK_YAPI:
        case TOK_OZELLIK:
        case TOK_MODUL:
        case TOK_KULLAN:
        case TOK_DISA:
        case TOK_SABIT:
        case TOK_UYGULA:
        case TOK_DOSYA_SONU:
            return 1;
        default:
            return 0;
    }
}

void parser_panik_sync(Parser *p) {
    /* Sync token'a kadar yut */
    while (!sync_token_mu(p->simdiki.tip)) {
        parser_ilerle(p);
    }
    /* ; ve } tuket — bunlar deyim/blok sonu */
    if (p->simdiki.tip == TOK_NOKTALI_VIRGUL ||
        p->simdiki.tip == TOK_SAG_SUSLU) {
        parser_ilerle(p);
    }
    /* Keyword'ler tuketilmez — onlar yeni tanim baslangici */
}

/* === Forward declarations === */

static Dugum *parse_blok(Parser *p);
static Dugum *parse_deyim(Parser *p);
static Dugum *parse_ust_oge(Parser *p);
static Dugum *parse_islev_tanimi(Parser *p);
static Dugum *parse_yapi_tanimi(Parser *p);
static Dugum *parse_kullan(Parser *p);
static Dugum *parse_disa(Parser *p);
static Dugum *parse_modul_tanimi(Parser *p);
static Dugum *parse_sabit_tanimi(Parser *p);
static Dugum *parse_degisken_deyimi(Parser *p);
static Dugum *parse_ver_deyimi(Parser *p);
static Dugum *parse_ifade_veya_atama_deyimi(Parser *p);

/* === Parametre === */

static Dugum *parse_parametre(Parser *p) {
    Token ad_tok = parser_simdiki(p);
    if (ad_tok.tip != TOK_TANIMLAYICI) {
        parser_hata(p, ad_tok, "P012", "parametre adi bekleniyor", NULL);
        return dugum_hata(p->arena, ad_tok.satir, ad_tok.sutun);
    }
    parser_ilerle(p);
    parser_bekle(p, TOK_IKI_NOKTA, "P013",
                 "parametre tipi icin ':' bekleniyor");
    Dugum *tip = parse_tip(p);

    Dugum *d = dugum_olustur(p->arena, DUGUM_PARAMETRE,
                             ad_tok.satir, ad_tok.sutun);
    if (!d) return NULL;
    d->veri.parametre.ad =
        ast_string_kopyala(p->arena, ad_tok.baslangic, ad_tok.uzunluk);
    d->veri.parametre.ad_uzunluk = ad_tok.uzunluk;
    d->veri.parametre.tip = tip;
    return d;
}

/* === Islev tanimi ===
 * islev_tanimi = "islev" tanimlayici "(" [parametreler] ")" ["->" tip] blok */

static Dugum *parse_islev_tanimi(Parser *p) {
    Token islev_tok = parser_simdiki(p);
    parser_ilerle(p);  /* 'islev' */

    Token ad_tok = parser_bekle(p, TOK_TANIMLAYICI, "P014",
                                "islev adi bekleniyor");
    parser_bekle(p, TOK_SOL_PAREN, "P015", "'(' bekleniyor");

    Liste params;
    liste_baslat(&params);

    if (!parser_eslesir(p, TOK_SAG_PAREN)) {
        do {
            Dugum *param = parse_parametre(p);
            liste_ekle(&params, p->arena, param);
        } while (parser_tuket(p, TOK_VIRGUL));
    }
    parser_bekle(p, TOK_SAG_PAREN, "P016", "')' bekleniyor");

    Dugum *donus = NULL;
    if (parser_tuket(p, TOK_OK)) {
        donus = parse_tip(p);
    }

    Dugum *govde = NULL;
    if (parser_eslesir(p, TOK_SOL_SUSLU)) {
        govde = parse_blok(p);
    } else {
        parser_hata(p, parser_simdiki(p), "P017",
                    "islev govdesi icin '{' bekleniyor", NULL);
    }

    Dugum *d = dugum_olustur(p->arena, DUGUM_ISLEV,
                             islev_tok.satir, islev_tok.sutun);
    if (!d) return NULL;
    d->veri.islev.ad =
        ast_string_kopyala(p->arena, ad_tok.baslangic, ad_tok.uzunluk);
    d->veri.islev.ad_uzunluk = ad_tok.uzunluk;
    d->veri.islev.parametreler = liste_array_yap(&params, p->arena);
    d->veri.islev.param_sayi = params.sayi;
    d->veri.islev.donus_tipi = donus;
    d->veri.islev.govde = govde;
    return d;
}

/* === Yapi tanimi === */
/* alan_tanimi = tanimlayici ":" tip ";" */

static Dugum *parse_alan(Parser *p) {
    Token ad_tok = parser_simdiki(p);
    if (ad_tok.tip != TOK_TANIMLAYICI) {
        parser_hata(p, ad_tok, "P018", "alan adi bekleniyor", NULL);
        parser_panik_sync(p);
        return dugum_hata(p->arena, ad_tok.satir, ad_tok.sutun);
    }
    parser_ilerle(p);
    parser_bekle(p, TOK_IKI_NOKTA, "P019",
                 "alan tipi icin ':' bekleniyor");
    Dugum *tip = parse_tip(p);
    parser_bekle(p, TOK_NOKTALI_VIRGUL, "P020",
                 "alan sonunda ';' bekleniyor");

    Dugum *d = dugum_olustur(p->arena, DUGUM_ALAN,
                             ad_tok.satir, ad_tok.sutun);
    if (!d) return NULL;
    d->veri.alan.ad =
        ast_string_kopyala(p->arena, ad_tok.baslangic, ad_tok.uzunluk);
    d->veri.alan.ad_uzunluk = ad_tok.uzunluk;
    d->veri.alan.tip = tip;
    return d;
}

/* yapi_tanimi = "yapi" tanimlayici "{" alanlar "}" */

static Dugum *parse_yapi_tanimi(Parser *p) {
    Token yapi_tok = parser_simdiki(p);
    parser_ilerle(p);  /* 'yapi' */

    Token ad_tok = parser_bekle(p, TOK_TANIMLAYICI, "P021",
                                "yapi adi bekleniyor");
    /* Generic tip parametreleri (< T >) ADIM 10'da */
    parser_bekle(p, TOK_SOL_SUSLU, "P022", "'{' bekleniyor");

    Liste alanlar;
    liste_baslat(&alanlar);
    while (!parser_eslesir(p, TOK_SAG_SUSLU) &&
           !parser_eslesir(p, TOK_DOSYA_SONU)) {
        Dugum *alan = parse_alan(p);
        if (alan) liste_ekle(&alanlar, p->arena, alan);
    }
    parser_bekle(p, TOK_SAG_SUSLU, "P023", "'}' bekleniyor");

    Dugum *d = dugum_olustur(p->arena, DUGUM_YAPI,
                             yapi_tok.satir, yapi_tok.sutun);
    if (!d) return NULL;
    d->veri.yapi.ad =
        ast_string_kopyala(p->arena, ad_tok.baslangic, ad_tok.uzunluk);
    d->veri.yapi.ad_uzunluk = ad_tok.uzunluk;
    d->veri.yapi.tip_paramlar = NULL;
    d->veri.yapi.tip_param_sayi = 0;
    d->veri.yapi.alanlar = liste_array_yap(&alanlar, p->arena);
    d->veri.yapi.alan_sayi = alanlar.sayi;
    return d;
}

/* === Sabit ===
 * sabit_tanimi = "sabit" tanimlayici ":" tip "=" ifade ";" */

static Dugum *parse_sabit_tanimi(Parser *p) {
    Token sabit_tok = parser_simdiki(p);
    parser_ilerle(p);

    Token ad_tok = parser_bekle(p, TOK_TANIMLAYICI, "P030",
                                "sabit adi bekleniyor");
    parser_bekle(p, TOK_IKI_NOKTA, "P031", "':' bekleniyor (sabit tipi)");
    Dugum *tip = parse_tip(p);
    parser_bekle(p, TOK_ESIT, "P032", "'=' bekleniyor (sabit degeri)");
    Dugum *deger = parse_ifade(p);
    parser_bekle(p, TOK_NOKTALI_VIRGUL, "P033", "';' bekleniyor");

    Dugum *d = dugum_olustur(p->arena, DUGUM_SABIT,
                             sabit_tok.satir, sabit_tok.sutun);
    if (!d) return NULL;
    d->veri.sabit.ad =
        ast_string_kopyala(p->arena, ad_tok.baslangic, ad_tok.uzunluk);
    d->veri.sabit.ad_uzunluk = ad_tok.uzunluk;
    d->veri.sabit.tip = tip;
    d->veri.sabit.deger = deger;
    return d;
}

/* === Kullan ===
 * kullan_bildirimi = "kullan" modul_yolu ";" */

static Dugum *parse_kullan(Parser *p) {
    Token kullan_tok = parser_simdiki(p);
    parser_ilerle(p);

    /* Yol icindeki tum tanimlayicilari topla, "::" ile birlestirip arena'da sakla */
    Token ilk = parser_bekle(p, TOK_TANIMLAYICI, "P040", "yol bekleniyor");

    /* Ham toplam uzunluk hesapla */
    int toplam_uz = ilk.uzunluk;
    /* Geçici depolama icin counter+arena gerek - basit: tek pass yap */
    /* Stratejimiz: arena'da parca parca buyut. Once ilk yaz, sonra ekle. */
    char *yol = (char *)arena_ayir(p->arena, (size_t)ilk.uzunluk + 1);
    if (yol) {
        memcpy(yol, ilk.baslangic, (size_t)ilk.uzunluk);
        yol[ilk.uzunluk] = '\0';
    }

    while (parser_eslesir(p, TOK_CIFT_IKI_NOKTA)) {
        parser_ilerle(p);
        Token sonra = parser_bekle(p, TOK_TANIMLAYICI, "P041",
                                   "yol devami bekleniyor");
        /* Yeni boyut: eski + 2 (::) + sonra.uzunluk + 1 (null) */
        int yeni_uz = toplam_uz + 2 + sonra.uzunluk;
        char *yeni = (char *)arena_ayir(p->arena, (size_t)yeni_uz + 1);
        if (yeni && yol) {
            memcpy(yeni, yol, (size_t)toplam_uz);
            memcpy(yeni + toplam_uz, "::", 2);
            memcpy(yeni + toplam_uz + 2, sonra.baslangic, (size_t)sonra.uzunluk);
            yeni[yeni_uz] = '\0';
            yol = yeni;
            toplam_uz = yeni_uz;
        }
    }
    parser_bekle(p, TOK_NOKTALI_VIRGUL, "P042", "';' bekleniyor");

    Dugum *d = dugum_olustur(p->arena, DUGUM_KULLAN,
                             kullan_tok.satir, kullan_tok.sutun);
    if (!d) return NULL;
    d->veri.kullan.yol = yol;
    d->veri.kullan.yol_uzunluk = toplam_uz;
    return d;
}

/* === Disa === */

static Dugum *parse_disa(Parser *p) {
    Token disa_tok = parser_simdiki(p);
    parser_ilerle(p);

    Dugum *tanim = NULL;
    Token sonra = parser_simdiki(p);
    switch (sonra.tip) {
        case TOK_ISLEV:  tanim = parse_islev_tanimi(p); break;
        case TOK_YAPI:   tanim = parse_yapi_tanimi(p);  break;
        case TOK_SABIT:  tanim = parse_sabit_tanimi(p); break;
        default:
            parser_hata(p, sonra, "P050",
                "'disa' sonrasi tanim bekleniyor (islev/yapi/sabit)", NULL);
            tanim = dugum_hata(p->arena, sonra.satir, sonra.sutun);
            parser_panik_sync(p);
            break;
    }

    Dugum *d = dugum_olustur(p->arena, DUGUM_DISA,
                             disa_tok.satir, disa_tok.sutun);
    if (!d) return NULL;
    d->veri.disa.tanim = tanim;
    return d;
}

/* === Modul ===
 * modul_tanimi = "modul" tanimlayici "{" ust_oge* "}" */

static Dugum *parse_modul_tanimi(Parser *p) {
    Token modul_tok = parser_simdiki(p);
    parser_ilerle(p);
    Token ad_tok = parser_bekle(p, TOK_TANIMLAYICI, "P060",
                                "modul adi bekleniyor");
    parser_bekle(p, TOK_SOL_SUSLU, "P061", "'{' bekleniyor");

    Liste uyeler;
    liste_baslat(&uyeler);
    while (!parser_eslesir(p, TOK_SAG_SUSLU) &&
           !parser_eslesir(p, TOK_DOSYA_SONU)) {
        Dugum *uye = parse_ust_oge(p);
        if (uye) liste_ekle(&uyeler, p->arena, uye);
        if (p->hata_sayisi >= PARSER_MAX_HATA) break;
    }
    parser_bekle(p, TOK_SAG_SUSLU, "P062", "'}' bekleniyor");

    Dugum *d = dugum_olustur(p->arena, DUGUM_MODUL,
                             modul_tok.satir, modul_tok.sutun);
    if (!d) return NULL;
    d->veri.modul.ad =
        ast_string_kopyala(p->arena, ad_tok.baslangic, ad_tok.uzunluk);
    d->veri.modul.ad_uzunluk = ad_tok.uzunluk;
    d->veri.modul.uyeler = liste_array_yap(&uyeler, p->arena);
    d->veri.modul.sayi = uyeler.sayi;
    return d;
}

/* === Ust oge === */

static Dugum *parse_ust_oge(Parser *p) {
    Token t = parser_simdiki(p);
    switch (t.tip) {
        case TOK_ISLEV:   return parse_islev_tanimi(p);
        case TOK_YAPI:    return parse_yapi_tanimi(p);
        case TOK_KULLAN:  return parse_kullan(p);
        case TOK_DISA:    return parse_disa(p);
        case TOK_MODUL:   return parse_modul_tanimi(p);
        case TOK_SABIT:   return parse_sabit_tanimi(p);
        default:
            parser_hata(p, t, "P001",
                "ust duzey tanim bekleniyor (islev/yapi/kullan/disa/modul/sabit)",
                NULL);
            parser_panik_sync(p);
            return NULL;
    }
}

/* === Blok ve deyimler === */

static Dugum *parse_blok(Parser *p) {
    Token sus_tok = parser_simdiki(p);
    parser_bekle(p, TOK_SOL_SUSLU, "P070", "'{' bekleniyor");

    Liste deyimler;
    liste_baslat(&deyimler);

    while (!parser_eslesir(p, TOK_SAG_SUSLU) &&
           !parser_eslesir(p, TOK_DOSYA_SONU)) {
        Token once = parser_simdiki(p);
        Dugum *deyim = parse_deyim(p);
        if (deyim) liste_ekle(&deyimler, p->arena, deyim);
        /* Sonsuz dongu korumasi: token ilerlemediyse zorla ilerle */
        if (parser_simdiki(p).baslangic == once.baslangic &&
            !parser_eslesir(p, TOK_DOSYA_SONU)) {
            parser_ilerle(p);
        }
        if (p->hata_sayisi >= PARSER_MAX_HATA) break;
    }
    parser_bekle(p, TOK_SAG_SUSLU, "P071", "'}' bekleniyor");

    return dugum_blok(p->arena,
                      liste_array_yap(&deyimler, p->arena),
                      deyimler.sayi,
                      sus_tok.satir, sus_tok.sutun);
}

static Dugum *parse_deyim(Parser *p) {
    Token t = parser_simdiki(p);
    switch (t.tip) {
        case TOK_DEGISKEN:    return parse_degisken_deyimi(p);
        case TOK_VER:         return parse_ver_deyimi(p);
        case TOK_SOL_SUSLU:   return parse_blok(p);
        /* eger, iken, icin, esles, guvensiz: ADIM 10'da */
        default:
            return parse_ifade_veya_atama_deyimi(p);
    }
}

/* degisken_tanimi = "degisken" tanimlayici [":" tip] "=" ifade ";" */

static Dugum *parse_degisken_deyimi(Parser *p) {
    Token deg_tok = parser_simdiki(p);
    parser_ilerle(p);
    Token ad_tok = parser_bekle(p, TOK_TANIMLAYICI, "P080",
                                "degisken adi bekleniyor");

    Dugum *tip = NULL;
    if (parser_tuket(p, TOK_IKI_NOKTA)) {
        tip = parse_tip(p);
    }
    parser_bekle(p, TOK_ESIT, "P081",
                 "'=' bekleniyor (degisken degeri)");
    Dugum *deger = parse_ifade(p);
    parser_bekle(p, TOK_NOKTALI_VIRGUL, "P082", "';' bekleniyor");

    Dugum *d = dugum_olustur(p->arena, DUGUM_DEGISKEN,
                             deg_tok.satir, deg_tok.sutun);
    if (!d) return NULL;
    d->veri.degisken.ad =
        ast_string_kopyala(p->arena, ad_tok.baslangic, ad_tok.uzunluk);
    d->veri.degisken.ad_uzunluk = ad_tok.uzunluk;
    d->veri.degisken.tip = tip;
    d->veri.degisken.deger = deger;
    return d;
}

/* ver_deyimi = "ver" [ifade] ";" */

static Dugum *parse_ver_deyimi(Parser *p) {
    Token ver_tok = parser_simdiki(p);
    parser_ilerle(p);

    Dugum *deger = NULL;
    if (!parser_eslesir(p, TOK_NOKTALI_VIRGUL)) {
        deger = parse_ifade(p);
    }
    parser_bekle(p, TOK_NOKTALI_VIRGUL, "P090", "';' bekleniyor");
    return dugum_ver(p->arena, deger, ver_tok.satir, ver_tok.sutun);
}

/* ifade veya atama: ifade parse, sonra '=' varsa atama, ';' varsa ifade_deyimi */

static Dugum *parse_ifade_veya_atama_deyimi(Parser *p) {
    int satir = p->simdiki.satir;
    int sutun = p->simdiki.sutun;
    Dugum *ifade = parse_ifade(p);

    if (parser_tuket(p, TOK_ESIT)) {
        Dugum *deger = parse_ifade(p);
        parser_bekle(p, TOK_NOKTALI_VIRGUL, "P100", "';' bekleniyor");
        Dugum *d = dugum_olustur(p->arena, DUGUM_ATAMA, satir, sutun);
        if (!d) return NULL;
        d->veri.atama.hedef = ifade;
        d->veri.atama.deger = deger;
        return d;
    }
    parser_bekle(p, TOK_NOKTALI_VIRGUL, "P101", "';' bekleniyor");
    Dugum *d = dugum_olustur(p->arena, DUGUM_IFADE_DEYIMI, satir, sutun);
    if (!d) return NULL;
    d->veri.ifade_deyimi.ifade = ifade;
    return d;
}

/* === Program (top-level) === */

Dugum *parser_calistir(Parser *p) {
    Liste uyeler;
    liste_baslat(&uyeler);

    int baslangic_satir = p->simdiki.satir;
    int baslangic_sutun = p->simdiki.sutun;

    while (!parser_eslesir(p, TOK_DOSYA_SONU)) {
        Token once = parser_simdiki(p);
        Dugum *uye = parse_ust_oge(p);
        if (uye) liste_ekle(&uyeler, p->arena, uye);
        /* Sonsuz dongu korumasi */
        if (parser_simdiki(p).baslangic == once.baslangic &&
            !parser_eslesir(p, TOK_DOSYA_SONU)) {
            parser_ilerle(p);
        }
        if (p->hata_sayisi >= PARSER_MAX_HATA) break;
    }

    return dugum_program(p->arena,
                         liste_array_yap(&uyeler, p->arena),
                         uyeler.sayi,
                         baslangic_satir, baslangic_sutun);
}
