#include "lexer.h"
#include <string.h>

/*
 * Anahtar kelime tablosu — UTF-8 byte sırasına göre sıralı.
 * Binary search: önce memcmp (min uzunluk kadar), eşitse uzunluk karşılaştırması.
 *
 * NOT: C'de Türkçe harflerin hex escape'leri sonrası hex rakam (a-f, A-F)
 * gelirse string concatenation ile ayrılmalıdır:
 *   "de\xc4\x9f" "er"  →  değer (doğru)
 *   "de\xc4\x9fer"     →  yanlış! \x9fe olarak okunur
 */

typedef struct {
    const char *metin;
    int uzunluk;
    TokenTipi tip;
} AnahtarKelime;

/* Sıralama: Python sorted(key=lambda x: x[1]) ile doğrulandı.
 * Her satırdaki uzunluk = UTF-8 byte uzunluğu (strlen). */
static const AnahtarKelime tablo[] = {
    {"bo\xc5\x9f",                       4, TOK_BOS       },  /* boş         */
    {"b\xc3\xb6lge",                     6, TOK_BOLGE     },  /* bölge       */
    {"de\xc4\x9f" "er",                  6, TOK_DEGER     },  /* değer       */
    {"de\xc4\x9f" "il",                  6, TOK_DEGIL     },  /* değil       */
    {"de\xc4\x9f" "ilse",                8, TOK_DEGILSE   },  /* değilse     */
    {"de\xc4\x9f" "i\xc5\x9fken",       10, TOK_DEGISKEN  },  /* değişken    */
    {"do\xc4\x9fru",                     6, TOK_DOGRU     },  /* doğru       */
    {"d\xc4\xb1\xc5\x9f" "a",           6, TOK_DISA      },  /* dışa        */
    {"e\xc4\x9f" "er",                   5, TOK_EGER      },  /* eğer        */
    {"e\xc5\x9fle\xc5\x9f",             7, TOK_ESLES     },  /* eşleş       */
    {"ger\xc3\xa7" "ekzamanl\xc4\xb1", 15, TOK_GERCEKZAMANLI},/* gerçekzamanlı */
    {"g\xc3\xbcvensiz",                  9, TOK_GUVENSIZ  },  /* güvensiz    */
    {"hata",                              4, TOK_HATA      },  /* hata        */
    {"hi\xc3\xa7",                       4, TOK_HIC       },  /* hiç         */
    {"iken",                              4, TOK_IKEN      },  /* iken        */
    {"imha",                              4, TOK_IMHA      },  /* imha        */
    {"i\xc3\xa7in",                      5, TOK_ICIN      },  /* için        */
    {"i\xc5\x9flev",                     6, TOK_ISLEV     },  /* işlev       */
    {"kendin",                            6, TOK_KENDIN    },  /* kendin      */
    {"kullan",                            6, TOK_KULLAN    },  /* kullan      */
    {"mod\xc3\xbcl",                     6, TOK_MODUL     },  /* modül       */
    {"olarak",                            6, TOK_OLARAK    },  /* olarak      */
    {"sabit",                             5, TOK_SABIT     },  /* sabit       */
    {"sabits\xc3\xbc" "re",             10, TOK_SABITSURE },  /* sabitsüre   */
    {"se\xc3\xa7imlik",                  9, TOK_SECIMLIK  },  /* seçimlik    */
    {"sonu\xc3\xa7",                     6, TOK_SONUC     },  /* sonuç       */
    {"tamam",                             5, TOK_TAMAM     },  /* tamam       */
    {"tekkez",                            6, TOK_TEKKEZ    },  /* tekkez      */
    {"uygula",                            6, TOK_UYGULA    },  /* uygula      */
    {"ve",                                2, TOK_VE        },  /* ve          */
    {"ver",                               3, TOK_VER       },  /* ver         */
    {"veya",                              4, TOK_VEYA      },  /* veya        */
    {"yanl\xc4\xb1\xc5\x9f",            8, TOK_YANLIS    },  /* yanlış      */
    {"yap\xc4\xb1",                      5, TOK_YAPI      },  /* yapı        */
    {"\xc3\xb6zellik",                   8, TOK_OZELLIK   },  /* özellik     */
};

static const int TABLO_BOYUT = sizeof(tablo) / sizeof(tablo[0]);

TokenTipi anahtar_kelime_bul(const char *metin, int uzunluk) {
    int sol = 0;
    int sag = TABLO_BOYUT - 1;

    while (sol <= sag) {
        int orta = (sol + sag) / 2;
        const AnahtarKelime *ak = &tablo[orta];

        int min_uz = uzunluk < ak->uzunluk ? uzunluk : ak->uzunluk;
        int sonuc = memcmp(metin, ak->metin, min_uz);
        if (sonuc == 0) {
            sonuc = uzunluk - ak->uzunluk;
        }

        if (sonuc < 0) {
            sag = orta - 1;
        } else if (sonuc > 0) {
            sol = orta + 1;
        } else {
            return ak->tip;
        }
    }

    return TOK_TANIMLAYICI;
}
