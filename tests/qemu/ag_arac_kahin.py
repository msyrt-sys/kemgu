"""Kullanıcı ağ araçlarını gerçek borular, slirp, QMP ve Ethernet kaydıyla sınar."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import hashlib
import re
import socket
import struct
import sys

from boru_kahin import BoruConsole
from dalga12_kahin import boot, fsck, wait_for
from dalga12_fat import Fat
from pcap_oku import pcap_kayitlari
from surec_imaj import imajlar
from surec_kahin import SurecConsole

MAC = bytes.fromhex('525400170011')
VERI = bytes((i*37+i//251)&255 for i in range(1<<20))
YANIT = bytes(b^165 for b in VERI)


def komut(c, metin, sure=35, es=None):
    ilk = c.ser.stat().st_size
    c.input(metin)
    def bitti():
        if es is not None and es.done():
            es.result()
        s = c.ser.read_bytes()[ilk:]
        return s if s.endswith(b'KABUK_HAZIR\n') else None
    s = wait_for(c.p, bitti, seconds=sure)
    assert b'HATA' not in s and b'nc:' not in s, (metin,s)
    return s


def dinleyici_bekle(c, port):
    # SYN göndermeden önce o porttaki gerçek kullanıcı soketinin LISTEN kaydını gör.
    tablo = c.sayi('sk_alan')
    def dinliyor():
        for i in range(16):
            ref, tur, durum, yerel = struct.unpack('<Qiii', c.mem(tablo + 53248*i, 20))
            if ref and (tur, durum, yerel) == (1, 3, port):
                return True
        return False
    wait_for(c.p, dinliyor, seconds=5)


def tcp_es(s):
    conn, _ = s.accept()
    with conn:
        conn.settimeout(40)
        veri = bytearray()
        while len(veri)<len(VERI):
            b = conn.recv(min(65536,len(VERI)-len(veri)))
            assert b, 'istemci erken FIN gönderdi'
            veri.extend(b)
        assert veri==VERI
        conn.settimeout(3)
        try:
            assert conn.recv(1)==b'', 'dosya sonundan sonra veri geldi'
        except TimeoutError as exc:
            raise AssertionError('standart girdi bitti, TCP FIN gelmedi') from exc
        conn.settimeout(40)
        conn.sendall(YANIT)
        conn.shutdown(socket.SHUT_WR)
        return hashlib.sha256(veri).hexdigest()


def udp_es(s):
    b, a = s.recvfrom(4096)
    assert b==b'udpdeney\n', b
    s.sendto(b, a)


def pcap_akim(paketler, port, giden, mac=MAC):
    sec = [p for p in paketler if p.protokol==6 and
           (p.kaynak==mac and p.hedef_port==port if giden else p.hedef==mac and p.kaynak_port==port)]
    syn = next(p for p in sec if p.bayrak&2)
    birikim = bytearray()
    for p in sec:
        b = p.yuk[4*(p.yuk[12]>>4):]
        if b:
            ofs = (p.sira-syn.sira-1)&0xffffffff
            assert ofs<=len(birikim), (ofs,len(birikim))
            ilk = min(len(b),len(birikim)-ofs)
            assert birikim[ofs:ofs+ilk]==b[:ilk]
            birikim.extend(b[ilk:])
    assert any(p.bayrak&1 for p in sec)
    return bytes(birikim)


def kos(w):
    progs = {a+'.elf':(w/(a+'.elf')).read_bytes() for a in ('nc','ag','agakim','echo','cozumle')}
    usb,nvme,sata = imajlar(w,progs)
    pcap = w/'arac-slirp.pcap'
    with socket.socket() as tcp, socket.socket(type=socket.SOCK_DGRAM) as udp:
        tcp.bind(('127.0.0.1',0)); tcp.listen(1); tcp.settimeout(90)
        udp.bind(('127.0.0.1',0)); udp.settimeout(90)
        tp,up = tcp.getsockname()[1],udp.getsockname()[1]
        net = ('-netdev','user,id=n0,hostfwd=tcp:127.0.0.1:0-10.0.2.15:9121',
               '-device',f'e1000,id=nic0,netdev=n0,mac={MAC.hex(":")}',
               '-object',f'filter-dump,id=f0,netdev=n0,file={pcap}')
        with boot(w,usb[0],nvme,sata,ag_aygitlari=net) as (q,p,seri), ThreadPoolExecutor(2) as havuz:
            c = BoruConsole(w,q,p,seri)
            qmp = q.call('human-monitor-command',{'command-line':'info network'})
            assert MAC.hex(':') in qmp
            # Dinleme portunu QEMU ayırır. İki paralel koşum arasında rezervasyon boşluğu kalmaz.
            yon=q.call('human-monitor-command',{'command-line':'info usernet'})
            esles=re.search(r'TCP\[HOST_FORWARD\]\s+\d+\s+127\.0\.0\.1\s+(\d+)\s+10\.0\.2\.15\s+9121\b',yon)
            assert esles,yon
            ileri=int(esles[1])
            once = c.heap(), c.bos_cerceve()
            wait_for(p,lambda:int.from_bytes(c.mem(c.sayi('ag_l5'),4),'little')==3,seconds=15)
            assert b'DHCP 3 ip 10.0.2.15' in komut(c,'ag durum')
            a = komut(c,'ag arayuzler')
            for alan in (b'e1000 MAC '+MAC.hex(':').encode()+b' bag 1',b'lo MAC 00:00:00:00:00:00',
                         b'IPv4 10.0.2.15/24',b'gecit 10.0.2.2',b'DNS 10.0.2.3'):
                assert alan in a, (alan,a)
            once_say = [int(v) for v in re.findall(rb'RX TX (\d+) (\d+)',a)[0]]
            assert b'iki\n' in komut(c,'echo iki | agakim satir')
            sunucu = havuz.submit(tcp_es,tcp)
            s = komut(c,f'agakim uret | nc -z 30000 10.0.2.2 {tp} | agakim al',90,sunucu)
            assert b'AKIM 1048576 OK\n' in s, s
            assert sunucu.result(5)==hashlib.sha256(VERI).hexdigest()
            yank = havuz.submit(udp_es,udp)
            assert b'udpdeney\n' in komut(c,f'echo udpdeney | nc -u -z 5000 10.0.2.2 {up}')
            yank.result(3)
            assert b'10.0.2.2 ' in komut(c,'ag arp')
            assert b'127.0.0.1' in komut(c,'cozumle 127.0.0.1')
            assert b'IPv4 10.0.2.15' in (a:=komut(c,'ag'))
            son_say = [int(v) for v in re.findall(rb'RX TX (\d+) (\d+)',a)[0]]
            assert all(y>x for x,y in zip(once_say,son_say))
            # Gerçek kullanıcı dinleyicisinin stdout'u dosyaya yönlendirilir.
            komut(c,'bagla --yazilabilir usb0p2')
            komut(c,'cd /diskler/usb0p2/bin')
            ilk = seri.stat().st_size
            c.input('nc -d -z 10000 9121 > gelen.bin')
            dinleyici_bekle(c, 9121)
            with socket.create_connection(('127.0.0.1',ileri),timeout=6) as istemci:
                istemci.sendall(VERI[:3072]); istemci.shutdown(socket.SHUT_WR)
                assert istemci.recv(1)==b''
            wait_for(p,lambda:b'KABUK_HAZIR\n' in seri.read_bytes()[ilk:],seconds=12)
            ilk = seri.stat().st_size
            SurecConsole.input(c,'baslat nc -d -z 15000 9123 &')
            wait_for(p,lambda:b'KABUK_HAZIR\n' in seri.read_bytes()[ilk:])
            dinleyici_bekle(c, 9123)
            ilk = seri.stat().st_size
            komut(c,'echo yerelveri | nc -z 6000 127.0.0.1 9123')
            # Ön plan istemcisinin dönüşü ve arka plan dinleyicisinin stdout'u ayrı olaylardır.
            wait_for(p, lambda: b'yerelveri\n' in seri.read_bytes()[ilk:], seconds=5)
            # Komut istemi istemci bittiğinde gelir. Arka plandaki dinleyici
            # son FIN'i bir sonraki zamanlayıcı diliminde tüketebilir.
            wait_for(p,lambda:c.bos_cerceve()==once[1],seconds=5)
            sonuc = seri.read_bytes()[ilk:]
            assert sonuc.count(b'yerelveri\n') == 1 and b'nc:' not in sonuc, sonuc
            son = c.heap(),c.bos_cerceve()
            assert once[1]==son[1], (once,son)
            print('araç heap/boş çerçeve',once,son,flush=True)
            c.expected=c.output()
            c.shutdown()
    fsck(w,usb[1],'ag-arac')
    assert Fat(usb[1].read()).get('/BIN/gelen.bin')==VERI[:3072]
    with pcap.open('rb') as f:
        paketler=list(pcap_kayitlari(f))
    assert pcap_akim(paketler,tp,True)==VERI
    assert pcap_akim(paketler,tp,False)==YANIT
    assert not any(p.ip_kaynak[:1]==b'\x7f' or p.ip_hedef[:1]==b'\x7f' for p in paketler)
    assert any(p.protokol==17 and p.hedef_port==up and p.yuk[8:]==b'udpdeney\n' for p in paketler)
    print('✓ nc iki yön boru 1 MiB SHA256',hashlib.sha256(VERI).hexdigest(),hashlib.sha256(YANIT).hexdigest(),
          'UDP, FAT32, loopback, QMP ve pcap',flush=True)


if __name__=='__main__':
    kos(Path(sys.argv[1]).resolve())
