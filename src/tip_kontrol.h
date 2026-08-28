#ifndef KEMGU_TIP_KONTROL_H
#define KEMGU_TIP_KONTROL_H

#include "ast.h"
#include "tip.h"
#include "sembol.h"
#include "arena.h"

/*
 * KEMGU Tip Kontrolu
 * ==================
 *
 * AST visitor pattern. Her ifade dugumu icin tip belirler.
 *
 * Hata kodlari (T001-T020):
 *   T001: tip uyumsuzlugu (genel)
 *   T002: tanimsiz sembol
 *   T003: sayisal tip bekleniyor
 *   T004: mantiksal tip bekleniyor
 *   T005: tamsayi tip bekleniyor
 *   T006: islev tipi bekleniyor (cagri icin)
 *   T007: yapi tipi bekleniyor (alan erisimi icin)
 *   T008: dizi tipi bekleniyor (indeks icin)
 *   T009: alan bulunamadi (yapi.alan)
 *   T010: arguman sayisi uyumsuz
 *   T011: bilinmeyen tip (ast tip dugumu cevirme)
 *   T012: yapi olusturmasi alan eksik/fazla
 *   T013: dizi elemanlari farkli tipte
 *   T014: bos dizi context bekleniyor (ADIM 11.5)
 *   T015: lambda parametre tip annot eksik
 *   T016: modul/yol cozumlemesi basarisiz
 *   T017: yapi olusturmada bilinmeyen alan
 *   T030: generic tip argumani bound'u karsilamiyor (constraint violation)
 *   T031: ozellik bilinmiyor (bound olarak verilen ad cozulemedi)
 *   G001: *T dereferans guvensiz blok disinda (C5 on-kosul #2)
 *   G002: satiriçi_asm guvensiz blok disinda (C5)
 *   G005: ISARETCI yakalayan closure frame'i asiyor (return / frame-asiri
 *          saklama). D-323 DARALTMA: env ARTIK HEAP (llvm.c V2-F2 @malloc) →
 *          SKALER yakalama cerceve asiminda dangling URETEMEZ, reddedilmez;
 *          isaretci-benzeri yakalama (metin/Dizi/ref/ham-pointer/yapi) kopyalanan
 *          isaretciyle gosterdigi bolgeyi asabilir → REDDEDILIR. Tip cozulemezse
 *          isaretci VARSAYILIR (default-deny).
 *   AS001: asm mimari etiketi hedef mimariyle uyusmuyor (C5 arch-tag;
 *          hedef KEMGU_HEDEF_MIMARI — llvm.h, hedefe-duyarli C8'de)
 *   AS002: asm operandi uygunsuz tip — yalniz kopyalanabilir primitif
 *          (tamN, dtamN, mantiksal, karakter, ham *T); tekkez/yetki
 *          dogrudan gecemez, cikti lineer olamaz (C5 C.1 kara kutu)
 *   BL001: bolge_al beklenen *T baglami yok — degisken v: *T annot'u
 *          sart, sessiz varsayilan YOK (v1 bolge-container)
 *   BL002: bolge_al arguman hatasi (ilk arg yetki<R> degil / arg
 *          sayisi != 2 / eleman sayisi tamsayi degil)
 *
 * Hatalar 'hata_raporla' ile stderr'e yazilir, hata_sayisi artirilir.
 * Ifade tipi belirlenemezse TIP_HATA doner — caller bu tipi gormezden gelmeli.
 */

/* G005: escape analizi (src/escape.h) — tam tanim tip_kontrol.c'de include edilir. */
struct EscapeAnaliz;

/* Yüklenmiş modül izleme (cycle + duplicate detection) */
typedef struct YuklenmisModul {
    char *yol;             /* "stdlib/temel/matematik.kem" gibi */
    int yol_uz;
    struct YuklenmisModul *sonraki;
} YuklenmisModul;

