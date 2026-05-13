#include "tip_kontrol.h"
#include "hata.h"
#include "lexer.h"
#include "parser.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* === Setup === */

void tip_kontrol_baslat(TipKontrol *tk, Arena *a, Scope *global,
                        const char *dosya_adi, const char *kaynak) {
    tk->arena = a;
    tk->scope = global;
    tk->global_scope = global;
    tk->aktif_donus_tipi = NULL;
    uygula_tablosu_baslat(&tk->uygulamalar);
    tk->yuklenmisler = NULL;
    tk->hata_sayisi = 0;

    /* Built-in islevler — LLVM'de libc karsiliklarina map edilir */
    #define EKLE_BUILTIN(_ad, _ad_uz, _params, _n_params, _donus) do { \
        Sembol _s; memset(&_s, 0, sizeof(_s)); \
        _s.ad = (_ad); _s.ad_uzunluk = (_ad_uz); \
        _s.kategori = SEMBOL_ISLEV; \
        _s.tip = tip_olustur_islev(a, (_params), (_n_params), (_donus)); \
        sembol_ekle(global, a, &_s); \
    } while (0)

    /* yazdir(metin) -> tam32  (libc puts) */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("yazdir", 6, p, 1, tip_olustur_basit(a, TIP_TAM32));
    }

    /* bellek_al(tam64) -> metin  (libc malloc — metin = ptr) */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_TAM64);
        EKLE_BUILTIN("bellek_al", 9, p, 1, tip_olustur_basit(a, TIP_METIN));
    }

    /* bellek_serbest(metin) -> bos  (libc free) */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("bellek_serbest", 14, p, 1, tip_olustur_basit(a, TIP_BOS));
    }

    /* bellek_kopyala(metin, metin, tam64) -> metin  (libc memcpy) */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *) * 3);
        p[0] = tip_olustur_basit(a, TIP_METIN);
        p[1] = tip_olustur_basit(a, TIP_METIN);
        p[2] = tip_olustur_basit(a, TIP_TAM64);
        EKLE_BUILTIN("bellek_kopyala", 14, p, 3, tip_olustur_basit(a, TIP_METIN));
    }

    /* === Madde A: Metin runtime primitifleri (kdl_metin_*) === */

    /* metin_uzunluk(metin) -> tam32 */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("metin_uzunluk", 13, p, 1, tip_olustur_basit(a, TIP_TAM32));
    }
    /* metin_birlestir(metin, metin) -> metin */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *) * 2);
        p[0] = tip_olustur_basit(a, TIP_METIN);
        p[1] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("metin_birlestir", 15, p, 2, tip_olustur_basit(a, TIP_METIN));
    }
    /* metin_kes(metin, tam32, tam32) -> metin */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *) * 3);
        p[0] = tip_olustur_basit(a, TIP_METIN);
        p[1] = tip_olustur_basit(a, TIP_TAM32);
        p[2] = tip_olustur_basit(a, TIP_TAM32);
        EKLE_BUILTIN("metin_kes", 9, p, 3, tip_olustur_basit(a, TIP_METIN));
    }
    /* metin_kucuk(metin) -> metin */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("metin_kucuk", 11, p, 1, tip_olustur_basit(a, TIP_METIN));
    }
    /* metin_buyuk(metin) -> metin */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("metin_buyuk", 11, p, 1, tip_olustur_basit(a, TIP_METIN));
    }
    /* metin_icerir(metin, metin) -> mantiksal */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *) * 2);
        p[0] = tip_olustur_basit(a, TIP_METIN);
        p[1] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("metin_icerir", 12, p, 2,
                     tip_olustur_basit(a, TIP_MANTIKSAL));
    }
    /* metin_baslar(metin, metin) -> mantiksal */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *) * 2);
        p[0] = tip_olustur_basit(a, TIP_METIN);
        p[1] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("metin_baslar", 12, p, 2,
                     tip_olustur_basit(a, TIP_MANTIKSAL));
    }
    /* metin_biter(metin, metin) -> mantiksal */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *) * 2);
        p[0] = tip_olustur_basit(a, TIP_METIN);
        p[1] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("metin_biter", 11, p, 2,
                     tip_olustur_basit(a, TIP_MANTIKSAL));
    }
    /* metin_kirp(metin) -> metin */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("metin_kirp", 10, p, 1, tip_olustur_basit(a, TIP_METIN));
    }
    /* metin_yer_degistir(metin, metin, metin) -> metin */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *) * 3);
        p[0] = tip_olustur_basit(a, TIP_METIN);
        p[1] = tip_olustur_basit(a, TIP_METIN);
        p[2] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("metin_yer_degistir", 18, p, 3,
                     tip_olustur_basit(a, TIP_METIN));
    }

    /* === I/O built-in genisletme (src-bugfix — runtime/kdl_runtime.c) === */

    /* yazdir_tam(tam32) -> bos */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_TAM32);
        EKLE_BUILTIN("yazdir_tam", 10, p, 1, tip_olustur_basit(a, TIP_BOS));
    }
    /* yazdir_tam64(tam64) -> bos */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_TAM64);
        EKLE_BUILTIN("yazdir_tam64", 12, p, 1, tip_olustur_basit(a, TIP_BOS));
    }
    /* yazdir_satir() -> bos */
    {
        EKLE_BUILTIN("yazdir_satir", 12, NULL, 0,
                     tip_olustur_basit(a, TIP_BOS));
    }
    /* yaz_tam(tam32) -> bos */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_TAM32);
        EKLE_BUILTIN("yaz_tam", 7, p, 1, tip_olustur_basit(a, TIP_BOS));
    }
    /* yaz_tam64(tam64) -> bos */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_TAM64);
        EKLE_BUILTIN("yaz_tam64", 9, p, 1, tip_olustur_basit(a, TIP_BOS));
    }
    /* yaz_metin built-in YOK — stdlib/dosya.kem 2-param yaz_metin
     * tanimlar; cakisma onlemek icin (KIRMIZI_QUEUE: dosya_yaz_metin
     * rename gelecek). */

    #undef EKLE_BUILTIN
    tk->dosya_adi = dosya_adi;
    tk->kaynak = kaynak;
    tk->scope_seviyesi = 0;
    tk->lambda_govdesi_icinde = 0;
    tk->lambda_lineer_yakalama = 0;
    tk->lambda_baslangic_scope = NULL;
}

/* === Linear Types Spec V1: yardimci fonksiyonlar === */

/* Eger ifade DUGUM_TANIMLAYICI ise ve sembolu lineer ise -> tuket.
 * Cift tuketim L002 hatasi. */
static void lineer_tuket_eger_baglamaysa(TipKontrol *tk, const Dugum *d) {
    if (!d || d->tip != DUGUM_TANIMLAYICI) return;
    Sembol *s = sembol_bul_yazilabilir(tk->scope,
        d->veri.tanimlayici.metin, d->veri.tanimlayici.uzunluk);
    if (!s || !s->tip || s->tip->kategori != TIP_TEKKEZ) return;
    if (s->lineer_tuketildi >= 1) {
        tip_hata(tk, d, "L002",
            "lineer baglama iki kez tuketildi (move sonrasi erisim)");
    }
    s->lineer_tuketildi++;
}

/* Lambda govdesi icinde: parent scope'taki lineer baglamayi yakala.
 * Sadece bayrak set eder — tuketim asil consume context'inde
 * (kullan/imha/cagri arg/ver) gerceklesir. Boylece kullan(k) gibi
 * ifadeler cift tuketim hatasi (L002) uretmez. */
