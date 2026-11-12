#!/usr/bin/env python3
"""Uni2-Fixed16 PSF baytlarını değişmeden KEMGU kaynağına göm; iki biçimi ölçmek için."""
import argparse,gzip,hashlib
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--bicim',choices=['hex','yaz','parca'],default='parca');p.add_argument('--kaynak',default='/usr/share/consolefonts/Uni2-Fixed16.psf.gz');p.add_argument('--cikti',default='drivers/ekran/font_tr16.kem');a=p.parse_args()
b=gzip.open(a.kaynak,'rb').read()
assert b[:4]==bytes([54,4,3,16])
s='// ÜRETİLMİŞ: python3 tools/codex/font_tr16.py --bicim '+a.bicim+'\n'
s+='// Kaynak: /usr/share/consolefonts/Uni2-Fixed16.psf.gz (console-setup, X11 fixed).\n'
s+='// Lisans: /usr/share/doc/console-setup/copyright: All console fonts are public domain by nature.\n'
s+='// X11 fixed kamu malıdır; Unicode tablosu dahil özgün PSF baytları değişmeden gömülür.\n'
s+=f'// {len(b)} bayt; SHA256 {hashlib.sha256(b).hexdigest()}\n'
if a.bicim=='yaz':
 s+='kullan drivers::ortak::bayt::{ bayt_sifir, yaz_u8 };\ngenel işlev font_tr16() -> Dizi<dtam8> {\n'
 s+=f'    değişken b: Dizi<dtam8> = bayt_sifir({len(b)});\n'
 s+=''.join(f'    yaz_u8(b, {i}, {v});\n' for i,v in enumerate(b) if v)
else:
 s+='işlev nibble(c: tam32) -> tam32 { eğer c <= 57 { ver c - 48; } ver c - 87; }\n'
 if a.bicim=='parca':
  s+='işlev font_parca(b: Dizi<dtam8>, hex: metin) {\n'
 else:
  s+='genel işlev font_tr16() -> Dizi<dtam8> {\n    değişken hex: metin = "'+b.hex()+'";\n    değişken b: Dizi<dtam8> = [];\n'
 s+='    değişken i: tam32 = 0;\n    değişken n: tam32 = metin_uzunluk(hex);\n    iken i < n {\n        değişken v: tam32 = nibble(metin_bayt(hex,i) olarak tam32) * 16 + nibble(metin_bayt(hex,i+1) olarak tam32);\n        dizi_ekle(b,v olarak dtam8); i=i+2;\n    }\n'
 if a.bicim=='parca':
  s+='}\ngenel işlev font_tr16() -> Dizi<dtam8> {\n    değişken b: Dizi<dtam8> = [];\n'
  s+=''.join('    font_parca(b, "'+b[i:i+64].hex()+'");\n' for i in range(0,len(b),64))
s+='    ver b;\n}\n';Path(a.cikti).write_text(s,encoding='utf-8');print(a.bicim,len(s.encode()),len(b))
