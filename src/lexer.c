#include "lexer.h"
#include "utf8.h"
#include "hata.h"
#include <string.h>
#include <stdio.h>

extern TokenTipi anahtar_kelime_bul(const char *metin, int uzunluk);

/* ===== Yardımcı fonksiyonlar ===== */

static int bitti_mi(Lexer *l) { return *l->simdiki == '\0'; }
static char simdiki(Lexer *l) { return *l->simdiki; }
static char sonraki(Lexer *l) { return bitti_mi(l) ? '\0' : l->simdiki[1]; }

static char ilerle(Lexer *l) {
    char c = *l->simdiki;
    l->simdiki++;
    if (c == '\n') { l->satir++; l->sutun = 1; }
    else { l->sutun++; }
    return c;
}

static int eslesirse_ilerle(Lexer *l, char beklenen) {
    if (bitti_mi(l) || *l->simdiki != beklenen) return 0;
    ilerle(l);
    return 1;
}

static Token token_olustur(TokenTipi tip, const char *bas, int uzunluk,
                           int satir, int sutun) {
    Token t;
    t.tip = tip;
    t.baslangic = bas;
    t.uzunluk = uzunluk;
    t.satir = satir;
    t.sutun = sutun;
    return t;
}

static Token hata_token(Lexer *l, const char *bas, int satir, int sutun,
                        const char *kod, const char *mesaj, const char *ipucu) {
    hata_raporla(l->dosya_adi, l->kaynak, satir, sutun, kod, mesaj, ipucu);
    return token_olustur(TOK_HATALI, bas, (int)(l->simdiki - bas), satir, sutun);
}

/* ===== Boşluk ve yorum atlama ===== */

static void bosluk_atla(Lexer *l) {
    while (!bitti_mi(l)) {
        unsigned char c = (unsigned char)simdiki(l);

        if (utf8_bosluk_mu(c)) {
            if (c == '\r' && sonraki(l) == '\n') { ilerle(l); ilerle(l); }
            else { ilerle(l); }
            continue;
        }

        if (c == '/' && sonraki(l) == '/') {
            ilerle(l); ilerle(l);
            while (!bitti_mi(l) && simdiki(l) != '\n') ilerle(l);
            continue;
        }

        if (c == '/' && sonraki(l) == '*') {
            ilerle(l); ilerle(l);
            int derinlik = 1;
            while (!bitti_mi(l) && derinlik > 0) {
                if (simdiki(l) == '/' && sonraki(l) == '*') {
                    ilerle(l); ilerle(l); derinlik++;
                } else if (simdiki(l) == '*' && sonraki(l) == '/') {
                    ilerle(l); ilerle(l); derinlik--;
                } else {
                    ilerle(l);
                }
            }
            continue;
        }

        break;
    }
}

/* ===== Tanımlayıcı okuma ===== */

static Token tanimlayici_oku(Lexer *l) {
    const char *bas = l->simdiki;
    int bas_satir = l->satir;
    int bas_sutun = l->sutun;

    int byte_uz;
    if (utf8_tanimlayici_baslangic_mi(l->simdiki, &byte_uz)) {
        for (int i = 0; i < byte_uz; i++) ilerle(l);
    }

    while (!bitti_mi(l) && utf8_tanimlayici_devam_mi(l->simdiki, &byte_uz)) {
        for (int i = 0; i < byte_uz; i++) ilerle(l);
    }

    int uzunluk = (int)(l->simdiki - bas);
    TokenTipi tip = anahtar_kelime_bul(bas, uzunluk);
    return token_olustur(tip, bas, uzunluk, bas_satir, bas_sutun);
}

/* ===== Sayı okuma ===== */

