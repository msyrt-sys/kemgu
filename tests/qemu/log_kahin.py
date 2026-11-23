"""Seri kaydın son tam satırları ile konukta dışa aktarılan halkayı karşılaştırır."""
import argparse
from pathlib import Path

from dalga12_fat import Fat
from dalga12_kahin import boot, fsck, sha
from surec_imaj import imajlar
from surec_kahin import SurecConsole


def beklenen(seri):
    veri = seri[seri.index(b'KEMGU-OS UEFI\n'):]
    satirlar = veri.splitlines(keepends=True)
    assert b''.join(satirlar) == veri
    biriken = 0
    for i in range(len(satirlar)-1, -1, -1):
        if biriken+len(satirlar[i]) > 8192:
            break
        biriken += len(satirlar[i])
    else:
        i = -1
    return f'LOG dusen_satir {i+1}\n'.encode()+b''.join(satirlar[i+1:]), i+1


def kos(w):
    iz = ''.join(f'{i:04} çekirdek günlük satırı\n' for i in range(450)).encode()
    usb, nvme, sata = imajlar(w, {'iz.txt': iz})
    ilk = {p.disk: sha(p.disk) for p in (nvme, sata)}
    with boot(w, usb[0], nvme, sata, klavye="ps2") as (q, p, ser):
        c = SurecConsole(w, q, p, ser)
        c.run('bagla --yazilabilir usb0p2')
        c.run('cat /diskler/usb0p2/bin/iz.txt', iz)
        rapor, dusen = beklenen(ser.read_bytes())
        assert dusen > 0
        c.run('log kaydet /diskler/usb0p2/log.txt')
        sayfa, _ = beklenen(ser.read_bytes())
        c.run('log', b''.join(sayfa.splitlines(keepends=True)[:20])+b'Devami: log 2\n')
        ikinci, _ = beklenen(ser.read_bytes())
        c.run('log 2', b''.join(ikinci.splitlines(keepends=True)[20:40])+b'Devami: log 3\n')
        c.shutdown()
    fsck(w, usb[1], 'log')
    assert Fat(usb[1].read()).get('/log.txt') == rapor
    assert all(sha(p) == h for p, h in ilk.items())
    print(f'✓ log sarımı, {dusen} düşen satır, sayfa ve {len(rapor)} dosya baytı', flush=True)


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('dizin', type=Path)
    kos(ap.parse_args().dizin.resolve())
