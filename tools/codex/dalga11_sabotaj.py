#!/usr/bin/env python3
"""A/B kurallarını derlenebilir boz; gerçek QEMU kırmızısını kaydet; her zaman geri al."""
import os,subprocess,json
from pathlib import Path
w=Path('build/dalga11_sabotaj');w.mkdir(exist_ok=True)
cases=[('ist',Path('runtime/kdl_kesme.c'),b'i == 8 ? 1 :',b'i == 8 ? 0 :','df',10),
 ('ekran',Path('runtime/mimari/x86_64/kem_mmu_hw.kem'),'eğer *flag == 1346457161'.encode(),'eğer *flag == 1346457162'.encode(),'de',1),
 ('oob',Path('runtime/kem_heap.kem'),b'kdl_mimari_dizi_oob(i, boyut);','iken doğru { }'.encode(),'oob',1)]
results=[]
for name,path,old,new,mode,n in cases:
 original=path.read_bytes();assert original.count(old)==1,(name,'eşsiz değil')
 try:
  path.write_bytes(original.replace(old,new))
  subprocess.run(['make','build/bm_x86_start.o','build/bm_x86_kesme.o'],check=True,stdout=subprocess.DEVNULL)
  red=0
  for i in range(n):
   env=os.environ|{'ISTISNA_DERLEYICILER':'build/kemgu','ISTISNA_KIPLER':mode,'ISTISNA_TEKRAR':'1'}
   r=subprocess.run(['bash','test/bm_x86_istisna_harness.sh'],env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=40)
   (w/f'{name}-{i+1}.log').write_bytes(r.stdout)
   assert r.returncode==1 and b'AssertionError' in r.stdout,(name,r.returncode,r.stdout)
   assert b'--llvm' not in r.stdout and '🔴'.encode() not in r.stdout,'derleme hatası sabotaj sayılmaz'
   red+=1
  results.append({'sabotaj':name,'acilis':n,'kirmizi':red});print(results[-1],flush=True)
 finally:
  path.write_bytes(original)
  subprocess.run(['make','build/bm_x86_start.o','build/bm_x86_kesme.o'],check=True,stdout=subprocess.DEVNULL)
(w/'sonuc.json').write_text(json.dumps(results,ensure_ascii=False,indent=2))
