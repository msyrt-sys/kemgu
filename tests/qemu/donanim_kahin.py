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
    assert gercek == pci_kumesi(c.q), (gercek, pci_kumesi(c.q))
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
    with boot(w, usb[0], nvme, sata) as (q, p, ser):
        c = Console(w, q, p, ser)
        alinan = []
        def rapor(b):
            karsilastir(c, b)
            alinan.append(b)
        c.run('donanim', rapor)
        if tur == 'gpt':
            c.run('bagla --yazilabilir usb0p2')
            c.run('donanim kaydet /diskler/usb0p2/donanim.txt')
        c.shutdown()
    fsck(w, disk, 'donanim')
    assert Fat(disk.read()).get('/donanim.txt') == alinan[0]
    assert all(sha(d) == oz for d, oz in ilk.items())
    print(f'✓ {tur} donanim.txt, PCI, USB, ACPI, MADT, CPU, GOP ve bellek', flush=True)


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('dizin', type=Path)
    dizin = p.parse_args().dizin.resolve()
    for tur in ('gpt', 'mbr'):
        kos(dizin, tur)
