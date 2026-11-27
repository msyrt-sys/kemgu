"""Konuk raporunu QMP, fiziksel ACPI/bellek haritası ve bağımsız FAT okuyucusuyla karşılaştırır."""
from pathlib import Path
import argparse
import re
import struct

from acpi_uefi_kahin import denetle
from dalga12_fat import Fat
from dalga12_imaj import images
from dalga12_kahin import Console, boot, fsck, sha


def pci_kumesi(q):
    sonuc = {}
    def ekle(aygitlar):
        for a in aygitlar:
            bdf = f"{a['bus']:02x}:{a['slot']:02x}.{a['function']}"
            sonuc[bdf] = (a['id']['vendor'], a['id']['device'], a['class_info']['class'])
            ekle(a.get('pci_bridge', {}).get('devices', []))
    for kok in q.call('query-pci'):
        ekle(kok['devices'])
    return sonuc


def karsilastir(c, rapor):
    text = rapor.decode('utf-8')
    gercek = {bdf: (int(v, 16), int(d, 16), int(k, 16) >> 8)
              for bdf, v, d, k in re.findall(r'^PCI ([\da-f:.]+) (\w{4}):(\w{4}) (\w{6})$', text, re.M)}
    qmp_pci = pci_kumesi(c.q)
    assert gercek == qmp_pci, (gercek, qmp_pci)
    # Q35'in yerleşik 82574L kimliği, rapordaki NIC ve kayıtlı kesme aynı aygıtı göstermeli.
    assert any((uretici, aygit) == (0x8086, 0x10D3) for uretici, aygit, _ in qmp_pci.values())
    nic = re.search(r'^NIC e1000e [^\n]+ vektor (\d+)$', text, re.M)
    assert nic and re.search(rf'^KESME {nic[1]} 7 \d+$', text, re.M), text
    hmp = lambda komut: c.q.call('human-monitor-command', {'command-line': komut})
    assert len(gercek) == len(re.findall(r'Bus\s+\d+, device\s+\d+, function\s+\d+', hmp('info pci')))
    usb = hmp('info usb')
    cihazlar = re.findall(r'^USB (\d+) (\w{4}):(\w{4}) ([\d ]+)$', text, re.M)
    assert len(cihazlar) == len(re.findall(r'Device \d+\.\d+', usb)) == 2, (cihazlar, usb)
    assert sorted(v[3] for v in cihazlar) == ['3', '8']
    assert any((v[1], v[2]) == ('0627', '0001') for v in cihazlar)
    assert {'KLAVYE USB', 'FARE PS/2', 'DISK usb0', 'DISK nvme0', 'DISK ahci0'} <= set(text.splitlines())
    cpu = c.q.call('query-cpus-fast')[0]['qom-path']
    ozellik = lambda ad: c.q.call('qom-get', {'path': cpu, 'property': ad})
    assert ('CPU '+ozellik('vendor')+' '+ozellik('model-id')).strip() in text
    seri = c.ser.read_text(encoding='utf-8', errors='replace')
    adr = int(re.search(r'BILGI_ADRES (\d+)', seri)[1])
    alan = struct.unpack('<24Q', c.mem(adr, 192))
    harita = c.mem(alan[9], alan[10])
    desc = [struct.unpack_from('<IIQQQQ', harita, i) for i in range(0, len(harita), alan[11])]
    assert f'BELLEK bolge sayfa {len(desc)} {sum(d[4] for d in desc if d[0] == 7)}' in text
    assert 'GOP '+' '.join(map(str, alan[5:9])) in text
    kok = re.search(r'^ACPI_KOK .+$', seri, re.M)[0]
    denetle(lambda a, n, path: c.mem(a, n), c.w, desc, kok+'\n'+text)
    m = re.search(r'^ACPI_TABLO APIC (\d+) (\d+)$', text, re.M)
    madt = c.mem(int(m[1]), int(m[2]))
    pos, n = 44, 0
    while pos < len(madt):
        boy = madt[pos+1]
        assert boy >= 2 and pos+boy <= len(madt)
        pos += boy
        n += 1
    assert f'MADT lapic girdiler {int.from_bytes(madt[36:40], "little")} {n}' in text


