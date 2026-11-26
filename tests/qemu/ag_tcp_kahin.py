"""Bir etkin ve bir pasif bağlantıda konuk, tel ve pcap sıra uzayını karşılaştırır."""
import argparse
import hashlib
from pathlib import Path
import time

from ag_es import EthernetEs, ES_IP, KONUK_IP, TcpEs
from dalga12_kahin import boot, wait_for
from pcap_oku import cerceve_oku, pcap_kayitlari
from surec_imaj import imajlar
from surec_kahin import SurecConsole

MAC=bytes.fromhex('525400170002')
MASKE=0xffffffff
MSS=bytes.fromhex('020405b4')


def geri(p,seq,ack,bayrak):
    assert (p.sira,p.onay,p.bayrak)==(seq&MASKE,ack&MASKE,bayrak),(p.sira,p.onay,p.bayrak,seq,ack,bayrak)


def pasif(es):
    sp,dp=17001,9001
    seq=0xfffff060
    es.yolla(sp,dp,seq,0,2,sec=MSS)
    p,b=es.al()
    assert p.bayrak==18 and p.onay==seq+1 and not b and p.yuk[20:24]==MSS
    seq+=1
    ack=p.sira+1
    es.yolla(sp,dp,seq,ack,16)
    yuk=bytes((i*41+i//127)&255 for i in range(65536))
    giden,alinan=0,bytearray()
    while giden<len(yuk):
        parca=yuk[giden:giden+1400]
        es.yolla(sp,dp,seq,ack,24,parca)
        seq+=len(parca);giden+=len(parca)
        while len(alinan)<giden:
            p,b=es.al()
            assert p.sira==ack&MASKE and not(p.bayrak&7) and p.onay==seq&MASKE,(p,len(b))
            if b:
                alinan.extend(b);ack+=len(b)
                es.yolla(sp,dp,seq,ack,16)
    assert bytes(alinan)==yuk
    es.yolla(sp,dp,seq,ack,17)
    seq+=1
    while True:
        p,b=es.al()
        assert not b and p.onay==seq&MASKE and p.sira==ack&MASKE
        if p.bayrak&1:
            ack+=1;es.yolla(sp,dp,seq,ack,16);break
    return hashlib.sha256(yuk).hexdigest()


def aktif(es):
    p,b=es.al()
    assert p.bayrak==2 and p.hedef_port==9001 and p.yuk[20:24]==MSS and not b
    port=p.kaynak_port
    seq=0xffffff00
    ack=p.sira+1
    # Sıfır pencere ilk veri baytından önce yoklamayı zorlar.
    es.yolla(9001,port,seq,ack,18,win=0,sec=MSS)
    seq+=1
    ilk=p.sira
    p,b=es.al()
    if not b:
        geri(p,ack,seq,16);p,b=es.al()
    assert p.sira==ilk and len(b)==1,(p.sira,len(b),ilk)
    es.yolla(9001,port,seq,ack,16,win=8192)
    veri=bytearray()
    while True:
        p,b=es.al()
        assert p.sira==ack&MASKE and not p.bayrak&6
        if b:
            assert len(b)<=1460
            veri.extend(b);ack+=len(b)
            es.yolla(9001,port,seq,ack,24,b,win=8192);seq+=len(b)
        if p.bayrak&1:
            ack+=1;break
    assert bytes(veri)==bytes((i*37+i//251)&255 for i in range(65536))
    es.yolla(9001,port,seq,ack,17);seq+=1
    p,b=es.al();geri(p,ack,seq,16)
    assert not b
    return hashlib.sha256(veri).hexdigest()


def kapasite(es):
    baglar=[]
    for i in range(8):
        sp=21000+i
        bas=8000+i
        es.yolla(sp,9001,bas,0,2,sec=MSS)
        p,b=es.al()
        assert p.bayrak==18 and p.onay==bas+1 and p.hedef_port==sp and not b
        es.yolla(sp,9001,bas+1,p.sira+1,16)
        baglar.append((sp,bas+1,p.sira+1))
    es.yolla(21008,9001,8010,0,2,sec=MSS)
    p,b=es.al()
    geri(p,0,8011,20)
    assert not b
    for sp,seq,ack in baglar:
        es.yolla(sp,9001,seq,ack,17)
        while True:
            p,b=es.al()
            assert not b and p.hedef_port==sp and p.onay==seq+1
            if p.bayrak&1:
                es.yolla(sp,9001,seq+1,p.sira+1,16)
                break


def kos(w):
    disk,nvme,sata=imajlar(w,{})
    pcap=w/'tcp-temel.pcap'
    with EthernetEs() as hat:
        net=('-netdev',hat.qemu_netdev(),'-device',f'e1000,id=ag0,netdev=n0,mac={MAC.hex(":")}',
             '-object',f'filter-dump,id=f0,netdev=n0,file={pcap}')
        with boot(w,disk[0],nvme,sata,ag_aygitlari=net) as (q,p,seri):
            c=SurecConsole(w,q,p,seri)
            c.run('ag ayarla 192.0.2.2/24 192.0.2.1',b'AG AYAR OK\n')
            c.run('ag tcp dinle',b'TCP dinle 9001\n')
            while True:
                garp=cerceve_oku(hat.al(3))
                if garp.tur==2054 and garp.yuk[24:28]==KONUK_IP: break
            es=TcpEs(hat,MAC)
            heap,bos=c.heap(),c.bos_cerceve()
            h1=pasif(es)
            es.yolla(19001,9876,42,0,2,sec=MSS)
            r,_=es.al();geri(r,0,43,20)
            es.yolla(19002,9001,42,0,2,sec=MSS)
            r,_=es.al();es.yolla(19002,9001,43,r.sira+1,16)
            es.yolla(19002,9001,43,r.sira+1,20)
            wait_for(p,lambda: c.sayi('ag_tcp_rst')==1)
            c.run('ag tcp bagla 192.0.2.1',b'TCP baglanti basladi\n')
            h2=aktif(es)
            wait_for(p,lambda: c.sayi('ag_tcp_bitti')>=2,seconds=8)
            c.run('ag tcp durum',b'TCP yazilan okunan hata kapanan 65536 65536 0 2\n')
            assert (c.heap(),c.bos_cerceve())==(heap,bos)
            once=pcap.stat().st_size
            c.run('ag tcp bagla 127.0.0.1',b'TCP baglanti basladi\n')
            wait_for(p,lambda: c.sayi('ag_tcp_bitti')==4,seconds=20)
            c.run('ag tcp durum',b'TCP yazilan okunan hata kapanan 65536 65536 0 4\n')
            assert pcap.stat().st_size==once and (c.heap(),c.bos_cerceve())==(heap,bos)
            kapasite(es)
            wait_for(p,lambda: c.sayi('ag_tcp_bitti')==12)
            assert c.sayi('ag_tcp_red')==2 and (c.heap(),c.bos_cerceve())==(heap,bos)
            c.shutdown()
        with pcap.open('rb') as f: kayit=[p for p in pcap_kayitlari(f) if p.protokol==6]
        kaynak=[p for p in kayit if p.kaynak==MAC and p.kaynak_port==9001 and p.hedef_port==17001]
        assert hashlib.sha256(b''.join(p.yuk[4*(p.yuk[12]>>4):] for p in kaynak)).hexdigest()==h1
        assert sum(bool(p.bayrak&1) for p in kayit)==20 and sum(bool(p.bayrak&4) for p in kayit)==3
        assert all(p.ip_kaynak[0]!=127 and p.ip_hedef[0]!=127 for p in kayit)
        print(f'TCP OK 65536 bayt pasif={h1} aktif={h2}, loopback 65536, pcap={len(kayit)}, heap={heap} boş={bos}',flush=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('dizin',type=Path)
    kos(parser.parse_args().dizin.resolve())
