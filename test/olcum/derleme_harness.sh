#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
w=$(mktemp -d build/derleme-olcum.XXXXXX)
UEFI_CALISMA_DIZINI="$w/uefi" UEFI_YALNIZ_DERLE=1 UEFI_KAYNAK=test/ornekler/kem_os_x86_kabuk.kem bash test/bm_x86_uefi_harness.sh
python3 test/olcum/derleme.py "$w/uefi/comb.kem" "$w" "$@"
echo "DERLEME OK $w/olcum.json"
