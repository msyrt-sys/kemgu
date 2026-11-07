#!/usr/bin/env python3
"""Linux v6.17 xHCI başlıkları + Python bit/ofset kâhini."""
from pathlib import Path
import urllib.request,hashlib,struct,re
for name in ['xhci.h','xhci-caps.h','xhci-port.h','xhci-ext-caps.h']:
    p=Path('build/codex4/linux-'+name)
    if not p.exists():
        p.parent.mkdir(parents=True,exist_ok=True)
        with urllib.request.urlopen('https://raw.githubusercontent.com/torvalds/linux/v6.17/drivers/usb/host/'+name,timeout=20) as r:p.write_bytes(r.read())
    print(name,hashlib.sha256(p.read_bytes()).hexdigest())
cap=Path('build/codex4/linux-xhci-caps.h').read_text();port=Path('build/codex4/linux-xhci-port.h').read_text()
assert '0xfffffffc' in cap and '~0x1f' in cap
for n,bit in {'PORT_CONNECT':0,'PORT_PE':1,'PORT_RESET':4,'PORT_POWER':9,'PORT_CSC':17,'PORT_RC':21}.items():assert re.search(n+r'\s+\(1 << '+str(bit)+r'\)',port)
b=struct.pack('<8I',0x01100020,16|(8<<8)|(4<<24),(3<<21)|(7<<27),0,(256<<16)|5,8195,4127,0)
Path('tests/drivers/usb/xhci_fikstur.kem').write_text('// Kâhin: python3 tools/codex/dalga4_xhci.py\ngenel işlev xhci_cap_ornek() -> Dizi<dtam8> { değişken b: Dizi<dtam8> = ['+','.join(f'{v} olarak dtam8' for v in b)+']; ver b; }\n',encoding='utf-8')
v=1|2|(1<<9)|(3<<10)|(1<<17)|(1<<21)|(1<<25)
rw=(3<<14)|(1<<9)|(7<<25);change=sum(1<<i for i in range(17,24))
print('PORTSC',v,'reset',(v&rw)|16,'ack',(v&rw)|(1<<17),'rw',rw,'change',change)
print('PORT4',32+1024+3*16,'DB16',8192+16*4,'INTR7',4096+32+7*32,'scratch',3*32+7)
