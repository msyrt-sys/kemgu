#include "tip_kontrol.h"
#include "hata.h"

#include <string.h>
#include <stdio.h>

/* === Setup === */

void tip_kontrol_baslat(TipKontrol *tk, Arena *a, Scope *global,
                        const char *dosya_adi, const char *kaynak) {
    tk->arena = a;
    tk->scope = global;
    tk->global_scope = global;
    tk->aktif_donus_tipi = NULL;
    tk->hata_sayisi = 0;
    tk->dosya_adi = dosya_adi;
    tk->kaynak = kaynak;
}

void tip_hata(TipKontrol *tk, const Dugum *d,
              const char *kod, const char *mesaj) {
    if (!tk || !d) return;
    tk->hata_sayisi++;
    hata_raporla(tk->dosya_adi, tk->kaynak,
                 d->satir, d->sutun, kod, mesaj, NULL);
}

/* === Yardimci: hata tipi === */

static TipBilgisi *t_hata(TipKontrol *tk) {
    return tip_olustur_basit(tk->arena, TIP_HATA);
}

static TipBilgisi *t_basit(TipKontrol *tk, TipKategorisi k) {
    return tip_olustur_basit(tk->arena, k);
}

/* === Ad cevirici (built-in tip ad) === */

static int basit_tip_adindan(const char *ad, int uz, TipKategorisi *out) {
    /* Metin karsilastirma — basit lookup */
    struct { const char *ad; int uz; TipKategorisi k; } tbl[] = {
        {"tam8",      4, TIP_TAM8},
        {"tam16",     5, TIP_TAM16},
        {"tam32",     5, TIP_TAM32},
        {"tam64",     5, TIP_TAM64},
        {"dtam8",     5, TIP_DTAM8},
        {"dtam16",    6, TIP_DTAM16},
        {"dtam32",    6, TIP_DTAM32},
        {"dtam64",    6, TIP_DTAM64},
        {"kesirli32", 9, TIP_KESIRLI32},
        {"kesirli64", 9, TIP_KESIRLI64},
        {"karakter",  8, TIP_KARAKTER},
        {"metin",     5, TIP_METIN},
        /* mantiksal: m,a,n,t,ı,k,s,a,l = 1+1+1+1+2+1+1+1+1 = 10 byte */
        {"mant\xc4\xb1ksal", 10, TIP_MANTIKSAL},
        /* bos: b,o,ş = 1+1+2 = 4 byte */
        {"bo\xc5\x9f", 4, TIP_BOS},
    };
    int n = (int)(sizeof(tbl) / sizeof(tbl[0]));
    for (int i = 0; i < n; i++) {
        if (tbl[i].uz == uz && memcmp(tbl[i].ad, ad, (size_t)uz) == 0) {
            *out = tbl[i].k;
            return 1;
        }
    }
    return 0;
}

/* === AST tip -> TipBilgisi cevirici === */

TipBilgisi *ast_tip_to_bilgi(TipKontrol *tk, const Dugum *tip_d) {
    if (!tip_d) return t_hata(tk);

    switch (tip_d->tip) {
        case DUGUM_TIP_BASIT: {
            const char *ad = tip_d->veri.tip_basit.ad;
            int uz = tip_d->veri.tip_basit.ad_uzunluk;
            TipKategorisi k;
            if (basit_tip_adindan(ad, uz, &k)) {
                return t_basit(tk, k);
            }
            /* Yapi/Generic param? sembol tablosunda ara */
            const Sembol *s = sembol_bul(tk->scope, ad, uz);
            if (s) {
                if (s->kategori == SEMBOL_YAPI) {
                    return tip_olustur_yapi(tk->arena, s->ad, s->ad_uzunluk,
                                            NULL, 0);
                }
                if (s->kategori == SEMBOL_GENERIC_PARAM) {
                    return s->tip;  /* zaten TIP_GENERIC_PARAM */
                }
            }
            tip_hata(tk, tip_d, "T011", "bilinmeyen tip");
            return t_hata(tk);
        }

        case DUGUM_TIP_REFERANS: {
            TipBilgisi *hedef = ast_tip_to_bilgi(tk,
                tip_d->veri.tip_referans.hedef_tip);
            return tip_olustur_referans(tk->arena, hedef,
                tip_d->veri.tip_referans.degisken_mi);
        }

        case DUGUM_TIP_POINTER: {
            TipBilgisi *hedef = ast_tip_to_bilgi(tk,
                tip_d->veri.tip_pointer.hedef_tip);
            return tip_olustur_pointer(tk->arena, hedef);
        }

        case DUGUM_TIP_DIZI: {
            TipBilgisi *eleman = ast_tip_to_bilgi(tk,
                tip_d->veri.tip_dizi.eleman_tip);
            return tip_olustur_dizi(tk->arena, eleman);
        }

        case DUGUM_TIP_SECIMLIK: {
            TipBilgisi *ic = ast_tip_to_bilgi(tk,
                tip_d->veri.tip_secimlik.ic_tip);
            return tip_olustur_secimlik(tk->arena, ic);
        }

        case DUGUM_TIP_SONUC: {
            TipBilgisi *deger = ast_tip_to_bilgi(tk,
                tip_d->veri.tip_sonuc.deger_tip);
            TipBilgisi *hata = ast_tip_to_bilgi(tk,
                tip_d->veri.tip_sonuc.hata_tip);
            return tip_olustur_sonuc(tk->arena, deger, hata);
        }

        case DUGUM_TIP_ISLEV: {
            int n = tip_d->veri.tip_islev.param_sayi;
            TipBilgisi **params = NULL;
            if (n > 0) {
                params = (TipBilgisi **)arena_ayir(tk->arena,
                                                   sizeof(TipBilgisi *) * (size_t)n);
                if (params) {
                    for (int i = 0; i < n; i++) {
                        params[i] = ast_tip_to_bilgi(tk,
                            tip_d->veri.tip_islev.parametreler[i]);
                    }
                }
            }
            TipBilgisi *donus = ast_tip_to_bilgi(tk,
                tip_d->veri.tip_islev.donus_tip);
            return tip_olustur_islev(tk->arena, params, n, donus);
        }

        case DUGUM_TIP_KULLANICI: {
            /* Yol = DUGUM_TANIMLAYICI veya DUGUM_YOL */
            const Dugum *yol = tip_d->veri.tip_kullanici.yol;
            if (yol && yol->tip == DUGUM_TANIMLAYICI) {
                const char *ad = yol->veri.tanimlayici.metin;
                int uz = yol->veri.tanimlayici.uzunluk;
                int n = tip_d->veri.tip_kullanici.tip_arg_sayi;
                TipBilgisi **args = NULL;
                if (n > 0) {
                    args = (TipBilgisi **)arena_ayir(tk->arena,
                            sizeof(TipBilgisi *) * (size_t)n);
                    for (int i = 0; i < n; i++) {
                        args[i] = ast_tip_to_bilgi(tk,
                            tip_d->veri.tip_kullanici.tip_arg[i]);
                    }
                }
                /* Sembol tablosunda yapi mi diye bak */
                const Sembol *s = sembol_bul(tk->scope, ad, uz);
                if (s && s->kategori == SEMBOL_YAPI) {
                    return tip_olustur_yapi(tk->arena, s->ad, s->ad_uzunluk,
                                            args, n);
                }
                tip_hata(tk, tip_d, "T011", "bilinmeyen kullanici tipi");
                return t_hata(tk);
            }
            tip_hata(tk, tip_d, "T016", "tip yolu cozumlenemedi");
            return t_hata(tk);
        }

        default:
            tip_hata(tk, tip_d, "T011", "tip dugumu beklenirken farkli dugum");
            return t_hata(tk);
    }
}

