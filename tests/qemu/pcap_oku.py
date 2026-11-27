"""QEMU Ethernet pcap kaydını okuyup IPv4 ve taşıma sağlamalarını denetler."""
import argparse
import struct
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Cerceve:
    ham: bytes
    hedef: bytes
    kaynak: bytes
    tur: int
    yuk: bytes
    ip_kaynak: bytes = b''
    ip_hedef: bytes = b''
    protokol: int = 0
    kimlik: int = 0
    parca_ofseti: int = 0
    devami: bool = False
    kaynak_port: int = 0
    hedef_port: int = 0
    sira: int = 0
    onay: int = 0
    bayrak: int = 0
    icmp_tur: int = 0
    icmp_kod: int = 0


def saglama_gecerli(veri):
    # Eşin kurucusundan bağımsız olarak bayt sütunlarının ağırlıklı toplamını al.
    toplam = 256 * sum(veri[::2]) + sum(veri[1::2])
    return toplam != 0 and toplam % 65535 == 0


def cerceve_oku(ham):
    if len(ham) < 14:
        raise ValueError('kısa Ethernet')
    hedef, kaynak, tur = struct.unpack_from('!6s6sH', ham)
    veri = ham[14:]
    if tur != 0x0800:
        return Cerceve(ham, hedef, kaynak, tur, veri)
    if len(veri) < 20 or veri[0] >> 4 != 4:
        raise ValueError('IPv4 sürümü veya boyu')
    ihl = (veri[0] & 15) * 4
    boy, kimlik, bol = struct.unpack_from('!HHH', veri, 2)
    if not 20 <= ihl <= boy <= len(veri) or not saglama_gecerli(veri[:ihl]):
        raise ValueError('IPv4 başlığı veya sağlaması')
    ip1, ip2 = veri[12:16], veri[16:20]
    protokol = veri[9]
    yuk = veri[ihl:boy]
    alan = dict(ip_kaynak=ip1, ip_hedef=ip2, protokol=protokol, kimlik=kimlik,
                parca_ofseti=(bol & 8191) * 8, devami=bool(bol & 8192))
    if not bol & 0x3fff:
        if protokol == 1:
            if len(yuk) < 4 or not saglama_gecerli(yuk):
                raise ValueError('ICMP boyu veya sağlaması')
            alan.update(icmp_tur=yuk[0], icmp_kod=yuk[1])
        elif protokol in (6, 17):
            if len(yuk) < (20 if protokol == 6 else 8):
                raise ValueError('kısa TCP/UDP')
            sp, dp = struct.unpack_from('!HH', yuk)
            alan.update(kaynak_port=sp, hedef_port=dp)
            if protokol == 17:
                n, check = struct.unpack_from('!HH', yuk, 4)
                if not 8 <= n <= len(yuk):
                    raise ValueError('UDP boyu')
                yuk = yuk[:n]
            else:
                n = len(yuk)
                check = True
                if not 20 <= (yuk[12] >> 4) * 4 <= n:
                    raise ValueError('TCP başlık boyu')
                seq, ack = struct.unpack_from('!II', yuk, 4)
                alan.update(sira=seq, onay=ack, bayrak=yuk[13])
            onek = struct.pack('!4s4sBBH', ip1, ip2, 0, protokol, n)
            if check and not saglama_gecerli(onek + yuk):
                raise ValueError('taşıma sağlaması')
    return Cerceve(ham, hedef, kaynak, tur, yuk, **alan)


def pcap_kayitlari(f):
    bas = f.read(24)
    bicimler = {bytes.fromhex(k): v for k, v in (
        ('d4c3b2a1', '<'), ('a1b2c3d4', '>'), ('4d3cb2a1', '<'), ('a1b23c4d', '>'))}
    if len(bas) != 24 or bas[:4] not in bicimler:
        raise ValueError('pcap imzası')
    yon = bicimler[bas[:4]]
    ana, alt, _, _, sinir, bag = struct.unpack_from(yon+'HHIIII', bas, 4)
    if (ana, alt, bag) != (2, 4, 1) or sinir == 0:
        raise ValueError('Ethernet pcap bekleniyor')
    while kayit := f.read(16):
        if len(kayit) != 16:
            raise ValueError('kesik pcap kaydı')
        _, _, n, asil = struct.unpack(yon+'IIII', kayit)
        if n != asil or n > min(sinir, 65535+14):
            raise ValueError('kesilmiş veya aşırı büyük Ethernet kaydı')
        ham = f.read(n)
        if len(ham) != n:
            raise ValueError('eksik pcap verisi')
        yield cerceve_oku(ham)


def tcp_akimi(paketler, port, giden, mac):
    # Aynı akışın sıra dışı gelen veya yeniden iletilen parçalarını gerçek
    # TCP sıra numaralarıyla birleştir. Boşluklar ve çelişen kopyalar hatadır.
    parcalar = []
    sonlar = set()
    syn = None
    for p in paketler:
        if p.protokol != 6:
            continue
        if giden:
            if p.kaynak != mac or p.hedef_port != port:
                continue
        elif p.hedef != mac or p.kaynak_port != port:
            continue
        if syn is None:
            assert p.bayrak & 2, "TCP başlangıcı eksik"
            syn = p
        assert (p.ip_kaynak,p.ip_hedef,p.kaynak_port,p.hedef_port)==(
            syn.ip_kaynak,syn.ip_hedef,syn.kaynak_port,syn.hedef_port), "beklenmeyen TCP akışı"
        if p.bayrak&2:
            assert p.sira==syn.sira, "beklenmeyen TCP başlangıç sırası"
        b = p.yuk[4*(p.yuk[12]>>4):]
        ofs = (p.sira+(1 if p.bayrak&2 else 0)-syn.sira-1)&0xffffffff
        if b:
            parcalar.append((ofs,b))
        if p.bayrak&1:
            sonlar.add(ofs+len(b))
    birikim = bytearray()
    for ofs,b in sorted(parcalar,key=lambda parca:parca[0]):
        assert ofs<=len(birikim), ("PCAP akışında boşluk",ofs,len(birikim))
        ortak = min(len(b),len(birikim)-ofs)
        assert birikim[ofs:ofs+ortak]==b[:ortak], ("PCAP kopyası farklı",ofs)
        birikim.extend(b[ortak:])
    assert sonlar=={len(birikim)}, ("TCP FIN konumu",sonlar,len(birikim))
    return bytes(birikim)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('pcap', type=Path)
    with parser.parse_args().pcap.open('rb') as dosya:
        for i, c in enumerate(pcap_kayitlari(dosya), 1):
            print(i, c.kaynak.hex(':'), c.hedef.hex(':'), hex(c.tur), c.protokol, len(c.ham))
