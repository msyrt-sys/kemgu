"""Geçici kâhin çıktılarını build altında tutar ve hata günlüğünü korur."""
import shutil
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
        shutil.rmtree(yol)
