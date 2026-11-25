"""TFTP bloklarını ham Ethernet eşinde, teşhis TCP akışını bağımsız alıcıda doğrular."""
import hashlib
from pathlib import Path
import struct
import subprocess
import sys
import threading

from ag_arac_kahin import komut, pcap_akim, MAC
from ag_es import EthernetEs, ES_IP, ES_MAC, KONUK_IP, ethernet, ipv4, udp
from dalga12_fat import Fat
from dalga12_kahin import boot, fsck, wait_for
from pcap_oku import cerceve_oku, pcap_kayitlari
from surec_imaj import imajlar
from surec_kahin import SurecConsole

BOY = 2 << 20
TID = 35142


class TftpEs:
    def __init__(self, hat, veri):
        self.hat = hat
        self.veri = veri
        self.hedef = self.op = self.blok = 0
        self.yuklenen = bytearray()
        self.kayip = {1: set(), 2: set()}
        self.tekrarlanan = set()

    def yolla(self, op, blok, veri=b''):
        b = struct.pack('!HH', op, blok) + veri
        self.hat.gonder(ethernet(MAC, ES_MAC, 2048, ipv4(ES_IP, KONUK_IP, 17,
                         udp(ES_IP, KONUK_IP, TID, self.hedef, b))))

    def dusur(self, blok):
        if blok % 20 == 7 and blok not in self.kayip[self.op]:
            self.kayip[self.op].add(blok)
            return True
        return False

    def data(self):
        if not self.dusur(self.blok):
            ilk = (self.blok - 1) * 512
            self.yolla(3, self.blok, self.veri[ilk:ilk+512])

    def isle(self, p):
        b = p.yuk[8:]
        op, = struct.unpack_from('!H', b)
        if p.hedef_port == 69:
            assert op in (1, 2) and b[2:] == b'uzak.bin\0octet\0', b
            self.op, self.hedef = op, p.kaynak_port
            if op == 1:
                self.blok = 1
                self.data()
            else:
                self.blok = 0
                self.yolla(4, 0)
            return
        assert p.kaynak_port == self.hedef
        blok, = struct.unpack_from('!H', b, 2)
        if self.op == 1 and op == 4:
            if blok == self.blok:
                if blok >= len(self.veri)//512 + 1:
                    return
                if blok % 101 == 2 and blok not in self.tekrarlanan:
                    self.tekrarlanan.add(blok)
                    self.data()
                    return
                self.blok += 1
            if blok in (self.blok, self.blok-1):
                self.data()
        elif self.op == 2 and op == 3:
            if blok == self.blok + 1:
                if self.dusur(blok):
                    return
                self.yuklenen.extend(b[4:])
                self.blok = blok
            if blok == self.blok:
                self.yolla(4, blok)
        else:
            raise AssertionError((self.op, op, blok))


def pcap_veri(pcap, giden):
    bloklar = {}
    with pcap.open('rb') as f:
        for p in pcap_kayitlari(f):
            if p.protokol != 17 or (p.hedef_port if giden else p.kaynak_port) != TID:
                continue
            op, blok = struct.unpack_from('!HH', p.yuk, 8)
            if op == 3:
                b = p.yuk[12:]
                assert bloklar.setdefault(blok, b) == b
    assert sorted(bloklar) == list(range(1, len(bloklar)+1))
    assert len(bloklar[len(bloklar)]) < 512
    return b''.join(bloklar[i] for i in sorted(bloklar))


def hazir(c):
    wait_for(c.p, lambda: int.from_bytes(c.mem(c.sayi('ag_l5'), 4), 'little') == 3, seconds=15)
    komut(c, 'bagla --yazilabilir usb0p2')
    komut(c, 'cd /diskler/usb0p2/bin')


