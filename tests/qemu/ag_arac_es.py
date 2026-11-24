"""DNS, komşu tablosu, açık TCP uçları ve NIC sayaçları için bağımsız ham Ethernet eşi."""
from pathlib import Path
from queue import Queue
import re
import sys
import threading

from ag_arac_kahin import komut, MAC
from ag_es import EthernetEs, TcpEs, ES_IP, ES_MAC, KONUK_IP, dns_a_yanit, ethernet, ipv4, udp
from dalga12_kahin import boot, wait_for
from pcap_oku import cerceve_oku, pcap_kayitlari
from surec_imaj import imajlar
from surec_kahin import SurecConsole


def kos(w):
    disk,nvme,sata=imajlar(w,{ad+'.elf':(w/(ad+'.elf')).read_bytes() for ad in ('ag','nc','cozumle')})
    udp_yanit=Queue()
    tcp_gelen=Queue()
    hatalar=[]
    pcap=w/'arac-dgram.pcap'
    with EthernetEs() as hat:
        dur=threading.Event()
        def tel():
            try:
                while not dur.is_set():
                    try: ham=hat.al(.1)
                    except TimeoutError: continue
                    v=cerceve_oku(ham)
                    if v.kaynak!=MAC: continue
                    if v.protokol==17 and v.hedef_port==53:
                        b=v.yuk[8:]
                        assert b[12:]==b'\x05ornek\x04test\0\0\1\0\1'
                        yan=dns_a_yanit(b,bytes((198,51,100,17)),60)
                        hat.gonder(ethernet(MAC,ES_MAC,2048,ipv4(ES_IP,KONUK_IP,17,udp(ES_IP,KONUK_IP,53,v.kaynak_port,yan))))
                    elif v.protokol==6: tcp_gelen.put(v)
                    elif v.protokol==17 and v.hedef_port==18811: udp_yanit.put(v.yuk[8:])
                    else:
                        cevap=hat.yanit(ham)
                        if cevap: hat.gonder(cevap)
            except Exception as exc: hatalar.append(exc)
        t=threading.Thread(target=tel)
        t.start()
        try:
            net=('-netdev',hat.qemu_netdev(),'-device',f'e1000,id=n,netdev=n0,mac={MAC.hex(":")}',
                 '-object',f'filter-dump,id=f0,netdev=n0,file={pcap}')
            with boot(w,disk[0],nvme,sata,ag_aygitlari=net) as (q,p,seri):
                c=SurecConsole(w,q,p,seri)
                wait_for(p,lambda:int.from_bytes(c.mem(c.sayi('ag_l5'),4),'little')==3,seconds=15)
                once=c.heap(),c.bos_cerceve()
                assert b'198.51.100.17\n' in komut(c,'cozumle ornek.test')
                assert b'192.0.2.1 '+ES_MAC.hex(':').encode() in komut(c,'ag arp')
                komut(c,'ag yanki')
                def say():
                    s=komut(c,'ag arayuzler')
                    for y in (b'IPv4 192.0.2.2/24',b'gecit 192.0.2.1',b'DNS 192.0.2.1',b'NTP 192.0.2.1'): assert y in s
                    return tuple(map(int,re.search(rb'RX TX (\d+) (\d+)',s).groups()))
                once_say=say()
                for i in range(8):
                    veri=b'ARACSAY'+bytes([i])
                    hat.gonder(ethernet(MAC,ES_MAC,2048,ipv4(ES_IP,KONUK_IP,17,udp(ES_IP,KONUK_IP,18811,9000,veri))))
                    assert udp_yanit.get(timeout=3)==veri
                assert say()==(once_say[0]+8,once_say[1]+8)
                assert b'pid=' in komut(c,'baslat nc -d -z 20000 9143 &')
                def dinliyor():
                    alan=c.sayi('sk_alan')
                    # Eş SYN'i, kullanıcı soketi dinlemeye geçince gönderir.
                    return int.from_bytes(c.mem(alan+12,4),'little')==3
                wait_for(p,dinliyor,seconds=5)
                assert b'0.0.0.0:9143 0.0.0.0:0 durum 1' in komut(c,'ag baglantilar')
                es=TcpEs(hat,MAC)
                es.yolla(28811,9143,2048,0,2)
                v=tcp_gelen.get(timeout=3); assert (v.bayrak,v.onay)==(18,2049)
                ack=v.sira+1
                es.yolla(28811,9143,2049,ack,16)
                s=komut(c,'ag baglantilar')
                assert re.search(rb'192\.0\.2\.2:9143 192\.0\.2\.1:28811 durum [456]\nTX RX kuyruk 0 0\n',s),s
                es.yolla(28811,9143,2049,ack,17)
                while True:
                    v=tcp_gelen.get(timeout=4)
                    if v.bayrak&1:
                        es.yolla(28811,9143,2050,v.sira+1,16); break
                wait_for(p,lambda:c.bos_cerceve()==once[1],seconds=8)
                ilk=seri.stat().st_size
                c.input('nc -z 200 192.0.2.1 9148')
                wait_for(p,lambda:b'KABUK_HAZIR\n' in seri.read_bytes()[ilk:],seconds=4)
                hata=seri.read_bytes()[ilk:]
                assert b'nc: baglanti -110\n' in hata and b'cikis=1]' in hata,hata
                print('araç eş heap/boş çerçeve',once,(c.heap(),c.bos_cerceve()),flush=True)
        finally:
            dur.set();t.join(3)
        assert not hatalar and not t.is_alive(),hatalar
    with pcap.open('rb') as f: paketler=list(pcap_kayitlari(f))
    assert sum(p.protokol==17 and p.hedef_port==18811 for p in paketler)==8
    assert sum(p.protokol==17 and p.hedef_port==53 for p in paketler)==1
    assert any(p.protokol==6 and p.kaynak_port==9143 and p.hedef_port==28811 and p.bayrak==18 for p in paketler)
    print('✓ DNS kullanıcı ELF, ARP, TCP tablosu ve eşte RX/TX tam +8')


if __name__=='__main__':
    kos(Path(sys.argv[1]).resolve())
