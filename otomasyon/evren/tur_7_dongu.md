# Tur 7 — dongu — `deepseek-v4.1-flash`

- Tarih: 2026-10-12 09:41:11 · süre 175.1 sn · finish `stop` · reasoning_effort `low`
- usage: `{"prompt_tokens": 15344, "total_tokens": 25047, "completion_tokens": 9703, "prompt_tokens_details": {"cached_tokens": 0, "created_cache_tokens": 15296}, "completion_tokens_details": {"reasoning_tokens": 6803}}`
- Değişiklik blokları: 1 — yeni:otomasyon/evren/dongu.py
- İstem: [tur_7_dongu_istem.md](tur_7_dongu_istem.md)

## Yanıt

```yeni:otomasyon/evren/dongu.py
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
        log = log_yolu.read_text(encoding="utf-8") if log_yolu.exists() else ""
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
```

## Açıklama

- **Yeni dosya** `otomasyon/evren/dongu.py`: tek komutluk tur yürütücüsü.
- `executor.py` **subprocess** ile çağrılır (`sys.executable` + dosya yolu, `cwd=DEPO`); import edilmez, böylece modül düzeyindeki argparse çalışmaz.
- Adımlar orkestratör tâlimâtına göre sıralı; her başarısızlıkta erken durur ve `durum` kodunu özete yazar (`KOTA=3`, diğerleri `=1`, `TAMAM=0`).
- `sor` çıktısının son satırından `finish=([^,]+), blok=(\d+)` regex'iyle ayrıştırılır.
- `uygula --kuru` çıktısındaki `YENİ`/`DÜZENLE` satırları "uygulanan bloklar" olarak özete alınır; `HATA` varsa `CAPA_HATA` ile gerçek uygulama ATLANIR.
- Doğrulama komutları `bash -c`, depo kökünde; `tur_<T>_<I>_dogrulama.log` dosyasına komut başlığıyla biriktirilir (başarısız komutun son 15 satırı özete girer).
- İşaret kontrolü log içeriğinde `in` ile yapılır; eksik varsa `ISARET_EKSIK`.
- `git diff --stat` özete eklenir (salt-okuma; commit/push kuralı korunur).
- stdout özeti `OZET_MAX=40` satıra kırpılır; tam özet `.md`'ye yazılır.

