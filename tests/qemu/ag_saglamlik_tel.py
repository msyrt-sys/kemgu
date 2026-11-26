"""Hızı sınırlı yerel QEMU trafiğini ve her iş yükündeki çekirdek kaynaklarını ölçer."""
import argparse
import hashlib
from pathlib import Path
import random
import struct
import time
from ag_es import ES_IP, ES_MAC, KONUK_IP, EthernetEs, TcpEs, arp, ethernet, ipv4, udp, yankilama
from dalga12_kahin import boot, wait_for
from pcap_oku import cerceve_oku
from surec_imaj import imajlar
from surec_kahin import SurecConsole

MAC=bytes.fromhex('525400170018')
MASKE=0xffffffff


def olc(c,ad,once):
    son=c.heap(),c.bos_cerceve()
    print(f'AG18 OLCUM {ad} heap={once[0]}->{son[0]} cerceve={once[1]}->{son[1]}',flush=True)
    assert son==once,(ad,once,son)
    return son


def yankila(es,i):
    veri=struct.pack('!I',i)
    es.gonder(ethernet(MAC,ES_MAC,2048,ipv4(ES_IP,KONUK_IP,1,yankilama(8,18,i&65535,veri))))
    bitis=time.monotonic()+3
    while time.monotonic()<bitis:
        p=cerceve_oku(es.al(max(.001,bitis-time.monotonic())))
        if p.protokol==1 and p.icmp_tur==0 and p.yuk[4:]==struct.pack('!HH',18,i&65535)+veri:
            return
        if p.tur==2054 and p.yuk[24:28]==ES_IP:
            es.gonder(ethernet(MAC,ES_MAC,2054,arp(2,ES_MAC,ES_IP,MAC,KONUK_IP)))
    raise AssertionError('ICMP yanıtı gelmedi')


def ornekler(es):
    rng=random.Random(170018)
    for i in range(10000):
        govde=rng.randbytes(16+i%61)
        if i%4==0:
            paket=ethernet(MAC,ES_MAC,0x88b5,govde)
        elif i%4==1:
            ip=bytearray(ipv4(ES_IP,KONUK_IP,253,govde))
            ip[10]^=1
            paket=ethernet(MAC,ES_MAC,2048,ip)
        elif i%4==2:
            ip=ipv4(ES_IP,KONUK_IP,17,udp(ES_IP,KONUK_IP,49110,49111,govde))
            paket=ethernet(MAC,ES_MAC,2048,ip)
        else:
            icmp=bytearray(yankilama(8,18,i,govde))
            icmp[2]^=1
            paket=ethernet(MAC,ES_MAC,2048,ipv4(ES_IP,KONUK_IP,1,icmp))
        es.gonder(paket)
        time.sleep(.001)
        if i%128==127:
            yankila(es,i)


def baglanti(es,i,adet,q=None):
    sp=20000+i
    seq=100000+200*i
    es.yolla(sp,9001,seq,0,2,sec=bytes.fromhex('020405b4'))
    p,b=es.al()
    assert (p.bayrak,p.onay,p.hedef_port)==(18,seq+1,sp)
    seq+=1
    ack=(p.sira+1)&MASKE
    es.yolla(sp,9001,seq,ack,16)
    giden=0
    giden_sha=hashlib.sha256()
    gelen_sha=hashlib.sha256()
    desen=bytes((j*37+i)&255 for j in range(256))*50
    while giden<adet:
        n=min(1400 if giden<2800 else 11200,adet-giden)
        bas=giden%256
        beklenen=desen[bas:bas+n]
        for ofset in range(0,n,1400):
            es.yolla(sp,9001,seq+ofset,ack,24,beklenen[ofset:ofset+1400])
        son=seq+n
        got=0
        alindi=seq
        bitis=time.monotonic()+12
        while got<n:
            if time.monotonic()>bitis:
                raise TimeoutError((giden,n,got,alindi-seq))
            try:
                p,b=es.al(.45)
            except TimeoutError:
                if alindi<son:
                    ofset=alindi-seq
                    es.yolla(sp,9001,alindi,ack,24,beklenen[ofset:ofset+1400])
                else:
                    es.yolla(sp,9001,son,ack,16)
                continue
            alindi=max(alindi,min(son,p.onay))
            assert p.hedef_port==sp and not p.bayrak&7
            if b and p.sira!=ack:
                # Daha önce onaylanan veya yerinden önce gelen veri için kümülatif ACK.
                es.yolla(sp,9001,son,ack,16)
            if b and p.sira==ack:
                assert b==beklenen[got:got+len(b)]
                if q is not None and giden==1400 and not got:
                    # İlk kopyayı onaylamadan kabloyu kes ve aynı sıranın yeniden geldiğini gör.
                    q.call('set_link',{'name':'ag0','up':False})
                    time.sleep(.45)
                    q.call('set_link',{'name':'ag0','up':True})
                    while True:
                        tekrar,veri=es.al(8)
                        if veri and tekrar.sira==ack:
                            assert veri==b
                            break
                    print('AG18 link off/on TCP yeniden iletim',ack,len(b),flush=True)
                got+=len(b)
                gelen_sha.update(b)
                ack=(ack+len(b))&MASKE
                es.yolla(sp,9001,son,ack,16)
        seq=son
        giden_sha.update(beklenen)
        giden+=n
        if adet>1<<20 and giden//(16<<20)!=(giden-n)//(16<<20):
            print('AG18 yankilanan MiB',giden>>20,flush=True)
    es.yolla(sp,9001,seq,ack,17)
    while True:
        p,b=es.al()
        assert p.hedef_port==sp and not b
        if p.bayrak&1:
            es.yolla(sp,9001,seq+1,(p.sira+1)&MASKE,16)
            break
    assert giden_sha.digest()==gelen_sha.digest()
    return gelen_sha.hexdigest()


