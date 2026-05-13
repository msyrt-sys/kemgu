#include "escape.h"

#include <stdlib.h>
#include <string.h>

/* === Dahili kapasite yonetimi ===
 *
 * Kayit ve baglama dizileri arena'da degil, malloc/realloc ile yonetilir.
 * Sebep: fixed-point sirasinda kapasite ihtiyaci dinamik olarak buyur,
 * arena yalnizca ileriye dogru tahsis eder (realloc desteklemez).
 * escape_baslat -> escape_analiz_program -> escape_serbest (acik degil:
 * arena kapaninca biten tum kullanim; bu dosya sadece icerden serbest
 * birakir cunku EscapeAnaliz icin acik bir kapatma fonksiyonu kullanan
 * baslat akiyor — sonunda atexit gibi degil, kullanici test/ana tarafinda
 * scope sonunda serbest birakir).
 *
 * Programcida problemli olmaması icin: islem bitince free() cagrilmali.
 * Test ASan ile dogrulanir.
 */

#define KAYIT_BASLANGIC_KAP 16
#define BAG_BASLANGIC_KAP   16

void escape_baslat(EscapeAnaliz *ea, Arena *a) {
    ea->arena = a;
    ea->kayitlar = NULL;
    ea->kayit_sayi = 0;
    ea->kayit_kapasite = 0;
    ea->baglamalar = NULL;
    ea->bag_sayi = 0;
    ea->bag_kapasite = 0;
    ea->scope_seviye = 0;
    ea->dongu_derinligi = 0;
    ea->ver_baglaminda = 0;
    ea->degisti = 0;
}

void escape_serbest(EscapeAnaliz *ea) {
    if (!ea) return;
    free(ea->kayitlar);
    free(ea->baglamalar);
    ea->kayitlar = NULL;
    ea->baglamalar = NULL;
    ea->kayit_sayi = ea->kayit_kapasite = 0;
    ea->bag_sayi = ea->bag_kapasite = 0;
}

/* === Dahili: kayit yonetimi === */

static EscapeKayit *kayit_bul(EscapeAnaliz *ea, const Dugum *d) {
    for (int i = 0; i < ea->kayit_sayi; i++) {
        if (ea->kayitlar[i].dugum == d) return &ea->kayitlar[i];
    }
    return NULL;
}

static EscapeKayit *kayit_ekle(EscapeAnaliz *ea, const Dugum *d) {
    if (ea->kayit_sayi == ea->kayit_kapasite) {
        int yeni_kap = ea->kayit_kapasite == 0 ? KAYIT_BASLANGIC_KAP : ea->kayit_kapasite * 2;
        EscapeKayit *yeni = realloc(ea->kayitlar, (size_t)yeni_kap * sizeof(EscapeKayit));
        if (!yeni) return NULL;
        ea->kayitlar = yeni;
        ea->kayit_kapasite = yeni_kap;
    }
    EscapeKayit *k = &ea->kayitlar[ea->kayit_sayi++];
    k->dugum = d;
    k->kategori = ESC_YEREL;
    k->dongu_derinligi = ea->dongu_derinligi;
    return k;
}

static EscapeKayit *kayit_bul_veya_ekle(EscapeAnaliz *ea, const Dugum *d) {
    EscapeKayit *k = kayit_bul(ea, d);
    if (k) return k;
    return kayit_ekle(ea, d);
}

/* Bir tahsisi en az verilen kategoriye yukselt. Degisiklik olursa degisti=1. */
static void escape_yukselt(EscapeAnaliz *ea, const Dugum *d, EscapeKategorisi yeni) {
    if (!d) return;
    EscapeKayit *k = kayit_bul_veya_ekle(ea, d);
    if (!k) return;
    if ((int)yeni > (int)k->kategori) {
        k->kategori = yeni;
        ea->degisti = 1;
    }
}

/* === Dahili: degisken baglama yonetimi === */

