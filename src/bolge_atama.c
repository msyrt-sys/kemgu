#include "bolge_atama.h"
#include "hata.h"

#include <string.h>
#include <stdio.h>

void bolge_atama_baslat(BolgeAtama *ba, Arena *a,
                        const char *islev_adi, int uz) {
    ba->arena = a;
    ba->islev_adi = islev_adi;
    ba->islev_adi_uz = uz;
    ba->dongu_derinligi = 0;
    ba->dongu_id_sayaci = 0;
    ba->aktif_iterasyon = NULL;
    ba->ver_baglaminda = 0;
    ba->sembol_sayi = 0;
    ba->hata_sayisi = 0;
    ba->dosya_adi = NULL;
    ba->kaynak = NULL;
}

void bolge_atama_kaynak_ayarla(BolgeAtama *ba,
                               const char *dosya_adi, const char *kaynak) {
    ba->dosya_adi = dosya_adi;
    ba->kaynak = kaynak;
}

/* === Sembol-bolge haritasi === */

static void sembol_kaydet(BolgeAtama *ba, const char *ad, int uz,
                          BolgeBilgisi *bolge) {
    if (ba->sembol_sayi >= BOLGE_ATAMA_MAX_SEMBOL) return;
    BolgeSembol *s = &ba->semboller[ba->sembol_sayi++];
    s->ad = ad;
    s->ad_uzunluk = uz;
    s->bolge = bolge;
}

static BolgeBilgisi *sembol_bolgesi(BolgeAtama *ba, const char *ad, int uz) {
    for (int i = ba->sembol_sayi - 1; i >= 0; i--) {
        if (ba->semboller[i].ad_uzunluk == uz &&
            memcmp(ba->semboller[i].ad, ad, (size_t)uz) == 0) {
            return ba->semboller[i].bolge;
        }
    }
    return NULL;
}

static BolgeSembol *sembol_bul_mutable(BolgeAtama *ba,
                                        const char *ad, int uz) {
    for (int i = ba->sembol_sayi - 1; i >= 0; i--) {
        if (ba->semboller[i].ad_uzunluk == uz &&
            memcmp(ba->semboller[i].ad, ad, (size_t)uz) == 0) {
            return &ba->semboller[i];
        }
    }
    return NULL;
}

/* === Hata raporlama === */

static void bolge_hata(BolgeAtama *ba, const Dugum *d,
                       const char *kod, const char *mesaj) {
    ba->hata_sayisi++;
    if (ba->dosya_adi && ba->kaynak && d) {
        hata_raporla(ba->dosya_adi, ba->kaynak,
                     d->satir, d->sutun, kod, mesaj, NULL);
    }
}

/* === Yardimci: aktif baglama gore "varsayilan tahsis bolgesi" === */
static BolgeBilgisi *varsayilan_tahsis(BolgeAtama *ba) {
    /* R-VER: ver icinde -> CAGIRAN (escape ediyor) */
    if (ba->ver_baglaminda) {
        return bolge_olustur_cagiran(ba->arena,
            ba->islev_adi, ba->islev_adi_uz);
    }
    /* R-ITERASYON: dongu icinde + escape yok -> ITERASYON */
    if (ba->dongu_derinligi > 0 && ba->aktif_iterasyon) {
        return ba->aktif_iterasyon;
    }
    /* R-YEREL: default */
    return bolge_olustur_yerel(ba->arena,
        ba->islev_adi, ba->islev_adi_uz);
}

