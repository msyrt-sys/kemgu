"""OVMF SMP kapılarında MADT'yi, QEMU CPU kimliklerini ve fiziksel belleği okur."""

import argparse
import os
import re
import shutil
import struct
import subprocess
from contextlib import contextmanager
from pathlib import Path

from bekle import bekle, qmp_baglan, seri_oku
from dalga12_imaj import images
from dalga12_kahin import Console, Qmp, boot


@contextmanager
def acilis(w, imaj, adet):
    w.mkdir(parents=True, exist_ok=True)
    qmp = w / "qmp.sock"
    seri = w / "seri.log"
    for dosya in (qmp, seri):
        dosya.unlink(missing_ok=True)
    firmware = Path("/usr/share/OVMF")
    degisken = w / "vars.fd"
    shutil.copyfile(firmware / "OVMF_VARS_4M.fd", degisken)
    komut = [
        "qemu-system-x86_64",
        "-M",
        "q35",
        "-accel",
        "tcg,thread=multi",
        "-smp",
        str(adet),
        "-m",
        "256M",
        "-display",
        "none",
        "-no-reboot",
        "-nic",
        "none",
        "-drive",
        f"if=pflash,format=raw,readonly=on,file={firmware}/OVMF_CODE_4M.fd",
        "-drive",
        f"if=pflash,format=raw,file={degisken}",
        "-drive",
        f"if=none,id=esp,format=raw,snapshot=on,file={imaj}",
        "-device",
        "nvme,drive=esp,serial=kemgu-smp,bootindex=1",
        "-device",
        "isa-debug-exit,iobase=0xf4,iosize=0x04",
        "-serial",
        f"file:{seri}",
        "-qmp",
        f"unix:{qmp},server=on,wait=off",
    ]
    with (w / "qemu.log").open("w", encoding="utf-8") as gunluk:
        p = subprocess.Popen(komut, stdout=gunluk, stderr=subprocess.STDOUT)
        try:
            with (
                qmp_baglan(qmp, p, sure=20) as sock,
                sock.makefile("wb", buffering=0) as yaz,
                sock.makefile("rb") as oku,
            ):
                yield Qmp(yaz, oku), p, seri
        finally:
            if p.poll() is None:
                p.kill()
            p.wait()


def cpu_listesi(q, adet):
    cpular = sorted(q.call("query-cpus-fast"), key=lambda c: c["cpu-index"])
    assert [c["cpu-index"] for c in cpular] == list(range(adet)), cpular
    tid = [c["thread-id"] for c in cpular]
    assert len(set(tid)) == adet and min(tid) > 0, tid
    ids = [q.call("qom-get", {"path": c["qom-path"], "property": "apic-id"}) for c in cpular]
    assert len(set(ids)) == adet, ids
    return ids, tid


def bellek(q, dizin, adres, boy):
    dosya = (dizin / "bellek.bin").resolve()
    q.call("pmemsave", {"val": adres, "size": boy, "filename": str(dosya)})
    veri = dosya.read_bytes()
    assert len(veri) == boy, (adres, boy, len(veri))
    return veri


def semboller(elf):
    tablo = subprocess.check_output(["nm", "-n", str(elf)], text=True)
    return {t[2]: int(t[0], 16) for s in tablo.splitlines() if len(t := s.split()) == 3}


def sayac(q, dizin, adres, boy=8):
    return int.from_bytes(bellek(q, dizin, adres, boy), "little")


def madt_kimlikleri(q, w, seri):
    es = re.search(rb"^ACPI_TABLO APIC (\d+) (\d+)$", seri, re.MULTILINE)
    assert es, seri[-800:]
    ham = bellek(q, w, int(es[1]), int(es[2]))
    assert ham[:4] == b"APIC" and len(ham) >= 44 and sum(ham) % 256 == 0
    assert int.from_bytes(ham[4:8], "little") == len(ham)
    kimlikler = {}
    i = 44
    while i < len(ham):
        tur, boy = struct.unpack_from("BB", ham, i)
        assert 2 <= boy <= len(ham) - i, (i, boy)
        if tur == 0:
            assert boy >= 8
            id_, bayrak = struct.unpack_from("<BI", ham, i + 3)
            assert id_ not in kimlikler
            kimlikler[id_] = bayrak
        i += boy
    return kimlikler


