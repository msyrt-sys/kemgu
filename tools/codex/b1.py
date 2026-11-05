from yardim import testler
header='''// Unicode vektörleri: python3 tools/codex/b1.py; Python encode/decode(strict/replace).
kullan drivers::ortak::utf8::{ utf8_bayt_coz, utf8_coz, utf8_kodla, utf16le_coz, utf16le_kodla };
kullan drivers::ortak::bayt::{ bayt_sifir, yaz_u8, oku_u8 };
işlev kod_esit(a: Dizi<tam32>, b: Dizi<tam32>) -> mantıksal {
    eğer dizi_boyut(a) != dizi_boyut(b) { ver yanlış; }
    değişken i: tam32 = 0;
    iken i < dizi_boyut(a) { eğer dizi_al(a, i) != dizi_al(b, i) { ver yanlış; } i = i + 1; }
    ver doğru;
}
işlev bayt_esit(a: Dizi<dtam8>, b: Dizi<dtam8>) -> mantıksal {
    eğer dizi_boyut(a) != dizi_boyut(b) { ver yanlış; }
    değişken i: tam32 = 0;
    iken i < dizi_boyut(a) { eğer oku_u8(a, i) != oku_u8(b, i) { ver yanlış; } i = i + 1; }
    ver doğru;
}
'''
def ints(xs):return '['+', '.join(map(str,xs))+']'
def bytes_var(data):
 return f'    değişken b: Dizi<dtam8> = bayt_sifir({len(data)});\n'+''.join(f'    yaz_u8(b, {i}, {x});\n' for i,x in enumerate(data))
cases={}
for name,s in [('bos',''),('turkce','Türkçe ğüşıİöç Iı'),('emoji','A😀Z'),('sinir','\x01\x7f\x80\u07ff\u0800\ud7ff\ue000\uffff\U00010000\U0010ffff')]:
 expr='"'+s+'"' if name!='sinir' else 'utf8_kodla(kodlar)'
 cases[name]=f'    değişken kodlar: Dizi<tam32> = {ints(map(ord,s))};\n'+bytes_var(s.encode('utf-16le'))+f'''    değişken s: metin = {expr};
    ver kod_esit(utf8_coz(s), kodlar) ve metin_esit(utf8_kodla(kodlar), s) ve
        kod_esit(utf16le_coz(b, 0, {len(s.encode('utf-16le'))//2}), kodlar) ve bayt_esit(utf16le_kodla(kodlar), b);'''
# Bozuk baytları geçerli kodlamaların tek baytlık parçalarından oluştur.
# Python strict decoder her başarısız başlangıçta tam bir bayt ilerler.
sources=['é','€','😀','\u0800','\ud000','\U0010ffff']
byte_expr={}
for s in sources:
 for i,x in enumerate(s.encode()):byte_expr.setdefault(x,f'metin_kes(kod_metin({ord(s)}), {i}, 1)')
byte_expr[65]='"A"'
raws=[b'\x80',b'\xc3',b'\xe2\x82',b'\xf0\x9f\x98',b'\xe0\x80\x80',b'\xed\xa0\x80',b'\xf0\x80\x80\x80',b'\xf4\xbf\xbf\xbf',b'\xc3A',b'\xe2A\xac',b'\x80A\xf0\x9f\x98\x80']
def decode_one(raw):
 out=[];i=0
 while i<len(raw):
  found=False
  for n in range(1,5):
   try:s=raw[i:i+n].decode('utf-8','strict')
   except UnicodeDecodeError:continue
   if len(s)==1:
    out.append(ord(s));i+=n;found=True;break
  if not found:out.append(65533);i+=1
 return out
for i,raw in enumerate(raws):
 expr='""'
 for x in raw:expr=f'metin_birlestir({expr}, {byte_expr[x]})'
 cases[f'bozuk_utf8_{i}']=f'    değişken s: metin = {expr};\n    değişken beklenen: Dizi<tam32> = {ints(decode_one(raw))};\n    ver kod_esit(utf8_coz(s), beklenen);'
for i,raw in enumerate([b'\x00\xd8',b'\x00\xdc',b'\x00\xd8A\x00',b'A',b'\x00\xd8\x00\xd8',b'\x00\xd8\x00\xdc']):
 cases[f'bozuk_utf16_{i}']=bytes_var(raw)+f'    değişken beklenen: Dizi<tam32> = {ints(map(ord,raw.decode("utf-16le","replace")))};\n    ver kod_esit(utf16le_coz(b, 0, 8), beklenen);'
cases['sinir_ofset']=bytes_var('x😀\0A'.encode('utf-16le'))+'''    değişken emoji: Dizi<tam32> = [128512];
    değişken eksik: Dizi<tam32> = [65533];
    ver kod_esit(utf16le_coz(b, 2, 4), emoji) ve kod_esit(utf16le_coz(b, 2, 1), eksik) ve
        dizi_boyut(utf16le_coz(b, -1, 1)) == 0 ve dizi_boyut(utf16le_coz(b, 2147483647, 1)) == 0 ve
        dizi_boyut(utf16le_coz(b, 0, -1)) == 0;'''
cases['gecersiz_skaler']='''    değişken k: Dizi<tam32> = [-1, 55296, 57343, 1114112];
    değişken beklenen: Dizi<tam32> = [65533, 65533, 65533, 65533];
    ver kod_esit(utf8_coz(utf8_kodla(k)), beklenen) ve kod_esit(utf16le_coz(utf16le_kodla(k), 0, 4), beklenen);'''
cases['yasak_utf8_baslangic'] = bytes_var(bytes([192,128,193,128,245,255])) + '''    değişken beklenen: Dizi<tam32> = [65533,65533,65533,65533,65533,65533];
    ver kod_esit(utf8_bayt_coz(b, 0, 6), beklenen);'''
cases['bayt_araligi'] = bytes_var('A😀Z'.encode()) + '''    değişken beklenen: Dizi<tam32> = [128512];
    ver kod_esit(utf8_bayt_coz(b, 1, 4), beklenen) ve dizi_boyut(utf8_bayt_coz(b, -1, 1)) == 0 ve
        dizi_boyut(utf8_bayt_coz(b, 0, 2147483647)) == 0 ve dizi_boyut(utf8_bayt_coz(b, 7, 0)) == 0;'''
testler('tests/drivers/ortak/utf8_test.kem',header,cases)
print(len(cases),'test; emoji',ord('😀'),'UTF16',list('😀'.encode('utf-16le')))