VARSAYIMLAR
- `uygula --kuru` çıktısındaki uygulanan bloklar `YENİ ` ve `DÜZENLE ` ile başlayan satırlardır (executor.py'nin bu iki ön ek dışında satır yazmadığı kabul edildi).
- `dogrulama.log` her koşuda dosyanın başından yazılır (boş başlar) ve her komut sırayla "EKLE"nir; "EKLE" önceki komutların üstüne yazılmasın anlamında yorumlandı, turlar arası temizlik çağırana bırakıldı.
- Doğrulama `dogrulama` listesi boşken `isaretler` doluysa log dosyası yok → tüm işaretler eksik sayılır; bu makul bir varsayımdır (kanıt yoksa işaret yok).
- `sor` timeout'u 1800 sn (executor içi okuma zaman aşımı 300 sn olduğundan yeterli pay).

## Düşünme (reasoning)

Let me create the dongu.py file. I need to call executor.py's `sor` and `uygula` subcommands via subprocess.

Let me think through the flow:

1. Read JSON file
2. Run `sor` subcommand, capture output
3. Parse last line for `finish=` and `blok=`
4. If blok=0 → BLOK_YOK
5. Run `uygula --kuru`, check for HATA lines
6. Run `uygula` (real)
7. Run each dogrulama command, append to log
8. Check isaretler

Let me write this carefully.

The executor.py path is `otomasyon/evren/executor.py`, so path is `DEPO / "otomasyon" / "evren" / "executor.py"`.

For subprocess, use `sys.executable` as Python interpreter.

For sor output parsing - format: `tur T I: 12.3 sn, finish=stop, blok=4, usage={...}`

So parse regex: `finish=(\S+), blok=(\d+)` - but finish could be followed by comma, blok could be followed by comma.

Let me use regex: `finish=([^,]+),\s*blok=(\d+)`

For the summary file, write status, finish/blok, applied blocks (from kuru output), git diff --stat, error details.

Let me write it.

```python
#!/usr/bin/env python3
"""EVREN tur döngüsü — tek komutla bir turu koşar, KISA özet üretir.

executor.py'nin `sor`/`uygula` alt komutlarını SUBPROCESS ile çağırır (import
ETMEZ — executor'ın argparse'ı modül düzeyindedir).

  python3 otomasyon/evren/dongu.py <is_tanimi.json>

Adımlar: sor → kuru uygula (denetim) → gerçek uygula → dogrulama komutları →
isaret kontrolü. İlk başarısızlıkta DURur; durum kodunu özete yazar.

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


def ozet_yaz(taban, durum, satirlar):
    ...


def main():
    ...
```

Let me flesh out.

For the dogrulama log filename: `otomasyon/evren/tur_<T>_<I>_dogrulama.log`
Summary: `otomasyon/evren/tur_<T>_<I>_ozet.md`

Let me write the full code:

```python
#!/usr/bin/env python3
"""EVREN tur döngüsü — tek komutla bir turu koşar, KISA özet üretir.

executor.py'nin `sor`/`uygula` alt komutlarını SUBPROCESS ile çağırır (import
ETMEZ — executor'ın argparse'ı modül düzeyindedir).

  python3 otomasyon/evren/dongu.py <is_tanimi.json>

Adımlar sırayla; ilk başarısızlıkta DUR:
  1) sor        — kota/LLM; çıkış 3 → KOTA; başka ≠0 → SOR_HATA; blok=0 → BLOK_YOK
  2) uygula --kuru — çıktıda `HATA` varsa CAPA_HATA (UYGULAMA YAPILMAZ)
  3) uygula     — gerçek uygulama
  4) dogrulama  — her komut `bash -c`, depo kökünde; log'a EKLE; ≠0 → DOGRULAMA_HATA
  5) isaret kontr. — log'da geçmeyen işaret → ISARET_EKSIK; hepsi varsa TAMAM

Çıktı: tur_<T>_<I>_ozet.md (durum, finish/blok, bloklar, `git diff --stat`,
hata) + stdout'a AYNI özet. Çıkış kodu: TAMAM → 0, KOTA → 3, diğerleri → 1.
Commit/push YOK; kapı KOŞMAZ. Anahtar dosyasını OKUMAZ (executor okur).
"""
import json
import pathlib
import re
import subprocess
import sys

DEPO = pathlib.Path(__file__).resolve().parents[2]
EVREN = DEPO / "otomasyon" / "evren"
EXECUTOR = EVREN / "executor.py"


def calistir(argv):
    """executor.py'yi subprocess ile koş; (kod, stdout, stderr) döndür."""
    p = subprocess.run([sys.executable, str(EXECUTOR)] + argv,
                       cwd=DEPO, capture_output=True, text=True)
    return p.returncode, p.stdout, p.stderr


def son_parca_ayristir(cikti):
    """Son satırdan (finish, blok) ayrıştır; bulunamazsa (None, None)."""
    for satir in reversed(cikti.strip().splitlines()):
        m = re.search(r"finish=([^,]+),\s*blok=(\d+)", satir)
        if m:
            return m.group(1).strip(), int(m.group(2))
    return None, None


def git_diff_stat():
    p = subprocess.run(["git", "diff", "--stat"], cwd=DEPO,
                       capture_output=True, text=True)
    return p.stdout.strip() or "(değişiklik yok)"


def main():
    if len(sys.argv) != 2:
        print("kullanım: dongu.py <is_tanimi.json>", file=sys.stderr)
        sys.exit(1)
    tanim_yolu = pathlib.Path(sys.argv[1])
    tanim = json.loads(tanim_yolu.read_text(encoding="utf-8"))
    tur = str(tanim["tur"]); is_ = tanim["is"]; gorev = tanim["gorev"]
    baglam = tanim.get("baglam") or []
    kesit = tanim.get("kesit") or []
    effort = tanim.get("effort")
    dogrulama = tanim.get("dogrulama") or []
    isaretler = tanim.get("isaretler") or []

    taban = EVREN / f"tur_{tur}_{is_}"
    log_yolu = taban.with_name(f"tur_{tur}_{is_}_dogrulama.log")
    ozet_yolu = taban.with_name(f"tur_{tur}_{is_}_ozet.md")

    durum = "TAMAM"
    finish = None
    blok_sayisi = None
    blok_satirlari = []
    hata = ""
    dogrulama_log = ""

    # 1) sor
    argv = ["sor", "--tur", tur, "--is", is_, "--gorev", gorev]
    if baglam:
        argv += ["--baglam"] + baglam
    if kesit:
        argv += ["--kesit"] + kesit
    if effort:
        argv += ["--effort", effort]
    kod, out, err = calistir(argv)
    ...
```

Let me be careful with the dogrulama log accumulation. The spec says "EKLE" (append). So I open the file in write mode initially? Not specified. Let's just append.

Actually re-reading: "stdout+stderr'i `otomasyon/evren/tur_<T>_<I>_dogrulama.log` dosyasına EKLE (komut başlığıyla)". So append. Let's open in "a" mode. But for a fresh run this might have old content. Hmm, but the spec says EKLE. Let me just append to be literal. Actually a fresh start might be better... but spec says EKLE. I'll go with append - the orchestrator probably cleans up between runs.

Hmm, actually if we append to old content, `in isaretler` check could get false positives. But spec says EKLE explicitly, so let me follow it literally. Actually maybe I should truncate at the start and then append for each command? That's most sensible. Let me start fresh (write empty) then append each command. That's what "EKLE" typically means in the context of running multiple commands.

Actually no - "EKLE" means append, contrasting with "write" per-command. Let me truncate at the beginning (start fresh), then append each command. This satisfies both EKLE (per command append, not overwrite) and reasonable semantics.

Let me write:

```python
    # 4) dogrulama
    dogrulama_log = ""
    for komut in dogrulama:
        print(f"$ {komut}", file=...)
        p = subprocess.run(["bash", "-c", komut], cwd=DEPO, capture_output=True, text=True)
        parca = f"\n### $ {komut}\n{p.stdout}{p.stderr}"
        dogrulama_log += parca
        ...
```

Wait, I need to also write the log file. Let me accumulate and write at once, or write incrementally. Simpler: accumulate then write. But if it fails on the first command, we still want the log. The summary includes error details anyway.

Let me accumulate to a string and write once at end. Actually, better to write incrementally so partial progress is preserved. But single write at end is fine for our purpose.

Let me use the accumulate approach:

```python
    log_parcalar = []
    for komut in dogrulama:
        p = subprocess.run(["bash", "-c", komut], cwd=DEPO, capture_output=True, text=True)
        log_parcalar.append(f"\n### $ {komut}\n{p.stdout}\n{p.stderr}")
        if p.returncode != 0:
            durum = "DOGRULAMA_HATA"
            son15 = "\n".join((p.stdout + p.stderr).splitlines()[-15:])
            hata = f"komut: {komut}\n{son15}"
            break
    dogrulama_log = "".join(log_parcalar)
    if dogrulama:
        log_yolu.write_text(dogrulama_log, encoding="utf-8")
```

Then isaret kontrolü:

```python
    if durum == "TAMAM" and isaretler:
        eksik = [s for s in isaretler if s not in dogrulama_log]
        if eksik:
            durum = "ISARET_EKSIK"
            hata = "eksik işaret(ler): " + ", ".join(eksik)
```

Now for the summary. Let me build it:

```python
    ozet = []
    ozet.append(f"# Tur {tur} — {is_} — özet")
    ozet.append(f"- Durum: **{durum}**")
    ozet.append(f"- finish: `{finish}` · blok: `{blok_sayisi}`")
    if blok_satirlari:
        ozet.append("- Uygulanan bloklar (kuru):")
        for s in blok_satirlari:
            ozet.append(f"  - {s}")
    ozet.append("")
    ozet.append("## git diff --stat")
    ozet.append("```")
    ozet.append(git_diff_stat())
    ozet.append("```")
    if hata:
        ozet.append("")
        ozet.append("## Hata")
        ozet.append("```")
        ozet.append(hata)
        ozet.append("```")
    metin = "\n".join(ozet) + "\n"
    ozet_yolu.write_text(metin, encoding="utf-8")
    print(metin)
```

Wait, "en çok ~40 satır" for stdout. Let me trim to 40 lines.

Exit code: TAMAM → 0, KOTA → 3, others → 1.

Now the kuru step: "çıktıda `HATA` geçen satır varsa → durum `CAPA_HATA` (o satırları özete yaz), UYGULAMA YAPMA."

```python
    kod, out, err = calistir(["uygula", "--tur", tur, "--is", is_, "--kuru"])
    kuru_cikti = out + err
    hata_satirlari = [s for s in kuru_cikti.splitlines() if "HATA" in s]
    # also capture other lines for blok list?
    blok_satirlari = [s for s in kuru_cikti.splitlines() if s.startswith(("YENİ", "DÜZENLE", "HATA"))]
    if kod != 0 or hata_satirlari:
        durum = "CAPA_HATA"
        hata = "\n".join(hata_satirlari)
        # don't apply
    else:
        # 3) real apply
        kod2, out2, err2 = calistir(["uygula", "--tur", tur, "--is", is_])
        ...
```

If kuru had HATA, don't apply, go to summary. Let me structure with early exit function or flag.

Let me use a helper `_bitir(durum, ...)` that writes summary and exits. Actually simpler to use try/finally or just structure with conditionals and single summary point.

Let me just use goto-like pattern via function return. Actually let me structure cleanly:

```python
def main():
    ...
    # aşamalar
    sonuc = asama1(...)  # returns tuple
```

Hmm, let me just use exceptions for control flow, or a flag.

I'll use a simple pattern: set `durum` and skip remaining steps via conditionals.

Let me write the full code now.

```python
#!/usr/bin/env python3
"""EVREN tur döngüsü — tek komutla bir turu koşar, KISA özet üretir.

executor.py'nin `sor`/`uygula` alt komutlarını SUBPROCESS ile çağırır (import
ETMEZ — executor'ın argparse'ı modül düzeyindedir).

  python3 otomasyon/evren/dongu.py <is_tanimi.json>

Adımlar sırayla; ilk başarısızlıkta DUR:
  1) sor          — çıkış 3 → KOTA; başka ≠0 → SOR_HATA; blok=0 → BLOK_YOK
  2) uygula --kuru — çıktıda `HATA` varsa CAPA_HATA (UYGULAMA YAPILMAZ)
  3) uygula        — gerçek uygulama
  4) dogrulama     — her komut `bash -c`, depo kökünde; log'a EKLE; ≠0 → DOGRULAMA_HATA
  5) isaret        — log'da geçmeyen işaret → ISARET_EKSIK; hepsi varsa TAMAM

Çıktı: tur_<T>_<I>_ozet.md (durum, finish/blok, uygulanan bloklar,
`git diff --stat`, hata) + stdout'a AYNI özet. Çıkış kodu: TAMAM → 0, KOTA → 3,
diğerleri → 1. Commit/push YOK; kapı KOŞMAZ. Anahtar dosyasını OKUMAZ (executor okur).
"""
import json
import pathlib
import re
import subprocess
import sys

DEPO = pathlib.Path(__file__).resolve().parents[2]
EVREN = DEPO / "otomasyon" / "evren"
EXECUTOR = EVREN / "executor.py"


def calistir(argv):
    """executor.py'yi subprocess ile koş; (kod, stdout, stderr) döndür."""
    p = subprocess.run([sys.executable, str(EXECUTOR)] + argv,
                       cwd=DEPO, capture_output=True, text=True, timeout=600)
    return p.returncode, p.stdout, p.stderr


def son_ayristir(cikti):
    """Sor çıktısının son satırından (finish, blok) ayrıştır."""
    for satir in reversed(cikti.strip().splitlines()):
        m = re.search(r"finish=([^,]+),\s*blok=(\d+)", satir)
        if m:
            return m.group(1).strip(), int(m.group(2))
    return None, None


def git_diff_stat():
    p = subprocess.run(["git", "diff", "--stat"], cwd=DEPO,
                       capture_output=True, text=True)
    return p.stdout.strip() or "(değişiklik yok)"


def ozet_yaz(durum, taban, satirlar, ozet_yolu):
    ozet = [f"# Tur {taban[0]} — {taban[1]} — özet", "", f"- Durum: **{durum}**"]
    ozet += satirlar
    ozet += ["", "## git diff --stat", "```", git_diff_stat(), "```"]
    metin = "\n".join(ozet) + "\n"
    ozet_yolu.write_text(metin, encoding="utf-8")
    print("\n".join(metin.splitlines()[:40]))


def main():
    if len(sys.argv) != 2:
        print("kullanım: dongu.py <is_tanimi.json>", file=sys.stderr)
        sys.exit(1)
    tanim = json.loads(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"))
    tur = str(tanim["tur"]); is_ = tanim["is"]; gorev = tanim["gorev"]
    baglam = tanim.get("baglam") or []
    kesit = tanim.get("kesit") or []
    effort = tanim.get("effort")
    dogrulama = tanim.get("dogrulama") or []
    isaretler = tanim.get("isaretler") or []

    ozet_yolu = EVREN / f"tur_{tur}_{is_}_ozet.md"
    log_yolu = EVREN / f"tur_{tur}_{is_}_dogrulama.log"

    satirlar = []
    durum = None

    # 1) sor
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

    # 2) uygula --kuru
    blok_satirlari = []
    if durum is None:
        kkod, kout, kerr = calistir(["uygula", "--tur", tur, "--is", is_, "--kuru"])
        kuru = kout + kerr
        blok_satirlari = [s for s in kuru.splitlines()
                          if s.startswith(("YENİ", "DÜZENLE"))]
        hatalar = [s for s in kuru.splitlines() if "HATA" in s]
        if kkod != 0 or hatalar:
            durum = "CAPA_HATA"
            satirlar += ["", "## Hata (kuru)", "```",
                         "\n".join(hatalar) or kuru.strip(), "```"]
        else:
            satirlar.append("- Uygulanan bloklar (kuru):")
            satirlar += [f"  - {s}" for s in blok_satirlari]

    # 3) gerçek uygulama
    if durum is None:
        akod, aout, aerr = calistir(["uygula", "--tur", tur, "--is", is_])
        if akod != 0:
            durum = "UYGULA_HATA"
            satirlar += ["", "## Hata (uygula)", "```", (aout + aerr).strip(), "```"]

    # 4) dogrulama
    if durum is None:
        parcalar = []
        for komut in dogrulama:
            p = subprocess.run(["bash", "-c", komut], cwd=DEPO,
                               capture_output=True, text=True)
            parcalar.append(f"\n### $ {komut}\n{p.stdout}{p.stderr}")
            if p.returncode != 0:
                durum = "DOGRULAMA_HATA"
                son15 = "\n".join((p.stdout + p.stderr).splitlines()[-15:])
                satirlar += ["", f"## Hata (dogrulama: `{komut}`)", "```", son15, "```"]
                break
        if dogrulama:
            log_yolu.write_text("".join(parcalar), encoding="utf-8")
        if durum is None:
            durum = "TAMAM"

    # 5) isaret kontrolü
    if durum == "TAMAM" and isaretler:
        log = log_yolu.read_text(encoding="utf-8") if log_yolu.exists() else ""
        eksik = [s for s in isaretler if s not in log]
        if eksik:
            durum = "ISARET_EKSIK"
            satirlar += ["", "## Eksik işaretler", "```", "\n".join(eksik), "```"]

    if durum is None:
        durum = "TAMAM"

    ozet_yaz(durum, (tur, is_), satirlar, ozet_yolu)
    sys.exit(0 if durum == "TAMAM" else (3 if durum == "KOTA" else 1))


if __name__ == "__main__":
    main()
```

Wait, there's ambiguity: when there's no dogrulama list and isaretler is set. `isaretler` requires `log_yolu` to exist. If dogrulama is empty, log exists but empty → all markers missing → ISARET_EKSIK. That's fine.

But if dogrulama is empty, we don't write log_yolu. Then when isaretler checks, log_yolu might not exist → log = "". OK.

Hmm actually, if dogrulama empty and isaretler non-empty, ISARET_EKSIK makes sense (no evidence). Fine.

Wait - there's a subtle bug: I set `durum = "TAMAM"` inside step 4, but then step 5 checks `if durum == "TAMAM"`. But what if dogrulama empty and isaretler empty? Then durum = "TAMAM", step 5 skipped. Good.

But if dogrulama succeeds and durum set to TAMAM in step 4, and then step 5 changes to ISARET_EKSIK. Good.

Also the `if durum is None: durum = "TAMAM"` at end - when would durum still be None? Only if dogrulama loop didn't run... no wait, if no dogrulama in step 4, we still set durum="TAMAM" with the `if durum is None` check inside step 4? Let me re-check.

In step 4:
```python
if durum is None:
    parcalar = []
    for komut in dogrulama:
        ...
    if dogrulama:
        log_yolu.write_text(...)
    if durum is None:
        durum = "TAMAM"
```

Wait if dogrulama is empty, the loop doesn't run, then `if durum is None: durum = "TAMAM"`. Yes.

But wait — after the loop `durum` might be set to DOGRULAMA_HATA. Then `if durum is None` is False. OK good.

Actually the `if dogrulama: log_yolu.write_text` is after the loop but before the TAMAM assignment. Actually I should write the log regardless. Let me always write if there were any dogrulama commands run. Hmm actually let me just write if parcalar is non-empty. Or if dogrulama list is non-empty. Let me keep `if dogrulama:`.

Wait actually there's a subtlety: if durum is already not None (e.g., some earlier failure), we skip step 4. So `if durum is None` around step 4 is correct.

But if we skip step 4, durum stays non-None, and step 5 checks `durum == "TAMAM"` which is False → skipped. Good.

Now the "son 15 satır" - I need to output the last 15 lines of the failed command. Done above.

Now let me double check the summary format. Requirements say:
- durum, finish/blok, uygulanan bloklar (kuru çalıştırmanın satırları), `git diff --stat` çıktısı, hata ayrıntısı (varsa)

I have all that. Good.

Let's also handle: what if there's no blok_satirlari but durum is not CAPA_HATA (e.g., after apply)? I add "Uygulanan bloklar" only if kuru ran successfully. That's the "kuru çalıştırmanın satırları" - correct.

Hmm wait, should I add blok_satirlari to the summary even when there's an early failure? The spec says "uygulanan bloklar (kuru çalıştırmanın satırları)" — from the kuru run. If kuru didn't run, no blok lines. That's fine.

Let me also confirm the finish/blok parse. If sor failed (kod != 0), finish/blok might be None. That's fine, we list them anyway.

Actually issue: in KOTA case, the sor output says "DUR: günlük kalan token ..." and exits 3. There's no `finish=` line. So finish=None, blok=None. Fine.

Let me refine the regex to be safer: `finish=(\S+),\s*blok=(\d+)`. Using \S+ would capture the value up to comma. But there's already `,` after. Let me use `finish=([^,]+),\s*blok=(\d+)`.

Now `son_ayristir` iterates reversed lines - the format line is the last line. Good.

One more thing: timeout on subprocess for sor should be long since LLM. I set timeout=600. Maybe 900. Let me use 900 for safety. Actually executor's own timeout is 300 on read. So sor subprocess at most ~5min+. 600 is fine. Let me use 900.

Hmm actually putting timeout in generic `calistir` might be too much for kuru/uygula which are fast. But it's a max, not a min. Fine.

Let me also handle encoding. Files written with encoding="utf-8".

One concern about the print of summary: "en çok ~40 satır". I truncate to 40. Good.

Let me also think about `metin.splitlines()[:40]` — if there are fewer lines, fine.

Let me finalize. Also need to make sure comment style matches executor.py — Turkish comments, similar style. I think I've done that.

Let me review once more for any issues:

- `tanim.get("effort")` could be None → `if effort:` handles it.
- effort could be "" → falsy → skip. Fine.
- baglam/kesit lists: passed via argv.
- For subprocess with argv containing the strings, no shell involved → good.

Wait, one issue: `subprocess.run(..., timeout=900)` raises TimeoutExpired if exceeded. Should I catch? Spec doesn't mention. Let me not catch for simplicity. Actually a TimeoutExpired would crash with traceback and exit non-zero (uncaught exception → exit 1). Acceptable but messy. Let me add a try/except... actually let me leave it; spec doesn't require.

Actually, let me be a bit more careful and use no timeout or generous timeout. The executor itself has internal timeouts. Let me set 1800 (30 min) to be safe.

OK let me write final.

Actually one more: the log file. Spec says "stdout+stderr'i ... dosyasına EKLE (komut başlığıyla)". I write fresh with all commands. This is "each command's output appended to the log". OK.

Let me also make sure I trim the summary to 40 lines but writing 40 to stdout. The file has full. Good.

Final code:
