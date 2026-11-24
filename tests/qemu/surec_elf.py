#!/usr/bin/env python3
"""Bağımsız x86_64 kullanıcı ELF derlemesi ve program başlığı denetimi."""

import argparse
import re
import struct
import subprocess
import tempfile
from pathlib import Path

PENCERE = 0x54200000
HEAP = 0x54600000
HEAP_SON = 0x54e00000
SAYFA = 4096
ORTAK = (
    "runtime/kem_heap.kem",
    "runtime/kem_dizi_genis.kem",
    "runtime/kullanici/x86_64/kem_sis.kem",
    "runtime/kullanici/x86_64/kem_ag.kem",
    "runtime/kullanici/x86_64/kem_basla.kem",
)
# Aynı adlar ELF içinde kendi kullanıcı çalışma zamanından çözülebilir.
KULLANICI_SEMBOLLERI = {
    "_start", "__heap_start", "__heap_end", "memset", "memcpy", "malloc", "free",
    "kullanici_main", "kem_heap_kur", "kem_panik", "kem_uart_bayt",
    "kdl_panik", "kdl_dizi_oob", "kdl_mimari_dizi_oob",
    "kdl_bolge_olustur", "kdl_bolge_ayir", "kdl_bolge_serbest", "kdl_global_bolge_al",
    "kdl_dizi_olustur", "kdl_dizi_ekle_tam", "kdl_dizi_al_tam", "kdl_dizi_yaz_tam",
    "kdl_dizi_boyut", "kdl_dizi_kapasite_ayarla", "kdl_metin_uzunluk", "kdl_metin_bayt",
    "kdl_metin_birlestir", "kdl_tam_metin", "kdl_kod_metin",
    "kdl_dizi_ekle_tam64", "kdl_dizi_al_tam64", "kdl_dizi_yaz_tam64",
    "kdl_dizi_ekle_ptr", "kdl_dizi_al_ptr", "kdl_dizi_yaz_ptr",
    "kdl_dizi_ekle_yapi", "kdl_dizi_al_yapi", "kdl_dizi_yaz_yapi",
}


def cekirdek_sembolleri():
    yollar = [*Path("runtime/mimari/x86_64").glob("*.kem"),
              *Path("test/ornekler").glob("kem_os_x86*.kem"),
              Path("runtime/kem_gorev_cekirdek.kem")]
    adlar = set()
    for yol in yollar:
        adlar.update(re.findall(r"\bişlev\s+(\w+)\s*\(", yol.read_text(encoding="utf-8")))
    asm = Path("boot/start_x86_64.S").read_text(encoding="utf-8")
    adlar.update(re.findall(r"^\s*\.(?:global|weak)\s+(\w+)", asm, re.M))
    return adlar


def denetle(elf):
    veri = elf.read_bytes()
    if len(veri) < 64:
        raise ValueError("ELF başlığı eksik")
    imza, tur, mimari, _, giris, ph, _, _, eb, pb, adet, *_ = struct.unpack_from(
        "<16sHHIQQQIHHHHHH", veri
    )
    alt, heap = (0x54000000, 0x54100000) if giris == 0x54000000 else (PENCERE, HEAP)
    if (imza[:7], tur, mimari, giris, eb, pb) != (
        b"\x7fELF\x02\x01\x01", 2, 62, alt, 64, 56
    ) or ph < eb or ph + pb * adet > len(veri):
        raise ValueError("ET_EXEC, EM_X86_64, giriş veya başlık boyutu geçersiz")
    bolgeler = []
    giris_segmenti = False
    for i in range(adet):
        tip, bayrak, ofset, va, _, dosya_boyu, bellek_boyu, hiza = struct.unpack_from(
            "<IIQQQQQQ", veri, ph + i * pb
        )
        if tip != 1 or not bellek_boyu:
            continue
        son = (va + bellek_boyu + SAYFA - 1) // SAYFA * SAYFA
        if (bayrak not in (4, 5, 6) or not alt <= va < son <= heap
                or hiza != SAYFA or va % SAYFA or ofset % SAYFA
                or dosya_boyu > bellek_boyu or ofset + dosya_boyu > len(veri)
                or any(va < b and a < son for a, b in bolgeler)):
            raise ValueError("PT_LOAD izinleri, hizalama veya sınırları geçersiz")
        bolgeler.append((va, son))
        giris_segmenti |= bayrak == 5 and va <= giris < va + dosya_boyu
    if not giris_segmenti:
        raise ValueError("Giriş, dosyada yer alan R-X segmentinde bulunamadı")
    if subprocess.check_output(["llvm-nm", "-u", elf], text=True).strip():
        raise ValueError("Kullanıcı ELF'inde çözümlenmemiş sembol var")
    tanimli = subprocess.check_output(
        ["llvm-nm", "--defined-only", elf], text=True
    )
    semboller = {satir.split()[-1] for satir in tanimli.splitlines() if satir.strip()}
    cekirdek = cekirdek_sembolleri()
    yabanci = {s for s in semboller - KULLANICI_SEMBOLLERI if s in cekirdek or s.startswith("kdl_")}
    if yabanci:
        raise ValueError(f"Kullanıcı ELF'inde dış semboller var: {sorted(yabanci)}")
    print(f"✓ {elf.name}: ET_EXEC x86_64, {len(veri)} bayt, {len(bolgeler)} PT_LOAD, çözümsüz veya çekirdek sembolü yok")


