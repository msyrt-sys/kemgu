"""Dört hesap süreci sırasında gerçek blok, IRQ ve Ethernet teslimini karşılaştır."""

import re
import signal
import struct
import sys
from pathlib import Path
from time import monotonic

from ag_es import ES_IP, ES_MAC, KONUK_IP, EthernetEs, arp, ethernet, ipv4, yankilama
from ag_l3_kahin import MAC
from bekle import bekle
from dalga12_fat import Fat
from dalga12_kahin import boot
from pcap_oku import cerceve_oku
from smp_kahin import cpu_listesi, yazmac
from surec_imaj import imajlar
from surec_kahin import SurecConsole
from uefi_kabuk_kahin import keys


def panik(c, mesaj):
    # Enter bırakma işlemi için bir sonraki tıkı bekleme: bütün saatler duracak.
    for ad in "kesme sina":
        tus, kaydir = keys(ad)
        assert not kaydir
        c.key(tus, True)
        c.key(tus, False)
    c.tus_gonder("ret", True)
    bekle("tek panik iletisi", c.output, lambda s: mesaj in s, sure=12, surec=c.p)
    cpu_listesi(c.q, 4)

    kayit = c.sayi("smp_ap_kayitlari")
    adres = [
        int.from_bytes(c.mem(kayit + i * 64 + 48, 8), "little") + 16 for i in range(4)
    ]

    def durum():
        sonuc = []
        for i, adr in enumerate(adres):
            r = c.q.call(
                "human-monitor-command",
                {"command-line": "info registers", "cpu-index": i},
            )
            sonuc.append(
                (
                    int("HLT=1" in r),
                    (yazmac(r, "RFL") >> 9) & 1,
                    hex(yazmac(r, "RIP")),
                    int.from_bytes(c.mem(adr, 8), "little"),
                )
            )
        return sonuc

    ilk = durum()
    print("Panik ilk (halt, IF, RIP, tık)", ilk, flush=True)
    rs = bekle(
        "tüm vCPU'lar CLI ve HLT",
        durum,
        lambda kayit: all(x[:2] == (1, 0) for x in kayit),
        sure=5,
        surec=c.p,
    )
    assert c.output().count(mesaj) == 1
    assert int.from_bytes(c.mem(c.symbols["kdl_panik_lider"], 4), "little") in range(
        1, 5
    )
    resim = c.w / "smp-panik.ppm"
    c.q.call("screendump", {"filename": str(resim)})
    ham = resim.read_bytes()
    bas = re.match(rb"P6\s+(\d+)\s+(\d+)\s+255\s", ham)
    assert bas
    en, boy = map(int, bas.groups())
    px = ham[bas.end() :]
    kirmizi = bytes.fromhex("aa0000")
    beyaz = bytes.fromhex("ffffff")
    assert len(px) == en * boy * 3 and px[-3:] == kirmizi
    assert px.count(kirmizi) + px.count(beyaz) == en * boy and px.count(beyaz) > 80
    assert rs == durum()
    print(
        "Panik",
        mesaj,
        "RIP",
        [x[2] for x in rs],
        "GOP",
        en,
        boy,
        flush=True,
    )


