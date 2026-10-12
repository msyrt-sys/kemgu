#!/usr/bin/env bash
# ============================================================================
# gate_linux.sh — x86 parite döngüsünün HAKEMİ (DGX Spark / Linux).
# ----------------------------------------------------------------------------
# gate.sh'in Linux karşılığı. gate.sh Windows'a bağlıydı (/c/tmp, /c/msys64 —
# Spark'ta YOK, ölçüldü) ve FIXPOINT'i eskimiş bir satır sayısıyla (33371;
# bugün 85312) anıyordu. Bu kapı:
#   0. TAZE KLON: HEAD commit'i boş bir dizine `git clone` eder (build/ yok,
#      takip edilmeyen dosya yok) — "bende çalışıyor" artefaktlarını eler.
#   1. `make` (derleyici + runtime).
#   2. aarch64 REGRESYON: `make calistir_kem_os_arm` exit 0 (hedef tüm fazları
#      kendisi denetler) + gerçek seri çıktıda [1..10] işaretleri.
#   3. x86 İŞLEVSEL KANIT: `make <x86-hedef>` exit 0 + verilen işaretler o
#      hedefin GERÇEK çıktısında (hedef kendi objdump/QEMU denetimini yapar).
#   4. `make test_tumu` exit 0 + "stage1 IR == stage2 IR" (FIXPOINT birebir;
#      satır sayısı bilgi olarak raporlanır, sabit sayıya bağlanmaz).
# HİÇ model çağırmaz.
#
# Kullanım: gate_linux.sh <id> <x86-make-hedefi|-> [x86-işareti ...]
# exit 0 = hepsi yeşil; exit 1 = ilk patlayan assert yazılır.
# ============================================================================
set -uo pipefail
ID="${1:?görev-id gerekli}"; X86_HEDEF="${2:?x86 hedefi gerekli (yoksa -)}"; shift 2
X86_ISARET=("$@")
FAIL(){ echo "GATE-FAIL[$ID]: $1"; exit 1; }

# HERMETİK: Ubuntu kabuk ortamı DEBUGINFOD_URLS=https://debuginfod.ubuntu.com
# taşıyor. ASan/LSan rapor sembolleştirirken llvm-symbolizer bu sunucuya AĞDAN
# gider; erişim asılınca ASan ikilisi `timeout 60`ı aşar → kapı SAHTE kırmızı
# (ölçüldü 2026-10-12: calistir_kanal_omru 3/3 rc=124; değişken boşken anında
# biter, kod aynı). Kapının sonucu dış bir sunucuya bağlı olamaz.
export DEBUGINFOD_URLS=

KAYNAK="$(git rev-parse --show-toplevel)" || FAIL "git deposu değil"
SHA="$(git -C "$KAYNAK" rev-parse HEAD)"
KOK="${GATE_KOK:-/tmp/claude-1000}"; mkdir -p "$KOK"
TMP="$(mktemp -d "$KOK/gate_${ID}_XXXX")" || FAIL "mktemp"
LOG="$TMP/log"; mkdir -p "$LOG"
echo "[gate:$ID] commit $SHA → taze klon $TMP/kemgu"
git clone -q --no-hardlinks "$KAYNAK" "$TMP/kemgu" || FAIL "clone"
cd "$TMP/kemgu" && git checkout -q --detach "$SHA" || FAIL "checkout $SHA"
[ -z "$(git status --porcelain)" ] || FAIL "klon temiz değil"

T0=$(date +%s)
echo "[gate:$ID] 1/4 make"
make -j"$(nproc)" > "$LOG/make.log" 2>&1 || FAIL "make exit $? ($LOG/make.log)"

echo "[gate:$ID] 2/4 aarch64 kem_os (QEMU)"
make calistir_kem_os_arm > "$LOG/kem_os_arm.log" 2>&1 \
    || FAIL "calistir_kem_os_arm exit $? ($LOG/kem_os_arm.log)"
[ -s build/kem_os.out ] || FAIL "aarch64 QEMU seri çıktısı yok"
for m in "[1] BOOT OK" "[2] HEAP DIZI OK" "[3] MMIO OK" "[4] HESAP OK" "[5] EXC OK" \
         "DISK RW OK" "FS RW OK" "NET DEV OK" "NET ARP OK" "PING CANLI" "KEMGU KEM-OS OK"; do
    grep -qF "$m" build/kem_os.out || FAIL "aarch64 işaret eksik: '$m' (regresyon)"
done
grep -qxF "KEMGU KEM-OS OK" build/kem_os.out || FAIL "'KEMGU KEM-OS OK' satırı bozuk (garbling)"

if [ "$X86_HEDEF" != "-" ]; then
    echo "[gate:$ID] 3/4 x86 işlevsel kanıt: make $X86_HEDEF"
    make "$X86_HEDEF" > "$LOG/x86.log" 2>&1 || FAIL "$X86_HEDEF exit $? ($LOG/x86.log)"
    for m in "${X86_ISARET[@]}"; do
        grep -qF -- "$m" "$LOG/x86.log" || FAIL "x86 kanıt dizgisi yok: '$m' ($LOG/x86.log)"
    done
else
    echo "[gate:$ID] 3/4 x86 hedefi yok (atlandı — bilinçli, '-' verildi)"
fi

echo "[gate:$ID] 4/4 test_tumu (FIXPOINT dahil)"
make test_tumu > "$LOG/test_tumu.log" 2>&1; TT=$?
[ "$TT" -eq 0 ] || FAIL "test_tumu exit $TT ($LOG/test_tumu.log)"
FP="$(grep -E 'stage1 IR == stage2 IR' "$LOG/test_tumu.log" | head -1)"
[ -n "$FP" ] || FAIL "FIXPOINT satırı yok"
grep -q 'Tum testler gecti' "$LOG/test_tumu.log" || FAIL "'Tum testler gecti' yok"

echo "GATE-OK[$ID]: commit ${SHA:0:12} · taze klon · aarch64 [1..10] · x86 ($X86_HEDEF) · test_tumu · $(echo "$FP" | sed 's/^ *//') · $(( $(date +%s) - T0 )) sn"
echo "GATE-LOG[$ID]: $LOG"