static Token sayi_oku(Lexer *l) {
    const char *bas = l->simdiki;
    int bas_satir = l->satir;
    int bas_sutun = l->sutun;
    int ondalik = 0;

    /* 0x, 0b, 0o prefix kontrolü */
    if (simdiki(l) == '0' && !bitti_mi(l)) {
        char sonra = sonraki(l);
        if (sonra == 'x' || sonra == 'X') {
            ilerle(l); ilerle(l);
            while (!bitti_mi(l) && (utf8_hex_rakam_mi((unsigned char)simdiki(l)) || simdiki(l) == '_'))
                ilerle(l);
            return token_olustur(TOK_TAMSAYI, bas, (int)(l->simdiki - bas), bas_satir, bas_sutun);
        }
        if (sonra == 'b' || sonra == 'B') {
            ilerle(l); ilerle(l);
            while (!bitti_mi(l) && (simdiki(l) == '0' || simdiki(l) == '1' || simdiki(l) == '_'))
                ilerle(l);
            return token_olustur(TOK_TAMSAYI, bas, (int)(l->simdiki - bas), bas_satir, bas_sutun);
        }
        if (sonra == 'o' || sonra == 'O') {
            ilerle(l); ilerle(l);
            while (!bitti_mi(l) && ((simdiki(l) >= '0' && simdiki(l) <= '7') || simdiki(l) == '_'))
                ilerle(l);
            return token_olustur(TOK_TAMSAYI, bas, (int)(l->simdiki - bas), bas_satir, bas_sutun);
        }
    }

    while (!bitti_mi(l) && (utf8_rakam_mi((unsigned char)simdiki(l)) || simdiki(l) == '_'))
        ilerle(l);

    if (!bitti_mi(l) && simdiki(l) == '.' && sonraki(l) != '.') {
        ondalik = 1;
        ilerle(l);
        while (!bitti_mi(l) && (utf8_rakam_mi((unsigned char)simdiki(l)) || simdiki(l) == '_'))
            ilerle(l);
    }

    if (!bitti_mi(l) && (simdiki(l) == 'e' || simdiki(l) == 'E')) {
        ondalik = 1;
        ilerle(l);
        if (!bitti_mi(l) && (simdiki(l) == '+' || simdiki(l) == '-'))
            ilerle(l);
        while (!bitti_mi(l) && (utf8_rakam_mi((unsigned char)simdiki(l)) || simdiki(l) == '_'))
            ilerle(l);
    }

    TokenTipi tip = ondalik ? TOK_ONDALIK : TOK_TAMSAYI;
    return token_olustur(tip, bas, (int)(l->simdiki - bas), bas_satir, bas_sutun);
}

/* ===== Metin okuma ===== */

static Token metin_oku(Lexer *l) {
    const char *bas = l->simdiki;
    int bas_satir = l->satir;
    int bas_sutun = l->sutun;

    ilerle(l); /* açılış " */

    while (!bitti_mi(l) && simdiki(l) != '"') {
        if (simdiki(l) == '\\') {
            ilerle(l); /* \ */
            if (!bitti_mi(l)) ilerle(l); /* kaçış karakteri */
        } else if (simdiki(l) == '\n') {
            return hata_token(l, bas, bas_satir, bas_sutun,
                "L001", "kapatilmamis metin literali",
                "metin literali ayni satirda kapatilmalidir");
        } else {
            ilerle(l);
        }
    }

    if (bitti_mi(l)) {
        return hata_token(l, bas, bas_satir, bas_sutun,
            "L001", "kapatilmamis metin literali", NULL);
    }

    ilerle(l); /* kapanış " */
    return token_olustur(TOK_METIN, bas, (int)(l->simdiki - bas), bas_satir, bas_sutun);
}

/* ===== Raw metin okuma ===== */

static Token ham_metin_oku(Lexer *l) {
    const char *bas = l->simdiki;
    int bas_satir = l->satir;
    int bas_sutun = l->sutun;

    ilerle(l); /* r */

    int hash_sayisi = 0;
    while (!bitti_mi(l) && simdiki(l) == '#') {
        hash_sayisi++;
        ilerle(l);
    }

    if (bitti_mi(l) || simdiki(l) != '"') {
        return hata_token(l, bas, bas_satir, bas_sutun,
            "L011", "gecersiz raw metin baslangici", NULL);
    }
    ilerle(l); /* açılış " */

    while (!bitti_mi(l)) {
        if (simdiki(l) == '"') {
            ilerle(l);
            int kapanan_hash = 0;
            while (!bitti_mi(l) && simdiki(l) == '#' && kapanan_hash < hash_sayisi) {
                kapanan_hash++;
                ilerle(l);
            }
            if (kapanan_hash == hash_sayisi) {
                return token_olustur(TOK_HAM_METIN, bas, (int)(l->simdiki - bas),
                                     bas_satir, bas_sutun);
            }
        } else {
            ilerle(l);
        }
    }

    return hata_token(l, bas, bas_satir, bas_sutun,
        "L002", "kapatilmamis raw metin literali", NULL);
}

/* ===== Karakter okuma ===== */

