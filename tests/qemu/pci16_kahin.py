"""PCI envanteri, BAR boyları ve sayfa yapraklarını QMP ile doğrular."""
import argparse
import re
import struct
import subprocess
from pathlib import Path

from dalga12_fat import Fat
from dalga12_imaj import images
from dalga12_kahin import Console, boot
from donanim_kahin import pci_kumesi


def port_genislikleri(elf):
    # Ayrı ayrı gerçek x86 sembollerinde işlem genişliğini ve operand sırasını doğrula.
    for ad, komut, kaynak, hedef in (
        ('oku16','inw','dx','ax'), ('oku32','inl','dx','eax'),
        ('yaz16','outw','ax','dx'), ('yaz32','outl','eax','dx'),
    ):
        simge = 'kem_port_'+ad
        dokum = subprocess.check_output(['llvm-objdump','--disassemble-symbols='+simge,str(elf)],text=True)
        assert '<'+simge+'>:' in dokum and re.search(r'\b'+komut+r'\s+%'+kaynak+r',\s*%'+hedef+r'\b',dokum), dokum
    print('PORT16/32 OK inw, inl, outw, outl',flush=True)


def dugumler(veri):
    for a in veri:
        yield a
        yield from dugumler(a.get('pci_bridge', {}).get('devices', []))


def yaprak(c, adres):
    pt = c.symbols['uefi_pml4']
    for shift in (39, 30, 21, 12):
        g, = struct.unpack('<Q', c.mem(pt + (adres >> shift & 511) * 8, 8))
        assert g & 1, (hex(adres), shift)
        if shift == 12 or (shift in (21, 30) and g & 128):
            return g
        pt = g & (((1 << 52) - 1) & ~4095)
    raise AssertionError(adres)


def kos(w, tur):
    usb, nvme, sata = images(w, 'gpt')
    with boot(w, usb[0], nvme, sata, pci_topolojisi=tur) as (q,p,seri):
        c = Console(w,q,p,seri)
        agac = list(dugumler(q.call('query-pci')[0]['devices']))
        ilk_pci = pci_kumesi(q)
        if tur == 'kopru':
            # Firmware numarasını koruyup dış köprünün alt yol yazmacını donanımda sıfırla.
            kok = next(a for a in agac if a['bus'] == 0 and 'pci_bridge' in a)
            secici = 0x80000018 | kok['slot'] << 11 | kok['function'] << 8
            for komut in (f'o /w 0xcf8 {secici:#x}', 'o /b 0xcfe 0'):
                assert not q.call('human-monitor-command', {'command-line': komut}).strip()
            kirik = next(a for a in q.call('query-pci')[0]['devices'] if a['slot'] == kok['slot'] and a['function'] == kok['function'])
            assert kirik['pci_bridge']['bus']['subordinate'] == 0
        def incele(ham):
            s = ham.decode('utf-8')
            pci = {bdf: (int(v,16), int(d,16), int(cl,16) >> 8) for bdf,v,d,cl in
                   re.findall(r'^PCI ([0-9a-f:.]+) (\w{4}):(\w{4}) (\w{6})$', s, re.M)}
            assert pci == ilk_pci == pci_kumesi(q), (ilk_pci.keys() - pci.keys(), tur)
            barlar = {(int(b),int(i)): tuple(map(int,(t,a,n,g))) for b,i,t,a,n,g in
                      re.findall(r'^BAR (\d+) (\d+) (\d+) (\d+) (\d+) (-?\d+)$', s, re.M)}
            kopruler = {int(b):int(v) for b,v in re.findall(r'^KOPRU (\d+) (\d+)$',s,re.M)}
            beklenen = set()
            for a in agac:
                bdf = a['bus']*256 + a['slot']*8 + a['function']
                if 'pci_bridge' in a:
                    bus = a['pci_bridge']['bus']
                    assert kopruler[bdf] == bus['number'] | bus['secondary']<<8 | bus['subordinate']<<16
                for r in a['regions']:
                    i = r['bar']
                    beklenen.add((bdf,i))
                    t,adr,n,g = barlar[bdf,i]
                    assert (adr,n) == (max(0,r['address']),r['size']), (bdf,r,t,adr,n)
                    if i == 6:
                        assert t == 8
                    else:
                        et = int(r['type']=='io') | int(r.get('mem_type_64', False))*2 | int(r.get('prefetch', False))*4
                        assert t == et, (r,t,et)
                    if adr and r['type'] == 'memory':
                        assert g & 24 == 24 and yaprak(c,adr) & ~96 == g & ~96, (r,g)
            assert set(barlar) == beklenen, (set(barlar)-beklenen,beklenen-set(barlar))
            taban = int(re.search(r'^ECAM0 (\d+)$',s,re.M)[1])
            cfg = re.findall(r'^CFG (\d+) (\d+) (\d+)$',s,re.M)
            assert len(cfg) == 64
            ilk = b''.join(struct.pack('<I', int(v)) for _,v,e in cfg)
            assert all(int(o)==i*4 and v==e for i,(o,v,e) in enumerate(cfg))
            assert int(re.search(r'^PORT16 (\d+)',s,re.M)[1]) == pci['00:00.0'][0]
            assert bool(taban) == (tur != 'cf8')
            if taban:
                assert c.mem(taban,256) == ilk
                assert yaprak(c,taban) & 24 == 24
                ext = [(int(b),int(o),int(v)) for b,o,v in re.findall(r'^GENIS (\d+) (\d+) (\d+)$',s,re.M)]
                zincirler = {}
                for a in agac:
                    bdf = a['bus'] * 256 + a['slot'] * 8 + a['function']
                    alan = c.mem(taban + (bdf << 12), 4096)
                    ofs = 256
                    while ofs:
                        assert 256 <= ofs <= 4092 and ofs % 4 == 0 and (bdf,ofs) not in zincirler
                        deger, = struct.unpack_from('<I', alan, ofs)
                        if deger in (0, 0xffffffff):
                            break
                        zincirler[bdf,ofs] = deger
                        ofs = deger >> 20
                if tur == 'kok':
                    assert zincirler, 'PCIe genişletilmiş yetenek yok'
                assert len(ext) == len(zincirler) and {(b,o):v for b,o,v in ext} == zincirler
            if tur in ('kok','kopru'):
                assert all(any(a['bus'] and a['class_info']['class'] == tip for a in agac) for tip in (0x0108,0x0106))
            print('PCI16',tur,len(agac),'işlev',len(barlar),'BAR',len(kopruler),'köprü', flush=True)
        c.run('pci', incele)
        for aygit,part in [('nvme',nvme),('sata',sata)]:
            c.run(f'cat /diskler/{aygit}0p1/merhaba.txt', Fat(part.read()).get('/merhaba.txt') + b'\n')
        c.shutdown()
    print('PCI16 OK',tur,flush=True)


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('dizin',type=Path)
    p.add_argument('--topoloji',choices=('duz','kok','kopru','cf8'))
    a = p.parse_args()
    port_genislikleri(a.dizin/'k.elf')
    for tur in ([a.topoloji] if a.topoloji else ('duz','kok','kopru','cf8')):
        kos(a.dizin.resolve(),tur)
