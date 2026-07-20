/*
 * KEMGU Katman 2 (Concurrency / DRF V1) — görev runtime birim testi
 *
 * runtime/kdl_runtime.c'deki kdl_gorev_basla_kapanis / kdl_gorev_birlestir
 * ciftini DOGRUDAN (codegen'den bagimsiz) sinar. Codegen tarafinin uctan uca
 * kaniti ayrica test_llvm.c'dedir; burada runtime SEMANTIGI kanitlanir.
 *
 * NEDEN AYRI BIR TEST: runtime, thread yaratilamazsa gorevi SIRALI calistirir
 * (fallback). Sirali fallback ile gercek thread AYNI sonucu uretir — yani bir
 * gorev testinin "42 dondu" demesi thread'in gercekten spawn edildigini
 * KANITLAMAZ. Bu dosya o ayrimi (ve S1/S2 bolge sahipligini) acikca olcer.
 */

#include <stdio.h>
#include <stdint.h>

#if defined(_WIN32)
  #include <windows.h>
  static void kisa_bekle(int ms) { Sleep((DWORD)ms); }
#else
  #include <unistd.h>
  static void kisa_bekle(int ms) { usleep((useconds_t)ms * 1000); }
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

extern KdlGorevOpak *kdl_gorev_basla_kapanis(KdlGorevBare fn_bare,
                                             KdlGorevKapanis fn_kapanis,
                                             void *env);
extern int64_t kdl_gorev_birlestir(KdlGorevOpak *g);
extern uint64_t kdl_gorev_thread_sayisi;
extern uint64_t kdl_gorev_sirali_sayisi;

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
    KdlGorevOpak *g = kdl_gorev_basla_kapanis(isci_bare, NULL, NULL);
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
    KdlGorevOpak *g = kdl_gorev_basla_kapanis(NULL, isci_kapanis, &yakalanan);
    int64_t r = kdl_gorev_birlestir(g);
    test_sonuc("kapanis (env!=NULL): env okundu, sonuc 42", r == 42);
}

/* === 3: GERCEK thread mi, sirali fallback mi? ===
 * Sonuc dogrulugu bu ayrimi gostermez — sayac gosterir. */
static void test_gercek_thread(void) {
    uint64_t once_thread = kdl_gorev_thread_sayisi;
    uint64_t once_sirali = kdl_gorev_sirali_sayisi;
    KdlGorevOpak *g = kdl_gorev_basla_kapanis(isci_bare, NULL, NULL);
    (void)kdl_gorev_birlestir(g);
    test_sonuc("GERCEK thread spawn edildi (sirali fallback DEGIL)",
               kdl_gorev_thread_sayisi == once_thread + 1 &&
               kdl_gorev_sirali_sayisi == once_sirali);
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
    KdlGorevOpak *ga = kdl_gorev_basla_kapanis(isci_a, NULL, NULL);
    KdlGorevOpak *gb = kdl_gorev_basla_kapanis(isci_b, NULL, NULL);
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
        gs[i] = kdl_gorev_basla_kapanis(isci_sabit_7, NULL, NULL);
    for (int i = 0; i < 8; i++)
        if (kdl_gorev_birlestir(gs[i]) != 7) ok = 0;
    test_sonuc("8 es zamanli gorev: hepsi 7 dondu", ok);
}

/* === 6: NULL handle savunmasi === */
static void test_null_handle(void) {
    test_sonuc("kdl_gorev_birlestir(NULL) cokmeden 0 doner",
               kdl_gorev_birlestir(NULL) == 0);
}

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
    KdlGorevOpak *g = kdl_gorev_basla_kapanis(isci_gecikmeli_gonder, NULL, NULL);
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
    KdlGorevOpak *g = kdl_gorev_basla_kapanis(isci_bes_gonder, NULL, NULL);
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

int main(void) {
    printf("=== KEMGU gorev runtime (Katman 2 / DRF V1) testleri ===\n");
    test_bare_yol();
    test_kapanis_yolu();
    test_gercek_thread();
    test_bolge_ayrikligi();
    test_cok_gorev();
    test_null_handle();
    test_kanal_al_bos_bloklar();
    test_kanal_gonder_dolu_bloklar();
    test_kanal_null();
    printf("=== %d/%d test gecti ===\n", basarili, toplam_test);
    return basarisiz == 0 ? 0 : 1;
}
