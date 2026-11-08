#!/usr/bin/env python3
"""Dalga 6 sabitleri: sabit sürümlü QEMU kaynağı, yerel ch9.h ve Python hesabı."""
from pathlib import Path
import urllib.request,hashlib,re,json
root=Path('build/codex6/kaynak');root.mkdir(parents=True,exist_ok=True)
files=['qapi/ui.json','hw/usb/hcd-xhci.c','hw/usb/hcd-xhci.h','hw/usb/dev-hid.c','hw/usb/dev-storage.c']
for name in files:
 p=root/name.replace('/','_')
 if not p.exists():p.write_bytes(urllib.request.urlopen('https://raw.githubusercontent.com/qemu/qemu/v8.2.2/'+name,timeout=30).read())
 print(name,hashlib.sha256(p.read_bytes()).hexdigest())
x=(root/'hw_usb_hcd-xhci.c').read_text()
for name,bit in {'USBCMD_RS':0,'USBCMD_HCRST':1,'USBSTS_CNR':11,'USBSTS_HCE':12,'PORTSC_PR':4,'PORTSC_PP':9,'PORTSC_PRC':21,'TRB_TR_ISP':2,'TRB_TR_IOC':5,'TRB_LK_TC':1}.items():
 assert re.search(r'#define\s+'+name+r'\s+\(1\s*<<\s*'+str(bit)+r'\)',x),name
 print(name,1<<bit)
assert 'seg.size < 16' in x
u=(root/'qapi_ui.json').read_text();keys=u[u.index("{ 'enum': 'QKeyCode'"):];keys=keys[:keys.index(']')]
qcodes=['shift','a','bracket_left','dot','spc','apostrophe','semicolon','ret','i']
assert all("'"+k+"'" in keys for k in qcodes)
print('QKeyCode',qcodes)
ch=Path('/usr/include/linux/usb/ch9.h').read_text()
for name,value in [('USB_REQ_GET_DESCRIPTOR',6),('USB_REQ_SET_CONFIGURATION',9),('USB_DT_DEVICE',1),('USB_DT_CONFIG',2),('USB_DT_ENDPOINT',5)]:
 m=re.search(r'#define\s+'+name+r'\s+(0x[0-9a-fA-F]+|\d+)',ch);assert m and int(m[1],0)==value
 print(name,value)
print('Unicode',[ord(c) for c in 'Ağaç İş\n'])
print('DMA',2**20,'BAR',int('febd0000',16),'event control',(1<<24)|(3<<16)|(32<<10)|1)
print('QEMU VID/PID',int('46f4',16),int('0627',16),1)
ids=Path('/usr/share/hwdata/usb.ids')
print('usb.ids',str(ids) if ids.exists() else 'YOK; sabit sürümlü QEMU dev-storage/dev-hid kaynağı kullanıldı')
