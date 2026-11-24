"""DHCP T1/T2, DNS yedeği ve UDP yükünü bağımsız Ethernet eşinde sınar."""
import argparse
from collections import deque
from pathlib import Path
import struct
import threading
import time

from ag_es import EthernetEs, ES_IP, ES_MAC, KONUK_IP, arp, ethernet, ipv4, udp, dhcp_secenekleri, dhcp_yanit, dns_a_yanit
from dalga12_kahin import boot, wait_for
from pcap_oku import cerceve_oku, pcap_kayitlari
from surec_imaj import imajlar
from surec_kahin import SurecConsole

MAC=bytes.fromhex('525400170005')
DNS1=bytes((192,0,2,10))
CEVAP=bytes((198,51,100,17))


def kos(w,slirp):
    usb,nvme,sata=imajlar(w,{})
    arka_uc = 'slirp' if slirp else 'dgram'
    pcap = w/f'udp-{arka_uc}.pcap'
    with EthernetEs() as es:
        ag='user,id=n0' if slirp else es.qemu_netdev()
        qos=('-netdev',ag,'-device',f'e1000,id=ag0,netdev=n0,mac={MAC.hex(":")}',
             '-object',f'filter-dump,id=f0,netdev=n0,file={pcap}')
        dur=threading.Event()
        olay=[]
        dns=[]
        hata=[]
        sessiz=threading.Event()
        nak=threading.Event()
        icmp=[]
        gelen=deque()
        def tel():
            try:
                while not dur.is_set():
                    try:
                        ham=es.al(.1)
                    except TimeoutError:
                        continue
                    v=cerceve_oku(ham)
                    if v.kaynak!=MAC:
                        continue
                    if (v.kaynak_port,v.hedef_port)==(68,67):
                        b=v.yuk[8:]
                        opts=dhcp_secenekleri(b)
                        tur=opts[53][0]
                        assert b[28:34]==MAC and 42 in opts[55]
                        if tur==1 or 50 in opts:
                            ret=tur==3 and nak.is_set()
                            yan=dhcp_yanit(v,ES_IP,ES_MAC,KONUK_IP,8,(DNS1,ES_IP),zorla_nak=ret)
                            if ret:
                                nak.clear()
                                olay.append(('nak',time.monotonic()))
                            if tur==3:
                                assert opts[50]==KONUK_IP and opts[54]==ES_IP
                                olay.append(('ack',time.monotonic()))
                        elif v.ip_hedef==ES_IP:
                            assert v.hedef==ES_MAC and b[12:16]==KONUK_IP and 54 not in opts
                            olay.append(('t1',time.monotonic()))
                            yan=None
                        else:
                            assert v.ip_hedef==bytes([255])*4 and b[12:16]==KONUK_IP and 54 not in opts
                            olay.append(('t2',time.monotonic()))
                            yan=dhcp_yanit(v,ES_IP,ES_MAC,KONUK_IP,10,(DNS1,ES_IP))
                        if yan and not sessiz.is_set():
                            es.gonder(yan)
                    elif v.tur==2054 and v.yuk[6:8]==b'\0\1' and v.yuk[24:28] in (DNS1,ES_IP):
                        ip=v.yuk[24:28]
                        es.gonder(ethernet(MAC,ES_MAC,2054,arp(2,ES_MAC,ip,MAC,KONUK_IP)))
                    elif v.hedef_port==53:
                        dns.append(v.ip_hedef)
                        if v.ip_hedef==ES_IP:
                            cevap=dns_a_yanit(v.yuk[8:],CEVAP,5)
                            es.gonder(ethernet(MAC,ES_MAC,2048,ipv4(ES_IP,KONUK_IP,17,udp(ES_IP,KONUK_IP,53,v.kaynak_port,cevap))))
                    elif v.protokol==1 and (v.icmp_tur,v.icmp_kod)==(3,3):
                        icmp.append(v)
                    elif v.kaynak_port==9000 and v.hedef_port==18000:
                        gelen.append(v.yuk[8:])
            except Exception as exc:
                hata.append(exc)
        t=threading.Thread(target=tel)
        if not slirp:
            t.start()
        try:
            with boot(w,usb[0],nvme,sata,ag_aygitlari=qos) as (q,p,seri):
                c=SurecConsole(w,q,p,seri)
                def durum():
                    a=c.sayi('ag_l5')
                    return a and int.from_bytes(c.mem(a,4),'little')==3
                wait_for(p,durum,seconds=12)
                if slirp:
                    def slirp_durumu(b):
                        assert b.startswith(b'DHCP 3 ip 10.0.2.15 '), b
                    c.run('ag durum',slirp_durumu)
                else:
                    c.run('ag durum',b'DHCP 3 ip 192.0.2.2 dns 192.0.2.10 ntp 192.0.2.1\n')
                    c.run('cozumle ornek.test',b'198.51.100.17\n')
                    assert dns==[DNS1,ES_IP],dns
                    c.run('cozumle ornek.test',b'198.51.100.17\n')
                    assert dns==[DNS1,ES_IP],dns
                    c.run('ag yanki',b'UDP yanki 9000\n')
                    kapali=ipv4(ES_IP,KONUK_IP,17,udp(ES_IP,KONUK_IP,18000,9001,b'kapali'))
                    for _ in range(4):
                        es.gonder(ethernet(MAC,ES_MAC,2048,kapali))
                    wait_for(p,lambda: icmp or hata,seconds=3)
                    assert len(icmp)==1 and icmp[0].yuk[8:]==kapali[:28], icmp
                    bos,heap=c.bos_cerceve(),c.heap()
                    for i in range(1000):
                        yuk=struct.pack('!I',i)*13
                        es.gonder(ethernet(MAC,ES_MAC,2048,ipv4(ES_IP,KONUK_IP,17,udp(ES_IP,KONUK_IP,18000,9000,yuk))))
                        wait_for(p,lambda: gelen or hata,seconds=3)
                        assert not hata and gelen.popleft()==yuk,(i,hata)
                    assert c.bos_cerceve()==bos and c.heap()==heap
                    wait_for(p,lambda: any(k=='t2' for k,_ in olay) or hata,seconds=15)
                    assert not hata,hata
                    assert any(k=='t1' for k,_ in olay), 'T1 yenilemesi yok'
                    zaman={k:next(v for a,v in olay if a==k) for k in ('ack','t1','t2')}
                    t1=zaman['t1']-zaman['ack'];t2=zaman['t2']-zaman['ack']
                    assert 2.5<=t1<=5.8 and 5.5<=t2<=9.8 and t1<t2,(t1,t2)
                    dn_once=len(dns)
                    c.run('cozumle ornek.test',b'198.51.100.17\n')
                    assert dns[dn_once:]==[DNS1,ES_IP], dns
                    sessiz.set()
                    # Bağlantı kapalıyken de kira ve adres zamanında bırakılır.
                    q.call('set_link', {'name':'ag0','up':False})
                    wait_for(p, lambda: int.from_bytes(c.mem(c.sayi('ag_l3_alan')+8,8),'little')==0, seconds=14)
                    q.call('set_link', {'name':'ag0','up':True})
                    sessiz.clear()
                    wait_for(p,durum,seconds=14)
                    nak.set()
                    c.run('ag dhcp',b'DHCP OK\n')
                    wait_for(p,lambda: any(k=='nak' for k,_ in olay), seconds=5)
                    wait_for(p,durum,seconds=12)
                    print(f'1000 UDP heap {heap}→{c.heap()} boş {bos}→{c.bos_cerceve()}, T1 {t1:.3f}s T2 {t2:.3f}s',flush=True)
                c.shutdown()
        finally:
            dur.set()
            if not slirp:
                t.join()
        with pcap.open('rb') as f:
            kayit=list(pcap_kayitlari(f))
        assert any((v.kaynak_port,v.hedef_port)==(67,68) and dhcp_secenekleri(v.yuk[8:]).get(53)==b'\x05' for v in kayit)
        if not slirp:
            assert [v.yuk[8:] for v in kayit if v.kaynak_port==9000]==[struct.pack('!I',i)*13 for i in range(1000)]
        print('UDP OK', 'slirp' if slirp else 'dgram',len(kayit),flush=True)


if __name__=='__main__':
    a=argparse.ArgumentParser(description=__doc__)
    a.add_argument('dizin',type=Path)
    a.add_argument('--slirp',action='store_true')
    args=a.parse_args(); kos(args.dizin.resolve(),args.slirp)
