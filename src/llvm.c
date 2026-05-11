#include "llvm.h"

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

/*
 * KEMGU LLVM IR Backend (text uretici, ADIM A — genisletilmis)
 * ============================================================
 *
 * Kapsam:
 *   A.1 Parametreli islevler              ✓
 *   A.2 Yerel degisken + atama (alloca)   ✓
 *   A.3 Kontrol akisi (eger, iken)        ✓
 *   A.4 Cagri + recursive                 ✓
 *   A.5 Karsilastirma + mantiksal         ✓
 *   A.6 Yapi (struct)                     ✓
 *   A.7 Dizi (sabit boyut)                ✓
 *   A.8 Karakter + string                 ✓
 *
 * Pipeline:
 *   KEMGU kaynak -> AST -> bu modul -> LLVM IR text -> clang -x ir - -> exe
 *
 * Tip esleme (KEMGU -> LLVM):
 *   tam8/16/32/64    -> i8/i16/i32/i64
 *   dtam*            -> i8/i16/i32/i64 (signedness IR'de op seviyesinde)
 *   karakter         -> i32 (Unicode code point)
 *   mantiksal        -> i1
 *   metin            -> ptr (i8*)
 *   bos              -> void
 *   kesirli32/64     -> float / double
 *   &T, *T           -> ptr (opaque)
 *   Dizi<T>          -> ptr (heap'te N x T; basit modelde sabit boyut)
 *   Yapi             -> %struct.Ad (value), genelde alloca + ptr olarak
 *
 * Sembol haritasi: islev icindeki yerel degiskenler ad -> alloca register
 * eslemesi tutulur. Scope giris/cikis sembol_sayi watermark ile yonetilir.
 * Parametreler de entry blokta alloca + store ile lokal degiskene cevrilir
 * (Clang konvansiyonu — read/write tekdüze).
 *
 * Blok terminator yonetimi: her basic block bir terminator ile bitmeli
 * (ret/br/unreachable). Codegen mevcut blok terminate mi izler; eger
 * terminate olmussa sonraki kod ya yeni bir blokta yazilir ya da atlanir.
 */

#define MAX_LOKAL      256
#define MAX_YAPI       64
#define MAX_YAPI_ALAN  32
#define MAX_STR        256
#define MAX_ISLEV      256
#define MAX_ALIAS      64
#define ISIM_BUF       128

typedef struct {
    const char *ad;
    int ad_uzunluk;
    char addr[ISIM_BUF];   /* alloca register: %x.addr veya %x */
    char tip[ISIM_BUF];    /* LLVM tip: i32, %struct.Nokta, ptr, ... */
} Sembol;

typedef struct {
    const char *ad;
    int ad_uzunluk;
    char ascii_ad[ISIM_BUF];   /* %struct.<ascii_ad> icin */
    struct {
        const char *ad;
        int ad_uzunluk;
        char tip[ISIM_BUF];
    } alanlar[MAX_YAPI_ALAN];
    int alan_sayi;
} YapiKayit;

typedef struct {
    const char *metin;
    int uzunluk;
    int id;
} StrKayit;

typedef struct {
    const char *ad;
    int ad_uzunluk;
    char ascii_ad[ISIM_BUF];   /* LLVM tarafi ascii ad */
    char donus[ISIM_BUF];
    char params[MAX_YAPI_ALAN][ISIM_BUF];  /* parametre LLVM tipleri */
    int param_sayi;
} IslevKayit;

/* B.4: tip alias kaydi — Ad -> cozulmus LLVM tip stringi */
typedef struct {
    const char *ad;
    int ad_uzunluk;
    char hedef_llvm[ISIM_BUF];
} AliasKayit;

/* Built-in (stdlib) islev tanim — KEMGU adi <-> LLVM (C runtime) adi.
 * Bu islevler kullanici tarafindan tanimlanmaz; LLVM modulu basinda
 * declare ile bildirilir ve runtime (kdl_runtime.c) ile link edilir. */
typedef struct {
    const char *kemgu_ad;       /* UTF-8 byte dizisi */
    const char *llvm_ad;        /* C tarafi sembolu */
    const char *donus;          /* LLVM tip */
    int param_sayi;
    const char *params[4];      /* LLVM param tipleri (max 4) */
} BuiltinTanim;

/* yazd\xc4\xb1r -> "yazdır" — \xb1 sonrasi 'r' hex degil, guvenli.
 * Diger tum esleskler: \xb1 sonra _ veya k veya r (hex degil). */
static const BuiltinTanim BUILTINLER[] = {
    /* D.1 IO — yazdir varyantlari (newline ekler) */
    { "yazd\xc4\xb1r",                "kdl_yazdir_metin",     "void", 1, {"ptr"} },
    { "yazd\xc4\xb1r_tam",            "kdl_yazdir_tam",       "void", 1, {"i32"} },
    { "yazd\xc4\xb1r_tam64",          "kdl_yazdir_tam64",     "void", 1, {"i64"} },
    { "yazd\xc4\xb1r_kesirli",        "kdl_yazdir_kesirli",   "void", 1, {"double"} },
    { "yazd\xc4\xb1r_mant\xc4\xb1ksal","kdl_yazdir_mantiksal","void", 1, {"i1"} },
    { "yazd\xc4\xb1r_karakter",       "kdl_yazdir_karakter",  "void", 1, {"i32"} },
    { "yazd\xc4\xb1r_sat\xc4\xb1r",   "kdl_yazdir_satir",     "void", 0, {0} },
    /* yaz_* (newline yok) */
    { "yaz",                           "kdl_yaz_metin",       "void", 1, {"ptr"} },
    { "yaz_tam",                       "kdl_yaz_tam",         "void", 1, {"i32"} },
    { "yaz_karakter",                  "kdl_yaz_karakter",    "void", 1, {"i32"} },
    /* Hata cikis */
    { "hata_yazd\xc4\xb1r",           "kdl_hata_yazdir",      "void", 1, {"ptr"} },
    /* Okuma */
    { "oku_tam",                       "kdl_oku_tam",         "i32",  0, {0} },
    /* D.3 Metin */
    { "metin_uzunluk",                 "kdl_metin_uzunluk",   "i32",  1, {"ptr"} },
    /* D.4 Sayisal */
    { "mutlak",                        "kdl_mutlak",          "i32",  1, {"i32"} },
    { "min",                           "kdl_min",             "i32",  2, {"i32","i32"} },
    { "maks",                          "kdl_maks",            "i32",  2, {"i32","i32"} },
    { "mutlak64",                      "kdl_mutlak64",        "i64",  1, {"i64"} },
    { "min64",                         "kdl_min64",           "i64",  2, {"i64","i64"} },
    { "maks64",                        "kdl_maks64",          "i64",  2, {"i64","i64"} },
};
#define BUILTIN_SAYI (int)(sizeof(BUILTINLER) / sizeof(BUILTINLER[0]))

static const BuiltinTanim *builtin_bul(const char *ad, int u) {
    for (int i = 0; i < BUILTIN_SAYI; i++) {
        size_t bl = strlen(BUILTINLER[i].kemgu_ad);
        if ((size_t)u == bl &&
            memcmp(ad, BUILTINLER[i].kemgu_ad, bl) == 0) {
            return &BUILTINLER[i];
        }
    }
    return NULL;
}

typedef struct {
    FILE *out;
    int reg;
    int blok_id;
    int str_id;

    Sembol semboller[MAX_LOKAL];
    int sembol_sayi;

    YapiKayit yapilar[MAX_YAPI];
    int yapi_sayi;

    StrKayit stringler[MAX_STR];
    int string_sayi;

    IslevKayit islevler[MAX_ISLEV];
    int islev_sayi;

    AliasKayit aliases[MAX_ALIAS];
    int alias_sayi;

    int block_terminated;
    char aktif_donus[ISIM_BUF];  /* mevcut islev donus tipi */
} Codegen;

typedef struct {
    int reg;                 /* -1 hata */
    char tip[ISIM_BUF];      /* LLVM type */
    int is_lvalue;           /* 1 ise reg pointer'a isaret eder (alloca/gep) */
    char lvalue_tip[ISIM_BUF]; /* lvalue ise hedef deger tipi */
} IfadeSonuc;

/* === Yardimcilar === */

static int ad_eslesir(const char *a, int au, const char *b) {
    size_t bl = strlen(b);
    return (size_t)au == bl && memcmp(a, b, bl) == 0;
}

static int yeni_reg(Codegen *c) { return c->reg++; }
static int yeni_blok(Codegen *c) { return c->blok_id++; }

/* KEMGU adlarini (UTF-8) LLVM identifier'ina (ASCII) cevir.
 * LLVM identifier kurallari: [a-zA-Z_$.][a-zA-Z0-9_$.]*; UTF-8 byte'i
 * quoted olmayan adda yasak. Turkce karakterleri ASCII karsiliklarina
 * transliterate eder. */
