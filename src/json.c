#include "json.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* === Parser durumu === */

typedef struct {
    Arena *arena;
    const char *baslangic;
    int uzunluk;
    int konum;
    const char *hata;
} JsonParser;

static void hata_kur(JsonParser *p, const char *m) {
    if (!p->hata) p->hata = m;
}

static int eof(JsonParser *p) {
    return p->konum >= p->uzunluk;
}

static char simdiki(JsonParser *p) {
    if (eof(p)) return '\0';
    return p->baslangic[p->konum];
}

static void atla_bosluk(JsonParser *p) {
    while (!eof(p)) {
        char c = simdiki(p);
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') p->konum++;
        else break;
    }
}

static int eslesir(JsonParser *p, const char *s) {
    int n = (int)strlen(s);
    if (p->konum + n > p->uzunluk) return 0;
    return memcmp(p->baslangic + p->konum, s, (size_t)n) == 0;
}

/* === Forward decl === */
static JsonDeger *ayrist_deger(JsonParser *p);

/* === Olusturucular === */

static JsonDeger *deger_olustur(JsonParser *p, JsonTipi t) {
    JsonDeger *d = (JsonDeger *)arena_ayir_sifir(p->arena, sizeof(JsonDeger));
    if (d) d->tip = t;
    return d;
}

/* === String (escape destekli) === */

static const char *ayrist_metin(JsonParser *p, int *out_uz) {
    if (simdiki(p) != '"') { hata_kur(p, "string '\"' bekleniyor"); return NULL; }
    p->konum++;

    /* Kapasite hesabi (escape varsa sonuc kaynaktan kisa) — once tampon */
    int bas = p->konum;
    int kaynak_uz = 0;
    while (p->konum < p->uzunluk && p->baslangic[p->konum] != '"') {
        if (p->baslangic[p->konum] == '\\' && p->konum + 1 < p->uzunluk) {
            p->konum += 2;
        } else {
            p->konum++;
        }
        kaynak_uz = p->konum - bas;
    }
    if (eof(p)) { hata_kur(p, "kapanmamis string"); return NULL; }

    /* Cikti tampon (en kotu kaynak boyutu — \uXXXX 6 byte -> 3 byte UTF-8) */
    char *cikti = (char *)arena_ayir(p->arena, (size_t)kaynak_uz + 1);
    if (!cikti) { hata_kur(p, "arena bos"); return NULL; }
    int ci = 0;
    int i = bas;
    while (i < p->konum) {
        char c = p->baslangic[i];
        if (c == '\\' && i + 1 < p->konum) {
            char n = p->baslangic[i + 1];
            switch (n) {
                case '"': cikti[ci++] = '"'; i += 2; break;
                case '\\': cikti[ci++] = '\\'; i += 2; break;
                case '/': cikti[ci++] = '/'; i += 2; break;
                case 'n': cikti[ci++] = '\n'; i += 2; break;
                case 'r': cikti[ci++] = '\r'; i += 2; break;
                case 't': cikti[ci++] = '\t'; i += 2; break;
                case 'b': cikti[ci++] = '\b'; i += 2; break;
                case 'f': cikti[ci++] = '\f'; i += 2; break;
                case 'u': {
                    /* \uXXXX -> UTF-8 (BMP, no surrogate pair handling) */
                    if (i + 6 > p->konum) {
                        cikti[ci++] = c; cikti[ci++] = n; i += 2; break;
                    }
                    unsigned int kod = 0;
                    int gecerli = 1;
                    for (int k = 0; k < 4; k++) {
                        char hc = p->baslangic[i + 2 + k];
                        kod <<= 4;
                        if (hc >= '0' && hc <= '9') kod |= (unsigned)(hc - '0');
                        else if (hc >= 'a' && hc <= 'f') kod |= (unsigned)(hc - 'a' + 10);
                        else if (hc >= 'A' && hc <= 'F') kod |= (unsigned)(hc - 'A' + 10);
                        else { gecerli = 0; break; }
                    }
                    if (!gecerli) {
                        cikti[ci++] = c; cikti[ci++] = n; i += 2; break;
                    }
                    if (kod < 0x80) {
                        cikti[ci++] = (char)kod;
                    } else if (kod < 0x800) {
                        cikti[ci++] = (char)(0xC0 | (kod >> 6));
                        cikti[ci++] = (char)(0x80 | (kod & 0x3F));
                    } else {
                        /* BMP (U+D800-U+DFFF surrogate'lari basit aktarim) */
                        cikti[ci++] = (char)(0xE0 | (kod >> 12));
                        cikti[ci++] = (char)(0x80 | ((kod >> 6) & 0x3F));
                        cikti[ci++] = (char)(0x80 | (kod & 0x3F));
                    }
                    i += 6;
                    break;
                }
                default:
                    cikti[ci++] = c;
                    cikti[ci++] = n;
                    i += 2;
                    break;
            }
        } else {
            cikti[ci++] = c;
            i++;
        }
    }
    cikti[ci] = '\0';
    p->konum++;  /* kapanis " */
    if (out_uz) *out_uz = ci;
    return cikti;
}

