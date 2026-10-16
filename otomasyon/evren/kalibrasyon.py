#!/usr/bin/env python3
"""EVREN LLM — KEMGU Türkçe sözdizimi kalibrasyonu (salt-okunur ölçüm).

runtime/kem_heap.kem'den gerçek bir `çıplak işlev` (malloc) alır, her modele
aynı üç soruyu sorar, yanıtı ve (varsa) düşünme metnini ayrı ayrı
otomasyon/evren/kalibrasyon_<model>.md altına yazar.

Anahtar ~/.config/evren/key dosyasından okunur; hiçbir çıktıya yazılmaz.
Kullanım:  python3 otomasyon/evren/kalibrasyon.py [model ...]
"""
import json
import pathlib
import sys
import time
import urllib.error
import urllib.request

TABAN = "https://evren-llmapi.ssyz.org.tr/v1"
DEPO = pathlib.Path(__file__).resolve().parents[2]
KAYNAK = DEPO / "runtime" / "kem_heap.kem"
CIKTI = DEPO / "otomasyon" / "evren"
MODELLER = ["deepseek-v4.1-flash", "mimo-v2.6-pro"]
# Satır aralıkları 1-tabanlı ve kapsayıcı: küresel durum + malloc gövdesi.
ARALIKLAR = [(16, 19), (28, 65)]

SORULAR = """Aşağıdaki kod KEMGU adlı bir programlama dilinde yazılmıştır.

```kemgu
{kod}
```

Üç soruyu yanıtla:
1. Bu fonksiyon ne yapıyor? Adım adım açıkla.
2. `çıplak` niteleyicisi ne anlama geliyor, hangi amaca hizmet ediyor?
3. `tekkez<T>` ile `yetki<R>` arasındaki fark nedir?

Emin olmadığın noktaları açıkça belirt; bilmediğin şeyi tahminle doldurma."""


def kod_parcasi():
    satirlar = KAYNAK.read_text(encoding="utf-8").splitlines()
    parcalar = ["\n".join(satirlar[a - 1:b]) for a, b in ARALIKLAR]
    return "\n\n".join(parcalar)


def sor(anahtar, model, istem):
    govde = json.dumps({
        "model": model,
        "messages": [{"role": "user", "content": istem}],
        "max_tokens": 4096,
        "stream": False,
    }).encode("utf-8")
    istek = urllib.request.Request(
        TABAN + "/chat/completions", data=govde, method="POST",
        headers={"X-API-Key": anahtar, "Content-Type": "application/json"})
    t0 = time.monotonic()
    try:
        with urllib.request.urlopen(istek, timeout=600) as yanit:
            veri = json.load(yanit)
            kod = yanit.status
    except urllib.error.HTTPError as h:
        # Hata gövdesi anahtarı içermez ama yine de yalnız ilk 500 bayt.
        return {"http": h.code, "hata": h.read()[:500].decode("utf-8", "replace"),
                "sure": time.monotonic() - t0}
    return {"http": kod, "veri": veri, "sure": time.monotonic() - t0}


def yaz(model, istem, sonuc):
    satirlar = [f"# EVREN kalibrasyonu — `{model}`", "",
                f"- Tarih: {time.strftime('%Y-%m-%d %H:%M:%S')}",
                f"- HTTP: {sonuc['http']} · süre: {sonuc['sure']:.1f} sn",
                f"- Kaynak: `runtime/kem_heap.kem` satır "
                + ", ".join(f"{a}–{b}" for a, b in ARALIKLAR),
                "- Parametreler: `max_tokens: 4096`, `stream: false`", ""]
    if "veri" not in sonuc:
        satirlar += ["## HATA", "", "```", sonuc["hata"], "```"]
    else:
        v = sonuc["veri"]
        secim = v["choices"][0]
        mesaj = secim.get("message", {})
        satirlar.insert(5, f"- finish_reason: `{secim.get('finish_reason')}` · "
                           f"usage: `{json.dumps(v.get('usage'))}`")
        dusunme = mesaj.get("reasoning") or mesaj.get("reasoning_content")
        satirlar += ["## Yanıt", "", mesaj.get("content") or "_(boş)_", ""]
        satirlar += ["## Düşünme (reasoning)", ""]
        satirlar += [dusunme if dusunme else "_(yanıtta reasoning alanı yok)_", ""]
    satirlar += ["## Gönderilen istem", "", "````", istem, "````", ""]
    hedef = CIKTI / f"kalibrasyon_{model}.md"
    hedef.write_text("\n".join(satirlar), encoding="utf-8")
    return hedef


def main():
    anahtar = (pathlib.Path.home() / ".config/evren/key").read_text().strip()
    istem = SORULAR.format(kod=kod_parcasi())
    for model in sys.argv[1:] or MODELLER:
        sonuc = sor(anahtar, model, istem)
        hedef = yaz(model, istem, sonuc)
        print(f"{model}: HTTP {sonuc['http']} {sonuc['sure']:.1f} sn -> {hedef.relative_to(DEPO)}")


if __name__ == "__main__":
    main()
