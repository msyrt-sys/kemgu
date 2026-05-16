/*
 * KEMGU Bare-Metal Target Test Paketi (Faz 5 — Altyapi Bootstrap)
 * ================================================================
 *
 * --hedef bare-metal-x86_64 / bare-metal-aarch64 cikti dogrulamasi:
 *   - Triple x86_64-unknown-none / aarch64-unknown-none
 *   - kdl_* runtime declare'leri emit edilmez
 *   - %kdl_yetki struct tipi emit edilmez
 *   - clang -target ... -nostdlib -c ile ELF object derlenir
 *   - ld.lld + linker script ile final ELF binary uretilir
 *   - Entry point: _baslat (KEMGU programi tanimlar)
 *   - libc symbol (puts, malloc, printf) YOK
 *
 * Test stratejisi: system() ile kemgu komutu calistirir, .ll/.o dosyalarini
 * okur veya llvm-objdump cikitsini grep eder.
 *
 * QEMU boot testi: manuel (Linux/macOS ortaminda); bu test paketi sadece
 * derleme + sembol tablosu seviyesinde dogrulama yapar.
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

#ifdef _WIN32
#define DEV_NULL "NUL"
#define KEM_PATH "build\\test_bm_temp.kem"
#define LL_PATH  "build\\test_bm_temp.ll"
#define O_PATH   "build\\test_bm_temp.o"
#define ELF_PATH "build\\test_bm_temp.elf"
#define OUT_PATH "build\\test_bm_out.txt"
#define KEMGU_BIN ".\\build\\kemgu.exe"
#define LINK_X86 "src\\runtime_bare\\linker_x86_64.ld"
#define LINK_A64 "src\\runtime_bare\\linker_aarch64.ld"
#else
#define DEV_NULL "/dev/null"
#define KEM_PATH "build/test_bm_temp.kem"
#define LL_PATH  "build/test_bm_temp.ll"
#define O_PATH   "build/test_bm_temp.o"
#define ELF_PATH "build/test_bm_temp.elf"
#define OUT_PATH "build/test_bm_out.txt"
#define KEMGU_BIN "./build/kemgu"
#define LINK_X86 "src/runtime_bare/linker_x86_64.ld"
#define LINK_A64 "src/runtime_bare/linker_aarch64.ld"
#endif

/* KEMGU programini dosyaya yaz */
static int kemgu_yaz(const char *kaynak) {
    FILE *f = fopen("build/test_bm_temp.kem", "w");
    if (!f) return -1;
    fputs(kaynak, f);
    fclose(f);
    return 0;
}

/* kemgu --llvm --hedef X kaynak > out.ll
 * Donus: 0 = OK, !=0 = hata */
static int kemgu_llvm_uret(const char *hedef) {
    char komut[1024];
    if (hedef && *hedef) {
        snprintf(komut, sizeof(komut),
                 "%s --llvm --hedef %s %s > %s 2>%s",
                 KEMGU_BIN, hedef, KEM_PATH, LL_PATH, DEV_NULL);
    } else {
        snprintf(komut, sizeof(komut),
                 "%s --llvm %s > %s 2>%s",
                 KEMGU_BIN, KEM_PATH, LL_PATH, DEV_NULL);
    }
    return system(komut);
}

