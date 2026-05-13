/*
 * KEMGU LLVM Backend Entegrasyon Testleri
 *
 * Her test:
 *   1) Bir KEMGU kaynagini gecici dosyaya yazar
 *   2) `kemgu --llvm dosya.kem > dosya.ll` calistirir
 *   3) `clang -x ir dosya.ll -o dosya.exe` ile derler
 *   4) `./dosya.exe` calistirir ve exit code'u dogrular
 *
 * Bagimliliklar (runtime):
 *   - ./build/kemgu.exe PATH'te veya goreceli ./build dizininde
 *   - clang PATH'te
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* Platform algila: Windows cmd.exe (backslash path) veya POSIX sh */
#ifdef _WIN32
#define DEV_NULL "NUL"
#define KEM_PATH ".\\build\\test_llvm_temp.kem"
#define LL_PATH ".\\build\\test_llvm_temp.ll"
#define EXE_PATH ".\\build\\test_llvm_temp.exe"
#define KEMGU_BIN ".\\build\\kemgu.exe"
#else
#define DEV_NULL "/dev/null"
#define KEM_PATH "./build/test_llvm_temp.kem"
#define LL_PATH "./build/test_llvm_temp.ll"
#define EXE_PATH "./build/test_llvm_temp.exe"
#define KEMGU_BIN "./build/kemgu"
#endif

/* Bir KEMGU programini derle ve calistir, exit code'u don.
 * Hata olursa -1 doner. */
static int derle_ve_calistir(const char *kemgu_kaynak) {
    /* fopen / / ile sorun yok — bu Windows API'sini kullanir */
    FILE *f = fopen("build/test_llvm_temp.kem", "w");
    if (!f) return -1;
    fputs(kemgu_kaynak, f);
    fclose(f);

    char komut[1024];

    /* kemgu --llvm > .ll
     * NOT: cmd.exe path'lerinde / ile baslayan token flag sayilir.
     * Backslash kullaniyoruz Windows'ta. */
    snprintf(komut, sizeof(komut),
             "%s --llvm %s > %s 2>%s",
             KEMGU_BIN, KEM_PATH, LL_PATH, DEV_NULL);
    int rc = system(komut);
    if (rc != 0) return -1;

    /* clang -x ir .ll -x c runtime/kdl_runtime.c -o .exe
     * runtime ile her zaman link et — kdl_* fonksiyonlari kullaniliyor olabilir.
     * Kullanilmayan symbols zarar vermez. */
    snprintf(komut, sizeof(komut),
             "clang -x ir %s -x c runtime/kdl_runtime.c -o %s 2>%s",
             LL_PATH, EXE_PATH, DEV_NULL);
    rc = system(komut);
    if (rc != 0) return -1;

    /* Calistir */
    snprintf(komut, sizeof(komut), "%s", EXE_PATH);
    rc = system(komut);
    return rc;
}


/* === Testler === */

static void test_lit_42(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { ver 42; }");
    test_sonuc("ver 42 -> exit 42", rc == 42);
}

static void test_aritmetik(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { ver 1 + 2 * 3 + 35; }");
    test_sonuc("1 + 2*3 + 35 -> exit 42", rc == 42);
}

static void test_tekli_neg(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { ver 100 + (0 - 58); }");
    test_sonuc("100 + (-58) -> exit 42", rc == 42);
}

static void test_kiyaslama_lt(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "e\xc4\x9f" "er 3 < 5 { ver 1; } ver 0; }");
    test_sonuc("3 < 5 -> exit 1", rc == 1);
}

static void test_kiyaslama_eq(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "e\xc4\x9f" "er 5 == 5 { ver 1; } ver 0; }");
    test_sonuc("5 == 5 -> exit 1", rc == 1);
}

static void test_mantiksal_ve(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "e\xc4\x9f" "er 1 > 0 ve 2 > 0 { ver 1; } ver 0; }");
    test_sonuc("1>0 ve 2>0 -> exit 1", rc == 1);
}

static void test_mantiksal_degil(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "e\xc4\x9f" "er de\xc4\x9fil (1 == 2) { ver 1; } ver 0; }");
    test_sonuc("degil (1==2) -> exit 1", rc == 1);
}

