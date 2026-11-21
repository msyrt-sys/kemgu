"""Geçici çekirdek kopyasında oluşan kusuru gerçek QEMU kâhinine gösterir."""
import argparse
import os
import shutil
import subprocess

from surec_sabotaj import ROOT, snapshot

CORE = "runtime/mimari/x86_64/kem_surec_hw.kem"
SHELL = "test/ornekler/kem_os_x86_kabuk.kem"
CASES = {
    "yazmac": ("hesap", "boot/start_x86_64.S", "    rep movsq\n    movq $0, kdl_dilim_etkin",
               "    rep movsq\n    movq $0, -48(%rdi)\n    movq $0, kdl_dilim_etkin", "hesap.elf pid=2 cikis=1"),
    # Çocuk girişinde özel CR3 yüklemesi atlanır. Dönülen çekirdek kökü
    # gözetmen sayfaları içerdiğinden kullanıcı girişinde #PF beklenir.
    "adres": ("cocuk", CORE, "    kxm_cr3_yaz(kxs_cr3);",
              "    eğer kxs_pid < 3 { kxm_cr3_yaz(kxs_cr3); }", "çocuklar ortak adres doğrulamasına ulaşamadı"),
    "cerceve": ("hesap", SHELL, '    eşleş serbest_ardisik(&değişken ayirici,fizik,UEFI_SUREC_CERCEVE) {\n        hata(e) => { bm_yaz(e); kem_qemu_cik(1); }\n        tamam(v) => { }\n    }',
                "", "süreç çerçevesi geri dönmedi"),
    "uyandir": ("tus-ps2", SHELL, "hazir=kem_tik_oku() >= olay;",
                "hazir=yanlış;", "QEMU sonucu gelmedi"),
}


def run(name):
    scenario, file, before, after, evidence = CASES[name]
    work = snapshot(None)
    try:
        source = work / file
        text = source.read_text()
        assert text.count(before) == 1
        source.write_text(text.replace(before, after))
        if file.endswith(".S"):
            obj = work / "build/bm_x86_start.o"
            obj.unlink()
            subprocess.run(["clang", "-target", "x86_64-none-elf", "-c", file, "-o", obj],
                           check=True, cwd=work)
        result = subprocess.run(["bash", "test/bm_x86_surec_harness.sh"], cwd=work,
                                env={**os.environ, "SUREC_DERLEYICILER": "build/kemgu",
                                     "SUREC_TEKRAR": "1", "SUREC_SENARYO": scenario},
                                capture_output=True, text=True, timeout=300)
        log = result.stdout + result.stderr
        (ROOT / "build" / f"14-sabotaj-{name}.log").write_text(log)
        if result.returncode == 0 or "AssertionError" not in log or evidence not in log:
            raise RuntimeError(f"{name}: beklenen tanı alınamadı ({evidence})")
        print(f"QEMU KIRMIZI {name}: {evidence}", flush=True)
    finally:
        shutil.rmtree(work)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sec", nargs="*", metavar="SABOTAJ")
    for name in parser.parse_args().sec or CASES:
        run(name)
