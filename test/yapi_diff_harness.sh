#!/usr/bin/env bash
# ============================================================================
# yapi_diff_harness.sh — HOST korpusunda YAPISAL IR paritesi (D-422).
# ----------------------------------------------------------------------------
# NEDEN AYRI BİR KAPI — ÖLÇÜLDÜ, VARSAYILMADI:
# Tek bir oturumda AYNI sınıftan ÜÇ sapma çıktı ve DAVRANIŞSAL kapıların HİÇBİRİ
# görmedi:
#   1. açık `-> boş` dönüşü  → C `define void @f`,  self `define i32 @f`
#   2. `sonuç<bos,X>` payload → C `{i8, i8, i8}`,    self `{i8, i32, i8}`
#   3. `mantıksal` dönüş      → C `define i1 @main`, self `define i32 @main`
# Üçü de GEÇERLİ IR üretir, program AYNI sonucu verir, exit ve stdout DEĞİŞMEZ —
# çünkü LLVM `define`/`call` uyuşmazlığını sessizce kabul eder (D-295) ve x86-64
# ABI'sinde değer register'da hayatta kalır. `codegen_diff` exit koduna,
# `codegen_genis` exit+stdout'a bakar; ikisi de bu sınıfa KÖRDÜR.
#
# `baremetal_diff` (D-418) tam da bunu ölçer AMA yalnız `runtime/` yüzeyinde.
# Bu kapı aynı ölçümü HOST korpusuna taşır: `define` kümesi = ad + DÖNÜŞ TİPİ.
#
# ⚠ DÖNÜŞ TİPİNİ KESME: `sed 's/(.*//'` imzayı atar ama `define void @f` ile
# `define i32 @f` ayrımını KORUR — kusurun ta kendisi oradadır.
#
# MUAFİYET LİSTESİ = BİLİNEN YAPISAL SAPMA ENVANTERİDİR, hedefi KÜÇÜLMEKTİR.
# Yeni bir dosya eklemek için GEREKÇE DECISIONS_LOG'a yazılmalıdır.
#
# Kullanım: bash test/yapi_diff_harness.sh (veya make calistir_yapi_diff)
# ============================================================================
set -u
KEMGU=${KEMGU:-build/kemgu.exe}
CODEGEN=${CODEGEN:-build/codegen.exe}
TMP=$(mktemp -d 2>/dev/null || echo /tmp/yapidiff); mkdir -p "$TMP"

if [ ! -x "$CODEGEN" ]; then
    echo "ℹ codegen.exe yok — kapı atlandı (önce build/codegen.exe kurulmalı)."
    exit 0
fi

# ---- BİLİNEN SAPMA ENVANTERİ (D-422'de ölçüldü; 26 dosya, 4 kök) ------------
# (K1) `mantıksal` dönüş → C `i1`, self `i32`. Self aritmetik bağlamda bilerek
#      i32 kullanır; `islev_donus_tip`te i1'e çevirmek gövdedeki `ret` ile
#      tutarsızlık riski taşır → AYRI adım.
MUAF_K1="cg2_bool_lit cg2_buyuk cg2_degil cg2_esit cg2_farkli cg2_kucuk cg2_ve
cg2_veya cg7a_esit"
# (K2) `sonuç<bos,X>` payload → C `i8`, self `i32`. Birim tipin AGGREGATE ALANI
#      eşlemesi; dönüş eşlemesinden AYRI kök (D-422'de ölçüldü).
MUAF_K2="cg_cesit_ic_ayirici"
# (K3) lifted lambda dönüşü → self DAİMA i64 (runtime KdlGorevBare ABI, D-300),
#      C gövdeden çıkarsar. Bilinçli tasarım farkı.
MUAF_K3="cg_gorev_baslat cg_gorev_capture cg_gorev_desen_ic_tip
cg_gorev_i64_daralt cg_gorev_kanal cg_gorev_lambda_blok
cg_rho_sahip_confined cg_rho_sahip_kacis"
# (K4) generic BASE gövdesi → self yayar, C atlar (D-401: self çıkarsaması
#      kısmî olduğu için base gövde gerekli; atlamayı denemek 11/18→8/18
#      regresyonu verdi, geri alındı).
MUAF_K4="cg_generic_mono cg_generic_sonuc_ptr cg_modul_alias cg_modul_capraz
cg_modul_generic cg_modul_transitif cgmodul_mat cgmodul_zincir"

MUAF="$MUAF_K1 $MUAF_K2 $MUAF_K3 $MUAF_K4"
muaf_mi() { case " $(echo $MUAF) " in *" $1 "*) return 0;; esac; return 1; }

pass=0; fail=0; atla=0; muaf=0
for f in test/cg_korpus/*.kem; do
    [ -f "$f" ] || continue
    b=$(basename "$f" .kem)

    # Oracle IR üretemiyorsa karşılaştırma anlamsız → atla (başarısızlık DEĞİL).
    "$KEMGU" --llvm --tip-atla "$f" > "$TMP/c.ll" 2>/dev/null || { atla=$((atla+1)); continue; }
    grep -q "^define" "$TMP/c.ll" 2>/dev/null || { atla=$((atla+1)); continue; }

    # D-424: oracle'a `--tip-atla` geçiliyor; self-host `--llvm` de artık tip
    # hatasında durduğu için SİMETRİ şart (aksi hâlde kasıtlı tip-geçersiz
    # korpus dosyaları yalnız self tarafında reddedilir → sahte kırmızı).
    "$CODEGEN" --llvm --tip-atla "$f" > "$TMP/s.ll" 2>/dev/null || {
        echo "  🔴 $b — KEMGU codegen IR üretemedi"; fail=$((fail+1)); continue; }

    grep "^define" "$TMP/c.ll" | sed 's/(.*//' | sort > "$TMP/c.d"
    grep "^define" "$TMP/s.ll" | sed 's/(.*//' | sort > "$TMP/s.d"

    if diff -q "$TMP/c.d" "$TMP/s.d" >/dev/null 2>&1; then
        # Muaf listesindeki bir dosya ARTIK EŞLEŞİYORSA listeden çıkarılmalıdır.
        if muaf_mi "$b"; then
            echo "  ⚠ $b — MUAF ama artık EŞLEŞİYOR: muafiyet listesinden ÇIKAR."
        fi
        pass=$((pass+1))
    elif muaf_mi "$b"; then
        muaf=$((muaf+1))
    else
        echo "  🔴 $b — define kümesi farklı (ad + DÖNÜŞ TİPİ):"
        diff "$TMP/c.d" "$TMP/s.d" 2>/dev/null | head -4 | sed 's/^/      /'
        fail=$((fail+1))
    fi
done
echo "=== yapısal IR paritesi: $pass/$((pass+fail)) dosya ($muaf muaf, $atla atlandı) ==="
[ "$fail" -eq 0 ]
