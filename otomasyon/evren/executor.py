#!/usr/bin/env python3
"""EVREN executor — x86 parite döngüsünde mekanik üretimi deepseek'e devreder.

Orkestratör (Claude) bağlamı ve görevi verir; model DEĞİŞİKLİK BLOKLARI üretir;
bloklar ORKESTRATÖR DENETLEMEDEN UYGULANMAZ (`uygula` ayrı alt komut).

  executor.py sor    --tur N --is AD --gorev gorev.md [--baglam dosya ...]
  executor.py uygula --tur N --is AD [--kuru]

Önbellek: SISTEM ve ORTAK_BAGLAM turlar arası SABİT; işe özel bağlam onlardan
SONRA gelir (önek eşleşmesi korunur).
Kota: her turdan önce GET /v1/quota; günlük kalan < 2M → çıkış 3 (DUR).
Anahtar ~/.config/evren/key'den okunur; hiçbir çıktıya yazılmaz.
"""
import argparse
import json
import pathlib
import re
import sys
import time
import urllib.request

TABAN = "https://evren-llmapi.ssyz.org.tr/v1"
MODEL = "deepseek-v4.1-flash"
MAX_TOKENS = 16384
KOTA_ALT = 2_000_000
DEPO = pathlib.Path(__file__).resolve().parents[2]
CIKTI = DEPO / "otomasyon" / "evren"

SISTEM = """Sen KEMGU deposunda mekanik kod üreten bir yürütücüsün. KEMGU, Türkçe
sözdizimli bir sistem programlama dili ve onunla yazılmış bir işletim sistemidir.
Kararları orkestratör verir; sen yalnız istenen değişikliği üretirsin.

KESİN KURALLAR:
- Yalnız görevde adı geçen dosyalara dokun. `src/*.c`, `selfhost/*.kem`,
  `runtime/kem_gorev.kem`, `runtime/kem_mmu.kem`, `runtime/kem_zaman.kem`
  YASAK.
- Bağlamdaki desene BİREBİR uy (adlandırma, yorum dili Türkçe, `çıplak işlev`).
- Emin olmadığın bir şeyi uydurma; varsayımını yanıtın sonunda "VARSAYIMLAR"
  başlığı altında listele.

ÇIKTI BİÇİMİ (zorunlu, başka biçim kabul edilmez):
Var olan dosyada değişiklik — ESKI metin dosyada AYNEN ve TEK kez geçmeli:
```duzenle:<depo-göreli yol>
<<<<<<< ESKI
<değişecek metin, boşluklar dahil birebir>
=======
<yeni metin>
>>>>>>> YENI
```
Yeni dosya:
```yeni:<depo-göreli yol>
<dosyanın tam içeriği>
```
Birden çok blok verebilirsin. Bloklardan sonra kısa bir açıklama yaz."""


def oku(yol):
    return (DEPO / yol).read_text(encoding="utf-8")


def ortak_baglam():
    """Turlar arası SABİT bağlam başı (önbellek öneki)."""
    return f"""# ORTAK BAĞLAM (her turda aynı)

## KEMGU çekirdek kuralları (derleyiciden ölçülmüş)
- Normal işlev ilk parametre olarak bölge yetkisi `ptr %rho` alır ve prologunda
  `call ptr @kdl_bolge_olustur()` yapar. `çıplak işlev` ise C-ABI'dir:
  `define i64 @f(i64 %n)` — ρ YOK, bölge prologu YOK, ham deref `volatile`.
  Allocator ve çekirdek ilkelleri bu yüzden çıplaktır (aksi hâlde
  kdl_bolge_olustur → malloc → ... kendini tetikler).
- E013: çıplak işlev yalnız çıplak/extern (ve yerleşik) çağırabilir.
- `satıriçi_asm {{ mimari: <arm64|x86_64> şablon: r#"..."# girdi("k", e)
  çıktı("=k", &v) bozulan("~{{memory}}") }}` — `güvensiz {{ }}` içinde yazılır.
  AS001: etiket hedef mimariyle (`--mimari`) uyuşmazsa derleme DURUR → bir
  dosyada YALNIZ TEK mimarinin asm'si olabilir; mimariye özgü kod AYRI dosyaya
  konur ve Makefile CAT listesi mimariye göre seçer (D2b kararı).
- Şablon AT&T sözdizimi (x86); `$0..` çıktılar→girdiler sırası; `{{dx}}`,
  `{{al}}` belirli yazmaç kısıtları (boyut eşleşmeli: tam16↔dx, tam8↔al).
- KEMGU dizgi literalinde kaçış YOK (`"\\n"` iki karakterdir).
- Sayılar ondalık yazılır (`1016` = 0x3F8); `olarak` tip dönüşümüdür.
- Üst düzey ileri bildirim YOK; çapraz dosya çağrısı için dosyalar CAT edilir
  (tek derleme birimi).

## runtime/kem_heap.kem (desen kaynağı, tamamı)
```kemgu
{oku("runtime/kem_heap.kem")}
```
"""


