"""Tek işin derlenen sabotajını, köprüsünü, commit ve ilerleme kaydını yürütür."""
import argparse, subprocess, re
from pathlib import Path
from datetime import datetime, timezone
p=argparse.ArgumentParser()
p.add_argument('is_no');p.add_argument('test');p.add_argument('modul')
p.add_argument('once');p.add_argument('sonra');p.add_argument('aciklama');p.add_argument('dosyalar',nargs='+')
a=p.parse_args()
def kos(*args):
    return subprocess.run(args,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
def zorunlu(*args):
    r=kos(*args);print(r.stdout,flush=True)
    if r.returncode: raise SystemExit(r.returncode)
zorunlu('bash','otomasyon/chatgpt/dogrula.sh',a.test)
pth=Path(a.modul); eski=pth.read_text(encoding='utf-8')
assert a.once in eski and a.once != a.sonra
try:
    pth.write_text(eski.replace(a.once,a.sonra,1),encoding='utf-8')
    r=kos('bash','otomasyon/chatgpt/dogrula.sh',a.test)
    print('SABOTAJ:',r.stdout,flush=True)
    assert r.returncode != 0 and ('HATA[C çalıştır]' in r.stdout or 'HATA[self çalıştır]' in r.stdout), 'Sabotaj derlenmeli ve koşumda kırmızı olmalı'
finally:
    pth.write_text(eski,encoding='utf-8')
zorunlu('make','calistir_chatgpt_kopru')
zorunlu('git','add','--',*a.dosyalar)
zorunlu('git','commit','-m',f'SURUCU(codex): {a.is_no} — {a.aciklama}')
sayi=len(re.findall(r'işlev test_[^(]+\([^)]*\) -> mantıksal', Path(a.test).read_text(encoding='utf-8')))
with Path('otomasyon/chatgpt/codex_ilerleme.md').open('a',encoding='utf-8') as f:
    f.write(f'- [x] {a.is_no} · {sayi} test · sabotaj: `{a.once}` → `{a.sonra}` → koşum kırmızı; geri alındı, köprü yeşil · {datetime.now(timezone.utc).isoformat()}\n')
