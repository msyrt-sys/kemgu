#include "tip_kontrol.h"
#include "hata.h"
#include "lexer.h"
#include "parser.h"
#include "llvm.h"   /* C5 AS001: KEMGU_HEDEF_MIMARI (tek kaynak) */
#include "escape.h" /* G005: kacan-closure tespiti (forward DFA escape analizi) */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* === Setup === */

void tip_kontrol_baslat(TipKontrol *tk, Arena *a, Scope *global,
                        const char *dosya_adi, const char *kaynak) {
    tk->arena = a;
    tk->scope = global;
    tk->global_scope = global;
    tk->aktif_donus_tipi = NULL;
    uygula_tablosu_baslat(&tk->uygulamalar);
    tk->yuklenmisler = NULL;
    tk->hata_sayisi = 0;
    tk->guvensiz_baglam = 0;
    tk->ciplak_baglam = 0;   /* D-257 */

    /* A: built-in katmani ayristir — built-in'ler (ve dosya-modul kanonik
     * kayitlari) global'in PARENT'i olan ayri bir scope'ta yasar. Dosya-modul
     * scope'lari da builtin_scope'a baglanir: moduller built-in'leri gorur
     * ama giris dosyasinin ozel (top-level) adlarini GORMEZ. Yan etki:
     * kullanici built-in adi golgeleyebilir (onceden T024 cift-tanim idi). */
    tk->builtin_scope = NULL;
    if (global->parent == NULL) {
        Scope *b0 = scope_olustur(a, SCOPE_GLOBAL, NULL);
        if (b0) {
            global->parent = b0;
            tk->builtin_scope = b0;
        }
    }
    if (!tk->builtin_scope) tk->builtin_scope = global;

    /* Built-in islevler — LLVM'de libc karsiliklarina map edilir */
    #define EKLE_BUILTIN(_ad, _ad_uz, _params, _n_params, _donus) do { \
        Sembol _s; memset(&_s, 0, sizeof(_s)); \
        _s.ad = (_ad); _s.ad_uzunluk = (_ad_uz); \
        _s.kategori = SEMBOL_ISLEV; \
        _s.tip = tip_olustur_islev(a, (_params), (_n_params), (_donus)); \
        sembol_ekle(tk->builtin_scope, a, &_s); \
    } while (0)

    /* yazdir(metin) -> tam32  (libc puts) */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("yazdir", 6, p, 1, tip_olustur_basit(a, TIP_TAM32));
    }

    /* bellek_al(tam64) -> metin  (libc malloc — metin = ptr) */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_TAM64);
        EKLE_BUILTIN("bellek_al", 9, p, 1, tip_olustur_basit(a, TIP_METIN));
    }

    /* bellek_serbest(metin) -> bos  (libc free) */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("bellek_serbest", 14, p, 1, tip_olustur_basit(a, TIP_BOS));
    }

    /* bellek_kopyala(metin, metin, tam64) -> metin  (libc memcpy) */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *) * 3);
        p[0] = tip_olustur_basit(a, TIP_METIN);
        p[1] = tip_olustur_basit(a, TIP_METIN);
        p[2] = tip_olustur_basit(a, TIP_TAM64);
        EKLE_BUILTIN("bellek_kopyala", 14, p, 3, tip_olustur_basit(a, TIP_METIN));
    }

    /* === Madde A: Metin runtime primitifleri (kdl_metin_*) === */

    /* metin_uzunluk(metin) -> tam32 */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("metin_uzunluk", 13, p, 1, tip_olustur_basit(a, TIP_TAM32));
    }
    /* metin_bayt(metin, tam32) -> tam8  — i. HAM BAYT (UTF-8; ASCII'de =
     * karakter). Sınır dışı/NULL → 0. Tokenizer döngüsünün temel taşı:
     * metin_uzunluk ile birlikte bir metin üzerinde bayt-bayt gezinmeyi sağlar. */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *) * 2);
        p[0] = tip_olustur_basit(a, TIP_METIN);
        p[1] = tip_olustur_basit(a, TIP_TAM32);
        EKLE_BUILTIN("metin_bayt", 10, p, 2, tip_olustur_basit(a, TIP_TAM8));
    }
    /* metin_esit(metin, metin) -> mantıksal  — byte-byte eşitlik (strcmp==0).
     * Anahtar kelime/tanımlayıcı tanıma için (tokenizer). */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *) * 2);
        p[0] = tip_olustur_basit(a, TIP_METIN);
        p[1] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("metin_esit", 10, p, 2, tip_olustur_basit(a, TIP_MANTIKSAL));
    }
    /* metin_birlestir(metin, metin) -> metin */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *) * 2);
        p[0] = tip_olustur_basit(a, TIP_METIN);
        p[1] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("metin_birlestir", 15, p, 2, tip_olustur_basit(a, TIP_METIN));
    }
    /* metin_kes(metin, tam32, tam32) -> metin */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *) * 3);
        p[0] = tip_olustur_basit(a, TIP_METIN);
        p[1] = tip_olustur_basit(a, TIP_TAM32);
        p[2] = tip_olustur_basit(a, TIP_TAM32);
        EKLE_BUILTIN("metin_kes", 9, p, 3, tip_olustur_basit(a, TIP_METIN));
    }
    /* metin_kucuk(metin) -> metin */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("metin_kucuk", 11, p, 1, tip_olustur_basit(a, TIP_METIN));
    }
    /* metin_buyuk(metin) -> metin */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("metin_buyuk", 11, p, 1, tip_olustur_basit(a, TIP_METIN));
    }
    /* Adim 2: metin_kucuk_tr / metin_buyuk_tr / *_ascii varyantlari */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("metin_kucuk_tr", 14, p, 1, tip_olustur_basit(a, TIP_METIN));
    }
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("metin_buyuk_tr", 14, p, 1, tip_olustur_basit(a, TIP_METIN));
    }
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("metin_kucuk_ascii", 17, p, 1, tip_olustur_basit(a, TIP_METIN));
    }
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("metin_buyuk_ascii", 17, p, 1, tip_olustur_basit(a, TIP_METIN));
    }
    /* metin_icerir(metin, metin) -> mantiksal */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *) * 2);
        p[0] = tip_olustur_basit(a, TIP_METIN);
        p[1] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("metin_icerir", 12, p, 2,
                     tip_olustur_basit(a, TIP_MANTIKSAL));
    }
    /* metin_baslar(metin, metin) -> mantiksal */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *) * 2);
        p[0] = tip_olustur_basit(a, TIP_METIN);
        p[1] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("metin_baslar", 12, p, 2,
                     tip_olustur_basit(a, TIP_MANTIKSAL));
    }
    /* metin_biter(metin, metin) -> mantiksal */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *) * 2);
        p[0] = tip_olustur_basit(a, TIP_METIN);
        p[1] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("metin_biter", 11, p, 2,
                     tip_olustur_basit(a, TIP_MANTIKSAL));
    }
    /* metin_kirp(metin) -> metin */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("metin_kirp", 10, p, 1, tip_olustur_basit(a, TIP_METIN));
    }
    /* metin_yer_degistir(metin, metin, metin) -> metin */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *) * 3);
        p[0] = tip_olustur_basit(a, TIP_METIN);
        p[1] = tip_olustur_basit(a, TIP_METIN);
        p[2] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("metin_yer_degistir", 18, p, 3,
                     tip_olustur_basit(a, TIP_METIN));
    }

    /* === I/O built-in genisletme (src-bugfix — runtime/kdl_runtime.c) === */

    /* yazdir_tam(tam32) -> bos */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_TAM32);
        EKLE_BUILTIN("yazdir_tam", 10, p, 1, tip_olustur_basit(a, TIP_BOS));
    }
    /* yazdir_tam64(tam64) -> bos */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_TAM64);
        EKLE_BUILTIN("yazdir_tam64", 12, p, 1, tip_olustur_basit(a, TIP_BOS));
    }
    /* yazdir_satir() -> bos */
    {
        EKLE_BUILTIN("yazdir_satir", 12, NULL, 0,
                     tip_olustur_basit(a, TIP_BOS));
    }
    /* yaz_tam(tam32) -> bos */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_TAM32);
        EKLE_BUILTIN("yaz_tam", 7, p, 1, tip_olustur_basit(a, TIP_BOS));
    }
    /* yaz_tam64(tam64) -> bos */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_TAM64);
        EKLE_BUILTIN("yaz_tam64", 9, p, 1, tip_olustur_basit(a, TIP_BOS));
    }
    /* yaz_bayt(tam32) -> bos — HAM bayt (putchar, UTF-8 ENCODE ETMEZ).
     * Self-host parser ham UTF-8 dump için (yaz_karakter codepoint encode eder). */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_TAM32);
        EKLE_BUILTIN("yaz_bayt", 8, p, 1, tip_olustur_basit(a, TIP_BOS));
    }
    /* ondalik_bicimle(metin) -> metin — float lexeme'i strtod + %g ile biçimle
     * (self-host parser KESIRLI dump'ı C ast_duz_yaz ile birebir). */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("ondalik_bicimle", 15, p, 1, tip_olustur_basit(a, TIP_METIN));
    }
    /* yaz_metin built-in YOK — stdlib/dosya.kem 2-param yaz_metin
     * tanimlar; cakisma onlemek icin (KIRMIZI_QUEUE: dosya_yaz_metin
     * rename gelecek). */

    /* yazdir_metin(metin) -> bos
     * Bare-metal hedefte kdl_yazdir_metin (UART backend) cagrir;
     * host hedefte ayni isimli sembolu (kdl_runtime.c libc yolu) cagrir.
     * Eski "yazdir(metin) -> tam32 = puts" mappingi geriye uyum icin
     * korunuyor — yazdir_metin yeni isim. */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("yazdir_metin", 12, p, 1, tip_olustur_basit(a, TIP_BOS));
    }

    /* Track B C1: isaretsiz + onaltilik yazdirma (bare-metal hata ayiklama
     * + adres yazimi icin). Host runtime'da henuz implement edilmedi;
     * link asamasinda eksik sembol uyari verebilir — bare-metal akis icin
     * mevcut. */

    /* yazdir_isaretsiz_tam(dtam32) -> bos */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_DTAM32);
        EKLE_BUILTIN("yazdir_isaretsiz_tam", 20, p, 1,
                     tip_olustur_basit(a, TIP_BOS));
    }
    /* yazdir_isaretsiz_tam64(dtam64) -> bos */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_DTAM64);
        EKLE_BUILTIN("yazdir_isaretsiz_tam64", 22, p, 1,
                     tip_olustur_basit(a, TIP_BOS));
    }
    /* yazdir_onaltilik(dtam64) -> bos */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_DTAM64);
        EKLE_BUILTIN("yazdir_onaltilik", 16, p, 1,
                     tip_olustur_basit(a, TIP_BOS));
    }
    /* yaz_onaltilik(dtam64) -> bos */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_DTAM64);
        EKLE_BUILTIN("yaz_onaltilik", 13, p, 1,
                     tip_olustur_basit(a, TIP_BOS));
    }

    /* Track B D1/D2: karakter I/O (UTF-8 aware) — host runtime ve
     * bare-metal runtime ayni semboller (kdl_yazdir_karakter,
     * kdl_yaz_karakter, kdl_oku_karakter). */

    /* yazdir_karakter(karakter) -> bos */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_KARAKTER);
        EKLE_BUILTIN("yazdir_karakter", 15, p, 1,
                     tip_olustur_basit(a, TIP_BOS));
    }
    /* yaz_karakter(karakter) -> bos */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_KARAKTER);
        EKLE_BUILTIN("yaz_karakter", 12, p, 1,
                     tip_olustur_basit(a, TIP_BOS));
    }
    /* oku_karakter() -> karakter (kdl_oku_karakter — UART RX host/getchar) */
    {
        EKLE_BUILTIN("oku_karakter", 12, NULL, 0,
                     tip_olustur_basit(a, TIP_KARAKTER));
    }

    /* === G: Dosya syscall built-in'leri (runtime/kdl_runtime.c) === */

    /* dosya_ac(yol: metin, mod: metin) -> metin  (handle opaque ptr) */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *) * 2);
        p[0] = tip_olustur_basit(a, TIP_METIN);
        p[1] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("dosya_ac", 8, p, 2, tip_olustur_basit(a, TIP_METIN));
    }

    /* dosya_oku(yol: metin) -> metin  (tum dosya icerigini metin olarak) */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("dosya_oku", 9, p, 1, tip_olustur_basit(a, TIP_METIN));
    }

    /* dosya_yaz(handle: metin, icerik: metin) -> tam32  (yazilan byte) */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *) * 2);
        p[0] = tip_olustur_basit(a, TIP_METIN);
        p[1] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("dosya_yaz", 9, p, 2, tip_olustur_basit(a, TIP_TAM32));
    }

    /* dosya_kapat(handle: metin) -> boş */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("dosya_kapat", 11, p, 1, tip_olustur_basit(a, TIP_BOS));
    }

    /* dosya_var_mi(yol: metin) -> mantıksal */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("dosya_var_mi", 12, p, 1, tip_olustur_basit(a, TIP_MANTIKSAL));
    }

    /* dosya_sil(yol: metin) -> tam32 (0 basari, !=0 hata) */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("dosya_sil", 9, p, 1, tip_olustur_basit(a, TIP_TAM32));
    }

    /* dosya_yeniden_adlandir(eski: metin, yeni: metin) -> tam32 */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *) * 2);
        p[0] = tip_olustur_basit(a, TIP_METIN);
        p[1] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("dosya_yeniden_adlandir", 22, p, 2,
                     tip_olustur_basit(a, TIP_TAM32));
    }

    /* dosya_boyut(yol: metin) -> tam64 */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("dosya_boyut", 11, p, 1, tip_olustur_basit(a, TIP_TAM64));
    }

    /* === Adim 1 (OTP CLI): CLI args + OTP yardimcilari === */

    /* arg_sayi() -> tam32 */
    EKLE_BUILTIN("arg_sayi", 8, NULL, 0, tip_olustur_basit(a, TIP_TAM32));

    /* arg_al(i: tam32) -> metin */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *));
        p[0] = tip_olustur_basit(a, TIP_TAM32);
        EKLE_BUILTIN("arg_al", 6, p, 1, tip_olustur_basit(a, TIP_METIN));
    }

    /* otp_anahtar_uret(yol: metin, boyut: tam32) -> tam32 */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *) * 2);
        p[0] = tip_olustur_basit(a, TIP_METIN);
        p[1] = tip_olustur_basit(a, TIP_TAM32);
        EKLE_BUILTIN("otp_anahtar_uret", 16, p, 2,
                     tip_olustur_basit(a, TIP_TAM32));
    }

    /* otp_xor_uygula(msg, anahtar, cikti) -> tam32 */
    {
        TipBilgisi **p = (TipBilgisi **)arena_ayir(a, sizeof(TipBilgisi *) * 3);
        p[0] = tip_olustur_basit(a, TIP_METIN);
        p[1] = tip_olustur_basit(a, TIP_METIN);
        p[2] = tip_olustur_basit(a, TIP_METIN);
        EKLE_BUILTIN("otp_xor_uygula", 14, p, 3,
                     tip_olustur_basit(a, TIP_TAM32));
    }

    /* === Adim 6: Dizi capacity API === */
    /* dizi_kapasite(d: Dizi<T>) -> tam32 — generic, T atlanir */
    /* dizi_kapasite_ayarla(d: Dizi<T>, yeni: tam32) -> bos */
    /* Bu built-inler intrinsic gibi (tip kontrol DUGUM_CAGRI'de
     * ozel handler), sadece sembol tablosu lookup icin kayit */

    #undef EKLE_BUILTIN
    tk->dosya_adi = dosya_adi;
    tk->kaynak = kaynak;
    tk->scope_seviyesi = 0;
    tk->lambda_govdesi_icinde = 0;
    tk->imha_baglaminda = 0;
                               /* D-315: BASLATILMAZSA cop deger kontrolu SESSIZCE
                                * atlar (olculdu: kismi-tasinmis yapinin tasinmasi
                                * raporlanmiyordu). TipKontrol memset EDILMIYOR. */
    tk->lineer_sondaj = 0;     /* D-320 */
    tk->lambda_lineer_yakalama = 0;
    tk->lambda_yakalama = 0;
    tk->lambda_yakalama_isaretci = 0;   /* D-323 (TipKontrol memset EDILMIYOR) */
    tk->lambda_baslangic_scope = NULL;
    tk->lambda_blok_cikarsama = 0;      /* D-304 */
    tk->lambda_blok_donus = NULL;       /* D-304 */
    tk->aktif_escape = NULL;
}

/* === Linear Types Spec V1: yardimci fonksiyonlar === */

/* Eger ifade DUGUM_TANIMLAYICI ise ve sembolu lineer ise -> tuket.
 * Cift tuketim L002 hatasi (tekkez) veya CP005 (yetki). */
static void lineer_tuket_eger_baglamaysa(TipKontrol *tk, const Dugum *d) {
    if (!d || d->tip != DUGUM_TANIMLAYICI) return;
    /* D-320: sondaj (tip-ogrenme) ziyaretinde defter TUTULMAZ. Asil tuketim
     * pas 2'de sayilir; yoksa tek `kullan` iki kez sayilirdi (sahte L002). */
    if (tk->lineer_sondaj > 0) return;
    Sembol *s = sembol_bul_yazilabilir(tk->scope,
        d->veri.tanimlayici.metin, d->veri.tanimlayici.uzunluk);
    if (!s || !s->tip || !tip_lineer_mi(s->tip)) return;
    if (s->lineer_tuketildi >= 1) {
        if (s->tip->kategori == TIP_YETKI) {
            tip_hata(tk, d, "CP005",
                "yetki<R> iki kez tuketildi (move sonrasi erisim)");
        } else {
            tip_hata(tk, d, "L002",
                "lineer baglama iki kez tuketildi (move sonrasi erisim)");
        }
    }
    /* D-315 (V2.1): KISMI TASINMIS yapi BUTUN OLARAK TASINAMAZ. `imha` serbest
     * (kalan alanlari atar), ama cagri argumani / `ver` ile devretmek DELIKLI
     * bir degeri aliciya verirdi: alicinin tipi alani "var" gosterir, oysa
     * tasinmistir -> use-after-move. imha_baglaminda bayragi IMHA_IFADE kolunda
     * kurulur; digerlerinin hepsi tasimadir. */
    if (s->lineer_alan_maskesi != 0 && !tk->imha_baglaminda) {
        tip_hata(tk, d, "L002",
            "kismi tasinmis lineer yapi butun olarak tasinamaz "
            "(alani disari alinmis; yalniz imha edilebilir)");
    }
    s->lineer_tuketildi++;
}

/* ===================================================================
 * D-311 — L-COND: DAL-DUYARLI lineer tuketim (Linear Spec V1 §L-COND)
 * -------------------------------------------------------------------
 * Onceki model AKIS-DUYARSIZ bir SAYACTI: her `kullan/imha` sembolun
 * lineer_tuketildi'sini artiriyordu, dallardan bagimsiz. Bunun ikisi de
 * ampirik olculdu:
 *   (a) YANLIS RED (false positive): spec'in kanonik ornegi
 *       `eger p { kullan(t); } degilse { imha(t); }` L002 veriyordu —
 *       yani lineer bir kaynagi KOSULLU imha etmek IMKANSIZDI.
 *   (b) YANLIS KABUL (false negative): `eger p { kullan(t); }` (else yok)
 *       sessizce geciyordu — p yanlisken t hic tuketilmez (lineer sizinti),
 *       spec bunu L005 LINEAR_COND_INCONSISTENT olarak reddeder.
 * Cozum: dal girisinde tuketim durumu ANLIK-GORUNTULENIR, her dal kendi
 * kopyasi uzerinde calisir, cikista BIRLESTIRILIR:
 *   iki dal da tuketti  -> BIR tuketim (taban+1)
 *   tam olarak bir dal  -> L005 + tuketilmis say (ardil L001 kaskadi olmasin)
 *   hicbiri             -> taban
 * else'siz `eger` = else dali TUKETMEYEN dal olarak ele alinir.
 * =================================================================== */

/* Scope zincirindeki lineer sembolleri gez; her biri icin geri-cagirim. */
typedef struct {
    Sembol **semboller;
    int *taban;      /* dal oncesi lineer_tuketildi */
    /* D-315: kismi-tasima maskesi de dal-yerel olmali. Maskesiz snapshot,
     * bir dalda tasinan alani digerinde "tasinmis" gosterirdi (yanlis L002). */
    unsigned int *taban_maske;
    int sayi;
    int kapasite;
} LinAnlik;

static void lin_anlik_al(Scope *s, Arena *a, LinAnlik *anlik) {
    anlik->sayi = 0;
    anlik->kapasite = 0;
    anlik->semboller = NULL;
    anlik->taban = NULL;
    int n = 0;
    for (Scope *sc = s; sc; sc = sc->parent) {
        for (SembolLink *l = sc->bas; l; l = l->sonraki) {
            if (l->sembol.tip && tip_lineer_mi(l->sembol.tip)) n++;
        }
    }
    if (n == 0) return;
    anlik->semboller = (Sembol **)arena_ayir(a, sizeof(Sembol *) * (size_t)n);
    anlik->taban = (int *)arena_ayir(a, sizeof(int) * (size_t)n);
    anlik->taban_maske = (unsigned int *)arena_ayir(a,
                              sizeof(unsigned int) * (size_t)n);
    if (!anlik->semboller || !anlik->taban || !anlik->taban_maske) return;
    anlik->kapasite = n;
    for (Scope *sc = s; sc; sc = sc->parent) {
        for (SembolLink *l = sc->bas; l; l = l->sonraki) {
            if (l->sembol.tip && tip_lineer_mi(l->sembol.tip)
                && anlik->sayi < anlik->kapasite) {
                anlik->semboller[anlik->sayi] = &l->sembol;
                anlik->taban[anlik->sayi] = l->sembol.lineer_tuketildi;
                anlik->taban_maske[anlik->sayi] = l->sembol.lineer_alan_maskesi;
                anlik->sayi++;
            }
        }
    }
}

/* Anlik goruntudeki degerlere geri don (dal izolasyonu). */
static void lin_anlik_geri(const LinAnlik *anlik) {
    for (int i = 0; i < anlik->sayi; i++) {
        anlik->semboller[i]->lineer_tuketildi = anlik->taban[i];
        anlik->semboller[i]->lineer_alan_maskesi = anlik->taban_maske[i];
    }
}

/* Dal sonrasi durumu kaydet (dis diziye). */
static void lin_durum_yaz(const LinAnlik *anlik, int *hedef) {
    for (int i = 0; i < anlik->sayi; i++)
        hedef[i] = anlik->semboller[i]->lineer_tuketildi;
}

static void lineer_yakalama_kontrol(TipKontrol *tk, const Dugum *d);

/* D-315 (V2.1): alan sembolunun yapi icindeki 0-tabanli SIRASI (-1 yok). */
static int yapi_alan_indeksi_sirali(const Sembol *yapi_sem, const Sembol *alan) {
    if (!yapi_sem || !yapi_sem->yapi_scope || !alan) return -1;
    int ix = 0;
    for (SembolLink *l = yapi_sem->yapi_scope->bas; l; l = l->sonraki) {
        if (l->sembol.kategori != SEMBOL_DEGISKEN) continue;  /* generic param atla */
        if (&l->sembol == alan) return ix;
        ix++;
    }
    return -1;
}

/* D-315 (V2.1): erisim nesnesi BASIT bir baglama mi? (s.x -> s'nin sembolu).
 * Gecici deger / ic-ice erisim -> NULL (kismi tasima muhafazakar reddedilir). */
static Sembol *erisim_baglama_sembolu(TipKontrol *tk, const Dugum *nesne) {
    if (!nesne || nesne->tip != DUGUM_TANIMLAYICI) return NULL;
    return sembol_bul_yazilabilir(tk->scope, nesne->veri.tanimlayici.metin,
                                  nesne->veri.tanimlayici.uzunluk);
}

/* D-313 (Linear V2): yapi sembolunden TIP_YAPI kur ve `yapı tekkez K`
 * lineerligini tipe TASI. Bayrak tipte olmali cunku tip_lineer_mi (tip.c)
 * sembol tablosuna erisemez; onu tasimadan L001/L002/L-COND makinesi lineer
 * yapilari HIC gormezdi (sessiz kabul). */
static TipBilgisi *yapi_tipi_sembolden(TipKontrol *tk, const Sembol *s,
                                       TipBilgisi **args, int arg_sayi) {
    TipBilgisi *t = tip_olustur_yapi(tk->arena, s->ad, s->ad_uzunluk,
                                     args, arg_sayi);
    if (t && s->ast_dugumu && s->ast_dugumu->tip == DUGUM_YAPI) {
        t->veri.yapi.lineer_mi = s->ast_dugumu->veri.yapi.lineer_mi;
    }
    return t;
}

/* D-312 / L-LOOP: dongu govdesi cikisinda birlestir. Anlik goruntudeki bir
 * lineer baglama govdede tuketilmisse: dongu 0 kez donerse SIZINTI, >=2 kez
 * donerse CIFT TUKETIM -> ikisi de tek-kez disiplinini bozar. Tuketilmis
 * sayilir ki scope sonunda ayrica L001 patlamasin (tek kusur = tek hata). */
static void lineer_dongu_birlestir(TipKontrol *tk, const Dugum *d,
                                   const LinAnlik *anlik, const char *dongu_adi) {
    for (int i = 0; i < anlik->sayi; i++) {
        if (anlik->semboller[i]->lineer_tuketildi > anlik->taban[i]) {
            char msj[192];
            snprintf(msj, sizeof(msj),
                "`%s` govdesi disaridan gelen lineer baglamayi tuketiyor "
                "(dongu 0 kez donerse tuketilmez, >=2 kez donerse cift tuketim)",
                dongu_adi);
            tip_hata(tk, d, "L005", msj);
            anlik->semboller[i]->lineer_tuketildi = anlik->taban[i] + 1;
        }
    }
}

/* Lambda govdesi icinde: parent scope'taki lineer baglamayi yakala.
 * Sadece bayrak set eder — tuketim asil consume context'inde
 * (kullan/imha/cagri arg/ver) gerceklesir. Boylece kullan(k) gibi
 * ifadeler cift tuketim hatasi (L002) uretmez. */
static void lineer_yakalama_kontrol(TipKontrol *tk, const Dugum *d) {
    if (!d || d->tip != DUGUM_TANIMLAYICI) return;
    if (!tk->lambda_govdesi_icinde) return;
    if (!tk->lambda_baslangic_scope) return;

    /* Lambda kendi scope'unda mi? Eger oyle ise yakalama degil. */
    const Sembol *yerel_check = sembol_bul_yerel(tk->scope,
        d->veri.tanimlayici.metin, d->veri.tanimlayici.uzunluk);
    if (yerel_check) return;

    /* Parent scope'ta lineer baglama mi? (tekkez veya yetki) */
    const Sembol *parent = sembol_bul(tk->lambda_baslangic_scope,
        d->veri.tanimlayici.metin, d->veri.tanimlayici.uzunluk);
    if (!parent || !parent->tip || !tip_lineer_mi(parent->tip)) return;

    /* SADECE flag — closure-itself-linear (LC-2) tip isaretlemesi icin */
    tk->lambda_lineer_yakalama = 1;
}

/* G005 (V1 kacan-closure reddi) yardimci: lambda govdesindeki bir tanimlayici
 * KAPSAYAN bir fonksiyon/blok scope'undaki DEGISKEN/PARAMETRE'ye cozuluyorsa
 * = YAKALAMA. Lambda-ic (param/govde-yereli, golgeleme dahil) ve global/modul/
 * yapi (islev, sabit, tip) referanslari yakalama DEGIL. Codegen'in
 * lambda_serbest_tara'si ile birebir: yalniz cevre lokal/param capture edilir
 * (isim_bul g->isimler'de bulur), top-level isim/sabit capture edilmez.
 *
 * lambda_baslangic_scope = lambda sinirini isaretler: tk->scope'tan yukari
 * yururken bu scope'a ULASMADAN bulunan isim lambda-ictir (yakalama degil);
 * bu scope'tan ITIBAREN bulunan lokal/param yakalamadir. */
static int lambda_yakalama_yereli_mi(TipKontrol *tk, const char *ad, int uz) {
    int lambda_ici = 1;
    for (Scope *s = tk->scope; s; s = s->parent) {
        if (s == tk->lambda_baslangic_scope) lambda_ici = 0;
        const Sembol *sem = sembol_bul_yerel(s, ad, uz);
        if (sem) {
            if (lambda_ici) return 0;   /* lambda-ic (golgeleme dahil) */
            return (s->kategori == SCOPE_ISLEV || s->kategori == SCOPE_BLOK)
                   && (sem->kategori == SEMBOL_DEGISKEN
                       || sem->kategori == SEMBOL_PARAMETRE);
        }
    }
    return 0;
}

/* G005: lambda govdesi icinde HERHANGI bir cevre lokal/param yakalamasini
 * isaretle (lineer + lineer-olmayan). lineer_yakalama_kontrol'un yaninda cagrilir. */
/* D-323: yakalanan bir deger cerceve asiminda TEHLIKELI mi? Env HEAP oldugu icin
 * (llvm.c V2-F2: @malloc) skaler kopyasi guvenlidir; isaretci kopyasi ise gosterdigi
 * bolgeyi (ρ_yerel / cagiran cerceve) asabilir → dangling. Bilinmeyen/cozulemeyen
 * tip TEHLIKELI sayilir (default-deny: sessiz kabul yerine gurultulu red). */
static int yakalama_isaretci_benzeri(const TipBilgisi *t) {
    if (!t) return 1;
    switch (t->kategori) {
        case TIP_TAM8:  case TIP_TAM16: case TIP_TAM32: case TIP_TAM64:
        case TIP_DTAM8: case TIP_DTAM16: case TIP_DTAM32: case TIP_DTAM64:
        case TIP_KESIRLI32: case TIP_KESIRLI64:
        case TIP_MANTIKSAL: case TIP_KARAKTER: case TIP_BOS:
            return 0;   /* skaler: env'de DEGER kopyasi */
        case TIP_TEKKEZ:
            return yakalama_isaretci_benzeri(t->veri.tekkez.ic);
        default:
            return 1;   /* metin/Dizi/ref/ham-pointer/yapi + bilinmeyen → DENY */
    }
}

static void genel_yakalama_kontrol(TipKontrol *tk, const Dugum *d) {
    if (!d || d->tip != DUGUM_TANIMLAYICI) return;
    if (!tk->lambda_govdesi_icinde) return;
    if (!tk->lambda_baslangic_scope) return;
    if (lambda_yakalama_yereli_mi(tk, d->veri.tanimlayici.metin,
                                  d->veri.tanimlayici.uzunluk)) {
        tk->lambda_yakalama = 1;
        const Sembol *sem = sembol_bul(tk->scope, d->veri.tanimlayici.metin,
                                       d->veri.tanimlayici.uzunluk);
        /* Sembol/tip cozulemezse isaretci VARSAY (default-deny). */
        if (!sem || yakalama_isaretci_benzeri(sem->tip))
            tk->lambda_yakalama_isaretci = 1;
    }
}

/* Scope kapanisi: o scope'taki lineer baglamalar tuketilmis mi check.
 * Sadece SEMBOL_DEGISKEN/PARAMETRE icin (yapi alan'i degil). */
static void scope_lineer_kapanis_check(TipKontrol *tk, Scope *s) {
    if (!s) return;
    for (SembolLink *l = s->bas; l; l = l->sonraki) {
        Sembol *sem = &l->sembol;
        if (sem->kategori != SEMBOL_DEGISKEN &&
            sem->kategori != SEMBOL_PARAMETRE) continue;
        /* Linear takip: tekkez<T> ve yetki<R> ikisi de */
        if (!sem->tip || !tip_lineer_mi(sem->tip)) continue;
        if (sem->lineer_tuketildi == 0) {
            tk->hata_sayisi++;
            const char *kod = (sem->tip->kategori == TIP_YETKI)
                              ? "CP005" : "L001";
            const char *mesaj = (sem->tip->kategori == TIP_YETKI)
                ? "yetki<R> scope sonunda tuketilmedi"
                : "lineer baglama scope sonunda tuketilmedi";
            const char *ipucu = (sem->tip->kategori == TIP_YETKI)
                ? "geri_al(y) veya I/O cagrisi ile tuketin"
                : "kullan(...) veya imha(...) ile tuketin";
            hata_raporla(tk->dosya_adi, tk->kaynak,
                         sem->satir, sem->sutun, kod, mesaj, ipucu);
        }
    }
}

void tip_hata(TipKontrol *tk, const Dugum *d,
              const char *kod, const char *mesaj) {
    if (!tk || !d) return;
    tk->hata_sayisi++;
    hata_raporla(tk->dosya_adi, tk->kaynak,
                 d->satir, d->sutun, kod, mesaj, NULL);
}

/* === Yardimci: hata tipi === */

static TipBilgisi *t_hata(TipKontrol *tk) {
    return tip_olustur_basit(tk->arena, TIP_HATA);
}

/* D-086: dizi built-in argümanını çöz — `Dizi<T>` ya da `&Dizi<T>` /
 * `&değişken Dizi<T>` ise altındaki Dizi tipini döner (referansı soyar),
 * değilse NULL. Önceden tutarsızdı: `dizi_al(&Dizi)` sessiz kabul (t_hata),
 * `dizi_boyut(&Dizi)` T001 reddi. Artık tüm dizi built-in'leri referansı
 * otomatik soyar (çağıran &a verebilir; codegen D-086 ile deref eder). */
static TipBilgisi *dizi_arg_coz(TipBilgisi *t) {
    if (!t) return NULL;
    if (t->kategori == TIP_DIZI) return t;
    if (t->kategori == TIP_REFERANS && t->veri.referans.hedef &&
        t->veri.referans.hedef->kategori == TIP_DIZI) {
        return t->veri.referans.hedef;
    }
    return NULL;
}

/* === DZ003: N-bilinen baglamda BUYUTME yasak ===
 * `Dizi<T, N>` "tam olarak N" demektir; buyutmek bu sozu bozar. N yazilmamis
 * `Dizi<T>` eskisi gibi buyur — N yalniz ACIK annotasyondan bilindigi icin
 * (bkz. DUGUM_DIZI_OLUSTUR notu) hicbir mevcut kod kirilmaz. */
static void dz_buyutme_kontrol(TipKontrol *tk, const Dugum *d,
                               const TipBilgisi *dz, const char *fn_ad) {
    if (!dz || dz->kategori != TIP_DIZI) return;
    if (dz->veri.dizi.uzunluk <= 0) return;
    char msj[160];
    snprintf(msj, sizeof(msj),
        "%s ile buyutulemez: Dizi<T, %d> sabit uzunluktur "
        "(N'siz Dizi<T> kullanin)", fn_ad, dz->veri.dizi.uzunluk);
    tip_hata(tk, d, "DZ003", msj);
}

/* DZ Spec V1 Asama (b): buyutucu tablosu sorgusu (tanim tip_kontrol_program
 * yakininda — analiz orada kurulur). DUGUM_CAGRI'de DZ006 icin gerekir. */
static DzBuyutucu *dz_kayit_bul(TipKontrol *tk, const Dugum *islev);

/* === DZ004 / DZ005: DZ.3 akis kurali (YONLU) ===
 * N disa dogru SILINIR (Dizi<T,64> -> Dizi<T> serbest), ice dogru IDDIA
 * EDILEMEZ. Bu, `tip_esit` ile YAPILAMAZ: o simetriktir ve uzunlugu bilerek
 * yok sayar (silinmenin her yerde calismasi icin). */
static void dz_akis_kontrol(TipKontrol *tk, const Dugum *d,
                            const TipBilgisi *hedef,
                            const TipBilgisi *kaynak) {
    int r = tip_dizi_akis_uygun(hedef, kaynak);
    if (r == 0) return;
    char msj[192];
    if (r == 4) {
        snprintf(msj, sizeof(msj),
            "uzunlugu bilinmeyen Dizi<T>, Dizi<T, %d> hedefine verilemez "
            "(kaynagin tipi de N ile yazilmali)",
            hedef->veri.dizi.uzunluk);
        tip_hata(tk, d, "DZ004", msj);
    } else {
        snprintf(msj, sizeof(msj),
            "dizi uzunlugu uyusmuyor: Dizi<T, %d> verildi, Dizi<T, %d> bekleniyor",
            kaynak->veri.dizi.uzunluk, hedef->veri.dizi.uzunluk);
        tip_hata(tk, d, "DZ005", msj);
    }
}

/* MMIO Foundation: arg'in yetki<MMIO> (veya tekkez<yetki<MMIO>>) olup
 * olmadigini dogrular. y ODUNC alinir — TUKETILMEZ (geri_al ile tuketilir).
 * yetki<MMIO> degilse MM002 raporlanir. */
static void mmio_yetki_kontrol(TipKontrol *tk, const Dugum *arg) {
    TipBilgisi *y = tip_belirle(tk, arg);
    if (y->kategori == TIP_HATA) return;
    const TipBilgisi *k = tip_yetki_kaynak(y);
    int mmio = (tip_yetki_mi(y) && k && k->kategori == TIP_YAPI &&
                k->veri.yapi.ad && k->veri.yapi.ad_uzunluk == 4 &&
                memcmp(k->veri.yapi.ad, "MMIO", 4) == 0);
    if (!mmio) {
        tip_hata(tk, arg, "MM002",
                 "mmio islemi ilk argumani yetki<MMIO> olmali");
    }
}

/* v1 bölge-container: bölge_al ilk argümanı herhangi bir yetki<R>
 * ÖDÜNÇ alır (mmio deseni — TÜKETMEZ). v1'de R kısıtlanmaz; gerçek
 * arena (V2) AYNI imzayla R'yi kullanacak (evrim-koruyan). */
static void bolge_yetki_kontrol(TipKontrol *tk, const Dugum *arg) {
    TipBilgisi *y = tip_belirle(tk, arg);
    if (y->kategori == TIP_HATA) return;
    if (!tip_yetki_mi(y)) {
        tip_hata(tk, arg, "BL002",
                 "bolge_al ilk argumani yetki<R> olmali (odunc alinir)");
    }
}

/* Forward declaration (ADIM 11.6'da tanimli, kontrol_yapi_olustur_ic
 * tarafindan kullaniliyor — generic substitusyon icin) */
static TipBilgisi *substitusyon(TipKontrol *tk, const TipBilgisi *t,
                                 const Sembol *yapi_sem,
                                 const TipBilgisi *yapi_tipi);
static void tip_kontrol_deyim(TipKontrol *tk, const Dugum *d);  /* D-304 ileri bildirim */

/* === Madde D: Generic call inference helpers (paralel session, kullanilmaz) ===
 *
 * Bu bolumdeki GenericBaglama + linked list yardimcilari paralel commit
 * tarafindan eklendi. Bu oturumda alternatif GenBaglamalar (array) +
 * gen_bagla/gen_unify/gen_substitue daha asagida tanimli ve cagri site'da
 * o kullaniliyor. Asagidaki yardimcilar kullanilmiyor — tutuldu zira ileride
 * bound check icin gerekli olabilir (param_tip_generic_iceriyor_mu vb.). */

#if 0  /* kullanilmiyor — kullanilirsa #if 1 yap */

/* Generic param adi -> concrete tip baglamasi (linked list) */
typedef struct GenericBaglama {
    const char *ad;
    int ad_uzunluk;
    TipBilgisi *tip;
    struct GenericBaglama *sonraki;
} GenericBaglama;

static TipBilgisi *baglama_bul(GenericBaglama *b,
                                const char *ad, int ad_uz) {
    for (; b; b = b->sonraki) {
        if (b->ad_uzunluk == ad_uz &&
            memcmp(b->ad, ad, (size_t)ad_uz) == 0) {
            return b->tip;
        }
    }
    return NULL;
}

static void baglama_ekle(TipKontrol *tk, GenericBaglama **bas,
                          const char *ad, int ad_uz,
                          TipBilgisi *tip) {
    if (!tip || tip->kategori == TIP_GENERIC_PARAM) return;
    /* Varsa override etme — ilk binding kalir (zaten anchor) */
    if (baglama_bul(*bas, ad, ad_uz)) return;
    GenericBaglama *b = (GenericBaglama *)arena_ayir_sifir(tk->arena,
                                                            sizeof(GenericBaglama));
    if (!b) return;
    b->ad = ad;
    b->ad_uzunluk = ad_uz;
    b->tip = tip;
    b->sonraki = *bas;
    *bas = b;
}

static int param_tip_generic_iceriyor_mu(const TipBilgisi *t) {
    if (!t) return 0;
    if (t->kategori == TIP_GENERIC_PARAM) return 1;
    switch (t->kategori) {
        case TIP_REFERANS: return param_tip_generic_iceriyor_mu(t->veri.referans.hedef);
        case TIP_POINTER:  return param_tip_generic_iceriyor_mu(t->veri.pointer.hedef);
        case TIP_DIZI:     return param_tip_generic_iceriyor_mu(t->veri.dizi.eleman);
        case TIP_SECIMLIK: return param_tip_generic_iceriyor_mu(t->veri.secimlik.ic);
        case TIP_SONUC:
            return param_tip_generic_iceriyor_mu(t->veri.sonuc.deger) ||
                   param_tip_generic_iceriyor_mu(t->veri.sonuc.hata);
        case TIP_TEKKEZ:   return param_tip_generic_iceriyor_mu(t->veri.tekkez.ic);
        case TIP_VEKTOR:   return param_tip_generic_iceriyor_mu(t->veri.vektor.eleman);
        case TIP_ISLEV: {
            for (int i = 0; i < t->veri.islev.param_sayi; i++) {
                if (param_tip_generic_iceriyor_mu(t->veri.islev.parametreler[i]))
                    return 1;
            }
            return param_tip_generic_iceriyor_mu(t->veri.islev.donus);
        }
        case TIP_YAPI:
            for (int i = 0; i < t->veri.yapi.tip_arg_sayi; i++) {
                if (param_tip_generic_iceriyor_mu(t->veri.yapi.tip_arg[i]))
                    return 1;
            }
            return 0;
        default: return 0;
    }
}

/* tip_subst_baglamalar: t icindeki GENERIC_PARAM'lari baglamalardan al */
static TipBilgisi *tip_subst_baglamalar(TipKontrol *tk, TipBilgisi *t,
                                         GenericBaglama *b) {
    if (!t || !b) return t;
    switch (t->kategori) {
        case TIP_GENERIC_PARAM: {
            TipBilgisi *bound = baglama_bul(b,
                t->veri.generic_param.ad,
                t->veri.generic_param.ad_uzunluk);
            return bound ? bound : t;
        }
        case TIP_REFERANS: {
            TipBilgisi *nh = tip_subst_baglamalar(tk,
                t->veri.referans.hedef, b);
            if (nh == t->veri.referans.hedef) return t;
            return tip_olustur_referans(tk->arena, nh,
                t->veri.referans.degisken_mi);
        }
        case TIP_POINTER: {
            TipBilgisi *nh = tip_subst_baglamalar(tk,
                t->veri.pointer.hedef, b);
            if (nh == t->veri.pointer.hedef) return t;
            return tip_olustur_pointer(tk->arena, nh);
        }
        case TIP_DIZI: {
            TipBilgisi *ne = tip_subst_baglamalar(tk,
                t->veri.dizi.eleman, b);
            if (ne == t->veri.dizi.eleman) return t;
            return tip_olustur_dizi(tk->arena, ne);
        }
        case TIP_SECIMLIK: {
            TipBilgisi *ni = tip_subst_baglamalar(tk,
                t->veri.secimlik.ic, b);
            if (ni == t->veri.secimlik.ic) return t;
            return tip_olustur_secimlik(tk->arena, ni);
        }
        case TIP_SONUC: {
            TipBilgisi *nd = tip_subst_baglamalar(tk,
                t->veri.sonuc.deger, b);
            TipBilgisi *nh = tip_subst_baglamalar(tk,
                t->veri.sonuc.hata, b);
            if (nd == t->veri.sonuc.deger && nh == t->veri.sonuc.hata) return t;
            return tip_olustur_sonuc(tk->arena, nd, nh);
        }
        case TIP_TEKKEZ: {
            TipBilgisi *ni = tip_subst_baglamalar(tk,
                t->veri.tekkez.ic, b);
            if (ni == t->veri.tekkez.ic) return t;
            return tip_olustur_tekkez(tk->arena, ni);
        }
        case TIP_VEKTOR: {
            TipBilgisi *ne = tip_subst_baglamalar(tk,
                t->veri.vektor.eleman, b);
            if (ne == t->veri.vektor.eleman) return t;
            return tip_olustur_vektor(tk->arena, ne, t->veri.vektor.lane_sayi);
        }
        case TIP_ISLEV: {
            int n = t->veri.islev.param_sayi;
            int degisen = 0;
            TipBilgisi **np = NULL;
            if (n > 0) {
                np = (TipBilgisi **)arena_ayir(tk->arena,
                    sizeof(TipBilgisi *) * (size_t)n);
                for (int i = 0; i < n; i++) {
                    np[i] = tip_subst_baglamalar(tk,
                        t->veri.islev.parametreler[i], b);
                    if (np[i] != t->veri.islev.parametreler[i]) degisen = 1;
                }
            }
            TipBilgisi *nd = tip_subst_baglamalar(tk, t->veri.islev.donus, b);
            if (!degisen && nd == t->veri.islev.donus) return t;
            return tip_olustur_islev(tk->arena, np, n, nd);
        }
        default: return t;
    }
}

/* tip_unify: param ve arg'i paralel walk, generic baglamalari topla */
static void tip_unify(TipKontrol *tk,
                       TipBilgisi *param, TipBilgisi *arg,
                       GenericBaglama **bas) {
    if (!param || !arg) return;
    if (param->kategori == TIP_GENERIC_PARAM) {
        baglama_ekle(tk, bas,
            param->veri.generic_param.ad,
            param->veri.generic_param.ad_uzunluk, arg);
        return;
    }
    if (param->kategori != arg->kategori) return;
    switch (param->kategori) {
        case TIP_REFERANS:
            tip_unify(tk, param->veri.referans.hedef,
                          arg->veri.referans.hedef, bas);
            break;
        case TIP_POINTER:
            tip_unify(tk, param->veri.pointer.hedef,
                          arg->veri.pointer.hedef, bas);
            break;
        case TIP_DIZI:
            tip_unify(tk, param->veri.dizi.eleman,
                          arg->veri.dizi.eleman, bas);
            break;
        case TIP_SECIMLIK:
            tip_unify(tk, param->veri.secimlik.ic,
                          arg->veri.secimlik.ic, bas);
            break;
        case TIP_SONUC:
            tip_unify(tk, param->veri.sonuc.deger,
                          arg->veri.sonuc.deger, bas);
            tip_unify(tk, param->veri.sonuc.hata,
                          arg->veri.sonuc.hata, bas);
            break;
        case TIP_TEKKEZ:
            tip_unify(tk, param->veri.tekkez.ic,
                          arg->veri.tekkez.ic, bas);
            break;
        case TIP_VEKTOR:
            if (param->veri.vektor.lane_sayi != arg->veri.vektor.lane_sayi) break;
            tip_unify(tk, param->veri.vektor.eleman,
                          arg->veri.vektor.eleman, bas);
            break;
        case TIP_ISLEV: {
            if (param->veri.islev.param_sayi != arg->veri.islev.param_sayi) break;
            for (int i = 0; i < param->veri.islev.param_sayi; i++) {
                tip_unify(tk, param->veri.islev.parametreler[i],
                              arg->veri.islev.parametreler[i], bas);
            }
            tip_unify(tk, param->veri.islev.donus,
                          arg->veri.islev.donus, bas);
            break;
        }
        case TIP_YAPI: {
            if (param->veri.yapi.tip_arg_sayi != arg->veri.yapi.tip_arg_sayi) break;
            for (int i = 0; i < param->veri.yapi.tip_arg_sayi; i++) {
                tip_unify(tk, param->veri.yapi.tip_arg[i],
                              arg->veri.yapi.tip_arg[i], bas);
            }
            break;
        }
        default: break;
    }
}

#endif  /* kullanilmiyor — paralel session helperlari */

/* Forward (ADIM 15.5: bound check) */
static const char *tip_dugumu_kok_adi(const Dugum *t, int *out_uz);

static TipBilgisi *t_basit(TipKontrol *tk, TipKategorisi k) {
    return tip_olustur_basit(tk->arena, k);
}

/* === Madde D: Generic callback tip cikarsamasi (multi-param + compound) ===
 *
 * Bir cagri site'da `hedef<T,U,V>(...)` icin:
 *   1. param_tip <-> arg_tip unification ile her generic param T,U,V'yi
 *      argumanlarin somut tiplerinden cikar (compound tipler dahil:
 *      Dizi<T>, islev(T)->U, secimlik<T>, sonuc<T,E>, &T, *T)
 *   2. Donus tipi compound olabilir; her generic param tekrar substitue edilir
 *
 * Mevcut tek-T inference yerine name->concrete map kullanir. */

typedef struct GenBaglama {
    const char *ad;
    int ad_uz;
    const TipBilgisi *concrete;
} GenBaglama;

typedef struct GenBaglamalar {
    GenBaglama girisler[16];   /* En fazla 16 generic param — pratikte yeterli */
    int sayi;
} GenBaglamalar;

static void gen_bagla(GenBaglamalar *gb, const char *ad, int ad_uz,
                       const TipBilgisi *concrete) {
    if (!gb || !ad || gb->sayi >= 16) return;
    /* Var olan binding mi? — ilk gorulen kazanir */
    for (int i = 0; i < gb->sayi; i++) {
        if (gb->girisler[i].ad_uz == ad_uz &&
            memcmp(gb->girisler[i].ad, ad, (size_t)ad_uz) == 0) {
            return;  /* zaten bagli */
        }
    }
    gb->girisler[gb->sayi].ad = ad;
    gb->girisler[gb->sayi].ad_uz = ad_uz;
    gb->girisler[gb->sayi].concrete = concrete;
    gb->sayi++;
}

static const TipBilgisi *gen_bul(const GenBaglamalar *gb,
                                  const char *ad, int ad_uz) {
    if (!gb) return NULL;
    for (int i = 0; i < gb->sayi; i++) {
        if (gb->girisler[i].ad_uz == ad_uz &&
            memcmp(gb->girisler[i].ad, ad, (size_t)ad_uz) == 0) {
            return gb->girisler[i].concrete;
        }
    }
    return NULL;
}

/* param_tip ile arg_tip'i unify et — TIP_GENERIC_PARAM gorulen yerlere
 * arg_tip'ten karsilik binding ekle. Compound tiplere recursive. */
static void gen_unify(GenBaglamalar *gb, const TipBilgisi *param,
                       const TipBilgisi *arg) {
    if (!param || !arg) return;
    if (param->kategori == TIP_GENERIC_PARAM) {
        gen_bagla(gb, param->veri.generic_param.ad,
                  param->veri.generic_param.ad_uzunluk, arg);
        return;
    }
    /* Arg tarafi da generic param ise (govdede T->T gibi) — skip */
    if (arg->kategori == TIP_GENERIC_PARAM) return;
    /* Kategoriler farkliysa unify imkansiz — skip (hata zaten tip_esit'te) */
    if (param->kategori != arg->kategori) return;

    switch (param->kategori) {
        case TIP_REFERANS:
            gen_unify(gb, param->veri.referans.hedef, arg->veri.referans.hedef);
            break;
        case TIP_POINTER:
            gen_unify(gb, param->veri.pointer.hedef, arg->veri.pointer.hedef);
            break;
        case TIP_DIZI:
            gen_unify(gb, param->veri.dizi.eleman, arg->veri.dizi.eleman);
            break;
        case TIP_SECIMLIK:
            gen_unify(gb, param->veri.secimlik.ic, arg->veri.secimlik.ic);
            break;
        case TIP_SONUC:
            gen_unify(gb, param->veri.sonuc.deger, arg->veri.sonuc.deger);
            gen_unify(gb, param->veri.sonuc.hata,  arg->veri.sonuc.hata);
            break;
        case TIP_ISLEV: {
            int n = param->veri.islev.param_sayi;
            if (n == arg->veri.islev.param_sayi) {
                for (int i = 0; i < n; i++) {
                    gen_unify(gb,
                        param->veri.islev.parametreler[i],
                        arg->veri.islev.parametreler[i]);
                }
            }
            gen_unify(gb, param->veri.islev.donus, arg->veri.islev.donus);
            break;
        }
        default:
            break;
    }
}

/* Compound tip icinde TIP_GENERIC_PARAM'leri concrete tiplerle degistir.
 * gb NULL veya generic param eslemiyorsa orjinal tipi doner. */
static TipBilgisi *gen_substitue(TipKontrol *tk, const TipBilgisi *t,
                                   const GenBaglamalar *gb) {
    if (!t || !gb || gb->sayi == 0) return (TipBilgisi *)t;
    if (t->kategori == TIP_GENERIC_PARAM) {
        const TipBilgisi *c = gen_bul(gb,
            t->veri.generic_param.ad, t->veri.generic_param.ad_uzunluk);
        return c ? (TipBilgisi *)c : (TipBilgisi *)t;
    }
    switch (t->kategori) {
        case TIP_REFERANS: {
            TipBilgisi *nh = gen_substitue(tk, t->veri.referans.hedef, gb);
            if (nh == t->veri.referans.hedef) return (TipBilgisi *)t;
            return tip_olustur_referans(tk->arena, nh,
                                         t->veri.referans.degisken_mi);
        }
        case TIP_POINTER: {
            TipBilgisi *nh = gen_substitue(tk, t->veri.pointer.hedef, gb);
            if (nh == t->veri.pointer.hedef) return (TipBilgisi *)t;
            return tip_olustur_pointer(tk->arena, nh);
        }
        case TIP_DIZI: {
            TipBilgisi *ne = gen_substitue(tk, t->veri.dizi.eleman, gb);
            if (ne == t->veri.dizi.eleman) return (TipBilgisi *)t;
            return tip_olustur_dizi(tk->arena, ne);
        }
        case TIP_SECIMLIK: {
            TipBilgisi *ni = gen_substitue(tk, t->veri.secimlik.ic, gb);
            if (ni == t->veri.secimlik.ic) return (TipBilgisi *)t;
            return tip_olustur_secimlik(tk->arena, ni);
        }
        case TIP_SONUC: {
            TipBilgisi *nd = gen_substitue(tk, t->veri.sonuc.deger, gb);
            TipBilgisi *nh = gen_substitue(tk, t->veri.sonuc.hata, gb);
            if (nd == t->veri.sonuc.deger && nh == t->veri.sonuc.hata)
                return (TipBilgisi *)t;
            return tip_olustur_sonuc(tk->arena, nd, nh);
        }
        case TIP_ISLEV: {
            int n = t->veri.islev.param_sayi;
            TipBilgisi **yeni_p = NULL;
            int degisti = 0;
            if (n > 0) {
                yeni_p = (TipBilgisi **)arena_ayir(tk->arena,
                    sizeof(TipBilgisi *) * (size_t)n);
                for (int i = 0; i < n; i++) {
                    yeni_p[i] = gen_substitue(tk,
                        t->veri.islev.parametreler[i], gb);
                    if (yeni_p[i] != t->veri.islev.parametreler[i]) degisti = 1;
                }
            }
            TipBilgisi *nd = gen_substitue(tk, t->veri.islev.donus, gb);
            if (!degisti && nd == t->veri.islev.donus) return (TipBilgisi *)t;
            return tip_olustur_islev(tk->arena, yeni_p, n, nd);
        }
        default:
            return (TipBilgisi *)t;
    }
}

/* === Ad cevirici (built-in tip ad) === */

static int basit_tip_adindan(const char *ad, int uz, TipKategorisi *out) {
    /* Metin karsilastirma — basit lookup */
    struct { const char *ad; int uz; TipKategorisi k; } tbl[] = {
        {"tam8",      4, TIP_TAM8},
        {"tam16",     5, TIP_TAM16},
        {"tam32",     5, TIP_TAM32},
        {"tam64",     5, TIP_TAM64},
        {"dtam8",     5, TIP_DTAM8},
        {"dtam16",    6, TIP_DTAM16},
        {"dtam32",    6, TIP_DTAM32},
        {"dtam64",    6, TIP_DTAM64},
        {"kesirli32", 9, TIP_KESIRLI32},
        {"kesirli64", 9, TIP_KESIRLI64},
        {"karakter",  8, TIP_KARAKTER},
        {"metin",     5, TIP_METIN},
        /* mantiksal: m,a,n,t,ı,k,s,a,l = 1+1+1+1+2+1+1+1+1 = 10 byte */
        {"mant\xc4\xb1ksal", 10, TIP_MANTIKSAL},
        /* bos: b,o,ş = 1+1+2 = 4 byte */
        {"bo\xc5\x9f", 4, TIP_BOS},
        /* C2.7: ASCII birim-tip alias 'bos' (Türkçe DNA: ikisi de kabul) */
        {"bos", 3, TIP_BOS},
    };
    int n = (int)(sizeof(tbl) / sizeof(tbl[0]));
    for (int i = 0; i < n; i++) {
        if (tbl[i].uz == uz && memcmp(tbl[i].ad, ad, (size_t)uz) == 0) {
            *out = tbl[i].k;
            return 1;
        }
    }
    return 0;
}

/* === C2.7: çeşit (sum type) yardımcıları === */

/* 'ad' kayıtlı bir çeşit mi? DUGUM_CESIT döner, değilse NULL. */
static const Dugum *cesit_ara(TipKontrol *tk, const char *ad, int uz) {
    Scope *s0 = tk->scope ? tk->scope : tk->global_scope;
    const Sembol *s = sembol_bul(s0, ad, uz);
    if (s && s->kategori == SEMBOL_YAPI && s->ast_dugumu &&
        s->ast_dugumu->tip == DUGUM_CESIT) {
        return s->ast_dugumu;
    }
    return NULL;
}

/* T016 fix: bir yol ifadesini (TANIMLAYICI ya da YOL) isaret ettigi
 * modul_scope'a coz. mat -> mat'in scope'u; mat::ic -> ic'in scope'u
 * (recursive). Modul degilse NULL. Codegen'in @m1.m2 duzlestirmesiyle
 * ayni kapsam zinciri. */
/* A: gizli-farkindalikli ad arama — dosya-modul kanonik kayitlari
 * (gizli=1, builtin_scope'ta) normal cozumde GORUNMEZ; onlara yalniz
 * 'kullan' ile kurulan gorunur alias'lar uzerinden erisilir. Ayni
 * scope'ta es-adli ikinci sembol olamayacagi icin gizli eslesmede
 * parent'a gecmek dogru semantigi verir. */
static const Sembol *gorunur_sembol_bul(const Scope *s,
                                        const char *ad, int uz) {
    for (; s; s = s->parent) {
        const Sembol *sem = sembol_bul_yerel(s, ad, uz);
        if (sem && !sem->gizli) return sem;
    }
    return NULL;
}

static Scope *yol_modul_scope_coz(TipKontrol *tk, const Dugum *d) {
    if (!d) return NULL;
    if (d->tip == DUGUM_TANIMLAYICI) {
        const Sembol *m = gorunur_sembol_bul(tk->scope,
            d->veri.tanimlayici.metin, d->veri.tanimlayici.uzunluk);
        if (m && m->kategori == SEMBOL_MODUL && m->modul_scope) {
            return m->modul_scope;
        }
        return NULL;
    }
    if (d->tip == DUGUM_YOL) {
        Scope *parent = yol_modul_scope_coz(tk, d->veri.yol.sol);
        if (!parent) return NULL;
        const Sembol *m = sembol_bul_yerel(parent,
            d->veri.yol.sag_ad, d->veri.yol.sag_ad_uzunluk);
        if (m && m->kategori == SEMBOL_MODUL && m->modul_scope) {
            return m->modul_scope;
        }
        return NULL;
    }
    return NULL;
}

/* D dilim-1: yapı SEMBOL'ünü düz adla bul — önce görünür scope zinciri
 * (in-file / aynı modül), bulunamazsa YÜKLÜ tüm modüllerin scope'larında
 * düz adla ara. Çapraz-modül `geo2::Nokta` annotasyonlu bir değişkenin
 * alan erişimi (n.x) tip kontrolde yapının ALAN listesini ister; ad
 * codegen'in düz IR-ad uzayıyla tutarlıdır (D-011; per-modül ayrım D
 * ileri dilim). Görünür çözüm her zaman önce gelir (gölgeleme korunur). */
static const Sembol *yapi_sembol_capraz_bul(TipKontrol *tk,
                                            const char *ad, int uz) {
    const Sembol *s = sembol_bul(tk->scope, ad, uz);
    if (s && s->kategori == SEMBOL_YAPI) return s;
    if (tk->builtin_scope) {
        for (const SembolLink *l = tk->builtin_scope->bas; l; l = l->sonraki) {
            if (l->sembol.kategori == SEMBOL_MODUL && l->sembol.modul_scope) {
                const Sembol *ys = sembol_bul_yerel(l->sembol.modul_scope,
                                                    ad, uz);
                if (ys && ys->kategori == SEMBOL_YAPI) return ys;
            }
        }
    }
    return s;
}

/* === Tek-gecis ad cozumu: binding yazimi (bkz. ast.h CozumKategorisi) ===
 *
 * Resolver her ad-referansinda kazanan sembolu + kategorisini AST
 * dugumune yazar; codegen string'le yeniden cozmek yerine bunu tuketir.
 * Tek dogruluk kaynagi burasi. */

/* SCOPE_MODUL scope'unun modul sembolunu bul: modul sembolu parent
 * scope'a modul_scope=s ile kaydedilir (bkz. pre_populate_modul). */
static const Sembol *scope_modul_sembolu(const Scope *s) {
    if (!s || !s->parent) return NULL;
    for (const SembolLink *l = s->parent->bas; l; l = l->sonraki) {
        if (l->sembol.kategori == SEMBOL_MODUL &&
            l->sembol.modul_scope == s) {
            return &l->sembol;
        }
    }
    return NULL;
}

/* SCOPE_MODUL scope'unun noktali mangling onekini turet ("m1.m2") —
 * llvm.c modul_mangle ile ayni sema (KEMGU adlarinda '.' olamaz,
 * cakisma yok). Arena'da null-terminated string doner; turetilemezse
 * NULL (binding yazilmaz, codegen eski yola duser — guvenli taraf). */
#define COZUM_MODUL_DERINLIK_MAX 16
static const char *modul_onek_turet(TipKontrol *tk, const Scope *s,
                                    int *out_uz) {
    const Sembol *zincir[COZUM_MODUL_DERINLIK_MAX];
    int n = 0;
    const Scope *k = s;
    while (k && k->kategori == SCOPE_MODUL) {
        if (n >= COZUM_MODUL_DERINLIK_MAX) return NULL;
        const Sembol *ms = scope_modul_sembolu(k);
        if (!ms) return NULL;
        zincir[n++] = ms;
        k = k->parent;
    }
    if (n == 0) return NULL;
    int toplam = n - 1;  /* noktalar */
    for (int i = 0; i < n; i++) toplam += zincir[i]->ad_uzunluk;
    char *onek = (char *)arena_ayir(tk->arena, (size_t)toplam + 1);
    if (!onek) return NULL;
    int poz = 0;
    for (int i = n - 1; i >= 0; i--) {  /* distan ice: m1.m2 */
        memcpy(onek + poz, zincir[i]->ad, (size_t)zincir[i]->ad_uzunluk);
        poz += zincir[i]->ad_uzunluk;
        if (i > 0) onek[poz++] = '.';
    }
    onek[toplam] = '\0';
    if (out_uz) *out_uz = toplam;
    return onek;
}

/* Kazanan sembolu d dugumune bagla. AST dugumleri arena'da yazilabilir
 * nesnelerdir; resolver binding alanlarinin tek yazaridir — const'u
 * yalniz bu noktada kaldiririz. */
static void cozum_bagla(TipKontrol *tk, const Dugum *d,
                        const Sembol *sem, const Scope *bulundugu) {
    if (!d || !sem || !bulundugu) return;
    Dugum *yd = (Dugum *)d;
    /* A: secili-import alias'i — kazanan ASIL modulun uyesidir; binding
     * alias'in tasidigi onekle MODUL_UYESI olarak yazilir (codegen
     * @onek.ad emit eder). Bulundugu scope'un kategorisi onemsiz. */
    if (sem->ithal_onek) {
        yd->cozum_sembol = sem;
        yd->cozum_kategori = COZUM_MODUL_UYESI;
        yd->cozum_modul_onek = sem->ithal_onek;
        yd->cozum_modul_onek_uz = sem->ithal_onek_uz;
        return;
    }
    if (bulundugu->kategori == SCOPE_GLOBAL) {
        yd->cozum_sembol = sem;
        yd->cozum_kategori = COZUM_GLOBAL;
    } else if (bulundugu->kategori == SCOPE_MODUL) {
        int ouz = 0;
        const char *onek = modul_onek_turet(tk, bulundugu, &ouz);
        if (!onek) return;  /* onek yoksa binding yazma (COZUM_YOK kalir) */
        yd->cozum_sembol = sem;
        yd->cozum_kategori = COZUM_MODUL_UYESI;
        yd->cozum_modul_onek = onek;
        yd->cozum_modul_onek_uz = ouz;
    } else {
        yd->cozum_sembol = sem;
        yd->cozum_kategori = COZUM_YEREL;
    }
}

/* sembol_bul + binding: scope zincirini yuruyup kazanan sembolu VE
 * bulundugu scope'u tespit eder, d'ye baglar. sembol_bul ile ayni
 * arama sirasi (yerel -> iceren modul -> global). */
static const Sembol *sembol_coz_ve_bagla(TipKontrol *tk, const Dugum *d,
                                         const char *ad, int uz) {
    for (const Scope *s = tk->scope; s; s = s->parent) {
        const Sembol *sem = sembol_bul_yerel(s, ad, uz);
        if (!sem) continue;
        /* A: dosya-modul kanonik kaydi gorunmez — parent'a gec */
        if (sem->gizli) continue;
        /* A: ayni ad birden cok secili import'tan geldi — T042 */
        if (sem->ithal_cakisma) {
            tip_hata(tk, d, "T042",
                "belirsiz ad: birden cok secili import ayni adi getirdi "
                "(nitelikli erisim kullanin: modul::ad)");
        }
        cozum_bagla(tk, d, sem, s);
        return sem;
    }
    return NULL;
}

/* T016 fix: bir yol-sol'unu (TANIMLAYICI ya da modul-yolu) bir çeşit
 * tanimina coz. Renk -> dogrudan; g::Renk -> modul g icinde Renk.
 * Boylece modul-nitelikli varyant erisimi g::Renk::Kirmizi calisir. */
static const Dugum *yol_cesit_coz(TipKontrol *tk, const Dugum *d) {
    if (!d) return NULL;
    if (d->tip == DUGUM_TANIMLAYICI) {
        return cesit_ara(tk, d->veri.tanimlayici.metin,
                         d->veri.tanimlayici.uzunluk);
    }
    if (d->tip == DUGUM_YOL) {
        Scope *p = yol_modul_scope_coz(tk, d->veri.yol.sol);
        if (!p) return NULL;
        const Sembol *s = sembol_bul_yerel(p, d->veri.yol.sag_ad,
                                           d->veri.yol.sag_ad_uzunluk);
        if (s && s->kategori == SEMBOL_YAPI && s->ast_dugumu &&
            s->ast_dugumu->tip == DUGUM_CESIT) {
            return s->ast_dugumu;
        }
    }
    return NULL;
}

/* çeşit'te verilen varyant adı tanımlı mı? */
static int cesit_varyant_var(const Dugum *cd, const char *ad, int uz) {
    for (int i = 0; i < cd->veri.cesit.varyant_sayi; i++) {
        if (cd->veri.cesit.varyant_uzunluklar[i] == uz &&
            memcmp(cd->veri.cesit.varyantlar[i], ad, (size_t)uz) == 0) {
            return 1;
        }
    }
    return 0;
}

/* C3: varyant adının bildirim-sırası indeksi (-1 = yok). */
static int cesit_varyant_index(const Dugum *cd, const char *ad, int uz) {
    for (int i = 0; i < cd->veri.cesit.varyant_sayi; i++) {
        if (cd->veri.cesit.varyant_uzunluklar[i] == uz &&
            memcmp(cd->veri.cesit.varyantlar[i], ad, (size_t)uz) == 0) {
            return i;
        }
    }
    return -1;
}

/* C3: i. varyantın payload alan sayısı (0 = payloadsuz). */
static int cesit_varyant_payload_sayi(const Dugum *cd, int vi) {
    if (vi < 0 || vi >= cd->veri.cesit.varyant_sayi) return 0;
    if (!cd->veri.cesit.varyant_payload_sayilari) return 0;
    return cd->veri.cesit.varyant_payload_sayilari[vi];
}

/* === AST tip -> TipBilgisi cevirici === */

TipBilgisi *ast_tip_to_bilgi(TipKontrol *tk, const Dugum *tip_d) {
    if (!tip_d) return t_hata(tk);

    switch (tip_d->tip) {
        case DUGUM_TIP_BASIT: {
            const char *ad = tip_d->veri.tip_basit.ad;
            int uz = tip_d->veri.tip_basit.ad_uzunluk;
            TipKategorisi k;
            if (basit_tip_adindan(ad, uz, &k)) {
                return t_basit(tk, k);
            }
            /* Yapi/Generic param? sembol tablosunda ara */
            const Sembol *s = sembol_bul(tk->scope, ad, uz);
            if (s) {
                if (s->kategori == SEMBOL_YAPI) {
                    return yapi_tipi_sembolden(tk, s, NULL, 0);   /* D-313 */
                }
                if (s->kategori == SEMBOL_GENERIC_PARAM) {
                    return s->tip;  /* zaten TIP_GENERIC_PARAM */
                }
            }
            /* C3 çapraz-modül: modül-yerel yapı/çeşit (örn. recursive çeşit'in
             * payload tipi &Ifade, dışarıdan çözülürken) — yüklü modüllerde
             * düz adla ara (yapi_sembol_capraz_bul; alan-erişimiyle aynı). */
            {
                const Sembol *cs = yapi_sembol_capraz_bul(tk, ad, uz);
                if (cs && cs->kategori == SEMBOL_YAPI) {
                    return tip_olustur_yapi(tk->arena, cs->ad, cs->ad_uzunluk,
                                            NULL, 0);
                }
            }
            tip_hata(tk, tip_d, "T011", "bilinmeyen tip");
            return t_hata(tk);
        }

        case DUGUM_TIP_REFERANS: {
            TipBilgisi *hedef = ast_tip_to_bilgi(tk,
                tip_d->veri.tip_referans.hedef_tip);
            return tip_olustur_referans(tk->arena, hedef,
                tip_d->veri.tip_referans.degisken_mi);
        }

        case DUGUM_TIP_POINTER: {
            TipBilgisi *hedef = ast_tip_to_bilgi(tk,
                tip_d->veri.tip_pointer.hedef_tip);
            return tip_olustur_pointer(tk->arena, hedef);
        }

        case DUGUM_TIP_DIZI: {
            TipBilgisi *eleman = ast_tip_to_bilgi(tk,
                tip_d->veri.tip_dizi.eleman_tip);
            /* DZ: annotasyondaki N tip bilgisine tasinir (0 = bilinmiyor) */
            return tip_olustur_dizi_n(tk->arena, eleman,
                                      tip_d->veri.tip_dizi.uzunluk);
        }

        case DUGUM_TIP_SECIMLIK: {
            TipBilgisi *ic = ast_tip_to_bilgi(tk,
                tip_d->veri.tip_secimlik.ic_tip);
            return tip_olustur_secimlik(tk->arena, ic);
        }

        case DUGUM_TIP_TEKKEZ: {
            TipBilgisi *ic = ast_tip_to_bilgi(tk,
                tip_d->veri.tip_tekkez.ic_tip);
            /* Linear Types Spec V1: tekkez<tekkez<T>> destekli */
            return tip_olustur_tekkez(tk->arena, ic);
        }

        case DUGUM_TIP_SABITSURE: {
            TipBilgisi *ic = ast_tip_to_bilgi(tk,
                tip_d->veri.tip_sabitsure.ic_tip);
            if (ic->kategori == TIP_HATA) return t_hata(tk);
            /* CT006: sarılan tip sabitsüre-yetenekli olmalı */
            if (!tip_sabitsure_yetenekli_mi(ic)) {
                tip_hata(tk, tip_d, "CT006",
                    "sabitsure<...> sarilan tip constant-time yetenekli degil "
                    "(kesirli/metin/yapi/secimlik/nesting yasak)");
                return t_hata(tk);
            }
            return tip_olustur_sabitsure(tk->arena, ic);
        }

        case DUGUM_TIP_YETKI: {
            /* Capability Spec V1: yetki<R>, R = kaynak tipi.
             * V1: R DUGUM_TIP_BASIT veya DUGUM_TIP_KULLANICI olmalı,
             * adı bilinen kaynak setinden (Dosya/Soket/Bellek/Donanim/OTP_Anahtar).
             * Bilinmeyen kaynak: CP004 (CAPABILITY_TYPE_MISMATCH/INVALID). */
            const Dugum *r = tip_d->veri.tip_yetki.kaynak_tipi;
            if (!r) {
                tip_hata(tk, tip_d, "CP004",
                    "yetki<R> icin kaynak tipi gerekli");
                return t_hata(tk);
            }
            /* Kaynak adi bul */
            const char *ad = NULL;
            int ad_uz = 0;
            if (r->tip == DUGUM_TIP_BASIT) {
                ad = r->veri.tip_basit.ad;
                ad_uz = r->veri.tip_basit.ad_uzunluk;
            } else if (r->tip == DUGUM_TIP_KULLANICI &&
                       r->veri.tip_kullanici.yol &&
                       r->veri.tip_kullanici.yol->tip == DUGUM_TANIMLAYICI) {
                ad = r->veri.tip_kullanici.yol->veri.tanimlayici.metin;
                ad_uz = r->veri.tip_kullanici.yol->veri.tanimlayici.uzunluk;
            }
            int ok = 0;
            if (ad && ad_uz > 0) {
                /* OTP_Anahtar 11, Dosya 5, Soket 5, Bellek 6, Donanim 7, MMIO 4.
                 * MMIO Foundation (Karar 3): yetki<MMIO> tek top-level kaynak;
                 * cihaza-ozel (orn. MMIO_VirtIO) AYRI kaynak DEGIL — adres
                 * bolgesiyle daraltilmis MMIO formu olarak modellenir. */
                if ((ad_uz == 5 && memcmp(ad, "Dosya", 5) == 0) ||
                    (ad_uz == 5 && memcmp(ad, "Soket", 5) == 0) ||
                    (ad_uz == 6 && memcmp(ad, "Bellek", 6) == 0) ||
                    (ad_uz == 7 && memcmp(ad, "Donanim", 7) == 0) ||
                    (ad_uz == 4 && memcmp(ad, "MMIO", 4) == 0) ||
                    (ad_uz == 11 && memcmp(ad, "OTP_Anahtar", 11) == 0)) {
                    ok = 1;
                }
            }
            if (!ok) {
                tip_hata(tk, tip_d, "CP004",
                    "yetki<R>: bilinmeyen kaynak tipi "
                    "(Dosya/Soket/Bellek/Donanim/MMIO/OTP_Anahtar bekleniyor)");
                return t_hata(tk);
            }
            /* Kaynak TIP_YAPI olarak temsil edilir (ad bazli nominal eslesme).
             * ast_tip_to_bilgi'ye gitmeyiz cunku kaynak tipler symbol table'da
             * tanimli degil; built-in nominal isimler. */
            char *ad_kopya = (char *)arena_ayir(tk->arena, (size_t)ad_uz + 1);
            if (ad_kopya) {
                memcpy(ad_kopya, ad, (size_t)ad_uz);
                ad_kopya[ad_uz] = '\0';
            }
            TipBilgisi *kaynak = tip_olustur_yapi(tk->arena,
                                                 ad_kopya, ad_uz,
                                                 NULL, 0);
            return tip_olustur_yetki(tk->arena, kaynak);
        }

        case DUGUM_TIP_VEKTOR: {
            /* SIMD Spec V1: vektör<T, N>
             *   V001: T vektör-yetenekli olmalı (tam/dtam/kesirli/mantıksal)
             *   V002: N {2,4,8,16,32,64} setinde olmalı */
            TipBilgisi *eleman = ast_tip_to_bilgi(tk,
                tip_d->veri.tip_vektor.eleman_tip);
            int lane = tip_d->veri.tip_vektor.lane_sayi;
            if (eleman->kategori == TIP_HATA) return t_hata(tk);
            if (!tip_vektor_eleman_yetenekli_mi(eleman)) {
                tip_hata(tk, tip_d, "V001",
                    "vektor<T, N> tipinde T vektor-yetenekli skaler olmali "
                    "(tam/dtam/kesirli/mantiksal)");
                return t_hata(tk);
            }
            if (!tip_vektor_lane_gecerli_mi(lane)) {
                tip_hata(tk, tip_d, "V002",
                    "vektor<T, N> tipinde N {2,4,8,16,32,64} setinde olmali");
                return t_hata(tk);
            }
            return tip_olustur_vektor(tk->arena, eleman, lane);
        }

        case DUGUM_TIP_GOREV: {
            /* Concurrency / DRF V1: görev<T> — thread handle (linear) */
            TipBilgisi *ic = ast_tip_to_bilgi(tk,
                tip_d->veri.tip_gorev.ic_tip);
            if (ic->kategori == TIP_HATA) return t_hata(tk);
            /* D-294: annotasyon/parametre yolundaki AYNI kısıt (görev_başlat
             * yalnız YARATMA yolunu kapsar; `işlev t(g: görev<kesirli64>)`
             * gibi bildirimler buradan geçer). */
            if (ic->kategori == TIP_KESIRLI32 || ic->kategori == TIP_KESIRLI64) {
                tip_hata(tk, tip_d, "DRF001",
                    "gorev<T> V1'de kesirli T desteklemiyor (kesirli32/64) — "
                    "runtime sonucu tamsayi yazmacindan okur");
                return t_hata(tk);
            }
            return tip_olustur_gorev(tk->arena, ic);
        }

        case DUGUM_TIP_KANAL: {
            /* Concurrency / DRF V1: kanal<T> — channel endpoint */
            TipBilgisi *ic = ast_tip_to_bilgi(tk,
                tip_d->veri.tip_kanal.ic_tip);
            if (ic->kategori == TIP_HATA) return t_hata(tk);
            /* === V1 SINIRI (D-295, D-292'nin 32-bit kisitini DEGISTIRIR) ===
             * D-292'de kanal tamponu int32_t tasiyordu ve T 32-bit tamsayiyla
             * sinirliydi. O kisit `--check`i kapatiyordu ama `--llvm` TIP
             * KONTROLU CALISTIRMADIGI icin `kanal<tam64>` o yoldan derlenip
             * SESSIZCE veri kaybediyordu (olculdu: 2^33 gonder->al esit degil).
             * D-295'te kanal tamponu -- gorev ile simetrik olarak -- int64_t'ye
             * genisletildi: kirpma sinifi ortadan kalkti (gurultulu yapmak
             * yerine YOK edildi), tam64/metin/&T artik GERCEKTEN calisir.
             *
             * KALAN kisit KESIRLI T (gorev ile AYNI gerekce, DRF001): kanal
             * tamsayi tasir; kesirli deger i64'e sext/trunc ile degil ancak
             * fptosi ile girer -- yani DEGER bozulur (bit deseni degil). Bunu
             * bitcast'lamak yerine TIP seviyesinde reddediyoruz.
             * Bu tikac `kanal<T>` TIPININ cozuldugu tek nokta oldugu icin
             * parametre/degisken/donus fark etmeksizin her kullanimi kapsar. */
            if (ic->kategori == TIP_KESIRLI32 || ic->kategori == TIP_KESIRLI64) {
                tip_hata(tk, tip_d, "DRF006",
                    "kanal<T> V1'de kesirli T desteklemiyor (kesirli32/64) — "
                    "kanal tamsayi tasir, kesirli deger bozulur");
                return t_hata(tk);
            }
            return tip_olustur_kanal(tk->arena, ic);
        }

        case DUGUM_TIP_SONUC: {
            TipBilgisi *deger = ast_tip_to_bilgi(tk,
                tip_d->veri.tip_sonuc.deger_tip);
            TipBilgisi *hata = ast_tip_to_bilgi(tk,
                tip_d->veri.tip_sonuc.hata_tip);
            return tip_olustur_sonuc(tk->arena, deger, hata);
        }

        case DUGUM_TIP_ISLEV: {
            int n = tip_d->veri.tip_islev.param_sayi;
            TipBilgisi **params = NULL;
            if (n > 0) {
                params = (TipBilgisi **)arena_ayir(tk->arena,
                                                   sizeof(TipBilgisi *) * (size_t)n);
                if (params) {
                    for (int i = 0; i < n; i++) {
                        params[i] = ast_tip_to_bilgi(tk,
                            tip_d->veri.tip_islev.parametreler[i]);
                    }
                }
            }
            TipBilgisi *donus = ast_tip_to_bilgi(tk,
                tip_d->veri.tip_islev.donus_tip);
            return tip_olustur_islev(tk->arena, params, n, donus);
        }

        case DUGUM_TIP_KULLANICI: {
            /* Yol = DUGUM_TANIMLAYICI (niteliksiz: Liste<T>) veya
             * DUGUM_YOL (nitelikli: modül::Tip<T> — D dilim-1). Her iki
             * yolda da yapı SEMBOL_YAPI olarak çözülür; niteliksiz scope
             * zincirinde, nitelikli hedef modülün TİP namespace'inde. */
            const Dugum *yol = tip_d->veri.tip_kullanici.yol;
            /* D-303: gönderen<T>/alan<T> — kanal yön uçları. KEYWORD DEĞİL;
             * tip pozisyonunda generic-kullanıcı-tipi olarak parse edilir,
             * burada ADA göre özel-durum. Böylece `alan` (yapı alanı) serbest
             * tanımlayıcı olarak kalır — çakışma yok. */
            if (yol && yol->tip == DUGUM_TANIMLAYICI &&
                tip_d->veri.tip_kullanici.tip_arg_sayi == 1) {
                const char *tad = yol->veri.tanimlayici.metin;
                int tuz = yol->veri.tanimlayici.uzunluk;
                int kyon = -1;
                if (tuz == 9 && memcmp(tad, "g\xc3\xb6nderen", 9) == 0)
                    kyon = KANAL_YON_GONDEREN;
                else if (tuz == 4 && memcmp(tad, "alan", 4) == 0)
                    kyon = KANAL_YON_ALAN;
                if (kyon >= 0) {
                    TipBilgisi *kic = ast_tip_to_bilgi(tk,
                        tip_d->veri.tip_kullanici.tip_arg[0]);
                    if (kic->kategori == TIP_HATA) return t_hata(tk);
                    return tip_olustur_kanal_yon(tk->arena, kic, kyon);
                }
            }
            const Sembol *s = NULL;
            if (yol && yol->tip == DUGUM_TANIMLAYICI) {
                s = sembol_bul(tk->scope, yol->veri.tanimlayici.metin,
                               yol->veri.tanimlayici.uzunluk);
            } else if (yol && yol->tip == DUGUM_YOL) {
                /* Nitelikli tip: sol → hedef modül scope; sağ_ad → o
                 * scope'taki yapı (sembol_bul_yerel: parent'a bakmaz —
                 * value-path çözümüyle aynı kapsam zinciri). */
                Scope *msc = yol_modul_scope_coz(tk, yol->veri.yol.sol);
                /* Faz-1 (pre_populate: param/dönüş imzaları) `kullan`
                 * görünür-alias'ından ÖNCE çalışır → görünür `dizi` yok,
                 * yalnız GİZLİ kanonik dosya-modülü builtin_scope'ta var.
                 * Tip pozisyonu modülü açıkça adlandırdığı + modül zaten
                 * yüklü olduğu (bir yerde kullan var) için gizli kanoniğe
                 * düş (tek-segment; iç-içe yol görünür-alias faz-3'te). */
                if (!msc && yol->veri.yol.sol &&
                    yol->veri.yol.sol->tip == DUGUM_TANIMLAYICI) {
                    const Sembol *km = sembol_bul_yerel(tk->builtin_scope,
                        yol->veri.yol.sol->veri.tanimlayici.metin,
                        yol->veri.yol.sol->veri.tanimlayici.uzunluk);
                    if (km && km->kategori == SEMBOL_MODUL && km->modul_scope) {
                        msc = km->modul_scope;
                    }
                }
                if (msc) {
                    s = sembol_bul_yerel(msc, yol->veri.yol.sag_ad,
                                         yol->veri.yol.sag_ad_uzunluk);
                }
            }
            {
                int n = tip_d->veri.tip_kullanici.tip_arg_sayi;
                TipBilgisi **args = NULL;
                if (n > 0) {
                    args = (TipBilgisi **)arena_ayir(tk->arena,
                            sizeof(TipBilgisi *) * (size_t)n);
                    for (int i = 0; i < n; i++) {
                        args[i] = ast_tip_to_bilgi(tk,
                            tip_d->veri.tip_kullanici.tip_arg[i]);
                    }
                }
                if (s && s->kategori == SEMBOL_YAPI) {
                    /* Bound kontrolu: yapi tanimindaki her tip_param icin,
                     * arg o param'in bound'lari karsiliyor mu? */
                    const Dugum *yapi_d = s->ast_dugumu;
                    if (yapi_d && yapi_d->tip == DUGUM_YAPI &&
                        yapi_d->veri.yapi.tip_param_bound_sayilari) {
                        int param_n = yapi_d->veri.yapi.tip_param_sayi;
                        int eslesen = (param_n < n) ? param_n : n;
                        for (int pi = 0; pi < eslesen; pi++) {
                            int bs = yapi_d->veri.yapi.tip_param_bound_sayilari[pi];
                            if (bs == 0 || !args || !args[pi]) continue;
                            /* args[pi]'nin adini al */
                            const char *arg_ad = NULL;
                            int arg_uz = 0;
                            if (args[pi]->kategori == TIP_YAPI) {
                                arg_ad = args[pi]->veri.yapi.ad;
                                arg_uz = args[pi]->veri.yapi.ad_uzunluk;
                            } else if (args[pi]->kategori == TIP_GENERIC_PARAM) {
                                arg_ad = args[pi]->veri.generic_param.ad;
                                arg_uz = args[pi]->veri.generic_param.ad_uzunluk;
                            }
                            if (!arg_ad) continue;
                            /* Her bound icin tablo kontrolu */
                            for (int bi = 0; bi < bs; bi++) {
                                const Dugum *bd =
                                    yapi_d->veri.yapi.tip_param_boundlari[pi][bi];
                                int bd_uz = 0;
                                const char *bd_ad = tip_dugumu_kok_adi(bd, &bd_uz);
                                if (!bd_ad) continue;
                                /* Ozellik var mi? */
                                const Sembol *oz_s = sembol_bul(tk->global_scope,
                                                                bd_ad, bd_uz);
                                if (!oz_s || oz_s->kategori != SEMBOL_OZELLIK) {
                                    tip_hata(tk, tip_d, "T031",
                                        "bilinmeyen ozellik (bound olarak)");
                                    continue;
                                }
                                /* Generic param ise bound'u kendisi sahip oluyor
                                 * varsayilir (resolve sirasinda enclosing scope) */
                                if (args[pi]->kategori == TIP_GENERIC_PARAM) {
                                    continue;
                                }
                                if (!uygula_tablosu_implementations_eder(
                                        &tk->uygulamalar,
                                        arg_ad, arg_uz, bd_ad, bd_uz)) {
                                    tip_hata(tk, tip_d, "T030",
                                        "tip argumani bound karsilamiyor "
                                        "(uygula bildirimi yok)");
                                }
                            }
                        }
                    }
                    return yapi_tipi_sembolden(tk, s, args, n);   /* D-313 */
                }
                /* Niteliksiz çözülemedi → T011; nitelikli yol çözülemedi
                 * (modül yok / üye yapı değil) → T016. */
                if (yol && yol->tip == DUGUM_YOL) {
                    tip_hata(tk, tip_d, "T016", "nitelikli tip yolu "
                        "cozumlenemedi (modul/yapi bulunamadi)");
                } else {
                    tip_hata(tk, tip_d, "T011", "bilinmeyen kullanici tipi");
                }
                return t_hata(tk);
            }
        }

        default:
            tip_hata(tk, tip_d, "T011", "tip dugumu beklenirken farkli dugum");
            return t_hata(tk);
    }
}

/* === Tip belirle (ifade visitor) === */

/* === Sabitsüre Spec V1 yardımcıları === */

/* sabitsüre<T> sarmalayıcısını sok, T döner (kaynak sabitsüre değilse aynısı). */
static TipBilgisi *tip_ic_cek(TipBilgisi *t) {
    if (t && t->kategori == TIP_SABITSURE) return t->veri.sabitsure.ic;
    return t;
}

/* İkili op için: sol veya sağ sabitsüre ise sonuç da sabitsüre yapılır. */
static TipBilgisi *taint_yay(TipKontrol *tk, TipBilgisi *base,
                              TipBilgisi *sol, TipBilgisi *sag) {
    if (tip_sabitsure_mi(sol) || tip_sabitsure_mi(sag)) {
        if (tip_sabitsure_mi(base)) return base;
        return tip_olustur_sabitsure(tk->arena, base);
    }
    return base;
}

/* CT003 SABITSURE_LEAK helper:
 * 'kaynak' (genelde değer ifadesinin tipi) sabitsüre<T> ve 'beklenen' normal T
 * ise leak hatası. d düğümü ifade için. Hata raporlanırsa 1 döner. */
static int ct003_leak_kontrol(TipKontrol *tk, const Dugum *d,
                               const TipBilgisi *kaynak,
                               const TipBilgisi *beklenen) {
    if (!kaynak || !beklenen) return 0;
    if (tip_sabitsure_mi(kaynak) && !tip_sabitsure_mi(beklenen)) {
        tip_hata(tk, d, "CT003",
            "sabitsure tipi normal tipe implicit donusturulemez "
            "(ifsa(...) zorunlu)");
        return 1;
    }
    return 0;
}

/* Yardimci: ikili sayisal op (sol == sag, ikisi de sayisal) */
/* SIMD Spec V1: ikili operatör vektör üzerinde mi?
 * Sol veya sağ vektör ise vektör semantikleriyle çöz. */
static TipBilgisi *kontrol_ikili_vektor(TipKontrol *tk, const Dugum *d,
                                         TipBilgisi *sol, TipBilgisi *sag) {
    int sol_v = tip_vektor_mu(sol);
    int sag_v = tip_vektor_mu(sag);
    /* V004: skaler + vektör (V1'de yasak — explicit vektör_doldur gerek) */
    if (sol_v != sag_v) {
        tip_hata(tk, d, "V004",
            "vektor ile skaler karma operasyon V1'de yasak — "
            "vektor_doldur(s) ile explicit broadcast kullan");
        return t_hata(tk);
    }
    /* V003: lane sayıları eşit olmalı */
    if (sol->veri.vektor.lane_sayi != sag->veri.vektor.lane_sayi) {
        tip_hata(tk, d, "V003",
            "vektor operandlarinin lane sayilari (N) esit olmali");
        return t_hata(tk);
    }
    /* V003: element tipleri eşit olmalı */
    if (!tip_esit(sol->veri.vektor.eleman, sag->veri.vektor.eleman)) {
        tip_hata(tk, d, "V003",
            "vektor operandlarinin element tipleri (T) esit olmali");
        return t_hata(tk);
    }
    /* V005: kesirli vektörde % yasak */
    Operator op = d->veri.ikili.op;
    int eleman_kesirli =
        sol->veri.vektor.eleman->kategori == TIP_KESIRLI32 ||
        sol->veri.vektor.eleman->kategori == TIP_KESIRLI64;
    if (op == OP_MOD && eleman_kesirli) {
        tip_hata(tk, d, "V005",
            "kesirli vektor uzerinde % yasak (FP modulo undefined)");
        return t_hata(tk);
    }
    /* Eşit lane + eşit element → aynı tip dönsün */
    return sol;
}

static TipBilgisi *kontrol_ikili_sayisal(TipKontrol *tk, const Dugum *d,
                                         TipBilgisi *sol, TipBilgisi *sag) {
    /* SIMD Spec V1: önce vektör hattı */
    if (tip_vektor_mu(sol) || tip_vektor_mu(sag)) {
        return kontrol_ikili_vektor(tk, d, sol, sag);
    }
    if (!tip_sayisal_mi(sol)) {
        tip_hata(tk, d, "T003", "ikili operatorun sol tarafi sayisal degil");
        return t_hata(tk);
    }
    if (!tip_sayisal_mi(sag)) {
        tip_hata(tk, d, "T003", "ikili operatorun sag tarafi sayisal degil");
        return t_hata(tk);
    }
    /* Sabitsüre Spec V1 CT004: sabitsüre üzerinde / veya % YASAK
     * (x86 idiv/div, ARM udiv/sdiv variable-time). */
    Operator op = d->veri.ikili.op;
    if ((op == OP_BOLU || op == OP_MOD) &&
        (tip_sabitsure_mi(sol) || tip_sabitsure_mi(sag))) {
        tip_hata(tk, d, "CT004",
            "sabitsure tipi uzerinde / veya % yasak (variable-time div)");
        return t_hata(tk);
    }
    /* İç tipler eşit mi? sabitsüre<T> + T → her ikisinin iç T'si aynı olmalı.
     * Bu sayede sabitsüre<tam32> + tam32 → sabitsüre<tam32> (taint yayılım). */
    TipBilgisi *sol_ic = tip_ic_cek(sol);
    TipBilgisi *sag_ic = tip_ic_cek(sag);
    if (!tip_esit(sol_ic, sag_ic)) {
        tip_hata(tk, d, "T001", "ikili operator iki tarafi ayni tip olmali");
        return t_hata(tk);
    }
    return taint_yay(tk, sol_ic, sol, sag);
}

/* Yardimci: ikili mantiksal (sol+sag mantiksal) */
static TipBilgisi *kontrol_ikili_mantiksal(TipKontrol *tk, const Dugum *d,
                                           TipBilgisi *sol, TipBilgisi *sag) {
    if (!tip_mantiksal_mi(sol) || !tip_mantiksal_mi(sag)) {
        tip_hata(tk, d, "T004", "mantiksal op iki tarafi mantiksal olmali");
        return t_hata(tk);
    }
    TipBilgisi *base = t_basit(tk, TIP_MANTIKSAL);
    return taint_yay(tk, base, sol, sag);
}

/* Yardimci: yapi olusturma alan kontrolu (beklenen tip varsa generic
 * substitusyon yapilir — Kutu<tam32> { eleman: 5 } gibi durumlarda) */
static TipBilgisi *kontrol_yapi_olustur_ic(TipKontrol *tk, const Dugum *d,
                                            const TipBilgisi *beklenen) {
    const char *tip_ad = d->veri.yapi_olustur.tip_ad;
    int tip_ad_uz = d->veri.yapi_olustur.tip_ad_uzunluk;
    const Sembol *yapi_sem = sembol_bul(tk->scope, tip_ad, tip_ad_uz);
    if (!yapi_sem || yapi_sem->kategori != SEMBOL_YAPI) {
        tip_hata(tk, d, "T002", "yapi tipi tanimsiz");
        return t_hata(tk);
    }
    /* Generic substitusyon kaynagi */
    int generic_var = (beklenen && beklenen->kategori == TIP_YAPI &&
                       beklenen->veri.yapi.tip_arg_sayi > 0);

    /* Her alan_atama icin ad eşlesimi + tip eşlesimi */
    int hata = 0;
    for (int i = 0; i < d->veri.yapi_olustur.alan_sayi; i++) {
        const Dugum *aa = d->veri.yapi_olustur.alanlar[i];
        const char *ad = aa->veri.alan_atama.ad;
        int uz = aa->veri.alan_atama.ad_uzunluk;
        const Sembol *alan = sembol_yapi_alani(yapi_sem, ad, uz);
        if (!alan) {
            tip_hata(tk, aa, "T017", "yapida bilinmeyen alan");
            hata = 1;
            continue;
        }
        /* Generic substitusyon (beklenen Kutu<tam32> ise alan T -> tam32) */
        TipBilgisi *alan_tipi = generic_var
            ? substitusyon(tk, alan->tip, yapi_sem, beklenen)
            : alan->tip;
        /* Bidirectional: alan degeri alan tipi context'inde */
        TipBilgisi *deger_tip = tip_belirle_beklenen(tk,
            aa->veri.alan_atama.deger, alan_tipi);
        if (!tip_esit(alan_tipi, deger_tip) &&
            deger_tip->kategori != TIP_HATA) {
            tip_hata(tk, aa, "T001", "alan tipi uyumsuz");
            hata = 1;
        }
        /* Linear Types Spec V1: alan tekkez ise deger baglamadan move */
        if (alan_tipi && alan_tipi->kategori == TIP_TEKKEZ) {
            lineer_tuket_eger_baglamaysa(tk, aa->veri.alan_atama.deger);
        }
    }
    /* Eksik alan kontrolu (yapi alanlarinin hepsi var mi?) */
    if (yapi_sem->yapi_scope) {
        for (SembolLink *l = yapi_sem->yapi_scope->bas; l; l = l->sonraki) {
            if (l->sembol.kategori != SEMBOL_DEGISKEN) continue;
            int bulundu = 0;
            for (int i = 0; i < d->veri.yapi_olustur.alan_sayi; i++) {
                const Dugum *aa = d->veri.yapi_olustur.alanlar[i];
                if (aa->veri.alan_atama.ad_uzunluk == l->sembol.ad_uzunluk &&
                    memcmp(aa->veri.alan_atama.ad, l->sembol.ad,
                           (size_t)l->sembol.ad_uzunluk) == 0) {
                    bulundu = 1;
                    break;
                }
            }
            if (!bulundu) {
                tip_hata(tk, d, "T012", "yapi olusturmada eksik alan");
                hata = 1;
            }
        }
    }
    (void)hata;  /* hata zaten tk->hata_sayisi'na sayildi */
    /* Beklenen tipte tip_arg varsa concrete instance'i don */
    if (generic_var) {
        return yapi_tipi_sembolden(tk, yapi_sem,          /* D-313 */
                                   beklenen->veri.yapi.tip_arg,
                                   beklenen->veri.yapi.tip_arg_sayi);
    }
    return yapi_tipi_sembolden(tk, yapi_sem, NULL, 0);   /* D-313 */
}

/* Geriye uyumlu wrapper (mevcut tip_belirle cagrilari icin) */
static TipBilgisi *kontrol_yapi_olustur(TipKontrol *tk, const Dugum *d) {
    return kontrol_yapi_olustur_ic(tk, d, NULL);
}

/* C3: d bir çeşit varyant YAPICISI mı (Cesit::V(args))? Öyleyse arg sayısını
 * + tiplerini varyant payload'una göre kontrol et ve TIP_YAPI(cesit) dön.
 * Değilse NULL (çağıran normal çağrı/modül-fonksiyon yoluna düşer). */
static TipBilgisi *cesit_yapici_tip_kontrol(TipKontrol *tk, const Dugum *d,
                                            const TipBilgisi *beklenen) {
    if (!d || d->tip != DUGUM_CAGRI || !d->veri.cagri.hedef ||
        d->veri.cagri.hedef->tip != DUGUM_YOL) {
        return NULL;
    }
    const Dugum *yol = d->veri.cagri.hedef;
    const Dugum *cd = yol_cesit_coz(tk, yol->veri.yol.sol);
    if (!cd) return NULL;  /* sol çeşit değil → modül fonksiyon çağrısı */
    int vi = cesit_varyant_index(cd, yol->veri.yol.sag_ad,
                                 yol->veri.yol.sag_ad_uzunluk);
    if (vi < 0) {
        tip_hata(tk, d, "M002", "cesit varyanti bulunamadi");
        return t_hata(tk);
    }
    /* Generic çeşit (D-302): payload tipindeki T'yi çeşit generic scope'unda
     * çöz (→ TIP_GENERIC_PARAM) sonra beklenen `Secim<tam32>`'in tip_arg'ından
     * substitue et. beklenen yoksa T çözülemez (T011) — construction daima bir
     * beklenen bağlamı ister (annotasyon/ver/parametre). */
    int generic = cd->veri.cesit.tip_param_sayi > 0;
    const Sembol *csem = generic
        ? sembol_bul(tk->scope ? tk->scope : tk->global_scope,
                     cd->veri.cesit.ad, cd->veri.cesit.ad_uzunluk)
        : NULL;
    int beklenen_n = cesit_varyant_payload_sayi(cd, vi);
    int verilen_n = d->veri.cagri.sayi;
    if (verilen_n != beklenen_n) {
        tip_hata(tk, d, "M003",
                 "cesit varyant payload alan sayisi uyumsuz");
        return tip_olustur_yapi(tk->arena, cd->veri.cesit.ad,
                                cd->veri.cesit.ad_uzunluk, NULL, 0);
    }
    for (int i = 0; i < beklenen_n; i++) {
        const Dugum *pt = cd->veri.cesit.varyant_payload_tipleri[vi][i];
        TipBilgisi *bt;
        if (generic && csem && csem->yapi_scope) {
            Scope *eski = tk->scope;
            tk->scope = csem->yapi_scope;      /* T → TIP_GENERIC_PARAM */
            TipBilgisi *raw = ast_tip_to_bilgi(tk, pt);
            tk->scope = eski;
            bt = substitusyon(tk, raw, csem, beklenen);  /* T → tam32 */
        } else {
            bt = ast_tip_to_bilgi(tk, pt);
        }
        TipBilgisi *at = tip_belirle_beklenen(tk,
            d->veri.cagri.argumanlar[i], bt);
        if (at->kategori != TIP_HATA && bt->kategori != TIP_HATA &&
            !tip_esit(at, bt)) {
            tip_hata(tk, d->veri.cagri.argumanlar[i], "M004",
                     "cesit varyant payload tipi uyumsuz");
        }
    }
    /* Generic ise concrete instance dön (beklenen'in tip_arg'ını taşı). */
    if (generic && beklenen && beklenen->kategori == TIP_YAPI &&
        beklenen->veri.yapi.tip_arg_sayi > 0) {
        return tip_olustur_yapi(tk->arena, cd->veri.cesit.ad,
                                cd->veri.cesit.ad_uzunluk,
                                beklenen->veri.yapi.tip_arg,
                                beklenen->veri.yapi.tip_arg_sayi);
    }
    return tip_olustur_yapi(tk->arena, cd->veri.cesit.ad,
                            cd->veri.cesit.ad_uzunluk, NULL, 0);
}

/* Ana visitor */
TipBilgisi *tip_belirle(TipKontrol *tk, const Dugum *d) {
    if (!d) return t_hata(tk);

    switch (d->tip) {
        /* === Literaller === */
        case DUGUM_TAM:
            return t_basit(tk, TIP_TAM32);  /* default; ADIM 11.5'te context */
        case DUGUM_KESIRLI:
            return t_basit(tk, TIP_KESIRLI64);
        case DUGUM_METIN:
            return t_basit(tk, TIP_METIN);
        case DUGUM_KARAKTER:
            return t_basit(tk, TIP_KARAKTER);
        case DUGUM_MANTIKSAL:
            return t_basit(tk, TIP_MANTIKSAL);
        case DUGUM_BOS:
            return t_basit(tk, TIP_BOS);

        /* === Tanimlayici === */
        case DUGUM_TANIMLAYICI: {
            /* hiç -> seçimlik<T> none */
            if (d->veri.tanimlayici.uzunluk == 4 /* hiç = h+i+c+'̧'? UTF-8 = 4 byte */ &&
                memcmp(d->veri.tanimlayici.metin, "hi\xc3\xa7", 4) == 0) {
                /* T inference: beklenen tip varsa kullan, yoksa BILINMIYOR */
                TipBilgisi *ic = tip_olustur_basit(tk->arena, TIP_BILINMIYOR);
                return tip_olustur_secimlik(tk->arena, ic);
            }
            /* Tek-gecis ad cozumu: kazanan sembol + kategori dugume
             * yazilir (codegen tuketir — bkz. ast.h CozumKategorisi). */
            const Sembol *s = sembol_coz_ve_bagla(tk, d,
                d->veri.tanimlayici.metin, d->veri.tanimlayici.uzunluk);
            if (!s) {
                tip_hata(tk, d, "T002", "tanimsiz sembol");
                return t_hata(tk);
            }
            /* D-252: küresel değişken erişimi (okuma+yazma) YALNIZ güvensiz blokta
             * (Kırılmazlık: paylaşılan-mutable-durum = confinement'ın kaçındığı
             * aliasing → güvensize hapis). Safe .kem'de → E010. */
            if (s->kuresel && tk->guvensiz_baglam == 0) {
                tip_hata(tk, d, "E010",
                    "kuresel degiskene erisim yalniz guvensiz blokta");
            }
            /* Linear Types Spec V1: lambda govdesi icindeki lineer
             * baglamalar otomatik 'yakalama' sayilir → consume + closure
             * tipi tekkez<...> olarak isaretlenir (LC-2). */
            lineer_yakalama_kontrol(tk, d);
            /* G005: genel yakalama (lineer + lineer-olmayan) izle. */
            genel_yakalama_kontrol(tk, d);
            return s->tip ? s->tip : t_hata(tk);
        }

        /* === Linear Types Spec V1: kullan(e) extract === */
        case DUGUM_KULLAN_IFADE: {
            TipBilgisi *t = tip_belirle(tk, d->veri.kullan_ifade.operand);
            if (t->kategori == TIP_HATA) return t_hata(tk);
            if (t->kategori != TIP_TEKKEZ) {
                tip_hata(tk, d, "L007",
                    "kullan(...) operandi tekkez tipinde olmali");
                return t_hata(tk);
            }
            lineer_tuket_eger_baglamaysa(tk, d->veri.kullan_ifade.operand);
            return t->veri.tekkez.ic;
        }

        /* === Linear Types Spec V1: imha(e) dispose === */
        case DUGUM_IMHA_IFADE: {
            TipBilgisi *t = tip_belirle(tk, d->veri.imha_ifade.operand);
            if (t->kategori == TIP_HATA) return t_hata(tk);
            /* D-313: `imha` HERHANGİ bir lineer değeri kabul eder — `yapı tekkez K`
             * dahil. (`kullan` etmez: o sarmalanmış değeri ÇIKARIR, lineer yapının
             * sarmalanmış bir değeri yoktur → L007 orada aynen kalır.) İmha, lineer
             * yapıyı tüketmenin tek yerel yoludur; diğerleri taşımadır (arg/ver). */
            if (t->kategori != TIP_TEKKEZ && !tip_lineer_mi(t)) {
                tip_hata(tk, d, "L007",
                    "imha(...) operandi lineer tipte olmali "
                    "(tekkez<T>, yetki<R>, gorev<T> veya `yapi tekkez`)");
                return t_hata(tk);
            }
            tk->imha_baglaminda++;   /* D-315: kısmi taşınmış yapı imha edilebilir */
            lineer_tuket_eger_baglamaysa(tk, d->veri.imha_ifade.operand);
            tk->imha_baglaminda--;
            return t_basit(tk, TIP_BOS);
        }

        /* === Madde E (v2): explicit tip donusturme (x olarak T) — 4 garanti ===
         *   E001: x olarak tekkez<T> yasak (tekkez hedef)
         *   E002: metin/yapi/dizi gibi sayisal-disi kaynak yasak
         *   E003: tekkez<T> olarak X yasak (linear escape — kaynak tekkez)
         *   E004: kayıp prezisyon (tam64 olarak tam8, kesirli64 olarak kesirli32)
         */
        case DUGUM_TIP_DONUSTUR: {
            TipBilgisi *kt = tip_belirle(tk, d->veri.tip_donustur.kaynak);
            if (kt->kategori == TIP_HATA) return t_hata(tk);
            TipBilgisi *ht = ast_tip_to_bilgi(tk, d->veri.tip_donustur.hedef_tip);
            if (!ht || ht->kategori == TIP_HATA) return t_hata(tk);

            /* E001: hedef tekkez<T> yasak (Linear Types: olarak ile olusturamazsin) */
            if (ht->kategori == TIP_TEKKEZ) {
                tip_hata(tk, d, "E001",
                    "olarak ile tekkez<T> hedeflenemez (Linear Types kuralı)");
                return t_hata(tk);
            }

            /* E003: kaynak tekkez<T> yasak (linear escape — tekkez'i extract
             * etmek icin kullan() gerek, olarak ile escape yapilamaz) */
            if (kt->kategori == TIP_TEKKEZ) {
                tip_hata(tk, d, "E003",
                    "olarak ile tekkez<T> kaynaktan extract edilemez "
                    "(kullan(...) gerekir)");
                return t_hata(tk);
            }

            /* v1 bölge-container (E002 DAR gevsetme): YALNIZ guvenli yon
             * *T -> metin (typed -> opaque ptr) — bellek_kopyala grow-copy
             * icin. TERS YON KAPALI: metin -> *T ve tamN -> *T yasak;
             * typed buffer YALNIZ bölge_al'den dogar (tip butunlugu). */
            if (kt->kategori == TIP_POINTER && ht->kategori == TIP_METIN) {
                return ht;
            }
            /* D-248 (GAP-1): güvensiz blokta int <-> *T cast — integer-adres ->
             * ham pointer (MMIO/heap ham-bellek). YALNIZ güvensiz (raw pointer
             * güvensiz-scope; safe .kem etkilenmez). codegen inttoptr/ptrtoint. */
            if (tk->guvensiz_baglam != 0) {
                if (tip_tamsayi_mi(kt) && ht->kategori == TIP_POINTER) {
                    return ht;   /* int -> *T (inttoptr) */
                }
                if (kt->kategori == TIP_POINTER && tip_tamsayi_mi(ht)) {
                    return ht;   /* *T -> int (ptrtoint) */
                }
            }
            int kaynak_sayisal = tip_sayisal_mi(kt);
            int hedef_sayisal = tip_sayisal_mi(ht);
            if (!kaynak_sayisal || !hedef_sayisal) {
                /* Karakter <-> tam* da izinli */
                int char_to_int = (kt->kategori == TIP_KARAKTER &&
                                   tip_tamsayi_mi(ht));
                int int_to_char = (tip_tamsayi_mi(kt) &&
                                   ht->kategori == TIP_KARAKTER);
                if (!char_to_int && !int_to_char) {
                    /* E002: metin/dizi/yapi gibi sayisal disi kaynak/hedef */
                    tip_hata(tk, d, "E002",
                        "olarak: kaynak ve hedef sayisal/karakter olmali");
                    return t_hata(tk);
                }
            }

            /* E004: Kayip prezisyon — tam64/dtam64 -> tam8/16/32 ya da
             * kesirli64 -> kesirli32 explicit isaretsiz dusurme.
             * Bu cesit cast yapilmasi gerekiyorsa, kullanici niyet
             * ifade etmeli (& mask, mod, vs.) — implicit aritmetigi onler. */
            int kw_kaynak = 0, kw_hedef = 0;
            switch (kt->kategori) {
                case TIP_TAM8: case TIP_DTAM8:    kw_kaynak = 8; break;
                case TIP_TAM16: case TIP_DTAM16:  kw_kaynak = 16; break;
                case TIP_TAM32: case TIP_DTAM32:  kw_kaynak = 32; break;
                case TIP_TAM64: case TIP_DTAM64:  kw_kaynak = 64; break;
                case TIP_KESIRLI32:               kw_kaynak = 320; break;
                case TIP_KESIRLI64:               kw_kaynak = 640; break;
                case TIP_KARAKTER:                kw_kaynak = 32; break;
                default: break;
            }
            switch (ht->kategori) {
                case TIP_TAM8: case TIP_DTAM8:    kw_hedef = 8; break;
                case TIP_TAM16: case TIP_DTAM16:  kw_hedef = 16; break;
                case TIP_TAM32: case TIP_DTAM32:  kw_hedef = 32; break;
                case TIP_TAM64: case TIP_DTAM64:  kw_hedef = 64; break;
                case TIP_KESIRLI32:               kw_hedef = 320; break;
                case TIP_KESIRLI64:               kw_hedef = 640; break;
                case TIP_KARAKTER:                kw_hedef = 32; break;
                default: break;
            }
            /* E004: kayip prezisyon. Pratik kural: tam64 -> tam8/16 yasak,
             * kesirli64 -> kesirli32 yasak. tam64 -> tam32 izinli (32-bit
             * native word). Bu, "ortakli olunca cast" semantigini korur
             * ama acik narrowing'i blok eder. */
            int kaynak_int = kw_kaynak > 0 && kw_kaynak <= 64;
            int hedef_int = kw_hedef > 0 && kw_hedef <= 64;
            int kaynak_float = kw_kaynak >= 320;
            int hedef_float = kw_hedef >= 320;
            /* Sadece >32-bit kaynak -> <32-bit hedef yasak (significant lost) */
            if (kaynak_int && hedef_int && kw_kaynak >= 64 && kw_hedef < 32) {
                tip_hata(tk, d, "E004",
                    "olarak: kayip prezisyon (tam64 -> tam8/tam16)");
                return t_hata(tk);
            }
            if (kaynak_float && hedef_float && kw_kaynak > kw_hedef) {
                tip_hata(tk, d, "E004",
                    "olarak: kayip prezisyon (kesirli64 -> kesirli32)");
                return t_hata(tk);
            }
            return ht;
        }

        /* === Ikili === */
        case DUGUM_IKILI: {
            TipBilgisi *sol = tip_belirle(tk, d->veri.ikili.sol);
            TipBilgisi *sag = tip_belirle(tk, d->veri.ikili.sag);
            if (sol->kategori == TIP_HATA || sag->kategori == TIP_HATA) {
                return t_hata(tk);
            }
            /* Sayı literal BAĞLAM-BAĞIMLI (CLAUDE.md): bir taraf TİPLİ tamsayı,
             * diğer taraf TİPSİZ tamsayı LİTERALİ (DUGUM_TAM) ve tipler farklı
             * ise — literali karşı tarafın tipinde yeniden çıkar (x: tam64;
             * x + 1 artık T001 vermez; codegen zaten genişletiyordu). Explicit
             * cast (… olarak tamX) DUGUM_TAM değildir → etkilenmez; tam32+1
             * zaten eşit → etkilenmez. sabitsüre/vektör HARİÇ (taint/lane
             * yayılımı kendi kurallarına sahip — S2/V testleri). */
            if (!tip_sabitsure_mi(sol) && !tip_sabitsure_mi(sag) &&
                !tip_vektor_mu(sol) && !tip_vektor_mu(sag)) {
                if (d->veri.ikili.sol->tip == DUGUM_TAM &&
                    tip_tamsayi_mi(sol) && tip_tamsayi_mi(sag) &&
                    !tip_esit(sol, sag)) {
                    sol = tip_belirle_beklenen(tk, d->veri.ikili.sol, sag);
                } else if (d->veri.ikili.sag->tip == DUGUM_TAM &&
                           tip_tamsayi_mi(sol) && tip_tamsayi_mi(sag) &&
                           !tip_esit(sol, sag)) {
                    sag = tip_belirle_beklenen(tk, d->veri.ikili.sag, sol);
                }
            }
            switch (d->veri.ikili.op) {
                case OP_ARTI:  case OP_EKSI:
                case OP_CARPI: case OP_BOLU:  case OP_MOD:
                    return kontrol_ikili_sayisal(tk, d, sol, sag);

                case OP_ESIT:  case OP_ESIT_DEGIL: {
                    /* İç tipler eşit olmalı (sabitsüre<T> == T iç olarak eşit) */
                    TipBilgisi *sol_ic = tip_ic_cek(sol);
                    TipBilgisi *sag_ic = tip_ic_cek(sag);
                    if (!tip_esit(sol_ic, sag_ic)) {
                        tip_hata(tk, d, "T001",
                                 "esitlik karsilastirma ayni tip olmali");
                        return t_hata(tk);
                    }
                    /* Sabitsüre Spec V1 CT-CMP: sabitsüre operand → sonuç
                     * sabitsüre<mantıksal> (eşitlik bilgisi de gizli). */
                    TipBilgisi *base = t_basit(tk, TIP_MANTIKSAL);
                    return taint_yay(tk, base, sol, sag);
                }

                case OP_KUCUK: case OP_BUYUK:
                case OP_KUCUK_ESIT: case OP_BUYUK_ESIT: {
                    if (!tip_sayisal_mi(sol) || !tip_sayisal_mi(sag)) {
                        tip_hata(tk, d, "T003",
                                 "karsilastirma sayisal tip ister");
                        return t_hata(tk);
                    }
                    TipBilgisi *sol_ic = tip_ic_cek(sol);
                    TipBilgisi *sag_ic = tip_ic_cek(sag);
                    if (!tip_esit(sol_ic, sag_ic)) {
                        tip_hata(tk, d, "T001",
                                 "karsilastirma iki tarafi ayni tip olmali");
                        return t_hata(tk);
                    }
                    TipBilgisi *base = t_basit(tk, TIP_MANTIKSAL);
                    return taint_yay(tk, base, sol, sag);
                }

                case OP_VE: case OP_VEYA:
                    return kontrol_ikili_mantiksal(tk, d, sol, sag);

                case OP_BIT_VE: case OP_BIT_VEYA: case OP_BIT_OZVEYA: {
                    /* SIMD Spec V1: vektör tamsayı tiplerinde bit op izinli;
                     * kesirli vektörde V006 hata. */
                    if (tip_vektor_mu(sol) || tip_vektor_mu(sag)) {
                        if (tip_vektor_mu(sol) && tip_vektor_mu(sag)) {
                            /* Kesirli vektörde bit op yasak (V006) */
                            int kesirli_elem =
                                sol->veri.vektor.eleman->kategori == TIP_KESIRLI32 ||
                                sol->veri.vektor.eleman->kategori == TIP_KESIRLI64;
                            if (kesirli_elem) {
                                tip_hata(tk, d, "V006",
                                    "kesirli vektor uzerinde bit operatoru yasak");
                                return t_hata(tk);
                            }
                        }
                        return kontrol_ikili_vektor(tk, d, sol, sag);
                    }
                    /* Bit AND/OR/XOR: her iki operand tamsayi, iç tipler eşit.
                     * Sabitsüre Spec V1: taint yayılım (sabitsüre<T> ^ T → sabitsüre<T>) */
                    if (!tip_tamsayi_mi(sol)) {
                        tip_hata(tk, d, "T028",
                                 "bit operatoru (& | ^) tamsayi tipi ister");
                        return t_hata(tk);
                    }
                    /* Bidirectional: sag, sol iç tipinde yeniden çıkarsanır
                     * (sabitsüre soyma) */
                    TipBilgisi *sol_ic = tip_ic_cek(sol);
                    TipBilgisi *sag2 = tip_belirle_beklenen(tk,
                        d->veri.ikili.sag, sol_ic);
                    if (!tip_tamsayi_mi(sag2)) {
                        tip_hata(tk, d, "T028",
                                 "bit operatoru (& | ^) tamsayi tipi ister");
                        return t_hata(tk);
                    }
                    TipBilgisi *sag2_ic = tip_ic_cek(sag2);
                    if (!tip_esit(sol_ic, sag2_ic)) {
                        tip_hata(tk, d, "T001",
                                 "bit operatoru iki tarafi ayni tip olmali");
                        return t_hata(tk);
                    }
                    return taint_yay(tk, sol_ic, sol, sag2);
                }

                case OP_SOLA_KAYDIR: case OP_SAGA_KAYDIR: {
                    /* Kaydir (<<, >>): sol tamsayi, sag tamsayi (kaydirma
                     * miktari). Sabitsüre Spec V1 CT008: kaydirma miktari
                     * (sag) sabitsüre olamaz (variable-shift bazı CPU'larda
                     * variable-time — ARM Cortex-M, eski Intel). */
                    if (!tip_tamsayi_mi(sol)) {
                        tip_hata(tk, d, "T028",
                                 "kaydirma operatoru sol taraf tamsayi ister");
                        return t_hata(tk);
                    }
                    if (!tip_tamsayi_mi(sag)) {
                        tip_hata(tk, d, "T028",
                                 "kaydirma miktari tamsayi olmali");
                        return t_hata(tk);
                    }
                    if (tip_sabitsure_mi(sag)) {
                        tip_hata(tk, d, "CT008",
                            "kaydirma miktari sabitsure olamaz "
                            "(variable-shift variable-time)");
                        return t_hata(tk);
                    }
                    /* Taint yayılım: sol sabitsüre ise sonuç sabitsüre */
                    TipBilgisi *sol_ic = tip_ic_cek(sol);
                    if (tip_sabitsure_mi(sol)) {
                        return tip_olustur_sabitsure(tk->arena, sol_ic);
                    }
                    return sol;
                }

                default:
                    tip_hata(tk, d, "T001", "bilinmeyen ikili operator");
                    return t_hata(tk);
            }
        }

        /* === Tekli === */
        case DUGUM_TEKLI: {
            TipBilgisi *op = tip_belirle(tk, d->veri.tekli.operand);
            if (op->kategori == TIP_HATA) return t_hata(tk);
            switch (d->veri.tekli.op) {
                case OP_NEG:
                    if (!tip_sayisal_mi(op)) {
                        tip_hata(tk, d, "T003", "tekli '-' sayisal ister");
                        return t_hata(tk);
                    }
                    /* Sabitsüre Spec V1: -sabitsure<T> -> sabitsure<T> (taint korunur) */
                    return op;

                case OP_DEGIL:
                    if (!tip_mantiksal_mi(op)) {
                        tip_hata(tk, d, "T004", "'degil' mantiksal ister");
                        return t_hata(tk);
                    }
                    /* Taint korunur */
                    if (tip_sabitsure_mi(op)) {
                        return tip_olustur_sabitsure(tk->arena,
                            t_basit(tk, TIP_MANTIKSAL));
                    }
                    return t_basit(tk, TIP_MANTIKSAL);

                case OP_BIT_DEGIL:
                    if (!tip_tamsayi_mi(op)) {
                        tip_hata(tk, d, "T028",
                                 "bit DEGIL (~) tamsayi tipi ister");
                        return t_hata(tk);
                    }
                    /* Taint korunur */
                    return op;

                case OP_REF:
                    /* Linear Types Spec V1 L004 + Capability CP005:
                     * lineer (tekkez veya yetki) tipinde referans alinamaz */
                    if (tip_lineer_mi(op)) {
                        const char *kod = (op->kategori == TIP_YETKI)
                                          ? "CP005" : "L004";
                        const char *msg = (op->kategori == TIP_YETKI)
                            ? "yetki<R> tipinde referans alinamaz (linear ihlal)"
                            : "lineer (tekkez) tipinde referans alinamaz";
                        tip_hata(tk, d, kod, msg);
                        return t_hata(tk);
                    }
                    return tip_olustur_referans(tk->arena, op, 0);

                case OP_REF_DEGISKEN:
                    if (tip_lineer_mi(op)) {
                        const char *kod = (op->kategori == TIP_YETKI)
                                          ? "CP005" : "L004";
                        const char *msg = (op->kategori == TIP_YETKI)
                            ? "yetki<R> tipinde &degisken alinamaz (linear ihlal)"
                            : "lineer (tekkez) tipinde &degisken alinamaz";
                        tip_hata(tk, d, kod, msg);
                        return t_hata(tk);
                    }
                    return tip_olustur_referans(tk->arena, op, 1);

                case OP_DEREFERANS:
                    /* D-305: GÜVENLİ referans deref — `*v` ile &T'den T oku.
                     * Referans her zaman geçerli (ham pointer değil) → güvensiz
                     * GEREKMEZ. Yapı referansı zaten `.alan` ile auto-deref
                     * ediyordu; skaler `&T` için okuma yolu buydu (eskiden
                     * `*v` T001, `ver v`/`v+0` de reddediliyordu → hiç okunamıyordu). */
                    if (op->kategori == TIP_REFERANS) {
                        return op->veri.referans.hedef
                             ? op->veri.referans.hedef : t_hata(tk);
                    }
                    if (op->kategori != TIP_POINTER) {
                        tip_hata(tk, d, "T001",
                                 "'*' sadece pointer ya da referans tipinde "
                                 "kullanilir");
                        return t_hata(tk);
                    }
                    /* C5 on-kosul #2: *T HAM pointer dereferansi yalniz
                     * guvensiz blokta (ast.h'deki kural artik enforce).
                     * Güvenli referans (&T) yukarıda ele alındı — güvensiz istemez. */
                    if (tk->guvensiz_baglam == 0) {
                        tip_hata(tk, d, "G001",
                                 "*T ham pointer dereferans yalniz guvensiz blok "
                                 "icinde kullanilabilir");
                        return t_hata(tk);
                    }
                    return op->veri.pointer.hedef;

                default:
                    tip_hata(tk, d, "T001", "bilinmeyen tekli operator");
                    return t_hata(tk);
            }
        }

        /* === Cagri === */
        case DUGUM_CAGRI: {
            /* C3: çeşit varyant yapıcısı (Cesit::V(args)) — modül-fonksiyon
             * çağrısından ÖNCE dene (sol bir çeşit ise yapıcı). Beklenen-yok
             * yolu: generic çeşit T'sini çözemez (construction beklenen ister). */
            {
                TipBilgisi *cy = cesit_yapici_tip_kontrol(tk, d, NULL);
                if (cy) return cy;
            }
            /* Yerlesik konstrüktörler: değer(x), tamam(x), hata(x) */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.sayi == 1) {
                const char *ad = d->veri.cagri.hedef->veri.tanimlayici.metin;
                int uz = d->veri.cagri.hedef->veri.tanimlayici.uzunluk;
                /* "değer" = de\xc4\x9fer (6 byte) */
                if (uz == 6 && memcmp(ad, "de\xc4\x9f" "er", 6) == 0) {
                    TipBilgisi *iç = tip_belirle(tk,
                        d->veri.cagri.argumanlar[0]);
                    return tip_olustur_secimlik(tk->arena, iç);
                }
                /* "tamam" (5 byte) */
                if (uz == 5 && memcmp(ad, "tamam", 5) == 0) {
                    TipBilgisi *deg = tip_belirle(tk,
                        d->veri.cagri.argumanlar[0]);
                    TipBilgisi *hata = tip_olustur_basit(tk->arena,
                        TIP_BILINMIYOR);
                    return tip_olustur_sonuc(tk->arena, deg, hata);
                }
                /* "hata" (4 byte) */
                if (uz == 4 && memcmp(ad, "hata", 4) == 0) {
                    TipBilgisi *h = tip_belirle(tk,
                        d->veri.cagri.argumanlar[0]);
                    TipBilgisi *deg = tip_olustur_basit(tk->arena,
                        TIP_BILINMIYOR);
                    return tip_olustur_sonuc(tk->arena, deg, h);
                }
            }
            /* === Madde B: Dinamik dizi intrinsicleri === */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI) {
                const char *ad_b = d->veri.cagri.hedef->veri.tanimlayici.metin;
                int uz_b = d->veri.cagri.hedef->veri.tanimlayici.uzunluk;

                /* dizi_olustur<T>(N: tam64) -> Dizi<T>
                 * T context'ten (beklenen tip Dizi<T> ise) gelir;
                 * yoksa Dizi<tam32> varsayilir (v1 default). */
                if (uz_b == 12 && memcmp(ad_b, "dizi_olustur", 12) == 0) {
                    if (d->veri.cagri.sayi != 1) {
                        tip_hata(tk, d, "T010",
                            "dizi_olustur tam olarak bir arguman gerektirir");
                        return t_hata(tk);
                    }
                    TipBilgisi *kap = tip_belirle_beklenen(tk,
                        d->veri.cagri.argumanlar[0],
                        tip_olustur_basit(tk->arena, TIP_TAM64));
                    if (kap->kategori != TIP_HATA &&
                        kap->kategori != TIP_TAM64 &&
                        kap->kategori != TIP_TAM32) {
                        tip_hata(tk, d->veri.cagri.argumanlar[0], "T028",
                            "dizi_olustur kapasite tamsayi olmali");
                    }
                    /* T inferred: beklenen ya Dizi<T> ya da default tam32 */
                    TipBilgisi *eleman = NULL;
                    /* Beklenen TipKontrol'un cagiranindan gelmiyor doğrudan;
                     * beklenen Dizi<T> ise context'ten alinabilir — su an
                     * default tam32 (v1; v2'de degisken annot ile baglar) */
                    eleman = tip_olustur_basit(tk->arena, TIP_TAM32);
                    return tip_olustur_dizi(tk->arena, eleman);
                }

                /* dizi_ekle<T>(d: Dizi<T>, e: T) -> bos */
                if (uz_b == 9 && memcmp(ad_b, "dizi_ekle", 9) == 0) {
                    if (d->veri.cagri.sayi != 2) {
                        tip_hata(tk, d, "T010",
                            "dizi_ekle iki arguman gerektirir (d, e)");
                        return t_hata(tk);
                    }
                    TipBilgisi *dt = tip_belirle(tk, d->veri.cagri.argumanlar[0]);
                    TipBilgisi *dz = dizi_arg_coz(dt);   /* &Dizi auto-deref */
                    if (!dz && dt->kategori != TIP_HATA) {
                        tip_hata(tk, d->veri.cagri.argumanlar[0], "T001",
                            "dizi_ekle ilk argumani Dizi<T> olmali");
                    }
                    dz_buyutme_kontrol(tk, d, dz, "dizi_ekle");   /* DZ003 */
                    TipBilgisi *bek = dz ? dz->veri.dizi.eleman : NULL;
                    TipBilgisi *et = tip_belirle_beklenen(tk,
                        d->veri.cagri.argumanlar[1], bek);
                    if (bek && !tip_esit(et, bek) &&
                        et->kategori != TIP_HATA) {
                        tip_hata(tk, d->veri.cagri.argumanlar[1], "T001",
                            "dizi_ekle eleman tipi Dizi'nin eleman tipinden farkli");
                    }
                    return tip_olustur_basit(tk->arena, TIP_BOS);
                }

                /* dizi_al<T>(d: Dizi<T>, i: tam32) -> T */
                if (uz_b == 7 && memcmp(ad_b, "dizi_al", 7) == 0) {
                    if (d->veri.cagri.sayi != 2) {
                        tip_hata(tk, d, "T010",
                            "dizi_al iki arguman gerektirir (d, i)");
                        return t_hata(tk);
                    }
                    TipBilgisi *dt = tip_belirle(tk, d->veri.cagri.argumanlar[0]);
                    TipBilgisi *idx = tip_belirle_beklenen(tk,
                        d->veri.cagri.argumanlar[1],
                        tip_olustur_basit(tk->arena, TIP_TAM32));
                    if (!tip_tamsayi_mi(idx) && idx->kategori != TIP_HATA) {
                        tip_hata(tk, d->veri.cagri.argumanlar[1], "T028",
                            "dizi_al indeks tamsayi olmali");
                    }
                    TipBilgisi *dz = dizi_arg_coz(dt);   /* &Dizi auto-deref */
                    if (dz) return dz->veri.dizi.eleman;
                    return t_hata(tk);
                }

                /* dizi_yaz<T>(d: Dizi<T>, i: tam32, e: T) -> bos
                 * i. elemanı YERİNDE günceller (dizi_al'ın yazma eşi).
                 * Mutable cursor / in-place güncelleme (recursive-descent
                 * parser konum imleci) için gerekli. */
                if (uz_b == 8 && memcmp(ad_b, "dizi_yaz", 8) == 0) {
                    if (d->veri.cagri.sayi != 3) {
                        tip_hata(tk, d, "T010",
                            "dizi_yaz uc arguman gerektirir (d, i, e)");
                        return t_hata(tk);
                    }
                    TipBilgisi *dt = tip_belirle(tk, d->veri.cagri.argumanlar[0]);
                    TipBilgisi *dz = dizi_arg_coz(dt);   /* &Dizi auto-deref */
                    if (!dz && dt->kategori != TIP_HATA) {
                        tip_hata(tk, d->veri.cagri.argumanlar[0], "T001",
                            "dizi_yaz ilk argumani Dizi<T> olmali");
                    }
                    TipBilgisi *idx = tip_belirle_beklenen(tk,
                        d->veri.cagri.argumanlar[1],
                        tip_olustur_basit(tk->arena, TIP_TAM32));
                    if (!tip_tamsayi_mi(idx) && idx->kategori != TIP_HATA) {
                        tip_hata(tk, d->veri.cagri.argumanlar[1], "T028",
                            "dizi_yaz indeks tamsayi olmali");
                    }
                    TipBilgisi *bek = dz ? dz->veri.dizi.eleman : NULL;
                    TipBilgisi *et = tip_belirle_beklenen(tk,
                        d->veri.cagri.argumanlar[2], bek);
                    if (bek && !tip_esit(et, bek) &&
                        et->kategori != TIP_HATA) {
                        tip_hata(tk, d->veri.cagri.argumanlar[2], "T001",
                            "dizi_yaz eleman tipi Dizi'nin eleman tipinden farkli");
                    }
                    return tip_olustur_basit(tk->arena, TIP_BOS);
                }

                /* dizi_boyut(d: Dizi<T>) -> tam32 */
                if (uz_b == 10 && memcmp(ad_b, "dizi_boyut", 10) == 0) {
                    if (d->veri.cagri.sayi != 1) {
                        tip_hata(tk, d, "T010",
                            "dizi_boyut bir arguman gerektirir");
                        return t_hata(tk);
                    }
                    TipBilgisi *dt = tip_belirle(tk, d->veri.cagri.argumanlar[0]);
                    if (!dizi_arg_coz(dt) && dt->kategori != TIP_HATA) {
                        tip_hata(tk, d->veri.cagri.argumanlar[0], "T001",
                            "dizi_boyut argumani Dizi<T> olmali");
                    }
                    return tip_olustur_basit(tk->arena, TIP_TAM32);
                }
                /* Adim 6: dizi_kapasite(d: Dizi<T>) -> tam32 */
                if (uz_b == 13 && memcmp(ad_b, "dizi_kapasite", 13) == 0) {
                    if (d->veri.cagri.sayi != 1) {
                        tip_hata(tk, d, "T010",
                            "dizi_kapasite bir arguman gerektirir");
                        return t_hata(tk);
                    }
                    TipBilgisi *dt = tip_belirle(tk, d->veri.cagri.argumanlar[0]);
                    if (!dizi_arg_coz(dt) && dt->kategori != TIP_HATA) {
                        tip_hata(tk, d->veri.cagri.argumanlar[0], "T001",
                            "dizi_kapasite argumani Dizi<T> olmali");
                    }
                    return tip_olustur_basit(tk->arena, TIP_TAM32);
                }

                /* Adim 6: dizi_kapasite_ayarla(d, yeni: tam32) -> bos */
                if (uz_b == 20 &&
                    memcmp(ad_b, "dizi_kapasite_ayarla", 20) == 0) {
                    if (d->veri.cagri.sayi != 2) {
                        tip_hata(tk, d, "T010",
                            "dizi_kapasite_ayarla iki arguman gerektirir");
                        return t_hata(tk);
                    }
                    TipBilgisi *dt = tip_belirle(tk, d->veri.cagri.argumanlar[0]);
                    TipBilgisi *yt = tip_belirle_beklenen(tk,
                        d->veri.cagri.argumanlar[1],
                        tip_olustur_basit(tk->arena, TIP_TAM32));
                    if (!dizi_arg_coz(dt) && dt->kategori != TIP_HATA) {
                        tip_hata(tk, d->veri.cagri.argumanlar[0], "T001",
                            "dizi_kapasite_ayarla ilk arg Dizi<T> olmali");
                    }
                    dz_buyutme_kontrol(tk, d, dizi_arg_coz(dt),
                                       "dizi_kapasite_ayarla");   /* DZ003 */
                    if (!tip_tamsayi_mi(yt) && yt->kategori != TIP_HATA) {
                        tip_hata(tk, d->veri.cagri.argumanlar[1], "T028",
                            "dizi_kapasite_ayarla yeni kapasite tamsayi olmali");
                    }
                    return tip_olustur_basit(tk->arena, TIP_BOS);
                }
            }

            /* Linear Types Spec V1 producer intrinsic: tekkez_olustur(e) */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 14 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "tekkez_olustur", 14) == 0) {
                if (d->veri.cagri.sayi != 1) {
                    tip_hata(tk, d, "L008",
                        "tekkez_olustur tam olarak bir arguman gerektirir");
                    return t_hata(tk);
                }
                TipBilgisi *ic = tip_belirle(tk, d->veri.cagri.argumanlar[0]);
                if (ic->kategori == TIP_HATA) return t_hata(tk);
                /* tekkez<tekkez<T>> destekli: ic lineer ise wrap edilmeden
                 * once tuketmeliyiz (move into outer wrapper). */
                if (ic->kategori == TIP_TEKKEZ) {
                    lineer_tuket_eger_baglamaysa(tk,
                        d->veri.cagri.argumanlar[0]);
                }
                return tip_olustur_tekkez(tk->arena, ic);
            }
            /* Sabitsüre Spec V1 producer: sabitsüre_olustur(v: T) -> sabitsüre<T>
             * UTF-8: "sabits\xc3\xbcre_olustur" = 18 byte */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 18 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "sabits\xc3\xbc" "re_olustur", 18) == 0) {
                if (d->veri.cagri.sayi != 1) {
                    tip_hata(tk, d, "CT005",
                        "sabitsure_olustur tam olarak bir arguman gerektirir");
                    return t_hata(tk);
                }
                TipBilgisi *ic = tip_belirle(tk, d->veri.cagri.argumanlar[0]);
                if (ic->kategori == TIP_HATA) return t_hata(tk);
                /* Arg sabitsure ise nesting redundancy (CT006) */
                if (tip_sabitsure_mi(ic)) {
                    tip_hata(tk, d, "CT006",
                        "sabitsure<sabitsure<T>> nesting yasak");
                    return t_hata(tk);
                }
                /* İç tip yetenekli mi? */
                if (!tip_sabitsure_yetenekli_mi(ic)) {
                    tip_hata(tk, d, "CT006",
                        "sabitsure_olustur: sarilan tip constant-time yetenekli degil "
                        "(kesirli/metin/yapi yasak)");
                    return t_hata(tk);
                }
                return tip_olustur_sabitsure(tk->arena, ic);
            }
            /* === SIMD Spec V1 intrinsicleri ===
             * Bunların hepsi generic (vektör<T, N>); built-in tablo yerine
             * burada özel-cased. Argümanın tipinden T ve N çıkarılır. */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI) {
                const char *fn_ad = d->veri.cagri.hedef->veri.tanimlayici.metin;
                int fn_uz = d->veri.cagri.hedef->veri.tanimlayici.uzunluk;
                /* vektor_doldur(s: T) — V1: tek arg, dönüş <unknown,?>; bidirectional
                 * context'ten gelen beklenen tip vektor<T,N> ise ona uy.
                 * Beklenen yoksa hata. */
                if (fn_uz == 14 && memcmp(fn_ad, "vektor_doldur", 13) == 0 &&
                    fn_ad[13] == '\0') {
                    /* Bu yol kullanılmıyor — string null-terminate edilmemiş */
                }
                /* vektor_doldur(s: T) — context'ten N öğrenir (bidirectional) */
                if (fn_uz == 13 && memcmp(fn_ad, "vektor_doldur", 13) == 0) {
                    if (d->veri.cagri.sayi != 1) {
                        tip_hata(tk, d, "V020",
                            "vektor_doldur tam olarak 1 arguman alir");
                        return t_hata(tk);
                    }
                    TipBilgisi *arg_t = tip_belirle(tk,
                        d->veri.cagri.argumanlar[0]);
                    if (arg_t->kategori == TIP_HATA) return t_hata(tk);
                    if (!tip_vektor_eleman_yetenekli_mi(arg_t)) {
                        tip_hata(tk, d, "V001",
                            "vektor_doldur argumani vektor-yetenekli skaler olmali");
                        return t_hata(tk);
                    }
                    /* N bilinmiyor — beklenen tip vektor ise N'i kullan */
                    /* Default N=4 (V1 — bidirectional inference kullanılmadan) */
                    return tip_olustur_vektor(tk->arena, arg_t, 4);
                }
                /* vektor_eleman(v, i) -> T */
                if (fn_uz == 13 && memcmp(fn_ad, "vektor_eleman", 13) == 0) {
                    if (d->veri.cagri.sayi != 2) {
                        tip_hata(tk, d, "V020",
                            "vektor_eleman(v, i) 2 arguman alir");
                        return t_hata(tk);
                    }
                    TipBilgisi *vt = tip_belirle(tk,
                        d->veri.cagri.argumanlar[0]);
                    TipBilgisi *it = tip_belirle(tk,
                        d->veri.cagri.argumanlar[1]);
                    if (vt->kategori == TIP_HATA || it->kategori == TIP_HATA)
                        return t_hata(tk);
                    if (!tip_vektor_mu(vt)) {
                        tip_hata(tk, d->veri.cagri.argumanlar[0], "V020",
                            "vektor_eleman ilk arguman vektor olmali");
                        return t_hata(tk);
                    }
                    if (!tip_tamsayi_mi(it)) {
                        tip_hata(tk, d->veri.cagri.argumanlar[1], "V020",
                            "vektor_eleman ikinci arguman tamsayi olmali");
                        return t_hata(tk);
                    }
                    return vt->veri.vektor.eleman;
                }
                /* vektor_topla(v) -> T (sum reduction) */
                if (fn_uz == 12 && memcmp(fn_ad, "vektor_topla", 12) == 0) {
                    if (d->veri.cagri.sayi != 1) {
                        tip_hata(tk, d, "V009",
                            "vektor_topla 1 arguman alir");
                        return t_hata(tk);
                    }
                    TipBilgisi *vt = tip_belirle(tk,
                        d->veri.cagri.argumanlar[0]);
                    if (vt->kategori == TIP_HATA) return t_hata(tk);
                    if (!tip_vektor_mu(vt)) {
                        tip_hata(tk, d, "V009",
                            "vektor_topla operandi vektor olmali");
                        return t_hata(tk);
                    }
                    if (!tip_sayisal_mi(vt->veri.vektor.eleman)) {
                        tip_hata(tk, d, "V009",
                            "vektor_topla operandi sayisal vektor olmali");
                        return t_hata(tk);
                    }
                    return vt->veri.vektor.eleman;
                }
                /* vektor_min(v) -> T, vektor_max(v) -> T */
                if ((fn_uz == 10 && memcmp(fn_ad, "vektor_min", 10) == 0) ||
                    (fn_uz == 10 && memcmp(fn_ad, "vektor_max", 10) == 0)) {
                    if (d->veri.cagri.sayi != 1) {
                        tip_hata(tk, d, "V009",
                            "vektor_min/max 1 arguman alir");
                        return t_hata(tk);
                    }
                    TipBilgisi *vt = tip_belirle(tk,
                        d->veri.cagri.argumanlar[0]);
                    if (vt->kategori == TIP_HATA) return t_hata(tk);
                    if (!tip_vektor_mu(vt) || !tip_sayisal_mi(vt->veri.vektor.eleman)) {
                        tip_hata(tk, d, "V009",
                            "vektor_min/max operandi sayisal vektor olmali");
                        return t_hata(tk);
                    }
                    return vt->veri.vektor.eleman;
                }
                /* vektor_esit(a, b) -> vektor<mantiksal, N> */
                if (fn_uz == 11 && memcmp(fn_ad, "vektor_esit", 11) == 0) {
                    if (d->veri.cagri.sayi != 2) {
                        tip_hata(tk, d, "V020",
                            "vektor_esit 2 arguman alir");
                        return t_hata(tk);
                    }
                    TipBilgisi *at = tip_belirle(tk,
                        d->veri.cagri.argumanlar[0]);
                    TipBilgisi *bt = tip_belirle(tk,
                        d->veri.cagri.argumanlar[1]);
                    if (at->kategori == TIP_HATA || bt->kategori == TIP_HATA)
                        return t_hata(tk);
                    if (!tip_vektor_mu(at) || !tip_vektor_mu(bt)) {
                        tip_hata(tk, d, "V003",
                            "vektor_esit her iki argumani vektor olmali");
                        return t_hata(tk);
                    }
                    if (at->veri.vektor.lane_sayi != bt->veri.vektor.lane_sayi ||
                        !tip_esit(at->veri.vektor.eleman, bt->veri.vektor.eleman)) {
                        tip_hata(tk, d, "V003",
                            "vektor_esit operandlari ayni T, N olmali");
                        return t_hata(tk);
                    }
                    return tip_olustur_vektor(tk->arena,
                        tip_olustur_basit(tk->arena, TIP_MANTIKSAL),
                        at->veri.vektor.lane_sayi);
                }
                /* vektor_kucuk / vektor_buyuk (analog) */
                if ((fn_uz == 12 && memcmp(fn_ad, "vektor_kucuk", 12) == 0) ||
                    (fn_uz == 12 && memcmp(fn_ad, "vektor_buyuk", 12) == 0)) {
                    if (d->veri.cagri.sayi != 2) {
                        tip_hata(tk, d, "V020",
                            "vektor_kucuk/buyuk 2 arguman alir");
                        return t_hata(tk);
                    }
                    TipBilgisi *at = tip_belirle(tk,
                        d->veri.cagri.argumanlar[0]);
                    TipBilgisi *bt = tip_belirle(tk,
                        d->veri.cagri.argumanlar[1]);
                    if (at->kategori == TIP_HATA || bt->kategori == TIP_HATA)
                        return t_hata(tk);
                    if (!tip_vektor_mu(at) || !tip_vektor_mu(bt) ||
                        at->veri.vektor.lane_sayi != bt->veri.vektor.lane_sayi ||
                        !tip_esit(at->veri.vektor.eleman, bt->veri.vektor.eleman)) {
                        tip_hata(tk, d, "V003",
                            "vektor_kucuk/buyuk operandlari ayni T, N vektor olmali");
                        return t_hata(tk);
                    }
                    return tip_olustur_vektor(tk->arena,
                        tip_olustur_basit(tk->arena, TIP_MANTIKSAL),
                        at->veri.vektor.lane_sayi);
                }
                /* vektor_sec(mask, a, b) -> vektor<T, N> */
                if (fn_uz == 10 && memcmp(fn_ad, "vektor_sec", 10) == 0) {
                    if (d->veri.cagri.sayi != 3) {
                        tip_hata(tk, d, "V010",
                            "vektor_sec(mask, a, b) 3 arguman alir");
                        return t_hata(tk);
                    }
                    TipBilgisi *mt = tip_belirle(tk,
                        d->veri.cagri.argumanlar[0]);
                    TipBilgisi *at = tip_belirle(tk,
                        d->veri.cagri.argumanlar[1]);
                    TipBilgisi *bt = tip_belirle(tk,
                        d->veri.cagri.argumanlar[2]);
                    if (mt->kategori == TIP_HATA || at->kategori == TIP_HATA ||
                        bt->kategori == TIP_HATA) return t_hata(tk);
                    if (!tip_vektor_mu(mt) ||
                        mt->veri.vektor.eleman->kategori != TIP_MANTIKSAL) {
                        tip_hata(tk, d, "V010",
                            "vektor_sec mask vektor<mantiksal, N> olmali");
                        return t_hata(tk);
                    }
                    if (!tip_vektor_mu(at) || !tip_vektor_mu(bt) ||
                        at->veri.vektor.lane_sayi != bt->veri.vektor.lane_sayi ||
                        at->veri.vektor.lane_sayi != mt->veri.vektor.lane_sayi ||
                        !tip_esit(at->veri.vektor.eleman, bt->veri.vektor.eleman)) {
                        tip_hata(tk, d, "V010",
                            "vektor_sec a/b ayni vektor tipi, N mask ile esit olmali");
                        return t_hata(tk);
                    }
                    return at;
                }
                /* vektor_ve_azalt(v: vektor<mantiksal, N>) -> mantiksal
                 *   13 byte */
                if (fn_uz == 15 && memcmp(fn_ad, "vektor_ve_azalt", 15) == 0) {
                    if (d->veri.cagri.sayi != 1) {
                        tip_hata(tk, d, "V009",
                            "vektor_ve_azalt 1 arguman alir");
                        return t_hata(tk);
                    }
                    TipBilgisi *vt = tip_belirle(tk,
                        d->veri.cagri.argumanlar[0]);
                    if (vt->kategori == TIP_HATA) return t_hata(tk);
                    if (!tip_vektor_mu(vt) ||
                        vt->veri.vektor.eleman->kategori != TIP_MANTIKSAL) {
                        tip_hata(tk, d, "V009",
                            "vektor_ve_azalt operandi vektor<mantiksal, N> olmali");
                        return t_hata(tk);
                    }
                    return tip_olustur_basit(tk->arena, TIP_MANTIKSAL);
                }
                /* vektor_veya_azalt(v) -> mantiksal */
                if (fn_uz == 17 && memcmp(fn_ad, "vektor_veya_azalt", 17) == 0) {
                    if (d->veri.cagri.sayi != 1) {
                        tip_hata(tk, d, "V009",
                            "vektor_veya_azalt 1 arguman alir");
                        return t_hata(tk);
                    }
                    TipBilgisi *vt = tip_belirle(tk,
                        d->veri.cagri.argumanlar[0]);
                    if (vt->kategori == TIP_HATA) return t_hata(tk);
                    if (!tip_vektor_mu(vt) ||
                        vt->veri.vektor.eleman->kategori != TIP_MANTIKSAL) {
                        tip_hata(tk, d, "V009",
                            "vektor_veya_azalt operandi vektor<mantiksal, N> olmali");
                        return t_hata(tk);
                    }
                    return tip_olustur_basit(tk->arena, TIP_MANTIKSAL);
                }
            }
            /* Sabitsüre Spec V1 declassification: ifsa(s: sabitsure<T>) -> T
             * UTF-8: "if\xc5\x9fa" = 5 byte */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 5 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "if\xc5\x9f" "a", 5) == 0) {
                if (d->veri.cagri.sayi != 1) {
                    tip_hata(tk, d, "CT007",
                        "ifsa tam olarak bir arguman gerektirir");
                    return t_hata(tk);
                }
                TipBilgisi *s = tip_belirle(tk, d->veri.cagri.argumanlar[0]);
                if (s->kategori == TIP_HATA) return t_hata(tk);
                if (!tip_sabitsure_mi(s)) {
                    tip_hata(tk, d, "CT007",
                        "ifsa(...) operandi sabitsure tipinde olmali");
                    return t_hata(tk);
                }
                return s->veri.sabitsure.ic;
            }
            /* === Capability Spec V1 intrinsics === */
            /* yetki_olustur(kaynak_tipi: tam16, izin: tam16) -> yetki<R> */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 13 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "yetki_olustur", 13) == 0) {
                if (d->veri.cagri.sayi != 2) {
                    tip_hata(tk, d, "CP004",
                        "yetki_olustur tam 2 arguman gerektirir "
                        "(kaynak_tipi: tam16, izin: tam16)");
                    return t_hata(tk);
                }
                /* Arg 1: kaynak tipi sabitini (literal) bekle — R'yi cikarmak icin */
                Dugum *arg0 = d->veri.cagri.argumanlar[0];
                if (arg0->tip != DUGUM_TAM) {
                    tip_hata(tk, arg0, "CP004",
                        "yetki_olustur ilk argumani sabit tamsayi olmali "
                        "(1=Dosya 2=Soket 3=Bellek 4=Donanim 5=OTP_Anahtar "
                        "6=MMIO)");
                    return t_hata(tk);
                }
                int64_t kt = arg0->veri.tam.deger;
                const char *kaynak_ad = NULL;
                int kaynak_uz = 0;
                switch (kt) {
                    case 1: kaynak_ad = "Dosya"; kaynak_uz = 5; break;
                    case 2: kaynak_ad = "Soket"; kaynak_uz = 5; break;
                    case 3: kaynak_ad = "Bellek"; kaynak_uz = 6; break;
                    case 4: kaynak_ad = "Donanim"; kaynak_uz = 7; break;
                    case 5: kaynak_ad = "OTP_Anahtar"; kaynak_uz = 11; break;
                    case 6: kaynak_ad = "MMIO"; kaynak_uz = 4; break;
                    default:
                        tip_hata(tk, arg0, "CP004",
                            "yetki_olustur: bilinmeyen kaynak tipi id");
                        return t_hata(tk);
                }
                /* Arg 2: izin tipi tamsayi */
                TipBilgisi *izin_t = tip_belirle(tk, d->veri.cagri.argumanlar[1]);
                if (izin_t->kategori != TIP_HATA && !tip_tamsayi_mi(izin_t)) {
                    tip_hata(tk, d->veri.cagri.argumanlar[1], "CP004",
                        "yetki_olustur izin argumani tamsayi olmali");
                }
                /* Kaynak adi arena'ya kopyala */
                char *ad_kopya = (char *)arena_ayir(tk->arena,
                                                    (size_t)kaynak_uz + 1);
                if (ad_kopya) {
                    memcpy(ad_kopya, kaynak_ad, (size_t)kaynak_uz);
                    ad_kopya[kaynak_uz] = '\0';
                }
                TipBilgisi *kaynak = tip_olustur_yapi(tk->arena,
                                                     ad_kopya, kaynak_uz,
                                                     NULL, 0);
                return tip_olustur_yetki(tk->arena, kaynak);
            }
            /* delege(y: yetki<R>, izin: tam16) -> yetki<R>
             * y *tüketilmez*; üretilen alt-yetki linear takip edilir. */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 6 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "delege", 6) == 0) {
                if (d->veri.cagri.sayi != 2) {
                    tip_hata(tk, d, "CP004",
                        "delege tam 2 arguman gerektirir (y, izin)");
                    return t_hata(tk);
                }
                TipBilgisi *y_tip = tip_belirle(tk, d->veri.cagri.argumanlar[0]);
                if (y_tip->kategori == TIP_HATA) return t_hata(tk);
                if (!tip_yetki_mi(y_tip)) {
                    tip_hata(tk, d->veri.cagri.argumanlar[0], "CP004",
                        "delege ilk argumani yetki<R> olmali");
                    return t_hata(tk);
                }
                TipBilgisi *izin_t = tip_belirle(tk, d->veri.cagri.argumanlar[1]);
                if (izin_t->kategori != TIP_HATA && !tip_tamsayi_mi(izin_t)) {
                    tip_hata(tk, d->veri.cagri.argumanlar[1], "CP004",
                        "delege izin argumani tamsayi olmali");
                }
                /* Yeni yetki uretilir; y tuketilmez (alt-yetki kavrami) */
                return y_tip;
            }
            /* geri_al(y: yetki<R>) -> bos — y tuketilir */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 7 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "geri_al", 7) == 0) {
                if (d->veri.cagri.sayi != 1) {
                    tip_hata(tk, d, "CP004",
                        "geri_al tam 1 arguman gerektirir (yetki<R>)");
                    return t_hata(tk);
                }
                TipBilgisi *y_tip = tip_belirle(tk, d->veri.cagri.argumanlar[0]);
                if (y_tip->kategori == TIP_HATA) return t_hata(tk);
                if (!tip_yetki_mi(y_tip)) {
                    tip_hata(tk, d->veri.cagri.argumanlar[0], "CP004",
                        "geri_al argumani yetki<R> olmali");
                    return t_hata(tk);
                }
                /* Linear tuketim */
                lineer_tuket_eger_baglamaysa(tk, d->veri.cagri.argumanlar[0]);
                return tip_olustur_basit(tk->arena, TIP_BOS);
            }
            /* v1 bölge-container: bölge_al beklenen-TIP_POINTER yolu
             * tip_belirle_beklenen'de; buraya dusmesi = beklenen *T
             * baglami YOK (annotasyonsuz) -> T cikarsanamaz. Sessiz
             * varsayilan YOK (dizi_olustur'un tam32 default'unun aksine
             * ham pointer'da yanlis genislik tehlikeli). */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 9 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "b\xc3\xb6lge_al", 9) == 0) {
                if (d->veri.cagri.sayi != 2) {
                    tip_hata(tk, d, "BL002",
                        "bolge_al tam 2 arguman gerektirir "
                        "(b\xc3\xb6l: yetki<R>, n: tam64)");
                    return t_hata(tk);
                }
                tip_hata(tk, d, "BL001",
                    "bolge_al beklenen *T baglami ister "
                    "(degisken v: *T = bolge_al(bol, n))");
                return t_hata(tk);
            }
            /* === MMIO Foundation intrinsics (Karar 1-4) ===
             * mmio_oku32(y: yetki<MMIO>, adres: tam64) -> tam32
             * mmio_yaz32(y: yetki<MMIO>, adres: tam64, deger: tam32) -> bos
             *
             * y ÖDÜNÇ alinir: delege/kanal_al gibi TÜKETİLMEZ. Surucu tek
             * yetkiyle bircok register'a erisir; tuketim yalniz geri_al ile.
             * Donus duz tam32 (Karar 4 — sonuc<> degil, WCET icin). */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 10 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "mmio_oku32", 10) == 0) {
                if (d->veri.cagri.sayi != 2) {
                    tip_hata(tk, d, "MM001",
                        "mmio_oku32 tam 2 arguman gerektirir "
                        "(y: yetki<MMIO>, adres: tam64)");
                    return t_hata(tk);
                }
                mmio_yetki_kontrol(tk, d->veri.cagri.argumanlar[0]);
                TipBilgisi *adr = tip_belirle_beklenen(tk,
                    d->veri.cagri.argumanlar[1],
                    tip_olustur_basit(tk->arena, TIP_TAM64));
                if (adr->kategori != TIP_HATA && !tip_tamsayi_mi(adr)) {
                    tip_hata(tk, d->veri.cagri.argumanlar[1], "MM003",
                        "mmio_oku32 adres argumani tamsayi (tam64) olmali");
                }
                /* y TÜKETİLMEZ (odunc) — return tam32 */
                return tip_olustur_basit(tk->arena, TIP_TAM32);
            }
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 10 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "mmio_yaz32", 10) == 0) {
                if (d->veri.cagri.sayi != 3) {
                    tip_hata(tk, d, "MM001",
                        "mmio_yaz32 tam 3 arguman gerektirir "
                        "(y: yetki<MMIO>, adres: tam64, deger: tam32)");
                    return t_hata(tk);
                }
                mmio_yetki_kontrol(tk, d->veri.cagri.argumanlar[0]);
                TipBilgisi *adr = tip_belirle_beklenen(tk,
                    d->veri.cagri.argumanlar[1],
                    tip_olustur_basit(tk->arena, TIP_TAM64));
                if (adr->kategori != TIP_HATA && !tip_tamsayi_mi(adr)) {
                    tip_hata(tk, d->veri.cagri.argumanlar[1], "MM003",
                        "mmio_yaz32 adres argumani tamsayi (tam64) olmali");
                }
                TipBilgisi *deg = tip_belirle_beklenen(tk,
                    d->veri.cagri.argumanlar[2],
                    tip_olustur_basit(tk->arena, TIP_TAM32));
                if (deg->kategori != TIP_HATA && !tip_tamsayi_mi(deg)) {
                    tip_hata(tk, d->veri.cagri.argumanlar[2], "MM003",
                        "mmio_yaz32 deger argumani tamsayi (tam32) olmali");
                }
                /* y TÜKETİLMEZ (odunc) — return bos */
                return tip_olustur_basit(tk->arena, TIP_BOS);
            }
            /* === MMIO typed-width varyantlari (C9) ===
             * Ayni 32-bit deseni; D9 ring-bellek erisimi icin le16/le64.
             * mmio_oku16(y, adres) -> tam16 ; mmio_yaz16(y, adres, deger: tam16)
             * mmio_oku64(y, adres) -> tam64 ; mmio_yaz64(y, adres, deger: tam64)
             * y ÖDÜNÇ alinir (tuketmez); adres tam64; donus duz tamN. */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 10 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "mmio_oku16", 10) == 0) {
                if (d->veri.cagri.sayi != 2) {
                    tip_hata(tk, d, "MM001",
                        "mmio_oku16 tam 2 arguman gerektirir "
                        "(y: yetki<MMIO>, adres: tam64)");
                    return t_hata(tk);
                }
                mmio_yetki_kontrol(tk, d->veri.cagri.argumanlar[0]);
                TipBilgisi *adr = tip_belirle_beklenen(tk,
                    d->veri.cagri.argumanlar[1],
                    tip_olustur_basit(tk->arena, TIP_TAM64));
                if (adr->kategori != TIP_HATA && !tip_tamsayi_mi(adr)) {
                    tip_hata(tk, d->veri.cagri.argumanlar[1], "MM003",
                        "mmio_oku16 adres argumani tamsayi (tam64) olmali");
                }
                return tip_olustur_basit(tk->arena, TIP_TAM16);
            }
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 10 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "mmio_yaz16", 10) == 0) {
                if (d->veri.cagri.sayi != 3) {
                    tip_hata(tk, d, "MM001",
                        "mmio_yaz16 tam 3 arguman gerektirir "
                        "(y: yetki<MMIO>, adres: tam64, deger: tam16)");
                    return t_hata(tk);
                }
                mmio_yetki_kontrol(tk, d->veri.cagri.argumanlar[0]);
                TipBilgisi *adr = tip_belirle_beklenen(tk,
                    d->veri.cagri.argumanlar[1],
                    tip_olustur_basit(tk->arena, TIP_TAM64));
                if (adr->kategori != TIP_HATA && !tip_tamsayi_mi(adr)) {
                    tip_hata(tk, d->veri.cagri.argumanlar[1], "MM003",
                        "mmio_yaz16 adres argumani tamsayi (tam64) olmali");
                }
                TipBilgisi *deg = tip_belirle_beklenen(tk,
                    d->veri.cagri.argumanlar[2],
                    tip_olustur_basit(tk->arena, TIP_TAM16));
                if (deg->kategori != TIP_HATA && !tip_tamsayi_mi(deg)) {
                    tip_hata(tk, d->veri.cagri.argumanlar[2], "MM003",
                        "mmio_yaz16 deger argumani tamsayi (tam16) olmali");
                }
                return tip_olustur_basit(tk->arena, TIP_BOS);
            }
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 10 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "mmio_oku64", 10) == 0) {
                if (d->veri.cagri.sayi != 2) {
                    tip_hata(tk, d, "MM001",
                        "mmio_oku64 tam 2 arguman gerektirir "
                        "(y: yetki<MMIO>, adres: tam64)");
                    return t_hata(tk);
                }
                mmio_yetki_kontrol(tk, d->veri.cagri.argumanlar[0]);
                TipBilgisi *adr = tip_belirle_beklenen(tk,
                    d->veri.cagri.argumanlar[1],
                    tip_olustur_basit(tk->arena, TIP_TAM64));
                if (adr->kategori != TIP_HATA && !tip_tamsayi_mi(adr)) {
                    tip_hata(tk, d->veri.cagri.argumanlar[1], "MM003",
                        "mmio_oku64 adres argumani tamsayi (tam64) olmali");
                }
                return tip_olustur_basit(tk->arena, TIP_TAM64);
            }
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 10 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "mmio_yaz64", 10) == 0) {
                if (d->veri.cagri.sayi != 3) {
                    tip_hata(tk, d, "MM001",
                        "mmio_yaz64 tam 3 arguman gerektirir "
                        "(y: yetki<MMIO>, adres: tam64, deger: tam64)");
                    return t_hata(tk);
                }
                mmio_yetki_kontrol(tk, d->veri.cagri.argumanlar[0]);
                TipBilgisi *adr = tip_belirle_beklenen(tk,
                    d->veri.cagri.argumanlar[1],
                    tip_olustur_basit(tk->arena, TIP_TAM64));
                if (adr->kategori != TIP_HATA && !tip_tamsayi_mi(adr)) {
                    tip_hata(tk, d->veri.cagri.argumanlar[1], "MM003",
                        "mmio_yaz64 adres argumani tamsayi (tam64) olmali");
                }
                TipBilgisi *deg = tip_belirle_beklenen(tk,
                    d->veri.cagri.argumanlar[2],
                    tip_olustur_basit(tk->arena, TIP_TAM64));
                if (deg->kategori != TIP_HATA && !tip_tamsayi_mi(deg)) {
                    tip_hata(tk, d->veri.cagri.argumanlar[2], "MM003",
                        "mmio_yaz64 deger argumani tamsayi (tam64) olmali");
                }
                return tip_olustur_basit(tk->arena, TIP_BOS);
            }
            /* === Concurrency / DRF V1 intrinsics === */
            /* görev_başlat(c: işlev() -> T) -> görev<T>
             * c yakaladığı lineer değerleri t_yeni'ye transfer eder (DRF-L2).
             * V1'de c bir lambda (DUGUM_LAMBDA) veya değişken (linear closure) olur. */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 14 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "g\xc3\xb6rev_ba\xc5\x9f" "lat", 14) == 0) {
                if (d->veri.cagri.sayi != 1) {
                    tip_hata(tk, d, "DRF001",
                        "gorev_baslat tam 1 arguman gerektirir "
                        "(closure: islev() -> T)");
                    return t_hata(tk);
                }
                TipBilgisi *c_tip = tip_belirle(tk, d->veri.cagri.argumanlar[0]);
                if (c_tip->kategori == TIP_HATA) return t_hata(tk);
                /* c bir islev tipi olmali (tekkez<islev(...)> da olur — LC-2) */
                const TipBilgisi *cf = c_tip;
                if (cf->kategori == TIP_TEKKEZ) cf = cf->veri.tekkez.ic;
                if (cf->kategori != TIP_ISLEV) {
                    tip_hata(tk, d->veri.cagri.argumanlar[0], "DRF001",
                        "gorev_baslat argumani islev() -> T tipinde olmali");
                    return t_hata(tk);
                }
                if (cf->veri.islev.param_sayi != 0) {
                    tip_hata(tk, d->veri.cagri.argumanlar[0], "DRF001",
                        "gorev_baslat closure'i parametresiz olmali (() -> T)");
                    return t_hata(tk);
                }
                /* Linear closure ise (LC-3 ile beraber thread'e transfer) — tuket */
                lineer_tuket_eger_baglamaysa(tk, d->veri.cagri.argumanlar[0]);
                /* görev<T> dön — T = closure dönüş tipi */
                TipBilgisi *donus = cf->veri.islev.donus;
                if (!donus) donus = tip_olustur_basit(tk->arena, TIP_BOS);
                /* D-294 V1 SINIRI: T KESİRLİ olamaz. Runtime görev sonucunu
                 * tamsayı-dönüşlü bir fn-ptr ile alır (x0/rax); float dönüş
                 * v0/xmm0'da gelir → değer SESSİZCE çöp olurdu. Reddetmek
                 * yerine bitcast'lamak hata modunu loud→silent'a çevirirdi.
                 * Kapasite kaybı YOK: görev<kesirli*> zaten hiç derlenmiyordu
                 * (LLVM tip hatası); kazanç, düzgün bir KEMGU tanısı. */
                if (donus->kategori == TIP_KESIRLI32 ||
                    donus->kategori == TIP_KESIRLI64) {
                    tip_hata(tk, d->veri.cagri.argumanlar[0], "DRF001",
                        "gorev<T> V1'de kesirli T desteklemiyor "
                        "(kesirli32/64) — closure tamsayi/isaretci donmeli");
                    return t_hata(tk);
                }
                /* Karar 1 (D-30x): görev_başlat DÖNÜŞÜ artık
                 * sonuç<görev<T>, metin> — spawn (kaynak tükenmesi / thread'siz
                 * platform) başarısız olabilir. Panik yerine başarısızlık
                 * çağırana bir DEĞER olarak sunulur (çökmezlik). Hata tipi V1'de
                 * metin (payload'lı çeşit gelince GörevHata'ya yükseltilebilir).
                 * Çağıran `eşleş` ile açar; tamam(g) → görev<T>, hata(e) → metin. */
                TipBilgisi *gorev_tip = tip_olustur_gorev(tk->arena, donus);
                TipBilgisi *hata_tip = tip_olustur_basit(tk->arena, TIP_METIN);
                return tip_olustur_sonuc(tk->arena, gorev_tip, hata_tip);
            }
            /* kanal_oluştur(kapasite) -> kanal<T> — T YALNIZ beklenen tipten
             * gelir (tip_belirle_beklenen'deki DUGUM_CAGRI dalı). Buraya
             * düşmek "beklenen tip yok" demektir → T çıkarsanamaz. Sessizce
             * bir T uydurmak yerine DRF006 ile açıkça reddet. */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 14 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "kanal_olu\xc5\x9ftur", 14) == 0) {
                if (d->veri.cagri.sayi != 1) {
                    tip_hata(tk, d, "DRF006",
                        "kanal_olustur tam 1 arguman gerektirir "
                        "(kapasite: tam32)");
                    return t_hata(tk);
                }
                tip_hata(tk, d, "DRF006",
                    "kanal_olustur'un eleman tipi baglamdan cikarsanmali "
                    "(ornek: degisken k: kanal<tam32> = kanal_olustur(8))");
                return t_hata(tk);
            }
            /* görev_birleştir(g: görev<T>) -> T — g tuketilir (R-BİRLEŞTİR) */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 17 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "g\xc3\xb6rev_birle\xc5\x9f" "tir", 17) == 0) {
                if (d->veri.cagri.sayi != 1) {
                    tip_hata(tk, d, "DRF002",
                        "gorev_birlestir tam 1 arguman gerektirir (gorev<T>)");
                    return t_hata(tk);
                }
                TipBilgisi *g_tip = tip_belirle(tk, d->veri.cagri.argumanlar[0]);
                if (g_tip->kategori == TIP_HATA) return t_hata(tk);
                if (!tip_gorev_mu(g_tip)) {
                    tip_hata(tk, d->veri.cagri.argumanlar[0], "DRF002",
                        "gorev_birlestir argumani gorev<T> olmali");
                    return t_hata(tk);
                }
                /* Linear tuketim — g birleştirildikten sonra erişilemez */
                lineer_tuket_eger_baglamaysa(tk, d->veri.cagri.argumanlar[0]);
                /* T'yi dön */
                return g_tip->veri.gorev.ic;
            }
            /* kanal_gönder(k: kanal<T>, v: T) -> bos — v tuketilir (DRF-L5) */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 13 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "kanal_g\xc3\xb6nder", 13) == 0) {
                if (d->veri.cagri.sayi != 2) {
                    tip_hata(tk, d, "DRF003",
                        "kanal_gonder tam 2 arguman gerektirir (kanal<T>, v: T)");
                    return t_hata(tk);
                }
                TipBilgisi *k_tip = tip_belirle(tk, d->veri.cagri.argumanlar[0]);
                if (k_tip->kategori == TIP_HATA) return t_hata(tk);
                if (!tip_kanal_mu(k_tip)) {
                    tip_hata(tk, d->veri.cagri.argumanlar[0], "DRF003",
                        "kanal_gonder ilk argumani kanal<T> ya da gonderen<T> olmali");
                    return t_hata(tk);
                }
                /* D-303 yön güvenliği: alan<T> ucundan GÖNDERİLEMEZ. çift
                 * (kanal<T>) ve gönderen<T> kabul edilir. */
                if (k_tip->veri.kanal.yon == KANAL_YON_ALAN) {
                    tip_hata(tk, d->veri.cagri.argumanlar[0], "DRF007",
                        "alan<T> (alici ucu) uzerinden kanal_gonder yapilamaz "
                        "— gonderen<T> ya da kanal<T> gerekir");
                    return t_hata(tk);
                }
                TipBilgisi *t_tip = k_tip->veri.kanal.ic;
                TipBilgisi *v_tip = tip_belirle_beklenen(tk,
                    d->veri.cagri.argumanlar[1], t_tip);
                if (v_tip->kategori != TIP_HATA && !tip_esit(v_tip, t_tip)) {
                    tip_hata(tk, d->veri.cagri.argumanlar[1], "DRF003",
                        "kanal_gonder v argumani kanal eleman tipinde olmali");
                }
                /* v tuketilir (lineer ise) — k tuketilmez (kanal yeniden kullanilir) */
                lineer_tuket_eger_baglamaysa(tk, d->veri.cagri.argumanlar[1]);
                return tip_olustur_basit(tk->arena, TIP_BOS);
            }
            /* kanal_al(k: kanal<T>) -> T — k tuketilmez */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 8 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "kanal_al", 8) == 0) {
                if (d->veri.cagri.sayi != 1) {
                    tip_hata(tk, d, "DRF004",
                        "kanal_al tam 1 arguman gerektirir (kanal<T>)");
                    return t_hata(tk);
                }
                TipBilgisi *k_tip = tip_belirle(tk, d->veri.cagri.argumanlar[0]);
                if (k_tip->kategori == TIP_HATA) return t_hata(tk);
                if (!tip_kanal_mu(k_tip)) {
                    tip_hata(tk, d->veri.cagri.argumanlar[0], "DRF004",
                        "kanal_al argumani kanal<T> ya da alan<T> olmali");
                    return t_hata(tk);
                }
                /* D-303 yön güvenliği: gönderen<T> ucundan ALINAMAZ. */
                if (k_tip->veri.kanal.yon == KANAL_YON_GONDEREN) {
                    tip_hata(tk, d->veri.cagri.argumanlar[0], "DRF007",
                        "gonderen<T> (gonderici ucu) uzerinden kanal_al yapilamaz "
                        "— alan<T> ya da kanal<T> gerekir");
                    return t_hata(tk);
                }
                /* T'yi dön — k tuketilmez */
                return k_tip->veri.kanal.ic;
            }
            /* D-303: gönderen(k) / alan(k) — kanal<T> fabrikasından yön'lü uç
             * projeksiyonu (runtime-free; codegen'de identity, dondur gibi).
             * Uçlar aynı runtime kanalına type-level görünümlerdir. */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                ((d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 9 &&
                  memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                         "g\xc3\xb6nderen", 9) == 0) ||
                 (d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 4 &&
                  memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                         "alan", 4) == 0))) {
                int istenen = (d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 9)
                    ? KANAL_YON_GONDEREN : KANAL_YON_ALAN;
                /* DÜŞÜŞE-GÜVENLİ: yalnız (tam 1 arg VE arg kanal<T>) ise
                 * projeksiyon claim et. Değilse kullanıcının `alan`/`gönderen`
                 * adlı işlevine düş (`alan` = "alan/bölge" yaygın tanımlayıcı;
                 * built-in adı ONU gasp ETMEMELİ). */
                if (d->veri.cagri.sayi == 1) {
                    TipBilgisi *k_tip =
                        tip_belirle(tk, d->veri.cagri.argumanlar[0]);
                    if (tip_kanal_mu(k_tip)) {
                        return tip_olustur_kanal_yon(tk->arena,
                            k_tip->veri.kanal.ic, istenen);
                    }
                }
                /* düş: normal işlev çağrısı çözümü aşağıda */
            }
            /* dondur(v: &değişken T) -> &T — mutable referansi immutable yapar
             * (R-PAYLAŞ — Plan Karar E hibrit: built-in + frozen flag V2'de) */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 6 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "dondur", 6) == 0) {
                if (d->veri.cagri.sayi != 1) {
                    tip_hata(tk, d, "DRF005",
                        "dondur tam 1 arguman gerektirir (&degisken T)");
                    return t_hata(tk);
                }
                TipBilgisi *v_tip = tip_belirle(tk, d->veri.cagri.argumanlar[0]);
                if (v_tip->kategori == TIP_HATA) return t_hata(tk);
                if (v_tip->kategori != TIP_REFERANS ||
                    !v_tip->veri.referans.degisken_mi) {
                    tip_hata(tk, d->veri.cagri.argumanlar[0], "DRF005",
                        "dondur argumani &degisken T olmali");
                    return t_hata(tk);
                }
                /* Immutable referans dön */
                return tip_olustur_referans(tk->arena,
                                            v_tip->veri.referans.hedef, 0);
            }
            /* Method dispatch: hedef DUGUM_ERISIM ise (x.method())
             * x'in yapi tipi uzerinde method bul. */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_ERISIM) {
                const Dugum *erisim = d->veri.cagri.hedef;
                TipBilgisi *nesne_tip = tip_belirle(tk, erisim->veri.erisim.nesne);
                if (nesne_tip->kategori == TIP_REFERANS) {
                    nesne_tip = nesne_tip->veri.referans.hedef;
                }
                if (nesne_tip->kategori == TIP_YAPI) {
                    const Dugum *m = uygula_tablosu_method_bul(
                        &tk->uygulamalar,
                        nesne_tip->veri.yapi.ad,
                        nesne_tip->veri.yapi.ad_uzunluk,
                        erisim->veri.erisim.alan,
                        erisim->veri.erisim.alan_uzunluk);
                    if (m) {
                        /* Method bulundu — kendin parametresi otomatik (receiver)
                         * Aksi halde normal arg sayisi kontrolu */
                        int n = m->veri.islev.param_sayi;
                        int has_kendin = (n > 0 &&
                            m->veri.islev.parametreler[0]->veri.parametre.kendin_mi);
                        int beklenen_arg = has_kendin ? n - 1 : n;
                        if (d->veri.cagri.sayi != beklenen_arg) {
                            tip_hata(tk, d, "T010",
                                "method cagri arguman sayisi uyumsuz");
                            return t_hata(tk);
                        }
                        int offset = has_kendin ? 1 : 0;
                        for (int i = 0; i < beklenen_arg; i++) {
                            const Dugum *p = m->veri.islev.parametreler[i + offset];
                            TipBilgisi *pt = ast_tip_to_bilgi(tk,
                                p->veri.parametre.tip);
                            TipBilgisi *at = tip_belirle_beklenen(tk,
                                d->veri.cagri.argumanlar[i], pt);
                            if (!tip_esit(at, pt) &&
                                at->kategori != TIP_HATA) {
                                tip_hata(tk, d->veri.cagri.argumanlar[i],
                                    "T001", "method arg tipi uyumsuz");
                            }
                            /* DZ.3 akis + DZ006 — METHOD yolu. Bu blok
                             * olmadan `k.buyut(W)` DZ006'yi ATLIYORDU
                             * (olculdu: hatasiz geciyordu = soundness acigi);
                             * method dispatch normal cagri yolundan ONCE
                             * return ettigi icin oradaki kontroller
                             * calismiyor. `kendin` offset'i dikkate alinir. */
                            dz_akis_kontrol(tk, d->veri.cagri.argumanlar[i],
                                            pt, at);
                            if (at && at->kategori == TIP_DIZI &&
                                at->veri.dizi.uzunluk > 0) {
                                DzBuyutucu *mb = dz_kayit_bul(tk, m);
                                int pi = i + offset;
                                if (mb && pi < mb->param_sayi &&
                                    mb->bayrak[pi]) {
                                    char msj[224];
                                    snprintf(msj, sizeof(msj),
                                        "Dizi<T, %d> argumani buyutucu "
                                        "parametreye verilemez: '%.*s' %d. "
                                        "parametresini dizi_ekle/"
                                        "dizi_kapasite_ayarla ile buyutuyor "
                                        "(buyutme cagirana gorunur → N yalan "
                                        "olurdu)",
                                        at->veri.dizi.uzunluk,
                                        mb->ad_uz, mb->ad, pi + 1);
                                    tip_hata(tk,
                                        d->veri.cagri.argumanlar[i],
                                        "DZ006", msj);
                                }
                            }
                        }
                        if (m->veri.islev.donus_tipi) {
                            return ast_tip_to_bilgi(tk, m->veri.islev.donus_tipi);
                        }
                        return tip_olustur_basit(tk->arena, TIP_BOS);
                    }
                    /* Method bulunamadi — duser asagi normal alan erisim
                     * yoluna (DUGUM_ERISIM tip_belirle), oradan hata gelir. */
                }
            }
            TipBilgisi *hedef_tip = tip_belirle(tk, d->veri.cagri.hedef);
            if (hedef_tip->kategori == TIP_HATA) return t_hata(tk);

            /* Linear Types Spec V1 LC-3: tekkez<islev(...)> cagri = consume.
             * Hedef DUGUM_TANIMLAYICI ise sembolu tuket. */
            if (hedef_tip->kategori == TIP_TEKKEZ &&
                hedef_tip->veri.tekkez.ic &&
                hedef_tip->veri.tekkez.ic->kategori == TIP_ISLEV) {
                lineer_tuket_eger_baglamaysa(tk, d->veri.cagri.hedef);
                hedef_tip = hedef_tip->veri.tekkez.ic;
            }

            if (hedef_tip->kategori != TIP_ISLEV) {
                tip_hata(tk, d, "T006", "cagri icin islev tipi gerek");
                return t_hata(tk);
            }
            if (d->veri.cagri.sayi != hedef_tip->veri.islev.param_sayi) {
                tip_hata(tk, d, "T010", "cagri arguman sayisi uyumsuz");
                return t_hata(tk);
            }
            /* D-257 çıplak-call-rule: çıplak fn (ρ-suz C-ABI) yalnız çıplak/extern
             * çağırır. Normal (ρ-alan) user-fn çağrısı → verilecek ρ yok → codegen
             * `ptr null` geçer → callee null-region'a tahsis → segfault. Statik reddet.
             * Built-in/extern (ast_dugumu yok/ISLEV değil) ρ almaz → izinli. */
            if (tk->ciplak_baglam > 0 &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI) {
                const Sembol *cs = sembol_bul(tk->scope,
                    d->veri.cagri.hedef->veri.tanimlayici.metin,
                    d->veri.cagri.hedef->veri.tanimlayici.uzunluk);
                if (cs && cs->ast_dugumu &&
                    cs->ast_dugumu->tip == DUGUM_ISLEV &&
                    !cs->ast_dugumu->veri.islev.ciplak_mi) {
                    tip_hata(tk, d, "E013",
                        "\xc3\xa7\xc4\xb1plak i\xc5\x9flev yaln\xc4\xb1z \xc3\xa7\xc4\xb1plak/extern "
                        "\xc3\xa7" "a\xc4\x9f\xc4\xb1rabilir (\xcf\x81-suz C-ABI)");
                }
            }
            /* Madde D: Multi-param + compound type generic inference.
             * GenBaglamalar ile her arg/param ciftinde unify, donus
             * tipini substitue et. */
            GenBaglamalar gb;
            gb.sayi = 0;
            /* Once unify arg tipleri ile (substitue olmadan) — daha sonra
             * argumanlar substitue edilmis param tipi context'inde tekrar
             * cikarsanir. Iki pas: pas 1 inference, pas 2 type check. */
            tk->lineer_sondaj++;    /* D-320: pas 1 = SONDAJ, tuketim sayilmaz */
            for (int i = 0; i < d->veri.cagri.sayi; i++) {
                TipBilgisi *param_tip = hedef_tip->veri.islev.parametreler[i];
                TipBilgisi *arg_tip = tip_belirle(tk,
                    d->veri.cagri.argumanlar[i]);
                gen_unify(&gb, param_tip, arg_tip);
            }
            tk->lineer_sondaj--;
            /* Pas 2: arg tipini substitue edilmis param tipi context'inde
             * cikarsama + tip kontrolu */
            for (int i = 0; i < d->veri.cagri.sayi; i++) {
                TipBilgisi *param_tip = hedef_tip->veri.islev.parametreler[i];
                TipBilgisi *bek = gen_substitue(tk, param_tip, &gb);
                TipBilgisi *arg_tip = tip_belirle_beklenen(tk,
                    d->veri.cagri.argumanlar[i], bek);
                if (param_tip->kategori == TIP_GENERIC_PARAM) {
                    /* Concrete'i gb'de tutuyoruz; bind onceden yapildi */
                    continue;
                }
                /* Sabitsüre Spec V1 CT003: arg sabitsure<T> → param T leak.
                 * Önce bunu kontrol et (T001'den önce); ifsa(...) gerek. */
                if (arg_tip->kategori != TIP_HATA &&
                    param_tip->kategori != TIP_HATA &&
                    ct003_leak_kontrol(tk, d->veri.cagri.argumanlar[i],
                                        arg_tip, param_tip)) {
                    /* hata raporlandı */
                } else if (!tip_esit(arg_tip, bek) &&
                    arg_tip->kategori != TIP_HATA) {
                    tip_hata(tk, d->veri.cagri.argumanlar[i], "T001",
                             "arguman tipi parametre tipi ile uyumsuz");
                }
                /* DZ.3: parametre N-bilinen ise arguman da olmali */
                dz_akis_kontrol(tk, d->veri.cagri.argumanlar[i],
                                param_tip, arg_tip);
                /* === DZ006 (Asama b): N-bilinen arguman, BUYUTUCU parametreye
                 * gecemez. Diziler referansla gecer → callee'nin buyutmesi
                 * cagirana gorunur ve cagirandaki N'i YALANLAR (DZ.5). */
                if (arg_tip && arg_tip->kategori == TIP_DIZI &&
                    arg_tip->veri.dizi.uzunluk > 0) {
                    /* Cagri hedefini coz. `cozum_sembol` ONCE denenir: hem
                     * `f(...)` (TANIMLAYICI) hem `m::f(...)` (YOL) icin
                     * doludur. Yalniz sembol_bul'a bakmak modul-nitelikli
                     * cagriyi ATLIYORDU → DZ006 sessizce dusuyordu (olculdu:
                     * `m::mbuyut(W)` hatasiz geciyordu = soundness acigi). */
                    const Dugum *hd = d->veri.cagri.hedef;
                    const Sembol *fs = hd->cozum_sembol;
                    if (!fs && hd->tip == DUGUM_TANIMLAYICI) {
                        fs = sembol_bul(tk->scope,
                                hd->veri.tanimlayici.metin,
                                hd->veri.tanimlayici.uzunluk);
                    }
                    DzBuyutucu *bk = (fs && fs->ast_dugumu)
                        ? dz_kayit_bul(tk, fs->ast_dugumu) : NULL;
                    if (bk && i < bk->param_sayi && bk->bayrak[i]) {
                        char msj[224];
                        snprintf(msj, sizeof(msj),
                            "Dizi<T, %d> argumani buyutucu parametreye "
                            "verilemez: '%.*s' %d. parametresini "
                            "dizi_ekle/dizi_kapasite_ayarla ile buyutuyor "
                            "(buyutme cagirana gorunur → N yalan olurdu)",
                            arg_tip->veri.dizi.uzunluk,
                            bk->ad_uz, bk->ad, i + 1);
                        tip_hata(tk, d->veri.cagri.argumanlar[i],
                                 "DZ006", msj);
                    }
                }
                /* Linear Types Spec V1 + Capability Spec V1:
                 * param lineer (tekkez veya yetki) ise arg consume */
                if (param_tip && tip_lineer_mi(param_tip)) {
                    lineer_tuket_eger_baglamaysa(tk,
                        d->veri.cagri.argumanlar[i]);
                }
                /* D-070 (Sınıf A, değişken-arg): stack-array DEĞİŞKENİ Dizi<T>
                 * parametresine geçirilemez (G003). `değişken xs = [..]`
                 * (annotasyonsuz) STACK [N x T] üretir; Dizi<T> param dinamik
                 * KdlDizi* bekler → callee KdlDizi sanıp stack-array okur →
                 * misaligned UB/SEGFAULT. Compile-time reddet (çökmezlik #1);
                 * programcı annotasyonlu heap Dizi kullansın. (Literal-arg
                 * D-070 codegen'de heap'e route edilir — bu kural değişken yolu.) */
                if (param_tip && param_tip->kategori == TIP_DIZI) {
                    const Dugum *arg = d->veri.cagri.argumanlar[i];
                    if (arg && arg->tip == DUGUM_TANIMLAYICI) {
                        const Sembol *as = sembol_bul(tk->scope,
                            arg->veri.tanimlayici.metin,
                            arg->veri.tanimlayici.uzunluk);
                        if (as && as->kategori == SEMBOL_DEGISKEN &&
                            as->ast_dugumu &&
                            as->ast_dugumu->tip == DUGUM_DEGISKEN &&
                            as->ast_dugumu->veri.degisken.tip == NULL &&
                            as->ast_dugumu->veri.degisken.deger &&
                            as->ast_dugumu->veri.degisken.deger->tip ==
                                DUGUM_DIZI_OLUSTUR) {
                            tip_hata(tk, arg, "G003",
                                "stack dizi degiskeni Dizi<T> parametresine "
                                "gecirilemez (annotasyonlu heap Dizi kullanin: "
                                "degisken xs: Dizi<T> = [..])");
                        }
                    }
                }
            }
            /* === Adim 5: Bound-aware monomorphization check ===
             * hedef bir islev tanimlayicisi ise, tip_param_boundlari kontrol
             * et: her generic T'nin concrete tipi her bound (ozellik) icin
             * uygula tablosu kanitlamali. */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI) {
                const Sembol *fn_s = sembol_bul(tk->scope,
                    d->veri.cagri.hedef->veri.tanimlayici.metin,
                    d->veri.cagri.hedef->veri.tanimlayici.uzunluk);
                if (fn_s && fn_s->kategori == SEMBOL_ISLEV &&
                    fn_s->ast_dugumu &&
                    fn_s->ast_dugumu->tip == DUGUM_ISLEV) {
                    const Dugum *islev = fn_s->ast_dugumu;
                    int tps = islev->veri.islev.tip_param_sayi;
                    if (tps > 0 && islev->veri.islev.tip_param_bound_sayilari) {
                        for (int pi = 0; pi < tps; pi++) {
                            int bs = islev->veri.islev.tip_param_bound_sayilari[pi];
                            if (bs == 0) continue;
                            const char *tp_ad = islev->veri.islev.tip_paramlar[pi];
                            int tp_uz = (int)strlen(tp_ad);
                            const TipBilgisi *concrete = gen_bul(&gb,
                                tp_ad, tp_uz);
                            if (!concrete) continue;  /* Inferred degil — abstract */
                            if (concrete->kategori == TIP_GENERIC_PARAM) continue;
                            const char *arg_ad = NULL;
                            int arg_uz = 0;
                            if (concrete->kategori == TIP_YAPI) {
                                arg_ad = concrete->veri.yapi.ad;
                                arg_uz = concrete->veri.yapi.ad_uzunluk;
                            }
                            if (!arg_ad) continue;  /* Built-in tip — bound check yok */
                            for (int bi = 0; bi < bs; bi++) {
                                const Dugum *bd =
                                    islev->veri.islev.tip_param_boundlari[pi][bi];
                                int bd_uz = 0;
                                const char *bd_ad = tip_dugumu_kok_adi(bd, &bd_uz);
                                if (!bd_ad) continue;
                                const Sembol *oz_s = sembol_bul(tk->global_scope,
                                                                 bd_ad, bd_uz);
                                if (!oz_s || oz_s->kategori != SEMBOL_OZELLIK) {
                                    tip_hata(tk, d, "T031",
                                        "bilinmeyen ozellik (islev generic bound)");
                                    continue;
                                }
                                if (!uygula_tablosu_implementations_eder(
                                        &tk->uygulamalar,
                                        arg_ad, arg_uz, bd_ad, bd_uz)) {
                                    tip_hata(tk, d, "T030",
                                        "tip argumani islev bound'unu "
                                        "karsilamiyor (uygula bildirimi yok)");
                                }
                            }
                        }
                    }
                }
            }

            /* Donus tipi — generic param compound olabilir, substitue et */
            TipBilgisi *donus = hedef_tip->veri.islev.donus;
            return gen_substitue(tk, donus, &gb);
        }

        /* === Erisim (x.y) === */
        case DUGUM_ERISIM: {
            TipBilgisi *nesne_tip = tip_belirle(tk, d->veri.erisim.nesne);
            if (nesne_tip->kategori == TIP_HATA) return t_hata(tk);

            /* Referansi otomatik dereference et */
            if (nesne_tip->kategori == TIP_REFERANS) {
                nesne_tip = nesne_tip->veri.referans.hedef;
            }

            if (nesne_tip->kategori != TIP_YAPI) {
                tip_hata(tk, d, "T007", "alan erisimi yapi tipi gerek");
                return t_hata(tk);
            }
            const Sembol *yapi_sem = yapi_sembol_capraz_bul(tk,
                nesne_tip->veri.yapi.ad, nesne_tip->veri.yapi.ad_uzunluk);
            if (!yapi_sem || yapi_sem->kategori != SEMBOL_YAPI) {
                tip_hata(tk, d, "T002", "yapi tanimi bulunamadi");
                return t_hata(tk);
            }
            const Sembol *alan = sembol_yapi_alani(yapi_sem,
                d->veri.erisim.alan, d->veri.erisim.alan_uzunluk);
            if (!alan) {
                tip_hata(tk, d, "T009", "alan bulunamadi");
                return t_hata(tk);
            }
            /* D-313 (Linear V2) — KISMİ TAŞIMA YASAĞI: lineer bir yapının
             * LINEER alanını dışarı okumak UNSOUND olurdu. Okunan değer kendi
             * başına tüketilmesi gereken bir lineer bağlama olur; yapı da hâlâ
             * tüketilmek zorundadır → AYNI kaynak İKİ kez tüketilir (çift imha).
             * Ne yapılabilir: yapıyı bütün olarak `imha` et ya da taşı
             * (arg/`ver`). Alan-bazlı taşıma (partial move) V2.1 işi ve kendi
             * kodunu ister — burada YENİ KOD İCAT EDİLMEDİ, LR002 ailesi
             * mesaj ayrımıyla kullanıldı (adlandırma Mehmet'in kararı).
             * NOT: lineer yapının LINEER-OLMAYAN alanını okumak SERBEST (kopya
             * değer; kaynak sahipliği etkilenmez). */
            if (alan->tip && tip_lineer_mi(alan->tip)
                && nesne_tip->veri.yapi.lineer_mi) {
                /* D-315 (V2.1) — KISMI TASIMA: alan disari TASINIR (move).
                 * D-313'te bu tumden reddediliyordu cunku alan-bazli sahiplik
                 * izlenmiyordu; artik baglama basina bit-maske tutuluyor:
                 *   - ilk okuma  -> alani "tasindi" isaretle, alan tipini don
                 *     (donen deger kendi basina lineer -> L001/L002 makinesi
                 *     onu ayrica takip eder),
                 *   - ikinci okuma -> L002 (ayni alan iki kez tasinamaz).
                 * Yapinin KENDISI hala tuketilmelidir (imha) -> kabuk sizmaz. */
                /* D-320: sondaj ziyaretinde maske GUNCELLENMEZ (ve hata
                 * uretilmez) — ayni erisim pas 2'de tekrar gorulur; burada
                 * isaretlemek `f(k.a)` gibi TEK tasimayi iki kez saydirirdi. */
                if (tk->lineer_sondaj > 0) return alan->tip;
                int alan_ix = yapi_alan_indeksi_sirali(yapi_sem, alan);
                Sembol *bagl = erisim_baglama_sembolu(tk, d->veri.erisim.nesne);
                if (alan_ix < 0 || alan_ix >= 32 || !bagl) {
                    /* Cozulemedi (gecici deger / 32+ alan) -> KANITLANAMADI:
                     * muhafazakar reddet (D-313 davranisi). */
                    tip_hata(tk, d, "LR002",
                        "lineer alan yalniz bir BAGLAMA uzerinden tasinabilir "
                        "(gecici deger veya 32+ alanli yapi desteklenmiyor)");
                    return t_hata(tk);
                }
                unsigned int bit = 1u << (unsigned)alan_ix;
                if (bagl->lineer_alan_maskesi & bit) {
                    tip_hata(tk, d, "L002",
                        "lineer alan zaten disari tasindi (kismi tasima sonrasi "
                        "yeniden erisim)");
                    return t_hata(tk);
                }
                if (bagl->lineer_tuketildi >= 1) {
                    tip_hata(tk, d, "L002",
                        "yapi zaten tuketildi (tasima sonrasi alan erisimi)");
                    return t_hata(tk);
                }
                bagl->lineer_alan_maskesi |= bit;
                return alan->tip;
            }
            /* Generic instantiation: yapi.tip_arg varsa alan tipinde
             * substitusyon (T -> tam32 vs) */
            if (alan->tip && nesne_tip->veri.yapi.tip_arg_sayi > 0) {
                return substitusyon(tk, alan->tip, yapi_sem, nesne_tip);
            }
            return alan->tip ? alan->tip : t_hata(tk);
        }

        /* === Indeks (x[i]) === */
        case DUGUM_INDEKS: {
            TipBilgisi *nesne_tip = tip_belirle(tk, d->veri.indeks.nesne);
            TipBilgisi *idx_tip = tip_belirle(tk, d->veri.indeks.indeks);
            if (nesne_tip->kategori == TIP_HATA ||
                idx_tip->kategori == TIP_HATA) return t_hata(tk);

            /* Referansi otomatik dereference et */
            if (nesne_tip->kategori == TIP_REFERANS) {
                nesne_tip = nesne_tip->veri.referans.hedef;
            }
            /* Sabitsüre Spec V1: dizi de sabitsüre olabilir (sabitsure<Dizi<T>>) */
            int dizi_sabitsure = tip_sabitsure_mi(nesne_tip);
            if (dizi_sabitsure) nesne_tip = nesne_tip->veri.sabitsure.ic;

            /* v1 bölge-container (T008 gevsetme): *T ham pointer tabani
             * da indekslenebilir — eleman tipi pointee. *p deref'le ayni
             * guvenlik sinifi: YALNIZ guvensiz blokta (G001 tutarliligi).
             * TIP_DIZI yolu AYNEN korunur. */
            if (nesne_tip->kategori == TIP_POINTER) {
                if (!tip_tamsayi_mi(idx_tip)) {
                    tip_hata(tk, d, "T005", "indeks tamsayi olmali");
                    return t_hata(tk);
                }
                if (tk->guvensiz_baglam == 0) {
                    tip_hata(tk, d, "G001",
                             "*T pointer indeksleme yalniz guvensiz blok "
                             "icinde kullanilabilir");
                    return t_hata(tk);
                }
                return nesne_tip->veri.pointer.hedef
                       ? nesne_tip->veri.pointer.hedef : t_hata(tk);
            }
            if (nesne_tip->kategori != TIP_DIZI) {
                tip_hata(tk, d, "T008", "indeksleme dizi tipi gerek");
                return t_hata(tk);
            }
            if (!tip_tamsayi_mi(idx_tip)) {
                tip_hata(tk, d, "T005", "indeks tamsayi olmali");
                return t_hata(tk);
            }
            /* Sabitsüre Spec V1 CT002 SABITSURE_INDEX:
             * indeks sabitsüre olamaz — cache-line granülaritesinde gizli
             * bilgiyi sızdırır (Bernstein 2005 AES T-table saldırısı). */
            if (tip_sabitsure_mi(idx_tip)) {
                tip_hata(tk, d, "CT002",
                    "sabitsure tipinde dizi indeksi yasak "
                    "(cache-timing yan kanali — ifsa(idx) kullanin)");
                return t_hata(tk);
            }
            /* === DZ002: sabit indeks N sinirinin disinda ===
             * Yalniz indeks bir TAMSAYI LITERALI ve N biliniyorken uygulanir.
             * Degisken indeks (W[i]) V1'de statik denetlenmez — `i < N` ispati
             * bagimli/refinement tipler ister (DZ.9). Runtime sinir kontrolu
             * her hâlükârda yerinde (DZ.2). */
            if (nesne_tip->veri.dizi.uzunluk > 0 &&
                d->veri.indeks.indeks &&
                d->veri.indeks.indeks->tip == DUGUM_TAM) {
                long long sabit_i =
                    (long long)d->veri.indeks.indeks->veri.tam.deger;
                int nn = nesne_tip->veri.dizi.uzunluk;
                if (sabit_i < 0 || sabit_i >= (long long)nn) {
                    char msj[128];
                    snprintf(msj, sizeof(msj),
                        "sabit indeks %lld, sinir 0..%d", sabit_i, nn - 1);
                    tip_hata(tk, d, "DZ002", msj);
                }
            }
            /* Sabitsüre dizi içeren eleman tipi — sabitsüre kalır (taint) */
            TipBilgisi *elem = nesne_tip->veri.dizi.eleman;
            if (dizi_sabitsure && !tip_sabitsure_mi(elem)) {
                return tip_olustur_sabitsure(tk->arena, elem);
            }
            return elem;
        }

        /* === Yol (x::y) === */
        case DUGUM_YOL: {
            const Dugum *sol = d->veri.yol.sol;
            /* C2.7: Cesit::Varyant — sol bir çeşit ise varyant değeri.
             * T016 fix: sol modul-nitelikli olabilir (g::Renk::Kirmizi). */
            {
                const Dugum *cd = yol_cesit_coz(tk, sol);
                if (cd) {
                    if (!cesit_varyant_var(cd, d->veri.yol.sag_ad,
                                           d->veri.yol.sag_ad_uzunluk)) {
                        tip_hata(tk, d, "M002", "cesit varyanti bulunamadi");
                        return t_hata(tk);
                    }
                    return tip_olustur_yapi(tk->arena,
                        cd->veri.cesit.ad, cd->veri.cesit.ad_uzunluk, NULL, 0);
                }
            }
            /* T016 fix: modul yolu — sol TANIMLAYICI (mat::f) ya da YOL
             * (mat::ic::f) olabilir; sol'u modul_scope'a coz, sag uyeyi
             * yerel ara. Codegen @modul.ad / ic ice @m1.m2.ad ile tutarli. */
            Scope *msc = yol_modul_scope_coz(tk, sol);
            if (!msc) {
                tip_hata(tk, d, "T016", "modul bulunamadi");
                return t_hata(tk);
            }
            const Sembol *uye = sembol_bul_yerel(msc,
                d->veri.yol.sag_ad, d->veri.yol.sag_ad_uzunluk);
            if (!uye) {
                tip_hata(tk, d, "T002", "modul uyesi bulunamadi");
                return t_hata(tk);
            }
            /* A: dosya-modul gorunurlugu — yalniz 'genel' uyeler capraz-
             * modul erisilebilir. Modulun KENDI icinden (tk->scope zinciri
             * msc'den geciyorsa) tum kardesler gorunur. Dosya-ici moduller
             * (dosya_modulu=0) geriye uyumlu: denetim yok. */
            if (msc->dosya_modulu && !uye->genel) {
                int iceriden = 0;
                for (const Scope *s = tk->scope; s; s = s->parent) {
                    if (s == msc) { iceriden = 1; break; }
                }
                if (!iceriden) {
                    tip_hata(tk, d, "T041",
                        "modul uyesi 'genel' degil (private-by-default: "
                        "disari acmak icin 'genel' isareti gerekir)");
                    return t_hata(tk);
                }
            }
            /* Tek-gecis ad cozumu: kazanan uye + TAM modul oneki YOL
             * dugumune yazilir. Goreli yol (m icinden ic::g) boylece
             * codegen'de dogru mangle edilir (@m.ic.g) — onceki string
             * yolu yalniz yazildigi kadarini ("ic.g") biliyordu. */
            cozum_bagla(tk, d, uye, msc);
            return uye->tip ? uye->tip : t_hata(tk);
        }

        /* === Yapi olusturma === */
        case DUGUM_YAPI_OLUSTUR:
            return kontrol_yapi_olustur(tk, d);

        /* === Dizi olusturma === */
        case DUGUM_DIZI_OLUSTUR: {
            int n = d->veri.dizi_olustur.sayi;
            if (n == 0) {
                /* Bos dizi — context lazim, ADIM 11.5'te */
                tip_hata(tk, d, "T014", "bos dizi tipi cikarsanamaz (context lazim)");
                return tip_olustur_dizi(tk->arena, t_basit(tk, TIP_BILINMIYOR));
            }
            TipBilgisi *ilk = tip_belirle(tk, d->veri.dizi_olustur.elemanlar[0]);
            for (int i = 1; i < n; i++) {
                TipBilgisi *e = tip_belirle(tk,
                    d->veri.dizi_olustur.elemanlar[i]);
                if (!tip_esit(ilk, e) && e->kategori != TIP_HATA) {
                    tip_hata(tk, d->veri.dizi_olustur.elemanlar[i], "T013",
                             "dizi elemanlari farkli tipte");
                }
            }
            /* Linear Types Spec V1 LR-2: dizi tekkez eleman iceremez (V1) */
            if (ilk && ilk->kategori == TIP_TEKKEZ) {
                tip_hata(tk, d, "LR002",
                    "dizi elemani tekkez tipinde olamaz (V1: dizi lineer eleman iceremez)");
            }
            return tip_olustur_dizi(tk->arena, ilk);
        }

        /* === Lambda — Linear Types Spec V1 LC-2: closure-itself-linear === */
        case DUGUM_LAMBDA: {
            int n = d->veri.lambda.param_sayi;
            TipBilgisi **params = NULL;
            if (n > 0) {
                params = (TipBilgisi **)arena_ayir(tk->arena,
                            sizeof(TipBilgisi *) * (size_t)n);
            }

            /* Lambda kendi scope'unu ac */
            Scope *eski_scope = tk->scope;
            Scope *lambda_scope = scope_olustur(tk->arena, SCOPE_BLOK,
                                                 eski_scope);
            tk->scope = lambda_scope;

            for (int i = 0; i < n; i++) {
                const Dugum *p = d->veri.lambda.parametreler[i];
                if (!p->veri.parametre.tip) {
                    tip_hata(tk, p, "T015",
                             "lambda parametre tip annotasyonu gerek");
                    params[i] = t_hata(tk);
                    continue;
                }
                params[i] = ast_tip_to_bilgi(tk, p->veri.parametre.tip);
                /* Parametre sembolu ekle */
                Sembol ps;
                memset(&ps, 0, sizeof(ps));
                ps.ad = p->veri.parametre.ad;
                ps.ad_uzunluk = p->veri.parametre.ad_uzunluk;
                ps.kategori = SEMBOL_PARAMETRE;
                ps.tip = params[i];
                ps.satir = p->satir;
                ps.sutun = p->sutun;
                sembol_ekle(lambda_scope, tk->arena, &ps);
            }
            /* Closure-itself-linear takip: lambda govdesi flag'leri */
            int eski_lambda = tk->lambda_govdesi_icinde;
            int eski_yakalama = tk->lambda_lineer_yakalama;
            int eski_genel = tk->lambda_yakalama;   /* G005: ic-ice lambda korumasi */
            int eski_genel_ptr = tk->lambda_yakalama_isaretci;   /* D-323 */
            Scope *eski_baslangic = tk->lambda_baslangic_scope;
            tk->lambda_govdesi_icinde = 1;
            tk->lambda_lineer_yakalama = 0;
            tk->lambda_yakalama = 0;
            tk->lambda_yakalama_isaretci = 0;
            tk->lambda_baslangic_scope = eski_scope;

            /* Govde icin lambda_scope uzerinde yeni gövde scope (ADIM 29:
             * lambda govde scope v1 — gövde içi degisken bildirimleri
             * parametre scope'unu kirletmesin). */
            Scope *gov_eski = tk->scope;
            tk->scope = scope_olustur(tk->arena, SCOPE_ISLEV, gov_eski);
            for (int i = 0; i < n; i++) {
                const Dugum *p = d->veri.lambda.parametreler[i];
                Sembol s;
                memset(&s, 0, sizeof(s));
                s.ad = p->veri.parametre.ad;
                s.ad_uzunluk = p->veri.parametre.ad_uzunluk;
                s.kategori = SEMBOL_PARAMETRE;
                s.tip = params[i];
                s.ast_dugumu = p;
                sembol_ekle(tk->scope, tk->arena, &s);
            }
            /* D-304: blok-form gövde (`|| { ...; ver e; }`) ifade DEĞİL → deyim
             * olarak kontrol et ve dönüşü içindeki `ver`'lerden çıkarsa. İfade-form
             * (`|| e`) eski yol (gövdenin doğal tipi). Önceki durum: tip_belirle(BLOK)
             * → T001; blok-form lambda HİÇ --check'ten geçmiyordu. */
            TipBilgisi *donus;
            if (d->veri.lambda.govde &&
                d->veri.lambda.govde->tip == DUGUM_BLOK) {
                int eski_cik = tk->lambda_blok_cikarsama;
                TipBilgisi *eski_bd = tk->lambda_blok_donus;
                tk->lambda_blok_cikarsama = 1;
                tk->lambda_blok_donus = NULL;
                tip_kontrol_deyim(tk, d->veri.lambda.govde);
                donus = tk->lambda_blok_donus
                        ? tk->lambda_blok_donus
                        : tip_olustur_basit(tk->arena, TIP_BOS);
                tk->lambda_blok_cikarsama = eski_cik;
                tk->lambda_blok_donus = eski_bd;
            } else {
                donus = tip_belirle(tk, d->veri.lambda.govde);
            }
            tk->scope = gov_eski;

            int yakaladi = tk->lambda_lineer_yakalama;
            int yakaladi_genel = tk->lambda_yakalama;   /* G005 */
            int yakaladi_ptr = tk->lambda_yakalama_isaretci;   /* D-323 */

            /* Lambda parametreleri lineer ise govde icinde tuketilmeli (L001) */
            scope_lineer_kapanis_check(tk, lambda_scope);

            tk->lambda_govdesi_icinde = eski_lambda;
            tk->lambda_lineer_yakalama = eski_yakalama;
            tk->lambda_yakalama = eski_genel;
            tk->lambda_yakalama_isaretci = eski_genel_ptr;   /* D-323 */
            tk->lambda_baslangic_scope = eski_baslangic;
            tk->scope = eski_scope;

            /* === G005: YAKALAYAN(ISARETCI) ∧ KAÇAN closure reddi ===
             * D-323 DARALTMA (Mehmet karari): env ARTIK HEAP (llvm.c V2-F2 @malloc)
             * — eski "env stack-omurlu" gerekcesi OLCULDU ve GECERSIZ. Dolayisiyla
             * SKALER yakalama (env'de deger kopyasi) cerceve asiminda dangling
             * URETEMEZ → reddedilmez. ISARETCI-benzeri yakalama (metin/Dizi/ref/ham-pointer/
             * yapi) hala tehlikeli: kopyalanan isaretci gosterdigi bolgeyi (ρ_yerel
             * / cagiran cerceve) asabilir → G005 KORUNUR. Cozulemeyen tip = DENY.
             *
             * (Tarihsel gerekce:) env'i (+ {fn,env} cifti) alloca ile
             * STACK-omurlu (src/llvm.c) → frame'i asinca dangling/UAF; ayrica
             * closure_mu kaçışta kaybolup cagri yerinde mis-dispatch. Bu yuzden
             * derleme-zamaninda reddet. Kosul: (a) lambda cevre lokal/param
             * YAKALIYOR (yakaladi_genel — codegen capture'i ile birebir) VE
             * (b) lambda KAÇIYOR (escape.c forward DFA: ESC_CAGIRAN = `ver` /
             * transitif atama zinciri). Yakalamayan lambda (bare fn-ptr, env yok)
             * ve fonksiyon-ici kalan yakalayan closure REDDEDILMEZ (over-reject yok).
             * Bkz. D-071 KAPSAM-DISI "lambda escape ... non-escaping v1 garantisi". */
            if (yakaladi_genel && yakaladi_ptr && tk->aktif_escape &&
                escape_kategori(tk->aktif_escape, d) == ESC_CAGIRAN) {
                tip_hata(tk, d, "G005",
                    "isaretci yakalayan closure frame'i asamaz (kopyalanan isaretci "
                    "gosterdigi bolgeyi asabilir): skaler yakalayin, degeri parametre "
                    "olarak gecirin ya da V2'yi bekleyin");
            }

            TipBilgisi *islev_t =
                tip_olustur_islev(tk->arena, params, n, donus);

            /* LC-2: lineer yakalama varsa lambda kendisi tekkez */
            if (yakaladi) {
                return tip_olustur_tekkez(tk->arena, islev_t);
            }
            return islev_t;
        }

        /* === Hata dugumu === */
        case DUGUM_HATA:
            return t_hata(tk);

        /* === Diger (deyim/tanim) — burada tip belirleme yok === */
        default:
            tip_hata(tk, d, "T001", "ifade beklenirken farkli dugum tipi");
            return t_hata(tk);
    }
}

/* ========================================================================
 * ADIM 11.6: Generic instantiation (substitusyon)
 * ======================================================================== */

/* TIP_GENERIC_PARAM'leri concrete arg'larla recursive olarak substitute et.
 * yapi_sem: generic params'in tanimlandigi yapi sembolu
 * yapi_tipi: concrete tip (tip_arg ile) — substitusyon kaynagi */
static TipBilgisi *substitusyon(TipKontrol *tk, const TipBilgisi *t,
                                 const Sembol *yapi_sem,
                                 const TipBilgisi *yapi_tipi) {
    if (!t) return NULL;
    if (!yapi_sem || !yapi_tipi ||
        yapi_tipi->kategori != TIP_YAPI ||
        yapi_tipi->veri.yapi.tip_arg_sayi == 0) {
        return (TipBilgisi *)t;
    }

    if (t->kategori == TIP_GENERIC_PARAM) {
        /* Yapi_scope'taki generic params -> tip_arg sirasi ile esle */
        int idx = 0;
        for (SembolLink *l = yapi_sem->yapi_scope->bas; l; l = l->sonraki) {
            if (l->sembol.kategori != SEMBOL_GENERIC_PARAM) continue;
            if (l->sembol.ad_uzunluk == t->veri.generic_param.ad_uzunluk &&
                memcmp(l->sembol.ad, t->veri.generic_param.ad,
                       (size_t)t->veri.generic_param.ad_uzunluk) == 0) {
                if (idx < yapi_tipi->veri.yapi.tip_arg_sayi) {
                    return yapi_tipi->veri.yapi.tip_arg[idx];
                }
            }
            idx++;
        }
        return (TipBilgisi *)t;
    }

    /* Recursive substitusyon (referans, pointer, dizi, secimlik, sonuc) */
    switch (t->kategori) {
        case TIP_REFERANS: {
            TipBilgisi *nh = substitusyon(tk, t->veri.referans.hedef,
                                           yapi_sem, yapi_tipi);
            if (nh == t->veri.referans.hedef) return (TipBilgisi *)t;
            return tip_olustur_referans(tk->arena, nh,
                                         t->veri.referans.degisken_mi);
        }
        case TIP_POINTER: {
            TipBilgisi *nh = substitusyon(tk, t->veri.pointer.hedef,
                                           yapi_sem, yapi_tipi);
            if (nh == t->veri.pointer.hedef) return (TipBilgisi *)t;
            return tip_olustur_pointer(tk->arena, nh);
        }
        case TIP_DIZI: {
            TipBilgisi *ne = substitusyon(tk, t->veri.dizi.eleman,
                                           yapi_sem, yapi_tipi);
            if (ne == t->veri.dizi.eleman) return (TipBilgisi *)t;
            return tip_olustur_dizi(tk->arena, ne);
        }
        case TIP_SECIMLIK: {
            TipBilgisi *ni = substitusyon(tk, t->veri.secimlik.ic,
                                           yapi_sem, yapi_tipi);
            if (ni == t->veri.secimlik.ic) return (TipBilgisi *)t;
            return tip_olustur_secimlik(tk->arena, ni);
        }
        case TIP_SONUC: {
            TipBilgisi *nd = substitusyon(tk, t->veri.sonuc.deger,
                                           yapi_sem, yapi_tipi);
            TipBilgisi *nh = substitusyon(tk, t->veri.sonuc.hata,
                                           yapi_sem, yapi_tipi);
            if (nd == t->veri.sonuc.deger && nh == t->veri.sonuc.hata)
                return (TipBilgisi *)t;
            return tip_olustur_sonuc(tk->arena, nd, nh);
        }
        default:
            return (TipBilgisi *)t;
    }
}

/* ========================================================================
 * ADIM 11.5: Bidirectional tip cikarsamasi
 * ======================================================================== */

TipBilgisi *tip_belirle_beklenen(TipKontrol *tk, const Dugum *d,
                                  const TipBilgisi *beklenen) {
    if (!d) return t_hata(tk);
    if (!beklenen || beklenen->kategori == TIP_HATA) {
        return tip_belirle(tk, d);
    }

    switch (d->tip) {
        case DUGUM_TAM:
            /* Sayi literali context tamsayi tipine gore */
            if (tip_tamsayi_mi(beklenen)) {
                return t_basit(tk, beklenen->kategori);
            }
            break;

        case DUGUM_KESIRLI:
            if (beklenen->kategori == TIP_KESIRLI32 ||
                beklenen->kategori == TIP_KESIRLI64) {
                return t_basit(tk, beklenen->kategori);
            }
            break;

        case DUGUM_YAPI_OLUSTUR:
            if (beklenen->kategori == TIP_YAPI) {
                return kontrol_yapi_olustur_ic(tk, d, beklenen);
            }
            break;

        case DUGUM_DIZI_OLUSTUR: {
            if (beklenen->kategori != TIP_DIZI) break;
            const TipBilgisi *eleman_t = beklenen->veri.dizi.eleman;
            int n = d->veri.dizi_olustur.sayi;
            int bek_n = beklenen->veri.dizi.uzunluk;   /* DZ: 0 = bilinmiyor */
            /* === DZ001: dizi literali eleman sayisi != N ===
             * D-339'un SHA-256 hatasini DOGRUDAN yakalayan kural. Beklenen tip
             * yolundan gectigi icin `değişken` annotasyonu, cagri argumani,
             * `ver` ve yapi alani icin AYNI anda gecerlidir. Bos dizi de dahil
             * (N>0 iken `[]` yazmak da ihlaldir). */
            if (bek_n > 0 && n != bek_n) {
                char msj[128];
                snprintf(msj, sizeof(msj),
                    "dizi literali %d eleman iceriyor, tip %d istiyor",
                    n, bek_n);
                tip_hata(tk, d, "DZ001", msj);
            }
            if (n == 0) {
                /* Bos dizi -> beklenen tip */
                return tip_olustur_dizi_n(tk->arena,
                    t_basit(tk, eleman_t->kategori), bek_n);  /* shallow */
            }
            /* Dolu dizi: her eleman beklenen->dizi.eleman context'inde */
            for (int i = 0; i < n; i++) {
                TipBilgisi *e = tip_belirle_beklenen(tk,
                    d->veri.dizi_olustur.elemanlar[i], eleman_t);
                if (!tip_esit(e, eleman_t) && e->kategori != TIP_HATA) {
                    tip_hata(tk, d->veri.dizi_olustur.elemanlar[i],
                             "T013", "dizi elemani beklenen tip ile uyumsuz");
                }
            }
            /* DZ: sonuc, ANNOTASYONUN N'ini tasir (literalin sayisini degil).
             * Iki gerekce:
             *  (1) Degiskenin BILDIRILEN tipi odur; DZ001 uyusmazligi zaten
             *      raporladi, tip sistemi bildirime sadik kalmali.
             *  (2) N'i literalden CIKARSAMAK olmaz: `değişken xs = [1,2,3];
             *      dizi_ekle(xs, 4);` gibi ANNOTASYONSUZ mevcut kod aniden
             *      DZ003'e takilirdi. N yalniz ACIK annotasyondan bilinir →
             *      spec'in "hicbir mevcut kod kirilmaz" sozu korunur. */
            return tip_olustur_dizi_n(tk->arena,
                                      (TipBilgisi *)eleman_t, bek_n);
        }

        case DUGUM_CAGRI: {
            /* C3: çeşit varyant yapıcısı (Cesit::V(args)) — beklenen-bağlam
             * yolunda da modül-fonksiyon çağrısından ÖNCE dene. Generic çeşit
             * T'si beklenen'den (Secim<tam32>) substitue edilir (D-302). */
            {
                TipBilgisi *cy = cesit_yapici_tip_kontrol(tk, d, beklenen);
                if (cy) return cy;
            }
            /* Madde B: dizi_olustur<T>(N) beklenen Dizi<T> ise T'yi kullan.
             * Context-driven instantiation. */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 12 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "dizi_olustur", 12) == 0 &&
                beklenen->kategori == TIP_DIZI &&
                d->veri.cagri.sayi == 1) {
                tip_belirle_beklenen(tk, d->veri.cagri.argumanlar[0],
                    tip_olustur_basit(tk->arena, TIP_TAM64));
                return tip_olustur_dizi(tk->arena,
                    (TipBilgisi *)beklenen->veri.dizi.eleman);
            }
            /* Katman 2 / R-KANAL: kanal_oluştur(kapasite: tam32) -> kanal<T>.
             * T bir DEĞER argümanından çıkarsanamaz (kanal başlangıçta boş) →
             * beklenen tipten gelir; `dizi_olustur<T>(N)` / boş dizi deseninin
             * aynısı:
             *     değişken k: kanal<tam32> = kanal_oluştur(8);
             * Beklenen tip yoksa tip_belirle yolundaki DRF006 devreye girer. */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 14 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "kanal_olu\xc5\x9ftur", 14) == 0 &&
                beklenen->kategori == TIP_KANAL &&
                d->veri.cagri.sayi == 1) {
                TipBilgisi *n = tip_belirle_beklenen(tk,
                    d->veri.cagri.argumanlar[0],
                    tip_olustur_basit(tk->arena, TIP_TAM32));
                if (n->kategori != TIP_HATA && !tip_tamsayi_mi(n)) {
                    tip_hata(tk, d->veri.cagri.argumanlar[0], "DRF006",
                        "kanal_olustur kapasitesi tamsayi olmali");
                }
                return (TipBilgisi *)beklenen;   /* kanal<T> aynen doner */
            }
            /* v1 bölge-container: bölge_al(böl: yetki<R>, n: tam64) -> *T.
             * T context-driven (dizi_olustur deseni): beklenen *T olmali
             * (değişken v: *T = bölge_al(böl, n)). Yetki ÖDÜNÇ alinir
             * (tüketilmez — mmio deseni). v1 lowering malloc-vekaleten;
             * gerçek arena V2'de AYNI imzayla. */
            if (d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 9 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "b\xc3\xb6lge_al", 9) == 0 &&
                beklenen->kategori == TIP_POINTER &&
                d->veri.cagri.sayi == 2) {
                bolge_yetki_kontrol(tk, d->veri.cagri.argumanlar[0]);
                TipBilgisi *n = tip_belirle_beklenen(tk,
                    d->veri.cagri.argumanlar[1],
                    tip_olustur_basit(tk->arena, TIP_TAM64));
                if (n->kategori != TIP_HATA && !tip_tamsayi_mi(n)) {
                    tip_hata(tk, d->veri.cagri.argumanlar[1], "BL002",
                        "bolge_al eleman sayisi tamsayi (tam64) olmali");
                }
                return (TipBilgisi *)beklenen;  /* *T aynen doner */
            }
            /* Sabitsüre Spec V1: beklenen sabitsure<X> ve cagri
             * sabitsure_olustur(arg) ise — arg'ı X context'inde çıkarsa. */
            if (beklenen->kategori == TIP_SABITSURE &&
                d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 18 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "sabits\xc3\xbc" "re_olustur", 18) == 0 &&
                d->veri.cagri.sayi == 1) {
                TipBilgisi *ic = tip_belirle_beklenen(tk,
                    d->veri.cagri.argumanlar[0],
                    beklenen->veri.sabitsure.ic);
                if (ic->kategori == TIP_HATA) return t_hata(tk);
                if (tip_sabitsure_mi(ic)) {
                    tip_hata(tk, d, "CT006",
                        "sabitsure<sabitsure<T>> nesting yasak");
                    return t_hata(tk);
                }
                if (!tip_sabitsure_yetenekli_mi(ic)) {
                    tip_hata(tk, d, "CT006",
                        "sabitsure_olustur: sarilan tip constant-time yetenekli "
                        "degil");
                    return t_hata(tk);
                }
                return tip_olustur_sabitsure(tk->arena, ic);
            }
            /* SIMD Spec V1: beklenen vektor<T, N> ve cagri vektor_doldur(s)
             * ise — s'yi T context'inde çıkarsa, dönüş vektor<T, N>. */
            if (beklenen->kategori == TIP_VEKTOR &&
                d->veri.cagri.hedef &&
                d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI &&
                d->veri.cagri.hedef->veri.tanimlayici.uzunluk == 13 &&
                memcmp(d->veri.cagri.hedef->veri.tanimlayici.metin,
                       "vektor_doldur", 13) == 0 &&
                d->veri.cagri.sayi == 1) {
                TipBilgisi *arg_t = tip_belirle_beklenen(tk,
                    d->veri.cagri.argumanlar[0],
                    beklenen->veri.vektor.eleman);
                if (arg_t->kategori == TIP_HATA) return t_hata(tk);
                if (!tip_vektor_eleman_yetenekli_mi(arg_t)) {
                    tip_hata(tk, d, "V001",
                        "vektor_doldur argumani vektor-yetenekli skaler olmali");
                    return t_hata(tk);
                }
                /* arg tip vs beklenen element tip uyumsuzsa T001 */
                if (!tip_esit(arg_t, beklenen->veri.vektor.eleman)) {
                    tip_hata(tk, d, "T001",
                        "vektor_doldur arg tipi beklenen element tipi ile uyumsuz");
                    return t_hata(tk);
                }
                return tip_olustur_vektor(tk->arena, arg_t,
                    beklenen->veri.vektor.lane_sayi);
            }
            break;
        }

        case DUGUM_IKILI: {
            /* Sabitsüre: beklenen sabitsüre<X> ise, operandlar X context'inde
             * çıkarsanır. */
            if (beklenen->kategori == TIP_SABITSURE) {
                TipBilgisi *ic_bek = beklenen->veri.sabitsure.ic;
                TipBilgisi *sol = tip_belirle_beklenen(tk,
                    d->veri.ikili.sol, ic_bek);
                TipBilgisi *sag = tip_belirle_beklenen(tk,
                    d->veri.ikili.sag, ic_bek);
                if (sol->kategori == TIP_HATA || sag->kategori == TIP_HATA) {
                    return t_hata(tk);
                }
            }
            break;
        }

        default:
            break;
    }

    /* Default davranis */
    return tip_belirle(tk, d);
}

/* ========================================================================
 * ADIM 11.4: Deyim ve tanim tip kontrolu
 * ======================================================================== */

static void tip_kontrol_deyim(TipKontrol *tk, const Dugum *d);
static void tip_kontrol_tanim(TipKontrol *tk, const Dugum *d);

/* === 1. Gecis: yapi/islev/sabit sembollerini global'e ekle === */

static void pre_populate_yapi(TipKontrol *tk, const Dugum *yapi) {
    /* Yapi scope olustur */
    Scope *yapi_s = scope_olustur(tk->arena, SCOPE_YAPI, tk->global_scope);

    /* Generic params -> yapi scope'a ekle */
    for (int j = 0; j < yapi->veri.yapi.tip_param_sayi; j++) {
        const char *t_ad = yapi->veri.yapi.tip_paramlar[j];
        int t_uz = (int)strlen(t_ad);
        Sembol gp;
        memset(&gp, 0, sizeof(gp));
        gp.ad = t_ad;
        gp.ad_uzunluk = t_uz;
        gp.kategori = SEMBOL_GENERIC_PARAM;
        gp.tip = tip_olustur_generic_param(tk->arena, t_ad, t_uz);
        gp.satir = yapi->satir;
        gp.sutun = yapi->sutun;
        sembol_ekle(yapi_s, tk->arena, &gp);
    }

    /* Alanlar -> yapi scope'a ekle (tip bilgisi yapi_scope context'inde) */
    Scope *eski = tk->scope;
    tk->scope = yapi_s;
    for (int j = 0; j < yapi->veri.yapi.alan_sayi; j++) {
        const Dugum *alan = yapi->veri.yapi.alanlar[j];
        TipBilgisi *alan_tipi = ast_tip_to_bilgi(tk, alan->veri.alan.tip);
        /* Linear Types Spec V1 LR-2: yapi tekkez/yetki/gorev alani iceremez.
         * DRF V1 genişletmesi: tüm linear tipler LR-2 altında.
         * D-313 (Linear V2) MUAFİYETİ: `yapı tekkez K` KENDİSİ lineer olduğu
         * için lineer alan taşıyabilir — sahiplik zinciri kopmaz (K tüketilmeden
         * kaybolamaz, K tüketilince alanları da onunla gider). Sıradan `yapı`
         * için yasak AYNEN sürer: orada alan sahipsiz kalır ve sızardı. */
        if (alan_tipi && tip_lineer_mi(alan_tipi)
            && !yapi->veri.yapi.lineer_mi) {
            tip_hata(tk, alan, "LR002",
                "yapi alani lineer tipte olamaz "
                "(lineer alan icin `yapi tekkez` kullanin)");
        }
        Sembol s;
        memset(&s, 0, sizeof(s));
        s.ad = alan->veri.alan.ad;
        s.ad_uzunluk = alan->veri.alan.ad_uzunluk;
        s.kategori = SEMBOL_DEGISKEN;
        s.tip = alan_tipi;
        s.satir = alan->satir;
        s.sutun = alan->sutun;
        if (sembol_ekle(yapi_s, tk->arena, &s) != 0) {
            tip_hata(tk, alan, "T024", "yapi alan adi cakismasi");
        }
    }
    tk->scope = eski;

    /* Yapi sembolu global'e */
    Sembol y;
    memset(&y, 0, sizeof(y));
    y.ad = yapi->veri.yapi.ad;
    y.ad_uzunluk = yapi->veri.yapi.ad_uzunluk;
    y.kategori = SEMBOL_YAPI;
    y.yapi_scope = yapi_s;
    y.ast_dugumu = yapi;
    y.satir = yapi->satir;
    y.sutun = yapi->sutun;
    y.genel = yapi->veri.yapi.genel_mi;
    if (sembol_ekle(tk->global_scope, tk->arena, &y) != 0) {
        tip_hata(tk, yapi, "T026", "yapi tanimi cakismasi");
    }
}

/* C2.7: çeşit'i nominal tip olarak kaydet (SEMBOL_YAPI; varyantlar ast_dugumu'nda).
 * yapı'ya analog ama alansız/generic'siz — varyant kümesi DUGUM_CESIT'te. */
static void pre_populate_cesit(TipKontrol *tk, const Dugum *cesit) {
    Sembol c;
    memset(&c, 0, sizeof(c));
    c.ad = cesit->veri.cesit.ad;
    c.ad_uzunluk = cesit->veri.cesit.ad_uzunluk;
    c.kategori = SEMBOL_YAPI;   /* nominal isimli tip (çeşit dahil) */
    c.ast_dugumu = cesit;       /* exhaustiveness + varyant doğrulama buradan */
    c.satir = cesit->satir;
    c.sutun = cesit->sutun;
    c.genel = cesit->veri.cesit.genel_mi;
    /* Generic çeşit (D-302): tip paramlarını yapi_scope'a SEMBOL_GENERIC_PARAM
     * olarak kaydet (yapı ile birebir). Böylece `substitusyon` payload tipindeki
     * T'yi beklenen `Secim<tam32>`'in tip_arg'ından çözebilir; payload T de bu
     * scope'ta TIP_GENERIC_PARAM olarak resolve olur (yoksa T011). */
    if (cesit->veri.cesit.tip_param_sayi > 0) {
        Scope *cs = scope_olustur(tk->arena, SCOPE_YAPI, tk->global_scope);
        for (int j = 0; j < cesit->veri.cesit.tip_param_sayi; j++) {
            const char *t_ad = cesit->veri.cesit.tip_paramlar[j];
            int t_uz = (int)strlen(t_ad);
            Sembol gp;
            memset(&gp, 0, sizeof(gp));
            gp.ad = t_ad;
            gp.ad_uzunluk = t_uz;
            gp.kategori = SEMBOL_GENERIC_PARAM;
            gp.tip = tip_olustur_generic_param(tk->arena, t_ad, t_uz);
            sembol_ekle(cs, tk->arena, &gp);
        }
        c.yapi_scope = cs;
    }
    if (sembol_ekle(tk->global_scope, tk->arena, &c) != 0) {
        tip_hata(tk, cesit, "T026", "tip tanimi cakismasi (cesit)");
    }
}

static void pre_populate_islev(TipKontrol *tk, const Dugum *islev) {
    int n = islev->veri.islev.param_sayi;
    TipBilgisi **ptipler = NULL;
    if (n > 0) {
        ptipler = (TipBilgisi **)arena_ayir(tk->arena,
                    sizeof(TipBilgisi *) * (size_t)n);
    }
    /* Generic params: gecici scope'a ekle (parametre/donus tipi resolve icin) */
    Scope *gp_scope = NULL;
    if (islev->veri.islev.tip_param_sayi > 0) {
        gp_scope = scope_olustur(tk->arena, SCOPE_BLOK, tk->global_scope);
        for (int j = 0; j < islev->veri.islev.tip_param_sayi; j++) {
            const char *t_ad = islev->veri.islev.tip_paramlar[j];
            int t_uz = (int)strlen(t_ad);
            Sembol gp;
            memset(&gp, 0, sizeof(gp));
            gp.ad = t_ad;
            gp.ad_uzunluk = t_uz;
            gp.kategori = SEMBOL_GENERIC_PARAM;
            gp.tip = tip_olustur_generic_param(tk->arena, t_ad, t_uz);
            sembol_ekle(gp_scope, tk->arena, &gp);
        }
    }
    Scope *eski_scope = tk->scope;
    if (gp_scope) tk->scope = gp_scope;
    for (int j = 0; j < n; j++) {
        const Dugum *p = islev->veri.islev.parametreler[j];
        if (p->veri.parametre.kendin_mi) {
            /* kendin parametre: tipi uygula context'inde belirlenir,
             * pre-populate'da henuz bilinmiyor — bilinmeyen olarak isaretle */
            ptipler[j] = tip_olustur_basit(tk->arena, TIP_BILINMIYOR);
            continue;
        }
        ptipler[j] = ast_tip_to_bilgi(tk, p->veri.parametre.tip);
    }
    TipBilgisi *donus = islev->veri.islev.donus_tipi
        ? ast_tip_to_bilgi(tk, islev->veri.islev.donus_tipi)
        : tip_olustur_basit(tk->arena, TIP_BOS);
    tk->scope = eski_scope;
    TipBilgisi *islev_tipi = tip_olustur_islev(tk->arena, ptipler, n, donus);
    /* Realtime Spec V1: işlev imzasında qualifier flag taşınır. */
    if (islev_tipi) {
        islev_tipi->veri.islev.gercekzamanli_mi =
            islev->veri.islev.gercekzamanli_mi;
    }

    Sembol s;
    memset(&s, 0, sizeof(s));
    s.ad = islev->veri.islev.ad;
    s.ad_uzunluk = islev->veri.islev.ad_uzunluk;
    s.kategori = SEMBOL_ISLEV;
    s.tip = islev_tipi;
    s.ast_dugumu = islev;
    s.satir = islev->satir;
    s.sutun = islev->sutun;
    s.genel = islev->veri.islev.genel_mi;
    if (sembol_ekle(tk->global_scope, tk->arena, &s) != 0) {
        tip_hata(tk, islev, "T024", "islev tanimi cakismasi");
    }
}

static void pre_populate_sabit(TipKontrol *tk, const Dugum *sabit) {
    TipBilgisi *t = ast_tip_to_bilgi(tk, sabit->veri.sabit.tip);
    Sembol s;
    memset(&s, 0, sizeof(s));
    s.ad = sabit->veri.sabit.ad;
    s.ad_uzunluk = sabit->veri.sabit.ad_uzunluk;
    s.kategori = SEMBOL_SABIT;
    s.tip = t;
    s.ast_dugumu = sabit;
    s.satir = sabit->satir;
    s.sutun = sabit->sutun;
    s.genel = sabit->veri.sabit.genel_mi;
    if (sembol_ekle(tk->global_scope, tk->arena, &s) != 0) {
        tip_hata(tk, sabit, "T024", "sabit tanimi cakismasi");
    }
}

/* D-252: küresel değişken → global sembol. Tip-kısıt: yalnız skaler + ham-pointer
 * (Dizi/yapı allocator'a bağlı → circular; yasak). Init sabit-literal olmalı. Erişim
 * güvensiz-only (DUGUM_TANIMLAYICI'de E010 enforce). Bootstrap-circularity çözümü. */
static void pre_populate_kuresel(TipKontrol *tk, const Dugum *kd) {
    TipBilgisi *t = ast_tip_to_bilgi(tk, kd->veri.degisken.tip);
    int izinli = (t && (tip_sayisal_mi(t) || t->kategori == TIP_POINTER ||
                        t->kategori == TIP_MANTIKSAL || t->kategori == TIP_KARAKTER));
    if (!izinli) {
        tip_hata(tk, kd, "E011",
            "kuresel degisken tipi yalniz skaler/ham-pointer olabilir "
            "(Dizi/yapi/metin yasak — allocator'a baglanamaz)");
    }
    const Dugum *dv = kd->veri.degisken.deger;
    int sabit_init = (dv && (dv->tip == DUGUM_TAM || dv->tip == DUGUM_KESIRLI ||
                             dv->tip == DUGUM_MANTIKSAL || dv->tip == DUGUM_BOS ||
                             dv->tip == DUGUM_KARAKTER));
    if (!sabit_init) {
        tip_hata(tk, kd, "E012",
            "kuresel degisken baslangic degeri sabit-literal olmali "
            "(sayi/mantik/karakter/bos)");
    }
    Sembol s;
    memset(&s, 0, sizeof(s));
    s.ad = kd->veri.degisken.ad;
    s.ad_uzunluk = kd->veri.degisken.ad_uzunluk;
    s.kategori = SEMBOL_DEGISKEN;
    s.tip = t;
    s.ast_dugumu = kd;
    s.satir = kd->satir;
    s.sutun = kd->sutun;
    s.kuresel = 1;
    if (sembol_ekle(tk->global_scope, tk->arena, &s) != 0) {
        tip_hata(tk, kd, "T024", "kuresel tanimi cakismasi");
    }
}

static void pre_populate_ozellik(TipKontrol *tk, const Dugum *oz) {
    /* Ozellik sembolunu global'e ekle (bound olarak referans gerekli) */
    Sembol s;
    memset(&s, 0, sizeof(s));
    s.ad = oz->veri.ozellik.ad;
    s.ad_uzunluk = oz->veri.ozellik.ad_uzunluk;
    s.kategori = SEMBOL_OZELLIK;
    s.ast_dugumu = oz;
    s.satir = oz->satir;
    s.sutun = oz->sutun;
    if (sembol_ekle(tk->global_scope, tk->arena, &s) != 0) {
        tip_hata(tk, oz, "T024", "ozellik tanimi cakismasi");
    }
}

/* AST tip dugumunden 'kok' adi cikar (basit/kullanici). Bilinmiyorsa NULL. */
static const char *tip_dugumu_kok_adi(const Dugum *t, int *out_uz) {
    if (!t) return NULL;
    if (t->tip == DUGUM_TIP_BASIT) {
        *out_uz = t->veri.tip_basit.ad_uzunluk;
        return t->veri.tip_basit.ad;
    }
    if (t->tip == DUGUM_TIP_KULLANICI && t->veri.tip_kullanici.yol) {
        const Dugum *y = t->veri.tip_kullanici.yol;
        if (y->tip == DUGUM_TANIMLAYICI) {
            *out_uz = y->veri.tanimlayici.uzunluk;
            return y->veri.tanimlayici.metin;
        }
    }
    return NULL;
}

static void pre_populate_uygula(TipKontrol *tk, const Dugum *uy) {
    /* Hedef tip adi */
    int tip_uz = 0;
    const char *tip_ad = tip_dugumu_kok_adi(uy->veri.uygula.tip, &tip_uz);
    if (!tip_ad) return;

    if (uy->veri.uygula.ozellik_sayi == 0) {
        /* Inherent impl */
        uygula_tablosu_ekle(&tk->uygulamalar, tk->arena,
                            tip_ad, tip_uz, NULL, 0, uy);
    } else {
        /* Trait impls */
        for (int i = 0; i < uy->veri.uygula.ozellik_sayi; i++) {
            int oz_uz = 0;
            const char *oz_ad = tip_dugumu_kok_adi(
                uy->veri.uygula.ozellikler[i], &oz_uz);
            if (oz_ad) {
                uygula_tablosu_ekle(&tk->uygulamalar, tk->arena,
                                    tip_ad, tip_uz, oz_ad, oz_uz, uy);
            }
        }
    }
}

/* T016 fix: bir uye dizisini (program ya da modul govdesi) mevcut
 * tk->global_scope / tk->scope icine pre-populate eder. Cagiran modul
 * govdesi icin bu iki scope'u modul_scope'a takas eder; boylece mevcut
 * pre_populate_* (global_scope'a sabit ekleyen) helper'lari modul
 * uyelerini dogru scope'a kaydeder. Codegen'in @modul.ad duzlestirme
 * modeliyle TUTARLI: her modul kendi ad-uzayini tasir. */
static void pre_populate_modul(TipKontrol *tk, const Dugum *m);

static void pre_populate_uyeler(TipKontrol *tk, Dugum *const *uyeler,
                                int sayi) {
    /* 1) Once ozellikleri ekle (bound referansi icin) */
    for (int i = 0; i < sayi; i++) {
        const Dugum *uye = uyeler[i];
        if (uye->tip == DUGUM_OZELLIK) pre_populate_ozellik(tk, uye);
    }

    /* 2) Yapilari (tipleri) ekle */
    for (int i = 0; i < sayi; i++) {
        const Dugum *uye = uyeler[i];
        if (uye->tip == DUGUM_YAPI) pre_populate_yapi(tk, uye);
        else if (uye->tip == DUGUM_CESIT) pre_populate_cesit(tk, uye);
        else if (uye->tip == DUGUM_DISA && uye->veri.disa.tanim &&
                 uye->veri.disa.tanim->tip == DUGUM_YAPI) {
            pre_populate_yapi(tk, uye->veri.disa.tanim);
        }
        else if (uye->tip == DUGUM_DISA && uye->veri.disa.tanim &&
                 uye->veri.disa.tanim->tip == DUGUM_CESIT) {
            pre_populate_cesit(tk, uye->veri.disa.tanim);
        }
    }

    /* 3) Uygula bildirimlerini kayit et (yapi+ozellik bilindikten sonra) */
    for (int i = 0; i < sayi; i++) {
        const Dugum *uye = uyeler[i];
        if (uye->tip == DUGUM_UYGULA) pre_populate_uygula(tk, uye);
    }

    /* 4) Islevler, sabitler ve ic ice moduller */
    for (int i = 0; i < sayi; i++) {
        const Dugum *uye = uyeler[i];
        const Dugum *gercek = (uye->tip == DUGUM_DISA && uye->veri.disa.tanim)
                              ? uye->veri.disa.tanim : uye;
        if (gercek->tip == DUGUM_ISLEV) pre_populate_islev(tk, gercek);
        else if (gercek->tip == DUGUM_SABIT) pre_populate_sabit(tk, gercek);
        else if (gercek->tip == DUGUM_MODUL) pre_populate_modul(tk, gercek);
        else if (gercek->tip == DUGUM_DEGISKEN &&
                 gercek->veri.degisken.kuresel_mi)   /* D-252 küresel değişken */
            pre_populate_kuresel(tk, gercek);
    }
}

/* T016 fix: modulu SEMBOL_MODUL olarak kaydet + modul_scope kur.
 * modul_scope parent'i mevcut global_scope (ic ice modul = parent modul
 * scope'u) -> uyeler once kendi modulunde, sonra ust kapsamda cozulur. */
static void pre_populate_modul(TipKontrol *tk, const Dugum *m) {
    Scope *eski_global = tk->global_scope;
    Scope *eski_scope = tk->scope;

    /* A: dosya-modul — scope parent'i builtin_scope (giris dosyasinin
     * ozel adlari modul govdelerine SIZMAZ, built-in'ler gorunur);
     * kanonik sembol builtin_scope'a GIZLI olarak kaydedilir (yalniz
     * 'kullan' alias'lari ve onek turetme erisir). Dosya-ici modul:
     * eski davranis (parent + sembol = mevcut global). */
    int dosya_m = m->veri.modul.dosya_modulu;
    Scope *parent = dosya_m ? tk->builtin_scope : eski_global;
    Scope *msc = scope_olustur(tk->arena, SCOPE_MODUL, parent);
    if (!msc) return;
    /* Gorunurluk bayragi: dosya-modul icindeki ic ice moduller de
     * dosya-modul sayilir (genel denetimi onlara da uygulanir). */
    msc->dosya_modulu = dosya_m ||
        (eski_global && eski_global->dosya_modulu);

    /* Uye pre-populate'i modul_scope baglaminda yap (helper'lar
     * tk->global_scope'a ekler; tk->scope tip-adi cozumu icin). */
    tk->global_scope = msc;
    tk->scope = msc;
    pre_populate_uyeler(tk, m->veri.modul.uyeler, m->veri.modul.sayi);
    tk->global_scope = eski_global;
    tk->scope = eski_scope;

    /* Modul sembolunu ust (parent) scope'a ekle */
    Sembol s;
    memset(&s, 0, sizeof(s));
    s.ad = m->veri.modul.ad;
    s.ad_uzunluk = m->veri.modul.ad_uzunluk;
    s.kategori = SEMBOL_MODUL;
    s.modul_scope = msc;
    s.ast_dugumu = m;
    s.satir = m->satir;
    s.sutun = m->sutun;
    s.gizli = dosya_m;  /* dosya-modul: yalniz kullan-alias'lariyla erisim */
    if (sembol_ekle(dosya_m ? tk->builtin_scope : eski_global,
                    tk->arena, &s) != 0) {
        tip_hata(tk, m, "T024", "modul tanimi cakismasi");
    }
}

static void pre_populate(TipKontrol *tk, const Dugum *program) {
    if (!program || program->tip != DUGUM_PROGRAM) return;
    pre_populate_uyeler(tk, program->veri.program.uyeler,
                        program->veri.program.sayi);
}

/* === A: kullan baglari (iki-fazli yuklemenin 2. fazi) ===
 *
 * Loader (ana.c) erisilebilir dosya-modulleri kesfedip parse etti ve
 * sentetik DUGUM_MODUL (dosya_modulu=1) olarak programa ekledi;
 * pre_populate kanonik kayitlari (gizli, builtin_scope) kurdu. Bu faz
 * her 'kullan' bildirimini, BILDIRIMI ICEREN dosyanin scope'una
 * gorunur baglara cevirir:
 *   kullan dizi;            -> SEMBOL_MODUL "dizi" (nitelikli erisim)
 *   kullan dizi olarak d;   -> SEMBOL_MODUL "d" (alias)
 *   kullan dizi::{a, b};    -> modul bagi + a/b uye alias'lari
 *                              (ithal_onek="dizi" — binding MODUL_UYESI)
 * Tum moduller kayitli oldugu icin bildirim sirasi onemsiz (dongusel
 * import v1'de hata degil). Cok-segment ciplak yol (kullan a::b::c;)
 * legacy duzlestirme olarak tanim fazinda islenir. */

static int kullan_yeni_bicim_mi(const Dugum *k) {
    return k->veri.kullan.segment_sayi <= 1 ||
           k->veri.kullan.secili_sayi > 0 ||
           k->veri.kullan.alias_ad != NULL;
}

static void kullan_isle(TipKontrol *tk, const Dugum *k, Scope *hedef) {
    if (!kullan_yeni_bicim_mi(k)) return;  /* legacy — tanim fazinda */
    const char *mad = k->veri.kullan.yol;
    int muz = k->veri.kullan.yol_uzunluk;
    const Sembol *kanonik = sembol_bul_yerel(tk->builtin_scope, mad, muz);
    if (!kanonik || kanonik->kategori != SEMBOL_MODUL ||
        !kanonik->modul_scope) {
        tip_hata(tk, k, "T040",
            "kullan: mod\xc3\xbcl y\xc3\xbcklenemedi "
            "(dosya bulunamad\xc4\xb1 ya da y\xc3\xbckleyici ko\xc5\x9fmad\xc4\xb1)");
        return;
    }

    /* Modul bagi — alias varsa o adla (kullan dizi olarak d;) */
    {
        Sembol mb;
        memset(&mb, 0, sizeof(mb));
        mb.ad = k->veri.kullan.alias_ad ? k->veri.kullan.alias_ad
                                        : kanonik->ad;
        mb.ad_uzunluk = k->veri.kullan.alias_ad ? k->veri.kullan.alias_ad_uz
                                                : kanonik->ad_uzunluk;
        mb.kategori = SEMBOL_MODUL;
        mb.modul_scope = kanonik->modul_scope;
        mb.ast_dugumu = kanonik->ast_dugumu;
        mb.satir = k->satir;
        mb.sutun = k->sutun;
        if (sembol_ekle(hedef, tk->arena, &mb) != 0) {
            /* Ayni modul ikinci kez kullan edildiyse sessiz (idempotent);
             * farkli bir tanimla cakistiysa T024. */
            Sembol *mevcut = sembol_bul_yazilabilir(hedef, mb.ad,
                                                    mb.ad_uzunluk);
            if (!(mevcut && mevcut->kategori == SEMBOL_MODUL &&
                  mevcut->modul_scope == kanonik->modul_scope)) {
                tip_hata(tk, k, "T024", "kullan: ad cakismasi (modul bagi)");
            }
        }
    }

    /* Secili adlar (kullan dizi::{Liste, ekle};) */
    for (int i = 0; i < k->veri.kullan.secili_sayi; i++) {
        const char *ad = k->veri.kullan.secili_adlar[i];
        int uz = k->veri.kullan.secili_uzunluklar[i];
        const Sembol *hs = sembol_bul_yerel(kanonik->modul_scope, ad, uz);
        if (!hs) {
            tip_hata(tk, k, "T002", "secili import: modul uyesi bulunamadi");
            continue;
        }
        if (!hs->genel) {
            tip_hata(tk, k, "T041",
                "secili import: uye 'genel' degil (private-by-default)");
            continue;
        }
        Sembol alias = *hs;
        alias.ithal_onek = kanonik->ad;       /* mangling oneki = modul adi */
        alias.ithal_onek_uz = kanonik->ad_uzunluk;
        alias.gizli = 0;
        alias.ithal_cakisma = 0;
        alias.satir = k->satir;
        alias.sutun = k->sutun;
        if (sembol_ekle(hedef, tk->arena, &alias) != 0) {
            Sembol *mevcut = sembol_bul_yazilabilir(hedef, ad, uz);
            if (mevcut && mevcut->ithal_onek) {
                /* Iki secili import ayni adi getirdi: kullanim aninda
                 * T042 (a::f / b::f nitelikli erisim gecerli kalir). */
                if (mevcut->ithal_onek_uz != alias.ithal_onek_uz ||
                    memcmp(mevcut->ithal_onek, alias.ithal_onek,
                           (size_t)alias.ithal_onek_uz) != 0) {
                    mevcut->ithal_cakisma = 1;
                }
            } else {
                tip_hata(tk, k, "T024",
                    "secili import: yerel tanimla ad cakismasi");
            }
        }
    }
}

static void kullan_baglari_kur(TipKontrol *tk, const Dugum *program) {
    if (!program || program->tip != DUGUM_PROGRAM) return;
    for (int i = 0; i < program->veri.program.sayi; i++) {
        const Dugum *uye = program->veri.program.uyeler[i];
        if (!uye) continue;
        if (uye->tip == DUGUM_KULLAN) {
            kullan_isle(tk, uye, tk->global_scope);
        } else if (uye->tip == DUGUM_MODUL) {
            /* Modulun kendi kullan'lari kendi scope'una baglanir */
            Scope *arama = uye->veri.modul.dosya_modulu
                ? tk->builtin_scope : tk->global_scope;
            const Sembol *ms = sembol_bul_yerel(arama,
                uye->veri.modul.ad, uye->veri.modul.ad_uzunluk);
            if (!ms || ms->kategori != SEMBOL_MODUL || !ms->modul_scope) {
                continue;
            }
            for (int j = 0; j < uye->veri.modul.sayi; j++) {
                const Dugum *mu = uye->veri.modul.uyeler[j];
                if (mu && mu->tip == DUGUM_KULLAN) {
                    kullan_isle(tk, mu, ms->modul_scope);
                }
            }
        }
    }
}

/* C2.7: eşleş kapsayıcılık (Maranget usefulness, flat + sonuç<_,çeşit> bir
 * seviye nesting). Kapalı tip (çeşit / seçimlik / sonuç) üzerinde eksik varyant
 * → M001. Top-level wildcard '_' veya binding catch-all → exhaustive. Açık
 * tipler (tamsayı vb.) denetlenmez (geriye uyum: mevcut eşleş'ler kırılmaz). */
static void esles_exhaustive_kontrol(TipKontrol *tk, const Dugum *d,
                                     TipBilgisi *dt) {
    if (!dt || dt->kategori == TIP_HATA) return;
    int n = d->veri.esles.kol_sayi;

    /* Top-level wildcard / catch-all binding? ('hiç' hariç — o seçimlik varyantı) */
    for (int i = 0; i < n; i++) {
        const Dugum *ds = d->veri.esles.kollar[i]->veri.esles_kolu.desen;
        if (!ds) continue;
        if (ds->tip == DUGUM_DESEN_JOKER) return;
        if (ds->tip == DUGUM_DESEN_TANIMLAYICI) {
            const char *a = ds->veri.desen_tanimlayici.ad;
            int u = ds->veri.desen_tanimlayici.ad_uzunluk;
            if (!(u == 4 && memcmp(a, "hi\xc3\xa7", 4) == 0)) return;
        }
    }

    /* çeşit: her varyant Cesit::Varyant ile kapsanmalı */
    if (dt->kategori == TIP_YAPI) {
        const Dugum *cd = cesit_ara(tk, dt->veri.yapi.ad,
                                    dt->veri.yapi.ad_uzunluk);
        if (!cd) return;  /* yapı (struct) → exhaustiveness yok */
        char eksik[256]; int eo = 0, eksik_var = 0;
        for (int v = 0; v < cd->veri.cesit.varyant_sayi; v++) {
            const char *va = cd->veri.cesit.varyantlar[v];
            int vu = cd->veri.cesit.varyant_uzunluklar[v];
            int kapsandi = 0;
            for (int i = 0; i < n; i++) {
                const Dugum *ds = d->veri.esles.kollar[i]->veri.esles_kolu.desen;
                if (ds && ds->tip == DUGUM_DESEN_YOL &&
                    ds->veri.desen_yol.varyant_uz == vu &&
                    memcmp(ds->veri.desen_yol.varyant_ad, va, (size_t)vu) == 0) {
                    kapsandi = 1; break;
                }
            }
            if (!kapsandi) {
                eksik_var = 1;
                if (eo < (int)sizeof(eksik) - vu - 4) {
                    if (eo) { eksik[eo++] = ','; eksik[eo++] = ' '; }
                    memcpy(eksik + eo, va, (size_t)vu); eo += vu;
                }
            }
        }
        eksik[eo] = '\0';
        if (eksik_var) {
            char msg[320];
            snprintf(msg, sizeof(msg),
                "esles exhaustive degil — eksik varyant(lar): [%s]", eksik);
            tip_hata(tk, d, "M001", msg);
        }
        return;
    }

    /* seçimlik<T>: değer + hiç */
    if (dt->kategori == TIP_SECIMLIK) {
        int deger_c = 0, hic_c = 0;
        for (int i = 0; i < n; i++) {
            const Dugum *ds = d->veri.esles.kollar[i]->veri.esles_kolu.desen;
            if (!ds) continue;
            if (ds->tip == DUGUM_DESEN_YAPICI &&
                ds->veri.desen_yapici.ad_uzunluk == 6 &&
                memcmp(ds->veri.desen_yapici.ad, "de\xc4\x9f" "er", 6) == 0) {
                deger_c = 1;
            }
            if (ds->tip == DUGUM_DESEN_TANIMLAYICI &&
                ds->veri.desen_tanimlayici.ad_uzunluk == 4 &&
                memcmp(ds->veri.desen_tanimlayici.ad, "hi\xc3\xa7", 4) == 0) {
                hic_c = 1;
            }
        }
        if (!deger_c || !hic_c) {
            char msg[128]; int o = 0; int once = 0;
            o += snprintf(msg + o, (size_t)((int)sizeof(msg) - o),
                          "esles exhaustive degil — eksik: [");
            if (!deger_c) {
                o += snprintf(msg + o, (size_t)((int)sizeof(msg) - o),
                              "de\xc4\x9f" "er"); once = 1;
            }
            if (!hic_c) {
                o += snprintf(msg + o, (size_t)((int)sizeof(msg) - o),
                              "%shi\xc3\xa7", once ? ", " : "");
            }
            snprintf(msg + o, (size_t)((int)sizeof(msg) - o), "]");
            tip_hata(tk, d, "M001", msg);
        }
        return;
    }

    /* sonuç<T,H>: tamam + hata; H çeşit ise hata içi varyant düzeyi (D6). */
    if (dt->kategori == TIP_SONUC) {
        int tamam_c = 0, hata_catchall = 0;
        for (int i = 0; i < n; i++) {
            const Dugum *ds = d->veri.esles.kollar[i]->veri.esles_kolu.desen;
            if (!ds || ds->tip != DUGUM_DESEN_YAPICI) continue;
            const char *a = ds->veri.desen_yapici.ad;
            int u = ds->veri.desen_yapici.ad_uzunluk;
            if (u == 5 && memcmp(a, "tamam", 5) == 0) tamam_c = 1;
            else if (u == 4 && memcmp(a, "hata", 4) == 0) {
                if (ds->veri.desen_yapici.sayi > 0) {
                    const Dugum *sub = ds->veri.desen_yapici.alt_desenler[0];
                    if (sub && (sub->tip == DUGUM_DESEN_TANIMLAYICI ||
                                sub->tip == DUGUM_DESEN_JOKER)) hata_catchall = 1;
                } else {
                    hata_catchall = 1;
                }
            }
        }
        int hata_exh = hata_catchall;
        char eksik[256]; int eo = 0;
        TipBilgisi *H = dt->veri.sonuc.hata;
        const Dugum *hcd = (H && H->kategori == TIP_YAPI)
            ? cesit_ara(tk, H->veri.yapi.ad, H->veri.yapi.ad_uzunluk) : NULL;
        if (!hata_exh && hcd) {
            int hepsi = 1;
            for (int v = 0; v < hcd->veri.cesit.varyant_sayi; v++) {
                const char *va = hcd->veri.cesit.varyantlar[v];
                int vu = hcd->veri.cesit.varyant_uzunluklar[v];
                int kapsandi = 0;
                for (int i = 0; i < n; i++) {
                    const Dugum *ds =
                        d->veri.esles.kollar[i]->veri.esles_kolu.desen;
                    if (ds && ds->tip == DUGUM_DESEN_YAPICI &&
                        ds->veri.desen_yapici.ad_uzunluk == 4 &&
                        memcmp(ds->veri.desen_yapici.ad, "hata", 4) == 0 &&
                        ds->veri.desen_yapici.sayi > 0) {
                        const Dugum *sub =
                            ds->veri.desen_yapici.alt_desenler[0];
                        if (sub && sub->tip == DUGUM_DESEN_YOL &&
                            sub->veri.desen_yol.varyant_uz == vu &&
                            memcmp(sub->veri.desen_yol.varyant_ad, va,
                                   (size_t)vu) == 0) { kapsandi = 1; break; }
                    }
                }
                if (!kapsandi) {
                    hepsi = 0;
                    if (eo < (int)sizeof(eksik) - vu - 10) {
                        if (eo) { eksik[eo++] = ','; eksik[eo++] = ' '; }
                        eo += snprintf(eksik + eo,
                            (size_t)((int)sizeof(eksik) - eo),
                            "hata(%.*s)", vu, va);
                    }
                }
            }
            hata_exh = hepsi;
        }
        eksik[eo] = '\0';
        if (!tamam_c || !hata_exh) {
            char msg[320]; int o = 0; int once = 0;
            o += snprintf(msg + o, (size_t)((int)sizeof(msg) - o),
                          "esles exhaustive degil — eksik: [");
            if (!tamam_c) {
                o += snprintf(msg + o, (size_t)((int)sizeof(msg) - o),
                              "tamam"); once = 1;
            }
            if (!hata_exh) {
                o += snprintf(msg + o, (size_t)((int)sizeof(msg) - o),
                              "%s%s", once ? ", " : "", eo ? eksik : "hata");
            }
            snprintf(msg + o, (size_t)((int)sizeof(msg) - o), "]");
            tip_hata(tk, d, "M001", msg);
        }
        return;
    }
}

/* === Deyim tip kontrolu === */

/* C5 C.1: satirici_asm operandi icin izinli tip — yalniz kopyalanabilir
 * primitif (tamN, dtamN, mantiksal, karakter) + ham *T pointer.
 * Kesirli, metin, yapi, dizi, referans, tekkez, yetki: HAYIR (v1). */
static int asm_operand_tipi_uygun(const TipBilgisi *t) {
    if (!t) return 0;
    switch (t->kategori) {
        case TIP_TAM8:  case TIP_TAM16:  case TIP_TAM32:  case TIP_TAM64:
        case TIP_DTAM8: case TIP_DTAM16: case TIP_DTAM32: case TIP_DTAM64:
        case TIP_MANTIKSAL:
        case TIP_KARAKTER:
        case TIP_POINTER:
            return 1;
        default:
            return 0;
    }
}

static void tip_kontrol_deyim(TipKontrol *tk, const Dugum *d) {
    if (!d) return;

    switch (d->tip) {
        case DUGUM_DEGISKEN: {
            TipBilgisi *annot = NULL;
            TipBilgisi *deger_tip;
            if (d->veri.degisken.tip) {
                annot = ast_tip_to_bilgi(tk, d->veri.degisken.tip);
                /* Bidirectional: literal'lar annot context'inde cikarsanir */
                deger_tip = tip_belirle_beklenen(tk,
                    d->veri.degisken.deger, annot);
                /* Sabitsüre Spec V1 CT003: sabitsure<T> → T leak */
                if (deger_tip->kategori != TIP_HATA &&
                    annot->kategori != TIP_HATA &&
                    ct003_leak_kontrol(tk, d, deger_tip, annot)) {
                    /* hata raporlandı, atla */
                } else if (!tip_esit(annot, deger_tip) &&
                    deger_tip->kategori != TIP_HATA &&
                    annot->kategori != TIP_HATA) {
                    tip_hata(tk, d, "T001",
                             "degisken tip annot ile baslangic uyumsuz");
                /* DZ.3: annot N-bilinen ise deger de olmali. `else if` —
                 * tip zaten uyusmuyorsa ikinci bir hata yigmayiz. */
                } else {
                    dz_akis_kontrol(tk, d, annot, deger_tip);
                }
            } else {
                deger_tip = tip_belirle(tk, d->veri.degisken.deger);
            }
            TipBilgisi *son = annot ? annot : deger_tip;
            /* Linear Types Spec V1 + Capability Spec V1:
             * deger lineer baglamadan move ise tuket (tekkez VEYA yetki). */
            if (son && tip_lineer_mi(son)) {
                lineer_tuket_eger_baglamaysa(tk, d->veri.degisken.deger);
            }
            Sembol s;
            memset(&s, 0, sizeof(s));
            s.ad = d->veri.degisken.ad;
            s.ad_uzunluk = d->veri.degisken.ad_uzunluk;
            s.kategori = SEMBOL_DEGISKEN;
            s.tip = son;
            s.ast_dugumu = d;
            s.satir = d->satir;
            s.sutun = d->sutun;
            s.lineer_scope_seviyesi = tk->scope_seviyesi;
            if (sembol_ekle(tk->scope, tk->arena, &s) != 0) {
                tip_hata(tk, d, "T024", "degisken zaten tanimli");
            }
            break;
        }

        case DUGUM_ATAMA: {
            /* Hedef lvalue mi? (TANIMLAYICI, ERISIM, INDEKS) — D-248 (GAP-2):
             * güvensiz blokta *p (OP_DEREFERANS) da lvalue (ham pointer üzerinden
             * yazma; MMIO/heap). YALNIZ güvensiz (safe .kem etkilenmez). */
            const Dugum *hedef = d->veri.atama.hedef;
            int deref_lvalue = (hedef->tip == DUGUM_TEKLI &&
                                hedef->veri.tekli.op == OP_DEREFERANS &&
                                tk->guvensiz_baglam != 0);
            if (hedef->tip != DUGUM_TANIMLAYICI &&
                hedef->tip != DUGUM_ERISIM &&
                hedef->tip != DUGUM_INDEKS &&
                !deref_lvalue) {
                tip_hata(tk, d, "T022",
                         "atama hedefi lvalue olmali (tanimlayici/erisim/indeks"
                         "; *p yalniz guvensiz)");
            }
            TipBilgisi *ht = tip_belirle(tk, hedef);
            /* D-071 (Sınıf B, lambda yeniden-atama): işlev/lambda TİPLİ bir
             * değişken YENİDEN atanamaz (G004). KARMA closure temsili değere
             * bağlı: yakalamasız lambda/top-level fn → bare fn-ptr, yakalamalı
             * lambda → closure {fn,env}. Çağrı yeri ise statik `closure_mu`
             * bayrağına göre dispatch eder (bağlama anında sabitlenir). Farklı
             * yakalama-durumlu bir değerle yeniden atama → temsil uyumsuzluğu →
             * çağrıda bare-ptr'ı closure sanıp deref → SEGFAULT (kabul-ama-çöküyor).
             * Compile-time reddet (çökmezlik #1); programcı yeni bir `değişken`
             * ile bağlasın. (Tam değer-akışı desteği V2/D-072.) */
            if (hedef->tip == DUGUM_TANIMLAYICI &&
                ht->kategori == TIP_ISLEV) {
                tip_hata(tk, d, "G004",
                    "islev/lambda degiskeni yeniden atanamaz "
                    "(V1: KARMA closure temsili deger-bagimli; "
                    "yeni bir 'degisken' ile baglayin)");
                break;
            }
            /* Bidirectional: deger hedef tip context'inde */
            TipBilgisi *dt = tip_belirle_beklenen(tk, d->veri.atama.deger, ht);
            /* Sabitsüre Spec V1 CT003: sabitsure<T> → T leak */
            if (dt->kategori != TIP_HATA && ht->kategori != TIP_HATA &&
                ct003_leak_kontrol(tk, d, dt, ht)) {
                /* hata raporlandı */
            } else if (!tip_esit(ht, dt) &&
                ht->kategori != TIP_HATA && dt->kategori != TIP_HATA) {
                tip_hata(tk, d, "T001", "atama tipi uyumsuz");
            } else {
                dz_akis_kontrol(tk, d, ht, dt);   /* DZ.3 — atama */
            }
            break;
        }

        case DUGUM_VER: {
            /* D-304: blok-form lambda dönüş çıkarsaması — `ver <e>` tipini
             * KAYDET (aktif_donus_tipi'ye karşı kontrol yerine). İlk `ver`
             * kazanır; sonrakiler tutarlı varsayılır (well-typed lambda). */
            if (tk->lambda_blok_cikarsama && d->veri.ver.deger) {
                TipBilgisi *vt = tip_belirle(tk, d->veri.ver.deger);
                if (!tk->lambda_blok_donus && vt->kategori != TIP_HATA) {
                    tk->lambda_blok_donus = vt;
                }
                if (vt && tip_lineer_mi(vt)) {
                    lineer_tuket_eger_baglamaysa(tk, d->veri.ver.deger);
                }
                break;
            }
            if (!tk->aktif_donus_tipi) {
                tip_hata(tk, d, "T023", "ver islev govdesi disinda");
                break;
            }
            if (d->veri.ver.deger) {
                /* Bidirectional: deger donus tipi context'inde */
                TipBilgisi *deger = tip_belirle_beklenen(tk,
                    d->veri.ver.deger, tk->aktif_donus_tipi);
                /* Sabitsüre Spec V1 CT003: sabitsure<T> dönüş normal T'ye leak */
                if (deger->kategori != TIP_HATA &&
                    tk->aktif_donus_tipi->kategori != TIP_HATA &&
                    ct003_leak_kontrol(tk, d, deger, tk->aktif_donus_tipi)) {
                    /* hata raporlandı */
                } else if (tip_dizi_akis_uygun(tk->aktif_donus_tipi,
                                               deger) != 0) {
                    /* DZ.3 — `ver`: donus tipi N-bilenense deger de olmali */
                    dz_akis_kontrol(tk, d, tk->aktif_donus_tipi, deger);
                } else if (!tip_esit(deger, tk->aktif_donus_tipi) &&
                    deger->kategori != TIP_HATA &&
                    tk->aktif_donus_tipi->kategori != TIP_HATA) {
                    tip_hata(tk, d, "T020",
                             "ver tipi islev donus tipi ile uyumsuz");
                }
                /* Linear Types Spec V1 + Capability Spec V1:
                 * lineer baglama (tekkez VEYA yetki) ver ile cagirana devir → tuket */
                if (deger && tip_lineer_mi(deger)) {
                    lineer_tuket_eger_baglamaysa(tk, d->veri.ver.deger);
                }
            } else {
                /* ver; — donus tipi BOS olmali */
                if (tk->aktif_donus_tipi->kategori != TIP_BOS) {
                    tip_hata(tk, d, "T020",
                             "ver; gerek (donus tipi BOS olmali ya da deger ver)");
                }
            }
            break;
        }

        case DUGUM_EGER: {
            TipBilgisi *kosul = tip_belirle(tk, d->veri.eger.kosul);
            if (!tip_mantiksal_mi(kosul) && kosul->kategori != TIP_HATA) {
                tip_hata(tk, d, "T021", "eger kosulu mantiksal olmali");
            }
            /* Sabitsüre Spec V1 CT001 SABITSURE_IF_BRANCH:
             * gizli değer üzerinde dallanma timing kanalı açar. */
            if (tip_sabitsure_mi(kosul)) {
                tip_hata(tk, d, "CT001",
                    "eger kosulu sabitsure tipinde olamaz "
                    "(timing leak; ifsa(...) kullanin)");
            }
            /* D-311 / L-COND: dal-duyarlı lineer tüketim (bkz. lin_anlik_al). */
            LinAnlik anlik;
            lin_anlik_al(tk->scope, tk->arena, &anlik);
            int *then_durum = NULL, *else_durum = NULL;
            if (anlik.sayi > 0) {
                then_durum = (int *)arena_ayir(tk->arena,
                                               sizeof(int) * (size_t)anlik.sayi);
                else_durum = (int *)arena_ayir(tk->arena,
                                               sizeof(int) * (size_t)anlik.sayi);
            }

            tip_kontrol_deyim(tk, d->veri.eger.gozdoldur);
            if (then_durum) lin_durum_yaz(&anlik, then_durum);
            if (anlik.sayi > 0) lin_anlik_geri(&anlik);   /* dalları izole et */

            if (d->veri.eger.yan) {
                tip_kontrol_deyim(tk, d->veri.eger.yan);
            }
            if (else_durum) lin_durum_yaz(&anlik, else_durum);

            /* Birleştir: her lineer bağlama için iki dalın tüketimini karşılaştır. */
            for (int i = 0; i < anlik.sayi; i++) {
                int taban = anlik.taban[i];
                int t_then = then_durum ? (then_durum[i] > taban) : 0;
                int t_else = else_durum ? (else_durum[i] > taban) : 0;
                if (t_then && t_else) {
                    /* İki dal da tüketti → TOPLAMDA BİR tüketim (spec: OK).
                     * Eski sayaç burada 2'ye çıkıp L002 veriyordu. */
                    anlik.semboller[i]->lineer_tuketildi = taban + 1;
                } else if (t_then || t_else) {
                    /* Tam olarak bir dal tüketti → diğer yolda sızıntı. */
                    tip_hata(tk, d, "L005",
                        "kosullu dallar lineer tuketimde tutarsiz "
                        "(bir dal tuketiyor, digeri tuketmiyor)");
                    /* Tüketilmiş say: aksi halde scope sonunda ayrıca L001
                     * patlar ve tek kusur İKİ hata olarak raporlanır. */
                    anlik.semboller[i]->lineer_tuketildi = taban + 1;
                } else {
                    anlik.semboller[i]->lineer_tuketildi = taban;
                }
            }
            break;
        }

        case DUGUM_IKEN: {
            TipBilgisi *kosul = tip_belirle(tk, d->veri.iken.kosul);
            if (!tip_mantiksal_mi(kosul) && kosul->kategori != TIP_HATA) {
                tip_hata(tk, d, "T021", "iken kosulu mantiksal olmali");
            }
            /* Sabitsüre Spec V1 CT001 SABITSURE_WHILE_BRANCH */
            if (tip_sabitsure_mi(kosul)) {
                tip_hata(tk, d, "CT001",
                    "iken kosulu sabitsure tipinde olamaz "
                    "(loop iteration count = gizli = timing leak)");
            }
            {   /* D-312 / L-LOOP: döngü gövdesi DIŞ bir lineer bağlamayı
                 * tüketemez. Döngü 0 kez dönerse tüketilmez (sızıntı), ≥2 kez
                 * dönerse ÇİFT tüketim olur — ikisi de kaynağın (Dosya/Kilit)
                 * tek-kez disiplinini bozar. Ölçüldü: eskiden SESSİZCE geçiyordu.
                 * Kod: L005 (spec L006 tanımlamıyor; sınıf aynı — "tüketim yola
                 * bağlı"). Gövde İÇİNDE tanımlanan bağlamalar anlık görüntüde
                 * DEĞİLDİR → onlar normal L001/L002 disiplinine tabidir. */
                LinAnlik lp;
                lin_anlik_al(tk->scope, tk->arena, &lp);
                tip_kontrol_deyim(tk, d->veri.iken.govde);
                lineer_dongu_birlestir(tk, d, &lp, "iken");
            }
            break;
        }

        case DUGUM_ICIN: {
            TipBilgisi *kol = tip_belirle(tk, d->veri.icin.koleksiyon);
            TipBilgisi *eleman_tipi;
            if (kol->kategori == TIP_DIZI) {
                eleman_tipi = kol->veri.dizi.eleman;
            } else if (kol->kategori == TIP_HATA) {
                eleman_tipi = t_hata(tk);
            } else {
                tip_hata(tk, d, "T027", "icin koleksiyonu Dizi<T> olmali");
                eleman_tipi = t_hata(tk);
            }
            /* Yeni scope: icin degiskeni eleman_tipi olarak */
            Scope *eski = tk->scope;
            tk->scope_seviyesi++;
            tk->scope = scope_olustur(tk->arena, SCOPE_BLOK, eski);
            Sembol s;
            memset(&s, 0, sizeof(s));
            s.ad = d->veri.icin.degisken_adi;
            s.ad_uzunluk = d->veri.icin.degisken_adi_uzunluk;
            s.kategori = SEMBOL_DEGISKEN;
            s.tip = eleman_tipi;
            s.satir = d->satir;
            s.sutun = d->sutun;
            sembol_ekle(tk->scope, tk->arena, &s);
            LinAnlik lp2;   /* D-312 / L-LOOP — bkz. IKEN kolundaki not */
            lin_anlik_al(tk->scope, tk->arena, &lp2);
            tip_kontrol_deyim(tk, d->veri.icin.govde);
            lineer_dongu_birlestir(tk, d, &lp2, "i\xc3\xa7in");
            scope_lineer_kapanis_check(tk, tk->scope);
            tk->scope = eski;
            tk->scope_seviyesi--;
            break;
        }

        case DUGUM_ESLES: {
            /* Eşleş'in deger tipini belirle (secimlik<T> veya sonuc<T,H>) */
            TipBilgisi *dt = tip_belirle(tk, d->veri.esles.deger);
            /* C3: &Cesit referansı üzerinde eşleş — otomatik dereference
             * (recursive AST: çeşit ağacı referansla gezilir). */
            if (dt && dt->kategori == TIP_REFERANS && dt->veri.referans.hedef) {
                dt = dt->veri.referans.hedef;
            }
            /* Sabitsüre Spec V1 CT001 SABITSURE_MATCH: scrutinee sabitsure
             * olamaz — kol seçimi gizli bilgiyle dallanır. */
            if (tip_sabitsure_mi(dt)) {
                tip_hata(tk, d, "CT001",
                    "esles deger sabitsure tipinde olamaz "
                    "(kol secimi timing leak)");
            }
            /* D-312 / L-COND (eşleş): `eğer`in N-kollu genellemesi. Her kol
             * ANLIK GÖRÜNTÜDEN başlar; sonda birleştirilir. Ölçülen kusur ikili:
             *   tüm kollar tüketir → eskiden L002 (sayaç kol_sayısı kadar arttı),
             *   yalnız bazı kollar → eskiden SESSİZ (tüketmeyen yolda sızıntı). */
            LinAnlik m_anlik;
            lin_anlik_al(tk->scope, tk->arena, &m_anlik);
            int m_kol = d->veri.esles.kol_sayi;
            int *m_tuketen = NULL;   /* her lineer sembol için: kaç kol tüketti */
            if (m_anlik.sayi > 0) {
                m_tuketen = (int *)arena_ayir_sifir(tk->arena,
                                sizeof(int) * (size_t)m_anlik.sayi);
            }
            for (int i = 0; i < d->veri.esles.kol_sayi; i++) {
                const Dugum *kol = d->veri.esles.kollar[i];
                if (m_anlik.sayi > 0) lin_anlik_geri(&m_anlik);  /* kolu izole et */
                Scope *eski = tk->scope;
                tk->scope_seviyesi++;
                tk->scope = scope_olustur(tk->arena, SCOPE_BLOK, eski);

                /* Desen tanimlayici/yapici icindeki adlari scope'a ekle */
                const Dugum *desen = kol->veri.esles_kolu.desen;
                if (desen) {
                    if (desen->tip == DUGUM_DESEN_YAPI) {
                        /* D-318: `YapiAdi { alan1, alan2 }` destructuring.
                         * Semantik: desen yapiyi TUKETIR (lineer yapida sart —
                         * aksi halde hem yapi hem alanlar canli kalir = ayni
                         * kaynak iki kez); baglanan alanlar kendi tipleriyle
                         * kol scope'una girer, lineer olanlar kendi basina
                         * lineer baglama olur (L001 onlari ayrica izler). */
                        TipBilgisi *st = dt;
                        if (st && st->kategori == TIP_REFERANS)
                            st = st->veri.referans.hedef;
                        const Sembol *ysem = NULL;
                        if (st && st->kategori == TIP_YAPI) {
                            ysem = yapi_sembol_capraz_bul(tk, st->veri.yapi.ad,
                                                          st->veri.yapi.ad_uzunluk);
                        }
                        if (!ysem || st->veri.yapi.ad_uzunluk
                                        != desen->veri.desen_yapi.yapi_uz
                            || memcmp(st->veri.yapi.ad,
                                      desen->veri.desen_yapi.yapi_ad,
                                      (size_t)desen->veri.desen_yapi.yapi_uz) != 0) {
                            tip_hata(tk, desen, "T001",
                                "yapi deseni eslesen degerin tipiyle uyusmuyor");
                        } else {
                            /* V1 KURALI: TUM alanlar listelenmeli. Eksik alan,
                             * lineer yapida SESSIZ SIZINTI olurdu (listelenmeyen
                             * lineer alan hicbir yere baglanmaz ama yapi tuketilir).
                             * Rest-desen (`..`) YENI SOZDIZIMI -> icat edilmedi. */
                            int toplam = 0;
                            for (SembolLink *l = ysem->yapi_scope->bas; l;
                                 l = l->sonraki) {
                                if (l->sembol.kategori == SEMBOL_DEGISKEN) toplam++;
                            }
                            if (toplam != desen->veri.desen_yapi.alan_sayi) {
                                tip_hata(tk, desen, "T012",
                                    "yapi deseni TUM alanlari listelemeli "
                                    "(eksik alan sizabilir; `..` V1'de yok)");
                            }
                            for (int fi = 0;
                                 fi < desen->veri.desen_yapi.alan_sayi; fi++) {
                                const char *fad = desen->veri.desen_yapi.alan_adlar[fi];
                                int fuz = desen->veri.desen_yapi.alan_uzlar[fi];
                                const Sembol *alan =
                                    sembol_yapi_alani(ysem, fad, fuz);
                                if (!alan) {
                                    tip_hata(tk, desen, "T009",
                                             "yapi deseninde bilinmeyen alan");
                                    continue;
                                }
                                Sembol fs;
                                memset(&fs, 0, sizeof(fs));
                                fs.ad = fad;
                                fs.ad_uzunluk = fuz;
                                fs.kategori = SEMBOL_DEGISKEN;
                                fs.tip = alan->tip;
                                fs.ast_dugumu = desen;
                                fs.satir = desen->satir;
                                fs.sutun = desen->sutun;
                                fs.lineer_scope_seviyesi = tk->scope_seviyesi;
                                sembol_ekle(tk->scope, tk->arena, &fs);
                            }
                            /* Yapiyi TUKET (lineer ise); lineer degilse no-op. */
                            lineer_tuket_eger_baglamaysa(tk, d->veri.esles.deger);
                        }
                    } else if (desen->tip == DUGUM_DESEN_TANIMLAYICI) {
                        /* x => govde — x'in tipi dt */
                        Sembol s;
                        memset(&s, 0, sizeof(s));
                        s.ad = desen->veri.desen_tanimlayici.ad;
                        s.ad_uzunluk = desen->veri.desen_tanimlayici.ad_uzunluk;
                        s.kategori = SEMBOL_DEGISKEN;
                        s.tip = dt;
                        s.ast_dugumu = desen;
                        sembol_ekle(tk->scope, tk->arena, &s);
                    } else if (desen->tip == DUGUM_DESEN_YAPICI) {
                        /* değer(s) => govde — s'nin tipi secimlik<T>.iç tipi */
                        const char *yapici_ad = desen->veri.desen_yapici.ad;
                        int yapici_uz = desen->veri.desen_yapici.ad_uzunluk;
                        TipBilgisi *icerik_tipi = NULL;
                        /* "değer" -> dt secimlik ise iç */
                        if (yapici_uz == 6 &&
                            memcmp(yapici_ad, "de\xc4\x9f" "er", 6) == 0 &&
                            dt && dt->kategori == TIP_SECIMLIK) {
                            icerik_tipi = dt->veri.secimlik.ic;
                        } else if (yapici_uz == 5 &&
                                   memcmp(yapici_ad, "tamam", 5) == 0 &&
                                   dt && dt->kategori == TIP_SONUC) {
                            icerik_tipi = dt->veri.sonuc.deger;
                        } else if (yapici_uz == 4 &&
                                   memcmp(yapici_ad, "hata", 4) == 0 &&
                                   dt && dt->kategori == TIP_SONUC) {
                            icerik_tipi = dt->veri.sonuc.hata;
                        }
                        /* alt_desenler[0] tanimlayici ise bind et */
                        if (icerik_tipi &&
                            desen->veri.desen_yapici.sayi > 0) {
                            const Dugum *alt =
                                desen->veri.desen_yapici.alt_desenler[0];
                            if (alt && alt->tip == DUGUM_DESEN_TANIMLAYICI) {
                                Sembol s;
                                memset(&s, 0, sizeof(s));
                                s.ad = alt->veri.desen_tanimlayici.ad;
                                s.ad_uzunluk = alt->veri.desen_tanimlayici.ad_uzunluk;
                                s.kategori = SEMBOL_DEGISKEN;
                                s.tip = icerik_tipi;
                                s.ast_dugumu = alt;
                                sembol_ekle(tk->scope, tk->arena, &s);
                            }
                        }
                    } else if (desen->tip == DUGUM_DESEN_YOL) {
                        /* C2.7/C3: Cesit::Varyant[(a,b)] deseni — varyantı
                         * doğrula + payload alt-desenlerini varyant tiplerine
                         * bağla (X::V(a, b) => a,b payload tiplerinde). */
                        const Dugum *cd = (dt && dt->kategori == TIP_YAPI)
                            ? cesit_ara(tk, dt->veri.yapi.ad,
                                        dt->veri.yapi.ad_uzunluk) : NULL;
                        if (cd) {
                            int vi = cesit_varyant_index(cd,
                                desen->veri.desen_yol.varyant_ad,
                                desen->veri.desen_yol.varyant_uz);
                            if (vi < 0) {
                                tip_hata(tk, desen, "M002",
                                         "cesit varyanti bulunamadi");
                            } else {
                                int pn = cesit_varyant_payload_sayi(cd, vi);
                                int an = desen->veri.desen_yol.alt_sayi;
                                if (an > 0 && an != pn) {
                                    tip_hata(tk, desen, "M003",
                                        "cesit varyant payload desen sayisi "
                                        "uyumsuz");
                                }
                                for (int j = 0; j < an && j < pn; j++) {
                                    const Dugum *alt =
                                        desen->veri.desen_yol.alt_desenler[j];
                                    if (!alt ||
                                        alt->tip != DUGUM_DESEN_TANIMLAYICI) {
                                        continue;  /* joker vb. bind yok */
                                    }
                                    const Dugum *pt =
                                        cd->veri.cesit.varyant_payload_tipleri
                                            [vi][j];
                                    TipBilgisi *ptip;
                                    /* Generic çeşit (D-302): payload T'yi çeşit
                                     * generic scope'unda çöz sonra scrutinee
                                     * dt (Secim<tam32>) tip_arg'ından substitue. */
                                    if (cd->veri.cesit.tip_param_sayi > 0) {
                                        const Sembol *csem = sembol_bul(
                                            tk->scope ? tk->scope
                                                      : tk->global_scope,
                                            cd->veri.cesit.ad,
                                            cd->veri.cesit.ad_uzunluk);
                                        Scope *eski_s = tk->scope;
                                        if (csem && csem->yapi_scope)
                                            tk->scope = csem->yapi_scope;
                                        TipBilgisi *raw =
                                            ast_tip_to_bilgi(tk, pt);
                                        tk->scope = eski_s;
                                        ptip = substitusyon(tk, raw, csem, dt);
                                    } else {
                                        ptip = ast_tip_to_bilgi(tk, pt);
                                    }
                                    Sembol s;
                                    memset(&s, 0, sizeof(s));
                                    s.ad = alt->veri.desen_tanimlayici.ad;
                                    s.ad_uzunluk =
                                        alt->veri.desen_tanimlayici.ad_uzunluk;
                                    s.kategori = SEMBOL_DEGISKEN;
                                    s.tip = ptip;
                                    s.ast_dugumu = alt;
                                    sembol_ekle(tk->scope, tk->arena, &s);
                                }
                            }
                        }
                    }
                    /* DESEN_LITERAL, DESEN_JOKER: binding yok */
                }

                tip_kontrol_deyim(tk, kol->veri.esles_kolu.govde);
                scope_lineer_kapanis_check(tk, tk->scope);
                tk->scope = eski;
                tk->scope_seviyesi--;
                /* Bu kol dış bir lineer bağlamayı tüketti mi? */
                for (int k = 0; k < m_anlik.sayi; k++) {
                    if (m_anlik.semboller[k]->lineer_tuketildi > m_anlik.taban[k])
                        m_tuketen[k]++;
                }
            }
            /* D-312: kolları birleştir — ya HEPSİ tüketir ya HİÇBİRİ. */
            for (int k = 0; k < m_anlik.sayi; k++) {
                int taban = m_anlik.taban[k];
                if (m_tuketen[k] == 0) {
                    m_anlik.semboller[k]->lineer_tuketildi = taban;
                } else if (m_tuketen[k] == m_kol && m_kol > 0) {
                    /* Her yol tüketiyor → TOPLAMDA BİR tüketim. */
                    m_anlik.semboller[k]->lineer_tuketildi = taban + 1;
                } else {
                    tip_hata(tk, d, "L005",
                        "esles kollari lineer tuketimde tutarsiz "
                        "(bazi kollar tuketiyor, bazilari tuketmiyor)");
                    m_anlik.semboller[k]->lineer_tuketildi = taban + 1;
                }
            }
            /* C2.7: kapalı tip üzerinde kapsayıcılık denetimi. */
            esles_exhaustive_kontrol(tk, d, dt);
            break;
        }

        case DUGUM_GUVENSIZ:
            /* Aciklama yok-sayilir; blok ile ayni AMA unsafe-context
             * bayragi kurulur (C5 on-kosul #2): *T deref (G001) ve
             * satiriçi_asm (G002) yalniz bu baglam icinde gecerli.
             * Derinlik sayaci — ic ice guvensiz dogru calisir. */
            tk->guvensiz_baglam++;
            tip_kontrol_deyim(tk, d->veri.guvensiz.blok);
            tk->guvensiz_baglam--;
            break;

        /* === C5: satıriçi_asm tip kurallari ===
         * G002 — yalniz guvensiz blokta.
         * C.1 (lineer kara kutu) — operandlar yalniz kopyalanabilir
         * primitif (tamN/dtamN/mantiksal/karakter/ham *T); tekkez/yetki
         * DOGRUDAN GECEMEZ, cikti lineer OLAMAZ. Asm lineer-notr:
         * lineer yukumluluk ne tuketilir ne uretilir (yalniz ham adres
         * KEMGU seviyesinde cikarilip gecirilir).
         * C.2 — yalniz guvensiz-gate; capability ERTELENDI (borc notu). */
        case DUGUM_SATIRICI_ASM: {
            if (tk->guvensiz_baglam == 0) {
                tip_hata(tk, d, "G002",
                         "satirici_asm yalniz guvensiz blok "
                         "icinde kullanilabilir");
            }
            /* AS001: arch-tag hedef mimariyle uyusmali (llvm.h tek
             * kaynak; hedefe-duyarli triple C8'de). Yanlis hedefe
             * sessizce bozuk IR uretmek yerine derleme hatasi. */
            {
                const char *hm = llvm_hedef_mimari();   /* D-269: çalışma-zamanı hedef */
                int hm_uz = (int)strlen(hm);
                if (d->veri.satirici_asm.mimari &&
                    (d->veri.satirici_asm.mimari_uz != hm_uz ||
                     memcmp(d->veri.satirici_asm.mimari, hm,
                            (size_t)hm_uz) != 0)) {
                    tip_hata(tk, d, "AS001",
                             "satirici_asm mimari etiketi hedef "
                             "mimariyle uyusmuyor (--mimari ile hedef sec)");
                }
            }
            for (int i = 0; i < d->veri.satirici_asm.cikti_sayi; i++) {
                const char *ad = d->veri.satirici_asm.cikti_adlar[i];
                int uz = d->veri.satirici_asm.cikti_ad_uzlar[i];
                const Sembol *s = sembol_bul(tk->scope, ad, uz);
                if (!s) {
                    tip_hata(tk, d, "T002",
                             "asm cikti hedefi tanimsiz degisken");
                    continue;
                }
                if (s->kategori != SEMBOL_DEGISKEN &&
                    s->kategori != SEMBOL_PARAMETRE) {
                    tip_hata(tk, d, "T022",
                             "asm cikti hedefi degisken olmali (lvalue)");
                    continue;
                }
                if (s->tip && tip_lineer_mi(s->tip)) {
                    tip_hata(tk, d, "AS002",
                             "asm cikti hedefi lineer (tekkez/yetki) "
                             "olamaz — asm lineer deger uretemez");
                    continue;
                }
                if (!asm_operand_tipi_uygun(s->tip)) {
                    tip_hata(tk, d, "AS002",
                             "asm operandi yalniz kopyalanabilir primitif "
                             "(tamN/dtamN/mantiksal/karakter/*T)");
                }
            }
            for (int i = 0; i < d->veri.satirici_asm.girdi_sayi; i++) {
                TipBilgisi *t = tip_belirle(tk,
                    d->veri.satirici_asm.girdi_ifadeler[i]);
                if (!t || t->kategori == TIP_HATA) continue;
                if (tip_lineer_mi(t)) {
                    tip_hata(tk, d, "AS002",
                             "lineer (tekkez/yetki) deger asm'e dogrudan "
                             "gecemez — once ham adres cikar");
                    continue;
                }
                if (!asm_operand_tipi_uygun(t)) {
                    tip_hata(tk, d, "AS002",
                             "asm operandi yalniz kopyalanabilir primitif "
                             "(tamN/dtamN/mantiksal/karakter/*T)");
                }
            }
            break;
        }

        case DUGUM_BLOK: {
            Scope *eski = tk->scope;
            tk->scope_seviyesi++;
            tk->scope = scope_olustur(tk->arena, SCOPE_BLOK, eski);
            for (int i = 0; i < d->veri.blok.sayi; i++) {
                tip_kontrol_deyim(tk, d->veri.blok.deyimler[i]);
            }
            /* Linear Types Spec V1 LR-3: bolge (blok scope) kapanirken
             * tum lineer baglamalar tuketilmis olmali (L001 / LR001). */
            scope_lineer_kapanis_check(tk, tk->scope);
            tk->scope = eski;
            tk->scope_seviyesi--;
            break;
        }

        case DUGUM_IFADE_DEYIMI:
            tip_belirle(tk, d->veri.ifade_deyimi.ifade);
            break;

        case DUGUM_HATA:
            break;

        default:
            /* Bir ifade gibi davran (defansif) */
            tip_belirle(tk, d);
            break;
    }
}

/* === Tanim tip kontrolu === */

static void tip_kontrol_tanim(TipKontrol *tk, const Dugum *d) {
    if (!d) return;

    /* DISA -> ic tanim */
    if (d->tip == DUGUM_DISA) {
        if (d->veri.disa.tanim) tip_kontrol_tanim(tk, d->veri.disa.tanim);
        return;
    }

    switch (d->tip) {
        case DUGUM_ISLEV: {
            /* Tek-gecis ad cozumu on-kosulu: islev sembolu TANIMLANDIGI
             * scope'ta aranir (tk->scope — modul uyesi icin DUGUM_MODUL
             * case'i modul scope'unu kurdu). Onceki durum: yalniz
             * tk->global_scope'a bakiliyordu -> modul islev govdeleri
             * HIC denetlenmiyordu (sembol modul scope'unda, sessiz erken
             * donus) ya da ayni adli global ikizin imzasina karsi
             * denetleniyordu (arity farkinda OOB okuma). */
            const Sembol *islev_sem = sembol_bul_yerel(tk->scope,
                d->veri.islev.ad, d->veri.islev.ad_uzunluk);
            if (!islev_sem || !islev_sem->tip ||
                islev_sem->tip->kategori != TIP_ISLEV) return;
            /* Cift-tanim ikizinde (T024 zaten raporlandi) yanlis imzaya
             * karsi denetimi ve parametre dizisi OOB'sini onle. Built-in
             * golgelemesinde (ast_dugumu NULL) arity farki ayni OOB'yi
             * tetiklerdi — sayilar uyusmuyorsa govdeyi atla. */
            if (islev_sem->ast_dugumu && islev_sem->ast_dugumu != d) return;
            if (islev_sem->tip->veri.islev.param_sayi !=
                d->veri.islev.param_sayi) return;

            Scope *eski = tk->scope;
            TipBilgisi *eski_donus = tk->aktif_donus_tipi;
            tk->scope_seviyesi++;
            /* Govde scope'unun parent'i tanimlandigi scope: modul
             * uyesinde ciplak adlar once kardesleri gorur (lexical). */
            tk->scope = scope_olustur(tk->arena, SCOPE_ISLEV, eski);
            tk->aktif_donus_tipi = islev_sem->tip->veri.islev.donus;

            /* Generic params'i govde scope'una ekle */
            for (int i = 0; i < d->veri.islev.tip_param_sayi; i++) {
                const char *t_ad = d->veri.islev.tip_paramlar[i];
                int t_uz = (int)strlen(t_ad);
                Sembol gp;
                memset(&gp, 0, sizeof(gp));
                gp.ad = t_ad;
                gp.ad_uzunluk = t_uz;
                gp.kategori = SEMBOL_GENERIC_PARAM;
                gp.tip = tip_olustur_generic_param(tk->arena, t_ad, t_uz);
                sembol_ekle(tk->scope, tk->arena, &gp);
            }

            /* Parametreleri scope'a ekle */
            for (int i = 0; i < d->veri.islev.param_sayi; i++) {
                const Dugum *p = d->veri.islev.parametreler[i];
                Sembol s;
                memset(&s, 0, sizeof(s));
                s.ad = p->veri.parametre.ad;
                s.ad_uzunluk = p->veri.parametre.ad_uzunluk;
                s.kategori = SEMBOL_PARAMETRE;
                s.tip = islev_sem->tip->veri.islev.parametreler[i];
                s.ast_dugumu = p;
                s.satir = p->satir;
                s.sutun = p->sutun;
                if (sembol_ekle(tk->scope, tk->arena, &s) != 0) {
                    tip_hata(tk, p, "T024", "parametre adi cakismasi");
                }
            }

            /* G005: bu islev govdesi icin forward DFA escape analizi calistir.
             * Lambda case (DUGUM_LAMBDA) ESC_CAGIRAN sorgulayip kacan yakalayan
             * closure'i reddedebilsin. Per-islev: alloc/serbest dengeli (ASan
             * temiz); ic-ice islev tanimi yok (lambda ifade-duzeyinde) ama
             * aktif_escape yine de save/restore edilir. escape.c'nin mevcut hatti
             * (ana.c'de cagrilmiyordu) burada ilk kez tuketilir. */
            EscapeAnaliz ea;
            const struct EscapeAnaliz *eski_escape = tk->aktif_escape;
            escape_baslat(&ea, tk->arena);
            escape_analiz_islev(&ea, d);
            tk->aktif_escape = &ea;

            /* D-254 çıplak işlev: gövde örtük güvensiz-bağlam. Çıplak = güvensiz-tier
             * primitive (region-prologue'suz, ham pointer + küresel ile allocator
             * yazımı için) → gövdesi explicit `güvensiz {}` gerektirmez. Kırılmazlık
             * korunur: normal güvenli kod çıplak'ı kazara kullanamaz (opt-in keyword). */
            int ciplak_govde = d->veri.islev.ciplak_mi;
            if (ciplak_govde) { tk->guvensiz_baglam++; tk->ciplak_baglam++; }  /* D-257: call-rule */

            /* Govdeyi kontrol et */
            if (d->veri.islev.govde) {
                tip_kontrol_deyim(tk, d->veri.islev.govde);
            }

            if (ciplak_govde) { tk->guvensiz_baglam--; tk->ciplak_baglam--; }

            tk->aktif_escape = eski_escape;
            escape_serbest(&ea);

            /* Linear Types Spec V1: lineer parametreler govdede tuketilmeli */
            scope_lineer_kapanis_check(tk, tk->scope);

            tk->aktif_donus_tipi = eski_donus;
            tk->scope = eski;
            tk->scope_seviyesi--;
            break;
        }

        case DUGUM_SABIT: {
            /* Pre-populate'te sembol eklendi. Simdi deger kontrolu.
             * Bidirectional: literal'lar annot context'inde cikarsanir. */
            TipBilgisi *annot = ast_tip_to_bilgi(tk, d->veri.sabit.tip);
            TipBilgisi *deger = tip_belirle_beklenen(tk,
                d->veri.sabit.deger, annot);
            if (!tip_esit(deger, annot) &&
                deger->kategori != TIP_HATA &&
                annot->kategori != TIP_HATA) {
                tip_hata(tk, d, "T001", "sabit deger tip annot ile uyumsuz");
            }
            break;
        }

        case DUGUM_YAPI:
            /* Pre-populate yeterli (alan tipleri orada cozumlendi) */
            break;

        case DUGUM_OZELLIK: {
            /* Ozellik gövdesindeki default impl'leri tip kontrol et.
             * Method imzasi olanlar (govdesiz) atlanir. */
            for (int i = 0; i < d->veri.ozellik.uye_sayi; i++) {
                const Dugum *m = d->veri.ozellik.uyeler[i];
                if (!m || m->tip != DUGUM_ISLEV) continue;
                if (!m->veri.islev.govde) continue;  /* imza */

                /* Method gövdesi icin scope kur */
                Scope *eski = tk->scope;
                TipBilgisi *eski_donus = tk->aktif_donus_tipi;
                tk->scope = scope_olustur(tk->arena, SCOPE_ISLEV,
                                          tk->global_scope);
                tk->aktif_donus_tipi = m->veri.islev.donus_tipi
                    ? ast_tip_to_bilgi(tk, m->veri.islev.donus_tipi)
                    : tip_olustur_basit(tk->arena, TIP_BOS);

                for (int j = 0; j < m->veri.islev.param_sayi; j++) {
                    const Dugum *p = m->veri.islev.parametreler[j];
                    Sembol s;
                    memset(&s, 0, sizeof(s));
                    s.ad = p->veri.parametre.ad;
                    s.ad_uzunluk = p->veri.parametre.ad_uzunluk;
                    s.kategori = SEMBOL_PARAMETRE;
                    s.tip = ast_tip_to_bilgi(tk, p->veri.parametre.tip);
                    s.ast_dugumu = p;
                    sembol_ekle(tk->scope, tk->arena, &s);
                }
                /* D-256: çıplak method gövdesi = örtük güvensiz-bağlam (standalone
                 * DUGUM_ISLEV yolundaki grant ile birebir; codegen zaten çıplak-method'u
                 * prologue-skip ile emit eder — checker↔codegen tutarlılığı). */
                int ciplak_m = m->veri.islev.ciplak_mi;
                if (ciplak_m) { tk->guvensiz_baglam++; tk->ciplak_baglam++; }  /* D-257 */
                tip_kontrol_deyim(tk, m->veri.islev.govde);
                if (ciplak_m) { tk->guvensiz_baglam--; tk->ciplak_baglam--; }
                tk->aktif_donus_tipi = eski_donus;
                tk->scope = eski;
            }
            break;
        }

        case DUGUM_UYGULA: {
            /* uygula gövdesindeki islev tanimlarini tip-kontrol et.
             * Generic params (uygula<T> Tip<T>): kendi scope'larina T eklenir. */
            Scope *eski = tk->scope;
            tk->scope = scope_olustur(tk->arena, SCOPE_BLOK, tk->global_scope);
            /* Generic param'lar: T -> TIP_GENERIC_PARAM */
            for (int i = 0; i < d->veri.uygula.tip_param_sayi; i++) {
                const char *t_ad = d->veri.uygula.tip_paramlar[i];
                int t_uz = (int)strlen(t_ad);
                Sembol gp;
                memset(&gp, 0, sizeof(gp));
                gp.ad = t_ad;
                gp.ad_uzunluk = t_uz;
                gp.kategori = SEMBOL_GENERIC_PARAM;
                gp.tip = tip_olustur_generic_param(tk->arena, t_ad, t_uz);
                sembol_ekle(tk->scope, tk->arena, &gp);
            }

            /* uygula hedef tipi (kendin'in tipi olacak) */
            TipBilgisi *hedef_t = ast_tip_to_bilgi(tk, d->veri.uygula.tip);

            /* Her metodu kontrol et */
            for (int i = 0; i < d->veri.uygula.islev_sayi; i++) {
                const Dugum *m = d->veri.uygula.islevler[i];
                if (!m || m->tip != DUGUM_ISLEV) continue;
                if (!m->veri.islev.govde) continue;

                Scope *eski_m = tk->scope;
                TipBilgisi *eski_donus = tk->aktif_donus_tipi;
                tk->scope = scope_olustur(tk->arena, SCOPE_ISLEV, tk->scope);
                tk->aktif_donus_tipi = m->veri.islev.donus_tipi
                    ? ast_tip_to_bilgi(tk, m->veri.islev.donus_tipi)
                    : tip_olustur_basit(tk->arena, TIP_BOS);

                for (int j = 0; j < m->veri.islev.param_sayi; j++) {
                    const Dugum *p = m->veri.islev.parametreler[j];
                    Sembol s;
                    memset(&s, 0, sizeof(s));
                    s.ad = p->veri.parametre.ad;
                    s.ad_uzunluk = p->veri.parametre.ad_uzunluk;
                    s.kategori = SEMBOL_PARAMETRE;
                    if (p->veri.parametre.kendin_mi) {
                        /* kendin parametresi: tipi uygula.tip (referans olabilir) */
                        TipBilgisi *t = hedef_t;
                        if (p->veri.parametre.referans_mi) {
                            t = tip_olustur_referans(tk->arena, hedef_t,
                                                     p->veri.parametre.degisken_mi);
                        }
                        s.tip = t;
                    } else {
                        s.tip = ast_tip_to_bilgi(tk, p->veri.parametre.tip);
                    }
                    s.ast_dugumu = p;
                    sembol_ekle(tk->scope, tk->arena, &s);
                }
                /* D-256: çıplak method gövdesi = örtük güvensiz-bağlam (özellik yolu +
                 * standalone DUGUM_ISLEV ile birebir; checker↔codegen tutarlılığı). */
                int ciplak_m = m->veri.islev.ciplak_mi;
                if (ciplak_m) { tk->guvensiz_baglam++; tk->ciplak_baglam++; }  /* D-257 */
                tip_kontrol_deyim(tk, m->veri.islev.govde);
                if (ciplak_m) { tk->guvensiz_baglam--; tk->ciplak_baglam--; }
                tk->aktif_donus_tipi = eski_donus;
                tk->scope = eski_m;
            }
            tk->scope = eski;
            break;
        }

        case DUGUM_KULLAN: {
            /* A: yeni bicim (tek-segment / secili / alias) faz-2'de
             * (kullan_baglari_kur) islendi — burada is yok. Asagisi
             * LEGACY cok-segment duzlestirme yoludur (drivers/ +
             * test/crossfile tuketicileri icin korunur). */
            if (kullan_yeni_bicim_mi(d)) break;

            /* Yol formati: "x::y::z" -> "x/y/z.kem"
             * Arama sirasi: cari dizin, "stdlib/" prefix'i. */
            const char *y = d->veri.kullan.yol;
            int yu = d->veri.kullan.yol_uzunluk;
            if (!y || yu <= 0) break;

            /* "::" -> "/" donusumu, sonuna ".kem" ekle */
            char dosya_yolu[512];
            int o = 0;
            for (int i = 0; i < yu && o + 6 < (int)sizeof(dosya_yolu); i++) {
                if (i + 1 < yu && y[i] == ':' && y[i + 1] == ':') {
                    dosya_yolu[o++] = '/';
                    i++;
                } else {
                    dosya_yolu[o++] = y[i];
                }
            }
            /* .kem uzantisi */
            const char *uzanti = ".kem";
            for (int k = 0; k < 4 && o + 1 < (int)sizeof(dosya_yolu); k++) {
                dosya_yolu[o++] = uzanti[k];
            }
            dosya_yolu[o] = '\0';

            /* Duplicate kontrol */
            for (YuklenmisModul *m = tk->yuklenmisler; m; m = m->sonraki) {
                if (m->yol_uz == o && memcmp(m->yol, dosya_yolu, (size_t)o) == 0) {
                    return;  /* zaten yuklu */
                }
            }

            /* Dosyayi yukle */
            FILE *fp = fopen(dosya_yolu, "rb");
            if (!fp) {
                tip_hata(tk, d, "T040",
                    "kullan: modül dosyası bulunamadı");
                break;
            }
            fseek(fp, 0, SEEK_END);
            long boyut = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            if (boyut <= 0) { fclose(fp); break; }
            char *kaynak = (char *)arena_ayir(tk->arena, (size_t)boyut + 1);
            if (!kaynak) { fclose(fp); break; }
            fread(kaynak, 1, (size_t)boyut, fp);
            kaynak[boyut] = '\0';
            fclose(fp);

            /* Yüklenmis listesine ekle (duplicate engelleme) */
            YuklenmisModul *ym = (YuklenmisModul *)arena_ayir_sifir(
                tk->arena, sizeof(YuklenmisModul));
            if (ym) {
                char *yol_kopya = (char *)arena_ayir(tk->arena, (size_t)o + 1);
                memcpy(yol_kopya, dosya_yolu, (size_t)o + 1);
                ym->yol = yol_kopya;
                ym->yol_uz = o;
                ym->sonraki = tk->yuklenmisler;
                tk->yuklenmisler = ym;
            }

            /* Parse + tip-kontrol modulu */
            Lexer l;
            lexer_baslat(&l, kaynak, dosya_yolu);
            Parser p;
            parser_baslat(&p, &l, tk->arena, dosya_yolu, kaynak);
            Dugum *mprog = parser_calistir(&p);
            if (mprog && p.hata_sayisi == 0) {
                /* Üst düzey üyeleri pre-populate + tanim-kontrol */
                tip_kontrol_program(tk, mprog);
            }
            break;
        }

        case DUGUM_MODUL: {
            /* T016 fix: uyeleri MODUL SCOPE baglaminda kontrol et —
             * boylece kardes islevlere ciplak-ad cagrilar (kare()) ve
             * modul-yerel tipler cozulur. Modul sembolu pre_populate'da
             * mevcut scope'a (parent) eklendi; scope'unu bulup gir.
             * A: dosya-modul kanonigi builtin_scope'ta (gizli). */
            const Sembol *ms = d->veri.modul.dosya_modulu
                ? sembol_bul_yerel(tk->builtin_scope,
                      d->veri.modul.ad, d->veri.modul.ad_uzunluk)
                : sembol_bul_yerel(tk->scope,
                      d->veri.modul.ad, d->veri.modul.ad_uzunluk);
            Scope *eski = tk->scope;
            if (ms && ms->kategori == SEMBOL_MODUL && ms->modul_scope) {
                tk->scope = ms->modul_scope;
            }
            for (int i = 0; i < d->veri.modul.sayi; i++) {
                tip_kontrol_tanim(tk, d->veri.modul.uyeler[i]);
            }
            tk->scope = eski;
            break;
        }

        case DUGUM_HATA:
            break;

        default:
            break;
    }
}

/* === Ana fonksiyon === */

/* ===========================================================================
 * DZ Spec V1 Asama (b) — BUYUTUCU-PARAMETRE ETKI ANALIZI (DZ006)
 * ---------------------------------------------------------------------------
 * DZ.5'te olculen delik: diziler heap'te ve REFERANSLA gectigi icin, N'in
 * silindigi bir cagrinin icinde yapilan buyutme CAGIRANA GORUNUR:
 *
 *     işlev buyut(xs: Dizi<tam32>) { dizi_ekle(xs, 99); }
 *     değişken W: Dizi<tam32, 2> = [1, 2];
 *     buyut(W);      // N silinir → izinli; W artik 3 elemanli → N YALAN
 *
 * Cozum: bir islevin dizi parametresi
 *   (dogrudan)  govdede dizi_ekle/dizi_kapasite_ayarla'nin ILK argumani ise
 *   (transitif) buyutucu bir parametreye ARGUMAN olarak iletiliyorsa
 * BUYUTUCU'dur. Cagri grafiginde fixpoint ile yayilir. ALIAS ANALIZI GEREKMEZ
 * — yalnizca parametre adinin dogrudan gecirilmesi izlenir.
 *
 * YON: bu analiz over-approximate etmelidir. Bir buyutucuyu KACIRMAK unsound
 * (delik acik kalir); FAZLADAN isaretlemek yalnizca gecerli kodu reddeder
 * (loud). Bu yuzden gezici TUM konteyner dugumlerini acikca listeler ve
 * `default:` dali yalnizca COCUKSUZ yaprak dugumler icindir.
 * =========================================================================== */

static DzBuyutucu *dz_kayit_bul(TipKontrol *tk, const Dugum *islev) {
    for (DzBuyutucu *b = tk->dz_buyutucular; b; b = b->sonraki) {
        if (b->islev == islev) return b;
    }
    return NULL;
}

/* Ada gore ARA — transitif adimda kullanilir. Ayni ad birden fazla islevde
 * varsa (modul/uygula) HEPSI taranir ve bayraklar OR'lanir: over-approximate
 * = guvenli yon. */
static int dz_ad_buyutucu_mu(TipKontrol *tk, const char *ad, int ad_uz,
                             int param_i) {
    for (DzBuyutucu *b = tk->dz_buyutucular; b; b = b->sonraki) {
        if (b->ad_uz == ad_uz && memcmp(b->ad, ad, (size_t)ad_uz) == 0 &&
            param_i < b->param_sayi && b->bayrak[param_i]) {
            return 1;
        }
    }
    return 0;
}

/* Dugum, `ad` adli parametreye dogrudan basvuran bir tanimlayici mi? */
static int dz_param_referansi_mi(const Dugum *d, const char *ad, int ad_uz) {
    if (!d || d->tip != DUGUM_TANIMLAYICI) return 0;
    return d->veri.tanimlayici.uzunluk == ad_uz &&
           memcmp(d->veri.tanimlayici.metin, ad, (size_t)ad_uz) == 0;
}

/* Govdeyi gez; `kayit`in parametrelerinden buyutulenleri isaretle.
 * Donus: en az bir bayrak DEGISTIYSE 1 (fixpoint sonlandirmasi icin). */
static int dz_govde_tara(TipKontrol *tk, const Dugum *d, DzBuyutucu *kayit,
                         Dugum *const *paramlar) {
    if (!d) return 0;
    int degisti = 0;

    if (d->tip == DUGUM_CAGRI && d->veri.cagri.hedef &&
        d->veri.cagri.hedef->tip == DUGUM_TANIMLAYICI) {
        const char *fa = d->veri.cagri.hedef->veri.tanimlayici.metin;
        int fu = d->veri.cagri.hedef->veri.tanimlayici.uzunluk;
        int buyuten_builtin =
            (fu == 9  && memcmp(fa, "dizi_ekle", 9) == 0) ||
            (fu == 20 && memcmp(fa, "dizi_kapasite_ayarla", 20) == 0);
        for (int i = 0; i < kayit->param_sayi; i++) {
            if (kayit->bayrak[i]) continue;
            const Dugum *p = paramlar[i];
            if (!p || p->tip != DUGUM_PARAMETRE) continue;
            const char *pa = p->veri.parametre.ad;
            int pu = p->veri.parametre.ad_uzunluk;
            /* (dogrudan) buyuten built-in'in ILK argumani */
            if (buyuten_builtin && d->veri.cagri.sayi >= 1 &&
                dz_param_referansi_mi(d->veri.cagri.argumanlar[0], pa, pu)) {
                kayit->bayrak[i] = 1; degisti = 1; continue;
            }
            /* (transitif) buyutucu bir parametreye arguman olarak iletiliyor */
            if (!buyuten_builtin) {
                for (int j = 0; j < d->veri.cagri.sayi; j++) {
                    if (dz_param_referansi_mi(d->veri.cagri.argumanlar[j],
                                              pa, pu) &&
                        dz_ad_buyutucu_mu(tk, fa, fu, j)) {
                        kayit->bayrak[i] = 1; degisti = 1; break;
                    }
                }
            }
        }
    }

    /* --- Cocuklara in. Konteyner dugumler ACIKCA listelenir; atlanan bir tip
     * SESSIZ soundness acigi demektir (bkz. yukaridaki YON notu). --- */
    #define DZ_IN(x) do { degisti |= dz_govde_tara(tk, (x), kayit, paramlar); } while (0)
    switch (d->tip) {
        case DUGUM_BLOK:
            for (int i = 0; i < d->veri.blok.sayi; i++) DZ_IN(d->veri.blok.deyimler[i]);
            break;
        case DUGUM_IFADE_DEYIMI: DZ_IN(d->veri.ifade_deyimi.ifade); break;
        case DUGUM_DEGISKEN:     DZ_IN(d->veri.degisken.deger); break;
        case DUGUM_ATAMA:
            DZ_IN(d->veri.atama.hedef); DZ_IN(d->veri.atama.deger); break;
        case DUGUM_VER:          DZ_IN(d->veri.ver.deger); break;
        case DUGUM_EGER:
            DZ_IN(d->veri.eger.kosul); DZ_IN(d->veri.eger.gozdoldur);
            DZ_IN(d->veri.eger.yan); break;
        case DUGUM_IKEN:
            DZ_IN(d->veri.iken.kosul); DZ_IN(d->veri.iken.govde); break;
        case DUGUM_ICIN:
            DZ_IN(d->veri.icin.koleksiyon); DZ_IN(d->veri.icin.govde); break;
        case DUGUM_ESLES:
            DZ_IN(d->veri.esles.deger);
            for (int i = 0; i < d->veri.esles.kol_sayi; i++) DZ_IN(d->veri.esles.kollar[i]);
            break;
        case DUGUM_ESLES_KOLU:   DZ_IN(d->veri.esles_kolu.govde); break;
        case DUGUM_GUVENSIZ:     DZ_IN(d->veri.guvensiz.blok); break;
        case DUGUM_SATIRICI_ASM:
            for (int i = 0; i < d->veri.satirici_asm.girdi_sayi; i++)
                DZ_IN(d->veri.satirici_asm.girdi_ifadeler[i]);
            break;
        case DUGUM_IKILI:
            DZ_IN(d->veri.ikili.sol); DZ_IN(d->veri.ikili.sag); break;
        case DUGUM_TEKLI:        DZ_IN(d->veri.tekli.operand); break;
        case DUGUM_CAGRI:
            DZ_IN(d->veri.cagri.hedef);
            for (int i = 0; i < d->veri.cagri.sayi; i++) DZ_IN(d->veri.cagri.argumanlar[i]);
            break;
        case DUGUM_ERISIM:       DZ_IN(d->veri.erisim.nesne); break;
        case DUGUM_INDEKS:
            DZ_IN(d->veri.indeks.nesne); DZ_IN(d->veri.indeks.indeks); break;
        case DUGUM_LAMBDA:       DZ_IN(d->veri.lambda.govde); break;
        case DUGUM_YAPI_OLUSTUR:
            for (int i = 0; i < d->veri.yapi_olustur.alan_sayi; i++)
                DZ_IN(d->veri.yapi_olustur.alanlar[i]);
            break;
        case DUGUM_ALAN_ATAMA:   DZ_IN(d->veri.alan_atama.deger); break;
        case DUGUM_DIZI_OLUSTUR:
            for (int i = 0; i < d->veri.dizi_olustur.sayi; i++)
                DZ_IN(d->veri.dizi_olustur.elemanlar[i]);
            break;
        case DUGUM_KULLAN_IFADE: DZ_IN(d->veri.kullan_ifade.operand); break;
        case DUGUM_IMHA_IFADE:   DZ_IN(d->veri.imha_ifade.operand); break;
        case DUGUM_TIP_DONUSTUR: DZ_IN(d->veri.tip_donustur.kaynak); break;
        default:
            /* Yaprak (literal/tanimlayici/yol/desen/tip dugumleri) — cagri
             * icermez. Yeni bir KONTEYNER dugum tipi eklenirse buraya da
             * eklenmelidir; aksi halde icindeki buyutme GORULMEZ. */
            break;
    }
    #undef DZ_IN
    return degisti;
}

/* Program uyelerinden islev kayitlarini topla (modul/dışa/uygula icine iner). */
static void dz_kayitlari_topla(TipKontrol *tk, Dugum *const *uyeler, int sayi) {
    for (int i = 0; i < sayi; i++) {
        const Dugum *u = uyeler[i];
        if (!u) continue;
        switch (u->tip) {
            case DUGUM_ISLEV: {
                if (!u->veri.islev.govde) break;      /* yalniz imza */
                if (dz_kayit_bul(tk, u)) break;
                DzBuyutucu *b = (DzBuyutucu *)arena_ayir_sifir(tk->arena,
                                                    sizeof(DzBuyutucu));
                if (!b) break;
                b->islev = u;
                b->ad = u->veri.islev.ad;
                b->ad_uz = u->veri.islev.ad_uzunluk;
                b->param_sayi = u->veri.islev.param_sayi;
                if (b->param_sayi > 0) {
                    b->bayrak = (unsigned char *)arena_ayir_sifir(tk->arena,
                                    (size_t)b->param_sayi);
                    if (!b->bayrak) break;
                }
                b->sonraki = tk->dz_buyutucular;
                tk->dz_buyutucular = b;
                break;
            }
            case DUGUM_MODUL:
                dz_kayitlari_topla(tk, u->veri.modul.uyeler, u->veri.modul.sayi);
                break;
            case DUGUM_DISA:
                dz_kayitlari_topla(tk, &u->veri.disa.tanim, 1);
                break;
            case DUGUM_UYGULA:
                dz_kayitlari_topla(tk, u->veri.uygula.islevler,
                                   u->veri.uygula.islev_sayi);
                break;
            default: break;
        }
    }
}

/* Fixpoint: bayraklar durulana kadar tekrarla. Her tur en az bir bayragi
 * 0→1 yapar; toplam bayrak sayisi sonlu → sonlanir. Ust sinir guvenlik agi. */
static void dz_buyutucu_analiz(TipKontrol *tk, const Dugum *program) {
    tk->dz_buyutucular = NULL;
    dz_kayitlari_topla(tk, program->veri.program.uyeler,
                       program->veri.program.sayi);
    int tur = 0;
    int degisti = 1;
    while (degisti && tur < 64) {
        degisti = 0;
        for (DzBuyutucu *b = tk->dz_buyutucular; b; b = b->sonraki) {
            if (b->param_sayi <= 0) continue;
            degisti |= dz_govde_tara(tk, b->islev->veri.islev.govde, b,
                                     b->islev->veri.islev.parametreler);
        }
        tur++;
    }
}

void tip_kontrol_program(TipKontrol *tk, const Dugum *program) {
    if (!program || program->tip != DUGUM_PROGRAM) return;
    pre_populate(tk, program);
    /* A faz-2: tum moduller kayitli — kullan baglarini kur (dongusel
     * import bildirim sirasi onemsiz). */
    kullan_baglari_kur(tk, program);
    /* DZ Spec V1 Asama (b): buyutucu-parametre fixpoint'i. Cagri yerleri
     * ziyaret edilmeden ONCE tamamlanmali (DZ006 bu tabloyu okur). */
    dz_buyutucu_analiz(tk, program);
    for (int i = 0; i < program->veri.program.sayi; i++) {
        tip_kontrol_tanim(tk, program->veri.program.uyeler[i]);
    }
}
