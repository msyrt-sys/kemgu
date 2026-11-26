#!/usr/bin/env python3
"""[D-772] Ölçümlü koşum raporu: kapı başına süre (tarif satırlarının toplamı), en yavaş kapılar,
toplam duvar saati, başarısız kapılar ve build/ disk kullanımı. Kullanım:
  python3 test/zaman_raporu.py <zaman.tsv> [build_dizini] [en_cok]"""
import collections, pathlib, shutil, subprocess, sys

yol = pathlib.Path(sys.argv[1])
build = sys.argv[2] if len(sys.argv) > 2 else 'build'
n = int(sys.argv[3]) if len(sys.argv) > 3 else 15
sure = collections.defaultdict(float); bas = {}; son = {}; hata = set()
for satir in yol.read_text(encoding='utf-8').splitlines():
    p = satir.split('\t')
    if len(p) != 4: continue
    h, s, e, rc = p[0], float(p[1]), float(p[2]), int(p[3])
    sure[h] += e - s; bas[h] = min(bas.get(h, s), s); son[h] = max(son.get(h, e), e)
    if rc != 0: hata.add(h)
if not sure:
    print('ZAMAN RAPORU: kayıt yok'); sys.exit(0)
duvar = max(son.values()) - min(bas.values())
kapilar = {h: v for h, v in sure.items() if h.startswith('calistir_')}
def bicim(x):
    d, s = divmod(int(round(x)), 60); sa, d = divmod(d, 60)
    return f'{sa} sa {d:02d} dk {s:02d} sn' if sa else f'{d} dk {s:02d} sn'
print(f'=== ZAMAN RAPORU: duvar saati {bicim(duvar)} · {len(kapilar)} kapı · '
      f'kapı toplamı {bicim(sum(kapilar.values()))} · derleme/diğer {bicim(sum(sure.values()) - sum(kapilar.values()))} ===')
for h, v in sorted(kapilar.items(), key=lambda a: -a[1])[:n]:
    print(f'  {bicim(v):>16}  %{100 * v / duvar:4.1f}  {h}{"  🔴" if h in hata else ""}')
if hata:
    print('  başarısız: ' + ' '.join(sorted(hata)))
try:
    du = subprocess.run(['du', '-sh', build], capture_output=True, text=True).stdout.split()[0]
except Exception:
    du = '?'
bos = shutil.disk_usage(build).free / 2**30
print(f'  disk: {build} {du} · boş {bos:.0f} GiB')
