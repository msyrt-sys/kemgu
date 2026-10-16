#!/usr/bin/env python3
"""EVREN LLM — KEMGU kalibrasyonu 2: bağlamlı ÜRETİM testi (salt-okunur ölçüm).

Kalibrasyon 1 (kalibrasyon.py) bağlamsız ezber sorduğu için GEÇERSİZ sayıldı.
Bu test modele kem_heap.kem'in tamamını + çıplak/tekkez/yetki/bölge-prologu
bağlamını verir ve bir x86_64 port-I/O UART yazma işlevi ÜRETMESİNİ ister.
Karar modelin yorumuna değil DERLEYİCİ ÇIKTISINA dayanır:
  1. `--mimari x86_64` ile derleniyor mu?
  2. Üretilen çıplak işlevin imzasında ρ (`ptr %rho`) YOK mu?
  3. Gövdesinde `kdl_bolge_olustur` ÇAĞRISI yok mu? (declare her modülde var)
  4. `satıriçi_asm` `mimari: x86_64` etiketli ve nesne kodda gerçek `out`/`in` var mı?

Anahtar ~/.config/evren/key dosyasından okunur; hiçbir çıktıya yazılmaz.
Kullanım:  python3 otomasyon/evren/kalibrasyon2.py [model ...]
"""
import concurrent.futures
import json
import pathlib
import re
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request

TABAN = "https://evren-llmapi.ssyz.org.tr/v1"
DEPO = pathlib.Path(__file__).resolve().parents[2]
CIKTI = DEPO / "otomasyon" / "evren"
KEMGU = DEPO / "build" / "kemgu"
MODELLER = ["glm-5.3", "mimo-v2.6-pro", "deepseek-v4.1-flash"]
MAX_TOKENS = 16384


def satirlar(yol, a, b):
    t = (DEPO / yol).read_text(encoding="utf-8").splitlines()
    return "\n".join(t[a - 1:b])


def baglam():
    heap = (DEPO / "runtime/kem_heap.kem").read_text(encoding="utf-8")
    return f"""## 1. `runtime/kem_heap.kem` (tamamı — desen kaynağı)

```kemgu
{heap}
```

## 2. `çıplak` ve bölge prologu

`docs/USERLAND_ROADMAP.md`:
> {satirlar("docs/USERLAND_ROADMAP.md", 33, 37).replace(chr(10), chr(10) + "> ")}

Derleyici tanısı E013 (`src/tip_kontrol.c`): "çıplak işlev yalnız çıplak/extern
çağırabilir (ρ-suz C-ABI)".

Derleyici çıktısı (ölçüldü, `build/kemgu --llvm`):

```kemgu
çıplak işlev f(n: tam64) -> tam64 {{ değişken p: *tam64 = n olarak *tam64; ver *p; }}
işlev g(n: tam64) -> tam64 {{ ver n; }}
```
```llvm
define i64 @f(i64 %n) {{          ; çıplak: ρ parametresi YOK, bölge prologu YOK
  ...
  %5 = load volatile i64, ptr %4 ; çıplak gövdede ham deref volatile
define i64 @g(ptr %rho, i64 %n) {{ ; normal işlev: ρ (bölge yetkisi) ilk parametre
  %1 = call ptr @kdl_bolge_olustur()   ; normal işlevin prologu bölge kurar (→ malloc)
```
Bu yüzden `malloc` çıplak olmak ZORUNDADIR: normal olsaydı prologu
`kdl_bolge_olustur` → `malloc` → ... diye kendini tetiklerdi.

## 3. `tekkez<T>` (`belgeler/KEMGU_Linear_Types_Spec_V1.md`)

{satirlar("belgeler/KEMGU_Linear_Types_Spec_V1.md", 29, 56)}

## 4. `yetki<R>` (`belgeler/KEMGU_Capability_Spec_V1.md`)

{satirlar("belgeler/KEMGU_Capability_Spec_V1.md", 86, 128)}

## 5. `satıriçi_asm` sözdizimi — depodaki çalışan örnekler

x86_64, girdi/çıktı kısıtlı (`test/cg_korpus/cg_satirici_asm.kem`):
```kemgu
{satirlar("test/cg_korpus/cg_satirici_asm.kem", 4, 21)}
```

arm64, çıplak işlev içinde (`runtime/kem_virtio_blk.kem`):
```kemgu
{satirlar("runtime/kem_virtio_blk.kem", 62, 67)}
```
Kısıt dizgileri LLVM inline-asm kısıtlarıdır (`"r"`, `"=r"`, `"{{ax}}"` gibi belirli
yazmaç, `"~{{memory}}"`). Şablon AT&T sözdizimidir; `$0`, `$1` operand sırası
çıktılar → girdiler biçimindedir."""


