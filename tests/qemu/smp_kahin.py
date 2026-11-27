"""OVMF SMP kapılarında MADT'yi, QEMU CPU kimliklerini ve fiziksel belleği okur."""

import argparse
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


def kabuk(dizin):
    usb, nvme, sata = images(dizin, 1)
    with boot(dizin, usb[0], nvme, sata, smp=4) as (q, p, seri):
        c = Console(dizin, q, p, seri)
        ap_baslatma(q, seri, dizin, c.symbols, 4)
        c.shutdown()
    print("SMP UEFI kabuğu: dört işlemci uzun kipte, kabuk hazır ve düzenli kapanış.", flush=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("dizin", type=Path)
    ap.add_argument("kip", nargs="?", default="kabuk", choices=("kesif", "ap", "kabuk"))
    ap.add_argument("--adet", nargs="+", type=int, default=None)
    a = ap.parse_args()
    dizin = a.dizin.resolve()
    if a.kip == "kabuk":
        kabuk(dizin)
        return
    sim = semboller(dizin / "k.elf")
    for adet in a.adet or ([1, 2, 4, 8] if a.kip == "kesif" else [2, 4, 8]):
        w = dizin / f"{a.kip}-{adet}"
        with acilis(w, dizin / "pc.img", adet) as (q, p, seri):
            kesif(q, p, seri, w, sim, adet)
            if a.kip == "ap":
                ap_baslatma(q, seri, w, sim, adet)


if __name__ == "__main__":
    main()
