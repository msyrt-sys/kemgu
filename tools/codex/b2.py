from yardim import testler
import runpy, struct, zlib
base=runpy.run_path('tools/codex/a3.py')['b']
header='''// Fikstür: python3 tools/codex/b2.py — encode(utf-16le), struct, zlib.
kullan drivers::disk::bolum::{ Bolum, gpt_coz };
kullan drivers::fs::fat32_dizin::{ DizinGirdisi, dizin_coz };
kullan drivers::ortak::bayt::{ bayt_sifir, yaz_u8 };
'''
cases={}
for kind in ('gpt','lfn'):
 for variant in ('emoji','eslesmemis'):
  name='ABCDEFGHIJKL😀.txt' if kind=='lfn' else 'EFI😀'
  raw=name.encode('utf-16le') if variant=='emoji' else b'A\x00\x00\xd8B\x00'
  expected=name if variant=='emoji' else raw.decode('utf-16le','replace')
  if kind=='gpt':
   data=bytearray(base);data[1080:1152]=bytes(72);data[1080:1080+len(raw)]=raw
   struct.pack_into('<I',data,600,zlib.crc32(data[1024:]));struct.pack_into('<I',data,528,0);struct.pack_into('<I',data,528,zlib.crc32(data[512:604]))
  else:
   short=b'ABCDEF~1TXT';s=0
   for x in short:s=(((s&1)<<7)+(s>>1)+x)&255
   units=list(struct.unpack('<'+'H'*(len(raw)//2),raw));count=(len(units)+12)//13;data=bytearray()
   for seq in range(count,0,-1):
    rec=bytearray(32);rec[0]=seq|(64 if seq==count else 0);rec[11]=15;rec[13]=s
    chunk=units[(seq-1)*13:seq*13]
    if len(chunk)<13:chunk+=[0]+[65535]*(12-len(chunk))
    for off,v in zip(list(range(1,11,2))+list(range(14,26,2))+[28,30],chunk):struct.pack_into('<H',rec,off,v)
    data+=rec
   rec=bytearray(32);rec[:11]=short;rec[11]=32;data+=rec
  s=f'    değişken b: Dizi<dtam8> = bayt_sifir({len(data)});\n'
  for i,x in enumerate(data):
   if x:s+=f'    yaz_u8(b, {i}, {x});\n'
  typ,call=('Bolum','gpt_coz(b, 512)') if kind=='gpt' else ('DizinGirdisi','dizin_coz(b)')
  s+=f'''    eşleş {call} {{ hata(e) => {{ ver yanlış; }} tamam(v) => {{
        değişken xs: Dizi<{typ}> = v;
        eğer dizi_boyut(xs) != 1 {{ ver yanlış; }}
        değişken g: {typ} = dizi_al(xs, 0);
        ver metin_esit(g.ad, "{expected}");
    }} }}'''
  cases[kind+'_'+variant]=s
  print(kind,variant,'birim',len(raw)//2,'beklenen',repr(expected))
testler('tests/drivers/fs/unicode_ad_test.kem',header,cases)
