from yardim import testler,yaz
from pathlib import Path
import struct,subprocess,re
b=bytearray(512);b[:7]=b'\x7fELF\x02\x01\x01'
struct.pack_into('<HHIQQQIHHHHHH',b,16,2,62,1,0x400001,64,0,0,64,56,2,0,0,0)
struct.pack_into('<IIQQQQQQ',b,64,1,5,384,0x400000,0,16,32,1)
struct.pack_into('<IIQQQQQQ',b,120,1,6,400,0x500000,0,16,32,1)
header='''// Fikstürler: python3 tools/codex/c6.py; struct ve readelf -lhW /bin/true.
kullan drivers::elf::elf64::{ Elf64, ElfSegment, elf64_coz };
kullan drivers::ortak::bayt::{ bayt_sifir, yaz_u8, yaz_le16, yaz_le32, yaz_le64 };
kullan tests::drivers::elf::veri::gercek_elf::{ gercek_imaj };
işlev elf_verisi() -> Dizi<dtam8> {
    değişken b: Dizi<dtam8> = bayt_sifir(512);
'''
for i,x in enumerate(b):
 if x:header+=f'    yaz_u8(b, {i}, {x});\n'
header+='    ver b;\n}\n'
cases={}
cases['yuklemeler']='''    eşleş elf64_coz(elf_verisi()) { hata(e) => { ver yanlış; } tamam(v) => {
        değişken e: Elf64 = v; değişken xs: Dizi<ElfSegment> = e.yuklemeler;
        eğer e.makine != 62 veya e.tur != 2 veya e.giris != 4194305 veya dizi_boyut(xs) != 2 { ver yanlış; }
        değişken x: ElfSegment = dizi_al(xs, 0);
        ver x.dosya_ofseti == 384 ve x.sanal_adres == 4194304 ve x.dosya_boyu == 16 ve x.bellek_boyu == 32 ve x.bayraklar == 5;
    } }'''
for name,change,error in [('imza','yaz_u8(b, 0, 0);','imza'),('sinif','yaz_u8(b, 4, 1);','bicim'),('endian','yaz_u8(b, 5, 2);','bicim'),('makine','yaz_le16(b, 18, 1);','makine'),('ph_boyu','yaz_le16(b, 54, 55);','baslik boyu'),('ph_tasma','yaz_le64(b, 32, 9223372036854775807);','program basligi'),('dosya_buyuk','yaz_le64(b, 96, 33);','dosya siniri'),('dosya_sinir','yaz_le64(b, 72, 511);','dosya siniri'),('wx','yaz_le32(b, 68, 7);','wx'),('ortusme','yaz_le64(b, 136, 4194320);','ortusme'),('giris_rw','yaz_le64(b, 24, 5242880);','giris'),('giris_son','yaz_le64(b, 24, 4194336);','giris'),('adres_tasma','yaz_le64(b, 80, 9223372036854775807);','bellek siniri'),('hiza','yaz_le64(b, 112, 3);','hiza'),('ph_sayi','yaz_le16(b, 56, 65535);','program basligi')]:
 cases[name]=f'    değişken b: Dizi<dtam8> = elf_verisi(); {change}\n    eşleş elf64_coz(b) {{ tamam(v) => {{ ver yanlış; }} hata(e) => {{ ver metin_esit(e, "{error}"); }} }}'
cases['kisa']='    eşleş elf64_coz(bayt_sifir(63)) { tamam(v) => { ver yanlış; } hata(e) => { ver metin_esit(e, "kisa elf"); } }'
cases['aarch64']='''    değişken b: Dizi<dtam8> = elf_verisi(); yaz_le16(b, 18, 183);
    eşleş elf64_coz(b) { hata(e) => { ver yanlış; } tamam(v) => { değişken e: Elf64 = v; ver e.makine == 183; } }'''
real=Path('/bin/true').read_bytes();entry,phoff=struct.unpack_from('<QQ',real,24);phsize,phnum=struct.unpack_from('<HH',real,54)
assert phsize==56 and phoff+phnum*56<=4096
loads=[struct.unpack_from('<IIQQQQQQ',real,phoff+i*56) for i in range(phnum) if struct.unpack_from('<I',real,phoff+i*56)[0]==1]
readelf=subprocess.run(['readelf','-lhW','/bin/true'],check=True,capture_output=True,text=True).stdout
assert int(re.search(r'Entry point address:\s*(0x[0-9a-f]+)',readelf).group(1),16)==entry
rl=[line for line in readelf.splitlines() if line.strip().startswith('LOAD ')]
assert len(rl)==len(loads)
for line,seg in zip(rl,loads):
 values=[int(x,16) for x in re.findall(r'0x[0-9a-f]+',line)]
 assert values==[seg[2],seg[3],seg[4],seg[5],seg[6],seg[7]]
fixture='''// /bin/true ilk 4KiB; dosya boyu korunup kalan yükler sıfırlandı. Çalıştırılmaz.
// Kahin: python3 tools/codex/c6.py ve readelf -lhW /bin/true.
kullan drivers::ortak::bayt::{ bayt_sifir, yaz_u8 };
genel işlev gercek_imaj() -> Dizi<dtam8> {
'''+f'    değişken b: Dizi<dtam8> = bayt_sifir({len(real)});\n'
for i,x in enumerate(real[:4096]):
 if x:fixture+=f'    yaz_u8(b, {i}, {x});\n'
fixture+='    ver b;\n}\n';yaz('tests/drivers/elf/veri/gercek_elf.kem',fixture)
s='''    eşleş elf64_coz(gercek_imaj()) { hata(e) => { yazdir_metin(e); ver yanlış; } tamam(v) => {
        değişken e: Elf64 = v; değişken xs: Dizi<ElfSegment> = e.yuklemeler;
'''+f'        eğer e.giris != {entry} veya e.makine != {struct.unpack_from("<H",real,18)[0]} veya dizi_boyut(xs) != {len(loads)} {{ ver yanlış; }}\n'
for i,x in enumerate(loads):s+=f'''        değişken s{i}: ElfSegment = dizi_al(xs, {i});
        eğer s{i}.bayraklar != {x[1]} veya s{i}.dosya_ofseti != {x[2]} veya s{i}.sanal_adres != {x[3]} veya s{i}.dosya_boyu != {x[5]} veya s{i}.bellek_boyu != {x[6]} veya s{i}.hiza != {x[7]} {{ ver yanlış; }}
'''
cases['gercek']=s+'        ver doğru;\n    } }'
testler('tests/drivers/elf/elf64_test.kem',header,cases)
p=Path('Makefile');s=p.read_text(encoding='utf-8');needle='tests/drivers/bellek/*_test.kem'
if 'tests/drivers/elf/*_test.kem' not in s:s=s.replace(needle,needle+' tests/drivers/elf/*_test.kem')
p.write_text(s,encoding='utf-8')
print(len(cases),'test; /bin/true',len(real),'bayt; giriş',entry,'LOAD',len(loads))
print('\n'.join(rl))
