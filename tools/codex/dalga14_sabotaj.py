"""Geçici çekirdek kopyasında oluşan kusuru gerçek QEMU kâhinine gösterir."""
import argparse
import os
import shutil
import subprocess

from surec_sabotaj import ROOT, snapshot

CORE = "runtime/mimari/x86_64/kem_surec_hw.kem"
SHELL = "test/ornekler/kem_os_x86_kabuk.kem"
CASES = {
    "v1-esitle": ("v1-kesinti", "test/ornekler/kem_os_x86_vfs_ortak.kem", "işlev ksv_esitle(b: &değişken KsvBaglam) -> tam64 {", "işlev ksv_esitle(b: &değişken KsvBaglam) -> tam64 { güvensiz { eğer kxs_pid == 4 { ver 0; } }", "Dirty bit is set."),
    "gecmis": ("kabuk14", "drivers/girdi/satir.kem", "d.konum = d.konum - 1;", "eğer d.konum > 1 { d.konum = d.konum - 2; } değilse { d.konum=0; }", "kabuk girdi pikselleri"),
    "ortak": ("kabuk14", SHELL, "ortak=i; }", "ortak=i; eğer i < dizi_boyut(ilk) { ortak=i+1; } }", "kabuk girdi pikselleri"),
    "tik-satir": ("gezgin", "test/kullanici/dosyalar.kem", "((sis_alan(p,4)/16) olarak tam32)-1", "((sis_alan(p,4)/16) olarak tam32)-2", "ekran farklı"),
    "ust-yol": ("gezgin", "test/kullanici/dosyalar.kem", "yol=ge_ust(g.yol);", "yol=g.yol;", "ekran farklı"),
    "utf8": ("duzenle-icerik", "drivers/duzenle/tampon.kem", "bas=du_geri(t,bas);", "eğer bas > 0 { bas=bas-1; }", "kaydedilen UTF-8 içerik farklı"),
    "birlestir": ("duzenle-icerik", "drivers/duzenle/tampon.kem", "eğer bas == son { ver; }", "eğer bas == son { ver; } eğer du_bayt(t,bas) == 10 ve son < t.boy { son=du_ileri(t,son); }", "kaydedilen UTF-8 içerik farklı"),
    "kaydet": ("duzenle", "test/kullanici/duzenle.kem", "ver sis_esitle();", "ver 0;", "Dirty bit is set."),
    "hucre": ("ekran-ps2", "test/ornekler/kem_os_x86_vfs_ortak.kem", "((c olarak tam32)+i)*8,(a olarak tam32)*16", "((a olarak tam32)+i)*8,(c olarak tam32)*16", "uygulama pikselleri"),
    "kenar": ("ekran-usb", "test/ornekler/kem_os_x86_vfs_ortak.kem", "eğer ksv_fare_x < 0 { ksv_fare_x=0; }", "", "ekranden.elf pid=2 cikis=5"),
    "kabuk-ciz": ("ekran-ps2", SHELL, "                ksv_kabugu_ciz();", "", "kabuk ekranı farklı"),
    "yazmac": ("hesap", "boot/start_x86_64.S", "    rep movsq\n    movq $0, kdl_dilim_etkin",
               "    rep movsq\n    movq $0, -48(%rdi)\n    movq $0, kdl_dilim_etkin", "hesap.elf pid=2 cikis=1"),
    # Çocuk girişinde özel CR3 yüklemesi atlanır. Dönülen çekirdek kökü
    # gözetmen sayfaları içerdiğinden kullanıcı girişinde #PF beklenir.
    "adres": ("cocuk", CORE, "    kxm_cr3_yaz(kxs_cr3);",
              "    eğer kxs_pid < 3 { kxm_cr3_yaz(kxs_cr3); }", "çocuklar ortak adres doğrulamasına ulaşamadı"),
    "cerceve": ("hesap", SHELL, '    eşleş serbest_ardisik(&değişken ayirici,fizik,kxs_cerceve_sayisi(yer)) {\n        hata(e) => { bm_yaz(e); kem_qemu_cik(1); }\n        tamam(v) => { }\n    }',
                "", "süreç çerçevesi geri dönmedi"),
    "ofset": ("buyukelf", SHELL, "ks_parca_kopyala(b,q.bas,q.boy,hedef)", "ks_parca_kopyala(b,q.bas,q.boy,hedef+1)", "buyukelf.elf pid=2 cikis=11"),
    "bss": ("buyukelf", CORE, "kxs_esle(taban,yer,s.sanal_adres,s.sanal_adres+s.bellek_boyu,izin);",
            "değişken son: tam64 = s.bellek_boyu; eğer son > 1048576 { son=1048576; } kxs_esle(taban,yer,s.sanal_adres,s.sanal_adres+son,izin);", "SONLANDIRILDI: #PF"),
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
