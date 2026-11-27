"""Make tariflerinde ve harness kaynaklarında sabit yazma yolları ve host uçları."""

import argparse
import ast
import posixpath
import re
from collections import defaultdict
from dataclasses import dataclass
from itertools import pairwise
from pathlib import Path

from make_tarifleri import onkosul_agaci, tarifleri_oku

KOK = Path(__file__).resolve().parents[2]
BETIK = re.compile(r"(?<![\w/])((?:test|tests/qemu|tools/codex|otomasyon/chatgpt)/[\w./-]+\.(?:sh|py))\b")
DEGISKEN = re.compile(r"(?<!\$)\$(?:\{(\w+)\}|(\w+))")
TOKEN = r"""(?:"[^"\n]*"|'[^'\n]*'|[^\s;|&<>]+)"""
CIKTI = re.compile(rf"(?:\s-o\s+|(?<![<>])>{{1,2}}\s*|\bmkdir\s+(?:-[\w-]+\s+)*)({TOKEN})")
ATAMA = re.compile(rf"(?:^|[\s;])(\w+)=({TOKEN})")
KOKLER = {
    "W",
    "w",
    "TMP",
    "TMPDIR",
    "DIR_OUT",
    "CALISMA",
    "UEFI_CALISMA_DIZINI",
    "BM_CALISMA_DIZINI",
    "UEFI_PC_IMG",
}
UC = re.compile(r"(?:(?:hostfwd=|\b)(tcp|udp):(?:127\.0\.0\.1|localhost|0\.0\.0\.0|\[::1\])?:([0-9]+))")
SOKET = re.compile(r"(?:\bunix:|\baddr\.path=|\bfile:)([^,\s\x22\x27]+)")
DINAMIK = re.compile(r"\$\$|\$\{|\$\(|\$\w|[{}*%]|X{6}")
YOK = {"/dev/null", "stdio", "-"}


@dataclass(frozen=True, order=True)
class Erisim:
    yol: str
    tur: str
    dosya: str
    satir: int


def temiz(s):
    return s.strip(' \t"\x27()').removeprefix("./")


def yol_kaydi(s, tur, dosya, satir):
    s = temiz(s)
    if not s or s in YOK or DINAMIK.search(s):
        return None
    if tur != "port":
        s = posixpath.normpath(s)
        s = s.removeprefix(KOK.as_posix() + "/")
    if tur in ("port", "uç") or s.startswith(("build/", "/tmp/")):
        return Erisim(s, tur, dosya, satir)
    return None


def qemu_ciktisi(bayrak, deger, anlik):
    if bayrak in ("-hda", "-hdb", "-hdc", "-hdd"):
        return None if anlik else (deger, "imaj")
    if bayrak not in ("-drive", "-object"):
        return None
    secimler = temiz(deger).split(",")
    dosya = next((x[5:] for x in secimler if x.startswith("file=")), None)
    if not dosya:
        return None
    if bayrak == "-object":
        return (dosya, "yazma") if "filter-dump" in secimler else None
    if anlik or set(secimler) & {"readonly=on", "read-only=on", "snapshot=on", "media=cdrom"}:
        return None
    return dosya, "imaj"


