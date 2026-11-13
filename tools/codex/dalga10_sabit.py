#!/usr/bin/env python3
"""Dalga 10 fikstürlerinin bağımsız sayısal hesabı; kaynakları değiştirmez."""
from pathlib import Path
import re,struct,uuid
print('ABI',24*8,int.from_bytes(b'KEMUEFI1','little'))
print('GOP',1344*4,1344*4*720)
print('Harita',7*48,(16-1-12)+(8-2)+(8-1),16*4096)
print('Yığın testi',800//2,(800//2)*4096)
print('Sayfa maskesi',((1<<52)-1)&~4095,'UC büyük sayfa',sum(1<<n for n in (0,1,3,4,7)))
s=Path('tests/drivers/acpi/uefi_kok_test.kem').read_text()
for name,signature,fmt,addresses in [('xsdt',b'XSDT','Q',[0x1122334455667788,0xfedc1000,0x112345000]),('rsdt',b'RSDT','I',[0x13579000,0x2468a000])]:
    b=bytearray(36+len(addresses)*struct.calcsize(fmt));b[:4]=signature
    struct.pack_into('<I',b,4,len(b));b[8]=1;b[10:16]=b'PYTEST'
    struct.pack_into('<'+fmt*len(addresses),b,36,*addresses);b[9]=(-sum(b))%256
    fixture=bytearray(len(b));body=s.split('işlev '+name+'()')[1].split('ver b;')[0]
    for offset,value in re.findall(r'yaz_u8\(b,(\d+),(\d+)\)',body):fixture[int(offset)]=int(value)
    assert fixture==b,(name,fixture,b)
    print(signature.decode(),'boy',len(b),'sağlama',b[9],'adresler',addresses)
print('ESP GUID baytları',uuid.UUID('C12A7328-F81F-11D2-BA4B-00A0C93EC93B').bytes_le.hex())
n=2048+64*1024*1024//512+33
c,hs=divmod(n-1,255*63);h,s=divmod(hs,63)
print('İmaj sektör/bayt',n,n*512,'son CHS',h,(s+1)|((c>>8)<<6),c&255)