static void bag_ekle(EscapeAnaliz *ea, const char *ad, int ad_uz, const Dugum *deger) {
    if (ea->bag_sayi == ea->bag_kapasite) {
        int yeni_kap = ea->bag_kapasite == 0 ? BAG_BASLANGIC_KAP : ea->bag_kapasite * 2;
        EscapeBag *yeni = realloc(ea->baglamalar, (size_t)yeni_kap * sizeof(EscapeBag));
        if (!yeni) return;
        ea->baglamalar = yeni;
        ea->bag_kapasite = yeni_kap;
    }
    EscapeBag *b = &ea->baglamalar[ea->bag_sayi++];
    b->ad = ad;
    b->ad_uz = ad_uz;
    b->deger = deger;
    b->scope_seviye = ea->scope_seviye;
}

static const Dugum *bag_cozumle(EscapeAnaliz *ea, const char *ad, int ad_uz) {
    /* Sondan basa: en yakin scope'taki bag */
    for (int i = ea->bag_sayi - 1; i >= 0; i--) {
        if (ea->baglamalar[i].ad_uz == ad_uz
            && memcmp(ea->baglamalar[i].ad, ad, (size_t)ad_uz) == 0) {
            return ea->baglamalar[i].deger;
        }
    }
    return NULL;
}

/* Atama: var = e ise mevcut bagin degerini guncelle (sadece en yakin esleme). */
static void bag_guncelle(EscapeAnaliz *ea, const char *ad, int ad_uz, const Dugum *yeni) {
    for (int i = ea->bag_sayi - 1; i >= 0; i--) {
        if (ea->baglamalar[i].ad_uz == ad_uz
            && memcmp(ea->baglamalar[i].ad, ad, (size_t)ad_uz) == 0) {
            ea->baglamalar[i].deger = yeni;
            return;
        }
    }
    /* Yoksa yeni bag olarak ekle (lvalue yeni ise) */
    bag_ekle(ea, ad, ad_uz, yeni);
}

static void scope_gir(EscapeAnaliz *ea) {
    ea->scope_seviye++;
}

static void scope_cik(EscapeAnaliz *ea) {
    int sev = ea->scope_seviye;
    while (ea->bag_sayi > 0 && ea->baglamalar[ea->bag_sayi - 1].scope_seviye >= sev) {
        ea->bag_sayi--;
    }
    ea->scope_seviye--;
}

/* Ifade icindeki tum tahsisleri belirli kategoriye yukselt (kosullu dallar icin).
 * Recursive: eger ifadesinde her iki dali da islemek icin. */
static void ifadeyi_yukselt(EscapeAnaliz *ea, const Dugum *ifade, EscapeKategorisi yeni);

static void ifadeyi_yukselt(EscapeAnaliz *ea, const Dugum *ifade, EscapeKategorisi yeni) {
    if (!ifade) return;
    switch (ifade->tip) {
        case DUGUM_METIN:
        case DUGUM_DIZI_OLUSTUR:
        case DUGUM_YAPI_OLUSTUR:
        case DUGUM_LAMBDA:
        case DUGUM_CAGRI:
            escape_yukselt(ea, ifade, yeni);
            return;
        case DUGUM_TANIMLAYICI: {
            const Dugum *bag = bag_cozumle(ea,
                ifade->veri.tanimlayici.metin,
                ifade->veri.tanimlayici.uzunluk);
            if (bag) ifadeyi_yukselt(ea, bag, yeni);
            return;
        }
        case DUGUM_EGER:
            ifadeyi_yukselt(ea, ifade->veri.eger.gozdoldur, yeni);
            ifadeyi_yukselt(ea, ifade->veri.eger.yan, yeni);
            return;
        case DUGUM_BLOK: {
            int n = ifade->veri.blok.sayi;
            if (n == 0) return;
            /* Sadece son deyim — degeri o doner. Diger deyimlerdeki tahsisler ayri analiz edilir. */
            ifadeyi_yukselt(ea, ifade->veri.blok.deyimler[n - 1], yeni);
            return;
        }
        case DUGUM_ERISIM:
            ifadeyi_yukselt(ea, ifade->veri.erisim.nesne, yeni);
            return;
        case DUGUM_INDEKS:
            ifadeyi_yukselt(ea, ifade->veri.indeks.nesne, yeni);
            return;
        case DUGUM_TEKLI:
            if (ifade->veri.tekli.op == OP_REF
                || ifade->veri.tekli.op == OP_REF_DEGISKEN
                || ifade->veri.tekli.op == OP_DEREFERANS) {
                ifadeyi_yukselt(ea, ifade->veri.tekli.operand, yeni);
            }
            return;
        case DUGUM_IFADE_DEYIMI:
            ifadeyi_yukselt(ea, ifade->veri.ifade_deyimi.ifade, yeni);
            return;
        default:
            return;
    }
}

