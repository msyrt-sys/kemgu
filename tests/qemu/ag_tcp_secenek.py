"""TCP uzantılarının tel değerleri, gecikmeli ACK, canlılık ve 64 MiB aktarım."""
import argparse
import hashlib
import heapq
import os
from pathlib import Path
import struct
import time

from ag_es import ES_IP, KONUK_IP, EthernetEs, TcpEs
from ag_tcp_kahin import MAC, MASKE, MSS
from dalga12_kahin import boot, wait_for
from pcap_oku import cerceve_oku, pcap_kayitlari, tcp_akimi
from surec_imaj import imajlar
from surec_kahin import SurecConsole


def secenekler(p):
    h=4*(p.yuk[12]>>4)
    k=20
    out={}
    while k<h:
        tip=p.yuk[k]
        if tip==0: break
        if tip==1:
            k+=1;continue
        n=p.yuk[k+1]
        assert 2<=n<=h-k and tip not in out
        out[tip]=p.yuk[k+2:k+n];k+=n
    return out


def ts(say,yanki):
    return b'\1\1\x08\x0a'+struct.pack('!II',say&MASKE,yanki&MASKE)


def zaman(p):
    return struct.unpack('!II',secenekler(p)[8])


def cpu_s(p):
    path=Path(f'/proc/{p.pid}/stat')
    if not path.exists(): return None
    a=path.read_text().split(') ',1)[1].split()
    return (int(a[11])+int(a[12]))/os.sysconf('SC_CLK_TCK')


def secenek(es,c,p):
    sp,dp=22000,9001
    client_isn=914
    es.yolla(sp,dp,client_isn,0,2,win=32000,sec=MSS+b'\1\3\3\3'+ts(811,0))
    cevap,b=es.al()
    sec=secenekler(cevap)
    assert cevap.bayrak==18 and sec[2]==MSS[2:] and sec[3]==b'\2' and zaman(cevap)[1]==811
    assert int.from_bytes(cevap.yuk[14:16],'big')==65535
    ack=(cevap.sira+1)&MASKE
    seq=client_isn+1
    es.yolla(sp,dp,seq,ack,16,sec=ts(812,zaman(cevap)[0]))
    ilk=time.monotonic()
    es.yolla(sp,dp,seq,ack,24,b'X',sec=ts(813,zaman(cevap)[0]));seq+=1
    cevap,b=es.al(1.5); gecikme=time.monotonic()-ilk
    assert not b and cevap.bayrak==16 and cevap.onay==seq and zaman(cevap)[1]==813
    assert .008<=gecikme<=.65,gecikme
    pencere=int.from_bytes(cevap.yuk[14:16],'big')<<2
    assert 131067<=pencere<=131071,pencere
    # Eşin sessiz kaldığı açık bağlantıda üç ayrı yoklama ve sonrasında kapanış.
    for _ in range(3):
        cevap,b=es.al(2.5)
        assert cevap.bayrak==16 and cevap.sira==(ack-1)&MASKE and not b
    wait_for(p,lambda:c.sayi('ag_tcp_rst')==1 and c.sayi('ag_tcp_yoklama')==3,seconds=3)
    return gecikme,pencere


