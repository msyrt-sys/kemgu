#ifndef KEMGU_LLVM_H
#define KEMGU_LLVM_H

#include "ast.h"

#include <stdio.h>

/*
 * KEMGU LLVM IR Backend (Minimum -- Text-based, libLLVM yok)
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

/* AST'den LLVM IR text uret. NULL guvenli.
 * Default triple: "x86_64-pc-windows-gnu". */
void llvm_ir_uret(const Dugum *program, FILE *out);

/* AST'den LLVM IR text uret, hedef triple parametreli (Bare-metal Faz).
 *
 * hedef_triple NULL ise default "x86_64-pc-windows-gnu" kullanilir.
 * Substring "-none-" veya "-unknown-none" varsa libc declare'leri
 * emit EDILMEZ; yalniz programcinin tanimladigi sembolleri uretir.
 *
 * Kullanim:
 *   llvm_ir_uret_hedef(prog, stdout, "aarch64-unknown-none");  // bare-metal
 *   llvm_ir_uret_hedef(prog, stdout, "x86_64-unknown-linux-gnu");
 *   llvm_ir_uret_hedef(prog, stdout, NULL);  // default = host
 */
void llvm_ir_uret_hedef(const Dugum *program, FILE *out,
                        const char *hedef_triple);

/* Yardimci: triple bare-metal pattern uyuyor mu?
 * NULL veya bos => 0 (host). */
int llvm_hedef_bare_metal_mi(const char *hedef_triple);

#endif /* KEMGU_LLVM_H */
