#include "sembol.h"

#include <string.h>

/* === Scope === */

Scope *scope_olustur(Arena *a, ScopeKategorisi kat, Scope *parent) {
    Scope *s = (Scope *)arena_ayir_sifir(a, sizeof(Scope));
    if (!s) return NULL;
    s->kategori = kat;
    s->parent = parent;
    return s;
}

/* === Sembol ekleme === */

int sembol_ekle(Scope *s, Arena *a, const Sembol *sem) {
    if (!s || !sem || !a) return -1;

    /* Cift tanim kontrolu (sadece yerel scope) */
    if (sembol_bul_yerel(s, sem->ad, sem->ad_uzunluk) != NULL) {
        return -1;
    }

    SembolLink *link = (SembolLink *)arena_ayir(a, sizeof(SembolLink));
    if (!link) return -1;
    link->sembol = *sem;  /* shallow copy */
    link->sonraki = NULL;

    if (s->son) {
        s->son->sonraki = link;
    } else {
        s->bas = link;
    }
    s->son = link;
    s->sembol_sayisi++;
    return 0;
}

/* === Arama === */

const Sembol *sembol_bul_yerel(const Scope *s,
                                const char *ad, int ad_uzunluk) {
    if (!s || !ad) return NULL;
    for (SembolLink *l = s->bas; l; l = l->sonraki) {
        if (l->sembol.ad_uzunluk == ad_uzunluk &&
            memcmp(l->sembol.ad, ad, (size_t)ad_uzunluk) == 0) {
            return &l->sembol;
        }
    }
    return NULL;
}

const Sembol *sembol_bul(const Scope *s,
                          const char *ad, int ad_uzunluk) {
    while (s) {
        const Sembol *found = sembol_bul_yerel(s, ad, ad_uzunluk);
        if (found) return found;
        s = s->parent;
    }
    return NULL;
}

const Sembol *sembol_yapi_alani(const Sembol *yapi_sem,
                                 const char *ad, int ad_uzunluk) {
    if (!yapi_sem || yapi_sem->kategori != SEMBOL_YAPI) return NULL;
    if (!yapi_sem->yapi_scope) return NULL;
    /* Sadece yapi scope'unda ara — parent'a (global) gecme */
    return sembol_bul_yerel(yapi_sem->yapi_scope, ad, ad_uzunluk);
}

/* === Yardimcilar === */

const char *sembol_kategorisi_adi(SembolKategorisi k) {
    switch (k) {
        case SEMBOL_DEGISKEN:      return "DEGISKEN";
        case SEMBOL_SABIT:         return "SABIT";
        case SEMBOL_PARAMETRE:     return "PARAMETRE";
        case SEMBOL_ISLEV:         return "ISLEV";
        case SEMBOL_YAPI:          return "YAPI";
        case SEMBOL_OZELLIK:       return "OZELLIK";
        case SEMBOL_MODUL:         return "MODUL";
        case SEMBOL_GENERIC_PARAM: return "GENERIC_PARAM";
    }
    return "BILINMEYEN";
}

/* === Uygula tablosu === */

void uygula_tablosu_baslat(UygulaTablosu *t) {
    if (!t) return;
    t->bas = NULL;
    t->son = NULL;
    t->sayi = 0;
}

void uygula_tablosu_ekle(UygulaTablosu *t, Arena *a,
                         const char *tip_adi, int tip_uz,
                         const char *ozellik_adi, int ozellik_uz,
                         const Dugum *ast_dugumu) {
    if (!t || !a) return;
    UygulaKaydi *k = (UygulaKaydi *)arena_ayir_sifir(a, sizeof(UygulaKaydi));
    if (!k) return;
    k->tip_adi = tip_adi;
    k->tip_ad_uz = tip_uz;
    k->ozellik_adi = ozellik_adi;
    k->ozellik_ad_uz = ozellik_uz;
    k->ast_dugumu = ast_dugumu;
    k->sonraki = NULL;
    if (t->son) t->son->sonraki = k;
    else t->bas = k;
    t->son = k;
    t->sayi++;
}

int uygula_tablosu_implementations_eder(const UygulaTablosu *t,
                                         const char *tip_adi, int tip_uz,
                                         const char *ozellik_adi, int ozellik_uz) {
    if (!t || !tip_adi || tip_uz <= 0) return 0;
    for (const UygulaKaydi *k = t->bas; k; k = k->sonraki) {
        if (k->tip_ad_uz != tip_uz) continue;
        if (memcmp(k->tip_adi, tip_adi, (size_t)tip_uz) != 0) continue;
        /* Inherent isteniyor mu? (ozellik_adi NULL veya uz 0) */
        if (!ozellik_adi || ozellik_uz <= 0) {
            if (!k->ozellik_adi || k->ozellik_ad_uz <= 0) return 1;
            continue;
        }
        if (!k->ozellik_adi || k->ozellik_ad_uz != ozellik_uz) continue;
        if (memcmp(k->ozellik_adi, ozellik_adi, (size_t)ozellik_uz) == 0) {
            return 1;
        }
    }
    return 0;
}

const char *scope_kategorisi_adi(ScopeKategorisi k) {
    switch (k) {
        case SCOPE_GLOBAL: return "GLOBAL";
        case SCOPE_MODUL:  return "MODUL";
        case SCOPE_ISLEV:  return "ISLEV";
        case SCOPE_BLOK:   return "BLOK";
        case SCOPE_YAPI:   return "YAPI";
    }
    return "BILINMEYEN";
}
