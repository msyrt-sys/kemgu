"""OVMF seri devir işaretini gerçek kabuk istemi ve konuk TSC ölçümüyle eşler."""
from acilis_karar import karar, yabanci_qemu
from bekle import bekle, seri_oku
import argparse
import re
import time
from statistics import median
from pathlib import Path

from dalga12_kahin import Console, boot
from surec_imaj import imajlar

ASAMALAR = ['devir','bellek','acpi','pci','usb','diskler','kabuk']


def olc(w):
    # [D-794] Çekişme örneği kendi QEMU'muz kapalıyken alınır (açılıştan önce ve sonra).
    cekisme=yabanci_qemu()
    usb,nvme,sata=imajlar(w,{})
    with boot(w,usb[0],nvme,sata) as (q,p,s):
        ilk=None
        def ornek():
            nonlocal ilk
            ham=seri_oku(s)
            simdi=time.monotonic_ns()
            if ilk is None and b'KEMGU EFI DEVIR' in ham: ilk=simdi
            return ham,simdi
        ham,simdi=bekle('EFI devir ve KABUK_HAZIR',ornek,lambda o:b'KABUK_HAZIR\n' in o[0],
                         sure=20,aralik=.002,surec=p)
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
    return host,guest,max(cekisme,yabanci_qemu())


def tur(w,tekrar,esik):
    host,konuk,cekisme=zip(*(olc(w) for _ in range(tekrar)))
    print(f'ACILIS_MEDYAN host={median(host):.1f} konuk={median(konuk):.1f} esik={esik:.1f} '
          f'cekisme={max(cekisme)}',flush=True)
    return median(host),median(konuk),max(cekisme)


if __name__ == '__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('dizin',type=Path)
    p.add_argument('--tekrar',type=int,default=5)
    p.add_argument('--esik',type=float,default=650)
    a=p.parse_args()
    assert a.tekrar > 0
    w=a.dizin.resolve()
    assert karar(lambda: tur(w,a.tekrar,a.esik),a.esik),'açılış süresi eşiği aşıldı'
