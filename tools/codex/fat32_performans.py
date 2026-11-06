#!/usr/bin/env python3
"""Değiştirilmemiş FAT32 testlerinin self-host çalıştırma süresi kapısı.

Repo kökünde make build/codegen build/kdl_runtime.o build/kdl_runtime_mmio.o
sonrasında çalıştırılır. Derleme/link süreye katılmaz. Üç koşumun HER BİRİ
3 saniyeyi aşmamalıdır; çıktı ve exit kodları da denetlenir.
"""
from pathlib import Path
import json
import statistics
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'build' / 'fat32_performans'
TESTLER = ('fat32_coklu_test', 'fat32_yaz_test')


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    sonuc = []
    for ad in TESTLER:
        kaynak = f'tests/drivers/fs/{ad}.kem'
        ir = OUT / f'{ad}.ll'
        exe = OUT / ad
        with ir.open('wb') as out:
            subprocess.run(['build/codegen', '--llvm', kaynak], cwd=ROOT,
                           stdout=out, check=True)
        subprocess.run(['clang', '-Wno-override-module', '-x', 'ir', str(ir),
                        '-x', 'none', 'build/kdl_runtime.o',
                        'build/kdl_runtime_mmio.o', '-o', str(exe)],
                       cwd=ROOT, check=True)
        sureler = []
        onceki = None
        for i in range(3):
            out = OUT / f'{ad}.{i}.out'
            baslangic = time.monotonic()
            with out.open('wb') as f:
                subprocess.run([str(exe)], cwd=ROOT, stdout=f, check=True,
                               timeout=180)
            sureler.append(time.monotonic() - baslangic)
            veri = out.read_bytes()
            if onceki is not None and veri != onceki:
                raise RuntimeError(f'{ad}: tekrar çıktıları farklı')
            onceki = veri
        kayit = {'test': ad, 'saniye': sureler,
                 'medyan': statistics.median(sureler),
                 'gecti': max(sureler) <= 3.0}
        sonuc.append(kayit)
        print(json.dumps(kayit, ensure_ascii=False), flush=True)
    (OUT / 'sonuc.json').write_text(json.dumps(sonuc, indent=2), encoding='utf-8')
    return 0 if all(r['gecti'] for r in sonuc) else 1


if __name__ == '__main__':
    raise SystemExit(main())