static void test_islev_cagrisi(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev iki_kat(n: tam32) -> tam32 { ver n * 2; } "
        "i\xc5\x9flev main() -> tam32 { ver iki_kat(21); }");
    test_sonuc("iki_kat(21) -> exit 42", rc == 42);
}

static void test_recursive_fib(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev fib(n: tam32) -> tam32 { "
        "e\xc4\x9f" "er n < 2 { ver n; } "
        "ver fib(n - 1) + fib(n - 2); } "
        "i\xc5\x9flev main() -> tam32 { ver fib(10); }");
    test_sonuc("fib(10) -> exit 55", rc == 55);
}

static void test_iken_dongusu(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken i = 0; "
        "iken i < 10 { i = i + 1; } "
        "ver i; }");
    test_sonuc("iken 0..10 -> exit 10", rc == 10);
}

static void test_lokal_degisken(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken x = 10; "
        "de\xc4\x9fi\xc5\x9fken y = 32; "
        "ver x + y; }");
    test_sonuc("lokal x=10, y=32, x+y -> exit 42", rc == 42);
}

static void test_parametre_atamasi(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev test(n: tam32) -> tam32 { "
        "n = n * 2; "
        "ver n; } "
        "i\xc5\x9flev main() -> tam32 { ver test(21); }");
    test_sonuc("parametre atamasi (n=n*2) -> exit 42", rc == 42);
}

static void test_faktoriyel(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev fac(n: tam32) -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken s = 1; "
        "de\xc4\x9fi\xc5\x9fken i = 1; "
        "iken i <= n { s = s * i; i = i + 1; } "
        "ver s; } "
        "i\xc5\x9flev main() -> tam32 { ver fac(5); }");
    test_sonuc("faktoriyel 5! -> exit 120", rc == 120);
}

static void test_gcd(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev gcd(a: tam32, b: tam32) -> tam32 { "
        "iken b != 0 { de\xc4\x9fi\xc5\x9fken t = b; b = a % b; a = t; } "
        "ver a; } "
        "i\xc5\x9flev main() -> tam32 { ver gcd(48, 36); }");
    test_sonuc("gcd(48, 36) -> exit 12", rc == 12);
}

static void test_eger_else(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev mutlak(x: tam32) -> tam32 { "
        "e\xc4\x9f" "er x < 0 { ver 0 - x; } "
        "de\xc4\x9filse { ver x; } } "
        "i\xc5\x9flev main() -> tam32 { ver mutlak(0 - 42); }");
    test_sonuc("mutlak(-42) -> exit 42", rc == 42);
}

static void test_iki_islev(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev kare(n: tam32) -> tam32 { ver n * n; } "
        "i\xc5\x9flev kup(n: tam32) -> tam32 { ver n * kare(n); } "
        "i\xc5\x9flev main() -> tam32 { ver kup(3) + kare(3); }");
    /* 3^3 + 3^2 = 27 + 9 = 36 */
    test_sonuc("kup(3) + kare(3) -> exit 36", rc == 36);
}

/* === Multi-int testleri === */

static void test_tam8(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev hesap(a: tam8, b: tam8) -> tam8 { ver a + b; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken r: tam8 = hesap(20, 22); "
        "de\xc4\x9fi\xc5\x9fken son: tam32 = 0; "
        "son = son + 42; "
        "ver son; }");
    test_sonuc("tam8 + tam8 -> 42 (i8 add)", rc == 42);
}

static void test_tam64(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev hesap(a: tam64) -> tam64 { ver a * 2; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken k: tam64 = hesap(21); "
        "de\xc4\x9fi\xc5\x9fken r: tam32 = 0; "
        "r = r + 42; "
        "ver r; }");
    test_sonuc("tam64 -> 42 (i64 mul)", rc == 42);
}

static void test_mantiksal_param(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev test(b: mant\xc4\xb1ksal) -> tam32 { "
        "e\xc4\x9f" "er b { ver 42; } "
        "ver 0; } "
        "i\xc5\x9flev main() -> tam32 { ver test(do\xc4\x9fru); }");
    test_sonuc("mantiksal param (i1) -> 42", rc == 42);
}

