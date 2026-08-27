#ifndef KEMGU_ESCAPE_H
#define KEMGU_ESCAPE_H

#include "arena.h"
#include "ast.h"

/*
 * KEMGU Escape Analizi (DFA Fixed-Point)
 * =======================================
 *
 * KEMGU_Bellek_Modeli.md, Katman 3:
 *
 *   ESC-0 (Kacmaz):     scope'tan cikmaz -> rho_yerel veya rho_iterasyon
 *   ESC-1 (Cagirana):   ver ile doner -> rho_cagiran
 *   ESC-2 (Belirsiz):   gomulme, yakalama, kosullu dal -> baglam-duyarli
 *
 * Algoritma: forward data-flow analizi, fixed-point iterasyonu.
 *
 *   Pass 1: Tum tahsis bolgelerini topla (metin, dizi_olustur, yapi_olustur,
 *           lambda, cagri sonucu). Default: ESC_YEREL.
 *
 *   Pass 2: Degisken bag haritalari ile birlikte AST'yi tara. Her 'ver e' icin
 *           e'nin altinda yatan tahsis bolgelerini bul, ESC_CAGIRAN olarak isaretle.
 *           Atamalar: x = e ise, x'in eski baglamasinin escape kategorisini
 *           e'ye yay (transitif).
 *
 *   Pass 3: Hicbir kategori degismeyene kadar Pass 2'yi tekrarla.
 *
 * Bu kademe, bolge_atama.c'nin yapamayacagi su senaryolari yakalar:
 *
 *   degisken x = "merhaba"; ver x;       // "merhaba" -> CAGIRAN (transitif)
 *   degisken y = [1,2,3]; ver y[0];      // y -> YEREL (ic eleman basit kopya)
 *   eger k { ver x; } degilse { ... }    // x -> CAGIRAN (kosullu yol)
 *
 * === ESC_ITERASYON SAGLAMLIK GERI-CEKILMESI (loop-carried; D-101) ===
 *
 * ESC_ITERASYON omru EN KISA bolgedir (rho_iterasyon <= rho_yerel). Gelecekte
 * (F4.3) iterasyon-basina serbest birakilacagi icin, iterasyonu ASAN bir tahsisi
 * ITERASYON saymak = canliyken serbest = UAF. Iterasyon-yerelligi SAGLAM tespit
 * etmek, kaccis rotalarini kapatan kapilara KOSULLUYDU:
 *   - D-007 (diziler skaler-eleman) -> dis agregaya REFERANS saklanamaz,
 *   - R-GOMME (gomme yok) -> kaccan agregada gomulu heap-ref yok.
 * ANCAK bu kapilar su an ENFORCE EDILMIYOR: `Dizi<Dizi<T>>`, `Dizi<metin>` ve Dizi
 * alanli yapi tip-kontrolden gecer ve by-ref `KdlDizi*` olarak lower edilir. Boylece
 * `dis[i] = tahsis` (DUGUM_INDEKS lvalue) ve `nesne.alan = tahsis` (DUGUM_ERISIM
 * lvalue) bir dongu-tahsisini iterasyondan kaccirir; sentaktik tespit bunu kaccirinca
 * under-approximation = gizli UAF olurdu.
 *
 * KARAR (guvenli geri-cekilme): escape analizi HICBIR tahsisi ESC_ITERASYON
 * URETMEZ; dongu icindeki tahsisler dahil HEPSI ESC_YEREL (daha uzun omurlu =
 * guvenli). ESC_ITERASYON enum'u API/gelecek icin KORUNUR ama bu analiz tarafindan
 * atanmaz. Per-iterasyon optimizasyonu, gercek bolge-serbest semantigi (F4.3)
 * geldiginde -- kapilar enforce edilince ya da TUM kaccis rotalari (ver / daha-sig
 * degisken / agrega-lvalue store / agrega-gomme / cagri / closure) kapsaninca --
 * saglamca eklenecek. Bu sayede UAF su an IMKANSIZ ve soundness arguman'i TRIVIAL.
 *
 * Mevcut sinirlamalar (v1):
 *   - ESC_ITERASYON uretilmez (yukaridaki geri-cekilme; F4.3'te saglamca eklenir)
 *   - Inter-procedural yok (cagri sonuclari konservatif: yerel)
 *   - Closure yakalama henuz islenmiyor (G005 kaccan closure'i ayrica reddeder)
 *   - Yapi/dizi elemanlarinin alt-escape'i takip edilmiyor (alanin escape'i
 *     parent allocation'a yansir, ama tersine yansima yok)
 */

