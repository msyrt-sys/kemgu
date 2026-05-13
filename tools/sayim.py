#!/usr/bin/env python3
"""
KEMGU Repo Metrikleri

Repo'daki kod, test, dokuman istatistiklerini sayar. README badge'leri
ve checkpoint raporlari icin makine-okunabilir cikti uretir.

Kullanim:
    python tools/sayim.py               # tablolu rapor
    python tools/sayim.py --json        # JSON cikti
    python tools/sayim.py --badge SVG_URL > badges/test-count.svg.txt
"""

import os
import sys
import subprocess
import argparse
import json


def satir_say(yol: str) -> int:
    """Bir dosyadaki satir sayisi."""
    try:
        with open(yol, "r", encoding="utf-8", errors="ignore") as f:
            return sum(1 for _ in f)
    except (IOError, OSError):
        return 0


def dizin_topla(dizin: str, uzantilar):
    """Dizin altindaki uzantilara uyan dosyalarin (sayi, toplam_satir)."""
    sayi = 0
    toplam = 0
    if not os.path.isdir(dizin):
        return (0, 0)
    for kok, _dirs, dosyalar in os.walk(dizin):
        # eski/ ve build/ skip — referans/arac, kod degil
        if "eski" in kok.replace("\\", "/").split("/"):
            continue
        if "build" in kok.replace("\\", "/").split("/"):
            continue
        for d in dosyalar:
            for u in uzantilar:
                if d.endswith(u):
                    sayi += 1
                    toplam += satir_say(os.path.join(kok, d))
    return (sayi, toplam)


def git_durum():
    try:
        branch = subprocess.run(
            ["git", "branch", "--show-current"],
            capture_output=True, text=True, timeout=5
        ).stdout.strip()
        commit = subprocess.run(
            ["git", "rev-parse", "--short", "HEAD"],
            capture_output=True, text=True, timeout=5
        ).stdout.strip()
        # Bütün branch'leri say
        branch_listesi = subprocess.run(
            ["git", "branch", "-a"],
            capture_output=True, text=True, timeout=5
        ).stdout
        toplam_branch = len([
            l for l in branch_listesi.splitlines()
            if l.strip() and "->" not in l
        ])
        return {"branch": branch, "commit": commit, "toplam_branch": toplam_branch}
    except (FileNotFoundError, subprocess.TimeoutExpired):
        return {"branch": "?", "commit": "?", "toplam_branch": 0}


def test_sayisi_tahmin():
    """test_tumu run etmeden, kod statik analizinden tahmin."""
    # test_*.c dosyalarinda TEST(...) veya test_sonuc(...) sayim
    test_dosya_sayisi = 0
    tahmini_test = 0
    for f in os.listdir("test") if os.path.isdir("test") else []:
        if not f.startswith("test_") or not f.endswith(".c"):
            continue
        test_dosya_sayisi += 1
        try:
            with open(os.path.join("test", f), "r",
                      encoding="utf-8", errors="ignore") as fh:
                icerik = fh.read()
            # test_sonuc cagrilari ~ test sayisi
            tahmini_test += icerik.count("test_sonuc(")
        except (IOError, OSError):
            pass
    # Snapshot test
    snapshot_sayi = 0
    if os.path.isdir("test/snapshots"):
        snapshot_sayi = len([
            f for f in os.listdir("test/snapshots") if f.endswith(".kem")
        ])
    return {
        "test_dosya": test_dosya_sayisi,
        "tahmini_birim_test": tahmini_test,
        "snapshot": snapshot_sayi,
    }


def main():
    p = argparse.ArgumentParser(description="KEMGU repo metrikleri")
    p.add_argument("--json", action="store_true", help="JSON cikti")
    args = p.parse_args()

    rapor = {}

    # Kod sayim
    src_n, src_l = dizin_topla("src", (".c", ".h"))
    test_n, test_l = dizin_topla("test", (".c", ".h"))
    runtime_n, runtime_l = dizin_topla("runtime", (".c", ".h"))
    stdlib_n, stdlib_l = dizin_topla("stdlib", (".kem",))
    ornek_n, ornek_l = dizin_topla("test/ornekler", (".kem",))
    snapshot_n, snapshot_l = dizin_topla("test/snapshots", (".kem",))
    belge_n, belge_l = dizin_topla("belgeler", (".md",))
    tools_n, tools_l = dizin_topla("tools", (".py",))

    rapor["kod"] = {
        "src_dosya": src_n, "src_satir": src_l,
        "test_dosya": test_n, "test_satir": test_l,
        "runtime_dosya": runtime_n, "runtime_satir": runtime_l,
        "tools_dosya": tools_n, "tools_satir": tools_l,
    }
    rapor["kemgu_kaynaklar"] = {
        "stdlib": {"dosya": stdlib_n, "satir": stdlib_l},
        "ornek":  {"dosya": ornek_n, "satir": ornek_l},
        "snapshot": {"dosya": snapshot_n, "satir": snapshot_l},
    }
    rapor["belgeler"] = {"dosya": belge_n, "satir": belge_l}
    rapor["git"] = git_durum()
    rapor["test"] = test_sayisi_tahmin()

    # Toplam satir
    rapor["toplam_satir"] = src_l + test_l + runtime_l + stdlib_l + tools_l

    if args.json:
        print(json.dumps(rapor, indent=2, ensure_ascii=False))
        return

    print("KEMGU Repo Metrikleri")
    print("=" * 40)
    print(f"\n## Git\n  Branch: {rapor['git']['branch']}")
    print(f"  Commit: {rapor['git']['commit']}")
    print(f"  Toplam branch: {rapor['git']['toplam_branch']}")

    print("\n## Kod (C / Header / Python)")
    print(f"  {'src/':<14} {src_n:>4} dosya, {src_l:>6} satir")
    print(f"  {'test/':<14} {test_n:>4} dosya, {test_l:>6} satir")
    print(f"  {'runtime/':<14} {runtime_n:>4} dosya, {runtime_l:>6} satir")
    print(f"  {'tools/':<14} {tools_n:>4} dosya, {tools_l:>6} satir")

    print("\n## KEMGU kaynaklari (.kem)")
    print(f"  {'stdlib/':<14} {stdlib_n:>4} dosya, {stdlib_l:>6} satir")
    print(f"  {'test/ornekler/':<14} {ornek_n:>4} dosya, {ornek_l:>6} satir")
    print(f"  {'test/snapshots/':<14} {snapshot_n:>4} dosya, {snapshot_l:>6} satir")

    print("\n## Belgeler")
    print(f"  {'belgeler/':<14} {belge_n:>4} dosya, {belge_l:>6} satir")

    print("\n## Test (statik tahmin)")
    print(f"  Test C dosyasi:    {rapor['test']['test_dosya']}")
    print(f"  Tahmini birim test: {rapor['test']['tahmini_birim_test']}")
    print(f"  Snapshot:          {rapor['test']['snapshot']}")

    print("\n## Toplam satir (C + KEMGU + Python)")
    print(f"  {rapor['toplam_satir']}")


if __name__ == "__main__":
    main()
