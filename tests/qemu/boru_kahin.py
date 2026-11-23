"""Boru ve yönlendirme çıktısını gerçek diskteki baytlardan doğrular."""
import argparse
from pathlib import Path

from dalga12_fat import Fat
from dalga12_kahin import boot, fsck, sha
from surec_imaj import imajlar
from surec_kahin import SurecConsole
from uefi_kabuk_kahin import keys


class BoruConsole(SurecConsole):
    def input(self,text):
        for c in text+'\n':
            if c in '<>|':
                mod = {'<':None,'>':'shift','|':'alt_r'}[c]
                if mod:
                    self.key(mod,True)
                self.key('less',True)
                self.key('less',False)
                if mod:
                    self.key(mod,False)
            else:
                k,shift = {'-':('equal',False)}.get(c,keys(c))
                if shift:
                    self.key('shift',True)
                self.key(k,True)
                self.key(k,False)
                if shift:
                    self.key('shift',False)


def say(b):
    return f'satir={b.count(bytes([10]))} kelime={len(b.split())} bayt={len(b)}\n'.encode()


def kos(w,tur):
    notlar='ilk\nağaç bir\nson\nağaç iki\n'.encode()
    buyuk=b'kesintisiz baytlar\n'*1401
    dosyalar={p.name:p.read_bytes() for p in (w/'kullanici').glob('*.elf')}
    dosyalar.update({'iz.txt':notlar,'akim.txt':buyuk})
    usb,nvme,sata=imajlar(w,dosyalar)
    bek={}
    ilk={p:sha(p) for p in (nvme.disk,sata.disk)}
    with boot(w,usb[0],nvme,sata,klavye='ps2') as (q,p,s):
        c=BoruConsole(w,q,p,s)
        c.run('bagla --yazilabilir usb0p2')
        c.run('cd /diskler/usb0p2/bin')
        if tur == 'abi':
            c.run('baslat boruden',b'[boruden.elf pid=1 cikis=0]\n')
        elif tur == 'zincir':
            c.run('kedi < iz.txt | bul ağaç | say > sonuc.txt')
            bek['sonuc.txt']=say('2:ağaç bir\n4:ağaç iki\n'.encode())
        elif tur == 'dolu':
            c.run('kedi akim.txt | say',say(buyuk))
        else:
            c.run('echo ilk > n.txt')
            c.run('echo ikinci >> n.txt')
            c.run('kedi < n.txt',b'ilk\nikinci\n')
            bek['n.txt']=b'ilk\nikinci\n'
            c.run('echo eksik |',b'HATA eksik boru komutu\n')
        c.shutdown()
    fsck(w,usb[1],tur)
    fat=Fat(usb[1].read())
    for ad,b in bek.items():
        assert fat.get('/BIN/'+ad) == b,(ad,fat.get('/BIN/'+ad),b)
    assert all(sha(p) == h for p,h in ilk.items())
    print('BORU OK',tur,flush=True)


if __name__ == '__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('dizin',type=Path)
    p.add_argument('--tur',choices=('dolu','zincir','yon','abi'))
    a=p.parse_args()
    for tur in ([a.tur] if a.tur else ('dolu','zincir','yon','abi')):
        kos(a.dizin.resolve(),tur)