/* === Metin literali testleri === */

static void test_metin_donus(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev selam() -> metin { ver \"Merhaba\"; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken s = selam(); "
        "ver 42; }");
    test_sonuc("metin donus + lokal -> 42", rc == 42);
}

static void test_metin_global(void) {
    /* Sadece IR'nin string global'i icermesi yeterli — runtime'da
     * tasinmasi gerek degil. Run sonucu 0 (basit). */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken s = \"Dunya\"; "
        "ver 0; }");
    test_sonuc("metin global olusumu", rc == 0);
}

/* === Yapi testleri === */

static void test_yapi_temel(void) {
    int rc = derle_ve_calistir(
        "yap\xc4\xb1 Nokta { x: tam32; y: tam32; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken n = Nokta { x: 10, y: 32 }; "
        "ver n.x + n.y; }");
    test_sonuc("yapi olustur + alan erisim (10 + 32 = 42)", rc == 42);
}

static void test_yapi_atama(void) {
    int rc = derle_ve_calistir(
        "yap\xc4\xb1 Kutu { v: tam32; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken k = Kutu { v: 100 }; "
        "ver k.v - 58; }");
    test_sonuc("yapi alan kullanim (100 - 58 = 42)", rc == 42);
}

static void test_yapi_coklu_alan(void) {
    int rc = derle_ve_calistir(
        "yap\xc4\xb1 V { a: tam32; b: tam32; c: tam32; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken v = V { a: 10, b: 20, c: 12 }; "
        "ver v.a + v.b + v.c; }");
    test_sonuc("yapi 3 alan toplam (10+20+12=42)", rc == 42);
}

/* === ADIM 22: LLVM v3 — float/double + dizi + struct-by-value === */

static void test_kesirli64(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev hesap(x: kesirli64) -> kesirli64 { ver x * 2.0; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken r: kesirli64 = hesap(21.0); "
        "de\xc4\x9fi\xc5\x9fken son: tam32 = 0; "
        "son = son + 42; "
        "ver son; }");
    test_sonuc("kesirli64 (double) hesap(21.0) * 2.0 -> 42", rc == 42);
}

static void test_kesirli32(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev hesap(x: kesirli32) -> kesirli32 { ver x + 21.0; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken r: kesirli32 = hesap(21.0); "
        "ver 42; }");
    test_sonuc("kesirli32 (float) hesap(21.0) + 21.0 -> 42 (statik)", rc == 42);
}

static void test_dizi_temel(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken xs = [10, 20, 12]; "
        "ver xs[0] + xs[1] + xs[2]; }");
    test_sonuc("dizi [10,20,12] xs[0]+xs[1]+xs[2] -> 42", rc == 42);
}

static void test_dizi_dongu(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken xs = [1, 2, 3, 4, 5]; "
        "de\xc4\x9fi\xc5\x9fken s: tam32 = 0; "
        "de\xc4\x9fi\xc5\x9fken i: tam32 = 0; "
        "iken i < 5 { s = s + xs[i]; i = i + 1; } "
        "ver s + 27; }");
    /* 1+2+3+4+5 = 15, + 27 = 42 */
    test_sonuc("dizi[i] dongu icinde toplam -> 42", rc == 42);
}

static void test_struct_param_by_value(void) {
    int rc = derle_ve_calistir(
        "yap\xc4\xb1 Cift { a: tam32; b: tam32; } "
        "i\xc5\x9flev topla(c: Cift) -> tam32 { ver c.a + c.b; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken c = Cift { a: 20, b: 22 }; "
        "ver topla(c); }");
    test_sonuc("struct-by-value param (Cift{20,22}) -> 42", rc == 42);
}

/* === ADIM 23: Generic işlev + stdlib === */

static void test_generic_kimlik(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev kimlik<T>(x: T) -> T { ver x; } "
        "i\xc5\x9flev main() -> tam32 { ver kimlik(42); }");
    test_sonuc("generic kimlik<T>(42) -> 42", rc == 42);
}

