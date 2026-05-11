#include "parser.h"
#include "hata.h"

#include <string.h>
#include <stdio.h>

/* === Linked list yardimcilari (parse-zamani cocuk listeleri) ===
 * parser.h'da declare — ifade.c de kullanir. */

void liste_baslat(Liste *l) {
    l->bas = NULL;
    l->son = NULL;
    l->sayi = 0;
}

void liste_ekle(Liste *l, Arena *a, Dugum *d) {
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

Dugum **liste_array_yap(const Liste *l, Arena *a) {
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
    p->yapi_olusturma_izni = 1;
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

void parser_buyuk_ayir(Parser *p) {
    /* p->simdiki TOK_SAGA_KAYDIR ('>>') ise, onu '>' (TOK_BUYUK) yap
     * ve sonraki olarak da '>' yerlestir. Generic kapanis icin gerekli. */
    if (p->simdiki.tip != TOK_SAGA_KAYDIR) return;

    Token sonraki_buyuk = p->simdiki;
    sonraki_buyuk.baslangic += 1;
    sonraki_buyuk.uzunluk = 1;
    sonraki_buyuk.sutun += 1;
    sonraki_buyuk.tip = TOK_BUYUK;

    p->simdiki.uzunluk = 1;
    p->simdiki.tip = TOK_BUYUK;

    /* Sonraki'yi (eger varsa) korumak icin fallback. Su an siradakini
     * uretiyoruz: var olan sonraki'yi siliyoruz cunku farkli olabilir. */
    p->sonraki = sonraki_buyuk;
    p->sonraki_var = 1;
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

static Dugum *parse_deyim(Parser *p);
static Dugum *parse_ust_oge(Parser *p);
static Dugum *parse_islev_tanimi(Parser *p);
static Dugum *parse_yapi_tanimi(Parser *p);
static Dugum *parse_ozellik_tanimi(Parser *p);
static Dugum *parse_uygula_tanimi(Parser *p);
static char **parse_tip_param_listesi_genis(Parser *p, int *out_sayi,
                                            Dugum ****out_boundlar,
                                            int **out_bound_sayilari);
static Dugum *parse_kullan(Parser *p);
static Dugum *parse_disa(Parser *p);
static Dugum *parse_modul_tanimi(Parser *p);
static Dugum *parse_sabit_tanimi(Parser *p);
static Dugum *parse_degisken_deyimi(Parser *p);
static Dugum *parse_ver_deyimi(Parser *p);
static Dugum *parse_ifade_veya_atama_deyimi(Parser *p);
static Dugum *parse_eger_deyimi(Parser *p);
static Dugum *parse_iken_deyimi(Parser *p);
static Dugum *parse_icin_deyimi(Parser *p);
static Dugum *parse_esles_deyimi(Parser *p);
static Dugum *parse_guvensiz_blogu(Parser *p);
static Dugum *parse_desen(Parser *p);
static Dugum *parse_esles_kolu(Parser *p);

/* === Parametre (public — ifade.c lambda icin kullanir) === */

Dugum *parse_parametre(Parser *p) {
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
 * islev_tanimi = "islev" tanimlayici "(" [parametreler] ")" ["->" tip] blok
 *
 * Eger imza_yeterli=1: gövde opsiyonel (";" ile imza, "{" ile tanim).
 * Özellik gövdesi ve uygula imzalari icin kullanilir. */

static Dugum *parse_islev_genel(Parser *p, int imza_yeterli) {
    Token islev_tok = parser_simdiki(p);
    parser_ilerle(p);  /* 'islev' */

    Token ad_tok = parser_bekle(p, TOK_TANIMLAYICI, "P014",
                                "islev adi bekleniyor");

    /* Generic tip parametreleri: islev<T, U: Bound>(...) opsiyonel */
    int tip_param_sayi = 0;
    Dugum ***tip_param_boundlari = NULL;
    int *tip_param_bound_sayilari = NULL;
    char **tip_paramlar = parse_tip_param_listesi_genis(p, &tip_param_sayi,
        &tip_param_boundlari, &tip_param_bound_sayilari);

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
    } else if (imza_yeterli && parser_tuket(p, TOK_NOKTALI_VIRGUL)) {
        /* Imza yeterli — govde NULL kalir */
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
    d->veri.islev.tip_paramlar = tip_paramlar;
    d->veri.islev.tip_param_sayi = tip_param_sayi;
    d->veri.islev.tip_param_boundlari = tip_param_boundlari;
    d->veri.islev.tip_param_bound_sayilari = tip_param_bound_sayilari;
    d->veri.islev.parametreler = liste_array_yap(&params, p->arena);
    d->veri.islev.param_sayi = params.sayi;
    d->veri.islev.donus_tipi = donus;
    d->veri.islev.govde = govde;
    return d;
}

static Dugum *parse_islev_tanimi(Parser *p) {
    return parse_islev_genel(p, 0);
}

/* === Generic tip parametre listesi yardimcisi ===
 *   < T1, T2, ... >
 *   < T1: Bound1 + Bound2, T2: Bound3, T3 >    (bound'lar opsiyonel)
 *
 * Eger '<' yoksa NULL doner ve *out_sayi = 0.
 *
 * Bound listeleri *out_boundlar (NULL geçilebilir) ve *out_bound_sayilari
 * (NULL geçilebilir) yoluyla dondurulur. Her ikisi paralel dizilerdir;
 * bound olmayan parametre icin sayisi 0 olur.
 */

static char **parse_tip_param_listesi_genis(Parser *p, int *out_sayi,
                                            Dugum ****out_boundlar,
                                            int **out_bound_sayilari) {
    *out_sayi = 0;
    if (out_boundlar) *out_boundlar = NULL;
    if (out_bound_sayilari) *out_bound_sayilari = NULL;
    if (!parser_tuket(p, TOK_KUCUK)) return NULL;

    typedef struct BLink {
        Dugum *bound;
        struct BLink *son;
    } BLink;
    typedef struct PLink {
        const char *bas;
        int uz;
        BLink *bound_bas;
        BLink *bound_son;
        int bound_sayi;
        struct PLink *son;
    } PLink;
    PLink *bas = NULL;
    PLink *son_link = NULL;

    if (!parser_eslesir(p, TOK_BUYUK)) {
        do {
            if (parser_eslesir(p, TOK_BUYUK)) break;
            Token t = parser_bekle(p, TOK_TANIMLAYICI, "P024",
                                    "tip parametre adi bekleniyor");
            PLink *link = (PLink *)arena_ayir_sifir(p->arena, sizeof(PLink));
            if (!link) continue;
            link->bas = t.baslangic;
            link->uz = t.uzunluk;
            link->bound_sayi = 0;

            /* Opsiyonel bound listesi: ":" bound1 ["+" bound2 ...] */
            if (parser_tuket(p, TOK_IKI_NOKTA)) {
                do {
                    Dugum *bound = parse_tip(p);
                    BLink *bl = (BLink *)arena_ayir(p->arena, sizeof(BLink));
                    if (bl) {
                        bl->bound = bound;
                        bl->son = NULL;
                        if (link->bound_son) link->bound_son->son = bl;
                        else link->bound_bas = bl;
                        link->bound_son = bl;
                        link->bound_sayi++;
                    }
                } while (parser_tuket(p, TOK_ARTI));
            }

            if (son_link) son_link->son = link;
            else bas = link;
            son_link = link;
            (*out_sayi)++;
        } while (parser_tuket(p, TOK_VIRGUL));
    }
    parser_buyuk_ayir(p);
    parser_bekle(p, TOK_BUYUK, "P025",
                 "tip parametre listesinde '>' bekleniyor");

    if (*out_sayi == 0) return NULL;
    char **arr = (char **)arena_ayir(p->arena,
                                      sizeof(char *) * (size_t)(*out_sayi));
    if (!arr) return NULL;

    /* Bound dizilerini kur (caller istediyse) */
    Dugum ***boundlar = NULL;
    int *bound_sayilari = NULL;
    if (out_boundlar) {
        boundlar = (Dugum ***)arena_ayir_sifir(p->arena,
            sizeof(Dugum **) * (size_t)(*out_sayi));
    }
    if (out_bound_sayilari) {
        bound_sayilari = (int *)arena_ayir_sifir(p->arena,
            sizeof(int) * (size_t)(*out_sayi));
    }

    int i = 0;
    for (PLink *l = bas; l; l = l->son, i++) {
        arr[i] = ast_string_kopyala(p->arena, l->bas, l->uz);
        if (bound_sayilari) bound_sayilari[i] = l->bound_sayi;
        if (boundlar && l->bound_sayi > 0) {
            Dugum **bda = (Dugum **)arena_ayir(p->arena,
                sizeof(Dugum *) * (size_t)l->bound_sayi);
            if (bda) {
                int j = 0;
                for (BLink *bl = l->bound_bas; bl; bl = bl->son) {
                    bda[j++] = bl->bound;
                }
                boundlar[i] = bda;
            }
        }
    }

    if (out_boundlar) *out_boundlar = boundlar;
    if (out_bound_sayilari) *out_bound_sayilari = bound_sayilari;
    return arr;
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

    /* Generic tip parametreleri: <T1, T2, ...> opsiyonel (bound destekli) */
    int tip_param_sayi = 0;
    Dugum ***tip_param_boundlari = NULL;
    int *tip_param_bound_sayilari = NULL;
    char **tip_paramlar = parse_tip_param_listesi_genis(p, &tip_param_sayi,
        &tip_param_boundlari, &tip_param_bound_sayilari);

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
    d->veri.yapi.tip_paramlar = tip_paramlar;
    d->veri.yapi.tip_param_sayi = tip_param_sayi;
    d->veri.yapi.tip_param_boundlari = tip_param_boundlari;
    d->veri.yapi.tip_param_bound_sayilari = tip_param_bound_sayilari;
    d->veri.yapi.alanlar = liste_array_yap(&alanlar, p->arena);
    d->veri.yapi.alan_sayi = alanlar.sayi;
    return d;
}

/* === Ozellik (trait) tanimi ===
 * ozellik_tanimi = "ozellik" tanimlayici [tip_param_listesi] "{" uyeler "}"
 *
 * Govde icindeki her uye bir islev imzasi veya tam tanimi olabilir:
 *   islev m() -> tam32;            (imza)
 *   islev m() -> tam32 { ... }     (default impl)
 */

static Dugum *parse_ozellik_tanimi(Parser *p) {
    Token ozellik_tok = parser_simdiki(p);
    parser_ilerle(p);  /* 'ozellik' */

    Token ad_tok = parser_bekle(p, TOK_TANIMLAYICI, "P200",
                                "ozellik adi bekleniyor");

    int tip_param_sayi = 0;
    Dugum ***tip_param_boundlari = NULL;
    int *tip_param_bound_sayilari = NULL;
    char **tip_paramlar = parse_tip_param_listesi_genis(p, &tip_param_sayi,
        &tip_param_boundlari, &tip_param_bound_sayilari);

    parser_bekle(p, TOK_SOL_SUSLU, "P201",
                 "ozellik govdesi icin '{' bekleniyor");

    Liste uyeler;
    liste_baslat(&uyeler);
    while (!parser_eslesir(p, TOK_SAG_SUSLU) &&
           !parser_eslesir(p, TOK_DOSYA_SONU)) {
        Token sm = parser_simdiki(p);
        if (sm.tip == TOK_ISLEV) {
            Dugum *m = parse_islev_genel(p, 1);  /* imza yeterli */
            if (m) liste_ekle(&uyeler, p->arena, m);
        } else {
            parser_hata(p, sm, "P202",
                "ozellik govdesinde 'islev' bekleniyor", NULL);
            parser_panik_sync(p);
            break;
        }
    }
    parser_bekle(p, TOK_SAG_SUSLU, "P203",
                 "ozellik govde sonu '}' bekleniyor");

    Dugum *d = dugum_olustur(p->arena, DUGUM_OZELLIK,
                             ozellik_tok.satir, ozellik_tok.sutun);
    if (!d) return NULL;
    d->veri.ozellik.ad =
        ast_string_kopyala(p->arena, ad_tok.baslangic, ad_tok.uzunluk);
    d->veri.ozellik.ad_uzunluk = ad_tok.uzunluk;
    d->veri.ozellik.tip_paramlar = tip_paramlar;
    d->veri.ozellik.tip_param_sayi = tip_param_sayi;
    d->veri.ozellik.tip_param_boundlari = tip_param_boundlari;
    d->veri.ozellik.tip_param_bound_sayilari = tip_param_bound_sayilari;
    d->veri.ozellik.uyeler = liste_array_yap(&uyeler, p->arena);
    d->veri.ozellik.uye_sayi = uyeler.sayi;
    return d;
}

/* === Uygula (impl) ===
 *
 * uygula [tip_param_listesi] tip               "{" islevler "}"   (inherent)
 * uygula [tip_param_listesi] tip "icin" tip2   "{" islevler "}"   (trait impl)
 *
 * "icin" anahtar kelimesi ozellik(trait) ile hedef tip arasini ayirir.
 * Sintaks: ilk olarak parse_tip, eger sonrasi TOK_ICIN ise ikinci parse_tip.
 *
 * AST:
 *   uygula.ozellikler[] — varsa trait yollari (DUGUM_TIP_BASIT/KULLANICI), simdilik 1 tane
 *   uygula.tip          — hedef tip
 *   uygula.islevler[]   — gerçeklenen metotlar (govdeli)
 */

static Dugum *parse_uygula_tanimi(Parser *p) {
    Token uygula_tok = parser_simdiki(p);
    parser_ilerle(p);  /* 'uygula' */

    int tip_param_sayi = 0;
    Dugum ***tip_param_boundlari = NULL;
    int *tip_param_bound_sayilari = NULL;
    char **tip_paramlar = parse_tip_param_listesi_genis(p, &tip_param_sayi,
        &tip_param_boundlari, &tip_param_bound_sayilari);

    Dugum *birinci_tip = parse_tip(p);
    Dugum *hedef_tip = birinci_tip;
    Dugum **ozellikler = NULL;
    int ozellik_sayi = 0;

    if (parser_tuket(p, TOK_ICIN)) {
        /* trait impl: birinci_tip aslinda ozellik, hedef yeni */
        hedef_tip = parse_tip(p);
        ozellikler = (Dugum **)arena_ayir(p->arena, sizeof(Dugum *));
        if (ozellikler) {
            ozellikler[0] = birinci_tip;
            ozellik_sayi = 1;
        }
    }

    parser_bekle(p, TOK_SOL_SUSLU, "P210",
                 "uygula govdesi icin '{' bekleniyor");

    Liste islevler;
    liste_baslat(&islevler);
    while (!parser_eslesir(p, TOK_SAG_SUSLU) &&
           !parser_eslesir(p, TOK_DOSYA_SONU)) {
        Token sm = parser_simdiki(p);
        if (sm.tip == TOK_ISLEV) {
            Dugum *m = parse_islev_tanimi(p);  /* govde zorunlu */
            if (m) liste_ekle(&islevler, p->arena, m);
        } else {
            parser_hata(p, sm, "P211",
                "uygula govdesinde 'islev' bekleniyor", NULL);
            parser_panik_sync(p);
            break;
        }
    }
    parser_bekle(p, TOK_SAG_SUSLU, "P212",
                 "uygula govde sonu '}' bekleniyor");

    Dugum *d = dugum_olustur(p->arena, DUGUM_UYGULA,
                             uygula_tok.satir, uygula_tok.sutun);
    if (!d) return NULL;
    d->veri.uygula.tip_paramlar = tip_paramlar;
    d->veri.uygula.tip_param_sayi = tip_param_sayi;
    d->veri.uygula.tip_param_boundlari = tip_param_boundlari;
    d->veri.uygula.tip_param_bound_sayilari = tip_param_bound_sayilari;
    d->veri.uygula.tip = hedef_tip;
    d->veri.uygula.ozellikler = ozellikler;
    d->veri.uygula.ozellik_sayi = ozellik_sayi;
    d->veri.uygula.islevler = liste_array_yap(&islevler, p->arena);
    d->veri.uygula.islev_sayi = islevler.sayi;
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
        case TOK_OZELLIK: return parse_ozellik_tanimi(p);
        case TOK_UYGULA:  return parse_uygula_tanimi(p);
        case TOK_KULLAN:  return parse_kullan(p);
        case TOK_DISA:    return parse_disa(p);
        case TOK_MODUL:   return parse_modul_tanimi(p);
        case TOK_SABIT:   return parse_sabit_tanimi(p);
        default:
            parser_hata(p, t, "P001",
                "ust duzey tanim bekleniyor (islev/yapi/ozellik/uygula/kullan/disa/modul/sabit)",
                NULL);
            parser_panik_sync(p);
            return NULL;
    }
}

/* === Blok ve deyimler === */
/* parse_blok public — ifade.c lambda govdesi icin kullanir */

Dugum *parse_blok(Parser *p) {
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
        case TOK_EGER:        return parse_eger_deyimi(p);
        case TOK_IKEN:        return parse_iken_deyimi(p);
        case TOK_ICIN:        return parse_icin_deyimi(p);
        case TOK_ESLES:       return parse_esles_deyimi(p);
        case TOK_GUVENSIZ:    return parse_guvensiz_blogu(p);
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

/* === Kondisyonel ifade yardimcisi ===
 * eger/iken/icin/esles sonrasi ifade parse ederken, tanimlayici sonrasi
 * '{' yapi_olusturma yerine blok basi sayilmali. Flag ile yonetilir. */

static Dugum *parse_kosul_ifadesi(Parser *p) {
    int eski_izin = p->yapi_olusturma_izni;
    p->yapi_olusturma_izni = 0;
    Dugum *d = parse_ifade(p);
    p->yapi_olusturma_izni = eski_izin;
    return d;
}

/* === Eger deyimi ===
 * eger_deyimi = "eger" ifade blok { "degilse" "eger" ifade blok } [ "degilse" blok ] */

static Dugum *parse_eger_deyimi(Parser *p) {
    Token eger_tok = parser_simdiki(p);
    parser_ilerle(p);  /* 'eger' */

    Dugum *kosul = parse_kosul_ifadesi(p);
    Dugum *gozdoldur = parse_blok(p);

    Dugum *yan = NULL;
    if (parser_tuket(p, TOK_DEGILSE)) {
        if (parser_eslesir(p, TOK_EGER)) {
            /* degilse eger -> recursive (else if zinciri) */
            yan = parse_eger_deyimi(p);
        } else {
            yan = parse_blok(p);
        }
    }

    return dugum_eger(p->arena, kosul, gozdoldur, yan,
                      eger_tok.satir, eger_tok.sutun);
}

/* === Iken deyimi ===
 * iken_deyimi = "iken" ifade blok */

static Dugum *parse_iken_deyimi(Parser *p) {
    Token iken_tok = parser_simdiki(p);
    parser_ilerle(p);

    Dugum *kosul = parse_kosul_ifadesi(p);
    Dugum *govde = parse_blok(p);

    Dugum *d = dugum_olustur(p->arena, DUGUM_IKEN,
                             iken_tok.satir, iken_tok.sutun);
    if (!d) return NULL;
    d->veri.iken.kosul = kosul;
    d->veri.iken.govde = govde;
    return d;
}

/* === Icin deyimi ===
 * icin_deyimi = "icin" tanimlayici ":" ifade blok */

static Dugum *parse_icin_deyimi(Parser *p) {
    Token icin_tok = parser_simdiki(p);
    parser_ilerle(p);

    Token deg_tok = parser_bekle(p, TOK_TANIMLAYICI, "P200",
                                  "degisken adi bekleniyor (icin dongusu)");
    parser_bekle(p, TOK_IKI_NOKTA, "P201", "':' bekleniyor (icin dongusu)");
    Dugum *koleksiyon = parse_kosul_ifadesi(p);
    Dugum *govde = parse_blok(p);

    Dugum *d = dugum_olustur(p->arena, DUGUM_ICIN,
                             icin_tok.satir, icin_tok.sutun);
    if (!d) return NULL;
    d->veri.icin.degisken_adi =
        ast_string_kopyala(p->arena, deg_tok.baslangic, deg_tok.uzunluk);
    d->veri.icin.degisken_adi_uzunluk = deg_tok.uzunluk;
    d->veri.icin.koleksiyon = koleksiyon;
    d->veri.icin.govde = govde;
    return d;
}

/* === Desen ve esles kolu === */

static Dugum *parse_desen(Parser *p) {
    Token t = parser_simdiki(p);

    /* Joker: tanımlayıcı "_" */
    if (t.tip == TOK_TANIMLAYICI && t.uzunluk == 1 && t.baslangic[0] == '_') {
        parser_ilerle(p);
        return dugum_olustur(p->arena, DUGUM_DESEN_JOKER, t.satir, t.sutun);
    }

    /* Tanımlayıcı veya yapıcı (TipAdi(alt_desenler)).
     * Anahtar kelime desenleri (hic, deger) da burada — secimlik<T>
     * pattern matching icin: 'hic' (None) ve 'deger(v)' (Some(v)). */
    if (t.tip == TOK_TANIMLAYICI || t.tip == TOK_HIC || t.tip == TOK_DEGER) {
        const char *ad_baslangic = t.baslangic;
        int ad_uzunluk = t.uzunluk;
        int satir = t.satir;
        int sutun = t.sutun;
        parser_ilerle(p);

        if (parser_eslesir(p, TOK_SOL_PAREN)) {
            parser_ilerle(p);
            Liste alt;
            liste_baslat(&alt);
            if (!parser_eslesir(p, TOK_SAG_PAREN)) {
                do {
                    if (parser_eslesir(p, TOK_SAG_PAREN)) break;
                    Dugum *alt_d = parse_desen(p);
                    liste_ekle(&alt, p->arena, alt_d);
                } while (parser_tuket(p, TOK_VIRGUL));
            }
            parser_bekle(p, TOK_SAG_PAREN, "P210",
                         "yapici deseninde ')' bekleniyor");

            Dugum *d = dugum_olustur(p->arena, DUGUM_DESEN_YAPICI,
                                     satir, sutun);
            if (d) {
                d->veri.desen_yapici.ad =
                    ast_string_kopyala(p->arena, ad_baslangic, ad_uzunluk);
                d->veri.desen_yapici.ad_uzunluk = ad_uzunluk;
                d->veri.desen_yapici.alt_desenler =
                    liste_array_yap(&alt, p->arena);
                d->veri.desen_yapici.sayi = alt.sayi;
            }
            return d;
        }

        Dugum *d = dugum_olustur(p->arena, DUGUM_DESEN_TANIMLAYICI,
                                 satir, sutun);
        if (d) {
            d->veri.desen_tanimlayici.ad =
                ast_string_kopyala(p->arena, ad_baslangic, ad_uzunluk);
            d->veri.desen_tanimlayici.ad_uzunluk = ad_uzunluk;
        }
        return d;
    }

    /* Literal desenler — parse_ifade safe (operator yok arasinda, => durdurur) */
    if (t.tip == TOK_TAMSAYI || t.tip == TOK_ONDALIK ||
        t.tip == TOK_METIN   || t.tip == TOK_KARAKTER ||
        t.tip == TOK_DOGRU   || t.tip == TOK_YANLIS || t.tip == TOK_BOS) {
        Dugum *lit = parse_ifade(p);
        Dugum *d = dugum_olustur(p->arena, DUGUM_DESEN_LITERAL,
                                 t.satir, t.sutun);
        if (d) d->veri.desen_literal.deger = lit;
        return d;
    }

    parser_hata(p, t, "P211", "desen bekleniyor", NULL);
    return dugum_hata(p->arena, t.satir, t.sutun);
}

static Dugum *parse_esles_kolu(Parser *p) {
    int satir = p->simdiki.satir;
    int sutun = p->simdiki.sutun;
    Dugum *desen = parse_desen(p);
    parser_bekle(p, TOK_KALIN_OK, "P220",
                 "esles kolunda '=>' bekleniyor");

    Dugum *govde;
    if (parser_eslesir(p, TOK_SOL_SUSLU)) {
        govde = parse_blok(p);
    } else {
        govde = parse_ifade(p);
        parser_bekle(p, TOK_NOKTALI_VIRGUL, "P221",
                     "esles kolu sonunda ';' bekleniyor");
    }

    Dugum *d = dugum_olustur(p->arena, DUGUM_ESLES_KOLU, satir, sutun);
    if (!d) return NULL;
    d->veri.esles_kolu.desen = desen;
    d->veri.esles_kolu.govde = govde;
    return d;
}

/* === Esles deyimi ===
 * esles_deyimi = "esles" ifade "{" { esles_kolu } "}" */

static Dugum *parse_esles_deyimi(Parser *p) {
    Token esles_tok = parser_simdiki(p);
    parser_ilerle(p);

    Dugum *deger = parse_kosul_ifadesi(p);
    parser_bekle(p, TOK_SOL_SUSLU, "P230",
                 "esles govdesi icin '{' bekleniyor");

    Liste kollar;
    liste_baslat(&kollar);
    while (!parser_eslesir(p, TOK_SAG_SUSLU) &&
           !parser_eslesir(p, TOK_DOSYA_SONU)) {
        Dugum *kol = parse_esles_kolu(p);
        if (kol) liste_ekle(&kollar, p->arena, kol);
        if (p->hata_sayisi >= PARSER_MAX_HATA) break;
    }
    parser_bekle(p, TOK_SAG_SUSLU, "P231",
                 "esles sonunda '}' bekleniyor");

    Dugum *d = dugum_olustur(p->arena, DUGUM_ESLES,
                             esles_tok.satir, esles_tok.sutun);
    if (!d) return NULL;
    d->veri.esles.deger = deger;
    d->veri.esles.kollar = liste_array_yap(&kollar, p->arena);
    d->veri.esles.kol_sayi = kollar.sayi;
    return d;
}

/* === Guvensiz blok ===
 * guvensiz_blogu = "guvensiz" [ "[" tanimlayici ":" metin_literali "]" ] blok */

static Dugum *parse_guvensiz_blogu(Parser *p) {
    Token guv_tok = parser_simdiki(p);
    parser_ilerle(p);

    const char *aciklama_ad = NULL;
    int aciklama_ad_uz = 0;
    const char *aciklama_metin = NULL;
    int aciklama_metin_uz = 0;

    if (parser_tuket(p, TOK_SOL_KOSELI)) {
        Token ad_tok = parser_bekle(p, TOK_TANIMLAYICI, "P240",
                                     "aciklama adi bekleniyor");
        parser_bekle(p, TOK_IKI_NOKTA, "P241", "':' bekleniyor");
        Token metin_tok = parser_bekle(p, TOK_METIN, "P242",
                                        "metin literali bekleniyor");
        parser_bekle(p, TOK_SAG_KOSELI, "P243",
                     "guvensiz aciklamasinda ']' bekleniyor");

        aciklama_ad = ast_string_kopyala(p->arena,
                                          ad_tok.baslangic, ad_tok.uzunluk);
        aciklama_ad_uz = ad_tok.uzunluk;
        /* metin tirnaklar dahil — icerigi cikar */
        int ic_uz = metin_tok.uzunluk - 2;
        if (ic_uz < 0) ic_uz = 0;
        aciklama_metin = ast_string_kopyala(p->arena,
                                             metin_tok.baslangic + 1, ic_uz);
        aciklama_metin_uz = ic_uz;
    }

    Dugum *blok = parse_blok(p);

    Dugum *d = dugum_olustur(p->arena, DUGUM_GUVENSIZ,
                             guv_tok.satir, guv_tok.sutun);
    if (!d) return NULL;
    d->veri.guvensiz.aciklama_ad = aciklama_ad;
    d->veri.guvensiz.aciklama_ad_uzunluk = aciklama_ad_uz;
    d->veri.guvensiz.aciklama_metin = aciklama_metin;
    d->veri.guvensiz.aciklama_metin_uzunluk = aciklama_metin_uz;
    d->veri.guvensiz.blok = blok;
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