/* === Visitor ===
 *
 * AST'yi rekursif gez. Her tahsisi kaydet, atamayi propaganda, ver'i isle. */

static void visit(EscapeAnaliz *ea, const Dugum *d);

static void visit_list(EscapeAnaliz *ea, Dugum **liste, int sayi) {
    for (int i = 0; i < sayi; i++) visit(ea, liste[i]);
}

static void visit(EscapeAnaliz *ea, const Dugum *d) {
    if (!d) return;

    switch (d->tip) {
        /* === Tahsis literalleri: kayit olustur === */
        case DUGUM_METIN:
        case DUGUM_DIZI_OLUSTUR:
        case DUGUM_YAPI_OLUSTUR:
        case DUGUM_LAMBDA: {
            EscapeKayit *k = kayit_bul_veya_ekle(ea, d);
            if (k && ea->dongu_derinligi > 0 && k->kategori == ESC_YEREL) {
                /* Dongu icinde olusturulan tahsis: ITERASYON (escape ekleme degisikligi sayilir) */
                if ((int)ESC_ITERASYON > (int)k->kategori) {
                    k->kategori = ESC_ITERASYON;
                    ea->degisti = 1;
                }
            }
            /* Recurse alt-ifadelere */
            if (d->tip == DUGUM_DIZI_OLUSTUR) {
                visit_list(ea, d->veri.dizi_olustur.elemanlar, d->veri.dizi_olustur.sayi);
            } else if (d->tip == DUGUM_YAPI_OLUSTUR) {
                for (int i = 0; i < d->veri.yapi_olustur.alan_sayi; i++) {
                    Dugum *aa = d->veri.yapi_olustur.alanlar[i];
                    if (aa && aa->tip == DUGUM_ALAN_ATAMA) {
                        visit(ea, aa->veri.alan_atama.deger);
                    }
                }
            } else if (d->tip == DUGUM_LAMBDA) {
                scope_gir(ea);
                visit(ea, d->veri.lambda.govde);
                scope_cik(ea);
            }
            return;
        }

        /* === Cagri sonucu: konservatif tahsis === */
        case DUGUM_CAGRI:
            kayit_bul_veya_ekle(ea, d);
            visit(ea, d->veri.cagri.hedef);
            visit_list(ea, d->veri.cagri.argumanlar, d->veri.cagri.sayi);
            return;

        /* === Ver: escape kaynagi === */
        case DUGUM_VER: {
            int eski = ea->ver_baglaminda;
            ea->ver_baglaminda = 1;
            visit(ea, d->veri.ver.deger);
            ea->ver_baglaminda = eski;
            /* Altta yatan tum tahsisleri CAGIRAN'a yukselt (kosullu dallar dahil) */
            ifadeyi_yukselt(ea, d->veri.ver.deger, ESC_CAGIRAN);
            return;
        }

        /* === Degisken tanimi: bag olustur === */
        case DUGUM_DEGISKEN: {
            visit(ea, d->veri.degisken.deger);
            if (d->veri.degisken.ad && d->veri.degisken.ad_uzunluk > 0) {
                bag_ekle(ea, d->veri.degisken.ad,
                         d->veri.degisken.ad_uzunluk,
                         d->veri.degisken.deger);
            }
            return;
        }

        /* === Atama: bagi guncelle (append-only — MAY-analysis icin)
         *
         * Atama propagation YOK: x = e'de eski tahsisin kategorisini yeniye
         * tasimak yanlistir (eski escape ettiyse bile yeni atanan o noktadan
         * sonra kullanilmayabilir). MAY-flow icin ek bag ekliyoruz:
         * sonraki kullanim x'i hem eski hem yeni tahsise baglayabilir
         * (en yakin bag once bulunur, ama fixed-point bunu cozecek).
         */
        case DUGUM_ATAMA: {
            visit(ea, d->veri.atama.deger);
            visit(ea, d->veri.atama.hedef);
            if (d->veri.atama.hedef
                && d->veri.atama.hedef->tip == DUGUM_TANIMLAYICI) {
                bag_guncelle(ea,
                    d->veri.atama.hedef->veri.tanimlayici.metin,
                    d->veri.atama.hedef->veri.tanimlayici.uzunluk,
                    d->veri.atama.deger);
            }
            return;
        }

        /* === Bloklar/Scope === */
        case DUGUM_BLOK:
            scope_gir(ea);
            visit_list(ea, d->veri.blok.deyimler, d->veri.blok.sayi);
            scope_cik(ea);
            return;

        case DUGUM_IFADE_DEYIMI:
            visit(ea, d->veri.ifade_deyimi.ifade);
            return;

        /* === Kontrol akisi === */
        case DUGUM_EGER:
            visit(ea, d->veri.eger.kosul);
            visit(ea, d->veri.eger.gozdoldur);
            visit(ea, d->veri.eger.yan);
            return;

        case DUGUM_IKEN:
            visit(ea, d->veri.iken.kosul);
            ea->dongu_derinligi++;
            visit(ea, d->veri.iken.govde);
            ea->dongu_derinligi--;
            return;

        case DUGUM_ICIN:
            visit(ea, d->veri.icin.koleksiyon);
            ea->dongu_derinligi++;
            scope_gir(ea);
            /* Dongu degiskeni: koleksiyonun elemanlarina baglanmis kabul edilir.
             * Direkt bir bag eklenmiyor — kullanildiginda alt_tahsis NULL doner,
             * koleksiyonun alt-tahsisi ayri kayit alir. */
            visit(ea, d->veri.icin.govde);
            scope_cik(ea);
            ea->dongu_derinligi--;
            return;

        case DUGUM_ESLES:
            visit(ea, d->veri.esles.deger);
            for (int i = 0; i < d->veri.esles.kol_sayi; i++) {
                Dugum *kol = d->veri.esles.kollar[i];
                if (kol && kol->tip == DUGUM_ESLES_KOLU) {
                    visit(ea, kol->veri.esles_kolu.govde);
                }
            }
            return;

        case DUGUM_GUVENSIZ:
            visit(ea, d->veri.guvensiz.blok);
            return;

        /* === Ifadeler === */
        case DUGUM_IKILI:
            visit(ea, d->veri.ikili.sol);
            visit(ea, d->veri.ikili.sag);
            return;

        case DUGUM_TEKLI:
            visit(ea, d->veri.tekli.operand);
            return;

        case DUGUM_ERISIM:
            visit(ea, d->veri.erisim.nesne);
            return;

        case DUGUM_INDEKS:
            visit(ea, d->veri.indeks.nesne);
            visit(ea, d->veri.indeks.indeks);
            return;

        case DUGUM_YOL:
            /* Modul uyeleri: global — escape kaynagi degil */
            return;

        /* === Ust duzey === */
        case DUGUM_PROGRAM:
            for (int i = 0; i < d->veri.program.sayi; i++) {
                visit(ea, d->veri.program.uyeler[i]);
            }
            return;

        case DUGUM_MODUL:
            for (int i = 0; i < d->veri.modul.sayi; i++) {
                visit(ea, d->veri.modul.uyeler[i]);
            }
            return;

        case DUGUM_DISA:
            visit(ea, d->veri.disa.tanim);
            return;

        case DUGUM_ISLEV:
            /* Bu fonksiyon iceriden cagrilirsa, fonksiyon govdesini iliki defa
             * gezeriz. Burada sadece programi gezerken cocuk olarak inilirse atla.
             * escape_analiz_islev'i kullanici ayrica cagirir. */
            return;

        case DUGUM_SABIT:
            visit(ea, d->veri.sabit.deger);
            return;

        /* === Basit literaller: tahsis yok === */
        case DUGUM_TAM:
        case DUGUM_KESIRLI:
        case DUGUM_KARAKTER:
        case DUGUM_MANTIKSAL:
        case DUGUM_BOS:
        case DUGUM_TANIMLAYICI:
            return;

        default:
            return;
    }
}