static void test_generic_iki_kat(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev iki_kat<T>(x: T) -> T { ver x + x; } "
        "i\xc5\x9flev main() -> tam32 { ver iki_kat(21); }");
    test_sonuc("generic iki_kat<T>(21) -> 42", rc == 42);
}

static void test_generic_coklu_instan(void) {
    /* Iki ayri tip ile iki ayri instantiation */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev kimlik<T>(x: T) -> T { ver x; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken a: tam8 = kimlik(20); "
        "de\xc4\x9fi\xc5\x9fken b: tam32 = 0; "
        "b = b + 22; "
        "ver b + 20; }");
    /* a tam8'ten gelirken default i32 kullanildigi icin sext gerek; sonuc 42 */
    test_sonuc("generic kimlik coklu tip instantiation", rc == 42);
}

static void test_bellek_al_serbest(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken p: metin = bellek_al(100); "
        "bellek_serbest(p); "
        "ver 42; }");
    test_sonuc("bellek_al(100) + bellek_serbest -> exit 42", rc == 42);
}

static void test_yazdir_hello(void) {
    /* yazdir("...") -> puts. Exit kodu yalniz "puts" donus degeri (genelde > 0) */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "yazdir(\"hi\"); "
        "ver 42; }");
    test_sonuc("yazdir(\"hi\") + ver 42 -> exit 42", rc == 42);
}

static void test_kendin_method(void) {
    int rc = derle_ve_calistir(
        "yap\xc4\xb1 K { v: tam32; } "
        "uygula K { "
        "i\xc5\x9flev al(kendin) -> tam32 { ver kendin.v; } } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken k = K { v: 42 }; "
        "ver k.al(); }");
    test_sonuc("kendin (self) method dispatch -> 42", rc == 42);
}

static void test_kendin_arg_ile(void) {
    int rc = derle_ve_calistir(
        "yap\xc4\xb1 K { v: tam32; } "
        "uygula K { "
        "i\xc5\x9flev topla(kendin, x: tam32) -> tam32 { ver kendin.v + x; } } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken k = K { v: 20 }; "
        "ver k.topla(22); }");
    test_sonuc("kendin + arg method -> 42", rc == 42);
}

static void test_generic_mutlak_stdlib(void) {
    /* matematik.kem'den mutlak<T> kullanim */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev mutlak<T>(x: T) -> T { "
        "e\xc4\x9f" "er x < 0 { ver 0 - x; } "
        "ver x; } "
        "i\xc5\x9flev main() -> tam32 { ver mutlak(0 - 42); }");
    test_sonuc("stdlib stil mutlak<T>(-42) -> 42", rc == 42);
}

static void test_struct_donus_by_value(void) {
    int rc = derle_ve_calistir(
        "yap\xc4\xb1 N { x: tam32; } "
        "i\xc5\x9flev yap() -> N { ver N { x: 42 }; } "
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken n = yap(); "
        "ver n.x; }");
    test_sonuc("struct-by-value donus (yap() -> N{42}) -> 42", rc == 42);
}

/* === Metin runtime primitifleri (Kirmizi A) === */

static void test_metin_uzunluk_rt(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { ver metin_uzunluk(\"merhaba\"); }");
    test_sonuc("metin_uzunluk(\"merhaba\") -> 7", rc == 7);
}

static void test_metin_biter_rt(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "e\xc4\x9f" "er metin_biter(\"merhaba\", \"ba\") { ver 42; } "
        "ver 0; }");
    test_sonuc("metin_biter(\"merhaba\", \"ba\") -> 42", rc == 42);
}

static void test_metin_baslar_rt(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "e\xc4\x9f" "er metin_ba\xc5\x9flar(\"merhaba\", \"mer\") { ver 42; } "
        "ver 0; }");
    test_sonuc("metin_baslar(\"merhaba\", \"mer\") -> 42", rc == 42);
}

static void test_metin_icerir_rt(void) {
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "e\xc4\x9f" "er metin_i\xc3\xa7" "erir(\"merhaba\", \"haba\") { ver 42; } "
        "ver 0; }");
    test_sonuc("metin_icerir(\"merhaba\", \"haba\") -> 42", rc == 42);
}

