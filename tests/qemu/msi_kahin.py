"""AHCI MSI teslimini konuk vektörü, PCI yazmaçları ve IOAPIC pin sayaçlarıyla karşılaştırır."""
import argparse
import re
import struct
from pathlib import Path

from dalga12_imaj import images
from dalga12_fat import Fat
from dalga12_kahin import boot, Console


def kos(w, yoklama):
    usb, nvme, sata = images(w, 'gpt')
    with boot(w, usb[0], nvme, sata) as (q,p,seri):
        c = Console(w,q,p,seri)
        ham = seri.read_text(encoding='utf-8',errors='replace')
        print('\n'.join(line for line in ham.splitlines() if 'AHCI YOL' in line or 'MSI zaman' in line),flush=True)
        assert 'AHCI YOL '+('yoklama' if yoklama else 'MSI') in ham
        if 'MSI zaman asimi' in ham:
            c.run('cat /diskler/sata0p1/merhaba.txt', Fat(sata.read()).get('/merhaba.txt')+b'\n')
            raise AssertionError('MSI bekleniyordu. Zaman aşımı bildirildi, yoklama ile SATA dosyası okundu.')
        def kesmeler():
            kayit = {}
            def oku(ham):
                kayit.update({int(v):(int(k),int(n)) for v,k,n in re.findall(rb'^KESME (\d+) (\d+) (\d+)$',ham,re.M)})
            c.run('donanim',oku)
            return kayit
        def red(yanit):
            assert yanit == b'KESME CAKISMA RED\n', yanit
        c.run('kesme sina',red)
        a = kesmeler()
        assert {33,44,48,65} <= a.keys(),a
        v = [i for i,(k,n) in a.items() if k == 5]
        assert len(v) == int(not yoklama), a
        sata_bdf = next(d for d in q.call('query-pci')[0]['devices'] if d['class_info']['class'] == 0x0106)
        bdf = sata_bdf['slot']*8+sata_bdf['function']
        pencere, = struct.unpack('<Q', c.mem(c.symbols['pci_ecam_yollari'],8))
        ecam, = struct.unpack('<Q', c.mem(pencere,8))
        cfg = c.mem(ecam+(bdf<<12),256)
        ofs = cfg[52]&252
        while ofs and cfg[ofs] != 5:
            ofs = cfg[ofs+1]&252
        assert ofs != 0
        ctl, = struct.unpack_from('<H',cfg,ofs+2)
        assert bool(ctl&1) == (not yoklama)
        if v:
            assert struct.unpack_from('<H',cfg,4)[0]&1024
            veri, = struct.unpack_from('<H',cfg,ofs+(12 if ctl&128 else 8))
            assert veri == v[0]
        def irq():
            h = q.call('human-monitor-command',{'command-line':'info irq'})
            # HMP her yonga için ayrı sayaç verir. PCI bağlantı hatları 16..23'tür.
            io = h.split('IRQ statistics for ioapic:')[-1].split('IRQ statistics for')[0]
            assert 'IRQ statistics for ioapic:' in h, h
            return {int(n):int(k) for n,k in re.findall(r'^\s*(\d+):\s*(\d+)',io,re.M) if 16 <= int(n) <= 23}
        once=irq()
        c.run('cat /diskler/sata0p1/merhaba.txt', Fat(sata.read()).get('/merhaba.txt')+b'\n')
        sonra=irq()
        b=kesmeler()
        assert once == sonra,(once,sonra)
        if v: assert b[v[0]][1] > a[v[0]][1], ('MSI bekleniyordu',a,b)
        nv = [i for i,(k,n) in b.items() if k == 6]
        assert len(nv) == 1 and 'NVME YOL MSI-X CQ1' in ham, b
        nv = nv[0]
        aygit = next(d for d in q.call('query-pci')[0]['devices'] if d['class_info']['class'] == 0x0108)
        alan = c.mem(ecam+((aygit['slot']*8+aygit['function'])<<12),256)
        of = alan[52]&252
        while of and alan[of] != 17:
            of = alan[of+1]&252
        assert of and struct.unpack_from('<H',alan,of+2)[0]&0xc000 == 0x8000
        yer, = struct.unpack_from('<I',alan,of+4)
        taban = next(r['address'] for r in aygit['regions'] if r['bar'] == (yer&7))
        adr,ust,vektor,maske = struct.unpack('<4I',c.mem(taban+(yer&~7)+16,16))
        assert adr&0xfff00000 == 0xfee00000 and ust == 0 and vektor == nv and maske&1 == 0
        # Zaten bağlanmış diskten ilk dosya okumasını 8 IOPS ile sınırla.
        # CQ1 gerçek I/O gecikmesinde de vektörünü korumalı.
        sinir = dict(device='dnvme', bps=0, bps_rd=0, bps_wr=0, iops=0, iops_rd=8, iops_wr=0)
        q.call('block_set_io_throttle',sinir)
        disk = next(d['inserted'] for d in q.call('query-block') if d['device']=='dnvme')
        assert disk['iops_rd'] == 8
        def okuma_adedi():
            return next(d['stats']['rd_operations'] for d in q.call('query-blockstats') if d.get('device')=='dnvme')
        istek = okuma_adedi()
        c.run('cat /diskler/nvme0p1/merhaba.txt', Fat(nvme.read()).get('/merhaba.txt')+b'\n')
        adet = okuma_adedi()-istek
        assert adet >= 2, adet
        q.call('block_set_io_throttle',dict(sinir, iops_rd=0))
        print(f'NVMe CQ1 gecikme: 8 IOPS, {adet} fiziksel okuma',flush=True)
        son = kesmeler()
        assert son[nv][1] > b[nv][1] and irq() == sonra
        print('NVMe CQ1',nv,b[nv][1],son[nv][1],flush=True)
        print('AHCI', 'yoklama' if yoklama else (v[0],a[v[0]][1],b[v[0]][1]), 'IOAPIC',once,sonra,flush=True)
        c.shutdown()


if __name__ == '__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('dizin',type=Path)
    p.add_argument('--yoklama',action='store_true')
    a=p.parse_args()
    kos(a.dizin.resolve(),a.yoklama)
