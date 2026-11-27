"""PVH girdisini konuk COM1 alıcı FIFO'sunu açtıktan sonra gönderir."""

import subprocess
import sys
import threading
from pathlib import Path


def kos(cikti, komut):
    with cikti.open("wb") as dosya, subprocess.Popen(
        komut, stdin=subprocess.PIPE, stdout=subprocess.PIPE
    ) as qemu:
        # Konuk başarı işaretinden sonra hlt ile bekler. Sonsuz okuma için üst sınır.
        sinir = threading.Timer(20, qemu.kill)
        sinir.start()
        try:
            for satir in qemu.stdout:
                dosya.write(satir)
                if satir.rstrip() == b"X86 DMA RAM BLK OK":
                    qemu.stdin.write(b"KEMGU-RX\n")
                    qemu.stdin.flush()
                if satir.rstrip() == b"X86 PANIK YOLU OK":
                    return
            raise RuntimeError(f"PVH UART son işarete ulaşmadı, QEMU rc={qemu.poll()}")
        finally:
            sinir.cancel()
            qemu.kill()
            qemu.wait()
            sinir.join()


if __name__ == "__main__":
    if sys.argv[2:3] != ["--"]:
        raise SystemExit("kullanım: pvh_uart_girdi.py ÇIKTI -- QEMU [SEÇENEKLER]")
    kos(Path(sys.argv[1]), sys.argv[3:])
