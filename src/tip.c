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

TipBilgisi *tip_olustur_tekkez(Arena *a, TipBilgisi *ic) {
    TipBilgisi *t = tip_olustur_basit(a, TIP_TEKKEZ);
    if (!t) return NULL;
    t->veri.tekkez.ic = ic;
    return t;
}

TipBilgisi *tip_olustur_sabitsure(Arena *a, TipBilgisi *ic) {
    TipBilgisi *t = tip_olustur_basit(a, TIP_SABITSURE);
    if (!t) return NULL;
    t->veri.sabitsure.ic = ic;
    return t;
}

TipBilgisi *tip_olustur_yetki(Arena *a, TipBilgisi *kaynak) {
    TipBilgisi *t = tip_olustur_basit(a, TIP_YETKI);
    if (!t) return NULL;
    t->veri.yetki.kaynak = kaynak;
    return t;
}

int tip_yetki_mi(const TipBilgisi *t) {
    if (!t) return 0;
    if (t->kategori == TIP_YETKI) return 1;
    /* tekkez<yetki<R>> da capability sayilir */
    if (t->kategori == TIP_TEKKEZ) return tip_yetki_mi(t->veri.tekkez.ic);
    return 0;
}

const TipBilgisi *tip_yetki_kaynak(const TipBilgisi *t) {
    if (!t) return NULL;
    if (t->kategori == TIP_YETKI) return t->veri.yetki.kaynak;
    if (t->kategori == TIP_TEKKEZ) return tip_yetki_kaynak(t->veri.tekkez.ic);
    return NULL;
}

TipBilgisi *tip_olustur_vektor(Arena *a, TipBilgisi *eleman, int lane_sayi) {
    TipBilgisi *t = tip_olustur_basit(a, TIP_VEKTOR);
    if (!t) return NULL;
    t->veri.vektor.eleman = eleman;
    t->veri.vektor.lane_sayi = lane_sayi;
    return t;
}

TipBilgisi *tip_olustur_gorev(Arena *a, TipBilgisi *ic) {
    TipBilgisi *t = tip_olustur_basit(a, TIP_GOREV);
    if (!t) return NULL;
    t->veri.gorev.ic = ic;
    return t;
}

TipBilgisi *tip_olustur_kanal(Arena *a, TipBilgisi *ic) {
    TipBilgisi *t = tip_olustur_basit(a, TIP_KANAL);
    if (!t) return NULL;
    t->veri.kanal.ic = ic;
    return t;
}

int tip_gorev_mu(const TipBilgisi *t) {
    return t != NULL && t->kategori == TIP_GOREV;
}

int tip_kanal_mu(const TipBilgisi *t) {
    return t != NULL && t->kategori == TIP_KANAL;
}


int tip_lineer_mi(const TipBilgisi *t) {
    if (!t) return 0;
    /* tekkez<...> + Capability Spec V1: yetki<R> de linear takip edilir.
     * Concurrency / DRF V1: görev<T> linear (tek-birleştirme: DRF-L1/L2).
     * kanal<T> non-linear V1: kanal endpoint birden çok thread arasında
     * "transfer tamponu" (ρ_kanal); v gönderiminde v tüketilir ama kanal
     * yeniden kullanılır. */
    return t->kategori == TIP_TEKKEZ
        || t->kategori == TIP_YETKI
        || t->kategori == TIP_GOREV;
}

int tip_sabitsure_mi(const TipBilgisi *t) {
    return t != NULL && t->kategori == TIP_SABITSURE;
}

int tip_sabitsure_yetenekli_mi(const TipBilgisi *t) {
    if (!t) return 0;
    switch (t->kategori) {
        /* Sabit-süre op'lar var: tamX, dtamX */
        case TIP_TAM8:    case TIP_TAM16:    case TIP_TAM32:    case TIP_TAM64:
        case TIP_DTAM8:   case TIP_DTAM16:   case TIP_DTAM32:   case TIP_DTAM64:
            return 1;
        /* karakter = tam32 alt-küme */
        case TIP_KARAKTER: return 1;
        /* mantıksal i1, CT select destekli */
        case TIP_MANTIKSAL: return 1;
        /* Dizi: eleman sabitsüre-yetenekli olmalı */
        case TIP_DIZI: return tip_sabitsure_yetenekli_mi(t->veri.dizi.eleman);
        /* Generic param: deferred — instantiation'da kontrol */
        case TIP_GENERIC_PARAM: return 1;
        /* Yasaklı (CT006 SABITSURE_WRAP_INVALID):
         *   kesirli32/64 — FP variable-time (fdiv, sqrt, denormal)
         *   metin — UTF-8 değişken-uzunluk
         *   yapı — V1: dış sarmalayıcı yerine alan-bazlı zorunlu
         *   seçimlik/sonuç — tag check dallanma
         *   işlev — call dispatch zamanlama
         *   referans/pointer — pointer-bazlı saldırılar
         *   tekkez — V1: ortogonal, V2'de combine
         *   sabitsüre — nesting redundancy (CT006) */
        default: return 0;
    }
}