static Token karakter_oku(Lexer *l) {
    const char *bas = l->simdiki;
    int bas_satir = l->satir;
    int bas_sutun = l->sutun;

    ilerle(l); /* açılış ' */

    if (bitti_mi(l) || simdiki(l) == '\'') {
        if (!bitti_mi(l)) ilerle(l);
        return hata_token(l, bas, bas_satir, bas_sutun,
            "L009", "bos karakter literali",
            "karakter literali tam olarak bir karakter icermelidir");
    }

    if (simdiki(l) == '\\') {
        ilerle(l);
        if (!bitti_mi(l)) ilerle(l);
    } else {
        int byte_uz = utf8_karakter_uzunlugu((unsigned char)simdiki(l));
        if (byte_uz == 0) byte_uz = 1;
        for (int i = 0; i < byte_uz; i++) {
            if (!bitti_mi(l)) ilerle(l);
        }
    }

    if (bitti_mi(l) || simdiki(l) != '\'') {
        while (!bitti_mi(l) && simdiki(l) != '\'' && simdiki(l) != '\n') ilerle(l);
        if (!bitti_mi(l) && simdiki(l) == '\'') ilerle(l);
        return hata_token(l, bas, bas_satir, bas_sutun,
            "L010", "karakter literalinde birden fazla karakter",
            "karakter literali tam olarak bir karakter icermelidir");
    }

    ilerle(l); /* kapanış ' */
    return token_olustur(TOK_KARAKTER, bas, (int)(l->simdiki - bas), bas_satir, bas_sutun);
}

/* ===== Ana tokenizer ===== */

void lexer_baslat(Lexer *lexer, const char *kaynak, const char *dosya_adi) {
    lexer->kaynak = kaynak;
    lexer->simdiki = kaynak;
    lexer->satir = 1;
    lexer->sutun = 1;
    lexer->dosya_adi = dosya_adi;
}

