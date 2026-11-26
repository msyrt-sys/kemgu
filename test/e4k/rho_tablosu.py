#!/usr/bin/env python3
"""[D-774] IR'dan saf_*/kirli_* çağrılarının ρ argümanını çıkarır: işlevin kendi
kdl_bolge_olustur() yazmacı (ρ_yerel), döngü tur bölgesi (D-775) ya da başka bir şey
(ρ_caller) mı."""
import re, sys
t = open(sys.argv[1], encoding='utf-8').read()
satir = []
for fm in re.finditer(r'define [^@]*@"?([\w.$]+)"?\((.*?)\n\}', t, re.S):
    ad, govde = fm.group(1), fm.group(0)
    olustur = re.findall(r'(%\d+) = call ptr @kdl_bolge_olustur\(\)', govde)
    yerel = olustur[0] if olustur else None          # işlev girişi: ρ_yerel
    tur = set(olustur[1:])                            # [D-775] döngü gövdesi girişi: tur bölgesi
    for c in re.finditer(r'call [^@]*@"?(saf_\w+|kirli_\w+)"?\(ptr (%\w+)', govde):
        r = c.group(2)
        satir.append(f'{ad} {c.group(1)} {"yerel" if r == yerel else "tur" if r in tur else "caller"}')
print('\n'.join(sorted(satir)))
