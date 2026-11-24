"""Gerçek kullanıcı ELF'i, teldeki ARP/ICMP paketleri ve bağımsız pcap okuması."""
import argparse
import contextlib
from pathlib import Path
import re
import struct
import threading
import time

from ag_es import ES_IP, ES_MAC, KONUK_IP, EthernetEs, arp, ethernet, ipv4, yankilama
from dalga12_kahin import boot
from pcap_oku import cerceve_oku, pcap_kayitlari
from surec_imaj import imajlar
from surec_kahin import SurecConsole

MAC = bytes.fromhex('525400170004')


def ping(c, ip, adet):
    def bak(cikti):
        assert f'PING alinan {adet} / {adet}\n'.encode() in cikti, cikti
        satirlar = re.findall(rb'^PING (\d+) us (\d+)$', cikti, re.M)
        assert [int(i) for i, _ in satirlar] == list(range(adet)), cikti
        sureler = [int(t) for _, t in satirlar]
        s = re.search(rb'RTT us min ort max (\d+) (\d+) (\d+)', cikti)
        assert s and [int(t) for t in s.groups()] == [min(sureler), sum(sureler)//adet,max(sureler)]
    c.run(f'ping {ip} {adet} 0 1000',bak)


def kos(w, slirp=False):
    usb,nvme,sata=imajlar(w, {'ping.elf':(w/'ping.elf').read_bytes()})
    pcap=w/'l3.pcap'
    with (contextlib.nullcontext() if slirp else EthernetEs()) as es:
        net = 'user,id=n0' if slirp else es.qemu_netdev()
        args=('-netdev',net,'-device',f'e1000,id=ag0,netdev=n0,mac={MAC.hex(":")}',
              '-object',f'filter-dump,id=f0,netdev=n0,file={pcap}')
        with boot(w,usb[0],nvme,sata,ag_aygitlari=args) as (q,p,seri):
            c=SurecConsole(w,q,p,seri)
            ip,gecit=('10.0.2.15','10.0.2.2') if slirp else ('192.0.2.2','192.0.2.1')
            c.run(f'ag ayarla {ip}/24 {gecit}',b'AG AYAR OK\n')
            if slirp:
                ping(c,gecit,2)
            else:
                def oku(kosul, sure=5):
                    son=time.monotonic()+sure
                    while time.monotonic()<son:
                        paket=cerceve_oku(es.al(max(.001,son-time.monotonic())))
                        if kosul(paket):
                            return paket
                    raise AssertionError('beklenen Ethernet çerçevesi yok')
                garp=oku(lambda a: a.tur==2054 and a.yuk[14:18]==KONUK_IP and a.yuk[24:28]==KONUK_IP)
                assert garp.kaynak == MAC and garp.hedef == bytes([255])*6
                komsu = bytes.fromhex('025159170003')
                komsu_ip = bytes((192,0,2,3))
                es.gonder(ethernet(bytes([255])*6,komsu,2054,arp(1,komsu,komsu_ip,bytes(6),KONUK_IP)))
                cevap=oku(lambda a: a.tur==2054 and a.yuk[6:8]==b'\0\2')
                assert cevap.hedef==komsu and cevap.yuk[:28]==arp(2,MAC,KONUK_IP,komsu,komsu_ip), 'ARP hedef MAC'
                heap,bos=c.heap(),c.bos_cerceve()
                for i in range(100):
                    yuk=struct.pack('!I',i)*8
                    ham=ethernet(MAC,ES_MAC,0x0800,ipv4(ES_IP,KONUK_IP,1,yankilama(8,751,i,yuk)))
                    es.gonder(ham)
                    try:
                        yanit=oku(lambda a: a.protokol==1 and a.icmp_tur==0 and a.ip_hedef==ES_IP)
                    except TimeoutError as exc:
                        alan=c.sayi('ag_l3_alan')
                        raise AssertionError((i,c.sayi('ag_rx_sayi'),c.sayi('ag_tx_sayi'),c.mem(alan,136).hex(), c.mem(alan+4096,74*4)[::4].hex(),c.mem(alan+12288,74*4)[::4].hex())) from exc
                    assert (yanit.kaynak,yanit.hedef,yanit.yuk[4:]) == (MAC,ES_MAC,struct.pack('!HH',751,i)+yuk)
                assert c.bos_cerceve()==bos
                print(f'100 ping heap {heap}→{c.heap()} boş {bos}→{c.bos_cerceve()}', flush=True)
                bitti=threading.Event()
                sorun=[]
                def yansit():
                    try:
                        while not bitti.is_set():
                            try:
                                ham=es.al(.1)
                            except TimeoutError:
                                continue
                            yanit=es.yanit(ham)
                            if yanit is not None:
                                es.gonder(yanit)
                    except Exception as e:
                        sorun.append(e)
                t=threading.Thread(target=yansit)
                t.start()
                try:
                    ping(c,gecit,20)
                    once_pcap=pcap.stat().st_size
                    ping(c,'127.0.0.1',3)
                    assert pcap.stat().st_size==once_pcap,'loopback sırasında dış trafik'
                    time.sleep(31)
                    ping(c,gecit,1)
                finally:
                    bitti.set()
                    t.join()
                assert not sorun, sorun
            c.shutdown()
        with pcap.open('rb') as f:
            kayit=list(pcap_kayitlari(f))
        # Kimlik, sıra ve yük eşleşmesini pcap'ten iki yönde yeniden çıkar.
        istek=[v for v in kayit if v.protokol==1 and v.icmp_tur==8 and v.kaynak==MAC]
        yanit=[v for v in kayit if v.protokol==1 and v.icmp_tur==0 and v.hedef==MAC]
        assert len(istek)==len(yanit)==(2 if slirp else 21)
        assert [v.yuk[4:] for v in istek]==[v.yuk[4:] for v in yanit]
        if not slirp:
            sorgu=[v for v in kayit if v.tur==2054 and v.kaynak==MAC and v.yuk[6:8]==b'\0\1' and v.yuk[24:28]==ES_IP]
            assert len(sorgu)==2,len(sorgu)
            assert sum(v.protokol==1 and v.icmp_tur==0 and v.kaynak==MAC for v in kayit)==100
        print('L3 OK', 'slirp' if slirp else 'dgram', len(kayit), flush=True)


if __name__=='__main__':
    a=argparse.ArgumentParser(description=__doc__)
    a.add_argument('dizin',type=Path)
    a.add_argument('--slirp',action='store_true')
    arg=a.parse_args()
    kos(arg.dizin.resolve(),arg.slirp)
