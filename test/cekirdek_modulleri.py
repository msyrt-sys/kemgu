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


def modul_ozeti(giris):
    ozet = hashlib.sha256()
    sirada = [giris]
    gorulen = {giris.resolve()}
    while sirada:
        kaynak = sirada.pop()
        for es in YOL.finditer(kaynak.read_text(encoding="utf-8")):
            adlar = re.findall(AD, YORUM.sub("", es[1]))
            # Eski çıplak içe aktarmalarda son bileşen bir üye de olabilir.
            for son in range(1, len(adlar) + 1):
                bagil = Path(*adlar[:son]).with_suffix(".kem")
                for kok in (kaynak.parent, Path("."), Path("kütüphane")):
                    aday = kok / bagil
                    if not aday.is_file():
                        continue
                    yol = aday.resolve()
                    if yol not in gorulen:
                        gorulen.add(yol)
                        sirada.append(aday)
                        ozet.update(bagil.as_posix().encode("utf-8") + b"\0")
                        ozet.update(hashlib.sha256(aday.read_bytes()).digest())
                    break
    return ozet.hexdigest()


if __name__ == "__main__":
    print(modul_ozeti(Path(sys.argv[1])))