/* === Tip belirle (ifade visitor) === */

/* Yardimci: ikili sayisal op (sol == sag, ikisi de sayisal) */
static TipBilgisi *kontrol_ikili_sayisal(TipKontrol *tk, const Dugum *d,
                                         TipBilgisi *sol, TipBilgisi *sag) {
    if (!tip_sayisal_mi(sol)) {
        tip_hata(tk, d, "T003", "ikili operatorun sol tarafi sayisal degil");
        return t_hata(tk);
    }
    if (!tip_sayisal_mi(sag)) {
        tip_hata(tk, d, "T003", "ikili operatorun sag tarafi sayisal degil");
        return t_hata(tk);
    }
    if (!tip_esit(sol, sag)) {
        tip_hata(tk, d, "T001", "ikili operator iki tarafi ayni tip olmali");
        return t_hata(tk);
    }
    return sol;
}

/* Yardimci: ikili mantiksal (sol+sag mantiksal) */
static TipBilgisi *kontrol_ikili_mantiksal(TipKontrol *tk, const Dugum *d,
                                           TipBilgisi *sol, TipBilgisi *sag) {
    if (!tip_mantiksal_mi(sol) || !tip_mantiksal_mi(sag)) {
        tip_hata(tk, d, "T004", "mantiksal op iki tarafi mantiksal olmali");
        return t_hata(tk);
    }
    return t_basit(tk, TIP_MANTIKSAL);
}

/* Yardimci: yapi olusturma alan kontrolu */
static TipBilgisi *kontrol_yapi_olustur(TipKontrol *tk, const Dugum *d) {
    const char *tip_ad = d->veri.yapi_olustur.tip_ad;
    int tip_ad_uz = d->veri.yapi_olustur.tip_ad_uzunluk;
    const Sembol *yapi_sem = sembol_bul(tk->scope, tip_ad, tip_ad_uz);
    if (!yapi_sem || yapi_sem->kategori != SEMBOL_YAPI) {
        tip_hata(tk, d, "T002", "yapi tipi tanimsiz");
        return t_hata(tk);
    }
    /* Her alan_atama icin ad eşlesimi + tip eşlesimi */
    int hata = 0;
    for (int i = 0; i < d->veri.yapi_olustur.alan_sayi; i++) {
        const Dugum *aa = d->veri.yapi_olustur.alanlar[i];
        const char *ad = aa->veri.alan_atama.ad;
        int uz = aa->veri.alan_atama.ad_uzunluk;
        const Sembol *alan = sembol_yapi_alani(yapi_sem, ad, uz);
        if (!alan) {
            tip_hata(tk, aa, "T017", "yapida bilinmeyen alan");
            hata = 1;
            continue;
        }
        /* Bidirectional: alan degeri alan tipi context'inde */
        TipBilgisi *deger_tip = tip_belirle_beklenen(tk,
            aa->veri.alan_atama.deger, alan->tip);
        if (!tip_esit(alan->tip, deger_tip) &&
            deger_tip->kategori != TIP_HATA) {
            tip_hata(tk, aa, "T001", "alan tipi uyumsuz");
            hata = 1;
        }
    }
    /* Eksik alan kontrolu (yapi alanlarinin hepsi var mi?) */
    if (yapi_sem->yapi_scope) {
        for (SembolLink *l = yapi_sem->yapi_scope->bas; l; l = l->sonraki) {
            if (l->sembol.kategori != SEMBOL_DEGISKEN) continue;
            int bulundu = 0;
            for (int i = 0; i < d->veri.yapi_olustur.alan_sayi; i++) {
                const Dugum *aa = d->veri.yapi_olustur.alanlar[i];
                if (aa->veri.alan_atama.ad_uzunluk == l->sembol.ad_uzunluk &&
                    memcmp(aa->veri.alan_atama.ad, l->sembol.ad,
                           (size_t)l->sembol.ad_uzunluk) == 0) {
                    bulundu = 1;
                    break;
                }
            }
            if (!bulundu) {
                tip_hata(tk, d, "T012", "yapi olusturmada eksik alan");
                hata = 1;
            }
        }
    }
    (void)hata;  /* hata zaten tk->hata_sayisi'na sayildi */
    return tip_olustur_yapi(tk->arena, yapi_sem->ad, yapi_sem->ad_uzunluk,
                            NULL, 0);
}

