"""Üç sürecin ayrı TCP akışlarını ve klavyeyle birlikte beklenen iki UDP fd'sini gözler."""
from pathlib import Path
import sys
import time

from ag_es import EthernetEs, ES_IP, ES_MAC, KONUK_IP, TcpEs, ethernet, ipv4, udp
from dalga12_kahin import boot, wait_for
from pcap_oku import pcap_kayitlari
from surec_imaj import imajlar
from surec_kahin import SurecConsole

MAC = bytes.fromhex('525400170010')


def seri_bekle(c, konum, isaret):
    def sonuc():
        metin = c.ser.read_bytes()[konum:]
        if isaret in metin:
            return True
        if b'cikis=' in metin or b'KABUK HATA' in metin:
            raise AssertionError((isaret, metin))
        return False
    wait_for(c.p, sonuc, seconds=18)


def tcp_al(es, sec):
    son = time.monotonic() + 10
    while time.monotonic() < son:
        p, veri = es.al(son - time.monotonic())
        if sec(p, veri):
            return p, veri
    raise TimeoutError('beklenen TCP olayı')


def kos(w):
    disk, nvme, sata = imajlar(w, {'soketdeney.elf': (w/'soketdeney.elf').read_bytes()})
    pcap = w/'soket.pcap'
    with EthernetEs() as hat:
        net = ('-netdev', hat.qemu_netdev(), '-device', f'e1000,id=n,netdev=n0,mac={MAC.hex(":")}',
               '-object', f'filter-dump,id=f0,netdev=n0,file={pcap}')
        with boot(w, disk[0], nvme, sata, ag_aygitlari=net) as (q, proc, seri):
            c = SurecConsole(w, q, proc, seri)
            c.run('ag ayarla 192.0.2.2/24 192.0.2.1', b'AG AYAR OK\n')
            es = TcpEs(hat, MAC)
            once = c.heap(), c.bos_cerceve()
            acik = {}
            for i, harf in enumerate(b'abc'):
                es_id = bytes([harf])
                komut = 'baslat soketdeney ' + chr(harf)
                konum = seri.stat().st_size
                c.input(komut + (' &' if i < 2 else ''))
                p, _ = tcp_al(es, lambda p, _: p.bayrak == 2 and p.hedef_port == 9100)
                port = p.kaynak_port
                assert port not in acik
                seq = 500000 + 1000*i
                ack = p.sira+1
                es.yolla(9100, port, seq, ack, 18)
                p, veri = tcp_al(es, lambda p,v: p.kaynak_port == port and v)
                assert (p.sira, p.onay, veri) == (ack, seq+1, es_id)
                seq += 1
                ack += 1
                es.yolla(9100, port, seq, ack, 24, es_id)
                p, veri = tcp_al(es, lambda p,v: p.kaynak_port == port and v)
                assert (p.sira, p.onay, veri) == (ack, seq+1, bytes([harf+32]))
                seq += 1
                ack += 1
                es.yolla(9100, port, seq, ack, 16)
                acik[port] = (seq, ack)
                if i < 2:
                    seri_bekle(c, konum, b'KABUK_HAZIR\n')
            uclu = list(acik)
            c.key('ctrl', True)
            c.key('c', True)
            c.key('c', False)
            c.key('ctrl', False)
            seri_bekle(c, konum, b'^C')
            for j in (2, 0, 1):
                port = uclu[j]
                seq, ack = acik[port]
                if j != 2:
                    es.yolla(9100, port, seq, ack, 24, b'G')
                    seq += 1
                p, _ = tcp_al(es, lambda p,_: p.kaynak_port == port and p.bayrak & 1)
                assert (p.sira, p.onay) == (ack, seq)
                es.yolla(9100, port, seq, ack+1, 17)
            seri_bekle(c, konum, b'KABUK_HAZIR\n')
            konum = seri.stat().st_size
            c.input('baslat soketdeney mix')
            seri_bekle(c, konum, b'KARISIK HAZIR\n')

            def yolla(port, veri):
                paket = udp(ES_IP, KONUK_IP, 9200, port, veri)
                hat.gonder(ethernet(MAC, ES_MAC, 2048, ipv4(ES_IP, KONUK_IP, 17, paket)))

            yolla(9102, b'B')
            seri_bekle(c, konum, b'SOKET B\n')
            c.key('k', True)
            c.key('k', False)
            seri_bekle(c, konum, b'KLAVYE K\n')
            yolla(9101, b'A')
            seri_bekle(c, konum, b'cikis=0]')
            konum = seri.stat().st_size
            c.input('baslat soketdeney dinle')
            seri_bekle(c, konum, b'DINLE HAZIR\n')
            es.yolla(22000, 9110, 512, 0, 2)
            p, _ = tcp_al(es, lambda p,_: p.kaynak_port==9110 and p.bayrak==18)
            assert p.onay==513
            ack = p.sira+1
            es.yolla(22000, 9110, 513, ack, 16)
            es.yolla(22000, 9110, 513, ack, 24, b'Z')
            sonuc = bytearray()
            while True:
                p, veri = tcp_al(es, lambda p,_: p.kaynak_port==9110)
                assert (p.sira,p.onay)==(ack,514), (p.sira,p.onay,ack)
                sonuc.extend(veri)
                ack += len(veri)
                if p.bayrak&1:
                    es.yolla(22000, 9110, 514, ack+1, 17)
                    break
                if veri:
                    es.yolla(22000, 9110, 514, ack, 16)
            assert sonuc==b'Z'
            seri_bekle(c, konum, b'cikis=0]')
            print('soket heap/boş çerçeve', once, (c.heap(), c.bos_cerceve()), flush=True)
            assert once[1] == c.bos_cerceve()
    with pcap.open('rb') as f:
        paketler = list(pcap_kayitlari(f))
    for i, port in enumerate(uclu):
        akim = [p for p in paketler if p.kaynak==MAC and p.protokol==6 and p.kaynak_port==port]
        assert sum(bool(p.bayrak&1) for p in akim)==1
        yuk = b''.join(p.yuk[(p.yuk[12]>>4)*4:] for p in akim)
        assert yuk==bytes([97+i,129+i]), (port,yuk)
    datagramlar = [(p.kaynak_port,p.yuk[8:]) for p in paketler
                   if p.kaynak==MAC and p.protokol==17 and p.hedef_port==9200]
    assert datagramlar==[(9102,b'B'),(9101,b'A')], datagramlar
    print('✓ üç canlı süreç, üç FIN, QMP Ctrl+C, boru + UDP B + klavye K + UDP A ve pcap')


if __name__ == '__main__':
    kos(Path(sys.argv[1]))