def kabuk(metin, dosya, ilk, hedef=""):
    env = {}
    bulgular = set()

    def al(s, tur, no):
        s = temiz(s)
        s = re.sub(r"\$\{\w+:-(build/[^}]+)\}", r"\1", s)
        s = DEGISKEN.sub(lambda m: env.get(m[1] or m[2], m[0]), s)
        # Sabit kökün içindeki modül/Make desenleri tek sahipli çıktı ailesidir.
        if (
            hedef
            and s.startswith("build/")
            and not any(x in s for x in ("$$", "$BASHPID", "$RANDOM", "XXXXXX", "$("))
        ) and ("%" in s or re.search(r"\$[A-Za-z_]", s)):
            s = re.sub(r"\$(\w+)", r"<\1>", s.replace("%", "<desen>"))
            tur = "şablon"
        if r := yol_kaydi(s, tur, dosya, no):
            bulgular.add(r)

    satirlar = metin.splitlines()
    for indeks, satir in enumerate(satirlar):
        no = ilk + indeks
        if indeks == 0 or not satirlar[indeks - 1].endswith("\\"):
            bas = no
            parcalar = []
        parcalar.append(satir.removesuffix("\\"))
        if satir.endswith("\\"):
            continue
        no = bas
        s = (
            " ".join(parcalar)
            .lstrip("@+-\t ")
            .replace("$(BUILD)", "build")
            .replace("$(EXE)", "")
            .replace("$@", hedef)
        )
        # Tariflerde make, çift doları kabuğa tek dolar olarak verir.
        if hedef:
            s = s.replace("$$", "$")
        if not s or s.startswith("#"):
            continue
        for ad, deger in ATAMA.findall(s):
            deger = temiz(deger)
            if hedef and (re.fullmatch(r"\$\{\w+[%#].+}", deger) or deger == "$(basename"):
                deger = f"<{ad}>"
            env[ad] = DEGISKEN.sub(lambda m: env.get(m[1] or m[2], m[0]), deger)
            if ad == "kok" and "KEMGU_CEKIRDEK_ONBELLEK_KOK" in deger:
                al(deger, "önbellek", no)
            if ad in KOKLER:
                al(env[ad], "çalışma", no)
            if ad.upper() in ("PORT", "TCP_PORT", "UDP_PORT") and deger.isdigit() and int(deger):
                al(("udp:" if ad.startswith("UDP") else "tcp:") + deger, "port", no)
        if "tools/kapi/kilitli.py " in s:
            for m in re.finditer(r"tools/kapi/kilitli\.py\s+(\S+)", s):
                al(m[1], "kilit", no)
        # Positional çıktılar argv sözleşmesinden gelir. Derleyici/kaynak argümanları girdidir.
        for m in re.finditer(rf"(?:uefi_imaj|paket_deposu|ag_arm_derle|surec_elf)\.py\s+({TOKEN})", s):
            al(m[1], "ürün", no)
        for m in re.finditer(rf"disk_denetimi\.py\s+basla\s+{TOKEN}\s+({TOKEN})", s):
            al(m[1], "yazma", no)
        for m in re.finditer(rf"(?:\bof=|\bZAMAN_KAYDI=)({TOKEN})", s):
            al(m[1], "yazma", no)
        for m in re.finditer(r"\b(?:llvm-)?objcopy\s+([^;&|\n]+)", s):
            al(m[1].split()[-1], "ürün", no)
        for m in CIKTI.finditer(s):
            al(m[1], "yazma", no)
        for m in re.finditer(rf"\b(?:cp|mv)\s+(?:--?\w+\s+)*\S+\s+({TOKEN})", s):
            al(m[1], "yazma", no)
        for m in UC.finditer(s):
            if int(m[2]):
                al(m[1] + ":" + m[2], "port", no)
        for m in SOKET.finditer(s):
            al(m[1], "uç", no)
        for m in re.finditer(rf"(?:^|\s)(-drive|-object|-hd[abcd])\s+({TOKEN})", s):
            sonuc = qemu_ciktisi(m[1], m[2], "-snapshot" in s.split())
            if sonuc:
                al(*sonuc, no)
    return bulgular


