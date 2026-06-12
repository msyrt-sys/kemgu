/*
 * KEMGU — C2 IR-verifier birim testleri (llvm_ir_dogrula)
 *
 * llvm_dogrula.c saf metin tarayicidir (AST/parser bagimliligi yok), bu
 * yuzden izole + ASan altinda test edilir. Gecerli IR kabul edilmeli;
 * terminator'suz blok iceren IR reddedilmeli — C2 kapisinin yakaladigi
 * C1-sinifi missing-terminator hatasi. "Kasitli bozuk codegen yolu kapi
 * tarafindan yakalanir" kabul kriteri burada deterministik dogrulanir.
 */

#include "llvm.h"

#include <stdio.h>
#include <string.h>

static int toplam = 0, basarili = 0, basarisiz = 0;

static void onayla(const char *ad, int kosul) {
    toplam++;
    if (kosul) {
        basarili++;
        printf("  [%d] %s ... \xe2\x9c\x93\n", toplam, ad);
    } else {
        basarisiz++;
        printf("  [%d] %s ... \xe2\x9c\x97\n", toplam, ad);
    }
}

int main(void) {
    char hata[256];

    printf("=== C2 IR-verifier (llvm_ir_dogrula) ===\n");

    /* 1) Gecerli: her blok terminator'lu (esles switch-with-fallout deseni). */
    const char *gecerli =
        "target triple = \"x86_64-pc-windows-gnu\"\n"
        "define i32 @s(i32 %x) {\n"
        "entry:\n"
        "  %0 = icmp eq i32 %x, 0\n"
        "  br i1 %0, label %bb1, label %bb2\n"
        "bb1:\n"
        "  ret i32 10\n"
        "bb2:\n"
        "  br label %bb0\n"
        "bb0:\n"
        "  ret i32 99\n"
        "}\n";
    onayla("gecerli IR kabul edilir (dogrula == 0)",
           llvm_ir_dogrula(gecerli, hata, sizeof(hata)) == 0);

    /* 2) Bozuk: bb1 terminator'suz, ardindan bb2 etiketi (C1-sinifi hata). */
    const char *bozuk_etiket =
        "define i32 @s(i32 %x) {\n"
        "entry:\n"
        "  br label %bb1\n"
        "bb1:\n"
        "  %0 = add i32 0, 10\n"   /* terminator yok! */
        "bb2:\n"
        "  ret i32 99\n"
        "}\n";
    int r2 = llvm_ir_dogrula(bozuk_etiket, hata, sizeof(hata));
    onayla("terminator'suz blok (sonraki etiket) reddedilir", r2 != 0);
    onayla("reddedilen blok icin hata mesaji uretilir", hata[0] != '\0');
    onayla("hata mesaji ihlal eden blogu ('bb1') belirtir",
           strstr(hata, "bb1") != NULL);

    /* 3) Bozuk: son blok terminator'suz, islev '}' ile kapaniyor. */
    const char *bozuk_kapanis =
        "define i32 @s() {\n"
        "entry:\n"
        "  %0 = add i32 0, 1\n"    /* terminator yok, '}' geliyor */
        "}\n";
    onayla("terminator'suz son blok (islev kapanisi) reddedilir",
           llvm_ir_dogrula(bozuk_kapanis, hata, sizeof(hata)) != 0);

    /* 4) Global/declare/type satirlari (islev disi) yok sayilir. */
    const char *globaller =
        "declare i32 @puts(ptr)\n"
        "@.str = private constant [2 x i8] c\"x\\00\"\n"
        "%T = type { i32, i32 }\n"
        "define void @v() {\n"
        "entry:\n"
        "  ret void\n"
        "}\n";
    onayla("global/declare/type satirlari yok sayilir (dogrula == 0)",
           llvm_ir_dogrula(globaller, hata, sizeof(hata)) == 0);

    /* 5) 'unreachable' bir terminatordur. */
    const char *unreach =
        "define void @u() {\n"
        "entry:\n"
        "  unreachable\n"
        "}\n";
    onayla("unreachable gecerli terminator sayilir",
           llvm_ir_dogrula(unreach, hata, sizeof(hata)) == 0);

    /* 6) Cok islevli modul: ilk islev gecerli, ikincide bozuk blok. */
    const char *cok_islev =
        "define i32 @a() {\n"
        "entry:\n"
        "  ret i32 1\n"
        "}\n"
        "define i32 @b() {\n"
        "entry:\n"
        "  %0 = add i32 0, 2\n"    /* terminator yok */
        "}\n";
    int r6 = llvm_ir_dogrula(cok_islev, hata, sizeof(hata));
    onayla("cok islevli modulde ikinci islevin bozuk blogu yakalanir",
           r6 != 0 && strstr(hata, "@b") != NULL);

    /* 7) NULL ve bos girdi guvenli. */
    onayla("NULL girdi guvenli (dogrula == 0)",
           llvm_ir_dogrula(NULL, hata, sizeof(hata)) == 0);
    onayla("bos girdi guvenli (dogrula == 0)",
           llvm_ir_dogrula("", hata, sizeof(hata)) == 0);

    printf("\n=== %d/%d test gecti ===\n", basarili, toplam);
    return basarisiz > 0 ? 1 : 0;
}
