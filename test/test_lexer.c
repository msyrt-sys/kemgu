#include "lexer.h"
#include <stdio.h>
#include <string.h>

static int toplam_test = 0;
static int basarili = 0;
static int basarisiz = 0;

static void test_sonuc(const char *ad, int durum) {
    toplam_test++;
    if (durum) { basarili++; printf("  [%d] %s ... \xe2\x9c\x93\n", toplam_test, ad); }
    else { basarisiz++; printf("  [%d] %s ... \xe2\x9c\x97\n", toplam_test, ad); }
}

static void tek_token_test(const char *ad, const char *kaynak, TokenTipi beklenen, const char *beklenen_metin) {
    Lexer l;
    lexer_baslat(&l, kaynak, "test");
    Token t = lexer_sonraki_token(&l);
    int tip_ok = (t.tip == beklenen);
    int metin_ok = beklenen_metin ? (t.uzunluk == (int)strlen(beklenen_metin) && memcmp(t.baslangic, beklenen_metin, t.uzunluk) == 0) : 1;
    if (!tip_ok) {
        printf("  FAIL %s: beklenen %s, gelen %s\n", ad, token_tipi_adi(beklenen), token_tipi_adi(t.tip));
    }
    test_sonuc(ad, tip_ok && metin_ok);
}

/* ===== Anahtar kelime testleri ===== */
static void test_anahtar_kelimeler(void) {
    printf("\n--- Anahtar Kelimeler ---\n");
    tek_token_test("e\xc4\x9f" "er", "e\xc4\x9f" "er", TOK_EGER, "e\xc4\x9f" "er");
    tek_token_test("de\xc4\x9f" "ilse", "de\xc4\x9f" "ilse", TOK_DEGILSE, "de\xc4\x9f" "ilse");
    tek_token_test("i\xc3\xa7in", "i\xc3\xa7in", TOK_ICIN, "i\xc3\xa7in");
    tek_token_test("iken", "iken", TOK_IKEN, "iken");
    tek_token_test("e\xc5\x9fle\xc5\x9f", "e\xc5\x9fle\xc5\x9f", TOK_ESLES, "e\xc5\x9fle\xc5\x9f");
    tek_token_test("ver", "ver", TOK_VER, "ver");
    tek_token_test("i\xc5\x9flev", "i\xc5\x9flev", TOK_ISLEV, "i\xc5\x9flev");
    tek_token_test("yap\xc4\xb1", "yap\xc4\xb1", TOK_YAPI, "yap\xc4\xb1");
    tek_token_test("\xc3\xb6zellik", "\xc3\xb6zellik", TOK_OZELLIK, "\xc3\xb6zellik");
    tek_token_test("mod\xc3\xbcl", "mod\xc3\xbcl", TOK_MODUL, "mod\xc3\xbcl");
    tek_token_test("de\xc4\x9f" "i\xc5\x9fken", "de\xc4\x9f" "i\xc5\x9fken", TOK_DEGISKEN, "de\xc4\x9f" "i\xc5\x9fken");
    tek_token_test("sabit", "sabit", TOK_SABIT, "sabit");
    tek_token_test("do\xc4\x9fru", "do\xc4\x9fru", TOK_DOGRU, "do\xc4\x9fru");
    tek_token_test("yanl\xc4\xb1\xc5\x9f", "yanl\xc4\xb1\xc5\x9f", TOK_YANLIS, "yanl\xc4\xb1\xc5\x9f");
    tek_token_test("bo\xc5\x9f", "bo\xc5\x9f", TOK_BOS, "bo\xc5\x9f");
    tek_token_test("ve", "ve", TOK_VE, "ve");
    tek_token_test("veya", "veya", TOK_VEYA, "veya");
    tek_token_test("de\xc4\x9f" "il", "de\xc4\x9f" "il", TOK_DEGIL, "de\xc4\x9f" "il");
    tek_token_test("kullan", "kullan", TOK_KULLAN, "kullan");
    tek_token_test("d\xc4\xb1\xc5\x9f" "a", "d\xc4\xb1\xc5\x9f" "a", TOK_DISA, "d\xc4\xb1\xc5\x9f" "a");
    tek_token_test("tamam", "tamam", TOK_TAMAM, "tamam");
    tek_token_test("hata", "hata", TOK_HATA, "hata");
    tek_token_test("b\xc3\xb6lge", "b\xc3\xb6lge", TOK_BOLGE, "b\xc3\xb6lge");
    tek_token_test("uygula", "uygula", TOK_UYGULA, "uygula");
    tek_token_test("kendin", "kendin", TOK_KENDIN, "kendin");
    tek_token_test("se\xc3\xa7imlik", "se\xc3\xa7imlik", TOK_SECIMLIK, "se\xc3\xa7imlik");
    tek_token_test("sonu\xc3\xa7", "sonu\xc3\xa7", TOK_SONUC, "sonu\xc3\xa7");
    tek_token_test("de\xc4\x9f" "er", "de\xc4\x9f" "er", TOK_DEGER, "de\xc4\x9f" "er");
    tek_token_test("hi\xc3\xa7", "hi\xc3\xa7", TOK_HIC, "hi\xc3\xa7");
    tek_token_test("g\xc3\xbcvensiz", "g\xc3\xbcvensiz", TOK_GUVENSIZ, "g\xc3\xbcvensiz");
}

