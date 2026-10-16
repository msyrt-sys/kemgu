#!/usr/bin/env bash
# [D-666] GÖREV OLUŞTURUCULARI — BAŞLATILMAMIŞ ALAN KAPISI (MemorySanitizer)
# Depodaki İLK MSan kapısı. ASan başlatılmamış okumayı GÖRMEZ.
# Yetenek ÖLÇÜLÜR (D-551/D-661): MSan yalnız Linux+clang; yoksa AÇIKÇA atlanır.
# Sabotajla ölçüldü (S169: calloc -> malloc): use-of-uninitialized-value
# @ kdl_runtime.c (kdl_gorev_birlestir), rc != 0.
set -u
: "${EXE=$(test -x build/kemgu.exe && echo .exe)}"
TMP=$(mktemp -d "build/gorev_msan.XXXXXX" 2>/dev/null || echo "build/gorev_msan.$$")
mkdir -p "$TMP"; trap 'rm -rf "$TMP"' EXIT
msan_var=0
if [ -z "$EXE" ] && command -v clang >/dev/null 2>&1 \
   && printf 'int main(void){return 0;}\n' > "$TMP/p.c" \
   && clang -fsanitize=memory -o "$TMP/p" "$TMP/p.c" 2>/dev/null && "$TMP/p" >/dev/null 2>&1; then
    msan_var=1
fi
if [ "$msan_var" -eq 0 ]; then
    echo "  ⚠ MemorySanitizer YOK bu ortamda (Windows/MinGW ya da clang desteksiz) -> kapi ATLANDI. Bkz. D-666."
    echo "=== gorev baslatma (MSan): ATLANDI (MSan YOK) ==="
    exit 0
fi
if ! clang -std=c11 -g -O1 -fsanitize=memory -fsanitize-memory-track-origins -Iruntime \
       -o "$TMP/t" test/test_gorev_baslatma.c runtime/kdl_runtime.c -lpthread -lm 2>"$TMP/e"; then
    echo "  🔴 MSan derlemesi BASARISIZ:"; head -3 "$TMP/e" | sed 's/^/       /'; exit 1
fi
timeout 120 "$TMP/t" > "$TMP/o" 2>&1; rc=$?
n=$(grep -c "WARNING: MemorySanitizer" "$TMP/o")
if [ "$rc" -ne 0 ] || [ "$n" -ne 0 ]; then
    echo "  🔴 MemorySanitizer: rc=$rc, $n uyari"
    grep -m1 -A2 "WARNING: MemorySanitizer" "$TMP/o" | sed 's/^/       /'
    echo "=== gorev baslatma (MSan): BASARISIZ ==="; exit 1
fi
echo "  ✅ MemorySanitizer: 0 baslatilmamis okuma (rc=0)"
echo "=== gorev baslatma (MSan): GECTI ==="
