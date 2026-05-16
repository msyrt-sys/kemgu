/*
 * KEMGU --hedef CLI Flag + LLVM Emission Hedef-Duyarliligi Testleri
 * ===================================================================
 *
 * Faz Bare-metal Kalem 1 + 2 birlestirilmis testler.
 *
 * Strateji: kemgu --llvm --hedef=<triple> ile IR uret (system() ile dosyaya
 * redirect), sonra dosyayi okuyup parse et (declare bazli IR introspection,
 * yalniz grep degil).
 *
 * Toplam: 8 test.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define KEMGU_BIN ".\\build\\kemgu.exe"
#define DEV_NULL "NUL"
#define KEM_PATH ".\\build\\test_hedef_tmp.kem"
#define LL_PATH  ".\\build\\test_hedef_tmp.ll"
#define ERR_PATH ".\\build\\test_hedef_tmp.err"
#else
#define KEMGU_BIN "./build/kemgu"
#define DEV_NULL "/dev/null"
#define KEM_PATH "./build/test_hedef_tmp.kem"
#define LL_PATH  "./build/test_hedef_tmp.ll"
#define ERR_PATH "./build/test_hedef_tmp.err"
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

/* === IR introspection === */

typedef struct {
    char triple[128];
    int kdl_yetki_tip;      /* %kdl_yetki = type ... var mi */
    int declare_sayisi;     /* declare ... satir sayisi */
    int bare_metal_yorumu;  /* "Bare-metal hedef:" var mi */
} IrAnaliz;

static int satir_baslar_mi(const char *satir, const char *prefix) {
    while (*satir == ' ' || *satir == '\t') satir++;
    return strncmp(satir, prefix, strlen(prefix)) == 0;
}

static void ir_analiz_et(const char *ir, IrAnaliz *a) {
    memset(a, 0, sizeof(*a));
    const char *p = ir;
    while (*p) {
        const char *son = strchr(p, '\n');
        if (!son) son = p + strlen(p);
        int uz = (int)(son - p);
        if (uz > 1023) uz = 1023;
        char satir[1024];
        memcpy(satir, p, (size_t)uz);
        satir[uz] = '\0';

        if (satir_baslar_mi(satir, "target triple = \"")) {
            const char *bas = strchr(satir, '"');
            if (bas) {
                bas++;
                const char *sn = strchr(bas, '"');
                if (sn && (sn - bas) < (long)sizeof(a->triple)) {
                    int n = (int)(sn - bas);
                    memcpy(a->triple, bas, (size_t)n);
                    a->triple[n] = '\0';
                }
            }
        }
        if (strstr(satir, "%kdl_yetki = type") != NULL) {
            a->kdl_yetki_tip = 1;
        }
        if (satir_baslar_mi(satir, "declare ")) {
            a->declare_sayisi++;
        }
        if (strstr(satir, "Bare-metal hedef:") != NULL) {
            a->bare_metal_yorumu = 1;
        }

        if (*son == '\0') break;
        p = son + 1;
    }
}

/* === Yardimcilar: dosya yaz / oku === */

static int kaynak_yaz(const char *yol, const char *icerik) {
    FILE *f = fopen(yol, "w");
    if (!f) return -1;
    fputs(icerik, f);
    fclose(f);
    return 0;
}

static int dosya_oku(const char *yol, char *tampon, size_t cap) {
    FILE *f = fopen(yol, "rb");
    if (!f) return -1;
    size_t okunan = fread(tampon, 1, cap - 1, f);
    tampon[okunan] = '\0';
    fclose(f);
    return (int)okunan;
}

#define BUYUK_TAMPON (1 << 17)

/* kemgu --llvm [--hedef=...] <kem> > LL_PATH 2> ERR_PATH; rc dondur */
static int llvm_calistir(const char *hedef_arg) {
    char komut[1024];
    if (hedef_arg && hedef_arg[0]) {
        snprintf(komut, sizeof(komut),
                 "%s --llvm %s %s > %s 2> %s",
                 KEMGU_BIN, hedef_arg, KEM_PATH, LL_PATH, ERR_PATH);
    } else {
        snprintf(komut, sizeof(komut),
                 "%s --llvm %s > %s 2> %s",
                 KEMGU_BIN, KEM_PATH, LL_PATH, ERR_PATH);
    }
    return system(komut);
}

static const char *KAYNAK_BASIT =
    "i\xc5\x9flev main() -> tam32 { ver 42; }\n";

/* ========================================================================
 * Testler
 * ======================================================================== */