static void ad_ascii_yap(const char *kaynak, int uzunluk,
                          char *cikti, size_t cikti_max) {
    size_t out = 0;
    int i = 0;
    while (i < uzunluk && out + 1 < cikti_max) {
        unsigned char c = (unsigned char)kaynak[i];
        if (c < 0x80) {
            cikti[out++] = (char)c;
            i++;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < uzunluk) {
            unsigned int cp =
                (((unsigned int)(c & 0x1F)) << 6) |
                ((unsigned int)((unsigned char)kaynak[i+1]) & 0x3Fu);
            char r;
            switch (cp) {
                case 0x00E7: r = 'c'; break;
                case 0x00C7: r = 'C'; break;
                case 0x011F: r = 'g'; break;
                case 0x011E: r = 'G'; break;
                case 0x0131: r = 'i'; break;
                case 0x0130: r = 'I'; break;
                case 0x00F6: r = 'o'; break;
                case 0x00D6: r = 'O'; break;
                case 0x015F: r = 's'; break;
                case 0x015E: r = 'S'; break;
                case 0x00FC: r = 'u'; break;
                case 0x00DC: r = 'U'; break;
                default:     r = '_'; break;
            }
            cikti[out++] = r;
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            cikti[out++] = '_';
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            cikti[out++] = '_';
            i += 4;
        } else {
            cikti[out++] = '_';
            i++;
        }
    }
    cikti[out] = 0;
}

/* KEMGU tipini LLVM tip stringine cevir. yapi adlari basit tip olarak
 * gelir (parser DUGUM_TIP_BASIT'e koyar), kullanici tipi mi diye
 * yapi_kayit listesiyle kontrol ederiz. */
static const YapiKayit *yapi_bul(Codegen *c, const char *ad, int u) {
    for (int i = 0; i < c->yapi_sayi; i++) {
        if (c->yapilar[i].ad_uzunluk == u &&
            memcmp(c->yapilar[i].ad, ad, (size_t)u) == 0) {
            return &c->yapilar[i];
        }
    }
    return NULL;
}

/* Sembol tipi "%struct.<ascii_ad>" formatinda — ascii ad ile yapiyi bul. */
static const YapiKayit *yapi_bul_ascii(Codegen *c, const char *ascii_ad) {
    for (int i = 0; i < c->yapi_sayi; i++) {
        if (strcmp(c->yapilar[i].ascii_ad, ascii_ad) == 0) {
            return &c->yapilar[i];
        }
    }
    return NULL;
}

static void kemgu_tip_to_llvm(Codegen *c, const Dugum *tip,
                              char *cikti, size_t cikti_max) {
    if (!tip) { snprintf(cikti, cikti_max, "void"); return; }

    if (tip->tip == DUGUM_TIP_BASIT) {
        const char *ad = tip->veri.tip_basit.ad;
        int u = tip->veri.tip_basit.ad_uzunluk;

        if (ad_eslesir(ad, u, "tam8")  || ad_eslesir(ad, u, "dtam8"))
            { snprintf(cikti, cikti_max, "i8");  return; }
        if (ad_eslesir(ad, u, "tam16") || ad_eslesir(ad, u, "dtam16"))
            { snprintf(cikti, cikti_max, "i16"); return; }
        if (ad_eslesir(ad, u, "tam32") || ad_eslesir(ad, u, "dtam32"))
            { snprintf(cikti, cikti_max, "i32"); return; }
        if (ad_eslesir(ad, u, "tam64") || ad_eslesir(ad, u, "dtam64"))
            { snprintf(cikti, cikti_max, "i64"); return; }
        if (ad_eslesir(ad, u, "karakter"))
            { snprintf(cikti, cikti_max, "i32"); return; }
        /* mantiksal: "mant\xc4\xb1ksal" 10 byte */
        if (u == 10 && memcmp(ad, "mant\xc4\xb1" "ksal", 10) == 0)
            { snprintf(cikti, cikti_max, "i1"); return; }
        if (ad_eslesir(ad, u, "metin"))
            { snprintf(cikti, cikti_max, "ptr"); return; }
        /* bos: "bo\xc5\x9f" 4 byte */
        if (u == 4 && memcmp(ad, "bo\xc5\x9f", 4) == 0)
            { snprintf(cikti, cikti_max, "void"); return; }
        if (ad_eslesir(ad, u, "kesirli32"))
            { snprintf(cikti, cikti_max, "float"); return; }
        if (ad_eslesir(ad, u, "kesirli64"))
            { snprintf(cikti, cikti_max, "double"); return; }

        /* Bilinmeyen basit tip — yapi adi mi? */
        const YapiKayit *yk = yapi_bul(c, ad, u);
        if (yk) {
            snprintf(cikti, cikti_max, "%%struct.%s", yk->ascii_ad);
            return;
        }
        /* B.4: tip alias mi? */
        for (int i = 0; i < c->alias_sayi; i++) {
            if (c->aliases[i].ad_uzunluk == u &&
                memcmp(c->aliases[i].ad, ad, (size_t)u) == 0) {
                snprintf(cikti, cikti_max, "%s", c->aliases[i].hedef_llvm);
                return;
            }
        }
        snprintf(cikti, cikti_max, "i32"); /* fallback */
        return;
    }

    if (tip->tip == DUGUM_TIP_REFERANS ||
        tip->tip == DUGUM_TIP_POINTER) {
        snprintf(cikti, cikti_max, "ptr");
        return;
    }
    if (tip->tip == DUGUM_TIP_DIZI) {
        snprintf(cikti, cikti_max, "ptr");
        return;
    }
    if (tip->tip == DUGUM_TIP_KULLANICI) {
        const Dugum *yol = tip->veri.tip_kullanici.yol;
        if (yol && yol->tip == DUGUM_TANIMLAYICI) {
            const YapiKayit *yk = yapi_bul(c,
                yol->veri.tanimlayici.metin,
                yol->veri.tanimlayici.uzunluk);
            if (yk) {
                snprintf(cikti, cikti_max, "%%struct.%s", yk->ascii_ad);
                return;
            }
        }
        snprintf(cikti, cikti_max, "ptr");
        return;
    }

    snprintf(cikti, cikti_max, "i32"); /* en son fallback */
}

/* === Sembol tablosu === */

static int sembol_ekle(Codegen *c, const char *ad, int u,
                       const char *addr, const char *tip) {
    if (c->sembol_sayi >= MAX_LOKAL) return -1;
    Sembol *s = &c->semboller[c->sembol_sayi];
    s->ad = ad;
    s->ad_uzunluk = u;
    snprintf(s->addr, ISIM_BUF, "%s", addr);
    snprintf(s->tip, ISIM_BUF, "%s", tip);
    return c->sembol_sayi++;
}

static Sembol *sembol_bul(Codegen *c, const char *ad, int u) {
    for (int i = c->sembol_sayi - 1; i >= 0; i--) {
        if (c->semboller[i].ad_uzunluk == u &&
            memcmp(c->semboller[i].ad, ad, (size_t)u) == 0) {
            return &c->semboller[i];
        }
    }
    return NULL;
}

/* === Yapi kayitlari (yapi_bul yukarida) === */

static int yapi_alan_indeks(const YapiKayit *yk, const char *ad, int u) {
    for (int i = 0; i < yk->alan_sayi; i++) {
        if (yk->alanlar[i].ad_uzunluk == u &&
            memcmp(yk->alanlar[i].ad, ad, (size_t)u) == 0) {
            return i;
        }
    }
    return -1;
}

/* === Islev kayitlari (signature lookup) === */

static const IslevKayit *islev_bul(Codegen *c, const char *ad, int u) {
    for (int i = 0; i < c->islev_sayi; i++) {
        if (c->islevler[i].ad_uzunluk == u &&
            memcmp(c->islevler[i].ad, ad, (size_t)u) == 0) {
            return &c->islevler[i];
        }
    }
    return NULL;
}

/* === String literal listesi (deferred global emit) === */

static int string_ekle(Codegen *c, const char *metin, int u) {
    /* Ayni stringi iki kez kaydetmeye gerek yok — basit dedup */
    for (int i = 0; i < c->string_sayi; i++) {
        if (c->stringler[i].uzunluk == u &&
            memcmp(c->stringler[i].metin, metin, (size_t)u) == 0) {
            return c->stringler[i].id;
        }
    }
    if (c->string_sayi >= MAX_STR) return -1;
    int id = c->str_id++;
    c->stringler[c->string_sayi].metin = metin;
    c->stringler[c->string_sayi].uzunluk = u;
    c->stringler[c->string_sayi].id = id;
    c->string_sayi++;
    return id;
}

/* === Forward decl === */

static IfadeSonuc ifade_uret(Codegen *c, const Dugum *d);
static void deyim_uret(Codegen *c, const Dugum *d);

/* === Sayisal tip helper === */

static int integer_tip_mi(const char *tip) {
    return tip[0] == 'i' && tip[1] >= '0' && tip[1] <= '9';
}
static int float_tip_mi(const char *tip) {
    return strcmp(tip, "float") == 0 || strcmp(tip, "double") == 0;
}

