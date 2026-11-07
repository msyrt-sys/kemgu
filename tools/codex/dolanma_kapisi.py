#!/usr/bin/env python3
"""Dalga 1 ek API adlarının ve geçici probe dizininin geri gelmesini yakalar."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
ESKI_ADLAR = ('imaja_dosya_yaz', 'imajda_dizin_olustur', 'imajdan_dosya_sil',
             'imajdan_dosya_oku', 'imaj_dizin_listele',
             'satir_karakter', 'satir_tus')


def main():
    ihlaller = []
    for dizin in ('drivers', 'tests/drivers', 'tools/codex'):
        for p in sorted((ROOT / dizin).rglob('*')):
            if p.suffix not in ('.kem', '.py', '.md') or p.resolve() == Path(__file__).resolve():
                continue
            for satir, metin in enumerate(p.read_text(encoding='utf-8').splitlines(), 1):
                for ad in ESKI_ADLAR:
                    if re.search(r'\b' + ad + r'\b', metin):
                        ihlaller.append(f'{p.relative_to(ROOT)}:{satir}: {ad}')
    if (ROOT / 'tests/drivers/probe').exists():
        ihlaller.append('tests/drivers/probe: geçici örnek dizini duruyor')
    for ihlal in ihlaller:
        print(ihlal)
    print(f'DOLANMA_KAPISI: {len(ihlaller)} ihlal')
    return 1 if ihlaller else 0


if __name__ == '__main__':
    raise SystemExit(main())
