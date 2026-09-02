#!/usr/bin/env bash
# [D-543] KANAL OMRU KAPISI — davranissal kapilar bu sinifa KORDUR.
#
# `codegen_diff` yalniz CIKIS KODUNA bakar: kanal serbesti kaldirilsa program
# yine 15 doner ve kapi YESIL kalir (D-417'nin spekulasyon-bariyeri dersi).
# Bu kapi UC seyi birden olcer:
#   (1) YAPISAL : fikstur en az 1 `kdl_kanal_serbest` yayiyor mu
#   (2) DAVRANIS: program dogru cikis kodunu veriyor mu (kanit yanlissa cokerdi)
#   (3) SAGLIK  : ASan HIC hata basmiyor mu (cift-serbest / UAF avi)
#
# (3) ZORUNLU: gelistirme sirasinda ölçüldü ki kanal listesi lifted lambda'ya
# SIZINCA cop isaretci serbest birakiliyor (SEGV in free). Yalniz (1)+(2)
# olsaydi o kusur GORUNMEZDI — fikstur yine 15 donuyordu.
set -u
KEMGU="${KEMGU:-build/kemgu${EXE=}}"
[ -x "$KEMGU" ] || KEMGU="build/kemgu"
F=test/cg_korpus/cg_kanal_omru.kem
W=build/kanal_kapi; mkdir -p "$W"
hata=0

[ -x "$KEMGU" ] || { echo "🔴 kemgu ikilisi yok: $KEMGU"; exit 2; }
[ -f "$F" ] || { echo "🔴 fikstur yok: $F"; exit 2; }

"$KEMGU" --llvm "$F" > "$W/o.ll" 2>/dev/null || { echo "🔴 IR uretilemedi"; exit 2; }

n=$(grep -c "kdl_kanal_serbest(ptr %" "$W/o.ll")
if [ "$n" -lt 1 ]; then
    echo "🔴 YAPISAL: kanal serbest cagrisi YOK (kanit dusmus) — beklenen >=1, gelen $n"
    hata=1
else
    echo "  ✅ yapisal: $n adet kdl_kanal_serbest"
fi

if clang -x ir "$W/o.ll" -x none build/kdl_runtime.o -o "$W/o" 2>/dev/null; then
    "$W/o" >/dev/null 2>&1; rc=$?
    if [ "$rc" -ne 15 ]; then echo "🔴 DAVRANIS: exit=$rc (beklenen 15)"; hata=1
    else echo "  ✅ davranis: exit=15"; fi
else
    echo "🔴 fikstur linklenemedi"; hata=1
fi

if clang -fsanitize=address -x ir "$W/o.ll" -x none build/kdl_runtime.o -o "$W/oa" 2>/dev/null; then
    setarch -R "$W/oa" >/dev/null 2>"$W/e.txt" || true
    ah=$(grep -cE "ERROR: AddressSanitizer" "$W/e.txt")
    if [ "$ah" -ne 0 ]; then
        echo "🔴 SAGLIK: ASan $ah hata — cift-serbest/UAF:"
        grep -E "ERROR: AddressSanitizer" "$W/e.txt" | head -2
        hata=1
    else echo "  ✅ saglik: ASan 0 hata"; fi
else
    echo "  ⚠ ASan derlemesi yok — SAGLIK olcumu ATLANDI (sessiz degil, bildirildi)"
fi

if [ "$hata" -ne 0 ]; then echo "=== kanal omru: BASARISIZ ==="; exit 2; fi
echo "=== kanal omru: 3/3 olcum gecti ==="