/* Iki integer tip arasi donusum: i1 -> kapsamli (zext), digerleri sext/trunc.
 * src_reg ve src tipi ile dst tipini eslestirir; gerekirse yeni reg uretir
 * ve donurur. Donusum gerekmezse src_reg geri doner. */
static int int_cevir(Codegen *c, int src_reg,
                     const char *src, const char *dst) {
    if (strcmp(src, dst) == 0) return src_reg;
    if (!integer_tip_mi(src) || !integer_tip_mi(dst)) return src_reg;
    int sb = atoi(src + 1);
    int db = atoi(dst + 1);
    int newr = yeni_reg(c);
    if (sb < db) {
        /* i1 -> daha buyuk: zext (mantiksal degeri korur) */
        if (sb == 1) {
            fprintf(c->out, "  %%%d = zext %s %%%d to %s\n",
                    newr, src, src_reg, dst);
        } else {
            fprintf(c->out, "  %%%d = sext %s %%%d to %s\n",
                    newr, src, src_reg, dst);
        }
    } else if (sb > db) {
        fprintf(c->out, "  %%%d = trunc %s %%%d to %s\n",
                newr, src, src_reg, dst);
    } else {
        return src_reg;
    }
    return newr;
}

/* Bos sonuc */
static IfadeSonuc hata_sonuc(void) {
    IfadeSonuc s;
    s.reg = -1;
    s.tip[0] = 0;
    s.is_lvalue = 0;
    s.lvalue_tip[0] = 0;
    return s;
}

/* === Ifade uretimi === */