/* ===== Tanımlayıcı testleri ===== */
static void test_tanimlayicilar(void) {
    printf("\n--- Tanimlayicilar ---\n");
    tek_token_test("basit ASCII", "abc", TOK_TANIMLAYICI, "abc");
    tek_token_test("alt cizgi", "_test", TOK_TANIMLAYICI, "_test");
    tek_token_test("rakamli", "x42", TOK_TANIMLAYICI, "x42");
    tek_token_test("Turkce: sayac", "saya\xc3\xa7", TOK_TANIMLAYICI, "saya\xc3\xa7");
    tek_token_test("Turkce: ogrenci", "\xc3\xb6\xc4\x9frenci", TOK_TANIMLAYICI, "\xc3\xb6\xc4\x9frenci");
    tek_token_test("Turkce: isci", "i\xc5\x9f\xc3\xa7i", TOK_TANIMLAYICI, "i\xc5\x9f\xc3\xa7i");
}

/* ===== Sayı testleri ===== */
static void test_sayilar(void) {
    printf("\n--- Sayilar ---\n");
    tek_token_test("tamsayi", "42", TOK_TAMSAYI, "42");
    tek_token_test("sifir", "0", TOK_TAMSAYI, "0");
    tek_token_test("buyuk sayi", "1000000", TOK_TAMSAYI, "1000000");
    tek_token_test("ayracli sayi", "1_000_000", TOK_TAMSAYI, "1_000_000");
    tek_token_test("hex", "0xFF", TOK_TAMSAYI, "0xFF");
    tek_token_test("binary", "0b1010", TOK_TAMSAYI, "0b1010");
    tek_token_test("octal", "0o777", TOK_TAMSAYI, "0o777");
    tek_token_test("ondalik", "3.14", TOK_ONDALIK, "3.14");
    tek_token_test("bilimsel", "1e10", TOK_ONDALIK, "1e10");
    tek_token_test("bilimsel negatif", "1.6e-19", TOK_ONDALIK, "1.6e-19");
}

/* ===== Metin testleri ===== */
static void test_metinler(void) {
    printf("\n--- Metinler ---\n");
    tek_token_test("basit metin", "\"merhaba\"", TOK_METIN, "\"merhaba\"");
    tek_token_test("bos metin", "\"\"", TOK_METIN, "\"\"");
    tek_token_test("kacisli metin", "\"a\\nb\"", TOK_METIN, "\"a\\nb\"");
    tek_token_test("Turkce metin", "\"\xc3\xb6\xc4\x9frenci\"", TOK_METIN, "\"\xc3\xb6\xc4\x9frenci\"");

    /* Kapatılmamış metin → hata */
    {
        Lexer l;
        lexer_baslat(&l, "\"kapatilmamis", "test");
        Token t = lexer_sonraki_token(&l);
        test_sonuc("kapatilmamis metin hata", t.tip == TOK_HATALI);
    }
}

