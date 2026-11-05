from pathlib import Path

def yaz(yol, metin):
    p = Path(yol)
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text("\n".join(satir.rstrip() for satir in metin.splitlines()) + "\n", encoding="utf-8")

def testler(yol, baslik, vakalar):
    s = baslik + "\n"
    for ad, govde in vakalar.items():
        s += f"işlev test_{ad}() -> mantıksal {{\n{govde}\n}}\n\n"
    s += "işlev main() -> tam32 {\n    değişken h: tam32 = 0;\n"
    for ad in vakalar:
        s += f'    eğer değil test_{ad}() {{ yazdir_metin("BASARISIZ {ad}"); h = h + 1; }}\n'
    s += "    ver h;\n}\n"
    yaz(yol, s)