static IfadeSonuc ifade_uret(Codegen *c, const Dugum *d) {
    if (!d) return hata_sonuc();
    if (c->block_terminated) return hata_sonuc();

    switch (d->tip) {
    case DUGUM_TAM: {
        int r = yeni_reg(c);
        fprintf(c->out, "  %%%d = add i32 0, %" PRId64 "\n",
                r, d->veri.tam.deger);
        IfadeSonuc s;
        s.reg = r;
        snprintf(s.tip, ISIM_BUF, "i32");
        s.is_lvalue = 0;
        s.lvalue_tip[0] = 0;
        return s;
    }

    case DUGUM_MANTIKSAL: {
        int r = yeni_reg(c);
        fprintf(c->out, "  %%%d = add i1 0, %d\n",
                r, d->veri.mantiksal.deger ? 1 : 0);
        IfadeSonuc s;
        s.reg = r;
        snprintf(s.tip, ISIM_BUF, "i1");
        s.is_lvalue = 0;
        s.lvalue_tip[0] = 0;
        return s;
    }

    case DUGUM_KARAKTER: {
        int r = yeni_reg(c);
        fprintf(c->out, "  %%%d = add i32 0, %u\n",
                r, d->veri.karakter.kod_noktasi);
        IfadeSonuc s;
        s.reg = r;
        snprintf(s.tip, ISIM_BUF, "i32");
        s.is_lvalue = 0;
        s.lvalue_tip[0] = 0;
        return s;
    }

    case DUGUM_METIN: {
        int id = string_ekle(c, d->veri.metin_lit.metin,
                             d->veri.metin_lit.uzunluk);
        int r = yeni_reg(c);
        /* String global'i ptr olarak referans et */
        fprintf(c->out,
            "  %%%d = getelementptr inbounds [%d x i8], ptr @.str.%d, i32 0, i32 0\n",
            r, d->veri.metin_lit.uzunluk + 1, id);
        IfadeSonuc s;
        s.reg = r;
        snprintf(s.tip, ISIM_BUF, "ptr");
        s.is_lvalue = 0;
        s.lvalue_tip[0] = 0;
        return s;
    }

    case DUGUM_TANIMLAYICI: {
        const char *ad = d->veri.tanimlayici.metin;
        int u = d->veri.tanimlayici.uzunluk;
        Sembol *s = sembol_bul(c, ad, u);
        if (!s) {
            fprintf(c->out, "  ; HATA: tanimsiz sembol %.*s\n", u, ad);
            return hata_sonuc();
        }
        /* Sembol register'i (alloca veya gep adresi) - integer alloca register ID
         * formatinda saklanmadi; tam string ile saklandi. */
        IfadeSonuc out;
        out.reg = 0;  /* unused — addr stringi direkt yazacagiz */
        out.is_lvalue = 1;
        snprintf(out.tip, ISIM_BUF, "ptr");
        snprintf(out.lvalue_tip, ISIM_BUF, "%s", s->tip);
        /* Reg yerine: addr stringini kullaniyoruz. Sonra yuklerken
         * load <tip>, ptr <addr> diye yazariz. Ama IfadeSonuc.reg int.
         * Cozum: yeni bir reg'e GEP olmadan direkt addr alalim.
         * Aslinda alloca register zaten ptr. Onu reg'e atamak icin
         * %X = bitcast yapmaya gerek yok. Bu durumu ozel ele alalim:
         * ozel "addr stringi" tasiyan ek alan ya da hemen load yapalim. */
        /* Basitlestirme: hemen load yap, lvalue'yu sembolden olusan
         * GEP yerine olusturanin sorumlulugu. Atama icin ayri yol. */
        int r = yeni_reg(c);
        fprintf(c->out, "  %%%d = load %s, ptr %s\n", r, s->tip, s->addr);
        out.reg = r;
        out.is_lvalue = 0;
        snprintf(out.tip, ISIM_BUF, "%s", s->tip);
        out.lvalue_tip[0] = 0;
        return out;
    }

    case DUGUM_TEKLI: {
        Operator op = d->veri.tekli.op;
        if (op == OP_REF || op == OP_REF_DEGISKEN) {
            /* &x: tanimlayici ise alloca adresini doner */
            const Dugum *op_d = d->veri.tekli.operand;
            if (op_d && op_d->tip == DUGUM_TANIMLAYICI) {
                Sembol *s = sembol_bul(c, op_d->veri.tanimlayici.metin,
                                       op_d->veri.tanimlayici.uzunluk);
                if (!s) return hata_sonuc();
                /* Adresi zaten %s->addr — bu bir ptr, hicbir instr olusturmadan
                 * yeni reg'e atayalim: gep idx 0 */
                int r = yeni_reg(c);
                fprintf(c->out, "  %%%d = getelementptr %s, ptr %s, i32 0\n",
                        r, s->tip, s->addr);
                IfadeSonuc out;
                out.reg = r;
                snprintf(out.tip, ISIM_BUF, "ptr");
                out.is_lvalue = 0;
                out.lvalue_tip[0] = 0;
                return out;
            }
            fprintf(c->out, "  ; HATA: & sadece tanimlayici icin destekli\n");
            return hata_sonuc();
        }
        if (op == OP_DEREFERANS) {
            IfadeSonuc x = ifade_uret(c, d->veri.tekli.operand);
            if (x.reg < 0) return hata_sonuc();
            /* x ptr olmali; load yap — ama hedef tipi bilmiyoruz. i32 varsay. */
            int r = yeni_reg(c);
            fprintf(c->out, "  %%%d = load i32, ptr %%%d\n", r, x.reg);
            IfadeSonuc out;
            out.reg = r;
            snprintf(out.tip, ISIM_BUF, "i32");
            out.is_lvalue = 0;
            out.lvalue_tip[0] = 0;
            return out;
        }
        IfadeSonuc x = ifade_uret(c, d->veri.tekli.operand);
        if (x.reg < 0) return hata_sonuc();
        int r = yeni_reg(c);
        if (op == OP_NEG) {
            if (float_tip_mi(x.tip)) {
                fprintf(c->out, "  %%%d = fneg %s %%%d\n", r, x.tip, x.reg);
            } else {
                fprintf(c->out, "  %%%d = sub %s 0, %%%d\n", r, x.tip, x.reg);
            }
            IfadeSonuc out;
            out.reg = r;
            snprintf(out.tip, ISIM_BUF, "%s", x.tip);
            out.is_lvalue = 0;
            out.lvalue_tip[0] = 0;
            return out;
        }
        if (op == OP_DEGIL) {
            fprintf(c->out, "  %%%d = xor i1 %%%d, 1\n", r, x.reg);
            IfadeSonuc out;
            out.reg = r;
            snprintf(out.tip, ISIM_BUF, "i1");
            out.is_lvalue = 0;
            out.lvalue_tip[0] = 0;
            return out;
        }
        return hata_sonuc();
    }

    case DUGUM_IKILI: {
        Operator op = d->veri.ikili.op;
        IfadeSonuc l = ifade_uret(c, d->veri.ikili.sol);
        IfadeSonuc r = ifade_uret(c, d->veri.ikili.sag);
        if (l.reg < 0 || r.reg < 0) return hata_sonuc();

        /* Karsilastirma operatorleri — icmp/fcmp, sonuc i1 */
        const char *icmp_pred = NULL;
        const char *fcmp_pred = NULL;
        switch (op) {
            case OP_ESIT:        icmp_pred = "eq";  fcmp_pred = "oeq"; break;
            case OP_ESIT_DEGIL:  icmp_pred = "ne";  fcmp_pred = "one"; break;
            case OP_KUCUK:       icmp_pred = "slt"; fcmp_pred = "olt"; break;
            case OP_BUYUK:       icmp_pred = "sgt"; fcmp_pred = "ogt"; break;
            case OP_KUCUK_ESIT:  icmp_pred = "sle"; fcmp_pred = "ole"; break;
            case OP_BUYUK_ESIT:  icmp_pred = "sge"; fcmp_pred = "oge"; break;
            default: break;
        }
        if (icmp_pred) {
            int rr = yeni_reg(c);
            if (float_tip_mi(l.tip)) {
                fprintf(c->out, "  %%%d = fcmp %s %s %%%d, %%%d\n",
                        rr, fcmp_pred, l.tip, l.reg, r.reg);
            } else {
                fprintf(c->out, "  %%%d = icmp %s %s %%%d, %%%d\n",
                        rr, icmp_pred, l.tip, l.reg, r.reg);
            }
            IfadeSonuc out;
            out.reg = rr;
            snprintf(out.tip, ISIM_BUF, "i1");
            out.is_lvalue = 0;
            out.lvalue_tip[0] = 0;
            return out;
        }

        /* Mantiksal — operandlar i1 olmali */
        if (op == OP_VE || op == OP_VEYA) {
            int rr = yeni_reg(c);
            const char *llop = (op == OP_VE) ? "and" : "or";
            fprintf(c->out, "  %%%d = %s i1 %%%d, %%%d\n",
                    rr, llop, l.reg, r.reg);
            IfadeSonuc out;
            out.reg = rr;
            snprintf(out.tip, ISIM_BUF, "i1");
            out.is_lvalue = 0;
            out.lvalue_tip[0] = 0;
            return out;
        }

        /* Aritmetik */
        const char *opname = NULL;
        int isfloat = float_tip_mi(l.tip);
        switch (op) {
            case OP_ARTI:  opname = isfloat ? "fadd" : "add";  break;
            case OP_EKSI:  opname = isfloat ? "fsub" : "sub";  break;
            case OP_CARPI: opname = isfloat ? "fmul" : "mul";  break;
            case OP_BOLU:  opname = isfloat ? "fdiv" : "sdiv"; break;
            case OP_MOD:   opname = isfloat ? "frem" : "srem"; break;
            default: break;
        }
        if (!opname) {
            fprintf(c->out, "  ; HATA: ikili op desteklenmiyor\n");
            return hata_sonuc();
        }
        int rr = yeni_reg(c);
        fprintf(c->out, "  %%%d = %s %s %%%d, %%%d\n",
                rr, opname, l.tip, l.reg, r.reg);
        IfadeSonuc out;
        out.reg = rr;
        snprintf(out.tip, ISIM_BUF, "%s", l.tip);
        out.is_lvalue = 0;
        out.lvalue_tip[0] = 0;
        return out;
    }

    case DUGUM_CAGRI: {
        const Dugum *hedef = d->veri.cagri.hedef;
        if (!hedef || hedef->tip != DUGUM_TANIMLAYICI) {
            fprintf(c->out, "  ; HATA: cagri hedefi tanimlayici degil\n");
            return hata_sonuc();
        }
        const char *fad = hedef->veri.tanimlayici.metin;
        int fu = hedef->veri.tanimlayici.uzunluk;

        /* Ozel intrinsic: uzunluk(dizi) — compile-time sabit boyut */
        if (fu == 7 && memcmp(fad, "uzunluk", 7) == 0 &&
            d->veri.cagri.sayi == 1) {
            const Dugum *arg = d->veri.cagri.argumanlar[0];
            if (arg && arg->tip == DUGUM_TANIMLAYICI) {
                Sembol *s = sembol_bul(c, arg->veri.tanimlayici.metin,
                                       arg->veri.tanimlayici.uzunluk);
                if (s && s->tip[0] == '[') {
                    int n = atoi(s->tip + 1);
                    int r = yeni_reg(c);
                    fprintf(c->out, "  %%%d = add i32 0, %d\n", r, n);
                    IfadeSonuc out;
                    out.reg = r;
                    snprintf(out.tip, ISIM_BUF, "i32");
                    out.is_lvalue = 0;
                    out.lvalue_tip[0] = 0;
                    return out;
                }
            }
            fprintf(c->out, "  ; HATA: uzunluk() dizi tanimlayicisi bekler\n");
            return hata_sonuc();
        }

        /* Kullanici tanimi -> sonra built-in */
        const IslevKayit *ik = islev_bul(c, fad, fu);
        const BuiltinTanim *bt = NULL;
        const char *call_donus;
        const char *call_llvm_ad;
        char call_llvm_ad_buf[ISIM_BUF];
        int call_param_sayi;
        const char *call_params[MAX_YAPI_ALAN];

        if (ik) {
            call_donus = ik->donus;
            snprintf(call_llvm_ad_buf, ISIM_BUF, "%s", ik->ascii_ad);
            call_llvm_ad = call_llvm_ad_buf;
            call_param_sayi = ik->param_sayi;
            for (int i = 0; i < ik->param_sayi && i < MAX_YAPI_ALAN; i++) {
                call_params[i] = ik->params[i];
            }
        } else if ((bt = builtin_bul(fad, fu)) != NULL) {
            call_donus = bt->donus;
            call_llvm_ad = bt->llvm_ad;
            call_param_sayi = bt->param_sayi;
            for (int i = 0; i < bt->param_sayi && i < 4; i++) {
                call_params[i] = bt->params[i];
            }
        } else {
            fprintf(c->out, "  ; HATA: tanimsiz islev %.*s\n", fu, fad);
            return hata_sonuc();
        }

        /* Argumanlari uret */
        int nargs = d->veri.cagri.sayi;
        int *argregs = (int *)calloc((size_t)(nargs > 0 ? nargs : 1),
                                     sizeof(int));
        char (*argtips)[ISIM_BUF] = (char (*)[ISIM_BUF])calloc(
            (size_t)(nargs > 0 ? nargs : 1), ISIM_BUF);
        if (!argregs || !argtips) {
            free(argregs); free(argtips);
            return hata_sonuc();
        }
        for (int i = 0; i < nargs; i++) {
            IfadeSonuc x = ifade_uret(c, d->veri.cagri.argumanlar[i]);
            const char *beklenen = (i < call_param_sayi) ? call_params[i] : x.tip;
            if (x.reg >= 0) {
                argregs[i] = int_cevir(c, x.reg, x.tip, beklenen);
            } else {
                argregs[i] = x.reg;
            }
            snprintf(argtips[i], ISIM_BUF, "%s", beklenen);
        }

        int isvoid = (strcmp(call_donus, "void") == 0);
        int rr = isvoid ? 0 : yeni_reg(c);

        if (isvoid) {
            fprintf(c->out, "  call void @%s(", call_llvm_ad);
        } else {
            fprintf(c->out, "  %%%d = call %s @%s(",
                    rr, call_donus, call_llvm_ad);
        }
        for (int i = 0; i < nargs; i++) {
            if (i) fputs(", ", c->out);
            fprintf(c->out, "%s %%%d", argtips[i], argregs[i]);
        }
        fputs(")\n", c->out);

        free(argregs); free(argtips);

        IfadeSonuc out;
        out.reg = isvoid ? 0 : rr;
        snprintf(out.tip, ISIM_BUF, "%s", call_donus);
        out.is_lvalue = 0;
        out.lvalue_tip[0] = 0;
        return out;
    }

    case DUGUM_YAPI_OLUSTUR: {
        const char *yad = d->veri.yapi_olustur.tip_ad;
        int yu = d->veri.yapi_olustur.tip_ad_uzunluk;
        const YapiKayit *yk = yapi_bul(c, yad, yu);
        if (!yk) {
            fprintf(c->out, "  ; HATA: bilinmeyen yapi %.*s\n", yu, yad);
            return hata_sonuc();
        }
        const char *aad = yk->ascii_ad;
        /* alloca + her alana store + yapinin loaded value'sunu don */
        int addr = yeni_reg(c);
        fprintf(c->out, "  %%%d = alloca %%struct.%s\n", addr, aad);

        int na = d->veri.yapi_olustur.alan_sayi;
        for (int i = 0; i < na; i++) {
            const Dugum *aa = d->veri.yapi_olustur.alanlar[i];
            int idx = yapi_alan_indeks(yk, aa->veri.alan_atama.ad,
                                       aa->veri.alan_atama.ad_uzunluk);
            if (idx < 0) {
                fprintf(c->out, "  ; HATA: bilinmeyen alan %.*s.%.*s\n",
                        yu, yad,
                        aa->veri.alan_atama.ad_uzunluk,
                        aa->veri.alan_atama.ad);
                continue;
            }
            IfadeSonuc v = ifade_uret(c, aa->veri.alan_atama.deger);
            if (v.reg < 0) continue;
            int gep = yeni_reg(c);
            fprintf(c->out,
                "  %%%d = getelementptr inbounds %%struct.%s, ptr %%%d, i32 0, i32 %d\n",
                gep, aad, addr, idx);
            fprintf(c->out, "  store %s %%%d, ptr %%%d\n",
                    yk->alanlar[idx].tip, v.reg, gep);
        }
        int loaded = yeni_reg(c);
        fprintf(c->out, "  %%%d = load %%struct.%s, ptr %%%d\n",
                loaded, aad, addr);
        IfadeSonuc out;
        out.reg = loaded;
        snprintf(out.tip, ISIM_BUF, "%%struct.%.96s", aad);
        out.is_lvalue = 0;
        out.lvalue_tip[0] = 0;
        return out;
    }

    case DUGUM_ERISIM: {
        /* nesne.alan — nesne yapi value veya yapi ptr olabilir */
        const Dugum *nesne_d = d->veri.erisim.nesne;
        const char *alan_ad = d->veri.erisim.alan;
        int alan_u = d->veri.erisim.alan_uzunluk;

        /* Eger nesne bir tanimlayici ise, direkt alloca adresinden GEP yapariz */
        if (nesne_d->tip == DUGUM_TANIMLAYICI) {
            Sembol *s = sembol_bul(c, nesne_d->veri.tanimlayici.metin,
                                   nesne_d->veri.tanimlayici.uzunluk);
            if (!s) return hata_sonuc();
            /* s->tip "%struct.<ascii_ad>" olmali */
            if (strncmp(s->tip, "%struct.", 8) != 0) {
                fprintf(c->out, "  ; HATA: %.*s yapi degil\n",
                        nesne_d->veri.tanimlayici.uzunluk,
                        nesne_d->veri.tanimlayici.metin);
                return hata_sonuc();
            }
            const YapiKayit *yk = yapi_bul_ascii(c, s->tip + 8);
            if (!yk) return hata_sonuc();
            int idx = yapi_alan_indeks(yk, alan_ad, alan_u);
            if (idx < 0) {
                fprintf(c->out, "  ; HATA: bilinmeyen alan %.*s\n",
                        alan_u, alan_ad);
                return hata_sonuc();
            }
            int gep = yeni_reg(c);
            fprintf(c->out,
                "  %%%d = getelementptr inbounds %s, ptr %s, i32 0, i32 %d\n",
                gep, s->tip, s->addr, idx);
            int loaded = yeni_reg(c);
            fprintf(c->out, "  %%%d = load %s, ptr %%%d\n",
                    loaded, yk->alanlar[idx].tip, gep);
            IfadeSonuc out;
            out.reg = loaded;
            snprintf(out.tip, ISIM_BUF, "%s", yk->alanlar[idx].tip);
            out.is_lvalue = 0;
            out.lvalue_tip[0] = 0;
            return out;
        }
        fprintf(c->out, "  ; HATA: karmasik erisim hedefi desteklenmiyor\n");
        return hata_sonuc();
    }

    case DUGUM_INDEKS: {
        /* dizi[i] — dizi tanimlayici, eleman tipi sembolden cikar */
        const Dugum *nesne_d = d->veri.indeks.nesne;
        if (nesne_d->tip != DUGUM_TANIMLAYICI) {
            fprintf(c->out, "  ; HATA: indeks hedefi tanimlayici degil\n");
            return hata_sonuc();
        }
        Sembol *s = sembol_bul(c, nesne_d->veri.tanimlayici.metin,
                               nesne_d->veri.tanimlayici.uzunluk);
        if (!s) return hata_sonuc();
        /* Tip "[N x T]" formatinda ise eleman tipini sok */
        char eleman_tip[ISIM_BUF];
        int sabit_boyut = 0;
        if (s->tip[0] == '[') {
            /* [N x T] -> T'yi cek */
            const char *p = strstr(s->tip, " x ");
            if (p) {
                sabit_boyut = atoi(s->tip + 1);
                p += 3;
                int len = (int)strlen(p);
                if (len > 0 && p[len-1] == ']') len--;
                snprintf(eleman_tip, ISIM_BUF, "%.*s", len, p);
            } else {
                snprintf(eleman_tip, ISIM_BUF, "i32");
            }
        } else {
            snprintf(eleman_tip, ISIM_BUF, "i32");
        }
        IfadeSonuc i = ifade_uret(c, d->veri.indeks.indeks);
        if (i.reg < 0) return hata_sonuc();
        int gep = yeni_reg(c);
        if (sabit_boyut > 0) {
            fprintf(c->out,
                "  %%%d = getelementptr inbounds %s, ptr %s, i32 0, %s %%%d\n",
                gep, s->tip, s->addr, i.tip, i.reg);
        } else {
            fprintf(c->out,
                "  %%%d = getelementptr inbounds %s, ptr %s, %s %%%d\n",
                gep, eleman_tip, s->addr, i.tip, i.reg);
        }
        int loaded = yeni_reg(c);
        fprintf(c->out, "  %%%d = load %s, ptr %%%d\n",
                loaded, eleman_tip, gep);
        IfadeSonuc out;
        out.reg = loaded;
        snprintf(out.tip, ISIM_BUF, "%s", eleman_tip);
        out.is_lvalue = 0;
        out.lvalue_tip[0] = 0;
        return out;
    }

    case DUGUM_DIZI_OLUSTUR: {
        /* [e1, e2, ...] — sabit boyut dizi, alloca + her elemana store */
        int n = d->veri.dizi_olustur.sayi;
        if (n <= 0) {
            fprintf(c->out, "  ; HATA: bos dizi desteklenmiyor\n");
            return hata_sonuc();
        }
        /* Eleman tipi — ilk elemandan cikar */
        IfadeSonuc e0 = ifade_uret(c, d->veri.dizi_olustur.elemanlar[0]);
        if (e0.reg < 0) return hata_sonuc();
        char et[ISIM_BUF];
        snprintf(et, ISIM_BUF, "%s", e0.tip);
        int addr = yeni_reg(c);
        fprintf(c->out, "  %%%d = alloca [%d x %s]\n", addr, n, et);
        /* Ilk elemani yaz */
        int gep0 = yeni_reg(c);
        fprintf(c->out,
            "  %%%d = getelementptr inbounds [%d x %s], ptr %%%d, i32 0, i32 0\n",
            gep0, n, et, addr);
        fprintf(c->out, "  store %s %%%d, ptr %%%d\n", et, e0.reg, gep0);
        for (int i = 1; i < n; i++) {
            IfadeSonuc ei = ifade_uret(c, d->veri.dizi_olustur.elemanlar[i]);
            if (ei.reg < 0) continue;
            int gep = yeni_reg(c);
            fprintf(c->out,
                "  %%%d = getelementptr inbounds [%d x %s], ptr %%%d, i32 0, i32 %d\n",
                gep, n, et, addr, i);
            fprintf(c->out, "  store %s %%%d, ptr %%%d\n", et, ei.reg, gep);
        }
        IfadeSonuc out;
        out.reg = addr;
        snprintf(out.tip, ISIM_BUF, "[%d x %.64s]", n, et);
        out.is_lvalue = 0;
        out.lvalue_tip[0] = 0;
        return out;
    }

    default:
        fprintf(c->out, "  ; HATA: ifade tipi %d desteklenmiyor\n", d->tip);
        return hata_sonuc();
    }
}

