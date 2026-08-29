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


/* [D-488] Cagrilan-ozeti tablosu PROGRAM GENELINDEDIR ve ARENA-destekli:
 * tuketiciler (llvm.c / tip_kontrol.c) ISLEV BASINA taze EscapeAnaliz kurar,
 * yani tabloyu orada tutmak her isleve yeniden kurdururdu (O(F^2)). Arena
 * cagiran tarafindan serbest birakildigi icin ayrica free GEREKMEZ. */
static struct EaFnKayit *g_fn_tablo = NULL;
static int g_fn_sayi = 0;
static const Dugum *g_fn_program = NULL;

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
    k->kesin_yerel = 0;   /* F4.2b: ky_isaretle sonradan KANITLARSA 1 yapar (uninit garbage = UAF). */
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
            escape_yukselt(ea, ifade, yeni);
            return;
        /* [F4.2b SOUNDNESS — DERİN TRANSİTİF TERFİ] Bir agregat (dizi/yapı) kaçınca
         * GÖMÜLÜ heap-ref alt-tahsisleri de transitif kaçar (R-GÖMME). Eski sığ kod
         * yalnız düğümün KENDİSİNİ yükseltiyordu → `[[1,2],[3,4]]` / `K{f:[...]}` iç
         * dizileri ESC_YEREL kalıp ρ_yerel'de serbest ediliyordu = SESSİZ UAF (escape
         * hunt: 18 doğrulanmış UAF'ın kök sebebi). Artık elemanlara/alanlara recurse. */
        case DUGUM_DIZI_OLUSTUR:
            escape_yukselt(ea, ifade, yeni);
            for (int i = 0; i < ifade->veri.dizi_olustur.sayi; i++)
                ifadeyi_yukselt(ea, ifade->veri.dizi_olustur.elemanlar[i], yeni);
            return;
        case DUGUM_YAPI_OLUSTUR:
            escape_yukselt(ea, ifade, yeni);
            for (int i = 0; i < ifade->veri.yapi_olustur.alan_sayi; i++) {
                Dugum *aa = ifade->veri.yapi_olustur.alanlar[i];
                if (aa && aa->tip == DUGUM_ALAN_ATAMA)
                    ifadeyi_yukselt(ea, aa->veri.alan_atama.deger, yeni);
            }
            return;
        case DUGUM_LAMBDA:
            /* Lambda'nın KENDİSİ kaçar (G005 için). YAKALADIĞI bağlamalar için
             * free-routing GUARD'ı (islev_lambda_icerir → o fn'de routing YOK) sound
             * backstop'tur: lexical scope gereği yalnız BU fn'in lambda'sı BU fn'in
             * lokalini yakalayabilir; block-form gövde value-path'i eksik kalabilir →
             * guard şart (bkz. bolge_yerel_yonlendir). */
            escape_yukselt(ea, ifade, yeni);
            return;
        case DUGUM_CAGRI:
            /* Çağrı SONUCU'nu yükselt (node-self). Argümanlar AYRICA visit'in CAGRI
             * kolunda KOŞULSUZ ESC_CAGIRAN'a yükseltiliyor (passthrough `gecir(arr)`
             * dâhil) → burada arg-recurse'a GEREK YOK + RİSKLİ (büyük kaynakta derin
             * çağrı-zinciri + bağ-takibi yığın taşmasına yol açıyordu — codegen.kem). */
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
            /* SAGLAMLIK (D-101): dongu icindeki tahsis ARTIK ESC_ITERASYON
             * ISARETLENMEZ — yalnizca kayit olustur (default ESC_YEREL).
             *
             * Eski kod burada kosulsuz ITERASYON isaretliyordu. ITERASYON omru EN
             * KISA bolgedir (F4.3'te iterasyon-basina serbest); iterasyonu ASAN bir
             * tahsisi ITERASYON saymak = canliyken serbest = UAF. Iterasyon-yerelligi
             * SAGLAM tespit etmek, kaccis rotalarini KAPATAN kapilara (D-007 skaler-
             * dizi, R-GOMME gomme-yok) bagliydi; bu kapilar su an ENFORCE EDILMIYOR:
             * Dizi<Dizi<T>>, Dizi<metin>, Dizi-alanli yapi tipe gecer ve by-ref
             * KdlDizi* olarak lower edilir -> `dis[i] = tahsis` / `nesne.alan = tahsis`
             * gibi rotalar tahsisi iterasyondan kaccirir ve sentaktik tespit yetersiz
             * kalirdi (under-approximation = gizli UAF).
             *
             * Bu yuzden GUVENLI GERI-CEKILME: escape analizi HICBIR tahsisi
             * ESC_ITERASYON uretmez (hepsi YEREL — daha uzun omurlu, guvenli). Per-
             * iterasyon optimizasyonu, gercek bolge-serbest semantigi (F4.3) geldiginde
             * -- kapilar enforce edilince ya da TUM kaccis rotalari kapsaninca --
             * saglamca eklenecek. Su an UAF imkansiz (ITERASYON hic uretilmez). */
            kayit_bul_veya_ekle(ea, d);
            /* Recurse alt-ifadelere. NOT: gömülü heap-ref alt-tahsislerin (iç dizi,
             * alan değeri) ESC_CAGIRAN'a transitif terfisi BURADA koşulsuz YAPILMAZ;
             * agregat KAÇTIĞINDA ifadeyi_yukselt (artık DERİN) hallediyor → yerel
             * agregatın gömülü dizisi gereksiz yere ρ_caller'a sürülmez (daha keskin). */
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
                /* F4.2b GUARD: bu islev bir lambda iceriyor → free-routing'i bu
                 * islevde KAPAT (closure-capture sound backstop; lexical scope). */
                ea->islev_lambda_icerir = 1;
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
            /* [F4.2b SOUNDNESS] Çağrı ARGÜMANI callee tarafından RETAIN edilebilir
             * (örn. `dugum_n(p, ..., kids)` → kids'i AST'de saklar; `dizi_ekle(d, e)`).
             * Interprocedural özet yok → KONSERVATİF: her argümanı (ve bağlı olduğu
             * tahsisleri) ESC_CAGIRAN'a yükselt. Aksi halde arg ESC_YEREL kalır →
             * ρ_yerel'de serbest edilir → callee retain ettiyse UAF (bu fazın
             * routing'ini düşüren gerçek bug — escape.c kaçış-yolu boşluğu, A1-bitişik).
             * Over-approx: dizi_al/dizi_boyut gibi RETAIN-ETMEYEN builtin'lerde de
             * yükseltir (kabul: sızıntı bir hata, UAF bir felaket).
             *
             * İSTİSNA — LAMBDA argümanı: free-routing YALNIZ dizileri (DUGUM_DIZI_OLUSTUR)
             * ρ_yerel'de serbest eder; lambda HİÇBİR ZAMAN serbest edilmez → lambda arg'ı
             * CAGIRAN'a yükseltmek free-routing için GEREKSİZ. Üstelik ZARARLI: G005
             * (tip_kontrol.c, D-071) `escape_kategori==ESC_CAGIRAN` ile KAÇAN+yakalayan
             * closure'ı reddeder; `görev_başlat(|| ...)` deseni lambda'yı arg olarak verir
             * ve görev'in KENDİ sahiplik modeli (R-YAKALAMA-THREAD) vardır → CAGIRAN
             * yükseltme G005'i yanlış-pozitif tetikler (DRF T34/T37/T38). Lambda'yı
             * ATLA: G005-ilgili kategorisi (ver'lenmedikçe YEREL) korunur. */
            for (int ai = 0; ai < d->veri.cagri.sayi; ai++) {
                Dugum *arg = d->veri.cagri.argumanlar[ai];
                if (arg && arg->tip == DUGUM_LAMBDA) continue;
                ifadeyi_yukselt(ea, arg, ESC_CAGIRAN);
            }
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
            } else if (d->veri.atama.hedef
                       && (d->veri.atama.hedef->tip == DUGUM_ERISIM
                           || d->veri.atama.hedef->tip == DUGUM_INDEKS)) {
                /* [F4.2b SOUNDNESS] Agregat-eleman/alan ataması (`dis[i] = arr`,
                 * `s.alan = arr`): RHS, kaçabilen bir yapı/diziye SAKLANIR → kaçar.
                 * KONSERVATİF: RHS'i ESC_CAGIRAN'a yükselt. Aksi halde YEREL kalır →
                 * ρ_yerel'de serbest → agregat onu tutarken UAF (R3'ün "kaçan-yapıda
                 * saklama" yolu; A1 testleri bunu YEREL bırakır — A1 için sound, free
                 * için DEĞİL). Transitif R-GÖMME terfisi F4.4'e ertelenir; bu guard
                 * ile düz desenler sound. */
                ifadeyi_yukselt(ea, d->veri.atama.deger, ESC_CAGIRAN);
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

/* ====================================================================
 * F4.2b — KESİN-YEREL (confined) KANITI: POZİTİF, DEFAULT-DENY local-proof
 * ====================================================================
 * Escape DFA bir MAY-yaklaşımıdır (alias/yeniden-atama/loop-carried kaçışları
 * KAÇIRIR → "DFA escape bulamadı" free için GÜVENİLMEZ; her kaçırılan yol = UAF,
 * D-102 uyarısı). Bunun yerine: bir diziyi ρ_yerel'de serbest ETMEDEN ÖNCE
 * yerelliğini KANITLA. Bir dizi-değişkeni "confined" (kesin-yerel) sayılır ANCAK
 * VE ANCAK govdedeki HER kullanımı şunlardan biriyse:
 *   - `var[i]` okuma (DUGUM_INDEKS, nesne=var),
 *   - `var[i] = ...` yerinde yazma (ATAMA hedef=INDEKS(var)),
 *   - retain-ETMEYEN dizi-builtin'inin İLK argümanı (dizi_al/boyut/yaz/ekle/kapasite).
 * Başka HER konum (ver, b=var alias, [..,var,..]/yapı alanı, çağrı argümanı,
 * lambda yakalama, &var, var yeniden-atama) → DENY (ρ_caller). Default-deny =
 * inşa-gereği sound (principle 1+3). Skaler-eleman kısıtı llvm tarafında. */

/* ===================================================================
 * [D-488] INTERPROCEDURAL PARAMETRE-TUTMA OZETI
 * ===================================================================
 * SORU: `f(xs)` cagrisinda f, xs'i SAKLIYOR MU (retain)? Saklamiyorsa
 * xs'in hapsedilme kaniti KORUNUR -> ρ_yerel'de serbest edilebilir.
 *
 * ⚠ YENI ANALIZ YAZILMADI. Bu soru `ky_confined(f.govde, param_adi)` ile
 * BIREBIR AYNIDIR ve o yuklem F4.2b'de 18-UAF avindan gecmis, kanitlanmis
 * makinedir: `ver p` -> 0, lambda yakalama -> 0, kuresele/agregaya atama
 * -> 0, `g(p)` -> 0 (ozyineli sorgu), BILINMEYEN DUGUM -> 0.
 *
 * SAGLAMLIK (default-DENY her eksende):
 *   - cagrilan bilinmiyorsa (yerlesik / dolayli / bagli-olmayan ad) -> DENY
 *   - govde YOKSA (yalniz imza) -> DENY
 *   - OZYINELEME (f -> f) -> DENY (fn_devam yigini; sonlanma garantisi)
 *   - yigin dolarsa -> DENY
 * Yani "kanit bulamadim" DAIMA eski (konservatif) davranisa duser.
 */
typedef struct EaFnKayit {
    const char *ad;
    int uz;
    const Dugum *islev;
} EaFnKayit;

/* ky_confined ile karsilikli ozyineli — ileri bildirim. */
static int ky_confined(const Dugum *d, const char *ad, int uz);

/* Analiz suresince aktif tablo. ky_confined imzasi DEGISTIRILMEDI (dosya
 * icinde ~30 ozyineli cagri yeri var; imza degisikligi degisim yuzeyini
 * gereksiz genisletirdi). Tek is parcacikli derleyici. */

static void ea_fn_ekle(Arena *a, const Dugum *fn, int *kap) {
    if (!fn || fn->tip != DUGUM_ISLEV || !fn->veri.islev.govde) return;
    if (g_fn_sayi >= *kap) {
        int yeni = *kap ? *kap * 2 : 64;
        EaFnKayit *t = (EaFnKayit *)arena_ayir_sifir(a, (size_t)yeni * sizeof(EaFnKayit));
        if (!t) return;                     /* tahsis yok -> tablo buyumez -> DENY */
        for (int i = 0; i < g_fn_sayi; i++) t[i] = g_fn_tablo[i];
        g_fn_tablo = t; *kap = yeni;
    }
    g_fn_tablo[g_fn_sayi].ad = fn->veri.islev.ad;
    g_fn_tablo[g_fn_sayi].uz = fn->veri.islev.ad_uzunluk;
    g_fn_tablo[g_fn_sayi].islev = fn;
    g_fn_sayi++;
}

static void ea_fn_topla(Arena *a, const Dugum *d, int *kap) {
    if (!d) return;
    switch (d->tip) {
        case DUGUM_PROGRAM:
            for (int i = 0; i < d->veri.program.sayi; i++)
                ea_fn_topla(a, d->veri.program.uyeler[i], kap);
            return;
        case DUGUM_MODUL:
            for (int i = 0; i < d->veri.modul.sayi; i++)
                ea_fn_topla(a, d->veri.modul.uyeler[i], kap);
            return;
        case DUGUM_DISA: ea_fn_topla(a, d->veri.disa.tanim, kap); return;
        case DUGUM_ISLEV: ea_fn_ekle(a, d, kap); return;
        default: return;
    }
}

void escape_fn_tablo_kur(Arena *a, const Dugum *program) {
    if (!a || !program) return;
    if (g_fn_program == program) return;    /* ayni program -> yeniden kurma */
    g_fn_tablo = NULL; g_fn_sayi = 0;
    int kap = 0;
    ea_fn_topla(a, program, &kap);
    g_fn_program = program;
}

static const Dugum *ea_fn_bul(const char *ad, int uz) {
    if (!ad || !g_fn_tablo) return NULL;
    for (int i = 0; i < g_fn_sayi; i++) {
        const EaFnKayit *k = &g_fn_tablo[i];
        if (k->uz == uz && k->ad && memcmp(k->ad, ad, (size_t)uz) == 0)
            return k->islev;
    }
    return NULL;   /* bilinmeyen -> cagiran DENY'e duser */
}

/* f'in idx. parametresi govdede HAPSEDILMIS mi (yani f onu SAKLAMIYOR mu)? */
static const Dugum *g_fn_devam[32];
static int g_fn_devam_sayi = 0;

static int ea_param_tutmuyor(const Dugum *fn, int idx) {
    if (!fn || fn->tip != DUGUM_ISLEV) return 0;
    if (!fn->veri.islev.govde) return 0;                  /* yalniz imza -> DENY */
    if (idx < 0 || idx >= fn->veri.islev.param_sayi) return 0;
    const Dugum *p = fn->veri.islev.parametreler[idx];
    if (!p || p->tip != DUGUM_PARAMETRE || !p->veri.parametre.ad) return 0;

    /* OZYINELEME KORUYUCUSU: f zaten degerlendirilmekteyse DENY. Hem
     * sonlanmayi garanti eder hem konservatif taraftadir. */
    for (int i = 0; i < g_fn_devam_sayi; i++)
        if (g_fn_devam[i] == fn) return 0;
    if (g_fn_devam_sayi >= 32) return 0;        /* yigin doldu -> DENY */

    g_fn_devam[g_fn_devam_sayi++] = fn;
    int r = ky_confined(fn->veri.islev.govde,
                        p->veri.parametre.ad, p->veri.parametre.ad_uzunluk);
    g_fn_devam_sayi--;
    return r;
}

static int ky_ad_esit(const Dugum *d, const char *ad, int uz) {
    return d && d->tip == DUGUM_TANIMLAYICI
        && d->veri.tanimlayici.uzunluk == uz
        && memcmp(d->veri.tanimlayici.metin, ad, (size_t)uz) == 0;
}

/* [D-495] `olarak` (cast) ESCAPE SEMANTIGINI DEGISTIRMEZ: `p olarak metin`
 * hala p'nin DEGERIDIR. Kaci s sorusu icin katmani soy. Ham isaretcilerde bu
 * sekil KACINILMAZDIR (`bellek_kopyala(yeni olarak metin, ...)`). */
static const Dugum *cast_soy(const Dugum *d) {
    while (d && d->tip == DUGUM_TIP_DONUSTUR) d = d->veri.tip_donustur.kaynak;
    return d;
}

/* [D-495] Argumanlarini SAKLAMAYAN bellek yerlesikleri — KURATE LISTE.
 * ⚠⚠ `bellek_serbest` BILEREK YOK VE BU KRITIK: o `free`e eslesir. Onu
 * hapsedilmis saymak, hem ACIK `free` hem BOLGE serbesti calisirdi = CIFT
 * SERBEST. Liste "isaretci parametresi alan her yerlesik" DEGIL, "argumanini
 * TUTMAYAN yerlesik" demektir (D-459'un kurate-liste disiplini). */
static int ky_bellek_builtin_tutmaz(const char *ad, int uz) {
    return (uz == 14 && memcmp(ad, "bellek_kopyala", 14) == 0);
}

static int ky_dizi_builtin_confined(const char *ad, int uz) {
    /* Diziyi RETAIN ETMEYEN, yalnız oku/yerinde-değiştir builtin'leri. */
    return (uz == 7  && memcmp(ad, "dizi_al", 7) == 0)
        || (uz == 10 && memcmp(ad, "dizi_boyut", 10) == 0)
        || (uz == 8  && memcmp(ad, "dizi_yaz", 8) == 0)
        || (uz == 9  && memcmp(ad, "dizi_ekle", 9) == 0)
        || (uz == 13 && memcmp(ad, "dizi_kapasite", 13) == 0);
}

/* var subtree'de HERHANGİ bir yerde geçiyor mu? Bilinmeyen düğüm → 1 (konservatif). */
static int ky_var_gecer(const Dugum *d, const char *ad, int uz) {
    if (!d) return 0;
    switch (d->tip) {
        case DUGUM_TANIMLAYICI: return ky_ad_esit(d, ad, uz);
        case DUGUM_TAM: case DUGUM_KESIRLI: case DUGUM_KARAKTER:
        case DUGUM_MANTIKSAL: case DUGUM_BOS: case DUGUM_METIN:
        case DUGUM_YOL: case DUGUM_PARAMETRE:
            return 0;
        case DUGUM_INDEKS:
            return ky_var_gecer(d->veri.indeks.nesne, ad, uz)
                || ky_var_gecer(d->veri.indeks.indeks, ad, uz);
        case DUGUM_ATAMA:
            return ky_var_gecer(d->veri.atama.hedef, ad, uz)
                || ky_var_gecer(d->veri.atama.deger, ad, uz);
        case DUGUM_CAGRI: {
            if (ky_var_gecer(d->veri.cagri.hedef, ad, uz)) return 1;
            for (int i = 0; i < d->veri.cagri.sayi; i++)
                if (ky_var_gecer(d->veri.cagri.argumanlar[i], ad, uz)) return 1;
            return 0;
        }
        case DUGUM_DIZI_OLUSTUR:
            for (int i = 0; i < d->veri.dizi_olustur.sayi; i++)
                if (ky_var_gecer(d->veri.dizi_olustur.elemanlar[i], ad, uz)) return 1;
            return 0;
        case DUGUM_YAPI_OLUSTUR:
            for (int i = 0; i < d->veri.yapi_olustur.alan_sayi; i++) {
                Dugum *aa = d->veri.yapi_olustur.alanlar[i];
                if (aa && aa->tip == DUGUM_ALAN_ATAMA
                    && ky_var_gecer(aa->veri.alan_atama.deger, ad, uz)) return 1;
            }
            return 0;
        case DUGUM_LAMBDA: return ky_var_gecer(d->veri.lambda.govde, ad, uz);
        case DUGUM_VER: return ky_var_gecer(d->veri.ver.deger, ad, uz);
        case DUGUM_DEGISKEN: return ky_var_gecer(d->veri.degisken.deger, ad, uz);
        case DUGUM_SABIT: return ky_var_gecer(d->veri.sabit.deger, ad, uz);
        case DUGUM_BLOK:
            for (int i = 0; i < d->veri.blok.sayi; i++)
                if (ky_var_gecer(d->veri.blok.deyimler[i], ad, uz)) return 1;
            return 0;
        case DUGUM_IFADE_DEYIMI: return ky_var_gecer(d->veri.ifade_deyimi.ifade, ad, uz);
        case DUGUM_EGER:
            return ky_var_gecer(d->veri.eger.kosul, ad, uz)
                || ky_var_gecer(d->veri.eger.gozdoldur, ad, uz)
                || ky_var_gecer(d->veri.eger.yan, ad, uz);
        case DUGUM_IKEN:
            return ky_var_gecer(d->veri.iken.kosul, ad, uz)
                || ky_var_gecer(d->veri.iken.govde, ad, uz);
        case DUGUM_ICIN:
            return ky_var_gecer(d->veri.icin.koleksiyon, ad, uz)
                || ky_var_gecer(d->veri.icin.govde, ad, uz);
        case DUGUM_ESLES: {
            if (ky_var_gecer(d->veri.esles.deger, ad, uz)) return 1;
            for (int i = 0; i < d->veri.esles.kol_sayi; i++) {
                Dugum *k = d->veri.esles.kollar[i];
                if (k && k->tip == DUGUM_ESLES_KOLU
                    && ky_var_gecer(k->veri.esles_kolu.govde, ad, uz)) return 1;
            }
            return 0;
        }
        case DUGUM_GUVENSIZ: return ky_var_gecer(d->veri.guvensiz.blok, ad, uz);
        case DUGUM_IKILI:
            return ky_var_gecer(d->veri.ikili.sol, ad, uz)
                || ky_var_gecer(d->veri.ikili.sag, ad, uz);
        case DUGUM_TEKLI: return ky_var_gecer(d->veri.tekli.operand, ad, uz);
        case DUGUM_ERISIM: return ky_var_gecer(d->veri.erisim.nesne, ad, uz);
        case DUGUM_IMHA_IFADE:
            /* [D-507] `imha(e)` / `kullan(e)` — LINEER TUKETIM dugumleri.
             * Dallari YOKTU -> `default: return 1` (bilinmeyen -> konservatif
             * "geciyor") lineer yakalayan HER kapanisin hapsedilmesini
             * DUSURUYORDU. Bisect ile bulundu: skaler yakalayan ayni sekil
             * GECIYORDU (c1 conf=1, c2 conf=0). D-495'in cast kokuyle AYNI
             * SINIF: eksik dal = sessiz asiri-muhafazakarlik. */
            return ky_var_gecer(d->veri.imha_ifade.operand, ad, uz);
        case DUGUM_KULLAN_IFADE:
            return ky_var_gecer(d->veri.kullan_ifade.operand, ad, uz);
        case DUGUM_TIP_DONUSTUR:
            /* [D-495] Cast TAM OLARAK kaynagini icerir — `default: return 1`
             * (bilinmeyen -> konservatif "geciyor") burada GEREKSIZ KATIYDI ve
             * `bellek_kopyala(yeni olarak metin, eski olarak metin, ..)`
             * seklinde HER IKI isaretcinin de hapsedilmesini dusuruyordu.
             * Bisect ile bulundu: cast'siz ayni sekil GECIYORDU. */
            return ky_var_gecer(d->veri.tip_donustur.kaynak, ad, uz);
        default: return 1;  /* bilinmeyen → konservatif "geçiyor" */
    }
}

/* var'in TÜM kullanimlari confined mi? Bare var ulasirsa (parent tuketmediyse) = 0. */
static int ky_confined(const Dugum *d, const char *ad, int uz) {
    if (!d) return 1;
    switch (d->tip) {
        case DUGUM_TANIMLAYICI:
            return ky_ad_esit(d, ad, uz) ? 0 : 1;  /* bare var = guvensiz */
        case DUGUM_TAM: case DUGUM_KESIRLI: case DUGUM_KARAKTER:
        case DUGUM_MANTIKSAL: case DUGUM_BOS: case DUGUM_METIN:
        case DUGUM_YOL: case DUGUM_PARAMETRE:
            return 1;
        case DUGUM_INDEKS:
            if (ky_ad_esit(d->veri.indeks.nesne, ad, uz))   /* var[i] okuma OK */
                return ky_confined(d->veri.indeks.indeks, ad, uz);
            return ky_confined(d->veri.indeks.nesne, ad, uz)
                && ky_confined(d->veri.indeks.indeks, ad, uz);
        case DUGUM_ATAMA:
            if (d->veri.atama.hedef && d->veri.atama.hedef->tip == DUGUM_INDEKS
                && ky_ad_esit(d->veri.atama.hedef->veri.indeks.nesne, ad, uz)) {
                /* var[i] = RHS : yerinde yazma OK */
                return ky_confined(d->veri.atama.hedef->veri.indeks.indeks, ad, uz)
                    && ky_confined(d->veri.atama.deger, ad, uz);
            }
            if (ky_ad_esit(d->veri.atama.hedef, ad, uz))
                return 0;  /* var = ... : yeniden-atama → guvensiz (eski deger alias olabilir) */
            return ky_confined(d->veri.atama.hedef, ad, uz)
                && ky_confined(d->veri.atama.deger, ad, uz);
        case DUGUM_CAGRI: {
            const Dugum *h = d->veri.cagri.hedef;
            int safe0 = (h && h->tip == DUGUM_TANIMLAYICI
                && ky_dizi_builtin_confined(h->veri.tanimlayici.metin, h->veri.tanimlayici.uzunluk)
                && d->veri.cagri.sayi >= 1
                && ky_ad_esit(d->veri.cagri.argumanlar[0], ad, uz));
            /* [D-507] CAGIRMAK TUTMAK DEGILDIR. `g()` — yani degiskenin
             * CAGRI HEDEFI olmasi — onceden KACIS sayiliyordu (`ky_confined(h)`
             * bare tanimlayiciya 0 doner) ve kapanis env'inin hapsedilmesini
             * DUSURUYORDU. Cagri, kapanisi bir yere SAKLAMAZ; yalnizca lifted
             * islevi env ile calistirir.
             * ⚠ SAGLAMLIK G006'YA DAYANIR: lifted govde env'e isaretci
             *   DONDUREMEZ (`|| &a` artik derleme zamaninda reddediliyor).
             *   G006 OLMADAN bu gevsetme UAF acardi.
             * ⚠ YALNIZ HEDEF konumu: ARGUMAN olarak gecmek (`al(g)`) HALA
             *   asagidaki DENY yolundan gecer (callee saklayabilir; D-488
             *   ozeti kanit bulursa oradan gecer). */
            if (!(h && ky_ad_esit(h, ad, uz)) && !ky_confined(h, ad, uz)) return 0;
            for (int i = 0; i < d->veri.cagri.sayi; i++) {
                if (i == 0 && safe0) continue;  /* dizi_*(var, ...) ilk arg tuketildi */
                /* [D-488] KULLANICI ISLEVI OZETI: arg TAM OLARAK bu degiskense ve
                 * cagrilan f onu SAKLAMIYORSA (ky_confined(f.govde, param)) hapsedilme
                 * KORUNUR. Kanit bulunamazsa asagidaki eski DENY yolu isler. */
                if (ky_ad_esit(cast_soy(d->veri.cagri.argumanlar[i]), ad, uz)
                    && h && h->tip == DUGUM_TANIMLAYICI
                    && ea_param_tutmuyor(ea_fn_bul(h->veri.tanimlayici.metin,
                                                   h->veri.tanimlayici.uzunluk), i))
                    continue;
                /* [D-495] SAKLAMAYAN BELLEK YERLESIGI: `bellek_kopyala` (memcpy)
                 * argumanini TUTMAZ -> hapsedilme KORUNUR. Cast katmani soyulur
                 * cunku ham isaretci bu yerlesiklere DAIMA `olarak metin` ile
                 * gecer. ⚠ `bellek_serbest` (free) listede YOK: onu hapsedilmis
                 * saymak CIFT SERBEST uretirdi (acik free + bolge serbesti). */
                if (h && h->tip == DUGUM_TANIMLAYICI
                    && ky_bellek_builtin_tutmaz(h->veri.tanimlayici.metin,
                                                h->veri.tanimlayici.uzunluk)
                    && ky_ad_esit(cast_soy(d->veri.cagri.argumanlar[i]), ad, uz))
                    continue;
                if (!ky_confined(d->veri.cagri.argumanlar[i], ad, uz)) return 0;
            }
            return 1;
        }
        case DUGUM_LAMBDA:
            return ky_var_gecer(d->veri.lambda.govde, ad, uz) ? 0 : 1;  /* yakalama = guvensiz */
        case DUGUM_VER: return ky_confined(d->veri.ver.deger, ad, uz);
        case DUGUM_DEGISKEN: return ky_confined(d->veri.degisken.deger, ad, uz);
        case DUGUM_SABIT: return ky_confined(d->veri.sabit.deger, ad, uz);
        case DUGUM_BLOK:
            for (int i = 0; i < d->veri.blok.sayi; i++)
                if (!ky_confined(d->veri.blok.deyimler[i], ad, uz)) return 0;
            return 1;
        case DUGUM_IFADE_DEYIMI: return ky_confined(d->veri.ifade_deyimi.ifade, ad, uz);
        case DUGUM_EGER:
            return ky_confined(d->veri.eger.kosul, ad, uz)
                && ky_confined(d->veri.eger.gozdoldur, ad, uz)
                && ky_confined(d->veri.eger.yan, ad, uz);
        case DUGUM_IKEN:
            return ky_confined(d->veri.iken.kosul, ad, uz)
                && ky_confined(d->veri.iken.govde, ad, uz);
        case DUGUM_ICIN:
            return ky_confined(d->veri.icin.koleksiyon, ad, uz)
                && ky_confined(d->veri.icin.govde, ad, uz);
        case DUGUM_ESLES: {
            if (!ky_confined(d->veri.esles.deger, ad, uz)) return 0;
            for (int i = 0; i < d->veri.esles.kol_sayi; i++) {
                Dugum *k = d->veri.esles.kollar[i];
                if (k && k->tip == DUGUM_ESLES_KOLU
                    && !ky_confined(k->veri.esles_kolu.govde, ad, uz)) return 0;
            }
            return 1;
        }
        case DUGUM_GUVENSIZ: return ky_confined(d->veri.guvensiz.blok, ad, uz);
        case DUGUM_IKILI:
            return ky_confined(d->veri.ikili.sol, ad, uz)
                && ky_confined(d->veri.ikili.sag, ad, uz);
        case DUGUM_TEKLI:
            /* [D-494] DEREFERANS ISARETCIYI SIZDIRMAZ — `&` SIZDIRIR.
             * Eskiden ikisi de ayni yoldan gecip `*p`yi de KACIS sayiyordu
             * (operand cıplak `p` -> 0). Bu, ham isaretcileri (`bölge_al`
             * sonucu) YAPISAL OLARAK hapsedilemez kiliyordu: ham isaretci
             * DAIMA deref edilir.
             * SAGLAMLIK: `*p` p'nin DEGERINI degil, GOSTERDIGI SEYI okur/yazar
             * -> p disari cikmaz. `&p` ise p'nin ADRESINI verir -> KACIS.
             * `*p` icindeki daha derin kullanimlar YINE denetlenir (`**p`,
             * `*(f(p))` gibi sekiller ozyineli olarak dogru sonuclanir). */
            if (d->veri.tekli.op == OP_DEREFERANS
                && ky_ad_esit(d->veri.tekli.operand, ad, uz))
                return 1;
            return ky_confined(d->veri.tekli.operand, ad, uz);  /* &var → bare → 0 */
        case DUGUM_ERISIM: return ky_confined(d->veri.erisim.nesne, ad, uz);
        /* [D-507] ky_var_gecer ikizi: tuketim operandinin hapsedilmesi sorulur. */
        case DUGUM_IMHA_IFADE:
            return ky_confined(d->veri.imha_ifade.operand, ad, uz);
        case DUGUM_KULLAN_IFADE:
            return ky_confined(d->veri.kullan_ifade.operand, ad, uz);
        case DUGUM_TIP_DONUSTUR:
            /* [D-495] Cast bir DEGERI donusturur; isaretciyi SIZDIRMAZ.
             * `ky_var_gecer` ikizi burada da sart: `sonuc = v[2] olarak tam32`
             * varsayilan yola dusup REDDEDILIYORDU (cast dali yoktu), oysa o
             * yalnizca bir OKUMADIR. Kaynak neyse hapsedilme onun sorusudur. */
            return ky_confined(d->veri.tip_donustur.kaynak, ad, uz);
        case DUGUM_DIZI_OLUSTUR:
            for (int i = 0; i < d->veri.dizi_olustur.sayi; i++)
                if (!ky_confined(d->veri.dizi_olustur.elemanlar[i], ad, uz)) return 0;
            return 1;
        case DUGUM_YAPI_OLUSTUR:
            for (int i = 0; i < d->veri.yapi_olustur.alan_sayi; i++) {
                Dugum *aa = d->veri.yapi_olustur.alanlar[i];
                if (aa && aa->tip == DUGUM_ALAN_ATAMA
                    && !ky_confined(aa->veri.alan_atama.deger, ad, uz)) return 0;
            }
            return 1;
        default:
            return ky_var_gecer(d, ad, uz) ? 0 : 1;  /* bilinmeyen + var geçiyor → guvensiz */
    }
}

static void ky_set(EscapeAnaliz *ea, const Dugum *d) {
    for (int i = 0; i < ea->kayit_sayi; i++)
        if (ea->kayitlar[i].dugum == d) { ea->kayitlar[i].kesin_yerel = 1; return; }
}

/* Govdeyi gez; her `değişken V: Dizi<...> = [literal]` icin V confined ise
 * literal-dugumunu kesin_yerel isaretle. */
static void ky_isaretle(EscapeAnaliz *ea, const Dugum *d, const Dugum *fn_govde) {
    if (!d) return;
    switch (d->tip) {
        case DUGUM_DEGISKEN:
            /* [D-507] `değişken F = |..| ..` — KAPANIS dali. env `@malloc`
             * ile aliniyor ve HIC serbest edilmiyordu (llvm.c'nin kendi
             * yorumu: "SERBEST BIRAKMA YOK (leak ...); F4 sonra").
             * F confined ise LAMBDA dugumu kesin_yerel -> codegen env'i
             * ρ_yerel'den tahsis eder, islev sonunda TOPLU serbest.
             * ⚠ Tip ANNOTASYONU ARANMAZ: kapanis bagi genelde annotasyonsuz
             *   yazilir (`değişken arttir = |n| ..`) — annotasyon sartı
             *   gercek kodu disarida birakirdi (D-506'nin literal-only
             *   kapsam dersi). */
            if (d->veri.degisken.deger
                && d->veri.degisken.deger->tip == DUGUM_LAMBDA
                && d->veri.degisken.ad && d->veri.degisken.ad_uzunluk > 0
                && ky_confined(fn_govde, d->veri.degisken.ad,
                               d->veri.degisken.ad_uzunluk)) {
                kayit_bul_veya_ekle(ea, d->veri.degisken.deger);
                ky_set(ea, d->veri.degisken.deger);
            }
            /* [D-494] `değişken V: *T = bölge_al(...)` — HAM ISARETCI dali.
             * `bölge_al` D-415'ten beri GERCEK bir tahsistir (`malloc`) ve
             * HIC serbest edilmiyordu; kodun kendi yorumu bunu "v1
             * malloc-VEKALETEN (gercek arena V2)" diye isaretlemisti.
             * V confined ise CAGRI dugumu kesin_yerel -> codegen onu
             * ρ_yerel'den tahsis eder ve islev sonunda TOPLU serbest edilir. */
            if (d->veri.degisken.tip
                && d->veri.degisken.tip->tip == DUGUM_TIP_POINTER
                && d->veri.degisken.deger
                && d->veri.degisken.deger->tip == DUGUM_CAGRI
                && d->veri.degisken.ad && d->veri.degisken.ad_uzunluk > 0) {
                const Dugum *h = d->veri.degisken.deger->veri.cagri.hedef;
                if (h && h->tip == DUGUM_TANIMLAYICI
                    && h->veri.tanimlayici.uzunluk == 9
                    && memcmp(h->veri.tanimlayici.metin, "b\xc3\xb6lge_al", 9) == 0
                    && ky_confined(fn_govde, d->veri.degisken.ad,
                                   d->veri.degisken.ad_uzunluk)) {
                    kayit_bul_veya_ekle(ea, d->veri.degisken.deger);
                    ky_set(ea, d->veri.degisken.deger);
                }
            }
            /* [D-506] `değişken V: Dizi<T> = dizi_olustur(N)` — YERLESIK CAGRI dali.
             * D-494'un `bölge_al` dalinin BIREBIR SIMETRIGI. Onceden YALNIZ dizi
             * LITERALI (`[1,2,3]`) isaretleniyordu; `dizi_olustur(N)` bir CAGRI
             * dugumudur ve hic isaretlenmiyordu -> codegen onu KOSULSUZ ρ_caller'a
             * yayiyordu. OLCULDU: ayni govde literalle ρ_yerel, `dizi_olustur` ile
             * ρ_global; 200K cagrida zirve bellek 100 MB (sabit olmasi gerekirken).
             * `dizi_olustur` boyutlu dizi kurmanin DEYIMSEL yoludur (stdlib/dizi.kem
             * onu kullanir) -> literal-only kapsam gercek kodu disarida birakiyordu. */
            if (d->veri.degisken.tip && d->veri.degisken.tip->tip == DUGUM_TIP_DIZI
                && d->veri.degisken.deger
                && d->veri.degisken.deger->tip == DUGUM_CAGRI
                && d->veri.degisken.ad && d->veri.degisken.ad_uzunluk > 0) {
                const Dugum *dh = d->veri.degisken.deger->veri.cagri.hedef;
                if (dh && dh->tip == DUGUM_TANIMLAYICI
                    && dh->veri.tanimlayici.uzunluk == 12
                    && memcmp(dh->veri.tanimlayici.metin, "dizi_olustur", 12) == 0
                    && ky_confined(fn_govde, d->veri.degisken.ad,
                                   d->veri.degisken.ad_uzunluk)) {
                    kayit_bul_veya_ekle(ea, d->veri.degisken.deger);
                    ky_set(ea, d->veri.degisken.deger);
                }
            }
            if (d->veri.degisken.tip && d->veri.degisken.tip->tip == DUGUM_TIP_DIZI
                && d->veri.degisken.deger
                && d->veri.degisken.deger->tip == DUGUM_DIZI_OLUSTUR
                && d->veri.degisken.ad && d->veri.degisken.ad_uzunluk > 0
                && ky_confined(fn_govde, d->veri.degisken.ad, d->veri.degisken.ad_uzunluk)) {
                ky_set(ea, d->veri.degisken.deger);
            }
            return;
        case DUGUM_BLOK:
            for (int i = 0; i < d->veri.blok.sayi; i++)
                ky_isaretle(ea, d->veri.blok.deyimler[i], fn_govde);
            return;
        case DUGUM_EGER:
            ky_isaretle(ea, d->veri.eger.gozdoldur, fn_govde);
            ky_isaretle(ea, d->veri.eger.yan, fn_govde);
            return;
        case DUGUM_IKEN: ky_isaretle(ea, d->veri.iken.govde, fn_govde); return;
        case DUGUM_ICIN: ky_isaretle(ea, d->veri.icin.govde, fn_govde); return;
        case DUGUM_ESLES:
            for (int i = 0; i < d->veri.esles.kol_sayi; i++) {
                Dugum *k = d->veri.esles.kollar[i];
                if (k && k->tip == DUGUM_ESLES_KOLU)
                    ky_isaretle(ea, k->veri.esles_kolu.govde, fn_govde);
            }
            return;
        case DUGUM_GUVENSIZ: ky_isaretle(ea, d->veri.guvensiz.blok, fn_govde); return;
        default: return;
    }
}

int escape_kesin_yerel(const EscapeAnaliz *ea, const Dugum *d) {
    if (!ea || !d) return 0;
    for (int i = 0; i < ea->kayit_sayi; i++)
        if (ea->kayitlar[i].dugum == d) return ea->kayitlar[i].kesin_yerel;
    return 0;
}

void escape_analiz_islev(EscapeAnaliz *ea, const Dugum *islev) {
    if (!islev || islev->tip != DUGUM_ISLEV) return;
    const Dugum *govde = islev->veri.islev.govde;
    if (!govde) return;

    /* F4.2b: per-islev lambda-guard bayragini sifirla (visit LAMBDA set eder). */
    ea->islev_lambda_icerir = 0;

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

    /* F4.2b: fixed-point bitti → POZİTİF kesin-yerel kanıtı (confined dizi-değişkenleri
     * isaretle). free-routing YALNIZ bu işaretli + skaler-eleman dizileri ρ_yerel'e
     * yönlendirir (bolge_yerel_yonlendir). */
    ky_isaretle(ea, govde, govde);
}

void escape_analiz_program(EscapeAnaliz *ea, const Dugum *program) {
    if (!program) return;

    /* [D-488] Bu yol (program-genelinde gezinme) tabloyu KENDISI kurar.
     * Derleyicinin gercek yolu `escape_analiz_islev`tir (islev basina) —
     * orada tablo `escape_fn_tablo_kur` ile ONCEDEN kurulmus olmalidir;
     * kurulmamissa ea_fn_bul NULL doner -> eski konservatif davranis. */
    escape_fn_tablo_kur(ea->arena, program);


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

/* F4.2b (principle 1): düğüm escape analizinde AÇIKÇA kayıtlı mı? Free-routing
 * yalnız KAYITLI-VE-YEREL'e güvenir; kayıtsız düğüm escape_kategori'de default
 * ESC_YEREL döner ama bu free için GÜVENİLMEZ → kayıtsız → ρ_caller. */
int escape_kayitli_mi(const EscapeAnaliz *ea, const Dugum *d) {
    for (int i = 0; i < ea->kayit_sayi; i++) {
        if (ea->kayitlar[i].dugum == d) return 1;
    }
    return 0;
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
