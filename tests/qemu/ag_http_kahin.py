"""Konuk HTTP istemcisi ve sunucusu için bağımsız HTTP/1.1 ve FAT32 ölçümü."""
import hashlib
import re
import http.client
import urllib.request
import urllib.error
from concurrent.futures import ThreadPoolExecutor
import sys
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from contextlib import contextmanager, closing

from ag_arac_kahin import komut
from bekle import bekle
from pcap_oku import pcap_kayitlari
from uefi_kabuk_kahin import keys
from dalga12_fat import Fat
from dalga12_kahin import boot, fsck, wait_for
from surec_imaj import imajlar
from surec_kahin import SurecConsole

BUYUK = bytes((i * 43 + i // 197) & 255 for i in range(65536)) * 128
KUCUK = bytes(range(255, -1, -1)) * 7 + b'HTTP dosya sonu'


class HttpKonsol(SurecConsole):
    def input(self, text):
        tr = {':': ('slash', True), '-': ('equal', False), '&': ('6', True)}
        for char in text + '\n':
            ad, shift = tr.get(char, keys(char))
            if shift:
                self.key('shift', True)
            self.key(ad, True)
            self.key(ad, False)
            if shift:
                self.key('shift', False)


@contextmanager
def http_es():
    kayit = []
    class Es(BaseHTTPRequestHandler):
        protocol_version = 'HTTP/1.1'

        def log_message(self, *args):
            pass

        def do_GET(self):
            kayit.append((self.path, self.client_address[1], self.headers['Host']))
            if self.path.startswith('/yon') and self.path[4:].isdigit():
                no=int(self.path[4:])
                self.send_response(302)
                self.send_header('Location',f'/yon{no+1}' if no<6 else '/son')
                self.send_header('Content-Length','0')
                self.end_headers()
            elif self.path=='/orta':
                veri=BUYUK[:73793]
                self.send_response(200)
                self.send_header('Content-Length',str(len(veri)))
                self.end_headers()
                self.wfile.write(veri)
            elif self.path=='/eksik':
                self.send_response(200)
                self.send_header('Content-Length','100')
                self.send_header('Connection','close')
                self.end_headers()
                self.wfile.write(b'eksik')
                self.close_connection=True
            elif self.path in ('/bir', '/iki'):
                self.send_response(301 if self.path=='/bir' else 307)
                self.send_header('Location','iki' if self.path=='/bir' else '/buyuk')
                self.send_header('Content-Length','0')
                self.end_headers()
            elif self.path=='/buyuk':
                self.send_response(200)
                self.send_header('Transfer-Encoding','chunked')
                self.end_headers()
                for i in range(0,len(BUYUK),49157):
                    b=BUYUK[i:i+49157]
                    self.wfile.write(f'{len(b):x};alan=evet\r\n'.encode()+b+b'\r\n')
                self.wfile.write(b'0\r\nX-Son: evet\r\n\r\n')
            else:
                self.send_response(200)
                self.send_header('Content-Length',str(len(KUCUK)))
                self.end_headers()
                self.wfile.write(KUCUK)
    with ThreadingHTTPServer(('127.0.0.1',0), Es) as sunucu:
        t=threading.Thread(target=sunucu.serve_forever,daemon=True); t.start()
        try:
            yield sunucu.server_port,kayit
        finally:
            sunucu.shutdown(); t.join()


def kos(w):
    progs={'indir.elf':(w/'indir.elf').read_bytes()}
    usb,nvme,sata=imajlar(w,progs)
    pcap=w/'http-istemci.pcap'
    net=('-netdev','user,id=n0','-device','e1000,netdev=n0',
         '-object',f'filter-dump,id=f0,netdev=n0,file={pcap}')
    with http_es() as (port,kayit), boot(w,usb[0],nvme,sata,ag_aygitlari=net) as (q,p,seri):
        c=HttpKonsol(w,q,p,seri)
        once=c.heap(),c.bos_cerceve()
        wait_for(p,lambda:int.from_bytes(c.mem(c.sayi('ag_l5'),4),'little')==3,seconds=15)
        komut(c,'bagla --yazilabilir usb0p2')
        komut(c,'cd /diskler/usb0p2/bin')
        url=f'http://10.0.2.2:{port}'
        s=komut(c,f'indir {url}/bir buyuk.bin {url}/son kisa.bin',120)
        assert f'indir tamam {len(BUYUK)}\n'.encode() in s and f'indir tamam {len(KUCUK)}\n'.encode() in s,s
        assert [r[0] for r in kayit]==['/bir','/iki','/buyuk','/son'],kayit
        assert kayit[2][1]==kayit[3][1],kayit
        assert all(r[2]==f'10.0.2.2:{port}' for r in kayit),kayit
        s=komut(c,f'indir {url}/son /ram/ufak.bin',35)
        assert f'indir tamam {len(KUCUK)}\n'.encode() in s,s
        # Bağımsız senaryolarda önceki FIN'ler dört saniyelik TIME_WAIT'te
        # sekiz TCP yuvasını tutabilir. Gerçek havuzun geri verilmesini ölç.
        def tcp_bos():
            return int.from_bytes(c.mem(c.symbols['ag_tcp_etkin'],4),'little')
        def kapanislari_bekle():
            ilk = tcp_bos()
            bekle('HTTP bağlantılarının TCP havuzuna iadesi', tcp_bos, lambda n:n==0,
                  sure=18, aralik=.05, surec=p)
            print('HTTP TCP etkin yuva', ilk, '→ 0', flush=True)
        kapanislari_bekle()
        s=komut(c,f'indir {url}/yon0 korun.bin',35)
        assert b'indir hata -40' in s and [r[0] for r in kayit[-6:]]==[f'/yon{i}' for i in range(6)],(s,kayit)
        kapanislari_bekle()
        s=komut(c,f'indir {url}/eksik kisa.bin',35)
        assert b'indir hata -74' in s and kayit[-1][0]=='/eksik',(s,kayit[-1])
        son=c.heap(),c.bos_cerceve()
        assert once[1]==son[1],(once,son)
        print('HTTP istemci heap/boş çerçeve',once,son,flush=True)
        c.expected=c.output()
        c.shutdown()
    fsck(w,usb[1],'http-istemci')
    fat=Fat(usb[1].read())
    for ad,veri in [('buyuk.bin',BUYUK),('kisa.bin',KUCUK)]:
        b=fat.get('/BIN/'+ad); assert b==veri
        print('HTTP FAT32',ad,len(b),hashlib.sha256(b).hexdigest(),flush=True)
    tel_dogrula(pcap,port)



def tel_dogrula(pcap,port):
    akislar={}
    with pcap.open('rb') as f:
        for p in pcap_kayitlari(f):
            if p.protokol!=6 or p.kaynak_port!=port:
                continue
            if p.bayrak&2:
                akislar[p.hedef_port]=(p.sira+1,{})
            if p.hedef_port not in akislar:
                continue
            bas,veri=akislar[p.hedef_port]
            b=p.yuk[(p.yuk[12]>>4)*4:]
            if b:
                ofs=(p.sira-bas)&0xffffffff
                if ofs in veri:
                    assert veri[ofs]==b
                veri[ofs]=b
    for _,paket in akislar.values():
        tel=bytearray()
        for i,b in sorted(paket.items()):
            assert i<=len(tel)
            ortak=min(len(tel)-i,len(b))
            assert tel[i:i+ortak]==b[:ortak]
            tel.extend(b[ortak:])
        if b'Transfer-Encoding: chunked\r\n' not in tel[:512]:
            continue
        i=tel.index(b'\r\n\r\n')+4
        toplam=hashlib.sha256()
        while True:
            son=tel.index(b'\r\n',i)
            n=int(tel[i:son].split(b';')[0],16)
            i=son+2
            if not n:
                break
            toplam.update(tel[i:i+n])
            assert tel[i+n:i+n+2]==b'\r\n'
            i+=n+2
        assert toplam.digest()==hashlib.sha256(BUYUK).digest()
        i=tel.index(b'\r\n\r\n',i)+4
        assert tel[i:].startswith(b'HTTP/1.1 200 ')
        i=tel.index(b'\r\n\r\n',i)+4
        assert tel[i:]==KUCUK
        return
    raise AssertionError('pcap içinde iki yanıtlı HTTP bağlantısı bulunamadı')


def sunucu_kos(w):
    dosyalar={f'd{i}.bin':bytes([i+51])* (1801+i*3491) for i in range(8)}
    program={'sunucu.elf':(w/'sunucu.elf').read_bytes(),**dosyalar}
    usb,nvme,sata=imajlar(w,program)
    pcap=w/'http-sunucu.pcap'
    net=('-netdev','user,id=n0,hostfwd=tcp:127.0.0.1:0-10.0.2.15:9131',
         '-device','e1000,netdev=n0','-object',f'filter-dump,id=f0,netdev=n0,file={pcap}')
    with boot(w,usb[0],nvme,sata,ag_aygitlari=net) as (q,p,seri):
        c=HttpKonsol(w,q,p,seri)
        wait_for(p,lambda:int.from_bytes(c.mem(c.sayi('ag_l5'),4),'little')==3,seconds=15)
        yon=q.call('human-monitor-command',{'command-line':'info usernet'})
        port=int(re.search(r'TCP\[HOST_FORWARD\]\s+\d+\s+127\.0\.0\.1\s+(\d+)\s+10\.0\.2\.15\s+9131',yon)[1])
        once=c.heap(),c.bos_cerceve()
        ilk=seri.stat().st_size
        c.input('sunucu 9131 /diskler/usb0p2/bin')
        wait_for(p,lambda:b'HTTP dinliyor 9131' in seri.read_bytes()[ilk:],seconds=20)
        # Sekiz ayrı soket yalnız başlığın son LF'sini eksik bırakır. Tümü bağlandıktan sonra tamamlarlar.
        geldi=threading.Barrier(8,timeout=20)
        sekiz_bitti=threading.Event()
        class Bag(http.client.HTTPConnection):
            def send(self,data):
                if isinstance(data,bytes) and data.endswith(b'\r\n\r\n') and not data.startswith(b'GET /d7.bin '):
                    super().send(data[:-1])
                    geldi.wait()
                    assert sekiz_bitti.wait(20),'sekizinci istemci ilk yedisini bekledi'
                    return super().send(data[-1:])
                return super().send(data)
        class Istek(urllib.request.HTTPHandler):
            def http_open(self,req):
                return self.do_open(Bag,req)
        def al(ad):
            opener=urllib.request.build_opener(urllib.request.ProxyHandler({}),Istek())
            with opener.open(f'http://127.0.0.1:{port}/{ad}',timeout=35) as r:
                b=r.read()
            assert hashlib.sha256(b).digest()==hashlib.sha256(dosyalar[ad]).digest(),ad
        with ThreadPoolExecutor(8) as havuz:
            bekleyen=[havuz.submit(al,ad) for ad in list(dosyalar)[:7]]
            geldi.wait()
            try:
                al('d7.bin')
            finally:
                sekiz_bitti.set()
            for istek in bekleyen:
                istek.result(20)
        with closing(http.client.HTTPConnection('127.0.0.1',port,timeout=15)) as h:
            for yol,durum,bek in [('/',200,b'd0.bin\n'),('/d3.bin',200,dosyalar['d3.bin']),
                                  ('/yok.txt',404,b''),('/../gizli',403,b''),('/%2e%2e/gizli',403,b'')]:
                h.request('GET',yol)
                yanit=h.getresponse(); b=yanit.read()
                assert yanit.status==durum and bek in b,(yol,yanit.status,b[:80])
        c.key('ctrl',True); c.key('c',True); c.key('c',False); c.key('ctrl',False)
        wait_for(p,lambda:seri.read_bytes()[ilk:].endswith(b'KABUK_HAZIR\n'),seconds=12)
        assert b'HTTP 403 Forbidden GET ' in seri.read_bytes()[ilk:]
        son=c.heap(),c.bos_cerceve()
        assert once[1]==son[1],(once,son)
        print('HTTP sunucu heap/boş çerçeve',once,son,flush=True)
        c.expected=c.output(); c.shutdown()
    fsck(w,usb[1],'http-sunucu')
    with pcap.open('rb') as f:
        istekler={p.kaynak_port for p in pcap_kayitlari(f) if p.protokol==6 and p.hedef_port==9131 and p.bayrak&2}
    assert len(istekler)>=9
    print('HTTP sunucu 8 eşzamanlı urllib, SHA256, dizin, keep-alive, 403/404, Ctrl+C',flush=True)


def blok_kos(w):
    usb,nvme,sata=imajlar(w,{'indir.elf':(w/'indir.elf').read_bytes()})
    net=('-netdev','user,id=n0','-device','e1000,netdev=n0')
    with http_es() as (port,_), boot(w,usb[0],nvme,sata,ag_aygitlari=net) as (q,p,seri):
        c=HttpKonsol(w,q,p,seri)
        wait_for(p,lambda:int.from_bytes(c.mem(c.sayi('ag_l5'),4),'little')==3,seconds=15)
        once=c.heap(),c.bos_cerceve()
        for disk in ('nvme0p1','sata0p1'):
            komut(c,f'bagla --yazilabilir {disk}')
            s=komut(c,f'indir http://10.0.2.2:{port}/orta /diskler/{disk}/orta.bin')
            assert b'indir tamam 73793' in s,s
        son=c.heap(),c.bos_cerceve()
        assert once[1]==son[1],(once,son)
        print('HTTP NVMe/AHCI heap/boş çerçeve',once,son,flush=True)
        c.expected=c.output(); c.shutdown()
    for bolum,isim in [(nvme,'nvme'),(sata,'ahci')]:
        fsck(w,bolum,'http-'+isim)
        assert Fat(bolum.read()).get('/orta.bin')==BUYUK[:73793]
    print('HTTP NVMe ve AHCI 73793 bayt iki imajda birebir',flush=True)


if __name__=='__main__':
    w=Path(sys.argv[1]).resolve()
    if len(sys.argv)==2 or sys.argv[2]=='istemci':
        kos(w)
    if len(sys.argv)==2 or sys.argv[2]=='sunucu':
        sunucu_kos(w)
    if len(sys.argv)==2 or sys.argv[2]=='blok':
        blok_kos(w)
