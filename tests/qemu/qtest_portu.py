"""QEMU'nun port 0 ile açtığı qtest dinleyicisini bağlantı tüketmeden bulur."""

import os
import subprocess
import sys

from bekle import bekle


def qtest_portu(pid, sure=10, surec=None):
    def dinleyiciler():
        if os.name == "nt":
            tablo = subprocess.check_output(
                ["netstat.exe", "-ano", "-p", "TCP"], timeout=min(3, sure)
            ).decode(errors="replace")
            # Yerelleştirilmiş LISTENING metnine bağlı kalmadan uzak uç ve PID ile ayır.
            return [
                t[1]
                for s in tablo.splitlines()
                if len(t := s.split()) == 5 and t[0] == "TCP" and t[2] == "0.0.0.0:0" and t[-1] == str(pid)
            ]
        tablo = subprocess.check_output(["ss", "-Hltnp"], text=True, timeout=min(3, sure))
        return [s.split()[3] for s in tablo.splitlines() if f"pid={pid}," in s]

    adresler = bekle(f"QEMU pid={pid} qtest TCP dinleyicisi", dinleyiciler, sure=sure, surec=surec)
    if len(adresler) != 1 or not adresler[0].startswith("127.0.0.1:"):
        raise RuntimeError(f"QEMU pid={pid} için beklenmeyen TCP dinleyicileri {adresler}")
    return int(adresler[0].rsplit(":", 1)[1])


if __name__ == "__main__":
    print(qtest_portu(int(sys.argv[1])))
