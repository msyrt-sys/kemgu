"""Seçimleri geçmiş commit'lerin kendi Makefile ve kaynak grafiğiyle karşılaştırır."""

import argparse
import shutil
import subprocess
import sys
import tarfile
from pathlib import Path

from kapi_haritasi import KOK, TABLO, kaynak_grafi
from kapi_secici import TAM, git, kurallari_oku, sec, yollar
from make_tarifleri import onkosul_agaci, tarifleri_oku

sys.path.insert(0, str(KOK / "tests/qemu"))
from calisma_dizini import calisma_dizini


def kaynaklari_ac(surum, dizin):
    arsiv = dizin / "agac.tar"
    git(KOK, "archive", "--format=tar", "-o", str(arsiv), surum)
    kok = dizin / "agac"
    with tarfile.open(arsiv) as tar:
        for uye in tar:
            if not uye.isfile():
                continue
            yol = (kok / uye.name).resolve()
            if kok not in yol.parents:
                raise ValueError(f"Geçersiz Git arşiv yolu {uye.name!r}")
            yol.parent.mkdir(parents=True, exist_ok=True)
            with tar.extractfile(uye) as kaynak, yol.open("wb") as hedef:
                shutil.copyfileobj(kaynak, hedef)
    return kok


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--surum", default="HEAD")
    p.add_argument("--adet", type=int, default=30)
    a = p.parse_args()
    bas = (
        git(KOK, "rev-parse", "--verify", "--end-of-options", a.surum + "^{commit}")
        .decode()
        .strip()
    )
    kurallar = kurallari_oku(TABLO.read_text(encoding="utf-8"))
    surumler = git(KOK, "rev-list", f"--max-count={a.adet}", bas).decode().splitlines()
    print("| Commit | Dosya | Seçim | Değişikliğe bağlı QEMU kapısı | Eksik |")
    print("|---|---:|---|---:|---:|")
    eksikler = 0
    for surum in surumler:
        ebeveyn = git(KOK, "rev-list", "--parents", "-n1", surum).decode().split()[1:]
        fark = yollar(
            git(
                KOK,
                "diff-tree",
                "--no-commit-id",
                "-r",
                "--root",
                "--no-renames",
                "--name-only",
                "-z",
                *ebeveyn[:1],
                surum,
            )
        )
        secilen, _ = sec(kurallar, fark)
        with calisma_dizini("kapi-gecmisi-") as w:
            kok = kaynaklari_ac(surum, Path(w))
            tarifler = tarifleri_oku(kok)
            takim = onkosul_agaci(tarifler, "test_tumu")
            tuketiciler = kaynak_grafi(kok)
            qemu = {
                k
                for k in takim
                if k.startswith("calistir_")
                and (
                    k.startswith(
                        (
                            "calistir_bm_",
                            "calistir_kem_os_",
                            "calistir_qemu",
                            "calistir_qtest",
                        )
                    )
                    or "qemu-system-" in tarifler[k].komut
                )
            }
            for yol, kapilar in tuketiciler.items():
                if yol.endswith((".sh", ".py")):
                    icerik = (kok / yol).read_text(encoding="utf-8")
                    if any(
                        "qemu-system-" in satir and not satir.lstrip().startswith("#")
                        for satir in icerik.splitlines()
                    ):
                        qemu.update(takim & kapilar)
            # Tabanın derleyicisi, çekirdeği veya tarifi değiştiğinde tüm QEMU yaprakları adaydır.
            ortak = any(
                y == "Makefile"
                or y.startswith(("src/", "selfhost/", "boot/", "linker/"))
                or y.startswith("runtime/")
                and y.endswith(".c")
                for y in fark
            )
            gerekli = (
                qemu
                if ortak
                else {k for yol in fark for k in tuketiciler.get(yol, ()) if k in qemu}
            )
            kapsanan = set().union(*(onkosul_agaci(tarifler, k) for k in secilen))
            eksik = gerekli - kapsanan
            eksikler += len(eksik)
        kip = "tam takım" if secilen == TAM else " ".join(sorted(secilen))
        print(
            f"| {surum[:8]} | {len(fark)} | {kip} | {len(gerekli)} | {len(eksik)} |",
            flush=True,
        )
        if eksik:
            print("Eksik kapılar:", " ".join(sorted(eksik)), file=sys.stderr)
    print(f"{len(surumler)} commit, {eksikler} eksik QEMU kapısı.")
    return bool(eksikler)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except subprocess.CalledProcessError as hata:
        sys.exit(hata.returncode)
