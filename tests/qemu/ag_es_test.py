"""Sabit tel örnekleri ve iPXE ile host ağ eşini denetler."""
import io
import shutil
import socket
import struct
import subprocess
from calisma_dizini import calisma_dizini
import time
import unittest
from pathlib import Path

from ag_es import (EthernetEs, Hat, ES_MAC, ES_IP, KONUK_IP, arp, dhcp_secenekleri,
                   ethernet, ipv4, parcalara_ayir, saglama, udp, yankilama)
from pcap_oku import cerceve_oku, pcap_kayitlari


# RFC 1071 §3'teki 0001 f203 f4f5 f6f7 sözcüklerinin tümleyeni 220d.
MAC = bytes.fromhex('525400123456')
IP_BAS = bytes.fromhex('45000073000040004011b861c0a80001c0a800c7')
# Boş ICMP echo isteği, IPv4 başlık kimliği 1, 192.0.2.2 → 192.0.2.1.
PING = bytes.fromhex('0251591700015254001234560800'
                     '4500001c000100004001f6dcc0000202c0000201'
                     '0800f7fd00010001')


class TelTesti(unittest.TestCase):
    def test_saglama(self):
        self.assertEqual(saglama(bytes.fromhex('0001f203f4f5f6f7')), 0x220d)
        self.assertEqual(saglama(b'\x01\x02\x03'), 0xfbfd)
        self.assertEqual(saglama(IP_BAS), 0)
        self.assertEqual(ipv4(KONUK_IP, ES_IP, 1, PING[-8:]), PING[14:])

    def test_pcap(self):
        for yon in ('<', '>'):
            bas = struct.pack(yon+'IHH4I', 0xa1b2c3d4, 2, 4, 0, 0, 65535, 1)
            kayit = struct.pack(yon+'4I', 1, 0, len(PING), len(PING)) + PING
            self.assertEqual(next(pcap_kayitlari(io.BytesIO(bas+kayit))).icmp_tur, 8)
            for veri in (bas+kayit[:-1], bas+kayit[:10]):
                with self.assertRaises(ValueError):
                    list(pcap_kayitlari(io.BytesIO(veri)))
        for i in (24, 36):
            bozuk = bytearray(PING)
            bozuk[i] ^= 1
            with self.assertRaises(ValueError):
                cerceve_oku(bozuk)

    def test_protokoller(self):
        with EthernetEs() as es:
            c = cerceve_oku(es.yanit(PING))
            self.assertEqual((c.icmp_tur, c.ip_hedef, c.yuk[4:]), (0, KONUK_IP, PING[-4:]))
            paket = ethernet(bytes([255])*6, MAC, 0x806, arp(1, MAC, KONUK_IP, bytes(6), ES_IP))
            self.assertEqual(cerceve_oku(es.yanit(paket)).yuk, arp(2, ES_MAC, ES_IP, MAC, KONUK_IP))
            ic = udp(KONUK_IP, ES_IP, 8100, 9000, b'abc')
            ham = ethernet(ES_MAC, MAC, 0x0800, ipv4(KONUK_IP, ES_IP, 17, ic))
            cevap = cerceve_oku(es.yanit(ham))
            self.assertEqual((cevap.hedef_port, cevap.yuk[8:]), (8100, b'abc'))
            with self.assertRaisesRegex(ValueError, 'taşıma'):
                cerceve_oku(ham[:-1]+b'd')
            tcp = bytes.fromhex('1234005000000001000000005002100000000000')
            toplam = saglama(KONUK_IP+ES_IP+bytes.fromhex('00060014')+tcp)
            tcp = tcp[:16]+struct.pack('!H', toplam)+tcp[18:]
            paket = ethernet(ES_MAC, MAC, 0x0800, ipv4(KONUK_IP, ES_IP, 6, tcp))
            self.assertEqual(cerceve_oku(paket).sira, 1)
            with self.assertRaisesRegex(ValueError, 'taşıma'):
                cerceve_oku(paket[:-1]+b'x')

    def test_hat(self):
        paketler = parcalara_ayir(KONUK_IP, ES_IP, 17, bytes(85), 52, 3)
        parcalar = [cerceve_oku(ethernet(ES_MAC, MAC, 0x0800, p)) for p in paketler]
        self.assertEqual([(c.parca_ofseti,c.devami) for c in parcalar], [(0,True),(32,True),(64,False)])
        def gecir():
            hat = Hat(17, kayip=0.1, cift=0.2, sira_boz=0.3)
            sonuc = [b for i in range(100) for b in hat.uygula(i)]
            return sonuc+hat.bosalt()
        sonuc = gecir()
        self.assertEqual(sonuc, gecir())
        self.assertLess(len(set(sonuc)), 100)
        self.assertGreater(len(sonuc), len(set(sonuc)))
        self.assertNotEqual(sonuc, sorted(sonuc))


def firmware():
    qemu = shutil.which('qemu-system-x86_64')
    if not qemu:
        print('ATLANDI (QEMU yok): host Ethernet eşi iPXE açılışı')
        return
    with calisma_dizini(prefix='ag17-') as dizin, EthernetEs() as es:
        kayit = Path(dizin)/'ag.pcap'
        komut = [qemu, '-M','q35','-display','none','-nodefaults','-no-reboot','-boot','n',
                 '-netdev', es.qemu_netdev(), '-device','e1000,netdev=n0,bootindex=1,mac='+MAC.hex(':'),
                 '-object', f'filter-dump,id=f0,netdev=n0,file={kayit}']
        p = subprocess.Popen(komut)
        try:
            son = time.monotonic()+20
            hamlar = []
            tipler = []
            while 3 not in tipler and time.monotonic() < son:
                if p.poll() is not None:
                    raise RuntimeError('QEMU erken kapandı')
                try:
                    ham = es.al(.5)
                except socket.timeout:
                    continue
                c = cerceve_oku(ham)
                if c.hedef_port != 67:
                    continue
                tipler.append(dhcp_secenekleri(c.yuk[8:])[53][0])
                hamlar.append(ham)
                yanit = es.yanit(ham)
                es.gonder(yanit)
                hamlar.append(yanit)
            assert tipler and tipler[-1] == 3 and set(tipler[:-1]) == {1}, tipler
            istek = cerceve_oku(hamlar[-2]).yuk[8:]
            alan = dhcp_secenekleri(istek)
            assert alan[50] == KONUK_IP and alan[54] == ES_IP
            assert any(cerceve_oku(teklif).yuk[12:16] == istek[4:8] for teklif in hamlar[1:-1:2])
            p.terminate()
            p.wait(timeout=5)
            # İkinci konuk iletisi, ilk sunucu yanıtının NIC'e gerçekten ulaştığını kanıtlar.
            with kayit.open('rb') as f:
                tel = [c.ham for c in pcap_kayitlari(f)]
            assert all(h in tel for h in hamlar[:-1])
            print('AG_ES QEMU OK: iPXE DISCOVER/OFFER/REQUEST, dgram ve pcap birebir')
        finally:
            if p.poll() is None:
                p.terminate()
                p.wait(timeout=5)


if __name__ == '__main__':
    import sys
    # Ana süreç çıkışını unittest sonucundan al.
    if not unittest.TextTestRunner().run(unittest.defaultTestLoader.loadTestsFromTestCase(TelTesti)).wasSuccessful():
        sys.exit(1)
    firmware()
