"""Birleşik çekirdek kaynağından erişilen .kem modüllerinin içerik özeti."""
import hashlib
import re
import sys
from pathlib import Path

# Modül adındaki sözcüklerin arasına boşluk ve yorum girebilir. Metin içindeki
# fazladan bir eşleşme en çok gereksiz derlemeye yol açar. Yol yalnız adlardan oluşur.
ARA = r"(?:\s|//[^\n]*(?:\n|$)|/\*.*?\*/)*"
AD = r"[^\W\d]\w*"
YOL = re.compile(rf"\bkullan\b{ARA}({AD}(?:{ARA}::{ARA}{AD})*)", re.DOTALL)
YORUM = re.compile(r"/\*.*?\*/|//[^\n]*", re.DOTALL)


def dogrudan_moduller(kaynak, kok):
    for es in YOL.finditer(kaynak.read_text(encoding="utf-8")):
        adlar = re.findall(AD, YORUM.sub("", es[1]))
        # Eski çıplak içe aktarmalarda son bileşen bir üye de olabilir.
        for son in range(1, len(adlar) + 1):
            bagil = Path(*adlar[:son]).with_suffix(".kem")
            for dizin in (kaynak.parent, kok, kok / "kütüphane"):
                aday = dizin / bagil
                if aday.is_file():
                    yield bagil, aday
                    break


def modul_ozeti(giris):
    ozet = hashlib.sha256()
    sirada = [giris]
    gorulen = {giris.resolve()}
    while sirada:
        kaynak = sirada.pop()
        for bagil, aday in dogrudan_moduller(kaynak, Path(".")):
            yol = aday.resolve()
            if yol not in gorulen:
                gorulen.add(yol)
                sirada.append(aday)
                ozet.update(bagil.as_posix().encode("utf-8") + b"\0")
                ozet.update(hashlib.sha256(aday.read_bytes()).digest())
    return ozet.hexdigest()


if __name__ == "__main__":
    print(modul_ozeti(Path(sys.argv[1])))