def python_tara(dosya, metin):
    agac = ast.parse(metin, filename=dosya)
    env = {"ROOT": ".", "KOK": ".", "BUILD": "build"}

    def deger(n):
        if isinstance(n, ast.Constant) and isinstance(n.value, (str, int)):
            return str(n.value)
        if isinstance(n, ast.Name):
            return env.get(n.id)
        if isinstance(n, ast.BinOp) and isinstance(n.op, (ast.Div, ast.Add)):
            a, b = deger(n.left), deger(n.right)
            return (
                (a + ("/" if isinstance(n.op, ast.Div) else "") + b)
                if a is not None and b is not None
                else None
            )
        if isinstance(n, ast.Call):
            if isinstance(n.func, ast.Name) and n.func.id in ("Path", "str") and n.args:
                return deger(n.args[0])
            if isinstance(n.func, ast.Attribute) and n.func.attr in ("absolute", "resolve"):
                return deger(n.func.value)
        return None

    def yazar_mi(n, konum):
        kip = (
            n.args[konum]
            if len(n.args) > konum
            else next((k.value for k in n.keywords if k.arg == "mode"), None)
        )
        return kip is not None and re.search("[wax+]", deger(kip) or "")

    soket_turleri = {}
    sonuc = set()

    def al(s, tur, n):
        if s is not None and (r := yol_kaydi(s, tur, dosya, n.lineno)):
            sonuc.add(r)

    for n in sorted(ast.walk(agac), key=lambda x: getattr(x, "lineno", 0)):
        if isinstance(n, ast.Assign):
            for ad in n.targets:
                if isinstance(ad, ast.Name):
                    if (
                        isinstance(n.value, ast.Call)
                        and isinstance(n.value.func, ast.Attribute)
                        and n.value.func.attr == "socket"
                    ):
                        soket_turleri[ad.id] = "udp" if "SOCK_DGRAM" in ast.unparse(n.value) else "tcp"
                    v = deger(n.value)
                    if v is None:
                        env.pop(ad.id, None)
                    else:
                        env[ad.id] = v
        if isinstance(n, (ast.List, ast.Tuple)):
            argv = [deger(x) for x in n.elts]
            for bayrak, arg in pairwise(argv):
                if not arg:
                    continue
                if bayrak == "-o":
                    al(arg, "ürün", n)
                elif sonuc_argv := qemu_ciktisi(bayrak, arg, "-snapshot" in argv):
                    al(*sonuc_argv, n)
        if isinstance(n, ast.Constant) and isinstance(n.value, str):
            for m in UC.finditer(n.value):
                if int(m[2]):
                    al(m[1] + ":" + m[2], "port", n)
            for m in SOKET.finditer(n.value):
                if not m[1].endswith("/"):
                    al(m[1], "uç", n)
        if not isinstance(n, ast.Call):
            continue
        f = n.func
        if isinstance(f, ast.Name) and f.id == "open" and n.args and yazar_mi(n, 1):
            al(deger(n.args[0]), "yazma", n)
        if not isinstance(f, ast.Attribute):
            continue
        if f.attr in ("write_bytes", "write_text", "touch", "mkdir", "unlink"):
            al(deger(f.value), "yazma", n)
        if f.attr == "open" and yazar_mi(n, 0):
            al(deger(f.value), "yazma", n)
        if (
            f.attr in ("copy", "copy2", "copyfile", "copytree", "move", "replace", "rename")
            and len(n.args) >= 2
            and isinstance(f.value, ast.Name)
            and f.value.id in ("shutil", "os")
        ):
            al(deger(n.args[1]), "yazma", n)
        if f.attr == "bind" and n.args and not isinstance(n.args[0], ast.Tuple):
            al(deger(n.args[0]), "uç", n)
        if f.attr == "bind" and n.args and isinstance(n.args[0], ast.Tuple) and len(n.args[0].elts) == 2:
            port = deger(n.args[0].elts[1])
            if port and port.isdigit() and int(port):
                tur = soket_turleri.get(ast.unparse(f.value))
                for protokol in [tur] if tur else ["tcp", "udp"]:
                    al(protokol + ":" + port, "port", n)
    return sonuc


