#include "ast.h"

#include <string.h>   /* memcpy, strlen */

/* === Yardimcilar === */

char *ast_string_kopyala(Arena *a, const char *kaynak, int uzunluk) {
    if (!a || !kaynak || uzunluk <= 0) return NULL;
    char *kopya = (char *)arena_ayir(a, (size_t)uzunluk + 1);
    if (!kopya) return NULL;
    memcpy(kopya, kaynak, (size_t)uzunluk);
    kopya[uzunluk] = '\0';
    return kopya;
}

/* === Generic olusturucu === */

Dugum *dugum_olustur(Arena *a, DugumTipi tip, int satir, int sutun) {
    if (!a) return NULL;
    Dugum *d = (Dugum *)arena_ayir_sifir(a, sizeof(Dugum));
    if (!d) return NULL;
    d->tip = tip;
    d->satir = satir;
    d->sutun = sutun;
    return d;
}

/* === Literaller === */

Dugum *dugum_tam(Arena *a, int64_t deger, int satir, int sutun) {
    Dugum *d = dugum_olustur(a, DUGUM_TAM, satir, sutun);
    if (!d) return NULL;
    d->veri.tam.deger = deger;
    return d;
}

Dugum *dugum_kesirli(Arena *a, double deger, int satir, int sutun) {
    Dugum *d = dugum_olustur(a, DUGUM_KESIRLI, satir, sutun);
    if (!d) return NULL;
    d->veri.kesirli.deger = deger;
    return d;
}

Dugum *dugum_metin(Arena *a, const char *metin, int uzunluk,
                   int satir, int sutun) {
    Dugum *d = dugum_olustur(a, DUGUM_METIN, satir, sutun);
    if (!d) return NULL;
    d->veri.metin_lit.metin = ast_string_kopyala(a, metin, uzunluk);
    d->veri.metin_lit.uzunluk = uzunluk;
    return d;
}

Dugum *dugum_karakter(Arena *a, uint32_t kod_noktasi, int satir, int sutun) {
    Dugum *d = dugum_olustur(a, DUGUM_KARAKTER, satir, sutun);
    if (!d) return NULL;
    d->veri.karakter.kod_noktasi = kod_noktasi;
    return d;
}

Dugum *dugum_mantiksal(Arena *a, int deger, int satir, int sutun) {
    Dugum *d = dugum_olustur(a, DUGUM_MANTIKSAL, satir, sutun);
    if (!d) return NULL;
    d->veri.mantiksal.deger = deger ? 1 : 0;
    return d;
}

Dugum *dugum_bos(Arena *a, int satir, int sutun) {
    return dugum_olustur(a, DUGUM_BOS, satir, sutun);
}

Dugum *dugum_tanimlayici(Arena *a, const char *metin, int uzunluk,
                         int satir, int sutun) {
    Dugum *d = dugum_olustur(a, DUGUM_TANIMLAYICI, satir, sutun);
    if (!d) return NULL;
    d->veri.tanimlayici.metin = ast_string_kopyala(a, metin, uzunluk);
    d->veri.tanimlayici.uzunluk = uzunluk;
    return d;
}

/* === Ifadeler === */

Dugum *dugum_ikili(Arena *a, Operator op, Dugum *sol, Dugum *sag,
                   int satir, int sutun) {
    Dugum *d = dugum_olustur(a, DUGUM_IKILI, satir, sutun);
    if (!d) return NULL;
    d->veri.ikili.op = op;
    d->veri.ikili.sol = sol;
    d->veri.ikili.sag = sag;
    return d;
}

Dugum *dugum_tekli(Arena *a, Operator op, Dugum *operand,
                   int satir, int sutun) {
    Dugum *d = dugum_olustur(a, DUGUM_TEKLI, satir, sutun);
    if (!d) return NULL;
    d->veri.tekli.op = op;
    d->veri.tekli.operand = operand;
    return d;
}

/* === Konteyner dugumler === */

Dugum *dugum_blok(Arena *a, Dugum **deyimler, int sayi,
                  int satir, int sutun) {
    Dugum *d = dugum_olustur(a, DUGUM_BLOK, satir, sutun);
    if (!d) return NULL;
    d->veri.blok.deyimler = deyimler;
    d->veri.blok.sayi = sayi;
    return d;
}

Dugum *dugum_program(Arena *a, Dugum **uyeler, int sayi,
                     int satir, int sutun) {
    Dugum *d = dugum_olustur(a, DUGUM_PROGRAM, satir, sutun);
    if (!d) return NULL;
    d->veri.program.uyeler = uyeler;
    d->veri.program.sayi = sayi;
    return d;
}

Dugum *dugum_eger(Arena *a, Dugum *kosul, Dugum *gozdoldur, Dugum *yan,
                  int satir, int sutun) {
    Dugum *d = dugum_olustur(a, DUGUM_EGER, satir, sutun);
    if (!d) return NULL;
    d->veri.eger.kosul = kosul;
    d->veri.eger.gozdoldur = gozdoldur;
    d->veri.eger.yan = yan;
    return d;
}

Dugum *dugum_ver(Arena *a, Dugum *deger, int satir, int sutun) {
    Dugum *d = dugum_olustur(a, DUGUM_VER, satir, sutun);
    if (!d) return NULL;
    d->veri.ver.deger = deger;
    return d;
}

