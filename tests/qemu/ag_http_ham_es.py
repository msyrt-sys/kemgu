"""Yalnız QEMU'nun yerel datagram hattında üç sabit HTTP test dosyası sunar."""
from contextlib import contextmanager
import threading
import time

from ag_es import EthernetEs, TcpEs, ES_IP, KONUK_IP, ES_MAC, ethernet, ipv4, yankilama
from pcap_oku import cerceve_oku


TAM_PAKET = bytes(range(22))


class HamHat(EthernetEs):
    def __init__(self, mac):
        super().__init__()
        self.konuk_mac = mac
        self.tam_yanit = threading.Event()

    def tam_paketi_sina(self):
        # 14 Ethernet + 20 IPv4 + 8 ICMP + 22 veri = tam 64 bayt.
        b = yankilama(8,0x1616,16,TAM_PAKET)
        self.gonder(ethernet(self.konuk_mac,ES_MAC,2048,ipv4(ES_IP,KONUK_IP,1,b)))
        assert self.tam_yanit.wait(3), '64 baytlık ECM yanıtının son USB paketi gelmedi'


class HttpOturum:
    def __init__(self, tcp, port, ilk):
        self.tcp = tcp
        self.port = port
        self.ack = ilk+1
        self.bas = 170016+port*128
        self.istek = bytearray()
        self.yanit = None
        self.onay = self.bas
        self.son = self.bas
        self.zaman = time.monotonic()
        self.bitti = False
        self.yolla(self.bas-1,18,sec=bytes.fromhex('020405b4'))

    def yolla(self, sira, bayrak, veri=b'',sec=b''):
        self.tcp.yolla(8016,self.port,sira,self.ack,bayrak,veri,sec=sec)

    def pencere(self):
        if self.yanit is None or self.bitti:
            return
        bitis=self.bas+len(self.yanit)
        if self.son==bitis and self.onay==bitis:
            self.yolla(self.son,17)
            self.son+=1
        while self.son<bitis and self.son-self.onay<2800:
            no=self.son-self.bas
            parca=self.yanit[no:no+1400]
            self.yolla(self.son,24,parca)
            self.son+=len(parca)
        self.zaman=time.monotonic()

    def al(self,p,dosyalar,kayit):
        veri = p.yuk[4*(p.yuk[12]>>4):]
        if p.bayrak&4:
            self.bitti=True
            return
        if p.bayrak&2:
            self.yolla(self.bas-1,18,sec=bytes.fromhex('020405b4'))
            return
        if self.onay<=p.onay<=self.son:
            self.onay=p.onay
        if veri and p.sira==(self.ack&0xffffffff):
            self.istek.extend(veri)
            assert len(self.istek)<=4096
            self.ack+=len(veri)
            if b'\r\n\r\n' in self.istek and self.yanit is None:
                yontem,yol,_=bytes(self.istek).split(b'\r\n',1)[0].split()
                assert yontem==b'GET'
                ad=yol.decode('ascii')
                govde=dosyalar[ad]
                kayit.append(ad)
                self.yanit=f'HTTP/1.1 200 OK\r\nContent-Length: {len(govde)}\r\nConnection: close\r\n\r\n'.encode()+govde
        if p.bayrak&1 and (p.sira+len(veri))&0xffffffff==self.ack&0xffffffff:
            self.ack+=1
            self.yolla(self.son,16)
        if self.yanit is not None and self.onay>self.bas+len(self.yanit):
            self.bitti=True
        elif self.yanit is None and veri:
            self.yolla(self.son,16)
        else:
            self.pencere()

    def tekrar(self):
        if self.yanit is not None and not self.bitti and self.son>self.onay and time.monotonic()-self.zaman>.20:
            self.son=self.onay
            self.pencere()


@contextmanager
def ham_depo(depo,mac):
    dosyalar={'/'+ad:(depo/ad).read_bytes() for ad in ('paketler.txt','hesapla.elf','veri.bin')}
    kayit=[]
    hatalar=[]
    with HamHat(mac) as hat:
        tcp=TcpEs(hat,mac)
        dur=threading.Event()
        def calis():
            oturumlar={}
            try:
                while not dur.is_set():
                    try:
                        ham=hat.al(.04)
                    except TimeoutError:
                        for o in oturumlar.values():
                            o.tekrar()
                        continue
                    p=cerceve_oku(ham)
                    if p.kaynak!=mac:
                        continue
                    if p.protokol==1 and p.icmp_tur==0 and p.yuk[4:8]==bytes.fromhex('16160010') and p.yuk[8:]==TAM_PAKET:
                        assert len(p.ham)==64
                        hat.tam_yanit.set()
                    elif p.protokol==6 and p.hedef_port==8016:
                        if p.kaynak_port not in oturumlar:
                            if p.bayrak==2:
                                assert len(oturumlar)<8
                                oturumlar[p.kaynak_port]=HttpOturum(tcp,p.kaynak_port,p.sira)
                        else:
                            oturumlar[p.kaynak_port].al(p,dosyalar,kayit)
                    else:
                        cevap=hat.yanit(ham)
                        if cevap:
                            hat.gonder(cevap)
            except Exception as exc:
                hatalar.append(exc)
        t=threading.Thread(target=calis)
        t.start()
        try:
            yield hat,kayit
        finally:
            dur.set()
            t.join()
            if hatalar:
                raise hatalar[0]
