"""Kitaptaki KEMGU kodunu çıkarır, çalıştırır; yardım tablosunu konukla karşılaştırır."""
from concurrent.futures import ThreadPoolExecutor
import argparse
import html
import os
import re
import subprocess
from pathlib import Path

from ag_es import EthernetEs, ES_IP, ES_MAC, KONUK_IP, ethernet, ipv4, udp
from dalga12_kahin import boot, wait_for
from duzenle_kahin import piksel, tamam, tus
from surec_imaj import GUNLUK, imajlar
from surec_kahin import SurecConsole


def kos(w):
    metin=Path('belgeler/KEMGU_OS_Program_Yazma.md').read_text(encoding='utf-8')
    ornekler=re.findall(r'<!-- program: (\w+) -->\s*```kem\n(.*?)\n```',metin,re.S)
    assert len(ornekler)==metin.count('```kem\n')==5
    programlar={}
    for ad,kaynak in ornekler:
        p=w/(ad+'.kem')
        p.write_text(kaynak+'\n',encoding='utf-8')
        elf=w/(ad+'.elf')
        subprocess.run(['python3','tests/qemu/surec_elf.py',elf,'--kaynak',p,'--derleyici',
                        os.environ['SUREC_KAHIN_DERLEYICI']],check=True)
        programlar[elf.name]=elf.read_bytes()
    usb,nvme,sata=imajlar(w,programlar)
    es=EthernetEs()
    mac=bytes.fromhex('525400170020')
    with es,boot(w,usb[0],nvme,sata,klavye='ps2',ag_aygitlari=(
            '-netdev',es.qemu_netdev(),'-device',f'e1000,netdev=n0,mac={mac.hex(":")}')) as (q,p,s):
        c=SurecConsole(w,q,p,s)
        c.run('baslat okuornek /diskler/usb0p2/notlar/günlük.txt',GUNLUK+b'[okuornek.elf pid=1 cikis=0]\n')
        c.input('baslat olayornek')
        piksel(c,[(2,1,'Tuşa bas',65280,0x102040)])
        tus(c,'a')
        tamam(c,'olayornek',2)
        c.run('baslat aktarornek /diskler/usb0p2/notlar/günlük.txt',b'[aktarornek.elf pid=3 cikis=0]\n')
        c.run('ls /ram',b'ornek.txt\n')
        c.run('cat /ram/ornek.txt',GUNLUK)
        c.run('ag ayarla 192.0.2.2/24 192.0.2.1',b'AG AYAR OK\n')
        c.run('baslat yankiornek 127.0.0.1',b'Yanki alindi\n[yankiornek.elf pid=4 cikis=0]\n')
        # İlk konuk çerçevesinden dönüş adresini öğren, ardından program gerçekten bekleyince gönder.
        es.al(3)
        for pid,port in [(5,9119),(6,9118)]:
            ilk=s.stat().st_size
            veri=f'port {port}\n'.encode()
            def gonder():
                wait_for(p,lambda:b'UDP 9118 9119 hazir\n' in s.read_bytes()[ilk:])
                es.gonder(ethernet(mac,ES_MAC,2048,ipv4(ES_IP,KONUK_IP,17,udp(ES_IP,KONUK_IP,20100,port,veri))))
            with ThreadPoolExecutor(1) as havuz:
                cevap=havuz.submit(gonder)
                c.run('baslat bekleornek',b'UDP 9118 9119 hazir\n'+veri+f'[bekleornek.elf pid={pid} cikis=0]\n'.encode())
                cevap.result()
        tablo=Path('belgeler/KEMGU_OS_Mimari.md').read_text(encoding='utf-8').split('## Kabuk komutları')[1]
        yardim=re.findall(r'^\| `(\w+)` \| ([^|]+) \|$',tablo,re.M)
        assert {'bilgi','kopyala','tasi','duzen','girdisay','kesme','pci'} <= dict(yardim).keys()
        assert len(dict(yardim))==len(yardim)
        def adlari_denetle(ham):
            adlar=ham.decode().split()
            assert len(set(adlar))==len(adlar) and set(adlar)==dict(yardim).keys()
        c.run('yardim',adlari_denetle)
        for ad,acik in yardim:
            c.run('yardim '+ad,(html.unescape(acik)+'\n').encode())
        c.run('yardim bilinmeyen',b'HATA yardim konusu bulunamadi\n')
        c.shutdown()
    print(f'BELGE15 OK {len(ornekler)} kaynak, {len(yardim)} komut',flush=True)


if __name__ == '__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('dizin',type=Path)
    kos(p.parse_args().dizin.resolve())
