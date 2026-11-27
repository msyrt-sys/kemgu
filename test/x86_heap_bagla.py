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
    # Ortak görev/kanal paniği de SMP açılışından sonra aynı sahip seçimine girer.
    panik = "çıplak işlev kem_panik(m: metin) {"
    if metin.count(panik) != 1:
        raise ValueError(f"{yol}: ortak panik girişi bulunamadı")
    metin = metin.replace(panik, panik + "\n    eğer kis_panik_lapic!=0 { kdl_panik(m); ver; }")
    yol.write_text(metin, encoding="utf-8")


if __name__ == "__main__":
    bagla(Path(sys.argv[1]))
