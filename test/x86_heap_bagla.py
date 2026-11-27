"""Birleşik SMP çekirdeğinin malloc/free girişlerini aynı algoritmanın kilitli sürümüne bağla."""

import sys
from pathlib import Path


def bagla(yol):
    metin = yol.read_text(encoding="utf-8")
    for islem in ("ayir", "iade"):
        kaynak = f"kem_heap_{islem}"
        hedef = f"smp_heap_{islem}"
        imza = ("çıplak işlev malloc(n: tam64) -> *tam8 { ver " + kaynak + "(n); }"
                if islem == "ayir" else "çıplak işlev free(p: *tam8) { " + kaynak + "(p); }")
        if metin.count(imza) != 1 or f"çıplak işlev {hedef}(" not in metin:
            raise ValueError(f"{yol}: {islem} için tek heap girişi ve SMP sarmalı gerekli")
        metin = metin.replace(imza, imza.replace(kaynak, hedef))
    yol.write_text(metin, encoding="utf-8")


if __name__ == "__main__":
    bagla(Path(sys.argv[1]))
