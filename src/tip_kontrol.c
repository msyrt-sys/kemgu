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
    /* Adim 2: metin_kucuk_tr / metin_buyuk_tr / *_ascii varyantlari */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("metin_kucuk_tr", 14, p, 1, tip_olustur_basit(a, TIP_METIN));
    }
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("metin_buyuk_tr", 14, p, 1, tip_olustur_basit(a, TIP_METIN));
    }
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("metin_kucuk_ascii", 17, p, 1, tip_olustur_basit(a, TIP_METIN));
    }
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("metin_buyuk_ascii", 17, p, 1, tip_olustur_basit(a, TIP_METIN));
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

    /* === G: Dosya syscall built-in'leri (runtime/kdl_runtime.c) === */

    /* dosya_ac(yol: metin, mod: metin) -> metin  (handle opaque ptr) */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *) * 2);
        p[0] = tip_olustur_basit(a, TIP_METIN);
        p[1] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("dosya_ac", 8, p, 2, tip_olustur_basit(a, TIP_METIN));
    }

    /* dosya_oku(yol: metin) -> metin  (tum dosya icerigini metin olarak) */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("dosya_oku", 9, p, 1, tip_olustur_basit(a, TIP_METIN));
    }

    /* dosya_yaz(handle: metin, icerik: metin) -> tam32  (yazilan byte) */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *) * 2);
        p[0] = tip_olustur_basit(a, TIP_METIN);
        p[1] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("dosya_yaz", 9, p, 2, tip_olustur_basit(a, TIP_TAM32));
    }

    /* dosya_kapat(handle: metin) -> boş */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("dosya_kapat", 11, p, 1, tip_olustur_basit(a, TIP_BOS));
    }

    /* dosya_var_mi(yol: metin) -> mantıksal */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("dosya_var_mi", 12, p, 1, tip_olustur_basit(a, TIP_MANTIKSAL));
    }

    /* dosya_sil(yol: metin) -> tam32 (0 basari, !=0 hata) */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("dosya_sil", 9, p, 1, tip_olustur_basit(a, TIP_TAM32));
    }

    /* dosya_yeniden_adlandir(eski: metin, yeni: metin) -> tam32 */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *) * 2);
        p[0] = tip_olustur_basit(a, TIP_METIN);
        p[1] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("dosya_yeniden_adlandir", 22, p, 2,
                     tip_olustur_basit(a, TIP_TAM32));
    }

    /* dosya_boyut(yol: metin) -> tam64 */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("dosya_boyut", 11, p, 1, tip_olustur_basit(a, TIP_TAM64));
    }

    /* === Adim 1 (OTP CLI): CLI args + OTP yardimcilari === */

    /* arg_sayi() -> tam32 */
    EKLE_BUILTIN("arg_sayi", 8, NULL, 0, tip_olustur_basit(a, TIP_TAM32));

    /* arg_al(i: tam32) -> metin */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_TAM32);
        EKLE_BUILTIN("arg_al", 6, p, 1, tip_olustur_basit(a, TIP_METIN));
    }

    /* otp_anahtar_uret(yol: metin, boyut: tam32) -> tam32 */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *) * 2);
        p[0] = tip_olustur_basit(a, TIP_METIN);
        p[1] = tip_olustur_basit(a, TIP_TAM32);
        EKLE_BUILTIN("otp_anahtar_uret", 16, p, 2,
                     tip_olustur_basit(a, TIP_TAM32));
    }

    /* otp_xor_uygula(msg, anahtar, cikti) -> tam32 */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *) * 3);
        p[0] = tip_olustur_basit(a, TIP_METIN);
        p[1] = tip_olustur_basit(a, TIP_METIN);
        p[2] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("otp_xor_uygula", 14, p, 3,
                     tip_olustur_basit(a, TIP_TAM32));
    }

    /* === Adim 6: Dizi capacity API === */
    /* dizi_kapasite(d: Dizi<T>) -> tam32 — generic, T atlanir */
    /* dizi_kapasite_ayarla(d: Dizi<T>, yeni: tam32) -> bos */
    /* Bu built-inler intrinsic gibi (tip kontrol DUGUM_CAGRI'de
     * ozel handler), sadece sembol tablosu lookup icin kayit */

    #undef EKLE_BUILTIN
    tk->dosya_adi = dosya_adi;
    tk->kaynak = kaynak;
    tk->scope_seviyesi = 0;
    tk->lambda_govdesi_icinde = 0;
    tk->lambda_lineer_yakalama = 0;
    tk->lambda_baslangic_scope = NULL;
}

/* === Linear Types Spec V1: yardimci fonksiyonlar === */

/* Read-only check — eger ifade lineer baglama ve zaten tuketildiyse hata.
 * Tuketim sayisini ARTIRMAZ (CP-IO semantik: y tuketilmez ama revoked olmamali).
 * Capability spec'inin "dosya_oku_yetkili / dosya_yaz_yetkili" gibi tuketim-
 * yapmayan ops icin gereklidir. */
static void lineer_kullanim_kontrolu(TipKontrol *tk, const Dugum *d) {
    if (!d || d->tip != DUGUM_TANIMLAYICI) return;
    Sembol *s = sembol_bul_yazilabilir(tk->scope,
        d->veri.tanimlayici.metin, d->veri.tanimlayici.uzunluk);
    if (!s || !s->tip || !tip_lineer_mi(s->tip)) return;
    if (s->lineer_tuketildi >= 1) {
        if (s->tip->kategori == TIP_YETKI) {
            tip_hata(tk, d, "CP005",
                "yetki<R> iptal sonrasi kullanim (tuketildi, sonra erisim)");
        } else {
            tip_hata(tk, d, "L002",
                "lineer baglama tuketildi, sonra erisim");
        }
    }
    /* Artirma yok — bu helper sadece kontrol */
}

/* Eger ifade DUGUM_TANIMLAYICI ise ve sembolu lineer ise -> tuket.
 * Cift tuketim L002 hatasi (tekkez) veya CP005 (yetki). */
