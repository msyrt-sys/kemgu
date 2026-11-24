"""Eksik aygıt ve USB çıkarma koşullarında OVMF kabuğundan yanıt bekler."""
import argparse
from pathlib import Path

from dalga12_kahin import boot, sha, wait_for
from surec_imaj import imajlar
from surec_kahin import SurecConsole

DURUMLAR = {
    "disksiz": (dict(yalniz_firmware_diski=True), "KABUK HATA disk bulunamadi"),
    "xhci-yok": (dict(xhci=False, acilis="nvme"), "KABUK HATA xhci denetleyici yok"),
    "sata-bos": (dict(sata_takili=False), "KABUK HATA ahci PxSSTS zaman asimi"),
    "nvme-bos": (dict(nvme_ns=False), "KABUK HATA nvme"),
    "usb-cikar-klavye": ({"klavye": "usb"}, "KABUK HATA usb bellek cikarildi"),
    "usb-cikar": ({}, "KABUK HATA usb bellek cikarildi"),
}


def kos(dizin, durum):
    secim, hata = DURUMLAR[durum]
    usb, nvme, sata = imajlar(dizin, {})
    ilk = {p.disk: sha(p.disk) for p in (usb[0], nvme, sata)}
    with boot(dizin, usb[0], nvme, sata, **({"klavye": "ps2"} | secim)) as (q, p, ser):
        disk = {"xhci-yok":"NVMe", "disksiz":None}.get(durum, "USB")
        konsol = SurecConsole(dizin, q, p, ser, disk=disk)
        if durum.startswith("usb-cikar"):
            q.call("device_del", {"id": "bellek"})
            konsol.run("ls /diskler/usb0p2/notlar", (hata + "\n").encode())
        else:
            satirlar = ser.read_text(encoding="utf-8", errors="replace").splitlines()
            assert any(s == hata or s.startswith(hata + " ") for s in satirlar), satirlar
        if durum == "disksiz":
            q.call('device_del', {'id':'devir'})
            def bos():
                return not any(d.get('device') == 'ddevir' for d in q.call('query-block'))
            wait_for(p,bos)
            konsol.run('ls /ram')
            konsol.run('yaz /ram/deneme disksiz')
            konsol.run('cat /ram/deneme',b'disksiz\n')
        konsol.run("echo yasiyor", b"yasiyor\n")
        konsol.shutdown()
    assert all(sha(yol) == ozet for yol, ozet in ilk.items())
    print(f"✓ {durum} tek açılış, hata satırı, kabuk komutu, kapanış ve disk özetleri", flush=True)


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("dizin", type=Path)
    ap.add_argument("--durum", action="append", choices=tuple(DURUMLAR))
    args = ap.parse_args()
    for durum in args.durum or DURUMLAR:
        kos(args.dizin.resolve(), durum)
