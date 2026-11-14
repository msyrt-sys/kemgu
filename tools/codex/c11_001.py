#!/usr/bin/env python3
"""C11-001 küçük tekrar üretimi; derleyiciyi değiştirmez, bulguyu ve dolanmayı ölçer."""
from pathlib import Path
import subprocess
value=int('0102030405060708',16)
source=f'''çıplak işlev main() -> tam32 {{
 değişken n: tam64 = {value};
 değişken p: *tam64 = (&n) olarak *tam64;
 *p=0;
 eğer n != 0 {{ ver 1; }}
 ver 0;
}}
'''
w=Path('build/c11_001_tekrar');w.mkdir(exist_ok=True)
for kind,kem in [('asil',source),('dolan',source.replace('*p=0;','*p=0 olarak tam64;'))]:
 p=w/(kind+'.kem');p.write_text(kem)
 for compiler in ('kemgu','codegen'):
  ll=w/f'{kind}-{compiler}.ll';binary=w/f'{kind}-{compiler}';binary.unlink(missing_ok=True)
  ll.write_bytes(subprocess.check_output(['build/'+compiler,'--llvm',str(p)]))
  subprocess.run(['clang','-Wno-override-module','-x','ir',str(ll),'-o',str(binary)],check=True)
  rc=subprocess.run([str(binary)]).returncode
  print(kind,compiler,'beklenen=0','gerçek='+str(rc),[l.strip() for l in ll.read_text().splitlines() if 'store volatile' in l])
  if kind=='dolan':assert rc==0,'açık tam64 dönüşümü de bozuk'
