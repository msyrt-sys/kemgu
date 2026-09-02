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
    timeout 20 "$W/o" >/dev/null 2>&1; rc=$?
    if [ "$rc" -ne 15 ]; then echo "🔴 DAVRANIS: exit=$rc (beklenen 15)"; hata=1
    else echo "  ✅ davranis: exit=15"; fi
else
    echo "🔴 fikstur linklenemedi"; hata=1
fi

if clang -fsanitize=address -x ir "$W/o.ll" -x none build/kdl_runtime.o -o "$W/oa" 2>/dev/null; then
    timeout 60 setarch -R "$W/oa" >/dev/null 2>"$W/e.txt" || true
    ah=$(grep -cE "ERROR: AddressSanitizer" "$W/e.txt")
    if [ "$ah" -ne 0 ]; then
        echo "🔴 SAGLIK: ASan $ah hata — cift-serbest/UAF:"
        grep -E "ERROR: AddressSanitizer" "$W/e.txt" | head -2
        hata=1
    else echo "  ✅ saglik: ASan 0 hata"; fi
else
    echo "  ⚠ ASan derlemesi yok — SAGLIK olcumu ATLANDI (sessiz degil, bildirildi)"
fi

# --- [D-544] NEGATIF OLCUM: kacan uc -> serbest YASAK -------------------------
# `gonderen(k)` PROJEKSIYONDUR: kanalin KENDI handle'ini verir. O uc `ver` ile
# cerceveden kaciyorsa kanali serbest birakmak CAGIRANDA UAF olur.
# Bu, takma-ad kanitinin TEK gate'idir. Yalniz pozitif fikstur olsaydi
# "takma adi hic kanitlama" sabotaji kapidan GECERDI (D-425).
N=test/cg_korpus/cg_kanal_kacan_uc.kem
if [ ! -f "$N" ]; then
    echo "NEGATIF fikstur yok: $N"; hata=1
elif ! "$KEMGU" --llvm "$N" > "$W/n.ll" 2>/dev/null; then
    echo "NEGATIF fikstur icin IR uretilemedi"; hata=1
else
    nn=$(grep -c "kdl_kanal_serbest(ptr %" "$W/n.ll")
    if [ "$nn" -ne 0 ]; then
        echo "NEGATIF: kacan uc oldugu halde $nn serbest yayildi (UAF riski)"
        hata=1
    else
        echo "  negatif: kacan uc -> 0 serbest"
    fi
    if clang -x ir "$W/n.ll" -x none build/kdl_runtime.o -o "$W/n" 2>/dev/null; then
        timeout 20 "$W/n" >/dev/null 2>&1; nrc=$?
        if [ "$nrc" -ne 42 ]; then echo "NEGATIF davranis: exit=$nrc (beklenen 42)"; hata=1
        else echo "  negatif davranis: exit=42"; fi
    else
        echo "NEGATIF fikstur linklenemedi"; hata=1
    fi
fi

if [ "$hata" -ne 0 ]; then echo "=== kanal omru: BASARISIZ ==="; exit 2; fi
echo "=== kanal omru: 5/5 olcum gecti ==="
