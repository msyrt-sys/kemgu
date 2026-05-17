#include "tekkez_kontrol.h"
#include "hata.h"
#include "tip_kontrol.h"

#include <string.h>
#include <stdlib.h>

/*
 * Affine deger izleme.
 *
 * Strateji: AST visitor. Her DUGUM_DEGISKEN/PARAMETRE eklendiginde tip
 * kontrol et: tekkez ise LinKayit ekle (LIN_AKTIF).
 * Her TANIMLAYICI kullaniminda: linear ise tuket veya hata.
 *
 * Scope girisi/cikisinda: blok sonunda aktif tum LIN_AKTIF kayitlar
 * = T041 (hic tuketilmedi) hatasi.
 *
 * 'kullan(x)' ve 'imha(x)' cagrilari ozel: arg DUGUM_TANIMLAYICI ise
 * o ismi LIN_TUKETILDI yap.
 *
 * Diger TANIMLAYICI kullanimi: linear ise T040 (iki kez tuketim) eger
 * zaten LIN_TUKETILDI; aksi halde sessiz drop sayilir (T042 — '_'
 * benzeri kaybolma).
 *
 * Onemli: bu implementasyon Spec B v1'in cekirdek davranisini saglar.
 * Closure linearity (B.5) ve region etkilesimi (B.3) gelecek
 * iterasyonlarda eklenir.
 */

static void tk_hata(TekKezKontrol *tk, const Dugum *d,
                     const char *kod, const char *mesaj,
                     const char *ipucu) {
    if (!tk || !d) return;
    tk->hata_sayisi++;
    hata_raporla(tk->dosya_adi, tk->kaynak,
                 d->satir, d->sutun, kod, mesaj, ipucu);
}

void tekkez_kontrol_baslat(TekKezKontrol *tk, Arena *a, Scope *g,
                            const char *dosya_adi, const char *kaynak) {
    tk->arena = a;
    tk->global_scope = g;
    tk->hata_sayisi = 0;
    tk->dosya_adi = dosya_adi;
    tk->kaynak = kaynak;
    tk->aktif = NULL;
    tk->aktif_mi = 0;  /* default kapali */
}

static int isim_esit(const char *a, int a_n, const char *b, int b_n) {
    return a_n == b_n && memcmp(a, b, (size_t)a_n) == 0;
}

static LinKayit *lin_bul(TekKezKontrol *tk, const char *ad, int n) {
    for (LinKayit *k = tk->aktif; k; k = k->sonraki) {
        if (isim_esit(k->ad, k->ad_uz, ad, n)) return k;
    }
    return NULL;
}

static void lin_ekle(TekKezKontrol *tk, const char *ad, int n,
                     int satir, int sutun) {
    LinKayit *k = (LinKayit *)arena_ayir(tk->arena, sizeof(LinKayit));
    if (!k) return;
    k->ad = ad;
    k->ad_uz = n;
    k->durum = LIN_AKTIF;
    k->satir = satir;
    k->sutun = sutun;
    k->sonraki = tk->aktif;
    tk->aktif = k;
}

/* Forward decls */
static void analiz_deyim(TekKezKontrol *tk, const Dugum *d);
static void analiz_ifade(TekKezKontrol *tk, const Dugum *d);
static int tekkez_tipi_mi_ast(const Dugum *tip_d);

/* AST tip dugumu tekkez<...> mi? */
static int tekkez_tipi_mi_ast(const Dugum *tip_d) {
    return tip_d && tip_d->tip == DUGUM_TIP_TEKKEZ;
}

/* '_' (joker) ile mi atandi? — KEMGU'da `_` parametre adi
 * olarak gecerli, atama ifadesinde lvalue olarak yok. Burada
 * DUGUM_DEGISKEN ad'i "_" mi diye bakariz. */
static int joker_baglama_mi(const Dugum *d) {
    if (d->tip != DUGUM_DEGISKEN) return 0;
    return d->veri.degisken.ad_uzunluk == 1 &&
           d->veri.degisken.ad[0] == '_';
}

