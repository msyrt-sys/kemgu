#!/usr/bin/env python3
"""USB ch9.h / hid.h sabitleri ve struct.pack tanımlayıcı kâhini."""
from pathlib import Path
import re,struct
ch=Path('/usr/include/linux/usb/ch9.h').read_text();hid=Path('/usr/include/linux/hid.h').read_text()
for n,v in {'USB_REQ_GET_DESCRIPTOR':6,'USB_REQ_SET_ADDRESS':5,'USB_REQ_SET_CONFIGURATION':9,'USB_DT_DEVICE':1,'USB_DT_CONFIG':2,'USB_DT_INTERFACE':4,'USB_DT_ENDPOINT':5,'USB_TYPE_CLASS':32}.items():
    if n=='USB_TYPE_CLASS':assert re.search(r'#define\s+USB_TYPE_CLASS\s+\(0x01\s*<<\s*5\)',ch)
    else:assert int(re.search(r'#define\s+'+n+r'\s+(0x[0-9a-fA-F]+|\d+)',ch).group(1),0)==v
for n,v in {'HID_REQ_SET_PROTOCOL':11,'HID_REQ_SET_IDLE':10}.items():assert int(re.search(n+r'\s*=\s*(0x[0-9A-F]+)',hid).group(1),0)==v
assert re.search(r'HID_DT_HID\s+\(USB_TYPE_CLASS\s*\|\s*0x01\)',hid)
dev=struct.pack('<BBHBBBBHHHBBBB',18,1,0x200,0,0,0,64,0x1234,0x5678,0x100,1,2,3,1)
parts=[struct.pack('<9B',9,4,0,0,1,3,1,1,0),struct.pack('<BBHBBBH',9,33,0x111,31,1,34,63),struct.pack('<BBBBHB',7,5,0x81,3,8,10),struct.pack('<9B',9,4,1,0,2,8,6,80,0),struct.pack('<BBBBHB',7,5,2,2,512,0),struct.pack('<BBBBHB',7,5,0x82,2,512,0)]
config=struct.pack('<BBHBBBBB',9,2,9+sum(map(len,parts)),2,1,0,128,50)+b''.join(parts)
def fn(name,b):return 'genel işlev '+name+'() -> Dizi<dtam8> { değişken b: Dizi<dtam8> = ['+','.join(f'{x} olarak dtam8' for x in b)+']; ver b; }\n'
Path('tests/drivers/usb/tanim_fikstur.kem').write_text('// Kâhin: python3 tools/codex/dalga4_usb.py\n'+fn('usb_aygit_ornek',dev)+fn('usb_yapi_ornek',config),encoding='utf-8')
print('Aygıt',list(dev),'Yapı boyu',len(config),'HID',list(parts[1]))
for args in [(128,6,256,0,18),(0,5,7,0,0),(0,9,1,0,0),(33,11,0,2,0),(33,10,1283,2,0)]:print('SETUP',list(struct.pack('<BBHHH',*args)))
