#!/usr/bin/env python3
"""Codex dalga commit'lerinin bağımsız incelemesi (EVREN).

Her bölüm commit'i için: promptun o bölümü + izin ve ilke bölümleri + Codex'in
ilerleme notu + commit diff'i modele verilir; model somut, satır referanslı
bulgu listesi üretir. Model DOSYAYA DOKUNMAZ; bulguları Claude doğrular.

Kullanım:
  python3 otomasyon/evren/inceleme.py <worktree> <prompt.md> <ilerleme.md> <cikti_dizini> [taban]
Kota: her çalıştırmadan önce GET /quota; günlük kalan < 2M → çıkış 3 (DUR).
"""
import concurrent.futures as cf
import os
import pathlib
import re
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import executor  # noqa: E402  (akisli_sor, kota, KOTA_ALT)

DIFF_SINIR = 150_000   # karakter; aşılırsa diff kırpılır ve bunu modele söyleriz
ESZAMANLI = 4
EFOR = "low"
executor.MAX_TOKENS = 32768


def git(wt, *a):
    return subprocess.run(["git", "-C", wt, *a], capture_output=True, text=True,
                          encoding="utf-8", errors="replace", check=True).stdout


def bolum_metni(prompt, n):
    m = re.search(rf"^### Bölüm {n} —.*?(?=^### Bölüm {n + 1} —|^---|\Z)", prompt, re.M | re.S)
    return m.group(0) if m else ""


def genel_metin(prompt):
    parca = []
    for baslik in ("## 4. İzin genişletmesi", "## 5. Tasarım ilkeleri", "## 7. Kapılar"):
        m = re.search(rf"^{re.escape(baslik)}.*?(?=^## |\Z)", prompt, re.M | re.S)
        if m:
            parca.append(m.group(0))
    return "\n".join(parca)


def ilerleme_notu(ilerleme, n):
    m = re.search(rf"^#+ .*Bölüm {n}\b.*?(?=^#+ .*Bölüm {n + 1}\b|\Z)", ilerleme, re.M | re.S)
    return m.group(0)[:20_000] if m else "(ilerleme belgesinde bu bölüm için ayrı not bulunamadı)"


SISTEM = """Sen bir kod incelemecisisin. KEMGU adlı Türkçe sözdizimli bir sistem dili ve onunla
yazılmış bir işletim sistemi (KEMGU-OS) üzerinde başka bir yapay zekâ ajanının (Codex) yaptığı
bir commit'i inceliyorsun. Görevin, commit'in görev tanımındaki gereksinimleri gerçekten karşılayıp
karşılamadığını ve somut kusur içerip içermediğini bulmak.

Özellikle şunlara bak:
1. GEREKSİNİM EKSİK: görev metnindeki bir madde (kapı, kâhin, sabotaj, belge) diff'te yok ya da
   yalnız iddia edilmiş.
2. SABOTAJ: istenen sabotaj uygulanmamış, başka/zayıf bir sabotajla değiştirilmiş ya da kapı onu
   ayırt edemeyecek biçimde kurulmuş.
3. KÂHİN ZAYIFLIĞI: kapı yalnız konuğun kendi raporuna güveniyor (host bağımsız doğrulamıyor),
   beklenen değer konuk çıktısından türetiliyor, ya da kapı her durumda geçebilir.
4. SINIRSIZ BEKLEME / SESSİZ HATA: zaman aşımı olmayan donanım döngüsü, hatayı yutup devam eden yol,
   sessiz kısaltma.
5. İZİN DIŞI DOSYA: görevde yasaklanan bir dosyaya dokunulmuş.
6. MANTIK HATASI: yanlış bit/ofset/sabit, sınır hatası, ters koşul.

Kurallar:
- YALNIZ diff'te gördüğün şeye dayan. Her bulgu için dosya yolu ve diff'teki satırı/alıntıyı ver.
- Emin olmadığın şeyi "ŞÜPHE" olarak işaretle, "KUSUR" deme.
- Genel tavsiye, üslup yorumu, övgü YAZMA.
- Bulgu yoksa yalnız "BULGU YOK" yaz.

Çıktı biçimi (Markdown):
## Bulgular
- [KUSUR|ŞÜPHE|EKSİK] `dosya:satır` — tek cümlelik açıklama. Alıntı: `...`
## Karşılanan gereksinimler
- madde → kanıt (dosya)
"""


