"""Konuğun CMOS güncellemesini QMP RTC, tel zaman damgaları ve kullanıcı programıyla ölçer."""
from protokol_suresi import NTP_ES_GECIKMESI
from datetime import datetime, timezone, timedelta
from pathlib import Path
import re
import struct
import sys
import threading
import time

from ag_arac_kahin import komut, MAC
from ag_es import EthernetEs, ES_IP, ES_MAC, KONUK_IP, ethernet, ipv4, udp, ntp_yanit
from dalga12_kahin import boot, wait_for
from pcap_oku import cerceve_oku, pcap_kayitlari
from surec_imaj import imajlar
from surec_kahin import SurecConsole

ZAMAN=datetime(2031,7,15,12,34,56,tzinfo=timezone.utc)
NS=int(ZAMAN.timestamp())*10**9


def rtc(q):
    d=q.call('qom-get',{'path':'/machine','property':'rtc-time'})
    return datetime(d['tm_year']+1900,d['tm_mon']+1,d['tm_mday'],d['tm_hour'],d['tm_min'],d['tm_sec'],tzinfo=timezone.utc)


def kos(w):
    usb,nvme,sata=imajlar(w,{'saat.elf':(w/'saat.elf').read_bytes()},ntp_otomatik=True)
    pcap=w/'sntp.pcap'
    with EthernetEs() as hat:
        dur=threading.Event(); kayit=[]; hatalar=[]
        def dinle():
            try:
                while not dur.is_set():
                    try: b=hat.al(.08)
                    except TimeoutError: continue
                    v=cerceve_oku(b)
                    if v.kaynak!=MAC: continue
                    if v.protokol==17 and v.hedef_port==123:
                        istek=v.yuk[8:]
                        assert v.ip_hedef==ES_IP and istek[:40]==bytes([35])+bytes(39)
                        fark=len(kayit)*3600*10**9
                        alis=NS+fark
                        # Ağda gecikme oluşturan bağımsız eş, işlem süresini de T3'e işler.
                        time.sleep(NTP_ES_GECIKMESI)
                        yan=ntp_yanit(istek,alis,alis+20_000_000)
                        kayit.append((istek,yan))
                        hat.gonder(ethernet(MAC,ES_MAC,2048,ipv4(ES_IP,KONUK_IP,17,udp(ES_IP,KONUK_IP,123,v.kaynak_port,yan))))
                    else:
                        cev=hat.yanit(b)
                        if cev: hat.gonder(cev)
            except Exception as e: hatalar.append(e)
        t=threading.Thread(target=dinle);t.start()
        try:
            net=('-netdev',hat.qemu_netdev(),'-device',f'e1000,netdev=n0,mac={MAC.hex(":")}',
                 '-object',f'filter-dump,id=f0,netdev=n0,file={pcap}')
            with boot(w,usb[0],nvme,sata,ag_aygitlari=net) as (q,p,seri):
                c=SurecConsole(w,q,p,seri)
                wait_for(p,lambda:b'NTP RTC ' in seri.read_bytes(),seconds=12)
                saat=rtc(q)
                assert abs((saat-ZAMAN).total_seconds())<=2,(saat,ZAMAN)
                once=c.heap(),c.bos_cerceve()
                s=komut(c,'ag ntp 192.0.2.1')
                es=re.search(rb'NTP ofset_us gecikme_us (-?\d+) (\d+)',s)
                assert es,s
                ofset,gecikme=map(int,es.groups())
                assert 20_000<=gecikme<=1_500_000,gecikme
                assert abs(ofset-3600_000_000)<5_000_000,ofset
                saat=rtc(q); hedef=ZAMAN+timedelta(hours=1)
                assert abs((saat-hedef).total_seconds())<=2,(saat,hedef)
                s=komut(c,'baslat saat metin')
                assert re.search(rb'2031-07-15 13:34:5[6-9]',s),s
                son=c.heap(),c.bos_cerceve()
                assert once[1]==son[1],(once,son)
                print('SNTP QMP',saat.isoformat(),'ofset_us',ofset,'gecikme_us',gecikme,'heap/çerçeve',once,son,flush=True)
                c.expected=c.output(); c.shutdown()
        finally:
            dur.set(); t.join(3)
        assert not t.is_alive() and not hatalar,hatalar
    with pcap.open('rb') as f:
        tel=[p for p in pcap_kayitlari(f) if p.protokol==17 and (p.kaynak_port==123 or p.hedef_port==123)]
    assert [p.yuk[8:] for p in tel]==[b for ikili in kayit for b in ikili] and len(kayit)==2
    for i, (istek,yan) in enumerate(kayit):
        assert yan[24:32]==istek[40:48]
        assert struct.unpack_from('!I',yan,32)[0]==int(ZAMAN.timestamp())+3600*i+2208988800
    def mikrosaniye(b,ofs):
        saniye,kesir=struct.unpack_from('!II',b,ofs)
        return (saniye-2208988800)*1_000_000+kesir*1_000_000//(1<<32)
    # T2 - T1 = ileri yol + saat farkı. Gecikme, iki yönün toplamıdır.
    istek,yan=kayit[1]
    etkin=mikrosaniye(yan,32)-mikrosaniye(istek,40)
    assert abs(2*(etkin-ofset)-gecikme)<=4,(etkin,ofset,gecikme)
    print('✓ DHCP seçenek 42 ile açılışta ve elle SNTP, RTC QMP ±2 s ve kullanıcı saat ELF',flush=True)


if __name__=='__main__':
    kos(Path(sys.argv[1]).resolve())
