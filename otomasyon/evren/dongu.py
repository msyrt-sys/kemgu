#!/usr/bin/env python3
"""EVREN tur döngüsü — tek komutla bir turu koşar, KISA bir özet üretir.

executor.py'nin `sor`/`uygula` alt komutlarını SUBPROCESS ile çağırır (import
ETMEZ — executor'ın argparse'ı modül düzeyindedir).

  python3 otomasyon/evren/dongu.py <is_tanimi.json>

Adımlar sırayla; ilk başarısızlıkta DUR:
  1) sor           — çıkış 3 → KOTA; başka ≠0 → SOR_HATA; blok=0 → BLOK_YOK
  2) uygula --kuru — çıktıda `HATA` geçen satır varsa CAPA_HATA (UYGULAMA YAPILMAZ)
  3) uygula        — gerçek uygulama
  4) dogrulama     — her komut `bash -c`, depo kökünde; log'a EKLE; ≠0 → DOGRULAMA_HATA
  5) isaret        — log'da geçmeyen işaret → ISARET_EKSIK; hepsi varsa TAMAM

Çıktı: tur_<T>_<I>_ozet.md (durum, finish/blok, uygulanan bloklar,
`git diff --stat`, hata ayrıntısı) + stdout'a AYNI özet (≤ ~40 satır).
Çıkış kodu: TAMAM → 0, KOTA → 3, diğerleri → 1. Commit/push YOK; kapı KOŞMAZ.
Anahtar dosyasını OKUMAZ (executor okur).
"""
import json
import pathlib
import re
import subprocess
import sys

DEPO = pathlib.Path(__file__).resolve().parents[2]
EVREN = DEPO / "otomasyon" / "evren"
EXECUTOR = EVREN / "executor.py"
OZET_MAX = 40


def calistir(argv):
    """executor.py'yi subprocess ile koş; (kod, stdout, stderr) döndür."""
    p = subprocess.run([sys.executable, str(EXECUTOR)] + argv,
                       cwd=DEPO, capture_output=True, text=True, timeout=1800)
    return p.returncode, p.stdout, p.stderr


def son_ayristir(cikti):
    """Sor çıktısının son satırından (finish, blok) ayrıştır; yoksa (None, None)."""
    for satir in reversed(cikti.strip().splitlines()):
        m = re.search(r"finish=([^,]+),\s*blok=(\d+)", satir)
        if m:
            return m.group(1).strip(), int(m.group(2))
    return None, None


def git_diff_stat():
    """`git diff --stat` özeti (commit/push YOK, yalnız okuma)."""
    p = subprocess.run(["git", "diff", "--stat"], cwd=DEPO,
                       capture_output=True, text=True)
    return p.stdout.strip() or "(değişiklik yok)"


def ozet_yaz(durum, tur, is_, satirlar, ozet_yolu):
    """Özeti .md'ye yaz, stdout'a aynı içeriği (en çok OZET_MAX satır) bas."""
    ozet = [f"# Tur {tur} — {is_} — özet", "", f"- Durum: **{durum}**"]
    ozet += satirlar
    ozet += ["", "## git diff --stat", "```", git_diff_stat(), "```"]
    metin = "\n".join(ozet) + "\n"
    ozet_yolu.write_text(metin, encoding="utf-8")
    print("\n".join(metin.splitlines()[:OZET_MAX]))


def main():
    if len(sys.argv) != 2:
        print("kullanım: dongu.py <is_tanimi.json>", file=sys.stderr)
        sys.exit(1)
    tanim = json.loads(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"))
    tur = str(tanim["tur"])
    is_ = tanim["is"]
    gorev = tanim["gorev"]
    baglam = tanim.get("baglam") or []
    kesit = tanim.get("kesit") or []
    effort = tanim.get("effort")
    dogrulama = tanim.get("dogrulama") or []
    isaretler = tanim.get("isaretler") or []

    ozet_yolu = EVREN / f"tur_{tur}_{is_}_ozet.md"
    log_yolu = EVREN / f"tur_{tur}_{is_}_dogrulama.log"

    satirlar = []
    durum = None

    # 1) sor — LLM/kota; finish/blok ayrıştır.
    argv = ["sor", "--tur", tur, "--is", is_, "--gorev", gorev]
    if baglam:
        argv += ["--baglam"] + list(baglam)
    if kesit:
        argv += ["--kesit"] + list(kesit)
    if effort:
        argv += ["--effort", effort]
    kod, out, err = calistir(argv)
    finish, blok = son_ayristir(out + err)
    satirlar.append(f"- finish: `{finish}` · blok: `{blok}`")
    if kod == 3:
        durum = "KOTA"
    elif kod != 0:
        durum = "SOR_HATA"
        satirlar += ["", "## Hata (sor)", "```", (out + err).strip(), "```"]
    elif blok == 0:
        durum = "BLOK_YOK"

    # 2) uygula --kuru — captain denetimi; HATA varsa UYGULAMA YOK.
    if durum is None:
        kkod, kout, kerr = calistir(["uygula", "--tur", tur, "--is", is_, "--kuru"])
        kuru = (kout + kerr).strip()
        blok_satirlari = [s for s in kuru.splitlines()
                          if s.startswith(("YENİ", "DÜZENLE"))]
        hatalar = [s for s in kuru.splitlines() if "HATA" in s]
        if kkod != 0 or hatalar:
            durum = "CAPA_HATA"
            satirlar += ["", "## Hata (kuru)", "```",
                         "\n".join(hatalar) or kuru, "```"]
        else:
            satirlar.append("- Uygulanan bloklar (kuru):")
            satirlar += [f"  - {s}" for s in blok_satirlari]

    # 3) gerçek uygulama.
    if durum is None:
        akod, aout, aerr = calistir(["uygula", "--tur", tur, "--is", is_])
        if akod != 0:
            durum = "UYGULA_HATA"
            satirlar += ["", "## Hata (uygula)", "```", (aout + aerr).strip(), "```"]

    # 4) dogrulama komutları — sırayla; log'a ekle; ilk ≠0'da DUR.
    if durum is None:
        parcalar = []
        for komut in dogrulama:
            p = subprocess.run(["bash", "-c", komut], cwd=DEPO,
                               capture_output=True, text=True)
            parcalar.append(f"\n### $ {komut}\n{p.stdout}{p.stderr}")
            if p.returncode != 0:
                durum = "DOGRULAMA_HATA"
                son15 = "\n".join((p.stdout + p.stderr).splitlines()[-15:])
                satirlar += ["", f"## Hata (dogrulama: `{komut}`)",
                             "```", son15, "```"]
                break
        if dogrulama:
            log_yolu.write_text("".join(parcalar), encoding="utf-8")
        if durum is None:
            durum = "TAMAM"

    # 5) isaret kontrolü — log'da geçmeyen işaret eksiktir.
    if durum == "TAMAM" and isaretler:
        # Doğrulama koşmadıysa eski log OKUNMAZ (bayat işaret sahte TAMAM verirdi).
        log = log_yolu.read_text(encoding="utf-8") if (dogrulama and log_yolu.exists()) else ""
        eksik = [s for s in isaretler if s not in log]
        if eksik:
            durum = "ISARET_EKSIK"
            satirlar += ["", "## Eksik işaretler", "```", "\n".join(eksik), "```"]

    if durum is None:
        durum = "TAMAM"

    ozet_yaz(durum, tur, is_, satirlar, ozet_yolu)
    sys.exit(0 if durum == "TAMAM" else (3 if durum == "KOTA" else 1))


if __name__ == "__main__":
    main()
