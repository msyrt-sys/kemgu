"""USB 2 hub ağacını HMP, xHCI çıkış bağlamları, GOP ve FAT32 ile karşılaştırır."""
import argparse
import re
import struct
from pathlib import Path

from dalga12_fat import Fat
from dalga12_kahin import boot, fsck, wait_for
from duzenle_kahin import ge_ekran, ge_girdiler, tamam
from pci16_kahin import dugumler
from surec_imaj import imajlar
from surec_kahin import SurecConsole


def hmp_agaci(q):
    bilgi = q.call('human-monitor-command', {'command-line':'info usb'})
    siniflar = {'Hub':9,'Keyboard':3,'Mouse':3,'Tablet':3,'MSD':8}
    sonuc = {}
    for yol, urun in re.findall(r'Port ([\d.]+), Speed 12 Mb/s, Product QEMU USB (\w+)', bilgi):
        sonuc[tuple(map(int,yol.split('.')))] = siniflar[urun]
    assert len(sonuc) == bilgi.count('Device '),bilgi
    return sonuc


def envanter(c):
    rapor = []
    c.run('donanim',rapor.append)
    if not rapor[0].startswith(b'CPU '):
        ayna = c.mem(c.sayi('EKRAN_AYNA_TABAN'), 160).split(b'\n', 1)[0]
        raise AssertionError(('donanim komutu ve yanıtı', ayna, rapor[0]))
    siniflar = {int(y):int(t) for y,t in re.findall(rb'^USB (\d+) \w{4}:\w{4} (\d+)$',rapor[0],re.M)}
    q = c.q
    x = next(a for a in dugumler(q.call('query-pci')[0]['devices']) if a.get('qdev_id')=='x')
    bar = next(r['address'] for r in x['regions'] if r['bar']==0)
    def oku(a,fmt):
        return struct.unpack(fmt,c.mem(a,struct.calcsize(fmt)))
    cap = oku(bar,'<I')[0]&255
    dcbaa = oku(bar+cap+48,'<Q')[0]&~63
    k = (oku(bar+16,'<I')[0]>>16)*4
    while k:
        bas,ad,port = oku(bar+k,'<3I')
        if bas&255==2 and bas>>24==2 and ad==0x20425355:
            ilk = port&255
            break
        adim = bas>>8&255
        assert adim,'USB2 protokol alanı bulunamadı'
        k += adim*4
    else:
        raise AssertionError('USB2 protokol alanı bulunamadı')
    yollar, yuvalar = {}, {}
    for satir in re.findall(rb'^USB_YOL ([\d ]+)$',rapor[0],re.M):
        y,kok,rota,ust,alt,n = map(int,satir.split())
        ctx, = oku(dcbaa+8*y,'<Q')
        dw0,dw1,tt,dw3 = oku(ctx,'<4I')
        assert (dw0&0xfffff,(dw1>>16)&255,tt&0xffff,dw3>>27)==(rota,kok,0,3)
        yol = [kok-ilk+1]
        while rota:
            yol.append(rota&15)
            rota >>= 4
        yol = tuple(yol)
        assert yol not in yollar and (not n)==(siniflar[y]!=9)
        yollar[yol]=siniflar[y]
        yuvalar[yol]=(y,ust,alt)
    assert len(yollar)==len(siniflar)
    for yol,(_,ust,alt) in yuvalar.items():
        assert (ust,alt)==((yuvalar[yol[:-1]][0],yol[-1]) if len(yol)>1 else (0,0))
    return yollar


