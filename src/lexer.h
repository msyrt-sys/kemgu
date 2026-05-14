#ifndef KEMGU_LEXER_H
#define KEMGU_LEXER_H

typedef enum {
    /* ===== Anahtar kelimeler ===== */
    TOK_EGER,           /* eğer        */
    TOK_DEGILSE,        /* değilse     */
    TOK_ICIN,           /* için        */
    TOK_IKEN,           /* iken        */
    TOK_ESLES,          /* eşleş       */
    TOK_VER,            /* ver         */
    TOK_ISLEV,          /* işlev       */
    TOK_YAPI,           /* yapı        */
    TOK_OZELLIK,        /* özellik     */
    TOK_MODUL,          /* modül       */
    TOK_DEGISKEN,       /* değişken    */
    TOK_SABIT,          /* sabit       */
    TOK_DOGRU,          /* doğru       */
    TOK_YANLIS,         /* yanlış      */
    TOK_BOS,            /* boş         */
    TOK_VE,             /* ve          */
    TOK_VEYA,           /* veya        */
    TOK_DEGIL,          /* değil       */
    TOK_KULLAN,         /* kullan      */
    TOK_DISA,           /* dışa        */
    TOK_TAMAM,          /* tamam       */
    TOK_HATA,           /* hata        */
    TOK_BOLGE,          /* bölge       */
    TOK_UYGULA,         /* uygula      */
    TOK_KENDIN,         /* kendin      */
    TOK_SECIMLIK,       /* seçimlik    */
    TOK_SONUC,          /* sonuç       */
    TOK_DEGER,          /* değer       */
    TOK_HIC,            /* hiç         */
    TOK_GUVENSIZ,       /* güvensiz    */
    TOK_TEKKEZ,         /* tekkez      — Linear Types Spec V1 */
    TOK_IMHA,           /* imha        — Linear Types Spec V1 */
    TOK_OLARAK,         /* olarak      — Madde E: tip donusturme (x olarak T) */
    TOK_SABITSURE,      /* sabitsüre   — Sabitsüre Spec V1 (constant-time) */
    TOK_GERCEKZAMANLI,  /* gerçekzamanlı — Realtime Spec V1 (hard real-time) */
    TOK_YETKI,          /* yetki       — Capability Spec V1 (object-capability) */
    TOK_DELEGE,         /* delege      — Capability Spec V1 (sub-capability) */
    TOK_GERI_AL,        /* geri_al     — Capability Spec V1 (revoke) */

    /* ===== Literaller ===== */
    TOK_TAMSAYI,        /* 42, 0xFF, 0b1010 */
    TOK_ONDALIK,        /* 3.14, 1.6e-19    */
    TOK_METIN,          /* "merhaba"        */
    TOK_HAM_METIN,      /* r#"raw"#         */
    TOK_KARAKTER,       /* 'a'              */

    /* ===== Tanımlayıcı ===== */
    TOK_TANIMLAYICI,

    /* ===== Aritmetik ===== */
    TOK_ARTI,           /* +  */
    TOK_EKSI,           /* -  */
    TOK_YILDIZ,         /* *  */
    TOK_BOLU,           /* /  */
    TOK_MOD,            /* %  */

    /* ===== Karşılaştırma ===== */
    TOK_ESIT_ESIT,      /* == */
    TOK_ESIT_DEGIL,     /* != */
    TOK_KUCUK,          /* <  */
    TOK_BUYUK,          /* >  */
    TOK_KUCUK_ESIT,     /* <= */
    TOK_BUYUK_ESIT,     /* >= */

    /* ===== Atama ===== */
    TOK_ESIT,           /* =  */
    TOK_ARTI_ESIT,      /* += */
    TOK_EKSI_ESIT,      /* -= */
    TOK_YILDIZ_ESIT,    /* *= */
    TOK_BOLU_ESIT,      /* /= */
    TOK_MOD_ESIT,       /* %= */

    /* ===== Bit işlemleri ===== */
    TOK_VE_BIT,         /* &  */
    TOK_VEYA_BIT,       /* |  */
    TOK_OZVEYA_BIT,     /* ^  */
    TOK_DEGIL_BIT,      /* ~  */
    TOK_SOLA_KAYDIR,    /* << */
    TOK_SAGA_KAYDIR,    /* >> */

    /* ===== Diğer operatörler ===== */
    TOK_OK,             /* -> */
    TOK_KALIN_OK,       /* => */
    TOK_CIFT_IKI_NOKTA, /* :: */
    TOK_NOKTA,          /* .  */
    TOK_ARALIK,         /* .. */
    TOK_UC_NOKTA,       /* ... */

    /* ===== Ayraçlar ===== */
    TOK_SOL_PAREN,      /* (  */
    TOK_SAG_PAREN,      /* )  */
    TOK_SOL_SUSLU,      /* {  */
    TOK_SAG_SUSLU,      /* }  */
    TOK_SOL_KOSELI,     /* [  */
    TOK_SAG_KOSELI,     /* ]  */
    TOK_VIRGUL,         /* ,  */
    TOK_IKI_NOKTA,      /* :  */
    TOK_NOKTALI_VIRGUL, /* ;  */

    /* ===== Özel ===== */
    TOK_DOSYA_SONU,
    TOK_HATALI,
} TokenTipi;

typedef struct {
    TokenTipi tip;
    const char *baslangic;
    int uzunluk;
    int satir;
    int sutun;
} Token;

typedef struct {
    const char *kaynak;
    const char *simdiki;
    int satir;
    int sutun;
    const char *dosya_adi;
} Lexer;

void lexer_baslat(Lexer *lexer, const char *kaynak, const char *dosya_adi);
Token lexer_sonraki_token(Lexer *lexer);
const char *token_tipi_adi(TokenTipi tip);

#endif /* KEMGU_LEXER_H */