def kesif(q, p, seri, w, sim, adet):
    ids, tid = cpu_listesi(q, adet)
    veri = bekle(
        "SMP keşif sonucu",
        lambda: seri_oku(seri),
        lambda s: b"APIC_HAZIR\n" in s or b"AP cevrimici zaman asimi" in s or b"AP yigin siniri" in s,
        sure=45,
        surec=p,
    )
    assert b"APIC_HAZIR\n" in veri, veri[-1200:]
    madt = madt_kimlikleri(q, w, veri)
    etkin = {id_ for id_, bayrak in madt.items() if bayrak & 3}
    satirlar = [
        (int(i), int(id_), int(bsp))
        for i, id_, bsp in re.findall(rb"^SMP_CPU (\d+) (\d+) (\d+)$", veri, re.MULTILINE)
    ]
    if adet == 1:
        # Tek BSP için firmware çekirdeği mevcut APIC satırını ve sabit kayıt yuvasını kullanır.
        lapic = re.search(rb"^APIC_LAPIC \d+ (\d+) \d+$", veri, re.MULTILINE)
        assert lapic and not satirlar
        satirlar = [(0, int(lapic[1]), int(lapic[1]))]
    assert [i for i, _, _ in satirlar] == list(range(adet)), satirlar
    gercek = [id_ for _, id_, _ in satirlar]
    assert gercek[0] == ids[0] and {bsp for _, _, bsp in satirlar} == {ids[0]}
    assert set(ids) == set(gercek) == etkin, (ids, gercek, madt)
    assert sayac(q, w, sim["smp_cekirdek_sayisi"], 4) == adet
    adr = sayac(q, w, sim["smp_kimlik_sayfasi"])
    assert adr > 0 and (adet == 1 or adr % 4096 == 0)
    assert list(struct.unpack(f"<{adet}Q", bellek(q, w, adr, 8 * adet))) == gercek
    print(f"SMP keşif {adet}: APIC={gercek}, MADT={madt}, TCG thread-id={tid}", flush=True)


def yazmac(durum, ad):
    m = re.search(rf"\b{ad}\s*=\s*([a-fA-F0-9]+)", durum)
    assert m, (ad, durum)
    return int(m[1], 16)


def ap_baslatma(q, seri, w, sim, adet):
    veri = seri_oku(seri)
    es = re.search(rb"^SMP_SAYFA (\d+) (\d+) (\d+)$", veri, re.MULTILINE)
    assert es, veri[-800:]
    alcak, bilgi, cr3 = map(int, es.groups())
    assert 4096 <= alcak <= 1048576 - 8192 and alcak % 4096 == 0
    alan = struct.unpack("<24Q", bellek(q, w, bilgi, 192))
    harita = bellek(q, w, alan[9], alan[10])
    tanimlar = [struct.unpack_from("<IIQQQQ", harita, i) for i in range(0, len(harita), alan[11])]
    assert any(t == 7 and bas <= alcak and alcak + 8192 <= bas + n * 4096 for t, _, bas, _, n, _ in tanimlar)
    assert sayac(q, w, sim["smp_cevrimici"], 4) == adet
    kayit = sayac(q, w, sim["smp_ap_kayitlari"])
    ids, _ = cpu_listesi(q, adet)
    yiginlar = []
    zamanlar = {}
    for no, id_, us in re.findall(rb"^SMP_AP (\d+) (\d+) (\d+)$", veri, re.MULTILINE):
        no, id_, us = int(no), int(id_), int(us)
        assert no not in zamanlar and ids[no] == id_ and us > 0
        zamanlar[no] = us
    assert set(zamanlar) == set(range(1, adet))
    for no, id_ in enumerate(ids):
        durum = q.call("human-monitor-command", {"command-line": "info registers", "cpu-index": no})
        assert yazmac(durum, "CR0") & 0x80000001 == 0x80000001 and yazmac(durum, "EFER") & 0x400, (no, durum)
        assert yazmac(durum, "CR3") == cr3 and "CS64" in durum, (no, durum)
        if no == 0:
            bsp_rsp = yazmac(durum, "RSP")
            continue
        onay, alt, ust, rsp, ap_cr3, ap_id = struct.unpack("<6Q", bellek(q, w, kayit + no * 64, 48))
        assert onay == 1 and ap_id == id_ and ap_cr3 == cr3
        assert (
            alt % 4096 == 0 and ust - alt == 16384 and alt <= rsp < ust and alt <= yazmac(durum, "RSP") < ust
        )
        assert not alt <= bsp_rsp < ust
        assert ust <= sim["__stack_top"] - 524288 or alt >= sim["__stack_top"]
        assert all(ust <= a or b <= alt for a, b in yiginlar)
        yiginlar.append((alt, ust))
    print(
        f"SMP AP {adet}: sayfa={alcak:#x}, CR3={cr3:#x}, uzun kip={adet}, "
        f"ayrı AP yığını={len(yiginlar)}, başlatma us={list(zamanlar.values())}",
        flush=True,
    )


