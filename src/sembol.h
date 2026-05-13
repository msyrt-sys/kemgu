#ifndef KEMGU_SEMBOL_H
#define KEMGU_SEMBOL_H

#include "arena.h"
#include "tip.h"
#include "ast.h"

#include <stddef.h>

/*
 * KEMGU Sembol Tablosu
 * ====================
 *
 * Scope hiyerarsisi: parent pointer'li linked list.
 *   GLOBAL -> MODUL -> ISLEV -> BLOK -> BLOK ...
 *
 * Her scope'ta sembol listesi (linked list — parser pattern'iyle uyumlu).
 *
 * Isim cozumlemesi: current -> parent -> ... -> GLOBAL.
 * 'sembol_bul' tum hiyerarsi tarar.
 * 'sembol_bul_yerel' sadece o scope'a bakar.
 *
 * Cift tanim algilamasi: 'sembol_ekle' ayni adi yerel scope'ta tanimliysa
 * -1 doner. Caller hata raporlamali.
 */

/* === Sembol kategorileri === */

typedef enum {
    SEMBOL_DEGISKEN,      /* yerel degisken */
    SEMBOL_SABIT,         /* sabit */
    SEMBOL_PARAMETRE,     /* islev parametresi */
    SEMBOL_ISLEV,         /* islev tanimi */
    SEMBOL_YAPI,          /* yapi tanimi */
    SEMBOL_OZELLIK,       /* ozellik (trait) tanimi */
    SEMBOL_MODUL,         /* modul (kendi scope'una sahip) */
    SEMBOL_GENERIC_PARAM, /* generic tip parametresi */
} SembolKategorisi;

/* === Sembol === */

typedef struct Sembol {
    const char *ad;                /* arena'da null-terminated kopya */
    int ad_uzunluk;
    SembolKategorisi kategori;
    TipBilgisi *tip;               /* sembolun tip bilgisi (NULL olabilir) */
    const Dugum *ast_dugumu;       /* tanim AST'sine geri pointer (debug/hata) */
    int satir;                     /* tanim konumu */
    int sutun;
    /* Yapi sembolu icin: alanlar (kendi mini-scope'u olarak) */
    struct Scope *yapi_scope;      /* sadece SEMBOL_YAPI icin, alanlar burada */
    /* Modul sembolu icin: modulun ic scope'u */
    struct Scope *modul_scope;     /* sadece SEMBOL_MODUL icin */

    /* Linear Types Spec V1: lineer baglamalar icin tuketim takibi.
     * tip TIP_TEKKEZ ise kullanim sayisi takip edilir; > 1 = L002,
     * scope sonunda 0 = L001. */
    int lineer_tuketildi;          /* 0 = henuz tuketilmedi; 1+ = tuketim sayisi */
    int lineer_scope_seviyesi;     /* tanim aninda scope derinligi */
} Sembol;

/* === Scope === */

typedef enum {
    SCOPE_GLOBAL,
    SCOPE_MODUL,
    SCOPE_ISLEV,
    SCOPE_BLOK,
    SCOPE_YAPI,           /* yapi icin generic param + alan scope */
} ScopeKategorisi;

typedef struct SembolLink {
    Sembol sembol;
    struct SembolLink *sonraki;
} SembolLink;

typedef struct Scope {
    ScopeKategorisi kategori;
    struct Scope *parent;          /* NULL = global */
    SembolLink *bas;               /* sembol listesinin basi */
    SembolLink *son;               /* sondaki link (hizli ekleme icin) */
    int sembol_sayisi;
} Scope;

/* === API === */

/* Yeni scope yarat. parent NULL = global. */
Scope *scope_olustur(Arena *a, ScopeKategorisi kat, Scope *parent);

/* Sembol ekle. Ayni isim yerel scope'ta varsa -1 doner (caller hata
 * raporlar). Aksi halde 0. Sembol arena'ya kopyalanir. */
int sembol_ekle(Scope *s, Arena *a, const Sembol *sem);

/* Sembol ara — parent zinciri dahil. Bulunmazsa NULL. */
const Sembol *sembol_bul(const Scope *s, const char *ad, int ad_uzunluk);

/* Sembol ara — sadece bu scope (parent'a bakmaz). */
const Sembol *sembol_bul_yerel(const Scope *s,
                                const char *ad, int ad_uzunluk);

/* Linear Types Spec V1: lineer tuketim isaretleme icin mutable Sembol*
 * alma. sembol_bul ile ayni algoritma fakat const olmayan pointer doner.
 * Lineer_tuketildi alanini guncellemek icin gerekli. */
Sembol *sembol_bul_yazilabilir(Scope *s, const char *ad, int ad_uzunluk);

/* Yapi alanini ara (yapi sembolunun yapi_scope'u icinde). */
const Sembol *sembol_yapi_alani(const Sembol *yapi_sem,
                                 const char *ad, int ad_uzunluk);

/* === Uygula (impl) kayit defteri ===
 *
 * Her 'uygula Trait icin Tip { ... }' veya 'uygula Tip { ... }' bildirimi
 * burada kayit edilir. ozellik_adi NULL/0 ise inherent (trait olmadan) impl.
 *
 * Sorgu: uygula_implementations_eder(tablo, tip_adi, ozellik_adi) -> 1/0
 */

typedef struct UygulaKaydi {
    const char *tip_adi;
    int tip_ad_uz;
    const char *ozellik_adi;       /* NULL veya 0 uz = inherent */
    int ozellik_ad_uz;
    const Dugum *ast_dugumu;
    struct UygulaKaydi *sonraki;
} UygulaKaydi;

typedef struct UygulaTablosu {
    UygulaKaydi *bas;
    UygulaKaydi *son;
    int sayi;
} UygulaTablosu;

void uygula_tablosu_baslat(UygulaTablosu *t);

/* Kayit ekle. ozellik_adi NULL/0 -> inherent. */
void uygula_tablosu_ekle(UygulaTablosu *t, Arena *a,
                         const char *tip_adi, int tip_uz,
                         const char *ozellik_adi, int ozellik_uz,
                         const Dugum *ast_dugumu);

/* Sorgu: tip 'tip_adi' ozellik 'ozellik_adi' implement ediyor mu? */
int uygula_tablosu_implementations_eder(const UygulaTablosu *t,
                                         const char *tip_adi, int tip_uz,
                                         const char *ozellik_adi, int ozellik_uz);

/* Method arama: tip 'tip_adi' icin 'metot_adi' adli islev bulundugunda
 * AST islev dugumunu doner (DUGUM_ISLEV). Inherent VE trait impl'lere bakar.
 * Bulunamazsa NULL. */
const Dugum *uygula_tablosu_method_bul(const UygulaTablosu *t,
                                        const char *tip_adi, int tip_uz,
                                        const char *metot_adi, int metot_uz);

/* === Yardimci === */

const char *sembol_kategorisi_adi(SembolKategorisi k);
const char *scope_kategorisi_adi(ScopeKategorisi k);

#endif /* KEMGU_SEMBOL_H */