/* === Deyim uretimi === */

static void deyim_uret(Codegen *c, const Dugum *d) {
    if (!d) return;
    if (c->block_terminated) return;

    switch (d->tip) {
    case DUGUM_DEGISKEN: {
        const char *ad = d->veri.degisken.ad;
        int u = d->veri.degisken.ad_uzunluk;

        /* Dizi durumu — DIZI_OLUSTUR kendi alloca'sini yapar, sembolu
         * direkt o adrese baglariz. */
        if (d->veri.degisken.deger &&
            d->veri.degisken.deger->tip == DUGUM_DIZI_OLUSTUR) {
            IfadeSonuc x = ifade_uret(c, d->veri.degisken.deger);
            if (x.reg >= 0) {
                char a[ISIM_BUF];
                snprintf(a, ISIM_BUF, "%%%d", x.reg);
                sembol_ekle(c, ad, u, a, x.tip);
            }
            return;
        }

        /* Tip: ya annot ya da degerden cikarsa. Annot yoksa cogu durumda
         * sayisal default i32. */
        char tip[ISIM_BUF];
        if (d->veri.degisken.tip) {
            kemgu_tip_to_llvm(c, d->veri.degisken.tip, tip, ISIM_BUF);
        } else {
            snprintf(tip, ISIM_BUF, "i32");
        }

        /* alloca */
        int addr_reg = yeni_reg(c);
        fprintf(c->out, "  %%%d = alloca %s\n", addr_reg, tip);
        char addr_str[ISIM_BUF];
        snprintf(addr_str, ISIM_BUF, "%%%d", addr_reg);
        sembol_ekle(c, ad, u, addr_str, tip);

        if (d->veri.degisken.deger) {
            IfadeSonuc v = ifade_uret(c, d->veri.degisken.deger);
            if (v.reg < 0) return;
            int sreg = int_cevir(c, v.reg, v.tip, tip);
            fprintf(c->out, "  store %s %%%d, ptr %s\n", tip, sreg, addr_str);
        }
        return;
    }

    case DUGUM_ATAMA: {
        const Dugum *hedef = d->veri.atama.hedef;
        if (hedef->tip == DUGUM_TANIMLAYICI) {
            Sembol *s = sembol_bul(c, hedef->veri.tanimlayici.metin,
                                   hedef->veri.tanimlayici.uzunluk);
            if (!s) return;
            IfadeSonuc v = ifade_uret(c, d->veri.atama.deger);
            if (v.reg < 0) return;
            fprintf(c->out, "  store %s %%%d, ptr %s\n",
                    s->tip, v.reg, s->addr);
            return;
        }
        if (hedef->tip == DUGUM_ERISIM) {
            const Dugum *nesne_d = hedef->veri.erisim.nesne;
            if (nesne_d->tip != DUGUM_TANIMLAYICI) {
                fprintf(c->out, "  ; HATA: atama hedefi karmasik\n");
                return;
            }
            Sembol *s = sembol_bul(c, nesne_d->veri.tanimlayici.metin,
                                   nesne_d->veri.tanimlayici.uzunluk);
            if (!s) return;
            if (strncmp(s->tip, "%struct.", 8) != 0) return;
            const YapiKayit *yk = yapi_bul_ascii(c, s->tip + 8);
            if (!yk) return;
            int idx = yapi_alan_indeks(yk, hedef->veri.erisim.alan,
                                       hedef->veri.erisim.alan_uzunluk);
            if (idx < 0) return;
            IfadeSonuc v = ifade_uret(c, d->veri.atama.deger);
            if (v.reg < 0) return;
            int gep = yeni_reg(c);
            fprintf(c->out,
                "  %%%d = getelementptr inbounds %s, ptr %s, i32 0, i32 %d\n",
                gep, s->tip, s->addr, idx);
            fprintf(c->out, "  store %s %%%d, ptr %%%d\n",
                    yk->alanlar[idx].tip, v.reg, gep);
            return;
        }
        if (hedef->tip == DUGUM_INDEKS) {
            const Dugum *nesne_d = hedef->veri.indeks.nesne;
            if (nesne_d->tip != DUGUM_TANIMLAYICI) {
                fprintf(c->out, "  ; HATA: dizi atama karmasik\n");
                return;
            }
            Sembol *s = sembol_bul(c, nesne_d->veri.tanimlayici.metin,
                                   nesne_d->veri.tanimlayici.uzunluk);
            if (!s) return;
            char eleman_tip[ISIM_BUF];
            int sabit_boyut = 0;
            if (s->tip[0] == '[') {
                const char *p = strstr(s->tip, " x ");
                if (p) {
                    sabit_boyut = atoi(s->tip + 1);
                    p += 3;
                    int len = (int)strlen(p);
                    if (len > 0 && p[len-1] == ']') len--;
                    snprintf(eleman_tip, ISIM_BUF, "%.*s", len, p);
                } else {
                    snprintf(eleman_tip, ISIM_BUF, "i32");
                }
            } else {
                snprintf(eleman_tip, ISIM_BUF, "i32");
            }
            IfadeSonuc i = ifade_uret(c, hedef->veri.indeks.indeks);
            if (i.reg < 0) return;
            IfadeSonuc v = ifade_uret(c, d->veri.atama.deger);
            if (v.reg < 0) return;
            int gep = yeni_reg(c);
            if (sabit_boyut > 0) {
                fprintf(c->out,
                    "  %%%d = getelementptr inbounds %s, ptr %s, i32 0, %s %%%d\n",
                    gep, s->tip, s->addr, i.tip, i.reg);
            } else {
                fprintf(c->out,
                    "  %%%d = getelementptr inbounds %s, ptr %s, %s %%%d\n",
                    gep, eleman_tip, s->addr, i.tip, i.reg);
            }
            fprintf(c->out, "  store %s %%%d, ptr %%%d\n",
                    eleman_tip, v.reg, gep);
            return;
        }
        fprintf(c->out, "  ; HATA: atama hedefi desteklenmiyor\n");
        return;
    }

    case DUGUM_VER: {
        if (d->veri.ver.deger) {
            IfadeSonuc v = ifade_uret(c, d->veri.ver.deger);
            if (v.reg < 0) {
                fprintf(c->out, "  ret %s 0\n", c->aktif_donus);
            } else {
                int sreg = int_cevir(c, v.reg, v.tip, c->aktif_donus);
                fprintf(c->out, "  ret %s %%%d\n", c->aktif_donus, sreg);
            }
        } else {
            if (strcmp(c->aktif_donus, "void") == 0) {
                fprintf(c->out, "  ret void\n");
            } else {
                fprintf(c->out, "  ret %s 0\n", c->aktif_donus);
            }
        }
        c->block_terminated = 1;
        return;
    }

    case DUGUM_EGER: {
        IfadeSonuc k = ifade_uret(c, d->veri.eger.kosul);
        if (k.reg < 0) return;
        int then_id = yeni_blok(c);
        int else_id = yeni_blok(c);
        int end_id  = yeni_blok(c);
        int has_else = (d->veri.eger.yan != NULL);
        fprintf(c->out, "  br i1 %%%d, label %%if.then.%d, label %%%s.%d\n",
                k.reg, then_id, has_else ? "if.else" : "if.end", has_else ? else_id : end_id);

        /* then */
        fprintf(c->out, "\nif.then.%d:\n", then_id);
        c->block_terminated = 0;
        deyim_uret(c, d->veri.eger.gozdoldur);
        if (!c->block_terminated) {
            fprintf(c->out, "  br label %%if.end.%d\n", end_id);
        }

        if (has_else) {
            fprintf(c->out, "\nif.else.%d:\n", else_id);
            c->block_terminated = 0;
            deyim_uret(c, d->veri.eger.yan);
            if (!c->block_terminated) {
                fprintf(c->out, "  br label %%if.end.%d\n", end_id);
            }
        }

        fprintf(c->out, "\nif.end.%d:\n", end_id);
        c->block_terminated = 0;
        return;
    }

    case DUGUM_IKEN: {
        int head_id = yeni_blok(c);
        int body_id = yeni_blok(c);
        int end_id  = yeni_blok(c);

        fprintf(c->out, "  br label %%while.head.%d\n", head_id);

        fprintf(c->out, "\nwhile.head.%d:\n", head_id);
        c->block_terminated = 0;
        IfadeSonuc k = ifade_uret(c, d->veri.iken.kosul);
        if (k.reg < 0) return;
        fprintf(c->out, "  br i1 %%%d, label %%while.body.%d, label %%while.end.%d\n",
                k.reg, body_id, end_id);

        fprintf(c->out, "\nwhile.body.%d:\n", body_id);
        c->block_terminated = 0;
        deyim_uret(c, d->veri.iken.govde);
        if (!c->block_terminated) {
            fprintf(c->out, "  br label %%while.head.%d\n", head_id);
        }

        fprintf(c->out, "\nwhile.end.%d:\n", end_id);
        c->block_terminated = 0;
        return;
    }

    case DUGUM_ESLES: {
        /* C: eşleş — if-else zinciri olarak çevrilir.
         * Desen tipleri:
         *   LITERAL    -> icmp eq + koşullu dal
         *   TANIMLAYICI -> her zaman eşleşir, değişkene bağla
         *   JOKER      -> her zaman eşleşir
         *   YAPICI (deger/hic/tamam/hata) -> uçulmaz (secçimlik storage yok) */
        IfadeSonuc v = ifade_uret(c, d->veri.esles.deger);
        if (v.reg < 0) return;
        int end_id = yeni_blok(c);

        for (int i = 0; i < d->veri.esles.kol_sayi; i++) {
            const Dugum *kol = d->veri.esles.kollar[i];
            const Dugum *desen = kol->veri.esles_kolu.desen;
            int last = (i == d->veri.esles.kol_sayi - 1);
            int arm_id = yeni_blok(c);
            int next_id = last ? end_id : yeni_blok(c);

            switch (desen->tip) {
            case DUGUM_DESEN_LITERAL: {
                /* Literal degerini ifade olarak uret + icmp eq */
                const Dugum *lit = desen->veri.desen_literal.deger;
                IfadeSonuc lv = ifade_uret(c, lit);
                if (lv.reg < 0) {
                    fprintf(c->out, "  br label %%match.arm.%d\n", arm_id);
                } else {
                    int sreg = int_cevir(c, lv.reg, lv.tip, v.tip);
                    int cmp = yeni_reg(c);
                    fprintf(c->out, "  %%%d = icmp eq %s %%%d, %%%d\n",
                            cmp, v.tip, v.reg, sreg);
                    fprintf(c->out,
                        "  br i1 %%%d, label %%match.arm.%d, label %%match.next.%d\n",
                        cmp, arm_id, next_id);
                }
                break;
            }
            case DUGUM_DESEN_TANIMLAYICI:
            case DUGUM_DESEN_JOKER:
                /* Her zaman match — direkt dallan */
                fprintf(c->out, "  br label %%match.arm.%d\n", arm_id);
                break;
            default:
                /* DESEN_YAPICI vs - henuz desteksiz, atla */
                fprintf(c->out,
                    "  ; UYARI: yapici deseni LLVM'de henuz destekli degil\n");
                fprintf(c->out, "  br label %%match.next.%d\n", next_id);
                break;
            }

            /* === Kol gövdesi === */
            fprintf(c->out, "\nmatch.arm.%d:\n", arm_id);
            c->block_terminated = 0;
            int watermark = c->sembol_sayi;

            /* Tanimlayici desen ise, sembol olarak ekle (eslese degerine bagla) */
            if (desen->tip == DUGUM_DESEN_TANIMLAYICI) {
                /* Tanimlayici bir alloca + store al */
                int addr = yeni_reg(c);
                fprintf(c->out, "  %%%d = alloca %s\n", addr, v.tip);
                fprintf(c->out, "  store %s %%%d, ptr %%%d\n",
                        v.tip, v.reg, addr);
                char a[ISIM_BUF];
                snprintf(a, ISIM_BUF, "%%%d", addr);
                sembol_ekle(c,
                    desen->veri.desen_tanimlayici.ad,
                    desen->veri.desen_tanimlayici.ad_uzunluk,
                    a, v.tip);
            }

            deyim_uret(c, kol->veri.esles_kolu.govde);
            if (!c->block_terminated) {
                fprintf(c->out, "  br label %%match.end.%d\n", end_id);
            }
            c->sembol_sayi = watermark;

            if (!last) {
                fprintf(c->out, "\nmatch.next.%d:\n", next_id);
                c->block_terminated = 0;
            }
        }

        fprintf(c->out, "\nmatch.end.%d:\n", end_id);
        c->block_terminated = 0;
        return;
    }

    case DUGUM_ICIN: {
        /* F: için x: liste { ... } — Dizi<T> uzerinde index'li dongu.
         * Sembol liste tipi "[N x T]" (sabit boyut, dizi_olustur'dan) varsayar.
         * Lokal indeks alloca, baslangic 0, her yinelemede +1, son N. */
        const Dugum *kol_d = d->veri.icin.koleksiyon;
        if (kol_d->tip != DUGUM_TANIMLAYICI) {
            fprintf(c->out, "  ; HATA: 'icin' karmasik koleksiyon desteksiz\n");
            return;
        }
        Sembol *liste = sembol_bul(c, kol_d->veri.tanimlayici.metin,
                                   kol_d->veri.tanimlayici.uzunluk);
        if (!liste || liste->tip[0] != '[') {
            fprintf(c->out, "  ; HATA: 'icin' sabit-boyut dizi gerek\n");
            return;
        }
        int n = atoi(liste->tip + 1);
        char eleman_tip[ISIM_BUF];
        const char *xp = strstr(liste->tip, " x ");
        if (xp) {
            xp += 3;
            int len = (int)strlen(xp);
            if (len > 0 && xp[len-1] == ']') len--;
            snprintf(eleman_tip, ISIM_BUF, "%.*s", len, xp);
        } else {
            snprintf(eleman_tip, ISIM_BUF, "i32");
        }

        int head_id = yeni_blok(c);
        int body_id = yeni_blok(c);
        int end_id  = yeni_blok(c);

        /* indeks alloca */
        int idx_addr = yeni_reg(c);
        fprintf(c->out, "  %%%d = alloca i32\n", idx_addr);
        fprintf(c->out, "  store i32 0, ptr %%%d\n", idx_addr);
        fprintf(c->out, "  br label %%for.head.%d\n", head_id);

        /* head: cond i < N */
        fprintf(c->out, "\nfor.head.%d:\n", head_id);
        c->block_terminated = 0;
        int idx_val = yeni_reg(c);
        fprintf(c->out, "  %%%d = load i32, ptr %%%d\n", idx_val, idx_addr);
        int cmp = yeni_reg(c);
        fprintf(c->out, "  %%%d = icmp slt i32 %%%d, %d\n", cmp, idx_val, n);
        fprintf(c->out,
            "  br i1 %%%d, label %%for.body.%d, label %%for.end.%d\n",
            cmp, body_id, end_id);

        /* body: x = liste[i] */
        fprintf(c->out, "\nfor.body.%d:\n", body_id);
        c->block_terminated = 0;
        int watermark = c->sembol_sayi;
        int idx_val2 = yeni_reg(c);
        fprintf(c->out, "  %%%d = load i32, ptr %%%d\n", idx_val2, idx_addr);
        int gep = yeni_reg(c);
        fprintf(c->out,
            "  %%%d = getelementptr inbounds %s, ptr %s, i32 0, i32 %%%d\n",
            gep, liste->tip, liste->addr, idx_val2);
        /* x alloca + store + sembol ekle */
        int x_addr = yeni_reg(c);
        fprintf(c->out, "  %%%d = alloca %s\n", x_addr, eleman_tip);
        int elem = yeni_reg(c);
        fprintf(c->out, "  %%%d = load %s, ptr %%%d\n",
                elem, eleman_tip, gep);
        fprintf(c->out, "  store %s %%%d, ptr %%%d\n",
                eleman_tip, elem, x_addr);
        char x_addr_str[ISIM_BUF];
        snprintf(x_addr_str, ISIM_BUF, "%%%d", x_addr);
        sembol_ekle(c, d->veri.icin.degisken_adi,
                    d->veri.icin.degisken_adi_uzunluk,
                    x_addr_str, eleman_tip);

        deyim_uret(c, d->veri.icin.govde);
        if (!c->block_terminated) {
            /* i = i + 1, head'e geri */
            int idx_now = yeni_reg(c);
            fprintf(c->out, "  %%%d = load i32, ptr %%%d\n",
                    idx_now, idx_addr);
            int idx_next = yeni_reg(c);
            fprintf(c->out, "  %%%d = add i32 %%%d, 1\n", idx_next, idx_now);
            fprintf(c->out, "  store i32 %%%d, ptr %%%d\n",
                    idx_next, idx_addr);
            fprintf(c->out, "  br label %%for.head.%d\n", head_id);
        }
        c->sembol_sayi = watermark;

        fprintf(c->out, "\nfor.end.%d:\n", end_id);
        c->block_terminated = 0;
        return;
    }

    case DUGUM_BLOK: {
        int watermark = c->sembol_sayi;
        for (int i = 0; i < d->veri.blok.sayi; i++) {
            if (c->block_terminated) break;
            deyim_uret(c, d->veri.blok.deyimler[i]);
        }
        c->sembol_sayi = watermark;
        return;
    }

    case DUGUM_IFADE_DEYIMI: {
        ifade_uret(c, d->veri.ifade_deyimi.ifade);
        return;
    }

    default:
        fprintf(c->out, "  ; deyim tipi %d desteklenmiyor (atlandi)\n",
                d->tip);
        return;
    }
}