def incele(wt, prompt, ilerleme, n, sha, anahtar, cikti):
    stat = git(wt, "show", "--stat", "--format=%H%n%s", sha)
    diff = git(wt, "show", "--format=", sha)
    kirpildi = ""
    if len(diff) > DIFF_SINIR:
        kirpildi = f"\n\n[NOT: diff {len(diff)} karakter; ilk {DIFF_SINIR} karakter verildi.]"
        diff = diff[:DIFF_SINIR]
    kullanici = (f"# Görev tanımı — Bölüm {n}\n{bolum_metni(prompt, n)}\n\n"
                 f"# Genel kurallar (izinler, ilkeler, kapılar)\n{genel_metin(prompt)}\n\n"
                 f"# Codex'in bu bölüm için ilerleme notu (İDDİA — doğrulanmamış)\n{ilerleme_notu(ilerleme, n)}\n\n"
                 f"# Commit özeti\n```\n{stat}\n```\n\n# Diff\n```diff\n{diff}\n```{kirpildi}")
    mesajlar = [{"role": "system", "content": SISTEM}, {"role": "user", "content": kullanici}]
    # [2026-11-24] effort=None iken 16k bütçenin TAMAMI düşünmeye gitti (içerik boş,
    # bitiş=length; kalibrasyon2'deki artefaktın aynısı) → düşük çaba + geniş bütçe.
    try:
        icerik, _, usage, bitis, sure = executor.akisli_sor(anahtar, mesajlar, effort=EFOR)
    except Exception as e:   # 503 vb.: yedek modelle bir kez
        icerik, _, usage, bitis, sure = executor.akisli_sor(anahtar, mesajlar, effort=EFOR,
                                                            model=executor.YEDEK_MODEL)
        icerik = f"(birincil model hata verdi: {type(e).__name__}; yedek model)\n\n" + icerik
    yol = pathlib.Path(cikti) / f"bolum_{n:02d}.md"
    yol.write_text(f"# Bölüm {n} — EVREN incelemesi\n\n- commit: `{sha}`\n- model: {executor.MODEL}"
                   f" · bitiş: {bitis} · süre: {sure:.0f} sn · usage: {usage}\n\n{icerik}\n",
                   encoding="utf-8")
    return n, bitis, (usage or {}).get("total_tokens")


def main():
    wt, prompt_yol, ilerleme_yol, cikti = sys.argv[1:5]
    taban = sys.argv[5] if len(sys.argv) > 5 else "origin/main"
    anahtar = pathlib.Path.home().joinpath(".config/evren/key").read_text().strip()
    q = executor.kota(anahtar)
    kalan = q.get("daily_remaining_tokens")
    print(f"kota: günlük kalan {kalan}")
    if kalan is not None and kalan < executor.KOTA_ALT:
        print("DUR: günlük kalan kota 2M altında"); sys.exit(3)
    prompt = pathlib.Path(prompt_yol).read_text(encoding="utf-8")
    ilerleme = pathlib.Path(ilerleme_yol).read_text(encoding="utf-8")
    pathlib.Path(cikti).mkdir(parents=True, exist_ok=True)
    isler = []
    for satir in git(wt, "log", "--reverse", "--format=%H %s", f"{taban}..HEAD").splitlines():
        sha, konu = satir.split(" ", 1)
        m = re.search(r"bölüm (\d+)", konu)
        sec = os.environ.get("EVREN_BOLUMLER")   # ör. "1,2,12" — yalnız bunlar
        if m and (not sec or m.group(1) in sec.split(",")):
            isler.append((int(m.group(1)), sha))
    with cf.ThreadPoolExecutor(ESZAMANLI) as h:
        for n, bitis, tok in h.map(lambda a: incele(wt, prompt, ilerleme, a[0], a[1], anahtar, cikti), isler):
            print(f"bölüm {n}: bitiş={bitis} token={tok}")


if __name__ == "__main__":
    main()
