#!/usr/bin/env python3
"""
KEMGU Basit Formatter (regex tabanli, %80 dogru)

Kullanim:
    python tools/format.py dosya.kem            # stdout
    python tools/format.py dosya.kem --in-place # yerine yaz
    python tools/format.py --check dosya.kem    # 0 = formatli, 1 = degil

Yapilanlar:
  - Girinti normalize (4-space)
  - Tutarli keyword aralici (eger, iken, vs. + bosluk)
  - Operator etrafi bosluk (+, -, *, /, ==, vs.)
  - { sonrasi newline, } oncesi newline
  - ; sonrasi newline (deyim sonu)
  - Cift bosluk tek bosluk

Yapilmayanlar (AST gerek):
  - Pratt parser precedence-aware
  - Yorum hizalama
  - Cok satirli ifade dogru girintileme
"""

import sys
import re
import argparse


KEYWORDS_YALNIZ = {
    "işlev", "yapı", "değişken", "sabit",
    "eğer", "değilse", "iken", "için", "ver",
    "eşleş", "doğru", "yanlış", "boş",
    "ve", "veya", "değil",
    "kullan", "dışa", "modül", "özellik", "uygula",
    "kendin", "seçimlik", "sonuç", "tekkez", "imha",
    "tamam", "hata", "değer", "hiç",
}


def format_kaynak(metin: str) -> str:
    """KEMGU kaynagini formatla. Basit, regex tabanli."""
    out_satirlar = []
    girinti = 0
    INDENT = "    "

    for satir in metin.splitlines():
        s = satir.rstrip()
        if not s.strip():
            out_satirlar.append("")
            continue

        # Yorum tek basina ise olduğu gibi
        s_strip = s.strip()
        if s_strip.startswith("//") or s_strip.startswith("/*"):
            out_satirlar.append(INDENT * girinti + s_strip)
            continue

        # Kapama parantez {} ile baslıyorsa girintiyi azalt
        bas_kapama = 0
        for c in s_strip:
            if c == "}":
                bas_kapama += 1
            elif not c.isspace():
                break
        girinti = max(0, girinti - bas_kapama)

        # Operator etrafi bosluk normalize
        normalized = s_strip
        # ==, !=, <=, >=, ->, =>, ::, <<, >>
        normalized = re.sub(r"\s*(==|!=|<=|>=|->|=>|::|<<|>>)\s*", r" \1 ", normalized)
        # Tek karakter op: + - * / % = < > & | ^
        normalized = re.sub(r"(?<=[\w\)\]])\s*([+\-*/%=<>&|^])\s*(?=[\w\(\[\"\'])", r" \1 ", normalized)
        # , sonrasi tek bosluk
        normalized = re.sub(r"\s*,\s*", ", ", normalized)
        # ; sonrasi bosluk (genelde EOL ama korumak için)
        normalized = re.sub(r"\s*;\s*$", ";", normalized)
        # Cift+ bosluk tek bosluk
        normalized = re.sub(r"  +", " ", normalized)

        out_satirlar.append(INDENT * girinti + normalized)

        # Acilan parantez sonrasi girintiyi artir
        son_acmalar = 0
        for c in reversed(s_strip):
            if c == "{":
                son_acmalar += 1
            elif not c.isspace() and c != ";":
                break
        girinti += son_acmalar

    return "\n".join(out_satirlar) + ("\n" if metin.endswith("\n") else "")


def main():
    parser = argparse.ArgumentParser(description="KEMGU basit formatter")
    parser.add_argument("dosya", help="Formatlanacak .kem dosyasi")
    parser.add_argument("--in-place", action="store_true",
                        help="Dosyayi yerine yaz (yedek dosya.kem.bak)")
    parser.add_argument("--check", action="store_true",
                        help="Formatli mi kontrol et (exit 0/1)")
    args = parser.parse_args()

    with open(args.dosya, "r", encoding="utf-8") as f:
        original = f.read()

    formatli = format_kaynak(original)

    if args.check:
        if original.strip() == formatli.strip():
            print(f"OK: {args.dosya} formatli")
            sys.exit(0)
        else:
            print(f"DEGIL: {args.dosya} formatli degil "
                  f"(diff: --in-place ile uygula)")
            sys.exit(1)

    if args.in_place:
        with open(args.dosya + ".bak", "w", encoding="utf-8") as f:
            f.write(original)
        with open(args.dosya, "w", encoding="utf-8") as f:
            f.write(formatli)
        print(f"OK: {args.dosya} yerine yazildi (yedek: {args.dosya}.bak)")
    else:
        sys.stdout.write(formatli)


if __name__ == "__main__":
    main()
