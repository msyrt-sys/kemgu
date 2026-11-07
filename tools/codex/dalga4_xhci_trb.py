#!/usr/bin/env python3
from pathlib import Path
import re,struct
s=Path('build/codex4/linux-xhci.h').read_text()
for name,num in {'TRB_NORMAL':1,'TRB_SETUP':2,'TRB_DATA':3,'TRB_STATUS':4,'TRB_LINK':6,'TRB_ENABLE_SLOT':9,'TRB_ADDR_DEV':11,'TRB_CONFIG_EP':12,'TRB_TRANSFER':32,'TRB_COMPLETION':33,'TRB_PORT_STATUS':34}.items():
    assert int(re.search(r'#define\s+'+name+r'\s+(\d+)',s).group(1))==num
normal=struct.pack('<QII',131072,512,(1<<10)|(1<<5)|1)
event=struct.pack('<QII',65536,1<<24,(32<<10)|(3<<16)|(1<<24)|1)
def fn(name,b):return 'genel işlev '+name+'() -> Dizi<dtam8> { değişken b: Dizi<dtam8> = ['+','.join(f'{v} olarak dtam8' for v in b)+']; ver b; }\n'
Path('tests/drivers/usb/trb_fikstur.kem').write_text('// Kâhin: python3 tools/codex/dalga4_xhci_trb.py\n'+fn('normal_trb_ornek',normal)+fn('olay_trb_ornek',event),encoding='utf-8')
print('normal',list(normal),'event',list(event))
print('slot DW0',(3<<20)|(3<<27),'EP DW1',(512<<16)|(6<<3)|(3<<1),'Link',6<<10|3,'Link addr',4096+3*16)
print('control mask',63<<10|1,'event EHB',8192|8,'64K',1<<16)
