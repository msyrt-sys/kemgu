#!/usr/bin/env bash
# [D-770] E4k bellek kapısı. (1) yardımcı-yerel dizi büyütmesi ana bölgeyi büyütmez (C + self);
# (2) göreve taşınan dizinin büyütülmesi TSan altında yarışsız (TSan yoksa BİLDİREREK atlanır).
set -u
cd "$(dirname "$0")/.."
EXE="${EXE-}"; case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) EXE="${EXE:-.exe}";; esac
W=$(mktemp -d build/e4k.XXXXXX); trap 'rm -rf "$W"' EXIT
fail=0
for c in kemgu codegen; do
  "build/$c$EXE" --llvm test/e4k/yerel_buyume.kem > "$W/$c.ll" 2> "$W/$c.err" || { echo "🔴 $c --llvm başarısız"; cat "$W/$c.err"; fail=1; continue; }
  python3 - "$W/$c.ll" "$W/$c.y.ll" <<'PY'
import sys
t=open(sys.argv[1],encoding='utf-8').read()
assert 'define i32 @main(' in t
open(sys.argv[2],'w',encoding='utf-8').write(t.replace('define i32 @main(','define i32 @kem_main(',1))
PY
  clang -w -x ir "$W/$c.y.ll" -x none test/e4k/surucu.c build/kdl_runtime.o -o "$W/$c.bin" || { echo "🔴 $c bağlama başarısız"; fail=1; continue; }
  out=$("$W/$c.bin"); blok=$(echo "$out" | sed -n 's/^BLOK \([0-9]*\) RC \([0-9]*\)$/\1/p'); rc=$(echo "$out" | sed -n 's/^BLOK [0-9]* RC \([0-9]*\)$/\1/p')
  if [ -z "$blok" ] || [ "$rc" != 0 ] || [ "$blok" -gt 2 ]; then echo "🔴 E4K $c yerel_buyume: ana bölge $blok blok (sınır 2), rc=$rc"; fail=1
  else echo "E4K OK $c yerel_buyume ana bölge $blok blok"; fi
done
if [ -n "$EXE" ] || ! echo 'int main(void){return 0;}' | clang -fsanitize=thread -x c - -o "$W/tsan_dene" >/dev/null 2>&1; then
  echo "ATLANDI (ThreadSanitizer yok — görev büyütme yarış denetimi yapılmadı)"
else
  RUN=""; setarch -R true >/dev/null 2>&1 && RUN="setarch -R"
  clang -std=c11 -g -O1 -fsanitize=thread -Iruntime -c runtime/kdl_runtime.c -o "$W/rt_tsan.o" || { echo "🔴 TSan runtime derlenemedi"; exit 1; }
  for c in kemgu codegen; do
    "build/$c$EXE" --llvm test/e4k/gorev_buyume.kem > "$W/g_$c.ll" || { echo "🔴 $c gorev_buyume --llvm"; fail=1; continue; }
    clang -w -fsanitize=thread -x ir "$W/g_$c.ll" -x none "$W/rt_tsan.o" -lpthread -o "$W/g_$c" || { echo "🔴 $c gorev_buyume bağlama"; fail=1; continue; }
    iyi=0
    for r in 1 2 3; do
      $RUN "$W/g_$c" > "$W/g_$c.$r.log" 2>&1; rc=$?; u=$(grep -c 'WARNING: ThreadSanitizer' "$W/g_$c.$r.log")
      if [ "$rc" != 42 ] || [ "$u" != 0 ]; then echo "🔴 E4K $c gorev_buyume koşum $r: rc=$rc TSan uyarısı=$u"; grep -m3 -A6 "WARNING" "$W/g_$c.$r.log"; fail=1
      else iyi=$((iyi+1)); fi
    done
    [ "$iyi" = 3 ] && echo "E4K OK $c gorev_buyume 3/3 (TSan 0 uyarı)"
  done
fi
exit $fail
