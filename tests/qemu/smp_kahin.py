"""OVMF SMP kapılarında MADT'yi, QEMU CPU kimliklerini ve fiziksel belleği okur."""

import argparse
import re
import shutil
import struct
import subprocess
from contextlib import contextmanager
from pathlib import Path

from bekle import bekle, qmp_baglan, seri_oku
from dalga12_kahin import Qmp


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
        lambda s: b"SMP_KESIF_HAZIR\n" in s or b"SMP_HATA " in s,
        sure=45,
        surec=p,
    )
    assert b"SMP_KESIF_HAZIR\n" in veri, veri[-1200:]
    madt = madt_kimlikleri(q, w, veri)
    etkin = {id_ for id_, bayrak in madt.items() if bayrak & 3}
    satirlar = [
        (int(i), int(id_), int(bsp))
        for i, id_, bsp in re.findall(rb"^SMP_CPU (\d+) (\d+) (\d+)$", veri, re.MULTILINE)
    ]
    assert [i for i, _, _ in satirlar] == list(range(adet)), satirlar
    gercek = [id_ for _, id_, _ in satirlar]
    assert gercek[0] == ids[0] and {bsp for _, _, bsp in satirlar} == {ids[0]}
    assert set(ids) == set(gercek) == etkin, (ids, gercek, madt)
    assert sayac(q, w, sim["smp_cekirdek_sayisi"], 4) == adet
    adr = sayac(q, w, sim["smp_kimlik_sayfasi"])
    assert adr > 0 and adr % 4096 == 0
    assert list(struct.unpack(f"<{adet}Q", bellek(q, w, adr, 8 * adet))) == gercek
    print(f"SMP keşif {adet}: APIC={gercek}, MADT={madt}, TCG thread-id={tid}", flush=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("dizin", type=Path)
    ap.add_argument("kip", choices=("kesif",))
    ap.add_argument("--adet", nargs="+", type=int, default=[1, 2, 4, 8])
    a = ap.parse_args()
    dizin = a.dizin.resolve()
    sim = semboller(dizin / "k.elf")
    for adet in a.adet:
        w = dizin / f"{a.kip}-{adet}"
        with acilis(w, dizin / "pc.img", adet) as (q, p, seri):
            kesif(q, p, seri, w, sim, adet)


if __name__ == "__main__":
    main()
