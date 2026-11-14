#!/usr/bin/env python3
"""Yerel ACPICA FADT düzeni ve reset/PS2 bitleri; QEMU TCG CPUID sabitleri."""
from pathlib import Path
import re
paths=sorted(Path('/usr/src').glob('*/include/acpi/actbl.h'));assert paths
p=paths[-1];source=p.read_text();s=source[source.index('struct acpi_table_fadt {'):];s=s[:s.index('};')]
size={'struct acpi_table_header':36,'struct acpi_generic_address':12,'u8':1,'u16':2,'u32':4,'u64':8};off=0;fields={}
for typ,name in re.findall(r'(struct acpi_\w+|u\d+)\s+(\w+)\s*;',s):fields[name]=off;off+=size[typ]
assert [fields[x] for x in ('boot_flags','flags','reset_register','reset_value','pm_timer_block')]==[109,112,116,128,76]
assert re.search(r'ACPI_FADT_8042\s+\(1<<1\)',source) and re.search(r'ACPI_FADT_RESET_REGISTER\s+\(1<<10\)',source)
print('FADT',p,fields)
print('8042/reset bayrakları',1<<1,1<<10,'PIO reset',int('cf9',16),'PM timer',int('608',16),'8042 reset',int('fe',16))
print('TCG CPUID',1<<30,[int.from_bytes(s,'little') for s in (b'TCGT',b'CGTC',b'GTCG')])
print('Ayırt edici MMIO',int('100006021',16),'PIO',int('3fb',16))

import struct
b=bytearray(244);b[:4]=b'FACP';struct.pack_into('<I',b,4,len(b));b[8]=3
struct.pack_into('<I',b,76,0x608);struct.pack_into('<H',b,109,2);struct.pack_into('<I',b,112,0x84a5)
struct.pack_into('<BBBBQ',b,116,1,8,0,0,0xcf9);b[128]=15;b[9]=(-sum(b))&255
print('FADT fikstürü sağlama',b[9],[(i,x) for i,x in enumerate(b) if x])
