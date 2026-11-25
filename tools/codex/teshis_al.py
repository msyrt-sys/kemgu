"""Bir KEMGU teşhis raporunu kullanıcının seçtiği yerel dosyaya kaydeder."""
import argparse
import hashlib
import os
from pathlib import Path
import re
import socket
import tempfile
from time import monotonic

SINIR = 131072


def kaydet(baglanti, yol, sure=15):
    son = monotonic() + sure

    def al(n):
        kalan = son - monotonic()
        if kalan <= 0:
            raise TimeoutError("teşhis süresi doldu")
        baglanti.settimeout(kalan)
        return baglanti.recv(n)

    baslik = bytearray()
    while len(baslik) < 32 and not baslik.endswith(b'\n'):
        b = al(1)
        if not b:
            raise ValueError("eksik teşhis başlığı")
        baslik.extend(b)
    es = re.fullmatch(rb'KEMGU-TESHIS/1 ([1-9][0-9]{0,5})\n', baslik)
    if not es or int(es[1]) > SINIR:
        raise ValueError("geçersiz teşhis başlığı")
    boy = kalan = int(es[1])
    karma = hashlib.sha256()
    with tempfile.NamedTemporaryFile(dir=yol.parent, delete=False) as f:
        gecici = Path(f.name)
        try:
            while kalan:
                b = al(min(65536, kalan))
                if not b:
                    raise ValueError("rapor kısa")
                f.write(b)
                karma.update(b)
                kalan -= len(b)
            if al(1):
                raise ValueError("bildirilen uzunluk aşıldı")
            f.flush()
            os.fsync(f.fileno())
            f.close()
            os.replace(gecici, yol)
        finally:
            gecici.unlink(missing_ok=True)
    baglanti.sendall(b'\x06')
    return boy, karma.hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('dosya', type=Path)
    p.add_argument('--adres', default='127.0.0.1')
    p.add_argument('--port', type=int, default=9140)
    a = p.parse_args()
    with socket.socket() as s:
        s.bind((a.adres, a.port))
        s.listen(1)
        s.settimeout(120)
        print('DINLIYOR', s.getsockname()[1], flush=True)
        conn, _ = s.accept()
        with conn:
            n, sha = kaydet(conn, a.dosya)
        print('KAYDEDILDI', n, sha, flush=True)


if __name__ == '__main__':
    main()
