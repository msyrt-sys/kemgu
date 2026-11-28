#!/usr/bin/env bash
# [D-805] GÖREV BÖLGESİ (ρ_sahip) HAPSEDİLME KANITI KAPISI — C ↔ self.
#
# D-309 görevin bölgesini join'de YALNIZ pozitif bir hapsedilme kanıtı varsa
# serbest bırakır (`kdl_gorev_basla_kapanis`ın 4. argümanı = rho_serbest). Kanıt
# yanlışsa serbest bırakma USE-AFTER-FREE'dir ve SESSİZDİR: serbest bellek hâlâ
# eski değerleri taşıdığı için program yine doğru cevabı verir (ölçüldü: exit 42).
# `codegen_diff` yalnız çıkış koduna baktığı için bu sınıfa KÖRDÜR; bölge_operand
# yalnız dizi tahsislerinin ρ sınıfını karşılaştırır. Bu kapıdan önce iki
# derleyicinin kanıt paritesini HİÇBİR ŞEY ölçmüyordu — ve self-host kanıtı üç
# yerde sağlamsızdı (D-805: bağlama çözümü, çağrılan gövdede yanlış kapsam, `ver`
# ifadesinin içi taranmıyordu).
#
# Üç ölçüm:
#   (1) PARİTE   : görev başlatan HER depo dosyasında C ve self aynı bayrak dizisini
#                  yayıyor mu (yalnız biri IR üretiyorsa da kırmızı).
#   (2) BEKLENTİ : test/gorev_rho/*.kem `// RHO_BEKLE:` başlığındaki bayraklar,
#                  İKİ derleyicide. Parite tek başına iki tarafı birden bozan
#                  değişikliği göremez; beklenti görür.
#   (3) SAĞLIK   : `// KOSUM: yok` demeyen fikstürler iki derleyiciyle ASan altında
#                  koşar: exit 42 + "ERROR: AddressSanitizer" YOK. Çalışma zamanı
#                  ASan'la KAYNAKTAN derlenir — dizi erişimcileri runtime içindedir
#                  ve enstrümansız runtime'daki okumayı ASan GÖRMEZ.
#                  Sızıntı (LSan) bilerek kapalı: kanıtsız görevin bölgesi tasarım
#                  gereği sızar (D-511, D-591 kararı).
set -u
KEMGU="${KEMGU:-build/kemgu${EXE=}}"
[ -x "$KEMGU" ] || KEMGU="build/kemgu"
CODEGEN="${CODEGEN:-build/codegen${EXE=}}"
[ -x "$CODEGEN" ] || CODEGEN="build/codegen"
for x in "$KEMGU" "$CODEGEN"; do [ -x "$x" ] || { echo "🔴 ikili yok: $x"; exit 2; }; done
# ⚠ `command -v setarch` YETMEZ (D-551): gerçekten ÇALIŞTIĞINI ölç.
ASAN_RUN=""
if setarch -R true >/dev/null 2>&1; then ASAN_RUN="setarch -R"; fi
source test/calisma_dizini.sh
W=$(calisma_ac build/gorev_rho) || exit 1
calisma_izle "$W"
hata=0

bayrak() {   # $1 = .ll → "0,1,..." (spawn sırasıyla)
    grep -oE "call ptr @kdl_gorev_basla_kapanis\([^)]*\)" "$1" \
        | grep -oE "i32 [0-9]+\)$" | tr -d ')' | awk '{print $2}' | paste -sd, -
}

# (1) PARİTE — süreç ikamesi YOK (D-563: MSYS2 araçlarıyla çalışmaz).
# Yalnız DEPO dosyaları (`git ls-files`): düz `grep -r .` git'in yok saydığı
# `.claude/worktrees/` altındaki başka oturumların kopyalarını da tarıyordu (ölçüldü:
# 46 yerine ~150 dosya). Geliştirici kabuğunda görünmüyordu: orada `grep` bir kabuk
# İŞLEVİYDİ ve o dizinleri atlıyordu (ölçüldü: 0 / 106 eşleşme); harness etkileşimsiz
# koşar ve gerçek `grep`i kullanır.
git ls-files '*.kem' 2>/dev/null | xargs grep -l "görev_başlat" 2>/dev/null | sort > "$W/liste"
n=0
while IFS= read -r f; do
    c=""; s=""
    timeout 60 "$KEMGU" --llvm "$f" > "$W/p.c.ll" 2>/dev/null && c=$(bayrak "$W/p.c.ll")
    timeout 60 "$CODEGEN" --llvm "$f" > "$W/p.s.ll" 2>/dev/null && s=$(bayrak "$W/p.s.ll")
    [ -z "$c$s" ] && continue
    n=$((n+1))
    if [ "$c" != "$s" ]; then echo "  🔴 PARİTE $f: C=[$c] self=[$s]"; hata=1; fi