/* === Sayi === */

static JsonDeger *ayrist_sayi(JsonParser *p) {
    int bas = p->konum;
    int kesirli_mi = 0;
    if (simdiki(p) == '-') p->konum++;
    while (!eof(p) && isdigit((unsigned char)simdiki(p))) p->konum++;
    if (simdiki(p) == '.') {
        kesirli_mi = 1;
        p->konum++;
        while (!eof(p) && isdigit((unsigned char)simdiki(p))) p->konum++;
    }
    if (simdiki(p) == 'e' || simdiki(p) == 'E') {
        kesirli_mi = 1;
        p->konum++;
        if (simdiki(p) == '+' || simdiki(p) == '-') p->konum++;
        while (!eof(p) && isdigit((unsigned char)simdiki(p))) p->konum++;
    }
    int n = p->konum - bas;
    if (n == 0) { hata_kur(p, "sayi bekleniyor"); return NULL; }

    /* Stringden parse: null-terminated kopya gerek */
    char tampon[64];
    if (n >= (int)sizeof(tampon)) n = (int)sizeof(tampon) - 1;
    memcpy(tampon, p->baslangic + bas, (size_t)n);
    tampon[n] = '\0';

    JsonDeger *d;
    if (kesirli_mi) {
        d = deger_olustur(p, JSON_KESIRLI);
        if (d) d->veri.kesirli = strtod(tampon, NULL);
    } else {
        d = deger_olustur(p, JSON_TAMSAYI);
        if (d) d->veri.tamsayi = strtoll(tampon, NULL, 10);
    }
    return d;
}

/* === Dizi === */

static JsonDeger *ayrist_dizi(JsonParser *p) {
    if (simdiki(p) != '[') { hata_kur(p, "'[' bekleniyor"); return NULL; }
    p->konum++;
    atla_bosluk(p);

    /* Linked list buyu, sonra array'e cevir */
    typedef struct Link { JsonDeger *d; struct Link *son; } Link;
    Link *bas = NULL, *son = NULL;
    int sayi = 0;

    if (simdiki(p) != ']') {
        for (;;) {
            atla_bosluk(p);
            JsonDeger *e = ayrist_deger(p);
            if (!e) return NULL;
            Link *l = (Link *)arena_ayir(p->arena, sizeof(Link));
            if (!l) { hata_kur(p, "arena bos"); return NULL; }
            l->d = e; l->son = NULL;
            if (son) son->son = l; else bas = l;
            son = l;
            sayi++;
            atla_bosluk(p);
            if (simdiki(p) == ',') { p->konum++; continue; }
            break;
        }
    }
    atla_bosluk(p);
    if (simdiki(p) != ']') { hata_kur(p, "']' bekleniyor"); return NULL; }
    p->konum++;

    JsonDeger *d = deger_olustur(p, JSON_DIZI);
    if (!d) return NULL;
    if (sayi > 0) {
        JsonDeger **arr = (JsonDeger **)arena_ayir(p->arena,
            sizeof(JsonDeger *) * (size_t)sayi);
        if (!arr) { hata_kur(p, "arena bos"); return NULL; }
        int i = 0;
        for (Link *l = bas; l; l = l->son) arr[i++] = l->d;
        d->veri.dizi.elemanlar = arr;
        d->veri.dizi.sayi = sayi;
    }
    return d;
}

