"""Aynı UNIX Ethernet hattındaki iki gerçek KEMGU makinesinin uçtan uca kâhini."""
from contextlib import contextmanager
import hashlib
from pathlib import Path
import shutil
import sys
from calisma_dizini import calisma_dizini

from ag_arac_kahin import komut, pcap_akim
from ag_arm_kahin import TCP, arm_ac
from ag_http_kahin import HttpKonsol
from ag_paket_kahin import fat_oku
from boru_kahin import BoruConsole
from dalga12_fat import Fat
from dalga12_kahin import boot, fsck, wait_for
from pcap_oku import pcap_kayitlari
from surec_imaj import imajlar


class AgKonsol(HttpKonsol):
    def input(self,text):
        if any(k in text for k in '<>|'):
            BoruConsole.input(self,text)
        else:
            super().input(text)


def hazir(kok,ad):
    w=kok/ad
    w.mkdir(exist_ok=True,parents=True)
    shutil.copy2(kok/'k.elf',w/'k.elf')
    shutil.copytree(kok/'esp',w/'esp',dirs_exist_ok=True)
    return w


def hat(yol,sunucu):
    return f'stream,id=n0,server={"on" if sunucu else "off"},addr.type=unix,addr.path={yol}'


@contextmanager
def pc(w,imaj,net,mac):
    usb,nvme,sata=imaj
    args=('-netdev',net,'-device',f'e1000,netdev=n0,mac={mac}',
          '-object',f'filter-dump,id=tel,netdev=n0,file={w}/ag.pcap')
    with boot(w,usb[0],nvme,sata,ag_aygitlari=args) as (q,p,s):
        c=AgKonsol(w,q,p,s)
        yield c
        c.expected=c.output()
        c.shutdown()


def arm(kok,imaj):
    w=hazir(kok,'es-x86')
    disk=imajlar(w,{})
    a=kok/'es-arm'
    with calisma_dizini(prefix='kem-ag-') as tmp:
        yol=Path(tmp)/'ether'
        with pc(w,disk,hat(yol,True),'52:54:00:17:19:01') as c:
            c.run('ag ayarla 192.0.2.1/24 192.0.2.2',b'AG AYAR OK\n')
            c.run('ag tcp dinle',b'TCP dinle 9001\n')
            once=c.heap(),c.bos_cerceve()
            with arm_ac(a,imaj,hat(yol,False),'192.0.2.1','192.0.2.2',9001,0) as (_,p,s):
                wait_for(p,lambda:s.exists() and b'KEMGU KEM-OS OK' in s.read_bytes(),seconds=100)
                seri=s.read_bytes()
                assert all(k in seri for k in (b'[9] NET ARP OK',b'[10] PING CANLI',b'[43] AG TCP 1048576 OK')),seri[-600:]
            assert (c.heap(),c.bos_cerceve())==once
    with (a/'arm-disk.img').open('rb') as f:
        f.seek(256*512)
        assert f.read(len(TCP))==TCP
    with (w/'ag.pcap').open('rb') as f:
        paketler=list(pcap_kayitlari(f))
    for giden in (False,True):
        assert pcap_akim(paketler,9001,giden,bytes.fromhex('525400170017'))==TCP
    print('AG19 x86 ARM iki yon 1 MiB SHA256',hashlib.sha256(TCP).hexdigest(),'heap/cerceve',once,flush=True)


def isaret(c,konum,metin,sure=25):
    wait_for(c.p,lambda:metin in c.ser.read_bytes()[konum:],seconds=sure)