/* Lambda body'de outer scope linear referans var mi?
 * Spec B.5: linear yakalayan kapanis kendisi linear olur.
 * Bu yardimci: body'i recursive gez, TANIMLAYICI'lar arasinda outer
 * lin_kayitlari ile eslesen tekkez var mi bak.
 * 'outer_bas' = lambda creation oncesi aktif listenin basi (cagiranin
 * kayitlari). Lambda param scope'undan ONCE olan kayitlar. */
static int outer_linear_referans_var(const Dugum *d, LinKayit *outer_bas,
                                       const Dugum *lambda) {
    if (!d) return 0;
    switch (d->tip) {
        case DUGUM_TANIMLAYICI: {
            const char *ad = d->veri.tanimlayici.metin;
            int n = d->veri.tanimlayici.uzunluk;
            /* Lambda param mi? — yakalama degil */
            for (int i = 0; i < lambda->veri.lambda.param_sayi; i++) {
                const Dugum *p = lambda->veri.lambda.parametreler[i];
                if (p->veri.parametre.ad_uzunluk == n &&
                    memcmp(p->veri.parametre.ad, ad, (size_t)n) == 0) {
                    return 0;
                }
            }
            /* outer kayitlarda var mi? (LinKayit listesi linked list,
             * lambda creation sirasinda outer_bas'tan basliyor) */
            for (LinKayit *k = outer_bas; k; k = k->sonraki) {
                if (k->ad_uz == n &&
                    memcmp(k->ad, ad, (size_t)n) == 0) {
                    return 1;
                }
            }
            return 0;
        }
        case DUGUM_IKILI:
            return outer_linear_referans_var(d->veri.ikili.sol, outer_bas, lambda) ||
                   outer_linear_referans_var(d->veri.ikili.sag, outer_bas, lambda);
        case DUGUM_TEKLI:
            return outer_linear_referans_var(d->veri.tekli.operand, outer_bas, lambda);
        case DUGUM_CAGRI: {
            if (outer_linear_referans_var(d->veri.cagri.hedef, outer_bas, lambda))
                return 1;
            for (int i = 0; i < d->veri.cagri.sayi; i++) {
                if (outer_linear_referans_var(d->veri.cagri.argumanlar[i],
                                                outer_bas, lambda)) return 1;
            }
            return 0;
        }
        case DUGUM_BLOK: {
            for (int i = 0; i < d->veri.blok.sayi; i++) {
                if (outer_linear_referans_var(d->veri.blok.deyimler[i],
                                                outer_bas, lambda)) return 1;
            }
            return 0;
        }
        case DUGUM_VER:
            return d->veri.ver.deger &&
                   outer_linear_referans_var(d->veri.ver.deger, outer_bas, lambda);
        case DUGUM_IFADE_DEYIMI:
            return outer_linear_referans_var(d->veri.ifade_deyimi.ifade,
                                              outer_bas, lambda);
        case DUGUM_DEGISKEN:
            return outer_linear_referans_var(d->veri.degisken.deger,
                                              outer_bas, lambda);
        case DUGUM_EGER:
            return outer_linear_referans_var(d->veri.eger.kosul, outer_bas, lambda) ||
                   outer_linear_referans_var(d->veri.eger.gozdoldur, outer_bas, lambda) ||
                   (d->veri.eger.yan &&
                    outer_linear_referans_var(d->veri.eger.yan, outer_bas, lambda));
        case DUGUM_IKEN:
            return outer_linear_referans_var(d->veri.iken.kosul, outer_bas, lambda) ||
                   outer_linear_referans_var(d->veri.iken.govde, outer_bas, lambda);
        default:
            return 0;
    }
}

/* CAGRI 'kullan' veya 'imha' mi? */
static const Dugum *cagri_tuketici_arg(const Dugum *d) {
    if (d->tip != DUGUM_CAGRI) return NULL;
    const Dugum *h = d->veri.cagri.hedef;
    if (!h || h->tip != DUGUM_TANIMLAYICI) return NULL;
    const char *ad = h->veri.tanimlayici.metin;
    int n = h->veri.tanimlayici.uzunluk;
    int kullan = (n == 6 && memcmp(ad, "kullan", 6) == 0);
    int imha = (n == 4 && memcmp(ad, "imha", 4) == 0);
    if (!kullan && !imha) return NULL;
    if (d->veri.cagri.sayi < 1) return NULL;
    /* Ilk arg tuketici hedef */
    return d->veri.cagri.argumanlar[0];
}

