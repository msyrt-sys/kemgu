"""Sırası değişmiş IPv4 parçalarını, eksik veri zaman aşımını ve pcap'i denetler."""
import argparse
import hashlib
from pathlib import Path
import time

from ag_es import EthernetEs, ES_IP, ES_MAC, KONUK_IP, ethernet, ipv4, parcalara_ayir, saglama, udp
from dalga12_kahin import boot, wait_for
from pcap_oku import cerceve_oku, pcap_kayitlari, saglama_gecerli
from surec_imaj import imajlar
from surec_kahin import SurecConsole

MAC = bytes.fromhex('525400170002')


def birlestir(parcalar, kaynak, hedef):
    ilk = parcalar[0]
    assert len({p.kimlik for p in parcalar}) == 1
    sira = sorted(parcalar, key=lambda p: p.parca_ofseti)
    paket = bytearray()
    for i, p in enumerate(sira):
        assert (p.ip_kaynak,p.ip_hedef,p.protokol)==(kaynak,hedef,17)
        assert p.parca_ofseti == len(paket) and p.devami == (i<len(sira)-1)
        assert len(p.yuk)<=1480 and (not p.devami or len(p.yuk)%8==0)
        paket.extend(p.yuk)
    sozde = kaynak+hedef+b'\x00\x11'+len(paket).to_bytes(2,'big')
    assert saglama_gecerli(sozde+paket) and int.from_bytes(paket[4:6],'big')==len(paket)
    assert ilk.kaynak == MAC and ilk.hedef == ES_MAC
    return bytes(paket)


def kos(w):
    disk,nvme,sata=imajlar(w,{})
    pcap=w/'parca.pcap'
    with EthernetEs() as es:
        aygit=('-netdev',es.qemu_netdev(),'-device',f'e1000,id=ag0,netdev=n0,mac={MAC.hex(":")}',
               '-object',f'filter-dump,id=f0,netdev=n0,file={pcap}')
        with boot(w,disk[0],nvme,sata,ag_aygitlari=aygit) as (q,p,seri):
            c=SurecConsole(w,q,p,seri)
            c.run('ag ayarla 192.0.2.2/24 192.0.2.1',b'AG AYAR OK\n')
            c.run('ag yanki',b'UDP yanki 9000\n')
            son=time.monotonic()+5
            while True:
                garp=cerceve_oku(es.al(max(.001,son-time.monotonic())))
                if garp.tur==2054 and garp.kaynak==MAC and garp.yuk[24:28]==KONUK_IP: break
                assert time.monotonic()<son, 'Konuk ARP duyurusu eksik'
            heap,bos=c.heap(),c.bos_cerceve()
            yuk=bytes((i*73+i//251)%256 for i in range(8192))
            kimlikler=[317,318,319]
            once=c.sayi('ag_rx_sayi')
            def yolla(kim, dizi):
                nonlocal once
                tel=parcalara_ayir(ES_IP,KONUK_IP,17,udp(ES_IP,KONUK_IP,17777,9000,yuk),1500,kim)
                for i in dizi:
                    es.gonder(ethernet(MAC,ES_MAC,2048,tel[i]))
                    once+=1
                    wait_for(p,lambda: c.sayi('ag_rx_sayi')>=once,seconds=3)
            def yankiyi_al():
                gelen=[]
                son=time.monotonic()+3
                while len(gelen)<6:
                    kalan=son-time.monotonic()
                    assert kalan>0,'8 KiB UDP yanıti eksik'
                    try:
                        f=cerceve_oku(es.al(kalan))
                    except TimeoutError as exc:
                        raise AssertionError('8 KiB UDP yanıtı eksik') from exc
                    if f.kaynak==MAC and f.protokol==17 and f.ip_hedef==ES_IP: gelen.append(f)
                assert birlestir(gelen,KONUK_IP,ES_IP)==udp(KONUK_IP,ES_IP,9000,17777,yuk)
                return gelen[0].kimlik
            yolla(kimlikler[0],[5,2,0,0,1,4,3])
            ilk=yankiyi_al()
            yolla(kimlikler[1],[5,0,4,2,1])
            wait_for(p,lambda: c.sayi('ag_parca_atilan')==1,seconds=6)
            assert c.sayi('ag_parca_birlesen')==1
            yolla(kimlikler[2],[1,2,3,4,5,0])
            ikinci=yankiyi_al()
            assert ilk!=ikinci and (c.heap(),c.bos_cerceve())==(heap,bos)
            dusen=bytearray(ipv4(ES_IP,KONUK_IP,17,udp(ES_IP,KONUK_IP,17777,9000,b'ttl'),kimlik=320))
            dusen[8]=0
            dusen[10:12]=bytes(2)
            dusen[10:12]=saglama(dusen[:20]).to_bytes(2,'big')
            es.gonder(ethernet(MAC,ES_MAC,2048,dusen))
            wait_for(p,lambda: c.sayi('ag_rx_sayi')>=once+1,seconds=3)
            def sayac(b):
                assert b'cakisma ttl icmp_hata 0 1 0\n' in b and b'PARCA birlesen atilan giden 2 1 12\n' in b,b
            c.run('ag sayac',sayac)
            def donanim(b):
                assert b'PARCA birlesen atilan giden 2 1 12\n' in b,b
            c.run('donanim',donanim)
            c.run('ag parca',b'PARCA birlesen atilan giden 2 1 12\n')
            c.shutdown()
        with pcap.open('rb') as f:
            tel=list(pcap_kayitlari(f))
        for kim in (ilk,ikinci):
            ham=birlestir([p for p in tel if p.kaynak==MAC and p.protokol==17 and p.kimlik==kim],KONUK_IP,ES_IP)
            assert hashlib.sha256(ham[8:]).digest()==hashlib.sha256(yuk).digest()
        assert sum(p.kaynak==MAC and p.protokol==17 and p.ip_hedef==ES_IP for p in tel)==12
        assert [len([p for p in tel if p.kaynak==ES_MAC and p.kimlik==kim]) for kim in kimlikler]==[7,5,6]
        print(f'PARCA OK 8192 bayt SHA256={hashlib.sha256(yuk).hexdigest()} iki yanıt, süre aşımı 1, heap={heap} boş={bos}')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('dizin',type=Path)
    kos(parser.parse_args().dizin.resolve())
