"""Geçici kâhin çıktılarını build altında tutar ve hata günlüğünü korur."""
import os
import shutil
import stat
import sys
import tempfile
from contextlib import contextmanager
from pathlib import Path


BUILD = Path(__file__).resolve().parents[2] / 'build'


@contextmanager
def calisma_dizini(prefix='qemu-', dir=None):
    kok = Path(dir).resolve() if dir is not None else BUILD
    if kok != BUILD and BUILD not in kok.parents:
        kok = BUILD
    kok.mkdir(parents=True, exist_ok=True)
    yol = tempfile.mkdtemp(prefix='kemgu-py.' + prefix, dir=kok)
    try:
        yield yol
    except BaseException:
        print(f'Tanı dizini: {yol}', file=sys.stderr)
        raise
    else:
        guvenli_sil(yol)


def _salt_okunuru_ac(islev, yol, _hata):
    # [D-784] Windows'ta git nesne dosyaları salt okunurdur; rmtree izin değiştirmeden silemez
    # (WinError 5, Windows CI'da calistir_kapi_secici'nin git örneğinde ölçüldü). Yazma iznini
    # aç ve aynı işlemi bir kez daha dene; yine olmazsa hata yükselir (sessiz atlama yok).
    # POSIX'te silme üst dizinin yazma iznine bağlıdır; ikisini de aç.
    os.chmod(os.path.dirname(yol) or '.', stat.S_IRWXU)
    os.chmod(yol, stat.S_IWRITE | stat.S_IREAD)
    islev(yol)


def guvenli_sil(yol):
    if sys.version_info >= (3, 12):
        shutil.rmtree(yol, onexc=_salt_okunuru_ac)
    else:
        shutil.rmtree(yol, onerror=_salt_okunuru_ac)