/* === Nesne === */

static JsonDeger *ayrist_nesne(JsonParser *p) {
    if (simdiki(p) != '{') { hata_kur(p, "'{' bekleniyor"); return NULL; }
    p->konum++;
    atla_bosluk(p);

    JsonDeger *d = deger_olustur(p, JSON_NESNE);
    if (!d) return NULL;

    JsonAlan *son = NULL;

    if (simdiki(p) != '}') {
        for (;;) {
            atla_bosluk(p);
            int ad_uz = 0;
            const char *ad = ayrist_metin(p, &ad_uz);
            if (!ad) return NULL;
            atla_bosluk(p);
            if (simdiki(p) != ':') {
                hata_kur(p, "':' bekleniyor (nesne)");
                return NULL;
            }
            p->konum++;
            atla_bosluk(p);
            JsonDeger *v = ayrist_deger(p);
            if (!v) return NULL;
            JsonAlan *a =
                (JsonAlan *)arena_ayir_sifir(p->arena, sizeof(JsonAlan));
            if (!a) { hata_kur(p, "arena bos"); return NULL; }
            a->ad = ad;
            a->ad_uz = ad_uz;
            a->deger = v;
            if (son) son->sonraki = a;
            else d->veri.nesne.bas = a;
            son = a;
            d->veri.nesne.alan_sayi++;
            atla_bosluk(p);
            if (simdiki(p) == ',') { p->konum++; continue; }
            break;
        }
    }
    atla_bosluk(p);
    if (simdiki(p) != '}') { hata_kur(p, "'}' bekleniyor"); return NULL; }
    p->konum++;

    return d;
}

/* === Genel deger === */

static JsonDeger *ayrist_deger(JsonParser *p) {
    atla_bosluk(p);
    if (eof(p)) { hata_kur(p, "beklenmedik eof"); return NULL; }
    char c = simdiki(p);
    if (c == '{') return ayrist_nesne(p);
    if (c == '[') return ayrist_dizi(p);
    if (c == '"') {
        JsonDeger *d = deger_olustur(p, JSON_METIN);
        if (!d) return NULL;
        d->veri.str.metin = ayrist_metin(p, &d->veri.str.uzunluk);
        if (!d->veri.str.metin) return NULL;
        return d;
    }
    if (c == 't' && eslesir(p, "true")) {
        p->konum += 4;
        JsonDeger *d = deger_olustur(p, JSON_BOOL);
        if (d) d->veri.bool_deger = 1;
        return d;
    }
    if (c == 'f' && eslesir(p, "false")) {
        p->konum += 5;
        JsonDeger *d = deger_olustur(p, JSON_BOOL);
        if (d) d->veri.bool_deger = 0;
        return d;
    }
    if (c == 'n' && eslesir(p, "null")) {
        p->konum += 4;
        return deger_olustur(p, JSON_NULL);
    }
    if (c == '-' || (c >= '0' && c <= '9')) {
        return ayrist_sayi(p);
    }
    hata_kur(p, "tanimsiz deger");
    return NULL;
}

/* === Public API === */

JsonDeger *json_ayrist(Arena *a, const char *kaynak, int uzunluk,
                        const char **out_hata) {
    JsonParser p;
    p.arena = a;
    p.baslangic = kaynak;
    p.uzunluk = uzunluk;
    p.konum = 0;
    p.hata = NULL;

    JsonDeger *d = ayrist_deger(&p);
    if (!d || p.hata) {
        if (out_hata) *out_hata = p.hata ? p.hata : "parse hatasi";
        return NULL;
    }
    if (out_hata) *out_hata = NULL;
    return d;
}