Token lexer_sonraki_token(Lexer *l) {
    bosluk_atla(l);

    if (bitti_mi(l)) {
        return token_olustur(TOK_DOSYA_SONU, l->simdiki, 0, l->satir, l->sutun);
    }

    const char *bas = l->simdiki;
    int bas_satir = l->satir;
    int bas_sutun = l->sutun;
    char c = simdiki(l);

    /* Tanımlayıcı veya anahtar kelime */
    if (utf8_tanimlayici_baslangic_mi(l->simdiki, NULL)) {
        if (c == 'r' && !bitti_mi(l)) {
            char sonra = sonraki(l);
            if (sonra == '"') return ham_metin_oku(l);
            if (sonra == '#') {
                const char *bak = l->simdiki + 1;
                while (*bak == '#') bak++;
                if (*bak == '"') return ham_metin_oku(l);
            }
        }
        return tanimlayici_oku(l);
    }

    /* Sayı */
    if (utf8_rakam_mi((unsigned char)c)) return sayi_oku(l);

    /* Metin */
    if (c == '"') return metin_oku(l);

    /* Karakter */
    if (c == '\'') return karakter_oku(l);

    /* Operatörler ve noktalama */
    ilerle(l);

    switch (c) {
        case '+':
            if (eslesirse_ilerle(l, '=')) return token_olustur(TOK_ARTI_ESIT, bas, 2, bas_satir, bas_sutun);
            return token_olustur(TOK_ARTI, bas, 1, bas_satir, bas_sutun);
        case '-':
            if (eslesirse_ilerle(l, '>')) return token_olustur(TOK_OK, bas, 2, bas_satir, bas_sutun);
            if (eslesirse_ilerle(l, '=')) return token_olustur(TOK_EKSI_ESIT, bas, 2, bas_satir, bas_sutun);
            return token_olustur(TOK_EKSI, bas, 1, bas_satir, bas_sutun);
        case '*':
            if (eslesirse_ilerle(l, '=')) return token_olustur(TOK_YILDIZ_ESIT, bas, 2, bas_satir, bas_sutun);
            return token_olustur(TOK_YILDIZ, bas, 1, bas_satir, bas_sutun);
        case '/':
            if (eslesirse_ilerle(l, '=')) return token_olustur(TOK_BOLU_ESIT, bas, 2, bas_satir, bas_sutun);
            return token_olustur(TOK_BOLU, bas, 1, bas_satir, bas_sutun);
        case '%':
            if (eslesirse_ilerle(l, '=')) return token_olustur(TOK_MOD_ESIT, bas, 2, bas_satir, bas_sutun);
            return token_olustur(TOK_MOD, bas, 1, bas_satir, bas_sutun);
        case '=':
            if (eslesirse_ilerle(l, '=')) return token_olustur(TOK_ESIT_ESIT, bas, 2, bas_satir, bas_sutun);
            if (eslesirse_ilerle(l, '>')) return token_olustur(TOK_KALIN_OK, bas, 2, bas_satir, bas_sutun);
            return token_olustur(TOK_ESIT, bas, 1, bas_satir, bas_sutun);
        case '!':
            if (eslesirse_ilerle(l, '=')) return token_olustur(TOK_ESIT_DEGIL, bas, 2, bas_satir, bas_sutun);
            return hata_token(l, bas, bas_satir, bas_sutun, "L005",
                "beklenmeyen karakter '!'", "KEMGU'da '!' yerine 'degil' kullanin");
        case '<':
            if (eslesirse_ilerle(l, '=')) return token_olustur(TOK_KUCUK_ESIT, bas, 2, bas_satir, bas_sutun);
            if (eslesirse_ilerle(l, '<')) return token_olustur(TOK_SOLA_KAYDIR, bas, 2, bas_satir, bas_sutun);
            return token_olustur(TOK_KUCUK, bas, 1, bas_satir, bas_sutun);
        case '>':
            if (eslesirse_ilerle(l, '=')) return token_olustur(TOK_BUYUK_ESIT, bas, 2, bas_satir, bas_sutun);
            if (eslesirse_ilerle(l, '>')) return token_olustur(TOK_SAGA_KAYDIR, bas, 2, bas_satir, bas_sutun);
            return token_olustur(TOK_BUYUK, bas, 1, bas_satir, bas_sutun);
        case '&': return token_olustur(TOK_VE_BIT, bas, 1, bas_satir, bas_sutun);
        case '|': return token_olustur(TOK_VEYA_BIT, bas, 1, bas_satir, bas_sutun);
        case '^': return token_olustur(TOK_OZVEYA_BIT, bas, 1, bas_satir, bas_sutun);
        case '~': return token_olustur(TOK_DEGIL_BIT, bas, 1, bas_satir, bas_sutun);
        case '.':
            if (eslesirse_ilerle(l, '.')) {
                if (eslesirse_ilerle(l, '.')) return token_olustur(TOK_UC_NOKTA, bas, 3, bas_satir, bas_sutun);
                return token_olustur(TOK_ARALIK, bas, 2, bas_satir, bas_sutun);
            }
            return token_olustur(TOK_NOKTA, bas, 1, bas_satir, bas_sutun);
        case ':':
            if (eslesirse_ilerle(l, ':')) return token_olustur(TOK_CIFT_IKI_NOKTA, bas, 2, bas_satir, bas_sutun);
            return token_olustur(TOK_IKI_NOKTA, bas, 1, bas_satir, bas_sutun);
        case '(': return token_olustur(TOK_SOL_PAREN, bas, 1, bas_satir, bas_sutun);
        case ')': return token_olustur(TOK_SAG_PAREN, bas, 1, bas_satir, bas_sutun);
        case '{': return token_olustur(TOK_SOL_SUSLU, bas, 1, bas_satir, bas_sutun);
        case '}': return token_olustur(TOK_SAG_SUSLU, bas, 1, bas_satir, bas_sutun);
        case '[': return token_olustur(TOK_SOL_KOSELI, bas, 1, bas_satir, bas_sutun);
        case ']': return token_olustur(TOK_SAG_KOSELI, bas, 1, bas_satir, bas_sutun);
        case ',': return token_olustur(TOK_VIRGUL, bas, 1, bas_satir, bas_sutun);
        case ';': return token_olustur(TOK_NOKTALI_VIRGUL, bas, 1, bas_satir, bas_sutun);
        default:
            return hata_token(l, bas, bas_satir, bas_sutun, "L005",
                "beklenmeyen karakter", NULL);
    }
}