typedef enum {
    ESC_YEREL,        /* lokal (escape etmez) — default; dongu tahsisleri DAHIL (D-101) */
    ESC_ITERASYON,    /* iterasyon-yerel; rho_iterasyon (EN KISA omur). API/gelecek icin
                       * KORUNUR; D-101 geri-cekilmesiyle bu analiz tarafindan URETILMEZ. */
    ESC_CAGIRAN,      /* ver ile cagirana escape eder */
} EscapeKategorisi;

typedef struct EscapeKayit {
    const Dugum *dugum;        /* AST dugumu (tahsis bolgesi) */
    EscapeKategorisi kategori;
    int dongu_derinligi;       /* tahsis edildigi dongu derinligi (0 = islev seviyesi) */
    int kesin_yerel;           /* F4.2b: POZİTİF kanıt — bağlı değişken "confined"
                                * (tüm kullanımları yerinde okuma/yazma + retain-etmeyen
                                * dizi-builtin); ρ_yerel free-routing YALNIZ bunu kullanır. */
} EscapeKayit;

typedef struct EscapeBag {
    const char *ad;            /* degisken adi (kopyalanmaz — AST ile aynı omur) */
    int ad_uz;
    const Dugum *deger;        /* baglandigi ifade (allocation site veya baska ifade) */
    int scope_seviye;          /* hangi scope'ta tanimlandi (pop icin) */
} EscapeBag;

typedef struct EscapeAnaliz {
    Arena *arena;

    /* Tahsis bolgesi -> escape kategorisi haritasi (lineer arama) */
    EscapeKayit *kayitlar;
    int kayit_sayi;
    int kayit_kapasite;

    /* Aktif degisken baglamalari (scope stack — sondan basa arama) */
    EscapeBag *baglamalar;
    int bag_sayi;
    int bag_kapasite;

    /* Pass durumu */
    int scope_seviye;
    int dongu_derinligi;
    int ver_baglaminda;
    int degisti;               /* bu pass'ta kategori degisti mi? */

    /* [D-496] ⚠⚠ BU BAYRAK KURULUYOR AMA HIC OKUNMUYOR — ÖLÇÜLDÜ.
     * Yorumu bir "sound backstop" VAAT EDIYOR (aşağıda, F4.2b'den) ama
     * `llvm.c` ve `bolge_atama.c` onu HIC SORGULAMIYOR. Yani tarif edilen
     * koruma PRATIKTE YOK.
     *
     * ⚠ ANCAK BU BIR SOUNDNESS DELIGI DEGIL — iddia edilmedi, ÖLÇÜLDÜ.
     * Yerini IKI BASKA (ve daha KESKIN) mekanizma tutuyor:
     *   1. Kaçan kapanış işaretçi yakalarsa **G005** DERLEME ZAMANINDA
     *      reddeder (ölçüldü: `ver || dizi_al(xs,0)` -> G005). Gürültülü.
     *   2. Kaçmayan kapanışta `ky_confined`in DUGUM_LAMBDA dalı yakalanan
     *      HER değişkenin hapsedilmesini DÜŞÜRÜR -> dizi ρ_caller'a gider
     *      (ölçüldü: `kdl_dizi_olustur(ptr %rho, ...)`).
     * (2) bu bayraktan DAHA İYİDİR: değişken-başına, işlev-geneli değil.
     *
     * ⚠ ÖLÜ KOD OLARAK BIRAKMAK RISKLIDIR (D-459'un dersi: ölü kodu bırakmak
     * sonraki okuyucuyu ona GÜVENMEYE davet eder). Silinmedi çünkü F4.4'ün
     * kapanış-env ekseni buraya bağlanabilir; ama o gün gelene kadar bu not
     * "bu bayrak seni KORUMUYOR" diyor. */
    /* F4.2b: analiz edilen islev govdesinde EN AZ BIR lambda var mi? Varsa
     * free-routing GUARD'i o islevde ρ_yerel yonlendirmeyi KAPATIR (closure
     * capture sound backstop — lexical scope: yalniz bu fn'in lambda'si bu fn'in
     * lokalini yakalar; block-form capture takibi v1'de yok). */
    int islev_lambda_icerir;

} EscapeAnaliz;

