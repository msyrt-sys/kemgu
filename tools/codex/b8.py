"""KEMGU seyrek imajı ile dosfstools arasında bağımsız FAT32 kahini."""
from pathlib import Path
import argparse, subprocess, struct, shutil
from yardim import yaz, testler
W=Path('build/codex-kahin')
CONTENT='Merhaba Dünya — ğüşıöç'.encode()
def run(args):
 r=subprocess.run(args,capture_output=True,text=True)
 print(r.stdout,end=''); print(r.stderr,end='')
 if r.returncode: raise SystemExit(f'{args[0]} exit={r.returncode}')
 return r

def hazirla():
 W.mkdir(parents=True,exist_ok=True)
 path=W/'linux.img'
 with path.open('wb') as f:f.truncate(70000*512)
 if not shutil.which('mkfs.fat'):raise SystemExit('ATLANDI: mkfs.fat yok')
 run(['mkfs.fat','--invariant','-F','32','-s','1','-n','LINUX','-i','12345678',str(path)])
 mcopy=shutil.which('mcopy')
 if mcopy:
  (W/'kahin.txt').write_bytes(CONTENT)
  run([mcopy,'-i',str(path),str(W/'kahin.txt'),'::KAHIN.TXT'])
 else:print('ATLANDI: mcopy yok; Linux dosya ekleme ve ters yön içerik kontrolü. Linux boş kök fikstürü yine ayrıştırılır.')
 data=path.read_bytes()
 sb=struct.unpack_from('<H',data,11)[0];ks=data[13];reserved=struct.unpack_from('<H',data,14)[0]
 fat=struct.unpack_from('<I',data,36)[0];first=reserved+data[16]*fat
 last=max(i for i,x in enumerate(data) if x)
 prefix=max((first+ks)*sb,((last+sb)//sb)*sb)
 data=data[:prefix]
 fixture='''// Üretim: python3 tools/codex/b8.py hazirla; mkfs.fat --invariant -F 32 -s 1 -n LINUX -i 12345678.
kullan drivers::ortak::bayt::{ bayt_sifir, yaz_u8 };
genel işlev imaj() -> Dizi<dtam8> {
'''+f'    değişken b: Dizi<dtam8> = bayt_sifir({len(data)});\n'
 for i,x in enumerate(data):
  if x:fixture+=f'    yaz_u8(b, {i}, {x});\n'
 fixture+='    ver b;\n}\n'
 yaz('tests/drivers/fs/veri/linux_fat32.kem',fixture)
 h='''// Bağımsız mkfs.fat fikstürü; araç ve sabit komutları tools/codex/b8.py içinde.
kullan drivers::fs::fat32_bpb::{ Fat32Bpb };
kullan drivers::fs::fat32_dizin::{ DizinGirdisi };
kullan drivers::fs::fat32_imaj::{ DizinResmi, FatKayit, imaj_bpb };
kullan drivers::fs::fat32_oku;
kullan tests::drivers::fs::veri::linux_fat32::{ imaj };
kullan drivers::ortak::bayt::{ oku_u8 };
'''
 cases={'bpb':f'''    eşleş imaj_bpb(imaj()) {{ hata(e) => {{ ver yanlış; }} tamam(v) => {{
        değişken b: Fat32Bpb = v; ver b.sektor_bayt == {sb} ve b.kume_sektor == {ks} ve b.fat_sektor == {fat} ve b.ilk_veri_sektoru == {first};
    }} }}''','liste':f'''    eşleş fat32_oku::dizin_listele(imaj(), "/") {{ hata(e) => {{ ver yanlış; }} tamam(v) => {{ ver dizi_boyut(v) == {1 if mcopy else 0}; }} }}'''}
 if mcopy:
  body='''    eşleş fat32_oku::dosya_oku(imaj(), "/kahin.txt") { hata(e) => { ver yanlış; } tamam(v) => {
        değişken b: Dizi<dtam8> = v;
'''+f'        eğer dizi_boyut(b) != {len(CONTENT)} {{ ver yanlış; }}\n'
  for i,x in enumerate(CONTENT):body+=f'        eğer oku_u8(b, {i}) != {x} {{ ver yanlış; }}\n'
  cases['icerik']=body+'        ver doğru;\n    } }'
 cases['eksik']='    eşleş fat32_oku::dosya_oku(imaj(), "/YOK.TXT") { tamam(v) => { ver yanlış; } hata(e) => { ver metin_esit(e, "bulunamadi"); } }'
 testler('tests/drivers/fs/fat32_linux_test.kem',h,cases)
 print(f'Linux fikstürü: {len(data)} bayt önek, FAT={fat}, ilk={first}; {len(cases)} test')

def denetle():
 path=W/'kemgu.img'; rows={}
 for line in (W/'seyrek.txt').read_text().splitlines():
  number,hexdata=line.split();n=int(number);b=bytes.fromhex(hexdata)
  assert n>=0 and len(b)==512 and n not in rows
  rows[n]=b
 assert 0 in rows
 total=struct.unpack_from('<I',rows[0],32)[0]
 assert total==70000
 with path.open('wb') as f:
  f.truncate(total*512)
  for n,b in rows.items():
   assert n<total;f.seek(n*512);f.write(b)
 if shutil.which('fsck.fat'):run(['fsck.fat','-n',str(path)])
 else:print('ATLANDI: fsck.fat yok')
 if shutil.which('mdir'):run(['mdir','-i',str(path),'::/belgeler'])
 else:print('ATLANDI: mdir yok')
 if shutil.which('mcopy'):
  dest=W/'okunan.txt';dest.unlink(missing_ok=True)
  run(['mcopy','-i',str(path),'::/belgeler/merhaba.txt',str(dest)])
  assert dest.read_bytes()==CONTENT
 else:print('ATLANDI: mcopy yok; dış dosya içeriği karşılaştırması')
 print('FAT32_KAHIN OK',len(rows),'sıfır olmayan sektör')

if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('islem',choices=['hazirla','denetle']);a=p.parse_args()
 (hazirla if a.islem=='hazirla' else denetle)()