static void T1_default_triple(void) {
    kaynak_yaz(KEM_PATH, KAYNAK_BASIT);
    llvm_calistir(NULL);
    static char ir[BUYUK_TAMPON];
    dosya_oku(LL_PATH, ir, sizeof(ir));
    IrAnaliz a;
    ir_analiz_et(ir, &a);
    int ok = strcmp(a.triple, "x86_64-pc-windows-gnu") == 0
          && a.declare_sayisi > 30
          && a.kdl_yetki_tip == 1
          && a.bare_metal_yorumu == 0;
    test_sonuc("H1: default triple = host + libc declare'leri var", ok);
}

static void T2_aarch64_bare_metal(void) {
    llvm_calistir("--hedef=aarch64-unknown-none");
    static char ir[BUYUK_TAMPON];
    dosya_oku(LL_PATH, ir, sizeof(ir));
    IrAnaliz a;
    ir_analiz_et(ir, &a);
    int ok = strcmp(a.triple, "aarch64-unknown-none") == 0
          && a.declare_sayisi == 0
          && a.kdl_yetki_tip == 1
          && a.bare_metal_yorumu == 1;
    test_sonuc("H2: aarch64-unknown-none bare-metal, libc declare YOK", ok);
}

static void T3_linux_host(void) {
    llvm_calistir("--hedef=x86_64-unknown-linux-gnu");
    static char ir[BUYUK_TAMPON];
    dosya_oku(LL_PATH, ir, sizeof(ir));
    IrAnaliz a;
    ir_analiz_et(ir, &a);
    int ok = strcmp(a.triple, "x86_64-unknown-linux-gnu") == 0
          && a.declare_sayisi > 30
          && a.kdl_yetki_tip == 1
          && a.bare_metal_yorumu == 0;
    test_sonuc("H3: x86_64-unknown-linux-gnu host + libc var", ok);
}

static void T4_gecersiz_triple_pass_through(void) {
    llvm_calistir("--hedef=garip-triple");
    static char ir[BUYUK_TAMPON];
    dosya_oku(LL_PATH, ir, sizeof(ir));
    IrAnaliz a;
    ir_analiz_et(ir, &a);
    /* Pass-through: triple yine yazilir, host gibi davranis (declare var) */
    int ok = strcmp(a.triple, "garip-triple") == 0
          && a.declare_sayisi > 30
          && a.bare_metal_yorumu == 0;
    test_sonuc("H4: bilinmeyen triple pass-through (uyari + host davranis)",
               ok);
}

static void T5_bos_hedef_hata(void) {
    /* --hedef= bos -> hata mesaji stderr'e + return 2; IR uretilmemis */
    llvm_calistir("--hedef=");
    static char err[BUYUK_TAMPON];
    int n = dosya_oku(ERR_PATH, err, sizeof(err));
    /* IR dosyasi olabilir ama bos */
    int ok = (n > 0) && strstr(err, "bos olamaz") != NULL;
    test_sonuc("H5: --hedef= (bos) hata mesaji stderr'de", ok);
}

static void T6_bare_metal_kdl_yetki_tip_korunur(void) {
    /* Tip struct definition bare-metal'de de kalir */
    llvm_calistir("--hedef=aarch64-unknown-none");
    static char ir[BUYUK_TAMPON];
    dosya_oku(LL_PATH, ir, sizeof(ir));
    int ok = strstr(ir, "%kdl_yetki = type { i64, i16, i16, i8") != NULL
          && strstr(ir, "declare %kdl_yetki @kdl_yetki_olustur") == NULL;
    test_sonuc("H6: bare-metal'de %kdl_yetki tip VAR, runtime declare YOK",
               ok);
}

static void T7_bare_metal_kdl_dosya_yok(void) {
    llvm_calistir("--hedef=aarch64-unknown-none");
    static char ir[BUYUK_TAMPON];
    dosya_oku(LL_PATH, ir, sizeof(ir));
    int ok = strstr(ir, "@kdl_dosya_ac") == NULL
          && strstr(ir, "@kdl_dosya_oku") == NULL
          && strstr(ir, "@kdl_dosya_yaz") == NULL
          && strstr(ir, "@kdl_dosya_kapat") == NULL;
    test_sonuc("H7: bare-metal'de kdl_dosya_* declare YOK", ok);
}