/* Ana visitor */
TipBilgisi *tip_belirle(TipKontrol *tk, const Dugum *d) {
    if (!d) return t_hata(tk);

    switch (d->tip) {
        /* === Literaller === */
        case DUGUM_TAM:
            return t_basit(tk, TIP_TAM32);  /* default; ADIM 11.5'te context */
        case DUGUM_KESIRLI:
            return t_basit(tk, TIP_KESIRLI64);
        case DUGUM_METIN:
            return t_basit(tk, TIP_METIN);
        case DUGUM_KARAKTER:
            return t_basit(tk, TIP_KARAKTER);
        case DUGUM_MANTIKSAL:
            return t_basit(tk, TIP_MANTIKSAL);
        case DUGUM_BOS:
            return t_basit(tk, TIP_BOS);

        /* === Tanimlayici === */
        case DUGUM_TANIMLAYICI: {
            const Sembol *s = sembol_bul(tk->scope,
                d->veri.tanimlayici.metin, d->veri.tanimlayici.uzunluk);
            if (!s) {
                tip_hata(tk, d, "T002", "tanimsiz sembol");
                return t_hata(tk);
            }
            return s->tip ? s->tip : t_hata(tk);
        }

        /* === Ikili === */
        case DUGUM_IKILI: {
            TipBilgisi *sol = tip_belirle(tk, d->veri.ikili.sol);
            TipBilgisi *sag = tip_belirle(tk, d->veri.ikili.sag);
            if (sol->kategori == TIP_HATA || sag->kategori == TIP_HATA) {
                return t_hata(tk);
            }
            switch (d->veri.ikili.op) {
                case OP_ARTI:  case OP_EKSI:
                case OP_CARPI: case OP_BOLU:  case OP_MOD:
                    return kontrol_ikili_sayisal(tk, d, sol, sag);

                case OP_ESIT:  case OP_ESIT_DEGIL:
                    if (!tip_esit(sol, sag)) {
                        tip_hata(tk, d, "T001",
                                 "esitlik karsilastirma ayni tip olmali");
                        return t_hata(tk);
                    }
                    return t_basit(tk, TIP_MANTIKSAL);

                case OP_KUCUK: case OP_BUYUK:
                case OP_KUCUK_ESIT: case OP_BUYUK_ESIT: {
                    if (!tip_sayisal_mi(sol) || !tip_sayisal_mi(sag)) {
                        tip_hata(tk, d, "T003",
                                 "karsilastirma sayisal tip ister");
                        return t_hata(tk);
                    }
                    if (!tip_esit(sol, sag)) {
                        tip_hata(tk, d, "T001",
                                 "karsilastirma iki tarafi ayni tip olmali");
                        return t_hata(tk);
                    }
                    return t_basit(tk, TIP_MANTIKSAL);
                }

                case OP_VE: case OP_VEYA:
                    return kontrol_ikili_mantiksal(tk, d, sol, sag);

                default:
                    tip_hata(tk, d, "T001", "bilinmeyen ikili operator");
                    return t_hata(tk);
            }
        }

        /* === Tekli === */
        case DUGUM_TEKLI: {
            TipBilgisi *op = tip_belirle(tk, d->veri.tekli.operand);
            if (op->kategori == TIP_HATA) return t_hata(tk);
            switch (d->veri.tekli.op) {
                case OP_NEG:
                    if (!tip_sayisal_mi(op)) {
                        tip_hata(tk, d, "T003", "tekli '-' sayisal ister");
                        return t_hata(tk);
                    }
                    return op;

                case OP_DEGIL:
                    if (!tip_mantiksal_mi(op)) {
                        tip_hata(tk, d, "T004", "'degil' mantiksal ister");
                        return t_hata(tk);
                    }
                    return t_basit(tk, TIP_MANTIKSAL);

                case OP_REF:
                    return tip_olustur_referans(tk->arena, op, 0);

                case OP_REF_DEGISKEN:
                    return tip_olustur_referans(tk->arena, op, 1);

                case OP_DEREFERANS:
                    if (op->kategori != TIP_POINTER) {
                        tip_hata(tk, d, "T001",
                                 "'*' sadece pointer tipinde kullanilir");
                        return t_hata(tk);
                    }
                    return op->veri.pointer.hedef;

                default:
                    tip_hata(tk, d, "T001", "bilinmeyen tekli operator");
                    return t_hata(tk);
            }
        }

        /* === Cagri === */
        case DUGUM_CAGRI: {
            TipBilgisi *hedef_tip = tip_belirle(tk, d->veri.cagri.hedef);
            if (hedef_tip->kategori == TIP_HATA) return t_hata(tk);
            if (hedef_tip->kategori != TIP_ISLEV) {
                tip_hata(tk, d, "T006", "cagri icin islev tipi gerek");
                return t_hata(tk);
            }
            if (d->veri.cagri.sayi != hedef_tip->veri.islev.param_sayi) {
                tip_hata(tk, d, "T010", "cagri arguman sayisi uyumsuz");
                return t_hata(tk);
            }
            for (int i = 0; i < d->veri.cagri.sayi; i++) {
                TipBilgisi *param_tip = hedef_tip->veri.islev.parametreler[i];
                /* Bidirectional: arg, parametre tipi context'inde cikarsanir */
                TipBilgisi *arg_tip = tip_belirle_beklenen(tk,
                    d->veri.cagri.argumanlar[i], param_tip);
                if (!tip_esit(arg_tip, param_tip) &&
                    arg_tip->kategori != TIP_HATA) {
                    tip_hata(tk, d->veri.cagri.argumanlar[i], "T001",
                             "arguman tipi parametre tipi ile uyumsuz");
                }
            }
            return hedef_tip->veri.islev.donus;
        }

        /* === Erisim (x.y) === */
        case DUGUM_ERISIM: {
            TipBilgisi *nesne_tip = tip_belirle(tk, d->veri.erisim.nesne);
            if (nesne_tip->kategori == TIP_HATA) return t_hata(tk);

            /* Referansi otomatik dereference et */
            if (nesne_tip->kategori == TIP_REFERANS) {
                nesne_tip = nesne_tip->veri.referans.hedef;
            }

            if (nesne_tip->kategori != TIP_YAPI) {
                tip_hata(tk, d, "T007", "alan erisimi yapi tipi gerek");
                return t_hata(tk);
            }
            const Sembol *yapi_sem = sembol_bul(tk->scope,
                nesne_tip->veri.yapi.ad, nesne_tip->veri.yapi.ad_uzunluk);
            if (!yapi_sem || yapi_sem->kategori != SEMBOL_YAPI) {
                tip_hata(tk, d, "T002", "yapi tanimi bulunamadi");
                return t_hata(tk);
            }
            const Sembol *alan = sembol_yapi_alani(yapi_sem,
                d->veri.erisim.alan, d->veri.erisim.alan_uzunluk);
            if (!alan) {
                tip_hata(tk, d, "T009", "alan bulunamadi");
                return t_hata(tk);
            }
            return alan->tip ? alan->tip : t_hata(tk);
        }

        /* === Indeks (x[i]) === */
        case DUGUM_INDEKS: {
            TipBilgisi *nesne_tip = tip_belirle(tk, d->veri.indeks.nesne);
            TipBilgisi *idx_tip = tip_belirle(tk, d->veri.indeks.indeks);
            if (nesne_tip->kategori == TIP_HATA ||
                idx_tip->kategori == TIP_HATA) return t_hata(tk);

            /* Referansi otomatik dereference et */
            if (nesne_tip->kategori == TIP_REFERANS) {
                nesne_tip = nesne_tip->veri.referans.hedef;
            }

            if (nesne_tip->kategori != TIP_DIZI) {
                tip_hata(tk, d, "T008", "indeksleme dizi tipi gerek");
                return t_hata(tk);
            }
            if (!tip_tamsayi_mi(idx_tip)) {
                tip_hata(tk, d, "T005", "indeks tamsayi olmali");
                return t_hata(tk);
            }
            return nesne_tip->veri.dizi.eleman;
        }

        /* === Yol (x::y) === */
        case DUGUM_YOL: {
            /* Sol modul tanimlayici olmali */
            const Dugum *sol = d->veri.yol.sol;
            if (sol->tip != DUGUM_TANIMLAYICI) {
                tip_hata(tk, d, "T016", "yol cozumlemesi karmasik");
                return t_hata(tk);
            }
            const Sembol *m = sembol_bul(tk->scope,
                sol->veri.tanimlayici.metin, sol->veri.tanimlayici.uzunluk);
            if (!m || m->kategori != SEMBOL_MODUL || !m->modul_scope) {
                tip_hata(tk, d, "T016", "modul bulunamadi");
                return t_hata(tk);
            }
            const Sembol *uye = sembol_bul_yerel(m->modul_scope,
                d->veri.yol.sag_ad, d->veri.yol.sag_ad_uzunluk);
            if (!uye) {
                tip_hata(tk, d, "T002", "modul uyesi bulunamadi");
                return t_hata(tk);
            }
            return uye->tip ? uye->tip : t_hata(tk);
        }

        /* === Yapi olusturma === */
        case DUGUM_YAPI_OLUSTUR:
            return kontrol_yapi_olustur(tk, d);

        /* === Dizi olusturma === */
        case DUGUM_DIZI_OLUSTUR: {
            int n = d->veri.dizi_olustur.sayi;
            if (n == 0) {
                /* Bos dizi — context lazim, ADIM 11.5'te */
                tip_hata(tk, d, "T014", "bos dizi tipi cikarsanamaz (context lazim)");
                return tip_olustur_dizi(tk->arena, t_basit(tk, TIP_BILINMIYOR));
            }
            TipBilgisi *ilk = tip_belirle(tk, d->veri.dizi_olustur.elemanlar[0]);
            for (int i = 1; i < n; i++) {
                TipBilgisi *e = tip_belirle(tk,
                    d->veri.dizi_olustur.elemanlar[i]);
                if (!tip_esit(ilk, e) && e->kategori != TIP_HATA) {
                    tip_hata(tk, d->veri.dizi_olustur.elemanlar[i], "T013",
                             "dizi elemanlari farkli tipte");
                }
            }
            return tip_olustur_dizi(tk->arena, ilk);
        }

        /* === Lambda === */
        case DUGUM_LAMBDA: {
            int n = d->veri.lambda.param_sayi;
            TipBilgisi **params = NULL;
            if (n > 0) {
                params = (TipBilgisi **)arena_ayir(tk->arena,
                            sizeof(TipBilgisi *) * (size_t)n);
            }
            for (int i = 0; i < n; i++) {
                const Dugum *p = d->veri.lambda.parametreler[i];
                if (!p->veri.parametre.tip) {
                    tip_hata(tk, p, "T015",
                             "lambda parametre tip annotasyonu gerek");
                    params[i] = t_hata(tk);
                    continue;
                }
                params[i] = ast_tip_to_bilgi(tk, p->veri.parametre.tip);
            }
            /* Govde tipi — basit: parse_ifade donus tipi.
             * Tam degerlendirme ADIM 11.4'te (deyimler dahil). */
            TipBilgisi *donus = tip_belirle(tk, d->veri.lambda.govde);
            return tip_olustur_islev(tk->arena, params, n, donus);
        }

        /* === Hata dugumu === */
        case DUGUM_HATA:
            return t_hata(tk);

        /* === Diger (deyim/tanim) — burada tip belirleme yok === */
        default:
            tip_hata(tk, d, "T001", "ifade beklenirken farkli dugum tipi");
            return t_hata(tk);
    }
}

