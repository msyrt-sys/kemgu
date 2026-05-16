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

/* LLVM IR uretim secenekleri. */
typedef struct {
    int baremetal;          /* 1 -> aarch64-unknown-none + main->_baslat alias */
    const char *triple;     /* NULL -> default; aksi halde target triple override */
} LlvmSecenek;

/* AST'den LLVM IR text uret. NULL guvenli. */
void llvm_ir_uret(const Dugum *program, FILE *out);

/* Secenek-aware varyant. KIRMIZI K8d: bare-metal hedef + _baslat alias.
 * NULL secenek default davranisa esit. */
void llvm_ir_uret_secenek(const Dugum *program, FILE *out,
                          const LlvmSecenek *secenek);

#endif /* KEMGU_LLVM_H */
