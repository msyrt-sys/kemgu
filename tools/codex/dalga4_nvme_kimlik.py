#!/usr/bin/env python3
"""Identify alan ofsetlerini yerel Linux C yapılarından Python ile hesaplar."""
from pathlib import Path
import re
import struct
s=sorted(Path('/usr/src').glob('*/include/linux/nvme.h'))[-1].read_text()
def offsets(name,stop):
    block=s.split('struct '+name+' {',1)[1].split('};',1)[0]
    out={};pos=0
    for line in block.splitlines():
        m=re.match(r'\s*(__le64|__le32|__le16|__u8|char)\s+(\w+)(?:\[(\d+)\])?;',line)
        if not m:continue
        ty,field,n=m.groups();size={'__le64':8,'__le32':4,'__le16':2,'__u8':1,'char':1}[ty]
        pos=(pos+size-1)//size*size;out[field]=pos;pos+=size*int(n or 1)
        if field==stop:break
    return out,pos
c,_=offsets('nvme_id_ctrl','nn');n,end=offsets('nvme_id_ns','eui64')
assert {k:c[k] for k in ['vid','sn','mn','fr','mdts','nn']}==dict(vid=0,sn=4,mn=24,fr=64,mdts=77,nn=516)
assert n['nsze']==0 and n['nlbaf']==25 and n['flbas']==26 and end==128
print('CTRL', {k:c[k] for k in ['vid','sn','mn','fr','mdts','nn']},'NS',n['nsze'],n['nlbaf'],n['flbas'],'LBAF',end)
ctrl=bytearray(4096);struct.pack_into('<H',ctrl,c['vid'],0x1234)
for key,value,width in [('sn',b'  SN-123',20),('mn',b'NVME TEST',40),('fr',b'1.0',8)]:ctrl[c[key]:c[key]+width]=value.ljust(width,b' ')
ctrl[c['mdts']]=2;struct.pack_into('<I',ctrl,c['nn'],3)
ns=bytearray(4096);struct.pack_into('<Q',ns,0,1<<40);ns[25]=1;ns[26]=1
struct.pack_into('<HBB',ns,128,0,9,0);struct.pack_into('<HBB',ns,132,0,12,2)
def function(name,b):
    writes='\n'.join(f'    yaz_u8(b,{i},{v});' for i,v in enumerate(b) if v)
    return f'genel işlev {name}() -> Dizi<dtam8> {{\n    değişken b: Dizi<dtam8> = bayt_sifir(4096);\n{writes}\n    ver b;\n}}\n'
Path('tests/drivers/nvme/kimlik_fikstur.kem').write_text('// Kâhin: python3 tools/codex/dalga4_nvme_kimlik.py\nkullan drivers::ortak::bayt::{ bayt_sifir, yaz_u8 };\n'+function('denetleyici_ornek',ctrl)+function('ad_alani_ornek',ns),encoding='utf-8')
print('kapasite',1<<40,'blok',1<<12)