/* ========================================================================
 * ADIM 11.5: Bidirectional tip cikarsamasi
 * ======================================================================== */

TipBilgisi *tip_belirle_beklenen(TipKontrol *tk, const Dugum *d,
                                  const TipBilgisi *beklenen) {
    if (!d) return t_hata(tk);
    if (!beklenen || beklenen->kategori == TIP_HATA) {
        return tip_belirle(tk, d);
    }

    switch (d->tip) {
        case DUGUM_TAM:
            /* Sayi literali context tamsayi tipine gore */
            if (tip_tamsayi_mi(beklenen)) {
                return t_basit(tk, beklenen->kategori);
            }
            break;

        case DUGUM_KESIRLI:
            if (beklenen->kategori == TIP_KESIRLI32 ||
                beklenen->kategori == TIP_KESIRLI64) {
                return t_basit(tk, beklenen->kategori);
            }
            break;

        case DUGUM_DIZI_OLUSTUR: {
            if (beklenen->kategori != TIP_DIZI) break;
            const TipBilgisi *eleman_t = beklenen->veri.dizi.eleman;
            int n = d->veri.dizi_olustur.sayi;
            if (n == 0) {
                /* Bos dizi -> beklenen tip */
                return tip_olustur_dizi(tk->arena,
                    t_basit(tk, eleman_t->kategori));  /* shallow */
            }
            /* Dolu dizi: her eleman beklenen->dizi.eleman context'inde */
            for (int i = 0; i < n; i++) {
                TipBilgisi *e = tip_belirle_beklenen(tk,
                    d->veri.dizi_olustur.elemanlar[i], eleman_t);
                if (!tip_esit(e, eleman_t) && e->kategori != TIP_HATA) {
                    tip_hata(tk, d->veri.dizi_olustur.elemanlar[i],
                             "T013", "dizi elemani beklenen tip ile uyumsuz");
                }
            }
            return tip_olustur_dizi(tk->arena,
                                    (TipBilgisi *)eleman_t);
        }

        default:
            break;
    }

    /* Default davranis */
    return tip_belirle(tk, d);
}