const char *token_tipi_adi(TokenTipi tip) {
    switch (tip) {
        case TOK_EGER: return "EGER";
        case TOK_DEGILSE: return "DEGILSE";
        case TOK_ICIN: return "ICIN";
        case TOK_IKEN: return "IKEN";
        case TOK_ESLES: return "ESLES";
        case TOK_VER: return "VER";
        case TOK_ISLEV: return "ISLEV";
        case TOK_YAPI: return "YAPI";
        case TOK_OZELLIK: return "OZELLIK";
        case TOK_MODUL: return "MODUL";
        case TOK_DEGISKEN: return "DEGISKEN";
        case TOK_SABIT: return "SABIT";
        case TOK_DOGRU: return "DOGRU";
        case TOK_YANLIS: return "YANLIS";
        case TOK_BOS: return "BOS";
        case TOK_VE: return "VE";
        case TOK_VEYA: return "VEYA";
        case TOK_DEGIL: return "DEGIL";
        case TOK_KULLAN: return "KULLAN";
        case TOK_DISA: return "DISA";
        case TOK_TAMAM: return "TAMAM";
        case TOK_HATA: return "HATA";
        case TOK_BOLGE: return "BOLGE";
        case TOK_UYGULA: return "UYGULA";
        case TOK_KENDIN: return "KENDIN";
        case TOK_SECIMLIK: return "SECIMLIK";
        case TOK_SONUC: return "SONUC";
        case TOK_DEGER: return "DEGER";
        case TOK_HIC: return "HIC";
        case TOK_GUVENSIZ: return "GUVENSIZ";
        case TOK_TEKKEZ: return "TEKKEZ";
        case TOK_IMHA: return "IMHA";
        case TOK_OLARAK: return "OLARAK";
        case TOK_SABITSURE: return "SABITSURE";
        case TOK_YETKI: return "YETKI";
        case TOK_DELEGE: return "DELEGE";
        case TOK_GERI_AL: return "GERI_AL";
        case TOK_TAMSAYI: return "TAMSAYI";
        case TOK_ONDALIK: return "ONDALIK";
        case TOK_METIN: return "METIN";
        case TOK_HAM_METIN: return "HAM_METIN";
        case TOK_KARAKTER: return "KARAKTER";
        case TOK_TANIMLAYICI: return "TANIMLAYICI";
        case TOK_ARTI: return "ARTI";
        case TOK_EKSI: return "EKSI";
        case TOK_YILDIZ: return "YILDIZ";
        case TOK_BOLU: return "BOLU";
        case TOK_MOD: return "MOD";
        case TOK_ESIT_ESIT: return "ESIT_ESIT";
        case TOK_ESIT_DEGIL: return "ESIT_DEGIL";
        case TOK_KUCUK: return "KUCUK";
        case TOK_BUYUK: return "BUYUK";
        case TOK_KUCUK_ESIT: return "KUCUK_ESIT";
        case TOK_BUYUK_ESIT: return "BUYUK_ESIT";
        case TOK_ESIT: return "ESIT";
        case TOK_ARTI_ESIT: return "ARTI_ESIT";
        case TOK_EKSI_ESIT: return "EKSI_ESIT";
        case TOK_YILDIZ_ESIT: return "YILDIZ_ESIT";
        case TOK_BOLU_ESIT: return "BOLU_ESIT";
        case TOK_MOD_ESIT: return "MOD_ESIT";
        case TOK_VE_BIT: return "VE_BIT";
        case TOK_VEYA_BIT: return "VEYA_BIT";
        case TOK_OZVEYA_BIT: return "OZVEYA_BIT";
        case TOK_DEGIL_BIT: return "DEGIL_BIT";
        case TOK_SOLA_KAYDIR: return "SOLA_KAYDIR";
        case TOK_SAGA_KAYDIR: return "SAGA_KAYDIR";
        case TOK_OK: return "OK";
        case TOK_KALIN_OK: return "KALIN_OK";
        case TOK_CIFT_IKI_NOKTA: return "CIFT_IKI_NOKTA";
        case TOK_NOKTA: return "NOKTA";
        case TOK_ARALIK: return "ARALIK";
        case TOK_UC_NOKTA: return "UC_NOKTA";
        case TOK_SOL_PAREN: return "SOL_PAREN";
        case TOK_SAG_PAREN: return "SAG_PAREN";
        case TOK_SOL_SUSLU: return "SOL_SUSLU";
        case TOK_SAG_SUSLU: return "SAG_SUSLU";
        case TOK_SOL_KOSELI: return "SOL_KOSELI";
        case TOK_SAG_KOSELI: return "SAG_KOSELI";
        case TOK_VIRGUL: return "VIRGUL";
        case TOK_IKI_NOKTA: return "IKI_NOKTA";
        case TOK_NOKTALI_VIRGUL: return "NOKTALI_VIRGUL";
        case TOK_DOSYA_SONU: return "DOSYA_SONU";
        case TOK_HATALI: return "HATALI";
    }
    return "BILINMEYEN";
}