GOREV = """# Görev

Yukarıdaki bağlamı kullanarak, `kem_heap.kem`'deki desene birebir uyarak, x86_64
için port-mapped I/O (`out`/`in`) kullanan bir `çıplak` UART yazma fonksiyonu yaz.
`0x09000000` PL011 MMIO yolunu (ve `kdl_mmio_yaz32`) kullanma. `satıriçi_asm`
bloğunu doğru mimari etiketiyle yaz.

Yanıtında derlenebilir KEMGU kodunu TEK bir ```kemgu kod bloğu içinde ver
(yalnız senin yazdığın işlev(ler); kem_heap.kem'i tekrar etme). Kısa bir
açıklama ekleyebilirsin."""


def sor(anahtar, model, istem):
    govde = json.dumps({
        "model": model,
        "messages": [{"role": "user", "content": istem}],
        "max_tokens": MAX_TOKENS,
        "stream": False,
    }).encode("utf-8")
    istek = urllib.request.Request(
        TABAN + "/chat/completions", data=govde, method="POST",
        headers={"X-API-Key": anahtar, "Content-Type": "application/json"})
    t0 = time.monotonic()
    try:
        with urllib.request.urlopen(istek, timeout=1200) as yanit:
            return {"http": yanit.status, "veri": json.load(yanit),
                    "sure": time.monotonic() - t0}
    except urllib.error.HTTPError as h:
        return {"http": h.code, "hata": h.read()[:500].decode("utf-8", "replace"),
                "sure": time.monotonic() - t0}
    except Exception as e:  # zaman aşımı / ağ — gürültülü bildir
        return {"http": None, "hata": f"{type(e).__name__}: {e}",
                "sure": time.monotonic() - t0}


def kos(argv, girdi=None):
    r = subprocess.run(argv, input=girdi, capture_output=True, text=True)
    return r.returncode, r.stdout, r.stderr


def islev_govdesi(ir, ad):
    m = re.search(r"^define [^\n]*@" + re.escape(ad) + r"\(.*?^}", ir, re.M | re.S)
    return m.group(0) if m else None