void escape_analiz_islev(EscapeAnaliz *ea, const Dugum *islev) {
    if (!islev || islev->tip != DUGUM_ISLEV) return;
    const Dugum *govde = islev->veri.islev.govde;
    if (!govde) return;

    /* Fixed-point: tum kategori degisiklikleri durana kadar tekrar. */
    int max_iter = 16;  /* guvenlik: sonsuz dongu engelleyici (pratikte 2-3 yeter) */
    for (int iter = 0; iter < max_iter; iter++) {
        ea->degisti = 0;
        ea->bag_sayi = 0;       /* her pass'ta bag haritalari sifirlanir */
        ea->scope_seviye = 0;
        ea->ver_baglaminda = 0;
        ea->dongu_derinligi = 0;

        /* Parametreleri yapi olarak ekle — onlarin bagi yok (CAGIRAN-set olarak ele alinabilir) */
        scope_gir(ea);
        for (int i = 0; i < islev->veri.islev.param_sayi; i++) {
            Dugum *p = islev->veri.islev.parametreler[i];
            if (p && p->tip == DUGUM_PARAMETRE && p->veri.parametre.ad) {
                /* Parametre tahsisini ESC_CAGIRAN olarak ele alma ihtiyaci yok — cagiran
                 * verir; sadece kayitsiz birakiyoruz, bag_cozumle NULL doner ve dolayisi
                 * ile parametre uzerinden escape akmaz (interproc yok). */
                (void)p;  /* simdilik: parametreyi tahsis olarak gormuyoruz */
            }
        }
        visit(ea, govde);
        scope_cik(ea);

        if (!ea->degisti) break;
    }
}

