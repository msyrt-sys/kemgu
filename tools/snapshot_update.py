#!/usr/bin/env python3
"""
KEMGU Snapshot Baseline Batch Guncelleme

test/snapshots/ altindaki tum .kem dosyalari icin ./build/kemgu --parse
calistirir, ciktiyi .ast baseline dosyasina yazar.

ONEMLI: Bu intentional bir AST sozdizim degisikligi sonrasi
calistirilmali. Yanlislikla calistirirsanız snapshot regression
test'i bozulur. Once 'git diff test/snapshots/*.ast' ile farklari
inceleyin.

Kullanim:
    python tools/snapshot_update.py                    # tum snapshot'lar
    python tools/snapshot_update.py 21_modul_kullan    # tek snapshot
    python tools/snapshot_update.py --dry-run          # diff'leri goster
    python tools/snapshot_update.py --diff             # mevcut farklari raporla (yazmadan)
"""

import os
import sys
import subprocess
import argparse


SNAPSHOT_DIR = "test/snapshots"
KEMGU_BIN = "./build/kemgu.exe" if os.name == "nt" else "./build/kemgu"


def baseline_uret(kem_dosya: str) -> str:
    """kemgu --parse calistirir, stdout doner."""
    sonuc = subprocess.run(
        [KEMGU_BIN, "--parse", kem_dosya],
        capture_output=True, timeout=30
    )
    return sonuc.stdout.decode("utf-8", errors="replace") + \
           sonuc.stderr.decode("utf-8", errors="replace")


def main():
    p = argparse.ArgumentParser(description="Snapshot baseline batch guncelleme")
    p.add_argument("snapshot", nargs="?", default=None,
                   help="Tek snapshot adi (orn. 21_modul_kullan). Yoksa hepsi.")
    p.add_argument("--dry-run", action="store_true",
                   help="Yazmadan farki goster")
    p.add_argument("--diff", action="store_true",
                   help="Sadece mevcut diff'leri raporla (yazmadan)")
    args = p.parse_args()

    if not os.path.exists(KEMGU_BIN):
        print(f"HATA: {KEMGU_BIN} bulunamadi. Once 'make' calistirin.")
        sys.exit(1)

    if not os.path.isdir(SNAPSHOT_DIR):
        print(f"HATA: {SNAPSHOT_DIR} bulunamadi.")
        sys.exit(1)

    if args.snapshot:
        kem_dosyalar = [os.path.join(SNAPSHOT_DIR, args.snapshot + ".kem")]
    else:
        kem_dosyalar = sorted(
            os.path.join(SNAPSHOT_DIR, f)
            for f in os.listdir(SNAPSHOT_DIR)
            if f.endswith(".kem")
        )

    print(f"KEMGU Snapshot Update — {len(kem_dosyalar)} dosya")
    print("=" * 40)

    fark_var = 0
    guncellendi = 0
    for kem in kem_dosyalar:
        if not os.path.exists(kem):
            print(f"  ATLA: {kem} yok")
            continue
        ast_yol = kem[:-4] + ".ast"
        yeni = baseline_uret(kem)

        mevcut = ""
        if os.path.exists(ast_yol):
            with open(ast_yol, "rb") as f:
                mevcut = f.read().decode("utf-8", errors="replace")

        if mevcut == yeni:
            print(f"  TAM:  {os.path.basename(kem):<30} (degisim yok)")
            continue

        fark_var += 1
        if args.diff:
            print(f"  FARK: {os.path.basename(kem):<30} "
                  f"({len(mevcut)} -> {len(yeni)} byte)")
            continue
        if args.dry_run:
            print(f"  FARK: {os.path.basename(kem):<30} "
                  f"({len(mevcut)} -> {len(yeni)} byte) [--dry-run]")
            continue

        # Yaz
        with open(ast_yol, "w", encoding="utf-8") as f:
            f.write(yeni)
        guncellendi += 1
        print(f"  YAZ:  {os.path.basename(kem):<30} ({len(yeni)} byte)")

    print("=" * 40)
    print(f"Fark olan: {fark_var}, Yazilan: {guncellendi}")
    if args.diff or args.dry_run:
        print("[i] --dry-run / --diff modu: hicbir baseline yazilmadi.")


if __name__ == "__main__":
    main()