def verim(es,adet,olcek):
    p,b=es.al()
    sec=secenekler(p)
    assert p.bayrak==2 and 8 in sec and ((3 in sec)==olcek)
    port=p.kaynak_port
    bas=(p.sira+1)&MASKE
    seq=60000
    bas_ms=time.monotonic()
    peer_ts=lambda: 500+int((time.monotonic()-bas_ms)*1000)
    secenek=MSS+(b'\1\3\3\2' if olcek else b'')+ts(peer_ts(),zaman(p)[0])
    es.yolla(9001,port,seq-1,bas,18,win=65535,sec=secenek)
    elde=0
    digest=hashlib.sha256()
    bekleyen=[]
    dongu=bytes((i*37+i//251)&255 for i in range(251*256))
    desen=dongu+dongu
    while True:
        gecen=time.monotonic()-bas_ms
        assert gecen<256, (f'64 MiB aktarımı 0.25 MiB/s eşiğinin altında: '
                           f'{elde}/{adet} bayt, {gecen:.3f} sn, {elde/1048576/gecen:.3f} MiB/s')
        p,b=es.al(8)
        stamp,yanki=zaman(p)
        assert p.hedef_port==9001 and p.onay==seq and yanki>=500
        ofs=(p.sira-bas)&MASKE
        if b:
            # Alıcı ilan ettiği pencereye sığan sıra dışı segmenti saklar.
            # ACK yalnız boşluksuz alınan son baytı izler.
            assert ofs+len(b)<=min(adet, elde+(65535 << (2 if olcek else 0)))
            j=ofs%len(dongu)
            assert b==desen[j:j+len(b)], ("TCP yükü farklı",ofs)
            heapq.heappush(bekleyen,(ofs,b))
            while bekleyen and bekleyen[0][0]<=elde:
                baslangic,veri=heapq.heappop(bekleyen)
                if baslangic+len(veri)>elde:
                    digest.update(veri[elde-baslangic:])
                    elde=baslangic+len(veri)
            es.yolla(9001,port,seq,bas+elde,16,win=65535,sec=ts(peer_ts(),stamp))
        if p.bayrak&1:
            assert ofs==adet and elde==adet,(ofs,elde)
            es.yolla(9001,port,seq,bas+elde+1,17,win=65535,sec=ts(peer_ts(),stamp))
            son,b=es.al()
            assert son.bayrak==16 and son.onay==seq+1
            break
    sure=time.monotonic()-bas_ms
    assert not bekleyen
    beklenen=hashlib.sha256()
    for i in range(0,adet,len(dongu)): beklenen.update(dongu[:min(len(dongu),adet-i)])
    assert digest.digest()==beklenen.digest()
    return sure,digest.hexdigest()


def kos(w,kip):
    disk,nvme,sata=imajlar(w,{})
    pcap=w/f'tcp-{kip}.pcap'
    with EthernetEs() as hat:
        net=('-netdev',hat.qemu_netdev(),'-device',f'e1000,id=ag0,netdev=n0,mac={MAC.hex(":")}',
             '-object',f'filter-dump,id=f0,netdev=n0,file={pcap}')
        with boot(w,disk[0],nvme,sata,ag_aygitlari=net) as (q,p,seri):
            c=SurecConsole(w,q,p,seri)
            c.run('ag ayarla 192.0.2.2/24 192.0.2.1',b'AG AYAR OK\n')
            while True:
                g=cerceve_oku(hat.al(3))
                if g.tur==2054 and g.yuk[24:28]==KONUK_IP: break
            heap,bos=c.heap(),c.bos_cerceve()
            es=TcpEs(hat,MAC)
            if kip=='secenek':
                c.run('ag tcp sessiz',b'TCP sessiz 9001\n')
                gecikme,pencere=secenek(es,c,p)
            else:
                olcek=kip=='olcekli'
                if not olcek: c.run('ag tcp olcek kapat',b'TCP olcek kapali\n')
                c.run('ag tcp 64m',b'TCP 67108864\n')
                c.run('ag tcp bagla 192.0.2.1',b'TCP baglanti basladi\n')
                ilk=cpu_s(p)
                sure,h=verim(es,64*1024*1024,olcek)
                cpu=None if ilk is None else cpu_s(p)-ilk
                wait_for(p,lambda:c.sayi('ag_tcp_bitti')==1,seconds=9)
            assert (c.heap(),c.bos_cerceve())==(heap,bos)
            c.shutdown()
        with pcap.open('rb') as f:
            paketler=pcap_kayitlari(f)
            if kip=='secenek':
                assert sum(v.protokol==6 and v.kaynak==MAC for v in paketler)>=5
            else:
                tel=tcp_akimi(paketler,9001,True,MAC)
                assert len(tel)==64*1024*1024 and hashlib.sha256(tel).hexdigest()==h
        if kip=='secenek':
            print(f'TCP SECENEK OK ACK {gecikme:.3f}s, pencere {pencere}, keepalive=3 heap={heap} boş={bos}',flush=True)
        else:
            assert 64/sure>=.25
            print(f'TCP VERIM OK {kip} 64 MiB süre={sure:.3f}s hız={64/sure:.3f} MiB/s QEMU_CPU={cpu} SHA256={h} heap={heap} boş={bos}',flush=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('dizin',type=Path)
    parser.add_argument('kip',choices=('secenek','olcekli','olceksiz'))
    a=parser.parse_args();kos(a.dizin.resolve(),a.kip)
