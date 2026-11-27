"""Gerçek ELF çocukları için seri sonuç ve donmuş makinedeki işlemci kanıtı."""

import re
import struct
import sys
from pathlib import Path
from time import monotonic

from bekle import bekle
from dalga12_kahin import boot
from smp_kahin import bellek, cpu_listesi, sayac, tablo_taban, yazmac
from surec_imaj import imajlar
from surec_kahin import SurecConsole


def eszamanli(q, w, cpu_adresleri, tablo):
    q.call("stop")
    try:
        kosan = []
        tssler = set()
        for no, adres in enumerate(cpu_adresleri):
            durum = q.call(
                "human-monitor-command",
                {"command-line": "info registers", "cpu-index": no},
            )
            if yazmac(durum, "CPL") != 3:
                continue
            kok = yazmac(durum, "CR3")
            assert tablo_taban(durum, "GS", True) == 0
            ham = bellek(q, w, adres, 144)
            alt, ust, tss = struct.unpack_from("<3Q", ham, 32)
            assert tablo_taban(durum, "TR", True) == tss and tss not in tssler
            rsp0 = sayac(q, w, tss + 4)
            assert alt < rsp0 <= ust and rsp0 % 16 == 0, (no, alt, rsp0, ust)
            tssler.add(tss)
            (pid,) = struct.unpack_from("<Q", ham, 24)
            (yerel_kok,) = struct.unpack_from("<Q", ham, 88)
            (baglam,) = struct.unpack_from("<Q", ham, 136)
            assert (
                kok == yerel_kok
                and 0 <= baglam - tablo < 8 * 272
                and (baglam - tablo) % 272 == 0
            )
            kayit = bellek(q, w, baglam + 168, 32)
            assert (
                struct.unpack_from("<Q", kayit)[0] == pid
                and struct.unpack_from("<Q", kayit, 24)[0] == kok
            )
            if 2 <= pid <= 5:
                kosan.append((no, pid, hex(kok)))
        assert len({k[1] for k in kosan}) == len({k[2] for k in kosan}) == len(kosan), (
            kosan
        )
        return kosan
    finally:
        q.call("cont")


def tur(w, n):
    usb, nvme, sata = imajlar(
        w, {"smpden.elf": (w / "kullanici/smpden.elf").read_bytes()}
    )
    with boot(w, usb[0], nvme, sata, smp=n) as (q, p, seri):
        c = SurecConsole(w, q, p, seri)
        apic, tid = cpu_listesi(q, n)
        adresler = [c.symbols["kdl_cekirdek_bsp"]]
        if n > 1:
            kayit = sayac(q, w, c.symbols["smp_ap_kayitlari"])
            adresler = [sayac(q, w, kayit + i * 64 + 48) for i in range(n)]

        def rapor_sina(cocuklar_calisti):
            kilit = sayac(q, w, c.symbols["smp_bellek_kilitleri"])

            def ornek():
                sonuc = []
                for no, adr in enumerate(adresler):
                    g = bellek(q, w, adr + 16, 200)
                    (tik,) = struct.unpack_from("<Q", g)
                    tek, _, bos = struct.unpack_from("<3Q", g, 176)
                    dilim = (
                        sayac(q, w, kilit + 128 + 64 + no * 64 + 16) if n > 1 else tek
                    )
                    # Bu deneyde beş farklı PID ayrı yuvalarda. Fiziksel PID
                    # dizisinden say; aynı çocuğun yüzlerce tıkı tek PID'dir.
                    pidler = struct.unpack("<8Q", bellek(q, w, adr + 232, 64))
                    farkli = len({pid for pid in pidler if pid})
                    assert sayac(q, w, adr + 224) == farkli <= dilim
                    sonuc.append((tik, bos, farkli))
                return sonuc

            once = ornek()
            kayit = []
            c.run("islemci", kayit.append)
            sonra = ornek()
            veri = kayit[0]
            satirlar = [
                list(map(int, s.split()[1:]))
                for s in veri.splitlines()
                if s.startswith(b"ISLEMCI ")
            ]
            assert len(satirlar) == n and [s[1] for s in satirlar] == apic, satirlar
            for i, (no, _, tik, farkli, bosta) in enumerate(satirlar):
                alt, ust = once[i], sonra[i]
                assert no == i and alt[0] <= tik <= ust[0] and alt[2] <= farkli <= ust[2]
                assert (
                    alt[1] * 100 // max(1, ust[0])
                    <= bosta
                    <= min(100, ust[1] * 100 // max(1, alt[0]))
                )
                if cocuklar_calisti:
                    assert 1 <= farkli <= 5
                    if n == 1:
                        assert farkli == 5
            print("QMP ile doğrulanan ISLEMCI satırları", satirlar, flush=True)

        rapor_sina(False)
        c.input("calistir /diskler/usb0p2/bin/smpden.elf")

        def kuyruk():
            s = c.output()[len(c.expected) :]
            if b"KABUK_HAZIR" in s and b"SMP BITTI\n" not in s:
                raise AssertionError(s)
            return s

        bekle(
            "dört çocuk boru engelinde",
            kuyruk,
            lambda s: b"SMP BAS\n" in s,
            sure=30,
            surec=p,
        )
        bas = monotonic()
        ornek = []
        if n > 1:

            def ornekle():
                if b"SMP SONUC " in kuyruk():
                    raise AssertionError(
                        f"Dört kullanıcı süreci ayrı çekirdeklerde görülmedi: {ornek}"
                    )
                ornek[:] = eszamanli(q, w, adresler, c.symbols["kdl_dilim_tablo"])
                return ornek

            bekle(
                "dört ayrı CPL3/PID/CR3",
                ornekle,
                lambda s: len(s) == 4,
                sure=30,
                aralik=0.04,
                surec=p,
            )
        son = bekle(
            "çocuk sonuçları ve bekle sonrası kabuk",
            kuyruk,
            lambda s: s.endswith(b"KABUK_HAZIR\n"),
            sure=120,
            surec=p,
        )
        gecen = monotonic() - bas
        beklenen = {}
        for i in range(4):
            adet = 600000000 + i * 1009
            beklenen[i] = adet * (adet + 1) // 2
        satirlar = re.findall(rb"^SMP SONUC (\d+) (\d+) (\d+)$", son, re.MULTILINE)
        assert (
            len(satirlar) == 4 and {int(i): int(t) for i, t, _ in satirlar} == beklenen
        ), son
        assert all(0 <= int(no) < n for _, _, no in satirlar)
        assert b"SMP BITTI\n[smpden.elf pid=1 cikis=0]\n" in son, son
        print(
            f"SMP {n}: {gecen:.3f} sn, TCG TID={tid}, CPL3 (no,PID,CR3)={ornek}, sonuçlar={beklenen}",
            flush=True,
        )
        c.expected += son
        rapor_sina(True)
        c.shutdown()
        return gecen


if __name__ == "__main__":
    w = Path(sys.argv[1]).resolve()
    bir = tur(w, 1)
    dort = tur(w, 4)
    print(f"Dört süreç hızlanması {bir / dort:.3f}×", flush=True)
    assert dort < bir, (bir, dort)
