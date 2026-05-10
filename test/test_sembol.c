#include "sembol.h"
#include "tip.h"
#include "arena.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* === Test cercevesi === */

static int toplam_test = 0;
static int basarili = 0;
static int basarisiz = 0;

static void test_sonuc(const char *ad, int durum) {
    toplam_test++;
    if (durum) {
        basarili++;
        printf("  [%d] %s ... \xe2\x9c\x93\n", toplam_test, ad);
    } else {
        basarisiz++;
        printf("  [%d] %s ... \xe2\x9c\x97\n", toplam_test, ad);
    }
}

/* === Yardimci: basit sembol olustur === */

static Sembol mk_sembol(const char *ad, SembolKategorisi kat, TipBilgisi *tip) {
    Sembol s;
    memset(&s, 0, sizeof(s));
    s.ad = ad;
    s.ad_uzunluk = (int)strlen(ad);
    s.kategori = kat;
    s.tip = tip;
    s.satir = 1;
    s.sutun = 1;
    return s;
}

/* === Scope olusturma === */

static void test_scope_global(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_olustur(a, SCOPE_GLOBAL, NULL);
    int ok = s && s->kategori == SCOPE_GLOBAL
          && s->parent == NULL
          && s->sembol_sayisi == 0;
    test_sonuc("scope_olustur(GLOBAL)", ok);
    arena_serbest(a);
}

static void test_scope_ic_ice(void) {
    Arena *a = arena_olustur(0);
    Scope *g = scope_olustur(a, SCOPE_GLOBAL, NULL);
    Scope *i = scope_olustur(a, SCOPE_ISLEV, g);
    Scope *b = scope_olustur(a, SCOPE_BLOK, i);
    int ok = g && i && b
          && b->parent == i
          && i->parent == g
          && g->parent == NULL;
    test_sonuc("scope ic ice (global -> islev -> blok)", ok);
    arena_serbest(a);
}

/* === Sembol ekleme === */

static void test_sembol_ekle_basit(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_olustur(a, SCOPE_GLOBAL, NULL);
    TipBilgisi *t = tip_olustur_basit(a, TIP_TAM32);
    Sembol sem = mk_sembol("x", SEMBOL_DEGISKEN, t);
    int rc = sembol_ekle(s, a, &sem);
    int ok = rc == 0 && s->sembol_sayisi == 1;
    test_sonuc("sembol_ekle basit", ok);
    arena_serbest(a);
}

static void test_sembol_ekle_birden_fazla(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_olustur(a, SCOPE_GLOBAL, NULL);
    TipBilgisi *t = tip_olustur_basit(a, TIP_TAM32);
    Sembol s1 = mk_sembol("x", SEMBOL_DEGISKEN, t);
    Sembol s2 = mk_sembol("y", SEMBOL_DEGISKEN, t);
    Sembol s3 = mk_sembol("z", SEMBOL_DEGISKEN, t);
    int ok = (sembol_ekle(s, a, &s1) == 0)
          && (sembol_ekle(s, a, &s2) == 0)
          && (sembol_ekle(s, a, &s3) == 0)
          && s->sembol_sayisi == 3;
    test_sonuc("sembol ekle 3 farkli", ok);
    arena_serbest(a);
}

static void test_sembol_cift_tanim(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_olustur(a, SCOPE_GLOBAL, NULL);
    TipBilgisi *t = tip_olustur_basit(a, TIP_TAM32);
    Sembol s1 = mk_sembol("x", SEMBOL_DEGISKEN, t);
    int rc1 = sembol_ekle(s, a, &s1);
    int rc2 = sembol_ekle(s, a, &s1);  /* tekrar */
    int ok = rc1 == 0 && rc2 == -1 && s->sembol_sayisi == 1;
    test_sonuc("sembol cift tanim algilar (-1 doner)", ok);
    arena_serbest(a);
}

/* === Sembol arama === */

static void test_sembol_bul_yerel(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_olustur(a, SCOPE_GLOBAL, NULL);
    TipBilgisi *t = tip_olustur_basit(a, TIP_TAM32);
    Sembol sem = mk_sembol("yas", SEMBOL_DEGISKEN, t);
    sembol_ekle(s, a, &sem);

    const Sembol *bulundu = sembol_bul_yerel(s, "yas", 3);
    int ok = bulundu != NULL
          && bulundu->ad_uzunluk == 3
          && memcmp(bulundu->ad, "yas", 3) == 0
          && bulundu->tip->kategori == TIP_TAM32;
    test_sonuc("sembol_bul_yerel basit", ok);
    arena_serbest(a);
}

