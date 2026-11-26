"""Paket yöneticisinin SHA-256 son dolgusu ve büyük ELF aktarımı için ek gerçek örnek."""
import hashlib
from pathlib import Path
import sys

from ag_arac_kahin import komut
from ag_http_kahin import HttpKonsol
from ag_paket_kahin import depo_ac, fat_oku
from dalga12_fat import Fat
from dalga12_kahin import boot, fsck, wait_for
from surec_imaj import imajlar


def kos(w):
    depo=w/'buyuk-depo'
    depo.mkdir(exist_ok=True)
    kaynak=Path('build/paket_deposu/hesapla.elf').read_bytes()
    boy=512*1024+61
    veri=kaynak+hashlib.shake_256(b'paket-dolgu').digest(boy-len(kaynak))
    sha=hashlib.sha256(veri).hexdigest()
    (depo/'buyuk.elf').write_bytes(veri)
    (depo/'paketler.txt').write_bytes(f'buyuk 2 buyuk.elf {boy} {sha}\n'.encode())
    usb,nvme,sata=imajlar(w,{'paket.elf':(w/'paket.elf').read_bytes()})
    net=('-netdev','user,id=n0','-device','e1000,netdev=n0',
         '-object',f'filter-dump,id=f0,netdev=n0,file={w/"paket-buyuk.pcap"}')
    with depo_ac(depo) as (port,istek), boot(w,usb[0],nvme,sata,ag_aygitlari=net) as (q,p,seri):
        c=HttpKonsol(w,q,p,seri)
        wait_for(p,lambda:int.from_bytes(c.mem(c.sayi('ag_l5'),4),'little')==3,seconds=15)
        komut(c,'bagla --yazilabilir usb0p2')
        once=c.heap(),c.bos_cerceve()
        s=komut(c,f'paket guncelle http://10.0.2.2:{port}/paketler.txt',45)
        assert b'paket bildirimi guncellendi' in s,s
        s=komut(c,'paket kur buyuk',180)
        assert b'paket kuruldu buyuk' in s,s
        assert b'42\n' in komut(c,'buyuk 84/2')
        son=c.heap(),c.bos_cerceve()
        assert once[1]==son[1],(once,son)
        c.expected=c.output()
        c.shutdown()
    assert istek==['/paketler.txt','/buyuk.elf'],istek
    fsck(w,usb[1],'paket-buyuk')
    assert fat_oku(Fat(usb[1].read()),'bin/buyuk.elf')==veri
    print('✓ Büyük paket',boy,sha,'heap/boş çerçeve',once,son,flush=True)


if __name__=='__main__':
    kos(Path(sys.argv[1]).resolve())