JsonDeger *json_alan(const JsonDeger *nesne, const char *ad) {
    if (!nesne || nesne->tip != JSON_NESNE || !ad) return NULL;
    int ad_uz = (int)strlen(ad);
    for (JsonAlan *a = nesne->veri.nesne.bas; a; a = a->sonraki) {
        if (a->ad_uz == ad_uz && memcmp(a->ad, ad, (size_t)ad_uz) == 0) {
            return a->deger;
        }
    }
    return NULL;
}

const char *json_metin(const JsonDeger *d, int *out_uz) {
    if (!d || d->tip != JSON_METIN) {
        if (out_uz) *out_uz = 0;
        return NULL;
    }
    if (out_uz) *out_uz = d->veri.str.uzunluk;
    return d->veri.str.metin;
}

long long json_tamsayi(const JsonDeger *d) {
    if (!d) return 0;
    if (d->tip == JSON_TAMSAYI) return d->veri.tamsayi;
    if (d->tip == JSON_KESIRLI) return (long long)d->veri.kesirli;
    return 0;
}

int json_dizi_sayi(const JsonDeger *d) {
    if (!d || d->tip != JSON_DIZI) return 0;
    return d->veri.dizi.sayi;
}

JsonDeger *json_dizi_eleman(const JsonDeger *d, int i) {
    if (!d || d->tip != JSON_DIZI) return NULL;
    if (i < 0 || i >= d->veri.dizi.sayi) return NULL;
    return d->veri.dizi.elemanlar[i];
}

/* === Yazici (malloc tabanli) === */

void json_yazici_baslat(JsonYazici *y) {
    y->tampon = NULL;
    y->kullanilan = 0;
    y->kapasite = 0;
}

void json_yazici_serbest(JsonYazici *y) {
    free(y->tampon);
    y->tampon = NULL;
    y->kullanilan = 0;
    y->kapasite = 0;
}

static void yazici_buyu(JsonYazici *y, size_t en_az) {
    size_t yeni = y->kapasite == 0 ? 256 : y->kapasite * 2;
    while (yeni < en_az) yeni *= 2;
    char *p = (char *)realloc(y->tampon, yeni);
    if (!p) return;
    y->tampon = p;
    y->kapasite = yeni;
}

void json_yaz_n(JsonYazici *y, const char *s, size_t n) {
    if (y->kullanilan + n + 1 > y->kapasite) yazici_buyu(y, y->kullanilan + n + 1);
    if (!y->tampon) return;
    memcpy(y->tampon + y->kullanilan, s, n);
    y->kullanilan += n;
    y->tampon[y->kullanilan] = '\0';
}

void json_yaz(JsonYazici *y, const char *s) {
    json_yaz_n(y, s, strlen(s));
}

void json_yaz_int(JsonYazici *y, long long v) {
    char tampon[32];
    int n = snprintf(tampon, sizeof(tampon), "%lld", v);
    if (n > 0) json_yaz_n(y, tampon, (size_t)n);
}

void json_yaz_metin_lit_n(JsonYazici *y, const char *s, size_t n) {
    json_yaz_n(y, "\"", 1);
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
            case '"':  json_yaz_n(y, "\\\"", 2); break;
            case '\\': json_yaz_n(y, "\\\\", 2); break;
            case '\n': json_yaz_n(y, "\\n", 2); break;
            case '\r': json_yaz_n(y, "\\r", 2); break;
            case '\t': json_yaz_n(y, "\\t", 2); break;
            case '\b': json_yaz_n(y, "\\b", 2); break;
            case '\f': json_yaz_n(y, "\\f", 2); break;
            default:
                if (c < 0x20) {
                    char esc[8];
                    int en = snprintf(esc, sizeof(esc), "\\u%04x", c);
                    if (en > 0) json_yaz_n(y, esc, (size_t)en);
                } else {
                    char tek = (char)c;
                    json_yaz_n(y, &tek, 1);
                }
                break;
        }
    }
    json_yaz_n(y, "\"", 1);
}

void json_yaz_metin_lit(JsonYazici *y, const char *s) {
    json_yaz_metin_lit_n(y, s, strlen(s));
}
