"""Dört kullanıcı ELF'inin seri çıktısı ve QEMU'nun gerçek ekranı."""
import argparse
import os
import time
from bekle import bekle
from pathlib import Path
import struct
from datetime import datetime, timedelta

from dalga12_kahin import boot, RTC, wait_for
from duzenle_kahin import tus, tamam
from psf_kahin import screendump, font_yukle, glif_ciz
from surec_imaj import imajlar
from surec_kahin import SurecConsole
from uefi_kabuk_kahin import keys


class ProgramConsole(SurecConsole):
    def input(self, text):
        ozel = {'*':('minus',False),'+':('4',True),'(':('8',True),')':('9',True),'-':('equal',False)}
        for c in text+'\n':
            k,s = ozel.get(c, keys(c))
            tus(self,k,shift=s)


def bmp():
    w,h = 1440,2
    row = b''.join(bytes((x%251, 100, y)) for y in (0,160) for x in range(w))
    return struct.pack('<2sIHHI',b'BM',54+len(row),0,0,54)+struct.pack(
        '<IiiHHIIiiII',40,w,-h,1,24,0,len(row),0,0,0,0)+row


def goster(c):
    c.input('baslat goster /diskler/usb0p2/bin/test.bmp')
    def ekran(sigdir,bas):
        def bak():
            w,h,px = screendump(c.q.f,c.w/'bmp.ppm',c.q.response)
            bek = bytearray(w*h*3)
            gen = min(1440-bas,w)
            yuk = max(1,2*gen//1440) if sigdir else 2
            for y in range(yuk):
                for x in range(gen):
                    pos = (y*w+x)*3
                    bek[pos:pos+3] = bytes((y*160,100,(bas+(x*1440//gen if sigdir else x))%251))
            return px == bek
        wait_for(c.p,bak,12)
    ekran(True,0)
    tus(c,'right')
    ekran(False,16)
    tus(c,'q')
    tamam(c,'goster',1)


def rtc_yolu(q):
    # Q35'in aygıt numarası topolojiye bağlıdır; QOM türünden gerçek nesneyi bul.
    yollar = ['/machine']
    while yollar:
        ust = yollar.pop()
        for nesne in q.call('qom-list', {'path': ust}):
            tur = nesne['type']
            if not tur.startswith('child<'):
                continue
            yol = ust + '/' + nesne['name']
            if tur == 'child<mc146818rtc>':
                return yol
            yollar.append(yol)
    raise AssertionError('QOM RTC aygıtı bulunamadı')


def saat(c):
    c.input('baslat saat')
    font = font_yukle()
    desen = {}
    for ch in '0123456789:':
        pic = [0]*128
        glif_ciz(pic,8,16,font,0,0,ord(ch),0xffffff,0x102040)
        desen[b''.join(n.to_bytes(3,'big') for n in pic)] = ch
    def oku():
        w,h,px = screendump(c.q.f,c.w/'saat.ppm',c.q.response)
        x,y = ((w//8-8)//2)*8,(h//16//2)*16
        metin = ''
        for i in range(8):
            k = b''.join(px[3*((y+j)*w+x+i*8):3*((y+j)*w+x+i*8+8)] for j in range(16))
            metin += desen.get(k,'?')
        if '?' in metin:
            return None
        t = datetime.combine(RTC.date(),datetime.strptime(metin,'%H:%M:%S').time())
        assert RTC <= t < RTC+timedelta(minutes=3)
        return t
    yol = rtc_yolu(c.q)
    def donanim():
        t = c.q.call('qom-get', {'path': yol, 'property':'date'})
        return datetime(t['tm_year']+1900,t['tm_mon']+1,t['tm_mday'],t['tm_hour'],t['tm_min'],t['tm_sec'])
    def ornek():
        if b'HATA RTC\n' in c.output():
            raise AssertionError('RTC güncellemesi konukta durdu: HATA RTC')
        return donanim(), oku(), donanim()
    def uyumlu(o):
        once, ekran, sonra = o
        return ekran is not None and once-timedelta(seconds=1) <= ekran <= sonra
    bas = time.monotonic()
    ilk = bekle('RTC ilk ekran ve donanım okuması', ornek, uyumlu, sure=15, surec=c.p)
    son = bekle('RTC donanım ve ekran birlikte ilerlemeli', ornek,
                lambda o: uyumlu(o) and o[1] > ilk[1] and o[2] > ilk[2], sure=15, surec=c.p)
    print('RTC ilerledi', ilk[1].time(), son[1].time(), 'QOM', ilk[2].time(), son[2].time(),
          'host_sn', round(time.monotonic()-bas, 3), flush=True)
    tus(c,'a')
    tamam(c,'saat',1)


def kos(w, ad):
    programlar = {ad+'.elf': (w/'kullanici'/(ad+'.elf')).read_bytes(), 'test.bmp':bmp(),
                  'a.txt':'bir\nAĞAÇ\nson\n'.encode(), 'b.txt':'ağaç var'.encode()}
    usb,nvme,sata = imajlar(w,programlar)
    with boot(w,usb[0],nvme,sata,klavye='ps2') as (q,p,s):
        c = ProgramConsole(w,q,p,s)
        if ad == 'hesapla':
            c.run('baslat hesapla (2+3)*4',b'20\n[hesapla.elf pid=1 cikis=0]\n')
            c.run('baslat hesapla 1/0',b'HATA sifira bolme\n[hesapla.elf pid=2 cikis=2]\n')
            c.screen()
        elif ad == 'bul':
            c.run('cd /diskler/usb0p2/bin')
            c.run('baslat bul -i ağaç a.txt b.txt','a.txt:2:AĞAÇ\nb.txt:1:ağaç var\n[bul.elf pid=1 cikis=0]\n'.encode())
            c.screen()
        elif ad == 'goster':
            goster(c)
        else:
            saat(c)
        c.shutdown()
    print('PROGRAM15 OK',ad,flush=True)


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('dizin',type=Path)
    p.add_argument('--program',default=os.environ.get('PROGRAM15_TEK'),choices=('hesapla','bul','goster','saat'))
    a = p.parse_args()
    for ad in ([a.program] if a.program else ('hesapla','bul','goster','saat')):
        kos(a.dizin.resolve(),ad)