static void T8_bare_metal_kdl_metin_yok(void) {
    llvm_calistir("--hedef=aarch64-unknown-none");
    static char ir[BUYUK_TAMPON];
    dosya_oku(LL_PATH, ir, sizeof(ir));
    int ok = strstr(ir, "@kdl_metin_uzunluk") == NULL
          && strstr(ir, "@kdl_metin_birlestir") == NULL
          && strstr(ir, "@puts") == NULL
          && strstr(ir, "@malloc") == NULL;
    test_sonuc("H8: bare-metal'de kdl_metin_* + libc (puts/malloc) YOK", ok);
}

/* === Kalem 7: 3 ek IR-level test === */

static void T9_host_bare_declare_farki(void) {
    /* Host vs bare-metal declare sayisi farki: bare-metal 0,
     * host 30'dan fazla. Fark = host emit'i. */
    llvm_calistir(NULL);  /* default host */
    static char ir_host[BUYUK_TAMPON];
    dosya_oku(LL_PATH, ir_host, sizeof(ir_host));
    IrAnaliz host_a;
    ir_analiz_et(ir_host, &host_a);

    llvm_calistir("--hedef=aarch64-unknown-none");
    static char ir_bare[BUYUK_TAMPON];
    dosya_oku(LL_PATH, ir_bare, sizeof(ir_bare));
    IrAnaliz bare_a;
    ir_analiz_et(ir_bare, &bare_a);

    /* Beklenen: host > 30, bare-metal = 0, fark >= 30 */
    int ok = host_a.declare_sayisi > 30
          && bare_a.declare_sayisi == 0
          && (host_a.declare_sayisi - bare_a.declare_sayisi) >= 30;
    test_sonuc("H9: host-bare declare sayi farki >= 30 (libc'siz)", ok);
}

static void T10_islev_emit_her_iki_modda(void) {
    /* main() islevi bare-metal'de de host'ta da emit edilir (KEMGU programci
     * kodu hedef-bagimsiz). 'define ... @main' satiri her iki modda da var. */
    llvm_calistir(NULL);
    static char ir_host[BUYUK_TAMPON];
    dosya_oku(LL_PATH, ir_host, sizeof(ir_host));

    llvm_calistir("--hedef=aarch64-unknown-none");
    static char ir_bare[BUYUK_TAMPON];
    dosya_oku(LL_PATH, ir_bare, sizeof(ir_bare));

    int host_has = strstr(ir_host, "define ") != NULL
                && strstr(ir_host, "@main") != NULL;
    int bare_has = strstr(ir_bare, "define ") != NULL
                && strstr(ir_bare, "@main") != NULL;
    test_sonuc("H10: main() define her iki hedefte emit (kullanici kod hedef-bagimsiz)",
               host_has && bare_has);
}

static void T11_yorum_baremetal_tutarli(void) {
    /* Bare-metal yorum 'libc/KDL declare'leri atlandi' tek mesaji,
     * host modunda yok. */
    llvm_calistir("--hedef=aarch64-unknown-none");
    static char ir_bare[BUYUK_TAMPON];
    dosya_oku(LL_PATH, ir_bare, sizeof(ir_bare));

    llvm_calistir(NULL);
    static char ir_host[BUYUK_TAMPON];
    dosya_oku(LL_PATH, ir_host, sizeof(ir_host));

    int bare_yorum = strstr(ir_bare, "Bare-metal hedef:") != NULL
                  && strstr(ir_bare, "atlandi") != NULL;
    int host_yorum_yok = strstr(ir_host, "Bare-metal hedef:") == NULL
                      && strstr(ir_host, "atlandi") == NULL;
    test_sonuc("H11: bare-metal yorum tutarli (var bare, yok host)",
               bare_yorum && host_yorum_yok);
}

int main(void) {
    printf("=== KEMGU --hedef CLI flag + LLVM Emission Testleri ===\n");
    printf("Bare-metal Faz Kalem 1+2: hedef-duyarli IR emission\n\n");

    T1_default_triple();
    T2_aarch64_bare_metal();
    T3_linux_host();
    T4_gecersiz_triple_pass_through();
    T5_bos_hedef_hata();
    T6_bare_metal_kdl_yetki_tip_korunur();
    T7_bare_metal_kdl_dosya_yok();
    T8_bare_metal_kdl_metin_yok();
    T9_host_bare_declare_farki();
    T10_islev_emit_her_iki_modda();
    T11_yorum_baremetal_tutarli();

    /* Temizlik */
    remove(KEM_PATH);
    remove(LL_PATH);
    remove(ERR_PATH);

    printf("\n=== %d/%d test gecti (basarili) ===\n", basarili, toplam_test);
    return basarisiz > 0 ? 1 : 0;
}