/* === Yapi tanimlari === */

static void yapi_kaydet(Codegen *c, const Dugum *y) {
    if (c->yapi_sayi >= MAX_YAPI) return;
    YapiKayit *yk = &c->yapilar[c->yapi_sayi++];
    yk->ad = y->veri.yapi.ad;
    yk->ad_uzunluk = y->veri.yapi.ad_uzunluk;
    ad_ascii_yap(yk->ad, yk->ad_uzunluk, yk->ascii_ad, ISIM_BUF);
    yk->alan_sayi = 0;
    for (int i = 0; i < y->veri.yapi.alan_sayi && i < MAX_YAPI_ALAN; i++) {
        const Dugum *al = y->veri.yapi.alanlar[i];
        yk->alanlar[yk->alan_sayi].ad = al->veri.alan.ad;
        yk->alanlar[yk->alan_sayi].ad_uzunluk = al->veri.alan.ad_uzunluk;
        kemgu_tip_to_llvm(c, al->veri.alan.tip,
                          yk->alanlar[yk->alan_sayi].tip, ISIM_BUF);
        yk->alan_sayi++;
    }
}

static void yapi_emit(Codegen *c, const YapiKayit *yk) {
    fprintf(c->out, "%%struct.%s = type { ", yk->ascii_ad);
    for (int i = 0; i < yk->alan_sayi; i++) {
        if (i) fputs(", ", c->out);
        fputs(yk->alanlar[i].tip, c->out);
    }
    fputs(" }\n", c->out);
}

