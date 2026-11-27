"""Make tüketicilerini, harness girdilerini ve geçişli KEMGU modüllerini eşler."""

import argparse
import ast
import re
import sys
from collections import defaultdict
from functools import cache
from pathlib import Path

from make_tarifleri import onkosul_agaci, tarifleri_oku

KOK = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(KOK / "test"))
from cekirdek_modulleri import dogrudan_moduller

TABLO = KOK / "test/kapi_haritasi.tsv"
# Kabuğun ve Python'un yer tutucuları dosya deseni olarak genişletilir. Böylece
# örneğin {MODE}_qtest.kem bütün olası kipleri kapsar.
YOL = re.compile(
    r"(?<![\w/])((?:test|tests|drivers|runtime|stdlib|tools|otomasyon|belgeler)/"
    r"(?:[\w./*?-]|\$?\{\w+\}|\$\w+)+?\.(?:kem|py|sh|md))\b"
)
KOK_GLOB = re.compile(r"""Path\(['"]([\w/-]+)['"]\)\.glob\(['"]([\w.*-]+\.kem)['"]\)""")
BELGE_KAPILARI = ("calistir_belge_kapisi", "calistir_bm_x86_belge15")
SURUCU_BASLIGI = "# Kaynak içe aktarmalarından üretilen sürücü satırları"


def kaynak_grafi(kok):
    tarifler = tarifleri_oku(kok)

    @cache
    def yollar(desen):
        desen = re.sub(r"\$?\{\w+\}|\$\w+", "*", desen)
        if ".." in Path(desen).parts:
            return ()
        return tuple(
            p.relative_to(kok).as_posix() for p in kok.glob(desen) if p.is_file()
        )

    @cache
    def kaynak_adlari(metin):
        metin = "\n".join(
            s for s in metin.splitlines() if not s.lstrip().startswith("#")
        )
        desenler = set(YOL.findall(metin))
        # Bu kâhin seçiliyken ad çağıran tarifte SUREC_KAHIN olarak verilir.
        # Bütün Python dosyalarını olası girdi saymak ilgisiz kapıları da seçer.
        desenler.discard("tests/qemu/$SUREC_KAHIN.py")
        desenler.update(f"{a}/{b}" for a, b in KOK_GLOB.findall(metin))
        desenler.update(
            f"tests/qemu/{ad}.py" for ad in re.findall(r"\bSUREC_KAHIN=(\w+)", metin)
        )
        return {p for desen in desenler for p in yollar(desen)}

    @cache
    def girdiler(ad):
        p = kok / ad
        if ad.startswith("tools/kapi/"):
            # Üst testlerde geçen örnek dosya adları KEMGU derlemesine girmez.
            return set()
        if ad.endswith(".kem"):
            return {d.relative_to(kok).as_posix() for _, d in dogrudan_moduller(p, kok)}
        if ad.endswith((".sh", ".py")):
            metin = p.read_text(encoding="utf-8")
            bulunan = kaynak_adlari(metin)
            if ad.endswith(".py"):
                for n in ast.walk(ast.parse(metin)):
                    if isinstance(n, ast.ImportFrom) and n.module:
                        isimler = (n.module,)
                    elif isinstance(n, ast.Import):
                        isimler = (a.name for a in n.names)
                    else:
                        continue
                    for isim in isimler:
                        komsu = p.with_name(isim.split(".")[0] + ".py")
                        if komsu.is_file():
                            bulunan.add(komsu.relative_to(kok).as_posix())
            return bulunan
        return set()

    def yapim_girdileri(ad):
        bekleyen = [ad]
        gorulen = set()
        bulunan = set()
        while bekleyen:
            hedef = bekleyen.pop()
            if hedef in gorulen or hedef not in tarifler:
                continue
            gorulen.add(hedef)
            t = tarifler[hedef]
            bulunan.update(kaynak_adlari(t.komut))
            for on in t.onkosullar:
                if on.endswith((".kem", ".sh", ".py", ".md")) and (kok / on).is_file():
                    bulunan.add(on)
                elif on in tarifler:
                    bekleyen.append(on)
        return bulunan

    tuketici = defaultdict(set)
    # Kısmi takım yalnız test_tumu içinden seçilir; ayrı stres deneylerinin
    # yirmi tekrarını veya isteğe bağlı imaj dağıtım hedeflerini başlatmaz.
    for kapi in onkosul_agaci(tarifler, "test_tumu"):
        if not kapi.startswith("calistir_") or not tarifler[kapi].komut:
            continue
        bekleyen = list(yapim_girdileri(kapi))
        gorulen = set()
        while bekleyen:
            ad = bekleyen.pop()
            if ad in gorulen:
                continue
            gorulen.add(ad)
            tuketici[ad].add(kapi)
            bekleyen.extend(girdiler(ad) - gorulen)
    return tuketici


def surucu_kapilari(kok, tuketici):
    alanlar = {
        p.relative_to(kok).parts[1] for p in (kok / "drivers").glob("*/**/*.kem")
    }
    sonuc = {alan: set() for alan in sorted(alanlar)}
    for yol, hedefler in tuketici.items():
        bolum = Path(yol).parts
        if bolum[0] == "drivers" and len(bolum) >= 3 and bolum[1] in sonuc:
            sonuc[bolum[1]].update(hedefler)
    return sonuc


def harita_metni(alanlar, metin):
    sabit, ayir, _ = metin.partition(SURUCU_BASLIGI + "\n")
    if not ayir:
        raise ValueError("Tabloda üretilen sürücü bölümü bulunamadı")
    satirlar = [sabit.rstrip("\n"), SURUCU_BASLIGI]
    for alan, kapilar in sorted(alanlar.items()):
        satirlar.append(
            f"drivers/{alan}/*\t" + (" ".join(sorted(kapilar)) or "test_tumu")
        )
    return "\n".join(satirlar) + "\n"


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--yaz", action="store_true")
    a = p.parse_args()
    metin = harita_metni(
        surucu_kapilari(KOK, kaynak_grafi(KOK)), TABLO.read_text(encoding="utf-8")
    )
    if a.yaz:
        TABLO.write_text(metin, encoding="utf-8")
    else:
        print(metin, end="")
