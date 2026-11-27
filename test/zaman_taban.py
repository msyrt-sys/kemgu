#!/usr/bin/env python3
"""Üç ölçümde aynı kapıların gözlendiğini doğrular ve taban sürelerini çıkarır."""
import argparse
import math
import statistics
from pathlib import Path

from zaman_raporu import kayitlari_oku


def taban_yaz(kayitlar, cikti):
    turlar = [kayitlari_oku(yol) for yol in kayitlar]
    adlar = sorted({ad for tur in turlar for ad in tur.sureler if ad.startswith("calistir_")})
    if not adlar:
        raise ValueError("üç ölçümde kapı kaydı bulunamadı")
    satirlar = ["kapi\ttur1_sn\ttur2_sn\ttur3_sn\tortalama_sn\ten_buyuk_sapma_sn\tonerilen_butce_sn"]
    for ad in adlar:
        eksikler = [str(i) for i, tur in enumerate(turlar, 1) if ad not in tur.sureler]
        if eksikler:
            raise ValueError(f"{ad} için eksik tur: {', '.join(eksikler)}")
        sureler = [tur.sureler[ad] for tur in turlar]
        ortalama = statistics.mean(sureler)
        sapma = max(abs(s - ortalama) for s in sureler)
        butce = max(60, math.ceil(1.5 * max(sureler)))
        satirlar.append("\t".join([ad, *(f"{s:.3f}" for s in [*sureler, ortalama, sapma]), str(butce)]))
    cikti.write_text("\n".join(satirlar) + "\n", encoding="utf-8")
    for i, tur in enumerate(turlar, 1):
        print(f"Tur {i}: {tur.duvar:.1f} sn, hatalı hedefler: {', '.join(sorted(tur.hatalar)) or 'yok'}")
    print(f"{len(adlar)} kapının üç ölçümü: {cikti}")


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("kayitlar", type=Path, nargs=3, metavar="KAYIT")
    p.add_argument("--cikti", type=Path, default=Path("build/zaman_taban.tsv"))
    args = p.parse_args()
    try:
        taban_yaz(args.kayitlar, args.cikti)
    except (OSError, ValueError) as hata:
        p.exit(2, f"Taban ölçümü okunamadı: {hata}\n")
