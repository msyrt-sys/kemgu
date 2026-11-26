"""Gerçek depo ELF'ini, kurulu listesini ve geçici dosyaları FAT32'de denetler."""
from contextlib import contextmanager
from functools import partial
import hashlib
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import shutil
import sys
import threading

from ag_arac_kahin import komut, MAC
from ag_http_kahin import HttpKonsol
from dalga12_fat import Fat
from dalga12_kahin import boot, fsck, wait_for
from pcap_oku import pcap_kayitlari
from surec_imaj import imajlar


@contextmanager
def depo_ac(dizin):
    istekler = []

    class Sunucu(SimpleHTTPRequestHandler):
        def log_message(self,*args):
            pass

        def do_GET(self):
            istekler.append(self.path)
            super().do_GET()

    with ThreadingHTTPServer(('127.0.0.1',0), partial(Sunucu,directory=str(dizin))) as s:
        t = threading.Thread(target=s.serve_forever)
        t.start()
        try:
            yield s.server_port, istekler
        finally:
            s.shutdown()
            t.join()


def fat_oku(fat,yol):
    k = fat.root
    for ad in yol.split('/'):
        girdiler = {s.lower():v for s,v in fat.directory(k).items()}
        k,n,d = girdiler[ad]
    return {a.lower() for a in fat.directory(k)} if d else fat.chain(k)[:n] if n else b''


def kos(w):
    depo = w/'depo'
    shutil.copytree('build/paket_deposu',depo,dirs_exist_ok=True)
    hesapla = (depo/'hesapla.elf').read_bytes()
    sha = hashlib.sha256(hesapla).hexdigest()
    # Yalnız veri kopyasının son baytı değiştirilir. Konuk bu paketi çalıştırmaz.
    bozuk = bytearray(hesapla)
    bozuk[-1] ^= 1
    (depo/'bozuk.elf').write_bytes(bozuk)
    with (depo/'paketler.txt').open('ab') as f:
        f.write(f'bozuk 1 bozuk.elf {len(bozuk)} {sha}\n'.encode())
    usb,nvme,sata = imajlar(w,{'paket.elf':(w/'paket.elf').read_bytes()})
    tel = w/'paket.pcap'
    def aygit(pcap):
        return ('-netdev','user,id=n0','-device',f'e1000,netdev=n0,mac={MAC.hex(":")}',
                '-object',f'filter-dump,id=f0,netdev=n0,file={pcap}')

    def ac(q,p,seri):
        c = HttpKonsol(w,q,p,seri)
        wait_for(p,lambda:int.from_bytes(c.mem(c.sayi('ag_l5'),4),'little')==3,seconds=15)
        komut(c,'bagla --yazilabilir usb0p2')
        return c,(c.heap(),c.bos_cerceve())

    def kapat(c,once):
        son = c.heap(),c.bos_cerceve()
        assert once[1]==son[1],(once,son)
        print('Paket heap/boş çerçeve',once,son,flush=True)
        c.expected=c.output()
        c.shutdown()

    devam = w/'paket-devam.pcap'
    with depo_ac(depo) as (port,istekler):
        with boot(w,usb[0],nvme,sata,ag_aygitlari=aygit(tel)) as (q,p,seri):
            c,once = ac(q,p,seri)
            s=komut(c,f'paket guncelle http://10.0.2.2:{port}',60)
            assert b'paket bildirimi guncellendi' in s,s
            assert b'hesapla 1\n' in komut(c,'paket liste')
            s=komut(c,'paket kur hesapla',70)
            assert b'paket kuruldu hesapla' in s,s
            assert b'42\n' in komut(c,'hesapla 44-2')
            kapat(c,once)
        fsck(w,usb[1],'paket-kuruldu')
        fat=Fat(usb[1].read())
        assert hashlib.sha256(fat_oku(fat,'bin/hesapla.elf')).hexdigest()==sha
        assert fat_oku(fat,'paket/kurulu.txt')==f'hesapla 1 hesapla.elf {len(hesapla)} {sha}\n'.encode()
        with boot(w,usb[0],nvme,sata,ag_aygitlari=aygit(devam)) as (q,p,seri):
            c,once=ac(q,p,seri)
            s=komut(c,'paket bilgi hesapla')
            assert sha.encode() in s and b'kurulu 1\n' in s,s
            s=komut(c,'paket kur bozuk',70)
            assert b'paket SHA256 uyusmuyor' in s and b'paket hata -74' in s,s
            komut(c,'sync')
            fat=Fat(usb[1].read())
            assert 'bozuk.elf' not in fat_oku(fat,'bin')
            assert 'yuk.tmp' not in fat_oku(fat,'paket'), 'bozuk paketin gecici dosyasi kaldi'
            assert b'paket kaldirildi hesapla' in komut(c,'paket kaldir hesapla')
            ilk=seri.stat().st_size
            c.input('hesapla 44-2')
            wait_for(p,lambda:b'HATA bilinmeyen komut' in seri.read_bytes()[ilk:],seconds=5)
            kapat(c,once)
    fat = Fat(usb[1].read())
    assert fat_oku(fat,'paket/kurulu.txt') == b''
    assert fat_oku(fat,'paket')-{'depo.txt','kurulu.txt','.','..'} == set()
    assert 'hesapla.elf' not in fat_oku(fat,'bin')
    fsck(w,usb[1],'paket')
    assert istekler == ['/paketler.txt','/hesapla.elf','/bozuk.elf'],istekler
    veri=bytearray()
    for yol in (tel,devam):
        with yol.open('rb') as f:
            for p in pcap_kayitlari(f):
                if p.protokol==6 and p.hedef_port==port:
                    veri.extend(p.yuk[4*(p.yuk[12]>>4):])
    assert all(f'GET {y} HTTP/1.1'.encode() in veri for y in istekler)
    print('✓ Paket HTTP + FAT32 SHA256',sha,'çalıştırma 42, bozuk paket ve geçici dosya yok, fsck temiz',flush=True)


if __name__=='__main__':
    kos(Path(sys.argv[1]).resolve())