def kota(anahtar):
    istek = urllib.request.Request(TABAN + "/quota", headers={"X-API-Key": anahtar})
    with urllib.request.urlopen(istek, timeout=60) as y:
        return json.load(y)


def akisli_sor(anahtar, mesajlar, effort=None):
    istek_govde = {"model": MODEL, "messages": mesajlar, "max_tokens": MAX_TOKENS,
                   "stream": True, "stream_options": {"include_usage": True}}
    if effort:
        istek_govde["reasoning_effort"] = effort
    govde = json.dumps(istek_govde).encode()
    istek = urllib.request.Request(TABAN + "/chat/completions", data=govde, method="POST",
                                   headers={"X-API-Key": anahtar,
                                            "Content-Type": "application/json"})
    icerik, dusunme, usage, bitis = [], [], None, None
    t0 = time.monotonic()
    with urllib.request.urlopen(istek, timeout=300) as y:   # okuma başına zaman aşımı
        for ham in y:
            satir = ham.decode("utf-8", "replace").strip()
            if not satir.startswith("data:"):
                continue
            veri = satir[5:].strip()
            if veri == "[DONE]":
                break
            parca = json.loads(veri)
            if parca.get("usage"):
                usage = parca["usage"]
            for s in parca.get("choices", []):
                d = s.get("delta", {})
                if d.get("content"):
                    icerik.append(d["content"])
                r = d.get("reasoning") or d.get("reasoning_content")
                if r:
                    dusunme.append(r)
                if s.get("finish_reason"):
                    bitis = s["finish_reason"]
    return "".join(icerik), "".join(dusunme), usage, bitis, time.monotonic() - t0


def bloklar(metin):
    """(tur, yol, govde) listesi. tur: duzenle | yeni."""
    return [(m.group(1), m.group(2).strip(), m.group(3))
            for m in re.finditer(r"```(duzenle|yeni):([^\n]+)\n(.*?)\n```(?=\s|$)",
                                 metin, re.S)]


