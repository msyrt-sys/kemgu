#include "tip.h"

#include <string.h>

/* === Olusturucular === */

TipBilgisi *tip_olustur_basit(Arena *a, TipKategorisi k) {
    TipBilgisi *t = (TipBilgisi *)arena_ayir_sifir(a, sizeof(TipBilgisi));
    if (!t) return NULL;
    t->kategori = k;
    return t;
}

TipBilgisi *tip_olustur_referans(Arena *a, TipBilgisi *hedef, int degisken_mi) {
    TipBilgisi *t = tip_olustur_basit(a, TIP_REFERANS);
    if (!t) return NULL;
    t->veri.referans.hedef = hedef;
    t->veri.referans.degisken_mi = degisken_mi ? 1 : 0;
    return t;
}

TipBilgisi *tip_olustur_pointer(Arena *a, TipBilgisi *hedef) {
    TipBilgisi *t = tip_olustur_basit(a, TIP_POINTER);
    if (!t) return NULL;
    t->veri.pointer.hedef = hedef;
    return t;
}

TipBilgisi *tip_olustur_dizi(Arena *a, TipBilgisi *eleman) {
    TipBilgisi *t = tip_olustur_basit(a, TIP_DIZI);
    if (!t) return NULL;
    t->veri.dizi.eleman = eleman;
    return t;
}

TipBilgisi *tip_olustur_secimlik(Arena *a, TipBilgisi *ic) {
    TipBilgisi *t = tip_olustur_basit(a, TIP_SECIMLIK);
    if (!t) return NULL;
    t->veri.secimlik.ic = ic;
    return t;
}

TipBilgisi *tip_olustur_sonuc(Arena *a, TipBilgisi *deger, TipBilgisi *hata) {
    TipBilgisi *t = tip_olustur_basit(a, TIP_SONUC);
    if (!t) return NULL;
    t->veri.sonuc.deger = deger;
    t->veri.sonuc.hata = hata;
    return t;
}

TipBilgisi *tip_olustur_islev(Arena *a, TipBilgisi **params, int param_sayi,
                               TipBilgisi *donus) {
    TipBilgisi *t = tip_olustur_basit(a, TIP_ISLEV);
    if (!t) return NULL;
    t->veri.islev.parametreler = params;
    t->veri.islev.param_sayi = param_sayi;
    t->veri.islev.donus = donus;
    return t;
}

TipBilgisi *tip_olustur_yapi(Arena *a, const char *ad, int ad_uzunluk,
                              TipBilgisi **tip_arg, int tip_arg_sayi) {
    TipBilgisi *t = tip_olustur_basit(a, TIP_YAPI);
    if (!t) return NULL;
    t->veri.yapi.ad = ad;  /* arena'da kopyalanmis varsayim */
    t->veri.yapi.ad_uzunluk = ad_uzunluk;
    t->veri.yapi.tip_arg = tip_arg;
    t->veri.yapi.tip_arg_sayi = tip_arg_sayi;
    return t;
}

TipBilgisi *tip_olustur_generic_param(Arena *a, const char *ad, int ad_uzunluk) {
    TipBilgisi *t = tip_olustur_basit(a, TIP_GENERIC_PARAM);
    if (!t) return NULL;
    t->veri.generic_param.ad = ad;
    t->veri.generic_param.ad_uzunluk = ad_uzunluk;
    return t;
}

/* === Esitlik (nominal, recursive) === */

