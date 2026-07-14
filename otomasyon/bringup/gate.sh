#!/usr/bin/env bash
# ============================================================================
# gate.sh — KEMGU REAL-OS bring-up MODEL-FÜZ HAKEM (D-271 sonrası mekanik faz).
# ----------------------------------------------------------------------------
# Kullanım: bash otomasyon/bringup/gate.sh <görev-id> <beklenen-marker>
# HİÇ model çağırmaz. Gerçek build + gerçek QEMU + gerçek test_tumu ile yargılar.
# exit 0 = hepsi geçti; exit 1 = hangi assert patladıysa yazar.
#
# İŞ:
#  1. kem_os'u kur: `make calistir_kem_os_arm` — Makefile CAT eder (kem_os.kem + tüm
#     runtime/kem_*.kem sürücüler, --mimari arm64) + clang + ld. (Makefile'ın kendi QEMU
#     assert'i toleranslı; ELF her hâlükârda üretilir.)
#  2. BAĞIMSIZ QEMU boot — TÜM cihazlar: virtio-blk(+disk.img) + virtio-net(+netdev user)
#     (kemgu_os_arm.c kanıtlı-C flag'leri).
#  3. ASSERT: çekirdek [1..5]+KEM-OS OK HÂLÂ yeşil + B1'den bu göreve kadar TÜM markerlar +
#     bu görevin markerı ($2). Gerçek seri çıktı, garbling yok (marker satırı tam-eşleşme).
#  4. ASSERT: `make test_tumu` exit 0 + "stage1 IR == stage2 IR" (FIXPOINT).
# ============================================================================
set -uo pipefail
ID="${1:?görev-id gerekli}"; MARKER="${2:?marker gerekli}"
export PATH=/c/msys64/clang64/bin:/c/msys64/ucrt64/bin:$PATH
export TMPDIR=/c/tmp
REPO="$(git rev-parse --show-toplevel)"; cd "$REPO" || { echo "GATE-FAIL[$ID]: repo yok"; exit 1; }
FAIL(){ echo "GATE-FAIL[$ID]: $1"; exit 1; }

# Kümülatif marker sırası (TASKS.txt + B1). $2'nin konumuna kadar HEPSİ beklenir → önceki-yeşil.
ORDERED=("[6] DISK RW OK" "[7] FS RW OK" "[8] NET DEV OK" "[9] NET ARP OK" "[10] PING CANLI")
CORE=("[1] BOOT OK" "[2] HEAP DIZI OK" "[3] MMIO OK" "[4] HESAP OK" "[5] EXC OK" "KEMGU KEM-OS OK")

command -v qemu-system-aarch64 >/dev/null 2>&1 || FAIL "qemu-system-aarch64 yok (gate koşamaz)"
command -v mingw32-make >/dev/null 2>&1 && MAKE=mingw32-make || MAKE=make

# --- 1. Build (Makefile CAT + kemgu --mimari arm64 + clang + ld). QEMU-assert toleranslı. ---
echo "[gate:$ID] kem_os build (make calistir_kem_os_arm)..."
$MAKE calistir_kem_os_arm > /c/tmp/gate_build.log 2>&1 || true
[ -f build/kem_os.elf ] || FAIL "kem_os.elf üretilemedi (build hatası — /c/tmp/gate_build.log)"

# --- 2. Bağımsız QEMU boot: TÜM cihazlar (blk + net). ---
# NOT: QEMU Windows-binary → serial/disk yolları REPO-göreli (build/...) olmalı; MSYS
# mutlak yol (/c/tmp/...) Windows QEMU'da 'open failed' verir (Makefile de build/ kullanır).
echo "[gate:$ID] bağımsız QEMU boot (virtio-blk + virtio-net)..."
dd if=/dev/zero of=build/gate_disk.img bs=512 count=64 2>/dev/null
rm -f build/gate_kemos.out
timeout 15 qemu-system-aarch64 -M virt -cpu cortex-a72 -display none \
    -global virtio-mmio.force-legacy=false \
    -drive file=build/gate_disk.img,format=raw,if=none,id=d0 -device virtio-blk-device,drive=d0 \
    -netdev user,id=n0 -device virtio-net-device,netdev=n0 \
    -serial file:build/gate_kemos.out -kernel build/kem_os.elf 2>/dev/null || true
[ -f build/gate_kemos.out ] || FAIL "QEMU seri çıktı üretmedi"
OUT="$(cat build/gate_kemos.out)"
echo "--- QEMU seri çıktı ---"; echo "$OUT"; echo "--- son ---"

# --- 3. ASSERT: çekirdek + kümülatif markerlar (önceki-yeşil + bu görev). ---
for m in "${CORE[@]}"; do
    printf '%s\n' "$OUT" | grep -qF "$m" || FAIL "çekirdek marker eksik: '$m' (regresyon)"
done
# garbling: KEM-OS OK satırı TAM eşleşme (kontrol-karakter/bozulma yok).
printf '%s\n' "$OUT" | grep -qxF "KEMGU KEM-OS OK" || FAIL "'KEMGU KEM-OS OK' satırı bozuk/garbled"
# kümülatif: $MARKER'ın ORDERED'daki konumuna kadar hepsi beklenir.
hedef_idx=-1
for k in "${!ORDERED[@]}"; do [ "${ORDERED[$k]}" = "$MARKER" ] && hedef_idx=$k; done
if [ "$MARKER" != "-" ] && [ "$hedef_idx" -ge 0 ]; then
    for k in $(seq 0 "$hedef_idx"); do
        printf '%s\n' "$OUT" | grep -qF "${ORDERED[$k]}" || FAIL "marker eksik: '${ORDERED[$k]}' (önceki-yeşil/bu-görev regresyon)"
    done
elif [ "$MARKER" != "-" ]; then
    # ORDERED dışı marker (ör. self-test b1-check zaten [6]); yine de bu markerı bekle.
    printf '%s\n' "$OUT" | grep -qF "$MARKER" || FAIL "görev markerı yok: '$MARKER'"
fi
echo "[gate:$ID] markerlar YEŞİL (çekirdek + kümülatif → $MARKER)"

# --- 4. test_tumu + FIXPOINT (regresyon kapısı). ---
echo "[gate:$ID] test_tumu (FIXPOINT dahil)..."
$MAKE test_tumu > /c/tmp/gate_tt.log 2>&1; TT=$?
[ "$TT" -eq 0 ] || FAIL "test_tumu exit $TT (regresyon — /c/tmp/gate_tt.log)"
grep -qE 'stage1 IR == stage2 IR' /c/tmp/gate_tt.log || FAIL "FIXPOINT 'stage1 IR == stage2 IR' yok"

echo "GATE-OK[$ID]: çekirdek[1..5]+KEM-OS OK + kümülatif→$MARKER + test_tumu + FIXPOINT hepsi YEŞİL"
exit 0