static void analiz_ifade(TekKezKontrol *tk, const Dugum *d) {
    if (!d) return;

    /* === 'kullan(x, ...)' veya 'imha(x)' — ozel tuketim === */
    const Dugum *tuketim = cagri_tuketici_arg(d);
    if (tuketim) {
        if (tuketim->tip == DUGUM_TANIMLAYICI) {
            LinKayit *k = lin_bul(tk,
                tuketim->veri.tanimlayici.metin,
                tuketim->veri.tanimlayici.uzunluk);
            if (k) {
                if (k->durum == LIN_TUKETILDI) {
                    tk_hata(tk, tuketim, "T040",
                            "linear deger iki kez tuketildi",
                            "Bir 'tekkez<T>' degeri sadece bir kez "
                            "'kullan' veya 'imha' edilebilir.");
                } else {
                    k->durum = LIN_TUKETILDI;
                }
            }
        }
        /* Cagri ic argumanlari (kalanlari) normal analiz */
        for (int i = 1; i < d->veri.cagri.sayi; i++) {
            analiz_ifade(tk, d->veri.cagri.argumanlar[i]);
        }
        return;
    }

    /* === Standart recursive === */
    switch (d->tip) {
        case DUGUM_TANIMLAYICI: {
            /* Linear deger TANIMLAYICI olarak baska yerde kullanildi —
             * bu sessiz drop ile sayilir. T042 baslat (gelecek strict
             * mode'da hata; su an uyari niteliginde — sadece tuketim
             * dis senaryo izle). */
            LinKayit *k = lin_bul(tk,
                d->veri.tanimlayici.metin,
                d->veri.tanimlayici.uzunluk);
            if (k && k->durum == LIN_AKTIF) {
                /* Kullanim olabilir veya silent drop — su an silent
                 * drop yasak ama "argument as expression" senaryosunda
                 * tuketim olabilir. Conservative: aktif kalsin, scope
                 * sonu kontrol et. */
                /* Su an yorum yok — gercek tuketim sadece kullan/imha. */
            }
            return;
        }
        case DUGUM_IKILI:
            analiz_ifade(tk, d->veri.ikili.sol);
            analiz_ifade(tk, d->veri.ikili.sag);
            return;
        case DUGUM_TEKLI:
            analiz_ifade(tk, d->veri.tekli.operand);
            return;
        case DUGUM_CAGRI:
            analiz_ifade(tk, d->veri.cagri.hedef);
            for (int i = 0; i < d->veri.cagri.sayi; i++) {
                analiz_ifade(tk, d->veri.cagri.argumanlar[i]);
            }
            return;
        case DUGUM_ERISIM:
            analiz_ifade(tk, d->veri.erisim.nesne);
            return;
        case DUGUM_INDEKS:
            analiz_ifade(tk, d->veri.indeks.nesne);
            analiz_ifade(tk, d->veri.indeks.indeks);
            return;
        case DUGUM_DIZI_OLUSTUR:
            for (int i = 0; i < d->veri.dizi_olustur.sayi; i++) {
                analiz_ifade(tk, d->veri.dizi_olustur.elemanlar[i]);
            }
            return;
        case DUGUM_YAPI_OLUSTUR:
            for (int i = 0; i < d->veri.yapi_olustur.alan_sayi; i++) {
                analiz_ifade(tk,
                    d->veri.yapi_olustur.alanlar[i]->veri.alan_atama.deger);
            }
            return;
        case DUGUM_LAMBDA: {
            /* B-Adim 5: lambda govdesi linear takibi.
             * Spec B.5: linear yakalayan kapanis kendisi linear olur — bu
             * inferans gelecek (kapanisi cagirip cagirmadigimizi izlemek
             * gerek). Su an: lambda parametresi tekkez ise lin_ekle ve
             * govdede tuketim takibi yap. */
            LinKayit *eski = tk->aktif;
            for (int i = 0; i < d->veri.lambda.param_sayi; i++) {
                const Dugum *p = d->veri.lambda.parametreler[i];
                if (p->veri.parametre.tip &&
                    p->veri.parametre.tip->tip == DUGUM_TIP_TEKKEZ) {
                    lin_ekle(tk, p->veri.parametre.ad,
                             p->veri.parametre.ad_uzunluk,
                             p->satir, p->sutun);
                }
            }
            if (d->veri.lambda.govde) {
                if (d->veri.lambda.govde->tip == DUGUM_BLOK) {
                    analiz_deyim(tk, d->veri.lambda.govde);
                } else {
                    analiz_ifade(tk, d->veri.lambda.govde);
                }
            }
            /* Lambda sonunda kontrol — lambda param tuketilmedi mi? */
            LinKayit *cur = tk->aktif;
            while (cur != eski) {
                if (cur->durum == LIN_AKTIF) {
                    Dugum pseudo;
                    memset(&pseudo, 0, sizeof(pseudo));
                    pseudo.tip = DUGUM_HATA;
                    pseudo.satir = cur->satir;
                    pseudo.sutun = cur->sutun;
                    tk_hata(tk, &pseudo, "T041",
                        "lambda govdesinde linear deger tuketilmedi",
                        "Lambda parametresi/yereli 'kullan' veya 'imha' "
                        "ile tuketilmeli.");
                }
                cur = cur->sonraki;
            }
            tk->aktif = eski;
            return;
        }
        default:
            return;
    }
}

