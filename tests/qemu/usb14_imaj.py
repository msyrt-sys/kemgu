"""Kurulum imajını üretir ve bölüm dosyalarını imajdan yeniden okuyarak denetler."""
import argparse
import os
import struct
import zlib
from pathlib import Path

from dalga12_fat import Fat
from dalga12_kahin import fsck
from surec_imaj import GUNLUK, usb_hazirla

def dogrula(w,parcalar,programlar):
    disk=parcalar[0].disk
    with disk.open("rb") as f:
        for lba in (1,disk.stat().st_size//512-1):
            f.seek(lba*512); h=bytearray(f.read(512))
            assert h[:8]==b"EFI PART"
            boy,crc=struct.unpack_from("<II",h,12)
            assert boy==92
            struct.pack_into("<I",h,16,0)
            assert zlib.crc32(h[:boy])==crc
            tablo,n,boy,crc=struct.unpack_from("<QIII",h,72)
            f.seek(tablo*512)
            assert zlib.crc32(f.read(n*boy))==crc
    for i,p in enumerate(parcalar):
        fsck(w,p,f"usb-bolum-{i+1}")
    data=Fat(parcalar[1].read())
    assert data.get("/NOTLAR/günlük.txt")==GUNLUK
    for ad,exe in programlar.items():
        assert data.get("/BIN/"+ad)==exe,ad
    data.verify_info()

if __name__=="__main__":
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("dizin",type=Path);p.add_argument("hedef",type=Path)
    a=p.parse_args()
    programlar={f.name:f.read_bytes() for f in (a.dizin/"kullanici").glob("*.elf")}
    assert {"duzenle.elf","dosyalar.elf","hesap.elf","kedi.elf","say.elf"}<=programlar.keys()
    parcalar=usb_hazirla(a.dizin,programlar)
    dogrula(a.dizin,parcalar,programlar)
    os.replace(parcalar[0].disk,a.hedef)
    print(f"{a.hedef.resolve()} — {a.hedef.stat().st_size} bayt, {len(programlar)} ELF")