def tur(w, yalniz_panik=False):
    disk, nvme, sata = imajlar(
        w, {"hesap.elf": (w / "kullanici/hesap.elf").read_bytes()}
    )
    with EthernetEs(dizin=w) as es:
        net = (
            "-netdev",
            es.qemu_netdev(),
            "-device",
            f"e1000,id=ag0,netdev=n0,mac={MAC.hex(':')}",
        )
        with boot(w, disk[0], nvme, sata, klavye="ps2", smp=4, ag_aygitlari=net) as (
            q,
            p,
            ser,
        ):
            c = SurecConsole(w, q, p, ser)
            cpu_listesi(q, 4)
            if yalniz_panik:
                panik(c, b"SMP DURUS DENEYI\n")
                return
            c.run("ag ayarla 192.0.2.2/24 192.0.2.1", b"AG AYAR OK\n")

            def al(kosul):
                son = monotonic() + 6
                while monotonic() < son:
                    a = cerceve_oku(es.al(max(0.001, son - monotonic())))
                    if kosul(a):
                        return a
                raise AssertionError("telde beklenen yanıt yok")

            es.gonder(
                ethernet(MAC, ES_MAC, 2054, arp(1, ES_MAC, ES_IP, bytes(6), KONUK_IP))
            )
            al(lambda a: a.tur == 2054 and a.yuk[6:8] == b"\0\2")
            bos = c.bos_cerceve()
            for pid in range(1, 5):
                c.run(
                    "baslat /diskler/usb0p2/bin/hesap.elf uzun &",
                    f"pid={pid}\n".encode(),
                )

            def dilimler():
                tablo = c.mem(c.symbols["kdl_dilim_tablo"], 4 * 272)
                return [
                    struct.unpack_from("<Q", tablo, i * 272 + 184)[0] for i in range(4)
                ]

            ilk = bekle(
                "dört işlemin ilk gerçek zaman dilimi",
                dilimler,
                lambda d: min(d) > 0,
                sure=12,
                surec=p,
            )

            def canli():
                def dogrula(s):
                    bulunan = re.findall(
                        rb"^(\d+) (?:hazir|kosuyor) hesap\.elf (\d+)$", s, re.MULTILINE
                    )
                    assert sorted(int(pid) for pid, _ in bulunan) == [1, 2, 3, 4], s
                    assert all(int(t) > 0 for _, t in bulunan), s

                c.run("ps", dogrula)

            canli()
            vektor = c.sayi("kesme_tablo")

            def irq():
                return [
                    struct.unpack_from("<2Q", c.mem(vektor + i * 32, 16))
                    for i in range(80)
                ]

            nv = next(i for i, (k, _) in enumerate(irq()) if k == 6)

            def rd():
                return next(
                    x["stats"]["rd_operations"]
                    for x in q.call("query-blockstats")
                    if x.get("device") == "dnvme"
                )

            def yanki(i):
                yuk = bytes(range(32)) + bytes([i])
                es.gonder(
                    ethernet(
                        MAC,
                        ES_MAC,
                        2048,
                        ipv4(ES_IP, KONUK_IP, 1, yankilama(8, 1800, i, yuk)),
                    )
                )
                cevap = al(lambda a: a.protokol == 1 and a.icmp_tur == 0)
                assert (cevap.kaynak, cevap.hedef, cevap.yuk[4:]) == (
                    MAC,
                    ES_MAC,
                    struct.pack("!HH", 1800, i) + yuk,
                )

            yanki(0)
            kaynak = Fat(nvme.read()).get("/merhaba.txt")
            c.run("cat /diskler/nvme0p1/merhaba.txt", kaynak + b"\n")
            ham = nvme.read()[:4096]
            gc = (
                f"SMP_GC 8 512 {sum((i + 1) * b for i, b in enumerate(ham))}\n".encode()
            )
            c.run("kesme disk", gc)
            once = irq()
            r = rd()
            heap, sayfa = c.heap(), c.bos_cerceve()
            for i in range(1, 5):
                c.run("kesme disk", gc)
                yanki(i)
            istek = rd() - r
            son = irq()
            assert istek == 32 and son[nv][1] - once[nv][1] == istek, (
                istek,
                once[nv],
                son[nv],
            )
            assert son[33][1] > once[33][1]
            halka = c.sayi("ps2_irq_klavye")
            assert halka > 0
            for i in range(5, 21):
                c.key("shift", True)
                yanki(i)
                c.key("shift", False)
            assert (c.heap(), c.bos_cerceve()) == (heap, sayfa)
            assert int.from_bytes(c.mem(halka + 16, 8), "little") == 0
            assert c.sayi("ps2_irq_hata") == c.sayi("smp_kilit_zaman_asimi") == 0
            assert irq()[33][1] - son[33][1] == 32
            canli()
            assert all(b > a for a, b in zip(ilk, dilimler(), strict=True))
            for pid in range(1, 5):
                c.run(f"sonlandir {pid}")
                c.run(
                    f"bekle {pid}",
                    f"[hesap.elf pid={pid} SONLANDIRILDI: sonlandir (kullanici)]\n".encode(),
                )
            assert c.bos_cerceve() == bos
            print(
                f"NVMe {istek} okuma/MSI-X; klavye 32 IRQ; ICMP 21/21; heap {heap}, boş {sayfa}, toplandı {bos}",
                flush=True,
            )
            panik(c, b"SMP DURUS DENEYI\n")


if __name__ == "__main__":

    def sure_doldu(*_):
        raise TimeoutError("SMP sağlamlık koşumu 240 saniyeyi aştı")

    signal.signal(signal.SIGALRM, sure_doldu)
    signal.alarm(240)
    try:
        tur(Path(sys.argv[1]).resolve())
    finally:
        signal.alarm(0)
