#!/usr/bin/env python3
"""Raporun kullanıcıya verdiği metni ve süreç çıkış kodunu sabit kayıtlarla sınar."""
import os
import subprocess
import sys
from pathlib import Path


DIZIN = Path(__file__).resolve().parent
ORNEK = DIZIN / "olcum"
sys.path.insert(0, str(DIZIN.parent / "tests" / "qemu"))
from calisma_dizini import calisma_dizini


def rapor(kayit, butce, calisma, siki, beklenen):
    komut = [sys.executable, str(DIZIN / "zaman_raporu.py"), str(kayit), str(calisma),
             "0", "--butce", str(butce)]
    sonuc = subprocess.run(komut, env={**os.environ, "ZAMAN_BUTCE_SIKI": str(int(siki))},
                           capture_output=True, encoding="utf-8")
    if sonuc.returncode != beklenen:
        raise RuntimeError(f"rapor rc={sonuc.returncode}, beklenen {beklenen}\n{sonuc.stdout}{sonuc.stderr}")
    return sonuc.stdout


def denetle(calisma):
    butce = ORNEK / "kapi_butce.tsv"
    for siki in (False, True):
        metin = rapor(ORNEK / "zaman.tsv", butce, calisma, siki, 3 if siki else 0)
        beklenen = ("4 kapı", "calistir_ornek_yavas  ⚠ BÜTÇE (62.000 sn > 60 sn)",
                    "calistir_ornek_yeni  bütçe yok", "başarısız: calistir_ornek_hata")
        for ifade in beklenen:
            if ifade not in metin:
                raise RuntimeError(f"raporda eksik: {ifade}\n{metin}")
        if "calistir_ornek_normal" in metin:
            raise RuntimeError("tam bütçe sınırındaki kapı aşım olarak listelendi")
    kayit = calisma / "zaman.tsv"
    kayit.write_text("", encoding="utf-8")
    if "kayıt yok" not in rapor(kayit, butce, calisma, True, 0):
        raise RuntimeError("boş ölçüm uyarısı eksik")
    kayit.write_text("calistir_ornek_yeni\t200\t200\t0\n", encoding="utf-8")
    if "bütçe yok" not in rapor(kayit, butce, calisma, True, 0):
        raise RuntimeError("sıfır süreli yeni kapı raporda eksik")


if __name__ == "__main__":
    with calisma_dizini(prefix="butce-") as dizin:
        denetle(Path(dizin))
    print("KAPI_BUTCE OK: toplama, sınır ve aşım, eksik bütçe, başarısız kapı, boş kayıt, sıfır süre")