static void test_sembol_bul_yok(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_olustur(a, SCOPE_GLOBAL, NULL);
    const Sembol *yok = sembol_bul_yerel(s, "x", 1);
    test_sonuc("sembol_bul_yerel yok -> NULL", yok == NULL);
    arena_serbest(a);
}

static void test_sembol_bul_parent(void) {
    Arena *a = arena_olustur(0);
    Scope *g = scope_olustur(a, SCOPE_GLOBAL, NULL);
    Scope *i = scope_olustur(a, SCOPE_ISLEV, g);
    Scope *b = scope_olustur(a, SCOPE_BLOK, i);

    TipBilgisi *t = tip_olustur_basit(a, TIP_TAM32);
    Sembol sem = mk_sembol("global_var", SEMBOL_SABIT, t);
    sembol_ekle(g, a, &sem);

    /* Inner scope'tan global'da ara */
    const Sembol *bulundu = sembol_bul(b, "global_var", 10);
    int ok = bulundu != NULL && bulundu->kategori == SEMBOL_SABIT;
    test_sonuc("sembol_bul parent zinciri", ok);
    arena_serbest(a);
}

static void test_sembol_bul_yerel_parent_atlamaz(void) {
    Arena *a = arena_olustur(0);
    Scope *g = scope_olustur(a, SCOPE_GLOBAL, NULL);
    Scope *i = scope_olustur(a, SCOPE_ISLEV, g);

    TipBilgisi *t = tip_olustur_basit(a, TIP_TAM32);
    Sembol sem = mk_sembol("z", SEMBOL_SABIT, t);
    sembol_ekle(g, a, &sem);

    /* sembol_bul_yerel parent'a bakmamali */
    const Sembol *bulundu = sembol_bul_yerel(i, "z", 1);
    test_sonuc("sembol_bul_yerel parent'a bakmaz", bulundu == NULL);
    arena_serbest(a);
}

static void test_sembol_shadowing(void) {
    Arena *a = arena_olustur(0);
    Scope *g = scope_olustur(a, SCOPE_GLOBAL, NULL);
    Scope *i = scope_olustur(a, SCOPE_ISLEV, g);

    TipBilgisi *t32 = tip_olustur_basit(a, TIP_TAM32);
    TipBilgisi *t64 = tip_olustur_basit(a, TIP_TAM64);

    Sembol gx = mk_sembol("x", SEMBOL_DEGISKEN, t32);
    Sembol ix = mk_sembol("x", SEMBOL_DEGISKEN, t64);

    sembol_ekle(g, a, &gx);
    sembol_ekle(i, a, &ix);

    /* Inner scope'tan x ararsam inner versiyonu (tam64) bulmali */
    const Sembol *bulundu = sembol_bul(i, "x", 1);
    int ok = bulundu != NULL && bulundu->tip->kategori == TIP_TAM64;
    test_sonuc("sembol shadowing (inner gecerli)", ok);
    arena_serbest(a);
}

/* === Yapi alanlari === */

static void test_sembol_yapi_alanlari(void) {
    Arena *a = arena_olustur(0);
    Scope *g = scope_olustur(a, SCOPE_GLOBAL, NULL);

    /* yapi Hasta { ad: metin; yas: tam32; } */
    Scope *yapi_s = scope_olustur(a, SCOPE_YAPI, g);
    TipBilgisi *tm = tip_olustur_basit(a, TIP_METIN);
    TipBilgisi *t32 = tip_olustur_basit(a, TIP_TAM32);
    Sembol alan_ad = mk_sembol("ad", SEMBOL_DEGISKEN, tm);
    Sembol alan_yas = mk_sembol("yas", SEMBOL_DEGISKEN, t32);
    sembol_ekle(yapi_s, a, &alan_ad);
    sembol_ekle(yapi_s, a, &alan_yas);

    Sembol hasta = mk_sembol("Hasta", SEMBOL_YAPI, NULL);
    hasta.yapi_scope = yapi_s;
    sembol_ekle(g, a, &hasta);

    /* Erisim */
    const Sembol *h_sem = sembol_bul(g, "Hasta", 5);
    int ok = h_sem != NULL && h_sem->kategori == SEMBOL_YAPI;

    if (ok) {
        const Sembol *ad_sem = sembol_yapi_alani(h_sem, "ad", 2);
        const Sembol *yas_sem = sembol_yapi_alani(h_sem, "yas", 3);
        const Sembol *yok = sembol_yapi_alani(h_sem, "yok_alan", 8);
        ok = ad_sem != NULL && ad_sem->tip->kategori == TIP_METIN
          && yas_sem != NULL && yas_sem->tip->kategori == TIP_TAM32
          && yok == NULL;
    }
    test_sonuc("yapi alani: Hasta.ad, Hasta.yas", ok);
    arena_serbest(a);
}

