"""GNU make'in ayrıştırdığı hedef, önkoşul ve tarifleri komut çalıştırmadan okur."""

import os
import re
import shutil
import subprocess
from dataclasses import dataclass


@dataclass
class Tarif:
    onkosullar: tuple
    satir: int
    komut: str


MAKE_DEGISKEN = re.compile(r"(?<!\$)\$\(([\w.-]+)\)")


def degiskenleri_ac(metin, degerler):
    # Yalnız makefile'ın bildirdiği adları aç. $(shell …) ve dış ortam çalıştırılmaz.
    def ac(s, zincir):
        def yerine(m):
            ad = m[1]
            if ad in zincir or ad not in degerler:
                return m[0]
            return ac(degerler[ad], zincir | {ad})

        return MAKE_DEGISKEN.sub(yerine, s)

    return ac(metin, set())


def make_programi():
    return os.environ.get("MAKE") or shutil.which("mingw32-make") or "make"


def tarifleri_oku(kok, makefile="Makefile"):
    # Boş hedef + soru kipi: özyinelemeli make içeren tarifler bile yürütülmez.
    env = {**os.environ, "LC_ALL": "C", "MAKEFLAGS": "", "MFLAGS": ""}
    sonuc = subprocess.run(
        [
            make_programi(),
            "-prRq",
            "--no-print-directory",
            "-f",
            str(makefile),
            "--eval=__kemgu_tarif_oku: ;",
            "__kemgu_tarif_oku",
        ],
        cwd=kok,
        env=env,
        capture_output=True,
        check=False,
        encoding="utf-8",
        errors="replace",
    )
    if sonuc.returncode not in (0, 1):
        raise ValueError(sonuc.stderr.strip())
    onsoz, ayirac, veri = sonuc.stdout.partition("# Files\n")
    degerler = dict(re.findall(r"^# makefile[^\n]*\n([\w.-]+)[ \t]*:?=[ \t]*([^\n]*)", onsoz, re.MULTILINE))

    if not ayirac:
        raise ValueError("GNU make hedef veritabanı bulunamadı")
    # -rR ile örtük kurallar bölümünde yalnız deponun desen tarifleri kalır.
    desenler = sonuc.stdout.partition("# Implicit Rules\n")[2].partition("# Files\n")[0]
    veri = desenler + "\n\n" + veri
    tarifler = {}
    for blok in veri.split("\n\n"):
        komut = re.search(r"^#\s+recipe to execute \(from '([^']+)', line (\d+)\):\n", blok, re.MULTILINE)
        baslik = blok[: komut.start()] if komut else blok
        # Hedefe özgü değişken satırları, asıl bağımlılık satırının önüne gelir.
        satirlar = re.findall(r"^([^#\s:]+):(?![=:])([^\n]*)$", baslik, re.MULTILINE)
        if not satirlar:
            continue
        ad, deps = satirlar[-1]
        tarifler[ad] = Tarif(
            tuple(x for x in deps.split() if x != "|"),
            int(komut[2]) if komut else 0,
            degiskenleri_ac(blok[komut.end() :], degerler) if komut else "",
        )
    if "test_tumu" not in tarifler:
        raise ValueError("test_tumu önkoşulları okunamadı")
    return tarifler


def onkosul_agaci(tarifler, hedef):
    bekleyen = [hedef]
    gorulen = set()
    while bekleyen:
        ad = bekleyen.pop()
        if ad in gorulen or ad not in tarifler:
            continue
        gorulen.add(ad)
        bekleyen.extend(tarifler[ad].onkosullar)
    return gorulen
