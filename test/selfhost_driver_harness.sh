#!/usr/bin/env bash
# ============================================================================
# selfhost_driver_harness.sh — AŞAMA 4 (driver) + AŞAMA 5 (self-host) kanıtı (D-086).
# ----------------------------------------------------------------------------
# Tek birleşik KEMGU binary'si (selfhost/codegen.kem = lexer+parser+checker+codegen
# + dispatch) DÖRT modunu C kemgu oracle'ına karşı doğrular — HEM C-derlenmiş HEM
# SELF-HOST-derlenmiş driver için (Aşama 5: derleyici kendini üretir + doğru çalışır):
#   --token  → C --token      (lexer dump; sıfır-diff)      korpus: test/lex_korpus
#   --parse  → C --ast        (düz AST dump; sıfır-diff)    korpus: test/parse_korpus
#   --check  → C --checkdump  (KOD\tsat\tsüt / OK; sıfır-diff) korpus: test/check_korpus
#   --llvm   → C --llvm       (exit-kod eşdeğerlik)         korpus: test/cg_korpus
#
# İki driver:
#   kemgu_self   = C-codegen(kemgu.exe --llvm) ile derlenen driver.
#   kemgu_self2  = kemgu_self'in codegen.kem'i derlemesiyle elde edilen driver
#                  (= driver kendini derler; Aşama 5 self-hosting). Her ikisi de 4 modu geçer.
#   Ayrıca kemgu_self2 --llvm IR == kemgu_self --llvm IR (codegen.kem üzerinde) = FIXPOINT.
#
# Kullanım: bash test/selfhost_driver_harness.sh  (veya make calistir_self_driver)
# ============================================================================
set -u
# [D-469] EXE uzantisi: Makefile `export EXE` ile gelir. Dogrudan cagrimda
# (make'siz) TANIMSIZ olurdu ve `set -u` altinda harness COKERDI -> ikilinin
# varligindan TESPIT et. Windows: .exe, Linux/macOS: bos.
: "${EXE=$(test -x build/kemgu.exe && echo .exe)}"
KEMGU=${KEMGU:-build/kemgu${EXE}}
RT=${RT:-build/kdl_runtime.o}
SELF=${SELF:-build/kemgu_self${EXE}}
SELF2=${SELF2:-build/kemgu_self2.exe}
SRC=${SRC:-selfhost/codegen.kem}
TMP=$(mktemp -d 2>/dev/null || echo /tmp/selfdrv); mkdir -p "$TMP"
genel_fail=0

link() {  # $1=ll $2=exe ; Win11 Defender exec yarışı → 3 deneme
    for _t in 1 2 3; do clang -x ir "$1" -x none "$RT" -o "$2" 2>/dev/null; [ -x "$2" ] && return 0; done
    return 1
}

# ---- 1) C-codegen ile driver'ı derle (kemgu_self) ----
if ! "$KEMGU" --llvm "$SRC" > "$TMP/self.ll" 2>/dev/null; then echo "🔴 driver --llvm IR üretemedi"; exit 1; fi
if ! link "$TMP/self.ll" "$SELF"; then echo "🔴 kemgu_self link edilemedi"; exit 1; fi

# ---- 2) SELF-HOST: driver kendini derler (kemgu_self2) ----
if ! "$SELF" --llvm "$SRC" > "$TMP/self2.ll" 2>/dev/null; then echo "🔴 kemgu_self driver'ı derleyemedi"; exit 1; fi
if ! link "$TMP/self2.ll" "$SELF2"; then echo "🔴 kemgu_self2 link edilemedi"; exit 1; fi

# ---- FIXPOINT: kemgu_self2 IR == kemgu_self IR (codegen.kem üzerinde) ----
"$SELF2" --llvm "$SRC" > "$TMP/self2b.ll" 2>/dev/null
if diff -q "$TMP/self2.ll" "$TMP/self2b.ll" >/dev/null 2>&1; then
    echo "  FIXPOINT: kemgu_self2 codegen.kem IR'ı kararlı ($(wc -l < "$TMP/self2.ll") satır) ✓"
else
    echo "  🔴 FIXPOINT: kemgu_self2 IR kararsız"; genel_fail=1
fi

# ---- Yardımcı: bir driver binary'sini 3 byte-diff modunda doğrula ----
# $1=driver  $2=etiket-öneki
byte_modlari() {
    local drv="$1" pre="$2" pass fail x mod oracle korpus
    for spec in "--token --token test/lex_korpus" "--parse --ast test/parse_korpus" "--check --checkdump test/check_korpus"; do
        set -- $spec; mod="$1"; oracle="$2"; korpus="$3"
        pass=0; fail=0
        for x in "$korpus"/*.kem; do
            [ -f "$x" ] || continue
            "$KEMGU" "$oracle" "$x" 2>/dev/null > "$TMP/o.txt"
            "$drv"   "$mod"    "$x" 2>/dev/null > "$TMP/a.txt"
            if diff -q "$TMP/o.txt" "$TMP/a.txt" >/dev/null 2>&1; then pass=$((pass+1));
            else echo "    🔴 [$pre $mod] $(basename "$x"):"; diff "$TMP/o.txt" "$TMP/a.txt" | head -4; fail=$((fail+1)); fi
        done
        printf "  %-10s %-8s vs C %-12s %d/%d\n" "$pre" "$mod" "$oracle" "$pass" "$((pass+fail))"
        [ "$fail" -eq 0 ] || genel_fail=1
    done
}

echo "=== AŞAMA 4/5 driver — C-derlenmiş + self-host-derlenmiş, 4 mod ==="
byte_modlari "$SELF"  "C-built"
byte_modlari "$SELF2" "self-host"

# ---- LLVM exit-kod eşdeğerlik (her iki driver; codegen_diff_harness delege) ----
for drv in "$SELF" "$SELF2"; do
    CODEGEN="$drv" KORPUS="test/cg_korpus" KEMGU="$KEMGU" RT="$RT" \
        bash test/codegen_diff_harness.sh > "$TMP/llvm.out" 2>&1
    rc=$?
    tail -1 "$TMP/llvm.out" | sed "s|^|  LLVM ($(basename "$drv")): |"
    [ "$rc" -eq 0 ] || genel_fail=1
done

echo "=== driver sonucu: $([ "$genel_fail" -eq 0 ] && echo 'TÜM MODLAR + SELF-HOST + FIXPOINT GEÇTİ ✓' || echo 'BAŞARISIZ 🔴') ==="
[ "$genel_fail" -eq 0 ]
