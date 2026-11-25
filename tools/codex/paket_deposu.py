"""Yerel ELF çıktılarından paketler.txt ve HTTP ile sunulacak dosyaları üretir."""
import argparse
import hashlib
import os
from pathlib import Path
import re
import tempfile


def yayinla(yol, veri):
    with tempfile.NamedTemporaryFile(dir=yol.parent, delete=False) as f:
        gecici = Path(f.name)
        try:
            f.write(veri)
            f.close()
            os.replace(gecici, yol)
        finally:
            gecici.unlink(missing_ok=True)


def depo(hedef, programlar, surum):
    hedef.mkdir(parents=True, exist_ok=True)
    if not re.fullmatch(r'[a-z0-9.-]{1,24}', surum):
        raise ValueError('geçersiz sürüm')
    satirlar = []
    adlar = set()
    for elf in sorted(programlar):
        ad = elf.stem
        if elf.suffix != '.elf' or not re.fullmatch(r'[a-z][a-z0-9_-]{0,31}', ad) or ad in adlar:
            raise ValueError(f'geçersiz veya yinelenen ad {elf.name}')
        adlar.add(ad)
        veri = elf.read_bytes()
        if veri[:6] != b'\x7fELF\x02\x01' or not 64 <= len(veri) <= 16 << 20:
            raise ValueError(f'geçersiz ELF {elf}')
        yayinla(hedef/elf.name, veri)
        satirlar.append(f'{ad} {surum} {elf.name} {len(veri)} {hashlib.sha256(veri).hexdigest()}\n')
    if not 1 <= len(satirlar) <= 64:
        raise ValueError('depo 1..64 paket içermeli')
    yayinla(hedef/'paketler.txt', ''.join(satirlar).encode('ascii'))


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('hedef', type=Path)
    p.add_argument('programlar', type=Path, nargs='+')
    p.add_argument('--surum', default='1')
    a = p.parse_args()
    depo(a.hedef, a.programlar, a.surum)