/* ========================================================================
 * ADIM 11.4: Deyim ve tanim tip kontrolu
 * ======================================================================== */

static void tip_kontrol_deyim(TipKontrol *tk, const Dugum *d);
static void tip_kontrol_tanim(TipKontrol *tk, const Dugum *d);

/* === 1. Gecis: yapi/islev/sabit sembollerini global'e ekle === */

static void pre_populate_yapi(TipKontrol *tk, const Dugum *yapi) {
    /* Yapi scope yarat */
    Scope *yapi_s = scope_olustur(tk->arena, SCOPE_YAPI, tk->global_scope);

    /* Generic params -> yapi scope'a ekle */
    for (int j = 0; j < yapi->veri.yapi.tip_param_sayi; j++) {
        const char *t_ad = yapi->veri.yapi.tip_paramlar[j];
        int t_uz = (int)strlen(t_ad);
        Sembol gp;
        memset(&gp, 0, sizeof(gp));
        gp.ad = t_ad;
        gp.ad_uzunluk = t_uz;
        gp.kategori = SEMBOL_GENERIC_PARAM;
        gp.tip = tip_olustur_generic_param(tk->arena, t_ad, t_uz);
        gp.satir = yapi->satir;
        gp.sutun = yapi->sutun;
        sembol_ekle(yapi_s, tk->arena, &gp);
    }

    /* Alanlar -> yapi scope'a ekle (tip bilgisi yapi_scope context'inde) */
    Scope *eski = tk->scope;
    tk->scope = yapi_s;
    for (int j = 0; j < yapi->veri.yapi.alan_sayi; j++) {
        const Dugum *alan = yapi->veri.yapi.alanlar[j];
        TipBilgisi *alan_tipi = ast_tip_to_bilgi(tk, alan->veri.alan.tip);
        Sembol s;
        memset(&s, 0, sizeof(s));
        s.ad = alan->veri.alan.ad;
        s.ad_uzunluk = alan->veri.alan.ad_uzunluk;
        s.kategori = SEMBOL_DEGISKEN;
        s.tip = alan_tipi;
        s.satir = alan->satir;
        s.sutun = alan->sutun;
        if (sembol_ekle(yapi_s, tk->arena, &s) != 0) {
            tip_hata(tk, alan, "T024", "yapi alan adi cakismasi");
        }
    }
    tk->scope = eski;

    /* Yapi sembolu global'e */
    Sembol y;
    memset(&y, 0, sizeof(y));
    y.ad = yapi->veri.yapi.ad;
    y.ad_uzunluk = yapi->veri.yapi.ad_uzunluk;
    y.kategori = SEMBOL_YAPI;
    y.yapi_scope = yapi_s;
    y.ast_dugumu = yapi;
    y.satir = yapi->satir;
    y.sutun = yapi->sutun;
    if (sembol_ekle(tk->global_scope, tk->arena, &y) != 0) {
        tip_hata(tk, yapi, "T026", "yapi tanimi cakismasi");
    }
}

