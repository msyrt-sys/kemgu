#include "bolge_atama.h"

#include <string.h>

void bolge_atama_baslat(BolgeAtama *ba, Arena *a,
                        const char *islev_adi, int uz) {
    ba->arena = a;
    ba->islev_adi = islev_adi;
    ba->islev_adi_uz = uz;
    ba->dongu_derinligi = 0;
    ba->dongu_id_sayaci = 0;
    ba->aktif_iterasyon = NULL;
    ba->ver_baglaminda = 0;
    ba->thread_id_sayaci = 0;
    ba->kanal_id_sayaci = 0;
    ba->aktif_gorev = NULL;
}

/* Cagri hedefi intrinsic-style ad mi (örn. "_gorev_baslat") */
static int cagri_intrinsic_mi(const Dugum *d, const char *ad) {
    if (!d || d->tip != DUGUM_CAGRI) return 0;
    const Dugum *h = d->veri.cagri.hedef;
    if (!h || h->tip != DUGUM_TANIMLAYICI) return 0;
    int uz = (int)strlen(ad);
    return h->veri.tanimlayici.uzunluk == uz &&
           memcmp(h->veri.tanimlayici.metin, ad, (size_t)uz) == 0;
}

/* Yardimci: aktif baglama gore "varsayilan tahsis bolgesi" */
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

        /* === Tanimlayici / cagri / erisim / indeks: bagimli === */
        case DUGUM_TANIMLAYICI:
            /* Sembolun bolgesi (sembol tablosunda saklanmiyor su an,
             * default yerel) */
            return bolge_olustur_yerel(ba->arena,
                ba->islev_adi, ba->islev_adi_uz);

        case DUGUM_CAGRI: {
            /* === R-GÖREV: _gorev_baslat(closure) -> yeni SAHIP bolgesi ===
             * Yeni gorev kendine ait bolgeyi alir; donus degeri ise gorev
             * handle'i (genelde dtam64 id) — escape ederse CAGIRAN olur. */
            if (cagri_intrinsic_mi(d, "_gorev_baslat")) {
                BolgeBilgisi *sahip = bolge_olustur_sahip(ba->arena,
                    ba->thread_id_sayaci++);
                /* Closure argumaninin govdesi bu SAHIP bolgesinde
                 * calisir — recursive analiz oraya yapilabilir. */
                (void)sahip;
                /* R-GÖREV: donus -> CAGIRAN (handle) */
                if (ba->ver_baglaminda) {
                    return bolge_olustur_cagiran(ba->arena,
                        ba->islev_adi, ba->islev_adi_uz);
                }
                return bolge_olustur_yerel(ba->arena,
                    ba->islev_adi, ba->islev_adi_uz);
            }

            /* === R-KANAL: _kanal_olustur() -> yeni KANAL bolgesi === */
            if (cagri_intrinsic_mi(d, "_kanal_olustur")) {
                return bolge_olustur_kanal(ba->arena,
                    ba->kanal_id_sayaci++);
            }

            /* _kanal_gonder(ch, val) -> deger KANAL'a transfer.
             * Sonuc: bos (LIT) — fonksiyon void. */
            if (cagri_intrinsic_mi(d, "_kanal_gonder")) {
                /* Burada gercek "transfer" semantigi var:
                 * val'in eski bolgesi terk edilir. Type system bunu
                 * enforce eder (gelecek). Su an: LIT don. */
                return bolge_olustur_basit(ba->arena, BOLGE_LIT);
            }

            /* _kanal_al(ch) -> alici thread'in SAHIP bolgesinden deger.
             * Bu fonksiyon icinde aktif gorev varsa o bolge; yoksa yerel. */
            if (cagri_intrinsic_mi(d, "_kanal_al")) {
                if (ba->aktif_gorev) return ba->aktif_gorev;
                return varsayilan_tahsis(ba);
            }

            /* _gorev_birlestir(handle) -> R-BIRLESTIR: gorev sonucu donus tipi
             * cagiranin bolgesinde — escape eder. */
            if (cagri_intrinsic_mi(d, "_gorev_birlestir")) {
                if (ba->ver_baglaminda) {
                    return bolge_olustur_cagiran(ba->arena,
                        ba->islev_adi, ba->islev_adi_uz);
                }
                return varsayilan_tahsis(ba);
            }

            /* Cagrinin sonucu: islev donus tipi
             * Varsayilan: yerel (escape ederse R-VER zaten flag aktif) */
            return varsayilan_tahsis(ba);
        }

        case DUGUM_IKILI:
        case DUGUM_TEKLI:
            /* Operator sonucu: alt ifadenin bolgesine yakin
             * Basit: varsayilan tahsis */
            return varsayilan_tahsis(ba);

        case DUGUM_ERISIM:
            /* x.y -> y'nin bolgesi x'in bolgesidir (alana dair) */
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
            /* Blok ifadesi: son deyimin bolgesi (basit) */
            int n = d->veri.blok.sayi;
            if (n == 0) return bolge_olustur_basit(ba->arena, BOLGE_LIT);
            return bolge_belirle(ba, d->veri.blok.deyimler[n - 1]);
        }

        /* === Deyimler — bolge yok ama recursive bilgi === */
        case DUGUM_VER: {
            int eski = ba->ver_baglaminda;
            ba->ver_baglaminda = 1;
            BolgeBilgisi *r = d->veri.ver.deger
                ? bolge_belirle(ba, d->veri.ver.deger)
                : bolge_olustur_basit(ba->arena, BOLGE_LIT);
            ba->ver_baglaminda = eski;
            return r;
        }

        case DUGUM_ICIN: {
            /* Yeni iterasyon bolgesi yarat */
            BolgeBilgisi *eski_it = ba->aktif_iterasyon;
            int eski_d = ba->dongu_derinligi;
            ba->aktif_iterasyon = bolge_olustur_iterasyon(ba->arena,
                ba->dongu_id_sayaci++);
            ba->dongu_derinligi++;
            BolgeBilgisi *r = bolge_belirle(ba, d->veri.icin.govde);
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

        case DUGUM_DEGISKEN:
            return bolge_belirle(ba, d->veri.degisken.deger);

        case DUGUM_ATAMA:
            return bolge_belirle(ba, d->veri.atama.deger);

        case DUGUM_GUVENSIZ:
            return bolge_belirle(ba, d->veri.guvensiz.blok);

        case DUGUM_HATA:
            return bolge_olustur_basit(ba->arena, BOLGE_HATA);

        default:
            return bolge_olustur_basit(ba->arena, BOLGE_BILINMIYOR);
    }
}