def derle(derleyici, kaynak, hedef, eski=False):
    hedef.parent.mkdir(parents=True, exist_ok=True)
    hedef.unlink(missing_ok=True)
    with tempfile.TemporaryDirectory(prefix="elf-", dir=hedef.parent) as tmp:
        w = Path(tmp)
        birlesik, ham, ir, nesne, elf = (w / f"u.{uzanti}" for uzanti in ("kem", "ham.ll", "ll", "o", "elf"))
        ek = ("runtime/kullanici/x86_64/kem_http.kem",) if Path(kaynak).stem in {"indir", "sunucu", "paket"} else ()
        birlesik.write_bytes(b"".join(Path(p).read_bytes() for p in (*ORTAK, *ek, kaynak)))
        linker = Path("linker/user-x86_64.ld")
        if eski:
            # Dalga 13'ün sabit giriş, heap ve argc adresleriyle bir ET_EXEC üret.
            kod = birlesik.read_text().replace("KUL_ARGC_ADRESI: tam64 = 1426059264", "KUL_ARGC_ADRESI: tam64 = 1411379200")
            birlesik.write_text(kod)
            ld = linker.read_text().replace("0x54200000", "0x54000000").replace("0x54600000", "0x54100000").replace("0x54e00000", "0x541ef000")
            linker = w / "eski.ld"
            linker.write_text(ld)
        subprocess.run(["build/kemgu", "--check", "--mimari", "x86_64-none", birlesik], check=True)
        with ham.open("wb") as cikti:
            subprocess.run([derleyici, "--llvm", "--mimari", "x86_64-none", birlesik], stdout=cikti, check=True)
        with ir.open("wb") as cikti:
            subprocess.run(["awk", "-f", "test/strip_defined_declares.awk", ham], stdout=cikti, check=True)
        subprocess.run([
            "clang", "-target", "x86_64-unknown-none", "-ffreestanding", "-nostdlib",
            "-mgeneral-regs-only", "-O2", "-Wno-override-module", "-ffunction-sections",
            "-fdata-sections", "-x", "ir", ir, "-c", "-o", nesne,
        ], check=True)
        subprocess.run([
            "ld.lld", "-m", "elf_x86_64", "--gc-sections", "-T", linker,
            "-z", "max-page-size=4096", "-o", elf, nesne,
        ], check=True)
        denetle(elf)
        elf.replace(hedef)


if __name__ == "__main__":
    ayr = argparse.ArgumentParser(description=__doc__)
    ayr.add_argument("elf", type=Path)
    ayr.add_argument("--eski", action="store_true")
    ayr.add_argument("--kaynak", type=Path)
    ayr.add_argument("--derleyici", default="build/kemgu")
    args = ayr.parse_args()
    try:
        if args.kaynak:
            derle(args.derleyici, args.kaynak, args.elf, args.eski)
        else:
            denetle(args.elf)
    except (OSError, ValueError, subprocess.CalledProcessError) as e:
        ayr.exit(1, f"ELF denetimi: {e}\n")
