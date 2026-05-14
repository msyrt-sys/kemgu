#include "wcet.h"
#include "hata.h"
#include "tip.h"

#include <string.h>
#include <stdio.h>

/* === Yardimcilar === */

static void rt_hata(WcetKontrol *wk, const Dugum *d,
                    const char *kod, const char *mesaj) {
    if (!d) return;
    wk->hata_sayisi++;
    hata_raporla(wk->dosya_adi, wk->kaynak,
                 d->satir, d->sutun, kod, mesaj, NULL);
}

static int identifier_es(const char *a, int a_uz, const char *b, int b_uz) {
    if (a_uz != b_uz) return 0;
    return memcmp(a, b, (size_t)a_uz) == 0;
}

/* AST walk — cost veya -1 (hata) doner. */
static int64_t walk(WcetKontrol *wk, const Dugum *d);

/* Cagri hedef adini cikar (DUGUM_TANIMLAYICI veya DUGUM_YOL). */
static int cagri_ad_cikar(const Dugum *hedef, const char **ad, int *uz) {
    if (!hedef) return 0;
    if (hedef->tip == DUGUM_TANIMLAYICI) {
        *ad = hedef->veri.tanimlayici.metin;
        *uz = hedef->veri.tanimlayici.uzunluk;
        return 1;
    }
    if (hedef->tip == DUGUM_YOL) {
        /* modul::ad — son segmenti al */
        *ad = hedef->veri.yol.sag_ad;
        *uz = hedef->veri.yol.sag_ad_uzunluk;
        return 1;
    }
    return 0;
}

