"""F1 çok dosyalı bağımsız disk kahini; yalnız build/codex-f1 altına imaj yazar."""
from pathlib import Path
import struct,subprocess,shutil,argparse
from yardim import yaz
W=Path('build/codex-f1')
def u32(b,o):return struct.unpack_from('<I',b,o)[0]
def hazirla():
 s=Path('tests/drivers/fs/fat32_kahin_test.kem').read_text().split('işlev test_dok(')[0]
 s=s.replace('// Sabitler: python3 tools/codex/b8.py; UTF-8 encode ve 70000*512 disk boyu.','// Kahin: python3 tools/codex/f1.py; 110 oluştur, 11 sil, 12 ekle = 111 dosya.')
 s=s.replace('dizin_olustur };','dizin_olustur, dosya_sil };').replace('dosya_oku };','dosya_oku, dizin_listele };')
 s+='''işlev ornek_yolu(i: tam32) -> metin {
    değişken n: metin = ondalik(i); iken metin_uzunluk(n) < 3 { n = metin_birlestir("0", n); }
    ver metin_birlestir(metin_birlestir("/belgeler/alt/cakisma ", n), ".txt");
}
işlev ornek_verisi(i: tam32) -> Dizi<dtam8> {
    değişken b: Dizi<dtam8> = []; dizi_ekle(b, (i & 255) olarak dtam8);
    dizi_ekle(b, (i / 256) olarak dtam8); dizi_ekle(b, (128 | (i % 128)) olarak dtam8); ver b;
}
işlev test_coklu(b: Dizi<dtam8>) -> mantıksal {
    eşleş fat32_yaz::dizin_olustur(b, "/belgeler") { hata(e) => { ver yanlış; } tamam(v) => { } }
    eşleş fat32_yaz::dizin_olustur(b, "/belgeler/alt") { hata(e) => { ver yanlış; } tamam(v) => { } }
    değişken i: tam32 = 0;
    iken i < 110 {
        eşleş fat32_yaz::dosya_yaz(b, ornek_yolu(i), ornek_verisi(i)) { hata(e) => { yazdir_metin(e); ver yanlış; } tamam(n) => { eğer n != 3 { ver yanlış; } } }
        i = i + 1;
    }
    i = 0; iken i < 110 {
        eşleş fat32_yaz::dosya_sil(b, ornek_yolu(i)) { hata(e) => { ver yanlış; } tamam(v) => { eğer değil v { ver yanlış; } } } i = i + 10;
    }
    i = 110; iken i < 122 {
        eşleş fat32_yaz::dosya_yaz(b, ornek_yolu(i), ornek_verisi(i)) { hata(e) => { ver yanlış; } tamam(n) => { eğer n != 3 { ver yanlış; } } } i = i + 1;
    }
    eşleş fat32_oku::dizin_listele(b, "/belgeler/alt") { hata(e) => { ver yanlış; } tamam(v) => { eğer dizi_boyut(v) != 113 { ver yanlış; } } }
    i = 0; iken i < 122 {
        eşleş fat32_oku::dosya_oku(b, ornek_yolu(i)) {
            hata(e) => { eğer i >= 110 veya i % 10 != 0 veya değil metin_esit(e, "bulunamadi") { ver yanlış; } }
            tamam(v) => { değişken veri: Dizi<dtam8> = v;
                eğer i < 110 ve i % 10 == 0 { ver yanlış; }
                eğer dizi_boyut(veri) != 3 { ver yanlış; }
                eğer oku_u8(veri, 0) != (i & 255) veya oku_u8(veri, 1) != i / 256 veya oku_u8(veri, 2) != (128 | (i % 128)) { ver yanlış; }
            }
        } i = i + 1;
    }
    ver doğru;
}
işlev main() -> tam32 {
    eşleş fat32_bicimle::fat32_bicimle(70000 olarak tam64, 1, "KEMGU") {
        hata(e) => { ver 1; }
        tamam(v) => { değişken b: Dizi<dtam8> = v; eğer değil test_coklu(b) { ver 1; } seyrek_dok(b); ver 0; }
    }
}
'''
 yaz('tests/drivers/fs/fat32_coklu_test.kem',s)