def tablo_taban(durum, ad, secici=False):
    sec = r"[0-9a-fA-F]+\s+" if secici else ""
    es = re.search(rf"^{ad}\s*=\s*{sec}([0-9a-fA-F]+)", durum, re.MULTILINE)
    assert es, (ad, durum)
    return int(es[1], 16)


def tik_kayitlari(q, w, sim, adet):
    ids, _ = cpu_listesi(q, adet)
    kayit = sayac(q, w, sim["smp_ap_kayitlari"])
    adresler = []
    gdtler, tssler, araliklar = set(), set(), []
    for no, id_ in enumerate(ids):
        durum = q.call(
            "human-monitor-command", {"command-line": "info registers", "cpu-index": no}
        )
        gs = tablo_taban(durum, "GS", True)
        adr = sayac(q, w, kayit + no * 64 + 48) if adet > 1 else sim["kdl_cekirdek_bsp"]
        assert gs == adr and adr not in adresler, (no, gs, adr, adresler)
        adresler.append(adr)
        n, kimlik, _, _, alt, ust, tss, gdt, *kalan = struct.unpack(
            "<12Q", bellek(q, w, adr, 96)
        )
        istler, cr3 = kalan[:3], kalan[3]
        assert (n, kimlik, cr3) == (no, id_, yazmac(durum, "CR3"))
        assert tablo_taban(durum, "IDT") == sim["kdl_idt"]
        assert tablo_taban(durum, "GDT") == gdt and gdt not in gdtler
        assert tablo_taban(durum, "TR", True) == tss and tss not in tssler
        assert tablo_taban(durum, "TR") == 0x28
        gdtler.add(gdt)
        tssler.add(tss)
        tanim, yuksek = struct.unpack("<QQ", bellek(q, w, gdt + 40, 16))
        gercek = (tanim >> 16 & 0xFFFFFF) | (tanim >> 32 & 0xFF000000) | (yuksek << 32)
        assert gercek == tss and tanim & 0xFFFF == 103
        assert (tanim >> 40 & 0x8F) == 0x8B
        ham = bellek(q, w, tss, 104)
        assert struct.unpack_from("<3Q", ham, 36) == tuple(istler)
        assert struct.unpack_from("<H", ham, 102)[0] == 104
        if no:
            assert struct.unpack_from("<Q", ham, 4)[0] == ust
        for ilk, son in [(alt, ust)] + [(son - 16384, son) for son in istler]:
            assert ilk < son and all(son <= a or b <= ilk for a, b in araliklar), (
                no,
                ilk,
                son,
                araliklar,
            )
            araliklar.append((ilk, son))
        for son in istler:
            assert sayac(q, w, son - 16384) == 0x4B454D4755495354
    return adresler


