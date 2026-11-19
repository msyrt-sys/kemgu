"""QEMU için üç bağımsız disk ve her FAT32 bölümünün doğrulanabilir sınırları."""
from dataclasses import dataclass
from pathlib import Path
import shutil
import struct
import uuid
import zlib

from fat_yazici import hazirla
from uefi_imaj import hazirla_uefi

SECTOR = 512
DATA = uuid.UUID('EBD0A0A2-B9E5-4433-87C0-68B6B72699C7')
ESP = uuid.UUID('C12A7328-F81F-11D2-BA4B-00A0C93EC93B')
FAT_LEN = 64 * 1024**2 // SECTOR


@dataclass(frozen=True)
class Parti:
    disk: Path
    no: int
    start: int
    count: int
    sig: bytes
    kind: int

    def read(self):
        with self.disk.open('rb') as f:
            f.seek(self.start * SECTOR)
            data = f.read(self.count * SECTOR)
        assert len(data) == self.count * SECTOR
        return data


def guid(name):
    return uuid.uuid5(uuid.NAMESPACE_DNS, 'kemgu-dalga12-' + name).bytes_le


def kopyala(dst, src, bas=0):
    with src.open('rb') as f:
        f.seek(bas*SECTOR)
        for _ in range(FAT_LEN // 2048):
            chunk = f.read(2048*SECTOR)
            assert len(chunk) == 2048*SECTOR
            dst.write(chunk)


def gpt(path, volumes):
    entries = bytearray(128 * 128)
    partitions = []
    for i, (src, offset, type_guid) in enumerate(volumes):
        first = 2048 + i * FAT_LEN
        sig = guid(f'{path.stem}-{i}')
        struct.pack_into('<16s16sQQQ', entries, i*128, type_guid.bytes_le, sig,
                         first, first + FAT_LEN - 1, 0)
        partitions.append(Parti(path,i+1,first,FAT_LEN,sig,2))
    sectors = partitions[-1].start + FAT_LEN + 33
    mbr = bytearray(SECTOR)
    mbr[447:450] = bytes((0, 2, 0))
    mbr[451:454] = bytes((255, 255, 255))
    mbr[450] = 0xEE
    struct.pack_into('<II', mbr, 454, 1, sectors-1)
    mbr[510:] = b'\x55\xaa'

    def header(at, other, table):
        h = bytearray(SECTOR)
        struct.pack_into('<8sIIIIQQQQ16sQIII', h, 0, b'EFI PART', 0x10000, 92,
                         0, 0, at, other, 34, sectors-34, guid(path.stem),
                         table, 128, 128, zlib.crc32(entries))
        struct.pack_into('<I', h, 16, zlib.crc32(h[:92]))
        return h

    with path.open('wb') as f:
        f.write(mbr)
        f.write(header(1,sectors-1,2))
        f.write(entries)
        for p, (src, offset, _) in zip(partitions, volumes):
            f.seek(p.start*SECTOR)
            kopyala(f,src,offset)
        f.seek((sectors-33)*SECTOR)
        f.write(entries)
        f.write(header(sectors-1,1,sectors-33))
    return partitions


def mbr(path, source, offset, signature):
    boot = bytearray(SECTOR)
    struct.pack_into('<I',boot,440,signature)
    struct.pack_into('<BBBBBBBBII',boot,446,0x80,32,33,0,0x0C,255,255,255,2048,FAT_LEN)
    boot[510:] = b'\x55\xaa'
    with path.open('wb') as f:
        f.write(boot)
        f.seek(2048*SECTOR)
        kopyala(f,source,offset)
    return Parti(path,1,2048,FAT_LEN,struct.pack('<I',signature),1)


def images(w, mode):
    source = w/'dalga12-fs.fat'
    hazirla(source)
    boot = w/'dalga12-boot-gpt.img'
    hazirla_uefi(boot,w/'esp/EFI/BOOT/BOOTX64.EFI',w/'k.elf')
    nvme, = gpt(w/'dalga12-nvme.img', [(source,0,DATA)])
    sata = mbr(w/'dalga12-sata.img',source,0,0x98F0D0C2)
    if mode == 'gpt':
        usb = gpt(w/'dalga12-usb.img', [(boot,2048,ESP),(source,0,DATA)])
    else:
        usb = [mbr(w/'dalga12-usb.img',boot,2048,0x72CB901E)]
    return usb,nvme,sata