static void pre_populate_islev(TipKontrol *tk, const Dugum *islev) {
    int n = islev->veri.islev.param_sayi;
    TipBilgisi **ptipler = NULL;
    if (n > 0) {
        ptipler = (TipBilgisi **)arena_ayir(tk->arena,
                    sizeof(TipBilgisi *) * (size_t)n);
    }
    for (int j = 0; j < n; j++) {
        const Dugum *p = islev->veri.islev.parametreler[j];
        ptipler[j] = ast_tip_to_bilgi(tk, p->veri.parametre.tip);
    }
    TipBilgisi *donus = islev->veri.islev.donus_tipi
        ? ast_tip_to_bilgi(tk, islev->veri.islev.donus_tipi)
        : tip_olustur_basit(tk->arena, TIP_BOS);
    TipBilgisi *islev_tipi = tip_olustur_islev(tk->arena, ptipler, n, donus);

    Sembol s;
    memset(&s, 0, sizeof(s));
    s.ad = islev->veri.islev.ad;
    s.ad_uzunluk = islev->veri.islev.ad_uzunluk;
    s.kategori = SEMBOL_ISLEV;
    s.tip = islev_tipi;
    s.ast_dugumu = islev;
    s.satir = islev->satir;
    s.sutun = islev->sutun;
    if (sembol_ekle(tk->global_scope, tk->arena, &s) != 0) {
        tip_hata(tk, islev, "T024", "islev tanimi cakismasi");
    }
}

static void pre_populate_sabit(TipKontrol *tk, const Dugum *sabit) {
    TipBilgisi *t = ast_tip_to_bilgi(tk, sabit->veri.sabit.tip);
    Sembol s;
    memset(&s, 0, sizeof(s));
    s.ad = sabit->veri.sabit.ad;
    s.ad_uzunluk = sabit->veri.sabit.ad_uzunluk;
    s.kategori = SEMBOL_SABIT;
    s.tip = t;
    s.ast_dugumu = sabit;
    s.satir = sabit->satir;
    s.sutun = sabit->sutun;
    if (sembol_ekle(tk->global_scope, tk->arena, &s) != 0) {
        tip_hata(tk, sabit, "T024", "sabit tanimi cakismasi");
    }
}

static void pre_populate(TipKontrol *tk, const Dugum *program) {
    if (!program || program->tip != DUGUM_PROGRAM) return;

    /* Once yapilari (tipleri) ekle, sonra islevleri (parametre tipleri
     * yapilara referans verebilir). */
    for (int i = 0; i < program->veri.program.sayi; i++) {
        const Dugum *uye = program->veri.program.uyeler[i];
        if (uye->tip == DUGUM_YAPI) pre_populate_yapi(tk, uye);
        else if (uye->tip == DUGUM_DISA &&
                 uye->veri.disa.tanim &&
                 uye->veri.disa.tanim->tip == DUGUM_YAPI) {
            pre_populate_yapi(tk, uye->veri.disa.tanim);
        }
    }

    /* Islevler ve sabitler */
    for (int i = 0; i < program->veri.program.sayi; i++) {
        const Dugum *uye = program->veri.program.uyeler[i];
        const Dugum *gercek = (uye->tip == DUGUM_DISA && uye->veri.disa.tanim)
                              ? uye->veri.disa.tanim : uye;
        if (gercek->tip == DUGUM_ISLEV) pre_populate_islev(tk, gercek);
        else if (gercek->tip == DUGUM_SABIT) pre_populate_sabit(tk, gercek);
    }
}

/* === Deyim tip kontrolu === */