done < "$W/liste"
[ "$n" -gt 0 ] || { echo "🔴 PARİTE: görev başlatan hiç dosya ölçülmedi"; hata=1; }
echo "  parite: $n dosya ölçüldü"

# (2)+(3) — ASan runtime BİR KEZ derlenir.
if ! clang -fsanitize=address -c runtime/kdl_runtime.c -o "$W/rt.o" 2>"$W/rt.err" ||
   ! clang -fsanitize=address -c runtime/kdl_runtime_mmio.c -o "$W/mmio.o" 2>>"$W/rt.err"; then
    echo "🔴 ASan runtime derlenemedi"; head -3 "$W/rt.err"; exit 2
fi
m=0
for f in test/gorev_rho/r*.kem; do
    [ -f "$f" ] || continue
    b=$(basename "$f" .kem)
    bek=$(grep -m1 -oE "RHO_BEKLE: [0-9,]+" "$f" | awk '{print $2}')
    [ -n "$bek" ] || { echo "  🔴 $b: RHO_BEKLE başlığı yok"; hata=1; continue; }
    kosum=1; grep -q "^// KOSUM: yok" "$f" && kosum=0
    m=$((m+1))
    for et in C S; do
        cc="$KEMGU"; [ "$et" = S ] && cc="$CODEGEN"
        if ! timeout 60 "$cc" --llvm "$f" > "$W/$et.ll" 2>/dev/null; then
            echo "  🔴 $b $et: IR üretilemedi"; hata=1; continue
        fi
        g=$(bayrak "$W/$et.ll")
        if [ "$g" != "$bek" ]; then
            echo "  🔴 $b $et BEKLENTİ: bayrak=[$g] beklenen=[$bek]"; hata=1
        fi
        [ "$kosum" -eq 1 ] || continue
        if ! clang -fsanitize=address -x ir "$W/$et.ll" -x none "$W/rt.o" "$W/mmio.o" -o "$W/$et.a" 2>/dev/null; then
            echo "  🔴 $b $et: ASan ikilisi linklenemedi"; hata=1; continue
        fi
        ASAN_OPTIONS=detect_leaks=0 timeout 60 $ASAN_RUN "$W/$et.a" >/dev/null 2>"$W/$et.err"; arc=$?
        # POZİTİF KANIT (D-551): 127/124 = hiç koşmadı / asıldı → ölçüm YAPILMADI.
        if [ "$arc" -eq 127 ] || [ "$arc" -eq 124 ]; then
            echo "  🔴 $b $et SAĞLIK: ASan ikilisi KOŞMADI (rc=$arc)"; hata=1; continue
        fi
        if grep -q "ERROR: AddressSanitizer" "$W/$et.err"; then
            echo "  🔴 $b $et SAĞLIK: $(grep -m1 -oE 'AddressSanitizer: [a-z-]+' "$W/$et.err")"; hata=1; continue
        fi
        if [ "$arc" -ne 42 ]; then echo "  🔴 $b $et DAVRANIŞ: exit=$arc (beklenen 42)"; hata=1; fi
    done
done
[ "$m" -gt 0 ] || { echo "🔴 test/gorev_rho fikstürü yok"; hata=1; }
if [ "$hata" -eq 0 ]; then
    echo "=== görev ρ_sahip kanıtı: parite $n dosya · beklenti+ASan $m fikstür (C + self) ==="
    exit 0
fi
echo "=== görev ρ_sahip kanıtı: BAŞARISIZ ==="
exit 2