Dugum *dugum_hata(Arena *a, int satir, int sutun) {
    return dugum_olustur(a, DUGUM_HATA, satir, sutun);
}

/* === Enum -> string === */

const char *dugum_tipi_adi(DugumTipi tip) {
    switch (tip) {
        case DUGUM_PROGRAM:           return "PROGRAM";
        case DUGUM_MODUL:             return "MODUL";
        case DUGUM_KULLAN:            return "KULLAN";
        case DUGUM_DISA:              return "DISA";
        case DUGUM_ISLEV:             return "ISLEV";
        case DUGUM_YAPI:              return "YAPI";
        case DUGUM_OZELLIK:           return "OZELLIK";
        case DUGUM_UYGULA:            return "UYGULA";
        case DUGUM_SABIT:             return "SABIT";
        case DUGUM_PARAMETRE:         return "PARAMETRE";
        case DUGUM_ALAN:              return "ALAN";
        case DUGUM_DEGISKEN:          return "DEGISKEN";
        case DUGUM_ATAMA:             return "ATAMA";
        case DUGUM_VER:               return "VER";
        case DUGUM_EGER:              return "EGER";
        case DUGUM_IKEN:              return "IKEN";
        case DUGUM_ICIN:              return "ICIN";
        case DUGUM_ESLES:             return "ESLES";
        case DUGUM_GUVENSIZ:          return "GUVENSIZ";
        case DUGUM_BLOK:              return "BLOK";
        case DUGUM_IFADE_DEYIMI:      return "IFADE_DEYIMI";
        case DUGUM_IKILI:             return "IKILI";
        case DUGUM_TEKLI:             return "TEKLI";
        case DUGUM_CAGRI:             return "CAGRI";
        case DUGUM_ERISIM:            return "ERISIM";
        case DUGUM_INDEKS:            return "INDEKS";
        case DUGUM_YOL:               return "YOL";
        case DUGUM_LAMBDA:            return "LAMBDA";
        case DUGUM_YAPI_OLUSTUR:      return "YAPI_OLUSTUR";
        case DUGUM_DIZI_OLUSTUR:      return "DIZI_OLUSTUR";
        case DUGUM_ALAN_ATAMA:        return "ALAN_ATAMA";
        case DUGUM_TAM:               return "TAM";
        case DUGUM_KESIRLI:           return "KESIRLI";
        case DUGUM_METIN:             return "METIN";
        case DUGUM_KARAKTER:          return "KARAKTER";
        case DUGUM_MANTIKSAL:         return "MANTIKSAL";
        case DUGUM_BOS:               return "BOS";
        case DUGUM_TANIMLAYICI:       return "TANIMLAYICI";
        case DUGUM_TIP_BASIT:         return "TIP_BASIT";
        case DUGUM_TIP_REFERANS:      return "TIP_REFERANS";
        case DUGUM_TIP_POINTER:       return "TIP_POINTER";
        case DUGUM_TIP_DIZI:          return "TIP_DIZI";
        case DUGUM_TIP_SECIMLIK:      return "TIP_SECIMLIK";
        case DUGUM_TIP_SONUC:         return "TIP_SONUC";
        case DUGUM_TIP_ISLEV:         return "TIP_ISLEV";
        case DUGUM_TIP_KULLANICI:     return "TIP_KULLANICI";
        case DUGUM_TIP_TEKKEZ:        return "TIP_TEKKEZ";
        case DUGUM_KULLAN_IFADE:      return "KULLAN_IFADE";
        case DUGUM_IMHA_IFADE:        return "IMHA_IFADE";
        case DUGUM_DESEN_LITERAL:     return "DESEN_LITERAL";
        case DUGUM_DESEN_TANIMLAYICI: return "DESEN_TANIMLAYICI";
        case DUGUM_DESEN_YAPICI:      return "DESEN_YAPICI";
        case DUGUM_DESEN_JOKER:       return "DESEN_JOKER";
        case DUGUM_ESLES_KOLU:        return "ESLES_KOLU";
        case DUGUM_HATA:              return "HATA";
    }
    return "BILINMEYEN";
}

const char *operator_adi(Operator op) {
    switch (op) {
        case OP_ARTI:          return "+";
        case OP_EKSI:          return "-";
        case OP_CARPI:         return "*";
        case OP_BOLU:          return "/";
        case OP_MOD:           return "%";
        case OP_ESIT:          return "==";
        case OP_ESIT_DEGIL:    return "!=";
        case OP_KUCUK:         return "<";
        case OP_BUYUK:         return ">";
        case OP_KUCUK_ESIT:    return "<=";
        case OP_BUYUK_ESIT:    return ">=";
        case OP_VE:            return "ve";
        case OP_VEYA:          return "veya";
        case OP_BIT_VE:        return "&";
        case OP_BIT_VEYA:      return "|";
        case OP_BIT_OZVEYA:    return "^";
        case OP_SOLA_KAYDIR:   return "<<";
        case OP_SAGA_KAYDIR:   return ">>";
        case OP_NEG:           return "neg";
        case OP_DEGIL:         return "degil";
        case OP_BIT_DEGIL:     return "~";
        case OP_REF:           return "&";
        case OP_REF_DEGISKEN:  return "&degisken";
        case OP_DEREFERANS:    return "deref*";
    }
    return "BILINMEYEN";
}
