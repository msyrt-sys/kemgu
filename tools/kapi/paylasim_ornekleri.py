"""Aynı host kaynağına farklı söz dizimlerinden ulaşan küçük denetim örnekleri."""

from make_tarifleri import degiskenleri_ac
from paylasim_denetimi import kabuk, python_tara


def yollar(kayit):
    return {r.yol for r in kayit}


def denetle():
    tarif = degiskenleri_ac("mkdir -p $(ORTAK)", {"BUILD": "build", "ORTAK": "$(BUILD)/ortak"})
    assert yollar(kabuk(tarif, "Makefile", 1)) == {"build/ortak"}
    tarif = 'for spec in a:0 b:1; do mod=$${spec%%:*}; c="build/$$mod.kem"; cat giris > "$$c"; done'
    assert yollar(kabuk(tarif, "Makefile", 1, "iki_ornek")) == {"build/<mod>.kem"}
    sh = kabuk(
        'TMP=build/ortak\nmkdir -p "$TMP"\necho OK > "$TMP/son"\n'
        "qemu -serial file:build/seri -qmp unix:build/qmp,server=on\n"
        "qemu -netdev user,hostfwd=tcp:127.0.0.1:41357-:80\n"
        "qemu -drive file=build/ana,snapshot=on -drive file=build/veri,format=raw\n"
        "cc -o build/tek.o\n",
        "ornek.sh",
        1,
    )
    assert {
        "build/ortak",
        "build/ortak/son",
        "build/seri",
        "build/qmp",
        "build/tek.o",
        "build/veri",
        "tcp:41357",
    } <= yollar(sh)
    assert "build/ana" not in yollar(sh)
    py = python_tara(
        "ornek.py",
        """
from pathlib import Path
from socket import socket
import socket as soket
p = Path("build") / "ortak"
(p / "son").write_text("tamam")
s = soket.socket(soket.AF_INET,soket.SOCK_DGRAM)
s.bind(("127.0.0.1",41357))
komut = ["clang", "-o", str(Path("build") / "a.o")]
qemu = ["qemu", "-drive", "file=build/temel,readonly=on", "-drive", "file=build/ikinci"]
""",
    )
    assert yollar(py) == {"build/ortak/son", "udp:41357", "build/a.o", "build/ikinci"}, yollar(py)
    dinamik = kabuk(
        'W=$(mktemp -d build/tur.XXXXXX)\ncc -o "$W/tek.o"\nqemu -qmp "unix:$W/qmp,server=on"\n',
        "ozel.sh",
        1,
    )
    assert not dinamik, dinamik
    print("Paylaşım örnekleri geçti: çalışma dizini, yönlendirme, port, QMP/seri ve özel mktemp.")


if __name__ == "__main__":
    denetle()
