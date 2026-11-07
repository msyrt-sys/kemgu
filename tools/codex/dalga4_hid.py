"""HID kullanım tablosunu görevdeki KEY eşlemesinden ve yerel Linux başlığından üretir."""
from pathlib import Path
import re
h=Path('/usr/include/linux/input-event-codes.h').read_text()
keys={n:int(v,0) for n,v in re.findall(r'^#define\s+(KEY_\w+)\s+(0x[0-9a-fA-F]+|[0-9]+)\b',h,re.M)}
names=list('ABCDEFGHIJKLMNOPQRSTUVWXYZ')+list('1234567890')+['ENTER','ESC','BACKSPACE','TAB','SPACE','MINUS','EQUAL','LEFTBRACE','RIGHTBRACE','BACKSLASH']
m={4+i:keys['KEY_'+n] for i,n in enumerate(names)}
for u,n in {0x33:'SEMICOLON',0x34:'APOSTROPHE',0x35:'GRAVE',0x36:'COMMA',0x37:'DOT',0x38:'SLASH',0x39:'CAPSLOCK',0x64:'102ND'}.items():m[u]=keys['KEY_'+n]
assert all(v<128 for v in m.values())
s='// Python üretimi: dalga4_hid.py, /usr/include/linux/input-event-codes.h.\n'
s+='genel işlev hid_set1(k: tam32) -> tam32 {\n'
s+=''.join(f'    eğer k == {u} {{ ver {v}; }}\n' for u,v in m.items())+'    ver 0;\n}\n'
Path('drivers/usb/hid_tus_tablosu.kem').write_text(s)
print('KEY sayısı',len(m),'Ağaç İş Unicode',list(map(ord,'Ağaç İş')))
print('signed sınırlar',[(x,x-256 if x>=128 else x) for x in [0,1,127,128,255]])
