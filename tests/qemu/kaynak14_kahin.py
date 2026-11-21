"""UEFI ayırıcısını, çekirdek heap yüksek su izini ve program sürelerini ölçer."""

import csv
import os
import re
import time

from dalga12_fat import Fat
from dalga12_kahin import fsck, wait_for
from duzenle_kahin import du_ekran, tamam, tus, yaz

SUREC_CERCEVESI = 3594
EDITOR_HEAP_ESIGI = 24 * 1024**2


def kaydet(c, senaryo, acilis, satirlar):
    dosya = c.w / f"kaynak-{senaryo}-{acilis:02}.csv"
    with dosya.open("w", encoding="utf-8", newline="") as f:
        tablo = csv.writer(f)
        tablo.writerow(("döngü", "durum", "boş_çerçeve", "çekirdek_heap_bayt",
                        "geçen_ms", "konuk_tik"))
        tablo.writerows(satirlar)
    print(f"KAYNAK {senaryo} {acilis} {dosya} ölçüm={len(satirlar)}", flush=True)


def cocuk(c, acilis):
    cmd = "calistir /diskler/usb0p2/bin/kaynakden.elf"
    bos = c.bos_cerceve()
    sonuc = [(0, "kabuk", bos, c.heap(), 0, 0)]
    c.input(cmd)
    c.akis += "> " + cmd + "\nOLC_HAZIR\n"
    c.expected += b"OLC_HAZIR\n"
    wait_for(c.p, lambda: c.output() == c.expected)

    ana = c.bos_cerceve()
    assert bos - ana == SUREC_CERCEVESI
    if acilis == 1:
        heap = c.heap()
        time.sleep(8)
        assert (ana, heap) == (c.bos_cerceve(), c.heap())

    def basladi():
        return re.match(rb"BAS (\d+)\n", c.output()[len(c.expected):])

    for i in range(1, 101):
        bas = time.monotonic_ns()
        c.key("spc", True)
        kayit = wait_for(c.p, basladi, 5)
        sure_ms = (time.monotonic_ns() - bas) / 1e6
        # Çocuk tuş beklediği sürece çerçeveleri canlı ölçülebilir.
        cer = c.bos_cerceve()
        assert ana - cer == SUREC_CERCEVESI, (i, ana, cer)
        sonuc.append((i, "başladı", cer, c.heap(), round(sure_ms, 3), int(kayit[1])))

        c.key("spc", False)
        tus(c, "a")
        c.expected += kayit[0] + b"SON\n"
        c.akis += kayit[0].decode() + "SON\n"
        wait_for(c.p, lambda: c.output().startswith(c.expected))
        if i < 100:
            cer = c.bos_cerceve()
            assert cer == ana, (i, cer, ana)
            sure_ms = (time.monotonic_ns() - bas) / 1e6
            sonuc.append((i, "toplandı", cer, c.heap(), round(sure_ms, 3), ""))

    tamam(c, "kaynakden", 2)
    assert c.bos_cerceve() == bos
    sonuc.append((100, "kabuk", bos, c.heap(), 0, ""))
    kaydet(c, "çocuk", acilis, sonuc)


def editor(c, part, acilis):
    yol = "/diskler/usb0p2/notlar/olcum.txt"
    c.run("bagla --yazilabilir usb0p2")
    bos = c.bos_cerceve()
    sonuc = [(0, "kabuk", bos, c.heap(), 0, "")]
    sinir = int(os.environ.get("SUREC_EDITOR_DONGU", "5"))
    assert 1 <= sinir <= 50

    for i in range(1, sinir + 1):
        cmd = "calistir /diskler/usb0p2/bin/duzenle.elf " + yol
        yaz(c, cmd)
        bas = time.monotonic_ns()
        tus(c, "ret")
        c.akis += "> " + cmd + "\n"
        # Zaman damgası eşleşen GOP görüntüsünün alındığı andır.
        yakalandi = du_ekran(c, yol, ["a" * (i - 1)], 0, 0)
        cer = c.bos_cerceve()
        assert bos - cer == SUREC_CERCEVESI
        sonuc.append((i, "açıldı", cer, c.heap(), round((yakalandi - bas) / 1e6, 3), ""))

        yaz(c, "a")
        tus(c, "s", ctrl=True)
        du_ekran(c, yol, ["a" * i], 0, 1, False, "  Kaydedildi")
        tus(c, "q", ctrl=True)
        tamam(c, "duzenle", i + 1)
        assert c.bos_cerceve() == bos
        assert Fat(part.read()).get("/NOTLAR/olcum.txt") == b"a" * i
        heap = c.heap()
        sure_ms = (time.monotonic_ns() - bas) / 1e6
        sonuc.append((i, "kaydedilip kapandı", bos, heap, round(sure_ms, 3), ""))
        if heap > EDITOR_HEAP_ESIGI and i < sinir:
            assert sinir > 5, f"Beş çevrimlik temel ölçümde heap tükendi: {i}/{sinir}"
            print(f"E4k {i}/{sinir} sürekli döngüde heap={heap}", flush=True)
            break

    fsck(c.w, part, "kaynak-editor")
    kaydet(c, "düzenleyici", acilis, sonuc)
