#!/usr/bin/env python3
"""UEFI ABI sayıları: ctypes alan yerleşimi ve isteğe bağlı resmi EDK2 başlık karşılaştırması."""
import ctypes as c,re,sys,uuid
from pathlib import Path
class Baslik(c.Structure):
    _fields_=[('imza',c.c_uint64),('surum',c.c_uint32),('boy',c.c_uint32),('crc',c.c_uint32),('ayrilmis',c.c_uint32)]
class Sistem(c.Structure):
    _fields_=[('bas',Baslik),('uretici',c.c_uint64),('surum',c.c_uint32)]+[(x,c.c_uint64) for x in 'girdi_handle girdi cikti_handle cikti hata_handle hata runtime servis adet tablolar'.split()]
assert Sistem.servis.offset==96 and Sistem.tablolar.offset==112 and c.sizeof(Sistem)==120
assert int.from_bytes(b'KEMUEFI1','little')==3551447044756161867
assert 24*8==192 and 16*4096==65536 and 32*1024*1024==33554432
print('Sistem: servis=96 adet=104 tablolar=112 boy=120; bilgi=192; magic=3551447044756161867')
if len(sys.argv)>1:
    d=Path(sys.argv[1]);s=(d/'UefiSpec.h').read_text();s=s[:s.index('} EFI_BOOT_SERVICES;')];s=s[s.rfind('typedef struct {'):]
    names=re.findall(r'^\s+(?:EFI_\w+|VOID)\s+\*?(\w+);',s,re.M)
    for name,off in [('AllocatePages',40),('GetMemoryMap',56),('HandleProtocol',152),('ExitBootServices',232),('SetWatchdogTimer',256),('LocateProtocol',320)]:
        assert 24+(names.index(name)-1)*8==off
        print(name,off)
    for file,key in [('GraphicsOutput.h','EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID'),('SimpleFileSystem.h','EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID'),('LoadedImage.h','EFI_LOADED_IMAGE_PROTOCOL_GUID')]:
        s=(d/file).read_text();i=s.index('#define '+key);j=s.index('}',s.index('}',i)+1)
        ns=[int(v,16) for v in re.findall(r'0x[0-9a-fA-F]+',s[i:j])]
        b=ns[0].to_bytes(4,'little')+ns[1].to_bytes(2,'little')+ns[2].to_bytes(2,'little')+bytes(ns[3:])
        print(key,uuid.UUID(bytes_le=b))
