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
    ba->escape = NULL;
}

void bolge_atama_escape_bagla(BolgeAtama *ba, const EscapeAnaliz *ea) {
    ba->escape = ea;
}

/* Escape sonucundan bolge kategorisine donusum */
static BolgeBilgisi *escape_to_bolge(BolgeAtama *ba, EscapeKategorisi ek) {
    switch (ek) {
        case ESC_CAGIRAN:
            return bolge_olustur_cagiran(ba->arena,
                ba->islev_adi, ba->islev_adi_uz);
        case ESC_ITERASYON:
            if (ba->aktif_iterasyon) return ba->aktif_iterasyon;
            return bolge_olustur_iterasyon(ba->arena, 0);
        case ESC_YEREL:
        default:
            return bolge_olustur_yerel(ba->arena,
                ba->islev_adi, ba->islev_adi_uz);
    }
}

/* Yardimci: tahsis bolgesi belirle.
 *
 * Eger escape analizi bagliysa, AST dugumune ait escape kategorisi sorgulanir;
 * yoksa syntax tabanli ver_baglaminda/dongu_derinligi heuristikine duser. */
static BolgeBilgisi *varsayilan_tahsis_dugum(BolgeAtama *ba, const Dugum *d) {
    if (ba->escape && d) {
        EscapeKategorisi ek = escape_kategori(ba->escape, d);
        /* Eger escape ESC_YEREL diyorsa ama ver_baglaminda 1'se,
         * yine de ver baglamini tercih et (escape analizinin gozden kacirdigi
         * durumlara karsi guvenli taraf). */
        if (ek == ESC_YEREL && ba->ver_baglaminda) {
            return bolge_olustur_cagiran(ba->arena,
                ba->islev_adi, ba->islev_adi_uz);
        }
        return escape_to_bolge(ba, ek);
    }
    /* Syntax tabanli fallback (eski davranis) */
    if (ba->ver_baglaminda) {
        return bolge_olustur_cagiran(ba->arena,
            ba->islev_adi, ba->islev_adi_uz);
    }
    if (ba->dongu_derinligi > 0 && ba->aktif_iterasyon) {
        return ba->aktif_iterasyon;
    }
    return bolge_olustur_yerel(ba->arena,
        ba->islev_adi, ba->islev_adi_uz);
}

/* Geriye uyumluluk: dugum bilmeyen versiyon (yapi_olusturma sonek vb. icin) */
static BolgeBilgisi *varsayilan_tahsis(BolgeAtama *ba) {
    return varsayilan_tahsis_dugum(ba, NULL);
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

        /* === R-YEREL veya R-VER: bilesik literal ===
         * Escape analizi varsa dogrudan dugumden okur. */
        case DUGUM_METIN:
        case DUGUM_DIZI_OLUSTUR:
        case DUGUM_YAPI_OLUSTUR:
        case DUGUM_LAMBDA:
            return varsayilan_tahsis_dugum(ba, d);

        case DUGUM_CAGRI: {
            /* Cagri sonucu: escape analizinde kayitli (varsa). */
            BolgeBilgisi *r = varsayilan_tahsis_dugum(ba, d);
            return r;
        }

        /* === Tanimlayici / cagri / erisim / indeks: bagimli === */
        case DUGUM_TANIMLAYICI:
            /* Sembolun bolgesi (sembol tablosunda saklanmiyor su an,
             * default yerel) */
            return bolge_olustur_yerel(ba->arena,
                ba->islev_adi, ba->islev_adi_uz);

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