def sor(a):
    anahtar = (pathlib.Path.home() / ".config/evren/key").read_text().strip()
    q = kota(anahtar)
    kalan = q.get("daily_remaining_tokens", 0)
    print(f"kota: günlük kalan {kalan} · pencere {q.get('used_tokens')}/{q.get('cap')}")
    if kalan < KOTA_ALT:
        print(f"DUR: günlük kalan token {kalan} < {KOTA_ALT}")
        sys.exit(3)
    is_baglam = "".join(f"\n## {y}\n```\n{oku(y)}\n```\n" for y in a.baglam)
    # [tur 4b dersi] Kesit AYRI bir dosya yolu taşırsa model onu düzenleme hedefi
    # sanar. --kesit GERÇEK dosyanın yolunu ve satır aralığını etiketler.
    for k in a.kesit:
        yol, aralik = k.rsplit(":", 1)
        bas, son = (int(x) for x in aralik.split("-"))
        satirlar = oku(yol).splitlines()[bas - 1:son]
        is_baglam += (f"\n## {yol} — KESİT (satır {bas}-{son}; düzenleme hedefi: `{yol}`, "
                      f"blok yolu `{yol}` olmalı)\n```\n" + "\n".join(satirlar) + "\n```\n")
    gorev = pathlib.Path(a.gorev).read_text(encoding="utf-8")
    kullanici = ortak_baglam() + "\n# İŞE ÖZEL BAĞLAM\n" + is_baglam + "\n# GÖREV\n\n" + gorev
    icerik, dusunme, usage, bitis, sure = akisli_sor(
        anahtar, [{"role": "system", "content": SISTEM}, {"role": "user", "content": kullanici}],
        a.effort)
    taban = CIKTI / f"tur_{a.tur}_{a.is_}"
    (taban.parent / (taban.name + "_istem.md")).write_text(
        f"# Tur {a.tur} — {a.is_} — istem\n\n## SISTEM\n````\n{SISTEM}\n````\n\n"
        f"## KULLANICI\n````\n{kullanici}\n````\n", encoding="utf-8")
    bl = bloklar(icerik)
    (taban.parent / (taban.name + ".md")).write_text("\n".join([
        f"# Tur {a.tur} — {a.is_} — `{MODEL}`", "",
        f"- Tarih: {time.strftime('%Y-%m-%d %H:%M:%S')} · süre {sure:.1f} sn · finish `{bitis}`"
        f" · reasoning_effort `{a.effort or 'varsayılan'}`",
        f"- usage: `{json.dumps(usage)}`",
        f"- Değişiklik blokları: {len(bl)} — " + ", ".join(f"{t}:{y}" for t, y, _ in bl),
        f"- İstem: [{taban.name}_istem.md]({taban.name}_istem.md)", "",
        "## Yanıt", "", icerik or "_(boş)_", "",
        "## Düşünme (reasoning)", "", dusunme or "_(yok)_", ""]), encoding="utf-8")
    print(f"tur {a.tur} {a.is_}: {sure:.1f} sn, finish={bitis}, blok={len(bl)}, "
          f"usage={json.dumps(usage)}")


def uygula(a):
    md = (CIKTI / f"tur_{a.tur}_{a.is_}.md").read_text(encoding="utf-8")
    yanit = md.split("## Yanıt", 1)[1].split("## Düşünme (reasoning)", 1)[0]
    hata = 0
    for tur, yol, govde in bloklar(yanit):
        hedef = DEPO / yol
        if tur == "yeni":
            print(f"YENİ   {yol} ({len(govde.splitlines())} satır)"
                  + (" — VAR, üzerine yazılır" if hedef.exists() else ""))
            if not a.kuru:
                hedef.parent.mkdir(parents=True, exist_ok=True)
                hedef.write_text(govde + "\n", encoding="utf-8")
            continue
        m = re.match(r"<<<<<<< ESKI\n(.*?)\n=======\n(.*?)\n?>>>>>>> YENI\s*$", govde, re.S)
        if not m or not hedef.exists():
            print(f"HATA   {yol}: biçim bozuk ya da dosya yok"); hata += 1; continue
        eski, yeni = m.group(1), m.group(2)
        metin = hedef.read_text(encoding="utf-8")
        n = metin.count(eski)
        if n != 1:
            print(f"HATA   {yol}: ESKI metin {n} kez geçiyor (1 olmalı)"); hata += 1; continue
        print(f"DÜZENLE {yol}: -{len(eski.splitlines())} +{len(yeni.splitlines())} satır")
        if not a.kuru:
            hedef.write_text(metin.replace(eski, yeni, 1), encoding="utf-8")
    sys.exit(1 if hata else 0)


def main():
    p = argparse.ArgumentParser()
    alt = p.add_subparsers(dest="komut", required=True)
    s = alt.add_parser("sor")
    s.add_argument("--tur", required=True); s.add_argument("--is", dest="is_", required=True)
    s.add_argument("--gorev", required=True); s.add_argument("--baglam", nargs="*", default=[])
    s.add_argument("--effort", default=None, help="reasoning_effort (ör. low)")
    s.add_argument("--kesit", nargs="*", default=[], help="GERÇEK yol:BAŞ-SON (ör. Makefile:80-95)")
    u = alt.add_parser("uygula")
    u.add_argument("--tur", required=True); u.add_argument("--is", dest="is_", required=True)
    u.add_argument("--kuru", action="store_true")
    a = p.parse_args()
    sor(a) if a.komut == "sor" else uygula(a)


if __name__ == "__main__":
    main()
