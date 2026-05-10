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
        TipBilgisi *deger_tip = tip_belirle(tk, aa->veri.alan_atama.deger);
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
                TipBilgisi *arg_tip = tip_belirle(tk,
                    d->veri.cagri.argumanlar[i]);
                TipBilgisi *param_tip = hedef_tip->veri.islev.parametreler[i];
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