void escape_analiz_program(EscapeAnaliz *ea, const Dugum *program) {
    if (!program) return;

    /* Recursive: islevleri bul (program -> modul -> disa -> islev) */
    switch (program->tip) {
        case DUGUM_PROGRAM:
            for (int i = 0; i < program->veri.program.sayi; i++) {
                escape_analiz_program(ea, program->veri.program.uyeler[i]);
            }
            return;
        case DUGUM_MODUL:
            for (int i = 0; i < program->veri.modul.sayi; i++) {
                escape_analiz_program(ea, program->veri.modul.uyeler[i]);
            }
            return;
        case DUGUM_DISA:
            escape_analiz_program(ea, program->veri.disa.tanim);
            return;
        case DUGUM_ISLEV:
            escape_analiz_islev(ea, program);
            return;
        default:
            return;
    }
}

/* === Sorgu === */

EscapeKategorisi escape_kategori(const EscapeAnaliz *ea, const Dugum *d) {
    for (int i = 0; i < ea->kayit_sayi; i++) {
        if (ea->kayitlar[i].dugum == d) return ea->kayitlar[i].kategori;
    }
    return ESC_YEREL;
}

int escape_dongu_derinligi(const EscapeAnaliz *ea, const Dugum *d) {
    for (int i = 0; i < ea->kayit_sayi; i++) {
        if (ea->kayitlar[i].dugum == d) return ea->kayitlar[i].dongu_derinligi;
    }
    return 0;
}

const char *escape_kategori_adi(EscapeKategorisi k) {
    switch (k) {
        case ESC_YEREL:     return "YEREL";
        case ESC_ITERASYON: return "ITERASYON";
        case ESC_CAGIRAN:   return "CAGIRAN";
    }
    return "BILINMEYEN";
}
