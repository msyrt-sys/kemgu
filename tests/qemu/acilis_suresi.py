"""OVMF seri devir işaretini gerçek kabuk istemi ve konuk TSC ölçümüyle eşler."""
import argparse
import re
import time
from statistics import median
from pathlib import Path

from dalga12_kahin import Console, boot
from surec_imaj import imajlar

ASAMALAR = ['devir','bellek','acpi','pci','usb','diskler','kabuk']


def olc(w):
    usb,nvme,sata=imajlar(w,{})
    with boot(w,usb[0],nvme,sata) as (q,p,s):
        ilk=None
        son_sure=time.monotonic()+20
        while time.monotonic() < son_sure:
            ham=s.read_bytes() if s.exists() else b''
            simdi=time.monotonic_ns()
            if ilk is None and b'KEMGU EFI DEVIR' in ham:
                ilk=simdi
            if b'KABUK_HAZIR\n' in ham:
                break
            assert p.poll() is None
            time.sleep(.002)
        assert ilk is not None and ham.count(b'KABUK_HAZIR\n') == 1
        host=(simdi-ilk)/1e6
        damgalar=[(ad.decode(),int(t)) for ad,t in re.findall(rb'^ACILIS_TSC (\w+) (\d+)$',ham,re.M)]
        assert [ad for ad,_ in damgalar] == ASAMALAR,damgalar
        degerler=[v for _,v in damgalar]
        assert degerler[0] == 0 and all(a<b for a,b in zip(degerler,degerler[1:]))
        hiz=int(re.search(rb'^ACILIS_CEVRIM_MS (\d+)$',ham,re.M)[1])
        assert hiz > 0
        guest=degerler[-1]/hiz
        print('ACILIS_MS', ' '.join(f'{ad}={v/hiz:.1f}' for ad,v in damgalar),f'host={host:.1f}',flush=True)
        c=Console(w,q,p,s)
        c.shutdown()
    return host,guest


if __name__ == '__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('dizin',type=Path)
    p.add_argument('--tekrar',type=int,default=5)
    p.add_argument('--esik',type=float,default=650)
    a=p.parse_args()
    assert a.tekrar > 0
    host,konuk=zip(*(olc(a.dizin.resolve()) for _ in range(a.tekrar)))
    print(f'ACILIS_MEDYAN host={median(host):.1f} konuk={median(konuk):.1f} esik={a.esik:.1f}',flush=True)
    assert median(host) < a.esik and median(konuk) < a.esik
