/*
 * KEMGU Katman 2 (Concurrency / DRF V1) — görev runtime birim testi
 *
 * runtime/kdl_runtime.c'deki kdl_gorev_basla_kapanis / kdl_gorev_birlestir
 * ciftini DOGRUDAN (codegen'den bagimsiz) sinar. Codegen tarafinin uctan uca
 * kaniti ayrica test_llvm.c'dedir; burada runtime SEMANTIGI kanitlanir.
 *
 * NEDEN AYRI BIR TEST: bir gorev testinin "42 dondu" demesi, thread'in
 * gercekten spawn edildigini KANITLAMAZ. Bu dosya o ayrimi (ve S1/S2 bolge
 * sahipligini) sayaclarla acikca olcer.
 *
 * D-296 GUNCELLEMESI: eskiden thread yaratilamazsa gorev SIRALI calistiriliyordu
 * (fallback) ve iki yol AYNI sonucu uretiyordu — ayrimi gormek icin sayaclar
 * sarttir. Fallback artik KALDIRILDI (spawn basarisizligi -> kdl_panik), cunku
 * sirali calistirma bloklayan kanal islemi yapan bir govdede KALICI KILITLENME
 * uretiyordu (olculdu: exit 124). Sayaclar korundu: `kdl_gorev_sirali_sayisi`
 * artik bir INVARYANT TANIGIDIR (daima 0 — test [9]).
 */

/* [D-480] POSIX ozellik-test makrosu — HER #include'DAN ONCE (D-474 ile ayni
 * kural). `-std=c11` kati ISO'dur; onsuz `nanosleep`/`struct timespec` glibc'de
 * GIZLIDIR. Windows'ta tanimlanmaz -> o dal etkilenmez. */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#  define _POSIX_C_SOURCE 200112L
#endif

#include <stdio.h>
#include <stdint.h>

#if defined(_WIN32)
  #include <windows.h>
  static void kisa_bekle(int ms) { Sleep((DWORD)ms); }
#else
  /* [D-480] `usleep` DEGIL `nanosleep`.
   *
   * `-std=c11` KATI ISO'dur ve glibc POSIX bildirimlerini gizler; ustelik
   * `usleep`/`useconds_t` POSIX.1-2008'de KALDIRILDI (obsolete) ve glibc'de
   * ancak `_DEFAULT_SOURCE` ile gorunur. Linux'ta olculdu:
   *   error: 'useconds_t' undeclared
   * `nanosleep` POSIX.1-2001'dir ve `_POSIX_C_SOURCE 200112L` ile gorunur --
   * yani kaldirilmis bir API'yi geri acmak yerine GECERLI olani kullanmak
   * hem dogru hem daha az makro gerektiriyor. Windows dali DEGISMEDI.
   * ⚠ `_POSIX_C_SOURCE` DOSYANIN BASINDA tanimli (her #include'dan ONCE);
   * burada tanimlamak GEC KALIRDI -- <stdio.h> zaten islenmis olurdu. */
  #include <time.h>
  static void kisa_bekle(int ms) {
      struct timespec ts;
      ts.tv_sec  = ms / 1000;
      ts.tv_nsec = (long)(ms % 1000) * 1000000L;
      nanosleep(&ts, NULL);
  }
#endif

static int toplam_test = 0;
static int basarili = 0;
static int basarisiz = 0;

static void test_sonuc(const char *ad, int durum) {
    toplam_test++;
    if (durum) {
        basarili++;
        printf("  [%d] %s ... \xe2\x9c\x93\n", toplam_test, ad);
    } else {
        basarisiz++;
        printf("  [%d] %s ... \xe2\x9c\x97\n", toplam_test, ad);
    }
}

/* Runtime API (kdl_runtime.c) — codegen'in urettigi `declare`lerle ayni ABI.
 * D-294: donusler int64_t (T genisletme; `görev<metin>` gibi isaretci T'ler
 * icin i32 tasima isaretciyi kirpardi). Bu bildirimler tanimla BIREBIR
 * eslesmeli: ayri derleme birimleri oldugu icin uyusmazligi linker YAKALAMAZ
 * ve test dusuk 32 biti okuyup YANLIS SEBEPLE gecerdi. */
typedef int64_t (*KdlGorevBare)(void *rho);
typedef int64_t (*KdlGorevKapanis)(void *rho, void *env);
typedef struct KdlGorevOpak KdlGorevOpak;

