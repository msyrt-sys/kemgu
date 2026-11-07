#!/usr/bin/env python3
from pathlib import Path
import re,struct
s=sorted(Path('/usr/src').glob('*/include/linux/ata.h'))[-1].read_text()
for name,expected in {'SERNO':10,'PROD':27,'COMMAND_SET_2':83,'LBA_CAPACITY_2':100,'SECTOR_SIZE':106,'LOGICAL_SECTOR_SIZE':117}.items():
    assert int(re.search(r'ATA_ID_'+name+r'\s*=\s*(\d+)',s).group(1))==expected
b=bytearray(512)
for word,raw,width in [(10,b'SN-AHCI-01',20),(27,b'AHCI DISK',40)]:
    raw=raw.ljust(width,b' ')
    for i in range(0,width,2):b[word*2+i:word*2+i+2]=raw[i:i+2][::-1]
struct.pack_into('<H',b,166,0x4400);struct.pack_into('<Q',b,200,0x123456789ab);struct.pack_into('<H',b,212,0x4000)
writes='\n'.join(f'    yaz_u8(b,{i},{v});' for i,v in enumerate(b) if v)
Path('tests/drivers/ahci/kimlik_fikstur.kem').write_text('// Kâhin: python3 tools/codex/dalga4_ata_kimlik.py (yerel ata.h, struct, bayt çifti tersleme).\nkullan drivers::ortak::bayt::{ bayt_sifir, yaz_u8 };\ngenel işlev ata_ornek() -> Dizi<dtam8> {\n    değişken b: Dizi<dtam8> = bayt_sifir(512);\n'+writes+'\n    ver b;\n}\n',encoding='utf-8')
print('kapasite',0x123456789ab,'lba48',0x4400,'sektör bilgisi',0x4000,0x5000,'uzun blok',2048*2)
