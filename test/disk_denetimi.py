#!/usr/bin/env python3
"""Tam takımın başında ve sonunda build dizinini ölçer, kalan çalışma dizinlerini bildirir."""
import argparse
import json
import os
import re
import stat
from pathlib import Path


def olc(kok):
    gecici = []
    boy = 0
    gorulen = set()
    def okunamadi(hata):
        raise hata

    for dizin, altlar, dosyalar in os.walk(kok, onerror=okunamadi):
        for ad in altlar + dosyalar:
            yol = Path(dizin) / ad
            bilgi = yol.lstat()
            kimlik = (bilgi.st_dev, bilgi.st_ino)
            if not bilgi.st_ino or kimlik not in gorulen:
                boy += bilgi.st_blocks * 512 if hasattr(bilgi, 'st_blocks') else bilgi.st_size
            if bilgi.st_ino:
                gorulen.add(kimlik)
            if Path(dizin) == kok and stat.S_ISDIR(bilgi.st_mode):
                if (re.search(r'\.[A-Za-z0-9]{6}$|\.\d+$|^_.+_\d+$', ad) or ad.startswith('kemgu-py.')):
                    gecici.append(ad)
    return {'bayt': boy, 'dizinler': sorted(gecici)}


def denetle(kok, once, son):
    kalan = son['dizinler']
    yeni = set(kalan) - set(once['dizinler'])
    print(f"Disk {kok}: önce {once['bayt']} B, sonra {son['bayt']} B, büyüme {son['bayt'] - once['bayt']:+d} B")
    print(f"Geçici dizin: önce {len(once['dizinler'])}, sonra {len(kalan)}, yeni {len(yeni)}")
    for ad in kalan:
        print(f"  {'yeni' if ad in yeni else 'önceden vardı'}: {kok / ad}")
    return 1 if kalan else 0


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('islem', choices=('basla', 'denetle'))
    p.add_argument('kok', type=Path)
    p.add_argument('kayit', type=Path)
    a = p.parse_args()
    if not a.kok.is_dir():
        p.error(f'ölçüm dizini bulunamadı: {a.kok}')
    try:
        if a.islem == 'basla':
            sonuc = olc(a.kok)
            a.kayit.write_text(json.dumps(sonuc, ensure_ascii=False)+'\n', encoding='utf-8')
            print(f"Disk başlangıcı: {sonuc['bayt']} B, {len(sonuc['dizinler'])} geçici dizin")
        else:
            once = json.loads(a.kayit.read_text(encoding='utf-8'))
            raise SystemExit(denetle(a.kok, once, olc(a.kok)))
    except (OSError, ValueError, KeyError) as hata:
        p.exit(2, f'Disk ölçümü tamamlanamadı: {hata}\n')