/* ===== Raw metin testleri ===== */
static void test_ham_metin(void) {
    printf("\n--- Raw Metin ---\n");
    tek_token_test("raw basit", "r\"hello\"", TOK_HAM_METIN, "r\"hello\"");
    tek_token_test("raw hash", "r#\"he\"llo\"#", TOK_HAM_METIN, "r#\"he\"llo\"#");
    tek_token_test("raw cift hash", "r##\"a\"#b\"##", TOK_HAM_METIN, "r##\"a\"#b\"##");
}

/* ===== Karakter testleri ===== */
static void test_karakterler(void) {
    printf("\n--- Karakterler ---\n");
    tek_token_test("ASCII karakter", "'a'", TOK_KARAKTER, "'a'");
    tek_token_test("kacis karakter", "'\\n'", TOK_KARAKTER, "'\\n'");
    tek_token_test("Turkce karakter", "'\xc3\xb6'", TOK_KARAKTER, "'\xc3\xb6'");

    {
        Lexer l;
        lexer_baslat(&l, "''", "test");
        Token t = lexer_sonraki_token(&l);
        test_sonuc("bos karakter hata", t.tip == TOK_HATALI);
    }
}

/* ===== Operatör testleri ===== */
static void test_operatorler(void) {
    printf("\n--- Operatorler ---\n");
    tek_token_test("+",   "+",  TOK_ARTI,        "+");
    tek_token_test("-",   "-",  TOK_EKSI,        "-");
    tek_token_test("*",   "*",  TOK_YILDIZ,      "*");
    tek_token_test("/",   "/ ", TOK_BOLU,        "/");
    tek_token_test("%",   "%",  TOK_MOD,         "%");
    tek_token_test("==",  "==", TOK_ESIT_ESIT,   "==");
    tek_token_test("!=",  "!=", TOK_ESIT_DEGIL,  "!=");
    tek_token_test("<",   "< ", TOK_KUCUK,       "<");
    tek_token_test(">",   "> ", TOK_BUYUK,       ">");
    tek_token_test("<=",  "<=", TOK_KUCUK_ESIT,  "<=");
    tek_token_test(">=",  ">=", TOK_BUYUK_ESIT,  ">=");
    tek_token_test("=",   "= ", TOK_ESIT,        "=");
    tek_token_test("+=",  "+=", TOK_ARTI_ESIT,   "+=");
    tek_token_test("-=",  "-=", TOK_EKSI_ESIT,   "-=");
    tek_token_test("*=",  "*=", TOK_YILDIZ_ESIT, "*=");
    tek_token_test("/=",  "/=", TOK_BOLU_ESIT,   "/=");
    tek_token_test("%=",  "%=", TOK_MOD_ESIT,    "%=");
    tek_token_test("->",  "->", TOK_OK,          "->");
    tek_token_test("=>",  "=>", TOK_KALIN_OK,    "=>");
    tek_token_test("::",  "::", TOK_CIFT_IKI_NOKTA, "::");
    tek_token_test(".",   ". ", TOK_NOKTA,       ".");
    tek_token_test("..",  ".. ",TOK_ARALIK,      "..");
    tek_token_test("...", "...",TOK_UC_NOKTA,    "...");
    tek_token_test("<<",  "<<", TOK_SOLA_KAYDIR, "<<");
    tek_token_test(">>",  ">>", TOK_SAGA_KAYDIR, ">>");
    tek_token_test("&",   "&",  TOK_VE_BIT,      "&");
    tek_token_test("|",   "|",  TOK_VEYA_BIT,    "|");
    tek_token_test("^",   "^",  TOK_OZVEYA_BIT,  "^");
    tek_token_test("~",   "~",  TOK_DEGIL_BIT,   "~");
}

/* ===== Ayraç testleri ===== */
static void test_ayraclar(void) {
    printf("\n--- Ayraclar ---\n");
    tek_token_test("(",  "(",  TOK_SOL_PAREN,      "(");
    tek_token_test(")",  ")",  TOK_SAG_PAREN,      ")");
    tek_token_test("{",  "{",  TOK_SOL_SUSLU,      "{");
    tek_token_test("}",  "}",  TOK_SAG_SUSLU,      "}");
    tek_token_test("[",  "[",  TOK_SOL_KOSELI,     "[");
    tek_token_test("]",  "]",  TOK_SAG_KOSELI,     "]");
    tek_token_test(",",  ",",  TOK_VIRGUL,         ",");
    tek_token_test(":",  ": ", TOK_IKI_NOKTA,      ":");
    tek_token_test(";",  ";",  TOK_NOKTALI_VIRGUL, ";");
}

