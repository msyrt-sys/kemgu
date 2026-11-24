"""QEMU dgram arka ucu için yerel Ethernet eşi ve belirlenimci hat benzetimi."""
import random
import socket
import struct
from contextlib import AbstractContextManager

from pcap_oku import cerceve_oku

ES_MAC = bytes.fromhex('025159170001')
ES_IP = bytes((192, 0, 2, 1))
KONUK_IP = bytes((192, 0, 2, 2))


def saglama(veri):
    if len(veri) % 2:
        veri += bytes(1)
    toplam = sum(x for x, in struct.iter_unpack('!H', veri))
    while toplam >> 16:
        toplam = (toplam & 65535) + (toplam >> 16)
    return toplam ^ 65535


def ethernet(hedef, kaynak, tur, yuk):
    return struct.pack('!6s6sH', hedef, kaynak, tur) + yuk


def ipv4(kaynak, hedef, protokol, yuk, kimlik=1, parca=0):
    bas = struct.pack('!BBHHHBBH4s4s', 0x45, 0, 20+len(yuk), kimlik, parca,
                      64, protokol, 0, kaynak, hedef)
    return bas[:10] + struct.pack('!H', saglama(bas)) + bas[12:] + yuk


def parcalara_ayir(kaynak, hedef, protokol, yuk, mtu, kimlik):
    adim = (mtu-20)//8*8
    if adim < 8 or len(yuk) > 65515:
        raise ValueError('IPv4 parçalama boyu')
    if len(yuk)+20 <= mtu:
        return [ipv4(kaynak, hedef, protokol, yuk, kimlik)]
    return [ipv4(kaynak, hedef, protokol, yuk[i:i+adim], kimlik,
                 i//8 | (8192 if i+adim < len(yuk) else 0))
            for i in range(0, len(yuk), adim)]


def udp(kaynak, hedef, sp, dp, icerik):
    boy = 8+len(icerik)
    paket = struct.pack('!4H', sp, dp, boy, 0) + icerik
    kontrol = saglama(struct.pack('!4s4sBBH', kaynak, hedef, 0, 17, boy)+paket)
    return paket[:6] + struct.pack('!H', kontrol or 65535) + icerik


def yankilama(tur, kimlik, sira, yuk):
    b = struct.pack('!BBHHH', tur, 0, 0, kimlik, sira) + yuk
    return b[:2]+struct.pack('!H', saglama(b))+b[4:]


def arp(islem, mac, ip, hedef_mac, hedef_ip):
    return struct.pack('!HHBBH6s4s6s4s', 1, 0x0800, 6, 4, islem, mac, ip, hedef_mac, hedef_ip)


def dhcp_secenekleri(b):
    if len(b) < 240 or b[236:240] != bytes.fromhex('63825363'):
        raise ValueError('DHCP başlığı')
    i = 240
    alan = {}
    while i < len(b):
        kod = b[i]
        i += 1
        if kod == 255:
            return alan
        if kod == 0:
            continue
        if i >= len(b) or i+1+b[i] > len(b):
            raise ValueError('kesik DHCP seçeneği')
        n = b[i]
        alan[kod] = alan.get(kod, b'') + b[i+1:i+1+n]
        i += 1+n
    raise ValueError('DHCP son işareti eksik')


def dhcp_yanit(c, ip, mac, kiraci, kira=60):
    b = c.yuk[8:]
    alan = dhcp_secenekleri(b)
    istek = alan.get(53)
    if b[:3] != bytes((1, 1, 6)) or istek not in (b'\x01', b'\x03'):
        return None
    if istek == b'\x03' and alan.get(54, ip) != ip:
        return None
    secilen = alan.get(50, b[12:16])
    tur = 2 if istek == b'\x01' else (5 if secilen == kiraci else 6)
    mesaj = bytearray(240)
    mesaj[:3] = bytes((2, 1, 6))
    mesaj[4:12] = b[4:12]
    if tur != 6:
        mesaj[16:20] = kiraci
    mesaj[20:24] = ip
    mesaj[28:44] = b[28:44]
    mesaj[236:240] = b[236:240]
    secenek = [(53, bytes([tur])), (54, ip)]
    if tur != 6:
        secenek.extend([(1, bytes((255, 255, 255, 0))), (3, ip), (6, ip), (42, ip),
                        (51, struct.pack('!I', kira)), (58, struct.pack('!I', kira//2)),
                        (59, struct.pack('!I', kira*7//8))])
    for kod, deger in secenek:
        mesaj.extend(bytes((kod, len(deger))) + deger)
    mesaj.append(255)
    hedef = bytes([255])*4 if tur == 6 or b[10] & 128 or b[12:16] == bytes(4) else b[12:16]
    hedef_mac = bytes([255])*6 if hedef == bytes([255])*4 else c.kaynak
    return ethernet(hedef_mac, mac, 0x0800, ipv4(ip, hedef, 17, udp(ip, hedef, 67, 68, mesaj)))


class Hat:
    def __init__(self, tohum, kayip=0, cift=0, sira_boz=0):
        if not all(0 <= oran <= 1 for oran in (kayip, cift, sira_boz)):
            raise ValueError('hat olasılığı')
        self.rastgele = random.Random(tohum)
        self.kayip, self.cift, self.sira_boz = kayip, cift, sira_boz
        self.bekleyen = None

    def uygula(self, paket):
        if self.rastgele.random() < self.kayip:
            return []
        cikti = [paket] * (2 if self.rastgele.random() < self.cift else 1)
        if self.bekleyen is not None:
            cikti.extend(self.bosalt())
        elif self.rastgele.random() < self.sira_boz:
            self.bekleyen = cikti
            return []
        return cikti

    def bosalt(self):
        once = self.bekleyen
        self.bekleyen = None
        return once or []


class EthernetEs(AbstractContextManager):
    def __init__(self, ip=ES_IP, mac=ES_MAC, yanki_portu=9000):
        self.ip, self.mac, self.yanki_portu = ip, mac, yanki_portu
        self.soket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.soket.bind(('127.0.0.1', 0))
        self.konuk = None

    def __exit__(self, *hata):
        self.soket.close()

    def qemu_netdev(self):
        # Konuk ucunda da portu işletim sistemi ayırır. İlk çerçeve dönüş adresini verir.
        return ('dgram,id=n0,local.type=inet,local.host=127.0.0.1,local.port=0,'
                f'remote.type=inet,remote.host=127.0.0.1,remote.port={self.soket.getsockname()[1]}')

    def al(self, sure):
        self.soket.settimeout(sure)
        ham, adres = self.soket.recvfrom(65535)
        if self.konuk is not None and self.konuk != adres:
            raise ValueError('beklenmeyen QEMU ucu')
        self.konuk = adres
        return ham

    def gonder(self, ham):
        if self.konuk is None:
            raise RuntimeError('ilk konuk çerçevesi bekleniyor')
        self.soket.sendto(ham, self.konuk)

    def yanit(self, ham):
        c = cerceve_oku(ham)
        if c.hedef not in (self.mac, bytes([255])*6):
            return None
        if c.tur == 0x0806 and len(c.yuk) >= 28:
            h, p, hl, pl, islem, mac, ip, _, hedef = struct.unpack_from('!HHBBH6s4s6s4s', c.yuk)
            if (h, p, hl, pl, islem, hedef, mac) == (1, 0x0800, 6, 4, 1, self.ip, c.kaynak):
                return ethernet(mac, self.mac, c.tur, arp(2, self.mac, self.ip, mac, ip))
        if c.protokol == 17 and (c.kaynak_port, c.hedef_port) == (68, 67):
            return dhcp_yanit(c, self.ip, self.mac, KONUK_IP)
        if c.tur != 0x0800 or c.ip_hedef != self.ip or c.devami or c.parca_ofseti:
            return None
        if c.protokol == 1 and (c.icmp_tur, c.icmp_kod) == (8, 0) and len(c.yuk) >= 8:
            kimlik, sira = struct.unpack_from('!HH', c.yuk, 4)
            veri = yankilama(0, kimlik, sira, c.yuk[8:])
        elif c.protokol == 17 and c.hedef_port == self.yanki_portu:
            veri = udp(self.ip, c.ip_kaynak, c.hedef_port, c.kaynak_port, c.yuk[8:])
        else:
            return None
        return ethernet(c.kaynak, self.mac, 0x0800, ipv4(self.ip, c.ip_kaynak, c.protokol, veri))
