"""UEFI haritası, FAT kopyaları, GOP pikselleri ve fiziksel klavye ile araçları sınar."""
import argparse
import re
import struct
from pathlib import Path

from boru_kahin import BoruConsole
from dalga12_fat import Fat
from dalga12_kahin import boot, fsck
from duzenle_kahin import ARKA, du_ekran, ge_ekran, piksel, tamam, tus, yaz
from surec_imaj import imajlar

KOK = "/diskler/usb0p2"
VERI = bytes(range(251)) * 5 + b'kismi son'


def cerceveler(c):
    assert c.q.call('query-memory-size-summary')['base-memory'] == 256 << 20
    adres = int(re.search(rb'^BILGI_ADRES (\d+)', c.ser.read_bytes(), re.M)[1])
    a = struct.unpack('<24Q', c.mem(adres, 192))
    ham = c.mem(a[9], a[10])
    sayfalar = set()
    for i in range(0, len(ham), a[11]):
        tur, _, bas, _, n, bayrak = struct.unpack_from('<IIQQQQ', ham, i)
        if tur in (3, 4, 7) and not bayrak >> 63:
            sayfalar.update(range(max(256, bas//4096), min(1 << 20, bas//4096+n)))
    for lo, hi in ((a[14], a[15]), (adres, adres+4096), (a[9], a[9]+a[16]),
                   (a[3], a[3]+a[4]), (a[13], a[13]+4096), (a[20], a[21])):
        sayfalar.difference_update(range(lo//4096, (hi+4095)//4096))
    # Çekirdek ek tablolarını CR3 ağacında fiziksel adreslerinden say.
    kayit = c.q.call('human-monitor-command', {'command-line':'info registers'})
    cr3 = int(re.search(r'CR3=([0-9a-fA-F]+)', kayit)[1], 16)
    tablolar = set()
    def gez(p, seviye):
        if p in tablolar:
            return
        tablolar.add(p)
        if seviye == 1:
            return
        for g in struct.unpack('<512Q', c.mem(p, 4096)):
            if g & 1 and not g & 128:
                gez(g & 0x000ffffffffff000, seviye-1)
    gez(cr3 & ~4095, 4)
    tablo_sayfalari = sayfalar & {p//4096 for p in tablolar}
    assert tablo_sayfalari
    # Ağın sabit tamponlarını QMP'den okunan fiziksel tabanlarından say.
    ag_sayfalari = set()
    for ad, adet in (('ag_l3_alan', 7), ('ag_l5', 3), ('ag_parca_alan', 77),
                     ('ag_tcp_alan', 2060), ('sk_alan', 208)):
        taban, = struct.unpack('<Q', c.mem(c.symbols[ad], 8))
        assert taban and taban % 4096 == 0, (ad, taban)
        alan = set(range(taban//4096, taban//4096 + adet))
        assert alan <= sayfalar and alan.isdisjoint(tablo_sayfalari | ag_sayfalari), ad
        ag_sayfalari.update(alan)
    return len(sayfalar - tablo_sayfalari - ag_sayfalari)-16-8-8207-32-8, len(sayfalar)


def bos_bayt(part):
    fs = Fat(part.read())
    bos = sum((x[0] & 0x0fffffff) == 0 for x in struct.iter_unpack('<I', fs.fat[8:4*(fs.count+2)]))
    return bos*fs.spc*512


def bilgi(c, usb, nvme, sata):
    bos, toplam = cerceveler(c)
    diskler = {KOK:bos_bayt(usb[1]), '/diskler/usb0p1':bos_bayt(usb[0]),
               '/diskler/nvme0p1':bos_bayt(nvme), '/diskler/sata0p1':bos_bayt(sata), '/ram':32768}
    once = c.heap()
    def dogrula(ham):
        metin = ham.decode()
        gercek = tuple(map(int,re.search(r'^CERCEVE bos toplam (\d+) (\d+)$',metin,re.M).groups()))
        assert gercek == (bos,toplam), ('CERCEVE', gercek, (bos,toplam))
        assert dict((ad,int(n)) for ad,n in re.findall(r'^BOS_BAYT (\S+) (\d+)$',metin,re.M)) == diskler, metin
        assert 'GIRDI USB PS2 tr\n' in metin and 'SUREC etkin 0\n' in metin
        assert int(re.search(r'CALISMA ms (\d+)',metin)[1]) > 0
        assert once <= int(re.search(r'HEAP bayt (\d+)',metin)[1]) <= c.heap()
        assert any(int(n)>0 for n in re.findall(r'^KESME \d+ \d+ (\d+)$',metin,re.M))
    c.run('bilgi', dogrula)
    print('BILGI CERCEVE OK',bos,toplam,diskler,flush=True)


def baslat(c, ad, yol):
    cmd = f'calistir {KOK}/bin/{ad}.elf {yol}'
    c.input(cmd)
    c.akis += '> '+cmd+'\n'


def editor(c, part):
    c.run('bagla --yazilabilir usb0p2')
    yol = KOK+'/notlar/ara.txt'
    baslat(c,'duzenle',yol)
    du_ekran(c,yol,[],0,0)
    yaz(c,'ab')
    tus(c,'z',ctrl=True)
    du_ekran(c,yol,['a'],0,1,True)
    tus(c,'y',ctrl=True)
    du_ekran(c,yol,['ab'],0,2,True)
    yaz(c,'\nAğaç')
    tus(c,'g',ctrl=True); yaz(c,'1\n')
    du_ekran(c,yol,['ab','Ağaç'],0,0,True)
    tus(c,'i',ctrl=True)
    tus(c,'f',ctrl=True); yaz(c,'ağaç\n')
    du_ekran(c,yol,['ab','Ağaç'],1,0,True,'  Bulundu')
    tus(c,'b',ctrl=True); yaz(c,'ab\n')
    du_ekran(c,yol,['ab','Ağaç'],0,0,True,'  Bulundu')
    tus(c,'h',ctrl=True); yaz(c,'ağaç\nx\n2\n'); tus(c,'e')
    du_ekran(c,yol,['ab','x'],1,1,True,'  Değiştirilen 1')
    tus(c,'s',ctrl=True); du_ekran(c,yol,['ab','x'],1,1,False,'  Kaydedildi')
    tus(c,'q',ctrl=True); tamam(c,'duzenle',1)
    c.shutdown()
    assert Fat(part.read()).get('/NOTLAR/ara.txt') == b'ab\nx'
    fsck(c.w,part,'arac16-duzenle')


def gezgin(c, part):
    c.run('bagla --yazilabilir usb0p2')
    c.run('kopyala '+KOK+'/bin/parca.bin /ram/parca')
    baslat(c,'dosyalar','/ram')
    satir = ['../', f'parca  {len(VERI)}']
    ge_ekran(c,'/ram',satir,0)
    tus(c,'down'); tus(c,'f5'); yaz(c,KOK+'/kopya.bin\n')
    # Kopya içeriğini gerçek imajdan okurken konuk diski sync ile boşaltır.
    tus(c,'q'); tamam(c,'dosyalar',1)
    c.run('sync')
    c.q.call('stop')
    try:
        yazilan = Fat(part.read()).get('/kopya.bin')
        assert yazilan == VERI, ('kısmi küme',len(yazilan),len(VERI))
    finally:
        c.q.call('cont')
    baslat(c,'dosyalar','/ram')
    ge_ekran(c,'/ram',satir,0)
    tus(c,'down'); tus(c,'f6'); yaz(c,KOK+'/tasinan.bin\n')
    ge_ekran(c,'/ram',['../'],0)
    tus(c,'f7'); yaz(c,'klasor\n'); ge_ekran(c,'/ram',['../','klasor/'],0)
    tus(c,'down'); tus(c,'delete'); tus(c,'h'); ge_ekran(c,'/ram',['../','klasor/'],1)
    tus(c,'delete'); tus(c,'e'); ge_ekran(c,'/ram',['../'],0)
    tus(c,'f7'); yaz(c,'/diskler/usb0p1/yeni\n')
    piksel(c,[(0,0,'/ram',0xffffff,ARKA),(1,0,'../',0,0xffffff),(-1,0,'Salt okunur',0xffffff,ARKA)])
    tus(c,'q'); tamam(c,'dosyalar',2)
    c.run('tasi '+KOK+'/tasinan.bin /ram/geri')
    c.run('ls /ram',b'geri\n'); c.run('sil /ram/geri'); c.run('ls /ram')
    c.shutdown()
    fs = Fat(part.read())
    assert fs.get('/kopya.bin') == VERI
    assert 'tasinan.bin' not in {s.lower() for s in fs.directory(fs.root)}
    fsck(c.w,part,'arac16-gezgin')


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('dizin',type=Path)
    p.add_argument('--senaryo',choices=('bilgi','editor','gezgin'))
    a = p.parse_args()
    w = a.dizin.resolve()
    elfs = {f.name:f.read_bytes() for f in (w/'kullanici').glob('*.elf')}
    elfs['parca.bin'] = VERI
    for ad in ([a.senaryo] if a.senaryo else ('bilgi','editor','gezgin')):
        usb,nvme,sata = imajlar(w,elfs)
        if ad == 'bilgi':
            # FAT32'de 0xffffffff geçerli "sayı bilinmiyor" değeridir.
            # Üç taşıyıcı içerikten sayılmalı, ESP'de gerçek FSInfo sayacı kalır.
            for part in (usb[1], nvme, sata):
                with part.disk.open('r+b') as f:
                    for sector in (1, 7):
                        f.seek((part.start+sector)*512+488)
                        f.write(struct.pack('<I',0xffffffff))
        with boot(w,usb[0],nvme,sata) as (q,proc,seri):
            c = BoruConsole(w,q,proc,seri)
            if ad == 'bilgi':
                bilgi(c,usb,nvme,sata)
                c.shutdown()
            elif ad == 'editor':
                editor(c,usb[1])
            else:
                gezgin(c,usb[1])
        print('ARAC16 OK',ad,flush=True)