def tftp(w, boy):
    veri = hashlib.shake_256(b'Dalga17 TFTP').digest(boy)
    usb, nvme, sata = imajlar(w, {'tftp.elf': (w/'tftp.elf').read_bytes()})
    pcap = w/'tftp.pcap'
    with EthernetEs() as hat:
        es = TftpEs(hat, veri)
        dur = threading.Event()
        hatalar = []

        def tel():
            try:
                while not dur.is_set():
                    try:
                        ham = hat.al(.1)
                    except TimeoutError:
                        continue
                    p = cerceve_oku(ham)
                    if p.kaynak == MAC and p.protokol == 17 and p.hedef_port in (69,TID):
                        es.isle(p)
                    else:
                        cevap = hat.yanit(ham)
                        if cevap:
                            hat.gonder(cevap)
            except Exception as exc:
                hatalar.append(exc)

        isci = threading.Thread(target=tel)
        isci.start()
        try:
            net = ('-netdev',hat.qemu_netdev(),'-device',f'e1000,netdev=n0,mac={MAC.hex(":")}',
                   '-object',f'filter-dump,id=f0,netdev=n0,file={pcap}')
            with boot(w,usb[0],nvme,sata,ag_aygitlari=net) as (q,p,seri):
                c = SurecConsole(w,q,p,seri)
                hazir(c)
                once = c.heap(), c.bos_cerceve()
                sayilar = []
                for islem in ('al 192.0.2.1 uzak.bin al.bin', 'gonder 192.0.2.1 al.bin uzak.bin'):
                    s = komut(c,'tftp '+islem,320)
                    assert b'TFTP tamam ' in s, (s,hatalar,es.blok)
                    sayilar.append(int(s.split(b'TFTP tamam ')[1].splitlines()[0]))
                son = c.heap(), c.bos_cerceve()
                assert son[1] == once[1], (once,son)
                print('TFTP heap/boş çerçeve', once, son, flush=True)
                c.expected = c.output()
                c.shutdown()
        finally:
            dur.set()
            isci.join(3)
        assert not hatalar and not isci.is_alive(), hatalar
    fsck(w,usb[1],'tftp')
    dosya = Fat(usb[1].read()).get('/BIN/al.bin')
    assert hashlib.sha256(dosya).digest() == hashlib.sha256(veri).digest(), (
        'FAT32 SHA256', hashlib.sha256(dosya).hexdigest(), hashlib.sha256(veri).hexdigest(), len(dosya), boy)
    assert sayilar == [boy,boy] and es.yuklenen == dosya == veri
    assert es.tekrarlanan and es.kayip[1] == es.kayip[2] == set(range(7, boy//512+2, 20))
    assert pcap_veri(pcap,True) == pcap_veri(pcap,False) == veri
    print('✓ TFTP FAT32, iki yön pcap ve eş SHA256', hashlib.sha256(veri).hexdigest(),
          'bayt', boy, 'yön başına kayıp', len(es.kayip[1]), 'yinelenen', len(es.tekrarlanan), flush=True)


def teshis(w):
    usb,nvme,sata = imajlar(w, {'teshis.elf': (w/'teshis.elf').read_bytes()})
    pcap = w/'teshis.pcap'
    net = ('-netdev','user,id=n0','-device',f'e1000,id=nic0,netdev=n0,mac={MAC.hex(":")}',
           '-object',f'filter-dump,id=f0,netdev=n0,file={pcap}')
    kayitlar = []
    with boot(w,usb[0],nvme,sata,ag_aygitlari=net) as (q,p,seri):
        c = SurecConsole(w,q,p,seri)
        hazir(c)
        komut(c,'donanim kaydet donanim.txt')
        komut(c,'echo TESHIS17')
        once = c.heap(), c.bos_cerceve()
        for ad in ('donanim','log'):
            dosya = w/(ad+'-alindi.txt')
            with subprocess.Popen([sys.executable,'tools/codex/teshis_al.py',str(dosya),'--port','0'],
                                  stdout=subprocess.PIPE, text=True) as alici:
                try:
                    bas = alici.stdout.readline().split()
                    assert bas[0] == 'DINLIYOR'
                    port = int(bas[1])
                    if ad == 'log':
                        adres = c.sayi('kgun_adres')
                        ilk = c.sayi('kgun_bas')
                        boy = c.sayi('kgun_adet')
                        halka = c.mem(adres,8192)
                        gunluk = f"LOG dusen_satir {c.sayi('kgun_dusen')}\n".encode() + (halka+halka)[ilk:ilk+boy]
                    s = komut(c,f'{ad} gonder 10.0.2.2 {port}',35)
                    assert b'teshis gonderildi ' in s, s
                    sonuc = alici.communicate(timeout=5)[0].split()
                    assert alici.returncode == 0 and sonuc[0] == 'KAYDEDILDI', sonuc
                    veri = dosya.read_bytes()
                    assert (int(sonuc[1]),sonuc[2]) == (len(veri),hashlib.sha256(veri).hexdigest())
                    if ad == 'log':
                        assert veri == gunluk, (len(veri),len(gunluk))
                    kayitlar.append((port,veri))
                finally:
                    if alici.poll() is None:
                        alici.terminate()
        son = c.heap(), c.bos_cerceve()
        assert once[1] == son[1], (once,son)
        q.call('set_link',{'name':'nic0','up':False})
        ilk = seri.stat().st_size
        c.input('log gonder 10.0.2.2 1234')
        wait_for(p,lambda:b'HATA teshis ag yok' in seri.read_bytes()[ilk:],seconds=5)
        q.call('set_link',{'name':'nic0','up':True})
        print('Teşhis heap/boş çerçeve',once,son, flush=True)
        c.expected = c.output()
        c.shutdown()
    fsck(w,usb[1],'teshis')
    assert kayitlar[0][1] == Fat(usb[1].read()).get('/BIN/donanim.txt')
    assert kayitlar[1][1].startswith(b'LOG dusen_satir ') and b'TESHIS17' in kayitlar[1][1]
    with pcap.open('rb') as f:
        paketler = list(pcap_kayitlari(f))
    for port,veri in kayitlar:
        assert pcap_akim(paketler,port,True) == f'KEMGU-TESHIS/1 {len(veri)}\n'.encode()+veri
        print('✓ Teşhis alıcı + pcap', len(veri), hashlib.sha256(veri).hexdigest(), flush=True)


if __name__ == '__main__':
    w = Path(sys.argv[1]).resolve()
    if len(sys.argv) < 3 or sys.argv[2] == 'tftp':
        tftp(w, int(sys.argv[3]) if len(sys.argv)>3 else BOY)
    if len(sys.argv) < 3 or sys.argv[2] == 'teshis':
        teshis(w)
