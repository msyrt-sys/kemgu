#ifndef KEMGU_LLVM_H
#define KEMGU_LLVM_H

#include "ast.h"

#include <stdio.h>

/*
 * KEMGU LLVM IR Backend (Minimum — Text-based, libLLVM yok)
 * ===========================================================
 *
 * Bu modul AST'yi LLVM IR text formatinda yazar. Cikti `clang -x ir -`
 * veya `llc` ile native koda derlenebilir.
 *
 * Su anki kapsam (ADIM 13.1 minimum):
 *   - Basit islev tanimlari (parametresiz)
 *   - tam32 donus tipi
 *   - Tam sayi literal'leri
 *   - Basit ikili op (+, -, *, /, %)
 *   - 'ver' deyimi
 *
 * Genisletilecek (gelecek): tip cesitliligi, parametreler, kontrol akisi,
 * yapi/dizi, cagri, vs.
 *
 * Kullanim:
 *   llvm_ir_uret(program, stdout);
 *
 * Sonra:
 *   ./build/kemgu --llvm program.kem | clang -x ir - -o program.exe
 */

/* Hedef platformlar (Faz 5: bare-metal target).
 *
 * LLVM triple ve runtime declare'lerini etkiler:
 *   HEDEF_HOST              -> x86_64-pc-windows-gnu, kdl_runtime declare'leri
 *                              + main entry point
 *   HEDEF_BARE_METAL_X86_64 -> x86_64-unknown-none, kdl declare YOK,
 *                              entry _baslat, no libc
 *   HEDEF_BARE_METAL_AARCH64-> aarch64-unknown-none, kdl declare YOK,
 *                              entry _baslat, no libc
 */
typedef enum {
    HEDEF_HOST = 0,
    HEDEF_BARE_METAL_X86_64,
    HEDEF_BARE_METAL_AARCH64,
} LlvmHedef;

/* AST'den LLVM IR text uret. NULL guvenli.
 * Eski API geriye uyumlu (HEDEF_HOST varsayilan). */
void llvm_ir_uret(const Dugum *program, FILE *out);
void llvm_ir_uret_hedef(const Dugum *program, FILE *out, LlvmHedef hedef);

#endif /* KEMGU_LLVM_H */
