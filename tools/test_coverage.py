#!/usr/bin/env python3
"""
KEMGU Test Coverage Raporu

gcov ciktisini parse eder, hangi src/ fonksiyonlari test edilmemis
veya az test edilmis listeler. Once Makefile coverage hedefi
(--coverage flag'i ile derle) sonra bu script.

Kullanim:
    # 1) Coverage destekli derle (gcov hedefi gelecek)
    # python tools/test_coverage.py [--show-uncovered] [--src-dir src]

NOT: Bu MVP — gcov henuz Makefile'a entegre degil. Script parse
mantigini hazirlar; gerçek gcov ciktisi olusunca dogrudan calisir.
"""

import sys
import os
import re
import argparse
import subprocess


def gcov_calistir(src_dir: str = "src", build_dir: str = "build"):
    """Her .gcda dosyasi icin gcov calistir, *.gcov uretir."""
    sonuclar = {}
    if not os.path.isdir(build_dir):
        return sonuclar
    for f in os.listdir(build_dir):
        if f.endswith(".gcda"):
            o_path = os.path.join(build_dir, f)
            try:
                subprocess.run(
                    ["gcov", "-b", "-c", o_path],
                    cwd=build_dir, capture_output=True, timeout=10
                )
            except (FileNotFoundError, subprocess.TimeoutExpired):
                pass
    # *.gcov dosyalarini topla
    for f in os.listdir("."):
        if f.endswith(".gcov"):
            sonuclar[f] = os.path.join(".", f)
    return sonuclar


def gcov_parse(gcov_dosya: str):
    """gcov dosyasini parse et: (toplam_satir, kapsanan, kapsanmayan_satirlar)"""
    toplam, kapsanan = 0, 0
    kapsanmayan_satirlar = []
    fonk_kapsam = {}
    with open(gcov_dosya, "r", encoding="utf-8", errors="ignore") as f:
        icerik = f.read()

    # Satir formati: "   COUNT:LINE:source"
    # COUNT: "#####" = hic calistirilmadi, "-" = executable degil, sayi = exec count
    for satir in icerik.splitlines():
        m = re.match(r"\s*([#\-\d]+)\s*:\s*(\d+)\s*:(.*)", satir)
        if not m:
            continue
        count_s, line_no, _kaynak = m.group(1), int(m.group(2)), m.group(3)
        if count_s == "-":
            continue
        toplam += 1
        if count_s == "#####":
            kapsanmayan_satirlar.append(line_no)
        else:
            kapsanan += 1
    return (toplam, kapsanan, kapsanmayan_satirlar)


def main():
    p = argparse.ArgumentParser(description="KEMGU test coverage raporu")
    p.add_argument("--src-dir", default="src", help="kaynak dizini")
    p.add_argument("--build-dir", default="build", help="build dizini")
    p.add_argument("--show-uncovered", action="store_true",
                   help="Kapsanmayan fonksiyon/satirlar listele")
    args = p.parse_args()

    print("KEMGU Test Coverage Raporu")
    print("=" * 40)

    gcov_dosyalar = gcov_calistir(args.src_dir, args.build_dir)
    if not gcov_dosyalar:
        print(f"[!] {args.build_dir}/*.gcda bulunamadi.")
        print("    Coverage icin --coverage flag ile derleyin:")
        print("    CFLAGS='-Wall -Wextra -std=c11 -g -O0 -fprofile-arcs "
              "-ftest-coverage' make")
        print("    make test_tumu")
        print("    python tools/test_coverage.py")
        sys.exit(0)

    toplam_top = 0
    kapsanan_top = 0
    print(f"\n{'Dosya':<30} {'Toplam':>8} {'Kapsanan':>10} {'Oran':>8}")
    print("-" * 60)
    for adi, yol in sorted(gcov_dosyalar.items()):
        t, k, eksik = gcov_parse(yol)
        if t == 0:
            continue
        toplam_top += t
        kapsanan_top += k
        oran = (k * 100.0 / t) if t > 0 else 0
        print(f"{adi:<30} {t:>8} {k:>10} {oran:>7.1f}%")
        if args.show_uncovered and eksik:
            print(f"  Kapsanmayan satirlar: {eksik[:10]}"
                  f"{'...' if len(eksik) > 10 else ''}")

    print("-" * 60)
    genel_oran = (kapsanan_top * 100.0 / toplam_top) if toplam_top > 0 else 0
    print(f"{'TOPLAM':<30} {toplam_top:>8} {kapsanan_top:>10} {genel_oran:>7.1f}%")


if __name__ == "__main__":
    main()