static void analiz_deyim(TekKezKontrol *tk, const Dugum *d) {
    if (!d) return;
    switch (d->tip) {
        case DUGUM_DEGISKEN: {
            /* Joker baglama yasak (T042) — linear deger '_' ile drop edilemez */
            if (joker_baglama_mi(d) &&
                d->veri.degisken.tip &&
                tekkez_tipi_mi_ast(d->veri.degisken.tip)) {
                tk_hata(tk, d, "T042",
                        "linear deger '_' ile baglanamaz (sessiz drop)",
                        "'_' yerine 'kullan(x)' veya 'imha(x)' ile "
                        "deger tuketilmeli.");
                return;
            }

            /* === B-Adim 5 genisletme: closure-itself-linear inferansi
             * (Spec B.5) — strict sound model === */
            /* Deger lambda + body'de outer scope tekkez referansi varsa,
             * lambda binding kendisi linear isaretlenir. Outer tekkez
             * tuketim takibi lambda body'nin normal analizi tarafindan
             * yapilir (cagri_tuketici_arg outer'da LIN_TUKETILDI yapar).
             * Bu sayede double-consume olmaz. */
            if (d->veri.degisken.deger &&
                d->veri.degisken.deger->tip == DUGUM_LAMBDA) {
                LinKayit *outer_bas = tk->aktif;
                if (outer_linear_referans_var(
                        d->veri.degisken.deger->veri.lambda.govde,
                        outer_bas, d->veri.degisken.deger)) {
                    /* Lambda body'i normal analiz et — body icindeki
                     * kullan/imha outer'i tuketim olarak isaretler */
                    analiz_ifade(tk, d->veri.degisken.deger);
                    /* Lambda binding kendisi linear */
                    lin_ekle(tk, d->veri.degisken.ad,
                             d->veri.degisken.ad_uzunluk,
                             d->satir, d->sutun);
                    return;
                }
            }

            /* Deger ifadesini analiz et (varsa) */
            if (d->veri.degisken.deger) {
                analiz_ifade(tk, d->veri.degisken.deger);
            }
            /* Tip annotation tekkez<T> ise kayit ekle */
            if (d->veri.degisken.tip &&
                tekkez_tipi_mi_ast(d->veri.degisken.tip)) {
                lin_ekle(tk, d->veri.degisken.ad,
                         d->veri.degisken.ad_uzunluk,
                         d->satir, d->sutun);
            }
            return;
        }
        case DUGUM_ATAMA:
            analiz_ifade(tk, d->veri.atama.deger);
            return;
        case DUGUM_VER:
            if (d->veri.ver.deger) {
                analiz_ifade(tk, d->veri.ver.deger);
            }
            return;
        case DUGUM_EGER:
            analiz_ifade(tk, d->veri.eger.kosul);
            analiz_deyim(tk, d->veri.eger.gozdoldur);
            if (d->veri.eger.yan) {
                analiz_deyim(tk, d->veri.eger.yan);
            }
            return;
        case DUGUM_IKEN:
            analiz_ifade(tk, d->veri.iken.kosul);
            analiz_deyim(tk, d->veri.iken.govde);
            return;
        case DUGUM_ICIN:
            analiz_ifade(tk, d->veri.icin.koleksiyon);
            analiz_deyim(tk, d->veri.icin.govde);
            return;
        case DUGUM_ESLES:
            analiz_ifade(tk, d->veri.esles.deger);
            for (int i = 0; i < d->veri.esles.kol_sayi; i++) {
                analiz_deyim(tk,
                    d->veri.esles.kollar[i]->veri.esles_kolu.govde);
            }
            return;
        case DUGUM_BLOK: {
            /* Scope baslat — aktif listenin snapshot'i */
            LinKayit *eski_bas = tk->aktif;
            for (int i = 0; i < d->veri.blok.sayi; i++) {
                analiz_deyim(tk, d->veri.blok.deyimler[i]);
            }
            /* Scope sonu — sadece bu scope'ta eklenenleri kontrol et */
            LinKayit *cur = tk->aktif;
            while (cur != eski_bas) {
                if (cur->durum == LIN_AKTIF) {
                    /* Pseudo-Dugum: konum kullaniyoruz */
                    Dugum pseudo;
                    memset(&pseudo, 0, sizeof(pseudo));
                    pseudo.tip = DUGUM_HATA;
                    pseudo.satir = cur->satir;
                    pseudo.sutun = cur->sutun;
                    tk_hata(tk, &pseudo, "T041",
                            "linear deger hic tuketilmedi",
                            "Scope sonunda 'tekkez<T>' deger 'kullan' "
                            "veya 'imha' ile tuketilmeli.");
                }
                cur = cur->sonraki;
            }
            tk->aktif = eski_bas;
            return;
        }
        case DUGUM_GUVENSIZ:
            analiz_deyim(tk, d->veri.guvensiz.blok);
            return;
        case DUGUM_IFADE_DEYIMI:
            analiz_ifade(tk, d->veri.ifade_deyimi.ifade);
            return;
        default:
            return;
    }
}

