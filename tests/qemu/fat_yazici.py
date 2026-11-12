#!/usr/bin/env python3
"""KEMGU'dan bağımsız FAT32/GPT yazıcısı ve dosya okuyucu; yalnız Python stdlib."""
from pathlib import Path
import struct,zlib,uuid,sys
SEKTOR=512
MERHABA='Merhaba Dünya — ğüşıöç'.encode()
NOT=b'Bagimsiz FAT32 yazicisi\n'
KAYDIR=''.join(f'Satır {i:02d} ğ\n' for i in range(70)).encode('utf-8')
def hazirla(path,gpt=False,kaydir=False):
    toplam=64*1024*1024//SEKTOR; ayr=32; fs=1
    while fs*128 < toplam-ayr-2*fs+2:fs+=1
    ilk=ayr+2*fs;kume=toplam-ilk
    b=bytearray(toplam*SEKTOR)
    b[:3]=b'\xeb\x58\x90';b[3:11]=b'PYFAT32 '
    struct.pack_into('<HBHBHHBHHHII',b,11,512,1,ayr,2,0,0,0xf8,0,63,255,2048 if gpt else 0,toplam)
    struct.pack_into('<IHHIHH',b,36,fs,0,0,2,1,6)
    b[64]=0x80;b[66]=0x29;struct.pack_into('<I',b,67,0x08080808)
    b[71:82]=b'KEMGU8     ';b[82:90]=b'FAT32   ';b[510:512]=b'\x55\xaa'
    struct.pack_into('<I',b,512,0x41615252);struct.pack_into('<IIII',b,512+484,0x61417272,kume-7,9,0)
    struct.pack_into('<I',b,512+508,0xaa550000)
    b[6*512:7*512]=b[:512];b[7*512:8*512]=b[512:1024]
    fat=bytearray(fs*512)
    for idx,val in enumerate([0x0ffffff8,0x0fffffff,0x0fffffff,4,5,0x0fffffff,0x0fffffff,0x0fffffff,0x0fffffff]):struct.pack_into('<I',fat,idx*4,val)
    b[ayr*512:(ayr+fs)*512]=fat;b[(ayr+fs)*512:ilk*512]=fat
    def entry(short,cluster,size,attr=32):
        e=bytearray(32);e[:11]=short; e[11]=attr
        struct.pack_into('<H',e,20,cluster>>16);struct.pack_into('<HI',e,26,cluster&65535,size)
        # 2026-01-01: FAT date; valid date for fsck.
        struct.pack_into('<H',e,24,((2026-1980)<<9)|(1<<5)|1)
        return e
    def named(name,short,cluster,size,attr=32):
        # LFN, independent UTF-16LE encoder and 8.3 checksum.
        units=list(struct.unpack('<'+'H'*(len(name.encode('utf-16le'))//2),name.encode('utf-16le')))+[0]
        while len(units)%13:units.append(65535)
        chk=0
        for x in short:chk=(((chk&1)<<7)+(chk>>1)+x)&255
        out=bytearray()
        for i in reversed(range(len(units)//13)):
            e=bytearray(32);e[0]=(i+1)|(64 if i==len(units)//13-1 else 0);e[11]=15;e[13]=chk
            for ofs,v in zip([1,3,5,7,9,14,16,18,20,22,24,28,30],units[i*13:i*13+13]):struct.pack_into('<H',e,ofs,v)
            out+=e
        return out+entry(short,cluster,size,attr)
    def put(c,data):b[(ilk+c-2)*512:(ilk+c-2)*512+len(data)]=data
    root=entry(b'KEMGU8     ',0,0,8)+named('dalga8.txt',b'DALGA8  TXT',3,1300)+named('belgeler',b'BELGELER   ',6,0,16)+named('merhaba.txt',b'MERHABA TXT',7,len(MERHABA))
    put(2,root);put(3,bytes((i*7+3)&255 for i in range(1300)))
    put(6,entry(b'.          ',6,0,16)+entry(b'..         ',0,0,16)+named('not.txt',b'NOT     TXT',8,len(NOT)))
    put(7,MERHABA);put(8,NOT)
    if kaydir:
        # Ek dosya bağımsız yazıcıyla oluşturulur; eski imaj biçimi varsayılan kalır.
        adet=(len(KAYDIR)+511)//512
        for i in range(adet):
            for taban in (ayr,ayr+fs):
                struct.pack_into('<I',b,taban*512+(9+i)*4,10+i if i+1<adet else 0x0fffffff)
        for taban in (512,7*512):struct.pack_into('<II',b,taban+488,kume-7-adet,9+adet)
        put(2,root+named('kaydir.txt',b'KAYDIR  TXT',9,len(KAYDIR)))
        put(9,KAYDIR)
    if not gpt:Path(path).write_bytes(b);return
    bas=2048;son=bas+toplam-1;n=son+34
    disk=bytearray(n*512);disk[bas*512:(son+1)*512]=b
    disk[510:512]=b'\x55\xaa';disk[446+4]=0xee;struct.pack_into('<II',disk,446+8,1,n-1)
    entries=bytearray(128*128)
    entries[:16]=uuid.UUID('EBD0A0A2-B9E5-4433-87C0-68B6B72699C7').bytes_le
    entries[16:32]=uuid.uuid5(uuid.NAMESPACE_DNS,'kemgu-dalga8-partition').bytes_le
    struct.pack_into('<QQQ',entries,32,bas,son,0);entries[56:68]='KEMGU8'.encode('utf-16le')
    def header(lba,alt,tablo):
        h=bytearray(512);struct.pack_into('<8sIIIIQQQQ16sQIII',h,0,b'EFI PART',0x10000,92,0,0,lba,alt,34,n-34,uuid.uuid5(uuid.NAMESPACE_DNS,'kemgu-dalga8-disk').bytes_le,tablo,128,128,zlib.crc32(entries))
        struct.pack_into('<I',h,16,zlib.crc32(h[:92]));return h
    disk[2*512:34*512]=entries;disk[(n-33)*512:(n-1)*512]=entries
    disk[512:1024]=header(1,n-1,2);disk[(n-1)*512:]=header(n-1,1,n-33)
    Path(path).write_bytes(disk)
def bolum(path):
    b=Path(path).read_bytes()
    if b[512:520]==b'EFI PART':
        bas,son=struct.unpack_from('<QQ',b,1024+32);b=b[bas*512:(son+1)*512]
    return b
def oku(path,ad):
    b=bolum(path);sb=struct.unpack_from('<H',b,11)[0];ks=b[13];ayr=struct.unpack_from('<H',b,14)[0];fs=struct.unpack_from('<I',b,36)[0];ilk=ayr+b[16]*fs
    def chain(c):
        seen=set();out=bytearray()
        while c<0x0ffffff8:
            assert 2<=c<(len(b)//sb-ilk)//ks+2 and c not in seen;seen.add(c)
            off=(ilk+(c-2)*ks)*sb;out+=b[off:off+ks*sb];c=struct.unpack_from('<I',b,ayr*sb+c*4)[0]&0xfffffff
        return out
    data=chain(struct.unpack_from('<I',b,44)[0])
    for part in ad.strip('/').split('/'):
        short=(part.split('.')[0].upper().ljust(8)+('' if '.' not in part else part.split('.')[1].upper()).ljust(3)).encode();found=False
        for i in range(0,len(data),32):
            if data[i]==0:break
            if data[i:i+11]==short and data[i+11]!=15:
                c=(struct.unpack_from('<H',data,i+20)[0]<<16)|struct.unpack_from('<H',data,i+26)[0];size=struct.unpack_from('<I',data,i+28)[0];attr=data[i+11]
                data=chain(c);found=True;break
        assert found,part
    return bytes(data if attr&16 else data[:size])
if __name__=='__main__':
    if sys.argv[1]=='yaz':hazirla(sys.argv[2],'--gpt' in sys.argv)
    elif sys.argv[1]=='denetle':
        assert oku(sys.argv[2],'/dalga8.txt')==bytes((i*31+11)&255 for i in range(1300))
        assert oku(sys.argv[2],'/merhaba.txt')==MERHABA
        assert oku(sys.argv[2],'/belgeler/not.txt')==NOT
        print('PY_FAT32 OK: dalga8.txt 1300 bayt; iki dokunulmamış dosya aynı')
