"""ACPI S5 kapanışını QMP olayı ve kapanıştan sonra FAT32 imajıyla doğrular."""
import json
from pathlib import Path
import re
import struct
import sys

from dalga12_fat import Fat
from dalga12_imaj import images
from dalga12_kahin import Console, boot, fsck


def denetle(w):
    usb, nvme, sata = images(w, 'gpt')
    with boot(w, usb[0], nvme, sata, debug_cikis=False) as (q, p, ser):
        c = Console(w, q, p, ser)
        rapor = ser.read_bytes()
        addr = int(re.search(rb'^ACPI_TABLO FACP (\d+)', rapor, re.M)[1])
        fadt = c.mem(addr, 148)
        dsdt = struct.unpack_from('<Q', fadt, 140)[0] or struct.unpack_from('<I', fadt, 40)[0]
        size = struct.unpack_from('<I', c.mem(dsdt, 36), 4)[0]
        table = c.mem(dsdt, size)
        assert table[:4] == b'DSDT' and sum(table) % 256 == 0
        assert bytes.fromhex('08 5f 53 35 5f 12 06 04 00 00 00 00') in table
        assert f'ACPI_S5 {dsdt} 0 0\n'.encode() in rapor
        c.run('bagla --yazilabilir usb0p2')
        c.run('yaz /diskler/usb0p2/son.txt kapanis')
        c.shutdown()
        for line in q.reader:
            olay = json.loads(line)
            if 'event' in olay:
                q.olaylar.append(olay)
        nedenler = [olay['data'] for olay in q.olaylar if olay['event'] == 'SHUTDOWN']
        assert nedenler == [dict(guest=True, reason='guest-shutdown')], nedenler
    assert Fat(usb[1].read()).get('/son.txt') == b'kapanis'
    fsck(w, usb[1], 's5')
    print(f'ACPI_S5 OK DSDT={dsdt} uzunluk={size} guest-shutdown rc=0', flush=True)


if __name__ == '__main__':
    denetle(Path(sys.argv[1]).resolve())
