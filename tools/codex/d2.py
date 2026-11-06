from yardim import testler,yaz
from pathlib import Path
import gzip,struct
# PSF2 dört glif, 9x2; MSB önce, satır başına iki bayt.
syn=struct.pack('<8I',0x864ab572,0,32,1,4,4,2,9)+bytes([128,128,64,0,255,128,128,0,224,0,64,0,128,0,64,0])+b'A'+ 'ğ'.encode()+b'\xff'+'😀'.encode()+b'\xff?\xfefi\xff'+'�'.encode()+b'\xff'
assert int.from_bytes(syn[:4],'little')==2253043058
def fixture(name,fn,data):
 s=f'// Üretim/kahin: python3 tools/codex/d2.py.\nkullan drivers::ortak::bayt::{{ bayt_sifir, yaz_u8 }};\ngenel işlev {fn}() -> Dizi<dtam8> {{\n    değişken b: Dizi<dtam8> = bayt_sifir({len(data)});\n'
 for i,x in enumerate(data):
  if x:s+=f'    yaz_u8(b, {i}, {x});\n'
 s+='    ver b;\n}\n';yaz('tests/drivers/ekran/veri/'+name+'.kem',s)
fixture('psf_sentetik','sentetik_font',syn)
def parse(data):
 mapping={}
 if data[:2]==b'\x36\x04':
  mode,h=data[2:4];n=512 if mode&1 else 256;off=4+n*h;w=8;size=h
  if not mode&6:return w,h,n,{i:i for i in range(n)}
  for glyph in range(n):
   sequence=False
   while True:
    cp=struct.unpack_from('<H',data,off)[0];off+=2
    if cp==65535:break
    if cp==65534:sequence=True
    elif not sequence:mapping.setdefault(cp,glyph)
 else:
  magic,version,hs,flags,n,size,h,w=struct.unpack_from('<8I',data);assert magic==0x864ab572
  off=hs+n*size
  if not flags&1:return w,h,n,{i:i for i in range(n)}
  for glyph in range(n):
   end=data.index(b'\xff',off);part=data[off:end].split(b'\xfe')[0]
   for ch in part.decode('utf-8'):mapping.setdefault(ord(ch),glyph)
   off=end+1
 return w,h,n,mapping
wanted='ğşıİöçü';options=[]
for p in Path('/usr/share/consolefonts').glob('*.psf.gz'):
 try:
  d=gzip.decompress(p.read_bytes());w,h,n,m=parse(d)
  if all(ord(ch) in m for ch in wanted):options.append((len(d),str(p),d,w,h,n,m))
 except (ValueError,UnicodeError,struct.error):pass
if not options:raise SystemExit('ATLANDI: Türkçe eşlemeli sistem konsol fontu yok')
_,source,real,w,h,n,m=min(options,key=lambda x:(x[0],x[1]))
fixture('psf_gercek','gercek_font',real)
print('Gerçek font',source,len(real),'bayt',w,h,n,{ch:m[ord(ch)] for ch in wanted})
header='''// Python PSF çözümü: tools/codex/d2.py; Türkçe kodlar ord, indeksler bağımsız tablodan.
kullan drivers::ekran::psf::{ PsfFont, psf_coz, glif_indeksi };
kullan tests::drivers::ekran::veri::psf_sentetik::{ sentetik_font };
kullan tests::drivers::ekran::veri::psf_gercek::{ gercek_font };
kullan drivers::ortak::bayt::{ bayt_sifir, yaz_u8, yaz_le16, yaz_le32 };
işlev indeks_mi(f: PsfFont, kod: tam32, i: tam32) -> mantıksal { eşleş glif_indeksi(f, kod) { hiç => { ver yanlış; } değer(v) => { ver v == i; } } }
'''
cases={}
cases['psf2']='''    eşleş psf_coz(sentetik_font()) { hata(e) => { ver yanlış; } tamam(v) => {
        değişken f: PsfFont = v;
        eğer f.genislik != 9 veya f.yukseklik != 2 veya f.glif_sayisi != 4 veya f.glif_bayt != 4 { ver yanlış; }
        eğer değil indeks_mi(f, 65, 0) veya değil indeks_mi(f, 287, 0) veya değil indeks_mi(f, 128512, 1) veya değil indeks_mi(f, 63, 2) veya değil indeks_mi(f, 65533, 3) { ver yanlış; }
        eşleş glif_indeksi(f, 102) { değer(i) => { ver yanlış; } hiç => { ver doğru; } }
    } }'''
