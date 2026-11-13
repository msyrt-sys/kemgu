#!/usr/bin/env python3
"""Bağımsız Python FAT32 + GPT ESP yazıcısı; KEMGU sürücüsü çağrılmaz."""
from pathlib import Path
import sys,struct,zlib,uuid,subprocess,tempfile
from fat_yazici import hazirla,oku,MERHABA,NOT,KAYDIR

def hazirla_uefi(path,efi,elf):
    path=Path(path)
    with tempfile.TemporaryDirectory(dir=path.parent) as tmp:
        base=Path(tmp)/'base.fat';hazirla(base,False,True);b=bytearray(base.read_bytes())
    sb=512;ayr=struct.unpack_from('<H',b,14)[0];fs=struct.unpack_from('<I',b,36)[0]
    ilk=ayr+2*fs;toplam=len(b)//sb
    sonraki=struct.unpack_from('<I',b,512+492)[0];baslangic=sonraki
    def koy(c,data):b[(ilk+c-2)*sb:(ilk+c-2)*sb+len(data)]=data
    def ayir(data):
        nonlocal sonraki
        n=max(1,(len(data)+sb-1)//sb);ilk_kume=sonraki
        assert sonraki+n<=toplam-ilk+2,'FAT dolu'
        for i in range(n):
            for fat in (ayr,ayr+fs):struct.pack_into('<I',b,fat*sb+(sonraki+i)*4,sonraki+i+1 if i+1<n else 0xfffffff)
        koy(sonraki,data);sonraki+=n;return ilk_kume
    def entry(name,c,size=0,directory=False):
        assert len(name)==11
        e=bytearray(32);e[:11]=name;e[11]=16 if directory else 32
        struct.pack_into('<H',e,20,c>>16);struct.pack_into('<HI',e,26,c&65535,size)
        struct.pack_into('<H',e,24,((2026-1980)<<9)|(1<<5)|1)
        return e
    efidir=ayir(bytes(512));bootdir=ayir(bytes(512));kerneldir=ayir(bytes(512))
    efi=Path(efi).read_bytes();elf=Path(elf).read_bytes()
    assert efi[:2]==b'MZ' and elf[:4]==b'\x7fELF'
    efifile=ayir(efi);elffile=ayir(elf)
    def directory(c,parent,children):koy(c,entry(b'.          ',c,directory=True)+entry(b'..         ',parent,directory=True)+children)
    directory(efidir,0,entry(b'BOOT       ',bootdir,directory=True))
    directory(bootdir,efidir,entry(b'BOOTX64 EFI',efifile,len(efi)))
    directory(kerneldir,0,entry(b'CEKIRDEKELF',elffile,len(elf)))
    root=ilk*sb;end=root
    while b[end]!=0:end+=32
    assert end+96<=root+sb
    b[end:end+64]=entry(b'EFI        ',efidir,directory=True)+entry(b'KEMGU      ',kerneldir,directory=True)
    label=b'KEMGU10    ';b[root:root+11]=label
    for off in (0,6*sb):
        b[off+71:off+82]=label;struct.pack_into('<I',b,off+28,2048)
    for off in (sb,7*sb):
        free=struct.unpack_from('<I',b,off+488)[0]
        struct.pack_into('<II',b,off+488,free-(sonraki-baslangic),sonraki)
    bas=2048;son=bas+toplam-1;n=son+34
    disk=bytearray(n*sb);disk[bas*sb:(son+1)*sb]=b
    # UEFI 2.10 §5.2.3: koruyucu MBR'nin başlangıç/son CHS alanları da doldurulur.
    disk[447:450]=bytes([0,2,0])
    cylinder,hs=divmod(n-1,255*63);head,sector=divmod(hs,63)
    disk[451:454]=bytes([head,(sector+1)|((cylinder>>8)<<6),cylinder&255]) if cylinder<=1023 else bytes([255]*3)
    disk[510:512]=b'\x55\xaa';disk[450]=0xee;struct.pack_into('<II',disk,454,1,n-1)
    entries=bytearray(128*128)
    entries[:16]=uuid.UUID('C12A7328-F81F-11D2-BA4B-00A0C93EC93B').bytes_le
    entries[16:32]=uuid.uuid5(uuid.NAMESPACE_DNS,'kemgu-uefi-esp').bytes_le
    struct.pack_into('<QQQ',entries,32,bas,son,0);entries[56:70]='KEMGU10'.encode('utf-16le')
    def header(lba,alt,tablo):
        h=bytearray(sb);struct.pack_into('<8sIIIIQQQQ16sQIII',h,0,b'EFI PART',0x10000,92,0,0,lba,alt,34,n-34,uuid.uuid5(uuid.NAMESPACE_DNS,'kemgu-uefi-disk').bytes_le,tablo,128,128,zlib.crc32(entries))
        struct.pack_into('<I',h,16,zlib.crc32(h[:92]));return h
    disk[2*sb:34*sb]=entries;disk[(n-33)*sb:(n-1)*sb]=entries
    disk[sb:2*sb]=header(1,n-1,2);disk[(n-1)*sb:]=header(n-1,1,n-33)
    path.write_bytes(disk)
    assert oku(path,'/EFI/BOOT/BOOTX64.EFI')==efi
    assert oku(path,'/KEMGU/CEKIRDEK.ELF')==elf
    assert oku(path,'/merhaba.txt')==MERHABA and oku(path,'/belgeler/not.txt')==NOT and oku(path,'/kaydir.txt')==KAYDIR
    with tempfile.TemporaryDirectory(dir=path.parent) as tmp:
        fat=Path(tmp)/'esp.fat';fat.write_bytes(b);subprocess.run(['fsck.fat','-n',str(fat)],check=True)
    print(f'UEFI_IMAJ OK: GPT ESP {len(disk)} bayt; EFI={len(efi)}, ELF={len(elf)}, FAT iki kopya, Python okuyucu + fsck temiz',flush=True)
if __name__=='__main__':hazirla_uefi(*sys.argv[1:])
