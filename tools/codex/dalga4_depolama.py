#!/usr/bin/env python3
"""GPT birincil başlık + 128 girdi; bağımsız zlib/uuid/struct kâhini."""
from pathlib import Path
import struct
import uuid
import zlib
sectors=80000;start=34;length=70000
b=bytearray(start*512)
b[450]=0xee;struct.pack_into('<II',b,454,1,sectors-1);struct.pack_into('<H',b,510,0xaa55)
ent=bytearray(128*128)
ent[:16]=uuid.UUID('EBD0A0A2-B9E5-4433-87C0-68B6B72699C7').bytes_le
ent[16:32]=uuid.UUID('23456789-1234-5678-9abc-def012345678').bytes_le
struct.pack_into('<QQQ',ent,32,start,start+length-1,0);ent[56:64]='VERI'.encode('utf-16le')
b[1024:1024+len(ent)]=ent
header=bytearray(92);header[:8]=b'EFI PART'
struct.pack_into('<IIIIQQQQ',header,8,0x10000,92,0,0,1,sectors-1,start,sectors-34)
header[56:72]=uuid.UUID('12345678-1234-5678-9abc-def012345678').bytes_le
struct.pack_into('<QIII',header,72,2,128,128,zlib.crc32(ent))
struct.pack_into('<I',header,16,zlib.crc32(header));b[512:604]=header
fat=(70000-32+2+129)//130;first=32+2*fat;prefix=first+2
print('GPT CRC',zlib.crc32(ent),struct.unpack_from('<I',header,16)[0],'FAT',fat,first,prefix,'başlangıç',start)
assert prefix==1112
writes='\n'.join(f'    yaz_u8(b,{i},{v});' for i,v in enumerate(b) if v)
text='''// Kâhin: python3 tools/codex/dalga4_depolama.py (zlib.crc32, uuid.bytes_le, struct).
kullan drivers::ortak::bayt::{ bayt_sifir, yaz_u8 };
kullan drivers::fs::fat32_bicimle;
kullan drivers::fs::fat32_yaz;
kullan drivers::fs::fat32_bpb::{ Fat32Bpb };
kullan drivers::fs::fat32_imaj::{ DizinResmi, FatKayit };
kullan drivers::fs::fat32_dizin::{ DizinGirdisi };
genel işlev gpt_on_ek() -> Dizi<dtam8> {
    değişken b: Dizi<dtam8> = bayt_sifir(17408);
'''+writes+'''
    ver b;
}
genel işlev depolama_verisi(n: tam32) -> Dizi<dtam8> {
    değişken b: Dizi<dtam8> = bayt_sifir(n); değişken i: tam32 = 0;
    iken i < n { yaz_u8(b,i,((i*29+7)&255) olarak tam64); i=i+1; } ver b;
}
genel işlev depolama_ornek() -> sonuç<Dizi<dtam8>,metin> {
    değişken fat: Dizi<dtam8> = [];
    eşleş fat32_bicimle::fat32_bicimle(70000,1,"") { hata(e) => { ver hata(e); } tamam(v) => { fat=v; } }
    değişken once: Dizi<dtam8> = [111 olarak dtam8,110 olarak dtam8,99 olarak dtam8,101 olarak dtam8];
    eşleş fat32_yaz::dosya_yaz(fat,"/KAYIT.TXT",once) { hata(e) => { ver hata(e); } tamam(v) => { eğer v != 4 { ver hata("fikstur yazimi"); } } }
    değişken b: Dizi<dtam8> = gpt_on_ek(); değişken i: tam32 = 0;
    iken i < dizi_boyut(fat) { dizi_ekle(b,dizi_al(fat,i)); i=i+1; }
    ver tamam(b);
}
'''
Path('tests/drivers/ortak/depolama_fikstur.kem').write_text(text,encoding='utf-8')
print('dosya',list(b'once'),'yeni uzunluk',1300,'ilk/son',7,((1300-1)*29+7)&255)