/* === Modul scope === */

static void test_sembol_modul(void) {
    Arena *a = arena_olustur(0);
    Scope *g = scope_olustur(a, SCOPE_GLOBAL, NULL);
    Scope *modul_s = scope_olustur(a, SCOPE_MODUL, g);

    TipBilgisi *t = tip_olustur_basit(a, TIP_TAM32);
    Sembol m_sembol = mk_sembol("matematik", SEMBOL_MODUL, NULL);
    m_sembol.modul_scope = modul_s;
    sembol_ekle(g, a, &m_sembol);

    Sembol pi = mk_sembol("PI", SEMBOL_SABIT, t);
    sembol_ekle(modul_s, a, &pi);

    const Sembol *m = sembol_bul(g, "matematik", 9);
    int ok = m != NULL && m->kategori == SEMBOL_MODUL && m->modul_scope == modul_s;
    if (ok) {
        const Sembol *pi_sem = sembol_bul_yerel(m->modul_scope, "PI", 2);
        ok = pi_sem != NULL && pi_sem->kategori == SEMBOL_SABIT;
    }
    test_sonuc("modul: matematik::PI erisimi", ok);
    arena_serbest(a);
}

/* === Generic param === */

static void test_sembol_generic_param(void) {
    Arena *a = arena_olustur(0);
    /* yapi Kutu<T> { ... } — T generic param scope'ta */
    Scope *g = scope_olustur(a, SCOPE_GLOBAL, NULL);
    Scope *yapi_s = scope_olustur(a, SCOPE_YAPI, g);

    TipBilgisi *T = tip_olustur_generic_param(a, "T", 1);
    Sembol gp = mk_sembol("T", SEMBOL_GENERIC_PARAM, T);
    sembol_ekle(yapi_s, a, &gp);

    const Sembol *t_sem = sembol_bul(yapi_s, "T", 1);
    int ok = t_sem != NULL
          && t_sem->kategori == SEMBOL_GENERIC_PARAM
          && t_sem->tip != NULL
          && t_sem->tip->kategori == TIP_GENERIC_PARAM;
    test_sonuc("generic param: T scope'ta bulunur", ok);
    arena_serbest(a);
}

/* === Cesitli kategoriler === */

static void test_sembol_kategoriler(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_olustur(a, SCOPE_GLOBAL, NULL);

    Sembol semler[] = {
        mk_sembol("v", SEMBOL_DEGISKEN, NULL),
        mk_sembol("c", SEMBOL_SABIT, NULL),
        mk_sembol("p", SEMBOL_PARAMETRE, NULL),
        mk_sembol("f", SEMBOL_ISLEV, NULL),
        mk_sembol("Y", SEMBOL_YAPI, NULL),
        mk_sembol("O", SEMBOL_OZELLIK, NULL),
    };
    int ok = 1;
    for (int i = 0; i < 6; i++) {
        if (sembol_ekle(s, a, &semler[i]) != 0) { ok = 0; break; }
    }
    if (ok) {
        for (int i = 0; i < 6; i++) {
            const Sembol *bulundu = sembol_bul_yerel(s,
                semler[i].ad, semler[i].ad_uzunluk);
            if (!bulundu || bulundu->kategori != semler[i].kategori) {
                ok = 0; break;
            }
        }
    }
    test_sonuc("6 farkli kategori sembol kayit/erisim", ok);
    arena_serbest(a);
}

static void test_sembol_kategorisi_adi(void) {
    int ok = (strcmp(sembol_kategorisi_adi(SEMBOL_DEGISKEN), "DEGISKEN") == 0)
          && (strcmp(sembol_kategorisi_adi(SEMBOL_ISLEV),    "ISLEV") == 0)
          && (strcmp(sembol_kategorisi_adi(SEMBOL_YAPI),     "YAPI") == 0)
          && (strcmp(scope_kategorisi_adi(SCOPE_GLOBAL),    "GLOBAL") == 0)
          && (strcmp(scope_kategorisi_adi(SCOPE_BLOK),      "BLOK") == 0);
    test_sonuc("kategori adlari", ok);
}