BolgeBilgisi *bolge_belirle(BolgeAtama *ba, const Dugum *d) {
    if (!d) return NULL;

    switch (d->tip) {
        /* === R-LIT: basit literal -> bolge yok (stack) === */
        case DUGUM_TAM:
        case DUGUM_KESIRLI:
        case DUGUM_KARAKTER:
        case DUGUM_MANTIKSAL:
        case DUGUM_BOS:
            return bolge_olustur_basit(ba->arena, BOLGE_LIT);

        /* === R-YEREL veya R-VER: bilesik literal === */
        case DUGUM_METIN:
        case DUGUM_DIZI_OLUSTUR:
        case DUGUM_YAPI_OLUSTUR:
        case DUGUM_LAMBDA:
            return varsayilan_tahsis(ba);

        /* === E.1: Tanimlayici -> harita'dan bolge === */
        case DUGUM_TANIMLAYICI: {
            BolgeBilgisi *b = sembol_bolgesi(ba,
                d->veri.tanimlayici.metin, d->veri.tanimlayici.uzunluk);
            if (b) return b;
            /* Bilinmeyen tanimlayici — default yerel */
            return bolge_olustur_yerel(ba->arena,
                ba->islev_adi, ba->islev_adi_uz);
        }

        case DUGUM_CAGRI: {
            /* Cagrinin sonucu — donus tipinin bolgesi normalde CAGIRAN
             * cunku caller'in scope'unda yasayacak. Bu basit modelde
             * cagri sonucu yerel olarak isaretlenir; ver baglaminda CAGIRAN. */
            return varsayilan_tahsis(ba);
        }

        case DUGUM_IKILI:
        case DUGUM_TEKLI: {
            /* OP_REF (&x): operandin bolgesi.
             * Diger operatorler: varsayilan */
            if (d->tip == DUGUM_TEKLI &&
                (d->veri.tekli.op == OP_REF ||
                 d->veri.tekli.op == OP_REF_DEGISKEN)) {
                return bolge_belirle(ba, d->veri.tekli.operand);
            }
            if (d->tip == DUGUM_TEKLI &&
                d->veri.tekli.op == OP_DEREFERANS) {
                /* *p: p'nin isaret ettigi degerin bolgesi —
                 * konservatif olarak p'nin bolgesini kullan */
                return bolge_belirle(ba, d->veri.tekli.operand);
            }
            return varsayilan_tahsis(ba);
        }

        case DUGUM_ERISIM:
            /* x.y -> y'nin bolgesi x'in bolgesidir (alan x'e ait) */
            return bolge_belirle(ba, d->veri.erisim.nesne);

        case DUGUM_INDEKS:
            return bolge_belirle(ba, d->veri.indeks.nesne);

        case DUGUM_YOL:
            /* x::y -> global (modul uyeleri) */
            return bolge_olustur_basit(ba->arena, BOLGE_GLOBAL);

        /* === R-KOSUL: koşullu dallanma -> LCA === */
        case DUGUM_EGER: {
            BolgeBilgisi *gd = bolge_belirle(ba, d->veri.eger.gozdoldur);
            BolgeBilgisi *yan = d->veri.eger.yan
                ? bolge_belirle(ba, d->veri.eger.yan)
                : NULL;
            if (yan) return bolge_lca(ba->arena, gd, yan);
            return gd ? gd : varsayilan_tahsis(ba);
        }

        case DUGUM_BLOK: {
            /* Blok ifadesi: scope watermark — sembol haritasini sifirla.
             * Son deyimin bolgesini don. */
            int watermark = ba->sembol_sayi;
            int n = d->veri.blok.sayi;
            BolgeBilgisi *son = NULL;
            for (int i = 0; i < n; i++) {
                son = bolge_belirle(ba, d->veri.blok.deyimler[i]);
            }
            ba->sembol_sayi = watermark;
            return son ? son : bolge_olustur_basit(ba->arena, BOLGE_LIT);
        }

        /* === Deyimler — yan etki: sembol haritasi guncellenir === */
        case DUGUM_VER: {
            int eski = ba->ver_baglaminda;
            ba->ver_baglaminda = 1;
            BolgeBilgisi *r = d->veri.ver.deger
                ? bolge_belirle(ba, d->veri.ver.deger)
                : bolge_olustur_basit(ba->arena, BOLGE_LIT);
            ba->ver_baglaminda = eski;

            /* E.3: VER ihlal kontrolu — sadece *referans* deger sızdırılırsa
             * ihlal. KEMGU'da degerler kopya semantigi (kopyalanir caller'a).
             * Ama '&yerel' referansi caller'a verilirse → use-after-free. */
            if (d->veri.ver.deger &&
                d->veri.ver.deger->tip == DUGUM_TEKLI &&
                (d->veri.ver.deger->veri.tekli.op == OP_REF ||
                 d->veri.ver.deger->veri.tekli.op == OP_REF_DEGISKEN)) {
                /* Operand bolgesi kontrol — ver_baglaminda KAPALI olarak */
                int eski_v = ba->ver_baglaminda;
                ba->ver_baglaminda = 0;
                BolgeBilgisi *ref_b = bolge_belirle(ba,
                    d->veri.ver.deger->veri.tekli.operand);
                ba->ver_baglaminda = eski_v;
                if (ref_b && (ref_b->kategori == BOLGE_YEREL ||
                              ref_b->kategori == BOLGE_ITERASYON)) {
                    bolge_hata(ba, d, "B001",
                        "yerel/iterasyon adresinin 'ver' ile sızdırılmasi yasak");
                }
            }
            return r;
        }

        case DUGUM_ICIN: {
            /* Yeni iterasyon bolgesi yarat */
            BolgeBilgisi *eski_it = ba->aktif_iterasyon;
            int eski_d = ba->dongu_derinligi;
            ba->aktif_iterasyon = bolge_olustur_iterasyon(ba->arena,
                ba->dongu_id_sayaci++);
            ba->dongu_derinligi++;

            /* Iterasyon degiskeni: koleksiyonun bolgesinde */
            int watermark = ba->sembol_sayi;
            BolgeBilgisi *kol_b = bolge_belirle(ba, d->veri.icin.koleksiyon);
            (void)kol_b;
            sembol_kaydet(ba, d->veri.icin.degisken_adi,
                          d->veri.icin.degisken_adi_uzunluk,
                          ba->aktif_iterasyon);

            BolgeBilgisi *r = bolge_belirle(ba, d->veri.icin.govde);
            ba->sembol_sayi = watermark;

            ba->dongu_derinligi = eski_d;
            ba->aktif_iterasyon = eski_it;
            return r;
        }

        case DUGUM_IKEN: {
            BolgeBilgisi *eski_it = ba->aktif_iterasyon;
            int eski_d = ba->dongu_derinligi;
            ba->aktif_iterasyon = bolge_olustur_iterasyon(ba->arena,
                ba->dongu_id_sayaci++);
            ba->dongu_derinligi++;
            BolgeBilgisi *r = bolge_belirle(ba, d->veri.iken.govde);
            ba->dongu_derinligi = eski_d;
            ba->aktif_iterasyon = eski_it;
            return r;
        }

        case DUGUM_IFADE_DEYIMI:
            return bolge_belirle(ba, d->veri.ifade_deyimi.ifade);

        case DUGUM_DEGISKEN: {
            /* E.1: degisken storage bolgesi = tanim baglaminin yerel bolgesi.
             * Dongu icinde -> ITERASYON, disinda -> YEREL.
             * Deger ifadesi de analiz edilir (yan etkilerle escape tespit). */
            if (d->veri.degisken.deger) {
                /* Deger bolgesi (LIT/YEREL/etc) — escape ihlali icin */
                (void)bolge_belirle(ba, d->veri.degisken.deger);
            }
            /* x'in *storage* bolgesi yerel/iterasyon (kopya semantik) */
            int eski_v = ba->ver_baglaminda;
            ba->ver_baglaminda = 0;
            BolgeBilgisi *storage = varsayilan_tahsis(ba);
            ba->ver_baglaminda = eski_v;
            sembol_kaydet(ba, d->veri.degisken.ad,
                          d->veri.degisken.ad_uzunluk, storage);
            return storage;
        }

        case DUGUM_ATAMA: {
            /* E.2: atama dataflow — kopya semantik nedeniyle scalar deger
             * atamalari OK (B002 raporlanmaz). Gercek ihlal: referans/pointer
             * uzerinden atama — gelecek surumlerde detayli. Su an sadece
             * sembol haritasini guncelle (en kisa omur takip). */
            BolgeBilgisi *yeni = bolge_belirle(ba, d->veri.atama.deger);
            const Dugum *hedef = d->veri.atama.hedef;
            if (hedef && hedef->tip == DUGUM_TANIMLAYICI && yeni) {
                BolgeSembol *s = sembol_bul_mutable(ba,
                    hedef->veri.tanimlayici.metin,
                    hedef->veri.tanimlayici.uzunluk);
                if (s && s->bolge) {
                    /* Konservatif takip: en kisa omru kaydet (escape analizi) */
                    if (bolge_omru_kisa_mi(yeni, s->bolge)) {
                        s->bolge = yeni;
                    }
                }
            }
            return yeni;
        }

        case DUGUM_GUVENSIZ:
            return bolge_belirle(ba, d->veri.guvensiz.blok);

        case DUGUM_HATA:
            return bolge_olustur_basit(ba->arena, BOLGE_HATA);

        default:
            return bolge_olustur_basit(ba->arena, BOLGE_BILINMIYOR);
    }
}
