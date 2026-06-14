#!/usr/bin/env bash
# Sınıf B lambda/closure codegen V2 (D-071) — E2E doğrulama.
# 4 örnek: derle (--llvm) + link + çalıştır → beklenen exit. ASan ile UB/leak yok.
set -u
KEMGU=${KEMGU:-build/kemgu.exe}; RT=${RT:-build/kdl_runtime.o}
TMP=$(mktemp -d 2>/dev/null || echo /tmp/lambdav2); mkdir -p "$TMP"
pass=0; fail=0
calistir() {  # ad dosya beklenen_rc
    local ad="$1" dosya="$2" brc="$3"
    if ! "$KEMGU" --llvm "$dosya" > "$TMP/$ad.ll" 2>"$TMP/$ad.ce"; then echo "  🔴 $ad: --llvm hata"; fail=$((fail+1)); return; fi
    if ! clang -x ir "$TMP/$ad.ll" -x none "$RT" -o "$TMP/$ad.exe" 2>"$TMP/$ad.le"; then echo "  🔴 $ad: link"; head -3 "$TMP/$ad.le"; fail=$((fail+1)); return; fi
    "$TMP/$ad.exe" >/dev/null 2>&1; local rc=$?
    if [ "$rc" -eq "$brc" ]; then echo "  ✅ $ad: rc=$rc"; pass=$((pass+1)); else echo "  🔴 $ad: rc=$rc (beklenen $brc)"; fail=$((fail+1)); fi
}
echo "=== Lambda V2 (D-071) ==="
calistir lambda10    test/snapshots/10_lambda.kem        42
calistir islev04     test/ornekler/04_islev.kem          42
calistir hesap42     test/snapshots/42_lambda_hesap.kem  42
calistir capture25   test/snapshots/25_closure_capture.kem 42
echo "=== lambda V2: $pass/$((pass+fail)) ==="
[ "$fail" -eq 0 ]
