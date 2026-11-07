#!/usr/bin/env python3
"""AHCI 1.3.1 H2D FIS, başlık ve PRDT bayt kâhini (Python struct)."""
import struct
from pathlib import Path
lba=0x123456789abc;count=0x1234
fis=bytearray(20);fis[:3]=bytes([0x27,0x80,0x25]);fis[4:7]=lba.to_bytes(6,'little')[:3];fis[7]=0x40;fis[8:11]=lba.to_bytes(6,'little')[3:];struct.pack_into('<H',fis,12,count)
print('FIS',list(fis),'LBA',lba,'bayt',count*512,'DBC/I',(count*512-1)|(1<<31))
print('iki PRDT',12289*512,12289*512-4194304,'DBC/I',((12289*512-4194304)-1)|(1<<31),'ikinci DBA',1048576+4194304)
Path('tests/drivers/ahci/fis_fikstur.kem').write_text('// Kâhin: python3 tools/codex/dalga4_ahci_komut.py\ngenel işlev fis_ornek() -> Dizi<dtam8> { değişken b: Dizi<dtam8> = ['+','.join(f'{v} olarak dtam8' for v in fis)+']; ver b; }\n',encoding='utf-8')
