#!/usr/bin/env python3
"""ABI belgesi, gerçek x86 dağıtıcı dalları ve kullanıcı sarmalayıcıları."""

import re
from pathlib import Path


def islevler(metin, desen):
    eslesen = list(re.finditer(desen, metin, re.M))
    for i, m in enumerate(eslesen):
        yield m.groups(), metin[m.end():eslesen[i + 1].start() if i + 1 < len(eslesen) else len(metin)]


def belge():
    txt = Path("belgeler/KEMGU_OS_Syscall_ABI.md").read_text(encoding="utf-8")
    out = {}
    for no, ad, adet in re.findall(r"^\| (\d+) \| (\w+) \| (\d+) \|", txt, re.M):
        if ad.startswith("eski_"):
            continue
        if int(no) in out:
            raise ValueError(f"Belgelenen numara yineleniyor: {no}")
        out[int(no)] = (ad, int(adet))
    return out


def sarma():
    metin = Path("runtime/kullanici/x86_64/kem_sis.kem").read_text(encoding="utf-8")
    out = {}
    for (ad, args), govde in islevler(metin, r"^çıplak işlev sis_(\w+)\(([^)]*)\)"):
        m = re.search(r"\bsis_cagir\((\d+),[ \t]*([^\n;]*)\);", govde)
        if m:
            no = int(m[1])
            parametreler = re.findall(r"(\w+):\s*(?:metin|tam64)", args)
            if no in out:
                raise ValueError(f"Sarmalayıcı numarası yineleniyor: {no}")
            gercek = set(re.findall(r"\b\w+\b", m[2])) & set(parametreler)
            if len(gercek) != len(parametreler):
                raise ValueError(f"Sarmalayıcı argümanı çağrıya taşınmıyor: {ad}")
            out[no] = (ad, len(gercek))
    return out


def dagitici():
    dosyalar = {
        "runtime/mimari/x86_64/kem_gorev_hw.kem": ("kdl_syscall_isle", {"arg", "arg2", "arg3"}),
        "runtime/mimari/x86_64/kem_surec_hw.kem": ("kxs_hizmet", {"a", "b", "c"}),
        "test/ornekler/kem_os_x86_vfs_ortak.kem": ("ksv_dagit", {"a", "c", "d"}),
    }
    kaynaklar = {yol: Path(yol).read_text(encoding="utf-8") for yol in dosyalar}
    sabitler = dict(re.findall(
        r"^sabit KX_SIS_(\w+): tam64 = (\d+);", "\n".join(kaynaklar.values()), re.M
    ))
    eski = {10: "tik", 11: "pid", 13: "cik", 14: "durum"}
    sec = set(eski) | {int(n) for n in sabitler.values()}
    out = {}
    for yol, (islev, args) in dosyalar.items():
        bas = re.search(rf"^(?:çıplak )?işlev {islev}\([^\n]+\{{\n", kaynaklar[yol], re.M)
        govde = kaynaklar[yol][bas.end():].split("\n}", 1)[0]
        dallar = re.findall(
            r"^    (eğer num == [^\n]+?\{[^\n]*)(\n(?:        [^\n]*\n?)*)?",
            govde, re.M
        )
        for kosul, govde in dallar:
            kullanim = set(re.findall(r"\b\w+\b", kosul + govde)) & args
            for kod in re.findall(r"num == (\d+|KX_SIS_\w+)", kosul):
                if kod.isdecimal():
                    no = int(kod)
                    if no not in eski:
                        continue
                    ad = eski[no]
                else:
                    ad = kod.removeprefix("KX_SIS_").lower()
                    no = int(sabitler[ad.upper()])
                deger = (ad, len(kullanim))
                if no in out and out[no] != deger:
                    raise ValueError(f"Dağıtıcı dalları uyuşmuyor: {no} {out[no]} {deger}")
                out[no] = deger
    if set(out) != sec:
        raise ValueError(f"Dağıtıcı dalı eksik: {sec - set(out)}")
    return out


if __name__ == "__main__":
    a, b, c = belge(), sarma(), dagitici()
    if set(range(73, 91)) - a.keys():
        raise SystemExit("Ağ ABI dizisinde eksik çağrı")
    if set(range(91, 95)) - a.keys():
        raise SystemExit("SMP ABI dizisinde eksik çağrı")
    if a != b or a != c:
        for no in sorted(a.keys() | b.keys() | c.keys()):
            d, s, k = a.get(no), b.get(no), c.get(no)
            if d != s or d != k:
                print(f"ABI {no}: belge={d}, sarmalayıcı={s}, dağıtıcı={k}")
        raise SystemExit(1)
    print(f"✓ ABI v2: {len(a)} çağrının numarası ve argüman sayısı eşleşiyor")