/* D-309: 4. param rho_serbest — codegen'in POZITIF confinement kanitinin
 * sonucu. 0 = kanit yok -> ρ_sahip sizdirilir (eski davranis), 1 = kanitli
 * -> join'de serbest. Bildirim tanimla BIREBIR eslesmeli (yukaridaki not). */
extern KdlGorevOpak *kdl_gorev_basla_kapanis(KdlGorevBare fn_bare,
                                             KdlGorevKapanis fn_kapanis,
                                             void *env, int32_t rho_serbest);
extern int64_t kdl_gorev_birlestir(KdlGorevOpak *g);
extern uint64_t kdl_gorev_thread_sayisi;
extern uint64_t kdl_gorev_sirali_sayisi;
extern int kdl_bolge_bakiye(void);   /* D-309 olcum kapisi: acik bolge sayisi */

/* Katman 2 / R-KANAL runtime */
typedef struct KdlKanalOpak KdlKanalOpak;
extern KdlKanalOpak *kdl_kanal_olustur(int32_t kapasite);
extern void kdl_kanal_gonder(KdlKanalOpak *k, int64_t deger);   /* D-295 */
extern int64_t kdl_kanal_al(KdlKanalOpak *k);                    /* D-295 */
extern void kdl_kanal_serbest(KdlKanalOpak *k);

/* === 1: bare yol (env == NULL) — codegen'in yakalamasiz lambda'si ===
 * Lifted lambda imzasi: i32 @lambda_N(ptr %rho) */
static void *bare_gorulen_rho;
static int64_t isci_bare(void *rho) {
    bare_gorulen_rho = rho;
    return 42;
}

static void test_bare_yol(void) {
    bare_gorulen_rho = NULL;
    KdlGorevOpak *g = kdl_gorev_basla_kapanis(isci_bare, NULL, NULL, 0);
    int64_t r = kdl_gorev_birlestir(g);
    test_sonuc("bare (env=NULL): sonuc 42", r == 42);
    /* R-GÖREV: gorev KENDI ρ_sahip'ini alir -> govdeye NULL olmayan ρ gecer. */
    test_sonuc("bare: gorev govdesine ρ_sahip gecti (NULL degil)",
               bare_gorulen_rho != NULL);
}

/* === 2: kapanis yolu (env != NULL) — yakalamali lambda ===
 * Lifted lambda imzasi: i32 @lambda_N(ptr %rho, ptr %env) */
static int64_t isci_kapanis(void *rho, void *env) {
    (void)rho;
    return *(int32_t *)env + 2;
}

static void test_kapanis_yolu(void) {
    int32_t yakalanan = 40;
    KdlGorevOpak *g = kdl_gorev_basla_kapanis(NULL, isci_kapanis, &yakalanan, 0);
    int64_t r = kdl_gorev_birlestir(g);
    test_sonuc("kapanis (env!=NULL): env okundu, sonuc 42", r == 42);
}

/* === 3: GERCEK thread mi, sirali fallback mi? ===
 * Sonuc dogrulugu bu ayrimi gostermez — sayac gosterir. */
static void test_gercek_thread(void) {
    uint64_t once_thread = kdl_gorev_thread_sayisi;
    uint64_t once_sirali = kdl_gorev_sirali_sayisi;
    KdlGorevOpak *g = kdl_gorev_basla_kapanis(isci_bare, NULL, NULL, 0);
    (void)kdl_gorev_birlestir(g);
    test_sonuc("GERCEK thread spawn edildi (sirali fallback DEGIL)",
               kdl_gorev_thread_sayisi == once_thread + 1 &&
               kdl_gorev_sirali_sayisi == once_sirali);
}

/* === 3b: D-296 INVARYANTI — sirali fallback ARTIK YOK ===
 * Fallback kaldirildi (spawn basarisizligi -> kdl_panik). Bu sayac bu yuzden
 * PROGRAM BOYUNCA 0 kalmali. Sifirdan farkli okunmasi, fallback'in bir sekilde
 * geri geldigini gosterir — ki o durumda bloklayan kanal islemi yapan bir gorev
 * KILITLENIR (olculdu: exit 124). Bu, ucuz ama gercek bir regresyon bekcisidir. */
static void test_sirali_fallback_yok(void) {
    test_sonuc("D-296: sirali fallback HIC devreye girmedi (sayac 0)",
               kdl_gorev_sirali_sayisi == 0);
}