/* ===== Yorum testleri ===== */
static void test_yorumlar(void) {
    printf("\n--- Yorumlar ---\n");
    {
        Lexer l;
        lexer_baslat(&l, "// yorum\n42", "test");
        Token t = lexer_sonraki_token(&l);
        test_sonuc("satir yorumu atlanir", t.tip == TOK_TAMSAYI);
    }
    {
        Lexer l;
        lexer_baslat(&l, "/* yorum */ 42", "test");
        Token t = lexer_sonraki_token(&l);
        test_sonuc("blok yorumu atlanir", t.tip == TOK_TAMSAYI);
    }
    {
        Lexer l;
        lexer_baslat(&l, "/* /* ic */ dis */ 42", "test");
        Token t = lexer_sonraki_token(&l);
        test_sonuc("ic ice blok yorum", t.tip == TOK_TAMSAYI);
    }
}

/* ===== Tam program testi ===== */
static void test_tam_program(void) {
    printf("\n--- Tam Program ---\n");
    const char *program = "yap\xc4\xb1 Hasta {\n    ad: metin;\n    ya\xc5\x9f: tam32;\n}\n";
    Lexer l;
    lexer_baslat(&l, program, "test.kem");
    Token t;
    TokenTipi beklenen[] = {
        TOK_YAPI, TOK_TANIMLAYICI, TOK_SOL_SUSLU,
        TOK_TANIMLAYICI, TOK_IKI_NOKTA, TOK_TANIMLAYICI, TOK_NOKTALI_VIRGUL,
        TOK_TANIMLAYICI, TOK_IKI_NOKTA, TOK_TANIMLAYICI, TOK_NOKTALI_VIRGUL,
        TOK_SAG_SUSLU, TOK_DOSYA_SONU
    };
    int beklenen_sayisi = (int)(sizeof(beklenen) / sizeof(beklenen[0]));
    int hepsi_dogru = 1;
    for (int i = 0; i < beklenen_sayisi; i++) {
        t = lexer_sonraki_token(&l);
        if (t.tip != beklenen[i]) {
            printf("  FAIL tam program: token %d beklenen %s, gelen %s\n",
                   i, token_tipi_adi(beklenen[i]), token_tipi_adi(t.tip));
            hepsi_dogru = 0;
            break;
        }
    }
    if (hepsi_dogru) test_sonuc("yapi tanimi tokenizasyonu", 1);
    else { toplam_test++; basarisiz++; }
}

/* ===== Satır/sütun takibi ===== */
static void test_satir_takibi(void) {
    printf("\n--- Satir/Sutun Takibi ---\n");
    const char *kaynak = "x\ny\n  z";
    Lexer l;
    lexer_baslat(&l, kaynak, "test");
    Token t1 = lexer_sonraki_token(&l);
    test_sonuc("ilk token satir 1", t1.satir == 1 && t1.sutun == 1);
    Token t2 = lexer_sonraki_token(&l);
    test_sonuc("ikinci token satir 2", t2.satir == 2 && t2.sutun == 1);
    Token t3 = lexer_sonraki_token(&l);
    test_sonuc("ucuncu token satir 3 sutun 3", t3.satir == 3 && t3.sutun == 3);
}

int main(void) {
    printf("KEMGU Lexer Test Paketi\n");
    printf("========================\n");
    test_anahtar_kelimeler();
    test_tanimlayicilar();
    test_sayilar();
    test_metinler();
    test_ham_metin();
    test_karakterler();
    test_operatorler();
    test_ayraclar();
    test_yorumlar();
    test_tam_program();
    test_satir_takibi();
    printf("\n========================\n");
    printf("Toplam: %d | Basarili: %d | Basarisiz: %d\n", toplam_test, basarili, basarisiz);
    return basarisiz > 0 ? 1 : 0;
}