/* === Islev imza on-kayit === */

static void islev_kaydet(Codegen *c, const Dugum *isl) {
    if (c->islev_sayi >= MAX_ISLEV) return;
    IslevKayit *ik = &c->islevler[c->islev_sayi++];
    ik->ad = isl->veri.islev.ad;
    ik->ad_uzunluk = isl->veri.islev.ad_uzunluk;
    ad_ascii_yap(ik->ad, ik->ad_uzunluk, ik->ascii_ad, ISIM_BUF);
    kemgu_tip_to_llvm(c, isl->veri.islev.donus_tipi, ik->donus, ISIM_BUF);
    ik->param_sayi = 0;
    for (int i = 0; i < isl->veri.islev.param_sayi &&
                    i < MAX_YAPI_ALAN; i++) {
        kemgu_tip_to_llvm(c, isl->veri.islev.parametreler[i]->veri.parametre.tip,
                          ik->params[ik->param_sayi], ISIM_BUF);
        ik->param_sayi++;
    }
}

/* === Islev uret === */

static void islev_uret(Codegen *c, const Dugum *isl) {
    /* Govdesi yoksa (forward decl) skip */
    if (!isl->veri.islev.govde) return;

    const IslevKayit *ik = islev_bul(c, isl->veri.islev.ad,
                                     isl->veri.islev.ad_uzunluk);
    if (!ik) return;

    snprintf(c->aktif_donus, ISIM_BUF, "%s", ik->donus);
    c->reg = 0;
    c->blok_id = 0;
    c->sembol_sayi = 0;
    c->block_terminated = 0;

    fprintf(c->out, "define %s @%s(", ik->donus, ik->ascii_ad);
    for (int i = 0; i < ik->param_sayi; i++) {
        if (i) fputs(", ", c->out);
        fprintf(c->out, "%s %%p%d", ik->params[i], i);
    }
    fputs(") {\nentry:\n", c->out);

    /* Parametreler icin entry'de alloca + store, sembol haritasina ekle.
     * Reg sayaci p0/p1/... isimli SSA degerlerini saymaz; sadece numerik
     * %0, %1, ... uretiriz. */
    /* Param SSA isimleri %p0, %p1, ... olduguna gore numerik reg sayaci
     * 0'dan baslar — celismez. */
    for (int i = 0; i < ik->param_sayi; i++) {
        int addr = yeni_reg(c);
        fprintf(c->out, "  %%%d = alloca %s\n", addr, ik->params[i]);
        fprintf(c->out, "  store %s %%p%d, ptr %%%d\n",
                ik->params[i], i, addr);
        char addrs[ISIM_BUF];
        snprintf(addrs, ISIM_BUF, "%%%d", addr);
        const Dugum *pd = isl->veri.islev.parametreler[i];
        sembol_ekle(c, pd->veri.parametre.ad,
                    pd->veri.parametre.ad_uzunluk,
                    addrs, ik->params[i]);
    }

    /* Gövde */
    deyim_uret(c, isl->veri.islev.govde);

    /* Eger fonksiyon sonunda terminate olmadiysa default ret ekle */
    if (!c->block_terminated) {
        if (strcmp(c->aktif_donus, "void") == 0) {
            fputs("  ret void\n", c->out);
        } else {
            fprintf(c->out, "  ret %s 0\n", c->aktif_donus);
        }
    }
    fputs("}\n\n", c->out);
}

