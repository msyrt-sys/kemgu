#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
# [D-768] Ölçüm `/usr/bin/time -v` ister; yoksa (Windows CI) BİLDİREREK atla
# (D-566 perf_bellek emsali — sessiz atlama DEĞİL).
# Yetenek, aracı ÇALIŞTIRACAK süreçten ölçülür: Windows CI'da bash (MSYS) /usr/bin/time'ı
# görür ama ölçümü yapan yerel Windows Python'u o yolu açamaz (FileNotFoundError, ölçüldü).
if ! python3 -c 'import os,subprocess,sys; sys.exit(0 if os.access("/usr/bin/time", os.X_OK) and subprocess.run(["/usr/bin/time","-v","true"],capture_output=True).returncode==0 else 1)' 2>/dev/null; then
    echo "ATLANDI (/usr/bin/time ölçüm sürecinden çalıştırılamıyor — derleme ölçümü yapılmadı)"; exit 0
fi
w=$(mktemp -d build/derleme-olcum.XXXXXX)
UEFI_CALISMA_DIZINI="$w/uefi" UEFI_YALNIZ_DERLE=1 UEFI_KAYNAK=test/ornekler/kem_os_x86_kabuk.kem bash test/bm_x86_uefi_harness.sh
python3 test/olcum/derleme.py "$w/uefi/comb.kem" "$w" "$@"
echo "DERLEME OK $w/olcum.json"