def x86(kok,arp_kapali=False):
    a=hazir(kok,'arp-a' if arp_kapali else 'pc-a')
    b=hazir(kok,'arp-b' if arp_kapali else 'pc-b')
    if arp_kapali:
        shutil.copy2(kok/'arp-kapali/k.elf',a/'k.elf')
    veri=bytes((i*17+i//37)&255 for i in range(4099))
    depo=Path('build/paket_deposu')
    paket=(depo/'hesapla.elf').read_bytes()
    bildirim=f'hesapla 1 hesapla.elf {len(paket)} {hashlib.sha256(paket).hexdigest()}\n'.encode()
    sol={'sunucu.elf':(kok/'sunucu.elf').read_bytes(),'alicix.elf':(kok/'alicix.elf').read_bytes(),
         'nc.elf':(kok/'nc.elf').read_bytes(),'hesapla.elf':paket,'paketler.txt':bildirim,'iz.bin':veri}
    sag={ad+'.elf':(kok/(ad+'.elf')).read_bytes() for ad in ('indir','nc','paket','tftp','ping')}
    sol_disk=imajlar(a,{} if arp_kapali else sol)
    sag_disk=imajlar(b,sag)
    with calisma_dizini(prefix='kem-iki-') as tmp:
        yol=Path(tmp)/'ether'
        with pc(a,sol_disk,hat(yol,True),'52:54:00:17:19:01') as c,pc(b,sag_disk,hat(yol,False),'52:54:00:17:19:02') as d:
            c.run('ag ayarla 192.0.2.1/24 192.0.2.2',b'AG AYAR OK\n')
            d.run('ag ayarla 192.0.2.2/24 192.0.2.1',b'AG AYAR OK\n')
            if arp_kapali:
                i=d.ser.stat().st_size
                d.input('ping 192.0.2.1 1 0 500')
                isaret(d,i,b'KABUK_HAZIR\n')
                yan=d.ser.read_bytes()[i:]
                assert b'PING alinan 0 / 1' in yan and b'-110' in yan,yan
                assert b'192.0.2.2' in komut(d,'ag durum')
                print('AG19 ARP zaman asimi ve kabuk hazir',yan,flush=True)
            else:
                once=[(k.heap(),k.bos_cerceve()) for k in (c,d)]
                for k in (c,d):
                    komut(k,'bagla --yazilabilir usb0p2')
                    komut(k,'cd /diskler/usb0p2/bin')
                i=c.ser.stat().st_size
                c.input('sunucu 9119 /diskler/usb0p2/bin')
                isaret(c,i,b'HTTP dinliyor 9119')
                url='http://192.0.2.1:9119'
                assert b'indir tamam 4099' in komut(d,'indir '+url+'/iz.bin iz.bin')
                assert b'paket bildirimi guncellendi' in komut(d,'paket guncelle '+url)
                assert b'paket kuruldu hesapla' in komut(d,'paket kur hesapla')
                assert b'42\n' in komut(d,'hesapla 44-2')
                for bas in [True,False]:
                    if bas: c.key('ctrl',True)
                    c.key('c',bas)
                    if not bas: c.key('ctrl',False)
                isaret(c,i,b'KABUK_HAZIR\n')
                i=c.ser.stat().st_size
                c.input('nc -d -z 10000 9121 > gelen.bin')
                assert b'nc:' not in komut(d,'nc -z 10000 192.0.2.1 9121 < iz.bin')
                isaret(c,i,b'KABUK_HAZIR\n')
                assert b'nc:' not in c.ser.read_bytes()[i:]
                i=c.ser.stat().st_size
                c.input('alicix')
                isaret(c,i,b'TFTP alici hazir')
                assert b'TFTP tamam 4099' in komut(d,'tftp gonder 192.0.2.1 iz.bin iz.bin')
                isaret(c,i,b'TFTP alici sonuc 4099')
                isaret(c,i,b'KABUK_HAZIR\n')
                for k in (c,d): komut(k,'sync')
                son=[(k.heap(),k.bos_cerceve()) for k in (c,d)]
                assert [v[1] for v in once]==[v[1] for v in son]
    if arp_kapali:
        with (b/'ag.pcap').open('rb') as f: pk=list(pcap_kayitlari(f))
        assert any(x.tur==2054 and x.yuk[6:8]==b'\0\1' and x.yuk[24:28]==bytes((192,0,2,1)) for x in pk)
        assert not any(x.tur==2054 and x.yuk[6:8]==b'\0\2' and x.yuk[14:18]==bytes((192,0,2,1)) for x in pk)
        return
    for w,disk in ((a,sol_disk),(b,sag_disk)): fsck(w,disk[0][1],'iki-konuk')
    sol_fat=Fat(sol_disk[0][1].read())
    sag_fat=Fat(sag_disk[0][1].read())
    assert all(fat_oku(sol_fat,'bin/'+isim)==veri for isim in ('gelen.bin','tftp.bin'))
    assert fat_oku(sag_fat,'bin/iz.bin')==veri and fat_oku(sag_fat,'bin/hesapla.elf')==paket
    print('AG19 x86 x86 HTTP nc TFTP ve konuk paket deposu SHA256',hashlib.sha256(veri).hexdigest(),once,son,flush=True)


if __name__=='__main__':
    kok=Path(sys.argv[1]).resolve()
    if sys.argv[2]=='x86': x86(kok)
    elif sys.argv[2]=='arp': x86(kok,True)
    else: arm(kok,Path(sys.argv[2]).resolve())
