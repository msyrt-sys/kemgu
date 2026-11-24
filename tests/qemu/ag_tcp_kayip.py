"""Sabit tohumlu iki yönlü hata enjeksiyonu ile gerçek TCP bayt akışını denetler."""
import argparse
import hashlib
from pathlib import Path
import time

from ag_es import ES_IP, ES_MAC, KONUK_IP, EthernetEs, Hat, TcpEs, tcp_paket, ethernet, ipv4
from ag_tcp_kahin import MAC, MASKE, MSS
from dalga12_kahin import boot, wait_for
from pcap_oku import cerceve_oku, pcap_kayitlari
from surec_imaj import imajlar
from surec_kahin import SurecConsole


def aktar(es,adet,sure_siniri):
    p,_=es.al(8)
    assert p.bayrak==2
    port=p.kaynak_port
    al_bas=(p.sira+1)&MASKE
    yaz_bas=0xfffff001
    gelen_hat=Hat(17081,.05,.02,.05)
    giden_hat=Hat(17082,.05,.02,.05)
    pencere=16384
    es.yolla(9001,port,yaz_bas-1,al_bas,18,win=pencere,sec=MSS)
    alinan=bytearray(adet)
    gorulen=bytearray(adet)
    veri=bytes((i*37+i//251)&255 for i in range(adet))
    al=onay=yolla=yeniden=0
    ayni=0
    yuk_bas=zaman=time.monotonic()
    son_ilerleme=zaman
    son_ack=zaman
    fin=False

    def cik(seq,ack,flags,yuk=b''):
        ham=tcp_paket(ES_IP,KONUK_IP,9001,port,yaz_bas+seq,al_bas+ack,flags,pencere,yuk)
        es.kim=(es.kim+1)&65535
        paket=ethernet(MAC,ES_MAC,2048,ipv4(ES_IP,KONUK_IP,6,ham,es.kim))
        for f in giden_hat.uygula(paket): es.hat.gonder(f)

    def isle(p,b):
        nonlocal al,onay,ayni,son_ack,son_ilerleme,yeniden,fin
        if p.bayrak&2:
            es.yolla(9001,port,yaz_bas-1,al_bas,18,win=pencere,sec=MSS)
            return
        assert not p.bayrak&4, 'Konuk RST gönderdi'
        ofs=(p.sira-al_bas)&MASKE
        ack=(p.onay-yaz_bas)&MASKE
        if ack<=yolla and ack>onay:
            onay=ack;ayni=0;son_ack=time.monotonic();son_ilerleme=son_ack
        elif onay==ack and onay<yolla and not b:
            ayni+=1
            if ayni==3:
                cik(onay,al,24,veri[onay:min(yolla,onay+1200)])
                yeniden+=1;son_ack=time.monotonic()
        if b:
            if ofs<adet:
                assert ofs+len(b)<=adet
                alinan[ofs:ofs+len(b)]=b
                gorulen[ofs:ofs+len(b)]=b'\1'*len(b)
                eski=al
                while al<adet and gorulen[al]: al+=1
                if al>eski: son_ilerleme=time.monotonic()
            cik(yolla,al,16)
        if p.bayrak&1:
            assert ofs==adet and onay==adet and not b,(ofs,onay,al)
            fin=True

    while not fin:
        simdi=time.monotonic()
        if simdi-zaman>sure_siniri or simdi-son_ilerleme>35:
            raise AssertionError(f'TCP aktarımı durdu: konuktan={al}, konuğa={onay}, tel={yolla}, süre={simdi-zaman:.3f}')
        if al==adet:
            for _ in range(2):
                n=min(1200,adet-yolla,onay+8400-yolla)
                if n<=0: break
                cik(yolla,al,24,veri[yolla:yolla+n]); yolla+=n
            if onay<yolla and simdi-son_ack>.18:
                # Eşin en eski onaysız verisini tekrar ilet. Konuk RTO'su bağımsızdır.
                cik(onay,al,24,veri[onay:min(yolla,onay+1200)])
                son_ack=simdi;yeniden+=1;ayni=0
        try:
            p,b=es.al(.003 if al==adet else .03)
            for f,icerik in gelen_hat.uygula((p,b)):
                isle(f,icerik)
        except TimeoutError:
            for f,b in gelen_hat.bosalt(): isle(f,b)
            for ham in giden_hat.bosalt(): es.hat.gonder(ham)
    es.yolla(9001,port,yaz_bas+adet,al_bas+adet+1,17)
    bit=time.monotonic()+5
    while True:
        p,b=es.al(max(.01,bit-time.monotonic()))
        if p.onay==(yaz_bas+adet+1)&MASKE and not b: break
        assert time.monotonic()<bit
    assert bytes(alinan)==veri
    sayilar=[(hat.atilan,hat.siralanan,hat.ciftlenen) for hat in (gelen_hat,giden_hat)]
    return hashlib.sha256(alinan).hexdigest(),time.monotonic()-yuk_bas,yeniden,sayilar


def kos(w,kisa=False):
    adet=65536 if kisa else 4*1024*1024
    disk,nvme,sata=imajlar(w,{})
    pcap=w/'tcp-kayip.pcap'
    with EthernetEs() as hat:
        net=('-netdev',hat.qemu_netdev(),'-device',f'e1000,id=ag0,netdev=n0,mac={MAC.hex(":")}',
             '-object',f'filter-dump,id=f0,netdev=n0,file={pcap}')
        with boot(w,disk[0],nvme,sata,ag_aygitlari=net) as (q,p,seri):
            c=SurecConsole(w,q,p,seri)
            c.run('ag ayarla 192.0.2.2/24 192.0.2.1',b'AG AYAR OK\n')
            if not kisa: c.run('ag tcp 4m',b'TCP 4194304\n')
            while True:
                g=cerceve_oku(hat.al(4))
                if g.tur==2054 and g.yuk[24:28]==KONUK_IP: break
            heap,bos=c.heap(),c.bos_cerceve()
            c.run('ag tcp bagla 192.0.2.1',b'TCP baglanti basladi\n')
            h,sure,es_tekrar,sayilar=aktar(TcpEs(hat,MAC),adet,600)
            wait_for(p,lambda:c.sayi('ag_tcp_bitti')==1,seconds=9)
            assert c.sayi('ag_tcp_veri_hatasi')==0 and c.sayi('ag_tcp_okunan')==adet
            tekrar=c.sayi('ag_tcp_tekrar')
            assert 0<tekrar<max(100,adet//2048),tekrar
            assert (c.heap(),c.bos_cerceve())==(heap,bos)
            c.run('ag tcp sayac',f'TCP tekrar red hata {tekrar} 0 0\n'.encode())
            c.shutdown()
        taban={}
        veriler={mac:bytearray(adet) for mac in (MAC,ES_MAC)}
        mevcut={mac:bytearray(adet) for mac in veriler}
        ilk=set();tekrar_tel=0
        with pcap.open('rb') as f:
            for paket in pcap_kayitlari(f):
                if paket.protokol!=6: continue
                mac=paket.kaynak
                if paket.bayrak&2: taban[mac]=(paket.sira+1)&MASKE
                yuk=paket.yuk[4*(paket.yuk[12]>>4):]
                if not yuk: continue
                ofs=(paket.sira-taban[mac])&MASKE
                assert ofs+len(yuk)<=adet
                veriler[mac][ofs:ofs+len(yuk)]=yuk
                mevcut[mac][ofs:ofs+len(yuk)]=bytes([1])*len(yuk)
                if mac==MAC:
                    if paket.sira in ilk: tekrar_tel+=1
                    ilk.add(paket.sira)
        assert tekrar_tel>0
        for mac,veri in veriler.items():
            assert 0 not in mevcut[mac] and hashlib.sha256(veri).hexdigest()==h
        if not kisa: assert all(min(s)>0 for s in sayilar),sayilar
        print(f'TCP KAYIP OK {adet}x2 SHA256={h} süre={sure:.3f}s konuk_tekrar={tekrar} pcap_tekrar={tekrar_tel} eş_tekrar={es_tekrar} kayıp/sıra/çift={sayilar} heap={heap} boş={bos}',flush=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('dizin',type=Path)
    parser.add_argument('--kisa',action='store_true')
    a=parser.parse_args(); kos(a.dizin.resolve(),a.kisa)
