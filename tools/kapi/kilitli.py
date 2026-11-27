"""Sabit ürünü kullanan alt süreci POSIX veya Windows dosya kilidi altında çalıştır."""

import errno
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path


def calistir(yol, komut):
    yol.parent.mkdir(parents=True, exist_ok=True)
    with yol.open("a+b") as dosya:
        if os.name == "nt":
            import msvcrt

            # EOF ötesindeki tek bayt da Windows tarafından kilitlenebilir.
            while True:
                try:
                    dosya.seek(0)
                    msvcrt.locking(dosya.fileno(), msvcrt.LK_NBLCK, 1)
                    break
                except OSError as hata:
                    if hata.errno not in (errno.EACCES, errno.EAGAIN, errno.EDEADLK):
                        raise
                    time.sleep(0.05)
        else:
            import fcntl

            fcntl.flock(dosya, fcntl.LOCK_EX)
        komut[0] = shutil.which(komut[0]) or komut[0]
        # GNU make iş sunucusunun devrettiği borular alt make'e ulaşmalı.
        return subprocess.run(komut, close_fds=False, check=False).returncode


if __name__ == "__main__":
    raise SystemExit(calistir(Path(sys.argv[1]), sys.argv[2:]))
