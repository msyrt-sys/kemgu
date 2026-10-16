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

/* C5/C8: hedef mimari + triple — TEK KAYNAK, su an sabit-kodlu.
 * Hedefe-duyarli triple secimi C8'in isi (bu PR'da YAPILMAZ).
 * satirici_asm 'mimari:' etiketi KEMGU_HEDEF_MIMARI ile karsilastirilir;
 * uyusmazsa AS001 derleme hatasi (yanlis hedefe sessizce bozuk IR
 * uretmek YASAK). Sonuc: arm64-tagli asm bu triple altinda reddedilir,
 * x86_64 asm calisir — kasitli. */
/* [D-469] VARSAYILAN TRIPLE ARTIK DERLEME PLATFORMUNDAN GELIR.
 * Oncesinde `x86_64-pc-windows-gnu` SABITTI: Linux'ta (DGX Spark dahil)
 * uretilen her IR YANLIS triple tasirdi. clang cogu durumda yine derler ama
 * bu SESSIZ bir yanlislik -- ve ARM64 makinede x86 triple'i ciddi sapmalar
 * dogurur (veri modeli, cagri sozlesmesi, hedef ozellikleri).
 * Makefile PLATFORM/ARCH'i ZATEN tespit ediyordu; eksik olan yalniz bu
 * makronun ondan BESLENMESIYDI. `-D` ile ezilebilir. */
#ifndef KEMGU_HEDEF_MIMARI
#  if defined(__aarch64__) || defined(_M_ARM64)
#    define KEMGU_HEDEF_MIMARI "arm64"
#  else
#    define KEMGU_HEDEF_MIMARI "x86_64"
#  endif
#endif

#ifndef KEMGU_HEDEF_TRIPLE
#  if defined(_WIN32)
#    if defined(_M_ARM64)
#      define KEMGU_HEDEF_TRIPLE "aarch64-pc-windows-gnu"
#    else
#      define KEMGU_HEDEF_TRIPLE "x86_64-pc-windows-gnu"
#    endif
#  elif defined(__APPLE__)
#    if defined(__aarch64__)
#      define KEMGU_HEDEF_TRIPLE "arm64-apple-darwin"
#    else
#      define KEMGU_HEDEF_TRIPLE "x86_64-apple-darwin"
#    endif
#  else
#    if defined(__aarch64__)
#      define KEMGU_HEDEF_TRIPLE "aarch64-unknown-linux-gnu"
#    else
#      define KEMGU_HEDEF_TRIPLE "x86_64-pc-linux-gnu"
#    endif
#  endif
#endif

/* D-269 (P1): satıriçi_asm arch-gate + emit edilen triple ÇALIŞMA-ZAMANI seçilebilir.
 * Varsayılan = yukarıdaki makrolar (x86_64) → bayrak verilmezse davranış BİREBİR eskisi
 * (fixpoint/regresyon güvenli). `--mimari arm64` verilince asm 'mimari:' etiketi 'arm64'
 * ile karşılaştırılır + triple aarch64 emit edilir → aarch64 sysreg/bariyer asm .kem'de
 * açılır (kem_os subsystem göçü önkoşulu). MİNİMAL: yalnız asm-gate + triple; başka target-
 * awareness YOK (KEMGU IR zaten target-agnostik; triple clang --target ile override edilir). */
void llvm_hedef_ayarla(const char *mimari, const char *triple);
const char *llvm_hedef_mimari(void);   /* geçerli asm arch-tag (AS001 karşılaştırması) */
const char *llvm_hedef_triple(void);   /* geçerli emit triple */

/* AST'den LLVM IR text uret. NULL guvenli.
 * Donus: olumcul codegen hatasi sayisi (AS001 mimari uyusmazligi).
 * >0 ise cikti IR'i KULLANILMAMALI (hatali asm bloklari emit edilmedi);
 * cagiran derlemeyi hata koduyla bitirmeli. */
int llvm_ir_uret(const Dugum *program, FILE *out);

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