int tip_sabitsure_uyumlu_mu(const TipBilgisi *kaynak, const TipBilgisi *hedef) {
    if (!kaynak || !hedef) return 1;  /* NULL = hata zaten — başka yerde raporlanır */

    int k_ct = tip_sabitsure_mi(kaynak);
    int h_ct = tip_sabitsure_mi(hedef);

    /* sabitsüre<T> → T (CT003 leak) */
    if (k_ct && !h_ct) return 0;
    /* T → sabitsüre<T>: V1'de explicit sabitsüre_olustur şart — caller'da
     * tip_esit ile yakalanır (uyumsuz). Burada normalde uyumsuz dönmeliydik
     * ama bidirectional inference için 1 dönüp tip_esit'e bırakacağız. */
    return 1;
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
    /* TIP_BILINMIYOR: konstrüktörler (hiç, tamam, hata) bunu üretir.
     * Inference için diğer tipler ile uyumlu kabul edilir. */
    if (a->kategori == TIP_BILINMIYOR || b->kategori == TIP_BILINMIYOR) {
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

        case TIP_TEKKEZ:
            return tip_esit(a->veri.tekkez.ic, b->veri.tekkez.ic);

        case TIP_SABITSURE:
            /* Nominal: sabitsüre<T> == sabitsüre<T'> iff T == T'.
             * sabitsüre<T> ile T arasında tip_esit DAİMA 0 (kategoriler farklı,
             * yukarıda zaten reject edildi) — bu sayede implicit upgrade/downgrade
             * tip kontrol seviyesinde T001 verir. */
            return tip_esit(a->veri.sabitsure.ic, b->veri.sabitsure.ic);

        case TIP_YETKI:
            /* Capability Spec V1: yetki<R> == yetki<R'> iff R == R'.
             * Tip eslesmesi nominal — Dosya != Soket vs. (CP004 enforcement). */
            return tip_esit(a->veri.yetki.kaynak, b->veri.yetki.kaynak);

        case TIP_VEKTOR:
            /* SIMD Spec V1: vektör<T1, N1> == vektör<T2, N2> iff
             * T1==T2 ve N1==N2 (nominal, recursive). */
            return a->veri.vektor.lane_sayi == b->veri.vektor.lane_sayi
                && tip_esit(a->veri.vektor.eleman, b->veri.vektor.eleman);

        case TIP_GOREV:
            /* DRF V1: görev<T1> == görev<T2> iff T1==T2 */
            return tip_esit(a->veri.gorev.ic, b->veri.gorev.ic);

        case TIP_KANAL:
            /* DRF V1: kanal<T1> == kanal<T2> iff T1==T2 */
            return tip_esit(a->veri.kanal.ic, b->veri.kanal.ic);
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

        case TIP_TEKKEZ:
            fputs("tekkez<", out);
            tip_yazdir(t->veri.tekkez.ic, out);
            fputc('>', out);
            return;

        case TIP_SABITSURE:
            fputs("sabitsure<", out);
            tip_yazdir(t->veri.sabitsure.ic, out);
            fputc('>', out);
            return;

        case TIP_YETKI:
            fputs("yetki<", out);
            tip_yazdir(t->veri.yetki.kaynak, out);
            fputc('>', out);
            return;

        case TIP_VEKTOR:
            fputs("vektor<", out);
            tip_yazdir(t->veri.vektor.eleman, out);
            fprintf(out, ", %d>", t->veri.vektor.lane_sayi);
            return;

        case TIP_GOREV:
            fputs("gorev<", out);
            tip_yazdir(t->veri.gorev.ic, out);
            fputc('>', out);
            return;

        case TIP_KANAL:
            fputs("kanal<", out);
            tip_yazdir(t->veri.kanal.ic, out);
            fputc('>', out);
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
        case TIP_TEKKEZ:    return "TEKKEZ";
        case TIP_SABITSURE: return "SABITSURE";
        case TIP_YETKI:     return "YETKI";
        case TIP_VEKTOR:    return "VEKTOR";
        case TIP_GOREV:     return "GOREV";
        case TIP_KANAL:     return "KANAL";
        case TIP_BILINMIYOR: return "BILINMIYOR";
        case TIP_HATA:      return "HATA";
    }
    return "BILINMEYEN";
}

/* === Yardimcilar === */

int tip_sayisal_mi(const TipBilgisi *t) {
    if (!t) return 0;
    /* Sabitsüre Spec V1: sabitsüre<T> → iç T sayısal ise sayısal kabul */
    if (t->kategori == TIP_SABITSURE) return tip_sayisal_mi(t->veri.sabitsure.ic);
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
    if (t->kategori == TIP_SABITSURE) return tip_tamsayi_mi(t->veri.sabitsure.ic);
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
    if (t->kategori == TIP_SABITSURE) return tip_mantiksal_mi(t->veri.sabitsure.ic);
    if (t->kategori == TIP_MANTIKSAL) return 1;
    if (t->kategori == TIP_GENERIC_PARAM) return 1;  /* deferred */
    return 0;
}

/* === SIMD Spec V1 yardımcıları === */

int tip_vektor_eleman_yetenekli_mi(const TipBilgisi *t) {
    if (!t) return 0;
    switch (t->kategori) {
        case TIP_TAM8:    case TIP_TAM16:    case TIP_TAM32:    case TIP_TAM64:
        case TIP_DTAM8:   case TIP_DTAM16:   case TIP_DTAM32:   case TIP_DTAM64:
        case TIP_KESIRLI32: case TIP_KESIRLI64:
        case TIP_MANTIKSAL:
            return 1;
        case TIP_GENERIC_PARAM:
            return 1;  /* deferred */
        default:
            return 0;
    }
}

int tip_vektor_lane_gecerli_mi(int n) {
    switch (n) {
        case 2:
        case 4:
        case 8:
        case 16:
        case 32:
        case 64:
            return 1;
        default:
            return 0;
    }
}

int tip_vektor_mu(const TipBilgisi *t) {
    return t != NULL && t->kategori == TIP_VEKTOR;
}
