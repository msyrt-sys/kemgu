#!/usr/bin/env python3
"""[D-748/D-749] Ekran kâhinlerinin ORTAK, KEMGU'dan BAĞIMSIZ parçaları: fikstür kaynağından
font baytları, PSF1/PSF2 ayrıştırma (ilk glif kazanır), glif geri düşmesi (glif → U+FFFD →
'?' → arka dolgu), drivers/ekran/konsol.kem semantiğinin Python modeli ve QMP screendump."""
import re, json, struct
from pathlib import Path

def font_baytlari():
    s = Path('tests/drivers/ekran/veri/psf_gercek.kem').read_text(encoding='utf-8')
    n = int(re.search(r'bayt_sifir\((\d+)\)', s).group(1))
    b = bytearray(n)
    for i, v in re.findall(r'yaz_u8\(b, (\d+), (\d+)\);', s):
        b[int(i)] = int(v)
    return bytes(b)

def psf(d):
    """(genişlik, yükseklik, glif_bayt, veri_ofseti, glif_sayisi, eşleme) — ilk glif kazanır."""
    m = {}
    if d[:2] == b'\x36\x04':
        mode, h = d[2], d[3]; n = 512 if mode & 1 else 256; w, size, off = 8, h, 4
        u = off + n * size
        if mode & 6:
            for g in range(n):
                seq = False
                while True:
                    cp = struct.unpack_from('<H', d, u)[0]; u += 2
                    if cp == 0xFFFF: break
                    if cp == 0xFFFE: seq = True
                    elif not seq: m.setdefault(cp, g)
        else:
            m = {i: i for i in range(n)}
    else:
        magic, ver, hs, flags, n, size, h, w = struct.unpack_from('<8I', d); assert magic == 0x864ab572
        off = hs; u = off + n * size
        if flags & 1:
            for g in range(n):
                e = d.index(b'\xff', u); part = d[u:e].split(b'\xfe')[0]
                for ch in part.decode('utf-8'): m.setdefault(ord(ch), g)
                u = e + 1
        else:
            m = {i: i for i in range(n)}
    return w, h, size, off, n, m

def glif_ciz(img, gen, yuk, font, x0, y0, kod, on, arka):
    d, (w, h, size, off, n, m) = font
    g = m.get(kod, m.get(0xFFFD, m.get(ord('?'), -1))); sb = (w + 7) // 8
    for r in range(h):
        for c in range(w):
            x, y = x0 + c, y0 + r
            if not (0 <= x < gen and 0 <= y < yuk): continue
            p = arka
            if g >= 0 and d[off + g * size + r * sb + c // 8] & (0x80 >> (c % 8)): p = on
            img[y * gen + x] = p

def font_yukle():
    d = font_baytlari(); return d, psf(d)

def konsol(metin, sutun, satir):
    """drivers/ekran/konsol.kem konsol_kod modeli: 10 alt satır, 13 satır başı, 9 sekme (8'e),
    8 geri sil, diğer <32 ve 127 yok sayılır, sütun sonunda kaydırarak satır atla, son satırdan
    sonra yukarı kaydır. Hücre ızgarası (kod listesi) döner."""
    izg = [32] * (sutun * satir); x = y = 0
    def atla():
        nonlocal x, y, izg
        x = 0; y += 1
        if y >= satir: izg = izg[sutun:] + [32] * sutun; y = satir - 1
    def yaz(k):
        nonlocal x
        izg[y * sutun + x] = k; x += 1
        if x >= sutun: atla()
    for ch in metin:
        k = ord(ch)
        if k == 10: atla()
        elif k == 13: x = 0
        elif k == 9:
            for _ in range(8 - x % 8): yaz(32)
        elif k == 8:
            if x > 0: x -= 1
            elif y > 0: y -= 1; x = sutun - 1
            else: continue
            izg[y * sutun + x] = 32
        elif k < 32 or k == 127: pass
        else: yaz(k)
    return izg

def screendump(f, yol, yanit):
    f.write(json.dumps({'execute': 'screendump', 'arguments': {'filename': str(yol)}}).encode() + b'\n'); yanit()
    b = Path(yol).read_bytes()
    t = re.match(rb'P6\s+(\d+)\s+(\d+)\s+(\d+)\s', b)
    assert t, 'PPM başlığı yok'
    return int(t.group(1)), int(t.group(2)), b[t.end():]

def karsilastir(px, beklenen, gen):
    fark = 0; ilk = None
    for i, e in enumerate(beklenen):
        g = (px[3 * i] << 16) | (px[3 * i + 1] << 8) | px[3 * i + 2]
        if g != e:
            fark += 1
            if ilk is None: ilk = (i % gen, i // gen, hex(g), hex(e))
    return fark, ilk
