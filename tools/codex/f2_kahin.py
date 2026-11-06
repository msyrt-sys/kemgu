"""fsck.fat'ın kurtardığı küme listesi ile KEMGU erişilebilirlik sonucunu karşılaştırır."""
from pathlib import Path
import subprocess,struct,hashlib,re
W=Path('build/codex-f2');images={};orphans=[];current=None
for line in (W/'seyrek.txt').read_text().splitlines():
 if line.startswith('IMAJ '):current=line.split()[1];images[current]=bytearray(70000*512)
 elif line.startswith('SAHIPSIZ '):orphans.append(int(line.split()[1]))
 else:
  n,h=line.split();raw=bytes.fromhex(h);n=int(n);assert len(raw)==512 and 0<=n<70000;images[current][n*512:(n+1)*512]=raw
for name,data in images.items():(W/(name+'.img')).write_bytes(data)
def run(mode,p):
 r=subprocess.run(['fsck.fat',mode,str(p)],capture_output=True,text=True);print(r.stdout);return r
clean=W/'temiz.img';assert run('-n',clean).returncode==0
p=W/'sahipsiz.img';before=p.read_bytes();assert run('-n',p).returncode==1;assert p.read_bytes()==before
# Yalnız bu aracın oluşturduğu kopya üzerinde fsck önerisini uygula.
copy=W/'fsck_kopya.img';copy.write_bytes(before);r=run('-a',copy);assert r.returncode in (0,1)
assert run('-n',copy).returncode==0
after=copy.read_bytes();base=struct.unpack_from('<H',before,14)[0]*512;first=32+2*struct.unpack_from('<I',before,36)[0];count=70000-first
# fsck -a sahipsiz zincirleri FSCK*.REC dosyalarına bağlar; küme tahsisleri korunur.
root=struct.unpack_from('<I',after,44)[0];root_off=(first+root-2)*512
recovered=[];recovery_files=0
for off in range(root_off,root_off+512,32):
 rec=after[off:off+32]
 if rec[0]==0:break
 if rec[:4]!=b'FSCK':continue
 recovery_files+=1;k=struct.unpack_from('<H',rec,26)[0]|(struct.unpack_from('<H',rec,20)[0]<<16);seen=set()
 while k<0xffffff8:
  assert 2<=k<count+2 and k not in seen;seen.add(k);recovered.append(k)
  k=struct.unpack_from('<I',after,base+4*k)[0]&0xfffffff
assert sorted(recovered)==orphans==[100,101,200],(recovered,orphans)
assert recovery_files==2
assert struct.unpack_from('<I',images['temiz'],1000)[0]==68889
print('F2_KAHIN OK: fsck ile aynı sahipsiz kümeler',orphans,'; onarılmış FSInfo temiz')
