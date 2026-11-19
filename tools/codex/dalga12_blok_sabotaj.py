"""Kirliliği kaybeden LRU, üç aygıtlı QEMU yazma kapısında kırmızı olmalı."""
from pathlib import Path
import subprocess
import os

p = Path('test/ornekler/kem_os_x86_blok_ortak.kem')
original = p.read_bytes()
line = ' eşleş bd_ham_yaz(d,k.lba,k.veri) { hata(e) => { ver hata(e); } tamam(v) => { } }\n'
# Bozma yalnız taşma kolunda. Flush içindeki toplu yazma yerinde kalır.
start = original.index(b' // LRU')
assert original[start:].count(line.encode()) == 1
try:
    p.write_bytes(original[:start] + original[start:].replace(line.encode(), b'', 1))
    env = dict(os.environ, BLOK_DERLEYICILER='build/kemgu', BLOK_TEKRAR='1')
    proc = subprocess.run(['bash', 'test/bm_x86_blok_harness.sh'], env=env,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    print(proc.stdout)
    assert proc.returncode != 0 and 'BLOK_HATA geri okuma' in proc.stdout
finally:
    p.write_bytes(original)