static void lineer_yakalama_kontrol(TipKontrol *tk, const Dugum *d) {
    if (!d || d->tip != DUGUM_TANIMLAYICI) return;
    if (!tk->lambda_govdesi_icinde) return;
    if (!tk->lambda_baslangic_scope) return;

    /* Lambda kendi scope'unda mi? Eger oyle ise yakalama degil. */
    const Sembol *yerel_check = sembol_bul_yerel(tk->scope,
        d->veri.tanimlayici.metin, d->veri.tanimlayici.uzunluk);
    if (yerel_check) return;

    /* Parent scope'ta lineer baglama mi? */
    const Sembol *parent = sembol_bul(tk->lambda_baslangic_scope,
        d->veri.tanimlayici.metin, d->veri.tanimlayici.uzunluk);
    if (!parent || !parent->tip || parent->tip->kategori != TIP_TEKKEZ) return;

    /* SADECE flag — closure-itself-linear (LC-2) tip isaretlemesi icin */
    tk->lambda_lineer_yakalama = 1;
}

/* Scope kapanisi: o scope'taki lineer baglamalar tuketilmis mi check.
 * Sadece SEMBOL_DEGISKEN/PARAMETRE icin (yapi alan'i degil). */
static void scope_lineer_kapanis_check(TipKontrol *tk, Scope *s) {
    if (!s) return;
    for (SembolLink *l = s->bas; l; l = l->sonraki) {
        Sembol *sem = &l->sembol;
        if (sem->kategori != SEMBOL_DEGISKEN &&
            sem->kategori != SEMBOL_PARAMETRE) continue;
        if (!sem->tip || sem->tip->kategori != TIP_TEKKEZ) continue;
        if (sem->lineer_tuketildi == 0) {
            tk->hata_sayisi++;
            hata_raporla(tk->dosya_adi, tk->kaynak,
                         sem->satir, sem->sutun, "L001",
                         "lineer baglama scope sonunda tuketilmedi",
                         "kullan(...) veya imha(...) ile tuketin");
        }
    }
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

/* Forward declaration (ADIM 11.6'da tanimli, kontrol_yapi_olustur_ic
 * tarafindan kullaniliyor — generic substitusyon icin) */
static TipBilgisi *substitusyon(TipKontrol *tk, const TipBilgisi *t,
                                 const Sembol *yapi_sem,
                                 const TipBilgisi *yapi_tipi);

/* Forward (ADIM 15.5: bound check) */
static const char *tip_dugumu_kok_adi(const Dugum *t, int *out_uz);

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

        case DUGUM_TIP_TEKKEZ: {
            TipBilgisi *ic = ast_tip_to_bilgi(tk,
                tip_d->veri.tip_tekkez.ic_tip);
            /* Linear Types Spec V1: tekkez<tekkez<T>> destekli */
            return tip_olustur_tekkez(tk->arena, ic);
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
                    /* Bound kontrolu: yapi tanimindaki her tip_param icin,
                     * arg o param'in bound'lari karsiliyor mu? */
                    const Dugum *yapi_d = s->ast_dugumu;
                    if (yapi_d && yapi_d->tip == DUGUM_YAPI &&
                        yapi_d->veri.yapi.tip_param_bound_sayilari) {
                        int param_n = yapi_d->veri.yapi.tip_param_sayi;
                        int eslesen = (param_n < n) ? param_n : n;
                        for (int pi = 0; pi < eslesen; pi++) {
                            int bs = yapi_d->veri.yapi.tip_param_bound_sayilari[pi];
                            if (bs == 0 || !args || !args[pi]) continue;
                            /* args[pi]'nin adini al */
                            const char *arg_ad = NULL;
                            int arg_uz = 0;
                            if (args[pi]->kategori == TIP_YAPI) {
                                arg_ad = args[pi]->veri.yapi.ad;
                                arg_uz = args[pi]->veri.yapi.ad_uzunluk;
                            } else if (args[pi]->kategori == TIP_GENERIC_PARAM) {
                                arg_ad = args[pi]->veri.generic_param.ad;
                                arg_uz = args[pi]->veri.generic_param.ad_uzunluk;
                            }
                            if (!arg_ad) continue;
                            /* Her bound icin tablo kontrolu */
                            for (int bi = 0; bi < bs; bi++) {
                                const Dugum *bd =
                                    yapi_d->veri.yapi.tip_param_boundlari[pi][bi];
                                int bd_uz = 0;
                                const char *bd_ad = tip_dugumu_kok_adi(bd, &bd_uz);
                                if (!bd_ad) continue;
                                /* Ozellik var mi? */
                                const Sembol *oz_s = sembol_bul(tk->global_scope,
                                                                bd_ad, bd_uz);
                                if (!oz_s || oz_s->kategori != SEMBOL_OZELLIK) {
                                    tip_hata(tk, tip_d, "T031",
                                        "bilinmeyen ozellik (bound olarak)");
                                    continue;
                                }
                                /* Generic param ise bound'u kendisi sahip oluyor
                                 * varsayilir (resolve sirasinda enclosing scope) */
                                if (args[pi]->kategori == TIP_GENERIC_PARAM) {
                                    continue;
                                }
                                if (!uygula_tablosu_implementations_eder(
                                        &tk->uygulamalar,
                                        arg_ad, arg_uz, bd_ad, bd_uz)) {
                                    tip_hata(tk, tip_d, "T030",
                                        "tip argumani bound karsilamiyor "
                                        "(uygula bildirimi yok)");
                                }
                            }
                        }
                    }
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

/* Yardimci: yapi olusturma alan kontrolu (beklenen tip varsa generic
 * substitusyon yapilir — Kutu<tam32> { eleman: 5 } gibi durumlarda) */
static TipBilgisi *kontrol_yapi_olustur_ic(TipKontrol *tk, const Dugum *d,
                                            const TipBilgisi *beklenen) {
    const char *tip_ad = d->veri.yapi_olustur.tip_ad;
    int tip_ad_uz = d->veri.yapi_olustur.tip_ad_uzunluk;
    const Sembol *yapi_sem = sembol_bul(tk->scope, tip_ad, tip_ad_uz);
    if (!yapi_sem || yapi_sem->kategori != SEMBOL_YAPI) {
        tip_hata(tk, d, "T002", "yapi tipi tanimsiz");
        return t_hata(tk);
    }
    /* Generic substitusyon kaynagi */
    int generic_var = (beklenen && beklenen->kategori == TIP_YAPI &&
                       beklenen->veri.yapi.tip_arg_sayi > 0);

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
        /* Generic substitusyon (beklenen Kutu<tam32> ise alan T -> tam32) */
        TipBilgisi *alan_tipi = generic_var
            ? substitusyon(tk, alan->tip, yapi_sem, beklenen)
            : alan->tip;
        /* Bidirectional: alan degeri alan tipi context'inde */
        TipBilgisi *deger_tip = tip_belirle_beklenen(tk,
            aa->veri.alan_atama.deger, alan_tipi);
        if (!tip_esit(alan_tipi, deger_tip) &&
            deger_tip->kategori != TIP_HATA) {
            tip_hata(tk, aa, "T001", "alan tipi uyumsuz");
            hata = 1;
        }
        /* Linear Types Spec V1: alan tekkez ise deger baglamadan move */
        if (alan_tipi && alan_tipi->kategori == TIP_TEKKEZ) {
            lineer_tuket_eger_baglamaysa(tk, aa->veri.alan_atama.deger);
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
    /* Beklenen tipte tip_arg varsa concrete instance'i don */
    if (generic_var) {
        return tip_olustur_yapi(tk->arena, yapi_sem->ad, yapi_sem->ad_uzunluk,
                                beklenen->veri.yapi.tip_arg,
                                beklenen->veri.yapi.tip_arg_sayi);
    }
    return tip_olustur_yapi(tk->arena, yapi_sem->ad, yapi_sem->ad_uzunluk,
                            NULL, 0);
}

/* Geriye uyumlu wrapper (mevcut tip_belirle cagrilari icin) */
static TipBilgisi *kontrol_yapi_olustur(TipKontrol *tk, const Dugum *d) {
    return kontrol_yapi_olustur_ic(tk, d, NULL);
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
            /* hiç -> seçimlik<T> none */
            if (d->veri.tanimlayici.uzunluk == 4 /* hiç = h+i+c+'̧'? UTF-8 = 4 byte */ &&
                memcmp(d->veri.tanimlayici.metin, "hi\xc3\xa7", 4) == 0) {
                /* T inference: beklenen tip varsa kullan, yoksa BILINMIYOR */
                TipBilgisi *ic = tip_olustur_basit(tk->arena, TIP_BILINMIYOR);
                return tip_olustur_secimlik(tk->arena, ic);
            }
            const Sembol *s = sembol_bul(tk->scope,
                d->veri.tanimlayici.metin, d->veri.tanimlayici.uzunluk);
            if (!s) {
                tip_hata(tk, d, "T002", "tanimsiz sembol");
                return t_hata(tk);
            }
            /* Linear Types Spec V1: lambda govdesi icindeki lineer
             * baglamalar otomatik 'yakalama' sayilir → consume + closure
             * tipi tekkez<...> olarak isaretlenir (LC-2). */
            lineer_yakalama_kontrol(tk, d);
            return s->tip ? s->tip : t_hata(tk);
        }

        /* === Linear Types Spec V1: kullan(e) extract === */
        case DUGUM_KULLAN_IFADE: {
            TipBilgisi *t = tip_belirle(tk, d->veri.kullan_ifade.operand);
            if (t->kategori == TIP_HATA) return t_hata(tk);
            if (t->kategori != TIP_TEKKEZ) {
                tip_hata(tk, d, "L007",
                    "kullan(...) operandi tekkez tipinde olmali");
                return t_hata(tk);
            }
            lineer_tuket_eger_baglamaysa(tk, d->veri.kullan_ifade.operand);
            return t->veri.tekkez.ic;
        }

        /* === Linear Types Spec V1: imha(e) dispose === */
        case DUGUM_IMHA_IFADE: {
            TipBilgisi *t = tip_belirle(tk, d->veri.imha_ifade.operand);
            if (t->kategori == TIP_HATA) return t_hata(tk);
            if (t->kategori != TIP_TEKKEZ) {
                tip_hata(tk, d, "L007",
                    "imha(...) operandi tekkez tipinde olmali");
                return t_hata(tk);
            }
            lineer_tuket_eger_baglamaysa(tk, d->veri.imha_ifade.operand);
            return t_basit(tk, TIP_BOS);
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

                case OP_BIT_VE: case OP_BIT_VEYA: case OP_BIT_OZVEYA: {
                    /* Bit AND/OR/XOR: her iki operand tamsayi, ayni tip.
                     * Bidirectional: sag, sol tipi context'inde yeniden
                     * cikarsanir — literal'ler sol tipe kayar (page table). */
                    if (!tip_tamsayi_mi(sol)) {
                        tip_hata(tk, d, "T028",
                                 "bit operatoru (& | ^) tamsayi tipi ister");
                        return t_hata(tk);
                    }
                    TipBilgisi *sag2 = tip_belirle_beklenen(tk,
                        d->veri.ikili.sag, sol);
                    if (!tip_tamsayi_mi(sag2)) {
                        tip_hata(tk, d, "T028",
                                 "bit operatoru (& | ^) tamsayi tipi ister");
                        return t_hata(tk);
                    }
                    if (!tip_esit(sol, sag2)) {
                        tip_hata(tk, d, "T001",
                                 "bit operatoru iki tarafi ayni tip olmali");
                        return t_hata(tk);
                    }
                    return sol;
                }

                case OP_SOLA_KAYDIR: case OP_SAGA_KAYDIR: {
                    /* Kaydir (<<, >>): sol tamsayi, sag tamsayi (kaydirma
                     * miktari), sonuc sol tarafin tipi. Sag opsiyonel olarak
                     * farkli tamsayi tipi olabilir. */
                    if (!tip_tamsayi_mi(sol)) {
                        tip_hata(tk, d, "T028",
                                 "kaydirma operatoru sol taraf tamsayi ister");
                        return t_hata(tk);
                    }
                    if (!tip_tamsayi_mi(sag)) {
                        tip_hata(tk, d, "T028",
                                 "kaydirma miktari tamsayi olmali");
                        return t_hata(tk);
                    }
                    return sol;
                }

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

                case OP_BIT_DEGIL:
                    if (!tip_tamsayi_mi(op)) {
                        tip_hata(tk, d, "T028",
                                 "bit DEGIL (~) tamsayi tipi ister");
                        return t_hata(tk);
                    }
                    return op;

                case OP_REF:
                    /* Linear Types Spec V1 L004: tekkez referans alinamaz */
                    if (op->kategori == TIP_TEKKEZ) {
                        tip_hata(tk, d, "L004",
                            "lineer (tekkez) tipinde referans alinamaz");
                        return t_hata(tk);
                    }
                    return tip_olustur_referans(tk->arena, op, 0);

                case OP_REF_DEGISKEN:
                    if (op->kategori == TIP_TEKKEZ) {
                        tip_hata(tk, d, "L004",
                            "lineer (tekkez) tipinde &degisken alinamaz");
                        return t_hata(tk);
                    }
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
            /* Yerlesik konstrüktörler: değer(x), tamam(x), hata(x) */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.sayi == 1) {
                const char *ad = d->veri.cagri.hedef->veri.tanimlayici.metin;
                int uz = d->veri.cagri.hedef->veri.tanimlayici.uzunluk;
                /* "değer" = de\xc4\x9fer (6 byte) */
                if (uz == 6 && memcmp(ad, "de\xc4\x9f" "er", 6) == 0) {
                    TipBilgisi *iç = tip_belirle(tk,
                        d->veri.cagri.argumanlar[0]);
                    return tip_olustur_secimlik(tk->arena, iç);
                }
                /* "tamam" (5 byte) */
                if (uz == 5 && memcmp(ad, "tamam", 5) == 0) {
                    TipBilgisi *deg = tip_belirle(tk,
                        d->veri.cagri.argumanlar[0]);
                    TipBilgisi *hata = tip_olustur_basit(tk->arena,
                        TIP_BILINMIYOR);
                    return tip_olustur_sonuc(tk->arena, deg, hata);
                }
                /* "hata" (4 byte) */
                if (uz == 4 && memcmp(ad, "hata", 4) == 0) {
                    TipBilgisi *h = tip_belirle(tk,
                        d->veri.cagri.argumanlar[0]);
                    TipBilgisi *deg = tip_olustur_basit(tk->arena,
                        TIP_BILINMIYOR);
                    return tip_olustur_sonuc(tk->arena, deg, h);
                }
            }
            /* === Madde B: Dinamik dizi intrinsicleri === */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI) {
                const char *ad_b = d->veri.cagri.hedef->veri.tanimlayici.metin;
                int uz_b = d->veri.cagri.hedef->veri.tanimlayici.uzunluk;

                /* dizi_olustur<T>(N: tam64) -> Dizi<T>
                 * T context'ten (beklenen tip Dizi<T> ise) gelir;
                 * yoksa Dizi<tam32> varsayilir (v1 default). */
                if (uz_b == 12 && memcmp(ad_b, "dizi_olustur", 12) == 0) {
                    if (d->veri.cagri.sayi != 1) {
                        tip_hata(tk, d, "T010",
                            "dizi_olustur tam olarak bir arguman gerektirir");
                        return t_hata(tk);
                    }
                    TipBilgisi *kap = tip_belirle_beklenen(tk,
                        d->veri.cagri.argumanlar[0],
                        tip_olustur_basit(tk->arena, TIP_TAM64));
                    if (kap->kategori != TIP_HATA &&
                        kap->kategori != TIP_TAM64 &&
                        kap->kategori != TIP_TAM32) {
                        tip_hata(tk, d->veri.cagri.argumanlar[0], "T028",
                            "dizi_olustur kapasite tamsayi olmali");
                    }
                    /* T inferred: beklenen ya Dizi<T> ya da default tam32 */
                    TipBilgisi *eleman = NULL;
                    /* Beklenen TipKontrol'un cagiranindan gelmiyor doğrudan;
                     * beklenen Dizi<T> ise context'ten alinabilir — su an
                     * default tam32 (v1; v2'de degisken annot ile baglar) */
                    eleman = tip_olustur_basit(tk->arena, TIP_TAM32);
                    return tip_olustur_dizi(tk->arena, eleman);
                }

                /* dizi_ekle<T>(d: Dizi<T>, e: T) -> bos */
                if (uz_b == 9 && memcmp(ad_b, "dizi_ekle", 9) == 0) {
                    if (d->veri.cagri.sayi != 2) {
                        tip_hata(tk, d, "T010",
                            "dizi_ekle iki arguman gerektirir (d, e)");
                        return t_hata(tk);
                    }
                    TipBilgisi *dt = tip_belirle(tk, d->veri.cagri.argumanlar[0]);
                    if (dt->kategori != TIP_DIZI &&
                        dt->kategori != TIP_HATA) {
                        tip_hata(tk, d->veri.cagri.argumanlar[0], "T001",
                            "dizi_ekle ilk argumani Dizi<T> olmali");
                    }
                    TipBilgisi *bek = (dt->kategori == TIP_DIZI)
                        ? dt->veri.dizi.eleman : NULL;
                    TipBilgisi *et = tip_belirle_beklenen(tk,
                        d->veri.cagri.argumanlar[1], bek);
                    if (bek && !tip_esit(et, bek) &&
                        et->kategori != TIP_HATA) {
                        tip_hata(tk, d->veri.cagri.argumanlar[1], "T001",
                            "dizi_ekle eleman tipi Dizi'nin eleman tipinden farkli");
                    }
                    return tip_olustur_basit(tk->arena, TIP_BOS);
                }

                /* dizi_al<T>(d: Dizi<T>, i: tam32) -> T */
                if (uz_b == 7 && memcmp(ad_b, "dizi_al", 7) == 0) {
                    if (d->veri.cagri.sayi != 2) {
                        tip_hata(tk, d, "T010",
                            "dizi_al iki arguman gerektirir (d, i)");
                        return t_hata(tk);
                    }
                    TipBilgisi *dt = tip_belirle(tk, d->veri.cagri.argumanlar[0]);
                    TipBilgisi *idx = tip_belirle_beklenen(tk,
                        d->veri.cagri.argumanlar[1],
                        tip_olustur_basit(tk->arena, TIP_TAM32));
                    if (!tip_tamsayi_mi(idx) && idx->kategori != TIP_HATA) {
                        tip_hata(tk, d->veri.cagri.argumanlar[1], "T028",
                            "dizi_al indeks tamsayi olmali");
                    }
                    if (dt->kategori == TIP_DIZI) return dt->veri.dizi.eleman;
                    return t_hata(tk);
                }

                /* dizi_boyut(d: Dizi<T>) -> tam32 */
                if (uz_b == 10 && memcmp(ad_b, "dizi_boyut", 10) == 0) {
                    if (d->veri.cagri.sayi != 1) {
                        tip_hata(tk, d, "T010",
                            "dizi_boyut bir arguman gerektirir");
                        return t_hata(tk);
                    }
                    TipBilgisi *dt = tip_belirle(tk, d->veri.cagri.argumanlar[0]);
                    if (dt->kategori != TIP_DIZI &&
                        dt->kategori != TIP_HATA) {
                        tip_hata(tk, d->veri.cagri.argumanlar[0], "T001",
                            "dizi_boyut argumani Dizi<T> olmali");
                    }
                    return tip_olustur_basit(tk->arena, TIP_TAM32);
                }
            }

            /* Linear Types Spec V1 producer intrinsic: tekkez_yarat(e) */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 12 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "tekkez_yarat", 12) == 0) {
                if (d->veri.cagri.sayi != 1) {
                    tip_hata(tk, d, "L008",
                        "tekkez_yarat tam olarak bir arguman gerektirir");
                    return t_hata(tk);
                }
                TipBilgisi *ic = tip_belirle(tk, d->veri.cagri.argumanlar[0]);
                if (ic->kategori == TIP_HATA) return t_hata(tk);
                /* tekkez<tekkez<T>> destekli: ic lineer ise wrap edilmeden
                 * once tuketmeliyiz (move into outer wrapper). */
                if (ic->kategori == TIP_TEKKEZ) {
                    lineer_tuket_eger_baglamaysa(tk,
                        d->veri.cagri.argumanlar[0]);
                }
                return tip_olustur_tekkez(tk->arena, ic);
            }
            /* Method dispatch: hedef DUGUM_ERISIM ise (x.method())
             * x'in yapi tipi uzerinde method bul. */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_ERISIM) {
                const Dugum *erisim = d->veri.cagri.hedef;
                TipBilgisi *nesne_tip = tip_belirle(tk, erisim->veri.erisim.nesne);
                if (nesne_tip->kategori == TIP_REFERANS) {
                    nesne_tip = nesne_tip->veri.referans.hedef;
                }
                if (nesne_tip->kategori == TIP_YAPI) {
                    const Dugum *m = uygula_tablosu_method_bul(
                        &tk->uygulamalar,
                        nesne_tip->veri.yapi.ad,
                        nesne_tip->veri.yapi.ad_uzunluk,
                        erisim->veri.erisim.alan,
                        erisim->veri.erisim.alan_uzunluk);
                    if (m) {
                        /* Method bulundu — kendin parametresi otomatik (receiver)
                         * Aksi halde normal arg sayisi kontrolu */
                        int n = m->veri.islev.param_sayi;
                        int has_kendin = (n > 0 &&
                            m->veri.islev.parametreler[0]->veri.parametre.kendin_mi);
                        int beklenen_arg = has_kendin ? n - 1 : n;
                        if (d->veri.cagri.sayi != beklenen_arg) {
                            tip_hata(tk, d, "T010",
                                "method cagri arguman sayisi uyumsuz");
                            return t_hata(tk);
                        }
                        int offset = has_kendin ? 1 : 0;
                        for (int i = 0; i < beklenen_arg; i++) {
                            const Dugum *p = m->veri.islev.parametreler[i + offset];
                            TipBilgisi *pt = ast_tip_to_bilgi(tk,
                                p->veri.parametre.tip);
                            TipBilgisi *at = tip_belirle_beklenen(tk,
                                d->veri.cagri.argumanlar[i], pt);
                            if (!tip_esit(at, pt) &&
                                at->kategori != TIP_HATA) {
                                tip_hata(tk, d->veri.cagri.argumanlar[i],
                                    "T001", "method arg tipi uyumsuz");
                            }
                        }
                        if (m->veri.islev.donus_tipi) {
                            return ast_tip_to_bilgi(tk, m->veri.islev.donus_tipi);
                        }
                        return tip_olustur_basit(tk->arena, TIP_BOS);
                    }
                    /* Method bulunamadi — duser asagi normal alan erisim
                     * yoluna (DUGUM_ERISIM tip_belirle), oradan hata gelir. */
                }
            }
            TipBilgisi *hedef_tip = tip_belirle(tk, d->veri.cagri.hedef);
            if (hedef_tip->kategori == TIP_HATA) return t_hata(tk);

            /* Linear Types Spec V1 LC-3: tekkez<islev(...)> cagri = consume.
             * Hedef DUGUM_TANIMLAYICI ise sembolu tuket. */
            if (hedef_tip->kategori == TIP_TEKKEZ &&
                hedef_tip->veri.tekkez.ic &&
                hedef_tip->veri.tekkez.ic->kategori == TIP_ISLEV) {
                lineer_tuket_eger_baglamaysa(tk, d->veri.cagri.hedef);
                hedef_tip = hedef_tip->veri.tekkez.ic;
            }

            if (hedef_tip->kategori != TIP_ISLEV) {
                tip_hata(tk, d, "T006", "cagri icin islev tipi gerek");
                return t_hata(tk);
            }
            if (d->veri.cagri.sayi != hedef_tip->veri.islev.param_sayi) {
                tip_hata(tk, d, "T010", "cagri arguman sayisi uyumsuz");
                return t_hata(tk);
            }
            /* Generic islev: ilk TIP_GENERIC_PARAM gorulen yere ilk uygun
             * argumanin tipini bagla (basit inference — tek param T icin
             * cogu pratik durumda yeterli). */
            TipBilgisi *inferred_T = NULL;
            for (int i = 0; i < d->veri.cagri.sayi; i++) {
                TipBilgisi *param_tip = hedef_tip->veri.islev.parametreler[i];
                /* Bidirectional: arg, parametre tipi context'inde cikarsanir */
                TipBilgisi *bek = param_tip;
                if (param_tip->kategori == TIP_GENERIC_PARAM && inferred_T) {
                    bek = inferred_T;
                }
                TipBilgisi *arg_tip = tip_belirle_beklenen(tk,
                    d->veri.cagri.argumanlar[i], bek);
                if (param_tip->kategori == TIP_GENERIC_PARAM) {
                    if (!inferred_T) inferred_T = arg_tip;
                    /* Generic params herhangi bir tipi kabul eder */
                    continue;
                }
                if (!tip_esit(arg_tip, param_tip) &&
                    arg_tip->kategori != TIP_HATA) {
                    tip_hata(tk, d->veri.cagri.argumanlar[i], "T001",
                             "arguman tipi parametre tipi ile uyumsuz");
                }
                /* Linear Types Spec V1: param tekkez ise arg consume */
                if (param_tip && param_tip->kategori == TIP_TEKKEZ) {
                    lineer_tuket_eger_baglamaysa(tk,
                        d->veri.cagri.argumanlar[i]);
                }
            }
            /* Donus tipi generic ise inferred T ile degistir */
            TipBilgisi *donus = hedef_tip->veri.islev.donus;
            if (donus && donus->kategori == TIP_GENERIC_PARAM && inferred_T) {
                return inferred_T;
            }
            return donus;
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
            /* Generic instantiation: yapi.tip_arg varsa alan tipinde
             * substitusyon (T -> tam32 vs) */
            if (alan->tip && nesne_tip->veri.yapi.tip_arg_sayi > 0) {
                return substitusyon(tk, alan->tip, yapi_sem, nesne_tip);
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
            /* Linear Types Spec V1 LR-2: dizi tekkez eleman iceremez (V1) */
            if (ilk && ilk->kategori == TIP_TEKKEZ) {
                tip_hata(tk, d, "LR002",
                    "dizi elemani tekkez tipinde olamaz (V1: dizi lineer eleman iceremez)");
            }
            return tip_olustur_dizi(tk->arena, ilk);
        }

        /* === Lambda — Linear Types Spec V1 LC-2: closure-itself-linear === */
        case DUGUM_LAMBDA: {
            int n = d->veri.lambda.param_sayi;
            TipBilgisi **params = NULL;
            if (n > 0) {
                params = (TipBilgisi **)arena_ayir(tk->arena,
                            sizeof(TipBilgisi *) * (size_t)n);
            }

            /* Lambda kendi scope'unu ac */
            Scope *eski_scope = tk->scope;
            Scope *lambda_scope = scope_olustur(tk->arena, SCOPE_BLOK,
                                                 eski_scope);
            tk->scope = lambda_scope;

            for (int i = 0; i < n; i++) {
                const Dugum *p = d->veri.lambda.parametreler[i];
                if (!p->veri.parametre.tip) {
                    tip_hata(tk, p, "T015",
                             "lambda parametre tip annotasyonu gerek");
                    params[i] = t_hata(tk);
                    continue;
                }
                params[i] = ast_tip_to_bilgi(tk, p->veri.parametre.tip);
                /* Parametre sembolu ekle */
                Sembol ps;
                memset(&ps, 0, sizeof(ps));
                ps.ad = p->veri.parametre.ad;
                ps.ad_uzunluk = p->veri.parametre.ad_uzunluk;
                ps.kategori = SEMBOL_PARAMETRE;
                ps.tip = params[i];
                ps.satir = p->satir;
                ps.sutun = p->sutun;
                sembol_ekle(lambda_scope, tk->arena, &ps);
            }
            /* Closure-itself-linear takip: lambda govdesi flag'leri */
            int eski_lambda = tk->lambda_govdesi_icinde;
            int eski_yakalama = tk->lambda_lineer_yakalama;
            Scope *eski_baslangic = tk->lambda_baslangic_scope;
            tk->lambda_govdesi_icinde = 1;
            tk->lambda_lineer_yakalama = 0;
            tk->lambda_baslangic_scope = eski_scope;

            /* Govde icin lambda_scope uzerinde yeni gövde scope (ADIM 29:
             * lambda govde scope v1 — gövde içi degisken bildirimleri
             * parametre scope'unu kirletmesin). */
            Scope *gov_eski = tk->scope;
            tk->scope = scope_olustur(tk->arena, SCOPE_ISLEV, gov_eski);
            for (int i = 0; i < n; i++) {
                const Dugum *p = d->veri.lambda.parametreler[i];
                Sembol s;
                memset(&s, 0, sizeof(s));
                s.ad = p->veri.parametre.ad;
                s.ad_uzunluk = p->veri.parametre.ad_uzunluk;
                s.kategori = SEMBOL_PARAMETRE;
                s.tip = params[i];
                s.ast_dugumu = p;
                sembol_ekle(tk->scope, tk->arena, &s);
            }
            TipBilgisi *donus = tip_belirle(tk, d->veri.lambda.govde);
            tk->scope = gov_eski;

            int yakaladi = tk->lambda_lineer_yakalama;

            /* Lambda parametreleri lineer ise govde icinde tuketilmeli (L001) */
            scope_lineer_kapanis_check(tk, lambda_scope);

            tk->lambda_govdesi_icinde = eski_lambda;
            tk->lambda_lineer_yakalama = eski_yakalama;
            tk->lambda_baslangic_scope = eski_baslangic;
            tk->scope = eski_scope;

            TipBilgisi *islev_t =
                tip_olustur_islev(tk->arena, params, n, donus);

            /* LC-2: lineer yakalama varsa lambda kendisi tekkez */
            if (yakaladi) {
                return tip_olustur_tekkez(tk->arena, islev_t);
            }
            return islev_t;
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
 * ADIM 11.6: Generic instantiation (substitusyon)
 * ======================================================================== */

/* TIP_GENERIC_PARAM'leri concrete arg'larla recursive olarak substitute et.
 * yapi_sem: generic params'in tanimlandigi yapi sembolu
 * yapi_tipi: concrete tip (tip_arg ile) — substitusyon kaynagi */
static TipBilgisi *substitusyon(TipKontrol *tk, const TipBilgisi *t,
                                 const Sembol *yapi_sem,
                                 const TipBilgisi *yapi_tipi) {
    if (!t) return NULL;
    if (!yapi_sem || !yapi_tipi ||
        yapi_tipi->kategori != TIP_YAPI ||
        yapi_tipi->veri.yapi.tip_arg_sayi == 0) {
        return (TipBilgisi *)t;
    }

    if (t->kategori == TIP_GENERIC_PARAM) {
        /* Yapi_scope'taki generic params -> tip_arg sirasi ile esle */
        int idx = 0;
        for (SembolLink *l = yapi_sem->yapi_scope->bas; l; l = l->sonraki) {
            if (l->sembol.kategori != SEMBOL_GENERIC_PARAM) continue;
            if (l->sembol.ad_uzunluk == t->veri.generic_param.ad_uzunluk &&
                memcmp(l->sembol.ad, t->veri.generic_param.ad,
                       (size_t)t->veri.generic_param.ad_uzunluk) == 0) {
                if (idx < yapi_tipi->veri.yapi.tip_arg_sayi) {
                    return yapi_tipi->veri.yapi.tip_arg[idx];
                }
            }
            idx++;
        }
        return (TipBilgisi *)t;
    }

    /* Recursive substitusyon (referans, pointer, dizi, secimlik, sonuc) */
    switch (t->kategori) {
        case TIP_REFERANS: {
            TipBilgisi *nh = substitusyon(tk, t->veri.referans.hedef,
                                           yapi_sem, yapi_tipi);
            if (nh == t->veri.referans.hedef) return (TipBilgisi *)t;
            return tip_olustur_referans(tk->arena, nh,
                                         t->veri.referans.degisken_mi);
        }
        case TIP_POINTER: {
            TipBilgisi *nh = substitusyon(tk, t->veri.pointer.hedef,
                                           yapi_sem, yapi_tipi);
            if (nh == t->veri.pointer.hedef) return (TipBilgisi *)t;
            return tip_olustur_pointer(tk->arena, nh);
        }
        case TIP_DIZI: {
            TipBilgisi *ne = substitusyon(tk, t->veri.dizi.eleman,
                                           yapi_sem, yapi_tipi);
            if (ne == t->veri.dizi.eleman) return (TipBilgisi *)t;
            return tip_olustur_dizi(tk->arena, ne);
        }
        case TIP_SECIMLIK: {
            TipBilgisi *ni = substitusyon(tk, t->veri.secimlik.ic,
                                           yapi_sem, yapi_tipi);
            if (ni == t->veri.secimlik.ic) return (TipBilgisi *)t;
            return tip_olustur_secimlik(tk->arena, ni);
        }
        case TIP_SONUC: {
            TipBilgisi *nd = substitusyon(tk, t->veri.sonuc.deger,
                                           yapi_sem, yapi_tipi);
            TipBilgisi *nh = substitusyon(tk, t->veri.sonuc.hata,
                                           yapi_sem, yapi_tipi);
            if (nd == t->veri.sonuc.deger && nh == t->veri.sonuc.hata)
                return (TipBilgisi *)t;
            return tip_olustur_sonuc(tk->arena, nd, nh);
        }
        default:
            return (TipBilgisi *)t;
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

        case DUGUM_YAPI_OLUSTUR:
            if (beklenen->kategori == TIP_YAPI) {
                return kontrol_yapi_olustur_ic(tk, d, beklenen);
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

        case DUGUM_CAGRI: {
            /* Madde B: dizi_olustur<T>(N) beklenen Dizi<T> ise T'yi kullan.
             * Context-driven instantiation. */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 12 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "dizi_olustur", 12) == 0 &&
                beklenen->kategori == TIP_DIZI &&
                d->veri.cagri.sayi == 1) {
                /* Kapasiteyi tam64 olarak kontrol et */
                tip_belirle_beklenen(tk, d->veri.cagri.argumanlar[0],
                    tip_olustur_basit(tk->arena, TIP_TAM64));
                return tip_olustur_dizi(tk->arena,
                    (TipBilgisi *)beklenen->veri.dizi.eleman);
            }
            break;
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
        /* Linear Types Spec V1 LR-2: yapi tekkez alan iceremez (V1) */
        if (alan_tipi && alan_tipi->kategori == TIP_TEKKEZ) {
            tip_hata(tk, alan, "LR002",
                "yapi alani tekkez tipinde olamaz (V1: yapi lineer alan iceremez)");
        }
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
    /* Generic params: gecici scope'a ekle (parametre/donus tipi resolve icin) */
    Scope *gp_scope = NULL;
    if (islev->veri.islev.tip_param_sayi > 0) {
        gp_scope = scope_olustur(tk->arena, SCOPE_BLOK, tk->global_scope);
        for (int j = 0; j < islev->veri.islev.tip_param_sayi; j++) {
            const char *t_ad = islev->veri.islev.tip_paramlar[j];
            int t_uz = (int)strlen(t_ad);
            Sembol gp;
            memset(&gp, 0, sizeof(gp));
            gp.ad = t_ad;
            gp.ad_uzunluk = t_uz;
            gp.kategori = SEMBOL_GENERIC_PARAM;
            gp.tip = tip_olustur_generic_param(tk->arena, t_ad, t_uz);
            sembol_ekle(gp_scope, tk->arena, &gp);
        }
    }
    Scope *eski_scope = tk->scope;
    if (gp_scope) tk->scope = gp_scope;
    for (int j = 0; j < n; j++) {
        const Dugum *p = islev->veri.islev.parametreler[j];
        if (p->veri.parametre.kendin_mi) {
            /* kendin parametre: tipi uygula context'inde belirlenir,
             * pre-populate'da henuz bilinmiyor — bilinmeyen olarak isaretle */
            ptipler[j] = tip_olustur_basit(tk->arena, TIP_BILINMIYOR);
            continue;
        }
        ptipler[j] = ast_tip_to_bilgi(tk, p->veri.parametre.tip);
    }
    TipBilgisi *donus = islev->veri.islev.donus_tipi
        ? ast_tip_to_bilgi(tk, islev->veri.islev.donus_tipi)
        : tip_olustur_basit(tk->arena, TIP_BOS);
    tk->scope = eski_scope;
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

static void pre_populate_ozellik(TipKontrol *tk, const Dugum *oz) {
    /* Ozellik sembolunu global'e ekle (bound olarak referans gerekli) */
    Sembol s;
    memset(&s, 0, sizeof(s));
    s.ad = oz->veri.ozellik.ad;
    s.ad_uzunluk = oz->veri.ozellik.ad_uzunluk;
    s.kategori = SEMBOL_OZELLIK;
    s.ast_dugumu = oz;
    s.satir = oz->satir;
    s.sutun = oz->sutun;
    if (sembol_ekle(tk->global_scope, tk->arena, &s) != 0) {
        tip_hata(tk, oz, "T024", "ozellik tanimi cakismasi");
    }
}

/* AST tip dugumunden 'kok' adi cikar (basit/kullanici). Bilinmiyorsa NULL. */
static const char *tip_dugumu_kok_adi(const Dugum *t, int *out_uz) {
    if (!t) return NULL;
    if (t->tip == DUGUM_TIP_BASIT) {
        *out_uz = t->veri.tip_basit.ad_uzunluk;
        return t->veri.tip_basit.ad;
    }
    if (t->tip == DUGUM_TIP_KULLANICI && t->veri.tip_kullanici.yol) {
        const Dugum *y = t->veri.tip_kullanici.yol;
        if (y->tip == DUGUM_TANIMLAYICI) {
            *out_uz = y->veri.tanimlayici.uzunluk;
            return y->veri.tanimlayici.metin;
        }
    }
    return NULL;
}

static void pre_populate_uygula(TipKontrol *tk, const Dugum *uy) {
    /* Hedef tip adi */
    int tip_uz = 0;
    const char *tip_ad = tip_dugumu_kok_adi(uy->veri.uygula.tip, &tip_uz);
    if (!tip_ad) return;

    if (uy->veri.uygula.ozellik_sayi == 0) {
        /* Inherent impl */
        uygula_tablosu_ekle(&tk->uygulamalar, tk->arena,
                            tip_ad, tip_uz, NULL, 0, uy);
    } else {
        /* Trait impls */
        for (int i = 0; i < uy->veri.uygula.ozellik_sayi; i++) {
            int oz_uz = 0;
            const char *oz_ad = tip_dugumu_kok_adi(
                uy->veri.uygula.ozellikler[i], &oz_uz);
            if (oz_ad) {
                uygula_tablosu_ekle(&tk->uygulamalar, tk->arena,
                                    tip_ad, tip_uz, oz_ad, oz_uz, uy);
            }
        }
    }
}

static void pre_populate(TipKontrol *tk, const Dugum *program) {
    if (!program || program->tip != DUGUM_PROGRAM) return;

    /* 1) Once ozellikleri ekle (bound referansi icin) */
    for (int i = 0; i < program->veri.program.sayi; i++) {
        const Dugum *uye = program->veri.program.uyeler[i];
        if (uye->tip == DUGUM_OZELLIK) pre_populate_ozellik(tk, uye);
    }

    /* 2) Yapilari (tipleri) ekle */
    for (int i = 0; i < program->veri.program.sayi; i++) {
        const Dugum *uye = program->veri.program.uyeler[i];
        if (uye->tip == DUGUM_YAPI) pre_populate_yapi(tk, uye);
        else if (uye->tip == DUGUM_DISA &&
                 uye->veri.disa.tanim &&
                 uye->veri.disa.tanim->tip == DUGUM_YAPI) {
            pre_populate_yapi(tk, uye->veri.disa.tanim);
        }
    }

    /* 3) Uygula bildirimlerini kayit et (yapi+ozellik bilindikten sonra) */
    for (int i = 0; i < program->veri.program.sayi; i++) {
        const Dugum *uye = program->veri.program.uyeler[i];
        if (uye->tip == DUGUM_UYGULA) pre_populate_uygula(tk, uye);
    }

    /* 4) Islevler ve sabitler */
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
            /* Linear Types Spec V1: deger lineer baglamadan move ise tuket */
            if (son && son->kategori == TIP_TEKKEZ) {
                lineer_tuket_eger_baglamaysa(tk, d->veri.degisken.deger);
            }
            Sembol s;
            memset(&s, 0, sizeof(s));
            s.ad = d->veri.degisken.ad;
            s.ad_uzunluk = d->veri.degisken.ad_uzunluk;
            s.kategori = SEMBOL_DEGISKEN;
            s.tip = son;
            s.ast_dugumu = d;
            s.satir = d->satir;
            s.sutun = d->sutun;
            s.lineer_scope_seviyesi = tk->scope_seviyesi;
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
                /* Linear Types Spec V1: lineer baglama ver ile cagirana
                 * devredildi → tuket */
                if (deger && deger->kategori == TIP_TEKKEZ) {
                    lineer_tuket_eger_baglamaysa(tk, d->veri.ver.deger);
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
            tk->scope_seviyesi++;
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
            scope_lineer_kapanis_check(tk, tk->scope);
            tk->scope = eski;
            tk->scope_seviyesi--;
            break;
        }

        case DUGUM_ESLES: {
            /* Eşleş'in deger tipini belirle (secimlik<T> veya sonuc<T,H>) */
            TipBilgisi *dt = tip_belirle(tk, d->veri.esles.deger);
            for (int i = 0; i < d->veri.esles.kol_sayi; i++) {
                const Dugum *kol = d->veri.esles.kollar[i];
                Scope *eski = tk->scope;
                tk->scope_seviyesi++;
                tk->scope = scope_olustur(tk->arena, SCOPE_BLOK, eski);

                /* Desen tanimlayici/yapici icindeki adlari scope'a ekle */
                const Dugum *desen = kol->veri.esles_kolu.desen;
                if (desen) {
                    if (desen->tip == DUGUM_DESEN_TANIMLAYICI) {
                        /* x => govde — x'in tipi dt */
                        Sembol s;
                        memset(&s, 0, sizeof(s));
                        s.ad = desen->veri.desen_tanimlayici.ad;
                        s.ad_uzunluk = desen->veri.desen_tanimlayici.ad_uzunluk;
                        s.kategori = SEMBOL_DEGISKEN;
                        s.tip = dt;
                        s.ast_dugumu = desen;
                        sembol_ekle(tk->scope, tk->arena, &s);
                    } else if (desen->tip == DUGUM_DESEN_YAPICI) {
                        /* değer(s) => govde — s'nin tipi secimlik<T>.iç tipi */
                        const char *yapici_ad = desen->veri.desen_yapici.ad;
                        int yapici_uz = desen->veri.desen_yapici.ad_uzunluk;
                        TipBilgisi *icerik_tipi = NULL;
                        /* "değer" -> dt secimlik ise iç */
                        if (yapici_uz == 6 &&
                            memcmp(yapici_ad, "de\xc4\x9f" "er", 6) == 0 &&
                            dt && dt->kategori == TIP_SECIMLIK) {
                            icerik_tipi = dt->veri.secimlik.ic;
                        } else if (yapici_uz == 5 &&
                                   memcmp(yapici_ad, "tamam", 5) == 0 &&
                                   dt && dt->kategori == TIP_SONUC) {
                            icerik_tipi = dt->veri.sonuc.deger;
                        } else if (yapici_uz == 4 &&
                                   memcmp(yapici_ad, "hata", 4) == 0 &&
                                   dt && dt->kategori == TIP_SONUC) {
                            icerik_tipi = dt->veri.sonuc.hata;
                        }
                        /* alt_desenler[0] tanimlayici ise bind et */
                        if (icerik_tipi &&
                            desen->veri.desen_yapici.sayi > 0) {
                            const Dugum *alt =
                                desen->veri.desen_yapici.alt_desenler[0];
                            if (alt && alt->tip == DUGUM_DESEN_TANIMLAYICI) {
                                Sembol s;
                                memset(&s, 0, sizeof(s));
                                s.ad = alt->veri.desen_tanimlayici.ad;
                                s.ad_uzunluk = alt->veri.desen_tanimlayici.ad_uzunluk;
                                s.kategori = SEMBOL_DEGISKEN;
                                s.tip = icerik_tipi;
                                s.ast_dugumu = alt;
                                sembol_ekle(tk->scope, tk->arena, &s);
                            }
                        }
                    }
                    /* DESEN_LITERAL, DESEN_JOKER: binding yok */
                }

                tip_kontrol_deyim(tk, kol->veri.esles_kolu.govde);
                scope_lineer_kapanis_check(tk, tk->scope);
                tk->scope = eski;
                tk->scope_seviyesi--;
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
            tk->scope_seviyesi++;
            tk->scope = scope_olustur(tk->arena, SCOPE_BLOK, eski);
            for (int i = 0; i < d->veri.blok.sayi; i++) {
                tip_kontrol_deyim(tk, d->veri.blok.deyimler[i]);
            }
            /* Linear Types Spec V1 LR-3: bolge (blok scope) kapanirken
             * tum lineer baglamalar tuketilmis olmali (L001 / LR001). */
            scope_lineer_kapanis_check(tk, tk->scope);
            tk->scope = eski;
            tk->scope_seviyesi--;
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
            tk->scope_seviyesi++;
            tk->scope = scope_olustur(tk->arena, SCOPE_ISLEV, tk->global_scope);
            tk->aktif_donus_tipi = islev_sem->tip->veri.islev.donus;

            /* Generic params'i govde scope'una ekle */
            for (int i = 0; i < d->veri.islev.tip_param_sayi; i++) {
                const char *t_ad = d->veri.islev.tip_paramlar[i];
                int t_uz = (int)strlen(t_ad);
                Sembol gp;
                memset(&gp, 0, sizeof(gp));
                gp.ad = t_ad;
                gp.ad_uzunluk = t_uz;
                gp.kategori = SEMBOL_GENERIC_PARAM;
                gp.tip = tip_olustur_generic_param(tk->arena, t_ad, t_uz);
                sembol_ekle(tk->scope, tk->arena, &gp);
            }

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

            /* Linear Types Spec V1: lineer parametreler govdede tuketilmeli */
            scope_lineer_kapanis_check(tk, tk->scope);

            tk->aktif_donus_tipi = eski_donus;
            tk->scope = eski;
            tk->scope_seviyesi--;
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

        case DUGUM_OZELLIK: {
            /* Ozellik gövdesindeki default impl'leri tip kontrol et.
             * Method imzasi olanlar (govdesiz) atlanir. */
            for (int i = 0; i < d->veri.ozellik.uye_sayi; i++) {
                const Dugum *m = d->veri.ozellik.uyeler[i];
                if (!m || m->tip != DUGUM_ISLEV) continue;
                if (!m->veri.islev.govde) continue;  /* imza */

                /* Method gövdesi icin scope kur */
                Scope *eski = tk->scope;
                TipBilgisi *eski_donus = tk->aktif_donus_tipi;
                tk->scope = scope_olustur(tk->arena, SCOPE_ISLEV,
                                          tk->global_scope);
                tk->aktif_donus_tipi = m->veri.islev.donus_tipi
                    ? ast_tip_to_bilgi(tk, m->veri.islev.donus_tipi)
                    : tip_olustur_basit(tk->arena, TIP_BOS);

                for (int j = 0; j < m->veri.islev.param_sayi; j++) {
                    const Dugum *p = m->veri.islev.parametreler[j];
                    Sembol s;
                    memset(&s, 0, sizeof(s));
                    s.ad = p->veri.parametre.ad;
                    s.ad_uzunluk = p->veri.parametre.ad_uzunluk;
                    s.kategori = SEMBOL_PARAMETRE;
                    s.tip = ast_tip_to_bilgi(tk, p->veri.parametre.tip);
                    s.ast_dugumu = p;
                    sembol_ekle(tk->scope, tk->arena, &s);
                }
                tip_kontrol_deyim(tk, m->veri.islev.govde);
                tk->aktif_donus_tipi = eski_donus;
                tk->scope = eski;
            }
            break;
        }

        case DUGUM_UYGULA: {
            /* uygula gövdesindeki islev tanimlarini tip-kontrol et.
             * Generic params (uygula<T> Tip<T>): kendi scope'larina T eklenir. */
            Scope *eski = tk->scope;
            tk->scope = scope_olustur(tk->arena, SCOPE_BLOK, tk->global_scope);
            /* Generic param'lar: T -> TIP_GENERIC_PARAM */
            for (int i = 0; i < d->veri.uygula.tip_param_sayi; i++) {
                const char *t_ad = d->veri.uygula.tip_paramlar[i];
                int t_uz = (int)strlen(t_ad);
                Sembol gp;
                memset(&gp, 0, sizeof(gp));
                gp.ad = t_ad;
                gp.ad_uzunluk = t_uz;
                gp.kategori = SEMBOL_GENERIC_PARAM;
                gp.tip = tip_olustur_generic_param(tk->arena, t_ad, t_uz);
                sembol_ekle(tk->scope, tk->arena, &gp);
            }

            /* uygula hedef tipi (kendin'in tipi olacak) */
            TipBilgisi *hedef_t = ast_tip_to_bilgi(tk, d->veri.uygula.tip);

            /* Her metodu kontrol et */
            for (int i = 0; i < d->veri.uygula.islev_sayi; i++) {
                const Dugum *m = d->veri.uygula.islevler[i];
                if (!m || m->tip != DUGUM_ISLEV) continue;
                if (!m->veri.islev.govde) continue;

                Scope *eski_m = tk->scope;
                TipBilgisi *eski_donus = tk->aktif_donus_tipi;
                tk->scope = scope_olustur(tk->arena, SCOPE_ISLEV, tk->scope);
                tk->aktif_donus_tipi = m->veri.islev.donus_tipi
                    ? ast_tip_to_bilgi(tk, m->veri.islev.donus_tipi)
                    : tip_olustur_basit(tk->arena, TIP_BOS);

                for (int j = 0; j < m->veri.islev.param_sayi; j++) {
                    const Dugum *p = m->veri.islev.parametreler[j];
                    Sembol s;
                    memset(&s, 0, sizeof(s));
                    s.ad = p->veri.parametre.ad;
                    s.ad_uzunluk = p->veri.parametre.ad_uzunluk;
                    s.kategori = SEMBOL_PARAMETRE;
                    if (p->veri.parametre.kendin_mi) {
                        /* kendin parametresi: tipi uygula.tip (referans olabilir) */
                        TipBilgisi *t = hedef_t;
                        if (p->veri.parametre.referans_mi) {
                            t = tip_olustur_referans(tk->arena, hedef_t,
                                                     p->veri.parametre.degisken_mi);
                        }
                        s.tip = t;
                    } else {
                        s.tip = ast_tip_to_bilgi(tk, p->veri.parametre.tip);
                    }
                    s.ast_dugumu = p;
                    sembol_ekle(tk->scope, tk->arena, &s);
                }
                tip_kontrol_deyim(tk, m->veri.islev.govde);
                tk->aktif_donus_tipi = eski_donus;
                tk->scope = eski_m;
            }
            tk->scope = eski;
            break;
        }

        case DUGUM_KULLAN: {
            /* Yol formati: "x::y::z" -> "x/y/z.kem"
             * Arama sirasi: cari dizin, "stdlib/" prefix'i. */
            const char *y = d->veri.kullan.yol;
            int yu = d->veri.kullan.yol_uzunluk;
            if (!y || yu <= 0) break;

            /* "::" -> "/" donusumu, sonuna ".kem" ekle */
            char dosya_yolu[512];
            int o = 0;
            for (int i = 0; i < yu && o + 6 < (int)sizeof(dosya_yolu); i++) {
                if (i + 1 < yu && y[i] == ':' && y[i + 1] == ':') {
                    dosya_yolu[o++] = '/';
                    i++;
                } else {
                    dosya_yolu[o++] = y[i];
                }
            }
            /* .kem uzantisi */
            const char *uzanti = ".kem";
            for (int k = 0; k < 4 && o + 1 < (int)sizeof(dosya_yolu); k++) {
                dosya_yolu[o++] = uzanti[k];
            }
            dosya_yolu[o] = '\0';

            /* Duplicate kontrol */
            for (YuklenmisModul *m = tk->yuklenmisler; m; m = m->sonraki) {
                if (m->yol_uz == o && memcmp(m->yol, dosya_yolu, (size_t)o) == 0) {
                    return;  /* zaten yuklu */
                }
            }

            /* Dosyayi yukle */
            FILE *fp = fopen(dosya_yolu, "rb");
            if (!fp) {
                tip_hata(tk, d, "T040",
                    "kullan: modül dosyası bulunamadı");
                break;
            }
            fseek(fp, 0, SEEK_END);
            long boyut = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            if (boyut <= 0) { fclose(fp); break; }
            char *kaynak = (char *)arena_ayir(tk->arena, (size_t)boyut + 1);
            if (!kaynak) { fclose(fp); break; }
            fread(kaynak, 1, (size_t)boyut, fp);
            kaynak[boyut] = '\0';
            fclose(fp);

            /* Yüklenmis listesine ekle (duplicate engelleme) */
            YuklenmisModul *ym = (YuklenmisModul *)arena_ayir_sifir(
                tk->arena, sizeof(YuklenmisModul));
            if (ym) {
                char *yol_kopya = (char *)arena_ayir(tk->arena, (size_t)o + 1);
                memcpy(yol_kopya, dosya_yolu, (size_t)o + 1);
                ym->yol = yol_kopya;
                ym->yol_uz = o;
                ym->sonraki = tk->yuklenmisler;
                tk->yuklenmisler = ym;
            }

            /* Parse + tip-kontrol modulu */
            Lexer l;
            lexer_baslat(&l, kaynak, dosya_yolu);
            Parser p;
            parser_baslat(&p, &l, tk->arena, dosya_yolu, kaynak);
            Dugum *mprog = parser_calistir(&p);
            if (mprog && p.hata_sayisi == 0) {
                /* Üst düzey üyeleri pre-populate + tanim-kontrol */
                tip_kontrol_program(tk, mprog);
            }
            break;
        }

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