def envanter(kok, makefile):
    tarifler = tarifleri_oku(kok, makefile)
    sahip = defaultdict(set)
    kaynak = {}
    bag = defaultdict(set)
    adaylar = {
        *kok.glob("test/*.sh"),
        *kok.glob("test/*.py"),
        *kok.glob("test/olcum/*.sh"),
        *kok.glob("test/olcum/*.py"),
        *kok.glob("tests/qemu/*.py"),
    }
    for p in sorted(adaylar):
        ad = p.relative_to(kok).as_posix()
        metin = p.read_text(encoding="utf-8")
        if p.suffix == ".py":
            kaynak[ad] = python_tara(ad, metin)
            for n in ast.walk(ast.parse(metin)):
                isimler = (
                    [a.name for a in n.names]
                    if isinstance(n, ast.Import)
                    else [n.module]
                    if isinstance(n, ast.ImportFrom)
                    else []
                )
                for isim in isimler:
                    if isim and (d := p.with_name(isim.split(".")[0] + ".py")) in adaylar:
                        bag[ad].add(d.relative_to(kok).as_posix())
        else:
            kaynak[ad] = kabuk(metin, ad, 1)
        bag[ad].update(
            BETIK.findall("\n".join(s for s in metin.splitlines() if not s.lstrip().startswith("#")))
        )
    erisim = defaultdict(lambda: defaultdict(set))
    for hedef, tarif in tarifler.items():
        if not tarif.komut or hedef.startswith((".", "__")):
            continue
        for r in kabuk(tarif.komut, str(makefile), tarif.satir, hedef):
            erisim[r.yol][hedef].add(r)
        aranan = [*BETIK.findall(tarif.komut)]
        aranan.extend("tests/qemu/" + ad + ".py" for ad in re.findall(r"SUREC_KAHIN=(\w+)", tarif.komut))
        goruldu = set()
        while aranan:
            ad = aranan.pop()
            if ad in goruldu or ad not in kaynak:
                continue
            goruldu.add(ad)
            sahip[ad].add(hedef)
            aranan.extend(bag[ad])
    for ad, kayitlar in kaynak.items():
        for r in kayitlar:
            for hedef in sahip[ad] or {ad}:
                erisim[r.yol][hedef].add(r)
    # Ölçümlü alt make her çalışan tarifi aynı zaman kabuğundan geçirir.
    zaman_kabugu = "test/zaman_kabuk.sh"
    zaman_yolu = kok / zaman_kabugu
    if zaman_yolu.is_file():
        satirlar = zaman_yolu.read_text(encoding="utf-8").splitlines()
        ekleme = next((i for i, s in enumerate(satirlar, 1) if '>> "$KEMGU_ZAMAN_DOSYASI"' in s), None)
        for tarif in tarifler.values():
            for takim, yol in re.findall(r"\b(\w+)\s+ZAMAN_KAYDI=(build/[\w./-]+)", tarif.komut):
                if ekleme is None:
                    raise ValueError("Ölçümlü takımın zaman yazıcısı çözülemedi")
                kayit = Erisim(yol, "ortak günlük", zaman_kabugu, ekleme)
                kilit_satiri = next(
                    (i for i, s in enumerate(satirlar, 1) if '9>> "$KEMGU_ZAMAN_DOSYASI.kilit"' in s), None
                )
                if kilit_satiri is None:
                    raise ValueError("Ölçümlü takımın süre kayıtlarında ortak kilit eksik")
                kilit = Erisim(yol + ".kilit", "kilit", zaman_kabugu, kilit_satiri)
                for hedef in onkosul_agaci(tarifler, takim):
                    if tarifler[hedef].komut:
                        erisim[yol][hedef].add(kayit)
                        erisim[kilit.yol][hedef].add(kilit)
    return erisim


def izinleri_oku(p):
    izinler = {}
    for i, s in enumerate(p.read_text(encoding="utf-8").splitlines(), 1):
        if not s or s.startswith("#"):
            continue
        yol, dosya, gerekce = s.split("\t")
        if (yol, dosya) in izinler or not gerekce.strip():
            raise ValueError(f"{p}:{i}: gerekçe eksik veya izin yinelenmiş")
        izinler[yol, dosya] = gerekce
    return izinler


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--kok", type=Path, default=KOK)
    p.add_argument("--makefile", default="Makefile")
    p.add_argument("--envanter", type=Path)
    a = p.parse_args()
    erisim = envanter(a.kok, a.makefile)
    izinler = izinleri_oku(a.kok / "test/olcum/paylasim_izinleri.tsv")
    hata = 0
    satirlar = ["kaynak\ttür\thedef\tkonum\tgerekçe"]
    for yol, sahipler in sorted(erisim.items()):
        for hedef, kayitlar in sorted(sahipler.items()):
            for r in sorted(kayitlar):
                gerekce = izinler.get((yol, r.dosya), "")
                satirlar.append(f"{yol}\t{r.tur}\t{hedef}\t{r.dosya}:{r.satir}\t{gerekce}")
        if len(sahipler) > 1 and any((yol, r.dosya) not in izinler for k in sahipler.values() for r in k):
            hata += 1
            print("ÇAKIŞMA", yol, ",".join(sorted(sahipler)))
            for r in sorted(set.union(*sahipler.values())):
                print(f"  {r.dosya}:{r.satir} ({r.tur})")
    if a.envanter:
        a.envanter.parent.mkdir(parents=True, exist_ok=True)
        a.envanter.write_text("\n".join(satirlar) + "\n", encoding="utf-8")
    print(f"Paylaşım denetimi: {len(erisim)} kaynak, {len(satirlar) - 1} sahiplik kaydı, {hata} çakışma.")
    return int(bool(hata))


if __name__ == "__main__":
    raise SystemExit(main())