static void lineer_tuket_eger_baglamaysa(TipKontrol *tk, const Dugum *d) {
    if (!d || d->tip != DUGUM_TANIMLAYICI) return;
    Sembol *s = sembol_bul_yazilabilir(tk->scope,
        d->veri.tanimlayici.metin, d->veri.tanimlayici.uzunluk);
    if (!s || !s->tip || !tip_lineer_mi(s->tip)) return;
    if (s->lineer_tuketildi >= 1) {
        if (s->tip->kategori == TIP_YETKI) {
            tip_hata(tk, d, "CP005",
                "yetki<R> iki kez tuketildi (move sonrasi erisim)");
        } else {
            tip_hata(tk, d, "L002",
                "lineer baglama iki kez tuketildi (move sonrasi erisim)");
        }
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

    /* Parent scope'ta lineer baglama mi? (tekkez veya yetki) */
    const Sembol *parent = sembol_bul(tk->lambda_baslangic_scope,
        d->veri.tanimlayici.metin, d->veri.tanimlayici.uzunluk);
    if (!parent || !parent->tip || !tip_lineer_mi(parent->tip)) return;

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
        /* Linear takip: tekkez<T> ve yetki<R> ikisi de */
        if (!sem->tip || !tip_lineer_mi(sem->tip)) continue;
        if (sem->lineer_tuketildi == 0) {
            tk->hata_sayisi++;
            const char *kod = (sem->tip->kategori == TIP_YETKI)
                              ? "CP005" : "L001";
            const char *mesaj = (sem->tip->kategori == TIP_YETKI)
                ? "yetki<R> scope sonunda tuketilmedi"
                : "lineer baglama scope sonunda tuketilmedi";
            const char *ipucu = (sem->tip->kategori == TIP_YETKI)
                ? "geri_al(y) veya I/O cagrisi ile tuketin"
                : "kullan(...) veya imha(...) ile tuketin";
            hata_raporla(tk->dosya_adi, tk->kaynak,
                         sem->satir, sem->sutun, kod, mesaj, ipucu);
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

/* === Madde D: Generic call inference helpers (paralel session, kullanilmaz) ===
 *
 * Bu bolumdeki GenericBaglama + linked list yardimcilari paralel commit
 * tarafindan eklendi. Bu oturumda alternatif GenBaglamalar (array) +
 * gen_bagla/gen_unify/gen_substitue daha asagida tanimli ve cagri site'da
 * o kullaniliyor. Asagidaki yardimcilar kullanilmiyor — tutuldu zira ileride
 * bound check icin gerekli olabilir (param_tip_generic_iceriyor_mu vb.). */

#if 0  /* kullanilmiyor — kullanilirsa #if 1 yap */

/* Generic param adi -> concrete tip baglamasi (linked list) */
typedef struct GenericBaglama {
    const char *ad;
    int ad_uzunluk;
    TipBilgisi *tip;
    struct GenericBaglama *sonraki;
} GenericBaglama;

static TipBilgisi *baglama_bul(GenericBaglama *b,
                                const char *ad, int ad_uz) {
    for (; b; b = b->sonraki) {
        if (b->ad_uzunluk == ad_uz &&
            memcmp(b->ad, ad, (size_t)ad_uz) == 0) {
            return b->tip;
        }
    }
    return NULL;
}

static void baglama_ekle(TipKontrol *tk, GenericBaglama **bas,
                          const char *ad, int ad_uz,
                          TipBilgisi *tip) {
    if (!tip || tip->kategori == TIP_GENERIC_PARAM) return;
    /* Varsa override etme — ilk binding kalir (zaten anchor) */
    if (baglama_bul(*bas, ad, ad_uz)) return;
    GenericBaglama *b = (GenericBaglama *)arena_ayir_sifir(tk->arena,
                                                            sizeof(GenericBaglama));
    if (!b) return;
    b->ad = ad;
    b->ad_uzunluk = ad_uz;
    b->tip = tip;
    b->sonraki = *bas;
    *bas = b;
}

static int param_tip_generic_iceriyor_mu(const TipBilgisi *t) {
    if (!t) return 0;
    if (t->kategori == TIP_GENERIC_PARAM) return 1;
    switch (t->kategori) {
        case TIP_REFERANS: return param_tip_generic_iceriyor_mu(t->veri.referans.hedef);
        case TIP_POINTER:  return param_tip_generic_iceriyor_mu(t->veri.pointer.hedef);
        case TIP_DIZI:     return param_tip_generic_iceriyor_mu(t->veri.dizi.eleman);
        case TIP_SECIMLIK: return param_tip_generic_iceriyor_mu(t->veri.secimlik.ic);
        case TIP_SONUC:
            return param_tip_generic_iceriyor_mu(t->veri.sonuc.deger) ||
                   param_tip_generic_iceriyor_mu(t->veri.sonuc.hata);
        case TIP_TEKKEZ:   return param_tip_generic_iceriyor_mu(t->veri.tekkez.ic);
        case TIP_VEKTOR:   return param_tip_generic_iceriyor_mu(t->veri.vektor.eleman);
        case TIP_ISLEV: {
            for (int i = 0; i < t->veri.islev.param_sayi; i++) {
                if (param_tip_generic_iceriyor_mu(t->veri.islev.parametreler[i]))
                    return 1;
            }
            return param_tip_generic_iceriyor_mu(t->veri.islev.donus);
        }
        case TIP_YAPI:
            for (int i = 0; i < t->veri.yapi.tip_arg_sayi; i++) {
                if (param_tip_generic_iceriyor_mu(t->veri.yapi.tip_arg[i]))
                    return 1;
            }
            return 0;
        default: return 0;
    }
}

/* tip_subst_baglamalar: t icindeki GENERIC_PARAM'lari baglamalardan al */
static TipBilgisi *tip_subst_baglamalar(TipKontrol *tk, TipBilgisi *t,
                                         GenericBaglama *b) {
    if (!t || !b) return t;
    switch (t->kategori) {
        case TIP_GENERIC_PARAM: {
            TipBilgisi *bound = baglama_bul(b,
                t->veri.generic_param.ad,
                t->veri.generic_param.ad_uzunluk);
            return bound ? bound : t;
        }
        case TIP_REFERANS: {
            TipBilgisi *nh = tip_subst_baglamalar(tk,
                t->veri.referans.hedef, b);
            if (nh == t->veri.referans.hedef) return t;
            return tip_olustur_referans(tk->arena, nh,
                t->veri.referans.degisken_mi);
        }
        case TIP_POINTER: {
            TipBilgisi *nh = tip_subst_baglamalar(tk,
                t->veri.pointer.hedef, b);
            if (nh == t->veri.pointer.hedef) return t;
            return tip_olustur_pointer(tk->arena, nh);
        }
        case TIP_DIZI: {
            TipBilgisi *ne = tip_subst_baglamalar(tk,
                t->veri.dizi.eleman, b);
            if (ne == t->veri.dizi.eleman) return t;
            return tip_olustur_dizi(tk->arena, ne);
        }
        case TIP_SECIMLIK: {
            TipBilgisi *ni = tip_subst_baglamalar(tk,
                t->veri.secimlik.ic, b);
            if (ni == t->veri.secimlik.ic) return t;
            return tip_olustur_secimlik(tk->arena, ni);
        }
        case TIP_SONUC: {
            TipBilgisi *nd = tip_subst_baglamalar(tk,
                t->veri.sonuc.deger, b);
            TipBilgisi *nh = tip_subst_baglamalar(tk,
                t->veri.sonuc.hata, b);
            if (nd == t->veri.sonuc.deger && nh == t->veri.sonuc.hata) return t;
            return tip_olustur_sonuc(tk->arena, nd, nh);
        }
        case TIP_TEKKEZ: {
            TipBilgisi *ni = tip_subst_baglamalar(tk,
                t->veri.tekkez.ic, b);
            if (ni == t->veri.tekkez.ic) return t;
            return tip_olustur_tekkez(tk->arena, ni);
        }
        case TIP_VEKTOR: {
            TipBilgisi *ne = tip_subst_baglamalar(tk,
                t->veri.vektor.eleman, b);
            if (ne == t->veri.vektor.eleman) return t;
            return tip_olustur_vektor(tk->arena, ne, t->veri.vektor.lane_sayi);
        }
        case TIP_ISLEV: {
            int n = t->veri.islev.param_sayi;
            int degisen = 0;
            TipBilgisi **np = NULL;
            if (n > 0) {
                np = (TipBilgisi **)arena_ayir(tk->arena,
                    sizeof(TipBilgisi *) * (size_t)n);
                for (int i = 0; i < n; i++) {
                    np[i] = tip_subst_baglamalar(tk,
                        t->veri.islev.parametreler[i], b);
                    if (np[i] != t->veri.islev.parametreler[i]) degisen = 1;
                }
            }
            TipBilgisi *nd = tip_subst_baglamalar(tk, t->veri.islev.donus, b);
            if (!degisen && nd == t->veri.islev.donus) return t;
            return tip_olustur_islev(tk->arena, np, n, nd);
        }
        default: return t;
    }
}

/* tip_unify: param ve arg'i paralel walk, generic baglamalari topla */
static void tip_unify(TipKontrol *tk,
                       TipBilgisi *param, TipBilgisi *arg,
                       GenericBaglama **bas) {
    if (!param || !arg) return;
    if (param->kategori == TIP_GENERIC_PARAM) {
        baglama_ekle(tk, bas,
            param->veri.generic_param.ad,
            param->veri.generic_param.ad_uzunluk, arg);
        return;
    }
    if (param->kategori != arg->kategori) return;
    switch (param->kategori) {
        case TIP_REFERANS:
            tip_unify(tk, param->veri.referans.hedef,
                          arg->veri.referans.hedef, bas);
            break;
        case TIP_POINTER:
            tip_unify(tk, param->veri.pointer.hedef,
                          arg->veri.pointer.hedef, bas);
            break;
        case TIP_DIZI:
            tip_unify(tk, param->veri.dizi.eleman,
                          arg->veri.dizi.eleman, bas);
            break;
        case TIP_SECIMLIK:
            tip_unify(tk, param->veri.secimlik.ic,
                          arg->veri.secimlik.ic, bas);
            break;
        case TIP_SONUC:
            tip_unify(tk, param->veri.sonuc.deger,
                          arg->veri.sonuc.deger, bas);
            tip_unify(tk, param->veri.sonuc.hata,
                          arg->veri.sonuc.hata, bas);
            break;
        case TIP_TEKKEZ:
            tip_unify(tk, param->veri.tekkez.ic,
                          arg->veri.tekkez.ic, bas);
            break;
        case TIP_VEKTOR:
            if (param->veri.vektor.lane_sayi != arg->veri.vektor.lane_sayi) break;
            tip_unify(tk, param->veri.vektor.eleman,
                          arg->veri.vektor.eleman, bas);
            break;
        case TIP_ISLEV: {
            if (param->veri.islev.param_sayi != arg->veri.islev.param_sayi) break;
            for (int i = 0; i < param->veri.islev.param_sayi; i++) {
                tip_unify(tk, param->veri.islev.parametreler[i],
                              arg->veri.islev.parametreler[i], bas);
            }
            tip_unify(tk, param->veri.islev.donus,
                          arg->veri.islev.donus, bas);
            break;
        }
        case TIP_YAPI: {
            if (param->veri.yapi.tip_arg_sayi != arg->veri.yapi.tip_arg_sayi) break;
            for (int i = 0; i < param->veri.yapi.tip_arg_sayi; i++) {
                tip_unify(tk, param->veri.yapi.tip_arg[i],
                              arg->veri.yapi.tip_arg[i], bas);
            }
            break;
        }
        default: break;
    }
}

#endif  /* kullanilmiyor — paralel session helperlari */

/* Forward (ADIM 15.5: bound check) */
static const char *tip_dugumu_kok_adi(const Dugum *t, int *out_uz);

static TipBilgisi *t_basit(TipKontrol *tk, TipKategorisi k) {
    return tip_olustur_basit(tk->arena, k);
}

/* === Madde D: Generic callback tip cikarsamasi (multi-param + compound) ===
 *
 * Bir cagri site'da `hedef<T,U,V>(...)` icin:
 *   1. param_tip <-> arg_tip unification ile her generic param T,U,V'yi
 *      argumanlarin somut tiplerinden cikar (compound tipler dahil:
 *      Dizi<T>, islev(T)->U, secimlik<T>, sonuc<T,E>, &T, *T)
 *   2. Donus tipi compound olabilir; her generic param tekrar substitue edilir
 *
 * Mevcut tek-T inference yerine name->concrete map kullanir. */

typedef struct GenBaglama {
    const char *ad;
    int ad_uz;
    const TipBilgisi *concrete;
} GenBaglama;

typedef struct GenBaglamalar {
    GenBaglama girisler[16];   /* En fazla 16 generic param — pratikte yeterli */
    int sayi;
} GenBaglamalar;

static void gen_bagla(GenBaglamalar *gb, const char *ad, int ad_uz,
                       const TipBilgisi *concrete) {
    if (!gb || !ad || gb->sayi >= 16) return;
    /* Var olan binding mi? — ilk gorulen kazanir */
    for (int i = 0; i < gb->sayi; i++) {
        if (gb->girisler[i].ad_uz == ad_uz &&
            memcmp(gb->girisler[i].ad, ad, (size_t)ad_uz) == 0) {
            return;  /* zaten bagli */
        }
    }
    gb->girisler[gb->sayi].ad = ad;
    gb->girisler[gb->sayi].ad_uz = ad_uz;
    gb->girisler[gb->sayi].concrete = concrete;
    gb->sayi++;
}

static const TipBilgisi *gen_bul(const GenBaglamalar *gb,
                                  const char *ad, int ad_uz) {
    if (!gb) return NULL;
    for (int i = 0; i < gb->sayi; i++) {
        if (gb->girisler[i].ad_uz == ad_uz &&
            memcmp(gb->girisler[i].ad, ad, (size_t)ad_uz) == 0) {
            return gb->girisler[i].concrete;
        }
    }
    return NULL;
}

/* param_tip ile arg_tip'i unify et — TIP_GENERIC_PARAM gorulen yerlere
 * arg_tip'ten karsilik binding ekle. Compound tiplere recursive. */
static void gen_unify(GenBaglamalar *gb, const TipBilgisi *param,
                       const TipBilgisi *arg) {
    if (!param || !arg) return;
    if (param->kategori == TIP_GENERIC_PARAM) {
        gen_bagla(gb, param->veri.generic_param.ad,
                  param->veri.generic_param.ad_uzunluk, arg);
        return;
    }
    /* Arg tarafi da generic param ise (govdede T->T gibi) — skip */
    if (arg->kategori == TIP_GENERIC_PARAM) return;
    /* Kategoriler farkliysa unify imkansiz — skip (hata zaten tip_esit'te) */
    if (param->kategori != arg->kategori) return;

    switch (param->kategori) {
        case TIP_REFERANS:
            gen_unify(gb, param->veri.referans.hedef, arg->veri.referans.hedef);
            break;
        case TIP_POINTER:
            gen_unify(gb, param->veri.pointer.hedef, arg->veri.pointer.hedef);
            break;
        case TIP_DIZI:
            gen_unify(gb, param->veri.dizi.eleman, arg->veri.dizi.eleman);
            break;
        case TIP_SECIMLIK:
            gen_unify(gb, param->veri.secimlik.ic, arg->veri.secimlik.ic);
            break;
        case TIP_SONUC:
            gen_unify(gb, param->veri.sonuc.deger, arg->veri.sonuc.deger);
            gen_unify(gb, param->veri.sonuc.hata,  arg->veri.sonuc.hata);
            break;
        case TIP_ISLEV: {
            int n = param->veri.islev.param_sayi;
            if (n == arg->veri.islev.param_sayi) {
                for (int i = 0; i < n; i++) {
                    gen_unify(gb,
                        param->veri.islev.parametreler[i],
                        arg->veri.islev.parametreler[i]);
                }
            }
            gen_unify(gb, param->veri.islev.donus, arg->veri.islev.donus);
            break;
        }
        default:
            break;
    }
}

/* Compound tip icinde TIP_GENERIC_PARAM'leri concrete tiplerle degistir.
 * gb NULL veya generic param eslemiyorsa orjinal tipi doner. */
static TipBilgisi *gen_substitue(TipKontrol *tk, const TipBilgisi *t,
                                   const GenBaglamalar *gb) {
    if (!t || !gb || gb->sayi == 0) return (TipBilgisi *)t;
    if (t->kategori == TIP_GENERIC_PARAM) {
        const TipBilgisi *c = gen_bul(gb,
            t->veri.generic_param.ad, t->veri.generic_param.ad_uzunluk);
        return c ? (TipBilgisi *)c : (TipBilgisi *)t;
    }
    switch (t->kategori) {
        case TIP_REFERANS: {
            TipBilgisi *nh = gen_substitue(tk, t->veri.referans.hedef, gb);
            if (nh == t->veri.referans.hedef) return (TipBilgisi *)t;
            return tip_olustur_referans(tk->arena, nh,
                                         t->veri.referans.degisken_mi);
        }
        case TIP_POINTER: {
            TipBilgisi *nh = gen_substitue(tk, t->veri.pointer.hedef, gb);
            if (nh == t->veri.pointer.hedef) return (TipBilgisi *)t;
            return tip_olustur_pointer(tk->arena, nh);
        }
        case TIP_DIZI: {
            TipBilgisi *ne = gen_substitue(tk, t->veri.dizi.eleman, gb);
            if (ne == t->veri.dizi.eleman) return (TipBilgisi *)t;
            return tip_olustur_dizi(tk->arena, ne);
        }
        case TIP_SECIMLIK: {
            TipBilgisi *ni = gen_substitue(tk, t->veri.secimlik.ic, gb);
            if (ni == t->veri.secimlik.ic) return (TipBilgisi *)t;
            return tip_olustur_secimlik(tk->arena, ni);
        }
        case TIP_SONUC: {
            TipBilgisi *nd = gen_substitue(tk, t->veri.sonuc.deger, gb);
            TipBilgisi *nh = gen_substitue(tk, t->veri.sonuc.hata, gb);
            if (nd == t->veri.sonuc.deger && nh == t->veri.sonuc.hata)
                return (TipBilgisi *)t;
            return tip_olustur_sonuc(tk->arena, nd, nh);
        }
        case TIP_ISLEV: {
            int n = t->veri.islev.param_sayi;
            TipBilgisi **yeni_p = NULL;
            int degisti = 0;
            if (n > 0) {
                yeni_p = (TipBilgisi **)arena_ayir(tk->arena,
                    sizeof(TipBilgisi *) * (size_t)n);
                for (int i = 0; i < n; i++) {
                    yeni_p[i] = gen_substitue(tk,
                        t->veri.islev.parametreler[i], gb);
                    if (yeni_p[i] != t->veri.islev.parametreler[i]) degisti = 1;
                }
            }
            TipBilgisi *nd = gen_substitue(tk, t->veri.islev.donus, gb);
            if (!degisti && nd == t->veri.islev.donus) return (TipBilgisi *)t;
            return tip_olustur_islev(tk->arena, yeni_p, n, nd);
        }
        default:
            return (TipBilgisi *)t;
    }
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

        case DUGUM_TIP_SABITSURE: {
            TipBilgisi *ic = ast_tip_to_bilgi(tk,
                tip_d->veri.tip_sabitsure.ic_tip);
            if (ic->kategori == TIP_HATA) return t_hata(tk);
            /* CT006: sarılan tip sabitsüre-yetenekli olmalı */
            if (!tip_sabitsure_yetenekli_mi(ic)) {
                tip_hata(tk, tip_d, "CT006",
                    "sabitsure<...> sarilan tip constant-time yetenekli degil "
                    "(kesirli/metin/yapi/secimlik/nesting yasak)");
                return t_hata(tk);
            }
            return tip_olustur_sabitsure(tk->arena, ic);
        }

        case DUGUM_TIP_YETKI: {
            /* Capability Spec V1: yetki<R>, R = kaynak tipi.
             * V1: R DUGUM_TIP_BASIT veya DUGUM_TIP_KULLANICI olmalı,
             * adı bilinen kaynak setinden (Dosya/Soket/Bellek/Donanim/OTP_Anahtar).
             * Bilinmeyen kaynak: CP004 (CAPABILITY_TYPE_MISMATCH/INVALID). */
            const Dugum *r = tip_d->veri.tip_yetki.kaynak_tipi;
            if (!r) {
                tip_hata(tk, tip_d, "CP004",
                    "yetki<R> icin kaynak tipi gerekli");
                return t_hata(tk);
            }
            /* Kaynak adi bul */
            const char *ad = NULL;
            int ad_uz = 0;
            if (r->tip == DUGUM_TIP_BASIT) {
                ad = r->veri.tip_basit.ad;
                ad_uz = r->veri.tip_basit.ad_uzunluk;
            } else if (r->tip == DUGUM_TIP_KULLANICI &&
                       r->veri.tip_kullanici.yol &&
                       r->veri.tip_kullanici.yol->tip == DUGUM_TANIMLAYICI) {
                ad = r->veri.tip_kullanici.yol->veri.tanimlayici.metin;
                ad_uz = r->veri.tip_kullanici.yol->veri.tanimlayici.uzunluk;
            }
            int ok = 0;
            if (ad && ad_uz > 0) {
                /* OTP_Anahtar 11 byte, Dosya 5, Soket 5, Bellek 6, Donanim 7 */
                if ((ad_uz == 5 && memcmp(ad, "Dosya", 5) == 0) ||
                    (ad_uz == 5 && memcmp(ad, "Soket", 5) == 0) ||
                    (ad_uz == 6 && memcmp(ad, "Bellek", 6) == 0) ||
                    (ad_uz == 7 && memcmp(ad, "Donanim", 7) == 0) ||
                    (ad_uz == 11 && memcmp(ad, "OTP_Anahtar", 11) == 0)) {
                    ok = 1;
                }
            }
            if (!ok) {
                tip_hata(tk, tip_d, "CP004",
                    "yetki<R>: bilinmeyen kaynak tipi "
                    "(Dosya/Soket/Bellek/Donanim/OTP_Anahtar bekleniyor)");
                return t_hata(tk);
            }
            /* Kaynak TIP_YAPI olarak temsil edilir (ad bazli nominal eslesme).
             * ast_tip_to_bilgi'ye gitmeyiz cunku kaynak tipler symbol table'da
             * tanimli degil; built-in nominal isimler. */
            char *ad_kopya = (char *)arena_ayir(tk->arena, (size_t)ad_uz + 1);
            if (ad_kopya) {
                memcpy(ad_kopya, ad, (size_t)ad_uz);
                ad_kopya[ad_uz] = '\0';
            }
            TipBilgisi *kaynak = tip_olustur_yapi(tk->arena,
                                                 ad_kopya, ad_uz,
                                                 NULL, 0);
            return tip_olustur_yetki(tk->arena, kaynak);
        }

        case DUGUM_TIP_VEKTOR: {
            /* SIMD Spec V1: vektör<T, N>
             *   V001: T vektör-yetenekli olmalı (tam/dtam/kesirli/mantıksal)
             *   V002: N {2,4,8,16,32,64} setinde olmalı */
            TipBilgisi *eleman = ast_tip_to_bilgi(tk,
                tip_d->veri.tip_vektor.eleman_tip);
            int lane = tip_d->veri.tip_vektor.lane_sayi;
            if (eleman->kategori == TIP_HATA) return t_hata(tk);
            if (!tip_vektor_eleman_yetenekli_mi(eleman)) {
                tip_hata(tk, tip_d, "V001",
                    "vektor<T, N> tipinde T vektor-yetenekli skaler olmali "
                    "(tam/dtam/kesirli/mantiksal)");
                return t_hata(tk);
            }
            if (!tip_vektor_lane_gecerli_mi(lane)) {
                tip_hata(tk, tip_d, "V002",
                    "vektor<T, N> tipinde N {2,4,8,16,32,64} setinde olmali");
                return t_hata(tk);
            }
            return tip_olustur_vektor(tk->arena, eleman, lane);
        }

        case DUGUM_TIP_GOREV: {
            /* Concurrency / DRF V1: görev<T> — thread handle (linear) */
            TipBilgisi *ic = ast_tip_to_bilgi(tk,
                tip_d->veri.tip_gorev.ic_tip);
            if (ic->kategori == TIP_HATA) return t_hata(tk);
            return tip_olustur_gorev(tk->arena, ic);
        }

        case DUGUM_TIP_KANAL: {
            /* Concurrency / DRF V1: kanal<T> — channel endpoint (linear) */
            TipBilgisi *ic = ast_tip_to_bilgi(tk,
                tip_d->veri.tip_kanal.ic_tip);
            if (ic->kategori == TIP_HATA) return t_hata(tk);
            return tip_olustur_kanal(tk->arena, ic);
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

/* === Sabitsüre Spec V1 yardımcıları === */

/* sabitsüre<T> sarmalayıcısını sok, T döner (kaynak sabitsüre değilse aynısı). */
static TipBilgisi *tip_ic_cek(TipBilgisi *t) {
    if (t && t->kategori == TIP_SABITSURE) return t->veri.sabitsure.ic;
    return t;
}

/* İkili op için: sol veya sağ sabitsüre ise sonuç da sabitsüre yapılır. */
static TipBilgisi *taint_yay(TipKontrol *tk, TipBilgisi *base,
                              TipBilgisi *sol, TipBilgisi *sag) {
    if (tip_sabitsure_mi(sol) || tip_sabitsure_mi(sag)) {
        if (tip_sabitsure_mi(base)) return base;
        return tip_olustur_sabitsure(tk->arena, base);
    }
    return base;
}

/* CT003 SABITSURE_LEAK helper:
 * 'kaynak' (genelde değer ifadesinin tipi) sabitsüre<T> ve 'beklenen' normal T
 * ise leak hatası. d düğümü ifade için. Hata raporlanırsa 1 döner. */
static int ct003_leak_kontrol(TipKontrol *tk, const Dugum *d,
                               const TipBilgisi *kaynak,
                               const TipBilgisi *beklenen) {
    if (!kaynak || !beklenen) return 0;
    if (tip_sabitsure_mi(kaynak) && !tip_sabitsure_mi(beklenen)) {
        tip_hata(tk, d, "CT003",
            "sabitsure tipi normal tipe implicit donusturulemez "
            "(ifsa(...) zorunlu)");
        return 1;
    }
    return 0;
}

/* Yardimci: ikili sayisal op (sol == sag, ikisi de sayisal) */
/* SIMD Spec V1: ikili operatör vektör üzerinde mi?
 * Sol veya sağ vektör ise vektör semantikleriyle çöz. */
static TipBilgisi *kontrol_ikili_vektor(TipKontrol *tk, const Dugum *d,
                                         TipBilgisi *sol, TipBilgisi *sag) {
    int sol_v = tip_vektor_mu(sol);
    int sag_v = tip_vektor_mu(sag);
    /* V004: skaler + vektör (V1'de yasak — explicit vektör_doldur gerek) */
    if (sol_v != sag_v) {
        tip_hata(tk, d, "V004",
            "vektor ile skaler karma operasyon V1'de yasak — "
            "vektor_doldur(s) ile explicit broadcast kullan");
        return t_hata(tk);
    }
    /* V003: lane sayıları eşit olmalı */
    if (sol->veri.vektor.lane_sayi != sag->veri.vektor.lane_sayi) {
        tip_hata(tk, d, "V003",
            "vektor operandlarinin lane sayilari (N) esit olmali");
        return t_hata(tk);
    }
    /* V003: element tipleri eşit olmalı */
    if (!tip_esit(sol->veri.vektor.eleman, sag->veri.vektor.eleman)) {
        tip_hata(tk, d, "V003",
            "vektor operandlarinin element tipleri (T) esit olmali");
        return t_hata(tk);
    }
    /* V005: kesirli vektörde % yasak */
    Operator op = d->veri.ikili.op;
    int eleman_kesirli =
        sol->veri.vektor.eleman->kategori == TIP_KESIRLI32 ||
        sol->veri.vektor.eleman->kategori == TIP_KESIRLI64;
    if (op == OP_MOD && eleman_kesirli) {
        tip_hata(tk, d, "V005",
            "kesirli vektor uzerinde % yasak (FP modulo undefined)");
        return t_hata(tk);
    }
    /* Eşit lane + eşit element → aynı tip dönsün */
    return sol;
}

static TipBilgisi *kontrol_ikili_sayisal(TipKontrol *tk, const Dugum *d,
                                         TipBilgisi *sol, TipBilgisi *sag) {
    /* SIMD Spec V1: önce vektör hattı */
    if (tip_vektor_mu(sol) || tip_vektor_mu(sag)) {
        return kontrol_ikili_vektor(tk, d, sol, sag);
    }
    if (!tip_sayisal_mi(sol)) {
        tip_hata(tk, d, "T003", "ikili operatorun sol tarafi sayisal degil");
        return t_hata(tk);
    }
    if (!tip_sayisal_mi(sag)) {
        tip_hata(tk, d, "T003", "ikili operatorun sag tarafi sayisal degil");
        return t_hata(tk);
    }
    /* Sabitsüre Spec V1 CT004: sabitsüre üzerinde / veya % YASAK
     * (x86 idiv/div, ARM udiv/sdiv variable-time). */
    Operator op = d->veri.ikili.op;
    if ((op == OP_BOLU || op == OP_MOD) &&
        (tip_sabitsure_mi(sol) || tip_sabitsure_mi(sag))) {
        tip_hata(tk, d, "CT004",
            "sabitsure tipi uzerinde / veya % yasak (variable-time div)");
        return t_hata(tk);
    }
    /* İç tipler eşit mi? sabitsüre<T> + T → her ikisinin iç T'si aynı olmalı.
     * Bu sayede sabitsüre<tam32> + tam32 → sabitsüre<tam32> (taint yayılım). */
    TipBilgisi *sol_ic = tip_ic_cek(sol);
    TipBilgisi *sag_ic = tip_ic_cek(sag);
    if (!tip_esit(sol_ic, sag_ic)) {
        tip_hata(tk, d, "T001", "ikili operator iki tarafi ayni tip olmali");
        return t_hata(tk);
    }
    return taint_yay(tk, sol_ic, sol, sag);
}

/* Yardimci: ikili mantiksal (sol+sag mantiksal) */
static TipBilgisi *kontrol_ikili_mantiksal(TipKontrol *tk, const Dugum *d,
                                           TipBilgisi *sol, TipBilgisi *sag) {
    if (!tip_mantiksal_mi(sol) || !tip_mantiksal_mi(sag)) {
        tip_hata(tk, d, "T004", "mantiksal op iki tarafi mantiksal olmali");
        return t_hata(tk);
    }
    TipBilgisi *base = t_basit(tk, TIP_MANTIKSAL);
    return taint_yay(tk, base, sol, sag);
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

        /* === Madde E (v2): explicit tip donusturme (x olarak T) — 4 garanti ===
         *   E001: x olarak tekkez<T> yasak (tekkez hedef)
         *   E002: metin/yapi/dizi gibi sayisal-disi kaynak yasak
         *   E003: tekkez<T> olarak X yasak (linear escape — kaynak tekkez)
         *   E004: kayıp prezisyon (tam64 olarak tam8, kesirli64 olarak kesirli32)
         */
        case DUGUM_TIP_DONUSTUR: {
            TipBilgisi *kt = tip_belirle(tk, d->veri.tip_donustur.kaynak);
            if (kt->kategori == TIP_HATA) return t_hata(tk);
            TipBilgisi *ht = ast_tip_to_bilgi(tk, d->veri.tip_donustur.hedef_tip);
            if (!ht || ht->kategori == TIP_HATA) return t_hata(tk);

            /* E001: hedef tekkez<T> yasak (Linear Types: olarak ile yaratamazsin) */
            if (ht->kategori == TIP_TEKKEZ) {
                tip_hata(tk, d, "E001",
                    "olarak ile tekkez<T> hedeflenemez (Linear Types kuralı)");
                return t_hata(tk);
            }

            /* E003: kaynak tekkez<T> yasak (linear escape — tekkez'i extract
             * etmek icin kullan() gerek, olarak ile escape yapilamaz) */
            if (kt->kategori == TIP_TEKKEZ) {
                tip_hata(tk, d, "E003",
                    "olarak ile tekkez<T> kaynaktan extract edilemez "
                    "(kullan(...) gerekir)");
                return t_hata(tk);
            }

            int kaynak_sayisal = tip_sayisal_mi(kt);
            int hedef_sayisal = tip_sayisal_mi(ht);
            if (!kaynak_sayisal || !hedef_sayisal) {
                /* Karakter <-> tam* da izinli */
                int char_to_int = (kt->kategori == TIP_KARAKTER &&
                                   tip_tamsayi_mi(ht));
                int int_to_char = (tip_tamsayi_mi(kt) &&
                                   ht->kategori == TIP_KARAKTER);
                if (!char_to_int && !int_to_char) {
                    /* E002: metin/dizi/yapi gibi sayisal disi kaynak/hedef */
                    tip_hata(tk, d, "E002",
                        "olarak: kaynak ve hedef sayisal/karakter olmali");
                    return t_hata(tk);
                }
            }

            /* E004: Kayip prezisyon — tam64/dtam64 -> tam8/16/32 ya da
             * kesirli64 -> kesirli32 explicit isaretsiz dusurme.
             * Bu cesit cast yapilmasi gerekiyorsa, kullanici niyet
             * ifade etmeli (& mask, mod, vs.) — implicit aritmetigi onler. */
            int kw_kaynak = 0, kw_hedef = 0;
            switch (kt->kategori) {
                case TIP_TAM8: case TIP_DTAM8:    kw_kaynak = 8; break;
                case TIP_TAM16: case TIP_DTAM16:  kw_kaynak = 16; break;
                case TIP_TAM32: case TIP_DTAM32:  kw_kaynak = 32; break;
                case TIP_TAM64: case TIP_DTAM64:  kw_kaynak = 64; break;
                case TIP_KESIRLI32:               kw_kaynak = 320; break;
                case TIP_KESIRLI64:               kw_kaynak = 640; break;
                case TIP_KARAKTER:                kw_kaynak = 32; break;
                default: break;
            }
            switch (ht->kategori) {
                case TIP_TAM8: case TIP_DTAM8:    kw_hedef = 8; break;
                case TIP_TAM16: case TIP_DTAM16:  kw_hedef = 16; break;
                case TIP_TAM32: case TIP_DTAM32:  kw_hedef = 32; break;
                case TIP_TAM64: case TIP_DTAM64:  kw_hedef = 64; break;
                case TIP_KESIRLI32:               kw_hedef = 320; break;
                case TIP_KESIRLI64:               kw_hedef = 640; break;
                case TIP_KARAKTER:                kw_hedef = 32; break;
                default: break;
            }
            /* E004: kayip prezisyon. Pratik kural: tam64 -> tam8/16 yasak,
             * kesirli64 -> kesirli32 yasak. tam64 -> tam32 izinli (32-bit
             * native word). Bu, "ortakli olunca cast" semantigini korur
             * ama acik narrowing'i blok eder. */
            int kaynak_int = kw_kaynak > 0 && kw_kaynak <= 64;
            int hedef_int = kw_hedef > 0 && kw_hedef <= 64;
            int kaynak_float = kw_kaynak >= 320;
            int hedef_float = kw_hedef >= 320;
            /* Sadece >32-bit kaynak -> <32-bit hedef yasak (significant lost) */
            if (kaynak_int && hedef_int && kw_kaynak >= 64 && kw_hedef < 32) {
                tip_hata(tk, d, "E004",
                    "olarak: kayip prezisyon (tam64 -> tam8/tam16)");
                return t_hata(tk);
            }
            if (kaynak_float && hedef_float && kw_kaynak > kw_hedef) {
                tip_hata(tk, d, "E004",
                    "olarak: kayip prezisyon (kesirli64 -> kesirli32)");
                return t_hata(tk);
            }
            return ht;
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

                case OP_ESIT:  case OP_ESIT_DEGIL: {
                    /* İç tipler eşit olmalı (sabitsüre<T> == T iç olarak eşit) */
                    TipBilgisi *sol_ic = tip_ic_cek(sol);
                    TipBilgisi *sag_ic = tip_ic_cek(sag);
                    if (!tip_esit(sol_ic, sag_ic)) {
                        tip_hata(tk, d, "T001",
                                 "esitlik karsilastirma ayni tip olmali");
                        return t_hata(tk);
                    }
                    /* Sabitsüre Spec V1 CT-CMP: sabitsüre operand → sonuç
                     * sabitsüre<mantıksal> (eşitlik bilgisi de gizli). */
                    TipBilgisi *base = t_basit(tk, TIP_MANTIKSAL);
                    return taint_yay(tk, base, sol, sag);
                }

                case OP_KUCUK: case OP_BUYUK:
                case OP_KUCUK_ESIT: case OP_BUYUK_ESIT: {
                    if (!tip_sayisal_mi(sol) || !tip_sayisal_mi(sag)) {
                        tip_hata(tk, d, "T003",
                                 "karsilastirma sayisal tip ister");
                        return t_hata(tk);
                    }
                    TipBilgisi *sol_ic = tip_ic_cek(sol);
                    TipBilgisi *sag_ic = tip_ic_cek(sag);
                    if (!tip_esit(sol_ic, sag_ic)) {
                        tip_hata(tk, d, "T001",
                                 "karsilastirma iki tarafi ayni tip olmali");
                        return t_hata(tk);
                    }
                    TipBilgisi *base = t_basit(tk, TIP_MANTIKSAL);
                    return taint_yay(tk, base, sol, sag);
                }

                case OP_VE: case OP_VEYA:
                    return kontrol_ikili_mantiksal(tk, d, sol, sag);

                case OP_BIT_VE: case OP_BIT_VEYA: case OP_BIT_OZVEYA: {
                    /* SIMD Spec V1: vektör tamsayı tiplerinde bit op izinli;
                     * kesirli vektörde V006 hata. */
                    if (tip_vektor_mu(sol) || tip_vektor_mu(sag)) {
                        if (tip_vektor_mu(sol) && tip_vektor_mu(sag)) {
                            /* Kesirli vektörde bit op yasak (V006) */
                            int kesirli_elem =
                                sol->veri.vektor.eleman->kategori == TIP_KESIRLI32 ||
                                sol->veri.vektor.eleman->kategori == TIP_KESIRLI64;
                            if (kesirli_elem) {
                                tip_hata(tk, d, "V006",
                                    "kesirli vektor uzerinde bit operatoru yasak");
                                return t_hata(tk);
                            }
                        }
                        return kontrol_ikili_vektor(tk, d, sol, sag);
                    }
                    /* Bit AND/OR/XOR: her iki operand tamsayi, iç tipler eşit.
                     * Sabitsüre Spec V1: taint yayılım (sabitsüre<T> ^ T → sabitsüre<T>) */
                    if (!tip_tamsayi_mi(sol)) {
                        tip_hata(tk, d, "T028",
                                 "bit operatoru (& | ^) tamsayi tipi ister");
                        return t_hata(tk);
                    }
                    /* Bidirectional: sag, sol iç tipinde yeniden çıkarsanır
                     * (sabitsüre soyma) */
                    TipBilgisi *sol_ic = tip_ic_cek(sol);
                    TipBilgisi *sag2 = tip_belirle_beklenen(tk,
                        d->veri.ikili.sag, sol_ic);
                    if (!tip_tamsayi_mi(sag2)) {
                        tip_hata(tk, d, "T028",
                                 "bit operatoru (& | ^) tamsayi tipi ister");
                        return t_hata(tk);
                    }
                    TipBilgisi *sag2_ic = tip_ic_cek(sag2);
                    if (!tip_esit(sol_ic, sag2_ic)) {
                        tip_hata(tk, d, "T001",
                                 "bit operatoru iki tarafi ayni tip olmali");
                        return t_hata(tk);
                    }
                    return taint_yay(tk, sol_ic, sol, sag2);
                }

                case OP_SOLA_KAYDIR: case OP_SAGA_KAYDIR: {
                    /* Kaydir (<<, >>): sol tamsayi, sag tamsayi (kaydirma
                     * miktari). Sabitsüre Spec V1 CT008: kaydirma miktari
                     * (sag) sabitsüre olamaz (variable-shift bazı CPU'larda
                     * variable-time — ARM Cortex-M, eski Intel). */
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
                    if (tip_sabitsure_mi(sag)) {
                        tip_hata(tk, d, "CT008",
                            "kaydirma miktari sabitsure olamaz "
                            "(variable-shift variable-time)");
                        return t_hata(tk);
                    }
                    /* Taint yayılım: sol sabitsüre ise sonuç sabitsüre */
                    TipBilgisi *sol_ic = tip_ic_cek(sol);
                    if (tip_sabitsure_mi(sol)) {
                        return tip_olustur_sabitsure(tk->arena, sol_ic);
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
                    /* Sabitsüre Spec V1: -sabitsure<T> -> sabitsure<T> (taint korunur) */
                    return op;

                case OP_DEGIL:
                    if (!tip_mantiksal_mi(op)) {
                        tip_hata(tk, d, "T004", "'degil' mantiksal ister");
                        return t_hata(tk);
                    }
                    /* Taint korunur */
                    if (tip_sabitsure_mi(op)) {
                        return tip_olustur_sabitsure(tk->arena,
                            t_basit(tk, TIP_MANTIKSAL));
                    }
                    return t_basit(tk, TIP_MANTIKSAL);

                case OP_BIT_DEGIL:
                    if (!tip_tamsayi_mi(op)) {
                        tip_hata(tk, d, "T028",
                                 "bit DEGIL (~) tamsayi tipi ister");
                        return t_hata(tk);
                    }
                    /* Taint korunur */
                    return op;

                case OP_REF:
                    /* Linear Types Spec V1 L004 + Capability CP005:
                     * lineer (tekkez veya yetki) tipinde referans alinamaz */
                    if (tip_lineer_mi(op)) {
                        const char *kod = (op->kategori == TIP_YETKI)
                                          ? "CP005" : "L004";
                        const char *msg = (op->kategori == TIP_YETKI)
                            ? "yetki<R> tipinde referans alinamaz (linear ihlal)"
                            : "lineer (tekkez) tipinde referans alinamaz";
                        tip_hata(tk, d, kod, msg);
                        return t_hata(tk);
                    }
                    return tip_olustur_referans(tk->arena, op, 0);

                case OP_REF_DEGISKEN:
                    if (tip_lineer_mi(op)) {
                        const char *kod = (op->kategori == TIP_YETKI)
                                          ? "CP005" : "L004";
                        const char *msg = (op->kategori == TIP_YETKI)
                            ? "yetki<R> tipinde &degisken alinamaz (linear ihlal)"
                            : "lineer (tekkez) tipinde &degisken alinamaz";
                        tip_hata(tk, d, kod, msg);
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
                /* Adim 6: dizi_kapasite(d: Dizi<T>) -> tam32 */
                if (uz_b == 13 && memcmp(ad_b, "dizi_kapasite", 13) == 0) {
                    if (d->veri.cagri.sayi != 1) {
                        tip_hata(tk, d, "T010",
                            "dizi_kapasite bir arguman gerektirir");
                        return t_hata(tk);
                    }
                    TipBilgisi *dt = tip_belirle(tk, d->veri.cagri.argumanlar[0]);
                    if (dt->kategori != TIP_DIZI &&
                        dt->kategori != TIP_HATA) {
                        tip_hata(tk, d->veri.cagri.argumanlar[0], "T001",
                            "dizi_kapasite argumani Dizi<T> olmali");
                    }
                    return tip_olustur_basit(tk->arena, TIP_TAM32);
                }

                /* Adim 6: dizi_kapasite_ayarla(d, yeni: tam32) -> bos */
                if (uz_b == 20 &&
                    memcmp(ad_b, "dizi_kapasite_ayarla", 20) == 0) {
                    if (d->veri.cagri.sayi != 2) {
                        tip_hata(tk, d, "T010",
                            "dizi_kapasite_ayarla iki arguman gerektirir");
                        return t_hata(tk);
                    }
                    TipBilgisi *dt = tip_belirle(tk, d->veri.cagri.argumanlar[0]);
                    TipBilgisi *yt = tip_belirle_beklenen(tk,
                        d->veri.cagri.argumanlar[1],
                        tip_olustur_basit(tk->arena, TIP_TAM32));
                    if (dt->kategori != TIP_DIZI &&
                        dt->kategori != TIP_HATA) {
                        tip_hata(tk, d->veri.cagri.argumanlar[0], "T001",
                            "dizi_kapasite_ayarla ilk arg Dizi<T> olmali");
                    }
                    if (!tip_tamsayi_mi(yt) && yt->kategori != TIP_HATA) {
                        tip_hata(tk, d->veri.cagri.argumanlar[1], "T028",
                            "dizi_kapasite_ayarla yeni kapasite tamsayi olmali");
                    }
                    return tip_olustur_basit(tk->arena, TIP_BOS);
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
            /* Sabitsüre Spec V1 producer: sabitsure_yarat(v: T) -> sabitsure<T>
             * UTF-8: "sabits\xc3\xbcre_yarat" = 16 byte */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 16 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "sabits\xc3\xbc" "re_yarat", 16) == 0) {
                if (d->veri.cagri.sayi != 1) {
                    tip_hata(tk, d, "CT005",
                        "sabitsure_yarat tam olarak bir arguman gerektirir");
                    return t_hata(tk);
                }
                TipBilgisi *ic = tip_belirle(tk, d->veri.cagri.argumanlar[0]);
                if (ic->kategori == TIP_HATA) return t_hata(tk);
                /* Arg sabitsure ise nesting redundancy (CT006) */
                if (tip_sabitsure_mi(ic)) {
                    tip_hata(tk, d, "CT006",
                        "sabitsure<sabitsure<T>> nesting yasak");
                    return t_hata(tk);
                }
                /* İç tip yetenekli mi? */
                if (!tip_sabitsure_yetenekli_mi(ic)) {
                    tip_hata(tk, d, "CT006",
                        "sabitsure_yarat: sarilan tip constant-time yetenekli degil "
                        "(kesirli/metin/yapi yasak)");
                    return t_hata(tk);
                }
                return tip_olustur_sabitsure(tk->arena, ic);
            }
            /* === SIMD Spec V1 intrinsicleri ===
             * Bunların hepsi generic (vektör<T, N>); built-in tablo yerine
             * burada özel-cased. Argümanın tipinden T ve N çıkarılır. */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI) {
                const char *fn_ad = d->veri.cagri.hedef->veri.tanimlayici.metin;
                int fn_uz = d->veri.cagri.hedef->veri.tanimlayici.uzunluk;
                /* vektor_doldur(s: T) — V1: tek arg, dönüş <unknown,?>; bidirectional
                 * context'ten gelen beklenen tip vektor<T,N> ise ona uy.
                 * Beklenen yoksa hata. */
                if (fn_uz == 14 && memcmp(fn_ad, "vektor_doldur", 13) == 0 &&
                    fn_ad[13] == '\0') {
                    /* Bu yol kullanılmıyor — string null-terminate edilmemiş */
                }
                /* vektor_doldur(s: T) — context'ten N öğrenir (bidirectional) */
                if (fn_uz == 13 && memcmp(fn_ad, "vektor_doldur", 13) == 0) {
                    if (d->veri.cagri.sayi != 1) {
                        tip_hata(tk, d, "V020",
                            "vektor_doldur tam olarak 1 arguman alir");
                        return t_hata(tk);
                    }
                    TipBilgisi *arg_t = tip_belirle(tk,
                        d->veri.cagri.argumanlar[0]);
                    if (arg_t->kategori == TIP_HATA) return t_hata(tk);
                    if (!tip_vektor_eleman_yetenekli_mi(arg_t)) {
                        tip_hata(tk, d, "V001",
                            "vektor_doldur argumani vektor-yetenekli skaler olmali");
                        return t_hata(tk);
                    }
                    /* N bilinmiyor — beklenen tip vektor ise N'i kullan */
                    /* Default N=4 (V1 — bidirectional inference kullanılmadan) */
                    return tip_olustur_vektor(tk->arena, arg_t, 4);
                }
                /* vektor_eleman(v, i) -> T */
                if (fn_uz == 13 && memcmp(fn_ad, "vektor_eleman", 13) == 0) {
                    if (d->veri.cagri.sayi != 2) {
                        tip_hata(tk, d, "V020",
                            "vektor_eleman(v, i) 2 arguman alir");
                        return t_hata(tk);
                    }
                    TipBilgisi *vt = tip_belirle(tk,
                        d->veri.cagri.argumanlar[0]);
                    TipBilgisi *it = tip_belirle(tk,
                        d->veri.cagri.argumanlar[1]);
                    if (vt->kategori == TIP_HATA || it->kategori == TIP_HATA)
                        return t_hata(tk);
                    if (!tip_vektor_mu(vt)) {
                        tip_hata(tk, d->veri.cagri.argumanlar[0], "V020",
                            "vektor_eleman ilk arguman vektor olmali");
                        return t_hata(tk);
                    }
                    if (!tip_tamsayi_mi(it)) {
                        tip_hata(tk, d->veri.cagri.argumanlar[1], "V020",
                            "vektor_eleman ikinci arguman tamsayi olmali");
                        return t_hata(tk);
                    }
                    return vt->veri.vektor.eleman;
                }
                /* vektor_topla(v) -> T (sum reduction) */
                if (fn_uz == 12 && memcmp(fn_ad, "vektor_topla", 12) == 0) {
                    if (d->veri.cagri.sayi != 1) {
                        tip_hata(tk, d, "V009",
                            "vektor_topla 1 arguman alir");
                        return t_hata(tk);
                    }
                    TipBilgisi *vt = tip_belirle(tk,
                        d->veri.cagri.argumanlar[0]);
                    if (vt->kategori == TIP_HATA) return t_hata(tk);
                    if (!tip_vektor_mu(vt)) {
                        tip_hata(tk, d, "V009",
                            "vektor_topla operandi vektor olmali");
                        return t_hata(tk);
                    }
                    if (!tip_sayisal_mi(vt->veri.vektor.eleman)) {
                        tip_hata(tk, d, "V009",
                            "vektor_topla operandi sayisal vektor olmali");
                        return t_hata(tk);
                    }
                    return vt->veri.vektor.eleman;
                }
                /* vektor_min(v) -> T, vektor_max(v) -> T */
                if ((fn_uz == 10 && memcmp(fn_ad, "vektor_min", 10) == 0) ||
                    (fn_uz == 10 && memcmp(fn_ad, "vektor_max", 10) == 0)) {
                    if (d->veri.cagri.sayi != 1) {
                        tip_hata(tk, d, "V009",
                            "vektor_min/max 1 arguman alir");
                        return t_hata(tk);
                    }
                    TipBilgisi *vt = tip_belirle(tk,
                        d->veri.cagri.argumanlar[0]);
                    if (vt->kategori == TIP_HATA) return t_hata(tk);
                    if (!tip_vektor_mu(vt) || !tip_sayisal_mi(vt->veri.vektor.eleman)) {
                        tip_hata(tk, d, "V009",
                            "vektor_min/max operandi sayisal vektor olmali");
                        return t_hata(tk);
                    }
                    return vt->veri.vektor.eleman;
                }
                /* vektor_esit(a, b) -> vektor<mantiksal, N> */
                if (fn_uz == 11 && memcmp(fn_ad, "vektor_esit", 11) == 0) {
                    if (d->veri.cagri.sayi != 2) {
                        tip_hata(tk, d, "V020",
                            "vektor_esit 2 arguman alir");
                        return t_hata(tk);
                    }
                    TipBilgisi *at = tip_belirle(tk,
                        d->veri.cagri.argumanlar[0]);
                    TipBilgisi *bt = tip_belirle(tk,
                        d->veri.cagri.argumanlar[1]);
                    if (at->kategori == TIP_HATA || bt->kategori == TIP_HATA)
                        return t_hata(tk);
                    if (!tip_vektor_mu(at) || !tip_vektor_mu(bt)) {
                        tip_hata(tk, d, "V003",
                            "vektor_esit her iki argumani vektor olmali");
                        return t_hata(tk);
                    }
                    if (at->veri.vektor.lane_sayi != bt->veri.vektor.lane_sayi ||
                        !tip_esit(at->veri.vektor.eleman, bt->veri.vektor.eleman)) {
                        tip_hata(tk, d, "V003",
                            "vektor_esit operandlari ayni T, N olmali");
                        return t_hata(tk);
                    }
                    return tip_olustur_vektor(tk->arena,
                        tip_olustur_basit(tk->arena, TIP_MANTIKSAL),
                        at->veri.vektor.lane_sayi);
                }
                /* vektor_kucuk / vektor_buyuk (analog) */
                if ((fn_uz == 12 && memcmp(fn_ad, "vektor_kucuk", 12) == 0) ||
                    (fn_uz == 12 && memcmp(fn_ad, "vektor_buyuk", 12) == 0)) {
                    if (d->veri.cagri.sayi != 2) {
                        tip_hata(tk, d, "V020",
                            "vektor_kucuk/buyuk 2 arguman alir");
                        return t_hata(tk);
                    }
                    TipBilgisi *at = tip_belirle(tk,
                        d->veri.cagri.argumanlar[0]);
                    TipBilgisi *bt = tip_belirle(tk,
                        d->veri.cagri.argumanlar[1]);
                    if (at->kategori == TIP_HATA || bt->kategori == TIP_HATA)
                        return t_hata(tk);
                    if (!tip_vektor_mu(at) || !tip_vektor_mu(bt) ||
                        at->veri.vektor.lane_sayi != bt->veri.vektor.lane_sayi ||
                        !tip_esit(at->veri.vektor.eleman, bt->veri.vektor.eleman)) {
                        tip_hata(tk, d, "V003",
                            "vektor_kucuk/buyuk operandlari ayni T, N vektor olmali");
                        return t_hata(tk);
                    }
                    return tip_olustur_vektor(tk->arena,
                        tip_olustur_basit(tk->arena, TIP_MANTIKSAL),
                        at->veri.vektor.lane_sayi);
                }
                /* vektor_sec(mask, a, b) -> vektor<T, N> */
                if (fn_uz == 10 && memcmp(fn_ad, "vektor_sec", 10) == 0) {
                    if (d->veri.cagri.sayi != 3) {
                        tip_hata(tk, d, "V010",
                            "vektor_sec(mask, a, b) 3 arguman alir");
                        return t_hata(tk);
                    }
                    TipBilgisi *mt = tip_belirle(tk,
                        d->veri.cagri.argumanlar[0]);
                    TipBilgisi *at = tip_belirle(tk,
                        d->veri.cagri.argumanlar[1]);
                    TipBilgisi *bt = tip_belirle(tk,
                        d->veri.cagri.argumanlar[2]);
                    if (mt->kategori == TIP_HATA || at->kategori == TIP_HATA ||
                        bt->kategori == TIP_HATA) return t_hata(tk);
                    if (!tip_vektor_mu(mt) ||
                        mt->veri.vektor.eleman->kategori != TIP_MANTIKSAL) {
                        tip_hata(tk, d, "V010",
                            "vektor_sec mask vektor<mantiksal, N> olmali");
                        return t_hata(tk);
                    }
                    if (!tip_vektor_mu(at) || !tip_vektor_mu(bt) ||
                        at->veri.vektor.lane_sayi != bt->veri.vektor.lane_sayi ||
                        at->veri.vektor.lane_sayi != mt->veri.vektor.lane_sayi ||
                        !tip_esit(at->veri.vektor.eleman, bt->veri.vektor.eleman)) {
                        tip_hata(tk, d, "V010",
                            "vektor_sec a/b ayni vektor tipi, N mask ile esit olmali");
                        return t_hata(tk);
                    }
                    return at;
                }
                /* vektor_ve_azalt(v: vektor<mantiksal, N>) -> mantiksal
                 *   13 byte */
                if (fn_uz == 15 && memcmp(fn_ad, "vektor_ve_azalt", 15) == 0) {
                    if (d->veri.cagri.sayi != 1) {
                        tip_hata(tk, d, "V009",
                            "vektor_ve_azalt 1 arguman alir");
                        return t_hata(tk);
                    }
                    TipBilgisi *vt = tip_belirle(tk,
                        d->veri.cagri.argumanlar[0]);
                    if (vt->kategori == TIP_HATA) return t_hata(tk);
                    if (!tip_vektor_mu(vt) ||
                        vt->veri.vektor.eleman->kategori != TIP_MANTIKSAL) {
                        tip_hata(tk, d, "V009",
                            "vektor_ve_azalt operandi vektor<mantiksal, N> olmali");
                        return t_hata(tk);
                    }
                    return tip_olustur_basit(tk->arena, TIP_MANTIKSAL);
                }
                /* vektor_veya_azalt(v) -> mantiksal */
                if (fn_uz == 17 && memcmp(fn_ad, "vektor_veya_azalt", 17) == 0) {
                    if (d->veri.cagri.sayi != 1) {
                        tip_hata(tk, d, "V009",
                            "vektor_veya_azalt 1 arguman alir");
                        return t_hata(tk);
                    }
                    TipBilgisi *vt = tip_belirle(tk,
                        d->veri.cagri.argumanlar[0]);
                    if (vt->kategori == TIP_HATA) return t_hata(tk);
                    if (!tip_vektor_mu(vt) ||
                        vt->veri.vektor.eleman->kategori != TIP_MANTIKSAL) {
                        tip_hata(tk, d, "V009",
                            "vektor_veya_azalt operandi vektor<mantiksal, N> olmali");
                        return t_hata(tk);
                    }
                    return tip_olustur_basit(tk->arena, TIP_MANTIKSAL);
                }
            }
            /* Sabitsüre Spec V1 declassification: ifsa(s: sabitsure<T>) -> T
             * UTF-8: "if\xc5\x9fa" = 5 byte */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 5 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "if\xc5\x9f" "a", 5) == 0) {
                if (d->veri.cagri.sayi != 1) {
                    tip_hata(tk, d, "CT007",
                        "ifsa tam olarak bir arguman gerektirir");
                    return t_hata(tk);
                }
                TipBilgisi *s = tip_belirle(tk, d->veri.cagri.argumanlar[0]);
                if (s->kategori == TIP_HATA) return t_hata(tk);
                if (!tip_sabitsure_mi(s)) {
                    tip_hata(tk, d, "CT007",
                        "ifsa(...) operandi sabitsure tipinde olmali");
                    return t_hata(tk);
                }
                return s->veri.sabitsure.ic;
            }
            /* === Capability Spec V1 intrinsics === */
            /* yetki_olustur(kaynak_tipi: tam16, izin: tam16) -> yetki<R> */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 13 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "yetki_olustur", 13) == 0) {
                if (d->veri.cagri.sayi != 2) {
                    tip_hata(tk, d, "CP004",
                        "yetki_olustur tam 2 arguman gerektirir "
                        "(kaynak_tipi: tam16, izin: tam16)");
                    return t_hata(tk);
                }
                /* Arg 1: kaynak tipi sabitini (literal) bekle — R'yi cikarmak icin */
                Dugum *arg0 = d->veri.cagri.argumanlar[0];
                if (arg0->tip != DUGUM_TAM) {
                    tip_hata(tk, arg0, "CP004",
                        "yetki_olustur ilk argumani sabit tamsayi olmali "
                        "(1=Dosya 2=Soket 3=Bellek 4=Donanim 5=OTP_Anahtar)");
                    return t_hata(tk);
                }
                int64_t kt = arg0->veri.tam.deger;
                const char *kaynak_ad = NULL;
                int kaynak_uz = 0;
                switch (kt) {
                    case 1: kaynak_ad = "Dosya"; kaynak_uz = 5; break;
                    case 2: kaynak_ad = "Soket"; kaynak_uz = 5; break;
                    case 3: kaynak_ad = "Bellek"; kaynak_uz = 6; break;
                    case 4: kaynak_ad = "Donanim"; kaynak_uz = 7; break;
                    case 5: kaynak_ad = "OTP_Anahtar"; kaynak_uz = 11; break;
                    default:
                        tip_hata(tk, arg0, "CP004",
                            "yetki_olustur: bilinmeyen kaynak tipi id");
                        return t_hata(tk);
                }
                /* Arg 2: izin tipi tamsayi */
                TipBilgisi *izin_t = tip_belirle(tk, d->veri.cagri.argumanlar[1]);
                if (izin_t->kategori != TIP_HATA && !tip_tamsayi_mi(izin_t)) {
                    tip_hata(tk, d->veri.cagri.argumanlar[1], "CP004",
                        "yetki_olustur izin argumani tamsayi olmali");
                }
                /* Kaynak adi arena'ya kopyala */
                char *ad_kopya = (char *)arena_ayir(tk->arena,
                                                    (size_t)kaynak_uz + 1);
                if (ad_kopya) {
                    memcpy(ad_kopya, kaynak_ad, (size_t)kaynak_uz);
                    ad_kopya[kaynak_uz] = '\0';
                }
                TipBilgisi *kaynak = tip_olustur_yapi(tk->arena,
                                                     ad_kopya, kaynak_uz,
                                                     NULL, 0);
                return tip_olustur_yetki(tk->arena, kaynak);
            }
            /* delege(y: yetki<R>, izin: tam16) -> yetki<R>
             * y *tüketilmez*; üretilen alt-yetki linear takip edilir. */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 6 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "delege", 6) == 0) {
                if (d->veri.cagri.sayi != 2) {
                    tip_hata(tk, d, "CP004",
                        "delege tam 2 arguman gerektirir (y, izin)");
                    return t_hata(tk);
                }
                TipBilgisi *y_tip = tip_belirle(tk, d->veri.cagri.argumanlar[0]);
                if (y_tip->kategori == TIP_HATA) return t_hata(tk);
                if (!tip_yetki_mi(y_tip)) {
                    tip_hata(tk, d->veri.cagri.argumanlar[0], "CP004",
                        "delege ilk argumani yetki<R> olmali");
                    return t_hata(tk);
                }
                TipBilgisi *izin_t = tip_belirle(tk, d->veri.cagri.argumanlar[1]);
                if (izin_t->kategori != TIP_HATA && !tip_tamsayi_mi(izin_t)) {
                    tip_hata(tk, d->veri.cagri.argumanlar[1], "CP004",
                        "delege izin argumani tamsayi olmali");
                }
                /* Yeni yetki uretilir; y tuketilmez (alt-yetki kavrami) */
                return y_tip;
            }
            /* geri_al(y: yetki<R>) -> bos — y tuketilir */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 7 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "geri_al", 7) == 0) {
                if (d->veri.cagri.sayi != 1) {
                    tip_hata(tk, d, "CP004",
                        "geri_al tam 1 arguman gerektirir (yetki<R>)");
                    return t_hata(tk);
                }
                TipBilgisi *y_tip = tip_belirle(tk, d->veri.cagri.argumanlar[0]);
                if (y_tip->kategori == TIP_HATA) return t_hata(tk);
                if (!tip_yetki_mi(y_tip)) {
                    tip_hata(tk, d->veri.cagri.argumanlar[0], "CP004",
                        "geri_al argumani yetki<R> olmali");
                    return t_hata(tk);
                }
                /* Linear tuketim */
                lineer_tuket_eger_baglamaysa(tk, d->veri.cagri.argumanlar[0]);
                return tip_olustur_basit(tk->arena, TIP_BOS);
            }
            /* === Faz 2: Yetki-gated dosya I/O built-in'leri (CP.7) ===
             * Runtime: runtime/kdl_runtime.c kdl_dosya_*_yetkili.
             * Linear semantik:
             *   - dosya_ac_yetkili: üretici (Dosya kaynak tipi, izin literal)
             *   - dosya_oku_yetkili: CP-IO — y tüketilmez
             *   - dosya_yaz_yetkili: CP-IO — y tüketilmez
             *   - dosya_kapat_yetkili: CP-GERI_AL semantik — y tüketilir */

            /* dosya_ac_yetkili(yol: metin, izin: tam16) -> yetki<Dosya>
             * Hata durumunda id=0 olan yetki döner (runtime iptal=1); kullanıcı
             * yetki_id() ile kontrol eder. */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 16 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "dosya_ac_yetkili", 16) == 0) {
                if (d->veri.cagri.sayi != 2) {
                    tip_hata(tk, d, "CP004",
                        "dosya_ac_yetkili tam 2 arguman gerektirir "
                        "(yol: metin, izin: tam16)");
                    return t_hata(tk);
                }
                TipBilgisi *yol_t = tip_belirle(tk, d->veri.cagri.argumanlar[0]);
                if (yol_t->kategori != TIP_HATA && yol_t->kategori != TIP_METIN) {
                    tip_hata(tk, d->veri.cagri.argumanlar[0], "CP004",
                        "dosya_ac_yetkili ilk argumani metin olmali (yol)");
                }
                TipBilgisi *izin_t = tip_belirle(tk, d->veri.cagri.argumanlar[1]);
                if (izin_t->kategori != TIP_HATA && !tip_tamsayi_mi(izin_t)) {
                    tip_hata(tk, d->veri.cagri.argumanlar[1], "CP004",
                        "dosya_ac_yetkili izin argumani tamsayi olmali");
                }
                /* yetki<Dosya> dön — kaynak tipi nominal "Dosya" */
                const char *ad = "Dosya";
                int ad_uz = 5;
                char *ad_kopya = (char *)arena_ayir(tk->arena,
                                                    (size_t)ad_uz + 1);
                if (ad_kopya) {
                    memcpy(ad_kopya, ad, (size_t)ad_uz);
                    ad_kopya[ad_uz] = '\0';
                }
                TipBilgisi *kaynak = tip_olustur_yapi(tk->arena,
                                                     ad_kopya, ad_uz,
                                                     NULL, 0);
                return tip_olustur_yetki(tk->arena, kaynak);
            }

            /* dosya_oku_yetkili(y: yetki<Dosya>) -> metin
             * CP-IO: y *tüketilmez*. Hata durumunda boş metin döner. */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 17 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "dosya_oku_yetkili", 17) == 0) {
                if (d->veri.cagri.sayi != 1) {
                    tip_hata(tk, d, "CP004",
                        "dosya_oku_yetkili tam 1 arguman gerektirir "
                        "(yetki<Dosya>)");
                    return t_hata(tk);
                }
                TipBilgisi *y_tip = tip_belirle(tk, d->veri.cagri.argumanlar[0]);
                if (y_tip->kategori == TIP_HATA) return t_hata(tk);
                if (!tip_yetki_mi(y_tip)) {
                    tip_hata(tk, d->veri.cagri.argumanlar[0], "CP004",
                        "dosya_oku_yetkili argumani yetki<Dosya> olmali");
                    return t_hata(tk);
                }
                /* CP004: kaynak tipi Dosya olmali */
                const TipBilgisi *kaynak = tip_yetki_kaynak(y_tip);
                if (kaynak && kaynak->kategori == TIP_YAPI) {
                    if (kaynak->veri.yapi.ad_uzunluk != 5 ||
                        memcmp(kaynak->veri.yapi.ad, "Dosya", 5) != 0) {
                        tip_hata(tk, d->veri.cagri.argumanlar[0], "CP004",
                            "dosya_oku_yetkili: yetki<Dosya> bekleniyor "
                            "(yetki kaynak tipi farkli)");
                    }
                }
                /* CP-IO: y tuketilmez ama revoked olmamali (CP005 check) */
                lineer_kullanim_kontrolu(tk, d->veri.cagri.argumanlar[0]);
                return tip_olustur_basit(tk->arena, TIP_METIN);
            }

            /* dosya_yaz_yetkili(y: yetki<Dosya>, icerik: metin) -> tam32
             * CP-IO: y *tüketilmez*. Dönüş: yazılan byte sayısı (-1 hata). */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 17 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "dosya_yaz_yetkili", 17) == 0) {
                if (d->veri.cagri.sayi != 2) {
                    tip_hata(tk, d, "CP004",
                        "dosya_yaz_yetkili tam 2 arguman gerektirir "
                        "(yetki<Dosya>, icerik: metin)");
                    return t_hata(tk);
                }
                TipBilgisi *y_tip = tip_belirle(tk, d->veri.cagri.argumanlar[0]);
                if (y_tip->kategori == TIP_HATA) return t_hata(tk);
                if (!tip_yetki_mi(y_tip)) {
                    tip_hata(tk, d->veri.cagri.argumanlar[0], "CP004",
                        "dosya_yaz_yetkili ilk argumani yetki<Dosya> olmali");
                    return t_hata(tk);
                }
                const TipBilgisi *kaynak = tip_yetki_kaynak(y_tip);
                if (kaynak && kaynak->kategori == TIP_YAPI) {
                    if (kaynak->veri.yapi.ad_uzunluk != 5 ||
                        memcmp(kaynak->veri.yapi.ad, "Dosya", 5) != 0) {
                        tip_hata(tk, d->veri.cagri.argumanlar[0], "CP004",
                            "dosya_yaz_yetkili: yetki<Dosya> bekleniyor");
                    }
                }
                TipBilgisi *icerik_t = tip_belirle(tk, d->veri.cagri.argumanlar[1]);
                if (icerik_t->kategori != TIP_HATA &&
                    icerik_t->kategori != TIP_METIN) {
                    tip_hata(tk, d->veri.cagri.argumanlar[1], "CP004",
                        "dosya_yaz_yetkili ikinci argumani metin olmali");
                }
                /* CP-IO: y tuketilmez ama revoked olmamali (CP005 check) */
                lineer_kullanim_kontrolu(tk, d->veri.cagri.argumanlar[0]);
                return tip_olustur_basit(tk->arena, TIP_TAM32);
            }

            /* dosya_kapat_yetkili(y: yetki<Dosya>) -> bos
             * CP-GERI_AL semantik: y tuketilir. */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 19 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "dosya_kapat_yetkili", 19) == 0) {
                if (d->veri.cagri.sayi != 1) {
                    tip_hata(tk, d, "CP004",
                        "dosya_kapat_yetkili tam 1 arguman gerektirir "
                        "(yetki<Dosya>)");
                    return t_hata(tk);
                }
                TipBilgisi *y_tip = tip_belirle(tk, d->veri.cagri.argumanlar[0]);
                if (y_tip->kategori == TIP_HATA) return t_hata(tk);
                if (!tip_yetki_mi(y_tip)) {
                    tip_hata(tk, d->veri.cagri.argumanlar[0], "CP004",
                        "dosya_kapat_yetkili argumani yetki<Dosya> olmali");
                    return t_hata(tk);
                }
                const TipBilgisi *kaynak = tip_yetki_kaynak(y_tip);
                if (kaynak && kaynak->kategori == TIP_YAPI) {
                    if (kaynak->veri.yapi.ad_uzunluk != 5 ||
                        memcmp(kaynak->veri.yapi.ad, "Dosya", 5) != 0) {
                        tip_hata(tk, d->veri.cagri.argumanlar[0], "CP004",
                            "dosya_kapat_yetkili: yetki<Dosya> bekleniyor");
                    }
                }
                /* Linear tuketim — geri_al gibi */
                lineer_tuket_eger_baglamaysa(tk, d->veri.cagri.argumanlar[0]);
                return tip_olustur_basit(tk->arena, TIP_BOS);
            }
            /* === Concurrency / DRF V1 intrinsics === */
            /* görev_başlat(c: işlev() -> T) -> görev<T>
             * c yakaladığı lineer değerleri t_yeni'ye transfer eder (DRF-L2).
             * V1'de c bir lambda (DUGUM_LAMBDA) veya değişken (linear closure) olur. */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 14 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "g\xc3\xb6rev_ba\xc5\x9f" "lat", 14) == 0) {
                if (d->veri.cagri.sayi != 1) {
                    tip_hata(tk, d, "DRF001",
                        "gorev_baslat tam 1 arguman gerektirir "
                        "(closure: islev() -> T)");
                    return t_hata(tk);
                }
                TipBilgisi *c_tip = tip_belirle(tk, d->veri.cagri.argumanlar[0]);
                if (c_tip->kategori == TIP_HATA) return t_hata(tk);
                /* c bir islev tipi olmali (tekkez<islev(...)> da olur — LC-2) */
                const TipBilgisi *cf = c_tip;
                if (cf->kategori == TIP_TEKKEZ) cf = cf->veri.tekkez.ic;
                if (cf->kategori != TIP_ISLEV) {
                    tip_hata(tk, d->veri.cagri.argumanlar[0], "DRF001",
                        "gorev_baslat argumani islev() -> T tipinde olmali");
                    return t_hata(tk);
                }
                if (cf->veri.islev.param_sayi != 0) {
                    tip_hata(tk, d->veri.cagri.argumanlar[0], "DRF001",
                        "gorev_baslat closure'i parametresiz olmali (() -> T)");
                    return t_hata(tk);
                }
                /* Linear closure ise (LC-3 ile beraber thread'e transfer) — tuket */
                lineer_tuket_eger_baglamaysa(tk, d->veri.cagri.argumanlar[0]);
                /* görev<T> dön — T = closure dönüş tipi */
                TipBilgisi *donus = cf->veri.islev.donus;
                if (!donus) donus = tip_olustur_basit(tk->arena, TIP_BOS);
                return tip_olustur_gorev(tk->arena, donus);
            }
            /* görev_birleştir(g: görev<T>) -> T — g tuketilir (R-BİRLEŞTİR) */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 17 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "g\xc3\xb6rev_birle\xc5\x9f" "tir", 17) == 0) {
                if (d->veri.cagri.sayi != 1) {
                    tip_hata(tk, d, "DRF002",
                        "gorev_birlestir tam 1 arguman gerektirir (gorev<T>)");
                    return t_hata(tk);
                }
                TipBilgisi *g_tip = tip_belirle(tk, d->veri.cagri.argumanlar[0]);
                if (g_tip->kategori == TIP_HATA) return t_hata(tk);
                if (!tip_gorev_mu(g_tip)) {
                    tip_hata(tk, d->veri.cagri.argumanlar[0], "DRF002",
                        "gorev_birlestir argumani gorev<T> olmali");
                    return t_hata(tk);
                }
                /* Linear tuketim — g birleştirildikten sonra erişilemez */
                lineer_tuket_eger_baglamaysa(tk, d->veri.cagri.argumanlar[0]);
                /* T'yi dön */
                return g_tip->veri.gorev.ic;
            }
            /* kanal_gönder(k: kanal<T>, v: T) -> bos — v tuketilir (DRF-L5) */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 13 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "kanal_g\xc3\xb6nder", 13) == 0) {
                if (d->veri.cagri.sayi != 2) {
                    tip_hata(tk, d, "DRF003",
                        "kanal_gonder tam 2 arguman gerektirir (kanal<T>, v: T)");
                    return t_hata(tk);
                }
                TipBilgisi *k_tip = tip_belirle(tk, d->veri.cagri.argumanlar[0]);
                if (k_tip->kategori == TIP_HATA) return t_hata(tk);
                if (!tip_kanal_mu(k_tip)) {
                    tip_hata(tk, d->veri.cagri.argumanlar[0], "DRF003",
                        "kanal_gonder ilk argumani kanal<T> olmali");
                    return t_hata(tk);
                }
                TipBilgisi *t_tip = k_tip->veri.kanal.ic;
                TipBilgisi *v_tip = tip_belirle_beklenen(tk,
                    d->veri.cagri.argumanlar[1], t_tip);
                if (v_tip->kategori != TIP_HATA && !tip_esit(v_tip, t_tip)) {
                    tip_hata(tk, d->veri.cagri.argumanlar[1], "DRF003",
                        "kanal_gonder v argumani kanal eleman tipinde olmali");
                }
                /* v tuketilir (lineer ise) — k tuketilmez (kanal yeniden kullanilir) */
                lineer_tuket_eger_baglamaysa(tk, d->veri.cagri.argumanlar[1]);
                return tip_olustur_basit(tk->arena, TIP_BOS);
            }
            /* kanal_oluştur(kapasite: tam32) -> kanal<T> — Faz 3 runtime
             * V1: default eleman tipi tam32 (runtime int32-only).
             * Context-driven inference için tip_belirle_beklenen path'inde
             * beklenen kanal<T>'den T çıkarsanır. */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 14 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "kanal_olu\xc5\x9f" "tur", 14) == 0) {
                if (d->veri.cagri.sayi != 1) {
                    tip_hata(tk, d, "DRF006",
                        "kanal_olustur tam 1 arguman gerektirir (kapasite: tam32)");
                    return t_hata(tk);
                }
                TipBilgisi *kap_tip = tip_belirle_beklenen(tk,
                    d->veri.cagri.argumanlar[0],
                    tip_olustur_basit(tk->arena, TIP_TAM32));
                if (kap_tip->kategori != TIP_HATA && !tip_tamsayi_mi(kap_tip)) {
                    tip_hata(tk, d->veri.cagri.argumanlar[0], "DRF006",
                        "kanal_olustur kapasite tamsayi olmali");
                }
                /* Default eleman tipi: tam32 */
                return tip_olustur_kanal(tk->arena,
                    tip_olustur_basit(tk->arena, TIP_TAM32));
            }
            /* kanal_al(k: kanal<T>) -> T — k tuketilmez */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 8 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "kanal_al", 8) == 0) {
                if (d->veri.cagri.sayi != 1) {
                    tip_hata(tk, d, "DRF004",
                        "kanal_al tam 1 arguman gerektirir (kanal<T>)");
                    return t_hata(tk);
                }
                TipBilgisi *k_tip = tip_belirle(tk, d->veri.cagri.argumanlar[0]);
                if (k_tip->kategori == TIP_HATA) return t_hata(tk);
                if (!tip_kanal_mu(k_tip)) {
                    tip_hata(tk, d->veri.cagri.argumanlar[0], "DRF004",
                        "kanal_al argumani kanal<T> olmali");
                    return t_hata(tk);
                }
                /* T'yi dön — k tuketilmez */
                return k_tip->veri.kanal.ic;
            }
            /* dondur(v: &değişken T) -> &T — mutable referansi immutable yapar
             * (R-PAYLAŞ — Plan Karar E hibrit: built-in + frozen flag V2'de) */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 6 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "dondur", 6) == 0) {
                if (d->veri.cagri.sayi != 1) {
                    tip_hata(tk, d, "DRF005",
                        "dondur tam 1 arguman gerektirir (&degisken T)");
                    return t_hata(tk);
                }
                TipBilgisi *v_tip = tip_belirle(tk, d->veri.cagri.argumanlar[0]);
                if (v_tip->kategori == TIP_HATA) return t_hata(tk);
                if (v_tip->kategori != TIP_REFERANS ||
                    !v_tip->veri.referans.degisken_mi) {
                    tip_hata(tk, d->veri.cagri.argumanlar[0], "DRF005",
                        "dondur argumani &degisken T olmali");
                    return t_hata(tk);
                }
                /* Immutable referans dön */
                return tip_olustur_referans(tk->arena,
                                            v_tip->veri.referans.hedef, 0);
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
            /* Madde D: Multi-param + compound type generic inference.
             * GenBaglamalar ile her arg/param ciftinde unify, donus
             * tipini substitue et. */
            GenBaglamalar gb;
            gb.sayi = 0;
            /* Once unify arg tipleri ile (substitue olmadan) — daha sonra
             * argumanlar substitue edilmis param tipi context'inde tekrar
             * cikarsanir. Iki pas: pas 1 inference, pas 2 type check. */
            for (int i = 0; i < d->veri.cagri.sayi; i++) {
                TipBilgisi *param_tip = hedef_tip->veri.islev.parametreler[i];
                TipBilgisi *arg_tip = tip_belirle(tk,
                    d->veri.cagri.argumanlar[i]);
                gen_unify(&gb, param_tip, arg_tip);
            }
            /* Pas 2: arg tipini substitue edilmis param tipi context'inde
             * cikarsama + tip kontrolu */
            for (int i = 0; i < d->veri.cagri.sayi; i++) {
                TipBilgisi *param_tip = hedef_tip->veri.islev.parametreler[i];
                TipBilgisi *bek = gen_substitue(tk, param_tip, &gb);
                TipBilgisi *arg_tip = tip_belirle_beklenen(tk,
                    d->veri.cagri.argumanlar[i], bek);
                if (param_tip->kategori == TIP_GENERIC_PARAM) {
                    /* Concrete'i gb'de tutuyoruz; bind onceden yapildi */
                    continue;
                }
                /* Sabitsüre Spec V1 CT003: arg sabitsure<T> → param T leak.
                 * Önce bunu kontrol et (T001'den önce); ifsa(...) gerek. */
                if (arg_tip->kategori != TIP_HATA &&
                    param_tip->kategori != TIP_HATA &&
                    ct003_leak_kontrol(tk, d->veri.cagri.argumanlar[i],
                                        arg_tip, param_tip)) {
                    /* hata raporlandı */
                } else if (!tip_esit(arg_tip, bek) &&
                    arg_tip->kategori != TIP_HATA) {
                    tip_hata(tk, d->veri.cagri.argumanlar[i], "T001",
                             "arguman tipi parametre tipi ile uyumsuz");
                }
                /* Linear Types Spec V1 + Capability Spec V1:
                 * param lineer (tekkez veya yetki) ise arg consume */
                if (param_tip && tip_lineer_mi(param_tip)) {
                    lineer_tuket_eger_baglamaysa(tk,
                        d->veri.cagri.argumanlar[i]);
                }
            }
            /* === Adim 5: Bound-aware monomorphization check ===
             * hedef bir islev tanimlayicisi ise, tip_param_boundlari kontrol
             * et: her generic T'nin concrete tipi her bound (ozellik) icin
             * uygula tablosu kanitlamali. */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI) {
                const Sembol *fn_s = sembol_bul(tk->scope,
                    d->veri.cagri.hedef->veri.tanimlayici.metin,
                    d->veri.cagri.hedef->veri.tanimlayici.uzunluk);
                if (fn_s && fn_s->kategori == SEMBOL_ISLEV &&
                    fn_s->ast_dugumu &&
                    fn_s->ast_dugumu->tip == DUGUM_ISLEV) {
                    const Dugum *islev = fn_s->ast_dugumu;
                    int tps = islev->veri.islev.tip_param_sayi;
                    if (tps > 0 && islev->veri.islev.tip_param_bound_sayilari) {
                        for (int pi = 0; pi < tps; pi++) {
                            int bs = islev->veri.islev.tip_param_bound_sayilari[pi];
                            if (bs == 0) continue;
                            const char *tp_ad = islev->veri.islev.tip_paramlar[pi];
                            int tp_uz = (int)strlen(tp_ad);
                            const TipBilgisi *concrete = gen_bul(&gb,
                                tp_ad, tp_uz);
                            if (!concrete) continue;  /* Inferred degil — abstract */
                            if (concrete->kategori == TIP_GENERIC_PARAM) continue;
                            const char *arg_ad = NULL;
                            int arg_uz = 0;
                            if (concrete->kategori == TIP_YAPI) {
                                arg_ad = concrete->veri.yapi.ad;
                                arg_uz = concrete->veri.yapi.ad_uzunluk;
                            }
                            if (!arg_ad) continue;  /* Built-in tip — bound check yok */
                            for (int bi = 0; bi < bs; bi++) {
                                const Dugum *bd =
                                    islev->veri.islev.tip_param_boundlari[pi][bi];
                                int bd_uz = 0;
                                const char *bd_ad = tip_dugumu_kok_adi(bd, &bd_uz);
                                if (!bd_ad) continue;
                                const Sembol *oz_s = sembol_bul(tk->global_scope,
                                                                 bd_ad, bd_uz);
                                if (!oz_s || oz_s->kategori != SEMBOL_OZELLIK) {
                                    tip_hata(tk, d, "T031",
                                        "bilinmeyen ozellik (islev generic bound)");
                                    continue;
                                }
                                if (!uygula_tablosu_implementations_eder(
                                        &tk->uygulamalar,
                                        arg_ad, arg_uz, bd_ad, bd_uz)) {
                                    tip_hata(tk, d, "T030",
                                        "tip argumani islev bound'unu "
                                        "karsilamiyor (uygula bildirimi yok)");
                                }
                            }
                        }
                    }
                }
            }

            /* Donus tipi — generic param compound olabilir, substitue et */
            TipBilgisi *donus = hedef_tip->veri.islev.donus;
            return gen_substitue(tk, donus, &gb);
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
            /* Sabitsüre Spec V1: dizi de sabitsüre olabilir (sabitsure<Dizi<T>>) */
            int dizi_sabitsure = tip_sabitsure_mi(nesne_tip);
            if (dizi_sabitsure) nesne_tip = nesne_tip->veri.sabitsure.ic;

            if (nesne_tip->kategori != TIP_DIZI) {
                tip_hata(tk, d, "T008", "indeksleme dizi tipi gerek");
                return t_hata(tk);
            }
            if (!tip_tamsayi_mi(idx_tip)) {
                tip_hata(tk, d, "T005", "indeks tamsayi olmali");
                return t_hata(tk);
            }
            /* Sabitsüre Spec V1 CT002 SABITSURE_INDEX:
             * indeks sabitsüre olamaz — cache-line granülaritesinde gizli
             * bilgiyi sızdırır (Bernstein 2005 AES T-table saldırısı). */
            if (tip_sabitsure_mi(idx_tip)) {
                tip_hata(tk, d, "CT002",
                    "sabitsure tipinde dizi indeksi yasak "
                    "(cache-timing yan kanali — ifsa(idx) kullanin)");
                return t_hata(tk);
            }
            /* Sabitsüre dizi içeren eleman tipi — sabitsüre kalır (taint) */
            TipBilgisi *elem = nesne_tip->veri.dizi.eleman;
            if (dizi_sabitsure && !tip_sabitsure_mi(elem)) {
                return tip_olustur_sabitsure(tk->arena, elem);
            }
            return elem;
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
                tip_belirle_beklenen(tk, d->veri.cagri.argumanlar[0],
                    tip_olustur_basit(tk->arena, TIP_TAM64));
                return tip_olustur_dizi(tk->arena,
                    (TipBilgisi *)beklenen->veri.dizi.eleman);
            }
            /* DRF V1 Faz 3: beklenen kanal<T> ve cagri kanal_olustur(N) ise
             * — kanal<T> dön. T context'ten gelir (runtime int32-only ama
             * tip kontrolde generic). */
            if (beklenen->kategori == TIP_KANAL &&
                d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 14 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "kanal_olu\xc5\x9f" "tur", 14) == 0 &&
                d->veri.cagri.sayi == 1) {
                tip_belirle_beklenen(tk, d->veri.cagri.argumanlar[0],
                    tip_olustur_basit(tk->arena, TIP_TAM32));
                return tip_olustur_kanal(tk->arena,
                    (TipBilgisi *)beklenen->veri.kanal.ic);
            }
            /* Sabitsüre Spec V1: beklenen sabitsure<X> ve cagri
             * sabitsure_yarat(arg) ise — arg'ı X context'inde çıkarsa. */
            if (beklenen->kategori == TIP_SABITSURE &&
                d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 16 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "sabits\xc3\xbc" "re_yarat", 16) == 0 &&
                d->veri.cagri.sayi == 1) {
                TipBilgisi *ic = tip_belirle_beklenen(tk,
                    d->veri.cagri.argumanlar[0],
                    beklenen->veri.sabitsure.ic);
                if (ic->kategori == TIP_HATA) return t_hata(tk);
                if (tip_sabitsure_mi(ic)) {
                    tip_hata(tk, d, "CT006",
                        "sabitsure<sabitsure<T>> nesting yasak");
                    return t_hata(tk);
                }
                if (!tip_sabitsure_yetenekli_mi(ic)) {
                    tip_hata(tk, d, "CT006",
                        "sabitsure_yarat: sarilan tip constant-time yetenekli "
                        "degil");
                    return t_hata(tk);
                }
                return tip_olustur_sabitsure(tk->arena, ic);
            }
            /* SIMD Spec V1: beklenen vektor<T, N> ve cagri vektor_doldur(s)
             * ise — s'yi T context'inde çıkarsa, dönüş vektor<T, N>. */
            if (beklenen->kategori == TIP_VEKTOR &&
                d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 13 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "vektor_doldur", 13) == 0 &&
                d->veri.cagri.sayi == 1) {
                TipBilgisi *arg_t = tip_belirle_beklenen(tk,
                    d->veri.cagri.argumanlar[0],
                    beklenen->veri.vektor.eleman);
                if (arg_t->kategori == TIP_HATA) return t_hata(tk);
                if (!tip_vektor_eleman_yetenekli_mi(arg_t)) {
                    tip_hata(tk, d, "V001",
                        "vektor_doldur argumani vektor-yetenekli skaler olmali");
                    return t_hata(tk);
                }
                /* arg tip vs beklenen element tip uyumsuzsa T001 */
                if (!tip_esit(arg_t, beklenen->veri.vektor.eleman)) {
                    tip_hata(tk, d, "T001",
                        "vektor_doldur arg tipi beklenen element tipi ile uyumsuz");
                    return t_hata(tk);
                }
                return tip_olustur_vektor(tk->arena, arg_t,
                    beklenen->veri.vektor.lane_sayi);
            }
            break;
        }

        case DUGUM_IKILI: {
            /* Sabitsüre: beklenen sabitsüre<X> ise, operandlar X context'inde
             * çıkarsanır. */
            if (beklenen->kategori == TIP_SABITSURE) {
                TipBilgisi *ic_bek = beklenen->veri.sabitsure.ic;
                TipBilgisi *sol = tip_belirle_beklenen(tk,
                    d->veri.ikili.sol, ic_bek);
                TipBilgisi *sag = tip_belirle_beklenen(tk,
                    d->veri.ikili.sag, ic_bek);
                if (sol->kategori == TIP_HATA || sag->kategori == TIP_HATA) {
                    return t_hata(tk);
                }
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
        /* Linear Types Spec V1 LR-2: yapi tekkez/yetki/gorev alani iceremez (V1)
         * DRF V1 genişletmesi: tüm linear tipler LR-2 altında. */
        if (alan_tipi && tip_lineer_mi(alan_tipi)) {
            tip_hata(tk, alan, "LR002",
                "yapi alani lineer tipte olamaz (V1: yapi lineer alan iceremez)");
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
    /* Realtime Spec V1: işlev imzasında qualifier flag taşınır. */
    if (islev_tipi) {
        islev_tipi->veri.islev.gercekzamanli_mi =
            islev->veri.islev.gercekzamanli_mi;
    }

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
                /* Sabitsüre Spec V1 CT003: sabitsure<T> → T leak */
                if (deger_tip->kategori != TIP_HATA &&
                    annot->kategori != TIP_HATA &&
                    ct003_leak_kontrol(tk, d, deger_tip, annot)) {
                    /* hata raporlandı, atla */
                } else if (!tip_esit(annot, deger_tip) &&
                    deger_tip->kategori != TIP_HATA &&
                    annot->kategori != TIP_HATA) {
                    tip_hata(tk, d, "T001",
                             "degisken tip annot ile baslangic uyumsuz");
                }
            } else {
                deger_tip = tip_belirle(tk, d->veri.degisken.deger);
            }
            TipBilgisi *son = annot ? annot : deger_tip;
            /* Linear Types Spec V1 + Capability Spec V1:
             * deger lineer baglamadan move ise tuket (tekkez VEYA yetki). */
            if (son && tip_lineer_mi(son)) {
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
            /* Sabitsüre Spec V1 CT003: sabitsure<T> → T leak */
            if (dt->kategori != TIP_HATA && ht->kategori != TIP_HATA &&
                ct003_leak_kontrol(tk, d, dt, ht)) {
                /* hata raporlandı */
            } else if (!tip_esit(ht, dt) &&
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
                /* Sabitsüre Spec V1 CT003: sabitsure<T> dönüş normal T'ye leak */
                if (deger->kategori != TIP_HATA &&
                    tk->aktif_donus_tipi->kategori != TIP_HATA &&
                    ct003_leak_kontrol(tk, d, deger, tk->aktif_donus_tipi)) {
                    /* hata raporlandı */
                } else if (!tip_esit(deger, tk->aktif_donus_tipi) &&
                    deger->kategori != TIP_HATA &&
                    tk->aktif_donus_tipi->kategori != TIP_HATA) {
                    tip_hata(tk, d, "T020",
                             "ver tipi islev donus tipi ile uyumsuz");
                }
                /* Linear Types Spec V1 + Capability Spec V1:
                 * lineer baglama (tekkez VEYA yetki) ver ile cagirana devir → tuket */
                if (deger && tip_lineer_mi(deger)) {
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
            /* Sabitsüre Spec V1 CT001 SABITSURE_IF_BRANCH:
             * gizli değer üzerinde dallanma timing kanalı açar. */
            if (tip_sabitsure_mi(kosul)) {
                tip_hata(tk, d, "CT001",
                    "eger kosulu sabitsure tipinde olamaz "
                    "(timing leak; ifsa(...) kullanin)");
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
            /* Sabitsüre Spec V1 CT001 SABITSURE_WHILE_BRANCH */
            if (tip_sabitsure_mi(kosul)) {
                tip_hata(tk, d, "CT001",
                    "iken kosulu sabitsure tipinde olamaz "
                    "(loop iteration count = gizli = timing leak)");
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
            /* Sabitsüre Spec V1 CT001 SABITSURE_MATCH: scrutinee sabitsure
             * olamaz — kol seçimi gizli bilgiyle dallanır. */
            if (tip_sabitsure_mi(dt)) {
                tip_hata(tk, d, "CT001",
                    "esles deger sabitsure tipinde olamaz "
                    "(kol secimi timing leak)");
            }
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
