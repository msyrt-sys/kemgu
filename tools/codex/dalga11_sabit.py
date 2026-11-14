#!/usr/bin/env python3
"""Yerel Linux başlığından TSS/IDT düzenini doğrula; sabitleri hesapla."""
import re,struct
from pathlib import Path
roots=sorted(Path('/usr/src').glob('linux-headers-*/arch/x86/include/asm'))
assert roots,'yerel x86 başlıkları yok'
p=next(p for p in reversed(roots) if (p/'processor.h').exists())
s=(p/'processor.h').read_text();s=s[s.index('struct x86_hw_tss {',s.index('#else',s.index('struct x86_hw_tss {'))):]
fields=re.findall(r'\bu(\d+)\s+(\w+)(?:\[(\d+)\])?;',s[:s.index('} __attribute__')])
off=0;offsets={}
for bits,name,count in fields:
 offsets[name]=off;off+=int(bits)//8*int(count or 1)
assert offsets['ist']==0x24 and off==104,(offsets,off)
d=(p/'desc_defs.h').read_text();assert re.search(r'u16\s+ist\s*:\s*3',d)
print('TSS',offsets,'boyut',off,'gate IST maskesi',(1<<3)-1)
print('Yığın',4*4096,'kanarya',int.from_bytes(b'TSIUGMEK','little'))
assert int.from_bytes(b'TSIUGMEK','little')==0x4b454d4755495354
print('Sabitler doğrulandı:',p)
a=(p/'apicdef.h').read_text()
for name,value in {'APIC_ID':32,'APIC_LVR':48,'APIC_TASKPRI':128,'APIC_EOI':176,'APIC_SPIV':240,'APIC_LVTT':800,'APIC_LVT0':848,'APIC_LVT1':864,'APIC_TMICT':896,'APIC_TMCCT':912,'APIC_TDCR':992,'APIC_TDR_DIV_16':3}.items():
 found=re.search(r'#define\s+'+name+r'\s+(0x[0-9a-fA-F]+)\b',a);assert found and int(found[1],16)==value,(name,found)
 print(name,value)
io=(p/'io_apic.h').read_text();io=io[io.index('struct IO_APIC_route_entry'):]
fields=re.findall(r'\b(\w+)\s*:\s*(\d+)\s*,?',io[:io.index('};')]);off=0;offsets={}
for name,n in fields:offsets[name]=off;off+=int(n)
assert offsets['active_low']==13 and offsets['is_level']==15 and offsets['masked']==16 and offsets['destid_0_7']==56,offsets
print('IOAPIC RTE',offsets,'PIT 100 Hz böleni',round(1193182/100),'LVT',48|(1<<16),48|(1<<17),'maskeyi kaldır',((1<<32)-1)^(1<<16))
print('En uzun istisna satırı',len('ISTISNA vek=0x'+'f'*16+' hata=0x'+'f'*16+' rip=0x'+'f'*16+' cr2=0x'+'f'*16+' #DF'),'tampon',2**8)
print('SPSC',2**10,'başlık',3*8,'tahsis',2**10+3*8,'fare imleci',7*7*8,'i8042 CFG',255^(16|32))