/* === 4: S1/S2 — her gorev KENDI bolgesini sahiplenir ===
 * S2 (baslangic sahipligi) + S1 (tekil sahiplik): iki ayri gorev ayni ρ'yu
 * PAYLASMAMALI. Paylassalardi iki thread ayni bump-allokatore es zamanli
 * yazardi -> veri yarisi. Bu test o paylasimin OLMADIGINI dogrudan olcer. */
static void *rho_a;
static void *rho_b;
static int64_t isci_a(void *rho) { rho_a = rho; return 1; }
static int64_t isci_b(void *rho) { rho_b = rho; return 2; }

static void test_bolge_ayrikligi(void) {
    rho_a = NULL; rho_b = NULL;
    KdlGorevOpak *ga = kdl_gorev_basla_kapanis(isci_a, NULL, NULL, 0);
    KdlGorevOpak *gb = kdl_gorev_basla_kapanis(isci_b, NULL, NULL, 0);
    int64_t ra = kdl_gorev_birlestir(ga);
    int64_t rb = kdl_gorev_birlestir(gb);
    test_sonuc("iki gorev: sonuclar karismadi (1 ve 2)", ra == 1 && rb == 2);
    test_sonuc("S2: her gorev ρ_sahip aldi (ikisi de NULL degil)",
               rho_a != NULL && rho_b != NULL);
    test_sonuc("S1: iki gorevin ρ_sahip'i AYRI (bolge paylasilmiyor)",
               rho_a != rho_b);
}

/* === 5: cok gorev — sonuc/handle karismasi yok === */
static int64_t isci_sabit_7(void *rho) { (void)rho; return 7; }

static void test_cok_gorev(void) {
    KdlGorevOpak *gs[8];
    int ok = 1;
    for (int i = 0; i < 8; i++)
        gs[i] = kdl_gorev_basla_kapanis(isci_sabit_7, NULL, NULL, 0);
    for (int i = 0; i < 8; i++)
        if (kdl_gorev_birlestir(gs[i]) != 7) ok = 0;
    test_sonuc("8 es zamanli gorev: hepsi 7 dondu", ok);
}

/* === 6: NULL handle — D-296'da SESSIZ-0 KAPATILDI ===
 * Eskiden burada `kdl_gorev_birlestir(NULL) == 0` iddia ediliyordu ve bu
 * "savunmaci" gorunuyordu; aslinda SESSIZ YANLIS CEVAPTI: donen 0, gercekten
 * 0 donmus bir gorevden ayirt edilemez (D-292'nin bos-kanal hatasiyla ayni
 * sinif). Artik kdl_panik ile GURULTULU olduğu icin bu yol IN-PROCESS test
 * EDILEMEZ (panik abort eder, test kosucusunu de oldururdu).
 * Kapsama: davranis kdl_runtime.c'de belgeli; tetikleyicisi yalnizca OOM
 * (kdl_gorev_basla_kapanis'in malloc/bolge basarisizligi). Ayri-surec testi
 * gerekseydi calistir_* hedefi yazilirdi — maliyeti faydasini asiyor. */

/* ========================================================================
 * KANAL (R-KANAL) — bloklama semantigi
 * ======================================================================== */

/* === 7: BOS kanalda kdl_kanal_al BLOKLAR ===
 * Bu testin ayirt ediciligi kritik: gonderici BILEREK gecikir, boylece alici
 * kesinlikle once kanal_al'a girer. ESKI (bloklamayan) surumde bu DETERMINISTIK
 * olarak 0 donerdi ve o 0, gercekten gonderilmis bir 0'dan ayirt edilemezdi.
 * Bloklayan surumde 42 doner. */
static KdlKanalOpak *kanal_gecikmeli;
static int64_t isci_gecikmeli_gonder(void *rho) {
    (void)rho;
    kisa_bekle(60);                       /* alici once kanal_al'a girsin */
    kdl_kanal_gonder(kanal_gecikmeli, 42);
    return 0;
}

static void test_kanal_al_bos_bloklar(void) {
    kanal_gecikmeli = kdl_kanal_olustur(4);
    KdlGorevOpak *g = kdl_gorev_basla_kapanis(isci_gecikmeli_gonder, NULL, NULL, 0);
    int64_t v = kdl_kanal_al(kanal_gecikmeli);   /* BLOKLAMALI */
    (void)kdl_gorev_birlestir(g);
    test_sonuc("bos kanalda kanal_al BLOKLAR (0 degil, 42 alir)", v == 42);
    kdl_kanal_serbest(kanal_gecikmeli);
}