int tip_esit(const TipBilgisi *a, const TipBilgisi *b) {
    if (!a || !b) return 0;
    if (a == b) return 1;
    /* Generic param ozel kural:
     *   - Iki generic param: ad esit ise esit, degilse degil
     *   - Bir taraf generic, diger concrete: deferred — esit kabul
     *   (bu sayede T == 0 (i32) gibi karsilastirmalar generic gov dede gecer) */
    if (a->kategori == TIP_GENERIC_PARAM &&
        b->kategori == TIP_GENERIC_PARAM) {
        return a->veri.generic_param.ad_uzunluk == b->veri.generic_param.ad_uzunluk
            && memcmp(a->veri.generic_param.ad, b->veri.generic_param.ad,
                      (size_t)a->veri.generic_param.ad_uzunluk) == 0;
    }
    if (a->kategori == TIP_GENERIC_PARAM || b->kategori == TIP_GENERIC_PARAM) {
        return 1;
    }
    if (a->kategori != b->kategori) return 0;

    switch (a->kategori) {
        /* Basit tipler — kategori esitse OK */
        case TIP_TAM8:    case TIP_TAM16:    case TIP_TAM32:   case TIP_TAM64:
        case TIP_DTAM8:   case TIP_DTAM16:   case TIP_DTAM32:  case TIP_DTAM64:
        case TIP_KESIRLI32: case TIP_KESIRLI64:
        case TIP_MANTIKSAL: case TIP_KARAKTER: case TIP_METIN: case TIP_BOS:
        case TIP_BILINMIYOR: case TIP_HATA:
            return 1;

        case TIP_REFERANS:
            return a->veri.referans.degisken_mi == b->veri.referans.degisken_mi
                && tip_esit(a->veri.referans.hedef, b->veri.referans.hedef);

        case TIP_POINTER:
            return tip_esit(a->veri.pointer.hedef, b->veri.pointer.hedef);

        case TIP_DIZI:
            return tip_esit(a->veri.dizi.eleman, b->veri.dizi.eleman);

        case TIP_SECIMLIK:
            return tip_esit(a->veri.secimlik.ic, b->veri.secimlik.ic);

        case TIP_SONUC:
            return tip_esit(a->veri.sonuc.deger, b->veri.sonuc.deger)
                && tip_esit(a->veri.sonuc.hata, b->veri.sonuc.hata);

        case TIP_ISLEV: {
            if (a->veri.islev.param_sayi != b->veri.islev.param_sayi) return 0;
            for (int i = 0; i < a->veri.islev.param_sayi; i++) {
                if (!tip_esit(a->veri.islev.parametreler[i],
                              b->veri.islev.parametreler[i])) return 0;
            }
            return tip_esit(a->veri.islev.donus, b->veri.islev.donus);
        }

        case TIP_YAPI: {
            /* Nominal: ad esit + generic args esit */
            if (a->veri.yapi.ad_uzunluk != b->veri.yapi.ad_uzunluk) return 0;
            if (memcmp(a->veri.yapi.ad, b->veri.yapi.ad,
                       (size_t)a->veri.yapi.ad_uzunluk) != 0) return 0;
            if (a->veri.yapi.tip_arg_sayi != b->veri.yapi.tip_arg_sayi) return 0;
            for (int i = 0; i < a->veri.yapi.tip_arg_sayi; i++) {
                if (!tip_esit(a->veri.yapi.tip_arg[i],
                              b->veri.yapi.tip_arg[i])) return 0;
            }
            return 1;
        }

        case TIP_GENERIC_PARAM:
            if (a->veri.generic_param.ad_uzunluk !=
                b->veri.generic_param.ad_uzunluk) return 0;
            return memcmp(a->veri.generic_param.ad,
                          b->veri.generic_param.ad,
                          (size_t)a->veri.generic_param.ad_uzunluk) == 0;
    }
    return 0;
}

/* === Yazdirma === */

void tip_yazdir(const TipBilgisi *t, FILE *out) {
    if (!t) { fputs("(NULL)", out); return; }

    switch (t->kategori) {
        case TIP_TAM8:    fputs("tam8",    out); return;
        case TIP_TAM16:   fputs("tam16",   out); return;
        case TIP_TAM32:   fputs("tam32",   out); return;
        case TIP_TAM64:   fputs("tam64",   out); return;
        case TIP_DTAM8:   fputs("dtam8",   out); return;
        case TIP_DTAM16:  fputs("dtam16",  out); return;
        case TIP_DTAM32:  fputs("dtam32",  out); return;
        case TIP_DTAM64:  fputs("dtam64",  out); return;
        case TIP_KESIRLI32: fputs("kesirli32", out); return;
        case TIP_KESIRLI64: fputs("kesirli64", out); return;
        case TIP_MANTIKSAL: fputs("mantiksal", out); return;
        case TIP_KARAKTER:  fputs("karakter",  out); return;
        case TIP_METIN:     fputs("metin",     out); return;
        case TIP_BOS:       fputs("bos",       out); return;

        case TIP_REFERANS:
            fputs(t->veri.referans.degisken_mi ? "&degisken " : "&", out);
            tip_yazdir(t->veri.referans.hedef, out);
            return;

        case TIP_POINTER:
            fputc('*', out);
            tip_yazdir(t->veri.pointer.hedef, out);
            return;

        case TIP_DIZI:
            fputs("Dizi<", out);
            tip_yazdir(t->veri.dizi.eleman, out);
            fputc('>', out);
            return;

        case TIP_SECIMLIK:
            fputs("secimlik<", out);
            tip_yazdir(t->veri.secimlik.ic, out);
            fputc('>', out);
            return;

        case TIP_SONUC:
            fputs("sonuc<", out);
            tip_yazdir(t->veri.sonuc.deger, out);
            fputs(", ", out);
            tip_yazdir(t->veri.sonuc.hata, out);
            fputc('>', out);
            return;

        case TIP_ISLEV:
            fputs("islev(", out);
            for (int i = 0; i < t->veri.islev.param_sayi; i++) {
                if (i > 0) fputs(", ", out);
                tip_yazdir(t->veri.islev.parametreler[i], out);
            }
            fputs(") -> ", out);
            tip_yazdir(t->veri.islev.donus, out);
            return;

        case TIP_YAPI:
            fwrite(t->veri.yapi.ad, 1,
                   (size_t)t->veri.yapi.ad_uzunluk, out);
            if (t->veri.yapi.tip_arg_sayi > 0) {
                fputc('<', out);
                for (int i = 0; i < t->veri.yapi.tip_arg_sayi; i++) {
                    if (i > 0) fputs(", ", out);
                    tip_yazdir(t->veri.yapi.tip_arg[i], out);
                }
                fputc('>', out);
            }
            return;

        case TIP_GENERIC_PARAM:
            fwrite(t->veri.generic_param.ad, 1,
                   (size_t)t->veri.generic_param.ad_uzunluk, out);
            return;

        case TIP_BILINMIYOR: fputs("?", out); return;
        case TIP_HATA:       fputs("(HATA)", out); return;
    }
    fputs("(BILINMEYEN)", out);
}

