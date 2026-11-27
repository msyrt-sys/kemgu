"""Q35 açılış diski, klavye, bellek ve disk etiketi çapraz denemeleri."""
import argparse
import os
from collections import Counter
from hashlib import sha256
from pathlib import Path
import struct

from dalga12_fat import Fat
from dalga12_kahin import boot, fsck, sha
from surec_imaj import imajlar
from surec_kahin import SurecConsole


def durumlar():
    # Her taşıma iki klavye ve iki tabloyla görülür; RAM eşlemesi döndürülür.
    for i, aygit in enumerate(('usb', 'nvme', 'sata')):
        for j, klavye in enumerate(('ps2', 'usb')):
            for k, tablo in enumerate(('mbr', 'gpt')):
                yield aygit, klavye, ('256M', '1G', '4G')[(i+j+k)%3], tablo


def mbr_yaz(bolumler):
    # Yalnız yeni test imajında iki GPT kopyasını kaldır. Veri yerinde kalır.
    sektor = bytearray(512)
    struct.pack_into('<I', sektor, 440, 0x3471A89B)
    for p, tur in zip(bolumler, (0xef, 0x0c)):
        struct.pack_into('<BBBBBBBBII', sektor, 446+16*(p.no-1),
                         128 if p.no == 1 else 0, 32, 33, 0, tur, 255, 255, 255, p.start, p.count)
    sektor[510:] = b'\x55\xaa'
    with bolumler[0].disk.open('r+b') as f:
        f.write(sektor+bytes(33*512))
        f.seek(-33*512, 2)
        f.write(bytes(33*512))


def kos(w, durum, smp=1):
    aygit, klavye, ram, tablo = durum
    birimler, nvme, sata = imajlar(w, {'merhaba.elf': (w/'kullanici/merhaba.elf').read_bytes()})
    esp, veri = birimler
    if tablo == 'mbr':
        mbr_yaz(birimler)
    ilk = {p: sha(p) for p in (nvme.disk, sata.disk)}
    esp_oz = sha256(esp.read()).digest()
    ad = aygit+'0p2'
    yol = '/diskler/'+ad
    metin = ' '.join(durum)
    with boot(w, esp, nvme, sata, klavye=klavye, acilis=aygit, ram=ram, smp=smp) as (q, p, seri):
        c = SurecConsole(w, q, p, seri, disk={'usb':'USB', 'nvme':'NVMe', 'sata':'AHCI'}[aygit])
        assert len(q.call('query-cpus-fast')) == smp
        assert q.call('query-memory-size-summary')['base-memory'] == {'256M':256, '1G':1024, '4G':4096}[ram]*2**20
        c.run('bagla --yazilabilir '+ad)
        c.run(f'yaz {yol}/matris.txt {metin}')
        c.run('cat '+yol+'/matris.txt', (metin+'\n').encode())
        exe = yol+'/bin/merhaba.elf'
        c.run('calistir '+exe, (exe+'\n[merhaba.elf pid=1 cikis=1]\n').encode())
        c.shutdown()
    for part in (esp, veri):
        fsck(w, part, f'matris-{part.no}')
    assert Fat(veri.read()).get('/matris.txt') == metin.encode()
    assert sha256(esp.read()).digest() == esp_oz and all(sha(p) == h for p, h in ilk.items())


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('dizin', type=Path)
    p.add_argument('--tekrar', type=int, default=3)
    p.add_argument('--tablo', default=os.environ.get('PC_MATRIS_TABLO'), choices=('mbr', 'gpt'))
    a = p.parse_args()
    assert a.tekrar >= 1
    hatalar = Counter()
    for durum in durumlar():
        if a.tablo and durum[-1] != a.tablo:
            continue
        for tur in range(a.tekrar):
            # Aynı disk, RAM ve klavye ile üç gerçek donanım topolojisi.
            smp = (1, 2, 4)[tur % 3]
            try:
                kos(a.dizin.resolve(), durum, smp)
                print('MATRIS OK', *durum, tur+1, f'vCPU={smp}', flush=True)
            except Exception as exc:
                hatalar[durum] += 1
                print('MATRIS HATA', *durum, tur+1, f'vCPU={smp}', repr(exc)[:350], flush=True)
    if hatalar:
        raise SystemExit(f'Matris başarısızlıkları {dict(hatalar)}')
