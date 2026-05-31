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

/*
 * C2: IR-verifier kapisi (text backend icin LLVMVerifyModule esdegeri).
 *
 * libLLVM linklenmedigi icin (text uretici) LLVMVerifyModule cagrilamaz;
 * bunun yerine emit edilen IR metnini tarayarak her `define`'in her temel
 * blogunun gecerli bir terminator (ret/br/switch/unreachable/...) ile
 * bittigini dogrular;  LangRef "her basic block bir terminator ile biter"
 * degismezini uygular. Bu, C1 sinifi missing-terminator regresyonlarini
 * (or. esles kolunun bir sonraki bloga dusmesi) opt'a/clang'a varmadan
 * yakalar.
 *
 * Donus: 0 = gecerli; !=0 = ihlal bulundu. `hata` doluysa ilk ihlalin
 * aciklamasi yazilir (en fazla hata_boyut-1 bayt + NUL).
 */
int llvm_ir_dogrula(const char *ir_metni, char *hata, size_t hata_boyut);

#endif /* KEMGU_LLVM_H */