def kos(w, derinlik, hizli):
    dosyalar = {} if hizli else {'dosyalar.elf':(w/'kullanici/dosyalar.elf').read_bytes()}
    usb,nvme,sata = imajlar(w,dosyalar)
    yol = '/diskler/usb0p1'
    # Açılış NVMe üzerindedir. USB belleğin ilk bölümü bağımsız veri imajıdır.
    with boot(w,usb[0],nvme,sata,acilis='nvme',fare='usb',hub_derinligi=derinlik) as (q,p,s):
        c = SurecConsole(w,q,p,s,disk='NVMe')
        assert envanter(c)==hmp_agaci(q)
        if not hizli:
            gercek = Fat(nvme.read()).get('/merhaba.txt')
            c.run('cat '+yol+'/merhaba.txt',gercek.rstrip(b'\n')+b'\n')
            c.run('bagla --yazilabilir usb0p1')
            c.run('yaz '+yol+'/hub.txt hub verisi')
            c.run('sync')
            c.input('calistir '+yol+'/bin/dosyalar.elf '+yol)
            adlar=ge_girdiler(nvme,'/')
            ge_ekran(c,yol,adlar,0)
            q.call('input-send-event',{'events':[{'type':'rel','data':{'axis':'x','value':12}}]})
            wait_for(p,lambda:c.sayi('ksv_fare_x')==12)
            for bas in (True,False):
                q.call('input-send-event',{'events':[{'type':'btn','data':{'button':'wheel-down','down':bas}}]})
            ge_ekran(c,yol,adlar,3)
            # Uygulamayı kapatan tuşu yineleme süresinden uzun tut. Kabuk
            # odağı alınca eski sahibin tuşu komut satırına sızmamalı.
            c.key('q',True)
            tamam(c,'dosyalar',1)
            ilk=c.tik()
            wait_for(p,lambda:c.tik()>=ilk+65)
            c.key('q',False)
        alt = '.'.join(['1']*derinlik+['4'])
        farklar=[]
        for ad,aygit in [('bir','usb-tablet'),('iki','usb-hub')]:
            args={'driver':aygit,'id':ad,'bus':'x.0','port':alt}
            if aygit=='usb-hub': args['ports']=2
            q.call('device_add',args)
            for takili in (True,False):
                if not takili: q.call('device_del',{'id':ad})
                beklenen=wait_for(p,lambda: (agac if (tuple(map(int,alt.split('.'))) in (agac:=hmp_agaci(q)))==takili else None))
                # Hub interrupt IN aralığı ve güç/sıfırlama süresi konuk saatinde geçer.
                ilk=c.sayi('kap_lapic_tik')
                wait_for(p,lambda:c.sayi('kap_lapic_tik')>=ilk+70)
                # Aygıtın bildirdiği kalıcı hata da ölçüm günlüğüne girer.
                ek=c.output()[len(c.expected):]
                if ek:
                    assert ek==b'KABUK HATA USB hub port degisimi onaylanmadi\n',ek
                    c.expected+=ek
                alinan=envanter(c)
                if alinan!=beklenen: farklar.append((ad,takili,beklenen,alinan))
                print('HUB_TAK' if takili else 'HUB_CIK',ad,alinan,flush=True)
        assert not farklar,('USB hub takma/çıkarma envanteri',farklar)
        if not hizli and derinlik==2:
            # Aynı veri diski çıkarılıp yeniden takılınca eski FAT önbelleği okunamaz.
            q.call('device_del',{'id':'bellek'})
            ilk=c.sayi('kap_lapic_tik')
            wait_for(p,lambda:c.sayi('kap_lapic_tik')>=ilk+70)
            assert envanter(c)==hmp_agaci(q)
            c.run('cat '+yol+'/hub.txt',b'KABUK HATA usb bellek cikarildi\n')
            # -drive, son ön uçla birlikte silinebilir. Dosya yeni bir düğüm olarak açılır.
            q.call('blockdev-add',{'node-name':'yenidepo','driver':'raw','file':{'driver':'file','filename':str(nvme.disk)}})
            q.call('device_add',{'driver':'usb-storage','id':'depo2','bus':'x.0','port':'1.1.2','drive':'yenidepo'})
            ilk=c.sayi('kap_lapic_tik')
            wait_for(p,lambda:c.sayi('kap_lapic_tik')>=ilk+70)
            assert envanter(c)==hmp_agaci(q)
            c.run('cat /diskler/usb1p1/hub.txt',b'hub verisi\n')
        c.shutdown()
    if not hizli:
        fsck(w,nvme,'hub')
        assert Fat(nvme.read()).get('/hub.txt')==b'hub verisi'
    print('USB_HUB OK',derinlik,flush=True)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('dizin',type=Path)
    p.add_argument('--derinlik',type=int,choices=(1,2))
    p.add_argument('--yalniz-envanter',action='store_true')
    a=p.parse_args()
    for n in ([a.derinlik] if a.derinlik else [1,2]):
        kos(a.dizin.resolve(),n,a.yalniz_envanter)