static void analiz_tanim(TekKezKontrol *tk, const Dugum *d) {
    if (!d) return;
    if (d->tip == DUGUM_DISA && d->veri.disa.tanim) {
        analiz_tanim(tk, d->veri.disa.tanim);
        return;
    }
    if (d->tip == DUGUM_ISLEV) {
        /* Yeni scope: parametreleri ekle, govde analiz */
        LinKayit *eski = tk->aktif;
        for (int i = 0; i < d->veri.islev.param_sayi; i++) {
            const Dugum *p = d->veri.islev.parametreler[i];
            if (p->veri.parametre.tip &&
                tekkez_tipi_mi_ast(p->veri.parametre.tip)) {
                lin_ekle(tk, p->veri.parametre.ad,
                         p->veri.parametre.ad_uzunluk,
                         p->satir, p->sutun);
            }
        }
        if (d->veri.islev.govde) {
            analiz_deyim(tk, d->veri.islev.govde);
        }
        /* Islev sonunda aktif kayitlari kontrol et */
        LinKayit *cur = tk->aktif;
        while (cur != eski) {
            if (cur->durum == LIN_AKTIF) {
                Dugum pseudo;
                memset(&pseudo, 0, sizeof(pseudo));
                pseudo.tip = DUGUM_HATA;
                pseudo.satir = cur->satir;
                pseudo.sutun = cur->sutun;
                tk_hata(tk, &pseudo, "T041",
                        "linear deger hic tuketilmedi (islev sonu)",
                        "'tekkez<T>' parametresi/yerel degeri kullan/imha "
                        "ile tuketilmeli, ya da ver ile geri donulmeli.");
            }
            cur = cur->sonraki;
        }
        tk->aktif = eski;
    }
}

void tekkez_kontrol_program(TekKezKontrol *tk, const Dugum *prog) {
    if (!tk || !prog || !tk->aktif_mi) return;
    if (prog->tip != DUGUM_PROGRAM) return;
    for (int i = 0; i < prog->veri.program.sayi; i++) {
        analiz_tanim(tk, prog->veri.program.uyeler[i]);
    }
}