def tik(q, p, seri, w, sim, adet):
    bekle(
        "UEFI ana döngüsü",
        lambda: seri_oku(seri),
        lambda s: b"UEFI_HAZIR\n" in s,
        sure=30,
        surec=p,
    )
    adr = tik_kayitlari(q, w, sim, adet)

    def ornek():
        # Saat ve CPU'lar dururken bütün çekirdekler aynı anda örneklenir.
        q.call("stop")
        try:
            return [sayac(q, w, a + 16) for a in adr]
        finally:
            q.call("cont")

    ilk = ornek()
    son = bekle(
        "BSP'de 150 LAPIC tıkı",
        ornek,
        lambda s: s[0] - ilk[0] >= 150,
        sure=15,
        aralik=0.1,
        surec=p,
    )
    fark = [b - a for a, b in zip(ilk, son)]
    assert all(abs(x - fark[0]) * 5 <= fark[0] for x in fark), (
        "çekirdek tık farkları",
        fark,
    )
    print(
        f"SMP tık {adet}: GS={list(map(hex, adr))}, tık farkları={fark}, oran={[round(n / fark[0], 3) for n in fark]}",
        flush=True,
    )


def gs_kabuk(dizin):
    from surec_imaj import imajlar

    exe = "/diskler/usb0p2/bin/merhaba.elf"
    for adet in (1, 4):
        usb, nvme, sata = imajlar(
            dizin, {"merhaba.elf": (dizin / "kullanici/merhaba.elf").read_bytes()}
        )
        with boot(dizin, usb[0], nvme, sata, smp=adet) as (q, p, seri):
            c = Console(dizin, q, p, seri)
            adr = c.symbols["kdl_cekirdek_bsp"]
            ilk = sayac(q, dizin, adr + 96)
            c.run(
                f"calistir {exe} Ağaç",
                f"{exe}\nAğaç\n[merhaba.elf pid=1 cikis=2]\n".encode(),
            )
            n, kullanici_gs = struct.unpack("<QQ", bellek(q, dizin, adr + 96, 16))
            durum = q.call(
                "human-monitor-command",
                {"command-line": "info registers", "cpu-index": 0},
            )
            assert (
                n > ilk and kullanici_gs == 0 and tablo_taban(durum, "GS", True) == adr
            )
            c.shutdown()
        print(
            f"GS ring3 {adet}: kullanıcı tabanı={kullanici_gs}, sistem çağrısı={n - ilk}, BSP tabanı={adr:#x}",
            flush=True,
        )


def kabuk(dizin):
    usb, nvme, sata = images(dizin, 1)
    with boot(dizin, usb[0], nvme, sata, smp=4) as (q, p, seri):
        c = Console(dizin, q, p, seri)
        ap_baslatma(q, seri, dizin, c.symbols, 4)
        c.shutdown()
    print(
        "SMP UEFI kabuğu: dört işlemci uzun kipte, kabuk hazır ve düzenli kapanış.",
        flush=True,
    )


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("dizin", type=Path)
    ap.add_argument(
        "kip", nargs="?", default="kabuk", choices=("kesif", "ap", "kabuk", "tik")
    )
    ap.add_argument("--adet", nargs="+", type=int, default=None)
    a = ap.parse_args()
    dizin = a.dizin.resolve()
    if a.kip == "kabuk":
        if os.environ.get("SMP_KABUK_GS") == "1":
            gs_kabuk(dizin)
        else:
            kabuk(dizin)
        return
    sim = semboller(dizin / "k.elf")
    for adet in a.adet or {"kesif": [1, 2, 4, 8], "ap": [2, 4, 8], "tik": [4]}[a.kip]:
        w = dizin / f"{a.kip}-{adet}"
        with acilis(w, dizin / "pc.img", adet) as (q, p, seri):
            kesif(q, p, seri, w, sim, adet)
            if a.kip == "ap":
                ap_baslatma(q, seri, w, sim, adet)
            elif a.kip == "tik":
                tik(q, p, seri, w, sim, adet)


if __name__ == "__main__":
    main()
