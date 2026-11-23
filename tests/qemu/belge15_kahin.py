"""Kitaptaki KEMGU kodunu çıkarır, çalıştırır; yardım tablosunu konukla karşılaştırır."""
import argparse
import html
import os
import re
import subprocess
from pathlib import Path

from dalga12_kahin import boot
from duzenle_kahin import piksel, tamam, tus
from surec_imaj import GUNLUK, imajlar
from surec_kahin import SurecConsole


def kos(w):
    metin=Path('belgeler/KEMGU_OS_Program_Yazma.md').read_text(encoding='utf-8')
    ornekler=re.findall(r'<!-- program: (\w+) -->\s*```kem\n(.*?)\n```',metin,re.S)
    assert len(ornekler)==metin.count('```kem\n')==2
    programlar={}
    for ad,kaynak in ornekler:
        p=w/(ad+'.kem')
        p.write_text(kaynak+'\n',encoding='utf-8')
        elf=w/(ad+'.elf')
        subprocess.run(['python3','tests/qemu/surec_elf.py',elf,'--kaynak',p,'--derleyici',
                        os.environ['SUREC_KAHIN_DERLEYICI']],check=True)
        programlar[elf.name]=elf.read_bytes()
    usb,nvme,sata=imajlar(w,programlar)
    with boot(w,usb[0],nvme,sata,klavye='ps2') as (q,p,s):
        c=SurecConsole(w,q,p,s)
        c.run('baslat okuornek /diskler/usb0p2/notlar/günlük.txt',GUNLUK+b'[okuornek.elf pid=1 cikis=0]\n')
        c.input('baslat olayornek')
        piksel(c,[(2,1,'Tuşa bas',65280,0x102040)])
        tus(c,'a')
        tamam(c,'olayornek',2)
        tablo=Path('belgeler/KEMGU_OS_Mimari.md').read_text(encoding='utf-8').split('## Kabuk komutları')[1]
        yardim=re.findall(r'^\| `(\w+)` \| ([^|]+) \|$',tablo,re.M)
        assert len(yardim)>=30 and len(dict(yardim))==len(yardim)
        for ad,acik in yardim:
            c.run('yardim '+ad,(html.unescape(acik)+'\n').encode())
        c.run('yardim bilinmeyen',b'HATA yardim konusu bulunamadi\n')
        c.shutdown()
    print(f'BELGE15 OK {len(ornekler)} kaynak, {len(yardim)} komut',flush=True)


if __name__ == '__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('dizin',type=Path)
    kos(p.parse_args().dizin.resolve())