static void tip_kontrol_deyim(TipKontrol *tk, const Dugum *d) {
    if (!d) return;

    switch (d->tip) {
        case DUGUM_DEGISKEN: {
            TipBilgisi *annot = NULL;
            TipBilgisi *deger_tip;
            if (d->veri.degisken.tip) {
                annot = ast_tip_to_bilgi(tk, d->veri.degisken.tip);
                /* Bidirectional: literal'lar annot context'inde cikarsanir */
                deger_tip = tip_belirle_beklenen(tk,
                    d->veri.degisken.deger, annot);
                if (!tip_esit(annot, deger_tip) &&
                    deger_tip->kategori != TIP_HATA &&
                    annot->kategori != TIP_HATA) {
                    tip_hata(tk, d, "T001",
                             "degisken tip annot ile baslangic uyumsuz");
                }
            } else {
                deger_tip = tip_belirle(tk, d->veri.degisken.deger);
            }
            TipBilgisi *son = annot ? annot : deger_tip;
            Sembol s;
            memset(&s, 0, sizeof(s));
            s.ad = d->veri.degisken.ad;
            s.ad_uzunluk = d->veri.degisken.ad_uzunluk;
            s.kategori = SEMBOL_DEGISKEN;
            s.tip = son;
            s.ast_dugumu = d;
            s.satir = d->satir;
            s.sutun = d->sutun;
            if (sembol_ekle(tk->scope, tk->arena, &s) != 0) {
                tip_hata(tk, d, "T024", "degisken zaten tanimli");
            }
            break;
        }

        case DUGUM_ATAMA: {
            /* Hedef lvalue mi? (TANIMLAYICI, ERISIM, INDEKS) */
            const Dugum *hedef = d->veri.atama.hedef;
            if (hedef->tip != DUGUM_TANIMLAYICI &&
                hedef->tip != DUGUM_ERISIM &&
                hedef->tip != DUGUM_INDEKS) {
                tip_hata(tk, d, "T022",
                         "atama hedefi lvalue olmali (tanimlayici/erisim/indeks)");
            }
            TipBilgisi *ht = tip_belirle(tk, hedef);
            /* Bidirectional: deger hedef tip context'inde */
            TipBilgisi *dt = tip_belirle_beklenen(tk, d->veri.atama.deger, ht);
            if (!tip_esit(ht, dt) &&
                ht->kategori != TIP_HATA && dt->kategori != TIP_HATA) {
                tip_hata(tk, d, "T001", "atama tipi uyumsuz");
            }
            break;
        }

        case DUGUM_VER: {
            if (!tk->aktif_donus_tipi) {
                tip_hata(tk, d, "T023", "ver islev govdesi disinda");
                break;
            }
            if (d->veri.ver.deger) {
                /* Bidirectional: deger donus tipi context'inde */
                TipBilgisi *deger = tip_belirle_beklenen(tk,
                    d->veri.ver.deger, tk->aktif_donus_tipi);
                if (!tip_esit(deger, tk->aktif_donus_tipi) &&
                    deger->kategori != TIP_HATA &&
                    tk->aktif_donus_tipi->kategori != TIP_HATA) {
                    tip_hata(tk, d, "T020",
                             "ver tipi islev donus tipi ile uyumsuz");
                }
            } else {
                /* ver; — donus tipi BOS olmali */
                if (tk->aktif_donus_tipi->kategori != TIP_BOS) {
                    tip_hata(tk, d, "T020",
                             "ver; gerek (donus tipi BOS olmali ya da deger ver)");
                }
            }
            break;
        }

        case DUGUM_EGER: {
            TipBilgisi *kosul = tip_belirle(tk, d->veri.eger.kosul);
            if (!tip_mantiksal_mi(kosul) && kosul->kategori != TIP_HATA) {
                tip_hata(tk, d, "T021", "eger kosulu mantiksal olmali");
            }
            tip_kontrol_deyim(tk, d->veri.eger.gozdoldur);
            if (d->veri.eger.yan) {
                tip_kontrol_deyim(tk, d->veri.eger.yan);
            }
            break;
        }

        case DUGUM_IKEN: {
            TipBilgisi *kosul = tip_belirle(tk, d->veri.iken.kosul);
            if (!tip_mantiksal_mi(kosul) && kosul->kategori != TIP_HATA) {
                tip_hata(tk, d, "T021", "iken kosulu mantiksal olmali");
            }
            tip_kontrol_deyim(tk, d->veri.iken.govde);
            break;
        }

        case DUGUM_ICIN: {
            TipBilgisi *kol = tip_belirle(tk, d->veri.icin.koleksiyon);
            TipBilgisi *eleman_tipi;
            if (kol->kategori == TIP_DIZI) {
                eleman_tipi = kol->veri.dizi.eleman;
            } else if (kol->kategori == TIP_HATA) {
                eleman_tipi = t_hata(tk);
            } else {
                tip_hata(tk, d, "T027", "icin koleksiyonu Dizi<T> olmali");
                eleman_tipi = t_hata(tk);
            }
            /* Yeni scope: icin degiskeni eleman_tipi olarak */
            Scope *eski = tk->scope;
            tk->scope = scope_olustur(tk->arena, SCOPE_BLOK, eski);
            Sembol s;
            memset(&s, 0, sizeof(s));
            s.ad = d->veri.icin.degisken_adi;
            s.ad_uzunluk = d->veri.icin.degisken_adi_uzunluk;
            s.kategori = SEMBOL_DEGISKEN;
            s.tip = eleman_tipi;
            s.satir = d->satir;
            s.sutun = d->sutun;
            sembol_ekle(tk->scope, tk->arena, &s);
            tip_kontrol_deyim(tk, d->veri.icin.govde);
            tk->scope = eski;
            break;
        }

        case DUGUM_ESLES: {
            /* deger tipini belirle (kontrol icin gerekli — desen tip kontrol
             * ileride ADIM 11.6'da generic/secimlik desenleri ile detayli) */
            TipBilgisi *dt = tip_belirle(tk, d->veri.esles.deger);
            (void)dt;
            for (int i = 0; i < d->veri.esles.kol_sayi; i++) {
                const Dugum *kol = d->veri.esles.kollar[i];
                /* Yeni scope kol icin (desen icindeki tanimlayicilar) */
                Scope *eski = tk->scope;
                tk->scope = scope_olustur(tk->arena, SCOPE_BLOK, eski);
                /* Govde — blok ya da ifade. Basit: deyim olarak kontrol et */
                tip_kontrol_deyim(tk, kol->veri.esles_kolu.govde);
                tk->scope = eski;
            }
            break;
        }

        case DUGUM_GUVENSIZ:
            /* Aciklama yok-saymi: gerekli olabilir ama tip kontrol
             * acisindan blok ile ayni */
            tip_kontrol_deyim(tk, d->veri.guvensiz.blok);
            break;

        case DUGUM_BLOK: {
            Scope *eski = tk->scope;
            tk->scope = scope_olustur(tk->arena, SCOPE_BLOK, eski);
            for (int i = 0; i < d->veri.blok.sayi; i++) {
                tip_kontrol_deyim(tk, d->veri.blok.deyimler[i]);
            }
            tk->scope = eski;
            break;
        }

        case DUGUM_IFADE_DEYIMI:
            tip_belirle(tk, d->veri.ifade_deyimi.ifade);
            break;

        case DUGUM_HATA:
            break;

        default:
            /* Bir ifade gibi davran (defansif) */
            tip_belirle(tk, d);
            break;
    }
}