def degerlendir(kod):
    """Dört ölçütü derleyici + assembler çıktısından hesaplar."""
    s = {"olcut": {}, "not": [], "derleme_kipi": None}
    ciplak = re.findall(r"çıplak\s+işlev\s+([A-Za-z_][\wçğıöşüÇĞİÖŞÜ]*)", kod)
    # 2./3. ölçüt modelin tanımladığı TÜM işlevlere uygulanır (çıplak olmayan bir
    # yardımcı da ρ alır ve bölge kurar → ölçüt onu da yakalamalı).
    tum = re.findall(r"(?:^|\s)işlev\s+([A-Za-z_][\wçğıöşüÇĞİÖŞÜ]*)", kod)
    s["ciplak"] = ciplak
    with tempfile.TemporaryDirectory(dir="/tmp/claude-1000") as td:
        td = pathlib.Path(td)
        tek = td / "uret.kem"
        tek.write_text(kod, encoding="utf-8")
        rc, ir, err = kos([str(KEMGU), "--llvm", "--mimari", "x86_64", str(tek)])
        s["derleme_kipi"] = "tek başına"
        if rc != 0 and "T002" in err:
            # Model kem_heap işlevlerine dayanıyorsa kem_os gibi CAT ile dene.
            birlesik = td / "birlesik.kem"
            birlesik.write_text((DEPO / "runtime/kem_heap.kem").read_text(encoding="utf-8")
                                + "\n" + kod, encoding="utf-8")
            rc2, ir2, err2 = kos([str(KEMGU), "--llvm", "--mimari", "x86_64", str(birlesik)])
            s["not"].append(f"tek başına derleme T002 verdi; kem_heap.kem ile CAT denendi (rc={rc2})")
            if rc2 == 0:
                rc, ir, err, s["derleme_kipi"] = rc2, ir2, err2, "kem_heap.kem ile CAT"
        s["derleme_rc"], s["derleme_err"] = rc, err.strip()
        s["olcut"]["1_derleniyor"] = rc == 0
        govdeler = {ad: islev_govdesi(ir, ad) for ad in tum} if rc == 0 else {}
        s["govdeler"] = govdeler
        if not ciplak:
            s["not"].append("kodda `çıplak işlev` YOK")
        imza_ok = rc == 0 and bool(ciplak) and all(
            g is not None and "%rho" not in g.splitlines()[0] for g in govdeler.values())
        s["olcut"]["2_imza_rho_yok"] = imza_ok
        prolog_ok = rc == 0 and bool(ciplak) and all(
            g is not None and not re.search(r"call [^\n]*@kdl_bolge_olustur", g)
            for g in govdeler.values())
        s["olcut"]["3_bolge_cagrisi_yok"] = prolog_ok
        etiket_ok = bool(re.search(r"satıriçi_asm\s*\{[^}]*mimari:\s*x86_64", kod))
        if not etiket_ok:
            s["not"].append("`satıriçi_asm` bloğu `mimari: x86_64` etiketli değil (ya da blok yok)")
        # Yorumlar ayıklanır: model PL011'i yalnız ANIYOR olabilir (kalibrasyon 2'de
        # deepseek'in `// … kdl_mmio_yaz32 …` yorumu sahte alarm verdi).
        yorumsuz = re.sub(r"//[^\n]*", "", kod)
        pl011 = bool(re.search(r"150994944|0x0*9000000|kdl_mmio_yaz32", yorumsuz))
        if pl011:
            s["not"].append("kod PL011 / kdl_mmio_yaz32 yolunu KULLANIYOR (yasaklanmıştı)")
        komutlar = []
        if rc == 0:
            ll = td / "uret.ll"
            ll.write_text(ir, encoding="utf-8")
            if s["derleme_kipi"] != "tek başına":
                # kem_os ile aynı son-işlem: tanımlı sembolün declare'ini sil (Makefile).
                ham = td / "ham.ll"
                ham.write_text(ir, encoding="utf-8")
                _, temiz, _ = kos(["awk", "-f", str(DEPO / "test/strip_defined_declares.awk"), str(ham)])
                ll.write_text(temiz, encoding="utf-8")
            obj = td / "uret.o"
            rca, _, erra = kos(["clang", "-target", "x86_64-unknown-none", "-ffreestanding",
                                "-Wno-override-module", "-O2", "-x", "ir", str(ll), "-c",
                                "-o", str(obj)])
            if rca != 0:
                s["not"].append("x86_64 assembler/codegen REDDETTİ: " + erra.strip()[:400])
            else:
                _, dis, _ = kos(["llvm-objdump", "-d", "--no-show-raw-insn", str(obj)])
                komutlar = [l.strip() for l in dis.splitlines()
                            if re.search(r"\t(out|in)[bwl]?\s", l)]
        s["io_komutlari"] = komutlar
        s["olcut"]["4_asm_x86_out_in"] = etiket_ok and bool(komutlar)
        s["ir"] = ir if rc == 0 else ""
    s["gecti"] = all(s["olcut"].values())
    return s


def kod_bloku(metin):
    m = re.search(r"```kemgu\s*\n(.*?)```", metin or "", re.S)
    if not m:
        m = re.search(r"```[a-zA-Z]*\s*\n(.*?)```", metin or "", re.S)
    return m.group(1) if m else None


def karar_satirlari(kod):
    """KARAR bölümü (tablo + notlar + IR + objdump + değerlendirilen kod)."""
    s = []
    d = degerlendir(kod)
    gecti = d["gecti"]
    isim = {"1_derleniyor": "1. `--mimari x86_64` ile derleniyor",
            "2_imza_rho_yok": "2. çıplak işlev imzasında ρ (`ptr %rho`) yok",
            "3_bolge_cagrisi_yok": "3. gövdede `kdl_bolge_olustur` çağrısı yok",
            "4_asm_x86_out_in": "4. `mimari: x86_64` etiketli asm + nesne kodda gerçek `out`/`in`"}
    s += [f"## KARAR: {'GEÇTİ' if gecti else 'KALDI'}", "",
          "| Ölçüt (derleyici çıktısından) | Sonuç |", "|---|---|"]
    s += [f"| {isim[k]} | {'✅' if ok else '❌'} |" for k, ok in d["olcut"].items()]
    s += ["", f"- Çıplak işlevler: {', '.join('`'+a+'`' for a in d['ciplak']) or '—'}",
          f"- Derleme kipi: {d['derleme_kipi']} · rc={d['derleme_rc']}"]
    s += [f"- Not: {n}" for n in d["not"]]
    if d["derleme_err"]:
        s += ["", "### Derleyici tanısı", "", "```", d["derleme_err"][:3000], "```"]
    if d["govdeler"]:
        s += ["", "### Üretilen IR (çıplak işlev gövdeleri)", "", "```llvm"]
        s += [g if g else f"; @{a} IR'da bulunamadı" for a, g in d["govdeler"].items()]
        s += ["```"]
    s += ["", "### Nesne koddaki port-I/O komutları (llvm-objdump)", "", "```",
          "\n".join(d["io_komutlari"]) or "(yok)", "```", ""]
    s += ["### Değerlendirilen kod", "", "```kemgu", kod.rstrip(), "```", ""]
    return s, gecti