def denetle():
 W.mkdir(parents=True,exist_ok=True);data=bytearray(70000*512)
 for line in (W/'seyrek.txt').read_text().splitlines():
  a,h=line.split();a=int(a);raw=bytes.fromhex(h);assert len(raw)==512 and 0<=a<70000;data[a*512:(a+1)*512]=raw
 image=W/'coklu.img';image.write_bytes(data)
 r=subprocess.run(['fsck.fat','-n',str(image)],capture_output=True,text=True);print(r.stdout);assert r.returncode==0,r.stderr
 sb=struct.unpack_from('<H',data,11)[0];spc=data[13];reserved=struct.unpack_from('<H',data,14)[0];fsz=u32(data,36);first=reserved+data[16]*fsz;count=(u32(data,32)-first)//spc
 fat=[u32(data,reserved*sb+4*i)&0xfffffff for i in range(count+2)]
 assert data[reserved*sb:(reserved+fsz)*sb]==data[(reserved+fsz)*sb:(reserved+2*fsz)*sb]
 def chain(k):
  seen=set();out=[]
  while k<0xffffff8:
   assert 2<=k<len(fat) and k not in seen;seen.add(k);out.append(k);k=fat[k]
  return out
 def content(k):return b''.join(data[(first+(c-2)*spc)*sb:(first+(c-1)*spc)*sb] for c in chain(k))
 def checksum(short):
  s=0
  for b in short:s=(((s&1)<<7)+(s>>1)+b)&255
  return s
 def directory(k):
  out={};lfns={};checks=[];expected=0
  raw=content(k)
  for off in range(0,len(raw),32):
   rec=raw[off:off+32]
   if rec[0]==0:break
   if rec[0]==229:lfns={};checks=[];continue
   if rec[11]==15:
    seq=rec[0]&31
    if rec[0]&64:lfns={};checks=[];expected=seq
    assert seq==expected;expected-=1;checks.append(rec[13]);lfns[seq]=b''.join(rec[o:o+2] for o in list(range(1,11,2))+list(range(14,26,2))+list(range(28,32,2)));continue
   short=rec[:11]
   if lfns:
    assert expected==0 and all(x==checksum(short) for x in checks)
    units=b''.join(lfns[i] for i in sorted(lfns));words=struct.unpack('<'+'H'*(len(units)//2),units);words=words[:words.index(0)] if 0 in words else words
    name=struct.pack('<'+'H'*len(words),*words).decode('utf-16le')
   else:name=short[:8].decode().rstrip()+('.'+short[8:].decode().rstrip() if short[8:].strip() else '')
   lfns={};checks=[]
   if rec[11]&8:continue
   assert name not in out
   out[name]=(struct.unpack_from('<H',rec,20)[0]<<16|struct.unpack_from('<H',rec,26)[0],u32(rec,28),short)
  return out
 root=directory(u32(data,44));docs=directory(root['belgeler'][0]);files=directory(docs['alt'][0]);actual={k:v for k,v in files.items() if k not in ('.','..')}
 expected={f'cakisma {i:03d}.txt':bytes([i&255,i//256,128|(i%128)]) for i in range(122) if i>=110 or i%10!=0}
 assert len(expected)==111 and actual.keys()==expected.keys()
 shorts=[]
 for name,(cluster,size,short) in actual.items():
  assert content(cluster)[:size]==expected[name];shorts.append(short)
 assert len(set(shorts))==111 and any(b'~100' in s for s in shorts)
 free=sum(x==0 for x in fat[2:]);assert u32(data,512+488)==free and u32(data,3584+488)==free
 for tool in ('mdir','mcopy'):
  if not shutil.which(tool):print('ATLANDI:',tool,'yok; bağımsız Python FAT/LFN/içerik denetimi çalıştı')
 print('F1_KAHIN OK',len(actual),'dosya; 2 alt dizin; ~100+ çakışmaları; boş küme',free)
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('islem',choices=['hazirla','denetle']);a=p.parse_args();(hazirla if a.islem=='hazirla' else denetle)()
