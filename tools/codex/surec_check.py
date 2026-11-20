#!/usr/bin/env python3
"""Süreç parçalarını muaf tutmadan gerçek birleşik kaynağıyla denetler."""
import re
import subprocess
import sys
import tempfile
from pathlib import Path

compiler, source = sys.argv[1:]
if source.endswith("vfs_ortak.kem"):
    harness = "test/bm_x86_kabuk_harness.sh"
    program = "test/ornekler/kem_os_x86_kabuk.kem"
else:
    harness = "test/bm_x86_istisna_harness.sh"
    program = source
parts = []
for line in Path(harness).read_text(encoding="utf-8").splitlines():
    match = re.fullmatch(r'RT="([^"\n]+)"', line)
    if match:
        names = match[1].split()
        parts = parts + names[1:] if names[0] == "$RT" else names
if not parts:
    raise SystemExit("Birleşik kaynak listesi bulunamadı")
with tempfile.TemporaryDirectory(prefix="surec-check-", dir="build") as tmp:
    combined = Path(tmp) / "birlesik.kem"
    combined.write_bytes(b"\n".join(Path(p).read_bytes() for p in [*parts, program]))
    raise SystemExit(subprocess.run([compiler, "--check", "--mimari", "x86_64-none", combined]).returncode)