/* === Turkce sembol adlari === */

static void test_sembol_turkce_ad(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_olustur(a, SCOPE_GLOBAL, NULL);

    /* "yas_artisi" UTF-8 — keyword olmayan tanimlayici */
    const char *ad = "yas_arti\xc5\x9f""i";  /* yas_artişi */
    int ad_uz = (int)strlen(ad);

    TipBilgisi *t = tip_olustur_basit(a, TIP_TAM32);
    Sembol sem;
    memset(&sem, 0, sizeof(sem));
    sem.ad = ad;
    sem.ad_uzunluk = ad_uz;
    sem.kategori = SEMBOL_DEGISKEN;
    sem.tip = t;
    sembol_ekle(s, a, &sem);

    const Sembol *bulundu = sembol_bul(s, ad, ad_uz);
    int ok = bulundu != NULL && bulundu->ad_uzunluk == ad_uz
          && memcmp(bulundu->ad, ad, (size_t)ad_uz) == 0;
    test_sonuc("Turkce UTF-8 sembol adi", ok);
    arena_serbest(a);
}

/* === NULL guvenlik === */

static void test_null_guvenlik(void) {
    int ok1 = (sembol_bul(NULL, "x", 1) == NULL);
    int ok2 = (sembol_bul_yerel(NULL, "x", 1) == NULL);
    int ok3 = (sembol_yapi_alani(NULL, "x", 1) == NULL);
    int ok4 = (sembol_ekle(NULL, NULL, NULL) == -1);
    test_sonuc("NULL guvenlik (4 fonksiyon)", ok1 && ok2 && ok3 && ok4);
}

/* === Stres === */

static void test_stres_1000_sembol(void) {
    Arena *a = arena_olustur(0);
    Scope *s = scope_olustur(a, SCOPE_GLOBAL, NULL);
    TipBilgisi *t = tip_olustur_basit(a, TIP_TAM32);

    /* 1000 farkli isimli sembol — arena'da string + kayit */
    int ok = 1;
    char isim_buf[16];
    for (int i = 0; i < 1000; i++) {
        snprintf(isim_buf, sizeof(isim_buf), "v%d", i);
        int uz = (int)strlen(isim_buf);
        char *kalici = (char *)arena_ayir(a, (size_t)uz + 1);
        if (!kalici) { ok = 0; break; }
        memcpy(kalici, isim_buf, (size_t)uz + 1);

        Sembol sem;
        memset(&sem, 0, sizeof(sem));
        sem.ad = kalici;
        sem.ad_uzunluk = uz;
        sem.kategori = SEMBOL_DEGISKEN;
        sem.tip = t;

        if (sembol_ekle(s, a, &sem) != 0) { ok = 0; break; }
    }
    if (ok) ok = (s->sembol_sayisi == 1000);
    test_sonuc("1000 sembol ekle (ASan sizinti yok)", ok);
    arena_serbest(a);
}

/* === Main === */

int main(void) {
    printf("KEMGU Sembol Tablosu Test Paketi\n");
    printf("=================================\n");

    printf("\n--- Scope Olusturma ---\n");
    test_scope_global();
    test_scope_ic_ice();

    printf("\n--- Sembol Ekleme ---\n");
    test_sembol_ekle_basit();
    test_sembol_ekle_birden_fazla();
    test_sembol_cift_tanim();

    printf("\n--- Arama ---\n");
    test_sembol_bul_yerel();
    test_sembol_bul_yok();
    test_sembol_bul_parent();
    test_sembol_bul_yerel_parent_atlamaz();
    test_sembol_shadowing();

    printf("\n--- Yapi Alanlari ---\n");
    test_sembol_yapi_alanlari();

    printf("\n--- Modul ---\n");
    test_sembol_modul();

    printf("\n--- Generic Param ---\n");
    test_sembol_generic_param();

    printf("\n--- Kategoriler ---\n");
    test_sembol_kategoriler();
    test_sembol_kategorisi_adi();

    printf("\n--- UTF-8 ---\n");
    test_sembol_turkce_ad();

    printf("\n--- NULL Guvenlik ---\n");
    test_null_guvenlik();

    printf("\n--- Stres ---\n");
    test_stres_1000_sembol();

    printf("\n=================================\n");
    printf("Toplam: %d | Basarili: %d | Basarisiz: %d\n",
           toplam_test, basarili, basarisiz);
    return basarisiz > 0 ? 1 : 0;
}
