"""Sabit değişiklik örnekleri, gerçek kaynak kâhinleri ve bildirim tazeliği."""

import os
import stat
import sys
from pathlib import Path

from kapi_haritasi import (
    BELGE_KAPILARI,
    KOK,
    TABLO,
    harita_metni,
    kaynak_grafi,
    surucu_kapilari,
)
from kapi_secici import TAM, degisenler, git, kurallari_oku, sec
from make_tarifleri import onkosul_agaci, tarifleri_oku

sys.path.insert(0, str(KOK / "tests/qemu"))
from calisma_dizini import calisma_dizini


def dogrula(kosul, hata):
    if not kosul:
        raise ValueError(hata)


def git_ornegi():
    with calisma_dizini("secici-git-") as dizin:
        kok = Path(dizin)
        git(kok, "init", "-q")
        for ad in ("eski.md", "sil.kem", "dokunma.md"):
            (kok / ad).write_text(ad, encoding="utf-8")
        git(kok, "add", "--", ".")
        taban = git(kok, "write-tree").decode("ascii").strip()
        dogrula(not degisenler(kok, taban), "Temiz ağaçta kapı seçildi")
        git(kok, "mv", "--", "eski.md", "Türkçe belge.md")
        (kok / "sil.kem").unlink()
        (kok / "yeni dosya.xyz").write_text("yeni", encoding="utf-8")
        beklenen = {"Türkçe belge.md", "eski.md", "sil.kem", "yeni dosya.xyz"}
        dogrula(
            set(degisenler(kok, taban)) == beklenen,
            "Git ekleme, silme veya yeniden adlandırma eksik",
        )
        # [D-784] Windows'ta git nesneleri salt okunurdur ve bu dizinin temizliği WinError 5 ile
        # düşüyordu (calisma_dizini.guvenli_sil). POSIX'te aynı yolu tetiklemek için bir nesne
        # dizinini salt okunur yap; temizlik yine de dizini SİLMELİ.
        nesne = sorted(p for p in (kok / ".git" / "objects").iterdir() if p.is_dir() and len(p.name) == 2)
        dogrula(nesne, "Git nesne dizini oluşmadı")
        os.chmod(nesne[0], stat.S_IREAD | stat.S_IEXEC)
    dogrula(not Path(dizin).exists(), "Salt okunur nesneli geçici git deposu silinmedi")


def main():
    git_ornegi()
    metin = TABLO.read_text(encoding="utf-8")
    kurallar = kurallari_oku(metin)

    takim = onkosul_agaci(tarifleri_oku(KOK), "test_tumu")
    gecersiz = set().union(*kurallar.values()) - takim
    dogrula(not gecersiz, f"Tam takım dışında hedef: {sorted(gecersiz)}")

    def dene(dosyalar, beklenen, *, tam_esit=True):
        sonuc, nedenler = sec(kurallar, dosyalar)
        dogrula(
            sonuc == beklenen if tam_esit else sonuc != TAM and beklenen <= sonuc,
            f"{dosyalar}: beklenen {sorted(beklenen)}, seçilen {sorted(sonuc)}, gerekçe {nedenler}",
        )

    dene([], set())
    dene(["LOOP.md", "belgeler/KEMGU_OS_Program_Yazma.md"], set(BELGE_KAPILARI))
    dene(
        ["belgeler/KEMGU_OS_Syscall_ABI.md"],
        {"calistir_bm_x86_surec_abi"},
        tam_esit=False,
    )
    for yol in (
        "src/parser.c",
        "selfhost/check.kem",
        "runtime/mimari/kesme.c",
        "Makefile",
        "boot/start.S",
        "linker/x.ld",
        "gelecek/yeni.xyz",
    ):
        dene([yol, "LOOP.md"], TAM)
    # Sabit beklenenler gerçek kabuk, qtest, ARM ve host giriş noktalarından gelir.
    dene(["drivers/sim/kabuk.kem"], {"calistir_chatgpt_kopru"})
    for alan, kapilar in {
        "nvme": {"calistir_qtest", "calistir_bm_x86_nvme", "calistir_chatgpt_kopru"},
        "usb": {"calistir_qtest", "calistir_bm_x86_usb", "calistir_bm_x86_usb_ag"},
        "ag": {
            "calistir_kem_os_arm_ag",
            "calistir_bm_x86_ag_iki",
            "calistir_bm_x86_e1000",
        },
        "virtio": {"calistir_surucu_diff", "calistir_surucu_kosum"},
    }.items():
        dene([f"drivers/{alan}/yeni.kem"], kapilar, tam_esit=False)
    dene(
        ["LOOP.md", "drivers/sim/kabuk.kem"],
        {*BELGE_KAPILARI, "calistir_chatgpt_kopru"},
    )
    dene(["drivers/bilinmeyen/yeni.kem"], TAM)
    dogrula(
        metin == harita_metni(surucu_kapilari(KOK, kaynak_grafi(KOK)), metin),
        "Kapı haritası kaynaklarla farklı, python3 tools/kapi/kapi_haritasi.py --yaz ile yenileyin.",
    )
    print(
        "Kapı seçici geçti: belgeler, dört korunan dizin, runtime C, Makefile, beş sürücü, karma ve eşlenmeyen yollar."
    )


if __name__ == "__main__":
    main()