HTTP_DOSYA=b'KEMGU HTTP iz dosyasi'


def http_yuz(c,p,s,tcp):
    ilk=s.stat().st_size
    c.input('sunucu 9131 /diskler/usb0p2/bin')
    wait_for(p,lambda:b'HTTP dinliyor 9131' in s.read_bytes()[ilk:],seconds=20)
    once=c.heap(),c.bos_cerceve()
    sp,dp,seq=29001,9131,701901
    tcp.yolla(sp,dp,seq,0,2)
    h,_=tcp.al()
    assert h.bayrak==18 and h.hedef_port==sp and h.onay==seq+1
    ack=(h.sira+1)&MASKE
    seq+=1
    tcp.yolla(sp,dp,seq,ack,16)
    istek=b'GET /iz.txt HTTP/1.1\r\nHost: kemgu\r\n\r\n'
    for _ in range(100):
        tcp.yolla(sp,dp,seq,ack,24,istek)
        seq+=len(istek)
        yanit=bytearray()
        while not yanit.endswith(HTTP_DOSYA):
            h,b=tcp.al()
            assert h.hedef_port==sp and not h.bayrak&7
            if b and h.sira==ack:
                yanit.extend(b)
                assert len(yanit)<=1024
                ack=(ack+len(b))&MASKE
                tcp.yolla(sp,dp,seq,ack,16)
        baslik,govde=yanit.split(b'\r\n\r\n',1)
        assert baslik.startswith(b'HTTP/1.1 200 OK\r\n') and govde==HTTP_DOSYA
        assert f'Content-Length: {len(govde)}'.encode() in baslik.split(b'\r\n')
    tcp.yolla(sp,dp,seq,ack,17)
    while True:
        h,b=tcp.al()
        if h.bayrak&1:
            tcp.yolla(sp,dp,seq+1,(h.sira+1)&MASKE,16)
            break
    son=c.heap(),c.bos_cerceve()
    print(f'AG18 OLCUM 100-http heap={once[0]}->{son[0]} cerceve={once[1]}->{son[1]}',flush=True)
    assert once[1]==son[1]
    c.key('ctrl',True)
    c.key('c',True)
    c.key('c',False)
    c.key('ctrl',False)
    wait_for(p,lambda:s.read_bytes()[ilk:].endswith(b'KABUK_HAZIR\n'),seconds=15)
    assert s.read_bytes()[ilk:].count(b'HTTP 200 OK GET ')==100

def kos(w,kip):
    programlar={'sunucu.elf':(w/'sunucu.elf').read_bytes(),'iz.txt':HTTP_DOSYA} if kip=='cerceve' else {}
    usb,nvme,sata=imajlar(w,programlar)
    with EthernetEs() as es:
        net=('-netdev',es.qemu_netdev(),'-device',f'e1000,id=ag0,netdev=n0,mac={MAC.hex(":")}')
        with boot(w,usb[0],nvme,sata,ag_aygitlari=net) as (q,p,s):
            c=SurecConsole(w,q,p,s)
            c.run('ag ayarla 192.0.2.2/24 192.0.2.1',b'AG AYAR OK\n')
            while True:
                a=cerceve_oku(es.al(3))
                if a.tur==2054 and a.yuk[24:28]==KONUK_IP:
                    break
            if kip=='cerceve':
                once=c.heap(),c.bos_cerceve()
                c.run('ag tablolar',b'AG18 tablo 100 tur 0\n')
                olc(c,'100-tablo-turu',once)
            c.run('ag tcp dinle',b'TCP dinle 9001\n')
            # Komşu belleği ve protokol zamanlayıcılarının ilk çalışması ölçüme dahildir.
            yankila(es,0)
            once=c.heap(),c.bos_cerceve()
            tcp=TcpEs(es,MAC)
            if kip=='cerceve':
                say=c.sayi('ag_rx_sayi')
                l3=c.sayi('ag_l3_alan')
                def sayaclar():
                    return struct.unpack('<QQ',c.mem(l3+72,16))
                ilk=sayaclar()
                ornekler(es)
                assert c.sayi('ag_rx_sayi')-say>=10000
                son=sayaclar()
                fark=tuple(a-b for a,b in zip(son,ilk))
                print('AG18 saglama/bilinmeyen artisi',fark,flush=True)
                assert fark[0]>=4900 and fark[1]>=2400
                olc(c,'10000-karisik',once)
                for i in range(10000):
                    yankila(es,i)
                    time.sleep(.0005)
                olc(c,'10000-ping',once)
                biten=c.sayi('ag_tcp_bitti')
                for i in range(500):
                    baglanti(tcp,i,32)
                wait_for(p,lambda:c.sayi('ag_tcp_bitti')>=biten+500)
                olc(c,'500-tcp',once)
                http_yuz(c,p,s,tcp)
            else:
                bas=time.monotonic()
                sha=baglanti(tcp,0,100*1024*1024,q)
                wait_for(p,lambda:c.sayi('ag_tcp_bitti')>=1)
                olc(c,'200-MiB-toplam',once)
                print('AG18 iki yön 100+100 MiB SHA256',sha,'saniye',round(time.monotonic()-bas,2),flush=True)
            c.expected=c.output()
            c.shutdown()


if __name__=='__main__':
    a=argparse.ArgumentParser()
    a.add_argument('dizin',type=Path)
    a.add_argument('kip',choices=('cerceve','aktarim','zaman'))
    v=a.parse_args()
    if v.kip=='zaman':
        from ag_udp_kahin import kos as kira_sina
        kira_sina(v.dizin.resolve(),False,True)
    else:
        kos(v.dizin.resolve(),v.kip)
