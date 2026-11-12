#!/usr/bin/env python3
"""[D-748] Bare-metal ekran kâhini: QMP screendump ile KEMGU'dan BAĞIMSIZ Python çizimini
piksel piksel karşılaştırır. Yazı tipi baytları fikstür KAYNAĞINDAN okunur (KEMGU derlenmez),
PSF ayrıştırma ve glif geri düşme kuralı (glif → U+FFFD → '?' → arka dolgu) burada yeniden
yazılmıştır."""
import re, sys, os, json, time, socket, struct, subprocess
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from psf_kahin import font_baytlari, psf, glif_ciz, font_yukle, screendump, karsilastir

GEN, YUK = 640, 480
ARKA, ON, SARI = 0x102040, 0xFFFFFF, 0xFFD800
SATIRLAR = [("KEMGU-OS ekran v0", SARI), ("Merhaba Dünya — ğüşıöç İ", ON), ("ĞÜŞİÖÇ ğüşıöç 0123456789", ON), ("─│┌┐└┘├┤┬┴┼═║", ON)]

def beklenen():
    font = font_yukle(); w, h = font[1][0], font[1][1]
    img = [ARKA] * (GEN * YUK)
    for k, (t, renk) in enumerate(SATIRLAR):
        for i, ch in enumerate(t): glif_ciz(img, GEN, YUK, font, 8 + i * w, 8 + k * (h + 4), ord(ch), renk, ARKA)
    return img, w, h

def acilis(elf, w, n, exp):
    ser = w / 'seri.txt'; q = w / 'qmp.sock'; dump = w / 'ekran.ppm'
    for f in (ser, q, dump): f.unlink(missing_ok=True)
    cmd = ['qemu-system-x86_64', '-M', 'q35', '-m', '256M', '-vga', 'std', '-display', 'none', '-no-reboot',
           '-serial', f'file:{ser}', '-qmp', f'unix:{q},server=on,wait=off', '-kernel', str(elf),
           '-device', 'isa-debug-exit,iobase=0xf4,iosize=0x04']
    bas = time.monotonic()
    p = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    try:
        son = time.monotonic() + 60
        while b'EKRAN_HAZIR' not in (ser.read_bytes() if ser.exists() else b''):
            if p.poll() is not None or time.monotonic() > son:
                raise RuntimeError(f'açılış {n}: EKRAN_HAZIR yok rc={p.poll()} seri={(ser.read_bytes() if ser.exists() else b"")!r}')
            time.sleep(0.05)
        print(f'  boot-ready {time.monotonic()-bas:.4f} s', flush=True)
        s = socket.socket(socket.AF_UNIX); s.settimeout(10); s.connect(str(q)); f = s.makefile('rwb', buffering=0)
        def yanit():
            while True:
                d = json.loads(f.readline())
                if 'error' in d: raise RuntimeError(d['error'])
                if 'return' in d: return d['return']
        f.readline(); f.write(b'{"execute":"qmp_capabilities"}\n'); yanit()
        gw, gh, px = screendump(f, dump, yanit)
        if (gw, gh) != (GEN, YUK): raise AssertionError(f'açılış {n}: mod {gw}x{gh}, beklenen {GEN}x{YUK}')
        fark, ilk = karsilastir(px, exp, GEN)
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
