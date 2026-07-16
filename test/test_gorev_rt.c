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

/* Runtime API (kdl_runtime.c) — codegen'in urettigi `declare`lerle ayni ABI. */
typedef int32_t (*KdlGorevBare)(void *rho);
typedef int32_t (*KdlGorevKapanis)(void *rho, void *env);
typedef struct KdlGorevOpak KdlGorevOpak;

extern KdlGorevOpak *kdl_gorev_basla_kapanis(KdlGorevBare fn_bare,
                                             KdlGorevKapanis fn_kapanis,
                                             void *env);
extern int32_t kdl_gorev_birlestir(KdlGorevOpak *g);
extern uint64_t kdl_gorev_thread_sayisi;
extern uint64_t kdl_gorev_sirali_sayisi;

/* === 1: bare yol (env == NULL) — codegen'in yakalamasiz lambda'si ===
 * Lifted lambda imzasi: i32 @lambda_N(ptr %rho) */
static void *bare_gorulen_rho;
static int32_t isci_bare(void *rho) {
    bare_gorulen_rho = rho;
    return 42;
}

static void test_bare_yol(void) {
    bare_gorulen_rho = NULL;
    KdlGorevOpak *g = kdl_gorev_basla_kapanis(isci_bare, NULL, NULL);
    int32_t r = kdl_gorev_birlestir(g);
    test_sonuc("bare (env=NULL): sonuc 42", r == 42);
    /* R-GÖREV: gorev KENDI ρ_sahip'ini alir -> govdeye NULL olmayan ρ gecer. */
    test_sonuc("bare: gorev govdesine ρ_sahip gecti (NULL degil)",
               bare_gorulen_rho != NULL);
}

/* === 2: kapanis yolu (env != NULL) — yakalamali lambda ===
 * Lifted lambda imzasi: i32 @lambda_N(ptr %rho, ptr %env) */
static int32_t isci_kapanis(void *rho, void *env) {
    (void)rho;
    return *(int32_t *)env + 2;
}

static void test_kapanis_yolu(void) {
    int32_t yakalanan = 40;
    KdlGorevOpak *g = kdl_gorev_basla_kapanis(NULL, isci_kapanis, &yakalanan);
    int32_t r = kdl_gorev_birlestir(g);
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
static int32_t isci_a(void *rho) { rho_a = rho; return 1; }
static int32_t isci_b(void *rho) { rho_b = rho; return 2; }

static void test_bolge_ayrikligi(void) {
    rho_a = NULL; rho_b = NULL;
    KdlGorevOpak *ga = kdl_gorev_basla_kapanis(isci_a, NULL, NULL);
    KdlGorevOpak *gb = kdl_gorev_basla_kapanis(isci_b, NULL, NULL);
    int32_t ra = kdl_gorev_birlestir(ga);
    int32_t rb = kdl_gorev_birlestir(gb);
    test_sonuc("iki gorev: sonuclar karismadi (1 ve 2)", ra == 1 && rb == 2);
    test_sonuc("S2: her gorev ρ_sahip aldi (ikisi de NULL degil)",
               rho_a != NULL && rho_b != NULL);
    test_sonuc("S1: iki gorevin ρ_sahip'i AYRI (bolge paylasilmiyor)",
               rho_a != rho_b);
}

/* === 5: cok gorev — sonuc/handle karismasi yok === */
static int32_t isci_sabit_7(void *rho) { (void)rho; return 7; }

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

int main(void) {
    printf("=== KEMGU gorev runtime (Katman 2 / DRF V1) testleri ===\n");
    test_bare_yol();
    test_kapanis_yolu();
    test_gercek_thread();
    test_bolge_ayrikligi();
    test_cok_gorev();
    test_null_handle();
    printf("=== %d/%d test gecti ===\n", basarili, toplam_test);
    return basarisiz == 0 ? 0 : 1;
}