def kos(w, tur):
    usb, nvme, sata = images(w, tur)
    disk = usb[-1]
    ilk = {d: sha(d) for d in (nvme.disk, sata.disk)}
    with boot(w, usb[0], nvme, sata, ag_aygitlari=('-nic', 'user,model=e1000e')) as (q, p, ser):
        c = Console(w, q, p, ser)
        def sayaclar():
            tablo, = struct.unpack('<Q', c.mem(c.symbols['kesme_tablo'],8))
            return {v:(kim,n) for v,(kim,n,_,_) in enumerate(struct.iter_unpack('<4Q',c.mem(tablo,8192))) if 1 <= kim <= 7}
        nic_alanlari = ('ag_rx_sayi', 'ag_tx_sayi', 'ag_rx_bayt', 'ag_tx_bayt', 'ag_atilan')
        def nic_sayaclari():
            return tuple(int.from_bytes(c.mem(c.symbols[ad], 8), 'little') for ad in nic_alanlari)
        alinan = []
        alt = {}
        ust = sayaclar()
        cpular = sorted(q.call('query-cpus-fast'), key=lambda v:v['cpu-index'])
        apic = [q.call('qom-get', {'path':v['qom-path'], 'property':'apic-id'}) for v in cpular]
        cp = [c.symbols['kdl_cekirdek_bsp']]
        if len(cpular)>1:
            kayit = int.from_bytes(c.mem(c.symbols['smp_ap_kayitlari'],8),'little')
            cp += [int.from_bytes(c.mem(kayit+i*64+48,8),'little') for i in range(1,len(cpular))]
        def tikler():
            veri = [c.mem(adr+16,200) for adr in cp]
            return [(int.from_bytes(v[:8],'little'),int.from_bytes(v[192:200],'little')) for v in veri]
        ekran_tik_alt = tikler()
        disk_tik_alt = [(0,0)]*len(cp)
        disk_tik_ust = ekran_tik_alt
        ekran_alt = nic_sayaclari()
        disk_alt = (0,) * len(nic_alanlari)
        disk_ust = ekran_alt
        def rapor(b):
            karsilastir(c, b)
            alinan.append(b)
        c.run('donanim', rapor)
        ekran_ust = nic_sayaclari()
        ekran_tik_ust = tikler()
        if tur == 'gpt':
            c.run('bagla --yazilabilir usb0p2')
            alt = sayaclar()
            disk_alt = nic_sayaclari()
            disk_tik_alt = tikler()
            c.run('donanim kaydet /diskler/usb0p2/donanim.txt')
            ust = sayaclar()
            disk_ust = nic_sayaclari()
            disk_tik_ust = tikler()
        c.shutdown()
    fsck(w, disk, 'donanim')
    kayit = Fat(disk.read()).get('/donanim.txt')
    # Dosya ile ekran farklı anlara aittir. Değişen sayaçları QMP RAM örnekleri sınırlar.
    kalip = rb'^KESME (\d+) (\d+) (\d+)\n'
    # Otomatik MBR raporu ilk DHCP çerçevesinden önce diske yazılabilir.
    nic_kalibi = rb' rx (\d+) tx (\d+) rx_bayt (\d+) tx_bayt (\d+) atilan (\d+)(?= vektor )'
    cpu_kalibi = rb'^ISLEMCI (\d+) (\d+) (\d+) (\d+) (\d+)$'
    def sabit_alanlar(rapor, once, sonra, tik_alt, tik_ust):
        satirlar = [tuple(map(int, s)) for s in re.findall(cpu_kalibi, rapor, re.M)]
        assert len(satirlar)==len(cp) and [s[1] for s in satirlar]==apic, satirlar
        for i,(no,_,tik,dilim,bosta) in enumerate(satirlar):
            assert no==i and tik_alt[i][0]<=tik<=tik_ust[i][0] and dilim==0 and 0<=bosta<=100
            assert tik_alt[i][1]*100//max(1,tik_ust[i][0])<=bosta<=min(100,tik_ust[i][1]*100//max(1,tik_alt[i][0]))
        rapor = re.sub(cpu_kalibi, rb'ISLEMCI \g<1> \g<2> tik \g<4> bosta', rapor, flags=re.M)
        nic = re.findall(nic_kalibi, rapor)
        assert len(nic) == 1, rapor
        degerler = tuple(map(int, nic[0]))
        assert all(a <= n <= b for a, n, b in zip(once, degerler, sonra)), (once, degerler, sonra)
        return re.sub(nic_kalibi, b'', re.sub(kalip, b'', rapor, flags=re.M))
    assert sabit_alanlar(kayit, disk_alt, disk_ust, disk_tik_alt, disk_tik_ust) == sabit_alanlar(alinan[0], ekran_alt, ekran_ust, ekran_tik_alt, ekran_tik_ust)
    sayilan = {int(v):(int(k),int(n)) for v,k,n in re.findall(kalip,kayit,re.M)}
    beklenen = set(ust) if tur == 'gpt' else set(ust)-{65}
    assert set(sayilan) == beklenen,(sayilan,ust)
    for v,(kim,n) in sayilan.items():
        assert kim == ust[v][0] and alt.get(v,(kim,0))[1] <= n <= ust[v][1]
    assert all(sha(d) == oz for d, oz in ilk.items())
    print(f'✓ {tur} donanim.txt, PCI, USB, ACPI, MADT, CPU, GOP ve bellek', flush=True)


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('dizin', type=Path)
    dizin = p.parse_args().dizin.resolve()
    for tur in ('gpt', 'mbr'):
        kos(dizin, tur)
