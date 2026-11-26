"""QEMU ve host koşulları için sınırlı bekleme; hata anındaki son gözlemi saklar."""
import subprocess
import time


def bekle(ad, oku, uygun=bool, *, sure=15, aralik=.01, surec=None):
    son = time.monotonic() + sure
    while True:
        durum = oku()
        if uygun(durum):
            return durum
        kalan = son - time.monotonic()
        rc = surec.poll() if surec is not None else None
        if rc is not None or kalan <= 0:
            ozet = repr(durum)
            if len(ozet) > 1200:
                ozet = ozet[:160] + ' … ' + ozet[-1024:]
            raise AssertionError(f'{ad}: {sure:g} sn sınırı, süreç rc={rc}, son durum={ozet}')
        time.sleep(min(aralik, kalan))


def gozle(ad, sure, denetle, *, surec=None, aralik=.1):
    """Boşta bekleme veya hız deneyi boyunca her örneği denetle."""
    bas = time.monotonic()
    def ornek():
        denetle()
        return time.monotonic() - bas
    return bekle(ad, ornek, lambda gecen: gecen >= sure, sure=sure+2, aralik=aralik, surec=surec)


def seri_oku(yol):
    try:
        return yol.read_bytes()
    except FileNotFoundError:
        return b''


def hlt_bekle(komut, surec, sure=5):
    return bekle('vCPU HLT', lambda: komut('human-monitor-command', {'command-line':'info registers'}),
                 lambda r: 'HLT=1' in r, sure=sure, surec=surec)


def soket_bekle(yol, surec, sure=10):
    return bekle(f'QMP soketi {yol}', yol.exists, sure=sure, surec=surec)


def tik_bekle(oku, adet=2, *, surec=None):
    ilk = oku()
    return bekle(f'konuk tik sayacı {ilk}+{adet}', oku, lambda n: n-ilk >= adet,
                 sure=5, aralik=.002, surec=surec)


def tam_seri(ad, oku, beklenen, surec, sure):
    def tamam(veri):
        if not beklenen.startswith(veri):
            raise AssertionError(f'{ad}: seri uyuşmadı, son baytlar={veri[-240:]!r}')
        return veri == beklenen
    return bekle(ad, oku, tamam, sure=sure, surec=surec)


def konuk_sayaci(dizin, komut, sembol, *, elf=None):
    """ELF sembolünü bir kez çöz; QMP üzerinden geçerli sayaç değerini oku."""
    tablo = subprocess.check_output(['nm','-n',str(elf or dizin/'k.elf')],text=True)
    adres, = [int(t.split()[0],16) for t in tablo.splitlines()
              if len(t.split()) == 3 and t.split()[2] == sembol]
    hedef = (dizin/f'{sembol}-sayac.bin').resolve()
    def oku():
        komut('pmemsave', {'val':adres,'size':8,'filename':str(hedef)})
        return int.from_bytes(hedef.read_bytes(),'little')
    return oku
