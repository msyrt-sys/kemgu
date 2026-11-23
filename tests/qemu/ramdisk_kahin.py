"""Konukta çerçeveye yüklenen bölüm imajını ve geçici VFS'yi host baytlarıyla sınar."""
import argparse
import hashlib
import re
import struct
from pathlib import Path

from boru_kahin import BoruConsole
from dalga12_fat import Fat
from dalga12_imaj import DATA, ESP, gpt, images
from dalga12_kahin import Console, boot, sha
from fat_yazici import hazirla
from surec_imaj import imajlar
from uefi_imaj import hazirla_uefi


def ramdisk(w,tur):
    ic=w/'ramdisk.img'
    boy=34*2048
    hazirla(ic,tur=='gpt',toplam=boy)
    if tur=='mbr':
        veri=ic.read_bytes()
        mbr=bytearray(2048*512)
        struct.pack_into('<I',mbr,440,0xad160006)
        mbr[450]=0x0c
        struct.pack_into('<II',mbr,454,2048,boy)
        mbr[510:512]=bytes.fromhex('55aa')
        ic.write_bytes(mbr+veri)
    hedef=ic.read_bytes()
    usb,nvme,sata=images(w,'gpt')
    esp=w/'esp-ram.img'
    hazirla_uefi(esp,w/'esp/EFI/BOOT/BOOTX64.EFI',w/'k.elf',ic)
    usb=gpt(w/'ram-kaynak.img',[(esp,2048,ESP),(w/'dalga12-fs.fat',0,DATA)])
    once=sha(usb[0].disk)
    aygit='nvme' if tur=='gpt' else 'usb'
    with boot(w,usb[0],nvme,sata,acilis=aygit,ram='512M') as (q,p,ser):
        c=Console(w,q,p,ser,disk='NVMe' if aygit=='nvme' else 'USB')
        m=re.search(rb'^RAMDISK fiziksel bayt (\d+) (\d+)$',ser.read_bytes(),re.M)
        assert m,ser.read_bytes()[-1800:]
        adres,n=map(int,m.groups())
        assert adres%4096==0 and n==len(hedef)
        oz=hashlib.sha256(c.mem(adres,n)).hexdigest()
        assert oz==hashlib.sha256(hedef).hexdigest()
        kaynak=Fat(hedef[2048*512:(2048+boy)*512])
        yol='/diskler/ram0p1'
        c.run('cat '+yol+'/merhaba.txt',kaynak.get('/merhaba.txt').rstrip(b'\n')+b'\n')
        c.run('bagla --yazilabilir ram0p1')
        c.run('yaz '+yol+'/ucucu.txt bellek')
        c.run('sync')
        c.run('cat '+yol+'/ucucu.txt',b'bellek\n')
        degisen=Fat(c.mem(adres+2048*512,boy*512))
        assert degisen.get('/ucucu.txt')==b'bellek'
        c.shutdown()
    assert sha(usb[0].disk)==once
    assert Fat(usb[0].read()).get('/RAMDISK.IMG')==hedef
    print('RAMDISK OK',tur,aygit,adres,n,oz,flush=True)


def gecici(w,kisa):
    dosyalar={p.name:p.read_bytes() for p in (w/'kullanici').glob('*.elf')}
    dosyalar['sinir.txt']=b'a'*33001
    usb,nvme,sata=imajlar(w,dosyalar)
    with boot(w,usb[0],nvme,sata) as (q,p,ser):
        c=BoruConsole(w,q,p,ser)
        assert b'ramdisk yok\n' in ser.read_bytes()
        c.run('ls /ram')
        c.run('mkdir /ram/gecici')
        c.run('yaz /ram/gecici/ilk.txt kemgu')
        c.run('mv /ram/gecici/ilk.txt /ram/gecici/metin.txt')
        c.run('ls /ram/gecici',b'metin.txt\n')
        if not kisa:
            c.run('cp /diskler/usb0p2/bin/merhaba.elf /ram/merhaba.elf')
            c.run('calistir /ram/merhaba.elf',b'/ram/merhaba.elf\n[merhaba.elf pid=1 cikis=1]\n')
            c.run('sil /ram/merhaba.elf')
            c.run('kedi < /ram/gecici/metin.txt',b'kemgu')
            c.run('kedi < /ram/gecici/metin.txt | bul kemgu > /ram/bul.txt')
            c.run('cat /ram/bul.txt',b'1:kemgu\n')
            c.run('cp /diskler/usb0p2/bin/sinir.txt /ram/dolu',b'HATA /ram cerceve siniri\n')
        c.run('sil /ram/gecici/metin.txt')
        c.run('ls /ram/gecici')
        c.run('rmdir /ram/gecici')
        c.run('yaz /ram/oturum.txt ilk')
        c.shutdown()
    with boot(w,usb[0],nvme,sata) as (q,p,ser):
        c=BoruConsole(w,q,p,ser)
        c.run('ls /ram')
        c.run('cat /ram/oturum.txt',b'HATA bulunamadi\n')
        c.shutdown()
    print('RAM_VFS OK',flush=True)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('dizin',type=Path)
    p.add_argument('--tur',choices=('mbr','gpt','gecici'))
    p.add_argument('--kisa',action='store_true')
    a=p.parse_args()
    for tur in ([a.tur] if a.tur else ('gpt','mbr','gecici')):
        if tur=='gecici': gecici(a.dizin.resolve(),a.kisa)
        else: ramdisk(a.dizin.resolve(),tur)
