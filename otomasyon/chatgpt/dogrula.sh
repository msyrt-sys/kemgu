#!/usr/bin/env bash
# ChatGPT çıktısı doğrulayıcı — bir .kem test dosyasını İKİ derleyicide ölçer.
#   otomasyon/chatgpt/dogrula.sh <test.kem> [...]     (repo kökünden)
#   otomasyon/chatgpt/dogrula.sh --kart               (dil kartındaki örneği ölç)
# Her dosya için: C --check · C --llvm|clang|çalıştır (exit 0) ·
# self-host --check (OK) · self-host --llvm|clang|çalıştır (exit 0).
# Hepsi geçerse dosya başına "CHATGPT_DOGRULA OK <yol>" basar; ilk hatada exit 1.
set -u
# Çalıştırma zaman aşımı (sn). 30 sn CI'da YETMEDİ (D-737): FAT32 yazma testleri
# self-host ile Spark'ta 14-16 sn, x86 CI koşucusunda 30 sn'yi aştı (exit 124).
# Asılmayı yakalamak için sınır KALIR; yalnız gerçek iş yüküne göre ayarlandı.
ZA="${CHATGPT_ZAMAN_ASIMI:-180}"
EXE=""; case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) EXE=".exe";; esac
KEMGU="build/kemgu${EXE}"; SELF="build/codegen${EXE}"; RT="build/kdl_runtime.o"
for x in "$KEMGU" "$SELF" "$RT"; do
  [ -f "$x" ] || { echo "HATA: $x yok — önce: make && make $SELF"; exit 1; }
done
# host-mock MMIO ilkelleri (sürücü testleri mock transport üzerinden koşar)
[ -f build/kdl_runtime_mmio.o ] || make -s build/kdl_runtime_mmio.o >/dev/null
RT="$RT build/kdl_runtime_mmio.o"
W="build/_chatgpt_dogrula_$$"; mkdir -p "$W"; trap 'rm -rf "$W"' EXIT

dosyalar=("$@")
if [ "${1:-}" = "--kart" ]; then
  python3 - "$W/kart_ornek.kem" <<'PY'
import sys, re
s = open("otomasyon/chatgpt/KEMGU_dil_karti.md", encoding="utf-8").read()
m = re.search(r"<!-- ORNEK -->\s*```kemgu\n(.*?)\n```", s, re.S)
if not m: sys.exit("HATA: kartta <!-- ORNEK --> bloğu yok")
open(sys.argv[1], "w", encoding="utf-8").write(m.group(1) + "\n")
PY
  [ $? -eq 0 ] || exit 1
  dosyalar=("$W/kart_ornek.kem")
fi
[ ${#dosyalar[@]} -gt 0 ] || { echo "kullanım: $0 <test.kem> ... | --kart"; exit 1; }

for f in "${dosyalar[@]}"; do
  b="$W/$(basename "$f" .kem)"
  "$KEMGU" --check "$f" > "$b.cchk" 2>&1 || { echo "HATA[C --check] $f"; cat "$b.cchk"; exit 1; }
  "$KEMGU" --llvm "$f" > "$b.c.ll" 2> "$b.c.err" || { echo "HATA[C --llvm] $f"; cat "$b.c.err"; exit 1; }
  clang -x ir "$b.c.ll" -x none $RT -o "$b.c" 2> "$b.c.cerr" || { echo "HATA[C link] $f"; head -20 "$b.c.cerr"; exit 1; }
  timeout "$ZA" "$b.c" > "$b.c.out" 2>&1; rc=$?
  [ $rc -eq 0 ] || { echo "HATA[C çalıştır] $f exit=$rc"; head -40 "$b.c.out"; exit 1; }
  "$SELF" --check "$f" > "$b.schk" 2>&1
  [ "$(cat "$b.schk")" = "OK" ] || { echo "HATA[self --check] $f"; cat "$b.schk"; exit 1; }
  "$SELF" --llvm "$f" > "$b.s.ll" 2> "$b.s.err" || { echo "HATA[self --llvm] $f"; cat "$b.s.err"; exit 1; }
  clang -x ir "$b.s.ll" -x none $RT -o "$b.s" 2> "$b.s.cerr" || { echo "HATA[self link] $f"; head -20 "$b.s.cerr"; exit 1; }
  timeout "$ZA" "$b.s" > "$b.s.out" 2>&1; rc=$?
  [ $rc -eq 0 ] || { echo "HATA[self çalıştır] $f exit=$rc"; head -40 "$b.s.out"; exit 1; }
  cmp -s "$b.c.out" "$b.s.out" || { echo "HATA[stdout C≠self] $f"; diff "$b.c.out" "$b.s.out" | head; exit 1; }
  echo "CHATGPT_DOGRULA OK $f"
done
