"""NIC tanımlayıcılarının gidiş dönüşünü dgram, QMP bellek ve pcap ile karşılaştırır."""
import argparse
import re
import time
from pathlib import Path

from ag_es import ES_MAC, EthernetEs, ethernet
from dalga12_kahin import boot, wait_for
from pcap_oku import pcap_kayitlari
from surec_imaj import imajlar
from surec_kahin import SurecConsole


def kos(w, model='e1000', kopru=False):
    mac = bytes.fromhex('525400170002')
    pcap = w/'nic.pcap'
    usb,nvme,sata = imajlar(w, {})
    yol = ',bus=alt' if kopru else ''
    with EthernetEs() as es:
        aygit = ('-netdev', es.qemu_netdev(), '-device', f'{model},id=ag0,netdev=n0,mac={mac.hex(":")}{yol}',
                 '-object', f'filter-dump,id=f0,netdev=n0,file={pcap}')
        with boot(w,usb[0],nvme,sata,ag_aygitlari=aygit, pci_topolojisi='kopru' if kopru else 'duz') as (q,p,seri):
            c = SurecConsole(w,q,p,seri)
            ag = q.call('human-monitor-command', {'command-line':'info network'})
            assert mac.hex(':') in ag, ag
            hedef = ethernet(bytes([255])*6, mac, 0x88b5, bytes(range(14,60)))
            def durum(link, sayac=None):
                olcum = []
                def bak(satir):
                    m = re.fullmatch(rb'NIC (\w+) ([0-9a-f:]+) link (\d) rx (\d+) tx (\d+) rx_bayt (\d+) tx_bayt (\d+) atilan (\d+) vektor (-?\d+)\n',satir)
                    assert m and m[1].decode()==model and m[2].decode()==mac.hex(':') and int(m[3])==link, satir
                    olcum.extend(map(int, m.groups()[3:8]))
                    if sayac is not None:
                        assert olcum == sayac, (olcum, sayac)
                c.run('ag',bak)
                return olcum
            bas = durum(1)
            def donanim(satirlar):
                assert b'NIC '+model.encode()+b' '+mac.hex(':').encode()+b' link 1' in satirlar, satirlar
            c.run('donanim', donanim)
            vektor = int.from_bytes(c.mem(c.symbols['ag_vektor'],4),'little',signed=True)
            assert (vektor == -1 if kopru else 66 <= vektor < 80), vektor
            irq = c.sayi('kesme_tablo') + 32*vektor + 8
            once_irq = int.from_bytes(c.mem(irq,8),'little') if not kopru else 0
            c.run('ag dene')
            son = time.monotonic()+5
            while True:
                kalan = son-time.monotonic()
                if kalan <= 0:
                    raise AssertionError('NIC TX çerçevesi gelmedi')
                try:
                    ham = es.al(kalan)
                except TimeoutError as exc:
                    raise AssertionError('NIC TX çerçevesi gelmedi') from exc
                if ham == hedef:
                    break
            once = c.sayi('ag_rx_sayi')
            boy = c.sayi('ag_rx_bayt')
            bos = c.bos_cerceve()
            heap = c.heap()
            gelen = []
            for i in range(18):
                ham = ethernet(mac,ES_MAC,0x88b5,bytes([i+29])*46)
                es.gonder(ham)
                gelen.append(ham)
                try:
                    wait_for(p,lambda: c.sayi('ag_rx_sayi')==once+i+1,seconds=3)
                except AssertionError as exc:
                    raise AssertionError(f'RX çerçeve {i+1}, alınan {c.sayi("ag_rx_sayi")-once}') from exc
                assert c.mem(c.sayi('ag_dma')+40960,len(ham)) == ham
            assert c.sayi('ag_rx_bayt') == boy + sum(map(len,gelen))
            if not kopru:
                assert int.from_bytes(c.mem(irq,8),'little') > once_irq
            assert c.heap()==heap and c.bos_cerceve()==bos
            c.run('ag son',b'CERCEVE '+gelen[-1].hex().encode()+b'\n')
            for acik in (False,True):
                q.call('set_link',{'name':'ag0','up':acik})
                wait_for(p,lambda: bool(int.from_bytes(c.mem(c.sayi('ag_bar')+8,4),'little')&2)==acik)
                durum(int(acik), [bas[0]+len(gelen), bas[1]+1, bas[2]+sum(map(len,gelen)), bas[3]+len(hedef), bas[4]])
            c.shutdown()
        with pcap.open('rb') as f:
            tel = [c.ham for c in pcap_kayitlari(f) if c.tur == 0x88b5]
        assert tel == [hedef,*gelen], (len(tel),len(gelen)+1)
        print(f'NIC OK {model} TX=1 RX={len(gelen)} pcap; heap={heap}→{heap} boş={bos}→{bos}',flush=True)


if __name__=='__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('dizin',type=Path)
    parser.add_argument('--kopru', action='store_true')
    args = parser.parse_args()
    kos(args.dizin.resolve(), kopru=args.kopru)