const char *tip_kategorisi_adi(TipKategorisi k) {
    switch (k) {
        case TIP_TAM8:    return "TAM8";
        case TIP_TAM16:   return "TAM16";
        case TIP_TAM32:   return "TAM32";
        case TIP_TAM64:   return "TAM64";
        case TIP_DTAM8:   return "DTAM8";
        case TIP_DTAM16:  return "DTAM16";
        case TIP_DTAM32:  return "DTAM32";
        case TIP_DTAM64:  return "DTAM64";
        case TIP_KESIRLI32: return "KESIRLI32";
        case TIP_KESIRLI64: return "KESIRLI64";
        case TIP_MANTIKSAL: return "MANTIKSAL";
        case TIP_KARAKTER:  return "KARAKTER";
        case TIP_METIN:     return "METIN";
        case TIP_BOS:       return "BOS";
        case TIP_REFERANS:  return "REFERANS";
        case TIP_POINTER:   return "POINTER";
        case TIP_DIZI:      return "DIZI";
        case TIP_SECIMLIK:  return "SECIMLIK";
        case TIP_SONUC:     return "SONUC";
        case TIP_ISLEV:     return "ISLEV";
        case TIP_YAPI:      return "YAPI";
        case TIP_GENERIC_PARAM: return "GENERIC_PARAM";
        case TIP_BILINMIYOR: return "BILINMIYOR";
        case TIP_HATA:      return "HATA";
    }
    return "BILINMEYEN";
}

/* === Yardimcilar === */

int tip_sayisal_mi(const TipBilgisi *t) {
    if (!t) return 0;
    switch (t->kategori) {
        case TIP_TAM8:    case TIP_TAM16:    case TIP_TAM32:    case TIP_TAM64:
        case TIP_DTAM8:   case TIP_DTAM16:   case TIP_DTAM32:   case TIP_DTAM64:
        case TIP_KESIRLI32: case TIP_KESIRLI64:
            return 1;
        case TIP_GENERIC_PARAM:
            /* Generic param: gercek kontrol instantiation'da yapilir.
             * Generic islev govdesinde T'nin sayisal olduğunu kabul ediyoruz —
             * eger degilse callsite'da hata cikar (monomorphized kodda). */
            return 1;
        default:
            return 0;
    }
}

int tip_tamsayi_mi(const TipBilgisi *t) {
    if (!t) return 0;
    switch (t->kategori) {
        case TIP_TAM8:    case TIP_TAM16:    case TIP_TAM32:    case TIP_TAM64:
        case TIP_DTAM8:   case TIP_DTAM16:   case TIP_DTAM32:   case TIP_DTAM64:
            return 1;
        case TIP_GENERIC_PARAM:
            return 1;  /* deferred */
        default:
            return 0;
    }
}

int tip_mantiksal_mi(const TipBilgisi *t) {
    if (!t) return 0;
    if (t->kategori == TIP_MANTIKSAL) return 1;
    if (t->kategori == TIP_GENERIC_PARAM) return 1;  /* deferred */
    return 0;
}