static int64_t walk(WcetKontrol *wk, const Dugum *d) {
    if (!d) return 0;
    switch (d->tip) {
        /* === Sabit cost (1) === */
        case DUGUM_TAM:
        case DUGUM_KESIRLI:
        case DUGUM_METIN:
        case DUGUM_KARAKTER:
        case DUGUM_MANTIKSAL:
        case DUGUM_BOS:
        case DUGUM_TANIMLAYICI:
        case DUGUM_YOL:
            return 1;

        /* === Ikili op === */
        case DUGUM_IKILI: {
            int64_t a = walk(wk, d->veri.ikili.sol);
            int64_t b = walk(wk, d->veri.ikili.sag);
            if (a < 0 || b < 0) return -1;
            int64_t op = 1;
            switch (d->veri.ikili.op) {
                case OP_BOLU:
                case OP_MOD:
                    op = 30;
                    break;
                case OP_ESIT:
                case OP_ESIT_DEGIL:
                case OP_KUCUK:
                case OP_BUYUK:
                case OP_KUCUK_ESIT:
                case OP_BUYUK_ESIT:
                case OP_VE:
                case OP_VEYA:
                    op = 2;
                    break;
                default:
                    op = 1;
                    break;
            }
            return a + b + op;
        }

        case DUGUM_TEKLI: {
            int64_t a = walk(wk, d->veri.tekli.operand);
            if (a < 0) return -1;
            return a + 1;
        }

        /* === Cagri === */
        case DUGUM_CAGRI: {
            const char *ad = NULL;
            int ad_uz = 0;
            int has = cagri_ad_cikar(d->veri.cagri.hedef, &ad, &ad_uz);

            /* RT003 direct self-recursion */
            if (has && wk->aktif_islev &&
                identifier_es(ad, ad_uz,
                              wk->aktif_islev->veri.islev.ad,
                              wk->aktif_islev->veri.islev.ad_uzunluk)) {
                rt_hata(wk, d, "RT003",
                    "gercekzamanli islev govdesinde ozyineleme (V1 yasak)");
                return -1;
            }

            /* RT004 / RT005 — callee sembol */
            const Sembol *callee = NULL;
            if (has) {
                callee = sembol_bul(wk->global_scope, ad, ad_uz);
            }
            if (has && callee && callee->kategori == SEMBOL_ISLEV &&
                callee->tip && callee->tip->kategori == TIP_ISLEV &&
                !callee->tip->veri.islev.gercekzamanli_mi) {
                rt_hata(wk, d, "RT004",
                    "gercekzamanli islev govdesinde non-realtime cagri");
                return -1;
            }
            /* Bilinmeyen sembol (built-in olmayan, sembol tablosunda yok):
             * V1'de RT005 — WCET hesaplanamaz. Standart kutuphane
             * built-in'leri yine RT004 olmali; ama V1'de built-in flag yok. */
            if (has && !callee) {
                rt_hata(wk, d, "RT005",
                    "gercekzamanli cagri: callee bilinmiyor (WCET hesaplanamaz)");
                return -1;
            }
            if (!has) {
                /* Indirect call (lambda vs metod ifadesi) — V1: yasak */
                rt_hata(wk, d, "RT005",
                    "gercekzamanli govdede dolayli cagri (V1 yasak)");
                return -1;
            }

            /* Cost: argumanlar + callee tahmini + call/ret overhead */
            int64_t toplam = 4; /* call/ret + arg setup */
            for (int i = 0; i < d->veri.cagri.sayi; i++) {
                int64_t c = walk(wk, d->veri.cagri.argumanlar[i]);
                if (c < 0) return -1;
                toplam += c;
            }
            /* V1: callee gerceği gercekzamanli, sabit 50 cycle varsayilir.
             * V2: inter-procedural WCET propagation. */
            toplam += 50;
            return toplam;
        }

        /* === Erisim / indeks === */
        case DUGUM_ERISIM: {
            int64_t a = walk(wk, d->veri.erisim.nesne);
            if (a < 0) return -1;
            return a + 2;
        }
        case DUGUM_INDEKS: {
            int64_t a = walk(wk, d->veri.indeks.nesne);
            int64_t i = walk(wk, d->veri.indeks.indeks);
            if (a < 0 || i < 0) return -1;
            return a + i + 3;
        }

        /* === Yapi olustur (stack) === */
        case DUGUM_YAPI_OLUSTUR: {
            int64_t toplam = (int64_t)d->veri.yapi_olustur.alan_sayi * 2;
            for (int i = 0; i < d->veri.yapi_olustur.alan_sayi; i++) {
                int64_t c = walk(wk, d->veri.yapi_olustur.alanlar[i]);
                if (c < 0) return -1;
                toplam += c;
            }
            return toplam;
        }
        case DUGUM_ALAN_ATAMA:
            return walk(wk, d->veri.alan_atama.deger);

        /* === RT001 — dynamic allocation === */
        case DUGUM_DIZI_OLUSTUR:
            rt_hata(wk, d, "RT001",
                "gercekzamanli islev govdesinde dinamik dizi tahsisi");
            return -1;

        case DUGUM_LAMBDA:
            rt_hata(wk, d, "RT001",
                "gercekzamanli islev govdesinde lambda (closure tahsis)");
            return -1;

        /* === RT002 — loops (V1: tum durumda yasak) === */
        case DUGUM_IKEN:
            rt_hata(wk, d, "RT002",
                "gercekzamanli islev govdesinde 'iken' loop (V1 yasak)");
            return -1;
        case DUGUM_ICIN:
            rt_hata(wk, d, "RT002",
                "gercekzamanli islev govdesinde 'icin' loop (V1 yasak)");
            return -1;

        /* === Kontrol akisi === */
        case DUGUM_EGER: {
            int64_t k = walk(wk, d->veri.eger.kosul);
            int64_t a = walk(wk, d->veri.eger.gozdoldur);
            int64_t b = d->veri.eger.yan ? walk(wk, d->veri.eger.yan) : 0;
            if (k < 0 || a < 0 || b < 0) return -1;
            return k + (a > b ? a : b) + 1;
        }
        case DUGUM_VER: {
            int64_t a = d->veri.ver.deger ? walk(wk, d->veri.ver.deger) : 0;
            if (a < 0) return -1;
            return a + 1;
        }

        /* === Eslesme (pattern matching) === */
        case DUGUM_ESLES: {
            int64_t v = walk(wk, d->veri.esles.deger);
            if (v < 0) return -1;
            int64_t maks = 0;
            for (int i = 0; i < d->veri.esles.kol_sayi; i++) {
                int64_t c = walk(wk, d->veri.esles.kollar[i]);
                if (c < 0) return -1;
                if (c > maks) maks = c;
            }
            return v + maks + d->veri.esles.kol_sayi;
        }
        case DUGUM_ESLES_KOLU:
            return walk(wk, d->veri.esles_kolu.govde);

        case DUGUM_GUVENSIZ:
            return walk(wk, d->veri.guvensiz.blok);

        /* === Blok / atama / deyim === */
        case DUGUM_BLOK: {
            int64_t toplam = 0;
            for (int i = 0; i < d->veri.blok.sayi; i++) {
                int64_t c = walk(wk, d->veri.blok.deyimler[i]);
                if (c < 0) return -1;
                toplam += c;
            }
            return toplam;
        }
        case DUGUM_DEGISKEN: {
            int64_t v = d->veri.degisken.deger ? walk(wk, d->veri.degisken.deger) : 0;
            if (v < 0) return -1;
            return v + 1;
        }
        case DUGUM_ATAMA: {
            int64_t l = walk(wk, d->veri.atama.hedef);
            int64_t r = walk(wk, d->veri.atama.deger);
            if (l < 0 || r < 0) return -1;
            return l + r + 1;
        }
        case DUGUM_IFADE_DEYIMI:
            return walk(wk, d->veri.ifade_deyimi.ifade);

        /* === Linear consume — sabit cost === */
        case DUGUM_KULLAN_IFADE: {
            int64_t a = walk(wk, d->veri.kullan_ifade.operand);
            if (a < 0) return -1;
            return a + 1;
        }
        case DUGUM_IMHA_IFADE: {
            int64_t a = walk(wk, d->veri.imha_ifade.operand);
            if (a < 0) return -1;
            return a + 1;
        }
        case DUGUM_TIP_DONUSTUR: {
            int64_t a = walk(wk, d->veri.tip_donustur.kaynak);
            if (a < 0) return -1;
            return a + 1;
        }

        /* === Tip dugumleri — govde icinde gozlemlenirse parametre annot
         * gibi yer tutucu; cost 0 (komut uretmez) === */
        case DUGUM_TIP_BASIT:
        case DUGUM_TIP_REFERANS:
        case DUGUM_TIP_POINTER:
        case DUGUM_TIP_DIZI:
        case DUGUM_TIP_SECIMLIK:
        case DUGUM_TIP_SONUC:
        case DUGUM_TIP_ISLEV:
        case DUGUM_TIP_KULLANICI:
        case DUGUM_TIP_TEKKEZ:
        case DUGUM_TIP_SABITSURE:
        case DUGUM_PARAMETRE:
        case DUGUM_ALAN:
            return 0;

        /* === Hata / yer tutucu === */
        case DUGUM_HATA:
            return -1;

        default:
            return 0;
    }
}