/* === String escape (LLVM IR formati) === */

static void string_emit(FILE *out, const char *m, int u) {
    for (int i = 0; i < u; i++) {
        unsigned char ch = (unsigned char)m[i];
        if (ch == '"' || ch == '\\' || ch < 0x20 || ch >= 0x7f) {
            fprintf(out, "\\%02X", ch);
        } else {
            fputc(ch, out);
        }
    }
    fputs("\\00", out);  /* null terminator */
}

/* === Public API === */

void llvm_ir_uret(const Dugum *program, FILE *out) {
    if (!out) return;
    if (!program || program->tip != DUGUM_PROGRAM) {
        fputs("; (program AST'si yok)\n", out);
        return;
    }

    Codegen c;
    memset(&c, 0, sizeof(c));
    c.out = out;

    fputs("; KEMGU LLVM IR (text uretici, ADIM D — stdlib bagli)\n", out);
    fputs("target triple = \"x86_64-pc-windows-gnu\"\n\n", out);

    /* Standart kutuphane (KDL) built-in islev declare'lari.
     * Hepsi runtime/kdl_runtime.c icinde implement edilir; clang ile link
     * edilirken bagimlilik cozulur. */
    fputs("; --- KDL standart kutuphane bildirimleri ---\n", out);
    for (int i = 0; i < BUILTIN_SAYI; i++) {
        const BuiltinTanim *b = &BUILTINLER[i];
        fprintf(out, "declare %s @%s(", b->donus, b->llvm_ad);
        for (int j = 0; j < b->param_sayi; j++) {
            if (j) fputs(", ", out);
            fputs(b->params[j], out);
        }
        fputs(")\n", out);
    }
    fputc('\n', out);

    /* 1. Yapi tanimlarini topla + emit et */
    for (int i = 0; i < program->veri.program.sayi; i++) {
        const Dugum *u = program->veri.program.uyeler[i];
        const Dugum *gercek = (u->tip == DUGUM_DISA) ? u->veri.disa.tanim : u;
        if (gercek && gercek->tip == DUGUM_YAPI) {
            yapi_kaydet(&c, gercek);
        }
    }
    for (int i = 0; i < c.yapi_sayi; i++) {
        yapi_emit(&c, &c.yapilar[i]);
    }
    if (c.yapi_sayi > 0) fputc('\n', out);

    /* 1b. Tip alias'lari topla — yapilardan sonra (alias yapi tipine
     * isaret edebilir). kemgu_tip_to_llvm yapi_kayitlari kullanir. */
    for (int i = 0; i < program->veri.program.sayi; i++) {
        const Dugum *u = program->veri.program.uyeler[i];
        const Dugum *gercek = (u->tip == DUGUM_DISA) ? u->veri.disa.tanim : u;
        if (gercek && gercek->tip == DUGUM_TIP_ALIAS &&
            c.alias_sayi < MAX_ALIAS) {
            AliasKayit *ak = &c.aliases[c.alias_sayi++];
            ak->ad = gercek->veri.tip_alias.ad;
            ak->ad_uzunluk = gercek->veri.tip_alias.ad_uzunluk;
            kemgu_tip_to_llvm(&c, gercek->veri.tip_alias.hedef,
                              ak->hedef_llvm, ISIM_BUF);
        }
    }

    /* 2. Islev imzalarini on-kaydet (forward reference icin) */
    for (int i = 0; i < program->veri.program.sayi; i++) {
        const Dugum *u = program->veri.program.uyeler[i];
        const Dugum *gercek = (u->tip == DUGUM_DISA) ? u->veri.disa.tanim : u;
        if (gercek && gercek->tip == DUGUM_ISLEV) {
            islev_kaydet(&c, gercek);
        }
    }

    /* 3. Islev govdelerini uret. Bu sirada string_ekle calistirilir; cikti
     * stdout'a fonksiyon icinde gider — string globalleri sonraya birak. */
    /* Once islev govdelerini bir tampon dosyaya yazip, sonra string
     * globalleri stdout'a once yazip ardindan tamponu kopyalamak gerekir.
     * Pratik cozum: tmpfile() kullan. */
    FILE *tampon = tmpfile();
    if (!tampon) {
        /* tmpfile yoksa direkt yaz, string globalleri yanlis siralanir
         * ama clang yine de kabul edebilir */
        c.out = out;
        for (int i = 0; i < program->veri.program.sayi; i++) {
            const Dugum *u = program->veri.program.uyeler[i];
            const Dugum *gercek = (u->tip == DUGUM_DISA)
                ? u->veri.disa.tanim : u;
            if (gercek && gercek->tip == DUGUM_ISLEV) {
                islev_uret(&c, gercek);
            }
        }
        return;
    }
    c.out = tampon;
    for (int i = 0; i < program->veri.program.sayi; i++) {
        const Dugum *u = program->veri.program.uyeler[i];
        const Dugum *gercek = (u->tip == DUGUM_DISA) ? u->veri.disa.tanim : u;
        if (gercek && gercek->tip == DUGUM_ISLEV) {
            islev_uret(&c, gercek);
        }
    }

    /* String literalleri global olarak stdout'a yaz */
    for (int i = 0; i < c.string_sayi; i++) {
        fprintf(out,
            "@.str.%d = private unnamed_addr constant [%d x i8] c\"",
            c.stringler[i].id, c.stringler[i].uzunluk + 1);
        string_emit(out, c.stringler[i].metin, c.stringler[i].uzunluk);
        fputs("\"\n", out);
    }
    if (c.string_sayi > 0) fputc('\n', out);

    /* Tamponu kopyala */
    rewind(tampon);
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), tampon)) > 0) {
        fwrite(buf, 1, n, out);
    }
    fclose(tampon);
}
