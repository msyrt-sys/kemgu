from yardim import testler
import struct
from pathlib import Path
def checksum(b):
 s=0
 for x in b:s=(((s&1)<<7)+(s>>1)+x)&255
 return s
assert checksum(b'FOO     TXT')==101 and checksum(b'UZUNDO~1TXT')==206
name='Uzun Döküman.txt';units=list(struct.unpack('<'+'H'*(len(name.encode('utf-16le'))//2),name.encode('utf-16le')))
print('ad birimi',len(units),'LFN parçaları',(len(units)+12)//13,'saglama',checksum(b'UZUNDO~1TXT'))
short=bytearray(32);short[:11]=b'FOO     TXT';short[11]=32
struct.pack_into('<H',short,20,1);struct.pack_into('<H',short,26,2);struct.pack_into('<I',short,28,1234)
lfn=bytearray()
offsets=list(range(1,11,2))+list(range(14,26,2))+list(range(28,32,2))
for seq in range((len(units)+12)//13,0,-1):
 rec=bytearray(32);rec[0]=seq|(64 if seq==2 else 0);rec[11]=15;rec[13]=206
 chunk=units[(seq-1)*13:seq*13]
 if len(chunk)<13:chunk+=[0]+[65535]*(12-len(chunk))
 for off,v in zip(offsets,chunk):struct.pack_into('<H',rec,off,v)
 lfn+=rec
rec=bytearray(short);rec[:11]=b'UZUNDO~1TXT';lfn+=rec
header='''// Sabit/fikstür: python3 tools/codex/a5.py (UTF-16LE, struct, bağımsız LFN sağlama).
kullan drivers::fs::fat32_dizin::{ DizinGirdisi, kisa_ad_saglama, dizin_coz };
kullan drivers::ortak::bayt::{ bayt_sifir, yaz_u8, yaz_ascii };
'''
for fn,data in [('kisa_veri',short),('uzun_veri',lfn)]:
 header+=f'işlev {fn}() -> Dizi<dtam8> {{\n    değişken b: Dizi<dtam8> = bayt_sifir({len(data)});\n'
 for i,v in enumerate(data):
  if v:header+=f'    yaz_u8(b, {i}, {v});\n'
 header+='    ver b;\n}\n'
header+='''işlev ad_esit(b: Dizi<dtam8>, ad: metin) -> mantıksal {
    eşleş dizin_coz(b) { hata(e) => { ver yanlış; } tamam(v) => {
        // C003: sonuç içindeki yapı dizisini açık tiple bağla.
        değişken xs: Dizi<DizinGirdisi> = v;
        eğer dizi_boyut(xs) != 1 { ver yanlış; }
        değişken g: DizinGirdisi = dizi_al(xs, 0);
        ver metin_esit(g.ad, ad);
    } }
}
'''
cases={
'saglama':'    ver kisa_ad_saglama(kisa_veri(), 0) == 101 ve kisa_ad_saglama(uzun_veri(), 64) == 206 ve kisa_ad_saglama(bayt_sifir(10), 0) == 0;',
'kisa':'''    eşleş dizin_coz(kisa_veri()) { hata(e) => { ver yanlış; } tamam(v) => {
        değişken xs: Dizi<DizinGirdisi> = v;
        eğer dizi_boyut(xs) != 1 { ver yanlış; }
        değişken g: DizinGirdisi = dizi_al(xs, 0);
        ver metin_esit(g.ad, "FOO.TXT") ve g.ilk_kume == 65538 ve g.boyut == 1234 ve değil g.dizin_mi ve g.ozellik == 32;
    } }''',
'uzanti_yok':'    değişken b: Dizi<dtam8> = kisa_veri(); yaz_ascii(b, 0, "README     "); ver ad_esit(b, "README");',
'uzun':'    ver ad_esit(uzun_veri(), "Uzun Döküman.txt");',
'dizin':'''    değişken b: Dizi<dtam8> = kisa_veri(); yaz_u8(b, 11, 16);
    eşleş dizin_coz(b) { hata(e) => { ver yanlış; } tamam(v) => {
        değişken xs: Dizi<DizinGirdisi> = v; değişken g: DizinGirdisi = dizi_al(xs, 0); ver g.dizin_mi;
    } }''',
'kisa_girdi':'    eşleş dizin_coz(bayt_sifir(31)) { hata(e) => { ver metin_esit(e, "kisa dizin"); } tamam(v) => { ver yanlış; } }',
'bos':'    eşleş dizin_coz(bayt_sifir(0)) { hata(e) => { ver yanlış; } tamam(v) => { ver dizi_boyut(v) == 0; } }',
}
for name,off,val in [('saglama_bozuk',13,0),('ara_saglama',45,0),('sira_tekrar',32,2),('ilk_sira_yok',0,2),('sira_sifir',0,64),('sira_buyuk',0,85),('lfn_tur',12,1),('lfn_kume',26,1),('silinen',32,229),('etiket_arada',43,8)]:
 cases[name]=f'    değişken b: Dizi<dtam8> = uzun_veri(); yaz_u8(b, {off}, {val}); ver ad_esit(b, "UZUNDO~1.TXT");'
for name,off,val in [('silinmis',0,229),('etiket',11,8),('son',0,0)]:
 cases[name]=f'''    değişken b: Dizi<dtam8> = kisa_veri(); yaz_u8(b, {off}, {val});
    eşleş dizin_coz(b) {{ hata(e) => {{ ver yanlış; }} tamam(v) => {{ ver dizi_boyut(v) == 0; }} }}'''
cases['son_sonrasi']='''    değişken b: Dizi<dtam8> = bayt_sifir(64); yaz_ascii(b, 32, "FOO     TXT");
    eşleş dizin_coz(b) { hata(e) => { ver yanlış; } tamam(v) => { ver dizi_boyut(v) == 0; } }'''
cases['kisa_ile_saglama'] = '    değişken b: Dizi<dtam8> = uzun_veri(); yaz_u8(b, 13, 0); yaz_u8(b, 45, 0); ver ad_esit(b, "UZUNDO~1.TXT");'
testler('tests/drivers/fs/fat32_dizin_test.kem',header,cases)
print(len(cases),'test')