void wcet_kontrol_baslat(WcetKontrol *wk, Arena *a, Scope *global,
                         const char *dosya_adi, const char *kaynak) {
    if (!wk) return;
    wk->arena = a;
    wk->global_scope = global;
    wk->hata_sayisi = 0;
    wk->dosya_adi = dosya_adi;
    wk->kaynak = kaynak;
    wk->aktif_islev = NULL;
}

int64_t wcet_islev_hesapla(WcetKontrol *wk, const Dugum *islev) {
    if (!islev || islev->tip != DUGUM_ISLEV) return -1;
    if (!islev->veri.islev.govde) return 0; /* imza yeterli, gövdesiz */
    const Dugum *eski = wk->aktif_islev;
    wk->aktif_islev = islev;
    int64_t r = walk(wk, islev->veri.islev.govde);
    wk->aktif_islev = eski;
    return r;
}

/* Modul / uygula / disa wrap'larini acarak ic islev'i bul. */
static void taran(WcetKontrol *wk, const Dugum *u) {
    if (!u) return;
    const Dugum *act = u;
    if (act->tip == DUGUM_DISA) act = act->veri.disa.tanim;
    if (!act) return;
    if (act->tip == DUGUM_ISLEV && act->veri.islev.gercekzamanli_mi) {
        wcet_islev_hesapla(wk, act);
        return;
    }
    if (act->tip == DUGUM_MODUL) {
        for (int j = 0; j < act->veri.modul.sayi; j++) {
            taran(wk, act->veri.modul.uyeler[j]);
        }
        return;
    }
    if (act->tip == DUGUM_UYGULA) {
        for (int j = 0; j < act->veri.uygula.islev_sayi; j++) {
            const Dugum *m = act->veri.uygula.islevler[j];
            if (m && m->tip == DUGUM_ISLEV && m->veri.islev.gercekzamanli_mi) {
                wcet_islev_hesapla(wk, m);
            }
        }
        return;
    }
    if (act->tip == DUGUM_OZELLIK) {
        /* Default impl gövdeleri */
        for (int j = 0; j < act->veri.ozellik.uye_sayi; j++) {
            const Dugum *m = act->veri.ozellik.uyeler[j];
            if (m && m->tip == DUGUM_ISLEV && m->veri.islev.gercekzamanli_mi
                && m->veri.islev.govde) {
                wcet_islev_hesapla(wk, m);
            }
        }
    }
}

void wcet_kontrol_program(WcetKontrol *wk, const Dugum *program) {
    if (!wk || !program) return;
    if (program->tip != DUGUM_PROGRAM) {
        taran(wk, program);
        return;
    }
    for (int i = 0; i < program->veri.program.sayi; i++) {
        taran(wk, program->veri.program.uyeler[i]);
    }
}
