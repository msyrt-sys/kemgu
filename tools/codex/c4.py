from yardim import testler
from pathlib import Path
import struct
regions=[(0x3000,0x3000,1),(0x1000,0x3000,1),(0x2800,0x1000,2),(0x5000,0x2000,1),(0,0,1)]
# Bağımsız Python uç süpürme kahini.
ends=sorted({x for a,n,t in regions if n for x in (a,a+n)})
expected=[]
for a,z in zip(ends,ends[1:]):
 types=[t for x,n,t in regions if x<=a and x+n>=z]
 if not types:continue
 non=[t for t in types if t!=1];t=min(non) if non else 1
 if expected and expected[-1][0]+expected[-1][1]==a and expected[-1][2]==t:expected[-1]=(expected[-1][0],z-expected[-1][0],t)
 else:expected.append((a,z-a,t))
print(expected,'bayt',sum(n for a,n,t in expected if t==1),'sayfa',sum(max(0,(a+n)//4096-(a+4095)//4096) for a,n,t in expected if t==1))
header='''// Kahin: python3 tools/codex/c4.py; struct.pack ve bağımsız aralık uç süpürmesi.
kullan drivers::bellek::harita::{ BellekBolgesi, e820_coz, mb2_coz, harita_duzenle, kullanilabilir_bayt, sayfa_sayisi };
kullan drivers::ortak::bayt::{ bayt_sifir, yaz_u8, yaz_le32, yaz_le64 };
'''
for fn,data in [('e820_veri',b''.join(struct.pack('<QQI',*r) for r in regions)),('mb2_veri',struct.pack('<IIII',6,16+len(regions)*24,24,0)+b''.join(struct.pack('<QQII',*r,0) for r in regions))]:
 header+=f'işlev {fn}() -> Dizi<dtam8> {{\n    değişken b: Dizi<dtam8> = bayt_sifir({len(data)});\n'
 for i,v in enumerate(data):
  if v:header+=f'    yaz_u8(b, {i}, {v});\n'
 header+='    ver b;\n}\n'
header+='''işlev beklenen(r: sonuç<Dizi<BellekBolgesi>, metin>) -> mantıksal {
    eşleş r { hata(e) => { ver yanlış; } tamam(v) => {
        değişken xs: Dizi<BellekBolgesi> = v;
'''+f'        eğer dizi_boyut(xs) != {len(expected)} {{ ver yanlış; }}\n'
for i,(a,n,t) in enumerate(expected):header+=f'        değişken b{i}: BellekBolgesi = dizi_al(xs, {i}); eğer b{i}.taban != {a} veya b{i}.uzunluk != {n} veya b{i}.tur != {t} {{ ver yanlış; }}\n'
header+=f'        ver kullanilabilir_bayt(xs) == {sum(n for a,n,t in expected if t==1)} ve sayfa_sayisi(xs) == {sum(max(0,(a+n)//4096-(a+4095)//4096) for a,n,t in expected if t==1)};\n    }} }}\n}}\n'
cases={'e820':'    ver beklenen(e820_coz(e820_veri()));','mb2':'    ver beklenen(mb2_coz(mb2_veri()));','kisa':'    eşleş e820_coz(bayt_sifir(19)) { tamam(v) => { ver yanlış; } hata(e) => { ver doğru; } }','bos':'    eşleş e820_coz(bayt_sifir(0)) { hata(e) => { ver yanlış; } tamam(v) => { ver dizi_boyut(v) == 0; } }'}
for name,change in [('adim_sifir','yaz_le32(b, 8, 0);'),('adim_kucuk','yaz_le32(b, 8, 20);'),('boy_tasma','yaz_le32(b, 4, 4294967295);'),('artik','yaz_le32(b, 4, 17);'),('tur','yaz_le32(b, 0, 5);'),('surum','yaz_le32(b, 12, 1);')]:cases[name]=f'    değişken b: Dizi<dtam8> = mb2_veri(); {change}\n    eşleş mb2_coz(b) {{ tamam(v) => {{ ver yanlış; }} hata(e) => {{ ver doğru; }} }}'
for name,base,length in [('negatif',-1,1),('tasma',9223372036854775807,1),('uzunluk',0,-1)]:cases[name]=f'''    değişken b: Dizi<dtam8> = bayt_sifir(20); yaz_le64(b, 0, {base} olarak tam64); yaz_le64(b, 8, {length} olarak tam64);
    eşleş e820_coz(b) {{ tamam(v) => {{ ver yanlış; }} hata(e) => {{ ver metin_esit(e, "adres"); }} }}'''
cases['hizasiz']='''    değişken xs: Dizi<BellekBolgesi> = [];
    dizi_ekle(xs, BellekBolgesi { taban: 4097, uzunluk: 8191, tur: 1 });
    ver kullanilabilir_bayt(xs) == 8191 ve sayfa_sayisi(xs) == 1;'''
testler('tests/drivers/bellek/harita_test.kem',header,cases)
p=Path('Makefile');s=p.read_text(encoding='utf-8');needle='tests/drivers/pci/*_test.kem'
if 'tests/drivers/bellek/*_test.kem' not in s:s=s.replace(needle,needle+' tests/drivers/bellek/*_test.kem')
p.write_text(s,encoding='utf-8');print(len(cases),'test')