/* === Tanim tip kontrolu === */

static void tip_kontrol_tanim(TipKontrol *tk, const Dugum *d) {
    if (!d) return;

    /* DISA -> ic tanim */
    if (d->tip == DUGUM_DISA) {
        if (d->veri.disa.tanim) tip_kontrol_tanim(tk, d->veri.disa.tanim);
        return;
    }

    switch (d->tip) {
        case DUGUM_ISLEV: {
            const Sembol *islev_sem = sembol_bul_yerel(tk->global_scope,
                d->veri.islev.ad, d->veri.islev.ad_uzunluk);
            if (!islev_sem || !islev_sem->tip ||
                islev_sem->tip->kategori != TIP_ISLEV) return;

            Scope *eski = tk->scope;
            TipBilgisi *eski_donus = tk->aktif_donus_tipi;
            tk->scope = scope_olustur(tk->arena, SCOPE_ISLEV, tk->global_scope);
            tk->aktif_donus_tipi = islev_sem->tip->veri.islev.donus;

            /* Parametreleri scope'a ekle */
            for (int i = 0; i < d->veri.islev.param_sayi; i++) {
                const Dugum *p = d->veri.islev.parametreler[i];
                Sembol s;
                memset(&s, 0, sizeof(s));
                s.ad = p->veri.parametre.ad;
                s.ad_uzunluk = p->veri.parametre.ad_uzunluk;
                s.kategori = SEMBOL_PARAMETRE;
                s.tip = islev_sem->tip->veri.islev.parametreler[i];
                s.ast_dugumu = p;
                s.satir = p->satir;
                s.sutun = p->sutun;
                if (sembol_ekle(tk->scope, tk->arena, &s) != 0) {
                    tip_hata(tk, p, "T024", "parametre adi cakismasi");
                }
            }

            /* Govdeyi kontrol et */
            if (d->veri.islev.govde) {
                tip_kontrol_deyim(tk, d->veri.islev.govde);
            }

            tk->aktif_donus_tipi = eski_donus;
            tk->scope = eski;
            break;
        }

        case DUGUM_SABIT: {
            /* Pre-populate'te sembol eklendi. Simdi deger kontrolu.
             * Bidirectional: literal'lar annot context'inde cikarsanir. */
            TipBilgisi *annot = ast_tip_to_bilgi(tk, d->veri.sabit.tip);
            TipBilgisi *deger = tip_belirle_beklenen(tk,
                d->veri.sabit.deger, annot);
            if (!tip_esit(deger, annot) &&
                deger->kategori != TIP_HATA &&
                annot->kategori != TIP_HATA) {
                tip_hata(tk, d, "T001", "sabit deger tip annot ile uyumsuz");
            }
            break;
        }

        case DUGUM_YAPI:
            /* Pre-populate yeterli (alan tipleri orada cozumlendi) */
            break;

        case DUGUM_KULLAN:
            /* Modul cozumleme ileride (su an no-op) */
            break;

        case DUGUM_MODUL:
            /* Modul scope kontrolu basit — recursive */
            /* Su an: ic uyeleri global scope'a ekle (modul ad alani yok) */
            for (int i = 0; i < d->veri.modul.sayi; i++) {
                tip_kontrol_tanim(tk, d->veri.modul.uyeler[i]);
            }
            break;

        case DUGUM_HATA:
            break;

        default:
            break;
    }
}

/* === Ana fonksiyon === */

void tip_kontrol_program(TipKontrol *tk, const Dugum *program) {
    if (!program || program->tip != DUGUM_PROGRAM) return;
    pre_populate(tk, program);
    for (int i = 0; i < program->veri.program.sayi; i++) {
        tip_kontrol_tanim(tk, program->veri.program.uyeler[i]);
    }
}