s='''    eşleş psf_coz(gercek_font()) { hata(e) => { yazdir_metin(e); ver yanlış; } tamam(v) => {
        değişken f: PsfFont = v;
'''+f'        eğer f.genislik != {w} veya f.yukseklik != {h} veya f.glif_sayisi != {n} {{ ver yanlış; }}\n'
for ch in wanted:s+=f'        eğer değil indeks_mi(f, {ord(ch)}, {m[ord(ch)]}) {{ ver yanlış; }}\n'
cases['gercek']=s+'        ver doğru;\n    } }'
for count,mode in [(256,0),(512,1)]:
 cases['psf1_'+str(count)]=f'''    değişken b: Dizi<dtam8> = bayt_sifir({4+count}); yaz_u8(b, 0, 54); yaz_u8(b, 1, 4); yaz_u8(b, 2, {mode}); yaz_u8(b, 3, 1);
    eşleş psf_coz(b) {{ hata(e) => {{ ver yanlış; }} tamam(v) => {{ değişken f: PsfFont = v; ver f.glif_sayisi == {count} ve indeks_mi(f, {count-1}, {count-1}); }} }}'''
cases['psf1_unicode']='''    değişken b: Dizi<dtam8> = bayt_sifir(774); yaz_u8(b, 0, 54); yaz_u8(b, 1, 4); yaz_u8(b, 2, 2); yaz_u8(b, 3, 1);
    yaz_le16(b, 260, 287); değişken i: tam32 = 262;
    iken i < 774 { yaz_le16(b, i, 65535); i = i+2; }
    eşleş psf_coz(b) { hata(e) => { ver yanlış; } tamam(v) => { değişken f: PsfFont = v; ver indeks_mi(f, 287, 0); } }'''
for name,change,error in [('magic','yaz_u8(b, 0, 0);','imza'),('surum','yaz_le32(b, 4, 1);','surum'),('baslik','yaz_le32(b, 8, 31);','baslik'),('sayisifir','yaz_le32(b, 16, 0);','boyut'),('boy','yaz_le32(b, 20, 3);','glif boyu'),('tasma','yaz_le32(b, 20, 4294967295);','glif boyu'),('genislik','yaz_le32(b, 28, 0);','boyut'),('utf8','yaz_u8(b, 48, 192);','unicode tablo'),('son','yaz_u8(b, '+str(len(syn)-1)+', 65);','unicode tablo')]:
 cases[name]=f'    değişken b: Dizi<dtam8> = sentetik_font(); {change}\n    eşleş psf_coz(b) {{ tamam(v) => {{ ver yanlış; }} hata(e) => {{ ver metin_esit(e, "{error}"); }} }}'
cases['kisa']='    eşleş psf_coz(bayt_sifir(3)) { tamam(v) => { ver yanlış; } hata(e) => { ver metin_esit(e, "kisa psf"); } }'
testler('tests/drivers/ekran/psf_test.kem',header,cases)
yaz('otomasyon/chatgpt/codex_font_kahini.md',f'PSF kahini: python3 tools/codex/d2.py\n\nKaynak: {source}\n\nBoyut: {w}×{h}, {n} glif, {len(real)} bayt.\n\n'+str({ch:m[ord(ch)] for ch in wanted})+'\n')
print(len(cases),'test')
