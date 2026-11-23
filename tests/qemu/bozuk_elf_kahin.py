"""Kısa dosyayı konukta yüklemeyi dene ve aynı kabukta ikinci komutu çalıştır."""
import argparse
from pathlib import Path

from dalga12_kahin import Console, boot, sha
from surec_imaj import imajlar


def kos(w):
    usb, nvme, sata = imajlar(w, {'kisa.elf': b'ELF'})
    once = {p: sha(p) for p in (usb[0].disk, nvme.disk, sata.disk)}
    with boot(w, usb[0], nvme, sata) as (q, p, seri):
        c = Console(w, q, p, seri)
        c.run('calistir /diskler/usb0p2/bin/kisa.elf', b'HATA ELF basligi eksik\n')
        c.run('echo devam', b'devam\n')
        c.shutdown()
    assert all(sha(p) == h for p,h in once.items())
    print('BOZUK_ELF OK', flush=True)


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('dizin', type=Path)
    kos(p.parse_args().dizin.resolve())