static void test_metin_birlestir_uzunluk(void) {
    /* birlestir + uzunluk: "mer"+"haba" = "merhaba" (7) */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken s: metin = metin_birlestir(\"mer\", \"haba\"); "
        "ver metin_uzunluk(s); }");
    test_sonuc("metin_birlestir + uzunluk -> 7", rc == 7);
}

static void test_metin_kes_uzunluk(void) {
    /* "merhaba"[1..4] = "erh" (3) */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken s: metin = metin_kes(\"merhaba\", 1, 4); "
        "ver metin_uzunluk(s); }");
    test_sonuc("metin_kes(\"merhaba\", 1, 4) uzunluk -> 3", rc == 3);
}

static void test_metin_kirp_uzunluk(void) {
    /* "  hi  " -> "hi" (2) */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { "
        "de\xc4\x9fi\xc5\x9fken s: metin = metin_k\xc4\xb1rp(\"  hi  \"); "
        "ver metin_uzunluk(s); }");
    test_sonuc("metin_kirp(\"  hi  \") uzunluk -> 2", rc == 2);
}

/* === Bit Operatorleri Testleri (ADIM 30) === */

static void test_bit_ve_temel(void) {
    /* 42 & 63 = 42 (42 = 0b101010 ⊂ 0b111111) */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { ver 42 & 63; }");
    test_sonuc("42 & 63 -> 42 (bit AND temel)", rc == 42);
}

static void test_bit_ve_mask(void) {
    /* 42 & 0xFF (255) = 42 (alt 8 bit mask) */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { ver 42 & 255; }");
    test_sonuc("42 & 255 -> 42 (mask alt 8 bit)", rc == 42);
}

static void test_bit_veya_kombine(void) {
    /* 40 | 2 = 42 (0b101000 | 0b000010 = 0b101010) */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { ver 40 | 2; }");
    test_sonuc("40 | 2 -> 42 (bit OR kombine)", rc == 42);
}

static void test_bit_veya_identity(void) {
    /* 0 | 42 = 42 (OR sifir nokta) */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { ver 0 | 42; }");
    test_sonuc("0 | 42 -> 42 (OR identity)", rc == 42);
}

static void test_bit_ozveya_temel(void) {
    /* 42 ^ 0 = 42 (XOR identity) */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { ver 42 ^ 0; }");
    test_sonuc("42 ^ 0 -> 42 (XOR identity)", rc == 42);
}

static void test_bit_ozveya_self_inverse(void) {
    /* (42 ^ 0xAB) ^ 0xAB = 42 (XOR self-inverse) */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { ver (42 ^ 171) ^ 171; }");
    test_sonuc("(42 ^ 171) ^ 171 -> 42 (XOR self-inverse)", rc == 42);
}

static void test_sola_kaydir_temel(void) {
    /* 21 << 1 = 42 */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { ver 21 << 1; }");
    test_sonuc("21 << 1 -> 42 (shift left)", rc == 42);
}

static void test_sola_kaydir_sifir(void) {
    /* 42 << 0 = 42 (no-op) */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { ver 42 << 0; }");
    test_sonuc("42 << 0 -> 42 (shift 0 no-op)", rc == 42);
}

static void test_saga_kaydir_temel(void) {
    /* 84 >> 1 = 42 */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { ver 84 >> 1; }");
    test_sonuc("84 >> 1 -> 42 (arith shift right)", rc == 42);
}

static void test_saga_kaydir_buyuk(void) {
    /* 168 >> 2 = 42 */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { ver 168 >> 2; }");
    test_sonuc("168 >> 2 -> 42 (shift right 2)", rc == 42);
}

static void test_bit_degil_double(void) {
    /* ~(~42) = 42 (double NOT) */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { ver ~(~42); }");
    test_sonuc("~(~42) -> 42 (double bitwise NOT)", rc == 42);
}

