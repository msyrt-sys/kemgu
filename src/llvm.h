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

/* AST'den LLVM IR text uret. NULL guvenli. */
void llvm_ir_uret(const Dugum *program, FILE *out);

/* Hedef triple ozelligi ile (orn. "x86_64-unknown-none", "aarch64-unknown-linux-gnu").
 * NULL ise varsayilan "x86_64-pc-windows-gnu" kullanilir. */
void llvm_ir_uret_target(const Dugum *program, FILE *out, const char *triple);

#endif /* KEMGU_LLVM_H */
