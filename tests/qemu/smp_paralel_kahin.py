"""Bölütlü konuk eleğini bağımsız tek Python eleği ve gerçek QEMU saatleriyle karşılaştır."""

import math
import re
import sys
from pathlib import Path
from time import monotonic

from bekle import bekle
from dalga12_kahin import boot
from smp_kahin import cpu_listesi
from surec_imaj import imajlar
from surec_kahin import SurecConsole

SINIR = 1 << 25
ADETLER = (1, 2, 4, 8)


def sekizde_birler():
    asal = bytearray(b"\x01") * SINIR
    asal[:2] = b"\x00\x00"
    for p in range(2, math.isqrt(SINIR - 1) + 1):
        if asal[p]:
            adet = (SINIR - 1 - p * p) // p + 1
            asal[p * p :: p] = bytes(adet)
    return [asal.count(1, i * SINIR // 8, (i + 1) * SINIR // 8) for i in range(8)]


def olc(c, cpular, n, parcalar):
    once = c.output()
    c.input(f"calistir /diskler/usb0p2/bin/paralel.elf {n}")
    baslik = f"ASAL BAS {n} {SINIR}\n".encode()

    def oku():
        veri = c.output()[len(once) :]
        if b"KABUK_HAZIR\n" in veri and baslik not in veri:
            raise AssertionError(veri)
        return veri

    bekle("asal programı başladı", oku, lambda s: baslik in s, sure=30, surec=c.p)
    ilk = monotonic()
    veri = bekle(
        "sonuçlar ve çocuğun çıkışı",
        oku,
        lambda s: s.endswith(b"KABUK_HAZIR\n"),
        sure=120,
        surec=c.p,
    )
    sure = monotonic() - ilk
    assert re.search(rb"\[paralel\.elf pid=\d+ cikis=0\]\n", veri), veri
    satirlar = re.findall(rb"^ASAL PARCA (\d+) (\d+)$", veri, re.M)
    bol = 8 // n
    beklenen = {i: sum(parcalar[i * bol : (i + 1) * bol]) for i in range(n)}
    assert len(satirlar) == n and {int(i): int(k) for i, k in satirlar} == beklenen, (
        veri
    )
    toplam = re.findall(rb"^ASAL TOPLAM (\d+) (\d+)$", veri, re.M)
    assert toplam == [(str(n).encode(), str(sum(parcalar)).encode())], (
        toplam,
        sum(parcalar),
    )
    c.expected += veri
    print(
        f"PARALEL vCPU={cpular} N={n} sn={sure:.3f} toplam={sum(parcalar)} parçalar={beklenen}",
        flush=True,
    )


def tur(w, cpular, adetler, parcalar):
    usb, nvme, sata = imajlar(
        w, {"paralel.elf": (w / "kullanici/paralel.elf").read_bytes()}
    )
    with boot(w, usb[0], nvme, sata, smp=cpular) as (q, p, ser):
        cpu_listesi(q, cpular)
        c = SurecConsole(w, q, p, ser)
        for n in adetler:
            olc(c, cpular, n, parcalar)
        c.shutdown()


if __name__ == "__main__":
    dizin = Path(sys.argv[1]).resolve()
    sekiz = sekizde_birler()
    for cekirdek in ADETLER:
        tur(dizin, cekirdek, ADETLER, sekiz)