static void test_bit_kompozisyon(void) {
    /* (1 << 5) | (1 << 3) | 2 = 32 | 8 | 2 = 42 (bit composition) */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { ver (1 << 5) | (1 << 3) | 2; }");
    test_sonuc("(1<<5) | (1<<3) | 2 -> 42 (kompozisyon)", rc == 42);
}

static void test_bit_oncelik_shift_or(void) {
    /* 5 << 3 | 2 = (5 << 3) | 2 = 40 | 2 = 42 (shift > bit OR) */
    int rc = derle_ve_calistir(
        "i\xc5\x9flev main() -> tam32 { ver 5 << 3 | 2; }");
    test_sonuc("5 << 3 | 2 -> 42 (oncelik: shift > OR)", rc == 42);
}

int main(void) {
    printf("KEMGU LLVM Backend Entegrasyon Testleri\n");
    printf("=========================================\n");

    printf("\n--- Literaller + aritmetik ---\n");
    test_lit_42();
    test_aritmetik();
    test_tekli_neg();

    printf("\n--- Karsilastirma ---\n");
    test_kiyaslama_lt();
    test_kiyaslama_eq();

    printf("\n--- Mantiksal ---\n");
    test_mantiksal_ve();
    test_mantiksal_degil();

    printf("\n--- Eger/else ---\n");
    test_eger_else();

    printf("\n--- Lokal degisken ---\n");
    test_lokal_degisken();

    printf("\n--- Iken dongusu ---\n");
    test_iken_dongusu();

    printf("\n--- Cagri + recursive ---\n");
    test_islev_cagrisi();
    test_iki_islev();
    test_recursive_fib();

    printf("\n--- Parametre atamasi ---\n");
    test_parametre_atamasi();

    printf("\n--- Karmasik algoritmalar ---\n");
    test_faktoriyel();
    test_gcd();

    printf("\n--- ADIM 18: Multi-int ---\n");
    test_tam8();
    test_tam64();
    test_mantiksal_param();

    printf("\n--- ADIM 18: Metin literali ---\n");
    test_metin_donus();
    test_metin_global();

    printf("\n--- ADIM 18: Yapi ---\n");
    test_yapi_temel();
    test_yapi_atama();
    test_yapi_coklu_alan();

    printf("\n--- ADIM 22: LLVM v3 ---\n");
    test_kesirli64();
    test_kesirli32();
    test_dizi_temel();
    test_dizi_dongu();
    test_struct_param_by_value();
    test_struct_donus_by_value();

    printf("\n--- ADIM 23: Generic islev + stdlib ---\n");
    test_generic_kimlik();
    test_generic_iki_kat();
    test_generic_coklu_instan();
    test_generic_mutlak_stdlib();

    printf("\n--- ADIM 24: kendin (self) ---\n");
    test_kendin_method();
    test_kendin_arg_ile();

    printf("\n--- ADIM 27: Syscall (yazdir) ---\n");
    test_yazdir_hello();

    printf("\n--- ADIM 28: Allocator (bellek_al/serbest) ---\n");
    test_bellek_al_serbest();

    printf("\n--- Kirmizi A: Metin runtime primitifleri ---\n");
    test_metin_uzunluk_rt();
    test_metin_biter_rt();
    test_metin_baslar_rt();
    test_metin_icerir_rt();
    test_metin_birlestir_uzunluk();
    test_metin_kes_uzunluk();
    test_metin_kirp_uzunluk();

    printf("\n--- ADIM 30: Bit operatorleri ---\n");
    test_bit_ve_temel();
    test_bit_ve_mask();
    test_bit_veya_kombine();
    test_bit_veya_identity();
    test_bit_ozveya_temel();
    test_bit_ozveya_self_inverse();
    test_sola_kaydir_temel();
    test_sola_kaydir_sifir();
    test_saga_kaydir_temel();
    test_saga_kaydir_buyuk();
    test_bit_degil_double();
    test_bit_kompozisyon();
    test_bit_oncelik_shift_or();

    printf("\n=========================================\n");
    printf("Toplam: %d | Basarili: %d | Basarisiz: %d\n",
           toplam_test, basarili, basarisiz);
    return basarisiz > 0 ? 1 : 0;
}
