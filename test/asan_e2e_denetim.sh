#!/usr/bin/env bash
# ============================================================================
# asan_e2e_denetim.sh — KEMGU codegen bellek güvenliği denetimi (ASan + UBSan)
# ----------------------------------------------------------------------------
# ÜRETİLEN KODU AddressSanitizer + UndefinedBehaviorSanitizer ile derleyip
# çalıştırır. test_llvm E2E zinciri (kemgu --llvm | clang | run) üretilen
# programı SANITIZER'SIZ koşturur — codegen bellek hataları (heap-overflow,
# misaligned-access, garbage func-ptr) o yüzden gözden kaçıyordu. Bu betik
# D-030 (dizi_olustur element_byte heap-overflow) sınıfı hataları yakalar.
#
# Kullanım:  bash test/asan_e2e_denetim.sh
#   PATH'te clang (Clang64) + kemgu derlenmiş (build/kemgu.exe) olmalı.
#   build/kdl_runtime.o gerekmez — runtime kaynağı ASan ile birlikte derlenir.
#
# Çıkış kodu: 0 = tüm çalışabilir örnekler ASan-temiz; 1 = en az bir ihlal.
# ============================================================================
set -u
KEMGU=${KEMGU:-./build/kemgu.exe}
RT="runtime/kdl_runtime.c runtime/kdl_runtime_mmio.c"
TMP=$(mktemp -d 2>/dev/null || echo /tmp/asan_denetim)
mkdir -p "$TMP"

# Bilinen başarısızlıklar (kök-neden + takip). Bunlar ASan-temiz DEĞİL ama
# nedeni belgeli — denetimi kızartmasınlar diye dışlanır (bkz. DECISIONS_LOG D-031).
#   Lambda/closure: D-004 ile V2'ye ERTELENDİ (fonksiyon-değer codegen yok).
#   Sınıf A KISMEN kapandı (D-070): LİTERAL-arg `f([..])` → heap (03_kontrol PASS,
#     allowlist'ten çıktı; 36_quicksort_stub silinmiş). Kalan: stack-array DEĞİŞKENİ
#     `değişken xs=[..]; f(xs)` → Dizi<T> param (35/40) = stack↔KdlDizi temsil
#     uyumsuzluğu, DUR-SOR (Mehmet'te — whole-program flow veya temsil kararı).
ALLOWLIST="04_islev 10_lambda 25_closure_capture 42_lambda_hesap \
           35_binary_search 40_dizi_islemler"

pass=0; fail=0; skip=0; allow=0
for f in test/ornekler/*.kem test/snapshots/*.kem; do
    base=$(basename "$f" .kem)
    case " $ALLOWLIST " in *" $base "*) allow=$((allow+1)); continue;; esac
    grep -qE "i\xc5\x9flev main|işlev main" "$f" 2>/dev/null || { skip=$((skip+1)); continue; }
    "$KEMGU" --check "$f" >/dev/null 2>&1 || { skip=$((skip+1)); continue; }
    "$KEMGU" --llvm "$f" > "$TMP/a.ll" 2>/dev/null || { skip=$((skip+1)); continue; }
    if ! clang -fsanitize=address,undefined -x ir "$TMP/a.ll" -x none $RT \
              -o "$TMP/a.exe" 2>/dev/null; then
        skip=$((skip+1)); continue
    fi
    out=$("$TMP/a.exe" 2>&1)
    if echo "$out" | grep -qiE "AddressSanitizer|runtime error|SUMMARY:.*[Ss]anitizer"; then
        echo "  🔴 ASAN/UBSAN: $base"
        echo "$out" | grep -iE "ERROR|overflow|misalign|use-after|SUMMARY" | head -2
        fail=$((fail+1))
    else
        pass=$((pass+1))
    fi
done
echo "=== ASan E2E denetimi: PASS=$pass  FAIL=$fail  SKIP=$skip  ALLOW(bilinen)=$allow ==="
[ "$fail" -eq 0 ]
