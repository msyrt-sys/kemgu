#!/usr/bin/env bash
# Sınıf B lambda/closure codegen V2 (D-071) — E2E doğrulama.
# 4 örnek (D-071) + 1 (V2-F1, D-097): derle (--llvm) + link + çalıştır → beklenen
# exit. ASan ile UB/leak yok.
set -u
# [D-469] EXE uzantisi: Makefile `export EXE` ile gelir. Dogrudan cagrimda
# (make'siz) TANIMSIZ olurdu ve `set -u` altinda harness COKERDI -> ikilinin
# varligindan TESPIT et. Windows: .exe, Linux/macOS: bos.
: "${EXE=$(test -x build/kemgu.exe && echo .exe)}"
KEMGU=${KEMGU:-build/kemgu${EXE}}; RT=${RT:-build/kdl_runtime.o}
# [D-562] GECICI DIZIN DEPO-GORELI. `/tmp` KULLANILAMAZ: Windows'ta
# recipe kabugu (Git-for-Windows sh) ile MSYS2 araclari (diff, cmp)
# AYRI `/tmp` baglamalari cozer -> ayni dizgi iki farkli gercek dizine
# isaret eder ve dosya 'yok' gorunur. D-561'de olculdu: `[ -f ]` VAR
# derken `diff` 'No such file' diyordu ve bu 'STDOUT farkli' diye
# YANLIS ATFEDILIYORDU. build/ zaten .gitignore'da.
TMP=$(mktemp -d "build/lambdav2.XXXXXX" 2>/dev/null || echo "build/lambdav2.$$")
mkdir -p "$TMP"
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
# V2-F1 (D-097): yakalayan closure işlev-param'a geçer + doğru env!=null dispatch.
calistir cloparam43  test/snapshots/43_closure_param.kem 42
echo "=== lambda V2: $pass/$((pass+fail)) ==="
[ "$fail" -eq 0 ]
