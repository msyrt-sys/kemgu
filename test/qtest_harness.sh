#!/usr/bin/env bash
# QEMU qtest kapısı — sans-IO sürücüleri (drivers/*) GERÇEK QEMU aygıt modellerine
# karşı koşturur (LOOP E4h). CPU koşmaz (`-accel tcg -S`); KEMGU programı qtest
# soketinden yazmaç/port/misafir belleği erişimi yapar.
#
#   bash test/qtest_harness.sh nvme
#
# Neden: Codex'in sahte aygıt modelleri standardı NASIL ANLADIĞINI yansıtır; aynı
# yanlış sürücüye ve modele birlikte girebilir. QEMU'nun kendi aygıt modeli
# bağımsız bir uygulamadır. Yazılan veri ayrıca QEMU kapandıktan SONRA disk
# imajından Python ile doğrulanır (KEMGU'nun kendi okumasından bağımsız kâhin).
#
# İki derleyici: C (build/kemgu) ve self-host (build/codegen) ikilileri AYRI
# QEMU örneklerine karşı koşar; ikisi de geçmeli.
set -u
cd "$(dirname "$0")/.."
EXE=""; case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) EXE=".exe";; esac
QEMU=qemu-system-x86_64
if ! command -v "$QEMU" >/dev/null 2>&1; then
  echo "  ⚠ qtest: $QEMU YOK — kapı ATLANDI (CI ortamı; Spark'ta koşar)"; exit 0
fi
AYGIT="${1:-nvme}"
W="build/_qtest_$$"; mkdir -p "$W"
QPID=""
temizle() { [ -n "$QPID" ] && kill "$QPID" 2>/dev/null; wait 2>/dev/null; rm -rf "$W"; }
trap temizle EXIT

case "$AYGIT" in
  xhci|usb_depo|usb_kbd) python3 tests/qemu/usb_harness.py "$AYGIT"; exit $? ;;
  nvme) PROG=tests/qemu/nvme_qtest.kem
        AYGIT_ARG=(-drive "file=$W/disk.img,if=none,id=d0,format=raw" -device nvme,drive=d0,serial=kemgu01) ;;
  ahci) PROG=tests/qemu/ahci_qtest.kem      # q35 dahili ICH9 AHCI (00:1f.2), port 0
        AYGIT_ARG=(-drive "file=$W/disk.img,if=none,id=d0,format=raw" -device ide-hd,drive=d0,bus=ide.0) ;;
  *) echo "bilinmeyen aygıt: $AYGIT"; exit 2 ;;
esac

hata=0
for d in kemgu codegen; do
  "build/$d$EXE" --llvm "$PROG" > "$W/$d.ll" 2> "$W/$d.err" || { echo "  🔴 $d --llvm"; head "$W/$d.err"; hata=1; continue; }
  clang -x ir "$W/$d.ll" -x none build/kdl_runtime.o -o "$W/$d" 2> "$W/$d.cerr" || { echo "  🔴 $d link"; head "$W/$d.cerr"; hata=1; continue; }

  # Taze disk: sektör s, bayt j = (s*7 + j*13 + 5) & 255  (nvme_qtest.kem ile AYNI formül)
  python3 - "$W/disk.img" <<'PY'
import sys
b = bytearray(4 * 1024 * 1024)
for s in range(len(b) // 512):
    base = s * 512
    for j in range(512):
        b[base + j] = (s * 7 + j * 13 + 5) & 255
open(sys.argv[1], "wb").write(b)
PY
  PORT=$(python3 -c 'import socket;s=socket.socket();s.bind(("127.0.0.1",0));print(s.getsockname()[1]);s.close()')
  "$QEMU" -machine q35 -accel tcg -S -display none -nodefaults -m 256M \
     -qtest "tcp:127.0.0.1:$PORT,server=on,wait=on" -qtest-log /dev/null \
     "${AYGIT_ARG[@]}" > "$W/qemu.log" 2>&1 &
  QPID=$!
  # QEMU dinlemeye başlayana kadar bekle (en çok 10 sn). BAĞLANMADAN bak:
  # wait=on soketi tek istemci kabul eder; yoklama bağlantısı onu tüketirdi.
  for i in $(seq 1 100); do
    ss -ltnH "sport = :$PORT" 2>/dev/null | grep -q . && break
    sleep 0.1
  done
  timeout 120 "$W/$d" "$PORT" > "$W/$d.out" 2>&1; rc=$?
  kill "$QPID" 2>/dev/null; wait "$QPID" 2>/dev/null; QPID=""
  cat "$W/$d.out"
  if [ $rc -ne 0 ] || ! grep -qE "QTEST_[A-Z]+ OK" "$W/$d.out"; then
    echo "  🔴 $d: rc=$rc"; cat "$W/qemu.log" | head -5; hata=1; continue
  fi
  # Bağımsız kâhin: QEMU kapandıktan sonra imajı ana makinede oku.
  if python3 - "$W/disk.img" <<'PY'
import sys
b = open(sys.argv[1], "rb").read()
for j in range(9 * 512):                      # LBA 200.., 9 sektör
    if b[200 * 512 + j] != (j * 31 + 11) & 255:
        print(f"  imaj: LBA 200 + bayt {j} YANLIS"); sys.exit(1)
for s in list(range(0, 200)) + list(range(209, 260)):   # dokunulmayan alan
    for j in range(512):
        if b[s * 512 + j] != (s * 7 + j * 13 + 5) & 255:
            print(f"  imaj: sektor {s} BOZULDU"); sys.exit(1)
PY
  then echo "  ✓ $d: QEMU $AYGIT gidiş-dönüş + imaj kâhini"; else echo "  🔴 $d: imaj kâhini"; hata=1; fi
done
[ $hata -eq 0 ] && echo "=== qtest $AYGIT: C + self-host GEÇTİ (gerçek QEMU aygıtı) ===" || { echo "=== qtest $AYGIT: BAŞARISIZ ==="; exit 2; }