/* === 8: DOLU kanalda kdl_kanal_gonder BLOKLAR (akis denetimi) ===
 * Kapasite 2, 5 mesaj -> gonderici en az 3 kez dolu-bloklar. Eski surumde
 * tasan mesajlar SESSIZCE DUSERDI -> toplam 15 degil, 3 (1+2) cikardi. */
static KdlKanalOpak *kanal_akis;
static int64_t isci_bes_gonder(void *rho) {
    (void)rho;
    for (int32_t i = 1; i <= 5; i++) kdl_kanal_gonder(kanal_akis, i);
    return 0;
}

static void test_kanal_gonder_dolu_bloklar(void) {
    kanal_akis = kdl_kanal_olustur(2);            /* bilincli kucuk kapasite */
    KdlGorevOpak *g = kdl_gorev_basla_kapanis(isci_bes_gonder, NULL, NULL, 0);
    kisa_bekle(60);                               /* gonderici dolu-bloklasin */
    int64_t toplam = 0;
    int sira_ok = 1;
    for (int32_t i = 1; i <= 5; i++) {
        int64_t v = kdl_kanal_al(kanal_akis);
        if (v != i) sira_ok = 0;                  /* FIFO sirasi korunmali */
        toplam += v;
    }
    (void)kdl_gorev_birlestir(g);
    test_sonuc("dolu kanalda kanal_gonder BLOKLAR (5 mesaj, kap=2, toplam 15)",
               toplam == 15);
    test_sonuc("kanal FIFO sirasi korunur (1,2,3,4,5)", sira_ok);
    kdl_kanal_serbest(kanal_akis);
}

/* === 9: NULL kanal savunmasi === */
static void test_kanal_null(void) {
    kdl_kanal_gonder(NULL, 1);                    /* cokmemeli */
    test_sonuc("kdl_kanal_al(NULL) cokmeden 0 doner", kdl_kanal_al(NULL) == 0);
}

/* === 10: D-309 — ρ_sahip KOSULLU serbest birakma (OLCUM KAPISI) ===
 * "Serbest biraktik" demek yetmez; ACIK BOLGE SAYISI olculur. kdl_bolge_bakiye()
 * = olusturulan - serbest. Iki gorev ayni govdeyle kosar; tek fark bayrak:
 *   rho_serbest=0 -> bakiye +1 (eski davranis: sizdir)
 *   rho_serbest=1 -> bakiye +0 (ρ_sahip join'de geri verildi)
 * Bayragin ETKISIZ olmasi (ikisi de +1) veya kanitsiz serbest (ikisi de +0)
 * bu testte GURULTULU duser. */
static void test_rho_sahip_serbest(void) {
    int taban = kdl_bolge_bakiye();
    KdlGorevOpak *g0 = kdl_gorev_basla_kapanis(isci_bare, NULL, NULL, 0);
    (void)kdl_gorev_birlestir(g0);
    int sizdiran = kdl_bolge_bakiye() - taban;
    test_sonuc("D-309: kanitsiz gorev (rho_serbest=0) ρ_sahip'i SIZDIRIR (+1)",
               sizdiran == 1);

    int taban2 = kdl_bolge_bakiye();
    KdlGorevOpak *g1 = kdl_gorev_basla_kapanis(isci_bare, NULL, NULL, 1);
    int64_t r = kdl_gorev_birlestir(g1);
    int hapsedilen = kdl_bolge_bakiye() - taban2;
    test_sonuc("D-309: kanitli gorev (rho_serbest=1) ρ_sahip'i GERI VERIR (+0)",
               hapsedilen == 0);
    test_sonuc("D-309: serbest birakma sonucu BOZMADI (42)", r == 42);
}

int main(void) {
    printf("=== KEMGU gorev runtime (Katman 2 / DRF V1) testleri ===\n");
    test_bare_yol();
    test_kapanis_yolu();
    test_gercek_thread();
    test_bolge_ayrikligi();
    test_cok_gorev();
    test_sirali_fallback_yok();
    test_kanal_al_bos_bloklar();
    test_kanal_gonder_dolu_bloklar();
    test_kanal_null();
    test_rho_sahip_serbest();
    printf("=== %d/%d test gecti ===\n", basarili, toplam_test);
    return basarisiz == 0 ? 0 : 1;
}
