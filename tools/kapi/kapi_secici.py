"""Çalışma ağacı değişikliklerini bildirime göre GNU make kapılarına dönüştürür."""

import argparse
import fnmatch
import os
import re
import subprocess
import sys

from kapi_haritasi import KOK, TABLO, harita_metni, kaynak_grafi, surucu_kapilari
from make_tarifleri import make_programi

TAM = frozenset({"test_tumu"})


def git(kok, *arg):
    return subprocess.check_output(["git", *arg], cwd=kok)


def yollar(veri):
    return [os.fsdecode(b) for b in veri.split(b"\0") if b]


def degisenler(kok, taban):
    agac = (
        git(kok, "rev-parse", "--verify", "--end-of-options", taban + "^{tree}")
        .decode("ascii")
        .strip()
    )
    fark = yollar(git(kok, "diff", "--no-renames", "--name-only", "-z", agac, "--"))
    yeni = yollar(git(kok, "ls-files", "--others", "--exclude-standard", "-z", "--"))
    return sorted(set(fark + yeni))


def kurallari_oku(metin):
    kurallar = {}
    for no, satir in enumerate(metin.splitlines(), 1):
        if not satir or satir.startswith("#"):
            continue
        desen, ayir, deger = satir.partition("\t")
        kapilar = deger.split()
        if (
            not ayir
            or desen in kurallar
            or not kapilar
            or not all(re.fullmatch(r"calistir_\w+|test_tumu", k) for k in kapilar)
        ):
            raise ValueError(f"Kapı haritası {no}. satır geçersiz")
        kurallar[desen] = frozenset(kapilar)
    return kurallar


def sec(kurallar, dosyalar):
    sonuc = set()
    nedenler = []
    for yol in sorted(set(dosyalar)):
        desen = next((d for d in kurallar if fnmatch.fnmatchcase(yol, d)), None)
        kapilar = kurallar.get(desen, TAM)
        sonuc.update(kapilar)
        nedenler.append((yol, desen or "eşlenmeyen dosya", kapilar))
    return (TAM if "test_tumu" in sonuc else sonuc), nedenler


def main():
    p = argparse.ArgumentParser(description=__doc__)
    g = p.add_mutually_exclusive_group()
    g.add_argument("--taban", default=os.environ.get("TABAN", "origin/main"))
    g.add_argument(
        "--dosya", action="append", help="Git yerine verilen dosya kümesini dene"
    )
    p.add_argument("--calistir", action="store_true")
    a = p.parse_args()
    dosyalar = a.dosya if a.dosya is not None else degisenler(KOK, a.taban)
    metin = TABLO.read_text(encoding="utf-8")
    kurallar = kurallari_oku(metin)
    kapilar, nedenler = sec(kurallar, dosyalar)
    for yol, desen, hedefler in nedenler:
        print(f"{yol!r} → {desen} ({len(hedefler)} hedef)")
    # Tek sürücü içe aktarma eklerse eski tabloda başka bir alanın tüketicisi
    # eksik kalabilir. Bayat tabloda kısmi seçim güvenli değildir.
    if (
        kapilar
        and kapilar != TAM
        and any(d.startswith("drivers/") for _, d, _ in nedenler)
    ):
        guncel = harita_metni(surucu_kapilari(KOK, kaynak_grafi(KOK)), metin)
        if metin != guncel:
            print("Sürücü bağımlılıkları değişti, harita yenilenene kadar tam takım.")
            kapilar = TAM
    sirali = sorted(kapilar)
    print("Seçim:", " ".join(sirali) if sirali else "değişiklik yok", flush=True)
    if a.calistir and sirali:
        return subprocess.run(
            [make_programi(), "-j10", "--output-sync=target", *sirali],
            cwd=KOK,
            check=False,
        ).returncode
    return 0


if __name__ == "__main__":
    sys.exit(main())
