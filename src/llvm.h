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
#define KEMGU_HEDEF_MIMARI "x86_64"
#define KEMGU_HEDEF_TRIPLE "x86_64-pc-windows-gnu"

/* D-269 (P1): satıriçi_asm arch-gate + emit edilen triple ÇALIŞMA-ZAMANI seçilebilir.
 * Varsayılan = yukarıdaki makrolar (x86_64) → bayrak verilmezse davranış BİREBİR eskisi
 * (fixpoint/regresyon güvenli). `--mimari arm64` verilince asm 'mimari:' etiketi 'arm64'
 * ile karşılaştırılır + triple aarch64 emit edilir → aarch64 sysreg/bariyer asm .kem'de
 * açılır (kem_os subsystem göçü önkoşulu). MİNİMAL: yalnız asm-gate + triple; başka target-
 * awareness YOK (KEMGU IR zaten target-agnostik; triple clang --target ile override edilir). */
/* D-346: sabitsüre CT bariyerinin GÜCÜ (yalnız arm64'te anlamlı).
 *   "csdb" (VARSAYILAN) — HINT uzayında; FEAT'siz çekirdekte mimari olarak NOP,
 *                         yani HER aarch64 çekirdeğinde güvenle çalışır.
 *   "sb"               — FEAT_SB (ARMv8.5+); daha güçlü spekülasyon bariyeri.
 *                         ⚠ FEAT_SB'SİZ çekirdekte encoding (0xd50330ff)
 *                         `msr S0_3_C3_C0_7, xzr` olarak çözülür — NOP DEĞİL,
 *                         tahsis edilmemiş sistem yazması. Bu yüzden opt-in;
 *                         yalnız hedefin ARMv8.5+ olduğu BİLİNİYORSA kullan
 *                         (ör. DGX Spark / Grace = Neoverse V2). Ölçüm ve
 *                         gerekçe: DECISIONS_LOG D-346. */
void llvm_ct_bariyer_ayarla(const char *tur);
const char *llvm_ct_bariyer(void);

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
