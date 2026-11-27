#!/usr/bin/env python3
"""Ölçümlü make koşumunun kapı sürelerini ve bütçe aşımlarını raporlar."""
import argparse
import math
import os
import shutil
import subprocess
from dataclasses import dataclass, field
from pathlib import Path


@dataclass
class Olcum:
    sureler: dict = field(default_factory=dict)
    hatalar: set = field(default_factory=set)
    baslangic: float = math.inf
    bitis: float = -math.inf

    @property
    def duvar(self):
        return self.bitis - self.baslangic if self.sureler else 0.0


def kayitlari_oku(yol):
    sonuc = Olcum()
    with yol.open(encoding="utf-8") as girdi:
        for no, satir in enumerate(girdi, 1):
            if not satir.strip():
                continue
            try:
                ad, bas, son, kod = satir.rstrip("\r\n").split("\t")
                bas, son, kod = float(bas), float(son), int(kod)
                if not ad or not math.isfinite(bas) or not math.isfinite(son) or son < bas:
                    raise ValueError("geçersiz hedef veya süre")
            except ValueError as hata:
                raise ValueError(f"{yol}:{no}: bozuk zaman kaydı ({hata})") from hata
            sonuc.sureler[ad] = sonuc.sureler.get(ad, 0.0) + (son - bas)
            sonuc.baslangic = min(sonuc.baslangic, bas)
            sonuc.bitis = max(sonuc.bitis, son)
            if kod:
                sonuc.hatalar.add(ad)
    return sonuc


def butceleri_oku(yol):
    butceler = {}
    with yol.open(encoding="utf-8") as girdi:
        for no, satir in enumerate(girdi, 1):
            if not satir.strip() or satir.startswith("#"):
                continue
            try:
                ad, deger, kategori = satir.rstrip("\r\n").split("\t")
                if (ad, deger, kategori) == ("kapi", "butce_sn", "kategori"):
                    continue
                sure = int(deger)
                if not ad or ad in butceler or sure <= 0 or kategori not in {"host", "qemu-x86", "qemu-arm", "derleme"}:
                    raise ValueError("geçersiz veya yinelenen bütçe")
            except ValueError as hata:
                raise ValueError(f"{yol}:{no}: bozuk bütçe ({hata})") from hata
            butceler[ad] = sure
    return butceler


def bicim(sure):
    dakika, saniye = divmod(round(sure), 60)
    saat, dakika = divmod(dakika, 60)
    onek = f"{saat} sa " if saat else ""
    return f"{onek}{dakika:02d} dk {saniye:02d} sn"


def raporla(kayit, butceler, build, adet, siki):
    if not kayit.sureler:
        print("ZAMAN RAPORU: kayıt yok")
        return 0
    kapilar = {ad: s for ad, s in kayit.sureler.items() if ad.startswith("calistir_")}
    toplam = sum(kapilar.values())
    print(f"=== ZAMAN RAPORU: duvar saati {bicim(kayit.duvar)} · {len(kapilar)} kapı · "
          f"kapı toplamı {bicim(toplam)} · derleme/diğer {bicim(sum(kayit.sureler.values()) - toplam)} ===")
    asim = False
    for sira, (ad, sure) in enumerate(sorted(kapilar.items(), key=lambda kayit: (-kayit[1], kayit[0]))):
        uyari = ""
        if ad not in butceler:
            uyari = "  bütçe yok"
        elif sure > butceler[ad]:
            uyari = f"  ⚠ BÜTÇE ({sure:.3f} sn > {butceler[ad]} sn)"
            asim = True
        if sira < adet or uyari or ad in kayit.hatalar:
            pay = 100 * sure / kayit.duvar if kayit.duvar else 0
            hata = "  🔴" if ad in kayit.hatalar else ""
            print(f"  {bicim(sure):>16}  %{pay:4.1f}  {ad}{hata}{uyari}")
    if kayit.hatalar:
        print("  başarısız: " + " ".join(sorted(kayit.hatalar)))
    try:
        boy = subprocess.check_output(["du", "-sh", str(build)], encoding="utf-8").split()[0]
    except (OSError, IndexError, subprocess.CalledProcessError):
        boy = "?"
    bos = shutil.disk_usage(build).free / 2**30
    print(f"  disk: {build} {boy} · boş {bos:.0f} GiB")
    return 3 if siki and asim else 0


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("zaman", type=Path)
    p.add_argument("build", type=Path, nargs="?", default=Path("build"))
    p.add_argument("adet", type=int, nargs="?", default=15)
    p.add_argument("--butce", type=Path, default=Path(__file__).with_name("kapi_butce.tsv"))
    args = p.parse_args()
    try:
        sonuc = raporla(kayitlari_oku(args.zaman), butceleri_oku(args.butce), args.build,
                        args.adet, os.environ.get("ZAMAN_BUTCE_SIKI") == "1")
    except (OSError, ValueError) as hata:
        p.exit(2, f"Zaman raporu üretilemedi: {hata}\n")
    raise SystemExit(sonuc)
