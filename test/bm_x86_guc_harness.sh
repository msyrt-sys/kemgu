#!/usr/bin/env bash
# Her mekanizma C ve self ikilisiyle ayrı, -no-reboot QEMU açılışında.
set -euo pipefail
cd "$(dirname "$0")/.."
if [ ! -f /usr/share/OVMF/OVMF_CODE_4M.fd ]; then
 echo "ATLANDI (OVMF yok)"; exit 0
fi
if ! command -v qemu-system-x86_64 >/dev/null 2>&1; then
 echo "ATLANDI (QEMU yok)"; exit 0
fi
for derleyici in build/kemgu build/codegen; do
 for kip in yenidenbaslat kapat 8042 uclu; do
  kaynak=test/ornekler/kem_os_x86_kabuk.kem
  kabuk=1;dogrudan=0
  if [ "$kip" = 8042 ] || [ "$kip" = uclu ]; then
   kaynak=build/kem_os_x86_guc_case.kem;kabuk=0;dogrudan=1
   GUC_KIP="$kip" python3 - <<'PY'
import os
from pathlib import Path
s=Path('test/ornekler/kem_os_x86_guc.kem').read_text()
if os.environ['GUC_KIP']=='uclu':s=s.replace('guc_yeniden_asama(1);','guc_yeniden_asama(2);')
Path('build/kem_os_x86_guc_case.kem').write_text(s)
PY
  fi
  UEFI_YALNIZ_DERLE=1 UEFI_DERLEYICI="$derleyici" UEFI_KAYNAK="$kaynak" bash test/bm_x86_uefi_harness.sh
  UEFI_GUC="$kip" UEFI_GUC_DOGRUDAN="$dogrudan" UEFI_KABUK="$kabuk" UEFI_PS2_IRQ=1 python3 - <<'PY'
import sys
sys.path.insert(0,'tests/qemu');sys.argv=['bm_uefi.py','build/_bm_uefi']
import bm_uefi
bm_uefi.boot()
PY
 done
done