void escape_baslat(EscapeAnaliz *ea, Arena *a);

/* [D-488] INTERPROCEDURAL PARAMETRE-TUTMA OZETI — program basinda BIR KEZ kur.
 * `f(xs)` cagrisinda xs'in hapsedilme kaniti KOSULSUZ dusuyordu (yalniz
 * beyaz-listeli dizi-yerlesiginin 0. konumu muafti) -> gercek programlarda
 * rho_yerel yonlendirmesi ~%1'e iniyordu (OLCULDU: parser.kem 0/35,
 * checker.kem 1/181, codegen.kem 3/267).
 * YENI ANALIZ ICAT EDILMEDI: "f, p'yi SAKLIYOR MU?" sorusu `ky_confined` ile
 * BIREBIR AYNIDIR (ver p -> DENY, lambda yakalama -> DENY, kuresele/agregaya
 * atama -> DENY, bilinmeyen dugum -> DENY). Kanit makinesi cagri yerine tasindi.
 * Cagrilmazsa tablo bos kalir -> tum sorgular DENY -> ESKI davranis (guvenli).
 * Tablo ARENA-desteklidir; ayrica serbest birakma GEREKMEZ. */
void escape_fn_tablo_kur(Arena *a, const Dugum *program);

/* Dahili malloc/realloc edilmis tablolari serbest birak. */
void escape_serbest(EscapeAnaliz *ea);

/* Tum islev govdesini fixed-point'e kadar analiz et. */
void escape_analiz_islev(EscapeAnaliz *ea, const Dugum *islev);

/* Genel AST'yi (program/modul/islev listesi) gez ve her isleve analiz uygula. */
void escape_analiz_program(EscapeAnaliz *ea, const Dugum *program);

/* Bir AST dugumunun escape kategorisini sorgula.
 * Kayitsiz dugumler icin ESC_YEREL doner. */
EscapeKategorisi escape_kategori(const EscapeAnaliz *ea, const Dugum *d);

/* F4.2b: dugum escape kaydinda ACIKCA var mi? (free-routing principle 1:
 * kayitsiz -> default-YEREL'e GUVENME -> ρ_caller). */
int escape_kayitli_mi(const EscapeAnaliz *ea, const Dugum *d);

/* F4.2b: dizi-tahsis dugumu KESİN-YEREL (confined) kanıtlandı mı? POZİTİF default-deny
 * local-proof — bağlı değişkenin TÜM kullanımları yerinde okuma/yazma + retain-etmeyen
 * dizi-builtin ise 1. ρ_yerel free-routing'in TEK sound koşulu (escape DFA'ya GÜVENME). */
int escape_kesin_yerel(const EscapeAnaliz *ea, const Dugum *d);

/* Tahsis bolgesinin dongu derinligi (0 = islev seviyesi). */
int escape_dongu_derinligi(const EscapeAnaliz *ea, const Dugum *d);

/* Yazdirma — debug */
const char *escape_kategori_adi(EscapeKategorisi k);

#endif /* KEMGU_ESCAPE_H */
