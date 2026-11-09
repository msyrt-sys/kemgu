#!/usr/bin/env python3
"""[D-748] Bare-metal ekran kâhini: QMP screendump ile KEMGU'dan BAĞIMSIZ Python çizimini
piksel piksel karşılaştırır. Yazı tipi baytları fikstür KAYNAĞINDAN okunur (KEMGU derlenmez),
PSF ayrıştırma ve glif geri düşme kuralı (glif → U+FFFD → '?' → arka dolgu) burada yeniden
yazılmıştır."""
import re, sys, os, json, time, socket, struct, subprocess
from pathlib import Path

GEN, YUK = 640, 480
ARKA, ON, SARI = 0x102040, 0xFFFFFF, 0xFFD800
SATIRLAR = [("KEMGU-OS ekran v0", SARI), ("Merhaba Dünya — ğüşıöç İ", ON), ("ĞÜŞİÖÇ ğüşıöç 0123456789", ON)]

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

def beklenen():
    d = font_baytlari(); w, h, size, off, n, m = psf(d)
    img = [ARKA] * (GEN * YUK); sb = (w + 7) // 8
    for k, (s, renk) in enumerate(SATIRLAR):
        y0 = 8 + k * (h + 4)
        for i, ch in enumerate(s):
            g = m.get(ord(ch), m.get(0xFFFD, m.get(ord('?'), -1)))
            x0 = 8 + i * w
            for r in range(h):
                for c in range(w):
                    x, y = x0 + c, y0 + r
                    if not (0 <= x < GEN and 0 <= y < YUK): continue
                    p = ARKA
                    if g >= 0 and d[off + g * size + r * sb + c // 8] & (0x80 >> (c % 8)): p = renk
                    img[y * GEN + x] = p
    return img, w, h

def ppm(p):
    b = p.read_bytes()
    t = re.match(rb'P6\s+(\d+)\s+(\d+)\s+(\d+)\s', b)
    assert t, 'PPM başlığı yok'
    return int(t.group(1)), int(t.group(2)), b[t.end():]

def acilis(elf, w, n, exp):
    ser = w / 'seri.txt'; q = w / 'qmp.sock'; dump = w / 'ekran.ppm'
    for f in (ser, q, dump): f.unlink(missing_ok=True)
    cmd = ['qemu-system-x86_64', '-M', 'q35', '-m', '256M', '-vga', 'std', '-display', 'none', '-no-reboot',
           '-serial', f'file:{ser}', '-qmp', f'unix:{q},server=on,wait=off', '-kernel', str(elf),
           '-device', 'isa-debug-exit,iobase=0xf4,iosize=0x04']
    p = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    try:
        son = time.monotonic() + 60
        while b'EKRAN_HAZIR' not in (ser.read_bytes() if ser.exists() else b''):
            if p.poll() is not None or time.monotonic() > son:
                raise RuntimeError(f'açılış {n}: EKRAN_HAZIR yok rc={p.poll()} seri={(ser.read_bytes() if ser.exists() else b"")!r}')
            time.sleep(0.05)
        s = socket.socket(socket.AF_UNIX); s.settimeout(10); s.connect(str(q)); f = s.makefile('rwb', buffering=0)
        def yanit():
            while True:
                d = json.loads(f.readline())
                if 'error' in d: raise RuntimeError(d['error'])
                if 'return' in d: return d['return']
        f.readline(); f.write(b'{"execute":"qmp_capabilities"}\n'); yanit()
        f.write(json.dumps({'execute': 'screendump', 'arguments': {'filename': str(dump)}}).encode() + b'\n'); yanit()
        gw, gh, px = ppm(dump)
        if (gw, gh) != (GEN, YUK): raise AssertionError(f'açılış {n}: mod {gw}x{gh}, beklenen {GEN}x{YUK}')
        fark = 0; ilk = None
        for i, e in enumerate(exp):
            g = (px[3 * i] << 16) | (px[3 * i + 1] << 8) | px[3 * i + 2]
            if g != e:
                fark += 1
                if ilk is None: ilk = (i % GEN, i // GEN, hex(g), hex(e))
        kayit = os.environ.get('EKRAN_KAYIT')
        if kayit and n == 1:
            import zlib
            ham = b''.join(b'\x00' + px[y * GEN * 3:(y + 1) * GEN * 3] for y in range(YUK))
            def parca(t, v): return struct.pack('>I', len(v)) + t + v + struct.pack('>I', zlib.crc32(t + v))
            Path(kayit).write_bytes(b'\x89PNG\r\n\x1a\n' + parca(b'IHDR', struct.pack('>IIBBBBB', GEN, YUK, 8, 2, 0, 0, 0)) + parca(b'IDAT', zlib.compress(ham)) + parca(b'IEND', b''))
        if fark: raise AssertionError(f'açılış {n}: {fark} piksel FARKLI, ilk (x,y,gelen,beklenen)={ilk}')
        satir = [l for l in ser.read_bytes().decode('utf-8', 'replace').splitlines() if l.startswith('DISPI')]
        print(f'  ✓ ekran açılış {n}: 640x480 {GEN*YUK} piksel BİREBİR · {satir[0] if satir else ""}', flush=True)
        s.close()
    finally:
        if p.poll() is None: p.kill(); p.wait()

def main():
    elf = Path(sys.argv[1]).resolve(); w = Path(sys.argv[2]).resolve(); tekrar = int(sys.argv[3])
    exp, fw, fh = beklenen()
    d = font_baytlari(); m = psf(d)[5]
    eksik = [c for c in 'ğüşıöçĞÜŞİÖÇ' if ord(c) not in m]
    assert not eksik, f'yazı tipinde Türkçe glif yok: {eksik} (kâhin geri düşmeyi ölçerdi, harfi değil)'
    on_px = sum(1 for v in exp if v != ARKA)
    assert on_px > 500, f'kâhin metin çizmedi ({on_px} piksel)'
    for n in range(1, tekrar + 1): acilis(elf, w, n, exp)
    print(f'=== bm_x86_ekran: {tekrar}/{tekrar} açılış, font {fw}x{fh}, {on_px} ön-plan pikseli BİREBİR ===')

main()
