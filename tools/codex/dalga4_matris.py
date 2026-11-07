"""İki bölümlü bağımsız GPT: p1 ESP, p2 veri; fiziksel sıra kasıtlı ters."""
from pathlib import Path
import struct,uuid,zlib
sectors=160000
ent=bytearray(16384)
for i,(guid,start,length,name) in enumerate([
 ('C12A7328-F81F-11D2-BA4B-00A0C93EC93B',70034,70000,'ESP'),
 ('EBD0A0A2-B9E5-4433-87C0-68B6B72699C7',34,70000,'VERI')]):
 o=i*128;ent[o:o+16]=uuid.UUID(guid).bytes_le
 ent[o+16:o+32]=uuid.UUID(int=i+1).bytes_le
 struct.pack_into('<QQQ',ent,o+32,start,start+length-1,0)
 enc=name.encode('utf-16le');ent[o+56:o+56+len(enc)]=enc
b=bytearray(34*512);b[450]=238;struct.pack_into('<IIH',b,454,1,sectors-1,0);struct.pack_into('<H',b,510,0xaa55)
h=bytearray(92);h[:8]=b'EFI PART';struct.pack_into('<IIIIQQQQ',h,8,0x10000,92,0,0,1,sectors-1,34,sectors-34)
h[56:72]=uuid.UUID(int=123).bytes_le;struct.pack_into('<QIII',h,72,2,128,128,zlib.crc32(ent));struct.pack_into('<I',h,16,zlib.crc32(h))
b[512:604]=h;b[1024:]=ent
out='// Python struct/zlib/uuid: dalga4_matris.py.\nkullan drivers::ortak::bayt::{ bayt_sifir, yaz_u8 };\ngenel işlev matris_gpt() -> Dizi<dtam8> {\n    değişken b: Dizi<dtam8> = bayt_sifir(17408);\n'
out+=''.join(f'    yaz_u8(b,{i},{v});\n' for i,v in enumerate(b) if v)+'    ver b;\n}\n'
Path('tests/drivers/sim/matris_fikstur.kem').write_text(out)
print('GPT CRC',zlib.crc32(ent),struct.unpack_from('<I',h,16)[0],'p1',70034,140033,'p2',34,70033)
print('seri X',0x12345678,'şema',hex(0xEF),'ESP bytes',uuid.UUID('C12A7328-F81F-11D2-BA4B-00A0C93EC93B').bytes_le.hex())