typedef struct TipKontrol {
    Arena *arena;
    Scope *scope;                  /* mevcut scope */
    Scope *global_scope;           /* yapi/islev tanimlari icin (ileri referans) */
    Scope *builtin_scope;          /* A: built-in'ler + dosya-modul kanonik
                                      kayitlari (gizli). global_scope'un parent'i;
                                      dosya-modul scope'lari da buna baglanir —
                                      giris dosyasinin ozel adlari modullere sizmaz. */
    TipBilgisi *aktif_donus_tipi;  /* aktif islev gövdesi içinde 'ver' icin */
    UygulaTablosu uygulamalar;     /* (Tip, Ozellik) -> impl registry */
    YuklenmisModul *yuklenmisler;  /* duplicate-load engelleme */
    int hata_sayisi;
    const char *dosya_adi;
    const char *kaynak;
    /* === Linear Types Spec V1 takibi === */
    int scope_seviyesi;            /* mevcut scope derinligi (lineer omur kontrolu) */
    int lambda_govdesi_icinde;     /* >0 = lambda govdesi visit ediyoruz */
    /* D-315 (Linear V2.1): imha(...) operandini tuketiyoruz. Kismi tasinmis
     * bir yapi YALNIZ imha ile tuketilebilir (tasima delikli deger devrederdi). */
    int imha_baglaminda;
    /* D-320: SONDAJ (probe) modu — ifadeyi YALNIZ tip ogrenmek icin ziyaret
     * ediyoruz, tuketim SAYILMAZ. DUGUM_CAGRI argumanlari iki pas gorur
     * (pas 1: generic unify, pas 2: beklenen-tip cikarsama + kontrol); her
     * ziyaret lineer durumu mutasyona ugratirsa `f(kullan(t))` gibi TEK
     * tuketim IKI sayilir -> sahte L002. Pas 1 bu bayrakla sondaj olarak
     * isaretlenir; lineer defter YALNIZ pas 2'de guncellenir. */
    int lineer_sondaj;
    /* D-304: blok-form lambda dönüş çıkarsaması. cikarsama>0 iken blok içindeki
     * `ver <e>` deyimi e'nin tipini blok_donus'a KAYDEDER (aktif_donus_tipi'ye
     * karşı kontrol yerine) → lambda dönüş tipi gövdeden çıkarsanır. */
    int lambda_blok_cikarsama;
    TipBilgisi *lambda_blok_donus;
    int lambda_lineer_yakalama;    /* >0 = lambda lineer baglama yakaladi
                                      (closure-itself-linear icin) */
    int lambda_yakalama;           /* G005: >0 = lambda HERHANGI bir cevre
                                      lokal/param yakaladi (lineer + lineer-olmayan).
                                      codegen lambda_serbest_tara ile birebir. */
    int lambda_yakalama_isaretci;  /* D-323: >0 = yakalananlardan EN AZ BIRI
                                      isaretci-benzeri (metin/Dizi/ref/ham-pointer/yapi).
                                      G005 YALNIZ bu durumda tetiklenir: env HEAP
                                      oldugu icin skaler yakalama (deger kopyasi)
                                      cerceve asiminda dangling URETEMEZ; isaretci
                                      kopyasi ise gosterdigi bolgeyi asabilir. */
    /* [D-505] R-YAKALAMA-THREAD (Bellek Modeli sat.144/323):
     * gorev_baslat kapanisi ISARETCI-benzeri yakaladiginda SAHIPLIK THREADE
     * TASINIR -> kaynak erisimi kaybeder. Spec ZATEN boyle diyordu ( DRF
     * saglamlik ispatinin parcasi) ama UYGULANMAMISTI: iki gorev ayni
     * Dizi<T>ye yaziyordu ve --check SIFIR tani veriyordu (D-504: 100000
     * yerine 62868 gozlendi).
     * ⚠ SKALER YAKALAMA ETKILENMEZ (D-323 daraltmasinin AYNISI): env HEAP
     *   kopyasidir, skaler kopya YARISAMAZ.
     * gorev_kapanis_derinlik: gorev_baslat argumani kontrol edilirken >0.
     * tasinan_*: lambda TARANIRKEN toplanir, lambda BITINCE isaretlenir
     *   (tarama sirasinda isaretlemek ayni lambda icindeki 2. kullanimi
     *    sahte L002 yapardi). */
    int gorev_kapanis_derinlik;
    const char *tasinan_ad[64];
    int tasinan_uz[64];
    int tasinan_sayi;
    Scope *lambda_baslangic_scope; /* lambda govdesi disinda kalan scope sınırı */
    /* G005: aktif islev govdesinin escape analizi (forward DFA, src/escape.c).
     * DUGUM_ISLEV govde kontrolu sirasinda kurulur; lambda case ESC_CAGIRAN
     * sorgular. NULL = islev disi baglam (escape bilgisi yok). */
    const struct EscapeAnaliz *aktif_escape;
    /* === C5: unsafe-context bayragi === */
    int guvensiz_baglam;           /* >0 = guvensiz blok icindeyiz (derinlik).
                                      *T dereferans (G001) ve satiriçi_asm (G002)
                                      yalniz guvensiz baglamda gecerli. */
    int ciplak_baglam;             /* D-257: >0 = çıplak işlev gövdesindeyiz.
                                      Çıplak (ρ-suz C-ABI) yalnız çıplak/extern
                                      çağırır; normal (ρ-alan) fn çağrısı → E013
                                      (verilecek ρ yok → ABI uyumsuz segfault). */
} TipKontrol;

void tip_kontrol_baslat(TipKontrol *tk, Arena *a, Scope *global,
                        const char *dosya_adi, const char *kaynak);

/* Ifadenin tipini belirle. Hata varsa raporla, TIP_HATA donebilir. */
TipBilgisi *tip_belirle(TipKontrol *tk, const Dugum *ifade);

/* Bidirectional tip cikarsamasi: 'beklenen' tip parametre olarak verilirse
 * literal'lar (TAM, KESIRLI, bos dizi) o tipe gore cikarsanir.
 *   degisken x: tam8 = 1;       // 1 -> tam8 (default tam32 yerine)
 *   degisken xs: Dizi<T> = [];  // bos dizi -> Dizi<T>
 *   f(arg)  // arg, parametre tipi context'inde cikarsanir
 * beklenen NULL ise tip_belirle ile ayni davranir. */
TipBilgisi *tip_belirle_beklenen(TipKontrol *tk, const Dugum *ifade,
                                  const TipBilgisi *beklenen);

/* AST tip dugumunu (DUGUM_TIP_*) TipBilgisi'ye cevir.
 * Built-in tipler (tam32 vs) ve kullanici tipleri (sembol arama) icin. */
TipBilgisi *ast_tip_to_bilgi(TipKontrol *tk, const Dugum *tip_d);

/* Tum programi tip kontrol et (iki gecis: pre-populate + govde kontrol). */
void tip_kontrol_program(TipKontrol *tk, const Dugum *program);

/* Hata raporlama */
void tip_hata(TipKontrol *tk, const Dugum *d,
              const char *kod, const char *mesaj);

#endif /* KEMGU_TIP_KONTROL_H */