/* .ll dosyasini oku, doner pointer (kullanim sonrasi free) */
static char *ll_oku(void) {
    FILE *f = fopen("build/test_bm_temp.ll", "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f);
    buf[r] = '\0';
    fclose(f);
    return buf;
}

/* clang -target -nostdlib -c ile ELF object urettir
 * Donus: 0 = OK */
static int clang_object_derle(const char *target) {
    char komut[1024];
    snprintf(komut, sizeof(komut),
             "clang -target %s -nostdlib -c %s -o %s 2>%s",
             target, LL_PATH, O_PATH, DEV_NULL);
    return system(komut);
}

/* ld.lld ile linker script kullanarak ELF binary uret */
static int lld_link(const char *machine, const char *script) {
    char komut[1024];
    snprintf(komut, sizeof(komut),
             "ld.lld -m %s -T %s %s -o %s 2>%s",
             machine, script, O_PATH, ELF_PATH, DEV_NULL);
    return system(komut);
}

/* llvm-objdump -t ile sembol tablosu cikar, OUT_PATH'e yaz */
static int objdump_symbols(const char *path) {
    char komut[1024];
    snprintf(komut, sizeof(komut),
             "llvm-objdump -t %s > %s 2>%s",
             path, OUT_PATH, DEV_NULL);
    return system(komut);
}

/* OUT_PATH dosyasini oku, doner pointer */
static char *out_oku(void) {
    FILE *f = fopen("build/test_bm_out.txt", "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f);
    buf[r] = '\0';
    fclose(f);
    return buf;
}

static const char *KAYNAK_BASIT =
    "i\xc5\x9flev _baslat() -> tam32 { ver 42; }\n";

static const char *KAYNAK_HESAP =
    "i\xc5\x9flev hesap(x: tam32) -> tam32 { ver x * 2 + 1; }\n"
    "i\xc5\x9flev _baslat() -> tam32 { ver hesap(20) + 1; }\n";

/* === T1-T4: Triple kontrol === */

static void test_t01_default_host(void) {
    kemgu_yaz("i\xc5\x9flev main() -> tam32 { ver 42; }\n");
    int rc = kemgu_llvm_uret(NULL);
    char *ll = ll_oku();
    int ok = (rc == 0) && ll && strstr(ll, "x86_64-pc-windows-gnu") != NULL;
    free(ll);
    test_sonuc("T1 default --hedef -> windows-gnu triple", ok);
}

static void test_t02_hedef_host_explicit(void) {
    kemgu_yaz("i\xc5\x9flev main() -> tam32 { ver 42; }\n");
    int rc = kemgu_llvm_uret("host");
    char *ll = ll_oku();
    int ok = (rc == 0) && ll && strstr(ll, "x86_64-pc-windows-gnu") != NULL;
    free(ll);
    test_sonuc("T2 --hedef host explicit -> windows-gnu", ok);
}

static void test_t03_hedef_bare_x86_64(void) {
    kemgu_yaz(KAYNAK_BASIT);
    int rc = kemgu_llvm_uret("bare-metal-x86_64");
    char *ll = ll_oku();
    int ok = (rc == 0) && ll && strstr(ll, "x86_64-unknown-none") != NULL;
    free(ll);
    test_sonuc("T3 bare-metal-x86_64 triple", ok);
}

static void test_t04_hedef_bare_aarch64(void) {
    kemgu_yaz(KAYNAK_BASIT);
    int rc = kemgu_llvm_uret("bare-metal-aarch64");
    char *ll = ll_oku();
    int ok = (rc == 0) && ll && strstr(ll, "aarch64-unknown-none") != NULL;
    free(ll);
    test_sonuc("T4 bare-metal-aarch64 triple", ok);
}

/* === T5-T8: declare blok skip kontrol === */

static void test_t05_host_kdl_var(void) {
    kemgu_yaz(KAYNAK_BASIT);
    kemgu_llvm_uret("host");
    char *ll = ll_oku();
    int ok = ll && strstr(ll, "@kdl_yazdir_tam") != NULL;
    free(ll);
    test_sonuc("T5 host modunda kdl_yazdir_tam declare VAR", ok);
}

static void test_t06_bare_x86_kdl_yok(void) {
    kemgu_yaz(KAYNAK_BASIT);
    kemgu_llvm_uret("bare-metal-x86_64");
    char *ll = ll_oku();
    int ok = ll && strstr(ll, "@kdl_yazdir_tam") == NULL
                && strstr(ll, "@kdl_kanal_gonder") == NULL;
    free(ll);
    test_sonuc("T6 bare-metal-x86_64 modunda kdl_* declare YOK", ok);
}

static void test_t07_bare_aarch64_kdl_yok(void) {
    kemgu_yaz(KAYNAK_BASIT);
    kemgu_llvm_uret("bare-metal-aarch64");
    char *ll = ll_oku();
    int ok = ll && strstr(ll, "@kdl_metin_uzunluk") == NULL
                && strstr(ll, "@kdl_dosya_oku") == NULL;
    free(ll);
    test_sonuc("T7 bare-metal-aarch64 modunda kdl_* declare YOK", ok);
}

static void test_t08_bare_kdl_yetki_yok(void) {
    kemgu_yaz(KAYNAK_BASIT);
    kemgu_llvm_uret("bare-metal-x86_64");
    char *ll = ll_oku();
    int ok = ll && strstr(ll, "%kdl_yetki = type") == NULL;
    free(ll);
    test_sonuc("T8 bare-metal modunda %kdl_yetki type tanimi YOK", ok);
}

/* === T9-T12: Clang ile ELF object derleme === */

static void test_t09_bare_x86_object_olusur(void) {
    kemgu_yaz(KAYNAK_BASIT);
    kemgu_llvm_uret("bare-metal-x86_64");
    int rc = clang_object_derle("x86_64-unknown-none");
    test_sonuc("T9 bare-metal-x86_64 ELF object derlenir", rc == 0);
}

static void test_t10_bare_aarch64_object_olusur(void) {
    kemgu_yaz(KAYNAK_BASIT);
    kemgu_llvm_uret("bare-metal-aarch64");
    int rc = clang_object_derle("aarch64-unknown-none");
    test_sonuc("T10 bare-metal-aarch64 ELF object derlenir", rc == 0);
}

static void test_t11_bare_x86_baslat_sembol(void) {
    kemgu_yaz(KAYNAK_BASIT);
    kemgu_llvm_uret("bare-metal-x86_64");
    clang_object_derle("x86_64-unknown-none");
    objdump_symbols(O_PATH);
    char *out = out_oku();
    int ok = out && strstr(out, "_baslat") != NULL;
    free(out);
    test_sonuc("T11 ELF symbol tablosunda _baslat global var", ok);
}

static void test_t12_bare_x86_libc_yok(void) {
    kemgu_yaz(KAYNAK_BASIT);
    kemgu_llvm_uret("bare-metal-x86_64");
    clang_object_derle("x86_64-unknown-none");
    objdump_symbols(O_PATH);
    char *out = out_oku();
    /* libc/runtime sembolleri olmamali (puts, malloc, kdl_*) */
    int ok = out && strstr(out, "puts") == NULL
                 && strstr(out, "malloc") == NULL
                 && strstr(out, "kdl_") == NULL;
    free(out);
    test_sonuc("T12 ELF symbol tablosunda libc/kdl_ YOK", ok);
}

/* === T13-T15: Linker script + final ELF === */

static void test_t13_bare_x86_linker_script(void) {
    kemgu_yaz(KAYNAK_HESAP);
    kemgu_llvm_uret("bare-metal-x86_64");
    clang_object_derle("x86_64-unknown-none");
    int rc = lld_link("elf_x86_64", LINK_X86);
    test_sonuc("T13 ld.lld + x86_64 linker script -> ELF binary", rc == 0);
}

static void test_t14_bare_x86_entry_0x100000(void) {
    kemgu_yaz(KAYNAK_HESAP);
    kemgu_llvm_uret("bare-metal-x86_64");
    clang_object_derle("x86_64-unknown-none");
    lld_link("elf_x86_64", LINK_X86);
    objdump_symbols(ELF_PATH);
    char *out = out_oku();
    /* _baslat 0x10... aralıginda olmali (1MB+) */
    int ok = out && strstr(out, "00000000001") != NULL
                 && strstr(out, "_baslat") != NULL;
    free(out);
    test_sonuc("T14 x86_64 ELF entry _baslat 1MB+ adreste", ok);
}

static void test_t15_bare_aarch64_linker_script(void) {
    kemgu_yaz(KAYNAK_HESAP);
    kemgu_llvm_uret("bare-metal-aarch64");
    clang_object_derle("aarch64-unknown-none");
    int rc = lld_link("aarch64elf", LINK_A64);
    test_sonuc("T15 ld.lld + aarch64 linker script -> ELF binary", rc == 0);
}

/* === T16-T17: CLI hata path'i === */

static void test_t16_bilinmeyen_hedef(void) {
    kemgu_yaz(KAYNAK_BASIT);
    char komut[512];
    snprintf(komut, sizeof(komut),
             "%s --llvm --hedef invalid-target %s > %s 2>%s",
             KEMGU_BIN, KEM_PATH, DEV_NULL, DEV_NULL);
    int rc = system(komut);
    /* Windows'ta exit code 0x02 olarak doner — yorum lazim. */
    test_sonuc("T16 bilinmeyen --hedef -> exit !=0", rc != 0);
}

static void test_t17_hedef_arg_yok(void) {
    kemgu_yaz(KAYNAK_BASIT);
    char komut[512];
    snprintf(komut, sizeof(komut),
             "%s --llvm --hedef > %s 2>%s",
             KEMGU_BIN, DEV_NULL, DEV_NULL);
    int rc = system(komut);
    test_sonuc("T17 --hedef parametresiz -> exit !=0", rc != 0);
}

int main(void) {
    printf("\n=== KEMGU Bare-Metal Target Test Paketi (Faz 5) ===\n");
    printf("(--hedef bayragi + LLVM triple + ELF object/binary)\n\n");

    printf("--- T1-T4: Triple secimi ---\n");
    test_t01_default_host();
    test_t02_hedef_host_explicit();
    test_t03_hedef_bare_x86_64();
    test_t04_hedef_bare_aarch64();

    printf("\n--- T5-T8: declare blok skip (bare-metal) ---\n");
    test_t05_host_kdl_var();
    test_t06_bare_x86_kdl_yok();
    test_t07_bare_aarch64_kdl_yok();
    test_t08_bare_kdl_yetki_yok();

    printf("\n--- T9-T12: ELF object derleme + sembol kontrol ---\n");
    test_t09_bare_x86_object_olusur();
    test_t10_bare_aarch64_object_olusur();
    test_t11_bare_x86_baslat_sembol();
    test_t12_bare_x86_libc_yok();

    printf("\n--- T13-T15: Linker script + final ELF ---\n");
    test_t13_bare_x86_linker_script();
    test_t14_bare_x86_entry_0x100000();
    test_t15_bare_aarch64_linker_script();

    printf("\n--- T16-T17: CLI hata path'i ---\n");
    test_t16_bilinmeyen_hedef();
    test_t17_hedef_arg_yok();

    printf("\n=========================================\n");
    printf("Toplam: %d | Basarili: %d | Basarisiz: %d\n",
           toplam_test, basarili, basarisiz);
    if (basarisiz == 0) {
        printf("=== %d/%d test gecti (basarili) ===\n",
               basarili, toplam_test);
        printf("\nNOT: QEMU boot testi manuel — Linux/macOS ortaminda:\n");
        printf("  qemu-system-x86_64 -kernel build/test_bm_temp.elf -nographic\n");
        printf("  qemu-system-aarch64 -M virt -cpu cortex-a57 -kernel ... -nographic\n");
    }
    return basarisiz > 0 ? 1 : 0;
}
