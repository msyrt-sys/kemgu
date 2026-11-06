"""Dokümanın bütün modülleri ve dışa açık adları taşıdığını denetler."""
from pathlib import Path
import re
s=Path('drivers/README.md').read_text();n=0
for p in sorted(Path('drivers').rglob('*.kem')):
 key=str(p.relative_to('drivers').with_suffix(''))
 rows=[r for r in s.splitlines() if r.startswith(f'| [{key}]({key}.kem) |')]
 assert len(rows)==1, f'Modül satırı eksik/tekrar: {key}'
 for name in re.findall(r'^genel (?:işlev|yapı|çeşit|sabit)\s+(\w+)',p.read_text(),re.M):
  assert '`'+name+'`' in rows[0], f'API eksik: {key}::{name}'
 assert len(rows[0].split('|'))==8, f'Tablo sütunu eksik: {key}'
 n+=1
print('ENVANTER OK',n,'modül')
