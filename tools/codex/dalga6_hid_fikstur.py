#!/usr/bin/env python3
"""Gerçek usb_kbd qtest günlüğündeki C/self HID raporlarını karşılaştırıp dök."""
from pathlib import Path
import sys
rows=[[int(b) for b in line.split()[1:]] for line in Path(sys.argv[1]).read_text().splitlines() if line.startswith('HID ')]
assert rows and len(rows)%2==0
half=len(rows)//2
assert rows[:half]==rows[half:]
for row in rows[:half]:
    assert len(row)==8 and all(0<=b<256 for b in row)
    print('['+','.join(f'{b} olarak dtam8' for b in row)+'],')
print('Unicode:',[ord(c) for c in 'Ağaç İş\n'])