def yaz(model, sonuc):
    s = [f"# EVREN kalibrasyonu 2 (bağlamlı üretim) — `{model}`", "",
         f"- Tarih: {time.strftime('%Y-%m-%d %H:%M:%S')}",
         f"- HTTP: {sonuc['http']} · süre: {sonuc['sure']:.1f} sn",
         f"- Parametreler: `max_tokens: {MAX_TOKENS}`, `stream: false`",
         "- İstem: [kalibrasyon2_istem.md](kalibrasyon2_istem.md)", ""]
    if "veri" not in sonuc:
        s += ["## KARAR: KALDI — istek başarısız", "", "```", str(sonuc["hata"]), "```", ""]
        return s, False
    v = sonuc["veri"]
    secim = v["choices"][0]
    mesaj = secim.get("message", {})
    icerik = mesaj.get("content") or ""
    dusunme = mesaj.get("reasoning") or mesaj.get("reasoning_content")
    s.insert(5, f"- finish_reason: `{secim.get('finish_reason')}` · usage: `{json.dumps(v.get('usage'))}`")
    kod = kod_bloku(icerik)
    if kod is None:
        s += ["## KARAR: KALDI — yanıtta kod bloğu yok", ""]
        gecti = False
    else:
        karar, gecti = karar_satirlari(kod)
        s += karar
    s += ["## Yanıt (ham)", "", icerik or "_(boş)_", "",
          "## Düşünme (reasoning)", "", dusunme or "_(yanıtta reasoning alanı yok)_", ""]
    return s, gecti


def yeniden_degerlendir():
    """API'ye gitmeden kayıtlı yanıtların KARAR bölümünü yeniden üretir
    (değerlendirici düzeltildiğinde). Ham yanıt ve düşünme metni korunur."""
    for md in sorted(CIKTI.glob("kalibrasyon2_*.md")):
        if md.name == "kalibrasyon2_istem.md":
            continue
        t = md.read_text(encoding="utf-8")
        m = re.search(r"### Değerlendirilen kod\n\n```kemgu\n(.*?)\n```", t, re.S)
        if not m or "## KARAR" not in t:
            print(f"{md.name}: değerlendirilecek kod yok, atlandı")
            continue
        bas, son = t.index("## KARAR"), t.index("## Yanıt (ham)")
        karar, gecti = karar_satirlari(m.group(1) + "\n")
        md.write_text(t[:bas] + "\n".join(karar) + t[son:], encoding="utf-8")
        print(f"{md.name}: {'GEÇTİ' if gecti else 'KALDI'} (yeniden değerlendirildi)")


def main():
    if sys.argv[1:] == ["--yeniden"]:
        return yeniden_degerlendir()
    anahtar = (pathlib.Path.home() / ".config/evren/key").read_text().strip()
    istem = baglam() + "\n\n" + GOREV
    (CIKTI / "kalibrasyon2_istem.md").write_text(
        "# Kalibrasyon 2 — gönderilen istem (tüm modellere aynı)\n\n````\n" + istem + "\n````\n",
        encoding="utf-8")
    modeller = sys.argv[1:] or MODELLER
    with concurrent.futures.ThreadPoolExecutor(len(modeller)) as ex:
        isler = {ex.submit(sor, anahtar, m, istem): m for m in modeller}
        for f in concurrent.futures.as_completed(isler):
            m = isler[f]
            sonuc = f.result()
            satir, gecti = yaz(m, sonuc)
            hedef = CIKTI / f"kalibrasyon2_{m}.md"
            hedef.write_text("\n".join(satir), encoding="utf-8")
            print(f"{m}: HTTP {sonuc['http']} {sonuc['sure']:.1f} sn -> "
                  f"{'GEÇTİ' if gecti else 'KALDI'} ({hedef.relative_to(DEPO)})", flush=True)


if __name__ == "__main__":
    main()
